
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/hdmi.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/mm.h>

// #include <drm/drmP.h>
#include <drm/drm_bridge.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>
#include <drm/drm_drv.h>
#include <video/videomode.h>

//#include "ms7210_mpi.h"
#include "ms7210.h"

enum ms7210_type {
	MS7210 = 0,
};

struct ms7210 {
	struct i2c_client *i2c_main;
	struct gpio_desc *reset_gpio;

	u8 edid_buf[256];
	enum drm_connector_status status;
	struct drm_bridge bridge;
	struct drm_connector connector;
	struct drm_display_mode *mode;
	struct videomode vmode;

	int spdif;
	unsigned int f_tmds;
	unsigned int f_audio;
	unsigned int audio_source;

	DVIN_CONFIG_T g_t_dvin_config;
	VIDEOTIMING_T g_t_hdtx_timing;
	HD_CONFIG_T g_t_hdtx_infoframe;
};

#define PRINT_INT_MEMBER(struct_instance, member) \
    do { \
        printk("%s.%s = %d\n", #struct_instance, #member, (struct_instance)->member); \
    } while(0)

// static const struct regmap_config ms7210_regmap_config = {
// 	.reg_bits = 16,
// 	.val_bits = 8,
// 	.cache_type = REGCACHE_NONE,
// };

static UINT8 u8_sys_edid_default_buf[256] =
{
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x05, 0xe3, 0x03, 0x27, 0x1b, 0x11, 0x01, 0x00,
	0x12, 0x22, 0x01, 0x03, 0x80, 0x3c, 0x22, 0x78, 0x2e, 0x2b, 0x55, 0xa8, 0x53, 0x53, 0x9b, 0x26,
	0x10, 0x50, 0x54, 0xbf, 0xef, 0x00, 0xd1, 0xc0, 0xb3, 0x00, 0x95, 0x00, 0x81, 0x80, 0x81, 0xc0,
	0x31, 0x68, 0x45, 0x68, 0x61, 0x68, 0x6a, 0x5e, 0x00, 0xa0, 0xa0, 0xa0, 0x29, 0x50, 0x30, 0x20,
	0x35, 0x00, 0x55, 0x50, 0x21, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xff, 0x00, 0x31, 0x41, 0x51,
	0x51, 0x34, 0x48, 0x41, 0x30, 0x36, 0x39, 0x39, 0x31, 0x35, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x51,
	0x32, 0x37, 0x42, 0x33, 0x30, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfd,
	0x00, 0x30, 0x64, 0x1e, 0x96, 0x28, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xfb,
	0x02, 0x03, 0x3d, 0xb1, 0x4e, 0x10, 0x1f, 0x05, 0x14, 0x04, 0x13, 0x03, 0x12, 0x02, 0x11, 0x01,
	0x5d, 0x5e, 0x5f, 0x6d, 0x03, 0x0c, 0x00, 0x10, 0x00, 0x38, 0x44, 0x20, 0x00, 0x60, 0x01, 0x02,
	0x03, 0x67, 0xd8, 0x5d, 0xc4, 0x01, 0x78, 0x80, 0x00, 0xe3, 0x05, 0xe3, 0x01, 0xe6, 0x06, 0x07,
	0x01, 0x62, 0x62, 0x00, 0x68, 0x1a, 0x00, 0x00, 0x01, 0x01, 0x30, 0x64, 0xe6, 0x76, 0x9b, 0x00,
	0x78, 0xa0, 0xa0, 0x2d, 0x50, 0x30, 0x20, 0x35, 0x00, 0x55, 0x50, 0x21, 0x00, 0x00, 0x1e, 0x05,
	0x76, 0x00, 0xa0, 0xa0, 0xa0, 0x29, 0x50, 0x30, 0x20, 0x98, 0x04, 0x55, 0x50, 0x21, 0x00, 0x00,
	0x1a, 0xf0, 0x3c, 0x00, 0xd0, 0x51, 0xa0, 0x35, 0x50, 0x60, 0x88, 0x3a, 0x00, 0x55, 0x50, 0x21,
	0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xef,
};

/* Default video timing */
// static VIDEOTIMING_T hdtx_timing_1080p = {0x07,2200,1125,1920,1080,14850,6000,192,41,44,5};
static VIDEOTIMING_T hdtx_timing_4k = {0x07,4400,2250,3840,2160,29700,6000,384,82,88,10};
static uint8_t u8_color_space = HD_YCBCR422;
static uint8_t u8_color_depth = HD_COLOR_DEPTH_8BIT;
static uint8_t u8_colorimetry = HD_COLORIMETRY_709;

/* Default Audio Config */
#define MS7210_AUDIO_DEFAULT_SAMPLE_RATE	HD_AUD_RATE_48K
#define MS7210_AUDIO_DEFAULT_BIT			HD_AUD_LENGTH_16BITS
#define MS7210_AUDIO_DEFAULT_CHN			HD_AUD_2CH
static int ms7210_sample_rate = MS7210_AUDIO_DEFAULT_SAMPLE_RATE;

/* Debug Print */
static void debug_show_drm_display_mode(const struct drm_display_mode *adj_mode)
{
	printk("============== drm_display_mode ================\n");
	printk("adj_mode->name = %s\n", adj_mode->name);
	PRINT_INT_MEMBER(adj_mode, clock);      /* in kHz */
	PRINT_INT_MEMBER(adj_mode, hdisplay);
	PRINT_INT_MEMBER(adj_mode, hsync_start);
	PRINT_INT_MEMBER(adj_mode, hsync_end);
	PRINT_INT_MEMBER(adj_mode, htotal);
	PRINT_INT_MEMBER(adj_mode, hskew);
	PRINT_INT_MEMBER(adj_mode, vdisplay);
	PRINT_INT_MEMBER(adj_mode, vsync_start);
	PRINT_INT_MEMBER(adj_mode, vsync_end);
	PRINT_INT_MEMBER(adj_mode, vtotal);
	PRINT_INT_MEMBER(adj_mode, vscan);

	PRINT_INT_MEMBER(adj_mode, flags);
	PRINT_INT_MEMBER(adj_mode, crtc_clock);
	PRINT_INT_MEMBER(adj_mode, crtc_hdisplay);
	PRINT_INT_MEMBER(adj_mode, crtc_hblank_start);
	PRINT_INT_MEMBER(adj_mode, crtc_hblank_end);
	PRINT_INT_MEMBER(adj_mode, crtc_hsync_start);
	PRINT_INT_MEMBER(adj_mode, crtc_hsync_end);
	PRINT_INT_MEMBER(adj_mode, crtc_htotal);
	PRINT_INT_MEMBER(adj_mode, crtc_hskew);
	PRINT_INT_MEMBER(adj_mode, crtc_vdisplay);
	PRINT_INT_MEMBER(adj_mode, crtc_vblank_start);
	PRINT_INT_MEMBER(adj_mode, crtc_vblank_end);
	PRINT_INT_MEMBER(adj_mode, crtc_vsync_start);
	PRINT_INT_MEMBER(adj_mode, crtc_vsync_end);
	PRINT_INT_MEMBER(adj_mode, crtc_vtotal);
	printk("=================================================\n");
}

static void debug_show_video_mode(struct videomode *vmode)
{
	printk("================== videomode ====================\n");
	printk("vmode->pixelclock = %ld\n", vmode->pixelclock);	/* pixelclock in Hz */
	printk("vmode->hactive = %d\n", vmode->hactive);
	printk("vmode->hfront_porch = %d\n", vmode->hfront_porch);
	printk("vmode->hback_porch = %d\n", vmode->hback_porch);
	printk("vmode->hsync_len = %d\n", vmode->hsync_len);
	printk("vmode->vactive = %d\n", vmode->vactive);
	printk("vmode->vfront_porch = %d\n", vmode->vfront_porch);
	printk("vmode->vback_porch = %d\n", vmode->vback_porch);
	printk("vmode->vsync_len = %d\n", vmode->vsync_len);
	printk("=================================================\n");
}

VOID sys_default_hd_video_config(HD_CONFIG_T *t_hd_infoframe)
{
    t_hd_infoframe->u8_hd_flag = TRUE;
    t_hd_infoframe->u8_vic = 16;
    //t_hd_infoframe->u16_video_clk = 7425;
    t_hd_infoframe->u8_clk_rpt = HD_X1CLK;
    t_hd_infoframe->u8_scan_info = HD_OVERSCAN;  // HD_OVERSCAN, HD_UNDERSCAN
    t_hd_infoframe->u8_aspect_ratio = HD_16X9;
    t_hd_infoframe->u8_color_space = u8_color_space;
    t_hd_infoframe->u8_color_depth = u8_color_depth;
    t_hd_infoframe->u8_colorimetry = u8_colorimetry;
}

VOID sys_default_hd_vendor_specific_config(HD_CONFIG_T *t_hd_infoframe)
{
    t_hd_infoframe->u8_video_format = HD_NO_ADD_FORMAT;
    t_hd_infoframe->u8_4Kx2K_vic = HD_4Kx2K_30HZ;
    t_hd_infoframe->u8_3D_structure = HD_FRAME_PACKING;
}

VOID sys_default_hd_audio_config(HD_CONFIG_T *t_hd_infoframe)
{
    t_hd_infoframe->u8_audio_mode = HD_AUD_MODE_AUDIO_SAMPLE;
    t_hd_infoframe->u8_audio_rate = ms7210_sample_rate;
    t_hd_infoframe->u8_audio_bits = MS7210_AUDIO_DEFAULT_BIT;
    t_hd_infoframe->u8_audio_channels = MS7210_AUDIO_DEFAULT_CHN;
    t_hd_infoframe->u8_audio_speaker_locations = 0;
}

static inline struct ms7210 *bridge_to_ms7210(struct drm_bridge *bridge)
{
	return container_of(bridge, struct ms7210, bridge);
}

static int ms7210_get_edid_block(void *data, u8 *buf, unsigned int block,
	size_t len)
{
	pr_info("%s(): block[%d] len[%ld]\n", __FUNCTION__, block, len);
#if 1
	BOOL b_succ = 0;
	struct ms7210 *ms = (struct ms7210 *)data;

	if (len > 128)
		return -EINVAL;

	if (ms->status == connector_status_connected) {
		b_succ = ms7210_hdtx_edid_get(ms->edid_buf);
		pr_info("ms7210_hdtx_edid_get: ret = %d\n", b_succ);
		if (!b_succ) {
			pr_info("ms7210 get EDID Faild, Use Default EDID\n");
			memcpy(ms->edid_buf, u8_sys_edid_default_buf, 256);
		}
	}

	if (block % 2 == 0)
		memcpy(buf, ms->edid_buf, len);
	else
		memcpy(buf, ms->edid_buf + 128, len);
#else
	// Test: Use Default EDID
	if (block % 2 == 0)
		memcpy(buf, u8_sys_edid_default_buf, len);
	else
		memcpy(buf, u8_sys_edid_default_buf + 128, len);

#endif
	return 0;
}

static struct edid *ms7210_get_edid(struct ms7210 *ms,
	struct drm_connector *connector)
{
	struct edid *edid;
	edid = drm_do_get_edid(connector, ms7210_get_edid_block, ms);
	if (!edid) {
		pr_err("%s(): Not EDID\n", __FUNCTION__);
		return NULL;
	}

	return edid;
}

static struct edid *ms7210_bridge_get_edid(struct drm_bridge *bridge,
	struct drm_connector *connector)
{
	pr_info("%s\n", __FUNCTION__);
	struct ms7210 *ms = bridge_to_ms7210(bridge);
	return ms7210_get_edid(ms, connector);
}

static int ms7210_get_modes(struct ms7210 *ms,
	struct drm_connector *connector)
{
	struct edid *edid;
	unsigned int count;

	edid = drm_do_get_edid(connector, ms7210_get_edid_block, ms);
	if (!edid) {
		pr_err("%s(): Not EDID\n", __FUNCTION__);
		return 0;
	}

	#if 0
	print_hex_dump(KERN_INFO, "EDID buffer: ",
	DUMP_PREFIX_NONE, 16, 1,
	edid, sizeof(struct edid), false);
	#endif

	drm_connector_update_edid_property(connector, edid);
	count = drm_add_edid_modes(connector, edid);
	pr_info("%s(): Update %d edid method\n", __FUNCTION__, count);
	if (edid)
		kfree(edid);

	return count;
}

/* Connector funcs */
static struct ms7210 *connector_to_ms7210(struct drm_connector *connector)
{
	return container_of(connector, struct ms7210, connector);
}

static int ms7210_connector_get_modes(struct drm_connector *connector)
{
	struct ms7210 *ms = connector_to_ms7210(connector);
	return ms7210_get_modes(ms, connector);
}

static enum drm_mode_status ms7210_mode_valid(struct ms7210 *ms,
	struct drm_display_mode *mode)
{
	/* TODO: check mode */
	return MODE_OK;
}

static enum drm_mode_status
ms7210_connector_mode_valid(struct drm_connector *connector,
			     struct drm_display_mode *mode)
{
	struct ms7210 *ms = connector_to_ms7210(connector);

	return ms7210_mode_valid(ms, mode);
}


static struct drm_connector_helper_funcs ms7210_connector_helper_funcs = {
	.get_modes = ms7210_connector_get_modes,
	.mode_valid = ms7210_connector_mode_valid,
};

static enum drm_connector_status
ms7210_connector_detect(struct drm_connector *connector, bool force)
{
	struct ms7210 *ms = connector_to_ms7210(connector);
#if 1
	bool status = 0;
	status = ms7210_hdtx_hpd_detect();
	if (status == TRUE) {
		ms->status = connector_status_connected;
	}
	else {
		ms->status = connector_status_disconnected;
	}
#else
	ms->status = connector_status_connected;
#endif
	pr_debug("ms7210_connector_detect(): hpd status = %d\n", ms->status);

	return ms->status;
}


static const struct drm_connector_funcs ms7210_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = ms7210_connector_detect,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};


