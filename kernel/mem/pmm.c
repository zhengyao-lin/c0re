#include "pub/com.h"
#include "pub/x86.h"
#include "pub/string.h"
#include "pub/error.h"

#include "lib/sync.h"
#include "lib/debug.h"

#include "mem/mmu.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/swap.h"
#include "mem/ffit.h"

/* *
 * Task State Segment:
 *
 * The TSS may reside anywhere in memory. A special segment register called
 * the Task Register (TR) holds a segment selector that points a valid TSS
 * segment descriptor which resides in the GDT. Therefore, to use a TSS
 * the following must be done in function gdt_init:
 *   - create a TSS descriptor entry in GDT
 *   - add enough information to the TSS in memory as needed
 *   - load the TR register with a segment selector for that segment
 *
 * There are several fileds in TSS for specifying the new stack pointer when a
 * privilege level change happens. But only the fields SS0 and ESP0 are useful
 * in our os kernel.
 *
 * The field SS0 contains the stack segment selector for CPL = 0, and the ESP0
 * contains the new ESP value for CPL = 0. When an interrupt happens in protected
 * mode, the x86 CPU will look in the TSS for SS0 and ESP0 and load their value
 * into SS and ESP respectively.
 * */
static taskstate_t ts = {0};

page_t *c0re_pages;
size_t c0re_npage;

const page_allocator_t *page_alloc;

/* *
 * Global Descriptor Table:
 *
 * The kernel and user segments are identical (except for the DPL). To load
 * the %ss register, the CPL must equal the DPL. Thus, we must duplicate the
 * segments for the user and the kernel. Defined as follows:
 *   - 0x0 :  unused (always faults -- for trapping NULL far pointers)
 *   - 0x8 :  kernel code segment
 *   - 0x10:  kernel data segment
 *   - 0x18:  user code segment
 *   - 0x20:  user data segment
 *   - 0x28:  defined for tss, initialized in gdt_init
 * */
static segdesc_t gdt[] = {
    GDT_SEG_NULL,
    [GDT_SEGNO_KTEXT] = GDT_SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [GDT_SEGNO_KDATA] = GDT_SEG(STA_W,         0x0, 0xFFFFFFFF, DPL_KERNEL),
    [GDT_SEGNO_UTEXT] = GDT_SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_USER),
    [GDT_SEGNO_UDATA] = GDT_SEG(STA_W,         0x0, 0xFFFFFFFF, DPL_USER),
    [GDT_SEGNO_TSS]   = GDT_SEG_NULL
};

static descloader_t gdt_pd = { sizeof(gdt) - 1, (uint32_t)gdt };

/* *
 * lgdt - load the global descriptor table register and reset the
 * data/code segement registers for kernel.
 * */
C0RE_INLINE void lgdt(descloader_t *pd)
{
    asm volatile ("lgdt (%0)" :: "r" (pd));

    asm volatile ("movw %%ax, %%gs" :: "a" (SEGR_USER_DS));
    asm volatile ("movw %%ax, %%fs" :: "a" (SEGR_USER_DS));
    asm volatile ("movw %%ax, %%es" :: "a" (SEGR_KERNEL_DS));
    asm volatile ("movw %%ax, %%ds" :: "a" (SEGR_KERNEL_DS));
    asm volatile ("movw %%ax, %%ss" :: "a" (SEGR_KERNEL_DS));
    
    // reload cs
    // TODO: can we do it before all these moves?
    asm volatile ("ljmp %0, $1f\n 1:\n" :: "i" (SEGR_KERNEL_CS));
}

// /* temporary kernel stack */
// uint8_t stack0[1024];

void load_esp0(uintptr_t esp0)
{
    ts.ts_esp0 = esp0;
}

/* gdt_init - initialize the default GDT and TSS */
static void gdt_init()
{
    // setup a TSS so that we can get the right stack when we trap from
    // user to the kernel. But not safe here, it's only a temporary value,
    // it will be set to KSTACKTOP in lab2.
    extern char kernel_stacktop[];
    
    load_esp0((uintptr_t)kernel_stacktop);
    ts.ts_ss0 = SEGR_KERNEL_DS;

    // initialize the TSS field of the gdt
    gdt[GDT_SEGNO_TSS] = GDT_SEG_TSS(STS_T32A, (uint32_t)&ts, sizeof(ts), DPL_KERNEL);

    // reload all segment registers
    lgdt(&gdt_pd);

    // load the TSS
    ltr(GD_TSS);
}

