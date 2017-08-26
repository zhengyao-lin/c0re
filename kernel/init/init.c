#include "pub/string.h"

#include "lib/debug.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/swap.h"

#include "intr/trap.h"

#include "driver/pic.h"
#include "driver/clock.h"
#include "driver/ide.h"

void c0re_init() {
    // extern char bss_begin[], bss_end[];
    // memset(bss_begin, 0, bss_end - bss_begin);
    // initialize bss -- in case someone forget to init?
    
    cons_init();

    trace("c0re starting");
    
    swap_disable();
    
    pmm_init();
    
    pic_init();
    idt_init();

    vmm_init();

    ide_init();
    swap_init();
    
    clock_init();
 
    intr_enable();
    
    while (1) ;
}
