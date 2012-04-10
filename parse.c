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
 * These are all the punctuation handled in the parser, these don't
 * need tokens, they're already tokens.
 */
#if 0
	"&&", "||", "<=", ">=", "==", "!=", ";", ",", "!", "*",
	"/" , "(" , ")" , "-" , "+" , "=" , "[" , "]", "{", "}", "...",
	"." , "<" , ">" , "&" , "|" , 
#endif

#define STORE(X) {  \
	printf(X);      \
	break;          \
}

void parse_debug(struct parsenode *tree) {
	while (tree) {	
		switch (tree->type) {
			case PARSE_TYPE_ADD:       STORE("OPERATOR:  ADD    \n");
			case PARSE_TYPE_BAND:      STORE("OPERATOR:  BITAND \n");
			case PARSE_TYPE_BOR:       STORE("OPERATOR:  BITOR  \n");
			case PARSE_TYPE_COMMA:     STORE("OPERATOR:  SEPERATOR\n");
			case PARSE_TYPE_DOT:       STORE("OPERATOR:  DOT\n");
			case PARSE_TYPE_DIVIDE:    STORE("OPERATOR:  DIVIDE\n");
			case PARSE_TYPE_EQUAL:     STORE("OPERATOR:  ASSIGNMENT\n");
			
			case PARSE_TYPE_BREAK:     STORE("STATEMENT: BREAK  \n");
			case PARSE_TYPE_CONTINUE:  STORE("STATEMENT: CONTINUE\n");
			case PARSE_TYPE_GOTO:      STORE("STATEMENT: GOTO\n");
			case PARSE_TYPE_RETURN:    STORE("STATEMENT: RETURN\n");
			case PARSE_TYPE_DONE:      STORE("STATEMENT: DONE\n");

			case PARSE_TYPE_VOID:      STORE("DECLTYPE:  VOID\n");
			case PARSE_TYPE_STRING:    STORE("DECLTYPE:  STRING\n");
			case PARSE_TYPE_ELIP:      STORE("DECLTYPE:  VALIST\n");
			case PARSE_TYPE_ENTITY:    STORE("DECLTYPE:  ENTITY\n");
			case PARSE_TYPE_FLOAT:     STORE("DECLTYPE:  FLOAT\n");
			case PARSE_TYPE_VECTOR:    STORE("DECLTYPE:  VECTOR\n");
			
			case PARSE_TYPE_GT:        STORE("TEST:      GREATER THAN\n");
			case PARSE_TYPE_LT:        STORE("TEST:      LESS THAN\n");
			case PARSE_TYPE_GTEQ:      STORE("TEST:      GREATER THAN OR EQUAL\n");
			case PARSE_TYPE_LTEQ:      STORE("TEST:      LESS THAN OR EQUAL\n");
			case PARSE_TYPE_LNEQ:      STORE("TEST:      NOT EQUAL\n");
			case PARSE_TYPE_EQEQ:      STORE("TEST:      EQUAL-EQUAL\n");
			
			case PARSE_TYPE_LBS:       STORE("BLOCK:     BEG\n");
			case PARSE_TYPE_RBS:       STORE("BLOCK:     END\n");
			case PARSE_TYPE_ELSE:      STORE("BLOCK:     ELSE\n");
			case PARSE_TYPE_IF:        STORE("BLOCK:     IF\n");
			
			case PARSE_TYPE_LAND:      STORE("LOGICAL:   AND\n");
			case PARSE_TYPE_LNOT:      STORE("LOGICAL:   NOT\n");
			case PARSE_TYPE_LOR:       STORE("LOGICAL:   OR\n");
			
			case PARSE_TYPE_LPARTH:    STORE("PARTH:     BEG\n");
			case PARSE_TYPE_RPARTH:    STORE("PARTH:     END\n");
			
			case PARSE_TYPE_WHILE:     STORE("LOOP:      WHILE\n");
			case PARSE_TYPE_FOR:       STORE("LOOP:      FOR\n");
			case PARSE_TYPE_DO:        STORE("LOOP:      DO\n");
			
			//case PARSE_TYPE_IDENT:     STORE("IDENT:     ???\n");
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
	while ((token = lex_token(file)) != ERROR_LEX      && \
		    token                    != ERROR_COMPILER && \
		    token                    != ERROR_INTERNAL && \
		    token                    != ERROR_PARSE    && \
		    token                    != ERROR_PREPRO   && file->length >= 0) {
		switch (token) {
			case TOKEN_IF:
				while ((token == ' ' || token == '\n') && file->length >= 0)
					token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_IF);
				break;
			case TOKEN_ELSE:
				token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_ELSE);
				break;
			case TOKEN_FOR:
				//token = lex_token(file);
				while ((token == ' ' || token == '\n') && file->length >= 0)
					token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_FOR);
				break;
			
			/*
			 * This is a quick and easy way to do typedefs at parse time
			 * all power is in typedef_add(), in typedef.c.  We handle 
			 * the tokens accordingly here.
			 */
			case TOKEN_TYPEDEF: {
				char *f = NULL;
				char *t = NULL;
				token = lex_token(file); 
				token = lex_token(file); f = strdup(file->lastok);
				token = lex_token(file); 
				token = lex_token(file); t = strdup(file->lastok);
				
				typedef_add(f, t);
				
				free(f);
				free(t);
				
				while (token != '\n')
					token = lex_token(file);
				break;
			}
			
			/*
			 * Returns are addable as-is, statement checking is during
			 * the actual parse tree check.
			 */
			case TOKEN_RETURN:
				PARSE_TREE_ADD(PARSE_TYPE_RETURN);
				break;
				//PARSE_PERFORM(PARSE_TYPE_RETURN,  {});
			
				
			case TOKEN_DO:        PARSE_PERFORM(PARSE_TYPE_DO,      {});
			case TOKEN_WHILE:     PARSE_PERFORM(PARSE_TYPE_WHILE,   {});
			case TOKEN_BREAK:     PARSE_PERFORM(PARSE_TYPE_BREAK,   {});
			case TOKEN_CONTINUE:  PARSE_PERFORM(PARSE_TYPE_CONTINUE,{});
			case TOKEN_GOTO:      PARSE_PERFORM(PARSE_TYPE_GOTO,    {});
			case TOKEN_VOID:      PARSE_PERFORM(PARSE_TYPE_VOID,    {});
			case TOKEN_STRING:    PARSE_PERFORM(PARSE_TYPE_STRING,  {});
			case TOKEN_FLOAT:     PARSE_PERFORM(PARSE_TYPE_FLOAT,   {});
			case TOKEN_VECTOR:    PARSE_PERFORM(PARSE_TYPE_VECTOR,  {});
			case TOKEN_ENTITY:    PARSE_PERFORM(PARSE_TYPE_ENTITY,  {});
				
			/*
			 * From here down is all language punctuation:  There is no
			 * need to actual create tokens from these because they're already
			 * tokenized as these individual tokens (which are in a special area
			 * of the ascii table which doesn't conflict with our other tokens
			 * which are higer than the ascii table.)
			 */
			case '#':
				/*
				 * Skip the preprocessor for now:  We'll implement our own
				 * eventually.  For now we need to make sure directives are
				 * not accidently tokenized.
				 */
				token = lex_token(file);
				token = lex_token(file);
				
				/* skip all tokens to end of directive */
				while (token != '\n')
					token = lex_token(file);
				break;
				
			case '.':
				token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_DOT);
				break;
				
			case '(':
				token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_LPARTH);
				break;
			case ')':
				token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_RPARTH);
				break;
				
			case '&':               /* &  */
				token = lex_token(file);
				if (token == '&') { /* && */
					token = lex_token(file);
					PARSE_TREE_ADD(PARSE_TYPE_LAND);
					break;
				}
				PARSE_TREE_ADD(PARSE_TYPE_BAND);
				break;
			case '|':               /* |  */
				token = lex_token(file);
				if (token == '|') { /* || */
					token = lex_token(file);
					PARSE_TREE_ADD(PARSE_TYPE_LOR);
					break;
				}
				PARSE_TREE_ADD(PARSE_TYPE_BOR);
				break;
			case '!':
				token = lex_token(file);
				if (token == '=') { /* != */
					token = lex_token(file);
					PARSE_TREE_ADD(PARSE_TYPE_LNEQ);
					break;
				}
				PARSE_TREE_ADD(PARSE_TYPE_LNOT);
				break;
			case '<':               /* <  */
				token = lex_token(file);
				if (token == '=') { /* <= */
					token = lex_token(file);
					PARSE_TREE_ADD(PARSE_TYPE_LTEQ);
					break;
				}
				PARSE_TREE_ADD(PARSE_TYPE_LT);
				break;
			case '>':               /* >  */
				token = lex_token(file);
				if (token == '=') { /* >= */
					token = lex_token(file);
					PARSE_TREE_ADD(PARSE_TYPE_GTEQ);
					break;
				}
				PARSE_TREE_ADD(PARSE_TYPE_GT);
				break;
			case '=':
				token = lex_token(file);
				if (token == '=') { /* == */
					token = lex_token(file);
					PARSE_TREE_ADD(PARSE_TYPE_EQEQ);
					break;
				}
				PARSE_TREE_ADD(PARSE_TYPE_EQUAL);
				break;
			case ';':
				token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_DONE);
				break;
			case '-':
				token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_MINUS);
				break;
			case '+':
				token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_ADD);
				break;
			case '{':
				token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_LBS);
				break;
			case '}':
				token = lex_token(file);
				PARSE_TREE_ADD(PARSE_TYPE_RBS);
				break;
				
			/*
			 * TODO: Fix lexer to spit out ( ) as tokens, it seems the
			 * using '(' or ')' in parser doesn't work properly unless
			 * there are spaces before them to allow the lexer to properly
			 * seperate identifiers. -- otherwise it eats all of it.
			 */
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
