#include "mem/mmu.h"

#include "driver/ide.h"

#include "fs/fs.h"
#include "fs/swapfs.h"

#include "mem/swap.h"

size_t swapfs_init()
{
    assert((PAGE_SIZE % FS_SECTOR_SIZE) == 0);
    
    if (!ide_device_valid(FS_SWAP_DEV_NO)) {
        trace("swap fs not available");
        return 0;
    }
                                             // NOTE: how many sectors per page
    return ide_device_size(FS_SWAP_DEV_NO) / (PAGE_SIZE / FS_SECTOR_SIZE);
           // NOTE: maximum number of pages in the device
}

int swapfs_read(swap_entry_t entry, page_t *page)
{
    return ide_read_secs(FS_SWAP_DEV_NO, swap_getOffset(entry) * FS_PAGE_NSECTOR,
                         page2kva(page), FS_PAGE_NSECTOR);
}

int swapfs_write(swap_entry_t entry, page_t *page)
{
    return ide_write_secs(FS_SWAP_DEV_NO, swap_getOffset(entry) * FS_PAGE_NSECTOR,
                          page2kva(page), FS_PAGE_NSECTOR);
}
