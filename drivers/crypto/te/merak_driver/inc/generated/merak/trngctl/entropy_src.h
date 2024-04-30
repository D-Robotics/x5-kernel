//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRNGCTL_ENTROPY_SRC_REG_H__
#define __TRNGCTL_ENTROPY_SRC_REG_H__

#define TRNGCTL_ENTROPY_SRC_SRC_SEL_SHIFT 4 /**< 0/1: internal/external source */
#define TRNGCTL_ENTROPY_SRC_SRC_SEL_WIDTH 1
#define TRNGCTL_ENTROPY_SRC_GRP3_EN_SHIFT 3
#define TRNGCTL_ENTROPY_SRC_GRP3_EN_WIDTH 1
#define TRNGCTL_ENTROPY_SRC_GRP2_EN_SHIFT 2
#define TRNGCTL_ENTROPY_SRC_GRP2_EN_WIDTH 1
#define TRNGCTL_ENTROPY_SRC_GRP1_EN_SHIFT 1
#define TRNGCTL_ENTROPY_SRC_GRP1_EN_WIDTH 1
#define TRNGCTL_ENTROPY_SRC_GRP0_EN_SHIFT 0 /**< group0 chain enable */
#define TRNGCTL_ENTROPY_SRC_GRP0_EN_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * trngctl_entropy_src register definition.
 */
typedef union trngctl_entropy_srcReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        grp0_en: 1  /**< group0 chain enable */,
        grp1_en: 1,
        grp2_en: 1,
        grp3_en: 1,
        src_sel: 1  /**< 0/1: internal/external source */,
        hole0: 27; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} trngctl_entropy_srcReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_trngctl_entropy_src(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("trngctl_entropy_src: 0x%08x is\n", x);


    t = (x >> TRNGCTL_ENTROPY_SRC_SRC_SEL_SHIFT) & ((1U << TRNGCTL_ENTROPY_SRC_SRC_SEL_WIDTH) - 1);
    BITFIELD_LOG(" src_sel=0x%x (0/1: internal/external source)\n", t);
    x &= ~(((1U << TRNGCTL_ENTROPY_SRC_SRC_SEL_WIDTH) - 1) << TRNGCTL_ENTROPY_SRC_SRC_SEL_SHIFT);


    t = (x >> TRNGCTL_ENTROPY_SRC_GRP3_EN_SHIFT) & ((1U << TRNGCTL_ENTROPY_SRC_GRP3_EN_WIDTH) - 1);
    BITFIELD_LOG(" grp3_en=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_ENTROPY_SRC_GRP3_EN_WIDTH) - 1) << TRNGCTL_ENTROPY_SRC_GRP3_EN_SHIFT);


    t = (x >> TRNGCTL_ENTROPY_SRC_GRP2_EN_SHIFT) & ((1U << TRNGCTL_ENTROPY_SRC_GRP2_EN_WIDTH) - 1);
    BITFIELD_LOG(" grp2_en=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_ENTROPY_SRC_GRP2_EN_WIDTH) - 1) << TRNGCTL_ENTROPY_SRC_GRP2_EN_SHIFT);


    t = (x >> TRNGCTL_ENTROPY_SRC_GRP1_EN_SHIFT) & ((1U << TRNGCTL_ENTROPY_SRC_GRP1_EN_WIDTH) - 1);
    BITFIELD_LOG(" grp1_en=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_ENTROPY_SRC_GRP1_EN_WIDTH) - 1) << TRNGCTL_ENTROPY_SRC_GRP1_EN_SHIFT);


    t = (x >> TRNGCTL_ENTROPY_SRC_GRP0_EN_SHIFT) & ((1U << TRNGCTL_ENTROPY_SRC_GRP0_EN_WIDTH) - 1);
    BITFIELD_LOG(" grp0_en=0x%x (group0 chain enable)\n", t);
    x &= ~(((1U << TRNGCTL_ENTROPY_SRC_GRP0_EN_WIDTH) - 1) << TRNGCTL_ENTROPY_SRC_GRP0_EN_SHIFT);

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

#define log_trngctl_entropy_src(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNGCTL_ENTROPY_SRC_REG_H__ */
