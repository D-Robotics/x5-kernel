//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __CTL_CLOCK_CTRL_REG_H__
#define __CTL_CLOCK_CTRL_REG_H__

#define CTL_CLOCK_CTRL_OTP_CLK_EN_SHIFT 4
#define CTL_CLOCK_CTRL_OTP_CLK_EN_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * ctl_clock_ctrl register definition.
 */
typedef union ctl_clock_ctrlReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        hole4: 4,
        otp_clk_en: 1,
        hole0: 27; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} ctl_clock_ctrlReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_ctl_clock_ctrl(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("ctl_clock_ctrl: 0x%08x is\n", x);


    t = (x >> CTL_CLOCK_CTRL_OTP_CLK_EN_SHIFT) & ((1U << CTL_CLOCK_CTRL_OTP_CLK_EN_WIDTH) - 1);
    BITFIELD_LOG(" otp_clk_en=0x%x\n", t);
    x &= ~(((1U << CTL_CLOCK_CTRL_OTP_CLK_EN_WIDTH) - 1) << CTL_CLOCK_CTRL_OTP_CLK_EN_SHIFT);

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

#define log_ctl_clock_ctrl(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __CTL_CLOCK_CTRL_REG_H__ */
