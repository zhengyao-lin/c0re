#include "pub/com.h"
#include "pub/x86.h"
#include "pub/string.h"

#include "lib/io.h"
#include "lib/debug.h"
#include "intr/trap.h"

#include "mem/mmu.h"
#include "mem/vmm.h"

#include "driver/console.h"
#include "driver/clock.h"

/* *
 * Interrupt descriptor table:
 *
 * Must be built at run time because shifted function addresses can't
 * be represented in relocation records.
 * */
static gatedesc_t idt[256] = {{0}};

static descloader_t idt_pd = {
    sizeof(idt) - 1, (uintptr_t)idt
};

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S */
void idt_init()
{
    /* init IDT by ISR(interrupt service routine), i.e. c0re_trapvec */
    extern uintptr_t c0re_trapvec[];
    int i;

    for (i = 0; i < C0RE_ARRLEN(idt); i++) {
        IDT_SETGATE(idt[i], false, GD_KTEXT, c0re_trapvec[i], DPL_KERNEL);
    }

    IDT_SETGATE(idt[TRAPNO_SWITCH_TOK], false, GD_KTEXT, c0re_trapvec[TRAPNO_SWITCH_TOK], DPL_USER);

    lidt(&idt_pd);
}

/* trap_in_kernel - test if trap happened in kernel */
// bool
// trap_in_kernel(struct trapframe *tf) {
//     return (tf->tf_cs == (uint16_t)KERNEL_CS);
// }

C0RE_INLINE
int handle_page_fault(trapframe_t *tf)
{
    extern vma_set_t *c0re_check_vma_set;
    
    // print_pgfault(tf);
    
    // NOTE: currently only used in check?
    if (c0re_check_vma_set) {
        return vmm_doPageFault(c0re_check_vma_set, tf->tf_err, rcr2());
    }
    
    panic("unhandled page fault");
    return -1;
}

trapframe_t switchk2u, *switchu2k;

/* trap_dispatch - dispatch based on what type of trap occurred */
static void dispatch(trapframe_t *tf)
{
    int c, ret;

    switch (tf->tf_trapno) {
        case TRAPNO_PGFLT:
            // page fault
            // trace("page fault!");
            if ((ret = handle_page_fault(tf)) != 0) {
                panic("unable to handle page fault error: %e", ret);
            }
            
            break;
        
        case IRQ_OFFSET + IRQ_TIMER:
            // trace("timer");
            // TODO: finish after clock is finished
            // increase a system clock variable
            // some debug util probably
            _clock_inc();
            break;
            
        case IRQ_OFFSET + IRQ_COM1:
            trace("com1 intr");
            // c = cons_getc();
            // cprintf("serial [%03d] %c\n", c, c);
            break;

        case IRQ_OFFSET + IRQ_KBD:
            // trace("cur time: %d sec", (int)((double)clock_tick() / CLOCK_TICK_PER_SEC));
            c = cons_getc();
            if (c) kputc(c); // simple echo
            // kprintf("kbd [%03d] %c\n", c, c);
            break;

        case TRAPNO_SWITCH_TOU:
            if (tf->tf_cs != SEGR_USER_CS) {
                // not in user mode?
                switchk2u = *tf;
                switchk2u.tf_cs = SEGR_USER_CS;
                switchk2u.tf_ds = switchk2u.tf_es = switchk2u.tf_ss = SEGR_USER_DS;
    		
                // set eflags, make sure ucore can use io under user mode.
                // if CPL > IOPL, then cpu will generate a general protection.
                switchk2u.tf_eflags |= EFLAG_IOPL_MASK;
                
                // --------------------- +
                // |    lower stack    |
                // ---------------------
                // |                   | <- switchk2u.tf_esp (we want to set the next esp here)
                // |     trapframe     |
                // |                   | <- tf
                // ---------------------
                // |    stored esp     | line 24: pushl %esp in trapentry.S
                // ---------------------
                switchk2u.tf_esp = (uintptr_t)tf + sizeof(trapframe_t) - 8;
    		
                // set temporary stack
                // then iret will jump to the right stack
                // set the "stored esp" in the diagram aobve
                *((uintptr_t *)tf - 1) = (uintptr_t)&switchk2u;
            }
            
            break;
            
        case TRAPNO_SWITCH_TOK:
            if (tf->tf_cs != SEGR_KERNEL_CS) {
                tf->tf_cs = SEGR_KERNEL_CS;
                tf->tf_ds = tf->tf_es = SEGR_KERNEL_DS;
                tf->tf_eflags &= ~EFLAG_IOPL_MASK;
                
                // --------------------- +
                // |    lower stack    |
                // ---------------------
                // |                   |
                // |     trapframe     |
                // |                   | <- tf
                // ---------------------
                // |    stored esp     |
                // ---------------------
                // ?? where does this point to ??
                switchu2k = (trapframe_t *)(tf->tf_esp - (sizeof(trapframe_t) - 8));
                
                memmove(switchu2k, tf, sizeof(trapframe_t) - 8);
                *((uintptr_t *)tf - 1) = (uintptr_t)switchu2k;
            }
            break;
        
        case IRQ_OFFSET + IRQ_IDE1:
        case IRQ_OFFSET + IRQ_IDE2:
            /* do nothing */
            break;
            
        default:
            // in kernel, it must be a mistake
            if ((tf->tf_cs & 3) == 0) {
                // print_trapframe(tf);
                panic("unexpected trap in kernel");
            }
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap() returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void c0re_trap(trapframe_t *tf)
{
    // dispatch based on what type of trap occurred
    dispatch(tf);
}
