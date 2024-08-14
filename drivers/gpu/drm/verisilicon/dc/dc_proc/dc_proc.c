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

#include <linux/media-bus-format.h>

#include "dc_proc.h"
#include "vs_plane.h"
#include "dc_info.h"

u64 format_modifiers[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID,
};

// generic dc_proc APIs

static struct device *__find_device(struct list_head *device_list, const char *name)
{
	struct dc_device *dc_dev;

	list_for_each_entry (dc_dev, device_list, head)
		if (strcmp(dc_dev->name, name) == 0)
			return dc_dev->dev;

	return ERR_PTR(-ENODEV);
}

static void __dc_proc_destroy(struct list_head *dc_proc_list)
{
	struct dc_proc *dc_proc, *tmp;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry_safe_reverse (dc_proc, tmp, dc_proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;

		list_del(&dc_proc->head);

		if (funcs && funcs->destroy)
			funcs->destroy(dc_proc);
	}
}

static int __dc_proc_init(void *parent, struct list_head *dc_proc_list,
			  const struct dc_proc_info *dc_proc_info_list, u8 dc_proc_num,
			  struct list_head *device_list, void *info)
{
	struct device *dev;
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *dc_proc_info;
	int i, ret;

	INIT_LIST_HEAD(dc_proc_list);

	for (i = 0; i < dc_proc_num; i++) {
		dc_proc_info = &dc_proc_info_list[i];
		funcs	     = dc_proc_info->funcs;
		if (!funcs->create) {
			ret = -EINVAL;
			goto err_clean_up;
		}

		dev = __find_device(device_list, dc_proc_info->name);
		if (IS_ERR(dev)) {
			ret = -ENODEV;
			goto err_clean_up;
		}

		dc_proc = funcs->create(dev, dc_proc_info);
		if (IS_ERR(dc_proc)) {
			ret = -ENODEV;
			goto err_clean_up;
		}

		dc_proc->parent = parent;
		dc_proc->info	= dc_proc_info;

		if (funcs->update_info)
			funcs->update_info(dc_proc, info);
		list_add_tail(&dc_proc->head, dc_proc_list);
	}

	return 0;

err_clean_up:
	__dc_proc_destroy(dc_proc_list);

	return ret;
}

static int __dc_proc_check(struct list_head *dc_proc_list, void *state)
{
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;
	int ret;

	list_for_each_entry (dc_proc, dc_proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs->check) {
			ret = funcs->check(dc_proc, state);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static void __dc_proc_update(struct list_head *dc_proc_list, void *old_state)
{
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry_reverse (dc_proc, dc_proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs && funcs->update)
			funcs->update(dc_proc, old_state);
	}
}

static void __dc_proc_disable(struct list_head *dc_proc_list, void *old_state)
{
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry_reverse (dc_proc, dc_proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs && funcs->disable)
			funcs->disable(dc_proc, old_state);
	}
}

static int __dc_plane_proc_check_format_mod(struct list_head *dc_proc_list, u32 format,
					    u64 modifier)
{
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;
	int ret = 0;

	list_for_each_entry_reverse (dc_proc, dc_proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs && funcs->check_format_mod)
			ret = funcs->check_format_mod(dc_proc, format, modifier);

		if (ret)
			return ret;
	}

	return ret;
}

static void __dc_proc_commit(struct list_head *dc_proc_list)
{
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry (dc_proc, dc_proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs->commit)
			funcs->commit(dc_proc);
	}
}

static void __dc_proc_destroy_state(struct list_head *proc_state_list)
{
	struct dc_proc_state *dc_proc_state, *tmp;
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry_safe_reverse (dc_proc_state, tmp, proc_state_list, head) {
		dc_proc = dc_proc_state->proc;
		info	= dc_proc->info;
		funcs	= info->funcs;

		list_del(&dc_proc_state->head);

		if (funcs && funcs->destroy_state)
			funcs->destroy_state(dc_proc_state);
	}
}

static int __dc_proc_create_state(void *parent, struct list_head *proc_list,
				  struct list_head *proc_state_list)
{
	struct dc_proc *dc_proc;
	struct dc_proc_state *dc_proc_state;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry (dc_proc, proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs && funcs->create_state) {
			dc_proc_state = funcs->create_state(parent, dc_proc);
			if (!dc_proc_state)
				goto err_clean_up;

