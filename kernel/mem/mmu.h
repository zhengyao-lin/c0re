#ifndef _KERNEL_MEM_MMU_H_
#define _KERNEL_MEM_MMU_H_

/* memory management unit */

/* mem layout */

/* global segment numbers */
#define GDT_SEGNO_KTEXT    1 // K for kernel
#define GDT_SEGNO_KDATA    2
#define GDT_SEGNO_UTEXT    3
#define GDT_SEGNO_UDATA    4
#define GDT_SEGNO_TSS      5

/* global descriptor numbers */
#define GD_KTEXT        ((GDT_SEGNO_KTEXT) << 3)        // kernel text
#define GD_KDATA        ((GDT_SEGNO_KDATA) << 3)        // kernel data
#define GD_UTEXT        ((GDT_SEGNO_UTEXT) << 3)        // user text
#define GD_UDATA        ((GDT_SEGNO_UDATA) << 3)        // user data
#define GD_TSS          ((GDT_SEGNO_TSS) << 3)          // task segment selector

/* priviledge levels */
#define DPL_KERNEL      (0)
#define DPL_USER        (3)

/* cs/ds register value */
#define SEGR_KERNEL_CS    ((GD_KTEXT) | DPL_KERNEL)
#define SEGR_KERNEL_DS    ((GD_KDATA) | DPL_KERNEL)
#define SEGR_USER_CS      ((GD_UTEXT) | DPL_USER)
#define SEGR_USER_DS      ((GD_UDATA) | DPL_USER)

/* Normal segment */
#define GDT_SEGNULL_ASM \
    .short 0, 0;        \
    .byte 0, 0, 0, 0

#define GDT_SEG_ASM(type, base, lim)                                      \
                    /* 12 means dividing by 4096(4k, granularity) */      \
    .short  (((lim) >> 12) & 0xffff),           ((base) & 0xffff);        \
                                              /* 0x90 for P and S bits */ \
    .byte   (((base) >> 16) & 0xff),            (0x90 | (type)),          \
            (0xc0 | (((lim) >> 28) & 0xf)),     (((base) >> 24) & 0xff)
          /* 0xc0 here sets the G(granularity, to 4k) and D(???) bits */

/* eflags register */
#define EFLAG_CF            0x00000001    // carry Flag
#define EFLAG_PF            0x00000004    // parity Flag
#define EFLAG_AF            0x00000010    // auxiliary carry Flag
#define EFLAG_ZF            0x00000040    // zero Flag
#define EFLAG_SF            0x00000080    // sign Flag
#define EFLAG_TF            0x00000100    // trap Flag
#define EFLAG_IF            0x00000200    // interrupt Flag
#define EFLAG_DF            0x00000400    // direction Flag
#define EFLAG_OF            0x00000800    // overflow Flag
#define EFLAG_IOPL_MASK     0x00003000    // I/O Privilege Level bitmask
#define EFLAG_IOPL_0        0x00000000    //   IOPL == 0
#define EFLAG_IOPL_1        0x00001000    //   IOPL == 1
#define EFLAG_IOPL_2        0x00002000    //   IOPL == 2
#define EFLAG_IOPL_3        0x00003000    //   IOPL == 3
#define EFLAG_NT            0x00004000    // nested Task
#define EFLAG_RF            0x00010000    // resume Flag
#define EFLAG_VM            0x00020000    // virtual 8086 mode
#define EFLAG_AC            0x00040000    // alignment Check
#define EFLAG_VIF           0x00080000    // virtual Interrupt Flag
#define EFLAG_VIP           0x00100000    // virtual Interrupt Pending
#define EFLAG_ID            0x00200000    // ID flag

/* application segment type bits */
#define STA_X               0x8            // executable segment
#define STA_E               0x4            // expand down (non-executable segments)
#define STA_C               0x4            // conforming code segment (executable only)
#define STA_W               0x2            // writeable (non-executable segments)
#define STA_R               0x2            // readable (executable segments)
#define STA_A               0x1            // accessed

