#ifndef _KERNEL_DRIVER_CLOCK_H_
#define _KERNEL_DRIVER_CLOCK_H_

#include "pub/com.h"

#define CLOCK_TICK_PER_SEC 100

void clock_init();
long clock_tick();

// used only in trap.c
void _clock_inc();

#endif
