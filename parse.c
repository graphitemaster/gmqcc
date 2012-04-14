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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "gmqcc.h"

/*
 * These are not lexical tokens:  These are parse tree types.  Most people
 * perform tokenizing on language punctuation which is wrong.  That stuff
 * is technically already tokenized, it just needs to be parsed into a tree
 */
#define PARSE_TYPE_DO       0
#define PARSE_TYPE_ELSE     1
#define PARSE_TYPE_IF       2
#define PARSE_TYPE_WHILE    3
#define PARSE_TYPE_BREAK    4
#define PARSE_TYPE_CONTINUE 5
#define PARSE_TYPE_RETURN   6
#define PARSE_TYPE_GOTO     7
#define PARSE_TYPE_FOR      8
#define PARSE_TYPE_VOID     9
#define PARSE_TYPE_STRING   10
#define PARSE_TYPE_FLOAT    11
#define PARSE_TYPE_VECTOR   12
#define PARSE_TYPE_ENTITY   13
#define PARSE_TYPE_LAND     14
#define PARSE_TYPE_LOR      15
#define PARSE_TYPE_LTEQ     16
#define PARSE_TYPE_GTEQ     17
#define PARSE_TYPE_EQEQ     18
#define PARSE_TYPE_LNEQ     19
#define PARSE_TYPE_COMMA    20
#define PARSE_TYPE_LNOT     21
#define PARSE_TYPE_STAR     22
#define PARSE_TYPE_DIVIDE   23
#define PARSE_TYPE_LPARTH   24
#define PARSE_TYPE_RPARTH   25
#define PARSE_TYPE_MINUS    26
#define PARSE_TYPE_ADD      27
#define PARSE_TYPE_EQUAL    28
#define PARSE_TYPE_LBS      29
#define PARSE_TYPE_RBS      30
#define PARSE_TYPE_ELIP     31
#define PARSE_TYPE_DOT      32
#define PARSE_TYPE_LT       33
#define PARSE_TYPE_GT       34
#define PARSE_TYPE_BAND     35
#define PARSE_TYPE_BOR      36
#define PARSE_TYPE_DONE     37
#define PARSE_TYPE_IDENT    38

/*
 * Adds a parse type to the parse tree, this is where all the hard
 * work actually begins.
 */
#define PARSE_TREE_ADD(X)                                        \
	do {                                                         \
		parsetree->next       = mem_a(sizeof(struct parsenode)); \
		parsetree->next->next = NULL;                            \
		parsetree->next->type = (X);                             \
		parsetree             = parsetree->next;                 \
	} while (0)

/*
 * This is all the punctuation handled in the parser, these don't
 * need tokens, they're already tokens.
 */
#if 0
	"&&", "||", "<=", ">=", "==", "!=", ";", ",", "!", "*",
	"/" , "(" , ")" , "-" , "+" , "=" , "[" , "]", "{", "}", "...",
	"." , "<" , ">" , "&" , "|" , 
#endif

#define STORE(X,C) {  \
    long f = fill;    \
    while(f--) {      \
      putchar(' ');   \
    }                 \
    fill C;           \
	printf(X);        \
	break;            \
}

