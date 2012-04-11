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
typedef prog_section_both prog_section_var;
typedef prog_section_both prog_section_field;

typedef struct {
	int32_t   entry;      /* in statement table for instructions  */
	uint32_t  args;       /* What is this?                        */
	uint32_t  locals;     /* Total ints of params + locals        */
	uint32_t  profile;    /* What is this?                        */
	uint32_t  name;       /* name of function in string table     */
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
	prog_section vars;         /* prog_section_var        */
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
 * code_globals_data         -- raw prog_section_var array
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
VECTOR_MAKE(prog_section_var,       code_vars      );
VECTOR_MAKE(prog_section_field,     code_fields    );
VECTOR_MAKE(prog_section_function,  code_functions );
VECTOR_MAKE(prog_section_var,       code_globals   );
VECTOR_MAKE(char*,                  code_strings   );

/* program header */
prog_header code_header;
void code_write() {
	code_header.version    = 6;
	code_header.crc16      = 0; /* TODO: */
	code_header.statements = (prog_section){sizeof(prog_header),                                                          code_statements_elements };
	code_header.vars       = (prog_section){sizeof(prog_header)+sizeof(prog_section_statement)*code_statements_elements,  code_vars_elements       };
	code_header.fields     = (prog_section){sizeof(prog_header)+sizeof(prog_section_var)      *code_vars_elements,        code_fields_elements     };
	code_header.functions  = (prog_section){sizeof(prog_header)+sizeof(prog_section_field)    *code_fields_elements,      code_functions_elements  };
	code_header.globals    = (prog_section){sizeof(prog_header)+sizeof(prog_section_function) *code_functions_elements,   code_globals_elements    };
	/* how, I think I don't have strings figured out yet :| */
	code_header.entfield   = 0; /* TODO: */
	
	#if 0 /* is this right? */
	fwrite(&code_header,         1, sizeof(prog_header), fp);
	fwrite(code_statements_data, 1, sizeof(prog_section_statement)*code_statements_elements, fp);
	fwrite(code_vars_data,       1, sizeof(prog_section_var)      *code_vars_elements,       fp);
	fwrite(code_fields_data,     1, sizeof(prog_section_field)    *code_fields_elements,     fp);
	fwrite(code_functions_data,  1, sizeof(prog_section_function) *code_functions_elements,  fp);
	fwrite(code_globals_data,    1, sizeof(prog_section_var)      *code_globals_elements,    fp);
	#endif
}
