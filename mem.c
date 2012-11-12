#include "gmqcc.h"
/*
 * GMQCC does a lot of allocations on shortly lived objects all of which
 * call down to malloc/free internally.  The overhead involved with these
 * allocations makes GMQCC slow. To combat this, a special allocator was
 * in need.  This is an implementation of a user-space buddy allocator
 * that sits ontop of malloc/free.  I'd like to thank Lee Salzman for
 * guiding me in the right direction for designing this.
 */
#define GMQCC_MEM_USED 0xEDCA10A1EDCA10A1
#define GMQCC_MEM_FREE 0xEEF8EEF8EEF8EEF8
#define GMQCC_MEM_CORE 0x00000000000000AA
#define GMQCC_MEM_BSL -1
#define GMQCC_MEM_BSR  1

typedef unsigned long int mem_addr;

static void   *mem_heap = NULL;
static size_t  mem_look = 0;  /* lookup table offset */
static size_t  mem_size = 0;  /* heap size           */

/* read or write to heap */
#define GMQCC_MEM_WRITEHEAP(OFFSET, TYPE, VALUE) *((TYPE *) ((unsigned char*)mem_heap + (OFFSET))) = (VALUE)
#define GMQCC_MEM_READHEAP (OFFSET, TYPE) ((TYPE)*((TYPE *)(((unsigned char*)mem_heap + (OFFSET)))))

/* read of write first block to heap */
#define GMQCC_MEM_WRITEFBA(SIZE, ADDR) GMQCC_MEM_WRITEHEAP(mem_look + (SIZE) * sizeof(mem_addr), mem_addr, ADDR)
#define GMQCC_MEM_READFBA(SIZE)        GMQCC_MEM_READHEAP (mem_look + (SIZE) * sizeof(mem_addr), ADDR)

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

static void mem_init_table(size_t size) {
    size_t i;
    
    mem_look = 8 * ((mem_addr)1 << (size - 1)) + sizeof(mem_addr);
    
    GMQCC_MEM_WRITEHEAP(0,        mem_addr, mem_look);
    GMQCC_MEM_WRITEHEAP(mem_look, mem_addr, size);
    
    /* write pointers to first free bock of said size */
    for (i = 1; i < size; i++)
        GMQCC_MEM_WRITEHEAP(mem_look + sizeof(mem_addr) * i, mem_addr, 0);
        
    GMQCC_MEM_WRITEHEAP(mem_look + sizeof(mem_addr) * size, mem_addr, sizeof(mem_addr));
    GMQCC_MEM_WRITEHEAP(sizeof(mem_addr), mem_addr, size);
    GMQCC_MEM_MARKFREE (sizeof(mem_addr) << 1);
    GMQCC_MEM_WRITEHEAP(sizeof(mem_addr) * 3, mem_addr, 0);
    GMQCC_MEM_WRITEHEAP(sizeof(mem_addr) * 4, mem_addr, 0);
}

/* get needed block size */
static size_t mem_getnbs(const size_t need) {
    size_t b = 8;
    size_t s = 1;
    
    while (need > b) {
        b >>= 1;
        s ++;
    }
    
    return s;
}

void mem_init(size_t size) {
    size_t alloc = 0;
    size_t count = 1;
    size_t block = 1;
    
    if (!(mem_heap = malloc(size)))
        abort();
    
    memset(mem_heap, GMQCC_MEM_CORE, size);
    mem_size = size;
    alloc    = size - (2 * sizeof(mem_addr));
    
    while (alloc + sizeof(mem_addr) > 8 * block) {
        alloc  -= sizeof(mem_addr);
        block <<= 1;
        count  ++;
    }
    
    /* over shot ? */
    block >>= 1;
    count --;
    
    mem_init_table(count);
}

/* doesn't get any simpler :-) */
void mem_destroy() {
    free(mem_heap);
    mem_heap = NULL;
}

void *mem_alloc(size_t amount) {
    size_t   need  = amount + 4 * sizeof(mem_addr);
    size_t   size  = mem_getnbs    (need);
    mem_addr block = mem_allocblock(size);
    if (!block) return NULL;
    
    return mem_heap + block + 4 * sizeof(mem_addr);
}
