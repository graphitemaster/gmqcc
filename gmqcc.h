#ifndef GMQCC_HDR
#define GMQCC_HDR
#include <vector>
#include <string>
#include <utility>
#include <memory>
#include <map>
using std::move;
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define GMQCC_VERSION_MAJOR 0
#define GMQCC_VERSION_MINOR 3
#define GMQCC_VERSION_PATCH 6
#define GMQCC_VERSION ((GMQCC_VERSION_MAJOR<<16)|(GMQCC_VERSION_MINOR<<8)|GMQCC_VERSION_PATCH)

#ifdef GMQCC_VERSION_TYPE_RELEASE
#    ifdef GMQCC_GITINFO
#        define GMQCC_DEV_VERSION_STRING "git build: " GMQCC_GITINFO "\n"
#    elif defined(GMQCC_VERSION_TYPE_DEVEL)
#        define GMQCC_DEV_VERSION_STRING "development build\n"
#    else
#        define GMQCC_DEV_VERSION_STRING
#    endif /*! GMQCC_GITINGO */
#else
#    define GMQCC_DEV_VERSION_STRING
#endif

#define GMQCC_STRINGIFY(x) #x
#define GMQCC_IND_STRING(x) GMQCC_STRINGIFY(x)
#define GMQCC_FULL_VERSION_STRING \
"GMQCC " \
GMQCC_IND_STRING(GMQCC_VERSION_MAJOR) "." \
GMQCC_IND_STRING(GMQCC_VERSION_MINOR) "." \
GMQCC_IND_STRING(GMQCC_VERSION_PATCH) \
" Built " __DATE__ " " __TIME__ \
"\n" GMQCC_DEV_VERSION_STRING

#ifndef __cplusplus
#   define false (unsigned char)(0)
#   define true  (unsigned char)(1)
    typedef unsigned char bool;
#endif

#if defined(__GNUC__) || defined(__CLANG__)
#   include <stdint.h>
#   if (__GNUC__ >= 2) || defined(__CLANG__)
#       define GMQCC_NORETURN    __attribute__((noreturn))
#       define GMQCC_FORCEINLINE __attribute__((always_inline))
#       define GMQCC_INLINE      __inline
#   endif
#   define GMQCC_LIKELY(X)   __builtin_expect((X), 1)
#   define GMQCC_UNLIKELY(X) __builtin_expect((X), 0)
#   define GMQCC_WARN        __attribute__((warn_unused_result))
#   define GMQCC_USED        __attribute__((used))
#   define GMQCC_RESTRICT    __restrict__
#else
#   ifdef _MSC_VER
        /* conversion from 'int' to 'float', possible loss of data */
#       pragma warning(disable : 4244)

        typedef unsigned __int8  uint8_t;
        typedef unsigned __int16 uint16_t;
        typedef unsigned __int32 uint32_t;
        typedef unsigned __int64 uint64_t;
        typedef __int16          int16_t;
        typedef __int32          int32_t;
        typedef __int64          int64_t;
#       define GMQCC_NORETURN    __declspec(noreturn)
#       define GMQCC_FORCEINLINE __forceinline
#       define GMQCC_INLINE      __inline
#       define GMQCC_RESTRICT    __restrict
#   else
#       define GMQCC_NORETURN
#       define GMQCC_FORCEINLINE
#       define GMQCC_INLINE
#       define GMQCC_RESTRICT
#   endif
#   define GMQCC_LIKELY(X)   (X)
#   define GMQCC_UNLIKELY(X) (X)
#   define GMQCC_WARN
#   define GMQCC_USED
#endif

#define GMQCC_BYTE_ORDER_LITTLE 1234
#define GMQCC_BYTE_ORDER_BIG    4321

