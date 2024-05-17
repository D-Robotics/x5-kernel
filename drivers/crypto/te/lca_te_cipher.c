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

#include "lca_te_driver.h"
#include "lca_te_buf_mgr.h"
#ifdef CFG_TE_ASYNC_EN
#define template_skcipher	template_u.skcipher
#else
#define template_blkcipher	template_u.blkcipher
#endif

#define  _CHECK_CHIAIN_MODE_VALID(_mode_)                    \
    ((((TE_CHAIN_MODE_XTS) == (_mode_)) ||                   \
    ((TE_CHAIN_MODE_CTR) == (_mode_)) ||                     \
    ((TE_CHAIN_MODE_OFB) == (_mode_)) ||                     \
    ((TE_CHAIN_MODE_CBC_NOPAD) == (_mode_)) ||               \
    ((TE_CHAIN_MODE_ECB_NOPAD) == (_mode_))) ? 1:0)

#define SM4_XTS_KEY_SIZE  (32)
#define AES_XTS_KEY_MIN_SIZE  (32)
#define AES_XTS_KEY_MAX_SIZE  (64)

struct te_cipher_handle {
	struct list_head alg_list;
};

struct lca_te_cipher_ctx {
	struct te_drvdata *drvdata;
	te_algo_t alg;
	uint8_t *iv;             /**< initial vector or nonce(CTR) */
	uint8_t *stream;         /**< stream block (CTR) */
	size_t off;           /**< offset of iv (OFB) or stream (CTR) */
	pid_t tid;
	union {
		struct te_cipher_ctx ctx;
		struct te_xts_ctx xctx;
	};
};

#ifdef CFG_TE_ASYNC_EN
struct lca_te_xts_req {
	struct te_xts_ctx xctx;
	te_xts_request_t te_xts_req;
};
struct lca_te_cipher_req {
	struct te_cipher_ctx ctx;
	te_cipher_request_t te_req;
};
struct te_cipher_req_ctx {
	te_sca_operation_t op;
	union {
		struct lca_te_xts_req *te_xts;
		struct lca_te_cipher_req *te_cipher;
	}enc;
	union {
		struct lca_te_xts_req *te_xts;
		struct lca_te_cipher_req *te_cipher;
	}dec;
};
#endif

static int lca_cipher_init(struct crypto_tfm *tfm)
{
	int rc = -1;
	struct lca_te_cipher_ctx *ctx_p = crypto_tfm_ctx(tfm);
#ifdef CFG_TE_ASYNC_EN
	struct te_crypto_alg *te_alg =
			container_of(tfm->__crt_alg, struct te_crypto_alg,
					 skcipher_alg.base);
#else
	struct te_crypto_alg *te_alg =
			container_of(tfm->__crt_alg, struct te_crypto_alg,
					 crypto_alg);
#endif
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	dev_dbg(dev, "Initializing context @%p for %s\n", ctx_p,
		crypto_tfm_alg_name(tfm));

	pm_runtime_get_sync(dev);
#ifdef CFG_TE_ASYNC_EN
	crypto_skcipher_set_reqsize(__crypto_skcipher_cast(tfm),
					sizeof(struct te_cipher_req_ctx));
#endif
	memset(ctx_p, 0, sizeof(*ctx_p));

	ctx_p->alg = te_alg->alg;
	ctx_p->drvdata = te_alg->drvdata;

	switch (TE_ALG_GET_CHAIN_MODE(ctx_p->alg)) {
	case TE_CHAIN_MODE_XTS:
		rc = te_xts_init(&ctx_p->xctx, ctx_p->drvdata->h,
					TE_ALG_GET_MAIN_ALG(ctx_p->alg));

		break;
	case TE_CHAIN_MODE_ECB_NOPAD:
	case TE_CHAIN_MODE_CBC_NOPAD:
	case TE_CHAIN_MODE_CTR:
	case TE_CHAIN_MODE_OFB:
		{
			int ivsize=0;
			ivsize = crypto_skcipher_ivsize(__crypto_skcipher_cast(tfm));

			if(ivsize > 0) {
				ctx_p->iv = kmalloc(ivsize, GFP_KERNEL);
				if (!ctx_p->iv) {
					rc = -ENOMEM;
					goto err;
				}
				ctx_p->stream = kmalloc(ivsize, GFP_KERNEL);
				if (!ctx_p->stream) {
					kfree(ctx_p->iv);
					ctx_p->iv = NULL;
					rc = -ENOMEM;
					goto err;
				}
				memset(ctx_p->stream, 0, ivsize);
				ctx_p->off = 0;
			}
			rc = te_cipher_init(&ctx_p->ctx, ctx_p->drvdata->h,
						TE_ALG_GET_MAIN_ALG(ctx_p->alg));
		}
		break;
	default:
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
	}

	ctx_p->tid = current->pid;
err:
	pm_runtime_put_autosuspend(dev);
	return rc;
}


