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

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/media-bus-format.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_print.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>

#define DISP_BT1120_DC2SIF_CFG	     0
#define SIF_DPI_VSYNC_POLARITY	     BIT(7)
#define SIF_DPI_VSYNC_POLARITY_SHIFT 7
#define SIF_DPI_HSYNC_POLARITY	     BIT(6)
#define SIF_DPI_HSYNC_POLARITY_SHIFT 6
#define SIF_DPI_DE_POLARITY	     BIT(5)
#define SIF_DPI_DE_POLARITY_SHIFT    5
#define SIF_DPI2IPI_EN		     BIT(4)
#define SIF_DPI2IPI_EN_SHIFT	     4
#define SIF_DPI_DATA_FORMAT	     GENMASK(3, 1)
#define SIF_DPI_DATA_FORMAT_SHIFT    1
#define BT1120_DC2SIF_EN	     BIT(0)

#define DISP_CSITX_IDI_EN 0x8
#define CSITX_IDI_EN	  BIT(0)

#define DISP_CSITX_DPI_CFG	     0xC
#define CSI_DPI_VSYNC_POLARITY	     BIT(5)
#define CSI_DPI_VSYNC_POLARITY_SHIFT 5
#define CSI_DPI_HSYNC_POLARITY	     BIT(4)
#define CSI_DPI_HSYNC_POLARITY_SHIFT 4
#define CSI_DPI_DE_POLARITY	     BIT(3)
#define CSI_DPI_DE_POLARITY_SHIFT    3
#define CSI_DPI_DATA_FORMAT	     GENMASK(2, 0)

#define DISP_DC2CSI_DSI_EN	0x10
#define DISP_DC2CSI_DSI_EN_MASK GENMASK(1, 0)
#define DC2DSI_EN		BIT(1)
#define DC2CSI_EN		BIT(0)

#define DISP_DC2BT1120_EN 0x14
#define DC2BT1120_EN	  BIT(0)

#define DISP_BT1120_2_CSITX_EN 0x18
#define BT1120_2_CSITX_EN      BIT(0)

#define DISP_BT1120_PIN_MUX_CTRL0 0x9c
#define SELECT_BT1120_DATA	  0x55555555

#define DISP_BT1120_PIN_MUX_CTRL1 0xa0
#define SELECT_BT1120_CLK	  0x1

struct dss_data {
	u32 *offsets;
	u32 *masks;
	u32 *values;
	u32 *rst_values;
	const char *crtc_name;
	u32 cnt;
	bool use_post_disable;
};

struct syscon_bridge;

struct syscon_bridge_funcs {
	u32 *(*get_input_bus_fmt)(struct syscon_bridge *sbridge,
				  struct drm_bridge_state *bridge_state,
				  struct drm_crtc_state *crtc_state,
				  struct drm_connector_state *conn_state, u32 output_fmt,
				  unsigned int *num_input_fmts);
	u32 (*get_out_bus_fmt)(struct syscon_bridge *sbridge);
	void (*mode_set)(struct syscon_bridge *sbridge, u32 bus_fmt);
};

struct syscon_data {
	const struct dss_data *reg_datas;
	u8 reg_cnt;
	const struct syscon_bridge_funcs *funcs;
};

struct syscon_bridge {
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	struct regmap *dss_regmap;
	struct drm_display_mode mode;
	const struct syscon_data *priv;
	const struct dss_data *active_data;
};

static const struct dss_data dsi_data[] = {
	{
		.offsets = (u32[]){DISP_DC2CSI_DSI_EN},
		.masks	 = (u32[]){DISP_DC2CSI_DSI_EN_MASK},
		.values	 = (u32[]){DC2DSI_EN},
		/**
		 * IDI output need DC2CSI_EN set, so reset to this value after
		 * dsi pipe is teared down
		 */
		.rst_values	  = (u32[]){DC2CSI_EN},
		.cnt		  = 1,
		.use_post_disable = false,
	},
};

static const struct syscon_data dsi_subsys = {
	.reg_datas = dsi_data,
	.reg_cnt   = ARRAY_SIZE(dsi_data),
};

