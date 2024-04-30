/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_DRM_H__
#define __VS_DRM_H__

#include <drm/drm_file.h>
#include "drm.h"

enum drm_vs_degamma_mode {
	VS_DEGAMMA_DISABLE = 0,
	VS_DEGAMMA_BT709 = 1,
	VS_DEGAMMA_BT2020 = 2,
};

/* histogram input location */
enum vs_histogram_input_pos {
	/* histogram output after alpha blending */
	VS_HISTOGRAM_BLENDING = 0,
	/* histogram output after 3D LUT */
	VS_HISTOGRAM_3D_LUT,
	/* histogram output after linear csc */
	VS_HISTOGRAM_CSC,
};

/* histogram bin mode */
enum vs_histogram_bin_mode {
	/* maximum value among the R, G, B channels of each sample pixel is binned */
	VS_HISTO_BIN_MAX = 0,
	/* weighted sum of the R, G, B channels for each sample pixel is binned */
	VS_HISTO_BIN_WEIGHTED,
};

/**
 * struct drm_vs_histogram_conf - histogram configuration
 *
 * @index: index of ROI to which the current histogram binds
 * @enable: whether to enable histogram or not
 * @outter_x1: outside boundary left coordinate
 * @outter_y1: outside boundary top coordinate
 * @outter_x2: outside boundary right coordinate
 * @outter_y2: outside boundary bottom coordinate
 * @inner_x1: inside boundary left coordinate
 * @inner_y1: inside boundary top coordinate
 * @inner_x2: inside boundary right coordinate
 * @inner_y2: inside boundary bottom coordinate
 * @coef_r: weighted coefficient of R channel
 * @coef_g: weighted coefficient of G channel
 * @coef_b: weighted coefficient of B channel
 * @input_pos: enum vs_histogram_input_pos
 * @bin_mode: enum vs_histogram_bin_mode
 */
struct drm_vs_histogram_conf {
	__u8	index;
	__u8	enable;
	__u16	outter_x1;
	__u16	outter_y1;
	__u16	outter_x2;
	__u16	outter_y2;
	__u16	inner_x1;
	__u16	inner_y1;
	__u16	inner_x2;
	__u16	inner_y2;

	__u16	coef_r;
	__u16	coef_g;
	__u16	coef_b;

	enum vs_histogram_input_pos intput_pos;
	enum vs_histogram_bin_mode bin_mode;
};

/**
 * struct drm_vs_histogram_data - histogram configuration
 *
 * @values: histogram values
 * @index: index of ROI to which the current histogram binds
 */
struct drm_vs_histogram_data {
	__u32 values[256];
	__u8 index;
};

/**
 * struct drm_vs_histogram_setup - histogram configuration
 *
 * @crtc_id: crtc to which histogram setup binds to
 */
struct drm_vs_histogram_setup {
	__u32 crtc_id;
};

/**
 * struct drm_vs_threed_conf - 3d configuration
 *
 * @values: 3d LUT values
 */
struct drm_vs_threed_conf {
	__u32 enlarge;
	__u16 scale_r;
	__u16 scale_g;
	__u16 scale_b;
	__u16 offset_r;
	__u16 offset_g;
	__u16 offset_b;
	__u32 reserved;
	__u32 values[0];
};

/**
 * struct drm_vs_lcsc_csc - linear CSC configuration
 */
struct drm_vs_lcsc_csc {
	__u16	pre_gain_red;
	__u16	pre_gain_green;
	__u16	pre_gain_blue;
	__u16	pre_offset_red;
	__u16	pre_offset_green;
	__u16	pre_offset_blue;
	__u32	coef[9];
	__u32	post_gain_red;
	__u32	post_gain_green;
	__u32	post_gain_blue;
	__u32	post_offset_red;
	__u32	post_offset_green;
	__u32	post_offset_blue;
};

/**
 * struct drm_vs_threed_data - 3d data
 *
 * @values: histogram values
 */
struct drm_vs_threed_data {
	__u32 values[4913];
};

#define DRM_VS_HISTOGRAM_SETUP		0x00

#define DRM_IOCTL_VS_HISTOGRAM_SETUP	DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_VS_HISTOGRAM_SETUP, struct drm_vs_histogram_setup)

/* VS specific events */
#define DRM_VS_HISTOGRAM_EVENT		0x80000000

/**
 * struct drm_vs_histogram_event - histogram event
 *
 * @base: drm_event to inform user space
 * @blob_id: output histogram blob data
 */
struct drm_vs_histogram_event {
	struct drm_event base;
	__u32 sequence;
	__u32 blob_id;
};

#endif /* __VS_DRM_H__ */
