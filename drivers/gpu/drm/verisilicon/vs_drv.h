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

#ifndef __VS_DRV_H__
#define __VS_DRV_H__

/** @addtogroup DRM
 *  vs drm driver API.
 *  @ingroup DRIVER
 *
 *  @{
 */

#include <linux/module.h>
#include <linux/platform_device.h>

#include <drm/drm_device.h>
#include <drm/drm_gem.h>

#include "vs_plane.h"

/**
 * vs drm private structure
 *
 */
struct vs_drm_private {
	struct drm_device drm;
	/**
	 * device for DMA API, use the first attached device if support iommu,
	 * else use drm device (only contiguous buffer support).
	 */
	struct device *dma_dev;

	/**
	 * iommu domain for DRM. All DC IOMMU share same domain to reduce
	 * mapping.
	 */
	struct iommu_domain *domain;

	/** pitch_alignment, buffer pitch alignment required by sub-devices.*/
	unsigned int pitch_alignment;

#ifdef CONFIG_VERISILICON_GEM_ION
	/** ion client for vs_gem.*/
	struct ion_client *client;
#endif

	struct drm_property *gamut_conv;
	struct drm_property *gamut_conv_matrix;
	struct drm_property *position;
	struct drm_property *lcsc0_degamma;
	struct drm_property *lcsc0_csc;
	struct drm_property *lcsc0_regamma;
	struct drm_property *lcsc1_degamma;
	struct drm_property *lcsc1_csc;
	struct drm_property *lcsc1_regamma;
	struct drm_property *lcsc_degamma_size;
	struct drm_property *lcsc_regamma_size;
	struct drm_property *bg_color;
	struct drm_property *dither;
	struct drm_property *cursor_mask;
};

/**
 * @brief      attach iommu device
 *
 * @param[in]  drm_dev drm device
 * @param[in]  dev device
 * @return     0 on success, error code on failure
 */
int vs_drm_iommu_attach_device(struct drm_device *drm_dev, struct device *dev);

/**
 * @brief      detach iommu device
 *
 * @param[in]  drm_dev drm device
 * @param[in]  dev device
 *
 */
void vs_drm_iommu_detach_device(struct drm_device *drm_dev, struct device *dev);

/**
 * @brief      update vs drm pitch alignment
 *
 * @param[in]  drm_dev drm device
 * @param[in]  dev device
 *
 */
void vs_drm_update_pitch_alignment(struct drm_device *drm_dev, unsigned int alignment);

#define drm_to_vs_priv(dev) container_of(dev, struct vs_drm_private, drm)

static inline struct device *to_dma_dev(struct drm_device *dev)
{
	const struct vs_drm_private *priv = drm_to_vs_priv(dev);

	return priv->dma_dev;
}

static inline bool is_iommu_enabled(struct drm_device *dev)
{
	const struct vs_drm_private *priv = drm_to_vs_priv(dev);

	return !!priv->domain;
}

/** @} */

#endif /* __VS_DRV_H__ */
