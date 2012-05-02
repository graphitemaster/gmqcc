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
    char  type;   /* type, float, vector, string */
    char  elem;   /* 0=x, 1=y, or 2=Z?   */
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
 * Dumps all values of all constants and assembly related
 * information obtained during the assembly procedure.
 */
void asm_dumps() {
    size_t i = 0;
    for (; i < assembly_constants_elements; i++) {
        globals *g = &assembly_constants_data[i];
        switch (g->type) {
            case TYPE_VECTOR: {
                util_debug("ASM", "vector %s %c[%f]\n", g->name,
                    (g->elem == 0) ? 'X' :(
                    (g->elem == 1) ? 'Y' :
                    (g->elem == 2) ? 'Z' :' '),
                    INT2FLT(code_globals_data[g->offset])
                );
                break;
            }
        }
    }
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
            float   val1;
            float   val2;
            float   val3;
            globals global;

            char *find = (char*)skip + 7;
            char *name = (char*)skip + 7;
            while (*find == ' ' || *find == '\t') find++;

            /* constant? */
            if (strchr(find, ',')) {
                /* strip name */
                *strchr((name = util_strdup(find)), ',')='\0';
                /* find data  */
                find += strlen(name) + 1;
                while (*find == ' ' || *find == '\t') find++;
                /* valid name */
                if (util_strupper(name) || isdigit(*name)) {
                    printf("invalid name for vector variable\n");
                    mem_d(name);
                }
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

                PARSE_ELEMENT(find, val1, { find ++; while (*find == ' ') { find ++; } });
                PARSE_ELEMENT(find, val2, { find ++; while (*find == ' ') { find ++; } });
                PARSE_ELEMENT(find, val3, { find ++; /* no need to do anything here */ });
                #undef  PARSE_ELEMENT
                #define BUILD_ELEMENT(X,Y)                 \
                    global.type   = TYPE_VECTOR;           \
                    global.name   = util_strdup(name);     \
                    global.elem   = (X);                   \
                    global.offset = code_globals_elements; \
                    assembly_constants_add(global);        \
                    code_globals_add(FLT2INT(Y))
                BUILD_ELEMENT(0, val1);
                BUILD_ELEMENT(1, val2);
                BUILD_ELEMENT(2, val3);
                #undef  BUILD_ELEMENT
                mem_d(name);
            } else {
                /* TODO global not constant */
            }
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
    if (*state == ASM_FUNCTION)
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
            function.locals     = 0;
            function.profile    = 0;
            function.name       = code_chars_elements;
            function.file       = 0;
            function.nargs      = 0;
            def.type            = TYPE_FUNCTION;
            def.offset          = code_globals_elements;
            def.name            = code_chars_elements;
            memset(function.argsize, 0, sizeof(function.argsize));
            code_functions_add(function);
            code_defs_add     (def);
            code_chars_put    (name, strlen(name));
            code_chars_add    ('\0');
            
            util_debug("ASM", "added internal function %s to function table\n", name);

            /*
             * Sanatize the numerical constant used to select the
             * internal function.  Must ensure it's all numeric, since
             * atoi can silently drop characters from a string and still
             * produce a valid constant that would lead to runtime problems.
             */
            if (util_strdigit(find))
                util_debug("ASM", "found internal function %s, -%d\n", name, atoi(find));
            else
                printf("invalid internal function identifier, must be all numeric\n");

        } else {
            /*
             * The function isn't an internal one. Determine the name and
             * amount of arguments the function accepts by searching for
             * the `#` (pound sign).
             */
            int   args = 0;
            char *find = strchr(name, '#');
            char *peek = find;
            
            /*
             * Code structures for filling after determining the correct
             * information to add to the code write system.
             */
            prog_section_function function;
            prog_section_def      def;
            if (find) {
                find ++;

                /* skip whitespace */
                if (*find == ' ' || *find == '\t')
                    find++;

                /*
                 * If the input is larger than eight, it's considered
                 * invalid and shouldn't be allowed.  The QuakeC VM only
                 * allows a maximum of eight arguments.
                 */
                if (strlen(find) > 1 || *find == '9') {
                    printf("invalid number of arguments, must be a valid number from 0-8\n");
                    mem_d(copy);
                    mem_d(name);
                    return false;
                }

                if (*find != '0') {
                    /*
                     * if we made it this far we have a valid number for the
                     * argument count, so fall through a switch statement and
                     * do it.
                     */
                    switch (*find) {
                        case '8': args++; case '7': args++;
                        case '6': args++; case '5': args++;
                        case '4': args++; case '3': args++;
                        case '2': args++; case '1': args++;
                    }
                }
            } else {
                printf("missing number of argument count in function %s\n", name);
            }

            /*
             * Now we need to strip the name apart into it's exact size
             * by working in the peek buffer till we hit the name again.
             */
            if (*peek == '#') {
                peek --; /* '#'    */
                peek --; /* number */
            }
            while (*peek == ' ' || *peek == '\t') peek--;

            /*
             * We're guranteed to be exactly where we need to be in the
             * peek buffer to null terminate and get our name from name
             * without any garbage before or after it.
             */
            *++peek='\0';

            /*
             * We got valid function structure information now. Lets add
             * the function to the code writer function table.
             */
            function.entry      = code_statements_elements-1;
            function.firstlocal = 0;
            function.locals     = 0;
            function.profile    = 0;
            function.name       = code_chars_elements;
            function.file       = 0;
            function.nargs      = args;
            def.type            = TYPE_FUNCTION;
            def.offset          = code_globals_elements;
            def.name            = code_chars_elements;
            memset(function.argsize, 0, sizeof(function.argsize));
            code_functions_add(function);
            code_globals_add(code_statements_elements);
            code_chars_put    (name, strlen(name));
            code_chars_add    ('\0');

            /* update assembly state */
            
            *state = ASM_FUNCTION;
            util_debug("ASM", "added context function %s to function table\n", name);
        }
        
        mem_d(copy);
        mem_d(name);
        return true;
    }
    return false;
}

