#ifndef ASTIR_COMMON_H__
#define ASTIR_COMMON_H__

#define MEM_VECTOR_PROTO(Towner, Tmem, mem)        \
    void Towner##_##mem##_add(Towner*, Tmem);      \
    void Towner##_##mem##_remove(Towner*, size_t);

#define MEM_VECTOR_PROTO_ALL(Towner, Tmem, mem)          \
    MEM_VECTOR_PROTO(Towner, Towner, Tmem, mem)          \
    qbool Towner##_##mem##_find(Towner*, Tmem, size_t*); \
    void Towner##_##mem##_clear(Towner*);

#define MEM_VECTOR_MAKE(Twhat, name) \
    Twhat  *name;                    \
    size_t name##_count;             \
    size_t name##_alloc

#define _MEM_VEC_FUN_ADD(Tself, Twhat, mem)       \
void Tself##_##mem##_add(Tself *self, Twhat f)    \
{                                                 \
    if (self->mem##_count == self->mem##_alloc) { \
        if (!self->mem##_alloc)                   \
            self->mem##_alloc = 16;               \
        else                                      \
            self->mem##_alloc *= 2;               \
        self->mem = (Twhat*)realloc(self->mem,    \
            sizeof(Twhat) * self->mem##_alloc);   \
    }                                             \
    self->mem[self->mem##_count++] = f;           \
}

#define _MEM_VEC_FUN_REMOVE(Tself, Twhat, mem)       \
void Tself##_##mem##_remove(Tself *self, size_t idx) \
{                                                    \
    size_t i;                                        \
    if (idx >= self->mem##_count)                    \
        return;                                      \
    for (i = idx; i < self->mem##_count-1; ++i)      \
        self->mem[i] = self->mem[i+1];               \
    self->mem##_count--;                             \
    if (self->mem##_count < self->mem##_count/2)     \
    {                                                \
        self->mem##_alloc /= 2;                      \
        self->mem = (Twhat*)realloc(self->mem,       \
            self->mem##_alloc * sizeof(Twhat));      \
    }                                                \
}

#define _MEM_VEC_FUN_FIND(Tself, Twhat, mem)                    \
qbool Tself##_##mem##_find(Tself *self, Twhat obj, size_t *idx) \
{                                                               \
    size_t i;                                                   \
    for (i = 0; i < self->mem##_count; ++i) {                   \
        if (self->mem[i] == obj) {                              \
            if (idx)                                            \
                *idx = i;                                       \
            return itrue;                                       \
        }                                                       \
    }                                                           \
    return ifalse;                                              \
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
_MEM_VEC_FUN_CLEAR(Tself, Twhat, mem)            \
_MEM_VEC_FUN_FIND(Tself, Twhat, mem)

typedef enum { false, true } qbool;

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
