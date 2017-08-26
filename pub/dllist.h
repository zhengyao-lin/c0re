#ifndef _PUB_DLLIST_H_
#define _PUB_DLLIST_H_

#include "pub/com.h"

/**
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when manipulating
 * whole lists rather than single entries, as sometimes we already know
 * the next/prev entries and we can generate better code by using them
 * directly rather than using the generic single-entry routines.
 **/

typedef struct dllist_t_tag {
    struct dllist_t_tag *prev, *next;
} dllist_t;

C0RE_INLINE void dllist_init(dllist_t *elem);
C0RE_INLINE void dllist_add(dllist_t *head, dllist_t *elem);
C0RE_INLINE void dllist_add_before(dllist_t *head, dllist_t *elem);
C0RE_INLINE void dllist_add_after(dllist_t *head, dllist_t *elem);

C0RE_INLINE void dllist_del(dllist_t *head);
C0RE_INLINE void dllist_del_init(dllist_t *head);
C0RE_INLINE bool dllist_empty(dllist_t *list);

C0RE_INLINE dllist_t *dllist_next(dllist_t *head);
C0RE_INLINE dllist_t *dllist_prev(dllist_t *head);

C0RE_INLINE void _dllist_add(dllist_t *elem, dllist_t *prev, dllist_t *next);
C0RE_INLINE void _dllist_del(dllist_t *prev, dllist_t *next);

/**
 * dllist_init - initialize a new entry
 * @elem:        new entry to be initialized
 **/
C0RE_INLINE void dllist_init(dllist_t *elem)
{
    elem->prev = elem->next = elem;
}

/**
 * dllist_add - add a new entry
 * @head:        list head to add after
 * @elem:        new entry to be added
 *
 * Insert the new element @elem *after* the element @head which
 * is already in the list.
 **/
C0RE_INLINE void dllist_add(dllist_t *head, dllist_t *elem)
{
    dllist_add_after(head, elem);
}

/**
 * list_add_before - add a new entry
 * @head:        list head to add before
 * @elem:        new entry to be added
 *
 * Insert the new element @elem *before* the element @head which
 * is already in the list.
 **/
C0RE_INLINE void dllist_add_before(dllist_t *head, dllist_t *elem)
{
    _dllist_add(elem, head->prev, head);
}

/**
 * list_add_after - add a new entry
 * @head:        list head to add after
 * @elem:        new entry to be added
 *
 * Insert the new element @elem *after* the element @head which
 * is already in the list.
 **/
C0RE_INLINE void dllist_add_after(dllist_t *head, dllist_t *elem)
{
    _dllist_add(elem, head, head->next);
}

/**
 * dllist_del - deletes entry from list
 * @head:        the element to delete from the list
 *
 * Note: list_empty() on @head does not return true after this, the entry is
 * in an undefined state.
 **/
C0RE_INLINE void dllist_del(dllist_t *head)
{
    _dllist_del(head->prev, head->next);
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @head:        the element to delete from the list.
 *
 * Note: list_empty() on @head returns true after this.
 **/
C0RE_INLINE void dllist_del_init(dllist_t *head)
{
    dllist_del(head);
    dllist_init(head);
}

/**
 * list_empty - tests whether a list is empty
 * @list:       the list to test.
 **/
C0RE_INLINE bool dllist_empty(dllist_t *list)
{
    return list->next == list;
}

/* *
 * list_next - get the next entry
 * @head:        the list head
 **/
C0RE_INLINE dllist_t *dllist_next(dllist_t *head)
{
    return head->next;
}

/**
 * list_prev - get the previous entry
 * @head:        the list head
 **/
C0RE_INLINE dllist_t *dllist_prev(dllist_t *head)
{
    return head->prev;
}

/**
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 **/
C0RE_INLINE void _dllist_add(dllist_t *elem, dllist_t *prev, dllist_t *next)
{
    prev->next = next->prev = elem;
    elem->next = next;
    elem->prev = prev;
}

/**
 * Delete a list entry by making the prev/next entries point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 **/
C0RE_INLINE void _dllist_del(dllist_t *prev, dllist_t *next)
{
    prev->next = next;
    next->prev = prev;
}

#endif
