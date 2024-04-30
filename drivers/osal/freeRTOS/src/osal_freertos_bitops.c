#include "osal_bitops.h"

void osal_set_bit(uint32_t nr, volatile uint64_t *p)
{
	osal_cmn_set_bit(nr, p);
}

void osal_clear_bit(uint32_t nr, volatile uint64_t *p)
{
	osal_cmn_clear_bit(nr, p);
}

void osal_change_bit(uint32_t nr, volatile uint64_t *p)
{
	osal_cmn_change_bit(nr, p);
}

int32_t osal_test_and_set_bit(uint32_t nr, volatile uint64_t *p)
{
	return osal_cmn_test_and_set_bit(nr, p);
}

int32_t osal_test_and_clear_bit(uint32_t nr, volatile uint64_t *p)
{
	return osal_cmn_test_and_clear_bit(nr, p);
}

int32_t osal_test_and_change_bit(uint32_t nr, volatile uint64_t *p)
{
	return osal_cmn_test_and_change_bit(nr, p);
}

uint64_t osal_find_first_bit(const uint64_t *addr, size_t size)
{
	return osal_cmn_find_first_bit(addr, size);
}

uint64_t osal_find_first_zero_bit(const uint64_t *addr, size_t size)
{
	return osal_cmn_find_first_zero_bit(addr, size);
}

int32_t osal_test_bit(uint32_t nr, const volatile uint64_t *p)
{
	return osal_cmn_test_bit(nr, p);
}