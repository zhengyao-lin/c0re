#ifndef _PUB_ATOMIC_H_
#define _PUB_ATOMIC_H_

#include "pub/com.h"

/* Atomic operations that C can't guarantee us. Useful for resource counting etc.. */

/**
 * NOTE: bt*l instructions respectively do an action on the nth bit of *(int *)addr
 * btsl: set the bit
 * btrl: reset the bit
 * btcl: change/flip the bit
 * btl: read the bit
 **/

C0RE_INLINE void btsl(int n, volatile void *addr);
C0RE_INLINE void btrl(int n, volatile void *addr);
C0RE_INLINE void btcl(int n, volatile void *addr);
C0RE_INLINE bool btl(int n, volatile void *addr);

/**
 * set_bit - Atomically set a bit in memory
 * @nr:     the bit to set
 * @addr:   the address to start counting from
 *
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 **/
C0RE_INLINE
void btsl(int n, volatile void *addr)
{
    asm volatile ("btsl %1, %0" :"=m" (*(volatile long *)addr) : "Ir" (n));
}

/**
 * clear_bit - Atomically clears a bit in memory
 * @nr:     the bit to clear
 * @addr:   the address to start counting from
 **/
C0RE_INLINE
void btrl(int n, volatile void *addr)
{
    asm volatile ("btrl %1, %0" :"=m" (*(volatile long *)addr) : "Ir" (n));
}

/**
 * change_bit - Atomically toggle a bit in memory
 * @nr:     the bit to change
 * @addr:   the address to start counting from
 **/
C0RE_INLINE
void btcl(int n, volatile void *addr)
{
    asm volatile ("btcl %1, %0" :"=m" (*(volatile long *)addr) : "Ir" (n));
}

/**
 * test_bit - Determine whether a bit is set
 * @nr:     the bit to test
 * @addr:   the address to count from
 **/
C0RE_INLINE
bool btl(int n, volatile void *addr)
{
    int oldbit;
    asm volatile ("btl %2, %1; sbbl %0, %0" : "=r" (oldbit) : "m" (*(volatile long *)addr), "Ir" (n));
    return oldbit != 0;
}

#endif
