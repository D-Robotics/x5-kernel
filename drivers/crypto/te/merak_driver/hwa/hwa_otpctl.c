//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <hwa/te_hwa_otpctl.h>
#include <hwa/te_hwa.h>
#include "te_regs.h"

/**
 * OTP word size in byte
 */
#define OTP_WORD_SIZE 4

/**
 * OTP write wait time in us.
 */
#define OTP_WR_WAIT_US  100

/**
 * Derive the OTP CTL hwa ctx pointer from the hwa handler
 */
#define HWA_OTPCTL_CTX(_h) __extension__({                \
    hwa_otpctl_ctx_t *_ctx = NULL;                        \
    _ctx = (hwa_otpctl_ctx_t*)hwa_crypt_ctx(&(_h)->base); \
    TE_ASSERT(_ctx != NULL);                              \
    _ctx;                                                 \
})

/**
 * \brief   Read one word from the OTP at the specified \p _ofs. The OTP
 *          works as being set with an initial value of 0 always in s/w
 *          perspective.
 * \_ofs    offset into the OTP table
 * \return  OTP value
 */
#define HWA_OTP_READ(_ofs) __extension__({                \
    te_read32((void*)ctx->regs + (_ofs));                 \
})

/**
 * \brief   Write one word to the OTP at the specified \p _ofs. The OTP
 *          works as being set with an intial value of 0 always in s/w
 *          perspective. Indirect write allowed only.
 * \_ofs    offset into the OTP table
 * \_val    value to write
 * \return  void
 */
#define HWA_OTP_WRITE(_ofs, _val) do {                    \
    ctx->regs->wr_addr = (_ofs);                          \
    ctx->regs->wr_data = (_val);                          \
    ctx->regs->otp_wr.bits.trig = true;                   \
} while(0)

/**
 * Get otpctl_\p rn.fn field value.
 */
#define OTPCTL_FIELD_GET(val, rn, fn)     HWA_FIELD_GET((val),OTPCTL_##rn,fn)

/**
 * Set otpctl_\p rn.fn field value to \p fv.
 */
#define OTPCTL_FIELD_SET(val, rn, fn, fv) HWA_FIELD_SET((val),OTPCTL_##rn,fn,(fv))

/**
 * Get otpctl HWA register
 */
#define OTPCTL_REG_GET(regs, nm)     HWA_REG_GET(regs, otpctl, nm)

/**
 * Set otpctl HWA register
 */
#define OTPCTL_REG_SET(regs, nm, nv) HWA_REG_SET(regs, otpctl, nm, nv)

/**
 * OTP CTL HWA private context structure
 */
typedef struct hwa_otpctl_ctx {
    struct te_otpctl_regs *regs;/**< OTP CTL register file */
    uint32_t total;             /**< OTP total length, in byte */
    osal_spin_lock_t spin;      /**< lock */
    uint8_t skey_wsize;         /**< OTP model/root key size in word */
} hwa_otpctl_ctx_t;

static int otp_write_word(hwa_otpctl_ctx_t *ctx, uint32_t ofs, uint32_t val)
{
    otpctl_update_statReg_t update_stat =  { 0 };
    /* write a word */
    HWA_OTP_WRITE(ofs, val);

    /* wait otp write done */
    while (1) {
        update_stat.val = OTPCTL_REG_GET(ctx->regs, update_stat);
        if (update_stat.bits.update_busy == 0) {
            break;
        }
        osal_delay_us(OTP_WR_WAIT_US);
    }

    /* check result */
    return update_stat.bits.update_fail ?
           TE_ERROR_ACCESS_DENIED : TE_SUCCESS;
}

static int otp_get_ctl(struct te_hwa_otpctl *h, te_otp_ctl_t *ctl)
{
    uint32_t val = 0;
    hwa_otpctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !ctl) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_OTPCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    val = OTPCTL_REG_GET(ctx->regs, setting);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    osal_memset(ctl, 0, sizeof(*ctl));
    ctl->direct_rd = OTPCTL_FIELD_GET(val, SETTING, OTP_DIRECT_RD);
    ctl->general_ctl = OTPCTL_FIELD_GET(val, SETTING, OTP_GENERIC_CTRL);
    return TE_SUCCESS;
}

