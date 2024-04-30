//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/* \file lca_te_akcipher.h
 * Arm China Trust Engine akcipher Crypto API
 */

#ifndef __LCA_TE_AKCIPHER_H__
#define __LCA_TE_AKCIPHER_H__

#include <linux/kernel.h>
#include <crypto/algapi.h>
#include <crypto/ctr.h>


int lca_te_akcipher_alloc(struct te_drvdata *drvdata);
int lca_te_akcipher_free(struct te_drvdata *drvdata);

#endif /*__LCA_TE_AKCIPHER_H__*/


