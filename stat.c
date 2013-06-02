#include "gmqcc.h"

/*
 * GMQCC performs tons of allocations, constructions, and crazyness
 * all around. When trying to optimizes systems, or just get fancy
 * statistics out of the compiler, it's often printf mess. This file
 * implements the statistics system of the compiler. I.E the allocator
 * we use to track allocations, and other systems of interest.
 */
#define ST_SIZE 1024

typedef struct stat_mem_block_s {
    const char              *file;
    size_t                   line;
    size_t                   size;
    struct stat_mem_block_s *next;
    struct stat_mem_block_s *prev;
} stat_mem_block_t;

static uint64_t          stat_mem_allocated         = 0;
static uint64_t          stat_mem_deallocated       = 0;
static uint64_t          stat_mem_allocated_total   = 0;
static uint64_t          stat_mem_deallocated_total = 0;
static uint64_t          stat_mem_high              = 0;
static uint64_t          stat_mem_peak              = 0;
static uint64_t          stat_used_strdups          = 0;
static uint64_t          stat_used_vectors          = 0;
static uint64_t          stat_used_hashtables       = 0;
static uint64_t          stat_type_vectors          = 0;
static uint64_t          stat_type_hashtables       = 0;
static stat_size_table_t stat_size_vectors          = NULL;
static stat_size_table_t stat_size_hashtables       = NULL;
static stat_mem_block_t *stat_mem_block_root        = NULL;

/*
 * A basic header of information wrapper allocator. Simply stores
 * information as a header, returns the memory + 1 past it, can be
 * retrieved again with - 1. Where type is stat_mem_block_t*.
 */
void *stat_mem_allocate(size_t size, size_t line, const char *file) {
    stat_mem_block_t *info = (stat_mem_block_t*)malloc(sizeof(stat_mem_block_t) + size);
    void             *data = (void*)(info + 1);
    
    if(!info)
        return NULL;
        
    info->line = line;
    info->size = size;
    info->file = file;
    info->prev = NULL;
    info->next = stat_mem_block_root;
    
    if (stat_mem_block_root)
        stat_mem_block_root->prev = info;
        
    stat_mem_block_root       = info;
    stat_mem_allocated       += size;
    stat_mem_high            += size;
    stat_mem_allocated_total ++;
    
    if (stat_mem_high > stat_mem_peak)
        stat_mem_peak = stat_mem_high;

    return data;
}

void stat_mem_deallocate(void *ptr) {
    stat_mem_block_t *info = NULL;
    
    if (!ptr)
        return;
        
    info = ((stat_mem_block_t*)ptr - 1);
    
    stat_mem_deallocated       += info->size;
    stat_mem_high              -= info->size;
    stat_mem_deallocated_total ++;
    
    if (info->prev) info->prev->next = info->next;
    if (info->next) info->next->prev = info->prev;
    
    /* move ahead */
    if (info == stat_mem_block_root)
        stat_mem_block_root = info->next;
        
    free(info);
}

void *stat_mem_reallocate(void *ptr, size_t size, size_t line, const char *file) {
    stat_mem_block_t *oldinfo = NULL;
    stat_mem_block_t *newinfo;
    
    if (!ptr)
        return stat_mem_allocate(size, line, file);
    
    /* stay consistent with glic */
    if (!size) {
        stat_mem_deallocate(ptr);
        return NULL;
    }
    
    oldinfo = ((stat_mem_block_t*)ptr - 1);
    newinfo = ((stat_mem_block_t*)malloc(sizeof(stat_mem_block_t) + size));
    
    if (!newinfo) {
        stat_mem_deallocate(ptr);
        return NULL;
    }
    
    memcpy(newinfo+1, oldinfo+1, oldinfo->size);
    
    if (oldinfo->prev) oldinfo->prev->next = oldinfo->next;
    if (oldinfo->next) oldinfo->next->prev = oldinfo->prev;
    
    /* move ahead */
    if (oldinfo == stat_mem_block_root)
        stat_mem_block_root = oldinfo->next;
        
    newinfo->line = line;
    newinfo->size = size;
    newinfo->file = file;
    newinfo->prev = NULL;
    newinfo->next = stat_mem_block_root;
    
    if (stat_mem_block_root)
        stat_mem_block_root->prev = newinfo;
    
    stat_mem_block_root = newinfo;
    stat_mem_allocated -= oldinfo->size;
    stat_mem_high      -= oldinfo->size;
    stat_mem_allocated += newinfo->size;
    stat_mem_high      += newinfo->size;
    
    if (stat_mem_high > stat_mem_peak)
        stat_mem_peak = stat_mem_high;
        
    free(oldinfo);
    
    return newinfo + 1;
}

/*
 * strdup does it's own malloc, we need to track malloc. We don't want
 * to overwrite malloc though, infact, we can't really hook it at all
 * without library specific assumptions. So we re implement strdup.
 */
