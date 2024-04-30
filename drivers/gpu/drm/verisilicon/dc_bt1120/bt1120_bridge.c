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
#include <linux/delay.h>
#include <linux/media-bus-format.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_of.h>

#include "vs_drv.h"
#include "vs_gem.h"
#include "bt1120_bridge.h"

extern struct device *bt1120_dev;

#define CRTC_NAME "crtc-bt1120"

static inline struct bt1120_bridge *bridge_to_bt1120_bridge(struct drm_bridge *bridge)
{
	return container_of(bridge, struct bt1120_bridge, bridge);
}

static void bt1120_bridge_atomic_disable(struct drm_bridge *bridge,
					 struct drm_bridge_state *old_bridge_state)
{
	struct bt1120_bridge *bt1120_bridge = bridge_to_bt1120_bridge(bridge);
	struct vs_bt1120 *bt1120	    = dev_get_drvdata(bt1120_bridge->parent);

	if (!bt1120->is_online)
		return;

	/* disable bt1120_en*/
	bt1120_disable(bt1120);
}

static struct drm_crtc *drm_encoder_get_crtc(struct drm_encoder *encoder)
{
	struct drm_connector *connector;
	struct drm_device *dev = encoder->dev;
	bool uses_atomic       = false;
	struct drm_connector_list_iter conn_iter;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter (connector, &conn_iter) {
		if (!connector->state)
			continue;

		uses_atomic = true;

		if (connector->state->best_encoder != encoder)
			continue;

		drm_connector_list_iter_end(&conn_iter);
		return connector->state->crtc;
	}
	drm_connector_list_iter_end(&conn_iter);

	if (uses_atomic)
		return NULL;

	return encoder->crtc;
}

static void bt1120_bridge_atomic_pre_enable(struct drm_bridge *bridge,
					    struct drm_bridge_state *old_bridge_state)
{
	struct drm_crtc *drm_crtc	       = drm_encoder_get_crtc(bridge->encoder);
	struct drm_display_mode *adjusted_mode = &drm_crtc->state->adjusted_mode;

	/* bt1120 output sync signals are positive active*/
	adjusted_mode->flags &= 0xFFFFFFF0;
	adjusted_mode->flags |= DRM_MODE_FLAG_PHSYNC;
	adjusted_mode->flags |= DRM_MODE_FLAG_PVSYNC;
}

static int bt1120_bridge_attach(struct drm_bridge *bridge, enum drm_bridge_attach_flags flags)
{
	struct bt1120_bridge *bt1120_bridge = bridge_to_bt1120_bridge(bridge);

	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found\n");
		return -ENODEV;
	}

	if (!bt1120_bridge->next_bridge)
		return 0;

	return drm_bridge_attach(bridge->encoder, bt1120_bridge->next_bridge, bridge, flags);
}

static void bt1120_bridge_mode_set(struct drm_bridge *bridge, const struct drm_display_mode *mode,
				   const struct drm_display_mode *adjusted_mode)
{
	struct bt1120_bridge *bt1120_bridge = bridge_to_bt1120_bridge(bridge);
	struct vs_bt1120 *bt1120	    = dev_get_drvdata(bt1120_bridge->parent);

	bt1120_set_online_configs(bt1120, mode);
}

static u32 *bt1120_bridge_get_input_bus_fmts(struct drm_bridge *bridge,
					     struct drm_bridge_state *bridge_state,
					     struct drm_crtc_state *crtc_state,
					     struct drm_connector_state *conn_state, u32 output_fmt,
					     unsigned int *num_input_fmts)
{
	struct bt1120_bridge *bt1120_bridge = bridge_to_bt1120_bridge(bridge);
	const struct drm_crtc *crtc	    = conn_state->crtc;
	struct vs_bt1120 *bt1120	    = dev_get_drvdata(bt1120_bridge->parent);
	u32 *input_fmts			    = NULL;

	if (!strcmp(crtc->name, CRTC_NAME))
		bt1120->is_online = false;
	else
		bt1120->is_online = true;

	switch (output_fmt) {
	case MEDIA_BUS_FMT_FIXED:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
		break;
	default:
		*num_input_fmts = 0;
		return NULL;
	}

	input_fmts = kmalloc(sizeof(u32), GFP_KERNEL);
	if (!input_fmts) {
		*num_input_fmts = 0;
		return NULL;
	}

	*num_input_fmts = 1;

	if (bt1120->is_online)
		input_fmts[0] = MEDIA_BUS_FMT_RGB888_1X24;
	else
		input_fmts[0] = MEDIA_BUS_FMT_UYVY8_1X16;

	return input_fmts;
}

