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

#include <linux/component.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/phy/phy.h>
#include <linux/iopoll.h>
#include <linux/phy.h>
#include <linux/phy/phy-mipi-dphy.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>

#include <drm/drm_bridge.h>
#include <drm/drm_encoder.h>
#include <drm/drm_modes.h>
#include <drm/drm_of.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_print.h>
#include <drm/drm_panel.h>

#include "dw_mipi_dsi.h"

#define DSI_VERSION 0x00

#define DSI_PWR_UP 0x04
#define RESET	   0
#define POWERUP	   BIT(0)

#define DSI_CLKMGR_CFG		 0x08
#define TO_CLK_DIVISION(div)	 (((div) & 0xff) << 8)
#define TX_ESC_CLK_DIVISION(div) ((div) & 0xff)

#define DSI_DPI_VCID   0x0c
#define DPI_VCID(vcid) ((vcid) & 0x3)

#define DSI_DPI_COLOR_CODING	 0x10
#define LOOSELY18_EN		 BIT(8)
#define DPI_COLOR_CODING_16BIT_1 0x0
#define DPI_COLOR_CODING_16BIT_2 0x1
#define DPI_COLOR_CODING_16BIT_3 0x2
#define DPI_COLOR_CODING_18BIT_1 0x3
#define DPI_COLOR_CODING_18BIT_2 0x4
#define DPI_COLOR_CODING_24BIT	 0x5

#define DSI_DPI_CFG_POL	  0x14
#define COLORM_ACTIVE_LOW BIT(4)
#define SHUTD_ACTIVE_LOW  BIT(3)
#define HSYNC_ACTIVE_LOW  BIT(2)
#define VSYNC_ACTIVE_LOW  BIT(1)
#define DATAEN_ACTIVE_LOW BIT(0)

#define DSI_DPI_LP_CMD_TIM    0x18
#define OUTVACT_LPCMD_TIME(p) (((p) & 0xff) << 16)
#define INVACT_LPCMD_TIME(p)  ((p) & 0xff)

#define DSI_DBI_VCID		0x1c
#define DSI_DBI_CFG		0x20
#define DSI_DBI_PARTITIONING_EN 0x24
#define DSI_DBI_CMDSIZE		0x28

#define DSI_PCKHDL_CFG 0x2c
#define EOTP_TX_LP_EN  BIT(5)
#define CRC_RX_EN      BIT(4)
#define ECC_RX_EN      BIT(3)
#define BTA_EN	       BIT(2)
#define EOTP_RX_EN     BIT(1)
#define EOTP_TX_EN     BIT(0)

#define DSI_GEN_VCID 0x30

#define DSI_MODE_CFG	  0x34
#define ENABLE_VIDEO_MODE 0
#define ENABLE_CMD_MODE	  BIT(0)

#define DSI_VID_MODE_CFG		    0x38
#define VPG_ORIENTATION			    BIT(24)
#define VPG_EN				    BIT(16)
#define ENABLE_LOW_POWER		    (0x3f << 8)
#define ENABLE_LOW_POWER_MASK		    (0x3f << 8)
#define VID_MODE_TYPE_NON_BURST_SYNC_PULSES 0x0
#define VID_MODE_TYPE_NON_BURST_SYNC_EVENTS 0x1
#define VID_MODE_TYPE_BURST		    0x2
#define VID_MODE_TYPE_MASK		    0x3

#define DSI_VID_PKT_SIZE 0x3c
#define VID_PKT_SIZE(p)	 ((p) & 0x3fff)

#define DSI_VID_NUM_CHUNKS 0x40
#define VID_NUM_CHUNKS(c)  ((c) & 0x1fff)

#define DSI_VID_NULL_SIZE 0x44
#define VID_NULL_SIZE(b)  ((b) & 0x1fff)

#define DSI_VID_HSA_TIME      0x48
#define DSI_VID_HBP_TIME      0x4c
#define DSI_VID_HLINE_TIME    0x50
#define DSI_VID_VSA_LINES     0x54
#define DSI_VID_VBP_LINES     0x58
#define DSI_VID_VFP_LINES     0x5c
#define DSI_VID_VACTIVE_LINES 0x60
#define DSI_EDPI_CMD_SIZE     0x64

#define DSI_CMD_MODE_CFG   0x68
#define MAX_RD_PKT_SIZE_LP BIT(24)
#define DCS_LW_TX_LP	   BIT(19)
#define DCS_SR_0P_TX_LP	   BIT(18)
#define DCS_SW_1P_TX_LP	   BIT(17)
#define DCS_SW_0P_TX_LP	   BIT(16)
#define GEN_LW_TX_LP	   BIT(14)
#define GEN_SR_2P_TX_LP	   BIT(13)
#define GEN_SR_1P_TX_LP	   BIT(12)
#define GEN_SR_0P_TX_LP	   BIT(11)
#define GEN_SW_2P_TX_LP	   BIT(10)
#define GEN_SW_1P_TX_LP	   BIT(9)
#define GEN_SW_0P_TX_LP	   BIT(8)
#define ACK_RQST_EN	   BIT(1)
#define TEAR_FX_EN	   BIT(0)

#define CMD_MODE_ALL_LP                                                                            \
	(MAX_RD_PKT_SIZE_LP | DCS_LW_TX_LP | DCS_SR_0P_TX_LP | DCS_SW_1P_TX_LP | DCS_SW_0P_TX_LP | \
	 GEN_LW_TX_LP | GEN_SR_2P_TX_LP | GEN_SR_1P_TX_LP | GEN_SR_0P_TX_LP | GEN_SW_2P_TX_LP |    \
	 GEN_SW_1P_TX_LP | GEN_SW_0P_TX_LP)

#define DSI_GEN_HDR	 0x6c
#define DSI_GEN_PLD_DATA 0x70

