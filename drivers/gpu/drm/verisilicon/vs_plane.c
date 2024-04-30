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

#include <drm/drm_atomic.h>
#include <drm/drm_blend.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_gem_dma_helper.h>

#include "vs_crtc.h"
#include "vs_plane.h"
#include "vs_gem.h"
#include "vs_fb.h"

static void vs_plane_destroy(struct drm_plane *plane)
{
	struct vs_plane *vs_plane = to_vs_plane(plane);

	drm_plane_cleanup(plane);

	if (vs_plane->funcs->destroy)
		vs_plane->funcs->destroy(vs_plane);

	kfree(vs_plane);
}

static void vs_plane_atomic_destroy_state(struct drm_plane *plane, struct drm_plane_state *state)
{
	struct vs_plane *vs_plane = to_vs_plane(plane);

	__drm_atomic_helper_plane_destroy_state(state);

	if (vs_plane->funcs->destroy_state)
		vs_plane->funcs->destroy_state(state);
	else
		kfree(state);
}

static void vs_plane_reset(struct drm_plane *plane)
{
	struct vs_plane *vs_plane = to_vs_plane(plane);
	struct drm_plane_state *state;
	const struct vs_plane_info *info = vs_plane->info;

	if (plane->state) {
		vs_plane_atomic_destroy_state(plane, plane->state);
		plane->state = NULL;
	}

	if (vs_plane->funcs->create_state)
		state = vs_plane->funcs->create_state(vs_plane);
	else
		state = kzalloc(sizeof(*state), GFP_KERNEL);

	if (WARN_ON(!state))
		return;

	__drm_atomic_helper_plane_reset(plane, state);

	state->color_encoding = DRM_COLOR_YCBCR_BT709;
	state->zpos	      = info->zpos.val;
}

static struct drm_plane_state *vs_plane_atomic_duplicate_state(struct drm_plane *plane)
{
	struct vs_plane *vs_plane = to_vs_plane(plane);
	struct drm_plane_state *state;

	if (WARN_ON(!plane->state))
		return NULL;

	if (vs_plane->funcs->duplicate_state)
		state = vs_plane->funcs->duplicate_state(vs_plane);
	else
		state = kzalloc(sizeof(*state), GFP_KERNEL);

	if (!state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, state);

	return state;
}

static int vs_plane_atomic_set_property(struct drm_plane *plane, struct drm_plane_state *state,
					struct drm_property *property, uint64_t val)
{
	struct vs_plane *vs_plane = to_vs_plane(plane);

	if (vs_plane->funcs->set_prop)
		return vs_plane->funcs->set_prop(vs_plane, state, property, val);

	return -EINVAL;
}

static int vs_plane_atomic_get_property(struct drm_plane *plane,
					const struct drm_plane_state *state,
					struct drm_property *property, uint64_t *val)
{
	struct vs_plane *vs_plane = to_vs_plane(plane);

	if (vs_plane->funcs->get_prop)
		return vs_plane->funcs->get_prop(vs_plane, state, property, val);

	return -EINVAL;
}

static void vs_plane_print_state(struct drm_printer *p, const struct drm_plane_state *state)
{
	struct vs_plane *vs_plane = to_vs_plane(state->plane);

	if (vs_plane->funcs->print_state)
		vs_plane->funcs->print_state(p, state);
}

static bool vs_format_mod_supported(struct drm_plane *plane, uint32_t format, uint64_t modifier)
{
	struct vs_plane *vs_plane = to_vs_plane(plane);
	int ret			  = false;

	if (vs_plane->funcs->check_format_mod)
		ret = vs_plane->funcs->check_format_mod(vs_plane, format, modifier);

	return ret;
}

static const struct drm_plane_funcs vs_plane_funcs = {
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.destroy		= vs_plane_destroy,
	.reset			= vs_plane_reset,
	.atomic_duplicate_state = vs_plane_atomic_duplicate_state,
	.atomic_destroy_state	= vs_plane_atomic_destroy_state,
	.atomic_set_property	= vs_plane_atomic_set_property,
	.atomic_get_property	= vs_plane_atomic_get_property,
	.atomic_print_state	= vs_plane_print_state,
	.format_mod_supported	= vs_format_mod_supported,
};

