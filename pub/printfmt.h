#ifndef _PUB_PRINTFMT_H_
#define _PUB_PRINTFMT_H_

#include "pub/com.h"
#include "pub/stdarg.h"

/* print format */

typedef void (*putc_fn_t)(int, void*);

void printfmt(putc_fn_t putch, void *putdat, const char *fmt, ...);
void vprintfmt(putc_fn_t putch, void *putdat, const char *fmt, va_list ap);
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

#endif