#define DSI_CMD_PKT_STATUS 0x74
#define GEN_RD_CMD_BUSY	   BIT(6)
#define GEN_PLD_R_FULL	   BIT(5)
#define GEN_PLD_R_EMPTY	   BIT(4)
#define GEN_PLD_W_FULL	   BIT(3)
#define GEN_PLD_W_EMPTY	   BIT(2)
#define GEN_CMD_FULL	   BIT(1)
#define GEN_CMD_EMPTY	   BIT(0)

#define DSI_TO_CNT_CFG 0x78
#define HSTX_TO_CNT(p) (((p) & 0xffff) << 16)
#define LPRX_TO_CNT(p) ((p) & 0xffff)

#define DSI_HS_RD_TO_CNT 0x7c
#define DSI_LP_RD_TO_CNT 0x80
#define DSI_HS_WR_TO_CNT 0x84
#define DSI_LP_WR_TO_CNT 0x88
#define DSI_BTA_TO_CNT	 0x8c

#define DSI_LPCLK_CTRL	   0x94
#define AUTO_CLKLANE_CTRL  BIT(1)
#define PHY_TXREQUESTCLKHS BIT(0)

#define DSI_PHY_TMR_LPCLK_CFG	0x98
#define PHY_CLKHS2LP_TIME(lbcc) (((lbcc) & 0x3ff) << 16)
#define PHY_CLKLP2HS_TIME(lbcc) ((lbcc) & 0x3ff)

#define DSI_PHY_TMR_CFG	     0x9c
#define PHY_HS2LP_TIME(lbcc) (((lbcc) & 0x3ff) << 16)
#define PHY_LP2HS_TIME(lbcc) ((lbcc) & 0x3ff)

#define DSI_PHY_RSTZ	0xa0
#define PHY_DISFORCEPLL 0
#define PHY_ENFORCEPLL	BIT(3)
#define PHY_DISABLECLK	0
#define PHY_ENABLECLK	BIT(2)
#define PHY_RSTZ	0
#define PHY_UNRSTZ	BIT(1)
#define PHY_SHUTDOWNZ	0
#define PHY_UNSHUTDOWNZ BIT(0)

#define DSI_PHY_IF_CFG		  0xa4
#define PHY_STOP_WAIT_TIME(cycle) (((cycle) & 0xff) << 8)
#define N_LANES(n)		  (((n)-1) & 0x3)

#define DSI_PHY_ULPS_CTRL   0xa8
#define DSI_PHY_TX_TRIGGERS 0xac

#define DSI_PHY_STATUS		0xb0
#define PHY_STOP_STATE_CLK_LANE BIT(2)
#define PHY_LOCK		BIT(0)

#define DSI_INT_ST0  0xbc
#define DSI_INT_ST1  0xc0
#define DSI_INT_MSK0 0xc4
#define DSI_INT_MSK1 0xc8

#define DSI_PHY_TMR_RD_CFG 0xf4
#define MAX_RD_TIME(lbcc)  ((lbcc) & 0x7fff)

#define TO_CLK_DIV_BURST     10
#define TO_CLK_DIV_NON_BURST 100

#define PHY_STATUS_TIMEOUT_US	  10000
#define CMD_PKT_STATUS_TIMEOUT_US 20000

#define MAX_LANE_COUNT 4

struct dw_mipi_dsi;

struct dw_mipi_dsi_funcs {
	struct dw_mipi_dsi *(*get_dsi)(struct device *dev);
	int (*bind)(struct dw_mipi_dsi *dsi);
	void (*unbind)(struct dw_mipi_dsi *dsi);
};

struct dw_mipi_dsi {
	struct device *dev;
	void __iomem *base;
	struct clk *pclk;
	struct phy *dphy;

	struct device_link *phy_link;

	struct mipi_dsi_host host;
	unsigned int channel;
	unsigned int lanes;
	enum mipi_dsi_pixel_format format;
	unsigned long mode_flags;

	unsigned int lane_link_rate; /* kHz */

	const struct dw_mipi_dsi_funcs *funcs;
	struct phy_configure_opts_mipi_dphy dphy_cfg;
};

struct dw_mipi_dsi_primary {
	struct dw_mipi_dsi dsi;
	struct dw_mipi_dsi *secondary_dsi;

	struct drm_bridge bridge;
	struct drm_bridge *panel_bridge;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dir;
	struct dentry *debugfs_file;
#endif
};

enum dw_mipi_dsi_pattern {
	DW_MIPI_DSI_PATTERN_OFF = 0,
	DW_MIPI_DSI_PATTERN_H,
	DW_MIPI_DSI_PATTERN_V,
};

#ifdef CONFIG_DEBUG_FS
static const char *const pattern_string[] = {"OFF", "H", "V"};
#endif

static inline struct dw_mipi_dsi *host_to_dsi(struct mipi_dsi_host *host)
{
	return container_of(host, struct dw_mipi_dsi, host);
}

static inline struct dw_mipi_dsi_primary *dsi_to_primary(struct dw_mipi_dsi *dsi)
{
	return container_of(dsi, struct dw_mipi_dsi_primary, dsi);
}

static inline struct dw_mipi_dsi_primary *bridge_to_primary(struct drm_bridge *bridge)
{
	return container_of(bridge, struct dw_mipi_dsi_primary, bridge);
}

static inline void dw_write(struct dw_mipi_dsi *dsi, u32 reg, u32 val)
{
	writel(val, dsi->base + reg);
}

static inline u32 dw_read(struct dw_mipi_dsi *dsi, u32 reg)
{
	return readl(dsi->base + reg);
}

static void dw_mipi_dsi_video_mode_config(struct dw_mipi_dsi *dsi)
{
	u32 val;

	val = ENABLE_LOW_POWER;

	/* prefer burst mode */
	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
		val |= VID_MODE_TYPE_BURST;
	else if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
		val |= VID_MODE_TYPE_NON_BURST_SYNC_PULSES;
	else
		val |= VID_MODE_TYPE_NON_BURST_SYNC_EVENTS;

	dw_write(dsi, DSI_VID_MODE_CFG, val);
}

