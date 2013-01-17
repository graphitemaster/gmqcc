/*
 * Copyright (C) 2012, 2013
 *     Dale Weiler
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
#ifndef GMQCC_HDR
#define GMQCC_HDR
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

/*
 * Disable some over protective warnings in visual studio because fixing them is a waste
 * of my time.
 */
#ifdef _MSC_VER
#   pragma warning(disable : 4244 ) /* conversion from 'int' to 'float', possible loss of data */
#   pragma warning(disable : 4018 ) /* signed/unsigned mismatch                                */
#endif

#define GMQCC_VERSION_MAJOR 0
#define GMQCC_VERSION_MINOR 3
#define GMQCC_VERSION_PATCH 0
#define GMQCC_VERSION_BUILD(J,N,P) (((J)<<16)|((N)<<8)|(P))
#define GMQCC_VERSION \
    GMQCC_VERSION_BUILD(GMQCC_VERSION_MAJOR, GMQCC_VERSION_MINOR, GMQCC_VERSION_PATCH)
/* Undefine the following on a release-tag: */
#define GMQCC_VERSION_TYPE_DEVEL

/* Full version string in case we need it */
#ifdef GMQCC_GITINFO
#    define GMQCC_DEV_VERSION_STRING "git build: " GMQCC_GITINFO "\n"
#elif defined(GMQCC_VERSION_TYPE_DEVEL)
#    define GMQCC_DEV_VERSION_STRING "development build\n"
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

/*
 * We cannot rely on C99 at all, since compilers like MSVC
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
/*
 * Visual studio has __forcinline we can use.  So lets use that
 * I suspect it also has just __inline of some sort, but our use
 * of inline is correct (not guessed), WE WANT IT TO BE INLINE
 */
#elif defined(_MSC_VER)
#    define GMQCC_INLINE __forceinline
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

#ifndef _MSC_VER
#   include <stdint.h>
#else
    typedef unsigned __int8  uint8_t;
    typedef unsigned __int16 uint16_t;
    typedef unsigned __int32 uint32_t;
    typedef unsigned __int64 uint64_t;

    typedef __int16          int16_t;
    typedef __int32          int32_t;
    typedef __int64          int64_t;
#endif

/* 
 *windows makes these prefixed because they're C99
 * TODO: utility versions that are type-safe and not
 * just plain textual subsitution.
 */
#ifdef _MSC_VER
#    define snprintf(X, Y, Z, ...) _snprintf(X, Y, Z, __VA_ARGS__)
    /* strtof doesn't exist -> strtod does though :) */
#    define strtof(X, Y)          (float)(strtod(X, Y))
#endif

/*
 * Very roboust way at determining endianess at compile time: this handles
 * almost every possible situation.  Otherwise a runtime check has to be
 * performed.
 */
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
#       endif
#   elif !defined (__MINGW32__)
#       include <endian.h>
#       if !defined (__BEOS__)
#           include <byteswap.h>
#       endif
#   endif
#endif
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
#       endif
#   elif defined (_LITTLE_ENDIAN) || defined (_BIG_ENDIAN)
#       if defined (_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_LITTLE
#       elif !defined (_LITTLE_ENDIAN) && defined (_BIG_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_BIG
#       elif defined (_BYTE_ORDER) && (_BYTE_ORDER == _LITTLE_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_LITTLE
#       elif defined (_BYTE_ORDER) && (_BYTE_ORDER == _BIG_ENDIAN)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_BIG
#       endif
#   elif defined (__LITTLE_ENDIAN__) || defined (__BIG_ENDIAN__)
#       if defined (__LITTLE_ENDIAN__) && !defined (__BIG_ENDIAN__)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_LITTLE
#       elif !defined (__LITTLE_ENDIAN__) && defined (__BIG_ENDIAN__)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_BIG
#       elif defined (__BYTE_ORDER__) && (__BYTE_ORDER__ == __LITTLE_ENDIAN__)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_LITTLE
#       elif defined (__BYTE_ORDER__) && (__BYTE_ORDER__ == __BIG_ENDIAN__)
#           define PLATFORM_BYTE_ORDER GMQCC_BYTE_ORDER_BIG
#       endif
#   endif
#endif
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



