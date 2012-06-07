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
#include "gmqcc.h"
static typedef_node *typedef_table[1024];

void typedef_init() {
    int i;
    for(i = 0; i < sizeof(typedef_table)/sizeof(*typedef_table); i++)
        typedef_table[i] = NULL;
}

uint32_t typedef_hash(const char *s) {
    return util_crc32(s, strlen(s), 1024);
}

typedef_node *typedef_find(const char *s) {
    unsigned int  hash = typedef_hash(s);
    typedef_node *find = typedef_table[hash];
    return find;
}

void typedef_clear() {
    int i;
    for(i = 1024; i > 0; i--) {
        if(typedef_table[i]) {
            mem_d(typedef_table[i]->name);
            mem_d(typedef_table[i]);
        }
    }
}

int typedef_add(lex_file *file, const char *from, const char *to) {
    unsigned int  hash = typedef_hash(to);
    typedef_node *find = typedef_table[hash];

    if (find)
        return error(file, ERROR_PARSE, "typedef for %s already exists or conflicts\n", to);

    /* check if the type exists first */
    if (strncmp(from, "float",  sizeof("float"))  == 0 ||
        strncmp(from, "vector", sizeof("vector")) == 0 ||
        strncmp(from, "string", sizeof("string")) == 0 ||
        strncmp(from, "entity", sizeof("entity")) == 0 ||
        strncmp(from, "void",   sizeof("void"))   == 0) {

        typedef_table[hash] = mem_a(sizeof(typedef_node));
        if (typedef_table[hash])
            typedef_table[hash]->name = util_strdup(from);
        else
            return error(file, ERROR_PARSE, "ran out of resources for typedef %s\n", to);
        return -100;
    } else {
        /* search the typedefs for it (typedef-a-typedef?) */
        find = typedef_table[typedef_hash(from)];
        if (find) {
            typedef_table[hash] = mem_a(sizeof(typedef_node));
            if (typedef_table[hash])
                typedef_table[hash]->name = util_strdup(find->name);
            else
                return error(file, ERROR_PARSE, "ran out of resources for typedef %s\n", to);
            return -100;
        }
    }
    return error(file, ERROR_PARSE, "cannot typedef `%s` (not a type)\n", from);
}
