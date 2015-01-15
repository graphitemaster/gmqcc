#include <string.h>
#include <stdlib.h>

#include "gmqcc.h"

/*
 * strdup does it's own malloc, we need to track malloc. We don't want
 * to overwrite malloc though, infact, we can't really hook it at all
 * without library specific assumptions. So we re implement strdup.
 */
char *stat_mem_strdup(const char *src, bool empty) {
    size_t len = 0;
    char *ptr = nullptr;

    if (!src)
        return nullptr;

    len = strlen(src);
    if ((!empty ? len : true) && (ptr = (char*)mem_a(len + 1))) {
        memcpy(ptr, src, len);
        ptr[len] = '\0';
    }

    return ptr;
}

/*
 * The reallocate function for resizing vectors.
 */
void _util_vec_grow(void **a, size_t i, size_t s) {
    vector_t *d = vec_meta(*a);
    size_t m = 0;
    void *p = nullptr;

    if (*a) {
        m = 2 * d->allocated + i;
        p = mem_r(d, s * m + sizeof(vector_t));
    } else {
        m = i + 1;
        p = mem_a(s * m + sizeof(vector_t));
        ((vector_t*)p)->used = 0;
    }

    d = (vector_t*)p;
    d->allocated = m;
    *a = d + 1;
}

void _util_vec_delete(void *data) {
    mem_d(vec_meta(data));
}

/*
 * Hash table for generic data, based on dynamic memory allocations
 * all around.  This is the internal interface, please look for
 * EXPOSED INTERFACE comment below
 */
struct hash_node_t {
    char *key;   /* the key for this node in table */
    void *value; /* pointer to the data as void*   */
    hash_node_t *next;  /* next node (linked list)        */
};

size_t hash(const char *key);

size_t util_hthash(hash_table_t *ht, const char *key) {
    return hash(key) % ht->size;
}

static hash_node_t *_util_htnewpair(const char *key, void *value) {
    hash_node_t *node;
    if (!(node = (hash_node_t*)mem_a(sizeof(hash_node_t))))
        return nullptr;

    if (!(node->key = util_strdupe(key))) {
        mem_d(node);
        return nullptr;
    }

    node->value = value;
    node->next  = nullptr;

    return node;
}

/*
 * EXPOSED INTERFACE for the hashtable implementation
 * util_htnew(size)                             -- to make a new hashtable
 * util_htset(table, key, value, sizeof(value)) -- to set something in the table
 * util_htget(table, key)                       -- to get something from the table
 * util_htdel(table)                            -- to delete the table
 */
hash_table_t *util_htnew(size_t size) {
    hash_table_t *hashtable = nullptr;

    if (size < 1)
        return nullptr;

    if (!(hashtable = (hash_table_t*)mem_a(sizeof(hash_table_t))))
        return nullptr;

    if (!(hashtable->table = (hash_node_t**)mem_a(sizeof(hash_node_t*) * size))) {
        mem_d(hashtable);
        return nullptr;
    }

    hashtable->size = size;
    memset(hashtable->table, 0, sizeof(hash_node_t*) * size);

    return hashtable;
}

void util_htseth(hash_table_t *ht, const char *key, size_t bin, void *value) {
    hash_node_t *newnode = nullptr;
    hash_node_t *next    = nullptr;
    hash_node_t *last    = nullptr;

    next = ht->table[bin];

    while (next && next->key && strcmp(key, next->key) > 0)
        last = next, next = next->next;

    /* already in table, do a replace */
    if (next && next->key && strcmp(key, next->key) == 0) {
        next->value = value;
    } else {
        /* not found, grow a pair man :P */
        newnode = _util_htnewpair(key, value);
        if (next == ht->table[bin]) {
            newnode->next  = next;
            ht->table[bin] = newnode;
        } else if (!next) {
            last->next = newnode;
        } else {
            newnode->next = next;
            last->next = newnode;
        }
    }
}

void util_htset(hash_table_t *ht, const char *key, void *value) {
    util_htseth(ht, key, util_hthash(ht, key), value);
}

void *util_htgeth(hash_table_t *ht, const char *key, size_t bin) {
    hash_node_t *pair = ht->table[bin];

    while (pair && pair->key && strcmp(key, pair->key) > 0)
        pair = pair->next;

    if (!pair || !pair->key || strcmp(key, pair->key) != 0)
        return nullptr;

    return pair->value;
}

void *util_htget(hash_table_t *ht, const char *key) {
    return util_htgeth(ht, key, util_hthash(ht, key));
}

void *code_util_str_htgeth(hash_table_t *ht, const char *key, size_t bin);
void *code_util_str_htgeth(hash_table_t *ht, const char *key, size_t bin) {
    hash_node_t *pair;
    size_t len, keylen;
    int cmp;

    keylen = strlen(key);

    pair = ht->table[bin];
    while (pair && pair->key) {
        len = strlen(pair->key);
        if (len < keylen) {
            pair = pair->next;
            continue;
        }
        if (keylen == len) {
            cmp = strcmp(key, pair->key);
            if (cmp == 0)
                return pair->value;
            if (cmp < 0)
                return nullptr;
            pair = pair->next;
            continue;
        }
        cmp = strcmp(key, pair->key + len - keylen);
        if (cmp == 0) {
            uintptr_t up = (uintptr_t)pair->value;
            up += len - keylen;
            return (void*)up;
        }
        pair = pair->next;
    }
    return nullptr;
}

/*
 * Free all allocated data in a hashtable, this is quite the amount
 * of work.
 */
void util_htrem(hash_table_t *ht, void (*callback)(void *data)) {
    size_t i = 0;

    for (; i < ht->size; ++i) {
        hash_node_t *n = ht->table[i];
        hash_node_t *p;

        /* free in list */
        while (n) {
            if (n->key)
                mem_d(n->key);
            if (callback)
                callback(n->value);
            p = n;
            n = p->next;
            mem_d(p);
        }

    }
    /* free table */
    mem_d(ht->table);
    mem_d(ht);
}

void util_htrmh(hash_table_t *ht, const char *key, size_t bin, void (*cb)(void*)) {
    hash_node_t **pair = &ht->table[bin];
    hash_node_t *tmp;

    while (*pair && (*pair)->key && strcmp(key, (*pair)->key) > 0)
        pair = &(*pair)->next;

    tmp = *pair;
    if (!tmp || !tmp->key || strcmp(key, tmp->key) != 0)
        return;

    if (cb)
        (*cb)(tmp->value);

    *pair = tmp->next;
    mem_d(tmp->key);
    mem_d(tmp);
}

void util_htrm(hash_table_t *ht, const char *key, void (*cb)(void*)) {
    util_htrmh(ht, key, util_hthash(ht, key), cb);
}

void util_htdel(hash_table_t *ht) {
    util_htrem(ht, nullptr);
}
