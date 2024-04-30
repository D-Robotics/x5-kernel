//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/* \file lca_te_aead.h
 * Arm China Trust Engine aead Crypto API
 */

#ifndef __LCA_TE_AEAD_H__
#define __LCA_TE_AEAD_H__

#include <linux/kernel.h>
#include <crypto/algapi.h>
#include <crypto/ctr.h>


int lca_te_aead_alloc(struct te_drvdata *drvdata);
int lca_te_aead_free(struct te_drvdata *drvdata);

#endif /*__LCA_TE_AEAD_H__*/

