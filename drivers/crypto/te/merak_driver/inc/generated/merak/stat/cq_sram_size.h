//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_CQ_SRAM_SIZE_REG_H__
#define __STAT_CQ_SRAM_SIZE_REG_H__

#define STAT_CQ_SRAM_SIZE_ACA_SRAM_SIZE_SHIFT 27 /**< KB */
#define STAT_CQ_SRAM_SIZE_ACA_SRAM_SIZE_WIDTH 5
#define STAT_CQ_SRAM_SIZE_CTX_MAX_SIZE_SHIFT 21 /**< wrap-out ctx max size in 32-bit word */
#define STAT_CQ_SRAM_SIZE_CTX_MAX_SIZE_WIDTH 6
#define STAT_CQ_SRAM_SIZE_ACA_CQ_DEPTH_SHIFT 10
#define STAT_CQ_SRAM_SIZE_ACA_CQ_DEPTH_WIDTH 5
#define STAT_CQ_SRAM_SIZE_SCA_CQ_DEPTH_SHIFT 5
#define STAT_CQ_SRAM_SIZE_SCA_CQ_DEPTH_WIDTH 5
#define STAT_CQ_SRAM_SIZE_HASH_CQ_DEPTH_SHIFT 0
#define STAT_CQ_SRAM_SIZE_HASH_CQ_DEPTH_WIDTH 5

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * stat_cq_sram_size register definition.
 */
typedef union stat_cq_sram_sizeReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        hash_cq_depth: 5,
        sca_cq_depth: 5,
        aca_cq_depth: 5,
        hole21: 6,
        ctx_max_size: 6  /**< wrap-out ctx max size in 32-bit word */,
        aca_sram_size: 5  /**< KB */; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} stat_cq_sram_sizeReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_stat_cq_sram_size(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("stat_cq_sram_size: 0x%08x is\n", x);


    t = (x >> STAT_CQ_SRAM_SIZE_ACA_SRAM_SIZE_SHIFT) & ((1U << STAT_CQ_SRAM_SIZE_ACA_SRAM_SIZE_WIDTH) - 1);
    BITFIELD_LOG(" aca_sram_size=0x%x (KB)\n", t);
    x &= ~(((1U << STAT_CQ_SRAM_SIZE_ACA_SRAM_SIZE_WIDTH) - 1) << STAT_CQ_SRAM_SIZE_ACA_SRAM_SIZE_SHIFT);


    t = (x >> STAT_CQ_SRAM_SIZE_CTX_MAX_SIZE_SHIFT) & ((1U << STAT_CQ_SRAM_SIZE_CTX_MAX_SIZE_WIDTH) - 1);
    BITFIELD_LOG(" ctx_max_size=0x%x (wrap-out ctx max size in 32-bit word)\n", t);
    x &= ~(((1U << STAT_CQ_SRAM_SIZE_CTX_MAX_SIZE_WIDTH) - 1) << STAT_CQ_SRAM_SIZE_CTX_MAX_SIZE_SHIFT);


    t = (x >> STAT_CQ_SRAM_SIZE_ACA_CQ_DEPTH_SHIFT) & ((1U << STAT_CQ_SRAM_SIZE_ACA_CQ_DEPTH_WIDTH) - 1);
    BITFIELD_LOG(" aca_cq_depth=0x%x\n", t);
    x &= ~(((1U << STAT_CQ_SRAM_SIZE_ACA_CQ_DEPTH_WIDTH) - 1) << STAT_CQ_SRAM_SIZE_ACA_CQ_DEPTH_SHIFT);


    t = (x >> STAT_CQ_SRAM_SIZE_SCA_CQ_DEPTH_SHIFT) & ((1U << STAT_CQ_SRAM_SIZE_SCA_CQ_DEPTH_WIDTH) - 1);
    BITFIELD_LOG(" sca_cq_depth=0x%x\n", t);
    x &= ~(((1U << STAT_CQ_SRAM_SIZE_SCA_CQ_DEPTH_WIDTH) - 1) << STAT_CQ_SRAM_SIZE_SCA_CQ_DEPTH_SHIFT);


    t = (x >> STAT_CQ_SRAM_SIZE_HASH_CQ_DEPTH_SHIFT) & ((1U << STAT_CQ_SRAM_SIZE_HASH_CQ_DEPTH_WIDTH) - 1);
    BITFIELD_LOG(" hash_cq_depth=0x%x\n", t);
    x &= ~(((1U << STAT_CQ_SRAM_SIZE_HASH_CQ_DEPTH_WIDTH) - 1) << STAT_CQ_SRAM_SIZE_HASH_CQ_DEPTH_SHIFT);

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

#define log_stat_cq_sram_size(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_CQ_SRAM_SIZE_REG_H__ */
