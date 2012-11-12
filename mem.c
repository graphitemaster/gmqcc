#include "gmqcc.h"
#include <assert.h>
/*
 * GMQCC does a lot of allocations on shortly lived objects all of which
 * call down to malloc/free internally.  The overhead involved with these
 * allocations makes GMQCC slow. To combat this, a special allocator was
 * in need.  This is an implementation of a user-space buddy allocator
 * that sits ontop of malloc/free.  I'd like to thank Lee Salzman for
 * guiding me in the right direction for designing this.
 */
#define GMQCC_MEM_USED  0xEDCA10A1EDCA10A1
#define GMQCC_MEM_FREE  0xEEF8EEF8EEF8EEF8
#define GMQCC_MEM_CORE  0x00000000000000AA
#define GMQCC_MEM_BSL  -1
#define GMQCC_MEM_BSR   1
#define GMQCC_MEM_DEBUG 1

#ifdef  GMQCC_MEM_DEBUG
#   include <stdio.h>
#   define GMQCC_MEM_TRACE(TAG, ...)            \
    do {                                        \
        printf("[mem:%s]: %s ", TAG, __func__); \
        printf(__VA_ARGS__);                    \
        printf("\n");                           \
    } while (0)
#else
#   define GMQCC_MEM_TRACE(TAG, ...)
#endif

typedef unsigned long int mem_addr;

static void   *mem_heap = NULL;
static size_t  mem_look = 0;  /* lookup table offset */
static size_t  mem_size = 0;  /* heap size           */

/* read or write to heap */
#define GMQCC_MEM_WRITEHEAP(OFFSET, TYPE, VALUE) *((TYPE *) ((unsigned char*)mem_heap + (OFFSET))) = (VALUE)
#define GMQCC_MEM_READHEAP(OFFSET, TYPE)  ((TYPE)*((TYPE *)(((unsigned char*)mem_heap + (OFFSET)))))

/* read of write first block to heap */
#define GMQCC_MEM_WRITEFBA(SIZE, ADDR) GMQCC_MEM_WRITEHEAP(mem_look + (SIZE) * sizeof(mem_addr), mem_addr, ADDR)
#define GMQCC_MEM_READFBA(SIZE)        GMQCC_MEM_READHEAP (mem_look + (SIZE) * sizeof(mem_addr), mem_addr)

/* read and write block sizes from heap */
#define GMQCC_MEM_WRITEBS(ADDR, SIZE) GMQCC_MEM_WRITEHEAP(ADDR, mem_addr, (SIZE))
#define GMQCC_MEM_READBS(ADDR)        GMQCC_MEM_READHEAP (ADDR, mem_addr);
    
/*
 * Getting address of previous/following siblings, as well as
 * setting address of previous/following siblings.
 */
#define GMQCC_MEM_GETADDROFPS(ADDR)   GMQCC_MEM_READHEAP ((ADDR) + 2 * sizeof(mem_addr), mem_addr)
#define GMQCC_MEM_GETADDROFFS(ADDR)   GMQCC_MEM_READHEAP ((ADDR) + 3 * sizeof(mem_addr), mem_addr)
#define GMQCC_MEM_SETADDROFPS(ADDR,V) GMQCC_MEM_WRITEHEAP((ADDR) + 2 * sizeof(mem_addr), mem_addr, V)
#define GMQCC_MEM_SETADDROFFS(ADDR,V) GMQCC_MEM_WRITEHEAP((ADDR) + 3 * sizeof(mem_addr), mem_addr, V)

/* Marking blocks as used or free */
#define GMQCC_MEM_MARKUSED(ADDR) GMQCC_MEM_WRITEHEAP((ADDR) + 1 * sizeof(mem_addr), mem_addr, GMQCC_MEM_USED)
#define GMQCC_MEM_MARKFREE(ADDR) GMQCC_MEM_WRITEHEAP((ADDR) + 1 * sizeof(mem_addr), mem_addr, GMQCC_MEM_FREE)

/* Has block? */
#define GMQCC_MEM_HASBLOCK(SIZE) (GMQCC_MEM_READFBA(size) != 0)