static int otp_set_ctl(struct te_hwa_otpctl *h, const te_otp_ctl_t *ctl)
{
    uint32_t val = 0;
    hwa_otpctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !ctl) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_OTPCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    val = OTPCTL_REG_GET(ctx->regs, setting);
    OTPCTL_FIELD_SET(val, SETTING, OTP_DIRECT_RD, ctl->direct_rd);
    OTPCTL_FIELD_SET(val, SETTING, OTP_GENERIC_CTRL, ctl->general_ctl);
    OTPCTL_REG_SET(ctx->regs, setting, val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int otp_state(struct te_hwa_otpctl *h, te_otp_stat_t *stat)
{
    uint32_t val = 0;
    hwa_otpctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !stat) {
        return TE_ERROR_BAD_PARAMS;
    }

    osal_memset(stat, 0, sizeof(*stat));
    ctx = HWA_OTPCTL_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    val = OTPCTL_REG_GET(ctx->regs, setting);
    stat->init_done = OTPCTL_FIELD_GET(val, SETTING, OTP_INIT_DONE);
    stat->lcs_load_fail = OTPCTL_FIELD_GET(val, SETTING, LCS_LOAD_FAIL);
    stat->mid_load_fail = OTPCTL_FIELD_GET(val, SETTING, MID_LOAD_FAIL);
    stat->did_load_fail = OTPCTL_FIELD_GET(val, SETTING, DID_LOAD_FAIL);
    stat->key_load_fail = OTPCTL_FIELD_GET(val, SETTING, KEY_LOAD_FAIL);
    stat->lock_load_fail = OTPCTL_FIELD_GET(val, SETTING, LOCK_LOAD_FAIL);
    stat->data_inv_fail = OTPCTL_FIELD_GET(val, SETTING, DATA_INV_FAIL);
    stat->lcs_err = OTPCTL_FIELD_GET(val, SETTING, LCS_ERR);

    val = OTPCTL_REG_GET(ctx->regs, update_stat);
    stat->up_busy = OTPCTL_FIELD_GET(val, UPDATE_STAT, UPDATE_BUSY);
    stat->up_fail = OTPCTL_FIELD_GET(val, UPDATE_STAT, UPDATE_FAIL);
    stat->otp_rdy = OTPCTL_FIELD_GET(val, UPDATE_STAT, OTP_READY);
    stat->shdw_valid = OTPCTL_FIELD_GET(val, UPDATE_STAT, AO_SHD_VALID);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int otp_write(struct te_hwa_otpctl *h, uint32_t off,
                     const uint8_t *buf, uint32_t len)
{
    int rc = TE_SUCCESS;
    uint32_t val = 0;
    uint32_t pos = off;
    hwa_otpctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || (!buf && len != 0) || (pos & (OTP_WORD_SIZE - 1))
        || (len & (OTP_WORD_SIZE - 1))) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (off > off + len) {
        return TE_ERROR_OVERFLOW;
    }

    ctx = HWA_OTPCTL_CTX(h);
    if (off + len > ctx->total) {
        return TE_ERROR_EXCESS_DATA;
    }

    if (0 == len) {
        return TE_SUCCESS;
    }

    osal_spin_lock_irqsave(&ctx->spin, &flags);

    /* complete words */
    for (; off + len - pos >= OTP_WORD_SIZE; pos += OTP_WORD_SIZE) {
        osal_memcpy(&val, buf + pos - off, OTP_WORD_SIZE);
        if ((rc = otp_write_word(ctx, pos, val)) != TE_SUCCESS) {
            goto out;
        }
    }

out:
    osal_spin_unlock_irqrestore(&ctx->spin, flags);
    return rc;
}

