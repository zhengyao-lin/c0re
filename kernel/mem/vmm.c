#include "pub/com.h"
#include "pub/dllist.h"
#include "pub/error.h"

#include "mem/swap.h"
#include "mem/vmm.h"
#include "mem/pmm.h"

vma_t *vma_new(uintptr_t start, uintptr_t end, uint32_t flags)
{
    vma_t *vma = kmalloc(sizeof(*vma));

    if (vma) {
        vma->start = start;
        vma->end = end;
        vma->flags = flags;
    }

    return vma;
}

C0RE_INLINE
bool vma_has(vma_t *vma, uintptr_t addr)
{
    return addr >= vma->start && addr < vma->end;
}

vma_set_t *vma_set_new()
{
    vma_set_t *set = kmalloc(sizeof(*set));

    if (set) {
        dllist_init(&(set->mset));

        set->mcache = NULL;
        set->mcount = 0;

        set->pgdir = NULL;

        if (swap_hasInit()) swap_initVMASet(set);
        else set->swap_data = NULL;
    }

    return set;
}

void vma_set_free(vma_set_t *set)
{
    dllist_t *list = &(set->mset), *dll;
    
    while ((dll = dllist_next(list)) != list) {
        dllist_del(dll);
        kfree(dll2vma(dll, link), sizeof(vma_t));  // kfree vma
    }
    
    kfree(set, sizeof(*set)); // kfree mm
    // mm = NULL; // WTF is this???
}

C0RE_INLINE
bool is_vma_overlap(vma_t *prev, vma_t *next)
{
    return !(prev->start < prev->end &&
             prev->end <= next->start &&
             next->start < next->end);
}

void vma_set_insert(vma_set_t *set, vma_t *vma)
{
    assert(vma->start < vma->end);
    
    dllist_t *list = &(set->mset);
    dllist_t *prev = list, *next;
    
    dllist_t *cur = list;
    
    while ((cur = dllist_next(cur)) != list) {
        vma_t *prevvma = dll2vma(cur, link);
        
        if (prevvma->start > vma->start) {
            break; // find the right inert position
        }
        
        prev = cur;
    }

    next = dllist_next(prev);

    /* check overlap(of vma and prev) */
    if (prev != list) {
        assert(!is_vma_overlap(dll2vma(prev, link), vma));
    }
    
    /* check overlap(of vma and next) */
    if (next != list) {
        assert(!is_vma_overlap(vma, dll2vma(next, link)));
    }

    vma->set = set;
    dllist_add_after(prev, &(vma->link));

    set->mcount++;
}

// find which vma the addr is at
vma_t *vma_set_find(vma_set_t *set, uintptr_t addr)
{
    vma_t *vma = NULL;
    
    if (set) {
        vma = set->mcache;
        
        if (!vma || !vma_has(vma, addr)) {
            // addr not in the cache vma -- keep finding
            bool found = false;
            
            dllist_t *list = &(set->mset), *cur = list;
            
            while ((cur = dllist_next(cur)) != list) {
                vma = dll2vma(cur, link);
                if (vma_has(vma, addr)) {
                    found = true;
                    break;
                }
            }
            
            if (!found) vma = NULL;
            else set->mcache = vma; // set cache
        }
    }
    
    return vma;
}

static void check_vmm();
static size_t page_fault_count = 0;

void vmm_init()
{
    check_vmm();
}

size_t vmm_getPageFaultCount()
{
    return page_fault_count;
}

int vmm_doPageFault(vma_set_t *set, uint32_t error, uintptr_t addr)
{
    int ret = -E_INVAL;
    
    // try to find a vma which include addr
    vma_t *vma = vma_set_find(set, addr);

    page_fault_count++;
    
    // the addr is in the range of a set's vma?
    if (!vma || /* TODO: what's this for??? */vma->start > addr) {
        trace("vmm_doPageFault: invalid addr %p which cannot be found in vma\n", (void *)addr);
        goto failed;
    }
    
    //check the error
    // error code stored in cr2 has 3 useful bits
    // bit 1 == 1 means that the physical page does not exist
    // bit 2 == 1 means write error(e.g. writing non-writtable area)
    // bit 3 == 1 means privillege error
    // TODO: see https://chyyuu.gitbooks.io/ucore_os_docs/content/lab3/lab3_4_page_fault_handler.html
    switch (error & 3) {
        default: /* error code flag : default is 3 ( W/R=1, P=1): write, present */
                
        case 2: /* error code flag : (W/R=1, P=0): write, not present */
            if (!(vma->flags & VMA_FLAG_WRITE)) {
                trace("vmm_doPageFault: write non-writtable vma(error code flag = write AND not present)");
                goto failed;
            }

            break;
        
        // why??
        case 1: /* error code flag : (W/R=0, P=1): read, present */
            trace("vmm_doPageFault: illegal error code flag 'read AND present'");
            goto failed;
            
        case 0: /* error code flag : (W/R=0, P=0): read, not present */
            if (!(vma->flags & (VMA_FLAG_READ | VMA_FLAG_EXEC))) {
                trace("vmm_doPageFault: read non-readable and non-executable vma(error code flag = read AND not present)");
                goto failed;
            }
    }
    /* IF (write an existed addr) OR
     *    (write an non_existed addr && addr is writable) OR
     *    (read  an non_existed addr && addr is readable)
     * THEN
     *    continue process
     */
    
    uint32_t perm = PTE_FLAG_U;
    
    if (vma->flags & VMA_FLAG_WRITE) {
        perm |= PTE_FLAG_W;
    }
    
    addr = ROUNDDOWN(addr, PAGE_SIZE);

    ret = -E_NO_MEM;

    pte_t *ptep = get_pte(set->pgdir, addr, true);
    
    // try to find a pte, if pte's PT(Page Table) doesn't existed, then create a PT.
    // (notice the 3th parameter '1')
    if (ptep == NULL) {
        trace("vmm_doPageFault: cannot find page table entry");
        goto failed;
    }
    
    if (*ptep == 0) { // if the phy addr doesn't exist, then alloc a page & map the phy addr with logical addr
        if (!pgdir_palloc(set->pgdir, addr, perm)) {
            trace("vmm_doPageFault: pgdir_alloc_page failed\n");
            goto failed;
        }
    } else { // if this pte is a swap entry, then load data from disk to a page with phy addr
             // and call page_insert to map the phy addr with logical addr
        // NOTE: if a PTE is not present but non-zero, it's a swap entry
        // then you cast it to swap_entry_t
        if(swap_hasInit()) {
            page_t *page = NULL;
            ret = swap_in(set, addr, &page);
            
            if (ret) {
                trace("vmm_doPageFault: swap_in failed\n");
                goto failed;
            }
            
            page_insert(set->pgdir, page, addr, perm);
            swap_mapSwappable(set, addr, page, 1);
            
            page->pra_vaddr = addr;
        } else {
            trace("vmm_doPageFault: swap not available(ptep = %x)", *ptep);
            goto failed;
        }
   }
   
   ret = 0;
   
failed:
    return ret;
}

