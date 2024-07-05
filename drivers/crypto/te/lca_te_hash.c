//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

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

#include "lca_te_driver.h"
#include "lca_te_buf_mgr.h"


#ifndef SM3_DIGEST_SIZE
#define SM3_DIGEST_SIZE  (32)
#endif

#ifndef SM3_BLOCK_SIZE
#define SM3_BLOCK_SIZE  (64)
#endif
#ifndef MD5_BLOCK_SIZE
#define MD5_BLOCK_SIZE  (64)
#endif

#define LCA_TE_ALG_MAIN_INVALID  0x0
#define LCA_TE_ALG_MAIN_HASH  0x1
#define LCA_TE_ALG_MAIN_CMAC  0x2
#define LCA_TE_ALG_MAIN_CBCMAC  0x3
#define TE_MAX_STAT_SZ  512

#define  _LCA_GET_MAIN_MODE(_alg_) ((((TE_ALG_HMAC_MD5) == (_alg_))                 \
    || ((TE_ALG_HMAC_SHA1) == (_alg_)) || ((TE_ALG_HMAC_SHA224) == (_alg_))    \
    || ((TE_ALG_HMAC_SHA256) == (_alg_)) || ((TE_ALG_HMAC_SHA384) == (_alg_))  \
    || ((TE_ALG_HMAC_SHA512) == (_alg_)) || ((TE_ALG_HMAC_SM3) == (_alg_))     \
    || ((TE_ALG_MD5) == (_alg_)) || ((TE_ALG_SHA1) == (_alg_))                     \
    || ((TE_ALG_SHA224) == (_alg_)) || ((TE_ALG_SHA256) == (_alg_))                \
    || ((TE_ALG_SHA384) == (_alg_)) || ((TE_ALG_SHA512) == (_alg_))                \
    || ((TE_ALG_SM3) == (_alg_))) ? LCA_TE_ALG_MAIN_HASH                           \
    : ((((TE_ALG_AES_CMAC) == (_alg_)) || ((TE_ALG_SM4_CMAC) == (_alg_))        \
    ||((TE_ALG_DES_CMAC) == (_alg_)) || ((TE_ALG_TDES_CMAC) == (_alg_)))           \
    ? LCA_TE_ALG_MAIN_CMAC                                                         \
    : ((((TE_ALG_AES_CBC_MAC_NOPAD) == (_alg_))                                    \
    || ((TE_ALG_DES_CBC_MAC_NOPAD) == (_alg_))                                     \
    || ((TE_ALG_TDES_CBC_MAC_NOPAD) == (_alg_))                                    \
    || ((TE_ALG_SM4_CBC_MAC_NOPAD) == (_alg_)))                                    \
    ? LCA_TE_ALG_MAIN_CBCMAC : LCA_TE_ALG_MAIN_INVALID)))                          \

#define  _LCA_CMAC_GET_MAIN_ALG(_alg_)                                      \
    (((TE_ALG_AES_CMAC) == (_alg_)) ? TE_MAIN_ALGO_AES :                     \
    (((TE_ALG_SM4_CMAC) == (_alg_)) ? TE_MAIN_ALGO_SM4 :                     \
    (((TE_ALG_DES_CMAC) == (_alg_)) ? TE_MAIN_ALGO_DES:                      \
    (((TE_ALG_TDES_CMAC) == (_alg_)) ? TE_MAIN_ALGO_TDES:0))))


#define  _LCA_CBCMAC_GET_MAIN_ALG(_alg_)                                      \
    (((TE_ALG_AES_CBC_MAC_NOPAD) == (_alg_)) ? TE_MAIN_ALGO_AES :             \
    (((TE_ALG_SM4_CBC_MAC_NOPAD) == (_alg_)) ? TE_MAIN_ALGO_SM4 :             \
    (((TE_ALG_DES_CBC_MAC_NOPAD) == (_alg_)) ? TE_MAIN_ALGO_DES :             \
    (((TE_ALG_TDES_CBC_MAC_NOPAD) == (_alg_)) ? TE_MAIN_ALGO_TDES : 0))))


struct te_hash_handle {
	struct list_head hash_list;
};

struct te_hash_alg {
	struct list_head entry;
	int alg;
	int inter_digestsize;
	unsigned int blocksize;
	bool is_hmac;
	struct te_drvdata *drvdata;
#ifdef CFG_TE_ASYNC_EN
	struct ahash_alg ahash_alg;
#else
	struct shash_alg shash_alg;
#endif
};



/* hash per-session context */
struct te_hash_ctx {
	struct te_drvdata *drvdata;
	struct mutex mutex;
	pid_t tid;
	int flag;
	int alg;
	int inter_digestsize;
	unsigned int blocksize;
	bool is_hmac;
	u8 *mackey;
	u8 *maciv;
	unsigned int keylen;
	union {
		te_hmac_ctx_t hctx;
		te_dgst_ctx_t dctx;
		te_cmac_ctx_t cctx;
		te_cbcmac_ctx_t cbctx;
	};
};
#ifdef CFG_TE_ASYNC_EN
struct te_ahash_req_ctx {
	union {
		te_dgst_request_t *dgst_req;
		te_hmac_request_t *hmac_req;
		te_cmac_request_t *cmac_req;
	}init;
	union {
		te_dgst_request_t *dgst_req;
		te_hmac_request_t *hmac_req;
		te_cmac_request_t *cmac_req;
	}update;
	union {
		te_dgst_request_t *dgst_req;
		te_hmac_request_t *hmac_req;
		te_cmac_request_t *cmac_req;
	}final;
	union {
		te_dgst_request_t *dgst_req;
		te_hmac_request_t *hmac_req;
		te_cmac_request_t *cmac_req;
	}dgst;
	void *priv[];
};
#endif

static int lca_te_hash_cra_init(struct crypto_tfm *tfm)
{
	int rc=0;
	struct te_hash_ctx *ctx = crypto_tfm_ctx(tfm);
#ifdef CFG_TE_ASYNC_EN
	struct hash_alg_common *hash_alg_common =
		container_of(tfm->__crt_alg, struct hash_alg_common, base);
	struct ahash_alg *ahash_alg =
		container_of(hash_alg_common, struct ahash_alg, halg);
	struct te_hash_alg *halg =
			container_of(ahash_alg, struct te_hash_alg, ahash_alg);
#else
	struct shash_alg *shash_alg =
		container_of(tfm->__crt_alg, struct shash_alg, base);
	struct te_hash_alg *halg =
		container_of(shash_alg, struct te_hash_alg, shash_alg);
#endif
	struct device *dev = drvdata_to_dev(halg->drvdata);

	dev_dbg(dev, "Initializing context @%p for %s\n", ctx,
		crypto_tfm_alg_name(tfm));

	memset(ctx, 0, sizeof(*ctx));
	ctx->alg = halg->alg;
	ctx->inter_digestsize = halg->inter_digestsize;
	ctx->drvdata = halg->drvdata;
	ctx->is_hmac = halg->is_hmac;
	ctx->blocksize = halg->blocksize;

	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			rc = te_hmac_init(&ctx->hctx, ctx->drvdata->h, ctx->alg);
			if (rc) {
				goto err;
			}
		} else {
			rc = te_dgst_init(&ctx->dctx, ctx->drvdata->h, ctx->alg);
			if (rc) {
				goto err;
			}
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
		rc = te_cmac_init(&ctx->cctx, ctx->drvdata->h,
				_LCA_CMAC_GET_MAIN_ALG(ctx->alg));
		if (rc) {
			goto err;
		}
		break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		rc = te_cbcmac_init(&ctx->cbctx, ctx->drvdata->h,
				_LCA_CBCMAC_GET_MAIN_ALG(ctx->alg));
		if (rc) {
			goto err;
		}
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}

