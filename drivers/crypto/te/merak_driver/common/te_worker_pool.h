//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include "te_worker.h"
#ifndef __TRUSTENGINE_WORKER_POOL_H__
#define __TRUSTENGINE_WORKER_POOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * \brief           Create global worker pool.
 *
 * \return          \c None.
 */
int te_worker_pool_create(void);

/**
 * \brief               This function enqueue a task to worker pool.
 *
 * \param[int] task     The task instance.
 * \return              \c None
 */
void te_worker_pool_enqueue(te_worker_task_t *task);

/**
 * \brief               Destroy global worker pool.
 *
 * \return              \c None
 */
void te_worker_pool_destroy(void);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_COMMON_H__ */
