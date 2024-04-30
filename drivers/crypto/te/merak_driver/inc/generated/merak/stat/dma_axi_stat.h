//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_DMA_AXI_STAT_REG_H__
#define __STAT_DMA_AXI_STAT_REG_H__

#define STAT_DMA_AXI_STAT_WR_CH_EN_SHIFT 18 /**< dma_axi_wchn_pipe_ena */
#define STAT_DMA_AXI_STAT_WR_CH_EN_WIDTH 1
#define STAT_DMA_AXI_STAT_RD_CH_EN_SHIFT 17 /**< dma_axi_rchn_pipe_ena */
#define STAT_DMA_AXI_STAT_RD_CH_EN_WIDTH 1
#define STAT_DMA_AXI_STAT_WR_OUTSTD_SHIFT 12 /**< dma_wr_outstand */
#define STAT_DMA_AXI_STAT_WR_OUTSTD_WIDTH 5
#define STAT_DMA_AXI_STAT_RD_OUTSTD_SHIFT 8 /**< dma_rd_outstand */
#define STAT_DMA_AXI_STAT_RD_OUTSTD_WIDTH 4
#define STAT_DMA_AXI_STAT_ADDR_WIDTH_SHIFT 0
#define STAT_DMA_AXI_STAT_ADDR_WIDTH_WIDTH 7

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * stat_dma_axi_stat register definition.
 */
typedef union stat_dma_axi_statReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        addr_width: 7,
        hole8: 1,
        rd_outstd: 4  /**< dma_rd_outstand */,
        wr_outstd: 5  /**< dma_wr_outstand */,
        rd_ch_en: 1  /**< dma_axi_rchn_pipe_ena */,
        wr_ch_en: 1  /**< dma_axi_wchn_pipe_ena */,
        hole0: 13; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} stat_dma_axi_statReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_stat_dma_axi_stat(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("stat_dma_axi_stat: 0x%08x is\n", x);


    t = (x >> STAT_DMA_AXI_STAT_WR_CH_EN_SHIFT) & ((1U << STAT_DMA_AXI_STAT_WR_CH_EN_WIDTH) - 1);
    BITFIELD_LOG(" wr_ch_en=0x%x (dma_axi_wchn_pipe_ena)\n", t);
    x &= ~(((1U << STAT_DMA_AXI_STAT_WR_CH_EN_WIDTH) - 1) << STAT_DMA_AXI_STAT_WR_CH_EN_SHIFT);


    t = (x >> STAT_DMA_AXI_STAT_RD_CH_EN_SHIFT) & ((1U << STAT_DMA_AXI_STAT_RD_CH_EN_WIDTH) - 1);
    BITFIELD_LOG(" rd_ch_en=0x%x (dma_axi_rchn_pipe_ena)\n", t);
    x &= ~(((1U << STAT_DMA_AXI_STAT_RD_CH_EN_WIDTH) - 1) << STAT_DMA_AXI_STAT_RD_CH_EN_SHIFT);


    t = (x >> STAT_DMA_AXI_STAT_WR_OUTSTD_SHIFT) & ((1U << STAT_DMA_AXI_STAT_WR_OUTSTD_WIDTH) - 1);
    BITFIELD_LOG(" wr_outstd=0x%x (dma_wr_outstand)\n", t);
    x &= ~(((1U << STAT_DMA_AXI_STAT_WR_OUTSTD_WIDTH) - 1) << STAT_DMA_AXI_STAT_WR_OUTSTD_SHIFT);


    t = (x >> STAT_DMA_AXI_STAT_RD_OUTSTD_SHIFT) & ((1U << STAT_DMA_AXI_STAT_RD_OUTSTD_WIDTH) - 1);
    BITFIELD_LOG(" rd_outstd=0x%x (dma_rd_outstand)\n", t);
    x &= ~(((1U << STAT_DMA_AXI_STAT_RD_OUTSTD_WIDTH) - 1) << STAT_DMA_AXI_STAT_RD_OUTSTD_SHIFT);


    t = (x >> STAT_DMA_AXI_STAT_ADDR_WIDTH_SHIFT) & ((1U << STAT_DMA_AXI_STAT_ADDR_WIDTH_WIDTH) - 1);
    BITFIELD_LOG(" addr_width=0x%x\n", t);
    x &= ~(((1U << STAT_DMA_AXI_STAT_ADDR_WIDTH_WIDTH) - 1) << STAT_DMA_AXI_STAT_ADDR_WIDTH_SHIFT);

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

#define log_stat_dma_axi_stat(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_DMA_AXI_STAT_REG_H__ */
