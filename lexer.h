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
    unsigned int id;
    unsigned int assoc;
    unsigned int prec;
    unsigned int flags;
} oper_info;

static const oper_info operators[] = {
    { "++",    1, ASSOC_LEFT,  16, OP_SUFFIX},
    { "--",    2, ASSOC_LEFT,  16, OP_SUFFIX},

    { ".",    10, ASSOC_LEFT,  15, 0 },

    { "!",    21, ASSOC_RIGHT, 14, 0 },
    { "~",    22, ASSOC_RIGHT, 14, 0 },
    { "+",    23, ASSOC_RIGHT, 14, OP_PREFIX },
    { "-",    24, ASSOC_RIGHT, 14, OP_PREFIX },
    { "++",   25, ASSOC_RIGHT, 14, OP_PREFIX },
    { "--",   26, ASSOC_RIGHT, 14, OP_PREFIX },
/*  { "&",    27, ASSOC_RIGHT, 14, OP_PREFIX }, */

    { "*",    30, ASSOC_LEFT,  13, 0 },
    { "/",    31, ASSOC_LEFT,  13, 0 },
    { "%",    32, ASSOC_LEFT,  13, 0 },

    { "+",    40, ASSOC_LEFT,  12, 0 },
    { "-",    41, ASSOC_LEFT,  12, 0 },

    { "<<",   50, ASSOC_LEFT,  11, 0 },
    { ">>",   51, ASSOC_LEFT,  11, 0 },

    { "<",    60, ASSOC_LEFT,  10, 0 },
    { ">",    61, ASSOC_LEFT,  10, 0 },
    { "<=",   62, ASSOC_LEFT,  10, 0 },
    { ">=",   63, ASSOC_LEFT,  10, 0 },

    { "==",   70, ASSOC_LEFT,  9,  0 },
    { "!=",   71, ASSOC_LEFT,  9,  0 },

    { "&",    80, ASSOC_LEFT,  8,  0 },

    { "^",    90, ASSOC_LEFT,  7,  0 },

    { "|",   100, ASSOC_LEFT,  6,  0 },

    { "&&",  110, ASSOC_LEFT,  5,  0 },

    { "||",  120, ASSOC_LEFT,  4,  0 },

    { "?",   130, ASSOC_RIGHT, 3,  0 },

    { "=",   140, ASSOC_RIGHT, 2,  0 },
    { "+=",  141, ASSOC_RIGHT, 2,  0 },
    { "-=",  142, ASSOC_RIGHT, 2,  0 },
    { "*=",  143, ASSOC_RIGHT, 2,  0 },
    { "/=",  144, ASSOC_RIGHT, 2,  0 },
    { "%=",  145, ASSOC_RIGHT, 2,  0 },
    { ">>=", 146, ASSOC_RIGHT, 2,  0 },
    { "<<=", 147, ASSOC_RIGHT, 2,  0 },
    { "&=",  148, ASSOC_RIGHT, 2,  0 },
    { "^=",  149, ASSOC_RIGHT, 2,  0 },
    { "|=",  150, ASSOC_RIGHT, 2,  0 },
};

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
