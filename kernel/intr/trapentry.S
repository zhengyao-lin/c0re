#define LAYOUT_ONLY
#include "mem/mmu.h"
#undef LAYOUT_ONLY

# vectors.S sends all traps here.
.text

.globl c0re_alltraps
c0re_alltraps:
    # push registers to build a trap frame
    # therefore make the stack look like a trapframe_t
	# (stack grows down)
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    pushal

    # load GD_KDATA into %ds and %es to set up data segments for kernel
    movl $GD_KDATA, %eax
    movw %ax, %ds
    movw %ax, %es

    # push %esp to pass a pointer to the trapframe as an argument to trap()
	# i.e. trapframe_t *tf
    pushl %esp

    # call trap(tf), where tf=%esp
    call c0re_trap

    # pop the pushed stack pointer
    popl %esp

    # return falls through to trapret...
.globl c0re_trapret
c0re_trapret:
    # restore registers from stack
    popal

    # restore %ds, %es, %fs and %gs
    popl %gs
    popl %fs
    popl %es
    popl %ds

    # get rid of the trap number and error code
    addl $0x8, %esp
    iret
