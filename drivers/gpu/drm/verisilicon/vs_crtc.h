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

#ifndef __VS_CRTC_H__
#define __VS_CRTC_H__

/** @addtogroup DRM
 *  vs drm crtc API.
 *  @ingroup DRIVER
 *
 *  @{
 */

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

/**
 * vs crtc crc source
 */
enum vs_crc_source {
	VS_CRC_SOURCE_AUTO = 0,
	VS_CRC_SOURCE_NONE,
	VS_CRC_SOURCE_NUM,
};

/**
 * vs crtc configuration structure
 *
 */
struct vs_crtc_info {
	/** crtc name */
	const char *name;
	/** max bits per pixel */
	u16 max_bpc;
	/** supported format in DRM_COLOR_FORMAT_<format> */
	u16 color_formats;
	/** gamma table size, set 0 if no gamma supported */
	u16 gamma_size;
	/** gamma bit depth */
	u8 gamma_bits;
	/** display id */
	u8 id;
};

struct vs_crtc;

/**
 * vs crtc function structure
 */
struct vs_crtc_funcs {
	void (*destroy)(struct vs_crtc *vs_crtc);
	void (*enable)(struct vs_crtc *vs_crtc);
	void (*disable)(struct vs_crtc *vs_crtc);
	bool (*mode_fixup)(struct vs_crtc *vs_crtc, const struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
	int (*check)(struct vs_crtc *vs_crtc, struct drm_crtc_state *new_state);
	void (*update)(struct vs_crtc *vs_crtc, struct drm_crtc_state *old_state);
	void (*enable_vblank)(struct vs_crtc *vs_crtc, bool enable);
	void (*enable_crc)(struct vs_crtc *vs_crtc, bool enable);
	void (*commit)(struct vs_crtc *vs_crtc, struct drm_crtc_state *old_state);

	struct drm_crtc_state *(*create_state)(struct vs_crtc *vs_crtc);
	void (*destroy_state)(struct drm_crtc_state *state);
	struct drm_crtc_state *(*duplicate_state)(struct vs_crtc *vs_crtc);
	void (*print_state)(struct drm_printer *p, const struct drm_crtc_state *state);
	int (*set_prop)(struct vs_crtc *vs_crtc, struct drm_crtc_state *state,
			struct drm_property *property, uint64_t val);
	int (*get_prop)(struct vs_crtc *vs_crtc, const struct drm_crtc_state *state,
			struct drm_property *property, uint64_t *val);
};

/**
 * vs crtc structure
 */
struct vs_crtc {
	struct drm_crtc base;
	/** belong to which display controller device. */
	struct device *dev;
	const struct vs_crtc_info *info;
	const struct vs_crtc_funcs *funcs;
};

/**
 * @brief      initialize new vs crtc
 *
 * @param[in]  dev drm device
 * @param[in]  vs_crtc vs crtc object
 * @param[in]  info vs display configuration
 *
 */
int vs_crtc_init(struct drm_device *drm_dev, struct vs_crtc *vs_crtc);

static inline struct vs_crtc *to_vs_crtc(struct drm_crtc *crtc)
{
	return container_of(crtc, struct vs_crtc, base);
}

/**
 * @brief      find crtc by name list
 *
 * @param[in]  dev drm device
 * @param[in]  names crtc names list
 * @param[in]  name_cnt crtc numbers
 *
 */
u32 find_possible_crtcs_with_name(struct drm_device *drm_dev, const char *const names[],
				  u8 name_cnt);

void set_crtc_cursor_plane(struct drm_device *drm_dev, struct drm_plane *drm_plane);
void set_crtc_primary_plane(struct drm_device *drm_dev, struct drm_plane *drm_plane);

/** @} */

#endif /* __VS_CRTC_H__ */