/* Bridge funcs */
static void ms7210_bridge_enable(struct drm_bridge *bridge)
{
	pr_info("%s\n", __FUNCTION__);
	struct ms7210 *ms = bridge_to_ms7210(bridge);

	DVIN_TIMING_DET_T t_dvin_timing_det;
	BOOL g_b_tx_input_valid = FALSE;
	g_b_tx_input_valid = ms7210_dvin_timing_get(&t_dvin_timing_det);
#if 0
	printk("t_dvin_timing_det.u16_htotal = %d\n",t_dvin_timing_det.u16_htotal);
	printk("t_dvin_timing_det.u16_vtotal = %d\n",t_dvin_timing_det.u16_vtotal);
	printk("t_dvin_timing_det.u16_hactive = %d\n",t_dvin_timing_det.u16_hactive);
	printk("t_dvin_timing_det.u16_pixclk = %d\n",t_dvin_timing_det.u16_pixclk);
	printk("g_b_tx_input_valid = %d\n", g_b_tx_input_valid);
#endif

	//ms->g_t_hdtx_timing.u16_pixclk = t_dvin_timing_det.u16_pixclk;
	ms7210_dvin_timing_config(&ms->g_t_dvin_config, &ms->g_t_hdtx_timing, &ms->g_t_hdtx_infoframe);
	ms->g_t_hdtx_infoframe.u8_color_space = u8_color_space;
	ms->g_t_hdtx_infoframe.u8_color_depth = u8_color_depth;
	ms->g_t_hdtx_infoframe.u8_audio_rate = ms7210_sample_rate;
	pr_info("[MS7210] Audio Sample rate: %d\n", ms7210_sample_rate);
	ms7210_hdtx_output_config(&ms->g_t_hdtx_infoframe);

}

