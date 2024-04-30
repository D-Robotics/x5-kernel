//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_OTP_SIZE0_REG_H__
#define __STAT_OTP_SIZE0_REG_H__

#define STAT_OTP_SIZE0_OTP_NSEC_SIZE_SHIFT 12 /**< 32-bit word size of nsec region */
#define STAT_OTP_SIZE0_OTP_NSEC_SIZE_WIDTH 11
#define STAT_OTP_SIZE0_OTP_SEC_SIZE_SHIFT 0 /**< 32-bit word size of sec region */
#define STAT_OTP_SIZE0_OTP_SEC_SIZE_WIDTH 11

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * stat_otp_size0 register definition.
 */
typedef union stat_otp_size0Reg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        otp_sec_size: 11  /**< 32-bit word size of sec region */,
        hole12: 1,
        otp_nsec_size: 11  /**< 32-bit word size of nsec region */,
        hole0: 9; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} stat_otp_size0Reg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_stat_otp_size0(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("stat_otp_size0: 0x%08x is\n", x);


    t = (x >> STAT_OTP_SIZE0_OTP_NSEC_SIZE_SHIFT) & ((1U << STAT_OTP_SIZE0_OTP_NSEC_SIZE_WIDTH) - 1);
    BITFIELD_LOG(" otp_nsec_size=0x%x (32-bit word size of nsec region)\n", t);
    x &= ~(((1U << STAT_OTP_SIZE0_OTP_NSEC_SIZE_WIDTH) - 1) << STAT_OTP_SIZE0_OTP_NSEC_SIZE_SHIFT);


    t = (x >> STAT_OTP_SIZE0_OTP_SEC_SIZE_SHIFT) & ((1U << STAT_OTP_SIZE0_OTP_SEC_SIZE_WIDTH) - 1);
    BITFIELD_LOG(" otp_sec_size=0x%x (32-bit word size of sec region)\n", t);
    x &= ~(((1U << STAT_OTP_SIZE0_OTP_SEC_SIZE_WIDTH) - 1) << STAT_OTP_SIZE0_OTP_SEC_SIZE_SHIFT);

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

#define log_stat_otp_size0(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_OTP_SIZE0_REG_H__ */
