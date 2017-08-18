#include "pub/com.h"
#include "pub/stdarg.h"
#include "pub/printfmt.h"

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

static void kputch(int c, int *cnt)
{
    kputc(c);
    (*cnt)++;
}

int vkprintf(const char *fmt, va_list ap)
{
    int cnt = 0;
    vprintfmt((putc_fn_t)kputch, &cnt, fmt, ap);
    return cnt;
}

int kprintf(const char *fmt, ...)
{
    va_list ap;
    int cnt;
    
    va_start(ap, fmt);
    cnt = vkprintf(fmt, ap);
    va_end(ap);
    
    return cnt;
}
