//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/scatterlist.h>
#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <crypto/internal/rsa.h>
#include <crypto/internal/kpp.h>
#include <crypto/kpp.h>
#include <crypto/internal/rng.h>
#include <crypto/rng.h>
#include <crypto/ecdh.h>
#include <crypto/dh.h>

#include <crypto/scatterwalk.h>

#include "te_dhm.h"
#include "te_ecp.h"
#include "te_ecdh.h"

#include "lca_te_driver.h"


#define TE_LCA_CHECK_RET_GO                                                    \
    do {                                                                       \
        if ((TE_SUCCESS) != (rc)) {                                            \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

#define TE_ECDH_MAX_SIZE   ( 2*66 + 1 )


struct te_kpp_handle {
	struct list_head kpp_list;
};

struct te_ecdh_ctx {
	unsigned int curve_id;
	te_ecp_group_t te_grp;
	te_bn_t *d;
	te_bn_t *k;
	te_ecp_point_t Q;
	te_ecp_point_t other_Q;

	size_t privkey_sz;
};

struct te_dh_ctx {
	te_bn_t *P;
	te_bn_t *G;
	te_bn_t *X;
	te_bn_t *GX;
	te_bn_t *GY;
	te_bn_t *K;
	unsigned int p_size;
	unsigned int x_size;
};


struct te_kpp_ctx {
	struct te_drvdata *drvdata;
	bool is_dh;
	union {
		struct te_ecdh_ctx ecdh;
		struct te_dh_ctx dh;
	}u;
};

struct te_kpp_alg {
	struct list_head entry;
	bool is_dh;
	struct te_drvdata *drvdata;
	struct kpp_alg kpp_alg;
};

struct te_kpp_template {
	char name[CRYPTO_MAX_ALG_NAME];
	char driver_name[CRYPTO_MAX_ALG_NAME];
	struct kpp_alg kpp;
	bool is_dh;
	struct te_drvdata *drvdata;
};

struct te_kpp_req_ctx {
	union {
		te_dhm_request_t dhm_req;
		te_ecdh_request_t ecdh_req;
	}u;
};

static void te_kpp_free_key_bufs(struct te_kpp_ctx *ctx)
{

	if (ctx->is_dh) {
		te_bn_free(ctx->u.dh.P);
		te_bn_free(ctx->u.dh.G);
		te_bn_free(ctx->u.dh.X);
		te_bn_free(ctx->u.dh.GX);
		te_bn_free(ctx->u.dh.GY);
		te_bn_free(ctx->u.dh.K);
	} else {
		te_bn_free(ctx->u.ecdh.d);
		te_ecp_point_free(&ctx->u.ecdh.Q);
		te_bn_free(ctx->u.ecdh.k);
		te_ecp_point_free(&ctx->u.ecdh.other_Q);
		te_ecp_group_free(&ctx->u.ecdh.te_grp);
	}
}

static int te_kpp_init_key_bufs(struct te_kpp_ctx *ctx)
{
	int rc=0;

	if (ctx->is_dh) {
		rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.dh.P);
		TE_LCA_CHECK_RET_GO;
		rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.dh.G);
		TE_LCA_CHECK_RET_GO;
		rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.dh.X);
		TE_LCA_CHECK_RET_GO;
		rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.dh.GX);
		TE_LCA_CHECK_RET_GO;
		rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.dh.GY);
		TE_LCA_CHECK_RET_GO;
		rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.dh.K);
		TE_LCA_CHECK_RET_GO;
	} else {
		rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.ecdh.d);
		TE_LCA_CHECK_RET_GO;
		rc = te_ecp_point_init(ctx->drvdata->h, &ctx->u.ecdh.Q);
		TE_LCA_CHECK_RET_GO;
		rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.ecdh.k);
		TE_LCA_CHECK_RET_GO;
		rc = te_ecp_point_init(ctx->drvdata->h, &ctx->u.ecdh.other_Q);
		TE_LCA_CHECK_RET_GO;
		rc = te_ecp_group_init(ctx->drvdata->h, &ctx->u.ecdh.te_grp);
		TE_LCA_CHECK_RET_GO;
	}

	return 0;

finish:
	te_kpp_free_key_bufs(ctx);
	return rc;
}


