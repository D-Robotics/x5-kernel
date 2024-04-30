//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/* \file lca_te_hash.h
 * Arm China Trust Engine Hash Crypto API
 */

#ifndef __LCA_TE_HASH_H__
#define __LCA_TE_HASH_H__

#include "lca_te_driver.h"


#define TE_EXPORT_MAGIC 0xC2EE1070U

/*struct hash_state {
	uint32_t magic;
	union {
		struct te_hmac_ctx hctx;
		struct te_dgst_ctx dctx;
	};
};*/

int lca_te_hash_alloc(struct te_drvdata *drvdata);
int lca_te_hash_free(struct te_drvdata *drvdata);

#endif /*__LCA_TE_HASH_H__*/


