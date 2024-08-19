/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef __DC_HW_TYPE_H__
#define __DC_HW_TYPE_H__

/** @addtogroup DC
 *  vs dc hw types.
 *  @ingroup DRIVER
 *
 *  @{
 */

#include <linux/io.h>

#include "dc_8000_nano_type.h"
/**
 * id[2:0] is dc_hw_type; id[7:3] is index of this type
 */
#define DC_GET_TYPE(id)	       ((id) & 0x7)
#define DC_GET_INDEX(id)       ((id) >> 3)
#define DC_SET_ID(type, index) (((index) << 3) | (type))

/**
 * display controller family
 */
enum dc_hw_family {
	DC_8000_NANO_FAMILY,
	DC_8000_FAMILY,
	DC_9000_FAMILY,
	DC_FAMILY_NUM,
};

/**
 * display controller module type
 */
enum dc_hw_type {
	DC_PRIMARY_PLANE = 0,
	DC_OVERLAY_PLANE,
	DC_CURSOR_PLANE,
	DC_DISPLAY,
};

/**
 * display controller color format swizzle
 */
enum dc_hw_swizzle {
	DC_SWIZZLE_ARGB = 0,
	DC_SWIZZLE_RGBA,
	DC_SWIZZLE_ABGR,
	DC_SWIZZLE_BGRA,
};

/**
 * display controller color format swizzle uv
 */
enum dc_hw_swizzle_uv {
	DC_SWIZZLE_UV = 0,
	DC_SWIZZLE_VU,
};

/**
 * display controller compress mode
 */
enum dc_hw_compress_mode {
	DC_COMPRESS_NONE = 0,
	DC_COMPRESS_DEC400,
};

/**
 * display controller cursor size
 */
enum dc_hw_cursor_size {
	DC_CURSOR_SIZE_32X32 = 0,
	DC_CURSOR_SIZE_64X64,
	DC_CURSOR_SIZE_128X128,
	DC_CURSOR_SIZE_256X256,
};

/**
 * display controller output port
 */
enum dc_hw_port {
	DC_PORT_DPI = 0,
	DC_PORT_DP,
	DC_PORT_DBI,
};

/**
 * display controller output bus format
 */
enum dc_hw_bus_format {
	DC_BUS_FMT_RGB565_1,
	DC_BUS_FMT_RGB565_2,
	DC_BUS_FMT_RGB565_3,
	DC_BUS_FMT_RGB666_PACKED,
	DC_BUS_FMT_RGB666,
	DC_BUS_FMT_RGB888,
	DC_BUS_FMT_RGB101010,
	DC_BUS_FMT_UYVY8,
	DC_BUS_FMT_YUV8,
	DC_BUS_FMT_UYVY10,
	DC_BUS_FMT_YUV10,
};

/**
 * display controller out gamut
 */
enum dc_hw_out_gamut {
	DC_OUT_GAMUT_BT709,
	DC_OUT_GAMUT_BT2020,
};

/**
 * display controller blend mode
 */
enum dc_hw_blend_mode {
	/**
	 * out.rgb = plane_alpha * fg.rgb +
	 *      (1 - (plane_alpha * fg.alpha)) * bg.rgb
	 */
	DC_BLEND_PREMULTI,
	/**
	 * out.rgb = plane_alpha * fg.alpha * fg.rgb +
	 *      (1 - (plane_alpha * fg.alpha)) * bg.rgb
	 */
	DC_BLEND_COVERAGE,
	/**
	 * out.rgb = plane_alpha * fg.rgb +
	 *      (1 - plane_alpha) * bg.rgb
	 */
	DC_BLEND_PIXEL_NONE,
};

/**
 * display controller write-back position
 */
enum dc_hw_wb_pos {
	DC_WB_BEFORE_DITHER = 0,
	DC_WB_BACKEND,
};

/**
 * display controller degamma
 */
enum dc_hw_degamma {
	DC_DEGAMMA_DISABLE = 0,
	DC_DEGAMMA_BT709,
	DC_DEGAMMA_BT2020,
};

/**
 * display controller gamut conversion
 */
enum dc_hw_gamut_conversion {
	DC_GAMUT_CONV_BYPASS = 0,
	DC_GAMUT_CONV_BT601_TO_BT709,
	DC_GAMUT_CONV_BT601_TO_BT2020,
	DC_GAMUT_CONV_BT2020_TO_BT709,
	DC_GAMUT_CONV_BT709_TO_BT2020,
	DC_GAMUT_CONV_USER,
};

