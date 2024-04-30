/*
 * Copyright 2023, Horizon Robotics
 */
#ifndef __OSAL_WAIT_EVENT_H__
#define __OSAL_WAIT_EVENT_H__

#include <linux/wait.h>
#include <linux/jiffies.h>

typedef struct wait_queue_head osal_waitqueue_t;

/**
 * @brief initialize wait queue
 *
 * @param[in] wq: waitqueue instance to be initialized
 */
static inline void osal_waitqueue_init(osal_waitqueue_t *wq)
{
    init_waitqueue_head((struct wait_queue_head *)wq);
}

/**
 * @brief wait for event with condition
 *
 * @param[in]        wq: osal_waitqueue_t, waitqueue instance
 * @param[in] condition: bool expression for waking up
 */
#define osal_wait_event(wq, condition)  \
do {                                    \
    might_sleep();                      \
    if (condition)                      \
        break;                          \
    (void)___wait_event((wq), condition, TASK_UNINTERRUPTIBLE, 1, 0, schedule()); \
} while (0)

/**
 * @brief wait for event with condition with timeout
 *
 * @param[in]         wq: osal_waitqueue_t, waitqueue instance
 * @param[in]  condition: bool expression for waking up
 * @param[in] timeout_ms: uint64_t, timeout time in microseconds
 *
 * @return = 0 when timeout while condition is false
 *         = 1 when timeout while condition is true
 *         > 1 the remaining time in milliseconds
 */
#define osal_wait_event_timeout(wq, condition, timeout_ms)                  \
({                                                                          \
    int64_t __timeout_jfs =  msecs_to_jiffies(timeout_ms);                  \
    int64_t __ret;                                                          \
                                                                            \
    if (__timeout_jfs == 0)                                                 \
        __timeout_jfs = 1;                                                  \
    __ret = __timeout_jfs;                                                  \
                                                                            \
    might_sleep();                                                          \
    if (!___wait_cond_timeout(condition)) {                                 \
        __ret = ___wait_event((wq), ___wait_cond_timeout(condition),        \
                TASK_UNINTERRUPTIBLE, 1, __timeout_jfs,                     \
                __ret = schedule_timeout(__ret));                           \
    }                                                                       \
    (__ret > 1) ? jiffies_to_msecs(__ret) : __ret;                          \
})

/**
 * @brief wait for event and is interrruptibe by signal
 *
 * @param[in]         wq: osal_waitqueue_t, waitqueue instance
 * @param[in]  condition: bool expression for waking up
 *
 * @return = 0 when timer has expired,
 *         < 0 errno
 */
#define osal_wait_event_interruptible(wq, condition)                    \
    wait_event_interruptible_exclusive((wq), (condition))

/**
 * @brief wait for event with condition with timeout and is interruptible
 *
 * @param[in]         wq: osal_waitqueue_t, waitqueue instance
 * @param[in]  condition: bool expression for waking up
 * @param[in] timeout_ms: timeout time in microseconds
 *
 * @return = 0 when timeout while condition is false
 *         = 1 when timeout while condition is true
 *         > 1 the remaining time in milliseconds while condition is true
 *         < 0 errno if interrupted while condition is false
 */
#define osal_wait_event_interruptible_timeout(wq, condition, timeout_ms)    \
({                                                                          \
    int64_t __timeout_jfs =  msecs_to_jiffies(timeout_ms);                  \
    int64_t __ret;                                                          \
                                                                            \
    if (__timeout_jfs == 0)                                                 \
        __timeout_jfs = 1;                                                  \
    __ret = __timeout_jfs;                                                  \
                                                                            \
    might_sleep();                                                          \
    if (!___wait_cond_timeout(condition)) {                                 \
        __ret = ___wait_event((wq), ___wait_cond_timeout(condition),        \
                TASK_INTERRUPTIBLE, 1, __timeout_jfs,                       \
                __ret = schedule_timeout(__ret));                           \
    }                                                                       \
    (__ret > 1) ? jiffies_to_msecs(__ret) : __ret;                          \
})

/**
 * @brief wake up a process in  waitqueue
 *
 * @param[in] wq: waitqueue instance
 */
static inline void osal_wake_up(osal_waitqueue_t *wq)
{
    wake_up((struct wait_queue_head *)wq);
}

/**
 * @brief wake up all processes in waitqueue
 *
 * @param[in] wq: waitqueue instance
 */
static inline void osal_wake_up_all(osal_waitqueue_t *wq)
{
    wake_up_all((struct wait_queue_head *)wq);
}

#endif
