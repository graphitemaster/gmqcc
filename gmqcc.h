/*
 * Copyright (C) 2012
 *     Dale Weiler, Wolfgang Bumiller
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
#ifndef GMQCC_HDR
#define GMQCC_HDR
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

/*
 * Disable some over protective warnings in visual studio because fixing them is a waste
 * of my time.
 */
#ifdef _MSC_VER
#	pragma warning(disable : 4244 ) /* conversion from 'int' to 'float', possible loss of data */
#	pragma warning(disable : 4018 ) /* signed/unsigned mismatch                                */
#	pragma warning(disable : 4996 ) /* This function or variable may be unsafe                 */
#	pragma warning(disable : 4700 ) /* uninitialized local variable used                       */
#endif

#define GMQCC_VERSION_MAJOR 0
#define GMQCC_VERSION_MINOR 2
#define GMQCC_VERSION_PATCH 0
#define GMQCC_VERSION_BUILD(J,N,P) (((J)<<16)|((N)<<8)|(P))
#define GMQCC_VERSION \
    GMQCC_VERSION_BUILD(GMQCC_VERSION_MAJOR, GMQCC_VERSION_MINOR, GMQCC_VERSION_PATCH)

/*
 * We cannoy rely on C99 at all, since compilers like MSVC
 * simply don't support it.  We define our own boolean type
 * as a result (since we cannot include <stdbool.h>). For
 * compilers that are in 1999 mode (C99 compliant) we can use
 * the language keyword _Bool which can allow for better code
 * on GCC and GCC-like compilers, opposed to `int`.
 */
#ifndef __cplusplus
#   ifdef  false
#       undef  false
#   endif /* !false */
#   ifdef  true
#       undef true
#   endif /* !true  */
#   define false (0)
#   define true  (1)
#   ifdef __STDC_VERSION__
#       if __STDC_VERSION__ < 199901L && __GNUC__ < 3
            typedef int  bool;
#       else
            typedef _Bool bool;
#       endif
#   else
        typedef int bool;
#   endif /* !__STDC_VERSION__ */
#endif    /* !__cplusplus      */

/*
 * Of some functions which are generated we want to make sure
 * that the result isn't ignored. To find such function calls,
 * we use this macro.
 */
#if defined(__GNUC__) || defined(__CLANG__)
#   define GMQCC_WARN __attribute__((warn_unused_result))
#else
#   define GMQCC_WARN
#endif
/*
 * This is a hack to silent clang regarding empty
 * body if statements.
 */
#define GMQCC_SUPPRESS_EMPTY_BODY do { } while (0)

/*
 * Inline is not supported in < C90, however some compilers
 * like gcc and clang might have an inline attribute we can
 * use if present.
 */
#ifdef __STDC_VERSION__
#    if __STDC_VERSION__ < 199901L
#       if defined(__GNUC__) || defined (__CLANG__)
#           if __GNUC__ < 2
#               define GMQCC_INLINE
#           else
#               define GMQCC_INLINE __attribute__ ((always_inline))
#           endif
#       else
#           define GMQCC_INLINE
#       endif
#    else
#       define GMQCC_INLINE inline
#    endif
#else
#    define GMQCC_INLINE
#endif /* !__STDC_VERSION__ */

/*
 * noreturn is present in GCC and clang
 * it's required for _ast_node_destory otherwise -Wmissing-noreturn
 * in clang complains about there being no return since abort() is
 * called.
 */
#if (defined(__GNUC__) && __GNUC__ >= 2) || defined(__CLANG__)
#    define GMQCC_NORETURN __attribute__ ((noreturn))
#else
#    define GMQCC_NORETURN
#endif

/*
 * stdint.h and inttypes.h -less subset
 * for systems that don't have it, which we must
 * assume is all systems. (int8_t not required)
 */
#if   CHAR_MIN  == -128
    typedef unsigned char  uint8_t; /* same as below */
#elif SCHAR_MIN == -128
    typedef unsigned char  uint8_t; /* same as above */
#endif
#if   SHRT_MAX  == 0x7FFF
    typedef short          int16_t;
    typedef unsigned short uint16_t;
#elif INT_MAX   == 0x7FFF
    typedef int            int16_t;
    typedef unsigned int   uint16_t;
