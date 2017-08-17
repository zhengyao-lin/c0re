#ifndef _BOOT_BOOTASM_H_
#define _BOOT_BOOTASM_H_

/* Assembler macros to create x86 segments */

/* Normal segment */
#define SEG_NULLASM \
    .short 0, 0;    \
    .byte 0, 0, 0, 0

#define SEG_ASM(type, base, lim)                                          \
                    /* 12 means dividing by 4096(4k, granularity) */      \
    .short  (((lim) >> 12) & 0xffff),           ((base) & 0xffff);        \
                                              /* 0x90 for P and S bits */ \
    .byte   (((base) >> 16) & 0xff),            (0x90 | (type)),          \
            (0xc0 | (((lim) >> 28) & 0xf)),     (((base) >> 24) & 0xff)
          /* 0xc0 here sets the G(granularity, to 4k) and D(???) bits */


/* Application segment type bits */
#define STA_X       0x8     // Executable segment
#define STA_E       0x4     // Expand down (non-executable segments)
#define STA_C       0x4     // Conforming code segment (executable only)
#define STA_W       0x2     // Writeable (non-executable segments)
#define STA_R       0x2     // Readable (executable segments)
#define STA_A       0x1     // Accessed

#endif // _BOOT_BOOTASM_H_
