//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <hwa/te_hwa_dbgctl.h>
#include "te_regs.h"

/**
 * Derive the DBG CTL hwa ctx pointer from the hwa handler
 */
#define HWA_DBGCTL_CTX(_h) __extension__({                \
    hwa_dbgctl_ctx_t *_ctx = NULL;                        \
    _ctx = (hwa_dbgctl_ctx_t*)hwa_crypt_ctx(&(_h)->base); \
    TE_ASSERT(_ctx != NULL);                              \
    _ctx;                                                 \
})

/**
 * Debug CTL HWA private context structure
 */
typedef struct hwa_dbgctl_ctx {
    struct te_dbgctl_regs *regs;   /**< Debug CTL register file */
    osal_spin_lock_t spin;      /**< lock */
} hwa_dbgctl_ctx_t;

static int dbg_get_dbgctl(struct te_hwa_dbgctl *h, uint32_t *ctl)
{
    hwa_dbgctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !ctl) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_DBGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    *ctl = LE32TOH(ctx->regs->ctrl);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int dbg_set_dbgctl(struct te_hwa_dbgctl *h, const uint32_t ctl)
{
    hwa_dbgctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_DBGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ctx->regs->ctrl = HTOLE32(ctl);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int dbg_get_locken(struct te_hwa_dbgctl *h, uint32_t *lock)
{
    hwa_dbgctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !lock) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_DBGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    *lock = LE32TOH(ctx->regs->lock);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int dbg_set_locken(struct te_hwa_dbgctl *h, const uint32_t lock)
{
    hwa_dbgctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_DBGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ctx->regs->lock = HTOLE32(lock);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

int te_hwa_dbgctl_alloc( struct te_dbgctl_regs *regs,
                         struct te_hwa_host *host,
                         te_hwa_dbgctl_t **hwa )
{
    int rc = TE_SUCCESS;
    te_hwa_dbgctl_t *dbg = NULL;

    if (!regs || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc mem */
    if ((dbg = osal_calloc(1, sizeof(*dbg))) == NULL) {
        return TE_ERROR_OOM;
    }

    /* init hwa */
    rc = te_hwa_dbgctl_init(regs, host, dbg);
    if (rc != TE_SUCCESS) {
        osal_free(dbg);
        return rc;
    }

    *hwa = dbg;
    return TE_SUCCESS;
}

int te_hwa_dbgctl_free( te_hwa_dbgctl_t *hwa )
{
    int rc = TE_SUCCESS;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    rc = te_hwa_dbgctl_exit(hwa);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    osal_memset(hwa, 0, sizeof(*hwa));
    osal_free(hwa);
    return TE_SUCCESS;
}

int te_hwa_dbgctl_init( struct te_dbgctl_regs *regs,
                        struct te_hwa_host *host,
                        te_hwa_dbgctl_t *hwa )
{
    int rc = TE_SUCCESS;
    hwa_dbgctl_ctx_t *ctx = NULL;

    if (!regs || !host || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    if ((ctx = osal_calloc(1, sizeof(*ctx))) == NULL) {
        return TE_ERROR_OOM;
    }

    rc = osal_spin_lock_init(&ctx->spin);
    if (rc != OSAL_SUCCESS) {
        goto err;
    }

    ctx->regs = regs;
    osal_memset(hwa, 0, sizeof(*hwa));
    hwa_crypt_init(&hwa->base, host, (void*)ctx);

    /* set ops */
    hwa->get_dbgctl = dbg_get_dbgctl;
    hwa->set_dbgctl = dbg_set_dbgctl;
    hwa->get_locken = dbg_get_locken;
    hwa->set_locken = dbg_set_locken;

    return TE_SUCCESS;

err:
    osal_free(ctx);
    ctx = NULL;
    return rc;
}

int te_hwa_dbgctl_exit( te_hwa_dbgctl_t *hwa )
{
    hwa_dbgctl_ctx_t *ctx = NULL;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = (hwa_dbgctl_ctx_t*)hwa_crypt_ctx(&hwa->base);
    osal_spin_lock_destroy(&ctx->spin);
    osal_memset(ctx, 0, sizeof(*ctx));
    osal_free(ctx);
    hwa_crypt_exit(&hwa->base);
    return TE_SUCCESS;
}

