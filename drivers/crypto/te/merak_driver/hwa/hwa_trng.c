//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <hwa/te_hwa_trng.h>
#include "te_regs.h"

/**
 * Derive the TRNG hwa ctx pointer from the hwa handler
 */
#define HWA_TRNG_CTX(_h) __extension__({                  \
    hwa_trng_ctx_t *_ctx = NULL;                          \
    _ctx = (hwa_trng_ctx_t*)hwa_crypt_ctx(&(_h)->base);   \
    TE_ASSERT(_ctx != NULL);                              \
    _ctx;                                                 \
})

/**
 * Get trng_\p rn.fn field value.
 */
#define TRNG_FIELD_GET(val, rn, fn)     HWA_FIELD_GET((val),TRNG_##rn,fn)

/**
 * Set trng_\p rn.fn field value to \p fv.
 */
#define TRNG_FIELD_SET(val, rn, fn, fv) HWA_FIELD_SET((val),TRNG_##rn,fn,(fv))

/**
 * Get trng HWA register
 */
#define TRNG_REG_GET(regs, nm)     HWA_REG_GET(regs, trng, nm)

/**
 * Set trng HWA register
 */
#define TRNG_REG_SET(regs, nm, nv) HWA_REG_SET(regs, trng, nm, nv)

/**
 *  RN pool HWA private context structure
 */
typedef struct hwa_trng_ctx {
    struct te_trng_regs *regs;  /**< RNP register file */
    osal_spin_lock_t spin;      /**< lock */
} hwa_trng_ctx_t;

static int trng_get_ctl(struct te_hwa_trng *h, te_rnp_ctl_t *ctl)
{
    union {
        te_rnp_ctl_t ctl;
        uint32_t val;
    } u = {0};
    hwa_trng_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !ctl) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_TRNG_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = TRNG_REG_GET(ctx->regs, ctrl);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *ctl = u.ctl;
    return TE_SUCCESS;
}

static int trng_set_ctl(struct te_hwa_trng *h, const te_rnp_ctl_t *ctl)
{
    union {
        te_rnp_ctl_t ctl;
        uint32_t val;
    } u = {0};
    hwa_trng_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !ctl) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.ctl = *ctl;
    ctx = HWA_TRNG_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    TRNG_REG_SET(ctx->regs, ctrl, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int trng_state(struct te_hwa_trng *h, te_rnp_stat_t *stat)
{
    union {
        te_rnp_stat_t stat;
        uint32_t val;
    } u = {0};
    hwa_trng_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !stat) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_TRNG_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = TRNG_REG_GET(ctx->regs, stat);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *stat = u.stat;
    return TE_SUCCESS;
}

static int trng_int_state(struct te_hwa_trng *h, te_rnp_int_t *state)
{
    union {
        te_rnp_int_t state;
        uint32_t val;
    } u = {0};
    hwa_trng_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !state) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_TRNG_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = TRNG_REG_GET(ctx->regs, intr);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *state = u.state;
    return TE_SUCCESS;
}

static int trng_eoi(struct te_hwa_trng *h, const te_rnp_int_t *state)
{
    union {
        te_rnp_int_t state;
        uint32_t val;
    } u = {0};
    hwa_trng_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !state) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.state = *state;
    ctx = HWA_TRNG_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    TRNG_REG_SET(ctx->regs, intr, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int trng_get_int_msk(struct te_hwa_trng *h, te_rnp_int_t *msk)
{
    union {
        te_rnp_int_t msk;
        uint32_t val;
    } u = {0};
    hwa_trng_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !msk) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_TRNG_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = TRNG_REG_GET(ctx->regs, intr_msk);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *msk = u.msk;
    return TE_SUCCESS;
}

static int trng_set_int_msk(struct te_hwa_trng *h, const te_rnp_int_t *msk)
{
    union {
        te_rnp_int_t msk;
        uint32_t val;
    } u = {0};
    hwa_trng_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !msk) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.msk = *msk;
    ctx = HWA_TRNG_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    TRNG_REG_SET(ctx->regs, intr_msk, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int trng_read(struct te_hwa_trng *h, uint8_t *buf, size_t len)
{
#define TRNG_WORD_SIZE          (4U)
    size_t i = 0;
    uint32_t val = 0, step = 0;
    hwa_trng_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !buf || len > sizeof(ctx->regs->data)) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_TRNG_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    for (i = 0; i < len; i += step) {
        step = (len - i) > TRNG_WORD_SIZE ? TRNG_WORD_SIZE : (len - i);
        val = ctx->regs->data[i / TRNG_WORD_SIZE];
        val = BE32TOH(val);  /* swap endian if needed */
        memcpy(buf + i, &val, step);
    }
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

int te_hwa_trng_alloc( struct te_trng_regs *regs,
                       struct te_hwa_host *host,
                       te_hwa_trng_t **hwa )
{
    int rc = TE_SUCCESS;
    te_hwa_trng_t *rnp = NULL;

    if (!regs || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc mem */
    if ((rnp = osal_calloc(1, sizeof(*rnp))) == NULL) {
        return TE_ERROR_OOM;
    }

    /* init hwa */
    rc = te_hwa_trng_init(regs, host, rnp);
    if (rc != TE_SUCCESS) {
        osal_free(rnp);
        return rc;
    }

    *hwa = rnp;
    return TE_SUCCESS;
}

int te_hwa_trng_free( te_hwa_trng_t *hwa )
{
    int rc = TE_SUCCESS;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    rc = te_hwa_trng_exit(hwa);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    osal_memset(hwa, 0, sizeof(*hwa));
    osal_free(hwa);
    return TE_SUCCESS;
}

int te_hwa_trng_init( struct te_trng_regs *regs,
                      struct te_hwa_host *host,
                      te_hwa_trng_t *hwa )
{
    int rc = TE_SUCCESS;
    hwa_trng_ctx_t *ctx = NULL;

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
    hwa->get_ctl     = trng_get_ctl;
    hwa->set_ctl     = trng_set_ctl;
    hwa->state       = trng_state;
    hwa->int_state   = trng_int_state;
    hwa->eoi         = trng_eoi;
    hwa->get_int_msk = trng_get_int_msk;
    hwa->set_int_msk = trng_set_int_msk;
    hwa->read        = trng_read;

    return TE_SUCCESS;

err:
    osal_free(ctx);
    ctx = NULL;
    return rc;
}

int te_hwa_trng_exit( te_hwa_trng_t *hwa )
{
    hwa_trng_ctx_t *ctx = NULL;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = (hwa_trng_ctx_t*)hwa_crypt_ctx(&hwa->base);
    osal_spin_lock_destroy(&ctx->spin);
    osal_memset(ctx, 0, sizeof(*ctx));
    osal_free(ctx);
    hwa_crypt_exit(&hwa->base);
    return TE_SUCCESS;
}

