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

#ifndef GMQCC_PLATFORM_HEADER
#   error "This header shouldn't be included!"
#endif

#undef GMQCC_PLATFORM_HEADER
#include <stdarg.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
#   ifndef STDERR_FILENO
#       define STDERR_FILENO 2
#   endif
#   ifndef STDOUT_FILENO
#       define STDOUT_FILENO 1
#   endif
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

/*
 * Function: platform_vsnprintf
 *  Write formatted output using a pointer to a lis of arguments.
 *
 * Parameters:
 *  buffer - Storage location for output.
 *  bytes  - Maximum number of characters to write.
 *  format - Format specification.
 *  arg    - Variable argument list.
 *
 * Returns:
 *  The number of characters written if the number of characters to write
 *  is less than or equal to `bytes`; if the number of characters to write
 *  is greater than `bytes`, this function returns -1 indicating that the
 *  output has been truncated. The return value does not include the
 *  terminating null, if one is written.
 *
 * Remarks:
 *  Function takes pointer to an argument list, then formats the data,
 *  and writes up to `bytes` characters to the memory pointed to by
 *  `buffer`. If there is room at the end (that is, if the number of
 *  character to write is less than `bytes`), the buffer will be null-terminated.
 */
int platform_vsnprintf(char *buffer, size_t bytes, const char *format, va_list arg);

/*
 * Function: platform_vsscanf
 *  Reads formatted data from a string.
 *
 * Parameters:
 *  buffer - Stored data to read.
 *  format - Format specification.
 *  arg    - Variable argument list.
 *
 * Returns:
 *  The number of fields that are successfully converted and assigned;
 *  the return value does not include fields that were read but not
 *  assigned. A return vlaue of 0 indicated that no fields were assigned.
 *  The return value if EOF for error or if the end of the string is
 *  reached before the first conversion.
 *
 * Remarks:
 *  Reads data from `buffer` into the locations that are given by each
 *  argument in the `arg` argument list. Every argument in the list must
 *  be a pointer to a variable that has a type that corresponds to a
 *  type specifier in `format`. The `format` argument controls th
 *  interpretation of the input fields and has the same form and function
 *  as the `format` argument for the *scanf* function. If copying takes
 *  place between strings that overlap, the behaviour is undefined.
 */
int platform_vsscanf(const char *buffer, const char *format, va_list arg);

/*
 * Function: platform_localtime
 *  Convert a time value and correct for the local time zone.
 *
 * Parameters
 *  timer - Pointer to stored time.
 *
 * Returns:
 *  A pointer to a structure result, or NULL if the date passed to
 *  the function is before midnight, January 1, 1970.
 */
const struct tm *platform_localtime(const time_t *timer);

/*
 * Function: platform_ctime
 *  Convert a time value to a string and adjust for local time zone
 *  settings.
 *
 * Parameters:
 *  timer - Pointer to stored time.
 *
 * Returns:
 *  Pointer to the character string result. NULL will be returned if time
 *  represents a date before midnight, January 1, 1970, UTC.
 *
 * Remarks:
 *  Converts a time value stored as a `time_t` value into a chracter string.
 *  The `timer` value is usually obtained from a call to *time*, which returns
 *  the number of seconds since midnight, January 1, 1970 UTC. The return
 *  value of the string contains exactly 26 characters. A 24-hour clock is used.
 *  All fields have constant width. The newline chracter and the null character
 *  occupy the last two positions of the string. The converted character string
 *  is also adjusted according to the local time zone settings.
 */
const char *platform_ctime(const time_t *timer);

/*
 * Function: platform_strncat
 *  Append characters of a string.
 *
 * Parameters:
 *  dest - Null terminated destination string
 *  src  - Source string
 *  num  - Number of characters to append
 *
 * Returns:
 *  Pointer to the destination string. No return value is used to indicate
 *  an error.
 *
 * Remarks:
 *  Function appends, at mode, the first `num` characters of `src` to
 *  `dest`. The initial character of `src` overwrites the terminating
 *  null chracter of `dest`. If a null character appears in `src` before
 *  `num` chracters are appended, `platform_strncat` appends all chracters
 *  from `src`, up to the null chracter. If `num` is greater than the
 *  length of `src`, the length of `src` is used in place of `num`.
 */
char *platform_strncat(char *dest, const char *src, size_t num);

/*
 * Function: platform_tmpnam
 *  Generates names you can use to create temporary files.
 *
 * Parameters:
 *  str - Pointer that will hold the generated name and will be identical
 *        to the name returned by the function. This is a convenient way
 *        to save the generated name.
 *
 * Returns:
 *  Pointer to the name generate or *NULL* if there is a failure. Failure
 *  can occur if you attempt more than TMP_MAX calls.
 *
 * Remarks:
 *  Returns a name unique in the current workign directory.
 */
const char *platform_tmpnam(char *str);

/*
 * Function: platform_getenv
 *  Get a value from the current enviroment.
 *
 * Parameters:
 *  var - Enviroment variable name
 *
 * Returns:
 *  A pointer to the enviroment table entry containing `var. It's not
 *  safe to modify the value of the enviroment variable using the returned
 *  pointer. The return value is *NULL* if `var` is not found in the
 *  enviroment table.
 */
const char *platform_getenv(char *var);

int platform_vasprintf(char **dat, const char *fmt, va_list args);

/*
 * Function: platform_strcat
 *  Append characters of a string.
 *
 * Parameters:
 *  dest - Null terminated destination string
 *  src  - Source string
 *
 * Returns:
 *  Pointer to the destination string. No return value is used to indicate
 *  an error.
 *
 * Remarks:
 *  Appens `src` to `dest` and terminates with resulting null character.
 *  The initial character of `src` overwrites the terminating null
 *  character of `dest`. The behaviour of platform_strcat is undefined
 *  if the source and destination string overlap.
 */
char *platform_strcat(char *dest, const char *src);

/*
 * Function: platform_strncpy
 *  Copys characters of one string to another.
 *
 * Parameters:
 *  dest - Destination string.
 *  src  - Source string.
 *  num  - Number of characters to be copied.
 *
 * Returns:
 *  `dest`. No return value is reserved to indicate an error.
 *
 * Remarks:
 *  Copies the initial characters of `src` to `dest` and returns `dest`.
 *  If `num` is less than or equal to the length of `src1 a null character
 *  is not appended automatically to the copied string. If `num` is greater
 *  than the length of `src`, the destination string is padded with null
 *  characters up to length `num`. The behaviour of this function is undefined
 *  if the source and destination strings overlap.
 */
char *platform_strncpy(char *dest, const char *src, size_t num);

/*
 * Function: platform_strerror
 *  Get a system error message
 *
 * Parameters:
 *  err - Error number.
 *
 * Returns:
 *  A pointer to the error message
 */
const char *platform_strerror(int err);

/*
 * Function: platform_fopen
 *  Opens a file
 *
 * Parameters:
 *  filename - File name.
 *  mode     - Kind of access that's enabled.
 *
 * Returns:
 *  A pointer to the open file. A null pointer value indicates an error.
 */
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

/*
 * Function: platform_isatty
 *  Determines whether a file descriptor is associated with a character
 *  device.
 *
 * Returns:
 *  A nonzero value if the descriptor is associated with a character
 *  device. Otherwise `platform_isatty` returns 0.
 */
int platform_isatty(int fd);

#endif
