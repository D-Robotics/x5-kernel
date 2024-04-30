//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <hwa/te_hwa_sca.h>
#include "te_regs.h"

#define OSAL_SPIN_LOCK_INIT(spin) __extension__({         \
    osal_spin_lock_init(spin);                            \
})

#define OSAL_SPIN_LOCK_DESTROY(spin) do {                 \
    osal_spin_lock_destroy(spin);                         \
} while(0)

#define OSAL_SPIN_LOCK(spin) do {                         \
    osal_spin_lock(spin);                                 \
} while(0)

#define OSAL_SPIN_UNLOCK(spin) do {                       \
    osal_spin_unlock(spin);                               \
} while(0)

#define OSAL_SPIN_LOCK_IRQSAVE(spin, flags) do {          \
    osal_spin_lock_irqsave((spin),(flags));               \
} while(0)

#define OSAL_SPIN_UNLOCK_IRQRESTORE(spin, flags) do {     \
    osal_spin_unlock_irqrestore((spin),(flags));          \
} while(0)

/**
 * Derive the SCA hwa ctx pointer from the SCA hwa handler
 */
#define HWA_SCA_CTX(_h) __extension__({                   \
    hwa_sca_ctx_t *_ctx = NULL;                           \
    _ctx = (hwa_sca_ctx_t*)hwa_crypt_ctx(&(_h)->base);    \
    TE_ASSERT(_ctx != NULL);                              \
    _ctx;                                                 \
})

/**
 * Lock the SCA hwa
 */
#define HWA_SCA_LOCK(_ctx) do {       \
    OSAL_SPIN_LOCK(&(_ctx)->spin);    \
} while(0)

/**
 * Unlock the SCA hwa
 */
#define HWA_SCA_UNLOCK(_ctx) do {     \
    OSAL_SPIN_UNLOCK(&(_ctx)->spin);  \
} while(0)

/**
 * Lock the SCA hwa plus irq
 */
#define HWA_SCA_LOCK_IRQ(_ctx)                          \
{                                                       \
    unsigned long _flags = 0;                           \
    OSAL_SPIN_LOCK_IRQSAVE(&(_ctx)->spin, &_flags)

/**
 * Lock the SCA hwa plus irq
 */
#define HWA_SCA_UNLOCK_IRQ(_ctx)                        \
    OSAL_SPIN_UNLOCK_IRQRESTORE(&(_ctx)->spin, _flags); \
}

/**
 * Get sca_\p rn.fn field value.
 */
#define SCA_FIELD_GET(val, rn, fn)     HWA_FIELD_GET((val),SCA_##rn,fn)

/**
 * Set sca_\p rn.fn field value to \p fv.
 */
#define SCA_FIELD_SET(val, rn, fn, fv) HWA_FIELD_SET((val),SCA_##rn,fn,(fv))

/**
 * Get sca HWA register
 */
#define SCA_REG_GET(regs, nm)     HWA_REG_GET(regs, sca, nm)

/**
 * Set sca HWA register
 */
#define SCA_REG_SET(regs, nm, nv) HWA_REG_SET(regs, sca, nm, nv)

/**
 * Common file for both SCA and HASH HWAs
 */

/**
 * SCA HWA private context structure
 */
typedef struct hwa_sca_ctx {
    osal_spin_lock_t spin;
    bool ishash;                 /**< true for hash HWA */
    te_sca_regs_t *regs;         /**< SCA register file */
} hwa_sca_ctx_t;

static int sca_cq_write_func(te_hwa_sca_t *h, const uint32_t func)
{
    hwa_sca_ctx_t *ctx = NULL;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK(ctx);
    ctx->regs->cq_func = HTOLE32(func);
    HWA_SCA_UNLOCK(ctx);

    return TE_SUCCESS;
}

static int sca_cq_write_para(te_hwa_sca_t *h, const uint32_t *para,
                             uint32_t nbytes)
{
    uint32_t i = 0;
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !para || (nbytes & 3)) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (0 == nbytes)
        return TE_SUCCESS;

    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK(ctx);
    for (i = 0; i < nbytes / 4; i++) {
        ctx->regs->cq_para = HTOLE32(para[i]);
    }
    HWA_SCA_UNLOCK(ctx);

    return TE_SUCCESS;
}

static int sca_csq_read(te_hwa_sca_t *h, te_sca_csq_entry_t *ent)
{
    union {
        te_sca_csq_entry_t ent;
        uint32_t val;
    } u = {0};
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !ent) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK(ctx);
    u.val = SCA_REG_GET(ctx->regs, csq);
    HWA_SCA_UNLOCK(ctx);

    *ent = u.ent;
    return TE_SUCCESS;
}