#endif
#if   INT_MAX   == 0x7FFFFFFF
    typedef int            int32_t;
    typedef unsigned int   uint32_t;
#elif LONG_MAX  == 0x7FFFFFFF
    typedef long           int32_t;
    typedef unsigned long  uint32_t;
#endif


#if defined(__GNUC__) || defined (__CLANG__)
	typedef int          int64_t  __attribute__((__mode__(__DI__)));
	typedef unsigned int uint64_t __attribute__((__mode__(__DI__)));
#elif defined(_MSC_VER)
	typedef __int64          int64_t;
	typedef unsigned __int64 uint64_t;
#else
    /*
    * Incoorectly size the types so static assertions below will
    * fail.  There is no valid way to get a 64bit type at this point
    * without making assumptions of too many things.
    */
    typedef struct { char _fail : 0; } int64_t;
    typedef struct { char _fail : 0; } uint64_t;
#endif
#ifdef _LP64 /* long pointer == 64 */
    typedef unsigned long  uintptr_t;
    typedef long           intptr_t;
#else
    typedef unsigned int   uintptr_t;
    typedef int            intptr_t;
#endif
/* Ensure type sizes are correct: */
typedef char uint8_size_is_correct  [sizeof(uint8_t)  == 1?1:-1];
typedef char uint16_size_is_correct [sizeof(uint16_t) == 2?1:-1];
typedef char uint32_size_is_correct [sizeof(uint32_t) == 4?1:-1];
typedef char uint64_size_is_correct [sizeof(uint64_t) == 8?1:-1];
typedef char int16_size_if_correct  [sizeof(int16_t)  == 2?1:-1];
typedef char int32_size_is_correct  [sizeof(int32_t)  == 4?1:-1];
typedef char int64_size_is_correct  [sizeof(int64_t)  >= 8?1:-1];
/* intptr_t / uintptr_t correct size check */
typedef char uintptr_size_is_correct[sizeof(intptr_t) == sizeof(int*)?1:-1];
typedef char intptr_size_is_correct [sizeof(uintptr_t)== sizeof(int*)?1:-1];

/*===================================================================*/
/*=========================== util.c ================================*/
/*===================================================================*/
FILE *util_fopen(const char *filename, const char *mode);

void *util_memory_a      (size_t,       unsigned int, const char *);
void  util_memory_d      (void       *, unsigned int, const char *);
void *util_memory_r      (void       *, size_t,       unsigned int, const char *);
void  util_meminfo       ();

bool  util_filexists     (const char *);
bool  util_strupper      (const char *);
bool  util_strdigit      (const char *);
bool  util_strncmpexact  (const char *, const char *, size_t);
char *util_strdup        (const char *);
char *util_strrq         (const char *);
char *util_strrnl        (const char *);
char *util_strsws        (const char *);
char *util_strchp        (const char *, const char *);
void  util_debug         (const char *, const char *, ...);
int   util_getline       (char **, size_t *, FILE *);
void  util_endianswap    (void *,  int, int);

size_t util_strtocmd    (const char *, char *, size_t);
size_t util_strtononcmd (const char *, char *, size_t);

uint16_t util_crc16(uint16_t crc, const char *data, size_t len);
uint32_t util_crc32(uint32_t crc, const char *data, size_t len);

#ifdef NOTRACK
#    define mem_a(x)    malloc (x)
#    define mem_d(x)    free   (x)
#    define mem_r(x, n) realloc(x, n)
#else
#    define mem_a(x)    util_memory_a((x), __LINE__, __FILE__)
#    define mem_d(x)    util_memory_d((x), __LINE__, __FILE__)
#    define mem_r(x, n) util_memory_r((x), (n), __LINE__, __FILE__)
#endif

/*
 * TODO: make these safer to use.  Currently this only works on
 * x86 and x86_64, some systems will likely not like this. Such
 * as BE systems.
 */
#define FLT2INT(Y) *((int32_t*)&(Y))
#define INT2FLT(Y) *((float  *)&(Y))