#if defined (__GNUC__) || defined (__GNU_LIBRARY__)
#   if defined (__FreeBSD__) || defined (__OpenBSD__)
#       include <sys/endian.h>
#   elif defined (BSD) && (BSD >= 199103) || defined (__DJGPP__) || defined (__CYGWIN32__)
#       include <machine/endian.h>
#   elif defined (__APPLE__)
#       if defined (__BIG_ENDIAN__) && !defined(BIG_ENDIAN)
#           define BIG_ENDIAN
#       elif defined (__LITTLE_ENDIAN__) && !defined (LITTLE_ENDIAN)
#           define LITTLE_ENDIAN
#       endif /*! defined (__BIG_ENDIAN__) && !defined(BIG_ENDIAN) */
#   elif !defined (__MINGW32__)
#       include <endian.h>
#       if !defined (__BEOS__)
#           include <byteswap.h>
#       endif /*! !definde (__BEOS__) */
#   endif /*! defined (__FreeBSD__) || defined (__OpenBSD__) */
#endif /*! defined (__GNUC__) || defined (__GNU_LIBRARY__) */
#if !defined(PLATFORM_BYTE_ORDER)
#   if defined (LITTLE_ENDIAN) || defined (BIG_ENDIAN)
#       if defined (LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_LITTLE
#       elif !defined (LITTLE_ENDIAN) && defined (BIG_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_BIG
#       elif defined (BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_LITTLE
#       elif defined (BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_BIG
#       endif /*! defined (LITTLE_ENDIAN) && !defined(BIG_ENDIAN) */
#   elif defined (_LITTLE_ENDIAN) || defined (_BIG_ENDIAN)
#       if defined (_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_LITTLE
#       elif !defined (_LITTLE_ENDIAN) && defined (_BIG_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_BIG
#       elif defined (_BYTE_ORDER) && (_BYTE_ORDER == _LITTLE_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_LITTLE
#       elif defined (_BYTE_ORDER) && (_BYTE_ORDER == _BIG_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_BIG
#       endif /*! defined (_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN) */
#   elif defined (__LITTLE_ENDIAN__) || defined (__BIG_ENDIAN__)
#       if defined (__LITTLE_ENDIAN__) && !defined (__BIG_ENDIAN__)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_LITTLE
#       elif !defined (__LITTLE_ENDIAN__) && defined (__BIG_ENDIAN__)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_BIG
#       elif defined (__BYTE_ORDER__) && (__BYTE_ORDER__ == __LITTLE_ENDIAN__)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_LITTLE
#       elif defined (__BYTE_ORDER__) && (__BYTE_ORDER__ == __BIG_ENDIAN__)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_BIG
#       endif /*! defined (__LITTLE_ENDIAN__) && !defined (__BIG_ENDIAN__) */
#   endif /*! defined(LITTLE_ENDIAN) || defined (BIG_ENDIAN) */
#endif /*! !defined(PLATFORM_BYTE_ORDER) */
#if !defined (PLATFORM_BYTE_ORDER)
#   if   defined (__alpha__) || defined (__alpha)    || defined (i386)       || \
         defined (__i386__)  || defined (_M_I86)     || defined (_M_IX86)    || \
         defined (__OS2__)   || defined (sun386)     || defined (__TURBOC__) || \
         defined (vax)       || defined (vms)        || defined (VMS)        || \
         defined (__VMS)     || defined (__x86_64__) || defined (_M_IA64)    || \
         defined (_M_X64)    || defined (__i386)     || defined (__x86_64)
#       define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_LITTLE
#   elif defined (AMIGA)     || defined (applec)     || defined (__AS400__)  || \
         defined (_CRAY)     || defined (__hppa)     || defined (__hp9000)   || \
         defined (ibm370)    || defined (mc68000)    || defined (m68k)       || \
         defined (__MRC__)   || defined (__MVS__)    || defined (__MWERKS__) || \
         defined (sparc)     || defined (__sparc)    || defined (SYMANTEC_C) || \
         defined (__TANDEM)  || defined (THINK_C)    || defined (__VMCMS__)  || \
         defined (__PPC__)   || defined (__PPC)      || defined (PPC)
#       define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_BIG
#   else
#       define PLATFORM_BYTE_ORDER -1
#   endif
#endif

#define GMQCC_ARRAY_COUNT(X) (sizeof(X) / sizeof((X)[0]))

/* stat.c */
char *stat_mem_strdup(const char *, bool);

#define mem_a(SIZE)              malloc(SIZE)
#define mem_d(PTRN)              free((void*)PTRN)
#define mem_r(PTRN, SIZE)        realloc((void*)PTRN, SIZE)

#define util_strdup(SRC)         stat_mem_strdup((char*)(SRC), false)
#define util_strdupe(SRC)        stat_mem_strdup((char*)(SRC), true)

