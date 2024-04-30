//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __ACA_REGS_H__
#define __ACA_REGS_H__

#include "aca/gr_sram_addr.h"
#include "aca/gr_len_type.h"
#include "aca/use_grid.h"
#include "aca/ctrl.h"
#include "aca/entry.h"
#include "aca/status.h"
#include "aca/intr_stat.h"
#include "aca/intr_msk.h"
#include "aca/suspd_msk.h"

#define ACA_OFS_GR_SRAM_ADDR     0x0000
#define ACA_OFS_GR_LEN_TYPE      0x0100
#define ACA_OFS_USE_GRID         0x0140
#define ACA_OFS_CTRL             0x0144
#define ACA_OFS_ENTRY            0x0148
#define ACA_OFS_STATUS           0x014c
#define ACA_OFS_SRAM_WADDR       0x0150
#define ACA_OFS_SRAM_WDATA       0x0154
#define ACA_OFS_SRAM_RADDR       0x0158
#define ACA_OFS_SRAM_RDATA       0x015c
#define ACA_OFS_INTR_STAT        0x0160
#define ACA_OFS_INTR_MSK         0x0164
#define ACA_OFS_SUSPD_MSK        0x0168

#define ACA_REGS_SIZE            0x016c

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_aca module register file definition.
 */
typedef struct te_aca_regs {
    volatile aca_gr_sram_addrReg_t gr_sram_addr[32]; /**< +0x000 GR SRAM addr */
    volatile uint32_t hole1[32];
    volatile aca_gr_len_typeReg_t gr_len_type[16]; /**< +0x100 GR length type */
    volatile aca_use_gridReg_t use_grid; /**< +0x140 N/P/T0/T1 GR ID */
    volatile aca_ctrlReg_t ctrl;     /**< +0x144  */
    volatile aca_entryReg_t entry;   /**< +0x148  */
    volatile aca_statusReg_t status; /**< +0x14c ACA status */
    volatile uint32_t sram_waddr;    /**< +0x150  */
    volatile uint32_t sram_wdata;    /**< +0x154  */
    volatile uint32_t sram_raddr;    /**< +0x158  */
    volatile uint32_t sram_rdata;    /**< +0x15c  */
    volatile aca_intr_statReg_t intr_stat; /**< +0x160  */
    volatile aca_intr_mskReg_t intr_msk; /**< +0x164  */
    volatile aca_suspd_mskReg_t suspd_msk; /**< +0x168 mask engine suspension on intr */
} te_aca_regs_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __ACA_REGS_H__ */
