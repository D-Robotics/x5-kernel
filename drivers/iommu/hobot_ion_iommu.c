/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright 2020 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
/**
 * @file hobot_ion_iommu.c
 *
 * @NO{S17E08C01I}
 * @ASIL{B}
 */
#define pr_fmt(fmt)    "hobot_ion_iommu: " fmt

#include <linux/device.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/dma-buf.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include "hobot_ion_iommu.h"
#include <linux/ion.h>
#if IS_ENABLED(CONFIG_ARM_SMMU_V3)
#include "arm-smmu-v3.h"
#endif
#if IS_ENABLED(CONFIG_HOBOT_IOVMM)
/**
 * @NO{S17E08C01U}
 * @ASIL{B}
 * @brief map the ion buffer address
 *
 * @param[in] buffer: ion buffer
 * @param[in] dev: corresponding device
 * @param[in] dir: dma data flow direction
 * @param[in] iommu_prot: iommu map attributes
 *
 * @retval iovm_map: succeed
 * @retval other: failed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct ion_iovm_map *ion_buffer_iova_create(const struct ion_buffer *buffer,
		struct device *dev, enum dma_data_direction dir, int32_t iommu_prot)
{
	/* Must be called under buffer->lock held */
	struct ion_iovm_map *iovm_map;

	/* code review D6: Dynamic alloc memory to record mapped ion memory */
	iovm_map = (struct ion_iovm_map *)kzalloc(sizeof(struct ion_iovm_map), GFP_ATOMIC);
	if (iovm_map == NULL) {
		dev_err(dev, "%s: Failed to allocate ion_iovm_map for %s\n",
			__func__, dev_name(dev));
		return (struct ion_iovm_map *)ERR_PTR(-ENOMEM);
	}

	iovm_map->iova = hobot_iovmm_map_sg(dev, buffer->sg_table->sgl, buffer->size,
						dir, iommu_prot);

	if (IS_ERR_VALUE(iovm_map->iova) != 0) {/*PRQA S 2895*//*kernel function*/
		dev_err(dev, "%s: Unable to allocate IOVA for %s, ret = %llx.\n",
			__func__, dev_name(dev), iovm_map->iova);
		kfree((void *)iovm_map);
		return (struct ion_iovm_map *)ERR_PTR(-EFAULT);
	}

	iovm_map->dev = dev;
	iovm_map->domain = hobot_iovmm_get_domain_from_dev(dev);
	iovm_map->map_cnt = 1;
	iovm_map->size = buffer->size;

	dev_dbg(dev, "%s: new map added for dev %s, iova %pa\n", __func__,/*PRQA S 1294, 0685*//*kernel function*/
			dev_name(dev), &iovm_map->iova);

	return iovm_map;
}

/**
 * @NO{S17E08C01U}
 * @ASIL{B}
 * @brief map the ion buffer address
 *
 * @param[in] attachment: Structure for recording connection information between device and buffer
 * @param[in] direction: dma data flow direction
 * @param[in] iommu_prot: iommu map attributes
 *
 * @retval dma_addr_t: succeed
 * @retval other: failed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static dma_addr_t ion_iovmm_map_dma_buf(const struct dma_buf_attachment *attachment,
			enum dma_data_direction direction,
			int32_t iommu_prot)
{
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct ion_buffer *buffer = (struct ion_buffer *)dmabuf->priv;
	struct ion_iovm_map *iovm_map;
	struct iommu_domain *domain;

	domain = hobot_iovmm_get_domain_from_dev(attachment->dev);
	if (domain == NULL) {
		(void)pr_err("%s: invalid iommu device.\n", __func__);
		return (dma_addr_t)-ENODEV; /* PRQA S 2895 */
	}

	mutex_lock(&buffer->lock);

	iovm_map = ion_buffer_iova_create(buffer, attachment->dev,
					direction, iommu_prot);

	if (IS_ERR_OR_NULL((const void *)iovm_map)) {
		(void)pr_err("%s: Failed to create iova.\n", __func__);
		mutex_unlock(&buffer->lock);
		return (dma_addr_t)PTR_ERR((void *)iovm_map);
	}

	list_add_tail(&iovm_map->list, &buffer->iovas);
	mutex_unlock(&buffer->lock);

	return iovm_map->iova;
}

