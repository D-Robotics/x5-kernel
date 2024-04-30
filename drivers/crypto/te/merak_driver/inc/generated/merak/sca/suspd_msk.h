//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __SCA_SUSPD_MSK_REG_H__
#define __SCA_SUSPD_MSK_REG_H__

#define SCA_SUSPD_MSK_PARA_ERR_SHIFT 5
#define SCA_SUSPD_MSK_PARA_ERR_WIDTH 1
#define SCA_SUSPD_MSK_CMD_FIN_SHIFT 4
#define SCA_SUSPD_MSK_CMD_FIN_WIDTH 1
#define SCA_SUSPD_MSK_OP_ERR_SHIFT 3
#define SCA_SUSPD_MSK_OP_ERR_WIDTH 1
#define SCA_SUSPD_MSK_CQ_WR_ERR_SHIFT 2
#define SCA_SUSPD_MSK_CQ_WR_ERR_WIDTH 1
#define SCA_SUSPD_MSK_CSQ_RD_ERR_SHIFT 1
#define SCA_SUSPD_MSK_CSQ_RD_ERR_WIDTH 1
#define SCA_SUSPD_MSK_OPCODE_ERR_SHIFT 0
#define SCA_SUSPD_MSK_OPCODE_ERR_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * sca_suspd_msk register definition.
 */
typedef union sca_suspd_mskReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        opcode_err: 1,
        csq_rd_err: 1,
        cq_wr_err: 1,
        op_err: 1,
        cmd_fin: 1,
        para_err: 1,
        hole0: 26; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} sca_suspd_mskReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_sca_suspd_msk(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("sca_suspd_msk: 0x%08x is\n", x);


    t = (x >> SCA_SUSPD_MSK_PARA_ERR_SHIFT) & ((1U << SCA_SUSPD_MSK_PARA_ERR_WIDTH) - 1);
    BITFIELD_LOG(" para_err=0x%x\n", t);
    x &= ~(((1U << SCA_SUSPD_MSK_PARA_ERR_WIDTH) - 1) << SCA_SUSPD_MSK_PARA_ERR_SHIFT);


    t = (x >> SCA_SUSPD_MSK_CMD_FIN_SHIFT) & ((1U << SCA_SUSPD_MSK_CMD_FIN_WIDTH) - 1);
    BITFIELD_LOG(" cmd_fin=0x%x\n", t);
    x &= ~(((1U << SCA_SUSPD_MSK_CMD_FIN_WIDTH) - 1) << SCA_SUSPD_MSK_CMD_FIN_SHIFT);


    t = (x >> SCA_SUSPD_MSK_OP_ERR_SHIFT) & ((1U << SCA_SUSPD_MSK_OP_ERR_WIDTH) - 1);
    BITFIELD_LOG(" op_err=0x%x\n", t);
    x &= ~(((1U << SCA_SUSPD_MSK_OP_ERR_WIDTH) - 1) << SCA_SUSPD_MSK_OP_ERR_SHIFT);


    t = (x >> SCA_SUSPD_MSK_CQ_WR_ERR_SHIFT) & ((1U << SCA_SUSPD_MSK_CQ_WR_ERR_WIDTH) - 1);
    BITFIELD_LOG(" cq_wr_err=0x%x\n", t);
    x &= ~(((1U << SCA_SUSPD_MSK_CQ_WR_ERR_WIDTH) - 1) << SCA_SUSPD_MSK_CQ_WR_ERR_SHIFT);


    t = (x >> SCA_SUSPD_MSK_CSQ_RD_ERR_SHIFT) & ((1U << SCA_SUSPD_MSK_CSQ_RD_ERR_WIDTH) - 1);
    BITFIELD_LOG(" csq_rd_err=0x%x\n", t);
    x &= ~(((1U << SCA_SUSPD_MSK_CSQ_RD_ERR_WIDTH) - 1) << SCA_SUSPD_MSK_CSQ_RD_ERR_SHIFT);


    t = (x >> SCA_SUSPD_MSK_OPCODE_ERR_SHIFT) & ((1U << SCA_SUSPD_MSK_OPCODE_ERR_WIDTH) - 1);
    BITFIELD_LOG(" opcode_err=0x%x\n", t);
    x &= ~(((1U << SCA_SUSPD_MSK_OPCODE_ERR_WIDTH) - 1) << SCA_SUSPD_MSK_OPCODE_ERR_SHIFT);

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

#define log_sca_suspd_msk(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __SCA_SUSPD_MSK_REG_H__ */
