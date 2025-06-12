// SPDX-License-Identifier: GPL-2.0
/*
 * DRM Bridge layer for DW MIPI CSI
 */

#include <drm/drm_bridge.h>
#include <drm/drm_connector.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_print.h>
#include <linux/media-bus-format.h>

#include "dw_mipi_csi.h"
#include "dw_mipi_csi_hal.h"
#include "dw_mipi_csi_drm.h"

/* Forward declarations */
static const struct drm_bridge_funcs csi_bridge_funcs;

#define CSI_DEFAULT_BUS_FMT MEDIA_BUS_FMT_RGB888_1X24

static enum dw_bus_fmt media_bus_fmt_to_dw_bus_fmt(u32 bus_fmt)
{
	switch (bus_fmt) {
	case MEDIA_BUS_FMT_RGB565_1X16:
		return DW_BUS_FMT_RGB565_1;
	case MEDIA_BUS_FMT_RGB666_1X18:
		return DW_BUS_FMT_RGB666_PACKED;
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
		return DW_BUS_FMT_RGB666;
	case MEDIA_BUS_FMT_RGB888_1X24:
		return DW_BUS_FMT_RGB888;
	case MEDIA_BUS_FMT_UYVY8_1X16:
		return DW_BUS_FMT_YUV422;
	case MEDIA_BUS_FMT_YUYV8_1X16:
		return DW_BUS_FMT_YUV422;
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return DW_BUS_FMT_RAW10;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return DW_BUS_FMT_RAW8;
	default:
		/* Default to RGB888 if unknown */
		return DW_BUS_FMT_RGB888;
	}
}

/*
 * csi_bridge_atomic_get_input_bus_fmts - optional callback to define
 * what bus fmts this bridge accepts, given the upstream's output_fmt.
 */
static u32 *csi_bridge_atomic_get_input_bus_fmts(struct drm_bridge *bridge,
						 struct drm_bridge_state *bridge_state,
						 struct drm_crtc_state *crtc_state,
						 struct drm_connector_state *conn_state,
						 u32 output_fmt, unsigned int *num_input_fmts)
{
	u32 *fmts = NULL;

	/* Just pass the same format from upstream if not MEDIA_BUS_FMT_FIXED */
	fmts = kmalloc(sizeof(*fmts), GFP_KERNEL);
	if (!fmts) {
		*num_input_fmts = 0;
		return NULL;
	}

	*num_input_fmts = 1;

	/* if upstream is 'fixed', fallback to default, else pass it through */
	if (output_fmt == MEDIA_BUS_FMT_FIXED)
		fmts[0] = CSI_DEFAULT_BUS_FMT;
	else
		fmts[0] = output_fmt;

	return fmts;
}

/*
 * csi_bridge_atomic_check - can do any checks or store data for later usage.
 * For example, we might store csi->bus_fmt here if we want, or do nothing.
 */
static int csi_bridge_atomic_check(struct drm_bridge *bridge, struct drm_bridge_state *bridge_state,
				   struct drm_crtc_state *crtc_state,
				   struct drm_connector_state *conn_state)
{
	struct dw_mipi_csi *csi = bridge_to_csi(bridge);
	/*
	 * We could store bridge_state->input_bus_cfg.format in csi->bus_fmt
	 * here. But typically we do it in .mode_set or .atomic_enable.
	 */
	DRM_DEV_DEBUG(csi->dev, "atomic_check called\n");
	return 0;
}

/*
 * csi_bridge_mode_set - old style callback for setting mode
 * if we want to do it in .atomic_enable, that is also possible.
 */