/* New flexible vector implementation from Dale */
#define _vec_raw(A) (((size_t*)(void*)(A)) - 2)
#define _vec_beg(A) (_vec_raw(A)[0])
#define _vec_end(A) (_vec_raw(A)[1])
#define _vec_needsgrow(A,N) ((!(A)) || (_vec_end(A) + (N) >= _vec_beg(A)))
#define _vec_mightgrow(A,N) (_vec_needsgrow((A), (N)) ? (void)_vec_forcegrow((A),(N)) : (void)0)
#define _vec_forcegrow(A,N) _util_vec_grow(((void**)&(A)), (N), sizeof(*(A)))
#define _vec_remove(A,S,I,N) (memmove((char*)(A)+(I)*(S),(char*)(A)+((I)+(N))*(S),(S)*(vec_size(A)-(I)-(N))), _vec_end(A)-=(N))
void _util_vec_grow(void **a, size_t i, size_t s);
/* exposed interface */
#define vec_free(A)          ((A) ? (mem_d((void*)_vec_raw(A)), (A) = NULL) : 0)
#define vec_push(A,V)        (_vec_mightgrow((A),1), (A)[_vec_end(A)++] = (V))
#define vec_size(A)          ((A) ? _vec_end(A) : 0)
#define vec_add(A,N)         (_vec_mightgrow((A),(N)), _vec_end(A)+=(N), &(A)[_vec_end(A)-(N)])
#define vec_last(A)          ((A)[_vec_end(A)-1])
#define vec_append(A,N,S)    memcpy(vec_add((A), (N)), (S), N * sizeof(*(S)))
#define vec_remove(A,I,N)    _vec_remove((A), sizeof(*(A)), (I), (N))
#define vec_pop(A)           vec_remove((A), _vec_end(A)-1, 1)
/* these are supposed to NOT reallocate */
#define vec_shrinkto(A,N)    (_vec_end(A) = (N))
#define vec_shrinkby(A,N)    (_vec_end(A) -= (N))

/*===================================================================*/
/*=========================== code.c ================================*/
/*===================================================================*/

/* Note: if you change the order, fix type_sizeof in ir.c */
enum {
    TYPE_VOID     ,
    TYPE_STRING   ,
    TYPE_FLOAT    ,
    TYPE_VECTOR   ,
    TYPE_ENTITY   ,
    TYPE_FIELD    ,
    TYPE_FUNCTION ,
    TYPE_POINTER  ,
    TYPE_INTEGER  ,
    TYPE_VARIANT  ,
    TYPE_STRUCT   ,
    TYPE_UNION    ,
    TYPE_ARRAY    ,

    TYPE_COUNT
};

extern const char *type_name[TYPE_COUNT];

extern size_t type_sizeof[TYPE_COUNT];
extern uint16_t type_store_instr[TYPE_COUNT];
extern uint16_t field_store_instr[TYPE_COUNT];
/* could use type_store_instr + INSTR_STOREP_F - INSTR_STORE_F
 * but this breaks when TYPE_INTEGER is added, since with the enhanced
 * instruction set, the old ones are left untouched, thus the _I instructions
 * are at a seperate place.
 */
extern uint16_t type_storep_instr[TYPE_COUNT];
/* other useful lists */
extern uint16_t type_eq_instr[TYPE_COUNT];
extern uint16_t type_ne_instr[TYPE_COUNT];
extern uint16_t type_not_instr[TYPE_COUNT];

typedef struct {
    uint32_t offset;      /* Offset in file of where data begins  */
    uint32_t length;      /* Length of section (how many of)      */
} prog_section;

typedef struct {
    uint32_t     version;      /* Program version (6)     */
    uint16_t     crc16;        /* What is this?           */
    uint16_t     skip;         /* see propsal.txt         */

    prog_section statements;   /* prog_section_statement  */
    prog_section defs;         /* prog_section_def        */
    prog_section fields;       /* prog_section_field      */
    prog_section functions;    /* prog_section_function   */
    prog_section strings;      /* What is this?           */
    prog_section globals;      /* What is this?           */
    uint32_t     entfield;     /* Number of entity fields */
} prog_header;

/*
 * Each paramater incerements by 3 since vector types hold
 * 3 components (x,y,z).
 */
