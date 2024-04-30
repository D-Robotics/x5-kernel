/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_SEMAPHORE_H__
#define __OSAL_SEMAPHORE_H__

#include <linux/jiffies.h>
#include <linux/semaphore.h>

typedef struct semaphore osal_sem_t;

/**
 * @brief init a semaphore
 *
 * @param[in]  sem: the semaphore instance to init
 * @param[in]  val: the semaphore instance to init
 */
static inline void osal_sema_init(osal_sem_t *sem, int32_t val)
{
	sema_init((struct semaphore *)sem, val);
}

/**
 * @brief increase 1 to a semaphore and wakeup waiting process if it becomes 0
 *
 * @param[in]  sem: the semaphore instance to operate
 */
static inline void osal_sema_up(osal_sem_t *sem)
{
	up((struct semaphore *)sem);
}

/**
 * @brief decrease 1 to a semaphore if counter > 0, sleep if counter is 0.
 *        this interface is interruptible by signal
 *
 * @param[in]  sem: the semaphore instance to operate
 *
 * @retval      0: when success
 *         -EINTR: when interrupted by signal
 */
static inline int32_t osal_sema_down_interruptible(osal_sem_t *sem)
{
	return down_interruptible((struct semaphore *)sem);
}

/**
 * @brief decrease 1 to a semaphore if counter > 0, sleep with timeout if counter is 0
 *
 * @param[in]     sem: the semaphore instance to operate
 * @param[in] time_ms: max time to sleep
 *
 * @retval      0: when success
 *         -EINTR: when interrupted by signal
 */
static inline int32_t osal_sema_down_timeout(osal_sem_t *sem, uint32_t time_ms)
{
	return down_timeout((struct semaphore *)sem, msecs_to_jiffies(time_ms));
}

#endif
