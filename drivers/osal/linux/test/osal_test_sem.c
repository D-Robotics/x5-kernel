
#include "osal.h"
#include "osal_linux_test.h"
#include <linux/sched/signal.h>

static osal_sem_t g_sem_sender;
static osal_sem_t g_sem_th;
static int g_done;

static int32_t thread_func_1(void *data) {
	int32_t thread_id = *(int32_t *)data;
	int32_t i, ret;

	osal_pr_info("Thread %d is executing in function 1.\n", thread_id);
	for(i = 0; i < 11; i++) {
		ret = osal_sema_down_timeout(&g_sem_th, 300);
		osal_pr_info("Thread %d osal_sema_down_timeout %d, ret:%d\n", thread_id, i, ret);
		ret = osal_sema_down_interruptible(&g_sem_th);
		osal_pr_info("Thread %d osal_sema_down_interruptible %d, ret:%d\n", thread_id, i, ret);
	}

	g_done++;
	return 0;
}

static int32_t thread_func_2(void *data) {
	int32_t thread_id = *(int32_t *)data;
	int32_t i;

	osal_sema_down_interruptible(&g_sem_sender);

	osal_pr_info("Thread %d is executing in function 1.\n", thread_id);
	for(i = 0; i < 10; i++) {
		osal_pr_info("Thread %d osal_complate %d\n", thread_id, i);
		osal_sema_up(&g_sem_th);
		osal_sema_up(&g_sem_th);
		osal_msleep(10);
	}

	g_done++;
	return 0;
}


static int32_t test_multiple_thread(void)
{
	osal_thread_t thread1, thread2;
	int32_t thread_id1 = 1;
	int32_t thread_id2 = 2;
	int max_waiting_100ms = 20;

	osal_sema_init(&g_sem_sender, 0);
	osal_sema_init(&g_sem_th, 0);


	if (osal_thread_init(&thread1, NULL, thread_func_1, &thread_id1) != 0) {
		osal_pr_info("create thread 1 failed\n");
		return -ENODEV;
	}
	if (osal_thread_init(&thread2, NULL, thread_func_2, &thread_id2) != 0) {
		osal_pr_info("create thread 2 failed\n");
		return -ENODEV;
	}

	osal_sema_up(&g_sem_sender);

	osal_thread_start(&thread1);
	osal_thread_start(&thread2);

	osal_msleep(2000);
	osal_sema_up(&g_sem_sender);


	while(g_done < 2 && max_waiting_100ms-- > 0)
		osal_msleep(100);

	if (max_waiting_100ms <= 0)
		send_sig(SIGTERM, thread1.ptask, 0);

	osal_pr_info("Both threads have finished.\n");

	return 0;
}



void test_osal_sem(void)
{
	test_multiple_thread();
}

