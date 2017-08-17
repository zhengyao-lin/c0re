#ifndef _KERNEL_LIB_DEBUG_H_
#define _KERNEL_LIB_DEBUG_H_

#include "driver/console.h"
#include "lib/io.h"

#define trace(msg) \
    kputs(msg); \
    kputc('\n')

#endif
