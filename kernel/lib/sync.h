#ifndef _KERNEL_LIB_SYNC_H_
#define _KERNEL_LIB_SYNC_H_

#include "pub/com.h"
#include "pub/x86.h"

#include "intr/trap.h"
#include "mem/mmu.h"

C0RE_INLINE bool _intr_save()
{
    if (read_eflags() & EFLAG_IF) {
        intr_disable();
        return 1;
    }
    
    return 0;
}

C0RE_INLINE void _intr_restore(bool flag)
{
    if (flag) {
        intr_enable();
    }
}

#define intr_save(x)      ({ x = _intr_save(); })
#define intr_restore(x)   _intr_restore(x)

#define no_intr_block(block) ({ \
        if (_intr_save()) {     \
            block;              \
            intr_restore(true); \
        } else {                \
            block;              \
        }                       \
    })

#endif
