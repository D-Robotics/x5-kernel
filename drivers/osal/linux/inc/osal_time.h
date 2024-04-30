/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_TIME_H__
#define __OSAL_TIME_H__

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/timer.h>

#define NS_PER_SEC  (1000000000ULL)

typedef struct {
	struct timer_list timer;
	void (*cb_func)(void *);
	void *data;
	uint32_t interval_ms;
} osal_timer_t;

typedef struct {
	int64_t tv_sec;
	uint64_t tv_nsec;
} osal_time_t;

/**
 * @brief get time of day in nano seconds
 *
 * @return time of day in nano seconds
 */
static inline uint64_t osal_time_get_ns(void)
{
    return ktime_get_raw_ns();
}

/**
 * @brief get time of day in sec and nsec
 *
 * @param[out] t: osal_time_t instance to store time
 */
static inline void osal_time_get(osal_time_t *t)
{
	ktime_get_raw_ts64((struct timespec64 *)t);
}

/**
 * @brief get time of day in nano seconds
 *
 * @return time of day in nano seconds
 */
static inline uint64_t osal_time_get_real_ns(void)
{
    return ktime_get_real_ns();
}

/**
 * @brief get time of day in sec and nsec
 *
 * @param[out] t: osal_time_t instance to store time
 */
static inline void osal_time_get_real(osal_time_t *t)
{
    *(struct timespec64 *)t = ktime_to_timespec64(ktime_get_real());
}

/**
 * @brief sleep in microseconds
 *
 * @param[in] microseconds to sleep
 */
static inline void osal_usleep(uint64_t us)
{
    usleep_range(us - 1, us + 1);
}

/**
 * @brief sleep in milliseconds
 *
 * @param[in] milliseconds to sleep
 */
static inline void osal_msleep(uint64_t ms)
{
    msleep(ms);
}

/**
 * @brief busyloop in microseconds
 *
 * @param[in] microseconds to busyloop
 */
static inline void osal_udelay(uint64_t us)
{
    udelay(us);
}

/**
 * @brief busyloop in milliseconds
 *
 * @param[in] milliseconds to busyloop
 */
static inline void osal_mdelay(uint64_t ms)
{
    mdelay(ms);
}

/**
 * @brief internal timer worker callback function
 *
 * @param[in] arg timer instance
 */
static inline void os_timer_worker(struct timer_list* arg)
{
    osal_timer_t* tmp_timer = from_timer(tmp_timer, arg, timer);

    if (tmp_timer != NULL) {
        if (tmp_timer->cb_func != NULL) {
            tmp_timer->cb_func(tmp_timer->data);
        }
    }
}

/**
 * @brief init a timer with callback function
 *
 * @param[in] timer: timer instance
 * @param[in]  func: callback function of the timer
 * @param[in] timer: param to the callback function
 *
 * @return 0 on success, error code when failed
 */
static inline int32_t osal_timer_init(osal_timer_t *timer, void (*func)(void *), void *data)
{
    if (timer != NULL) {
        timer->cb_func = func;
        timer->data = data;
        //init_timer(&timer->timer);
        //timer->timer.function = &os_timer_worker;
        timer_setup(&timer->timer, os_timer_worker, 0);
    }
    return 0;
}

/**
 * @brief start a timer with expired time in milliseconds
 *
 * @param[in]       timer: timer instance
 * @param[in] interval_ms: timer expired time in milliseconds
 *
 */
static inline void osal_timer_start(osal_timer_t *timer, uint32_t interval_ms)
{
    if (timer != NULL) {
        timer->timer.expires = jiffies + msecs_to_jiffies(interval_ms);
        add_timer(&timer->timer);
    }
}

/**
 * @brief delete a timer
 *
 * @param[in] timer: timer instance
 *
 * @return 0 on success, error code when failed
 */
static inline int32_t osal_timer_stop(osal_timer_t *timer)
{
    int32_t ret = 0;

    if (timer != NULL) {
        ret = del_timer_sync(&timer->timer);
    }

    return ret;
}

#endif