static int dsi_host_attach(struct mipi_dsi_host *host, struct mipi_dsi_device *device)
{
	struct dw_mipi_dsi *dsi = host_to_dsi(host);

	if (device->lanes > MAX_LANE_COUNT) {
		DRM_ERROR("the number of data lanes(%u) is too many\n", device->lanes);
		return -EINVAL;
	}

	dsi->lanes	= device->lanes;
	dsi->channel	= device->channel;
	dsi->format	= device->format;
	dsi->mode_flags = device->mode_flags;

	return 0;
}

static int dsi_host_detach(struct mipi_dsi_host *host, struct mipi_dsi_device *device)
{
	return 0;
}

static void dw_mipi_message_config(struct dw_mipi_dsi *dsi, const struct mipi_dsi_msg *msg)
{
	bool lpm = msg->flags & MIPI_DSI_MSG_USE_LPM;
	u32 val	 = 0;

	if (msg->flags & MIPI_DSI_MSG_REQ_ACK)
		val |= ACK_RQST_EN;
	if (lpm)
		val |= CMD_MODE_ALL_LP;

	dw_write(dsi, DSI_LPCLK_CTRL, lpm ? 0 : PHY_TXREQUESTCLKHS);
	dw_write(dsi, DSI_CMD_MODE_CFG, val);
}

static int dw_mipi_dsi_gen_pkt_hdr_write(struct dw_mipi_dsi *dsi, u32 hdr_val)
{
	int ret;
	u32 val, mask;

	ret = readl_poll_timeout(dsi->base + DSI_CMD_PKT_STATUS, val, !(val & GEN_CMD_FULL), 1000,
				 CMD_PKT_STATUS_TIMEOUT_US);
	if (ret) {
		DRM_ERROR("failed to get available command FIFO\n");
		return ret;
	}

	dw_write(dsi, DSI_GEN_HDR, hdr_val);

	mask = GEN_CMD_EMPTY | GEN_PLD_W_EMPTY;
	ret  = readl_poll_timeout(dsi->base + DSI_CMD_PKT_STATUS, val, (val & mask) == mask, 1000,
				  CMD_PKT_STATUS_TIMEOUT_US);
	if (ret) {
		DRM_ERROR("failed to write command FIFO\n");
		return ret;
	}

	return 0;
}

static int dw_mipi_dsi_write(struct dw_mipi_dsi *dsi, const struct mipi_dsi_packet *packet)
{
	const u8 *tx_buf = packet->payload;
	int len = packet->payload_length, pld_data_bytes = sizeof(u32), ret;
	__le32 word;
	u32 val;

	while (len) {
		if (len < pld_data_bytes) {
			word = 0;
			memcpy(&word, tx_buf, len);
			dw_write(dsi, DSI_GEN_PLD_DATA, le32_to_cpu(word));
			len = 0;
		} else {
			memcpy(&word, tx_buf, pld_data_bytes);
			dw_write(dsi, DSI_GEN_PLD_DATA, le32_to_cpu(word));
			tx_buf += pld_data_bytes;
			len -= pld_data_bytes;
		}

		ret = readl_poll_timeout(dsi->base + DSI_CMD_PKT_STATUS, val,
					 !(val & GEN_PLD_W_FULL), 1000, CMD_PKT_STATUS_TIMEOUT_US);
		if (ret) {
			DRM_ERROR("failed to get write payload FIFO\n");
			return ret;
		}
	}

	word = 0;
	memcpy(&word, packet->header, sizeof(packet->header));
	return dw_mipi_dsi_gen_pkt_hdr_write(dsi, le32_to_cpu(word));
}

static int dw_mipi_dsi_read(struct dw_mipi_dsi *dsi, const struct mipi_dsi_msg *msg)
{
	int i, j, ret, len = msg->rx_len;
	u8 *buf = msg->rx_buf;
	u32 val;

	/* Wait end of the read operation */
	ret = readl_poll_timeout(dsi->base + DSI_CMD_PKT_STATUS, val, !(val & GEN_RD_CMD_BUSY),
				 1000, CMD_PKT_STATUS_TIMEOUT_US);
	if (ret) {
		DRM_ERROR("Timeout during read operation\n");
		return ret;
	}

	for (i = 0; i < len; i += 4) {
		/* Read fifo must not be empty before all bytes are read */
		ret = readl_poll_timeout(dsi->base + DSI_CMD_PKT_STATUS, val,
					 !(val & GEN_PLD_R_EMPTY), 1000, CMD_PKT_STATUS_TIMEOUT_US);
		if (ret) {
			DRM_ERROR("Read payload FIFO is empty\n");
			return ret;
		}

		val = dw_read(dsi, DSI_GEN_PLD_DATA);
		for (j = 0; j < 4 && j + i < len; j++)
			buf[i + j] = val >> (8 * j);
	}

	return ret;
}

static ssize_t dsi_host_transfer(struct mipi_dsi_host *host, const struct mipi_dsi_msg *msg)
{
	struct dw_mipi_dsi *dsi = host_to_dsi(host);
	struct mipi_dsi_packet packet;
	int ret, nb_bytes;

	ret = mipi_dsi_create_packet(&packet, msg);
	if (ret) {
		DRM_ERROR("failed to create packet: %d\n", ret);
		return ret;
	}

	dw_mipi_message_config(dsi, msg);

	ret = dw_mipi_dsi_write(dsi, &packet);
	if (ret)
		return ret;

	if (msg->rx_buf && msg->rx_len) {
		ret = dw_mipi_dsi_read(dsi, msg);
		if (ret)
			return ret;
		nb_bytes = msg->rx_len;
	} else {
		nb_bytes = packet.size;
	}

	return nb_bytes;
}

static const struct mipi_dsi_host_ops dw_mipi_dsi_host_ops = {
	.attach	  = dsi_host_attach,
	.detach	  = dsi_host_detach,
	.transfer = dsi_host_transfer,
};

