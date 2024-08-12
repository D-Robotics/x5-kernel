//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#define pr_fmt(fmt) "te_crypt: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <crypto/algapi.h>
#include <crypto/hash.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#include <crypto/md5.h>
#include <crypto/internal/hash.h>

#include "te_hmac.h"
#include "te_cmac.h"
#include "te_hash.h"

#include "lca_te_hash.h"
#include "lca_te_buf_mgr.h"
#include "lca_te_ctx.h"
#include "lca_te_ra.h"

/**
 * AHASH Driver Context Management Introduction.
 *
 * The ahash driver is optimized to promote parallel operation performance for
 * single-tfm-multi-thread and multi-tfm-multi-thread scenarios. The ideas are:
 * - One driver ctx for each request.
 * - The init(), update(), final() request makes use of driver ctx from the
 *   driver context manager. See 'lca_te_ctx.c' for details.
 * - The digest() request lives with the the all-in-one async interface from
 *   the driver, where a new driver context is created on the fly.
 * - Attach-and-reset a driver ctx on entry to init().
 * - Attach-and-import a driver ctx on entry to update() and final().
 * - Export-and-detach the driver ctx on completion of init() and update().
 * - Detach the driver ctx on completion of final().
 * - Detach the driver ctx on init() or update() or final() error.
 * - Submit request by way of the request agent except digest().
 *
 *  +---------------------------------------------------+      \
 *  |          transformation object (tfm)              | ...  |
 *  +---------------------------------------------------+      |
 *     |       |           |            |            |          > LCA DRV
 *  +-----+ +-----+     +-----+      +-----+      +-----+      |
 *  |req#0| |req#1| ... |req#m|      |req#k| ...  |req#p|      |
 *  +-----+ +-----+     +-----+      +-----+      +-----+      /
 *     |       |           |            |            |
 *     +-------+----...----+            |            |         \
 *     |       |           |            |            |         |
 *  +-----+ +-----+     +-----+      +-----+      +-----+       > TE DRV
 *  |ctx0 | |ctx1 | ... |ctxn |      |ctx#r| ...  |ctx#s|      |
 *  +-----+ +-----+     +-----+      +-----+      +-----+      /
 *  \                         /      \                  /
 *   `-----------v-----------'        `-------v--------'
 *    init(),update(),final()              digest()
 *
 */

#ifndef SM3_DIGEST_SIZE
#define SM3_DIGEST_SIZE (32)
#endif

#ifndef SM3_BLOCK_SIZE
#define SM3_BLOCK_SIZE  (64)
#endif
#ifndef MD5_BLOCK_SIZE
#define MD5_BLOCK_SIZE  (64)
#endif

#define LCA_TE_TYPE_INVAL    0x0
#define LCA_TE_TYPE_HASH     0x1
#define LCA_TE_TYPE_HMAC     0x2
#define LCA_TE_TYPE_CMAC     0x3
#define LCA_TE_TYPE_CBCMAC   0x4

#define TE_MAX_STAT_SZ    512
#define CBCMAC_MAX_IV_SZ  TE_MAX_SCA_BLOCK

struct te_hash_handle {
	lca_te_ra_handle ra;
	struct list_head hash_list;
};

struct te_hash_template {
	char name[CRYPTO_MAX_ALG_NAME];
	char driver_name[CRYPTO_MAX_ALG_NAME];
	char mac_name[CRYPTO_MAX_ALG_NAME];
	char mac_driver_name[CRYPTO_MAX_ALG_NAME];
	unsigned int blocksize;
	struct ahash_alg template_ahash;
	te_algo_t alg;
	bool ishash;
	struct te_drvdata *drvdata;
};

struct te_hash_alg {
	struct list_head entry;
	te_algo_t alg;
	struct te_drvdata *drvdata;
	struct ahash_alg ahash_alg;
};

/* Context associated with one transformation object, in heap, zeroized */
struct te_hash_ctx {
	struct te_drvdata *drvdata;
	struct lca_te_drv_ctx_gov *cgov; /**< Driver context governor */
	te_algo_t alg;
	unsigned int blocksize;
	u8 *mackey;
	unsigned int keylen;
};

/* Context associated with one request object, in heap/stack, un-initialized */
struct te_ahash_req_ctx {
	struct te_request base;          /**< Must put it in the beginning */
	union {
		te_hmac_ctx_t *hctx;
		te_dgst_ctx_t *dctx;
		te_cmac_ctx_t *cctx;
		te_cbcmac_ctx_t *cbctx;
		te_base_ctx_t *bctx;
	};
	union {
		te_dgst_request_t dgst_req;
		te_hmac_request_t hmac_req;
		te_cmac_request_t cmac_req; /**< CMAC and CBCMAC request */
	};
	u8 iv[CBCMAC_MAX_IV_SZ];        /**< Zero IV for CBCMAC use only */
	void *priv[];
};

static int lca_te_hash_type(te_algo_t alg)
{
	int type = LCA_TE_TYPE_INVAL;

	switch (TE_ALG_GET_CLASS(alg)) {
	case TE_OPERATION_DIGEST:
		type = LCA_TE_TYPE_HASH;
		break;
	case TE_OPERATION_MAC:
		if (TE_CHAIN_MODE_CBC_NOPAD == TE_ALG_GET_CHAIN_MODE(alg)) {
			type = LCA_TE_TYPE_CBCMAC;
		} else if (TE_CHAIN_MODE_CMAC == TE_ALG_GET_CHAIN_MODE(alg)) {
			type = LCA_TE_TYPE_CMAC;
		} else if (0U == TE_ALG_GET_CHAIN_MODE(alg)) {
			type = LCA_TE_TYPE_HMAC;
		}
		break;
	default:
		break;
	}

	return type;
}

static int lca_te_cmac_size(te_algo_t alg, unsigned int *macsz)
{
	int rc = TE_SUCCESS;

	switch (TE_ALG_GET_MAIN_ALG(alg)) {
	case TE_MAIN_ALGO_AES:
		*macsz = TE_AES_BLOCK_SIZE;
		break;
	case TE_MAIN_ALGO_DES:
	case TE_MAIN_ALGO_TDES:
		*macsz = TE_DES_BLOCK_SIZE;
		break;
	case TE_MAIN_ALGO_SM4:
		*macsz = TE_SM4_BLOCK_SIZE;
		break;
	default:
		printk("unknown cipher MAC algo (0x%x)\n", alg);
		rc = TE_ERROR_BAD_PARAMS;
		break;
	}

	return rc;
}

/**
 * te_cmac_init() fallback: initialize a cmac ctx.
 *
 * The te_cmac_init() has the following drawback:
 * D1: accept main algorithm only.
 */
TE_DRV_INIT_FALLBACK(cmac)

/**
 * te_cbcmac_init() fallback: initialize a cbcmac ctx.
 *
 * The te_cbcmac_init() has the following drawback:
 * D1: accept main algorithm only.
 */
TE_DRV_INIT_FALLBACK(cbcmac)

/**
 * dgst/hmac/cmac/cbcmac driver reset function drawback:
 * D1: The driver context is reset to START state.
 * D2: The te_cbcmac_reset() and te_dgst_reset() doesn't support resetting
 *     non-started driver contexts.
 *
 * It is desired from the .reset function to reset one driver context to a
 * state that is ready for another init/update/final call sequences.
 *
 * However, the .reset() function of the existing drivers includes
 * an extra .start() effect. See the textual diagram below.
 *
 * \code
 *          .- .reset() <-.
 *          v             |            ! .reset() can be called to
 * .start() -> .update() -> .finish()  recover an erroneous ctx.
 *              ^    |
 *              '----'
 * \endcode
 *
 * The above .reset() effect is not desired to the LCA ahash driver.
 * To mitigate that effect, an extra .finish() needs to be done
 * right after the .reset().
 */

/**
 * te_dgst_reset() fallback: reset a hash driver ctx to the INIT state.
 * \code
 * .export() -> .reset() -> .finish() -> Done
 *     |                                  ^
 *     '----------------------------------'
 *          err = TE_ERROR_BAD_STATE
 *
 * \endcode
 */
