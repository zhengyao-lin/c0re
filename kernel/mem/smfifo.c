#include "pub/com.h"
#include "pub/dllist.h"

#include "lib/debug.h"

#include "mem/smfifo.h"

static dllist_t fifo_head;

static int smfifo_init()
{
    return 0;
}

static int smfifo_initVMASet(vma_set_t *set)
{
    dllist_init(&fifo_head);
    set->swap_data = &fifo_head;
    
    trace("smfifo: init fifo_head %p", (void *)&fifo_head);
    
    return 0;
}

static int smfifo_mapSwappable(vma_set_t *set, uintptr_t addr,
                               page_t *page, int swap_in)
{
    dllist_t *head = (dllist_t *)set->swap_data;
    dllist_t *entry = &(page->pra_link);

    assert(head && entry);
    
    dllist_add(head, entry);
    
    return 0;
}

static int smfifo_setUnswappable(vma_set_t *set, uintptr_t addr)
{
    return 0;
}

static int smfifo_swapOut(vma_set_t *set, page_t **result, int in_tick)
{
    dllist_t *head = (dllist_t *)set->swap_data;

    assert(head);
    assert(in_tick == 0);
    
     /* Select the victim */
     /*LAB3 EXERCISE 2: YOUR CODE*/ 
     //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
     //(2)  set the addr of addr of this page to ptr_page
     
    /* Select the tail */
    dllist_t *dll = head->prev;
    
    assert(head != dll);
    
    page_t *p = dll2page(dll, pra_link);
    dllist_del(dll);
    
    assert(p);
    *result = p;

    return 0;
}

static int smfifo_tick(vma_set_t *set)
{
    return 0;
}

static int smfifo_check()
{
    size_t init = vmm_getPageFaultCount();
    
    *(unsigned char *)0x3000 = 0x0c;
    assert(vmm_getPageFaultCount() - init == 0);

    *(unsigned char *)0x1000 = 0x0a;
    assert(vmm_getPageFaultCount() - init == 0);

    *(unsigned char *)0x4000 = 0x0d;
    assert(vmm_getPageFaultCount() - init == 0);

    *(unsigned char *)0x2000 = 0x0b;
    assert(vmm_getPageFaultCount() - init == 0);

    *(unsigned char *)0x5000 = 0x0e;
    assert(vmm_getPageFaultCount() - init == 1);

    *(unsigned char *)0x2000 = 0x0b;
    assert(vmm_getPageFaultCount() - init == 1);

    *(unsigned char *)0x1000 = 0x0a;
    assert(vmm_getPageFaultCount() - init == 2);

    *(unsigned char *)0x2000 = 0x0b;
    assert(vmm_getPageFaultCount() - init == 3);

    *(unsigned char *)0x3000 = 0x0c;
    assert(vmm_getPageFaultCount() - init == 4);

    *(unsigned char *)0x4000 = 0x0d;
    assert(vmm_getPageFaultCount() - init == 5);

    *(unsigned char *)0x5000 = 0x0e;
    assert(vmm_getPageFaultCount() - init == 6);

    assert(*(unsigned char *)0x1000 == 0x0a);
    
    *(unsigned char *)0x1000 = 0x0a;
    assert(vmm_getPageFaultCount() - init == 7);

    return 0;
}

swap_manager_t swap_manager_fifo = {
     .name            = "fifo swap manager",
     
     .init            = &smfifo_init,
     .initVMASet      = &smfifo_initVMASet,
     
     .tick            = &smfifo_tick,
     .mapSwappable    = &smfifo_mapSwappable,
     .setUnswappable  = &smfifo_setUnswappable,
     .swapOut         = &smfifo_swapOut,
     
     .check           = &smfifo_check
};
