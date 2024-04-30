//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HWA_TRNG_TYPE_H__
#define __TRUSTENGINE_HWA_TRNG_TYPE_H__

#include "te_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

typedef struct te_trng_entropy_src {
    uint32_t grp0_en:1;
    uint32_t grp1_en:1;
    uint32_t grp2_en:1;
    uint32_t grp3_en:1;
    uint32_t src_sel:1;
} te_trng_entropy_src_t;

typedef struct te_trng_entropy_sample {
    uint32_t div:16;
    uint32_t dly:8;
} te_trng_entropy_sample_t;

typedef struct te_trng_postproc {
    uint32_t prng_bypass:1;
    uint32_t vn_bypass:1;
    uint32_t crng_bypass:1;
    uint32_t rsvd:8;
    uint32_t lfsr_drop_num:2;
    uint32_t lfsr_sel:1;
    uint32_t fast_mode:1;
} te_trng_postproc_t;

typedef struct te_trng_eval {
    uint32_t adap_tst_th:10;
    uint32_t rep_tst_th:6;
    uint32_t adap_tst_en:1;
    uint32_t rep_tst_en:1;
    uint32_t ac_tst_en:1;
    uint32_t ac_tst_th:4;
} te_trng_eval_t;

typedef struct te_trng_ro {
    uint32_t grp0_taps_en:3;
    uint32_t rsvd3:1;
    uint32_t grp1_taps_en:3;
    uint32_t rsvd7:1;
    uint32_t grp2_taps_en:3;
    uint32_t rsvd11:1;
    uint32_t grp3_taps_en:3;
    uint32_t rsvd15:1;
} te_trng_ro_t;

typedef struct te_trng_err_thr {
    uint32_t vn_err_th:4;
    uint32_t rsvd:4;
    uint32_t crng_err_th:8;
    uint32_t rep_err_th:8;
    uint32_t adap_err_th:8;
} te_trng_err_thr_t;

typedef struct te_trng_conf {
    te_trng_entropy_src_t src;
    te_trng_entropy_sample_t sample;
    te_trng_ro_t ro;
    te_trng_postproc_t postproc;
    te_trng_eval_t eval;
    te_trng_err_thr_t thr;
} te_trng_conf_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_TRNG_TYPE_H__ */
