//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/* \file lca_te_otp.h
 * Arm China Trust Engine OTP API
 */

#ifndef __LCA_TE_OTP_H__
#define __LCA_TE_OTP_H__

#include "lca_te_driver.h"



int lca_te_otp_alloc(struct te_drvdata *drvdata);
int lca_te_otp_free(struct te_drvdata *drvdata);
int lca_te_otp_read(size_t offset, uint8_t *buf, size_t len );
#endif /*__LCA_TE_OTP_H__*/