static void dw_mipi_dsi_set_mode(struct dw_mipi_dsi *dsi, unsigned long mode_flags)
{
	u32 val;

	dw_write(dsi, DSI_PWR_UP, RESET);

	if (mode_flags & MIPI_DSI_MODE_VIDEO) {
		dw_write(dsi, DSI_MODE_CFG, ENABLE_VIDEO_MODE);

		val = PHY_TXREQUESTCLKHS;
		if (dsi->mode_flags & MIPI_DSI_CLOCK_NON_CONTINUOUS)
			val |= AUTO_CLKLANE_CTRL;
		dw_write(dsi, DSI_LPCLK_CTRL, val);
	} else {
		dw_write(dsi, DSI_MODE_CFG, ENABLE_CMD_MODE);
	}

	dw_write(dsi, DSI_PWR_UP, POWERUP);
}

static void dw_mipi_dsi_init(struct dw_mipi_dsi *dsi)
{
	u8 esc_clk;
	u32 esc_clk_division;
	u32 cfg;

	/* limit esc clk on FPGA */
	if (!dsi->pclk)
		esc_clk = 5; /* 5MHz */
	else
		esc_clk = 20; /* 20MHz */

	/*
	 * The maximum permitted escape clock is 20MHz and it is derived from
	 * lanebyteclk, which is running at "lane_link_rate / 8".  Thus we want:
	 *
	 *     (lane_link_rate >> 3) / esc_clk_division < 20
	 * which is:
	 *     (lane_link_rate >> 3) / 20 > esc_clk_division
	 */
	esc_clk_division = ((dsi->lane_link_rate / 1000) >> 3) / esc_clk + 1;

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
		cfg = TO_CLK_DIVISION(TO_CLK_DIV_BURST) | TX_ESC_CLK_DIVISION(esc_clk_division);
	else
		cfg = TO_CLK_DIVISION(TO_CLK_DIV_NON_BURST) | TX_ESC_CLK_DIVISION(esc_clk_division);

	dw_write(dsi, DSI_PWR_UP, RESET);

	dw_write(dsi, DSI_CLKMGR_CFG, cfg);
}

static void dw_mipi_dsi_dpi_config(struct dw_mipi_dsi *dsi, const struct drm_display_mode *mode)
{
	u32 val = 0, color = 0;

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB888:
		color = DPI_COLOR_CODING_24BIT;
		break;
	case MIPI_DSI_FMT_RGB666:
		color = DPI_COLOR_CODING_18BIT_2 | LOOSELY18_EN;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		color = DPI_COLOR_CODING_18BIT_1;
		break;
	case MIPI_DSI_FMT_RGB565:
		color = DPI_COLOR_CODING_16BIT_1;
		break;
	}

	if (!(mode->flags & DRM_MODE_FLAG_PVSYNC))
		val |= VSYNC_ACTIVE_LOW;
	if (!(mode->flags & DRM_MODE_FLAG_PHSYNC))
		val |= HSYNC_ACTIVE_LOW;

	dw_write(dsi, DSI_DPI_VCID, DPI_VCID(dsi->channel));
	dw_write(dsi, DSI_DPI_COLOR_CODING, color);
	dw_write(dsi, DSI_DPI_CFG_POL, val);
	/*
	 * TODO dw drv improvements
	 * largest packet sizes during hfp or during vsa/vpb/vfp
	 * should be computed according to byte lane, lane number and only
	 * if sending lp cmds in high speed is enable (PHY_TXREQUESTCLKHS)
	 */
	dw_write(dsi, DSI_DPI_LP_CMD_TIM, OUTVACT_LPCMD_TIME(4) | INVACT_LPCMD_TIME(4));
}

static void dw_mipi_dsi_command_mode_config(struct dw_mipi_dsi *dsi,
					    const struct drm_display_mode *mode)
{
	u32 hstx_to;

	hstx_to = drm_mode_vrefresh(mode);
	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
		hstx_to *= mode->vtotal * TO_CLK_DIV_BURST;
	else
		hstx_to *= TO_CLK_DIV_NON_BURST;

	hstx_to = dsi->lane_link_rate * 1000 / 8 / hstx_to;
	hstx_to += hstx_to / 10; /* add 10% margin */

	dw_write(dsi, DSI_TO_CNT_CFG, HSTX_TO_CNT(hstx_to) | LPRX_TO_CNT(1000));

	/*
	 * TODO dw drv improvements
	 * the Bus-Turn-Around Timeout Counter should be computed
	 * according to byte lane...
	 */
	dw_write(dsi, DSI_BTA_TO_CNT, 0xd00);
}

/* Get lane byte clock cycles. */
static u32 dw_mipi_dsi_get_hcomponent_lbcc(struct dw_mipi_dsi *dsi,
					   const struct drm_display_mode *mode, u32 hcomponent)
{
	u32 frac, lbcc;

	lbcc = hcomponent * dsi->lane_link_rate / 8;

	frac = lbcc % mode->clock;
	lbcc = lbcc / mode->clock;
	if (frac)
		lbcc++;

	return lbcc;
}

static void dw_mipi_dsi_video_packet_config(struct dw_mipi_dsi *dsi,
					    const struct drm_display_mode *mode)
{
	u32 vid_packet_size = mode->hdisplay;
	int null_packet_size, chunks = 1;

	if (!(dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)) {
		int bpp = mipi_dsi_pixel_format_to_bpp(dsi->format);

		chunks = 0;

		do {
			chunks++;

			null_packet_size =
				(dsi->lane_link_rate * dsi->lanes / 8 / mode->clock - bpp / 8) *
				mode->hdisplay / chunks;
		} while (null_packet_size != VID_NULL_SIZE(null_packet_size) &&
			 null_packet_size > 0);

		dw_write(dsi, DSI_VID_NULL_SIZE,
			 VID_NULL_SIZE(null_packet_size > 12 ? null_packet_size - 12 : 0));
		dw_write(dsi, DSI_VID_NUM_CHUNKS, VID_NUM_CHUNKS(chunks));
	}

	dw_write(dsi, DSI_VID_PKT_SIZE, VID_PKT_SIZE(vid_packet_size / chunks));
}

