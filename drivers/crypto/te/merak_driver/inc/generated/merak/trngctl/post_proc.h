//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRNGCTL_POST_PROC_REG_H__
#define __TRNGCTL_POST_PROC_REG_H__

#define TRNGCTL_POST_PROC_FAST_MODE_SHIFT 14
#define TRNGCTL_POST_PROC_FAST_MODE_WIDTH 1
#define TRNGCTL_POST_PROC_LFSR_SEL_SHIFT 13 /**< select xor */
#define TRNGCTL_POST_PROC_LFSR_SEL_WIDTH 1
#define TRNGCTL_POST_PROC_LFSR_DROP_NUM_SHIFT 11
#define TRNGCTL_POST_PROC_LFSR_DROP_NUM_WIDTH 2
#define TRNGCTL_POST_PROC_CRNG_BYPASS_SHIFT 2
#define TRNGCTL_POST_PROC_CRNG_BYPASS_WIDTH 1
#define TRNGCTL_POST_PROC_VN_BYPASS_SHIFT 1
#define TRNGCTL_POST_PROC_VN_BYPASS_WIDTH 1
#define TRNGCTL_POST_PROC_PRNG_BYPASS_SHIFT 0
#define TRNGCTL_POST_PROC_PRNG_BYPASS_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * trngctl_post_proc register definition.
 */
typedef union trngctl_post_procReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        prng_bypass: 1,
        vn_bypass: 1,
        crng_bypass: 1,
        hole11: 8,
        lfsr_drop_num: 2,
        lfsr_sel: 1  /**< select xor */,
        fast_mode: 1,
        hole0: 17; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} trngctl_post_procReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_trngctl_post_proc(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("trngctl_post_proc: 0x%08x is\n", x);


    t = (x >> TRNGCTL_POST_PROC_FAST_MODE_SHIFT) & ((1U << TRNGCTL_POST_PROC_FAST_MODE_WIDTH) - 1);
    BITFIELD_LOG(" fast_mode=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_POST_PROC_FAST_MODE_WIDTH) - 1) << TRNGCTL_POST_PROC_FAST_MODE_SHIFT);


    t = (x >> TRNGCTL_POST_PROC_LFSR_SEL_SHIFT) & ((1U << TRNGCTL_POST_PROC_LFSR_SEL_WIDTH) - 1);
    BITFIELD_LOG(" lfsr_sel=0x%x (select xor)\n", t);
    x &= ~(((1U << TRNGCTL_POST_PROC_LFSR_SEL_WIDTH) - 1) << TRNGCTL_POST_PROC_LFSR_SEL_SHIFT);


    t = (x >> TRNGCTL_POST_PROC_LFSR_DROP_NUM_SHIFT) & ((1U << TRNGCTL_POST_PROC_LFSR_DROP_NUM_WIDTH) - 1);
    BITFIELD_LOG(" lfsr_drop_num=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_POST_PROC_LFSR_DROP_NUM_WIDTH) - 1) << TRNGCTL_POST_PROC_LFSR_DROP_NUM_SHIFT);


    t = (x >> TRNGCTL_POST_PROC_CRNG_BYPASS_SHIFT) & ((1U << TRNGCTL_POST_PROC_CRNG_BYPASS_WIDTH) - 1);
    BITFIELD_LOG(" crng_bypass=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_POST_PROC_CRNG_BYPASS_WIDTH) - 1) << TRNGCTL_POST_PROC_CRNG_BYPASS_SHIFT);


    t = (x >> TRNGCTL_POST_PROC_VN_BYPASS_SHIFT) & ((1U << TRNGCTL_POST_PROC_VN_BYPASS_WIDTH) - 1);
    BITFIELD_LOG(" vn_bypass=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_POST_PROC_VN_BYPASS_WIDTH) - 1) << TRNGCTL_POST_PROC_VN_BYPASS_SHIFT);


    t = (x >> TRNGCTL_POST_PROC_PRNG_BYPASS_SHIFT) & ((1U << TRNGCTL_POST_PROC_PRNG_BYPASS_WIDTH) - 1);
    BITFIELD_LOG(" prng_bypass=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_POST_PROC_PRNG_BYPASS_WIDTH) - 1) << TRNGCTL_POST_PROC_PRNG_BYPASS_SHIFT);

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

#define log_trngctl_post_proc(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNGCTL_POST_PROC_REG_H__ */
