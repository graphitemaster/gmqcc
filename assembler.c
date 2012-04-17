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
#include "gmqcc.h"
/*
 * This is the assembler, gmqas, this is being implemented because I'm
 * not exactly sure how codegen would work for the C compiler as of yet
 * and also I plan to allow inline assembly for the compiler.
 */
static const char *const asm_instr[] = {
	[INSTR_DONE]       = "DONE",
	[INSTR_MUL_F]      = "MUL_F",
	[INSTR_MUL_V]      = "MUL_V",
	[INSTR_MUL_FV]     = "MUL_FV",
	[INSTR_MUL_VF]     = "MUL_VF",
	[INSTR_DIV_F]      = "DIV",
	[INSTR_ADD_F]      = "ADD_F",
	[INSTR_ADD_V]      = "ADD_V",
	[INSTR_SUB_F]      = "SUB_F",
	[INSTR_SUB_V]      = "DUB_V",
	[INSTR_EQ_F]       = "EQ_F",
	[INSTR_EQ_V]       = "EQ_V",
	[INSTR_EQ_S]       = "EQ_S",
	[INSTR_EQ_E]       = "EQ_E",
	[INSTR_EQ_FNC]     = "ES_FNC",
	[INSTR_NE_F]       = "NE_F",
	[INSTR_NE_V]       = "NE_V",
	[INSTR_NE_S]       = "NE_S",
	[INSTR_NE_E]       = "NE_E",
	[INSTR_NE_FNC]     = "NE_FNC",
	[INSTR_LE]         = "LE",
	[INSTR_GE]         = "GE",
	[INSTR_LT]         = "LT",
	[INSTR_GT]         = "GT",
	[INSTR_LOAD_F]     = "FIELD_F",
	[INSTR_LOAD_V]     = "FIELD_V",
	[INSTR_LOAD_S]     = "FIELD_S",
	[INSTR_LOAD_ENT]   = "FIELD_ENT",
	[INSTR_LOAD_FLD]   = "FIELD_FLD",
	[INSTR_LOAD_FNC]   = "FIELD_FNC",
	[INSTR_ADDRESS]    = "ADDRESS",
	[INSTR_STORE_F]    = "STORE_F",
	[INSTR_STORE_V]    = "STORE_V",
	[INSTR_STORE_S]    = "STORE_S",
	[INSTR_STORE_ENT]  = "STORE_ENT",
	[INSTR_STORE_FLD]  = "STORE_FLD",
	[INSTR_STORE_FNC]  = "STORE_FNC",
	[INSTR_STOREP_F]   = "STOREP_F",
	[INSTR_STOREP_V]   = "STOREP_V",
	[INSTR_STOREP_S]   = "STOREP_S",
	[INSTR_STOREP_ENT] = "STOREP_ENT",
	[INSTR_STOREP_FLD] = "STOREP_FLD",
	[INSTR_STOREP_FNC] = "STOREP_FNC",
	[INSTR_RETURN]     = "RETURN",
	[INSTR_NOT_F]      = "NOT_F",
	[INSTR_NOT_V]      = "NOT_V",
	[INSTR_NOT_S]      = "NOT_S",
	[INSTR_NOT_ENT]    = "NOT_ENT",
	[INSTR_NOT_FNC]    = "NOT_FNC",
	[INSTR_IF]         = "IF",
	[INSTR_IFNOT]      = "IFNOT",
	[INSTR_CALL0]      = "CALL0",
	[INSTR_CALL1]      = "CALL1",
	[INSTR_CALL2]      = "CALL2",
	[INSTR_CALL3]      = "CALL3",
	[INSTR_CALL4]      = "CALL4",
	[INSTR_CALL5]      = "CALL5",
	[INSTR_CALL6]      = "CALL6",
	[INSTR_CALL7]      = "CALL7",
	[INSTR_CALL8]      = "CALL8",
	[INSTR_STATE]      = "STATE",
	[INSTR_GOTO]       = "GOTO",
	[INSTR_AND]        = "AND",
	[INSTR_OR]         = "OR",
	[INSTR_BITAND]     = "AND",
	[INSTR_BITOR]      = "OR"
};

/*
 * Some assembler keywords not part of the opcodes above: these are
 * for creating functions, or constants.
 */