/* code review E1: No need to return value */
/**
 * @NO{S17E08C01U}
 * @ASIL{B}
 * @brief Unmap the buffer address
 *
 * @param[in] attachment: Structure for recording connection information between device and buffer
 * @param[in] iova: Mapped IO virtual address
 *
 * @return None
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static void ion_iovmm_unmap_dma_buf(
	const struct dma_buf_attachment *attachment, dma_addr_t iova)
{
	int32_t ret;
	struct ion_iovm_map *this_map = NULL;
	struct ion_iovm_map *iovm_map;
	struct device *dev = attachment->dev;
	struct ion_buffer *buffer = (struct ion_buffer *)attachment->dmabuf->priv;
	struct iommu_domain *domain;

	domain = hobot_iovmm_get_domain_from_dev(attachment->dev);
	if (domain == NULL) {
		(void)pr_err("%s: invalid iommu device\n", __func__);
		return;
	}

	mutex_lock(&buffer->lock);
	list_for_each_entry(iovm_map, &buffer->iovas, list) {/*PRQA S 0497*//*pointer manipulation*/
		if ((domain == iovm_map->domain) && (iova == iovm_map->iova)) {
			if (iovm_map->map_cnt > 0U)
				iovm_map->map_cnt--;
			this_map = iovm_map;
			break;
		}
	}

	if (this_map == NULL) {
		(void)pr_warn("%s: IOVA 0x%llx is not found for %s\n",
			__func__, iova, dev_name(dev));
		mutex_unlock(&buffer->lock);
		return;
	}
	if (this_map->map_cnt == 0U) {
		pr_debug("%s: unmap previous %pa for dev %s\n",/*PRQA S 0685, 1294*//*kernel function*/
			__func__, &this_map->iova, dev_name(this_map->dev));
		ret = hobot_iovmm_unmap_sg(this_map->dev, this_map->iova, this_map->size);
		if (ret != 0) {
			(void)pr_err("%s: Failed to unmap previous %pa for dev %s\n",
				__func__, &this_map->iova, dev_name(this_map->dev));
		}
		list_del(&this_map->list);
		kfree((void *)this_map);
	}

	mutex_unlock(&buffer->lock);
}