static void ms7210_bridge_disable(struct drm_bridge *bridge)
{
	pr_info("%s\n", __FUNCTION__);
	// struct ms7210 *ms = bridge_to_ms7210(bridge);
	ms7210_hdtx_shutdown_output();
}

static void ms7210_bridge_mode_set(struct drm_bridge *bridge,
				    const struct drm_display_mode *mode,
				    const struct drm_display_mode *adj_mode)
{
	int vic;
	struct ms7210 *ms = bridge_to_ms7210(bridge);
	struct videomode *vmode = &ms->vmode;
	uint8_t polarity = 0;

	vic = drm_match_cea_mode(mode);
	pr_info("%s(): VIC[%d]\n", __FUNCTION__, vic);

	ms->mode = drm_mode_duplicate(bridge->dev, mode);

	drm_mode_debug_printmodeline(mode);
	drm_display_mode_to_videomode(mode, vmode);

	if (0) {
		debug_show_drm_display_mode(adj_mode);
		debug_show_video_mode(vmode);
	}

	ms->g_t_hdtx_infoframe.u8_vic = vic;
	uint16_t vtotal = vmode->vactive + vmode->vfront_porch + vmode->vback_porch + vmode->vsync_len;
	uint16_t vfront_porch = (adj_mode->flags & DRM_MODE_FLAG_INTERLACE)? (vmode->vfront_porch / 2) : vmode->vfront_porch;

	ms->g_t_hdtx_timing.u16_htotal = adj_mode->crtc_htotal;
	ms->g_t_hdtx_timing.u16_vtotal = vtotal;
	ms->g_t_hdtx_timing.u16_hactive = vmode->hactive;
	ms->g_t_hdtx_timing.u16_vactive = vmode->vactive;
	ms->g_t_hdtx_timing.u16_pixclk = vmode->pixelclock / 10000;
	ms->g_t_hdtx_timing.u16_vfreq =  drm_mode_vrefresh(mode) * 100;
	ms->g_t_hdtx_timing.u16_hoffset = adj_mode->crtc_htotal - vmode->hactive - vmode->hfront_porch;
	uint16_t voffset = vtotal - vmode->vactive - vfront_porch;
	ms->g_t_hdtx_timing.u16_voffset = (adj_mode->flags & DRM_MODE_FLAG_INTERLACE)? (voffset / 2) : voffset;
	// ms->g_t_hdtx_timing.u16_voffset = 20;
	ms->g_t_hdtx_timing.u16_hsyncwidth = vmode->hsync_len;
	ms->g_t_hdtx_timing.u16_vsyncwidth = (adj_mode->flags & DRM_MODE_FLAG_INTERLACE)? (vmode->vsync_len / 2) : vmode->vsync_len;

	if (adj_mode->flags & DRM_MODE_FLAG_PHSYNC) {
		polarity |= MSRT_BIT1;
	}
	if (adj_mode->flags & DRM_MODE_FLAG_PVSYNC) {
		polarity |= MSRT_BIT2;
	}

	if (adj_mode->flags & DRM_MODE_FLAG_INTERLACE) {
		polarity &= ~MSRT_BIT0;
	}
	else {
		polarity |= MSRT_BIT0;
	}
	ms->g_t_hdtx_timing.u8_polarity = polarity;

	#if 0
	printk("u8_polarity = %d\n",	ms->g_t_hdtx_timing.u8_polarity);
	printk("u16_htotal = %d\n",	ms->g_t_hdtx_timing.u16_htotal);
	printk("u16_vtotal = %d\n",	ms->g_t_hdtx_timing.u16_vtotal);
	printk("u16_hactive = %d\n",	ms->g_t_hdtx_timing.u16_hactive);
	printk("u16_vactive = %d\n",	ms->g_t_hdtx_timing.u16_vactive);
	printk("u16_pixclk = %d\n",	ms->g_t_hdtx_timing.u16_pixclk);
	printk("u16_vfreq = %d\n",	ms->g_t_hdtx_timing.u16_vfreq);
	printk("u16_hoffset = %d\n",	ms->g_t_hdtx_timing.u16_hoffset);
	printk("u16_voffset = %d\n",	ms->g_t_hdtx_timing.u16_voffset);
	printk("u16_hsyncwidth = %d\n",	ms->g_t_hdtx_timing.u16_hsyncwidth);
	printk("u16_vsyncwidth = %d\n",	ms->g_t_hdtx_timing.u16_vsyncwidth);
	#endif
}


