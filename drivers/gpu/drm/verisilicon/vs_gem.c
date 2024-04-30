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

#include <linux/dma-buf.h>

#include "vs_drv.h"
#include "vs_gem.h"

static const struct drm_gem_object_funcs vs_gem_default_funcs;

#ifdef CONFIG_VERISILICON_GEM_ION
#define ION_ALIGN 0x10

static int vs_gem_alloc_buf(struct vs_gem_object *vs_obj)
{
	struct drm_device *dev	    = vs_obj->base.dev;
	struct vs_drm_private *priv = drm_to_vs_priv(dev);
	int ret			    = -ENOMEM;
	size_t len		    = 0;
	unsigned int heap_id_mask   = ~0u;
	unsigned int ionFlags	    = 0;
	dma_addr_t start;
	phys_addr_t phys;

	if (vs_obj->handle) {
		DRM_DEV_DEBUG_KMS(dev->dev, "vs ion handle already allocated.\n");
		return 0;
	}

	vs_obj->handle = ion_alloc(priv->client, vs_obj->size, ION_ALIGN, heap_id_mask, ionFlags);
	if (IS_ERR(vs_obj->handle)) {
		DRM_DEV_DEBUG_KMS(dev->dev, "vs ion alloc failed!\n");
		return ret;
	}
	vs_obj->sgt = vs_obj->handle->buffer->sg_table;

	ion_phys(priv->client, vs_obj->handle->id, &vs_obj->dma_addr, &len);

	/** do lite-mmu mapping.*/
	phys  = sg_phys(vs_obj->sgt->sgl);
	start = phys & dma_get_mask(to_dma_dev(dev));
	len   = PAGE_ALIGN(vs_obj->sgt->sgl->length);

	ret = iommu_map(priv->domain, start, phys, len, 0);
	if (ret != 0) {
		DRM_DEV_DEBUG_KMS(dev->dev, "vs gem iommu_map failed!\n");
	}

	return 0;
}

static void vs_gem_free_buf(struct vs_gem_object *vs_obj)
{
	struct drm_device *dev	    = vs_obj->base.dev;
	struct vs_drm_private *priv = drm_to_vs_priv(dev);
	dma_addr_t start;
	phys_addr_t phys;
	size_t len;

	if (!vs_obj->handle) {
		DRM_DEV_DEBUG_KMS(dev->dev, "vs ion handle is invalid.\n");
		return;
	}

	/** do lite-mmu unmapping.*/
	phys  = sg_phys(vs_obj->sgt->sgl);
	start = phys & dma_get_mask(to_dma_dev(dev));
	len   = PAGE_ALIGN(vs_obj->sgt->sgl->length);

	iommu_unmap(priv->domain, start, len);

	ion_free(priv->client, vs_obj->handle);
}

struct sg_table *vs_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	const struct vs_gem_object *vs_obj = to_vs_gem_object(obj);

	if (!vs_obj->sgt) {
		DRM_DEV_ERROR(obj->dev->dev, "failed to get sgtable.\n");
		return ERR_PTR(-ENXIO);
	}

	return vs_obj->sgt;
}

static int vs_gem_mmap_obj(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	struct vs_gem_object *vs_obj = to_vs_gem_object(obj);
	unsigned long vm_size;
	int ret;

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;

	vm_size = vma->vm_end - vma->vm_start;

	if (vm_size > vs_obj->size)
		return -EINVAL;

	ret = ion_mmap(vs_obj->handle->buffer, vma);

	if (ret)
		drm_gem_vm_close(vma);
	return ret;
}
#else
static int vs_gem_alloc_buf(struct vs_gem_object *vs_obj)
{
	struct drm_device *dev = vs_obj->base.dev;
	int ret		       = -ENOMEM;

	if (vs_obj->dma_addr) {
		DRM_DEV_DEBUG_KMS(dev->dev, "already allocated.\n");
		return 0;
	}

	vs_obj->dma_attrs = DMA_ATTR_WRITE_COMBINE | DMA_ATTR_NO_KERNEL_MAPPING;

	// if (!is_iommu_enabled(dev))
	vs_obj->dma_attrs |= DMA_ATTR_FORCE_CONTIGUOUS;

	vs_obj->cookie = dma_alloc_attrs(to_dma_dev(dev), vs_obj->size, &vs_obj->dma_addr,
					 GFP_KERNEL, vs_obj->dma_attrs);
	if (!vs_obj->cookie) {
		DRM_DEV_ERROR(dev->dev, "failed to allocate [0x%x] buffer.\n", (u32)vs_obj->size);
		return ret;
	}

	return 0;
}

static void vs_gem_free_buf(struct vs_gem_object *vs_obj)
{
	struct drm_device *dev = vs_obj->base.dev;

	if (!vs_obj->dma_addr) {
		DRM_DEV_DEBUG_KMS(dev->dev, "dma_addr is invalid.\n");
		return;
	}

	dma_free_attrs(to_dma_dev(dev), vs_obj->size, vs_obj->cookie, (dma_addr_t)vs_obj->dma_addr,
		       vs_obj->dma_attrs);
}

struct sg_table *vs_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct vs_gem_object *vs_obj = to_vs_gem_object(obj);
	struct sg_table *sgt;
	int ret;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	ret = dma_get_sgtable_attrs(to_dma_dev(obj->dev), sgt, vs_obj->cookie, vs_obj->dma_addr,
				    vs_obj->size, vs_obj->dma_attrs);
	if (ret) {
		DRM_DEV_ERROR(obj->dev->dev, "failed to get sgtable.\n");
		return ERR_PTR(ret);
	}

	return sgt;
}