/**
 * @NO{S17E08C01U}
 * @ASIL{B}
 * @brief get the ion handle by ion fd
 *
 * @param[out] dma: Output information of dma buffer
 * @param[in] client: ion client
 * @param[in] fd: ion fd
 *
 * @retval "=0": succeed
 * @retval "!=0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t do_fd_ion_import_dma_buf(struct ion_dma_buf_data *dma,
				struct ion_client *client, int32_t fd) {
	dma->ion_handle = ion_import_dma_buf(client, fd);
	if (IS_ERR_OR_NULL((void *)dma->ion_handle)) {
		(void)pr_err("%s: ion_import_dma_buf(fd=%d) failed: %ld\n", __func__,
			fd, PTR_ERR((void *)dma->ion_handle));
		return PTR_ERR(dma->ion_handle);
	}
	return 0;
}

/**
 * @NO{S17E08C01U}
 * @ASIL{B}
 * @brief get the dma buffer by ion fd
 *
 * @param[out] dma: Output information of dma buffer
 * @param[in] client: ion client
 * @param[in] fd: ion fd
 *
 * @retval "=0": succeed
 * @retval "!=0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t do_fd_dma_buf_get(struct ion_dma_buf_data *dma,
				struct ion_client *client, int32_t fd) {
	dma->dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL((void *)dma->dma_buf)) {
		(void)pr_err("%s: dma_buf_get(fd=%d) failed: %ld\n", __func__,
			fd, PTR_ERR((void *)dma->dma_buf));
		ion_free(client, dma->ion_handle);
		return PTR_ERR(dma->dma_buf);
	}
	return 0;
}

/**
 * @NO{S17E08C01U}
 * @ASIL{B}
 * @brief Connect buffer and device, create attachment structure to store connection information
 *
 * @param[out] dma: Output information of dma buffer
 * @param[in] client: ion client
 * @param[in] dev: corresponding device
 *
 * @retval "=0": succeed
 * @retval "=-1": failed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t do_fd_dma_buf_attach(struct ion_dma_buf_data *dma,
				struct ion_client *client, struct device *dev) {
	dma->attachment = dma_buf_attach(dma->dma_buf, dev);
	if (IS_ERR_OR_NULL((void *)dma->attachment)) {
		dev_err(dev, "%s: dma_buf_attach() failed: %ld\n", __func__,
				PTR_ERR((void *)dma->attachment));
		dma_buf_put(dma->dma_buf);
		ion_free(client, dma->ion_handle);
		return PTR_ERR(dma->attachment);
	}
	return 0;
}

/**
 * @NO{S17E08C01U}
 * @ASIL{B}
 * @brief Get connected scatterlist table
 *
 * @param[out] dma: Output information of dma buffer
 * @param[in] client: ion client
 *
 * @retval "=0": succeed
 * @retval "=-1": failed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t do_fd_dma_buf_map_attachment(struct ion_dma_buf_data *dma,
				struct ion_client *client) {
	dma->sg_table = dma_buf_map_attachment(dma->attachment, DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL((void *)dma->sg_table)) {
		(void)pr_err("%s: dma_buf_map_attachment() failed: %ld\n", __func__,
				PTR_ERR((void *)dma->sg_table));
		dma_buf_detach(dma->dma_buf, dma->attachment);
		dma_buf_put(dma->dma_buf);
		ion_free(client, dma->ion_handle);
		return PTR_ERR(dma->sg_table);
	}
	return 0;
}

/**
 * @NO{S17E08C01U}
 * @ASIL{B}
 * @brief Map the dma buffer address
 *
 * @param[out] dma: Output information of dma buffer
 * @param[in] client: ion client
 * @param[in] iommu_prot: iommu map Attributes
 *
 * @retval "=0": succeed
 * @retval "!=0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t do_fd_ion_iovmm_map_dma_buf(struct ion_dma_buf_data *dma,
				struct ion_client *client, int32_t iommu_prot) {
	dma->dma_addr = ion_iovmm_map_dma_buf(dma->attachment,
			DMA_TO_DEVICE, iommu_prot);
	if (IS_ERR_VALUE(dma->dma_addr) != 0) {/*PRQA S 2895*//*kernel function*/
		(void)pr_err("%s: Unable to map ion fd, ret = %llu.\n",
			__func__, dma->dma_addr);
		dma_buf_unmap_attachment(dma->attachment, dma->sg_table, DMA_TO_DEVICE);
		dma_buf_detach(dma->dma_buf, dma->attachment);
		dma_buf_put(dma->dma_buf);
		ion_free(client, dma->ion_handle);
		return (int32_t)dma->dma_addr;
	}
	return 0;
}

