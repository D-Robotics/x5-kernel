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

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_vblank.h>
#include <drm/drm_writeback.h>

#include "vs_crtc.h"
#include "vs_plane.h"

static const char *const pipe_crc_sources[] = {
	[VS_CRC_SOURCE_NONE] = "none",
	[VS_CRC_SOURCE_AUTO] = "auto",
};

static inline bool crtc_match_name(struct vs_crtc *vs_crtc, const char *const names[], u8 name_cnt)
{
	u8 i;

	for (i = 0; i < name_cnt; i++)
		if (!strcmp(vs_crtc->info->name, names[i]))
			return true;

	return false;
}

u32 find_possible_crtcs_with_name(struct drm_device *drm_dev, const char *const names[],
				  u8 name_cnt)
{
	struct drm_crtc *drm_crtc;
	struct vs_crtc *vs_crtc;
	u32 crtc_masks = 0;

	drm_for_each_crtc (drm_crtc, drm_dev) {
		vs_crtc = to_vs_crtc(drm_crtc);

		if (crtc_match_name(vs_crtc, names, name_cnt))
			crtc_masks |= drm_crtc_mask(drm_crtc);
	}

	return crtc_masks;
}

void set_crtc_primary_plane(struct drm_device *drm_dev, struct drm_plane *drm_plane)
{
	struct drm_crtc *drm_crtc;

	drm_for_each_crtc (drm_crtc, drm_dev) {
		if (drm_crtc->primary)
			continue;

		if (!(drm_plane->possible_crtcs & drm_crtc_mask(drm_crtc)))
			continue;

		drm_crtc->primary = drm_plane;
	}
}

void set_crtc_cursor_plane(struct drm_device *drm_dev, struct drm_plane *drm_plane)
{
	struct drm_crtc *drm_crtc;

	drm_for_each_crtc (drm_crtc, drm_dev) {
		if (drm_crtc->cursor)
			continue;

		if (!(drm_plane->possible_crtcs & drm_crtc_mask(drm_crtc)))
			continue;

		drm_crtc->cursor = drm_plane;
	}
}

static void vs_crtc_destroy(struct drm_crtc *crtc)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);

	drm_crtc_cleanup(crtc);

	if (vs_crtc->funcs->destroy)
		vs_crtc->funcs->destroy(vs_crtc);
}

static void vs_crtc_atomic_destroy_state(struct drm_crtc *crtc, struct drm_crtc_state *state)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);

	__drm_atomic_helper_crtc_destroy_state(state);

	if (vs_crtc->funcs->destroy_state)
		vs_crtc->funcs->destroy_state(state);
	else
		kfree(state);
}

static void vs_crtc_reset(struct drm_crtc *crtc)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	struct drm_crtc_state *state;

	if (crtc->state) {
		vs_crtc_atomic_destroy_state(crtc, crtc->state);
		crtc->state = NULL;
	}

	if (vs_crtc->funcs->create_state)
		state = vs_crtc->funcs->create_state(vs_crtc);
	else
		state = kzalloc(sizeof(*state), GFP_KERNEL);

	if (state)
		__drm_atomic_helper_crtc_reset(crtc, state);
}

static struct drm_crtc_state *vs_crtc_atomic_duplicate_state(struct drm_crtc *crtc)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	struct drm_crtc_state *state;

	if (WARN_ON(!crtc->state))
		return NULL;

	if (vs_crtc->funcs->duplicate_state)
		state = vs_crtc->funcs->duplicate_state(vs_crtc);
	else
		state = kzalloc(sizeof(*state), GFP_KERNEL);

	if (!state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, state);

	return state;
}

static int vs_crtc_atomic_set_property(struct drm_crtc *crtc, struct drm_crtc_state *state,
				       struct drm_property *property, uint64_t val)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);

	if (vs_crtc->funcs->set_prop)
		return vs_crtc->funcs->set_prop(vs_crtc, state, property, val);

	return -EINVAL;
}

static int vs_crtc_atomic_get_property(struct drm_crtc *crtc, const struct drm_crtc_state *state,
				       struct drm_property *property, uint64_t *val)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);

	if (vs_crtc->funcs->get_prop)
		return vs_crtc->funcs->get_prop(vs_crtc, state, property, val);

	return -EINVAL;
}

static int vs_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);

	if (vs_crtc->funcs->enable_vblank)
		vs_crtc->funcs->enable_vblank(vs_crtc, true);

	return 0;
}

static void vs_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);

	if (vs_crtc->funcs->enable_vblank)
		vs_crtc->funcs->enable_vblank(vs_crtc, false);
}

static int parse_crc_source(const char *source, int *s)
{
	int i;

	if (!source) {
		*s = VS_CRC_SOURCE_NONE;
		return 0;
	}

	i = match_string(pipe_crc_sources, ARRAY_SIZE(pipe_crc_sources), source);
	if (i < 0)
		return i;

	*s = i;

	return 0;
}

static int vs_crtc_set_crc_source(struct drm_crtc *crtc, const char *source)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	bool enable;

	int s;

	if (parse_crc_source(source, &s) < 0)
		return -EINVAL;

	enable = (s != VS_CRC_SOURCE_NONE);

	if (vs_crtc->funcs->enable_crc)
		vs_crtc->funcs->enable_crc(vs_crtc, enable);

	return 0;
}

