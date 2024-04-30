//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRNGCTL_ERR_CNT_TH_REG_H__
#define __TRNGCTL_ERR_CNT_TH_REG_H__

#define TRNGCTL_ERR_CNT_TH_ADAP_ERR_TH_SHIFT 24 /**< ADAPT error counter threshold */
#define TRNGCTL_ERR_CNT_TH_ADAP_ERR_TH_WIDTH 8
#define TRNGCTL_ERR_CNT_TH_REP_ERR_TH_SHIFT 16 /**< REPET error counter threshold */
#define TRNGCTL_ERR_CNT_TH_REP_ERR_TH_WIDTH 8
#define TRNGCTL_ERR_CNT_TH_CRNG_ERR_TH_SHIFT 8 /**< CRNG error counter threshold */
#define TRNGCTL_ERR_CNT_TH_CRNG_ERR_TH_WIDTH 8
#define TRNGCTL_ERR_CNT_TH_VN_ERR_TH_SHIFT 0 /**< VN error counter threshold */
#define TRNGCTL_ERR_CNT_TH_VN_ERR_TH_WIDTH 4

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * trngctl_err_cnt_th register definition.
 */
typedef union trngctl_err_cnt_thReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        vn_err_th: 4  /**< VN error counter threshold */,
        hole8: 4,
        crng_err_th: 8  /**< CRNG error counter threshold */,
        rep_err_th: 8  /**< REPET error counter threshold */,
        adap_err_th: 8  /**< ADAPT error counter threshold */; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} trngctl_err_cnt_thReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_trngctl_err_cnt_th(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("trngctl_err_cnt_th: 0x%08x is\n", x);


    t = (x >> TRNGCTL_ERR_CNT_TH_ADAP_ERR_TH_SHIFT) & ((1U << TRNGCTL_ERR_CNT_TH_ADAP_ERR_TH_WIDTH) - 1);
    BITFIELD_LOG(" adap_err_th=0x%x (ADAPT error counter threshold)\n", t);
    x &= ~(((1U << TRNGCTL_ERR_CNT_TH_ADAP_ERR_TH_WIDTH) - 1) << TRNGCTL_ERR_CNT_TH_ADAP_ERR_TH_SHIFT);


    t = (x >> TRNGCTL_ERR_CNT_TH_REP_ERR_TH_SHIFT) & ((1U << TRNGCTL_ERR_CNT_TH_REP_ERR_TH_WIDTH) - 1);
    BITFIELD_LOG(" rep_err_th=0x%x (REPET error counter threshold)\n", t);
    x &= ~(((1U << TRNGCTL_ERR_CNT_TH_REP_ERR_TH_WIDTH) - 1) << TRNGCTL_ERR_CNT_TH_REP_ERR_TH_SHIFT);


    t = (x >> TRNGCTL_ERR_CNT_TH_CRNG_ERR_TH_SHIFT) & ((1U << TRNGCTL_ERR_CNT_TH_CRNG_ERR_TH_WIDTH) - 1);
    BITFIELD_LOG(" crng_err_th=0x%x (CRNG error counter threshold)\n", t);
    x &= ~(((1U << TRNGCTL_ERR_CNT_TH_CRNG_ERR_TH_WIDTH) - 1) << TRNGCTL_ERR_CNT_TH_CRNG_ERR_TH_SHIFT);


    t = (x >> TRNGCTL_ERR_CNT_TH_VN_ERR_TH_SHIFT) & ((1U << TRNGCTL_ERR_CNT_TH_VN_ERR_TH_WIDTH) - 1);
    BITFIELD_LOG(" vn_err_th=0x%x (VN error counter threshold)\n", t);
    x &= ~(((1U << TRNGCTL_ERR_CNT_TH_VN_ERR_TH_WIDTH) - 1) << TRNGCTL_ERR_CNT_TH_VN_ERR_TH_SHIFT);

    if (x) {
        int i = 0;
        BITFIELD_LOG(" (Unknown :");
        while (x) {
            if (x & 1) {
                BITFIELD_LOG(" %d", i);
            }
            x >>= 1;
            i++;
        }
        BITFIELD_LOG(")\n");
    }

    BITFIELD_LOG("\n");
}

#else  /* WITH_BITFIELD_LOG */

#define log_trngctl_err_cnt_th(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNGCTL_ERR_CNT_TH_REG_H__ */
