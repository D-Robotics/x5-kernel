#include "osal_common_test.h"
#include "osal_common_atomic.h"

static osal_atomic_t tval;
static osal_spinlock_t lock;
static int32_t failed_num;

void test_osal_cmn_atomic_set(void)
{
	osal_cmn_atomic_set(&tval, 1);
	CHECK_RESULT(1, tval == 1, tval);
	osal_cmn_atomic_set(&tval, ~0);
	CHECK_RESULT(2, (unsigned long)tval == ~0, tval);
	osal_cmn_atomic_set(&tval, 0);
}

void test_osal_cmn_atomic_inc(void)
{
	osal_cmn_atomic_set(&tval, 1);
	osal_cmn_atomic_inc(&tval);
	CHECK_RESULT(1, tval == 2, tval);
	osal_cmn_atomic_set(&tval, 0);
}
void test_osal_cmn_atomic_dec(void)
{
	osal_cmn_atomic_set(&tval, 0xf1);
	osal_cmn_atomic_dec(&tval);
	CHECK_RESULT(1, tval == 0xf0, tval);
	osal_cmn_atomic_set(&tval, 0);
}

void test_osal_cmn_atomic_read(void)
{
	osal_cmn_atomic_set(&tval, 0xf2);
	CHECK_RESULT(1, tval == 0xf2, tval);
	osal_cmn_atomic_set(&tval, 0);
}
void test_osal_cmn_atomic_dec_return(void)
{
	int32_t ret;
	osal_cmn_atomic_set(&tval, 0xf2);
	ret = osal_cmn_atomic_dec_return(&tval);
	CHECK_RESULT(1, ret == 0xf1, tval);
	osal_cmn_atomic_set(&tval, 0);
}
void test_osal_cmn_atomic_inc_return(void)
{
	int32_t ret;
	osal_cmn_atomic_set(&tval, 0xf2);
	ret = osal_cmn_atomic_inc_return(&tval);
	CHECK_RESULT(1, ret == 0xf3, tval);
	osal_cmn_atomic_set(&tval, 0);
}


#define CNT_INC_THREAD_1 10240000
#define CNT_INC_THREAD_2 10230000
#define TEST_NON_ATOMIC 1
#if TEST_NON_ATOMIC
long nval;
#endif
static void *thread_func_1(void *arg) {
    int32_t thread_id = *(int32_t *)arg;
	int32_t i;

    printf("Thread %d start to run.\n", thread_id);
    for(i = 0; i < CNT_INC_THREAD_1; i++) {
#if TEST_NON_ATOMIC
		nval++;
#else
		osal_cmn_atomic_inc(&tval);
#endif
	}
    printf("Thread %d done.\n", thread_id);

    return NULL;
}

static void *thread_func_2(void *arg) {
    int32_t thread_id = *(int32_t *)arg;
	int32_t i;

    printf("Thread %d start to run.\n", thread_id);
    for(i = 0; i < CNT_INC_THREAD_2; i++) {
#if TEST_NON_ATOMIC
		nval++;
#else
		osal_cmn_atomic_inc(&tval);
#endif
	}
    printf("Thread %d done.\n", thread_id);

    return NULL;
}

int32_t test_multiple_thread(void)
{
	pthread_t thread1, thread2;
    int32_t thread_id1 = 1;
    int32_t thread_id2 = 2;

	osal_cmn_atomic_set(&tval, 0);

    if (pthread_create(&thread1, NULL, thread_func_1, &thread_id1) != 0) {
        perror("pthread_create for thread 1");
        exit(-1);
    }

    if (pthread_create(&thread2, NULL, thread_func_2, &thread_id2) != 0) {
        perror("pthread_create for thread 2");
        exit(-1);
    }

    if (pthread_join(thread1, NULL) != 0) {
        perror("pthread_join for thread 1");
        exit(-1);
    }

    if (pthread_join(thread2, NULL) != 0) {
        perror("pthread_join for thread 2");
        exit(-1);
    }

    printf("Both threads have finished.\n");
#if TEST_NON_ATOMIC
	CHECK_RESULT(1, nval == (CNT_INC_THREAD_1 + CNT_INC_THREAD_2), nval);
#else
	CHECK_RESULT(1, osal_cmn_atomic_read(&tval) == (CNT_INC_THREAD_1 + CNT_INC_THREAD_2), osal_cmn_atomic_read(&tval));
#endif

	return 0;
}


int32_t main(int32_t argc, char *argv[])
{
	test_osal_cmn_atomic_set();
	test_osal_cmn_atomic_inc();
	test_osal_cmn_atomic_dec();
	test_osal_cmn_atomic_read();
	test_osal_cmn_atomic_dec_return();
	test_osal_cmn_atomic_inc_return();
	test_multiple_thread();

	CHECK_FINAL_RESULT();

	return 0;
}