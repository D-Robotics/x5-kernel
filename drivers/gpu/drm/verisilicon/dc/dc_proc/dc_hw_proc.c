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

#include <linux/bits.h>
#include <linux/media-bus-format.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include <drm/drm_writeback.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_vblank.h>
#include <drm/drm_print.h>

#include "vs_dc.h"
#include "vs_plane.h"
#include "vs_writeback.h"
#include "vs_crtc.h"
#include "dc_info.h"
#include "dc_proc.h"
#include "dc_hw_proc.h"
#include "dc_hw_type.h"
#include "vs_fb.h"
#include "vs_gem.h"

static const struct drm_prop_enum_list vs_gamut_conv_list[] = {
	{DC_GAMUT_CONV_BYPASS, "Bypass"},
	{DC_GAMUT_CONV_BT709_TO_BT2020, "BT709_BT2020"},
	{DC_GAMUT_CONV_BT2020_TO_BT709, "BT2020_BT709"},
	{DC_GAMUT_CONV_USER, "User"},
};

static const struct drm_prop_enum_list vs_wb_position[] = {
	{DC_WB_BEFORE_DITHER, "blend_out"},
	{DC_WB_BACKEND, "backend_out"},
};

static void update_format(u32 drm_format, struct dc_hw_fb *fb,
			  const struct vs_color_format_mapping *map)
{
	u8 i;

	if (!map)
		return;

	for (i = 0;; i++) {
		if (map[i].drm_format != drm_format && map[i].drm_format != DRM_FORMAT_TABLE_END)
			continue;

		/* use the first format as default */
		if (map[i].drm_format == DRM_FORMAT_TABLE_END)
			i = 0;

		fb->format = map[i].dc_format;
		fb->is_rgb = is_rgb(map[i].drm_format);
		if (fb->is_rgb) {
			fb->swizzle    = map[i].dc_swizzle;
			fb->uv_swizzle = 0;
		} else {
			fb->swizzle    = 0;
			fb->uv_swizzle = map[i].dc_swizzle;
		}
		return;
	}
}

static void update_modifier(u64 drm_modifier, struct dc_hw_fb *fb,
			    const struct vs_modifier_mapping *map)
{
	u8 i;

	if (!map)
		return;

	for (i = 0;; i++) {
		if (map[i].drm_modifier != drm_modifier &&
		    map[i].drm_modifier != DRM_MODIFIER_TABLE_END)
			continue;

		fb->tile_mode	  = map[i].dc_tile_mode;
		fb->compress_mode = map[i].dc_compress_mode;
		return;
	}
}

static u8 to_vs_rotation(u8 drm_rotation, const struct vs_rotation_mapping *map)
{
	u8 i;

	if (!map)
		return 0;

	drm_rotation &= (DRM_MODE_ROTATE_MASK | DRM_MODE_REFLECT_MASK);

	for (i = 0;; i++) {
		if (map[i].drm_rotation == drm_rotation ||
		    map[i].drm_rotation == DRM_ROTATION_TABLE_END)
			return map[i].dc_rotation;
	}
}

static u8 to_vs_gamut(u8 drm_color_gamut, const struct vs_color_gamut_mapping *map)
{
	u8 i;

	if (!map)
		return 0;

	for (i = 0;; i++) {
		if (map[i].drm_color_gamut == drm_color_gamut ||
		    map[i].drm_color_gamut == DRM_GAMUT_TABLE_END)
			return map[i].dc_color_gamut;
	}
}

static inline u8 to_vs_color_range(u32 color_range)
{
	u8 cr;

	switch (color_range) {
	case DRM_COLOR_YCBCR_LIMITED_RANGE:
		cr = DC_LIMITED_RANGE;
		break;
	case DRM_COLOR_YCBCR_FULL_RANGE:
		cr = DC_FULL_RANGE;
		break;
	default:
		cr = DC_LIMITED_RANGE;
		break;
	}

	return cr;
}

static inline enum dc_hw_bus_format to_vs_bus_fmt(u32 bus_format)
{
	enum dc_hw_bus_format fmt;

	switch (bus_format) {
	case MEDIA_BUS_FMT_RGB565_1X16:
		fmt = DC_BUS_FMT_RGB565_1;
		break;
	case MEDIA_BUS_FMT_RGB666_1X18:
		fmt = DC_BUS_FMT_RGB666_PACKED;
		break;
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
		fmt = DC_BUS_FMT_RGB666;
		break;
	case MEDIA_BUS_FMT_RGB888_1X24:
		fmt = DC_BUS_FMT_RGB888;
		break;
	case MEDIA_BUS_FMT_RGB101010_1X30:
		fmt = DC_BUS_FMT_RGB101010;
		break;
	case MEDIA_BUS_FMT_UYVY8_1X16:
		fmt = DC_BUS_FMT_YUV8;
		break;
	case MEDIA_BUS_FMT_YUV8_1X24:
		fmt = DC_BUS_FMT_RGB888;
		break;
	case MEDIA_BUS_FMT_UYVY10_1X20:
		fmt = DC_BUS_FMT_UYVY10;
		break;
	case MEDIA_BUS_FMT_YUV10_1X30:
		fmt = DC_BUS_FMT_YUV10;
		break;
	default:
		fmt = DC_BUS_FMT_RGB888;
		break;
	}

	return fmt;
}

static u8 fb_get_color_plane_number(struct drm_framebuffer *fb)
{
	const struct drm_format_info *info;

	if (!fb)
		return 0;

	info = drm_format_info(fb->format->format);
	if (!info || info->num_planes > MAX_NUM_PLANES)
		return 0;

	return info->num_planes;
}

void get_buffer_addr(struct drm_framebuffer *drm_fb, dma_addr_t dma_addr[])
{
	u8 num_planes, i;

	num_planes = fb_get_color_plane_number(drm_fb);

	for (i = 0; i < num_planes; i++) {
		struct vs_gem_object *vs_obj;

		vs_obj	    = vs_fb_get_gem_obj(drm_fb, i);
		dma_addr[i] = vs_obj->dma_addr + drm_fb->offsets[i];
	}
}

static void update_stride(struct drm_framebuffer *drm_fb, struct dc_hw_fb *fb)
{
	if (drm_fb->format->format != DRM_FORMAT_RGB888 &&
	    drm_fb->format->format != DRM_FORMAT_BGR888)
		return;

	fb->y_stride = fb->y_stride / 3 * 4;
}

static void update_fb(struct drm_framebuffer *drm_fb, struct dc_hw_fb *fb,
		      const struct vs_color_format_mapping *format_map,
		      const struct vs_modifier_mapping *modifier_map)
{
	dma_addr_t dma_addr[MAX_NUM_PLANES] = {};

	get_buffer_addr(drm_fb, dma_addr);

	fb->y_address = dma_addr[0];
	fb->u_address = dma_addr[1];
	fb->v_address = dma_addr[2];
	fb->y_stride  = drm_fb->pitches[0];
	fb->u_stride  = drm_fb->pitches[1];
	fb->v_stride  = drm_fb->pitches[2];
	fb->width     = drm_fb->width;
	fb->height    = drm_fb->height;

	update_stride(drm_fb, fb);

	update_format(drm_fb->format->format, fb, format_map);
	update_modifier(drm_fb->modifier, fb, modifier_map);
}

