//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRNGCTL_RO_SETTING_REG_H__
#define __TRNGCTL_RO_SETTING_REG_H__

#define TRNGCTL_RO_SETTING_GRP3_TAPS_EN_SHIFT 12
#define TRNGCTL_RO_SETTING_GRP3_TAPS_EN_WIDTH 3
#define TRNGCTL_RO_SETTING_GRP2_TAPS_EN_SHIFT 8
#define TRNGCTL_RO_SETTING_GRP2_TAPS_EN_WIDTH 3
#define TRNGCTL_RO_SETTING_GRP1_TAPS_EN_SHIFT 4
#define TRNGCTL_RO_SETTING_GRP1_TAPS_EN_WIDTH 3
#define TRNGCTL_RO_SETTING_GRP0_TAPS_EN_SHIFT 0
#define TRNGCTL_RO_SETTING_GRP0_TAPS_EN_WIDTH 3

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * trngctl_ro_setting register definition.
 */
typedef union trngctl_ro_settingReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        grp0_taps_en: 3,
        hole4: 1,
        grp1_taps_en: 3,
        hole8: 1,
        grp2_taps_en: 3,
        hole12: 1,
        grp3_taps_en: 3,
        hole0: 17; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} trngctl_ro_settingReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_trngctl_ro_setting(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("trngctl_ro_setting: 0x%08x is\n", x);


    t = (x >> TRNGCTL_RO_SETTING_GRP3_TAPS_EN_SHIFT) & ((1U << TRNGCTL_RO_SETTING_GRP3_TAPS_EN_WIDTH) - 1);
    BITFIELD_LOG(" grp3_taps_en=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_RO_SETTING_GRP3_TAPS_EN_WIDTH) - 1) << TRNGCTL_RO_SETTING_GRP3_TAPS_EN_SHIFT);


    t = (x >> TRNGCTL_RO_SETTING_GRP2_TAPS_EN_SHIFT) & ((1U << TRNGCTL_RO_SETTING_GRP2_TAPS_EN_WIDTH) - 1);
    BITFIELD_LOG(" grp2_taps_en=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_RO_SETTING_GRP2_TAPS_EN_WIDTH) - 1) << TRNGCTL_RO_SETTING_GRP2_TAPS_EN_SHIFT);


    t = (x >> TRNGCTL_RO_SETTING_GRP1_TAPS_EN_SHIFT) & ((1U << TRNGCTL_RO_SETTING_GRP1_TAPS_EN_WIDTH) - 1);
    BITFIELD_LOG(" grp1_taps_en=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_RO_SETTING_GRP1_TAPS_EN_WIDTH) - 1) << TRNGCTL_RO_SETTING_GRP1_TAPS_EN_SHIFT);


    t = (x >> TRNGCTL_RO_SETTING_GRP0_TAPS_EN_SHIFT) & ((1U << TRNGCTL_RO_SETTING_GRP0_TAPS_EN_WIDTH) - 1);
    BITFIELD_LOG(" grp0_taps_en=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_RO_SETTING_GRP0_TAPS_EN_WIDTH) - 1) << TRNGCTL_RO_SETTING_GRP0_TAPS_EN_SHIFT);

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

#define log_trngctl_ro_setting(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNGCTL_RO_SETTING_REG_H__ */