#define OFS_NULL      0
#define OFS_RETURN    1
#define OFS_PARM0     (OFS_RETURN+3)
#define OFS_PARM1     (OFS_PARM0 +3)
#define OFS_PARM2     (OFS_PARM1 +3)
#define OFS_PARM3     (OFS_PARM2 +3)
#define OFS_PARM4     (OFS_PARM3 +3)
#define OFS_PARM5     (OFS_PARM4 +3)
#define OFS_PARM6     (OFS_PARM5 +3)
#define OFS_PARM7     (OFS_PARM6 +3)

typedef struct {
    uint16_t opcode;

    /* operand 1 */
    union {
        int16_t  s1; /* signed   */
        uint16_t u1; /* unsigned */
    } o1;
    /* operand 2 */
    union {
        int16_t  s1; /* signed   */
        uint16_t u1; /* unsigned */
    } o2;
    /* operand 3 */
    union {
        int16_t  s1; /* signed   */
        uint16_t u1; /* unsigned */
    } o3;

    /*
     * This is the same as the structure in darkplaces
     * {
     *     unsigned short op;
     *     short          a,b,c;
     * }
     * But this one is more sane to work with, and the
     * type sizes are guranteed.
     */
} prog_section_statement;

typedef struct {
    /* The types:
     * 0 = ev_void
     * 1 = ev_string
     * 2 = ev_float
     * 3 = ev_vector
     * 4 = ev_entity
     * 5 = ev_field
     * 6 = ev_function
     * 7 = ev_pointer -- engine only
     * 8 = ev_bad     -- engine only
     */
    uint16_t type;
    uint16_t offset;
    uint32_t name;
} prog_section_both;
typedef prog_section_both prog_section_def;
typedef prog_section_both prog_section_field;

/* this is ORed to the type */
#define DEF_SAVEGLOBAL (1<<15)
#define DEF_TYPEMASK   ((1<<15)-1)

typedef struct {
    int32_t   entry;      /* in statement table for instructions  */
    uint32_t  firstlocal; /* First local in local table           */
    uint32_t  locals;     /* Total ints of params + locals        */
    uint32_t  profile;    /* Always zero (engine uses this)       */
    uint32_t  name;       /* name of function in string table     */
    uint32_t  file;       /* file of the source file              */
    uint32_t  nargs;      /* number of arguments                  */
    uint8_t   argsize[8]; /* size of arguments (keep 8 always?)   */
} prog_section_function;

/*
 * Instructions
 * These are the external instructions supported by the interperter
 * this is what things compile to (from the C code).
 */
enum {
    INSTR_DONE,
    INSTR_MUL_F,
    INSTR_MUL_V,
    INSTR_MUL_FV, /* NOTE: the float operands must NOT be at the same locations: A != C */
    INSTR_MUL_VF, /* and here: B != C */
    INSTR_DIV_F,
    INSTR_ADD_F,
    INSTR_ADD_V,
    INSTR_SUB_F,
    INSTR_SUB_V,
    INSTR_EQ_F,
    INSTR_EQ_V,
    INSTR_EQ_S,
    INSTR_EQ_E,
    INSTR_EQ_FNC,
    INSTR_NE_F,
    INSTR_NE_V,
    INSTR_NE_S,
    INSTR_NE_E,
    INSTR_NE_FNC,
    INSTR_LE,
    INSTR_GE,
    INSTR_LT,
    INSTR_GT,
    INSTR_LOAD_F,
    INSTR_LOAD_V,
    INSTR_LOAD_S,
    INSTR_LOAD_ENT,
    INSTR_LOAD_FLD,
    INSTR_LOAD_FNC,
    INSTR_ADDRESS,
    INSTR_STORE_F,
    INSTR_STORE_V,
    INSTR_STORE_S,
    INSTR_STORE_ENT,
    INSTR_STORE_FLD,
    INSTR_STORE_FNC,
    INSTR_STOREP_F,
    INSTR_STOREP_V,
    INSTR_STOREP_S,
    INSTR_STOREP_ENT,
    INSTR_STOREP_FLD,
    INSTR_STOREP_FNC,
    INSTR_RETURN,
    INSTR_NOT_F,
    INSTR_NOT_V,
    INSTR_NOT_S,
    INSTR_NOT_ENT,
    INSTR_NOT_FNC,
    INSTR_IF,
    INSTR_IFNOT,
    INSTR_CALL0,
    INSTR_CALL1,
    INSTR_CALL2,
    INSTR_CALL3,
    INSTR_CALL4,
    INSTR_CALL5,
    INSTR_CALL6,
    INSTR_CALL7,
    INSTR_CALL8,
    INSTR_STATE,
    INSTR_GOTO,
    INSTR_AND,
    INSTR_OR,
    INSTR_BITAND,
    INSTR_BITOR,

