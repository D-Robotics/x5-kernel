//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRNG_CTRL_REG_H__
#define __TRNG_CTRL_REG_H__

#define TRNG_CTRL_TRNG_CLK_EN_SHIFT 1
#define TRNG_CTRL_TRNG_CLK_EN_WIDTH 1
#define TRNG_CTRL_RNP_FILL_REQ_SHIFT 0
#define TRNG_CTRL_RNP_FILL_REQ_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * trng_ctrl register definition.
 */
typedef union trng_ctrlReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        rnp_fill_req: 1,
        trng_clk_en: 1,
        hole0: 30; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} trng_ctrlReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_trng_ctrl(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("trng_ctrl: 0x%08x is\n", x);


    t = (x >> TRNG_CTRL_TRNG_CLK_EN_SHIFT) & ((1U << TRNG_CTRL_TRNG_CLK_EN_WIDTH) - 1);
    BITFIELD_LOG(" trng_clk_en=0x%x\n", t);
    x &= ~(((1U << TRNG_CTRL_TRNG_CLK_EN_WIDTH) - 1) << TRNG_CTRL_TRNG_CLK_EN_SHIFT);


    t = (x >> TRNG_CTRL_RNP_FILL_REQ_SHIFT) & ((1U << TRNG_CTRL_RNP_FILL_REQ_WIDTH) - 1);
    BITFIELD_LOG(" rnp_fill_req=0x%x\n", t);
    x &= ~(((1U << TRNG_CTRL_RNP_FILL_REQ_WIDTH) - 1) << TRNG_CTRL_RNP_FILL_REQ_SHIFT);

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

#define log_trng_ctrl(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNG_CTRL_REG_H__ */
