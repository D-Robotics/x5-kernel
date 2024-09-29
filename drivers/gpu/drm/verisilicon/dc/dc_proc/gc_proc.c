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

#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_print.h>

#include "dc_proc.h"
#include "gc_proc.h"
#include "dc_hw_proc.h"
#include "vs_gem.h"
#include "vs_fb.h"
#include "vs_drv.h"
#include "nano2D_kernel_driver.h"
#include "nano2D.h"

struct gpu_plane_context {
	struct drm_framebuffer *fb_display;
	struct drm_framebuffer *gpu_out;
	struct drm_framebuffer *fb_tmp;
	int opened;
};

static struct gpu_plane_context context;

struct gpu_plane_proc {
	struct dc_proc base;
	/** add gpu node structure here */
	struct vs_n2d_aux *aux;
	bool updating; // new framebuffer is pushed in, current fb can be recycled in next vblank
};

static inline struct gpu_plane_proc *to_gpu_plane_proc(struct dc_proc *proc)
{
	return container_of(proc, struct gpu_plane_proc, base);
}

static struct drm_framebuffer *vs_create_fb_internal(struct drm_device *dev,
						     const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct vs_gem_object *objs[MAX_NUM_PLANES];
	unsigned int height, size = 0;
	const struct drm_format_info *info;
	struct drm_framebuffer *fb;
	int ret;
	u8 i;

	info = drm_format_info(mode_cmd->pixel_format);
	if (!info)
		return ERR_PTR(-EINVAL);

	for (i = 0; i < info->num_planes; i++) {
		height = drm_format_info_plane_height(info, mode_cmd->height, i);
		size   = height * mode_cmd->pitches[i] + mode_cmd->offsets[i];
	}

	objs[0] = vs_gem_create_object(dev, size);
	if (IS_ERR(objs[0])) {
		dev_err(dev->dev, "Failed to create GEM object.\n");
		ret = -ENXIO;
		return ERR_PTR(ret);
	}

	for (i = 1; i < info->num_planes; i++) {
		drm_gem_object_get(&objs[0]->base);
		// 2d GPU use one buffer to hold multiple color planes, but drm_framebuffer need
		// drm_gem_object for each plane, so bind same drm_gem_object to each color planes
		objs[i] = objs[0];
	}

	fb = vs_fb_alloc(dev, mode_cmd, objs, info->num_planes);
	if (IS_ERR(fb)) {
		ret = PTR_ERR(fb);
		goto err;
	}

	return fb;
err:
	for (i = 0; i < info->num_planes; i++)
		drm_gem_object_put(&objs[i]->base);

	return ERR_PTR(ret);
}

static void gpu_proc_disable_plane(struct dc_proc *dc_proc, void *old_state)
{
	struct gpu_plane_proc *hw_plane		= to_gpu_plane_proc(dc_proc);
	const struct gpu_plane_info *plane_info = hw_plane->base.info->info;
	int error				= 0;
	struct vs_n2d_aux *aux			= hw_plane->aux;

	if (context.opened == 1) {
		error = aux_close(aux);
		if (error < 0) {
			dev_info(aux->dev, "%s: failed to close n2d aux.\n", __func__);
			return;
		}
		context.opened = 0;
	}

	if (plane_info->features & GPU_PLANE_OUT) {
		pr_debug("[disable]removing fb display\n");
		drm_framebuffer_assign(&context.fb_display, NULL);
		drm_framebuffer_assign(&context.gpu_out, NULL);
		// crtc disable is immediately take effect, this flag shall not be cleared in vblank
		// handler, so need reset the flag here.
		hw_plane->updating = false;
	}
}

