#include "osal_common_test.h"
#include "osal_common_fifo.h"


#define TEST_FIFO_SIZE 12345
#define TEST_BUF_SIZE 256

static osal_fifo_t fifo;
static osal_spinlock_t lock;
static pthread_mutex_t mutex;
static int32_t failed_num;

uint8_t fifo_buf[TEST_FIFO_SIZE];
uint8_t inbuf[TEST_BUF_SIZE];
uint8_t outbuf[TEST_BUF_SIZE];


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

static int32_t test_osal_cmn_fifo_init(void)
{
	int32_t ret;

	ret = osal_cmn_fifo_init(&fifo, fifo_buf, sizeof(fifo_buf));

	CHECK_RESULT(1, ret == 0, ret);
	CHECK_RESULT(2, osal_cmn_fifo_size(&fifo) == rounddown_pow_of_two(TEST_FIFO_SIZE), osal_cmn_fifo_size(&fifo));

	memset(&fifo, 0, sizeof(fifo));

	return ret;
}

static int32_t test_osal_cmn_fifo_alloc(void)
{
	int32_t ret;

	ret = osal_cmn_fifo_alloc(&fifo, TEST_FIFO_SIZE, OSAL_KMALLOC_KERNEL);

	CHECK_RESULT(1, ret == 0, ret);
	CHECK_RESULT(2, osal_cmn_fifo_size(&fifo) == roundup_pow_of_two(TEST_FIFO_SIZE), osal_cmn_fifo_size(&fifo));

	if (ret != 0)
		printf("ERROR: osal_cmn_fifo_alloc failed \n");
	else
		osal_cmn_fifo_free(&fifo);

	memset(&fifo, 0, sizeof(fifo));

	return ret;
}
static int32_t test_osal_cmn_fifo_free(void)
{
	return test_osal_cmn_fifo_alloc();
}
static int32_t test_osal_cmn_fifo_in(void)
{
	int32_t i, ret;
	int32_t inlen = sizeof(inbuf);

	ret = osal_cmn_fifo_alloc(&fifo, TEST_FIFO_SIZE, OSAL_KMALLOC_ATOMIC);
	CHECK_RESULT(1, ret == 0, ret);

	for (i = 0; i < TEST_FIFO_SIZE/inlen; i++) {
		memset(inbuf, i, inlen);
		ret = osal_cmn_fifo_in(&fifo, inbuf, inlen);
		CHECK_RESULT(2, ret == inlen, ret);
		CHECK_RESULT(3, osal_cmn_fifo_len(&fifo) == inlen * (i+1), osal_cmn_fifo_len(&fifo));
	}

	for (i = 0; i < TEST_FIFO_SIZE/inlen; i++) {
		memset(inbuf, i, inlen);
		ret = osal_cmn_fifo_out(&fifo, outbuf, inlen);
		CHECK_RESULT(4, ret == inlen, ret);

		ret = memcmp(inbuf, outbuf, inlen);
		CHECK_RESULT(5, ret == 0, ret);
	}

	osal_cmn_fifo_free(&fifo);
	memset(&fifo, 0, sizeof(fifo));

	return 0;
}

static int32_t test_osal_cmn_fifo_out(void)
{
	return test_osal_cmn_fifo_in();
}
static int32_t test_osal_cmn_fifo_copy_from_to_user()
{
	uint32_t len;
	int32_t ret;
	uint32_t copied;

	osal_cmn_fifo_init(&fifo, fifo_buf, sizeof(fifo_buf));

	len = sizeof(inbuf);
	memset(inbuf, 'a', len);
	ret = osal_cmn_fifo_copy_from_user(&fifo, inbuf, len, &copied);
	CHECK_RESULT(0, ret == 0, ret);

	CHECK_RESULT(1, osal_cmn_fifo_len(&fifo) == len, osal_cmn_fifo_len(&fifo));
	CHECK_RESULT(2, len == copied, copied);

	osal_cmn_fifo_copy_to_user(&fifo, &outbuf, len, &copied);
	CHECK_RESULT(3, osal_cmn_fifo_len(&fifo) == 0, osal_cmn_fifo_len(&fifo));
	CHECK_RESULT(4, len == copied, copied);

	return 0;
}
static int32_t test_osal_cmn_fifo_initialized()
{
	osal_cmn_fifo_init(&fifo, fifo_buf, sizeof(fifo_buf));
	CHECK_RESULT(1, osal_cmn_fifo_initialized(&fifo) == true, false);
	return 0;
}
static int32_t test_osal_cmn_fifo_reset(void)
{
	osal_cmn_fifo_init(&fifo, fifo_buf, sizeof(fifo_buf));
	osal_cmn_fifo_in(&fifo, inbuf, sizeof(inbuf));
	osal_cmn_fifo_reset(&fifo);
	CHECK_RESULT(1, osal_cmn_fifo_len(&fifo) == 0, 0);
	CHECK_RESULT(2, osal_cmn_fifo_is_empty(&fifo) == true, 0);

	return 0;
}
static int32_t test_osal_cmn_fifo_len(void)
{
	return test_osal_cmn_fifo_in();
}
static int32_t test_osal_cmn_fifo_is_empty(void)
{
	return test_osal_cmn_fifo_reset();
}
static int32_t test_osal_cmn_fifo_is_empty_spinlocked()
{
	osal_cmn_fifo_reset(&fifo);
	CHECK_RESULT(1, osal_cmn_fifo_is_empty_spinlocked(&fifo, &lock) == true, 0);

	return 0;
}

