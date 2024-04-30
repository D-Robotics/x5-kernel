// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024, VeriSilicon Holdings Co., Ltd. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/component.h>
#include <linux/clk.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/reset.h>
#include <linux/delay.h>

#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>

#include "vs_fb.h"
#include "dc_proc.h"
#include "dc_hw_proc.h"
#include "vs_sif_reg.h"
#include "vs_sif.h"
#include "vs_drv.h"

enum sif_format_type {
	SIF_FMT_NV12 = 0,
	SIF_FMT_NV16,
	SIF_FMT_YUYV,
	SIF_FMT_RGB444 = 4,
	SIF_FMT_RGB555,
	SIF_FMT_RGB565,
	SIF_FMT_RGB888,
};

enum sif_irq_type {
	SIF_FS_IRQ		      = BIT(0),
	SIF_DMA_DONE_IRQ	      = BIT(1),
	SIF_OVERFLOW_IRQ	      = BIT(2),
	SIF_DMA_DONE_EBD_IRQ	      = BIT(3),
	SIF_BUF_ERROR_EBD_IRQ	      = BIT(4),
	SIF_EBD_BUF_ALMOST_FULL_IRQ   = BIT(5),
	SIF_PIXEL_BUF_ALMOST_FULL_IRQ = BIT(6),
	SIF_IPI_FRAME_END_IRQ	      = BIT(7),
	SIF_FRAME_SIZE_ERR_IRQ	      = BIT(8),
	SIF_IPI_HALT_INT_IRQ	      = BIT(9),
};

static inline void sif_write(struct vs_sif *hw, u32 reg, u32 value)
{
	writel(value, hw->base + reg);
}

static inline u32 sif_read(struct vs_sif *hw, u32 reg)
{
	return readl(hw->base + reg);
}

static inline void sif_set_clear(struct vs_sif *hw, u32 reg, u32 set, u32 clear)
{
	u32 value = sif_read(hw, reg);

	value &= ~clear;
	value |= set;
	sif_write(hw, reg, value);
}

static u8 to_sif_color_format(u32 drm_format)
{
	switch (drm_format) {
	case DRM_FORMAT_XBGR4444:
		return SIF_FMT_RGB444;
	case DRM_FORMAT_XBGR1555:
		return SIF_FMT_RGB555;
	case DRM_FORMAT_BGR565:
		return SIF_FMT_RGB565;
	case DRM_FORMAT_XBGR8888:
		return SIF_FMT_RGB888;
	case DRM_FORMAT_NV12:
		return SIF_FMT_NV12;
	case DRM_FORMAT_NV16:
		return SIF_FMT_NV16;
	case DRM_FORMAT_YUYV:
		return SIF_FMT_YUYV;
	default:
		return SIF_FMT_RGB888;
	}

	return 0;
}

static void sif_init(struct vs_sif *sif)
{
	sif_write(sif, IPI0_IRQ_EN, SIF_DMA_DONE_IRQ);
}

void sif_set_output(struct vs_sif *sif, struct drm_framebuffer *fb)
{
	dma_addr_t dma_addr[MAX_NUM_PLANES] = {};
	u8 color;

	if (!fb) {
		sif_set_clear(sif, SIF_DMA_CTL, IPI0_MIPI_DMA_CONFIG_UPDATE, IPI0_MIPI_DMA_EN);

		reset_control_assert(sif->rst);

		usleep_range(1000, 1100);

		reset_control_deassert(sif->rst);

		sif_init(sif);

		return;
	}

	sif_set_clear(sif, SIF_DMA_CTL, IPI0_MIPI_DMA_EN, 0);

	get_buffer_addr(fb, dma_addr);

	color = to_sif_color_format(fb->format->format);

	sif_write(sif, IPI0_IMG_OUT_BADDR_Y, dma_addr[0]);
	sif_write(sif, IPI0_IMG_OUT_BADDR_UV, dma_addr[1]);
	sif_set_clear(sif, SIF_DMA_CTL, IPI0_MIPI_PIXEL_TYPE_MASK & (color << 1), 0);

	sif_write(sif, IPI0_IMG_OUT_PIX_HSIZE, fb->width - 1);
	sif_write(sif, IPI0_IMG_OUT_PIX_VSIZE, fb->height - 1);
	sif_write(sif, IPI0_IMG_OUT_PIX_HSTRIDE, fb->pitches[0] - 1);

	sif_set_clear(sif, SIF_DMA_CTL, IPI0_MIPI_DMA_CONFIG_UPDATE, 0);
}