			dc_proc_state->proc = dc_proc;

			list_add_tail(&dc_proc_state->head, proc_state_list);
		}
	}

	return 0;

err_clean_up:
	__dc_proc_destroy_state(proc_state_list);

	return -EINVAL;
}

static int __dc_proc_duplicate_state(void *parent, struct list_head *proc_old_state_list,
				     struct list_head *proc_new_state_list)
{
	struct dc_proc_state *current_state, *new_state;
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry (current_state, proc_old_state_list, head) {
		dc_proc = current_state->proc;
		info	= dc_proc->info;
		funcs	= info->funcs;
		if (funcs && funcs->duplicate_state) {
			new_state = funcs->duplicate_state(parent, dc_proc, current_state);
			if (!new_state)
				goto err_clean_up;

			list_add_tail(&new_state->head, proc_new_state_list);
		}
	}
	return 0;

err_clean_up:
	__dc_proc_destroy_state(proc_new_state_list);

	return -EINVAL;
}

static void __dc_proc_print_state(struct drm_printer *p, struct list_head *proc_state_list)
{
	struct dc_proc *dc_proc;
	struct dc_proc_state *dc_proc_state;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry (dc_proc_state, proc_state_list, head) {
		dc_proc = dc_proc_state->proc;
		info	= dc_proc->info;
		funcs	= info->funcs;
		if (funcs && funcs->print_state)
			funcs->print_state(p, dc_proc_state);
	}
}

