#include "pub/string.h"

#include "lib/debug.h"
#include "mem/pmm.h"
#include "intr/trap.h"
#include "driver/pic.h"
#include "driver/clock.h"

void c0re_init() {
    // extern char bss_begin[], bss_end[];
    // memset(bss_begin, 0, bss_end - bss_begin);
    // initialize bss -- in case someone forget to init?
    
    cons_init();

    trace("hello, c0re");
    
    // TODO !!!
    // pmm_init();
    
    // pic_init();
    // idt_init();
    // 
    // intr_enable();
    // 
    // clock_init();
    
    while (1) ;
}
