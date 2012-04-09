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
#include <limits.h>
#include "gmqcc.h"


static const char *const parse_punct[] = {
	"&&", "||", "<=", ">=", "==", "!=", ";", ",", "!", "*",
	"/" , "(" , "-" , "+" , "=" , "[" , "]", "{", "}", "...",
	"." , "<" , ">" , "#" , "&" , "|" , "$", "@", ":", NULL
	/* 
	 * $,@,: are extensions:
	 * $ is a shorter `self`, so instead of self.frags, $.frags
	 * @ is a constructor
	 * : is compiler builtin functions
	 */
};

int parse(struct lex_file *file) {
	int     token = 0;
	while ((token = lex_token(file)) != ERROR_LEX      && \
		    token                    != ERROR_COMPILER && \
		    token                    != ERROR_INTERNAL && \
		    token                    != ERROR_PARSE    && \
		    token                    != ERROR_PREPRO   && file->length >= 0) {
		switch (token) {
			case TOKEN_IF:
				token = lex_token(file);
				while ((token == ' ' || token == '\n') && file->length >= 0)
					token = lex_token(file);
					
				if (token != '(')
					error(ERROR_PARSE, "Expected `(` after if\n", "");
				break;
			
			/* TODO: Preprocessor */
			case '#':
				token = lex_token(file);
				token = lex_token(file);
				token = lex_token(file);
				token = lex_token(file);
				token = lex_token(file);
				token = lex_token(file);
				break;
				
			/* PUNCTUATION PARSING BEGINS */
			case '&':               /* &  */
				token = lex_token(file);
				if (token == '&') { /* && */
					token = lex_token(file);
					printf("--> LOGICAL AND\n");
					goto end;
				}
				printf("--> BITWISE AND\n");
				break;
			case '|':               /* |  */
				token = lex_token(file);
				if (token == '|') { /* || */
					token = lex_token(file);
					printf("--> LOGICAL OR\n");
					goto end;
				}
				printf("--> BITWISE OR\n");
				break;
			case '!':
				token = lex_token(file);
				if (token == '=') { /* != */
					token = lex_token(file);
					printf("--> LOGICAL NOT EQUAL\n");
					goto end;
				}
				printf("--> LOGICAL NOT\n");
				break;
			case '<':               /* <  */
				token = lex_token(file);
				if (token == '=') { /* <= */
					token = lex_token(file);
					printf("--> LESS THAN OR EQUALL\n");
					goto end;
				}
				printf("--> LESS THAN\n");
				break;
			case '>':               /* >  */
				token = lex_token(file);
				if (token == '=') { /* >= */
					token = lex_token(file);
					printf("--> GREATER THAN OR EQUAL\n");
					goto end;
				}
				printf("--> GREATER THAN\n");
				break;
			case '=':
				token = lex_token(file);
				if (token == '=') { /* == */
					token = lex_token(file);
					printf("--> COMPARISION \n");
					goto end;
				}
				printf("--> ASSIGNMENT\n");
				break;
			case ';':
				token = lex_token(file);
				printf("--> FINISHED STATMENT\n");
				break;
			case '-':
				token = lex_token(file);
				printf("--> SUBTRACTION EXPRESSION\n");
				break;
			case '+':
				token = lex_token(file);
				printf("--> ASSIGNMENT EXPRRESSION\n");
				break;
		}
		end:;
	}
	lex_reset(file);
	
	//	"&&", "||", "<=", ">=", "==", "!=", ";", ",", "!", "*",
	//"/" , "(" , "-" , "+" , "=" , "[" , "]", "{", "}", "...",
	//"." , "<" , ">" , "#" , "&" , "|" , "$", "@", ":", NULL
	
	return 1;
}	
