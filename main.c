/*
 * Copyright (C) 2012
 *     Dale Weiler
 *     Wolfgang Bumiller
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

/* counter increased in ir.c */
unsigned int optimization_count[COUNT_OPTIMIZATIONS];
static bool opts_output_wasset = false;

cmd_options opts;

/* set by the standard */
const oper_info *operators      = NULL;
size_t           operator_count = 0;

typedef struct { char *filename; int type; } argitem;
static argitem *items = NULL;

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
            "  -s filename            add a progs.src file to be used\n");
    con_out("  -E                     stop after preprocessing\n");
    con_out("  -std=standard          select one of the following standards\n"
            "       -std=qcc          original QuakeC\n"
            "       -std=fteqcc       fteqcc QuakeC\n"
            "       -std=gmqcc        this compiler (default)\n");
    con_out("  -f<flag>               enable a flag\n"
            "  -fno-<flag>            disable a flag\n"
            "  -fhelp                 list possible flags\n");
    con_out("  -W<warning>            enable a warning\n"
            "  -Wno-<warning>         disable a warning\n"
            "  -Wall                  enable all warnings\n"
            "  -Werror                treat warnings as errors\n");
    con_out("  -Whelp                 list possible warnings\n");
    con_out("  -O<number>             optimization level\n"
            "  -O<name>               enable specific optimization\n"
            "  -Ono-<name>            disable specific optimization\n"
            "  -Ohelp                 list optimizations\n");
    con_out("  -force-crc=num         force a specific checksum into the header\n");
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
    return options_setflag_all(name, on, opts.flags, opts_flag_list, COUNT_FLAGS);
}
static bool options_setwarn(const char *name, bool on) {
    return options_setflag_all(name, on, opts.warn, opts_warn_list, COUNT_WARNINGS);
}
static bool options_setoptim(const char *name, bool on) {
    return options_setflag_all(name, on, opts.optimization, opts_opt_list, COUNT_OPTIMIZATIONS);
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

void options_set(uint32_t *flags, size_t idx, bool on)
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

static void set_optimizations(unsigned int level)
{
    size_t i;
    for (i = 0; i < COUNT_OPTIMIZATIONS; ++i)
        options_set(opts.optimization, i, level >= opts_opt_oflag[i]);
}

static bool options_parse(int argc, char **argv) {
    bool argend = false;
    size_t itr;
    char  buffer[1024];
    char *redirout = (char*)stdout;
    char *redirerr = (char*)stderr;

    while (!argend && argc > 1) {
        char *argarg;
        argitem item;

        ++argv;
        --argc;

        if (argv[0][0] == '-') {
    /* All gcc-type long options */
            if (options_long_gcc("std", &argc, &argv, &argarg)) {
                if      (!strcmp(argarg, "gmqcc") || !strcmp(argarg, "default")) {
                    options_set(opts.flags, ADJUST_VECTOR_FIELDS, true);
                    opts.standard = COMPILER_GMQCC;
                } else if (!strcmp(argarg, "qcc")) {
                    options_set(opts.flags, ADJUST_VECTOR_FIELDS, false);
                    options_set(opts.flags, ASSIGN_FUNCTION_TYPES, true);
                    opts.standard = COMPILER_QCC;
                } else if (!strcmp(argarg, "fte") || !strcmp(argarg, "fteqcc")) {
                    options_set(opts.flags, FTEPP,                true);
                    options_set(opts.flags, TRANSLATABLE_STRINGS, true);
                    options_set(opts.flags, ADJUST_VECTOR_FIELDS, false);
                    options_set(opts.flags, ASSIGN_FUNCTION_TYPES, true);
                    options_set(opts.warn, WARN_TERNARY_PRECEDENCE, true);
                    options_set(opts.flags, CORRECT_TERNARY, false);
                    opts.standard = COMPILER_FTEQCC;
                } else if (!strcmp(argarg, "qccx")) {
                    options_set(opts.flags, ADJUST_VECTOR_FIELDS, false);
                    opts.standard = COMPILER_QCCX;
                } else {
                    con_out("Unknown standard: %s\n", argarg);
                    return false;
                }
                continue;
            }
            if (options_long_gcc("force-crc", &argc, &argv, &argarg)) {
                opts.forcecrc = true;
                opts.forced_crc = strtol(argarg, NULL, 0);
                continue;
            }
            if (options_long_gcc("redirout", &argc, &argv, &redirout)) {
                con_change(redirout, redirerr);
                continue;
            }
            if (options_long_gcc("redirerr", &argc, &argv, &redirerr)) {
                con_change(redirout, redirerr);
                continue;
            }

            /* show defaults (like pathscale) */
            if (!strcmp(argv[0]+1, "show-defaults")) {
                size_t itr;
                char   buffer[1024];
                for (itr = 0; itr < COUNT_FLAGS; ++itr) {
                    if (!OPTS_FLAG(itr))
                        continue;

                    memset(buffer, 0, sizeof(buffer));
                    util_strtononcmd(opts_flag_list[itr].name, buffer, strlen(opts_flag_list[itr].name) + 1);

                    con_out("-f%s ", buffer);
                }
                for (itr = 0; itr < COUNT_WARNINGS; ++itr) {
                    if (!OPTS_WARN(itr))
                        continue;

                    memset(buffer, 0, sizeof(buffer));
                    util_strtononcmd(opts_warn_list[itr].name, buffer, strlen(opts_warn_list[itr].name) + 1);
                    con_out("-W%s ", buffer);
                }
                con_out("\n");
                exit(0);
            }

            if (!strcmp(argv[0]+1, "debug")) {
                opts.debug = true;
                continue;
            }
            if (!strcmp(argv[0]+1, "dump")) {
                opts.dump = true;
                continue;
            }
            if (!strcmp(argv[0]+1, "dumpfin")) {
                opts.dumpfin = true;
                continue;
            }
            if (!strcmp(argv[0]+1, "memchk")) {
                opts.memchk = true;
                continue;
            }
            if (!strcmp(argv[0]+1, "nocolor")) {
                con_color(0);
                continue;
            }

            switch (argv[0][1]) {
                /* -h, show usage but exit with 0 */
                case 'h':
                    usage();
                    exit(0);
                    /* break; never reached because of exit(0) */

                case 'E':
                    opts.pp_only = true;
                    break;

                /* debug turns on -flno */
                case 'g':
                    options_setflag("LNO", true);
                    break;

                /* handle all -fflags */
                case 'f':
                    util_strtocmd(argv[0]+2, argv[0]+2, strlen(argv[0]+2)+1);
                    if (!strcmp(argv[0]+2, "HELP") || *(argv[0]+2) == '?') {
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
                    if (!strcmp(argv[0]+2, "HELP") || *(argv[0]+2) == '?') {
                        con_out("Possible warnings:\n");
                        for (itr = 0; itr < COUNT_WARNINGS; ++itr) {
                            util_strtononcmd(opts_warn_list[itr].name, buffer, sizeof(buffer));
                            con_out(" -W%s\n", buffer);
                        }
                        exit(0);
                    }
                    else if (!strcmp(argv[0]+2, "NO_ERROR")) {
                        opts.werror = false;
                        break;
                    }
                    else if (!strcmp(argv[0]+2, "ERROR")) {
                        opts.werror = true;
                        break;
                    }
                    else if (!strcmp(argv[0]+2, "NONE")) {
                        for (itr = 0; itr < sizeof(opts.warn)/sizeof(opts.warn[0]); ++itr)
                            opts.warn[itr] = 0;
                        break;
                    }
                    else if (!strcmp(argv[0]+2, "ALL")) {
                        for (itr = 0; itr < sizeof(opts.warn)/sizeof(opts.warn[0]); ++itr)
                            opts.warn[itr] = 0xFFFFFFFFL;
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
                        con_out("option -O requires a numerical argument, or optimization name with an optional 'no-' prefix\n");
                        return false;
                    }
                    if (isdigit(argarg[0])) {
                        opts.O = atoi(argarg);
                        set_optimizations(opts.O);
                    } else {
                        util_strtocmd(argarg, argarg, strlen(argarg)+1);
                        if (!strcmp(argarg, "HELP")) {
                            con_out("Possible optimizations:\n");
                            for (itr = 0; itr < COUNT_OPTIMIZATIONS; ++itr) {
                                util_strtononcmd(opts_opt_list[itr].name, buffer, sizeof(buffer));
                                con_out(" -O%-20s (-O%u)\n", buffer, opts_opt_oflag[itr]);
                            }
                            exit(0);
                        }
                        else if (!strcmp(argarg, "ALL"))
                            set_optimizations(opts.O = 9999);
                        else if (!strncmp(argarg, "NO_", 3)) {
                            if (!options_setoptim(argarg+3, false)) {
                                con_out("unknown optimization: %s\n", argarg+3);
                                return false;
                            }
                        }
                        else {
                            if (!options_setoptim(argarg, true)) {
                                con_out("unknown optimization: %s\n", argarg);
                                return false;
                            }
                        }
                    }
                    break;

                case 'o':
                    if (!options_witharg(&argc, &argv, &argarg)) {
                        con_out("option -o requires an argument: the output file name\n");
                        return false;
                    }
                    opts.output = argarg;
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
                    vec_push(items, item);
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
                            opts.output = argarg;
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
            item.filename = argv[0];
            item.type     = TYPE_QC;
            vec_push(items, item);
        }
    }
    return true;
}

/* returns the line number, or -1 on error */
static bool progs_nextline(char **out, size_t *alen,FILE *src) {
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
    bool operators_free = false;
    bool progs_src = false;
    FILE *outfile = NULL;

    memset(&opts, 0, sizeof(opts));
    opts.output         = "progs.dat";
    opts.standard       = COMPILER_GMQCC;
    opts.max_array_size = (1024<<3);

    app_name = argv[0];
    con_init();

    /* default options / warn flags */
    options_set(opts.warn, WARN_UNKNOWN_CONTROL_SEQUENCE, true);
    options_set(opts.warn, WARN_EXTENSIONS, true);
    options_set(opts.warn, WARN_FIELD_REDECLARED, true);
    options_set(opts.warn, WARN_TOO_FEW_PARAMETERS, true);
    options_set(opts.warn, WARN_MISSING_RETURN_VALUES, true);
    options_set(opts.warn, WARN_USED_UNINITIALIZED, true);
    options_set(opts.warn, WARN_LOCAL_CONSTANTS, true);
    options_set(opts.warn, WARN_VOID_VARIABLES, true);
    options_set(opts.warn, WARN_IMPLICIT_FUNCTION_POINTER, true);
    options_set(opts.warn, WARN_VARIADIC_FUNCTION, true);
    options_set(opts.warn, WARN_FRAME_MACROS, true);
    options_set(opts.warn, WARN_UNUSED_VARIABLE, true);
    options_set(opts.warn, WARN_EFFECTLESS_STATEMENT, true);
    options_set(opts.warn, WARN_END_SYS_FIELDS, true);
    options_set(opts.warn, WARN_ASSIGN_FUNCTION_TYPES, true);
    options_set(opts.warn, WARN_PREPROCESSOR, true);
    options_set(opts.warn, WARN_MULTIFILE_IF, true);
    options_set(opts.warn, WARN_DOUBLE_DECLARATION, true);
    options_set(opts.warn, WARN_CONST_VAR, true);
    options_set(opts.warn, WARN_MULTIBYTE_CHARACTER, true);

    options_set(opts.flags, ADJUST_VECTOR_FIELDS, true);
    options_set(opts.flags, FTEPP, false);
    options_set(opts.flags, CORRECT_TERNARY, true);

    if (!options_parse(argc, argv)) {
        return usage();
    }

    /* the standard decides which set of operators to use */
    if (opts.standard == COMPILER_GMQCC) {
        operators = c_operators;
        operator_count = c_operator_count;
    } else if (opts.standard == COMPILER_FTEQCC) {
        operators = fte_operators;
        operator_count = fte_operator_count;
    } else {
        operators = qcc_operators;
        operator_count = qcc_operator_count;
    }

    if (operators == fte_operators) {
        /* fix ternary? */
        if (OPTS_FLAG(CORRECT_TERNARY)) {
            oper_info *newops;
            if (operators[operator_count-2].id != opid1(',') ||
                operators[operator_count-1].id != opid2(':','?'))
            {
                con_err("internal error: operator precedence table wasn't updated correctly!\n");
                exit(1);
            }
            operators_free = true;
            newops = mem_a(sizeof(operators[0]) * operator_count);
            memcpy(newops, operators, sizeof(operators[0]) * operator_count);
            memcpy(&newops[operator_count-2], &operators[operator_count-1], sizeof(newops[0]));
            memcpy(&newops[operator_count-1], &operators[operator_count-2], sizeof(newops[0]));
            newops[operator_count-2].prec = newops[operator_count-1].prec+1;
            operators = newops;
        }
    }

    if (opts.dump) {
        for (itr = 0; itr < COUNT_FLAGS; ++itr) {
            con_out("Flag %s = %i\n", opts_flag_list[itr].name, OPTS_FLAG(itr));
        }
        for (itr = 0; itr < COUNT_WARNINGS; ++itr) {
            con_out("Warning %s = %i\n", opts_warn_list[itr].name, OPTS_WARN(itr));
        }
        con_out("output = %s\n", opts.output);
        con_out("optimization level = %i\n", (int)opts.O);
        con_out("standard = %i\n", opts.standard);
    }

    if (opts.pp_only) {
        if (opts_output_wasset) {
            outfile = util_fopen(opts.output, "wb");
            if (!outfile) {
                con_err("failed to open `%s` for writing\n", opts.output);
                retval = 1;
                goto cleanup;
            }
        }
        else
            outfile = stdout;
    }

    if (!opts.pp_only) {
        if (!parser_init()) {
            con_err("failed to initialize parser\n");
            retval = 1;
            goto cleanup;
        }
    }
    if (opts.pp_only || OPTS_FLAG(FTEPP)) {
        if (!ftepp_init()) {
            con_err("failed to initialize parser\n");
            retval = 1;
            goto cleanup;
        }
    }

    util_debug("COM", "starting ...\n");

    if (!vec_size(items)) {
        FILE *src;
        char *line;
        size_t linelen = 0;

        progs_src = true;

        src = util_fopen("progs.src", "rb");
        if (!src) {
            con_err("failed to open `progs.src` for reading\n");
            retval = 1;
            goto cleanup;
        }

        line = NULL;
        if (!progs_nextline(&line, &linelen, src) || !line[0]) {
            con_err("illformatted progs.src file: expected output filename in first line\n");
            retval = 1;
            goto srcdone;
        }

        if (!opts_output_wasset) {
            opts.output = util_strdup(line);
            opts_output_free = true;
        }

        while (progs_nextline(&line, &linelen, src)) {
            argitem item;
            if (!line[0] || (line[0] == '/' && line[1] == '/'))
                continue;
            item.filename = util_strdup(line);
            item.type     = TYPE_QC;
            vec_push(items, item);
        }

srcdone:
        fclose(src);
        mem_d(line);
    }

    if (retval)
        goto cleanup;

    if (vec_size(items)) {
        if (!opts.pp_only) {
            con_out("Mode: %s\n", (progs_src ? "progs.src" : "manual"));
            con_out("There are %lu items to compile:\n", (unsigned long)vec_size(items));
        }
        for (itr = 0; itr < vec_size(items); ++itr) {
            if (!opts.pp_only) {
                con_out("  item: %s (%s)\n",
                       items[itr].filename,
                       ( (items[itr].type == TYPE_QC ? "qc" :
                         (items[itr].type == TYPE_ASM ? "asm" :
                         (items[itr].type == TYPE_SRC ? "progs.src" :
                         ("unknown"))))));
            }

            if (opts.pp_only) {
                const char *out;
                if (!ftepp_preprocess_file(items[itr].filename)) {
                    retval = 1;
                    goto cleanup;
                }
                out = ftepp_get();
                if (out)
                    fprintf(outfile, "%s", out);
                ftepp_flush();
            }
            else {
                if (OPTS_FLAG(FTEPP)) {
                    const char *data;
                    if (!ftepp_preprocess_file(items[itr].filename)) {
                        retval = 1;
                        goto cleanup;
                    }
                    data = ftepp_get();
                    if (vec_size(data)) {
                        if (!parser_compile_string_len(items[itr].filename, data, vec_size(data))) {
                            retval = 1;
                            goto cleanup;
                        }
                    }
                    ftepp_flush();
                }
                else {
                    if (!parser_compile_file(items[itr].filename)) {
                        retval = 1;
                        goto cleanup;
                    }
                }
            }

            if (progs_src) {
                mem_d(items[itr].filename);
                items[itr].filename = NULL;
            }
        }

        ftepp_finish();
        if (!opts.pp_only) {
            if (!parser_finish(opts.output)) {
                retval = 1;
                goto cleanup;
            }
        }
    }

    /* stuff */

    if (!opts.pp_only) {
        for (itr = 0; itr < COUNT_OPTIMIZATIONS; ++itr) {
            if (optimization_count[itr]) {
                con_out("%s: %u\n", opts_opt_list[itr].name, (unsigned int)optimization_count[itr]);
            }
        }
    }

cleanup:
    util_debug("COM", "cleaning ...\n");
    ftepp_finish();
    con_close();
    vec_free(items);

    if (!opts.pp_only)
        parser_cleanup();
    if (opts_output_free)
        mem_d((char*)opts.output);
    if (operators_free)
        mem_d((void*)operators);

    lex_cleanup();
    util_meminfo();
    return retval;
}
