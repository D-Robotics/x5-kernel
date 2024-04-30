/*
 * Copyright 2022, Horizon Robotics
 */

#ifndef __osal_BITOPS_H__
#define __osal_BITOPS_H__

#ifdef OSAL_FREERTOS
#include <stdint.h>
#include <stddef.h>
#endif

#include "osal_common_atomic.h"

#define BITS_PER_BYTE  8
#define BYTES_PER_32BIT 4
#define BITS_PER_LONG (sizeof((size_t)0) * BITS_PER_BYTE)

#define BIT_MASK(nr)        ((size_t)(1) << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)        ((nr) / BITS_PER_LONG)

static inline uint32_t __osal_cmn_ffs(uint64_t word)
{
    return __builtin_ctzll(word);
}

static inline uint32_t __osal_cmn_ffz(uint64_t word)
{
    return __builtin_ctzll(~word);
}

#define __osal_bitops_min(a, b) ((a) > (b) ? (b) : (a))


/**
 * @brief set a bit to 1
 *
 * @param[in] nr: indicate which bit to set
 * @param[in]  p:  the start memory to operate
 */
static inline void osal_cmn_set_bit(uint32_t bit, uint64_t *p)
{
	size_t mask = (size_t)BIT_MASK(bit);
	p = (size_t *)p + BIT_WORD(bit);

	if (sizeof((size_t)0) == BYTES_PER_32BIT) {
		(void)__osal_cmn_atomic_or((osal_atomic_t *)p, (osal_atomic_t)mask);
	} else {
		(void)__osal_cmn_atomic_long_or((osal_atomic_long_t *)p, (osal_atomic_long_t)mask);
	}
}

/**
 * @brief clear a bit to 0
 *
 * @param[in] nr: indicate which bit to clear
 * @param[in]  p: the start memory to operate
 */
static inline void osal_cmn_clear_bit(uint32_t bit, uint64_t *p)
{
	size_t mask = (size_t)BIT_MASK(bit);
	p = (size_t *)p + BIT_WORD(bit);

	if (sizeof((size_t)0) == BYTES_PER_32BIT) {
		(void)__osal_cmn_atomic_and((osal_atomic_t *)p, ~(osal_atomic_t)mask);
	} else {
		(void)__osal_cmn_atomic_long_and((osal_atomic_long_t *)p, ~(osal_atomic_long_t)mask);
	}
}

/**
 * @brief change a bit from 0 to 1 or 1 to 0
 *
 * @param[in] nr: indicate which bit to change
 * @param[in]  p: the start memory to operate
 */
static inline void osal_cmn_change_bit(uint32_t bit, uint64_t *p)
{
	size_t mask = (size_t)BIT_MASK(bit);
	p = (size_t *)p + BIT_WORD(bit);

	if (sizeof((size_t)0) == BYTES_PER_32BIT) {
		(void)__osal_cmn_atomic_xor((osal_atomic_t *)p, (osal_atomic_t)mask);
	} else {
		(void)__osal_cmn_atomic_long_xor((osal_atomic_long_t *)p, (osal_atomic_long_t)mask);
	}
}

/**
 * @brief check if the bit is 1 and set if not
 *
 * @param[in] nr: indicate which bit to check
 * @param[in]  p: the start memory to operate
 *
 * @return 1 if old bit is 1, 0 if old bit is 0
 */
static inline uint32_t osal_cmn_test_and_set_bit(uint32_t bit, uint64_t *p)
{
	size_t old;
	size_t mask = (size_t)BIT_MASK(bit);
	p = (size_t *)p + BIT_WORD(bit);

	if (sizeof((size_t)0) == BYTES_PER_32BIT) {
		old = (size_t)__osal_cmn_atomic_or((osal_atomic_t *)p, (osal_atomic_t)mask);
	} else {
		old = (size_t)__osal_cmn_atomic_long_or((osal_atomic_long_t *)p, (osal_atomic_long_t)mask);
	}
	return (old & mask) != 0;
}

/**
 * @brief check if the bit is 1, clear it if it is.
 *
 * @param[in] nr: indicate which bit to check
 * @param[in]  p: the start memory to operate
 *
 * @return 1 if old bit is 1, 0 if old bit is 0
 */
static inline int32_t osal_cmn_test_and_clear_bit(uint32_t bit, uint64_t *p)
{
	size_t old;
	size_t mask = (size_t)BIT_MASK(bit);
	p = (size_t *)p + BIT_WORD(bit);

	if (sizeof((size_t)0) == BYTES_PER_32BIT) {
		old = (size_t)__osal_cmn_atomic_and((osal_atomic_t *)p, ~(osal_atomic_t)mask);
	} else {
		old = (size_t)__osal_cmn_atomic_long_and((osal_atomic_long_t *)p, ~(osal_atomic_long_t)mask);
	}

	return (old & mask) != 0;
}

/**
 * @brief check if the bit is 1, change it if it is.
 *
 * @param[in] nr: indicate which bit to check
 * @param[in]  p: the start memory to operate
 *
 * @return 1 if old bit is 1, 0 if old bit is 0
 */
static inline uint32_t osal_cmn_test_and_change_bit(uint32_t bit, uint64_t *p)
{
	size_t old;
	size_t mask = (size_t)BIT_MASK(bit);
	p = (size_t *)p + BIT_WORD(bit);

	if (sizeof((size_t)0) == BYTES_PER_32BIT) {
		old = (size_t)__osal_cmn_atomic_xor((osal_atomic_t *)p, (osal_atomic_t)mask);
	} else {
		old = (size_t)__osal_cmn_atomic_long_xor((osal_atomic_long_t *)p, (osal_atomic_long_t)mask);
	}

	return (old & mask) != 0;
}

/**
 * @brief check if the bit is 1.
 *
 * @param[in] nr: indicate which bit to check
 * @param[out] p: the start memory to operate
 *
 * @return 1 if bit nr is 1, 0 if bit nr is 0
 */
static inline uint32_t osal_cmn_test_bit(uint32_t bit, uint64_t *p)
{
	return 1UL & (((size_t *)p)[BIT_WORD(bit)] >> (bit & (BITS_PER_LONG-1)));
}

/**
 * @brief find the position of first bit 1 from a memory address
 *
 * @param[in] size: max number of bits to find
 * @param[in] p: the start memory to find
 *
 * @return [0,size): the position of bit 1
 *             size: bit 1 is not found
 */
static inline size_t osal_cmn_find_first_bit(uint64_t *addr, size_t size)
{
	int32_t i;

	for (i = 0; i * BITS_PER_LONG < size; i++) {
		if (addr[i])
			return __osal_bitops_min(i * BITS_PER_LONG + __osal_cmn_ffs(addr[i]), size);
	}

	return size;
}

/**
 * @brief find the position of first bit 0 from a memory address
 *
 * @param[in] size: max number of bits to find
 * @param[in]    p: the start memory to find
 *
 * @return [0,size): the postion of bit 0
 *             size: bit 0 is not found
 */
static inline size_t osal_cmn_find_first_zero_bit(uint64_t *addr, size_t size)
{
	int32_t i;

	for (i = 0; i * BITS_PER_LONG < size; i++) {
		if (addr[i] != ~0UL)
			return __osal_bitops_min(i * BITS_PER_LONG + __osal_cmn_ffz(addr[i]), size);
	}

	return size;
}

#endif
