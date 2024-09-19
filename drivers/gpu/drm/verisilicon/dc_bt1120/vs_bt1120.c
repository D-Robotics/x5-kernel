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

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_bridge.h>
#include <drm/drm_of.h>
#include <drm/drm_managed.h>

#include "vs_bt1120.h"
#include "vs_bt1120_reg.h"
#include "vs_drv.h"
#include "vs_gem.h"
#include "vs_writeback.h"

#define BT1120_CSC_COEFF_CNT  12
#define DRM_FORMAT_TABLE_END  0
#define RGB_TO_YUV_TABLE_SIZE 12
#define NUM_MODIFIERS	      2
#define GAMMA_SIZE	      256
#define HSTRIDE_UNIT_SIZE     8
#define INTERLACED_MODE	      0
#define BT1120_CRTC_NAME      "crtc-bt1120"
#define DC_CRTC_NAME	      "crtc-dc8000"
#define BT1120_AUX_DEV_NAME   "vs_sif"

struct device *bt1120_dev;

/*
 * RGB to YUV, BT601 full range conversion parameters
 */
static s16 RGB2YUV601[RGB_TO_YUV_TABLE_SIZE] = {
	263, 516, 100, 16, -152, -298, 450, 128, 450, -377, -74, 128,
};

static u64 format_modifiers[] = {DRM_FORMAT_MOD_LINEAR, DRM_FORMAT_MOD_INVALID};

static const u32 primary_formats[] = {
	DRM_FORMAT_NV16, DRM_FORMAT_NV61, DRM_FORMAT_NV12, DRM_FORMAT_NV21,
	DRM_FORMAT_YUYV, DRM_FORMAT_YVYU, DRM_FORMAT_UYVY, DRM_FORMAT_VYUY,
};

static const char *const crtc_names[] = {
	BT1120_CRTC_NAME,
};

static struct bt1120_plane_info g_bt1120_plane_info = {
	.info =
		{
			.name		= "Primary",
			.type		= DRM_PLANE_TYPE_PRIMARY,
			.modifiers	= format_modifiers,
			.num_modifiers	= NUM_MODIFIERS,
			.num_formats	= ARRAY_SIZE(primary_formats),
			.formats	= primary_formats,
			.min_width	= 0,
			.min_height	= 0,
			.max_width	= 4096,
			.max_height	= 4096,
			.color_encoding = BIT(DRM_COLOR_YCBCR_BT601) | BIT(DRM_COLOR_YCBCR_BT709),
			.min_scale	= DRM_PLANE_NO_SCALING,
			.max_scale	= DRM_PLANE_NO_SCALING,
			.crtc_names	= crtc_names,
			.num_crtcs	= ARRAY_SIZE(crtc_names),
		},
	.dma_burst_len = 0xa,
};

static struct bt1120_display_info g_bt1120_disp_info = {
	.info =
		{
			.name	       = BT1120_CRTC_NAME,
			.max_bpc       = 8,
			.color_formats = DRM_COLOR_FORMAT_YCBCR422 | DRM_COLOR_FORMAT_YCBCR420,
		},
};

#ifdef CONFIG_VERISILICON_WRITEBACK_SIF
static const u32 wb_formats[] = {DRM_FORMAT_YUYV};

static const struct bt1120_proc_info bt1120_proc_info_wb = {
	.name = BT1120_AUX_DEV_NAME,
};

static const char *const wb_crtc_names[] = {BT1120_CRTC_NAME, DC_CRTC_NAME};
#endif

#ifdef CONFIG_VERISILICON_WRITEBACK
static struct bt1120_wb_info bt1120_wb_info = {
	.info =
		{
			.wb_formats	= wb_formats,
			.num_wb_formats = ARRAY_SIZE(wb_formats),
			.crtc_names	= wb_crtc_names,
			.num_crtcs	= ARRAY_SIZE(wb_crtc_names),
		},
	.proc_info = &bt1120_proc_info_wb,
};
#endif

static const struct bt1120_info g_bt1120_info = {
	.plane	 = &g_bt1120_plane_info,
	.display = &g_bt1120_disp_info,
#ifdef CONFIG_VERISILICON_WRITEBACK
	.writebacks = &bt1120_wb_info,
#endif
};

static inline void bt1120_write(struct vs_bt1120 *bt1120, u32 reg, u32 val)
{
	writel(val, bt1120->base + reg);
}

static inline u32 bt1120_read(struct vs_bt1120 *bt1120, u32 reg)
{
	return readl(bt1120->base + reg);
}

static inline void bt1120_set_clear(struct vs_bt1120 *bt1120, u32 reg, u32 set, u32 clear)
{
	u32 value = bt1120_read(bt1120, reg);

	value &= ~clear;
	value |= set;
	bt1120_write(bt1120, reg, value);
}

static void bt1120_set_csc_coeff(struct vs_bt1120 *bt1120)
{
	u32 index;

	for (index = 0; index < BT1120_CSC_COEFF_CNT; index++)
		bt1120_write(bt1120, REG_BT1120_CSC_COEFF + index * 4, RGB2YUV601[index] & 0xFFFF);
}

void bt1120_disable(struct vs_bt1120 *bt1120)
{
	/* disable bt1120_en*/
	bt1120_set_clear(bt1120, REG_BT1120_CTL, 0, BIT(0));
}

