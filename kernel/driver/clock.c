#include "pub/x86.h"

#include "intr/trap.h"
#include "driver/pic.h"
#include "driver/clock.h"

/* *
 * support for time-related hardware gadgets - the 8253 timer,
 * which generates interruptes on IRQ-0.
 * */

#define IO_TIMER1           0x040               // 8253 Timer #1

/* *
 * Frequency of all three count-down timers; (TIMER_FREQ/freq)
 * is the appropriate count to generate a frequency of freq Hz.
 * */
 
#define TIMER_TICK_PER_SEC CLOCK_TICK_PER_SEC

#define TIMER_FREQ      1193182
#define TIMER_DIV(x)    ((TIMER_FREQ + (x) / 2) / (x)) // x in ms

#define TIMER_MODE      (IO_TIMER1 + 3)         // timer mode port
#define TIMER_SEL0      0x00                    // select counter 0
#define TIMER_RATEGEN   0x04                    // mode 2, rate generator
#define TIMER_16BIT     0x30                    // r/w counter 16 bits, LSB first

static volatile size_t ticks;

long clock_tick()
{
    return ticks;
}

void _clock_inc()
{
    ticks += 1;
}

/* *
 * clock_init - initialize 8253 clock to interrupt 100 times per second,
 * and then enable IRQ_TIMER.
 * */
void
clock_init(void) {
    // set 8253 timer-chip
    outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
    outb(IO_TIMER1, TIMER_DIV(TIMER_TICK_PER_SEC) & 0xff); // low byte
    outb(IO_TIMER1, TIMER_DIV(TIMER_TICK_PER_SEC) >> 8); // high byte

    ticks = 0;

    pic_enable(IRQ_TIMER);
}
