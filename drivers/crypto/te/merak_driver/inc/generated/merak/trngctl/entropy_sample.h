//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRNGCTL_ENTROPY_SAMPLE_REG_H__
#define __TRNGCTL_ENTROPY_SAMPLE_REG_H__

#define TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DLY_SHIFT 16 /**< sampling process delay */
#define TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DLY_WIDTH 8
#define TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DIV_SHIFT 0 /**< sample divider. 0 is an invalid div */
#define TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DIV_WIDTH 16

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * trngctl_entropy_sample register definition.
 */
typedef union trngctl_entropy_sampleReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        sample_div: 16  /**< sample divider. 0 is an invalid div */,
        sample_dly: 8  /**< sampling process delay */,
        hole0: 8; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} trngctl_entropy_sampleReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_trngctl_entropy_sample(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("trngctl_entropy_sample: 0x%08x is\n", x);


    t = (x >> TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DLY_SHIFT) & ((1U << TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DLY_WIDTH) - 1);
    BITFIELD_LOG(" sample_dly=0x%x (sampling process delay)\n", t);
    x &= ~(((1U << TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DLY_WIDTH) - 1) << TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DLY_SHIFT);


    t = (x >> TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DIV_SHIFT) & ((1U << TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DIV_WIDTH) - 1);
    BITFIELD_LOG(" sample_div=0x%x (sample divider. 0 is an invalid div)\n", t);
    x &= ~(((1U << TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DIV_WIDTH) - 1) << TRNGCTL_ENTROPY_SAMPLE_SAMPLE_DIV_SHIFT);

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

#define log_trngctl_entropy_sample(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNGCTL_ENTROPY_SAMPLE_REG_H__ */