static const struct dss_data csi_data[] = {
	{
		.offsets = (u32[]){DISP_DC2CSI_DSI_EN, DISP_CSITX_IDI_EN, DISP_BT1120_2_CSITX_EN},
		.masks	 = (u32[]){DISP_DC2CSI_DSI_EN_MASK, CSITX_IDI_EN, BT1120_2_CSITX_EN},
		.values	 = (u32[]){0, 0, BT1120_2_CSITX_EN},
		.rst_values = (u32[]){0, 0, 0},
		.cnt	    = 3,
		.use_post_disable = false,
	},
};

enum dpi_bus_format {
	BUS_FMT_RGB565_1,
	BUS_FMT_RGB565_2,
	BUS_FMT_RGB565_3,
	BUS_FMT_RGB666_PACKED,
	BUS_FMT_RGB666,
	BUS_FMT_RGB888,
	BUS_FMT_YUV422,
};

static enum dpi_bus_format bus_fmt_to_dpi_fmt(u32 bus_fmt)
{
	enum dpi_bus_format fmt;

	switch (bus_fmt) {
	case MEDIA_BUS_FMT_RGB565_1X16:
		fmt = BUS_FMT_RGB565_1;
		break;
	case MEDIA_BUS_FMT_RGB666_1X18:
		fmt = BUS_FMT_RGB666_PACKED;
		break;
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
		fmt = BUS_FMT_RGB666;
		break;
	case MEDIA_BUS_FMT_RGB888_1X24:
		fmt = BUS_FMT_RGB888;
		break;
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
		fmt = BUS_FMT_YUV422;
		break;
	default:
		fmt = BUS_FMT_RGB888;
		break;
	}

	return fmt;
}

static void csi_mode_set(struct syscon_bridge *sbridge, u32 bus_fmt)
{
	const struct drm_display_mode *mode = &sbridge->mode;
	bool v_sync_polarity = !(mode->flags & DRM_MODE_FLAG_PVSYNC);
	bool h_sync_polarity = !(mode->flags & DRM_MODE_FLAG_PHSYNC);
	struct drm_bridge *bridge = &sbridge->bridge;
	struct drm_encoder *encoder = bridge->encoder;
	struct drm_crtc *crtc = encoder ? encoder->crtc : NULL;
	const char *crtc_name = crtc ? crtc->name : "unknown";

	DRM_INFO("csi_mode_set: crtc=%s, bus_fmt=0x%x, default vsync=%u, hsync=%u\n",
	         crtc_name, bus_fmt, v_sync_polarity, h_sync_polarity);

	/* DATA FORMAT */
	regmap_update_bits(sbridge->dss_regmap,
	                   DISP_CSITX_DPI_CFG,
	                   CSI_DPI_DATA_FORMAT,
	                   bus_fmt_to_dpi_fmt(bus_fmt));
	DRM_INFO("  -> CSI_DPI_DATA_FORMAT set to %u\n",
	         bus_fmt_to_dpi_fmt(bus_fmt));

	/* VSYNC/HSYNC POLARITY override for bt1120 */
	if (!strcmp(crtc_name, "crtc-bt1120")) {
		DRM_INFO("  bt1120 path: forcing VSYNC=0, HSYNC=0\n");
		regmap_update_bits(sbridge->dss_regmap,
		                   DISP_CSITX_DPI_CFG,
		                   CSI_DPI_VSYNC_POLARITY,
		                   0 << CSI_DPI_VSYNC_POLARITY_SHIFT);
		regmap_update_bits(sbridge->dss_regmap,
		                   DISP_CSITX_DPI_CFG,
		                   CSI_DPI_HSYNC_POLARITY,
		                   0 << CSI_DPI_HSYNC_POLARITY_SHIFT);
	} else {
		DRM_INFO("  normal path: VSYNC=%u, HSYNC=%u\n",
		         v_sync_polarity, h_sync_polarity);
		regmap_update_bits(sbridge->dss_regmap,
		                   DISP_CSITX_DPI_CFG,
		                   CSI_DPI_VSYNC_POLARITY,
		                   v_sync_polarity << CSI_DPI_VSYNC_POLARITY_SHIFT);
		regmap_update_bits(sbridge->dss_regmap,
		                   DISP_CSITX_DPI_CFG,
		                   CSI_DPI_HSYNC_POLARITY,
		                   h_sync_polarity << CSI_DPI_HSYNC_POLARITY_SHIFT);
	}

	/* DATA ENABLE POLARITY: always zero */
	regmap_update_bits(sbridge->dss_regmap,
	                   DISP_CSITX_DPI_CFG,
	                   CSI_DPI_DE_POLARITY,
	                   0);
	DRM_INFO("  -> CSI_DPI_DE_POLARITY set to 0\n");
}


