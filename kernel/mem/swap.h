#ifndef _KERNEL_MEM_SWAP_H_
#define _KERNEL_MEM_SWAP_H_

#include "pub/com.h"
#include "mem/mmu.h"
#include "mem/pmm.h"
#include "mem/vmm.h"

#define SWAP_MAX_RETRY_TIME 16

typedef pte_t swap_entry_t;

typedef struct {
    const char *name;
    
    int (*init)();
    int (*initVMASet)(vma_set_t *set);
    
    int (*tick)(vma_set_t *set);
    int (*mapSwappable)(vma_set_t *set, uintptr_t addr, page_t *page, int swap_in);
    int (*setUnswappable)(vma_set_t *set, uintptr_t addr);
    
    int (*swapOut)(vma_set_t *set, page_t **result, int in_tick);
    
    int (*check)();
} swap_manager_t;

#define SWAP_MAX_OFFSET_LIMIT (1 << 24)

size_t swap_getOffset(swap_entry_t entry);

C0RE_INLINE
bool swap_hasInit()
{
    extern bool swap_init_ok;
    return swap_init_ok;
}

C0RE_INLINE
void swap_disable()
{
    extern bool swap_init_ok;
    swap_init_ok = false;
}

C0RE_INLINE
void swap_enable()
{
    extern bool swap_init_ok;
    swap_init_ok = true;
}

/* some swap interfaces(a redirect from swap manager) */

int swap_init();
int swap_initVMASet(vma_set_t *set);

int swap_tick(vma_set_t *set);
int swap_mapSwappable(vma_set_t *set, uintptr_t addr, page_t *page, int swap_in);
int swap_setUnswappable(vma_set_t *set, uintptr_t addr);

int swap_out(vma_set_t *set, int n, int in_tick);
int swap_in(vma_set_t *set, uintptr_t addr, page_t **result);

#endif
