//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_REGS_H__
#define __STAT_REGS_H__

#include "stat/clock_status.h"
#include "stat/top_cfg0.h"
#include "stat/cq_sram_size.h"
#include "stat/top_cfg1.h"
#include "stat/top_stat.h"
#include "stat/ctx_sram.h"
#include "stat/intr_host.h"
#include "stat/cfg_host.h"
#include "stat/dma_fifo_depth.h"
#include "stat/dma_axi_stat.h"
#include "stat/otp_size0.h"
#include "stat/otp_size1.h"

#define STAT_OFS_CLOCK_STATUS     0x0000
#define STAT_OFS_VERSION          0x0004
#define STAT_OFS_TOP_CFG0         0x0008
#define STAT_OFS_CQ_SRAM_SIZE     0x000c
#define STAT_OFS_TOP_CFG1         0x0010
#define STAT_OFS_TOP_STAT         0x0014
#define STAT_OFS_CTX_SRAM         0x0018
#define STAT_OFS_HOST             0x0020
#define STAT_OFS_DMA_FIFO_DEPTH   0x00d0
#define STAT_OFS_DMA_AXI_STAT     0x00d4
#define STAT_OFS_OTP_SIZE0        0x00d8
#define STAT_OFS_OTP_SIZE1        0x00dc

#define STAT_REGS_SIZE            0x00e0

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_stat module register file definition.
 */
typedef struct te_stat_regs {
    volatile stat_clock_statusReg_t clock_status; /**< +0x000 sub-block clock status */
    volatile uint32_t version;       /**< +0x004  */
    volatile stat_top_cfg0Reg_t top_cfg0; /**< +0x008  */
    volatile stat_cq_sram_sizeReg_t cq_sram_size; /**< +0x00c  */
    volatile stat_top_cfg1Reg_t top_cfg1; /**< +0x010  */
    volatile stat_top_statReg_t top_stat; /**< +0x014  */
    volatile stat_ctx_sramReg_t ctx_sram; /**< +0x018  */
    volatile uint32_t hole7;
    struct {
        volatile stat_intr_hostReg_t intr_host;
        volatile stat_cfg_hostReg_t cfg_host;
    } host[16];                      /**< +0x020 host info */
    volatile uint32_t hole8[12];
    volatile stat_dma_fifo_depthReg_t dma_fifo_depth; /**< +0x0d0  */
    volatile stat_dma_axi_statReg_t dma_axi_stat; /**< +0x0d4  */
    volatile stat_otp_size0Reg_t otp_size0; /**< +0x0d8  */
    volatile stat_otp_size1Reg_t otp_size1; /**< +0x0dc  */
} te_stat_regs_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_REGS_H__ */