static int __dc_proc_create_prop(struct list_head *proc_list)
{
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;
	int ret;

	list_for_each_entry (dc_proc, proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs->create_prop) {
			ret = funcs->create_prop(dc_proc);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int __dc_proc_post_create(struct list_head *proc_list)
{
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;
	int ret;

	list_for_each_entry (dc_proc, proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs->post_create) {
			ret = funcs->post_create(dc_proc);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int __dc_proc_set_prop(struct list_head *proc_state_list, struct drm_property *property,
			      u64 val)
{
	struct dc_proc *dc_proc;
	struct dc_proc_state *dc_proc_state;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;
	int ret;

	list_for_each_entry (dc_proc_state, proc_state_list, head) {
		dc_proc = dc_proc_state->proc;
		info	= dc_proc->info;
		funcs	= info->funcs;
		if (funcs && funcs->set_prop) {
			ret = funcs->set_prop(dc_proc, dc_proc_state, property, val);
			if (ret == -EAGAIN)
				continue;

			return ret;
		}
	}

	return -EINVAL;
}

static int __dc_proc_get_prop(struct list_head *proc_state_list, struct drm_property *property,
			      u64 *val)
{
	struct dc_proc *dc_proc;
	struct dc_proc_state *dc_proc_state;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;
	int ret;

	list_for_each_entry (dc_proc_state, proc_state_list, head) {
		dc_proc = dc_proc_state->proc;
		info	= dc_proc->info;
		funcs	= info->funcs;
		if (funcs && funcs->get_prop) {
			ret = funcs->get_prop(dc_proc, dc_proc_state, property, val);
			if (ret == -EAGAIN)
				continue;

			return ret;
		}
	}

	return -EINVAL;
}

static void __dc_proc_vblank(struct list_head *dc_proc_list, u32 irq_status)
{
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry (dc_proc, dc_proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;

		if (funcs->vblank_status)
			funcs->vblank_status(dc_proc, irq_status);
		else if (funcs->vblank)
			funcs->vblank(dc_proc);
	}
}

static void __dc_proc_suspend(struct list_head *dc_proc_list)
{
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry (dc_proc, dc_proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs->suspend)
			funcs->suspend(dc_proc);
	}
}

static void __dc_proc_resume(struct list_head *dc_proc_list)
{
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry (dc_proc, dc_proc_list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs->resume)
			funcs->resume(dc_proc);
	}
}

// dc_plane_proc APIs

void dc_plane_destroy(struct vs_plane *vs_plane)
{
	struct dc_plane *dc_plane = to_dc_plane(vs_plane);

	__dc_proc_destroy(&dc_plane->list);
	if (vs_plane->info) {
		if (vs_plane->info->modifiers && vs_plane->info->num_modifiers > NUM_MODIFIERS) {
			kfree(vs_plane->info->modifiers);
			vs_plane->info->num_modifiers = NUM_MODIFIERS;
			vs_plane->info->modifiers     = format_modifiers;
		}
	}

	kfree(dc_plane);
}

static int dc_plane_check(struct vs_plane *vs_plane, struct drm_plane_state *state)
{
	struct dc_plane *dc_plane = to_dc_plane(vs_plane);

	return __dc_proc_check(&dc_plane->list, state);
}

static void dc_plane_update(struct vs_plane *vs_plane, struct drm_plane_state *old_state)
{
	struct dc_plane *dc_plane = to_dc_plane(vs_plane);

	__dc_proc_update(&dc_plane->list, old_state);
}

static void dc_plane_disable(struct vs_plane *vs_plane, struct drm_plane_state *old_state)
{
	struct dc_plane *dc_plane = to_dc_plane(vs_plane);

	return __dc_proc_disable(&dc_plane->list, old_state);
}

static int dc_plane_check_format_mod(struct vs_plane *vs_plane, u32 format, u64 modifier)
{
	struct dc_plane *dc_plane = to_dc_plane(vs_plane);

	return __dc_plane_proc_check_format_mod(&dc_plane->list, format, modifier);
}

static struct drm_plane_state *dc_plane_create_state(struct vs_plane *vs_plane)
{
	struct dc_plane *dc_plane = to_dc_plane(vs_plane);
	struct dc_plane_state *dc_plane_state;
	int ret;

	dc_plane_state = kzalloc(sizeof(*dc_plane_state), GFP_KERNEL);
	if (!dc_plane_state)
		return NULL;

	INIT_LIST_HEAD(&dc_plane_state->list);

	ret = __dc_proc_create_state(dc_plane_state, &dc_plane->list, &dc_plane_state->list);
	if (ret)
		return NULL;

	return &dc_plane_state->base;
}

static void dc_plane_destroy_state(struct drm_plane_state *state)
{
	struct dc_plane_state *dc_plane_state = to_dc_plane_state(state);

	__dc_proc_destroy_state(&dc_plane_state->list);
	kfree(dc_plane_state);
}

static struct drm_plane_state *dc_plane_duplicate_state(struct vs_plane *vs_plane)
{
	const struct drm_plane *drm_plane	      = &vs_plane->base;
	const struct drm_plane_state *drm_plane_state = drm_plane->state;
	struct dc_plane_state *current_state, *new_state;
	int ret;

	if (WARN_ON(!drm_plane_state))
		return NULL;

	new_state = kzalloc(sizeof(*new_state), GFP_KERNEL);
	if (!new_state)
		return NULL;

	current_state = to_dc_plane_state(drm_plane_state);

	memcpy(new_state, current_state, sizeof(*current_state));

	INIT_LIST_HEAD(&new_state->list);

	ret = __dc_proc_duplicate_state(new_state, &current_state->list, &new_state->list);
	if (ret)
		return NULL;

	return &new_state->base;
}

static void dc_plane_print_state(struct drm_printer *p, const struct drm_plane_state *state)
{
	struct dc_plane_state *dc_plane_state = to_dc_plane_state(state);

	__dc_proc_print_state(p, &dc_plane_state->list);
}

static int dc_plane_set_prop(struct vs_plane *vs_plane, struct drm_plane_state *state,
			     struct drm_property *property, u64 val)
{
	struct dc_plane_state *dc_plane_state = to_dc_plane_state(state);

	return __dc_proc_set_prop(&dc_plane_state->list, property, val);
}

static int dc_plane_get_prop(struct vs_plane *vs_plane, const struct drm_plane_state *state,
			     struct drm_property *property, u64 *val)
{
	struct dc_plane_state *dc_plane_state = to_dc_plane_state(state);

	return __dc_proc_get_prop(&dc_plane_state->list, property, val);
}

static const struct vs_plane_funcs vs_plane_funcs = {
	.destroy	  = dc_plane_destroy,
	.check		  = dc_plane_check,
	.update		  = dc_plane_update,
	.disable	  = dc_plane_disable,
	.check_format_mod = dc_plane_check_format_mod,
	.create_state	  = dc_plane_create_state,
	.destroy_state	  = dc_plane_destroy_state,
	.duplicate_state  = dc_plane_duplicate_state,
	.print_state	  = dc_plane_print_state,
	.set_prop	  = dc_plane_set_prop,
	.get_prop	  = dc_plane_get_prop,
};

int dc_plane_create_prop(struct dc_plane *dc_plane)
{
	return __dc_proc_create_prop(&dc_plane->list);
}

int dc_plane_init(struct dc_plane *dc_plane, struct dc_plane_info *dc_plane_info,
		  struct list_head *device_list)
{
	struct vs_plane *vs_plane	    = &dc_plane->vs_plane;
	struct vs_plane_info *vs_plane_info = &dc_plane_info->info;

	vs_plane->info	= vs_plane_info;
	vs_plane->funcs = &vs_plane_funcs;

	dc_plane->info = dc_plane_info;

	INIT_LIST_HEAD(&dc_plane->head);

	return __dc_proc_init(dc_plane, &dc_plane->list, dc_plane_info->proc_info,
			      dc_plane_info->num_proc, device_list, vs_plane_info);
}

static void dc_plane_commit(struct vs_plane *vs_plane)
{
	struct dc_plane *dc_plane = to_dc_plane(vs_plane);

	__dc_proc_commit(&dc_plane->list);
}

void dc_plane_vblank(struct vs_plane *vs_plane, u32 status)
{
	struct dc_plane *dc_plane = to_dc_plane(vs_plane);

	__dc_proc_vblank(&dc_plane->list, status);
}

void dc_plane_suspend(struct vs_plane *vs_plane)
{
	struct dc_plane *dc_plane = to_dc_plane(vs_plane);

	__dc_proc_suspend(&dc_plane->list);
}

void dc_plane_resume(struct vs_plane *vs_plane)
{
	struct dc_plane *dc_plane = to_dc_plane(vs_plane);

	__dc_proc_resume(&dc_plane->list);
}

// dc_proc APIs

static void set_dc_crtc_state(struct drm_crtc_state *crtc_state)
{
	const struct drm_crtc *crtc	    = crtc_state->crtc;
	struct dc_crtc_state *dc_crtc_state = to_dc_crtc_state(crtc_state);
	struct drm_encoder *encoder;
	struct drm_bridge_state *bridge_state = NULL;
	struct drm_bus_cfg *input_bus_cfg     = NULL;
	struct drm_bridge *first_bridge;

	drm_for_each_encoder_mask (encoder, crtc->dev, crtc_state->encoder_mask) {
		dc_crtc_state->encoder_type = encoder->encoder_type;
		first_bridge		    = drm_bridge_chain_get_first_bridge(encoder);
		bridge_state = drm_atomic_get_new_bridge_state(crtc_state->state, first_bridge);
	}
	input_bus_cfg = &bridge_state->input_bus_cfg;
	// cppcheck-suppress knownConditionTrueFalse
	if (!input_bus_cfg)
		return;
	dc_crtc_state->output_fmt = input_bus_cfg->format;

	switch (dc_crtc_state->output_fmt) {
	case MEDIA_BUS_FMT_RGB565_1X16:
	case MEDIA_BUS_FMT_RGB666_1X18:
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_RGB101010_1X30:
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_YUV8_1X24:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
	case MEDIA_BUS_FMT_UYVY10_1X20:
	case MEDIA_BUS_FMT_YUV10_1X30:
		break;
	default:
		dc_crtc_state->output_fmt = MEDIA_BUS_FMT_RGB888_1X24;
		break;
	}
}

void dc_crtc_destroy(struct vs_crtc *vs_crtc)
{
	struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);

	__dc_proc_destroy(&dc_crtc->list);
}

static void dc_crtc_enable(struct vs_crtc *vs_crtc)
{
	const struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry (dc_proc, &dc_crtc->list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs->enable)
			funcs->enable(dc_proc);
	}
}

