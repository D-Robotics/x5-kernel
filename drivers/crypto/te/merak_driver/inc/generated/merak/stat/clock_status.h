//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_CLOCK_STATUS_REG_H__
#define __STAT_CLOCK_STATUS_REG_H__

#define STAT_CLOCK_STATUS_DMA_AXI_EN_SHIFT 8
#define STAT_CLOCK_STATUS_DMA_AXI_EN_WIDTH 1
#define STAT_CLOCK_STATUS_DMA_HASH_EN_SHIFT 7
#define STAT_CLOCK_STATUS_DMA_HASH_EN_WIDTH 1
#define STAT_CLOCK_STATUS_DMA_SCA_EN_SHIFT 6
#define STAT_CLOCK_STATUS_DMA_SCA_EN_WIDTH 1
#define STAT_CLOCK_STATUS_OTP_CLK_EN_SHIFT 4
#define STAT_CLOCK_STATUS_OTP_CLK_EN_WIDTH 1
#define STAT_CLOCK_STATUS_TRNG_CLK_EN_SHIFT 3
#define STAT_CLOCK_STATUS_TRNG_CLK_EN_WIDTH 1
#define STAT_CLOCK_STATUS_ACA_CLK_EN_SHIFT 2
#define STAT_CLOCK_STATUS_ACA_CLK_EN_WIDTH 1
#define STAT_CLOCK_STATUS_SCA_CLK_EN_SHIFT 1
#define STAT_CLOCK_STATUS_SCA_CLK_EN_WIDTH 1
#define STAT_CLOCK_STATUS_HASH_CLK_EN_SHIFT 0
#define STAT_CLOCK_STATUS_HASH_CLK_EN_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * stat_clock_status register definition.
 */
typedef union stat_clock_statusReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        hash_clk_en: 1,
        sca_clk_en: 1,
        aca_clk_en: 1,
        trng_clk_en: 1,
        otp_clk_en: 1,
        hole6: 1,
        dma_sca_en: 1,
        dma_hash_en: 1,
        dma_axi_en: 1,
        hole0: 23; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} stat_clock_statusReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_stat_clock_status(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("stat_clock_status: 0x%08x is\n", x);


    t = (x >> STAT_CLOCK_STATUS_DMA_AXI_EN_SHIFT) & ((1U << STAT_CLOCK_STATUS_DMA_AXI_EN_WIDTH) - 1);
    BITFIELD_LOG(" dma_axi_en=0x%x\n", t);
    x &= ~(((1U << STAT_CLOCK_STATUS_DMA_AXI_EN_WIDTH) - 1) << STAT_CLOCK_STATUS_DMA_AXI_EN_SHIFT);


    t = (x >> STAT_CLOCK_STATUS_DMA_HASH_EN_SHIFT) & ((1U << STAT_CLOCK_STATUS_DMA_HASH_EN_WIDTH) - 1);
    BITFIELD_LOG(" dma_hash_en=0x%x\n", t);
    x &= ~(((1U << STAT_CLOCK_STATUS_DMA_HASH_EN_WIDTH) - 1) << STAT_CLOCK_STATUS_DMA_HASH_EN_SHIFT);


    t = (x >> STAT_CLOCK_STATUS_DMA_SCA_EN_SHIFT) & ((1U << STAT_CLOCK_STATUS_DMA_SCA_EN_WIDTH) - 1);
    BITFIELD_LOG(" dma_sca_en=0x%x\n", t);
    x &= ~(((1U << STAT_CLOCK_STATUS_DMA_SCA_EN_WIDTH) - 1) << STAT_CLOCK_STATUS_DMA_SCA_EN_SHIFT);


    t = (x >> STAT_CLOCK_STATUS_OTP_CLK_EN_SHIFT) & ((1U << STAT_CLOCK_STATUS_OTP_CLK_EN_WIDTH) - 1);
    BITFIELD_LOG(" otp_clk_en=0x%x\n", t);
    x &= ~(((1U << STAT_CLOCK_STATUS_OTP_CLK_EN_WIDTH) - 1) << STAT_CLOCK_STATUS_OTP_CLK_EN_SHIFT);


    t = (x >> STAT_CLOCK_STATUS_TRNG_CLK_EN_SHIFT) & ((1U << STAT_CLOCK_STATUS_TRNG_CLK_EN_WIDTH) - 1);
    BITFIELD_LOG(" trng_clk_en=0x%x\n", t);
    x &= ~(((1U << STAT_CLOCK_STATUS_TRNG_CLK_EN_WIDTH) - 1) << STAT_CLOCK_STATUS_TRNG_CLK_EN_SHIFT);


    t = (x >> STAT_CLOCK_STATUS_ACA_CLK_EN_SHIFT) & ((1U << STAT_CLOCK_STATUS_ACA_CLK_EN_WIDTH) - 1);
    BITFIELD_LOG(" aca_clk_en=0x%x\n", t);
    x &= ~(((1U << STAT_CLOCK_STATUS_ACA_CLK_EN_WIDTH) - 1) << STAT_CLOCK_STATUS_ACA_CLK_EN_SHIFT);


    t = (x >> STAT_CLOCK_STATUS_SCA_CLK_EN_SHIFT) & ((1U << STAT_CLOCK_STATUS_SCA_CLK_EN_WIDTH) - 1);
    BITFIELD_LOG(" sca_clk_en=0x%x\n", t);
    x &= ~(((1U << STAT_CLOCK_STATUS_SCA_CLK_EN_WIDTH) - 1) << STAT_CLOCK_STATUS_SCA_CLK_EN_SHIFT);


    t = (x >> STAT_CLOCK_STATUS_HASH_CLK_EN_SHIFT) & ((1U << STAT_CLOCK_STATUS_HASH_CLK_EN_WIDTH) - 1);
    BITFIELD_LOG(" hash_clk_en=0x%x\n", t);
    x &= ~(((1U << STAT_CLOCK_STATUS_HASH_CLK_EN_WIDTH) - 1) << STAT_CLOCK_STATUS_HASH_CLK_EN_SHIFT);

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

#define log_stat_clock_status(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_CLOCK_STATUS_REG_H__ */
