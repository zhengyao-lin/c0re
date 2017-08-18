#include "pub/x86.h"

#include "lib/io.h"
#include "lib/debug.h"

void _panic(char *file, int line, const char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    kprintf("c0re panicked at %s: line %d: ", file, line);
    vkprintf(fmt, ap);
    kputc('\n');
    va_end(ap);
    
    hlt();
}
