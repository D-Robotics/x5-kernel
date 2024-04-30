//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __DMA_TIMEOUT_CTRL_REG_H__
#define __DMA_TIMEOUT_CTRL_REG_H__

#define DMA_TIMEOUT_CTRL_EN_SHIFT 31 /**< timeout enable */
#define DMA_TIMEOUT_CTRL_EN_WIDTH 1
#define DMA_TIMEOUT_CTRL_CNT_SHIFT 0 /**< timeout counter based on clk_axi */
#define DMA_TIMEOUT_CTRL_CNT_WIDTH 31

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * dma_timeout_ctrl register definition.
 */
typedef union dma_timeout_ctrlReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        cnt: 31  /**< timeout counter based on clk_axi */,
        en: 1  /**< timeout enable */; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} dma_timeout_ctrlReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_dma_timeout_ctrl(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("dma_timeout_ctrl: 0x%08x is\n", x);


    t = (x >> DMA_TIMEOUT_CTRL_EN_SHIFT) & ((1U << DMA_TIMEOUT_CTRL_EN_WIDTH) - 1);
    BITFIELD_LOG(" en=0x%x (timeout enable)\n", t);
    x &= ~(((1U << DMA_TIMEOUT_CTRL_EN_WIDTH) - 1) << DMA_TIMEOUT_CTRL_EN_SHIFT);


    t = (x >> DMA_TIMEOUT_CTRL_CNT_SHIFT) & ((1U << DMA_TIMEOUT_CTRL_CNT_WIDTH) - 1);
    BITFIELD_LOG(" cnt=0x%x (timeout counter based on clk_axi)\n", t);
    x &= ~(((1U << DMA_TIMEOUT_CTRL_CNT_WIDTH) - 1) << DMA_TIMEOUT_CTRL_CNT_SHIFT);

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

#define log_dma_timeout_ctrl(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __DMA_TIMEOUT_CTRL_REG_H__ */
