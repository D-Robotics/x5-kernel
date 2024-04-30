/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_TIME_H__
#define __OSAL_TIME_H__

#include <stdint.h>
#include "FreeRTOS.h"
#include "timers.h"
#include "J6e.h"

typedef struct {
    uint32_t timer_id;
    TimerHandle_t handle;
    void (*cb_func)(void *);
    void *data;
    uint32_t interval_ms;
} osal_timer_t;

typedef struct {
	int64_t tv_sec;
	uint64_t tv_nsec;
} osal_time_t;

/**
 * @brief get time of monotonous timeline 
 *
 * @return time value of ms
 */
extern uint64_t osal_time_get_ns(void);

/**
 * @brief get time of day in sec and nsec
 *
 * @param[out] t: osal_time_t instance to store time
 */
static inline void osal_time_get(osal_time_t *t)
{
	uint64_t ts = osal_time_get_ns();
	t->tv_sec = (int64_t)(ts / 1000000000);
	t->tv_nsec = (uint64_t)ts % 1000000000;
}

/**
 * @brief get time of day in nano seconds
 *
 * @return time of day in nano seconds
 */
static inline uint64_t osal_time_get_real_ns(void)
{
    return osal_time_get_ns();
}

/**
 * @brief get time of day in sec and nsec
 *
 * @param[out] t: osal_time_t instance to store time
 */
static inline void osal_time_get_real(osal_time_t *t)
{
	uint64_t ts = osal_time_get_ns();
	t->tv_sec = (int64_t)(ts / 1000000000);
	t->tv_nsec = (uint64_t)ts % 1000000000;
}

/**
 * @brief sleep for us
 *
 * @param[in] time value of us
 */
extern void osal_usleep(uint64_t us);

/**
 * @brief sleep for ms
 *
 * @param[in] time value of ms
 */
extern void osal_msleep(uint64_t ms);

/**
 * @brief delay for us
 *
 * @param[in] time value of us
 */
extern void osal_udelay(uint64_t us);

/**
 * @brief delay for ms
 *
 * @param[in] time value of ms
 */
extern void osal_mdelay(uint64_t ms);

/**
 * @brief initilized a timer
 *
 * @param[in] func the address that the timer callback will start at
 * @param[in] data a pointer to the arguments for func
 * @param[out] timer timer instance to be initilized
 * 
 * @return 0 on success, an error code on failure.
 */
extern int osal_timer_init(osal_timer_t *timer, void (*func)(void *), void *data);

/**
 * @brief start a timer
 *
 * @param[in] timer timer instance
 * @param[in] interval_ms timeout for the timer
 */
extern void osal_timer_start(osal_timer_t *timer, uint64_t interval_ms);

/**
 * @brief stop a timer
 *
 * @param[in] timer timer instance
 * 
 * @return 0 on success, an error code on failure.
 */
extern int osal_timer_stop(osal_timer_t *timer);

#endif //__OSAL_TIME_H__
