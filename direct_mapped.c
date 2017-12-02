#include <stdint.h> 
#include "memory_block.h"
#include "direct_mapped.h"

direct_mapped_cache* dmc_init(main_memory* mm)
{ 
    // allocate the cache 
    direct_mapped_cache* dmc = malloc(sizeof(direct_mapped_cache));

    // initialize cache stats 
    dmc->cs = cs_init();

    // initialize every block to null 
    for (int i = 0; i < DIRECT_MAPPED_NUM_SETS; i++) {
        dmc->dirty_bits[i] = 0; 
        dmc->mem_blocks[i] = NULL; 
    }
    
    // set the main memory 
    dmc->mm = mm; 
    return dmc;
}

static inline uintptr_t addr_block_start(void* addr) {
    uintptr_t cast_addr = (uintptr_t) addr; 
    return (cast_addr - (cast_addr % MAIN_MEMORY_BLOCK_SIZE));
}

static inline int addr_to_set(void* addr)
{
    uintptr_t block_start = addr_block_start(addr);
    return (((uintptr_t) block_start / MAIN_MEMORY_BLOCK_SIZE) & ((1 << DIRECT_MAPPED_NUM_SETS_LN) - 1));
}

// load_dmc_cache
//      Guarantees that the address `addr` is in cache 
//      `dmc`. Returns 1 in the case of a miss, 0 in a 
//       hit.
int load_dmc_cache(direct_mapped_cache* dmc, void* addr) {
    uintptr_t start_addr = addr_block_start(addr); 
    int set = addr_to_set(addr); 
    assert(set < DIRECT_MAPPED_NUM_SETS);


    if (dmc->mem_blocks[set] == NULL) {
        dmc->mem_blocks[set] = mm_read(dmc->mm, (void*) start_addr);
        dmc->dirty_bits[set] = 0; 
        return 1;
    } else if ((uintptr_t) dmc->mem_blocks[set]->start_addr != start_addr) {
        if (dmc->dirty_bits[set]) {
            mm_write(dmc->mm, dmc->mem_blocks[set]->start_addr, 
                dmc->mem_blocks[set]);      
        }
        mb_free(dmc->mem_blocks[set]); 
        dmc->dirty_bits[set] = 0;
        dmc->mem_blocks[set] = mm_read(dmc->mm, (void*) start_addr);
        return 1;
    }

    return 0; 
}

void dmc_store_word(direct_mapped_cache* dmc, void* addr, unsigned int val)
{
    dmc->cs.w_queries++; 
    int set = addr_to_set(addr);
    size_t start_addr = addr_block_start(addr);
    assert(set < DIRECT_MAPPED_NUM_SETS); 
    dmc->cs.w_misses += load_dmc_cache(dmc, addr);
    size_t offset = (uintptr_t) addr - start_addr; 

    dmc->dirty_bits[set] = 1; 
    *((unsigned int*) (dmc->mem_blocks[set]->data + offset)) = val;
}

unsigned int dmc_load_word(direct_mapped_cache* dmc, void* addr)
{  
    dmc->cs.r_queries++; 
    int set = addr_to_set(addr);
    assert(set < DIRECT_MAPPED_NUM_SETS); 

    size_t start_addr = addr_block_start(addr);
    dmc->cs.r_misses += load_dmc_cache(dmc, addr);
    size_t offset = (uintptr_t) addr - start_addr; 
    return *((unsigned int*) (dmc->mem_blocks[set]->data + offset));
}

void dmc_free(direct_mapped_cache* dmc)
{
    for (int i = 0; i < DIRECT_MAPPED_NUM_SETS; i++) {
        if (dmc->mem_blocks[i] != NULL) {
            mb_free(dmc->mem_blocks[i]);
        }
    }

    free(dmc);
}
