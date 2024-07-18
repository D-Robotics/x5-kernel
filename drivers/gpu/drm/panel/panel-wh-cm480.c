// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 *
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

/**
 * @modes: Pointer to array of fixed modes appropriate for this panel.  If
 *         only one mode then this can just be the address of this the mode.
 *         NOTE: cannot be used with "timings" and also if this is specified
 *         then you cannot override the mode in the device tree.
 * @num_modes: Number of elements in modes array.
 * @timings: Pointer to array of display timings.  NOTE: cannot be used with
 *           "modes" and also these will be used to validate a device tree
 *           override if one is present.
 * @num_timings: Number of elements in timings array.
 * @bpc: Bits per color.
 * @size: Structure containing the physical size of this panel.
 * @delay: Structure containing various delay values for this panel.
 * @bus_format: See MEDIA_BUS_FMT_... defines.
 * @bus_flags: See DRM_BUS_FLAG_... defines.
 */
struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;
	const struct display_timing *timings;
	unsigned int num_timings;

	unsigned int bpc;

	/**
	 * @width: width (in millimeters) of the panel's active display area
	 * @height: height (in millimeters) of the panel's active display area
	 */
	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *           become ready and start receiving video data
	 * @hpd_absent_delay: Add this to the prepare delay if we know Hot
	 *                    Plug Detect isn't used.
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int hpd_absent_delay;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;

	u32 bus_format;
	u32 bus_flags;
	int connector_type;
};

struct panel_simple {
	struct drm_panel base;
	bool prepared;
	bool enabled;

	const struct panel_desc *desc;

	struct regulator *supply;

	struct drm_display_mode override_mode;

	enum drm_panel_orientation orientation;
};

static inline struct panel_simple *to_panel_simple(struct drm_panel *panel)
{
	return container_of(panel, struct panel_simple, base);
}

static unsigned int panel_simple_get_timings_modes(struct panel_simple *panel,
						   struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	unsigned int i, num = 0;

	for (i = 0; i < panel->desc->num_timings; i++) {
		const struct display_timing *dt = &panel->desc->timings[i];
		struct videomode vm;

		videomode_from_timing(dt, &vm);
		mode = drm_mode_create(connector->dev);
		if (!mode) {
			dev_err(panel->base.dev, "failed to add mode %ux%u\n", dt->hactive.typ,
				dt->vactive.typ);
			continue;
		}

		drm_display_mode_from_videomode(&vm, mode);

		mode->type |= DRM_MODE_TYPE_DRIVER;

		if (panel->desc->num_timings == 1)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_probed_add(connector, mode);
		num++;
	}

	return num;
}

static unsigned int panel_simple_get_display_modes(struct panel_simple *panel,
						   struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	unsigned int i, num = 0;

	for (i = 0; i < panel->desc->num_modes; i++) {
		const struct drm_display_mode *m = &panel->desc->modes[i];

		mode = drm_mode_duplicate(connector->dev, m);
		if (!mode) {
			dev_err(panel->base.dev, "failed to add mode %ux%u@%u\n", m->hdisplay,
				m->vdisplay, drm_mode_vrefresh(m));
			continue;
		}

		mode->type |= DRM_MODE_TYPE_DRIVER;

		if (panel->desc->num_modes == 1)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_set_name(mode);

		drm_mode_probed_add(connector, mode);
		num++;
	}

	return num;
}

static int panel_simple_get_non_edid_modes(struct panel_simple *panel,
					   struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	bool has_override = panel->override_mode.type;
	unsigned int num  = 0;

	if (!panel->desc)
		return 0;

	if (has_override) {
		mode = drm_mode_duplicate(connector->dev, &panel->override_mode);
		if (mode) {
			drm_mode_probed_add(connector, mode);
			num = 1;
		} else {
			dev_err(panel->base.dev, "failed to add override mode\n");
		}
	}

	/* Only add timings if override was not there or failed to validate */
	if (num == 0 && panel->desc->num_timings)
		num = panel_simple_get_timings_modes(panel, connector);

	/*
	 * Only add fixed modes if timings/override added no mode.
	 *
	 * We should only ever have either the display timings specified
	 * or a fixed mode. Anything else is rather bogus.
	 */
	WARN_ON(panel->desc->num_timings && panel->desc->num_modes);
	if (num == 0)
		num = panel_simple_get_display_modes(panel, connector);

	connector->display_info.bpc	  = panel->desc->bpc;
	connector->display_info.width_mm  = panel->desc->size.width;
	connector->display_info.height_mm = panel->desc->size.height;
	if (panel->desc->bus_format)
		drm_display_info_set_bus_formats(&connector->display_info, &panel->desc->bus_format,
						 1);
	connector->display_info.bus_flags = panel->desc->bus_flags;

	return num;
}