static int ms7210_bridge_attach(struct drm_bridge *bridge, enum drm_bridge_attach_flags flags)
{
	pr_info("%s\n", __FUNCTION__);

	struct ms7210 *ms = bridge_to_ms7210(bridge);
	struct drm_device *drm = bridge->dev;
	int ret;

	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found");
		return -ENODEV;
	}

	if (flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)
		return drm_bridge_attach(bridge->encoder, NULL,
					 bridge, flags);

	drm_connector_helper_add(&ms->connector,
				 &ms7210_connector_helper_funcs);

	if (!drm_core_check_feature(drm, DRIVER_ATOMIC)) {
		dev_err(&ms->i2c_main->dev,
			"ms7210 driver is only compatible with DRM devices supporting atomic updates\n");
		return -ENOTSUPP;
	}

	ret = drm_connector_init(bridge->dev, &ms->connector,
		&ms7210_connector_funcs,
		DRM_MODE_CONNECTOR_HDMIA);
	   if (ret) {
		   DRM_ERROR("Failed to initialize connector with drm\n");
		   return ret;
	   }

	if (ms->i2c_main->irq) {
		ms->connector.polled = DRM_CONNECTOR_POLL_HPD;
		pr_info("connector detect: hpd interrupt\n");
	}
	else {
		ms->connector.polled = DRM_CONNECTOR_POLL_CONNECT |
				DRM_CONNECTOR_POLL_DISCONNECT;
		pr_info("connector detect: poll\n");
	}
	ms->connector.interlace_allowed = true;
	drm_connector_attach_encoder(&ms->connector, bridge->encoder);

	pr_info(KERN_ERR "%s finish.\n", __FUNCTION__);

	return ret;
}


