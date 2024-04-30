//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/crypto.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>

#include "lca_te_driver.h"
#include "lca_te_cipher.h"
#include "lca_te_akcipher.h"
#include "lca_te_kpp.h"
#include "lca_te_hash.h"
#include "lca_te_aead.h"
#include "lca_te_otp.h"
#include "lca_te_trng.h"


#include "hwa/te_hwa.h"
#include "driver/te_drv.h"


#ifdef CONFIG_OF
static const struct of_device_id te_dev_of_match[] = {
	{ .compatible = "armchina,trust-engine-600" },
	{}
};
MODULE_DEVICE_TABLE(of, te_dev_of_match);


static int te_get_res_of(struct device *dev, struct te_drvdata *dd)
{
	struct device_node *np = dev->of_node;
	const struct of_device_id *match;
	int err = 0;

	match = of_match_device(te_dev_of_match, dev);
	if (!match) {
		dev_err(dev, "no compatible OF match\n");
		err = -EINVAL;
		goto error;
	}

	err = of_property_read_u32(np,"host-number", &dd->n);
	if (err < 0) {
		dev_err(dev, "can't translate OF node host-number\n");
		err = -EINVAL;
		goto error;
	}

error:
	return err;
}

#else

static int te_get_res_of(struct device *dev, struct te_drvdata *dd)
{
	return -EINVAL;
}

#endif


#ifdef CONFIG_PM
int te_runtime_suspend(struct device *dev)
{
	int rc = -1;
	struct te_drvdata *drvdata = (struct te_drvdata *)dev->driver_data;

	dev_dbg(dev, "start %s ...\n", __func__);

	rc = te_drv_suspend(drvdata->h, TE_SUSPEND_STANDBY);

	clk_disable_unprepare(drvdata->clk);

	return rc;
}

int te_runtime_resume(struct device *dev)
{
	int rc = -1;
	struct te_drvdata *drvdata = (struct te_drvdata *)dev->driver_data;

	dev_dbg(dev, "start %s ...\n", __func__);
	rc = clk_prepare_enable(drvdata->clk);
	if (rc) {
		dev_err(dev, "%s failed: Can't enable clk\n", __func__);
		return rc;
	}

	rc = te_drv_resume(drvdata->h);

	return rc;
}

#endif
#ifdef CONFIG_PM_SLEEP
int te_pm_suspend(struct device *dev)
{
	int rc = -1;
	struct te_drvdata *drvdata = (struct te_drvdata *)dev->driver_data;

	pm_runtime_get_sync(dev);

	dev_dbg(dev, "start %s ...\n", __func__);
	rc = te_drv_suspend(drvdata->h, TE_SUSPEND_MEM);
	if (rc) {
		dev_err(dev, "%s failed\n", __func__);
		return rc;
	}

	pm_runtime_put_noidle(dev);

	clk_disable_unprepare(drvdata->clk);
	return rc;
}

int te_pm_resume(struct device *dev)
{
	int rc = -1;
	struct te_drvdata *drvdata = (struct te_drvdata *)dev->driver_data;

	dev_dbg(dev, "start %s ...\n", __func__);

	rc = clk_prepare_enable(drvdata->clk);
	if (rc) {
		dev_err(dev, "%s failed: Can't enable clk\n", __func__);
		return rc;
	}

	rc = te_drv_resume(drvdata->h);
	if (rc) {
		dev_err(dev, "%s failed\n", __func__);
		return rc;
	}

	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return rc;
}
#endif

static const struct dev_pm_ops te_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(te_pm_suspend, te_pm_resume)
	SET_RUNTIME_PM_OPS(te_runtime_suspend, te_runtime_resume, NULL)
};

