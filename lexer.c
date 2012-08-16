#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "gmqcc.h"
#include "lexer.h"

MEM_VEC_FUNCTIONS(token, char, value)

void lexerror(lex_file *lex, const char *fmt, ...)
{
	va_list ap;

	if (lex)
		printf("error %s:%lu: ", lex->name, (unsigned long)lex->sline);
	else
		printf("error: ");

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	printf("\n");
}

void lexwarn(lex_file *lex, int warn, const char *fmt, ...)
{
	va_list ap;

	if (!OPTS_WARN(warn))
	    return;

	if (lex)
		printf("warning %s:%lu: ", lex->name, (unsigned long)lex->sline);
	else
		printf("warning: ");

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	printf("\n");
}

token* token_new()
{
	token *tok = (token*)mem_a(sizeof(token));
	if (!tok)
		return NULL;
	memset(tok, 0, sizeof(*tok));
	return tok;
}

void token_delete(token *self)
{
	if (self->next && self->next->prev == self)
		self->next->prev = self->prev;
	if (self->prev && self->prev->next == self)
		self->prev->next = self->next;
	MEM_VECTOR_CLEAR(self, value);
	mem_d(self);
}

token* token_copy(const token *cp)
{
	token* self = token_new();
	if (!self)
		return NULL;
	/* copy the value */
	self->value_alloc = cp->value_count + 1;
	self->value_count = cp->value_count;
	self->value = (char*)mem_a(self->value_alloc);
	if (!self->value) {
		mem_d(self);
		return NULL;
	}
	memcpy(self->value, cp->value, cp->value_count);
	self->value[self->value_alloc-1] = 0;

	/* rest */
	self->ctx = cp->ctx;
	self->ttype = cp->ttype;
	memcpy(&self->constval, &cp->constval, sizeof(self->constval));
	return self;
}

void token_delete_all(token *t)
{
	token *n;

	do {
		n = t->next;
		token_delete(t);
		t = n;
	} while(t);
}

token* token_copy_all(const token *cp)
{
	token *cur;
	token *out;

	out = cur = token_copy(cp);
	if (!out)
		return NULL;

	while (cp->next) {
		cp = cp->next;
		cur->next = token_copy(cp);
		if (!cur->next) {
			token_delete_all(out);
			return NULL;
		}
		cur->next->prev = cur;
		cur = cur->next;
	}

	return out;
}

lex_file* lex_open(const char *file)
{
	lex_file *lex;
	FILE *in = util_fopen(file, "rb");

	if (!in) {
		lexerror(NULL, "open failed: '%s'\n", file);
		return NULL;
	}

	lex = (lex_file*)mem_a(sizeof(*lex));
	if (!lex) {
		fclose(in);
		lexerror(NULL, "out of memory\n");
		return NULL;
	}

	memset(lex, 0, sizeof(*lex));

	lex->file = in;
	lex->name = util_strdup(file);
	lex->line = 1; /* we start counting at 1 */

	lex->peekpos = 0;

	return lex;
}

void lex_close(lex_file *lex)
{
	if (lex->file)
		fclose(lex->file);
	if (lex->tok)
		token_delete(lex->tok);
	mem_d(lex->name);
	mem_d(lex);
}

/* Get or put-back data
 * The following to functions do NOT understand what kind of data they
 * are working on.
 * The are merely wrapping get/put in order to count line numbers.
 */
static int lex_getch(lex_file *lex)
{
	int ch;

	if (lex->peekpos) {
		lex->peekpos--;
		if (lex->peek[lex->peekpos] == '\n')
			lex->line++;
		return lex->peek[lex->peekpos];
	}

	ch = fgetc(lex->file);
	if (ch == '\n')
		lex->line++;
	return ch;
}

static void lex_ungetch(lex_file *lex, int ch)
{
	lex->peek[lex->peekpos++] = ch;
	if (ch == '\n')
		lex->line--;
}

/* classify characters
 * some additions to the is*() functions of ctype.h
 */

