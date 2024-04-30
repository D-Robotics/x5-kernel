//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __CTL_TOP_CFG_REG_H__
#define __CTL_TOP_CFG_REG_H__

#define CTL_TOP_CFG_RNP_ARB_ALGO_SHIFT 9
#define CTL_TOP_CFG_RNP_ARB_ALGO_WIDTH 1
#define CTL_TOP_CFG_RNP_ARB_GRAN_SHIFT 8
#define CTL_TOP_CFG_RNP_ARB_GRAN_WIDTH 1
#define CTL_TOP_CFG_ACA_ARB_ALGO_SHIFT 7
#define CTL_TOP_CFG_ACA_ARB_ALGO_WIDTH 1
#define CTL_TOP_CFG_ACA_ARB_GRAN_SHIFT 6
#define CTL_TOP_CFG_ACA_ARB_GRAN_WIDTH 1
#define CTL_TOP_CFG_SCA_ARB_ALGO_SHIFT 5
#define CTL_TOP_CFG_SCA_ARB_ALGO_WIDTH 1
#define CTL_TOP_CFG_SCA_ARB_GRAN_SHIFT 4
#define CTL_TOP_CFG_SCA_ARB_GRAN_WIDTH 1
#define CTL_TOP_CFG_HASH_ARB_ALGO_SHIFT 3
#define CTL_TOP_CFG_HASH_ARB_ALGO_WIDTH 1
#define CTL_TOP_CFG_HASH_ARB_GRAN_SHIFT 2
#define CTL_TOP_CFG_HASH_ARB_GRAN_WIDTH 1
#define CTL_TOP_CFG_SW_INIT_DONE_SHIFT 1
#define CTL_TOP_CFG_SW_INIT_DONE_WIDTH 1
#define CTL_TOP_CFG_CTX_POOL_LOCK_SHIFT 0
#define CTL_TOP_CFG_CTX_POOL_LOCK_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * ctl_top_cfg register definition.
 */
typedef union ctl_top_cfgReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        ctx_pool_lock: 1,
        sw_init_done: 1,
        hash_arb_gran: 1,
        hash_arb_algo: 1,
        sca_arb_gran: 1,
        sca_arb_algo: 1,
        aca_arb_gran: 1,
        aca_arb_algo: 1,
        rnp_arb_gran: 1,
        rnp_arb_algo: 1,
        hole0: 22; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} ctl_top_cfgReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_ctl_top_cfg(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("ctl_top_cfg: 0x%08x is\n", x);


    t = (x >> CTL_TOP_CFG_RNP_ARB_ALGO_SHIFT) & ((1U << CTL_TOP_CFG_RNP_ARB_ALGO_WIDTH) - 1);
    BITFIELD_LOG(" rnp_arb_algo=0x%x\n", t);
    x &= ~(((1U << CTL_TOP_CFG_RNP_ARB_ALGO_WIDTH) - 1) << CTL_TOP_CFG_RNP_ARB_ALGO_SHIFT);


    t = (x >> CTL_TOP_CFG_RNP_ARB_GRAN_SHIFT) & ((1U << CTL_TOP_CFG_RNP_ARB_GRAN_WIDTH) - 1);
    BITFIELD_LOG(" rnp_arb_gran=0x%x\n", t);
    x &= ~(((1U << CTL_TOP_CFG_RNP_ARB_GRAN_WIDTH) - 1) << CTL_TOP_CFG_RNP_ARB_GRAN_SHIFT);


    t = (x >> CTL_TOP_CFG_ACA_ARB_ALGO_SHIFT) & ((1U << CTL_TOP_CFG_ACA_ARB_ALGO_WIDTH) - 1);
    BITFIELD_LOG(" aca_arb_algo=0x%x\n", t);
    x &= ~(((1U << CTL_TOP_CFG_ACA_ARB_ALGO_WIDTH) - 1) << CTL_TOP_CFG_ACA_ARB_ALGO_SHIFT);


    t = (x >> CTL_TOP_CFG_ACA_ARB_GRAN_SHIFT) & ((1U << CTL_TOP_CFG_ACA_ARB_GRAN_WIDTH) - 1);
    BITFIELD_LOG(" aca_arb_gran=0x%x\n", t);
    x &= ~(((1U << CTL_TOP_CFG_ACA_ARB_GRAN_WIDTH) - 1) << CTL_TOP_CFG_ACA_ARB_GRAN_SHIFT);


    t = (x >> CTL_TOP_CFG_SCA_ARB_ALGO_SHIFT) & ((1U << CTL_TOP_CFG_SCA_ARB_ALGO_WIDTH) - 1);
    BITFIELD_LOG(" sca_arb_algo=0x%x\n", t);
    x &= ~(((1U << CTL_TOP_CFG_SCA_ARB_ALGO_WIDTH) - 1) << CTL_TOP_CFG_SCA_ARB_ALGO_SHIFT);


    t = (x >> CTL_TOP_CFG_SCA_ARB_GRAN_SHIFT) & ((1U << CTL_TOP_CFG_SCA_ARB_GRAN_WIDTH) - 1);
    BITFIELD_LOG(" sca_arb_gran=0x%x\n", t);
    x &= ~(((1U << CTL_TOP_CFG_SCA_ARB_GRAN_WIDTH) - 1) << CTL_TOP_CFG_SCA_ARB_GRAN_SHIFT);


    t = (x >> CTL_TOP_CFG_HASH_ARB_ALGO_SHIFT) & ((1U << CTL_TOP_CFG_HASH_ARB_ALGO_WIDTH) - 1);
    BITFIELD_LOG(" hash_arb_algo=0x%x\n", t);
    x &= ~(((1U << CTL_TOP_CFG_HASH_ARB_ALGO_WIDTH) - 1) << CTL_TOP_CFG_HASH_ARB_ALGO_SHIFT);


    t = (x >> CTL_TOP_CFG_HASH_ARB_GRAN_SHIFT) & ((1U << CTL_TOP_CFG_HASH_ARB_GRAN_WIDTH) - 1);
    BITFIELD_LOG(" hash_arb_gran=0x%x\n", t);
    x &= ~(((1U << CTL_TOP_CFG_HASH_ARB_GRAN_WIDTH) - 1) << CTL_TOP_CFG_HASH_ARB_GRAN_SHIFT);


    t = (x >> CTL_TOP_CFG_SW_INIT_DONE_SHIFT) & ((1U << CTL_TOP_CFG_SW_INIT_DONE_WIDTH) - 1);
    BITFIELD_LOG(" sw_init_done=0x%x\n", t);
    x &= ~(((1U << CTL_TOP_CFG_SW_INIT_DONE_WIDTH) - 1) << CTL_TOP_CFG_SW_INIT_DONE_SHIFT);


    t = (x >> CTL_TOP_CFG_CTX_POOL_LOCK_SHIFT) & ((1U << CTL_TOP_CFG_CTX_POOL_LOCK_WIDTH) - 1);
    BITFIELD_LOG(" ctx_pool_lock=0x%x\n", t);
    x &= ~(((1U << CTL_TOP_CFG_CTX_POOL_LOCK_WIDTH) - 1) << CTL_TOP_CFG_CTX_POOL_LOCK_SHIFT);

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

#define log_ctl_top_cfg(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __CTL_TOP_CFG_REG_H__ */