static int get_random_numbers(u8 *buf, unsigned int len)
{
	struct crypto_rng *rng = NULL;
	char *drbg = "drbg_nopr_sha256"; /* Hash DRBG with SHA-256, no PR */
	int ret;

	if (!buf || !len) {
		pr_err("No output buffer provided\n");
		return -EINVAL;
	}

	rng = crypto_alloc_rng(drbg, 0, 0);
	if (IS_ERR(rng)) {
		pr_err("could not allocate RNG handle for %s\n", drbg);
		return PTR_ERR(rng);
	}

	ret = crypto_rng_reset(rng, NULL, crypto_rng_seedsize(rng));
	if (ret) {
		pr_err("RNG reset fail REt:%d\n",ret);
		goto finish;
	}
	ret = crypto_rng_get_bytes(rng, buf, len);
	if (ret < 0)
		pr_err("generation of random numbers failed ret:%d\n",ret);

finish:
	crypto_free_rng(rng);
	return (ret<0?ret:0);
}

static int te_rng( void *p_rng, unsigned char *output, size_t output_len )
{
	return get_random_numbers(output, output_len);
}

static unsigned int te_ecdh_max_size(struct crypto_kpp *tfm)
{
	return TE_ECDH_MAX_SIZE;
}
static int te_ecdh_set_secret(struct crypto_kpp *tfm, const void *buf,
				 unsigned int len)
{
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct ecdh params;
	int rc = -EINVAL;


	if (crypto_ecdh_decode_key(buf, len, &params) < 0) {
		dev_err(dev, "crypto_ecdh_decode_key failed\n");
		return -EINVAL;
	}

	pm_runtime_get_sync(dev);
#if 0
	if (ECC_CURVE_NIST_P192 == params.curve_id)
		ctx->u.ecdh.curve_id = TE_ECP_DP_SECP192R1;
	else if (ECC_CURVE_NIST_P256 == params.curve_id)
		ctx->u.ecdh.curve_id = TE_ECP_DP_SECP256R1;
	else
		goto finish;
#endif

	ctx->u.ecdh.privkey_sz = params.key_size;

	if (params.key && params.key_size) {
		rc = te_bn_import(ctx->u.ecdh.d, params.key, params.key_size, 1);
		TE_LCA_CHECK_RET_GO;
	} else {
		rc = te_bn_import_s32(ctx->u.ecdh.d, 0);
		TE_LCA_CHECK_RET_GO;
	}

	rc = te_ecp_group_load(&ctx->u.ecdh.te_grp, ctx->u.ecdh.curve_id);
	TE_LCA_CHECK_RET_GO;

finish:
	pm_runtime_put_autosuspend(dev);

	return rc;
}

static void te_ecdh_gen_pubkey_complete(struct te_async_request *te_req, int err)
{
	struct kpp_request *req = (struct kpp_request *)te_req->data;
	struct te_kpp_req_ctx *areq_ctx = kpp_request_ctx(req);
	struct crypto_kpp *tfm = crypto_kpp_reqtfm(req);
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	u8 *pubkey = NULL;
	size_t pubkey_sz;
	size_t copied;
	int rc = 0;

	if (err) {
		goto fail;
	}

	pubkey_sz = TE_ECDH_MAX_SIZE;
	pubkey = kzalloc(TE_ECDH_MAX_SIZE, GFP_KERNEL);
	if (!pubkey) {
		err = -ENOMEM;
		goto fail;
	}
	rc = te_ecp_point_export(areq_ctx->u.ecdh_req.gen_public_args.grp,
		areq_ctx->u.ecdh_req.gen_public_args.Q, 0, pubkey,
				&pubkey_sz);
	TE_LCA_CHECK_RET_GO;

	/*"pubkey + 1" means exclude the first byte x004*/
	copied = sg_copy_from_buffer(req->dst, sg_nents(req->dst), pubkey + 1,
					 pubkey_sz - 1);
	if (copied != pubkey_sz - 1)
		rc = -EINVAL;

	/*if private key is not set, update the priv key size to generated*/
	if(!ctx->u.ecdh.privkey_sz)
		ctx->u.ecdh.privkey_sz = te_bn_bytelen(areq_ctx->u.ecdh_req.gen_public_args.d);

finish:
	err = rc;
	kfree_sensitive(pubkey);
fail:
	kpp_request_complete(req, err);
	pm_runtime_put_autosuspend(dev);
}

