//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <crypto/algapi.h>
#include <crypto/des.h>
#include "te_cipher.h"
#include "te_xts.h"
#include <crypto/internal/des.h>

#include "lca_te_cipher.h"
#include "lca_te_buf_mgr.h"
#include "lca_te_ctx.h"
#include "lca_te_ra.h"

/**
 * SKCIPHER Driver Context Management Introduction.
 *
 * The skcipher driver is optimized to promote parallel operation performance
 * for single-tfm-multi-thread and multi-tfm-multi-thread scenarios. The ideas
 * are:
 * - One driver ctx for each request.
 * - The driver ctx is controlled by the driver context manager. See
 *   'lca_te_ctx.c' for details.
 * - Attach a driver ctx on entry to encrypt() and decrypt().
 * - Detach the driver ctx on completion of encrypt() and decrypt().
 * - Detach the driver ctx on encrypt() or decrypt() error.
 * - Submit request by way of the request agent.
 *
 *  +-------------------------+      \
 *  |  transformation (tfm)   | ...  |
 *  +-------------------------+      |
 *     |       |           |          > LCA DRV
 *  +-----+ +-----+     +-----+      |
 *  |req#0| |req#1| ... |req#m|      |
 *  +-----+ +-----+     +-----+      /
 *     |       |           |
 *     +-------+----...----+         \
 *     |       |           |         |
 *  +-----+ +-----+     +-----+       > TE DRV
 *  |ctx0 | |ctx1 | ... |ctxn |      |
 *  +-----+ +-----+     +-----+      /
 *  \                         /
 *   `-----------v-----------'
 *      encrypt(),decrypt()
 *
 */

#define template_skcipher	template_u.skcipher

#define  CHECK_CHAIN_MODE_VALID(_mode_)                      \
    ((((TE_CHAIN_MODE_XTS) == (_mode_)) ||                   \
    ((TE_CHAIN_MODE_CTR) == (_mode_)) ||                     \
    ((TE_CHAIN_MODE_OFB) == (_mode_)) ||                     \
    ((TE_CHAIN_MODE_CBC_NOPAD) == (_mode_)) ||               \
    ((TE_CHAIN_MODE_ECB_NOPAD) == (_mode_))) ? 1 : 0)

#define SM4_XTS_KEY_SIZE      (32)
#define AES_XTS_KEY_MIN_SIZE  (32)
#define AES_XTS_KEY_MAX_SIZE  (64)
#define MAX_BLOCK_SIZE        (16)

struct te_cipher_handle {
	lca_te_ra_handle ra;
	struct list_head alg_list;
};

struct lca_te_cipher_ctx {
	struct te_drvdata *drvdata;
	te_algo_t alg;
	struct lca_te_drv_ctx_gov *cgov; /**< Driver context governor */
	uint8_t iv[MAX_BLOCK_SIZE];      /**< Initial vector or nonce(CTR) */
	uint8_t stream[MAX_BLOCK_SIZE];  /**< Stream block (CTR) */
	size_t off;              /**< Offset of iv (OFB) or stream (CTR) */
};

struct te_cipher_req_ctx {
	struct te_request base;          /**< Must put it in the beginning */
	te_sca_operation_t op;
	union {
		te_cipher_ctx_t *cctx;
		te_xts_ctx_t *xctx;
		te_base_ctx_t *bctx;
	};
	union {
		te_xts_request_t xts_req;
		te_cipher_request_t cph_req;
	};
};

/**
 * te_cipher_init() fallback: initialize a cipher ctx.
 *
 * The te_cipher_init() has the following drawback:
 * D1: accept main algorithm only.
 */
TE_DRV_INIT_FALLBACK(cipher)

/**
 * te_xts_init() fallback: initialize a xts ctx.
 *
 * The te_xts_init() has the following drawback:
 * D1: accept main algorithm only.
 */
TE_DRV_INIT_FALLBACK(xts)

