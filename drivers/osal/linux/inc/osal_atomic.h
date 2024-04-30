/*
 * Copyright 2022, Horizon Robotics
 */

#ifndef __OSAL_ATOMIC_H__
#define __OSAL_ATOMIC_H__

#include <linux/types.h>
#include <linux/atomic.h>

typedef atomic_t osal_atomic_t;

/**
 * @brief set a value to an atomic variable
 *
 * @param[in]  v: atomic variable address
 * @param[in]  i: value to set to atomic variable
 */
static inline void osal_atomic_set (osal_atomic_t *v, int32_t i)
{
	atomic_set((atomic_t *)v, i);
}

/**
 * @brief increase 1 to an atomic variable
 *
 * @param[in]  v: atomic variable pointer to increase
 */
static inline void osal_atomic_inc (osal_atomic_t *v)
{
	atomic_inc((atomic_t *)v);
}

/**
 * @brief decrease 1 to an atomic variable
 *
 * @param[in]  v: atomic variable pointer to decrease
 */
static inline void osal_atomic_dec (osal_atomic_t *v)
{
	atomic_dec((atomic_t *)v);
}

/**
 * @brief add a value an atomic variable
 *
 * @param[in]  v: atomic variable pointer to decrease
 */
static inline void osal_atomic_add (int32_t val, osal_atomic_t *v)
{
	atomic_add(val, (atomic_t *)v);
}

/**
 * @brief read integer value of an atomic variable
 *
 * @param[in]  v: atomic variable pointer to read
 *
 * @return integer value read from an atomic pointer
 */
static inline int32_t osal_atomic_read (osal_atomic_t *v)
{
	return atomic_read((atomic_t *)v);
}

/**
 * @brief decrease 1 to an atomic variable and return the new value
 *
 * @param[in]  v: atomic variable pointer to decrease
 *
 * @return integer value after decreased 1
 */
static inline int32_t osal_atomic_dec_return (osal_atomic_t *v)
{
	return atomic_dec_return((atomic_t *)v);
}

/**
 * @brief increase 1 to an atomic variable and return the new value
 *
 * @param[in]  v: atomic variable pointer to increase
 *
 * @return integer value after increased 1
 */
static inline int32_t osal_atomic_inc_return (osal_atomic_t *v)
{
	return atomic_inc_return((atomic_t *)v);
}

#endif