static void page_allocator_init()
{
    page_alloc = &page_ffit_allocator;
    trace("memory management: %s", page_alloc->name);
    page_alloc->init();
}

//init_memmap - call pmm->addMem to build Page struct for free memory  
static void addMem(page_t *base, size_t n)
{
    page_alloc->addMem(base, n);
}

page_t *palloc(size_t n)
{
    page_t *ret;
    size_t retry = 0;
    
    while (retry < SWAP_MAX_RETRY_TIME) {
        no_intr_block(ret = page_alloc->alloc(n));
    
        // successful allocation OR
        // too big block OR
        // no swap space
        if (ret || n > 1 || !swap_hasInit()) break;
    
        extern vma_set_t *c0re_check_vma_set;
        trace("swap: out of memory, try to swap out %d pages", n);
        swap_out(c0re_check_vma_set, n, 0);
    
        retry++;
    }
    
    if (retry == SWAP_MAX_RETRY_TIME) {
        trace("swap: max retry time reached. unable to swap out enough pages");
    }
    
    return ret;
}

void pfree(page_t *base)
{
    no_intr_block(page_alloc->free(base));
}

size_t nfpage()
{
    size_t ret;
    no_intr_block(ret = page_alloc->nfree());
    return ret;
}

static void page_init()
{
    e820map_t *memmap = (e820map_t *)(0x8000 + KERNEL_BASE);
    uint64_t maxpa = 0;  // pa = physical memory
    uint64_t begin, end;
    int i;

    trace("e820map:");
    
    for (i = 0; i < memmap->nmap; i++) {
        begin = memmap->map[i].addr,
        end = begin + memmap->map[i].size;
        
        trace(DBG_TAB "memory: %08llx, [%08llx, %08llx], type = %d",
              memmap->map[i].size, begin, end - 1, memmap->map[i].type);
              
        if (memmap->map[i].type == E820_ARM) {
            if (maxpa < end && begin < KERNEL_MEMSIZE) {
                maxpa = end;
            }
        }
    }
    
    if (maxpa > KERNEL_MEMSIZE) {
        maxpa = KERNEL_MEMSIZE;
    }

    extern char bss_end[];

    c0re_pages = (page_t *)ROUNDUP((void *)bss_end, PAGE_SIZE);
    c0re_npage = maxpa / PAGE_SIZE;

    for (i = 0; i < c0re_npage; i++) {
        page_setReserved(c0re_pages + i);
    }

    uintptr_t freemem = PADDR((uintptr_t)c0re_pages + sizeof(page_t) * c0re_npage);

    for (i = 0; i < memmap->nmap; i++) {
        uint64_t begin = memmap->map[i].addr, end = begin + memmap->map[i].size;
        
        if (memmap->map[i].type == E820_ARM) {
            if (begin < freemem) {
                begin = freemem;
            }
            
            if (end > KERNEL_MEMSIZE) {
                end = KERNEL_MEMSIZE;
            }
            
            if (begin < end) {
                begin = ROUNDUP(begin, PAGE_SIZE);
                end = ROUNDDOWN(end, PAGE_SIZE);
                if (begin < end) {
                    addMem(pa2page(begin), (end - begin) / PAGE_SIZE);
                }
            }
        }
    }
}

// get_pte - get pte and return the kernel virtual address of this pte for la
//        - if the PT contians this pte didn't exist, alloc a page for PT
// parameter:
//  pgdir:  the kernel virtual base address of PDT
//  la:     the linear address need to map
//  create: a logical value to decide if alloc a page for PT
// return vaule: the kernel virtual address of this pte
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    // this function get the corresponsing page table entry by the linear address
    // if the entry does not exist, it allocate one
    
    pde_t *pdep = &pgdir[PD_INDEX(la)];
    
    if (!(*pdep & PTE_FLAG_P)) {
        // not present -> alloc page
        page_t *page;
        
        if (!create || (page = palloc(1)) == NULL) {
            return NULL;
        }
        
        page_clearRef(page);
        page_incRef(page);
        
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PAGE_SIZE);
        
        *pdep = pa | PTE_FLAG_U | PTE_FLAG_W | PTE_FLAG_P;
    }
    
    // 1. get the page table(*pdep)
    // 2. get the page table address(remove the low 12 bits *pdep)
    // 3. cast to kernel address(KADDR(PTE_ADDR(*pdep)))
    // 4. get the corresponding table entry address

    return &((pte_t *)KADDR(PTE_ADDR(*pdep)))[PT_INDEX(la)];
}

