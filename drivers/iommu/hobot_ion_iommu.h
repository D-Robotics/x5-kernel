/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_ION_IOMMU_H
#define HOBOT_ION_IOMMU_H

#include <linux/dma-buf.h>
#include <linux/iommu.h>
#include <linux/ion.h>
#ifdef CONFIG_HOBOT_IOVMM
#include "hobot_iovmm.h"
#endif

#if defined(CONFIG_DROBOT_LITE_MMU)
#include "drobot-lite-mmu.h"
#endif /* CONFIG_DROBOT_LITE_MMU */

/**
 * @file hobot_ion_iommu.h
 *
 * @NO{S17E08C01I}
 * @ASIL{B}
 */

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
		int32_t fd, int32_t iommu_prot);

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
		struct ion_dma_buf_data *dma);

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
		struct ion_handle *handle, int32_t iommu_prot);

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
		struct ion_dma_buf_data *dma);

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
		phys_addr_t pyhs_addr, size_t size, dma_addr_t *iova, int32_t iommu_prot);

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
		dma_addr_t iova, size_t size);

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
int32_t ion_iommu_reset_mapping(struct device *dev);

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
int32_t ion_iommu_save_mapping(struct device *dev);

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
int32_t ion_iommu_restore_mapping(struct device *dev);

/**
 * @NO{S17E08C01I}
 * @ASIL{B}
 * @brief change the ste status from enable to bypass or from bypass to enable
 *
 * @param[in] dev: the master device structure
 * @param[in] flasg: the change flags, 0: bypass, 1: enable
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
int32_t hobot_smmu_ste_status_change(struct device *dev, uint32_t flags);

/**
 * @NO{S17E08C01I}
 * @ASIL{B}
 * @brief soft reset the smmu
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
int32_t hobot_smmu_soft_reset(struct device *dev);

#endif/* HOBOT_ION_IOMMU*/
