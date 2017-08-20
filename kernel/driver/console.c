#include "pub/com.h"
#include "pub/x86.h"
#include "pub/string.h"

#include "lib/io.h"
#include "lib/debug.h"
#include "lib/sync.h"

#include "intr/trap.h"
#include "driver/pic.h"
#include "driver/kbdreg.h"

#include "mem/mmu.h"
#include "mem/pmm.h"

/* stupid I/O delay routine necessitated by historical PC design flaws */
C0RE_INLINE
void delay(void)
{
    inb(0x84); inb(0x84);
    inb(0x84); inb(0x84);
}

/***** Serial I/O code *****/
#define COM1            0x3F8

#define COM_RX          0       // In:  Receive buffer (DLAB=0)
#define COM_TX          0       // Out: Transmit buffer (DLAB=0)
#define COM_DLL         0       // Out: Divisor Latch Low (DLAB=1)
#define COM_DLM         1       // Out: Divisor Latch High (DLAB=1)
#define COM_IER         1       // Out: Interrupt Enable Register
#define COM_IER_RDI     0x01    // Enable receiver data interrupt
#define COM_IIR         2       // In:  Interrupt ID Register
#define COM_FCR         2       // Out: FIFO Control Register
#define COM_LCR         3       // Out: Line Control Register
#define COM_LCR_DLAB    0x80    // Divisor latch access bit
#define COM_LCR_WLEN8   0x03    // Wordlength: 8 bits
#define COM_MCR         4       // Out: Modem Control Register
#define COM_MCR_RTS     0x02    // RTS complement
#define COM_MCR_DTR     0x01    // DTR complement
#define COM_MCR_OUT2    0x08    // Out2 complement
#define COM_LSR         5       // In:  Line Status Register
#define COM_LSR_DATA    0x01    // Data available
#define COM_LSR_TXRDY   0x20    // Transmit buffer avail
#define COM_LSR_TSRE    0x40    // Transmitter off

#define MONO_BASE       0x3B4
#define MONO_BUF        0xB0000
#define CGA_BASE        0x3D4
#define CGA_BUF         0xB8000
#define CRT_ROWS        25
#define CRT_COLS        80
#define CRT_SIZE        (CRT_ROWS * CRT_COLS)

#define LPTPORT         0x378

static uint16_t *crt_buf;
static uint16_t crt_pos;
static uint16_t addr_6845;

// CGA = Color Graphics Adaptor
// CGA video memory mapping:
//   -- 0xB0000 - 0xB7777 monocolor text mode
//   -- 0xB8000 - 0xBFFFF multicolor text mode
// 
// 6845 chip is the video controller in IBM PC
// 6845 is controlled using IO port 0x3B4-0x3B5 for monocolor and 0x3D4-0x3D5 for multicolor
//   -- 0x3D5, 0x3B5: data register
//   -- 0x3D4, 0x3B4: index register(type of data in 0x3D5 and 0x3B5)

/* TEXT-mode CGA/VGA display output */
static void cga_init(void)
{
    volatile uint16_t *cp = (uint16_t *)KADDR(CGA_BUF);
                                                   // CGA_BUF: 0xB8000 (base address for multicolor video memory)
                                                   // use KADDR to reach the real physical address
    uint16_t was = *cp;                            // same the current value
    
    *cp = (uint16_t) 0xA55A;                       // write an arbitrary value
    
    if (*cp != 0xA55A) {                           // unable to get the same value
        cp = (uint16_t*)KADDR(MONO_BUF);           // monocolor
        addr_6845 = MONO_BASE;                     // MONO_BASE: 0x3B4
    } else {                                       // multicolor
        *cp = was;                                 // restore
        addr_6845 = CGA_BASE;                      // CGA_BASE: 0x3D4 
    }

    // 6845 index reg: 0eh -> read cursor(high byte)
    // 6845 index reg: 0fh -> read cursor(low byte)    
    uint32_t pos;
    
    outb(addr_6845, 14);                                        
    pos = inb(addr_6845 + 1) << 8;                 // high byte
    outb(addr_6845, 15);
    pos |= inb(addr_6845 + 1);                     // low byte

    crt_buf = (uint16_t*) cp;                      // start address of video memory
    crt_pos = pos;                                 // curosr position
}