    /*
     * Virtual instructions used by the assembler
     * keep at the end but before virtual instructions
     * for the IR below.
     */
    AINSTR_END,

    /*
     * Virtual instructions used by the IR
     * Keep at the end!
     */
    VINSTR_PHI,
    VINSTR_JUMP,
    VINSTR_COND
};

extern prog_section_statement *code_statements;
extern prog_section_def       *code_defs;
extern prog_section_field     *code_fields;
extern prog_section_function  *code_functions;
extern int                    *code_globals;
extern char                   *code_chars;
extern uint16_t code_crc;

typedef float   qcfloat;
typedef int32_t qcint;

/*
 * code_write -- writes out the compiled file
 * code_init  -- prepares the code file
 */
bool     code_write       (const char *filename);
void     code_init        ();
uint32_t code_genstring   (const char *string);
uint32_t code_cachedstring(const char *string);
qcint    code_alloc_field (size_t qcsize);

/*===================================================================*/
/*============================ con.c ================================*/
/*===================================================================*/
enum {
    CON_BLACK   = 30,
    CON_RED,
    CON_GREEN,
    CON_BROWN,
    CON_BLUE,
    CON_MAGENTA,
    CON_CYAN ,
    CON_WHITE
};

/* message level */
enum {
    LVL_MSG,
    LVL_WARNING,
    LVL_ERROR
};


void con_vprintmsg (int level, const char *name, size_t line, const char *msgtype, const char *msg, va_list ap);
void con_printmsg  (int level, const char *name, size_t line, const char *msgtype, const char *msg, ...);
void con_cvprintmsg(void *ctx, int lvl, const char *msgtype, const char *msg, va_list ap);
void con_cprintmsg (void *ctx, int lvl, const char *msgtype, const char *msg, ...);

void con_close();
void con_color(int state);
void con_init ();
void con_reset();
int  con_change(const char *out, const char *err);
int  con_verr  (const char *fmt, va_list va);
int  con_vout  (const char *fmt, va_list va);
int  con_err   (const char *fmt, ...);
int  con_out   (const char *fmt, ...);

