// SPDX-License-Identifier: GPL-2.0

#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/media-bus-format.h>

#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_panel.h>

enum dw_bus_fmt {
	DW_BUS_FMT_RGB565_1 = 0,
	DW_BUS_FMT_RGB565_2,
	DW_BUS_FMT_RGB565_3,
	DW_BUS_FMT_RGB666_PACKED,
	DW_BUS_FMT_RGB666,
	DW_BUS_FMT_RGB888,
	DW_BUS_FMT_YUV420,
	DW_BUS_FMT_YUV422,
	DW_BUS_FMT_RAW8,
	DW_BUS_FMT_RAW10,
	DW_BUS_FMT_COUNT,
};

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;
	const struct display_timing *timings;
	unsigned int num_timings;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

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

struct panel_simple_csi {
	struct drm_panel base;
	struct device *dev;

	bool prepared;
	bool enabled;

	const struct panel_desc *desc;

	struct drm_display_mode override_mode;

	enum drm_panel_orientation orientation;
};

struct panel_desc_csi {
	struct panel_desc desc;

	unsigned long flags;
	u32 format;
	unsigned int lanes;
};

static const struct drm_display_mode csi_panel_modes[] = {
	{
		.clock	     = 148500,
		.hdisplay    = 1920,
		.hsync_start = 2008,
		.hsync_end   = 2052,
		.htotal	     = 2200,
		.vdisplay    = 1080,
		.vsync_start = 1084,
		.vsync_end   = 1089,
		.vtotal	     = 1125,
		.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
	},
	{
		.clock	     = 74250,
		.hdisplay    = 1280,
		.hsync_start = 1390,
		.hsync_end   = 1430,
		.htotal	     = 1650,
		.vdisplay    = 720,
		.vsync_start = 725,
		.vsync_end   = 730,
		.vtotal	     = 750,
		.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
	},
	{
		.clock	     = 40000,
		.hdisplay    = 800,
		.hsync_start = 840,
		.hsync_end   = 968,
		.htotal	     = 1056,
		.vdisplay    = 600,
		.vsync_start = 601,
		.vsync_end   = 605,
		.vtotal	     = 628,
		.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
	},
	{
		.clock	     = 25200,
		.hdisplay    = 640,
		.hsync_start = 656,
		.hsync_end   = 752,
		.htotal	     = 800,
		.vdisplay    = 480,
		.vsync_start = 490,
		.vsync_end   = 492,
		.vtotal	     = 525,
		.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
	},
	{
		.clock	     = 297000,
		.hdisplay    = 3840,
		.hsync_start = 4016,
		.hsync_end   = 4104,
		.htotal	     = 4400,
		.vdisplay    = 2160,
		.vsync_start = 2168,
		.vsync_end   = 2178,
		.vtotal	     = 2250,
		.flags	     = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
	},
	{
		.clock	     = 115711,
		.hdisplay    = 2560,
		.hsync_start = 2568,
		.hsync_end   = 2600,
		.htotal	     = 2640,
		.vdisplay    = 1440,
		.vsync_start = 1447,
		.vsync_end   = 1455,
		.vtotal	     = 1461,
		.flags	     = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC,
	},
	{
		.clock	     = 234590,
		.hdisplay    = 2560,
		.hsync_start = 2568,
		.hsync_end   = 2600,
		.htotal	     = 2640,
		.vdisplay    = 1440,
		.vsync_start = 1467,
		.vsync_end   = 1475,
		.vtotal	     = 1481,
		.flags	     = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC,
	},
	{
		.clock	     = 120000,
		.hdisplay    = 2560,
		.hsync_start = 2608,
		.hsync_end   = 2640,
		.htotal	     = 2720,
		.vdisplay    = 1440,
		.vsync_start = 1443,
		.vsync_end   = 1448,
		.vtotal	     = 1481,
		.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
	},
	{
		.clock	     = 241500,
		.hdisplay    = 2560,
		.hsync_start = 2608,
		.hsync_end   = 2640,
		.htotal	     = 2720,
		.vdisplay    = 1440,
		.vsync_start = 1443,
		.vsync_end   = 1448,
		.vtotal	     = 1481,
		.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
	},
	{
		.clock	     = 65280, // 2040 * 1066 * 30 = 65.2 MHz
		.hdisplay    = 1472,
		.hsync_start = 1472 + 32, // 1504
		.hsync_end   = 1472 + 96, // 1568
		.htotal	     = 2040,
		.vdisplay    = 1056,
		.vsync_start = 1056 + 2,
		.vsync_end   = 1056 + 6,
		.vtotal	     = 1066, // 10
		.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
	}

};

