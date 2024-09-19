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

#ifndef __DC_PROC_H__
#define __DC_PROC_H__

/** @addtogroup DC
 *  vs dc processor data and API.
 *  @ingroup DRIVER
 *
 *  @{
 */

#include <drm/drm_fourcc.h>
#include <drm/drm_bridge.h>

#include "vs_plane.h"
#include "vs_crtc.h"
#include "vs_writeback.h"

#define fourcc_mod_vs_get_tile_mode(val) ((u8)((val) & DRM_FORMAT_MOD_VS_DEC_TILE_MODE_MASK))

#define fourcc_mod_vs_get_type(val) (((val) & DRM_FORMAT_MOD_VS_TYPE_MASK) >> 54)

extern u64 format_modifiers[];
#define NUM_MODIFIERS 2

#define DC_DEV_NAME_SIZE 16
/**
 * dc device for dc processor
 * dc devices include aux device and dc familay device
 */
struct dc_device {
	/** list head. */
	struct list_head head;
	/** device name from DTS "aux-names" property. */
	char name[DC_DEV_NAME_SIZE];
	/** aux device handler. */
	struct device *dev;
};

/*
 * Mapping of DEC400 modifier to all supported color format
 */
struct mod_format_mapping {
	/** DRM modifier */
	u64 modifier;
	/** dc color format mask */
	const u32 (*fourccs)[];
};

/**
 * display controller processor event
 */
enum dc_proc_event {
	DC_PROC_EVENT_PQ = 0,
	DC_PROC_EVENT_COUNT,
};

/**
 * display controller processor pq event
 */
struct dc_proc_pq_event {
	u8 id;
	union {
		u8 delay;
		u8 out_gamut;
	} data;
	bool enable;
};

struct dc_proc_info {
	/** string to match. */
	const char *name;
	/** generic processor configuration. */
	const void *info;
	/** dc plane functions. */
	const struct dc_proc_funcs *funcs;
};

struct dc_proc_funcs {
	struct dc_proc *(*create)(struct device *dev, const struct dc_proc_info *info);
	int (*post_create)(struct dc_proc *dc_proc);
	void (*destroy)(struct dc_proc *dc_proc);
	void (*update_info)(struct dc_proc *dc_proc, void *info);
	int (*check)(struct dc_proc *dc_proc, void *state);
	void (*update)(struct dc_proc *dc_proc, void *old_state);
	void (*enable)(struct dc_proc *dc_proc);
	void (*disable)(struct dc_proc *dc_proc, void *old_state);
	void (*commit)(struct dc_proc *dc_proc);
	void (*vblank)(struct dc_proc *dc_proc);
	void (*suspend)(struct dc_proc *dc_proc);
	void (*resume)(struct dc_proc *dc_proc);
	// parent dc_crtc_state/dc_plane_state/dc_wb_state
	struct dc_proc_state *(*create_state)(void *parent, struct dc_proc *dc_proc);
	void (*destroy_state)(struct dc_proc_state *state);
	struct dc_proc_state *(*duplicate_state)(void *parent, struct dc_proc *dc_proc,
						 void *state);
	void (*print_state)(struct drm_printer *p, struct dc_proc_state *state);
	int (*create_prop)(struct dc_proc *dc_proc);
	int (*set_prop)(struct dc_proc *dc_proc, struct dc_proc_state *state,
			struct drm_property *property, uint64_t val);
	int (*get_prop)(struct dc_proc *dc_proc, struct dc_proc_state *state,
			struct drm_property *property, uint64_t *val);

	bool (*mode_fixup)(struct dc_proc *dc_proc, const struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);

	void (*enable_vblank)(struct dc_proc *dc_proc, bool enable);
	void (*enable_crc)(struct dc_proc *dc_proc, bool enable);
	int (*check_format_mod)(struct dc_proc *dc_proc, uint32_t format, uint64_t modifier);
};

/**
 * generic dc proc
 */
struct dc_proc {
	struct list_head head;
	/** back pointer to dc_plane/dc_crtc/dc_wb **/
	void *parent;