static int te_ecdh_generate_public_key(struct kpp_request *req)
{
	int rc = 0;
	struct crypto_kpp *tfm = crypto_kpp_reqtfm(req);
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_kpp_req_ctx *areq_ctx = kpp_request_ctx(req);

	memset(areq_ctx, 0, sizeof(*areq_ctx));

	areq_ctx->u.ecdh_req.gen_public_args.d = ctx->u.ecdh.d;
	areq_ctx->u.ecdh_req.gen_public_args.Q = &ctx->u.ecdh.Q;
	areq_ctx->u.ecdh_req.gen_public_args.grp = &ctx->u.ecdh.te_grp;
	areq_ctx->u.ecdh_req.gen_public_args.f_rng = te_rng;
	areq_ctx->u.ecdh_req.gen_public_args.p_rng = NULL;
	areq_ctx->u.ecdh_req.base.completion = te_ecdh_gen_pubkey_complete;
	areq_ctx->u.ecdh_req.base.flags = req->base.flags;
	areq_ctx->u.ecdh_req.base.data = req;

	pm_runtime_get_sync(dev);

	rc = te_ecdh_gen_public_async(&areq_ctx->u.ecdh_req);
	if (rc != TE_SUCCESS) {
		pm_runtime_put_autosuspend(dev);
	}

	return ((rc == TE_SUCCESS) ? (-EINPROGRESS):rc);
}

static void te_ecdh_compute_shared_secret_complete(
							struct te_async_request *te_req, int err)
{
	struct kpp_request *req = (struct kpp_request *)te_req->data;
	struct crypto_kpp *tfm = crypto_kpp_reqtfm(req);
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_kpp_req_ctx *areq_ctx = kpp_request_ctx(req);
	u8 *secret = NULL;
	size_t secret_sz;
	size_t copied;
	int rc = 0;

	if (err) {
		goto fail;
	}
	secret_sz = te_bn_bytelen(areq_ctx->u.ecdh_req.compute_shared_args.K);
	secret = kzalloc(secret_sz, GFP_KERNEL);
	if (!secret) {
		err = -ENOMEM;
		goto fail;
	}
	rc = te_bn_export(areq_ctx->u.ecdh_req.compute_shared_args.K, secret,
				secret_sz);
	err = rc;
	TE_LCA_CHECK_RET_GO;

	copied = sg_copy_from_buffer(req->dst, sg_nents(req->dst), secret,
					 secret_sz);
	if (copied != secret_sz)
		err = -EINVAL;

finish:
	kfree_sensitive(secret);
fail:
	kpp_request_complete(req, err);
	pm_runtime_put_autosuspend(dev);
}

static int te_ecdh_compute_shared_secret(struct kpp_request *req)
{
	struct crypto_kpp *tfm = crypto_kpp_reqtfm(req);
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	u8 *pubkey;
	size_t pubkey_sz;
	size_t copied;
	int rc = -ENOMEM;
	struct te_kpp_req_ctx *areq_ctx = kpp_request_ctx(req);

	memset(areq_ctx, 0, sizeof(*areq_ctx));
	/*add the first byte 0x04*/
	pubkey_sz = 2*((ctx->u.ecdh.te_grp.pbits+7)/8) + 1;
	pubkey = kzalloc(pubkey_sz, GFP_KERNEL);
	if (!pubkey)
		return -ENOMEM;

	pubkey[0] = 0x04;

	pm_runtime_get_sync(dev);

	copied = sg_copy_to_buffer(req->src, 1, pubkey + 1,
				   pubkey_sz - 1);
	if (copied != pubkey_sz - 1) {
		rc = -EINVAL;
		goto free_pubkey;
	}

	areq_ctx->u.ecdh_req.compute_shared_args.d = ctx->u.ecdh.d;
	areq_ctx->u.ecdh_req.compute_shared_args.K = ctx->u.ecdh.k;
	areq_ctx->u.ecdh_req.compute_shared_args.grp = &ctx->u.ecdh.te_grp;
	areq_ctx->u.ecdh_req.compute_shared_args.other_Q = &ctx->u.ecdh.other_Q;
	areq_ctx->u.ecdh_req.compute_shared_args.f_rng = te_rng;
	areq_ctx->u.ecdh_req.compute_shared_args.p_rng = NULL;
	areq_ctx->u.ecdh_req.base.completion = te_ecdh_compute_shared_secret_complete;
	areq_ctx->u.ecdh_req.base.flags = req->base.flags;
	areq_ctx->u.ecdh_req.base.data = req;

	rc = te_ecp_point_import(areq_ctx->u.ecdh_req.compute_shared_args.grp,
					&ctx->u.ecdh.other_Q, 0, pubkey, pubkey_sz);
	if (rc != TE_SUCCESS) {
		goto free_pubkey;
	}

	rc = te_ecdh_compute_shared_async(&areq_ctx->u.ecdh_req);

free_pubkey:
	if (rc != TE_SUCCESS) {
		pm_runtime_put_autosuspend(dev);
	}

	kfree(pubkey);
	return ((rc == TE_SUCCESS) ? (-EINPROGRESS):rc);
}

