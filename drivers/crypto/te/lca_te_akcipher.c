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
#include <crypto/internal/akcipher.h>
#include <crypto/akcipher.h>
#include <crypto/scatterwalk.h>

#include "te_bn.h"
#include "te_rsa.h"
#include "lca_te_akcipher.h"
#include "lca_te_ra.h"

#define TE_LCA_CHECK_RET_GO                                                    \
    do {                                                                       \
        if ((TE_SUCCESS) != (rc)) {                                            \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

struct te_akcipher_handle {
	lca_te_ra_handle ra;
	struct list_head akcipher_list;
};

struct lca_te_rsa_key_t {
	te_bn_t *N;
	te_bn_t *E;
	te_bn_t *D;
	te_bn_t *P;
	te_bn_t *Q;
	te_bn_t *DP;
	te_bn_t *DQ;
	te_bn_t *QP;
};

struct te_akcipher_ctx {
	struct te_drvdata *drvdata;
	unsigned int reqsize;
	union {
		struct lca_te_rsa_key_t rsa;
	}u;
};

struct te_akcipher_alg {
	struct list_head entry;
	unsigned int reqsize;
	struct te_drvdata *drvdata;
	struct akcipher_alg akcipher_alg;
};

struct te_akcipher_template {
	char name[CRYPTO_MAX_ALG_NAME];
	char driver_name[CRYPTO_MAX_ALG_NAME];
	struct akcipher_alg akcipher;
	unsigned int reqsize;
	struct te_drvdata *drvdata;
};

struct te_rsa_req_ctx {
	struct te_request base;          /**< Must put it in the beginning */
	int buflen;
	u8 * buf;
	bool enc;
	te_rsa_request_t rsa;
};

static int te_rsa_init_key_bufs(struct te_akcipher_ctx *ctx)
{
	int rc=0;
	rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.rsa.N);
	TE_LCA_CHECK_RET_GO;
	rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.rsa.E);
	TE_LCA_CHECK_RET_GO;
	rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.rsa.P);
	TE_LCA_CHECK_RET_GO;
	rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.rsa.Q);
	TE_LCA_CHECK_RET_GO;
	rc = te_bn_alloc(ctx->drvdata->h, 0, &ctx->u.rsa.D);
	TE_LCA_CHECK_RET_GO;

	return 0;

finish:
	te_bn_free(ctx->u.rsa.N);
	te_bn_free(ctx->u.rsa.E);
	te_bn_free(ctx->u.rsa.P);
	te_bn_free(ctx->u.rsa.Q);
	te_bn_free(ctx->u.rsa.D);
	return TE2ERRNO(rc);
}

static void te_rsa_free_key_bufs(struct te_akcipher_ctx *ctx)
{
	/* Clean up old key data */
	te_bn_free(ctx->u.rsa.N);
	te_bn_free(ctx->u.rsa.E);
	te_bn_free(ctx->u.rsa.P);
	te_bn_free(ctx->u.rsa.Q);
	te_bn_free(ctx->u.rsa.D);
}

static int te_rsa_setkey(struct crypto_akcipher *tfm, const void *key,
			 unsigned int keylen, bool private)
{
	struct te_akcipher_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct rsa_key raw_key;
	int rc;

	pm_runtime_get_sync(dev);

	te_rsa_free_key_bufs(ctx);
	memset(&raw_key, 0, sizeof(raw_key));
	te_rsa_init_key_bufs(ctx);

	/* Code borrowed from crypto/rsa.c */
	if (private) {
		rc = rsa_parse_priv_key(&raw_key, key, keylen);
	} else {
		rc = rsa_parse_pub_key(&raw_key, key, keylen);
	}
	if (rc != 0) {
		goto out;
	}

	rc = te_bn_import(ctx->u.rsa.N, raw_key.n, raw_key.n_sz, 1);
	TE_LCA_CHECK_RET_GO;
	rc = te_bn_import(ctx->u.rsa.E, raw_key.e, raw_key.e_sz, 1);
	TE_LCA_CHECK_RET_GO;
	if(raw_key.d_sz) {
		rc = te_bn_import(ctx->u.rsa.D, raw_key.d, raw_key.d_sz, 1);
		TE_LCA_CHECK_RET_GO;
		rc = te_bn_import_s32(ctx->u.rsa.P, 0);
		TE_LCA_CHECK_RET_GO;
		rc = te_bn_import_s32(ctx->u.rsa.Q, 0);
		TE_LCA_CHECK_RET_GO;
		rc = te_rsa_complete_key(ctx->u.rsa.N, ctx->u.rsa.E, ctx->u.rsa.D,
						ctx->u.rsa.P,ctx->u.rsa.Q, NULL, NULL, NULL);
		TE_LCA_CHECK_RET_GO;
	}

	pm_runtime_put_autosuspend(dev);
	return 0;

finish:
	rc = TE2ERRNO(rc);
out:
	te_rsa_free_key_bufs(ctx);
	pm_runtime_put_autosuspend(dev);
	return rc;
}