/* System segment type bits */
#define STS_T16A            0x1            // available 16-bit TSS
#define STS_LDT             0x2            // local descriptor table
#define STS_T16B            0x3            // busy 16-bit TSS
#define STS_CG16            0x4            // 16-bit call gate
#define STS_TG              0x5            // task gate / coum transmitions
#define STS_IG16            0x6            // 16-bit interrupt gate
#define STS_TG16            0x7            // 16-bit trap gate
#define STS_T32A            0x9            // available 32-bit TSS
#define STS_T32B            0xB            // busy 32-bit TSS
#define STS_CG32            0xC            // 32-bit call gate
#define STS_IG32            0xE            // 32-bit interrupt gate
#define STS_TG32            0xF            // 32-bit trap gate

#ifndef __ASSEMBLER__

    #include "pub/com.h"

    /* gate descriptor for interrupts and traps */
    typedef struct {
        unsigned gd_off_15_0: 16;        // low 16 bits of offset in segment
        unsigned gd_ss: 16;              // segment selector
        unsigned gd_args: 5;             // # args, 0 for interrupt/trap gates
        unsigned gd_rsv1: 3;             // reserved(should be zero I guess)
        unsigned gd_type: 4;             // type(STS_{TG,IG32,TG32})
        unsigned gd_s: 1;                // must be 0 (system)
        unsigned gd_dpl: 2;              // descriptor(meaning new) privilege level
        unsigned gd_p: 1;                // present
        unsigned gd_off_31_16: 16;       // high bits of offset in segment
    } gatedesc_t;

    /* detail at https://chyyuu.gitbooks.io/ucore_os_docs/content/lab1/lab1_3_3_2_interrupt_exception.html */
    #define IDT_SETGATE(gate, istrap, sel, off, dpl) {       \
            (gate).gd_off_15_0 = (uint32_t)(off) & 0xffff;   \
            (gate).gd_ss = (sel);                            \
            (gate).gd_args = 0;                              \
            (gate).gd_rsv1 = 0;                              \
            (gate).gd_type = (istrap) ? STS_TG32 : STS_IG32; \
            (gate).gd_s = 0;                                 \
            (gate).gd_dpl = (dpl);                           \
            (gate).gd_p = 1;                                 \
            (gate).gd_off_31_16 = (uint32_t)(off) >> 16;     \
        }
        
    /* call gate descriptor */
    #define IDT_SETCALLGATE(gate, ss, off, dpl) {          \
            (gate).gd_off_15_0 = (uint32_t)(off) & 0xffff; \
            (gate).gd_ss = (ss);                           \
            (gate).gd_args = 0;                            \
            (gate).gd_rsv1 = 0;                            \
            (gate).gd_type = STS_CG32;                     \
            (gate).gd_s = 0;                               \
            (gate).gd_dpl = (dpl);                         \
            (gate).gd_p = 1;                               \
            (gate).gd_off_31_16 = (uint32_t)(off) >> 16;   \
        }

    /* segment descriptors */
    /* detail at https://en.wikipedia.org/wiki/Global_Descriptor_Table */
    typedef struct {
        unsigned sd_lim_15_0: 16;       // low bits of segment limit
        unsigned sd_base_15_0: 16;      // low bits of segment base address
        unsigned sd_base_23_16: 8;      // middle bits of segment base address
        unsigned sd_type: 4;            // segment type (see STS_ constants)
        unsigned sd_s: 1;               // 0 = system, 1 = application
        unsigned sd_dpl: 2;             // descriptor Privilege Level
        unsigned sd_p: 1;               // present
        unsigned sd_lim_19_16: 4;       // high bits of segment limit
        unsigned sd_avl: 1;             // unused (available for software use)
        unsigned sd_rsv1: 1;            // reserved
        unsigned sd_db: 1;              // 0 = 16-bit segment, 1 = 32-bit segment
        unsigned sd_g: 1;               // granularity: limit scaled by 4K when set
        unsigned sd_base_31_24: 8;      // high bits of segment base address
    } segdesc_t;

    #define GDT_SEG_NULL ((segdesc_t) { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 })

    /* more comments at boot/bootasm.h */
    #define GDT_SEG(type, base, lim, dpl)            \
        ((segdesc_t) {                               \
            ((lim) >> 12) & 0xffff, (base) & 0xffff, \
            ((base) >> 16) & 0xff, type, 1, dpl, 1,  \
            (unsigned)(lim) >> 28, 0, 0, 1, 1,       \
            (unsigned)(base) >> 24                   \
        })

    #define GDT_SEG16(type, base, lim, dpl)         \
        ((segdesc_t) {                              \
            (lim) & 0xffff, (base) & 0xffff,        \
            ((base) >> 16) & 0xff, type, 1, dpl, 1, \
            (unsigned)(lim) >> 16, 0, 0, 1, 0,      \
            (unsigned)(base) >> 24                  \
        })
    
    #define GDT_SEG_TSS(type, base, lim, dpl)       \
        ((segdesc_t) {                              \
            (lim) & 0xffff, (base) & 0xffff,        \
            ((base) >> 16) & 0xff, type, 0, dpl, 1, \
            (unsigned)(lim) >> 16, 0, 0, 1, 0,      \
            (unsigned)(base) >> 24                  \
        })

    /* task state segment format (as described by the Pentium architecture book) */
    typedef struct {
        uint32_t ts_link;        // old ts selector
        uintptr_t ts_esp0;       // stack pointers and segment selectors
        uint16_t ts_ss0;         // after an increase in privilege level
        uint16_t ts_padding1;
        uintptr_t ts_esp1;
        uint16_t ts_ss1;
        uint16_t ts_padding2;
        uintptr_t ts_esp2;
        uint16_t ts_ss2;
        uint16_t ts_padding3;
        uintptr_t ts_cr3;        // page directory base
        uintptr_t ts_eip;        // saved state from last task switch
        uint32_t ts_eflags;
        uint32_t ts_eax;         // more saved state (registers)
        uint32_t ts_ecx;
        uint32_t ts_edx;
        uint32_t ts_ebx;
        uintptr_t ts_esp;
        uintptr_t ts_ebp;
        uint32_t ts_esi;
        uint32_t ts_edi;
        uint16_t ts_es;           // even more saved state (segment selectors)
        uint16_t ts_padding4;
        uint16_t ts_cs;
        uint16_t ts_padding5;
        uint16_t ts_ss;
        uint16_t ts_padding6;
        uint16_t ts_ds;
        uint16_t ts_padding7;
        uint16_t ts_fs;
        uint16_t ts_padding8;
        uint16_t ts_gs;
        uint16_t ts_padding9;
        uint16_t ts_ldt;
        uint16_t ts_padding10;
        uint16_t ts_t;            // trap on task switch
        uint16_t ts_iomb;         // i/o map base address
    } taskstate_t;

