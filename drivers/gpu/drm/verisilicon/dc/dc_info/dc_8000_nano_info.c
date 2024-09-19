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

#include <drm/drm_blend.h>
#include <drm/drm_atomic_helper.h>

#include "dc_hw.h"
#include "dc_proc.h"
#include "vs_dc.h"
#include "dc_info.h"
#include "dc_hw_proc.h"
#ifdef CONFIG_VERISILICON_GC_PROC_SUPPORT
#include "gc_proc.h"
#endif

#define GAMMA_SIZE    256
#define GAMMA_EX_SIZE 300
#define DEGAMMA_SIZE  260
#define CRTC_NAME     "crtc-dc8000"

static const u32 primary_overlay_formats[] = {
	DRM_FORMAT_ARGB4444, //
	DRM_FORMAT_ARGB1555, //
	DRM_FORMAT_RGB565,   //
	DRM_FORMAT_RGB888,   //
	DRM_FORMAT_ARGB8888, //
	DRM_FORMAT_RGBA4444, //
	DRM_FORMAT_RGBA5551, //
	DRM_FORMAT_RGBA8888, //
	DRM_FORMAT_ABGR4444, //
	DRM_FORMAT_ABGR1555, //
	DRM_FORMAT_BGR565,   //
	DRM_FORMAT_BGR888,   //
	DRM_FORMAT_ABGR8888, //
	DRM_FORMAT_BGRA4444, //
	DRM_FORMAT_BGRA5551, //
	DRM_FORMAT_BGRA8888, //
	DRM_FORMAT_YUYV,     //
	DRM_FORMAT_YVYU,     //
	DRM_FORMAT_NV12,     //
	DRM_FORMAT_NV21,     //
	DRM_FORMAT_XRGB8888, //
	DRM_FORMAT_XBGR8888, //
};

static const u32 gc_overlay_formats[] = {
	DRM_FORMAT_ARGB4444, //
	DRM_FORMAT_ARGB1555, //
	DRM_FORMAT_RGB565,   //
	// DRM_FORMAT_RGB888,   //
	DRM_FORMAT_ARGB8888, //
	DRM_FORMAT_RGBA4444, //
	DRM_FORMAT_RGBA5551, //
	DRM_FORMAT_RGBA8888, //
	DRM_FORMAT_ABGR4444, //
	DRM_FORMAT_ABGR1555, //
	DRM_FORMAT_BGR565,   //
	// DRM_FORMAT_BGR888,   //
	DRM_FORMAT_ABGR8888, //
	DRM_FORMAT_BGRA4444, //
	DRM_FORMAT_BGRA5551, //
	DRM_FORMAT_BGRA8888, //
	DRM_FORMAT_YUYV,     //
	// DRM_FORMAT_YVYU,     //
	DRM_FORMAT_NV12,     //
	DRM_FORMAT_NV21,     //
	DRM_FORMAT_XRGB8888, //
	DRM_FORMAT_XBGR8888, //
};

static const u32 cursor_formats[] = {DRM_FORMAT_ARGB8888};

