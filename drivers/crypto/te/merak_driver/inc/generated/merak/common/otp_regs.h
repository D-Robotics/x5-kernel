//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OTP_REGS_H__
#define __OTP_REGS_H__


#define OTP_OFS_MID              0x0000
#define OTP_OFS_MODK             0x0004
#define OTP_OFS_DID              0x0014
#define OTP_OFS_ROOTK            0x0018
#define OTP_OFS_ROTPK_HASH       0x0028
#define OTP_OFS_LCS              0x0048
#define OTP_OFS_LOCK_CTRL        0x004c
#define OTP_OFS_UD_RGN           0x0050

#define OTP_REGS_SIZE            0x0054

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_otp module register file definition.
 */
typedef struct te_otp_regs {
    volatile uint32_t mid;           /**< +0x000 model ID */
    volatile uint32_t modk[4];       /**< +0x004 model key */
    volatile uint32_t did;           /**< +0x014 device ID */
    volatile uint32_t rootk[4];      /**< +0x018 device root key */
    volatile uint32_t rotpk_hash[8]; /**< +0x028 secure boot pk hash */
    volatile uint32_t lcs;           /**< +0x048 life cycle state */
    volatile uint32_t lock_ctrl;     /**< +0x04c  */
    volatile uint32_t ud_rgn;        /**< +0x050 usr defined region, inc. nsec,sec,and test rgns */
} te_otp_regs_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __OTP_REGS_H__ */
