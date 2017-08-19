#include "bootasm.h"

# start the CPU: switch to 32-bit protected mode, jump into C.
# the BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.set PROT_MODE_CSEG,        8                       # kernel code segment selector
.set PROT_MODE_DSEG,        16                      # kernel data segment selector
.set CR0_PE_ON,             0x1                     # protected mode enable flag
.set SMAP,                  0x534d4150              # just a signature for INT15h
.set ARD_SIZE,              20                      # address range descriptor size

# start address should be 0:7c00, in real mode, the beginning address of the running bootloader
.globl start
start:

.code16                                             # 16-bit mode

    cli                                             # disable interrupts
    cld                                             # string operations increment

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

    # probe memory
probe_memory:
    movl $0, 0x8000
    xorl %ebx, %ebx             # first time calling INT15h -> 0, else the last value after calling it
    movw $0x8004, %di           # es:di specifies the location to store the descriptor(line 42, es has already been set to 0)

start_probe:
    movl $0xe820, %eax          # just an argument for this function(in INT15h)
    movl $ARD_SIZE, %ecx        # size of the address range descriptor
    movl $SMAP, %edx            # signature
    int $0x15
    
    jnc cont                    # success -> jump to continue
    movw $12345, 0x8000         # not success!
    jmp finish_probe            # TODO: expect to do something??
    
cont:
    addw $ARD_SIZE, %di         # next descriptor address
    incl 0x8000                 # number of descriptors
    cmpl $0, %ebx               # if ebx is 0 -> probe finished
    jnz start_probe
    
finish_probe:

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

.data

# bootstrap GDT
.p2align 2                                          # force 4 byte alignment
gdt:
    GDT_SEGNULL_ASM                                 # null seg
    GDT_SEG_ASM(STA_X | STA_R, 0x0, 0xffffffff)     # code seg for bootloader and kernel, offset PROT_MODE_CSEG
    GDT_SEG_ASM(STA_W, 0x0, 0xffffffff)             # data seg for bootloader and kernel, offset PROT_MODE_DSEG

gdtdesc:
    .short 23                                       # sizeof(gdt) - 1 = 24 - 1 = 23
    .int gdt                                        # address gdt
