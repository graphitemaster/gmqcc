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
    size_t  read = util_getline(&line, byte, fp);
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
static GMQCC_INLINE bool asm_parse_type(const char *skip, size_t line, asm_state *state) {
    if (!(strstr(skip, "FLOAT:")  == &skip[0]) &&
         (strstr(skip, "VECTOR:") == &skip[0]) &&
         (strstr(skip, "ENTITY:") == &skip[0]) &&
         (strstr(skip, "FIELD:")  == &skip[0]) &&
         (strstr(skip, "STRING:") == &skip[0])) return false;

    /* TODO: determine if constant, global, or local */
    switch (*skip) {
        /* VECTOR */ case 'V': {
            float val1;
            float val2;
            float val3;

            const char *find = skip + 7;
            while (*find == ' ' || *find == '\t') find++;

            /*
             * Parse all three elements of the vector.  This will only
             * pass the first try if we hit a constant, otherwise it's
             * a global.
             */
            #define PARSE_ELEMENT(X,Y,Z)                    \
                if (isdigit(*X)  || *X == '-'||*X == '+') { \
                    bool negated = (*X == '-');             \
                    if  (negated || *X == '+')   { X++; }   \
                    Y = (negated)?-atof(X):atof(X);         \
                    X = strchr(X, ',');                     \
                    Z                                       \
                }

            PARSE_ELEMENT(find, val1, { if(find) { find +=3; }});
            PARSE_ELEMENT(find, val2, { if(find) { find +=2; }});
            PARSE_ELEMENT(find, val3, { if(find) { find +=1; }});
            #undef PARSE_ELEMENT

            printf("X:[0] = %f\n", val1);
            printf("Y:[1] = %f\n", val2);
            printf("Z:[2] = %f\n", val3);

            break;
        }
        /* ENTITY */ case 'E': {
            const char *find = skip + 7;
            while (*find == ' ' || *find == '\t') find++;
            printf("found ENTITY %s\n", find);
            break;
        }
        /* STRING */ case 'S': {
            const char *find = skip + 7;
            while (*find == ' ' || *find == '\t') find++;
            printf("found STRING %s\n", find);
            break;
        }
    }

    return false;
}

/*
 * Parses a function: trivial case, handles occurances of duplicated
 * names among other things.  Ensures valid name as well, and even
 * internal engine function selection.
 */
static GMQCC_INLINE bool asm_parse_func(const char *skip, size_t line, asm_state *state) {
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
            prog_section_function function;
            prog_section_def      def;
            
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
            *strchr(name, ',')='\0';

            /*
             * Now add the following items to the code system:
             *  function
             *  definition (optional)
             *  global     (optional)
             *  name
             */
            function.entry      = -atoi(find);
            function.firstlocal = 0;
            function.profile    = 0;
            function.name       = code_chars_elements;
            function.file       = 0;
            function.nargs      = 0;
            def.type            = TYPE_FUNCTION;
            def.offset          = code_globals_elements;
            def.name            = code_chars_elements;
            code_functions_add(function);
            code_defs_add     (def);
            code_globals_add  (code_chars_elements);
            code_chars_put    (name, strlen(name));
            code_chars_add    ('\0');

            /*
             * Sanatize the numerical constant used to select the
             * internal function.  Must ensure it's all numeric, since
             * atoi can silently drop characters from a string and still
             * produce a valid constant that would lead to runtime problems.
             */
            if (util_strdigit(find))
                printf("found internal function %s, -%d\n", name, atoi(find));
            else
                printf("invalid internal function identifier, must be all numeric\n");

        } else {
            printf("Found function %s\n", name);
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
