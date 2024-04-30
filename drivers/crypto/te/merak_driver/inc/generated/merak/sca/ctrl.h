//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __SCA_CTRL_REG_H__
#define __SCA_CTRL_REG_H__

#define SCA_CTRL_HOST_RUN_SHIFT 7 /**< host run trigger */
#define SCA_CTRL_HOST_RUN_WIDTH 1
#define SCA_CTRL_CLK_EN_SHIFT 6
#define SCA_CTRL_CLK_EN_WIDTH 1
#define SCA_CTRL_CQ_WM_SHIFT 1
#define SCA_CTRL_CQ_WM_WIDTH 5
#define SCA_CTRL_CSQ_EN_SHIFT 0
#define SCA_CTRL_CSQ_EN_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * sca_ctrl register definition.
 */
typedef union sca_ctrlReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        csq_en: 1,
        cq_wm: 5,
        clk_en: 1,
        host_run: 1  /**< host run trigger */,
        hole0: 24; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} sca_ctrlReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_sca_ctrl(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("sca_ctrl: 0x%08x is\n", x);


    t = (x >> SCA_CTRL_HOST_RUN_SHIFT) & ((1U << SCA_CTRL_HOST_RUN_WIDTH) - 1);
    BITFIELD_LOG(" host_run=0x%x (host run trigger)\n", t);
    x &= ~(((1U << SCA_CTRL_HOST_RUN_WIDTH) - 1) << SCA_CTRL_HOST_RUN_SHIFT);


    t = (x >> SCA_CTRL_CLK_EN_SHIFT) & ((1U << SCA_CTRL_CLK_EN_WIDTH) - 1);
    BITFIELD_LOG(" clk_en=0x%x\n", t);
    x &= ~(((1U << SCA_CTRL_CLK_EN_WIDTH) - 1) << SCA_CTRL_CLK_EN_SHIFT);


    t = (x >> SCA_CTRL_CQ_WM_SHIFT) & ((1U << SCA_CTRL_CQ_WM_WIDTH) - 1);
    BITFIELD_LOG(" cq_wm=0x%x\n", t);
    x &= ~(((1U << SCA_CTRL_CQ_WM_WIDTH) - 1) << SCA_CTRL_CQ_WM_SHIFT);


    t = (x >> SCA_CTRL_CSQ_EN_SHIFT) & ((1U << SCA_CTRL_CSQ_EN_WIDTH) - 1);
    BITFIELD_LOG(" csq_en=0x%x\n", t);
    x &= ~(((1U << SCA_CTRL_CSQ_EN_WIDTH) - 1) << SCA_CTRL_CSQ_EN_SHIFT);

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

#define log_sca_ctrl(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __SCA_CTRL_REG_H__ */