static const struct syscon_bridge_funcs csi_bridge_funcs = {
	.mode_set = csi_mode_set,
};

static const struct syscon_data csi_subsys = {
	.reg_datas = csi_data,
	.reg_cnt   = ARRAY_SIZE(csi_data),
	.funcs	   = &csi_bridge_funcs,
};

static const struct dss_data bt1120_data[] = {
	{
		.offsets	  = (u32[]){DISP_DC2BT1120_EN, DISP_BT1120_PIN_MUX_CTRL0,
					    DISP_BT1120_PIN_MUX_CTRL1},
		.masks		  = (u32[]){DC2BT1120_EN, SELECT_BT1120_DATA, SELECT_BT1120_CLK},
		.values		  = (u32[]){DC2BT1120_EN, SELECT_BT1120_DATA, SELECT_BT1120_CLK},
		.rst_values	  = (u32[]){0, 0, 0},
		.crtc_name	  = "crtc-dc8000",
		.cnt		  = 3,
		.use_post_disable = false,
	},
	{
		.offsets	  = (u32[]){DISP_DC2BT1120_EN, DISP_BT1120_PIN_MUX_CTRL0,
					    DISP_BT1120_PIN_MUX_CTRL1},
		.masks		  = (u32[]){DC2BT1120_EN, SELECT_BT1120_DATA, SELECT_BT1120_CLK},
		.values		  = (u32[]){0, SELECT_BT1120_DATA, SELECT_BT1120_CLK},
		.rst_values	  = (u32[]){0, 0, 0},
		.crtc_name	  = "crtc-bt1120",
		.cnt		  = 3,
		.use_post_disable = false,
	},
};

static const struct syscon_data bt1120_subsys = {
	.reg_datas = bt1120_data,
	.reg_cnt   = ARRAY_SIZE(bt1120_data),
};

static void dc_wb_mode_set(struct syscon_bridge *sbridge, u32 bus_fmt)
{
	const struct drm_display_mode *mode = &sbridge->mode;
	bool v_sync_polarity, h_sync_polarity;

	v_sync_polarity = !(mode->flags & DRM_MODE_FLAG_PVSYNC);
	h_sync_polarity = !(mode->flags & DRM_MODE_FLAG_PHSYNC);

	regmap_update_bits(sbridge->dss_regmap, DISP_BT1120_DC2SIF_CFG, SIF_DPI_DATA_FORMAT,
			   bus_fmt_to_dpi_fmt(bus_fmt) << SIF_DPI_DATA_FORMAT_SHIFT);

	regmap_update_bits(sbridge->dss_regmap, DISP_BT1120_DC2SIF_CFG, SIF_DPI_VSYNC_POLARITY,
			   v_sync_polarity << SIF_DPI_VSYNC_POLARITY_SHIFT);

	regmap_update_bits(sbridge->dss_regmap, DISP_BT1120_DC2SIF_CFG, SIF_DPI_HSYNC_POLARITY,
			   h_sync_polarity << SIF_DPI_HSYNC_POLARITY_SHIFT);

	regmap_update_bits(sbridge->dss_regmap, DISP_BT1120_DC2SIF_CFG, SIF_DPI_DE_POLARITY, 0);
}

static const struct syscon_bridge_funcs dc_wb_bridge_funcs = {
	.mode_set = dc_wb_mode_set,
};