static void dc_crtc_disable(struct vs_crtc *vs_crtc)
{
	const struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry (dc_proc, &dc_crtc->list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs->disable)
			funcs->disable(dc_proc, NULL);
	}
}

static bool dc_crtc_mode_fixup(struct vs_crtc *vs_crtc, const struct drm_display_mode *mode,
			       struct drm_display_mode *adjusted_mode)
{
	const struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;
	struct drm_crtc_state *new_crtc_state = container_of(mode, struct drm_crtc_state, mode);
	bool ret;

	list_for_each_entry (dc_proc, &dc_crtc->list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs->mode_fixup) {
			ret = funcs->mode_fixup(dc_proc, mode, adjusted_mode);
			if (!ret)
				return ret;
		}
	}

	set_dc_crtc_state(new_crtc_state);

	return true;
}

static int dc_crtc_check(struct vs_crtc *vs_crtc, struct drm_crtc_state *state)
{
	struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);

	return __dc_proc_check(&dc_crtc->list, state);
}

static void dc_crtc_update(struct vs_crtc *vs_crtc, struct drm_crtc_state *old_state)
{
	struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);

	__dc_proc_update(&dc_crtc->list, old_state);
}

static void dc_crtc_enable_vblank(struct vs_crtc *vs_crtc, bool enable)
{
	const struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry (dc_proc, &dc_crtc->list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs->enable_vblank)
			funcs->enable_vblank(dc_proc, enable);
	}
}

