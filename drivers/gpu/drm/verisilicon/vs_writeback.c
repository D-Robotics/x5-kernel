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

#include <drm/drm_print.h>
#include <drm/drm_atomic.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_writeback.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_edid.h>

#include "vs_gem.h"
#include "vs_writeback.h"
#include "vs_crtc.h"
#include "dc_hw.h"

static u32 get_bus_format(u32 fb_format)
{
	u32 bus_format;

	switch (fb_format) {
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_XRGB8888:
		bus_format = MEDIA_BUS_FMT_RGB888_1X24;
		break;
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		bus_format = MEDIA_BUS_FMT_RGB565_1X16;
		break;
	case DRM_FORMAT_YUYV:
		bus_format = MEDIA_BUS_FMT_UYVY8_1X16;
		break;
	case DRM_FORMAT_XRGB2101010:
		bus_format = MEDIA_BUS_FMT_RGB101010_1X30;
		break;
	default:
		bus_format = MEDIA_BUS_FMT_FIXED;
		break;
	}

	return bus_format;
}

static int add_modes_from_connector(struct drm_connector *dst, struct drm_connector *src)
{
	struct drm_display_mode *mode, *ptr;
	int count = 0;

	list_for_each_entry (ptr, &src->modes, head) {
		mode = drm_mode_duplicate(src->dev, ptr);

		drm_mode_probed_add(dst, mode);
		count++;
	}

	return count;
}

static int vs_wb_add_mode_from_connectors(struct drm_connector *wb_conn)
{
	struct drm_connector_list_iter iter;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	u32 possible_crtcs = 0;
	u32 target_crtcs   = 0;
	int count	   = 0;

	drm_connector_for_each_possible_encoder (wb_conn, encoder)
		possible_crtcs |= encoder->possible_crtcs;

	drm_connector_list_iter_begin(wb_conn->dev, &iter);
	drm_for_each_connector_iter (connector, &iter) {
		if (wb_conn == connector)
			continue;

		target_crtcs = 0;

		drm_connector_for_each_possible_encoder (connector, encoder)
			target_crtcs |= encoder->possible_crtcs;

		if (!(target_crtcs & possible_crtcs))
			continue;

		count += add_modes_from_connector(wb_conn, connector);
	}
	drm_connector_list_iter_end(&iter);

	return count;
}

static int vs_wb_get_modes(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	int count	       = 0;

	count = vs_wb_add_mode_from_connectors(connector);

	return count + drm_add_modes_noedid(connector, dev->mode_config.max_width,
					    dev->mode_config.max_height);
}

static enum drm_mode_status vs_wb_mode_valid(struct drm_connector *connector,
					     struct drm_display_mode *mode)
{
	struct drm_device *dev			  = connector->dev;
	const struct drm_mode_config *mode_config = &dev->mode_config;
	int w = mode->hdisplay, h = mode->vdisplay;

	if (w < mode_config->min_width || w > mode_config->max_width)
		return MODE_BAD_HVALUE;

	if (h < mode_config->min_height || h > mode_config->max_height)
		return MODE_BAD_VVALUE;

	return MODE_OK;
}

static int vs_wb_atomic_check(struct drm_connector *connector, struct drm_atomic_state *state)
{
	struct drm_framebuffer *fb;
	struct drm_connector_state *new_conn_state;
	u32 bus_format;

	new_conn_state = drm_atomic_get_new_connector_state(state, connector);
	if (WARN_ON(!new_conn_state))
		return -EINVAL;

	if (!new_conn_state->writeback_job)
		return 0;

	fb = new_conn_state->writeback_job->fb;

	bus_format = get_bus_format(fb->format->format);

	drm_display_info_set_bus_formats(&connector->display_info, &bus_format, 1);

	return 0;
}