const char *const asm_keys[] = {
	"FLOAT"    , /* define float  */
	"VECTOR"   , /* define vector */
	"ENTITY"   , /* define ent    */
	"FIELD"    , /* define field  */
	"STRING"   , /* define string */
	"FUNCTION"
};

static char *const asm_getline(size_t *byte, FILE *fp) {
	char   *line = NULL;
	ssize_t read = getline(&line, byte, fp);
	*byte = read;
	if (read == -1) {
		free (line);
		//exit (1);
		return NULL;
	}
	return line;
}

#define asm_rmnewline(L,S) *((L)+*(S)-1) = '\0'
#define asm_skipwhite(L)             \
    while((*(L)==' '||*(L)=='\t')) { \
        (L)++;                       \
    }
    
void asm_init(const char *file, FILE **fp) {
	*fp = fopen(file, "r");
	code_init();
}

void asm_close(FILE *fp) {
	fclose(fp);
	code_write();
}
	
void asm_parse(FILE *fp) {
	char  *data = NULL;
	char  *skip = NULL;
	long   line = 1; /* current line */
	size_t size = 0; /* size of line */
	
	while ((data = asm_getline(&size, fp)) != NULL) {
		skip = data;
		asm_skipwhite(skip);
		asm_rmnewline(skip, &size);
		
		#define DECLTYPE(X, CODE)                                         \
		    if (!strncmp(X, skip, strlen(X))) {                           \
		        if (skip[strlen(X)] != ':') {                             \
		            printf("%li: Missing `:` after decltype\n",line);     \
		            exit (1);                                             \
		        }                                                         \
		        skip += strlen(X)+1;                                      \
		        asm_skipwhite(skip);                                      \
		        if(!isalpha(*skip)) {                                     \
		            printf("%li: Invalid identififer: %s\n", line, skip); \
		            exit (1);                                             \
		        } else {                                                  \
		            size_t offset_code      = code_statements_elements+1; \
		            size_t offset_chars     = code_strings_elements   +1; \
		            size_t offset_globals   = code_globals_elements   +1; \
		            size_t offset_functions = code_functions_elements +1; \
		            size_t offset_fields    = code_fields_elements    +1; \
		            size_t offset_defs      = code_defs_elements      +1; \
		            CODE                                                  \
		            /* silent unused warnings */                          \
		            (void)offset_code;                                    \
		            (void)offset_chars;                                   \
		            (void)offset_globals;                                 \
		            (void)offset_functions;                               \
		            (void)offset_fields;                                  \
		            (void)offset_defs;                                    \
		        }                                                         \
		        goto end;                                                 \
		    }
		
		/* FLOAT    */
		DECLTYPE(asm_keys[0], {
			code_defs_add((prog_section_def){
				.type   = TYPE_FLOAT,
				.offset = offset_globals, /* global table */
				.name   = offset_chars    /* string table TODO */
			});
			float f = 0; /*TODO*/
			code_globals_add(*(int*)&f);
			
		});
		DECLTYPE(asm_keys[1], {
			code_defs_add((prog_section_def){
				.type   = TYPE_FLOAT,
				.offset = offset_globals, /* global table */
				.name   = offset_chars    /* string table TODO */
			});
			float f1 = 0;
			float f2 = 0;
			float f3 = 0;
			code_globals_add(*(int*)&f1);
			code_globals_add(*(int*)&f2);
			code_globals_add(*(int*)&f3);
		});
		/* ENTITY   */ DECLTYPE(asm_keys[2], {});
		/* FIELD    */ DECLTYPE(asm_keys[3], {});
		/* STRING   */
		DECLTYPE(asm_keys[4], {
			code_defs_add((prog_section_def){
				.type   = TYPE_STRING,    
				.offset = offset_globals, /* offset to offset in string table (for data)*/
				.name   = offset_chars    /* location of name in string table (for name)*/
			});
		});
		/* FUNCTION */ DECLTYPE(asm_keys[5], {});
		
		/* if we make it this far then we have statements */
		{
			size_t i = 0;
			for (; i < sizeof(asm_instr)/sizeof(*asm_instr); i++) {
				if (!strncmp(skip, asm_instr[i], strlen(asm_instr[i]))) {
					/* TODO */
					goto end;
				}
			}
		}
		
		/* if we made it this far something is wrong */
		printf("ERROR");
		
		end:
		free(data);
	}
}
