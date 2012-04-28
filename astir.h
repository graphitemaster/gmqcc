/*
 * Copyright (C) 2012 
 *     Wolfgang Bumiller
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
#ifndef GMQCC_ASTIR_HDR
#define GMQCC_ASTIR_HDR

#define MEM_VECTOR_PROTO(Towner, Tmem, mem)                   \
    bool GMQCC_WARN Towner##_##mem##_add(Towner*, Tmem);      \
    bool GMQCC_WARN Towner##_##mem##_remove(Towner*, size_t);

#define MEM_VECTOR_PROTO_ALL(Towner, Tmem, mem)                    \
    MEM_VECTOR_PROTO(Towner, Tmem, mem)                            \
    bool GMQCC_WARN Towner##_##mem##_find(Towner*, Tmem, size_t*); \
    void Towner##_##mem##_clear(Towner*);

#define MEM_VECTOR_MAKE(Twhat, name) \
    Twhat  *name;                    \
    size_t name##_count;             \
    size_t name##_alloc

#define _MEM_VEC_FUN_ADD(Tself, Twhat, mem)                          \
bool GMQCC_WARN Tself##_##mem##_add(Tself *self, Twhat f)            \
{                                                                    \
    Twhat *reall;                                                    \
    if (self->mem##_count == self->mem##_alloc) {                    \
        if (!self->mem##_alloc) {                                    \
            self->mem##_alloc = 16;                                  \
        } else {                                                     \
            self->mem##_alloc *= 2;                                  \
        }                                                            \
        reall = (Twhat*)mem_a(sizeof(Twhat) * self->mem##_alloc);    \
        if (!reall) {                                                \
            MEM_VECTOR_CLEAR(self, mem);                             \
            return false;                                            \
        }                                                            \
        memcpy(reall, self->mem, sizeof(Twhat) * self->mem##_count); \
        mem_d(self->mem);                                            \
        self->mem = reall;                                           \
    }                                                                \
    self->mem[self->mem##_count++] = f;                              \
    return true;                                                     \
}

#define _MEM_VEC_FUN_REMOVE(Tself, Twhat, mem)                       \
bool GMQCC_WARN Tself##_##mem##_remove(Tself *self, size_t idx)      \
{                                                                    \
    size_t i;                                                        \
    Twhat *reall;                                                    \
    if (idx >= self->mem##_count) {                                  \
        return true; /* huh... */                                    \
    }                                                                \
    for (i = idx; i < self->mem##_count-1; ++i) {                    \
        self->mem[i] = self->mem[i+1];                               \
    }                                                                \
    self->mem##_count--;                                             \
    if (self->mem##_count < self->mem##_count/2)                     \
    {                                                                \
        self->mem##_alloc /= 2;                                      \
        reall = (Twhat*)mem_a(sizeof(Twhat) * self->mem##_count);    \
        if (!reall) {                                                \
            return false;                                            \
        }                                                            \
        memcpy(reall, self->mem, sizeof(Twhat) * self->mem##_count); \
        mem_d(self->mem);                                            \
        self->mem = reall;                                           \
    }                                                                \
    return true;                                                     \
}

#define _MEM_VEC_FUN_FIND(Tself, Twhat, mem)                    \
bool GMQCC_WARN Tself##_##mem##_find(Tself *self, Twhat obj, size_t *idx) \
{                                                               \
    size_t i;                                                   \
    for (i = 0; i < self->mem##_count; ++i) {                   \
        if (self->mem[i] == obj) {                              \
            if (idx) {                                          \
                *idx = i;                                       \
            }                                                   \
            return true;                                        \
        }                                                       \
    }                                                           \
    return false;                                               \
}

#define _MEM_VEC_FUN_CLEAR(Tself, mem)  \
void Tself##_##mem##_clear(Tself *self) \
{                                       \
    if (!self->mem)                     \
        return;                         \
    free((void*) self->mem);            \
    self->mem = NULL;                   \
    self->mem##_count = 0;              \
    self->mem##_alloc = 0;              \
}

#define MEM_VECTOR_CLEAR(owner, mem) \
    if ((owner)->mem)                \
        free((void*)((owner)->mem)); \
    (owner)->mem = NULL;             \
    (owner)->mem##_count = 0;        \
    (owner)->mem##_alloc = 0

#define MEM_VECTOR_INIT(owner, mem) \
{                                   \
    (owner)->mem = NULL;            \
    (owner)->mem##_count = 0;       \
    (owner)->mem##_alloc = 0;       \
}

#define MEM_VEC_FUNCTIONS(Tself, Twhat, mem) \
_MEM_VEC_FUN_REMOVE(Tself, Twhat, mem)       \
_MEM_VEC_FUN_ADD(Tself, Twhat, mem)

#define MEM_VEC_FUNCTIONS_ALL(Tself, Twhat, mem) \
MEM_VEC_FUNCTIONS(Tself, Twhat, mem)             \
_MEM_VEC_FUN_CLEAR(Tself, mem)                   \
_MEM_VEC_FUN_FIND(Tself, Twhat, mem)

enum qc_types {
    /* Main QC types */
    qc_void,
    qc_float,
    qc_vector,
    qc_entity,
    qc_string,

    qc_int,

    /* "virtual" and internal types */
    qc_pointer,
    qc_variant, /* eg. OFS_RETURN/PARAM... */
    qc_function,
};

enum store_types {
    store_global,
    store_local,  /* local, assignable for now, should get promoted later */
    store_value,  /* unassignable */
};

typedef struct {
    float x, y, z;
} vector_t;

/* A shallow copy of a lex_file to remember where which ast node
 * came from.
 */
typedef struct lex_ctx
{
    const char *file;
    size_t     line;
} lex_ctx_t;

#endif
