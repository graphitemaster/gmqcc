/*
 * Copyright (C) 2012
 *     Wolfgang Bumiller
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef GMQCC_EXEC_HDR
#define GMQCC_EXEC_HDR

/* darkplaces has (or will have) a 64 bit prog loader
 * where the 32 bit qc program is autoconverted on load.
 * Since we may want to support that as well, let's redefine
 * float and int here.
 */
typedef float   qcfloat;
typedef int32_t qcint;

typedef union {
    qcint   _int;
    qcint    string;
    qcint    function;
    qcint    edict;
    qcfloat _float;
    qcfloat vector[3];
    qcint   ivector[3];
} qcany;

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

#define JUMPS_DEFAULT 1000000

#define VMXF_DEFAULT 0x0000
#define VMXF_TRACE   0x0001
#define VMXF_PROFILE 0x0002

struct qc_program_s;

typedef int (*prog_builtin)(struct qc_program_s *prog);

typedef struct {
    qcint          stmt;
    size_t         localsp;
    prog_function *function;
} qc_exec_stack;

typedef struct qc_program_s {
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

    MEM_VECTOR_MAKE(size_t, profile);

    MEM_VECTOR_MAKE(prog_builtin, builtins);

    /* size_t ip; */
    qcint  entities;
    size_t entityfields;
    bool   allowworldwrites;

    MEM_VECTOR_MAKE(qcint,         localstack);
    MEM_VECTOR_MAKE(qc_exec_stack, stack);
    size_t statement;

    int    argc; /* current arg count for debugging */
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
MEM_VEC_FUNCTIONS(qc_program,   qc_exec_stack, stack)

MEM_VEC_FUNCTIONS(qc_program,   size_t, profile)
_MEM_VEC_FUN_RESIZE(qc_program, size_t, profile)

qc_program* prog_load(const char *filename);
void        prog_delete(qc_program *prog);

bool prog_exec(qc_program *prog, prog_function *func, size_t flags, long maxjumps);

char*     prog_getstring(qc_program *prog, qcint str);
prog_def* prog_entfield(qc_program *prog, qcint off);
prog_def* prog_getdef(qc_program *prog, qcint off);
qcany*    prog_getedict(qc_program *prog, qcint e);
qcint     prog_tempstring(qc_program *prog, const char *_str);

#endif /* GMQCC_EXEC_HDR */