static n2d_buffer_format_t to_gc_format(u32 format)
{
	n2d_buffer_format_t f = 0;
	switch (format) {
	case DRM_FORMAT_ARGB4444:
		f = N2D_ARGB4444;
		break;
	case DRM_FORMAT_ARGB1555:
		f = N2D_A1R5G5B5;
		break;
	case DRM_FORMAT_RGB565:
		f = N2D_RGB565;
		break;
	case DRM_FORMAT_ARGB8888:
		f = N2D_ARGB8888;
		break;
	case DRM_FORMAT_RGBA4444:
		f = N2D_RGBA4444;
		break;
	case DRM_FORMAT_RGBA5551:
		f = N2D_R5G5B5A1;
		break;
	case DRM_FORMAT_RGBA8888:
		f = N2D_RGBA8888;
		break;
	case DRM_FORMAT_ABGR4444:
		f = N2D_ABGR4444;
		break;
	case DRM_FORMAT_ABGR1555:
		f = N2D_A1B5G5R5;
		break;
	case DRM_FORMAT_BGR565:
		f = N2D_BGR565;
		break;
	case DRM_FORMAT_ABGR8888:
		f = N2D_ABGR8888;
		break;
	case DRM_FORMAT_BGRA4444:
		f = N2D_BGRA4444;
		break;
	case DRM_FORMAT_BGRA5551:
		f = N2D_B5G5R5A1;
		break;
	case DRM_FORMAT_BGRA8888:
		f = N2D_BGRA8888;
		break;
	case DRM_FORMAT_XRGB8888:
		f = N2D_XRGB8888;
		break;
	case DRM_FORMAT_XBGR8888:
		f = N2D_XBGR8888;
		break;
	case DRM_FORMAT_YUYV:
		f = N2D_YUYV;
		break;
	case DRM_FORMAT_NV12:
		f = N2D_NV12;
		break;
	case DRM_FORMAT_NV21:
		f = N2D_NV21;
		break;
	default:
		f = N2D_ARGB8888;
		break;
	}

	return f;
}

static n2d_orientation_t to_gc_rotation(unsigned int rotation)
{
	n2d_orientation_t orientation;

	switch (rotation) {
	case DRM_MODE_ROTATE_0:
		orientation = N2D_0;
		break;
	case DRM_MODE_ROTATE_90:
		orientation = N2D_90;
		break;
	case DRM_MODE_ROTATE_180:
		orientation = N2D_180;
		break;
	case DRM_MODE_ROTATE_270:
		orientation = N2D_270;
		break;
	case DRM_MODE_REFLECT_X:
		orientation = N2D_FLIP_X;
		break;
	case DRM_MODE_REFLECT_Y:
		orientation = N2D_FLIP_Y;
		break;

	default:
		orientation = N2D_0;
		break;
	}

	return orientation;
}

static void update_iommu(struct device *dev, dma_addr_t phys, size_t size)
{
	struct iommu_domain *domain = NULL;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		dev_err(dev, "failed to get iommu domain.\n");
		return;
	}

	iommu_map(domain, phys, phys, PAGE_ALIGN(size), 0);
}

