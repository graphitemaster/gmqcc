/*
 * Copyright (C) 2012, 2013
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
#ifndef GMQCC_LEXER_HDR
#define GMQCC_LEXER_HDR

typedef struct token_s token;

struct token_s {
    int ttype;

    char *value;

    union {
        vector v;
        int    i;
        double f;
        int    t; /* type */
    } constval;

#if 0
    struct token_s *next;
    struct token_s *prev;
#endif

    lex_ctx ctx;
};

#if 0
token* token_new();
void   token_delete(token*);
token* token_copy(const token *cp);
void   token_delete_all(token *t);
token* token_copy_all(const token *cp);
#endif

/* Lexer
 *
 */
enum {
    /* Other tokens which we can return: */
    TOKEN_NONE = 0,
    TOKEN_START = 128,

    TOKEN_IDENT,

    TOKEN_TYPENAME,

    TOKEN_OPERATOR,

    TOKEN_KEYWORD, /* loop */

    TOKEN_DOTS, /* 3 dots, ... */

    TOKEN_ATTRIBUTE_OPEN,  /* [[ */
    TOKEN_ATTRIBUTE_CLOSE, /* ]] */

    TOKEN_VA_ARGS, /* for the ftepp only */
    TOKEN_VA_ARGS_ARRAY, /* for the ftepp only */

    TOKEN_STRINGCONST, /* not the typename but an actual "string" */
    TOKEN_CHARCONST,
    TOKEN_VECTORCONST,
    TOKEN_INTCONST,
    TOKEN_FLOATCONST,

    TOKEN_WHITE,
    TOKEN_EOL,

    TOKEN_EOF,

    /* We use '< TOKEN_ERROR', so TOKEN_FATAL must come after it and any
     * other error related tokens as well
     */
    TOKEN_ERROR,
    TOKEN_FATAL /* internal error, eg out of memory */
};

typedef struct {
    char *name;
    int   value;
} frame_macro;

typedef struct lex_file_s {
    FILE   *file;
    const char *open_string;
    size_t      open_string_length;
    size_t      open_string_pos;

    char   *name;
    size_t  line;
    size_t  sline; /* line at the start of a token */

    int     peek[256];
    size_t  peekpos;

    bool    eof;

    token   tok; /* not a pointer anymore */

    struct {
        bool noops;
        bool nodigraphs; /* used when lexing string constants */
        bool preprocessing; /* whitespace and EOLs become actual tokens */
        bool mergelines; /* backslash at the end of a line escapes the newline */
    } flags;

    int framevalue;
    frame_macro *frames;
    char *modelname;

    size_t push_line;
} lex_file;

lex_file* lex_open (const char *file);
lex_file* lex_open_string(const char *str, size_t len, const char *name);
void      lex_close(lex_file   *lex);
int       lex_do   (lex_file   *lex);
void      lex_cleanup(void);

/* Parser
 *
 */

enum {
    ASSOC_LEFT,
    ASSOC_RIGHT
};

#define OP_SUFFIX 1
#define OP_PREFIX 2

typedef struct {
    const char   *op;
    unsigned int operands;
    unsigned int id;
    unsigned int assoc;
    signed int   prec;
    unsigned int flags;
} oper_info;

#define opid1(a) (a)
#define opid2(a,b) ((a<<8)|b)
#define opid3(a,b,c) ((a<<16)|(b<<8)|c)