static int te_rsa_setprivkey(struct crypto_akcipher *tfm, const void *key,
			      unsigned int keylen)
{
	return te_rsa_setkey(tfm, key, keylen, true);
}

static int te_rsa_setpubkey(struct crypto_akcipher *tfm, const void *key,
			     unsigned int keylen)
{
	return te_rsa_setkey(tfm, key, keylen, false);
}

static unsigned int te_rsa_maxsize(struct crypto_akcipher *tfm)
{
	struct te_akcipher_ctx *ctx = akcipher_tfm_ctx(tfm);
	return TE_BN_BYTELEN(ctx->u.rsa.N);
}

static void te_akcipher_complete(struct te_async_request *te_req, int err)
{
	struct akcipher_request *req = (struct akcipher_request *)te_req->data;
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct te_akcipher_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct te_rsa_req_ctx *areq_ctx = akcipher_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_akcipher_handle *ahdl = ctx->drvdata->akcipher_handle;

	/* Set req->dst_len to key_len */
	if (areq_ctx->enc) {
		req->dst_len = areq_ctx->rsa.public_args.size;
	} else {
		req->dst_len = areq_ctx->rsa.private_args.size;
	}

	sg_copy_from_buffer(req->dst, sg_nents(req->dst),
			    (const void *)(areq_ctx->buf), req->dst_len);

	if (areq_ctx->buf) {
		kfree_sensitive(areq_ctx->buf);
		areq_ctx->buf = NULL;
		areq_ctx->buflen = 0;
	}

	lca_te_ra_complete(ahdl->ra, &areq_ctx->base);
	akcipher_request_complete(req, TE2ERRNO(err));
	pm_runtime_put_autosuspend(dev);
}