static const char *const *vs_crtc_get_crc_sources(struct drm_crtc *crtc, size_t *count)
{
	*count = ARRAY_SIZE(pipe_crc_sources);

	return pipe_crc_sources;
}

static int vs_crtc_verify_crc_source(struct drm_crtc *crtc, const char *source, size_t *values_cnt)
{
	int s;

	if (parse_crc_source(source, &s) < 0)
		return -EINVAL;

	*values_cnt = 1;
	return 0;
}

static void vs_crtc_print_state(struct drm_printer *p, const struct drm_crtc_state *state)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(state->crtc);

	if (vs_crtc->funcs->print_state)
		vs_crtc->funcs->print_state(p, state);
}

static const struct drm_crtc_funcs vs_crtc_funcs = {
	.set_config		= drm_atomic_helper_set_config,
	.destroy		= vs_crtc_destroy,
	.page_flip		= drm_atomic_helper_page_flip,
	.reset			= vs_crtc_reset,
	.atomic_duplicate_state = vs_crtc_atomic_duplicate_state,
	.atomic_destroy_state	= vs_crtc_atomic_destroy_state,
	.atomic_set_property	= vs_crtc_atomic_set_property,
	.atomic_get_property	= vs_crtc_atomic_get_property,
	.enable_vblank		= vs_crtc_enable_vblank,
	.disable_vblank		= vs_crtc_disable_vblank,
	.set_crc_source		= vs_crtc_set_crc_source,
	.verify_crc_source	= vs_crtc_verify_crc_source,
	.get_crc_sources	= vs_crtc_get_crc_sources,
	.atomic_print_state	= vs_crtc_print_state,
};

static bool vs_crtc_mode_fixup(struct drm_crtc *crtc, const struct drm_display_mode *mode,
			       struct drm_display_mode *adjusted_mode)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	bool ret		= true;

	if (vs_crtc->funcs->mode_fixup)
		ret = vs_crtc->funcs->mode_fixup(vs_crtc, mode, adjusted_mode);

	return ret;
}

static void vs_crtc_atomic_enable(struct drm_crtc *crtc, struct drm_atomic_state *old_state)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);

	if (vs_crtc->funcs->enable)
		vs_crtc->funcs->enable(vs_crtc);

	drm_crtc_vblank_on(crtc);
}

static void vs_crtc_atomic_disable(struct drm_crtc *crtc, struct drm_atomic_state *old_state)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);

	drm_crtc_vblank_off(crtc);

	if (vs_crtc->funcs->disable)
		vs_crtc->funcs->disable(vs_crtc);
}

static void vs_crtc_atomic_begin(struct drm_crtc *crtc, struct drm_atomic_state *state)
{
	struct drm_crtc_state *old_crtc_state = drm_atomic_get_old_crtc_state(state, crtc);
	struct vs_crtc *vs_crtc		      = to_vs_crtc(crtc);

	if (vs_crtc->funcs->update)
		vs_crtc->funcs->update(vs_crtc, old_crtc_state);
}

static void vs_crtc_atomic_flush(struct drm_crtc *crtc, struct drm_atomic_state *state)
{
	struct drm_crtc_state *old_crtc_state  = drm_atomic_get_old_crtc_state(state, crtc);
	struct vs_crtc *vs_crtc		       = to_vs_crtc(crtc);
	struct drm_pending_vblank_event *event = crtc->state->event;

	if (vs_crtc->funcs->commit)
		vs_crtc->funcs->commit(vs_crtc, old_crtc_state);

	if (event) {
		unsigned long flags;

		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		if (drm_crtc_vblank_get(crtc) == 0)
			drm_crtc_arm_vblank_event(crtc, event);
		else
			drm_crtc_send_vblank_event(crtc, event);

		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

		crtc->state->event = NULL;
	}
}

static int vs_crtc_atomic_check(struct drm_crtc *crtc, struct drm_atomic_state *state)
{
	struct drm_crtc_state *new_crtc_state = drm_atomic_get_new_crtc_state(state, crtc);
	struct vs_crtc *vs_crtc		      = to_vs_crtc(crtc);

	if (vs_crtc->funcs->check)
		return vs_crtc->funcs->check(vs_crtc, new_crtc_state);

	return 0;
}

static const struct drm_crtc_helper_funcs vs_crtc_helper_funcs = {
	.mode_fixup	= vs_crtc_mode_fixup,
	.atomic_enable	= vs_crtc_atomic_enable,
	.atomic_disable = vs_crtc_atomic_disable,
	.atomic_begin	= vs_crtc_atomic_begin,
	.atomic_flush	= vs_crtc_atomic_flush,
	.atomic_check	= vs_crtc_atomic_check,
};

int vs_crtc_init(struct drm_device *drm_dev, struct vs_crtc *vs_crtc)
{
	const struct vs_crtc_info *info = vs_crtc->info;
	int ret;

	ret = drm_crtc_init_with_planes(drm_dev, &vs_crtc->base, NULL, NULL, &vs_crtc_funcs,
					info->name);
	if (ret)
		return ret;

	drm_crtc_helper_add(&vs_crtc->base, &vs_crtc_helper_funcs);

	if (info->gamma_size) {
		ret = drm_mode_crtc_set_gamma_size(&vs_crtc->base, info->gamma_size);
		if (ret) {
			drm_crtc_cleanup(&vs_crtc->base);
			return ret;
		}

		drm_crtc_enable_color_mgmt(&vs_crtc->base, 0, false, info->gamma_size);
	}

	return 0;
}