	const struct dc_proc_info *info;
};

/**
 * generic dc proc state
 */
struct dc_proc_state {
	/** list head. */
	struct list_head head;
	/** dc processor handler. */
	struct dc_proc *proc;
};

/**
 * dc plane structure
 */
struct dc_plane {
	/** dc plane processor list. */
	struct list_head list;

	/** back pointer to vs_dc */
	struct list_head head;

	/** vs plane instance. */
	struct vs_plane vs_plane;

	struct dc_plane_info *info;
};

static inline struct vs_plane *proc_to_vs_plane(struct dc_proc *proc)
{
	struct dc_plane *dc_plane = (struct dc_plane *)proc->parent;

	return &dc_plane->vs_plane;
}

struct dc_plane_state {
	/** dc plane processor state list. */
	struct list_head list;

	/** drm plane state. */
	struct drm_plane_state base;
};

/**
 * dc crtc structure
 */
struct dc_crtc {
	/** dc crtc processor list. */
	struct list_head list;

	/** back pointer to vs_dc */
	struct list_head head;

	/** vs crtc instance. */
	struct vs_crtc vs_crtc;

	struct dc_crtc_info *info;
};

static inline struct vs_crtc *proc_to_vs_crtc(struct dc_proc *proc)
{
	struct dc_crtc *dc_crtc = (struct dc_crtc *)proc->parent;

	return &dc_crtc->vs_crtc;
}

struct dc_crtc_state {
	/** dc crtc processor state list. */
	struct list_head list;

	/** drm plane state. */
	struct drm_crtc_state base;

	u32 output_fmt;
	u8 encoder_type;
};

/**
 * dc writeback connector structure
 */
struct dc_wb {
	/** dc writeback processor list. */
	struct list_head list;

	/** back pointer to vs_dc */
	struct list_head head;

	/** vs writeback connector instance. */
	struct vs_wb vs_wb;

	struct dc_wb_info *info;
};

static inline struct vs_wb *proc_to_vs_wb(struct dc_proc *proc)
{
	struct dc_wb *dc_wb = (struct dc_wb *)proc->parent;

	return &dc_wb->vs_wb;
}

struct dc_wb_state {
	/** dc writeback processor state list. */
	struct list_head list;

	/** drm connector state. */
	struct drm_connector_state base;
};

static inline struct dc_wb_state *to_dc_wb_state(const struct drm_connector_state *state)
{
	return container_of(state, struct dc_wb_state, base);
}

u8 get_modifier(const struct mod_format_mapping *map, u64 *modifier, u8 size);
bool is_supported_format(const struct mod_format_mapping *mod_format_map, u32 format, u64 modifier);

static inline bool is_rgb(u32 format)
{
	switch (format) {
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_BGRA1010102:
		return true;
	default:
		return false;
	}
}

static inline bool is_yuv_422(u32 format)
{
	switch (format) {
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_P210:
		return true;
	default:
		return false;
	}
}

static inline bool is_yuv_444(u32 format)
{
	switch (format) {
	case DRM_FORMAT_VUY888:
	case DRM_FORMAT_VUY101010:
		return true;
	default:
		return false;
	}
}

/**
 * @brief get state of given dc_plane
 *
 * @param[in] state DRM plane state
 * @param[in] dc_plane dc plane handler
 *
 * @return state on success, NULL on failure
 */
struct dc_proc_state *get_plane_proc_state(struct drm_plane_state *state, struct dc_proc *dc_plane);

/**
 * @brief get state of given dc_crtc
 *
 * @param[in] state DRM crtc state
 * @param[in] dc_crtc dc crtc handler
 *
 * @return state on success, NULL on failure
 */
struct dc_proc_state *get_crtc_proc_state(struct drm_crtc_state *state, struct dc_proc *dc_crtc);

/**
 * @brief get dc_crtc state with dc_plane and dc_crtc name
 *
 * @param[in] dc_plane dc_plane pointer
 * @param[in] id dc crtc name
 *
 * @return state on success, NULL on failure
 */
