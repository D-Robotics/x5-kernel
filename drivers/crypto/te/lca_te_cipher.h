//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/* \file lca_te_cipher.h
 * Arm China Trust Engine Cipher Crypto API
 */

#ifndef __LCA_TE_CIPHER_H__
#define __LCA_TE_CIPHER_H__


#include "lca_te_driver.h"




int lca_te_cipher_free(struct te_drvdata *drvdata);
int lca_te_cipher_alloc(struct te_drvdata *drvdata);


#endif /*__LCA_TE_CIPHER_H__*/


