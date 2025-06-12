/* SPDX-License-Identifier: GPL-2.0 */
/*
 * DRM Bridge interface for DW MIPI CSI
 */

#ifndef __DW_MIPI_CSI_DRM_H__
#define __DW_MIPI_CSI_DRM_H__

struct dw_mipi_csi;

/**
 * dw_mipi_csi_drm_register - Registers the CSI device as a DRM bridge
 * @csi: pointer to the CSI context
 *
 * Returns 0 on success, otherwise negative error code.
 */
int dw_mipi_csi_drm_register(struct dw_mipi_csi *csi);

/**
 * dw_mipi_csi_drm_unregister - Unregisters the CSI device from DRM
 * @csi: pointer to the CSI context
 */
void dw_mipi_csi_drm_unregister(struct dw_mipi_csi *csi);

int dw_mipi_csi_drm_bind(struct dw_mipi_csi *csi);

#endif /* __DW_MIPI_CSI_DRM_H__ */
