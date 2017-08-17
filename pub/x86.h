#ifndef _LIB_X86_H_
#define _LIB_X86_H_

#include "pub/com.h"

// original do_div removed

C0RE_INLINE uint8_t inb(uint16_t port);
C0RE_INLINE void insl(uint32_t port, void *addr, int cnt);
C0RE_INLINE void outb(uint16_t port, uint8_t data);
C0RE_INLINE void outw(uint16_t port, uint16_t data);
C0RE_INLINE uint32_t read_ebp(void);

/* Pseudo-descriptors used for LGDT, LLDT(not used) and LIDT instructions. */
struct pseudodesc {
    uint16_t pd_lim;        // Limit
    uint32_t pd_base;        // Base address
} C0RE_PACKED;

C0RE_INLINE void lidt(struct pseudodesc *pd);
C0RE_INLINE void sti(void);
C0RE_INLINE void cli(void);
C0RE_INLINE void ltr(uint16_t sel);

/* INPUT byte */
C0RE_INLINE
uint8_t inb(uint16_t port)
{
    uint8_t data;
    asm volatile ("inb %1, %0" : "=a" (data) : "d" (port));
    return data;
}

/* INPUT int string */
C0RE_INLINE
void insl(uint32_t port, void *addr, int cnt)
{
    asm volatile (
        "cld;"
        "repne; insl;"
        : "=D" (addr), "=c" (cnt)
        : "d" (port), "0" (addr), "1" (cnt)
        : "memory", "cc"
    );
}

/* OUTPUT byte */
C0RE_INLINE
void outb(uint16_t port, uint8_t data)
{
    asm volatile ("outb %0, %1" :: "a" (data), "d" (port));
}

/* OUTPUT short */
C0RE_INLINE
void outw(uint16_t port, uint16_t data)
{
    asm volatile ("outw %0, %1" :: "a" (data), "d" (port));
}

C0RE_INLINE
uint32_t read_ebp()
{
    uint32_t ebp;
    asm volatile ("movl %%ebp, %0" : "=r" (ebp));
    return ebp;
}

/* load IDT */
C0RE_INLINE
void lidt(struct pseudodesc *pd)
{
    asm volatile ("lidt (%0)" :: "r" (pd));
}

C0RE_INLINE
void sti(void)
{
    asm volatile ("sti");
}

C0RE_INLINE
void cli(void)
{
    asm volatile ("cli");
}

C0RE_INLINE
void ltr(uint16_t sel)
{
    asm volatile ("ltr %0" :: "r" (sel));
}

C0RE_INLINE int __strcmp(const char *s1, const char *s2);
C0RE_INLINE char *__strcpy(char *dst, const char *src);
C0RE_INLINE void *__memset(void *s, char c, size_t n);
C0RE_INLINE void *__memmove(void *dst, const void *src, size_t n);
C0RE_INLINE void *__memcpy(void *dst, const void *src, size_t n);

#ifndef __HAVE_ARCH_STRCMP
#define __HAVE_ARCH_STRCMP

C0RE_INLINE
int __strcmp(const char *s1, const char *s2)
{
    int d0, d1, ret;
    asm volatile (
        "1: lodsb;"
        "scasb;"
        "jne 2f;"
        "testb %%al, %%al;"
        "jne 1b;"
        "xorl %%eax, %%eax;"
        "jmp 3f;"
        "2: sbbl %%eax, %%eax;"
        "orb $1, %%al;"
        "3:"
        : "=a" (ret), "=&S" (d0), "=&D" (d1)
        : "1" (s1), "2" (s2)
        : "memory"
    );
    return ret;
}

#endif /* __HAVE_ARCH_STRCMP */

#ifndef __HAVE_ARCH_STRCPY
#define __HAVE_ARCH_STRCPY

C0RE_INLINE
char *__strcpy(char *dst, const char *src)
{
    int d0, d1, d2;
    asm volatile (
        "1: lodsb;"
        "stosb;"
        "testb %%al, %%al;"
        "jne 1b;"
        : "=&S" (d0), "=&D" (d1), "=&a" (d2)
        : "0" (src), "1" (dst) : "memory"
    );
    return dst;
}

#endif /* __HAVE_ARCH_STRCPY */

#ifndef __HAVE_ARCH_MEMSET
#define __HAVE_ARCH_MEMSET

C0RE_INLINE
void *__memset(void *s, char c, size_t n)
{
    int d0, d1;
    asm volatile (
        "rep; stosb;"
        : "=&c" (d0), "=&D" (d1)
        : "0" (n), "a" (c), "1" (s)
        : "memory"
    );
    return s;
}

#endif /* __HAVE_ARCH_MEMSET */

#ifndef __HAVE_ARCH_MEMMOVE
#define __HAVE_ARCH_MEMMOVE

C0RE_INLINE
void *__memmove(void *dst, const void *src, size_t n)
{
    if (dst < src) {
        return __memcpy(dst, src, n);
    }
    int d0, d1, d2;
    asm volatile (
        "std;"
        "rep; movsb;"
        "cld;"
        : "=&c" (d0), "=&S" (d1), "=&D" (d2)
        : "0" (n), "1" (n - 1 + src), "2" (n - 1 + dst)
        : "memory"
    );
    return dst;
}

#endif /* __HAVE_ARCH_MEMMOVE */

#ifndef __HAVE_ARCH_MEMCPY
#define __HAVE_ARCH_MEMCPY

C0RE_INLINE
void *__memcpy(void *dst, const void *src, size_t n)
{
    int d0, d1, d2;
    asm volatile (
        "rep; movsl;"
        "movl %4, %%ecx;"
        "andl $3, %%ecx;"
        "jz 1f;"
        "rep; movsb;"
        "1:"
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        : "0" (n / 4), "g" (n), "1" (dst), "2" (src)
        : "memory"
    );
    return dst;
}

#endif /* __HAVE_ARCH_MEMCPY */

#endif // _LIB_X86_H_