static void dc_crtc_enable_crc(struct vs_crtc *vs_crtc, bool enable)
{
	const struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);
	struct dc_proc *dc_proc;
	const struct dc_proc_funcs *funcs;
	const struct dc_proc_info *info;

	list_for_each_entry (dc_proc, &dc_crtc->list, head) {
		info  = dc_proc->info;
		funcs = info->funcs;
		if (funcs->enable_crc)
			funcs->enable_crc(dc_proc, enable);
	}
}

static void dc_crtc_commit(struct vs_crtc *vs_crtc, struct drm_crtc_state *old_crtc_state)
{
	const struct drm_crtc *drm_crtc		    = &vs_crtc->base;
	const struct drm_crtc_state *new_crtc_state = drm_crtc->state;
	struct dc_crtc *dc_crtc			    = to_dc_crtc(vs_crtc);
	struct drm_plane *drm_plane;
	struct vs_plane *vs_plane;
	unsigned int plane_mask;

	plane_mask = old_crtc_state->plane_mask;
	plane_mask |= new_crtc_state->plane_mask;
	drm_for_each_plane_mask (drm_plane, drm_crtc->dev, plane_mask) {
		vs_plane = to_vs_plane(drm_plane);
		dc_plane_commit(vs_plane);
	}

	__dc_proc_commit(&dc_crtc->list);
}

static struct drm_crtc_state *dc_crtc_create_state(struct vs_crtc *vs_crtc)
{
	struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);
	struct dc_crtc_state *dc_crtc_state;
	int ret;

	dc_crtc_state = kzalloc(sizeof(*dc_crtc_state), GFP_KERNEL);
	if (!dc_crtc_state)
		return NULL;

	INIT_LIST_HEAD(&dc_crtc_state->list);

	ret = __dc_proc_create_state(dc_crtc_state, &dc_crtc->list, &dc_crtc_state->list);
	if (ret)
		return NULL;

	dc_crtc_state->output_fmt   = MEDIA_BUS_FMT_RGB888_1X24;
	dc_crtc_state->encoder_type = DRM_MODE_ENCODER_NONE;

	return &dc_crtc_state->base;
}

static void dc_crtc_destroy_state(struct drm_crtc_state *state)
{
	struct dc_crtc_state *dc_crtc_state = to_dc_crtc_state(state);

	__dc_proc_destroy_state(&dc_crtc_state->list);
	kfree(dc_crtc_state);
}

static struct drm_crtc_state *dc_crtc_duplicate_state(struct vs_crtc *vs_crtc)
{
	const struct drm_crtc *drm_crtc		    = &vs_crtc->base;
	const struct drm_crtc_state *drm_crtc_state = drm_crtc->state;
	struct dc_crtc_state *current_state, *new_state;
	int ret;

	if (WARN_ON(!drm_crtc_state))
		return NULL;