static void dw_mipi_dsi_timing_config(struct dw_mipi_dsi *dsi, const struct drm_display_mode *mode)
{
	u32 htotal, hsa, hbp, lbcc;

	htotal = mode->htotal;
	hsa    = mode->hsync_end - mode->hsync_start;
	hbp    = mode->htotal - mode->hsync_end;

	lbcc = dw_mipi_dsi_get_hcomponent_lbcc(dsi, mode, htotal);
	dw_write(dsi, DSI_VID_HLINE_TIME, lbcc);

	lbcc = dw_mipi_dsi_get_hcomponent_lbcc(dsi, mode, hsa);
	dw_write(dsi, DSI_VID_HSA_TIME, lbcc);

	lbcc = dw_mipi_dsi_get_hcomponent_lbcc(dsi, mode, hbp);
	dw_write(dsi, DSI_VID_HBP_TIME, lbcc);

	/* vertical */
	dw_write(dsi, DSI_VID_VACTIVE_LINES, mode->vdisplay);
	dw_write(dsi, DSI_VID_VSA_LINES, mode->vsync_end - mode->vsync_start);
	dw_write(dsi, DSI_VID_VFP_LINES, mode->vsync_start - mode->vdisplay);
	dw_write(dsi, DSI_VID_VBP_LINES, mode->vtotal - mode->vsync_end);
}

/* Get lane byte clock cycles from picosecond */
static inline unsigned int ps_to_lbcc(unsigned long long hs_clk_rate, unsigned int ps)
{
	unsigned long long ui, lbcc;

	ui = ALIGN(PSEC_PER_SEC, hs_clk_rate);
	do_div(ui, hs_clk_rate);

	lbcc = ps / 8;
	do_div(lbcc, ui);

	return lbcc;
}

static unsigned int dphy_clk_lp2hs_time(struct phy_configure_opts_mipi_dphy *cfg)
{
	return ps_to_lbcc(cfg->hs_clk_rate, cfg->lpx + cfg->clk_prepare + cfg->clk_zero);
}

static unsigned int dphy_clk_hs2lp_time(struct phy_configure_opts_mipi_dphy *cfg)
{
	return ps_to_lbcc(cfg->hs_clk_rate, cfg->clk_trail);
}

static unsigned int dphy_data_lp2hs_time(struct phy_configure_opts_mipi_dphy *cfg)
{
	return ps_to_lbcc(cfg->hs_clk_rate, cfg->lpx + cfg->hs_prepare + cfg->hs_zero);
}

static unsigned int dphy_data_hs2lp_time(struct phy_configure_opts_mipi_dphy *cfg)
{
	return ps_to_lbcc(cfg->hs_clk_rate, cfg->hs_trail);
}

static void dw_mipi_dsi_dphy_timing_config(struct dw_mipi_dsi *dsi)
{
	dw_write(dsi, DSI_PHY_TMR_CFG,
		 PHY_HS2LP_TIME(dphy_data_hs2lp_time(&dsi->dphy_cfg)) |
			 PHY_LP2HS_TIME(dphy_data_lp2hs_time(&dsi->dphy_cfg)));

	dw_write(dsi, DSI_PHY_TMR_LPCLK_CFG,
		 PHY_CLKHS2LP_TIME(dphy_clk_hs2lp_time(&dsi->dphy_cfg)) |
			 PHY_CLKLP2HS_TIME(dphy_clk_lp2hs_time(&dsi->dphy_cfg)));

	/*
	 * TODO dw drv improvements
	 * note: DSI_PHY_TMR_CFG.MAX_RD_TIME should be in line with
	 * DSI_CMD_MODE_CFG.MAX_RD_PKT_SIZE_LP (see CMD_MODE_ALL_LP)
	 */
	dw_write(dsi, DSI_PHY_TMR_RD_CFG, MAX_RD_TIME(10000));
}

static void dw_mipi_dsi_dphy_interface_config(struct dw_mipi_dsi *dsi)
{
	/*
	 * TODO dw drv improvements
	 * stop wait time should be the maximum between host dsi
	 * and panel stop wait times
	 */
	dw_write(dsi, DSI_PHY_IF_CFG, PHY_STOP_WAIT_TIME(0x20) | N_LANES(dsi->lanes));
}

static void dw_mipi_dsi_dphy_init(struct dw_mipi_dsi *dsi)
{
	phy_init(dsi->dphy);

	/* Put phy to reset state to prepare for configuration */
	dw_write(dsi, DSI_PHY_RSTZ, PHY_DISFORCEPLL | PHY_DISABLECLK | PHY_RSTZ | PHY_SHUTDOWNZ);

	phy_configure(dsi->dphy, (union phy_configure_opts *)&dsi->dphy_cfg);
}

static void dw_mipi_dsi_dphy_enable(struct dw_mipi_dsi *dsi)
{
	u32 val;
	int ret;

	phy_power_on(dsi->dphy);

	/* phy configuration has beenn done in reset state, now enable it */
	dw_write(dsi, DSI_PHY_RSTZ, PHY_ENFORCEPLL | PHY_ENABLECLK | PHY_UNRSTZ | PHY_UNSHUTDOWNZ);

	ret = readl_poll_timeout(dsi->base + DSI_PHY_STATUS, val, val & PHY_LOCK, 1000,
				 PHY_STATUS_TIMEOUT_US);
	if (ret)
		dev_err(dsi->dev, "failed to wait phy lock state\n");

	ret = readl_poll_timeout(dsi->base + DSI_PHY_STATUS, val, val & PHY_STOP_STATE_CLK_LANE,
				 1000, PHY_STATUS_TIMEOUT_US);
	if (ret)
		dev_err(dsi->dev, "failed to wait phy clk lane stop state\n");
}

/* The controller should generate 2 frames before preparing the peripheral. */
static void dw_mipi_dsi_wait_for_two_frames(const struct drm_display_mode *mode)
{
	int refresh, two_frames;

	refresh	   = drm_mode_vrefresh(mode);
	two_frames = DIV_ROUND_UP(MSEC_PER_SEC, refresh) * 2;
	msleep(two_frames);
}

