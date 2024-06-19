// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/err.h>
#include <linux/pm_clock.h>
#include <linux/of_address.h>
#include <linux/of_clk.h>
#include <linux/of_platform.h>
#include <linux/mfd/syscon.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <dt-bindings/power/drobot-x5-power.h>
#include "idle.h"

#define IDLE_ACK_DELAY	 1000
#define IDLE_ACK_TIMEOUT 3000

#define APB_LPI_QREQ0	     0x1000
#define APB_LPI_QREQ1	     0x1010
#define APB_LPI_QACCEPT0     0x1004
#define APB_LPI_QACCEPT1     0x1014
#define NOC_IDLE_REQ_REG0    0x0000
#define NOC_IDLE_REQ_REG1    0x0004
#define NOC_IDLE_STATUS_REG0 0x0008
#define NOC_IDLE_STATUS_REG1 0x000C

#define IDLE(r, s, m, re)                                                            \
	{                                                                            \
		.reg_request = (r), .reg_status = (s), .mask = (m), .reverse = (re), \
	}

struct drobot_idle_info {
	u32 reg_request;
	u32 reg_status;
	u32 mask;
	bool reverse; /* set bits is reversed if true */
};

struct drobot_idle {
	void __iomem *base;
	struct device *dev;
	const struct drobot_idle_info *info;
	spinlock_t lock;
};

static const struct drobot_idle_info x5_idles[] = {
	[ISO_PD_CPU_DDR]   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x4, false),
	[ISO_PD_BPU]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x18, false),
	[ISO_PD_DSP_HIFI5] = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x100, false),
	[ISO_PD_DSP_TOP]   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x240, false),
	[ISO_PD_HSIO]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x400, false),
	[ISO_CG_DSP_DMA]   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x200, false),
	[ISO_CG_DMA]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x800, false),
	[ISO_CG_EMMC]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x1000, false),
	[ISO_CG_GMAC]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x2000, false),
	[ISO_CG_SD]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x4000, false),
	[ISO_CG_SDIO]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x8000, false),
	[ISO_CG_SECURE]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x10000, false),
	[ISO_CG_USB2]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x20000, false),
	[ISO_CG_USB3]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x40000, false),
	[ISO_CG_DAP]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x80000, false),
	[ISO_CG_TESTP]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x100000, false),
	[ISO_CG_ETR]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x200000, false),
	[ISO_PD_VIN]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x400000, false),
	[ISO_CG_ISP]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x1000000, false),
	[ISO_CG_DW230]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x2000000, false),
	[ISO_CG_SIF]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x4000000, false),
	[ISO_CG_CAMERA]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x7000000, false),
	[ISO_CG_BT1120]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x8000000, false),
	[ISO_CG_DC8000]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x10000000, false),
	[ISO_CG_DISP_SIF]  = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x20000000, false),
	[ISO_CG_DISP]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x38000000, false),
	[ISO_CG_SRAM0]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x40000000, false),
	[ISO_CG_SRAM1]	   = IDLE(NOC_IDLE_REQ_REG0, NOC_IDLE_STATUS_REG0, 0x80000000, false),

	[ISO_PD_GPU]	   = IDLE(NOC_IDLE_REQ_REG1, NOC_IDLE_STATUS_REG1, 0x1, false),
	[ISO_CG_GPU3D]	   = IDLE(NOC_IDLE_REQ_REG1, NOC_IDLE_STATUS_REG1, 0x4, false),
	[ISO_CG_GPU2D]	   = IDLE(NOC_IDLE_REQ_REG1, NOC_IDLE_STATUS_REG1, 0x8, false),
	[ISO_PD_A55_AXIS]  = IDLE(NOC_IDLE_REQ_REG1, NOC_IDLE_STATUS_REG1, 0x10, false),
	[ISO_PD_AXIS]	   = IDLE(NOC_IDLE_REQ_REG1, NOC_IDLE_STATUS_REG1, 0x20, false),
	[ISO_CG_QSPI_AXIS] = IDLE(NOC_IDLE_REQ_REG1, NOC_IDLE_STATUS_REG1, 0x40, false),
	[ISO_PD_VIDEO]	   = IDLE(NOC_IDLE_REQ_REG1, NOC_IDLE_STATUS_REG1, 0x80, false),
	[ISO_CG_JPEG]	   = IDLE(NOC_IDLE_REQ_REG1, NOC_IDLE_STATUS_REG1, 0x200, false),
	[ISO_CG_VIDEO]	   = IDLE(NOC_IDLE_REQ_REG1, NOC_IDLE_STATUS_REG1, 0x400, false),

	[ISO_Q_GPU]   = IDLE(APB_LPI_QREQ0, APB_LPI_QACCEPT0, 0x18, true),
	[ISO_Q_VIDEO] = IDLE(APB_LPI_QREQ0, APB_LPI_QACCEPT0, 0x18000, true),
	[ISO_Q_BPU]   = IDLE(APB_LPI_QREQ0, APB_LPI_QACCEPT0, 0x3000000, true),

};

