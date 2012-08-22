#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "gmqcc.h"

MEM_VEC_FUNCTIONS(qc_program,   prog_section_statement, code)
MEM_VEC_FUNCTIONS(qc_program,   prog_section_def,       defs)
MEM_VEC_FUNCTIONS(qc_program,   prog_section_def,       fields)
MEM_VEC_FUNCTIONS(qc_program,   prog_section_function,  functions)
MEM_VEC_FUNCTIONS(qc_program,   char,                   strings)
MEM_VEC_FUN_APPEND(qc_program,  char,                   strings)
MEM_VEC_FUN_RESIZE(qc_program,  char,                   strings)
MEM_VEC_FUNCTIONS(qc_program,   qcint,                  globals)
MEM_VEC_FUNCTIONS(qc_program,   qcint,                  entitydata)
MEM_VEC_FUNCTIONS(qc_program,   bool,                   entitypool)

MEM_VEC_FUNCTIONS(qc_program,   qcint,         localstack)
MEM_VEC_FUN_APPEND(qc_program,  qcint,         localstack)
MEM_VEC_FUN_RESIZE(qc_program,  qcint,         localstack)
MEM_VEC_FUNCTIONS(qc_program,   qc_exec_stack, stack)

MEM_VEC_FUNCTIONS(qc_program,   size_t, profile)
MEM_VEC_FUN_RESIZE(qc_program,  size_t, profile)

MEM_VEC_FUNCTIONS(qc_program,   prog_builtin, builtins)

static void loaderror(const char *fmt, ...)
{
    int     err = errno;
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf(": %s\n", strerror(err));
}