static void aux_rotate(struct vs_n2d_aux *aux, unsigned int rotation, n2d_rectangle_t *rect)
{
	n2d_buffer_t src = {0}, dst = {0};
	n2d_uintptr_t handle	       = 0;
	n2d_user_memory_desc_t memDesc = {0};
	n2d_error_t error	       = N2D_SUCCESS;
	struct n2d_config *config      = &aux->config;

	if (context.opened == 0) {
		error = aux_open(aux);
		if (error < 0) {
			dev_info(aux->dev, "%s: failed to open n2d aux.\n", __func__);
			return;
		}
		context.opened = 1;
	}

	N2D_ON_ERROR(n2d_open());
	/* check size based on format/width, height */
	memDesc.flag	 = N2D_WRAP_FROM_USERMEMORY;
	memDesc.physical = config->in_buffer_addr[0][0]; /* assume the buffer is contiguous */
	memDesc.size	 = gcmALIGN(config->input_stride[0] * config->input_height[0] + 64,
				    64); /* assume buffer is aligned */
	update_iommu(aux->dev, memDesc.physical, memDesc.size);
	N2D_ON_ERROR(n2d_wrap(&memDesc, &handle));

	src.width    = config->input_width[0];
	src.height   = config->input_height[0];
	src.stride   = config->input_stride[0];
	src.format   = config->ninputs;
	src.handle   = handle;
	src.alignedh = config->input_height[0];
	src.alignedw = config->input_stride[0];
	if (src.format == N2D_NV12 || src.format == N2D_NV21) {
		src.alignedw = gcmALIGN(src.alignedw, 64);
	}
	src.orientation = to_gc_rotation(rotation);
	N2D_ON_ERROR(n2d_map(&src));
	src.tiling = N2D_LINEAR;

	memDesc.flag	 = N2D_WRAP_FROM_USERMEMORY;
	memDesc.physical = config->out_buffer_addr[0];
	memDesc.size	 = gcmALIGN(config->output_stride * config->output_height + 64, 64);
	update_iommu(aux->dev, memDesc.physical, memDesc.size);
	N2D_ON_ERROR(n2d_wrap(&memDesc, &handle));

	dst.width    = config->output_width;
	dst.height   = config->output_height;
	dst.stride   = config->output_stride;
	dst.format   = config->output_format;
	dst.handle   = handle;
	dst.alignedh = config->output_height;
	dst.alignedw = config->output_stride;
	if (dst.format == N2D_NV12 || dst.format == N2D_NV21) {
		dst.alignedw = gcmALIGN(dst.alignedw, 64);
	}
	N2D_ON_ERROR(n2d_map(&dst));
	dst.tiling = N2D_LINEAR;

	N2D_ON_ERROR(n2d_blit(&dst, rect, &src, N2D_NULL, N2D_BLEND_NONE));
	N2D_ON_ERROR(n2d_commit_ex(N2D_FALSE));

// cppcheck-suppress unusedLabel
on_error:
	n2d_free(&src);
	n2d_free(&dst);
	n2d_close();
}

static void aux_overlay(struct vs_n2d_aux *aux)
{
	aux_rotate(aux, 0, N2D_NULL);
}

static void update_cfg(struct vs_n2d_aux *aux, struct drm_framebuffer *dst,
		       struct drm_framebuffer *src)
{
	dma_addr_t dma_addr[MAX_NUM_PLANES] = {};

	struct n2d_config *cfg = &aux->config;
	cfg->input_width[0]    = src->width;
	cfg->input_height[0]   = src->height;
	cfg->input_stride[0]   = src->pitches[0]; /* default format is argb */
	get_buffer_addr(src, dma_addr);
	cfg->in_buffer_addr[0][0] = dma_addr[0]; /* assume the buffer is continuous */
	cfg->ninputs		  = to_gc_format(src->format->format);
	pr_debug("%s: src_format = %d.\n", __func__, cfg->ninputs);
	pr_debug("%s: src_width = %d.\n", __func__, cfg->input_width[0]);
	pr_debug("%s: src_height = %d.\n", __func__, cfg->input_height[0]);
	pr_debug("%s: src_stride = %d.\n", __func__, cfg->input_stride[0]);
	pr_debug("%s: src addr = 0x%llx.\n", __func__, dma_addr[0]);

	cfg->output_width  = dst->width;
	cfg->output_height = dst->height;
	cfg->output_stride = dst->pitches[0];
	get_buffer_addr(dst, dma_addr);
	cfg->out_buffer_addr[0] = dma_addr[0];
	cfg->output_format	= to_gc_format(dst->format->format);
	pr_debug("processing src: %d, dest %d\n", src->base.id, dst->base.id);
	pr_debug("%s: out_format = %d.\n", __func__, cfg->output_format);
	pr_debug("%s: out_width = %d.\n", __func__, cfg->output_width);
	pr_debug("%s: out_height = %d.\n", __func__, cfg->output_height);
	pr_debug("%s: out_stride = %d.\n", __func__, cfg->output_stride);
	pr_debug("%s: out addr = 0x%llx.\n", __func__, dma_addr[0]);
	pr_debug("%s: out addr 1 = 0x%llx.\n", __func__, dma_addr[1]);
}

