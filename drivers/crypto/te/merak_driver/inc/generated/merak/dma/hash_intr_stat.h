//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __DMA_HASH_INTR_STAT_REG_H__
#define __DMA_HASH_INTR_STAT_REG_H__

#define DMA_HASH_INTR_STAT_WSIZE_MISMATCH_SHIFT 26
#define DMA_HASH_INTR_STAT_WSIZE_MISMATCH_WIDTH 1
#define DMA_HASH_INTR_STAT_WBLK_WDAT_TO_SHIFT 25
#define DMA_HASH_INTR_STAT_WBLK_WDAT_TO_WIDTH 1
#define DMA_HASH_INTR_STAT_WBLK_RADDR_TO_SHIFT 24
#define DMA_HASH_INTR_STAT_WBLK_RADDR_TO_WIDTH 1
#define DMA_HASH_INTR_STAT_WBLK_WDAT_SLVERR_SHIFT 20
#define DMA_HASH_INTR_STAT_WBLK_WDAT_SLVERR_WIDTH 1
#define DMA_HASH_INTR_STAT_WBLK_WDAT_DECERR_SHIFT 19
#define DMA_HASH_INTR_STAT_WBLK_WDAT_DECERR_WIDTH 1
#define DMA_HASH_INTR_STAT_WBLK_RADDR_SLVERR_SHIFT 18
#define DMA_HASH_INTR_STAT_WBLK_RADDR_SLVERR_WIDTH 1
#define DMA_HASH_INTR_STAT_WBLK_RADDR_DECERR_SHIFT 17
#define DMA_HASH_INTR_STAT_WBLK_RADDR_DECERR_WIDTH 1
#define DMA_HASH_INTR_STAT_WDMA_IDLE_SHIFT 16
#define DMA_HASH_INTR_STAT_WDMA_IDLE_WIDTH 1
#define DMA_HASH_INTR_STAT_RBLK_RDAT_TO_SHIFT 9
#define DMA_HASH_INTR_STAT_RBLK_RDAT_TO_WIDTH 1
#define DMA_HASH_INTR_STAT_RBLK_RADDR_TO_SHIFT 8
#define DMA_HASH_INTR_STAT_RBLK_RADDR_TO_WIDTH 1
#define DMA_HASH_INTR_STAT_RFIFO_LAST_ERR_SHIFT 5
#define DMA_HASH_INTR_STAT_RFIFO_LAST_ERR_WIDTH 1
#define DMA_HASH_INTR_STAT_RBLK_RDAT_SLVERR_SHIFT 4
#define DMA_HASH_INTR_STAT_RBLK_RDAT_SLVERR_WIDTH 1
#define DMA_HASH_INTR_STAT_RBLK_RDAT_DECERR_SHIFT 3
#define DMA_HASH_INTR_STAT_RBLK_RDAT_DECERR_WIDTH 1
#define DMA_HASH_INTR_STAT_RBLK_RADDR_SLVERR_SHIFT 2
#define DMA_HASH_INTR_STAT_RBLK_RADDR_SLVERR_WIDTH 1
#define DMA_HASH_INTR_STAT_RBLK_RADDR_DECERR_SHIFT 1
#define DMA_HASH_INTR_STAT_RBLK_RADDR_DECERR_WIDTH 1
#define DMA_HASH_INTR_STAT_RDMA_IDLE_SHIFT 0
#define DMA_HASH_INTR_STAT_RDMA_IDLE_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * dma_hash_intr_stat register definition.
 */
