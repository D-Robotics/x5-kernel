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

#ifndef __VS_GEM_H__
#define __VS_GEM_H__

/** @addtogroup DRM
 *  vs drm gem API.
 *  @ingroup DRIVER
 *
 *  @{
 */

#include <linux/dma-buf.h>
#ifdef CONFIG_VERISILICON_GEM_ION
#include <linux/ion.h>
#endif

#include <drm/drm_gem.h>
#include <drm/drm_prime.h>

#include "vs_drv.h"

/**
 * vs gem object structure
 */
struct vs_gem_object {
	/** drm gem object.*/
	struct drm_gem_object base;
	/** size requested from user.*/
	size_t size;
	/**
	 * bus address(accessed by dma) to allocated memory region,
	 * this address could be physical address without IOMMU and device
	 * address with IOMMU.
	 */
	dma_addr_t dma_addr;
	/** Imported sg_table.*/
	struct sg_table *sgt;

#ifdef CONFIG_VERISILICON_GEM_ION
	/** ion handle for the ion_buffer.*/
	struct ion_handle *handle;
#else
	/**
	 * cookie returned by dma_alloc_attrs, not kernel virtual address with
	 * DMA_ATTR_NO_KERNEL_MAPPING.
	 */
	void *cookie;
	/** attribute for DMA API.*/
	unsigned long dma_attrs;
#endif
};

static inline struct vs_gem_object *to_vs_gem_object(struct drm_gem_object *obj)
{
	return container_of(obj, struct vs_gem_object, base);
}

/**
 * @brief      vs gem create object
 *
 * @param[in]  dev drm device structure
 * @param[in]  size size of vs_gem_object
 * @return     vs_gem_object
 *
 */
struct vs_gem_object *vs_gem_create_object(struct drm_device *dev, size_t size);

/**
 * @brief      vs gem prime buffer memory map
 *
 * @param[in]  obj GEM buffer object
 * @param[in]  vma virtual memory area struct
 * @return     0 on success, error code on failure
 *
 */
int vs_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma);

/**
 * @brief      create vs gem dump buffer
 *
 * @param[in]  file drm file private data
 * @param[in]  dev drm_device structure
 * @param[in]  args drm_mode_create_dumb structure
 * @return     0 on success, error code on failure
 *
 */
int vs_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
		       struct drm_mode_create_dumb *args);
/**
 * @brief      vs gem mmap function
 *
 * @param[in]  filp file structure
 * @param[in]  vma virtual memory area
 * @return     0 on success, error code on failure
 *
 */
int vs_gem_mmap(struct file *filp, struct vm_area_struct *vma);

/**
 * @brief      get sg table list
 *
 * @param[in]  obj GEM buffer object
 * @return     sg_table
 *
 */
struct sg_table *vs_gem_prime_get_sg_table(struct drm_gem_object *obj);

/**
 * @brief      vs core implementation of the import callback
 *
 * @param[in]  dev drm device structure
 * @param[in]  dma_buf shared buffer object
 * @return     drm_gem_object GEM buffer object
 *
 */
struct drm_gem_object *vs_gem_prime_import(struct drm_device *dev, struct dma_buf *dma_buf);

/**
 * @brief      vs gem prime buffer import scatter-gather table
 *
 * @param[in]  dev drm device structure
 * @param[in]  attach holds device-buffer attachment data
 * @param[in]  sgt sg_table
 * @return     drm_gem_object GEM buffer object
 *
 */
struct drm_gem_object *vs_gem_prime_import_sg_table(struct drm_device *dev,
						    struct dma_buf_attachment *attach,
						    struct sg_table *sgt);

/** @} */

#endif /* __VS_GEM_H__ */
