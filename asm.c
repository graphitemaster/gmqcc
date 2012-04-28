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

/*
 * Assembly text processing: this handles the internal collection
 * of text to allow parsing and assemblation.
 */
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

/*
 * Entire external interface for main.c - to perform actual assemblation
 * of assembly files.
 */
void asm_init(const char *file, FILE **fp) {
    *fp = fopen(file, "r");
    code_init();
}
void asm_close(FILE *fp) {
    fclose(fp);
    code_write();
}
void asm_clear() {
    size_t i = 0;
    for (; i < assembly_constants_elements; i++)
        mem_d(assembly_constants_data[i].name);
    mem_d(assembly_constants_data);
}

/*
 * Parses a type, could be global or not depending on the
 * assembly state: global scope with assignments are constants.
 * globals with no assignments are globals.  Function body types
 * are locals.
 */
static inline bool asm_parse_type(const char *skip, size_t line, asm_state *state) {
    if (strstr(skip, "FLOAT:")  == &skip[0]) { return true; }
    if (strstr(skip, "VECTOR:") == &skip[0]) { return true; }
    if (strstr(skip, "ENTITY:") == &skip[0]) { return true; }
    if (strstr(skip, "FIELD:")  == &skip[0]) { return true; }
    if (strstr(skip, "STRING:") == &skip[0]) { return true; }
    return false;
}

/*
 * Parses a function: trivial case, handles occurances of duplicated
 * names among other things.  Ensures valid name as well, and even
 * internal engine function selection.
 */
static inline bool asm_parse_func(const char *skip, size_t line, asm_state *state) {
    if (*state == ASM_FUNCTION && (strstr(skip, "FUNCTION:") == &skip[0]))
        return false;

    if (strstr(skip, "FUNCTION:") == &skip[0]) {
        char  *copy = util_strsws(skip+10);
        char  *name = util_strchp(copy, strchr(copy, '\0'));

        /* TODO: failure system, missing name */
        if (!name) {
            printf("expected name on function\n");
            mem_d(copy);
            mem_d(name);
            return false;
        }
        /* TODO: failure system, invalid name */
        if (!isalpha(*name) || util_strupper(name)) {
            printf("invalid identifer for function name\n");
            mem_d(copy);
            mem_d(name);
            return false;
        }

        /*
         * Function could be internal function, look for $
         * to determine this.
         */
        if (strchr(name, ',')) {
            char *find = strchr(name, ',') + 1;
            
            /* skip whitespace */
            while (*find == ' ' || *find == '\t')
                find++;
            
            if (*find != '$') {
                printf("expected $ for internal function selection, got %s instead\n", find);
                mem_d(copy);
                mem_d(name);
                return false;
            }
            find ++;
            if (!isdigit(*find)) {
                printf("invalid internal identifier, expected valid number\n");
                mem_d(copy);
                mem_d(name);
                return false;
            }
            /* reassign name */
            mem_d(name);
            name = util_strchp(name, strchr(name, ','));

            /*
             * Now add the following items to the code system:
             *  function
             *  definition (optional)
             *  global     (optional)
             *  name
             */
            code_functions_add((prog_section_function){
                -atoi(find), /* needs to be negated */
                 0, 0, 0,
                .name = code_chars_elements,
                 0, 0,{0}
            });
            code_defs_add((prog_section_def){
                .type   = TYPE_FUNCTION,
                .offset = code_globals_elements,
                .name   = code_chars_elements
            });
            code_globals_add(code_chars_elements);
            
            code_chars_put(name, strlen(name));
            code_chars_add('\0');

            /* TODO: sanatize `find` to ensure all numerical digits */
            
            printf("found internal function %s, -%d\n", name, atoi(find));
        } else {
        }

        mem_d(copy);
        mem_d(name);
        return true;
    }
    return false;
}

void asm_parse(FILE *fp) {
    char     *data  = NULL;
    char     *skip  = NULL;
    long      line  = 1; /* current line */
    size_t    size  = 0; /* size of line */
    asm_state state = ASM_NULL;

    #define asm_end(x)            \
        do {                      \
            mem_d(data);          \
            mem_d(copy);          \
            line++;               \
            util_debug("ASM", x); \
        } while (0); continue
    
    while ((data = asm_getline (&size, fp)) != NULL) {
        char *copy = util_strsws(data); /* skip   whitespace */
              skip = util_strrnl(copy); /* delete newline    */

        /* parse type */
        if(asm_parse_type(skip, line, &state)){ asm_end("asm_parse_type\n"); }
        /* parse func */
        if(asm_parse_func(skip, line, &state)){ asm_end("asm_parse_func\n"); }

        /* statement closure */
        if (state == ASM_FUNCTION && (
            (strstr(skip, "DONE")   == &skip[0])||
            (strstr(skip, "RETURN") == &skip[0]))) state = ASM_NULL;
        
        /* TODO: everything */
        (void)state;
        asm_end("asm_parse_end\n");
    }
    #undef asm_end
	asm_clear();
}
