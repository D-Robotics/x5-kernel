//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <hwa/te_hwa_trngctl.h>
#include "te_regs.h"

/**
 * Derive the TRNG CTL hwa ctx pointer from the hwa handler
 */
#define HWA_TRNGCTL_CTX(_h) __extension__({               \
    hwa_trngctl_ctx_t *_ctx = NULL;                       \
    _ctx = (hwa_trngctl_ctx_t*)hwa_crypt_ctx(&(_h)->base);\
    TE_ASSERT(_ctx != NULL);                              \
    _ctx;                                                 \
})

/**
 * Get trngctl_\p rn.fn field value.
 */
#define TRNGCTL_FIELD_GET(val, rn, fn)     HWA_FIELD_GET((val),TRNGCTL_##rn,fn)

/**
 * Set trngctl_\p rn.fn field value to \p fv.
 */
#define TRNGCTL_FIELD_SET(val, rn, fn, fv) HWA_FIELD_SET((val),TRNGCTL_##rn,fn,(fv))

/**
 * Get trngctl HWA register
 */
#define TRNGCTL_REG_GET(regs, nm)     HWA_REG_GET(regs, trngctl, nm)

/**
 * Set trngctl HWA register
 */
#define TRNGCTL_REG_SET(regs, nm, nv) HWA_REG_SET(regs, trngctl, nm, nv)

/**
 * TRNG CTL HWA private context structure
 */
typedef struct hwa_trngctl_ctx {
    struct te_trngctl_regs *regs;   /**< TRNG CTL register file */
    osal_spin_lock_t spin;      /**< lock */
} hwa_trngctl_ctx_t;

