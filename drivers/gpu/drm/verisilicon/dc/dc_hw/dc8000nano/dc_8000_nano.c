// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 VeriSilicon Holdings Co., Ltd.
 */

#include <linux/slab.h>

#include "dc_hw_priv.h"
#include "dc_hw_type.h"
#include "dc_8000_nano_reg.h"
#include "dc_8000_nano.h"
#include "dc_8000_nano_type.h"

static u16 zpos_val[3] = {0, 1, 2};

/* display controller plane register structure */
struct dc_hw_plane_reg {
	u32 config;
	u32 y_address;
	u32 y_stride;
	u32 tile_config;
	u32 u_address;
	u32 u_stride;
	u32 bg;
	u32 color_key;
	u32 color_key_high;
	u32 clear_value;
	u32 top_left;
	u32 size;
	u32 alpha_global_color;
	u32 blend_config;
};

/* unify plane register */
static const struct dc_hw_plane_reg dc_plane_reg[] = {
	{
		.config		    = GCREG_FRAME_BUFFER_CONFIG_Address,
		.y_address	    = GCREG_FRAME_BUFFER_ADDRESS_Address,
		.y_stride	    = GCREG_FRAME_BUFFER_STRIDE_Address,
		.tile_config	    = GCREG_DC_TILE_IN_CFG_Address,
		.u_address	    = GCREG_DC_TILE_UV_FRAME_BUFFER_ADR_Address,
		.u_stride	    = GCREG_DC_TILE_UV_FRAME_BUFFER_STR_Address,
		.bg		    = GCREG_FRAME_BUFFER_BACKGROUND_Address,
		.color_key	    = GCREG_FRAME_BUFFER_COLOR_KEY_Address,
		.color_key_high	    = GCREG_FRAME_BUFFER_COLOR_KEY_HIGH_Address,
		.clear_value	    = GCREG_FRAME_BUFFER_CLEAR_VALUE_Address,
		.top_left	    = GCREG_VIDEO_TL_Address,
		.size		    = GCREG_FRAME_BUFFER_SIZE_Address,
		.alpha_global_color = GCREG_VIDEO_GLOBAL_ALPHA_Address,
		.blend_config	    = GCREG_VIDEO_ALPHA_BLEND_CONFIG_Address,
	},
	{
		.config		    = GCREG_OVERLAY_CONFIG_Address,
		.y_address	    = GCREG_OVERLAY_ADDRESS_Address,
		.y_stride	    = GCREG_OVERLAY_STRIDE_Address,
		.tile_config	    = GCREG_DC_OVERLAY_TILE_IN_CFG_Address,
		.u_address	    = GCREG_DC_TILE_UV_OVERLAY_ADR_Address,
		.u_stride	    = GCREG_DC_TILE_UV_OVERLAY_STR_Address,
		.color_key	    = GCREG_OVERLAY_COLOR_KEY_Address,
		.color_key_high	    = GCREG_OVERLAY_COLOR_KEY_HIGH_Address,
		.clear_value	    = GCREG_OVERLAY_CLEAR_VALUE_Address,
		.top_left	    = GCREG_OVERLAY_TL_Address,
		.size		    = GCREG_OVERLAY_SIZE_Address,
		.alpha_global_color = GCREG_OVERLAY_GLOBAL_ALPHA_Address,
		.blend_config	    = GCREG_OVERLAY_ALPHA_BLEND_CONFIG_Address,
	},
	{
		.config		    = GCREG_OVERLAY_CONFIG1_Address,
		.y_address	    = GCREG_OVERLAY_ADDRESS1_Address,
		.y_stride	    = GCREG_OVERLAY_STRIDE1_Address,
		.color_key	    = GCREG_OVERLAY_COLOR_KEY1_Address,
		.color_key_high	    = GCREG_OVERLAY_COLOR_KEY_HIGH1_Address,
		.clear_value	    = GCREG_OVERLAY_CLEAR_VALUE1_Address,
		.top_left	    = GCREG_OVERLAY_TL1_Address,
		.size		    = GCREG_OVERLAY_SIZE1_Address,
		.alpha_global_color = GCREG_OVERLAY_GLOBAL_ALPHA1_Address,
		.blend_config	    = GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_Address,
	},
};