// map_segment - setup & enable the paging mechanism
// parameters
//  la:   linear address of this memory need to map (after x86 segment map)
//  pa:   physical address of this memory
//  size: memory size
//  perm: permission of this memory  

// this functin basically maps the range [la, la + size)
// to a physical address range [pa, pa + size)
// "segment" has no special meaning here
static void
map_segment(pde_t *pgdir, uintptr_t la, uintptr_t pa,
            size_t size, uint32_t perm)
{
    // same page offset
    assert(PAGE_OFS(la) == PAGE_OFS(pa));
    
    // align adresses to pages
    size_t n = ROUNDUP(size + PAGE_OFS(la), PAGE_SIZE) / PAGE_SIZE;
    la = ROUNDDOWN(la, PAGE_SIZE);
    pa = ROUNDDOWN(pa, PAGE_SIZE);
    
    for (; n > 0; n--, la += PAGE_SIZE, pa += PAGE_SIZE) {
        pte_t *pte = get_pte(pgdir, la, true);
        // the corresponding page table entry(which stores a physcial address
        // that the linear address maps to)
        assert(pte != NULL);

        *pte = pa | PTE_FLAG_P | perm;
    }
}

static void page_enable(uintptr_t pgdir_pa)
{
    lcr3(pgdir_pa);

    // turn on paging
    uint32_t cr0 = rcr0();
    
    cr0 |= CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP;
    cr0 &= ~(CR0_TS | CR0_EM);
    
    lcr0(cr0);
}

// virtual address of boot-time page directory
pde_t *c0re_pgdir;
// physical address of boot-time page directory(stored in cr3)
uintptr_t c0re_pgdir_pa;

//get_pgtable_items - In [left, right] range of PDT or PT, find a continuous linear addr space
//                  - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                  - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PAGE_SIZE=4K, if PT
// paramemters:
//  left:        the low side of table's range
//  right:       the high side of table's range
//  table:       the beginning addr of table
//  next_left:   the pointer of the high side of table's next range
//  next_right:  the pointer of the low side of table's next range
// return value: 0 - not a invalid item range, perm - a valid item range with perm permission
static int get_pgtable_items(size_t left, size_t right, uintptr_t *table,
                             size_t *next_left, size_t *next_right)
{
    if (left >= right) {
        return 0;
    }
    
    while (left < right && !(table[left] & PTE_FLAG_P))
        left++;
    
    if (left < right) {
        if (next_left) {
            *next_left = left;
        }

        int perm = table[left++] & PTE_FLAG_USER;

        while (left < right && (table[left] & PTE_FLAG_USER) == perm) {
            left++;
        }
        
        if (next_right != NULL) {
            *next_right = left;
        }
        
        return perm;
    }
    
    return 0;
}

static const char *perm2str(int perm)
{
    static char str[4];

    str[0] = (perm & PTE_FLAG_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_FLAG_W) ? 'w' : '-';
    str[3] = '\0';

    return str;
}

// print_pgdir - print page directory and table
void print_pgdir()
{    
    size_t left, right = 0, perm;
    
    pte_t *vpt = (pte_t *)KERNEL_VPT;
    pde_t *vpd = (pde_t *)PAGE_ADDR(PD_INDEX(KERNEL_VPT), PD_INDEX(KERNEL_VPT), 0);

    // kprintf("%p %p\n", vpt, vpd);

    kprintf("-------------------- BEGIN --------------------\n");
    
    while (1) {
        perm = get_pgtable_items(right, PD_NENTRY, vpd, &left, &right);
        if (!perm) break;
        
        kprintf("PDE(%03x) %08x-%08x %08x %s\n",
                right - left,             // page table count
                left * PT_SIZE,           // begin addr(virtual)
                right * PT_SIZE,          // end addr
                (right - left) * PT_SIZE, // size
                perm2str(perm));
                
        size_t l, r = left * PT_NENTRY;
        
        while (1) {
            perm = get_pgtable_items(r, right * PT_NENTRY, vpt, &l, &r);
            if (!perm) break;
            
            kprintf(DBG_TAB "PTE(%05x) %08x-%08x %08x %s\n",
                    r - l,               // page count
                    l * PAGE_SIZE,       // same as above
                    r * PAGE_SIZE,
                    (r - l) * PAGE_SIZE,
                    perm2str(perm));
        }
    }
    
    kprintf("--------------------- END ---------------------\n");
}

static void check_palloc();
static void check_pgdir();
static void check_c0re_pgdir();