void bt1120_set_online_configs(struct vs_bt1120 *bt1120, const struct drm_display_mode *mode)
{
	struct bt1120_scan scan;

	if (bt1120->is_online)
		bt1120_set_clear(bt1120, REG_BT1120_CTL, BIT(3), 0);
	else
		bt1120_set_clear(bt1120, REG_BT1120_CTL, 0, BIT(3));

	if (!bt1120->is_online)
		return;

	scan.h_sync_start   = 1;
	scan.h_sync_stop    = scan.h_sync_start + (mode->hsync_end - mode->hsync_start);
	scan.h_total	    = mode->htotal;
	scan.h_active_start = scan.h_sync_stop + (mode->htotal - mode->hsync_end);
	scan.h_active_stop  = scan.h_active_start + mode->hdisplay - 1;

	scan.v_sync_start   = 1;
	scan.v_sync_stop    = scan.v_sync_start + (mode->vsync_end - mode->vsync_start);
	scan.v_total	    = mode->vtotal;
	scan.v_active_start = scan.v_sync_stop + (mode->vtotal - mode->vsync_end);
	scan.v_active_stop  = scan.v_active_start + mode->vdisplay - 1;

	if (scan.h_total - scan.h_active_stop > 0)
		scan.hfp_range = 1;
	else
		scan.hfp_range = 0;

	bt1120_write(bt1120, REG_BT1120_HSYNC_ZONE_CTL, scan.h_sync_start);
	bt1120_write(bt1120, REG_BT1120_VSYNC_ZONE_CTL, scan.v_sync_start);

	bt1120_write(bt1120, REG_BT1120_DISP_XZONE_CTL,
		     scan.h_active_start | (scan.h_active_stop << XSTOP_SHIFT));
	bt1120_write(bt1120, REG_BT1120_DISP_YZONE_CTL,
		     scan.v_active_start | (scan.v_active_stop << YSTOP_SHIFT));

	bt1120_write(bt1120, REG_BT1120_HFP_RANGE_CTL, scan.hfp_range);
	bt1120_write(bt1120, REG_BT1120_SCAN_FORMAT_CTL, 0x1);

	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		bt1120_set_clear(bt1120, REG_BT1120_CTL, BIT(5), 0);
	else
		bt1120_set_clear(bt1120, REG_BT1120_CTL, 0, BIT(5));

	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		bt1120_set_clear(bt1120, REG_BT1120_CTL, BIT(6), 0);
	else
		bt1120_set_clear(bt1120, REG_BT1120_CTL, 0, BIT(6));

	bt1120_set_csc_coeff(bt1120);

	/* enable bt1120_en*/
	bt1120_set_clear(bt1120, REG_BT1120_CTL, BIT(0), 0);
}

__maybe_unused static void bt1120_set_output_clock_mode(struct vs_bt1120 *bt1120, bool is_sdr)
{
	if (is_sdr)
		bt1120_set_clear(bt1120, REG_BT1120_CTL, BIT(4), 0);
	else
		bt1120_set_clear(bt1120, REG_BT1120_CTL, 0, BIT(4));
}

static void update_format(struct bt1120_plane *bt1120_plane, u32 drm_format)
{
	u8 image_format = YUV422_SP;
	bool uv_swizzle = true;
	bool y_swizzle	= true;

	switch (drm_format) {
	case DRM_FORMAT_NV16:
		image_format = YUV422_SP;
		uv_swizzle   = false;
		break;
	case DRM_FORMAT_NV61:
		image_format = YUV422_SP;
		uv_swizzle   = true;
		break;
	case DRM_FORMAT_NV12:
		image_format = YUV420_SP;
		uv_swizzle   = false;
		break;
	case DRM_FORMAT_NV21:
		image_format = YUV420_SP;
		uv_swizzle   = true;
		break;
	case DRM_FORMAT_YUYV:
		image_format = YUV422_PACKED;
		uv_swizzle   = false;
		y_swizzle    = false;
		break;
	case DRM_FORMAT_YVYU:
		image_format = YUV422_PACKED;
		uv_swizzle   = true;
		y_swizzle    = false;
		break;
	case DRM_FORMAT_UYVY:
		image_format = YUV422_PACKED;
		uv_swizzle   = false;
		y_swizzle    = true;
		break;
	case DRM_FORMAT_VYUY:
		image_format = YUV422_PACKED;
		uv_swizzle   = true;
		y_swizzle    = true;
		break;
	default:
		break;
	}

	bt1120_set_clear(bt1120_plane->bt1120, REG_BT1120_CTL, image_format << IMG_FORMAT_SHIFT,
			 IMG_FORMAT_MASK);
	bt1120_write(bt1120_plane->bt1120, REG_BT1120_DDR_STORE_FORMAT_CTL,
		     uv_swizzle | !y_swizzle << Y_STORE_FORMAT_SHIFT);
}

static u8 fb_get_plane_number(struct drm_framebuffer *fb)
{
	const struct drm_format_info *info;

	if (!fb)
		return 0;

	info = drm_format_info(fb->format->format);
	if (!info || info->num_planes > MAX_NUM_PLANES)
		return 0;

	return info->num_planes;
}

static void get_buffer_addr(struct drm_framebuffer *drm_fb, dma_addr_t dma_addr[])
{
	u8 num_planes, i;

	num_planes = fb_get_plane_number(drm_fb);

	for (i = 0; i < num_planes; i++) {
		struct vs_gem_object *vs_obj;

		vs_obj	    = vs_fb_get_gem_obj(drm_fb, i);
		dma_addr[i] = vs_obj->dma_addr + drm_fb->offsets[i];
	}
}

static void bt1120_plane_destroy(struct vs_plane *vs_plane)
{
	struct bt1120_plane *bt1120_plane = to_bt1120_plane(vs_plane);
	kfree(bt1120_plane);
}