void parse_debug(struct parsenode *tree) {
	long fill = 0;
	while (tree) {	
		switch (tree->type) {
			case PARSE_TYPE_ADD:       STORE("OPERATOR:  ADD    \n", -=0);
			case PARSE_TYPE_BAND:      STORE("OPERATOR:  BITAND \n",-=0);
			case PARSE_TYPE_BOR:       STORE("OPERATOR:  BITOR  \n",-=0);
			case PARSE_TYPE_COMMA:     STORE("OPERATOR:  SEPERATOR\n",-=0);
			case PARSE_TYPE_DOT:       STORE("OPERATOR:  DOT\n",-=0);
			case PARSE_TYPE_DIVIDE:    STORE("OPERATOR:  DIVIDE\n",-=0);
			case PARSE_TYPE_EQUAL:     STORE("OPERATOR:  ASSIGNMENT\n",-=0);
			
			case PARSE_TYPE_BREAK:     STORE("STATEMENT: BREAK  \n",-=0);
			case PARSE_TYPE_CONTINUE:  STORE("STATEMENT: CONTINUE\n",-=0);
			case PARSE_TYPE_GOTO:      STORE("STATEMENT: GOTO\n",-=0);
			case PARSE_TYPE_RETURN:    STORE("STATEMENT: RETURN\n",-=0);
			case PARSE_TYPE_DONE:      STORE("STATEMENT: DONE\n",-=0);

			case PARSE_TYPE_VOID:      STORE("DECLTYPE:  VOID\n",-=0);
			case PARSE_TYPE_STRING:    STORE("DECLTYPE:  STRING\n",-=0);
			case PARSE_TYPE_ELIP:      STORE("DECLTYPE:  VALIST\n",-=0);
			case PARSE_TYPE_ENTITY:    STORE("DECLTYPE:  ENTITY\n",-=0);
			case PARSE_TYPE_FLOAT:     STORE("DECLTYPE:  FLOAT\n",-=0);
			case PARSE_TYPE_VECTOR:    STORE("DECLTYPE:  VECTOR\n",-=0);
			
			case PARSE_TYPE_GT:        STORE("TEST:      GREATER THAN\n",-=0);
			case PARSE_TYPE_LT:        STORE("TEST:      LESS THAN\n",-=0);
			case PARSE_TYPE_GTEQ:      STORE("TEST:      GREATER THAN OR EQUAL\n",-=0);
			case PARSE_TYPE_LTEQ:      STORE("TEST:      LESS THAN OR EQUAL\n",-=0);
			case PARSE_TYPE_LNEQ:      STORE("TEST:      NOT EQUAL\n",-=0);
			case PARSE_TYPE_EQEQ:      STORE("TEST:      EQUAL-EQUAL\n",-=0);
			
			case PARSE_TYPE_LBS:       STORE("BLOCK:     BEG\n",+=4);
			case PARSE_TYPE_RBS:       STORE("BLOCK:     END\n",-=4);
			case PARSE_TYPE_ELSE:      STORE("BLOCK:     ELSE\n",+=0);
			case PARSE_TYPE_IF:        STORE("BLOCK:     IF\n",+=0);
			
			case PARSE_TYPE_LAND:      STORE("LOGICAL:   AND\n",-=0);
			case PARSE_TYPE_LNOT:      STORE("LOGICAL:   NOT\n",-=0);
			case PARSE_TYPE_LOR:       STORE("LOGICAL:   OR\n",-=0);
			
			case PARSE_TYPE_LPARTH:    STORE("PARTH:     BEG\n",-=0);
			case PARSE_TYPE_RPARTH:    STORE("PARTH:     END\n",-=0);
			
			case PARSE_TYPE_WHILE:     STORE("LOOP:      WHILE\n",-=0);
			case PARSE_TYPE_FOR:       STORE("LOOP:      FOR\n",-=0);
			case PARSE_TYPE_DO:        STORE("LOOP:      DO\n",-=0);
		}
		tree = tree->next;
	}
}

/*
 * Performs a parse operation:  This is a macro to prevent bugs, if the
 * calls to lex_token are'nt exactly enough to feed to the end of the
 * actual lexees for the current thing that is being parsed, the state 
 * of the next iteration in the creation of the parse tree will be wrong
 * and everything will fail.
 */
#define PARSE_PERFORM(X,C) {     \
    token = lex_token(file);     \
    { C }                        \
    while (token != '\n') {      \
	    token = lex_token(file); \
    }                            \
    PARSE_TREE_ADD(X);           \
    break;                       \
}

void parse_clear(struct parsenode *tree) {
	if (!tree) return;
	struct parsenode *temp = NULL;
	while (tree != NULL) {
		temp = tree;
		tree = tree->next;
		mem_d (temp);
	}
	
	/* free any potential typedefs */
	typedef_clear();
}

const char *STRING_(char ch) {
	if (ch == ' ')
		return "<space>";
	if (ch == '\n')
		return "<newline>";
	if (ch == '\0')
		return "<null>";
		
	return &ch;
}

#define TOKEN_SKIPWHITE()        \
	token = lex_token(file);     \
	while (token == ' ') {       \
		token = lex_token(file); \
	}

/*
 * Generates a parse tree out of the lexees generated by the lexer.  This
 * is where the tree is built.  This is where valid check is performed.
 */
