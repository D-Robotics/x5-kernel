#include "osal.h"
#include "osal_linux_test.h"

#define TEST_FIFO_SIZE 12345
#define TEST_BUF_SIZE 256
osal_fifo_t fifo;
uint8_t fifo_buf[TEST_FIFO_SIZE];

uint8_t inbuf[TEST_BUF_SIZE];
uint8_t outbuf[TEST_BUF_SIZE];

osal_spinlock_t lock;
osal_mutex_t mutex;

#if 0
static inline uint32_t rounddown_pow_of_two(uint32_t n)
{
	n|=n>>1; n|=n>>2; n|=n>>4; n|=n>>8; n|=n>>16;
	return (n+1) >> 1;
}

static inline uint32_t roundup_pow_of_two(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}
#endif

static int32_t test_osal_fifo_init(void)
{
	int32_t ret;

	ret = osal_fifo_init(&fifo, fifo_buf, sizeof(fifo_buf));

	CHECK_RESULT(1, ret == 0, ret);
	CHECK_RESULT(2, osal_fifo_size(&fifo) == rounddown_pow_of_two(TEST_FIFO_SIZE), osal_fifo_size(&fifo));

	memset(&fifo, 0, sizeof(fifo));

	return ret;
}

static int32_t test_osal_fifo_alloc(void)
{
	int32_t ret;

	ret = osal_fifo_alloc(&fifo, TEST_FIFO_SIZE, 0);

	CHECK_RESULT(1, ret == 0, ret);
	CHECK_RESULT(2, osal_fifo_size(&fifo) == roundup_pow_of_two(TEST_FIFO_SIZE), osal_fifo_size(&fifo));

	if (ret != 0)
		osal_pr_err("ERROR: osal_fifo_alloc failed \n");
	else
		osal_fifo_free(&fifo);

	memset(&fifo, 0, sizeof(fifo));

	return ret;
}
static int32_t test_osal_fifo_free(void)
{
	return test_osal_fifo_alloc();
}
static int32_t test_osal_fifo_in(void)
{
	int32_t i, ret;
	int32_t inlen = sizeof(inbuf);

	ret = osal_fifo_alloc(&fifo, TEST_FIFO_SIZE, 0);
	CHECK_RESULT(1, ret == 0, ret);

	for (i = 0; i < TEST_FIFO_SIZE/inlen; i++) {
		memset(inbuf, i, inlen);
		ret = osal_fifo_in(&fifo, inbuf, inlen);
		CHECK_RESULT(2, ret == inlen, ret);
		CHECK_RESULT(3, osal_fifo_len(&fifo) == inlen * (i+1), osal_fifo_len(&fifo));
	}

	for (i = 0; i < TEST_FIFO_SIZE/inlen; i++) {
		memset(inbuf, i, inlen);
		ret = osal_fifo_out(&fifo, outbuf, inlen);
		CHECK_RESULT(4, ret == inlen, ret);

		ret = memcmp(inbuf, outbuf, inlen);
		CHECK_RESULT(5, ret == 0, ret);
	}

	osal_fifo_free(&fifo);
	memset(&fifo, 0, sizeof(fifo));

	return 0;
}


static int32_t test_osal_fifo_out(void)
{
	return test_osal_fifo_in();
}
static int32_t test_osal_fifo_copy_from_to_user(void)
{
#if 0
	int32_t len, ret;
	int32_t copied;


	osal_fifo_init(&fifo, fifo_buf, sizeof(fifo_buf));

	len = sizeof(inbuf);
	memset(inbuf, 'a', len);
	ret = osal_fifo_copy_from_user(&fifo, inbuf, len, &copied);
	CHECK_RESULT(0, ret == 0, ret);

	CHECK_RESULT(1, osal_fifo_len(&fifo) == len, osal_fifo_len(&fifo));
	CHECK_RESULT(2, len == copied, copied);

	osal_fifo_copy_to_user(&fifo, &outbuf, len, &copied);
	CHECK_RESULT(3, osal_fifo_len(&fifo) == 0, osal_fifo_len(&fifo));
	CHECK_RESULT(4, len == copied, copied);
#endif
	return 0;

}
static int32_t test_osal_fifo_initialized(void)
{
	osal_fifo_init(&fifo, fifo_buf, sizeof(fifo_buf));
	CHECK_RESULT(1, osal_fifo_initialized(&fifo) == true, false);

	return 0;
}
static int32_t test_osal_fifo_reset(void)
{
	osal_fifo_init(&fifo, fifo_buf, sizeof(fifo_buf));
	osal_fifo_in(&fifo, inbuf, sizeof(inbuf));
	osal_fifo_reset(&fifo);
	CHECK_RESULT(1, osal_fifo_len(&fifo) == 0, 0);
	CHECK_RESULT(2, osal_fifo_is_empty(&fifo) == true, 0);

	return 0;
}
static int32_t test_osal_fifo_len(void)
{
	return test_osal_fifo_in();
}
static int32_t test_osal_fifo_is_empty(void)
{
	return test_osal_fifo_reset();
}
static int32_t test_osal_fifo_is_empty_spinlocked(void)
{
	osal_fifo_reset(&fifo);
	CHECK_RESULT(1, osal_fifo_is_empty_spinlocked(&fifo, &lock) == true, 0);

	return 0;
}


