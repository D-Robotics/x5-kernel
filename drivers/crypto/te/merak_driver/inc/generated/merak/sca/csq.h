//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __SCA_CSQ_REG_H__
#define __SCA_CSQ_REG_H__

#define SCA_CSQ_DONE_SLOT_ID_SHIFT 8 /**< the slot_id of done cmd */
#define SCA_CSQ_DONE_SLOT_ID_WIDTH 5
#define SCA_CSQ_OPCODE_IDX_SHIFT 5 /**< the opcode index of done */
#define SCA_CSQ_OPCODE_IDX_WIDTH 3
#define SCA_CSQ_STAT_IDX_SHIFT 0 /**< the status of done cmd */
#define SCA_CSQ_STAT_IDX_WIDTH 5

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * sca_csq register definition.
 */
typedef union sca_csqReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        stat_idx: 5  /**< the status of done cmd */,
        opcode_idx: 3  /**< the opcode index of done */,
        done_slot_id: 5  /**< the slot_id of done cmd */,
        hole0: 19; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} sca_csqReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_sca_csq(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("sca_csq: 0x%08x is\n", x);


    t = (x >> SCA_CSQ_DONE_SLOT_ID_SHIFT) & ((1U << SCA_CSQ_DONE_SLOT_ID_WIDTH) - 1);
    BITFIELD_LOG(" done_slot_id=0x%x (the slot_id of done cmd)\n", t);
    x &= ~(((1U << SCA_CSQ_DONE_SLOT_ID_WIDTH) - 1) << SCA_CSQ_DONE_SLOT_ID_SHIFT);


    t = (x >> SCA_CSQ_OPCODE_IDX_SHIFT) & ((1U << SCA_CSQ_OPCODE_IDX_WIDTH) - 1);
    BITFIELD_LOG(" opcode_idx=0x%x (the opcode index of done)\n", t);
    x &= ~(((1U << SCA_CSQ_OPCODE_IDX_WIDTH) - 1) << SCA_CSQ_OPCODE_IDX_SHIFT);


    t = (x >> SCA_CSQ_STAT_IDX_SHIFT) & ((1U << SCA_CSQ_STAT_IDX_WIDTH) - 1);
    BITFIELD_LOG(" stat_idx=0x%x (the status of done cmd)\n", t);
    x &= ~(((1U << SCA_CSQ_STAT_IDX_WIDTH) - 1) << SCA_CSQ_STAT_IDX_SHIFT);

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

#define log_sca_csq(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __SCA_CSQ_REG_H__ */