static irqreturn_t sif_isr(int irq, void *data)
{
	struct vs_sif *sif = data;
	u32 status;

	status = sif_read(sif, IPI0_IRQ_STATUS);

	sif_write(sif, IPI0_IRQ_CLR, status);

	if (status & SIF_DMA_DONE_IRQ)
		schedule_work(sif->work);

	return IRQ_HANDLED;
}

static int sif_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;
	int ret;

	ret = vs_drm_iommu_attach_device(drm_dev, dev);
	if (ret < 0) {
		dev_err(dev, "Failed to attached iommu device.\n");
		return ret;
	}

	return 0;
}

static void sif_unbind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;

	vs_drm_iommu_detach_device(drm_dev, dev);
}

static const struct component_ops sif_component_ops = {
	.bind	= sif_bind,
	.unbind = sif_unbind,
};

static int sif_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vs_sif *sif;
	int irq, ret;

	sif = devm_kzalloc(dev, sizeof(*sif), GFP_KERNEL);
	if (!sif)
		return -ENOMEM;

	dev_set_drvdata(dev, sif);

	sif->dev = dev;

	sif->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(sif->base))
		return PTR_ERR(sif->base);

	irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(dev, irq, sif_isr, 0, dev_name(dev), sif);
	if (ret < 0) {
		dev_err(dev, "Failed to install irq:%u.\n", irq);
		return ret;
	}

	sif->apb_clk = devm_clk_get_optional(dev, "apb_clk");
	if (IS_ERR(sif->apb_clk)) {
		dev_err(dev, "failed to get apb_clk source\n");
		return PTR_ERR(sif->apb_clk);
	}

	ret = clk_prepare_enable(sif->apb_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare/enable apb_clk\n");
		return ret;
	}

	sif->rst = devm_reset_control_get_optional(dev, "rst");
	if (IS_ERR(sif->rst))
		return PTR_ERR(sif->rst);

	sif->axi_clk = devm_clk_get_optional(dev, "axi_clk");
	if (IS_ERR(sif->axi_clk)) {
		dev_err(dev, "failed to get axi_clk source\n");
		ret = PTR_ERR(sif->axi_clk);
		goto err_disable_apb_clk;
	}

	ret = clk_prepare_enable(sif->axi_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare/enable axi_clk\n");
		goto err_disable_apb_clk;
	}

	sif_init(sif);

	return component_add(dev, &sif_component_ops);

err_disable_apb_clk:
	clk_disable_unprepare(sif->apb_clk);
	return ret;
}

static int sif_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	component_del(dev, &sif_component_ops);

	dev_set_drvdata(dev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sif_suspend(struct device *dev)
{
	struct vs_sif *sif;

	sif = dev_get_drvdata(dev);

	reset_control_assert(sif->rst);

	usleep_range(1000, 1100);

	reset_control_deassert(sif->rst);

	clk_disable_unprepare(sif->axi_clk);

	clk_disable_unprepare(sif->apb_clk);

	return 0;
}

static int sif_resume(struct device *dev)
{
	struct vs_sif *sif;
	int ret;

	sif = dev_get_drvdata(dev);

	ret = clk_prepare_enable(sif->apb_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare/enable apb_clk\n");
		return ret;
	}

	ret = clk_prepare_enable(sif->axi_clk);
	if (ret < 0) {
		dev_err(dev, "failed to prepare/enable axi_clk\n");
		goto err_disable_apb_clk;
	}

	sif_init(sif);

	return 0;

err_disable_apb_clk:
	clk_disable_unprepare(sif->apb_clk);
	return ret;
}
#endif

static const struct dev_pm_ops sif_pm_ops = {SET_SYSTEM_SLEEP_PM_OPS(sif_suspend, sif_resume)};

static const struct of_device_id sif_driver_dt_match[] = {
	{.compatible = "verisilicon,disp_sif"},
	{},
};
MODULE_DEVICE_TABLE(of, sif_driver_dt_match);

struct platform_driver sif_platform_driver = {
	.probe	= sif_probe,
	.remove = sif_remove,
	.driver =
		{
			.name		= "vs-disp-sif",
			.of_match_table = of_match_ptr(sif_driver_dt_match),
			.pm		= &sif_pm_ops,
		},
};

MODULE_DESCRIPTION("VeriSilicon sif Driver");
MODULE_LICENSE("GPL");