/**
 * @NO{S17E08C01I}
 * @ASIL{B}
 * @brief Map the physical address corresponding to ion fd
 *
 * @param[in] dev: corresponding device
 * @param[out] dma: Output information of dma buffer
 * @param[in] client: ion client
 * @param[in] fd: ion fd
 * @param[in] iommu_prot: iommu map Attributes
 *
 * @retval "=0": succeed
 * @retval "!=0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_iommu_map_ion_fd(struct device *dev,
		struct ion_dma_buf_data *dma, struct ion_client *client,
		int32_t fd, int32_t iommu_prot)
{
	int32_t ret;

	if ((dev == NULL) || (dma == NULL) || (client == NULL)) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return -EINVAL;
	}

	ret = do_fd_ion_import_dma_buf(dma, client, fd);
	if (ret != 0) {
		return ret;
	}

	ret = do_fd_dma_buf_get(dma, client, fd);
	if (ret != 0) {
		return ret;
	}

	ret = do_fd_dma_buf_attach(dma, client, dev);
	if (ret != 0) {
		return ret;
	}

	ret = do_fd_dma_buf_map_attachment(dma, client);
	if (ret != 0) {
		return ret;
	}

	ret = do_fd_ion_iovmm_map_dma_buf(dma, client, iommu_prot);
	if (ret != 0) {
		return ret;
	}

	dma->client = client;
	return 0;
}
EXPORT_SYMBOL(ion_iommu_map_ion_fd);/*PRQA S 0307*//*kernel declaration*/

/* code review E1: No need to return value */
/**
 * @NO{S17E08C01I}
 * @ASIL{B}
 * @brief unmap the physical address corresponding to ion fd
 *
 * @param[in] dev: corresponding device
 * @param[in] dma: dma buffer
 *
 * @return None
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
void ion_iommu_unmap_ion_fd(struct device *dev,
		struct ion_dma_buf_data *dma)
{
	if ((dma == NULL) || (dev == NULL) ||
		(dma->attachment == NULL) || (dma->sg_table == NULL) ||
		(dma->ion_handle == NULL) || (dma->dma_buf == NULL) ||
		(dma->client == NULL)) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return;
	}

	ion_iovmm_unmap_dma_buf(dma->attachment, dma->dma_addr);

	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_TO_DEVICE);

	dma_buf_detach(dma->dma_buf, dma->attachment);
	dma_buf_put(dma->dma_buf);
	ion_free(dma->client, dma->ion_handle);
	(void)memset((void *)dma, 0, sizeof(struct ion_dma_buf_data));
}
EXPORT_SYMBOL(ion_iommu_unmap_ion_fd);/*PRQA S 0307*//*kernel declaration*/

/**
 * @NO{S17E08C01U}
 * @ASIL{B}
 * @brief Map the physical address corresponding to the ion handle
 *
 * @param[in] dev: corresponding device
 * @param[in] handle: ion handle
 * @param[in] direction: dma data flow direction
 * @param[in] iommu_prot: iommu map attributes
 *
 * @retval dma_addr_t: succeed
 * @retval other: failed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static dma_addr_t ion_iovmm_map_ion_handle(struct device *dev,
			const struct ion_handle *handle,
			enum dma_data_direction direction,
			int32_t iommu_prot)
{
	struct ion_buffer *buffer =  handle->buffer;
	struct ion_iovm_map *iovm_map;
	struct iommu_domain *domain;

	domain = hobot_iovmm_get_domain_from_dev(dev);
	if (domain == NULL) {
		(void)pr_err("%s: invalid iommu device.\n", __func__);
		return (dma_addr_t)-ENODEV; /* PRQA S 2895 *//*kernel macro*/
	}

	mutex_lock(&buffer->lock);

	iovm_map = ion_buffer_iova_create(buffer, dev,
					direction, iommu_prot);

	if (IS_ERR_OR_NULL((const void *)iovm_map)) {
		(void)pr_err("%s: Failed to create iova.\n", __func__);
		mutex_unlock(&buffer->lock);
		return (dma_addr_t)PTR_ERR((void *)iovm_map);
	}

	list_add_tail(&iovm_map->list, &buffer->iovas);
	mutex_unlock(&buffer->lock);

	return iovm_map->iova;
}