static int update_gamma_blob(struct drm_property_blob *blob, u8 bits, struct dc_hw_gamma *gamma)
{
	struct drm_color_lut *lut;
	struct dc_hw_gamma_data *data;
	u16 i;

	if (!blob || !blob->length)
		return -EINVAL;

	lut	    = blob->data;
	gamma->size = blob->length / sizeof(*lut);
	data	    = kmalloc_array(gamma->size, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	gamma->data = data;
	for (i = 0; i < gamma->size; i++) {
		data[i].r = drm_color_lut_extract(lut[i].red, bits);
		data[i].g = drm_color_lut_extract(lut[i].green, bits);
		data[i].b = drm_color_lut_extract(lut[i].blue, bits);
	}

	return 0;
}

static void update_degamma(struct dc_hw_processor *plane, const struct dc_hw_plane_info *plane_info,
			   struct dc_plane_proc_state *plane_state)
{
	struct drm_property_blob *blob;
	struct dc_hw_gamma degamma;
	u8 bits;

	blob = plane_state->degamma_lut;
	bits = plane_info->degamma_bits;
	if (update_gamma_blob(blob, bits, &degamma)) {
		dc_hw_update(plane, DC_PROP_PLANE_DEGAMMA, NULL);
	} else {
		dc_hw_update(plane, DC_PROP_PLANE_DEGAMMA, &degamma);
		kfree(degamma.data);
	}
}

static inline bool is_supported(const struct dc_hw_proc_info *info, u32 feature)
{
	return !!(feature & info->features);
}

static void update_plane(struct dc_proc *dc_proc, struct drm_plane_state *old_state)
{
	struct dc_plane_proc *plane_proc	  = to_dc_plane_proc(dc_proc);
	struct dc_hw_processor *plane		  = &plane_proc->plane;
	const struct dc_hw_plane_info *plane_info = to_dc_hw_plane_info(plane->info);
	struct vs_plane *vs_plane		  = proc_to_vs_plane(dc_proc);
	struct drm_plane_state *state		  = vs_plane->base.state;
	struct dc_plane_proc_state *hw_plane_state =
		to_dc_plane_proc_state(get_plane_proc_state(state, dc_proc));
	struct dc_crtc *dc_crtc	     = to_dc_crtc(to_vs_crtc(state->crtc));
	struct dc_proc *dc_crtc_proc = find_dc_proc_crtc(dc_crtc, dc_proc->info->name);
	struct vs_crtc *vs_crtc;
	struct drm_rect *src	       = &state->src;
	struct drm_rect *dst	       = &state->dst;
	struct drm_framebuffer *drm_fb = state->fb;
	struct dc_hw_fb fb;
	struct dc_hw_win win;
	struct dc_hw_blend blend;
	struct dc_hw_gamut_conv gamut_conv;

	if (!dc_crtc_proc || !hw_plane_state)
		return;

	vs_crtc = proc_to_vs_crtc(dc_crtc_proc);

	if (is_supported(&plane_info->info, DC_PLANE_DEGAMMA) && hw_plane_state->degamma_changed) {
		update_degamma(plane, plane_info, hw_plane_state);
		hw_plane_state->degamma_changed = false;
	}

	if (is_supported(&plane_info->info, DC_PLANE_RGB2RGB) &&
	    hw_plane_state->gamut_conv_changed) {
		gamut_conv.gamut_conv  = hw_plane_state->gamut_conv;
		gamut_conv.conv_matrix = hw_plane_state->gamut_conv_matrix ?
						 hw_plane_state->gamut_conv_matrix->data :
						 NULL;
		dc_hw_update(plane, DC_PROP_PLANE_GAMUT_CONV, &gamut_conv);
		hw_plane_state->gamut_conv_changed = false;
	}

	if (is_supported(&plane_info->info, DC_PLANE_ASSIGN) && state->crtc != old_state->crtc)
		dc_hw_update(plane, DC_PROP_PLANE_ASSIGN, (void *)&vs_crtc->info->id);

	update_fb(drm_fb, &fb, plane_info->format, plane_info->modifier);

	fb.rotation    = to_vs_rotation(state->rotation, plane_info->rotation);
	fb.color_range = to_vs_color_range(state->color_range);
	fb.color_gamut = to_vs_gamut(state->color_encoding, plane_info->gamut);

	win.src_x = src->x1 >> 16;
	win.src_y = src->y1 >> 16;
	win.src_w = drm_rect_width(src) >> 16;
	win.src_h = drm_rect_height(src) >> 16;
	win.dst_x = dst->x1;
	win.dst_y = dst->y1;
	win.dst_w = drm_rect_width(dst);
	win.dst_h = drm_rect_height(dst);

	blend.alpha	 = (u8)(state->alpha >> 8);
	blend.blend_mode = (u8)(state->pixel_blend_mode);

	dc_hw_update(plane, DC_PROP_PLANE_FB, &fb);
	dc_hw_update(plane, DC_PROP_PLANE_WIN, &win);
	dc_hw_update(plane, DC_PROP_PLANE_BLEND, &blend);
	dc_hw_update(plane, DC_PROP_PLANE_ZPOS, &state->normalized_zpos);
}

static void update_cursor_plane(struct dc_proc *dc_proc)
{
	struct dc_plane_proc *hw_plane = to_dc_plane_proc(dc_proc);
	struct vs_plane *vs_plane      = proc_to_vs_plane(dc_proc);
	struct drm_plane_state *state  = vs_plane->base.state;
	struct drm_framebuffer *drm_fb = state->fb;
	const struct dc_plane_proc_state *hw_plane_state =
		to_dc_plane_proc_state(get_plane_proc_state(state, dc_proc));
	struct dc_hw_cursor cursor;
	u16 width, height;
	dma_addr_t dma_addr[MAX_NUM_PLANES] = {};

	get_buffer_addr(drm_fb, dma_addr);

	width  = state->src_w >> 16;
	height = state->src_h >> 16;

	if (width > 128 || height > 128)
		cursor.size = DC_CURSOR_SIZE_256X256;
	else if ((width > 64) || (height > 64))
		cursor.size = DC_CURSOR_SIZE_128X128;
	else if ((width > 32) || (height > 32))
		cursor.size = DC_CURSOR_SIZE_64X64;
	else
		cursor.size = DC_CURSOR_SIZE_32X32;

	cursor.address = dma_addr[0];
	cursor.x       = state->crtc_x;
	cursor.y       = state->crtc_y;
	cursor.hot_x   = drm_fb->hot_x;
	cursor.hot_y   = drm_fb->hot_y;
	cursor.masked  = hw_plane_state->cursor_masked;

	dc_hw_update(&hw_plane->plane, DC_PROP_PLANE_CURSOR, &cursor);
}

static void dc_hw_proc_disable_plane(struct dc_proc *dc_proc, void *old_state)
{
	struct dc_plane_proc *hw_plane = to_dc_plane_proc(dc_proc);

	dc_hw_disable(&hw_plane->plane);
}

static void dc_hw_proc_update_plane(struct dc_proc *dc_proc, void *old_drm_plane_state)
{
	struct vs_plane *vs_plane = proc_to_vs_plane(dc_proc);

	switch (vs_plane->base.type) {
	case DRM_PLANE_TYPE_PRIMARY:
	case DRM_PLANE_TYPE_OVERLAY:
		update_plane(dc_proc, old_drm_plane_state);
		break;
	case DRM_PLANE_TYPE_CURSOR:
		update_cursor_plane(dc_proc);
		break;
	default:
		break;
	}
}

static int dc_hw_proc_check_plane(struct dc_proc *dc_proc, void *_state)
{
	struct drm_plane_state *state		  = _state;
	const struct dc_hw_plane_info *plane_info = dc_proc->info->info;
	const struct dc_plane_proc_state *hw_plane_state =
		to_dc_plane_proc_state(get_plane_proc_state(state, dc_proc));

	if (is_supported(&plane_info->info, DC_PLANE_RGB2RGB) &&
	    hw_plane_state->gamut_conv == DC_GAMUT_CONV_USER && !hw_plane_state->gamut_conv_matrix)
		return -EINVAL;

	if (!state->fb)
		return 0;

	return 0;
}

static void dc_hw_proc_commit_plane(struct dc_proc *dc_proc)
{
	struct dc_plane_proc *hw_plane = to_dc_plane_proc(dc_proc);

	dc_hw_commit(&hw_plane->plane);
}

static void dc_hw_proc_destroy_plane(struct dc_proc *dc_proc)
{
	struct dc_plane_proc *hw_plane = to_dc_plane_proc(dc_proc);

	kfree(hw_plane);
}

static void dc_hw_proc_resume_plane(struct dc_proc *dc_proc)
{
	struct dc_plane_proc *hw_plane = to_dc_plane_proc(dc_proc);

	dc_hw_setup(&hw_plane->plane);
}

static void dc_hw_proc_destroy_plane_state(struct dc_proc_state *dc_state)
{
	struct dc_plane_proc_state *state = to_dc_plane_proc_state(dc_state);

	drm_property_blob_put(state->degamma_lut);
	drm_property_blob_put(state->gamut_conv_matrix);

	kfree(state);
}

static struct dc_proc_state *dc_hw_proc_create_plane_state(void *parent, struct dc_proc *dc_proc)
{
	struct dc_plane_proc_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	INIT_LIST_HEAD(&state->base.head);

	state->parent = parent;

	return &state->base;
}

static struct dc_proc_state *dc_hw_proc_duplicate_plane_state(void *parent, struct dc_proc *dc_proc,
							      void *_dc_state)
{
	struct dc_proc_state *dc_state	      = _dc_state;
	struct dc_plane_proc_state *ori_state = to_dc_plane_proc_state(dc_state);
	struct dc_plane_proc_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (WARN_ON(!state))
		return NULL;

	memcpy(state, ori_state, sizeof(*state));

	if (state->degamma_lut)
		drm_property_blob_get(state->degamma_lut);

	if (state->gamut_conv_matrix)
		drm_property_blob_get(state->gamut_conv_matrix);

	INIT_LIST_HEAD(&state->base.head);

	state->parent = (struct dc_plane_state *)parent;

	return &state->base;
}

static int dc_hw_proc_create_plane_prop(struct dc_proc *dc_proc)
{
	struct dc_plane_proc *hw_plane	    = to_dc_plane_proc(dc_proc);
	const struct dc_hw_plane_info *info = to_dc_hw_plane_info(hw_plane->plane.info);
	struct drm_plane *plane		    = to_drm_plane(dc_proc);
	struct vs_drm_private *priv	    = drm_to_vs_priv(plane->dev);
	struct drm_mode_config *config	    = &plane->dev->mode_config;

	if (is_supported(&info->info, DC_PLANE_RGB2RGB)) {
		if (!priv->gamut_conv) {
			priv->gamut_conv = drm_property_create_enum(plane->dev, 0, "GAMUT_CONV",
								    vs_gamut_conv_list,
								    ARRAY_SIZE(vs_gamut_conv_list));
			if (!priv->gamut_conv)
				return -EINVAL;
		}
		drm_object_attach_property(&plane->base, priv->gamut_conv, DC_GAMUT_CONV_BYPASS);

		if (!priv->gamut_conv_matrix) {
			priv->gamut_conv_matrix = drm_property_create(
				plane->dev, DRM_MODE_PROP_BLOB, "GAMUT_CONV_MATRIX", 0);

			if (!priv->gamut_conv_matrix)
				return -EINVAL;
		}
		drm_object_attach_property(&plane->base, priv->gamut_conv_matrix, 0);
	}

	if (is_supported(&info->info, DC_PLANE_DEGAMMA) && info->degamma_size) {
		drm_object_attach_property(&plane->base, config->degamma_lut_property, 0);

		drm_object_attach_property(&plane->base, config->degamma_lut_size_property,
					   info->degamma_size);
	}

	if (is_supported(&info->info, DC_PLANE_CURSOR_MASKED)) {
		if (!priv->cursor_mask) {
			priv->cursor_mask =
				drm_property_create_bool(plane->dev, 0, "CURSOR_MASKED");
			if (!priv->cursor_mask)
				return -EINVAL;
		}
		drm_object_attach_property(&plane->base, priv->cursor_mask, 0);
	}

	return 0;
}

static int dc_hw_proc_set_plane_prop(struct dc_proc *dc_proc, struct dc_proc_state *dc_state,
				     struct drm_property *property, uint64_t val)
{
	struct dc_plane_proc_state *state    = to_dc_plane_proc_state(dc_state);
	struct dc_plane_proc *hw_plane	     = to_dc_plane_proc(dc_proc);
	const struct dc_hw_plane_info *info  = to_dc_hw_plane_info(hw_plane->plane.info);
	struct drm_plane *plane		     = to_drm_plane(dc_proc);
	const struct vs_drm_private *priv    = drm_to_vs_priv(plane->dev);
	const struct drm_mode_config *config = &plane->dev->mode_config;
	bool replaced			     = false;
	int ret;

	if (property == priv->gamut_conv) {
		state->gamut_conv	  = (u8)val;
		state->gamut_conv_changed = true;
	} else if (property == config->degamma_lut_property) {
		ret = dc_replace_property_blob_from_id(
			plane->dev, &state->degamma_lut, val,
			info->degamma_size * sizeof(struct drm_color_lut),
			sizeof(struct drm_color_lut), &replaced);
		state->degamma_changed = replaced;
		return ret;
	} else if (property == priv->gamut_conv_matrix) {
		ret = dc_replace_property_blob_from_id(plane->dev, &state->gamut_conv_matrix, val,
						       9 * sizeof(s16), -1, &replaced);
		state->gamut_conv_changed = replaced;
		return ret;
	} else if (property == priv->cursor_mask) {
		state->cursor_masked = !!val;
		return 0;
	} else {
		return -EAGAIN;
	}

	return 0;
}

static int dc_hw_proc_get_plane_prop(struct dc_proc *dc_proc, struct dc_proc_state *dc_state,
				     struct drm_property *property, uint64_t *val)
{
	struct dc_plane_proc_state *state    = to_dc_plane_proc_state(dc_state);
	struct drm_plane *plane		     = to_drm_plane(dc_proc);
	const struct vs_drm_private *priv    = drm_to_vs_priv(plane->dev);
	const struct drm_mode_config *config = &plane->dev->mode_config;

	if (property == priv->gamut_conv)
		*val = state->gamut_conv;
	else if (property == config->degamma_lut_property)
		*val = (state->degamma_lut) ? state->degamma_lut->base.id : 0;
	else if (property == priv->gamut_conv_matrix)
		*val = (state->gamut_conv_matrix) ? state->gamut_conv_matrix->base.id : 0;
	else if (property == priv->cursor_mask)
		*val = state->cursor_masked;
	else
		return -EAGAIN;

	return 0;
}

static void dc_hw_proc_plane_print_state(struct drm_printer *p, struct dc_proc_state *dc_state)
{
	struct dc_plane_proc_state *state = to_dc_plane_proc_state(dc_state);

	drm_printf(p, "\tRGB Conv Gamut=%s\n", vs_gamut_conv_list[state->gamut_conv].name);
}

static int dc_hw_proc_check_format_mod(struct dc_proc *dc_proc, u32 format, u64 modifier)
{
	struct dc_plane_proc *hw_plane	    = to_dc_plane_proc(dc_proc);
	const struct dc_hw_plane_info *info = to_dc_hw_plane_info(hw_plane->plane.info);

	if (modifier == DRM_FORMAT_MOD_LINEAR)
		return true;

	return is_supported_format(info->mod_format_map, format, modifier);
}

static void dc_hw_proc_update_plane_info(struct dc_proc *dc_proc, void *plane_info)
{
	const struct dc_proc_info *proc_info   = dc_proc->info;
	struct vs_plane_info *info	       = plane_info;
	const struct dc_hw_plane_info *hw_info = proc_info->info;
	u8 total, copied;
	void *tmp;

	if (!info->num_modifiers || !hw_info->mod_format_map)
		return;

	total = get_modifier(hw_info->mod_format_map, NULL, 0);
	if (!total)
		return;

	pr_debug("dc modifier cnt = %d\n", total);
	tmp = kzalloc((info->num_modifiers + total) * sizeof(u64), GFP_KERNEL);
	WARN_ON(!tmp);

	memcpy(tmp, info->modifiers, (info->num_modifiers - 1) * sizeof(u64));

	if (info->num_modifiers > NUM_MODIFIERS)
		kfree(info->modifiers);

	info->modifiers = tmp;

	copied = get_modifier(hw_info->mod_format_map, info->modifiers + info->num_modifiers - 1,
			      total);

	WARN_ON(copied != total);

	info->num_modifiers += total;

	info->modifiers[info->num_modifiers - 1] = DRM_FORMAT_MOD_INVALID;
}

static struct dc_proc *dc_hw_create_plane(struct device *dev, const struct dc_proc_info *info)
{
	struct vs_dc *dc			  = dev_get_drvdata(dev);
	struct dc_hw *hw			  = dc->hw;
	const struct dc_hw_plane_info *plane_info = (const struct dc_hw_plane_info *)info->info;
	struct dc_plane_proc *hw_plane;
	int ret;

	hw_plane = kzalloc(sizeof(*hw_plane), GFP_KERNEL);
	if (!hw_plane)
		return ERR_PTR(-ENOMEM);

	ret = dc_hw_init(&hw_plane->plane, hw, &plane_info->info);
	if (ret) {
		kfree(hw_plane);
		return ERR_PTR(ret);
	}

	dc_hw_setup(&hw_plane->plane);

	INIT_LIST_HEAD(&hw_plane->base.head);

	return &hw_plane->base;
}

const struct dc_proc_funcs dc_hw_plane_funcs = {
	.create		  = dc_hw_create_plane,
	.update		  = dc_hw_proc_update_plane,
	.update_info	  = dc_hw_proc_update_plane_info,
	.disable	  = dc_hw_proc_disable_plane,
	.check		  = dc_hw_proc_check_plane,
	.commit		  = dc_hw_proc_commit_plane,
	.destroy	  = dc_hw_proc_destroy_plane,
	.resume		  = dc_hw_proc_resume_plane,
	.destroy_state	  = dc_hw_proc_destroy_plane_state,
	.create_state	  = dc_hw_proc_create_plane_state,
	.duplicate_state  = dc_hw_proc_duplicate_plane_state,
	.create_prop	  = dc_hw_proc_create_plane_prop,
	.set_prop	  = dc_hw_proc_set_plane_prop,
	.get_prop	  = dc_hw_proc_get_plane_prop,
	.print_state	  = dc_hw_proc_plane_print_state,
	.check_format_mod = dc_hw_proc_check_format_mod,
};

static struct dc_crtc_proc_state *get_hw_disp_state(struct dc_proc *dc_proc)
{
	struct drm_crtc *crtc = to_drm_crtc(dc_proc);
	struct dc_proc_state *state;

	state = get_crtc_proc_state(crtc->state, dc_proc);

	return to_dc_crtc_proc_state(state);
}

static void dc_hw_proc_enable_disp(struct dc_proc *dc_proc)
{
	struct dc_crtc_proc *hw_disp	 = to_dc_crtc_proc(dc_proc);
	struct dc_hw_processor *display	 = &hw_disp->disp;
	struct dc_crtc_proc_state *state = get_hw_disp_state(dc_proc);
	struct dc_crtc_state *dc_crtc_state;
	struct drm_display_mode *mode;
	struct dc_hw_scan scan;
	struct dc_hw_output out;
	int ret;

	if (WARN_ON(!state))
		return;

	dc_crtc_state = state->parent;
	mode	      = &dc_crtc_state->base.adjusted_mode;

	ret = clk_prepare_enable(hw_disp->pix_clk);
	if (ret < 0) {
		pr_err("failed to prepare/enable pix_clk\n");
		return;
	}

	if (state->pix_clk_rate != mode->clock) {
		clk_set_rate(hw_disp->pix_clk, (unsigned long)(mode->clock) * 1000);
		state->pix_clk_rate = mode->clock;
	}

	out.bus_format = to_vs_bus_fmt(dc_crtc_state->output_fmt);
	out.gamut      = DC_OUT_GAMUT_BT2020;
	out.port       = hw_disp->out_port;

	scan.h_active	  = mode->hdisplay;
	scan.h_total	  = mode->htotal;
	scan.h_sync_start = mode->hsync_start;
	scan.h_sync_end	  = mode->hsync_end;
	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		scan.h_sync_polarity = true;
	else
		scan.h_sync_polarity = false;

	scan.v_active	  = mode->vdisplay;
	scan.v_total	  = mode->vtotal;
	scan.v_sync_start = mode->vsync_start;
	scan.v_sync_end	  = mode->vsync_end;
	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		scan.v_sync_polarity = true;
	else
		scan.v_sync_polarity = false;

	dc_hw_update(display, DC_PROP_DISP_OUTPUT, &out);
	dc_hw_update(display, DC_PROP_DISP_SCAN, &scan);
}

static void dc_hw_proc_disable_disp(struct dc_proc *dc_proc, void *_state)
{
	struct dc_crtc_proc *hw_disp	= to_dc_crtc_proc(dc_proc);
	struct dc_hw_processor *display = &hw_disp->disp;

	dc_hw_disable(display);

	clk_disable_unprepare(hw_disp->pix_clk);
}

static bool dc_hw_proc_mode_fixup(struct dc_proc *dc_proc, const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	struct dc_crtc_proc *hw_disp = to_dc_crtc_proc(dc_proc);
	long clk_rate;

	if (hw_disp->pix_clk) {
		clk_rate = clk_round_rate(hw_disp->pix_clk, adjusted_mode->clock * 1000);
		adjusted_mode->clock = DIV_ROUND_UP(clk_rate, 1000);
	}

	return true;
}

static void dc_hw_proc_update_disp(struct dc_proc *dc_proc, void *old_state)
{
	struct dc_crtc_proc *hw_disp	= to_dc_crtc_proc(dc_proc);
	struct dc_hw_processor *display = &hw_disp->disp;
	struct dc_hw_disp_info *info	= to_dc_hw_disp_info(display->info);
	struct vs_crtc *crtc		= proc_to_vs_crtc(dc_proc);
	struct drm_property_blob *blob;
	struct dc_hw_gamma gamma;
	struct dc_crtc_proc_state *hw_disp_state =
		to_dc_crtc_proc_state(get_crtc_proc_state(crtc->base.state, dc_proc));
	u8 bits;

	if (WARN_ON(!hw_disp_state))
		return;

	if (crtc->base.state->color_mgmt_changed) {
		blob = crtc->base.state->gamma_lut;
		bits = (u8)crtc->info->gamma_bits;
		if (update_gamma_blob(blob, bits, &gamma)) {
			dc_hw_update(display, DC_PROP_DISP_GAMMA, NULL);
		} else {
			dc_hw_update(display, DC_PROP_DISP_GAMMA, &gamma);
			kfree(gamma.data);
		}
	}

	if (hw_disp_state->lcsc0_changed) {
		blob = hw_disp_state->lcsc0_degamma;
		bits = (u8)info->lcsc_degamma_bits;
		if (update_gamma_blob(blob, bits, &gamma)) {
			dc_hw_update(display, DC_PROP_DISP_LCSC0_DEGAMMA, NULL);
		} else {
			dc_hw_update(display, DC_PROP_DISP_LCSC0_DEGAMMA, &gamma);
			kfree(gamma.data);
		}

		blob = hw_disp_state->lcsc0_csc;
		if ((blob) && blob->length)
			dc_hw_update(display, DC_PROP_DISP_LCSC0_CSC, blob->data);
		else
			dc_hw_update(display, DC_PROP_DISP_LCSC0_CSC, NULL);

		blob = hw_disp_state->lcsc0_regamma;
		bits = (u8)info->lcsc_degamma_bits;
		if (update_gamma_blob(blob, bits, &gamma)) {
			dc_hw_update(display, DC_PROP_DISP_LCSC0_REGAMMA, NULL);
		} else {
			dc_hw_update(display, DC_PROP_DISP_LCSC0_REGAMMA, &gamma);
			kfree(gamma.data);
		}
	}

	if (hw_disp_state->lcsc1_changed) {
		blob = hw_disp_state->lcsc1_degamma;
		bits = (u8)info->lcsc_degamma_bits;
		if (update_gamma_blob(blob, bits, &gamma)) {
			dc_hw_update(display, DC_PROP_DISP_LCSC1_DEGAMMA, NULL);
		} else {
			dc_hw_update(display, DC_PROP_DISP_LCSC1_DEGAMMA, &gamma);
			kfree(gamma.data);
		}

		blob = hw_disp_state->lcsc1_csc;
		if ((blob) && blob->length)
			dc_hw_update(display, DC_PROP_DISP_LCSC1_CSC, blob->data);
		else
			dc_hw_update(display, DC_PROP_DISP_LCSC1_CSC, NULL);

		blob = hw_disp_state->lcsc1_regamma;
		bits = (u8)info->lcsc_degamma_bits;
		if (update_gamma_blob(blob, bits, &gamma)) {
			dc_hw_update(display, DC_PROP_DISP_LCSC1_REGAMMA, NULL);
		} else {
			dc_hw_update(display, DC_PROP_DISP_LCSC1_REGAMMA, &gamma);
			kfree(gamma.data);
		}
	}

	if (hw_disp_state->bg_color_changed) {
		dc_hw_update(display, DC_PROP_DISP_BG_COLOR, &hw_disp_state->bg_color);
		hw_disp_state->bg_color_changed = false;
	}

	if (hw_disp_state->dither_changed) {
		blob = hw_disp_state->dither;
		if ((blob) && blob->length)
			dc_hw_update(display, DC_PROP_DISP_DITHER, blob->data);
		else
			dc_hw_update(display, DC_PROP_DISP_DITHER, NULL);

		hw_disp_state->dither_changed = false;
	}
}

static void dc_hw_proc_enable_vblank(struct dc_proc *dc_proc, bool enable)
{
	struct dc_crtc_proc *hw_disp	= to_dc_crtc_proc(dc_proc);
	struct dc_hw_processor *display = &hw_disp->disp;

	dc_hw_enable_vblank(display, enable);
}

static void dc_hw_proc_enable_crc(struct dc_proc *dc_proc, bool enable)
{
	struct dc_crtc_proc *hw_disp	= to_dc_crtc_proc(dc_proc);
	struct drm_crtc *crtc		= to_drm_crtc(dc_proc);
	struct dc_hw_processor *display = &hw_disp->disp;

	if (!(display->info->features & DC_DISPLAY_CRC))
		return;

	if (!hw_disp->crc_enable && enable)
		drm_crtc_vblank_get(crtc);
	else if (hw_disp->crc_enable && !enable)
		drm_crtc_vblank_put(crtc);

	hw_disp->crc_skip_count = 0;

	hw_disp->crc_enable = enable;

	dc_hw_update(display, DC_PROP_DISP_CRC, &enable);
}

static void get_crc(struct dc_crtc_proc *hw_disp)
{
	struct dc_hw_processor *display = &hw_disp->disp;
	struct vs_crtc *vs_crtc		= proc_to_vs_crtc(&hw_disp->base);
	u32 crc;
	int ret;

	if (!hw_disp->crc_enable)
		return;

	if (hw_disp->crc_skip_count < 1) {
		hw_disp->crc_skip_count++;
		return;
	}

	ret = dc_hw_get(display, DC_PROP_DISP_CRC, &crc, sizeof(crc));
	if (ret) {
		DRM_DEV_ERROR(vs_crtc->base.dev->dev, "failed to get crtc\n");
		return;
	}

	drm_crtc_add_crc_entry(&vs_crtc->base, false, 0, &crc);
}

static void hw_display_work(struct work_struct *work)
{
	struct dc_crtc_proc *hw_disp	      = container_of(work, struct dc_crtc_proc, work);
	const struct dc_hw_processor *display = &hw_disp->disp;

	if (is_supported(display->info, DC_DISPLAY_CRC))
		get_crc(hw_disp);
}

static void dc_hw_proc_vblank(struct dc_proc *dc_proc)
{
	struct dc_crtc_proc *hw_disp = to_dc_crtc_proc(dc_proc);
	struct vs_crtc *vs_crtc	     = proc_to_vs_crtc(&hw_disp->base);
	struct dc_crtc_proc_state *state =
		to_dc_crtc_proc_state(get_crtc_proc_state(vs_crtc->base.state, &hw_disp->base));
	struct dc_hw_processor *display = &hw_disp->disp;
	int ret;
	bool underflow = false;

	ret = dc_hw_get(display, DC_PROP_DISP_UNDERFLOW, &underflow, sizeof(underflow));
	if (ret)
		DRM_DEV_ERROR(vs_crtc->base.dev->dev, "failed to get underflow count\n");

	if (underflow)
		state->underflow_cnt++;

	if (!hw_disp->crc_enable)
		return;

	schedule_work(&hw_disp->work);
}

static void dc_hw_proc_commit_disp(struct dc_proc *dc_proc)
{
	struct dc_crtc_proc *hw_disp	= to_dc_crtc_proc(dc_proc);
	struct dc_hw_processor *display = &hw_disp->disp;

	dc_hw_commit(display);
}

static int dc_hw_proc_check_disp(struct dc_proc *dc_proc, void *state)
{
	struct drm_crtc_state *new_state       = state;
	struct vs_crtc *vs_crtc		       = proc_to_vs_crtc(dc_proc);
	const struct drm_crtc *crtc	       = &vs_crtc->base;
	const struct drm_crtc_state *old_state = crtc->state;

	if (new_state->gamma_lut && new_state->gamma_lut != old_state->gamma_lut &&
	    (new_state->gamma_lut->length != crtc->gamma_size * sizeof(struct drm_color_lut)))
		return -EINVAL;

	return 0;
}

static void dc_hw_proc_destroy_disp(struct dc_proc *dc_proc)
{
	struct dc_crtc_proc *hw_disp = to_dc_crtc_proc(dc_proc);

	clk_put(hw_disp->pix_clk);
	kfree(hw_disp);
}

static void dc_hw_proc_resume_disp(struct dc_proc *dc_proc)
{
	struct dc_crtc_proc *hw_disp = to_dc_crtc_proc(dc_proc);

	dc_hw_setup(&hw_disp->disp);
}

static void dc_hw_proc_destroy_disp_state(struct dc_proc_state *dc_state)
{
	struct dc_crtc_proc_state *state = to_dc_crtc_proc_state(dc_state);

	drm_property_blob_put(state->lcsc0_degamma);
	drm_property_blob_put(state->lcsc0_csc);
	drm_property_blob_put(state->lcsc0_regamma);
	drm_property_blob_put(state->lcsc1_degamma);
	drm_property_blob_put(state->lcsc1_csc);
	drm_property_blob_put(state->lcsc1_regamma);

	kfree(state);
}

static struct dc_proc_state *dc_hw_proc_create_disp_state(void *parent, struct dc_proc *dc_proc)
{
	struct dc_crtc_proc *hw_disp = to_dc_crtc_proc(dc_proc);
	struct dc_crtc_proc_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	INIT_LIST_HEAD(&state->base.head);
	state->pix_clk_rate = clk_get_rate(hw_disp->pix_clk) / 1000;
	state->parent	    = parent;

	return (struct dc_proc_state *)state;
}

static struct dc_proc_state *dc_hw_proc_duplicate_disp_state(void *parent, struct dc_proc *dc_proc,
							     void *_state)
{
	struct dc_proc_state *dc_state	     = _state;
	struct dc_crtc_proc_state *ori_state = to_dc_crtc_proc_state(dc_state);
	struct dc_crtc_proc_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (WARN_ON(!state))
		return NULL;

	memcpy(state, ori_state, sizeof(*state));

	INIT_LIST_HEAD(&state->base.head);
	if (state->lcsc0_degamma)
		drm_property_blob_get(state->lcsc0_degamma);

	if (state->lcsc0_csc)
		drm_property_blob_get(state->lcsc0_csc);

	if (state->lcsc0_regamma)
		drm_property_blob_get(state->lcsc0_regamma);

	if (state->lcsc1_degamma)
		drm_property_blob_get(state->lcsc1_degamma);

	if (state->lcsc1_csc)
		drm_property_blob_get(state->lcsc1_csc);

	if (state->lcsc1_regamma)
		drm_property_blob_get(state->lcsc1_regamma);

	if (state->dither)
		drm_property_blob_get(state->dither);

	state->parent = (struct dc_crtc_state *)parent;

	return &state->base;
}

static int dc_hw_proc_create_disp_prop(struct dc_proc *dc_proc)
{
	struct dc_crtc_proc *hw_disp	   = to_dc_crtc_proc(dc_proc);
	struct drm_crtc *crtc		   = to_drm_crtc(dc_proc);
	struct vs_drm_private *priv	   = drm_to_vs_priv(crtc->dev);
	struct dc_hw_disp_info *disp_info  = to_dc_hw_disp_info(hw_disp->disp.info);
	const struct dc_hw_proc_info *info = &disp_info->info;

	if (is_supported(info, DC_DISPLAY_BGCOLOR)) {
		if (!priv->bg_color) {
			priv->bg_color =
				drm_property_create_range(crtc->dev, 0, "BG_COLOR", 0, 0xffffffff);
			if (!priv->bg_color)
				return -EINVAL;
		}

		drm_object_attach_property(&crtc->base, priv->bg_color, 0);
	}

	if (is_supported(info, DC_DISPLAY_LCSC)) {
		if (!priv->lcsc0_degamma) {
			priv->lcsc0_degamma = drm_property_create(crtc->dev, DRM_MODE_PROP_BLOB,
								  "LCSC0 DEMAMMA", 0);
			if (!priv->lcsc0_degamma)
				return -EINVAL;
		}

		if (!priv->lcsc_degamma_size) {
			priv->lcsc_degamma_size =
				drm_property_create_range(crtc->dev, DRM_MODE_PROP_IMMUTABLE,
							  "LCSC_DEGAMMA_LUT_SIZE", 0, UINT_MAX);

			if (!priv->lcsc_degamma_size)
				return -EINVAL;
		}

		if (!priv->lcsc_regamma_size) {
			priv->lcsc_regamma_size =
				drm_property_create_range(crtc->dev, DRM_MODE_PROP_IMMUTABLE,
							  "LCSC_REGAMMA_LUT_SIZE", 0, UINT_MAX);

			if (!priv->lcsc_regamma_size)
				return -EINVAL;
		}

		if (!priv->lcsc0_csc) {
			priv->lcsc0_csc =
				drm_property_create(crtc->dev, DRM_MODE_PROP_BLOB, "LCSC0 CSC", 0);
			if (!priv->lcsc0_csc)
				return -EINVAL;
		}

		if (!priv->lcsc0_regamma) {
			priv->lcsc0_regamma = drm_property_create(crtc->dev, DRM_MODE_PROP_BLOB,
								  "LCSC0 REMAMMA", 0);
			if (!priv->lcsc0_regamma)
				return -EINVAL;
		}

		if (!priv->lcsc1_degamma) {
			priv->lcsc1_degamma = drm_property_create(crtc->dev, DRM_MODE_PROP_BLOB,
								  "LCSC1 DEMAMMA", 0);
			if (!priv->lcsc1_degamma)
				return -EINVAL;
		}

		if (!priv->lcsc1_csc) {
			priv->lcsc1_csc =
				drm_property_create(crtc->dev, DRM_MODE_PROP_BLOB, "LCSC1 CSC", 0);
			if (!priv->lcsc1_csc)
				return -EINVAL;
		}

		if (!priv->lcsc1_regamma) {
			priv->lcsc1_regamma = drm_property_create(crtc->dev, DRM_MODE_PROP_BLOB,
								  "LCSC1 REMAMMA", 0);
			if (!priv->lcsc1_regamma)
				return -EINVAL;
		}

		drm_object_attach_property(&crtc->base, priv->lcsc0_degamma, 0);
		drm_object_attach_property(&crtc->base, priv->lcsc0_csc, 0);
		drm_object_attach_property(&crtc->base, priv->lcsc0_regamma, 0);
		drm_object_attach_property(&crtc->base, priv->lcsc1_degamma, 0);
		drm_object_attach_property(&crtc->base, priv->lcsc1_csc, 0);
		drm_object_attach_property(&crtc->base, priv->lcsc1_regamma, 0);
		drm_object_attach_property(&crtc->base, priv->lcsc_degamma_size,
					   disp_info->lcsc_degamma_size);
		drm_object_attach_property(&crtc->base, priv->lcsc_regamma_size,
					   disp_info->lcsc_regamma_size);
	}

	if (is_supported(info, DC_DISPLAY_DITHER)) {
		if (!priv->dither) {
			priv->dither =
				drm_property_create(crtc->dev, DRM_MODE_PROP_BLOB, "DITHER", 0);

			if (!priv->dither)
				return -EINVAL;
		}

		drm_object_attach_property(&crtc->base, priv->dither, 0);
	}

	return 0;
}

static int dc_hw_proc_set_disp_prop(struct dc_proc *dc_proc, struct dc_proc_state *dc_state,
				    struct drm_property *property, uint64_t val)
{
	struct dc_crtc_proc_state *state  = to_dc_crtc_proc_state(dc_state);
	struct drm_crtc *crtc		  = to_drm_crtc(dc_proc);
	const struct vs_drm_private *priv = drm_to_vs_priv(crtc->dev);
	bool replaced			  = false;
	int ret;

	if (property == priv->bg_color) {
		state->bg_color_changed = true;
		state->bg_color		= (u32)val;
	} else if (property == priv->dither) {
		ret = dc_replace_property_blob_from_id(crtc->dev, &state->dither, val,
						       sizeof(u32) * 2, -1, &replaced);
		state->dither_changed = replaced;
		return ret;
	} else if (property == priv->lcsc0_degamma) {
		ret = dc_replace_property_blob_from_id(crtc->dev, &state->lcsc0_degamma, val, -1,
						       sizeof(struct drm_color_lut), &replaced);
		state->lcsc0_changed = replaced;
		return ret;
	} else if (property == priv->lcsc0_csc) {
		ret = dc_replace_property_blob_from_id(crtc->dev, &state->lcsc0_csc, val,
						       sizeof(struct dc_hw_lcsc_csc), -1,
						       &replaced);
		state->lcsc0_changed = replaced;
		return ret;
	} else if (property == priv->lcsc0_regamma) {
		ret = dc_replace_property_blob_from_id(crtc->dev, &state->lcsc0_regamma, val, -1,
						       sizeof(struct drm_color_lut), &replaced);
		state->lcsc0_changed = replaced;
		return ret;
	} else if (property == priv->lcsc1_degamma) {
		ret = dc_replace_property_blob_from_id(crtc->dev, &state->lcsc1_degamma, val, -1,
						       sizeof(struct drm_color_lut), &replaced);
		state->lcsc1_changed = replaced;
		return ret;
	} else if (property == priv->lcsc1_csc) {
		ret = dc_replace_property_blob_from_id(crtc->dev, &state->lcsc1_csc, val,
						       sizeof(struct dc_hw_lcsc_csc), -1,
						       &replaced);
		state->lcsc1_changed = replaced;
		return ret;
	} else if (property == priv->lcsc1_regamma) {
		ret = dc_replace_property_blob_from_id(crtc->dev, &state->lcsc1_regamma, val, -1,
						       sizeof(struct drm_color_lut), &replaced);
		state->lcsc1_changed = replaced;
		return ret;
	} else {
		return -EAGAIN;
	}

	return 0;
}

static int dc_hw_proc_get_disp_prop(struct dc_proc *dc_proc, struct dc_proc_state *dc_state,
				    struct drm_property *property, uint64_t *val)
{
	struct dc_crtc_proc_state *state  = to_dc_crtc_proc_state(dc_state);
	struct drm_crtc *crtc		  = to_drm_crtc(dc_proc);
	const struct vs_drm_private *priv = drm_to_vs_priv(crtc->dev);

	if (property == priv->bg_color)
		*val = state->bg_color;
	else if (property == priv->dither)
		*val = (state->dither) ? state->dither->base.id : 0;
	else if (property == priv->lcsc0_degamma)
		*val = (state->lcsc0_degamma) ? state->lcsc0_degamma->base.id : 0;
	else if (property == priv->lcsc0_csc)
		*val = (state->lcsc0_csc) ? state->lcsc0_csc->base.id : 0;
	else if (property == priv->lcsc0_regamma)
		*val = (state->lcsc0_regamma) ? state->lcsc0_regamma->base.id : 0;
	else if (property == priv->lcsc1_degamma)
		*val = (state->lcsc1_degamma) ? state->lcsc1_degamma->base.id : 0;
	else if (property == priv->lcsc1_csc)
		*val = (state->lcsc1_csc) ? state->lcsc1_csc->base.id : 0;
	else if (property == priv->lcsc1_regamma)
		*val = (state->lcsc1_regamma) ? state->lcsc1_regamma->base.id : 0;
	else
		return -EAGAIN;

	return 0;
}

static void dc_hw_proc_disp_print_state(struct drm_printer *p, struct dc_proc_state *dc_state)
{
	struct dc_crtc_proc_state *state  = to_dc_crtc_proc_state(dc_state);
	struct dc_crtc_proc *hw_disp	  = to_dc_crtc_proc(state->base.proc);
	struct dc_hw_disp_info *disp_info = to_dc_hw_disp_info(hw_disp->disp.info);

	drm_printf(p, "\tUnderflow cnt=%d\n", state->underflow_cnt);
	if (is_supported(&disp_info->info, DC_DISPLAY_BGCOLOR))
		drm_printf(p, "\tBG Color=0x%08x\n", state->bg_color);
}

static enum dc_hw_port get_out_bus(struct vs_dc *dc, const struct dc_hw_proc_info *info)
{
	const char *bus_name = dc->out_bus_list[DC_GET_INDEX(info->id)];
	enum dc_hw_port out  = DC_PORT_DPI;

	if (!strcmp(bus_name, "dpi"))
		out = DC_PORT_DPI;
	else if (!strcmp(bus_name, "dp"))
		out = DC_PORT_DP;
	else if (!strcmp(bus_name, "dbi"))
		out = DC_PORT_DBI;

	return out;
}

static struct dc_proc *dc_hw_create_crtc(struct device *dev, const struct dc_proc_info *info)
{
	struct vs_dc *dc			= dev_get_drvdata(dev);
	struct dc_hw *hw			= dc->hw;
	const struct dc_hw_disp_info *disp_info = (const struct dc_hw_disp_info *)info->info;
	struct dc_crtc_proc *hw_disp;
	int ret;

	hw_disp = kzalloc(sizeof(*hw_disp), GFP_KERNEL);
	if (!hw_disp)
		return ERR_PTR(-ENOMEM);

	ret = dc_hw_init(&hw_disp->disp, hw, &disp_info->info);
	if (ret)
		goto err_free;

	dc_hw_setup(&hw_disp->disp);

	hw_disp->out_port = get_out_bus(dc, &disp_info->info);

	if (disp_info->clk_name) {
		hw_disp->pix_clk = clk_get_optional(dev, disp_info->clk_name);
		if (IS_ERR(hw_disp->pix_clk)) {
			dev_err(dev, "failed to get pix_clk:%s\n", disp_info->clk_name);
			ret = PTR_ERR(hw_disp->pix_clk);
			goto err_free;
		}
	}

	INIT_LIST_HEAD(&hw_disp->base.head);

	INIT_WORK(&hw_disp->work, hw_display_work);

	return &hw_disp->base;

err_free:
	kfree(hw_disp);
	return ERR_PTR(ret);
}

const struct dc_proc_funcs dc_hw_crtc_funcs = {
	.create		 = dc_hw_create_crtc,
	.enable		 = dc_hw_proc_enable_disp,
	.disable	 = dc_hw_proc_disable_disp,
	.mode_fixup	 = dc_hw_proc_mode_fixup,
	.update		 = dc_hw_proc_update_disp,
	.enable_vblank	 = dc_hw_proc_enable_vblank,
	.enable_crc	 = dc_hw_proc_enable_crc,
	.vblank		 = dc_hw_proc_vblank,
	.commit		 = dc_hw_proc_commit_disp,
	.check		 = dc_hw_proc_check_disp,
	.destroy	 = dc_hw_proc_destroy_disp,
	.resume		 = dc_hw_proc_resume_disp,
	.destroy_state	 = dc_hw_proc_destroy_disp_state,
	.create_state	 = dc_hw_proc_create_disp_state,
	.duplicate_state = dc_hw_proc_duplicate_disp_state,
	.create_prop	 = dc_hw_proc_create_disp_prop,
	.set_prop	 = dc_hw_proc_set_disp_prop,
	.get_prop	 = dc_hw_proc_get_disp_prop,
	.print_state	 = dc_hw_proc_disp_print_state,
};

static void dc_hw_proc_commit_wb(struct dc_proc *dc_proc)
{
	struct dc_wb_proc *hw_wb = to_dc_wb_proc(dc_proc);
	struct vs_wb *vs_wb	 = proc_to_vs_wb(dc_proc);
	struct dc_wb_proc_state *dc_state =
		to_dc_wb_proc_state(get_wb_proc_state(vs_wb->base.base.state, dc_proc));
	struct drm_writeback_job *job = vs_wb->base.base.state->writeback_job;
	const struct dc_hw_wb_info *wb_info;
	const struct dc_hw_proc_info *info;
	struct dc_hw_fb wb_fb = {};
	enum dc_hw_wb_pos wb_pos;
	struct dc_hw_processor *writeback = &hw_wb->wb;

	if (!job || !job->fb) {
		dc_hw_update(writeback, DC_PROP_DISP_WRITEBACK_FB, NULL);
		return;
	}

	wb_info = (struct dc_hw_wb_info *)(dc_proc->info->info);
	info	= &wb_info->info;
	if (info->features & DC_DISPLAY_BLEND_WRITEBACK) {
		u8 *wait_vsync_cnt = job->priv;

		*wait_vsync_cnt = 0;
	}

	if (WARN_ON(!dc_state))
		return;

	wb_pos = dc_state->wb_position;

	update_fb(job->fb, &wb_fb, wb_info->format, wb_info->modifier);
	wb_fb.color_gamut = to_vs_gamut(dc_state->color_encoding, wb_info->gamut);

	dc_hw_update(writeback, DC_PROP_DISP_WRITEBACK_POS, (void *)wb_pos);

	dc_hw_update(writeback, DC_PROP_DISP_WRITEBACK_FB, &wb_fb);

	dc_hw_commit(writeback);
}

static void dc_hw_proc_destroy_wb(struct dc_proc *dc_proc)
{
	struct dc_wb_proc *hw_wb = to_dc_wb_proc(dc_proc);

	kfree(hw_wb);
}

static void dc_hw_proc_wb_vblank(struct dc_proc *dc_proc)
{
	struct dc_wb_proc *hw_wb		     = to_dc_wb_proc(dc_proc);
	struct vs_wb *vs_wb			     = proc_to_vs_wb(dc_proc);
	struct dc_hw_processor *writeback	     = &hw_wb->wb;
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

		dc_hw_update(writeback, DC_PROP_DISP_WRITEBACK_FB, NULL);
		dc_hw_commit(writeback);

		(*wait_vsync_cnt)--;
	}
}

