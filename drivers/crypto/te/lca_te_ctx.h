//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/* \file lca_te_ctx.h
 * Arm China Trust Engine Linux Crypto Driver Context Manager
 */

#include "driver/te_drv.h"
#include "lca_te_driver.h"

#ifndef __LCA_TE_CTX_H__
#define __LCA_TE_CTX_H__

struct lca_te_drv_ctx_gov;

#define LCA_TE_DRV_OPS_TYPE_DEF(drv)                                           \
struct lca_te_##drv##_ops {                                                    \
	int (*init)(te_##drv##_ctx_t *ctx, te_drv_handle hdl, te_algo_t alg);  \
	int (*free)(te_##drv##_ctx_t *ctx);                                    \
	int (*clone)(const te_##drv##_ctx_t *src, te_##drv##_ctx_t *dst);      \
	int (*setkey)(te_##drv##_ctx_t *ctx, const u8 *key, u32 keybits);      \
	int (*reset)(te_##drv##_ctx_t *ctx);                                   \
}

#define LCA_TE_DRV_OPS_TYPE(drv)      struct lca_te_##drv##_ops

#define LCA_TE_DRV_OPS_DEF(drv, name) LCA_TE_DRV_OPS_TYPE_DEF(drv) name

/**
 * struct te_base_ctx_t - TE driver context generic representation
 */
typedef struct te_base_ctx {
	te_crypt_ctx_t *crypt;
} te_base_ctx_t;

/**
 * struct lca_te_drv_ops - Generic TE driver operations
 * \code
 * typedef struct lca_te_base_ops {
 * 	int (*init)(te_base_ctx_t *bctx, te_drv_handle hdl, te_algo_t malg);
 * 	int (*free)(te_base_ctx_t *bctx);
 * 	int (*clone)(const te_base_ctx_t *src, te_base_ctx_t *dst);
 * 	int (*setkey)(te_base_ctx_t *bctx, const u8 *key, u32 keybits);
 * 	int (*reset)(te_base_ctx_t *bctx);
 * } lca_te_base_ops_t;
 * \endcode
 *
 * @init: Initialize the driver context.
 * @free: De-initialize the driver context.
 * @clone: Clone driver context.
 */
typedef LCA_TE_DRV_OPS_TYPE_DEF(base) lca_te_base_ops_t;

/**
 * Allocate one driver context governor.
 * This function shall be called once on initializing the transformation object.
 */
struct lca_te_drv_ctx_gov *lca_te_ctx_alloc_gov(const lca_te_base_ops_t *ops,
						te_drv_handle hdl,
						te_algo_t alg,
						struct crypto_tfm *tfm);

/**
 * Free the specified driver context governor.
 * This function shall be called once on destroying the transformation object.
 */
void lca_te_ctx_free_gov(struct lca_te_drv_ctx_gov *gov);

/**
 * Acquire one driver context and link it with the 'req' on success.
 * The driver context will be reset to a state that is ready for another
 * init-update-final request sequences when the 'reset' is true.
 * The 'reset' is mainly for the ahash driver use.
 */
int lca_te_ctx_get(struct lca_te_drv_ctx_gov *gov, te_base_ctx_t **pbctx,
		   void *req, bool reset);

/**
 * Release one driver context.
 */
int lca_te_ctx_put(struct lca_te_drv_ctx_gov *gov, te_base_ctx_t *bctx);

/**
 * Set key for all driver contexts.
 */
int lca_te_ctx_setkey(struct lca_te_drv_ctx_gov *gov, const u8 *key, u32 klen);

#endif /* __LCA_TE_CTX_H__ */
