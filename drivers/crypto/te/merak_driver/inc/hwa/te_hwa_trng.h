//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HWA_TRNG_H__
#define __TRUSTENGINE_HWA_TRNG_H__

#include "te_hwa_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_trng_regs;
struct te_hwa_host;

/**
 * TRNG Core state
 */
typedef enum te_trng_core_state {
    TRNG_CORE_IDLE = 0,
    TRNG_CORE_BUSY_OTH,
    TRNG_CORE_BUSY,
    TRNG_CORE_RESET
} te_trng_core_state_t;

typedef struct te_rnp_ctl {
    uint32_t fill_req:1;
    uint32_t clk_en:1;
} te_rnp_ctl_t;

typedef struct te_rnp_int {
    uint32_t fill_done:1;
} te_rnp_int_t;

typedef struct te_rnp_stat {
    uint32_t ac_tst_err:1;
    uint32_t trng_stat:2;    /**< core state */
} te_rnp_stat_t;

/**
 * Trust engine trng HWA structure
 */
typedef struct te_hwa_trng {
    te_hwa_crypt_t base;
    int (*get_ctl)(struct te_hwa_trng *h, te_rnp_ctl_t *ctl);
    int (*set_ctl)(struct te_hwa_trng *h, const te_rnp_ctl_t *ctl);
    int (*state)(struct te_hwa_trng *h, te_rnp_stat_t *stat);
    int (*int_state)(struct te_hwa_trng *h, te_rnp_int_t *state);
    int (*eoi)(struct te_hwa_trng *h, const te_rnp_int_t *state);
    int (*get_int_msk)(struct te_hwa_trng *h, te_rnp_int_t *msk);
    int (*set_int_msk)(struct te_hwa_trng *h, const te_rnp_int_t *msk);
    int (*read)(struct te_hwa_trng *h, uint8_t *buf, size_t len);
} te_hwa_trng_t;

int te_hwa_trng_alloc( struct te_trng_regs *regs,
                       struct te_hwa_host *host,
                       te_hwa_trng_t **hwa );

int te_hwa_trng_free( te_hwa_trng_t *hwa );

int te_hwa_trng_init( struct te_trng_regs *regs,
                      struct te_hwa_host *host,
                      te_hwa_trng_t *hwa );

int te_hwa_trng_exit( te_hwa_trng_t *hwa );


#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_TRNG_H__ */