static void mem_init_table(size_t size) {
    GMQCC_MEM_TRACE("flow", "(%lu)", size);
    size_t i;
    
    mem_look = 8 * ((mem_addr)1 << (size - 1)) + sizeof(mem_addr);
    
    GMQCC_MEM_WRITEHEAP(0,        mem_addr, mem_look);
    GMQCC_MEM_WRITEHEAP(mem_look, mem_addr, size);
    
    /* write pointers to first free bock of said size */
    for (i = 1; i < size; i++)
        GMQCC_MEM_WRITEHEAP(mem_look + sizeof(mem_addr) * i, mem_addr, 0);
        
    GMQCC_MEM_WRITEHEAP(mem_look + sizeof(mem_addr) * size, mem_addr, sizeof(mem_addr));
    GMQCC_MEM_WRITEHEAP(sizeof(mem_addr), mem_addr, size);
    GMQCC_MEM_MARKFREE (sizeof(mem_addr) * 2);
    GMQCC_MEM_WRITEHEAP(sizeof(mem_addr) * 3, mem_addr, 0);
    GMQCC_MEM_WRITEHEAP(sizeof(mem_addr) * 4, mem_addr, 0);
}

/* get needed block size */
static size_t mem_getnbs(const size_t need) {
    size_t b = 8;
    size_t s = 1;
    
    while (need > b) {
        b *= 2;
        s ++;
    }
    
    return s;
}

static void mem_removeblock(mem_addr a, size_t size) {
    mem_addr p = GMQCC_MEM_GETADDROFPS(a);
    mem_addr n = GMQCC_MEM_GETADDROFFS(a);
    
    GMQCC_MEM_SETADDROFPS(a, ~((mem_addr)0));
    GMQCC_MEM_SETADDROFFS(a, ~((mem_addr)0));
    
    /* handle singles in list */
    if ((p == 0) && (n == 0)) {
        GMQCC_MEM_WRITEFBA(size, 0);
        return;
    }
    
    /* first in list has different sibling semantics */
    if (p == 0) {
        GMQCC_MEM_WRITEFBA   (size, n);
        GMQCC_MEM_SETADDROFPS(n, 0);
        return;
    }
    
    /* last item also has special meaning :) */
    if (n == 0) {
        GMQCC_MEM_SETADDROFFS(p, 0);
        return;
    }
    
    /* middle of list */
    GMQCC_MEM_SETADDROFPS(n, p);
    GMQCC_MEM_SETADDROFFS(p, n);
}

static int mem_createblock(const size_t size) {
    mem_addr parent;
    int      test;
    
    GMQCC_MEM_TRACE("flow", "(%lu)", size);
    if (GMQCC_MEM_HASBLOCK(size))
        return 0;
        
    if (size > GMQCC_MEM_READHEAP(mem_look, mem_addr))
        abort();

    /* recrusive ... */
    if ((test = mem_createblock(size + 1)) != 0)
        return test;
        
    /* safe splits assured */
    parent = GMQCC_MEM_READFBA(size + 1);
    mem_removeblock(parent, size + 1);
    
    /* split it */
    GMQCC_MEM_WRITEFBA(size, parent);
    {
        /* find center and split */
        mem_addr block = parent + 8 * ((mem_addr)1 << (size - 1));
        mem_addr left  = parent;
        mem_addr right = block;
        
        GMQCC_MEM_TRACE(
            "dump",
            "block info:\n    left  addr: %lu\n    right addr: %lu\n    prev  addr: %lu",
            left, right, parent
        );
        
        /* left half  */
        GMQCC_MEM_WRITEHEAP  (left, mem_addr, size);
        GMQCC_MEM_MARKFREE   (left);
        GMQCC_MEM_SETADDROFPS(left, 0);
        GMQCC_MEM_SETADDROFFS(left, right);
        /* right half */
        GMQCC_MEM_WRITEHEAP  (right, mem_addr, size);
        GMQCC_MEM_MARKFREE   (right);
        GMQCC_MEM_SETADDROFPS(right, left);
        GMQCC_MEM_SETADDROFPS(right, 0);
    }
    return 0;
}

static mem_addr mem_allocblock(const size_t size) {
    GMQCC_MEM_TRACE("flow", "(%lu)", size);
    int      test = mem_createblock(size);
    mem_addr first;
    mem_addr next;
    
    if (test != 0)
        return 0;
    
    /* first free one */
    first = GMQCC_MEM_READFBA    (size);
    next  = GMQCC_MEM_GETADDROFFS(first);
    
    mem_removeblock(first, size);
    
    GMQCC_MEM_WRITEFBA(next, size);
    GMQCC_MEM_MARKUSED(first);
    
    return first;
}

