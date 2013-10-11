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
#include <string.h>
#include <stdlib.h>

#include "platform.h"

int platform_vsnprintf(char *buffer, size_t bytes, const char *format, va_list arg) {
    return vsnprintf(buffer, bytes, format, arg);
}

int platform_sscanf(const char *str, const char *format, ...) {
    int     rt;
    va_list va;

    va_start(va, format);
    rt = vsscanf(str, format, va);
    va_end (va);

    return rt;
}

const struct tm *platform_localtime(const time_t *timer) {
    return localtime(timer);
}

const char *platform_ctime(const time_t *timer) {
    return ctime(timer);
}

char *platform_strncat(char *dest, const char *src, size_t num) {
    return strncat(dest, src, num);
}

const char *platform_tmpnam(char *str) {
    return tmpnam(str);
}

const char *platform_getenv(char *var) {
    return getenv(var);
}

int platform_snprintf(char *src, size_t bytes, const char *format, ...) {
    int     rt;
    va_list va;

    va_start(va, format);
    rt = vsnprintf(src, bytes, format, va);
    va_end(va);

    return rt;
}

char *platform_strcat(char *dest, const char *src) {
    return strcat(dest, src);
}

char *platform_strncpy(char *dest, const char *src, size_t num) {
    return strncpy(dest, src, num);
}

const char *platform_strerror(int err) {
    return strerror(err);
}

FILE *platform_fopen(const char *filename, const char *mode) {
    return fopen(filename, mode);
}

size_t platform_fread(void *ptr, size_t size, size_t count, FILE *stream) {
    return fread(ptr, size, count, stream);
}

size_t platform_fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
    return fwrite(ptr, size, count, stream);
}

int platform_vfprintf(FILE *stream, const char *format, va_list arg) {
    return vfprintf(stream, format, arg);
}

int platform_fclose(FILE *stream) {
    return fclose(stream);
}

int platform_ferror(FILE *stream) {
    return ferror(stream);
}

int platform_fgetc(FILE *stream) {
    return fgetc(stream);
}

int platform_fputs(const char *str, FILE *stream) {
    return fputs(str, stream);
}

int platform_fseek(FILE *stream, long offset, int origin) {
    return fseek(stream, offset, origin);
}

long platform_ftell(FILE *stream) {
    return ftell(stream);
}

int platform_mkdir(const char *path, int mode) {
    return mkdir(path, mode);
}

DIR *platform_opendir(const char *path) {
    return opendir(path);
}

int platform_closedir(DIR *dir) {
    return closedir(dir);
}

struct dirent *platform_readdir(DIR *dir) {
    return readdir(dir);
}