static int otp_set_dummy(struct te_hwa_otpctl *h, const te_otp_dummy_t *dummy)
{
    uint32_t i = 0;
    uint32_t val = 0;
    hwa_otpctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !dummy) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_OTPCTL_CTX(h);
    TE_ASSERT((ctx->skey_wsize == 4) || (ctx->skey_wsize == 8));
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    /* device root key comes first */
    for (i = 0; i < ctx->skey_wsize; i++) {
        osal_memcpy(&val, dummy->rootk + i * OTP_WORD_SIZE, OTP_WORD_SIZE);
        ctx->regs->seckeys[i] = val;
    }
    /* model key comes right after device root key */
    for (i = 0; i < ctx->skey_wsize; i++) {
        osal_memcpy(&val, dummy->modk + i * OTP_WORD_SIZE, OTP_WORD_SIZE);
        ctx->regs->seckeys[ctx->skey_wsize + i] = val;
    }
    osal_memcpy(&val, &dummy->conf, OTP_WORD_SIZE);
    OTPCTL_REG_SET(ctx->regs, dummy_cfg, val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int otp_get_dummy(struct te_hwa_otpctl *h, te_otp_dummy_t *dummy)
{
    uint32_t i = 0;
    uint32_t val = 0;
    hwa_otpctl_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h || !dummy) {
        return TE_ERROR_BAD_PARAMS;
    }

    osal_memset(dummy, 0, sizeof(*dummy));
    ctx = HWA_OTPCTL_CTX(h);
    TE_ASSERT((ctx->skey_wsize == 4) || (ctx->skey_wsize == 8));
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    val = OTPCTL_REG_GET(ctx->regs, dummy_cfg);
    osal_memcpy(&dummy->conf, &val, sizeof(val));
    /* device root key comes first */
    for (i = 0; i < ctx->skey_wsize; i++) {
        val = ctx->regs->seckeys[i];
        osal_memcpy(dummy->rootk + i * OTP_WORD_SIZE, &val, OTP_WORD_SIZE);
    }
    /* model key comes right after device root key */
    for (i = 0; i < ctx->skey_wsize; i++) {
        val = ctx->regs->seckeys[ctx->skey_wsize + i];
        osal_memcpy(dummy->modk + i * OTP_WORD_SIZE, &val, OTP_WORD_SIZE);
    }
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

int te_hwa_otpctl_alloc( struct te_otpctl_regs *regs,
                         struct te_hwa_host *host,
                         te_hwa_otpctl_t **hwa )
{
    int rc = TE_SUCCESS;
    te_hwa_otpctl_t *otp = NULL;

    if (!regs || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc mem */
    if ((otp = osal_calloc(1, sizeof(*otp))) == NULL) {
        return TE_ERROR_OOM;
    }

    /* init hwa */
    rc = te_hwa_otpctl_init(regs, host, otp);
    if (rc != TE_SUCCESS) {
        osal_free(otp);
        return rc;
    }

    *hwa = otp;
    return TE_SUCCESS;
}

int te_hwa_otpctl_free( te_hwa_otpctl_t *hwa )
{
    int rc = TE_SUCCESS;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    rc = te_hwa_otpctl_exit(hwa);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    osal_memset(hwa, 0, sizeof(*hwa));
    osal_free(hwa);
    return TE_SUCCESS;
}

int te_hwa_otpctl_init( struct te_otpctl_regs *regs,
                        struct te_hwa_host *host,
                        te_hwa_otpctl_t *hwa )
{
    int rc = TE_SUCCESS;
    hwa_otpctl_ctx_t *ctx = NULL;
    te_rtl_conf_t conf = {0};
    uint32_t us_rgn_offs = OTP_OFS_UD_RGN;

    if (!regs || !host || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT(host->stat.conf);
    rc = host->stat.conf(&host->stat, &conf);
    if (rc != TE_SUCCESS) {
        return rc;
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

    if (conf.cfg1.otp_skey_256) {
        us_rgn_offs = 0x70U;
    }
    /* set ops */
    if (conf.cfg1.otp_exist) {
        hwa->get_ctl = otp_get_ctl;
        hwa->set_ctl = otp_set_ctl;
        hwa->state   = otp_state;
        hwa->write   = otp_write;
        ctx->total = us_rgn_offs + conf.cfg1.otp_tst_sz +
                     conf.cfg1.otp_s_sz + conf.cfg1.otp_ns_sz;
    } else {
        hwa->set_dummy = otp_set_dummy;
        hwa->get_dummy = otp_get_dummy;
    }

    /* save the model key and device root key word-size by reading the config in rtl. */
    ctx->skey_wsize = conf.cfg1.otp_skey_256 ? 8 : 4;

    return TE_SUCCESS;

err:
    osal_free(ctx);
    ctx = NULL;
    return rc;
#undef OTP_EXT_OFS_UD_RGN
}

int te_hwa_otpctl_exit( te_hwa_otpctl_t *hwa )
{
    hwa_otpctl_ctx_t *ctx = NULL;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = (hwa_otpctl_ctx_t*)hwa_crypt_ctx(&hwa->base);
    osal_spin_lock_destroy(&ctx->spin);
    osal_memset(ctx, 0, sizeof(*ctx));
    osal_free(ctx);
    hwa_crypt_exit(&hwa->base);
    return TE_SUCCESS;
}

