#ifndef _KERNEL_FS_SWAPFS_H_
#define _KERNEL_FS_SWAPFS_H_

#include "mem/mmu.h"
#include "mem/swap.h"

// return max swap offset
size_t swapfs_init();
int swapfs_read(swap_entry_t entry, page_t *page);
int swapfs_write(swap_entry_t entry, page_t *page);

#endif
