/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __DW_MIPI_CSI_DEBUGFS_H_
#define __DW_MIPI_CSI_DEBUGFS_H_

#ifdef CONFIG_DEBUG_FS
void dw_mipi_csi_debugfs_init(struct dw_mipi_csi *csi);
void dw_mipi_csi_debugfs_fini(struct dw_mipi_csi *csi);
#else
static inline void dw_mipi_csi_debugfs_init(struct dw_mipi_csi *csi) {}

static inline void dw_mipi_csi_debugfs_fini(struct dw_mipi_csi *csi) {}
#endif

#endif /* __DW_MIPI_CSI_DEBUGFS_H_ */