static void check_vma_set()
{
    size_t nfree = nfpage();

    // panic("no!");

    vma_set_t *set = vma_set_new();
    assert(set);

    int step1 = 10, step2 = step1 * 10;

    int i;
    
    for (i = step1; i >= 1; i--) {
        vma_t *vma = vma_new(i * 5, i * 5 + 2, 0);
        assert(vma);
        vma_set_insert(set, vma);
    }

    for (i = step1 + 1; i <= step2; i++) {
        vma_t *vma = vma_new(i * 5, i * 5 + 2, 0);
        assert(vma);
        vma_set_insert(set, vma);
    }
    
    // trace("vma count: %d", set->mcount);

    dllist_t *dll = dllist_next(&(set->mset));

    for (i = 1; i <= step2; i++) {
        assert(dll != &(set->mset));
        
        vma_t *mmap = dll2vma(dll, link);
        
        assert(mmap->start == i * 5 && mmap->end == i * 5 + 2);
        
        dll = dllist_next(dll);
    }

    for (i = 5; i <= 5 * step2; i += 5) {
        vma_t *vma1 = vma_set_find(set, i + 0);
        assert(vma1 != NULL);
        
        vma_t *vma2 = vma_set_find(set, i + 1);
        assert(vma2 != NULL);
        
        // trace("%d %p", i, vma_set_find(set, i + 2));
        
        assert(!vma_set_find(set, i + 2));
        assert(!vma_set_find(set, i + 3));
        assert(!vma_set_find(set, i + 4));

        assert(vma1->start == i  && vma1->end == i  + 2);
        assert(vma2->start == i  && vma2->end == i  + 2);
    }

    for (i = 4; i >= 0; i--) {
        vma_t *vma_below_5 = vma_set_find(set, i);
        
        if (vma_below_5) {
           trace("vma_below_5: i %x, start %x, end %x\n", i, vma_below_5->start, vma_below_5->end); 
        }
        
        assert(!vma_below_5);
    }

    vma_set_free(set);

    assert(nfree == nfpage());

    trace("check success: vma set");
}

vma_set_t *c0re_check_vma_set = NULL;

static void check_pgfault()
{
    trace("check begin: pgfault");
    
    size_t nfree = nfpage();

    vma_set_t *set = c0re_check_vma_set = vma_set_new();
    pde_t *pgdir = set->pgdir = c0re_pgdir;
    
    assert(c0re_check_vma_set);
    assert(pgdir[0] == 0);

    vma_t *vma = vma_new(0, PT_SIZE, VMA_FLAG_WRITE);
    assert(vma);

    vma_set_insert(set, vma);

    uintptr_t addr = 0x0;
    assert(vma_set_find(set, addr) == vma);

    int i, sum = 0;
    
    for (i = 0; i < 100; i++) {
        *(char *)(addr + i) = i;
        sum += i;
    }
    
    for (i = 0; i < 100; i++) {
        sum -= *(char *)(addr + i);
    }
    
    assert(sum == 0);

    page_remove(pgdir, ROUNDDOWN(addr, PAGE_SIZE));
    pfree(pde2page(pgdir[0]));
    
    pgdir[0] = 0;

    set->pgdir = NULL;
    vma_set_free(set);
    
    c0re_check_vma_set = NULL;

    assert(nfree == nfpage());

    trace("check success: page fault");
}

// check_vmm - check correctness of vmm
static void check_vmm()
{
    size_t nfree = nfpage();
    
    check_vma_set();
    check_pgfault();

    assert(nfree == nfpage());

    trace("check success: vmm");
}
