//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_TOP_CFG0_REG_H__
#define __STAT_TOP_CFG0_REG_H__

#define STAT_TOP_CFG0_ENC_ON_WRAP_SHIFT 31 /**< enc when wrapout */
#define STAT_TOP_CFG0_ENC_ON_WRAP_WIDTH 1
#define STAT_TOP_CFG0_SCA_SNOW3G_EXIST_SHIFT 30
#define STAT_TOP_CFG0_SCA_SNOW3G_EXIST_WIDTH 1
#define STAT_TOP_CFG0_SCA_ZUC_EXIST_SHIFT 29
#define STAT_TOP_CFG0_SCA_ZUC_EXIST_WIDTH 1
#define STAT_TOP_CFG0_SCA_KASUMI_EXIST_SHIFT 28
#define STAT_TOP_CFG0_SCA_KASUMI_EXIST_WIDTH 1
#define STAT_TOP_CFG0_RNP_HOST_NUM_SHIFT 15
#define STAT_TOP_CFG0_RNP_HOST_NUM_WIDTH 5
#define STAT_TOP_CFG0_ACA_HOST_NUM_SHIFT 10
#define STAT_TOP_CFG0_ACA_HOST_NUM_WIDTH 5
#define STAT_TOP_CFG0_SCA_HOST_NUM_SHIFT 5
#define STAT_TOP_CFG0_SCA_HOST_NUM_WIDTH 5
#define STAT_TOP_CFG0_HASH_HOST_NUM_SHIFT 0
#define STAT_TOP_CFG0_HASH_HOST_NUM_WIDTH 5

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * stat_top_cfg0 register definition.
 */
typedef union stat_top_cfg0Reg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        hash_host_num: 5,
        sca_host_num: 5,
        aca_host_num: 5,
        rnp_host_num: 5,
        hole28: 8,
        sca_kasumi_exist: 1,
        sca_zuc_exist: 1,
        sca_snow3g_exist: 1,
        enc_on_wrap: 1  /**< enc when wrapout */; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} stat_top_cfg0Reg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_stat_top_cfg0(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("stat_top_cfg0: 0x%08x is\n", x);


    t = (x >> STAT_TOP_CFG0_ENC_ON_WRAP_SHIFT) & ((1U << STAT_TOP_CFG0_ENC_ON_WRAP_WIDTH) - 1);
    BITFIELD_LOG(" enc_on_wrap=0x%x (enc when wrapout)\n", t);
    x &= ~(((1U << STAT_TOP_CFG0_ENC_ON_WRAP_WIDTH) - 1) << STAT_TOP_CFG0_ENC_ON_WRAP_SHIFT);


    t = (x >> STAT_TOP_CFG0_SCA_SNOW3G_EXIST_SHIFT) & ((1U << STAT_TOP_CFG0_SCA_SNOW3G_EXIST_WIDTH) - 1);
    BITFIELD_LOG(" sca_snow3g_exist=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_CFG0_SCA_SNOW3G_EXIST_WIDTH) - 1) << STAT_TOP_CFG0_SCA_SNOW3G_EXIST_SHIFT);


    t = (x >> STAT_TOP_CFG0_SCA_ZUC_EXIST_SHIFT) & ((1U << STAT_TOP_CFG0_SCA_ZUC_EXIST_WIDTH) - 1);
    BITFIELD_LOG(" sca_zuc_exist=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_CFG0_SCA_ZUC_EXIST_WIDTH) - 1) << STAT_TOP_CFG0_SCA_ZUC_EXIST_SHIFT);


    t = (x >> STAT_TOP_CFG0_SCA_KASUMI_EXIST_SHIFT) & ((1U << STAT_TOP_CFG0_SCA_KASUMI_EXIST_WIDTH) - 1);
    BITFIELD_LOG(" sca_kasumi_exist=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_CFG0_SCA_KASUMI_EXIST_WIDTH) - 1) << STAT_TOP_CFG0_SCA_KASUMI_EXIST_SHIFT);


    t = (x >> STAT_TOP_CFG0_RNP_HOST_NUM_SHIFT) & ((1U << STAT_TOP_CFG0_RNP_HOST_NUM_WIDTH) - 1);
    BITFIELD_LOG(" rnp_host_num=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_CFG0_RNP_HOST_NUM_WIDTH) - 1) << STAT_TOP_CFG0_RNP_HOST_NUM_SHIFT);


    t = (x >> STAT_TOP_CFG0_ACA_HOST_NUM_SHIFT) & ((1U << STAT_TOP_CFG0_ACA_HOST_NUM_WIDTH) - 1);
    BITFIELD_LOG(" aca_host_num=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_CFG0_ACA_HOST_NUM_WIDTH) - 1) << STAT_TOP_CFG0_ACA_HOST_NUM_SHIFT);


    t = (x >> STAT_TOP_CFG0_SCA_HOST_NUM_SHIFT) & ((1U << STAT_TOP_CFG0_SCA_HOST_NUM_WIDTH) - 1);
    BITFIELD_LOG(" sca_host_num=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_CFG0_SCA_HOST_NUM_WIDTH) - 1) << STAT_TOP_CFG0_SCA_HOST_NUM_SHIFT);


    t = (x >> STAT_TOP_CFG0_HASH_HOST_NUM_SHIFT) & ((1U << STAT_TOP_CFG0_HASH_HOST_NUM_WIDTH) - 1);
    BITFIELD_LOG(" hash_host_num=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_CFG0_HASH_HOST_NUM_WIDTH) - 1) << STAT_TOP_CFG0_HASH_HOST_NUM_SHIFT);

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

#define log_stat_top_cfg0(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_TOP_CFG0_REG_H__ */
