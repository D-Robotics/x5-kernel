/*
 * Copyright (c) 2020, Arm Technology (China) Co., Ltd.
 * All rights reserved.
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#ifndef __SQLIST_H__
#define __SQLIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#ifndef offsetof
#define offsetof(TYPE, MEMBER)  ((unsigned long)&((TYPE *)0)->MEMBER)
#endif

#ifndef ITERATOR_TO_CONTAINER

#define ITERATOR_TO_CONTAINER(it, type, member) ({                      \
    void *__cptr = (void *)(it);                                        \
    ((type *)((unsigned long)__cptr - offsetof(type, member))); })

#endif

#ifndef NULL
#define NULL   ((void*)0)
#endif

typedef struct __sqlist {
    struct __sqlist *next;
    struct __sqlist *prev;
} sqlist_t;


/**
 * \brief                   Iterate on a sqlist.
 * \param       h           A pointer to the head of sqlist_t to iterate.
 * \param       n           A sqlist_t node point.
 * \return      none
 */
#define SQLIST_FOR_EACH_NODE(h, n)                                      \
    for ((n) = (h)->next; (n) != (h); (n) = (n)->next)


/**
 * \brief                   Safely iterate on a sqlist
 * \param       h           A pointer to the head of sqlist_t to iterate.
 * \param       n           A sqlist_t node point.
 * \param       t           Temp sqlist_t node point for iterate, safely.
 */
#define SQLIST_FOR_EACH_NODE_SAFE(h, n, t)                              \
    for ((n) = (h)->next, (t) = (n)->next;                              \
            (n) != (h); (n) = (t), (t) = (n)->next)

/*
 * \brief                   Resolve the container of a sqlist node
 * \param       n           A pointer on a sqlist_t to get its container
 * \param       c           Container struct type pointer
 * \param       m           Member name of sqlist_t within the container struct
 */
#define SQLIST_CONTAINER(n, c, m)                                       \
    ((n) ? ITERATOR_TO_CONTAINER(n, __typeof__(*(c)), m) : NULL)
/*
 * \brief                   Peek container of the list head
 *
 * \param       h           A pointer to the head of a sqlist_t
 * \param       c           Container struct type pointer
 * \param       m           Member name of sqlist_t within the container struct
 */
#define SQLIST_PEEK_HEAD_CONTAINER(h, c, m)                             \
    SQLIST_CONTAINER(sqlist_peek_head(h), c, m)

/*
 * \brief                   Peek the next container
 *
 * \param       h           Head or node of sqlist_t to peek
 * \param       c           Container struct type pointer
 * \param       m           Member name of sqlist_t within the container struct
 */
#define SQLIST_PEEK_NEXT_CONTAINER(h, c, m)                             \
    ((c) ? SQLIST_CONTAINER(sqlist_peek_next(h, &((c)->m)),             \
                                                c, m) : NULL)

/**
 * \brief                   Iterate on a list under a container.
 *
 * \param       h           Head of sqlist_t to iterate.
 * \param       c           Container struct type pointer
 * \param       m           Member name of sqlist_t within the container struct
 */
#define SQLIST_FOR_EACH_CONTAINER(h, c, m)                              \
    for ((c) = SQLIST_PEEK_HEAD_CONTAINER(h, c, m); (c);                \
                (c) = SQLIST_PEEK_NEXT_CONTAINER(h, c, m))

/**
 * \brief                   Iterate on a list under a container, safely.
 *
 * \param       h           Head of sqlist_t to iterate.
 * \param       c           Container pointer
 * \param       t           Temp container point for iterate, safely.
 * \param       m           Member name of sqlist_t within the container struct
 */
#define SQLIST_FOR_EACH_CONTAINER_SAFE(h, c, t, m)                      \
    for ((c) = SQLIST_PEEK_HEAD_CONTAINER(h, c, m),                     \
         (t) = SQLIST_PEEK_NEXT_CONTAINER(h, c, m); (c);                \
         (c) = (t),                                                     \
         (t) = SQLIST_PEEK_NEXT_CONTAINER(h, c, m))



#define SQLIST_INIT(ptr_to_list) {(ptr_to_list), (ptr_to_list)}

/**
 * \brief                   initialize list
 *
 * \param       list        sqlist_t instance
 * \return      N/A
 */

static inline void sqlist_init(sqlist_t *list)
{
    list->next = (sqlist_t *)list;
    list->prev = (sqlist_t *)list;
}


/**
 * \brief                   tests whether a list is empty
 *
 * \param       list        sqlist_t instance
 * \return      1           empty
 * \return      0           otherwise
 */

static inline int sqlist_is_empty(sqlist_t *list)
{
    return list->next == list;
}

/**
 * \brief                   Peek the item of the sqlist head.
 *
 * \param       list        sqlist head
 * \return      pointer     the head element
 * \return      NULL        list is empty
 */

static inline sqlist_t *sqlist_peek_head(sqlist_t *list)
{
    return sqlist_is_empty(list) ? NULL : list->next;
}

