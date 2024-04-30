/*
 * Copyright 2022, Horizon Robotics
 */

#ifndef __OSAL_COMMON_ATOMIC_H__
#define __OSAL_COMMON_ATOMIC_H__

#ifdef OSAL_FREERTOS
#include <stdint.h>
#endif

typedef int osal_atomic_t;
typedef long osal_atomic_long_t;

/**
 * @brief OR a value to an atomic instance
 *
 * @param[in]  target: atomic variable instance
 * @param[in]   value: value to or
 *
 * @return the value before OR
 */
static  inline osal_atomic_t __osal_cmn_atomic_or(osal_atomic_t *target, osal_atomic_t value)
{
    return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST);
}

/**
 * @brief AND a value to an atomic instance
 *
 * @param[in]  target: atomic variable instance
 * @param[in]   value: value to and
 *
 * @return the value before AND
 */
static  inline osal_atomic_t __osal_cmn_atomic_and(osal_atomic_t *target, osal_atomic_t value)
{
    return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST);
}

/**
 * @brief XOR a value to an atomic instance
 *
 * @param[in]  target: atomic variable instance
 * @param[in]   value: value to xor
 *
 * @return the value before XOR
 */
static  inline osal_atomic_t __osal_cmn_atomic_xor(osal_atomic_t *target, osal_atomic_t value)
{
    return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST);
}


/**
 * @brief OR a value to an long atomic instance
 *
 * @param[in]  target: long atomic variable instance
 * @param[in]   value: value to or
 *
 * @return the value before OR
 */
static  inline osal_atomic_long_t __osal_cmn_atomic_long_or(osal_atomic_long_t *target, osal_atomic_long_t value)
{
    return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST);
}

/**
 * @brief AND a value to an long atomic instance
 *
 * @param[in]  target: long atomic variable instance
 * @param[in]   value: value to and
 *
 * @return the value before AND
 */
static  inline osal_atomic_long_t __osal_cmn_atomic_long_and(osal_atomic_long_t *target, osal_atomic_long_t value)
{
    return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST);
}

/**
 * @brief XOR a value to an long atomic instance
 *
 * @param[in]  target: long atomic variable instance
 * @param[in]   value: value to xor
 *
 * @return the long value before XOR
 */
static  inline osal_atomic_long_t __osal_cmn_atomic_long_xor(osal_atomic_long_t *target, osal_atomic_long_t value)
{
    return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST);
}


/**
 * @brief set a value to an atomic instance
 *
 * @param[in]  target: atomic variable instance
 * @param[in]   value: value to set
 *
 * @return value before set
 */
static inline int32_t osal_cmn_atomic_set(osal_atomic_t *target, int32_t value)
{
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

/**
 * @brief sub a value from an atomic instance
 *
 * @param[in]  target: atomic variable instance
 * @param[in]   value: value to add
 *
 * @return value before added
 */
static inline int32_t osal_cmn_atomic_add(osal_atomic_t *target, int32_t value)
{
    return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST);
}

/**
 * @brief substract a value from an atomic instance
 *
 * @param[in]  target: atomic variable instance
 * @param[in]   value: value to sub
 *
 * @return value before substracted
 */
static inline int32_t osal_cmn_atomic_sub(osal_atomic_t *target, int32_t value)
{
	return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST);
}

/**
 * @brief increase 1 to an atomic variable
 *
 * @param[out]  v: atomic variable pointer to increase
 */
static inline void osal_cmn_atomic_inc (osal_atomic_t *v)
{
	osal_cmn_atomic_add(v, 1);
}

/**
 * @brief decrease 1 to an atomic variable
 *
 * @param[out]  v: atomic variable pointer to decrease
 */
static inline void osal_cmn_atomic_dec (osal_atomic_t *v)
{
	osal_cmn_atomic_sub(v, 1);
}

/**
 * @brief read integer value of an atomic variable
 *
 * @param[out]  v: atomic variable pointer to read
 *
 * @return integer value read from an atomic pointer
 */
static inline int32_t osal_cmn_atomic_read (osal_atomic_t *v)
{
    return __atomic_load_n(v, __ATOMIC_SEQ_CST);
}

/**
 * @brief decrease 1 to an atomic variable and return the new value
 *
 * @param[out]  v: atomic variable pointer to decrease
 *
 * @return integer value after decreased 1
 */
static inline int32_t osal_cmn_atomic_dec_return (osal_atomic_t *v)
{
	return osal_cmn_atomic_sub(v, 1) - 1;
}

/**
 * @brief increase 1 to an atomic variable and return the new value
 *
 * @param[out]  v: atomic variable pointer to increase
 *
 * @return integer value after increased 1
 */
static inline int32_t osal_cmn_atomic_inc_return (osal_atomic_t *v)
{
	return osal_cmn_atomic_add(v, 1) + 1;
}

#endif