/*===================================================================*/
/*=========================== util.c ================================*/
/*===================================================================*/
void *util_memory_a      (size_t, /*****/ unsigned int, const char *);
void *util_memory_r      (void *, size_t, unsigned int, const char *);
void  util_memory_d      (void *);
void  util_meminfo       ();

bool  util_filexists     (const char *);
bool  util_strupper      (const char *);
bool  util_strdigit      (const char *);
char *util_strdup        (const char *);
void  util_debug         (const char *, const char *, ...);
void  util_endianswap    (void *,  size_t, unsigned int);

size_t util_strtocmd    (const char *, char *, size_t);
size_t util_strtononcmd (const char *, char *, size_t);

uint16_t util_crc16(uint16_t crc, const char *data, size_t len);

void     util_seed(uint32_t);
uint32_t util_rand();

int util_vasprintf(char **ret, const char *fmt, va_list);
int util_asprintf (char **ret, const char *fmt, ...);


#ifdef NOTRACK
#    define mem_a(x)    malloc (x)
#    define mem_d(x)    free   ((void*)x)
#    define mem_r(x, n) realloc((void*)x, n)
#else
#    define mem_a(x)    util_memory_a((x), __LINE__, __FILE__)
#    define mem_d(x)    util_memory_d((void*)(x))
#    define mem_r(x, n) util_memory_r((void*)(x), (n), __LINE__, __FILE__)
#endif

/*
 * A flexible vector implementation: all vector pointers contain some
 * data about themselfs exactly - sizeof(vector_t) behind the pointer
 * this data is represented in the structure below.  Doing this allows
 * us to use the array [] to access individual elements from the vector
 * opposed to using set/get methods.
 */     
typedef struct {
    size_t  allocated;
    size_t  used;

    /* can be extended now! whoot */
} vector_t;

/* hidden interface */
void _util_vec_grow(void **a, size_t i, size_t s);
#define GMQCC_VEC_WILLGROW(X,Y) ( \
    ((!(X) || vec_meta(X)->used + Y >= vec_meta(X)->allocated)) ? \
        (void)_util_vec_grow(((void**)&(X)), (Y), sizeof(*(X))) : \
        (void)0                                                   \
)

/* exposed interface */
#define vec_meta(A)       (((vector_t*)(A)) - 1)
#define vec_free(A)       ((void)((A) ? (mem_d((void*)vec_meta(A)), (A) = NULL) : 0))
#define vec_push(A,V)     (GMQCC_VEC_WILLGROW((A),1), (A)[vec_meta(A)->used++] = (V))
#define vec_size(A)       ((A) ? vec_meta(A)->used : 0)
#define vec_add(A,N)      (GMQCC_VEC_WILLGROW((A),(N)), vec_meta(A)->used += (N), &(A)[vec_meta(A)->used-(N)])
#define vec_last(A)       ((A)[vec_meta(A)->used - 1])
#define vec_pop(A)        ((void)(vec_meta(A)->used -= 1))
#define vec_shrinkto(A,N) ((void)(vec_meta(A)->used  = (N)))
#define vec_shrinkby(A,N) ((void)(vec_meta(A)->used -= (N)))
#define vec_append(A,N,S) ((void)(memcpy(vec_add((A), (N)), (S), (N) * sizeof(*(S)))))
#define vec_upload(X,Y,S) ((void)(memcpy(vec_add((X), (S) * sizeof(*(Y))), (Y), (S) * sizeof(*(Y)))))
#define vec_remove(A,I,N) ((void)(memmove((A)+(I),(A)+((I)+(N)),sizeof(*(A))*(vec_meta(A)->used-(I)-(N))),vec_meta(A)->used-=(N)))

