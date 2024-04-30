//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OTPCTL_UPDATE_STAT_REG_H__
#define __OTPCTL_UPDATE_STAT_REG_H__

#define OTPCTL_UPDATE_STAT_AO_SHD_VALID_SHIFT 3 /**< 0~5, mid:modk:did:rootk:lcs:lock */
#define OTPCTL_UPDATE_STAT_AO_SHD_VALID_WIDTH 6
#define OTPCTL_UPDATE_STAT_OTP_READY_SHIFT 2
#define OTPCTL_UPDATE_STAT_OTP_READY_WIDTH 1
#define OTPCTL_UPDATE_STAT_UPDATE_FAIL_SHIFT 1
#define OTPCTL_UPDATE_STAT_UPDATE_FAIL_WIDTH 1
#define OTPCTL_UPDATE_STAT_UPDATE_BUSY_SHIFT 0
#define OTPCTL_UPDATE_STAT_UPDATE_BUSY_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * otpctl_update_stat register definition.
 */
typedef union otpctl_update_statReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        update_busy: 1,
        update_fail: 1,
        otp_ready: 1,
        ao_shd_valid: 6  /**< 0~5, mid:modk:did:rootk:lcs:lock */,
        hole0: 23; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} otpctl_update_statReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_otpctl_update_stat(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("otpctl_update_stat: 0x%08x is\n", x);


    t = (x >> OTPCTL_UPDATE_STAT_AO_SHD_VALID_SHIFT) & ((1U << OTPCTL_UPDATE_STAT_AO_SHD_VALID_WIDTH) - 1);
    BITFIELD_LOG(" ao_shd_valid=0x%x (0~5, mid:modk:did:rootk:lcs:lock)\n", t);
    x &= ~(((1U << OTPCTL_UPDATE_STAT_AO_SHD_VALID_WIDTH) - 1) << OTPCTL_UPDATE_STAT_AO_SHD_VALID_SHIFT);


    t = (x >> OTPCTL_UPDATE_STAT_OTP_READY_SHIFT) & ((1U << OTPCTL_UPDATE_STAT_OTP_READY_WIDTH) - 1);
    BITFIELD_LOG(" otp_ready=0x%x\n", t);
    x &= ~(((1U << OTPCTL_UPDATE_STAT_OTP_READY_WIDTH) - 1) << OTPCTL_UPDATE_STAT_OTP_READY_SHIFT);


    t = (x >> OTPCTL_UPDATE_STAT_UPDATE_FAIL_SHIFT) & ((1U << OTPCTL_UPDATE_STAT_UPDATE_FAIL_WIDTH) - 1);
    BITFIELD_LOG(" update_fail=0x%x\n", t);
    x &= ~(((1U << OTPCTL_UPDATE_STAT_UPDATE_FAIL_WIDTH) - 1) << OTPCTL_UPDATE_STAT_UPDATE_FAIL_SHIFT);


    t = (x >> OTPCTL_UPDATE_STAT_UPDATE_BUSY_SHIFT) & ((1U << OTPCTL_UPDATE_STAT_UPDATE_BUSY_WIDTH) - 1);
    BITFIELD_LOG(" update_busy=0x%x\n", t);
    x &= ~(((1U << OTPCTL_UPDATE_STAT_UPDATE_BUSY_WIDTH) - 1) << OTPCTL_UPDATE_STAT_UPDATE_BUSY_SHIFT);

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

#define log_otpctl_update_stat(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __OTPCTL_UPDATE_STAT_REG_H__ */