static const struct drm_bridge_funcs ms7210_bridge_funcs = {
	.attach = ms7210_bridge_attach,
	.mode_set = ms7210_bridge_mode_set,
	.enable = ms7210_bridge_enable,
	.disable = ms7210_bridge_disable,
	.get_edid = ms7210_bridge_get_edid,
};

static const struct drm_bridge_timings default_ms7210_timings = {
	.input_bus_flags = DRM_BUS_FLAG_PIXDATA_SAMPLE_NEGEDGE
		 | DRM_BUS_FLAG_SYNC_SAMPLE_NEGEDGE
		 | DRM_BUS_FLAG_DE_HIGH,
};

// static struct i2c_board_info edid_info = {
// 	.type = "edid",
// 	.addr = 0x50,
// };

static void ms7210_uninit_regulators(struct ms7210 *ms)
{
	if (ms) {
		devm_kfree(&ms->i2c_main->dev, ms);
	}
}

static void ms7210_hw_reset(struct ms7210 *ms)
{
	if (ms->reset_gpio) {
		gpiod_set_value(ms->reset_gpio, 0);
		msleep(100);
		gpiod_set_value(ms->reset_gpio, 1);
		msleep(50);
		pr_info("MS7210 HW reset.\n");
	}
	else {
		pr_err("MS7210 Reset Pin not config\n");
	}
}

