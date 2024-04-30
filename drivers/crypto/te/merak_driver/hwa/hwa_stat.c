//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <hwa/te_hwa_stat.h>
#include "te_regs.h"

/**
 * Wrapout ctx max size unit
 */
#define TE_WRAP_SIZE_UNIT 4U

/**
 * ACA context SRAM size unit
 */
#define TE_CTX_SRAM_UNIT  1024U

/**
 * Derive the STAT hwa ctx pointer from the hwa handler
 */
#define HWA_STAT_CTX(_h) __extension__({                  \
    hwa_stat_ctx_t *_ctx = NULL;                          \
    _ctx = (hwa_stat_ctx_t*)hwa_crypt_ctx(&(_h)->base);   \
    TE_ASSERT(_ctx != NULL);                              \
    _ctx;                                                 \
})

/**
 * Get stat_\p rn.fn field value.
 */
#define STAT_FIELD_GET(val, rn, fn)     HWA_FIELD_GET((val),STAT_##rn,fn)

/**
 * Set stat_\p rn.fn field value to \p fv.
 */
#define STAT_FIELD_SET(val, rn, fn, fv) HWA_FIELD_SET((val),STAT_##rn,fn,(fv))

/**
 * Get stat HWA register
 */
#define STAT_REG_GET(regs, nm) HWA_REG_GET(regs, stat, nm)

/**
 * Top state HWA private context structure
 */
typedef struct hwa_stat_ctx {
    struct te_stat_regs *regs;   /**< State register file */
} hwa_stat_ctx_t;

static int stat_clock_state(struct te_hwa_stat *h, te_clk_stat_t *clk)
{
    hwa_stat_ctx_t *ctx = NULL;

    if (!h || !clk) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_STAT_CTX(h);
    *(uint32_t*)clk = STAT_REG_GET(ctx->regs, clock_status);
    return TE_SUCCESS;
}

static int stat_version(struct te_hwa_stat *h)
{
    hwa_stat_ctx_t *ctx = NULL;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_STAT_CTX(h);
    return LE32TOH(ctx->regs->version);
}

static int stat_state(struct te_hwa_stat *h, te_top_stat_t *stat)
{
    hwa_stat_ctx_t *ctx = NULL;

    if (!h || !stat) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_STAT_CTX(h);
    *(uint32_t*)stat = STAT_REG_GET(ctx->regs, top_stat);
    return TE_SUCCESS;
}

static int stat_conf(struct te_hwa_stat *h, te_rtl_conf_t *conf)
{
    uint32_t val = 0;
    hwa_stat_ctx_t *ctx = NULL;
    union {
        te_rtl_cfg0_t cfg0;
        uint32_t val;
    } u = {0};

    if (!h || !conf) {
        return TE_ERROR_BAD_PARAMS;
    }

    osal_memset(conf, 0, sizeof(*conf));
    ctx = HWA_STAT_CTX(h);

    /* top_cfg0 */
    u.val = STAT_REG_GET(ctx->regs, top_cfg0);
    conf->cfg0 = u.cfg0;

    /* top_cfg1 */
    val = STAT_REG_GET(ctx->regs, top_cfg1);
    conf->cfg1.trng_isrc = STAT_FIELD_GET(val, TOP_CFG1, TRNG_ISRC);
    conf->cfg1.otp_exist = STAT_FIELD_GET(val, TOP_CFG1, OTP_EXIST);
    conf->cfg1.otp_shd_to_ao = STAT_FIELD_GET(val, TOP_CFG1, OTP_SHD_TO_AO);
    conf->cfg1.otp_init_val = STAT_FIELD_GET(val, TOP_CFG1, OTP_INIT_VAL);
    conf->cfg1.otp_skey_256 = STAT_FIELD_GET(val, TOP_CFG1, OTP_SKEY_256);
    /* otp_size0 */
    val = STAT_REG_GET(ctx->regs, otp_size0);
    conf->cfg1.otp_s_sz = STAT_FIELD_GET(val, OTP_SIZE0, OTP_SEC_SIZE) * 4;
    conf->cfg1.otp_ns_sz = STAT_FIELD_GET(val, OTP_SIZE0, OTP_NSEC_SIZE) * 4;
    /* otp_size1 */
    val = STAT_REG_GET(ctx->regs, otp_size1);
    conf->cfg1.otp_tst_sz = STAT_FIELD_GET(val, OTP_SIZE1, OTP_TST_SIZE) * 4;

    /* cq_sram_size */
    val = STAT_REG_GET(ctx->regs, cq_sram_size);
    conf->sram.hash_cq_depth = STAT_FIELD_GET(val, CQ_SRAM_SIZE, HASH_CQ_DEPTH);
    conf->sram.sca_cq_depth = STAT_FIELD_GET(val, CQ_SRAM_SIZE, SCA_CQ_DEPTH);
    conf->sram.aca_cq_depth = STAT_FIELD_GET(val, CQ_SRAM_SIZE, ACA_CQ_DEPTH);
    conf->sram.wrap_max_sz =
        STAT_FIELD_GET(val, CQ_SRAM_SIZE, CTX_MAX_SIZE) * 4;
    conf->sram.aca_sram_sz =
        STAT_FIELD_GET(val, CQ_SRAM_SIZE, ACA_SRAM_SIZE) * TE_CTX_SRAM_UNIT;

    /* ctx_sram */
    val = STAT_REG_GET(ctx->regs, ctx_sram);
    conf->sram.ctx_sram_sz =
        STAT_FIELD_GET(val, CTX_SRAM, CTX_SRAM_SIZE) * 4;

    /* dma_fifo_depth */
    val = STAT_REG_GET(ctx->regs, dma_fifo_depth);
    conf->dma.sca_rfifo_depth = STAT_FIELD_GET(val, DMA_FIFO_DEPTH, SCA_RD);
    conf->dma.sca_wfifo_depth = STAT_FIELD_GET(val, DMA_FIFO_DEPTH, SCA_WR);
    conf->dma.hash_rfifo_depth = STAT_FIELD_GET(val, DMA_FIFO_DEPTH, HASH_RD);
    conf->dma.hash_wfifo_depth = STAT_FIELD_GET(val, DMA_FIFO_DEPTH, HASH_WR);

    /* dma_axi_stat */
    val = STAT_REG_GET(ctx->regs, dma_axi_stat);
    conf->dma.addr_width = STAT_FIELD_GET(val, DMA_AXI_STAT, ADDR_WIDTH);
    conf->dma.rd_outstd = STAT_FIELD_GET(val, DMA_AXI_STAT, RD_OUTSTD);
    conf->dma.wr_outstd = STAT_FIELD_GET(val, DMA_AXI_STAT, WR_OUTSTD);
    conf->dma.rd_ch_en = !!STAT_FIELD_GET(val, DMA_AXI_STAT, RD_CH_EN);
    conf->dma.wr_ch_en = !!STAT_FIELD_GET(val, DMA_AXI_STAT, WR_CH_EN);

    return TE_SUCCESS;
}

