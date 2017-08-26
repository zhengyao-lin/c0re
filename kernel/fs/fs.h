#ifndef _KERNEL_FS_FS_H_
#define _KERNEL_FS_FS_H_

#include "mem/mmu.h"

#define FS_SECTOR_SIZE            512
#define FS_PAGE_NSECTOR           (PAGE_SIZE / FS_SECTOR_SIZE)

#define FS_SWAP_DEV_NO            1

#endif
