#ifndef _KERNEL_DRIVER_CONSOLE_H_
#define _KERNEL_DRIVER_CONSOLE_H_

void cons_init();
void cons_putc(int c);
int cons_getc();

#endif