static int init_te_resources(struct platform_device *plat_dev)
{
	struct resource *req_mem_te_regs = NULL;
	struct te_drvdata *new_drvdata;
	struct device *dev = &plat_dev->dev;

	int rc = 0;

	new_drvdata = devm_kzalloc(dev, sizeof(*new_drvdata), GFP_KERNEL);
	if (!new_drvdata)
		return -ENOMEM;

	new_drvdata->plat_dev = plat_dev;

	/* Get device resources */
	/* First TE registers space */
	req_mem_te_regs = platform_get_resource(plat_dev, IORESOURCE_MEM, 0);
	/* Map registers space */
	new_drvdata->te_base = ioremap(req_mem_te_regs->start, resource_size(req_mem_te_regs));
	if (IS_ERR(new_drvdata->te_base)) {
		dev_err(dev, "Failed to ioremap registers");
		return PTR_ERR(new_drvdata->te_base);
	}
	dev_dbg(dev, "Got MEM resource (%s): %pR\n", req_mem_te_regs->name,
		req_mem_te_regs);
	dev_dbg(dev, "TE registers mapped from %pa to 0x%p\n",
		&req_mem_te_regs->start, new_drvdata->te_base);

	/* Then IRQ */
	new_drvdata->irq = platform_get_irq(plat_dev, 0);
	if (new_drvdata->irq < 0) {
		return new_drvdata->irq;
	}
	/* Then host id */
	rc = te_get_res_of(dev, new_drvdata);
	if (rc) {
		dev_err(dev, "fail to get host id %d\n", rc);
		return rc;
	}

	new_drvdata->clk = devm_clk_get(dev, "merak_clk");
	if (IS_ERR(new_drvdata->clk)) {
		dev_warn(dev, "clk not specified, assume fixed clock\n");
		/* clk set to NULL, CCF related api will ignore this */
		new_drvdata->clk = NULL;
	}

	rc = clk_prepare_enable(new_drvdata->clk);
	if (rc) {
		dev_err(dev, "fail to enable clock %d\n", rc);
		return rc;
	}

	/* Allocate hwa */
	rc = te_hwa_alloc((te_hwa_host_t **)&new_drvdata->hwa,new_drvdata->te_base,
					  new_drvdata->irq,new_drvdata->n);
	if (rc) {
		dev_err(dev, "te_hwa_alloc failed\n");
		goto post_err;
	}
	/* Allocate driver */
	rc = te_drv_alloc(new_drvdata->hwa,&(new_drvdata->h));
	if (rc) {
		dev_err(dev, "te_hwa_alloc failed\n");
		goto post_hwa_err;
	}
	/* Allocate crypto algs */
	rc = lca_te_cipher_alloc(new_drvdata);
	if (rc) {
		dev_err(dev, "te_cipher_alloc failed\n");
		goto post_drv_err;
	}
	/* Allocate crypto algs */
	rc = lca_te_hash_alloc(new_drvdata);
	if (rc) {
		dev_err(dev, "lca_te_hash_alloc failed\n");
		goto post_cipher_err;
	}
	/* Allocate crypto algs */
	rc = lca_te_aead_alloc(new_drvdata);
	if (rc) {
		dev_err(dev, "lca_te_aead_alloc failed\n");
		goto post_hash_err;
	}
	rc = lca_te_akcipher_alloc(new_drvdata);
	if (rc) {
		dev_err(dev, "lca_te_akcipher_alloc failed\n");
		goto post_aead_err;
	}
	rc = lca_te_kpp_alloc(new_drvdata);
	if (rc) {
		dev_err(dev, "lca_te_kpp_alloc failed\n");
		goto post_akcipher_err;
	}
	rc = lca_te_otp_alloc(new_drvdata);
	if (rc) {
		dev_err(dev, "lca_te_otp_alloc failed\n");
		goto post_kpp_err;
	}
	rc = lca_te_trng_alloc(new_drvdata);
	if (rc) {
		dev_err(dev, "lca_te_trng_alloc failed\n");
		goto post_otp_err;
	}
	platform_set_drvdata(plat_dev, new_drvdata);

	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, TE_AUTOSUSPEND_TIMEOUT);
	rc = pm_runtime_set_active(dev);
	if (rc) {
		dev_err(dev, "lca_runtime_pm setup failed\n");
		goto post_trng_err;
	}
	pm_runtime_enable(dev);

	return 0;

post_trng_err:
	pm_runtime_set_suspended(dev);
	pm_runtime_dont_use_autosuspend(dev);
	lca_te_trng_free(new_drvdata);
post_otp_err:
	lca_te_otp_free(new_drvdata);
post_kpp_err:
	lca_te_kpp_free(new_drvdata);
post_akcipher_err:
	lca_te_akcipher_free(new_drvdata);
post_aead_err:
	lca_te_aead_free(new_drvdata);
post_hash_err:
	lca_te_hash_free(new_drvdata);
post_cipher_err:
	lca_te_cipher_free(new_drvdata);
post_drv_err:
	te_drv_free(new_drvdata->h);
post_hwa_err:
	te_hwa_free(new_drvdata->hwa);
post_err:
	clk_disable_unprepare(new_drvdata->clk);
	return rc;
}

static void cleanup_te_resources(struct platform_device *plat_dev)
{
	struct device *dev = &plat_dev->dev;
	struct te_drvdata *drvdata =
		(struct te_drvdata *)platform_get_drvdata(plat_dev);

	pm_runtime_disable(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_dont_use_autosuspend(dev);

	lca_te_cipher_free(drvdata);
	lca_te_hash_free(drvdata);
	lca_te_aead_free(drvdata);
	lca_te_akcipher_free(drvdata);
	lca_te_kpp_free(drvdata);
	lca_te_otp_free(drvdata);
	lca_te_trng_free(drvdata);
	te_drv_free(drvdata->h);
	te_hwa_free(drvdata->hwa);
	clk_disable_unprepare(drvdata->clk);
}

static int te_probe(struct platform_device *plat_dev)
{
	int rc;
	struct device *dev = &plat_dev->dev;

	/* Map registers space */
	rc = init_te_resources(plat_dev);
	if (rc)
		return rc;

	dev_info(dev, "Arm China te device initialized\n");

	return 0;
}

static int te_remove(struct platform_device *plat_dev)
{
	struct device *dev = &plat_dev->dev;

	dev_dbg(dev, "Releasing te resources...\n");

	cleanup_te_resources(plat_dev);

	dev_info(dev, "Arm China te device terminated\n");

	return 0;
}

static struct platform_driver te_driver = {
	.driver = {
		   .name = "te",
#ifdef CONFIG_OF
		   .of_match_table = te_dev_of_match,
#endif
#ifdef CONFIG_PM
		   .pm = &te_pm,
#endif
	},
	.probe = te_probe,
	.remove = te_remove,
};

static int __init te_driver_init(void)
{

	return platform_driver_register(&te_driver);
}
module_init(te_driver_init);

static void __exit te_driver_exit(void)
{
	platform_driver_unregister(&te_driver);
}
module_exit(te_driver_exit);

/* Module description */
MODULE_DESCRIPTION("Arm China Trust Engine REE Driver");
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_AUTHOR("Arm China");
MODULE_LICENSE("GPL v2");

