//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 ARM Technology (China) Co., Ltd.
 */

/*#define DEBUG*/
#define pr_fmt(fmt) "te_crypt: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include "driver/te_drv.h"
#include "te_cipher.h"
#include "te_xts.h"
#include "te_hmac.h"
#include "te_cmac.h"
#include "te_hash.h"
#include "te_gcm.h"
#include "te_ccm.h"
#include "lca_te_ctx.h"

/**
 * Driver Context Manager
 *
 * #Introduction
 *
 * The context manager is designed to manage the driver contexts for various
 * TE drivers with the aim to promote parallel operation performance, including
 * cipher, xts, cmac, cbcmac, hash, hmac, gcm, and ccm.
 *
 * The driver contexts (ctx) are managed on transformation objects (tfm) basis.
 * A context governor object (gov) shall be create-and-linked with each tfm.
 * The gov manages a list of corresponding driver contexts. Each ctx represents
 * for a driver session. A ctx is attached to a specific request object (req) at
 * runtime to grant the crypto engine function. The LCA_TE_CTX_FLAG_USED flag of
 * the ctx is set on attaching and cleared on detaching. An attached ctx cannot
 * be used by the other reqs until the former req completes. The driver context
 * manager is of the following characteristics by design:
 * - One gov object is linked to each tfm object exclusively.
 * - Multiple ctx object are linked to each gov object.
 * - Have a x:y mapping between req objects and the ctx objects.
 *
 * +----------------------------+          +----------------------------+
 * |        tfm#0 object        |--+  ...  |        tfm#i object        |--+
 * +----------------------------+  |       +----------------------------+  |
 *     |        |            |     |           |        |            |     |
 * +------+ +------+     +------+  |       +------+ +------+     +------+  |
 * |req0.0| |req0.1| ... |req0.m|  |       |reqi.0| |reqi.1| ... |reqi.p|  |
 * +------+ +------+     +------+  |       +------+ +------+     +------+  |
 *  \                          /   |        \                          /   |
 *   xxxxxxxxxxxxxxxxxxxxxxxxxx    |  ...    xxxxxxxxxxxxxxxxxxxxxxxxxx    |
 *  /                          \   |        /                          \   |
 * +------+ +------+     +------+  |       +------+ +------+     +------+  |
 * |ctx0.0| |ctx0.1| ... |ctx0.n|  |       |ctxi.0| |ctxi.1| ... |ctxi.q|  |
 * +------+ +------+     +------+  |       +------+ +------+     +------+  |
 *     |        |            |     |           |        |            |     |
 * +----------------------------+  |       +----------------------------+  |
 * |        gov#0 object        |<-+  ...  |        gov#i object        |<-+
 * +----------------------------+          +----------------------------+
 *
 * The above figure outlines the relationships among objects of different types:
 * - tfm objects
 * - req objects
 * - gov objects
 * - ctx objects
 *
 * #Interfaces
 *
 * The ctx manager requires the following interfaces from the driver:
 * - int init(te_{name}_ctx_t *ctx, te_drv_handle hdl, te_algo_t alg);
 *   It initializes the specified driver ctx. This is mandatory.
 *   The {name} represents the specific driver name here and after. See
 *   section `Introduction` for supported driver name.
 * - int free(te_{name}_ctx_t *ctx);
 *   It de-initializes the specified driver ctx. This is mandatory.
 * - int clone(te_{name}_ctx_t *src, te_{name}_ctx_t *dst);
 *   It clones all the driver internal states of the src ctx to the dst ctx.
 *   This is mandatory.
 *   This function shall not assume the current state of the src ctx.
 * - int setkey(te_{name}_ctx_t *ctx, const u8 *key, u32 keybits);
 *   It configures the key for the specified ctx. This is mandatory to
 *   key-ed crypto only.
 * - int reset(te_{name}_ctx_t *ctx);
 *   It resets the specified ctx to a state that is ready for a fresh
 *   init-update-final, encrypt or decrypt request.
 *   The key data, if applicable, shall not be purged.
 *   This function shall not assume the current state of the ctx.
 *
 * The ctx manager requires the following interfaces from the driver:
 * - struct lca_te_drv_ctx_gov *lca_te_ctx_alloc_gov(
 *                                             const lca_te_base_ops_t *ops,
 *                                             te_drv_handle hdl,
 *                                             te_algo_t alg,
 *                                             struct crypto_tfm *tfm);
 * - void lca_te_ctx_free_gov(struct lca_te_drv_ctx_gov *gov);
 * - int lca_te_ctx_get(struct lca_te_drv_ctx_gov *gov, te_base_ctx_t **pbctx,
 *                      void *req, bool reset);
 * - int lca_te_ctx_put(struct lca_te_drv_ctx_gov *gov, te_base_ctx_t *bctx);
 * - int lca_te_ctx_setkey(struct lca_te_drv_ctx_gov *gov,
 *                         const u8 *key, u32 klen);
 *
 * #Functional Design
 *
 * The behaviors of the ctx manager are described as below:
 * - The ctx manager will create a gov object and pre-create a list of driver
 *   ctx for that gov on lca_te_ctx_alloc_gov(). The number of pre-created
 *   driver ctx is defined by LCA_TE_CTX_PRE, while the maximum number is
 *   defined by LCA_TE_CTX_MAX.
 * - Pre-allocating ctx is to save time in serving req during runtime. The more
 *   pre-allocating ctx the better parallel performance in general, and the more
 *   memory consumption as well.
 * - The LCA_TE_CTX_PRE is equal to the number of online CPUs by default.
 * - The LCA_TE_CTX_MAX is set to two times of total CPU number by default.
 * - All ctx objects of a gov will be synchronized on lca_te_ctx_setkey() in
 *   case of key-ed crypto algorithms, like cipher, aead, hmac, etc. The sync
 *   operation ensures all the ctx objects are of the same key.
 * - The LCA driver uses lca_te_ctx_get() to attach one driver ctx. The ctx
 *   manager will allocate a new ctx, add it into the ctx pool for the gov, and
 *   give it to the caller when the below conditions are all satisfied:
 *   . only one pre-allocated ctx is left.
 *   . the number of ctx is less than LCA_TE_CTX_MAX.
 *   . the continuous clone error is less than LCA_TE_CTX_RETRIES.
 *   Otherwise, the calling thread will sleep to wait for the other req to
 *   release a driver ctx. The max wait time is defined by LCA_TE_CTX_TIMEOUT.
 *   Once a ctx is obtained, it can be selectively reset to the INIT state so
 *   that gets ready for another init-update-final request or the equivalent.
 * - All the ctx objects of a gov will be freed on lca_te_ctx_free_gov(), and
 *   the gov object as well.
 *
 * The below figure outlines the operation steps for the following cases:
 * 1. setkey
 * 2. request
 *                +-----+
 *                |ctx#0|     wait?
 *                +-----+      ---
 *          1.1:sync ^          |
 *                   |      2.0:get  +-----+ 2.1:submit   +-----+
 *     1.0:setkey +-----+ ,--------->|     |------------->|     |
 *     ---------->|ctx#i|+           |req#j|              | TE  |
 *                +-----+ `<---------|     |<-------------| DRV |
 *                   |      2.4:put  +-----+ 2.2:complete +-----+
 *          1.1:sync v                       2.3:error
 *                +-----+
 *                |ctx#n|
 *                +-----+
 *
 */

