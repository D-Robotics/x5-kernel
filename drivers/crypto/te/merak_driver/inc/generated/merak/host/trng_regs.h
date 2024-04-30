//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRNG_REGS_H__
#define __TRNG_REGS_H__

#include "trng/ctrl.h"
#include "trng/stat.h"
#include "trng/intr.h"
#include "trng/intr_msk.h"

#define TRNG_OFS_CTRL             0x0000
#define TRNG_OFS_STAT             0x0004
#define TRNG_OFS_INTR             0x0008
#define TRNG_OFS_INTR_MSK         0x000c
#define TRNG_OFS_DATA             0x0010

#define TRNG_REGS_SIZE            0x0030

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_trng module register file definition.
 */
typedef struct te_trng_regs {
    volatile trng_ctrlReg_t ctrl;    /**< +0x000 control */
    volatile trng_statReg_t stat;    /**< +0x004 state */
    volatile trng_intrReg_t intr;    /**< +0x008 interrupt state */
    volatile trng_intr_mskReg_t intr_msk; /**< +0x00c interrupt mask */
    volatile uint32_t data[8];       /**< +0x010 random data */
} te_trng_regs_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNG_REGS_H__ */