#ifdef CFG_TE_ASYNC_EN
	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct te_ahash_req_ctx) + TE_MAX_STAT_SZ);
#endif
	ctx->tid = current->pid;
	mutex_init(&ctx->mutex);
err:
	dev_dbg(dev, "Initializing context @%p for %s rc:%x\n", ctx,
		crypto_tfm_alg_name(tfm),rc);
	return rc;
}

static void lca_te_hash_cra_exit(struct crypto_tfm *tfm)
{
	int rc=0;
	struct te_hash_ctx *ctx = crypto_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);

	pm_runtime_get_sync(dev);
	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			rc = te_hmac_free(&ctx->hctx);
		} else {
			rc = te_dgst_free(&ctx->dctx);
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
		rc = te_cmac_free(&ctx->cctx);
		break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		rc = te_cbcmac_free(&ctx->cbctx);
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}

	if (ctx->mackey) {
		kfree(ctx->mackey);
		ctx->mackey = NULL;
	}
	if (ctx->maciv) {
		kfree(ctx->maciv);
		ctx->maciv = NULL;
	}

	memset(ctx, 0, sizeof(*ctx));

	pm_runtime_put_autosuspend(dev);
	return;
}


#ifdef CFG_TE_ASYNC_EN
static void te_ahash_complete(struct te_async_request *te_req, int err)
{
	struct ahash_request *req = (struct ahash_request *)te_req->data;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx->drvdata);

	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			te_buf_mgr_free_memlist(&areq_ctx->dgst.hmac_req->hmac.in);
			kfree(areq_ctx->dgst.hmac_req);
			areq_ctx->dgst.hmac_req = NULL;
		} else {
			te_buf_mgr_free_memlist(&areq_ctx->dgst.dgst_req->dgst.in);
			kfree(areq_ctx->dgst.dgst_req);
			areq_ctx->dgst.dgst_req = NULL;
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
	case LCA_TE_ALG_MAIN_CBCMAC:
		te_buf_mgr_free_memlist(&areq_ctx->dgst.cmac_req->amac.in);
		kfree(areq_ctx->dgst.cmac_req);
		areq_ctx->dgst.cmac_req = NULL;
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}
	mutex_unlock(&ctx->mutex);
	ahash_request_complete(req, err);
	pm_runtime_put_autosuspend(dev);
}

static int lca_te_ahash_digest(struct ahash_request *req)
{
	int rc = -1;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct scatterlist *src = req->src;

	mutex_lock(&ctx->mutex);
	dev_dbg(dev, "algo (0x%x) ishamc:%d\n", ctx->alg, ctx->is_hmac);
	pm_runtime_get_sync(dev);
	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			areq_ctx->dgst.hmac_req = kmalloc(sizeof(te_hmac_request_t), GFP_KERNEL);
			if (!areq_ctx->dgst.hmac_req) {
				rc = -ENOMEM;
				goto err;
			}
			areq_ctx->dgst.hmac_req->base.flags = req->base.flags;
			areq_ctx->dgst.hmac_req->base.completion = te_ahash_complete;
			areq_ctx->dgst.hmac_req->base.data = req;
			areq_ctx->dgst.hmac_req->hmac.mac = req->result;
			areq_ctx->dgst.hmac_req->hmac.maclen = crypto_ahash_digestsize(tfm);
			areq_ctx->dgst.hmac_req->hmac.key.type = TE_KEY_TYPE_USER;
			areq_ctx->dgst.hmac_req->hmac.key.user.key = ctx->mackey;
			areq_ctx->dgst.hmac_req->hmac.key.user.keybits = ctx->keylen*BITS_IN_BYTE;

			rc = te_buf_mgr_gen_memlist(src, req->nbytes, &areq_ctx->dgst.hmac_req->hmac.in);
			if (rc != TE_SUCCESS) {
				kfree(areq_ctx->dgst.hmac_req);
				areq_ctx->dgst.hmac_req = NULL;
				goto err;
			}
			rc = te_ahmac(ctx->drvdata->h, ctx->alg, areq_ctx->dgst.hmac_req);
			if (rc != TE_SUCCESS) {
				te_buf_mgr_free_memlist(&areq_ctx->dgst.hmac_req->hmac.in);
				kfree(areq_ctx->dgst.hmac_req);
				areq_ctx->dgst.hmac_req = NULL;
				goto err;
			}
		} else {
			areq_ctx->dgst.dgst_req = kmalloc(sizeof(te_dgst_request_t), GFP_KERNEL);
			if (!areq_ctx->dgst.hmac_req) {
				rc = -ENOMEM;
				goto err;
			}
			areq_ctx->dgst.dgst_req->base.flags = req->base.flags;
			areq_ctx->dgst.dgst_req->base.completion = te_ahash_complete;
			areq_ctx->dgst.dgst_req->base.data = req;
			areq_ctx->dgst.dgst_req->dgst.hash = req->result;

			rc = te_buf_mgr_gen_memlist(src, req->nbytes, &areq_ctx->dgst.dgst_req->dgst.in);
			if (rc != TE_SUCCESS) {
				kfree(areq_ctx->dgst.dgst_req);
				areq_ctx->dgst.dgst_req = NULL;
				goto err;
			}
			rc = te_adgst(ctx->drvdata->h, ctx->alg, areq_ctx->dgst.dgst_req);
			if (rc != TE_SUCCESS) {
				te_buf_mgr_free_memlist(&areq_ctx->dgst.dgst_req->dgst.in);
				kfree(areq_ctx->dgst.dgst_req);
				areq_ctx->dgst.dgst_req = NULL;
				goto err;
			}
		}

		break;
	case LCA_TE_ALG_MAIN_CMAC:
		areq_ctx->dgst.cmac_req = kmalloc(sizeof(te_cmac_request_t), GFP_KERNEL);
		if (!areq_ctx->dgst.cmac_req) {
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->dgst.cmac_req->base.flags = req->base.flags;
		areq_ctx->dgst.cmac_req->base.completion = te_ahash_complete;
		areq_ctx->dgst.cmac_req->base.data = req;
		areq_ctx->dgst.cmac_req->amac.mac = req->result;
		areq_ctx->dgst.cmac_req->amac.maclen = crypto_ahash_digestsize(tfm);

		/*free old iv*/
		if (ctx->maciv)
			kfree(ctx->maciv);

		ctx->maciv = (uint8_t *)kzalloc(ctx->inter_digestsize,GFP_KERNEL);
		if (ctx->maciv == NULL) {
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->dgst.cmac_req->amac.iv = ctx->maciv;
		areq_ctx->dgst.cmac_req->amac.key.type = TE_KEY_TYPE_USER;
		areq_ctx->dgst.cmac_req->amac.key.user.key = ctx->mackey;
		areq_ctx->dgst.cmac_req->amac.key.user.keybits = ctx->keylen*BITS_IN_BYTE;
		rc = te_buf_mgr_gen_memlist(src, req->nbytes, &areq_ctx->dgst.cmac_req->amac.in);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->dgst.cmac_req);
			areq_ctx->dgst.cmac_req = NULL;
			kfree(ctx->maciv);
			ctx->maciv = NULL;
			goto err;
		}

		rc = te_acmac(ctx->drvdata->h, _LCA_CMAC_GET_MAIN_ALG(ctx->alg), areq_ctx->dgst.cmac_req);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->dgst.cmac_req->amac.in);
			kfree(areq_ctx->dgst.cmac_req);
			areq_ctx->dgst.cmac_req = NULL;
			kfree(ctx->maciv);
			ctx->maciv = NULL;
			goto err;
		}
		break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		areq_ctx->dgst.cmac_req = kmalloc(sizeof(te_cmac_request_t), GFP_KERNEL);
		if (!areq_ctx->dgst.cmac_req) {
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->dgst.cmac_req->base.flags = req->base.flags;
		areq_ctx->dgst.cmac_req->base.completion = te_ahash_complete;
		areq_ctx->dgst.cmac_req->base.data = req;
		areq_ctx->dgst.cmac_req->amac.mac = req->result;
		areq_ctx->dgst.cmac_req->amac.maclen = crypto_ahash_digestsize(tfm);

		/*free old iv*/
		if (ctx->maciv)
			kfree(ctx->maciv);

		ctx->maciv = (uint8_t *)kzalloc(ctx->inter_digestsize,GFP_KERNEL);
		if (ctx->maciv == NULL) {
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->dgst.cmac_req->amac.iv = ctx->maciv;
		areq_ctx->dgst.cmac_req->amac.key.type = TE_KEY_TYPE_USER;
		areq_ctx->dgst.cmac_req->amac.key.user.key = ctx->mackey;
		areq_ctx->dgst.cmac_req->amac.key.user.keybits = ctx->keylen*BITS_IN_BYTE;

		rc = te_buf_mgr_gen_memlist(src, req->nbytes, &areq_ctx->dgst.cmac_req->amac.in);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->dgst.cmac_req);
			areq_ctx->dgst.cmac_req = NULL;
			kfree(ctx->maciv);
			ctx->maciv = NULL;
			goto err;
		}
		rc = te_acbcmac(ctx->drvdata->h, _LCA_CBCMAC_GET_MAIN_ALG(ctx->alg), areq_ctx->dgst.cmac_req);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->dgst.cmac_req->amac.in);
			kfree(areq_ctx->dgst.cmac_req);
			areq_ctx->dgst.cmac_req = NULL;
			kfree(ctx->maciv);
			ctx->maciv = NULL;
			goto err;
		}
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
		goto err;
	}

	return -EINPROGRESS;