typedef struct trie_s {
    void          *value;
    struct trie_s *entries;
} correct_trie_t;

correct_trie_t* correct_trie_new();

typedef struct hash_table_t {
    size_t                size;
    struct hash_node_t **table;
} hash_table_t, *ht;

typedef struct hash_set_t {
    size_t  bits;
    size_t  mask;
    size_t  capacity;
    size_t *items;
    size_t  total;
} hash_set_t, *hs;

/*
 * hashtable implementation:
 *
 * Note:
 *      This was designed for pointers:  you manage the life of the object yourself
 *      if you do use this for non-pointers please be warned that the object may not
 *      be valid if the duration of it exceeds (i.e on stack).  So you need to allocate
 *      yourself, or put those in global scope to ensure duration is for the whole
 *      runtime.
 *
 * util_htnew(size)                             -- to make a new hashtable
 * util_htset(table, key, value, sizeof(value)) -- to set something in the table
 * util_htget(table, key)                       -- to get something from the table
 * util_htdel(table)                            -- to delete the table
 *
 * example of use:
 *
 * ht    foo  = util_htnew(1024);
 * int   data = 100;
 * char *test = "hello world\n";
 * util_htset(foo, "foo", (void*)&data);
 * util_gtset(foo, "bar", (void*)test);
 *
 * printf("foo: %d, bar %s",
 *     *((int *)util_htget(foo, "foo")),
 *      ((char*)util_htget(foo, "bar"))
 * );
 *
 * util_htdel(foo);
 */
hash_table_t *util_htnew (size_t size);
void          util_htset (hash_table_t *ht, const char *key, void *value);
void          util_htdel (hash_table_t *ht);
size_t        util_hthash(hash_table_t *ht, const char *key);
void          util_htseth(hash_table_t *ht, const char *key, size_t hash, void *value);

void         *util_htget (hash_table_t *ht, const char *key);
void         *util_htgeth(hash_table_t *ht, const char *key, size_t hash);

/*
 * hashset implementation:
 *      This was designed for pointers:  you manage the life of the object yourself
 *      if you do use this for non-pointers please be warned that the object may not
 *      be valid if the duration of it exceeds (i.e on stack).  So you need to allocate
 *      yourself, or put those in global scope to ensure duration is for the whole
 *      runtime.
 *
 * util_hsnew()                             -- to make a new hashset
 * util_hsadd(set, key)                     -- to add something in the set
 * util_hshas(set, key)                     -- to check if something is in the set
 * util_hsrem(set, key)                     -- to remove something in the set
 * util_hsdel(set)                          -- to delete the set
 *
 * example of use:
 * 
 * hs    foo = util_hsnew();
 * char *bar = "hello blub\n";
 * char *baz = "hello dale\n";
 *
 * util_hsadd(foo, bar);
 * util_hsadd(foo, baz);
 * util_hsrem(foo, baz);
 *
 * printf("bar %d | baz %d\n",
 *     util_hshas(foo, bar),
 *     util_hshad(foo, baz)
 * );
 *
 * util_hsdel(foo);  
 */

hash_set_t *util_hsnew(void);
int         util_hsadd(hash_set_t *, void *);
int         util_hshas(hash_set_t *, void *);
int         util_hsrem(hash_set_t *, void *);
void        util_hsdel(hash_set_t *);
 
/*===================================================================*/
/*============================ file.c ===============================*/
/*===================================================================*/
GMQCC_INLINE void    file_close  (FILE *);
GMQCC_INLINE int     file_error  (FILE *);
GMQCC_INLINE int     file_getc   (FILE *);
GMQCC_INLINE int     file_printf (FILE *, const char *, ...);
GMQCC_INLINE int     file_puts   (FILE *, const char *);
GMQCC_INLINE int     file_putc   (FILE *, int);
GMQCC_INLINE int     file_seek   (FILE *, long int, int);

