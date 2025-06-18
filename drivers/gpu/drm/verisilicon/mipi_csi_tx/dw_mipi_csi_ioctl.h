/* SPDX-License-Identifier: GPL-2.0 */
/*
 * DW MIPI CSI-TX ioctl public definitions
 *
 */

#ifndef _DW_MIPI_CSI_IOCTL_H_
#define _DW_MIPI_CSI_IOCTL_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define CSI_IOC_MAGIC	 'c'
#define CSI_IOC_INIT	 _IOW(CSI_IOC_MAGIC, 0x01, struct csi_ioctl_cfg)
#define CSI_IOC_SET_MODE _IOW(CSI_IOC_MAGIC, 0x02, struct csi_ioctl_cfg)

#define CSI_NODE_NAME "dw_mipi_csi_ioctl"

/* disp_sys_con registers */
#define DISP_BT1120_2_CSITX_EN	     0x18U
#define BT1120_2_CSITX_EN	     BIT(0)
#define DISP_CSITX_DPI_CFG	     0x0CU
#define CSI_DPI_DATA_FORMAT	     GENMASK(2, 0)
#define CSI_DPI_VSYNC_POLARITY	     BIT(5)
#define CSI_DPI_VSYNC_POLARITY_SHIFT 5
#define CSI_DPI_HSYNC_POLARITY	     BIT(4)
#define CSI_DPI_HSYNC_POLARITY_SHIFT 4
#define CSI_DPI_DE_POLARITY	     BIT(3)

struct csi_ioctl_cfg {
	__u32 lanes;
	__u32 bus_fmt;
	struct csi_mode_info mode;
	__u8 clk_continuous;
	__u32 clk_mult;
	__u8 margin_en;
	__u8 dphy_override_en;
	__u8 bt1120_enable;
};

void dw_csi_ioctl_destroy(struct dw_mipi_csi *csi);
int dw_csi_ioctl_create(struct dw_mipi_csi *csi);

#endif /* _DW_MIPI_CSI_IOCTL_H_ */
