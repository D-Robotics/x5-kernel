//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <te_common.h>
#include "te_worker_pool.h"

#ifndef CFG_TE_NUM_WORKERS
#define CFG_TE_NUM_WORKERS (8UL)
#endif

#if defined(CFG_TE_ASYNC_EN)

#if CFG_TE_NUM_WORKERS < (1L)
#error "CFG_TE_NUM_WORKERS must larger or equal 1"
#endif

typedef struct te_worker_pool {
    osal_spin_lock_t lock;
    te_worker_t *workers[CFG_TE_NUM_WORKERS];
    uint32_t index;
} te_worker_pool_t;

static struct te_worker_pool g_te_worker_pool  = { 0 };
#endif /* CFG_TE_ASYNC_EN */

int te_worker_pool_create(void)
{
    int ret = TE_SUCCESS;
#ifdef CFG_TE_ASYNC_EN
    int i = 0;
    te_worker_pool_t *pool = &g_te_worker_pool;

    /* Reset index to 0 */
    pool->index = 0;

    ret = osal_spin_lock_init(&pool->lock);
    if (ret != OSAL_SUCCESS) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    for (i = 0; i < CFG_TE_NUM_WORKERS; i++) {
        pool->workers[i] = te_worker_init();
        if (pool->workers[i] == NULL) {
            ret = TE_ERROR_OOM;
            goto err2;
        }
    }

    return TE_SUCCESS;

err2:
    for (i = 0; i < CFG_TE_NUM_WORKERS; i++) {
        if (pool->workers[i] != NULL) {
            te_worker_quit(pool->workers[i]);
            pool->workers[i] = NULL;
        }
    }

    osal_spin_lock_destroy(&pool->lock);

err1:
#endif /* CFG_TE_ASYNC_EN */
    return ret;
}

void te_worker_pool_destroy(void)
{
#ifdef CFG_TE_ASYNC_EN
    int i = 0;
    te_worker_pool_t *pool = &g_te_worker_pool;

    /* Reset index to 0 */
    pool->index = 0;

    for (i = 0; i < CFG_TE_NUM_WORKERS; i++) {
        if (pool->workers[i] != NULL) {
            te_worker_quit(pool->workers[i]);
            pool->workers[i] = NULL;
        }
    }

    osal_spin_lock_destroy(&pool->lock);
#endif /* CFG_TE_ASYNC_EN */
    return;
}

void te_worker_pool_enqueue(te_worker_task_t *task)
{
#ifdef CFG_TE_ASYNC_EN
    unsigned long flags = 0;
    int i = 0, cur = 0;
    te_worker_st_t st = { 0 };
    te_worker_pool_t *pool = &g_te_worker_pool;

    TE_ASSERT(task != NULL);
    osal_spin_lock_irqsave(&pool->lock, &flags);

    for (i = 0; i < CFG_TE_NUM_WORKERS; i++) {
        cur = (pool->index + i) % CFG_TE_NUM_WORKERS;
        st = te_worker_state(pool->workers[cur]);
        if (st == TEWORKER_ST_SLEEPING) {
            break;
        }
    }
    pool->index = (pool->index + 1) % CFG_TE_NUM_WORKERS;
    osal_spin_unlock_irqrestore(&pool->lock, flags);
    te_worker_enqueue(pool->workers[cur], task);

#else  /* CFG_TE_ASYNC_EN */
    (void)task;
#endif /* !CFG_TE_ASYNC_EN */
    return;
}