static const struct drm_bridge_funcs bt1120_bridge_funcs = {
	.atomic_get_input_bus_fmts = bt1120_bridge_get_input_bus_fmts,
	.attach			   = bt1120_bridge_attach,
	.mode_set		   = bt1120_bridge_mode_set,
	.atomic_pre_enable	   = bt1120_bridge_atomic_pre_enable,
	.atomic_disable		   = bt1120_bridge_atomic_disable,
	.atomic_reset		   = drm_atomic_helper_bridge_reset,
	.atomic_duplicate_state	   = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state	   = drm_atomic_helper_bridge_destroy_state,
};

static int bt1120_bridge_bind(struct device *dev, struct device *master, void *data)
{
	return 0;
}

static void bt1120_bridge_unbind(struct device *dev, struct device *master, void *data) {}

static const struct component_ops bt1120_bridge_component_ops = {
	.bind	= bt1120_bridge_bind,
	.unbind = bt1120_bridge_unbind,
};

static const struct of_device_id bt1120_bridge_driver_dt_match[] = {
	{.compatible = "verisilicon, bt1120-bridge"},
	{.compatible = "verisilicon, bt1120-bridge-wb"},
	{}};
MODULE_DEVICE_TABLE(of, bt1120_bridge_driver_dt_match);

static int bt1120_bridge_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bt1120_bridge *bt1120_bridge;
	u32 ret = 0;

	bt1120_bridge = devm_kzalloc(dev, sizeof(*bt1120_bridge), GFP_KERNEL);
	if (!bt1120_bridge)
		return -ENOMEM;

	bt1120_bridge->dev    = dev;
	bt1120_bridge->parent = bt1120_dev;

	dev_set_drvdata(dev, bt1120_bridge);

	/* Get the next bridge in the pipeline. */
	bt1120_bridge->next_bridge = devm_drm_of_get_bridge(dev, dev->of_node, 1, 0);
	if (IS_ERR(bt1120_bridge->next_bridge)) {
		ret = PTR_ERR(bt1120_bridge->next_bridge);

		bt1120_bridge->next_bridge = NULL;

		if (ret == -ENODEV)
			goto out;

		return ret;
	}

out:
	bt1120_bridge->bridge.funcs   = &bt1120_bridge_funcs;
	bt1120_bridge->bridge.of_node = dev->of_node;
	drm_bridge_add(&bt1120_bridge->bridge);

	return component_add(dev, &bt1120_bridge_component_ops);
}

static int bt1120_bridge_remove(struct platform_device *pdev)
{
	struct device *dev		    = &pdev->dev;
	struct bt1120_bridge *bt1120_bridge = dev_get_drvdata(dev);

	drm_bridge_remove(&bt1120_bridge->bridge);

	component_del(dev, &bt1120_bridge_component_ops);
	dev_set_drvdata(dev, NULL);

	return 0;
}

struct platform_driver bt1120_bridge_driver = {
	.probe	= bt1120_bridge_probe,
	.remove = bt1120_bridge_remove,
	.driver =
		{
			.name		= "vs-bt1120-bridge",
			.of_match_table = of_match_ptr(bt1120_bridge_driver_dt_match),
		},
};

MODULE_DESCRIPTION("VeriSilicon BT1120 Bridge Driver");
MODULE_LICENSE("GPL");
