#include "lib/debug.h"
#include "mem/pmm.h"
#include "intr/trap.h"
#include "driver/pic.h"
#include "driver/clock.h"

void c0re_init() {
    cons_init();
    
    trace("hello, c0re");
    
    pmm_init();
    
    pic_init();
    idt_init();
    
    intr_enable();
    
    clock_init();
    
    while (1) ;
}