static int lca_cipher_init(struct crypto_tfm *tfm)
{
	int rc = 0;
	struct lca_te_cipher_ctx *ctx_p = crypto_tfm_ctx(tfm);
	struct te_crypto_alg *te_alg =
			container_of(tfm->__crt_alg, struct te_crypto_alg,
				     skcipher_alg.base);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);
	struct lca_te_drv_ctx_gov *cgov = NULL;
	union {
		LCA_TE_DRV_OPS_DEF(xts, xts);
		LCA_TE_DRV_OPS_DEF(cipher, cph);
		lca_te_base_ops_t base;
	} drv_ops = {0};

	dev_dbg(dev, "initializing context @%p for %s\n", ctx_p,
		crypto_tfm_alg_name(tfm));

	crypto_skcipher_set_reqsize(__crypto_skcipher_cast(tfm),
				    sizeof(struct te_cipher_req_ctx) +
				    max(te_cipher_get_async_ctx_size(),
					te_xts_get_async_ctx_size()));
	memset(ctx_p, 0, sizeof(*ctx_p));

	ctx_p->alg = te_alg->alg;
	ctx_p->drvdata = te_alg->drvdata;

	switch (TE_ALG_GET_CHAIN_MODE(ctx_p->alg)) {
	case TE_CHAIN_MODE_XTS:
		drv_ops.xts.init   = TE_INIT_FALLBACK_FN(xts);
		drv_ops.xts.free   = te_xts_free;
		drv_ops.xts.clone  = te_xts_clone;
		drv_ops.xts.setkey = te_xts_setkey;
		break;
	case TE_CHAIN_MODE_ECB_NOPAD:
	case TE_CHAIN_MODE_CBC_NOPAD:
	case TE_CHAIN_MODE_CTR:
	case TE_CHAIN_MODE_OFB:
		drv_ops.cph.init   = TE_INIT_FALLBACK_FN(cipher);
		drv_ops.cph.free   = te_cipher_free;
		drv_ops.cph.clone  = te_cipher_clone;
		drv_ops.cph.setkey = te_cipher_setkey;
		break;
	default:
		dev_err(dev, "unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
		rc = -EINVAL;
		goto out;
	}

	pm_runtime_get_sync(dev);
	cgov = lca_te_ctx_alloc_gov(&drv_ops.base, ctx_p->drvdata->h,
				    ctx_p->alg, tfm);
	if (IS_ERR(cgov)) {
		rc = PTR_ERR(cgov);
		dev_err(dev, "lca_te_ctx_alloc_gov algo (0x%x) ret:%d\n",
			ctx_p->alg, rc);
		goto err;
	}

	ctx_p->cgov = cgov;

err:
	pm_runtime_put_autosuspend(dev);
out:
	return rc;
}

static void lca_cipher_exit(struct crypto_tfm *tfm)
{
	struct lca_te_cipher_ctx *ctx_p = crypto_tfm_ctx(tfm);
	struct te_crypto_alg *te_alg =
			container_of(tfm->__crt_alg, struct te_crypto_alg,
				     skcipher_alg.base);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	pm_runtime_get_sync(dev);
	lca_te_ctx_free_gov(ctx_p->cgov);
	memset(ctx_p, 0, sizeof(*ctx_p));
	pm_runtime_put_autosuspend(dev);
	return;
}

/* Block cipher alg */
static int lca_te_cipher_setkey(struct crypto_skcipher *sktfm, const u8 *key,
			    unsigned int keylen)
{
	int rc = -1;

	struct crypto_tfm *tfm = crypto_skcipher_tfm(sktfm);
	struct lca_te_cipher_ctx *ctx_p = crypto_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx_p->drvdata);

	pm_runtime_get_sync(dev);

	/* weak key process for DES and 3DES, code borrowed from
	 * des_generic.c
	 * */
	if(TE_ALG_GET_MAIN_ALG(ctx_p->alg) == TE_MAIN_ALGO_DES) {
		rc = crypto_des_verify_key(tfm, key);
		if (rc)
			goto out;
	}
	if(TE_ALG_GET_MAIN_ALG(ctx_p->alg) == TE_MAIN_ALGO_TDES) {
		rc = crypto_des3_ede_verify_key(tfm, key);
		if (rc)
			goto out;
	}

	rc = lca_te_ctx_setkey(ctx_p->cgov, key, keylen);
	if (rc != 0) {
		dev_err(dev, "lca_te_ctx_setkey algo (0x%x) ret:%d\n",
			ctx_p->alg, rc);
	}

out:
	pm_runtime_put_autosuspend(dev);
	return rc;
}

