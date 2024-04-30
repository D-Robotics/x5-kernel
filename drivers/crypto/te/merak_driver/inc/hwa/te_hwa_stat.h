//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HWA_STAT_H__
#define __TRUSTENGINE_HWA_STAT_H__

#include "te_hwa_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_stat_regs;
struct te_hwa_host;

/**
 * Valid of shadow registers of AO modules.
 */
typedef enum te_ao_valid {
    TE_AO_VALID_MID  = 1,
    TE_AO_VALID_MODK = (1 << 1),
    TE_AO_VALID_DID  = (1 << 2),
    TE_AO_VALID_ROTK = (1 << 3),
    TE_AO_VALID_LCS  = (1 << 4),
    TE_AO_VALID_LOCK = (1 << 5),
} te_ao_valid_t;

/**
 * Top status structure
 * Refer to stat_top_statReg_t;
 */
typedef struct te_top_stat {
    uint32_t hash_sreset:1;
    uint32_t sca_sreset:1;
    uint32_t aca_sreset:1;
    uint32_t otp_sreset:1;
    uint32_t trng_sreset:1;
    uint32_t dma_sreset:1;
    uint32_t sw_init_done:1;
    uint32_t otp_rdy:1;
    uint32_t rsvd8:8;
    uint32_t ao_shd_valid:6;
    uint32_t otp_init_done:1;
    uint32_t hw_key_valid:1;
    uint32_t ctx_pool_rdy:1;
} te_top_stat_t;

/**
 * RTL configuration0
 */
typedef struct te_rtl_cfg0 {
    uint32_t hash_nhost:5;
    uint32_t sca_nhost:5;
    uint32_t aca_nhost:5;
    uint32_t trng_nhost:5;
    uint32_t rsvd20:8;
    uint32_t sca_ekasumi:1;      /**< whether KASUMI exists in SCA */
    uint32_t sca_ezuc:1;         /**< whether ZUC exists in SCA */
    uint32_t sca_esnow3g:1;      /**< whether SNOW3G exists in SCA */
    uint32_t wrap_enc:1;
} te_rtl_cfg0_t;

/**
 * RTL configuration1
 */
typedef struct te_rtl_cfg1 {
    uint32_t trng_isrc:1;
    uint32_t otp_exist:1;
    uint32_t otp_shd_to_ao:1;
    uint32_t otp_init_val:1;
    uint32_t otp_skey_256:1;
    uint16_t otp_tst_sz;         /**< test region size in byte */
    uint16_t otp_s_sz;           /**< sec region size in byte */
    uint16_t otp_ns_sz;          /**< ns region size in byte */
} te_rtl_cfg1_t;

/**
 * SRAM configurations
 */
typedef struct te_sram_cfg {
    uint32_t hash_cq_depth:5;
    uint32_t sca_cq_depth:5;
    uint32_t aca_cq_depth:5;
    uint32_t ctx_sram_sz;        /**< SCA/HASH context SRAM size in byte */
    uint32_t wrap_max_sz;        /**< wrapout_ctx_max_size in byte */
    uint32_t aca_sram_sz;        /**< ACA SRAM size in byte */
} te_sram_cfg_t;

/**
 * DMA configurations
 */
typedef struct te_dma_cfg {
    uint8_t sca_rfifo_depth;
    uint8_t sca_wfifo_depth;
    uint8_t hash_rfifo_depth;
    uint8_t hash_wfifo_depth;
    uint8_t addr_width;
    uint8_t rd_outstd;
    uint8_t wr_outstd;
    bool rd_ch_en;
    bool wr_ch_en;
} te_dma_cfg_t;

/**
 * RTL configurations
 */
typedef struct te_rtl_conf {
    te_rtl_cfg0_t cfg0;
    te_rtl_cfg1_t cfg1;
    te_sram_cfg_t sram;
    te_dma_cfg_t dma;
} te_rtl_conf_t;

/**
 * Clock status structure
 * Refer to stat_clock_statusReg_t
 */
typedef struct te_clk_stat {
    uint32_t hash_en:1;
    uint32_t sca_en:1;
    uint32_t aca_en:1;
    uint32_t trng_en:1;
    uint32_t otp_en:1;
    uint32_t rsvd5:1;
    uint32_t dma_sca_en:1;
    uint32_t dma_hash_en:1;
    uint32_t dma_axi_en:1;
} te_clk_stat_t;

/**
 * Merak version structure
 */
typedef struct te_merak_ver {
    uint32_t minor:16;
    uint32_t major:8;
    uint32_t phase:8;
} te_merak_ver_t;

/**
 * Host configuration structure
 */
typedef struct te_host_conf {
    uint32_t hash_nctx1:6;
    uint32_t hash_nctx2:6;
    uint32_t sca_nctx1:6;
    uint32_t sca_nctx2:6;
    uint32_t hash_sec:1;
    uint32_t sca_sec:1;
    uint32_t aca_sec:1;
    uint32_t rnp_sec:1;
} te_host_conf_t;

/**
 * Host INT state structure
 */
typedef struct te_host_int {
    /**
     * Common interrupts
     */
    uint32_t hash:1;
    uint32_t sca:1;
    uint32_t aca:1;
    uint32_t rnp:1;
    /**
     * Host0 only interrupts
     */
    uint32_t trng:1;
    uint32_t ctxp_err:1;         /**< context pool overflow err */
} te_host_int_t;

/**
 * Trust engine status HWA structure
 */
typedef struct te_hwa_stat {
    te_hwa_crypt_t base;
    int (*clock_state)(struct te_hwa_stat *h, te_clk_stat_t *clk);
    int (*version)(struct te_hwa_stat *h);
    int (*state)(struct te_hwa_stat *h, te_top_stat_t *stat);
    int (*conf)(struct te_hwa_stat *h, te_rtl_conf_t *conf);
    int (*host_conf)(struct te_hwa_stat *h, int n, te_host_conf_t *conf);
    int (*host_state)(struct te_hwa_stat *h, int n, te_host_int_t *stat);
    /**
     * get the number of host.
     */
    int (*host_num)(struct te_hwa_stat *h);
} te_hwa_stat_t;

int te_hwa_stat_alloc( struct te_stat_regs *regs,
                       struct te_hwa_host *host,
                       te_hwa_stat_t **hwa );

int te_hwa_stat_free( te_hwa_stat_t *hwa );

int te_hwa_stat_init( struct te_stat_regs *regs,
                      struct te_hwa_host *host,
                      te_hwa_stat_t *hwa );

int te_hwa_stat_exit( te_hwa_stat_t *hwa );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_STAT_H__ */