	new_state = kzalloc(sizeof(*new_state), GFP_KERNEL);
	if (!new_state)
		return NULL;

	current_state = to_dc_crtc_state(drm_crtc_state);

	memcpy(new_state, current_state, sizeof(*current_state));

	INIT_LIST_HEAD(&new_state->list);

	ret = __dc_proc_duplicate_state(new_state, &current_state->list, &new_state->list);
	if (ret)
		return NULL;

	return &new_state->base;
}

static void dc_crtc_print_state(struct drm_printer *p, const struct drm_crtc_state *state)
{
	struct dc_crtc_state *dc_crtc_state = to_dc_crtc_state(state);

	__dc_proc_print_state(p, &dc_crtc_state->list);
}

static int dc_crtc_set_prop(struct vs_crtc *vs_crtc, struct drm_crtc_state *state,
			    struct drm_property *property, u64 val)
{
	struct dc_crtc_state *dc_crtc_state = to_dc_crtc_state(state);

	return __dc_proc_set_prop(&dc_crtc_state->list, property, val);
}

static int dc_crtc_get_prop(struct vs_crtc *vs_crtc, const struct drm_crtc_state *state,
			    struct drm_property *property, u64 *val)
{
	struct dc_crtc_state *dc_crtc_state = to_dc_crtc_state(state);

	return __dc_proc_get_prop(&dc_crtc_state->list, property, val);
}

static const struct vs_crtc_funcs vs_crtc_funcs = {
	.destroy	 = dc_crtc_destroy,
	.enable		 = dc_crtc_enable,
	.disable	 = dc_crtc_disable,
	.mode_fixup	 = dc_crtc_mode_fixup,
	.check		 = dc_crtc_check,
	.update		 = dc_crtc_update,
	.enable_vblank	 = dc_crtc_enable_vblank,
	.enable_crc	 = dc_crtc_enable_crc,
	.commit		 = dc_crtc_commit,
	.create_state	 = dc_crtc_create_state,
	.destroy_state	 = dc_crtc_destroy_state,
	.duplicate_state = dc_crtc_duplicate_state,
	.print_state	 = dc_crtc_print_state,
	.set_prop	 = dc_crtc_set_prop,
	.get_prop	 = dc_crtc_get_prop,
};

int dc_crtc_create_prop(struct dc_crtc *dc_crtc)
{
	return __dc_proc_create_prop(&dc_crtc->list);
}

int dc_crtc_init(struct dc_crtc *dc_crtc, struct dc_crtc_info *dc_crtc_info,
		 struct list_head *device_list)
{
	struct vs_crtc *vs_crtc		  = &dc_crtc->vs_crtc;
	struct vs_crtc_info *vs_crtc_info = &dc_crtc_info->info;

	vs_crtc->info  = vs_crtc_info;
	vs_crtc->funcs = &vs_crtc_funcs;

	dc_crtc->info = dc_crtc_info;

	INIT_LIST_HEAD(&dc_crtc->head);

	return __dc_proc_init(dc_crtc, &dc_crtc->list, dc_crtc_info->proc_info,
			      dc_crtc_info->num_proc, device_list, vs_crtc_info);
}

void dc_crtc_vblank(struct vs_crtc *vs_crtc, u32 irq_status)
{
	struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);

	__dc_proc_vblank(&dc_crtc->list, irq_status);
}

void dc_crtc_suspend(struct vs_crtc *vs_crtc)
{
	struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);

	__dc_proc_suspend(&dc_crtc->list);
}

void dc_crtc_resume(struct vs_crtc *vs_crtc)
{
	struct dc_crtc *dc_crtc = to_dc_crtc(vs_crtc);

	__dc_proc_resume(&dc_crtc->list);
}

// dc_wb_proc APIs

void dc_wb_destroy(struct vs_wb *vs_wb)
{
	struct dc_wb *dc_wb = to_dc_wb(vs_wb);

	__dc_proc_destroy(&dc_wb->list);
}

void dc_wb_vblank(struct vs_wb *vs_wb, u32 irq_status)
{
	struct dc_wb *dc_wb = to_dc_wb(vs_wb);

	__dc_proc_vblank(&dc_wb->list, irq_status);
}

