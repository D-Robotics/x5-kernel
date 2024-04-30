#include "osal.h"
#include "osal_linux_test.h"

static osal_atomic_t tval;

void test_osal_set_bit(void)
{
	osal_atomic_set(&tval, 0);
	osal_set_bit(3, (uint64_t *)&tval);
	CHECK_RESULT(1, osal_atomic_read(&tval) == (1 << 3), osal_atomic_read(&tval));
}
void test_osal_clear_bit(void)
{
	osal_atomic_set(&tval, 0xFFFFFFFF);
	osal_clear_bit(15, (uint64_t *)&tval);
	CHECK_RESULT(1, osal_atomic_read(&tval) == 0xffff7FFF, osal_atomic_read(&tval));
}
void test_osal_change_bit(void)
{
	osal_atomic_set(&tval, 0xFFFFFFFF);
	osal_change_bit(15, (uint64_t *)&tval);
	CHECK_RESULT(1, osal_atomic_read(&tval) == 0xffff7FFF, osal_atomic_read(&tval));
	osal_change_bit(15, (uint64_t *)&tval);
	CHECK_RESULT(2, osal_atomic_read(&tval) == 0xffffFFFF, osal_atomic_read(&tval));
}
void test_osal_test_and_set_bit(void)
{
	int32_t ret;
	osal_atomic_set(&tval, 0);
	ret = osal_test_and_set_bit(13, (uint64_t *)&tval);
	CHECK_RESULT(1, ret == 0, ret);
	CHECK_RESULT(2, osal_atomic_read(&tval) == (1<<13), osal_atomic_read(&tval));
	ret = osal_test_and_set_bit(13, (uint64_t *)&tval);
	CHECK_RESULT(3, ret == 1, ret);
	CHECK_RESULT(4, osal_atomic_read(&tval) == (1<<13), osal_atomic_read(&tval));
}
void test_osal_test_and_clear_bit(void)
{
	int32_t ret;
	osal_atomic_set(&tval, 1 << 13);
	ret = osal_test_and_clear_bit(13, (uint64_t *)&tval);
	CHECK_RESULT(1, ret == 1, ret);
	CHECK_RESULT(2, osal_atomic_read(&tval) == 0, osal_atomic_read(&tval));
	ret = osal_test_and_clear_bit(13, (uint64_t *)&tval);
	CHECK_RESULT(3, ret == 0, ret);
	CHECK_RESULT(4, osal_atomic_read(&tval) == 0, osal_atomic_read(&tval));
}
void test_osal_test_and_change_bit(void)
{
	int32_t ret;
	osal_atomic_set(&tval, 1 << 13);
	ret = osal_test_and_change_bit(13, (uint64_t *)&tval);
	CHECK_RESULT(1, ret == 1, ret);
	CHECK_RESULT(2, osal_atomic_read(&tval) == 0, osal_atomic_read(&tval));
	ret = osal_test_and_change_bit(13, (uint64_t *)&tval);
	CHECK_RESULT(3, ret == 0, ret);
	CHECK_RESULT(4, osal_atomic_read(&tval) == (1 << 13), osal_atomic_read(&tval));
}

void test_osal_test_bit(void)
{
	int32_t ret;
	uint64_t data[2];
	memset(data, 0, sizeof(data));
	data[0] = 1 << 13;
	ret = osal_test_bit(13, data);
	CHECK_RESULT(1, ret == 1, ret);

	memset(data, 0, sizeof(data));
	data[1] = 1 << 13;
	ret = osal_test_bit(13 + 64, data);
	CHECK_RESULT(2, ret == 1, ret);
}


void test_osal_find_first_bit(void)
{
	int32_t ret;
	osal_atomic_set(&tval, 0);
	ret = osal_find_first_bit((uint64_t *)&tval, 32);
	CHECK_RESULT(1, ret == 32, ret);

	osal_atomic_set(&tval, 1);
	ret = osal_find_first_bit((uint64_t *)&tval, 32);
	CHECK_RESULT(2, ret == 0, ret);

	osal_atomic_set(&tval, 2);
	ret = osal_find_first_bit((uint64_t *)&tval, 32);
	CHECK_RESULT(3, ret == 1, ret);

	osal_atomic_set(&tval, 0xff0);
	ret = osal_find_first_bit((uint64_t *)&tval, 32);
	CHECK_RESULT(4, ret == 4, ret);

	osal_atomic_set(&tval, 1 << 11);
	ret = osal_find_first_bit((uint64_t *)&tval, 32);
	CHECK_RESULT(5, ret == 11, ret);


	osal_atomic_set(&tval, 0);
	ret = osal_find_first_bit((uint64_t *)&tval, 32);
	CHECK_RESULT(6, ret == 32, ret);
}
void test_osal_find_first_zero_bit(void)
{
	int32_t ret;
	osal_atomic_set(&tval, 0);
	ret = osal_find_first_zero_bit((uint64_t *)&tval, 32);
	CHECK_RESULT(1, ret == 0, ret);

	osal_atomic_set(&tval, 1);
	ret = osal_find_first_zero_bit((uint64_t *)&tval, 32);
	CHECK_RESULT(2, ret == 1, ret);

	osal_atomic_set(&tval, 3);
	ret = osal_find_first_zero_bit((uint64_t *)&tval, 32);
	CHECK_RESULT(3, ret == 2, ret);

	osal_atomic_set(&tval, ~0);
	ret = osal_find_first_zero_bit((uint64_t *)&tval, 32);
	CHECK_RESULT(4, ret == 32, ret);
}

void test_osal_bitops(void)
{
	test_osal_set_bit();
	test_osal_clear_bit();
	test_osal_change_bit();
	test_osal_test_and_set_bit();
	test_osal_test_and_clear_bit();
	test_osal_test_and_change_bit();
	test_osal_test_bit();
	test_osal_find_first_bit();
	test_osal_find_first_zero_bit();

}
