//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __ACA_USE_GRID_REG_H__
#define __ACA_USE_GRID_REG_H__

#define ACA_USE_GRID_T1_GRID_SHIFT 15
#define ACA_USE_GRID_T1_GRID_WIDTH 5
#define ACA_USE_GRID_T0_GRID_SHIFT 10
#define ACA_USE_GRID_T0_GRID_WIDTH 5
#define ACA_USE_GRID_P_GRID_SHIFT 5
#define ACA_USE_GRID_P_GRID_WIDTH 5
#define ACA_USE_GRID_N_GRID_SHIFT 0
#define ACA_USE_GRID_N_GRID_WIDTH 5

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * aca_use_grid register definition.
 */
typedef union aca_use_gridReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        n_grid: 5,
        p_grid: 5,
        t0_grid: 5,
        t1_grid: 5,
        hole0: 12; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} aca_use_gridReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_aca_use_grid(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("aca_use_grid: 0x%08x is\n", x);


    t = (x >> ACA_USE_GRID_T1_GRID_SHIFT) & ((1U << ACA_USE_GRID_T1_GRID_WIDTH) - 1);
    BITFIELD_LOG(" t1_grid=0x%x\n", t);
    x &= ~(((1U << ACA_USE_GRID_T1_GRID_WIDTH) - 1) << ACA_USE_GRID_T1_GRID_SHIFT);


    t = (x >> ACA_USE_GRID_T0_GRID_SHIFT) & ((1U << ACA_USE_GRID_T0_GRID_WIDTH) - 1);
    BITFIELD_LOG(" t0_grid=0x%x\n", t);
    x &= ~(((1U << ACA_USE_GRID_T0_GRID_WIDTH) - 1) << ACA_USE_GRID_T0_GRID_SHIFT);


    t = (x >> ACA_USE_GRID_P_GRID_SHIFT) & ((1U << ACA_USE_GRID_P_GRID_WIDTH) - 1);
    BITFIELD_LOG(" p_grid=0x%x\n", t);
    x &= ~(((1U << ACA_USE_GRID_P_GRID_WIDTH) - 1) << ACA_USE_GRID_P_GRID_SHIFT);


    t = (x >> ACA_USE_GRID_N_GRID_SHIFT) & ((1U << ACA_USE_GRID_N_GRID_WIDTH) - 1);
    BITFIELD_LOG(" n_grid=0x%x\n", t);
    x &= ~(((1U << ACA_USE_GRID_N_GRID_WIDTH) - 1) << ACA_USE_GRID_N_GRID_SHIFT);

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

#define log_aca_use_grid(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __ACA_USE_GRID_REG_H__ */