static int sca_get_ctrl(te_hwa_sca_t *h, te_sca_ctl_t *ctl)
{
    union {
        te_sca_ctl_t ctl;
        uint32_t val;
    } u = {0};
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !ctl) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK(ctx);
    u.val = SCA_REG_GET(ctx->regs, ctrl);
    HWA_SCA_UNLOCK(ctx);

    *ctl = u.ctl;
    return TE_SUCCESS;
}

static int sca_set_ctrl(te_hwa_sca_t *h, const te_sca_ctl_t *ctl)
{
    union {
        te_sca_ctl_t ctl;
        uint32_t val;
    } u = {0};
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !ctl) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.ctl = *ctl;
    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK(ctx);
    SCA_REG_SET(ctx->regs, ctrl, u.val);
    HWA_SCA_UNLOCK(ctx);

    return TE_SUCCESS;
}

static int sca_state(te_hwa_sca_t *h, te_sca_stat_t *stat)
{
    union {
        te_sca_stat_t stat;
        uint32_t val;
    } u = {0};
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !stat) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_SCA_CTX(h);
    u.val = SCA_REG_GET(ctx->regs, stat);

    *stat = u.stat;
    return TE_SUCCESS;
}

static int sca_int_state(te_hwa_sca_t *h, te_sca_int_t *status)
{
    union {
        te_sca_int_stat_t stat;
        uint32_t val;
    } u = {0};
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !status) {
        return TE_ERROR_BAD_PARAMS;
    }

    osal_memset(status, 0, sizeof(*status));
    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK_IRQ(ctx);
    u.val = SCA_REG_GET(ctx->regs, intr_stat0);
    status->cmd_fin = LE32TOH(ctx->regs->intr_stat1);
    status->op_err = LE32TOH(ctx->regs->intr_stat2);
    HWA_SCA_UNLOCK_IRQ(ctx);

    status->stat = u.stat;
    return TE_SUCCESS;
}

static int sca_eoi(te_hwa_sca_t *h, const te_sca_int_t *status)
{
    union {
        te_sca_int_stat_t stat;
        uint32_t val;
    } u = {0};
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !status) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.stat = status->stat;
    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK_IRQ(ctx);
    ctx->regs->intr_stat1 = HTOLE32(status->cmd_fin);
    ctx->regs->intr_stat2 = HTOLE32(status->op_err);
    SCA_REG_SET(ctx->regs, intr_stat0, u.val);
    HWA_SCA_UNLOCK_IRQ(ctx);

    return TE_SUCCESS;
}

static int sca_get_int_msk(te_hwa_sca_t *h, te_sca_int_t *msk)
{
    union {
        te_sca_int_stat_t stat;
        uint32_t val;
    } u = {0};
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !msk) {
        return TE_ERROR_BAD_PARAMS;
    }

    osal_memset(msk, 0, sizeof(*msk));
    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK(ctx);
    u.val = SCA_REG_GET(ctx->regs, intr_msk0);
    msk->cmd_fin = LE32TOH(ctx->regs->intr_msk1);
    msk->op_err = LE32TOH(ctx->regs->intr_msk2);
    HWA_SCA_UNLOCK(ctx);

    msk->stat = u.stat;
    return TE_SUCCESS;
}

static int sca_set_int_msk(te_hwa_sca_t *h, const te_sca_int_t *msk)
{
    union {
        te_sca_int_stat_t stat;
        uint32_t val;
    } u = {0};
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !msk) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.stat = msk->stat;
    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK_IRQ(ctx);
    ctx->regs->intr_msk1 = HTOLE32(msk->cmd_fin);
    ctx->regs->intr_msk2 = HTOLE32(msk->op_err);
    SCA_REG_SET(ctx->regs, intr_msk0, u.val);
    HWA_SCA_UNLOCK_IRQ(ctx);

    return TE_SUCCESS;
}

static int sca_get_err_info(te_hwa_sca_t *h, te_sca_err_info_t *info)
{
    hwa_sca_ctx_t *ctx = NULL;
    sca_err_stat4Reg_t err_stat4 = { 0 };

    if (!h || !info) {
        return TE_ERROR_BAD_PARAMS;
    }

    osal_memset(info, 0, sizeof(*info));
    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK_IRQ(ctx);
    info->cmd_err = LE32TOH(ctx->regs->err_stat0);
    info->key_err = LE32TOH(ctx->regs->err_stat1);
    info->slot_err = LE32TOH(ctx->regs->err_stat2);
    info->axi.err = LE32TOH(ctx->regs->err_stat3);
    err_stat4.val = SCA_REG_GET(ctx->regs, err_stat4);
    info->axi.slot = err_stat4.bits.axi_err_slot_id;
    info->axi.addr_hi = LE32TOH(ctx->regs->err_stat5);
    info->axi.addr_lo = LE32TOH(ctx->regs->err_stat6);
    info->cq_wdata = LE32TOH(ctx->regs->err_stat7);
    HWA_SCA_UNLOCK_IRQ(ctx);

    return TE_SUCCESS;
}

