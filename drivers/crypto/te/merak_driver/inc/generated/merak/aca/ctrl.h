//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __ACA_CTRL_REG_H__
#define __ACA_CTRL_REG_H__

#define ACA_CTRL_CLK_EN_SHIFT 10
#define ACA_CTRL_CLK_EN_WIDTH 1
#define ACA_CTRL_OP_FIFO_WM_SHIFT 4
#define ACA_CTRL_OP_FIFO_WM_WIDTH 4
#define ACA_CTRL_OP_RUN_SHIFT 0
#define ACA_CTRL_OP_RUN_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * aca_ctrl register definition.
 */
typedef union aca_ctrlReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        op_run: 1,
        hole4: 3,
        op_fifo_wm: 4,
        hole10: 2,
        clk_en: 1,
        hole0: 21; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} aca_ctrlReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_aca_ctrl(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("aca_ctrl: 0x%08x is\n", x);


    t = (x >> ACA_CTRL_CLK_EN_SHIFT) & ((1U << ACA_CTRL_CLK_EN_WIDTH) - 1);
    BITFIELD_LOG(" clk_en=0x%x\n", t);
    x &= ~(((1U << ACA_CTRL_CLK_EN_WIDTH) - 1) << ACA_CTRL_CLK_EN_SHIFT);


    t = (x >> ACA_CTRL_OP_FIFO_WM_SHIFT) & ((1U << ACA_CTRL_OP_FIFO_WM_WIDTH) - 1);
    BITFIELD_LOG(" op_fifo_wm=0x%x\n", t);
    x &= ~(((1U << ACA_CTRL_OP_FIFO_WM_WIDTH) - 1) << ACA_CTRL_OP_FIFO_WM_SHIFT);


    t = (x >> ACA_CTRL_OP_RUN_SHIFT) & ((1U << ACA_CTRL_OP_RUN_WIDTH) - 1);
    BITFIELD_LOG(" op_run=0x%x\n", t);
    x &= ~(((1U << ACA_CTRL_OP_RUN_WIDTH) - 1) << ACA_CTRL_OP_RUN_SHIFT);

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

#define log_aca_ctrl(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __ACA_CTRL_REG_H__ */
