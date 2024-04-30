//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __ACA_INTR_MSK_REG_H__
#define __ACA_INTR_MSK_REG_H__

#define ACA_INTR_MSK_MODN_ZERO_ERR_SHIFT 8
#define ACA_INTR_MSK_MODN_ZERO_ERR_WIDTH 1
#define ACA_INTR_MSK_RED_TH_EVT_SHIFT 7
#define ACA_INTR_MSK_RED_TH_EVT_WIDTH 1
#define ACA_INTR_MSK_MULT_RED_ERR_SHIFT 6
#define ACA_INTR_MSK_MULT_RED_ERR_WIDTH 1
#define ACA_INTR_MSK_FIFO_EMPTY_SHIFT 5
#define ACA_INTR_MSK_FIFO_EMPTY_WIDTH 1
#define ACA_INTR_MSK_MODINV_ZERO_SHIFT 4
#define ACA_INTR_MSK_MODINV_ZERO_WIDTH 1
#define ACA_INTR_MSK_DIV_ZERO_SHIFT 3
#define ACA_INTR_MSK_DIV_ZERO_WIDTH 1
#define ACA_INTR_MSK_FIFO_OVERFLOW_SHIFT 2
#define ACA_INTR_MSK_FIFO_OVERFLOW_WIDTH 1
#define ACA_INTR_MSK_FIFO_UNDER_WM_SHIFT 1
#define ACA_INTR_MSK_FIFO_UNDER_WM_WIDTH 1
#define ACA_INTR_MSK_CMD_FIN_SHIFT 0
#define ACA_INTR_MSK_CMD_FIN_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * aca_intr_msk register definition.
 */
typedef union aca_intr_mskReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        cmd_fin: 1,
        fifo_under_wm: 1,
        fifo_overflow: 1,
        div_zero: 1,
        modinv_zero: 1,
        fifo_empty: 1,
        mult_red_err: 1,
        red_th_evt: 1,
        modn_zero_err: 1,
        hole0: 23; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} aca_intr_mskReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_aca_intr_msk(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("aca_intr_msk: 0x%08x is\n", x);


    t = (x >> ACA_INTR_MSK_MODN_ZERO_ERR_SHIFT) & ((1U << ACA_INTR_MSK_MODN_ZERO_ERR_WIDTH) - 1);
    BITFIELD_LOG(" modn_zero_err=0x%x\n", t);
    x &= ~(((1U << ACA_INTR_MSK_MODN_ZERO_ERR_WIDTH) - 1) << ACA_INTR_MSK_MODN_ZERO_ERR_SHIFT);


    t = (x >> ACA_INTR_MSK_RED_TH_EVT_SHIFT) & ((1U << ACA_INTR_MSK_RED_TH_EVT_WIDTH) - 1);
    BITFIELD_LOG(" red_th_evt=0x%x\n", t);
    x &= ~(((1U << ACA_INTR_MSK_RED_TH_EVT_WIDTH) - 1) << ACA_INTR_MSK_RED_TH_EVT_SHIFT);


    t = (x >> ACA_INTR_MSK_MULT_RED_ERR_SHIFT) & ((1U << ACA_INTR_MSK_MULT_RED_ERR_WIDTH) - 1);
    BITFIELD_LOG(" mult_red_err=0x%x\n", t);
    x &= ~(((1U << ACA_INTR_MSK_MULT_RED_ERR_WIDTH) - 1) << ACA_INTR_MSK_MULT_RED_ERR_SHIFT);


    t = (x >> ACA_INTR_MSK_FIFO_EMPTY_SHIFT) & ((1U << ACA_INTR_MSK_FIFO_EMPTY_WIDTH) - 1);
    BITFIELD_LOG(" fifo_empty=0x%x\n", t);
    x &= ~(((1U << ACA_INTR_MSK_FIFO_EMPTY_WIDTH) - 1) << ACA_INTR_MSK_FIFO_EMPTY_SHIFT);


    t = (x >> ACA_INTR_MSK_MODINV_ZERO_SHIFT) & ((1U << ACA_INTR_MSK_MODINV_ZERO_WIDTH) - 1);
    BITFIELD_LOG(" modinv_zero=0x%x\n", t);
    x &= ~(((1U << ACA_INTR_MSK_MODINV_ZERO_WIDTH) - 1) << ACA_INTR_MSK_MODINV_ZERO_SHIFT);


    t = (x >> ACA_INTR_MSK_DIV_ZERO_SHIFT) & ((1U << ACA_INTR_MSK_DIV_ZERO_WIDTH) - 1);
    BITFIELD_LOG(" div_zero=0x%x\n", t);
    x &= ~(((1U << ACA_INTR_MSK_DIV_ZERO_WIDTH) - 1) << ACA_INTR_MSK_DIV_ZERO_SHIFT);


    t = (x >> ACA_INTR_MSK_FIFO_OVERFLOW_SHIFT) & ((1U << ACA_INTR_MSK_FIFO_OVERFLOW_WIDTH) - 1);
    BITFIELD_LOG(" fifo_overflow=0x%x\n", t);
    x &= ~(((1U << ACA_INTR_MSK_FIFO_OVERFLOW_WIDTH) - 1) << ACA_INTR_MSK_FIFO_OVERFLOW_SHIFT);


    t = (x >> ACA_INTR_MSK_FIFO_UNDER_WM_SHIFT) & ((1U << ACA_INTR_MSK_FIFO_UNDER_WM_WIDTH) - 1);
    BITFIELD_LOG(" fifo_under_wm=0x%x\n", t);
    x &= ~(((1U << ACA_INTR_MSK_FIFO_UNDER_WM_WIDTH) - 1) << ACA_INTR_MSK_FIFO_UNDER_WM_SHIFT);


    t = (x >> ACA_INTR_MSK_CMD_FIN_SHIFT) & ((1U << ACA_INTR_MSK_CMD_FIN_WIDTH) - 1);
    BITFIELD_LOG(" cmd_fin=0x%x\n", t);
    x &= ~(((1U << ACA_INTR_MSK_CMD_FIN_WIDTH) - 1) << ACA_INTR_MSK_CMD_FIN_SHIFT);

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

#define log_aca_intr_msk(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __ACA_INTR_MSK_REG_H__ */