GMQCC_INLINE size_t  file_read   (void *,        size_t, size_t, FILE *);
GMQCC_INLINE size_t  file_write  (const void *,  size_t, size_t, FILE *);

GMQCC_INLINE FILE   *file_open   (const char *, const char *);
/*NOINLINE*/ int     file_getline(char  **, size_t *, FILE *);


/*===================================================================*/
/*=========================== correct.c =============================*/
/*===================================================================*/
typedef struct {
    char ***edits;
} correction_t;

void  correct_del (correct_trie_t*, size_t **);
void  correct_add (correct_trie_t*, size_t ***, const char *);
char *correct_str (correction_t *, correct_trie_t*, const char *);
void  correct_init(correction_t *);
void  correct_free(correction_t *);

/*===================================================================*/
/*=========================== code.c ================================*/
/*===================================================================*/

/* TODO: cleanup */
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

    TYPE_NIL      , /* it's its own type / untyped */
    TYPE_NOEXPR   , /* simply invalid in expressions */

    TYPE_COUNT
};

/* const/var qualifiers */
#define CV_NONE   0
#define CV_CONST  1
#define CV_VAR   -1
#define CV_WRONG  0x8000 /* magic number to help parsing */

extern const char *type_name        [TYPE_COUNT];
extern uint16_t    type_store_instr [TYPE_COUNT];
extern uint16_t    field_store_instr[TYPE_COUNT];

/*
 * could use type_store_instr + INSTR_STOREP_F - INSTR_STORE_F
 * but this breaks when TYPE_INTEGER is added, since with the enhanced
 * instruction set, the old ones are left untouched, thus the _I instructions
 * are at a seperate place.
 */
extern uint16_t type_storep_instr[TYPE_COUNT];
extern uint16_t type_eq_instr    [TYPE_COUNT];
extern uint16_t type_ne_instr    [TYPE_COUNT];
extern uint16_t type_not_instr   [TYPE_COUNT];

typedef struct {
    uint32_t offset;      /* Offset in file of where data begins  */
    uint32_t length;      /* Length of section (how many of)      */
} prog_section;

typedef struct {
    uint32_t     version;      /* Program version (6)     */
    uint16_t     crc16;
    uint16_t     skip;

    prog_section statements;   /* prog_section_statement  */
    prog_section defs;         /* prog_section_def        */
    prog_section fields;       /* prog_section_field      */
    prog_section functions;    /* prog_section_function   */
    prog_section strings;
    prog_section globals;
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
    int32_t   nargs;      /* number of arguments                  */
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
    VINSTR_COND,
    /* A never returning CALL.
     * Creating this causes IR blocks to be marked as 'final'.
     * No-Return-Call
     */
    VINSTR_NRCALL
};

/* TODO: cleanup this mess */
extern prog_section_statement *code_statements;
extern int                    *code_linenums;
extern prog_section_def       *code_defs;
extern prog_section_field     *code_fields;
extern prog_section_function  *code_functions;
extern int                    *code_globals;
extern char                   *code_chars;
extern uint16_t code_crc;

/* uhh? */
typedef float   qcfloat;
typedef int32_t qcint;

/*
 * code_write -- writes out the compiled file
 * code_init  -- prepares the code file
 */
bool     code_write       (const char *filename, const char *lno);
void     code_init        ();
uint32_t code_genstring   (const char *string);
qcint    code_alloc_field (size_t qcsize);

/* this function is used to keep statements and linenumbers together */
void     code_push_statement(prog_section_statement *stmt, int linenum);
void     code_pop_statement();

/*
 * A shallow copy of a lex_file to remember where which ast node
 * came from.
 */
typedef struct {
    const char *file;
    size_t      line;
} lex_ctx;

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

FILE *con_default_out();
FILE *con_default_err();

