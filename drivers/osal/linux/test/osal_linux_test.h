#ifndef __OSAL_LINUX_TEST_H__
#define __OSAL_LINUX_TEST_H__

#include <linux/string.h>

extern int32_t failed_num;
#define CHECK_RESULT(id, cond, ret) \
	if (!(cond)) { \
		failed_num++; \
		osal_pr_err("%s %d fail, ret:0x%lx\n", __func__, (id), (unsigned long)(ret)); \
	} else { \
		osal_pr_info("%s %d pass, ret:0x%lx\n", __func__, (id), (unsigned long)(ret)); \
	}

#define CHECK_FINAL_RESULT() \
	if (failed_num == 0) \
		osal_pr_info("ALL PASS!\n"); \
	else \
		osal_pr_err("%d Failed\n", failed_num);

#endif

extern void test_osal_alloc(void);
extern void test_osal_atomic(void);
extern void test_osal_bitops(void);
extern void test_osal_cache(void);
extern void test_osal_complete(void);
extern void test_osal_sem(void);
extern void test_osal_debug(void);
extern void test_osal_fifo(void);
extern void test_osal_io(void);
extern void test_osal_list(void);
extern void test_osal_lock(void);
extern void test_osal_time(void);
extern void test_osal_thread(void);
extern void test_osal_waitqueue(void);