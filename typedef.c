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
#include <string.h>
#include <stdint.h> /* replace if stdint.h doesn't exist! */
#include <limits.h>
#include "gmqcc.h"

/*
 * This implements a hashtable for typedef type keywords which end up
 * being translated to their full-expressed type.  This uses a singly
 * linked list with a fast hash function.
 */
static typedef_node *typedef_table[1024];

void typedef_init() {
	int i;
	for(i = 0; i < sizeof(typedef_table)/sizeof(*typedef_table); i++)
		typedef_table[i] = NULL;
}

/*
 * Fast collisionless hashfunction based off of:
 * http://www.azillionmonkeys.com/qed/hash.html
 * By: Paul Hsieh
 * 
 * The code is licensed under LGPL 2.1 or Paul
 * Hsieh's derivative license. Stated on his page
 * quote:
 * 
 * 	The LGPL 2.1 is not necessarily a more liberal license than my 
 *	derivative license, but this additional licensing makes the code
 * 	available to more developers. Note that this does not give you 
 * 	multi-licensing rights. You can only use the code under one of
 * 	the licenses at a time. 
 * 
 *  Paul Hsieh derivative license
 *
 *	The derivative content includes raw computer source code, ideas, 
 *	opinions, and excerpts whose original source is covered under 
 *	another license and transformations of such derivatives.
 *	Note that mere excerpts by themselves (with the exception of raw
 * 	source code) are not considered derivative works under this license.
 * 	Use and redistribution is limited to the following conditions:
 * 
 *	One may not create a derivative work which, in any way, violates the
 *	Paul Hsieh exposition license described above on the original content.
 *
 *	One may not apply a license to a derivative work that precludes anyone
 *	else from using and redistributing derivative content.
 *
 *	One may not attribute any derivative content to authors not involved
 *	in the creation of the content, though an attribution to the author
 *	is not necessary.
 * 
 *  Paul Hsieh exposition license
 *
 *	The content of all text, figures, tables and displayed layout is
 *	copyrighted by its author and owner Paul Hsieh unless specifically
 *	denoted otherwise. Redistribution is limited to the following conditions:
 *
 *	The redistributor must fully attribute the content's authorship and
 *	make a good faith effort to cite the original location of the original
 *	content.
 *
 *	The content may not be modified via excerpt or otherwise with the
 *	exception of additional citations such as described above without prior
 *	consent of Paul Hsieh.
 *
 *	The content may not be subject to a change in license without prior
 *	consent of Paul Hsieh.
 *
 *	The content may be used for commercial purposes.
 */

#if (defined(__GNUC__) && defined(__i386__)) || defined(_MSC_VER)
/*
 * Unalligned loads are faster if we can do them, otherwise fall back
 * to safer version below.
 */
#   define load16(D) (*((const uint16_t*)(D)))
#else
#   define load16(D) ((((uint32_t)(((const uint8_t*)(D))[1])) << 8) + \
                        (uint32_t)(((const uint8_t*)(D))[0]))
#endif
unsigned int inline typedef_hash(const char *data) {
	uint32_t hash = strlen(data);
	uint32_t size = hash;
	uint32_t temp = 0;
	
	int last;
	if (size <= 0|| data == NULL)
		return -1;
	
	last   = size & 3;
	size >>= 2;
	
	/* main loop */
	for (;size > 0; size--) {
		hash += (load16(data));
		temp  = (load16(data+2) << 11) ^ hash;
		hash  = (hash << 16) ^ temp;
		data += sizeof(uint16_t) << 1;
		hash += hash >> 11;
	}
	
	/* ends */
	switch (last) {
		case 3:
			hash += load16(data);
			hash ^= hash << 16;
			hash ^= ((signed char)data[sizeof(uint16_t)]) << 8;
			hash += hash >> 11;
			break;
		case 2:
			hash += load16(data);
			hash ^= hash << 11;
			hash += hash >> 17;
			break;
		case 1:
			hash += (signed char)*data;
			hash ^= hash << 10;
			hash += hash >> 1;
			break;
	}
	
	/* force avalanching of final 127 bits */
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;
	
	return hash % 1024;
}

typedef_node *typedef_find(const char *s) {
	unsigned int  hash = typedef_hash(s);
	typedef_node *find = typedef_table[hash];
	return find;
}

int typedef_add(const char *from, const char *to) {
	unsigned int  hash = typedef_hash(to);
	typedef_node *find = typedef_table[hash];
	if (find)
		return error(ERROR_PARSE, "typedef for %s already exists\n", to);
	
	/* check if the type exists first */
	if (strncmp(from, "void",   sizeof("void"))   == 0 ||
	    strncmp(from, "string", sizeof("string")) == 0 ||
	    strncmp(from, "float",  sizeof("float"))  == 0 ||
	    strncmp(from, "vector", sizeof("vector")) == 0 ||
	    strncmp(from, "entity", sizeof("entity")) == 0) {
		
		typedef_table[hash]       = mem_a(sizeof(typedef_node));
		typedef_table[hash]->name = strdup(from);
		return -100;
	} else {
		/* search the typedefs for it (typedef-a-typedef?) */
		typedef_node *find = typedef_table[typedef_hash(from)];
		if (find) {
			typedef_table[hash]       = mem_a(sizeof(typedef_node));
			typedef_table[hash]->name = strdup(find->name);
			return -100;
		}
	}
	return error(ERROR_PARSE, "cannot typedef %s (not a type)\n", from);
}