int parse_tree(struct lex_file *file) {
	struct parsenode *parsetree = NULL;
	struct parsenode *parseroot = NULL;
	
	/*
	 * Allocate memory for our parse tree:
	 * the parse tree is just a singly linked list which will contain
	 * all the data for code generation.
	 */
	if (!parseroot) {
		parseroot = mem_a(sizeof(struct parsenode));
		if (!parseroot)
			return error(ERROR_INTERNAL, "Ran out of memory", " ");
		parsetree       = parseroot;
		parsetree->type = -1; /* not a valid type -- root element */
	}
	
	int     token = 0;
	long    line  = 0;
	while ((token = lex_token(file)) != ERROR_LEX      && \
		    token                    != ERROR_COMPILER && \
		    token                    != ERROR_INTERNAL && \
		    token                    != ERROR_PARSE    && \
		    token                    != ERROR_PREPRO   && file->length >= 0) {
		line = file->line;
		switch (token) {
			case TOKEN_TYPEDEF: {
				char *f; /* from */
				char *t; /* to   */
				
				token = lex_token(file); 
				token = lex_token(file); f = util_strdup(file->lastok);
				token = lex_token(file); 
				token = lex_token(file); t = util_strdup(file->lastok);
				
				typedef_add(f, t);
				mem_d(f);
				mem_d(t);
				
				token = lex_token(file);
				if (token == ' ')
					token = lex_token(file);
					
				if (token != ';')
					error(ERROR_PARSE, "%s:%d Expected a `;` at end of typedef statement\n", file->name, file->line);
					
				token = lex_token(file);
				break;
			}
			
			case TOKEN_VOID:      PARSE_TREE_ADD(PARSE_TYPE_VOID);   goto fall;
			case TOKEN_STRING:    PARSE_TREE_ADD(PARSE_TYPE_STRING); goto fall;
			case TOKEN_VECTOR:    PARSE_TREE_ADD(PARSE_TYPE_VECTOR); goto fall;
			case TOKEN_ENTITY:    PARSE_TREE_ADD(PARSE_TYPE_ENTITY); goto fall;
			case TOKEN_FLOAT:     PARSE_TREE_ADD(PARSE_TYPE_FLOAT);  goto fall;
			{
			fall:;
				char *name = NULL;
				int   type = token; /* story copy */
				
				/* skip over space */
				token = lex_token(file);
				if (token == ' ')
					token = lex_token(file);
				
				/* save name */
				name = util_strdup(file->lastok);
				
				/* skip spaces */
				token = lex_token(file);
				if (token == ' ')
					token = lex_token(file);
					
				if (token == ';') {
					/*
					 * Definitions go to the defs table, they don't have
					 * any sort of data with them yet.
					 */
				} else if (token == '=') {
					token = lex_token(file);
					if (token == ' ')
						token = lex_token(file);
					
					/* strings are in file->lastok */
					switch (type) {
						case TOKEN_VOID:
							return error(ERROR_PARSE, "%s:%d Cannot assign value to type void\n", file->name, file->line);
						case TOKEN_STRING:
							if (*file->lastok != '"')
								error(ERROR_PARSE, "%s:%d Expected a '\"' (quote) for string constant\n", file->name, file->line);
							break;
						case TOKEN_VECTOR: {
							float compile_calc_x = 0;
							float compile_calc_y = 0;
							float compile_calc_z = 0;
							int   compile_calc_d = 0; /* dot?        */
							int   compile_calc_s = 0; /* sign (-, +) */
							
							char  compile_data[1024];
							char *compile_eval = compile_data;
							
							if (token != '{')
								error(ERROR_PARSE, "%s:%d Expected initializer list `{`,`}` for vector constant\n", file->name, file->line);	
							
							/*
							 * This parses a single vector element: x,y & z.  This will handle all the
							 * complicated mechanics of a vector, and can be extended as well.  This
							 * is a rather large macro, and is #undef after it's use below.
							 */
						#define PARSE_VEC_ELEMENT(NAME, BIT)                                                                                                                               \
							token = lex_token(file);                                                                                                                                           \
							if (token == ' ') {                                                                                                                                                \
								token = lex_token(file);                                                                                                                                       \
							}                                                                                                                                                                  \
							if (token == '.') {                                                                                                                                                \
								compile_calc_d = 1;                                                                                                                                            \
							}                                                                                                                                                                  \
							if (!isdigit(token) && !compile_calc_d && token != '+' && token != '-')                                                                                            \
								error(ERROR_PARSE,"%s:%d Invalid constant initializer element %c for vector, must be numeric\n", file->name, file->line, NAME);                                \
							if (token == '+') {                                                                                                                                                \
								compile_calc_s = '+';                                                                                                                                          \
							}                                                                                                                                                                  \
							if (token == '-' && !compile_calc_s) {                                                                                                                             \
								compile_calc_s = '-';                                                                                                                                          \
							}                                                                                                                                                                  \
							while (isdigit(token) || token == '.' || token == '+' || token == '-') {                                                                                           \
								*compile_eval++ = token;                                                                                                                                       \
								token           = lex_token(file);                                                                                                                             \
								if (token == '.' && compile_calc_d) {                                                                                                                          \
									error(ERROR_PARSE, "%s:%d Invalid constant initializer element %c for vector, must be numeric.\n", file->name, file->line, NAME);                          \
									token = lex_token(file);                                                                                                                                   \
								}                                                                                                                                                              \
								if ((token == '-' || token == '+') && compile_calc_s) {                                                                                                        \
									error(ERROR_PARSE, "%s:%d Invalid constant initializer sign for vector element %c\n", file->name, file->line, NAME);                                       \
									token = lex_token(file);                                                                                                                                   \
								} else if (token == '.' && !compile_calc_d) {                                                                                                                  \
									compile_calc_d = 1;                                                                                                                                        \
								} else if (token == '-' && !compile_calc_s) {                                                                                                                  \
									compile_calc_s = '-';                                                                                                                                      \
								} else if (token == '+' && !compile_calc_s) {                                                                                                                  \
									compile_calc_s = '+';                                                                                                                                      \
								}                                                                                                                                                              \
							}                                                                                                                                                                  \
							if (token == ' ') {                                                                                                                                                \
								token = lex_token(file);                                                                                                                                       \
							}                                                                                                                                                                  \
							if (NAME != 'z') {                                                                                                                                                 \
								if (token != ',' && token != ' ')  {                                                                                                                           \
									error(ERROR_PARSE, "%s:%d invalid constant initializer element %c for vector (missing spaces, or comma delimited list?)\n", NAME, file->name, file->line); \
								}                                                                                                                                                              \
							} else if (token != '}') {                                                                                                                                         \
								error(ERROR_PARSE, "%s:%d Expected `}` on end of constant initialization for vector\n", file->name, file->line);                                               \
							}                                                                                                                                                                  \
							compile_calc_##BIT = atof(compile_data);                                                                                                                           \
							compile_calc_d = 0;                                                                                                                                                \
							compile_calc_s = 0;                                                                                                                                                \
							compile_eval   = &compile_data[0];                                                                                                                                 \
							memset(compile_data, 0, sizeof(compile_data))
							
							/*
							 * Parse all elements using the macro above.
							 * We must undef the macro afterwards.
							 */
							PARSE_VEC_ELEMENT('x', x);
							PARSE_VEC_ELEMENT('y', y);
							PARSE_VEC_ELEMENT('z', z);
							#undef PARSE_VEC_ELEMENT
							
							/*
							 * Check for the semi-colon... This is insane
							 * the amount of parsing here that is.
							 */
							token = lex_token(file);
							if (token == ' ')
								token = lex_token(file);
							if (token != ';')
								error(ERROR_PARSE, "%s:%d Expected `;` on end of constant initialization for vector\n", file->name, file->line);
								
							printf("VEC_X: %f\n", compile_calc_x);
							printf("VEC_Y: %f\n", compile_calc_y);
							printf("VEC_Z: %f\n", compile_calc_z);
							break;
						}
							
						case TOKEN_ENTITY:
						case TOKEN_FLOAT:
							
							if (!isdigit(token))
								error(ERROR_PARSE, "%s:%d Expected numeric constant for float constant\n");
							break;
					}
				} else if (token == '(') {
					printf("FUNCTION ??\n");
				}
				mem_d(name);
			}
				
			/*
			 * From here down is all language punctuation:  There is no
			 * need to actual create tokens from these because they're already
			 * tokenized as these individual tokens (which are in a special area
			 * of the ascii table which doesn't conflict with our other tokens
			 * which are higer than the ascii table.)
			 */
			case '#':
				token = lex_token(file); /* skip '#' */
				if (token == ' ')
					token = lex_token(file);
				/*
				 * If we make it here we found a directive, the supported
				 * directives so far are #include.
				 */
				if (strncmp(file->lastok, "include", sizeof("include")) == 0) {
					/*
					 * We only suport include " ", not <> like in C (why?)
					 * because the latter is silly.
					 */
					while (*file->lastok != '"' && token != '\n')
						token = lex_token(file);
					if (token == '\n')
						return error(ERROR_PARSE, "%d: Invalid use of include preprocessor directive: wanted #include \"file.h\"\n", file->line-1);
				}
			
				/* skip all tokens to end of directive */
				while (token != '\n')
					token = lex_token(file);
				break;
				
			case LEX_IDENT:
				token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_IDENT);
				break;
		}
	}
	parse_debug(parseroot);
	lex_reset(file);
	parse_clear(parseroot);
	return 1;
}	
