#include "bootasm.h"

# start the CPU: switch to 32-bit protected mode, jump into C.
# the BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.set PROT_MODE_CSEG,        8                       # kernel code segment selector
.set PROT_MODE_DSEG,        16                      # kernel data segment selector
.set CR0_PE_ON,             0x1                     # protected mode enable flag

# start address should be 0:7c00, in real mode, the beginning address of the running bootloader
.globl start
start:

.code16                                             # 16-bit mode

    # test output
    # mov $0x7c0, %ax
    # mov %ax, %ds
    # mov %ax, %es
    #   
    # mov $msg, %ax
    # mov %ax, %bp
    # mov $0x13, %ah
    # mov $0, %al
    # mov $0, %bh
    # mov $0xFC, %bl
    # mov $10, %cx
    # mov $1, %dx
    # int $0x10
    # 
    # hlt

    cli                                             # Disable interrupts
    cld                                             # String operations increment

    # set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    movw %ax, %ds                                   # -> Data Segment
    movw %ax, %es                                   # -> Extra Segment
    movw %ax, %ss                                   # -> Stack Segment

    # enable A20
seta20.1:
    inb $0x64, %al                                  # wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.1

    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64                                 # 0xd1 means: write data to 8042's P2 port

seta20.2:
    inb $0x64, %al                                  # wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.2

    movb $0xdf, %al                                 # 0xdf -> port 0x60
    outb %al, $0x60                                 # 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1

    # switch from real to protected mode, using a bootstrap GDT
    # and segment translation that makes virtual addresses
    # identical to physical addresses, so that the
    # effective memory map does not change during the switch.
    lgdt gdtdesc
    
    # set PE bit of cr0
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0

    # jump to next instruction, but in 32-bit code segment.
    # switches processor into 32-bit mode.
    ljmp $PROT_MODE_CSEG, $protcseg

.code32                                             # 32-bit mode
protcseg:
    # set up the protected-mode data segment registers
    movw $PROT_MODE_DSEG, %ax                       # data segment selector
    movw %ax, %ds                                   # -> DS: Data Segment
    movw %ax, %es                                   # -> ES: Extra Segment
    movw %ax, %fs                                   # -> FS
    movw %ax, %gs                                   # -> GS
    movw %ax, %ss                                   # -> SS: Stack Segment

    # Set up the stack pointer and call into C. The stack region is from 0--0x7c00(start)
    movl $0x0, %ebp
    movl $start, %esp
    call bootmain

    # TODO: not sure if we can change the spin to halt
    hlt

# bootstrap GDT
.p2align 2                                          # force 4 byte alignment
gdt:
    SEG_NULLASM                                     # null seg
    SEG_ASM(STA_X | STA_R, 0x0, 0xffffffff)         # code seg for bootloader and kernel, offset PROT_MODE_CSEG
    SEG_ASM(STA_W, 0x0, 0xffffffff)                 # data seg for bootloader and kernel, offset PROT_MODE_DSEG

gdtdesc:
    .short 23                                       # sizeof(gdt) - 1 = 24 - 1 = 23
    .int gdt                                        # address gdt
    
msg:
    .ascii "123"
