//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HWA_OTPCTL_H__
#define __TRUSTENGINE_HWA_OTPCTL_H__

#include "te_hwa_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OTP_WR_TRIG   1

#ifndef __ASSEMBLY__

struct te_otpctl_regs;
struct te_hwa_host;

typedef enum te_ao_shdw_valid {
    SHDW_VALID_MID   = (1 << 0),
    SHDW_VALID_MODK  = (1 << 1),
    SHDW_VALID_DID   = (1 << 2),
    SHDW_VALID_ROOTK = (1 << 3),
    SHDW_VALID_LCS   = (1 << 4),
    SHDW_VALID_LOCK  = (1 << 5),
} te_ao_shdw_valid_t;

typedef struct te_otp_stat {
    uint32_t init_done:1;
    uint32_t lcs_load_fail:1;
    uint32_t mid_load_fail:1;
    uint32_t did_load_fail:1;
    uint32_t key_load_fail:1;
    uint32_t lock_load_fail:1;
    uint32_t data_inv_fail:1;
    uint32_t lcs_err:1;
    uint32_t rsvd:8;
    uint32_t up_busy:1;
    uint32_t up_fail:1;
    uint32_t otp_rdy:1;
    uint32_t shdw_valid:6;
} te_otp_stat_t;

typedef struct te_otp_ctl {
    uint32_t direct_rd:1;
    uint32_t general_ctl:8;
} te_otp_ctl_t;

typedef struct te_otp_dummy_conf {
    uint32_t lcs_dr:1;
    uint32_t lcs_dd:1;
    uint32_t lcs_dm:1;
    uint32_t lcs_cm:1;
    uint32_t lcs_valid:1;
    uint32_t key_valid:1;
} te_otp_dummy_conf_t;

typedef struct te_otp_dummy {
    te_otp_dummy_conf_t conf;
    uint8_t rootk[32];
    uint8_t modk[32];
} te_otp_dummy_t;

/**
 * Trust engine otp manage HWA structure
 */
typedef struct te_hwa_otpctl {
    te_hwa_crypt_t base;
    int (*get_ctl)(struct te_hwa_otpctl *h, te_otp_ctl_t *ctl);
    int (*set_ctl)(struct te_hwa_otpctl *h, const te_otp_ctl_t *ctl);
    int (*state)(struct te_hwa_otpctl *h, te_otp_stat_t *stat);
    int (*write)(struct te_hwa_otpctl *h, uint32_t off,
                 const uint8_t *buf, uint32_t len);
    int (*set_dummy)(struct te_hwa_otpctl *h, const te_otp_dummy_t *dummy);
    int (*get_dummy)(struct te_hwa_otpctl *h, te_otp_dummy_t *dummy);
} te_hwa_otpctl_t;

int te_hwa_otpctl_alloc( struct te_otpctl_regs *regs,
                         struct te_hwa_host *host,
                         te_hwa_otpctl_t **hwa );

int te_hwa_otpctl_free( te_hwa_otpctl_t *hwa );

int te_hwa_otpctl_init( struct te_otpctl_regs *regs,
                        struct te_hwa_host *host,
                        te_hwa_otpctl_t *hwa );

int te_hwa_otpctl_exit( te_hwa_otpctl_t *hwa );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_OTPCTL_H__ */
