#ifndef _KERNEL_MEM_VMM_H_
#define _KERNEL_MEM_VMM_H_

/* virtual memory management */

#include "pub/com.h"
#include "pub/dllist.h"

#include "mem/mmu.h"
#include "lib/sync.h"

struct vma_set_t_tag;

typedef struct {
    struct vma_set_t_tag *set;
    
    uintptr_t start;
    uintptr_t end;
    
    uint32_t flags;
    dllist_t link;
} vma_t;

typedef struct vma_set_t_tag {
    dllist_t mset;
    vma_t *mcache;
    size_t mcount;
    
    pde_t *pgdir;
    
    void *swap_data;
} vma_set_t;

#define dll2vma(dll, member) \
    to_struct((dll), vma_t, member)
    
#define VMA_FLAG_READ           0x00000001
#define VMA_FLAG_WRITE          0x00000002
#define VMA_FLAG_EXEC           0x00000004

vma_t *vma_new(uintptr_t start, uintptr_t end, uint32_t flags);

vma_set_t *vma_set_new();
void vma_set_free(vma_set_t *set);
void vma_set_insert(vma_set_t *set, vma_t *vma);
vma_t *vma_set_find(vma_set_t *set, uintptr_t addr);

void vmm_init();
int vmm_doPageFault(vma_set_t *set, uint32_t error, uintptr_t addr);
size_t vmm_getPageFaultCount();

#endif
