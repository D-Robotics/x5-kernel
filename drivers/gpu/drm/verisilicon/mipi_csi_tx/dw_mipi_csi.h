/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Common header for DW MIPI CSI driver
 */

#ifndef __DW_MIPI_CSI_H__
#define __DW_MIPI_CSI_H__

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/phy/phy.h>
#include <drm/drm_connector.h>
#include <drm/drm_bridge.h>

#define CSI_HSCLK_MIN_KHZ 40000U
#define CSI_HSCLK_MAX_KHZ 2500000U

enum dw_ipi_mode {
	IPI_STORE_FORWARD_MODE = 0,
	IPI_CUT_THROUGH_MODE,
	IPI_BACK_PRESSURE_MODE,
	IPI_MODE_MAX,
};

enum dw_vpg_mode {
	CSI_VPG_OFF = 0,
	CSI_VPG_ON,
	CSI_VPG_MAX,
};

enum dw_frame_num_mode {
	IPI_FRAME_ZERO = 0,
	IPI_FRAME_INCR_ONE,
	IPI_FRAME_MODE_MAX,
};

enum dw_line_num_mode {
	IPI_LINE_ZERO = 0,
	IPI_LINE_INCR_ONE,
	IPI_LINE_INCR_ANY,
	IPI_LINE_MODE_MAX,
};

struct dw_ipi_pkt_conf {
	enum dw_ipi_mode ipi_mode;
	u32 vc;
	enum dw_frame_num_mode frame_num_mode;
	u32 frame_max;
	enum dw_line_num_mode line_num_mode;
	u32 line_start;
	u32 line_step;
	u32 send_start;
	bool hsync_pkt_en;
};

enum dw_mipi_csi_pattern {
	DW_MIPI_CSI_PATTERN_OFF = 0,
	DW_MIPI_CSI_PATTERN_H,
	DW_MIPI_CSI_PATTERN_V,
};

struct csi_mode_info {
	u32 width;
	u32 height;
	u32 pixel_clock; // kHz
	u16 hdisplay;
	u16 hsync_start;
	u16 hsync_end;
	u16 htotal;
	u16 hskew;
	u16 vdisplay;
	u16 vsync_start;
	u16 vsync_end;
	u16 vtotal;
	u32 refresh_rate; // Hz
	bool interlaced;
};

enum dw_bus_fmt {
	DW_BUS_FMT_RGB565_1 = 0,
	DW_BUS_FMT_RGB565_2,
	DW_BUS_FMT_RGB565_3,
	DW_BUS_FMT_RGB666_PACKED,
	DW_BUS_FMT_RGB666,
	DW_BUS_FMT_RGB888,
	DW_BUS_FMT_YUV420,
	DW_BUS_FMT_YUV422,
	DW_BUS_FMT_RAW8,
	DW_BUS_FMT_RAW10,
	DW_BUS_FMT_COUNT,
};

/**
 * struct dw_mipi_csi - main context for the CSI IP
 * @dev: device pointer
 * @base: memory mapped register base
 * @pclk: optional clock
 * @dphy: optional MIPI D-PHY
 * @bridge: drm bridge object
 * @connector: drm connector
 * @lanes: number of data lanes
 * @bus_fmt: bus format, for example 24-bit RGB
 * @mode: cached drm display mode
 */
struct dw_mipi_csi {
	struct device *dev;
	void __iomem *base;
	struct clk *pclk;
	struct phy *dphy;
	struct device_link *phy_link;

	struct drm_bridge bridge;

	struct drm_bridge *panel_bridge;
	struct drm_connector connector;

	struct dw_ipi_pkt_conf pkt_conf;

	u32 lanes;
	u32 bus_fmt;

	struct phy_configure_opts_mipi_dphy dphy_cfg;
	unsigned int lane_link_rate;

	struct csi_mode_info mode;
	struct csi_mode_info vpg_mode;

	/* 1: continuous clock, 0: non-continuous */
	u8 clk_continuous;
	/* multiplier after override */
	u32 clk_multiplier;

	/* HS‑CLK override in kHz, 0 == auto */
	//debug only
	u32 hsclk_override_khz;

	/* 0=auto, 1=cut-through, 2=store-forward */
	//debug only
	u8 ipi_mode_override;

	/* timing overrides, 0 == the value to write */
	u32 override_lpx;
	u32 override_hs_prepare;
	u32 override_hs_zero;
	u32 override_hs_trail;
	u32 override_hs_exit;
	u32 override_clk_prepare;
	u32 override_clk_zero;
	u32 override_clk_trail;
	u32 override_clk_post;

	/* when true, apply the above override_* even if they are zero */
	u8 dphy_timing_override_enable;

	/* Redundant clock scaling enabled: 1 for on, 0 for off */
	u8 timing_margin_enable;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dir;
	struct dentry *pattern_file;
#ifdef CONFIG_HOBOT_CSI_IDI
	struct dentry *bypass_file;
#endif
	struct dentry *bus_fmt_file;
	struct dentry *lanes_file;
	struct dentry *info_file;
#endif
};

/**
 * Helper macro to get the csi pointer from a bridge
 */
static inline struct dw_mipi_csi *bridge_to_csi(struct drm_bridge *bridge)
{
	return container_of(bridge, struct dw_mipi_csi, bridge);
}

/**
 * Helper macro to get csi pointer from a connector
 */
static inline struct dw_mipi_csi *connector_to_csi(struct drm_connector *connector)
{
	return container_of(connector, struct dw_mipi_csi, connector);
}

extern struct platform_driver dw_mipi_csi_driver;

#endif /* __DW_MIPI_CSI_H__ */