static const oper_info c_operators[] = {
    { "(",   0, opid1('('),         ASSOC_LEFT,  99, OP_PREFIX}, /* paren expression - non function call */

    { "++",  1, opid3('S','+','+'), ASSOC_LEFT,  15, OP_SUFFIX},
    { "--",  1, opid3('S','-','-'), ASSOC_LEFT,  15, OP_SUFFIX},
    { ".",   2, opid1('.'),         ASSOC_LEFT,  15, 0 },
    { "(",   0, opid1('('),         ASSOC_LEFT,  15, 0 }, /* function call */
    { "[",   2, opid1('['),         ASSOC_LEFT,  15, 0 }, /* array subscript */

    { "!",   1, opid2('!', 'P'),    ASSOC_RIGHT, 14, OP_PREFIX },
    { "~",   1, opid2('~', 'P'),    ASSOC_RIGHT, 14, OP_PREFIX },
    { "+",   1, opid2('+','P'),     ASSOC_RIGHT, 14, OP_PREFIX },
    { "-",   1, opid2('-','P'),     ASSOC_RIGHT, 14, OP_PREFIX },
    { "++",  1, opid3('+','+','P'), ASSOC_RIGHT, 14, OP_PREFIX },
    { "--",  1, opid3('-','-','P'), ASSOC_RIGHT, 14, OP_PREFIX },
/*  { "&",   1, opid2('&','P'),     ASSOC_RIGHT, 14, OP_PREFIX }, */

    { "*",   2, opid1('*'),         ASSOC_LEFT,  13, 0 },
    { "/",   2, opid1('/'),         ASSOC_LEFT,  13, 0 },
    { "%",   2, opid1('%'),         ASSOC_LEFT,  13, 0 },

    { "+",   2, opid1('+'),         ASSOC_LEFT,  12, 0 },
    { "-",   2, opid1('-'),         ASSOC_LEFT,  12, 0 },

    { "<<",  2, opid2('<','<'),     ASSOC_LEFT,  11, 0 },
    { ">>",  2, opid2('>','>'),     ASSOC_LEFT,  11, 0 },

    { "<",   2, opid1('<'),         ASSOC_LEFT,  10, 0 },
    { ">",   2, opid1('>'),         ASSOC_LEFT,  10, 0 },
    { "<=",  2, opid2('<','='),     ASSOC_LEFT,  10, 0 },
    { ">=",  2, opid2('>','='),     ASSOC_LEFT,  10, 0 },

    { "==",  2, opid2('=','='),     ASSOC_LEFT,  9,  0 },
    { "!=",  2, opid2('!','='),     ASSOC_LEFT,  9,  0 },

    { "&",   2, opid1('&'),         ASSOC_LEFT,  8,  0 },

    { "^",   2, opid1('^'),         ASSOC_LEFT,  7,  0 },

    { "|",   2, opid1('|'),         ASSOC_LEFT,  6,  0 },

    { "&&",  2, opid2('&','&'),     ASSOC_LEFT,  5,  0 },

    { "||",  2, opid2('|','|'),     ASSOC_LEFT,  4,  0 },

    { "?",   3, opid2('?',':'),     ASSOC_RIGHT, 3,  0 },

    { "=",   2, opid1('='),         ASSOC_RIGHT, 2,  0 },
    { "+=",  2, opid2('+','='),     ASSOC_RIGHT, 2,  0 },
    { "-=",  2, opid2('-','='),     ASSOC_RIGHT, 2,  0 },
    { "*=",  2, opid2('*','='),     ASSOC_RIGHT, 2,  0 },
    { "/=",  2, opid2('/','='),     ASSOC_RIGHT, 2,  0 },
    { "%=",  2, opid2('%','='),     ASSOC_RIGHT, 2,  0 },
    { ">>=", 2, opid3('>','>','='), ASSOC_RIGHT, 2,  0 },
    { "<<=", 2, opid3('<','<','='), ASSOC_RIGHT, 2,  0 },
    { "&=",  2, opid2('&','='),     ASSOC_RIGHT, 2,  0 },
    { "^=",  2, opid2('^','='),     ASSOC_RIGHT, 2,  0 },
    { "|=",  2, opid2('|','='),     ASSOC_RIGHT, 2,  0 },
    { "&~=", 2, opid3('&','~','='), ASSOC_RIGHT, 2,  0 },

    { ":",   0, opid2(':','?'),     ASSOC_RIGHT, 1,  0 },

    { ",",   2, opid1(','),         ASSOC_LEFT,  0,  0 }
};
static const size_t c_operator_count = (sizeof(c_operators) / sizeof(c_operators[0]));

static const oper_info fte_operators[] = {
    { "(",   0, opid1('('),         ASSOC_LEFT,  99, OP_PREFIX}, /* paren expression - non function call */

    { "++",  1, opid3('S','+','+'), ASSOC_LEFT,  15, OP_SUFFIX},
    { "--",  1, opid3('S','-','-'), ASSOC_LEFT,  15, OP_SUFFIX},
    { ".",   2, opid1('.'),         ASSOC_LEFT,  15, 0 },
    { "(",   0, opid1('('),         ASSOC_LEFT,  15, 0 }, /* function call */
    { "[",   2, opid1('['),         ASSOC_LEFT,  15, 0 }, /* array subscript */

    { "!",   1, opid2('!', 'P'),    ASSOC_RIGHT, 14, OP_PREFIX },
    { "+",   1, opid2('+','P'),     ASSOC_RIGHT, 14, OP_PREFIX },
    { "-",   1, opid2('-','P'),     ASSOC_RIGHT, 14, OP_PREFIX },
    { "++",  1, opid3('+','+','P'), ASSOC_RIGHT, 14, OP_PREFIX },
    { "--",  1, opid3('-','-','P'), ASSOC_RIGHT, 14, OP_PREFIX },

    { "*",   2, opid1('*'),         ASSOC_LEFT,  13, 0 },
    { "/",   2, opid1('/'),         ASSOC_LEFT,  13, 0 },
    { "&",   2, opid1('&'),         ASSOC_LEFT,  13, 0 },
    { "|",   2, opid1('|'),         ASSOC_LEFT,  13, 0 },

    { "+",   2, opid1('+'),         ASSOC_LEFT,  12, 0 },
    { "-",   2, opid1('-'),         ASSOC_LEFT,  12, 0 },

    { "<<",  2, opid2('<','<'),     ASSOC_LEFT,  11, 0 },
    { ">>",  2, opid2('>','>'),     ASSOC_LEFT,  11, 0 },

    { "<",   2, opid1('<'),         ASSOC_LEFT,  10, 0 },
    { ">",   2, opid1('>'),         ASSOC_LEFT,  10, 0 },
    { "<=",  2, opid2('<','='),     ASSOC_LEFT,  10, 0 },
    { ">=",  2, opid2('>','='),     ASSOC_LEFT,  10, 0 },
    { "==",  2, opid2('=','='),     ASSOC_LEFT,  10,  0 },
    { "!=",  2, opid2('!','='),     ASSOC_LEFT,  10,  0 },

    { "?",   3, opid2('?',':'),     ASSOC_RIGHT, 9,  0 },

    { "=",   2, opid1('='),         ASSOC_RIGHT, 8,  0 },
    { "+=",  2, opid2('+','='),     ASSOC_RIGHT, 8,  0 },
    { "-=",  2, opid2('-','='),     ASSOC_RIGHT, 8,  0 },
    { "*=",  2, opid2('*','='),     ASSOC_RIGHT, 8,  0 },
    { "/=",  2, opid2('/','='),     ASSOC_RIGHT, 8,  0 },
    { "%=",  2, opid2('%','='),     ASSOC_RIGHT, 8,  0 },
    { "&=",  2, opid2('&','='),     ASSOC_RIGHT, 8,  0 },
    { "|=",  2, opid2('|','='),     ASSOC_RIGHT, 8,  0 },
    { "&~=", 2, opid3('&','~','='), ASSOC_RIGHT, 8,  0 },

    { "&&",  2, opid2('&','&'),     ASSOC_LEFT,  5,  0 },
    { "||",  2, opid2('|','|'),     ASSOC_LEFT,  5,  0 },

    /* Leave precedence 3 for : with -fcorrect-ternary */
    { ",",   2, opid1(','),         ASSOC_LEFT,  2,  0 },
    { ":",   0, opid2(':','?'),     ASSOC_RIGHT, 1,  0 }
};
static const size_t fte_operator_count = (sizeof(fte_operators) / sizeof(fte_operators[0]));

