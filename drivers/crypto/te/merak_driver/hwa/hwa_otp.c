//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <hwa/te_hwa_otp.h>
#include <hwa/te_hwa.h>
#include "te_regs.h"

/**
 * Derive the OTP hwa ctx pointer from the hwa handler
 */
#define HWA_OTP_CTX(_h) __extension__({                   \
    hwa_otp_ctx_t *_ctx = NULL;                           \
    _ctx = (hwa_otp_ctx_t*)hwa_crypt_ctx(&(_h)->base);    \
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
 * OTP HWA private context structure
 */
typedef struct hwa_otp_ctx {
    struct te_otp_regs *regs;   /**< OTP register file */
    uint32_t total;             /**< OTP total length, in byte */
} hwa_otp_ctx_t;

static int otp_read(struct te_hwa_otp *h, uint32_t off,
                    uint8_t *buf, uint32_t len)
{
#define OTP_WORD_SIZE 4
    uint32_t val = 0;
    uint32_t pos = off;
    hwa_otp_ctx_t *ctx = NULL;

    if (!h || (!buf && len != 0)) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (off > off + len) {
        return TE_ERROR_OVERFLOW;
    }

    ctx = HWA_OTP_CTX(h);
    if (off + len > ctx->total) {
        return TE_ERROR_EXCESS_DATA;
    }

    if (0 == len) {
        return TE_SUCCESS;
    }

    /* leading non-aligned-word */
    if (pos & (OTP_WORD_SIZE - 1)) {
        uint32_t olen = 0;
        val = HWA_OTP_READ(pos & ~(OTP_WORD_SIZE - 1));
        olen = (len <= (OTP_WORD_SIZE - (pos & (OTP_WORD_SIZE - 1)))) ? len :
               (OTP_WORD_SIZE - (pos & (OTP_WORD_SIZE - 1)));
        osal_memcpy(buf, (uint8_t*)&val + (pos & (OTP_WORD_SIZE - 1)), olen);
        pos += olen;
    }

    /* complete words */
    for (; off + len - pos >= OTP_WORD_SIZE; pos += OTP_WORD_SIZE) {
        val = HWA_OTP_READ(pos);
        osal_memcpy(buf + pos - off, &val, OTP_WORD_SIZE);
    }

    /* tail non-aligend-word */
    if (pos < off + len) {
        val = HWA_OTP_READ(pos);
        osal_memcpy(buf + pos - off, &val, off + len - pos);
        pos = off + len;
    }

    return TE_SUCCESS;
}

int te_hwa_otp_alloc( struct te_otp_regs *regs,
                      struct te_hwa_host *host,
                      te_hwa_otp_t **hwa )
{
    int rc = TE_SUCCESS;
    te_hwa_otp_t *otp = NULL;

    if (!regs || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc mem */
    if ((otp = osal_calloc(1, sizeof(*otp))) == NULL) {
        return TE_ERROR_OOM;
    }

    /* init hwa */
    rc = te_hwa_otp_init(regs, host, otp);
    if (rc != TE_SUCCESS) {
        osal_free(otp);
        return rc;
    }

    *hwa = otp;
    return TE_SUCCESS;
}

int te_hwa_otp_free( te_hwa_otp_t *hwa )
{
    int rc = TE_SUCCESS;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    rc = te_hwa_otp_exit(hwa);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    osal_memset(hwa, 0, sizeof(*hwa));
    osal_free(hwa);
    return TE_SUCCESS;
}

int te_hwa_otp_init( struct te_otp_regs *regs,
                     struct te_hwa_host *host,
                     te_hwa_otp_t *hwa )
{
    int rc = TE_SUCCESS;
    hwa_otp_ctx_t *ctx = NULL;
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

    ctx->regs = regs;
    osal_memset(hwa, 0, sizeof(*hwa));
    hwa_crypt_init(&hwa->base, host, (void*)ctx);

    if (conf.cfg1.otp_skey_256) {
        us_rgn_offs = 0x70U;
    }
    /* set ops */
    if (conf.cfg1.otp_exist) {
        hwa->read = otp_read;
        ctx->total = us_rgn_offs + conf.cfg1.otp_tst_sz +
                     conf.cfg1.otp_s_sz + conf.cfg1.otp_ns_sz;
    }

    return TE_SUCCESS;
}

int te_hwa_otp_exit( te_hwa_otp_t *hwa )
{
    hwa_otp_ctx_t *ctx = NULL;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = (hwa_otp_ctx_t*)hwa_crypt_ctx(&hwa->base);
    osal_memset(ctx, 0, sizeof(*ctx));
    osal_free(ctx);
    hwa_crypt_exit(&hwa->base);
    return TE_SUCCESS;
}

