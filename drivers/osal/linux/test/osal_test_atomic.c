#include "osal.h"
#include "osal_linux_test.h"
#include <linux/stddef.h>

static osal_atomic_t tval;

void teat_osal_atomic_set(void)
{
	osal_atomic_set(&tval, 1);
	CHECK_RESULT(1, osal_atomic_read(&tval) == 1, osal_atomic_read(&tval));
	osal_atomic_set(&tval, ~0);
	CHECK_RESULT(2, osal_atomic_read(&tval) == ~0, osal_atomic_read(&tval));
	osal_atomic_set(&tval, 0);
}

void teat_osal_atomic_inc(void)
{
	osal_atomic_set(&tval, 1);
	osal_atomic_inc(&tval);
	CHECK_RESULT(1, osal_atomic_read(&tval) == 2, osal_atomic_read(&tval));
	osal_atomic_set(&tval, 0);
}
void teat_osal_atomic_dec(void)
{
	osal_atomic_set(&tval, 0xf1);
	osal_atomic_dec(&tval);
	CHECK_RESULT(1, osal_atomic_read(&tval) == 0xf0, osal_atomic_read(&tval));
	osal_atomic_set(&tval, 0);
}

void teat_osal_atomic_add(void)
{
	osal_atomic_set(&tval, 0xf7);
	osal_atomic_add(8, &tval);
	CHECK_RESULT(1, osal_atomic_read(&tval) == 0xff, osal_atomic_read(&tval));

	osal_atomic_set(&tval, 0xf7);
	osal_atomic_add(-7, &tval);
	CHECK_RESULT(2, osal_atomic_read(&tval) == 0xf0, osal_atomic_read(&tval));
	osal_atomic_set(&tval, 0);
}

void teat_osal_atomic_read(void)
{
	osal_atomic_set(&tval, 0xf2);
	CHECK_RESULT(1, osal_atomic_read(&tval) == 0xf2, osal_atomic_read(&tval));
	osal_atomic_set(&tval, 0);
}
void teat_osal_atomic_dec_return(void)
{
	int32_t ret;
	osal_atomic_set(&tval, 0xf2);
	ret = osal_atomic_dec_return(&tval);
	CHECK_RESULT(1, osal_atomic_read(&tval) == 0xf1, osal_atomic_read(&tval));
	osal_atomic_set(&tval, 0);
}
void teat_osal_atomic_inc_return(void)
{
	int32_t ret;
	osal_atomic_set(&tval, 0xf2);
	ret = osal_atomic_inc_return(&tval);
	CHECK_RESULT(1, osal_atomic_read(&tval) == 0xf3, osal_atomic_read(&tval));
	osal_atomic_set(&tval, 0);
}


#define CNT_INC_THREAD_1 1024000
#define CNT_INC_THREAD_2 1023000
#define TEST_NON_ATOMIC 0
#if TEST_NON_ATOMIC
long nval;
#endif
static int32_t done = 0;
static int32_t thread_func_1(void *data) {
    int32_t thread_id = *(int32_t *)data;
	int32_t i;

    osal_pr_info("Thread %d start to run.\n", thread_id);
    for(i = 0; i < CNT_INC_THREAD_1; i++) {
#if TEST_NON_ATOMIC
		nval++;
#else
		osal_atomic_inc(&tval);
#endif
	}
    osal_pr_info("Thread %d done.\n", thread_id);

	done++;
    return 0;
}

static int32_t thread_func_2(void *data) {
    int32_t thread_id = *(int32_t *)data;
	int32_t i;

    osal_pr_info("Thread %d start to run.\n", thread_id);
    for(i = 0; i < CNT_INC_THREAD_2; i++) {
#if TEST_NON_ATOMIC
		nval++;
#else
		osal_atomic_inc(&tval);
#endif
	}
    osal_pr_info("Thread %d done.\n", thread_id);

	done++;
    return 0;
}

static int32_t test_multiple_thread(void)
{
    osal_thread_t thread1, thread2;
    int32_t thread_id1 = 1;
    int32_t thread_id2 = 2;

    osal_atomic_set(&tval, 0);

    if (osal_thread_init(&thread1, NULL, thread_func_1, &thread_id1) != 0) {
        osal_pr_err("create thread 1 failed\n");
		return -1;
    }

    if (osal_thread_init(&thread2, NULL, thread_func_2, &thread_id2) != 0) {
        osal_pr_err("create thread 2 failed\n");
        return -1;
    }

	osal_thread_start(&thread1);
	osal_thread_start(&thread2);

	while(done < 2)
		osal_msleep(10);

    osal_pr_info("Both threads have finished.\n");
#if TEST_NON_ATOMIC
	CHECK_RESULT(1, nval == (CNT_INC_THREAD_1 + CNT_INC_THREAD_2), nval);
#else
	CHECK_RESULT(1, osal_atomic_read(&tval) == (CNT_INC_THREAD_1 + CNT_INC_THREAD_2), osal_atomic_read(&tval));
#endif

	return 0;
}


void test_osal_atomic(void)
{
	teat_osal_atomic_set();
	teat_osal_atomic_inc();
	teat_osal_atomic_dec();
	teat_osal_atomic_add();
	teat_osal_atomic_read();
	teat_osal_atomic_dec_return();
	teat_osal_atomic_inc_return();
	test_multiple_thread();
}