/* code review E1: No need to return value */
/**
 * @NO{S17E08C01U}
 * @ASIL{B}
 * @brief Unmap the dma buffer address corresponding to ion handle
 *
 * @param[in] dev: corresponding device
 * @param[in] handle: ion handle
 * @param[in] iova: IO virtual address
 *
 * @retval "=0": succeed
 * @retval "!=0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static void ion_iovmm_unmap_ion_handle(
	struct device *dev, const struct ion_handle *handle, dma_addr_t iova)
{
	int32_t ret;
	struct ion_iovm_map *this_map = NULL;
	struct ion_iovm_map *iovm_map;
	struct ion_buffer *buffer = handle->buffer;
	struct iommu_domain *domain;

	domain = hobot_iovmm_get_domain_from_dev(dev);
	if (domain == NULL) {
		(void)pr_err("%s: invalid iommu device\n", __func__);
		return;
	}

	mutex_lock(&buffer->lock);
	list_for_each_entry(iovm_map, &buffer->iovas, list) {/*PRQA S 0497*//*pointer manipulation*/
		if ((domain == iovm_map->domain) && (iova == iovm_map->iova)) {
			if (iovm_map->map_cnt > 0U)
				iovm_map->map_cnt--;
			this_map = iovm_map;
			break;
		}
	}

	if (this_map == NULL) {
		(void)pr_warn("%s: IOVA 0x%llx is not found for %s\n",
			__func__, iova, dev_name(dev));
		mutex_unlock(&buffer->lock);
		return;
	}
	if (this_map->map_cnt == 0U) {
		pr_debug("%s: unmap previous %pa for dev %s\n",/*PRQA S 0685, 1294*//*kernel function*/
			__func__, &this_map->iova, dev_name(this_map->dev));
		ret = hobot_iovmm_unmap_sg(this_map->dev, this_map->iova, this_map->size);
		if (ret != 0) {
			(void)pr_err("%s: Failed to unmap previous %pa for dev %s\n",
				__func__, &this_map->iova, dev_name(this_map->dev));
		}
		list_del(&this_map->list);
		kfree((void *)this_map);
	}

	mutex_unlock(&buffer->lock);
}

/**
 * @NO{S17E08C01I}
 * @ASIL{B}
 * @brief Map the physical address corresponding to ion handle
 *
 * @param[in] dev: corresponding device
 * @param[out] dma: Output information of dma buffer
 * @param[in] client: ion client
 * @param[in] handle: ion handle
 * @param[in] iommu_prot: iommu map Attributes
 *
 * @retval "=0": succeed
 * @retval "!=0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_iommu_map_ion_handle(struct device *dev,
		struct ion_dma_buf_data *dma, struct ion_client *client,
		struct ion_handle *handle, int32_t iommu_prot)
{
	if ((dev == NULL) || (dma == NULL) || (client == NULL) || (handle == NULL) ||
		(handle->buffer == NULL)) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return -EINVAL;
	}

	dma->ion_handle = handle;
	dma->sg_table = handle->buffer->sg_table;
	dma->dma_addr = ion_iovmm_map_ion_handle(dev, handle, DMA_TO_DEVICE, iommu_prot);
	if (IS_ERR_VALUE(dma->dma_addr) != 0) {/*PRQA S 2895*//*kernel function*/
		dev_err(dev, "%s: Unable to map handle IOVA for %s, ret = %llu.\n",
			__func__, dev_name(dev), dma->dma_addr);
		return (int32_t)dma->dma_addr;
	}

	dma->client = client;
	return 0;
}
EXPORT_SYMBOL(ion_iommu_map_ion_handle);/*PRQA S 0307*//*kernel declaration*/

/* code review E1: No need to return value */
/**
 * @NO{S17E08C01I}
 * @ASIL{B}
 * @brief unmap the physical address corresponding to ion handle
 *
 * @param[in] dev: corresponding device
 * @param[in] dma: Output information of dma buffer
 *
 * @return None
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
void ion_iommu_unmap_ion_handle(struct device *dev,
		struct ion_dma_buf_data *dma)
{
	if ((dma == NULL) || (dev == NULL) ||
		(dma->sg_table == NULL) || (dma->ion_handle == NULL) ||
		(dma->client == NULL)) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return;
	}

	ion_iovmm_unmap_ion_handle(dev, dma->ion_handle, dma->dma_addr);

	(void)memset((void *)dma, 0, sizeof(struct ion_dma_buf_data));
}
EXPORT_SYMBOL(ion_iommu_unmap_ion_handle);/*PRQA S 0307, 0777*//*kernel declaration*/