static void vs_wb_atomic_commit(struct drm_connector *connector, struct drm_atomic_state *state)
{
	struct drm_writeback_connector *wb_connector;
	struct vs_wb *vs_wb;
	struct drm_connector_state *new_conn_state;

	wb_connector   = drm_connector_to_writeback(connector);
	new_conn_state = drm_atomic_get_new_connector_state(state, connector);
	vs_wb	       = to_vs_wb(wb_connector);

	if (vs_wb->funcs->commit)
		vs_wb->funcs->commit(vs_wb, new_conn_state);
}

static int vs_wb_prepare_writeback_job(struct drm_writeback_connector *connector,
				       struct drm_writeback_job *job)
{
	u8 *wait_vsync_cnt;

	wait_vsync_cnt = kzalloc(sizeof(u8), GFP_KERNEL);
	WARN_ON(!wait_vsync_cnt);

	*wait_vsync_cnt = 1;

	job->priv = wait_vsync_cnt;

	return 0;
}

static void vs_wb_cleanup_writeback_job(struct drm_writeback_connector *connector,
					struct drm_writeback_job *job)
{
	kfree(job->priv);
}

static const struct drm_connector_helper_funcs vs_wb_helper_funcs = {
	.get_modes	       = vs_wb_get_modes,
	.mode_valid	       = vs_wb_mode_valid,
	.prepare_writeback_job = vs_wb_prepare_writeback_job,
	.cleanup_writeback_job = vs_wb_cleanup_writeback_job,
	.atomic_check	       = vs_wb_atomic_check,
	.atomic_commit	       = vs_wb_atomic_commit,
};

static enum drm_connector_status vs_wb_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static void vs_wb_destroy(struct drm_connector *connector)
{
	struct drm_writeback_connector *wb_connector = drm_connector_to_writeback(connector);
	struct vs_wb *vs_wb			     = to_vs_wb(wb_connector);

	drm_connector_cleanup(connector);

	if (vs_wb->funcs->destroy)
		vs_wb->funcs->destroy(vs_wb);
}

static void vs_wb_atomic_destroy_state(struct drm_connector *connector,
				       struct drm_connector_state *state)
{
	struct drm_writeback_connector *wb_connector = drm_connector_to_writeback(connector);
	struct vs_wb *vs_wb			     = to_vs_wb(wb_connector);

	__drm_atomic_helper_connector_destroy_state(state);

	if (vs_wb->funcs->destroy_state)
		vs_wb->funcs->destroy_state(state);
	else
		kfree(state);
}

static void vs_wb_reset(struct drm_connector *connector)
{
	struct drm_writeback_connector *wb_connector = drm_connector_to_writeback(connector);
	struct vs_wb *vs_wb			     = to_vs_wb(wb_connector);
	struct drm_connector_state *state;

	if (connector->state) {
		vs_wb_atomic_destroy_state(connector, connector->state);
		connector->state = NULL;
	}

	if (vs_wb->funcs->create_state)
		state = vs_wb->funcs->create_state(vs_wb);
	else
		state = kzalloc(sizeof(*state), GFP_KERNEL);

	if (state)
		__drm_atomic_helper_connector_reset(connector, state);
}

static struct drm_connector_state *vs_wb_atomic_duplicate_state(struct drm_connector *connector)
{
	struct drm_writeback_connector *wb_connector = drm_connector_to_writeback(connector);
	struct vs_wb *vs_wb			     = to_vs_wb(wb_connector);
	struct drm_connector_state *state;

	if (WARN_ON(!connector->state))
		return NULL;

	if (vs_wb->funcs->duplicate_state)
		state = vs_wb->funcs->duplicate_state(vs_wb);
	else
		state = kzalloc(sizeof(*state), GFP_KERNEL);

	if (!state)
		return NULL;

	__drm_atomic_helper_connector_duplicate_state(connector, state);

	return state;
}

