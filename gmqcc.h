/*
 * Copyright (C) 2012 
 *     Dale Weiler
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
#include <ctype.h>

#define GMQCC_VERSION_MAJOR 0
#define GMQCC_VERSION_MINOR 1
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
#   define bool _Bool
#   if __STDC_VERSION__ < 199901L && __GNUC__ < 3
        typedef int  _Bool
#   endif
#   endif /* !__cplusplus */

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
#ifdef _LP64 /* long pointer == 64 */
    typedef unsigned long  uintptr_t;
    typedef long           intptr_t;
#else
    typedef unsigned int   uintptr_t;
    typedef int            intptr_t;
#endif 
/* Ensure type sizes are correct: */
typedef char uint8_size_is_correct  [sizeof(uint8_t)  == 1?1:-1];
typedef char uint16_size_if_correct [sizeof(uint16_t) == 2?1:-1];
typedef char uint32_size_is_correct [sizeof(uint32_t) == 4?1:-1];
//typedef char int8_size_is_correct   [sizeof(int8_t)   == 1?1:-1];
typedef char int16_size_if_correct  [sizeof(int16_t)  == 2?1:-1];
typedef char int32_size_is_correct  [sizeof(int32_t)  == 4?1:-1];
/* intptr_t / uintptr_t correct size check */
typedef char uintptr_size_is_correct[sizeof(intptr_t) == sizeof(int*)?1:-1];
typedef char intptr_size_is_correct [sizeof(uintptr_t)== sizeof(int*)?1:-1];

//===================================================================
//============================ lex.c ================================
//===================================================================
struct lex_file {
    FILE *file;        /* file handler */
    char *name;        /* name of file */
    char  peek  [5];  
    char  lastok[8192];
    
    int   last;    /* last token                   */
    int   current; /* current token                */
    int   length;  /* bytes left to parse          */
    int   size;    /* never changes (size of file) */
    int   line;    /* what line are we on?         */
};

/*
 * It's important that this table never exceed 32 keywords, the ascii
 * table starts at 33 (and we don't want conflicts)
 */
enum {
    TOKEN_DO       ,
    TOKEN_ELSE     ,
    TOKEN_IF       ,
    TOKEN_WHILE    ,
    TOKEN_BREAK    ,
    TOKEN_CONTINUE ,
    TOKEN_RETURN   ,
    TOKEN_GOTO     ,
    TOKEN_FOR      ,   // extension
    TOKEN_TYPEDEF  ,   // extension

    // ensure the token types are out of the
    // bounds of anyothers that may conflict.
    TOKEN_FLOAT    = 110,
    TOKEN_VECTOR        ,
    TOKEN_STRING        ,
    TOKEN_ENTITY        ,
    TOKEN_VOID
};

/*
 * Lexer state constants, these are numbers for where exactly in
 * the lexing the lexer is at. Or where it decided to stop if a lexer
 * error occurs.  These numbers must be > where the ascii-table ends
 * and > the last type token which is TOKEN_VOID
 */
enum {
    LEX_COMMENT = 1128, 
    LEX_CHRLIT        ,
    LEX_STRLIT        ,
    LEX_IDENT
};

int              lex_token  (struct lex_file *);
void             lex_reset  (struct lex_file *);
void             lex_close  (struct lex_file *);
struct lex_file *lex_include(struct lex_file *, char *);
struct lex_file *lex_open   (FILE *);

//===================================================================
//========================== error.c ================================
//===================================================================
#define ERROR_LEX      (SHRT_MAX+0)
#define ERROR_PARSE    (SHRT_MAX+1)
#define ERROR_INTERNAL (SHRT_MAX+2)
#define ERROR_COMPILER (SHRT_MAX+3)
#define ERROR_PREPRO   (SHRT_MAX+4)
int error(struct lex_file *, int, const char *, ...);

//===================================================================
//========================== parse.c ================================
//===================================================================
int parse_gen(struct lex_file *);

//===================================================================
//========================== typedef.c ==============================
//===================================================================
typedef struct typedef_node_t {
    char      *name;
} typedef_node;

void          typedef_init();
void          typedef_clear();
typedef_node *typedef_find(const char *);
int           typedef_add (struct lex_file *file, const char *, const char *);


//===================================================================
//=========================== util.c ================================
//===================================================================
void *util_memory_a      (unsigned int, unsigned int, const char *);
void  util_memory_d      (void       *, unsigned int, const char *);
void  util_meminfo       ();

char *util_strdup        (const char *);
char *util_strrq         (char *);
char *util_strrnl        (char *);
void  util_debug         (const char *, const char *, ...);
int   util_getline       (char **, size_t *, FILE *);
void  util_endianswap    (void *,  int, int);

uint32_t util_crc32(const char *, int, register const short); 

#ifdef NOTRACK
#    define mem_a(x) malloc(x)
#    define mem_d(x) free  (x)
#else
#    define mem_a(x) util_memory_a((x), __LINE__, __FILE__)
#    define mem_d(x) util_memory_d((x), __LINE__, __FILE__)
#endif

