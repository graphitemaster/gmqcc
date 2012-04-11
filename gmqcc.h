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
 * Instructions 
 * These are the external instructions supported by the interperter
 * this is what things compile to (from the C code).
 */
#define INSTR_DONE      0
// math
#define INSTR_MUL_F     1
#define INSTR_MUL_V     2
#define INSTR_MUL_FV    3
#define INSTR_MUL_VF    4
#define INSTR_DIV_F     5
#define INSTR_ADD_F     6
#define INSTR_ADD_V     7
#define INSTR_SUB_F     8
#define INSTR_SUB_V     9
// compare
#define INSTR_EQ_F      10
#define INSTR_EQ_V      11
#define INSTR_EQ_S      12
#define INSTR_EQ_E      13
#define INSTR_EQ_FNC    14
#define INSTR_NE_F      15
#define INSTR_NE_V      16
#define INSTR_NE_S      17
#define INSTR_NE_E      18
#define INSTR_NE_FNC    19
// multi compare
#define INSTR_LE        20
#define INSTR_GE        21
#define INSTR_LT        22
#define INSTR_GT        23
// load and store
#define INSTR_LOAD_F    24
#define INSTR_LOAD_V    25
#define INSTR_LOAD_S    26
#define INSTR_LOAD_ENT  27
#define INSTR_LOAD_FLD  28
#define INSTR_LOAD_FNC  29
#define INSTR_STORE_F   31
#define INSTR_STORE_V   32
#define INSTR_STORE_S   33
#define INSTR_STORE_ENT 34
#define INSTR_STORE_FLD 35
#define INSTR_STORE_FNC 36
// others
#define INSTR_ADDRESS   30
#define INSTR_RETURN    37
#define INSTR_NOT_F     38
#define INSTR_NOT_V     39
#define INSTR_NOT_S     40
#define INSTR_NOT_ENT   41
#define INSTR_NOT_FNC   42
#define INSTR_IF        43
#define INSTR_IFNOT     44
#define INSTR_CALL0     45
#define INSTR_CALL1     46
#define INSTR_CALL2     47
#define INSTR_CALL3     48
#define INSTR_CALL4     49
#define INSTR_CALL5     50
#define INSTR_CALL6     51
#define INSTR_CALL7     52
#define INSTR_CALL8     53
#define INSTR_STATE     54
#define INSTR_GOTO      55
#define INSTR_AND       56
#define INSTR_OR        57
#define INSTR_BITAND    59
#define INSTR_BITOR     60
#endif
