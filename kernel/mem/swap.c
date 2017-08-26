#include "pub/com.h"

#include "lib/debug.h"
#include "fs/swapfs.h"

#include "mem/swap.h"
#include "mem/smfifo.h"
#include "mem/pmm.h"
#include "mem/vmm.h"

static size_t max_swap_offset;

size_t swap_getOffset(swap_entry_t entry)
{
    size_t offset = entry >> 8;
    
    if (!offset || offset >= max_swap_offset) {
        panic("invalid swap_entry_t = %08x", entry);
    }
    
    return offset;
}


// the valid vaddr for check is between 0~CHECK_VALID_VADDR-1
#define CHECK_VALID_VIR_PAGE_NUM 5
#define BEING_CHECK_VALID_VADDR 0X1000
#define CHECK_VALID_VADDR ((CHECK_VALID_VIR_PAGE_NUM + 1) * 0x1000)

// the max number of valid physical pages for check
#define CHECK_VALID_PHY_PAGE_NUM 4

// the max access seq number
#define MAX_SEQ_NO 10

static swap_manager_t *swap_man = &swap_manager_fifo;

bool swap_init_ok = 0;

unsigned int swap_page[CHECK_VALID_VIR_PAGE_NUM];
unsigned int swap_in_seq_no[MAX_SEQ_NO],
             swap_out_seq_no[MAX_SEQ_NO];

static void check_swap();

int swap_init()
{
    max_swap_offset = swapfs_init();

    if (max_swap_offset == 0) {
        trace("swap diabled");
        swap_disable();
        return 0;
    }

    if (!(1024 <= max_swap_offset && max_swap_offset < SWAP_MAX_OFFSET_LIMIT)) {
        panic("bad max_swap_offset %08x", max_swap_offset);
    }

    // swap_man = &swap_manager_fifo;
    int r = swap_man->init();

    if (r == 0) {
        swap_enable();
        trace("swap: init manager = %s", swap_man->name);
        
        check_swap();
    }

    return r;
}

int swap_initVMASet(vma_set_t *set)
{
    // panic("wwww %p", swap_man);
    return swap_man->initVMASet(set);
}

int swap_tick(vma_set_t *set)
{
    return swap_man->tick(set);
}

int swap_mapSwappable(vma_set_t *set, uintptr_t addr, page_t *page, int swap_in)
{
    return swap_man->mapSwappable(set, addr, page, swap_in);
}

int swap_setUnswappable(vma_set_t *set, uintptr_t addr)
{
    return swap_man->setUnswappable(set, addr);
}

volatile unsigned int swap_out_num = 0;

int swap_out(vma_set_t *set, int n, int in_tick)
{
    int i;

    for (i = 0; i != n; i++) {
        uintptr_t v;

        //struct Page **ptr_page=NULL;
        page_t *page;

        trace("swap: call swap_out_victim, i %d", i);

        int r = swap_man->swapOut(set, &page, in_tick);

        if (r) {
            trace("swap: call swap_out_victim failed, i %d", i);
            break;
        }
        
        assert(!page_isReserved(page));

        trace("swap: choose victim page 0x%08x", page);

        v = page->pra_vaddr;
        
        pte_t *ptep = get_pte(set->pgdir, v, 0);
        assert(*ptep & PTE_FLAG_P); // table present

        // TODO: what does swap entry mean
        if (swapfs_write((page->pra_vaddr / PAGE_SIZE + 1) << 8, page)) {
            trace("swap: failed to save victim");
            swap_man->mapSwappable(set, v, page, 0); // swap back???
            continue;
        } else {
            trace("swap: i %d, store page in vaddr 0x%x to disk swap entry %d",
                  i, v, page->pra_vaddr / PAGE_SIZE + 1);
                  
            *ptep = (page->pra_vaddr / PAGE_SIZE + 1) << 8;
            
            pfree(page);
        }

        tlb_invalidate(set->pgdir, v);
    }

    return i;
}

int swap_in(vma_set_t *set, uintptr_t addr, page_t **presult)
{
     page_t *result = palloc_s(1);

     pte_t *ptep = get_pte(set->pgdir, addr, 0);
     // cprintf("SWAP: load ptep %x swap entry %d to vaddr 0x%08x, page %x, No %d\n", ptep, (*ptep)>>8, addr, result, (result-pages));
    
     int r;
     if ((r = swapfs_read((*ptep), result))) {
        trace("swap: failed to swap in"); // TODO: ???
     }
     
     trace("swap: load disk swap entry %d with swap_page in vadr 0x%x", (*ptep) >> 8, addr);
     *presult = result;
     
     return 0;
}

