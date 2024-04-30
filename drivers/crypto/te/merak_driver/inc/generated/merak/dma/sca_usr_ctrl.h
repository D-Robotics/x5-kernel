//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __DMA_SCA_USR_CTRL_REG_H__
#define __DMA_SCA_USR_CTRL_REG_H__

#define DMA_SCA_USR_CTRL_RUSR_SHIFT 16
#define DMA_SCA_USR_CTRL_RUSR_WIDTH 8
#define DMA_SCA_USR_CTRL_BUSR_SHIFT 8
#define DMA_SCA_USR_CTRL_BUSR_WIDTH 8
#define DMA_SCA_USR_CTRL_WUSR_SHIFT 0
#define DMA_SCA_USR_CTRL_WUSR_WIDTH 8

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * dma_sca_usr_ctrl register definition.
 */
typedef union dma_sca_usr_ctrlReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        wusr: 8,
        busr: 8,
        rusr: 8,
        hole0: 8; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} dma_sca_usr_ctrlReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_dma_sca_usr_ctrl(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("dma_sca_usr_ctrl: 0x%08x is\n", x);


    t = (x >> DMA_SCA_USR_CTRL_RUSR_SHIFT) & ((1U << DMA_SCA_USR_CTRL_RUSR_WIDTH) - 1);
    BITFIELD_LOG(" rusr=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_USR_CTRL_RUSR_WIDTH) - 1) << DMA_SCA_USR_CTRL_RUSR_SHIFT);


    t = (x >> DMA_SCA_USR_CTRL_BUSR_SHIFT) & ((1U << DMA_SCA_USR_CTRL_BUSR_WIDTH) - 1);
    BITFIELD_LOG(" busr=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_USR_CTRL_BUSR_WIDTH) - 1) << DMA_SCA_USR_CTRL_BUSR_SHIFT);


    t = (x >> DMA_SCA_USR_CTRL_WUSR_SHIFT) & ((1U << DMA_SCA_USR_CTRL_WUSR_WIDTH) - 1);
    BITFIELD_LOG(" wusr=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_USR_CTRL_WUSR_WIDTH) - 1) << DMA_SCA_USR_CTRL_WUSR_SHIFT);

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

#define log_dma_sca_usr_ctrl(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __DMA_SCA_USR_CTRL_REG_H__ */