static int te_rsa_do_encrypt(struct te_request *treq)
{
	int rc = 0;
	int i = 0;
	unsigned int offset = 0;
	unsigned int key_len = 0;
	struct akcipher_request *req =
		container_of((void*(*)[])treq, struct akcipher_request, __ctx);
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct te_akcipher_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct te_rsa_req_ctx *areq_ctx = akcipher_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx->drvdata);

	/* Don't zeroize areq_ctx->base */
	areq_ctx->buf = NULL;
	areq_ctx->buflen = 0;
	areq_ctx->enc = 0;
	memset(&areq_ctx->rsa, 0, sizeof(areq_ctx->rsa));

	/*
	 * The src and dst are scatterlist, so shall create a buffer to
	 * hold the input/output data for driver.
	 * There are several cases for src_len && dst_len:
	 * src_len > key_len:
	 *		Set buflen = src_len
	 *		Read LSBs of key_len from req->src
	 *		Check MSBs of (src_len - key_len) are 0.
	 *
	 * src_len == key_len:
	 *		Set buflen = src_len
	 *		Read Whole req->src
	 *
	 * src_len < key_len:
	 *		Set buflen = key_len
	 *		Read Whole req->src
	 *		Set MSBs of key_len - src_len to 0.
	 *
	 * dst_len > key_len:
	 *		Write dst with key_len
	 *		Update dst_len to key_len
	 *
	 * dst_len == key_len:
	 *		Write dst with key_len
	 *		Update dst_len to key_len
	 *
	 * dst_len < key_len:
	 *		Update dst_len to key_len
	 *		return -EOVERFLOW
	 *
	 */

	/* Init buflen to MAX(key_len, src_len) */
	key_len = TE_BN_BYTELEN(ctx->u.rsa.N);
	if (req->src_len > key_len) {
		areq_ctx->buflen = req->src_len;
		offset = req->src_len - key_len;
	} else {
		areq_ctx->buflen = key_len;
		offset = 0;
	}

	/* Check dst_len */
	if (req->dst_len < key_len) {
		req->dst_len = key_len;
		rc = -ETOOSMALL;
		goto err;
	}

	areq_ctx->buf = kzalloc(areq_ctx->buflen, GFP_KERNEL);
	if (!areq_ctx->buf) {
		rc = -ENOMEM;
		areq_ctx->buflen = 0;
		goto err;
	}

	/* we treat the input buffer as the bigendian data, so copy
	 * the data to the tail of the buffer.
	 * This covers areq_ctx->buflen == req->src_len and
	 * areq_ctx->buflen > req->src_len
	 */
	sg_copy_to_buffer(req->src, sg_nents(req->src),
			  areq_ctx->buf + areq_ctx->buflen - req->src_len,
			  req->src_len);
	if (offset != 0) {
		/* Check MSBs of offset are 0 */
		while(i < offset) {
			if (areq_ctx->buf[i] != 0) {
				rc = -EINVAL;
				kfree_sensitive(areq_ctx->buf);
				areq_ctx->buf = NULL;
				areq_ctx->buflen = 0;
				goto err;
			}
			i++;
		}
		/* Move data forward by offset */
		memmove(areq_ctx->buf, areq_ctx->buf + offset, key_len);
	}

	areq_ctx->rsa.public_args.N = ctx->u.rsa.N;
	areq_ctx->rsa.public_args.E= ctx->u.rsa.E;
	areq_ctx->rsa.public_args.input = areq_ctx->buf;
	areq_ctx->rsa.public_args.output = areq_ctx->buf;
	areq_ctx->rsa.public_args.size = key_len;
	areq_ctx->rsa.base.flags = req->base.flags;
	areq_ctx->rsa.base.data = req;
	areq_ctx->rsa.base.completion = te_akcipher_complete;
	areq_ctx->enc = true;

	rc = te_rsa_public_async(&areq_ctx->rsa);
	if(rc != TE_SUCCESS) {
		dev_err(dev, "te_rsa_public_async ret:%x\n", rc);
		kfree_sensitive(areq_ctx->buf);
		areq_ctx->buf = NULL;
		areq_ctx->buflen = 0;
		goto err_drv;
	}
	return -EINPROGRESS;

err_drv:
	rc = TE2ERRNO(rc);
err:
	return rc;
}

static int te_rsa_request(struct akcipher_request *req,
			  lca_te_submit_t fn)
{
	int rc = 0;
	struct te_request *treq = akcipher_request_ctx(req);
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct te_akcipher_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct device *dev = drvdata_to_dev(ctx->drvdata);
	struct te_akcipher_handle *ahdl = ctx->drvdata->akcipher_handle;

	pm_runtime_get_sync(dev);
	memset(treq, 0, sizeof(*treq));
	treq->fn   = fn;
	treq->areq = &req->base;
	/* No need to protect the akcipher request */
	rc = lca_te_ra_submit(ahdl->ra, treq, false);
	if (rc != -EINPROGRESS) {
		pm_runtime_put_autosuspend(dev);
	}
	return rc;
}

static int te_rsa_encrypt(struct akcipher_request *req)
{
	return te_rsa_request(req, te_rsa_do_encrypt);
}

