/* SPDX-License-Identifier: GPL-2.0 */
/*
 * HAL interface for DW MIPI CSI
 */

#ifndef __DW_MIPI_CSI_HAL_H__
#define __DW_MIPI_CSI_HAL_H__

#include <linux/types.h>
#include <drm/drm_modes.h>
#include "dw_mipi_csi.h"
#include <linux/atomic.h>

#define CSI_USAGE_IDLE   0U
#define CSI_USAGE_DRM    1U
#define CSI_USAGE_IOCTL  2U

extern atomic_t g_csi_usage;

#define CSI_IRQ_MASK_ALL_BITS 0xFFFFFFFF
#define KHZ		      1000
#define CSI_DEFAULT_STOP_WAIT 0x20

#define CSI_FIFO_WIDTH_BITS	64U
#define CSI_PPI_OVERHEAD_BYTES	6U  /* manual constant */
#define CSI_FIFO_MARGIN_64	12U /* threshold for 64‑bit FIFO */
#define CSI_OVERHEAD_FACTOR_64	10U /* (lane_byte_clk / ipi_clk) * 10 */
#define CSI_PHY_DATA_WIDTH_BITS 8U  /* 8‑bit mode */

#define IPI_MODE_FORCE_AUTO	     0
#define IPI_MODE_FORCE_CUT_THROUGH   1
#define IPI_MODE_FORCE_STORE_FORWARD 2

/**
 * dw_mipi_csi_hal_init - Initializes the hardware context for CSI
 * @csi: pointer to csi context
 *
 * This function prepares the context. Resource acquisition is expected to be
 * handled by the platform driver. Returns 0 on success, otherwise negative.
 */
int dw_mipi_csi_hal_init(struct dw_mipi_csi *csi, enum dw_vpg_mode vpg_mode);

/**
 * dw_mipi_csi_hal_power_on - Enables CSI clocks or other power resources
 * @csi: pointer to csi context
 *
 * Returns 0 on success.
 */
int dw_mipi_csi_hal_power_on(struct dw_mipi_csi *csi);

/**
 * dw_mipi_csi_hal_power_off - Disables CSI clocks or other power resources
 * @csi: pointer to csi context
 */
void dw_mipi_csi_hal_power_off(struct dw_mipi_csi *csi);

/**
 * dw_mipi_csi_hal_set_mode - Configures the CSI hardware with given mode
 * @csi: pointer to csi context
 * @mode: pointer to drm_display_mode
 *
 * Returns 0 on success, otherwise negative.
 */
int dw_mipi_csi_hal_set_mode(struct dw_mipi_csi *csi, const struct csi_mode_info *mode_info);

/**
 * dw_mipi_csi_hal_disable - Resets or disables the CSI hardware
 * @csi: pointer to csi context
 */
void dw_mipi_csi_hal_disable(struct dw_mipi_csi *csi);

void dw_mipi_csi_hal_reset(struct dw_mipi_csi *csi);

int dw_mipi_csi_hal_vpg_init(struct dw_mipi_csi *csi, enum dw_mipi_csi_pattern pattern);

void dw_mipi_csi_hal_irq_enable(struct dw_mipi_csi *csi);

void dw_mipi_csi_hal_irq_disable(struct dw_mipi_csi *csi);

void dw_mipi_csi_hal_irq_handle(struct dw_mipi_csi *csi);

void dw_mipi_csi_apply_clk_continuous(struct dw_mipi_csi *csi);

static inline void dw_write(struct dw_mipi_csi *csi, u32 reg, u32 val)
{
	writel(val, csi->base + reg);
}

static inline u32 dw_read(struct dw_mipi_csi *csi, u32 reg)
{
	return readl(csi->base + reg);
}

#endif /* __DW_MIPI_CSI_HAL_H__ */
