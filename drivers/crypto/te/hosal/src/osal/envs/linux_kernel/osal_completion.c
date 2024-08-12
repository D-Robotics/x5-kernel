//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/wait.h>
#include "osal_log.h"
#include "osal_assert.h"
#include "osal_completion.h"
#include "osal_internal.h"

typedef struct comp_ctx {
    struct completion comp;
} comp_ctx_t;

osal_err_t osal_completion_init(osal_completion_t *comp)
{
    comp_ctx_t *ctx = NULL;
    if (comp == NULL) {
        dump_stack();
        OSAL_ASSERT(comp != NULL);
    }

    if (comp->ctx != NULL) {
        dump_stack();
        OSAL_ASSERT(comp->ctx == NULL);
    }

    ctx = (comp_ctx_t *)kmalloc(sizeof(comp_ctx_t), GFP_KERNEL);
    OSAL_ASSERT(ctx != NULL);

    init_completion(&ctx->comp);
    comp->ctx = ctx;

    return OSAL_SUCCESS;
}

void osal_completion_destroy(osal_completion_t *comp)
{
    if (comp == NULL) {
        dump_stack();
        OSAL_ASSERT(comp != NULL);
    }

    if (comp->ctx == NULL) {
        dump_stack();
        OSAL_ASSERT(comp->ctx != NULL);
    }

    kfree(comp->ctx);
    comp->ctx = NULL;
    return;
}

void osal_completion_wait(osal_completion_t *comp)
{
    int ret = 0;
    comp_ctx_t *ctx = NULL;
    if (comp == NULL) {
        dump_stack();
        OSAL_ASSERT(comp != NULL);
    }

    if (comp->ctx == NULL) {
        dump_stack();
        OSAL_ASSERT(comp->ctx != NULL);
    }

    ctx = comp->ctx;
    do {
        ret = wait_for_completion_interruptible(&ctx->comp);
    } while (-ERESTARTSYS == ret);
    return;
}

void osal_completion_signal(osal_completion_t *comp)
{
    comp_ctx_t *ctx = NULL;

    if (comp == NULL) {
        dump_stack();
        OSAL_ASSERT(comp != NULL);
    }

    if (comp->ctx == NULL) {
        dump_stack();
        OSAL_ASSERT(comp->ctx != NULL);
    }
    ctx = comp->ctx;
    complete(&ctx->comp);
    return;
}

void osal_completion_broadcast(osal_completion_t *comp)
{
    comp_ctx_t *ctx = NULL;

    if (comp == NULL) {
        dump_stack();
        OSAL_ASSERT(comp != NULL);
    }

    if (comp->ctx == NULL) {
        dump_stack();
        OSAL_ASSERT(comp->ctx != NULL);
    }
    ctx = comp->ctx;

    complete_all(&ctx->comp);
    return;
}

void osal_completion_reset(osal_completion_t *comp)
{
    comp_ctx_t *ctx = NULL;

    if (comp == NULL) {
        dump_stack();
        OSAL_ASSERT(comp != NULL);
    }

    if (comp->ctx == NULL) {
        dump_stack();
        OSAL_ASSERT(comp->ctx != NULL);
    }
    ctx = comp->ctx;
    reinit_completion(&ctx->comp);
    return;
}