static int stat_host_conf(struct te_hwa_stat *h, int n, te_host_conf_t *conf)
{
    union {
        te_host_conf_t conf;
        uint32_t val;
    } u = {0};
    hwa_stat_ctx_t *ctx = NULL;

    if (!h || n < 0 || n > (int)TE_MAX_HOST_NUM || !conf) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_STAT_CTX(h);
    u.val = STAT_REG_GET(&ctx->regs->host[n], cfg_host);

    *conf = u.conf;
    return TE_SUCCESS;
}

static int stat_host_state(struct te_hwa_stat *h, int n, te_host_int_t *stat)
{
    union {
        te_host_int_t stat;
        uint32_t val;
    } u = {0};
    hwa_stat_ctx_t *ctx = NULL;

    if (!h || n < 0 || n > (int)TE_MAX_HOST_NUM || !stat) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_STAT_CTX(h);
    u.val = STAT_REG_GET(&ctx->regs->host[n], intr_host);

    *stat = u.stat;
    return TE_SUCCESS;
}

/**
* get the number of host.
*/
static int stat_host_num(struct te_hwa_stat *h)
{
    int n = 0;
    stat_top_cfg0Reg_t cfg0;
    hwa_stat_ctx_t *ctx = NULL;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_STAT_CTX(h);
    cfg0.val = STAT_REG_GET(ctx->regs, top_cfg0);
    n = cfg0.bits.hash_host_num;
    if (n < cfg0.bits.sca_host_num)
        n = cfg0.bits.sca_host_num;
    if (n < cfg0.bits.aca_host_num)
        n = cfg0.bits.aca_host_num;
    if (n < cfg0.bits.rnp_host_num)
        n = cfg0.bits.rnp_host_num;

    return n;
}

int te_hwa_stat_alloc( struct te_stat_regs *regs,
                       struct te_hwa_host *host,
                       te_hwa_stat_t **hwa )
{
    int rc = TE_SUCCESS;
    te_hwa_stat_t *stat = NULL;

    if (!regs || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc mem */
    if ((stat = osal_calloc(1, sizeof(*stat))) == NULL) {
        return TE_ERROR_OOM;
    }

    /* init hwa */
    rc = te_hwa_stat_init(regs, host, stat);
    if (rc != TE_SUCCESS) {
        osal_free(stat);
        return rc;
    }

    *hwa = stat;
    return TE_SUCCESS;
}

int te_hwa_stat_free( te_hwa_stat_t *hwa )
{
    int rc = TE_SUCCESS;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    rc = te_hwa_stat_exit(hwa);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    osal_memset(hwa, 0, sizeof(*hwa));
    osal_free(hwa);
    return TE_SUCCESS;
}

int te_hwa_stat_init( struct te_stat_regs *regs,
                      struct te_hwa_host *host,
                      te_hwa_stat_t *hwa )
{
    hwa_stat_ctx_t *ctx = NULL;

    if (!regs || !host || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    if ((ctx = osal_calloc(1, sizeof(*ctx))) == NULL) {
        return TE_ERROR_OOM;
    }

    ctx->regs = regs;
    osal_memset(hwa, 0, sizeof(*hwa));
    hwa_crypt_init(&hwa->base, host, (void*)ctx);

    /* set ops */
    hwa->clock_state = stat_clock_state;
    hwa->version = stat_version;
    hwa->state = stat_state;
    hwa->conf = stat_conf;
    hwa->host_conf = stat_host_conf;
    hwa->host_state = stat_host_state;
    hwa->host_num = stat_host_num;

    return TE_SUCCESS;
}

int te_hwa_stat_exit( te_hwa_stat_t *hwa )
{
    hwa_stat_ctx_t *ctx = NULL;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = (hwa_stat_ctx_t*)hwa_crypt_ctx(&hwa->base);
    osal_memset(ctx, 0, sizeof(*ctx));
    osal_free(ctx);
    hwa_crypt_exit(&hwa->base);
    return TE_SUCCESS;
}