static int mem_getside(mem_addr addr, const size_t size) {
    size_t  real = addr - sizeof(mem_addr);
    size_t  next = ((mem_addr)1 << (size));
    assert((real % 8) == 0); /* blow up */
    real /= 8;
    
    return ((real % next) == 0)? GMQCC_MEM_BSL : GMQCC_MEM_BSR;
}

static mem_addr mem_getaddr(mem_addr start, const size_t size) {
    size_t length = (((mem_addr)1 << (size - 1)) * 8);
    
    switch (mem_getside(start, size)) {
        case GMQCC_MEM_BSL: return start + length;
        case GMQCC_MEM_BSR: return start - length;
    }
    /* if reached blow up */
    return (abort(), 1);
}

static void mem_addblock(mem_addr a, size_t s) {
    mem_addr first = GMQCC_MEM_READFBA(s);
    if (first == 0) {
        /* only block */
        GMQCC_MEM_WRITEFBA   (s, a);
        GMQCC_MEM_SETADDROFPS(a, 0);
        GMQCC_MEM_SETADDROFFS(a, 0);
    } else {
        /* add to front */
        GMQCC_MEM_WRITEFBA   (s, a);
        GMQCC_MEM_SETADDROFPS(a, 0);
        GMQCC_MEM_SETADDROFFS(a, first);
        GMQCC_MEM_SETADDROFPS(first, a);
    }
}

void mem_init(size_t size) {
    size_t alloc = size;
    size_t count = 1;
    size_t block = 1;
    
    /* blow up if too small */
    assert (sizeof(void*) == sizeof(mem_addr));
    
    if (!(mem_heap = malloc(size)))
        abort();
    
    memset(mem_heap, GMQCC_MEM_CORE, size);
    mem_size = size;
    alloc    -= 2 * sizeof(mem_addr);
    
    while (alloc + sizeof(mem_addr) > 8 * block) {
        alloc -= sizeof(mem_addr);
        block *= 2;
        count ++;
    }
    
    /* over shot ? */
    block /= 2;
    count --;
    
    mem_init_table(count);
}

/* doesn't get any simpler :-) */
void mem_destroy() {
    free(mem_heap);
    mem_heap = NULL;
}

void *mem_alloc(size_t amount) {
    GMQCC_MEM_TRACE("flow", "(%lu)", amount);
    size_t   need  = amount + 4 * sizeof(mem_addr);
    size_t   size  = mem_getnbs    (need);
    mem_addr block = mem_allocblock(size);
    
    GMQCC_MEM_TRACE("dump", "will allocate %lu size block", size);
    /* standard behaviour */
    if (block == 0)
        return NULL;
    GMQCC_MEM_TRACE("dump", "returning offset %lu", block);
    return mem_heap + block + 4 * sizeof(mem_addr);
}

void mem_free(void *ptr) {
    mem_addr start = (mem_addr)(ptr - mem_heap) - 4 * sizeof(mem_addr);
    size_t   size  = GMQCC_MEM_READHEAP(start, mem_addr);
    mem_addr addr  = mem_getaddr(start, size);
    int      side  = mem_getside(start, size);
    
    
    GMQCC_MEM_TRACE (
        "dump",
        "deallocating %s buddy (neighbour at %lu)",
        (side == GMQCC_MEM_BSL) ? "left" : "right",
        addr
    );
    GMQCC_MEM_MARKFREE(start);
    
    /* while free block merge */
    while ((GMQCC_MEM_READHEAP(addr + 1 * sizeof(mem_addr), mem_addr)) == (mem_addr)GMQCC_MEM_FREE) {
        GMQCC_MEM_TRACE("dump", "merging ...");
        mem_removeblock(addr, size);
        
        /* find new start */
        start = addr < start ? addr : start;
        size ++;
        
        if (size == GMQCC_MEM_READHEAP(mem_look, mem_addr))
            break; /* blow up */
            
        addr = mem_getaddr(start, size);
        GMQCC_MEM_TRACE("dump", "new block start is %lu, buddy at %lu", start, addr);
    }
    
    /* add it */
    GMQCC_MEM_WRITEBS(start, size);
    mem_addblock     (start, size);
}

#include <stdio.h>
int main() {
    mem_init(1330);
    char *p = mem_alloc(sizeof(char) * 5);
    mem_free(p);
    mem_destroy();
    /* blows up on second alloc, why?  char *x = mem_alloc(200); */
}