static const char * x5_idles_name[] = {
	[ISO_PD_CPU_DDR]   = "iso pd ddr",
	[ISO_PD_BPU]	   = "iso pd bpu",
	[ISO_PD_DSP_HIFI5] = "iso pd dsp hifi",
	[ISO_PD_DSP_TOP]   = "iso pd dsp top",
	[ISO_PD_HSIO]	   = "iso pd hsio",
	[ISO_CG_DSP_DMA]   = "iso cg dsp dma",
	[ISO_CG_DMA]	   = "iso cg dma",
	[ISO_CG_EMMC]	   = "iso cg emmc",
	[ISO_CG_GMAC]	   = "iso cg gmac",
	[ISO_CG_SD]	   = "iso cg sd",
	[ISO_CG_SDIO]	   = "iso cg sdio",
	[ISO_CG_SECURE]	   = "iso cg secure",
	[ISO_CG_USB2]	   = "iso cg usb2",
	[ISO_CG_USB3]	   = "iso cg usb3",
	[ISO_CG_DAP]	   = "iso cg dap",
	[ISO_CG_TESTP]	   = "iso cg testp",
	[ISO_CG_ETR]	   = "iso cg etr",
	[ISO_PD_VIN]	   = "iso pd vin",
	[ISO_CG_ISP]	   = "iso cg isp",
	[ISO_CG_DW230]	   = "iso cg dw230",
	[ISO_CG_SIF]	   = "iso cg sif",
	[ISO_CG_CAMERA]	   = "iso cg cam",
	[ISO_CG_BT1120]	   = "iso cg bt1120",
	[ISO_CG_DC8000]	   = "iso cg dc8000",
	[ISO_CG_DISP_SIF]  = "iso cg disp sif",
	[ISO_CG_DISP]	   = "iso cg disp",
	[ISO_CG_SRAM0]	   = "iso cg sram0",
	[ISO_CG_SRAM1]	   = "iso cg sram1",

	[ISO_PD_GPU]	   = "iso pd gpu",
	[ISO_CG_GPU3D]	   = "iso cg gpu3d",
	[ISO_CG_GPU2D]	   = "iso cg gpu2d",
	[ISO_PD_A55_AXIS]  = "iso pd a55 axis",
	[ISO_PD_AXIS]	   = "iso pd axis",
	[ISO_CG_QSPI_AXIS] = "iso cg qspi axis",
	[ISO_PD_VIDEO]	   = "iso pd video",
	[ISO_CG_JPEG]	   = "iso cg jpeg",
	[ISO_CG_VIDEO]	   = "iso cg video",

	[ISO_Q_GPU]   = "iso q gpu",
	[ISO_Q_VIDEO] = "iso q video",
	[ISO_Q_BPU]   = "iso q bpu",
};