static GMQCC_INLINE bool asm_parse_stmt(const char *skip, size_t line, asm_state *state) {
    /*
     * This parses a valid statement in assembly and adds it to the code
     * table to be wrote.  This needs to handle correct checking of all
     * statements to ensure the correct amount of operands are passed to
     * the menomic.  This must also check for valid function calls (ensure
     * the names selected exist in the program scope) and ensure the correct
     * CALL* is used (depending on the amount of arguments the function
     * is expected to take)
     */
    char                  *c = (char*)skip;
    prog_section_statement s;
    size_t                 i = 0;

    /*
     * statements are only allowed when inside a function body
     * otherwise the assembly is invalid.
     */
    if (*state != ASM_FUNCTION)
        return false;

    /*
     * Skip any possible whitespace, it's not wanted we're searching
     * for an instruction.  TODO: recrusive decent parser skip on line
     * entry instead of pre-op.
     */
    while (*skip == ' ' || *skip == '\t')
        skip++;
    
    for (; i < sizeof(asm_instr)/sizeof(*asm_instr); i++) {
        /*
         * Iterate all possible instructions and check if the selected
         * instructure in the input stream `skip` is actually a valid
         * instruction.
         */
        if (!strncmp(skip, asm_instr[i].m, asm_instr[i].l)) {
            printf("found statement %s\n", asm_instr[i].m);
            /*
             * Parse the operands for `i` (the instruction). The order
             * of asm_instr is in the order of the menomic encoding so
             * `i` == menomic encoding.
             */
            s.opcode = i;
            switch (asm_instr[i].o) {
                /*
                 * Each instruction can have from 0-3 operands; and can
                 * be used with less or more operands depending on it's
                 * selected use.
                 * 
                 * DONE for example can use either 0 operands, or 1 (to
                 * emulate the effect of RETURN)
                 *
                 * TODO: parse operands correctly figure out what it is
                 * that the assembly is trying to do, i.e string table
                 * lookup, function calls etc.
                 *
                 * This needs to have a fall state, we start from the
                 * end of the string and work backwards.
                 */
                #define OPFILL(X)                                      \
                    do {                                               \
                        size_t w = 0;                                  \
                        if (!(c = strrchr(c, ','))) {                  \
                            printf("error, expected more operands\n"); \
                            return false;                              \
                        }                                              \
                        c++;                                           \
                        w++;                                           \
                        while (*c == ' ' || *c == '\t') {              \
                            c++;                                       \
                            w++;                                       \
                        }                                              \
                        X  = (const char*)c;                           \
                        c -= w;                                        \
                       *c  = '\0';                                     \
                        c  = (char*)skip;                              \
                    } while (0)
                    
                case 3: {
                    const char *data; OPFILL(data);
                    printf("OP3: %s\n", data);
                    s.o3.s1 = 0;
                }
                case 2: {
                    const char *data; OPFILL(data);
                    printf("OP2: %s\n", data);
                    s.o2.s1 = 0;
                }
                case 1: {
                    while (*c == ' ' || *c == '\t') c++;
                    c += asm_instr[i].l;
                    while (*c == ' ' || *c == '\t') c++;
                    
                    printf("OP1: %s\n", c);
                    s.o1.s1 = 0;
                }
                #undef OPFILL
            }
            /* add the statement now */
            code_statements_add(s);
        }
    }
    return true;
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

        /* TODO: statement END check */
        if (state == ASM_FUNCTION)
            state =  ASM_NULL;

        if (asm_parse_type(skip, line, &state)){ asm_end("asm_parse_type\n"); }
        if (asm_parse_func(skip, line, &state)){ asm_end("asm_parse_func\n"); }
        if (asm_parse_stmt(skip, line, &state)){ asm_end("asm_parse_stmt\n"); }
    }
    #undef asm_end
    asm_dumps();
    asm_clear();
}
