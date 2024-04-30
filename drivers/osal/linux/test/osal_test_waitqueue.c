#include "osal.h"
#include "osal_linux_test.h"
#include <linux/sched/signal.h>

osal_waitqueue_t wq;
enum {
    TEST_WAIT_COND_DEFAULT = 1,
    TEST_WAIT_COND_INTERRUPTILE,
    TEST_WAIT_COND_TIMEOUT,
    TEST_WAIT_COND_INTERRUPTILE_TIMEOUT,
    TEST_WAIT_COND_INVAL,
} ;
int osal_wait_test_flag = TEST_WAIT_COND_INVAL;
#define THREAD_NUM 16


static int32_t thread_func_default(void *data)
{
    int32_t thread_id = *(int32_t *)data;

    osal_pr_err("thread_func_default thread %d waiting... condition:%d\n",
            thread_id, osal_wait_test_flag == TEST_WAIT_COND_DEFAULT);
    osal_wait_event(wq, osal_wait_test_flag == TEST_WAIT_COND_DEFAULT);
    osal_pr_err("thread_func_default thread %d exit       condition:%d\n",
            thread_id, osal_wait_test_flag == TEST_WAIT_COND_DEFAULT);

    return 0;
}

static int32_t thread_func_interruptibe(void *data)
{
    int32_t thread_id = *(int32_t *)data;
    int64_t ret = 0;

    ret = osal_wait_event_interruptible(wq,
            osal_wait_test_flag == TEST_WAIT_COND_INTERRUPTILE);
    osal_wait_test_flag = TEST_WAIT_COND_INVAL;
    osal_pr_err("thread %d exited, ret:%lld\n", thread_id, ret);

    return 0;
}

static int32_t thread_func_timeout(void *data)
{
    int32_t thread_id = *(int32_t *)data;
    int64_t ret = 0;

    ret = osal_wait_event_timeout(wq,
            osal_wait_test_flag == TEST_WAIT_COND_TIMEOUT, 1000);

    osal_pr_err("thread %d exited, ret:%lld\n", thread_id, ret);

    return 0;
}

static int32_t thread_func_interruptible_timeout(void *data)
{
    int32_t thread_id = *(int32_t *)data;
    int64_t ret = 0;

    ret = osal_wait_event_interruptible_timeout(wq,
            osal_wait_test_flag == TEST_WAIT_COND_INTERRUPTILE_TIMEOUT, 1000);

    osal_pr_err("thread %d exited, ret:%lld\n", thread_id, ret);

    return 0;
}

void test_osal_waitqueue(void)
{
    osal_thread_t thread[THREAD_NUM];
    int thread_id[THREAD_NUM];
    int i;

    osal_wait_test_flag = TEST_WAIT_COND_INVAL;
    osal_pr_err("start test osal waitqueue\n");
    osal_waitqueue_init(&wq);

    osal_pr_err("------ test default ---------\n");
    for (i = 0; i < 4; i++) {
        thread_id[i] = i;
        osal_thread_init(&thread[i], NULL, thread_func_default, &thread_id[i]);
        osal_thread_start(&thread[i]);
    }
    msleep(10);

    osal_wait_test_flag = TEST_WAIT_COND_DEFAULT;
    osal_pr_err("---- wakeup a thread\n");
    osal_wake_up(&wq);
    msleep(100);

    osal_pr_err("---- wakeup all ,osal_wait_test_flag:%d, condition:%d\n",
            osal_wait_test_flag, osal_wait_test_flag == TEST_WAIT_COND_DEFAULT);
    osal_wake_up_all(&wq);
    msleep(100);

    osal_pr_err("------ test interruptible ---------\n");
    for (; i < 6; i++) {
        osal_pr_err("------ create thread %d ---------\n", i);
        thread_id[i] = i;
        osal_thread_init(&thread[i], NULL, thread_func_interruptibe, &thread_id[i]);
        osal_pr_err("------ create thread %d task:0x%llx---------\n", i, (uint64_t)thread[i].ptask);
        osal_thread_start(&thread[i]);
    }

    osal_wait_test_flag = TEST_WAIT_COND_INTERRUPTILE;
    osal_pr_err("---- wakeup a thread\n");
    osal_wake_up(&wq);
    msleep(100);

    osal_wait_test_flag = TEST_WAIT_COND_INVAL;
    osal_pr_err("---- interrupt a process\n");
    send_sig(SIGTERM, thread[i-1].ptask, 0);
    wake_up_process(thread[i-1].ptask);

    osal_pr_err("------ test timeout ---------\n");
    for (; i < 8; i++) {
        thread_id[i] = i;
        osal_thread_init(&thread[i], NULL, thread_func_timeout, &thread_id[i]);
        osal_thread_start(&thread[i]);
    }

    osal_wait_test_flag = TEST_WAIT_COND_TIMEOUT;
    osal_pr_err("---- wakeup a thread\n");
    osal_wake_up(&wq);
    msleep(100);

    osal_pr_err("---- wait for timeout \n");
    osal_msleep(1100);


    osal_pr_err("------ test interruptible timeout ---------\n");
    for (; i < 11; i++) {
        thread_id[i] = i;
        osal_thread_init(&thread[i], NULL, thread_func_interruptible_timeout, &thread_id[i]);
        osal_thread_start(&thread[i]);
    }

    osal_wait_test_flag = TEST_WAIT_COND_INTERRUPTILE_TIMEOUT;
    osal_pr_err("---- wakeup a thread\n");
    osal_wake_up(&wq);
    msleep(100);

    osal_pr_err("---- interrupt a thread \n");

    send_sig(SIGTERM, thread[i-2].ptask, 0);
    msleep(100);

    osal_pr_err("---- wait for timeout \n");
    osal_msleep(1100);

}