static void lca_te_cipher_complete(struct te_async_request *te_req, int err)
{
	struct skcipher_request *req = (struct skcipher_request *)te_req->data;
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct lca_te_cipher_ctx *ctx_p = crypto_skcipher_ctx(tfm);
	struct te_cipher_req_ctx *req_ctx = skcipher_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx_p->drvdata);
	struct te_cipher_handle *chdl = ctx_p->drvdata->cipher_handle;
	int e = TE2ERRNO(err);

	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		te_buf_mgr_free_memlist(&req_ctx->xts_req.src, e);
		te_buf_mgr_free_memlist(&req_ctx->xts_req.dst, e);
	} else {
		te_buf_mgr_free_memlist(&req_ctx->cph_req.src, e);
		te_buf_mgr_free_memlist(&req_ctx->cph_req.dst, e);
	}

	/* Release the driver ctx */
	lca_te_ctx_put(ctx_p->cgov, req_ctx->bctx);
	req_ctx->bctx = NULL;
	LCA_TE_RA_COMPLETE(chdl->ra, skcipher_request_complete, req, e);
	pm_runtime_put_autosuspend(dev);
}

static int lca_te_cipher_do_encrypt(struct te_request *treq)
{
	int rc = 0;
	struct skcipher_request *req =
		container_of((void*(*)[])treq, struct skcipher_request, __ctx);
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct lca_te_cipher_ctx *ctx_p = crypto_skcipher_ctx(tfm);
	struct te_cipher_req_ctx *req_ctx = skcipher_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx_p->drvdata);

	/* Don't zeroize req_ctx->base */
	req_ctx->op = TE_DRV_SCA_ENCRYPT;
	req_ctx->bctx = NULL;

	if(!CHECK_CHAIN_MODE_VALID(TE_ALG_GET_CHAIN_MODE(ctx_p->alg))){
		dev_err(dev, "unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
		return -EINVAL;
	}

	/* Get one driver ctx for the request */
	rc = lca_te_ctx_get(ctx_p->cgov, &req_ctx->bctx, (void *)req, false);
	if (rc != 0) {
		dev_err(dev, "lca_te_ctx_get algo (0x%x) ret:%d\n",
		ctx_p->alg, rc);
		goto out;
	}
	BUG_ON(req_ctx->bctx == NULL);
	req_ctx->bctx->crypt->alg = ctx_p->alg;

	if (TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		memset(&req_ctx->xts_req, 0, sizeof(req_ctx->xts_req));
		if (sizeof(req_ctx->xts_req.data_unit) <
		    crypto_skcipher_ivsize(tfm)) {
			dev_err(dev, "enc failed! invalid iv size for XTS \n");
			rc = -ENOBUFS;
			goto fail;
		}
		rc = TE_BUF_MGR_GEN_MEMLIST_SRC(req->src, req->cryptlen,
						&req_ctx->xts_req.src);
		if (rc != 0) {
			goto fail;
		}
		rc = TE_BUF_MGR_GEN_MEMLIST_DST(req->dst, req->cryptlen,
						&req_ctx->xts_req.dst);
		if (rc != 0)
			goto fail1;

		memcpy(req_ctx->xts_req.data_unit, req->iv,
		       crypto_skcipher_ivsize(tfm));
		req_ctx->xts_req.op = TE_DRV_SCA_ENCRYPT;
		req_ctx->xts_req.base.completion = lca_te_cipher_complete;
		req_ctx->xts_req.base.flags = req->base.flags;
		req_ctx->xts_req.base.data = req;
	} else {
		memset(&req_ctx->cph_req, 0, sizeof(req_ctx->cph_req));
		if (req->iv) {
			req_ctx->cph_req.iv = req->iv;
			req_ctx->cph_req.stream = NULL;
			req_ctx->cph_req.off = NULL;
		}

		rc = TE_BUF_MGR_GEN_MEMLIST_SRC(req->src, req->cryptlen,
						&req_ctx->cph_req.src);
		if (rc != 0) {
			goto fail;
		}
		rc = TE_BUF_MGR_GEN_MEMLIST_DST(req->dst, req->cryptlen,
						&req_ctx->cph_req.dst);
		if (rc != 0)
			goto fail1;

		req_ctx->cph_req.op = TE_DRV_SCA_ENCRYPT;
		req_ctx->cph_req.base.completion = lca_te_cipher_complete;
		req_ctx->cph_req.base.flags = req->base.flags;
		req_ctx->cph_req.base.data = req;
	}

	switch (TE_ALG_GET_CHAIN_MODE(ctx_p->alg)) {
	case TE_CHAIN_MODE_XTS:
		rc = te_xts_acrypt(req_ctx->xctx, &req_ctx->xts_req);
		break;
	case TE_CHAIN_MODE_ECB_NOPAD:
		rc = te_cipher_aecb(req_ctx->cctx, &req_ctx->cph_req);
		break;
	case TE_CHAIN_MODE_CBC_NOPAD:
		rc = te_cipher_acbc(req_ctx->cctx, &req_ctx->cph_req);
		break;
	case TE_CHAIN_MODE_CTR:
		rc = te_cipher_actr(req_ctx->cctx, &req_ctx->cph_req);
		break;
	case TE_CHAIN_MODE_OFB:
		rc = te_cipher_aofb(req_ctx->cctx, &req_ctx->cph_req);
		break;
	default:
		dev_err(dev, "unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
		rc = -EINVAL;
		goto fail2;
	}

	if (rc != TE_SUCCESS) {
		dev_err(dev, "te_acrypt enc algo (0x%x) ret:0x%x\n",
			ctx_p->alg, rc);
		goto err_drv;
	}

	return -EINPROGRESS;

err_drv:
	rc = TE2ERRNO(rc);
fail2:
	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		te_buf_mgr_free_memlist(&req_ctx->xts_req.dst, rc);
	} else {
		te_buf_mgr_free_memlist(&req_ctx->cph_req.dst, rc);
	}
fail1:
	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		te_buf_mgr_free_memlist(&req_ctx->xts_req.src, rc);
	} else {
		te_buf_mgr_free_memlist(&req_ctx->cph_req.src, rc);
	}
