//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <crypto/algapi.h>
#include <crypto/internal/skcipher.h>
#include <crypto/internal/hash.h>
#include <crypto/internal/aead.h>
#include <crypto/scatterwalk.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#include <crypto/ctr.h>
#include <crypto/authenc.h>
#include <crypto/aes.h>
#include <crypto/des.h>
#include <linux/version.h>

#include "lca_te_aead.h"
#include "lca_te_buf_mgr.h"
#include "lca_te_ctx.h"
#include "lca_te_ra.h"
#include "te_gcm.h"
#include "te_ccm.h"

/**
 * AEAD Driver Context Management Introduction.
 *
 * The aead driver is optimized to promote parallel operation performance for
 * single-tfm-multi-thread and multi-tfm-multi-thread scenarios. The ideas are:
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

#define template_aead	template_u.aead

#define AEAD_MAX_AUTH_SIZE        (16)

struct te_aead_req_ctx {
	struct te_request base;          /**< Must put it in the beginning */
	te_sca_operation_t op;
	u8 auth[AEAD_MAX_AUTH_SIZE];
	struct scatterlist src[2];
	struct scatterlist dst[2];
	union {
		te_gcm_ctx_t *gctx;
		te_ccm_ctx_t *cctx;
		te_base_ctx_t *bctx;
	};
	union {
		te_gcm_request_t gcm_req;
		te_ccm_request_t ccm_req;
	};
};

struct te_aead_handle {
	lca_te_ra_handle ra;
	struct list_head aead_list;
};

struct te_aead_ctx {
	struct te_drvdata *drvdata;
	struct lca_te_drv_ctx_gov *cgov; /**< Driver context governor */
	te_algo_t alg;
	unsigned int authsize;
	union {
		te_ccm_ctx_t cctx;
		te_gcm_ctx_t gctx;
	};
};

/**
 * te_gcm_init() fallback: initialize a gcm ctx.
 *
 * The te_gcm_init() has the following drawback:
 * D1: accept main algorithm only.
 */
TE_DRV_INIT_FALLBACK(gcm)

/**
 * te_ccm_init() fallback: initialize a ccm ctx.
 *
 * The te_ccm_init() has the following drawback:
 * D1: accept main algorithm only.
 */
TE_DRV_INIT_FALLBACK(ccm)

static int te_aead_init(struct crypto_aead *tfm)
{
	int rc = 0;
	struct aead_alg *alg = crypto_aead_alg(tfm);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct te_crypto_alg *te_alg =
			container_of(alg, struct te_crypto_alg, aead_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);
	struct lca_te_drv_ctx_gov *cgov = NULL;
	union {
		LCA_TE_DRV_OPS_DEF(gcm, gcm);
		LCA_TE_DRV_OPS_DEF(ccm, ccm);
		lca_te_base_ops_t base;
	} drv_ops = {0};

	dev_dbg(dev, "initializing context @%p for %s driver:%s\n", ctx,
		crypto_tfm_alg_name(&tfm->base),
		crypto_tfm_alg_driver_name(&tfm->base));

	/* Initialize modes in instance */
	ctx->alg = te_alg->alg;
	ctx->drvdata = te_alg->drvdata;
	crypto_aead_set_reqsize(tfm, sizeof(struct te_aead_req_ctx));
	switch (TE_ALG_GET_CHAIN_MODE(ctx->alg)) {
	case TE_CHAIN_MODE_GCM:
		drv_ops.gcm.init   = TE_INIT_FALLBACK_FN(gcm);
		drv_ops.gcm.free   = te_gcm_free;
		drv_ops.gcm.clone  = te_gcm_clone;
		drv_ops.gcm.setkey = te_gcm_setkey;
		break;
	case TE_CHAIN_MODE_CCM:
		drv_ops.ccm.init   = TE_INIT_FALLBACK_FN(ccm);
		drv_ops.ccm.free   = te_ccm_free;
		drv_ops.ccm.clone  = te_ccm_clone;
		drv_ops.ccm.setkey = te_ccm_setkey;
		break;
	default:
		dev_err(dev, "unsupported cipher mode (%d)\n",
			TE_ALG_GET_CHAIN_MODE(ctx->alg));
		rc = -EINVAL;
		goto out;
	}

	pm_runtime_get_sync(dev);
	cgov = lca_te_ctx_alloc_gov(&drv_ops.base, ctx->drvdata->h,
				    ctx->alg, &tfm->base);
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
	return rc;
}

