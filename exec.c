#include "gmqcc.h"

#define QCVM_EXECUTOR

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

typedef struct {
    char           *filename;

    prog_statement *code;
    prog_def       *defs;
    prog_def       *fields;
    prog_function  *functions;
    char           *strings;
    qcint          *globals;
    qcint          *entitydata;
} qc_program;

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
    if (prog->filename)   mem_d(prog->filename);
    if (prog->code)       mem_d(prog->code);
    if (prog->defs)       mem_d(prog->defs);
    if (prog->fields)     mem_d(prog->fields);
    if (prog->functions)  mem_d(prog->functions);
    if (prog->strings)    mem_d(prog->strings);
    if (prog->globals)    mem_d(prog->globals);
    if (prog->entitydata) mem_d(prog->entitydata);
    mem_d(prog);
}

#if defined(QCVM_EXECUTOR)
int main(int argc, char **argv)
{
    qc_program *prog;

    if (argc != 2) {
        printf("usage: %s prog.dat\n", argv[0]);
        exit(1);
    }

    prog = prog_load(argv[1]);
    if (!prog) {
        printf("failed to load program '%s'\n", argv[1]);
        exit(1);
    }

    prog_delete(prog);
    return 0;
}
#endif