static void dc_wb_commit(struct vs_wb *vs_wb, struct drm_connector_state *state)
{
	struct dc_wb *dc_wb = to_dc_wb(vs_wb);

	__dc_proc_commit(&dc_wb->list);
}

static struct drm_connector_state *dc_wb_create_state(struct vs_wb *vs_wb)
{
	struct dc_wb *dc_wb = to_dc_wb(vs_wb);
	struct dc_wb_state *dc_wb_state;
	int ret;

	dc_wb_state = kzalloc(sizeof(*dc_wb_state), GFP_KERNEL);
	if (!dc_wb_state)
		return NULL;

	INIT_LIST_HEAD(&dc_wb_state->list);

	ret = __dc_proc_create_state(dc_wb_state, &dc_wb->list, &dc_wb_state->list);
	if (ret)
		return NULL;

	return &dc_wb_state->base;
}

static void dc_wb_destroy_state(struct drm_connector_state *state)
{
	struct dc_wb_state *dc_wb_state = to_dc_wb_state(state);

	__dc_proc_destroy_state(&dc_wb_state->list);
	kfree(dc_wb_state);
}

static struct drm_connector_state *dc_wb_duplicate_state(struct vs_wb *vs_wb)
{
	struct drm_writeback_connector *drm_wb = &vs_wb->base;
	struct dc_wb_state *current_state      = to_dc_wb_state(drm_wb->base.state);
	struct dc_wb_state *new_state;
	int ret;

	if (WARN_ON(!current_state))
		return NULL;

	new_state = kzalloc(sizeof(*new_state), GFP_KERNEL);
	if (!new_state)
		return NULL;

	memcpy(new_state, current_state, sizeof(*current_state));

	INIT_LIST_HEAD(&new_state->list);

	ret = __dc_proc_duplicate_state(new_state, &current_state->list, &new_state->list);
	if (ret)
		return NULL;

	return &new_state->base;
}

static void dc_wb_print_state(struct drm_printer *p, const struct drm_connector_state *state)
{
	struct dc_wb_state *dc_wb_state = to_dc_wb_state(state);

	__dc_proc_print_state(p, &dc_wb_state->list);
}

static int dc_wb_set_prop(struct vs_wb *vs_wb, struct drm_connector_state *state,
			  struct drm_property *property, u64 val)
{
	struct dc_wb_state *dc_wb_state = to_dc_wb_state(state);

	return __dc_proc_set_prop(&dc_wb_state->list, property, val);
}

static int dc_wb_get_prop(struct vs_wb *vs_wb, const struct drm_connector_state *state,
			  struct drm_property *property, u64 *val)
{
	struct dc_wb_state *dc_wb_state = to_dc_wb_state(state);

	return __dc_proc_get_prop(&dc_wb_state->list, property, val);
}

static const struct vs_wb_funcs vs_wb_funcs = {
	.destroy	 = dc_wb_destroy,
	.commit		 = dc_wb_commit,
	.create_state	 = dc_wb_create_state,
	.destroy_state	 = dc_wb_destroy_state,
	.duplicate_state = dc_wb_duplicate_state,
	.set_prop	 = dc_wb_set_prop,
	.get_prop	 = dc_wb_get_prop,
	.print_state	 = dc_wb_print_state,
};

int dc_wb_create_prop(struct dc_wb *dc_wb)
{
	return __dc_proc_create_prop(&dc_wb->list);
}

int dc_wb_init(struct dc_wb *dc_wb, struct dc_wb_info *dc_wb_info, struct list_head *device_list)
{
	struct vs_wb *vs_wb	      = &dc_wb->vs_wb;
	struct vs_wb_info *vs_wb_info = &dc_wb_info->info;

	vs_wb->info  = vs_wb_info;
	vs_wb->funcs = &vs_wb_funcs;

	dc_wb->info = dc_wb_info;

	INIT_LIST_HEAD(&dc_wb->head);

	return __dc_proc_init(dc_wb, &dc_wb->list, dc_wb_info->proc_info, dc_wb_info->num_proc,
			      device_list, vs_wb_info);
}

// processor help APIs

static u8 get_modifier_num(const struct mod_format_mapping *map)
{
	u8 i;

	for (i = 0; map[i].modifier; i++)
		;

	return i;
}