static int panel_simple_disable(struct drm_panel *panel)
{
	struct panel_simple *p = to_panel_simple(panel);

	if (!p->enabled)
		return 0;

	if (p->desc->delay.disable)
		msleep(p->desc->delay.disable);

	p->enabled = false;

	return 0;
}

static int panel_simple_unprepare(struct drm_panel *panel)
{
	struct panel_simple *p = to_panel_simple(panel);

	if (!p->prepared)
		return 0;

	regulator_disable(p->supply);
	msleep(10);

	if (p->desc->delay.unprepare)
		msleep(p->desc->delay.unprepare);

	p->prepared = false;

	return 0;
}

#define dsi_dcs_write_seq(dsi, seq...)                            \
	do {                                                      \
		static const u8 d[] = {seq};                      \
		mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d)); \
	} while (0)

static int panel_simple_dsi_init(struct mipi_dsi_device *dsi)
{
	return 0;
}

static int panel_simple_prepare(struct drm_panel *panel)
{
	struct panel_simple *p	    = to_panel_simple(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(panel->dev);
	unsigned int delay;
	int err;

	if (p->prepared)
		return 0;

	err = regulator_enable(p->supply);
	if (err < 0) {
		dev_err(panel->dev, "failed to enable supply: %d\n", err);
		return err;
	}

	panel_simple_dsi_init(dsi);

	delay = p->desc->delay.prepare;
	if (delay)
		msleep(delay);

	p->prepared = true;

	return 0;
}

static int panel_simple_enable(struct drm_panel *panel)
{
	struct panel_simple *p = to_panel_simple(panel);

	if (p->enabled)
		return 0;

	if (p->desc->delay.enable)
		msleep(p->desc->delay.enable);

	p->enabled = true;

	return 0;
}

static int panel_simple_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct panel_simple *p = to_panel_simple(panel);
	int num		       = 0;

	/* add hard-coded panel modes */
	num += panel_simple_get_non_edid_modes(p, connector);

	/* set up connector's "panel orientation" property */
	drm_connector_set_panel_orientation(connector, p->orientation);

	return num;
}

static int panel_simple_get_timings(struct drm_panel *panel, unsigned int num_timings,
				    struct display_timing *timings)
{
	struct panel_simple *p = to_panel_simple(panel);
	unsigned int i;

	if (p->desc->num_timings < num_timings)
		num_timings = p->desc->num_timings;

	if (timings)
		for (i = 0; i < num_timings; i++)
			timings[i] = p->desc->timings[i];

	return p->desc->num_timings;
}

static const struct drm_panel_funcs panel_simple_funcs = {
	.disable     = panel_simple_disable,
	.unprepare   = panel_simple_unprepare,
	.prepare     = panel_simple_prepare,
	.enable	     = panel_simple_enable,
	.get_modes   = panel_simple_get_modes,
	.get_timings = panel_simple_get_timings,
};