/**
 * LCA_TE_CTX_MAX - Max number of TE driver ctx for a transformation object.
 * Note this parameter defines the maximum allowable number of driver ctx
 * during runtime. However, the number of generated driver ctx depends
 * on the number of outstanding requests and the availability of memory.
 * Hence, the LCA_TE_CTX_MAX value shall be carefully selected on a system.
 *
 * WARNING0: If the LCA_TE_CTX_MAX is too small, the parallel operation
 * performance would be affected. Moreover, the LCA user might suffer from
 * a -ETIMEDOUT error at the worst case.
 *
 * WARNING1: If the LCA_TE_CTX_MAX is too big, the parallel operation
 * perormance would be affected. That is because of the clone and buffering
 * overhead for quantities of async requests (i.e., iozone).
 *
 * Set it to two times of NR_CPUS if unsure.
 * \{
 */
#define LCA_TE_CTX_INFINITE  0xFFFFFFFFU
#define LCA_TE_CTX_MAX       (NR_CPUS * 2)
/** \} */

/**
 * Number of pre-allocated TE driver ctx for a transformation object.
 * Pre-allocating TE driver ctx is to promote the multi-threading performance
 * for one transformation object at a cost of increasing memory footprint.
 * It is recommended to set it to the number of online CPUs when multi-threading
 * is required. Otherwise set it to one to save memory.
 * The LCA_TE_CTX_MAX shall be respected here.
 * Set it to one if unsure.
 */
