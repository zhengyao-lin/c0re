#include "pub/com.h"
#include "pub/x86.h"
#include "pub/string.h"

#include "driver/console.h"

/* stupid I/O delay routine necessitated by historical PC design flaws */
static void delay()
{
    inb(0x84);
    inb(0x84);
    inb(0x84);
    inb(0x84);
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

// 显示器初始化，CGA 是 Color Graphics Adapter 的缩写
// CGA显存按照下面的方式映射：
//   -- 0xB0000 - 0xB7777 单色字符模式
//   -- 0xB8000 - 0xBFFFF 彩色字符模式及 CGA 兼容图形模式
// 6845芯片是IBM PC中的视频控制器
// CPU通过IO地址0x3B4-0x3B5来驱动6845控制单色显示，通过IO地址0x3D4-0x3D5来控制彩色显示。
//    -- 数据寄存器 映射 到 端口 0x3D5或0x3B5 
//    -- 索引寄存器 0x3D4或0x3B4,决定在数据寄存器中的数据表示什么。

/* TEXT-mode CGA/VGA display output */
static void cga_init(void)
{
    volatile uint16_t *cp = (uint16_t *)CGA_BUF;   // CGA_BUF: 0xB8000 (base address of video memory)
    
    uint16_t was = *cp;                            // save current value
    *cp = (uint16_t) 0xA55A;                       // write a random value and check if the value read is the same
    
    if (*cp != 0xA55A) {                           // monocolor
        cp = (uint16_t*)MONO_BUF;
        addr_6845 = MONO_BASE;
    } else {
        *cp = was;                                 // restore value
        addr_6845 = CGA_BASE;
    }

    // extract cursor location
    uint32_t pos;
    outb(addr_6845, 14);                                        
    pos = inb(addr_6845 + 1) << 8; // cursor position(high)
    outb(addr_6845, 15);
    pos |= inb(addr_6845 + 1); // cursor position(low)

    crt_buf = (uint16_t*)cp; // start address of cga video memory
    crt_pos = pos; // current cursor position
}

// static bool serial_exists = 0;
// 
// static void serial_init()
// {
//     // Turn off the FIFO
//     outb(COM1 + COM_FCR, 0);
// 
//     // Set speed; requires DLAB latch
//     outb(COM1 + COM_LCR, COM_LCR_DLAB);
//     outb(COM1 + COM_DLL, (uint8_t) (115200 / 9600));
//     outb(COM1 + COM_DLM, 0);
// 
//     // 8 data bits, 1 stop bit, parity off; turn off DLAB latch
//     outb(COM1 + COM_LCR, COM_LCR_WLEN8 & ~COM_LCR_DLAB);
// 
//     // No modem controls
//     outb(COM1 + COM_MCR, 0);
//     // Enable rcv interrupts
//     outb(COM1 + COM_IER, COM_IER_RDI);
// 
//     // Clear any preexisting overrun indications and interrupts
//     // Serial port doesn't exist if COM_LSR returns 0xFF
//     serial_exists = (inb(COM1 + COM_LSR) != 0xFF);
//     (void) inb(COM1+COM_IIR);
//     (void) inb(COM1+COM_RX);
// 
//     // if (serial_exists) {
//     //     pic_enable(IRQ_COM1);
//     // }
// }

static void lpt_putc_sub(int c)
{
    int i;
    
    for (i = 0; !(inb(LPTPORT + 1) & 0x80) && i < 12800; i ++) {
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
        case '\b':
            if (crt_pos > 0) {
                crt_pos--;
                crt_buf[crt_pos] = (c & ~0xff) | ' ';
            }
            break;
        
        case '\n':
            crt_pos += CRT_COLS;
        
        case '\r':
            crt_pos -= (crt_pos % CRT_COLS);
            break;
            
        default:
            crt_buf[crt_pos++] = c;     // write the character
            break;
    }

    // what is the purpose of this?
    // -- scroll up a line?
    if (crt_pos >= CRT_SIZE) {
        int i;
        memmove(crt_buf, crt_buf + CRT_COLS,
                (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
                
        for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++) {
            crt_buf[i] = 0x0700 | ' ';
        }
    
        crt_pos -= CRT_COLS;
    }

    // move the cursor
    outb(addr_6845, 14);
    outb(addr_6845 + 1, crt_pos >> 8);
    outb(addr_6845, 15);
    outb(addr_6845 + 1, crt_pos);
}

// static void serial_putc_sub(int c)
// {
//     int i;
//     for (i = 0; !(inb(COM1 + COM_LSR) & COM_LSR_TXRDY) && i < 12800; i ++) {
//         delay();
//     }
//     outb(COM1 + COM_TX, c);
// }
// 
// /* serial_putc - print character to serial port */
// static void serial_putc(int c)
// {
//     if (c != '\b') {
//         serial_putc_sub(c);
//     }
//     else {
//         serial_putc_sub('\b');
//         serial_putc_sub(' ');
//         serial_putc_sub('\b');
//     }
// }

/* cons_init - initializes the console devices */
void kinit(void)
{
    cga_init();
    // serial_init();
    // if (!serial_exists) {
    //     cprintf("serial port does not exist!!\n");
    // }
}

/* cons_putc - print a single character @c to console devices */
void kputc(int c)
{
    lpt_putc(c);
    cga_putc(c);
    // serial_putc(c);
}