fail:
	/* Release the driver ctx */
	lca_te_ctx_put(ctx_p->cgov, req_ctx->bctx);
	req_ctx->bctx = NULL;
out:
	return rc;
}

static int lca_te_cipher_request(struct skcipher_request *req,
				 lca_te_submit_t fn)
{
	int rc = 0;
	struct te_request *treq = skcipher_request_ctx(req);
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct lca_te_cipher_ctx *ctx_p = crypto_skcipher_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx_p->drvdata);
	struct te_cipher_handle *chdl = ctx_p->drvdata->cipher_handle;

	pm_runtime_get_sync(dev);
	memset(treq, 0, sizeof(*treq));
	treq->fn   = fn;
	treq->areq = &req->base;
	rc = lca_te_ra_submit(chdl->ra, treq, true);
	if (rc != -EINPROGRESS) {
		pm_runtime_put_autosuspend(dev);
	}
	return rc;
}

static int lca_te_cipher_encrypt(struct skcipher_request *req)
{
	return lca_te_cipher_request(req, lca_te_cipher_do_encrypt);
}

static int lca_te_cipher_do_decrypt(struct te_request *treq)
{
	int rc = 0;
	struct skcipher_request *req =
		container_of((void*(*)[])treq, struct skcipher_request, __ctx);
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct lca_te_cipher_ctx *ctx_p = crypto_skcipher_ctx(tfm);
	struct te_cipher_req_ctx *req_ctx = skcipher_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx_p->drvdata);

	/* Don't zeroize req_ctx->base */
	req_ctx->op = TE_DRV_SCA_DECRYPT;
	req_ctx->bctx = NULL;

	if(!CHECK_CHAIN_MODE_VALID(TE_ALG_GET_CHAIN_MODE(ctx_p->alg))){
		dev_err(dev, "unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
		return -EINVAL;
	}

	/* Get one driver ctx for the request */
	rc = lca_te_ctx_get(ctx_p->cgov, &req_ctx->bctx, (void *)req, false);
	if (rc != 0) {
		dev_err(dev, "lca_te_ctx_get algo (0x%x) ret:%d\n",
		ctx_p->alg, rc);
		goto out;
	}
	BUG_ON(req_ctx->bctx == NULL);
	req_ctx->bctx->crypt->alg = ctx_p->alg;

	if (TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		memset(&req_ctx->xts_req, 0, sizeof(req_ctx->xts_req));
		if (sizeof(req_ctx->xts_req.data_unit) <
		    crypto_skcipher_ivsize(tfm)) {
			dev_err(dev, "dec failed! invalid iv size for XTS \n");
			rc = -ENOBUFS;
			goto fail;
		}
		rc = TE_BUF_MGR_GEN_MEMLIST_SRC(req->src, req->cryptlen,
						&req_ctx->xts_req.src);
		if (rc != 0) {
			goto fail;
		}
		rc = TE_BUF_MGR_GEN_MEMLIST_DST(req->dst, req->cryptlen,
						&req_ctx->xts_req.dst);
		if (rc != 0)
			goto fail1;

		memcpy(req_ctx->xts_req.data_unit, req->iv,
		       crypto_skcipher_ivsize(tfm));
		req_ctx->xts_req.op = TE_DRV_SCA_DECRYPT;
		req_ctx->xts_req.base.completion = lca_te_cipher_complete;
		req_ctx->xts_req.base.flags = req->base.flags;
		req_ctx->xts_req.base.data = req;
	} else {
		memset(&req_ctx->cph_req, 0, sizeof(req_ctx->cph_req));
		if (req->iv) {
			req_ctx->cph_req.iv = req->iv;
			req_ctx->cph_req.stream = NULL;
			req_ctx->cph_req.off = NULL;
		}
		rc = TE_BUF_MGR_GEN_MEMLIST_SRC(req->src, req->cryptlen,
						&req_ctx->cph_req.src);
		if (rc != 0) {
			goto fail;
		}
		rc = TE_BUF_MGR_GEN_MEMLIST_DST(req->dst, req->cryptlen,
						&req_ctx->cph_req.dst);
		if (rc != 0)
			goto fail1;

		req_ctx->cph_req.op = TE_DRV_SCA_DECRYPT;
		req_ctx->cph_req.base.completion = lca_te_cipher_complete;
		req_ctx->cph_req.base.flags = req->base.flags;
		req_ctx->cph_req.base.data = req;
	}

	switch (TE_ALG_GET_CHAIN_MODE(ctx_p->alg)) {
	case TE_CHAIN_MODE_XTS:
		rc = te_xts_acrypt(req_ctx->xctx, &req_ctx->xts_req);
		break;
	case TE_CHAIN_MODE_ECB_NOPAD:
		rc = te_cipher_aecb(req_ctx->cctx, &req_ctx->cph_req);
		break;
	case TE_CHAIN_MODE_CBC_NOPAD:
		rc = te_cipher_acbc(req_ctx->cctx, &req_ctx->cph_req);
		break;
	case TE_CHAIN_MODE_CTR:
		rc = te_cipher_actr(req_ctx->cctx, &req_ctx->cph_req);
		break;
	case TE_CHAIN_MODE_OFB:
		rc = te_cipher_aofb(req_ctx->cctx, &req_ctx->cph_req);
		break;
	default:
		dev_err(dev, "unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
		rc = -EINVAL;
		goto fail2;
	}

	if (rc != TE_SUCCESS) {
		dev_err(dev, "te_acrypt dec algo (0x%x) ret:0x%x\n",
			ctx_p->alg, rc);
		goto err_drv;
	}

	return -EINPROGRESS;

err_drv:
	rc = TE2ERRNO(rc);
fail2:
	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		te_buf_mgr_free_memlist(&req_ctx->xts_req.dst, rc);
	} else {
		te_buf_mgr_free_memlist(&req_ctx->cph_req.dst, rc);
	}
fail1:
	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		te_buf_mgr_free_memlist(&req_ctx->xts_req.src, rc);
	} else {
		te_buf_mgr_free_memlist(&req_ctx->cph_req.src, rc);
	}
