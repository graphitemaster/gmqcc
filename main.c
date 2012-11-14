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
#include "lexer.h"

uint32_t    opts_flags[1 + (COUNT_FLAGS / 32)];
uint32_t    opts_warn [1 + (COUNT_WARNINGS / 32)];

uint32_t    opts_O        = 1;
const char *opts_output   = "progs.dat";
int         opts_standard = COMPILER_GMQCC;
bool        opts_debug    = false;
bool        opts_memchk   = false;
bool        opts_dump     = false;
bool        opts_werror   = false;
bool        opts_forcecrc = false;
bool        opts_pp_only  = false;

uint16_t    opts_forced_crc;

static bool opts_output_wasset = false;

/* set by the standard */
const oper_info *operators      = NULL;
size_t           operator_count = 0;

typedef struct { char *filename; int type; } argitem;
VECTOR_MAKE(argitem, items);

#define TYPE_QC  0
#define TYPE_ASM 1
#define TYPE_SRC 2

static const char *app_name;

static int usage() {
    con_out("usage: %s [options] [files...]", app_name);
    con_out("options:\n"
           "  -h, --help             show this help message\n"
           "  -debug                 turns on compiler debug messages\n"
           "  -memchk                turns on compiler memory leak check\n");
    con_out("  -o, --output=file      output file, defaults to progs.dat\n"
           "  -a filename            add an asm file to be assembled\n"
           "  -s filename            add a progs.src file to be used\n");
    con_out("  -E                     stop after preprocessing\n");
    con_out("  -f<flag>               enable a flag\n"
           "  -fno-<flag>            disable a flag\n"
           "  -std standard          select one of the following standards\n"
           "       -std=qcc          original QuakeC\n"
           "       -std=fteqcc       fteqcc QuakeC\n"
           "       -std=gmqcc        this compiler (default)\n");
    con_out("  -W<warning>            enable a warning\n"
           "  -Wno-<warning>         disable a warning\n"
           "  -Wall                  enable all warnings\n"
           "  -Werror                treat warnings as errors\n");
    con_out("  -force-crc=num         force a specific checksum into the header\n");
    con_out("\n");
    con_out("flags:\n"
           "  -fadjust-vector-fields\n"
           "            when assigning a vector field, its _y and _z fields also get assigned\n"
           );
    return -1;
}

static bool options_setflag_all(const char *name, bool on, uint32_t *flags, const opts_flag_def *list, size_t listsize) {
    size_t i;

    for (i = 0; i < listsize; ++i) {
        if (!strcmp(name, list[i].name)) {
            longbit lb = list[i].bit;
#if 0
            if (on)
                flags[lb.idx] |= (1<<(lb.bit));
            else
                flags[lb.idx] &= ~(1<<(lb.bit));
#else
            if (on)
                flags[0] |= (1<<lb);
            else
                flags[0] &= ~(1<<(lb));
#endif
            return true;
        }
    }
    return false;
}
static bool options_setflag(const char *name, bool on) {
    return options_setflag_all(name, on, opts_flags, opts_flag_list, COUNT_FLAGS);
}
static bool options_setwarn(const char *name, bool on) {
    return options_setflag_all(name, on, opts_warn, opts_warn_list, COUNT_WARNINGS);
}

static bool options_witharg(int *argc_, char ***argv_, char **out) {
    int  argc   = *argc_;
    char **argv = *argv_;

    if (argv[0][2]) {
        *out = argv[0]+2;
        return true;
    }
    /* eat up the next */
    if (argc < 2) /* no parameter was provided */
        return false;

    *out = argv[1];
    --*argc_;
    ++*argv_;
    return true;
}

