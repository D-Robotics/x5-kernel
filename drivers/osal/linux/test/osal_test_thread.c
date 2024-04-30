#include "osal.h"
#include "osal_linux_test.h"

osal_thread_t thread1;
osal_thread_t thread2;

static int32_t thread1_func(void *data)
{
	int32_t thread_id = *(int32_t *)data;
	int32_t i = 0;
	while (!osal_thread_should_stop()) {
		osal_pr_info("thread %d loop %d\n", thread_id, i++);
		osal_msleep(10);
	}

	osal_pr_info("thread %d exited\n", thread_id);

	return 0;
}

void test_osal_thread(void)
{
	int32_t thread1_id = 1;
	int32_t thread2_id = 2;
	osal_thread_attr_t attr;
	osal_thread_init(&thread1, NULL, thread1_func, &thread1_id);
	osal_thread_start(&thread1);

	memset(&attr, 0, sizeof(attr));
	attr.sched_policy = OSAL_SCHED_FIFO;
	attr.priority = 12;
	strncpy(attr.name, "osal_test_thread", sizeof(attr.name));
	osal_thread_init(&thread2, &attr, thread1_func, &thread2_id);
	osal_thread_start(&thread2);

	osal_msleep(100);

	osal_thread_destory(&thread1);
	osal_thread_destory(&thread2);
}
