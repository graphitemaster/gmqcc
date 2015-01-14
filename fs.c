/*
 * Copyright (C) 2012, 2013, 2014, 2015
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

int fs_file_getline(char **lineptr, size_t *n, FILE *stream) {
    int   chr;
    int   ret;
    char *pos;

    if (!lineptr || !n || !stream)
        return -1;
    if (!*lineptr) {
        if (!(*lineptr = (char*)mem_a((*n=64))))
            return -1;
    }

    chr = *n;
    pos = *lineptr;

    for (;;) {
        int c = getc(stream);

        if (chr < 2) {
            *n += (*n > 16) ? *n : 64;
            chr = *n + *lineptr - pos;
            if (!(*lineptr = (char*)mem_r(*lineptr,*n)))
                return -1;
            pos = *n - chr + *lineptr;
        }

        if (ferror(stream))
            return -1;
        if (c == EOF) {
            if (pos == *lineptr)
                return -1;
            else
                break;
        }

        *pos++ = c;
        chr--;
        if (c == '\n')
            break;
    }
    *pos = '\0';
    return (ret = pos - *lineptr);
}
