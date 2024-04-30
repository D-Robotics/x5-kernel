/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_SPINLOCK_H__
#define __OSAL_SPINLOCK_H__

#include <linux/spinlock.h>


typedef spinlock_t osal_spinlock_t;

/**
 * @brief initialize a spin lock
 *
 * @param[in] lock: the spinlock instance to be initialized
 */
static inline void osal_spin_init(osal_spinlock_t *lock)
{
	spin_lock_init((spinlock_t *)lock);
}

/**
 * @brief lock a spinlock, busyloop if the lock is on contention
 *
 * @param[in] lock: the spinlock instance to operate
 */
static inline void osal_spin_lock(osal_spinlock_t *lock)
{
	spin_lock((spinlock_t *)lock);
}

/**
 * @brief try to lock a spinlock, busyloop if the lock is on contention
 *
 * @param[in] lock: the spinlock instance to operate
 *
 * @return 1 if lock is accquired, 0 if on contention
 */
static inline int32_t osal_spin_trylock(osal_spinlock_t *lock)
{
	return spin_trylock((spinlock_t *)lock);
}

/**
 * @brief unlock a spinlock, busyloop if the lock is on contention
 *
 * @param[in] lock: the spinlock instance to operate
 */
static inline void osal_spin_unlock(osal_spinlock_t *lock)
{
	spin_unlock((spinlock_t *)lock);
}


/**
 * @brief lock a spinlock and disable irq, busyloop if the lock is on contention
 *
 * @param[in]  lock: the spinlock instance to operate
 * @param[in] flags: the flags variable to save irq status
 */
static inline void osal_spin_lock_irqsave(osal_spinlock_t *lock, uint64_t *flags)
{
	spin_lock_irqsave((spinlock_t *)lock, *(unsigned long *)flags);
}

/**
 * @brief try to lock a spinlock and disable irq, busyloop if the lock is on contention
 *
 * @param[in]  lock: the spinlock instance to operate
 * @param[in] flags: the flags variable to save irq status
 *
 * @return 1 if lock is accquired, 0 if on contention
 */
static inline int32_t osal_spin_trylock_irqsave(osal_spinlock_t *lock, uint64_t *flags)
{
	return spin_trylock_irqsave((spinlock_t *)lock, *(unsigned long *)flags);
}

/**
 * @brief unlock a spinlock and enable irq
 *
 * @param[in] lock: the spinlock instance to operate
 * @param[in] flags: the flags variable to restore irq status
 */
static inline void osal_spin_unlock_irqrestore(osal_spinlock_t *lock, uint64_t *flags)
{
	spin_unlock_irqrestore((spinlock_t *)lock, *(unsigned long *)flags);
}

#endif

