#ifndef _KERNEL_LIB_IO_H_
#define _KERNEL_LIB_IO_H_

#include "driver/console.h"

int kputns(char *str, int max);
int kputs(char *str);

#endif
