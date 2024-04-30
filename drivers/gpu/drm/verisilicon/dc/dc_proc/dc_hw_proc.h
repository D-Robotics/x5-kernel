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

#ifndef __DC_HW_PROC_H__
#define __DC_HW_PROC_H__

/** @addtogroup DC
 *  vs dc hw processor type and API.
 *  @ingroup DRIVER
 *
 *  @{
 */

#include "dc_hw.h"

#define DRM_FORMAT_TABLE_END   0
#define DRM_MODIFIER_TABLE_END 0xFFFF
#define DRM_ROTATION_TABLE_END 0
#define DRM_TILE_TABLE_END     0xFF
#define DRM_GAMUT_TABLE_END    0xFF

#define FRAC_16_16(mult, div) (((mult) << 16) / (div))

/**
 * display controller gamut
 */
enum dc_gamut {
	DC_GAMUT_BT601 = 0,
	DC_GAMUT_BT709,
	DC_GAMUT_BT2020,
};

/**
 * map DRM color format to DC color format
 */
struct vs_color_format_mapping {
	/** DRM color format. */
	u32 drm_format;
	/** dc color format, enum dc_xxxx_color_format. */
	u8 dc_format;
	/** ARGB/UV swizzle, enum dc_hw_swizzle/dc_hw_swizzle_uv. */
	u8 dc_swizzle;
};

/**
 * map DRM modifier to DC tile/compression mode
 */
struct vs_modifier_mapping {
	/** DRM modifier. */
	u64 drm_modifier;
	/** dc tile mode, enum dc_xxxx_tile_mode. */
	u8 dc_tile_mode;
	/** compress mode, enum dc_hw_compress_mode. */
	u8 dc_compress_mode;
};

/**
 * map DRM rotation to DC rotation
 */
struct vs_rotation_mapping {
	/** DRM rotation and reflect mode. */
	u8 drm_rotation;
	/** dc rotation, enum dc_xxxx_rotation. */
	u8 dc_rotation;
};

/**
 * map DRM color gamut to DC color gamut
 */
struct vs_color_gamut_mapping {
	/** DRM rotation and reflect mode. */
	u8 drm_color_gamut;
	/** dc color gamut, enum dc_xxxx_color_gamut. */
	u8 dc_color_gamut;
};

/**
 * hw plane configuration
 */
struct dc_hw_plane_info {
	/** hw processor info. */
	struct dc_hw_proc_info info;
	/** color format mapping table. */
	const struct vs_color_format_mapping *format;
	/** modifier mapping table. */
	const struct vs_modifier_mapping *modifier;
	/** rotation mapping table. */
	const struct vs_rotation_mapping *rotation;
	/** color gamut mapping table. */
	const struct vs_color_gamut_mapping *gamut;
	/** map DRM modifier to all supported color format */
	const struct mod_format_mapping *mod_format_map;
	/** degamma table size. */
	u16 degamma_size;
	u8 degamma_bits;
};

/**
 * hw display configuration
 */
struct dc_hw_disp_info {
	/** hw processor info. */
	struct dc_hw_proc_info info;
	/** pixel clock name of this display. */
	const char *clk_name;
	u8 lcsc_degamma_bits;
	u8 lcsc_regamma_bits;
	u16 lcsc_degamma_size;
	u16 lcsc_regamma_size;
};

/**
 * hw display writeback information structure
 */
struct dc_hw_wb_info {
	/** hw processor info. */
	struct dc_hw_proc_info info;
	/** writeback color format mapping table. */
	const struct vs_color_format_mapping *format;
	/** writeback modifier mapping table. */
	const struct vs_modifier_mapping *modifier;
	/** color gamut mapping table */
	const struct vs_color_gamut_mapping *gamut;
};

/**
 * hw plane structure
 */
struct dc_plane_proc {
	struct dc_proc base;
	/** plane. */
	struct dc_hw_processor plane;
};

static inline struct dc_plane_proc *to_dc_plane_proc(struct dc_proc *proc)
{
	return container_of(proc, struct dc_plane_proc, base);
}

static inline struct drm_plane *to_drm_plane(struct dc_proc *proc)
{
	struct dc_plane *dc_plane = proc->parent;

	return &dc_plane->vs_plane.base;
}

struct dc_crtc_proc {
	struct dc_proc base;

