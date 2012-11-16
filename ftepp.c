/*
 * Copyright (C) 2012
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
#include "gmqcc.h"
#include "lexer.h"

typedef struct {
	bool on;
	bool was_on;
	bool had_else;
} ppcondition;

typedef struct {
	int   token;
	char *value;
	/* a copy from the lexer */
	union {
		vector v;
		int    i;
		double f;
		int    t; /* type */
	} constval;
} pptoken;

typedef struct {
	lex_ctx ctx;

	char   *name;
	char  **params;
	/* yes we need an extra flag since `#define FOO x` is not the same as `#define FOO() x` */
	bool    has_params;

	pptoken **output;
} ppmacro;

typedef struct {
	lex_file    *lex;
	int          token;
	bool         newline;
	unsigned int errors;

	ppcondition *conditions;
	ppmacro    **macros;
} ftepp_t;

#define ftepp_tokval(f) ((f)->lex->tok.value)
#define ftepp_ctx(f)    ((f)->lex->tok.ctx)

static void ftepp_errorat(ftepp_t *ftepp, lex_ctx ctx, const char *fmt, ...)
{
	va_list ap;

	ftepp->errors++;

	va_start(ap, fmt);
    con_vprintmsg(LVL_ERROR, ctx.file, ctx.line, "error", fmt, ap);
	va_end(ap);
}

static void ftepp_error(ftepp_t *ftepp, const char *fmt, ...)
{
	va_list ap;

	ftepp->errors++;

	va_start(ap, fmt);
    con_vprintmsg(LVL_ERROR, ftepp->lex->tok.ctx.file, ftepp->lex->tok.ctx.line, "error", fmt, ap);
	va_end(ap);
}

ppmacro *ppmacro_new(lex_ctx ctx, const char *name)
{
	ppmacro *macro = (ppmacro*)mem_a(sizeof(ppmacro));
	memset(macro, 0, sizeof(*macro));
	macro->name = util_strdup(name);
	return macro;
}

void ppmacro_delete(ppmacro *self)
{
	vec_free(self->params);
	vec_free(self->output);
	mem_d(self->name);
	mem_d(self);
}

ftepp_t* ftepp_init()
{
	ftepp_t *ftepp;

	ftepp = (ftepp_t*)mem_a(sizeof(*ftepp));
	memset(ftepp, 0, sizeof(*ftepp));

	return ftepp;
}

void ftepp_delete(ftepp_t *self)
{
	vec_free(self->macros);
	vec_free(self->conditions);
	mem_d(self);
}

ppmacro* ftepp_macro_find(ftepp_t *ftepp, const char *name)
{
	size_t i;
	for (i = 0; i < vec_size(ftepp->macros); ++i) {
		if (!strcmp(name, ftepp->macros[i]->name))
			return ftepp->macros[i];
	}
	return NULL;
}

static inline int ftepp_next(ftepp_t *ftepp)
{
	return (ftepp->token = lex_do(ftepp->lex));
}

/* Important: this does not skip newlines! */
static bool ftepp_skipspace(ftepp_t *ftepp)
{
	if (ftepp->token != TOKEN_WHITE)
		return true;
	while (ftepp_next(ftepp) == TOKEN_WHITE) {}
	if (ftepp->token >= TOKEN_EOF) {
		ftepp_error(ftepp, "unexpected end of preprocessor directive");
		return false;
	}
	return true;
}

/**
 * The huge macro parsing code...
 */
static bool ftepp_define(ftepp_t *ftepp)
{
	ppmacro *macro;
	(void)ftepp_next(ftepp);
	if (!ftepp_skipspace(ftepp))
		return false;

	switch (ftepp->token) {
		case TOKEN_IDENT:
		case TOKEN_TYPENAME:
		case TOKEN_KEYWORD:
			macro = ppmacro_new(ftepp_ctx(ftepp), ftepp_tokval(ftepp));
			break;
		default:
			ftepp_error(ftepp, "expected macro name");
			return false;
	}

	(void)ftepp_next(ftepp);
	if (!ftepp_skipspace(ftepp))
		return false;
	if (ftepp->token != TOKEN_EOL) {
		ftepp_error(ftepp, "stray tokens after macro");
		return false;
	}
	vec_push(ftepp->macros, macro);
	return true;
}