static void lca_cipher_exit(struct crypto_tfm *tfm)
{
	int rc = -1;
	struct lca_te_cipher_ctx *ctx_p = crypto_tfm_ctx(tfm);
#ifdef CFG_TE_ASYNC_EN
		struct te_crypto_alg *te_alg =
				container_of(tfm->__crt_alg, struct te_crypto_alg,
						 skcipher_alg.base);
#else
		struct te_crypto_alg *te_alg =
				container_of(tfm->__crt_alg, struct te_crypto_alg,
						 crypto_alg);
#endif
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	pm_runtime_get_sync(dev);

	switch (TE_ALG_GET_CHAIN_MODE(ctx_p->alg)) {
	case TE_CHAIN_MODE_XTS:
		rc = te_xts_free(&ctx_p->xctx);
		break;
	case TE_CHAIN_MODE_ECB_NOPAD:
	case TE_CHAIN_MODE_CBC_NOPAD:
	case TE_CHAIN_MODE_CTR:
	case TE_CHAIN_MODE_OFB:
		if(ctx_p->iv)
			kfree(ctx_p->iv);
		ctx_p->iv = NULL;
		if(ctx_p->stream)
			kfree(ctx_p->stream);
		ctx_p->stream = NULL;
		ctx_p->off = 0;
		rc = te_cipher_free(&ctx_p->ctx);
		break;
	default:
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
	}

	pm_runtime_put_autosuspend(dev);
	return;
}


/* Block cipher alg */
#ifdef CFG_TE_ASYNC_EN

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

	if (TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		rc = te_xts_setkey(&ctx_p->xctx, key, keylen*BITS_IN_BYTE);
	} else {
		rc = te_cipher_setkey(&ctx_p->ctx, key, keylen*BITS_IN_BYTE);
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

	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		if(req_ctx->op == TE_DRV_SCA_ENCRYPT) {
			if (req_ctx->enc.te_xts->xctx.crypt)
				te_xts_free(&req_ctx->enc.te_xts->xctx);
			te_buf_mgr_free_memlist(&req_ctx->enc.te_xts->te_xts_req.src);
			te_buf_mgr_free_memlist(&req_ctx->enc.te_xts->te_xts_req.dst);
			kfree(req_ctx->enc.te_xts);
			req_ctx->enc.te_xts = NULL;
		} else {
			if (req_ctx->dec.te_xts->xctx.crypt)
				te_xts_free(&req_ctx->dec.te_xts->xctx);
			te_buf_mgr_free_memlist(&req_ctx->dec.te_xts->te_xts_req.src);
			te_buf_mgr_free_memlist(&req_ctx->dec.te_xts->te_xts_req.dst);
			kfree(req_ctx->dec.te_xts);
			req_ctx->dec.te_xts = NULL;
		}
	} else {
		if(req_ctx->op == TE_DRV_SCA_ENCRYPT) {
			if (req_ctx->enc.te_cipher->ctx.crypt)
				te_cipher_free(&req_ctx->enc.te_cipher->ctx);
			te_buf_mgr_free_memlist(&req_ctx->enc.te_cipher->te_req.src);
			te_buf_mgr_free_memlist(&req_ctx->enc.te_cipher->te_req.dst);
			kfree(req_ctx->enc.te_cipher);
			req_ctx->enc.te_cipher = NULL;
		} else {
			if (req_ctx->dec.te_cipher->ctx.crypt)
				te_cipher_free(&req_ctx->dec.te_cipher->ctx);
			te_buf_mgr_free_memlist(&req_ctx->dec.te_cipher->te_req.src);
			te_buf_mgr_free_memlist(&req_ctx->dec.te_cipher->te_req.dst);
			kfree(req_ctx->dec.te_cipher);
			req_ctx->dec.te_cipher = NULL;
		}
	}
	skcipher_request_complete(req, err);
	pm_runtime_put_autosuspend(dev);
}

