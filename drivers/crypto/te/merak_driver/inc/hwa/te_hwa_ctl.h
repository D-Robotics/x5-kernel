//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HWA_CTL_H__
#define __TRUSTENGINE_HWA_CTL_H__

#include "te_hwa_common.h"
#include "te_hwa_stat.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_ctl_regs;
struct te_hwa_host;

/**
 * Trust engine module enumeration
 */
typedef enum te_module {
    TE_MOD_HASH = 1,
    TE_MOD_SCA  = (1 << 1),
    TE_MOD_ACA  = (1 << 2),
    TE_MOD_TRNG = (1 << 3),
    TE_MOD_OTP  = (1 << 4),
    TE_MOD_DMA  = (1 << 5)
} te_module_t;

/**
 * Arbitration algorithms for multiple hosts
 */
typedef enum te_arb_alg {
    TE_ARB_ALG_FIXED = 0,        /**< secure takes precedence over NS */
    TE_ARB_ALG_RR                /**< round-robin */
} te_arb_algo_t;

/**
 * Arbitration granularities for multiple hosts
 */
typedef enum te_arb_gran {
    TE_ARB_GRAN_CMD = 0,         /**< schedule after each command */
    TE_ARB_GRAN_CQ,              /**< schedule when CQ is empty */
} te_arb_gran_t;

/**
 * Top configuration structure
 */
typedef struct te_top_conf {
    uint32_t ctx_pool_lock:1;
    uint32_t sw_init_done:1;
    uint32_t hash_arb_gran:1;
    uint32_t hash_arb_alg:1;
    uint32_t sca_arb_gran:1;
    uint32_t sca_arb_alg:1;
    uint32_t aca_arb_gran:1;
    uint32_t aca_arb_alg:1;
    uint32_t rnp_arb_gran:1;
    uint32_t rnp_arb_alg:1;
} te_top_conf_t;

/**
 * Trust engine top control structure
 */
typedef struct te_hwa_ctl {
    te_hwa_crypt_t base;
    int (*clock_ctl)(struct te_hwa_ctl *h, te_module_t mod, bool state);
    int (*sreset)(struct te_hwa_ctl *h, te_module_t mod);
    int (*top_conf)(struct te_hwa_ctl *h, te_top_conf_t *conf);
    int (*conf_top)(struct te_hwa_ctl *h, const te_top_conf_t *conf);
    int (*host_conf)(struct te_hwa_ctl *h, int n, te_host_conf_t *conf);
    int (*conf_host)(struct te_hwa_ctl *h, int n, const te_host_conf_t *conf);
} te_hwa_ctl_t;

int te_hwa_ctl_alloc( struct te_ctl_regs *regs,
                      struct te_hwa_host *host,
                      te_hwa_ctl_t **hwa );

int te_hwa_ctl_free( te_hwa_ctl_t *hwa );

int te_hwa_ctl_init( struct te_ctl_regs *regs,
                     struct te_hwa_host *host,
                     te_hwa_ctl_t *hwa );

int te_hwa_ctl_exit( te_hwa_ctl_t *hwa );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_CTL_H__ */