char *stat_mem_strdup(const char *src, size_t line, const char *file, bool empty) {
    size_t len = 0;
    char  *ptr = NULL;
    
    if (!src)
        return NULL;
    
    len = strlen(src);
    if (((!empty) ? len : true) && (ptr = (char*)stat_mem_allocate(len + 1, line, file))) {
        memcpy(ptr, src, len);
        ptr[len] = '\0';
    }
    
    stat_used_strdups ++;
    return ptr;
}

/*
 * The reallocate function for resizing vectors.
 */
void _util_vec_grow(void **a, size_t i, size_t s) {
    vector_t          *d = vec_meta(*a);
    size_t             m = 0;
    stat_size_entry_t *e = NULL;
    void              *p = NULL;
    
    if (*a) {
        m = 2 * d->allocated + i;
        p = mem_r(d, s * m + sizeof(vector_t));
    } else {
        m = i + 1;
        p = mem_a(s * m + sizeof(vector_t));
        ((vector_t*)p)->used = 0;
        stat_used_vectors++;
    }
    
    if (!stat_size_vectors)
        stat_size_vectors = stat_size_new();

    if ((e = stat_size_get(stat_size_vectors, s))) {
        e->value ++;
    } else {
        stat_size_put(stat_size_vectors, s, 1); /* start off with 1 */
        stat_type_vectors++;
    }

    *a = (vector_t*)p + 1;
    vec_meta(*a)->allocated = m;
}

/*
 * Hash table for generic data, based on dynamic memory allocations
 * all around.  This is the internal interface, please look for
 * EXPOSED INTERFACE comment below
 */
typedef struct hash_node_t {
    char               *key;   /* the key for this node in table */
    void               *value; /* pointer to the data as void*   */
    struct hash_node_t *next;  /* next node (linked list)        */
} hash_node_t;

GMQCC_INLINE size_t util_hthash(hash_table_t *ht, const char *key) {
    const uint32_t       mix   = 0x5BD1E995;
    const uint32_t       rot   = 24;
    size_t               size  = strlen(key);
    uint32_t             hash  = 0x1EF0 /* LICRC TAB */  ^ size;
    uint32_t             alias = 0;
    const unsigned char *data  = (const unsigned char*)key;

    while (size >= 4) {
        alias  = (data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
        alias *= mix;
        alias ^= alias >> rot;
        alias *= mix;

        hash  *= mix;
        hash  ^= alias;

        data += 4;
        size -= 4;
    }

    switch (size) {
        case 3: hash ^= data[2] << 16;
        case 2: hash ^= data[1] << 8;
        case 1: hash ^= data[0];
                hash *= mix;
    }

    hash ^= hash >> 13;
    hash *= mix;
    hash ^= hash >> 15;

    return (size_t) (hash % ht->size);
}

static hash_node_t *_util_htnewpair(const char *key, void *value) {
    hash_node_t *node;
    if (!(node = (hash_node_t*)mem_a(sizeof(hash_node_t))))
        return NULL;

    if (!(node->key = util_strdupe(key))) {
        mem_d(node);
        return NULL;
    }

    node->value = value;
    node->next  = NULL;

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
    hash_table_t      *hashtable = NULL;
    stat_size_entry_t *find      = NULL;
    
    if (size < 1)
        return NULL;
        
    if (!stat_size_hashtables)
        stat_size_hashtables = stat_size_new();

    if (!(hashtable = (hash_table_t*)mem_a(sizeof(hash_table_t))))
        return NULL;

    if (!(hashtable->table = (hash_node_t**)mem_a(sizeof(hash_node_t*) * size))) {
        mem_d(hashtable);
        return NULL;
    }
    
    if ((find = stat_size_get(stat_size_hashtables, size)))
        find->value++;
    else {
        stat_used_hashtables++;
        stat_size_put(stat_size_hashtables, size, 1);
    }

    hashtable->size = size;
    memset(hashtable->table, 0, sizeof(hash_node_t*) * size);

    stat_type_hashtables++;
    return hashtable;
}

void util_htseth(hash_table_t *ht, const char *key, size_t bin, void *value) {
    hash_node_t *newnode = NULL;
    hash_node_t *next    = NULL;
    hash_node_t *last    = NULL;

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
        return NULL;

    return pair->value;
}

void *util_htget(hash_table_t *ht, const char *key) {
    return util_htgeth(ht, key, util_hthash(ht, key));
}

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
                return NULL;
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
    return NULL;
}

/*
 * Free all allocated data in a hashtable, this is quite the amount
 * of work.
 */
