#include "lib/io.h"

int kputns(char *str, int max)
{
    int count;
    
    for (count = 0; *str != '\0'; str++, count++) {
        if (count >= max) break;
        kputc(*str);
    }
    
    return count;
}

int kputs(char *str)
{
    int count;
    
    for (count = 0; *str != '\0'; str++, count++) {
        kputc(*str);
    }
    
    return count;
}