static const struct vs_color_format_mapping format_map[] = {
	{
		DRM_FORMAT_ARGB4444,
		DC_8000_NANO_FORMAT_A4R4G4B4,
		DC_SWIZZLE_ARGB,
	},
	{
		DRM_FORMAT_RGBA4444,
		DC_8000_NANO_FORMAT_A4R4G4B4,
		DC_SWIZZLE_RGBA,
	},
	{
		DRM_FORMAT_ABGR4444,
		DC_8000_NANO_FORMAT_A4R4G4B4,
		DC_SWIZZLE_ABGR,
	},
	{
		DRM_FORMAT_BGRA4444,
		DC_8000_NANO_FORMAT_A4R4G4B4,
		DC_SWIZZLE_BGRA,
	},
	{
		DRM_FORMAT_ARGB1555,
		DC_8000_NANO_FORMAT_A1R5G5B5,
		DC_SWIZZLE_ARGB,
	},
	{
		DRM_FORMAT_RGBA5551,
		DC_8000_NANO_FORMAT_A1R5G5B5,
		DC_SWIZZLE_RGBA,
	},
	{
		DRM_FORMAT_ABGR1555,
		DC_8000_NANO_FORMAT_A1R5G5B5,
		DC_SWIZZLE_ABGR,
	},
	{
		DRM_FORMAT_BGRA5551,
		DC_8000_NANO_FORMAT_A1R5G5B5,
		DC_SWIZZLE_BGRA,
	},
	{
		DRM_FORMAT_BGR565,
		DC_8000_NANO_FORMAT_R5G6B5,
		DC_SWIZZLE_ABGR,
	},
	{
		DRM_FORMAT_RGB565,
		DC_8000_NANO_FORMAT_R5G6B5,
		DC_SWIZZLE_ARGB,
	},
	{
		DRM_FORMAT_BGR888,
		DC_8000_NANO_FORMAT_R8G8B8,
		DC_SWIZZLE_ABGR,
	},
	{
		DRM_FORMAT_RGB888,
		DC_8000_NANO_FORMAT_R8G8B8,
		DC_SWIZZLE_ARGB,
	},
	{
		DRM_FORMAT_ARGB8888,
		DC_8000_NANO_FORMAT_A8R8G8B8,
		DC_SWIZZLE_ARGB,
	},
	{
		DRM_FORMAT_RGBA8888,
		DC_8000_NANO_FORMAT_A8R8G8B8,
		DC_SWIZZLE_RGBA,
	},
	{
		DRM_FORMAT_ABGR8888,
		DC_8000_NANO_FORMAT_A8R8G8B8,
		DC_SWIZZLE_ABGR,
	},
	{
		DRM_FORMAT_BGRA8888,
		DC_8000_NANO_FORMAT_A8R8G8B8,
		DC_SWIZZLE_BGRA,
	},
	{
		DRM_FORMAT_YUYV,
		DC_8000_NANO_FORMAT_YUY2,
		DC_SWIZZLE_UV,
	},
	{
		DRM_FORMAT_YVYU,
		DC_8000_NANO_FORMAT_YUY2,
		DC_SWIZZLE_VU,
	},
	{
		DRM_FORMAT_NV12,
		DC_8000_NANO_FORMAT_NV12,
		DC_SWIZZLE_UV,
	},
	{
		DRM_FORMAT_NV21,
		DC_8000_NANO_FORMAT_NV12,
		DC_SWIZZLE_VU,
	},
	{
		DRM_FORMAT_XRGB8888,
		DC_8000_NANO_FORMAT_A8R8G8B8,
		DC_SWIZZLE_ARGB,
	},
	{
		DRM_FORMAT_XBGR8888,
		DC_8000_NANO_FORMAT_A8R8G8B8,
		DC_SWIZZLE_ABGR,
	},
	{
		DRM_FORMAT_TABLE_END,
		DC_8000_NANO_FORMAT_A8R8G8B8,
		DC_SWIZZLE_ARGB,
	},
};

static const struct vs_modifier_mapping modifier_map[] = {
	{DRM_FORMAT_MOD_LINEAR, DC_8000_NANO_LINEAR, DC_COMPRESS_NONE},
	{fourcc_mod_vs_norm_code(DRM_FORMAT_MOD_VS_TILED4x4), DC_8000_NANO_TILE_4X4,
	 DC_COMPRESS_NONE},
	{fourcc_mod_vs_norm_code(DRM_FORMAT_MOD_VS_TILE_8X8), DC_8000_NANO_TILE_8X8,
	 DC_COMPRESS_NONE},
	{DRM_MODIFIER_TABLE_END, DC_8000_NANO_LINEAR, DC_COMPRESS_NONE},
};

static const struct vs_color_gamut_mapping gamut_map[] = {
	{DRM_COLOR_YCBCR_BT601, DC_8000_NANO_COLOR_GAMUT_601},
	{DRM_COLOR_YCBCR_BT709, DC_8000_NANO_COLOR_GAMUT_709},
	{DRM_GAMUT_TABLE_END, DC_8000_NANO_COLOR_GAMUT_601},
};

static const u32 tile_4x4_fourccs[] = {DRM_FORMAT_ARGB8888,
				       DRM_FORMAT_ABGR8888,
				       DRM_FORMAT_RGBA8888,
				       DRM_FORMAT_BGRA8888,
				       DRM_FORMAT_YUYV,
				       DRM_FORMAT_YVYU,
				       0};

static const u32 tile_8x8_fourccs[] = {DRM_FORMAT_NV12, DRM_FORMAT_NV21, 0};

static struct mod_format_mapping dc_mod_format_map[] = {
	{
		fourcc_mod_vs_norm_code(DRM_FORMAT_MOD_VS_TILED4x4),
		&tile_4x4_fourccs,
	},
	{
		fourcc_mod_vs_norm_code(DRM_FORMAT_MOD_VS_TILE_8X8),
		&tile_8x8_fourccs,
	},
	{}};