err:
	pm_runtime_put_autosuspend(dev);
	mutex_unlock(&ctx->mutex);
	return rc;
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

	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			rc = te_hmac_export(&ctx->hctx, areq_ctx->priv, &len);
			kfree(areq_ctx->init.hmac_req);
			areq_ctx->init.hmac_req = NULL;
		} else {
			rc = te_dgst_export(&ctx->dctx, areq_ctx->priv, &len);
			kfree(areq_ctx->init.dgst_req);
			areq_ctx->init.dgst_req = NULL;
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
		rc = te_cmac_export(&ctx->cctx, areq_ctx->priv, &len);
		kfree(areq_ctx->init.cmac_req);
		areq_ctx->init.cmac_req = NULL;
        break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		rc = te_cbcmac_export(&ctx->cbctx, areq_ctx->priv, &len);
		kfree(areq_ctx->init.cmac_req);
		areq_ctx->init.cmac_req = NULL;
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}

	if (rc) {
		err = rc;
		dev_err(dev, "Internal error\n");
	} else {
		crypto_hash_alg_common(tfm)->statesize = len;
	}

	mutex_unlock(&ctx->mutex);
	ahash_request_complete(req, err);
	pm_runtime_put_autosuspend(dev);
}

static int lca_te_ahash_init(struct ahash_request *req)
{
	int rc = -1;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	unsigned char tmp[SHA512_DIGEST_SIZE] = {0};

	mutex_lock(&ctx->mutex);
	pm_runtime_get_sync(dev);
	memset(areq_ctx, 0, sizeof(*areq_ctx));
	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			if (ctx->tid != current->pid){
				rc = te_hmac_finish(&ctx->hctx, NULL, 0);
				if (rc != TE_SUCCESS) {
					dev_err(dev, "te_hmac_finish algo (0x%x) ret:%x\n", ctx->alg, rc);
					rc = -EINVAL;
					goto err;
				}
				ctx->tid = current->pid;
			}
			areq_ctx->init.hmac_req = kmalloc(sizeof(te_hmac_request_t), GFP_KERNEL);
			if (!areq_ctx->init.hmac_req) {
				rc = -ENOMEM;
				goto err;
			}
			areq_ctx->init.hmac_req->base.flags = req->base.flags;
			areq_ctx->init.hmac_req->base.completion = te_ahash_init_complete;
			areq_ctx->init.hmac_req->base.data = req;
			areq_ctx->init.hmac_req->st.key.type = TE_KEY_TYPE_USER;
			areq_ctx->init.hmac_req->st.key.user.key = ctx->mackey;
			areq_ctx->init.hmac_req->st.key.user.keybits = ctx->keylen*BITS_IN_BYTE;

			rc = te_hmac_astart(&ctx->hctx, areq_ctx->init.hmac_req);
			if (rc != TE_SUCCESS) {
				dev_err(dev, "te_dgst_astart algo (0x%x) ret:%x\n", ctx->alg, rc);
				kfree(areq_ctx->init.hmac_req);
				areq_ctx->init.hmac_req = NULL;
				goto err;
			}
		} else {
			if (ctx->tid != current->pid){
				rc = te_dgst_finish(&ctx->dctx, tmp);
				if (rc != TE_SUCCESS) {
					rc = -EINVAL;
					goto err;
				}
				ctx->tid = current->pid;
			}

			areq_ctx->init.dgst_req = kmalloc(sizeof(te_dgst_request_t), GFP_KERNEL);
			if (!areq_ctx->init.hmac_req) {
				rc = -ENOMEM;
				goto err;
			}
			areq_ctx->init.dgst_req->base.flags = req->base.flags;
			areq_ctx->init.dgst_req->base.completion = te_ahash_init_complete;
			areq_ctx->init.dgst_req->base.data = req;

			rc = te_dgst_astart(&ctx->dctx, areq_ctx->init.dgst_req);
			if (rc != TE_SUCCESS) {
				kfree(areq_ctx->init.dgst_req);
				areq_ctx->init.dgst_req = NULL;
				goto err;
			}
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
		if (ctx->tid != current->pid && ctx->flag != 0){
			rc = te_cmac_finish(&ctx->cctx, tmp, ctx->blocksize);
			if (rc != TE_SUCCESS) {
				rc = -EINVAL;
				goto err;
			}
			ctx->tid = current->pid;
		}

		rc = te_cmac_setkey(&ctx->cctx, ctx->mackey,
				ctx->keylen*BITS_IN_BYTE);
		if (rc != TE_SUCCESS) {
			rc = -EINVAL;
			goto err;
		}
		ctx->flag = 1;

		areq_ctx->init.cmac_req = kmalloc(sizeof(te_cmac_request_t), GFP_KERNEL);
		if (!areq_ctx->init.cmac_req) {
			rc = -ENOMEM;
			goto err;
		}

		areq_ctx->init.cmac_req->base.flags = req->base.flags;
		areq_ctx->init.cmac_req->base.completion = te_ahash_init_complete;
		areq_ctx->init.cmac_req->base.data = req;
		rc = te_cmac_astart(&ctx->cctx, areq_ctx->init.cmac_req);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->init.cmac_req);
			areq_ctx->init.cmac_req = NULL;
			goto err;
		}
		break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		if (ctx->tid != current->pid && ctx->flag != 0){
			rc = te_cbcmac_free(&ctx->cbctx);
			if (rc != TE_SUCCESS) {
				dev_err(dev, "te_cbcmac_free algo (0x%x) rc:%x\n", ctx->alg, rc);
				rc = -EINVAL;
				goto err;
			}
			rc = te_cbcmac_init(&ctx->cbctx, ctx->drvdata->h,
					_LCA_CBCMAC_GET_MAIN_ALG(ctx->alg));
			if (rc != TE_SUCCESS) {
				dev_err(dev, "te_cbcmac_init algo (0x%x) rc:%x\n", ctx->alg, rc);
				rc = -EINVAL;
				goto err;
			}
			ctx->tid = current->pid;
		}

		rc = te_cbcmac_setkey(&ctx->cbctx, ctx->mackey,
				ctx->keylen*BITS_IN_BYTE);
		if (rc != TE_SUCCESS) {
			rc = -EINVAL;
			goto err;
		}
		ctx->flag = 1;

		/*free old iv*/
		if (ctx->maciv)
			kfree(ctx->maciv);
		ctx->maciv = (uint8_t *)kzalloc(ctx->inter_digestsize,GFP_KERNEL);
		if (ctx->maciv == NULL) {
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->init.cmac_req = kmalloc(sizeof(te_cmac_request_t), GFP_KERNEL);
		if (!areq_ctx->init.hmac_req) {
			kfree(ctx->maciv);
			ctx->maciv = NULL;
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->init.cmac_req->base.flags = req->base.flags;
		areq_ctx->init.cmac_req->base.completion = te_ahash_init_complete;
		areq_ctx->init.cmac_req->base.data = req;
		areq_ctx->init.cmac_req->st.iv = ctx->maciv;

		rc = te_cbcmac_astart(&ctx->cbctx, areq_ctx->init.cmac_req);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->init.cmac_req);
			areq_ctx->init.cmac_req = NULL;
			goto err;
		}
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
		goto err;
	}

	return -EINPROGRESS;
