//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <linux/err.h>
#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/random.h>
#include "lca_te_driver.h"

//#include "driver/te_drv.h"
#include "driver/te_drv_trng.h"


#define TE_TRNG_QUALITY	512


struct te_trng {
	void *te_drv_h;
	struct te_drvdata *drvdata;
	struct hwrng rng;
};

static int lca_te_trng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
	struct te_trng *trng = NULL;
	struct device *dev = NULL;
	u32 ret;

	trng = container_of(rng, struct te_trng, rng);
	dev = drvdata_to_dev(trng->drvdata);

	pm_runtime_get_sync(dev);
	ret = te_trng_read((te_trng_drv_t *)trng->te_drv_h, buf, max);
	pm_runtime_put_autosuspend(dev);
	if(ret != 0)
		return 0;

	return max;
}

int lca_te_trng_alloc(struct te_drvdata *drvdata)
{
	struct te_trng *trng;
	struct platform_device *pdev = drvdata->plat_dev;
	te_crypt_drv_t *te_drv_handle;
	int ret;

	trng = devm_kzalloc(&pdev->dev, sizeof(*trng), GFP_KERNEL);
	if (!trng)
		return -ENOMEM;

	drvdata->trng_handle = trng;

	te_drv_handle = te_drv_get(drvdata->h,TE_DRV_TYPE_TRNG);
	if(te_drv_handle == NULL) {
		devm_kfree(&pdev->dev, trng);
		return -ENODEV;
	}

	trng->te_drv_h = te_drv_handle;
	trng->drvdata = drvdata;
	trng->rng.name = pdev->name;
	trng->rng.read = lca_te_trng_read;
	trng->rng.quality = TE_TRNG_QUALITY;

	ret = devm_hwrng_register(&pdev->dev, &trng->rng);
	if (ret) {
		te_drv_put(drvdata->h,TE_DRV_TYPE_TRNG);
		devm_kfree(&pdev->dev, trng);
		dev_err(&pdev->dev, "failed to register hwrng!\n");
	}

	return ret;
}

int lca_te_trng_free(struct te_drvdata *drvdata)
{
	struct te_trng *trng;
	struct platform_device *pdev = drvdata->plat_dev;

	trng = drvdata->trng_handle;
	devm_hwrng_unregister(&pdev->dev, &trng->rng);
	te_drv_put(drvdata->h,TE_DRV_TYPE_TRNG);
	devm_kfree(&pdev->dev,trng);
	return 0;
}