static int trngctl_get_conf(struct te_hwa_trngctl *h, te_trng_conf_t *conf)
{
    hwa_trngctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;
    union {
        te_trng_entropy_src_t src;
        te_trng_entropy_sample_t sample;
        te_trng_ro_t ro;
        te_trng_postproc_t postproc;
        te_trng_eval_t eval;
        te_trng_err_thr_t thr;
        uint32_t val;
    } u = {0};

    if (!h || !conf) {
        return TE_ERROR_BAD_PARAMS;
    }

    osal_memset(conf, 0, sizeof(*conf));
    ctx = HWA_TRNGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = TRNGCTL_REG_GET(ctx->regs, entropy_src);
    conf->src = u.src;
    u.val = TRNGCTL_REG_GET(ctx->regs, entropy_sample);
    conf->sample = u.sample;
    u.val = TRNGCTL_REG_GET(ctx->regs, ro_setting);
    conf->ro = u.ro;
    u.val = TRNGCTL_REG_GET(ctx->regs, post_proc);
    conf->postproc = u.postproc;
    u.val = TRNGCTL_REG_GET(ctx->regs, eval_setting);
    conf->eval = u.eval;
    u.val = TRNGCTL_REG_GET(ctx->regs, err_cnt_th);
    conf->thr = u.thr;
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int trngctl_setup(struct te_hwa_trngctl *h, const te_trng_conf_t *conf)
{
    hwa_trngctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;
    union {
        te_trng_entropy_src_t src;
        te_trng_entropy_sample_t sample;
        te_trng_ro_t ro;
        te_trng_postproc_t postproc;
        te_trng_eval_t eval;
        te_trng_err_thr_t thr;
        uint32_t val;
    } u = {0};

    if (!h || !conf) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_TRNGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.src = conf->src;
    TRNGCTL_REG_SET(ctx->regs, entropy_src, u.val);
    u.sample = conf->sample;
    TRNGCTL_REG_SET(ctx->regs, entropy_sample, u.val);
    u.ro = conf->ro;
    TRNGCTL_REG_SET(ctx->regs, ro_setting, u.val);
    u.postproc = conf->postproc;
    TRNGCTL_REG_SET(ctx->regs, post_proc, u.val);
    u.eval = conf->eval;
    TRNGCTL_REG_SET(ctx->regs, eval_setting, u.val);
    u.thr = conf->thr;
    TRNGCTL_REG_SET(ctx->regs, err_cnt_th, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int trngctl_get_ctl(struct te_hwa_trngctl *h, te_trng_ctl_t *ctl)
{
    union {
        te_trng_ctl_t ctl;
        uint32_t val;
    } u = {0};
    hwa_trngctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !ctl) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_TRNGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = TRNGCTL_REG_GET(ctx->regs, ctrl);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *ctl = u.ctl;
    return TE_SUCCESS;
}

static int trngctl_set_ctl(struct te_hwa_trngctl *h, const te_trng_ctl_t *ctl)
{
    union {
        te_trng_ctl_t ctl;
        uint32_t val;
    } u = {0};
    hwa_trngctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !ctl) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.ctl = *ctl;
    ctx = HWA_TRNGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    TRNGCTL_REG_SET(ctx->regs, ctrl, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int trngctl_state(struct te_hwa_trngctl *h, te_trng_stat_t *stat)
{
    union {
        te_trng_stat_t stat;
        uint32_t val;
    } u = {0};
    hwa_trngctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !stat) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_TRNGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = TRNGCTL_REG_GET(ctx->regs, stat);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *stat = u.stat;
    return TE_SUCCESS;
}

static int trngctl_int_state(struct te_hwa_trngctl *h, te_trng_int_t *status)
{
    union {
        te_trng_int_t status;
        uint32_t val;
    } u = {0};
    hwa_trngctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !status) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_TRNGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = TRNGCTL_REG_GET(ctx->regs, intr);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *status = u.status;
    return TE_SUCCESS;
}

static int trngctl_eoi(struct te_hwa_trngctl *h, const te_trng_int_t *status)
{
    union {
        te_trng_int_t status;
        uint32_t val;
    } u = {0};
    hwa_trngctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !status) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.status = *status;
    ctx = HWA_TRNGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    TRNGCTL_REG_SET(ctx->regs, intr, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int trngctl_get_int_msk(struct te_hwa_trngctl *h, te_trng_int_t *msk)
{
    union {
        te_trng_int_t msk;
        uint32_t val;
    } u = {0};
    hwa_trngctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !msk) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_TRNGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = TRNGCTL_REG_GET(ctx->regs, intr_msk);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *msk = u.msk;
    return TE_SUCCESS;
}

static int trngctl_set_int_msk(struct te_hwa_trngctl *h,
                               const te_trng_int_t *msk)
{
    union {
        te_trng_int_t msk;
        uint32_t val;
    } u = {0};
    hwa_trngctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !msk) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.msk = *msk;
    ctx = HWA_TRNGCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    TRNGCTL_REG_SET(ctx->regs, intr_msk, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int trngctl_get_err_cnt(struct te_hwa_trngctl *h, te_trng_err_cnt_t *err)
{
    hwa_trngctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !err) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_TRNGCTL_CTX(h);
    /**
     * Load error counts. Read clear!
     */
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    err->vn_cnt = LE32TOH(ctx->regs->vn_total);
    err->crng_cnt = LE32TOH(ctx->regs->crng_total);
    err->rep_cnt = LE32TOH(ctx->regs->rep_total);
    err->adap_cnt = LE32TOH(ctx->regs->adap_total);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

int te_hwa_trngctl_alloc( struct te_trngctl_regs *regs,
                          struct te_hwa_host *host,
                          te_hwa_trngctl_t **hwa )
{
    int rc = TE_SUCCESS;
    te_hwa_trngctl_t *ctl = NULL;

    if (!regs || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc mem */
    if ((ctl = osal_calloc(1, sizeof(*ctl))) == NULL) {
        return TE_ERROR_OOM;
    }

    /* init hwa */
    rc = te_hwa_trngctl_init(regs, host, ctl);
    if (rc != TE_SUCCESS) {
        osal_free(ctl);
        return rc;
    }

    *hwa = ctl;
    return TE_SUCCESS;
}

int te_hwa_trngctl_free( te_hwa_trngctl_t *hwa )
{
    int rc = TE_SUCCESS;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    rc = te_hwa_trngctl_exit(hwa);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    osal_memset(hwa, 0, sizeof(*hwa));
    osal_free(hwa);
    return TE_SUCCESS;
}

int te_hwa_trngctl_init( struct te_trngctl_regs *regs,
                         struct te_hwa_host *host,
                         te_hwa_trngctl_t *hwa )
{
    int rc = TE_SUCCESS;
    hwa_trngctl_ctx_t *ctx = NULL;

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
    hwa->get_conf    = trngctl_get_conf;
    hwa->setup       = trngctl_setup;
    hwa->get_ctl     = trngctl_get_ctl;
    hwa->set_ctl     = trngctl_set_ctl;
    hwa->state       = trngctl_state;
    hwa->int_state   = trngctl_int_state;
    hwa->eoi         = trngctl_eoi;
    hwa->get_int_msk = trngctl_get_int_msk;
    hwa->set_int_msk = trngctl_set_int_msk;
    hwa->get_err_cnt = trngctl_get_err_cnt;

    return TE_SUCCESS;

err:
    osal_free(ctx);
    ctx = NULL;
    return rc;
}

int te_hwa_trngctl_exit( te_hwa_trngctl_t *hwa )
{
    hwa_trngctl_ctx_t *ctx = NULL;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = (hwa_trngctl_ctx_t*)hwa_crypt_ctx(&hwa->base);
    osal_spin_lock_destroy(&ctx->spin);
    osal_memset(ctx, 0, sizeof(*ctx));
    osal_free(ctx);
    hwa_crypt_exit(&hwa->base);
    return TE_SUCCESS;
}

