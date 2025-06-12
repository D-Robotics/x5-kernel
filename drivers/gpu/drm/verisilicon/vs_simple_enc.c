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
#include <linux/of_device.h>
#include <linux/module.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_atomic.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include "vs_crtc.h"
#include "vs_simple_enc.h"

static const struct simple_encoder_priv hdmi_priv = {.encoder_type = DRM_MODE_ENCODER_TMDS};

static const struct simple_encoder_priv dsi_priv = {.encoder_type = DRM_MODE_ENCODER_DSI};

static const struct simple_encoder_priv csi_priv = {.encoder_type = DRM_MODE_ENCODER_VIRTUAL};

static const struct simple_encoder_priv virtual_priv = {.encoder_type = DRM_MODE_ENCODER_VIRTUAL};

static const struct simple_encoder_priv dpi_priv = {.encoder_type = DRM_MODE_ENCODER_DPI};

static const struct drm_encoder_funcs encoder_funcs = {.destroy = drm_encoder_cleanup};

static int encoder_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev    = data;
	struct simple_encoder *simple = dev_get_drvdata(dev);
	struct drm_encoder *encoder;
	struct drm_bridge *bridge;
	const char *name = NULL;
	int ret;

	encoder = &simple->encoder;

	/* Encoder. */
	ret = drm_encoder_init(drm_dev, encoder, &encoder_funcs, simple->priv->encoder_type, name);
	if (ret)
		return ret;

	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm_dev, dev->of_node);

	/* output port is port1*/
	bridge = devm_drm_of_get_bridge(dev, dev->of_node, 1, -1);
	if (IS_ERR(bridge)) {
		ret = PTR_ERR(bridge);
		dev_err(dev, "find bridge failed(err=%d)\n", ret);
		goto err;
	}

	ret = drm_bridge_attach(encoder, bridge, NULL, (enum drm_bridge_attach_flags)0);
	if (ret) {
		dev_err(dev, "attach bridge failed\n");
		goto err;
	}

	return 0;
err:
	drm_encoder_cleanup(encoder);

	return ret;
}

static void encoder_unbind(struct device *dev, struct device *master, void *data)
{
	struct simple_encoder *simple = dev_get_drvdata(dev);

	drm_encoder_cleanup(&simple->encoder);
}

static const struct component_ops encoder_component_ops = {
	.bind	= encoder_bind,
	.unbind = encoder_unbind,
};

static const struct of_device_id simple_encoder_dt_match[] = {
	{.compatible = "verisilicon,hdmi-encoder", .data = &hdmi_priv},
	{.compatible = "verisilicon,dp-encoder", .data = &hdmi_priv},
	{.compatible = "verisilicon,dsi-encoder", .data = &dsi_priv},
	{ .compatible = "verisilicon,csi-encoder", .data = &csi_priv},
	{.compatible = "verisilicon,virtual-encoder", .data = &virtual_priv},
	{.compatible = "verisilicon,dpi-encoder", .data = &dpi_priv},
	{},
};
MODULE_DEVICE_TABLE(of, simple_encoder_dt_match);

static int encoder_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct simple_encoder *simple;

	simple = devm_kzalloc(dev, sizeof(*simple), GFP_KERNEL);
	if (!simple)
		return -ENOMEM;

	simple->priv = of_device_get_match_data(dev);

	simple->dev = dev;

	dev_set_drvdata(dev, simple);

	return component_add(dev, &encoder_component_ops);
}

static int encoder_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	component_del(dev, &encoder_component_ops);

	dev_set_drvdata(dev, NULL);

	return 0;
}

struct platform_driver simple_encoder_driver = {
	.probe	= encoder_probe,
	.remove = encoder_remove,
	.driver =
		{
			.name		= "vs-simple-encoder",
			.of_match_table = of_match_ptr(simple_encoder_dt_match),
		},
};

MODULE_DESCRIPTION("Simple Encoder Driver");
MODULE_LICENSE("GPL");