static void dc_hw_proc_wb_destroy_state(struct dc_proc_state *state)
{
	struct dc_wb_proc_state *dc_state = to_dc_wb_proc_state(state);

	kfree(dc_state);
}

static struct dc_proc_state *dc_hw_proc_wb_create_state(void *parent, struct dc_proc *dc_proc)
{
	struct dc_wb_proc_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	INIT_LIST_HEAD(&state->base.head);
	state->parent	   = parent;
	state->wb_position = DC_WB_BACKEND;

	return &state->base;
}

static struct dc_proc_state *dc_hw_proc_wb_duplicate_state(void *parent, struct dc_proc *dc_proc,
							   void *_dc_state)
{
	const struct dc_proc_state *dc_state = _dc_state;
	struct dc_wb_proc_state *ori_state   = to_dc_wb_proc_state(dc_state);
	struct dc_wb_proc_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (WARN_ON(!state))
		return NULL;

	memcpy(state, ori_state, sizeof(*state));

	INIT_LIST_HEAD(&state->base.head);

	state->parent = (struct dc_wb_state *)parent;

	return &state->base;
}

static int dc_hw_proc_wb_create_prop(struct dc_proc *dc_proc)
{
	struct dc_hw_wb_info *wb_info	   = (struct dc_hw_wb_info *)dc_proc->info->info;
	const struct dc_hw_proc_info *info = &wb_info->info;
	struct drm_connector *connector	   = to_drm_connector(dc_proc);
	struct vs_drm_private *priv	   = drm_to_vs_priv(connector->dev);

	if (info->features & DC_DISPLAY_WRITEBACK_EX) {
		if (!priv->position) {
			priv->position = drm_property_create_enum(
				connector->dev, DRM_MODE_PROP_ATOMIC, "WB_POSITION", vs_wb_position,
				ARRAY_SIZE(vs_wb_position));
			if (!priv->position)
				return -EINVAL;
		}
		drm_object_attach_property(&connector->base, priv->position, DC_WB_BACKEND);
	}

	return 0;
}

