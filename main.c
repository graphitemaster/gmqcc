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

uint32_t    opt_flags[1 + (NUM_F_FLAGS / 32)];

uint32_t    opt_O        = 1;
const char *opt_output   = "progs.dat";
int         opt_standard = STD_DEF;

typedef struct { char *filename; int type; } argitem;
VECTOR_MAKE(argitem, items);

#define TYPE_QC  0
#define TYPE_ASM 1
#define TYPE_SRC 2

static const char *app_name;

static int usage() {
    printf("usage: %s [options] [files...]", app_name);
    printf("options:\n"
           "  -h, --help             show this help message\n"
           "  -o, --output=file      output file, defaults to progs.dat\n"
           "  -a filename            add an asm file to be assembled\n"
           "  -s filename            add a progs.src file to be used\n");
    printf("  -fflag                 enable a flag\n"
           "  -fno-flag              disable a flag\n"
           "  -std standard          select one of the following standards\n"
           "       -std=qcc          original QuakeC\n"
           "       -std=fteqcc       fteqcc QuakeC\n"
           "       -std=gmqcc        this compiler (default)\n");
    printf("\n");
    printf("flags:\n"
           "  -fdarkplaces-string-table-bug\n"
           "            patch the string table to work with some bugged darkplaces versions\n"
           "  -fomit-nullbytes\n"
           "            omits certain null-bytes for a smaller output - requires a patched engine\n"
           );
    return -1;
}

static bool options_setflag(const char *name, bool on) {
    size_t i;

    for (i = 0; i < opt_flag_list_count; ++i) {
        if (!strcmp(name, opt_flag_list[i].name)) {
            longbit lb = opt_flag_list[i].bit;
#if 0
            if (on)
                opt_flags[lb.idx] |= (1<<(lb.bit));
            else
                opt_flags[lb.idx] &= ~(1<<(lb.bit));
#else
            if (on)
                opt_flags[0] |= (1<<lb);
            else
                opt_flags[0] &= ~(1<<(lb));
#endif
            return true;
        }
    }
    return false;
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
    while (!argend && argc > 1) {
        char *argarg;
        argitem item;

        ++argv;
        --argc;

        if (argv[0][0] == '-') {
    /* All gcc-type long options */
            if (options_long_gcc("std", &argc, &argv, &argarg)) {
                if      (!strcmp(argarg, "gmqcc") || !strcmp(argarg, "default"))
                    opt_standard = STD_DEF;
                else if (!strcmp(argarg, "qcc"))
                    opt_standard = STD_QCC;
                else if (!strcmp(argarg, "fte") || !strcmp(argarg, "fteqcc"))
                    opt_standard = STD_FTE;
                else {
                    printf("Unknown standard: %s\n", argarg);
                    return false;
                }
                continue;
            }

            switch (argv[0][1]) {
                /* -h, show usage but exit with 0 */
                case 'h':
                    usage();
                    exit(0);
                    break;

                /* handle all -fflags */
                case 'f':
                    if (!strncmp(argv[0]+2, "no-", 3)) {
                        if (!options_setflag(argv[0]+5, false)) {
                            printf("unknown flag: %s\n", argv[0]+2);
                            return false;
                        }
                    }
                    else if (!options_setflag(argv[0]+2, true)) {
                        printf("unknown flag: %s\n", argv[0]+2);
                        return false;
                    }
                    break;

                case 'O':
                    if (!options_witharg(&argc, &argv, &argarg)) {
                        printf("option -O requires a numerical argument\n");
                        return false;
                    }
                    opt_O = atoi(argarg);
                    break;

                case 'o':
                    if (!options_witharg(&argc, &argv, &argarg)) {
                        printf("option -o requires an argument: the output file name\n");
                        return false;
                    }
                    opt_output = argarg;
                    break;

                case 'a':
                case 's':
                    item.type = argv[0][1] == 'a' ? TYPE_ASM : TYPE_SRC;
                    if (!options_witharg(&argc, &argv, &argarg)) {
                        printf("option -a requires a filename %s\n",
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
                        if (options_long_witharg("output", &argc, &argv, &argarg))
                            opt_output = argarg;
                        else
                        {
                            printf("Unknown parameter: %s\n", argv[0]);
                            return false;
                        }
                    }
                    break;

                default:
                    printf("Unknown parameter: %s\n", argv[0]);
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

int main(int argc, char **argv) {
    app_name = argv[0];

    if (!options_parse(argc, argv)) {
        return usage();
    }

    util_debug("COM", "starting ...\n");

    /* stuff */

    util_debug("COM", "cleaning ...\n");

    util_meminfo();
    return 0;
}
