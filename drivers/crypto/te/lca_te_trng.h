//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/* \file lca_te_otp.h
 * Arm China Trust Engine TRNG API
 */

#ifndef __LCA_TE_TRNG_H__
#define __LCA_TE_TRNG_H__

#include "lca_te_driver.h"



int lca_te_trng_alloc(struct te_drvdata *drvdata);
int lca_te_trng_free(struct te_drvdata *drvdata);

#endif /*__LCA_TE_TRNG_H__*/


