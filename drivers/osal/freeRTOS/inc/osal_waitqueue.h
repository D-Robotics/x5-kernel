/*
 * Copyright 2023, Horizon Robotics
 */
#ifndef __OSAL_WAIT_EVENT_H__
#define __OSAL_WAIT_EVENT_H__

#include "FreeRTOS.h"
#include "semphr.h"

typedef SemaphoreHandle_t osal_waitqueue_t;

/**
 * @brief initialize wait queue
 *
 * @param[in] wq: waitqueue instance to be initialized
 */
static inline void osal_waitqueue_init(osal_waitqueue_t *wq)
{
    vSemaphoreCreateBinary(*wq);
}

/**
 * @brief wait for event with condition
 *
 * @param[in]        wq: osal_waitqueue_t, waitqueue instance
 * @param[in] condition: bool expression for waking up
 */
#define osal_wait_event(wq, condition)                                      \
do {                                    \
    if (condition)                      \
        break;                          \
    xSemaphoreTake(wq, portMAX_DELAY);                                      \
} while (0)

/**
 * @brief wait for event with condition with timeout
 *
 * @param[in]         wq: osal_waitqueue_t, waitqueue instance
 * @param[in]  condition: bool expression for waking up
 * @param[in] timeout_ms: uint64_t, timeout time in microseconds
 *
 * @return = 0 when timer has expired,
 *         > 0 the remaining time in microseconds
 *         < 0 errno for other errors, never happens in Linux
 */
#define osal_wait_event_timeout(wq, condition, timeout_ms)                  \
({                                                                          \
    uint64_t __ret = pdMS_TO_TICKS(timeout_ms);                             \
    for (;;) {                                                              \
        if (condition)                                                      \
            break;                                                          \
        __ret = xSemaphoreTake(wq, pdMS_TO_TICKS(timeout_ms));              \
    }                                                                       \
    __ret;                                                                  \
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
#define osal_wait_event_interruptible(wq, condition)                        \
({                                                                          \
    int64_t __ret = 0;                                                      \
    for (;;) {                                                              \
        if (condition)                                                      \
            break;                                                          \
        __ret = xSemaphoreTake(wq, portMAX_DELAY);                          \
    }                                                                       \
    __ret;                                                                  \
})

/**
 * @brief wait for event with condition with timeout and is interruptible
 *
 * @param[in]         wq: osal_waitqueue_t, waitqueue instance
 * @param[in]  condition: bool expression for waking up
 * @param[in] timeout_ms: timeout time in microseconds
 *
 * @return = 0 when timer has expired,
 *         > 0 the remaining time in microseconds
 *         < 0 errno if interrupted
 */
#define osal_wait_event_interruptible_timeout(wq, condition, timeout_ms)    \
({                                                                          \
    int64_t __ret = pdMS_TO_TICKS(timeout_ms);                              \
    for (;;) {                                                              \
        if (condition)                                                      \
            break;                                                          \
        __ret = xSemaphoreTake(wq, pdMS_TO_TICKS(timeout_ms));              \
    }                                                                       \
    __ret;                                                                  \
})

/**
 * @brief wake up a process in  waitqueue
 *
 * @param[in] wq: waitqueue instance
 */
static inline void osal_wake_up(osal_waitqueue_t *wq)
{
    xSemaphoreGive(*wq);
}

/**
 * @brief wake up all processes in waitqueue
 *
 * @param[in] wq: waitqueue instance
 */
static inline void osal_wake_up_all(osal_waitqueue_t *wq)
{
    xSemaphoreGive(*wq);
}

#endif
