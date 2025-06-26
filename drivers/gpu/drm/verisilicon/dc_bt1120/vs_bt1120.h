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

#ifndef __VS_BT1120_H__
#define __VS_BT1120_H__

/** @addtogroup DC
 *  vs dc data types.
 *  @ingroup DRIVER
 *
 *  @{
 */

#include <drm/drm_fourcc.h>

#include "vs_plane.h"
#include "vs_crtc.h"
#include "vs_writeback.h"
#include "vs_sif.h"

#define BT1120_AUX_DEV_NAME_SIZE 16

/**
 * bt1120 image format
 */
enum bt1120_image_format {
	YUV422_PACKED,
	YUV422_SP,
	YUV420_SP,
};

/**
 * display scan timing structure
 *
 *        Sync              Back                Active                 Front
 *                          Porch               Region                 Porch
 *   <----------------><--------------><-----------------------><---------------->
 *                                       //////////////////////|
 *                                      ////////////////////// |
 *                     ................//////////////////////  |..................
 *   __________________
 *   <----- sync ----->
 *   <---------- active_start -------->
 *   <------------------------ active_end --------------------->
 *   <----------------------------------- total --------------------------------->
 *
 */
struct bt1120_scan {
	/** horizontal active start location. */
	u16 h_active_start;
	/** horizontal active stop location. */
	u16 h_active_stop;
	/** horizontal total size. */
	u16 h_total;

	/** horizontal sync start location. */
	u16 h_sync_start;
	/** horizontal sync stop location. */
	u16 h_sync_stop;

	/** vertical active location in line. */
	u16 v_active_start;
	/** vertical active location in line. */
	u16 v_active_stop;
	/** vertical total size in pixel. */
	u16 v_total;

	/** vertical sync start location. */
	u16 v_sync_start;
	/** vertical sync stop location. */
	u16 v_sync_stop;

	/** hfp range. */
	bool hfp_range;

	/** is_interlaced */
	bool is_interlaced;
};

/**
 * bt1120 plane configuration structure
 */
struct bt1120_plane_info {
	/** vs plane configuration */
	struct vs_plane_info info;

	/** dma burst length. */
	u16 dma_burst_len;
};

/**
 * bt1120 display configuration structure
 *
 */
struct bt1120_display_info {
	/** vs crtc configuration */
	struct vs_crtc_info info;
};

/**
 * bt1120 proc info configuration structure
 *
 */
struct bt1120_proc_info {
	/** string to match. */
	const char *name;
};

/**
 * bt1120 writeback configuration structure
 *
 */
struct bt1120_wb_info {
	/** vs writeback configuration */
	struct vs_wb_info info;

	/** bt1120 writeback processor configuration list */
	const struct bt1120_proc_info *proc_info;
};

/**
 * display controller configuration structure
 *
 */
struct bt1120_info {
	/** plane info */
	struct bt1120_plane_info *plane;

	/** display info */
	struct bt1120_display_info *display;

	/** writeback list */
	struct bt1120_wb_info *writebacks;
};

/**
 * bt1120 writeback connector structure
 */
struct bt1120_wb {
	/** vs writeback connector instance. */
	struct vs_wb vs_wb;

	struct bt1120_wb_info *info;

	struct vs_sif *sif;

	/** isr bottom half */
	struct work_struct work;
};

/**
 * bt1120 devices include aux device and bt1120 device
 */
struct bt1120_aux_device {
	/** list head. */
	struct list_head head;
	/** device name from DTS "aux-names" property. */
	char name[BT1120_AUX_DEV_NAME_SIZE];
	/** aux device handler. */
	struct device *dev;
};

/**
 * bt1120 plane instance
 */
struct bt1120_plane {
	/** attached vs_plane. */
	struct vs_plane vs_plane;

	/** pointer to vs_btt120. */
	struct vs_bt1120 *bt1120;

	/** plane info */
	struct bt1120_plane_info *info;
};

/**
 * bt1120 display instance
 */
struct bt1120_disp {
	/** attached vs_crtc. */
	struct vs_crtc vs_crtc;

	/** pointer to vs_btt120. */
	struct vs_bt1120 *bt1120;

	/** display info */
	struct bt1120_display_info *info;

	/** refresh_rate, in Hz. */
	u32 refresh_rate;
};

/**
 * bt1120 crtc state
 */
struct bt1120_disp_state {
	/** drm plane state. */
	struct drm_crtc_state base;

	u32 underflow_cnt;
};

/**
 * vs bt1120 structure
 */
struct vs_bt1120 {
	/** display controller register base. */
	void __iomem *base;
	/** DRM device. */
	struct drm_device *drm_dev;
	/** bt1120 device. */
	struct device *dev;

	struct drm_crtc *drm_crtc;

	/** bt1120 info matches the driver. */
	const struct bt1120_info *info;

	/** pixel clock handler. */
	struct clk *pix_clk;
	/** clock handler of AXI clock. */
	struct clk *axi_clk;
	/** clock handler of APB clock. */
	struct clk *apb_clk;

	/** online/offline mode state */
	bool is_online;

	/** output clk mode */
	bool is_sdr;

	/** odd or even field */
	bool is_odd_field;

	/** is_interlaced */
	bool is_interlaced;

	/** aux device list. */
	struct list_head aux_list;
};

extern struct platform_driver bt1120_platform_driver;
extern struct device *bt1120_dev;

static inline struct bt1120_wb *to_bt1120_wb(struct vs_wb *vs_wb)
{
	return container_of(vs_wb, struct bt1120_wb, vs_wb);
}

static inline struct bt1120_disp_state *to_bt1120_disp_state(const struct drm_crtc_state *state)
{
	return container_of(state, struct bt1120_disp_state, base);
}

static inline struct bt1120_plane *to_bt1120_plane(struct vs_plane *vs_plane)
{
	return container_of(vs_plane, struct bt1120_plane, vs_plane);
}

static inline struct bt1120_disp *to_bt1120_disp(struct vs_crtc *vs_crtc)
{
	return container_of(vs_crtc, struct bt1120_disp, vs_crtc);
}

void bt1120_set_online_configs(struct vs_bt1120 *bt1120, const struct drm_display_mode *mode);
void bt1120_disable(struct vs_bt1120 *bt1120);

/** @} */

#endif /* __VS_BT1120_H__ */
