/*
 * Copyright (C) 2012, 2013
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

#define CTIME_BUFFER    64
#define GETENV_BUFFER   4096
#define STRERROR_BUFFER 128

static void **platform_mem_pool = NULL;
static void platform_mem_atexit() {
    size_t i;
    for (i = 0; i < vec_size(platform_mem_pool); i++)
        mem_d(platform_mem_pool[i]);
    vec_free(platform_mem_pool);
}

static void *platform_mem_allocate(size_t bytes) {
    void *mem = NULL;
    if (!platform_mem_pool) {
        atexit(&platform_mem_atexit);
        vec_push(platform_mem_pool, NULL);
    }

    mem = mem_a(bytes);
    vec_push(platform_mem_pool, mem);

    return mem;
}

int platform_vsnprintf(char *buffer, size_t bytes, const char *format, va_list arg) {
    vsnprintf_s(buffer, bytes, bytes, format, arg);
}

int platform_sscanf(const char *str, const char *format, ...) {
    va_list va;
    va_start(va, format);
    vsscanf_s(str, format, va);
    va_end(va);
}

const struct tm *platform_localtime(const time_t *timer) {
    struct tm *t;
    t = (struct tm*)platform_mem_allocate(sizeof(struct tm));
    localtime_s(&t, timer);
    return &t;
}

const char *platform_ctime(const time_t *timer) {
    char *buffer = (char *)platform_mem_allocate(CTIME_BUFFER);
    ctime_s(buffer, CTIME_BUFFER, timer);
    return buffer;
}

char *platform_strncat(char *dest, const char *src, size_t num) {
    return strncat_s(dest, num, src, _TRUNCATE);
}

const char *platform_tmpnam(char *str) {
    return tmpnam_s(str, L_tmpnam);
}

const char *platform_getenv(char *var) {
    char  *buffer = (char *)platform_mem_allocate(GETENV_BUFFER);
    size_t size;
    getenv_s(&size, buffer, GETENV_BUFFER, var);
    return buffer;
}

int platform_snprintf(char *src, size_t bytes, const char *format, ...) {
    int      rt;
    va_list  va;
    va_start(va, format);

    rt = vsprintf_s(src, bytes, format, va);
    va_end  (va);

    return rt;
}

char *platform_strcat(char *dest, const char *src) {
    strcat_s(dest, strlen(src), src);
    return dest;
}

char *platform_strncpy(char *dest, const char *src, size_t num) {
    strncpy_s(dest, num, src, num);
    return dest;
}

const char *platform_strerror(int err) {
    char *buffer = (char*)platform_mem_allocate(STRERROR_BUFFER);
    strerror_s(buffer, STRERROR_BUFFER, err);
    return buffer;
}