/**
 * display controller color range
 */
enum dc_hw_color_range {
	DC_LIMITED_RANGE = 0,
	DC_FULL_RANGE,
};

/**
 * display controller property
 */
enum dc_hw_property {
	DC_PROP_PLANE_FB = 0,
	DC_PROP_PLANE_WIN,
	DC_PROP_PLANE_BLEND,
	DC_PROP_PLANE_DEGAMMA,
	DC_PROP_PLANE_ASSIGN,
	DC_PROP_PLANE_CURSOR,
	DC_PROP_PLANE_ZPOS,
	DC_PROP_PLANE_GAMUT_CONV,
	DC_PROP_DISP_GAMMA,
	DC_PROP_DISP_LCSC0_DEGAMMA,
	DC_PROP_DISP_LCSC0_CSC,
	DC_PROP_DISP_LCSC0_REGAMMA,
	DC_PROP_DISP_LCSC1_DEGAMMA,
	DC_PROP_DISP_LCSC1_CSC,
	DC_PROP_DISP_LCSC1_REGAMMA,
	DC_PROP_DISP_SCAN,
	DC_PROP_DISP_OUTPUT,
	DC_PROP_DISP_SECURE,
	DC_PROP_DISP_WRITEBACK_FB,
	DC_PROP_DISP_WRITEBACK_POS,
	DC_PROP_DISP_CRC,
	DC_PROP_DISP_UNDERFLOW,
	DC_PROP_DISP_BG_COLOR,
	DC_PROP_DISP_DITHER,
	DC_PROP_COUNT,
};

/**
 * display controller plane feature
 */
enum dc_hw_plane_feature {
	DC_PLANE_ROI	       = BIT(0),
	DC_PLANE_SCALE	       = BIT(1),
	DC_PLANE_POSITION      = BIT(2),
	DC_PLANE_DEGAMMA       = BIT(3),
	DC_PLANE_BLEND	       = BIT(4),
	DC_PLANE_RGB2RGB       = BIT(5),
	DC_PLANE_YUV2RGB       = BIT(6),
	DC_PLANE_ASSIGN	       = BIT(7),
	DC_PLANE_CURSOR_MASKED = BIT(8),
};

/**
 * display controller display feature
 */
enum dc_hw_display_feature {
	DC_DISPLAY_GAMMA	   = BIT(0),
	DC_DISPLAY_WRITEBACK	   = BIT(1),
	DC_DISPLAY_WRITEBACK_EX	   = BIT(2),
	DC_DISPLAY_BLEND_WRITEBACK = BIT(3),
	DC_DISPLAY_RGB2YUV	   = BIT(4),
	DC_DISPLAY_CRC		   = BIT(5),
	DC_DISPLAY_LCSC		   = BIT(6),
	DC_DISPLAY_BGCOLOR	   = BIT(7),
	DC_DISPLAY_DITHER	   = BIT(8),
};

/**
 * display controller framebuffer property structure
 *
 */
struct dc_hw_fb {
	/** 1st planar buffer address */
	u64 y_address;
	/** 2nd planar buffer address */
	u64 u_address;
	/** 3rd planar buffer address */
	u64 v_address;
	/** 1st planar buffer stride */
	u16 y_stride;
	/** 2nd planar buffer stride */
	u16 u_stride;
	/** 3rd planar buffer stride */
	u16 v_stride;
	/** buffer width in pixel */
	u16 width;
	/** buffer height in lines */
	u16 height;
	/** color format in enum dc_hw_color_format */
	u8 format;
	/** tile mode in enum dc_hw_tile_mode */
	u8 tile_mode;
	/** rotation in enum dc_hw_rotation */
	u8 rotation;
	/** gamut in enum dc_hw_color_gamut */
	u8 color_gamut;
	/** swizzle in enum dc_hw_swizzle */
	u8 swizzle;
	/** UV swizzle in bool */
	u8 uv_swizzle;
	/** color range in dc_hw_color_range */
	u8 color_range;
	/** true if color format is RGB format */
	u8 is_rgb;
	/** compress mode in enum dc_hw_compress_mode */
	u8 compress_mode;
};

/**
 * display controller input and output window of layer
 */
