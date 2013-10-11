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

#ifndef GMQCC_PLATFORM_HDR
#define GMQCC_PLATFORM_HDR
#include <stdarg.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
#   undef  STDERR_FILENO
#   undef  STDOUT_FILENO
#   define STDERR_FILENO 2
#   define STDOUT_FILENO 1

#   ifndef __MINGW32__
#       define _WIN32_LEAN_AND_MEAN
#       include <windows.h>
#       include <io.h>
#       include <fcntl.h>

        struct dirent {
            long               d_ino;
            unsigned short     d_reclen;
            unsigned short     d_namlen;
            char               d_name[FILENAME_MAX];
        };

        typedef struct {
            struct _finddata_t dd_dta;
            struct dirent      dd_dir;
            long               dd_handle;
            int                dd_stat;
            char               dd_name[1];
        } DIR;

#       ifdef S_ISDIR
#           undef  S_ISDIR
#       endif /*! S_ISDIR */
#       define S_ISDIR(X) ((X)&_S_IFDIR)
#   else
#       include <dirent.h>
#   endif /*!__MINGW32__*/
#else
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <unistd.h>
#   include <dirent.h>
#endif /*!_WIN32*/

int platform_vsnprintf(char *buffer, size_t bytes, const char *format, va_list arg);
int platform_sscanf(const char *str, const char *format, ...);
const struct tm *platform_localtime(const time_t *timer);
const char *platform_ctime(const time_t *timer);
char *platform_strncat(char *dest, const char *src, size_t num);
const char *platform_tmpnam(char *str);
const char *platform_getenv(char *var);
int platform_snprintf(char *src, size_t bytes, const char *format, ...);
int platform_vasprintf(char **dat, const char *fmt, va_list args);
char *platform_strcat(char *dest, const char *src);
char *platform_strncpy(char *dest, const char *src, size_t num);
const char *platform_strerror(int err);
FILE *platform_fopen(const char *filename, const char *mode);
size_t platform_fread(void *ptr, size_t size, size_t count, FILE *stream);
size_t platform_fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
int platform_fflush(FILE *stream);
int platform_vfprintf(FILE *stream, const char *format, va_list arg);
int platform_fclose(FILE *stream);
int platform_ferror(FILE *stream);
int platform_fgetc(FILE *stream);
int platform_fputs(const char *str, FILE *stream);
int platform_fseek(FILE *stream, long offset, int origin);
long platform_ftell(FILE *stream);
int platform_mkdir(const char *path, int mode);
DIR *platform_opendir(const char *path);
int platform_closedir(DIR *dir);
struct dirent *platform_readdir(DIR *dir);
int platform_isatty(int fd);

#endif