static int te_rsa_do_decrypt(struct te_request *treq)
{
	int rc = 0;
	int i = 0;
	unsigned int offset = 0;
	unsigned int key_len = 0;
	struct akcipher_request *req =
		container_of((void*(*)[])treq, struct akcipher_request, __ctx);
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct te_akcipher_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct te_rsa_req_ctx *areq_ctx = akcipher_request_ctx(req);
	struct device *dev = drvdata_to_dev(ctx->drvdata);

	/* Don't zeroize areq_ctx->base */
	areq_ctx->buf = NULL;
	areq_ctx->buflen = 0;
	areq_ctx->enc = 0;
	memset(&areq_ctx->rsa, 0, sizeof(areq_ctx->rsa));

	/* See comments in te_rsa_encrypt() */
	/* Init buflen to MAX(key_len, src_len) */
	key_len = TE_BN_BYTELEN(ctx->u.rsa.N);
	if (req->src_len > key_len) {
		areq_ctx->buflen = req->src_len;
		offset = req->src_len - key_len;
	} else {
		areq_ctx->buflen = key_len;
		offset = 0;
	}

	/* Check dst_len */
	if (req->dst_len < key_len) {
		req->dst_len = key_len;
		rc = -ETOOSMALL;
		goto err;
	}

	areq_ctx->buf = kzalloc(areq_ctx->buflen, GFP_KERNEL);
	if (!areq_ctx->buf) {
		rc = -ENOMEM;
		areq_ctx->buflen = 0;
		goto err;
	}

	/* we treat the input buffer as the bigendian data, so copy
	 * the data to the tail of the buffer.
	 * This covers areq_ctx->buflen == req->src_len and
	 * areq_ctx->buflen > req->src_len
	 */
	sg_copy_to_buffer(req->src, sg_nents(req->src),
			  areq_ctx->buf + areq_ctx->buflen - req->src_len,
			  req->src_len);
	if (offset != 0) {
		/* Check MSBs of offset are 0 */
		while(i < offset) {
			if (areq_ctx->buf[i] != 0) {
				rc = -EINVAL;
				kfree_sensitive(areq_ctx->buf);
				areq_ctx->buf = NULL;
				areq_ctx->buflen = 0;
				goto err;
			}
			i++;
		}
		/* Move data forward by offset */
		memmove(areq_ctx->buf, areq_ctx->buf + offset, key_len);
	}

	areq_ctx->rsa.private_args.N = ctx->u.rsa.N;
	areq_ctx->rsa.private_args.E= ctx->u.rsa.E;
	areq_ctx->rsa.private_args.D= ctx->u.rsa.D;
	areq_ctx->rsa.private_args.P= ctx->u.rsa.P;
	areq_ctx->rsa.private_args.Q= ctx->u.rsa.Q;
	areq_ctx->rsa.private_args.input = areq_ctx->buf;
	areq_ctx->rsa.private_args.output = areq_ctx->buf;
	areq_ctx->rsa.private_args.size = key_len;
	areq_ctx->rsa.base.flags = req->base.flags;
	areq_ctx->rsa.base.data = req;
	areq_ctx->rsa.base.completion = te_akcipher_complete;
	areq_ctx->enc = false;

	rc = te_rsa_private_async(&areq_ctx->rsa);
	if (rc != TE_SUCCESS) {
		dev_err(dev, "te_rsa_private_async ret:%x\n", rc);
		kfree_sensitive(areq_ctx->buf);
		areq_ctx->buf = NULL;
		areq_ctx->buflen = 0;
		goto err_drv;
	}
	return -EINPROGRESS;

err_drv:
	rc = TE2ERRNO(rc);
err:
	return rc;
}

static int te_rsa_decrypt(struct akcipher_request *req)
{
	return te_rsa_request(req, te_rsa_do_decrypt);
}