	struct dc_hw_processor disp;
	/** pixel clock handler. */
	struct clk *pix_clk;
	bool crc_enable;
	u8 crc_skip_count;

	enum dc_hw_port out_port;

	struct work_struct work;
};

static inline struct dc_crtc_proc *to_dc_crtc_proc(struct dc_proc *proc)
{
	return container_of(proc, struct dc_crtc_proc, base);
}

static inline struct drm_crtc *to_drm_crtc(struct dc_proc *proc)
{
	struct dc_crtc *dc_crtc = proc->parent;

	return &dc_crtc->vs_crtc.base;
}

/**
 * writeback processor instance
 */
struct dc_wb_proc {
	struct dc_proc base;
	/** writeback. */
	struct dc_hw_processor wb;
};

static inline struct dc_wb_proc *to_dc_wb_proc(struct dc_proc *proc)
{
	return container_of(proc, struct dc_wb_proc, base);
}

static inline struct drm_connector *to_drm_connector(struct dc_proc *proc)
{
	struct dc_wb *dc_wb = proc->parent;

	return &dc_wb->vs_wb.base.base;
}

/**
 * plane proc state
 */
struct dc_plane_proc_state {
	struct dc_proc_state base;
	struct dc_plane_state *parent;
	/** current degamma lookup table. */
	struct drm_property_blob *degamma_lut;
	/** current gamut conv matrix. */
	struct drm_property_blob *gamut_conv_matrix;
	/** current gamut conv. */
	u8 gamut_conv;
	/** degamma change flag. */
	bool degamma_changed : 1;
	/** gamut conv matrix change flag. */
	bool gamut_conv_changed : 1;
	bool cursor_masked : 1;
};

static inline struct dc_plane_proc_state *to_dc_plane_proc_state(struct dc_proc_state *state)
{
	return container_of(state, struct dc_plane_proc_state, base);
}

/**
 * hw plane state
 */
struct dc_crtc_proc_state {
	struct dc_proc_state base;
	struct dc_crtc_state *parent;
	/** pixel clock rate, in KHz. */
	u32 pix_clk_rate;
	u32 underflow_cnt;
	u32 bg_color;
	bool lcsc0_changed : 1;
	bool lcsc1_changed : 1;
	bool bg_color_changed : 1;
	bool dither_changed : 1;
	struct drm_property_blob *lcsc0_degamma;
	struct drm_property_blob *lcsc0_csc;
	struct drm_property_blob *lcsc0_regamma;
	struct drm_property_blob *lcsc1_degamma;
	struct drm_property_blob *lcsc1_csc;
	struct drm_property_blob *lcsc1_regamma;
	struct drm_property_blob *dither;
};

static inline struct dc_crtc_proc_state *to_dc_crtc_proc_state(struct dc_proc_state *state)
{
	return container_of(state, struct dc_crtc_proc_state, base);
}

/**
 * hw writeback state
 */
struct dc_wb_proc_state {
	struct dc_proc_state base;
	struct dc_wb_state *parent;
	/** current writeback position. */
	enum dc_hw_wb_pos wb_position;
	/** Color encoding for non RGB formats */
	enum drm_color_encoding color_encoding;
};

static inline struct dc_wb_proc_state *to_dc_wb_proc_state(const struct dc_proc_state *state)
{
	return container_of(state, struct dc_wb_proc_state, base);
}

static inline struct dc_hw_plane_info *to_dc_hw_plane_info(const struct dc_hw_proc_info *info)
{
	return container_of(info, struct dc_hw_plane_info, info);
}

static inline struct dc_hw_disp_info *to_dc_hw_disp_info(const struct dc_hw_proc_info *info)
{
	return container_of(info, struct dc_hw_disp_info, info);
}

static inline struct dc_hw_wb_info *to_dc_hw_wb_info(const struct dc_hw_proc_info *info)
{
	return container_of(info, struct dc_hw_wb_info, info);
}

/**
 * @brief find address of the framebuffer
 *
 * @param[in] drm_fb drm framebuffer
 * @param[in] dma_addr[] dma address
 *
 */
void get_buffer_addr(struct drm_framebuffer *drm_fb, dma_addr_t dma_addr[]);

/** @} */

#endif /* __DC_HW_PROC_H__ */