static void ms7210_chip_init(struct ms7210 *ms)
{
	ms7210_set_i2c_adapter(ms->i2c_main);

	if (!ms7210_chip_connect_detect()) {
		pr_err("MS7210 Not Connect\n");
		return;
	}

#if 1
	ms->g_t_dvin_config.u8_cs_mode = DVIN_CS_MODE_YUV422;
	ms->g_t_dvin_config.u8_bw_mode = DVIN_BW_MODE_16_20_24BIT;
	ms->g_t_dvin_config.u8_sq_mode = DVIN_SQ_MODE_NONSEQ;
	ms->g_t_dvin_config.u8_dr_mode = DVIN_DR_MODE_SDR;
	ms->g_t_dvin_config.u8_sy_mode = DVIN_SY_MODE_EMBEDDED;

	memcpy(&ms->g_t_hdtx_timing, &hdtx_timing_4k, sizeof(VIDEOTIMING_T));

	ms7210_dvin_video_config(FALSE);
    ms7210_dvin_init(&ms->g_t_dvin_config, 0);
    ms7210_dvin_video_config(TRUE);
    sys_default_hd_video_config(&ms->g_t_hdtx_infoframe);
    sys_default_hd_vendor_specific_config(&ms->g_t_hdtx_infoframe);
    sys_default_hd_audio_config(&ms->g_t_hdtx_infoframe);
#else
	ms7210_hdtx_shutdown_output();
#endif
}

ssize_t sample_rate_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	uint32_t sample_rate = 0;

	switch (ms7210_sample_rate) {
		case HD_AUD_RATE_44K1:
			sample_rate = 44100;
			break;
		case HD_AUD_RATE_48K:
			sample_rate = 48000;
			break;
		case HD_AUD_RATE_32K:
			sample_rate = 32000;
			break;
		case HD_AUD_RATE_88K2:
			sample_rate = 88200;
			break;
		case HD_AUD_RATE_96K:
			sample_rate = 96000;
			break;
		case HD_AUD_RATE_176K4:
			sample_rate = 176400;
			break;
		case HD_AUD_RATE_192K:
			sample_rate = 192000;
			break;
		default:
			pr_err("Unknow MS7210 Sample rate: %d\n", ms7210_sample_rate);
			break;
	}

	snprintf(buf, 32, "%d\n", sample_rate);

	return strlen(buf);
}

