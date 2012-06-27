#include <errno.h>
#include <string.h>
#include <stdarg.h>

#include "gmqcc.h"

#include "exec.h"

static void loaderror(const char *fmt, ...)
{
    int     err = errno;
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf(": %s\n", strerror(err));
}

qc_program* prog_load(const char *filename)
{
    qc_program *prog;
    prog_header header;
    FILE *file;

    file = fopen(filename, "rb");
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

    read_data (statements, code, prog_statement);
    read_data1(defs,             prog_def);
    read_data1(fields,           prog_def);
    read_data1(functions,        prog_function);
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
    MEM_VECTOR_CLEAR(prog, localstack);
    MEM_VECTOR_CLEAR(prog, stack);
    MEM_VECTOR_CLEAR(prog, profile);
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

prog_def* prog_entfield(qc_program *prog, qcint off)
{
    size_t i;
    for (i = 0; i < prog->fields_count; ++i) {
        if (prog->fields[i].offset == off)
            return (prog->fields + i);
    }
    return NULL;
}

prog_def* prog_getdef(qc_program *prog, qcint off)
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
    return (qcany*)(prog->entitydata + (prog->entityfields + e));
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

static void trace_print_global(qc_program *prog, unsigned int glob, int vtype)
{
    static char spaces[16+1] = "            ";
    prog_def *def;
    qcany    *value;
    int       len;

    if (!glob)
        return;

    def = prog_getdef(prog, glob);
    value = (qcany*)(&prog->globals[glob]);

    if (def) {
        len = printf("[%s] ", prog_getstring(prog, def->name));
        vtype = def->type;
    }
    else
        len = printf("[#%u] ", glob);

    switch (vtype) {
        case TYPE_VOID:
        case TYPE_ENTITY:
        case TYPE_FIELD:
        case TYPE_FUNCTION:
        case TYPE_POINTER:
            len += printf("%i,", value->_int);
            break;
        case TYPE_VECTOR:
            len += printf("'%g %g %g',", value->vector[0],
                                         value->vector[1],
                                         value->vector[2]);
            break;
        case TYPE_STRING:
            len += printf("\"%s\",", prog_getstring(prog, value->string));
            break;
        case TYPE_FLOAT:
        default:
            len += printf("%g,", value->_float);
            break;
    }
    if (len < 16) {
        spaces[16-len] = 0;
        printf(spaces);
        spaces[16-len] = ' ';
    }
}

static void prog_print_statement(qc_program *prog, prog_statement *st)
{
    if (st->opcode >= (sizeof(asm_instr)/sizeof(asm_instr[0]))) {
        printf("<illegal instruction %d>\n", st->opcode);
        return;
    }
    printf("%-12s", asm_instr[st->opcode].m);
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
            case INSTR_STORE_V:
                t[0] = t[1] = TYPE_VECTOR; t[2] = -1;
                break;
            case INSTR_STORE_S:
                t[0] = t[1] = TYPE_STRING; t[2] = -1;
                break;
        }
        if (t[0] >= 0) trace_print_global(prog, st->o1.u1, t[0]);
        if (t[1] >= 0) trace_print_global(prog, st->o2.u1, t[1]);
        if (t[2] >= 0) trace_print_global(prog, st->o3.u1, t[2]);
        printf("\n");
    }
}

static qcint prog_enterfunction(qc_program *prog, prog_function *func)
{
    qc_exec_stack st;
    prog_function *cur = NULL;

    if (prog->stack_count)
        cur = prog->stack[prog->stack_count-1].function;

    /* back up locals */
    st.localsp  = prog->localstack_count;
    st.stmt     = prog->statement;
    st.function = func;

    if (cur)
    {
        qcint *globals = prog->globals + cur->firstlocal;
        if (!qc_program_localstack_append(prog, globals, cur->locals))
        {
            printf("out of memory\n");
            exit(1);
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
    qc_exec_stack st = prog->stack[prog->stack_count-1];
    if (!qc_program_stack_remove(prog, prog->stack_count-1)) {
        printf("out of memory\n");
        exit(1);
    }

    if (st.localsp != prog->localstack_count) {
        if (!qc_program_localstack_resize(prog, st.localsp)) {
            printf("out of memory\n");
            exit(1);
        }
    }

    return st.stmt;
}

bool prog_exec(qc_program *prog, prog_function *func, size_t flags, long maxjumps)
{
    long jumpcount = 0;
    prog_statement *st;

    st = prog->code + prog_enterfunction(prog, func);
    --st;
    switch (flags)
    {
        default:
        case 0:
        {
#define QCVM_PROFILE 0
#define QCVM_TRACE   0
#           include "qcvm_execprogram.h"
            break;
        }
        case (VMXF_TRACE):
        {
#define QCVM_PROFILE 0
#define QCVM_TRACE   1
#           include "qcvm_execprogram.h"
            break;
        }
        case (VMXF_PROFILE):
        {
#define QCVM_PROFILE 1
#define QCVM_TRACE   0
#           include "qcvm_execprogram.h"
            break;
        }
        case (VMXF_TRACE|VMXF_PROFILE):
        {
#define QCVM_PROFILE 1
#define QCVM_TRACE   1
#           include "qcvm_execprogram.h"
            break;
        }
    };

cleanup:
    prog->localstack_count = 0;
    prog->stack_count = 0;
    return true;
}

/***********************************************************************
 * main for when building the standalone executor
 */

#if defined(QCVM_EXECUTOR)
int main(int argc, char **argv)
{
    size_t      i;
    qcint       fnmain = -1;
    qc_program *prog;

    if (argc != 2) {
        printf("usage: %s file\n", argv[0]);
        exit(1);
    }

    prog = prog_load(argv[1]);
    if (!prog) {
        printf("failed to load program '%s'\n", argv[1]);
        exit(1);
    }

    for (i = 1; i < prog->functions_count; ++i) {
        const char *name = prog_getstring(prog, prog->functions[i].name);
        printf("Found function: %s\n", name);
        if (!strcmp(name, "main"))
            fnmain = (qcint)i;
    }
    if (fnmain > 0)
    {
        prog_exec(prog, &prog->functions[fnmain], VMXF_TRACE, JUMPS_DEFAULT);
    }
    else
        printf("No main function found\n");

    prog_delete(prog);
    return 0;
}
#endif
