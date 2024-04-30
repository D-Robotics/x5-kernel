#include "osal_common_test.h"
#include "osal_common_bitops.h"

static uint64_t tval;
static osal_spinlock_t lock;
static int32_t failed_num;

void test_osal_cmn_set_bit(void)
{
	uint64_t arr[4];
	memset(arr, 0, sizeof(arr));

	tval = 0;
	osal_cmn_set_bit(3, &tval);
	CHECK_RESULT(1, tval == (1 << 3), tval);
	osal_cmn_set_bit(77, arr);
	CHECK_RESULT(2, arr[1] == (1 << 13), arr[1]);

}
void test_osal_cmn_clear_bit(void)
{
	tval = 0xFFFFFFFF;
	osal_cmn_clear_bit(15, &tval);
	CHECK_RESULT(1, tval == 0xffff7FFF, tval);
}
void test_osal_cmn_change_bit(void)
{
	tval = 0xFFFFFFFF;
	osal_cmn_change_bit(15, &tval);
	CHECK_RESULT(1, tval == 0xffff7FFF, tval);
	osal_cmn_change_bit(15, &tval);
	CHECK_RESULT(2, tval == 0xffffFFFF, tval);
}
void test_osal_cmn_test_and_set_bit(void)
{
	int32_t ret;
	tval = 0;

	ret = osal_cmn_test_and_set_bit(13, &tval);
	CHECK_RESULT(1, ret == 0, ret);
	CHECK_RESULT(2, tval == (1<<13), tval);
	ret = osal_cmn_test_and_set_bit(13, &tval);
	CHECK_RESULT(3, ret == 1, ret);
	CHECK_RESULT(4, tval == (1<<13), tval);
}
void test_osal_cmn_test_and_clear_bit (void)
{
	int32_t ret;

	tval = 1 << 13;
	ret = osal_cmn_test_and_clear_bit (13, &tval);
	CHECK_RESULT(1, ret == 1, ret);
	CHECK_RESULT(2, tval == 0, tval);
	ret = osal_cmn_test_and_clear_bit (13, &tval);
	CHECK_RESULT(3, ret == 0, ret);
	CHECK_RESULT(4, tval == 0, tval);
}
void test_osal_cmn_test_and_change_bit(void)
{
	int32_t ret;

	tval = 1 << 13;
	ret = osal_cmn_test_and_change_bit(13, &tval);
	CHECK_RESULT(1, ret == 1, ret);
	CHECK_RESULT(2, tval == 0, tval);
	ret = osal_cmn_test_and_change_bit(13, &tval);
	CHECK_RESULT(3, ret == 0, ret);
	CHECK_RESULT(4, tval == (1 << 13), tval);
}

void test_osal_cmn_test_bit(void)
{
	int32_t ret;
	uint64_t data[2];
	memset(data, 0, sizeof(data));
	data[0] = 1 << 13;
	ret = osal_cmn_test_bit(13, data);
	CHECK_RESULT(1, ret == 1, ret);

	memset(data, 0, sizeof(data));
	data[1] = 1 << 13;
	ret = osal_cmn_test_bit(13 + 64, data);
	CHECK_RESULT(2, ret == 1, ret);
}

void test_osal_cmn_find_first_bit(void)
{
	int32_t ret;
	tval = 0;
	ret = osal_cmn_find_first_bit(&tval, 32);
	CHECK_RESULT(1, ret == 32, ret);

	tval = 1;
	ret = osal_cmn_find_first_bit(&tval, 32);
	CHECK_RESULT(2, ret == 0, ret);

	tval = 2;
	ret = osal_cmn_find_first_bit(&tval, 32);
	CHECK_RESULT(3, ret == 1, ret);

	tval = 0xff0;
	ret = osal_cmn_find_first_bit(&tval, 32);
	CHECK_RESULT(4, ret == 4, ret);

	tval = 1 << 11;
	ret = osal_cmn_find_first_bit(&tval, 32);
	CHECK_RESULT(5, ret == 11, ret);


	tval = 0;
	ret = osal_cmn_find_first_bit(&tval, 32);
	CHECK_RESULT(6, ret == 32, ret);
}
void test_osal_cmn_find_first_zero_bit(void)
{
	int32_t ret;
	tval = 0;
	ret = osal_cmn_find_first_zero_bit(&tval, 32);
	CHECK_RESULT(1, ret == 0, ret);

	tval = 1;
	ret = osal_cmn_find_first_zero_bit(&tval, 32);
	CHECK_RESULT(2, ret == 1, ret);

	tval = 3;
	ret = osal_cmn_find_first_zero_bit(&tval, 32);
	CHECK_RESULT(3, ret == 2, ret);

	tval = ~0;
	ret = osal_cmn_find_first_zero_bit(&tval, 32);
	CHECK_RESULT(4, ret == 32, ret);
}

int32_t main(int32_t argc, char *argv[])
{

	test_osal_cmn_set_bit();
	test_osal_cmn_clear_bit();
	test_osal_cmn_change_bit();
	test_osal_cmn_test_and_set_bit();
	test_osal_cmn_test_and_clear_bit ();
	test_osal_cmn_test_and_change_bit();
	test_osal_cmn_test_bit();
	test_osal_cmn_find_first_bit();
	test_osal_cmn_find_first_zero_bit();

	CHECK_FINAL_RESULT();

	return 0;
}