#define LCA_TE_CTX_PRE       min(num_online_cpus(), (unsigned)LCA_TE_CTX_MAX)

/**
 * Wait TE driver ctx timeout in seconds.
 */
#define LCA_TE_CTX_TIMEOUT   60

/**
 * Retry times. Used by the clone only.
 */
#define LCA_TE_CTX_RETRIES   3

/**
 * TE driver ctx flags.
 */
#define LCA_TE_CTX_FLAG_USED (1U << 0)

typedef struct te_lca_drv_ctx {
	union {
		te_cipher_ctx_t cctx;
		te_xts_ctx_t xctx;
		te_dgst_ctx_t dctx;
		te_hmac_ctx_t hmac_ctx;
		te_cmac_ctx_t cmac_ctx;
		te_cbcmac_ctx_t cbcmac_ctx;
		te_gcm_ctx_t gcm_ctx;
		te_ccm_ctx_t ccm_ctx;
		te_base_ctx_t base;
	};

	unsigned flags;
	pid_t pid;
	void *req;
	struct list_head node;
} lca_te_drv_ctx_t;

typedef struct lca_te_drv_ctx_gov {
	struct list_head ctx_list;
	uint32_t num_ctx;           /**< Number of existing ctx */
	uint32_t nused;             /**< Number of used ctx */
	uint32_t clone_err;         /**< Number of clone errors */
	struct mutex mut;
	struct completion comp;

	lca_te_base_ops_t ops;
	te_drv_handle hdl;
	te_algo_t alg;
	struct crypto_tfm *tfm;
} lca_te_drv_ctx_gov_t;

static void te_ctx_free(lca_te_drv_ctx_gov_t *gov, lca_te_drv_ctx_t *ctx)
{
	if (gov->ops.free != NULL) {
		gov->ops.free(&ctx->base);
	}

	pr_debug("pid %d tfm %p algo (0x%x) free ctx %p\n",
		 current->pid, gov->tfm, gov->alg, ctx);
	memset(ctx, 0, sizeof(*ctx));
	kfree(ctx);
}

static void te_ctx_free_all(lca_te_drv_ctx_gov_t *gov)
{
	lca_te_drv_ctx_t *ctx = NULL;
	struct list_head *pos, *tmp;

	/* Free all contexts */
	list_for_each_safe(pos, tmp, &gov->ctx_list) {
		ctx = list_entry(pos, lca_te_drv_ctx_t, node);
		if ((ctx->flags & LCA_TE_CTX_FLAG_USED) != 0) {
			printk("driver ctx %p is still used by thread %d "
				   "req %p, algo 0x%x tfm %p\n",
				   ctx, ctx->pid, ctx->req, gov->alg, gov->tfm);
		}

		list_del(pos);
		te_ctx_free(gov, ctx);
	}
}

static lca_te_drv_ctx_t *te_ctx_find(struct lca_te_drv_ctx_gov *gov)
{
	int err = -ENOENT;
	lca_te_drv_ctx_t *ctx = NULL;
	struct list_head *pos;

	list_for_each(pos, &gov->ctx_list) {
		ctx = list_entry(pos, lca_te_drv_ctx_t, node);
		if (!(ctx->flags & LCA_TE_CTX_FLAG_USED)) {
			goto out;
		}
	}

	ctx = ERR_PTR(err);
out:
	return ctx;
}

static lca_te_drv_ctx_t *te_ctx_alloc(struct lca_te_drv_ctx_gov *gov)
{
	int rc = 0, err = -ENOMEM;
	lca_te_drv_ctx_t *ctx = NULL;

	ctx = (lca_te_drv_ctx_t *)kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (NULL == ctx) {
		goto out_err;
	}

	/* Initialize a TE driver ctx */
	BUG_ON(NULL == gov->ops.init);
	rc = gov->ops.init(&ctx->base, gov->hdl, gov->alg);
	if (rc != TE_SUCCESS) {
		pr_err("%s init algo (0x%x) ret:0x%x\n",
			crypto_tfm_alg_driver_name(gov->tfm),
			gov->alg, rc);
		goto err_drv;
	}

	pr_debug("pid %d tfm %p algo (0x%x) alloc ctx %p\n",
		 current->pid, gov->tfm, gov->alg, ctx);
	goto out;

err_drv:
	err = TE2ERRNO(rc);
	kfree(ctx);
out_err:
	ctx = ERR_PTR(err);
out:
	return ctx;
}