/* pmm_init - initialize the physical memory management */
void pmm_init()
{
    // gdt_init();
    page_allocator_init();
    page_init();
    
    check_palloc();
    
    c0re_pgdir = page2kva(palloc_s(1));
    c0re_pgdir_pa = PADDR(c0re_pgdir);
    memset(c0re_pgdir, 0, PAGE_SIZE);
    
    check_pgdir();
    
    // TODO: check alignment??
    assert(KERNEL_BASE % PT_SIZE == 0 && KERNEL_TOP % PT_SIZE == 0);

    // recursively insert c0re_pgdir in itself
    // to form a virtual page table at virtual address VPT
    // NOTE: map KERNEL_VPT to the page directory itself
    c0re_pgdir[PD_INDEX(KERNEL_VPT)] = c0re_pgdir_pa | PTE_FLAG_P | PTE_FLAG_W;
    
    // map all physical memory to linear memory with base linear addr KERNEL_BASE
    // linear_addr KERNEL_BASE ~ KERNEL_BASE + KERNEL_MEMSIZE = phy_addr 0 ~ KERNEL_MEMSIZE
    // but shouldn't use this map until enable_paging() & gdt_init() finished.
    map_segment(c0re_pgdir, KERNEL_BASE, 0, KERNEL_MEMSIZE, PTE_FLAG_W);
    
    // pd0 -> pd[KERNEL_BASE >> 22]
    // temp setting to keep the kernel working
    c0re_pgdir[0] = c0re_pgdir[PD_INDEX(KERNEL_BASE)];
    
    // NOTE: at this point, segmentation system is still working,
    // so linear address = virtual address - KERNEL_BASE
    // but if paging is enabled, physical address = linear address - KERNEL_BASE
    // so there will be two KERNEL_BASE's subtracted from a virtual address
    // which will crash the kernel
    // -- so we set page table 0 equal to page table that KERNEL_BASE uses(i.e. KERNEL_BASE >> 22)
    // and the extra KERNEL_BASE will not be subtracted from the linear addres
    
    // enable paging
    page_enable(c0re_pgdir_pa);

    gdt_init();

    // NOTE: segmentation system is disabled(no real translation between va and la)
    // restore the page directory
    c0re_pgdir[0] = 0;

    check_c0re_pgdir();
    print_pgdir();
}

void *kmalloc(size_t n)
{
    assert(n > 0 && n < 1024 * 1024);
    int npage = (n + PAGE_SIZE - 1) / PAGE_SIZE;
    return page2kva(palloc_s(npage));
}

void kfree(void *ptr, size_t n)
{
    // assert(n > 0 && n < 1024 * 1024);
    assert(ptr);
    pfree(kva2page(ptr));
}

// get_page - get related Page struct for linear address la using PDT pgdir
page_t *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_result)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    
    if (ptep_result) {
        *ptep_result = ptep;
    }
    
    if (ptep != NULL && *ptep & PTE_FLAG_P) {
        return pte2page(*ptep);
    }
    
    return NULL;
}

//page_remove_pte - free an page_t which is related to by linear address la
//                - and clean(invalidate) pte which is related linear address la
//note: PT is changed, so the TLB need to be invalidate 
C0RE_INLINE
void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)
{
    if (*ptep & PTE_FLAG_P) {
        page_t *page = pte2page(*ptep);
        
        if (page_decRef(page) == 0) {
            pfree(page);
        }
        
        *ptep = 0;
        tlb_invalidate(pgdir, la);
    }
}

//page_remove - free an Page which is related to by linear address la and has an validated pte
void page_remove(pde_t *pgdir, uintptr_t la)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    
    if (ptep) {
        page_remove_pte(pgdir, la, ptep);
    }
}

// page_insert - build the map of phy addr of an Page with the linear addr la
// paramemters:
//  pgdir: the kernel virtual base address of PDT
//  page:  the Page which need to map
//  la:    the linear address need to map
//  perm:  the permission of this Page which is setted in related pte
// return value: always 0
// note: PT is changed, so the TLB need to be invalidate 
int page_insert(pde_t *pgdir, page_t *page, uintptr_t la, uint32_t perm)
{
    pte_t *ptep = get_pte(pgdir, la, 1);
    
    if (!ptep) {
        // no memory
        return -E_NO_MEM;
    }
    
    page_incRef(page);
    
    if (*ptep & PTE_FLAG_P) {
        page_t *p = pte2page(*ptep);
        
        if (p == page) {
            page_decRef(page);
        } else {
            page_remove_pte(pgdir, la, ptep);
        }
    }
    
    *ptep = page2pa(page) | PTE_FLAG_P | perm;
    tlb_invalidate(pgdir, la);
    
    return 0;
}