struct dc_proc_state *get_crtc_state_with_plane(struct dc_proc *dc_plane, const char *id);

/**
 * @brief get state of given dc_wb
 *
 * @param[in] state DRM writeback connector state
 * @param[in] dc_wb dc writeback handler
 *
 * @return state on success, NULL on failure
 */
struct dc_proc_state *get_wb_proc_state(struct drm_connector_state *state, struct dc_proc *dc_wb);

/**
 * @brief find dc_plane which attached to given vs_plane with id
 *
 * @param[in] vs_plane vs plane handler
 * @param[in] id string of this dc_plane
 *
 * @return dc_plane handler on success, NULL on failure
 */
struct dc_plane *find_dc_plane(struct vs_plane *vs_plane, const char *id);

/**
 * @brief find crtc dc_proc
 *
 * @param[in] dc_crtc dc crtc handler
 * @param[in] id string of this dc_proc
 *
 * @return dc_crtc handler on success, NULL on failure
 */
struct dc_proc *find_dc_proc_crtc(struct dc_crtc *dc_crtc, const char *id);

/**
 * @brief update blob property, copy from drm_atomic_replace_property_blob_from_id
 *
 * @param[in] dev drm device
 * @param[in] blob to be updated blob property
 * @param[in] blob_id  blob property id
 * @param[in] expected_size  expected blob size, -1 for not care
 * @param[in] expected_elem_size  expected element size, -1 for not care
 * @param[in] replaced  output info to show whether blob is updated.
 */
int dc_replace_property_blob_from_id(struct drm_device *dev, struct drm_property_blob **blob,
				     u64 blob_id, ssize_t expected_size, ssize_t expected_elem_size,
				     bool *replaced);

static inline struct dc_plane *to_dc_plane(struct vs_plane *vs_plane)
{
	return container_of(vs_plane, struct dc_plane, vs_plane);
}

static inline struct dc_plane_state *to_dc_plane_state(const struct drm_plane_state *state)
{
	return container_of(state, struct dc_plane_state, base);
}

static inline struct dc_crtc *to_dc_crtc(struct vs_crtc *vs_crtc)
{
	return container_of(vs_crtc, struct dc_crtc, vs_crtc);
}

static inline struct dc_wb *to_dc_wb(struct vs_wb *vs_wb)
{
	return container_of(vs_wb, struct dc_wb, vs_wb);
}

static inline struct dc_crtc_state *to_dc_crtc_state(const struct drm_crtc_state *state)
{
	return container_of(state, struct dc_crtc_state, base);
}

int dc_plane_create_prop(struct dc_plane *dc_plane);
int dc_plane_init(struct dc_plane *dc_plane, struct dc_plane_info *dc_plane_info,
		  struct list_head *device_list);
void dc_plane_destroy(struct vs_plane *vs_plane);
void dc_plane_vblank(struct vs_plane *vs_plane);
void dc_plane_suspend(struct vs_plane *vs_plane);
void dc_plane_resume(struct vs_plane *vs_plane);
int dc_crtc_create_prop(struct dc_crtc *dc_crtc);
int dc_crtc_init(struct dc_crtc *dc_crtc, struct dc_crtc_info *dc_crtc_info,
		 struct list_head *device_list);
void dc_crtc_destroy(struct vs_crtc *vs_crtc);
void dc_crtc_vblank(struct vs_crtc *vs_crtc);
void dc_crtc_suspend(struct vs_crtc *vs_crtc);
void dc_crtc_resume(struct vs_crtc *vs_crtc);
int dc_wb_create_prop(struct dc_wb *dc_wb);
int dc_wb_init(struct dc_wb *dc_wb, struct dc_wb_info *dc_wb_info, struct list_head *device_list);
void dc_wb_vblank(struct vs_wb *vs_wb);
void dc_wb_destroy(struct vs_wb *vs_wb);
int dc_wb_post_create(struct dc_wb *dc_wb);
/** @} */

#endif /* __DC_PROC_H__ */