static bool serial_exists = 0;

static void serial_init(void)
{
    // turn off the FIFO
    outb(COM1 + COM_FCR, 0);

    // set speed; requires DLAB latch
    outb(COM1 + COM_LCR, COM_LCR_DLAB);
    outb(COM1 + COM_DLL, (uint8_t) (115200 / 9600));
    outb(COM1 + COM_DLM, 0);

    // 8 data bits, 1 stop bit, parity off; turn off DLAB latch
    outb(COM1 + COM_LCR, COM_LCR_WLEN8 & ~COM_LCR_DLAB);

    // no modem controls
    outb(COM1 + COM_MCR, 0);
    // enable rcv interrupts
    outb(COM1 + COM_IER, COM_IER_RDI);

    // clear any preexisting overrun indications and interrupts
    // serial port doesn't exist if COM_LSR returns 0xFF
    serial_exists = (inb(COM1 + COM_LSR) != 0xFF);
    (void)inb(COM1 + COM_IIR);
    (void)inb(COM1 + COM_RX);

    if (serial_exists) {
        pic_enable(IRQ_COM1);
    }
}

static void lpt_putc_sub(int c)
{
    int i;
    
    for (i = 0; !(inb(LPTPORT + 1) & 0x80) && i < 12800; i++) {
        delay();
    }
    
    outb(LPTPORT + 0, c);
    outb(LPTPORT + 2, 0x08 | 0x04 | 0x01);
    outb(LPTPORT + 2, 0x08);
}

/* lpt_putc - copy console output to parallel port */
static void lpt_putc(int c)
{
    if (c != '\b') {
        lpt_putc_sub(c);
    } else {
        lpt_putc_sub('\b');
        lpt_putc_sub(' ');
        lpt_putc_sub('\b');
    }
}

/* cga_putc - print character to console */
static void cga_putc(int c)
{
    // set black on white
    if (!(c & ~0xFF)) {
        c |= 0x0700;
    }

    switch (c & 0xff) {
        // TODO: temporary solution for control characters
        case '\b':
            if (crt_pos > 0) {
                crt_pos--;
                crt_buf[crt_pos] = (c & ~0xff) | ' ';
            }
            break;
        
        case '\n':
            crt_pos += CRT_COLS;
            // fallthrough, UNIX-like \n
        
        case '\r':
            crt_pos -= (crt_pos % CRT_COLS);
            break;
            
        default:
            crt_buf[crt_pos++] = c;     // write the character
            break;
    }

    // what is the purpose of this?
    // -- scroll to the next line?
    if (crt_pos >= CRT_SIZE) {
        int i;
        
        memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
        
        for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i ++) {
            crt_buf[i] = 0x0700 | ' ';
        }
        
        crt_pos -= CRT_COLS;
    }

    // move that little blinky thing
    outb(addr_6845, 14);
    outb(addr_6845 + 1, crt_pos >> 8);
    outb(addr_6845, 15);
    outb(addr_6845 + 1, crt_pos);
}

static void serial_putc_sub(int c)
{
    int i;
    
    for (i = 0; !(inb(COM1 + COM_LSR) & COM_LSR_TXRDY) && i < 12800; i++) {
        delay();
    }
    
    outb(COM1 + COM_TX, c);
}

/* serial_putc - print character to serial port */
static void serial_putc(int c)
{
    if (c != '\b') {
        serial_putc_sub(c);
    } else {
        serial_putc_sub('\b');
        serial_putc_sub(' ');
        serial_putc_sub('\b');
    }
}

/**
 * Here we manage the console input buffer, where we stash characters
 * received from the keyboard or serial port whenever the corresponding
 * interrupt occurs.
 **/