static const oper_info qcc_operators[] = {
    { "(",   0, opid1('('),         ASSOC_LEFT,  99, OP_PREFIX}, /* paren expression - non function call */

    { ".",   2, opid1('.'),         ASSOC_LEFT,  15, 0 },
    { "(",   0, opid1('('),         ASSOC_LEFT,  15, 0 }, /* function call */
    { "[",   2, opid1('['),         ASSOC_LEFT,  15, 0 }, /* array subscript */

    { "!",   1, opid2('!', 'P'),    ASSOC_RIGHT, 14, OP_PREFIX },
    { "+",   1, opid2('+','P'),     ASSOC_RIGHT, 14, OP_PREFIX },
    { "-",   1, opid2('-','P'),     ASSOC_RIGHT, 14, OP_PREFIX },

    { "*",   2, opid1('*'),         ASSOC_LEFT,  13, 0 },
    { "/",   2, opid1('/'),         ASSOC_LEFT,  13, 0 },
    { "&",   2, opid1('&'),         ASSOC_LEFT,  13, 0 },
    { "|",   2, opid1('|'),         ASSOC_LEFT,  13, 0 },

    { "+",   2, opid1('+'),         ASSOC_LEFT,  12, 0 },
    { "-",   2, opid1('-'),         ASSOC_LEFT,  12, 0 },

    { "<",   2, opid1('<'),         ASSOC_LEFT,  10, 0 },
    { ">",   2, opid1('>'),         ASSOC_LEFT,  10, 0 },
    { "<=",  2, opid2('<','='),     ASSOC_LEFT,  10, 0 },
    { ">=",  2, opid2('>','='),     ASSOC_LEFT,  10, 0 },
    { "==",  2, opid2('=','='),     ASSOC_LEFT,  10,  0 },
    { "!=",  2, opid2('!','='),     ASSOC_LEFT,  10,  0 },

    { "=",   2, opid1('='),         ASSOC_RIGHT, 8,  0 },
    { "+=",  2, opid2('+','='),     ASSOC_RIGHT, 8,  0 },
    { "-=",  2, opid2('-','='),     ASSOC_RIGHT, 8,  0 },
    { "*=",  2, opid2('*','='),     ASSOC_RIGHT, 8,  0 },
    { "/=",  2, opid2('/','='),     ASSOC_RIGHT, 8,  0 },
    { "%=",  2, opid2('%','='),     ASSOC_RIGHT, 8,  0 },
    { "&=",  2, opid2('&','='),     ASSOC_RIGHT, 8,  0 },
    { "|=",  2, opid2('|','='),     ASSOC_RIGHT, 8,  0 },

    { "&&",  2, opid2('&','&'),     ASSOC_LEFT,  5,  0 },
    { "||",  2, opid2('|','|'),     ASSOC_LEFT,  5,  0 },

    { ",",   2, opid1(','),         ASSOC_LEFT,  2,  0 },
};
static const size_t qcc_operator_count = (sizeof(qcc_operators) / sizeof(qcc_operators[0]));

extern const oper_info *operators;
extern size_t           operator_count;
void lexerror(lex_file*, const char *fmt, ...);

#endif