static void te_aead_exit(struct crypto_aead *tfm)
{
	struct aead_alg *alg = crypto_aead_alg(tfm);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct te_crypto_alg *te_alg =
			container_of(alg, struct te_crypto_alg, aead_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	dev_dbg(dev, "clearing context @%p for %s\n",
		crypto_aead_ctx(tfm), crypto_tfm_alg_name(&tfm->base));

	pm_runtime_get_sync(dev);
	lca_te_ctx_free_gov(ctx->cgov);
	memset(ctx, 0, sizeof(*ctx));
	pm_runtime_put_autosuspend(dev);
}

static int
te_aead_setkey(struct crypto_aead *tfm, const u8 *key, unsigned int keylen)
{
	int rc = -1;
	struct aead_alg *alg = crypto_aead_alg(tfm);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct te_crypto_alg *te_alg =
			container_of(alg, struct te_crypto_alg, aead_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	pm_runtime_get_sync(dev);

	rc = lca_te_ctx_setkey(ctx->cgov, key, keylen);
	if (rc != 0) {
		dev_err(dev, "lca_te_ctx_setkey algo (0x%x) ret:%d\n",
			ctx->alg, rc);
	}

	pm_runtime_put_autosuspend(dev);
	return rc;
}

static int te_aead_setauthsize(struct crypto_aead *tfm, unsigned int authsize)
{
	struct aead_alg *alg = crypto_aead_alg(tfm);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct te_crypto_alg *te_alg =
			container_of(alg, struct te_crypto_alg, aead_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	/* Unsupported authsize */
	if ((authsize > AEAD_MAX_AUTH_SIZE) ||
	    (authsize > crypto_aead_maxauthsize(tfm))) {
		return -EINVAL;
	}

	ctx->authsize = authsize;
	dev_dbg(dev, "authsize %d algo (0x%x)\n", ctx->authsize, ctx->alg);
	return 0;
}

static int te_gcm_setauthsize(struct crypto_aead *tfm, unsigned int authsize)
{
	switch (authsize) {
	case 4:
	case 8:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
		break;
	default:
		return -EINVAL;
	}

	return te_aead_setauthsize(tfm, authsize);
}

static int te_ccm_setauthsize(struct crypto_aead *tfm, unsigned int authsize)
{
	switch (authsize) {
	case 4:
	case 6:
	case 8:
	case 10:
	case 12:
	case 14:
	case 16:
		break;
	default:
		return -EINVAL;
	}

	return te_aead_setauthsize(tfm, authsize);
}

static int te_aead_gen_memlist(struct te_aead_req_ctx *areq_ctx,
			       unsigned int mode,
			       struct scatterlist *src,
			       struct scatterlist *dst,
			       unsigned int cryptlen)
{
	int rc = 0;
	te_memlist_t *in, *out, *aad;
	struct aead_request *req =
		container_of((void*(*)[])areq_ctx, struct aead_request, __ctx);

	if (TE_CHAIN_MODE_GCM == mode) {
		in  = &areq_ctx->gcm_req.crypt.in;
		out = &areq_ctx->gcm_req.crypt.out;
		aad = &areq_ctx->gcm_req.crypt.aad;
	} else if (TE_CHAIN_MODE_CCM == mode) {
		in  = &areq_ctx->ccm_req.crypt.in;
		out = &areq_ctx->ccm_req.crypt.out;
		aad = &areq_ctx->ccm_req.crypt.aad;
	} else {
		return -EINVAL;
	}

	rc = TE_BUF_MGR_GEN_MEMLIST_SRC(src, cryptlen, in);
	if (rc != 0) {
		goto err;
	}
	rc = TE_BUF_MGR_GEN_MEMLIST_DST(dst, cryptlen, out);
	if (rc != 0) {
		te_buf_mgr_free_memlist(in, rc);
		goto err;
	}
	rc = TE_BUF_MGR_GEN_MEMLIST_SRC(req->src, req->assoclen, aad);
	if (rc != 0) {
		te_buf_mgr_free_memlist(in, rc);
		te_buf_mgr_free_memlist(out, rc);
		goto err;
	}

err:
	return rc;
}

static void te_aead_free_memlist(struct te_aead_req_ctx *areq_ctx,
				 unsigned int mode, int err)
{
	BUG_ON((mode != TE_CHAIN_MODE_GCM) && (mode != TE_CHAIN_MODE_CCM));
	if (TE_CHAIN_MODE_GCM == mode) {
		te_buf_mgr_free_memlist(&areq_ctx->gcm_req.crypt.in, err);
		te_buf_mgr_free_memlist(&areq_ctx->gcm_req.crypt.out, err);
		te_buf_mgr_free_memlist(&areq_ctx->gcm_req.crypt.aad, err);
	} else {
		te_buf_mgr_free_memlist(&areq_ctx->ccm_req.crypt.in, err);
		te_buf_mgr_free_memlist(&areq_ctx->ccm_req.crypt.out, err);
		te_buf_mgr_free_memlist(&areq_ctx->ccm_req.crypt.aad, err);
	}
}

static void te_aead_complete(struct te_async_request *te_req, int err)
{
	struct aead_request *req = (struct aead_request *)te_req->data;
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct te_aead_req_ctx *areq_ctx = aead_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_aead_handle *ahdl = ctx->drvdata->aead_handle;
	int e = TE2ERRNO(err);

	if(TE_DRV_SCA_ENCRYPT == areq_ctx->op) {
		/* Copy auth_tag from buf to sg */
		scatterwalk_map_and_copy(areq_ctx->auth, req->dst,
					 req->assoclen + req->cryptlen,
					 ctx->authsize, 1);
	}

	te_aead_free_memlist(areq_ctx, TE_ALG_GET_CHAIN_MODE(ctx->alg), e);

	/* Release the driver ctx */
	lca_te_ctx_put(ctx->cgov, areq_ctx->bctx);
	areq_ctx->bctx = NULL;
	LCA_TE_RA_COMPLETE(ahdl->ra, aead_request_complete, req, e);
	pm_runtime_put_autosuspend(dev);
}

static int te_gcm_do_request(struct aead_request *req,
			     struct scatterlist *src,
			     struct scatterlist *dst)
{
	int rc = 0;
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_aead_req_ctx *areq_ctx = aead_request_ctx(req);
	unsigned int crylen = 0;

	memset(&areq_ctx->gcm_req, 0, sizeof(areq_ctx->gcm_req));
	areq_ctx->gcm_req.base.flags = req->base.flags;
	areq_ctx->gcm_req.base.data = req;
	areq_ctx->gcm_req.base.completion = te_aead_complete;
	areq_ctx->gcm_req.crypt.taglen = ctx->authsize;
	areq_ctx->gcm_req.crypt.tag = areq_ctx->auth;
	areq_ctx->gcm_req.crypt.op = areq_ctx->op;
	areq_ctx->gcm_req.crypt.iv = req->iv;
	areq_ctx->gcm_req.crypt.ivlen = crypto_aead_ivsize(tfm);

	crylen = (TE_DRV_SCA_ENCRYPT == areq_ctx->op) ? req->cryptlen :
		   (req->cryptlen - ctx->authsize);
	rc = te_aead_gen_memlist(areq_ctx, TE_CHAIN_MODE_GCM, src, dst, crylen);
	if (rc != 0) {
		goto out;
	}

	rc = te_gcm_acrypt(areq_ctx->gctx, &areq_ctx->gcm_req);
	if (rc != TE_SUCCESS) {
		dev_err(dev, "te_gcm_acrypt %s algo (0x%x) ret:0x%x\n",
			(TE_DRV_SCA_ENCRYPT == areq_ctx->op) ? "enc" : "dec",
			ctx->alg, rc);
		goto err_drv;
	}

	rc = 0;
	goto out;

err_drv:
	rc = TE2ERRNO(rc);
	te_aead_free_memlist(areq_ctx, TE_CHAIN_MODE_GCM, rc);
out:
	return rc;
}

/* Taken from crypto/ccm.c */
static inline int crypto_ccm_check_iv(const u8 *iv)
{
	/* 2 <= L <= 8, so 1 <= L' <= 7. */
	if (1 > iv[0] || iv[0] > 7)
		return -EINVAL;

	return 0;
}

static int te_ccm_do_request(struct aead_request *req,
			     struct scatterlist *src,
			     struct scatterlist *dst)
{
	int rc = 0;
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_aead_req_ctx *areq_ctx = aead_request_ctx(req);
	unsigned int l = 0;
	unsigned int crylen = 0;

	rc = crypto_ccm_check_iv(req->iv);
	if (rc != 0) {
		goto out;
	}

	/* L = L' + 1 */
	l = req->iv[0] + 1;
	memset(&areq_ctx->ccm_req, 0, sizeof(areq_ctx->ccm_req));
	areq_ctx->ccm_req.base.flags = req->base.flags;
	areq_ctx->ccm_req.base.data = req;
	areq_ctx->ccm_req.base.completion = te_aead_complete;
	areq_ctx->ccm_req.crypt.taglen = ctx->authsize;
	areq_ctx->ccm_req.crypt.tag = areq_ctx->auth;
	areq_ctx->ccm_req.crypt.op = areq_ctx->op;
	areq_ctx->ccm_req.crypt.nonce = req->iv + 1; /* Offset iv[0] */
	/* Exclude iv[0] and length field */
	areq_ctx->ccm_req.crypt.nlen = crypto_aead_ivsize(tfm) - l - 1;

	crylen = (TE_DRV_SCA_ENCRYPT == areq_ctx->op) ? req->cryptlen :
		   (req->cryptlen - ctx->authsize);
	rc = te_aead_gen_memlist(areq_ctx, TE_CHAIN_MODE_CCM, src, dst, crylen);
	if (rc != 0) {
		goto out;
	}

	rc = te_ccm_acrypt(areq_ctx->cctx, &areq_ctx->ccm_req);
	if (rc != TE_SUCCESS) {
		dev_err(dev, "te_ccm_acrypt %s algo (0x%x) ret:0x%x\n",
			(TE_DRV_SCA_ENCRYPT == areq_ctx->op) ? "enc" : "dec",
			ctx->alg, rc);
		goto err_drv;
	}

	rc = 0;
	goto out;

err_drv:
	rc = TE2ERRNO(rc);
	te_aead_free_memlist(areq_ctx, TE_CHAIN_MODE_CCM, rc);
out:
	return rc;
}

/*
 * https://www.kernel.org/doc/html/v4.14/crypto/api-aead.html#c.aead_request_set_crypt
 * The memory structure for cipher operation has the following structure:
 * AEAD encryption input:  assoc data || plaintext
 * AEAD encryption output: assoc data || cipherntext || auth tag
 * AEAD decryption input:  assoc data || ciphertext  || auth tag
 * AEAD decryption output: assoc data || plaintext
 */
static int te_aead_do_request(struct aead_request *req)
{
	int rc = -1;
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_aead_req_ctx *areq_ctx = aead_request_ctx(req);
	struct scatterlist *src, *dst;

	memset(areq_ctx->src, 0, sizeof(areq_ctx->src));
	memset(areq_ctx->dst, 0, sizeof(areq_ctx->dst));
	src = scatterwalk_ffwd(areq_ctx->src, req->src, req->assoclen);
	dst = src;

	if (req->src != req->dst) {
		dst = scatterwalk_ffwd(areq_ctx->dst, req->dst, req->assoclen);
	}

	/* Get one driver ctx for the request */
	rc = lca_te_ctx_get(ctx->cgov, &areq_ctx->bctx, (void *)req, false);
	if (rc != 0) {
		dev_err(dev, "lca_te_ctx_get algo (0x%x) ret:%d\n",
		ctx->alg, rc);
		goto out;
	}
	BUG_ON(areq_ctx->bctx == NULL);

	switch (TE_ALG_GET_CHAIN_MODE(ctx->alg)) {
	case TE_CHAIN_MODE_GCM:
		rc = te_gcm_do_request(req, src, dst);
		break;
	case TE_CHAIN_MODE_CCM:
		rc = te_ccm_do_request(req, src, dst);
		break;
	default:
		dev_err(dev, "unsupported cipher mode (%d)\n",
			TE_ALG_GET_CHAIN_MODE(ctx->alg));
		goto err;
	}

	if (rc != 0) {
		goto err;
	}

	return -EINPROGRESS;

err:
	/* Release the driver ctx */
	lca_te_ctx_put(ctx->cgov, areq_ctx->bctx);
	areq_ctx->bctx = NULL;
out:
	return rc;
}

static int te_aead_do_encrypt(struct te_request *treq)
{
	struct aead_request *req =
		container_of((void*(*)[])treq, struct aead_request, __ctx);
	struct te_aead_req_ctx *areq_ctx = aead_request_ctx(req);

	/* Don't zeroize areq_ctx->base */
	areq_ctx->op = TE_DRV_SCA_ENCRYPT;
	return te_aead_do_request(req);
}

static int te_aead_request(struct aead_request *req, lca_te_submit_t fn)
{
	int rc = 0;
	struct te_request *treq = aead_request_ctx(req);
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_aead_handle *chdl = ctx->drvdata->aead_handle;

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

static int te_aead_encrypt(struct aead_request *req)
{
	return te_aead_request(req, te_aead_do_encrypt);
}

static int te_aead_do_decrypt(struct te_request *treq)
{
	struct aead_request *req =
		container_of((void*(*)[])treq, struct aead_request, __ctx);
	struct te_aead_ctx *ctx = crypto_tfm_ctx(req->base.tfm);
	struct te_aead_req_ctx *areq_ctx = aead_request_ctx(req);

	/* Don't zeroize areq_ctx->base */
	memset(areq_ctx->auth, 0, sizeof(areq_ctx->auth));
	areq_ctx->op = TE_DRV_SCA_DECRYPT;

	/* Copy auth_tag from sg to buf */
	scatterwalk_map_and_copy(areq_ctx->auth, req->src,
				 req->assoclen + req->cryptlen - ctx->authsize,
				 ctx->authsize, 0);

	return te_aead_do_request(req);
}

static int te_aead_decrypt(struct aead_request *req)
{
	return te_aead_request(req, te_aead_do_decrypt);
}

/* TE Block aead alg */
static struct te_alg_template aead_algs[] = {
	{
		.name = "ccm(aes)",
		.driver_name = "ccm-aes-te",
		.blocksize = 1,
		.type = CRYPTO_ALG_TYPE_AEAD,
		.template_aead = {
			.setkey = te_aead_setkey,
			.setauthsize = te_ccm_setauthsize,
			.encrypt = te_aead_encrypt,
			.decrypt = te_aead_decrypt,
			.ivsize = AES_BLOCK_SIZE,
			.maxauthsize = AES_BLOCK_SIZE,
		},
		.alg = TE_ALG_AES_CCM,
	},
	{
		.name = "ccm(sm4)",
		.driver_name = "ccm-sm4-te",
		.blocksize = 1,
		.type = CRYPTO_ALG_TYPE_AEAD,
		.template_aead = {
			.setkey = te_aead_setkey,
			.setauthsize = te_ccm_setauthsize,
			.encrypt = te_aead_encrypt,
			.decrypt = te_aead_decrypt,
			.ivsize = SM4_BLOCK_SIZE,
			.maxauthsize = SM4_BLOCK_SIZE,
		},
		.alg = TE_ALG_SM4_CCM,
	},
	{
		.name = "gcm(aes)",
		.driver_name = "gcm-aes-te",
		.blocksize = 1,
		.type = CRYPTO_ALG_TYPE_AEAD,
		.template_aead = {
			.setkey = te_aead_setkey,
			.setauthsize = te_gcm_setauthsize,
			.encrypt = te_aead_encrypt,
			.decrypt = te_aead_decrypt,
			.ivsize = 12,
			.maxauthsize = AES_BLOCK_SIZE,
		},
		.alg = TE_ALG_AES_GCM,
	},
	{
		.name = "gcm(sm4)",
		.driver_name = "gcm-sm4-te",
		.blocksize = 1,
		.type = CRYPTO_ALG_TYPE_AEAD,
		.template_aead = {
			.setkey = te_aead_setkey,
			.setauthsize = te_gcm_setauthsize,
			.encrypt = te_aead_encrypt,
			.decrypt = te_aead_decrypt,
			.ivsize = 12,
			.maxauthsize = SM4_BLOCK_SIZE,
		},
		.alg = TE_ALG_SM4_GCM,
	},
};

static struct te_crypto_alg *te_aead_create_alg(struct te_alg_template *tmpl)
{
	struct te_crypto_alg *t_alg;
	struct aead_alg *alg;

	t_alg = kzalloc(sizeof(*t_alg), GFP_KERNEL);
	if (!t_alg) {
		return ERR_PTR(-ENOMEM);
	}
	alg = &tmpl->template_aead;

	snprintf(alg->base.cra_name, CRYPTO_MAX_ALG_NAME, "%s", tmpl->name);
	snprintf(alg->base.cra_driver_name, CRYPTO_MAX_ALG_NAME, "%s",
		 tmpl->driver_name);
	alg->base.cra_module = THIS_MODULE;
	alg->base.cra_priority = TE_CRA_PRIO;
	alg->base.cra_blocksize = tmpl->blocksize;
	alg->base.cra_ctxsize = sizeof(struct te_aead_ctx);
	alg->base.cra_flags = CRYPTO_ALG_ASYNC | tmpl->type;
	alg->init = te_aead_init;
	alg->exit = te_aead_exit;

	t_alg->aead_alg = *alg;
	t_alg->alg = tmpl->alg;

	return t_alg;
}

int lca_te_aead_free(struct te_drvdata *drvdata)
{
	struct te_crypto_alg *t_alg, *n;
	struct te_aead_handle *aead_handle =
		(struct te_aead_handle *)drvdata->aead_handle;

	if (aead_handle) {
		/* free request agent */
		lca_te_ra_free(aead_handle->ra);
		aead_handle->ra = NULL;

		/* Remove registered algs */
		list_for_each_entry_safe(t_alg, n, &aead_handle->aead_list, entry) {
			crypto_unregister_aead(&t_alg->aead_alg);
			list_del(&t_alg->entry);
			kfree(t_alg);
		}
		kfree(aead_handle);
		drvdata->aead_handle = NULL;
	}

	return 0;
}

int lca_te_aead_alloc(struct te_drvdata *drvdata)
{
	struct te_aead_handle *aead_handle;
	struct te_crypto_alg *t_alg = NULL;
	struct device *dev = drvdata_to_dev(drvdata);
	int rc = -ENOMEM;
	int alg;

	aead_handle = kmalloc(sizeof(*aead_handle), GFP_KERNEL);
	if (!aead_handle) {
		rc = -ENOMEM;
		goto fail0;
	}

	drvdata->aead_handle = aead_handle;
	INIT_LIST_HEAD(&aead_handle->aead_list);
	/* create request agent */
	rc = lca_te_ra_alloc(&aead_handle->ra, "aead");
	if (rc != 0) {
		dev_err(dev,"aead alloc ra failed %d\n", rc);
		goto fail1;
	}

	/* Linux crypto */
	for (alg = 0; alg < ARRAY_SIZE(aead_algs); alg++) {
		t_alg = te_aead_create_alg(&aead_algs[alg]);
		if (IS_ERR(t_alg)) {
			rc = PTR_ERR(t_alg);
			dev_err(dev, "%s alg allocation failed\n",
				aead_algs[alg].driver_name);
			goto fail1;
		}
		t_alg->drvdata = drvdata;
		rc = crypto_register_aead(&t_alg->aead_alg);
		if (unlikely(rc != 0)) {
			dev_err(dev, "%s alg registration failed\n",
				t_alg->aead_alg.base.cra_driver_name);
			goto fail2;
		} else {
			list_add_tail(&t_alg->entry, &aead_handle->aead_list);
			dev_dbg(dev, "registered %s\n",
				t_alg->aead_alg.base.cra_driver_name);
		}
	}

	return 0;

fail2:
	kfree(t_alg);
	t_alg = NULL;
fail1:
	lca_te_aead_free(drvdata);
fail0:
	return rc;
}