#define CONSBUFSIZE 512

static struct {
    uint8_t buf[CONSBUFSIZE];
    uint32_t rpos; // read position
    uint32_t wpos; // write position
} cons;

/**
 * cons_intr - called by device interrupt routines to feed input
 * characters into the circular console input buffer.
 **/
static void cons_intr(int (*proc)())
{
    int c;
    
    while ((c = (*proc)()) != -1) {
        if (c != 0) {
            cons.buf[cons.wpos++] = c;
            
            // wrap around
            if (cons.wpos == CONSBUFSIZE) {
                cons.wpos = 0;
            }
        }
    }
}

/* serial_proc_data - get data from serial port */
static int serial_proc_data()
{
    if (!(inb(COM1 + COM_LSR) & COM_LSR_DATA)) {
        return -1;
    }
    
    int c = inb(COM1 + COM_RX);
    
    if (c == 127) {
        c = '\b';
    }
    
    return c;
}

/* serial_intr - try to feed input characters from serial port */
void serial_intr()
{
    if (serial_exists) {
        cons_intr(serial_proc_data);
    }
}

/***** Keyboard input code *****/

#define NO              0

#define SHIFT           (1 << 0)
#define CTRL            (1 << 1)
#define ALT             (1 << 2)

#define CAPSLOCK        (1 << 3)
#define NUMLOCK         (1 << 4)
#define SCROLLLOCK      (1 << 5)

#define E0ESC           (1 << 6)

static uint8_t shiftcode[256] = {
    [0x1D] CTRL,
    [0x2A] SHIFT,
    [0x36] SHIFT,
    [0x38] ALT,
    [0x9D] CTRL,
    [0xB8] ALT
};

static uint8_t togglecode[256] = {
    [0x3A] CAPSLOCK,
    [0x45] NUMLOCK,
    [0x46] SCROLLLOCK
};

// key map on normal press
static uint8_t normalmap[256] = {
    NO,   0x1B, '1',  '2',  '3',  '4',  '5',  '6',  // 0x00
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  // 0x10
    'o',  'p',  '[',  ']',  '\n', NO,   'a',  's',
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  // 0x20
    '\'', '`',  NO,   '\\', 'z',  'x',  'c',  'v',
    'b',  'n',  'm',  ',',  '.',  '/',  NO,   '*',  // 0x30
    NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
    NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',  // 0x40
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
    '2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,   // 0x50
    [0xC7] KEY_HOME,        [0x9C] '\n' /*KP_Enter*/,
    [0xB5] '/' /*KP_Div*/,  [0xC8] KEY_UP,
    [0xC9] KEY_PGUP,        [0xCB] KEY_LF,
    [0xCD] KEY_RT,          [0xCF] KEY_END,
    [0xD0] KEY_DN,          [0xD1] KEY_PGDN,
    [0xD2] KEY_INS,         [0xD3] KEY_DEL
};

// key map when shift is pressed
static uint8_t shiftmap[256] = {
    NO,   033,  '!',  '@',  '#',  '$',  '%',  '^',  // 0x00
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  // 0x10
    'O',  'P',  '{',  '}',  '\n', NO,   'A',  'S',
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  // 0x20
    '"',  '~',  NO,   '|',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  '<',  '>',  '?',  NO,   '*',  // 0x30
    NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
    NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',  // 0x40
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
    '2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,   // 0x50
    [0xC7] KEY_HOME,        [0x9C] '\n' /*KP_Enter*/,
    [0xB5] '/' /*KP_Div*/,  [0xC8] KEY_UP,
    [0xC9] KEY_PGUP,        [0xCB] KEY_LF,
    [0xCD] KEY_RT,          [0xCF] KEY_END,
    [0xD0] KEY_DN,          [0xD1] KEY_PGDN,
    [0xD2] KEY_INS,         [0xD3] KEY_DEL
};

#define C(x) (x - '@')