static inline const struct dc_hw_plane_reg *get_plane_reg(u8 id)
{
	if (DC_GET_TYPE(id) == DC_PRIMARY_PLANE)
		return &dc_plane_reg[0];

	if (DC_GET_TYPE(id) == DC_OVERLAY_PLANE)
		return &dc_plane_reg[DC_GET_INDEX(id) + 1];

	return NULL;
}

static inline void dc_write(struct dc_hw *hw, u32 reg, u32 value)
{
	writel(value, hw->base + reg);
}

static inline u32 dc_read(struct dc_hw *hw, u32 reg)
{
	u32 value = readl(hw->base + reg);

	return value;
}

static inline void dc_set_clear(struct dc_hw *hw, u32 reg, u32 set, u32 clear)
{
	u32 value = dc_read(hw, reg);

	value &= ~clear;
	value |= set;
	dc_write(hw, reg, value);
}

static int dc_hw_init_video_plane(struct dc_hw *hw, const struct dc_hw_proc_info *info)
{
	return 0;
}

static int dc_hw_init_cursor_plane(struct dc_hw *hw, const struct dc_hw_proc_info *info)
{
	dc_write(hw, GCREG_CURSOR_BACKGROUND_Address, 0x00FFFFFF);
	dc_write(hw, GCREG_CURSOR_FOREGROUND_Address, 0x00AAAAAA);

	return 0;
}

static int dc_hw_init_display(struct dc_hw *hw, const struct dc_hw_proc_info *info)
{
	u32 config;

	config = VS_SET_FIELD(0, GCREG_PANEL_CONFIG, DE, 1) |
		 VS_SET_FIELD(0, GCREG_PANEL_CONFIG, DE_POLARITY, 0) |
		 VS_SET_FIELD(0, GCREG_PANEL_CONFIG, DATA_POLARITY, 0) |
		 VS_SET_FIELD(0, GCREG_PANEL_CONFIG, CLOCK_POLARITY, 0) |
		 VS_SET_FIELD(0, GCREG_PANEL_CONFIG, CLOCK, 1);

	dc_write(hw, GCREG_PANEL_CONFIG_Address, config);

	return 0;
}

static void dc_hw_enable_shadow(struct dc_hw *hw, u8 id, bool enable)
{
	u32 config;

	config = VS_SET_FIELD(0, GCREG_PANEL_CONTROL, VALID, enable);

	dc_write(hw, GCREG_PANEL_CONTROL_Address, config);
}

static u8 get_plane_index(u8 id)
{
	if (DC_GET_TYPE(id) == DC_PRIMARY_PLANE)
		return 0;

	return DC_GET_INDEX(id) + 1;
}

static void plane_update_zpos(struct dc_hw *hw, const struct dc_hw_proc_info *info, u16 *zpos)
{
	u8 i	   = get_plane_index(info->id);
	u32 config = 0;

	zpos_val[i] = *zpos;

	if (zpos_val[0] < zpos_val[1] && zpos_val[1] < zpos_val[2])
		config = 0;
	else if (zpos_val[0] < zpos_val[2] && zpos_val[2] < zpos_val[1])
		config = 1;
	else if (zpos_val[1] < zpos_val[0] && zpos_val[0] < zpos_val[2])
		config = 2;
	else if (zpos_val[1] < zpos_val[2] && zpos_val[2] < zpos_val[0])
		config = 3;
	else if (zpos_val[2] < zpos_val[0] && zpos_val[0] < zpos_val[1])
		config = 4;
	else if (zpos_val[2] < zpos_val[1] && zpos_val[1] < zpos_val[0])
		config = 5;

	dc_write(hw, GCREG_BLEND_STACK_ORDER_Address, config);
}

static void update_tile_config(struct dc_hw *hw, const struct dc_hw_proc_info *info,
			       struct dc_hw_fb *fb)
{
	const struct dc_hw_plane_reg *reg = get_plane_reg(info->id);
	u32 config;

	if (!fb->tile_mode) {
		dc_write(hw, reg->tile_config, 0);
		return;
	}