static int bt1120_plane_check(struct vs_plane *vs_plane, struct drm_plane_state *state)
{
	const struct drm_rect *src = &state->src;
	struct drm_framebuffer *drm_fb;
	u32 format, img_vsize, line_offset;

	if (!state->fb)
		return 0;

	drm_fb	    = state->fb;
	format	    = drm_fb->format->format;
	img_vsize   = drm_fb->height;
	line_offset = src->y1 >> 16;

	/** make sure image_vsize and dma line_offset are even numbers when input
	 *  format is YUV420 semi-planar.
	 */
	if (format == DRM_FORMAT_NV12 || format == DRM_FORMAT_NV21) {
		if ((img_vsize % 2) || (line_offset % 2))
			return -EINVAL;
	}

	return 0;
}

static void bt1120_plane_update(struct vs_plane *vs_plane, struct drm_plane_state *old_state)
{
	struct bt1120_plane *bt1120_plane   = to_bt1120_plane(vs_plane);
	struct drm_plane_state *state	    = vs_plane->base.state;
	const struct drm_rect *src	    = &state->src;
	const struct drm_rect *dst	    = &state->dst;
	struct drm_framebuffer *drm_fb	    = state->fb;
	dma_addr_t dma_addr[MAX_NUM_PLANES] = {};

	u32 format	= drm_fb->format->format;
	u32 img_vsize	= drm_fb->height;
	u32 line_offset = src->y1 >> 16;

	get_buffer_addr(drm_fb, dma_addr);
	update_format(bt1120_plane, format);

	bt1120_write(bt1120_plane->bt1120, REG_BT1120_IMG_IN_BADDR_Y_CTL, dma_addr[0]);
	bt1120_write(bt1120_plane->bt1120, REG_BT1120_IMG_IN_BADDR_UV_CTL, dma_addr[1]);

	bt1120_write(bt1120_plane->bt1120, REG_BT1120_IMG_PIX_HSTRIDE_CTL,
		     drm_fb->pitches[0] / HSTRIDE_UNIT_SIZE);

	bt1120_write(bt1120_plane->bt1120, REG_BT1120_IMG_PIX_VSIZE_CTL, img_vsize);
	bt1120_write(bt1120_plane->bt1120, REG_BT1120_IMG_PIX_HSIZE_CTL, drm_fb->width);

	bt1120_write(bt1120_plane->bt1120, REG_BT1120_LINE_OFFSET_CTL, line_offset);
	bt1120_write(bt1120_plane->bt1120, REG_BT1120_WORD_OFFSET_CTL, src->x1 >> 16);
	bt1120_write(bt1120_plane->bt1120, REG_BT1120_IMG_PIX_ZONE_CTL,
		     dst->x1 | (dst->y1 << PIX_Y_SHIFT));
}

static int bt1120_plane_check_format(struct vs_plane *vs_plane, u32 format, uint64_t modifier)
{
	if (format == DRM_FORMAT_NV16 || format == DRM_FORMAT_NV61 || format == DRM_FORMAT_NV12 ||
	    format == DRM_FORMAT_NV21 || format == DRM_FORMAT_YUYV || format == DRM_FORMAT_YVYU ||
	    format == DRM_FORMAT_UYVY || format == DRM_FORMAT_VYUY)
		return true;

	return false;
}

static const struct vs_plane_funcs bt1120_plane_funcs = {
	.destroy	  = bt1120_plane_destroy,
	.check		  = bt1120_plane_check,
	.update		  = bt1120_plane_update,
	.check_format_mod = bt1120_plane_check_format,
};

static void bt1120_plane_init(struct bt1120_plane *bt1120_plane,
			      struct bt1120_plane_info *bt1120_plane_info)
{
	struct vs_plane *vs_plane	    = &bt1120_plane->vs_plane;
	struct vs_plane_info *vs_plane_info = &bt1120_plane_info->info;

	vs_plane->info	= vs_plane_info;
	vs_plane->funcs = &bt1120_plane_funcs;

	bt1120_plane->info = bt1120_plane_info;

	bt1120_write(bt1120_plane->bt1120, REG_BT1120_DMA_BLENGTH_CTL,
		     bt1120_plane->info->dma_burst_len);
}

static void sif_work(struct work_struct *work)
{
	struct bt1120_wb *bt1120_wb		     = container_of(work, struct bt1120_wb, work);
	struct vs_wb *vs_wb			     = &bt1120_wb->vs_wb;
	struct drm_writeback_connector *wb_connector = &vs_wb->base;
	const struct drm_connector *connector	     = &wb_connector->base;
	struct drm_connector_state *state	     = connector->state;
	const struct drm_writeback_job *job	     = state->writeback_job;

	if (!job)
		return;

	sif_set_output(bt1120_wb->sif, NULL);

	drm_writeback_queue_job(wb_connector, state);
	drm_writeback_signal_completion(wb_connector, 0);
}

static void bt1120_wb_create_writeback(struct device *dev, struct bt1120_wb *bt1120_wb)
{
	struct vs_sif *sif = dev_get_drvdata(dev);

	bt1120_wb->sif = sif;

	INIT_WORK(&bt1120_wb->work, sif_work);
}

static void bt1120_wb_commit(struct vs_wb *vs_wb, struct drm_connector_state *state)
{
	struct bt1120_wb *bt1120_wb   = to_bt1120_wb(vs_wb);
	struct drm_writeback_job *job = vs_wb->base.base.state->writeback_job;

	/* make sure sif work is linked to correct wb connector*/
	bt1120_wb->sif->work = &bt1120_wb->work;

	if (!job || !job->fb) {
		sif_set_output(bt1120_wb->sif, NULL);
		return;
	}

	sif_set_output(bt1120_wb->sif, job->fb);
}