void con_vprintmsg (int level, const char *name, size_t line, const char *msgtype, const char *msg, va_list ap);
void con_printmsg  (int level, const char *name, size_t line, const char *msgtype, const char *msg, ...);
void con_cvprintmsg(void *ctx, int lvl, const char *msgtype, const char *msg, va_list ap);
void con_cprintmsg (void *ctx, int lvl, const char *msgtype, const char *msg, ...);

void con_close ();
void con_init  ();
void con_reset ();
void con_color (int);
int  con_change(const char *, const char *);
int  con_verr  (const char *, va_list);
int  con_vout  (const char *, va_list);
int  con_err   (const char *, ...);
int  con_out   (const char *, ...);

/* error/warning interface */
extern size_t compile_errors;
extern size_t compile_Werrors;
extern size_t compile_warnings;

void /********/ compile_error   (lex_ctx ctx, /*LVL_ERROR*/ const char *msg, ...);
void /********/ vcompile_error  (lex_ctx ctx, /*LVL_ERROR*/ const char *msg, va_list ap);
bool GMQCC_WARN compile_warning (lex_ctx ctx, int warntype, const char *fmt, ...);
bool GMQCC_WARN vcompile_warning(lex_ctx ctx, int warntype, const char *fmt, va_list ap);
void            compile_show_werrors();

/*===================================================================*/
/*========================= assembler.c =============================*/
/*===================================================================*/
/* TODO: remove this ... */
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

/*===================================================================*/
/*============================= exec.c ==============================*/
/*===================================================================*/

/* TODO: cleanup */
/*
 * Darkplaces has (or will have) a 64 bit prog loader
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

qc_program* prog_load(const char *filename, bool ignoreversion);
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
bool parser_compile_file  (const char *);
bool parser_compile_string(const char *, const char *, size_t);
bool parser_finish        (const char *);
void parser_cleanup       ();

/*===================================================================*/
/*====================== ftepp.c commandline ========================*/
/*===================================================================*/
struct lex_file_s;
typedef struct {
    const char  *name;
    char      *(*func)(struct lex_file_s *);
} ftepp_predef_t;

/*
 * line, file, counter, counter_last, random, random_last, date, time
 * increment when items are added
 */
#define FTEPP_PREDEF_COUNT 8

bool        ftepp_init             ();
bool        ftepp_preprocess_file  (const char *filename);
bool        ftepp_preprocess_string(const char *name, const char *str);
void        ftepp_finish           ();
const char *ftepp_get              ();
void        ftepp_flush            ();
void        ftepp_add_define       (const char *source, const char *name);
void        ftepp_add_macro        (const char *name,   const char *value);

extern const ftepp_predef_t ftepp_predefs[FTEPP_PREDEF_COUNT];

/*===================================================================*/
/*======================= main.c commandline ========================*/
/*===================================================================*/

#if 1
/* Helpers to allow for a whole lot of flags. Otherwise we'd limit
 * to 32 or 64 -f options...
 */
typedef struct {
    size_t  idx; /* index into an array of 32 bit words */
    uint8_t bit; /* bit index for the 8 bit group idx points to */
} longbit;
#define LONGBIT(bit) { ((bit)/32), ((bit)%32) }
#define LONGBIT_SET(B, I) ((B).idx = (I)/32, (B).bit = ((I)%32))
#else
typedef uint32_t longbit;
#define LONGBIT(bit) (bit)
#define LONGBIT_SET(B, I) ((B) = (I))
#endif

/*===================================================================*/
/*=========================== utf8lib.c =============================*/
/*===================================================================*/
typedef uint32_t uchar_t;

bool    u8_analyze (const char *_s, size_t *_start, size_t *_len, uchar_t *_ch, size_t _maxlen);
size_t  u8_strlen  (const char*);
size_t  u8_strnlen (const char*, size_t);
uchar_t u8_getchar (const char*, const char**);
uchar_t u8_getnchar(const char*, const char**, size_t);
int     u8_fromchar(uchar_t w,   char *to,     size_t maxlen);