static const struct dss_data dc_wb_data[] = {
	{
		.offsets =
			(u32[]){DISP_DC2BT1120_EN, DISP_BT1120_DC2SIF_CFG, DISP_BT1120_DC2SIF_CFG},
		.masks		  = (u32[]){DC2BT1120_EN, SIF_DPI2IPI_EN, BT1120_DC2SIF_EN},
		.values		  = (u32[]){0, SIF_DPI2IPI_EN, 0},
		.rst_values	  = (u32[]){0, 0, 0},
		.crtc_name	  = "crtc-dc8000",
		.cnt		  = 3,
		.use_post_disable = false,
	},
};

static const struct syscon_data dc_wb_subsys = {
	.reg_datas = dc_wb_data,
	.reg_cnt   = ARRAY_SIZE(dc_wb_data),
	.funcs	   = &dc_wb_bridge_funcs,
};

static void bt_wb_mode_set(struct syscon_bridge *sbridge, u32 bus_fmt)
{
	regmap_update_bits(sbridge->dss_regmap, DISP_BT1120_DC2SIF_CFG, SIF_DPI_DATA_FORMAT,
			   bus_fmt_to_dpi_fmt(bus_fmt) << SIF_DPI_DATA_FORMAT_SHIFT);

	regmap_update_bits(sbridge->dss_regmap, DISP_BT1120_DC2SIF_CFG, SIF_DPI_VSYNC_POLARITY,
			   0 << SIF_DPI_VSYNC_POLARITY_SHIFT);

	regmap_update_bits(sbridge->dss_regmap, DISP_BT1120_DC2SIF_CFG, SIF_DPI_HSYNC_POLARITY,
			   0 << SIF_DPI_HSYNC_POLARITY_SHIFT);

	regmap_update_bits(sbridge->dss_regmap, DISP_BT1120_DC2SIF_CFG, SIF_DPI_DE_POLARITY, 0);
}

static const struct syscon_bridge_funcs bt_wb_bridge_funcs = {
	.mode_set = bt_wb_mode_set,
};

static const struct dss_data bt1120_wb_data[] = {
	{
		.offsets =
			(u32[]){DISP_DC2BT1120_EN, DISP_BT1120_DC2SIF_CFG, DISP_BT1120_DC2SIF_CFG},
		.masks		  = (u32[]){DC2BT1120_EN, SIF_DPI2IPI_EN, BT1120_DC2SIF_EN},
		.values		  = (u32[]){DC2BT1120_EN, SIF_DPI2IPI_EN, BT1120_DC2SIF_EN},
		.rst_values	  = (u32[]){0, 0, 0},
		.crtc_name	  = "crtc-dc8000",
		.cnt		  = 3,
		.use_post_disable = true,
	},
	{
		.offsets =
			(u32[]){DISP_DC2BT1120_EN, DISP_BT1120_DC2SIF_CFG, DISP_BT1120_DC2SIF_CFG},
		.masks		  = (u32[]){DC2BT1120_EN, SIF_DPI2IPI_EN, BT1120_DC2SIF_EN},
		.values		  = (u32[]){0, SIF_DPI2IPI_EN, BT1120_DC2SIF_EN},
		.rst_values	  = (u32[]){0, 0, 0},
		.crtc_name	  = "crtc-bt1120",
		.cnt		  = 3,
		.use_post_disable = true,
	},
};

static const struct syscon_data bt1120_wb_subsys = {
	.reg_datas = bt1120_wb_data,
	.reg_cnt   = ARRAY_SIZE(bt1120_wb_data),
	.funcs	   = &bt_wb_bridge_funcs,
};

static inline struct syscon_bridge *drm_bridge_to_syscon_bridge(struct drm_bridge *bridge)
{
	return container_of(bridge, struct syscon_bridge, bridge);
}

static void syscon_bridge_init(struct syscon_bridge *sbridge)
{
	int i;
	const struct dss_data *data = &sbridge->priv->reg_datas[0];

	for (i = 0; i < data->cnt; i++)
		regmap_update_bits(sbridge->dss_regmap, data->offsets[i], data->masks[i],
				   data->rst_values[i]);
}

