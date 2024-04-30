//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_INTR_HOST_REG_H__
#define __STAT_INTR_HOST_REG_H__

#define STAT_INTR_HOST_CTX_POOL_ERR_SHIFT 5 /**< host0 only */
#define STAT_INTR_HOST_CTX_POOL_ERR_WIDTH 1
#define STAT_INTR_HOST_TRNG_SHIFT 4 /**< host0 only */
#define STAT_INTR_HOST_TRNG_WIDTH 1
#define STAT_INTR_HOST_RNP_SHIFT 3
#define STAT_INTR_HOST_RNP_WIDTH 1
#define STAT_INTR_HOST_ACA_SHIFT 2
#define STAT_INTR_HOST_ACA_WIDTH 1
#define STAT_INTR_HOST_SCA_SHIFT 1
#define STAT_INTR_HOST_SCA_WIDTH 1
#define STAT_INTR_HOST_HASH_SHIFT 0
#define STAT_INTR_HOST_HASH_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * stat_intr_host register definition.
 */
typedef union stat_intr_hostReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        hash: 1,
        sca: 1,
        aca: 1,
        rnp: 1,
        trng: 1  /**< host0 only */,
        ctx_pool_err: 1  /**< host0 only */,
        hole0: 26; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} stat_intr_hostReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_stat_intr_host(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("stat_intr_host: 0x%08x is\n", x);


    t = (x >> STAT_INTR_HOST_CTX_POOL_ERR_SHIFT) & ((1U << STAT_INTR_HOST_CTX_POOL_ERR_WIDTH) - 1);
    BITFIELD_LOG(" ctx_pool_err=0x%x (host0 only)\n", t);
    x &= ~(((1U << STAT_INTR_HOST_CTX_POOL_ERR_WIDTH) - 1) << STAT_INTR_HOST_CTX_POOL_ERR_SHIFT);


    t = (x >> STAT_INTR_HOST_TRNG_SHIFT) & ((1U << STAT_INTR_HOST_TRNG_WIDTH) - 1);
    BITFIELD_LOG(" trng=0x%x (host0 only)\n", t);
    x &= ~(((1U << STAT_INTR_HOST_TRNG_WIDTH) - 1) << STAT_INTR_HOST_TRNG_SHIFT);


    t = (x >> STAT_INTR_HOST_RNP_SHIFT) & ((1U << STAT_INTR_HOST_RNP_WIDTH) - 1);
    BITFIELD_LOG(" rnp=0x%x\n", t);
    x &= ~(((1U << STAT_INTR_HOST_RNP_WIDTH) - 1) << STAT_INTR_HOST_RNP_SHIFT);


    t = (x >> STAT_INTR_HOST_ACA_SHIFT) & ((1U << STAT_INTR_HOST_ACA_WIDTH) - 1);
    BITFIELD_LOG(" aca=0x%x\n", t);
    x &= ~(((1U << STAT_INTR_HOST_ACA_WIDTH) - 1) << STAT_INTR_HOST_ACA_SHIFT);


    t = (x >> STAT_INTR_HOST_SCA_SHIFT) & ((1U << STAT_INTR_HOST_SCA_WIDTH) - 1);
    BITFIELD_LOG(" sca=0x%x\n", t);
    x &= ~(((1U << STAT_INTR_HOST_SCA_WIDTH) - 1) << STAT_INTR_HOST_SCA_SHIFT);


    t = (x >> STAT_INTR_HOST_HASH_SHIFT) & ((1U << STAT_INTR_HOST_HASH_WIDTH) - 1);
    BITFIELD_LOG(" hash=0x%x\n", t);
    x &= ~(((1U << STAT_INTR_HOST_HASH_WIDTH) - 1) << STAT_INTR_HOST_HASH_SHIFT);

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

#define log_stat_intr_host(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_INTR_HOST_REG_H__ */