	switch (fb->format) {
	case DC_8000_NANO_FORMAT_A8R8G8B8:
		config = 1;
		break;
	case DC_8000_NANO_FORMAT_NV12:
		config = 3;
		break;
	case DC_8000_NANO_FORMAT_YUY2:
		config = 2;
		break;
	default:
		config = 0;
		break;
	}

	dc_write(hw, reg->tile_config, config);
}

static u8 get_second_plane_tile_mode(u8 tile_mode)
{
	if (tile_mode == DC_8000_NANO_TILE_8X8)
		return DC_8000_NANO_TILE_4X4;

	return DC_8000_NANO_LINEAR;
}

static u8 get_tile_height(u8 tile_mode)
{
	switch (tile_mode) {
	case DC_8000_NANO_TILE_4X4:
		return 4;
	case DC_8000_NANO_TILE_8X8:
		return 8;
	default:
		return 1;
	}

	return 1;
}

static void plane_update_fb(struct dc_hw *hw, const struct dc_hw_proc_info *info,
			    struct dc_hw_fb *fb)
{
	const struct dc_hw_plane_reg *reg = get_plane_reg(info->id);
	u32 stride			  = fb->y_stride;

	dc_write(hw, reg->y_address, fb->y_address);
	dc_write(hw, reg->size, fb->width | (fb->height << 16));

	update_tile_config(hw, info, fb);

	stride *= get_tile_height(fb->tile_mode);

	dc_write(hw, reg->y_stride, stride);

	dc_set_clear(hw, reg->config,
		     (fb->uv_swizzle << 19) | (fb->swizzle << 17) | BIT(3) | fb->format,
		     BIT(19) | (0x3 << 17) | 0x7);

	if (!fb->is_rgb && (info->features & DC_PLANE_YUV2RGB)) {
		dc_set_clear(hw, reg->tile_config, fb->color_gamut << 2, 0x01 << 2);

		if (!fb->u_stride)
			return;

		stride = fb->u_stride * get_tile_height(get_second_plane_tile_mode(fb->tile_mode));
		dc_write(hw, reg->u_address, fb->u_address);
		dc_write(hw, reg->u_stride, stride);
	}
}

static void update_position(struct dc_hw *hw, const struct dc_hw_proc_info *info,
			    struct dc_hw_win *win)
{
	const struct dc_hw_plane_reg *reg = get_plane_reg(info->id);

	dc_write(hw, reg->top_left, win->dst_x | (win->dst_y << 16));
}

static void plane_update_win(struct dc_hw *hw, const struct dc_hw_proc_info *info,
			     struct dc_hw_win *win)
{
	if (info->features & DC_PLANE_POSITION)
		update_position(hw, info, win);
}

static void plane_update_blend(struct dc_hw *hw, const struct dc_hw_proc_info *info,
			       struct dc_hw_blend *blend)
{
	const struct dc_hw_plane_reg *reg = get_plane_reg(info->id);

	if (!(info->features & DC_PLANE_BLEND))
		return;

	dc_write(hw, reg->alpha_global_color, (blend->alpha << 8) | blend->alpha);

	switch (blend->blend_mode) {
	case DC_BLEND_PREMULTI:
		dc_write(hw, reg->blend_config, 0x6989);
		break;
	case DC_BLEND_COVERAGE:
		dc_write(hw, reg->blend_config, 0x6991);
		break;
	case DC_BLEND_PIXEL_NONE:
		dc_write(hw, reg->blend_config, 0x6589);
		break;
	default:
		break;
	}
}

static void plane_update_cursor(struct dc_hw *hw, const struct dc_hw_proc_info *info,
				struct dc_hw_cursor *cursor)
{
	u32 config;

	dc_write(hw, GCREG_CURSOR_ADDRESS_Address, cursor->address);

	config = VS_SET_FIELD(0, GCREG_CURSOR_LOCATION, X, cursor->x < 0 ? 0: cursor->x) |
		 VS_SET_FIELD(0, GCREG_CURSOR_LOCATION, Y, cursor->y < 0 ? 0: cursor->y);
	dc_write(hw, GCREG_CURSOR_LOCATION_Address, config);

