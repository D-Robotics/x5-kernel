//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __SCA_ERR_STAT4_REG_H__
#define __SCA_ERR_STAT4_REG_H__

#define SCA_ERR_STAT4_AXI_ERR_SLOT_ID_SHIFT 0
#define SCA_ERR_STAT4_AXI_ERR_SLOT_ID_WIDTH 5

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * sca_err_stat4 register definition.
 */
typedef union sca_err_stat4Reg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        axi_err_slot_id: 5,
        hole0: 27; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} sca_err_stat4Reg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_sca_err_stat4(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("sca_err_stat4: 0x%08x is\n", x);


    t = (x >> SCA_ERR_STAT4_AXI_ERR_SLOT_ID_SHIFT) & ((1U << SCA_ERR_STAT4_AXI_ERR_SLOT_ID_WIDTH) - 1);
    BITFIELD_LOG(" axi_err_slot_id=0x%x\n", t);
    x &= ~(((1U << SCA_ERR_STAT4_AXI_ERR_SLOT_ID_WIDTH) - 1) << SCA_ERR_STAT4_AXI_ERR_SLOT_ID_SHIFT);

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

#define log_sca_err_stat4(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __SCA_ERR_STAT4_REG_H__ */
