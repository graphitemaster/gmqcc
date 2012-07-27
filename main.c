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

static const char *output = "progs.dat";
static const char *input  = NULL;

#define OptReq(opt, body)                                         \
    case opt:                                                     \
        if (argv[0][2]) argarg = argv[0]+2;                       \
        else {                                                    \
            if (argc < 2) {                                       \
                printf("option -%c requires an argument\n", opt); \
                exit(1);                                          \
            }                                                     \
            argarg = argv[1];                                     \
            --argc;                                               \
            ++argv;                                               \
        }                                                         \
        do { body } while (0);                                    \
        break;

#define LongReq(opt, body)                                   \
    if (!strcmp(argv[0], opt)) {                             \
        if (argc < 2) {                                      \
            printf("option " opt " requires an argument\n"); \
            exit(1);                                         \
        }                                                    \
        argarg = argv[1];                                    \
        --argc;                                              \
        ++argv;                                              \
        do { body } while (0);                               \
        break;                                               \
    } else if (!strncmp(argv[0], opt "=", sizeof(opt "=")))  \
    {                                                        \
        argarg = argv[0] + sizeof(opt "=");                  \
        do { body } while (0);                               \
        break;                                               \
    }

bool parser_compile(const char *filename, const char *datfile);
int main(int argc, char **argv) {
    const char *argarg;
    char opt;

    util_debug("COM", "starting ...\n");

    --argc;
    ++argv;
    while (argc > 0) {
        if (argv[0][0] == '-') {
            opt = argv[0][1];
            switch (opt)
            {
                OptReq('o', output = argarg; );
                case '-':
                    LongReq("--output", output = argarg; );
                default:
                    printf("Unrecognized option: %s\n", argv[0]);
                    break;
            }
        }
        else
        {
            if (input) {
                printf("Onlyh 1 input file allowed\n");
                exit(1);
            }
            input = argv[0];
        }
        --argc;
        ++argv;
    }

    if (!input) {
        printf("must specify an input file\n");
    }

    if (!parser_compile(input, output)) {
        printf("There were compile errors\n");
    }

    util_debug("COM", "cleaning ...\n");

    util_meminfo();
    return 0;
}
