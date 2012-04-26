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
bool opts_debug                     = false;
bool opts_memchk                    = false;
bool opts_darkplaces_stringtablebug = false;
bool opts_omit_nullcode             = false;
int  opts_compiler                  = COMPILER_GMQCC;

static const int usage(const char *const app) {
    printf("usage:\n");
    printf("    %s -c<file>          -oprog.dat -- compile file\n"     , app);
    printf("    %s -a<file>          -oprog.dat -- assemble file\n"    , app);
    printf("    %s -c<file> -i<file> -oprog.dat -- compile together (allowed multiple -i<file>)\n" , app);
    printf("    %s -a<file> -i<file> -oprog.dat -- assemble together(allowed multiple -i<file>)\n", app);
    printf("    example:\n");
    printf("    %s -cfoo.qc -ibar.qc -oqc.dat -afoo.qs -ibar.qs -oqs.dat\n", app);
    printf("    additional flags:\n");
    printf("        -debug           -- turns on compiler debug messages\n");
    printf("        -memchk          -- turns on compiler memory leak check\n");
    printf("        -help            -- prints this help/usage text\n");
    printf("        -std             -- select the QuakeC compile type (types below):\n");
    printf("            -std=qcc     -- original QuakeC\n");
    printf("            -std=ftqecc  -- fteqcc QuakeC\n");
    printf("            -std=qccx    -- qccx QuakeC\n");
    printf("            -std=gmqcc   -- this compiler QuakeC (default selection)\n");
    printf("    codegen flags:\n");
    printf("        -fdarkplaces-string-table-bug -- patches the string table to work with bugged versions of darkplaces\n");
    printf("        -fomit-nullcode               -- omits the generation of null code (will break everywhere see propsal.txt)\n");
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
            case 'i': items_add((argitem){util_strdup(&argv[1][2]), 2}); break; /* includes */
            default:
                if (!strncmp(&argv[1][1], "debug" , 5)) { opts_debug  = true; break; }
                if (!strncmp(&argv[1][1], "memchk", 6)) { opts_memchk = true; break; }
                if (!strncmp(&argv[1][1], "help",   4)) {
                    return usage(app);
                    break;
                }
                /* compiler type selection */
                if (!strncmp(&argv[1][1], "std=qcc"   , 7 )) { opts_compiler = COMPILER_QCC;    break; }
                if (!strncmp(&argv[1][1], "std=fteqcc", 10)) { opts_compiler = COMPILER_FTEQCC; break; }
                if (!strncmp(&argv[1][1], "std=qccx",   8 )) { opts_compiler = COMPILER_QCCX;   break; }
                if (!strncmp(&argv[1][1], "std=gmqcc",  9 )) { opts_compiler = COMPILER_GMQCC;  break; }
                if (!strncmp(&argv[1][1], "std=",       4 )) {
                    printf("invalid std selection, supported types:\n");
                    printf("    -std=qcc     -- original QuakeC\n");
                    printf("    -std=ftqecc  -- fteqcc QuakeC\n");
                    printf("    -std=qccx    -- qccx QuakeC\n");
                    printf("    -std=gmqcc   -- this compiler QuakeC (default selection)\n");
                    return 0;
                }

                /* code specific switches */
                if (!strcmp(&argv[1][1], "fdarkplaces-stringtablebug")) {
                    opts_darkplaces_stringtablebug = true;
                    break;
                }
                if (!strcmp(&argv[1][1], "fomit-nullcode")) {
                    opts_omit_nullcode = true;
                    break;
                }
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

    util_debug("COM", "starting ...\n");
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

    util_debug("COM", "cleaning ...\n"); 
    /* clean list */
    for (itr = 0; itr < items_elements; itr++)
        mem_d(items_data[itr].name);
    mem_d(items_data);

    util_meminfo();
    return 0;
}
