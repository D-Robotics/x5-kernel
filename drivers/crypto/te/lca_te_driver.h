//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/* \file lca_te_driver.h
 * Arm China Trust Engine Linux Crypto Driver
 */

#ifndef __LCA_TE_DRIVER_H__
#define __LCA_TE_DRIVER_H__

#ifdef COMP_IN_WQ
#include <linux/workqueue.h>
#else
#include <linux/interrupt.h>
#endif
#include <linux/dma-mapping.h>
#include <crypto/algapi.h>
#include <crypto/internal/skcipher.h>
#include <crypto/aes.h>
#include <crypto/des.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#include <crypto/aead.h>
#include <crypto/authenc.h>
#include <crypto/hash.h>
#include <crypto/skcipher.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#include "driver/te_drv.h"

#define BITS_IN_BYTE   (8)
#define DRV_MODULE_VERSION "1.0"
#define TE_CRA_PRIO 400
#ifndef SM4_KEY_SIZE
#define SM4_KEY_SIZE  (16)
#endif

#ifndef SM4_BLOCK_SIZE
#define SM4_BLOCK_SIZE  (16)
#endif

#define TE_AUTOSUSPEND_TIMEOUT	(3000)

#define TE2ERRNO(rc)         te_ret_to_errno(rc)
#define TE_DRV_CALL(fn, ...) TE2ERRNO(fn(__VA_ARGS__))

#define TE_BN_BYTELEN(bn) ({                                                   \
	int _blen_ = te_bn_bytelen(bn);                                        \
	(_blen_ > 0) ? _blen_ : 0;                                             \
})

/**
 * te_{type}_init() fallback: initialize a {type} ctx.
 *
 * The te_{type}_init() has the following drawback:
 * D1: accept main algorithm only.
 */
#define TE_INIT_FALLBACK_FN(type)	lca_te_##type##_init

#define TE_DRV_INIT_FALLBACK(type)                                             \
static int TE_INIT_FALLBACK_FN(type)(te_##type##_ctx_t *ctx,                   \
				te_drv_handle hdl,                             \
				te_algo_t alg)                                 \
{				                                               \
	return te_##type##_init(ctx, hdl, TE_ALG_GET_MAIN_ALG(alg));           \
}

/**
 * struct te_drvdata - driver private data context
 * @te_base:	virt address of the TE registers
 * @irq:	device IRQ number
 * @n:	host id
 */
struct te_drvdata {
	void __iomem *te_base;
	int irq;
	struct clk *clk;
	int n;
	struct platform_device *plat_dev;
	struct te_hwa_host *hwa;
	te_drv_handle h;
	void *cipher_handle;
	void *hash_handle;
	void *aead_handle;
	void *trng_handle;
	void *akcipher_handle;
	void *kpp_handle;
};

struct te_crypto_alg {
	struct list_head entry;
	te_algo_t alg;
	unsigned int data_unit;
	struct te_drvdata *drvdata;
	struct skcipher_alg skcipher_alg;
	struct aead_alg aead_alg;
};

struct te_alg_template {
	char name[CRYPTO_MAX_ALG_NAME];
	char driver_name[CRYPTO_MAX_ALG_NAME];
	unsigned int blocksize;
	u32 type;
	union {
		struct skcipher_alg skcipher;
		struct aead_alg aead;
	} template_u;
	te_algo_t alg;
	unsigned int data_unit;
	struct te_drvdata *drvdata;
};

struct te_request;

/**
 * \brief           The type of the request submitting function.
 * \param[in] treq  Request to submit
 * \retval          0 if the request is well handled synchronously.
 * \retval          -EINPROGRESS if the request is correctly submitted.
 * \retval          Other minus value in case of error.
 */
typedef int (*lca_te_submit_t)(struct te_request *treq);

/**
 * struct te_request - TE driver request base context.
 * \work           work instance used by the request agent.
 * \node           link point used by the request agent.
 * \fn             request submitting hook.
 * \areq           pointer to the base async request.
 * \priv           private data used by the request agent.
 */
struct te_request {
	struct work_struct work;
	struct list_head node;
	lca_te_submit_t fn;
	struct crypto_async_request *areq;
	void *priv;
};

static inline struct device *drvdata_to_dev(struct te_drvdata *drvdata)
{
	return &drvdata->plat_dev->dev;
}

int te_ret_to_errno(int rc);

#endif /*__LCA_TE_DRIVER_H__*/

