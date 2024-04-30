//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __DMA_SCA_ACE_CTRL_REG_H__
#define __DMA_SCA_ACE_CTRL_REG_H__

#define DMA_SCA_ACE_CTRL_ARDOMAIN_SHIFT 18
#define DMA_SCA_ACE_CTRL_ARDOMAIN_WIDTH 2
#define DMA_SCA_ACE_CTRL_ARBAR_SHIFT 16
#define DMA_SCA_ACE_CTRL_ARBAR_WIDTH 2
#define DMA_SCA_ACE_CTRL_ARSNOOP_SHIFT 12
#define DMA_SCA_ACE_CTRL_ARSNOOP_WIDTH 4
#define DMA_SCA_ACE_CTRL_AWUNIQUE_SHIFT 7
#define DMA_SCA_ACE_CTRL_AWUNIQUE_WIDTH 1
#define DMA_SCA_ACE_CTRL_AWSNOOP_SHIFT 4
#define DMA_SCA_ACE_CTRL_AWSNOOP_WIDTH 3
#define DMA_SCA_ACE_CTRL_AWDOMAIN_SHIFT 2
#define DMA_SCA_ACE_CTRL_AWDOMAIN_WIDTH 2
#define DMA_SCA_ACE_CTRL_AWBAR_SHIFT 0
#define DMA_SCA_ACE_CTRL_AWBAR_WIDTH 2

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * dma_sca_ace_ctrl register definition.
 */
typedef union dma_sca_ace_ctrlReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        awbar: 2,
        awdomain: 2,
        awsnoop: 3,
        awunique: 1,
        hole12: 4,
        arsnoop: 4,
        arbar: 2,
        ardomain: 2,
        hole0: 12; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} dma_sca_ace_ctrlReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_dma_sca_ace_ctrl(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("dma_sca_ace_ctrl: 0x%08x is\n", x);


    t = (x >> DMA_SCA_ACE_CTRL_ARDOMAIN_SHIFT) & ((1U << DMA_SCA_ACE_CTRL_ARDOMAIN_WIDTH) - 1);
    BITFIELD_LOG(" ardomain=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_ACE_CTRL_ARDOMAIN_WIDTH) - 1) << DMA_SCA_ACE_CTRL_ARDOMAIN_SHIFT);


    t = (x >> DMA_SCA_ACE_CTRL_ARBAR_SHIFT) & ((1U << DMA_SCA_ACE_CTRL_ARBAR_WIDTH) - 1);
    BITFIELD_LOG(" arbar=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_ACE_CTRL_ARBAR_WIDTH) - 1) << DMA_SCA_ACE_CTRL_ARBAR_SHIFT);


    t = (x >> DMA_SCA_ACE_CTRL_ARSNOOP_SHIFT) & ((1U << DMA_SCA_ACE_CTRL_ARSNOOP_WIDTH) - 1);
    BITFIELD_LOG(" arsnoop=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_ACE_CTRL_ARSNOOP_WIDTH) - 1) << DMA_SCA_ACE_CTRL_ARSNOOP_SHIFT);


    t = (x >> DMA_SCA_ACE_CTRL_AWUNIQUE_SHIFT) & ((1U << DMA_SCA_ACE_CTRL_AWUNIQUE_WIDTH) - 1);
    BITFIELD_LOG(" awunique=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_ACE_CTRL_AWUNIQUE_WIDTH) - 1) << DMA_SCA_ACE_CTRL_AWUNIQUE_SHIFT);


    t = (x >> DMA_SCA_ACE_CTRL_AWSNOOP_SHIFT) & ((1U << DMA_SCA_ACE_CTRL_AWSNOOP_WIDTH) - 1);
    BITFIELD_LOG(" awsnoop=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_ACE_CTRL_AWSNOOP_WIDTH) - 1) << DMA_SCA_ACE_CTRL_AWSNOOP_SHIFT);


    t = (x >> DMA_SCA_ACE_CTRL_AWDOMAIN_SHIFT) & ((1U << DMA_SCA_ACE_CTRL_AWDOMAIN_WIDTH) - 1);
    BITFIELD_LOG(" awdomain=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_ACE_CTRL_AWDOMAIN_WIDTH) - 1) << DMA_SCA_ACE_CTRL_AWDOMAIN_SHIFT);


    t = (x >> DMA_SCA_ACE_CTRL_AWBAR_SHIFT) & ((1U << DMA_SCA_ACE_CTRL_AWBAR_WIDTH) - 1);
    BITFIELD_LOG(" awbar=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_ACE_CTRL_AWBAR_WIDTH) - 1) << DMA_SCA_ACE_CTRL_AWBAR_SHIFT);

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

#define log_dma_sca_ace_ctrl(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __DMA_SCA_ACE_CTRL_REG_H__ */