static int create_fb(struct dc_proc *dc_proc)
{
	struct gpu_plane_proc *hw_plane		= to_gpu_plane_proc(dc_proc);
	struct drm_plane *plane			= to_drm_plane(dc_proc);
	const struct gpu_plane_info *plane_info = hw_plane->base.info->info;
	struct drm_device *drm_dev		= plane->dev;
	const struct vs_drm_private *priv	= drm_to_vs_priv(drm_dev);
	struct drm_plane_state *plane_state	= plane->state;
	struct drm_rect *dst			= &plane_state->dst;
	struct drm_framebuffer *fb;
	int i;
	struct drm_mode_fb_cmd2 fbreq = {
		.width	      = drm_rect_width(dst),
		.height	      = drm_rect_height(dst),
		.pixel_format = plane_info->fourcc,
	};
	const struct drm_format_info *info = drm_get_format_info(drm_dev, &fbreq);

	WARN_ON(!info);

	for (i = 0; i < info->num_planes; i++) {
		fbreq.pitches[i] = num_align(
			drm_format_info_plane_width(info, fbreq.width, i) * info->char_per_block[i],
			priv->pitch_alignment);
		if (i > 0)
			fbreq.offsets[i] = fbreq.pitches[i - 1] *
					   drm_format_info_plane_height(info, fbreq.height, i - 1);
	}

	if (context.gpu_out) { /* if size changes, realloc the buffer */
		if (context.gpu_out->width != fbreq.width ||
		    context.gpu_out->height != fbreq.height)
			drm_framebuffer_put(context.gpu_out);
		else
			return 0;
	}

	fb = vs_create_fb_internal(plane->dev, &fbreq);
	if (IS_ERR(fb))
		return PTR_ERR(fb);

	context.gpu_out = fb;

	return 0;
}

static void gpu_proc_update_plane(struct dc_proc *dc_proc, void *old_drm_plane_state)
{
	struct vs_plane *vs_plane		= proc_to_vs_plane(dc_proc);
	struct gpu_plane_proc *hw_plane		= to_gpu_plane_proc(dc_proc);
	const struct gpu_plane_info *plane_info = hw_plane->base.info->info;
	struct drm_plane_state *plane_state	= vs_plane->base.state;
	struct vs_n2d_aux *aux			= hw_plane->aux;

	struct drm_rect *dst			= &plane_state->dst;
	struct drm_mode_fb_cmd2 fbreq		= {
			  .width	= drm_rect_width(dst),
			  .height	= drm_rect_height(dst),
			  .pixel_format = plane_info->fourcc,
	  };


	if (plane_info->features & GPU_PLANE_OUT) {
		drm_framebuffer_assign(&plane_state->fb, context.fb_tmp);
		drm_framebuffer_assign(&context.fb_tmp, NULL);
		hw_plane->updating = true;
		return;
	}

	if (!context.gpu_out) {
		if (create_fb(dc_proc) != 0) {
			return;
		}
	} else if (context.gpu_out->width != fbreq.width ||
		   context.gpu_out->height != fbreq.height) {
		if (create_fb(dc_proc) != 0) {
			return;
		}
	}

	pr_debug("context.gpu_out = %d\n", context.gpu_out->base.id);
	pr_debug("context.fb_display = %d\n", context.fb_display ? context.fb_display->base.id : 0);

	update_cfg(aux, context.gpu_out, plane_state->fb);
	if (plane_state->rotation & (DRM_MODE_ROTATE_MASK | DRM_MODE_REFLECT_MASK)) {
		aux_rotate(aux, plane_state->rotation, NULL);
	} else {
		aux_overlay(aux);
	}

	drm_framebuffer_assign(&context.fb_tmp, plane_state->fb);
	drm_framebuffer_assign(&plane_state->fb, context.gpu_out);
}

static int gpu_proc_check_plane(struct dc_proc *dc_proc, void *_state)
{
	const struct drm_plane_state *state = _state;

	if (!state->fb)
		return 0;

	return 0;
}

static void gpu_proc_commit_plane(struct dc_proc *dc_proc) {}

static void gpu_proc_destroy_plane(struct dc_proc *dc_proc)
{
	struct gpu_plane_proc *hw_plane = to_gpu_plane_proc(dc_proc);

	kfree(hw_plane);
}

static void gpu_proc_resume_plane(struct dc_proc *dc_proc) {}

