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

void asm_parse(FILE *fp) {
    char     *data  = NULL;
    long      line  = 1; /* current line */
    size_t    size  = 0; /* size of line */
    asm_state state = ASM_NULL;
    
    while ((data = asm_getline(&size, fp)) != NULL) {
        data = util_strsws(data); /* skip   whitespace */
        data = util_strrnl(data); /* delete newline    */

        /* TODO: everything */
        (void)state;
        goto end;
    end:
        mem_d(data);
        line ++;
    }
	asm_clear();
}
