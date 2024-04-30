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

	struct gpio_desc *reset_gpio;

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
	gpiod_set_value_cansleep(p->reset_gpio, 0);

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
	dsi_dcs_write_seq(dsi, 0xb9, 0xF1, 0x12, 0x83);
	dsi_dcs_write_seq(dsi, 0xBA, 0x33, 0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00, 0x91, 0x0A, 0x00, 0x00, 0x02,
			  0x4F, 0xD1, 0x00, 0x00, 0x37);
	dsi_dcs_write_seq(dsi, 0xB8, 0x26);
	dsi_dcs_write_seq(dsi, 0xBF, 0x02, 0x10, 0x00);
	dsi_dcs_write_seq(dsi, 0xB3, 0x07, 0x0B, 0x1E, 0x1E, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xC0, 0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x08, 0x70, 0x00);
	dsi_dcs_write_seq(dsi, 0xBC, 0x46);
	dsi_dcs_write_seq(dsi, 0xCC, 0x0B);
	dsi_dcs_write_seq(dsi, 0xB4, 0x80);
	dsi_dcs_write_seq(dsi, 0xB2, 0xC8, 0x12, 0xA0);
	dsi_dcs_write_seq(dsi, 0xE3, 0x07, 0x07, 0x0B, 0x0B, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00,
			  0xFF, 0x80, 0xC0, 0x10);
	dsi_dcs_write_seq(dsi, 0xC1, 0x53, 0x00, 0x32, 0x32, 0x77, 0xF1, 0xFF, 0xFF, 0xCC, 0xCC,
			  0x77, 0x77);
	dsi_dcs_write_seq(dsi, 0xB5, 0x09, 0x09);

	dsi_dcs_write_seq(dsi, 0xB6, 0xB7, 0xB7);
	dsi_dcs_write_seq(dsi, 0xE9, 0xC2, 0x10, 0x0A, 0x00, 0x00, 0x81, 0x80, 0x12, 0x30, 0x00,
			  0x37, 0x86, 0x81, 0x80, 0x37, 0x18, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xBA, 0x46, 0x02, 0x08, 0x28,
			  0x88, 0x88, 0x88, 0x88, 0x88, 0xF8, 0xBA, 0x57, 0x13, 0x18, 0x38, 0x88,
			  0x88, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00);

	dsi_dcs_write_seq(dsi, 0xEA, 0x07, 0x12, 0x01, 0x01, 0x02, 0x3C, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x8F, 0xBA, 0x31, 0x75, 0x38, 0x18, 0x88, 0x88, 0x88, 0x88,
			  0x88, 0x8F, 0xBA, 0x20, 0x64, 0x28, 0x08, 0x88, 0x88, 0x88, 0x88, 0x88,
			  0x23, 0x10, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00);

	dsi_dcs_write_seq(dsi, 0xE0, 0x00, 0x02, 0x04, 0x1A, 0x23, 0x3F, 0x2C, 0x28, 0x05, 0x09,
			  0x0B, 0x10, 0x11, 0x10, 0x12, 0x12, 0x19, 0x00, 0x02, 0x04, 0x1A, 0x23,
			  0x3F, 0x2C, 0x28, 0x05, 0x09, 0x0B, 0x10, 0x11, 0x10, 0x12, 0x12, 0x19);

	dsi_dcs_write_seq(dsi, 0x11);
	msleep(250);
	dsi_dcs_write_seq(dsi, 0x29);
	msleep(50);

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

	gpiod_set_value_cansleep(p->reset_gpio, 1);
	msleep(2);
	gpiod_set_value_cansleep(p->reset_gpio, 0);
	msleep(2);
	gpiod_set_value_cansleep(p->reset_gpio, 1);
	msleep(25);
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

	panel = devm_kzalloc(dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	panel->enabled	= false;
	panel->prepared = false;
	panel->desc	= desc;

	panel->supply = devm_regulator_get(dev, "power");
	if (IS_ERR(panel->supply))
		return PTR_ERR(panel->supply);

	panel->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(panel->reset_gpio)) {
		err = PTR_ERR(panel->reset_gpio);
		if (err != -EPROBE_DEFER)
			dev_err(dev, "failed to request GPIO: %d\n", err);
		return err;
	}

	err = of_drm_get_panel_orientation(dev->of_node, &panel->orientation);
	if (err) {
		dev_err(dev, "%pOF: failed to get orientation %d\n", dev->of_node, err);
		return err;
	}

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

	drm_panel_init(&panel->base, dev, &panel_simple_funcs, connector_type);

	err = drm_panel_of_backlight(&panel->base);
	if (err)
		return err;

	drm_panel_add(&panel->base);

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
static const struct drm_display_mode jc_050hd134_mode = {
	.clock	     = 65000,
	.hdisplay    = 720,
	.hsync_start = 720 + 32,
	.hsync_end   = 720 + 32 + 20,
	.htotal	     = 720 + 32 + 20 + 20,
	.vdisplay    = 1280,
	.vsync_start = 1280 + 20,
	.vsync_end   = 1280 + 20 + 4,
	.vtotal	     = 1280 + 20 + 4 + 20,
	.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};

static const struct panel_desc_dsi jc_050hd134 = {
	.desc =
		{
			.modes	   = &jc_050hd134_mode,
			.num_modes = 1,
			.bpc	   = 8,
			.size =
				{
					.width	= 62,
					.height = 110,
				},
			.connector_type = DRM_MODE_CONNECTOR_DSI,
		},
	.flags	= MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE,
	.format = MIPI_DSI_FMT_RGB888,
	.lanes	= 4,
};

static const struct of_device_id dsi_of_match[] = {
	{.compatible = "jc-050hd134", .data = &jc_050hd134},
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

	err = mipi_dsi_detach(dsi);
	if (err < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", err);

	panel_simple_remove(&dsi->dev);
}

static void panel_simple_dsi_shutdown(struct mipi_dsi_device *dsi)
{
	panel_simple_shutdown(&dsi->dev);
}

static struct mipi_dsi_driver panel_simple_dsi_driver = {
	.driver =
		{
			.name		= "panel-jc-050hd134",
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
MODULE_DESCRIPTION("DRM Driver for jc-050hd134 panel");
MODULE_LICENSE("GPL");
