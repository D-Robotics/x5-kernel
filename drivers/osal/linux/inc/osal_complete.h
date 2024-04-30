/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_COMPLETE_H__
#define __OSAL_COMPLETE_H__

#include <linux/completion.h>
#include <linux/jiffies.h>

typedef struct completion osal_complete_t;
/**
 * @brief initialise a complete
 *
 * @param[in] cmpl complete instance to be initialized
 *
 * @return 0 on success, an error code on failure.
 */
static inline int32_t osal_complete_init(osal_complete_t *cmpl)
{
	init_completion(cmpl);

	return 0;
}

/**
 * @brief deinit a complete
 *
 * @param[in] cmpl complete instance
 *
 * @return 0 on success, an error code on failure.
 */
static inline int32_t osal_complete_deinit(osal_complete_t *cmpl)
{
	return 0;
}

/**
 * @brief wait on a complete
 *
 * @param[in] cmpl complete instance
 *
 * @return 0 on success, an error code on failure.
 */
static inline int32_t osal_complete_wait_interruptible(osal_complete_t *cmpl)
{
	return wait_for_completion_interruptible(cmpl);
}

/**
 * @brief wait a condition for timeout msecs
 *
 * @param[in] cmpl complete instance
 * @param[in] timeout wait for timeout in millseconds
 *
 * @return  -ERESTARTSYS if interrupted,
 *          0 if timed out,
 *          positive (at least 1, or number of msecs left till timeout) if completed.
 */
static inline int32_t osal_complete_wait_interruptible_timeout(osal_complete_t *cmpl, uint32_t timeout_ms)
{
	int32_t ret;
	ret = wait_for_completion_interruptible_timeout(cmpl, msecs_to_jiffies(timeout_ms));
	if (ret > 0)
		ret = jiffies_to_msecs(ret);

	return ret;
}

/**
 * @brief wake up a process waiting on the completion object
 *
 * @param[in] cmpl complete instance
 */
static inline void osal_complete(osal_complete_t *cmpl)
{
	complete(cmpl);
}

#endif