static void qcvmerror(qc_program *prog, const char *fmt, ...)
{
    va_list ap;

    prog->vmerror++;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

qc_program* prog_load(const char *filename)
{
    qc_program *prog;
    prog_header header;
    size_t      i;
    FILE *file;

    file = util_fopen(filename, "rb");
    if (!file)
        return NULL;

    if (fread(&header, sizeof(header), 1, file) != 1) {
        loaderror("failed to read header from '%s'", filename);
        fclose(file);
        return NULL;
    }

    if (header.version != 6) {
        loaderror("header says this is a version %i progs, we need version 6\n", header.version);
        fclose(file);
        return NULL;
    }

    prog = (qc_program*)mem_a(sizeof(qc_program));
    if (!prog) {
        fclose(file);
        printf("failed to allocate program data\n");
        return NULL;
    }
    memset(prog, 0, sizeof(*prog));

    prog->entityfields = header.entfield;
    prog->crc16 = header.crc16;

    prog->filename = util_strdup(filename);
    if (!prog->filename) {
        loaderror("failed to store program name");
        goto error;
    }

#define read_data(hdrvar, progvar, type)                                         \
    if (fseek(file, header.hdrvar.offset, SEEK_SET) != 0) {                      \
        loaderror("seek failed");                                                \
        goto error;                                                              \
    }                                                                            \
    prog->progvar##_alloc = header.hdrvar.length;                                \
    prog->progvar##_count = header.hdrvar.length;                                \
    prog->progvar = (type*)mem_a(header.hdrvar.length * sizeof(*prog->progvar)); \
    if (!prog->progvar)                                                          \
        goto error;                                                              \
    if (fread(prog->progvar, sizeof(*prog->progvar), header.hdrvar.length, file) \
        != header.hdrvar.length) {                                               \
        loaderror("read failed");                                                \
        goto error;                                                              \
    }
#define read_data1(x, y) read_data(x, x, y)

    read_data (statements, code, prog_section_statement);
    read_data1(defs,             prog_section_def);
    read_data1(fields,           prog_section_def);
    read_data1(functions,        prog_section_function);
    read_data1(strings,          char);
    read_data1(globals,          qcint);

    fclose(file);

    /* profile counters */
    if (!qc_program_profile_resize(prog, prog->code_count))
        goto error;

    /* Add tempstring area */
    prog->tempstring_start = prog->strings_count;
    prog->tempstring_at    = prog->strings_count;
    if (!qc_program_strings_resize(prog, prog->strings_count + 16*1024))
        goto error;

    /* spawn the world entity */
    if (!qc_program_entitypool_add(prog, true)) {
        loaderror("failed to allocate world entity\n");
        goto error;
    }
    for (i = 0; i < prog->entityfields; ++i) {
        if (!qc_program_entitydata_add(prog, 0)) {
            loaderror("failed to allocate world data\n");
            goto error;
        }
    }
    prog->entities = 1;

    return prog;

error:
    if (prog->filename)   mem_d(prog->filename);
    if (prog->code)       mem_d(prog->code);
    if (prog->defs)       mem_d(prog->defs);
    if (prog->fields)     mem_d(prog->fields);
    if (prog->functions)  mem_d(prog->functions);
    if (prog->strings)    mem_d(prog->strings);
    if (prog->globals)    mem_d(prog->globals);
    if (prog->entitydata) mem_d(prog->entitydata);
    if (prog->entitypool) mem_d(prog->entitypool);
    mem_d(prog);
    return NULL;
}

void prog_delete(qc_program *prog)
{
    if (prog->filename) mem_d(prog->filename);
    MEM_VECTOR_CLEAR(prog, code);
    MEM_VECTOR_CLEAR(prog, defs);
    MEM_VECTOR_CLEAR(prog, fields);
    MEM_VECTOR_CLEAR(prog, functions);
    MEM_VECTOR_CLEAR(prog, strings);
    MEM_VECTOR_CLEAR(prog, globals);
    MEM_VECTOR_CLEAR(prog, entitydata);
    MEM_VECTOR_CLEAR(prog, entitypool);
    MEM_VECTOR_CLEAR(prog, localstack);
    MEM_VECTOR_CLEAR(prog, stack);
    MEM_VECTOR_CLEAR(prog, profile);

    if (prog->builtins_alloc) {
        MEM_VECTOR_CLEAR(prog, builtins);
    }
    /* otherwise the builtins were statically allocated */
    mem_d(prog);
}

/***********************************************************************
 * VM code
 */

char* prog_getstring(qc_program *prog, qcint str)
{
    if (str < 0 || str >= prog->strings_count)
        return "<<<invalid string>>>";
    return prog->strings + str;
}

prog_section_def* prog_entfield(qc_program *prog, qcint off)
{
    size_t i;
    for (i = 0; i < prog->fields_count; ++i) {
        if (prog->fields[i].offset == off)
            return (prog->fields + i);
    }
    return NULL;
}

prog_section_def* prog_getdef(qc_program *prog, qcint off)
{
    size_t i;
    for (i = 0; i < prog->defs_count; ++i) {
        if (prog->defs[i].offset == off)
            return (prog->defs + i);
    }
    return NULL;
}

qcany* prog_getedict(qc_program *prog, qcint e)
{
    if (e >= prog->entitypool_count) {
        prog->vmerror++;
        printf("Accessing out of bounds edict %i\n", (int)e);
        e = 0;
    }
    return (qcany*)(prog->entitydata + (prog->entityfields * e));
}

qcint prog_spawn_entity(qc_program *prog)
{
    char  *data;
    size_t i;
    qcint  e;
    for (e = 0; e < (qcint)prog->entitypool_count; ++e) {
        if (!prog->entitypool[e]) {
            data = (char*)(prog->entitydata + (prog->entityfields * e));
            memset(data, 0, prog->entityfields * sizeof(qcint));
            return e;
        }
    }
    if (!qc_program_entitypool_add(prog, true)) {
        prog->vmerror++;
        printf("Failed to allocate entity\n");
        return 0;
    }
    prog->entities++;
    for (i = 0; i < prog->entityfields; ++i) {
        if (!qc_program_entitydata_add(prog, 0)) {
            printf("Failed to allocate entity\n");
            return 0;
        }
    }
    data = (char*)(prog->entitydata + (prog->entityfields * e));
    memset(data, 0, prog->entityfields * sizeof(qcint));
    return e;
}

void prog_free_entity(qc_program *prog, qcint e)
{
    if (!e) {
        prog->vmerror++;
        printf("Trying to free world entity\n");
        return;
    }
    if (e >= prog->entitypool_count) {
        prog->vmerror++;
        printf("Trying to free out of bounds entity\n");
        return;
    }
    if (!prog->entitypool[e]) {
        prog->vmerror++;
        printf("Double free on entity\n");
        return;
    }
    prog->entitypool[e] = false;
}

qcint prog_tempstring(qc_program *prog, const char *_str)
{
    /* we don't access it, but the macro-generated functions don't use
     * const
     */
    char *str = (char*)_str;

    size_t len = strlen(str);
    size_t at = prog->tempstring_at;

    /* when we reach the end we start over */
    if (at + len >= prog->strings_count)
        at = prog->tempstring_start;

    /* when it doesn't fit, reallocate */
    if (at + len >= prog->strings_count)
    {
        prog->strings_count = at;
        if (!qc_program_strings_append(prog, str, len+1)) {
            prog->vmerror = VMERR_TEMPSTRING_ALLOC;
            return 0;
        }
        return at;
    }

    /* when it fits, just copy */
    memcpy(prog->strings + at, str, len+1);
    prog->tempstring_at += len+1;
    return at;
}

static int print_escaped_string(const char *str, size_t maxlen)
{
    int len = 2;
    putchar('"');
    --maxlen; /* because we're lazy and have escape sequences */
    while (*str) {
        if (len >= maxlen) {
            putchar('.');
            putchar('.');
            putchar('.');
            len += 3;
            break;
        }
        switch (*str) {
            case '\a': len += 2; putchar('\\'); putchar('a'); break;
            case '\b': len += 2; putchar('\\'); putchar('b'); break;
            case '\r': len += 2; putchar('\\'); putchar('r'); break;
            case '\n': len += 2; putchar('\\'); putchar('n'); break;
            case '\t': len += 2; putchar('\\'); putchar('t'); break;
            case '\f': len += 2; putchar('\\'); putchar('f'); break;
            case '\v': len += 2; putchar('\\'); putchar('v'); break;
            case '\\': len += 2; putchar('\\'); putchar('\\'); break;
            case '"':  len += 2; putchar('\\'); putchar('"'); break;
            default:
                ++len;
                putchar(*str);
                break;
        }
        ++str;
    }
    putchar('"');
    return len;
}

static void trace_print_global(qc_program *prog, unsigned int glob, int vtype)
{
    static char spaces[28+1] = "                            ";
    prog_section_def *def;
    qcany    *value;
    int       len;

    if (!glob) {
        len = printf("<null>,");
        goto done;
    }

    def = prog_getdef(prog, glob);
    value = (qcany*)(&prog->globals[glob]);

    if (def) {
        const char *name = prog_getstring(prog, def->name);
        if (name[0] == '#')
            len = printf("$");
        else
            len = printf("%s ", name);
        vtype = def->type;
    }
    else
        len = printf("[@%u] ", glob);

    switch (vtype) {
        case TYPE_VOID:
        case TYPE_ENTITY:
        case TYPE_FIELD:
        case TYPE_FUNCTION:
        case TYPE_POINTER:
            len += printf("(%i),", value->_int);
            break;
        case TYPE_VECTOR:
            len += printf("'%g %g %g',", value->vector[0],
                                         value->vector[1],
                                         value->vector[2]);
            break;
        case TYPE_STRING:
            len += print_escaped_string(prog_getstring(prog, value->string), sizeof(spaces)-len-5);
            len += printf(",");
            /* len += printf("\"%s\",", prog_getstring(prog, value->string)); */
            break;
        case TYPE_FLOAT:
        default:
            len += printf("%g,", value->_float);
            break;
    }
done:
    if (len < sizeof(spaces)-1) {
        spaces[sizeof(spaces)-1-len] = 0;
        printf(spaces);
        spaces[sizeof(spaces)-1-len] = ' ';
    }
}

static void prog_print_statement(qc_program *prog, prog_section_statement *st)
{
    if (st->opcode >= (sizeof(asm_instr)/sizeof(asm_instr[0]))) {
        printf("<illegal instruction %d>\n", st->opcode);
        return;
    }
    printf(" <> %-12s", asm_instr[st->opcode].m);
    if (st->opcode >= INSTR_IF &&
        st->opcode <= INSTR_IFNOT)
    {
        trace_print_global(prog, st->o1.u1, TYPE_FLOAT);
        printf("%d\n", st->o2.s1);
    }
    else if (st->opcode >= INSTR_CALL0 &&
             st->opcode <= INSTR_CALL8)
    {
        printf("\n");
    }
    else if (st->opcode == INSTR_GOTO)
    {
        printf("%i\n", st->o1.s1);
    }
    else
    {
        int t[3] = { TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT };
        switch (st->opcode)
        {
            case INSTR_MUL_FV:
                t[1] = t[2] = TYPE_VECTOR;
                break;
            case INSTR_MUL_VF:
                t[0] = t[2] = TYPE_VECTOR;
                break;
            case INSTR_MUL_V:
                t[0] = t[1] = TYPE_VECTOR;
                break;
            case INSTR_ADD_V:
            case INSTR_SUB_V:
            case INSTR_EQ_V:
            case INSTR_NE_V:
                t[0] = t[1] = t[2] = TYPE_VECTOR;
                break;
            case INSTR_EQ_S:
            case INSTR_NE_S:
                t[0] = t[1] = TYPE_STRING;
                break;
            case INSTR_STORE_F:
            case INSTR_STOREP_F:
                t[2] = -1;
                break;
            case INSTR_STORE_V:
                t[0] = t[1] = TYPE_VECTOR; t[2] = -1;
                break;
            case INSTR_STORE_S:
                t[0] = t[1] = TYPE_STRING; t[2] = -1;
                break;
            case INSTR_STORE_ENT:
                t[0] = t[1] = TYPE_ENTITY; t[2] = -1;
                break;
            case INSTR_STORE_FLD:
                t[0] = t[1] = TYPE_FIELD; t[2] = -1;
                break;
            case INSTR_STORE_FNC:
                t[0] = t[1] = TYPE_FUNCTION; t[2] = -1;
                break;
            case INSTR_STOREP_V:
                t[0] = TYPE_VECTOR; t[1] = TYPE_ENTITY; t[2] = -1;
                break;
            case INSTR_STOREP_S:
                t[0] = TYPE_STRING; t[1] = TYPE_ENTITY; t[2] = -1;
                break;
            case INSTR_STOREP_ENT:
                t[0] = TYPE_ENTITY; t[1] = TYPE_ENTITY; t[2] = -1;
                break;
            case INSTR_STOREP_FLD:
                t[0] = TYPE_FIELD; t[1] = TYPE_ENTITY; t[2] = -1;
                break;
            case INSTR_STOREP_FNC:
                t[0] = TYPE_FUNCTION; t[1] = TYPE_ENTITY; t[2] = -1;
                break;
        }
        if (t[0] >= 0) trace_print_global(prog, st->o1.u1, t[0]);
        else           printf("(none),          ");
        if (t[1] >= 0) trace_print_global(prog, st->o2.u1, t[1]);
        else           printf("(none),          ");
        if (t[2] >= 0) trace_print_global(prog, st->o3.u1, t[2]);
        else           printf("(none)");
        printf("\n");
    }
    fflush(stdout);
}

static qcint prog_enterfunction(qc_program *prog, prog_section_function *func)
{
    qc_exec_stack st;
    size_t p, parampos;

    /* back up locals */
    st.localsp  = prog->localstack_count;
    st.stmt     = prog->statement;
    st.function = func;

#ifdef QCVM_BACKUP_STRATEGY_CALLER_VARS
    if (prog->stack_count)
    {
        prog_section_function *cur;
        cur = prog->stack[prog->stack_count-1].function;
        if (cur)
        {
            qcint *globals = prog->globals + cur->firstlocal;
            if (!qc_program_localstack_append(prog, globals, cur->locals))
            {
                printf("out of memory\n");
                exit(1);
            }
        }
    }
#else
    {
        qcint *globals = prog->globals + func->firstlocal;
        if (!qc_program_localstack_append(prog, globals, func->locals))
        {
            printf("out of memory\n");
            exit(1);
        }
    }
#endif

    /* copy parameters */
    parampos = func->firstlocal;
    for (p = 0; p < func->nargs; ++p)
    {
        size_t s;
        for (s = 0; s < func->argsize[p]; ++s) {
            prog->globals[parampos] = prog->globals[OFS_PARM0 + 3*p + s];
            ++parampos;
        }
    }

    if (!qc_program_stack_add(prog, st)) {
        printf("out of memory\n");
        exit(1);
    }

    return func->entry;
}

static qcint prog_leavefunction(qc_program *prog)
{
    prog_section_function *prev = NULL;
    size_t oldsp;

    qc_exec_stack st = prog->stack[prog->stack_count-1];

#ifdef QCVM_BACKUP_STRATEGY_CALLER_VARS
    if (prog->stack_count > 1) {
        prev  = prog->stack[prog->stack_count-2].function;
        oldsp = prog->stack[prog->stack_count-2].localsp;
    }
#else
    prev  = prog->stack[prog->stack_count-1].function;
    oldsp = prog->stack[prog->stack_count-1].localsp;
#endif
    if (prev) {
        qcint *globals = prog->globals + prev->firstlocal;
        memcpy(globals, prog->localstack + oldsp, prev->locals);
        if (!qc_program_localstack_resize(prog, oldsp)) {
            printf("out of memory\n");
            exit(1);
        }
    }

    if (!qc_program_stack_remove(prog, prog->stack_count-1)) {
        printf("out of memory\n");
        exit(1);
    }

    return st.stmt - 1; /* offset the ++st */
}

bool prog_exec(qc_program *prog, prog_section_function *func, size_t flags, long maxjumps)
{
    long jumpcount = 0;
    size_t oldxflags = prog->xflags;
    prog_section_statement *st;

    prog->vmerror = 0;
    prog->xflags = flags;

    st = prog->code + prog_enterfunction(prog, func);
    --st;
    switch (flags)
    {
        default:
        case 0:
        {
#define QCVM_PROFILE 0
#define QCVM_TRACE   0
#           include "execloop.h"
            break;
        }
        case (VMXF_TRACE):
        {
#define QCVM_PROFILE 0
#define QCVM_TRACE   1
#           include "execloop.h"
            break;
        }
        case (VMXF_PROFILE):
        {
#define QCVM_PROFILE 1
#define QCVM_TRACE   0
#           include "execloop.h"
            break;
        }
        case (VMXF_TRACE|VMXF_PROFILE):
        {
#define QCVM_PROFILE 1
#define QCVM_TRACE   1
#           include "execloop.h"
            break;
        }
    };

cleanup:
    prog->xflags = oldxflags;
    prog->localstack_count = 0;
    prog->stack_count = 0;
    if (prog->vmerror)
        return false;
    return true;
}

/***********************************************************************
 * main for when building the standalone executor
 */

#if defined(QCVM_EXECUTOR)
#include <math.h>

const char *type_name[TYPE_COUNT] = {
    "void",
    "string",
    "float",
    "vector",
    "entity",
    "field",
    "function",
    "pointer",
#if 0
    "integer",
#endif
    "variant"
};

bool        opts_debug    = false;
bool        opts_memchk   = false;

#define CheckArgs(num) do {                                                    \
    if (prog->argc != (num)) {                                                 \
        prog->vmerror++;                                                       \
        printf("ERROR: invalid number of arguments for %s: %i, expected %i\n", \
        __FUNCTION__, prog->argc, (num));                                      \
        return -1;                                                             \
    }                                                                          \
} while (0)

