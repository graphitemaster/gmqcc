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
	lex_file *lex;
} ftepp_t;

#define ftepp_tokval(f) ((f)->lex->tok.value)

ftepp_t* ftepp_init()
{
	ftepp_t *ftepp;

	ftepp = (ftepp_t*)mem_a(sizeof(*ftepp));
	memset(ftepp, 0, sizeof(*ftepp));

	return ftepp;
}

static bool ftepp_preprocess(ftepp_t *ftepp)
{
	int token;

	ftepp->lex->flags.preprocessing = true;

	for (token = lex_do(ftepp->lex); token < TOKEN_EOF; token = lex_do(ftepp->lex))
	{
		switch (token) {
			case TOKEN_EOL: printf("\n"); break;
			default:
				printf("%s", ftepp_tokval(ftepp));
				break;
		}
	}

	return (token == TOKEN_EOF);
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
