//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRNGCTL_INTR_REG_H__
#define __TRNGCTL_INTR_REG_H__

#define TRNGCTL_INTR_ADAP_TST_ERR_SHIFT 3
#define TRNGCTL_INTR_ADAP_TST_ERR_WIDTH 1
#define TRNGCTL_INTR_REP_TST_ERR_SHIFT 2
#define TRNGCTL_INTR_REP_TST_ERR_WIDTH 1
#define TRNGCTL_INTR_CRNG_ERR_SHIFT 1
#define TRNGCTL_INTR_CRNG_ERR_WIDTH 1
#define TRNGCTL_INTR_VN_ERR_SHIFT 0 /**< error counter exceeds its threshold */
#define TRNGCTL_INTR_VN_ERR_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * trngctl_intr register definition.
 */
typedef union trngctl_intrReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        vn_err: 1  /**< error counter exceeds its threshold */,
        crng_err: 1,
        rep_tst_err: 1,
        adap_tst_err: 1,
        hole0: 28; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} trngctl_intrReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_trngctl_intr(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("trngctl_intr: 0x%08x is\n", x);


    t = (x >> TRNGCTL_INTR_ADAP_TST_ERR_SHIFT) & ((1U << TRNGCTL_INTR_ADAP_TST_ERR_WIDTH) - 1);
    BITFIELD_LOG(" adap_tst_err=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_INTR_ADAP_TST_ERR_WIDTH) - 1) << TRNGCTL_INTR_ADAP_TST_ERR_SHIFT);


    t = (x >> TRNGCTL_INTR_REP_TST_ERR_SHIFT) & ((1U << TRNGCTL_INTR_REP_TST_ERR_WIDTH) - 1);
    BITFIELD_LOG(" rep_tst_err=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_INTR_REP_TST_ERR_WIDTH) - 1) << TRNGCTL_INTR_REP_TST_ERR_SHIFT);


    t = (x >> TRNGCTL_INTR_CRNG_ERR_SHIFT) & ((1U << TRNGCTL_INTR_CRNG_ERR_WIDTH) - 1);
    BITFIELD_LOG(" crng_err=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_INTR_CRNG_ERR_WIDTH) - 1) << TRNGCTL_INTR_CRNG_ERR_SHIFT);


    t = (x >> TRNGCTL_INTR_VN_ERR_SHIFT) & ((1U << TRNGCTL_INTR_VN_ERR_WIDTH) - 1);
    BITFIELD_LOG(" vn_err=0x%x (error counter exceeds its threshold)\n", t);
    x &= ~(((1U << TRNGCTL_INTR_VN_ERR_WIDTH) - 1) << TRNGCTL_INTR_VN_ERR_SHIFT);

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

#define log_trngctl_intr(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNGCTL_INTR_REG_H__ */