static unsigned int te_dh_max_size(struct crypto_kpp *tfm)
{
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);

	return ctx->u.dh.p_size;
}

static int te_dh_set_secret(struct crypto_kpp *tfm, const void *buf,
				 unsigned int len)
{
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct dh params;
	int rc = 0;


	if (crypto_dh_decode_key(buf, len, &params) < 0) {
		dev_err(dev, "crypto_ecdh_decode_key failed\n");
		return -EINVAL;
	}

	pm_runtime_get_sync(dev);

	ctx->u.dh.p_size = params.p_size;
	ctx->u.dh.x_size = params.key_size;
	rc = te_bn_import(ctx->u.dh.P, params.p, params.p_size, 1);
	TE_LCA_CHECK_RET_GO;
	rc = te_bn_import(ctx->u.dh.G, params.g, params.g_size, 1);
	TE_LCA_CHECK_RET_GO;
	if(!params.key_size)
		rc = te_bn_import_s32(ctx->u.dh.X, 0);
	else
		rc = te_bn_import(ctx->u.dh.X, params.key, params.key_size, 1);
	TE_LCA_CHECK_RET_GO;

finish:
	pm_runtime_put_autosuspend(dev);
	return rc;
}

static void te_dh_gen_pubkey_complete(struct te_async_request *te_req, int err)
{
	struct kpp_request *req = (struct kpp_request *)te_req->data;
	struct te_kpp_req_ctx *areq_ctx = kpp_request_ctx(req);
	struct crypto_kpp *tfm = crypto_kpp_reqtfm(req);
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	u8 *pubkey = NULL;
	size_t pubkey_sz;
	size_t copied;
	int rc = 0;

	if (err) {
		goto fail;
	}

	pubkey_sz = ctx->u.dh.p_size;
	pubkey = kzalloc(pubkey_sz, GFP_KERNEL);
	if (!pubkey) {
		err = -ENOMEM;
		goto fail;
	}
	rc = te_bn_export(areq_ctx->u.dhm_req.make_public_args.GX,
			pubkey, pubkey_sz);
	TE_LCA_CHECK_RET_GO;

	copied = sg_copy_from_buffer(req->dst, sg_nents(req->dst), pubkey,
					 pubkey_sz);
	if (copied != pubkey_sz)
		rc = -EINVAL;
finish:
	err = rc;
	if(pubkey)
		kfree_sensitive(pubkey);
fail:
	kpp_request_complete(req, err);
	pm_runtime_put_autosuspend(dev);
}

static int te_dh_generate_public_key(struct kpp_request *req)
{
	int rc = 0;
	struct crypto_kpp *tfm = crypto_kpp_reqtfm(req);
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_kpp_req_ctx *areq_ctx = kpp_request_ctx(req);

	memset(areq_ctx, 0, sizeof(*areq_ctx));

	areq_ctx->u.dhm_req.make_public_args.P = ctx->u.dh.P;
	areq_ctx->u.dhm_req.make_public_args.G = ctx->u.dh.G;
	areq_ctx->u.dhm_req.make_public_args.X = ctx->u.dh.X;
	areq_ctx->u.dhm_req.make_public_args.x_size = ctx->u.dh.x_size;
	areq_ctx->u.dhm_req.make_public_args.GX = ctx->u.dh.GX;
	areq_ctx->u.dhm_req.make_public_args.f_rng = te_rng;
	areq_ctx->u.dhm_req.make_public_args.p_rng = NULL;
	areq_ctx->u.dhm_req.base.completion = te_dh_gen_pubkey_complete;
	areq_ctx->u.dhm_req.base.flags = req->base.flags;
	areq_ctx->u.dhm_req.base.data = req;

	pm_runtime_get_sync(dev);

	rc = te_dhm_make_public_async(&areq_ctx->u.dhm_req);
	if (rc != TE_SUCCESS) {
		pm_runtime_put_autosuspend(dev);
	}

	return ((rc == TE_SUCCESS) ? (-EINPROGRESS):rc);
}

