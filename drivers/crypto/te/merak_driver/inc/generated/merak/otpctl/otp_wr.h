//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OTPCTL_OTP_WR_REG_H__
#define __OTPCTL_OTP_WR_REG_H__

#define OTPCTL_OTP_WR_TRIG_SHIFT 0 /**< write 1 to this bit to trigger the indirect write */
#define OTPCTL_OTP_WR_TRIG_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * otpctl_otp_wr register definition.
 */
typedef union otpctl_otp_wrReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        trig: 1  /**< write 1 to this bit to trigger the indirect write */,
        hole0: 31; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} otpctl_otp_wrReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_otpctl_otp_wr(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("otpctl_otp_wr: 0x%08x is\n", x);


    t = (x >> OTPCTL_OTP_WR_TRIG_SHIFT) & ((1U << OTPCTL_OTP_WR_TRIG_WIDTH) - 1);
    BITFIELD_LOG(" trig=0x%x (write 1 to this bit to trigger the indirect write)\n", t);
    x &= ~(((1U << OTPCTL_OTP_WR_TRIG_WIDTH) - 1) << OTPCTL_OTP_WR_TRIG_SHIFT);

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

#define log_otpctl_otp_wr(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __OTPCTL_OTP_WR_REG_H__ */