	config = VS_SET_FIELD(0, GCREG_CURSOR_CONFIG, HOT_SPOT_X, cursor->hot_x) |
		 VS_SET_FIELD(0, GCREG_CURSOR_CONFIG, HOT_SPOT_Y, cursor->hot_y) |
		 VS_SET_FIELD(0, GCREG_CURSOR_CONFIG, FORMAT,
			      (cursor->masked ? GCREG_CURSOR_CONFIG_FORMAT_MASKED :
						GCREG_CURSOR_CONFIG_FORMAT_A8R8G8B8));

	dc_write(hw, GCREG_CURSOR_CONFIG_Address, config);
}

static void disable_primary(struct dc_hw *hw, u8 id)
{
	u32 config;

	config = dc_read(hw, GCREG_FRAME_BUFFER_CONFIG_Address);

	config = VS_SET_FIELD(config, GCREG_FRAME_BUFFER_CONFIG, ENABLE, 0);

	dc_write(hw, GCREG_FRAME_BUFFER_CONFIG_Address, config);
}

static void disable_overlay(struct dc_hw *hw, u8 id)
{
	const struct dc_hw_plane_reg *reg = get_plane_reg(id);

	dc_set_clear(hw, reg->config, 0, BIT(3));
}

static void disable_cursor(struct dc_hw *hw, u8 id)
{
	dc_write(hw, GCREG_CURSOR_CONFIG_Address, 0x00);
}

static void disable_display(struct dc_hw *hw, u8 id)
{
	u32 config;

	config = dc_read(hw, GCREG_PANEL_CONFIG_Address);

	config = VS_SET_FIELD(config, GCREG_PANEL_CONFIG, CLOCK, 0);

	dc_write(hw, GCREG_PANEL_CONFIG_Address, config);

	dc_write(hw, GCREG_SOFT_RESET_Address, GCREG_SOFT_RESET_RESET_RESET);
}

static void disp_update_gamma(struct dc_hw *hw, const struct dc_hw_proc_info *info,
			      struct dc_hw_gamma *gamma)
{
	u16 j;
	u32 config;
	struct dc_hw_gamma_data *data;

	if (!(info->features & DC_DISPLAY_GAMMA))
		return;

	if (!gamma) {
		config = dc_read(hw, GCREG_PANEL_FUNCTION_Address);
		config = VS_SET_FIELD(config, GCREG_PANEL_FUNCTION, GAMMA, 0);
		dc_write(hw, GCREG_PANEL_FUNCTION_Address, config);
		return;
	}

	data = gamma->data;
	dc_write(hw, GCREG_GAMMA_INDEX_Address, 0x00);
	for (j = 0; j < gamma->size; j++) {
		config = VS_SET_FIELD(0, GCREG_GAMMA_DATA, RED, data[j].r) |
			 VS_SET_FIELD(0, GCREG_GAMMA_DATA, GREEN, data[j].g) |
			 VS_SET_FIELD(0, GCREG_GAMMA_DATA, BLUE, data[j].b);
		dc_write(hw, GCREG_GAMMA_DATA_Address, config);
	}

	config = dc_read(hw, GCREG_PANEL_FUNCTION_Address);
	config = VS_SET_FIELD(config, GCREG_PANEL_FUNCTION, GAMMA, 1);
	dc_write(hw, GCREG_PANEL_FUNCTION_Address, config);
}

static void disp_update_scan(struct dc_hw *hw, const struct dc_hw_proc_info *info,
			     struct dc_hw_scan *scan)
{
	u32 config;

	if (!scan)
		return;

	config = VS_SET_FIELD(0, GCREG_HDISPLAY, DISPLAY_END, scan->h_active) |
		 VS_SET_FIELD(0, GCREG_HDISPLAY, TOTAL, scan->h_total);

	dc_write(hw, GCREG_HDISPLAY_Address, config);

	config = VS_SET_FIELD(0, GCREG_HSYNC, START, scan->h_sync_start) |
		 VS_SET_FIELD(0, GCREG_HSYNC, END, scan->h_sync_end) |
		 VS_SET_FIELD(0, GCREG_HSYNC, PULSE, 1) |
		 VS_SET_FIELD(0, GCREG_HSYNC, POLARITY, (scan->h_sync_polarity ? 0 : 1));

	dc_write(hw, GCREG_HSYNC_Address, config);