// pgdir_alloc_page - call alloc_page & page_insert functions to 
//                  - allocate a page size memory & setup an addr map
//                  - pa<->la with linear address la and the PDT pgdir
page_t *pgdir_palloc(pde_t *pgdir, uintptr_t la, uint32_t perm)
{
    extern vma_set_t *c0re_check_vma_set;
    page_t *page = palloc(1);
    
    if (page) {
        if (page_insert(pgdir, page, la, perm)) {
            // error
            pfree(page);
            return NULL;
        }
        
        if (swap_hasInit()) {
            // used for check
            if (c0re_check_vma_set) {
                swap_mapSwappable(c0re_check_vma_set, la, page, 0);
                
                page->pra_vaddr = la;
                
                assert(page_getRef(page) == 1);
                //cprintf("get No. %d  page: pra_vaddr %x, pra_link.prev %x, pra_link_next %x in pgdir_alloc_page\n", (page-pages), page->pra_vaddr,page->pra_page_link.prev, page->pra_page_link.next);
            }
        }
    }

    return page;
}

// invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor
// TODO: wtf is this???
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    if (rcr3() == PADDR(pgdir)) {
        invlpg((void *)la);
    }
}

static void check_palloc()
{
    page_alloc->check();
    trace("check success: page alloc");
}

static void check_pgdir()
{
    assert(c0re_npage <= KERNEL_MEMSIZE / PAGE_SIZE);
    
    assert(c0re_pgdir != NULL && (uint32_t)PAGE_OFS(c0re_pgdir) == 0);
    assert(get_page(c0re_pgdir, 0x0, NULL) == NULL);

    page_t *p1, *p2;
    p1 = palloc(1);
    assert(page_insert(c0re_pgdir, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(c0re_pgdir, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_getRef(p1) == 1);

    ptep = &((pte_t *)KADDR(PDE_ADDR(c0re_pgdir[0])))[1];
    assert(get_pte(c0re_pgdir, PAGE_SIZE, 0) == ptep);

    p2 = palloc(1);
    assert(page_insert(c0re_pgdir, p2, PAGE_SIZE, PTE_FLAG_U | PTE_FLAG_W) == 0);
    assert((ptep = get_pte(c0re_pgdir, PAGE_SIZE, 0)) != NULL);
    assert(*ptep & PTE_FLAG_U);
    assert(*ptep & PTE_FLAG_W);
    assert(c0re_pgdir[0] & PTE_FLAG_U);
    assert(page_getRef(p2) == 1);

    assert(page_insert(c0re_pgdir, p1, PAGE_SIZE, 0) == 0);
    assert(page_getRef(p1) == 2);
    assert(page_getRef(p2) == 0);
    assert((ptep = get_pte(c0re_pgdir, PAGE_SIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_FLAG_U) == 0);

    page_remove(c0re_pgdir, 0x0);
    assert(page_getRef(p1) == 1);
    assert(page_getRef(p2) == 0);

    page_remove(c0re_pgdir, PAGE_SIZE);
    assert(page_getRef(p1) == 0);
    assert(page_getRef(p2) == 0);

    assert(page_getRef(pde2page(c0re_pgdir[0])) == 1);
    pfree(pde2page(c0re_pgdir[0]));
    c0re_pgdir[0] = 0;

    trace("check success: pgdir");
}

static void check_c0re_pgdir()
{
    pte_t *ptep;
    
    int i;
    
    for (i = 0; i < c0re_npage; i += PAGE_SIZE) {
        assert((ptep = get_pte(c0re_pgdir, (uintptr_t)KADDR(i), 0)) != NULL);
        assert(PTE_ADDR(*ptep) == i);
    }

    assert(PDE_ADDR(c0re_pgdir[PD_INDEX(KERNEL_VPT)]) == PADDR(c0re_pgdir));

    assert(c0re_pgdir[0] == 0);

    page_t *p;
    p = palloc(1);
    assert(page_insert(c0re_pgdir, p, 0x100, PTE_FLAG_W) == 0);
    assert(page_getRef(p) == 1);
    assert(page_insert(c0re_pgdir, p, 0x100 + PAGE_SIZE, PTE_FLAG_W) == 0);
    assert(page_getRef(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PAGE_SIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    pfree(p);
    pfree(pde2page(c0re_pgdir[0]));
    c0re_pgdir[0] = 0;

    trace("check success: c0re_pgdir");
}