static void bt1120_wb_destroy(struct vs_wb *vs_wb)
{
	struct bt1120_wb *bt1120_wb = to_bt1120_wb(vs_wb);
	kfree(bt1120_wb);
}

static int bt1120_wb_post_create(struct bt1120_wb *bt1120_wb)
{
	struct vs_wb *vs_wb	   = &bt1120_wb->vs_wb;
	struct drm_connector *conn = &vs_wb->base.base;
	struct drm_encoder *encoder;
	struct drm_bridge *bridge;
	int ret		   = 0;
	struct vs_sif *sif = bt1120_wb->sif;

	drm_connector_for_each_possible_encoder (conn, encoder)
		break;

	/* output port is port 1*/
	bridge = devm_drm_of_get_bridge(sif->dev, sif->dev->of_node, 1, -1);
	if (IS_ERR(bridge)) {
		ret    = PTR_ERR(bridge);
		bridge = NULL;

		if (ret == -ENODEV)
			return 0;

		dev_err(sif->dev, "find bridge failed(err=%d)\n", ret);

		return ret;
	}

	return drm_bridge_attach(encoder, bridge, NULL, DRM_BRIDGE_ATTACH_NO_CONNECTOR);
}

static void bt1120_wb_vblank(struct vs_wb *vs_wb)
{
	struct drm_writeback_connector *wb_connector = &vs_wb->base;
	const struct drm_connector *connector	     = &wb_connector->base;
	struct drm_connector_state *state	     = connector->state;
	struct drm_writeback_job *job		     = state->writeback_job;
	u8 *wait_vsync_cnt;

	if (job) {
		wait_vsync_cnt = job->priv;

		if (!*wait_vsync_cnt) {
			drm_writeback_queue_job(wb_connector, state);
			drm_writeback_signal_completion(wb_connector, 0);
			return;
		}

		(*wait_vsync_cnt)--;
	}
}

static const struct vs_wb_funcs vs_wb_funcs = {
	.destroy = bt1120_wb_destroy,
	.commit	 = bt1120_wb_commit,
};

static struct device *__find_device(struct list_head *device_list, const char *name)
{
	struct bt1120_aux_device *bt1120_aux_device;

	list_for_each_entry (bt1120_aux_device, device_list, head)
		if (strcmp(bt1120_aux_device->name, name) == 0)
			return bt1120_aux_device->dev;

	return ERR_PTR(-ENODEV);
}

static int bt1120_wb_init(struct bt1120_wb *bt1120_wb, struct bt1120_wb_info *bt1120_wb_info,
			  struct list_head *device_list)
{
	struct device *dev;
	struct vs_wb *vs_wb	      = &bt1120_wb->vs_wb;
	struct vs_wb_info *vs_wb_info = &bt1120_wb_info->info;
	int ret			      = 0;

	vs_wb->info  = vs_wb_info;
	vs_wb->funcs = &vs_wb_funcs;

	bt1120_wb->info = bt1120_wb_info;

	dev = __find_device(device_list, bt1120_wb_info->proc_info->name);
	if (IS_ERR(dev)) {
		ret = -ENODEV;
	}

	bt1120_wb_create_writeback(dev, bt1120_wb);

	return ret;
}

static void bt1120_disp_update_scan(struct bt1120_disp *bt1120_disp, struct bt1120_scan *scan,
				    struct drm_display_mode *mode)
{
	u32 reg_value;

	scan->h_sync_start   = 1;
	scan->h_sync_stop    = scan->h_sync_start + (mode->hsync_end - mode->hsync_start);
	scan->h_total	     = mode->htotal;
	scan->h_active_start = scan->h_sync_stop + (mode->htotal - mode->hsync_end);
	scan->h_active_stop  = scan->h_active_start + mode->hdisplay - 1;

	if (!scan->is_interlaced) {
		scan->v_sync_start   = 1;
		scan->v_sync_stop    = scan->v_sync_start + (mode->vsync_end - mode->vsync_start);
		scan->v_total	     = mode->vtotal;
		scan->v_active_start = scan->v_sync_stop + (mode->vtotal - mode->vsync_end);
		scan->v_active_stop  = scan->v_active_start + mode->vdisplay - 1;
	} else {
		scan->v_sync_start = 1;
		scan->v_sync_stop =
			scan->v_sync_start + (mode->vsync_end - mode->vsync_start + 1) / 2 - 1;
		scan->v_total	     = mode->vtotal / 2 + 1;
		scan->v_active_start = scan->v_sync_stop + (mode->vtotal - mode->vsync_end) / 2;
		scan->v_active_stop  = scan->v_active_start + mode->vdisplay / 2 - 1;

		reg_value = bt1120_read(bt1120_disp->bt1120, REG_BT1120_IMG_PIX_HSTRIDE_CTL);
		bt1120_write(bt1120_disp->bt1120, REG_BT1120_IMG_PIX_HSTRIDE_CTL, reg_value * 2);

		reg_value = bt1120_read(bt1120_disp->bt1120, REG_BT1120_IMG_PIX_VSIZE_CTL);
		bt1120_write(bt1120_disp->bt1120, REG_BT1120_IMG_PIX_VSIZE_CTL, reg_value / 2);
	}

	if (scan->h_total - scan->h_active_stop > 0)
		scan->hfp_range = 1;
	else
		scan->hfp_range = 0;

