//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <hwa/te_hwa_ctl.h>
#include "te_regs.h"

/**
 * Derive the CTL hwa ctx pointer from the hwa handler
 */
#define HWA_CTL_CTX(_h) __extension__({                   \
    hwa_ctl_ctx_t *_ctx = NULL;                           \
    _ctx = (hwa_ctl_ctx_t*)hwa_crypt_ctx(&(_h)->base);    \
    TE_ASSERT(_ctx != NULL);                              \
    _ctx;                                                 \
})

/**
 * Get ctl_\p rn.fn field value.
 */
#define CTL_FIELD_GET(val, rn, fn)     HWA_FIELD_GET((val),CTL_##rn,fn)

/**
 * Set ctl_\p rn.fn field value to \p fv.
 */
#define CTL_FIELD_SET(val, rn, fn, fv) HWA_FIELD_SET((val),CTL_##rn,fn,(fv))

/**
 * Get ctl HWA register
 */
#define CTL_REG_GET(regs, nm)     HWA_REG_GET(regs, ctl, nm)

/**
 * Set ctl HWA register
 */
#define CTL_REG_SET(regs, nm, nv) HWA_REG_SET(regs, ctl, nm, nv)

/**
 * Top control HWA private context structure
 */
typedef struct hwa_ctl_ctx {
    struct te_ctl_regs *regs;   /**< Control register file */
    osal_spin_lock_t spin;      /**< lock */
} hwa_ctl_ctx_t;

static int ctl_clock_ctl(struct te_hwa_ctl *h, te_module_t mod, bool state)
{
    uint32_t val = 0;
    hwa_ctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || (mod & ~(TE_MOD_OTP))) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_CTL_CTX(h);
    val = CTL_REG_GET(ctx->regs, clock_ctrl);
    if (mod & TE_MOD_OTP)
        CTL_FIELD_SET(val, CLOCK_CTRL, OTP_CLK_EN, !!state);

    osal_spin_lock_irqsave(&ctx->spin, &flags);
    CTL_REG_SET(ctx->regs, clock_ctrl, val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);
    return TE_SUCCESS;
}

static int ctl_sreset(struct te_hwa_ctl *h, te_module_t mod)
{
    ctl_reset_ctrlReg_t rst;
    hwa_ctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_CTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    rst.val = CTL_REG_GET(ctx->regs, reset_ctrl);
    if (mod & TE_MOD_HASH)
        rst.bits.hash_sreset = true;
    if (mod & TE_MOD_SCA)
        rst.bits.sca_sreset = true;
    if (mod & TE_MOD_ACA)
        rst.bits.aca_sreset = true;
    if (mod & TE_MOD_TRNG)
        rst.bits.trng_sreset = true;
    if (mod & TE_MOD_OTP)
        rst.bits.otp_sreset = true;
    if (mod & TE_MOD_DMA)
        rst.bits.dma_sreset = true;

    CTL_REG_SET(ctx->regs, reset_ctrl, rst.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);
    return TE_SUCCESS;
}

static int ctl_top_conf(struct te_hwa_ctl *h, te_top_conf_t *conf)
{
    union {
        te_top_conf_t conf;
        uint32_t val;
    } u = {0};
    hwa_ctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !conf) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_CTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = CTL_REG_GET(ctx->regs, top_cfg);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *conf = u.conf;
    return TE_SUCCESS;
}

static int ctl_conf_top(struct te_hwa_ctl *h, const te_top_conf_t *conf)
{
    union {
        te_top_conf_t conf;
        uint32_t val;
    } u = {0};
    hwa_ctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !conf) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.conf = *conf;
    ctx = HWA_CTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    CTL_REG_SET(ctx->regs, top_cfg, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);
    return TE_SUCCESS;
}

static int ctl_host_conf(struct te_hwa_ctl *h, int n, te_host_conf_t *conf)
{
    union {
        te_host_conf_t conf;
        uint32_t val;
    } u = {0};
    hwa_ctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || n < 0 || n > (int)TE_MAX_HOST_NUM || !conf) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_CTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = CTL_REG_GET(&ctx->regs->host[n], cfg_host);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *conf = u.conf;
    return TE_SUCCESS;
}

static int ctl_conf_host(struct te_hwa_ctl *h, int n, const te_host_conf_t *conf)
{
    union {
        te_host_conf_t conf;
        uint32_t val;
    } u = {0};
    hwa_ctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || n < 0 || n > (int)TE_MAX_HOST_NUM || !conf) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.conf = *conf;
    ctx = HWA_CTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    CTL_REG_SET(&ctx->regs->host[n], cfg_host, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);
    return TE_SUCCESS;
}

int te_hwa_ctl_alloc( struct te_ctl_regs *regs,
                      struct te_hwa_host *host,
                      te_hwa_ctl_t **hwa )
{
    int rc = TE_SUCCESS;
    te_hwa_ctl_t *ctl = NULL;

    if (!regs || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc mem */
    if ((ctl = osal_calloc(1, sizeof(*ctl))) == NULL) {
        return TE_ERROR_OOM;
    }

    /* init hwa */
    rc = te_hwa_ctl_init(regs, host, ctl);
    if (rc != TE_SUCCESS) {
        osal_free(ctl);
        return rc;
    }

    *hwa = ctl;
    return TE_SUCCESS;
}

int te_hwa_ctl_free( te_hwa_ctl_t *hwa )
{
    int rc = TE_SUCCESS;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    rc = te_hwa_ctl_exit(hwa);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    osal_memset(hwa, 0, sizeof(*hwa));
    osal_free(hwa);
    return TE_SUCCESS;
}

int te_hwa_ctl_init( struct te_ctl_regs *regs,
                     struct te_hwa_host *host,
                     te_hwa_ctl_t *hwa )
{
    int rc = TE_SUCCESS;
    hwa_ctl_ctx_t *ctx = NULL;

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
    hwa->clock_ctl = ctl_clock_ctl;
    hwa->sreset    = ctl_sreset;
    hwa->top_conf  = ctl_top_conf;
    hwa->conf_top  = ctl_conf_top;
    hwa->host_conf = ctl_host_conf;
    hwa->conf_host = ctl_conf_host;

    return TE_SUCCESS;

err:
    osal_free(ctx);
    ctx = NULL;
    return rc;
}

int te_hwa_ctl_exit( te_hwa_ctl_t *hwa )
{
    hwa_ctl_ctx_t *ctx = NULL;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = (hwa_ctl_ctx_t*)hwa_crypt_ctx(&hwa->base);
    osal_spin_lock_destroy(&ctx->spin);
    osal_memset(ctx, 0, sizeof(*ctx));
    osal_free(ctx);
    hwa_crypt_exit(&hwa->base);
    return TE_SUCCESS;
}

