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
#include <string.h>
#include <stdlib.h>

#include "platform.h"
#include "gmqcc.h"

int platform_vasprintf(char **dat, const char *fmt, va_list args) {
    int     ret;
    int     len;
    char   *tmp = NULL;
    char    buf[128];
    va_list cpy;

    va_copy(cpy, args);
    len = vsnprintf(buf, sizeof(buf), fmt, cpy);
    va_end (cpy);

    if (len < 0)
        return len;

    if (len < (int)sizeof(buf)) {
        *dat = util_strdup(buf);
        return len;
    }

    tmp = (char*)mem_a(len + 1);
    if ((ret = vsnprintf(tmp, len + 1, fmt, args)) != len) {
        mem_d(tmp);
        *dat = NULL;
        return -1;
    }

    *dat = tmp;
    return len;
}