static int sca_get_key(te_hwa_sca_t *h, uint8_t key[32])
{
    int i;
    uint32_t val = 0;
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !key) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_SCA_CTX(h);
    for (i = 0; i < 8; i++) {
        /* Do we need endian swap of key ? */
        val = ctx->regs->key[i];
        osal_memcpy(key, &val, 4);
    }
    return TE_SUCCESS;
}

static int sca_get_suspd_msk(te_hwa_sca_t *h, te_sca_suspd_msk_t *suspd)
{
    union {
        te_sca_suspd_msk_t suspd;
        uint32_t val;
    } u = {0};
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !suspd) {
        return TE_ERROR_BAD_PARAMS;
    }

    osal_memset(suspd, 0, sizeof(*suspd));
    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK(ctx);
    u.val = SCA_REG_GET(ctx->regs, suspd_msk);
    HWA_SCA_UNLOCK(ctx);

    *suspd = u.suspd;
    return TE_SUCCESS;
}

static int sca_set_suspd_msk(te_hwa_sca_t *h, const te_sca_suspd_msk_t *suspd)
{
    union {
        te_sca_suspd_msk_t suspd;
        uint32_t val;
    } u = {0};
    hwa_sca_ctx_t *ctx = NULL;

    if (!h || !suspd) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.suspd = *suspd;
    ctx = HWA_SCA_CTX(h);
    HWA_SCA_LOCK(ctx);
    SCA_REG_SET(ctx->regs, suspd_msk, u.val);
    HWA_SCA_UNLOCK(ctx);

    return TE_SUCCESS;
}

int te_hwa_sca_alloc( struct te_sca_regs *regs,
                      struct te_hwa_host *host,
                      bool ishash,
                      te_hwa_sca_t **hwa )
{
    int rc = TE_SUCCESS;
    te_hwa_sca_t *sca = NULL;

    if (!regs || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc mem */
    if ((sca = osal_calloc(1, sizeof(*sca))) == NULL) {
        return TE_ERROR_OOM;
    }

    /* init hwa */
    rc = te_hwa_sca_init(regs, host, ishash, sca);
    if (rc != TE_SUCCESS) {
        osal_free(sca);
        return rc;
    }

    *hwa = sca;
    return TE_SUCCESS;
}

int te_hwa_sca_free( te_hwa_sca_t *hwa )
{
    int rc = TE_SUCCESS;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    rc = te_hwa_sca_exit(hwa);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    osal_memset(hwa, 0, sizeof(*hwa));
    osal_free(hwa);
    return TE_SUCCESS;
}

int te_hwa_sca_init( struct te_sca_regs *regs,
                     struct te_hwa_host *host,
                     bool ishash,
                     te_hwa_sca_t *hwa )
{
    int rc = TE_SUCCESS;
    hwa_sca_ctx_t *ctx = NULL;

    if (!regs || !host || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    if ((ctx = osal_calloc(1, sizeof(*ctx))) == NULL) {
        return TE_ERROR_OOM;
    }

    rc = OSAL_SPIN_LOCK_INIT(&ctx->spin);
    if (rc != OSAL_SUCCESS) {
        goto err;
    }

    ctx->ishash = !!ishash;
    ctx->regs = regs;
    osal_memset(hwa, 0, sizeof(*hwa));
    hwa_crypt_init(&hwa->base, host, (void*)ctx);

    /* set ops */
    hwa->cq_write_func = sca_cq_write_func;
    hwa->cq_write_para = sca_cq_write_para;
    hwa->csq_read      = sca_csq_read;
    hwa->get_ctrl      = sca_get_ctrl;
    hwa->set_ctrl      = sca_set_ctrl;
    hwa->state         = sca_state;
    hwa->int_state     = sca_int_state;
    hwa->eoi           = sca_eoi;
    hwa->get_int_msk   = sca_get_int_msk;
    hwa->set_int_msk   = sca_set_int_msk;
    hwa->get_err_info  = sca_get_err_info;
    hwa->get_key       = sca_get_key;
    hwa->get_suspd_msk = sca_get_suspd_msk;
    hwa->set_suspd_msk = sca_set_suspd_msk;

    return TE_SUCCESS;

err:
    osal_free(ctx);
    ctx = NULL;
    return rc;
}

int te_hwa_sca_exit( te_hwa_sca_t *hwa )
{
    hwa_sca_ctx_t *ctx = NULL;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = (hwa_sca_ctx_t*)hwa_crypt_ctx(&hwa->base);
    OSAL_SPIN_LOCK_DESTROY(&ctx->spin);
    osal_memset(ctx, 0, sizeof(*ctx));
    osal_free(ctx);
    hwa_crypt_exit(&hwa->base);
    return TE_SUCCESS;
}