static bool options_long_witharg_all(const char *optname, int *argc_, char ***argv_, char **out, int ds, bool split) {
    int  argc   = *argc_;
    char **argv = *argv_;

    size_t len = strlen(optname);

    if (strncmp(argv[0]+ds, optname, len))
        return false;

    /* it's --optname, check how the parameter is supplied */
    if (argv[0][ds+len] == '=') {
        /* using --opt=param */
        *out = argv[0]+ds+len+1;
        return true;
    }

    if (!split || argc < ds) /* no parameter was provided, or only single-arg form accepted */
        return false;

    /* using --opt param */
    *out = argv[1];
    --*argc_;
    ++*argv_;
    return true;
}
static bool options_long_witharg(const char *optname, int *argc_, char ***argv_, char **out) {
    return options_long_witharg_all(optname, argc_, argv_, out, 2, true);
}
static bool options_long_gcc(const char *optname, int *argc_, char ***argv_, char **out) {
    return options_long_witharg_all(optname, argc_, argv_, out, 1, false);
}

static bool options_parse(int argc, char **argv) {
    bool argend = false;
    size_t itr;
    char buffer[1024];
    while (!argend && argc > 1) {
        char *argarg;
        argitem item;

        ++argv;
        --argc;

        if (argv[0][0] == '-') {
    /* All gcc-type long options */
            if (options_long_gcc("std", &argc, &argv, &argarg)) {
                if      (!strcmp(argarg, "gmqcc") || !strcmp(argarg, "default"))
                    opts_standard = COMPILER_GMQCC;
                else if (!strcmp(argarg, "qcc"))
                    opts_standard = COMPILER_QCC;
                else if (!strcmp(argarg, "fte") || !strcmp(argarg, "fteqcc"))
                    opts_standard = COMPILER_FTEQCC;
                else if (!strcmp(argarg, "qccx"))
                    opts_standard = COMPILER_QCCX;
                else {
                    con_out("Unknown standard: %s\n", argarg);
                    return false;
                }
                continue;
            }
            if (options_long_gcc("force-crc", &argc, &argv, &argarg)) {
                opts_forcecrc = true;
                opts_forced_crc = strtol(argarg, NULL, 0);
                continue;
            }
            if (!strcmp(argv[0]+1, "debug")) {
                opts_debug = true;
                continue;
            }
            if (!strcmp(argv[0]+1, "dump")) {
                opts_dump = true;
                continue;
            }
            if (!strcmp(argv[0]+1, "memchk")) {
                opts_memchk = true;
                continue;
            }

            switch (argv[0][1]) {
                /* -h, show usage but exit with 0 */
                case 'h':
                    usage();
                    exit(0);
                    break;

                case 'E':
                    opts_pp_only = true;
                    break;

                /* handle all -fflags */
                case 'f':
                    util_strtocmd(argv[0]+2, argv[0]+2, strlen(argv[0]+2)+1);
                    if (!strcmp(argv[0]+2, "HELP")) {
                        con_out("Possible flags:\n");
                        for (itr = 0; itr < COUNT_FLAGS; ++itr) {
                            util_strtononcmd(opts_flag_list[itr].name, buffer, sizeof(buffer));
                            con_out(" -f%s\n", buffer);
                        }
                        exit(0);
                    }
                    else if (!strncmp(argv[0]+2, "NO_", 3)) {
                        if (!options_setflag(argv[0]+5, false)) {
                            con_out("unknown flag: %s\n", argv[0]+2);
                            return false;
                        }
                    }
                    else if (!options_setflag(argv[0]+2, true)) {
                        con_out("unknown flag: %s\n", argv[0]+2);
                        return false;
                    }
                    break;
                case 'W':
                    util_strtocmd(argv[0]+2, argv[0]+2, strlen(argv[0]+2)+1);
                    if (!strcmp(argv[0]+2, "HELP")) {
                        con_out("Possible warnings:\n");
                        for (itr = 0; itr < COUNT_WARNINGS; ++itr) {
                            util_strtononcmd(opts_warn_list[itr].name, buffer, sizeof(buffer));
                            con_out(" -W%s\n", buffer);
                        }
                        exit(0);
                    }
                    else if (!strcmp(argv[0]+2, "NO_ERROR")) {
                        opts_werror = false;
                        break;
                    }
                    else if (!strcmp(argv[0]+2, "ERROR")) {
                        opts_werror = true;
                        break;
                    }
                    else if (!strcmp(argv[0]+2, "NONE")) {
                        for (itr = 0; itr < sizeof(opts_warn)/sizeof(opts_warn[0]); ++itr)
                            opts_warn[itr] = 0;
                        break;
                    }
                    else if (!strcmp(argv[0]+2, "ALL")) {
                        for (itr = 0; itr < sizeof(opts_warn)/sizeof(opts_warn[0]); ++itr)
                            opts_warn[itr] = 0xFFFFFFFFL;
                        break;
                    }
                    if (!strncmp(argv[0]+2, "NO_", 3)) {
                        if (!options_setwarn(argv[0]+5, false)) {
                            con_out("unknown warning: %s\n", argv[0]+2);
                            return false;
                        }
                    }
                    else if (!options_setwarn(argv[0]+2, true)) {
                        con_out("unknown warning: %s\n", argv[0]+2);
                        return false;
                    }
                    break;

                case 'O':
                    if (!options_witharg(&argc, &argv, &argarg)) {
                        con_out("option -O requires a numerical argument\n");
                        return false;
                    }
                    opts_O = atoi(argarg);
                    break;

                case 'o':
                    if (!options_witharg(&argc, &argv, &argarg)) {
                        con_out("option -o requires an argument: the output file name\n");
                        return false;
                    }
                    opts_output = argarg;
                    opts_output_wasset = true;
                    break;

                case 'a':
                case 's':
                    item.type = argv[0][1] == 'a' ? TYPE_ASM : TYPE_SRC;
                    if (!options_witharg(&argc, &argv, &argarg)) {
                        con_out("option -a requires a filename %s\n",
                                (argv[0][1] == 'a' ? "containing QC-asm" : "containing a progs.src formatted list"));
                        return false;
                    }
                    item.filename = argarg;
                    items_add(item);
                    break;

                case '-':
                    if (!argv[0][2]) {
                        /* anything following -- is considered a non-option argument */
                        argend = true;
                        break;
                    }
            /* All long options without arguments */
                    else if (!strcmp(argv[0]+2, "help")) {
                        usage();
                        exit(0);
                    }
                    else {
            /* All long options with arguments */
                        if (options_long_witharg("output", &argc, &argv, &argarg)) {
                            opts_output = argarg;
                            opts_output_wasset = true;
                        } else {
                            con_out("Unknown parameter: %s\n", argv[0]);
                            return false;
                        }
                    }
                    break;

                default:
                    con_out("Unknown parameter: %s\n", argv[0]);
                    return false;
            }
        }
        else
        {
            /* it's a QC filename */
            argitem item;
            item.filename = argv[0];
            item.type     = TYPE_QC;
            items_add(item);
        }
    }
    return true;
}