static void dw_mipi_dsi_packet_handler_config(struct dw_mipi_dsi *dsi)
{
	dw_write(dsi, DSI_PCKHDL_CFG, CRC_RX_EN | ECC_RX_EN | BTA_EN);
}

static void dw_mipi_dsi_mode_set(struct dw_mipi_dsi *dsi, const struct drm_display_mode *mode)
{
	dw_mipi_dsi_init(dsi);

	dw_mipi_dsi_dpi_config(dsi, mode);
	dw_mipi_dsi_packet_handler_config(dsi);
	dw_mipi_dsi_video_mode_config(dsi);
	dw_mipi_dsi_video_packet_config(dsi, mode);
	dw_mipi_dsi_command_mode_config(dsi, mode);
	dw_mipi_dsi_timing_config(dsi, mode);

	dw_mipi_dsi_dphy_init(dsi);
	dw_mipi_dsi_dphy_timing_config(dsi);
	dw_mipi_dsi_dphy_interface_config(dsi);

	dw_mipi_dsi_wait_for_two_frames(mode);

	/* Switch to cmd mode for panel-bridge pre_enable & panel prepare */
	dw_mipi_dsi_set_mode(dsi, 0);
}

static inline bool __is_dual_mode(struct dw_mipi_dsi_primary *primary)
{
	return !!primary->secondary_dsi;
}

static bool bridge_mode_fixup(struct drm_bridge *bridge, const struct drm_display_mode *mode,
			      struct drm_display_mode *adjusted_mode)
{
	struct dw_mipi_dsi_primary *primary = bridge_to_primary(bridge);
	struct phy_configure_opts_mipi_dphy dphy_cfg;
	int bpp = mipi_dsi_pixel_format_to_bpp(primary->dsi.format);
	int ret;

	/* Already adjusted mode */
	if (mode->clock != adjusted_mode->clock)
		return true;

	if (__is_dual_mode(primary)) {
		adjusted_mode->hdisplay /= 2;
		adjusted_mode->hsync_start /= 2;
		adjusted_mode->hsync_end /= 2;
		adjusted_mode->htotal /= 2;
		adjusted_mode->clock /= 2;
	}

	phy_mipi_dphy_get_default_config(adjusted_mode->clock * 1000, bpp, primary->dsi.lanes,
					 &dphy_cfg);

	ret = phy_validate(primary->dsi.dphy, PHY_MODE_MIPI_DPHY, 0,
			   (union phy_configure_opts *)&dphy_cfg);
	if (primary->dsi.dphy && ret)
		return false;

	memcpy(&primary->dsi.dphy_cfg, &dphy_cfg, sizeof(dphy_cfg));

	primary->dsi.lane_link_rate = dphy_cfg.hs_clk_rate / 1000;

	return true;
}

static void bridge_mode_set(struct drm_bridge *bridge, const struct drm_display_mode *mode,
			    const struct drm_display_mode *adjusted_mode)
{
	struct dw_mipi_dsi_primary *primary = bridge_to_primary(bridge);

	dw_mipi_dsi_mode_set(&primary->dsi, adjusted_mode);
	dw_mipi_dsi_dphy_enable(&primary->dsi);

	if (__is_dual_mode(primary)) {
		dw_mipi_dsi_mode_set(primary->secondary_dsi, adjusted_mode);
		dw_mipi_dsi_dphy_enable(primary->secondary_dsi);
	}
}

static void bridge_enable(struct drm_bridge *bridge)
{
	struct dw_mipi_dsi_primary *primary = bridge_to_primary(bridge);

	dw_mipi_dsi_set_mode(&primary->dsi, primary->dsi.mode_flags);
	if (__is_dual_mode(primary))
		dw_mipi_dsi_set_mode(primary->secondary_dsi, primary->dsi.mode_flags);
}

static void dw_mipi_dsi_disable(struct dw_mipi_dsi *dsi)
{
	dw_write(dsi, DSI_LPCLK_CTRL, 0);

	dw_write(dsi, DSI_PHY_RSTZ, PHY_RSTZ);

	dw_write(dsi, DSI_PWR_UP, RESET);
}

static void bridge_post_disable(struct drm_bridge *bridge)
{
	struct dw_mipi_dsi_primary *primary = bridge_to_primary(bridge);

	/*
	 * Switch to command mode before panel-bridge post_disable &
	 * panel unprepare.
	 * Note: panel-bridge disable & panel disable has been called
	 * before by the drm framework.
	 */
	dw_mipi_dsi_set_mode(&primary->dsi, 0);

	/*
	 * TODO Only way found to call panel-bridge post_disable &
	 * panel unprepare before the dsi "final" disable...
	 * This needs to be fixed in the drm_bridge framework and the API
	 * needs to be updated to manage our own call chains...
	 */
	if (primary->panel_bridge->funcs && primary->panel_bridge->funcs->post_disable)
		primary->panel_bridge->funcs->post_disable(primary->panel_bridge);

	phy_power_off(primary->dsi.dphy);

	if (__is_dual_mode(primary))
		dw_mipi_dsi_disable(primary->secondary_dsi);

	dw_mipi_dsi_disable(&primary->dsi);
}

#ifdef CONFIG_DEBUG_FS
static u8 dw_mipi_dsi_get_pattern(struct dw_mipi_dsi *dsi)
{
	u32 val = dw_read(dsi, DSI_VID_MODE_CFG);

	if (!(val & VPG_EN))
		return DW_MIPI_DSI_PATTERN_OFF;

	if (val & VPG_ORIENTATION)
		return DW_MIPI_DSI_PATTERN_H;

	return DW_MIPI_DSI_PATTERN_V;
}

