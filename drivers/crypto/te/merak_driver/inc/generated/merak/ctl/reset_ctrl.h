//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __CTL_RESET_CTRL_REG_H__
#define __CTL_RESET_CTRL_REG_H__

#define CTL_RESET_CTRL_DMA_SRESET_SHIFT 5
#define CTL_RESET_CTRL_DMA_SRESET_WIDTH 1
#define CTL_RESET_CTRL_TRNG_SRESET_SHIFT 4
#define CTL_RESET_CTRL_TRNG_SRESET_WIDTH 1
#define CTL_RESET_CTRL_OTP_SRESET_SHIFT 3
#define CTL_RESET_CTRL_OTP_SRESET_WIDTH 1
#define CTL_RESET_CTRL_ACA_SRESET_SHIFT 2
#define CTL_RESET_CTRL_ACA_SRESET_WIDTH 1
#define CTL_RESET_CTRL_SCA_SRESET_SHIFT 1
#define CTL_RESET_CTRL_SCA_SRESET_WIDTH 1
#define CTL_RESET_CTRL_HASH_SRESET_SHIFT 0
#define CTL_RESET_CTRL_HASH_SRESET_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * ctl_reset_ctrl register definition.
 */
typedef union ctl_reset_ctrlReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        hash_sreset: 1,
        sca_sreset: 1,
        aca_sreset: 1,
        otp_sreset: 1,
        trng_sreset: 1,
        dma_sreset: 1,
        hole0: 26; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} ctl_reset_ctrlReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_ctl_reset_ctrl(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("ctl_reset_ctrl: 0x%08x is\n", x);


    t = (x >> CTL_RESET_CTRL_DMA_SRESET_SHIFT) & ((1U << CTL_RESET_CTRL_DMA_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" dma_sreset=0x%x\n", t);
    x &= ~(((1U << CTL_RESET_CTRL_DMA_SRESET_WIDTH) - 1) << CTL_RESET_CTRL_DMA_SRESET_SHIFT);


    t = (x >> CTL_RESET_CTRL_TRNG_SRESET_SHIFT) & ((1U << CTL_RESET_CTRL_TRNG_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" trng_sreset=0x%x\n", t);
    x &= ~(((1U << CTL_RESET_CTRL_TRNG_SRESET_WIDTH) - 1) << CTL_RESET_CTRL_TRNG_SRESET_SHIFT);


    t = (x >> CTL_RESET_CTRL_OTP_SRESET_SHIFT) & ((1U << CTL_RESET_CTRL_OTP_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" otp_sreset=0x%x\n", t);
    x &= ~(((1U << CTL_RESET_CTRL_OTP_SRESET_WIDTH) - 1) << CTL_RESET_CTRL_OTP_SRESET_SHIFT);


    t = (x >> CTL_RESET_CTRL_ACA_SRESET_SHIFT) & ((1U << CTL_RESET_CTRL_ACA_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" aca_sreset=0x%x\n", t);
    x &= ~(((1U << CTL_RESET_CTRL_ACA_SRESET_WIDTH) - 1) << CTL_RESET_CTRL_ACA_SRESET_SHIFT);


    t = (x >> CTL_RESET_CTRL_SCA_SRESET_SHIFT) & ((1U << CTL_RESET_CTRL_SCA_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" sca_sreset=0x%x\n", t);
    x &= ~(((1U << CTL_RESET_CTRL_SCA_SRESET_WIDTH) - 1) << CTL_RESET_CTRL_SCA_SRESET_SHIFT);


    t = (x >> CTL_RESET_CTRL_HASH_SRESET_SHIFT) & ((1U << CTL_RESET_CTRL_HASH_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" hash_sreset=0x%x\n", t);
    x &= ~(((1U << CTL_RESET_CTRL_HASH_SRESET_WIDTH) - 1) << CTL_RESET_CTRL_HASH_SRESET_SHIFT);

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

#define log_ctl_reset_ctrl(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __CTL_RESET_CTRL_REG_H__ */
