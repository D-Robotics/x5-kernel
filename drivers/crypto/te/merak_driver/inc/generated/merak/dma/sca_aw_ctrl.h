//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __DMA_SCA_AW_CTRL_REG_H__
#define __DMA_SCA_AW_CTRL_REG_H__

#define DMA_SCA_AW_CTRL_OUTSTD_SHIFT 28 /**< aw_outstanding */
#define DMA_SCA_AW_CTRL_OUTSTD_WIDTH 3
#define DMA_SCA_AW_CTRL_BLEN_SHIFT 24 /**< max burst len */
#define DMA_SCA_AW_CTRL_BLEN_WIDTH 4
#define DMA_SCA_AW_CTRL_LOCK_SHIFT 20
#define DMA_SCA_AW_CTRL_LOCK_WIDTH 2
#define DMA_SCA_AW_CTRL_USR_SHIFT 12
#define DMA_SCA_AW_CTRL_USR_WIDTH 8
#define DMA_SCA_AW_CTRL_QOS_SHIFT 8
#define DMA_SCA_AW_CTRL_QOS_WIDTH 4
#define DMA_SCA_AW_CTRL_CACHE_SHIFT 4
#define DMA_SCA_AW_CTRL_CACHE_WIDTH 4
#define DMA_SCA_AW_CTRL_RGN_SHIFT 0 /**< awregion */
#define DMA_SCA_AW_CTRL_RGN_WIDTH 4

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * dma_sca_aw_ctrl register definition.
 */
typedef union dma_sca_aw_ctrlReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        rgn: 4  /**< awregion */,
        cache: 4,
        qos: 4,
        usr: 8,
        lock: 2,
        hole24: 2,
        blen: 4  /**< max burst len */,
        outstd: 3  /**< aw_outstanding */,
        hole0: 1; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} dma_sca_aw_ctrlReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_dma_sca_aw_ctrl(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("dma_sca_aw_ctrl: 0x%08x is\n", x);


    t = (x >> DMA_SCA_AW_CTRL_OUTSTD_SHIFT) & ((1U << DMA_SCA_AW_CTRL_OUTSTD_WIDTH) - 1);
    BITFIELD_LOG(" outstd=0x%x (aw_outstanding)\n", t);
    x &= ~(((1U << DMA_SCA_AW_CTRL_OUTSTD_WIDTH) - 1) << DMA_SCA_AW_CTRL_OUTSTD_SHIFT);


    t = (x >> DMA_SCA_AW_CTRL_BLEN_SHIFT) & ((1U << DMA_SCA_AW_CTRL_BLEN_WIDTH) - 1);
    BITFIELD_LOG(" blen=0x%x (max burst len)\n", t);
    x &= ~(((1U << DMA_SCA_AW_CTRL_BLEN_WIDTH) - 1) << DMA_SCA_AW_CTRL_BLEN_SHIFT);


    t = (x >> DMA_SCA_AW_CTRL_LOCK_SHIFT) & ((1U << DMA_SCA_AW_CTRL_LOCK_WIDTH) - 1);
    BITFIELD_LOG(" lock=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_AW_CTRL_LOCK_WIDTH) - 1) << DMA_SCA_AW_CTRL_LOCK_SHIFT);


    t = (x >> DMA_SCA_AW_CTRL_USR_SHIFT) & ((1U << DMA_SCA_AW_CTRL_USR_WIDTH) - 1);
    BITFIELD_LOG(" usr=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_AW_CTRL_USR_WIDTH) - 1) << DMA_SCA_AW_CTRL_USR_SHIFT);


    t = (x >> DMA_SCA_AW_CTRL_QOS_SHIFT) & ((1U << DMA_SCA_AW_CTRL_QOS_WIDTH) - 1);
    BITFIELD_LOG(" qos=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_AW_CTRL_QOS_WIDTH) - 1) << DMA_SCA_AW_CTRL_QOS_SHIFT);


    t = (x >> DMA_SCA_AW_CTRL_CACHE_SHIFT) & ((1U << DMA_SCA_AW_CTRL_CACHE_WIDTH) - 1);
    BITFIELD_LOG(" cache=0x%x\n", t);
    x &= ~(((1U << DMA_SCA_AW_CTRL_CACHE_WIDTH) - 1) << DMA_SCA_AW_CTRL_CACHE_SHIFT);


    t = (x >> DMA_SCA_AW_CTRL_RGN_SHIFT) & ((1U << DMA_SCA_AW_CTRL_RGN_WIDTH) - 1);
    BITFIELD_LOG(" rgn=0x%x (awregion)\n", t);
    x &= ~(((1U << DMA_SCA_AW_CTRL_RGN_WIDTH) - 1) << DMA_SCA_AW_CTRL_RGN_SHIFT);

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

#define log_dma_sca_aw_ctrl(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __DMA_SCA_AW_CTRL_REG_H__ */