/**
 * #if - the FTEQCC way:
 *    defined(FOO) => true if FOO was #defined regardless of parameters or contents
 *    <numbers>    => True if the number is not 0
 *    !<factor>    => True if the factor yields false
 *    <macro>      => becomes the macro's FIRST token regardless of parameters
 *    <e> && <e>   => True if both expressions are true
 *    <e> || <e>   => True if either expression is true
 *    <string>     => False
 *    <ident>      => False (remember for macros the <macro> rule applies instead)
 * Unary + and - are skipped
 * parenthesis in expressions are allowed
 * parameter lists on macros are errors
 * No mathematical calculations are executed
 */
static bool ftepp_if_expr(ftepp_t *ftepp, bool *out)
{
	ppmacro *macro;
	while (ftepp->token != TOKEN_EOL) {
		switch (ftepp->token) {
			case TOKEN_IDENT:
			case TOKEN_TYPENAME:
			case TOKEN_KEYWORD:
				macro = ftepp_macro_find(ftepp, ftepp_tokval(ftepp));
				if (!macro || !vec_size(macro->output)) {
					*out = false;
				} else {
					/* This does not expand recursively! */
					switch (macro->output[0]->token) {
						case TOKEN_INTCONST:
							*out = !!(macro->output[0]->constval.f);
							break;
						case TOKEN_FLOATCONST:
							*out = !!(macro->output[0]->constval.f);
							break;
						default:
							*out = false;
							break;
					}
				}
				break;
			case TOKEN_STRINGCONST:
				*out = false;
				break;
			case TOKEN_INTCONST:
				*out = !!(ftepp->lex->tok.constval.i);
				break;
			case TOKEN_FLOATCONST:
				*out = !!(ftepp->lex->tok.constval.f);
				break;

			default:
				ftepp_error(ftepp, "junk in #if");
				return false;
		}
	}
	(void)ftepp_next(ftepp);
	return true;
}

static bool ftepp_if(ftepp_t *ftepp, ppcondition *cond)
{
	bool result = false;

	memset(cond, 0, sizeof(*cond));
	(void)ftepp_next(ftepp);

	if (!ftepp_skipspace(ftepp))
		return false;
	if (ftepp->token == TOKEN_EOL) {
		ftepp_error(ftepp, "expected expression for #if-directive");
		return false;
	}

	if (!ftepp_if_expr(ftepp, &result))
		return false;

	cond->on = result;
	return true;
}

/**
 * ifdef is rather simple
 */
static bool ftepp_ifdef(ftepp_t *ftepp, ppcondition *cond)
{
	ppmacro *macro;
	memset(cond, 0, sizeof(*cond));
	(void)ftepp_next(ftepp);
	if (!ftepp_skipspace(ftepp))
		return false;

	switch (ftepp->token) {
		case TOKEN_IDENT:
		case TOKEN_TYPENAME:
		case TOKEN_KEYWORD:
			macro = ftepp_macro_find(ftepp, ftepp_tokval(ftepp));
			break;
		default:
			ftepp_error(ftepp, "expected macro name");
			return false;
	}

	(void)ftepp_next(ftepp);
	if (!ftepp_skipspace(ftepp))
		return false;
	if (ftepp->token != TOKEN_EOL) {
		ftepp_error(ftepp, "stray tokens after #ifdef");
		return false;
	}
	cond->on = !!macro;
	return true;
}

/* Basic structure handlers */
static bool ftepp_else_allowed(ftepp_t *ftepp)
{
	if (!vec_size(ftepp->conditions)) {
		ftepp_error(ftepp, "#else without #if");
		return false;
	}
	if (vec_last(ftepp->conditions).had_else) {
		ftepp_error(ftepp, "multiple #else for a single #if");
		return false;
	}
	return true;
}

