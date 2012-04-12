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
#include <stdint.h>
#include <stdlib.h>
#include "gmqcc.h"

typedef struct {
	uint16_t opcode;
	
	/* operand 1 */
	union {
		int16_t  s1; /* signed   */
		uint16_t u1; /* unsigned */
	};
	/* operand 2 */
	union {
		int16_t  s2; /* signed   */
		uint16_t u2; /* unsigned */
	};
	/* operand 3 */
	union {
		int16_t  s3; /* signed   */
		uint16_t u3; /* unsigned */
	};
	
	/*
	 * This is the same as the structure in darkplaces
	 * {
	 *     unsigned short op;
	 *     short          a,b,c;
	 * }
	 * But this one is more sane to work with, and the
	 * type sizes are guranteed.
	 */
} prog_section_statement;

typedef struct {
	/* The type is (I assume)
	 * 0 = ev_void
	 * 1 = ev_string
	 * 2 = ev_float
	 * 3 = ev_vector
	 * 4 = ev_entity
	 * 5 = ev_field
	 * 6 = ev_function
	 * 7 = ev_pointer
	 * 8 = ev_bad    (is this right for uint16_t type?)
	 */
	uint16_t type;
	uint16_t offset;     /* offset in file? (what about length)   */
	uint32_t name;       /* offset in string table? (confused :() */
} prog_section_both;

/*
 * var and field use the same structure.  But lets not use the same
 * name just for safety reasons?  (still castable ...).
 */
typedef prog_section_both prog_section_def;
typedef prog_section_both prog_section_field;

typedef struct {
	int32_t   entry;      /* in statement table for instructions  */
	uint32_t  firstlocal; /* First local in local table           */
	uint32_t  locals;     /* Total ints of params + locals        */
	uint32_t  profile;    /* Always zero (engine uses this)       */
	uint32_t  name;       /* name of function in string table     */
	uint32_t  file;       /* file of the source file              */
	uint32_t  nargs;      /* number of arguments                  */
	uint8_t   argsize[8]; /* size of arguments (keep 8 always?)   */
} prog_section_function;

typedef struct {
	uint32_t offset;      /* Offset in file of where data begins  */
	uint32_t length;      /* Length of section (how many of)      */
} prog_section;

typedef struct {
	uint32_t     version;      /* Program version (6)     */
	uint32_t     crc16;        /* What is this?           */
	prog_section statements;   /* prog_section_statement  */
	prog_section defs;         /* prog_section_def        */
	prog_section fields;       /* prog_section_field      */
	prog_section functions;    /* prog_section_function   */
	prog_section strings;      /* What is this?           */
	prog_section globals;      /* What is this?           */
	uint32_t     entfield;     /* Number of entity fields */
} prog_header;

/*
 * The macros below expand to a typesafe vector implementation, which
 * can be viewed in gmqcc.h
 * 
 * code_statements_data      -- raw prog_section_statement array
 * code_statements_elements  -- number of elements
 * code_statements_allocated -- size of the array allocated
 * code_statements_add(T)    -- add element (returns -1 on error)
 * 
 * code_vars_data            -- raw prog_section_var array
 * code_vars_elements        -- number of elements
 * code_vars_allocated       -- size of the array allocated
 * code_vars_add(T)          -- add element (returns -1 on error)
 * 
 * code_fields_data          -- raw prog_section_field array
 * code_fields_elements      -- number of elements
 * code_fields_allocated     -- size of the array allocated
 * code_fields_add(T)        -- add element (returns -1 on error)
 *
 * code_functions_data       -- raw prog_section_function array
 * code_functions_elements   -- number of elements
 * code_functions_allocated  -- size of the array allocated
 * code_functions_add(T)     -- add element (returns -1 on error)
 * 
 * code_globals_data         -- raw prog_section_def array
 * code_globals_elements     -- number of elements
 * code_globals_allocated    -- size of the array allocated
 * code_globals_add(T)       -- add element (returns -1 on error)
 * 
 * code_strings_data         -- raw char* array
 * code_strings_elements     -- number of elements
 * code_strings_allocated    -- size of the array allocated
 * code_strings_add(T)       -- add element (returns -1 on error)
 */
VECTOR_MAKE(prog_section_statement, code_statements);
VECTOR_MAKE(prog_section_def,       code_defs      );
VECTOR_MAKE(prog_section_field,     code_fields    );
VECTOR_MAKE(prog_section_function,  code_functions );
VECTOR_MAKE(int,                    code_globals   );
VECTOR_MAKE(char,                   code_strings   );
prog_header     code_header ={0};

