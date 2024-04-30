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

#ifndef __VS_FB_H__
#define __VS_FB_H__

/** @addtogroup DRM
 *  vs drm framebuffer API.
 *  @ingroup DRIVER
 *
 *  @{
 */

#define MAX_NUM_PLANES 3

/**
 * @fn         vs_fb_get_gem_obj
 *
 * @brief      get vs_gem_object based on plane name
 *
 * @param      [in]  fb - drm frame buffer
 * @param      [in]  plane - plane name
 * @return     vs_gem_object
 *
 */
struct vs_gem_object *vs_fb_get_gem_obj(struct drm_framebuffer *fb, unsigned char plane);

struct drm_framebuffer *vs_fb_alloc(struct drm_device *dev, const struct drm_mode_fb_cmd2 *mode_cmd,
				    struct vs_gem_object **obj, unsigned int num_planes);

/**
 * @fn         vs_fb_create
 *
 * @brief      create drm frame buffer
 *
 * @param      [in]  dev - drm device
 * @param      [in]  file_priv - drm file
 * @param      [in]  mode_cmd - drm_mode_fb_cmd2
 * @return     drm_framebuffer
 *
 */
struct drm_framebuffer *vs_fb_create(struct drm_device *dev, struct drm_file *file_priv,
				     const struct drm_mode_fb_cmd2 *mode_cmd);
/** @} */

#endif /* __VS_FB_H__ */
