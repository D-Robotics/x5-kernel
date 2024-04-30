//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <sqlist.h>
#ifndef __TRUSTENGINE_WORKER_H__
#define __TRUSTENGINE_WORKER_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

typedef enum {
    TEWORKER_ST_STOPPED = 0,
    TEWORKER_ST_RUNNING = 1,
    TEWORKER_ST_SLEEPING = 2,
    TEWORKER_ST_INVALID
} te_worker_st_t;

/**
 * TrustEngine worker thread structure
 */
typedef struct te_worker {
    osal_thread_t wthread;
    osal_spin_lock_t lock;
    osal_completion_t bell;
    volatile uint32_t command;
    volatile te_worker_st_t state;
    sqlist_t tasks;
} te_worker_t;

/**
 * Worker thread task item
 */
typedef struct te_worker_task {
    sqlist_t list;
    void *param;                                    /**< task parameter */
    void(*execute)(struct te_worker_task *task);    /**< task entry */
} te_worker_task_t;

/**
 * \brief           Create a worker thread instance.
 *
 * \return          \c worker ptr on success.
 * \return          \c NULL on failure.
 */
te_worker_t *te_worker_init(void);

/**
 * \brief               This function enqueue a task to worker thread.
 *
 * \param[in] worker    The worker thread instance.
 * \param[int] task     The task instance.
 * \return              \c None
 */
void te_worker_enqueue( te_worker_t *worker, te_worker_task_t *task );

/**
 * \brief               This function report worker thread state.
 *
 * \param[in] worker    The worker thread instance.
 * \return              \c worker state.
 */
te_worker_st_t te_worker_state( te_worker_t *worker );

/**
 * \brief               This function notify worker thread to quit.
 *
 * \param[in] worker    The worker thread instance.
 * \return              \c None
 */
void te_worker_quit( te_worker_t *worker );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_COMMON_H__ */