/**
 * @NO{S17E08C01I}
 * @ASIL{B}
 * @brief Map physical address
 *
 * @param[in] dev: corresponding device
 * @param[in] pyhs_addr: physical address
 * @param[in] size: map size
 * @param[out] iova: Mapped IO virtual address
 * @param[in] iommu_prot: iommu map Attributes
 *
 * @retval "=0": succeed
 * @retval "!=0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_iommu_map_ion_phys(struct device *dev,
		phys_addr_t pyhs_addr, size_t size, dma_addr_t *iova, int32_t iommu_prot)
{
	int32_t ret = 0;

	if ((dev == NULL) || (iova == NULL)) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return -EINVAL;
	}

	*iova = hobot_iovmm_map_page(dev, pyhs_addr, size,
				DMA_TO_DEVICE, iommu_prot);

	if (IS_ERR_VALUE(*iova) != 0) {/*PRQA S 2895*//*kernel function*/
		dev_err(dev, "%s: Unable to map phys IOVA for %s, ret=%llu.\n",
			__func__, dev_name(dev), *iova);
		return (int32_t)*iova;
	}

	return ret;
}
EXPORT_SYMBOL(ion_iommu_map_ion_phys);/*PRQA S 0307*//*kernel declaration*/

/* code review E1: No need to return value */
/**
 * @NO{S17E08C01I}
 * @ASIL{B}
 * @brief Unmap physical address
 *
 * @param[in] dev: corresponding device
 * @param[out] iova: Mapped IO virtual address
 * @param[in] size: memory size
 *
 * @return None
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
void ion_iommu_unmap_ion_phys(struct device *dev,
		dma_addr_t iova, size_t size)
{
	if (dev == NULL) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return;
	}

	(void)hobot_iovmm_unmap_page(dev, iova, size);
}
EXPORT_SYMBOL(ion_iommu_unmap_ion_phys);/*PRQA S 0307, 0777*//*kernel declaration*/

/**
 * @NO{S17E08C01I}
 * @ASIL{B}
 * @brief reset all the mapping information for device
 *
 * @param[in] dev: the master device structure
 *
 * @return "0":success
 * @return "<0":fail
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_iommu_reset_mapping(struct device *dev) {
	int32_t ret = 0;
	struct iommu_domain *domain;
	struct hobot_iovmm *vmm;
	unsigned long flags;
	struct hobot_vm_region *region, *tmp_region;

	if (dev == NULL) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return -ENODEV;
	}
	vmm = hobot_iommu_get_iovmm(dev);

	if ((vmm == NULL) || (vmm->domain == NULL)) {
		dev_err(dev, "%s: IOVMM/IOVMM domain not found\n", __func__);
		return -ENODEV;
	}
	domain = vmm->domain;
	dev_dbg(dev, "%s: IOVMM: Reset all mapping.\n", __func__);
	spin_lock_irqsave(&vmm->vmlist_lock, flags);/*PRQA S 2996*//*kernel function*/
	list_for_each_entry_safe(region, tmp_region, &vmm->vm_list, list) {/*PRQA S 0497*//*pointer manipulation*/
			ret = iommu_unmap(domain, region->start, region->size);
			list_del(&region->list);
			kfree((void *)region);
	}
	spin_unlock_irqrestore(&vmm->vmlist_lock, flags);/*PRQA S 2996*//*kernel function*/

	return ret;
}
EXPORT_SYMBOL(ion_iommu_reset_mapping);/*PRQA S 0307*//*kernel declaration*/

