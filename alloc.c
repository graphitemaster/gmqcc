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
#include "gmqcc.h"

/*
 * The compiler has it's own memory tracker / allocator for debugging
 * reasons.  It will help find things like buggy CSE or OOMs (if this
 * compiler ever grows to that point.)
 */
struct memblock_t {
	const char  *file;
	unsigned int line;
	unsigned int byte;
};

void *memory_a(unsigned int byte, unsigned int line, const char *file) {
	struct memblock_t *data = malloc(sizeof(struct memblock_t) + byte);
	if (!data) return NULL;
	data->line = line;
	data->byte = byte;
	data->file = file;
	printf("[MEM] allocation: %08u (bytes) at %s:%u\n", byte, file, line);
	return (void*)((uintptr_t)data+sizeof(struct memblock_t));
}

void memory_d(void *ptrn, unsigned int line, const char *file) {
	if (!ptrn) return;
	void              *data = (void*)((uintptr_t)ptrn-sizeof(struct memblock_t));
	struct memblock_t *info = (struct memblock_t*)data;
	printf("[MEM] released:   %08u (bytes) at %s:%u\n", info->byte, file, line);
	free(data);
}

/*
 * Ensure the macros are not already defined otherwise the memory
 * tracker will fail.  I hate trying to fix macro bugs, this should
 * help stop any of that from occuring.
 */
#ifdef mem_a
#undef mem_a
#endif
#ifdef mem_d
#undef mem_d
#endif