#endif /* __ASSEMBLER__ */

/**************** !!! LAB2 !!! ****************/

/**
 * virtual memory map:                                          Permissions
 *                                                              kernel/user
 *
 *     4G ------------------> +---------------------------------+
 *                            |                                 |
 *                            |         Empty Memory (*)        |
 *                            |                                 |
 *                            +---------------------------------+ 0xFB000000
 *                            |   Cur. Page Table (Kern, RW)    | RW/-- PTSIZE
 *     VPT -----------------> +---------------------------------+ 0xFAC00000
 *                            |        Invalid Memory (*)       | --/--
 *     KERNTOP -------------> +---------------------------------+ 0xF8000000
 *                            |                                 |
 *                            |    Remapped Physical Memory     | RW/-- KMEMSIZE
 *                            |                                 |
 *     KERNBASE ------------> +---------------------------------+ 0xC0000000
 *                            |                                 |
 *                            |                                 |
 *                            |                                 |
 *                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * (*) NOTE: The kernel ensures that "Invalid Memory" is *never* mapped.
 *     "Empty Memory" is normally unmapped, but user programs may map pages
 *     there if desired.
 *
 **/

/* all physical memory mapped at this address */
#define KERNEL_BASE            0xC0000000
#define KERNEL_MEMSIZE         0x38000000                           // the maximum amount of physical memory
#define KERNEL_TOP             (KERNEL_BASE + KERNEL_MEMSIZE)

