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

#ifndef __DC_INFO_H__
#define __DC_INFO_H__

/** @addtogroup DC
 *  vs dc info types.
 *  @ingroup DRIVER
 *
 *  @{
 */

#define DC_DEV_NAME_SIZE 16

/**
 * dc plane configuration structure
 */
struct dc_plane_info {
	/** vs plane configuration */
	struct vs_plane_info info;
	/** number of dc processor. */
	u8 num_proc;
	/** dc processor config list. */
	const struct dc_proc_info *proc_info;
};

/**
 * dc display configuration structure
 *
 */
struct dc_crtc_info {
	/** vs crtc configuration */
	struct vs_crtc_info info;
	/** display id */
	u8 id;
	/** number of dc processor */
	u8 num_proc;
	/** dc processor configuration list */
	const struct dc_proc_info *proc_info;
};

/**
 * dc writeback configuration structure
 *
 */
struct dc_wb_info {
	/** vs writeback configuration */
	struct vs_wb_info info;

	/** number of dc writeback processor */
	u8 num_proc;
	/** dc writeback processor configuration list */
	const struct dc_proc_info *proc_info;
};

/**
 * display controller configuration structure
 *
 */
struct dc_info {
	/** display controller name */
	const char *name;
	/** dc plane list */
	struct dc_plane_info *planes;
	/** dc display list */
	struct dc_crtc_info *displays;
	/** dc writeback list */
	struct dc_wb_info *writebacks;
	/** number of planes */
	u8 num_plane;
	/** number of displays */
	u8 num_display;
	/** number of writeback connectors */
	u8 num_wb;
	/** chip family */
	u8 family;
	/** minimum fb pixel width on this display controller */
	u16 min_width;
	/** minimum fb pixel height on this display controller */
	u16 min_height;
	/** maximum fb pixel width on this display controller */
	u16 max_width;
	/** maximum fb pixel height on this display controller */
	u16 max_height;
	/** max cursor width */
	u16 cursor_width;
	/** max cursor height */
	u16 cursor_height;
	/** buffer access byte alignment for HW */
	u16 pitch_alignment;
};

extern const struct dc_info dc_8000_nano_info;
extern const struct dc_proc_funcs dc_hw_plane_funcs;
extern const struct dc_proc_funcs dc_hw_crtc_funcs;
extern const struct dc_proc_funcs dc_hw_wb_funcs;
extern const struct dc_proc_funcs vs_sif_wb_funcs;

/** @} */

#endif /* __DC_INFO_H__ */
