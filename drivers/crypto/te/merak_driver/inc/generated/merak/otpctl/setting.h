//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OTPCTL_SETTING_REG_H__
#define __OTPCTL_SETTING_REG_H__

#define OTPCTL_SETTING_OTP_GENERIC_CTRL_SHIFT 9
#define OTPCTL_SETTING_OTP_GENERIC_CTRL_WIDTH 8
#define OTPCTL_SETTING_OTP_DIRECT_RD_SHIFT 8
#define OTPCTL_SETTING_OTP_DIRECT_RD_WIDTH 1
#define OTPCTL_SETTING_LCS_ERR_SHIFT 7 /**< RO */
#define OTPCTL_SETTING_LCS_ERR_WIDTH 1
#define OTPCTL_SETTING_DATA_INV_FAIL_SHIFT 6 /**< RO */
#define OTPCTL_SETTING_DATA_INV_FAIL_WIDTH 1
#define OTPCTL_SETTING_LOCK_LOAD_FAIL_SHIFT 5 /**< RO */
#define OTPCTL_SETTING_LOCK_LOAD_FAIL_WIDTH 1
#define OTPCTL_SETTING_KEY_LOAD_FAIL_SHIFT 4 /**< RO */
#define OTPCTL_SETTING_KEY_LOAD_FAIL_WIDTH 1
#define OTPCTL_SETTING_DID_LOAD_FAIL_SHIFT 3 /**< RO */
#define OTPCTL_SETTING_DID_LOAD_FAIL_WIDTH 1
#define OTPCTL_SETTING_MID_LOAD_FAIL_SHIFT 2 /**< RO */
#define OTPCTL_SETTING_MID_LOAD_FAIL_WIDTH 1
#define OTPCTL_SETTING_LCS_LOAD_FAIL_SHIFT 1 /**< RO */
#define OTPCTL_SETTING_LCS_LOAD_FAIL_WIDTH 1
#define OTPCTL_SETTING_OTP_INIT_DONE_SHIFT 0 /**< RO */
#define OTPCTL_SETTING_OTP_INIT_DONE_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * otpctl_setting register definition.
 */
typedef union otpctl_settingReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        otp_init_done: 1  /**< RO */,
        lcs_load_fail: 1  /**< RO */,
        mid_load_fail: 1  /**< RO */,
        did_load_fail: 1  /**< RO */,
        key_load_fail: 1  /**< RO */,
        lock_load_fail: 1  /**< RO */,
        data_inv_fail: 1  /**< RO */,
        lcs_err: 1  /**< RO */,
        otp_direct_rd: 1,
        otp_generic_ctrl: 8,
        hole0: 15; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} otpctl_settingReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_otpctl_setting(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("otpctl_setting: 0x%08x is\n", x);


    t = (x >> OTPCTL_SETTING_OTP_GENERIC_CTRL_SHIFT) & ((1U << OTPCTL_SETTING_OTP_GENERIC_CTRL_WIDTH) - 1);
    BITFIELD_LOG(" otp_generic_ctrl=0x%x\n", t);
    x &= ~(((1U << OTPCTL_SETTING_OTP_GENERIC_CTRL_WIDTH) - 1) << OTPCTL_SETTING_OTP_GENERIC_CTRL_SHIFT);


    t = (x >> OTPCTL_SETTING_OTP_DIRECT_RD_SHIFT) & ((1U << OTPCTL_SETTING_OTP_DIRECT_RD_WIDTH) - 1);
    BITFIELD_LOG(" otp_direct_rd=0x%x\n", t);
    x &= ~(((1U << OTPCTL_SETTING_OTP_DIRECT_RD_WIDTH) - 1) << OTPCTL_SETTING_OTP_DIRECT_RD_SHIFT);


    t = (x >> OTPCTL_SETTING_LCS_ERR_SHIFT) & ((1U << OTPCTL_SETTING_LCS_ERR_WIDTH) - 1);
    BITFIELD_LOG(" lcs_err=0x%x (RO)\n", t);
    x &= ~(((1U << OTPCTL_SETTING_LCS_ERR_WIDTH) - 1) << OTPCTL_SETTING_LCS_ERR_SHIFT);


    t = (x >> OTPCTL_SETTING_DATA_INV_FAIL_SHIFT) & ((1U << OTPCTL_SETTING_DATA_INV_FAIL_WIDTH) - 1);
    BITFIELD_LOG(" data_inv_fail=0x%x (RO)\n", t);
    x &= ~(((1U << OTPCTL_SETTING_DATA_INV_FAIL_WIDTH) - 1) << OTPCTL_SETTING_DATA_INV_FAIL_SHIFT);


    t = (x >> OTPCTL_SETTING_LOCK_LOAD_FAIL_SHIFT) & ((1U << OTPCTL_SETTING_LOCK_LOAD_FAIL_WIDTH) - 1);
    BITFIELD_LOG(" lock_load_fail=0x%x (RO)\n", t);
    x &= ~(((1U << OTPCTL_SETTING_LOCK_LOAD_FAIL_WIDTH) - 1) << OTPCTL_SETTING_LOCK_LOAD_FAIL_SHIFT);


    t = (x >> OTPCTL_SETTING_KEY_LOAD_FAIL_SHIFT) & ((1U << OTPCTL_SETTING_KEY_LOAD_FAIL_WIDTH) - 1);
    BITFIELD_LOG(" key_load_fail=0x%x (RO)\n", t);
    x &= ~(((1U << OTPCTL_SETTING_KEY_LOAD_FAIL_WIDTH) - 1) << OTPCTL_SETTING_KEY_LOAD_FAIL_SHIFT);


    t = (x >> OTPCTL_SETTING_DID_LOAD_FAIL_SHIFT) & ((1U << OTPCTL_SETTING_DID_LOAD_FAIL_WIDTH) - 1);
    BITFIELD_LOG(" did_load_fail=0x%x (RO)\n", t);
    x &= ~(((1U << OTPCTL_SETTING_DID_LOAD_FAIL_WIDTH) - 1) << OTPCTL_SETTING_DID_LOAD_FAIL_SHIFT);


    t = (x >> OTPCTL_SETTING_MID_LOAD_FAIL_SHIFT) & ((1U << OTPCTL_SETTING_MID_LOAD_FAIL_WIDTH) - 1);
    BITFIELD_LOG(" mid_load_fail=0x%x (RO)\n", t);
    x &= ~(((1U << OTPCTL_SETTING_MID_LOAD_FAIL_WIDTH) - 1) << OTPCTL_SETTING_MID_LOAD_FAIL_SHIFT);


    t = (x >> OTPCTL_SETTING_LCS_LOAD_FAIL_SHIFT) & ((1U << OTPCTL_SETTING_LCS_LOAD_FAIL_WIDTH) - 1);
    BITFIELD_LOG(" lcs_load_fail=0x%x (RO)\n", t);
    x &= ~(((1U << OTPCTL_SETTING_LCS_LOAD_FAIL_WIDTH) - 1) << OTPCTL_SETTING_LCS_LOAD_FAIL_SHIFT);


    t = (x >> OTPCTL_SETTING_OTP_INIT_DONE_SHIFT) & ((1U << OTPCTL_SETTING_OTP_INIT_DONE_WIDTH) - 1);
    BITFIELD_LOG(" otp_init_done=0x%x (RO)\n", t);
    x &= ~(((1U << OTPCTL_SETTING_OTP_INIT_DONE_WIDTH) - 1) << OTPCTL_SETTING_OTP_INIT_DONE_SHIFT);

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

#define log_otpctl_setting(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __OTPCTL_SETTING_REG_H__ */