ssize_t sample_rate_store(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	uint32_t sample_rate = 0;
	sscanf(buf, "%du", &sample_rate);

	switch (sample_rate) {
		case 44100:
			ms7210_sample_rate = HD_AUD_RATE_44K1;
			break;
		case 48000:
			ms7210_sample_rate = HD_AUD_RATE_48K;
			break;
		case 32000:
			ms7210_sample_rate = HD_AUD_RATE_32K;
			break;
		case 88200:
			ms7210_sample_rate = HD_AUD_RATE_88K2;
			break;
		case 96000:
			ms7210_sample_rate = HD_AUD_RATE_96K;
			break;
		case 176400:
			ms7210_sample_rate = HD_AUD_RATE_176K4;
			break;
		case 192000:
			ms7210_sample_rate = HD_AUD_RATE_176K4;
			break;
		default:
			pr_err("Not Support MS7210 Sample rate: %d\n", sample_rate);
			break;
	}

	return count;
}

static struct class_attribute sample_rate_attribute =
	__ATTR(sample_rate, 0644, sample_rate_show, sample_rate_store);

static struct attribute *ms7210_attributes[] = {
	&sample_rate_attribute.attr,
	NULL
};

static const struct attribute_group ms7210_group = {
	.attrs = ms7210_attributes,
};

static const struct attribute_group *ms7210_attr_group[] = {
	&ms7210_group,
	NULL,
};

static struct class ms7210_class = {
	.name = "ms7210",
	.class_groups = ms7210_attr_group,
};

static int ms7210_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct ms7210 *ms;
	struct device *dev = &i2c->dev;

	dev_dbg(dev, "ms7210_probe\n");

	if (!dev->of_node)
		return -EINVAL;

	ms = devm_kzalloc(dev, sizeof(*ms), GFP_KERNEL);
	if (!ms)
		return -ENOMEM;

	ms->i2c_main = i2c;
	ms->status = connector_status_disconnected;

	// if (dev->of_node)
	// 	ms->type = (enum ms7210_type)of_device_get_match_data(dev);
	// else
	// 	ms->type = id->driver_data;

	/* Reset Gpio */
	ms->reset_gpio = devm_gpiod_get_optional(&i2c->dev, "reset", GPIOD_OUT_LOW);
	if (!ms->reset_gpio)
	{
		/* reset_gpio not available */
		pr_err("rst-gpio not found !!!!!!! \n");
		goto uninit_regulators;
	}

	if (i2c->irq)
		dev_dbg(dev, "Using i2c interrupt to hpd\n");
	else
		dev_dbg(dev, "Using poll to hpd\n");

	i2c_set_clientdata(i2c, ms);

	ms7210_hw_reset(ms);
	ms7210_chip_init(ms);

	/* Add DRM Bridge */
	ms->bridge.funcs = &ms7210_bridge_funcs;
	ms->bridge.of_node = dev->of_node;
	ms->bridge.timings = &default_ms7210_timings;
	ms->bridge.ops = DRM_BRIDGE_OP_DETECT | DRM_BRIDGE_OP_EDID;
	drm_bridge_add(&ms->bridge);

	class_register(&ms7210_class);

	return 0;

uninit_regulators:
	ms7210_uninit_regulators(ms);
	ms = NULL;
	return -1;
}

static void ms7210_remove(struct i2c_client *i2c)
{
	pr_info("ms7210_remove\n");
	struct ms7210 *ms = i2c_get_clientdata(i2c);
	class_unregister(&ms7210_class);
	drm_bridge_remove(&ms->bridge);
	ms7210_uninit_regulators(ms);
}

static const struct i2c_device_id ms7210_i2c_ids[] = {
	{ "ms7210", MS7210 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ms7210_i2c_ids);

static const struct of_device_id ms7210_of_ids[] = {
	{ .compatible = "ms,ms7210", .data = (void *)MS7210 },
	{ }
};
MODULE_DEVICE_TABLE(of, ms7210_of_ids);

static struct i2c_driver ms7210_driver = {
	.driver = {
		.name = "ms7210",
		.of_match_table = ms7210_of_ids,
	},
	.id_table = ms7210_i2c_ids,
	.probe = ms7210_probe,
	.remove = ms7210_remove,
};

static int __init ms7210_init(void)
{
	return i2c_add_driver(&ms7210_driver);
}
module_init(ms7210_init);

static void __exit ms7210_exit(void)
{
	i2c_del_driver(&ms7210_driver);
}
module_exit(ms7210_exit);

MODULE_DESCRIPTION("MS7210 HDMI transmitter driver");
MODULE_LICENSE("GPL");
