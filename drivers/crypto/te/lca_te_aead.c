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

#include "lca_te_driver.h"
#include "lca_te_aead.h"
#include "lca_te_buf_mgr.h"
#include "te_gcm.h"
#include "te_ccm.h"

#define template_aead	template_u.aead

#ifdef CFG_TE_ASYNC_EN
struct te_aead_req_ctx {
	te_sca_operation_t op;
	u8 *auth;
	struct scatterlist src[2];
	struct scatterlist dst[2];
	union {
		te_gcm_request_t *te_gcm_req;
		te_ccm_request_t *te_ccm_req;
	}enc;
	union {
		te_gcm_request_t *te_gcm_req;
		te_ccm_request_t *te_ccm_req;
	}dec;
};
#endif

struct te_aead_handle {
	struct list_head aead_list;
};

struct te_aead_ctx {
	struct te_drvdata *drvdata;
	te_algo_t alg;
	unsigned int authsize;
	union {
		te_ccm_ctx_t cctx;
		te_gcm_ctx_t gctx;
	};
};


static void te_aead_exit(struct crypto_aead *tfm)
{
	int rc = 0;
	struct aead_alg *alg = crypto_aead_alg(tfm);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct te_crypto_alg *te_alg =
			container_of(alg, struct te_crypto_alg, aead_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	dev_dbg(dev, "Clearing context @%p for %s\n",
			  crypto_aead_ctx(tfm), crypto_tfm_alg_name(&tfm->base));

	pm_runtime_get_sync(dev);

	switch (TE_ALG_GET_CHAIN_MODE(ctx->alg)) {
	case TE_CHAIN_MODE_GCM:
		rc = te_gcm_free(&ctx->gctx);
		break;
	case TE_CHAIN_MODE_CCM:
		rc = te_ccm_free(&ctx->cctx);
		break;
	default:
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			TE_ALG_GET_CHAIN_MODE(ctx->alg));
	}

	pm_runtime_put_autosuspend(dev);

	return ;
}

static int te_aead_init(struct crypto_aead *tfm)
{
	int rc = -1;
	struct aead_alg *alg = crypto_aead_alg(tfm);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct te_crypto_alg *te_alg =
			container_of(alg, struct te_crypto_alg, aead_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);
	dev_dbg(dev, "Initializing context @%p for %s driver:%s\n", ctx,
		crypto_tfm_alg_name(&tfm->base), crypto_tfm_alg_driver_name(&tfm->base));

	pm_runtime_get_sync(dev);
	/* Initialize modes in instance */
	ctx->alg = te_alg->alg;
	ctx->drvdata = te_alg->drvdata;
#ifdef CFG_TE_ASYNC_EN
	crypto_aead_set_reqsize(tfm, sizeof(struct te_aead_req_ctx));
#endif
	switch (TE_ALG_GET_CHAIN_MODE(ctx->alg)) {
	case TE_CHAIN_MODE_GCM:
		rc = te_gcm_init(&ctx->gctx, ctx->drvdata->h, TE_ALG_GET_MAIN_ALG(ctx->alg));
		break;
	case TE_CHAIN_MODE_CCM:
		rc = te_ccm_init(&ctx->cctx, ctx->drvdata->h, TE_ALG_GET_MAIN_ALG(ctx->alg));
		break;
	default:
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			TE_ALG_GET_CHAIN_MODE(ctx->alg));
	}

	pm_runtime_put_autosuspend(dev);
	return rc;
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

	switch (TE_ALG_GET_CHAIN_MODE(ctx->alg)) {
	case TE_CHAIN_MODE_GCM:
		rc = te_gcm_setkey(&ctx->gctx, key, keylen*BITS_IN_BYTE);
		break;
	case TE_CHAIN_MODE_CCM:
		rc = te_ccm_setkey(&ctx->cctx, key, keylen*BITS_IN_BYTE);
		break;
	default:
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			TE_ALG_GET_CHAIN_MODE(ctx->alg));
	}

	pm_runtime_put_autosuspend(dev);
	return rc;
}

