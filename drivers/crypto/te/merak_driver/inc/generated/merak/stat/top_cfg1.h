//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_TOP_CFG1_REG_H__
#define __STAT_TOP_CFG1_REG_H__

#define STAT_TOP_CFG1_OTP_SKEY_256_SHIFT 4 /**< OTP key length of model key, and device root key */
#define STAT_TOP_CFG1_OTP_SKEY_256_WIDTH 1
#define STAT_TOP_CFG1_OTP_INIT_VAL_SHIFT 3 /**< OTP device intial value */
#define STAT_TOP_CFG1_OTP_INIT_VAL_WIDTH 1
#define STAT_TOP_CFG1_OTP_SHD_TO_AO_SHIFT 2 /**< OTP shadow registers save to AO */
#define STAT_TOP_CFG1_OTP_SHD_TO_AO_WIDTH 1
#define STAT_TOP_CFG1_OTP_EXIST_SHIFT 1
#define STAT_TOP_CFG1_OTP_EXIST_WIDTH 1
#define STAT_TOP_CFG1_TRNG_ISRC_SHIFT 0 /**< trng internal src existence */
#define STAT_TOP_CFG1_TRNG_ISRC_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * stat_top_cfg1 register definition.
 */
typedef union stat_top_cfg1Reg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        trng_isrc: 1  /**< trng internal src existence */,
        otp_exist: 1,
        otp_shd_to_ao: 1  /**< OTP shadow registers save to AO */,
        otp_init_val: 1  /**< OTP device intial value */,
        otp_skey_256: 1  /**< OTP key length of model key, and device root key */,
        hole0: 27; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} stat_top_cfg1Reg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_stat_top_cfg1(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("stat_top_cfg1: 0x%08x is\n", x);


    t = (x >> STAT_TOP_CFG1_OTP_SKEY_256_SHIFT) & ((1U << STAT_TOP_CFG1_OTP_SKEY_256_WIDTH) - 1);
    BITFIELD_LOG(" otp_skey_256=0x%x (OTP key length of model key, and device root key)\n", t);
    x &= ~(((1U << STAT_TOP_CFG1_OTP_SKEY_256_WIDTH) - 1) << STAT_TOP_CFG1_OTP_SKEY_256_SHIFT);


    t = (x >> STAT_TOP_CFG1_OTP_INIT_VAL_SHIFT) & ((1U << STAT_TOP_CFG1_OTP_INIT_VAL_WIDTH) - 1);
    BITFIELD_LOG(" otp_init_val=0x%x (OTP device intial value)\n", t);
    x &= ~(((1U << STAT_TOP_CFG1_OTP_INIT_VAL_WIDTH) - 1) << STAT_TOP_CFG1_OTP_INIT_VAL_SHIFT);


    t = (x >> STAT_TOP_CFG1_OTP_SHD_TO_AO_SHIFT) & ((1U << STAT_TOP_CFG1_OTP_SHD_TO_AO_WIDTH) - 1);
    BITFIELD_LOG(" otp_shd_to_ao=0x%x (OTP shadow registers save to AO)\n", t);
    x &= ~(((1U << STAT_TOP_CFG1_OTP_SHD_TO_AO_WIDTH) - 1) << STAT_TOP_CFG1_OTP_SHD_TO_AO_SHIFT);


    t = (x >> STAT_TOP_CFG1_OTP_EXIST_SHIFT) & ((1U << STAT_TOP_CFG1_OTP_EXIST_WIDTH) - 1);
    BITFIELD_LOG(" otp_exist=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_CFG1_OTP_EXIST_WIDTH) - 1) << STAT_TOP_CFG1_OTP_EXIST_SHIFT);


    t = (x >> STAT_TOP_CFG1_TRNG_ISRC_SHIFT) & ((1U << STAT_TOP_CFG1_TRNG_ISRC_WIDTH) - 1);
    BITFIELD_LOG(" trng_isrc=0x%x (trng internal src existence)\n", t);
    x &= ~(((1U << STAT_TOP_CFG1_TRNG_ISRC_WIDTH) - 1) << STAT_TOP_CFG1_TRNG_ISRC_SHIFT);

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

#define log_stat_top_cfg1(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_TOP_CFG1_REG_H__ */