static bool ftepp_hash(ftepp_t *ftepp)
{
	ppcondition cond;
	ppcondition *pc;

	lex_ctx ctx = ftepp_ctx(ftepp);

	if (!ftepp_skipspace(ftepp))
		return false;

	switch (ftepp->token) {
		case TOKEN_KEYWORD:
		case TOKEN_IDENT:
		case TOKEN_TYPENAME:
			if (!strcmp(ftepp_tokval(ftepp), "define")) {
				return ftepp_define(ftepp);
			}
			else if (!strcmp(ftepp_tokval(ftepp), "ifdef")) {
				if (!ftepp_ifdef(ftepp, &cond))
					return false;
				vec_push(ftepp->conditions, cond);
				return true;
			}
			else if (!strcmp(ftepp_tokval(ftepp), "ifndef")) {
				if (!ftepp_ifdef(ftepp, &cond))
					return false;
				cond.on = !cond.on;
				vec_push(ftepp->conditions, cond);
				return true;
			}
			else if (!strcmp(ftepp_tokval(ftepp), "elifdef")) {
				if (!ftepp_else_allowed(ftepp))
					return false;
				if (!ftepp_ifdef(ftepp, &cond))
					return false;
				pc = &vec_last(ftepp->conditions);
				pc->on     = !pc->was_on && cond.on;
				pc->was_on = pc->was_on || pc->on;
				return true;
			}
			else if (!strcmp(ftepp_tokval(ftepp), "elifndef")) {
				if (!ftepp_else_allowed(ftepp))
					return false;
				if (!ftepp_ifdef(ftepp, &cond))
					return false;
				cond.on = !cond.on;
				pc = &vec_last(ftepp->conditions);
				pc->on     = !pc->was_on && cond.on;
				pc->was_on = pc->was_on || pc->on;
				return true;
			}
			else if (!strcmp(ftepp_tokval(ftepp), "elif")) {
				if (!ftepp_else_allowed(ftepp))
					return false;
				if (!ftepp_if(ftepp, &cond))
					return false;
				pc = &vec_last(ftepp->conditions);
				pc->on     = !pc->was_on && cond.on;
				pc->was_on = pc->was_on  || pc->on;
				return true;
			}
			else if (!strcmp(ftepp_tokval(ftepp), "if")) {
				if (!ftepp_if(ftepp, &cond))
					return false;
				vec_push(ftepp->conditions, cond);
				return true;
			}
			else if (!strcmp(ftepp_tokval(ftepp), "else")) {
				if (!ftepp_else_allowed(ftepp))
					return false;
				pc = &vec_last(ftepp->conditions);
				pc->on = !pc->was_on;
				pc->had_else = true;
				return true;
			}
			else if (!strcmp(ftepp_tokval(ftepp), "endif")) {
				if (!vec_size(ftepp->conditions)) {
					ftepp_error(ftepp, "#endif without #if");
					return false;
				}
				vec_pop(ftepp->conditions);
				break;
			}
			else {
				ftepp_error(ftepp, "unrecognized preprocessor directive: `%s`", ftepp_tokval(ftepp));
				return false;
			}
			break;
		default:
			ftepp_error(ftepp, "unexpected preprocessor token: `%s`", ftepp_tokval(ftepp));
			return false;
		case TOKEN_EOL:
			ftepp_errorat(ftepp, ctx, "empty preprocessor directive");
			return false;
		case TOKEN_EOF:
			ftepp_error(ftepp, "missing newline at end of file", ftepp_tokval(ftepp));
			return false;
	}
	return true;
}

static void ftepp_out(ftepp_t *ftepp, const char *str)
{
	if (!vec_size(ftepp->conditions) ||
		vec_last(ftepp->conditions).on)
	{
		printf("%s", str);
	}
}

static bool ftepp_preprocess(ftepp_t *ftepp)
{
	bool newline = true;

	ftepp->lex->flags.preprocessing = true;

	ftepp_next(ftepp);
	do
	{
		if (ftepp->token >= TOKEN_EOF)
			break;

		ftepp->newline = newline;
		newline = false;

		switch (ftepp->token) {
			case '#':
				if (!ftepp->newline) {
					ftepp_out(ftepp, ftepp_tokval(ftepp));
					ftepp_next(ftepp);
					break;
				}
				if (ftepp_next(ftepp) >= TOKEN_EOF) {
					ftepp_error(ftepp, "error in preprocessor directive");
					ftepp->token = TOKEN_ERROR;
					break;
				}
				if (!ftepp_hash(ftepp))
					ftepp->token = TOKEN_ERROR;
				break;
			case TOKEN_EOL:
				newline = true;
				ftepp_out(ftepp, "\n");
				ftepp_next(ftepp);
				break;
			default:
				ftepp_out(ftepp, ftepp_tokval(ftepp));
				ftepp_next(ftepp);
				break;
		}
	} while (!ftepp->errors && ftepp->token < TOKEN_EOF);

	ftepp_delete(ftepp);
	return (ftepp->token == TOKEN_EOF);
}

bool ftepp_preprocess_file(const char *filename)
{
	ftepp_t *ftepp = ftepp_init();
    ftepp->lex = lex_open(filename);
    if (!ftepp->lex) {
        con_out("failed to open file \"%s\"\n", filename);
        return false;
    }
    return ftepp_preprocess(ftepp);
}

bool ftepp_preprocess_string(const char *name, const char *str)
{
	ftepp_t *ftepp = ftepp_init();
    ftepp->lex = lex_open_string(str, strlen(str), name);
    if (!ftepp->lex) {
        con_out("failed to create lexer for string \"%s\"\n", name);
        return false;
    }
    return ftepp_preprocess(ftepp);
}