struct dc_hw_win {
	/** input window horizontal offset */
	u16 src_x;
	/** input window vertical offset */
	u16 src_y;
	/** input window width */
	u16 src_w;
	/** input window height */
	u16 src_h;
	/** scaler output window horizontal offset */
	u16 dst_x;
	/** scaler output window vertical offset */
	u16 dst_y;
	/** scaler output window width */
	u16 dst_w;
	/** scaler output window height */
	u16 dst_h;
};

/**
 * display controller blend setting of layer
 */
struct dc_hw_blend {
	/** alpha value of fg/bg. */
	u8 alpha;
	/** blend mode in enum dc_hw_blend_mode. */
	u8 blend_mode;
};

/**
 * display controller cursor setting
 */
struct dc_hw_cursor {
	/** buffer address of cursor. */
	u32 address;
	/** cursor horizontal offset of background. */
	s16 x;
	/** cursor vertical offset of background. */
	s16 y;
	/** horizontal offset to cursor hotspot. */
	u16 hot_x;
	/** vertical offset to cursor hotspot. */
	u16 hot_y;
	/** cursor size in enum dc_hw_cursor_size. */
	u8 size;

	bool masked;
};

/**
 * display controller gamma component
 */
struct dc_hw_gamma_data {
	u16 r;
	u16 g;
	u16 b;
};

/**
 * display controller gamma data package
 */
struct dc_hw_gamma {
	/** number of gamma data. */
	u16 size;
	/** gamma data list. */
	struct dc_hw_gamma_data *data;
};

/**
 * display controller gamut conv
 */
struct dc_hw_gamut_conv {
	/** gamut conv value. */
	u8 gamut_conv;
	/** gamut conv matrix. */
	s16 *conv_matrix;
};

/**
 * display controller lcsc csc structure
 */
struct dc_hw_lcsc_csc {
	u16 pre_gain_red;
	u16 pre_gain_green;
	u16 pre_gain_blue;
	u16 pre_offset_red;
	u16 pre_offset_green;
	u16 pre_offset_blue;
	u32 coef[9];
	u32 post_gain_red;
	u32 post_gain_green;
	u32 post_gain_blue;
	u32 post_offset_red;
	u32 post_offset_green;
	u32 post_offset_blue;
};

/**
 * display scan timing structure
 *
 *            Active                 Front           Sync           Back
 *            Region                 Porch                          Porch
 *   <-----------------------><----------------><-------------><-------------->
 *     //////////////////////|
 *    ////////////////////// |
 *   //////////////////////  |..................               ................
 *                                              _______________
 *   <------- active ------->
 *   <--------------- sync_start -------------->
 *   <----------------------- sync_end ----------------------->
 *   <---------------------------------- total ------------------------------->
 *
 */
struct dc_hw_scan {
	/** horizontal active in pixel. */
	u16 h_active;
	/** horizontal total in pixel. */
	u16 h_total;
	/** horizontal sync start. */
	u16 h_sync_start;
	/** horizontal sync end. */
	u16 h_sync_end;
	/** vertical active in line. */
	u16 v_active;
	/** vertical total in pixel. */
	u16 v_total;
	/** vertical sync start. */
	u16 v_sync_start;
	/** vertical sync end. */
	u16 v_sync_end;
	/** horizontal sync polarity:0-negative; 1-positive. */
	bool h_sync_polarity;
	/** vertical sync polarity:0-negative; 1-positive. */
	bool v_sync_polarity;
};

/**
 * dc output interface setting structure
 */
struct dc_hw_output {
	/** bus format in enum dc_hw_bus_format. */
	enum dc_hw_bus_format bus_format;
	/** port selection in enum dc_hw_port. */
	enum dc_hw_port port;
	/** output gamut in enum dc_hw_out_gamut. */
	enum dc_hw_out_gamut gamut;
};

/**
 * processor configuration structure
 */
struct dc_hw_proc_info {
	/** supported feature with enum dc_hw_feature. */
	u32 features;
	/**
	 * processor id: id[1:0] is dc_hw_type; id[7:2]
	 * is index of this type.
	 */
	u8 id;
};

struct dc_hw;
struct dc_hw_proc_funcs;

/**
 * the lowest level unit for dc hw
 */
struct dc_hw_processor {
	/** dc_hw which this processor belong to. */
	struct dc_hw *hw;
	/** configuration of this processor. */
	const struct dc_hw_proc_info *info;
	/** operations of this processor. */
	const struct dc_hw_proc_funcs *funcs;
};

/** @} */

#endif /* __DC_HW_TYPE_H__ */
