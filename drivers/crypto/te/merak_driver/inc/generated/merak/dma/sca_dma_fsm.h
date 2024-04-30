//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __DMA_SCA_DMA_FSM_REG_H__
#define __DMA_SCA_DMA_FSM_REG_H__

#define DMA_SCA_DMA_FSM_RDATA_SHIFT 16
#define DMA_SCA_DMA_FSM_RDATA_WIDTH 16
#define DMA_SCA_DMA_FSM_WDATA_SHIFT 0
#define DMA_SCA_DMA_FSM_WDATA_WIDTH 16

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * dma_sca_dma_fsm register definition.
 */
typedef union dma_sca_dma_fsmReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        wdata: 16,
        rdata: 16; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} dma_sca_dma_fsmReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_dma_sca_dma_fsm(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("dma_sca_dma_fsm: 0x%08x is\n", x);


    t = (x >> DMA_SCA_DMA_FSM_RDATA_SHIFT) & ((1U << DMA_SCA_DMA_FSM_RDATA_WIDTH) - 1);
    BITFIELD_LOG(" rdata=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_DMA_FSM_RDATA_WIDTH) - 1) << DMA_SCA_DMA_FSM_RDATA_SHIFT);


    t = (x >> DMA_SCA_DMA_FSM_WDATA_SHIFT) & ((1U << DMA_SCA_DMA_FSM_WDATA_WIDTH) - 1);
    BITFIELD_LOG(" wdata=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_DMA_FSM_WDATA_WIDTH) - 1) << DMA_SCA_DMA_FSM_WDATA_SHIFT);

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

#define log_dma_sca_dma_fsm(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __DMA_SCA_DMA_FSM_REG_H__ */
