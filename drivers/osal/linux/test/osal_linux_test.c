/*
 * Copyright 2022, Horizon Robotics
 */

#include <linux/module.h>
#include <osal.h>

#include "osal_linux_test.h"

int32_t failed_num;
static int32_t __init osal_linux_init(void)
{
	osal_pr_info("%s\n", __func__);
	test_osal_alloc();
	test_osal_atomic();
	test_osal_bitops();
	test_osal_cache();
	test_osal_complete();
	test_osal_sem();
	test_osal_debug();
	test_osal_fifo();
	test_osal_io();
	test_osal_list();
	test_osal_lock();
	test_osal_time();
	test_osal_thread();
	test_osal_waitqueue();

	CHECK_FINAL_RESULT()

	return 0;
}

void __exit osal_linux_exit(void)
{
	osal_pr_info("%s\n", __func__);
}

// OSAL module should be loaded before others
module_init(osal_linux_init);
module_exit(osal_linux_exit);

MODULE_AUTHOR("Haipeng Yu<haipeng.yu@horizon.cc>");
MODULE_DESCRIPTION("Horizon osal Linux driver");
MODULE_LICENSE("GPL v2");