#define util_isalpha(a) ((((unsigned)(a)|32)-'a') < 26)
#define util_isdigit(a) (((unsigned)(a)-'0') < 10)
#define util_islower(a) (((unsigned)(a)-'a') < 26)
#define util_isupper(a) (((unsigned)(a)-'A') < 26)
#define util_isprint(a) (((unsigned)(a)-0x20) < 0x5F)
#define util_isspace(a) (((a) >= 9 && (a) <= 13) || (a) == ' ')

bool  util_strupper(const char *);
bool  util_strdigit(const char *);

void  util_endianswap(void *, size_t, unsigned int);

size_t util_strtocmd         (const char *, char *, size_t);
size_t util_strtononcmd      (const char *, char *, size_t);
size_t util_optimizationtostr(const char *, char *, size_t);

uint16_t util_crc16(uint16_t crc, const char *data, size_t len);

void     util_seed(uint32_t);
uint32_t util_rand(void);

int      util_asprintf (char **ret, const char *fmt, ...);
int      util_sscanf   (const char *str, const char *format, ...);
char    *util_strncpy  (char *dest, const char *src, size_t n);
char    *util_strncat  (char *dest, const char *src, size_t n);
char    *util_strcat   (char *dest, const char *src);
const char *util_strerror(int err);

const struct tm *util_localtime(const time_t *timer);
const char      *util_ctime    (const time_t *timer);

bool             util_isatty(FILE *);
size_t           hash(const char *key);

/*
 * A flexible vector implementation: all vector pointers contain some
 * data about themselfs exactly - sizeof(vector_t) behind the pointer
 * this data is represented in the structure below.  Doing this allows
 * us to use the array [] to access individual elements from the vector
 * opposed to using set/get methods.
 */
struct vector_t {
    size_t  allocated;
    size_t  used;

    /* can be extended now! whoot */
};

/* hidden interface */
void _util_vec_grow(void **a, size_t i, size_t s);
void _util_vec_delete(void *vec);

#define GMQCC_VEC_WILLGROW(X, Y) ( \
    ((!(X) || vec_meta(X)->used + Y >= vec_meta(X)->allocated)) ? \
        (void)_util_vec_grow(((void**)&(X)), (Y), sizeof(*(X))) : \
        (void)0                                                   \
)

/* exposed interface */
#define vec_meta(A)       ((vector_t*)(((char *)(A)) - sizeof(vector_t)))
#define vec_free(A)       ((void)((A) ? (_util_vec_delete((void *)(A)), (A) = nullptr) : 0))
#define vec_push(A,V)     (GMQCC_VEC_WILLGROW((A),1), (A)[vec_meta(A)->used++] = (V))
#define vec_size(A)       ((A) ? vec_meta(A)->used : 0)
#define vec_add(A,N)      (GMQCC_VEC_WILLGROW((A),(N)), vec_meta(A)->used += (N), &(A)[vec_meta(A)->used-(N)])
#define vec_last(A)       ((A)[vec_meta(A)->used - 1])
#define vec_pop(A)        ((void)(vec_meta(A)->used -= 1))
#define vec_shrinkto(A,N) ((void)(vec_meta(A)->used  = (N)))
#define vec_shrinkby(A,N) ((void)(vec_meta(A)->used -= (N)))
#define vec_append(A,N,S) ((void)(memcpy(vec_add((A), (N)), (S), (N) * sizeof(*(S)))))
#define vec_remove(A,I,N) ((void)(memmove((A)+(I),(A)+((I)+(N)),sizeof(*(A))*(vec_meta(A)->used-(I)-(N))),vec_meta(A)->used-=(N)))

typedef struct hash_table_s {
    size_t                size;
    struct hash_node_t **table;
} hash_table_t, *ht;

hash_table_t *util_htnew (size_t size);
void util_htrem(hash_table_t *ht, void (*callback)(void *data));
void util_htset(hash_table_t *ht, const char *key, void *value);
void util_htdel(hash_table_t *ht);
size_t util_hthash(hash_table_t *ht, const char *key);
void util_htseth(hash_table_t *ht, const char *key, size_t hash, void *value);
void util_htrmh(hash_table_t *ht, const char *key, size_t bin, void (*cb)(void*));
void util_htrm(hash_table_t *ht, const char *key, void (*cb)(void*));
void *util_htget(hash_table_t *ht, const char *key);
void *util_htgeth(hash_table_t *ht, const char *key, size_t hash);
int util_snprintf(char *str, size_t, const char *fmt, ...);
int util_getline(char  **, size_t *, FILE *);

