//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __ACA_ENTRY_REG_H__
#define __ACA_ENTRY_REG_H__

#define ACA_ENTRY_OPCODE_SHIFT 27
#define ACA_ENTRY_OPCODE_WIDTH 5
#define ACA_ENTRY_LT_ID_SHIFT 23 /**< len_type_id */
#define ACA_ENTRY_LT_ID_WIDTH 4
#define ACA_ENTRY_OP_A_SHIFT 18
#define ACA_ENTRY_OP_A_WIDTH 5
#define ACA_ENTRY_OP_B_SHIFT 12 /**< MSB=1/0 means imm/GR */
#define ACA_ENTRY_OP_B_WIDTH 6
#define ACA_ENTRY_NOSAVE_SHIFT 11 /**< not save to r */
#define ACA_ENTRY_NOSAVE_WIDTH 1
#define ACA_ENTRY_OP_R_SHIFT 6
#define ACA_ENTRY_OP_R_WIDTH 5
#define ACA_ENTRY_OP_C_SHIFT 1
#define ACA_ENTRY_OP_C_WIDTH 5
#define ACA_ENTRY_NEED_INT_SHIFT 0
#define ACA_ENTRY_NEED_INT_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * aca_entry register definition.
 */
typedef union aca_entryReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        need_int: 1,
        op_c: 5,
        op_r: 5,
        nosave: 1  /**< not save to r */,
        op_b: 6  /**< MSB=1/0 means imm/GR */,
        op_a: 5,
        lt_id: 4  /**< len_type_id */,
        opcode: 5; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} aca_entryReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_aca_entry(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("aca_entry: 0x%08x is\n", x);


    t = (x >> ACA_ENTRY_OPCODE_SHIFT) & ((1U << ACA_ENTRY_OPCODE_WIDTH) - 1);
    BITFIELD_LOG(" opcode=0x%x\n", t);
    x &= ~(((1U << ACA_ENTRY_OPCODE_WIDTH) - 1) << ACA_ENTRY_OPCODE_SHIFT);


    t = (x >> ACA_ENTRY_LT_ID_SHIFT) & ((1U << ACA_ENTRY_LT_ID_WIDTH) - 1);
    BITFIELD_LOG(" lt_id=0x%x (len_type_id)\n", t);
    x &= ~(((1U << ACA_ENTRY_LT_ID_WIDTH) - 1) << ACA_ENTRY_LT_ID_SHIFT);


    t = (x >> ACA_ENTRY_OP_A_SHIFT) & ((1U << ACA_ENTRY_OP_A_WIDTH) - 1);
    BITFIELD_LOG(" op_a=0x%x\n", t);
    x &= ~(((1U << ACA_ENTRY_OP_A_WIDTH) - 1) << ACA_ENTRY_OP_A_SHIFT);


    t = (x >> ACA_ENTRY_OP_B_SHIFT) & ((1U << ACA_ENTRY_OP_B_WIDTH) - 1);
    BITFIELD_LOG(" op_b=0x%x (MSB=1/0 means imm/GR)\n", t);
    x &= ~(((1U << ACA_ENTRY_OP_B_WIDTH) - 1) << ACA_ENTRY_OP_B_SHIFT);


    t = (x >> ACA_ENTRY_NOSAVE_SHIFT) & ((1U << ACA_ENTRY_NOSAVE_WIDTH) - 1);
    BITFIELD_LOG(" nosave=0x%x (not save to r)\n", t);
    x &= ~(((1U << ACA_ENTRY_NOSAVE_WIDTH) - 1) << ACA_ENTRY_NOSAVE_SHIFT);


    t = (x >> ACA_ENTRY_OP_R_SHIFT) & ((1U << ACA_ENTRY_OP_R_WIDTH) - 1);
    BITFIELD_LOG(" op_r=0x%x\n", t);
    x &= ~(((1U << ACA_ENTRY_OP_R_WIDTH) - 1) << ACA_ENTRY_OP_R_SHIFT);


    t = (x >> ACA_ENTRY_OP_C_SHIFT) & ((1U << ACA_ENTRY_OP_C_WIDTH) - 1);
    BITFIELD_LOG(" op_c=0x%x\n", t);
    x &= ~(((1U << ACA_ENTRY_OP_C_WIDTH) - 1) << ACA_ENTRY_OP_C_SHIFT);


    t = (x >> ACA_ENTRY_NEED_INT_SHIFT) & ((1U << ACA_ENTRY_NEED_INT_WIDTH) - 1);
    BITFIELD_LOG(" need_int=0x%x\n", t);
    x &= ~(((1U << ACA_ENTRY_NEED_INT_WIDTH) - 1) << ACA_ENTRY_NEED_INT_SHIFT);

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

#define log_aca_entry(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __ACA_ENTRY_REG_H__ */
