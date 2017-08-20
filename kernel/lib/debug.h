#ifndef _KERNEL_LIB_DEBUG_H_
#define _KERNEL_LIB_DEBUG_H_

#include "driver/console.h"
#include "lib/io.h"

#define trace(...) \
    kprintf(__VA_ARGS__); \
    kputc('\n')
    
void _panic(char *file, int line, const char *fmt, ...);

#define panic(...) _panic(__FILE__, __LINE__, __VA_ARGS__)

#define assert(cond) \
    if (!(cond)) { \
        panic("assertion error: %s\n", #cond); \
    }
    
#define DBG_TAB "   "

#endif