/**
 * @NO{S17E08C01I}
 * @ASIL{B}
 * @brief save all the mapping information for device
 *
 * @param[in] dev: the master device structure
 *
 * @return "0":success
 * @return "<0":fail
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_iommu_save_mapping(struct device *dev) {
	int32_t ret;
	struct iommu_domain *domain;
	struct hobot_iovmm *vmm;

	if (dev == NULL) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return -ENODEV;
	}
	vmm = hobot_iommu_get_iovmm(dev);

	if ((vmm == NULL) || (vmm->domain == NULL)) {
		dev_err(dev, "%s: IOVMM/IOVMM domain not found\n", __func__);
		return -ENODEV;
	}

	domain = vmm->domain;
	dev_dbg(dev, "%s: IOVMM: save all mapping.\n", __func__);
	ret = hobot_iommu_save_mapping(domain);

	return ret;
}
EXPORT_SYMBOL(ion_iommu_save_mapping);/*PRQA S 0307*//*kernel declaration*/

/**
 * @NO{S17E08C01I}
 * @ASIL{B}
 * @brief restore all the mapping information for device
 *
 * @param[in] dev: the master device structure
 *
 * @return "0":success
 * @return "<0":fail
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_iommu_restore_mapping(struct device *dev) {
	int32_t ret;
	struct iommu_domain *domain;
	struct hobot_iovmm *vmm;

	if (dev == NULL) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return -ENODEV;
	}
	vmm = hobot_iommu_get_iovmm(dev);

	if ((vmm == NULL) || (vmm->domain == NULL)) {
		dev_err(dev, "%s: IOVMM/IOVMM domain not found\n", __func__);
		return -ENODEV;
	}

	domain = vmm->domain;
	dev_dbg(dev, "%s: IOVMM: Restore all mapping.\n", __func__);
	ret = hobot_iommu_restore_mapping(domain);

	return ret;
}
EXPORT_SYMBOL(ion_iommu_restore_mapping);/*PRQA S 0307*//*kernel declaration*/

#else
int32_t ion_iommu_map_ion_fd(struct device *dev,
		struct ion_dma_buf_data *dma, struct ion_client *client,
		int32_t fd, int32_t iommu_prot)
{
	int32_t ret;
	phys_addr_t phys = 0;
	size_t len = 0;

	if ((dev == NULL) || (dma == NULL) || (client == NULL)) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return -EINVAL;
	}

	dma->ion_handle = ion_import_dma_buf(client, fd);
	if (IS_ERR_OR_NULL((void *)dma->ion_handle)) {
		(void)pr_err("%s: ion_import_dma_buf(fd=%d) failed: %ld\n", __func__,
			fd, PTR_ERR((void *)dma->ion_handle));
		return PTR_ERR(dma->ion_handle);
	}

	dma->sg_table = dma->ion_handle->buffer->sg_table;

	ret = ion_phys(client, dma->ion_handle->id, &phys, &len);
	if (ret) {
		(void)pr_err("%s: Get phys failed!\n", __func__);
		return -EINVAL;
	}
	dma->dma_addr = phys;
	dma->client = client;

	return 0;
}
EXPORT_SYMBOL(ion_iommu_map_ion_fd);

/* code review E1: No need to return value */
void ion_iommu_unmap_ion_fd(struct device *dev,
		struct ion_dma_buf_data *dma)
{
	ion_free(dma->client, dma->ion_handle);
}
EXPORT_SYMBOL(ion_iommu_unmap_ion_fd);

int32_t ion_iommu_map_ion_handle(struct device *dev,
		struct ion_dma_buf_data *dma, struct ion_client *client,
		struct ion_handle *handle, int32_t iommu_prot)
{
	phys_addr_t phys = 0;
	size_t len = 0;
	int32_t ret = 0;

