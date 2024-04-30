//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OTPCTL_DUMMY_CFG_REG_H__
#define __OTPCTL_DUMMY_CFG_REG_H__

#define OTPCTL_DUMMY_CFG_KEY_VALID_SHIFT 5
#define OTPCTL_DUMMY_CFG_KEY_VALID_WIDTH 1
#define OTPCTL_DUMMY_CFG_LCS_VALID_SHIFT 4
#define OTPCTL_DUMMY_CFG_LCS_VALID_WIDTH 1
#define OTPCTL_DUMMY_CFG_LCS_CM_SHIFT 3
#define OTPCTL_DUMMY_CFG_LCS_CM_WIDTH 1
#define OTPCTL_DUMMY_CFG_LCS_DM_SHIFT 2
#define OTPCTL_DUMMY_CFG_LCS_DM_WIDTH 1
#define OTPCTL_DUMMY_CFG_LCS_DD_SHIFT 1
#define OTPCTL_DUMMY_CFG_LCS_DD_WIDTH 1
#define OTPCTL_DUMMY_CFG_LCS_DR_SHIFT 0
#define OTPCTL_DUMMY_CFG_LCS_DR_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * otpctl_dummy_cfg register definition.
 */
typedef union otpctl_dummy_cfgReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        lcs_dr: 1,
        lcs_dd: 1,
        lcs_dm: 1,
        lcs_cm: 1,
        lcs_valid: 1,
        key_valid: 1,
        hole0: 26; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} otpctl_dummy_cfgReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_otpctl_dummy_cfg(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("otpctl_dummy_cfg: 0x%08x is\n", x);


    t = (x >> OTPCTL_DUMMY_CFG_KEY_VALID_SHIFT) & ((1U << OTPCTL_DUMMY_CFG_KEY_VALID_WIDTH) - 1);
    BITFIELD_LOG(" key_valid=0x%x\n", t);
    x &= ~(((1U << OTPCTL_DUMMY_CFG_KEY_VALID_WIDTH) - 1) << OTPCTL_DUMMY_CFG_KEY_VALID_SHIFT);


    t = (x >> OTPCTL_DUMMY_CFG_LCS_VALID_SHIFT) & ((1U << OTPCTL_DUMMY_CFG_LCS_VALID_WIDTH) - 1);
    BITFIELD_LOG(" lcs_valid=0x%x\n", t);
    x &= ~(((1U << OTPCTL_DUMMY_CFG_LCS_VALID_WIDTH) - 1) << OTPCTL_DUMMY_CFG_LCS_VALID_SHIFT);


    t = (x >> OTPCTL_DUMMY_CFG_LCS_CM_SHIFT) & ((1U << OTPCTL_DUMMY_CFG_LCS_CM_WIDTH) - 1);
    BITFIELD_LOG(" lcs_cm=0x%x\n", t);
    x &= ~(((1U << OTPCTL_DUMMY_CFG_LCS_CM_WIDTH) - 1) << OTPCTL_DUMMY_CFG_LCS_CM_SHIFT);


    t = (x >> OTPCTL_DUMMY_CFG_LCS_DM_SHIFT) & ((1U << OTPCTL_DUMMY_CFG_LCS_DM_WIDTH) - 1);
    BITFIELD_LOG(" lcs_dm=0x%x\n", t);
    x &= ~(((1U << OTPCTL_DUMMY_CFG_LCS_DM_WIDTH) - 1) << OTPCTL_DUMMY_CFG_LCS_DM_SHIFT);


    t = (x >> OTPCTL_DUMMY_CFG_LCS_DD_SHIFT) & ((1U << OTPCTL_DUMMY_CFG_LCS_DD_WIDTH) - 1);
    BITFIELD_LOG(" lcs_dd=0x%x\n", t);
    x &= ~(((1U << OTPCTL_DUMMY_CFG_LCS_DD_WIDTH) - 1) << OTPCTL_DUMMY_CFG_LCS_DD_SHIFT);


    t = (x >> OTPCTL_DUMMY_CFG_LCS_DR_SHIFT) & ((1U << OTPCTL_DUMMY_CFG_LCS_DR_WIDTH) - 1);
    BITFIELD_LOG(" lcs_dr=0x%x\n", t);
    x &= ~(((1U << OTPCTL_DUMMY_CFG_LCS_DR_WIDTH) - 1) << OTPCTL_DUMMY_CFG_LCS_DR_SHIFT);

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

#define log_otpctl_dummy_cfg(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __OTPCTL_DUMMY_CFG_REG_H__ */