static const struct panel_desc_csi csi_panel = {
	.desc =
		{
			.modes	   = csi_panel_modes,
			.num_modes = ARRAY_SIZE(csi_panel_modes),
			.bpc	    = 8,
			.size =
				{
					.width	= 217,
					.height = 136,
				},
			.connector_type = DRM_MODE_CONNECTOR_VIRTUAL,
			.bus_format	    = MEDIA_BUS_FMT_YUYV8_1X16,
		},
		.format  = MEDIA_BUS_FMT_YUYV8_1X16,
	.lanes	= 4,
};

static inline struct panel_simple_csi *to_panel_simple(struct drm_panel *panel)
{
	return container_of(panel, struct panel_simple_csi, base);
}

static unsigned int panel_simple_get_timings_modes(struct panel_simple_csi *panel,
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

static unsigned int panel_simple_get_display_modes(struct panel_simple_csi *panel,
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

static int panel_simple_get_non_edid_modes(struct panel_simple_csi *panel,
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
	struct panel_simple_csi *p = to_panel_simple(panel);

	if (!p->enabled)
		return 0;

	p->enabled = false;

	return 0;
}

static int panel_simple_unprepare(struct drm_panel *panel)
{
	//do nothing,return.
	return 0;
}

static int panel_simple_prepare(struct drm_panel *panel)
{
	//do nothing,return.
	return 0;
}

static int panel_simple_enable(struct drm_panel *panel)
{
	//do nothing,return.
	return 0;
}

static int panel_simple_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct panel_simple_csi *p = to_panel_simple(panel);
	int num			   = 0;

	/* add hard-coded panel modes */
	num += panel_simple_get_non_edid_modes(p, connector);

	/* set up connector's "panel orientation" property */
	drm_connector_set_panel_orientation(connector, p->orientation);

	return num;
}

static int panel_simple_get_timings(struct drm_panel *panel, unsigned int num_timings,
				    struct display_timing *timings)
{
	struct panel_simple_csi *p = to_panel_simple(panel);
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
	struct panel_simple_csi *panel;
	int connector_type;
	int err;

	panel = devm_kzalloc(dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	panel->enabled	= false;
	panel->prepared = false;
	panel->desc	= desc;
	panel->dev	= dev;

	err = of_drm_get_panel_orientation(dev->of_node, &panel->orientation);
	if (err) {
		dev_err(dev, "%pOF: failed to get orientation %d\n", dev->of_node, err);
		return err;
	}

	connector_type = desc->connector_type;

	dev_set_drvdata(dev, panel);
	pr_err("INIT with %d\n", panel->desc->connector_type);
	drm_panel_init(&panel->base, dev, &panel_simple_funcs, panel->desc->connector_type);

	drm_panel_add(&panel->base);

	return 0;
}

static const struct of_device_id csi_of_match[] = {{.compatible = "csi-panel", .data = &csi_panel},
						   {
							   /* sentinel */
						   }};
MODULE_DEVICE_TABLE(of, csi_of_match);

static int panel_simple_csi_probe(struct platform_device *pdev)
{
	const struct panel_desc_csi *desc;
	const struct of_device_id *id;
	int err;
	pr_err("PROBE START!\n");
	id = of_match_node(csi_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	desc = id->data;

	err = panel_simple_probe(&pdev->dev, &desc->desc);
	if (err < 0)
		return err;

	return err;
}

static int panel_simple_csi_remove(struct platform_device *pdev)
{
	struct panel_simple_csi *panel = dev_get_drvdata(&pdev->dev);

	if (!panel)
		return 0;
	drm_panel_remove(&panel->base);
	drm_panel_disable(&panel->base);
	drm_panel_unprepare(&panel->base);
	return 0;
}

static struct platform_driver panel_simple_csi_driver = {
	.driver =
		{
			.name		= "panel-drobot-csi",
			.of_match_table = csi_of_match,
		},
	.probe	= panel_simple_csi_probe,
	.remove = panel_simple_csi_remove,
};
module_platform_driver(panel_simple_csi_driver);

MODULE_DESCRIPTION("DRM Driver for drobot-csi panel");
MODULE_AUTHOR("jiale01.luo <jiale01.luo@d-robotics.cc>");
MODULE_LICENSE("GPL");