static int panel_simple_probe(struct device *dev, const struct panel_desc *desc)
{
	struct panel_simple *panel;
	int connector_type;
	int err;
printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	panel = devm_kzalloc(dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;
printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	panel->enabled	= false;
	panel->prepared = false;
	panel->desc	= desc;
printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	panel->supply = devm_regulator_get(dev, "power");
	if (IS_ERR(panel->supply))
		return PTR_ERR(panel->supply);
printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	err = of_drm_get_panel_orientation(dev->of_node, &panel->orientation);
	if (err) {
		dev_err(dev, "%pOF: failed to get orientation %d\n", dev->of_node, err);
		return err;
	}
printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	connector_type = desc->connector_type;
	/* Catch common mistakes for panels. */
	switch (connector_type) {
	case DRM_MODE_CONNECTOR_DSI:
		if (desc->bpc != 6 && desc->bpc != 8)
			dev_warn(dev, "Expected bpc in {6,8} but got: %u\n", desc->bpc);
		break;
	default:
		dev_warn(dev, "Specify a valid connector_type: %d\n", desc->connector_type);
		connector_type = DRM_MODE_CONNECTOR_DSI;
		break;
	}
printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	drm_panel_init(&panel->base, dev, &panel_simple_funcs, connector_type);
printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	err = drm_panel_of_backlight(&panel->base);
	if (err)
		return err;
printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	drm_panel_add(&panel->base);
printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	dev_set_drvdata(dev, panel);

	return 0;
}

static void panel_simple_remove(struct device *dev)
{
	struct panel_simple *panel = dev_get_drvdata(dev);

	drm_panel_remove(&panel->base);
	drm_panel_disable(&panel->base);
	drm_panel_unprepare(&panel->base);
}

static void panel_simple_shutdown(struct device *dev)
{
	struct panel_simple *panel = dev_get_drvdata(dev);

	drm_panel_disable(&panel->base);
	drm_panel_unprepare(&panel->base);
}

struct panel_desc_dsi {
	struct panel_desc desc;

	unsigned long flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
};

/** Another set of available timings:
static const struct drm_display_mode jc_050hd134_mode = {
	.clock	     = 63333,
	.hdisplay    = 720,
	.hsync_start = 720 + 35,
	.hsync_end   = 720 + 35 + 5,
	.htotal	     = 720 + 35 + 5 + 35,
	.vdisplay    = 1280,
	.vsync_start = 1280 + 16,
	.vsync_end   = 1280 + 16 + 3,
	.vtotal	     = 1280 + 16 + 3 + 11,
	.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};
**/
static const struct drm_display_mode wh_cm480_mode = {
	.clock	     = 29070,
	.hdisplay    = 800,
	.hsync_start = 800 + 90,
	.hsync_end   = 800 + 90 + 2,
	.htotal	     = 800 + 90 + 2 + 58,
	.vdisplay    = 480,
	.vsync_start = 480 + 7,
	.vsync_end   = 480 + 7 + 2,
	.vtotal	     = 480 + 7 + 2 + 21,
	.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};

static const struct panel_desc_dsi wh_cm480 = {
	.desc =
		{
			.modes	   = &wh_cm480_mode,
			.num_modes = 1,
			.bpc	   = 8,
			.size =
				{
					.width	= 800,
					.height = 480,
				},
			.connector_type = DRM_MODE_CONNECTOR_DSI,
		},
	.flags	= MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE,
	.format = MIPI_DSI_FMT_RGB888,
	.lanes	= 1,
};

static const struct of_device_id dsi_of_match[] = {
	{.compatible = "wh-cm480", .data = &wh_cm480},
	{
		/* sentinel */
	}};
MODULE_DEVICE_TABLE(of, dsi_of_match);

static int panel_simple_dsi_probe(struct mipi_dsi_device *dsi)
{
	const struct panel_desc_dsi *desc;
	const struct of_device_id *id;
	int err;

	id = of_match_node(dsi_of_match, dsi->dev.of_node);
	if (!id)
		return -ENODEV;

	desc = id->data;

	err = panel_simple_probe(&dsi->dev, &desc->desc);
	if (err < 0)
		return err;

	dsi->mode_flags = desc->flags;
	dsi->format	= desc->format;
	dsi->lanes	= desc->lanes;

	err = mipi_dsi_attach(dsi);
	if (err) {
		struct panel_simple *panel = dev_get_drvdata(&dsi->dev);

		drm_panel_remove(&panel->base);
	}

	return err;
}

static void panel_simple_dsi_remove(struct mipi_dsi_device *dsi)
{
	int err;

	if (dsi->attached) {
		err = mipi_dsi_detach(dsi);
		if (err < 0)
			dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", err);
	}

	panel_simple_remove(&dsi->dev);
}

static void panel_simple_dsi_shutdown(struct mipi_dsi_device *dsi)
{
	panel_simple_shutdown(&dsi->dev);
}

static struct mipi_dsi_driver panel_simple_dsi_driver = {
	.driver =
		{
			.name		= "panel-wh-cm480",
			.of_match_table = dsi_of_match,
		},
	.probe	  = panel_simple_dsi_probe,
	.remove	  = panel_simple_dsi_remove,
	.shutdown = panel_simple_dsi_shutdown,
};

static int __init panel_simple_init(void)
{
	return mipi_dsi_driver_register(&panel_simple_dsi_driver);
}
module_init(panel_simple_init);

static void __exit panel_simple_exit(void)
{
	mipi_dsi_driver_unregister(&panel_simple_dsi_driver);
}
module_exit(panel_simple_exit);

MODULE_AUTHOR("jiale luo <jiale01.luo@horizon.cc>");
MODULE_DESCRIPTION("DRM Driver for wh-cm480 panel");
MODULE_LICENSE("GPL");
