/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_DEBUG_H__
#define __OSAL_DEBUG_H__

/**
 * @brief print emergency log
 *
 * @param[in] fmt log format
 */
#define osal_pr_emerg(fmt, ...) pr_emerg(fmt, ##__VA_ARGS__)

 /**
  * @brief print error log
  *
  * @param[in] fmt log format
  */
#define osal_pr_err(fmt, ...) pr_err(fmt, ##__VA_ARGS__)

 /**
  * @brief print warning log
  *
  * @param[in] fmt log format
  */
#define osal_pr_warn(fmt, ...) pr_warn(fmt, ##__VA_ARGS__)

 /**
  * @brief print info log
  *
  * @param[in] fmt log format
  */
#define osal_pr_info(fmt, ...) pr_info(fmt, ##__VA_ARGS__)

 /**
  * @brief print debug log, support dynamic control
  *
  * @param[in] fmt log format
  */
#define osal_pr_debug(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

 /**
  * @brief print debug log with default loglevel
  *
  * @param[in] fmt log format
  */
#define osal_print(fmt, ...) printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)

 /**
  * @brief assert if expression is false
  *
  * @param[in] expression if expression is false, assert it
  */
#define OSAL_ASSERT(assert) \
	do { \
		if (unlikely(!(assert))) { \
			pr_emerg("Assertion failure in %s() at %s:%d: '%s'\n", \
				__func__, __FILE__, __LINE__, #assert); \
			BUG(); \
		} \
	} while (0)


#endif