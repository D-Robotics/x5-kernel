/*======================================================================================================================
 *  COPYRIGHT NOTICE
 *
 *  Copyright (C) 2023-2025 Horizon Robotics, Inc.
 *
 *  All rights reserved.
 *
 *  This program contains proprietary information belonging to Horizon Robotics. Passing on and copying of this
 *  document, use and communication of its contents is not permitted without prior written authorization.
========================================================================================================================
 *  Project              : J6
 *  Platform             : CORTEXR
 *  Peripheral           : CamSys
 *  Dependencies         : MCU
 *
 *  SW Version           :
 *  Build Version        :
 *  Author               :
 *  Vendor               : Horizon Robotics
======================================================================================================================*/

/**
 * @file osal_list.h
 *
 * @ASIL{QM}
 * @brief For list compatibility
 */

#ifndef OSAL_LIST_H_
#define OSAL_LIST_H_

struct list_head {
	struct list_head *next, *prev;
};

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &(((TYPE *)0)->MEMBER))
#endif

#ifndef typeof
#define typeof  __typeof__
#endif

#define container_of(ptr, type, member) ({ \
		const typeof(((type *)0)->member) *__mptr = (ptr); \
		(type *)((char *)__mptr - offsetof(type, member)); \
})

typedef struct list_head osal_list_head_t;

/**
 * @brief Initialize a list_head structure
 * @param[in] list: list_head structure to be initialized.
 */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

/**
 * @brief add a new entry
 * @param[in] new: new entry to be added
 * @param[in] prev: list head prev
 * @param[in] next: list head next
 */
static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/**
 * @brief add a new entry to head
 * @param[in] new: new entry to be added
 * @param[in] head: list head to add it after
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

/**
 * @brief add a new entry to tail
 * @param[in] new: new entry to be added
 * @param[in] head: list head to add it before
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

/**
 * @brief Delete a list entry by making the prev/next entries point to each other.
 * @param[in] prev: prev list member
 * @param[in] next: next list member
 */
static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * @brief Delete a list entry and clear the 'prev' pointer.
 * @param[in] entry: the element to delete from the list.
 */
static inline void __list_del_entry(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

/**
 * @brief list_del - deletes entry from list.
 * @param[in] entry: the element to delete from the list.
 */
static inline void list_del(struct list_head *entry)
{
	__list_del_entry(entry);
	entry->next = NULL;
	entry->prev = NULL;
}

/**
 * @brief list_empty - tests whether a list is empty
 * @param[in] head: the list to test.
 */
static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

/**
 * @brief init list
 *
 * @param[in] list list instance
 *
 */
static inline void osal_list_head_init(osal_list_head_t *list)
{
	INIT_LIST_HEAD((struct list_head *)list);
}
/**
 * @brief add a new entry, insert a new entry after the specified head.
 *
 * @param[in] new new entry to be added
 * @param[in] head list head to add it after
 *
 */
 static inline void osal_list_add(osal_list_head_t *new, osal_list_head_t *head)
 {
	list_add((struct list_head *)new, (struct list_head *)head);
 }

/**
 * @brief add a new entry, insert a new entry before the specified head.
 *
 * @param[in] new new entry to be added
 * @param[in] head list head to add it after
 *
 */
static inline void osal_list_add_tail(osal_list_head_t *new, osal_list_head_t *head)
{
	list_add_tail((struct list_head *)new, (struct list_head *)head);
}
/**
 * @brief delete entry form listf
 *
 * @param[in] entry the element to delete from the list
 *
 */
static inline void osal_list_del(osal_list_head_t *entry)
{
	list_del((struct list_head *)entry);
}
/**
 * @brief tests whether a list is empty
 *
 * @param[in] head the list to test
 *
 */
static inline int osal_list_empty(osal_list_head_t *head)
{
	return list_empty((struct list_head *)head);
}

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
 * @brief osal_list_next_entry - get the next element in list
 * @param[in] pos:    the type * to cursor
 * @param[in] member: the name of the list_head within the struct.
 */
#define osal_list_next_entry(pos, member) \
        osal_list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * @brief osal_list_last_entry -  get the last element for this entry
 * @param[in] ptr:    the &struct list_head pointer.
 * @param[in] type:   the type of the struct this is embedded in.
 * @param[in] member: the name of the list_head within the struct.
 */
#define osal_list_last_entry(ptr, type, member) \
        osal_list_entry((ptr)->prev, type, member)

/**
 * @brief osal_list_last_entry -  get the prev element for this entry
 * @param[in] pos:    the type * to cursor
 * @param[in] member: the name of the list_head within the struct.
 */
#define osal_list_prev_entry(pos, member) \
        osal_list_entry((pos)->member.prev, typeof(*(pos)), member)

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

#endif // OSAL_LIST_H_