static void te_dh_compute_shared_secret_complete(
							struct te_async_request *te_req, int err)
{
	struct kpp_request *req = (struct kpp_request *)te_req->data;
	struct te_kpp_req_ctx *areq_ctx = kpp_request_ctx(req);
	struct crypto_kpp *tfm = crypto_kpp_reqtfm(req);
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	u8 *secret = NULL;
	size_t secret_sz;
	size_t copied;
	int rc = 0;

	/*free the temporay bn first*/
	if (areq_ctx->u.dhm_req.compute_shared_args.pX)
		te_bn_free(areq_ctx->u.dhm_req.compute_shared_args.pX);
	if (areq_ctx->u.dhm_req.compute_shared_args.Vi)
		te_bn_free(areq_ctx->u.dhm_req.compute_shared_args.Vi);
	if (areq_ctx->u.dhm_req.compute_shared_args.Vf)
		te_bn_free(areq_ctx->u.dhm_req.compute_shared_args.Vf);

	if (err) {
		goto fail;
	}
	secret_sz = ctx->u.dh.p_size;
	secret = kzalloc(secret_sz, GFP_KERNEL);
	if (!secret) {
		err = -ENOMEM;
		goto fail;
	}
	rc = te_bn_export(areq_ctx->u.dhm_req.compute_shared_args.K,
				secret, secret_sz);
	err = rc;
	TE_LCA_CHECK_RET_GO;

	copied = sg_copy_from_buffer(req->dst, sg_nents(req->dst), secret,
					 secret_sz);
	if (copied != secret_sz)
		err = -EINVAL;
finish:
	kfree_sensitive(secret);
fail:
	kpp_request_complete(req, err);
	pm_runtime_put_autosuspend(dev);
}

static int te_dh_compute_shared_secret(struct kpp_request *req)
{
	struct crypto_kpp *tfm = crypto_kpp_reqtfm(req);
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	u8 *pubkey;
	size_t pubkey_sz;
	size_t copied;
	int rc = -ENOMEM;
	struct te_kpp_req_ctx *areq_ctx = kpp_request_ctx(req);

	memset(areq_ctx, 0, sizeof(*areq_ctx));

	pm_runtime_get_sync(dev);

	rc = te_bn_alloc(ctx->drvdata->h, 0, &areq_ctx->u.dhm_req.compute_shared_args.pX);
	TE_LCA_CHECK_RET_GO;
	rc = te_bn_alloc(ctx->drvdata->h, 0, &areq_ctx->u.dhm_req.compute_shared_args.Vi);
	TE_LCA_CHECK_RET_GO;
	rc = te_bn_alloc(ctx->drvdata->h, 0, &areq_ctx->u.dhm_req.compute_shared_args.Vf);
	TE_LCA_CHECK_RET_GO;
	rc = te_bn_import_s32(areq_ctx->u.dhm_req.compute_shared_args.pX, 0);
	TE_LCA_CHECK_RET_GO;
	rc = te_bn_import_s32(areq_ctx->u.dhm_req.compute_shared_args.Vi, 0);
	TE_LCA_CHECK_RET_GO;
	rc = te_bn_import_s32(areq_ctx->u.dhm_req.compute_shared_args.Vf, 0);
	TE_LCA_CHECK_RET_GO;
	pubkey_sz = ctx->u.dh.p_size;
	pubkey = kmalloc(pubkey_sz, GFP_KERNEL);
	if (!pubkey) {
		rc = -ENOMEM;
		goto finish;
	}

	copied = sg_copy_to_buffer(req->src, 1, pubkey,
				   pubkey_sz);
	if (copied != pubkey_sz) {
		rc = -EINVAL;
		goto free_pubkey;
	}

	areq_ctx->u.dhm_req.compute_shared_args.P = ctx->u.dh.P;
	areq_ctx->u.dhm_req.compute_shared_args.G = ctx->u.dh.G;
	areq_ctx->u.dhm_req.compute_shared_args.X = ctx->u.dh.X;
	areq_ctx->u.dhm_req.compute_shared_args.GY = ctx->u.dh.GY;
	areq_ctx->u.dhm_req.compute_shared_args.K = ctx->u.dh.K;
	areq_ctx->u.dhm_req.compute_shared_args.f_rng = te_rng;
	areq_ctx->u.dhm_req.compute_shared_args.p_rng = NULL;
	areq_ctx->u.dhm_req.base.completion = te_dh_compute_shared_secret_complete;
	areq_ctx->u.dhm_req.base.flags = req->base.flags;
	areq_ctx->u.dhm_req.base.data = req;

	rc = te_bn_import(ctx->u.dh.GY, pubkey, pubkey_sz, 1);
	if (rc != TE_SUCCESS) {
		goto free_pubkey;
	}

	rc = te_dhm_compute_shared_async(&areq_ctx->u.dhm_req);

free_pubkey:
	kfree(pubkey);
finish:
	if(rc != TE_SUCCESS) {
		if (areq_ctx->u.dhm_req.compute_shared_args.pX)
			te_bn_free(areq_ctx->u.dhm_req.compute_shared_args.pX);
		if (areq_ctx->u.dhm_req.compute_shared_args.Vi)
			te_bn_free(areq_ctx->u.dhm_req.compute_shared_args.Vi);
		if (areq_ctx->u.dhm_req.compute_shared_args.Vf)
			te_bn_free(areq_ctx->u.dhm_req.compute_shared_args.Vf);

		pm_runtime_put_autosuspend(dev);
	}
	return ((rc == TE_SUCCESS) ? (-EINPROGRESS):rc);
}

