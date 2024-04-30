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

#include <linux/module.h>

#include <drm/drm_gem.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_plane.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_writeback.h>
#include <drm/drm_gem_framebuffer_helper.h>

#include "vs_fb.h"
#include "vs_gem.h"

static struct drm_framebuffer_funcs vs_fb_funcs = {
	.create_handle = drm_gem_fb_create_handle,
	.destroy       = drm_gem_fb_destroy,
	.dirty	       = drm_atomic_helper_dirtyfb,
};

struct drm_framebuffer *vs_fb_alloc(struct drm_device *dev, const struct drm_mode_fb_cmd2 *mode_cmd,
				    struct vs_gem_object **obj, unsigned int num_planes)
{
	struct drm_framebuffer *fb;
	int ret, i;

	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	if (!fb)
		return ERR_PTR(-ENOMEM);

	drm_helper_mode_fill_fb_struct(dev, fb, mode_cmd);

	for (i = 0; i < num_planes; i++)
		fb->obj[i] = &obj[i]->base;

	ret = drm_framebuffer_init(dev, fb, &vs_fb_funcs);
	if (ret) {
		dev_err(dev->dev, "Failed to initialize framebuffer: %d\n", ret);
		kfree(fb);
		return ERR_PTR(ret);
	}

	return fb;
}

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

static bool any_wb_has_format(struct drm_device *dev, u32 format, u64 modifier)
{
	struct drm_connector_list_iter iter;
	struct drm_connector *connector		     = NULL;
	struct drm_writeback_connector *wb_connector = NULL;
	bool ret				     = false;

	drm_connector_list_iter_begin(dev, &iter);
	drm_for_each_connector_iter (connector, &iter) {
		if (connector->connector_type != DRM_MODE_CONNECTOR_WRITEBACK)
			continue;

		wb_connector = drm_connector_to_writeback(connector);

		if (wb_has_format(wb_connector, format, modifier)) {
			ret = true;
			break;
		}
	}
	drm_connector_list_iter_end(&iter);

	return ret;
}

struct drm_framebuffer *vs_fb_create(struct drm_device *dev, struct drm_file *file_priv,
				     const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_framebuffer *fb;
	const struct drm_format_info *info;
	struct vs_gem_object *objs[MAX_NUM_PLANES];
	struct drm_gem_object *obj;
	unsigned int height, size;
	unsigned char i, num_planes;
	int ret = 0;

	info = drm_format_info(mode_cmd->pixel_format);
	if (!info)
		return ERR_PTR(-EINVAL);

	if (!drm_any_plane_has_format(dev, mode_cmd->pixel_format, mode_cmd->modifier[0]) &&
	    !any_wb_has_format(dev, mode_cmd->pixel_format, mode_cmd->modifier[0])) {
		DRM_DEBUG_KMS("unsupported pixel format %p4cc / modifier 0x%llx\n", &info->format,
			      mode_cmd->modifier[0]);

		return ERR_PTR(-EINVAL);
	}

	num_planes = info->num_planes;
	if (num_planes > MAX_NUM_PLANES)
		return ERR_PTR(-EINVAL);

	for (i = 0; i < num_planes; i++) {
		obj = drm_gem_object_lookup(file_priv, mode_cmd->handles[i]);
		if (!obj) {
			dev_err(dev->dev, "Failed to lookup GEM object.\n");
			ret = -ENXIO;
			goto err;
		}

		height = drm_format_info_plane_height(info, mode_cmd->height, i);

		size = height * mode_cmd->pitches[i] + mode_cmd->offsets[i];

		if (obj->size < size) {
			drm_gem_object_put(obj);
			ret = -EINVAL;
			goto err;
		}

		objs[i] = to_vs_gem_object(obj);
	}

	fb = vs_fb_alloc(dev, mode_cmd, objs, i);
	if (IS_ERR(fb)) {
		ret = PTR_ERR(fb);
		goto err;
	}

	return fb;

err:
	for (; i > 0; i--)
		drm_gem_object_put(&objs[i - 1]->base);

	return ERR_PTR(ret);
}

struct vs_gem_object *vs_fb_get_gem_obj(struct drm_framebuffer *fb, unsigned char plane)
{
	if (plane > MAX_NUM_PLANES)
		return NULL;

	return to_vs_gem_object(fb->obj[plane]);
}
