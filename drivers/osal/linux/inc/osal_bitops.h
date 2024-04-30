/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_BITOPS_H__
#define __OSAL_BITOPS_H__

/**
 * @brief set a bit to 1
 *
 * @param[in] nr: indicate which bit to set
 * @param[out] p:  the start memory to operate
 */
static inline void osal_set_bit(uint32_t nr, volatile uint64_t *p)
{
	set_bit(nr, (volatile unsigned long *)p);
}

/**
 * @brief clear a bit to 0
 *
 * @param[in] nr: indicate which bit to clear
 * @param[out] p: the start memory to operate
 */
static inline void osal_clear_bit(uint32_t nr, volatile uint64_t *p)
{
	clear_bit(nr, (volatile unsigned long *)p);
}

/**
 * @brief change a bit from 0 to 1 or 1 to 0
 *
 * @param[in] nr: indicate which bit to change
 * @param[out] p: the start memory to operate
 */
static inline void osal_change_bit(uint32_t nr, volatile uint64_t *p)
{
	change_bit(nr, (volatile unsigned long *)p);
}

/**
 * @brief check if the bit is 1 and set if not
 *
 * @param[in] nr: indicate which bit to check
 * @param[out] p: the start memory to operate
 *
 * @return 1 if old bit is 1, 0 if old bit is 0
 */
static inline int32_t osal_test_and_set_bit(uint32_t nr, volatile uint64_t *p)
{
	return test_and_set_bit(nr, (volatile unsigned long *)p);
}

/**
 * @brief check if the bit is 1, clear it if it is.
 *
 * @param[in] nr: indicate which bit to check
 * @param[out] p: the start memory to operate
 *
 * @return 1 if old bit is 1, 0 if old bit is 0
 */
static inline int32_t osal_test_and_clear_bit(uint32_t nr, volatile uint64_t *p)
{
	return test_and_clear_bit(nr, (volatile unsigned long *)p);
}

/**
 * @brief check if the bit nr is 1.
 *
 * @param[in] nr: indicate which bit to check
 * @param[out] p: the start memory to operate
 *
 * @return 1 if bit nr is 1, 0 if bit nr is 0
 */
static inline int32_t osal_test_bit(uint32_t nr, const volatile uint64_t *p)
{
	return test_bit(nr, (const volatile unsigned long *)p);
}

/**
 * @brief check if the bit is 1, change it if it is.
 *
 * @param[in] nr: indicate which bit to check
 * @param[out] p: the start memory to operate
 *
 * @return 1 if old bit is 1, 0 if old bit is 0
 */
static inline int32_t osal_test_and_change_bit(uint32_t nr, volatile uint64_t *p)
{
	return test_and_change_bit(nr, (volatile unsigned long *)p);
}

/**
 * @brief find the position of first bit 1 from a memory address
 *
 * @param[in] size: max number of bits to find
 * @param[in] p: the start memory to find
 *
 * @return [0,size): the postion of bit 1
 *             size: bit 1 is not found
 */
static inline size_t osal_find_first_bit(const uint64_t *addr, size_t size)
{
	return find_first_bit((unsigned long *)addr, size);
}

/**
 * @brief find the position of first bit 0 from a memory address
 *
 * @param[in] size: max number of bits to find
 * @param[in] p: the start memory to find
 *
 * @return [0,size): the postion of bit 0
 *             size: bit 0 is not found
 */
static inline size_t osal_find_first_zero_bit(const uint64_t *addr, size_t size)
{
	return find_first_zero_bit((unsigned long *)addr, size);
}

#endif