static int te_aead_setauthsize(
	struct crypto_aead *tfm,
	unsigned int authsize)
{
	struct aead_alg *alg = crypto_aead_alg(tfm);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct te_crypto_alg *te_alg =
			container_of(alg, struct te_crypto_alg, aead_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);
	/* Unsupported auth. sizes */
	if ((authsize == 0) ||
		(authsize > crypto_aead_maxauthsize(tfm))) {
		return -ENOTSUPP;
	}

	ctx->authsize = authsize;
	dev_dbg(dev, "authlen=%d\n", ctx->authsize);

	return 0;
}

static int te_gcm_setauthsize(struct crypto_aead *tfm,
				   unsigned int authsize)
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

static int te_ccm_setauthsize(struct crypto_aead *tfm,
				   unsigned int authsize)
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

static void te_aead_complete(struct te_async_request *te_req, int err)
{
	struct aead_request *req = (struct aead_request *)te_req->data;
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct te_aead_ctx *ctx_p = crypto_aead_ctx(tfm);
	struct te_aead_req_ctx *areq_ctx = aead_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx_p->drvdata);

	if(err == TE_ERROR_SECURITY) {
		err = -EBADMSG;
	}

	if(areq_ctx->op == TE_DRV_SCA_ENCRYPT) {
		/*copy auth_tag from buf to sg*/
		scatterwalk_map_and_copy(areq_ctx->auth, req->dst,
					 req->assoclen + req->cryptlen,
					 ctx_p->authsize, 1);

		if (TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_GCM) {
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_gcm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_gcm_req->crypt.out);
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_gcm_req->crypt.aad);
			kfree(areq_ctx->enc.te_gcm_req);
			areq_ctx->enc.te_gcm_req = NULL;
		} else if (TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_CCM) {
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_ccm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_ccm_req->crypt.out);
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_ccm_req->crypt.aad);
			kfree(areq_ctx->enc.te_ccm_req);
			areq_ctx->enc.te_ccm_req = NULL;
		} else {
			dev_err(dev, "Invalid algo:0x%x, not support \n", ctx_p->alg);
		}
	} else {
		if (TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_GCM) {
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_gcm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_gcm_req->crypt.out);
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_gcm_req->crypt.aad);
			kfree(areq_ctx->dec.te_gcm_req);
			areq_ctx->dec.te_gcm_req = NULL;
		} else if (TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_CCM) {
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_ccm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_ccm_req->crypt.out);
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_ccm_req->crypt.aad);
			kfree(areq_ctx->dec.te_ccm_req);
			areq_ctx->dec.te_ccm_req = NULL;
		} else {
			dev_err(dev, "Invalid algo:0x%x, not support \n", ctx_p->alg);
		}
	}
	kfree(areq_ctx->auth);
	areq_ctx->auth = NULL;

	aead_request_complete(req, err);
	pm_runtime_put_autosuspend(dev);
}

/* taken from crypto/ccm.c */
static inline int crypto_ccm_check_iv(const u8 *iv)
{
	/* 2 <= L <= 8, so 1 <= L' <= 7. */
	if (1 > iv[0] || iv[0] > 7)
		return -EINVAL;

	return 0;
}

/*
 * https://www.kernel.org/doc/html/v4.14/crypto/api-aead.html#c.aead_request_set_crypt
 * The memory structure for cipher operation has the following structure:
 * AEAD encryption input:  assoc data || plaintext
 * AEAD encryption output: assoc data || cipherntext || auth tag
 * AEAD decryption input:  assoc data || ciphertext  || auth tag
 * AEAD decryption output: assoc data || plaintext
 */