static int vs_plane_atomic_check(struct drm_plane *plane, struct drm_atomic_state *state)
{
	struct vs_plane *vs_plane		= to_vs_plane(plane);
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state, plane);
	const struct drm_framebuffer *fb	= new_plane_state->fb;
	const struct vs_plane_info *info	= vs_plane->info;
	struct drm_crtc *crtc			= new_plane_state->crtc;
	struct drm_crtc_state *crtc_state;
	int ret = 0;

	if (vs_plane->funcs->check)
		ret = vs_plane->funcs->check(vs_plane, new_plane_state);
	if (ret)
		return ret;

	if (!crtc || !fb)
		return 0;

	if (fb->width < info->min_width || fb->width > info->max_width ||
	    fb->height < info->min_height || fb->height > info->max_height)
		return -EINVAL;

	crtc_state = drm_atomic_get_existing_crtc_state(state, crtc);
	if (IS_ERR(crtc_state))
		return -EINVAL;

	return drm_atomic_helper_check_plane_state(new_plane_state, crtc_state, info->min_scale,
						   info->max_scale, true, true);
}

static void vs_plane_atomic_update(struct drm_plane *plane, struct drm_atomic_state *old_state)
{
	struct drm_plane_state *old_plane_state = drm_atomic_get_old_plane_state(old_state, plane);
	struct vs_plane *vs_plane		= to_vs_plane(plane);

	if (vs_plane->funcs->update)
		vs_plane->funcs->update(vs_plane, old_plane_state);
}

static void vs_plane_atomic_disable(struct drm_plane *plane, struct drm_atomic_state *old_state)
{
	struct drm_plane_state *old_plane_state = drm_atomic_get_old_plane_state(old_state, plane);
	struct vs_plane *vs_plane		= to_vs_plane(plane);

	if (vs_plane->funcs->disable)
		vs_plane->funcs->disable(vs_plane, old_plane_state);
}

static const struct drm_plane_helper_funcs vs_plane_helper_funcs = {
	.atomic_check	= vs_plane_atomic_check,
	.atomic_update	= vs_plane_atomic_update,
	.atomic_disable = vs_plane_atomic_disable,
};

int vs_plane_init(struct drm_device *drm_dev, struct vs_plane *vs_plane)
{
	const struct vs_plane_info *info = vs_plane->info;
	u32 possible_crtcs =
		find_possible_crtcs_with_name(drm_dev, info->crtc_names, info->num_crtcs);
	int ret;

	ret = drm_universal_plane_init(drm_dev, &vs_plane->base, possible_crtcs, &vs_plane_funcs,
				       info->formats, info->num_formats, info->modifiers,
				       info->type, info->name);
	if (ret)
		return ret;

	drm_plane_helper_add(&vs_plane->base, &vs_plane_helper_funcs);

	/* Set up the plane properties */
	if (info->rotation) {
		ret = drm_plane_create_rotation_property(&vs_plane->base, DRM_MODE_ROTATE_0,
							 info->rotation);
		if (ret)
			goto error_cleanup_plane;
	}

	if (info->blend_mode) {
		ret = drm_plane_create_blend_mode_property(&vs_plane->base, info->blend_mode);
		if (ret)
			goto error_cleanup_plane;
		ret = drm_plane_create_alpha_property(&vs_plane->base);
		if (ret)
			goto error_cleanup_plane;
	}

	if (info->color_encoding) {
		ret = drm_plane_create_color_properties(
			&vs_plane->base, info->color_encoding,
			BIT(DRM_COLOR_YCBCR_LIMITED_RANGE) | BIT(DRM_COLOR_YCBCR_FULL_RANGE),
			DRM_COLOR_YCBCR_BT709, DRM_COLOR_YCBCR_LIMITED_RANGE);
		if (ret)
			goto error_cleanup_plane;
	}

	if (info->zpos.min < info->zpos.max) {
		ret = drm_plane_create_zpos_property(&vs_plane->base, info->zpos.val,
						     info->zpos.min, info->zpos.max);
		if (ret)
			goto error_cleanup_plane;
	} else {
		ret = drm_plane_create_zpos_immutable_property(&vs_plane->base, info->zpos.min);
		if (ret)
			goto error_cleanup_plane;
	}

	return ret;

error_cleanup_plane:
	drm_plane_cleanup(&vs_plane->base);
	return ret;
}