fail:
	/* Release the driver ctx */
	lca_te_ctx_put(ctx_p->cgov, req_ctx->bctx);
	req_ctx->bctx = NULL;
out:
	pm_runtime_put_autosuspend(dev);
	return rc;
}

static int lca_te_cipher_decrypt(struct skcipher_request *req)
{
	return lca_te_cipher_request(req, lca_te_cipher_do_decrypt);
}

/* Skcipher template */
static const struct te_alg_template skcipher_algs[] = {
	{
		.name = "xts(aes)",
		.driver_name = "xts-aes-te",
		.blocksize = AES_BLOCK_SIZE,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = AES_XTS_KEY_MIN_SIZE,
			.max_keysize = AES_XTS_KEY_MAX_SIZE,
			.ivsize = AES_BLOCK_SIZE,
			},
		.alg = TE_ALG_AES_XTS,
	},
	{
		.name = "xts(sm4)",
		.driver_name = "xts-sm4-te",
		.blocksize = AES_BLOCK_SIZE,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = SM4_XTS_KEY_SIZE,
			.max_keysize = SM4_XTS_KEY_SIZE,
			.ivsize = SM4_BLOCK_SIZE,
			},
		.alg = TE_ALG_SM4_XTS,
	},
	{
		.name = "ctr(aes)",
		.driver_name = "ctr-aes-te",
		.blocksize = 1,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
			},
		.alg = TE_ALG_AES_CTR,
	},
	{
		.name = "ctr(sm4)",
		.driver_name = "ctr-sm4-te",
		.blocksize = 1,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = SM4_KEY_SIZE,
			.max_keysize = SM4_KEY_SIZE,
			.ivsize = SM4_BLOCK_SIZE,
			},
		.alg = TE_ALG_SM4_CTR,
	},
	{
		.name = "ofb(aes)",
		.driver_name = "ofb-aes-te",
		.blocksize = 1,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
			},
		.alg = TE_ALG_AES_OFB,
	},
	{
		.name = "ofb(sm4)",
		.driver_name = "ofb-sm4-te",
		.blocksize = 1,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = SM4_KEY_SIZE,
			.max_keysize = SM4_KEY_SIZE,
			.ivsize = SM4_BLOCK_SIZE,
			},
		.alg = TE_ALG_SM4_OFB,
	},
	{
		.name = "cbc(aes)",
		.driver_name = "cbc-aes-te",
		.blocksize = AES_BLOCK_SIZE,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
			},
		.alg = TE_ALG_AES_CBC_NOPAD,
	},
	{
		.name = "cbc(sm4)",
		.driver_name = "cbc-sm4-te",
		.blocksize = SM4_BLOCK_SIZE,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = SM4_KEY_SIZE,
			.max_keysize = SM4_KEY_SIZE,
			.ivsize = SM4_BLOCK_SIZE,
			},
		.alg = TE_ALG_SM4_CBC_NOPAD,
	},
	{
		.name = "ecb(aes)",
		.driver_name = "ecb-aes-te",
		.blocksize = AES_BLOCK_SIZE,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = 0,
			},
		.alg = TE_ALG_AES_ECB_NOPAD,
	},
	{
		.name = "ecb(sm4)",
		.driver_name = "ecb-sm4-te",
		.blocksize = SM4_BLOCK_SIZE,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = SM4_KEY_SIZE,
			.max_keysize = SM4_KEY_SIZE,
			.ivsize = 0,
			},
		.alg = TE_ALG_SM4_ECB_NOPAD,
	},
	{
		.name = "cbc(des)",
		.driver_name = "cbc-des-te",
		.blocksize = DES_BLOCK_SIZE,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = DES_KEY_SIZE,
			.max_keysize = DES_KEY_SIZE,
			.ivsize = DES_BLOCK_SIZE,
			},
		.alg = TE_ALG_DES_CBC_NOPAD,
	},
	{
		.name = "cbc(des3_ede)",
		.driver_name = "cbc-3des-te",
		.blocksize = DES3_EDE_BLOCK_SIZE,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = DES3_EDE_KEY_SIZE,
			.max_keysize = DES3_EDE_KEY_SIZE,
			.ivsize = DES3_EDE_BLOCK_SIZE,
			},
		.alg = TE_ALG_TDES_CBC_NOPAD,
	},
	{
		.name = "ecb(des)",
		.driver_name = "ecb-des-te",
		.blocksize = DES_BLOCK_SIZE,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = DES_KEY_SIZE,
			.max_keysize = DES_KEY_SIZE,
			.ivsize = 0,
			},
		.alg = TE_ALG_DES_ECB_NOPAD,
	},
	{
		.name = "ecb(des3_ede)",
		.driver_name = "ecb-3des-te",
		.blocksize = DES3_EDE_BLOCK_SIZE,
		.template_skcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = DES3_EDE_KEY_SIZE,
			.max_keysize = DES3_EDE_KEY_SIZE,
			.ivsize = 0,
			},
		.alg = TE_ALG_TDES_ECB_NOPAD,
	},
};