	bt1120_write(bt1120_disp->bt1120, REG_BT1120_HSYNC_ZONE_CTL,
		     scan->h_sync_start | (scan->h_sync_stop << HSYNC_STOP_SHIFT));
	bt1120_write(bt1120_disp->bt1120, REG_BT1120_VSYNC_ZONE_CTL,
		     scan->v_sync_start | (scan->v_sync_stop << VSYNC_STOP_SHIFT));

	bt1120_write(bt1120_disp->bt1120, REG_BT1120_DISP_XZONE_CTL,
		     scan->h_active_start | (scan->h_active_stop << XSTOP_SHIFT));
	bt1120_write(bt1120_disp->bt1120, REG_BT1120_DISP_YZONE_CTL,
		     scan->v_active_start | (scan->v_active_stop << YSTOP_SHIFT));

	bt1120_write(bt1120_disp->bt1120, REG_BT1120_DISP_WIDTH_CTL, scan->h_total);
	bt1120_write(bt1120_disp->bt1120, REG_BT1120_DISP_HEIGHT_CTL, scan->v_total);

	bt1120_write(bt1120_disp->bt1120, REG_BT1120_HFP_RANGE_CTL, scan->hfp_range);

	bt1120_write(bt1120_disp->bt1120, REG_BT1120_SCAN_FORMAT_CTL, !scan->is_interlaced);
	bt1120_write(bt1120_disp->bt1120, REG_BT1120_INT_HEIGHT_OFFSET_CTL, 0x0);
}

static void bt1120_disp_enable(struct vs_crtc *vs_crtc)
{
	struct bt1120_disp *bt1120_disp = to_bt1120_disp(vs_crtc);
	struct drm_display_mode *mode	= &vs_crtc->base.state->adjusted_mode;
	struct bt1120_scan scan;
	u32 pix_clk_rate;
	int ret;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		scan.is_interlaced = true;
	else
		scan.is_interlaced = false;

	ret = clk_prepare_enable(bt1120_disp->bt1120->pix_clk);
	if (ret < 0) {
		pr_err("failed to prepare/enable pix_clk\n");
		return;
	}

	bt1120_disp->refresh_rate = 30;
	pix_clk_rate		  = clk_get_rate(bt1120_disp->bt1120->pix_clk) / 1000;

	if (pix_clk_rate != mode->clock) {
		clk_set_rate(bt1120_disp->bt1120->pix_clk, (unsigned long)(mode->clock) * 1000);
		bt1120_disp->refresh_rate =
			(unsigned long)(mode->clock) * 1000 / (mode->htotal * mode->vtotal);
	}

	bt1120_disp_update_scan(bt1120_disp, &scan, mode);

	bt1120_disp->bt1120->is_odd_field = true;

	/* clear irq status*/
	bt1120_write(bt1120_disp->bt1120, REG_BT1120_IRQ_STATUS, 0xf);

	/* enable frame_start & underflow interrupts*/
	if (scan.is_interlaced)
		bt1120_set_clear(bt1120_disp->bt1120, REG_BT1120_IRQ_EN_CTL, BIT(0) | BIT(2), 0);
	else
		bt1120_set_clear(bt1120_disp->bt1120, REG_BT1120_IRQ_EN_CTL, BIT(2), 0);

	bt1120_set_clear(bt1120_disp->bt1120, REG_BT1120_CTL, BIT(0), 0);
}

static void bt1120_disp_disable(struct vs_crtc *vs_crtc)
{
	struct bt1120_disp *bt1120_disp = to_bt1120_disp(vs_crtc);
	u8 value;

	/* clear irq */
	bt1120_write(bt1120_disp->bt1120, REG_BT1120_IRQ_EN_CTL, 0x0);

	value = bt1120_read(bt1120_disp->bt1120, REG_BT1120_IRQ_STATUS);
	bt1120_write(bt1120_disp->bt1120, REG_BT1120_IRQ_STATUS, value);

	/* disable bt1120_en */
	bt1120_set_clear(bt1120_disp->bt1120, REG_BT1120_CTL, 0, BIT(0));

	/* make sure refresh rate is valid value */
	if (bt1120_disp->refresh_rate == 0)
		bt1120_disp->refresh_rate = 30;
	/* wait till last framestart irq triggered, so last frame can be fully displayed.*/
	mdelay(2 * 1000 / bt1120_disp->refresh_rate);

	clk_disable_unprepare(bt1120_disp->bt1120->pix_clk);
}

static bool bt1120_disp_mode_fixup(struct vs_crtc *vs_crtc, const struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	struct bt1120_disp *bt1120_disp = to_bt1120_disp(vs_crtc);
	long clk_rate;

	if (bt1120_disp->bt1120->pix_clk) {
		clk_rate =
			clk_round_rate(bt1120_disp->bt1120->pix_clk, adjusted_mode->clock * 1000);
		adjusted_mode->clock = DIV_ROUND_UP(clk_rate, 1000);
	}

	return true;
}

static void bt1120_disp_enable_vblank(struct vs_crtc *vs_crtc, bool enabled)
{
	struct bt1120_disp *bt1120_disp = to_bt1120_disp(vs_crtc);

	/* enable dma_done interrupt*/
	if (enabled)
		bt1120_set_clear(bt1120_disp->bt1120, REG_BT1120_IRQ_EN_CTL, BIT(1), 0);
	else
		bt1120_set_clear(bt1120_disp->bt1120, REG_BT1120_IRQ_EN_CTL, 0, BIT(1));
}

static void bt1120_disp_destroy(struct vs_crtc *vs_crtc)
{
	struct bt1120_disp *bt1120_disp = to_bt1120_disp(vs_crtc);
	kfree(bt1120_disp);
}

