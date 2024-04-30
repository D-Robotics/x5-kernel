//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRNGCTL_EVAL_SETTING_REG_H__
#define __TRNGCTL_EVAL_SETTING_REG_H__

#define TRNGCTL_EVAL_SETTING_AC_TST_TH_SHIFT 19 /**< autocorrelation test threshold */
#define TRNGCTL_EVAL_SETTING_AC_TST_TH_WIDTH 4
#define TRNGCTL_EVAL_SETTING_AC_TST_EN_SHIFT 18 /**< autocorrelation test enable */
#define TRNGCTL_EVAL_SETTING_AC_TST_EN_WIDTH 1
#define TRNGCTL_EVAL_SETTING_REP_TST_EN_SHIFT 17 /**< repetition test enalbe */
#define TRNGCTL_EVAL_SETTING_REP_TST_EN_WIDTH 1
#define TRNGCTL_EVAL_SETTING_ADAP_TST_EN_SHIFT 16 /**< adaptive test enable */
#define TRNGCTL_EVAL_SETTING_ADAP_TST_EN_WIDTH 1
#define TRNGCTL_EVAL_SETTING_REP_TST_TH_SHIFT 10 /**< repetition test cutoff threshold */
#define TRNGCTL_EVAL_SETTING_REP_TST_TH_WIDTH 6
#define TRNGCTL_EVAL_SETTING_ADAP_TST_TH_SHIFT 0 /**< adaptive test cutoff threshold */
#define TRNGCTL_EVAL_SETTING_ADAP_TST_TH_WIDTH 10

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * trngctl_eval_setting register definition.
 */
typedef union trngctl_eval_settingReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        adap_tst_th: 10  /**< adaptive test cutoff threshold */,
        rep_tst_th: 6  /**< repetition test cutoff threshold */,
        adap_tst_en: 1  /**< adaptive test enable */,
        rep_tst_en: 1  /**< repetition test enalbe */,
        ac_tst_en: 1  /**< autocorrelation test enable */,
        ac_tst_th: 4  /**< autocorrelation test threshold */,
        hole0: 9; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} trngctl_eval_settingReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_trngctl_eval_setting(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("trngctl_eval_setting: 0x%08x is\n", x);


    t = (x >> TRNGCTL_EVAL_SETTING_AC_TST_TH_SHIFT) & ((1U << TRNGCTL_EVAL_SETTING_AC_TST_TH_WIDTH) - 1);
    BITFIELD_LOG(" ac_tst_th=0x%x (autocorrelation test threshold)\n", t);
    x &= ~(((1U << TRNGCTL_EVAL_SETTING_AC_TST_TH_WIDTH) - 1) << TRNGCTL_EVAL_SETTING_AC_TST_TH_SHIFT);


    t = (x >> TRNGCTL_EVAL_SETTING_AC_TST_EN_SHIFT) & ((1U << TRNGCTL_EVAL_SETTING_AC_TST_EN_WIDTH) - 1);
    BITFIELD_LOG(" ac_tst_en=0x%x (autocorrelation test enable)\n", t);
    x &= ~(((1U << TRNGCTL_EVAL_SETTING_AC_TST_EN_WIDTH) - 1) << TRNGCTL_EVAL_SETTING_AC_TST_EN_SHIFT);


    t = (x >> TRNGCTL_EVAL_SETTING_REP_TST_EN_SHIFT) & ((1U << TRNGCTL_EVAL_SETTING_REP_TST_EN_WIDTH) - 1);
    BITFIELD_LOG(" rep_tst_en=0x%x (repetition test enalbe)\n", t);
    x &= ~(((1U << TRNGCTL_EVAL_SETTING_REP_TST_EN_WIDTH) - 1) << TRNGCTL_EVAL_SETTING_REP_TST_EN_SHIFT);


    t = (x >> TRNGCTL_EVAL_SETTING_ADAP_TST_EN_SHIFT) & ((1U << TRNGCTL_EVAL_SETTING_ADAP_TST_EN_WIDTH) - 1);
    BITFIELD_LOG(" adap_tst_en=0x%x (adaptive test enable)\n", t);
    x &= ~(((1U << TRNGCTL_EVAL_SETTING_ADAP_TST_EN_WIDTH) - 1) << TRNGCTL_EVAL_SETTING_ADAP_TST_EN_SHIFT);


    t = (x >> TRNGCTL_EVAL_SETTING_REP_TST_TH_SHIFT) & ((1U << TRNGCTL_EVAL_SETTING_REP_TST_TH_WIDTH) - 1);
    BITFIELD_LOG(" rep_tst_th=0x%x (repetition test cutoff threshold)\n", t);
    x &= ~(((1U << TRNGCTL_EVAL_SETTING_REP_TST_TH_WIDTH) - 1) << TRNGCTL_EVAL_SETTING_REP_TST_TH_SHIFT);


    t = (x >> TRNGCTL_EVAL_SETTING_ADAP_TST_TH_SHIFT) & ((1U << TRNGCTL_EVAL_SETTING_ADAP_TST_TH_WIDTH) - 1);
    BITFIELD_LOG(" adap_tst_th=0x%x (adaptive test cutoff threshold)\n", t);
    x &= ~(((1U << TRNGCTL_EVAL_SETTING_ADAP_TST_TH_WIDTH) - 1) << TRNGCTL_EVAL_SETTING_ADAP_TST_TH_SHIFT);

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

#define log_trngctl_eval_setting(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNGCTL_EVAL_SETTING_REG_H__ */