/**
 * \brief                   Get the next node after specified node.
 * \Warning                 This API assume node is not NULL
 *
 * \param       list        sqlist head
 * \param       node        The node which to get the next item in the list.
 * \return      pointer     Next node from specified node.
 * \return      NULL        Specified node is the tail of the list.
 */

static inline sqlist_t *sqlist_peek_next_no_check(sqlist_t *list,
                                                  sqlist_t *node)
{
    return (node == list->prev) ? NULL : node->next;
}

/**
 * \brief                   Get the next node after specified node.
 *
 * \param       list        sqlist head
 * \param       node        The node which to get the next item in the list.
 * \return      pointer     Next node from specified node.
 * \return      NULL        Specified node is the tail of the list.
 */
static inline sqlist_t *sqlist_peek_next(sqlist_t *list,
                                         sqlist_t *node)
{
    return node ? sqlist_peek_next_no_check(list, node) : NULL;
}

/**
 * \brief                   Peek the item of tail of the sqlist.
 *
 * \param       list        sqlist head
 * \return      pointer     the tail element
 * \return      NULL        list is empty
 */

static inline sqlist_t *sqlist_peek_tail(sqlist_t *list)
{
    return sqlist_is_empty(list) ? NULL : list->prev;
}

/**
 * \brief                   Add node to tail of list.
 *
 * \param       list        sqlist head
 * \param       node        node to be insert at the tail of the list.
 * \return      N/A
 */
static inline void sqlist_insert_tail(sqlist_t *list, sqlist_t *node)
{
    node->next = list;
    node->prev = list->prev;

    list->prev->next = node;
    list->prev = node;
}

/**
 * \brief                   Use list emulate the enqueue operation.
 *
 * \param       list        sqlist head
 * \param       node        node need to be enqueued(Add to tail).
 * \return      N/A
 */
static inline void sqlist_enqueue(sqlist_t *list, sqlist_t *node)
{
    sqlist_insert_tail(list, node);
}

/**
 * \brief                   Use list emulate stack push operation.
 *
 * \param       list        sqlist head
 * \param       node        node need to be pushed(Add to tail).
 * \return      N/A
 */
static inline void sqlist_push(sqlist_t *list, sqlist_t *node)
{
    sqlist_insert_tail(list, node);
}

/**
 * \brief                   Add node to the head of list.
 *
 * \param       list        sqlist head
 * \param       node        node to be insert at the head of the list.
 * \return      N/A
 */

static inline void sqlist_insert_head(sqlist_t *list, sqlist_t *node)
{
    node->next = list->next;
    node->prev = list;

    list->next->prev = node;
    list->next = node;
}

/**
 * \brief                   Insert node after specified node.
 *
 * \param       list        sqlist head
 * \param       at          The insert location node.
 * \param       node        The node to be insert after 'at' location.
 * \return      N/A
 */
static inline void sqlist_insert_after(sqlist_t *list,
                                       sqlist_t *at, sqlist_t *node)
{
        if (!at) {
            sqlist_insert_head(list, node);
            return;
        }

        node->next = at->next;
        node->prev = at;
        at->next->prev = node;
        at->next = node;
        return;
}

/**
 * \brief                   Insert node before specified node.
 *
 * \param       list        sqlist head
 * \param       at          The insert location node.
 * \param       node        The node to be insert before 'at' location.
 * \return      N/A
 */
static inline void sqlist_insert_before(sqlist_t *list,
                                        sqlist_t *at, sqlist_t *node)
{
    if (!at) {
        sqlist_insert_tail(list, node);
        return;
    }

    node->prev = at->prev;
    node->next = at;
    at->prev->next = node;
    at->prev = node;
    return;
}

/**
 * \brief                   Remove specified node
 *
 * \param       node        node to remove
 * \return      N/A
 */
static inline void sqlist_remove(sqlist_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

/**
 * \brief                   Get and remove first node in a list
 *
 * \param       list        list head.
 * \return      pointer     first node in the list
 * \return      NULL        list is empty
 */
static inline sqlist_t *sqlist_get(sqlist_t *list)
{
    sqlist_t *node;

    if (sqlist_is_empty(list)) {
        return NULL;
    }

    node = list->next;
    sqlist_remove(node);
    return node;
}

/**
 * \brief                   Get and remove tail node in a list
 *
 * \param       list        list head.
 * \return      pointer     tail node in the list
 * \return      NULL        list is empty
 */
static inline sqlist_t *sqlist_get_tail(sqlist_t *list)
{
    sqlist_t *node;

    if (sqlist_is_empty(list)) {
        return NULL;
    }

    node = list->prev;
    sqlist_remove(node);
    return node;
}

/**
 * \brief                   Use list emulate the dequeue operation.
 *
 * \param       list        sqlist head
 * \return      pointer     first item from queue head
 * \return      NULL        queue is empty
 */
static inline sqlist_t *sqlist_dequeue(sqlist_t *list)
{
    return sqlist_get(list);
}

/**
 * \brief                   Use list emulate stack pop operation.
 *
 * \param       list        sqlist head
 * \return      pointer     top item in stack
 * \return      NULL        stack is empty
 */
static inline sqlist_t *sqlist_pop(sqlist_t *list)
{
    return sqlist_get_tail(list);
}

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __SQLIST_H__ */