	config = VS_SET_FIELD(0, GCREG_VDISPLAY, DISPLAY_END, scan->v_active) |
		 VS_SET_FIELD(0, GCREG_VDISPLAY, TOTAL, scan->v_total);

	dc_write(hw, GCREG_VDISPLAY_Address, config);

	config = VS_SET_FIELD(0, GCREG_VSYNC, START, scan->v_sync_start) |
		 VS_SET_FIELD(0, GCREG_VSYNC, END, scan->v_sync_end) |
		 VS_SET_FIELD(0, GCREG_VSYNC, PULSE, 1) |
		 VS_SET_FIELD(0, GCREG_VSYNC, POLARITY, (scan->v_sync_polarity ? 0 : 1));

	dc_write(hw, GCREG_VSYNC_Address, config);

	dc_hw_init_display(hw, info);

	dc_hw_enable_shadow(hw, info->id, true);

	config = VS_SET_FIELD(0, GCREG_PANEL_WORKING, WORKING, 0x1);

	dc_write(hw, GCREG_PANEL_WORKING_Address, config);
}

static void disp_enable_crc(struct dc_hw *hw, const struct dc_hw_proc_info *info, bool *enable) {}

static void disp_update_bg_color(struct dc_hw *hw, const struct dc_hw_proc_info *info, u32 *data)
{
	if (!(info->features & DC_DISPLAY_BGCOLOR))
		return;

	dc_write(hw, GCREG_FRAME_BUFFER_BACKGROUND_Address, *data);
}

static void disp_update_dither(struct dc_hw *hw, const struct dc_hw_proc_info *info, u32 *data)
{
	u32 config;

	if (!(info->features & DC_DISPLAY_DITHER))
		return;

	config = dc_read(hw, GCREG_PANEL_FUNCTION_Address);

	config = VS_SET_FIELD(config, GCREG_PANEL_FUNCTION, DITHER, 1);

	if (!data) {
		config = VS_SET_FIELD(config, GCREG_PANEL_FUNCTION, DITHER, 0);
		dc_write(hw, GCREG_PANEL_FUNCTION_Address, config);
		return;
	}

	dc_write(hw, GCREG_DISPLAY_DITHER_TABLE_LOW_Address, data[0]);
	dc_write(hw, GCREG_DISPLAY_DITHER_TABLE_HIGH_Address, data[1]);

	dc_write(hw, GCREG_PANEL_FUNCTION_Address, config);
}

static int disp_get_crc(struct dc_hw *hw, const struct dc_hw_proc_info *info, u32 *value,
			size_t size)
{
	if (size != sizeof(*value))
		return -EINVAL;

	*value = 0;

	return 0;
}

static int disp_get_underflow(struct dc_hw *hw, const struct dc_hw_proc_info *info, bool *value)
{
	*value = !!(dc_read(hw, GCREG_PANEL_STATE_Address));

	return 0;
}

static void setup_output_dpi(struct dc_hw *hw, u8 id, struct dc_hw_output *out)
{
	u32 dpi_cfg = 0;
	u32 config;

	switch (out->bus_format) {
	case DC_BUS_FMT_RGB565_1:
		dpi_cfg = 0;
		break;
	case DC_BUS_FMT_RGB565_2:
		dpi_cfg = 1;
		break;
	case DC_BUS_FMT_RGB565_3:
		dpi_cfg = 2;
		break;
	case DC_BUS_FMT_RGB666_PACKED:
		dpi_cfg = 3;
		break;
	case DC_BUS_FMT_RGB666:
		dpi_cfg = 4;
		break;
	case DC_BUS_FMT_RGB888:
		dpi_cfg = 5;
		break;
	default:
		dpi_cfg = 5;
		break;
	}

	dc_write(hw, GCREG_DPI_CONFIG_Address, dpi_cfg);

	config = VS_SET_FIELD(0, GCREG_DBI_CONFIG, BUS_OUTPUT_SEL, 0);
	dc_write(hw, GCREG_DBI_CONFIG_Address, config);
}

static inline void enable_yuv422_output(struct dc_hw *hw)
{
	dc_set_clear(hw, GCREG_FRAME_BUFFER_CONFIG_Address, BIT(11), 0);
}

