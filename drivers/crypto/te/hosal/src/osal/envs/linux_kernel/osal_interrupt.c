//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include "osal_interrupt.h"
#include "osal_log.h"
#include "osal_assert.h"
#include "osal_internal.h"

typedef struct irq_ctx {
    int32_t irq;
    osal_intr_handler_t handler;
    void *para;
} irq_ctx_t;

static irqreturn_t osal_def_handler(int irq, void *dev_id)
{
    osal_err_t ret = OSAL_SUCCESS;
    irq_ctx_t *ctx = (irq_ctx_t *)dev_id;

    if (irq != ctx->irq) {
        return IRQ_NONE;
    }

    ret = ctx->handler(ctx->para);
    OSAL_ASSERT(ret == OSAL_SUCCESS);

    return IRQ_HANDLED;
}

unsigned long osal_intr_lock(void)
{
    unsigned long flag;
    local_irq_save(flag);
    return flag;
}

void osal_intr_unlock(unsigned long flag)
{
    local_irq_restore(flag);
    return;
}

osal_err_t osal_irq_request(osal_intr_ctx_t *intr_ctx,
                            int32_t intr_num,
                            osal_intr_handler_t intr_handler,
                            void *para)
{
    irq_ctx_t *ctx = NULL;
    int ret        = -1;

    OSAL_ASSERT(intr_ctx != NULL);
    OSAL_ASSERT(intr_handler != NULL);

    ctx = (irq_ctx_t *)kmalloc(sizeof(irq_ctx_t), GFP_KERNEL);
    OSAL_ASSERT(ctx != NULL);

    ctx->irq     = intr_num;
    ctx->handler = intr_handler;
    ctx->para    = para;

    ret = request_irq(intr_num, osal_def_handler, IRQF_SHARED, "osal-irq",
                      (void *)ctx);
    if (ret < 0) {
        kfree(ctx);
        return OSAL_ERROR_GENERIC;
    }

    intr_ctx->ctx = ctx;

    return OSAL_SUCCESS;
}

void osal_irq_free(osal_intr_ctx_t *intr_ctx)
{
    irq_ctx_t *ctx = NULL;
    OSAL_ASSERT(intr_ctx != NULL);
    OSAL_ASSERT(intr_ctx->ctx != NULL);
    ctx = intr_ctx->ctx;

    free_irq(ctx->irq, (void *)ctx);

    kfree(ctx);
    intr_ctx->ctx = NULL;
    return;
}