static struct drm_crtc_state *bt1120_disp_create_state(struct vs_crtc *vs_crtc)
{
	struct bt1120_disp_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	state->underflow_cnt = 0;

	return &state->base;
}

static void bt1120_disp_destroy_state(struct drm_crtc_state *state)
{
	struct bt1120_disp_state *bt1120_state = to_bt1120_disp_state(state);

	kfree(bt1120_state);
}

static struct drm_crtc_state *bt1120_disp_duplicate_state(struct vs_crtc *vs_crtc)
{
	const struct drm_crtc *drm_crtc		    = &vs_crtc->base;
	const struct drm_crtc_state *drm_crtc_state = drm_crtc->state;
	struct bt1120_disp_state *current_state, *new_state;

	if (WARN_ON(!drm_crtc_state))
		return NULL;

	new_state = kzalloc(sizeof(*new_state), GFP_KERNEL);
	if (!new_state)
		return NULL;

	current_state = to_bt1120_disp_state(drm_crtc_state);

	memcpy(new_state, current_state, sizeof(*current_state));

	return &new_state->base;
}

static void bt1120_disp_print_state(struct drm_printer *p, const struct drm_crtc_state *state)
{
	struct bt1120_disp_state *bt1120_state = to_bt1120_disp_state(state);

	drm_printf(p, "\tUnderflow cnt=%d\n", bt1120_state->underflow_cnt);
}

static const struct vs_crtc_funcs bt1120_crtc_funcs = {
	.destroy	 = bt1120_disp_destroy,
	.enable		 = bt1120_disp_enable,
	.disable	 = bt1120_disp_disable,
	.mode_fixup	 = bt1120_disp_mode_fixup,
	.enable_vblank	 = bt1120_disp_enable_vblank,
	.create_state	 = bt1120_disp_create_state,
	.destroy_state	 = bt1120_disp_destroy_state,
	.duplicate_state = bt1120_disp_duplicate_state,
	.print_state	 = bt1120_disp_print_state,
};

static void bt1120_disp_init(struct bt1120_disp *bt1120_disp,
			     struct bt1120_display_info *bt1120_display_info)
{
	struct vs_crtc *vs_crtc		  = &bt1120_disp->vs_crtc;
	struct vs_crtc_info *vs_crtc_info = &bt1120_display_info->info;

	vs_crtc->info  = vs_crtc_info;
	vs_crtc->funcs = &bt1120_crtc_funcs;

	bt1120_disp->info = bt1120_display_info;
}

static int bt1120_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;
	struct vs_bt1120 *bt1120   = dev_get_drvdata(dev);
	const struct bt1120_info *bt1120_info;
	struct device_node *port;
	struct bt1120_plane_info *bt1120_plane_info;
	struct vs_plane *vs_plane;
	struct bt1120_plane *bt1120_plane;
	struct bt1120_display_info *bt1120_display_info;
	struct vs_crtc *vs_crtc;
	struct bt1120_disp *bt1120_disp;
	struct bt1120_wb_info *wb_info;
	struct bt1120_wb *bt1120_wb;
	struct vs_wb *vs_wb;
	u32 ret;

	if (!drm_dev || !bt1120) {
		dev_err(dev, "devices are not created.\n");
		return -ENODEV;
	}

	ret = vs_drm_iommu_attach_device(drm_dev, dev);
	if (ret < 0) {
		dev_err(dev, "Failed to attached iommu device.\n");
		goto out;
	}

	bt1120_info = bt1120->info;

	/* crtc */
	bt1120_display_info = bt1120_info->display;

	bt1120_disp = kzalloc(sizeof(*bt1120_disp), GFP_KERNEL);
	if (!bt1120_disp) {
		ret = -ENOMEM;
		goto err_detach_iommu;
	}
	bt1120_disp->bt1120 = bt1120;
	bt1120_disp_init(bt1120_disp, bt1120_display_info);

	vs_crtc	     = &bt1120_disp->vs_crtc;
	vs_crtc->dev = bt1120->dev;
	ret	     = vs_crtc_init(drm_dev, vs_crtc);
	if (ret)
		goto err_cleanup_crtcs;

	port = of_graph_get_port_by_id(dev->of_node, 0);
	if (!port) {
		dev_err(dev, "no port node found\n");
		ret = -EINVAL;
		goto err_cleanup_vs_crtcs;
	}
	of_node_put(port);
	vs_crtc->base.port = port;

	/* plane */
	bt1120_plane_info = bt1120_info->plane;

	bt1120_plane = kzalloc(sizeof(*bt1120_plane), GFP_KERNEL);
	if (!bt1120_plane) {
		ret = -ENOMEM;
		goto err_cleanup_vs_crtcs;
	}
	bt1120_plane->bt1120 = bt1120;
	bt1120_plane_init(bt1120_plane, bt1120_plane_info);

	vs_plane      = &bt1120_plane->vs_plane;
	vs_plane->dev = bt1120->dev;
	ret	      = vs_plane_init(drm_dev, vs_plane);
	if (ret)
		goto err_cleanup_planes;

	if (vs_plane->base.type == DRM_PLANE_TYPE_PRIMARY)
		set_crtc_primary_plane(drm_dev, &vs_plane->base);

	/* writeback */
	wb_info = &bt1120_info->writebacks[0];

	bt1120_wb = kzalloc(sizeof(*bt1120_wb), GFP_KERNEL);
	if (!bt1120_wb) {
		ret = -ENOMEM;
		goto err_cleanup_planes;
	}

	ret = bt1120_wb_init(bt1120_wb, wb_info, &bt1120->aux_list);
	if (ret)
		goto err_cleanup_planes;

	vs_wb	   = &bt1120_wb->vs_wb;
	vs_wb->dev = bt1120->dev;
	ret	   = vs_wb_init(drm_dev, vs_wb);
	if (ret)
		goto err_cleanup_planes;

	ret = bt1120_wb_post_create(bt1120_wb);
	if (ret)
		goto err_cleanup_planes;

	bt1120->drm_dev	 = drm_dev;
	bt1120->drm_crtc = &vs_crtc->base;

	return 0;