err:
	pm_runtime_put_autosuspend(dev);
	mutex_unlock(&ctx->mutex);
	return rc;
}

static void te_ahash_update_complete(
				struct te_async_request *te_req, int err)
{
	struct ahash_request *req = (struct ahash_request *)te_req->data;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	uint32_t len = crypto_hash_alg_common(tfm)->statesize;
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx->drvdata);

	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			te_hmac_export(&ctx->hctx, areq_ctx->priv, &len);
			te_buf_mgr_free_memlist(&areq_ctx->update.hmac_req->up.in);
			kfree(areq_ctx->update.hmac_req);
			areq_ctx->update.hmac_req = NULL;
		} else {
			te_dgst_export(&ctx->dctx, areq_ctx->priv, &len);
			te_buf_mgr_free_memlist(&areq_ctx->update.dgst_req->up.in);
			kfree(areq_ctx->update.dgst_req);
			areq_ctx->update.dgst_req = NULL;
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
		te_cmac_export(&ctx->cctx, areq_ctx->priv, &len);
		te_buf_mgr_free_memlist(&areq_ctx->update.cmac_req->up.in);
		kfree(areq_ctx->update.cmac_req);
		areq_ctx->update.cmac_req = NULL;
        break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		te_cbcmac_export(&ctx->cbctx, areq_ctx->priv, &len);
		te_buf_mgr_free_memlist(&areq_ctx->update.cmac_req->up.in);
		kfree(areq_ctx->update.cmac_req);
		areq_ctx->update.cmac_req = NULL;
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}

	mutex_unlock(&ctx->mutex);
	ahash_request_complete(req, err);
	pm_runtime_put_autosuspend(dev);
}

static int lca_te_ahash_update(struct ahash_request *req)
{
	int rc = -1;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	uint32_t len = crypto_hash_alg_common(tfm)->statesize;
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct scatterlist *src = req->src;

	mutex_lock(&ctx->mutex);
	pm_runtime_get_sync(dev);
	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			if (ctx->tid != current->pid){
				rc = te_hmac_import(&ctx->hctx, areq_ctx->priv, len);
				if (rc) {
					rc = -EINVAL;
					goto err;
				}
				ctx->tid = current->pid;
			}

			areq_ctx->update.hmac_req = kmalloc(sizeof(te_hmac_request_t), GFP_KERNEL);
			if (!areq_ctx->update.hmac_req) {
				rc = -ENOMEM;
				goto err;
			}
			areq_ctx->update.hmac_req->base.flags = req->base.flags;
			areq_ctx->update.hmac_req->base.completion = te_ahash_update_complete;
			areq_ctx->update.hmac_req->base.data = req;

			rc = te_buf_mgr_gen_memlist(src, req->nbytes, &areq_ctx->update.hmac_req->up.in);
			if (rc != TE_SUCCESS) {
				kfree(areq_ctx->update.hmac_req);
				areq_ctx->update.hmac_req = NULL;
				goto err;
			}

			rc = te_hmac_aupdate(&ctx->hctx, areq_ctx->update.hmac_req);
			if (rc != TE_SUCCESS) {
				te_buf_mgr_free_memlist(&areq_ctx->update.hmac_req->up.in);
				kfree(areq_ctx->update.hmac_req);
				areq_ctx->update.hmac_req = NULL;
				goto err;
			}
		} else {
			if (ctx->tid != current->pid){
				rc = te_dgst_import(&ctx->dctx, areq_ctx->priv, len);
				if (rc) {
					rc = -EINVAL;
					goto err;
				}
				ctx->tid = current->pid;
			}

			areq_ctx->update.dgst_req = kmalloc(sizeof(te_dgst_request_t), GFP_KERNEL);
			if (!areq_ctx->update.dgst_req) {
				rc = -ENOMEM;
				goto err;
			}
			areq_ctx->update.dgst_req->base.flags = req->base.flags;
			areq_ctx->update.dgst_req->base.completion = te_ahash_update_complete;
			areq_ctx->update.dgst_req->base.data = req;

			rc = te_buf_mgr_gen_memlist(src, req->nbytes, &areq_ctx->update.dgst_req->up.in);
			if (rc != TE_SUCCESS) {
				kfree(areq_ctx->update.dgst_req);
				areq_ctx->update.dgst_req = NULL;
				goto err;
			}

			rc = te_dgst_aupdate(&ctx->dctx, areq_ctx->update.dgst_req);
			if (rc != TE_SUCCESS) {
				te_buf_mgr_free_memlist(&areq_ctx->update.dgst_req->up.in);
				kfree(areq_ctx->update.dgst_req);
				areq_ctx->update.dgst_req = NULL;
				goto err;
			}
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
		if (ctx->tid != current->pid){
			rc = te_cmac_import(&ctx->cctx, areq_ctx->priv, len);
			if (rc) {
				rc = -EINVAL;
				goto err;
			}
			ctx->tid = current->pid;
		}

		areq_ctx->update.cmac_req = kmalloc(sizeof(te_cmac_request_t), GFP_KERNEL);
		if (!areq_ctx->update.cmac_req) {
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->update.cmac_req->base.flags = req->base.flags;
		areq_ctx->update.cmac_req->base.completion = te_ahash_update_complete;
		areq_ctx->update.cmac_req->base.data = req;

		rc = te_buf_mgr_gen_memlist(src, req->nbytes, &areq_ctx->update.cmac_req->up.in);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->update.cmac_req);
			areq_ctx->update.cmac_req = NULL;
			goto err;
		}
		rc = te_cmac_aupdate(&ctx->cctx, areq_ctx->update.cmac_req);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->update.cmac_req->up.in);
			kfree(areq_ctx->update.cmac_req);
			areq_ctx->update.cmac_req = NULL;
			goto err;
		}
		break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		if (ctx->tid != current->pid){
			rc = te_cbcmac_import(&ctx->cbctx, areq_ctx->priv, len);
			if (rc) {
				rc = -EINVAL;
				goto err;
			}
			ctx->tid = current->pid;
		}

		areq_ctx->update.cmac_req = kmalloc(sizeof(te_cmac_request_t), GFP_KERNEL);
		if (!areq_ctx->update.cmac_req) {
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->update.cmac_req->base.flags = req->base.flags;
		areq_ctx->update.cmac_req->base.completion = te_ahash_update_complete;
		areq_ctx->update.cmac_req->base.data = req;

		rc = te_buf_mgr_gen_memlist(src, req->nbytes, &areq_ctx->update.cmac_req->up.in);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->update.cmac_req);
			areq_ctx->update.cmac_req = NULL;
			goto err;
		}
		rc = te_cbcmac_aupdate(&ctx->cbctx, areq_ctx->update.cmac_req);
		if (rc != TE_SUCCESS) {
			te_buf_mgr_free_memlist(&areq_ctx->update.cmac_req->up.in);
			kfree(areq_ctx->update.cmac_req);
			areq_ctx->update.cmac_req = NULL;
			goto err;
		}
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
		goto err;
	}

	return -EINPROGRESS;
