/*
 * Copyright (C) 2012 
 * 	Dale Weiler
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
#include <stdio.h>

//===================================================================
//============================ lex.c ================================
//===================================================================
struct lex_file {
	FILE *file;
	char  peek  [5];
	char  lastok[8192];
	
	int   line;
	int   last;
	int   current;
	int   length;
	int   size;
};

/*
 * It's important that this table never exceed 32 keywords, the ascii
 * table starts at 33 (and we don't want conflicts)
 */
#define TOKEN_DO       0
#define TOKEN_ELSE     1
#define TOKEN_IF       2
#define TOKEN_WHILE    3
#define TOKEN_BREAK    4
#define TOKEN_CONTINUE 5
#define TOKEN_RETURN   6
#define TOKEN_GOTO     7
#define TOKEN_FOR      8   // extension
#define TOKEN_TYPEDEF  9   // extension

// ensure the token types are out of the
// bounds of anyothers that may conflict.
#define TOKEN_FLOAT    110
#define TOKEN_VECTOR   111
#define TOKEN_STRING   112
#define TOKEN_ENTITY   113
#define TOKEN_VOID     114

/*
 * Lexer state constants, these are numbers for where exactly in
 * the lexing the lexer is at. Or where it decided to stop if a lexer
 * error occurs.  These numbers must be > where the ascii-table ends
 * and > the last type token which is TOKEN_VOID
 */
#define LEX_COMMENT    1128 
#define LEX_CHRLIT     1129
#define LEX_STRLIT     1130
#define LEX_IDENT      1131

int              lex_token(struct lex_file *);
void             lex_reset(struct lex_file *);
void             lex_close(struct lex_file *);
struct lex_file *lex_open (FILE *);

//===================================================================
//========================== error.c ================================
//===================================================================
#define ERROR_LEX      (SHRT_MAX+0)
#define ERROR_PARSE    (SHRT_MAX+1)
#define ERROR_INTERNAL (SHRT_MAX+2)
#define ERROR_COMPILER (SHRT_MAX+3)
#define ERROR_PREPRO   (SHRT_MAX+4)
int error(int, const char *, ...);

//===================================================================
//========================== parse.c ================================
//===================================================================
int parse_tree(struct lex_file *);
struct parsenode {
	struct parsenode *next;
	int               type; /* some token */
};

//===================================================================
//========================== typedef.c ==============================
//===================================================================
typedef struct typedef_node_t {
	char      *name;
} typedef_node;

void          typedef_init();
void          typedef_clear();
typedef_node *typedef_find(const char *);
int           typedef_add (const char *, const char *);


//===================================================================
//=========================== util.c ================================
//===================================================================
void *util_memory_a(unsigned int, unsigned int, const char *);
void  util_memory_d(void       *, unsigned int, const char *);
char *util_strdup  (const char *);

#ifdef NOTRACK
#	define mem_a(x) malloc(x)
#	define mem_d(x) free  (x)
#else
#	define mem_a(x) util_memory_a((x), __LINE__, __FILE__)
#	define mem_d(x) util_memory_d((x), __LINE__, __FILE__)
#endif

#define VECTOR_MAKE(T,N)                                                 \
    T*     N##_data      = NULL;                                         \
    long   N##_elements  = 0;                                            \
    long   N##_allocated = 0;                                            \
    int    N##_add(T element) {                                          \
        if (N##_elements == N##_allocated) {                             \
            if (N##_allocated == 0) {                                    \
                N##_allocated = 12;                                      \
            } else {                                                     \
                N##_allocated *= 2;                                      \
            }                                                            \
            void *temp = realloc(N##_data, (N##_allocated * sizeof(T))); \
            if  (!temp) {                                                \
                free(temp);                                              \
                return -1;                                               \
            }                                                            \
            N##_data = (T*)temp;                                         \
        }                                                                \
        N##_data[N##_elements] = element;                                \
        return   N##_elements++;                                         \
    }

//===================================================================
//=========================== code.c ================================
//===================================================================
#define TYPE_VOID     0
#define TYPE_STRING   1
#define TYPE_FLOAT    2
#define TYPE_VECTOR   3
#define TYPE_ENTITY   4
#define TYPE_FIELD    5
#define TYPE_FUNCTION 6
#define TYPE_POINTER  7

/*
 * Each paramater incerements by 3 since vector types hold
 * 3 components (x,y,z).
 */
#define	OFS_NULL      0
#define	OFS_RETURN    1
#define	OFS_PARM0     (OFS_RETURN+3)
#define	OFS_PARM1     (OFS_PARM0 +3)
#define	OFS_PARM2     (OFS_PARM1 +3)
#define	OFS_PARM3     (OFS_PARM2 +3)
#define	OFS_PARM4     (OFS_PARM3 +3)
#define OFS_PARM5     (OFS_PARM4 +3)
#define OFS_PARM6     (OFS_PARM5 +3)
#define OFS_PARM7     (OFS_PARM6 +3)

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
	INSTR_BITOR
};


void code_write();
#endif