/*===================================================================*/
/*========================= assembler.c =============================*/
/*===================================================================*/
static const struct {
    const char  *m; /* menomic     */
    const size_t o; /* operands    */
    const size_t l; /* menomic len */
} asm_instr[] = {
    { "DONE"      , 1, 4 },
    { "MUL_F"     , 3, 5 },
    { "MUL_V"     , 3, 5 },
    { "MUL_FV"    , 3, 6 },
    { "MUL_VF"    , 3, 6 },
    { "DIV"       , 0, 3 },
    { "ADD_F"     , 3, 5 },
    { "ADD_V"     , 3, 5 },
    { "SUB_F"     , 3, 5 },
    { "SUB_V"     , 3, 5 },
    { "EQ_F"      , 0, 4 },
    { "EQ_V"      , 0, 4 },
    { "EQ_S"      , 0, 4 },
    { "EQ_E"      , 0, 4 },
    { "EQ_FNC"    , 0, 6 },
    { "NE_F"      , 0, 4 },
    { "NE_V"      , 0, 4 },
    { "NE_S"      , 0, 4 },
    { "NE_E"      , 0, 4 },
    { "NE_FNC"    , 0, 6 },
    { "LE"        , 0, 2 },
    { "GE"        , 0, 2 },
    { "LT"        , 0, 2 },
    { "GT"        , 0, 2 },
    { "FIELD_F"   , 0, 7 },
    { "FIELD_V"   , 0, 7 },
    { "FIELD_S"   , 0, 7 },
    { "FIELD_ENT" , 0, 9 },
    { "FIELD_FLD" , 0, 9 },
    { "FIELD_FNC" , 0, 9 },
    { "ADDRESS"   , 0, 7 },
    { "STORE_F"   , 0, 7 },
    { "STORE_V"   , 0, 7 },
    { "STORE_S"   , 0, 7 },
    { "STORE_ENT" , 0, 9 },
    { "STORE_FLD" , 0, 9 },
    { "STORE_FNC" , 0, 9 },
    { "STOREP_F"  , 0, 8 },
    { "STOREP_V"  , 0, 8 },
    { "STOREP_S"  , 0, 8 },
    { "STOREP_ENT", 0, 10},
    { "STOREP_FLD", 0, 10},
    { "STOREP_FNC", 0, 10},
    { "RETURN"    , 0, 6 },
    { "NOT_F"     , 0, 5 },
    { "NOT_V"     , 0, 5 },
    { "NOT_S"     , 0, 5 },
    { "NOT_ENT"   , 0, 7 },
    { "NOT_FNC"   , 0, 7 },
    { "IF"        , 0, 2 },
    { "IFNOT"     , 0, 5 },
    { "CALL0"     , 1, 5 },
    { "CALL1"     , 2, 5 },
    { "CALL2"     , 3, 5 },
    { "CALL3"     , 4, 5 },
    { "CALL4"     , 5, 5 },
    { "CALL5"     , 6, 5 },
    { "CALL6"     , 7, 5 },
    { "CALL7"     , 8, 5 },
    { "CALL8"     , 9, 5 },
    { "STATE"     , 0, 5 },
    { "GOTO"      , 0, 4 },
    { "AND"       , 0, 3 },
    { "OR"        , 0, 2 },
    { "BITAND"    , 0, 6 },
    { "BITOR"     , 0, 5 },

    { "END"       , 0, 3 } /* virtual assembler instruction */
};
/*===================================================================*/
/*============================= ir.c ================================*/
/*===================================================================*/

enum store_types {
    store_global,
    store_local,  /* local, assignable for now, should get promoted later */
    store_param,  /* parameters, they are locals with a fixed position */
    store_value,  /* unassignable */
    store_return  /* unassignable, at OFS_RETURN */
};

typedef struct {
    qcfloat x, y, z;
} vector;

vector  vec3_add  (vector, vector);
vector  vec3_sub  (vector, vector);
qcfloat vec3_mulvv(vector, vector);
vector  vec3_mulvf(vector, float);

/*
 * A shallow copy of a lex_file to remember where which ast node
 * came from.
 */
typedef struct {
    const char *file;
    size_t      line;
} lex_ctx;

/*===================================================================*/
/*============================= exec.c ==============================*/
/*===================================================================*/

/* darkplaces has (or will have) a 64 bit prog loader
 * where the 32 bit qc program is autoconverted on load.
 * Since we may want to support that as well, let's redefine
 * float and int here.
 */
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
typedef char qcint_size_is_correct   [sizeof(qcint)   == 4 ?1:-1];

enum {
    VMERR_OK,
    VMERR_TEMPSTRING_ALLOC,

    VMERR_END
};

#define VM_JUMPS_DEFAULT 1000000

/* execute-flags */
#define VMXF_DEFAULT 0x0000     /* default flags - nothing */
#define VMXF_TRACE   0x0001     /* trace: print statements before executing */
#define VMXF_PROFILE 0x0002     /* profile: increment the profile counters */

struct qc_program_s;

typedef int (*prog_builtin)(struct qc_program_s *prog);

typedef struct {
    qcint                  stmt;
    size_t                 localsp;
    prog_section_function *function;
} qc_exec_stack;

typedef struct qc_program_s {
    char           *filename;

    prog_section_statement *code;
    prog_section_def       *defs;
    prog_section_def       *fields;
    prog_section_function  *functions;
    char                   *strings;
    qcint                  *globals;
    qcint                  *entitydata;
    bool                   *entitypool;

    const char*            *function_stack;

    uint16_t crc16;

    size_t tempstring_start;
    size_t tempstring_at;

    qcint  vmerror;

    size_t *profile;

    prog_builtin *builtins;
    size_t        builtins_count;

    /* size_t ip; */
    qcint  entities;
    size_t entityfields;
    bool   allowworldwrites;

    qcint         *localstack;
    qc_exec_stack *stack;
    size_t statement;

    size_t xflags;

    int    argc; /* current arg count for debugging */
} qc_program;

