//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include "osal_log.h"
#include "osal_assert.h"
#include "osal_thread.h"
#include "osal_utils.h"

typedef struct th_ctx {
    struct task_struct *task;
    osal_thread_entry_t entry;
    volatile uint32_t stop;
    void *arg;
} th_ctx_t;

static int th_entry(void *arg)
{
    int ret       = -1;
    th_ctx_t *ctx = (th_ctx_t *)arg;

    ret       = ctx->entry(ctx->arg);
    ctx->stop = 1;
    return ret;
}

osal_err_t osal_thread_create(osal_thread_t *thread,
                              osal_thread_entry_t entry,
                              void *arg)
{
    th_ctx_t *ctx            = NULL;
    struct task_struct *task = NULL;
    OSAL_ASSERT(thread != NULL);
    OSAL_ASSERT(entry != NULL);
    ctx = (th_ctx_t *)kmalloc(sizeof(th_ctx_t), GFP_KERNEL);
    OSAL_ASSERT(ctx != NULL);

    ctx->entry = entry;
    ctx->arg   = arg;
    ctx->stop  = 0;

    task = kthread_run(th_entry, ctx, "osal_kthread");

    OSAL_ASSERT(!IS_ERR(task));
    ctx->task = task;

    *thread = (osal_thread_t)ctx;
    return OSAL_SUCCESS;
}

void osal_wait_thread_done(osal_thread_t thread)
{
    th_ctx_t *ctx = (th_ctx_t *)thread;

    OSAL_ASSERT(ctx != NULL);

    while (!ctx->stop) {
        osal_sleep_ms(10);
    }
    return;
}

void osal_thread_destroy(osal_thread_t thread)
{
    th_ctx_t *ctx = (th_ctx_t *)thread;

    OSAL_ASSERT(ctx != NULL);
    kfree(ctx);
    return;
}

uint32_t osal_thread_id(void)
{
    return current->pid;
}