static int te_aead_encrypt(struct aead_request *req)
{
#ifdef CFG_TE_ASYNC_EN
	int rc = -1;
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct te_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_aead_req_ctx *areq_ctx = aead_request_ctx(req);
	struct scatterlist *src, *dst;
	unsigned int l=0;

	src = scatterwalk_ffwd(areq_ctx->src, req->src, req->assoclen);
	dst = src;

	if (req->src != req->dst) {
		/*TO BE COMFIRMED: need to copy aad to dst or not ???*/
		dst = scatterwalk_ffwd(areq_ctx->dst, req->dst, req->assoclen);
	}

	areq_ctx->auth = kmalloc(crypto_aead_authsize(tfm), GFP_KERNEL);
	if (!areq_ctx->auth)
		return -ENOMEM;
	memset(areq_ctx->auth, 0, crypto_aead_authsize(tfm));
	areq_ctx->op = TE_DRV_SCA_ENCRYPT;

	pm_runtime_get_sync(dev);

	switch (TE_ALG_GET_CHAIN_MODE(ctx->alg)) {
	case TE_CHAIN_MODE_GCM:
		areq_ctx->enc.te_gcm_req = kmalloc(sizeof(te_gcm_request_t), GFP_KERNEL);
		if (!areq_ctx->enc.te_gcm_req) {
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->enc.te_gcm_req->base.flags = req->base.flags;
		areq_ctx->enc.te_gcm_req->base.data = req;
		areq_ctx->enc.te_gcm_req->base.completion = te_aead_complete;
		areq_ctx->enc.te_gcm_req->crypt.taglen = ctx->authsize;
		areq_ctx->enc.te_gcm_req->crypt.tag = areq_ctx->auth;
		areq_ctx->enc.te_gcm_req->crypt.op = TE_DRV_SCA_ENCRYPT;
		areq_ctx->enc.te_gcm_req->crypt.iv = req->iv;
		areq_ctx->enc.te_gcm_req->crypt.ivlen = crypto_aead_ivsize(tfm);

		rc = te_buf_mgr_gen_memlist(src, req->cryptlen, &areq_ctx->enc.te_gcm_req->crypt.in);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->enc.te_gcm_req);
			areq_ctx->enc.te_gcm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_buf_mgr_gen_memlist(dst, req->cryptlen, &areq_ctx->enc.te_gcm_req->crypt.out);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_gcm_req->crypt.in);
			kfree(areq_ctx->enc.te_gcm_req);
			areq_ctx->enc.te_gcm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_buf_mgr_gen_memlist(req->src, req->assoclen, &areq_ctx->enc.te_gcm_req->crypt.aad);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_gcm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_gcm_req->crypt.out);
			kfree(areq_ctx->enc.te_gcm_req);
			areq_ctx->enc.te_gcm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_gcm_acrypt(&ctx->gctx, areq_ctx->enc.te_gcm_req);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_gcm_req->crypt.aad);
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_gcm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_gcm_req->crypt.out);
			kfree(areq_ctx->enc.te_gcm_req);
			areq_ctx->enc.te_gcm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		break;
	case TE_CHAIN_MODE_CCM:
		rc = crypto_ccm_check_iv(req->iv);
		if (rc) {
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}

		areq_ctx->enc.te_ccm_req = kmalloc(sizeof(te_ccm_request_t), GFP_KERNEL);
		if (!areq_ctx->enc.te_ccm_req) {
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			rc = -ENOMEM;
			goto err;
		}
		/*L = L' + 1*/
		l = req->iv[0] + 1;
		areq_ctx->enc.te_ccm_req->base.flags = req->base.flags;
		areq_ctx->enc.te_ccm_req->base.data = req;
		areq_ctx->enc.te_ccm_req->base.completion = te_aead_complete;
		areq_ctx->enc.te_ccm_req->crypt.taglen = ctx->authsize;
		areq_ctx->enc.te_ccm_req->crypt.tag = areq_ctx->auth;
		areq_ctx->enc.te_ccm_req->crypt.op = TE_DRV_SCA_ENCRYPT;
		areq_ctx->enc.te_ccm_req->crypt.nonce = req->iv + 1;/* offset iv[0]*/
		 /*exclude iv[0] and length field*/
		areq_ctx->enc.te_ccm_req->crypt.nlen = crypto_aead_ivsize(tfm) - l -1;
		rc = te_buf_mgr_gen_memlist(src, req->cryptlen, &areq_ctx->enc.te_ccm_req->crypt.in);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->enc.te_ccm_req);
			areq_ctx->enc.te_ccm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_buf_mgr_gen_memlist(dst, req->cryptlen, &areq_ctx->enc.te_ccm_req->crypt.out);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_ccm_req->crypt.in);
			kfree(areq_ctx->enc.te_ccm_req);
			areq_ctx->enc.te_ccm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_buf_mgr_gen_memlist(req->src, req->assoclen, &areq_ctx->enc.te_ccm_req->crypt.aad);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_ccm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_ccm_req->crypt.out);
			kfree(areq_ctx->enc.te_ccm_req);
			areq_ctx->enc.te_ccm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_ccm_acrypt(&ctx->cctx, areq_ctx->enc.te_ccm_req);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_ccm_req->crypt.aad);
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_ccm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->enc.te_ccm_req->crypt.out);
			kfree(areq_ctx->enc.te_ccm_req);
			areq_ctx->enc.te_ccm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		break;
	default:
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			TE_ALG_GET_CHAIN_MODE(ctx->alg));
		kfree(areq_ctx->auth);
		areq_ctx->auth = NULL;
		goto err;
	}

	return -EINPROGRESS;
