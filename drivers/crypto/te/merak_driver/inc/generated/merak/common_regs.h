//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __COMMON_REGS_H__
#define __COMMON_REGS_H__

#include "common/stat_regs.h"
#include "common/otp_regs.h"

#define COMMON_OFS_STAT             0x0000
#define COMMON_OFS_OTP              0x1000

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_common register file definition.
 */
typedef struct te_common_regs {
    te_stat_regs_t stat;             /**< +0x000  */
    volatile uint32_t hole1[968];
    te_otp_regs_t otp;               /**< +0x1000  */
    volatile uint32_t hole_end[2027];
} te_common_regs_t;

#define COMMON_REGS_SIZE             0x3000

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __COMMON_REGS_H__ */
