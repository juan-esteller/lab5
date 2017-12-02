#include "memory_block.h"
#include "fully_associative.h"
#include <stdint.h> 

fully_associative_cache* fac_init(main_memory* mm) {
    fully_associative_cache* fac = malloc(sizeof(fully_associative_cache));
    fac->mm = mm; 
    fac->cs = cs_init();
    for (int i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++) {
        fac->ways[i] = NULL;
        fac->dirty_bits[i] = 0; 
        fac->places[i] = FULLY_ASSOCIATIVE_NUM_WAYS;
    }
    fac->ways_full = 0; 
    return fac;
}

static void mark_as_used(fully_associative_cache* fac, int way) {
    for (int i = 0; i < fac->ways_full; i++) {
        if (i != way && fac->places[i] < fac->places[way]) {
            fac->places[i]++; 
        }
    }
    fac->places[way] = 1; 
}

static int lru(fully_associative_cache* fac) {
    assert(fac->ways_full == FULLY_ASSOCIATIVE_NUM_WAYS); 
    int way = -1;  
    
    for (int i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++) {
        if (fac->places[i] == FULLY_ASSOCIATIVE_NUM_WAYS) {
            way = i; 
            break; 
        }
    }
    assert(way >= 0);
    return way; 
}

// load_fac_cache(fac, addr)
//      check for address `addr` in a fully associative cache. 
//      and load it into the cache if necessary.  
//      return 1 in the case of a cache miss.  
static int load_fac_cache(fully_associative_cache* fac, void* addr, int* way) {
    void* block_start = (void*) (((uintptr_t) addr) - 
                            ((uintptr_t) addr) % MAIN_MEMORY_BLOCK_SIZE);

    for (int i = 0; i < fac->ways_full; i++) {
       if (fac->ways[i]->start_addr == block_start) {
           *way = i;
           return 0; 
        }
    }

    if (fac->ways_full < FULLY_ASSOCIATIVE_NUM_WAYS) {
        fac->ways[fac->ways_full] = mm_read(fac->mm, block_start); 
        fac->dirty_bits[fac->ways_full] = 0; 
        *way = fac->ways_full; 
        fac->ways_full++; 
    } else {
        int evicted = lru(fac); 
        
        if (fac->dirty_bits[evicted]) {
            mm_write(fac->mm, fac->ways[evicted]->start_addr, fac->ways[evicted]);
        }
        mb_free(fac->ways[evicted]);
        fac->ways[evicted]= mm_read(fac->mm, block_start); 
        fac->dirty_bits[evicted] = 0; 
        *way = evicted; 
    }
    return 1; 
}

void fac_store_word(fully_associative_cache* fac, void* addr, unsigned int val) {
    fac->cs.w_queries++; 
    int way; 
    fac->cs.w_misses += load_fac_cache(fac, addr, &way);
    fac->dirty_bits[way] = 1; 
    mark_as_used(fac, way); 
    uintptr_t block_start = (uintptr_t) addr - 
                                (((uintptr_t) addr) % MAIN_MEMORY_BLOCK_SIZE);
    size_t offset = (uintptr_t) addr - block_start; 
    *((unsigned int*) (fac->ways[way]->data + offset)) = val; 
}


unsigned int fac_load_word(fully_associative_cache* fac, void* addr) {
    fac->cs.r_queries++; 
    int way; 
    fac->cs.r_misses += load_fac_cache(fac, addr, &way);
    mark_as_used(fac, way); 
    uintptr_t block_start = (uintptr_t) addr - 
                                (((uintptr_t) addr) % MAIN_MEMORY_BLOCK_SIZE);
    size_t offset = (uintptr_t) addr - block_start; 
    return *((unsigned int*) (fac->ways[way]->data + offset)); 
}

void fac_free(fully_associative_cache* fac) {
    // TODO
}
