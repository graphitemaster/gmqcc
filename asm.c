/*
 * Copyright (C) 2012 
 *     Dale Weiler
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
    ssize_t read = util_getline(&line, byte, fp);
    *byte = read;
    if (read == -1) {
        mem_d (line);
        return NULL;
    }
    return line;
}

void asm_init(const char *file, FILE **fp) {
    *fp = fopen(file, "r");
    code_init();
}

void asm_close(FILE *fp) {
    fclose(fp);
    code_write();
}

/*
 * Following parse states:
 *     ASM_FUNCTION -- in a function accepting input statements
 *     ....
 */
typedef enum {
    ASM_NULL,
    ASM_FUNCTION
} asm_state;

typedef struct {
    char *name;   /* name of constant    */
    int   offset; /* location in globals */
} globals;
VECTOR_MAKE(globals, assembly_constants);

void asm_clear() {
    size_t i = 0;
    for (; i < assembly_constants_elements; i++)
        mem_d(assembly_constants_data[i].name);
    mem_d(assembly_constants_data);
}

int asm_parsetype(const char *key, char **skip, long line) {
    size_t keylen = strlen(key);
    if (!strncmp(key, *skip, keylen)) {
        if ((*skip)[keylen] != ':'){
            printf("%li: Missing `:` after decltype\n", line);
            exit(1);
        }
        *skip += keylen+1;
        while (**skip == ' ' || **skip == '\t')
            (*skip)++;
        
        if (!isalpha(**skip)) {
            printf("%li: Invalid identififer: %s\n", line, *skip);
            exit(1);
        } else {
            assembly_constants_add((globals) {
                .name   = util_strdup("empty"),
                .offset = code_globals_elements
            });
            return 1;
        }
    }
    return 0;
}

void asm_parse(FILE *fp) {
    char     *data  = NULL;
    char     *skip  = NULL;
    long      line  = 1; /* current line */
    size_t    size  = 0; /* size of line */
    asm_state state = ASM_NULL;
    
    while ((data = skip = asm_getline(&size, fp)) != NULL) {
        /* remove any whitespace at start  */
        while (*skip == ' ' || *skip == '\t')
            skip++;
        /* remove newline at end of string */
        *(skip+*(&size)-1) = '\0';
        
        if (asm_parsetype(asm_keys[5], &skip, line)) {
            if (state != ASM_NULL) {
                printf("%li: Error unfinished function block, expected DONE or RETURN\n", line);
                goto end;
            }
            state = ASM_FUNCTION;
            code_defs_add((prog_section_def){
                .type   = TYPE_VOID,
                .offset = code_globals_elements,
                .name   = code_chars_elements
            });
            code_globals_add(code_functions_elements);
            code_functions_add((prog_section_function) {
                .entry      =  code_statements_elements,      
                .firstlocal =  0,
                .locals     =  0,
                .profile    =  0,
                .name       =  code_chars_elements,
                .file       =  0,
                .nargs      =  0,
                .argsize    = {0}
            });
            code_strings_add(skip);
        };

        #if 0
        /* if we make it this far then we have statements */
        {
            size_t i = 0;    /* counter   */
            size_t o = 0;    /* operands  */
            size_t c = 0;    /* copy      */
            char  *t = NULL; /* token     */
            
            /*
             * Most ops a single statement can have is three.
             * lets allocate some space for all of those here.
             */
            char op[3][32768] = {{0},{0},{0}};
            for (; i < sizeof(asm_instr)/sizeof(*asm_instr); i++) {
                if (!strncmp(skip, asm_instr[i].m, asm_instr[i].l)) {
                    if (state != ASM_FUNCTION) {
                        printf("%li: Statement not inside function block\n", line);
                        goto end;
                    }
                    
                    /* update parser state */
                    if (i == INSTR_DONE || i == INSTR_RETURN) {
                        goto end;
                        state = ASM_NULL;
                    }
                    
                    /* parse the statement */
                    c     = i;
                    o     = asm_instr[i].o; /* operands         */
                    skip += asm_instr[i].l; /* skip instruction */
                    t     = strtok(skip, " ,");
                    i     = 0;
                    while (t != NULL && i < 3) {
                        strcpy(op[i], t);
                        t = strtok(NULL, " ,");
                        i ++;
                    }
                    
                    /* check */
                    if (i != o) {
                        printf("not enough operands, expected: %li, got %li\n", o, i);
                    }
                    
                    /* TODO: hashtable value LOAD .... etc */
                    code_statements_add((prog_section_statement){
                        c,
                        { atof(op[0]) },
                        { atof(op[1]) },
                        { atof(op[2]) }
                    });
                    goto end;
                }
            }
        }
        #endif
        
        /* if we made it this far something is wrong */
        if (*skip != '\0')
            printf("%li: Invalid statement %s, expression, or decleration\n", line, skip);
        
        end:
        mem_d(data);
        line ++;
    }
	asm_clear();
}