/* put dc to first element */
#ifdef CONFIG_VERISILICON_GC_PROC_SUPPORT
static const struct dc_proc_info dc_proc_info_gpu_0[] = {
	{
		.name  = GC_HW_NAME,
		.info  = &((struct gpu_plane_info){
			 .features = GPU_PLANE_OUT,
			 .id	   = 0,
		 }),
		.funcs = &gpu_plane_funcs,
	},
	{
		.name  = DC_HW_DEV_NAME,
		.info  = &((struct dc_hw_plane_info){
			 {
				 .features = DC_PLANE_POSITION | DC_PLANE_BLEND | DC_PLANE_YUV2RGB,
				 .id	   = DC_SET_ID(DC_PRIMARY_PLANE, 0),
			 },
			 .format	 = format_map,
			 .modifier	 = modifier_map,
			 .mod_format_map = dc_mod_format_map,
			 .gamut		 = gamut_map,
		 }),
		.funcs = &dc_hw_plane_funcs,
	},
	{
		.name  = GC_HW_NAME,
		.info  = &((struct gpu_plane_info){
			 .features = GPU_PLANE_IN | GPU_PLANE_SCALE | GPU_PLANE_ROTATION,
			 .id	   = 0,
			 .fourcc   = DRM_FORMAT_NV12,
		 }),
		.funcs = &gpu_plane_funcs,
	},
};
#else
static const struct dc_proc_info dc_proc_info_primary[] = {
	{
		.name  = DC_HW_DEV_NAME,
		.info  = &((struct dc_hw_plane_info){
			 {
				 .features = DC_PLANE_POSITION | DC_PLANE_BLEND | DC_PLANE_YUV2RGB,
				 .id	   = DC_SET_ID(DC_PRIMARY_PLANE, 0),
			 },
			 .format	 = format_map,
			 .modifier	 = modifier_map,
			 .mod_format_map = dc_mod_format_map,
			 .gamut		 = gamut_map,
		 }),
		.funcs = &dc_hw_plane_funcs,
	},
};
#endif

#ifdef CONFIG_VERISILICON_GC_PROC_SUPPORT
static const struct dc_proc_info dc_proc_info_gpu_0[] = {
	{
		.name  = GC_HW_NAME,
		.info  = &((struct gpu_plane_info){
			 .features = GPU_PLANE_OUT,
			 .id	   = 0,
		 }),
		.funcs = &gpu_plane_funcs,
	},
	{
		.name  = DC_HW_DEV_NAME,
		.info  = &((struct dc_hw_plane_info){
			 {
				 .features = DC_PLANE_POSITION | DC_PLANE_BLEND | DC_PLANE_YUV2RGB,
				 .id	   = DC_SET_ID(DC_OVERLAY_PLANE, 0),
			 },
			 .format	 = format_map,
			 .modifier	 = modifier_map,
			 .mod_format_map = dc_mod_format_map,
			 .gamut		 = gamut_map,
		 }),
		.funcs = &dc_hw_plane_funcs,
	},
	{
		.name  = GC_HW_NAME,
		.info  = &((struct gpu_plane_info){
			 .features = GPU_PLANE_IN | GPU_PLANE_SCALE | GPU_PLANE_ROTATION,
			 .id	   = 0,
			 .fourcc   = DRM_FORMAT_NV12,
		 }),
		.funcs = &gpu_plane_funcs,
	},
};
#else
static const struct dc_proc_info dc_proc_info_overlay_0[] = {
	{
		.name  = DC_HW_DEV_NAME,
		.info  = &((struct dc_hw_plane_info){
			 {
				 .features = DC_PLANE_POSITION | DC_PLANE_BLEND | DC_PLANE_YUV2RGB,
				 .id	   = DC_SET_ID(DC_OVERLAY_PLANE, 0),
			 },
			 .format	 = format_map,
			 .modifier	 = modifier_map,
			 .mod_format_map = dc_mod_format_map,
			 .gamut		 = gamut_map,
		 }),
		.funcs = &dc_hw_plane_funcs,
	},
};

static const struct dc_proc_info dc_proc_info_overlay_1[] = {
	{
		.name  = DC_HW_DEV_NAME,
		.info  = &((struct dc_hw_plane_info){
			 {
				 .features = DC_PLANE_POSITION | DC_PLANE_BLEND,
				 .id	   = DC_SET_ID(DC_OVERLAY_PLANE, 1),
			 },
			 .format   = format_map,
			 .modifier = modifier_map,
		 }),
		.funcs = &dc_hw_plane_funcs,
	},
};

static const struct dc_proc_info dc_proc_info_cursor[] = {
	{
		.name  = DC_HW_DEV_NAME,
		.info  = &((struct dc_hw_proc_info){
			 .features = DC_PLANE_CURSOR_MASKED,
			 .id	   = DC_SET_ID(DC_CURSOR_PLANE, 0),
		 }),
		.funcs = &dc_hw_plane_funcs,
	},
};