#define GetGlobal(idx) ((qcany*)(prog->globals + (idx)))
#define GetArg(num) GetGlobal(OFS_PARM0 + 3*(num))
#define Return(any) *(GetGlobal(OFS_RETURN)) = (any)

static int qc_print(qc_program *prog)
{
    size_t i;
    const char *laststr = NULL;
    for (i = 0; i < prog->argc; ++i) {
        qcany *str = (qcany*)(prog->globals + OFS_PARM0 + 3*i);
        printf("%s", (laststr = prog_getstring(prog, str->string)));
    }
    if (laststr && (prog->xflags & VMXF_TRACE)) {
        size_t len = strlen(laststr);
        if (!len || laststr[len-1] != '\n')
            printf("\n");
    }
    return 0;
}

static int qc_error(qc_program *prog)
{
    printf("*** VM raised an error:\n");
    qc_print(prog);
    prog->vmerror++;
    return -1;
}

static int qc_ftos(qc_program *prog)
{
    char buffer[512];
    qcany *num;
    qcany str;
    CheckArgs(1);
    num = GetArg(0);
    snprintf(buffer, sizeof(buffer), "%g", num->_float);
    str.string = prog_tempstring(prog, buffer);
    Return(str);
    return 0;
}

static int qc_vtos(qc_program *prog)
{
    char buffer[512];
    qcany *num;
    qcany str;
    CheckArgs(1);
    num = GetArg(0);
    snprintf(buffer, sizeof(buffer), "`%g %g %g`", num->vector[0], num->vector[1], num->vector[2]);
    str.string = prog_tempstring(prog, buffer);
    Return(str);
    return 0;
}