#define KERNEL_PGSIZE          4096                                 // page size
#define KERNEL_STACKPAGE       2                                    // # of pages in kernel stack
#define KERNEL_STACKSIZE       (KERNEL_STACKPAGE * KERNEL_PGSIZE)   // sizeof kernel stack

/**
 * Virtual page table. Entry PDX[VPT] in the PD (Page Directory) contains
 * a pointer to the page directory itself, thereby turning the PD into a page
 * table, which maps all the PTEs (Page Table Entry) containing the page mappings
 * for the entire virtual address space into that 4 Meg region starting at VPT.
 **/
#define KERNEL_VPT                 0xFAC00000

/* HERE GOES THE -- PAGE!!! */

#ifndef __ASSEMBLER__

    #include "pub/com.h"
    #include "pub/atomic.h"
    #include "pub/dllist.h"

    typedef uintptr_t pte_t;
    typedef uintptr_t pde_t;
    typedef size_t page_number_t;

    // some constants for bios interrupt 15h AX = 0xE820
    #define E820_MAXENT         20      // number of entries in E820MAP
    #define E820_ARM            1       // address range memory
    #define E820_ARR            2       // address range reserved

    typedef struct {
        int nmap;
        struct {
            uint64_t addr;
            uint64_t size;
            uint32_t type;
        } C0RE_PACKED map[E820_MAXENT];
    } e820map_t;

    /**
     * struct Page - Page descriptor structures. Each Page describes one
     * physical page. In kern/mm/pmm.h, you can find lots of useful functions
     * that convert Page to other data types, such as phyical address.
     **/
    typedef struct page_t_tag {
        int ref;                        // page frame's reference counter
        uint32_t flags;                 // array of flags that describe the status of the page frame
        
        // used for allocator
        unsigned int nfree;             // number of free pages(or the real size of the page block)
        struct page_t_tag *prev;
        struct page_t_tag *next;
        
        dllist_t pra_link;              // used for pra (page replace algorithm)
        uintptr_t pra_vaddr;            // used for pra (page replace algorithm)
    } page_t;
    
    // convert dllist node to page
    #define dll2page(dll, member) \
        to_struct((dll), page_t, member)

    /* flags describing the status of a page frame */
    #define PAGE_FLAG_RESV              0 // the page is reserved for kernel and cannot be allocated
    #define PAGE_FLAG_FREE              1 // the page is freed

    #define page_setReserved(p)         btsl(PAGE_FLAG_RESV, &(p)->flags)
    #define page_resetReserved(p)       btrl(PAGE_FLAG_RESV, &(p)->flags)
    #define page_isReserved(p)          btl(PAGE_FLAG_RESV, &(p)->flags)

    #define page_setFree(p)             btsl(PAGE_FLAG_FREE, &(p)->flags)
    #define page_resetFree(p)           btrl(PAGE_FLAG_FREE, &(p)->flags)
    #define page_isFree(p)              btl(PAGE_FLAG_FREE, &(p)->flags)

    #define page_clearFlags(p)          ((p)->flags = 0)

    #define page_clearRef(p)            ((p)->ref = 0)
    #define page_incRef(p)              (++(p)->ref)
    #define page_decRef(p)              (--(p)->ref)
    #define page_getRef(p)              ((p)->ref)

    // a linear address 'la' has a three-part structure as follows:
    //
    // +--------10------+-------10-------+---------12---------+
    // | Page Directory |   Page Table   | Offset within Page |
    // |      Index     |     Index      |                    |
    // +----------------+----------------+--------------------+
    //  \--- PDX(la) --/ \--- PTX(la) --/ \---- POFF(la) ----/
    //  \----------- PPN(la) -----------/
    //
    
    // page directory index
    #define PD_INDEX(la) ((((uintptr_t)(la)) >> PD_INDEX_SHIFT) & 0x3ff)
    
    // page table index
    #define PT_INDEX(la) ((((uintptr_t)(la)) >> PT_INDEX_SHIFT) & 0x3ff)
    
    // page number field of address
    #define PAGE_NUMBER(la) (((uintptr_t)(la)) >> PT_INDEX_SHIFT)
    
    // offset in page
    #define PAGE_OFS(la) (((uintptr_t)(la)) & 0xfff)
    
    // construct linear address from indexes and offset
    #define PAGE_ADDR(d, t, o) (((uintptr_t)(d) << PD_INDEX_SHIFT | (uintptr_t)(t) << PT_INDEX_SHIFT | (uintptr_t)(o)))
    
    // address in page table or page directory entry
    #define PTE_ADDR(pte)   ((uintptr_t)(pte) & ~0xfff)
    #define PDE_ADDR(pde)   PTE_ADDR(pde)
    
    /* page directory and page table constants */
    #define PD_NENTRY       1024                    // page directory entries per page directory
    #define PT_NENTRY       1024                    // page table entries per page table
    
    #define PAGE_SIZE       4096                    // bytes mapped by a page
    #define PAGE_SHIFT      12                      // log2(PAGE_SIZE)
    #define PT_SIZE         (PAGE_SIZE * PT_NENTRY) // bytes mapped by a page directory entry
    #define PT_SHIFT        22                      // log2(PT_SIZE)
    
    #define PT_INDEX_SHIFT  12                      // offset of PT_INDEX_SHIFT in a linear address
    #define PD_INDEX_SHIFT  22                      // offset of PD_INDEX_SHIFT in a linear address
    
    /* page table/directory entry flags */
    #define PTE_FLAG_P      0x001                   // present
    #define PTE_FLAG_W      0x002                   // writeable
    #define PTE_FLAG_U      0x004                   // user
    #define PTE_FLAG_PWT    0x008                   // write-Through
    #define PTE_FLAG_PCD    0x010                   // cache-Disable
    #define PTE_FLAG_A      0x020                   // accessed
    #define PTE_FLAG_D      0x040                   // dirty
    #define PTE_FLAG_PS     0x080                   // page Size
    #define PTE_FLAG_MBZ    0x180                   // bits must be zero
    #define PTE_FLAG_AVAIL  0xe00                   // available for software use
                                                    // the PTE_AVAIL bits aren't used by the kernel or interpreted by the
                                                    // hardware, so user processes are allowed to set them arbitrarily.
    
    #define PTE_FLAG_USER   (PTE_FLAG_U | PTE_FLAG_W | PTE_FLAG_P)
    
    /* Control Register flags */
    #define CR0_PE          0x00000001              // protection enable
    #define CR0_MP          0x00000002              // monitor co-processor
    #define CR0_EM          0x00000004              // emulation
    #define CR0_TS          0x00000008              // task switched
    #define CR0_ET          0x00000010              // extension type
    #define CR0_NE          0x00000020              // numeric errror
    #define CR0_WP          0x00010000              // write protect
    #define CR0_AM          0x00040000              // alignment mask
    #define CR0_NW          0x20000000              // not writethrough
    #define CR0_CD          0x40000000              // cache disable
    #define CR0_PG          0x80000000              // paging
    
    #define CR4_PCE         0x00000100              // performance counter enable
    #define CR4_MCE         0x00000040              // machine check enable
    #define CR4_PSE         0x00000010              // page size extensions
    #define CR4_DE          0x00000008              // debugging extensions
    #define CR4_TSD         0x00000004              // time stamp disable
    #define CR4_PVI         0x00000002              // protected-mode virtual interrupts
    #define CR4_VME         0x00000001              // v86 mode extensions
    
#endif // __ASSEMBLER__

#endif