static void dw_mipi_dsi_set_pattern(struct dw_mipi_dsi *dsi, u8 pattern)
{
	u32 val = dw_read(dsi, DSI_VID_MODE_CFG);

	switch (pattern) {
	case DW_MIPI_DSI_PATTERN_OFF:
		val &= ~VPG_EN;
		break;

	case DW_MIPI_DSI_PATTERN_H:
		val |= VPG_EN | VPG_ORIENTATION;
		break;

	case DW_MIPI_DSI_PATTERN_V:
		val &= ~VPG_ORIENTATION;
		val |= VPG_EN;
		break;

	default:
		break;
	}

	dw_write(dsi, DSI_PWR_UP, RESET);

	dw_write(dsi, DSI_VID_MODE_CFG, val);

	dw_write(dsi, DSI_PWR_UP, POWERUP);
}

static ssize_t dsi_debugfs_read(struct file *f, char __user *buf, size_t size, loff_t *pos)
{
	struct dw_mipi_dsi_primary *primary = f->f_inode->i_private;
	char status[32];
	u8 pattern;

	pattern = dw_mipi_dsi_get_pattern(&primary->dsi);
	snprintf(status, sizeof(status), "%s [%s, %s, %s]\n", pattern_string[pattern],
		 pattern_string[DW_MIPI_DSI_PATTERN_OFF], pattern_string[DW_MIPI_DSI_PATTERN_H],
		 pattern_string[DW_MIPI_DSI_PATTERN_V]);

	return simple_read_from_buffer(buf, size, pos, status, strlen(status));
}

static ssize_t dsi_debugfs_write(struct file *f, const char __user *buf, size_t size, loff_t *pos)
{
	struct dw_mipi_dsi_primary *primary = f->f_inode->i_private;
	char cmd[32];
	int ret;

	if (*pos != 0 || size == 0 || size >= 32)
		return -EINVAL;

	ret = strncpy_from_user(cmd, buf, size);
	if (ret < 0)
		return ret;

	cmd[size] = '\0';

	if (!strncmp(cmd, pattern_string[DW_MIPI_DSI_PATTERN_OFF], 3))
		dw_mipi_dsi_set_pattern(&primary->dsi, DW_MIPI_DSI_PATTERN_OFF);
	else if (!strncmp(cmd, pattern_string[DW_MIPI_DSI_PATTERN_H], 1))
		dw_mipi_dsi_set_pattern(&primary->dsi, DW_MIPI_DSI_PATTERN_H);
	else if (!strncmp(cmd, pattern_string[DW_MIPI_DSI_PATTERN_V], 1))
		dw_mipi_dsi_set_pattern(&primary->dsi, DW_MIPI_DSI_PATTERN_V);

	return size;
}

static const struct file_operations dsi_debugfs_fops = {
	.owner	= THIS_MODULE,
	.read	= dsi_debugfs_read,
	.write	= dsi_debugfs_write,
	.llseek = seq_lseek,
};

static void dsi_debugfs_init(struct drm_bridge *bridge)
{
	struct dw_mipi_dsi_primary *primary = bridge_to_primary(bridge);
	char name[32];

	if (!primary->debugfs_dir)
		primary->debugfs_dir = debugfs_create_dir("dsi-bridge", NULL);
	if (!primary->debugfs_dir)
		return;

	snprintf(name, sizeof(name), "dsi-%d-pattern", bridge->encoder->index);
	primary->debugfs_file =
		debugfs_create_file(name, 0644, primary->debugfs_dir, primary, &dsi_debugfs_fops);
}

static void dsi_debugfs_fini(struct drm_bridge *bridge)
{
	struct dw_mipi_dsi_primary *primary = bridge_to_primary(bridge);

	if (!primary->debugfs_dir)
		return;

	debugfs_remove_recursive(primary->debugfs_dir);
	primary->debugfs_dir  = NULL;
	primary->debugfs_file = NULL;
}
#endif

static int bridge_attach(struct drm_bridge *bridge, enum drm_bridge_attach_flags flags)
{
	struct dw_mipi_dsi_primary *primary = bridge_to_primary(bridge);

	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found\n");
		return -ENODEV;
	}

#ifdef CONFIG_DEBUG_FS
	dsi_debugfs_init(bridge);
#endif

	/* Attach the panel-bridge to the dsi bridge */
	return drm_bridge_attach(bridge->encoder, primary->panel_bridge, bridge, flags);
}

static void bridge_detach(struct drm_bridge *bridge)
{
#ifdef CONFIG_DEBUG_FS
	dsi_debugfs_fini(bridge);
#endif
}

static const struct drm_bridge_funcs dw_mipi_dsi_bridge_funcs = {
	.mode_set     = bridge_mode_set,
	.enable	      = bridge_enable,
	.post_disable = bridge_post_disable,
	.attach	      = bridge_attach,
	.detach	      = bridge_detach,
	.mode_fixup   = bridge_mode_fixup,
};

static int dsi_attach_primary(struct dw_mipi_dsi *secondary, struct device *dev)
{
	struct dw_mipi_dsi *dsi		    = dev_get_drvdata(dev);
	struct dw_mipi_dsi_primary *primary = dsi_to_primary(dsi);

	if (!of_device_is_compatible(dev->of_node, "verisilicon,dw-mipi-dsi"))
		return -EINVAL;

	primary->secondary_dsi = secondary;
	return 0;
}

static struct dw_mipi_dsi *get_primary_dsi(struct device *dev)
{
	struct dw_mipi_dsi_primary *primary;

	primary = devm_kzalloc(dev, sizeof(*primary), GFP_KERNEL);
	if (!primary)
		return NULL;

	primary->dsi.dev = dev;

	return &primary->dsi;
}

static int primary_bind(struct dw_mipi_dsi *dsi)
{
	struct dw_mipi_dsi_primary *primary = dsi_to_primary(dsi);
	struct device *dev		    = primary->dsi.dev;
	struct drm_bridge *bridge;

	bridge = devm_drm_of_get_bridge(dev, dev->of_node, 1, 0);
	if (IS_ERR(bridge))
		return PTR_ERR(bridge);

	primary->panel_bridge = bridge;

	return 0;
}

static void primary_unbind(struct dw_mipi_dsi *dsi)
{
	struct dw_mipi_dsi_primary *primary = dsi_to_primary(dsi);

	primary->secondary_dsi = NULL;
}