static lca_te_drv_ctx_t *te_ctx_clone(struct lca_te_drv_ctx_gov *gov,
				      lca_te_drv_ctx_t *src)
{
	int rc = 0, err = -ENOTSUPP;
	lca_te_drv_ctx_t *ctx = NULL;

	if (NULL == gov->ops.clone) {
		goto out_err;
	}

	/**
	 * Create one ctx in case the driver clone doesn't initialize the
	 * dst ctx when the src ctx is in the INIT state.
	 */
	ctx = te_ctx_alloc(gov);
	if (IS_ERR(ctx)) {
		err = PTR_ERR(ctx);
		goto out_err;
	}

	/* Clone a driver ctx */
	rc = gov->ops.clone(&src->base, &ctx->base);
	if (rc != TE_SUCCESS) {
		pr_err("%s clone algo (0x%x) ret:0x%x\n",
			crypto_tfm_alg_driver_name(gov->tfm),
			gov->alg, rc);
		goto err_drv;
	}

	pr_debug("pid %d tfm %p algo (0x%x) clone ctx %p\n",
		 current->pid, gov->tfm, gov->alg, ctx);
	goto out;

err_drv:
	err = TE2ERRNO(rc);
	kfree(ctx);
out_err:
	ctx = ERR_PTR(err);
out:
	return ctx;
}

static int te_ctx_prepare(lca_te_drv_ctx_gov_t *gov)
{
	int err = 0;
	unsigned i;
	lca_te_drv_ctx_t *ctx;

	/* Pre-allocate driver context if applicable */
	pr_debug("pre-alloc %u ctx algo (0x%x)\n", LCA_TE_CTX_PRE, gov->alg);
	for (i = 0; i < LCA_TE_CTX_PRE; i++) {
		ctx = te_ctx_alloc(gov);
		if (IS_ERR(ctx)) {
			err = PTR_ERR(ctx);
			te_ctx_free_all(gov);
			goto out;
		}

		list_add_tail(&ctx->node, &gov->ctx_list);
		gov->num_ctx++;
	}

out:
	return err;
}

static int te_ctx_get(struct lca_te_drv_ctx_gov *gov, lca_te_drv_ctx_t **pctx,
		      void *req)
{
	int err = 0;
	long to = 0;
	lca_te_drv_ctx_t *ctx = NULL;

	if (0 == gov->num_ctx) {
		/* Create one ctx */
		ctx = te_ctx_alloc(gov);
		if (IS_ERR(ctx)) {
			err = PTR_ERR(ctx);
			goto out;
		}
		goto new_ctx;
	} else {
		/* 1. Try to find one free ctx */
		ctx = te_ctx_find(gov);
		if (!IS_ERR(ctx)) {
			/*
			 * 2. Clone one ctx to avoid future wait if needed.
			 * The clone operation happens if all satisfied:
			 * 1) Only one ctx is available.
			 * 2) num_ctx is less than LCA_TE_CTX_MAX.
			 * 3) clone_err is less than LCA_TE_CTX_RETRIES.
			 */
			if ((1 == (int)(gov->num_ctx - gov->nused)) &&
			    (gov->num_ctx < LCA_TE_CTX_MAX) &&
			    (gov->clone_err < LCA_TE_CTX_RETRIES)) {
				lca_te_drv_ctx_t *newctx;
				newctx = te_ctx_clone(gov, ctx);
				if (!IS_ERR(newctx)) {
					gov->clone_err = 0;
					ctx = newctx;
					/* Wake up one */
					complete(&gov->comp);
					goto new_ctx;
				}
				gov->clone_err++;
			}

			goto got_ctx;
		}

		/* 3. Wait others to put ctx */
		do {
			/* Release lock while wait */
			pr_debug("pid %d tfm %p req %p algo (0x%x) wait ctx\n",
				 current->pid, gov->tfm, req, gov->alg);
			mutex_unlock(&gov->mut);
			to = wait_for_completion_killable_timeout(&gov->comp,
						LCA_TE_CTX_TIMEOUT * HZ);

			mutex_lock(&gov->mut);

			pr_debug("pid %d tfm %p req %p wait ret %ld\n",
				 current->pid, gov->tfm, req, to);
			if (to < 0) {
				err = (int)to;
				goto out;
			} else if (0 == to) {
				err = -ETIMEDOUT;
				goto out;
			}

			ctx = te_ctx_find(gov);
			if (!IS_ERR(ctx)) {
				/* Get one ctx */
				goto got_ctx;
			}
		} while (-ENOENT == PTR_ERR(ctx));

		/* Failed when reaching here */
		err = PTR_ERR(ctx);
		goto out;
	}

new_ctx:
	list_add_tail(&ctx->node, &gov->ctx_list);
	gov->num_ctx++;
got_ctx:
	gov->nused++;
	ctx->flags |= LCA_TE_CTX_FLAG_USED;
	ctx->pid = current->pid;
	ctx->req = req;
	*pctx = ctx;
	pr_debug("pid %d tfm %p algo (0x%x) got ctx %p\n",
		 current->pid, gov->tfm, gov->alg, ctx);
out:
	return err;
}