/* code.c */

/* Note: if you change the order, fix type_sizeof in ir.c */
enum qc_type {
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

    TYPE_NIL      , /* it's its own type / untyped */
    TYPE_NOEXPR   , /* simply invalid in expressions */

    TYPE_COUNT
};

/* const/var qualifiers */
#define CV_NONE   0
#define CV_CONST  1
#define CV_VAR   -1
#define CV_WRONG  0x8000 /* magic number to help parsing */

extern const char    *type_name        [TYPE_COUNT];
extern const uint16_t type_store_instr [TYPE_COUNT];
extern const uint16_t field_store_instr[TYPE_COUNT];

/*
 * could use type_store_instr + INSTR_STOREP_F - INSTR_STORE_F
 * but this breaks when TYPE_INTEGER is added, since with the enhanced
 * instruction set, the old ones are left untouched, thus the _I instructions
 * are at a seperate place.
 */
extern const uint16_t type_storep_instr[TYPE_COUNT];
extern const uint16_t type_eq_instr    [TYPE_COUNT];
extern const uint16_t type_ne_instr    [TYPE_COUNT];
extern const uint16_t type_not_instr   [TYPE_COUNT];

struct prog_section_t {
    uint32_t offset;      /* Offset in file of where data begins  */
    uint32_t length;      /* Length of section (how many of)      */
};

struct prog_header_t {
    uint32_t       version;      /* Program version (6)     */
    uint16_t       crc16;
    uint16_t       skip;

    prog_section_t statements;   /* prog_section_statement  */
    prog_section_t defs;         /* prog_section_def        */
    prog_section_t fields;       /* prog_section_field      */
    prog_section_t functions;    /* prog_section_function   */
    prog_section_t strings;
    prog_section_t globals;
    uint32_t       entfield;     /* Number of entity fields */
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

union operand_t {
    int16_t s1;
    uint16_t u1;
};

struct prog_section_statement_t {
    uint16_t opcode;
    operand_t o1;
    operand_t o2;
    operand_t o3;
};

struct prog_section_both_t {
    /*
     * The types:
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
};

typedef prog_section_both_t prog_section_def_t;
typedef prog_section_both_t prog_section_field_t;

/* this is ORed to the type */
#define DEF_SAVEGLOBAL (1<<15)
#define DEF_TYPEMASK   ((1<<15)-1)

struct prog_section_function_t {
    int32_t   entry;      /* in statement table for instructions  */
    uint32_t  firstlocal; /* First local in local table           */
    uint32_t  locals;     /* Total ints of params + locals        */
    uint32_t  profile;    /* Always zero (engine uses this)       */
    uint32_t  name;       /* name of function in string table     */
    uint32_t  file;       /* file of the source file              */
    int32_t   nargs;      /* number of arguments                  */
    uint8_t   argsize[8]; /* size of arguments (keep 8 always?)   */
};

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
     * Virtual instructions used by the IR
     * Keep at the end!
     */
    VINSTR_END,
    VINSTR_PHI,
    VINSTR_JUMP,
    VINSTR_COND,

    /* A never returning CALL.
     * Creating this causes IR blocks to be marked as 'final'.
     * No-Return-Call
     */
    VINSTR_NRCALL,