err:
	pm_runtime_put_autosuspend(dev);
	mutex_unlock(&ctx->mutex);
	return rc;
}

static void te_ahash_final_complete(
				struct te_async_request *te_req, int err)
{
	struct ahash_request *req = (struct ahash_request *)te_req->data;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx->drvdata);

	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			kfree(areq_ctx->final.hmac_req);
			areq_ctx->final.hmac_req = NULL;
		} else {
			kfree(areq_ctx->final.dgst_req);
			areq_ctx->final.dgst_req = NULL;
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
	case LCA_TE_ALG_MAIN_CBCMAC:
		kfree(areq_ctx->final.cmac_req);
		areq_ctx->final.cmac_req = NULL;
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}

	mutex_unlock(&ctx->mutex);
	ahash_request_complete(req, err);
	pm_runtime_put_autosuspend(dev);
}

static int lca_te_ahash_final(struct ahash_request *req)
{
	int rc = -1;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	uint32_t len = crypto_hash_alg_common(tfm)->statesize;
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);

	mutex_lock(&ctx->mutex);
	pm_runtime_get_sync(dev);
	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			if (ctx->tid != current->pid){
				rc = te_hmac_import(&ctx->hctx, areq_ctx->priv, len);
				if (rc) {
					rc = -EINVAL;
					goto err;
				}
				ctx->tid = current->pid;
			}

			areq_ctx->final.hmac_req = kmalloc(sizeof(te_hmac_request_t), GFP_KERNEL);
			if (!areq_ctx->final.hmac_req) {
				rc = -ENOMEM;
				goto err;
			}
			areq_ctx->final.hmac_req->base.flags = req->base.flags;
			areq_ctx->final.hmac_req->base.completion = te_ahash_final_complete;
			areq_ctx->final.hmac_req->base.data = req;
			areq_ctx->final.hmac_req->fin.mac = req->result;
			areq_ctx->final.hmac_req->fin.maclen = crypto_ahash_digestsize(tfm);
			rc = te_hmac_afinish(&ctx->hctx, areq_ctx->final.hmac_req);
			if (rc != TE_SUCCESS) {
				kfree(areq_ctx->final.hmac_req);
				areq_ctx->final.hmac_req = NULL;
				goto err;
			}
		} else {
			if (ctx->tid != current->pid){
				rc = te_dgst_import(&ctx->dctx, areq_ctx->priv, len);
				if (rc) {
					rc = -EINVAL;
					goto err;
				}
				ctx->tid = current->pid;
			}

			areq_ctx->final.dgst_req = kmalloc(sizeof(te_dgst_request_t), GFP_KERNEL);
			if (!areq_ctx->final.dgst_req) {
				rc = -ENOMEM;
				goto err;
			}
			areq_ctx->final.dgst_req->base.flags = req->base.flags;
			areq_ctx->final.dgst_req->base.completion = te_ahash_final_complete;
			areq_ctx->final.dgst_req->base.data = req;
			areq_ctx->final.dgst_req->fin.hash = req->result;
			rc = te_dgst_afinish(&ctx->dctx, areq_ctx->final.dgst_req);
			if (rc != TE_SUCCESS) {
				kfree(areq_ctx->final.dgst_req);
				areq_ctx->final.dgst_req = NULL;
				goto err;
			}
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
		if (ctx->tid != current->pid){
			rc = te_cmac_import(&ctx->cctx, areq_ctx->priv, len);
			if (rc) {
				rc = -EINVAL;
				goto err;
			}
			ctx->tid = current->pid;
		}

		areq_ctx->final.cmac_req = kmalloc(sizeof(te_cmac_request_t), GFP_KERNEL);
		if (!areq_ctx->final.cmac_req) {
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->final.cmac_req->base.flags = req->base.flags;
		areq_ctx->final.cmac_req->base.completion = te_ahash_final_complete;
		areq_ctx->final.cmac_req->base.data = req;
		areq_ctx->final.cmac_req->fin.mac = req->result;
		areq_ctx->final.cmac_req->fin.maclen = crypto_ahash_digestsize(tfm);
		rc = te_cmac_afinish(&ctx->cctx, areq_ctx->final.cmac_req);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->final.cmac_req);
			areq_ctx->final.cmac_req = NULL;
			goto err;
		}
		break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		if (ctx->tid != current->pid){
			rc = te_cbcmac_import(&ctx->cbctx, areq_ctx->priv, len);
			if (rc) {
				rc = -EINVAL;
				goto err;
			}
			ctx->tid = current->pid;
		}

		areq_ctx->final.cmac_req = kmalloc(sizeof(te_cmac_request_t), GFP_KERNEL);
		if (!areq_ctx->final.cmac_req) {
			rc = -ENOMEM;
			goto err;
		}
		areq_ctx->final.cmac_req->base.flags = req->base.flags;
		areq_ctx->final.cmac_req->base.completion = te_ahash_final_complete;
		areq_ctx->final.cmac_req->base.data = req;
		areq_ctx->final.cmac_req->fin.mac = req->result;
		areq_ctx->final.cmac_req->fin.maclen = crypto_ahash_digestsize(tfm);
		rc = te_cbcmac_afinish(&ctx->cbctx, areq_ctx->final.cmac_req);
		if (rc != TE_SUCCESS) {
			kfree(areq_ctx->final.cmac_req);
			areq_ctx->final.cmac_req = NULL;
			goto err;
		}
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
		goto err;
	}

	return -EINPROGRESS;
err:
	pm_runtime_put_autosuspend(dev);
	mutex_unlock(&ctx->mutex);
	return rc;
}

static int lca_te_ahash_export(struct ahash_request *req, void *out)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	uint32_t len = crypto_hash_alg_common(tfm)->statesize;
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);

	mutex_lock(&ctx->mutex);
	memcpy(out, areq_ctx->priv, len);
	mutex_unlock(&ctx->mutex);
	return 0;
}

static int lca_te_ahash_import(struct ahash_request *req, const void *in)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	uint32_t len = crypto_ahash_statesize(tfm);
	struct te_ahash_req_ctx *areq_ctx = ahash_request_ctx(req);

	mutex_lock(&ctx->mutex);
	memcpy(areq_ctx->priv, in, len);
	mutex_unlock(&ctx->mutex);
	return 0;
}

