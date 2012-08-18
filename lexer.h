#ifndef GMQCC_LEXER_HDR_
#define GMQCC_LEXER_HDR_

typedef struct token_s token;

#include "ast.h"

struct token_s {
	int ttype;

	MEM_VECTOR_MAKE(char, value);

	union {
		vector v;
		int    i;
		double f;
		int    t; /* type */
	} constval;

	struct token_s *next;
	struct token_s *prev;

	lex_ctx ctx;
};

token* token_new();
void   token_delete(token*);
token* token_copy(const token *cp);
void   token_delete_all(token *t);
token* token_copy_all(const token *cp);

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

    TOKEN_STRINGCONST, /* not the typename but an actual "string" */
    TOKEN_CHARCONST,
    TOKEN_VECTORCONST,
    TOKEN_INTCONST,
    TOKEN_FLOATCONST,

    TOKEN_EOF,

    /* We use '< TOKEN_ERROR', so TOKEN_FATAL must come after it and any
     * other error related tokens as well
     */
    TOKEN_ERROR,
    TOKEN_FATAL /* internal error, eg out of memory */
};

static const char *_tokennames[] = {
    "TOKEN_START",
    "TOKEN_IDENT",
    "TOKEN_TYPENAME",
    "TOKEN_OPERATOR",
    "TOKEN_KEYWORD",
    "TOKEN_STRINGCONST",
    "TOKEN_CHARCONST",
    "TOKEN_VECTORCONST",
    "TOKEN_INTCONST",
    "TOKEN_FLOATCONST",
    "TOKEN_EOF",
    "TOKEN_ERROR",
    "TOKEN_FATAL",
};
typedef int
_all_tokennames_added_[
	((TOKEN_FATAL - TOKEN_START + 1) ==
	 (sizeof(_tokennames)/sizeof(_tokennames[0])))
	? 1 : -1];

typedef struct {
    char *name;
    int   value;
} frame_macro;

typedef struct {
	FILE   *file;
	char   *name;
	size_t  line;
	size_t  sline; /* line at the start of a token */

	char    peek[256];
	size_t  peekpos;

	token  *tok;

	struct {
	    bool noops;
	} flags;

    int framevalue;
	MEM_VECTOR_MAKE(frame_macro, frames);
	char *modelname;
} lex_file;

MEM_VECTOR_PROTO(lex_file, char, token);

lex_file* lex_open (const char *file);
void      lex_close(lex_file   *lex);
int       lex_do   (lex_file   *lex);

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
    unsigned int prec;
    unsigned int flags;
} oper_info;

#define opid1(a) (a)
#define opid2(a,b) ((a<<8)|b)
#define opid3(a,b,c) ((a<<16)|(b<<8)|c)

static const oper_info operators[] = {
    { "++",  1, opid3('S','+','+'), ASSOC_LEFT,  16, OP_SUFFIX},
    { "--",  1, opid3('S','-','-'), ASSOC_LEFT,  16, OP_SUFFIX},

    { ".",   2, opid1('.'),         ASSOC_LEFT,  15, 0 },
    { "(",   0, opid1('('),         ASSOC_LEFT,  15, OP_SUFFIX },

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

    { ",",   2, opid1(','),         ASSOC_LEFT,  1,  0 }
};
static const size_t operator_count = (sizeof(operators) / sizeof(operators[0]));

typedef struct
{
	lex_file *lex;
	int      error;
	lex_ctx  ctx;

	token    *tokens;
	token    *lastok;

	token    *tok; /* current token */

	MEM_VECTOR_MAKE(ast_value*, globals);
} parse_file;

MEM_VECTOR_PROTO(parse_file, ast_value*, globals);

parse_file* parse_open(const char *file);
void        parse_file_close(parse_file*);

bool        parse(parse_file*);

bool        parse_iskey(parse_file *self, const char *ident);

void lexerror(lex_file*, const char *fmt, ...);

#endif