static struct te_crypto_alg *te_create_alg(const struct te_alg_template *tmpl)
{
	struct te_crypto_alg *t_alg;
	struct skcipher_alg *alg;

	t_alg = kzalloc(sizeof(*t_alg), GFP_KERNEL);
	if (!t_alg)
		return ERR_PTR(-ENOMEM);

	alg = &t_alg->skcipher_alg;

	memcpy(alg, &tmpl->template_skcipher, sizeof(*alg));
	snprintf(alg->base.cra_name, CRYPTO_MAX_ALG_NAME, "%s", tmpl->name);
	snprintf(alg->base.cra_driver_name, CRYPTO_MAX_ALG_NAME, "%s",
		 tmpl->driver_name);
	alg->base.cra_module = THIS_MODULE;
	alg->base.cra_priority = TE_CRA_PRIO;
	alg->base.cra_blocksize = tmpl->blocksize;
	alg->base.cra_alignmask = 0;
	alg->base.cra_ctxsize = sizeof(struct lca_te_cipher_ctx);

	alg->base.cra_init = lca_cipher_init;
	alg->base.cra_exit = lca_cipher_exit;
	alg->base.cra_flags = CRYPTO_ALG_ASYNC;
	t_alg->alg = tmpl->alg;
	t_alg->data_unit = tmpl->data_unit;