static int te_ctx_put(struct lca_te_drv_ctx_gov *gov, lca_te_drv_ctx_t *ctx)
{
	pr_debug("pid %d tfm %p algo (0x%x) put ctx %p\n",
		 current->pid, gov->tfm, gov->alg, ctx);
	ctx->flags &= ~LCA_TE_CTX_FLAG_USED;
	ctx->pid = -1;
	ctx->req = NULL;
	gov->nused--;

	complete(&gov->comp);	/* Wake up one if applicable */
	return 0;
}

static int te_ctx_sync(struct lca_te_drv_ctx_gov *gov, lca_te_drv_ctx_t *sctx)
{
	int rc = 0, err = 0;
	lca_te_drv_ctx_t *ctx = NULL;
	struct list_head *pos;

	if (TE_OPERATION_DIGEST == TE_ALG_GET_CLASS(gov->alg)) {
		/* No need to sync ctx for key-less digest */
		goto out;
	}

	if (NULL == gov->ops.clone) {
		err = -EINVAL;
		goto out;
	}

	pr_debug("pid %d tfm %p algo (0x%x) sync ctx\n",
		 current->pid, gov->tfm, gov->alg);
	/* Sync other existing ctx, if any, with the source one */
	list_for_each(pos, &gov->ctx_list) {
		ctx = list_entry(pos, lca_te_drv_ctx_t, node);
		if (ctx == sctx) {
			continue;
		}

		BUG_ON(ctx->flags & LCA_TE_CTX_FLAG_USED);
		rc = gov->ops.clone(&sctx->base, &ctx->base);
		if (rc != TE_SUCCESS) {
			pr_err("%s clone algo (0x%x) ret:0x%x\n",
				crypto_tfm_alg_driver_name(gov->tfm),
				gov->alg, rc);
			err = TE2ERRNO(rc);
			break;
		}
	}

out:
	return err;
}

struct lca_te_drv_ctx_gov *lca_te_ctx_alloc_gov(const lca_te_base_ops_t *ops,
						te_drv_handle hdl,
						te_algo_t alg,
                                                struct crypto_tfm *tfm)
{
	int err = -EINVAL;
	lca_te_drv_ctx_gov_t *gov = NULL;

	if ((NULL == ops) || (NULL == ops->init) || (NULL == ops->free)) {
		goto out_err;
	}

	gov = (lca_te_drv_ctx_gov_t *)kzalloc(sizeof(*gov), GFP_KERNEL);
	if (NULL == gov) {
		err = -ENOMEM;
		goto out_err;
	}

	/* Fill the gov object */
	INIT_LIST_HEAD(&gov->ctx_list);
	gov->num_ctx = 0;
	memcpy(&gov->ops, ops, sizeof(*ops));
	gov->hdl = hdl;
	gov->alg = alg;
	gov->tfm = tfm;

	/* Pre-allocate driver context if applicable */
	err = te_ctx_prepare(gov);
	if (err != 0) {
		pr_err("te_ctx_prepare faild! algo (0x%x)\n", alg);
		goto out_free;
	}