/* Builds vector type (usefull for inside structures) */
#define VECTOR_TYPE(T,N)                                        \
    T*     N##_data      = NULL;                                \
    long   N##_elements  = 0;                                   \
    long   N##_allocated = 0
/* Builds vector add */
#define VECTOR_CORE(T,N)                                        \
    int    N##_add(T element) {                                 \
        if (N##_elements == N##_allocated) {                    \
            if (N##_allocated == 0) {                           \
                N##_allocated = 12;                             \
            } else {                                            \
                N##_allocated *= 2;                             \
            }                                                   \
            void *temp = mem_a(N##_allocated * sizeof(T));      \
            if  (!temp) {                                       \
                mem_d(temp);                                    \
                return -1;                                      \
            }                                                   \
            memcpy(temp, N##_data, (N##_elements * sizeof(T))); \
            mem_d(N##_data);                                    \
            N##_data = (T*)temp;                                \
        }                                                       \
        N##_data[N##_elements] = element;                       \
        return   N##_elements++;                                \
    }                                                           \
    int N##_put(T* elements, size_t len) {                      \
        len     --;                                             \
        elements--;                                             \
        while (N##_add(*++elements) != -1 && len--);            \
        return N##_elements;                                    \
    }
/* Builds a full vector inspot */
#define VECTOR_MAKE(T,N) \
    VECTOR_TYPE(T,N);    \
    VECTOR_CORE(T,N)
/* Builds a vector add function pointer for inside structures */
#define VECTOR_IMPL(T,N) int (*N##_add)(T)

//===================================================================
//=========================== code.c ================================
//===================================================================
enum {
    TYPE_VOID     ,
    TYPE_STRING   ,
    TYPE_FLOAT    ,
    TYPE_VECTOR   ,
    TYPE_ENTITY   ,
    TYPE_FIELD    ,
    TYPE_FUNCTION ,
    TYPE_POINTER
};

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
    };
    /* operand 2 */
    union {
        int16_t  s2; /* signed   */
        uint16_t u2; /* unsigned */
    };
    /* operand 3 */
    union {
        int16_t  s3; /* signed   */
        uint16_t u3; /* unsigned */
    };
    
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
    INSTR_MUL_FV,
    INSTR_MUL_VF,
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

    /* Virtual instructions used by the IR
     * Keep at the end!
     */
    VINSTR_PHI,
    VINSTR_JUMP,
    VINSTR_COND,
};

/*
 * The symbols below are created by the following
 * expanded macros:
 * 
 * VECTOR_MAKE(prog_section_statement, code_statements);
 * VECTOR_MAKE(prog_section_def,       code_defs      );
 * VECTOR_MAKE(prog_section_field,     code_fields    );
 * VECTOR_MAKE(prog_section_function,  code_functions );
 * VECTOR_MAKE(int,                    code_globals   );
 * VECTOR_MAKE(char,                   code_chars     );
 */
int         code_statements_add(prog_section_statement);
int         code_defs_add      (prog_section_def);
int         code_fields_add    (prog_section_field);
int         code_functions_add (prog_section_function);
int         code_globals_add   (int);
int         code_chars_add     (char);
int         code_statements_put(prog_section_statement*, size_t);
int         code_defs_put      (prog_section_def*,       size_t);
int         code_fields_put    (prog_section_field*,     size_t);
int         code_functions_put (prog_section_function*,  size_t);
int         code_globals_put   (int*,                    size_t);
int         code_chars_put     (char*,                   size_t);
extern long code_statements_elements;
extern long code_chars_elements;
extern long code_globals_elements;
extern long code_functions_elements;
extern long code_fields_elements;
extern long code_defs_elements;

/*
 * code_write -- writes out the compiled file
 * code_init  -- prepares the code file
 */
void code_write ();
void code_init  ();

