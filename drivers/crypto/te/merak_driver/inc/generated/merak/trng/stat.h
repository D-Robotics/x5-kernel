//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRNG_STAT_REG_H__
#define __TRNG_STAT_REG_H__

#define TRNG_STAT_TRNG_STAT_SHIFT 1 /**< core state 0/1/2/3=idle/oth_busy/this_busy/reset */
#define TRNG_STAT_TRNG_STAT_WIDTH 2
#define TRNG_STAT_AC_TST_ERR_SHIFT 0
#define TRNG_STAT_AC_TST_ERR_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * trng_stat register definition.
 */
typedef union trng_statReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        ac_tst_err: 1,
        trng_stat: 2  /**< core state 0/1/2/3=idle/oth_busy/this_busy/reset */,
        hole0: 29; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} trng_statReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_trng_stat(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("trng_stat: 0x%08x is\n", x);


    t = (x >> TRNG_STAT_TRNG_STAT_SHIFT) & ((1U << TRNG_STAT_TRNG_STAT_WIDTH) - 1);
    BITFIELD_LOG(" trng_stat=0x%x (core state 0/1/2/3=idle/oth_busy/this_busy/reset)\n", t);
    x &= ~(((1U << TRNG_STAT_TRNG_STAT_WIDTH) - 1) << TRNG_STAT_TRNG_STAT_SHIFT);


    t = (x >> TRNG_STAT_AC_TST_ERR_SHIFT) & ((1U << TRNG_STAT_AC_TST_ERR_WIDTH) - 1);
    BITFIELD_LOG(" ac_tst_err=0x%x\n", t);
    x &= ~(((1U << TRNG_STAT_AC_TST_ERR_WIDTH) - 1) << TRNG_STAT_AC_TST_ERR_SHIFT);

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

#define log_trng_stat(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNG_STAT_REG_H__ */