static int32_t test_osal_cmn_fifo_is_full(void)
{
	int32_t inlen = sizeof(inbuf);

	while(osal_cmn_fifo_in(&fifo, inbuf, inlen) > 0);

	CHECK_RESULT(1, osal_cmn_fifo_is_full(&fifo) == true, osal_cmn_fifo_len(&fifo));
	osal_cmn_fifo_reset(&fifo);

	return 0;
}

static int32_t test_osal_cmn_fifo_in_spinlocked(void)
{
	int32_t tmp;
	int32_t ret;
	uint8_t buf[55];

	tmp = osal_cmn_fifo_len(&fifo);
	ret = osal_cmn_fifo_in_spinlocked(&fifo, buf, sizeof(buf), &lock);
	CHECK_RESULT(1, (tmp + ret) == osal_cmn_fifo_len(&fifo), ret);

	return 0;
}
static int32_t test_osal_cmn_fifo_out_spinlocked(void)
{
	int32_t tmp;
	int32_t ret;
	uint8_t buf[55];

	tmp = osal_cmn_fifo_len(&fifo);
	ret = osal_cmn_fifo_out_spinlocked(&fifo, buf, sizeof(buf), &lock);
	CHECK_RESULT(1, (sizeof(buf) - tmp) == osal_cmn_fifo_len(&fifo), ret);

	return 0;
}


#define TEST_LOCKED 0
void *thread_func_1(void *arg) {
    int32_t thread_id = *(int32_t *)arg;
	int32_t i, ret;
	int32_t inlen = sizeof(inbuf);

    printf("Thread %d is executing in function 1.\n", thread_id);
    for(i = 0; i < 3*TEST_FIFO_SIZE/inlen; i++) {
		memset(inbuf, i%128, sizeof(inbuf));
#if TEST_LOCKED
		ret = osal_cmn_fifo_in_spinlocked(&fifo, inbuf, inlen, &lock);
#else
		ret = osal_cmn_fifo_in(&fifo, inbuf, inlen);
#endif
		usleep(10);
		if (ret == 0) {
			i--;
			usleep(1000);
		}
	}

    return NULL;
}

void *thread_func_2(void *arg) {
    int32_t thread_id = *(int32_t *)arg;
	int32_t i;
	int32_t outlen = sizeof(inbuf);
	uint8_t tmpbuf[TEST_BUF_SIZE];
	int32_t ret;

    printf("Thread %d is executing in function 1.\n", thread_id);
	while(osal_cmn_fifo_len(&fifo) == 0) {
		usleep(1000);
	}
    for(i = 0; i < 3*TEST_FIFO_SIZE/outlen; i++) {
		memset(tmpbuf, i%128, TEST_BUF_SIZE);
#if TEST_LOCKED
		ret = osal_cmn_fifo_out_spinlocked(&fifo, outbuf, outlen, &lock);
#else
		ret = osal_cmn_fifo_out(&fifo, outbuf, outlen);
#endif
		ret = memcmp(outbuf, tmpbuf, outlen);
		CHECK_RESULT(i, ret == 0, ret);
		usleep(10);
		if (ret == 0) {
			usleep(1000);
		}
	}

    return NULL;
}

int32_t test_single_thread(void)
{
	test_osal_cmn_fifo_init();
	test_osal_cmn_fifo_alloc();
	test_osal_cmn_fifo_free();
	test_osal_cmn_fifo_in();
	test_osal_cmn_fifo_out();
	test_osal_cmn_fifo_copy_from_to_user();
	test_osal_cmn_fifo_initialized();
	test_osal_cmn_fifo_reset();
	test_osal_cmn_fifo_len();
	test_osal_cmn_fifo_is_empty();
	test_osal_cmn_fifo_is_full();
	test_osal_cmn_fifo_in_spinlocked();
	test_osal_cmn_fifo_out_spinlocked();
	test_osal_cmn_fifo_is_empty_spinlocked();

	return 0;
}
int32_t test_multiple_thread(void)
{
	pthread_t thread1, thread2;
    int32_t thread_id1 = 1;
    int32_t thread_id2 = 2;
	memset(&fifo, 0, sizeof(fifo));
	osal_cmn_fifo_alloc(&fifo, TEST_FIFO_SIZE, OSAL_KMALLOC_KERNEL);

    if (pthread_create(&thread1, NULL, thread_func_1, &thread_id1) != 0) {
        perror("pthread_create for thread 1");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&thread2, NULL, thread_func_2, &thread_id2) != 0) {
        perror("pthread_create for thread 2");
        exit(EXIT_FAILURE);
    }

    if (pthread_join(thread1, NULL) != 0) {
        perror("pthread_join for thread 1");
        exit(EXIT_FAILURE);
    }

    if (pthread_join(thread2, NULL) != 0) {
        perror("pthread_join for thread 2");
        exit(EXIT_FAILURE);
    }

    printf("Both threads have finished.\n");

	return 0;
}

int32_t main()
{
	int32_t ret;

	pthread_spin_init(&lock, 0);
	pthread_mutex_init(&mutex, 0);

	ret = test_single_thread();
	ret = test_multiple_thread();

	CHECK_FINAL_RESULT();

    return ret;
}