static int lca_te_cipher_encrypt(struct skcipher_request *req)
{
	int rc = 0;
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct lca_te_cipher_ctx *ctx_p = crypto_skcipher_ctx(tfm);
	struct te_cipher_req_ctx *req_ctx = skcipher_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx_p->drvdata);
	te_cipher_ctx_t *ctx = NULL;
	te_xts_ctx_t *xctx   = NULL;

	req_ctx->op = TE_DRV_SCA_ENCRYPT;

	if(!_CHECK_CHIAIN_MODE_VALID(TE_ALG_GET_CHAIN_MODE(ctx_p->alg))){
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
		return TE_ERROR_INVAL_ALG;
	}

	pm_runtime_get_sync(dev);

	if (TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {

		req_ctx->enc.te_xts = kzalloc(sizeof(struct lca_te_xts_req), GFP_KERNEL);
		if (!req_ctx->enc.te_xts) {
			rc = -ENOMEM;
			goto fail;
		}

		if(sizeof(req_ctx->enc.te_xts->te_xts_req.data_unit) < crypto_skcipher_ivsize(tfm)) {
			dev_err(dev, "enc failed! invalid iv size for XTS \n");
			kfree(req_ctx->enc.te_xts);
			req_ctx->enc.te_xts = NULL;
			rc = -1;
			goto fail;
		}
		rc = te_buf_mgr_gen_memlist(req->src, req->cryptlen, &req_ctx->enc.te_xts->te_xts_req.src);
		if (rc != TE_SUCCESS) {
			kfree(req_ctx->enc.te_xts);
			req_ctx->enc.te_xts = NULL;
			goto fail;
		}
		rc = te_buf_mgr_gen_memlist(req->dst, req->cryptlen, &req_ctx->enc.te_xts->te_xts_req.dst);
		if (rc != TE_SUCCESS)
			goto fail1;


		memcpy(req_ctx->enc.te_xts->te_xts_req.data_unit, req->iv, crypto_skcipher_ivsize(tfm));
		req_ctx->enc.te_xts->te_xts_req.op = TE_DRV_SCA_ENCRYPT;
		req_ctx->enc.te_xts->te_xts_req.base.completion = lca_te_cipher_complete;
		req_ctx->enc.te_xts->te_xts_req.base.flags = req->base.flags;
		req_ctx->enc.te_xts->te_xts_req.base.data = req;

		if(ctx_p->tid != current->pid){
			rc = te_xts_clone(&ctx_p->xctx,&req_ctx->enc.te_xts->xctx);
			if (rc != TE_SUCCESS)
				goto fail2;
			xctx = (te_xts_ctx_t *)&req_ctx->enc.te_xts->xctx;
		} else {
			xctx = (te_xts_ctx_t *)&ctx_p->xctx;
		}

		xctx->crypt->alg = ctx_p->alg;
	} else {
		req_ctx->enc.te_cipher = kzalloc(sizeof(struct lca_te_cipher_req), GFP_KERNEL);
		if (!req_ctx->enc.te_cipher) {
			rc = -ENOMEM;
			goto fail;
		}

		if (req->iv) {
			req_ctx->enc.te_cipher->te_req.iv = req->iv;
			req_ctx->enc.te_cipher->te_req.stream = NULL;
			req_ctx->enc.te_cipher->te_req.off = NULL;
		}
		rc = te_buf_mgr_gen_memlist(req->src, req->cryptlen, &req_ctx->enc.te_cipher->te_req.src);
		if (rc != TE_SUCCESS) {
			kfree(req_ctx->enc.te_cipher);
			req_ctx->enc.te_cipher = NULL;
			goto fail;
		}
		rc = te_buf_mgr_gen_memlist(req->dst, req->cryptlen, &req_ctx->enc.te_cipher->te_req.dst);
		if (rc != TE_SUCCESS)
			goto fail1;

		req_ctx->enc.te_cipher->te_req.op = TE_DRV_SCA_ENCRYPT;
		req_ctx->enc.te_cipher->te_req.base.completion = lca_te_cipher_complete;
		req_ctx->enc.te_cipher->te_req.base.flags = req->base.flags;
		req_ctx->enc.te_cipher->te_req.base.data = req;

		if(ctx_p->tid != current->pid){
			rc = te_cipher_clone(&ctx_p->ctx,&req_ctx->enc.te_cipher->ctx);
			if (rc != TE_SUCCESS)
				goto fail2;
			ctx = (te_cipher_ctx_t *)&req_ctx->enc.te_cipher->ctx;
		} else {
			ctx = (te_cipher_ctx_t *)&ctx_p->ctx;
		}

		ctx->crypt->alg = ctx_p->alg;
	}

	switch (TE_ALG_GET_CHAIN_MODE(ctx_p->alg)) {
	case TE_CHAIN_MODE_XTS:
		rc = te_xts_acrypt(xctx, &req_ctx->enc.te_xts->te_xts_req);
		break;
	case TE_CHAIN_MODE_ECB_NOPAD:
		rc = te_cipher_aecb(ctx, &req_ctx->enc.te_cipher->te_req);
		break;
	case TE_CHAIN_MODE_CBC_NOPAD:
		rc = te_cipher_acbc(ctx, &req_ctx->enc.te_cipher->te_req);
		break;
	case TE_CHAIN_MODE_CTR:
		rc = te_cipher_actr(ctx, &req_ctx->enc.te_cipher->te_req);
		break;
	case TE_CHAIN_MODE_OFB:
		rc = te_cipher_aofb(ctx, &req_ctx->enc.te_cipher->te_req);
		break;
	default:
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
		rc = TE_ERROR_INVAL_ALG;
		goto fail3;
	}

	if (rc != TE_SUCCESS)
		goto fail3;

	return -EINPROGRESS;
fail3:
	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		if (req_ctx->enc.te_xts->xctx.crypt)
			te_xts_free(&req_ctx->enc.te_xts->xctx);
	} else {
		if (req_ctx->enc.te_cipher->ctx.crypt)
			te_cipher_free(&req_ctx->enc.te_cipher->ctx);
	}
