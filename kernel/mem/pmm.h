#ifndef _KERNEL_MEM_PMM_H_
#define _KERNEL_MEM_PMM_H_

#include "pub/com.h"

#include "lib/debug.h"

#include "mem/mmu.h"

typedef struct {
    const char *name;
    void (*init)(void); // init

    void (*addMem)(page_t *, size_t); // add new mem block
    
    page_t *(*alloc)(size_t);
    void (*free)(page_t *);
    
    size_t (*nfree)(); // total free page count
} page_allocator_t;

page_t *c0re_pages;
size_t c0re_npage;

/**
 * PADDR - takes a kernel virtual address (an address that points above KERNBASE),
 * where the machine's maximum 256MB of physical memory is mapped and returns the
 * corresponding physical address. it panicks if you pass it a non-kernel virtual address.
 **/
#define PADDR(kva) ({                                              \
        uintptr_t __m_kva = (uintptr_t)(kva);                      \
        if (__m_kva < KERNEL_BASE) {                               \
            panic("PADDR called with invalid kva %08lx", __m_kva); \
        }                                                          \
        __m_kva - KERNEL_BASE;                                     \
    })

/**
 * KADDR - takes a physical address and returns the corresponding kernel virtual
 * address. it panicks if you pass an invalid physical address.
 **/
#define KADDR(pa) ({                                             \
        uintptr_t __m_pa = (pa);                                 \
        size_t __m_ppn = PAGE_NUMBER(__m_pa);                    \
        if (__m_ppn >= c0re_npage) {                             \
            panic("KADDR called with invalid pa %08lx", __m_pa); \
        }                                                        \
        (void *)(__m_pa + KERNEL_BASE);                          \
    })
    
C0RE_INLINE
page_number_t page2ppn(page_t *page)
{
    return page - c0re_pages;
}

// pa for physical address
C0RE_INLINE
uintptr_t page2pa(page_t *page)
{
    return page2ppn(page) << PAGE_SHIFT;
}

C0RE_INLINE
page_t *pa2page(uintptr_t pa)
{
    if (PAGE_NUMBER(pa) >= c0re_npage) {
        panic("pa2page called with invalid physical address");
    }
    
    return &c0re_pages[PAGE_NUMBER(pa)];
}

C0RE_INLINE
void *page2kva(page_t *page)
{
    return KADDR(page2pa(page));
}

C0RE_INLINE
page_t *kva2page(void *kva)
{
    return pa2page(PADDR(kva));
}

C0RE_INLINE
page_t *pte2page(pte_t pte)
{
    if (!(pte & PTE_FLAG_P)) {
        panic("pte2page called with invalid page table entry");
    }
    
    return pa2page(PTE_ADDR(pte));
}

C0RE_INLINE
page_t *pde2page(pde_t pde)
{
    return pa2page(PDE_ADDR(pde));
}

void pmm_init();

#endif
