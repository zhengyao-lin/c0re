#include "mem/mmu.h"

#define REALLOC(x) (x - KERNEL_BASE)

.text

.globl c0re_entry
c0re_entry:

    # reload temperate gdt (second time) to remap all physical memory
    # virtual_addr 0~4G=linear_addr&physical_addr -KERNBASE~4G-KERNBASE 
    lgdt REALLOC(gdtdesc)
    movl $SEGR_KERNEL_DS, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    ljmp $SEGR_KERNEL_CS, $relocated

relocated:

    # set ebp, esp
    movl $0x0, %ebp
    # the kernel stack region is from kernel_stack -- kernel_stacktop,
    # the kernel stack size is KSTACKSIZE (8KB)defined in memlayout.h
    movl $kernel_stacktop, %esp
    
    # now kernel stack is ready , call the first C function
    call c0re_init

    # should never get here
    hlt

.data

.align KERNEL_PGSIZE

.globl kernel_stack
kernel_stack:
    .space KERNEL_STACKSIZE
    
.globl kernel_stacktop
kernel_stacktop:

.align 4
gdt:
    GDT_SEGNULL_ASM            # TODO: why the f there is a minus
    GDT_SEG_ASM(STA_X | STA_R, - KERNEL_BASE, 0xFFFFFFFF)      # code segment
    GDT_SEG_ASM(STA_W, - KERNEL_BASE, 0xFFFFFFFF)              # data segment

gdtdesc:
    .word 0x17                                              # sizeof(__gdt) - 1
    .long REALLOC(gdt)