static int lca_te_dgst_reset(te_dgst_ctx_t *ctx)
{
	int rc = TE_ERROR_BAD_PARAMS;
	unsigned char tmp[TE_MAX_HASH_SIZE];
	unsigned int olen = 0;

	if ((NULL == ctx) || (NULL == ctx->crypt)) {
		goto out;
	}

	/*
	 * The hash driver ctx can only be in one of the below states in LCA:
	 *    not-started (TE_DRV_HASH_STATE_INIT)
	 *    started     (TE_DRV_HASH_STATE_START)
	 *    updated     (TE_DRV_HASH_STATE_UPDATE, TE_DRV_HASH_STATE_LAST)
	 *
	 * Here is an idea to ping the driver ctx state by:
	 *     te_dgst_export(ctx, NULL, &olen);
	 *
	 * Firstly, the cost of te_dgst_export(ctx, NULL, &olen) is very low.
	 * Secondly, the te_dgst_export() will report TE_ERROR_BAD_STATE if
	 * the driver ctx is not in any of the following states:
	 *    TE_DRV_HASH_STATE_START
	 *    TE_DRV_HASH_STATE_UPDATE
	 *    TE_DRV_HASH_STATE_LAST
	 * Otherwise TE_ERROR_SHORT_BUFFER will be returned.
	 *
	 * The TE_DRV_HASH_STATE_INIT is really desired after the reset
	 * operation. So, we are happy to bypass the te_dgst_reset() if
	 * TE_ERROR_BAD_STATE is received from te_dgst_export().
	 */
	rc = te_dgst_export(ctx, NULL, &olen);
	if (TE_ERROR_BAD_STATE == rc) {
		rc = TE_SUCCESS;
		goto out;
	} else if (rc != TE_ERROR_SHORT_BUFFER) {
		pr_err("te_dgst_export algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

	rc = te_dgst_reset(ctx);
	if (rc != TE_SUCCESS) {
		pr_err("te_dgst_reset algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

	/* Do a finish() to undo the start() effect from the driver reset() */
	rc = te_dgst_finish(ctx, tmp);
	if (rc != TE_SUCCESS) {
		pr_err("te_dgst_finish algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

out:
	return rc;
}

/**
 * te_hmac_reset() fallback: reset a hmac driver ctx to the INIT state.
 * \code
 * .export() -> .reset() -> .finish() -> Done
 *     |                                  ^
 *     '----------------------------------'
 *          err = TE_ERROR_BAD_STATE
 *
 * \endcode
 */
static int lca_te_hmac_reset(te_hmac_ctx_t *ctx)
{
	int rc = TE_ERROR_BAD_PARAMS;
	unsigned int olen = 0;

	if ((NULL == ctx) || (NULL == ctx->crypt)) {
		goto out;
	}

	/*
	 * The hash driver ctx can only be in one of the below states in LCA:
	 *    not-started (TE_DRV_HASH_STATE_INIT)
	 *    started     (TE_DRV_HASH_STATE_START)
	 *    updated     (TE_DRV_HASH_STATE_UPDATE, TE_DRV_HASH_STATE_LAST)
	 *
	 * Here is an idea to ping the driver ctx state by:
	 *     te_hmac_export(ctx, NULL, &olen);
	 *
	 * Firstly, the cost of te_hmac_export(ctx, NULL, &olen) is very low.
	 * Secondly, the te_hmac_export() will report TE_ERROR_BAD_STATE if
	 * the driver ctx is not in any of the following states:
	 *    TE_DRV_HASH_STATE_START
	 *    TE_DRV_HASH_STATE_UPDATE
	 *    TE_DRV_HASH_STATE_LAST
	 * Otherwise TE_ERROR_SHORT_BUFFER will be returned.
	 *
	 * The TE_DRV_HASH_STATE_INIT is really desired after the reset
	 * operation. So, we are happy to bypass the te_hmac_reset() if
	 * TE_ERROR_BAD_STATE is received from te_hmac_export().
	 */
	rc = te_hmac_export(ctx, NULL, &olen);
	if (TE_ERROR_BAD_STATE == rc) {
		rc = TE_SUCCESS;
		goto out;
	} else if (rc != TE_ERROR_SHORT_BUFFER) {
		pr_err("te_hmac_export algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

	rc = te_hmac_reset(ctx);
	if (rc != TE_SUCCESS) {
		pr_err("te_hmac_reset algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

	/* Do a finish() to undo the start() effect from the driver reset() */
	rc = te_hmac_finish(ctx, NULL, 0);
	if (rc != TE_SUCCESS) {
		pr_err("te_hmac_finish algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

out:
	return rc;
}

/**
 * te_cmac_reset() fallback: reset a cmac driver ctx to the READY state.
 * \code
 * .export() -> .reset() -> .finish() -> Done
 *     |                                  ^
 *     '----------------------------------'
 *          err = TE_ERROR_BAD_STATE
 *
 * \endcode
 */
static int lca_te_cmac_reset(te_cmac_ctx_t *ctx)
{
	int rc = TE_ERROR_BAD_PARAMS;
	unsigned char tmp[TE_MAX_SCA_BLOCK];
	unsigned int len = 0;

	if ((NULL == ctx) || (NULL == ctx->crypt)) {
		goto out;
	}

	/*
	 * The sca driver ctx can only be in one of the below states in LCA:
	 *    not-started (TE_DRV_SCA_STATE_READY)
	 *    started     (TE_DRV_SCA_STATE_START)
	 *    updated     (TE_DRV_SCA_STATE_UPDATE, TE_DRV_SCA_STATE_LAST)
	 *
	 * Here is an idea to ping the driver ctx state by:
	 *     te_cmac_export(ctx, NULL, &olen);
	 *
	 * Firstly, the cost of te_cmac_export(ctx, NULL, &olen) is very low.
	 * Secondly, the te_cmac_export() will report TE_ERROR_BAD_STATE if
	 * the driver ctx is not in any of the following states:
	 *    TE_DRV_SCA_STATE_START
	 *    TE_DRV_SCA_STATE_UPDATE
	 *    TE_DRV_SCA_STATE_LAST
	 * Otherwise TE_ERROR_SHORT_BUFFER will be returned.
	 *
	 * The TE_DRV_SCA_STATE_READY is really desired after the reset
	 * operation. So, we are happy to bypass the te_cmac_reset() if
	 * TE_ERROR_BAD_STATE is received from te_cmac_export().
	 */
	rc = te_cmac_export(ctx, NULL, &len);
	if (TE_ERROR_BAD_STATE == rc) {
		rc = TE_SUCCESS;
		goto out;
	} else if (rc != TE_ERROR_SHORT_BUFFER) {
		pr_err("te_cmac_export algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

	rc = te_cmac_reset(ctx);
	if (rc != TE_SUCCESS) {
		pr_err("te_cmac_reset algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

	/* Do a finish() to undo the start() effect from the driver reset() */
	rc = lca_te_cmac_size(ctx->crypt->alg, &len);
	if (rc != TE_SUCCESS) {
		goto out;
	}

	rc = te_cmac_finish(ctx, tmp, len);
	if (rc != TE_SUCCESS) {
		pr_err("te_cmac_finish algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

out:
	return rc;
}

/**
 * te_cbcmac_reset() fallback: reset a cbcmac driver ctx to the READY state.
 * \code
 * .export() -> .reset() -> .finish() -> Done
 *     |                                  ^
 *     '----------------------------------'
 *          err = TE_ERROR_BAD_STATE
 *
 * \endcode
 */
static int lca_te_cbcmac_reset(te_cbcmac_ctx_t *ctx)
{
	int rc = TE_ERROR_BAD_PARAMS;
	unsigned char tmp[TE_MAX_SCA_BLOCK];
	unsigned int len = 0;

	if ((NULL == ctx) || (NULL == ctx->crypt)) {
		goto out;
	}

	/*
	 * The sca driver ctx can only be in one of the below states in LCA:
	 *    not-started (TE_DRV_SCA_STATE_READY)
	 *    started     (TE_DRV_SCA_STATE_START)
	 *    updated     (TE_DRV_SCA_STATE_UPDATE, TE_DRV_SCA_STATE_LAST)
	 *
	 * Here is an idea to ping the driver ctx state by:
	 *     te_cbcmac_export(ctx, NULL, &olen);
	 *
	 * Firstly, the cost of te_cbcmac_export(ctx, NULL, &olen) is very low.
	 * Secondly, the te_cbcmac_export() will report TE_ERROR_BAD_STATE if
	 * the driver ctx is not in any of the following states:
	 *    TE_DRV_SCA_STATE_START
	 *    TE_DRV_SCA_STATE_UPDATE
	 *    TE_DRV_SCA_STATE_LAST
	 * Otherwise TE_ERROR_SHORT_BUFFER will be returned.
	 *
	 * The TE_DRV_SCA_STATE_READY is really desired after the reset
	 * operation. So, we are happy to bypass the te_cbcmac_reset() if
	 * TE_ERROR_BAD_STATE is received from te_cbcmac_export().
	 */
	rc = te_cbcmac_export(ctx, NULL, &len);
	if (TE_ERROR_BAD_STATE == rc) {
		rc = TE_SUCCESS;
		goto out;
	} else if (rc != TE_ERROR_SHORT_BUFFER) {
		pr_err("te_cbcmac_export algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

	rc = te_cbcmac_reset(ctx);
	if (rc != TE_SUCCESS) {
		pr_err("te_cbcmac_reset algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

	/* Do a finish() to undo the start() effect from the driver reset() */
	rc = lca_te_cmac_size(ctx->crypt->alg, &len);
	if (rc != TE_SUCCESS) {
		goto out;
	}

	rc = te_cbcmac_finish(ctx, tmp, len);
	if (rc != TE_SUCCESS) {
		pr_err("te_cbcmac_finish algo (0x%x) ret:0x%x\n",
			ctx->crypt->alg, rc);
		goto out;
	}

out:
	return rc;
}

/**
 * te_dgst_clone() fallback: clone the src dgst ctx and have the dst ctx
 * ready for use if TE_SUCCESS.
 *
 * The te_dgst_clone() has the following drawbacks:
 * D1: the src ctx is not qualified by const.
 * D2: returns TE_SUCCESS without doing anything if the src ctx is not
 *     initialized. In other words, the dst ctx is not initialized in that
 *     case.
 * D3: the te_hash_clone() does not check the validity of the src sess_id
 *     before clone. As a result, TE_ERROR_GENERIC is returned if the src
 *     ctx is in TE_DRV_HASH_STATE_INIT state.
 */
static int lca_te_dgst_clone(const te_dgst_ctx_t *src, te_dgst_ctx_t *dst)
{
	int rc = TE_ERROR_BAD_PARAMS;
	unsigned int olen = 0;

	if ((NULL == src) || (NULL == src->crypt) || (NULL == dst)) {
		goto out;
	}

	/*
	 * The hash driver ctx can only be in one of the below states in LCA:
	 *    not-started (TE_DRV_HASH_STATE_INIT)
	 *    started     (TE_DRV_HASH_STATE_START)
	 *    updated     (TE_DRV_HASH_STATE_UPDATE, TE_DRV_HASH_STATE_LAST)
	 *
	 * Here is an idea to ping the driver ctx state by:
	 *     te_dgst_export(ctx, NULL, &olen);
	 *
	 * Firstly, the cost of te_dgst_export(ctx, NULL, &olen) is very low.
	 * Secondly, the te_dgst_export() will report TE_ERROR_BAD_STATE if
	 * the driver ctx is not in any of the following states:
	 *    TE_DRV_HASH_STATE_START
	 *    TE_DRV_HASH_STATE_UPDATE
	 *    TE_DRV_HASH_STATE_LAST
	 * Otherwise TE_ERROR_SHORT_BUFFER will be returned.
	 *
	 * Just reset the dst ctx if the src ctx is in TE_DRV_HASH_STATE_INIT
	 * state. Otherwise, pass down the clone request to the driver.
	 */
	rc = te_dgst_export((te_dgst_ctx_t *)src, NULL, &olen);
	if (TE_ERROR_BAD_STATE == rc) {
		/* Reset the dst ctx to the INIT state */
		rc = lca_te_dgst_reset(dst);
		if (rc != TE_SUCCESS) {
			pr_err("lca_te_dgst_reset algo (0x%x) ret:0x%x\n",
				src->crypt->alg, rc);
		}

		goto out; /* We are done anyway */
	} else if (rc != TE_ERROR_SHORT_BUFFER) {
		pr_err("te_dgst_export algo (0x%x) ret:0x%x\n",
			src->crypt->alg, rc);
		goto out;
	}

	/* Clone the src ctx */
	rc = te_dgst_clone((te_dgst_ctx_t *)src, dst);
	if (rc != TE_SUCCESS) {
		pr_err("te_dgst_clone algo (0x%x) ret:0x%x\n",
			src->crypt->alg, rc);
		goto out;
	}

out:
	return rc;
}

/**
 * te_hmac_clone() fallback: clone the src hmac ctx and have the dst ctx
 * ready for use if TE_SUCCESS.
 *
 * The te_hmac_clone() has the following drawback:
 * D1: the src ctx is not qualified by const.
 * D2: returns TE_SUCCESS without doing anything if the src ctx is in
 *     TE_DRV_HASH_STATE_INIT state. In other words, the dst ctx is not
 *     initialized in that case.
 */
static int lca_te_hmac_clone(const te_hmac_ctx_t *src, te_hmac_ctx_t *dst)
{
	int rc = TE_ERROR_BAD_PARAMS;
	unsigned int olen = 0;

	if ((NULL == src) || (NULL == src->crypt) || (NULL == dst)) {
		goto out;
	}

	/*
	 * The hash driver ctx can only be in one of the below states in LCA:
	 *    not-started (TE_DRV_HASH_STATE_INIT)
	 *    started     (TE_DRV_HASH_STATE_START)
	 *    updated     (TE_DRV_HASH_STATE_UPDATE, TE_DRV_HASH_STATE_LAST)
	 *
	 * Here is an idea to ping the driver ctx state by:
	 *     te_hmac_export(ctx, NULL, &olen);
	 *
	 * Firstly, the cost of te_hmac_export(ctx, NULL, &olen) is very low.
	 * Secondly, the te_hmac_export() will report TE_ERROR_BAD_STATE if
	 * the driver ctx is not in any of the following states:
	 *    TE_DRV_HASH_STATE_START
	 *    TE_DRV_HASH_STATE_UPDATE
	 *    TE_DRV_HASH_STATE_LAST
	 * Otherwise TE_ERROR_SHORT_BUFFER will be returned.
	 *
	 * Just reset the dst ctx if the src ctx is in TE_DRV_HASH_STATE_INIT
	 * state. Otherwise, pass down the clone request to the driver.
	 */
	rc = te_hmac_export((te_hmac_ctx_t *)src, NULL, &olen);
	if (TE_ERROR_BAD_STATE == rc) {
		/* Reset the dst ctx to the INIT state */
		rc = lca_te_hmac_reset(dst);
		if (rc != TE_SUCCESS) {
			pr_err("lca_te_hmac_reset algo (0x%x) ret:0x%x\n",
				src->crypt->alg, rc);
		}

		goto out; /* We are done anyway */
	} else if (rc != TE_ERROR_SHORT_BUFFER) {
		pr_err("te_hmac_export algo (0x%x) ret:0x%x\n",
			src->crypt->alg, rc);
		goto out;
	}

	/* Clone the src ctx */
	rc = te_hmac_clone((te_hmac_ctx_t *)src, dst);
	if (rc != TE_SUCCESS) {
		pr_err("te_hmac_clone algo (0x%x) ret:0x%x\n",
			src->crypt->alg, rc);
		goto out;
	}

out:
	return rc;
}

/**
 * Attach a driver context and import the request's partial state to it.
 */
static int lca_te_attach_import(struct te_ahash_req_ctx *areq_ctx)
{
	int rc = 0;
	struct ahash_request *req =
		container_of((void*(*)[])areq_ctx, struct ahash_request, __ctx);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	uint32_t len = crypto_hash_alg_common(tfm)->statesize;

	/* Get one driver ctx for the request */
	rc = lca_te_ctx_get(ctx->cgov, &areq_ctx->bctx, (void *)req, false);
	if (rc != 0) {
		dev_err(dev, "lca_te_ctx_get algo (0x%x) ret:%d\n",
		ctx->alg, rc);
		goto out;
	}
	BUG_ON(areq_ctx->bctx == NULL);

	/* Import the partial state */
	switch (lca_te_hash_type(ctx->alg)) {
	case LCA_TE_TYPE_HASH:
		rc = te_dgst_import(areq_ctx->dctx, areq_ctx->priv, len);
		break;
	case LCA_TE_TYPE_HMAC:
		rc = te_hmac_import(areq_ctx->hctx, areq_ctx->priv, len);
		break;
	case LCA_TE_TYPE_CMAC:
		rc = te_cmac_import(areq_ctx->cctx, areq_ctx->priv, len);
		break;
	case LCA_TE_TYPE_CBCMAC:
		rc = te_cbcmac_import(areq_ctx->cbctx, areq_ctx->priv, len);
		break;
	default:
		dev_err(dev, "unsupported ahash algo (0x%x)\n", ctx->alg);
		rc = -EINVAL;
		goto err_putctx;
	}

	if (rc != TE_SUCCESS) {
		dev_err(dev, "driver import algo (0x%x) ret:0x%x\n",
		ctx->alg, rc);
		goto err_drv;
	}

	goto out;

err_drv:
	rc = TE2ERRNO(rc);
err_putctx:
	lca_te_ctx_put(ctx->cgov, areq_ctx->bctx);
out:
	return rc;
}

/**
 * Set key for all driver contexts. This functin is effective for cmac
 * and cbcmac algorithms.
 */
static int lca_te_cmac_setkey(struct crypto_ahash *tfm, const u8 *key,
			      unsigned int keylen)
{
	int rc = -1;
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);

	pm_runtime_get_sync(dev);

	rc = lca_te_ctx_setkey(ctx->cgov, key, keylen);
	if (rc != 0) {
		dev_err(dev, "lca_te_ctx_setkey algo (0x%x) ret:%d\n",
			ctx->alg, rc);
	}

	pm_runtime_put_autosuspend(dev);
	return rc;
}

static int lca_te_hash_cra_init(struct crypto_tfm *tfm)
{
	int rc = 0;
	struct te_hash_ctx *ctx = crypto_tfm_ctx(tfm);
	struct hash_alg_common *hash_alg_common =
		container_of(tfm->__crt_alg, struct hash_alg_common, base);
	struct ahash_alg *ahash_alg =
		container_of(hash_alg_common, struct ahash_alg, halg);
	struct te_hash_alg *halg =
			container_of(ahash_alg, struct te_hash_alg, ahash_alg);
	struct device *dev = drvdata_to_dev(halg->drvdata);
	struct lca_te_drv_ctx_gov *cgov = NULL;
	union {
		LCA_TE_DRV_OPS_DEF(dgst, dgst);
		LCA_TE_DRV_OPS_DEF(hmac, hmac);
		LCA_TE_DRV_OPS_DEF(cmac, cmac);
		LCA_TE_DRV_OPS_DEF(cbcmac, cbcmac);
		lca_te_base_ops_t base;
	} drv_ops = {0};

	dev_dbg(dev, "initializing context @%p for %s\n", ctx,
		crypto_tfm_alg_name(tfm));

	memset(ctx, 0, sizeof(*ctx));
	ctx->alg = halg->alg;
	ctx->drvdata = halg->drvdata;
	ctx->blocksize = crypto_tfm_alg_blocksize(tfm);
	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct te_ahash_req_ctx) +
				 TE_MAX_STAT_SZ);

	switch (lca_te_hash_type(ctx->alg)) {
	case LCA_TE_TYPE_HASH:
		drv_ops.dgst.init   = te_dgst_init;
		drv_ops.dgst.free   = te_dgst_free;
		drv_ops.dgst.clone  = lca_te_dgst_clone;
		drv_ops.dgst.setkey = NULL;
		drv_ops.dgst.reset  = lca_te_dgst_reset;
		break;
	case LCA_TE_TYPE_HMAC:
		drv_ops.hmac.init   = te_hmac_init;
		drv_ops.hmac.free   = te_hmac_free;
		drv_ops.hmac.clone  = lca_te_hmac_clone;
		drv_ops.hmac.setkey = NULL;
		drv_ops.hmac.reset  = lca_te_hmac_reset;
		break;
	case LCA_TE_TYPE_CMAC:
		drv_ops.cmac.init   = TE_INIT_FALLBACK_FN(cmac);
		drv_ops.cmac.free   = te_cmac_free;
		drv_ops.cmac.clone  = te_cmac_clone;
		drv_ops.cmac.setkey = te_cmac_setkey;
		drv_ops.cmac.reset  = lca_te_cmac_reset;
		break;
	case LCA_TE_TYPE_CBCMAC:
		drv_ops.cbcmac.init   = TE_INIT_FALLBACK_FN(cbcmac);
		drv_ops.cbcmac.free   = te_cbcmac_free;
		drv_ops.cbcmac.clone  = te_cbcmac_clone;
		drv_ops.cbcmac.setkey = te_cbcmac_setkey;
		drv_ops.cbcmac.reset  = lca_te_cbcmac_reset;
		break;
	default:
		dev_err(dev, "unsupported ahash algo (0x%x)\n", ctx->alg);
		rc = -EINVAL;
		goto out;
	}

	pm_runtime_get_sync(dev);
	cgov = lca_te_ctx_alloc_gov(&drv_ops.base, ctx->drvdata->h,
				    ctx->alg, tfm);
	if (IS_ERR(cgov)) {
		rc = PTR_ERR(cgov);
		dev_err(dev, "lca_te_ctx_alloc_gov algo (0x%x) ret:%d\n",
			ctx->alg, rc);
		goto err;
	}

	ctx->cgov = cgov;

err:
	pm_runtime_put_autosuspend(dev);
out:
	dev_dbg(dev, "initializing context @%p for %s ret:%d\n", ctx,
		crypto_tfm_alg_name(tfm), rc);
	return rc;
}

static void lca_te_hash_cra_exit(struct crypto_tfm *tfm)
{
	struct te_hash_ctx *ctx = crypto_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);

	pm_runtime_get_sync(dev);

	lca_te_ctx_free_gov(ctx->cgov);
	if (ctx->mackey) {
		kfree(ctx->mackey);
		ctx->mackey = NULL;
	}

	memset(ctx, 0, sizeof(*ctx));
	pm_runtime_put_autosuspend(dev);
	return;
}

static void te_ahash_complete(struct te_async_request *te_req, int err)
{
	struct ahash_request *req = (struct ahash_request *)te_req->data;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_hash_handle *hhdl = ctx->drvdata->hash_handle;
	int e = TE2ERRNO(err);

	switch (lca_te_hash_type(ctx->alg)) {
	case LCA_TE_TYPE_HASH:
		te_buf_mgr_free_memlist(&areq_ctx->dgst_req.dgst.in, e);
		break;
	case LCA_TE_TYPE_HMAC:
		te_buf_mgr_free_memlist(&areq_ctx->hmac_req.hmac.in, e);
		break;
	case LCA_TE_TYPE_CMAC:
	case LCA_TE_TYPE_CBCMAC:
		te_buf_mgr_free_memlist(&areq_ctx->cmac_req.amac.in, e);
		break;
	default:
		dev_err(dev, "unsupported ahash algo (0x%x)\n", ctx->alg);
		break;
	}

	lca_te_ra_complete(hhdl->ra, &areq_ctx->base);
	ahash_request_complete(req, e);
	pm_runtime_put_autosuspend(dev);
}

static int lca_te_ahash_do_digest(struct te_request *treq)
{
	int rc = -1;
	struct ahash_request *req =
		container_of((void*(*)[])treq, struct ahash_request, __ctx);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct scatterlist *src = req->src;
	te_hmac_request_t *hreq = NULL;
	te_dgst_request_t *dreq = NULL;
	te_cmac_request_t *creq = NULL;

	dev_dbg(dev, "ahash digest algo (0x%x)\n", ctx->alg);
	pm_runtime_get_sync(dev);

	switch (lca_te_hash_type(ctx->alg)) {
	case LCA_TE_TYPE_HASH:
		dreq = &areq_ctx->dgst_req;
		memset(dreq, 0, sizeof(*dreq));
		dreq->base.flags = req->base.flags;
		dreq->base.completion = te_ahash_complete;
		dreq->base.data = req;
		dreq->dgst.hash = req->result;

		rc = TE_BUF_MGR_GEN_MEMLIST_SRC(src, req->nbytes,
						&dreq->dgst.in);
		if (rc != 0) {
			dreq = NULL;
			goto err;
		}
		rc = te_adgst(ctx->drvdata->h, ctx->alg, dreq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_adgst algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			rc = TE2ERRNO(rc);
			te_buf_mgr_free_memlist(&dreq->dgst.in, rc);
			dreq = NULL;
			goto err;
		}
		break;
	case LCA_TE_TYPE_HMAC:
		hreq = &areq_ctx->hmac_req;
		memset(hreq, 0, sizeof(*hreq));
		hreq->base.flags = req->base.flags;
		hreq->base.completion = te_ahash_complete;
		hreq->base.data = req;
		hreq->hmac.mac = req->result;
		hreq->hmac.maclen = crypto_ahash_digestsize(tfm);
		hreq->hmac.key.type = TE_KEY_TYPE_USER;
		hreq->hmac.key.user.key = ctx->mackey;
		hreq->hmac.key.user.keybits = ctx->keylen*BITS_IN_BYTE;

		rc = TE_BUF_MGR_GEN_MEMLIST_SRC(src, req->nbytes,
						&hreq->hmac.in);
		if (rc != 0) {
			hreq = NULL;
			goto err;
		}
		rc = te_ahmac(ctx->drvdata->h, ctx->alg, hreq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_ahmac algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			rc = TE2ERRNO(rc);
			te_buf_mgr_free_memlist(&hreq->hmac.in, rc);
			hreq = NULL;
			goto err;
		}
		break;
	case LCA_TE_TYPE_CMAC:
	case LCA_TE_TYPE_CBCMAC:
		creq = &areq_ctx->cmac_req;
		memset(creq, 0, sizeof(*creq));
		creq->base.flags = req->base.flags;
		creq->base.completion = te_ahash_complete;
		creq->base.data = req;
		creq->amac.mac = req->result;
		creq->amac.maclen = crypto_ahash_digestsize(tfm);

		memset(areq_ctx->iv, 0, sizeof(areq_ctx->iv));
		creq->amac.iv = areq_ctx->iv;
		creq->amac.key.type = TE_KEY_TYPE_USER;
		creq->amac.key.user.key = ctx->mackey;
		creq->amac.key.user.keybits = ctx->keylen * BITS_IN_BYTE;
		rc = TE_BUF_MGR_GEN_MEMLIST_SRC(src, req->nbytes,
						&creq->amac.in);
		if (rc != 0) {
			creq = NULL;
			goto err;
		}

		if (LCA_TE_TYPE_CMAC == lca_te_hash_type(ctx->alg)) {
			rc = te_acmac(ctx->drvdata->h,
				TE_ALG_GET_MAIN_ALG(ctx->alg), creq);
		} else {
			rc = te_acbcmac(ctx->drvdata->h,
				TE_ALG_GET_MAIN_ALG(ctx->alg), creq);
		}
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_%s algo (0x%x) ret:0x%x\n",
			(LCA_TE_TYPE_CMAC == lca_te_hash_type(ctx->alg)) ?
			"acmac" : "acbcmac", ctx->alg, rc);
			rc = TE2ERRNO(rc);
			te_buf_mgr_free_memlist(&creq->amac.in, rc);
			creq = NULL;
			goto err;
		}
		break;
	default:
		dev_err(dev, "unsupported ahash algo (0x%x)\n", ctx->alg);
		rc = -EINVAL;
		goto err;
	}

	return -EINPROGRESS;

err:
	pm_runtime_put_autosuspend(dev);
	return rc;
}

static int lca_te_ahash_request(struct ahash_request *req,
				lca_te_submit_t fn, bool prot)
{
	int rc = 0;
	struct te_request *treq = ahash_request_ctx(req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_hash_handle *hhdl = ctx->drvdata->hash_handle;

	pm_runtime_get_sync(dev);
	memset(treq, 0, sizeof(*treq));
	treq->fn   = fn;
	treq->areq = &req->base;
	rc = lca_te_ra_submit(hhdl->ra, treq, prot);
	if (rc != -EINPROGRESS) {
		pm_runtime_put_autosuspend(dev);
	}
	return rc;
}

static int lca_te_ahash_digest(struct ahash_request *req)
{
	/* No need to protect the digest() request */
	return lca_te_ahash_request(req, lca_te_ahash_do_digest, false);
}

static void te_ahash_init_complete(struct te_async_request *te_req, int err)
{
	int rc = -1;
	struct ahash_request *req = (struct ahash_request *)te_req->data;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	uint32_t len = TE_MAX_STAT_SZ;
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_hash_handle *hhdl = ctx->drvdata->hash_handle;
	int e = 0;

	/* Export the partial state */
	switch (lca_te_hash_type(ctx->alg)) {
	case LCA_TE_TYPE_HASH:
		rc = te_dgst_export(areq_ctx->dctx, areq_ctx->priv, &len);
		break;
	case LCA_TE_TYPE_HMAC:
		rc = te_hmac_export(areq_ctx->hctx, areq_ctx->priv, &len);
		break;
	case LCA_TE_TYPE_CMAC:
		rc = te_cmac_export(areq_ctx->cctx, areq_ctx->priv, &len);
		break;
	case LCA_TE_TYPE_CBCMAC:
		rc = te_cbcmac_export(areq_ctx->cbctx, areq_ctx->priv, &len);
		break;
	default:
		dev_err(dev, "unsupported ahash algo (0x%x)\n", ctx->alg);
		e = -EINVAL;
		goto out;
	}

	if (rc != TE_SUCCESS) {
		dev_err(dev, "driver export algo (0x%x) ret:0x%x\n",
			ctx->alg, rc);
		/* Respect the driver reporting error if any */
		if (TE_SUCCESS == err) {
			err = rc;
		}
	} else {
		crypto_hash_alg_common(tfm)->statesize = len;
	}

	e = TE2ERRNO(err);
out:
	/* Release the driver ctx */
	lca_te_ctx_put(ctx->cgov, areq_ctx->bctx);
	areq_ctx->bctx = NULL;
	LCA_TE_RA_COMPLETE(hhdl->ra, ahash_request_complete, req, e);
	pm_runtime_put_autosuspend(dev);
}

static int lca_te_ahash_do_init(struct te_request *treq)
{
	int rc = -1;
	struct ahash_request *req =
		container_of((void*(*)[])treq, struct ahash_request, __ctx);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	te_hmac_request_t *hreq = NULL;
	te_dgst_request_t *dreq = NULL;
	te_cmac_request_t *creq = NULL;

	/**
	 * Get one driver ctx for the request.
	 * Reset the driver ctx for it might be "polluted" by others.
	 */
	rc = lca_te_ctx_get(ctx->cgov, &areq_ctx->bctx, (void *)req, true);
	if (rc != 0) {
		dev_err(dev, "lca_te_ctx_get algo (0x%x) ret:%d\n",
		ctx->alg, rc);
		goto out;
	}
	BUG_ON(areq_ctx->bctx == NULL);

	switch (lca_te_hash_type(ctx->alg)) {
	case LCA_TE_TYPE_HASH:
		dreq = &areq_ctx->dgst_req;
		memset(dreq, 0, sizeof(*dreq));
		dreq->base.flags = req->base.flags;
		dreq->base.completion = te_ahash_init_complete;
		dreq->base.data = req;

		rc = te_dgst_astart(areq_ctx->dctx, dreq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_dgst_astart algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			dreq = NULL;
			goto err_drv;
		}
		break;
	case LCA_TE_TYPE_HMAC:
		hreq = &areq_ctx->hmac_req;
		memset(hreq, 0, sizeof(*hreq));
		hreq->base.flags = req->base.flags;
		hreq->base.completion = te_ahash_init_complete;
		hreq->base.data = req;
		hreq->st.key.type = TE_KEY_TYPE_USER;
		hreq->st.key.user.key = ctx->mackey;
		hreq->st.key.user.keybits = ctx->keylen * BITS_IN_BYTE;

		rc = te_hmac_astart(areq_ctx->hctx, hreq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_hmac_astart algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			hreq = NULL;
			goto err_drv;
		}
		break;
	case LCA_TE_TYPE_CMAC:
		creq = &areq_ctx->cmac_req;
		memset(creq, 0, sizeof(*creq));
		creq->base.flags = req->base.flags;
		creq->base.completion = te_ahash_init_complete;
		creq->base.data = req;
		rc = te_cmac_astart(areq_ctx->cctx, creq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_cmac_astart algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			creq = NULL;
			goto err_drv;
		}
		break;
	case LCA_TE_TYPE_CBCMAC:
		creq = &areq_ctx->cmac_req;
		memset(creq, 0, sizeof(*creq));
		creq->base.flags = req->base.flags;
		creq->base.completion = te_ahash_init_complete;
		creq->base.data = req;
		memset(areq_ctx->iv, 0, sizeof(areq_ctx->iv));
		creq->st.iv = areq_ctx->iv;

		rc = te_cbcmac_astart(areq_ctx->cbctx, creq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_cbcmac_astart algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			creq = NULL;
			goto err_drv;
		}
		break;
	default:
		dev_err(dev, "unsupported ahash algo (0x%x)\n", ctx->alg);
		rc = -EINVAL;
		goto err;
	}

	return -EINPROGRESS;

err_drv:
	rc = TE2ERRNO(rc);
err:
	/* Release the driver ctx on errors */
	lca_te_ctx_put(ctx->cgov, areq_ctx->bctx);
	areq_ctx->bctx = NULL;
out:
	return rc;
}

static int lca_te_ahash_init(struct ahash_request *req)
{
	return lca_te_ahash_request(req, lca_te_ahash_do_init, true);
}

static void te_ahash_update_complete(struct te_async_request *te_req, int err)
{
	int rc = 0;
	struct ahash_request *req = (struct ahash_request *)te_req->data;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	uint32_t len = crypto_hash_alg_common(tfm)->statesize;
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_hash_handle *hhdl = ctx->drvdata->hash_handle;
	int e = TE2ERRNO(err);

	/* Export the partial state */
	switch (lca_te_hash_type(ctx->alg)) {
	case LCA_TE_TYPE_HASH:
		rc = te_dgst_export(areq_ctx->dctx, areq_ctx->priv, &len);
		te_buf_mgr_free_memlist(&areq_ctx->dgst_req.up.in, e);
		break;
	case LCA_TE_TYPE_HMAC:
		rc = te_hmac_export(areq_ctx->hctx, areq_ctx->priv, &len);
		te_buf_mgr_free_memlist(&areq_ctx->hmac_req.up.in, e);
		break;
	case LCA_TE_TYPE_CMAC:
		rc = te_cmac_export(areq_ctx->cctx, areq_ctx->priv, &len);
		te_buf_mgr_free_memlist(&areq_ctx->cmac_req.up.in, e);
		break;
	case LCA_TE_TYPE_CBCMAC:
		rc = te_cbcmac_export(areq_ctx->cbctx, areq_ctx->priv, &len);
		te_buf_mgr_free_memlist(&areq_ctx->cmac_req.up.in, e);
		break;
	default:
		dev_err(dev, "unsupported ahash algo (0x%x)\n", ctx->alg);
		e = -EINVAL;
		goto out;
	}

	if (rc != TE_SUCCESS) {
		dev_err(dev, "driver export algo (0x%x) ret:0x%x\n",
			ctx->alg, rc);
		/* Respect the driver reporting error if any */
		if (TE_SUCCESS == err) {
			e = TE2ERRNO(rc);
		}
		goto out;
	}

out:
	/* Release the driver ctx */
	lca_te_ctx_put(ctx->cgov, areq_ctx->bctx);
	areq_ctx->bctx = NULL;
	LCA_TE_RA_COMPLETE(hhdl->ra, ahash_request_complete, req, e);
	pm_runtime_put_autosuspend(dev);
}

static int lca_te_ahash_do_update(struct te_request *treq)
{
	int rc = -1;
	struct ahash_request *req =
		container_of((void*(*)[])treq, struct ahash_request, __ctx);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct scatterlist *src = req->src;
	te_hmac_request_t *hreq = NULL;
	te_dgst_request_t *dreq = NULL;
	te_cmac_request_t *creq = NULL;

	/**
	 * Get and import one driver ctx for the incoming request.
	 */
	rc = lca_te_attach_import(areq_ctx);
	if (rc != 0) {
		goto out;
	}
	BUG_ON(areq_ctx->bctx == NULL);

	switch (lca_te_hash_type(ctx->alg)) {
	case LCA_TE_TYPE_HASH:
		dreq = &areq_ctx->dgst_req;
		memset(dreq, 0, sizeof(*dreq));
		dreq->base.flags = req->base.flags;
		dreq->base.completion = te_ahash_update_complete;
		dreq->base.data = req;

		rc = TE_BUF_MGR_GEN_MEMLIST_SRC(src, req->nbytes, &dreq->up.in);
		if (rc != 0) {
			dreq = NULL;
			goto err;
		}

		rc = te_dgst_aupdate(areq_ctx->dctx, dreq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_dgst_aupdate algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			rc = TE2ERRNO(rc);
			te_buf_mgr_free_memlist(&dreq->up.in, rc);
			dreq = NULL;
			goto err;
		}
		break;
	case LCA_TE_TYPE_HMAC:
		hreq = &areq_ctx->hmac_req;
		memset(hreq, 0, sizeof(*hreq));
		hreq->base.flags = req->base.flags;
		hreq->base.completion = te_ahash_update_complete;
		hreq->base.data = req;

		rc = TE_BUF_MGR_GEN_MEMLIST_SRC(src, req->nbytes, &hreq->up.in);
		if (rc != 0) {
			hreq = NULL;
			goto err;
		}

		rc = te_hmac_aupdate(areq_ctx->hctx, hreq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_hmac_aupdate algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			rc = TE2ERRNO(rc);
			te_buf_mgr_free_memlist(&hreq->up.in, rc);
			hreq = NULL;
			goto err;
		}
		break;
	case LCA_TE_TYPE_CMAC:
		creq = &areq_ctx->cmac_req;
		memset(creq, 0, sizeof(*creq));
		creq->base.flags = req->base.flags;
		creq->base.completion = te_ahash_update_complete;
		creq->base.data = req;

		rc = TE_BUF_MGR_GEN_MEMLIST_SRC(src, req->nbytes, &creq->up.in);
		if (rc != 0) {
			creq = NULL;
			goto err;
		}
		rc = te_cmac_aupdate(areq_ctx->cctx, creq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_cmac_aupdate algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			rc = TE2ERRNO(rc);
			te_buf_mgr_free_memlist(&creq->up.in, rc);
			creq = NULL;
			goto err;
		}
		break;
	case LCA_TE_TYPE_CBCMAC:
		creq = &areq_ctx->cmac_req;
		memset(creq, 0, sizeof(*creq));
		creq->base.flags = req->base.flags;
		creq->base.completion = te_ahash_update_complete;
		creq->base.data = req;

		rc = TE_BUF_MGR_GEN_MEMLIST_SRC(src, req->nbytes, &creq->up.in);
		if (rc != 0) {
			creq = NULL;
			goto err;
		}
		rc = te_cbcmac_aupdate(areq_ctx->cbctx, creq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_cbcmac_aupdate algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			rc = TE2ERRNO(rc);
			te_buf_mgr_free_memlist(&creq->up.in, rc);
			creq = NULL;
			goto err;
		}
		break;
	default:
		dev_err(dev, "unsupported ahash algo (0x%x)\n", ctx->alg);
		rc = -EINVAL;
		goto err;
	}

	return -EINPROGRESS;

err:
	/* Release the driver ctx on errors */
	lca_te_ctx_put(ctx->cgov, areq_ctx->bctx);
	areq_ctx->bctx = NULL;
out:
	return rc;
}

static int lca_te_ahash_update(struct ahash_request *req)
{
	return lca_te_ahash_request(req, lca_te_ahash_do_update, true);
}

static void te_ahash_final_complete(struct te_async_request *te_req, int err)
{
	struct ahash_request *req = (struct ahash_request *)te_req->data;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct te_hash_handle *hhdl = ctx->drvdata->hash_handle;
	int e = TE2ERRNO(err);

	/* Release the driver ctx */
	lca_te_ctx_put(ctx->cgov, areq_ctx->bctx);
	areq_ctx->bctx = NULL;
	LCA_TE_RA_COMPLETE(hhdl->ra, ahash_request_complete, req, e);
	pm_runtime_put_autosuspend(dev);
}

static int lca_te_ahash_do_final(struct te_request *treq)
{
	int rc = -1;
	struct ahash_request *req =
		container_of((void*(*)[])treq, struct ahash_request, __ctx);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	te_hmac_request_t *hreq = NULL;
	te_dgst_request_t *dreq = NULL;
	te_cmac_request_t *creq = NULL;

	/**
	 * Get and import one driver ctx for the incoming request.
	 */
	rc = lca_te_attach_import(areq_ctx);
	if (rc != 0) {
		goto out;
	}
	BUG_ON(areq_ctx->bctx == NULL);

	switch (lca_te_hash_type(ctx->alg)) {
	case LCA_TE_TYPE_HASH:
		dreq = &areq_ctx->dgst_req;
		memset(dreq, 0, sizeof(*dreq));
		dreq->base.flags = req->base.flags;
		dreq->base.completion = te_ahash_final_complete;
		dreq->base.data = req;
		dreq->fin.hash = req->result;
		rc = te_dgst_afinish(areq_ctx->dctx, dreq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_dgst_afinish algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			dreq = NULL;
			goto err_drv;
		}
		break;
	case LCA_TE_TYPE_HMAC:
		hreq = &areq_ctx->hmac_req;
		memset(hreq, 0, sizeof(*hreq));
		hreq->base.flags = req->base.flags;
		hreq->base.completion = te_ahash_final_complete;
		hreq->base.data = req;
		hreq->fin.mac = req->result;
		hreq->fin.maclen = crypto_ahash_digestsize(tfm);
		rc = te_hmac_afinish(areq_ctx->hctx, hreq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_hmac_afinish algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			hreq = NULL;
			goto err_drv;
		}
		break;
	case LCA_TE_TYPE_CMAC:
		creq = &areq_ctx->cmac_req;
		memset(creq, 0, sizeof(*creq));
		creq->base.flags = req->base.flags;
		creq->base.completion = te_ahash_final_complete;
		creq->base.data = req;
		creq->fin.mac = req->result;
		creq->fin.maclen = crypto_ahash_digestsize(tfm);
		rc = te_cmac_afinish(areq_ctx->cctx, creq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_cmac_afinish algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			creq = NULL;
			goto err_drv;
		}
		break;
	case LCA_TE_TYPE_CBCMAC:
		creq = &areq_ctx->cmac_req;
		memset(creq, 0, sizeof(*creq));
		creq->base.flags = req->base.flags;
		creq->base.completion = te_ahash_final_complete;
		creq->base.data = req;
		creq->fin.mac = req->result;
		creq->fin.maclen = crypto_ahash_digestsize(tfm);
		rc = te_cbcmac_afinish(areq_ctx->cbctx, creq);
		if (rc != TE_SUCCESS) {
			dev_err(dev, "te_cbcmac_afinish algo (0x%x) ret:0x%x\n",
				ctx->alg, rc);
			creq = NULL;
			goto err_drv;
		}
		break;
	default:
		dev_err(dev, "unsupported ahash algo (0x%x)\n", ctx->alg);
		goto err;
	}

	return -EINPROGRESS;

err_drv:
	rc = TE2ERRNO(rc);
err:
	/* Release the driver ctx on errors */
	lca_te_ctx_put(ctx->cgov, areq_ctx->bctx);
	areq_ctx->bctx = NULL;
out:
	return rc;
}

static int lca_te_ahash_final(struct ahash_request *req)
{
	return lca_te_ahash_request(req, lca_te_ahash_do_final, true);
}

static int lca_te_ahash_export(struct ahash_request *req, void *out)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	uint32_t len = crypto_hash_alg_common(tfm)->statesize;
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);

	/* Copy out the partial state */
	memcpy(out, areq_ctx->priv, len);
	return 0;
}

static int lca_te_ahash_import(struct ahash_request *req, const void *in)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	uint32_t len = crypto_ahash_statesize(tfm);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);

	/* Copy in the partial state */
	memcpy(areq_ctx->priv, in, len);
	return 0;
}

static int lca_te_ahash_setkey(struct crypto_ahash *tfm, const u8 *key,
			       unsigned int keylen)
{
	int rc = 0;
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);

	switch (lca_te_hash_type(ctx->alg)) {
	case LCA_TE_TYPE_HASH:
		break;
	case LCA_TE_TYPE_CMAC:
	case LCA_TE_TYPE_CBCMAC:
		/*
		 * Set the key to the the driver ctx so as to get rid of the
		 * set key operation on init(). This is aimed to promote the
		 * init() performance.
		 * The setkey() is called on transformation object basis,
		 * while the init() is called on request object basis.
		 * Usually, the setkey() is called only once per transformation
		 * object. But the init() might be called more than once
		 * depending on the number of requests.
		 */
		rc = lca_te_cmac_setkey(tfm, key, keylen);
		if (rc != 0) {
			goto err;
		}
		/**
		 * The fallthrough is required for the all-in-one digest()
		 * function for cmac and cbcmac.
		 */
		/* FALLTHRU */
		fallthrough;
	case LCA_TE_TYPE_HMAC:
		if (ctx->mackey) {
			kfree(ctx->mackey);
			ctx->mackey = NULL;
			ctx->keylen = 0;
		}

		ctx->mackey = kmalloc(keylen, GFP_KERNEL);
		if (NULL == ctx->mackey) {
			rc = -ENOMEM;
			goto err;
		}

		memcpy(ctx->mackey, key, keylen);
		ctx->keylen = keylen;
		break;
	default:
		rc = -EINVAL;
		break;
	}
err:
	return rc;
}

/* hash descriptors */
static struct te_hash_template te_ahash_algs[] = {
	{
		.name = "md5",
		.driver_name = "md5-te",
		.mac_name = "hmac(md5)",
		.mac_driver_name = "hmac-md5-te",
		.blocksize = MD5_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = MD5_DIGEST_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_MD5,
	},
	{
		.name = "sha1",
		.driver_name = "sha1-te",
		.mac_name = "hmac(sha1)",
		.mac_driver_name = "hmac-sha1-te",
		.blocksize = SHA1_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = SHA1_DIGEST_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA1,
	},
	{
		.name = "sha224",
		.driver_name = "sha224-te",
		.mac_name = "hmac(sha224)",
		.mac_driver_name = "hmac-sha224-te",
		.blocksize = SHA224_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = SHA224_DIGEST_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA224,
	},
	{
		.name = "sha256",
		.driver_name = "sha256-te",
		.mac_name = "hmac(sha256)",
		.mac_driver_name = "hmac-sha256-te",
		.blocksize = SHA256_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = SHA256_DIGEST_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA256,
	},
	{
		.name = "sha384",
		.driver_name = "sha384-te",
		.mac_name = "hmac(sha384)",
		.mac_driver_name = "hmac-sha384-te",
		.blocksize = SHA384_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = SHA384_DIGEST_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA384,
	},
	{
		.name = "sha512",
		.driver_name = "sha512-te",
		.mac_name = "hmac(sha512)",
		.mac_driver_name = "hmac-sha512-te",
		.blocksize = SHA512_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = SHA512_DIGEST_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA512,
	},
	{
		.name = "sm3",
		.driver_name = "sm3-te",
		.mac_name = "hmac(sm3)",
		.mac_driver_name = "hmac-sm3-te",
		.blocksize = SM3_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = SM3_DIGEST_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SM3,
	},
	{
		.mac_name = "cmac(aes)",
		.mac_driver_name = "cmac-aes-te",
		.blocksize = AES_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = AES_BLOCK_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = false,
		.alg = TE_ALG_AES_CMAC,
	},
	{
		.mac_name = "cmac(sm4)",
		.mac_driver_name = "cmac-sm4-te",
		.blocksize = SM4_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = SM4_BLOCK_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = false,
		.alg = TE_ALG_SM4_CMAC,
	},
	{
		.mac_name = "cmac(des)",
		.mac_driver_name = "cmac-des-te",
		.blocksize = DES_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = DES_BLOCK_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = false,
		.alg = TE_ALG_DES_CMAC,
	},
	{
		.mac_name = "cmac(des3_ede)",
		.mac_driver_name = "cmac-3des-te",
		.blocksize = DES3_EDE_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = DES3_EDE_BLOCK_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = false,
		.alg = TE_ALG_TDES_CMAC,
	},
	{
		.mac_name = "cbcmac(aes)",
		.mac_driver_name = "cbcmac-aes-te",
		.blocksize = AES_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = AES_BLOCK_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = false,
		.alg = TE_ALG_AES_CBC_MAC_NOPAD,
	},
	{
		.mac_name = "cbcmac(sm4)",
		.mac_driver_name = "cbcmac-sm4-te",
		.blocksize = SM4_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = SM4_BLOCK_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = false,
		.alg = TE_ALG_SM4_CBC_MAC_NOPAD,
	},
	{
		.mac_name = "cbcmac(des)",
		.mac_driver_name = "cbcmac-des-te",
		.blocksize = DES_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = DES_BLOCK_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = false,
		.alg = TE_ALG_DES_CBC_MAC_NOPAD,
	},
	{
		.mac_name = "cbcmac(des3_ede)",
		.mac_driver_name = "cbcmac-3des-te",
		.blocksize = DES3_EDE_BLOCK_SIZE,
		.template_ahash = {
			.init = lca_te_ahash_init,
			.update = lca_te_ahash_update,
			.final = lca_te_ahash_final,
			.digest = lca_te_ahash_digest,
			.export = lca_te_ahash_export,
			.import = lca_te_ahash_import,
			.setkey = lca_te_ahash_setkey,
			.halg = {
				.digestsize = DES3_EDE_BLOCK_SIZE,
				.statesize = TE_MAX_STAT_SZ,
			},
		},
		.ishash = false,
		.alg = TE_ALG_TDES_CBC_MAC_NOPAD,
	},
};

static struct te_hash_alg *te_hash_create_alg(
				struct te_hash_template *template, bool is_hmac)
{
	struct te_hash_alg *t_crypto_alg;
	struct crypto_alg *alg;
	struct ahash_alg *halg;

	t_crypto_alg = kzalloc(sizeof(*t_crypto_alg), GFP_KERNEL);
	if (!t_crypto_alg) {
		return ERR_PTR(-ENOMEM);
	}
	t_crypto_alg->ahash_alg = template->template_ahash;
	halg = &t_crypto_alg->ahash_alg;
	alg = &halg->halg.base;

	if (template->ishash && !is_hmac) {
		halg->setkey = NULL;
		t_crypto_alg->alg =
			TE_ALG_HASH_ALGO(TE_ALG_GET_MAIN_ALG(template->alg));
		snprintf(alg->cra_name, CRYPTO_MAX_ALG_NAME, "%s",
			 template->name);
		snprintf(alg->cra_driver_name, CRYPTO_MAX_ALG_NAME, "%s",
			 template->driver_name);
	} else {
		t_crypto_alg->alg = template->alg;
		snprintf(alg->cra_name, CRYPTO_MAX_ALG_NAME, "%s",
			 template->mac_name);
		snprintf(alg->cra_driver_name, CRYPTO_MAX_ALG_NAME, "%s",
			 template->mac_driver_name);
	}
	alg->cra_module = THIS_MODULE;
	alg->cra_ctxsize = sizeof(struct te_hash_ctx);
	alg->cra_priority = TE_CRA_PRIO;
	alg->cra_blocksize = template->blocksize;
	alg->cra_alignmask = 0;
	alg->cra_exit = lca_te_hash_cra_exit;
	alg->cra_init = lca_te_hash_cra_init;
	alg->cra_flags = CRYPTO_ALG_ASYNC;

	return t_crypto_alg;
}

int lca_te_hash_alloc(struct te_drvdata *drvdata)
{
	struct te_hash_handle *hash_handle;
	struct device *dev = drvdata_to_dev(drvdata);
	int rc = 0;
	int alg;

	hash_handle = kzalloc(sizeof(*hash_handle), GFP_KERNEL);
	if (!hash_handle) {
		dev_err(dev,"kzalloc failed to allocate %zu B\n",
			sizeof(*hash_handle));
		rc = -ENOMEM;
		goto fail;
	}

	drvdata->hash_handle = hash_handle;
	INIT_LIST_HEAD(&hash_handle->hash_list);
	/* create request agent */
	rc = lca_te_ra_alloc(&hash_handle->ra, "ahash");
	if (rc != 0) {
		dev_err(dev,"ahash alloc ra failed %d\n", rc);
		goto fail;
	}

	/* ahash registration */
	for (alg = 0; alg < ARRAY_SIZE(te_ahash_algs); alg++) {
		struct te_hash_alg *t_alg;
		if (te_ahash_algs[alg].ishash) {
			/* register hmac version */
			t_alg = te_hash_create_alg(&te_ahash_algs[alg], true);
			if (IS_ERR(t_alg)) {
				rc = PTR_ERR(t_alg);
				dev_err(dev,"%s alg allocation failed\n",
					te_ahash_algs[alg].mac_driver_name);
				goto fail;
			}
			t_alg->drvdata = drvdata;
			rc = crypto_register_ahash(&t_alg->ahash_alg);
			if (unlikely(rc)) {
				dev_err(dev,"%s alg registration failed\n",
					te_ahash_algs[alg].mac_driver_name);
				kfree(t_alg);
				goto fail;
			} else {
				list_add_tail(&t_alg->entry,
						  &hash_handle->hash_list);
				dev_dbg(dev,"registered %s\n",
					te_ahash_algs[alg].mac_driver_name);
			}

			/* register hash version */
			t_alg = te_hash_create_alg(&te_ahash_algs[alg], false);
			if (IS_ERR(t_alg)) {
				rc = PTR_ERR(t_alg);
				dev_err(dev,"%s alg allocation failed\n",
					te_ahash_algs[alg].driver_name);
				goto fail;
			}
			t_alg->drvdata = drvdata;
			rc = crypto_register_ahash(&t_alg->ahash_alg);
			if (unlikely(rc)) {
				dev_err(dev,"%s alg registration failed\n",
					te_ahash_algs[alg].driver_name);
				kfree(t_alg);
				goto fail;
			} else {
				list_add_tail(&t_alg->entry, &hash_handle->hash_list);
			}

		}else {
			/* register cmac and cbcmac version */
			t_alg = te_hash_create_alg(&te_ahash_algs[alg], false);
			if (IS_ERR(t_alg)) {
				rc = PTR_ERR(t_alg);
				dev_err(dev,"%s alg allocation failed\n",
					te_ahash_algs[alg].driver_name);
				goto fail;
			}
			t_alg->drvdata = drvdata;
			rc = crypto_register_ahash(&t_alg->ahash_alg);
			if (unlikely(rc)) {
				dev_err(dev,"%s alg registration failed\n",
					te_ahash_algs[alg].driver_name);
				kfree(t_alg);
				goto fail;
			} else {
				list_add_tail(&t_alg->entry, &hash_handle->hash_list);
			}
		}
	}

	return 0;

fail:
	lca_te_hash_free(drvdata);
	return rc;
}

int lca_te_hash_free(struct te_drvdata *drvdata)
{
	struct te_hash_alg *t_hash_alg, *hash_n;
	struct te_hash_handle *hash_handle = drvdata->hash_handle;

	if (hash_handle) {
		/* free request agent */
		lca_te_ra_free(hash_handle->ra);
		hash_handle->ra = NULL;

		list_for_each_entry_safe(t_hash_alg, hash_n, &hash_handle->hash_list, entry) {
			crypto_unregister_ahash(&t_hash_alg->ahash_alg);
			list_del(&t_hash_alg->entry);
			kfree(t_hash_alg);
		}

		kfree(hash_handle);
		drvdata->hash_handle = NULL;
	}
	return 0;
}