static int vs_wb_atomic_set_property(struct drm_connector *connector,
				     struct drm_connector_state *state,
				     struct drm_property *property, uint64_t val)
{
	struct drm_writeback_connector *wb_connector = drm_connector_to_writeback(connector);
	struct vs_wb *vs_wb			     = to_vs_wb(wb_connector);

	if (vs_wb->funcs->set_prop)
		return vs_wb->funcs->set_prop(vs_wb, state, property, val);

	return -EINVAL;
}

static int vs_wb_atomic_get_property(struct drm_connector *connector,
				     const struct drm_connector_state *state,
				     struct drm_property *property, uint64_t *val)
{
	struct drm_writeback_connector *wb_connector = drm_connector_to_writeback(connector);
	struct vs_wb *vs_wb			     = to_vs_wb(wb_connector);

	if (vs_wb->funcs->get_prop)
		return vs_wb->funcs->get_prop(vs_wb, state, property, val);

	return -EINVAL;
}

static void vs_wb_print_state(struct drm_printer *p, const struct drm_connector_state *state)
{
	struct drm_writeback_connector *wb_connector = drm_connector_to_writeback(state->connector);
	struct vs_wb *vs_wb			     = to_vs_wb(wb_connector);

	if (vs_wb->funcs->print_state)
		vs_wb->funcs->print_state(p, state);
}

static const struct drm_connector_funcs vs_wb_funcs = {
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.detect			= vs_wb_detect,
	.destroy		= vs_wb_destroy,
	.reset			= vs_wb_reset,
	.atomic_duplicate_state = vs_wb_atomic_duplicate_state,
	.atomic_destroy_state	= vs_wb_atomic_destroy_state,
	.atomic_get_property	= vs_wb_atomic_get_property,
	.atomic_set_property	= vs_wb_atomic_set_property,
	.atomic_print_state	= vs_wb_print_state,
};

static bool wb_has_format(struct drm_writeback_connector *wb_connector, u32 format, u64 modifier)
{
	const u32 *formats_data = wb_connector->pixel_formats_blob_ptr->data;
	size_t len		= wb_connector->pixel_formats_blob_ptr->length;
	unsigned int i;

	if (modifier != DRM_FORMAT_MOD_LINEAR)
		return false;

	for (i = 0; i < len / 4; i++)
		if (formats_data[i] == format)
			return true;

	return false;
}

static int wb_encoder_atomic_check(struct drm_encoder *encoder, struct drm_crtc_state *crtc_state,
				   struct drm_connector_state *conn_state)
{
	struct drm_connector *connector		     = conn_state->connector;
	struct drm_writeback_connector *wb_connector = drm_connector_to_writeback(connector);
	struct drm_framebuffer *fb;

	if (!conn_state->writeback_job)
		return 0;

	fb = conn_state->writeback_job->fb;
	if (fb->width != crtc_state->mode.hdisplay || fb->height != crtc_state->mode.vdisplay) {
		DRM_DEBUG_KMS("Invalid framebuffer size %ux%u\n", fb->width, fb->height);
		return -EINVAL;
	}

	if (!wb_has_format(wb_connector, fb->format->format, fb->modifier))
		return -EINVAL;

	return 0;
}

static const struct drm_encoder_helper_funcs wb_encoder_helper_funcs = {
	.atomic_check = wb_encoder_atomic_check,
};

int vs_wb_init(struct drm_device *drm_dev, struct vs_wb *vs_wb)
{
	const struct vs_wb_info *info = vs_wb->info;
	u32 possible_crtcs =
		find_possible_crtcs_with_name(drm_dev, info->crtc_names, info->num_crtcs);
	int ret;

	ret = drm_writeback_connector_init(drm_dev, &vs_wb->base, &vs_wb_funcs,
					   &wb_encoder_helper_funcs, info->wb_formats,
					   info->num_wb_formats, possible_crtcs);

	if (ret)
		return ret;

	drm_connector_helper_add(&vs_wb->base.base, &vs_wb_helper_funcs);

	return 0;
}