static int qc_spawn(qc_program *prog)
{
    qcany ent;
    CheckArgs(0);
    ent.edict = prog_spawn_entity(prog);
    Return(ent);
    return (ent.edict ? 0 : -1);
}

static int qc_kill(qc_program *prog)
{
    qcany *ent;
    CheckArgs(1);
    ent = GetArg(0);
    prog_free_entity(prog, ent->edict);
    return 0;
}

static int qc_vlen(qc_program *prog)
{
    qcany *vec, len;
    CheckArgs(1);
    vec = GetArg(0);
    len._float = sqrt(vec->vector[0] * vec->vector[0] + 
                      vec->vector[1] * vec->vector[1] +
                      vec->vector[2] * vec->vector[2]);
    Return(len);
    return 0;
}

static prog_builtin qc_builtins[] = {
    NULL,
    &qc_print, /*   1   */
    &qc_ftos,  /*   2   */
    &qc_spawn, /*   3   */
    &qc_kill,  /*   4   */
    &qc_vtos,  /*   5   */
    &qc_error, /*   6   */
    &qc_vlen   /*   7   */
};
static size_t qc_builtins_count = sizeof(qc_builtins) / sizeof(qc_builtins[0]);

static const char *arg0 = NULL;

void usage()
{
    printf("usage: [-debug] %s file\n", arg0);
    exit(1);
}

