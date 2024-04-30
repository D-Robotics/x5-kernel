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

#ifndef __VS_PLANE_H__
#define __VS_PLANE_H__

/** @addtogroup DRM
 *  vs drm plane API.
 *  @ingroup DRIVER
 *
 *  @{
 */

#include <drm/drm_plane.h>
#include <drm/drm_plane_helper.h>

#include "vs_fb.h"

/**
 * vs plane z position structure
 */
struct vs_zpos {
	u8 min;
	u8 max;
	u8 val;
};

/**
 * vs plane configuration structure
 */
struct vs_plane_info {
	/** plane name. */
	const char *name;
	/** plane type in enum drm_plane_type */
	enum drm_plane_type type;
	/** supported modifiers list, defined in drm_fourcc.h. */
	u64 *modifiers;
	/** number of format. */
	u16 num_modifiers;
	/** whether above modifier is statically allocated */
	const u32 *formats;
	/** number of format. */
	u16 num_formats;
	/** min of input width. */
	u16 min_width;
	/** min of input height. */
	u16 min_height;
	/** max of input width. */
	u16 max_width;
	/** max of input height. */
	u16 max_height;
	/** bitmask of DRM_MODE_ROTATE_<degrees> and DRM_MODE_REFLECT_<axis>. */
	u16 rotation;
	/** bitmask of DRM_MODE_BLEND_<mode>. */
	u16 blend_mode;
	/** bitmask of enum drm_color_encoding. */
	u16 color_encoding;
	/** min scale ratio in 16.16 fixed point. */
	int min_scale;
	/** max scale ratio in 16.16 fixed point. */
	int max_scale;
	/** z position structure. */
	struct vs_zpos zpos;
	/** crtc name list */
	const char *const *crtc_names;
	/** crtc number */
	u8 num_crtcs;
};

struct vs_plane;

/**
 * vs plane function pointers structure
 */
struct vs_plane_funcs {
	void (*destroy)(struct vs_plane *vs_plane);

	int (*check)(struct vs_plane *vs_plane, struct drm_plane_state *state);
	void (*update)(struct vs_plane *vs_plane, struct drm_plane_state *old_state);
	void (*disable)(struct vs_plane *vs_plane, struct drm_plane_state *old_state);
	int (*check_format_mod)(struct vs_plane *vs_plane, uint32_t format, uint64_t modifier);

	struct drm_plane_state *(*create_state)(struct vs_plane *vs_plane);
	void (*destroy_state)(struct drm_plane_state *state);
	struct drm_plane_state *(*duplicate_state)(struct vs_plane *vs_plane);
	void (*print_state)(struct drm_printer *p, const struct drm_plane_state *state);

	int (*set_prop)(struct vs_plane *vs_plane, struct drm_plane_state *state,
			struct drm_property *property, uint64_t val);
	int (*get_prop)(struct vs_plane *vs_plane, const struct drm_plane_state *state,
			struct drm_property *property, uint64_t *val);
};

/**
 * vs plane structure
 */
struct vs_plane {
	struct drm_plane base;
	/** belong to which display controller device. */
	struct device *dev;
	const struct vs_plane_info *info;
	const struct vs_plane_funcs *funcs;
};

/**
 * @brief      initialize vs plane
 *
 * @param[in]  vs_plane vs plane
 * @param[in]  drm_dev drm device
 *
 */
int vs_plane_init(struct drm_device *drm_dev, struct vs_plane *vs_plane);

static inline struct vs_plane *to_vs_plane(struct drm_plane *plane)
{
	return container_of(plane, struct vs_plane, base);
}

/** @} */

#endif /* __VS_PLANE_H__ */
