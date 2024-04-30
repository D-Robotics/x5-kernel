/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_LIST_H__
#define __OSAL_LIST_H__

#include <linux/list.h>

typedef struct list_head osal_list_head_t;

/**
 * @brief init list
 *
 * @param[in] list: list instance
 */
static inline void osal_list_head_init(osal_list_head_t *list)
{
	INIT_LIST_HEAD((struct list_head *)list);
}

/**
 * @brief add a new entry, insert a new entry after the specified head.
 *
 * @param[in] new:  new entry to be added
 * @param[in] head: list head to add to
 */
 static inline void osal_list_add(osal_list_head_t *new, osal_list_head_t *head)
 {
	list_add((struct list_head *)new, (struct list_head *)head);
 }

/**
 * @brief add a new entry, insert a new entry before the specified head.
 *
 * @param[in] new:  new entry to be added
 * @param[in] head: list head to add to
 */
static inline void osal_list_add_tail(osal_list_head_t *new, osal_list_head_t *head)
{
	list_add_tail((struct list_head *)new, (struct list_head *)head);
}

/**
 * @brief delete entry from list
 *
 * @param[in] entry: the element to delete from the list
 */
static inline void osal_list_del(osal_list_head_t *entry)
{
	list_del((struct list_head *)entry);
}

/**
 * @brief tests whether a list is empty
 *
 * @param[in] head the list to test
 */
static inline int32_t osal_list_empty(osal_list_head_t *head)
{
	return list_empty((struct list_head *)head);
}

#if 0
#define container_of(ptr, type, member) ({ \
        void *__mptr = (void *)(ptr); \
        ((type *)(__mptr - offsetof(type, member))); })
#endif

/**
 * @brief osal_list_entry -  get the struct for this entry
 * @param[in] ptr:    the &struct list_head pointer.
 * @param[in] type:   the type of the struct this is embedded in.
 * @param[in] member: the name of the list_head within the struct.
 */
#define osal_list_entry(ptr, type, member) \
        container_of(ptr, type, member)

/**
 * @brief osal_list_first_entry - get the first element from a list
 *        Note, that list is expected to be not empty.
 *
 * @param[in] ptr:    the list head to take the element from.
 * @param[in] type:   the type of the struct this is embedded in.
 * @param[in] member: the name of the list_head within the struct.
 *
 */
#define osal_list_first_entry(ptr, type, member) \
        osal_list_entry((ptr)->next, type, member)

/**
 * @brief osal_list_last_entry - get the last element from a list
 * @param[in] ptr:    the list head to take the element from.
 * @param[in] type:   the type of the struct this is embedded in.
 * @param[in] member: the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define osal_list_last_entry(ptr, type, member) \
    osal_list_entry((ptr)->prev, type, member)

/**
 * @brief osal_list_prev_entry - get the prev element in list
 * @param[in] pos:    the type * to cursor
 * @param[in] member: the name of the list_head within the struct.
 */
#define osal_list_prev_entry(pos, member) \
    osal_list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * @brief osal_list_next_entry - get the next element in list
 * @param[in] pos:    the type * to cursor
 * @param[in] member: the name of the list_head within the struct.
 */
#define osal_list_next_entry(pos, member) \
        osal_list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * @brief osal_list_for_each    -   iterate over a list
 * @param[in] pos:    the &struct list_head to use as a loop cursor.
 * @param[in] head:   the head for your list.
 */
#define osal_list_for_each(pos, head) \
        for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * @brief osal_list_for_each_safe - iterate over a list safe against removal of list entry
 * @param[in] pos:    the &struct list_head to use as a loop cursor.
 * @param[in] n:      another &struct list_head to use as temporary storage
 * @param[in] head:   the head for your list.
 */
#define osal_list_for_each_safe(pos, n, head) \
		for (pos = (head)->next, n = pos->next; pos != (head); \
				pos = n, n = pos->next)

/**
 * @brief osal_list_for_each_prev   -   iterate over a list backwards
 * @param[in] pos:    the &struct list_head to use as a loop cursor.
 * @param[in] head:   the head for your list.
 */
#define osal_list_for_each_prev(pos, head) \
        for (pos = (head)->prev; pos != (head); pos = pos->prev)

/**
 * @brief osal_list_for_each_prev_safe - iterate over a list backwards safe against removal of list entry
 * @param[in] pos:    the &struct list_head to use as a loop cursor.
 * @param[in] n:      another &struct list_head to use as temporary storage
 * @param[in] head:   the head for your list.
 */
#define osal_list_for_each_prev_safe(pos, n, head) \
        for (pos = (head)->prev, n = pos->prev; pos != (head); \
                pos = n, n = pos->prev)

/**
 * @brief osal_list_for_each_entry  -   iterate over list of given type
 * @param[in] pos:    the type * to use as a loop cursor.
 * @param[in] head:   the head for your list.
 * @param[in] member: the name of the list_head within the struct.
 */
#define osal_list_for_each_entry(pos, head, member) \
        for (pos = osal_list_first_entry(head, typeof(*pos), member); \
             &pos->member != (head); \
             pos = osal_list_next_entry(pos, member))

/**
 * @brief osal_list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @param[in] pos:    the type * to use as a loop cursor.
 * @param[in] n:      another type * to use as temporary storage
 * @param[in] head:   the head for your list.
 * @param[in] member: the name of the list_head within the struct.
 */
#define osal_list_for_each_entry_safe(pos, n, head, member) \
        for (pos = osal_list_first_entry(head, typeof(*pos), member), \
                n = osal_list_next_entry(pos, member); \
             &pos->member != (head); \
             pos = n, n = osal_list_next_entry(n, member))

/**
 * @brief osal_list_for_each_entry_reverse - iterate backwards over list of given type.
 * @param[in] pos:    the type * to use as a loop cursor.
 * @param[in] head:   the head for your list.
 * @param[in] member: the name of the list_head within the struct.
 */
#define osal_list_for_each_entry_reverse(pos, head, member) \
        for (pos = osal_list_last_entry(head, typeof(*pos), member); \
             &pos->member != (head); \
             pos = osal_list_prev_entry(pos, member))

/**
 * @brief osal_list_for_each_entry_safe_reverse - iterate backwards over list safe against removal
 *        Iterate backwards over list of given type, safe against removal of list entry.
 * @param[in] pos:    the type * to use as a loop cursor.
 * @param[in] n:      another type * to use as temporary storage
 * @param[in] head:   the head for your list.
 * @param[in] member: the name of the list_head within the struct.
 *

 */
#define osal_list_for_each_entry_safe_reverse(pos, n, head, member) \
        for (pos = osal_list_last_entry(head, typeof(*pos), member), \
                n = osal_list_prev_entry(pos, member); \
             &pos->member != (head); \
             pos = n, n = osal_list_prev_entry(n, member))

#endif