void util_htrem(hash_table_t *ht, void (*callback)(void *data)) {
    size_t i = 0;
    for (; i < ht->size; i++) {
        hash_node_t *n = ht->table[i];
        hash_node_t *p;

        /* free in list */
        while (n) {
            if (n->key)
                mem_d(n->key);
            if (callback)
                callback(n->value);
            p = n;
            n = n->next;
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
    util_htrem(ht, NULL);
}

/*
 * A tiny size_t key-value hashtbale for tracking vector and hashtable
 * sizes. We can use it for other things too, if we need to. This is
 * very TIGHT, and efficent in terms of space though.
 */
stat_size_table_t stat_size_new() {
    return (stat_size_table_t)memset(
        mem_a(sizeof(stat_size_entry_t*) * ST_SIZE),
        0, ST_SIZE * sizeof(stat_size_entry_t*)
    );
}

void stat_size_del(stat_size_table_t table) {
    size_t i = 0;
    for (; i < ST_SIZE; i++) if(table[i]) mem_d(table[i]);
    mem_d(table);
}

stat_size_entry_t *stat_size_get(stat_size_table_t table, size_t key) {
    size_t hash = (key % ST_SIZE);
    while (table[hash] && table[hash]->key != key)
        hash = (hash + 1) % ST_SIZE;
    return table[hash];
}
void stat_size_put(stat_size_table_t table, size_t key, size_t value) {
    size_t hash = (key % ST_SIZE);
    while (table[hash] && table[hash]->key != key)
        hash = (hash + 1) % ST_SIZE;
    table[hash] = (stat_size_entry_t*)mem_a(sizeof(stat_size_entry_t));
    table[hash]->key = key;
    table[hash]->value = value;
}

/*
 * The following functions below implement printing / dumping of statistical
 * information.
 */
static void stat_dump_mem_contents(stat_mem_block_t *memory, uint16_t cols) {
    uint32_t i, j;
    for (i = 0; i < memory->size + ((memory->size % cols) ? (cols - memory->size % cols) : 0); i++) {
        if (i % cols == 0)    con_out(" 0x%06X: ", i);
        if (i < memory->size) con_out("%02X " , 0xFF & ((unsigned char*)(memory + 1))[i]);
        else                  con_out(" ");

        if ((uint16_t)(i % cols) == (cols - 1)) {
            for (j = i - (cols - 1); j <= i; j++) {
                con_out("%c",
                    (j >= memory->size)
                        ? ' '
                        : (isprint(((unsigned char*)(memory + 1))[j]))
                            ? 0xFF & ((unsigned char*)(memory + 1)) [j]
                            : '.'
                );
            }
            con_out("\n");
        }
    }
}

static void stat_dump_mem_leaks() {
    stat_mem_block_t *info;
    for (info = stat_mem_block_root; info; info = info->next) {
        con_out("lost: %u (bytes) at %s:%u\n",
            info->size,
            info->file,
            info->line
        );
        
        stat_dump_mem_contents(info, OPTS_OPTION_U16(OPTION_MEMDUMPCOLS));
    }
}

static void stat_dump_mem_info() {
    con_out("Memory information:\n\
    Total allocations:   %llu\n\
    Total deallocations: %llu\n\
    Total allocated:     %f (MB)\n\
    Total deallocated:   %f (MB)\n\
    Total peak memory:   %f (MB)\n\
    Total leaked memory: %f (MB) in %llu allocations\n",
        stat_mem_allocated_total,
        stat_mem_deallocated_total,
        (float)(stat_mem_allocated)                        / 1048576.0f,
        (float)(stat_mem_deallocated)                      / 1048576.0f,
        (float)(stat_mem_peak)                             / 1048576.0f,
        (float)(stat_mem_allocated - stat_mem_deallocated) / 1048576.0f,
        stat_mem_allocated_total - stat_mem_deallocated_total
    );
}

static void stat_dump_stats_table(stat_size_table_t table, const char *string, uint64_t *size) {
    size_t i,j;
    
    for (i = 0, j = 0; i < ST_SIZE; i++) {
        stat_size_entry_t *entry;

        if (!(entry = table[i]))
            continue;

        con_out(string, (unsigned)j, (unsigned)entry->key, (unsigned)entry->value);
        j++;
        
        if (size)
            *size += entry->key * entry->value;
    }
}

void stat_info() {
    if (OPTS_OPTION_BOOL(OPTION_DEBUG))
        stat_dump_mem_leaks();

    if (OPTS_OPTION_BOOL(OPTION_DEBUG) ||
        OPTS_OPTION_BOOL(OPTION_MEMCHK))
        stat_dump_mem_info();

    if (OPTS_OPTION_BOOL(OPTION_MEMCHK) ||
        OPTS_OPTION_BOOL(OPTION_STATISTICS)) {
        uint64_t mem;
        
        con_out("\nAdditional Statistics:\n\
    Total vectors allocated:    %llu\n\
    Total string duplicates:    %llu\n\
    Total hashtables allocated: %llu\n\
    Total unique vector sizes:  %llu\n",
            stat_used_vectors,
            stat_used_strdups,
            stat_used_hashtables,
            stat_type_vectors
        );
        
        stat_dump_stats_table (
            stat_size_vectors,
            "        %2u| # of %4u byte vectors: %u\n",
            &mem
        );
        
        con_out (
            "    Total unique hashtable sizes: %llu\n",
            stat_type_hashtables
        );
        
        stat_dump_stats_table (
            stat_size_hashtables,
            "        %2u| # of %4u element hashtables: %u\n",
            NULL
        );
        
        con_out (
            "    Total vector memory:          %f (MB)\n",
            (float)(mem) / 1048576.0f
        );
    }

    if (stat_size_vectors)
        stat_size_del(stat_size_vectors);
    if (stat_size_hashtables)
        stat_size_del(stat_size_hashtables);
}
#undef ST_SIZE