err:
	pm_runtime_put_autosuspend(dev);
	return rc;
#else
	return TE_ERROR_NOT_SUPPORTED;
#endif
}


static int te_aead_decrypt(struct aead_request *req)
{
#ifdef CFG_TE_ASYNC_EN
	int rc = -1;
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct te_aead_ctx *ctx = crypto_tfm_ctx(req->base.tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_aead_req_ctx *areq_ctx = aead_request_ctx(req);
	unsigned int authsize = crypto_aead_authsize(tfm);
	struct scatterlist *src, *dst;
	unsigned int l=0;

	src = scatterwalk_ffwd(areq_ctx->src, req->src, req->assoclen);
	dst = src;

	if (req->src != req->dst) {
		dst = scatterwalk_ffwd(areq_ctx->dst, req->dst, req->assoclen);
	}

	areq_ctx->auth = kmalloc(authsize, GFP_KERNEL);
	if (!areq_ctx->auth)
		return -ENOMEM;
	memset(areq_ctx->auth, 0, authsize);
	/*copy auth_tag from sg to buf*/
	scatterwalk_map_and_copy(areq_ctx->auth, req->src,
				 req->assoclen + req->cryptlen - authsize,
				 authsize, 0);
	areq_ctx->op = TE_DRV_SCA_DECRYPT;

	pm_runtime_get_sync(dev);

	switch (TE_ALG_GET_CHAIN_MODE(ctx->alg)) {
	case TE_CHAIN_MODE_GCM:
		areq_ctx->dec.te_gcm_req = kmalloc(sizeof(te_gcm_request_t), GFP_KERNEL);
		if (!areq_ctx->dec.te_gcm_req) {
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->dec.te_gcm_req->base.flags = req->base.flags;
		areq_ctx->dec.te_gcm_req->base.data = req;
		areq_ctx->dec.te_gcm_req->base.completion = te_aead_complete;
		areq_ctx->dec.te_gcm_req->crypt.taglen = ctx->authsize;
		areq_ctx->dec.te_gcm_req->crypt.tag = areq_ctx->auth;
		areq_ctx->dec.te_gcm_req->crypt.op = TE_DRV_SCA_DECRYPT;
		areq_ctx->dec.te_gcm_req->crypt.iv = req->iv;
		areq_ctx->dec.te_gcm_req->crypt.ivlen = crypto_aead_ivsize(tfm);

		rc = te_buf_mgr_gen_memlist(src, req->cryptlen - authsize, &areq_ctx->dec.te_gcm_req->crypt.in);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->dec.te_gcm_req);
			areq_ctx->dec.te_gcm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_buf_mgr_gen_memlist(dst, req->cryptlen - authsize, &areq_ctx->dec.te_gcm_req->crypt.out);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_gcm_req->crypt.in);
			kfree(areq_ctx->dec.te_gcm_req);
			areq_ctx->dec.te_gcm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_buf_mgr_gen_memlist(req->src, req->assoclen, &areq_ctx->dec.te_gcm_req->crypt.aad);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_gcm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_gcm_req->crypt.out);
			kfree(areq_ctx->dec.te_gcm_req);
			areq_ctx->dec.te_gcm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_gcm_acrypt(&ctx->gctx, areq_ctx->dec.te_gcm_req);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_gcm_req->crypt.aad);
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_gcm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_gcm_req->crypt.out);
			kfree(areq_ctx->dec.te_gcm_req);
			areq_ctx->dec.te_gcm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		break;
	case TE_CHAIN_MODE_CCM:
		rc = crypto_ccm_check_iv(req->iv);
		if (rc) {
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}

		areq_ctx->dec.te_ccm_req = kmalloc(sizeof(te_gcm_request_t), GFP_KERNEL);
		if (!areq_ctx->dec.te_ccm_req) {
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			rc = -ENOMEM;
			goto err;
		}
		/*L = L' + 1*/
		l = req->iv[0] + 1;
		areq_ctx->dec.te_ccm_req->base.flags = req->base.flags;
		areq_ctx->dec.te_ccm_req->base.data = req;
		areq_ctx->dec.te_ccm_req->base.completion = te_aead_complete;
		//areq_ctx->dec.te_ccm_req->crypt.aadlen = req->assoclen;
		//areq_ctx->dec.te_ccm_req->crypt.aad = sg_virt(req->src);
		areq_ctx->dec.te_ccm_req->crypt.taglen = ctx->authsize;
		areq_ctx->dec.te_ccm_req->crypt.tag = areq_ctx->auth;
		areq_ctx->dec.te_ccm_req->crypt.op = TE_DRV_SCA_DECRYPT;
		areq_ctx->dec.te_ccm_req->crypt.nonce = req->iv + 1;/* offset iv[0]*/
		 /*exclude iv[0] and length field*/
		areq_ctx->dec.te_ccm_req->crypt.nlen = crypto_aead_ivsize(tfm) - l -1;

		rc = te_buf_mgr_gen_memlist(src, req->cryptlen - authsize, &areq_ctx->dec.te_ccm_req->crypt.in);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->dec.te_ccm_req);
			areq_ctx->dec.te_ccm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_buf_mgr_gen_memlist(dst, req->cryptlen - authsize, &areq_ctx->dec.te_ccm_req->crypt.out);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_ccm_req->crypt.in);
			kfree(areq_ctx->dec.te_ccm_req);
			areq_ctx->dec.te_ccm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_buf_mgr_gen_memlist(req->src, req->assoclen, &areq_ctx->dec.te_ccm_req->crypt.aad);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_ccm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_ccm_req->crypt.out);
			kfree(areq_ctx->dec.te_ccm_req);
			areq_ctx->dec.te_ccm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		rc = te_ccm_acrypt(&ctx->cctx, areq_ctx->dec.te_ccm_req);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_ccm_req->crypt.aad);
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_ccm_req->crypt.in);
			te_buf_mgr_free_memlist(&areq_ctx->dec.te_ccm_req->crypt.out);
			kfree(areq_ctx->dec.te_ccm_req);
			areq_ctx->dec.te_ccm_req = NULL;
			kfree(areq_ctx->auth);
			areq_ctx->auth = NULL;
			goto err;
		}
		break;
	default:
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			TE_ALG_GET_CHAIN_MODE(ctx->alg));
		kfree(areq_ctx->auth);
		areq_ctx->auth = NULL;
		goto err;
	}

	return -EINPROGRESS;
err:
	pm_runtime_put_autosuspend(dev);
	return rc;

#else
	return TE_ERROR_NOT_SUPPORTED;
#endif

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
	struct te_crypto_alg *t_alg;
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
			dev_err(dev, "Registered %s\n", t_alg->aead_alg.base.cra_driver_name);
		}
	}

	return 0;

fail2:
	kfree(t_alg);
fail1:
	lca_te_aead_free(drvdata);
fail0:
	return rc;
}
