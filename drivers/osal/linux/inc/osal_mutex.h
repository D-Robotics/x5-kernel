/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_MUTEX_H__
#define __OSAL_MUTEX_H__

#include <linux/mutex.h>

typedef struct mutex osal_mutex_t;

/**
 * @brief initialize a mutex lock
 *
 * @param[in] lock: the mutex instance to be initialized
 */
static inline void osal_mutex_init(osal_mutex_t *lock)
{
	mutex_init((struct mutex *)lock);
}

/**
 * @brief lock a mutex lock, wait if the lock is on contention
 *
 * @param[in] lock: the mutex instance to operate
 */
static inline void osal_mutex_lock(osal_mutex_t *lock)
{
	mutex_lock((struct mutex *)lock);
}

/**
 * @brief try lock a mutex lock, return
 *
 * @param[in] lock: the mutex instance to operate
 * @return 1 is the mutex is accquired, 0 on contention
 */
static inline int32_t osal_mutex_trylock(osal_mutex_t *lock)
{
	return mutex_trylock((struct mutex *)lock);
}

/**
 * @brief unlock a mutex lock
 *
 * @param[in] lock: the mutex instance to operate
 */
static inline void osal_mutex_unlock(osal_mutex_t *lock)
{
	mutex_unlock((struct mutex *)lock);
}

#endif

