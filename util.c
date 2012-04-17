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
#include <stdarg.h>
#include <errno.h>
#include "gmqcc.h"
 
struct memblock_t {
    const char  *file;
    unsigned int line;
    unsigned int byte;
};

void *util_memory_a(unsigned int byte, unsigned int line, const char *file) {
    struct memblock_t *data = malloc(sizeof(struct memblock_t) + byte);
    if (!data) return NULL;
    data->line = line;
    data->byte = byte;
    data->file = file;
    
    util_debug("MEM", "allocation: %08u (bytes) at %s:%u\n", byte, file, line);
    return (void*)((uintptr_t)data+sizeof(struct memblock_t));
}

void util_memory_d(void *ptrn, unsigned int line, const char *file) {
    if (!ptrn) return;
    void              *data = (void*)((uintptr_t)ptrn-sizeof(struct memblock_t));
    struct memblock_t *info = (struct memblock_t*)data;
    
    util_debug("MEM", "released:   %08u (bytes) at %s:%u\n", info->byte, file, line);
    free(data);
}

#ifndef mem_d
#define mem_d(x) util_memory_d((x), __LINE__, __FILE__)
#endif
#ifndef mem_a
#define mem_a(x) util_memory_a((x), __LINE__, __FILE__)
#endif

/*
 * Some string utility functions, because strdup uses malloc, and we want
 * to track all memory (without replacing malloc).
 */
char *util_strdup(const char *s) {
    size_t  len;
    char   *ptr;
    
    if (!s)
        return NULL;
        
    len = strlen(s);
    ptr = mem_a (len+1);
    
    if (ptr && len) {
        memcpy(ptr, s, len);
        ptr[len] = '\0';
    }
    
    return ptr;
}

/*
 * Remove quotes from a string, escapes from \ in string
 * as well.  This function shouldn't be used to create a
 * char array that is later freed (it uses pointer arith)
 */
char *util_strrq(char *s) {
    char *dst = s;
    char *src = s;
    char  chr;
    while ((chr = *src++) != '\0') {
        if (chr == '\\') {
            *dst++ = chr;
            if ((chr = *src++) == '\0')
                break;
            *dst++ = chr;
        } else if (chr != '"')
            *dst++ = chr;
    }
    *dst = '\0';
    return dst;
}

/*
 * Remove newline from a string (if it exists).  This is
 * done pointer wise instead of strlen(), and an array
 * access.
 */
char *util_strrnl(char *src) {
    if (!src) return NULL;
    char   *cpy = src;
    while (*cpy && *cpy != '\n')
        cpy++;
        
    *cpy = '\0';
    return src;
}

void util_debug(const char *area, const char *ms, ...) {
    va_list  va;
    va_start(va, ms);
    fprintf (stdout, "DEBUG: ");
    fputc   ('[',  stdout);
    fprintf (stdout, area);
    fputs   ("] ", stdout);
    vfprintf(stdout, ms, va);
    va_end  (va);
}

/*
 * Implements libc getline for systems that don't have it, which is 
 * assmed all.  This works the same as getline().
 */
int util_getline(char **lineptr, size_t *n, FILE *stream) {
    int   chr;
    int   ret;
    char *pos;    

    if (!lineptr || !n || !stream)
        return -1;
    if (!*lineptr) {
        if (!(*lineptr = mem_a((*n = 64))))
            return -1;
    }

    chr = *n;
    pos = *lineptr;

    for (;;) {
        int c = getc(stream);
        
        if (chr < 2) {
            char *tmp = mem_a((*n+=(*n>16)?*n:64));
            if  (!tmp)
                return -1;
            
            chr = *n + *lineptr - pos;
            strcpy(tmp,*lineptr);
            
            if (!(*lineptr = tmp))
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