qc_program* prog_load(const char *filename);
void        prog_delete(qc_program *prog);

bool prog_exec(qc_program *prog, prog_section_function *func, size_t flags, long maxjumps);

char*             prog_getstring (qc_program *prog, qcint str);
prog_section_def* prog_entfield  (qc_program *prog, qcint off);
prog_section_def* prog_getdef    (qc_program *prog, qcint off);
qcany*            prog_getedict  (qc_program *prog, qcint e);
qcint             prog_tempstring(qc_program *prog, const char *_str);


/*===================================================================*/
/*===================== parser.c commandline ========================*/
/*===================================================================*/

bool parser_init          ();
bool parser_compile_file  (const char *filename);
bool parser_compile_string(const char *name, const char *str);
bool parser_finish        (const char *output);
void parser_cleanup       ();
/* There's really no need to strlen() preprocessed files */
bool parser_compile_string_len(const char *name, const char *str, size_t len);

/*===================================================================*/
/*====================== ftepp.c commandline ========================*/
/*===================================================================*/
bool ftepp_init             ();
bool ftepp_preprocess_file  (const char *filename);
bool ftepp_preprocess_string(const char *name, const char *str);
void ftepp_finish           ();
const char *ftepp_get       ();
void ftepp_flush            ();

/*===================================================================*/
/*======================= main.c commandline ========================*/
/*===================================================================*/

#if 0
/* Helpers to allow for a whole lot of flags. Otherwise we'd limit
 * to 32 or 64 -f options...
 */
typedef struct {
    size_t  idx; /* index into an array of 32 bit words */
    uint8_t bit; /* index _into_ the 32 bit word, thus just uint8 */
} longbit;
#define LONGBIT(bit) { ((bit)/32), ((bit)%32) }
#else
typedef uint32_t longbit;
#define LONGBIT(bit) (bit)
#endif

/* Used to store the list of flags with names */
typedef struct {
    const char *name;
    longbit    bit;
} opts_flag_def;

/*===================================================================*/
/* list of -f flags, like -fdarkplaces-string-table-bug */
enum {
# define GMQCC_TYPE_FLAGS
# define GMQCC_DEFINE_FLAG(X) X,
#  include "opts.def"
    COUNT_FLAGS
};
static const opts_flag_def opts_flag_list[] = {
# define GMQCC_TYPE_FLAGS
# define GMQCC_DEFINE_FLAG(X) { #X, LONGBIT(X) },
#  include "opts.def"
    { NULL, LONGBIT(0) }
};

enum {
# define GMQCC_TYPE_WARNS
# define GMQCC_DEFINE_FLAG(X) WARN_##X,
#  include "opts.def"
    COUNT_WARNINGS
};
static const opts_flag_def opts_warn_list[] = {
# define GMQCC_TYPE_WARNS
# define GMQCC_DEFINE_FLAG(X) { #X, LONGBIT(WARN_##X) },
#  include "opts.def"
    { NULL, LONGBIT(0) }
};

/* other options: */
enum {
    COMPILER_QCC,     /* circa  QuakeC */
    COMPILER_FTEQCC,  /* fteqcc QuakeC */
    COMPILER_QCCX,    /* qccx   QuakeC */
    COMPILER_GMQCC    /* this   QuakeC */
};
extern uint32_t    opts_O;      /* -Ox */
extern const char *opts_output; /* -o file */
extern int         opts_standard;
extern bool        opts_debug;
extern bool        opts_memchk;
extern bool        opts_dumpfin;
extern bool        opts_dump;
extern bool        opts_werror;
extern bool        opts_forcecrc;
extern uint16_t    opts_forced_crc;
extern bool        opts_pp_only;
extern size_t      opts_max_array_size;

/*===================================================================*/
#define OPTS_FLAG(i) (!! (opts_flags[(i)/32] & (1<< ((i)%32))))
extern uint32_t opts_flags[1 + (COUNT_FLAGS / 32)];
#define OPTS_WARN(i) (!! (opts_warn[(i)/32] & (1<< ((i)%32))))
extern uint32_t opts_warn[1 + (COUNT_WARNINGS / 32)];

#endif