static void gpu_proc_destroy_plane_state(struct dc_proc_state *dc_state)
{
	struct gpu_plane_proc_state *state = to_gpu_plane_proc_state(dc_state);

	kfree(state);
}

static struct dc_proc_state *gpu_proc_create_plane_state(void *parent, struct dc_proc *dc_proc)
{
	struct gpu_plane_proc_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	INIT_LIST_HEAD(&state->base.head);

	state->parent = parent;

	return &state->base;
}

static struct dc_proc_state *gpu_proc_duplicate_plane_state(void *parent, struct dc_proc *dc_proc,
							    void *_dc_state)
{
	struct dc_proc_state *dc_state	       = _dc_state;
	struct gpu_plane_proc_state *ori_state = to_gpu_plane_proc_state(dc_state);
	struct gpu_plane_proc_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (WARN_ON(!state))
		return NULL;

	memcpy(state, ori_state, sizeof(*state));

	INIT_LIST_HEAD(&state->base.head);

	state->parent = (struct dc_plane_state *)parent;

	return &state->base;
}

static int gpu_proc_create_plane_prop(struct dc_proc *dc_proc)
{
	return 0;
}

static int gpu_proc_set_plane_prop(struct dc_proc *dc_proc, struct dc_proc_state *dc_state,
				   struct drm_property *property, uint64_t val)
{
	return 0;
}

static int gpu_proc_get_plane_prop(struct dc_proc *dc_proc, struct dc_proc_state *dc_state,
				   struct drm_property *property, uint64_t *val)
{
	return 0;
}

static void gpu_proc_plane_print_state(struct drm_printer *p, struct dc_proc_state *dc_state) {}

static int gpu_proc_check_format_mod(struct dc_proc *dc_proc, u32 format, u64 modifier)
{
	return true;
}

static struct dc_proc *gpu_proc_create_plane(struct device *dev, const struct dc_proc_info *info)
{
	struct gpu_plane_proc *hw_plane;
	struct vs_n2d_aux *aux;

	hw_plane = kzalloc(sizeof(*hw_plane), GFP_KERNEL);
	if (!hw_plane)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&hw_plane->base.head);

	aux = dev_get_drvdata(dev);
	if (aux == NULL) {
		dev_info(dev, "%s: failed to get n2d aux node.\n", __func__);
		return NULL;
	}
	hw_plane->aux = aux;

	return &hw_plane->base;
}

static void gpu_proc_vblank(struct dc_proc *dc_proc)
{
	struct gpu_plane_proc *hw_plane = to_gpu_plane_proc(dc_proc);

	if (context.gpu_out && hw_plane->updating) {
		hw_plane->updating = false;
		// gpu fb not NULL, atomic commit happens before this vblank
		// swap gpu fb and display fb
		drm_framebuffer_assign(&context.fb_tmp, context.fb_display);
		drm_framebuffer_assign(&context.fb_display, context.gpu_out);
		drm_framebuffer_assign(&context.gpu_out, context.fb_tmp);
		drm_framebuffer_assign(&context.fb_tmp, NULL);
	}
}

const struct dc_proc_funcs gpu_plane_funcs = {
	.create		  = gpu_proc_create_plane,
	.update		  = gpu_proc_update_plane,
	.disable	  = gpu_proc_disable_plane,
	.check		  = gpu_proc_check_plane,
	.commit		  = gpu_proc_commit_plane,
	.destroy	  = gpu_proc_destroy_plane,
	.resume		  = gpu_proc_resume_plane,
	.destroy_state	  = gpu_proc_destroy_plane_state,
	.create_state	  = gpu_proc_create_plane_state,
	.duplicate_state  = gpu_proc_duplicate_plane_state,
	.create_prop	  = gpu_proc_create_plane_prop,
	.set_prop	  = gpu_proc_set_plane_prop,
	.get_prop	  = gpu_proc_get_plane_prop,
	.print_state	  = gpu_proc_plane_print_state,
	.check_format_mod = gpu_proc_check_format_mod,
	.vblank		  = gpu_proc_vblank,
};