int main(int argc, char **argv)
{
    size_t      i;
    qcint       fnmain = -1;
    qc_program *prog;
    size_t      xflags = VMXF_DEFAULT;
    bool        opts_printfields = false;
    bool        opts_printdefs   = false;
    bool        opts_info  = false;

    arg0 = argv[0];

    if (argc < 2)
        usage();

    while (argc > 2) {
        if (!strcmp(argv[1], "-trace")) {
            --argc;
            ++argv;
            xflags |= VMXF_TRACE;
        }
        else if (!strcmp(argv[1], "-profile")) {
            --argc;
            ++argv;
            xflags |= VMXF_PROFILE;
        }
        else if (!strcmp(argv[1], "-info")) {
            --argc;
            ++argv;
            opts_info = true;
        }
        else if (!strcmp(argv[1], "-printdefs")) {
            --argc;
            ++argv;
            opts_printdefs = true;
        }
        else if (!strcmp(argv[1], "-printfields")) {
            --argc;
            ++argv;
            opts_printfields = true;
        }
        else
            usage();
    }


    prog = prog_load(argv[1]);
    if (!prog) {
        printf("failed to load program '%s'\n", argv[1]);
        exit(1);
    }

    prog->builtins       = qc_builtins;
    prog->builtins_count = qc_builtins_count;
    prog->builtins_alloc = 0;

    printf("Program's system-checksum = 0x%04x\n", (int)prog->crc16);

    for (i = 1; i < prog->functions_count; ++i) {
        const char *name = prog_getstring(prog, prog->functions[i].name);
        /* printf("Found function: %s\n", name); */
        if (!strcmp(name, "main"))
            fnmain = (qcint)i;
    }
    printf("Entity field space: %i\n", (int)prog->entityfields);
    if (opts_info) {
        prog_delete(prog);
        return 0;
    }
    if (opts_printdefs) {
        for (i = 0; i < prog->defs_count; ++i) {
            printf("Global: %8s %-16s at %u\n",
                   type_name[prog->defs[i].type],
                   prog_getstring(prog, prog->defs[i].name),
                   (unsigned int)prog->defs[i].offset);
        }
    }
    else if (opts_printfields) {
        for (i = 0; i < prog->fields_count; ++i) {
            printf("Field: %8s %-16s at %u\n",
                   type_name[prog->fields[i].type],
                   prog_getstring(prog, prog->fields[i].name),
                   (unsigned int)prog->fields[i].offset);
        }
    }
    else
    {
        if (fnmain > 0)
        {
            prog_exec(prog, &prog->functions[fnmain], xflags, VM_JUMPS_DEFAULT);
        }
        else
            printf("No main function found\n");
    }

    prog_delete(prog);
    return 0;
}
#endif
