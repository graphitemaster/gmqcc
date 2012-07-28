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
uint32_t    opt_O = 1;
const char *opt_output = "progs.dat";

static int usage() {
    printf("Usage:\n");
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

static bool options_long_witharg(const char *optname, int *argc_, char ***argv_, char **out) {
    int  argc   = *argc_;
    char **argv = *argv_;

    size_t len = strlen(optname);

    if (strncmp(argv[0]+2, optname, len))
        return false;

    /* it's --optname, check how the parameter is supplied */
    if (argv[0][2+len] == '=') {
        /* using --opt=param */
        *out = argv[0]+2+len+1;
        return true;
    }

    if (argc < 2) /* no parameter was provided */
        return false;

    /* using --opt param */
    *out = argv[1];
    --*argc_;
    ++*argv_;
    return true;
}

static bool options_parse(int argc, char **argv) {
    bool argend = false;
    while (!argend && argc) {
        char *argarg;
        if (argv[0][0] == '-') {
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
                    break;
            }
        }
        ++argv;
        --argc;
    }
    return true;
}

int main(int argc, char **argv) {
    /* char     *app = &argv[0][0]; */

    if (!options_parse(argc-1, argv+1)) {
        return usage();
    }

    util_debug("COM", "starting ...\n");

    /* stuff */

    util_debug("COM", "cleaning ...\n");

    util_meminfo();
    return 0;
}
