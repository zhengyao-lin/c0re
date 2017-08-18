#include "pub/com.h"
#include "pub/x86.h"

#include "mem/mmu.h"
#include "mem/pmm.h"

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

/* temporary kernel stack */
uint8_t stack0[1024];

/* gdt_init - initialize the default GDT and TSS */
static void gdt_init()
{
    // setup a TSS so that we can get the right stack when we trap from
    // user to the kernel. But not safe here, it's only a temporary value,
    // it will be set to KSTACKTOP in lab2.
    ts.ts_esp0 = (uint32_t)&stack0 + sizeof(stack0);
    ts.ts_ss0 = SEGR_KERNEL_DS;

    // initialize the TSS field of the gdt
    gdt[GDT_SEGNO_TSS] = GDT_SEG16(STS_T32A, (uint32_t)&ts, sizeof(ts), DPL_KERNEL);
    gdt[GDT_SEGNO_TSS].sd_s = 0;

    // reload all segment registers
    lgdt(&gdt_pd);

    // load the TSS
    ltr(GD_TSS);
}

/* pmm_init - initialize the physical memory management */
void pmm_init()
{
    gdt_init();
}