fail2:
	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		te_buf_mgr_free_memlist(&req_ctx->enc.te_xts->te_xts_req.dst);
	} else {
		te_buf_mgr_free_memlist(&req_ctx->enc.te_cipher->te_req.dst);
	}
fail1:
	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		te_buf_mgr_free_memlist(&req_ctx->enc.te_xts->te_xts_req.src);
		kfree(req_ctx->enc.te_xts);
		req_ctx->enc.te_xts = NULL;
	} else {
		te_buf_mgr_free_memlist(&req_ctx->enc.te_cipher->te_req.src);
		kfree(req_ctx->enc.te_cipher);
		req_ctx->enc.te_cipher = NULL;
	}

fail:
	pm_runtime_put_autosuspend(dev);
	return rc;
}


static int lca_te_cipher_decrypt(struct skcipher_request *req)
{
	int rc = 0;
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct lca_te_cipher_ctx *ctx_p = crypto_skcipher_ctx(tfm);
	struct te_cipher_req_ctx *req_ctx = skcipher_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx_p->drvdata);
	te_cipher_ctx_t *ctx = NULL;
	te_xts_ctx_t *xctx   = NULL;

	req_ctx->op = TE_DRV_SCA_DECRYPT;

	if(!_CHECK_CHIAIN_MODE_VALID(TE_ALG_GET_CHAIN_MODE(ctx_p->alg))){
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
		return TE_ERROR_INVAL_ALG;
	}

	pm_runtime_get_sync(dev);

	if (TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {

		req_ctx->dec.te_xts = kzalloc(sizeof(struct lca_te_xts_req), GFP_KERNEL);
		if (!req_ctx->dec.te_xts) {
			rc = -ENOMEM;
			goto fail;
		}

		if(sizeof(req_ctx->dec.te_xts->te_xts_req.data_unit) < crypto_skcipher_ivsize(tfm)) {
			dev_err(dev, "dec failed! invalid iv size for XTS \n");
			kfree(req_ctx->dec.te_xts);
			req_ctx->dec.te_xts = NULL;
			rc = -1;
			goto fail;
		}
		rc = te_buf_mgr_gen_memlist(req->src, req->cryptlen, &req_ctx->dec.te_xts->te_xts_req.src);
		if (rc != TE_SUCCESS) {
			kfree(req_ctx->dec.te_xts);
			req_ctx->dec.te_xts = NULL;
			goto fail;
		}
		rc = te_buf_mgr_gen_memlist(req->dst, req->cryptlen, &req_ctx->dec.te_xts->te_xts_req.dst);
		if (rc != TE_SUCCESS)
			goto fail1;


		memcpy(req_ctx->dec.te_xts->te_xts_req.data_unit, req->iv, crypto_skcipher_ivsize(tfm));
		req_ctx->dec.te_xts->te_xts_req.op = TE_DRV_SCA_DECRYPT;
		req_ctx->dec.te_xts->te_xts_req.base.completion = lca_te_cipher_complete;
		req_ctx->dec.te_xts->te_xts_req.base.flags = req->base.flags;
		req_ctx->dec.te_xts->te_xts_req.base.data = req;

		if(ctx_p->tid != current->pid){
			rc = te_xts_clone(&ctx_p->xctx,&req_ctx->dec.te_xts->xctx);
			if (rc != TE_SUCCESS)
				goto fail2;
			xctx = (te_xts_ctx_t *)&req_ctx->dec.te_xts->xctx;
		} else {
			xctx = (te_xts_ctx_t *)&ctx_p->xctx;
		}

		xctx->crypt->alg = ctx_p->alg;
	} else {
		req_ctx->dec.te_cipher = kzalloc(sizeof(struct lca_te_cipher_req), GFP_KERNEL);
		if (!req_ctx->dec.te_cipher) {
			rc = -ENOMEM;
			goto fail;
		}

		if (req->iv) {
			req_ctx->dec.te_cipher->te_req.iv = req->iv;
			req_ctx->dec.te_cipher->te_req.stream = NULL;
			req_ctx->dec.te_cipher->te_req.off = NULL;
		}
		rc = te_buf_mgr_gen_memlist(req->src, req->cryptlen, &req_ctx->dec.te_cipher->te_req.src);
		if (rc != TE_SUCCESS) {
			kfree(req_ctx->dec.te_cipher);
			req_ctx->dec.te_cipher = NULL;
			goto fail;
		}
		rc = te_buf_mgr_gen_memlist(req->dst, req->cryptlen, &req_ctx->dec.te_cipher->te_req.dst);
		if (rc != TE_SUCCESS)
			goto fail1;

		req_ctx->dec.te_cipher->te_req.op = TE_DRV_SCA_DECRYPT;
		req_ctx->dec.te_cipher->te_req.base.completion = lca_te_cipher_complete;
		req_ctx->dec.te_cipher->te_req.base.flags = req->base.flags;
		req_ctx->dec.te_cipher->te_req.base.data = req;

		if(ctx_p->tid != current->pid){
			rc = te_cipher_clone(&ctx_p->ctx,&req_ctx->dec.te_cipher->ctx);
			if (rc != TE_SUCCESS)
				goto fail2;
			ctx = (te_cipher_ctx_t *)&req_ctx->dec.te_cipher->ctx;
		} else {
			ctx = (te_cipher_ctx_t *)&ctx_p->ctx;
		}

		ctx->crypt->alg = ctx_p->alg;
	}

	switch (TE_ALG_GET_CHAIN_MODE(ctx_p->alg)) {
	case TE_CHAIN_MODE_XTS:
		rc = te_xts_acrypt(xctx, &req_ctx->dec.te_xts->te_xts_req);
		break;
	case TE_CHAIN_MODE_ECB_NOPAD:
		rc = te_cipher_aecb(ctx, &req_ctx->dec.te_cipher->te_req);
		break;
	case TE_CHAIN_MODE_CBC_NOPAD:
		rc = te_cipher_acbc(ctx, &req_ctx->dec.te_cipher->te_req);
		break;
	case TE_CHAIN_MODE_CTR:
		rc = te_cipher_actr(ctx, &req_ctx->dec.te_cipher->te_req);
		break;
	case TE_CHAIN_MODE_OFB:
		rc = te_cipher_aofb(ctx, &req_ctx->dec.te_cipher->te_req);
		break;
	default:
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
		rc = TE_ERROR_INVAL_ALG;
		goto fail3;
	}

	if (rc != TE_SUCCESS)
		goto fail3;

	return -EINPROGRESS;
fail3:
	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		if (req_ctx->dec.te_xts->xctx.crypt)
			te_xts_free(&req_ctx->dec.te_xts->xctx);
	} else {
		if (req_ctx->dec.te_cipher->ctx.crypt)
			te_cipher_free(&req_ctx->dec.te_cipher->ctx);
	}