static const struct dc_proc_info dc_proc_info_display[] = {
	{
		.name  = DC_HW_DEV_NAME,
		.info  = &((struct dc_hw_disp_info){
			 {
				 .features = DC_DISPLAY_GAMMA | DC_DISPLAY_DITHER,
				 .id	   = DC_SET_ID(DC_DISPLAY, 0),
			 },
			 .clk_name = "pix_clk",
		 }),
		.funcs = &dc_hw_crtc_funcs,
	},
};

#ifdef CONFIG_DC8000_NANO_WRITEBACK_DEBUG
static const u32 wb_formats[] = {
	DRM_FORMAT_ARGB8888,
	/* TODO. */
};

static const struct dc_proc_info dc_proc_info_wb_0[] = {
	{
		.name  = DC_HW_DEV_NAME,
		.info  = &((struct dc_hw_wb_info){
			 {
				 .features = DC_DISPLAY_WRITEBACK,
				 .id	   = DC_SET_ID(DC_DISPLAY, 0),
			 },
		 }),
		.funcs = &dc_hw_wb_funcs,
	},
};
#endif

#ifdef CONFIG_VERISILICON_WRITEBACK_SIF
static const u32 wb_formats[] = {
	DRM_FORMAT_BGR565, DRM_FORMAT_XBGR8888, DRM_FORMAT_YUYV,
	/* TODO. */
};

static const struct dc_proc_info dc_proc_info_wb_0[] = {
	{
		.name  = "vs_sif",
		.funcs = &vs_sif_wb_funcs,
	},
};
#endif

static const char *const crtc_names[] = {
	CRTC_NAME,
};

static struct dc_plane_info dc_plane_info[] = {
	{
		.info =
			{
				.name	       = "Primary",
				.type	       = DRM_PLANE_TYPE_PRIMARY,
				.modifiers     = format_modifiers,
				.num_modifiers = NUM_MODIFIERS,
#ifdef CONFIG_VERISILICON_GC_PROC_SUPPORT
				.num_formats = ARRAY_SIZE(gc_overlay_formats),
				.formats     = gc_overlay_formats,
				.rotation    = DRM_MODE_ROTATE_MASK | DRM_MODE_REFLECT_MASK,
				.min_scale   = FRAC_16_16(1, 8),
				.max_scale   = FRAC_16_16(8, 1),
#else
				.num_formats = ARRAY_SIZE(primary_overlay_formats),
				.formats     = primary_overlay_formats,
				.min_scale   = DRM_PLANE_NO_SCALING,
				.max_scale   = DRM_PLANE_NO_SCALING,
#endif
				.min_width  = 0,
				.min_height = 0,
				.max_width  = 4096,
				.max_height = 4096,
				.blend_mode = BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					      BIT(DRM_MODE_BLEND_PREMULTI) |
					      BIT(DRM_MODE_BLEND_COVERAGE),
				.color_encoding =
					BIT(DRM_COLOR_YCBCR_BT601) | BIT(DRM_COLOR_YCBCR_BT709),

				.zpos	    = {0, 2, 0},
				.crtc_names = crtc_names,
				.num_crtcs  = ARRAY_SIZE(crtc_names),
			},
#ifdef CONFIG_VERISILICON_GC_PROC_SUPPORT
		.proc_info = dc_proc_info_gpu_0,
		.num_proc  = ARRAY_SIZE(dc_proc_info_gpu_0),
#else
		.proc_info = dc_proc_info_primary,
		.num_proc  = ARRAY_SIZE(dc_proc_info_primary),
#endif
	},
	{
		.info =
			{
				.name	       = "Overlay_0",
				.type	       = DRM_PLANE_TYPE_OVERLAY,
				.modifiers     = format_modifiers,
				.num_modifiers = NUM_MODIFIERS,
				.min_width     = 0,
				.min_height    = 0,
				.max_width     = 4096,
				.max_height    = 4096,
				.blend_mode    = BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					      BIT(DRM_MODE_BLEND_PREMULTI) |
					      BIT(DRM_MODE_BLEND_COVERAGE),
				.color_encoding =
					BIT(DRM_COLOR_YCBCR_BT601) | BIT(DRM_COLOR_YCBCR_BT709),
				.num_formats = ARRAY_SIZE(primary_overlay_formats),
				.formats     = primary_overlay_formats,
				.min_scale   = DRM_PLANE_NO_SCALING,
				.max_scale   = DRM_PLANE_NO_SCALING,
				.zpos	     = {0, 2, 1},
				.crtc_names  = crtc_names,
				.num_crtcs   = ARRAY_SIZE(crtc_names),
			},
#ifdef CONFIG_VERISILICON_GC_PROC_SUPPORT
		.proc_info = dc_proc_info_gpu_0,
		.num_proc  = ARRAY_SIZE(dc_proc_info_gpu_0),
#else
		.proc_info = dc_proc_info_overlay_0,
		.num_proc  = ARRAY_SIZE(dc_proc_info_overlay_0),
#endif
	},
	{
		.info =
			{
				.name	       = "Overlay_1",
				.type	       = DRM_PLANE_TYPE_OVERLAY,
				.modifiers     = format_modifiers,
				.num_modifiers = NUM_MODIFIERS,
				.min_width     = 0,
				.min_height    = 0,
				.max_width     = 4096,
				.max_height    = 4096,
				.blend_mode    = BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					      BIT(DRM_MODE_BLEND_PREMULTI) |
					      BIT(DRM_MODE_BLEND_COVERAGE),

				.num_formats = ARRAY_SIZE(primary_overlay_formats),
				.formats     = primary_overlay_formats,
				.min_scale   = DRM_PLANE_NO_SCALING,
				.max_scale   = DRM_PLANE_NO_SCALING,
				.zpos	     = {0, 2, 2},
				.crtc_names  = crtc_names,
				.num_crtcs   = ARRAY_SIZE(crtc_names),
			},
		.proc_info = dc_proc_info_overlay_1,
		.num_proc  = ARRAY_SIZE(dc_proc_info_overlay_1),
	},
	{
		.info =
			{
				.name	     = "Cursor",
				.type	     = DRM_PLANE_TYPE_CURSOR,
				.num_formats = ARRAY_SIZE(cursor_formats),
				.formats     = cursor_formats,
				.modifiers   = NULL,
				.min_width   = 32,
				.min_height  = 32,
				.max_width   = 32,
				.max_height  = 32,
				.min_scale   = DRM_PLANE_NO_SCALING,
				.max_scale   = DRM_PLANE_NO_SCALING,
				.zpos	     = {255, 255, 255},
				.crtc_names  = crtc_names,
				.num_crtcs   = ARRAY_SIZE(crtc_names),
			},
		.proc_info = dc_proc_info_cursor,
		.num_proc  = ARRAY_SIZE(dc_proc_info_cursor),
	},
};

