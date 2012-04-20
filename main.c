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
// todo CLEANUP this argitem thing
typedef struct { char *name, type; } argitem;
VECTOR_MAKE(argitem, items);

/* global options */
int opts_debug  = 0;
int opts_memchk = 0;

static const int usage(const char *const app) {
    printf("usage:\n");
    printf("    %s -c<file> -- compile file\n" , app);
    printf("    %s -a<file> -- assemble file\n", app);
    printf("    additional flags:\n");
    printf("        -debug  -- turns on compiler debug messages\n");
    printf("        -memchk -- turns on compiler memory leak check\n");
    
    return -1;
}

int main(int argc, char **argv) {
    size_t itr = 0;
    char  *app = &argv[0][0];
    FILE  *fpp = NULL;

    /*
     * Parse all command line arguments.  This is rather annoying to do
     * because of all tiny corner cases.
     */
    if (argc <= 1 || (argv[1][0] != '-'))
        return usage(app);

    while ((argc > 1) && argv[1][0] == '-') {
        switch (argv[1][1]) {
            case 'c': items_add((argitem){util_strdup(&argv[1][2]), 0}); break; /* compile  */ 
            case 'a': items_add((argitem){util_strdup(&argv[1][2]), 1}); break; /* assemble */
            default:
                if (!strncmp(&argv[1][1], "debug" , 5)) { opts_debug  = 1; break; }
                if (!strncmp(&argv[1][1], "memchk", 6)) { opts_memchk = 1; break; }
                return usage(app);
                
        }
        ++argv;
        --argc;
    }

    /*
     * options could depend on another option, this is where option
     * validity checking like that would take place.
     */
    if (opts_memchk && !opts_debug) 
        printf("Warning: cannot enable -memchk, without -debug.\n");

    /* multi file multi path compilation system */
    for (; itr < items_elements; itr++) {
        switch (items_data[itr].type) {
            case 0:
                fpp = fopen(items_data[itr].name, "r");
                struct lex_file *lex = lex_open(fpp);
                parse_gen(lex);
                lex_close(lex);
                break;
            case 1:
                asm_init (items_data[itr].name, &fpp);
                asm_parse(fpp);
                asm_close(fpp);
                break;
        }
    }
    
    /* clean list */
    for (itr = 0; itr < items_elements; itr++)
        mem_d(items_data[itr].name);
    mem_d(items_data);

    if (opts_memchk)
        util_meminfo();
    return 0;
}