// key map when control is pressed
static uint8_t ctrlmap[256] = {
    NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
    NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
    C('Q'),  C('W'),  C('E'),  C('R'),  C('T'),  C('Y'),  C('U'),  C('I'),
    C('O'),  C('P'),  NO,      NO,      '\r',    NO,      C('A'),  C('S'),
    C('D'),  C('F'),  C('G'),  C('H'),  C('J'),  C('K'),  C('L'),  NO,
    NO,      NO,      NO,      C('\\'), C('Z'),  C('X'),  C('C'),  C('V'),
    C('B'),  C('N'),  C('M'),  NO,      NO,      C('/'),  NO,      NO,
    [0x97] KEY_HOME,
    [0xB5] C('/'),      [0xC8] KEY_UP,
    [0xC9] KEY_PGUP,    [0xCB] KEY_LF,
    [0xCD] KEY_RT,      [0xCF] KEY_END,
    [0xD0] KEY_DN,      [0xD1] KEY_PGDN,
    [0xD2] KEY_INS,     [0xD3] KEY_DEL
};

static uint8_t *charcode[4] = {
    normalmap,
    shiftmap,
    ctrlmap,
    ctrlmap
};

/**
 * kbd_proc_data - get data from keyboard
 *
 * The kbd_proc_data() function gets data from the keyboard.
 * If we finish a character, return it, else 0. And return -1 if no data.
 **/
static int kbd_proc_data()
{
    int c;
    uint8_t data;
    static uint32_t shift;

    if ((inb(KBSTATP) & KBS_DIB) == 0) {
        return -1;
    }

    data = inb(KBDATAP);

    if (data == 0xE0) {
        // E0 escape character
        shift |= E0ESC;
        return 0;
    } else if (data & 0x80) {
        // key released
        data = (shift & E0ESC ? data : data & 0x7F);
        shift &= ~(shiftcode[data] | E0ESC);
        return 0;
    } else if (shift & E0ESC) {
        // last character was an E0 escape; or with 0x80
        data |= 0x80;
        shift &= ~E0ESC;
    }

    shift |= shiftcode[data];
    shift ^= togglecode[data];

    c = charcode[shift & (CTRL | SHIFT)][data];
    if (shift & CAPSLOCK) {
        if ('a' <= c && c <= 'z')
            c += 'A' - 'a';
        else if ('A' <= c && c <= 'Z')
            c += 'a' - 'A';
    }

    // process special keys
    // Ctrl-Alt-Del: reboot
    if (!(~shift & (CTRL | ALT)) && c == KEY_DEL) {
        trace("rebooting...");
        outb(0x92, 0x3); // courtesy of Chris Frost
    }
    
    return c;
}

/* kbd_intr - try to feed input characters from keyboard */
static void kbd_intr()
{
    cons_intr(kbd_proc_data);
}

static void kbd_init(void)
{
    // drain the kbd buffer
    kbd_intr();
    pic_enable(IRQ_KBD);
}

/* cons_init - initializes the console devices */
void cons_init()
{
    cga_init();
    serial_init();
    kbd_init();
    
    cons.wpos = cons.rpos = 0;
    
    if (!serial_exists) {
        trace("no serial port found");
    }
}

/* cons_putc - print a single character @c to console devices */
void cons_putc(int c)
{
    no_intr_block({
        lpt_putc(c);
        cga_putc(c);
        serial_putc(c);
    });
}

/**
 * cons_getc - return the next input character from console,
 * or 0 if none waiting.
 **/
int cons_getc()
{
    int c = 0;

    no_intr_block({
        // poll for any pending input characters,
        // so that this function works even when interrupts are disabled
        // (e.g., when called from the kernel monitor).
        serial_intr();
        kbd_intr();

        // dequeue the next character from the input buffer.
        if (cons.rpos != cons.wpos) {
            c = cons.buf[cons.rpos++];
            
            if (cons.rpos == CONSBUFSIZE) {
                cons.rpos = 0;
            }
        }
    });
    
    return c;
}
