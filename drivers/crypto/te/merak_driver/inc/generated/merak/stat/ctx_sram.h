//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_CTX_SRAM_REG_H__
#define __STAT_CTX_SRAM_REG_H__

#define STAT_CTX_SRAM_CTX_SRAM_SIZE_SHIFT 0 /**< 32-bit word */
#define STAT_CTX_SRAM_CTX_SRAM_SIZE_WIDTH 16

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * stat_ctx_sram register definition.
 */
typedef union stat_ctx_sramReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        ctx_sram_size: 16  /**< 32-bit word */,
        hole0: 16; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} stat_ctx_sramReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_stat_ctx_sram(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("stat_ctx_sram: 0x%08x is\n", x);


    t = (x >> STAT_CTX_SRAM_CTX_SRAM_SIZE_SHIFT) & ((1U << STAT_CTX_SRAM_CTX_SRAM_SIZE_WIDTH) - 1);
    BITFIELD_LOG(" ctx_sram_size=0x%x (32-bit word)\n", t);
    x &= ~(((1U << STAT_CTX_SRAM_CTX_SRAM_SIZE_WIDTH) - 1) << STAT_CTX_SRAM_CTX_SRAM_SIZE_SHIFT);

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

#define log_stat_ctx_sram(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_CTX_SRAM_REG_H__ */