static int lca_te_ahash_setkey(struct crypto_ahash *tfm, const u8 *key,
		      unsigned int keylen)
{
	int rc = 0;
	struct te_hash_ctx *ctx = crypto_ahash_ctx(tfm);

	mutex_lock(&ctx->mutex);
	if (ctx->mackey)
		kfree(ctx->mackey);
	ctx->mackey = kmalloc(keylen, GFP_KERNEL);
	if (ctx->mackey == NULL) {
		rc = -ENOMEM;
		goto err;
	}

	memcpy(ctx->mackey, key, keylen);
	ctx->keylen = keylen;
err:
	mutex_unlock(&ctx->mutex);
	return rc;
}

#else
static int lca_te_hmac( te_drv_handle hdl, te_algo_t alg,
			const uint8_t *in, unsigned int len, uint8_t *mac,
			unsigned int maclen)
{
	int rc = 0;
	te_hmac_ctx_t hmac_ctx = {0};

	rc = te_hmac_init(&hmac_ctx, hdl, alg);
	if (rc != 0)
		return rc;
	rc = te_hmac_update(&hmac_ctx, in, len);
	if (rc != 0)
		goto finish;
	rc = te_hmac_finish(&hmac_ctx, mac, maclen);
	if (rc != 0)
		goto finish;
finish:
	te_hmac_free(&hmac_ctx);
	return rc;
}


static int lca_te_shash_digest(struct shash_desc *desc, const u8 *data,
			  unsigned int len, u8 *out)
{
	int rc=0;
	struct te_hash_ctx *ctx = crypto_shash_ctx(desc->tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);

	pm_runtime_get_sync(dev);
	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			rc = lca_te_hmac(ctx->drvdata->h, ctx->alg, data, len,
					out, crypto_shash_digestsize(desc->tfm));
		} else {
			rc = te_dgst(ctx->drvdata->h, ctx->alg, data, len, out);
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
	case LCA_TE_ALG_MAIN_CBCMAC:
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}
	pm_runtime_put_autosuspend(dev);
	return rc;
}

static int lca_te_shash_init(struct shash_desc *desc)
{
	int rc = -1;
	uint32_t len = 0;
	struct te_hash_ctx *ctx = crypto_shash_ctx(desc->tfm);
	struct crypto_shash *tfm = desc->tfm;
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	uint8_t * iv;

	pm_runtime_get_sync(dev);
	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			rc = te_hmac_start(&ctx->hctx, ctx->mackey,
				ctx->keylen*BITS_IN_BYTE);
			if (rc != TE_SUCCESS) {
				goto out;
			}

			rc = te_hmac_export(&ctx->hctx, NULL, &len);
		} else {
			rc = te_dgst_start(&ctx->dctx);
			if (rc != TE_SUCCESS) {
				goto out;
			}

			rc = te_dgst_export(&ctx->dctx, NULL, &len);
		}

		break;
	case LCA_TE_ALG_MAIN_CMAC:
		rc = te_cmac_setkey(&ctx->cctx, ctx->mackey,
				ctx->keylen*BITS_IN_BYTE);
		if (rc != TE_SUCCESS) {
			goto out;
		}

		rc = te_cmac_start(&ctx->cctx);
		if (rc != TE_SUCCESS) {
			goto out;
		}

		rc = te_cmac_export(&ctx->cctx, NULL, &len);
		break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		rc = te_cbcmac_setkey(&ctx->cbctx, ctx->mackey,
				ctx->keylen*BITS_IN_BYTE);
		if (rc != TE_SUCCESS) {
			goto out;
		}
		iv= (uint8_t *)kmalloc(ctx->inter_digestsize,GFP_KERNEL);
		if (iv == NULL) {
			rc = -ENOMEM;
			goto out;
		}
		memset(iv, 0, ctx->inter_digestsize);
		rc = te_cbcmac_start(&ctx->cbctx, iv);
		kfree(iv);
		if (rc != TE_SUCCESS) {
			goto out;
		}

		rc = te_cbcmac_export(&ctx->cbctx, NULL, &len);
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}

	/* Expected return code */
	if (rc == TE_ERROR_SHORT_BUFFER) {
		rc = 0;
		crypto_shash_alg(tfm)->statesize = len;
	}
out:
	pm_runtime_put_autosuspend(dev);
	return rc;
}


static int lca_te_shash_update(struct shash_desc *desc, const u8 *data,
			  unsigned int len)
{
	int rc=0;
	struct te_hash_ctx *ctx = crypto_shash_ctx(desc->tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);

	pm_runtime_get_sync(dev);
	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			rc = te_hmac_update(&ctx->hctx, data, len);
		} else {
			rc = te_dgst_update(&ctx->dctx, data, len);
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
		rc = te_cmac_update(&ctx->cctx, len, data);
		break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		rc = te_cbcmac_update(&ctx->cbctx, len, data);
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}
	pm_runtime_put_autosuspend(dev);
	return rc;
}


static int lca_te_shash_final(struct shash_desc *desc, u8 *out)
{
	int rc=0;
	struct te_hash_ctx *ctx = crypto_shash_ctx(desc->tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);

	pm_runtime_get_sync(dev);
	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			rc = te_hmac_finish(&ctx->hctx, out,
					crypto_shash_digestsize(desc->tfm));
		} else {
			rc = te_dgst_finish(&ctx->dctx,out);
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
		rc = te_cmac_finish(&ctx->cctx, out,
					crypto_shash_digestsize(desc->tfm));
		break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		rc = te_cbcmac_finish(&ctx->cbctx, out,
					crypto_shash_digestsize(desc->tfm));
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}
	pm_runtime_put_autosuspend(dev);
	return rc;
}

static int lca_te_shash_export(struct shash_desc *desc, void *out)
{
	int rc = -1;
	struct cypto_shash *tfm = desc->tfm;
	struct te_hash_ctx *ctx = crypto_shash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	uint32_t len = crypto_shash_statesize(tfm);

	dev_dbg(dev, "shash export algo (0x%x) %d\n", ctx->alg, _LCA_GET_MAIN_MODE(ctx->alg));

	pm_runtime_get_sync(dev);
	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			rc = te_hmac_export(&ctx->hctx, out, &len);
		} else {
			rc = te_dgst_export(&ctx->dctx, out, &len);
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
		rc = te_cmac_export(&ctx->cctx, out, &len);
		break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		rc = te_cbcmac_export(&ctx->cbctx, out, &len);
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}

	if (!rc) {
		if (len != crypto_shash_statesize(tfm))
			rc = TE_ERROR_GENERIC;
	}

	pm_runtime_put_autosuspend(dev);
	return rc;
}

static int lca_te_shash_import(struct shash_desc *desc, const void *in)
{
	int rc = -1;
	struct cypto_shash *tfm = desc->tfm;
	struct te_hash_ctx *ctx = crypto_shash_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	uint32_t len = crypto_shash_statesize(tfm);

	dev_dbg(dev, "shash import algo (0x%x) %d\n", ctx->alg, _LCA_GET_MAIN_MODE(ctx->alg));

	pm_runtime_get_sync(dev);
	switch (_LCA_GET_MAIN_MODE(ctx->alg)) {
	case LCA_TE_ALG_MAIN_HASH:
		if (ctx->is_hmac) {
			rc = te_hmac_import(&ctx->hctx, in, len);
		} else {
			rc = te_dgst_import(&ctx->dctx, in, len);
		}
		break;
	case LCA_TE_ALG_MAIN_CMAC:
		rc = te_cmac_import(&ctx->cctx, in, len);
		break;
	case LCA_TE_ALG_MAIN_CBCMAC:
		rc = te_cbcmac_import(&ctx->cbctx, in, len);
		break;
	case LCA_TE_ALG_MAIN_INVALID:
	default:
		dev_err(dev, "Unsupported algo (0x%x)\n", ctx->alg);
	}

	pm_runtime_put_autosuspend(dev);
	return rc;
}

