//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __HOST_REGS_H__
#define __HOST_REGS_H__

#include "host/aca_regs.h"
#include "host/sca_regs.h"
#include "host/hash_regs.h"
#include "host/trng_regs.h"
#include "host/trngctl_regs.h"
#include "host/dbgctl_regs.h"
#include "host/dma_regs.h"
#include "host/otpctl_regs.h"
#include "host/ctl_regs.h"

#define HOST_OFS_ACA              0x0000
#define HOST_OFS_SCA              0x0200
#define HOST_OFS_HASH             0x0280
#define HOST_OFS_TRNG             0x0300
#define HOST_OFS_TRNGCTL          0x0380
#define HOST_OFS_DBGCTL           0x0400
#define HOST_OFS_DMA              0x0500
#define HOST_OFS_OTPCTL           0x0600
#define HOST_OFS_CTL              0x0700
#define HOST_OFS_RSVD             0x0800

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_host register file definition.
 */
typedef struct te_host_regs {
    te_aca_regs_t aca;               /**< +0x000  */
    volatile uint32_t hole1[37];
    te_sca_regs_t sca;               /**< +0x200  */
    volatile uint32_t hole2[4];
    te_hash_regs_t hash;             /**< +0x280  */
    volatile uint32_t hole3[4];
    te_trng_regs_t trng;             /**< +0x300  */
    volatile uint32_t hole4[20];
    te_trngctl_regs_t trngctl;       /**< +0x380 host0 only */
    volatile uint32_t hole5[18];
    te_dbgctl_regs_t dbgctl;         /**< +0x400 host0 only */
    volatile uint32_t hole6[62];
    te_dma_regs_t dma;               /**< +0x500 host0 only */
    volatile uint32_t hole7[25];
    te_otpctl_regs_t otpctl;         /**< +0x600 host0 only */
    volatile uint32_t hole8[40];
    te_ctl_regs_t ctl;               /**< +0x700 host0 only */
    volatile uint32_t hole9[44];
    volatile uint32_t rsvd[512];     /**< +0x800  */
} te_host_regs_t;

#define HOST_REGS_SIZE             0x1000

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __HOST_REGS_H__ */