static int syscon_bridge_attach(struct drm_bridge *bridge, enum drm_bridge_attach_flags flags)
{
	struct syscon_bridge *sbridge = drm_bridge_to_syscon_bridge(bridge);

	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found\n");
		return -ENODEV;
	}

	syscon_bridge_init(sbridge);

	if (!sbridge->next_bridge)
		return 0;

	return drm_bridge_attach(bridge->encoder, sbridge->next_bridge, bridge, 0);
}

static void syscon_bridge_mode_set(struct drm_bridge *bridge, const struct drm_display_mode *mode,
				   const struct drm_display_mode *adjusted_mode)
{
	struct syscon_bridge *sbridge	      = drm_bridge_to_syscon_bridge(bridge);
	const struct dss_data *data	      = sbridge->active_data;
	struct drm_bridge_state *bridge_state = drm_priv_to_bridge_state(bridge->base.state);
	const struct syscon_bridge_funcs *funcs;
	u32 bus_fmt;
	int i;

	drm_mode_copy(&sbridge->mode, adjusted_mode);

	bus_fmt = bridge_state->input_bus_cfg.format;

	funcs = sbridge->priv->funcs;
	if (funcs && funcs->mode_set)
		funcs->mode_set(sbridge, bus_fmt);

	for (i = 0; i < data->cnt; i++)
		regmap_update_bits(sbridge->dss_regmap, data->offsets[i], data->masks[i],
				   data->values[i]);
}

static void syscon_bridge_disable(struct drm_bridge *bridge)
{
	struct syscon_bridge *sbridge = drm_bridge_to_syscon_bridge(bridge);
	const struct dss_data *data   = sbridge->active_data;
	int i;

	if (!data->use_post_disable) {
		for (i = 0; i < data->cnt; i++)
			regmap_update_bits(sbridge->dss_regmap, data->offsets[i], data->masks[i],
					   data->rst_values[i]);
	}
}

static void syscon_bridge_post_disable(struct drm_bridge *bridge)
{
	struct syscon_bridge *sbridge = drm_bridge_to_syscon_bridge(bridge);
	const struct dss_data *data   = sbridge->active_data;
	int i;

	if (data->use_post_disable) {
		for (i = 0; i < data->cnt; i++)
			regmap_update_bits(sbridge->dss_regmap, data->offsets[i], data->masks[i],
					   data->rst_values[i]);
	}
}

static u32 *syscon_bridge_get_input_bus_fmts(struct drm_bridge *bridge,
					     struct drm_bridge_state *bridge_state,
					     struct drm_crtc_state *crtc_state,
					     struct drm_connector_state *conn_state, u32 output_fmt,
					     unsigned int *num_input_fmts)
{
	struct syscon_bridge *sbridge = drm_bridge_to_syscon_bridge(bridge);
	const struct syscon_bridge_funcs *funcs;
	struct drm_connector *connector = conn_state->connector;
	u32 *input_fmts;

	funcs = sbridge->priv->funcs;
	if (funcs && funcs->get_input_bus_fmt)
		return funcs->get_input_bus_fmt(sbridge, bridge_state, crtc_state, conn_state,
						output_fmt, num_input_fmts);

	/*
	 * Default return input fmt same as output fmt if output is not MEDIA_BUS_FMT_FIXED
	 * else return format from connector.display_info
	 */
	input_fmts = kmalloc(sizeof(u32), GFP_KERNEL);
	if (!input_fmts) {
		*num_input_fmts = 0;
		return NULL;
	}

	*num_input_fmts = 1;

	if (output_fmt == MEDIA_BUS_FMT_FIXED && connector->display_info.num_bus_formats)
		input_fmts[0] = connector->display_info.bus_formats[0];
	else
		input_fmts[0] = output_fmt;

	return input_fmts;
}

static const struct dss_data *find_dss_data(struct syscon_bridge *sbridge, struct drm_crtc *crtc)
{
	const struct dss_data *data;
	int i;

	if (sbridge->priv->reg_cnt == 1)
		return &sbridge->priv->reg_datas[0];

	for (i = 0; i < sbridge->priv->reg_cnt; i++) {
		data = &sbridge->priv->reg_datas[i];
		if (!strcmp(data->crtc_name, crtc->name))
			return data;
	}

	return NULL;
}