fail2:
	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		te_buf_mgr_free_memlist(&req_ctx->dec.te_xts->te_xts_req.dst);
	} else {
		te_buf_mgr_free_memlist(&req_ctx->dec.te_cipher->te_req.dst);
	}
fail1:
	if(TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		te_buf_mgr_free_memlist(&req_ctx->dec.te_xts->te_xts_req.src);
		kfree(req_ctx->dec.te_xts);
		req_ctx->dec.te_xts = NULL;
	} else {
		te_buf_mgr_free_memlist(&req_ctx->dec.te_cipher->te_req.src);
		kfree(req_ctx->dec.te_cipher);
		req_ctx->dec.te_cipher = NULL;
	}

fail:
	pm_runtime_put_autosuspend(dev);
	return rc;
}


/* async template */
static const struct te_alg_template skcipher_algs[] = {
	{
		.name = "xts(aes)",
		.driver_name = "xts-aes-te",
		.blocksize = 1,
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
		.blocksize = 1,
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
#else

static int lca_te_cipher_setkey(struct crypto_tfm *tfm, const u8 *key,
			  unsigned int keylen)
{
	int rc = -1;
	struct lca_te_cipher_ctx *ctx_p = crypto_tfm_ctx(tfm);

	if (TE_ALG_GET_CHAIN_MODE(ctx_p->alg) == TE_CHAIN_MODE_XTS) {
		return te_xts_setkey(&ctx_p->xctx, key, keylen*BITS_IN_BYTE);
	}

	return te_cipher_setkey(&ctx_p->ctx, key, keylen*BITS_IN_BYTE);
}

static int lca_te_cipher_process(struct crypto_tfm *tfm,
			   te_sca_operation_t op, uint8_t *iv,
			   struct scatterlist *dst, struct scatterlist *src,
			   unsigned int nbytes)
{
	int rc = 0;
	struct lca_te_cipher_ctx *ctx_p = crypto_tfm_ctx(tfm);
	struct te_crypto_alg *te_alg =
			container_of(tfm->__crt_alg, struct te_crypto_alg,
					 crypto_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);
	te_memlist_t in_list,out_list;

	pm_runtime_get_sync(dev);
	rc = te_buf_mgr_gen_memlist(src,nbytes,&in_list);
	if (rc != TE_SUCCESS) {
		goto inlist_gen_err;
	}
	rc = te_buf_mgr_gen_memlist(dst,nbytes,&out_list);
	if (rc != TE_SUCCESS) {
		goto olist_gen_err;
	}

	switch (TE_ALG_GET_CHAIN_MODE(ctx_p->alg)) {
	case TE_CHAIN_MODE_XTS:
		ctx_p->xctx.crypt->alg = ctx_p->alg;
		rc = te_xts_crypt_list(&ctx_p->xctx, op, iv,
					&in_list, &out_list);
		break;
	case TE_CHAIN_MODE_ECB_NOPAD:
		ctx_p->ctx.crypt->alg = ctx_p->alg;
		rc = te_cipher_ecb_list(&ctx_p->ctx, op,
					&in_list, &out_list);
		break;
	case TE_CHAIN_MODE_CBC_NOPAD:
		ctx_p->ctx.crypt->alg = ctx_p->alg;
		rc = te_cipher_cbc_list(&ctx_p->ctx, op, iv,
					&in_list, &out_list);
		break;
	case TE_CHAIN_MODE_CTR:
		ctx_p->ctx.crypt->alg = ctx_p->alg;
		rc = te_cipher_ctr_list(&ctx_p->ctx, NULL,
					iv, NULL,&in_list, &out_list);
		break;
	case TE_CHAIN_MODE_OFB:
		ctx_p->ctx.crypt->alg = ctx_p->alg;
		rc = te_cipher_ofb_list(&ctx_p->ctx, NULL, iv,
					&in_list, &out_list);
		break;
	default:
		dev_err(dev, "Unsupported cipher mode (%d)\n",
			   TE_ALG_GET_CHAIN_MODE(ctx_p->alg));
	}

	te_buf_mgr_free_memlist(&out_list);

olist_gen_err:
	te_buf_mgr_free_memlist(&in_list);
inlist_gen_err:
	pm_runtime_put_autosuspend(dev);
	return rc;
}

static int lca_te_cipher_encrypt(struct blkcipher_desc *desc,
			   struct scatterlist *dst, struct scatterlist *src,
			   unsigned int nbytes)
{
	struct crypto_tfm *tfm = crypto_blkcipher_tfm(desc->tfm);

	return lca_te_cipher_process(tfm, TE_DRV_SCA_ENCRYPT,
		(uint8_t *)desc->info, dst, src, nbytes);
}

static int lca_te_cipher_decrypt(struct blkcipher_desc *desc,
			   struct scatterlist *dst, struct scatterlist *src,
			   unsigned int nbytes)
{
	struct crypto_tfm *tfm = crypto_blkcipher_tfm(desc->tfm);

	return lca_te_cipher_process(tfm, TE_DRV_SCA_DECRYPT,
		(uint8_t *)desc->info, dst, src, nbytes);
}

/* sync template */
static const struct te_alg_template blkcipher_algs[] = {
	{
		.name = "xts(aes)",
		.driver_name = "xts-aes-te",
		.blocksize = 1,
		.template_blkcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
			},
		.alg = TE_ALG_AES_XTS,
	},
	{
		.name = "xts(sm4)",
		.driver_name = "xts-sm4-te",
		.blocksize = 1,
		.template_blkcipher = {
			.setkey = lca_te_cipher_setkey,
			.encrypt = lca_te_cipher_encrypt,
			.decrypt = lca_te_cipher_decrypt,
			.min_keysize = SM4_KEY_SIZE,
			.max_keysize = SM4_KEY_SIZE,
			.ivsize = SM4_BLOCK_SIZE,
			},
		.alg = TE_ALG_SM4_XTS,
	},
	{
		.name = "ctr(aes)",
		.driver_name = "ctr-aes-te",
		.blocksize = 1,
		.template_blkcipher = {
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
		.template_blkcipher = {
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
		.template_blkcipher = {
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
		.template_blkcipher = {
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
		.template_blkcipher = {
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
		.template_blkcipher = {
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
		.template_blkcipher = {
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
		.template_blkcipher = {
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
		.template_blkcipher = {
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
		.template_blkcipher = {
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
		.template_blkcipher = {
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
		.template_blkcipher = {
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
#endif

static struct te_crypto_alg *te_create_alg(const struct te_alg_template *tmpl)
{
	struct te_crypto_alg *t_alg;
#ifdef CFG_TE_ASYNC_EN
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
#else
	struct crypto_alg *alg;

	t_alg = kzalloc(sizeof(*t_alg), GFP_KERNEL);
	if (!t_alg)
		return ERR_PTR(-ENOMEM);

	alg = &t_alg->crypto_alg;

	memcpy(&alg->cra_u.blkcipher, &tmpl->template_blkcipher, sizeof(struct blkcipher_alg));
	snprintf(alg->cra_name, CRYPTO_MAX_ALG_NAME, "%s", tmpl->name);
	snprintf(alg->cra_driver_name, CRYPTO_MAX_ALG_NAME, "%s",
		 tmpl->driver_name);
	alg->cra_module = THIS_MODULE;
	alg->cra_priority = TE_CRA_PRIO;
	alg->cra_blocksize = tmpl->blocksize;
	alg->cra_alignmask = 0;
	alg->cra_ctxsize = sizeof(struct lca_te_cipher_ctx);

	alg->cra_init = lca_cipher_init;
	alg->cra_exit = lca_cipher_exit;
	alg->cra_flags = CRYPTO_ALG_TYPE_BLKCIPHER;
#endif
	t_alg->alg = tmpl->alg;
	t_alg->data_unit = tmpl->data_unit;

	return t_alg;
}

int lca_te_cipher_free(struct te_drvdata *drvdata)
{
	struct te_crypto_alg *t_alg, *n;
	struct te_cipher_handle *cipher_handle = drvdata->cipher_handle;

	if (cipher_handle) {
		/* Remove registered algs */
		list_for_each_entry_safe(t_alg, n, &cipher_handle->alg_list,
					 entry) {

#ifdef CFG_TE_ASYNC_EN
			crypto_unregister_skcipher(&t_alg->skcipher_alg);
#else
			crypto_unregister_alg(&t_alg->crypto_alg);
#endif
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

	/* TO BE ADDED */
	/* Notify the te_drv_handle to related driver*/

	/* Linux crypto */
#ifdef CFG_TE_ASYNC_EN
	dev_dbg(dev, "Number of algorithms = %zu\n",
		ARRAY_SIZE(skcipher_algs));
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
			dev_dbg(dev, "Registered %s\n",
				t_alg->skcipher_alg.base.cra_driver_name);
		}
	}
#else
	dev_dbg(dev, "Number of algorithms = %zu\n",
		ARRAY_SIZE(blkcipher_algs));
	for (alg = 0; alg < ARRAY_SIZE(blkcipher_algs); alg++) {

		dev_dbg(dev, "creating %s\n", blkcipher_algs[alg].driver_name);
		t_alg = te_create_alg(&blkcipher_algs[alg]);
		if (IS_ERR(t_alg)) {
			rc = PTR_ERR(t_alg);
			dev_err(dev, "%s alg allocation failed\n",
				blkcipher_algs[alg].driver_name);
			goto fail0;
		}
		t_alg->drvdata = drvdata;

		dev_dbg(dev, "registering %s\n",
			blkcipher_algs[alg].driver_name);
		rc = crypto_register_alg(&t_alg->crypto_alg);
		dev_dbg(dev, "%s alg registration rc = %x\n",
			t_alg->crypto_alg.cra_driver_name, rc);
		if (rc) {
			dev_err(dev, "%s alg registration failed\n",
				t_alg->crypto_alg.cra_driver_name);
			kfree(t_alg);
			goto fail0;
		} else {
			list_add_tail(&t_alg->entry,
					  &cipher_handle->alg_list);
			dev_dbg(dev, "Registered %s\n",
				t_alg->crypto_alg.cra_driver_name);
		}
	}
#endif
	return 0;

fail0:
	lca_te_cipher_free(drvdata);
	return rc;
}