	/* Initialize locks */
	mutex_init(&gov->mut);
	init_completion(&gov->comp);
	pr_debug("pid %d tfm %p algo (0x%x) alloc gov %p\n",
		 current->pid, tfm, alg, gov);
	goto out;

out_free:
	memset(gov, 0, sizeof(*gov));
	kfree(gov);
out_err:
	gov = ERR_PTR(err);
out:
	return gov;
}

void lca_te_ctx_free_gov(struct lca_te_drv_ctx_gov *gov)
{
	if (NULL == gov) {
		return;
	}

	pr_debug("pid %d tfm %p algo (0x%x) free gov %p\n",
		 current->pid, gov->tfm, gov->alg, gov);
	/* Free all contexts */
	te_ctx_free_all(gov);

	mutex_destroy(&gov->mut);
	memset(gov, 0, sizeof(*gov));
	kfree(gov);
}

int lca_te_ctx_get(struct lca_te_drv_ctx_gov *gov, te_base_ctx_t **pbctx,
		   void *req, bool reset)
{
	int rc = 0, err = -EINVAL;
	lca_te_drv_ctx_t *ctx = NULL;

	if ((NULL == gov) || (NULL == pbctx)) {
		goto out;
	}

	mutex_lock(&gov->mut);
	err = te_ctx_get(gov, &ctx, req);
	mutex_unlock(&gov->mut);
	if (err != 0) {
		goto out;
	}

	/* Reset the driver ctx if needed */
	if (reset) {
		if (NULL == gov->ops.reset) {
			err = -ENOTSUPP;
			goto err_putctx;
		}

		rc = gov->ops.reset(&ctx->base);
		if (rc != TE_SUCCESS) {
			pr_err("%s reset algo (0x%x) ret:0x%x\n",
				crypto_tfm_alg_driver_name(gov->tfm),
				gov->alg, rc);
			goto err_drv;
		}
	}

	*pbctx = &ctx->base;
	goto out;

err_drv:
	err = TE2ERRNO(rc);
err_putctx:
	mutex_lock(&gov->mut);
	/* Put ctx */
	te_ctx_put(gov, ctx);
	mutex_unlock(&gov->mut);
	ctx = NULL;
out:
	return err;
}

int lca_te_ctx_put(struct lca_te_drv_ctx_gov *gov, te_base_ctx_t *bctx)
{
	int err = -EINVAL;
	lca_te_drv_ctx_t *ctx = NULL;

	if ((NULL == gov) || (NULL == bctx)) {
		goto out;
	}

	ctx = container_of(bctx, lca_te_drv_ctx_t, base);
	mutex_lock(&gov->mut);
	err = te_ctx_put(gov, ctx);
	mutex_unlock(&gov->mut);

out:
	return err;
}

int lca_te_ctx_setkey(struct lca_te_drv_ctx_gov *gov, const u8 *key, u32 klen)
{
	int rc = 0, err = 0;
	lca_te_drv_ctx_t *ctx = NULL;

	if ((NULL == gov) || (NULL == key)) {
		err = -EINVAL;
		goto out;
	}

	if (TE_OPERATION_DIGEST == TE_ALG_GET_CLASS(gov->alg)) {
		/* No need to sync ctx for key-less digest */
		goto out;
	}

	if (NULL == gov->ops.setkey) {
		err = -EINVAL;
		goto out;
	}

	mutex_lock(&gov->mut);
	/* Get one ctx */
	err = te_ctx_get(gov, &ctx, NULL);
	if (err != 0) {
		goto out_unlock;
	}

	/* Set key to the ctx */
	pr_debug("pid %d tfm %p algo (0x%x) setkey len %u\n",
		 current->pid, gov->tfm, gov->alg, klen);
	rc = gov->ops.setkey(&ctx->base, key, klen << 3);
	if (rc != TE_SUCCESS) {
		pr_err("%s setkey algo (0x%x) ret:0x%x\n",
			crypto_tfm_alg_driver_name(gov->tfm),
			gov->alg, rc);
		err = TE2ERRNO(rc);
		goto out_putctx;
	}

	/* Sync other existing ctx, if any, with the key-ed one */
	err = te_ctx_sync(gov, ctx);
	if (err != 0) {
		goto out_putctx;
	}

out_putctx:
	/* Put ctx */
	te_ctx_put(gov, ctx);
	ctx = NULL;

out_unlock:
	mutex_unlock(&gov->mut);

out:
	return err;
}