err_cleanup_planes:
	vs_plane->funcs->destroy(vs_plane);
err_cleanup_vs_crtcs:
	drm_crtc_cleanup(&vs_crtc->base);
err_cleanup_crtcs:
	vs_crtc->funcs->destroy(vs_crtc);
err_detach_iommu:
	vs_drm_iommu_detach_device(drm_dev, dev);
out:
	return ret;
}

static void bt1120_unbind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;

	vs_drm_iommu_detach_device(drm_dev, dev);
}

static const struct component_ops bt1120_component_ops = {
	.bind	= bt1120_bind,
	.unbind = bt1120_unbind,
};

static const struct of_device_id bt1120_driver_dt_match[] = {
	{.compatible = "verisilicon,bt1120", .data = &g_bt1120_info}, {}};
MODULE_DEVICE_TABLE(of, bt1120_driver_dt_match);

static void bt1120_wb_handle_vblank(struct drm_crtc *drm_crtc)
{
	const struct drm_crtc_state *crtc_state = drm_crtc->state;
	struct drm_connector *connector;
	struct drm_writeback_connector *wb_connector;
	struct drm_connector_list_iter conn_iter;
	struct vs_wb *vs_wb;

	drm_connector_list_iter_begin(drm_crtc->dev, &conn_iter);

	drm_for_each_connector_iter (connector, &conn_iter) {
		if (!(crtc_state->connector_mask & drm_connector_mask(connector)))
			continue;

		if (connector->connector_type != DRM_MODE_CONNECTOR_WRITEBACK)
			continue;

		wb_connector = drm_connector_to_writeback(connector);
		vs_wb	     = to_vs_wb(wb_connector);
		bt1120_wb_vblank(vs_wb);
	}

	drm_connector_list_iter_end(&conn_iter);
}

static irqreturn_t bt1120_isr(int irq, void *data)
{
	struct vs_bt1120 *bt1120	       = data;
	struct bt1120_disp_state *bt1120_state = to_bt1120_disp_state(bt1120->drm_crtc->state);
	u8 irq_status, scan_mode;
	u32 y_addr, uv_addr, stride;

	irq_status = bt1120_read(bt1120, REG_BT1120_IRQ_STATUS);
	bt1120_write(bt1120, REG_BT1120_IRQ_STATUS, irq_status);

	if (irq_status & DMA_DONE_IRQ_STATUS_MASK) {
		drm_crtc_handle_vblank(bt1120->drm_crtc);
		bt1120_wb_handle_vblank(bt1120->drm_crtc);
	}

	if (irq_status & FRAME_START_IRQ_STATUS_MASK) {
		/* scan mode. 0: interlaced, 1: progressive */
		scan_mode = bt1120_read(bt1120, REG_BT1120_SCAN_FORMAT_CTL);
		if (scan_mode == INTERLACED_MODE) {
			stride	= bt1120_read(bt1120, REG_BT1120_IMG_PIX_HSTRIDE_CTL);
			y_addr	= bt1120_read(bt1120, REG_BT1120_IMG_IN_BADDR_Y_CTL);
			uv_addr = bt1120_read(bt1120, REG_BT1120_IMG_IN_BADDR_UV_CTL);

			if (bt1120->is_odd_field) {
				bt1120_write(bt1120, REG_BT1120_IMG_IN_BADDR_Y_CTL,
					     y_addr + stride * HSTRIDE_UNIT_SIZE);
				bt1120_write(bt1120, REG_BT1120_IMG_IN_BADDR_UV_CTL,
					     uv_addr + stride * HSTRIDE_UNIT_SIZE);
				bt1120->is_odd_field = false;
			} else {
				bt1120_write(bt1120, REG_BT1120_IMG_IN_BADDR_Y_CTL,
					     y_addr - stride * HSTRIDE_UNIT_SIZE);
				bt1120_write(bt1120, REG_BT1120_IMG_IN_BADDR_UV_CTL,
					     uv_addr - stride * HSTRIDE_UNIT_SIZE);
				bt1120->is_odd_field = true;
			}
		}
	}

	if (irq_status & BUF_UNDERFLOW_IRQ_STATUS_MASK)
		bt1120_state->underflow_cnt++;

	return IRQ_HANDLED;
}

static struct bt1120_aux_device *bt1120_add_device(struct device *dev, const char *name)
{
	struct bt1120_aux_device *bt1120_aux_device;

	bt1120_aux_device = devm_kzalloc(dev, sizeof(*bt1120_aux_device), GFP_KERNEL);
	if (!bt1120_aux_device)
		return ERR_PTR(-ENOMEM);

	bt1120_aux_device->dev = dev;
	strncpy(bt1120_aux_device->name, name, BT1120_AUX_DEV_NAME_SIZE - 1);
	return bt1120_aux_device;
}