    /* Emulated instructions. */
    VINSTR_BITAND_V, /* BITAND_V must be the first emulated bitop */
    VINSTR_BITAND_VF,
    VINSTR_BITOR_V,
    VINSTR_BITOR_VF,
    VINSTR_BITXOR,
    VINSTR_BITXOR_V,
    VINSTR_BITXOR_VF,
    VINSTR_CROSS,
    VINSTR_NEG_F,
    VINSTR_NEG_V
};

/* TODO: elide */
extern const char *util_instr_str[VINSTR_END];

void util_swap_header(prog_header_t &code_header);
void util_swap_statements(std::vector<prog_section_statement_t> &statements);
void util_swap_defs_fields(std::vector<prog_section_both_t> &section);
void util_swap_functions(std::vector<prog_section_function_t> &functions);
void util_swap_globals(std::vector<int32_t> &globals);

typedef float qcfloat_t;
typedef int32_t qcint_t;
typedef uint32_t qcuint_t;

struct code_t {
    void* operator new(std::size_t);
    void operator delete(void*);
    code_t();
    ~code_t();
    std::vector<prog_section_statement_t> statements;
    std::vector<int> linenums;
    std::vector<int> columnnums;
    std::vector<prog_section_def_t> defs;
    std::vector<prog_section_field_t> fields;
    std::vector<prog_section_function_t> functions;
    std::vector<int> globals;
    std::vector<char> chars;
    uint16_t crc = 0;
    uint32_t entfields = 0;
    ht string_cache;
    qcint_t string_cached_empty = 0;
};

/*
 * A shallow copy of a lex_file to remember where which ast node
 * came from.
 */
struct lex_ctx_t {
    const char *file;
    size_t line;
    size_t column;
};

/*
 * code_write          -- writes out the compiled file
 * code_init           -- prepares the code file
 * code_genstrin       -- generates string for code
 * code_alloc_field    -- allocated a field
 * code_push_statement -- keeps statements and linenumbers together
 * code_pop_statement  -- keeps statements and linenumbers together
 */
bool      code_write         (code_t *, const char *filename, const char *lno);
GMQCC_WARN
code_t   *code_init          (void);
void      code_cleanup       (code_t *);
uint32_t  code_genstring     (code_t *, const char *string);
qcint_t   code_alloc_field   (code_t *, size_t qcsize);
void      code_push_statement(code_t *, prog_section_statement_t *stmt, lex_ctx_t ctx);
void      code_pop_statement (code_t *);


/* conout.c */
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

FILE *con_default_out(void);
FILE *con_default_err(void);

void con_vprintmsg (int level, const char *name, size_t line, size_t column, const char *msgtype, const char *msg, va_list ap);
void con_printmsg  (int level, const char *name, size_t line, size_t column, const char *msgtype, const char *msg, ...);
void con_cvprintmsg(lex_ctx_t ctx, int lvl, const char *msgtype, const char *msg, va_list ap);
void con_cprintmsg (lex_ctx_t ctx, int lvl, const char *msgtype, const char *msg, ...);

void con_close (void);
void con_init  (void);
void con_reset (void);
void con_color (int);
int  con_verr  (const char *, va_list);
int  con_vout  (const char *, va_list);
int  con_err   (const char *, ...);
int  con_out   (const char *, ...);

/* error/warning interface */
extern size_t compile_errors;
extern size_t compile_Werrors;
extern size_t compile_warnings;

void /********/ compile_error_  (lex_ctx_t ctx, /*LVL_ERROR*/ const char *msg, ...);
void /********/ vcompile_error  (lex_ctx_t ctx, /*LVL_ERROR*/ const char *msg, va_list ap);
bool GMQCC_WARN compile_warning_(lex_ctx_t ctx, int warntype, const char *fmt, ...);
bool GMQCC_WARN vcompile_warning(lex_ctx_t ctx, int warntype, const char *fmt, va_list ap);
void            compile_show_werrors(void);

template <typename T>
inline constexpr const T formatNormalize(const T argument) { return argument; }

inline const char *formatNormalize(const std::string &argument) {
    return argument.c_str();
}

template<typename... Ts>
inline bool GMQCC_WARN compile_warning(lex_ctx_t ctx, int warntype, const char *fmt, const Ts&... ts) {
    return compile_warning_(ctx, warntype, fmt, formatNormalize(ts)...);
}
template<typename... Ts>
inline void /********/ compile_error   (lex_ctx_t ctx, /*LVL_ERROR*/ const char *msg, const Ts&... ts) {
    return compile_error_(ctx, msg, formatNormalize(ts)...);
}

/* ir.c */
/* TODO: cleanup */
enum store_type {
    store_global,
    store_local,  /* local, assignable for now, should get promoted later */
    store_param,  /* parameters, they are locals with a fixed position */
    store_value,  /* unassignable */
    store_return  /* unassignable, at OFS_RETURN */
};

struct vec3_t {
    qcfloat_t x, y, z;
};

/* exec.c */

/* TODO: cleanup */
/*
 * Darkplaces has (or will have) a 64 bit prog loader
 * where the 32 bit qc program is autoconverted on load.
 * Since we may want to support that as well, let's redefine
 * float and int here.
 */
typedef union {
    qcint_t   _int;
    qcint_t    string;
    qcint_t    function;
    qcint_t    edict;
    qcfloat_t _float;
    qcfloat_t vector[3];
    qcint_t   ivector[3];
} qcany_t;

typedef char qcfloat_t_size_is_correct [sizeof(qcfloat_t) == 4 ?1:-1];
typedef char qcint_t_size_is_correct   [sizeof(qcint_t)   == 4 ?1:-1];

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

typedef struct qc_program qc_program_t;
typedef int (*prog_builtin_t)(qc_program_t *prog);

struct qc_exec_stack_t {
    qcint_t stmt;
    size_t localsp;
    prog_section_function_t *function;
};

struct qc_program {
    qc_program() = delete;
    qc_program(const char *name, uint16_t crc, size_t entfields);

