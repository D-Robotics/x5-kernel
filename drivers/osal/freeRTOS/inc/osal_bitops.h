/*
 * Copyright 2022, Horizon Robotics
 */

#ifndef __OSAL_BITOPS_H__
#define __OSAL_BITOPS_H__

#include "osal_common_bitops.h"

extern void osal_set_bit(uint32_t nr, volatile uint64_t *p);
extern void osal_clear_bit(uint32_t nr, volatile uint64_t *p);
extern void osal_change_bit(uint32_t nr, volatile uint64_t *p);
extern int32_t osal_test_and_set_bit(uint32_t nr, volatile uint64_t *p);
extern int32_t osal_test_and_clear_bit(uint32_t nr, volatile uint64_t *p);
extern int32_t osal_test_and_change_bit(uint32_t nr, volatile uint64_t *p);
extern uint64_t osal_find_first_bit(const uint64_t *addr, size_t size);
extern uint64_t osal_find_first_zero_bit(const uint64_t *addr, size_t size);
extern int32_t osal_test_bit(uint32_t nr, const volatile uint64_t *p);

#endif