static void options_set(uint32_t *flags, size_t idx, bool on)
{
    longbit lb = LONGBIT(idx);
#if 0
    if (on)
        flags[lb.idx] |= (1<<(lb.bit));
    else
        flags[lb.idx] &= ~(1<<(lb.bit));
#else
    if (on)
        flags[0] |= (1<<(lb));
    else
        flags[0] &= ~(1<<(lb));
#endif
}

/* returns the line number, or -1 on error */
static bool progs_nextline(char **out, size_t *alen,FILE *src)
{
    int    len;
    char  *line;
    char  *start;
    char  *end;

    line = *out;
    len = util_getline(&line, alen, src);
    if (len == -1)
        return false;

    /* start at first non-blank */
    for (start = line; isspace(*start); ++start) {}
    /* end at the first non-blank */
    for (end = start;  *end && !isspace(*end);  ++end)   {}

    *out = line;
    /* move the actual filename to the beginning */
    while (start != end) {
        *line++ = *start++;
    }
    *line = 0;
    return true;
}

int main(int argc, char **argv) {
    size_t itr;
    int retval = 0;
    bool opts_output_free = false;

    app_name = argv[0];
    con_init();

    /* default options / warn flags */
    options_set(opts_warn, WARN_UNKNOWN_CONTROL_SEQUENCE, true);
    options_set(opts_warn, WARN_EXTENSIONS, true);
    options_set(opts_warn, WARN_FIELD_REDECLARED, true);
    options_set(opts_warn, WARN_TOO_FEW_PARAMETERS, true);
    options_set(opts_warn, WARN_MISSING_RETURN_VALUES, true);
    options_set(opts_warn, WARN_USED_UNINITIALIZED, true);
    options_set(opts_warn, WARN_LOCAL_CONSTANTS, true);
    options_set(opts_warn, WARN_VOID_VARIABLES, true);
    options_set(opts_warn, WARN_IMPLICIT_FUNCTION_POINTER, true);
    options_set(opts_warn, WARN_VARIADIC_FUNCTION, true);
    options_set(opts_warn, WARN_FRAME_MACROS, true);
    options_set(opts_warn, WARN_UNUSED_VARIABLE, true);
    options_set(opts_warn, WARN_EFFECTLESS_STATEMENT, true);
    options_set(opts_warn, WARN_END_SYS_FIELDS, true);
    options_set(opts_warn, WARN_ASSIGN_FUNCTION_TYPES, true);

    if (!options_parse(argc, argv)) {
        return usage();
    }

    /* the standard decides which set of operators to use */
    if (opts_standard == COMPILER_GMQCC) {
        operators = c_operators;
        operator_count = c_operator_count;
    } else {
        operators = qcc_operators;
        operator_count = qcc_operator_count;
    }

    if (opts_dump) {
        for (itr = 0; itr < COUNT_FLAGS; ++itr) {
            con_out("Flag %s = %i\n", opts_flag_list[itr].name, OPTS_FLAG(itr));
        }
        for (itr = 0; itr < COUNT_WARNINGS; ++itr) {
            con_out("Warning %s = %i\n", opts_warn_list[itr].name, OPTS_WARN(itr));
        }
        con_out("output = %s\n", opts_output);
        con_out("optimization level = %i\n", (int)opts_O);
        con_out("standard = %i\n", opts_standard);
    }

    if (!parser_init()) {
        con_out("failed to initialize parser\n");
        retval = 1;
        goto cleanup;
    }

    util_debug("COM", "starting ...\n");

    if (items_elements) {
        con_out("Mode: manual\n");
        con_out("There are %lu items to compile:\n", (unsigned long)items_elements);
        for (itr = 0; itr < items_elements; ++itr) {
            con_out("  item: %s (%s)\n",
                   items_data[itr].filename,
                   ( (items_data[itr].type == TYPE_QC ? "qc" :
                     (items_data[itr].type == TYPE_ASM ? "asm" :
                     (items_data[itr].type == TYPE_SRC ? "progs.src" :
                     ("unknown"))))));

        if (!parser_compile_file(items_data[itr].filename))
        {
                retval = 1;
                goto cleanup;
            }
        }

        if (!parser_finish(opts_output)) {
            retval = 1;
            goto cleanup;
        }

    } else {
        FILE *src;
        char *line;
        size_t linelen = 0;

        con_out("Mode: progs.src\n");
        src = util_fopen("progs.src", "rb");
        if (!src) {
            con_out("failed to open `progs.src` for reading\n");
            retval = 1;
            goto cleanup;
        }

        line = NULL;
        if (!progs_nextline(&line, &linelen, src) || !line[0]) {
            con_out("illformatted progs.src file: expected output filename in first line\n");
            retval = 1;
            goto srcdone;
        }

        if (!opts_output_wasset) {
            opts_output = util_strdup(line);
            opts_output_free = true;
        }

        while (progs_nextline(&line, &linelen, src)) {
            if (!line[0] || (line[0] == '/' && line[1] == '/'))
                continue;
            con_out("  src: %s\n", line);
            if (!parser_compile_file(line)) {
                retval = 1;
                goto srcdone;
            }
        }

        parser_finish(opts_output);

srcdone:
        fclose(src);
        mem_d(line);
    }

    /* stuff */

cleanup:
    util_debug("COM", "cleaning ...\n");

    mem_d(items_data);

    parser_cleanup();
    if (opts_output_free)
        mem_d((char*)opts_output);

    lex_cleanup();
    util_meminfo();
    return retval;
}
