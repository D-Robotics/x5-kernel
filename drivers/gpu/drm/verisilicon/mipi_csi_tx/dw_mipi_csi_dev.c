// SPDX-License-Identifier: GPL-2.0
/*
 * DW MIPI CSI Platform Driver
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/component.h>
#include <linux/clk.h>
#include <linux/phy/phy.h>

#include "dw_mipi_csi.h"
#include "dw_mipi_csi_hal.h"
#include "dw_mipi_csi_drm.h"
#include "dw_mipi_csi_debugfs.h"

static int dw_mipi_csi_bind(struct device *dev, struct device *master, void *data)
{
	struct dw_mipi_csi *csi = dev_get_drvdata(dev);
	int ret			= 0;

	pm_runtime_resume_and_get(csi->dev);
	ret = clk_prepare_enable(csi->pclk);
	if (ret) {
		pm_runtime_put(csi->dev);
		return ret;
	}

	dw_mipi_csi_drm_bind(csi);

	/* clean state if enabled in u-boot */
	dw_mipi_csi_hal_disable(csi);

	return ret;
}

static void dw_mipi_csi_unbind(struct device *dev, struct device *master, void *data)
{
	struct dw_mipi_csi *csi = dev_get_drvdata(dev);

	clk_disable_unprepare(csi->pclk);
	pm_runtime_put(csi->dev);

	dw_mipi_csi_hal_disable(csi);
}

static const struct component_ops dw_mipi_csi_component_ops = {
	.bind	= dw_mipi_csi_bind,
	.unbind = dw_mipi_csi_unbind,
};

static irqreturn_t dw_mipi_csi_irq_handler(int irq, void *dev_id)
{
	struct dw_mipi_csi *csi = dev_id;

	if (!csi)
		return IRQ_NONE;

	dw_mipi_csi_hal_irq_handle(csi);

	return IRQ_HANDLED;
}

static int dw_mipi_csi_probe(struct platform_device *pdev)
{
	struct device *dev	= &pdev->dev;
	struct dw_mipi_csi *csi = NULL;
	int ret			= 0;
	int irq			= 0;

	csi = devm_kzalloc(dev, sizeof(*csi), GFP_KERNEL);
	if (!csi)
		return -ENOMEM;

	csi->dev = dev;
	dev_set_drvdata(dev, csi);

	/* Map registers */
	csi->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(csi->base))
		return PTR_ERR(csi->base);

	/* Get optional clock */
	csi->pclk = devm_clk_get_optional(dev, "pclk");
	if (IS_ERR(csi->pclk))
		return PTR_ERR(csi->pclk);

	/* Get optional PHY */
	csi->dphy = devm_phy_optional_get(dev, "dphy");
	if (IS_ERR(csi->dphy))
		return PTR_ERR(csi->dphy);

	if (csi->dphy) {
		csi->phy_link = device_link_add(dev, &csi->dphy->dev,
						DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		if (!csi->phy_link) {
			dev_err(dev, "failed to setup link with %s", dev_name(&csi->dphy->dev));
			return -EINVAL;
		}
	}

	// Read clk-continuous from device tree (optional, defaults to continuous mode)
	ret = of_property_read_u8(dev->of_node, "clk-continuous", &csi->clk_continuous);
	if (ret < 0) {
		pr_info("dw_mipi_csi: clk-continuous not found, defaulting to continuous clock \n");
		csi->clk_continuous = 1; // Default to continuous mode
	}

	// Read timing-margin-enable from device tree (optional, defaults to disable)
	ret = of_property_read_u8(dev->of_node, "timing-margin-enable", &csi->timing_margin_enable);
	if (ret < 0) {
		pr_info("dw_mipi_csi: timing-margin-enable not found, defaulting to enabled\n");
		csi->timing_margin_enable = 0; // Default to disable
	}

	/* Request interrupt */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "failed to get IRQ\n");
		return irq;
	}

	ret = devm_request_threaded_irq(dev, irq, NULL, dw_mipi_csi_irq_handler,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT, dev_name(dev), csi);
	if (ret) {
		dev_err(dev, "failed to request IRQ %d\n", irq);
		return ret;
	}

	dw_mipi_csi_debugfs_init(csi);

	pm_runtime_enable(dev);

	csi->lanes   = 4;
	csi->bus_fmt = DW_BUS_FMT_RGB888;

	ret = component_add(dev, &dw_mipi_csi_component_ops);
	if (ret) {
		dw_mipi_csi_drm_unregister(csi);
		goto err_pm_disable;
	}

	dw_mipi_csi_drm_register(csi);

	dw_mipi_csi_hal_irq_enable(csi);

	return 0;

err_pm_disable:
	pm_runtime_disable(dev);
	return ret;
}

static int dw_mipi_csi_remove(struct platform_device *pdev)
{
	struct dw_mipi_csi *csi = platform_get_drvdata(pdev);
	struct device *dev	= &pdev->dev;

	component_del(dev, &dw_mipi_csi_component_ops);
	dw_mipi_csi_drm_unregister(csi);

	pm_runtime_disable(dev);
	dw_mipi_csi_debugfs_fini(csi);
	return 0;
}

static const struct of_device_id dw_mipi_csi_of_match[] = {
	{.compatible = "verisilicon,dw-mipi-csi"}, {}};
MODULE_DEVICE_TABLE(of, dw_mipi_csi_of_match);

struct platform_driver dw_mipi_csi_driver = {
	.driver =
		{
			.name		= "dw-mipi-csi",
			.of_match_table = dw_mipi_csi_of_match,
		},
	.probe	= dw_mipi_csi_probe,
	.remove = dw_mipi_csi_remove,
};

MODULE_DESCRIPTION("DW MIPI CSI Controller Driver");
MODULE_AUTHOR("jiale01.luo <jiale01.luo@d-robotics.cc>");
MODULE_LICENSE("GPL");
