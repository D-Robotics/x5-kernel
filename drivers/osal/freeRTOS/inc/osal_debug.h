/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_DEBUG_H__
#define __OSAL_DEBUG_H__

#define osal_pr_emerg(fmt, ...)
#define osal_pr_err(fmt, ...)
#define osal_pr_warn(fmt, ...)
#define osal_pr_info(fmt, ...)
#define osal_pr_debug(fmt, ...)
#define osal_print(fmt, ...)
#define OSAL_ASSERT(assert) \
	do { \
		if (unlikely(!(assert))) { \
			osal_pr_emerg("Assertion failure in %s() at %s:%d: '%s'\n", \
				__func__, __FILE__, __LINE__, #assert); \
		} \
	} while (1)


#endif