static int vs_gem_mmap_obj(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	struct vs_gem_object *vs_obj = to_vs_gem_object(obj);
	struct drm_device *drm_dev   = vs_obj->base.dev;
	unsigned long vm_size;
	int ret;

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;

	vm_size = vma->vm_end - vma->vm_start;
	if (vm_size > vs_obj->size)
		return -EINVAL;

	ret = dma_mmap_attrs(to_dma_dev(drm_dev), vma, vs_obj->cookie, vs_obj->dma_addr,
			     vs_obj->size, vs_obj->dma_attrs);
	if (ret)
		drm_gem_vm_close(vma);
	return ret;
}
#endif

static void vs_gem_free_object(struct drm_gem_object *obj)
{
	struct vs_gem_object *vs_obj = to_vs_gem_object(obj);

	if (obj->import_attach)
		drm_prime_gem_destroy(obj, vs_obj->sgt);
	else
		vs_gem_free_buf(vs_obj);

	drm_gem_object_release(obj);

	kfree(vs_obj);
}

static struct vs_gem_object *vs_gem_alloc_object(struct drm_device *dev, size_t size)
{
	struct vs_gem_object *vs_obj;
	struct drm_gem_object *obj;
	int ret;

	vs_obj = kzalloc(sizeof(*vs_obj), GFP_KERNEL);
	if (!vs_obj)
		return ERR_PTR(-ENOMEM);

	vs_obj->size = size;
	obj	     = &vs_obj->base;

	ret = drm_gem_object_init(dev, obj, size);
	if (ret)
		goto err_free;

	vs_obj->base.funcs = &vs_gem_default_funcs;

	ret = drm_gem_create_mmap_offset(obj);
	if (ret) {
		drm_gem_object_release(obj);
		goto err_free;
	}

	return vs_obj;

err_free:
	kfree(vs_obj);
	return ERR_PTR(ret);
}

struct vs_gem_object *vs_gem_create_object(struct drm_device *dev, size_t size)
{
	struct vs_gem_object *vs_obj;
	int ret;

	size = PAGE_ALIGN(size);

	vs_obj = vs_gem_alloc_object(dev, size);
	if (IS_ERR(vs_obj))
		return vs_obj;

	ret = vs_gem_alloc_buf(vs_obj);
	if (ret) {
		drm_gem_object_release(&vs_obj->base);
		kfree(vs_obj);
		return ERR_PTR(ret);
	}

	return vs_obj;
}

static struct vs_gem_object *vs_gem_create_with_handle(struct drm_device *dev,
						       struct drm_file *file, size_t size,
						       unsigned int *handle)
{
	struct vs_gem_object *vs_obj;
	struct drm_gem_object *obj;
	int ret;

	vs_obj = vs_gem_create_object(dev, size);
	if (IS_ERR(vs_obj))
		return vs_obj;

	obj = &vs_obj->base;

	ret = drm_gem_handle_create(file, obj, handle);

	drm_gem_object_put(obj);
	if (ret)
		return ERR_PTR(ret);

	return vs_obj;
}

static const struct vm_operations_struct vs_vm_ops = {
	.open  = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct drm_gem_object_funcs vs_gem_default_funcs = {
	.free	      = vs_gem_free_object,
	.get_sg_table = vs_gem_prime_get_sg_table,
	.vm_ops	      = &vs_vm_ops,
};

int vs_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
		       struct drm_mode_create_dumb *args)
{
	struct vs_drm_private *priv = drm_to_vs_priv(dev);
	struct vs_gem_object *vs_obj;
	unsigned int pitch = args->width * DIV_ROUND_UP(args->bpp, 8);

	args->pitch = ALIGN(pitch, priv->pitch_alignment);
	args->size  = PAGE_ALIGN(args->pitch * args->height);

	vs_obj = vs_gem_create_with_handle(dev, file, args->size, &args->handle);
	return PTR_ERR_OR_ZERO(vs_obj);
}

struct drm_gem_object *vs_gem_prime_import(struct drm_device *dev, struct dma_buf *dma_buf)
{
	return drm_gem_prime_import_dev(dev, dma_buf, to_dma_dev(dev));
}

struct drm_gem_object *vs_gem_prime_import_sg_table(struct drm_device *dev,
						    struct dma_buf_attachment *attach,
						    struct sg_table *sgt)
{
	struct vs_gem_object *vs_obj;
	size_t size = attach->dmabuf->size;

	size = PAGE_ALIGN(size);

	vs_obj = vs_gem_alloc_object(dev, size);
	if (IS_ERR(vs_obj))
		return ERR_CAST(vs_obj);

	vs_obj->dma_addr = sg_dma_address(sgt->sgl);

	vs_obj->sgt = sgt;

	return &vs_obj->base;
}

int vs_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	int ret = 0;

	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if (ret < 0)
		return ret;

	return vs_gem_mmap_obj(obj, vma);
}

int vs_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_gem_object *obj;
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if (ret)
		return ret;

	obj = vma->vm_private_data;

	if (obj->import_attach)
		return dma_buf_mmap(obj->dma_buf, vma, 0);

	return vs_gem_mmap_obj(obj, vma);
}

MODULE_IMPORT_NS(DMA_BUF);
