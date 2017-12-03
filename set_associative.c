#include <stdint.h>
#include "memory_block.h"
#include "set_associative.h"

set_associative_cache* sac_init(main_memory* mm) {
    set_associative_cache* sac = malloc(sizeof(set_associative_cache)); 
    sac->mm = mm; 
    sac->cs = cs_init();

    for (int i = 0; i < SET_ASSOCIATIVE_NUM_SETS; i++) {
        for (int j = 0; j < SET_ASSOCIATIVE_NUM_WAYS; j++) {
            sac->way_places[i][j] = SET_ASSOCIATIVE_NUM_WAYS; 
            sac->dirty_bits[i][j] = 0; 
        }
        sac->set_sizes[i] = 0; 
    }

    return sac;
}

static uintptr_t block_start(void* addr) {
    return ((uintptr_t) addr - (uintptr_t) addr % MAIN_MEMORY_BLOCK_SIZE); }

static int addr_to_set(void* addr) {
    uintptr_t bs = block_start(addr);
    return (bs / MAIN_MEMORY_BLOCK_SIZE) & ((1 << SET_ASSOCIATIVE_NUM_SETS_LN) - 1);
}

static void mark_as_used(set_associative_cache* sac, int set, int way) {
    for (int i = 0; i < sac->set_sizes[set]; i++) {
        if (i != way && sac->way_places[set][i] < sac->way_places[set][way]) {
            sac->way_places[set][i]++; 
        }
    }
    sac->way_places[set][way] = 1; 
}

static int lru(set_associative_cache* sac, int set) {
    assert(sac->set_sizes[set] == SET_ASSOCIATIVE_NUM_WAYS); 
   
    int way = -1; 

    for (int i = 0; i < SET_ASSOCIATIVE_NUM_WAYS; i++) {
        if (sac->way_places[set][i] == SET_ASSOCIATIVE_NUM_WAYS) {
            way = i; 
            break; 
        }
    }
    assert(way >= 0); 
    return way; 
}

static int load_sac_cache(set_associative_cache* sac, void* addr, 
                int* set, int* way) {
    int addr_set = addr_to_set(addr); 
    *set = addr_set; 
    void* block_start = (void*) (((uintptr_t) addr) - 
                            ((uintptr_t) addr) % MAIN_MEMORY_BLOCK_SIZE);


    for (int i = 0; i < sac->set_sizes[addr_set]; i++) {
        if (sac->sets[addr_set][i]->start_addr == block_start) {
            *way = i; 
            return 0; 
        }
    }

    if (sac->set_sizes[addr_set] < SET_ASSOCIATIVE_NUM_WAYS) {
        sac->sets[addr_set][sac->set_sizes[addr_set]] = 
                mm_read(sac->mm, block_start);
        sac->dirty_bits[addr_set][sac->set_sizes[addr_set]] = 0;
        *way = sac->set_sizes[addr_set]; 
        sac->set_sizes[addr_set]++; 
    } else {
        int evicted = lru(sac, addr_set); 
        if (sac->dirty_bits[addr_set][evicted]) {
            mm_write(sac->mm, sac->sets[addr_set][evicted]->start_addr,
                sac->sets[addr_set][evicted]);
        }
        mb_free(sac->sets[addr_set][evicted]);
        sac->sets[addr_set][evicted] = mm_read(sac->mm, block_start); 
        sac->dirty_bits[addr_set][evicted] = 0; 
        *way = evicted; 
    }
    return 1; 
}

void sac_store_word(set_associative_cache* sac, void* addr, unsigned int val) {
    sac->cs.w_queries++; 
    int set, way; 
    sac->cs.w_misses += load_sac_cache(sac, addr, &set, &way); 
    sac->dirty_bits[set][way] = 1; 
    mark_as_used(sac, set, way);
    uintptr_t block_start = (uintptr_t) addr - 
                (((uintptr_t) addr) % MAIN_MEMORY_BLOCK_SIZE);
    size_t offset = (uintptr_t) addr - block_start; 
    *((unsigned int*) (sac->sets[set][way]->data + offset)) = val; 

}


unsigned int sac_load_word(set_associative_cache* sac, void* addr) {
    sac->cs.r_queries++; 
    int set, way; 
    sac->cs.r_misses += load_sac_cache(sac, addr, &set, &way); 
    mark_as_used(sac, set, way); 
    uintptr_t block_start = (uintptr_t) addr - 
                (((uintptr_t) addr) % MAIN_MEMORY_BLOCK_SIZE);
    size_t offset = (uintptr_t) addr - block_start; 
    return *((unsigned int*) (sac->sets[set][way]->data + offset)); 
}

void sac_free(set_associative_cache* sac) {
    // TODO
}
