/*
 * Copyright (C) 2012 
 * 	Dale Weiler
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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
	
	printf("[MEM] allocation: %08u (bytes) at %s:%u\n", byte, file, line);
	return (void*)((uintptr_t)data+sizeof(struct memblock_t));
}

void util_memory_d(void *ptrn, unsigned int line, const char *file) {
	if (!ptrn) return;
	void              *data = (void*)((uintptr_t)ptrn-sizeof(struct memblock_t));
	struct memblock_t *info = (struct memblock_t*)data;
	
	printf("[MEM] released:   %08u (bytes) at %s:%u\n", info->byte, file, line);
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

void util_debug(const char *ms, ...) {
	va_list  va;
	va_start(va, ms);
	vprintf (ms, va);
	va_end  (va);
}
