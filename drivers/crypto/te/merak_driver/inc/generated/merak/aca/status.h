//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __ACA_STATUS_REG_H__
#define __ACA_STATUS_REG_H__

#define ACA_STATUS_SRAM_SIZE_SHIFT 18 /**< in unit of 8KB, 3 is rsvd */
#define ACA_STATUS_SRAM_SIZE_WIDTH 2
#define ACA_STATUS_ENG_STAT_SHIFT 16 /**< status 0/1/2/3 - idle/oth/this/reset */
#define ACA_STATUS_ENG_STAT_WIDTH 2
#define ACA_STATUS_RED_TIMES_SHIFT 10 /**< times of substraction */
#define ACA_STATUS_RED_TIMES_WIDTH 6
#define ACA_STATUS_CARRY_SHIFT 9 /**< carry state of the last op */
#define ACA_STATUS_CARRY_WIDTH 1
#define ACA_STATUS_XOR_RES_ZERO_SHIFT 8
#define ACA_STATUS_XOR_RES_ZERO_WIDTH 1
#define ACA_STATUS_AND_RES_ZERO_SHIFT 7
#define ACA_STATUS_AND_RES_ZERO_WIDTH 1
#define ACA_STATUS_ADD_RES_ZERO_SHIFT 6
#define ACA_STATUS_ADD_RES_ZERO_WIDTH 1
#define ACA_STATUS_FIFO_FULL_SHIFT 5
#define ACA_STATUS_FIFO_FULL_WIDTH 1
#define ACA_STATUS_FIFO_EMPTY_SHIFT 4
#define ACA_STATUS_FIFO_EMPTY_WIDTH 1
#define ACA_STATUS_FIFO_FREE_SHIFT 0 /**< free number of the OP FIFO */
#define ACA_STATUS_FIFO_FREE_WIDTH 4

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * aca_status register definition.
 */
typedef union aca_statusReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        fifo_free: 4  /**< free number of the OP FIFO */,
        fifo_empty: 1,
        fifo_full: 1,
        add_res_zero: 1,
        and_res_zero: 1,
        xor_res_zero: 1,
        carry: 1  /**< carry state of the last op */,
        red_times: 6  /**< times of substraction */,
        eng_stat: 2  /**< status 0/1/2/3 - idle/oth/this/reset */,
        sram_size: 2  /**< in unit of 8KB, 3 is rsvd */,
        hole0: 12; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} aca_statusReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_aca_status(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("aca_status: 0x%08x is\n", x);


    t = (x >> ACA_STATUS_SRAM_SIZE_SHIFT) & ((1U << ACA_STATUS_SRAM_SIZE_WIDTH) - 1);
    BITFIELD_LOG(" sram_size=0x%x (in unit of 8KB, 3 is rsvd)\n", t);
    x &= ~(((1U << ACA_STATUS_SRAM_SIZE_WIDTH) - 1) << ACA_STATUS_SRAM_SIZE_SHIFT);


    t = (x >> ACA_STATUS_ENG_STAT_SHIFT) & ((1U << ACA_STATUS_ENG_STAT_WIDTH) - 1);
    BITFIELD_LOG(" eng_stat=0x%x (status 0/1/2/3 - idle/oth/this/reset)\n", t);
    x &= ~(((1U << ACA_STATUS_ENG_STAT_WIDTH) - 1) << ACA_STATUS_ENG_STAT_SHIFT);


    t = (x >> ACA_STATUS_RED_TIMES_SHIFT) & ((1U << ACA_STATUS_RED_TIMES_WIDTH) - 1);
    BITFIELD_LOG(" red_times=0x%x (times of substraction)\n", t);
    x &= ~(((1U << ACA_STATUS_RED_TIMES_WIDTH) - 1) << ACA_STATUS_RED_TIMES_SHIFT);


    t = (x >> ACA_STATUS_CARRY_SHIFT) & ((1U << ACA_STATUS_CARRY_WIDTH) - 1);
    BITFIELD_LOG(" carry=0x%x (carry state of the last op)\n", t);
    x &= ~(((1U << ACA_STATUS_CARRY_WIDTH) - 1) << ACA_STATUS_CARRY_SHIFT);


    t = (x >> ACA_STATUS_XOR_RES_ZERO_SHIFT) & ((1U << ACA_STATUS_XOR_RES_ZERO_WIDTH) - 1);
    BITFIELD_LOG(" xor_res_zero=0x%x\n", t);
    x &= ~(((1U << ACA_STATUS_XOR_RES_ZERO_WIDTH) - 1) << ACA_STATUS_XOR_RES_ZERO_SHIFT);


    t = (x >> ACA_STATUS_AND_RES_ZERO_SHIFT) & ((1U << ACA_STATUS_AND_RES_ZERO_WIDTH) - 1);
    BITFIELD_LOG(" and_res_zero=0x%x\n", t);
    x &= ~(((1U << ACA_STATUS_AND_RES_ZERO_WIDTH) - 1) << ACA_STATUS_AND_RES_ZERO_SHIFT);


    t = (x >> ACA_STATUS_ADD_RES_ZERO_SHIFT) & ((1U << ACA_STATUS_ADD_RES_ZERO_WIDTH) - 1);
    BITFIELD_LOG(" add_res_zero=0x%x\n", t);
    x &= ~(((1U << ACA_STATUS_ADD_RES_ZERO_WIDTH) - 1) << ACA_STATUS_ADD_RES_ZERO_SHIFT);


    t = (x >> ACA_STATUS_FIFO_FULL_SHIFT) & ((1U << ACA_STATUS_FIFO_FULL_WIDTH) - 1);
    BITFIELD_LOG(" fifo_full=0x%x\n", t);
    x &= ~(((1U << ACA_STATUS_FIFO_FULL_WIDTH) - 1) << ACA_STATUS_FIFO_FULL_SHIFT);


    t = (x >> ACA_STATUS_FIFO_EMPTY_SHIFT) & ((1U << ACA_STATUS_FIFO_EMPTY_WIDTH) - 1);
    BITFIELD_LOG(" fifo_empty=0x%x\n", t);
    x &= ~(((1U << ACA_STATUS_FIFO_EMPTY_WIDTH) - 1) << ACA_STATUS_FIFO_EMPTY_SHIFT);


    t = (x >> ACA_STATUS_FIFO_FREE_SHIFT) & ((1U << ACA_STATUS_FIFO_FREE_WIDTH) - 1);
    BITFIELD_LOG(" fifo_free=0x%x (free number of the OP FIFO)\n", t);
    x &= ~(((1U << ACA_STATUS_FIFO_FREE_WIDTH) - 1) << ACA_STATUS_FIFO_FREE_SHIFT);

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

#define log_aca_status(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __ACA_STATUS_REG_H__ */