void code_init() {
	/*
	 * The way progs.dat is suppose to work is odd, there needs to be
	 * some null (empty) statements, functions, and 28 globals
	 */
	prog_section_function  empty_function  = {0,0,0,0,0,0,0,{0}};
	prog_section_statement empty_statement = {0,{0},{0},{0}};
	int i;
	for(i = 0; i < 28; i++)
		code_globals_add(0);
		
	code_strings_add   ('\0');
	code_functions_add (empty_function);
	code_statements_add(empty_statement);
}

void code_test() {
	const char *X;
	size_t size = sizeof(X);
	size_t iter = 0;
	
	#define FOO(Y) \
		X = Y; \
		size = sizeof(Y); \
		for (iter=0; iter < size; iter++) { \
			code_strings_add(X[iter]);    \
		}
		
	FOO("m_init");
	FOO("print");
	FOO("hello world\n");
	FOO("m_keydown");
	FOO("m_draw");
	FOO("m_toggle");
	FOO("m_shutdown");
	
	code_globals_add(1);  /* m_init */
	code_globals_add(2);  /* print  */
	code_globals_add(14); /* hello world in string table */
	
	/* now the defs */
	code_defs_add((prog_section_def){.type=TYPE_VOID,    .offset=28/*globals[28]*/, .name=1 }); /* m_init */
	code_defs_add((prog_section_def){.type=TYPE_FUNCTION,.offset=29/*globals[29]*/, .name=8 }); /* print  */
	code_defs_add((prog_section_def){.type=TYPE_STRING,  .offset=30/*globals[30]*/, .name=14}); /*hello_world*/
	
	code_functions_add((prog_section_function){1,  0, 0, 0, .name=1, 0, 0, {0}}); /* m_init */
	code_functions_add((prog_section_function){-4, 0, 0, 0, .name=8, 0, 0, {0}}); /* print  */
	code_functions_add((prog_section_function){0,  0, 0, 0, .name=14+13,        0,0, {0}}); /* m_keydown */
	code_functions_add((prog_section_function){0,  0, 0, 0, .name=14+13+10,     0,0, {0}});
	code_functions_add((prog_section_function){0,  0, 0, 0, .name=14+13+10+7,   0,0, {0}});
	code_functions_add((prog_section_function){0,  0, 0, 0, .name=14+13+10+7+9, 0,0, {0}});
	
	code_statements_add((prog_section_statement){INSTR_STORE_F, {30}/*30 is hello_world */, {OFS_PARM0}, {0}});
	code_statements_add((prog_section_statement){INSTR_CALL1,   {29}/*29 is print       */, {0},         {0}});
	code_statements_add((prog_section_statement){INSTR_RETURN,  {0},                        {0},         {0}});
	
	code_header.version    = 6;
	code_header.crc16      = 0; /* TODO: */
	code_header.statements = (prog_section){sizeof(prog_header), code_statements_elements };
	code_header.defs       = (prog_section){code_header.statements.offset + sizeof(prog_section_statement)*code_statements_elements,  code_defs_elements       };
	code_header.fields     = (prog_section){code_header.defs.offset       + sizeof(prog_section_def)      *code_defs_elements,        code_fields_elements     };
	code_header.functions  = (prog_section){code_header.fields.offset     + sizeof(prog_section_field)    *code_fields_elements,      code_functions_elements  };
	code_header.globals    = (prog_section){code_header.functions.offset  + sizeof(prog_section_function) *code_functions_elements,   code_globals_elements    };
	code_header.strings    = (prog_section){code_header.globals.offset    + sizeof(int)                   *code_globals_elements,     code_strings_elements    };
	code_header.entfield   = 0; /* TODO: */
}

/* program header */
void code_write() {
	code_init();
	code_test();
	
	FILE *fp = fopen("program.dat", "wb");
	fwrite(&code_header,         1, sizeof(prog_header), fp);
	fwrite(code_statements_data, 1, sizeof(prog_section_statement)*code_statements_elements, fp);
	fwrite(code_defs_data,       1, sizeof(prog_section_def)      *code_defs_elements,       fp);
	fwrite(code_fields_data,     1, sizeof(prog_section_field)    *code_fields_elements,     fp);
	fwrite(code_functions_data,  1, sizeof(prog_section_function) *code_functions_elements,  fp);
	fwrite(code_globals_data,    1, sizeof(int)                   *code_globals_elements,    fp);
	fwrite(code_strings_data,    1, 1                             *code_strings_elements,    fp);
	
	free(code_statements_data);
	free(code_defs_data);
	free(code_fields_data);
	free(code_functions_data);
	free(code_globals_data);
	free(code_strings_data);
	
	fclose(fp);
}