static int te_kpp_init(struct crypto_kpp *tfm)
{
	int rc = -1;
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct kpp_alg *alg = crypto_kpp_alg(tfm);
	struct te_kpp_alg *te_alg =
			container_of(alg, struct te_kpp_alg, kpp_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	memset(ctx, 0, sizeof(*ctx));

	ctx->drvdata = te_alg->drvdata;
	ctx->is_dh = te_alg->is_dh;

	pm_runtime_get_sync(dev);

	rc = te_kpp_init_key_bufs(ctx);

	pm_runtime_put_autosuspend(dev);

	return rc;
}

static int te_kpp_p192_init(struct crypto_kpp *tfm)
{
	int rc = -1;
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct kpp_alg *alg = crypto_kpp_alg(tfm);
	struct te_kpp_alg *te_alg =
			container_of(alg, struct te_kpp_alg, kpp_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	memset(ctx, 0, sizeof(*ctx));

	ctx->drvdata = te_alg->drvdata;
	ctx->is_dh = te_alg->is_dh;

	ctx->u.ecdh.curve_id = TE_ECP_DP_SECP192R1;

	pm_runtime_get_sync(dev);

	rc = te_kpp_init_key_bufs(ctx);

	pm_runtime_put_autosuspend(dev);

	return rc;
}

static int te_kpp_p256_init(struct crypto_kpp *tfm)
{
	int rc = -1;
	struct te_kpp_ctx *ctx = kpp_tfm_ctx(tfm);
	struct kpp_alg *alg = crypto_kpp_alg(tfm);
	struct te_kpp_alg *te_alg =
			container_of(alg, struct te_kpp_alg, kpp_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	memset(ctx, 0, sizeof(*ctx));

	ctx->drvdata = te_alg->drvdata;
	ctx->is_dh = te_alg->is_dh;

	ctx->u.ecdh.curve_id = TE_ECP_DP_SECP256R1;

	pm_runtime_get_sync(dev);

	rc = te_kpp_init_key_bufs(ctx);

	pm_runtime_put_autosuspend(dev);

	return rc;
}

static void te_kpp_exit(struct crypto_kpp *tfm)
{
	struct te_kpp_ctx *ctx = crypto_tfm_ctx(&tfm->base);

	te_kpp_free_key_bufs(ctx);
}



static struct te_kpp_template kpp_algs[] = {
	{
		.name = "dh",
		.driver_name = "dh-te",
		.kpp = {
			.set_secret = te_dh_set_secret,
			.generate_public_key = te_dh_generate_public_key,
			.compute_shared_secret = te_dh_compute_shared_secret,
			.init = te_kpp_init,
			.exit = te_kpp_exit,
			.max_size = te_dh_max_size,
			.reqsize	= sizeof(struct te_kpp_req_ctx),
		},
		.is_dh = true,
	},
	{
		.name = "ecdh-nist-p192",
		.driver_name = "ecdh-nist-p192-te",
		.kpp = {
			.set_secret = te_ecdh_set_secret,
			.generate_public_key = te_ecdh_generate_public_key,
			.compute_shared_secret = te_ecdh_compute_shared_secret,
			.init = te_kpp_p192_init,
			.exit = te_kpp_exit,
			.max_size = te_ecdh_max_size,
			.reqsize	= sizeof(struct te_kpp_req_ctx),
		},
		.is_dh = false,
	},
	{
		.name = "ecdh-nist-p256",
		.driver_name = "ecdh-nist-p256-te",
		.kpp = {
			.set_secret = te_ecdh_set_secret,
			.generate_public_key = te_ecdh_generate_public_key,
			.compute_shared_secret = te_ecdh_compute_shared_secret,
			.init = te_kpp_p256_init,
			.exit = te_kpp_exit,
			.max_size = te_ecdh_max_size,
			.reqsize	= sizeof(struct te_kpp_req_ctx),
		},
		.is_dh = false,
	}
};
static struct te_kpp_alg *te_kpp_create_alg(struct te_kpp_template *tmpl)
{
	struct te_kpp_alg *t_alg;
	struct kpp_alg *alg;

	t_alg = kzalloc(sizeof(*t_alg), GFP_KERNEL);
	if (!t_alg) {
		return ERR_PTR(-ENOMEM);
	}
	alg = &tmpl->kpp;

	snprintf(alg->base.cra_name, CRYPTO_MAX_ALG_NAME, "%s", tmpl->name);
	snprintf(alg->base.cra_driver_name, CRYPTO_MAX_ALG_NAME, "%s",
		 tmpl->driver_name);
	alg->base.cra_module = THIS_MODULE;
	alg->base.cra_priority = TE_CRA_PRIO;

	alg->base.cra_ctxsize = sizeof(struct te_kpp_ctx);
	alg->base.cra_flags = CRYPTO_ALG_ASYNC;

	t_alg->kpp_alg = *alg;
	t_alg->is_dh = tmpl->is_dh;

	return t_alg;
}

int lca_te_kpp_free(struct te_drvdata *drvdata)
{
	struct te_kpp_alg *t_alg, *n;
	struct te_kpp_handle *kpp_handle =
		(struct te_kpp_handle *)drvdata->kpp_handle;

	if (kpp_handle) {
		/* Remove registered algs */
		list_for_each_entry_safe(t_alg, n, &kpp_handle->kpp_list, entry) {
			crypto_unregister_kpp(&t_alg->kpp_alg);
			list_del(&t_alg->entry);
			kfree(t_alg);
		}
		kfree(kpp_handle);
		drvdata->kpp_handle = NULL;
	}

	return 0;
}

int lca_te_kpp_alloc(struct te_drvdata *drvdata)
{
	struct te_kpp_handle *kpp_handle;
	struct te_kpp_alg *t_alg;
	struct device *dev = drvdata_to_dev(drvdata);
	int rc = -ENOMEM;
	int alg;

	kpp_handle = kmalloc(sizeof(*kpp_handle), GFP_KERNEL);
	if (!kpp_handle) {
		rc = -ENOMEM;
		goto fail0;
	}

	drvdata->kpp_handle = kpp_handle;


	INIT_LIST_HEAD(&kpp_handle->kpp_list);

	/* Linux crypto */
	for (alg = 0; alg < ARRAY_SIZE(kpp_algs); alg++) {
		t_alg = te_kpp_create_alg(&kpp_algs[alg]);
		if (IS_ERR(t_alg)) {
			rc = PTR_ERR(t_alg);
			dev_err(dev, "%s alg allocation failed\n",
					kpp_algs[alg].driver_name);
			goto fail1;
		}
		t_alg->drvdata = drvdata;
		rc = crypto_register_kpp(&t_alg->kpp_alg);
		if (unlikely(rc != 0)) {
			dev_err(dev, "%s alg registration failed\n",
					t_alg->kpp_alg.base.cra_driver_name);
			goto fail2;
		} else {
			list_add_tail(&t_alg->entry, &kpp_handle->kpp_list);
			dev_err(dev, "Registered %s\n", t_alg->kpp_alg.base.cra_driver_name);
		}
	}

	return 0;

fail2:
	kfree(t_alg);
fail1:
	lca_te_kpp_free(drvdata);
fail0:
	return rc;
}



