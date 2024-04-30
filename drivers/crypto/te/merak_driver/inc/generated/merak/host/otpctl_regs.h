//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OTPCTL_REGS_H__
#define __OTPCTL_REGS_H__

#include "otpctl/setting.h"
#include "otpctl/otp_wr.h"
#include "otpctl/update_stat.h"
#include "otpctl/dummy_cfg.h"

#define OTPCTL_OFS_SETTING          0x0000
#define OTPCTL_OFS_WR_ADDR          0x0004
#define OTPCTL_OFS_WR_DATA          0x0008
#define OTPCTL_OFS_OTP_WR           0x000c
#define OTPCTL_OFS_UPDATE_STAT      0x0010
#define OTPCTL_OFS_DUMMY_CFG        0x001c
#define OTPCTL_OFS_SECKEYS          0x0020

#define OTPCTL_REGS_SIZE            0x0060

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_otpctl module register file definition.
 */
typedef struct te_otpctl_regs {
    volatile otpctl_settingReg_t setting; /**< +0x000 OTP cfg and status */
    volatile uint32_t wr_addr;       /**< +0x004 indirect write address */
    volatile uint32_t wr_data;       /**< +0x008  */
    volatile otpctl_otp_wrReg_t otp_wr; /**< +0x00c  */
    volatile otpctl_update_statReg_t update_stat; /**< +0x010  */
    volatile uint32_t hole5[2];
    volatile otpctl_dummy_cfgReg_t dummy_cfg; /**< +0x01c  */
    volatile uint32_t seckeys[16];   /**< +0x020 root key & model key */
} te_otpctl_regs_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __OTPCTL_REGS_H__ */
