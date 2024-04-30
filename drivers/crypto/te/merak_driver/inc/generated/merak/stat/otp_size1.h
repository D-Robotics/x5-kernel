//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_OTP_SIZE1_REG_H__
#define __STAT_OTP_SIZE1_REG_H__

#define STAT_OTP_SIZE1_OTP_TST_SIZE_SHIFT 0 /**< 32-bit word size of test region */
#define STAT_OTP_SIZE1_OTP_TST_SIZE_WIDTH 11

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * stat_otp_size1 register definition.
 */
typedef union stat_otp_size1Reg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        otp_tst_size: 11  /**< 32-bit word size of test region */,
        hole0: 21; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} stat_otp_size1Reg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_stat_otp_size1(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("stat_otp_size1: 0x%08x is\n", x);


    t = (x >> STAT_OTP_SIZE1_OTP_TST_SIZE_SHIFT) & ((1U << STAT_OTP_SIZE1_OTP_TST_SIZE_WIDTH) - 1);
    BITFIELD_LOG(" otp_tst_size=0x%x (32-bit word size of test region)\n", t);
    x &= ~(((1U << STAT_OTP_SIZE1_OTP_TST_SIZE_WIDTH) - 1) << STAT_OTP_SIZE1_OTP_TST_SIZE_SHIFT);

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

#define log_stat_otp_size1(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_OTP_SIZE1_REG_H__ */