static int lca_te_shash_setkey(struct crypto_shash *tfm, const u8 *key,
			  unsigned int keylen)
{
	struct te_hash_ctx *ctx = crypto_shash_ctx(tfm);

	if (ctx->mackey)
		kfree(ctx->mackey);
	ctx->mackey = kmalloc(keylen, GFP_KERNEL);
	if (ctx->mackey == NULL)
		return -ENOMEM;
	memcpy(ctx->mackey, key, keylen);
	ctx->keylen = keylen;
	return 0;
}
#endif
struct te_hash_template {
	char name[CRYPTO_MAX_ALG_NAME];
	char driver_name[CRYPTO_MAX_ALG_NAME];
	char mac_name[CRYPTO_MAX_ALG_NAME];
	char mac_driver_name[CRYPTO_MAX_ALG_NAME];
	unsigned int blocksize;
	bool synchronize;
#ifdef CFG_TE_ASYNC_EN
	struct ahash_alg template_ahash;
#else
	struct shash_alg template_shash;
#endif
	int alg;
	bool ishash;
	int inter_digestsize;
	struct te_drvdata *drvdata;
};


/* hash descriptors */
#ifdef CFG_TE_ASYNC_EN
static struct te_hash_template te_ahash_algs[] = {
	{
		.name = "md5",
		.driver_name = "md5-te",
		.mac_name = "hmac(md5)",
		.mac_driver_name = "hmac-md5-te",
		.blocksize = MD5_BLOCK_SIZE,
		.synchronize = false,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_MD5,
		.inter_digestsize = MD5_DIGEST_SIZE,
	},
	{
		.name = "sha1",
		.driver_name = "sha1-te",
		.mac_name = "hmac(sha1)",
		.mac_driver_name = "hmac-sha1-te",
		.blocksize = SHA1_BLOCK_SIZE,
		.synchronize = true,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA1,
		.inter_digestsize = SHA1_DIGEST_SIZE,
	},
	{
		.name = "sha224",
		.driver_name = "sha224-te",
		.mac_name = "hmac(sha224)",
		.mac_driver_name = "hmac-sha224-te",
		.blocksize = SHA224_BLOCK_SIZE,
		.synchronize = true,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA224,
		.inter_digestsize = SHA224_DIGEST_SIZE,
	},
	{
		.name = "sha256",
		.driver_name = "sha256-te",
		.mac_name = "hmac(sha256)",
		.mac_driver_name = "hmac-sha256-te",
		.blocksize = SHA256_BLOCK_SIZE,
		.synchronize = true,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA256,
		.inter_digestsize = SHA256_DIGEST_SIZE,
	},
	{
		.name = "sha384",
		.driver_name = "sha384-te",
		.mac_name = "hmac(sha384)",
		.mac_driver_name = "hmac-sha384-te",
		.blocksize = SHA384_BLOCK_SIZE,
		.synchronize = true,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA384,
		.inter_digestsize = SHA384_DIGEST_SIZE,
	},
	{
		.name = "sha512",
		.driver_name = "sha512-te",
		.mac_name = "hmac(sha512)",
		.mac_driver_name = "hmac-sha512-te",
		.blocksize = SHA512_BLOCK_SIZE,
		.synchronize = true,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA512,
		.inter_digestsize = SHA512_DIGEST_SIZE,
	},
	{
		.name = "sm3",
		.driver_name = "sm3-te",
		.mac_name = "hmac(sm3)",
		.mac_driver_name = "hmac-sm3-te",
		.blocksize = SM3_BLOCK_SIZE,
		.synchronize = true,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SM3,
		.inter_digestsize = SM3_DIGEST_SIZE,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = false,
		.alg = TE_ALG_AES_CMAC,
		.inter_digestsize = AES_BLOCK_SIZE,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = false,
		.alg = TE_ALG_SM4_CMAC,
		.inter_digestsize = SM4_BLOCK_SIZE,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = false,
		.alg = TE_ALG_DES_CMAC,
		.inter_digestsize = DES_BLOCK_SIZE,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = false,
		.alg = TE_ALG_TDES_CMAC,
		.inter_digestsize = DES3_EDE_BLOCK_SIZE,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = false,
		.alg = TE_ALG_AES_CBC_MAC_NOPAD,
		.inter_digestsize = AES_BLOCK_SIZE,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = false,
		.alg = TE_ALG_SM4_CBC_MAC_NOPAD,
		.inter_digestsize = SM4_BLOCK_SIZE,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = false,
		.alg = TE_ALG_DES_CBC_MAC_NOPAD,
		.inter_digestsize = DES_BLOCK_SIZE,
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
				.statesize = HASH_MAX_STATESIZE,
			},
		},
		.ishash = false,
		.alg = TE_ALG_TDES_CBC_MAC_NOPAD,
		.inter_digestsize = DES3_EDE_BLOCK_SIZE,
	},
};