static inline void disable_yuv422_output(struct dc_hw *hw)
{
	dc_set_clear(hw, GCREG_FRAME_BUFFER_CONFIG_Address, 0, BIT(11));
}

static void disp_update_output(struct dc_hw *hw, const struct dc_hw_proc_info *info,
			       struct dc_hw_output *out)
{
	u32 config;

	if (out->bus_format == DC_BUS_FMT_YUV8)
		enable_yuv422_output(hw);
	else
		disable_yuv422_output(hw);

	config = dc_read(hw, GCREG_PANEL_FUNCTION_Address);
	config = VS_SET_FIELD(config, GCREG_PANEL_FUNCTION, OUTPUT, 1);
	dc_write(hw, GCREG_PANEL_FUNCTION_Address, config);

	setup_output_dpi(hw, DC_GET_INDEX(info->id), out);
}

static void disp_update_writeback_fb(struct dc_hw *hw, const struct dc_hw_proc_info *info,
				     struct dc_hw_fb *fb)
{
	if (!fb) {
		dc_write(hw, GCREG_PANEL_DEST_ADDRESS_Address, 0);
		dc_write(hw, GCREG_DEST_STRIDE_Address, 0);
		return;
	}

	dc_write(hw, GCREG_DEST_STRIDE_Address, fb->y_stride);
	dc_write(hw, GCREG_PANEL_DEST_ADDRESS_Address, fb->y_address);
}

static int dc_8000_nano_proc_init(struct dc_hw_processor *processor)
{
	struct dc_hw *hw		   = processor->hw;
	const struct dc_hw_proc_info *info = processor->info;
	int ret				   = 0;

	switch (DC_GET_TYPE(info->id)) {
	case DC_PRIMARY_PLANE:
	case DC_OVERLAY_PLANE:
		ret = dc_hw_init_video_plane(hw, info);
		break;

	case DC_CURSOR_PLANE:
		ret = dc_hw_init_cursor_plane(hw, info);
		break;

	case DC_DISPLAY:
		ret = dc_hw_init_display(hw, info);
		break;

	default:
		break;
	}

	return ret;
}

static int check_property(u8 prop, u8 type)
{
	switch (prop) {
	case DC_PROP_PLANE_FB:
	case DC_PROP_PLANE_WIN:
	case DC_PROP_PLANE_BLEND:
	case DC_PROP_PLANE_ZPOS:
		if (type <= DC_OVERLAY_PLANE)
			return 0;
		break;

	case DC_PROP_PLANE_CURSOR:
		if (type == DC_CURSOR_PLANE)
			return 0;
		break;

	case DC_PROP_DISP_GAMMA:
	case DC_PROP_DISP_SCAN:
	case DC_PROP_DISP_OUTPUT:
	case DC_PROP_DISP_WRITEBACK_FB:
	case DC_PROP_DISP_CRC:
	case DC_PROP_DISP_BG_COLOR:
	case DC_PROP_DISP_DITHER:
		if (type == DC_DISPLAY)
			return 0;
		break;

	default:
		break;
	}
	return -1;
}

static int dc_8000_nano_proc_get(struct dc_hw_processor *processor, u8 prop, void *value,
				 size_t size)
{
	struct dc_hw *hw		   = processor->hw;
	const struct dc_hw_proc_info *info = processor->info;
	int ret				   = 0;

	switch (prop) {
	case DC_PROP_DISP_CRC:
		ret = disp_get_crc(hw, info, value, size);
		break;
	case DC_PROP_DISP_UNDERFLOW:
		ret = disp_get_underflow(hw, info, value);
		break;
	default:
		break;
	}

	return ret;
}