	if ((dev == NULL) || (dma == NULL) || (client == NULL) || (handle == NULL) ||
		(handle->buffer == NULL)) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return -EINVAL;
	}
	dma->ion_handle = handle;
	dma->sg_table = handle->buffer->sg_table;

	ret = ion_phys(client, handle->id, &phys, &len);
	if (ret) {
		(void)pr_err("%s: Get phys failed!\n", __func__);
		return -EINVAL;
	}

	dma->dma_addr = phys;
	dma->client = client;

	return 0;
}
EXPORT_SYMBOL(ion_iommu_map_ion_handle);

/* code review E1: No need to return value */
void ion_iommu_unmap_ion_handle(struct device *dev,
		struct ion_dma_buf_data *dma)
{
}
EXPORT_SYMBOL(ion_iommu_unmap_ion_handle);

int32_t ion_iommu_map_ion_phys(struct device *dev,
		phys_addr_t phys_addr, size_t size, dma_addr_t *iova, int32_t iommu_prot)
{
#if defined(CONFIG_DROBOT_LITE_MMU)
	dma_addr_t start;
	phys_addr_t phys, mapped_phys;
	size_t len;
	struct lite_mmu_iommu *iommu = dev_iommu_priv_get(dev);
	struct iommu_domain *domain;
	int32_t ret;

	if (!iommu->domain)
		return -EINVAL;

	domain = iommu->domain;

	phys = phys_addr;
	start = phys & dma_get_mask(dev);
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS]
	len = PAGE_ALIGN(size);
	dev_dbg(dev, "Mapping Phys:%#llx(%#lx)\n", phys, len);
	ret = iommu_map(domain, start, phys, len, iommu_prot);

	if (ret != 0) {
		dev_err(dev, "%s: iommu_map failed w/ err: %d\n", __func__, ret);
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS]
		return -EINVAL;/*PRQA S 2890*//*kernel macro*/
	}

	mapped_phys = iommu_iova_to_phys(domain, start);
	if (mapped_phys != phys) {
		ret = -EINVAL;
		(void)iommu_unmap(domain, start, len);
		dev_err(dev,
			"%s: Failed(%d) to map same IOVMM REGION %pa (SIZE: %#zx, mapped_phys: 0x%llx, phys: 0x%llx)\n",
			__func__, ret, &start, len, mapped_phys, phys);
		return (dma_addr_t)ret;/*PRQA S 2896*//*kernel macro*/
	}
	*iova = start;
#else

	if ((dev == NULL) || (iova == NULL)) {
		(void)pr_err("%s: Invalid input parameters!\n", __func__);
		return -EINVAL;
	}
	*iova = phys_addr;
#endif
	return 0;
}
EXPORT_SYMBOL(ion_iommu_map_ion_phys);

/* code review E1: No need to return value */
void ion_iommu_unmap_ion_phys(struct device *dev,
		dma_addr_t iova, size_t size)
{
}
EXPORT_SYMBOL(ion_iommu_unmap_ion_phys);

int32_t ion_iommu_reset_mapping(struct device *dev)
{
#if defined(CONFIG_DROBOT_LITE_MMU)
	lite_mmu_reset_mapping(dev);
	return 0;
#else
	return -EINVAL;
#endif
}
EXPORT_SYMBOL(ion_iommu_reset_mapping);

int32_t ion_iommu_save_mapping(struct device *dev) {
#if defined(CONFIG_DROBOT_LITE_MMU)
	return 0;
#else
	return -EINVAL;
#endif
}
EXPORT_SYMBOL(ion_iommu_save_mapping);

int32_t ion_iommu_restore_mapping(struct device *dev) {
#if defined(CONFIG_DROBOT_LITE_MMU)
	lite_mmu_restore_mapping(dev);
	return 0;
#else
	return -EINVAL;
#endif
}
EXPORT_SYMBOL(ion_iommu_restore_mapping);

#endif

MODULE_IMPORT_NS(DMA_BUF);
MODULE_DESCRIPTION("IOMMU API for ARM architected SMMUv3 implementations");
MODULE_AUTHOR("jiahui.zhang#horizon.ai");
MODULE_LICENSE("GPL");
