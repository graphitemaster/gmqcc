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

/*
 * This is essentially a "wrapper" interface around standard C's IO
 * library.  There is two reason we implement this, 1) visual studio
 * hearts for "secure" varations, as part of it's "Security Enhancements
 * in the CRT" (http://msdn.microsoft.com/en-us/library/8ef0s5kh.aspx).
 * 2) But one of the greater reasons is for the possibility of large file
 * support in the future.  I don't expect to reach the 2GB limit any
 * time soon (mainly because that would be insane).  But when it comes
 * to adding support for some other larger IO tasks (in the test-suite,
 * or even the QCVM we'll need it). There is also a third possibility of
 * building .dat files directly from zip files (which would be very cool
 * at least I think so).  
 */
#ifdef _MSC_VER
/* {{{ */
    /*
     * Visual Studio has security CRT features which I actually want to support
     * if we ever port to Windows 8, and want GMQCC to be API safe.
     *
     * We handle them here, for all file-operations. 
     */

    static void file_exception (
        const wchar_t *expression,
        const wchar_t *function,
        const wchar_t *file,
        unsigned int   line,
        uintptr_t      reserved
    ) {
        wprintf(L"Invalid parameter dectected %s:%d %s [%s]\n", file, line, function, expression);
        wprintf(L"Aborting ...\n");
        abort();
    }

    static void file_init() {
        static bool init = false;

        if (init)
            return;

        _set_invalid_parameter_handler(&file_exception);

        /*
         * Turnoff the message box for CRT asserations otherwise
         * we don't get the error reported to the console as we should
         * otherwise get.
         */
        _CrtSetReportMode(_CRT_ASSERT, 0);
        init = !init;
    }


    FILE *fs_file_open(const char *filename, const char *mode) {
        FILE *handle = NULL;
        file_init();

        return (fopen_s(&handle, filename, mode) != 0) ? NULL : handle;
    }

    size_t fs_file_read(void *buffer, size_t size, size_t count, FILE *fp) {
        file_init();
        return fread_s(buffer, size*count, size, count, fp);
    }

    int fs_file_printf(FILE *fp, const char *format, ...) {
        int      rt;
        va_list  va;
        va_start(va, format);

        file_init();
        rt = vfprintf_s(fp, format, va);
        va_end  (va);

        return rt;
    }

/* }}} */
#else
/* {{{ */
    /*
     * All other compilers/platforms that don't restrict insane policies on
     * IO for no aparent reason.
     */
    FILE *fs_file_open(const char *filename, const char *mode) {
        return fopen(filename, mode);
    }

    size_t fs_file_read(void *buffer, size_t size, size_t count, FILE *fp) {
        return fread(buffer, size, count, fp);
    }

    int fs_file_printf(FILE *fp, const char *format, ...) {
        int      rt;
        va_list  va;
        va_start(va, format);
        rt = vfprintf(fp, format, va);
        va_end  (va);

        return rt;
    }

/* }}} */
#endif

/*
 * These are implemented as just generic wrappers to keep consistency in
 * the API.  Not as macros though  
 */
void fs_file_close(FILE *fp) {
    /* Invokes file_exception on windows if fp is null */
    fclose (fp);
}

size_t  fs_file_write (
    const void    *buffer,
    size_t         size,
    size_t         count,
    FILE          *fp
) {
    /* Invokes file_exception on windows if fp is null */
    return fwrite(buffer, size, count, fp);
}

int fs_file_error(FILE *fp) {
    /* Invokes file_exception on windows if fp is null */
    return ferror(fp);
}

int fs_file_getc(FILE *fp) {
    /* Invokes file_exception on windows if fp is null */
    return fgetc(fp);
}

int fs_file_puts(FILE *fp, const char *str) {
    /* Invokes file_exception on windows if fp is null */
    return fputs(str, fp);
}

int fs_file_seek(FILE *fp, long int off, int whence) {
    /* Invokes file_exception on windows if fp is null */
    return fseek(fp, off, whence);
}

int fs_file_putc(FILE *fp, int ch) {
    /* Invokes file_exception on windows if fp is null */
    return fputc(ch, fp);
}

int fs_file_flush(FILE *fp) {
    /* Invokes file_exception on windows if fp is null */
    return fflush(fp);
}

long int fs_file_tell(FILE *fp) {
    /* Invokes file_exception on windows if fp is null */
    return ftell(fp);
}

/*
 * Implements libc getline for systems that don't have it, which is
 * assmed all.  This works the same as getline().
 */
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
        int c = fs_file_getc(stream);

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

/*
 * Now we implement some directory functionality.  Windows lacks dirent.h
 * this is such a pisss off, we implement it here.
 */  
#if defined(_WIN32)
    DIR *fs_dir_open(const char *name) {
        DIR *dir = (DIR*)mem_a(sizeof(DIR) + strlen(name));
        if (!dir)
            return NULL;

        strcpy(dir->dd_name, name);
        return dir;
    }
        
    int fs_dir_close(DIR *dir) {
        FindClose((HANDLE)dir->dd_handle);
        mem_d ((void*)dir);
        return 0;
    }

    struct dirent *fs_dir_read(DIR *dir) {
        WIN32_FIND_DATA info;
        struct dirent  *data;
        int             rets;

        if (!dir->dd_handle) {
            char *dirname;
            if (*dir->dd_name) {
                size_t n = strlen(dir->dd_name);
                if ((dirname  = (char*)mem_a(n + 5) /* 4 + 1 */)) {
                    strcpy(dirname,     dir->dd_name);
                    strcpy(dirname + n, "\\*.*");   /* 4 + 1 */
                }
            } else {
                if (!(dirname = util_strdup("\\*.*")))
                    return NULL;
            }

            dir->dd_handle = (long)FindFirstFile(dirname, &info);
            mem_d(dirname);
            rets = !(!dir->dd_handle);
        } else if (dir->dd_handle != -11) {
            rets = FindNextFile ((HANDLE)dir->dd_handle, &info);
        } else {
            rets = 0;
        }

        if (!rets)
            return NULL;
        
        if ((data = (struct dirent*)mem_a(sizeof(struct dirent)))) {
            strncpy(data->d_name, info.cFileName, FILENAME_MAX - 1);
            data->d_name[FILENAME_MAX - 1] = '\0'; /* terminate */
            data->d_namlen                 = strlen(data->d_name);
        }
        return data;
    }

    int fs_dir_change(const char *path) {
        return !SetCurrentDirectory(path);
    }

    int fs_dir_make(const char *path) {
        return !CreateDirectory(path, NULL);
    }

    /*
     * Visual studio also lacks S_ISDIR for sys/stat.h, so we emulate this as well
     * which is not hard at all.
     */
#   undef  S_ISDIR
#   define S_ISDIR(X) ((X)&_S_IFDIR)
#else
    #include <sys/stat.h> /* mkdir */
    #include <unistd.h>   /* chdir */

    DIR *fs_dir_open(const char *name) {
        return opendir(name);
    }

    int fs_dir_close(DIR *dir) {
        return closedir(dir);
    }

    struct dirent *fs_dir_read(DIR *dir) {
        return readdir(dir);
    }

    int fs_dir_change(const char *path) {
        return chdir(path);
    }

    int fs_dir_make(const char *path) {
        return mkdir(path, 0700);
    }
#endif /*! defined (_WIN32) */
