//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include "osal_mutex.h"
#include "osal_log.h"
#include "osal_assert.h"

typedef struct mutex_ctx {
    struct mutex mutex;
} mutex_ctx_t;

osal_err_t osal_mutex_create(osal_mutex_t *mutex)
{
    mutex_ctx_t *ctx = NULL;

    OSAL_ASSERT(mutex != NULL);
    ctx = (mutex_ctx_t *)kmalloc(sizeof(mutex_ctx_t), GFP_KERNEL);
    OSAL_ASSERT(ctx != NULL);

    mutex_init(&ctx->mutex);
    *mutex = (osal_mutex_t *)ctx;
    return OSAL_SUCCESS;
}

void osal_mutex_lock(osal_mutex_t mutex)
{
    int ret = 0;
    mutex_ctx_t *ctx = (mutex_ctx_t *)mutex;

    OSAL_ASSERT(ctx != NULL);
    while (1) {
        ret = mutex_lock_interruptible(&ctx->mutex);
        if (-EINTR != ret) {
            break;
        }
    }
    return;
}

void osal_mutex_unlock(osal_mutex_t mutex)
{
    mutex_ctx_t *ctx = (mutex_ctx_t *)mutex;

    OSAL_ASSERT(ctx != NULL);
    mutex_unlock(&ctx->mutex);
    return;
}

void osal_mutex_destroy(osal_mutex_t mutex)
{
    mutex_ctx_t *ctx = (mutex_ctx_t *)mutex;

    OSAL_ASSERT(ctx != NULL);
    kfree(ctx);
    return;
}
