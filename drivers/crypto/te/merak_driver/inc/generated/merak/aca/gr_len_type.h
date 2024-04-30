//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __ACA_GR_LEN_TYPE_REG_H__
#define __ACA_GR_LEN_TYPE_REG_H__

#define ACA_GR_LEN_TYPE_LEN_SHIFT 0 /**< in unit of bits */
#define ACA_GR_LEN_TYPE_LEN_WIDTH 14

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * aca_gr_len_type register definition.
 */
typedef union aca_gr_len_typeReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        len: 14  /**< in unit of bits */,
        hole0: 18; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} aca_gr_len_typeReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_aca_gr_len_type(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("aca_gr_len_type: 0x%08x is\n", x);


    t = (x >> ACA_GR_LEN_TYPE_LEN_SHIFT) & ((1U << ACA_GR_LEN_TYPE_LEN_WIDTH) - 1);
    BITFIELD_LOG(" len=0x%x (in unit of bits)\n", t);
    x &= ~(((1U << ACA_GR_LEN_TYPE_LEN_WIDTH) - 1) << ACA_GR_LEN_TYPE_LEN_SHIFT);

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

#define log_aca_gr_len_type(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __ACA_GR_LEN_TYPE_REG_H__ */
