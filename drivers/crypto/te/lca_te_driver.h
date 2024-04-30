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
#ifdef CFG_TE_ASYNC_EN
	struct skcipher_alg skcipher_alg;
#else
	struct crypto_alg crypto_alg;
#endif
	struct aead_alg aead_alg;
};

struct te_alg_template {
	char name[CRYPTO_MAX_ALG_NAME];
	char driver_name[CRYPTO_MAX_ALG_NAME];
	unsigned int blocksize;
	u32 type;
	union {
#ifdef CFG_TE_ASYNC_EN
		struct skcipher_alg skcipher;
#else
		struct blkcipher_alg blkcipher;
#endif
		struct aead_alg aead;
	} template_u;
	te_algo_t alg;
	unsigned int data_unit;
	struct te_drvdata *drvdata;
};


static inline struct device *drvdata_to_dev(struct te_drvdata *drvdata)
{
	return &drvdata->plat_dev->dev;
}


#endif /*__LCA_TE_DRIVER_H__*/

