//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __DMA_SCA_DMA_MISC_REG_H__
#define __DMA_SCA_DMA_MISC_REG_H__

#define DMA_SCA_DMA_MISC_RDATA_FIFO_EMPTY_SHIFT 21
#define DMA_SCA_DMA_MISC_RDATA_FIFO_EMPTY_WIDTH 1
#define DMA_SCA_DMA_MISC_RDATA_FIFO_FULL_SHIFT 20
#define DMA_SCA_DMA_MISC_RDATA_FIFO_FULL_WIDTH 1
#define DMA_SCA_DMA_MISC_RREQ_OUTSTD_SHIFT 16 /**< read outstanding */
#define DMA_SCA_DMA_MISC_RREQ_OUTSTD_WIDTH 4
#define DMA_SCA_DMA_MISC_WDATA_FIFO_EMPTY_SHIFT 5
#define DMA_SCA_DMA_MISC_WDATA_FIFO_EMPTY_WIDTH 1
#define DMA_SCA_DMA_MISC_WDATA_FIFO_FULL_SHIFT 4
#define DMA_SCA_DMA_MISC_WDATA_FIFO_FULL_WIDTH 1
#define DMA_SCA_DMA_MISC_WREQ_OUTSTD_SHIFT 0 /**< write outstanding */
#define DMA_SCA_DMA_MISC_WREQ_OUTSTD_WIDTH 4

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * dma_sca_dma_misc register definition.
 */
typedef union dma_sca_dma_miscReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        wreq_outstd: 4  /**< write outstanding */,
        wdata_fifo_full: 1,
        wdata_fifo_empty: 1,
        hole16: 10,
        rreq_outstd: 4  /**< read outstanding */,
        rdata_fifo_full: 1,
        rdata_fifo_empty: 1,
        hole0: 10; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} dma_sca_dma_miscReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_dma_sca_dma_misc(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("dma_sca_dma_misc: 0x%08x is\n", x);


    t = (x >> DMA_SCA_DMA_MISC_RDATA_FIFO_EMPTY_SHIFT) & ((1U << DMA_SCA_DMA_MISC_RDATA_FIFO_EMPTY_WIDTH) - 1);
    BITFIELD_LOG(" rdata_fifo_empty=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_DMA_MISC_RDATA_FIFO_EMPTY_WIDTH) - 1) << DMA_SCA_DMA_MISC_RDATA_FIFO_EMPTY_SHIFT);


    t = (x >> DMA_SCA_DMA_MISC_RDATA_FIFO_FULL_SHIFT) & ((1U << DMA_SCA_DMA_MISC_RDATA_FIFO_FULL_WIDTH) - 1);
    BITFIELD_LOG(" rdata_fifo_full=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_DMA_MISC_RDATA_FIFO_FULL_WIDTH) - 1) << DMA_SCA_DMA_MISC_RDATA_FIFO_FULL_SHIFT);


    t = (x >> DMA_SCA_DMA_MISC_RREQ_OUTSTD_SHIFT) & ((1U << DMA_SCA_DMA_MISC_RREQ_OUTSTD_WIDTH) - 1);
    BITFIELD_LOG(" rreq_outstd=0x%x (read outstanding)\n", t);
    x &= ~(((1U << DMA_SCA_DMA_MISC_RREQ_OUTSTD_WIDTH) - 1) << DMA_SCA_DMA_MISC_RREQ_OUTSTD_SHIFT);


    t = (x >> DMA_SCA_DMA_MISC_WDATA_FIFO_EMPTY_SHIFT) & ((1U << DMA_SCA_DMA_MISC_WDATA_FIFO_EMPTY_WIDTH) - 1);
    BITFIELD_LOG(" wdata_fifo_empty=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_DMA_MISC_WDATA_FIFO_EMPTY_WIDTH) - 1) << DMA_SCA_DMA_MISC_WDATA_FIFO_EMPTY_SHIFT);


    t = (x >> DMA_SCA_DMA_MISC_WDATA_FIFO_FULL_SHIFT) & ((1U << DMA_SCA_DMA_MISC_WDATA_FIFO_FULL_WIDTH) - 1);
    BITFIELD_LOG(" wdata_fifo_full=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_DMA_MISC_WDATA_FIFO_FULL_WIDTH) - 1) << DMA_SCA_DMA_MISC_WDATA_FIFO_FULL_SHIFT);


    t = (x >> DMA_SCA_DMA_MISC_WREQ_OUTSTD_SHIFT) & ((1U << DMA_SCA_DMA_MISC_WREQ_OUTSTD_WIDTH) - 1);
    BITFIELD_LOG(" wreq_outstd=0x%x (write outstanding)\n", t);
    x &= ~(((1U << DMA_SCA_DMA_MISC_WREQ_OUTSTD_WIDTH) - 1) << DMA_SCA_DMA_MISC_WREQ_OUTSTD_SHIFT);

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

#define log_dma_sca_dma_misc(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __DMA_SCA_DMA_MISC_REG_H__ */