static void __send_request(struct drobot_idle *idle, u8 id, bool is_idle)
{
	u32 val;
	u32 mask	  = idle->info[id].mask;
	void __iomem *reg = idle->base + idle->info[id].reg_request;
	bool set	  = idle->info[id].reverse ? !is_idle : is_idle;
	unsigned long flags;

	spin_lock_irqsave(&idle->lock, flags);
	val = readl(reg);

	if (set)
		val |= mask;
	else
		val &= ~mask;

    writel(val, reg);
	spin_unlock_irqrestore(&idle->lock, flags);
}

static int __wait_idle(struct drobot_idle *idle, u8 id, bool is_idle)
{
	u32 val;
	u32 mask	  = idle->info[id].mask;
	void __iomem *reg = idle->base + idle->info[id].reg_status;
	bool set	  = idle->info[id].reverse ? !is_idle : is_idle;
	u32 target	  = set ? mask : 0;
	int ret;

	ret = readl_poll_timeout_atomic(reg, val, (val & mask) == target, IDLE_ACK_DELAY,
					IDLE_ACK_TIMEOUT);

	if (ret) {
		dev_err(idle->dev, "timeout to wait idle: %s to 0x%x, try again\n", x5_idles_name[id], target);
		__send_request(idle, id, !is_idle);
		__send_request(idle, id, is_idle);
		ret = readl_poll_timeout_atomic(reg, val, (val & mask) == target, IDLE_ACK_DELAY,
						IDLE_ACK_TIMEOUT);
		if (ret) {
			dev_err(idle->dev, "still timeout to wait idle: %s to 0x%x\n", x5_idles_name[id], target);
			return ret;
		}
	}

	return 0;
}

struct device *drobot_idle_get_dev(const struct device_node *of_node)
{
	struct platform_device *pdev;
	struct device_node *np;
	void *drvdata;

	np = of_parse_phandle(of_node, "idle-dev", 0);
	if (!np)
		return ERR_PTR(-ENODEV);

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev)
		return ERR_PTR(-ENODEV);

	drvdata = platform_get_drvdata(pdev);
	if (!drvdata)
		return ERR_PTR(-EPROBE_DEFER);

	return &pdev->dev;
}
EXPORT_SYMBOL_GPL(drobot_idle_get_dev);

int drobot_idle_request(struct device *dev, u8 idle_id, bool is_idle, bool skip_wait_idle)
{
	struct drobot_idle *idle = dev_get_drvdata(dev);

	__send_request(idle, idle_id, is_idle);

	if (!is_idle)
		if (skip_wait_idle)
			return 0;

	return __wait_idle(idle, idle_id, is_idle);
}
EXPORT_SYMBOL_GPL(drobot_idle_request);

static int drobot_idle_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct drobot_idle *idle;

	idle = devm_kzalloc(dev, sizeof(*idle), GFP_KERNEL);
	if (!idle)
		return -ENOMEM;

	idle->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(idle->base))
		return PTR_ERR(idle->base);

	spin_lock_init(&idle->lock);
	idle->dev = dev;
	idle->info = of_device_get_match_data(dev);

	dev_set_drvdata(dev, idle);

	return 0;
}

static int drobot_idle_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_set_drvdata(dev, NULL);

	return 0;
}

static const struct of_device_id drobot_idle_of_match[] = {
	{.compatible = "horizon,idle", .data = x5_idles},
	{},
};

MODULE_DEVICE_TABLE(of, drobot_idle_of_match);

static struct platform_driver drobot_idle_driver = {
	.driver =
		{
			.name		= "drobot_idle",
			.of_match_table = drobot_idle_of_match,
		},
	.probe	= drobot_idle_probe,
	.remove = drobot_idle_remove,
};

static int __init drobot_idle_drv_register(void)
{
	return platform_driver_register(&drobot_idle_driver);
}
postcore_initcall(drobot_idle_drv_register);
