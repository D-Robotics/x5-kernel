//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_DMA_FIFO_DEPTH_REG_H__
#define __STAT_DMA_FIFO_DEPTH_REG_H__

#define STAT_DMA_FIFO_DEPTH_HASH_WR_SHIFT 24 /**< dma_rfifo_depth_hash */
#define STAT_DMA_FIFO_DEPTH_HASH_WR_WIDTH 5
#define STAT_DMA_FIFO_DEPTH_HASH_RD_SHIFT 16 /**< dma_wfifo_depth_hash */
#define STAT_DMA_FIFO_DEPTH_HASH_RD_WIDTH 6
#define STAT_DMA_FIFO_DEPTH_SCA_WR_SHIFT 8 /**< dma_rfifo_depth_sca */
#define STAT_DMA_FIFO_DEPTH_SCA_WR_WIDTH 5
#define STAT_DMA_FIFO_DEPTH_SCA_RD_SHIFT 0 /**< dma_wfifo_depth_sca */
#define STAT_DMA_FIFO_DEPTH_SCA_RD_WIDTH 6

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * stat_dma_fifo_depth register definition.
 */
typedef union stat_dma_fifo_depthReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        sca_rd: 6  /**< dma_wfifo_depth_sca */,
        hole8: 2,
        sca_wr: 5  /**< dma_rfifo_depth_sca */,
        hole16: 3,
        hash_rd: 6  /**< dma_wfifo_depth_hash */,
        hole24: 2,
        hash_wr: 5  /**< dma_rfifo_depth_hash */,
        hole0: 3; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} stat_dma_fifo_depthReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_stat_dma_fifo_depth(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("stat_dma_fifo_depth: 0x%08x is\n", x);


    t = (x >> STAT_DMA_FIFO_DEPTH_HASH_WR_SHIFT) & ((1U << STAT_DMA_FIFO_DEPTH_HASH_WR_WIDTH) - 1);
    BITFIELD_LOG(" hash_wr=0x%x (dma_rfifo_depth_hash)\n", t);
    x &= ~(((1U << STAT_DMA_FIFO_DEPTH_HASH_WR_WIDTH) - 1) << STAT_DMA_FIFO_DEPTH_HASH_WR_SHIFT);


    t = (x >> STAT_DMA_FIFO_DEPTH_HASH_RD_SHIFT) & ((1U << STAT_DMA_FIFO_DEPTH_HASH_RD_WIDTH) - 1);
    BITFIELD_LOG(" hash_rd=0x%x (dma_wfifo_depth_hash)\n", t);
    x &= ~(((1U << STAT_DMA_FIFO_DEPTH_HASH_RD_WIDTH) - 1) << STAT_DMA_FIFO_DEPTH_HASH_RD_SHIFT);


    t = (x >> STAT_DMA_FIFO_DEPTH_SCA_WR_SHIFT) & ((1U << STAT_DMA_FIFO_DEPTH_SCA_WR_WIDTH) - 1);
    BITFIELD_LOG(" sca_wr=0x%x (dma_rfifo_depth_sca)\n", t);
    x &= ~(((1U << STAT_DMA_FIFO_DEPTH_SCA_WR_WIDTH) - 1) << STAT_DMA_FIFO_DEPTH_SCA_WR_SHIFT);


    t = (x >> STAT_DMA_FIFO_DEPTH_SCA_RD_SHIFT) & ((1U << STAT_DMA_FIFO_DEPTH_SCA_RD_WIDTH) - 1);
    BITFIELD_LOG(" sca_rd=0x%x (dma_wfifo_depth_sca)\n", t);
    x &= ~(((1U << STAT_DMA_FIFO_DEPTH_SCA_RD_WIDTH) - 1) << STAT_DMA_FIFO_DEPTH_SCA_RD_SHIFT);

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

#define log_stat_dma_fifo_depth(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_DMA_FIFO_DEPTH_REG_H__ */