#else
static struct te_hash_template te_hash_algs[] = {
	{
		.name = "md5",
		.driver_name = "md5-te",
		.mac_name = "hmac(md5)",
		.mac_driver_name = "hmac-md5-te",
		.blocksize = MD5_BLOCK_SIZE,
		.synchronize = true,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = MD5_DIGEST_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_MD5,
		.inter_digestsize = MD5_DIGEST_SIZE,
	},
	{
		.name = "sha1",
		.driver_name = "sha1-te",
		.mac_name = "hmac(sha1)",
		.mac_driver_name = "hmac-sha1-te",
		.blocksize = SHA1_BLOCK_SIZE,
		.synchronize = true,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = SHA1_DIGEST_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA1,
		.inter_digestsize = SHA1_DIGEST_SIZE,
	},
	{
		.name = "sha224",
		.driver_name = "sha224-te",
		.mac_name = "hmac(sha224)",
		.mac_driver_name = "hmac-sha224-te",
		.blocksize = SHA224_BLOCK_SIZE,
		.synchronize = true,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = SHA224_DIGEST_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA224,
		.inter_digestsize = SHA224_DIGEST_SIZE,
	},
	{
		.name = "sha256",
		.driver_name = "sha256-te",
		.mac_name = "hmac(sha256)",
		.mac_driver_name = "hmac-sha256-te",
		.blocksize = SHA256_BLOCK_SIZE,
		.synchronize = true,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = SHA256_DIGEST_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA256,
		.inter_digestsize = SHA256_DIGEST_SIZE,
	},
	{
		.name = "sha384",
		.driver_name = "sha384-te",
		.mac_name = "hmac(sha384)",
		.mac_driver_name = "hmac-sha384-te",
		.blocksize = SHA384_BLOCK_SIZE,
		.synchronize = true,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = SHA384_DIGEST_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA384,
		.inter_digestsize = SHA384_DIGEST_SIZE,
	},
	{
		.name = "sha512",
		.driver_name = "sha512-te",
		.mac_name = "hmac(sha512)",
		.mac_driver_name = "hmac-sha512-te",
		.blocksize = SHA512_BLOCK_SIZE,
		.synchronize = true,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = SHA512_DIGEST_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SHA512,
		.inter_digestsize = SHA512_DIGEST_SIZE,
	},
	{
		.name = "sm3",
		.driver_name = "sm3-te",
		.mac_name = "hmac(sm3)",
		.mac_driver_name = "hmac-sm3-te",
		.blocksize = SM3_BLOCK_SIZE,
		.synchronize = true,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = SM3_DIGEST_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = true,
		.alg = TE_ALG_HMAC_SM3,
		.inter_digestsize = SM3_DIGEST_SIZE,
	},
	{
		.mac_name = "cmac(aes)",
		.mac_driver_name = "cmac-aes-te",
		.blocksize = AES_BLOCK_SIZE,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = AES_BLOCK_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = false,
		.alg = TE_ALG_AES_CMAC,
		.inter_digestsize = AES_BLOCK_SIZE,
	},
	{
		.mac_name = "cmac(sm4)",
		.mac_driver_name = "cmac-sm4-te",
		.blocksize = SM4_BLOCK_SIZE,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = SM4_BLOCK_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = false,
		.alg = TE_ALG_SM4_CMAC,
		.inter_digestsize = SM4_BLOCK_SIZE,
	},
	{
		.mac_name = "cbcmac(aes)",
		.mac_driver_name = "cbcmac-aes-te",
		.blocksize = AES_BLOCK_SIZE,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = AES_BLOCK_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = false,
		.alg = TE_ALG_AES_CBC_MAC_NOPAD,
		.inter_digestsize = AES_BLOCK_SIZE,
	},
	{
		.mac_name = "cbcmac(sm4)",
		.mac_driver_name = "cbcmac-sm4-te",
		.blocksize = SM4_BLOCK_SIZE,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = SM4_BLOCK_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = false,
		.alg = TE_ALG_SM4_CBC_MAC_NOPAD,
		.inter_digestsize = SM4_BLOCK_SIZE,
	},
	{
		.mac_name = "cbcmac(des)",
		.mac_driver_name = "cbcmac-des-te",
		.blocksize = DES_BLOCK_SIZE,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = DES_BLOCK_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = false,
		.alg = TE_ALG_DES_CBC_MAC_NOPAD,
		.inter_digestsize = DES_BLOCK_SIZE,
	},
	{
		.mac_name = "cbcmac(des3_ede)",
		.mac_driver_name = "cbcmac-3des-te",
		.blocksize = DES3_EDE_BLOCK_SIZE,
		.template_shash = {
			.init = lca_te_shash_init,
			.update = lca_te_shash_update,
			.final = lca_te_shash_final,
			.digest = lca_te_shash_digest,
			.export = lca_te_shash_export,
			.import = lca_te_shash_import,
			.setkey = lca_te_shash_setkey,
			.digestsize = DES3_EDE_BLOCK_SIZE,
			.statesize = HASH_MAX_STATESIZE,
		},
		.ishash = false,
		.alg = TE_ALG_TDES_CBC_MAC_NOPAD,
		.inter_digestsize = DES3_EDE_BLOCK_SIZE,
	},
};
#endif

static struct te_hash_alg *te_hash_create_alg(
				struct te_hash_template *template, bool is_hmac)
{
	struct te_hash_alg *t_crypto_alg;
	struct crypto_alg *alg;
#ifdef CFG_TE_ASYNC_EN
	struct ahash_alg *halg;
#else
	struct shash_alg *halg;
#endif

	t_crypto_alg = kzalloc(sizeof(*t_crypto_alg), GFP_KERNEL);
	if (!t_crypto_alg) {
		return ERR_PTR(-ENOMEM);
	}
#ifdef CFG_TE_ASYNC_EN
	t_crypto_alg->ahash_alg = template->template_ahash;
	halg = &t_crypto_alg->ahash_alg;
	alg = &halg->halg.base;
#else
	t_crypto_alg->shash_alg = template->template_shash;
	halg = &t_crypto_alg->shash_alg;
	alg = &halg->base;
#endif

	if (template->ishash && !is_hmac) {
		halg->setkey = NULL;
		t_crypto_alg->alg = TE_ALG_MD5 - 1 +
						TE_ALG_GET_MAIN_ALG(template->alg);
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
#ifdef CFG_TE_ASYNC_EN
	alg->cra_flags = CRYPTO_ALG_ASYNC;
#else
	alg->cra_flags = CRYPTO_ALG_TYPE_SHASH;
#endif
	//alg->cra_type = &crypto_shash_type;

	t_crypto_alg->inter_digestsize = template->inter_digestsize;
	t_crypto_alg->blocksize = template->blocksize;
	t_crypto_alg->is_hmac = is_hmac;

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
#ifdef CFG_TE_ASYNC_EN
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
				dev_dbg(dev,"Registered %s\n",
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

#else
	/* shash registration */
	for (alg = 0; alg < ARRAY_SIZE(te_hash_algs); alg++) {
		struct te_hash_alg *t_alg;
		if (te_hash_algs[alg].ishash) {
			/* register hmac version */
			t_alg = te_hash_create_alg(&te_hash_algs[alg], true);
			if (IS_ERR(t_alg)) {
				rc = PTR_ERR(t_alg);
				dev_err(dev,"%s alg allocation failed\n",
						te_hash_algs[alg].mac_driver_name);
				goto fail;
			}
			t_alg->drvdata = drvdata;

			rc = crypto_register_shash(&t_alg->shash_alg);
			if (unlikely(rc)) {
				dev_err(dev,"%s alg registration failed\n",
						te_hash_algs[alg].mac_driver_name);
				kfree(t_alg);
				goto fail;
			} else {
				list_add_tail(&t_alg->entry,
						  &hash_handle->hash_list);
			}
			/* register hash version */
			t_alg = te_hash_create_alg(&te_hash_algs[alg], false);
			if (IS_ERR(t_alg)) {
				rc = PTR_ERR(t_alg);
				dev_err(dev,"%s alg allocation failed\n",
						te_hash_algs[alg].driver_name);
				goto fail;
			}
			t_alg->drvdata = drvdata;

			rc = crypto_register_shash(&t_alg->shash_alg);
			if (unlikely(rc)) {
				dev_err(dev,"%s alg registration failed\n",
						te_hash_algs[alg].driver_name);
				kfree(t_alg);
				goto fail;
			} else {
				list_add_tail(&t_alg->entry, &hash_handle->hash_list);
			}
		}else {
			/* register cmac and cbcmac version */
			t_alg = te_hash_create_alg(&te_hash_algs[alg], false);
			if (IS_ERR(t_alg)) {
				rc = PTR_ERR(t_alg);
				dev_err(dev,"%s alg allocation failed\n",
						te_hash_algs[alg].driver_name);
				goto fail;
			}
			t_alg->drvdata = drvdata;

			rc = crypto_register_shash(&t_alg->shash_alg);
			if (unlikely(rc)) {
				dev_err(dev,"%s alg registration failed\n",
						te_hash_algs[alg].driver_name);
				kfree(t_alg);
				goto fail;
			} else {
				list_add_tail(&t_alg->entry, &hash_handle->hash_list);
			}
		}
	}
#endif
	return 0;

fail:
	kfree(drvdata->hash_handle);
	drvdata->hash_handle = NULL;
	return rc;
}

int lca_te_hash_free(struct te_drvdata *drvdata)
{
	struct te_hash_alg *t_hash_alg, *hash_n;
	struct te_hash_handle *hash_handle = drvdata->hash_handle;

	if (hash_handle) {
		list_for_each_entry_safe(t_hash_alg, hash_n, &hash_handle->hash_list, entry) {
#ifdef CFG_TE_ASYNC_EN
			crypto_unregister_ahash(&t_hash_alg->ahash_alg);
#else
			crypto_unregister_shash(&t_hash_alg->shash_alg);
#endif
			list_del(&t_hash_alg->entry);
			kfree(t_hash_alg);
		}

		kfree(hash_handle);
		drvdata->hash_handle = NULL;
	}
	return 0;
}