/* Idents are alphanumberic, but they start with alpha or _ */
static bool isident_start(int ch)
{
	return isalpha(ch) || ch == '_';
}

static bool isident(int ch)
{
	return isident_start(ch) || isdigit(ch);
}

/* isxdigit_only is used when we already know it's not a digit
 * and want to see if it's a hex digit anyway.
 */
static bool isxdigit_only(int ch)
{
	return (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

/* Skip whitespace and comments and return the first
 * non-white character.
 * As this makes use of the above getch() ungetch() functions,
 * we don't need to care at all about line numbering anymore.
 *
 * In theory, this function should only be used at the beginning
 * of lexing, or when we *know* the next character is part of the token.
 * Otherwise, if the parser throws an error, the linenumber may not be
 * the line of the error, but the line of the next token AFTER the error.
 *
 * This is currently only problematic when using c-like string-continuation,
 * since comments and whitespaces are allowed between 2 such strings.
 * Example:
printf(   "line one\n"
// A comment
          "A continuation of the previous string"
// This line is skipped
      , foo);

 * In this case, if the parse decides it didn't actually want a string,
 * and uses lex->line to print an error, it will show the ', foo);' line's
 * linenumber.
 *
 * On the other hand, the parser is supposed to remember the line of the next
 * token's beginning. In this case we would want skipwhite() to be called
 * AFTER reading a token, so that the parser, before reading the NEXT token,
 * doesn't store teh *comment's* linenumber, but the actual token's linenumber.
 *
 * THIS SOLUTION
 *    here is to store the line of the first character after skipping
 *    the initial whitespace in lex->sline, this happens in lex_do.
 */
static int lex_skipwhite(lex_file *lex)
{
	int ch = 0;

	do
	{
		ch = lex_getch(lex);
		while (ch != EOF && isspace(ch)) ch = lex_getch(lex);

		if (ch == '/') {
			ch = lex_getch(lex);
			if (ch == '/')
			{
				/* one line comment */
				ch = lex_getch(lex);

				/* check for special: '/', '/', '*', '/' */
				if (ch == '*') {
					ch = lex_getch(lex);
					if (ch == '/') {
						ch = ' ';
						continue;
					}
				}

				while (ch != EOF && ch != '\n') {
					ch = lex_getch(lex);
				}
				continue;
			}
			if (ch == '*')
			{
				/* multiline comment */
				while (ch != EOF)
				{
					ch = lex_getch(lex);
					if (ch == '*') {
						ch = lex_getch(lex);
						if (ch == '/') {
							ch = lex_getch(lex);
							break;
						}
					}
				}
				if (ch == '/') /* allow *//* direct following comment */
				{
					lex_ungetch(lex, ch);
					ch = ' '; /* cause TRUE in the isspace check */
				}
				continue;
			}
			/* Otherwise roll back to the slash and break out of the loop */
			lex_ungetch(lex, ch);
			ch = '/';
			break;
		}
	} while (ch != EOF && isspace(ch));

	return ch;
}

/* Append a character to the token buffer */
static bool GMQCC_WARN lex_tokench(lex_file *lex, int ch)
{
	if (!token_value_add(lex->tok, ch)) {
		lexerror(lex, "out of memory");
		return false;
	}
	return true;
}

/* Append a trailing null-byte */
static bool GMQCC_WARN lex_endtoken(lex_file *lex)
{
	if (!token_value_add(lex->tok, 0)) {
		lexerror(lex, "out of memory");
		return false;
	}
	lex->tok->value_count--;
	return true;
}

/* Get a token */
static bool GMQCC_WARN lex_finish_ident(lex_file *lex)
{
	int ch;

	ch = lex_getch(lex);
	while (ch != EOF && isident(ch))
	{
		if (!lex_tokench(lex, ch))
			return (lex->tok->ttype = TOKEN_FATAL);
		ch = lex_getch(lex);
	}

	/* last ch was not an ident ch: */
	lex_ungetch(lex, ch);

	return true;
}

static int GMQCC_WARN lex_finish_string(lex_file *lex, int quote)
{
	int ch = 0;

	while (ch != EOF)
	{
		ch = lex_getch(lex);
		if (ch == quote)
			return TOKEN_STRINGCONST;

		if (ch == '\\') {
			ch = lex_getch(lex);
			if (ch == EOF) {
				lexerror(lex, "unexpected end of file");
				lex_ungetch(lex, EOF); /* next token to be TOKEN_EOF */
				return (lex->tok->ttype = TOKEN_ERROR);
			}

            switch (ch) {
            case '\\': break;
            case 'a':  ch = '\a'; break;
            case 'b':  ch = '\b'; break;
            case 'r':  ch = '\r'; break;
            case 'n':  ch = '\n'; break;
            case 't':  ch = '\t'; break;
            case 'f':  ch = '\f'; break;
            case 'v':  ch = '\v'; break;
            default:
                lexwarn(lex, WARN_UNKNOWN_CONTROL_SEQUENCE, "unrecognized control sequence: \\%c", ch);
			    /* so we just add the character plus backslash no matter what it actually is */
			    if (!lex_tokench(lex, '\\'))
				    return (lex->tok->ttype = TOKEN_FATAL);
            }
            /* add the character finally */
			if (!lex_tokench(lex, ch))
				return (lex->tok->ttype = TOKEN_FATAL);
		}
		else if (!lex_tokench(lex, ch))
			return (lex->tok->ttype = TOKEN_FATAL);
	}
	lexerror(lex, "unexpected end of file within string constant");
	lex_ungetch(lex, EOF); /* next token to be TOKEN_EOF */
	return (lex->tok->ttype = TOKEN_ERROR);
}

static int GMQCC_WARN lex_finish_digit(lex_file *lex, int lastch)
{
	bool ishex = false;

	int  ch = lastch;

	/* parse a number... */
	lex->tok->ttype = TOKEN_INTCONST;

	if (!lex_tokench(lex, ch))
		return (lex->tok->ttype = TOKEN_FATAL);

	ch = lex_getch(lex);
	if (ch != '.' && !isdigit(ch))
	{
		if (lastch != '0' || ch != 'x')
		{
			/* end of the number or EOF */
			lex_ungetch(lex, ch);
			if (!lex_endtoken(lex))
				return (lex->tok->ttype = TOKEN_FATAL);

			lex->tok->constval.i = lastch - '0';
			return lex->tok->ttype;
		}

		ishex = true;
	}

	/* EOF would have been caught above */

	if (ch != '.')
	{
		if (!lex_tokench(lex, ch))
			return (lex->tok->ttype = TOKEN_FATAL);
		ch = lex_getch(lex);
		while (isdigit(ch) || (ishex && isxdigit_only(ch)))
		{
			if (!lex_tokench(lex, ch))
				return (lex->tok->ttype = TOKEN_FATAL);
			ch = lex_getch(lex);
		}
	}
	/* NOT else, '.' can come from above as well */
	if (ch == '.' && !ishex)
	{
		/* Allow floating comma in non-hex mode */
		lex->tok->ttype = TOKEN_FLOATCONST;
		if (!lex_tokench(lex, ch))
			return (lex->tok->ttype = TOKEN_FATAL);

		/* continue digits-only */
		ch = lex_getch(lex);
		while (isdigit(ch))
		{
			if (!lex_tokench(lex, ch))
				return (lex->tok->ttype = TOKEN_FATAL);
			ch = lex_getch(lex);
		}
	}
	/* put back the last character */
	/* but do not put back the trailing 'f' or a float */
	if (lex->tok->ttype == TOKEN_FLOATCONST && ch == 'f')
		ch = lex_getch(lex);

	/* generally we don't want words to follow numbers: */
	if (isident(ch)) {
		lexerror(lex, "unexpected trailing characters after number");
		return (lex->tok->ttype = TOKEN_ERROR);
	}
	lex_ungetch(lex, ch);

	if (!lex_endtoken(lex))
		return (lex->tok->ttype = TOKEN_FATAL);
	if (lex->tok->ttype == TOKEN_FLOATCONST)
		lex->tok->constval.f = strtod(lex->tok->value, NULL);
	else
		lex->tok->constval.i = strtol(lex->tok->value, NULL, 0);
	return lex->tok->ttype;
}

int lex_do(lex_file *lex)
{
	int ch, nextch;

	if (lex->tok)
		token_delete(lex->tok);
	lex->tok = token_new();
	if (!lex->tok)
		return TOKEN_FATAL;

	ch = lex_skipwhite(lex);
	lex->sline = lex->line;
	lex->tok->ctx.line = lex->sline;
	lex->tok->ctx.file = lex->name;

	if (ch == EOF)
		return (lex->tok->ttype = TOKEN_EOF);

	/* single-character tokens */
	switch (ch)
	{
		case ';':
		case '(':
		case ')':
		case '{':
		case '}':
		case '[':
		case ']':

		case '#':
	        if (!lex_tokench(lex, ch) ||
	            !lex_endtoken(lex))
	        {
	            return (lex->tok->ttype = TOKEN_FATAL);
	        }
			return (lex->tok->ttype = ch);
		default:
			break;
	}

	if (lex->flags.noops)
	{
		/* Detect characters early which are normally
		 * operators OR PART of an operator.
		 */
		switch (ch)
		{
			case '+':
			case '-':
			case '*':
			case '/':
			case '<':
			case '>':
			case '=':
			case '&':
			case '|':
			case '^':
			case '~':
			case ',':
		    case '.':
	            if (!lex_tokench(lex, ch) ||
	                !lex_endtoken(lex))
	            {
	                return (lex->tok->ttype = TOKEN_FATAL);
	            }
				return (lex->tok->ttype = ch);
			default:
				break;
		}
	}

	if (ch == ',' || ch == '.') {
	    if (!lex_tokench(lex, ch) ||
	        !lex_endtoken(lex))
	    {
	        return (lex->tok->ttype = TOKEN_FATAL);
	    }
	    return (lex->tok->ttype = TOKEN_OPERATOR);
	}

	if (ch == '+' || ch == '-' || /* ++, --, +=, -=  and -> as well! */
	    ch == '>' || ch == '<' || /* <<, >>, <=, >= */
	    ch == '=' ||              /* == */
	    ch == '&' || ch == '|')   /* &&, ||, &=, |= */
	{
		if (!lex_tokench(lex, ch))
			return (lex->tok->ttype = TOKEN_FATAL);

		nextch = lex_getch(lex);
		if (nextch == ch || nextch == '=') {
			if (!lex_tokench(lex, nextch))
				return (lex->tok->ttype = TOKEN_FATAL);
		} else if (ch == '-' && nextch == '>') {
			if (!lex_tokench(lex, nextch))
				return (lex->tok->ttype = TOKEN_FATAL);
		} else
			lex_ungetch(lex, nextch);

		if (!lex_endtoken(lex))
			return (lex->tok->ttype = TOKEN_FATAL);
		return (lex->tok->ttype = TOKEN_OPERATOR);
	}

	if (ch == '^' || ch == '~' || ch == '!')
	{
		if (!lex_tokench(lex, ch) ||
			!lex_endtoken(lex))
		{
			return (lex->tok->ttype = TOKEN_FATAL);
		}
		return (lex->tok->ttype = TOKEN_OPERATOR);
	}

	if (ch == '*' || ch == '/') /* *=, /= */
	{
		if (!lex_tokench(lex, ch))
			return (lex->tok->ttype = TOKEN_FATAL);

		nextch = lex_getch(lex);
		if (nextch == '=') {
			if (!lex_tokench(lex, nextch))
				return (lex->tok->ttype = TOKEN_FATAL);
		} else
			lex_ungetch(lex, nextch);

		if (!lex_endtoken(lex))
			return (lex->tok->ttype = TOKEN_FATAL);
		return (lex->tok->ttype = TOKEN_OPERATOR);
	}

	if (isident_start(ch))
	{
		const char *v;
		if (!lex_tokench(lex, ch))
			return (lex->tok->ttype = TOKEN_FATAL);
		if (!lex_finish_ident(lex)) {
			/* error? */
			return (lex->tok->ttype = TOKEN_ERROR);
		}
		if (!lex_endtoken(lex))
			return (lex->tok->ttype = TOKEN_FATAL);
		lex->tok->ttype = TOKEN_IDENT;

		v = lex->tok->value;
		if (!strcmp(v, "void")) {
			lex->tok->ttype = TOKEN_TYPENAME;
		    lex->tok->constval.t = TYPE_VOID;
		} else if (!strcmp(v, "int")) {
			lex->tok->ttype = TOKEN_TYPENAME;
		    lex->tok->constval.t = TYPE_INTEGER;
		} else if (!strcmp(v, "float")) {
			lex->tok->ttype = TOKEN_TYPENAME;
		    lex->tok->constval.t = TYPE_FLOAT;
		} else if (!strcmp(v, "string")) {
			lex->tok->ttype = TOKEN_TYPENAME;
		    lex->tok->constval.t = TYPE_STRING;
		} else if (!strcmp(v, "entity")) {
			lex->tok->ttype = TOKEN_TYPENAME;
		    lex->tok->constval.t = TYPE_ENTITY;
		} else if (!strcmp(v, "vector")) {
			lex->tok->ttype = TOKEN_TYPENAME;
		    lex->tok->constval.t = TYPE_VECTOR;
		} else if (!strcmp(v, "for")  ||
		         !strcmp(v, "while")  ||
		         !strcmp(v, "do")     ||
		         !strcmp(v, "if")     ||
		         !strcmp(v, "else")   ||
		         !strcmp(v, "var")    ||
		         !strcmp(v, "local")  ||
		         !strcmp(v, "return") ||
		         !strcmp(v, "const"))
			lex->tok->ttype = TOKEN_KEYWORD;

		return lex->tok->ttype;
	}

	if (ch == '"')
	{
		lex->tok->ttype = lex_finish_string(lex, '"');
		while (lex->tok->ttype == TOKEN_STRINGCONST)
		{
			/* Allow c style "string" "continuation" */
			ch = lex_skipwhite(lex);
			if (ch != '"') {
				lex_ungetch(lex, ch);
				break;
			}

			lex->tok->ttype = lex_finish_string(lex, '"');
		}
		if (!lex_endtoken(lex))
			return (lex->tok->ttype = TOKEN_FATAL);
		return lex->tok->ttype;
	}

	if (ch == '\'')
	{
		/* we parse character constants like string,
		 * but return TOKEN_CHARCONST, or a vector type if it fits...
		 * Likewise actual unescaping has to be done by the parser.
		 * The difference is we don't allow 'char' 'continuation'.
		 */
		 lex->tok->ttype = lex_finish_string(lex, '\'');
		 if (!lex_endtoken(lex))
		 	 return (lex->tok->ttype = TOKEN_FATAL);

		 /* It's a vector if we can successfully scan 3 floats */
#ifdef WIN32
		 if (sscanf_s(lex->tok->value, " %f %f %f ",
		            &lex->tok->constval.v.x, &lex->tok->constval.v.y, &lex->tok->constval.v.z) == 3)
#else
		 if (sscanf(lex->tok->value, " %f %f %f ",
		            &lex->tok->constval.v.x, &lex->tok->constval.v.y, &lex->tok->constval.v.z) == 3)
#endif
		 {
		 	 lex->tok->ttype = TOKEN_VECTORCONST;
		 }

		 return lex->tok->ttype;
	}

	if (isdigit(ch))
	{
		lex->tok->ttype = lex_finish_digit(lex, ch);
		if (!lex_endtoken(lex))
			return (lex->tok->ttype = TOKEN_FATAL);
		return lex->tok->ttype;
	}

	lexerror(lex, "unknown token");
	return (lex->tok->ttype = TOKEN_ERROR);
}
