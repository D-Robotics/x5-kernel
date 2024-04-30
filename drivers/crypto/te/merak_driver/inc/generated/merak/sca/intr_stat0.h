//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __SCA_INTR_STAT0_REG_H__
#define __SCA_INTR_STAT0_REG_H__

#define SCA_INTR_STAT0_PARA_ERR_SHIFT 5
#define SCA_INTR_STAT0_PARA_ERR_WIDTH 1
#define SCA_INTR_STAT0_AXI_TO_ERR_SHIFT 4
#define SCA_INTR_STAT0_AXI_TO_ERR_WIDTH 1
#define SCA_INTR_STAT0_CQ_WR_ERR_SHIFT 3
#define SCA_INTR_STAT0_CQ_WR_ERR_WIDTH 1
#define SCA_INTR_STAT0_CSQ_RD_ERR_SHIFT 2 /**< csq is empty */
#define SCA_INTR_STAT0_CSQ_RD_ERR_WIDTH 1
#define SCA_INTR_STAT0_OPCODE_ERR_SHIFT 1
#define SCA_INTR_STAT0_OPCODE_ERR_WIDTH 1
#define SCA_INTR_STAT0_CQ_WM_SHIFT 0
#define SCA_INTR_STAT0_CQ_WM_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * sca_intr_stat0 register definition.
 */
typedef union sca_intr_stat0Reg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        cq_wm: 1,
        opcode_err: 1,
        csq_rd_err: 1  /**< csq is empty */,
        cq_wr_err: 1,
        axi_to_err: 1,
        para_err: 1,
        hole0: 26; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} sca_intr_stat0Reg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_sca_intr_stat0(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("sca_intr_stat0: 0x%08x is\n", x);


    t = (x >> SCA_INTR_STAT0_PARA_ERR_SHIFT) & ((1U << SCA_INTR_STAT0_PARA_ERR_WIDTH) - 1);
    BITFIELD_LOG(" para_err=0x%x\n", t);
    x &= ~(((1U << SCA_INTR_STAT0_PARA_ERR_WIDTH) - 1) << SCA_INTR_STAT0_PARA_ERR_SHIFT);


    t = (x >> SCA_INTR_STAT0_AXI_TO_ERR_SHIFT) & ((1U << SCA_INTR_STAT0_AXI_TO_ERR_WIDTH) - 1);
    BITFIELD_LOG(" axi_to_err=0x%x\n", t);
    x &= ~(((1U << SCA_INTR_STAT0_AXI_TO_ERR_WIDTH) - 1) << SCA_INTR_STAT0_AXI_TO_ERR_SHIFT);


    t = (x >> SCA_INTR_STAT0_CQ_WR_ERR_SHIFT) & ((1U << SCA_INTR_STAT0_CQ_WR_ERR_WIDTH) - 1);
    BITFIELD_LOG(" cq_wr_err=0x%x\n", t);
    x &= ~(((1U << SCA_INTR_STAT0_CQ_WR_ERR_WIDTH) - 1) << SCA_INTR_STAT0_CQ_WR_ERR_SHIFT);


    t = (x >> SCA_INTR_STAT0_CSQ_RD_ERR_SHIFT) & ((1U << SCA_INTR_STAT0_CSQ_RD_ERR_WIDTH) - 1);
    BITFIELD_LOG(" csq_rd_err=0x%x (csq is empty)\n", t);
    x &= ~(((1U << SCA_INTR_STAT0_CSQ_RD_ERR_WIDTH) - 1) << SCA_INTR_STAT0_CSQ_RD_ERR_SHIFT);


    t = (x >> SCA_INTR_STAT0_OPCODE_ERR_SHIFT) & ((1U << SCA_INTR_STAT0_OPCODE_ERR_WIDTH) - 1);
    BITFIELD_LOG(" opcode_err=0x%x\n", t);
    x &= ~(((1U << SCA_INTR_STAT0_OPCODE_ERR_WIDTH) - 1) << SCA_INTR_STAT0_OPCODE_ERR_SHIFT);


    t = (x >> SCA_INTR_STAT0_CQ_WM_SHIFT) & ((1U << SCA_INTR_STAT0_CQ_WM_WIDTH) - 1);
    BITFIELD_LOG(" cq_wm=0x%x\n", t);
    x &= ~(((1U << SCA_INTR_STAT0_CQ_WM_WIDTH) - 1) << SCA_INTR_STAT0_CQ_WM_SHIFT);

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

#define log_sca_intr_stat0(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __SCA_INTR_STAT0_REG_H__ */