static int bt1120_get_all_devices(struct device *dev)
{
	struct vs_bt1120 *bt1120 = dev_get_drvdata(dev);
	struct bt1120_aux_device *bt1120_aux_device, *tmp;
	struct platform_device *pdev;
	struct device_node *np;
	const char *aux_name;
	void *drvdata;
	int i, ret;

	i = of_property_count_strings(dev->of_node, "aux-names");

	if (i <= 0)
		return 0;

	while (i--) {
		np = of_parse_phandle(dev->of_node, "aux-devs", i);
		if (!np) {
			ret = -ENODEV;
			goto err_clean;
		}

		pdev = of_find_device_by_node(np);
		of_node_put(np);
		if (!pdev) {
			ret = -ENODEV;
			goto err_clean;
		}

		drvdata = platform_get_drvdata(pdev);
		if (!drvdata) {
			ret = -EPROBE_DEFER;
			goto err_clean;
		}

		of_property_read_string_index(dev->of_node, "aux-names", i, &aux_name);
		bt1120_aux_device = bt1120_add_device(&pdev->dev, aux_name);
		if (IS_ERR(bt1120_aux_device)) {
			ret = PTR_ERR(bt1120_aux_device);
			goto err_clean;
		}

		list_add_tail(&bt1120_aux_device->head, &bt1120->aux_list);
	}

	return 0;

err_clean:
	list_for_each_entry_safe_reverse (bt1120_aux_device, tmp, &bt1120->aux_list, head) {
		list_del(&bt1120_aux_device->head);
		kfree(bt1120_aux_device);
	}
	return ret;
}

static int bt1120_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vs_bt1120 *bt1120;
	u32 irq, ret;

	bt1120 = devm_kzalloc(dev, sizeof(*bt1120), GFP_KERNEL);
	if (!bt1120)
		return -ENOMEM;

	bt1120->dev = dev;
	dev_set_drvdata(dev, bt1120);

	bt1120_dev = dev;

	bt1120->apb_clk = devm_clk_get_optional(dev, "apb_clk");
	if (IS_ERR(bt1120->apb_clk)) {
		dev_err(dev, "failed to get apb_clk source\n");
		return PTR_ERR(bt1120->apb_clk);
	}

	ret = clk_prepare_enable(bt1120->apb_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare/enable apb_clk\n");
		return ret;
	}

	bt1120->axi_clk = devm_clk_get_optional(dev, "axi_clk");
	if (IS_ERR(bt1120->axi_clk)) {
		dev_err(dev, "failed to get axi_clk source\n");
		ret = PTR_ERR(bt1120->axi_clk);
		goto err_disable_apb_clk;
	}

	ret = clk_prepare_enable(bt1120->axi_clk);
	if (ret < 0) {
		dev_err(dev, "failed to prepare/enable axi_clk\n");
		goto err_disable_apb_clk;
	}

	bt1120->pix_clk = devm_clk_get_optional(dev, "pix_clk");
	if (IS_ERR(bt1120->pix_clk)) {
		dev_err(dev, "failed to get pix_clk source\n");
		ret = PTR_ERR(bt1120->pix_clk);
		goto err_disable_axi_clk;
	}

	INIT_LIST_HEAD(&bt1120->aux_list);

	ret = bt1120_get_all_devices(dev);
	if (ret)
		goto err_disable_axi_clk;

	bt1120->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(bt1120->base)) {
		ret = PTR_ERR(bt1120->base);
		goto err_disable_axi_clk;
	}

	irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(dev, irq, bt1120_isr, 0, dev_name(dev), bt1120);
	if (ret < 0) {
		dev_err(dev, "Failed to install irq:%u.\n", irq);
		goto err_disable_axi_clk;
	}

	bt1120->info = &g_bt1120_info;

	ret = component_add(dev, &bt1120_component_ops);
	if (!ret)
		return ret;

err_disable_axi_clk:
	clk_disable_unprepare(bt1120->axi_clk);
err_disable_apb_clk:
	clk_disable_unprepare(bt1120->apb_clk);
	return ret;
}

static int bt1120_remove(struct platform_device *pdev)
{
	struct device *dev	 = &pdev->dev;
	struct vs_bt1120 *bt1120 = dev_get_drvdata(dev);

	component_del(dev, &bt1120_component_ops);

	clk_disable_unprepare(bt1120->axi_clk);
	clk_disable_unprepare(bt1120->apb_clk);

	dev_set_drvdata(dev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int bt1120_suspend(struct device *dev)
{
	struct vs_bt1120 *bt1120 = dev_get_drvdata(dev);

	clk_disable_unprepare(bt1120->axi_clk);
	clk_disable_unprepare(bt1120->apb_clk);

	return 0;
}

static int bt1120_resume(struct device *dev)
{
	int ret;
	struct vs_bt1120 *bt1120 = dev_get_drvdata(dev);

	ret = clk_prepare_enable(bt1120->apb_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare/enable apb_clk\n");
		return ret;
	}

	ret = clk_prepare_enable(bt1120->axi_clk);
	if (ret < 0) {
		dev_err(dev, "failed to prepare/enable axi_clk\n");
		goto err_disable_apb_clk;
	}

	return 0;

err_disable_apb_clk:
	clk_disable_unprepare(bt1120->apb_clk);
	return ret;
}
#endif

static const struct dev_pm_ops bt1120_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bt1120_suspend, bt1120_resume)};

struct platform_driver bt1120_platform_driver = {
	.probe	= bt1120_probe,
	.remove = bt1120_remove,
	.driver =
		{
			.name		= "vs-bt1120",
			.of_match_table = of_match_ptr(bt1120_driver_dt_match),
			.pm		= &bt1120_pm_ops,
		},
};

MODULE_DESCRIPTION("VeriSilicon BT1120 Driver");
MODULE_LICENSE("GPL");
