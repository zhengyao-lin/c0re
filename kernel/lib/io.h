#ifndef _KERNEL_LIB_IO_H_
#define _KERNEL_LIB_IO_H_

#include "pub/stdarg.h"

#include "driver/console.h"

#define kputc cons_putc

int kputns(char *str, int max);
int kputs(char *str);
int vkprintf(const char *fmt, va_list ap);
int kprintf(const char *fmt, ...);

#endif
