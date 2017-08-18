#ifndef _KERNEL_MEM_MMU_H_
#define _KERNEL_MEM_MMU_H_

/* memory management unit? */

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

#ifndef LAYOUT_ONLY

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

#else
    #undef LAYOUT_ONLY
#endif /* LAYOUT_ONLY */

#endif