typedef union dma_hash_intr_statReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        rdma_idle: 1,
        rblk_raddr_decerr: 1,
        rblk_raddr_slverr: 1,
        rblk_rdat_decerr: 1,
        rblk_rdat_slverr: 1,
        rfifo_last_err: 1,
        hole8: 2,
        rblk_raddr_to: 1,
        rblk_rdat_to: 1,
        hole16: 6,
        wdma_idle: 1,
        wblk_raddr_decerr: 1,
        wblk_raddr_slverr: 1,
        wblk_wdat_decerr: 1,
        wblk_wdat_slverr: 1,
        hole24: 3,
        wblk_raddr_to: 1,
        wblk_wdat_to: 1,
        wsize_mismatch: 1,
        hole0: 5; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} dma_hash_intr_statReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_dma_hash_intr_stat(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("dma_hash_intr_stat: 0x%08x is\n", x);


    t = (x >> DMA_HASH_INTR_STAT_WSIZE_MISMATCH_SHIFT) & ((1U << DMA_HASH_INTR_STAT_WSIZE_MISMATCH_WIDTH) - 1);
    BITFIELD_LOG(" wsize_mismatch=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_WSIZE_MISMATCH_WIDTH) - 1) << DMA_HASH_INTR_STAT_WSIZE_MISMATCH_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_WBLK_WDAT_TO_SHIFT) & ((1U << DMA_HASH_INTR_STAT_WBLK_WDAT_TO_WIDTH) - 1);
    BITFIELD_LOG(" wblk_wdat_to=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_WBLK_WDAT_TO_WIDTH) - 1) << DMA_HASH_INTR_STAT_WBLK_WDAT_TO_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_WBLK_RADDR_TO_SHIFT) & ((1U << DMA_HASH_INTR_STAT_WBLK_RADDR_TO_WIDTH) - 1);
    BITFIELD_LOG(" wblk_raddr_to=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_WBLK_RADDR_TO_WIDTH) - 1) << DMA_HASH_INTR_STAT_WBLK_RADDR_TO_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_WBLK_WDAT_SLVERR_SHIFT) & ((1U << DMA_HASH_INTR_STAT_WBLK_WDAT_SLVERR_WIDTH) - 1);
    BITFIELD_LOG(" wblk_wdat_slverr=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_WBLK_WDAT_SLVERR_WIDTH) - 1) << DMA_HASH_INTR_STAT_WBLK_WDAT_SLVERR_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_WBLK_WDAT_DECERR_SHIFT) & ((1U << DMA_HASH_INTR_STAT_WBLK_WDAT_DECERR_WIDTH) - 1);
    BITFIELD_LOG(" wblk_wdat_decerr=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_WBLK_WDAT_DECERR_WIDTH) - 1) << DMA_HASH_INTR_STAT_WBLK_WDAT_DECERR_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_WBLK_RADDR_SLVERR_SHIFT) & ((1U << DMA_HASH_INTR_STAT_WBLK_RADDR_SLVERR_WIDTH) - 1);
    BITFIELD_LOG(" wblk_raddr_slverr=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_WBLK_RADDR_SLVERR_WIDTH) - 1) << DMA_HASH_INTR_STAT_WBLK_RADDR_SLVERR_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_WBLK_RADDR_DECERR_SHIFT) & ((1U << DMA_HASH_INTR_STAT_WBLK_RADDR_DECERR_WIDTH) - 1);
    BITFIELD_LOG(" wblk_raddr_decerr=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_WBLK_RADDR_DECERR_WIDTH) - 1) << DMA_HASH_INTR_STAT_WBLK_RADDR_DECERR_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_WDMA_IDLE_SHIFT) & ((1U << DMA_HASH_INTR_STAT_WDMA_IDLE_WIDTH) - 1);
    BITFIELD_LOG(" wdma_idle=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_WDMA_IDLE_WIDTH) - 1) << DMA_HASH_INTR_STAT_WDMA_IDLE_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_RBLK_RDAT_TO_SHIFT) & ((1U << DMA_HASH_INTR_STAT_RBLK_RDAT_TO_WIDTH) - 1);
    BITFIELD_LOG(" rblk_rdat_to=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_RBLK_RDAT_TO_WIDTH) - 1) << DMA_HASH_INTR_STAT_RBLK_RDAT_TO_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_RBLK_RADDR_TO_SHIFT) & ((1U << DMA_HASH_INTR_STAT_RBLK_RADDR_TO_WIDTH) - 1);
    BITFIELD_LOG(" rblk_raddr_to=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_RBLK_RADDR_TO_WIDTH) - 1) << DMA_HASH_INTR_STAT_RBLK_RADDR_TO_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_RFIFO_LAST_ERR_SHIFT) & ((1U << DMA_HASH_INTR_STAT_RFIFO_LAST_ERR_WIDTH) - 1);
    BITFIELD_LOG(" rfifo_last_err=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_RFIFO_LAST_ERR_WIDTH) - 1) << DMA_HASH_INTR_STAT_RFIFO_LAST_ERR_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_RBLK_RDAT_SLVERR_SHIFT) & ((1U << DMA_HASH_INTR_STAT_RBLK_RDAT_SLVERR_WIDTH) - 1);
    BITFIELD_LOG(" rblk_rdat_slverr=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_RBLK_RDAT_SLVERR_WIDTH) - 1) << DMA_HASH_INTR_STAT_RBLK_RDAT_SLVERR_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_RBLK_RDAT_DECERR_SHIFT) & ((1U << DMA_HASH_INTR_STAT_RBLK_RDAT_DECERR_WIDTH) - 1);
    BITFIELD_LOG(" rblk_rdat_decerr=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_RBLK_RDAT_DECERR_WIDTH) - 1) << DMA_HASH_INTR_STAT_RBLK_RDAT_DECERR_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_RBLK_RADDR_SLVERR_SHIFT) & ((1U << DMA_HASH_INTR_STAT_RBLK_RADDR_SLVERR_WIDTH) - 1);
    BITFIELD_LOG(" rblk_raddr_slverr=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_RBLK_RADDR_SLVERR_WIDTH) - 1) << DMA_HASH_INTR_STAT_RBLK_RADDR_SLVERR_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_RBLK_RADDR_DECERR_SHIFT) & ((1U << DMA_HASH_INTR_STAT_RBLK_RADDR_DECERR_WIDTH) - 1);
    BITFIELD_LOG(" rblk_raddr_decerr=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_RBLK_RADDR_DECERR_WIDTH) - 1) << DMA_HASH_INTR_STAT_RBLK_RADDR_DECERR_SHIFT);


    t = (x >> DMA_HASH_INTR_STAT_RDMA_IDLE_SHIFT) & ((1U << DMA_HASH_INTR_STAT_RDMA_IDLE_WIDTH) - 1);
    BITFIELD_LOG(" rdma_idle=0x%x\n", t);
    x &= ~(((1U << DMA_HASH_INTR_STAT_RDMA_IDLE_WIDTH) - 1) << DMA_HASH_INTR_STAT_RDMA_IDLE_SHIFT);

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

#define log_dma_hash_intr_stat(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __DMA_HASH_INTR_STAT_REG_H__ */