static int32_t test_osal_fifo_is_full(void)
{
	int32_t inlen = sizeof(inbuf);

	while(osal_fifo_in(&fifo, inbuf, inlen) > 0);

	CHECK_RESULT(1, osal_fifo_is_full(&fifo) == true, osal_fifo_len(&fifo));
	osal_fifo_reset(&fifo);

	return 0;
}

static int32_t test_osal_fifo_in_spinlocked(void)
{
	int32_t tmp;
	int32_t ret;
	uint8_t buf[55];

	tmp = osal_fifo_len(&fifo);
	ret = osal_fifo_in_spinlocked(&fifo, buf, sizeof(buf), &lock);
	CHECK_RESULT(1, (tmp + ret) == osal_fifo_len(&fifo), ret);

	return 0;
}
static int32_t test_osal_fifo_out_spinlocked(void)
{
	int32_t tmp;
	int32_t ret;
	uint8_t buf[55];

	tmp = osal_fifo_len(&fifo);
	ret = osal_fifo_out_spinlocked(&fifo, buf, sizeof(buf), &lock);
	CHECK_RESULT(1, (sizeof(buf) - tmp) == osal_fifo_len(&fifo), ret);

	return 0;
}

static int32_t done = 0;
#define TEST_LOCKED 0


static int32_t thread_func_1(void *data) {
    int32_t thread_id = *(int32_t *)data;
	int32_t i, ret;
	int32_t inlen = sizeof(inbuf);

    osal_pr_info("Thread %d is executing in function 1.\n", thread_id);
    for(i = 0; i < 3*TEST_FIFO_SIZE/inlen; i++) {
		memset(inbuf, i%128, sizeof(inbuf));
		//hex_dump(inbuf, sizeof(inbuf));
#if TEST_LOCKED
		ret = osal_fifo_in_spinlocked(&fifo, inbuf, inlen, &lock);
#else
		ret = osal_fifo_in(&fifo, inbuf, inlen);
#endif
		osal_usleep(10);
		if (ret == 0) {
			i--;
			osal_usleep(1000);
		}
	}

	done++;
    return 0;
}

static int32_t thread_func_2(void *data) {
    int32_t thread_id = *(int32_t *)data;
	int32_t i;
	int32_t outlen = sizeof(inbuf);
	uint8_t tmpbuf[TEST_BUF_SIZE];
	int32_t ret;

    osal_pr_info("Thread %d is executing in function 1.\n", thread_id);
	while(osal_fifo_len(&fifo) == 0) {
		osal_msleep(1);
	}
    for(i = 0; i < 3*TEST_FIFO_SIZE/outlen; i++) {
		memset(tmpbuf, i%128, TEST_BUF_SIZE);
#if TEST_LOCKED
		ret = osal_fifo_out_spinlocked(&fifo, outbuf, outlen, &lock);
#else
		ret = osal_fifo_out(&fifo, outbuf, outlen);
#endif
		ret = memcmp(outbuf, tmpbuf, outlen);
		CHECK_RESULT(i, ret == 0, ret);
		osal_usleep(10);
		if (ret == 0) {
				osal_msleep(1);
		}
	}

	done++;
    return 0;
}


static int32_t test_single_thread(void)
{
	test_osal_fifo_init();
	test_osal_fifo_alloc();
	test_osal_fifo_free();
	test_osal_fifo_in();
	test_osal_fifo_out();
	test_osal_fifo_copy_from_to_user();
	test_osal_fifo_initialized();
	test_osal_fifo_reset();
	test_osal_fifo_len();
	test_osal_fifo_is_empty();
	test_osal_fifo_is_full();
	test_osal_fifo_is_empty_spinlocked();
	test_osal_fifo_in_spinlocked();
	test_osal_fifo_out_spinlocked();

	return 0;
}


static int32_t test_multiple_thread(void)
{
    osal_thread_t thread1, thread2;
    int32_t thread_id1 = 1;
    int32_t thread_id2 = 2;

	memset(&fifo, 0, sizeof(fifo));
	osal_fifo_alloc(&fifo, TEST_FIFO_SIZE, 0);

    if (osal_thread_init(&thread1, NULL, thread_func_1, &thread_id1) != 0) {
        osal_pr_info("create thread 1 failed\n");
        return -ENODEV;
    }
    if (osal_thread_init(&thread2, NULL, thread_func_2, &thread_id2) != 0) {
        osal_pr_info("create thread 2 failed\n");
        return -ENODEV;
    }

	osal_thread_start(&thread1);
	osal_thread_start(&thread2);

	while(done < 2)
		osal_msleep(10);

    osal_pr_info("Both threads have finished.\n");

	return 0;
}


void test_osal_fifo(void)
{
	int32_t ret;

	osal_spin_init(&lock);
	osal_mutex_init(&mutex);

	ret = test_single_thread();
	ret = test_multiple_thread();

    return;
}