/*===================================================================*/
/*============================= opts.c ==============================*/
/*===================================================================*/
typedef struct {
    const char *name;
    longbit     bit;
} opts_flag_def;

bool opts_setflag  (const char *, bool);
bool opts_setwarn  (const char *, bool);
bool opts_setwerror(const char *, bool);
bool opts_setoptim (const char *, bool);

void opts_init         (const char *, int, size_t);
void opts_set          (uint32_t   *, size_t, bool);
void opts_setoptimlevel(unsigned int);
void opts_ini_init     (const char *);

/* Saner flag handling */
void opts_backup_non_Wall();
void opts_restore_non_Wall();
void opts_backup_non_Werror_all();
void opts_restore_non_Werror_all();

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

enum {
# define GMQCC_TYPE_OPTIMIZATIONS
# define GMQCC_DEFINE_FLAG(NAME, MIN_O) OPTIM_##NAME,
#  include "opts.def"
    COUNT_OPTIMIZATIONS
};
static const opts_flag_def opts_opt_list[] = {
# define GMQCC_TYPE_OPTIMIZATIONS
# define GMQCC_DEFINE_FLAG(NAME, MIN_O) { #NAME, LONGBIT(OPTIM_##NAME) },
#  include "opts.def"
    { NULL, LONGBIT(0) }
};
static const unsigned int opts_opt_oflag[] = {
# define GMQCC_TYPE_OPTIMIZATIONS
# define GMQCC_DEFINE_FLAG(NAME, MIN_O) MIN_O,
#  include "opts.def"
    0
};
extern unsigned int opts_optimizationcount[COUNT_OPTIMIZATIONS];

/* other options: */
typedef enum {
    COMPILER_QCC,     /* circa  QuakeC */
    COMPILER_FTEQCC,  /* fteqcc QuakeC */
    COMPILER_QCCX,    /* qccx   QuakeC */
    COMPILER_GMQCC    /* this   QuakeC */
} opts_std_t;

/* TODO: cleanup this */
typedef struct {
    uint32_t    O;              /* -Ox           */
    const char *output;         /* -o file       */
    bool        quiet;          /* -q --quiet    */
    bool        g;              /* -g            */
    opts_std_t  standard;       /* -std=         */
    bool        debug;          /* -debug        */
    bool        memchk;         /* -memchk       */
    bool        dumpfin;        /* -dumpfin      */
    bool        dump;           /* -dump         */
    bool        forcecrc;       /* --force-crc=  */
    uint16_t    forced_crc;     /* --force-crc=  */
    bool        pp_only;        /* -E            */
    size_t      max_array_size; /* --max-array=  */
    bool        add_info;       /* --add-info    */
    bool        correction;     /* --correct     */

    uint32_t flags        [1 + (COUNT_FLAGS         / 32)];
    uint32_t warn         [1 + (COUNT_WARNINGS      / 32)];
    uint32_t werror       [1 + (COUNT_WARNINGS      / 32)];
    uint32_t warn_backup  [1 + (COUNT_WARNINGS      / 32)];
    uint32_t werror_backup[1 + (COUNT_WARNINGS      / 32)];
    uint32_t optimization [1 + (COUNT_OPTIMIZATIONS / 32)];
} opts_cmd_t;

extern opts_cmd_t opts;

#define OPTS_GENERIC(f,i)    (!! (((f)[(i)/32]) & (1<< ((i)%32))))
#define OPTS_FLAG(i)         OPTS_GENERIC(opts.flags,        (i))
#define OPTS_WARN(i)         OPTS_GENERIC(opts.warn,         (i))
#define OPTS_WERROR(i)       OPTS_GENERIC(opts.werror,       (i))
#define OPTS_OPTIMIZATION(i) OPTS_GENERIC(opts.optimization, (i))

#endif