int dc_wb_post_create(struct dc_wb *dc_wb)
{
	return __dc_proc_post_create(&dc_wb->list);
}

u8 get_modifier(const struct mod_format_mapping *map, u64 *modifier, u8 size)
{
	u8 i;

	if (!modifier)
		return get_modifier_num(map);

	for (i = 0; map[i].modifier; i++) {
		if (i < size) {
			*modifier = map[i].modifier;
			modifier++;
		}
	}

	return i;
}

bool is_supported_format(const struct mod_format_mapping *mod_format_map, u32 format, u64 modifier)
{
	u8 i;
	const u32 *fourcc;

	if (!mod_format_map)
		return false;

	for (i = 0; mod_format_map[i].modifier; i++) {
		if (mod_format_map[i].modifier != modifier)
			continue;

		for (fourcc = *mod_format_map[i].fourccs; *fourcc; fourcc++) {
			if (format == *fourcc)
				return true;
		}
	}

	return false;
}

struct dc_proc_state *get_plane_proc_state(struct drm_plane_state *state, struct dc_proc *dc_proc)
{
	struct dc_plane_state *dc_plane_state = to_dc_plane_state(state);
	struct dc_proc_state *dc_proc_state;

	list_for_each_entry (dc_proc_state, &dc_plane_state->list, head)
		if (dc_proc_state->proc == dc_proc)
			return dc_proc_state;

	return NULL;
}

struct dc_proc_state *get_crtc_proc_state(struct drm_crtc_state *state, struct dc_proc *dc_proc)
{
	struct dc_crtc_state *dc_crtc_state = to_dc_crtc_state(state);
	struct dc_proc_state *dc_proc_state;

	list_for_each_entry (dc_proc_state, &dc_crtc_state->list, head)
		if (dc_proc_state->proc == dc_proc)
			return dc_proc_state;

	return NULL;
}

struct dc_proc_state *get_crtc_state_with_plane(struct dc_proc *plane_proc, const char *id)
{
	struct vs_plane *vs_plane	    = proc_to_vs_plane(plane_proc);
	struct drm_plane_state *plane_state = vs_plane->base.state;
	struct vs_crtc *vs_crtc		    = to_vs_crtc(plane_state->crtc);
	struct drm_crtc_state *crtc_state   = vs_crtc->base.state;
	struct dc_crtc *dc_crtc		    = to_dc_crtc(vs_crtc);
	struct dc_proc *dc_proc		    = find_dc_proc_crtc(dc_crtc, id);

	if (!dc_proc)
		return NULL;

	return get_crtc_proc_state(crtc_state, dc_proc);
}

struct dc_proc_state *get_wb_proc_state(struct drm_connector_state *state, struct dc_proc *dc_proc)
{
	struct dc_wb_state *vs_state = to_dc_wb_state(state);
	struct dc_proc_state *dc_proc_state;

	list_for_each_entry (dc_proc_state, &vs_state->list, head)
		if (dc_proc_state->proc == dc_proc)
			return dc_proc_state;

	return NULL;
}

struct dc_proc *find_dc_proc_crtc(struct dc_crtc *dc_crtc, const char *id)
{
	struct dc_proc *proc;

	list_for_each_entry (proc, &dc_crtc->list, head)
		if (!strcmp(proc->info->name, id))
			return proc;

	return NULL;
}

int dc_replace_property_blob_from_id(struct drm_device *dev, struct drm_property_blob **blob,
				     u64 blob_id, ssize_t expected_size, ssize_t expected_elem_size,
				     bool *replaced)
{
	struct drm_property_blob *new_blob = NULL;

	if (blob_id != 0) {
		new_blob = drm_property_lookup_blob(dev, blob_id);
		if (!new_blob)
			return -EINVAL;

		if (expected_size > 0 && new_blob->length != expected_size) {
			drm_property_blob_put(new_blob);
			return -EINVAL;
		}
		if (expected_elem_size > 0 && new_blob->length % expected_elem_size != 0) {
			drm_property_blob_put(new_blob);
			return -EINVAL;
		}
	}

	*replaced = drm_property_replace_blob(blob, new_blob);
	drm_property_blob_put(new_blob);

	return 0;
}
