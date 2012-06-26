#include "gmqcc.h"

#define QCVM_EXECUTOR

#define _MEM_VEC_FUN_APPEND(Tself, Twhat, mem)                       \
bool GMQCC_WARN Tself##_##mem##_append(Tself *s, Twhat *p, size_t c) \
{                                                                    \
    Twhat *reall;                                                    \
    if (s->mem##_count+c >= s->mem##_alloc) {                        \
        if (!s->mem##_alloc) {                                       \
            s->mem##_alloc = c < 16 ? 16 : c;                        \
        } else {                                                     \
            s->mem##_alloc *= 2;                                     \
            if (s->mem##_count+c >= s->mem##_alloc) {                \
                s->mem##_alloc = s->mem##_count+c;                   \
            }                                                        \
        }                                                            \
        reall = (Twhat*)mem_a(sizeof(Twhat) * s->mem##_alloc);       \
        if (!reall) {                                                \
            return false;                                            \
        }                                                            \
        memcpy(reall, s->mem, sizeof(Twhat) * s->mem##_count);       \
        mem_d(s->mem);                                               \
        s->mem = reall;                                              \
    }                                                                \
    memcpy(&s->mem[s->mem##_count], p, c*sizeof(*p));                \
    s->mem##_count += c;                                             \
    return true;                                                     \
}

#define _MEM_VEC_FUN_RESIZE(Tself, Twhat, mem)                   \
bool GMQCC_WARN Tself##_##mem##_resize(Tself *s, size_t c)       \
{                                                                \
    Twhat *reall;                                                \
    reall = (Twhat*)mem_a(sizeof(Twhat) * c);                    \
    if (c > s->mem##_count) {                                    \
        memcpy(reall, s->mem, sizeof(Twhat) * s->mem##_count);   \
    } else {                                                     \
        memcpy(reall, s->mem, sizeof(Twhat) * c);                \
    }                                                            \
    s->mem##_count = c;                                          \
    s->mem##_alloc = c;                                          \
    return true;                                                 \
}

/* darkplaces has (or will have) a 64 bit prog loader
 * where the 32 bit qc program is autoconverted on load.
 * Since we may want to support that as well, let's redefine
 * float and int here.
 */
typedef float   qcfloat;
typedef int32_t qcint;

typedef char qcfloat_size_is_correct [sizeof(qcfloat) == 4 ?1:-1];
typedef char qcint_size_is_correct   [sizeof(int)     == 4 ?1:-1];

typedef struct {
    uint32_t offset;
    uint32_t length;
} prog_section;

typedef struct {
    uint32_t     version;
    uint16_t     crc16;
    uint16_t     skip;

    prog_section statements;
    prog_section defs;
    prog_section fields;
    prog_section functions;
    prog_section strings;
    prog_section globals;
    uint32_t     entfield;
} prog_header;

typedef prog_section_both      prog_def;
typedef prog_section_function  prog_function;
typedef prog_section_statement prog_statement;

enum {
    VMERR_OK,
    VMERR_TEMPSTRING_ALLOC,

    VMERR_END
};

typedef struct {
    char           *filename;

    MEM_VECTOR_MAKE(prog_statement, code);
    MEM_VECTOR_MAKE(prog_def,       defs);
    MEM_VECTOR_MAKE(prog_def,       fields);
    MEM_VECTOR_MAKE(prog_function,  functions);
    MEM_VECTOR_MAKE(char,           strings);
    MEM_VECTOR_MAKE(qcint,          globals);
    MEM_VECTOR_MAKE(qcint,          entitydata);

    size_t tempstring_start;
    size_t tempstring_at;

    qcint  vmerror;

    MEM_VECTOR_MAKE(qcint,  localstack);
    MEM_VECTOR_MAKE(size_t, localsp);
} qc_program;
MEM_VEC_FUNCTIONS(qc_program, prog_statement, code)
MEM_VEC_FUNCTIONS(qc_program, prog_def,       defs)
MEM_VEC_FUNCTIONS(qc_program, prog_def,       fields)
MEM_VEC_FUNCTIONS(qc_program, prog_function,  functions)
MEM_VEC_FUNCTIONS(qc_program, char,           strings)
_MEM_VEC_FUN_APPEND(qc_program, char, strings)
_MEM_VEC_FUN_RESIZE(qc_program, char, strings)
MEM_VEC_FUNCTIONS(qc_program, qcint,          globals)
MEM_VEC_FUNCTIONS(qc_program, qcint,          entitydata)

MEM_VEC_FUNCTIONS(qc_program,   qcint, localstack)
_MEM_VEC_FUN_APPEND(qc_program, qcint, localstack)
_MEM_VEC_FUN_RESIZE(qc_program, qcint, localstack)
MEM_VEC_FUNCTIONS(qc_program,   size_t, localsp)

qc_program* prog_load(const char *filename)
{
    qc_program *prog;
    prog_header header;
    FILE *file;

    file = fopen(filename, "rb");
    if (!file)
        return NULL;

    if (fread(&header, sizeof(header), 1, file) != 1) {
        perror("read");
        fclose(file);
        return NULL;
    }

    if (header.version != 6) {
        printf("header says this is a version %i progs, we need version 6\n",
               header.version);
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

    prog->filename = util_strdup(filename);
    if (!prog->filename)
        goto error;
#define read_data(hdrvar, progvar, type)                                         \
    if (fseek(file, header.hdrvar.offset, SEEK_SET) != 0) {                      \
        perror("fseek");                                                         \
        goto error;                                                              \
    }                                                                            \
    prog->progvar##_alloc = header.hdrvar.length;                                \
    prog->progvar##_count = header.hdrvar.length;                                \
    prog->progvar = (type*)mem_a(header.hdrvar.length * sizeof(*prog->progvar)); \
    if (!prog->progvar)                                                          \
        goto error;                                                              \
    if (fread(prog->progvar, sizeof(*prog->progvar), header.hdrvar.length, file) \
        != header.hdrvar.length) {                                               \
        perror("read");                                                          \
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
    MEM_VECTOR_CLEAR(prog, localsp);
    mem_d(prog);
}

/***********************************************************************
 * VM code
 */

char* prog_getstring(qc_program *prog, qcint str)
{
    if (str < 0 || str >= prog->strings_count)
        return prog->strings;
    return prog->strings + str;
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

/***********************************************************************
 * main for when building the standalone executor
 */

#if defined(QCVM_EXECUTOR)
int main(int argc, char **argv)
{
    size_t      i;
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
        printf("Found function: %s\n", prog_getstring(prog, prog->functions[i].name));
    }

    prog_delete(prog);
    return 0;
}
#endif