static int dc_hw_proc_wb_set_prop(struct dc_proc *dc_proc, struct dc_proc_state *dc_state,
				  struct drm_property *property, uint64_t val)
{
	struct drm_connector *connector	  = to_drm_connector(dc_proc);
	struct dc_wb_proc_state *state	  = to_dc_wb_proc_state(dc_state);
	const struct vs_drm_private *priv = drm_to_vs_priv(connector->dev);

	if (property == priv->position) {
		state->wb_position = val;
		return 0;
	}

	return -EINVAL;
}

static int dc_hw_proc_wb_get_prop(struct dc_proc *dc_proc, struct dc_proc_state *dc_state,
				  struct drm_property *property, uint64_t *val)
{
	const struct dc_wb_proc_state *state = to_dc_wb_proc_state(dc_state);
	struct drm_connector *connector	     = to_drm_connector(dc_proc);
	const struct vs_drm_private *priv    = drm_to_vs_priv(connector->dev);

	if (property == priv->position) {
		*val = state->wb_position;
		return 0;
	}

	return -EINVAL;
}

static struct dc_proc *dc_hw_create_writeback(struct device *dev, const struct dc_proc_info *info)
{
	struct vs_dc *dc		    = dev_get_drvdata(dev);
	struct dc_hw *hw		    = dc->hw;
	const struct dc_hw_wb_info *wb_info = (const struct dc_hw_wb_info *)info->info;
	struct dc_wb_proc *hw_wb;
	int ret;

	hw_wb = kzalloc(sizeof(*hw_wb), GFP_KERNEL);
	if (!hw_wb)
		return ERR_PTR(-ENOMEM);

	ret = dc_hw_init(&hw_wb->wb, hw, &wb_info->info);
	if (ret) {
		kfree(hw_wb);
		return ERR_PTR(ret);
	}

	INIT_LIST_HEAD(&hw_wb->base.head);

	return &hw_wb->base;
}

const struct dc_proc_funcs dc_hw_wb_funcs = {
	.create		 = dc_hw_create_writeback,
	.commit		 = dc_hw_proc_commit_wb,
	.destroy	 = dc_hw_proc_destroy_wb,
	.vblank		 = dc_hw_proc_wb_vblank,
	.destroy_state	 = dc_hw_proc_wb_destroy_state,
	.create_state	 = dc_hw_proc_wb_create_state,
	.duplicate_state = dc_hw_proc_wb_duplicate_state,
	.create_prop	 = dc_hw_proc_wb_create_prop,
	.set_prop	 = dc_hw_proc_wb_set_prop,
	.get_prop	 = dc_hw_proc_wb_get_prop,
};