    std::string filename;
    std::vector<prog_section_statement_t> code;
    std::vector<prog_section_def_t> defs;
    std::vector<prog_section_def_t> fields;
    std::vector<prog_section_function_t> functions;
    std::vector<char> strings;
    std::vector<qcint_t> globals;
    std::vector<qcint_t> entitydata;
    std::vector<bool> entitypool;

    std::vector<const char*> function_stack;

    uint16_t crc16;

    size_t tempstring_start;
    size_t tempstring_at;

    qcint_t  vmerror;

    std::vector<size_t> profile;

    prog_builtin_t *builtins;
    size_t          builtins_count;

    /* size_t ip; */
    qcint_t  entities;
    size_t entityfields;
    bool   allowworldwrites;

    std::vector<qcint_t> localstack;
    std::vector<qc_exec_stack_t> stack;
    size_t statement;

    size_t xflags;

    int    argc; /* current arg count for debugging */

    /* cached fields */
    struct {
        qcint_t frame;
        qcint_t nextthink;
        qcint_t think;
    } cached_fields;

    struct {
        qcint_t self;
        qcint_t time;
    } cached_globals;

    bool supports_state; /* is INSTR_STATE supported? */
};

qc_program_t*       prog_load      (const char *filename, bool ignoreversion);
void                prog_delete    (qc_program_t *prog);
bool                prog_exec      (qc_program_t *prog, prog_section_function_t *func, size_t flags, long maxjumps);
const char*         prog_getstring (qc_program_t *prog, qcint_t str);
prog_section_def_t* prog_entfield  (qc_program_t *prog, qcint_t off);
prog_section_def_t* prog_getdef    (qc_program_t *prog, qcint_t off);
qcany_t*            prog_getedict  (qc_program_t *prog, qcint_t e);
qcint_t             prog_tempstring(qc_program_t *prog, const char *_str);


/* parser.c */
struct parser_t;
parser_t *parser_create(void);
bool parser_compile_file(parser_t *parser, const char *);
bool parser_compile_string(parser_t *parser, const char *, const char *, size_t);
bool parser_finish(parser_t *parser, const char *);

/* ftepp.c */
struct ftepp_t;
ftepp_t *ftepp_create           (void);
bool ftepp_preprocess_file  (ftepp_t *ftepp, const char *filename);
bool ftepp_preprocess_string(ftepp_t *ftepp, const char *name, const char *str);
void ftepp_finish(ftepp_t *ftepp);
const char *ftepp_get(ftepp_t *ftepp);
void ftepp_flush(ftepp_t *ftepp);
void ftepp_add_define(ftepp_t *ftepp, const char *source, const char *name);
void ftepp_add_macro(ftepp_t *ftepp, const char *name,   const char *value);

/* main.c */

#if 1
/* Helpers to allow for a whole lot of flags. Otherwise we'd limit
 * to 32 or 64 -f options...
 */
struct longbit {
    size_t  idx; /* index into an array of 32 bit words */
    uint8_t bit; /* bit index for the 8 bit group idx points to */
};
#define LONGBIT(bit) { ((bit)/32), ((bit)%32) }
#define LONGBIT_SET(B, I) ((B).idx = (I)/32, (B).bit = ((I)%32))
#else
typedef uint32_t longbit;
#define LONGBIT(bit) (bit)
#define LONGBIT_SET(B, I) ((B) = (I))
#endif

/* utf.8 */
typedef long utf8ch_t;
int utf8_from(char *, utf8ch_t);
int utf8_to(utf8ch_t *, const unsigned char *, size_t);

/* opts.c */
struct opts_flag_def_t {
    const char *name;
    longbit     bit;
};

bool opts_setflag  (const char *, bool);
bool opts_setwarn  (const char *, bool);
bool opts_setwerror(const char *, bool);
bool opts_setoptim (const char *, bool);

void opts_init         (const char *, int, size_t);
void opts_set          (uint32_t   *, size_t, bool);
void opts_setoptimlevel(unsigned int);
void opts_ini_init     (const char *);

/* Saner flag handling */
void opts_backup_non_Wall(void);
void opts_restore_non_Wall(void);
void opts_backup_non_Werror_all(void);
void opts_restore_non_Werror_all(void);


enum {
# define GMQCC_TYPE_FLAGS
# define GMQCC_DEFINE_FLAG(X) X,
#  include "opts.def"
    COUNT_FLAGS
};

enum {
# define GMQCC_TYPE_WARNS
# define GMQCC_DEFINE_FLAG(X) WARN_##X,
#  include "opts.def"
    COUNT_WARNINGS
};

enum {
# define GMQCC_TYPE_OPTIMIZATIONS
# define GMQCC_DEFINE_FLAG(NAME, MIN_O) OPTIM_##NAME,
#  include "opts.def"
    COUNT_OPTIMIZATIONS
};

enum {
#   define GMQCC_TYPE_OPTIONS
#   define GMQCC_DEFINE_FLAG(X) OPTION_##X,
#   include "opts.def"
    OPTION_COUNT
};

extern const opts_flag_def_t opts_flag_list[COUNT_FLAGS+1];
extern const opts_flag_def_t opts_warn_list[COUNT_WARNINGS+1];
extern const opts_flag_def_t opts_opt_list[COUNT_OPTIMIZATIONS+1];
extern const unsigned int    opts_opt_oflag[COUNT_OPTIMIZATIONS+1];
extern unsigned int          opts_optimizationcount[COUNT_OPTIMIZATIONS];

/* other options: */
enum {
    COMPILER_QCC,     /* circa  QuakeC */
    COMPILER_FTEQCC,  /* fteqcc QuakeC */
    COMPILER_QCCX,    /* qccx   QuakeC */
    COMPILER_GMQCC    /* this   QuakeC */
};

struct opt_value_t {
    union {
        bool b;
        uint16_t u16;
        uint32_t u32;
        union {
            char *p;
            const char *c;
        } str;
    } data;
    bool allocated;
};

struct opts_cmd_t {
    opt_value_t  options      [OPTION_COUNT];
    uint32_t     flags        [1 + (COUNT_FLAGS         / 32)];
    uint32_t     warn         [1 + (COUNT_WARNINGS      / 32)];
    uint32_t     werror       [1 + (COUNT_WARNINGS      / 32)];
    uint32_t     warn_backup  [1 + (COUNT_WARNINGS      / 32)];
    uint32_t     werror_backup[1 + (COUNT_WARNINGS      / 32)];
    uint32_t     optimization [1 + (COUNT_OPTIMIZATIONS / 32)];
    bool         optimizeoff; /* True when -O0 */
};

extern opts_cmd_t opts;

#define OPTS_GENERIC(f,i)    (!! (((f)[(i)/32]) & (1U << (unsigned)((i)%32))))

#define OPTS_FLAG(i)         OPTS_GENERIC(opts.flags,        (i))
#define OPTS_WARN(i)         OPTS_GENERIC(opts.warn,         (i))
#define OPTS_WERROR(i)       OPTS_GENERIC(opts.werror,       (i))
#define OPTS_OPTIMIZATION(i) OPTS_GENERIC(opts.optimization, (i))

#define OPTS_OPTION_DUPED(X) (opts.options[X].allocated)
#define OPTS_OPTION_BOOL(X)  (opts.options[X].data.b)
#define OPTS_OPTION_U16(X)   (opts.options[X].data.u16)
#define OPTS_OPTION_U32(X)   (opts.options[X].data.u32)
#define OPTS_OPTION_STR(X)   (opts.options[X].data.str.c)
#define OPTS_OPTION_DUP(X)  *(OPTS_OPTION_DUPED(X)=true, &(opts.options[X].data.str.p))

#endif /*! GMQCC_HDR */
