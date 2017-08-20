#include "pub/com.h"

#include "mem/ffit.h"
#include "mem/mmu.h"
#include "mem/pmm.h"

#include "lib/debug.h"

/* first-fir page allocator */

extern const page_allocator_t page_ffit_allocator;

/* free_area_t - maintains a doubly linked list to record free (unused) pages */
static struct {
    page_t *freed;              // free page header
    unsigned int nfree;        // # of free pages in this free list(!!NOTE NOT # of free blocks)
} free_area;

#define _FREED (free_area.freed)
#define _NFREE (free_area.nfree)

static void _ffit_dllist_append(page_t *append)
{
    if (_FREED) {
        append->next = _FREED->next;
        append->prev = _FREED;
        
        if (_FREED->next)
            _FREED->next->prev = append;
            
        _FREED->next = append;
    } else {
        _FREED = append;
        append->next = append->prev = NULL;
    }
}

static void _ffit_dllist_remove(page_t *page)
{
    // assert remove is in the dllist
    assert(page);
    
    if (page == _FREED) {
        _FREED = page->next;
    } else {
        if (page->next)
            page->next->prev = page->prev;
            
        if (page->prev)
            page->prev->next = page->next;
    }
}

// init an empty free area
static void ffit_init()
{
    _FREED = NULL;
    _NFREE = 0;
}

// add new mem block
static void ffit_addMem(page_t *base, size_t n)
{
    assert(n);
    
    page_t *p, *end = base + n;
    
    for (p = base; p != end; p++) {
        assert(page_isReserved(p)); // ??
        
        page_clearRef(p);
        page_clearFlags(p);
        p->nfree = 0;
    }
    
    base->nfree += n;
    page_setFree(base);
    
    _NFREE += n;
    _ffit_dllist_append(base);
}

// alloc using first-fit algorithm
static page_t *ffit_alloc(size_t n)
{
    assert(n);
    
    // no enough page
    if (n > _NFREE) return NULL;
    
    page_t *found = NULL;
    page_t *cur;
    
    for (cur = _FREED; cur; cur = cur->next) {
        if (cur->nfree >= n) { // first fit found
            found = cur;
            break;
        }
    }
    
    if (found) {
        _ffit_dllist_remove(found);
        
        if (found->nfree > n) { // cut the block
            page_t *rest = found + n;
            page_setFree(rest); // TODO: possible bug in the original code
            rest->nfree = rest->nfree - n;
            _ffit_dllist_append(rest);
        }
        
        _NFREE -= n;
        found->nfree = n; // NOTE: a little alteration
        
        page_resetFree(found);
    }
    
    return found;
}

// page must be allocated
static void ffit_free(page_t *page /* , size_t n (n is loged in line 103) */)
{
    assert(page && page->nfree);
    
    size_t n = page->nfree;
    page_t *p, *end = page + n;
    
    for (p = page; p != end; p++) {
        assert(!page_isReserved(p) && !page_isFree(p));
        
        page_clearFlags(p);
        page_clearRef(p);
    }
    
    // page->nfree = n;
    page_setFree(page);
    
    // merge consecutive block
    
    for (p = _FREED; p; p = p->next) {
        if (page + page->nfree == p) { // page -- p
            page->nfree += p->nfree;
            page_resetFree(p);
            _ffit_dllist_remove(p);
        } else if (p + p->nfree == page) { // p -- page
            p->nfree += page->nfree;
            page_resetFree(page);
            page = p;
            _ffit_dllist_remove(p); // so that you don't re-add it below
        }
    }
    
    _NFREE += n;
    _ffit_dllist_append(page);
}

static size_t ffit_nfree()
{
    return _NFREE;
}

const page_allocator_t page_ffit_allocator = {
    .name = "ffit",
    .init = ffit_init,
    .addMem = ffit_addMem,
    
    .alloc = ffit_alloc,
    .free = ffit_free,
    
    .nfree = ffit_nfree
};