static void csi_bridge_mode_set(struct drm_bridge *bridge, const struct drm_display_mode *mode,
				const struct drm_display_mode *adj)
{
	struct dw_mipi_csi *csi = bridge_to_csi(bridge);
	struct drm_bridge_state *bridge_state;
	u32 bus_fmt = 0;
	int ret	    = 0;

	if (!csi || !adj)
		return;

	/* Retrieve bus_fmt from atomic bridging. */
	bridge_state = drm_priv_to_bridge_state(bridge->base.state);
	if (!bridge_state) {
		DRM_DEV_ERROR(csi->dev, "No valid bridge_state\n");
		return;
	}

	bus_fmt = bridge_state->input_bus_cfg.format;
	if (bus_fmt == MEDIA_BUS_FMT_FIXED)
		bus_fmt = CSI_DEFAULT_BUS_FMT;

	/* Convert to internal dw_bus_fmt */
	csi->bus_fmt = media_bus_fmt_to_dw_bus_fmt(bus_fmt);

	DRM_DEV_DEBUG(csi->dev, "mode_set: bus_fmt=0x%x, w=%d,h=%d\n", bus_fmt, adj->hdisplay,
		      adj->vdisplay);

	/* Fill in csi->mode */
	csi->mode.width	       = adj->hdisplay;
	csi->mode.height       = adj->vdisplay;
	csi->mode.pixel_clock  = adj->clock;
	csi->mode.refresh_rate = drm_mode_vrefresh(adj);
	csi->mode.interlaced   = !!(adj->flags & DRM_MODE_FLAG_INTERLACE);
	csi->mode.hdisplay     = adj->hdisplay;
	csi->mode.hsync_start  = adj->hsync_start;
	csi->mode.hsync_end    = adj->hsync_end;
	csi->mode.htotal       = adj->htotal;
	csi->mode.vdisplay     = adj->vdisplay;
	csi->mode.vsync_start  = adj->vsync_start;
	csi->mode.vsync_end    = adj->vsync_end;
	csi->mode.vtotal       = adj->vtotal;

	/*
	 * Now call HAL init and set_mode
	 */
	ret = dw_mipi_csi_hal_init(csi, CSI_VPG_OFF);
	if (ret) {
		DRM_DEV_ERROR(csi->dev, "hal_init fail ret=%d\n", ret);
		return;
	}

	dw_mipi_csi_hal_set_mode(csi, &csi->mode);
}

static void csi_bridge_post_disable(struct drm_bridge *bridge)
{
	struct dw_mipi_csi *csi = bridge_to_csi(bridge);

	dw_mipi_csi_hal_disable(csi);
}

static int csi_bridge_attach(struct drm_bridge *bridge, enum drm_bridge_attach_flags flags)
{
	struct dw_mipi_csi *csi = bridge_to_csi(bridge);
	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found\n");
		return -ENODEV;
	}

	/* Attach the panel-bridge to the csi bridge */
	return drm_bridge_attach(bridge->encoder, csi->panel_bridge, bridge, flags);
}

static void csi_bridge_detach(struct drm_bridge *bridge)
{
	//nothing to do, return.
	return;
}

static const struct drm_bridge_funcs csi_bridge_funcs = {

	.atomic_reset		   = drm_atomic_helper_bridge_reset,
	.atomic_duplicate_state	   = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state	   = drm_atomic_helper_bridge_destroy_state,
	.atomic_get_input_bus_fmts = csi_bridge_atomic_get_input_bus_fmts,
	.atomic_check		   = csi_bridge_atomic_check,

	.attach	      = csi_bridge_attach,
	.detach	      = csi_bridge_detach,
	.mode_set     = csi_bridge_mode_set,
	.post_disable = csi_bridge_post_disable,
};

int dw_mipi_csi_drm_register(struct dw_mipi_csi *csi)
{
	if (!csi || !csi->dev) {
		DRM_ERROR("dw_mipi_csi: Invalid CSI or device pointer\n");
		return -EINVAL;
	}

	csi->bridge.funcs   = &csi_bridge_funcs;
	csi->bridge.of_node = csi->dev->of_node;

	drm_bridge_add(&csi->bridge);

	return 0;
}

int dw_mipi_csi_drm_bind(struct dw_mipi_csi *csi)
{
	struct drm_bridge *bridge;

	if (!csi || !csi->dev) {
		DRM_ERROR("dw_mipi_csi: Invalid CSI or device pointer\n");
		return -EINVAL;
	}

	bridge = devm_drm_of_get_bridge(csi->dev, csi->dev->of_node, 1, 0);
	if (IS_ERR(bridge)) {
		DRM_ERROR("Failed to bind panel bridge: %ld\n", PTR_ERR(bridge));
		return PTR_ERR(bridge);
	}

	csi->panel_bridge = bridge;

	return 0;
}

void dw_mipi_csi_drm_unregister(struct dw_mipi_csi *csi)
{
	drm_bridge_remove(&csi->bridge);
}
