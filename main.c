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
typedef struct { char *name, type; } argitem;
VECTOR_MAKE(argitem, items);

static const int usage(const char *const app) {
    printf("usage:\n"
           "    %s -c<file>          -oprog.dat -- compile file\n"
           "    %s -a<file>          -oprog.dat -- assemble file\n"
           "    %s -c<file> -i<file> -oprog.dat -- compile together (allowed multiple -i<file>)\n"
           "    %s -a<file> -i<file> -oprog.dat -- assemble together(allowed multiple -i<file>)\n"
           "    example:\n"
           "    %s -cfoo.qc -ibar.qc -oqc.dat -afoo.qs -ibar.qs -oqs.dat\n", app, app, app, app, app);

    printf("    additional flags:\n"
           "        -debug           -- turns on compiler debug messages\n"
           "        -memchk          -- turns on compiler memory leak check\n"
           "        -help            -- prints this help/usage text\n"
           "        -std             -- select the QuakeC compile type (types below):\n");

    printf("            -std=qcc     -- original QuakeC\n"
           "            -std=ftqecc  -- fteqcc QuakeC\n"
           "            -std=qccx    -- qccx QuakeC\n"
           "            -std=gmqcc   -- this compiler QuakeC (default selection)\n");

    printf("    codegen flags:\n"
           "        -fdarkplaces-string-table-bug -- patches the string table to work with bugged versions of darkplaces\n"
           "        -fomit-nullcode               -- omits the generation of null code (will break everywhere see propsal.txt)\n");
    return -1;
}

int main(int argc, char **argv) {
    size_t    itr = 0;
    char     *app = &argv[0][0];
    FILE     *fpp = NULL;
    lex_file *lex = NULL;

    /*
     * Parse all command line arguments.  This is rather annoying to do
     * because of all tiny corner cases.
     */
    if (argc <= 1 || (argv[1][0] != '-'))
        return usage(app);

    while ((argc > 1) && argv[1][0] == '-') {
        switch (argv[1][1]) {
            case 'v': {
                printf("GMQCC:\n"
                       "    version:    %d.%d.%d (0x%08X)\n"
                       "    build date: %s\n"
                       "    build time: %s\n",
                    (GMQCC_VERSION >> 16) & 0xFF,
                    (GMQCC_VERSION >>  8) & 0xFF,
                    (GMQCC_VERSION >>  0) & 0xFF,
                    (GMQCC_VERSION),
                    __DATE__,
                    __TIME__
                );
                return 0;
            }
            #define param_argument(argtype) do {                             \
                argitem item;                                                \
                if (argv[1][2]) {                                            \
                    item.name = util_strdup(&argv[1][2]);                    \
                    item.type = argtype;                                     \
                    items_add(item);                                         \
                } else {                                                     \
                    ++argv;                                                  \
                    --argc;                                                  \
                    if (argc <= 1)                                           \
                        goto clean_params_usage;                             \
                    item.name = util_strdup(argv[1]);                        \
                    item.type = argtype;                                     \
                    items_add(item);                                         \
                }                                                            \
            } while (0)

            case 'c': { param_argument(0); break; } /* compile  */
            case 'a': { param_argument(1); break; } /* assemble */
            case 'i': { param_argument(2); break; } /* includes */
            #undef parm_argument
            default:
                if (util_strncmpexact(&argv[1][1], "debug" , 5)) { opts_debug  = true; break; }
                if (util_strncmpexact(&argv[1][1], "memchk", 6)) { opts_memchk = true; break; }
                if (util_strncmpexact(&argv[1][1], "help",   4)) {
                    return usage(app);
                    break;
                }
                /* compiler type selection */
                if (util_strncmpexact(&argv[1][1], "std=qcc"   , 7 )) { opts_compiler = COMPILER_QCC;    break; }
                if (util_strncmpexact(&argv[1][1], "std=fteqcc", 10)) { opts_compiler = COMPILER_FTEQCC; break; }
                if (util_strncmpexact(&argv[1][1], "std=qccx",   8 )) { opts_compiler = COMPILER_QCCX;   break; }
                if (util_strncmpexact(&argv[1][1], "std=gmqcc",  9 )) { opts_compiler = COMPILER_GMQCC;  break; }
                if (util_strncmpexact(&argv[1][1], "std=",       4 )) {
                    printf("invalid std selection, supported types:\n"
                           "    -std=qcc     -- original QuakeC\n"
                           "    -std=ftqecc  -- fteqcc QuakeC\n"
                           "    -std=qccx    -- qccx QuakeC\n"
                           "    -std=gmqcc   -- this compiler QuakeC (default selection)\n");
                    return 0;
                }

                /* code specific switches */
                if (util_strncmpexact(&argv[1][1], "fdarkplaces-stringtablebug", 26)) {
                    opts_darkplaces_stringtablebug = true;
                    break;
                }
                if (util_strncmpexact(&argv[1][1], "fomit-nullcode", 14)) {
                    opts_omit_nullcode = true;
                    break;
                }
                return printf("invalid command line argument: %s\n", argv[1]);

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
                lex_init (items_data[itr].name, &lex);
                if (lex) {
                    lex_parse(lex);
                    lex_close(lex);
                }
                break;
            case 1:
                asm_init (items_data[itr].name, &fpp);
                if (fpp) {
                    asm_parse(fpp);
                    asm_close(fpp);
                }
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

clean_params_usage:
    for (itr = 0; itr < items_elements; itr++)
        mem_d(items_data[itr].name);
    mem_d(items_data);
    return usage(app);
}