static int te_rsa_init(struct crypto_akcipher *tfm)
{
	int rc = -1;
	struct te_akcipher_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct akcipher_alg *alg = crypto_akcipher_alg(tfm);
	struct te_akcipher_alg *te_alg =
			container_of(alg, struct te_akcipher_alg, akcipher_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	ctx->drvdata = te_alg->drvdata;

	akcipher_set_reqsize(tfm, sizeof(struct te_rsa_req_ctx));

	pm_runtime_get_sync(dev);
	rc = te_rsa_init_key_bufs(ctx);
	pm_runtime_put_autosuspend(dev);

	return rc;
}

static void te_rsa_exit(struct crypto_akcipher *tfm)
{
	struct te_akcipher_ctx *ctx = crypto_tfm_ctx(&tfm->base);
	struct akcipher_alg *alg = crypto_akcipher_alg(tfm);
	struct te_akcipher_alg *te_alg =
			container_of(alg, struct te_akcipher_alg, akcipher_alg);
	struct device *dev = drvdata_to_dev(te_alg->drvdata);

	pm_runtime_get_sync(dev);
	te_rsa_free_key_bufs(ctx);
	pm_runtime_put_autosuspend(dev);
}

static struct te_akcipher_template akcipher_algs[] = {
	{
		.name = "rsa",
		.driver_name = "rsa-te",
		.akcipher = {
			.encrypt = te_rsa_encrypt,
			.decrypt = te_rsa_decrypt,
			.sign = te_rsa_decrypt,
			.verify = te_rsa_encrypt,
			.set_pub_key = te_rsa_setpubkey,
			.set_priv_key = te_rsa_setprivkey,
			.max_size = te_rsa_maxsize,
			.init = te_rsa_init,
			.exit = te_rsa_exit,
		},
		.reqsize = sizeof(struct te_rsa_req_ctx),
	},
};

static struct te_akcipher_alg *te_akcipher_create_alg(struct te_akcipher_template *tmpl)
{
	struct te_akcipher_alg *t_alg;
	struct akcipher_alg *alg;

	t_alg = kzalloc(sizeof(*t_alg), GFP_KERNEL);
	if (!t_alg) {
		return ERR_PTR(-ENOMEM);
	}
	alg = &tmpl->akcipher;

	snprintf(alg->base.cra_name, CRYPTO_MAX_ALG_NAME, "%s", tmpl->name);
	snprintf(alg->base.cra_driver_name, CRYPTO_MAX_ALG_NAME, "%s",
		 tmpl->driver_name);
	alg->base.cra_module = THIS_MODULE;
	alg->base.cra_priority = TE_CRA_PRIO;

	alg->base.cra_ctxsize = sizeof(struct te_akcipher_ctx);
	alg->base.cra_flags = CRYPTO_ALG_ASYNC;
	/* Set the alg->reqsize in case use it w.t.o/before the .init() call */
	alg->reqsize = tmpl->reqsize;

	t_alg->akcipher_alg = *alg;
	t_alg->reqsize = tmpl->reqsize;

	return t_alg;
}

int lca_te_akcipher_free(struct te_drvdata *drvdata)
{
	struct te_akcipher_alg *t_alg, *n;
	struct te_akcipher_handle *akcipher_handle =
		(struct te_akcipher_handle *)drvdata->akcipher_handle;

	if (akcipher_handle) {
		/* free request agent */
		lca_te_ra_free(akcipher_handle->ra);
		akcipher_handle->ra = NULL;

		/* Remove registered algs */
		list_for_each_entry_safe(t_alg, n, &akcipher_handle->akcipher_list, entry) {
			crypto_unregister_akcipher(&t_alg->akcipher_alg);
			list_del(&t_alg->entry);
			kfree(t_alg);
		}
		kfree(akcipher_handle);
		drvdata->akcipher_handle = NULL;
	}

	return 0;
}

int lca_te_akcipher_alloc(struct te_drvdata *drvdata)
{
	struct te_akcipher_handle *akcipher_handle;
	struct te_akcipher_alg *t_alg;
	struct device *dev = drvdata_to_dev(drvdata);
	int rc = -ENOMEM;
	int alg;

	akcipher_handle = kmalloc(sizeof(*akcipher_handle), GFP_KERNEL);
	if (!akcipher_handle) {
		rc = -ENOMEM;
		goto fail0;
	}

	drvdata->akcipher_handle = akcipher_handle;
	INIT_LIST_HEAD(&akcipher_handle->akcipher_list);
	/* create request agent */
	rc = lca_te_ra_alloc(&akcipher_handle->ra, "akcipher");
	if (rc != 0) {
		dev_err(dev,"akcipher alloc ra failed %d\n", rc);
		goto fail1;
	}

	/* Linux crypto */
	for (alg = 0; alg < ARRAY_SIZE(akcipher_algs); alg++) {
		t_alg = te_akcipher_create_alg(&akcipher_algs[alg]);
		if (IS_ERR(t_alg)) {
			rc = PTR_ERR(t_alg);
			dev_err(dev, "%s alg allocation failed\n",
					akcipher_algs[alg].driver_name);
			goto fail1;
		}
		t_alg->drvdata = drvdata;
		rc = crypto_register_akcipher(&t_alg->akcipher_alg);
		if (unlikely(rc != 0)) {
			dev_err(dev, "%s alg registration failed\n",
					t_alg->akcipher_alg.base.cra_driver_name);
			goto fail2;
		} else {
			list_add_tail(&t_alg->entry, &akcipher_handle->akcipher_list);
			dev_dbg(dev, "Registered %s\n",
				     t_alg->akcipher_alg.base.cra_driver_name);
		}
	}

	return 0;

fail2:
	kfree(t_alg);
fail1:
	lca_te_akcipher_free(drvdata);
fail0:
	return rc;
}