	return t_alg;
}

int lca_te_cipher_free(struct te_drvdata *drvdata)
{
	struct te_crypto_alg *t_alg, *n;
	struct te_cipher_handle *cipher_handle = drvdata->cipher_handle;

	if (cipher_handle) {
		/* free request agent */
		lca_te_ra_free(cipher_handle->ra);
		cipher_handle->ra = NULL;

		/* Remove registered algs */
		list_for_each_entry_safe(t_alg, n, &cipher_handle->alg_list,
					 entry) {
			crypto_unregister_skcipher(&t_alg->skcipher_alg);
			list_del(&t_alg->entry);
			kfree(t_alg);
		}
		kfree(cipher_handle);
		drvdata->cipher_handle = NULL;
	}
	return 0;
}

int lca_te_cipher_alloc(struct te_drvdata *drvdata)
{
	struct te_cipher_handle *cipher_handle;
	struct te_crypto_alg *t_alg;
	struct device *dev = drvdata_to_dev(drvdata);
	int rc = -ENOMEM;
	int alg;

	cipher_handle = kmalloc(sizeof(*cipher_handle), GFP_KERNEL);
	if (!cipher_handle)
		return -ENOMEM;

	INIT_LIST_HEAD(&cipher_handle->alg_list);
	drvdata->cipher_handle = cipher_handle;
	/* create request agent */
	rc = lca_te_ra_alloc(&cipher_handle->ra, "skcipher");
	if (rc != 0) {
		dev_err(dev,"skcipher alloc ra failed %d\n", rc);
		goto fail0;
	}

	/* Linux crypto */
	dev_dbg(dev, "number of algorithms = %zu\n", ARRAY_SIZE(skcipher_algs));
	for (alg = 0; alg < ARRAY_SIZE(skcipher_algs); alg++) {
		dev_dbg(dev, "creating %s\n", skcipher_algs[alg].driver_name);
		t_alg = te_create_alg(&skcipher_algs[alg]);
		if (IS_ERR(t_alg)) {
			rc = PTR_ERR(t_alg);
			dev_err(dev, "%s alg allocation failed\n",
				skcipher_algs[alg].driver_name);
			goto fail0;
		}
		t_alg->drvdata = drvdata;

		dev_dbg(dev, "registering %s\n",
			skcipher_algs[alg].driver_name);
		rc = crypto_register_skcipher(&t_alg->skcipher_alg);
		dev_dbg(dev, "%s alg registration rc = %x\n",
			t_alg->skcipher_alg.base.cra_driver_name, rc);
		if (rc) {
			dev_err(dev, "%s alg registration failed\n",
				t_alg->skcipher_alg.base.cra_driver_name);
			kfree(t_alg);
			goto fail0;
		} else {
			list_add_tail(&t_alg->entry,
					&cipher_handle->alg_list);
			dev_dbg(dev, "registered %s\n",
				t_alg->skcipher_alg.base.cra_driver_name);
		}
	}
	return 0;

fail0:
	lca_te_cipher_free(drvdata);
	return rc;
}