static struct dc_crtc_info dc_display_info[] = {
	{
		.info =
			{
				.name	 = CRTC_NAME,
				.max_bpc = 10,
				.color_formats =
					DRM_COLOR_FORMAT_RGB444 | DRM_COLOR_FORMAT_YCBCR444 |
					DRM_COLOR_FORMAT_YCBCR422 | DRM_COLOR_FORMAT_YCBCR420,
				.gamma_size    = GAMMA_SIZE,
				.gamma_bits    = 8,
				.vblank_bit    = BIT(0),
				.underflow_bit = BIT(29),
			},
		.id	   = 0,
		.proc_info = dc_proc_info_display,
		.num_proc  = ARRAY_SIZE(dc_proc_info_display),
	},
};

#ifdef CONFIG_VERISILICON_WRITEBACK

static const char *const wb_crtc_names[] = {CRTC_NAME};

static struct dc_wb_info dc_wb_info[] = {
	{
		.info =
			{
				.wb_formats	= wb_formats,
				.num_wb_formats = ARRAY_SIZE(wb_formats),
				.crtc_names	= wb_crtc_names,
				.num_crtcs	= ARRAY_SIZE(wb_crtc_names),
			},
		.proc_info = dc_proc_info_wb_0,
		.num_proc  = ARRAY_SIZE(dc_proc_info_wb_0),
	},
};
#endif

const struct dc_info dc_8000_nano_info = {
	.name	     = "DC8000Nano",
	.family	     = DC_8000_NANO_FAMILY,
	.num_plane   = ARRAY_SIZE(dc_plane_info),
	.planes	     = dc_plane_info,
	.num_display = ARRAY_SIZE(dc_display_info),
	.displays    = dc_display_info,
#ifdef CONFIG_VERISILICON_WRITEBACK
	.num_wb	    = ARRAY_SIZE(dc_wb_info),
	.writebacks = dc_wb_info,
#endif
	.min_width	 = 0,
	.min_height	 = 0,
	.max_width	 = 4096,
	.max_height	 = 4096,
	.cursor_width	 = 32,
	.cursor_height	 = 32,
	.pitch_alignment = 192,
};