C0RE_INLINE
void check_content_set()
{
    size_t init = vmm_getPageFaultCount();
    
    *(unsigned char *)0x1000 = 0x0a;
    assert(vmm_getPageFaultCount() - init == 1);
    
    *(unsigned char *)0x1010 = 0x0a;
    assert(vmm_getPageFaultCount() - init == 1);
    
    *(unsigned char *)0x2000 = 0x0b;
    assert(vmm_getPageFaultCount() - init == 2);
    
    *(unsigned char *)0x2010 = 0x0b;
    assert(vmm_getPageFaultCount() - init == 2);
    
    *(unsigned char *)0x3000 = 0x0c;
    assert(vmm_getPageFaultCount() - init == 3);
    
    *(unsigned char *)0x3010 = 0x0c;
    assert(vmm_getPageFaultCount() - init == 3);
    
    *(unsigned char *)0x4000 = 0x0d;
    assert(vmm_getPageFaultCount() - init == 4);
    
    *(unsigned char *)0x4010 = 0x0d;
    assert(vmm_getPageFaultCount() - init == 4);
}

C0RE_INLINE
int check_content_access()
{
    return swap_man->check();
}

static page_t *check_rp[CHECK_VALID_PHY_PAGE_NUM];
static pte_t *check_ptep[CHECK_VALID_PHY_PAGE_NUM];
// static unsigned int check_swap_addr[CHECK_VALID_VIR_PAGE_NUM];

extern free_area_t free_area;

#define _FREED (free_area.freed)
#define _NFREE (free_area.nfree)

static void check_swap()
{
    //backup mem env
    int ret, count = 0, total = 0, i;
    
    page_t *cur;
    
    for (cur = _FREED; cur; cur = cur->next) {
        assert(page_isFree(cur));
        count++;
        total += cur->nfree;
    }
    
    assert(total == nfpage());
    
    trace("check begin: swap, count %d, total %d", count, total);

    // now we set the phy pages env
    extern vma_set_t *c0re_check_vma_set;
    vma_set_t *set = vma_set_new();
    
    assert(set);
    
    c0re_check_vma_set = set;

    pde_t *pgdir = set->pgdir = c0re_pgdir;
    assert(pgdir[0] == 0);

    vma_t*vma = vma_new(BEING_CHECK_VALID_VADDR, CHECK_VALID_VADDR, VMA_FLAG_WRITE | VMA_FLAG_READ);
    assert(vma);

    vma_set_insert(set, vma);

    //setup the temp Page Table vaddr 0~4MB
    kprintf("setting up page table for vaddr 0X1000 ... ");
    
    pte_t *temp_ptep = NULL;
    temp_ptep = get_pte(set->pgdir, BEING_CHECK_VALID_VADDR, 1);
    assert(temp_ptep!= NULL);
    
    trace("finished");

    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        check_rp[i] = palloc(1);
        assert(check_rp[i]);
        assert(!page_isFree(check_rp[i]));
    }
    
    page_t *freed = _FREED;
    unsigned int nfree = _NFREE;
    
    _FREED = NULL;
    _NFREE = 0;
    
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        pfree(check_rp[i]);
    }
    
    assert(_NFREE == CHECK_VALID_PHY_PAGE_NUM);

    trace("setting up init env ... ");
    // setup initial vir_page<->phy_page environment for page relpacement algorithm 

    // size_t init_pgfault_num = vmm_getPageFaultCount();

    check_content_set();
    
    assert(_NFREE == 0);
    
    for(i = 0; i < MAX_SEQ_NO; i++)
        swap_out_seq_no[i] = swap_in_seq_no[i] = -1;

    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        check_ptep[i] = 0;
        check_ptep[i] = get_pte(pgdir, (i + 1) * 0x1000, 0);

        assert(check_ptep[i] != NULL);
        assert(pte2page(*check_ptep[i]) == check_rp[i]);
        assert(*check_ptep[i] & PTE_FLAG_P);
    }
    
    trace("finished");
    
    // now access the virt pages to test  page relpacement algorithm 
    ret = check_content_access();
    assert(ret == 0);

    //restore kernel mem env
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        pfree(check_rp[i]);
    } 

    vma_set_free(set);
     
    _NFREE = nfree;
    _FREED = freed;

    for (cur = _FREED; cur; cur = cur->next) {
        count--;
        total -= cur->nfree;
    }

    trace("check success: swap, count %d, total %d", count, total);
}
