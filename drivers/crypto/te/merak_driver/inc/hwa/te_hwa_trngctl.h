//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HWA_TRNGCTL_H__
#define __TRUSTENGINE_HWA_TRNGCTL_H__

#include "te_hwa_common.h"
#include "te_hwa_trng_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_trngctl_regs;
struct te_hwa_host;

typedef struct te_trng_ctl {
    uint32_t hw_key_gen:1;
    uint32_t rsvd:3;
    uint32_t sreset:1;
} te_trng_ctl_t;

typedef struct te_trng_stat {
    uint32_t hw_key_valid:1;
} te_trng_stat_t;

typedef struct te_trng_int {
    uint32_t vn_err:1;
    uint32_t crng_err:1;
    uint32_t rep_tst_err:1;
    uint32_t adap_tst_err:1;
} te_trng_int_t;

typedef struct te_trng_err_cnt {
    uint32_t vn_cnt;              /* vn err total, read clear */
    uint32_t crng_cnt;            /* crng err total, read clear */
    uint32_t rep_cnt;             /* rep err total, read clear */
    uint32_t adap_cnt;            /* adapt err total, read clear */
} te_trng_err_cnt_t;

/**
 * Trust engine trng control HWA structure
 */
typedef struct te_hwa_trngctl {
    te_hwa_crypt_t base;
    int (*get_conf)(struct te_hwa_trngctl *h, te_trng_conf_t *conf);
    int (*setup)(struct te_hwa_trngctl *h, const te_trng_conf_t *conf);
    int (*get_ctl)(struct te_hwa_trngctl *h, te_trng_ctl_t *ctl);
    int (*set_ctl)(struct te_hwa_trngctl *h, const te_trng_ctl_t *ctl);
    int (*state)(struct te_hwa_trngctl *h, te_trng_stat_t *stat);
    int (*int_state)(struct te_hwa_trngctl *h, te_trng_int_t *status);
    int (*eoi)(struct te_hwa_trngctl *h, const te_trng_int_t *status);
    int (*get_int_msk)(struct te_hwa_trngctl *h, te_trng_int_t *msk);
    int (*set_int_msk)(struct te_hwa_trngctl *h, const te_trng_int_t *msk);
    int (*get_err_cnt)(struct te_hwa_trngctl *h, te_trng_err_cnt_t *cnt);
} te_hwa_trngctl_t;

int te_hwa_trngctl_alloc( struct te_trngctl_regs *regs,
                          struct te_hwa_host *host,
                          te_hwa_trngctl_t **hwa );

int te_hwa_trngctl_free( te_hwa_trngctl_t *hwa );

int te_hwa_trngctl_init( struct te_trngctl_regs *regs,
                         struct te_hwa_host *host,
                         te_hwa_trngctl_t *hwa );

int te_hwa_trngctl_exit( te_hwa_trngctl_t *hwa );


#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_TRNGCTL_H__ */
