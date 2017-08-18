#ifndef _KERNEL_INTR_TRAP_H_
#define _KERNEL_INTR_TRAP_H_

#include "pub/com.h"
#include "pub/x86.h"

#define intr_enable() sti()
#define intr_disable() cli()

/* trap Numbers */

/* processor-defined: */
#define TRAPNO_DIVIDE               0    // divide error
#define TRAPNO_DEBUG                1    // debug exception
#define TRAPNO_NMI                  2    // non-maskable interrupt
#define TRAPNO_BRKPT                3    // breakpoint
#define TRAPNO_OFLOW                4    // overflow
#define TRAPNO_BOUND                5    // bounds check
#define TRAPNO_ILLOP                6    // illegal opcode
#define TRAPNO_DEVICE               7    // device not available
#define TRAPNO_DBLFLT               8    // double fault
#define TRAPNO_COPROC               9    // reserved (not used since 486)
#define TRAPNO_TSS                  10   // invalid task switch segment
#define TRAPNO_SEGNP                11   // segment not present
#define TRAPNO_STACK                12   // stack exception
#define TRAPNO_GPFLT                13   // general protection fault
#define TRAPNO_PGFLT                14   // page fault
#define TRAPNO_RES                  15   // reserved
#define TRAPNO_FPERR                16   // floating point error
#define TRAPNO_ALIGN                17   // aligment check
#define TRAPNO_MCHK                 18   // machine check
#define TRAPNO_SIMDERR              19   // SIMD floating point error

#define TRAPNO_SYSCALL              0x80 // SYSCALL, ONLY FOR THIS PROJ

/* hardware IRQ numbers. We receive these as (IRQ_OFFSET + IRQ_xx) */
#define IRQ_OFFSET                  32    // IRQ 0 corresponds to int IRQ_OFFSET

#define IRQ_TIMER                   0
#define IRQ_KBD                     1
#define IRQ_COM1                    4
#define IRQ_IDE1                    14
#define IRQ_IDE2                    15
#define IRQ_ERROR                   19
#define IRQ_SPURIOUS                31

/**
 * these are arbitrarily chosen, but with care not to overlap
 * processor defined exceptions or interrupt vectors.
 **/
#define TRAPNO_SWITCH_TOU           120    // to user switch
#define TRAPNO_SWITCH_TOK           121    // to kernel switch

typedef struct {
    /* registers as pushed by pushal */
    struct pushregs {
        uint32_t reg_edi;
        uint32_t reg_esi;
        uint32_t reg_ebp;
        uint32_t reg_oesp; /* useless */
        uint32_t reg_ebx;
        uint32_t reg_edx;
        uint32_t reg_ecx;
        uint32_t reg_eax;
    } tf_regs;
    
    uint16_t tf_gs;
    uint16_t tf_padding0;
    uint16_t tf_fs;
    uint16_t tf_padding1;
    uint16_t tf_es;
    uint16_t tf_padding2;
    uint16_t tf_ds;
    uint16_t tf_padding3;
    uint32_t tf_trapno;
    /* below here defined by x86 hardware */
    uint32_t tf_err;
    uintptr_t tf_eip;
    uint16_t tf_cs;
    uint16_t tf_padding4;
    uint32_t tf_eflags;
    /* below here only when crossing rings, such as from user to kernel */
    uintptr_t tf_esp;
    uint16_t tf_ss;
    uint16_t tf_padding5;
} C0RE_PACKED trapframe_t;

void idt_init();
// void print_trapframe(struct trapframe *tf);
// void print_regs(struct pushregs *regs);
// bool trap_in_kernel(struct trapframe *tf);

#endif