static const struct dw_mipi_dsi_funcs g_primary = {
	.get_dsi = get_primary_dsi,
	.bind	 = primary_bind,
	.unbind	 = primary_unbind,
};

static struct dw_mipi_dsi *get_secondary_dsi(struct device *dev)
{
	struct dw_mipi_dsi *dsi;

	dsi = devm_kzalloc(dev, sizeof(*dsi), GFP_KERNEL);
	if (!dsi)
		return NULL;

	dsi->dev = dev;
	return dsi;
}

static int secondary_bind(struct dw_mipi_dsi *dsi)
{
	struct device_node *np;
	struct platform_device *pdev;

	np = of_find_compatible_node(NULL, NULL, "verisilicon,dw-mipi-dsi");
	if (!np)
		return -ENODEV;

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev)
		return -ENODEV;

	return dsi_attach_primary(dsi, &pdev->dev);
}

static void secondary_unbind(struct dw_mipi_dsi *dsi) {}

static const struct dw_mipi_dsi_funcs g_secondary = {
	.get_dsi = get_secondary_dsi,
	.bind	 = secondary_bind,
	.unbind	 = secondary_unbind,
};

static int dsi_bind(struct device *dev, struct device *primary, void *data)
{
	struct dw_mipi_dsi *dsi = dev_get_drvdata(dev);
	int ret;

	ret = dsi->funcs->bind(dsi);
	if (ret)
		return ret;

	pm_runtime_get_sync(dsi->dev);
	ret = clk_prepare_enable(dsi->pclk);
	if (ret) {
		pm_runtime_put(dsi->dev);
		return ret;
	}

	/* clean state if enabled in u-boot */
	dw_mipi_dsi_disable(dsi);

	return 0;
}

static void dsi_unbind(struct device *dev, struct device *primary, void *data)
{
	struct dw_mipi_dsi *dsi = dev_get_drvdata(dev);

	dsi->funcs->unbind(dsi);
	clk_disable_unprepare(dsi->pclk);
	pm_runtime_put(dsi->dev);
}

static const struct component_ops dsi_component_ops = {
	.bind	= dsi_bind,
	.unbind = dsi_unbind,
};

static const struct of_device_id dw_mipi_dsi_dt_match[] = {
	{.compatible = "verisilicon,dw-mipi-dsi", .data = &g_primary},
	{.compatible = "verisilicon,dw-mipi-dsi-2nd", .data = &g_secondary},
	{},
};
MODULE_DEVICE_TABLE(of, dw_mipi_dsi_dt_match);

static int dsi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct dw_mipi_dsi_funcs *funcs;
	struct dw_mipi_dsi *dsi;
	struct resource *res;
	int ret;

	funcs = of_device_get_match_data(dev);
	dsi   = funcs->get_dsi(dev);
	if (!dsi)
		return -ENOMEM;

	res	  = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dsi->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(dsi->base))
		return -ENODEV;

	dsi->pclk = devm_clk_get_optional(dev, "pclk");
	if (IS_ERR(dsi->pclk))
		return PTR_ERR(dsi->pclk);

	dsi->dphy = devm_phy_optional_get(dev, "dphy");
	if (IS_ERR(dsi->dphy))
		return PTR_ERR(dsi->dphy);

	if (dsi->dphy) {
		dsi->phy_link = device_link_add(dev, &dsi->dphy->dev,
						DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		if (!dsi->phy_link) {
			dev_err(dev, "failed to setup link with %s", dev_name(&dsi->dphy->dev));
			return -EINVAL;
		}
	}
	dsi->host.ops = &dw_mipi_dsi_host_ops;
	dsi->host.dev = dev;
	ret	      = mipi_dsi_host_register(&dsi->host);
	if (ret)
		return ret;

	dsi->funcs = funcs;
	dev_set_drvdata(dev, dsi);

	pm_runtime_enable(dev);

	if (of_device_is_compatible(dev->of_node, "verisilicon,dw-mipi-dsi")) {
		struct dw_mipi_dsi_primary *primary = dsi_to_primary(dsi);

		primary->bridge.funcs	= &dw_mipi_dsi_bridge_funcs;
		primary->bridge.of_node = dev->of_node;
		drm_bridge_add(&primary->bridge);
	}

	return component_add(dev, &dsi_component_ops);
}

static int dsi_remove(struct platform_device *pdev)
{
	struct device *dev	= &pdev->dev;
	struct dw_mipi_dsi *dsi = dev_get_drvdata(dev);

	if (dsi->phy_link)
		device_link_del(dsi->phy_link);

	mipi_dsi_host_unregister(&dsi->host);

	pm_runtime_disable(dev);

	if (of_device_is_compatible(dev->of_node, "verisilicon,dw-mipi-dsi")) {
		struct dw_mipi_dsi_primary *primary = dsi_to_primary(dsi);

		drm_bridge_remove(&primary->bridge);
	}

	component_del(dev, &dsi_component_ops);

	dev_set_drvdata(dev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dsi_suspend(struct device *dev)
{
	struct dw_mipi_dsi *dsi = dev_get_drvdata(dev);

	clk_disable_unprepare(dsi->pclk);

	return 0;
}

static int dsi_resume(struct device *dev)
{
	struct dw_mipi_dsi *dsi = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(dsi->pclk);
	if (ret < 0) {
		dev_err(dev, "failed to prepare/enable pclk\n");
	}

	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(dsi_pm_ops, dsi_suspend, dsi_resume);

struct platform_driver dw_mipi_dsi_driver = {
	.probe	= dsi_probe,
	.remove = dsi_remove,
	.driver =
		{
			.name		= "vs-dw-mipi-dsi",
			.of_match_table = of_match_ptr(dw_mipi_dsi_dt_match),
			.pm		= &dsi_pm_ops,
		},
};

MODULE_DESCRIPTION("DW MIPI DSI Controller Driver");
MODULE_LICENSE("GPL");