static void dc_8000_nano_proc_update(struct dc_hw_processor *processor, u8 prop, void *value)
{
	struct dc_hw *hw		   = processor->hw;
	const struct dc_hw_proc_info *info = processor->info;

	if (check_property(prop, DC_GET_TYPE(info->id)))
		return;

	dc_hw_enable_shadow(hw, info->id, false);

	switch (prop) {
	case DC_PROP_PLANE_FB:
		plane_update_fb(hw, info, (struct dc_hw_fb *)value);
		break;

	case DC_PROP_PLANE_WIN:
		plane_update_win(hw, info, (struct dc_hw_win *)value);
		break;

	case DC_PROP_PLANE_BLEND:
		plane_update_blend(hw, info, (struct dc_hw_blend *)value);
		break;

	case DC_PROP_PLANE_CURSOR:
		plane_update_cursor(hw, info, (struct dc_hw_cursor *)value);
		break;

	case DC_PROP_PLANE_ZPOS:
		plane_update_zpos(hw, info, (u16 *)value);
		break;

	case DC_PROP_DISP_GAMMA:
		disp_update_gamma(hw, info, (struct dc_hw_gamma *)value);
		break;

	case DC_PROP_DISP_SCAN:
		disp_update_scan(hw, info, (struct dc_hw_scan *)value);
		break;

	case DC_PROP_DISP_OUTPUT:
		disp_update_output(hw, info, (struct dc_hw_output *)value);
		break;

	case DC_PROP_DISP_WRITEBACK_FB:
		disp_update_writeback_fb(hw, info, (struct dc_hw_fb *)value);
		break;

	case DC_PROP_DISP_CRC:
		disp_enable_crc(hw, info, (bool *)value);
		break;

	case DC_PROP_DISP_BG_COLOR:
		disp_update_bg_color(hw, info, (u32 *)value);
		break;

	case DC_PROP_DISP_DITHER:
		disp_update_dither(hw, info, (u32 *)value);
		break;

	default:
		break;
	}
}

static void dc_8000_nano_proc_disable(struct dc_hw_processor *processor)
{
	struct dc_hw *hw		   = processor->hw;
	const struct dc_hw_proc_info *info = processor->info;

	switch (DC_GET_TYPE(info->id)) {
	case DC_PRIMARY_PLANE:
		disable_primary(hw, info->id);
		break;

	case DC_OVERLAY_PLANE:
		disable_overlay(hw, info->id);
		break;

	case DC_CURSOR_PLANE:
		disable_cursor(hw, info->id);
		break;

	case DC_DISPLAY:
		disable_display(hw, info->id);
		break;

	default:
		break;
	}
}

static void dc_8000_nano_proc_commit(struct dc_hw_processor *processor)
{
	struct dc_hw *hw		   = processor->hw;
	const struct dc_hw_proc_info *info = processor->info;

	dc_hw_enable_shadow(hw, info->id, true);
}

static void dc_8000_nano_proc_enable_vblank(struct dc_hw_processor *processor, bool enable)
{
	struct dc_hw *hw = processor->hw;
	u32 config;

	config = dc_read(hw, GCREG_DISPLAY_INTR_ENABLE_Address);
	config = VS_SET_FIELD(config, GCREG_DISPLAY_INTR_ENABLE, DISP0, enable);

	dc_write(hw, GCREG_DISPLAY_INTR_ENABLE_Address, config);
}

static const struct dc_hw_proc_funcs dc_8000_nano_proc_funcs = {
	.init	    = dc_8000_nano_proc_init,
	.disable    = dc_8000_nano_proc_disable,
	.update	    = dc_8000_nano_proc_update,
	.get	    = dc_8000_nano_proc_get,
	.commit	    = dc_8000_nano_proc_commit,
	.enable_irq = dc_8000_nano_proc_enable_vblank,
};

static int dc_8000_nano_init(struct dc_hw *hw)
{
	u32 rev = dc_read(hw, GCREG_DC_CHIP_REV_Address);

	switch (rev) {
	case 0x5544:
		hw->ver = DC_8000_NANO_VER_5544;
		break;
	default:
		return -ENODEV;
	}

	return 0;
}

static const struct dc_hw_proc_funcs *dc_8000_nano_get_proc_funcs(struct dc_hw *hw)
{
	return &dc_8000_nano_proc_funcs;
}

static u32 dc_8000_nano_get_interrupt(struct dc_hw *hw)
{
	if (!hw)
		return 0;

	return dc_read(hw, GCREG_DISPLAY_INTR_Address);
}

const struct dc_hw_funcs dc_8000_nano_funcs = {
	.init		= dc_8000_nano_init,
	.get_proc_funcs = dc_8000_nano_get_proc_funcs,
	.get_irq	= dc_8000_nano_get_interrupt,
};