static int syscon_bridge_atomic_check(struct drm_bridge *bridge,
				      struct drm_bridge_state *bridge_state,
				      struct drm_crtc_state *crtc_state,
				      struct drm_connector_state *conn_state)
{
	struct syscon_bridge *sbridge = drm_bridge_to_syscon_bridge(bridge);

	sbridge->active_data = find_dss_data(sbridge, crtc_state->crtc);

	if (!sbridge->active_data)
		return -EINVAL;

	return 0;
}

static const struct drm_bridge_funcs syscon_bridge_bridge_funcs = {
	.atomic_duplicate_state	   = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state	   = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset		   = drm_atomic_helper_bridge_reset,
	.atomic_get_input_bus_fmts = syscon_bridge_get_input_bus_fmts,
	.atomic_check		   = syscon_bridge_atomic_check,
	.attach			   = syscon_bridge_attach,
	.disable		   = syscon_bridge_disable,
	.post_disable		   = syscon_bridge_post_disable,
	.mode_set		   = syscon_bridge_mode_set,
};

static int sbridge_parse_dt(struct device *dev)
{
	struct syscon_bridge *sbridge = dev_get_drvdata(dev);
	int ret			      = 0;

	sbridge->dss_regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "verisilicon,syscon");
	if (IS_ERR(sbridge->dss_regmap)) {
		ret = PTR_ERR(sbridge->dss_regmap);
		dev_err(dev, "failed to get dss-syscon with %d\n", ret);

		sbridge->dss_regmap = NULL;
		return ret;
	}

	return ret;
}

static int syscon_bridge_probe(struct platform_device *pdev)
{
	struct syscon_bridge *sbridge;
	struct device *dev = &pdev->dev;
	int ret;

	sbridge = devm_kzalloc(dev, sizeof(*sbridge), GFP_KERNEL);
	if (!sbridge)
		return -ENOMEM;

	platform_set_drvdata(pdev, sbridge);

	ret = sbridge_parse_dt(dev);
	if (ret)
		return ret;

	/*
	 * Get the next bridge in the pipeline. If no next bridge, just skipping
	 * get next bridge.
	 */
	sbridge->next_bridge = devm_drm_of_get_bridge(dev, dev->of_node, 1, 0);
	if (IS_ERR(sbridge->next_bridge)) {
		ret		     = PTR_ERR(sbridge->next_bridge);
		sbridge->next_bridge = NULL;

		if (ret != -ENODEV)
			return ret;
	}

	/* Register the bridge. */
	sbridge->bridge.funcs	= &syscon_bridge_bridge_funcs;
	sbridge->bridge.of_node = pdev->dev.of_node;

	drm_bridge_add(&sbridge->bridge);

	sbridge->priv = of_device_get_match_data(dev);

	return 0;
}

static int syscon_bridge_remove(struct platform_device *pdev)
{
	struct syscon_bridge *sbridge = platform_get_drvdata(pdev);

	drm_bridge_remove(&sbridge->bridge);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id syscon_bridge_match[] = {
	{.compatible = "verisilicon,dsi-syscon-bridge", .data = &dsi_subsys},
	{.compatible = "verisilicon,csi-syscon-bridge", .data = &csi_subsys},
	{.compatible = "verisilicon,bt1120-syscon-bridge", .data = &bt1120_subsys},
	{.compatible = "verisilicon,dc-wb-syscon-bridge", .data = &dc_wb_subsys},
	{.compatible = "verisilicon,bt1120-wb-syscon-bridge", .data = &bt1120_wb_subsys},
	{},
};
MODULE_DEVICE_TABLE(of, syscon_bridge_match);

static struct platform_driver vs_syscon_bridge_driver = {
	.probe	= syscon_bridge_probe,
	.remove = syscon_bridge_remove,
	.driver =
		{
			.name		= "vs-syscon-bridge",
			.of_match_table = syscon_bridge_match,
		},
};
module_platform_driver(vs_syscon_bridge_driver);

MODULE_DESCRIPTION("Syscon DRM bridge driver");
MODULE_LICENSE("GPL");