//===================================================================
//========================= assembler.c =============================
//===================================================================
static const struct {
    const char  *m; /* menomic     */
    const size_t o; /* operands    */ 
    const size_t l; /* menomic len */
} const asm_instr[] = {
    [INSTR_DONE]       = { "DONE"      , 1, 4 },
    [INSTR_MUL_F]      = { "MUL_F"     , 3, 5 },
    [INSTR_MUL_V]      = { "MUL_V"     , 3, 5 },
    [INSTR_MUL_FV]     = { "MUL_FV"    , 3, 6 },
    [INSTR_MUL_VF]     = { "MUL_VF"    , 3, 6 },
    [INSTR_DIV_F]      = { "DIV"       , 0, 3 },
    [INSTR_ADD_F]      = { "ADD_F"     , 3, 5 },
    [INSTR_ADD_V]      = { "ADD_V"     , 3, 5 },
    [INSTR_SUB_F]      = { "SUB_F"     , 3, 5 },
    [INSTR_SUB_V]      = { "DUB_V"     , 3, 5 },
    [INSTR_EQ_F]       = { "EQ_F"      , 0, 4 },
    [INSTR_EQ_V]       = { "EQ_V"      , 0, 4 },
    [INSTR_EQ_S]       = { "EQ_S"      , 0, 4 },
    [INSTR_EQ_E]       = { "EQ_E"      , 0, 4 },
    [INSTR_EQ_FNC]     = { "ES_FNC"    , 0, 6 },
    [INSTR_NE_F]       = { "NE_F"      , 0, 4 },
    [INSTR_NE_V]       = { "NE_V"      , 0, 4 },
    [INSTR_NE_S]       = { "NE_S"      , 0, 4 },
    [INSTR_NE_E]       = { "NE_E"      , 0, 4 },
    [INSTR_NE_FNC]     = { "NE_FNC"    , 0, 6 },
    [INSTR_LE]         = { "LE"        , 0, 2 },
    [INSTR_GE]         = { "GE"        , 0, 2 },
    [INSTR_LT]         = { "LT"        , 0, 2 },
    [INSTR_GT]         = { "GT"        , 0, 2 },
    [INSTR_LOAD_F]     = { "FIELD_F"   , 0, 7 },
    [INSTR_LOAD_V]     = { "FIELD_V"   , 0, 7 },
    [INSTR_LOAD_S]     = { "FIELD_S"   , 0, 7 },
    [INSTR_LOAD_ENT]   = { "FIELD_ENT" , 0, 9 },
    [INSTR_LOAD_FLD]   = { "FIELD_FLD" , 0, 9 },
    [INSTR_LOAD_FNC]   = { "FIELD_FNC" , 0, 9 },
    [INSTR_ADDRESS]    = { "ADDRESS"   , 0, 7 },
    [INSTR_STORE_F]    = { "STORE_F"   , 0, 7 },
    [INSTR_STORE_V]    = { "STORE_V"   , 0, 7 },
    [INSTR_STORE_S]    = { "STORE_S"   , 0, 7 },
    [INSTR_STORE_ENT]  = { "STORE_ENT" , 0, 9 },
    [INSTR_STORE_FLD]  = { "STORE_FLD" , 0, 9 },
    [INSTR_STORE_FNC]  = { "STORE_FNC" , 0, 9 },
    [INSTR_STOREP_F]   = { "STOREP_F"  , 0, 8 },
    [INSTR_STOREP_V]   = { "STOREP_V"  , 0, 8 },
    [INSTR_STOREP_S]   = { "STOREP_S"  , 0, 8 },
    [INSTR_STOREP_ENT] = { "STOREP_ENT", 0, 10},
    [INSTR_STOREP_FLD] = { "STOREP_FLD", 0, 10},
    [INSTR_STOREP_FNC] = { "STOREP_FNC", 0, 10},
    [INSTR_RETURN]     = { "RETURN"    , 0, 6 },
    [INSTR_NOT_F]      = { "NOT_F"     , 0, 5 },
    [INSTR_NOT_V]      = { "NOT_V"     , 0, 5 },
    [INSTR_NOT_S]      = { "NOT_S"     , 0, 5 },
    [INSTR_NOT_ENT]    = { "NOT_ENT"   , 0, 7 },
    [INSTR_NOT_FNC]    = { "NOT_FNC"   , 0, 7 },
    [INSTR_IF]         = { "IF"        , 0, 2 },
    [INSTR_IFNOT]      = { "IFNOT"     , 0, 5 },
    [INSTR_CALL0]      = { "CALL0"     , 0, 5 },
    [INSTR_CALL1]      = { "CALL1"     , 0, 5 },
    [INSTR_CALL2]      = { "CALL2"     , 0, 5 },
    [INSTR_CALL3]      = { "CALL3"     , 0, 5 },
    [INSTR_CALL4]      = { "CALL4"     , 0, 5 },
    [INSTR_CALL5]      = { "CALL5"     , 0, 5 },
    [INSTR_CALL6]      = { "CALL6"     , 0, 5 },
    [INSTR_CALL7]      = { "CALL7"     , 0, 5 },
    [INSTR_CALL8]      = { "CALL8"     , 0, 5 },
    [INSTR_STATE]      = { "STATE"     , 0, 5 },
    [INSTR_GOTO]       = { "GOTO"      , 0, 4 },
    [INSTR_AND]        = { "AND"       , 0, 3 },
    [INSTR_OR]         = { "OR"        , 0, 2 },
    [INSTR_BITAND]     = { "BITAND"    , 0, 6 },
    [INSTR_BITOR]      = { "BITOR"     , 0, 5 }
};

void asm_init (const char *, FILE **);
void asm_close(FILE *);
void asm_parse(FILE *);
//======================================================================
//============================= main.c =================================
//======================================================================
enum {
    COMPILER_QCC,     /* circa  QuakeC */
    COMPILER_FTEQCC,  /* fteqcc QuakeC */
    COMPILER_QCCX,    /* qccx   QuakeC */
    COMPILER_GMQCC    /* this   QuakeC */
};
extern bool opts_debug;
extern bool opts_memchk;
extern bool opts_darkplaces_stringtablebug;
extern bool opts_omit_nullcode;
extern int  opts_compiler;
#endif
