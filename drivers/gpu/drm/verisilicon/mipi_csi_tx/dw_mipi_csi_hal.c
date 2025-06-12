// SPDX-License-Identifier: GPL-2.0
/*
 * HAL Implementation for DW MIPI CSI
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/phy.h>
#include <linux/phy/phy-mipi-dphy.h>
#include <linux/ratelimit.h>

#include "dw_mipi_csi_hal.h"
#include "dw_mipi_csi_reg.h"

#define CSI_IPI_FIFO_DEPTH	2048
#define CSI_IPI_FIFO_ENTRY_BITS 64

/* Interrupt mask descriptor */
struct csi_irq_mask_desc {
	const char *name;
	u32 reg_offset;
	u32 unmask_val;
};

struct irq_field_desc {
	const char *prefix;
	u32 bit;
	const char *desc;
};

static const struct irq_field_desc ipi_irq_fields[] = {
	{"IPI0", BIT(0), "Pixel Error"},
	{"IPI0", BIT(1), "FIFO Overflow"},
	{"IPI0", BIT(2), "Line Error"},
	{"IPI0", BIT(3), "FIFO Underflow"},
	{"IPI0", BIT(4), "Transmission Conflict"},

	{"IPI2", BIT(8), "Pixel Error"},
	{"IPI2", BIT(9), "FIFO Overflow"},
	{"IPI2", BIT(10), "Line Error"},
	{"IPI2", BIT(11), "FIFO Underflow"},

	{"IPI3", BIT(16), "Pixel Error"},
	{"IPI3", BIT(17), "FIFO Overflow"},
	{"IPI3", BIT(18), "Line Error"},
	{"IPI3", BIT(19), "FIFO Underflow"},

	{"IPI4", BIT(24), "Pixel Error"},
	{"IPI4", BIT(25), "FIFO Overflow"},
	{"IPI4", BIT(26), "Line Error"},
	{"IPI4", BIT(27), "FIFO Underflow"},
};

static const struct irq_field_desc idi_irq_fields[] = {
	{"IDI", BIT(0), "Word count mismatch"},	     {"IDI", BIT(1), "VC0 Frame Sequence Error"},
	{"IDI", BIT(2), "VC1 Frame Sequence Error"}, {"IDI", BIT(3), "VC2 Frame Sequence Error"},
	{"IDI", BIT(4), "VC3 Frame Sequence Error"}, {"IDI", BIT(5), "VC0 Line Sequence Error"},
	{"IDI", BIT(6), "VC1 Line Sequence Error"},  {"IDI", BIT(7), "VC2 Line Sequence Error"},
	{"IDI", BIT(8), "VC3 Line Sequence Error"},  {"IDI", BIT(9), "FIFO Overflow"},
};

static const struct irq_field_desc phy_irq_fields[] = {
	{"PHY", BIT(0), "TX HS Timeout"},
	{"PHY", BIT(1), "LP0 Contention Error"},
	{"PHY", BIT(2), "LP1 Contention Error"},
};

static const struct irq_field_desc vpg_irq_fields[] = {
	{"VPG", BIT(0), "Packet lost"},
};

static const struct irq_field_desc mt_ipi_irq_fields[] = {
	{"MT_IPI", BIT(0), "Header FIFO Overflow"},
};

static void dw_mipi_csi_log_irq_fields(struct dw_mipi_csi *csi, u32 status,
				       const struct irq_field_desc *fields, size_t count)
{
	size_t i;

	if (!csi || !fields)
		return;

	for (i = 0; i < count; i++) {
		if (status & fields[i].bit)
			dev_err_ratelimited(csi->dev, "[IRQ][%s] %s\n", fields[i].prefix,
					    fields[i].desc);
	}
}

static const struct csi_irq_mask_desc csi_irq_masks[] = {
	{"INT_MASK_N_VPG", 0x40, 0x00000001},
	{"INT_MASK_N_IDI", 0x48, 0x000003FF},
	{"INT_MASK_N_IPI", 0x50, 0x0f0f0f1f},
	{"INT_MASK_N_PHY", 0x58, 0x00000007},
};

static inline u32 dw_bus_fmt_to_bpp(enum dw_bus_fmt fmt)
{
	u32 bpp;

	switch (fmt) {
	case DW_BUS_FMT_RGB565_1:
	case DW_BUS_FMT_RGB565_2:
	case DW_BUS_FMT_RGB565_3:
		bpp = 16;
		break;
	case DW_BUS_FMT_RGB666_PACKED:
	case DW_BUS_FMT_RGB666:
		bpp = 18;
		break;
	case DW_BUS_FMT_RGB888:
		bpp = 24;
		break;
	case DW_BUS_FMT_YUV422:
		bpp = 16;
		break;
	case DW_BUS_FMT_RAW10:
		bpp = 30;
		break;
	default:
		bpp = 24;
		break;
	}

	return bpp;
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

static inline void csi_apply_hsclk_override(struct dw_mipi_csi *csi,
					    struct phy_configure_opts_mipi_dphy *cfg)
{
	if (csi->hsclk_override_khz) {
		dev_info(csi->dev, "HS-CLK overridden to %u kHz\n", csi->hsclk_override_khz);
		cfg->hs_clk_rate = (u64)csi->hsclk_override_khz * 1000U;
	}
}

u32 dw_bus_fmt_to_dt(enum dw_bus_fmt fmt)
{
	u32 dt;

	switch (fmt) {
	case DW_BUS_FMT_RGB565_1:
	case DW_BUS_FMT_RGB565_2:
	case DW_BUS_FMT_RGB565_3:
		dt = 0x22;
		break;
	case DW_BUS_FMT_RGB666_PACKED:
	case DW_BUS_FMT_RGB666:
		dt = 0x23;
		break;
	case DW_BUS_FMT_RGB888:
		dt = 0x24;
		break;
	case DW_BUS_FMT_YUV420:
		dt = 0x18;
		break;
	case DW_BUS_FMT_YUV422:
		dt = 0x1E;
		break;
	case DW_BUS_FMT_RAW8:
		dt = 0x2A;
		break;
	case DW_BUS_FMT_RAW10:
		dt = 0x2B;
		break;
	default:
		dt = 0x24;
		break;
	}

	return dt;
}

/**
 * dw_mipi_csi_apply_dphy_timing_overrides - apply user overrides to dphy_cfg
 * @csi: Pointer to the CSI controller context.
 *
 * If dphy_timing_override_enable is set, overwrite every timing field
 * with override_* (even if value is 0). Otherwise leave defaults alone.
 */
static void dw_mipi_csi_apply_dphy_timing_overrides(struct dw_mipi_csi *csi)
{
	if (!csi || !csi->dphy_timing_override_enable)
		return;

	csi->dphy_cfg.lpx	  = csi->override_lpx;
	csi->dphy_cfg.hs_prepare  = csi->override_hs_prepare;
	csi->dphy_cfg.hs_zero	  = csi->override_hs_zero;
	csi->dphy_cfg.hs_trail	  = csi->override_hs_trail;
	csi->dphy_cfg.hs_exit	  = csi->override_hs_exit;
	csi->dphy_cfg.clk_prepare = csi->override_clk_prepare;
	csi->dphy_cfg.clk_zero	  = csi->override_clk_zero;
	csi->dphy_cfg.clk_trail	  = csi->override_clk_trail;
	csi->dphy_cfg.clk_post	  = csi->override_clk_post;

	pr_info("dw_mipi_csi: applied DPHY timing overrides (enable=%u)\n",
		csi->dphy_timing_override_enable);
}

/**
 * dw_mipi_csi_apply_clk_continuous - Apply continuous or non-continuous clock mode to the
 * LPCLK_CTRL register
 * @csi: Pointer to the CSI controller context.
 *
 * This function configures the clock mode to either continuous or non-continuous
 * based on the `clk_continuous` field within the `dw_mipi_csi` structure.
 */
void dw_mipi_csi_apply_clk_continuous(struct dw_mipi_csi *csi)
{
	u32 val;

	if (csi->clk_continuous) {
		/* Set to continuous clock mode */
		val = CSI_LPCLK_CTRL_CONTINUOUS;
		pr_info("dw_mipi_csi: continuous clock mode enabled\n");
	} else {
		/* Set to non-continuous clock mode */
		val = CSI_LPCLK_CTRL_NON_CONTINUOUS;
		pr_info("dw_mipi_csi: non-continuous clock mode enabled\n");
	}

	/* Write the value to the LPCLK_CTRL register */
	dw_write(csi, CSI_LPCLK_CTRL, val);
}

static void dw_mipi_csi_esc_config(struct dw_mipi_csi *csi)
{
	u8 esc_clk;
	u32 esc_clk_division;
	u32 cfg;

	if (!csi) {
		pr_err("dw_mipi_csi: null csi pointer in esc_config\n");
		return;
	}

	/* limit esc clk on FPGA */
	if (!csi->pclk)
		esc_clk = 5; /* 5MHz */
	else
		esc_clk = 20; /* 20MHz */

	/*
	 * The maximum permitted escape clock is 20MHz and it is derived from
	 * lanebyteclk, which is running at "lane_link_rate / 8". Thus we want:
	 *
	 *     (lane_link_rate >> 3) / esc_clk_division < 20
	 * which is:
	 *     (lane_link_rate >> 3) / 20 > esc_clk_division
	 */

	esc_clk_division = ((csi->lane_link_rate / 1000) >> 3) / esc_clk + 1;

	cfg = TO_CLK_DIVISION(TO_CLK_DIV) | TX_ESC_CLK_DIVISION(esc_clk_division);

	dw_write(csi, CSI_CLKMGR_CFG, cfg);
}

static unsigned int dphy_data_lp2hs_time(struct phy_configure_opts_mipi_dphy *cfg)
{
	return ps_to_lbcc(cfg->hs_clk_rate, cfg->lpx + cfg->hs_prepare + cfg->hs_zero);
}

static unsigned int dphy_data_hs2lp_time(struct phy_configure_opts_mipi_dphy *cfg)
{
	return ps_to_lbcc(cfg->hs_clk_rate, cfg->hs_trail + cfg->hs_exit);
}

/* Get lane byte clock cycles. */
u32 dw_mipi_csi_get_hcomponent_lbcc(struct dw_mipi_csi *csi, const struct csi_mode_info *mode,
				    u32 hcomponent)
{
	u32 frac, lbcc;

	lbcc = hcomponent * csi->lane_link_rate / 8;

	frac = lbcc % mode->pixel_clock;
	lbcc = lbcc / mode->pixel_clock;
	if (frac)
		lbcc++;

	return lbcc;
}

/**
 * dw_mipi_csi_timeout_config - Configure D-PHY transmission timeout counter.
 * @csi: Pointer to the CSI controller context.
 * @mode: Pointer to timing mode information.
 *
 * This function calculates the HSTX timeout counter value based on
 * frame rate, vertical total lines, and link rate. Adds 10% margin.
 */
static void dw_mipi_csi_timeout_config(struct dw_mipi_csi *csi, const struct csi_mode_info *mode)
{
	u32 hstx_to;

	if (!csi) {
		pr_err("dw_mipi_csi: null csi pointer in timeout_config\n");
		return;
	}

	if (!mode) {
		pr_err("dw_mipi_csi: null mode pointer in timeout_config\n");
		return;
	}

	if (!mode->refresh_rate || !mode->vtotal) {
		pr_err("dw_mipi_csi: invalid mode parameters in timeout_config\n");
		return;
	}

	if (!csi->lane_link_rate) {
		pr_err("dw_mipi_csi: invalid lane_link_rate 0 in timeout_config\n");
		return;
	}

	/* Calculate timeout cycles:
	 * hstx_to = link_rate / (fps * vtotal) / 8
	 */
	hstx_to = mode->refresh_rate;
	hstx_to *= mode->vtotal * TO_CLK_DIV;
	hstx_to = csi->lane_link_rate * 1000 / 8 / hstx_to;

	/* Add 10% safety margin */
	hstx_to += hstx_to / 10;

	dw_write(csi, CSI_TO_CNT_CFG, HSTX_TO_CNT(hstx_to));
}

/**
 * dw_mipi_csi_dphy_init - Initialize DPHY hardware and apply configuration.
 * @csi: Pointer to the CSI controller context.
 *
 * This function powers on and configures the external D-PHY, placing
 * it into reset stop mode before programming the configuration structure.
 */
static void dw_mipi_csi_dphy_init(struct dw_mipi_csi *csi)
{
	if (!csi) {
		pr_err("dw_mipi_csi: null csi pointer in dphy_init\n");
		return;
	}

	if (!csi->dphy) {
		pr_err("dw_mipi_csi: null dphy pointer in dphy_init\n");
		return;
	}

	/* Initialize external PHY device */
	phy_init(csi->dphy);

	/* Force PHY to TX stop mode before configuration */
	dw_write(csi, CSI_PHY_RSTZ, PHY_FORCETXSTOPMODE | PHY_SHUTDOWNZ);

	/* Apply pre-computed configuration */
	phy_configure(csi->dphy, (union phy_configure_opts *)&csi->dphy_cfg);
}

/**
 * dw_mipi_csi_vpg_pkt_configure - Configure VPG packet output format.
 * @csi: Pointer to the CSI controller context.
 *
 * This function writes the data type and default flags for the VPG packet
 * generation. The settings are fixed to zero except data_type.
 */
static void dw_mipi_csi_vpg_pkt_configure(struct dw_mipi_csi *csi)
{
	u32 val;

	if (!csi) {
		pr_err("dw_mipi_csi: csi is NULL in vpg_pkt_configure\n");
		return;
	}

	val = FIELD_PREP(CSI_VPG_VC_EXT_MASK, 0) | FIELD_PREP(CSI_VPG_FRAME_NUM_MODE_MASK, 0) |
	      FIELD_PREP(CSI_VPG_LINE_NUM_MODE_MASK, 0) | FIELD_PREP(CSI_VPG_HSYNC_PKT_EN_MASK, 0) |
	      FIELD_PREP(CSI_VPG_VC_MASK, 0) |
	      FIELD_PREP(CSI_VPG_DATA_TYPE_MASK, dw_bus_fmt_to_dt(csi->bus_fmt));

	dw_write(csi, CSI_VPG_PKT_CFG, val);
	pr_info("dw_mipi_csi: configure VPG_PKT_CFG val = 0x%08x\n", val);

	/* Configure max frame number for VPG mode */
	dw_write(csi, CSI_VPG_MAX_FRAME_NUM, 0xffff);
}

/**
 * dw_mipi_csi_vpg_timing_config - Configure VPG timing based on mode info.
 * @csi: Pointer to the CSI controller context.
 * @mode: Pointer to display mode information.
 *
 * This function calculates and writes horizontal and vertical timing
 * parameters to VPG-specific registers based on VGA timing or others.
 */
static void dw_mipi_csi_vpg_timing_config(struct dw_mipi_csi *csi, struct csi_mode_info *mode)
{
	u32 htotal = 0;
	u32 hsa	   = 0;
	u32 hbp	   = 0;
	u32 lbcc   = 0;

	if (!csi) {
		pr_err("dw_mipi_csi: null csi pointer in vpg_timing_config\n");
		return;
	}

	if (!mode) {
		pr_err("dw_mipi_csi: null mode pointer in vpg_timing_config\n");
		return;
	}

	pr_info("dw_mipi_csi: hdisplay = %u, htotal = %u\n", mode->hdisplay, mode->htotal);
	dw_write(csi, CSI_VPG_PKT_SIZE, mode->hdisplay);

	/* Calculate horizontal sync parameters */
	htotal = mode->htotal;
	hsa    = mode->hsync_end - mode->hsync_start;
	hbp    = htotal - mode->hsync_end;

	pr_info("dw_mipi_csi: hsa = %u (hsync_end = %u, hsync_start = %u)\n", hsa, mode->hsync_end,
		mode->hsync_start);
	pr_info("dw_mipi_csi: hbp = %u (htotal = %u, hsync_end = %u)\n", hbp, htotal,
		mode->hsync_end);

	/* Horizontal total line time */
	lbcc = dw_mipi_csi_get_hcomponent_lbcc(csi, mode, htotal);
	dw_write(csi, CSI_VPG_HLINE_TIME, lbcc);
	pr_info("dw_mipi_csi: lbcc htotal = %u\n", lbcc);

	/* Horizontal sync active */
	lbcc = dw_mipi_csi_get_hcomponent_lbcc(csi, mode, hsa);
	dw_write(csi, CSI_VPG_HSA_TIME, lbcc);
	pr_info("dw_mipi_csi: lbcc hsa = %u\n", lbcc);

	/* Horizontal back porch */
	lbcc = dw_mipi_csi_get_hcomponent_lbcc(csi, mode, hbp);
	dw_write(csi, CSI_VPG_HBP_TIME, lbcc);
	pr_info("dw_mipi_csi: lbcc hbp = %u\n", lbcc);

	/* Configure vertical timing */
	pr_info("dw_mipi_csi: vdisplay = %u, vsync_start = %u, vsync_end = %u, vtotal = %u\n",
		mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal);

	dw_write(csi, CSI_VPG_ACT_LINES, mode->vdisplay);
	dw_write(csi, CSI_VPG_VSA_LINES, mode->vsync_end - mode->vsync_start);
	dw_write(csi, CSI_VPG_VFP_LINES, mode->vsync_start - mode->vdisplay);
	dw_write(csi, CSI_VPG_VBP_LINES, mode->vtotal - mode->vsync_end);
}

/**
 * dw_mipi_csi_calc_send_start - Calculate DATA_SEND_START value.
 * @csi: Pointer to the CSI controller context.
 *
 * This function calculates the IPI_DATA_SEND_START register value when using
 * cut-through mode. The value is derived from pixel timing and PHY throughput.
 * All calculations are performed in 16.16 fixed-point using integer arithmetic.
 *
 * Return: Recommended send_start value; 0 on invalid input.
 */
static u32 dw_mipi_csi_calc_send_start(struct dw_mipi_csi *csi)
{
	u32 h_active	      = 0U;
	u32 bpp_bits	      = 0U;
	u32 bpp_bytes	      = 0U;
	u32 ipi_data_timing   = 0U;
	u32 n_lanes	      = 0U;
	u32 lane_rate_khz     = 0U;
	u32 lane_byte_clk_khz = 0U;
	u32 ipi_clk_khz	      = 0U;
	u64 ratio_fp	      = 0ULL; /* 16.16 fixed‑point */
	u32 numerator	      = 0U;
	u32 denominator	      = 0U;
	u64 tmp64	      = 0ULL;
	u32 ppi_data_timing   = 0U;
	u32 delta	      = 0U;
	u32 overhead_cycles   = 0U;
	u32 send_start	      = 0U;

	if (!csi) {
		pr_err("dw_mipi_csi: null csi in calc_send_start\n");
		return 0U;
	}

	h_active = csi->mode.hdisplay;
	bpp_bits = dw_bus_fmt_to_bpp(csi->bus_fmt);
	if (!bpp_bits) {
		pr_err("dw_mipi_csi: invalid bus_fmt bpp = 0\n");
		return 0U;
	}

	bpp_bytes = bpp_bits / 8U;

	ipi_data_timing = h_active;

	n_lanes = csi->lanes;
	if (!n_lanes) {
		pr_err("dw_mipi_csi: lane count is 0\n");
		return 0U;
	}

	lane_rate_khz	  = csi->lane_link_rate;
	lane_byte_clk_khz = lane_rate_khz / 8U;
	ipi_clk_khz	  = csi->mode.pixel_clock;

	if (!ipi_clk_khz) {
		pr_err("dw_mipi_csi: pixel_clock is 0\n");
		return 0U;
	}

	/* ratio = F_ipi / F_byte (16.16 fixed‑point) */
	tmp64	 = ((u64)ipi_clk_khz) << 16;
	ratio_fp = div_u64(tmp64, (u64)lane_byte_clk_khz);

	numerator   = CSI_PPI_OVERHEAD_BYTES + h_active * bpp_bytes;
	denominator = (CSI_PHY_DATA_WIDTH_BITS / 8U) * n_lanes;

	tmp64		= ratio_fp * (u64)numerator;
	ppi_data_timing = (u32)((tmp64 >> 16) / denominator);

	delta = (ipi_data_timing > ppi_data_timing) ? (ipi_data_timing - ppi_data_timing) : 0U;

	tmp64		= ratio_fp * (u64)CSI_OVERHEAD_FACTOR_64;
	overhead_cycles = (u32)(tmp64 >> 16);

	if (delta <= CSI_FIFO_MARGIN_64)
		send_start = CSI_FIFO_MARGIN_64 + overhead_cycles;
	else
		send_start = delta + overhead_cycles;

	pr_info("dw_mipi_csi: h_active = %u\n", h_active);
	pr_info("dw_mipi_csi: bpp_bits = %u, bpp_bytes = %u\n", bpp_bits, bpp_bytes);
	pr_info("dw_mipi_csi: lanes = %u\n", n_lanes);
	pr_info("dw_mipi_csi: lane_link_rate = %u kHz\n", lane_rate_khz);
	pr_info("dw_mipi_csi: ipi_clk = %u kHz\n", ipi_clk_khz);
	pr_info("dw_mipi_csi: lane_byte_clk = %u kHz\n", lane_byte_clk_khz);
	pr_info("dw_mipi_csi: ipi_data_timing = %u\n", ipi_data_timing);
	pr_info("dw_mipi_csi: ppi_data_timing = %u\n", ppi_data_timing);
	pr_info("dw_mipi_csi: delta = %u\n", delta);
	pr_info("dw_mipi_csi: overhead_cycles = %u\n", overhead_cycles);
	pr_info("dw_mipi_csi: calculated send_start = %u\n", send_start);

	return send_start;
}

/**
 * dw_mipi_csi_ipi_pkt_configure - Configure IPI packet output format.
 * @csi: Pointer to the CSI controller context.
 *
 * This function calculates output packet mode and configures the IPI block
 * based on resolution and data format. Determines cut-through or store-forward
 * mode depending on line size.
 */
static void dw_mipi_csi_ipi_pkt_configure(struct dw_mipi_csi *csi)
{
	struct dw_ipi_pkt_conf *pkt_conf = NULL;
	u32 val				 = 0;
	u32 bits_per_pixel		 = 0;
	u32 line_width_bits		 = 0;
	u32 fifo_entries_per_line	 = 0;

	if (!csi) {
		pr_err("dw_mipi_csi: csi is NULL in ipi_pkt_configure\n");
		return;
	}

	pkt_conf = &csi->pkt_conf;

	/* Clear old config before applying new one */
	memset(pkt_conf, 0, sizeof(*pkt_conf));

	/* Calculate bits per pixel and total bits per line */
	bits_per_pixel	= dw_bus_fmt_to_bpp(csi->bus_fmt);
	line_width_bits = csi->mode.hdisplay * bits_per_pixel;

	/* Convert line bits to FIFO entry count */
	fifo_entries_per_line = DIV_ROUND_UP(line_width_bits, CSI_IPI_FIFO_ENTRY_BITS);

	pr_info("dw_mipi_csi: bits_per_pixel = %u\n", bits_per_pixel);
	pr_info("dw_mipi_csi: line_width_bits = %u\n", line_width_bits);
	pr_info("dw_mipi_csi: fifo_entries_per_line = %u\n", fifo_entries_per_line);

	/* Determine transmission mode based on line length */
	switch (csi->ipi_mode_override) {
	case IPI_MODE_FORCE_CUT_THROUGH:
		pkt_conf->ipi_mode = IPI_CUT_THROUGH_MODE;
		pr_info("dw_mipi_csi: using CUT_THROUGH_MODE (override)\n");
		break;
	case IPI_MODE_FORCE_STORE_FORWARD:
		pkt_conf->ipi_mode = IPI_STORE_FORWARD_MODE;
		pr_info("dw_mipi_csi: using STORE_FORWARD_MODE (override)\n");
		break;
	case IPI_MODE_FORCE_AUTO:
	default:
		// Store and forward need double fifo
		if (2 * fifo_entries_per_line <= CSI_IPI_FIFO_DEPTH) {
			pkt_conf->ipi_mode = IPI_STORE_FORWARD_MODE;
			pr_info("dw_mipi_csi: using STORE_FORWARD_MODE (auto)\n");
		} else {
			pkt_conf->ipi_mode = IPI_CUT_THROUGH_MODE;
			pr_info("dw_mipi_csi: using CUT_THROUGH_MODE (auto)\n");
		}
		break;
	}

	/* Compose IPI packet config register value */
	val = FIELD_PREP(CSI_IPI_MODE_MASK, pkt_conf->ipi_mode) |
	      FIELD_PREP(CSI_IPI_VC_EXT_MASK, pkt_conf->vc >> 2) |
	      (pkt_conf->frame_num_mode ? CSI_IPI_FRAME_NUM_MODE_MASK : 0) |
	      FIELD_PREP(CSI_IPI_LINE_NUM_MODE_MASK, pkt_conf->line_num_mode) |
	      (pkt_conf->hsync_pkt_en ? CSI_IPI_HSYNC_PKT_EN_MASK : 0) |
	      FIELD_PREP(CSI_IPI_VC_MASK, pkt_conf->vc & 0x3) |
	      FIELD_PREP(CSI_IPI_DATA_TYPE_MASK, dw_bus_fmt_to_dt(csi->bus_fmt));

	pr_info("dw_mipi_csi: ipi_mode = %u, vc = %u, line_num_mode = %u\n", pkt_conf->ipi_mode,
		pkt_conf->vc, pkt_conf->line_num_mode);
	pr_info("dw_mipi_csi: data_type = 0x%x\n", dw_bus_fmt_to_dt(csi->bus_fmt));
	pr_info("dw_mipi_csi: writing IPI_PKG_CFG = 0x%08x\n", val);

	dw_write(csi, CSI_IPI_PKG_CFG, val);

	pr_info("dw_mipi_csi: frame_max = %u, line_start = %u, line_step = %u\n",
		pkt_conf->frame_max, pkt_conf->line_start, pkt_conf->line_step);

	dw_write(csi, CSI_IPI_MAX_FRAME_NUM, pkt_conf->frame_max);
	dw_write(csi, CSI_IPI_START_LINE_NUM, pkt_conf->line_start);
	dw_write(csi, CSI_IPI_STEP_LINE_NUM, pkt_conf->line_step);

	/* For cut-through mode, configure send_start */
	if (pkt_conf->ipi_mode == IPI_CUT_THROUGH_MODE) {
		pkt_conf->send_start = dw_mipi_csi_calc_send_start(csi);
		pr_info("dw_mipi_csi: send_start = %u\n", pkt_conf->send_start);
		dw_write(csi, CSI_IPI_DATA_SEND_START, pkt_conf->send_start);
	}
}

/**
 * dw_mipi_csi_ipi_timing_config - Configure IPI horizontal and vertical timing.
 * @csi: Pointer to the CSI controller context.
 * @mode_info: Pointer to the timing mode structure.
 *
 * This function programs horizontal/vertical timing registers
 * for IPI output mode, based on display resolution and sync intervals.
 */
static void dw_mipi_csi_ipi_timing_config(struct dw_mipi_csi *csi,
					  const struct csi_mode_info *mode_info)
{
	const struct csi_mode_info *mode = mode_info;
	u32 htotal			 = 0;
	u32 hsa				 = 0;
	u32 hbp				 = 0;
	u32 lbcc			 = 0;

	if (!csi) {
		pr_err("dw_mipi_csi: null csi pointer in ipi_timing_config\n");
		return;
	}

	if (!mode) {
		pr_err("dw_mipi_csi: null mode pointer in ipi_timing_config\n");
		return;
	}

	/* Horizontal active pixel count */
	dw_write(csi, CSI_IPI_PIXELS, mode->hdisplay);

	/* Compute horizontal timing parameters */
	htotal = mode->htotal;
	hsa    = mode->hsync_end - mode->hsync_start;
	hbp    = mode->htotal - mode->hsync_end;

	/* Configure horizontal line time (in PPI clock cycles) */
	lbcc = dw_mipi_csi_get_hcomponent_lbcc(csi, mode, htotal);
	dw_write(csi, CSI_IPI_HLINE_PPI_TIME, lbcc);

	/* Configure HSA + HBP time (in PPI clock cycles) */
	lbcc = dw_mipi_csi_get_hcomponent_lbcc(csi, mode, hsa + hbp);
	dw_write(csi, CSI_IPI_HSA_HBP_PPI_TIME, lbcc);

	/* Vertical total lines */
	dw_write(csi, CSI_IPI_LINES, mode->vtotal);

	/* Vertical active lines */
	dw_write(csi, CSI_IPI_ACT_LINES, mode->vdisplay);

	/* VSA (VSync active) lines */
	dw_write(csi, CSI_IPI_VSA_LINES, mode->vsync_end - mode->vsync_start);

	/* VFP (Vertical front porch) lines */
	dw_write(csi, CSI_IPI_VFP_LINES, mode->vsync_start - mode->vdisplay);

	/* VBP (Vertical back porch) lines */
	dw_write(csi, CSI_IPI_VBP_LINES, mode->vtotal - mode->vsync_end);
}

/**
 * dw_mipi_csi_dphy_timing_config - Configure DPHY LP/HS switch timing.
 * @csi: Pointer to the CSI controller context.
 *
 * This function sets the HS-to-LP and LP-to-HS transition times
 * based on the configured DPHY parameters.
 */
static void dw_mipi_csi_dphy_timing_config(struct dw_mipi_csi *csi)
{
	u32 val = 0;

	if (!csi) {
		pr_err("dw_mipi_csi: null csi pointer in dphy_timing_config\n");
		return;
	}

	val = PHY_HS2LP_TIME(dphy_data_hs2lp_time(&csi->dphy_cfg)) |
	      PHY_LP2HS_TIME(dphy_data_lp2hs_time(&csi->dphy_cfg));

	dw_write(csi, CSI_PHY_SWITCH_TIME, val);
}

/**
 * dw_mipi_csi_dphy_interface_config - Configure DPHY interface control.
 * @csi: Pointer to the CSI controller context.
 *
 * This function configures stop wait time and number of data lanes.
 * Note: stop wait time should ideally be the maximum between host and panel.
 */
static void dw_mipi_csi_dphy_interface_config(struct dw_mipi_csi *csi)
{
	u32 val = 0;

	if (!csi) {
		pr_err("dw_mipi_csi: null csi pointer in dphy_interface_config\n");
		return;
	}

	/*
	 * TODO dw drv improvements
	 * stop wait time should be the maximum between host csi
	 * and panel stop wait times
	 */
	val = PHY_STOP_WAIT_TIME(0x0) | N_LANES(csi->lanes);

	dw_write(csi, CSI_PHY_IF_CFG, val);
}

/**
 * dw_mipi_csi_enable - Bring CSI controller out of reset.
 * @csi: Pointer to the CSI controller context.
 */
static void dw_mipi_csi_enable(struct dw_mipi_csi *csi)
{
	if (!csi) {
		pr_err("dw_mipi_csi: null csi pointer in enable\n");
		return;
	}

	dw_write(csi, CSI_RESETN, POWERUP);
}

/**
 * dw_mipi_csi_dphy_enable - Enable and initialize D-PHY.
 * @csi: Pointer to the CSI controller context.
 *
 * This function powers on the external D-PHY, applies control bits,
 * waits for PLL lock and lane stop-state readiness, then completes
 * the PHY startup sequence.
 */
static void dw_mipi_csi_dphy_enable(struct dw_mipi_csi *csi)
{
	u32 val		    = 0;
	u32 stop_state_mask = 0;
	int i;
	int ret = 0;

	if (!csi) {
		pr_err("dw_mipi_csi: null csi pointer in dphy_enable\n");
		return;
	}

	if (!csi->dphy) {
		pr_err("dw_mipi_csi: null dphy pointer in dphy_enable\n");
		return;
	}

	if (!csi->base) {
		pr_err("dw_mipi_csi: null base pointer in dphy_enable\n");
		return;
	}

	if (!csi->dev) {
		pr_err("dw_mipi_csi: null dev pointer in dphy_enable\n");
		return;
	}

	if (csi->lanes == 0 || csi->lanes > 4) {
		pr_err("dw_mipi_csi: invalid lanes number %u in dphy_enable\n", csi->lanes);
		return;
	}

	/* Power on external D-PHY */
	phy_power_on(csi->dphy);

	/* Assert PHY control bits */
	dw_write(csi, CSI_PHY_RSTZ, PHY_ENFORCEPLL | PHY_ENABLECLK | PHY_UNRSTZ | PHY_UNSHUTDOWNZ);

	/* Wait for PHY PLL to lock */
	ret = readl_poll_timeout(csi->base + CSI_PHY_STATUS, val, val & PHY_LOCK, 1000,
				 PHY_STATUS_TIMEOUT_US);
	if (ret) {
		dev_err(csi->dev, "dw_mipi_csi: failed to wait phy lock state\n");
		goto err_power_off;
	}

	dev_info(csi->dev, "dw_mipi_csi: PHY PLL locked, status = 0x%x\n", val);

	/* Generate stop state bitmask for all lanes */
	// If continuous, don't include clock lane
	stop_state_mask = (csi->clk_continuous) ? 0 : PHY_STOP_STATE_CLK_LANE;
	for (i = 0; i < csi->lanes; i++)
		stop_state_mask |= (1 << (4 + 2 * (i + 1)));

	/* Wait for all lanes to enter stop state */
	ret = readl_poll_timeout(csi->base + CSI_PHY_STATUS, val,
				 (val & stop_state_mask) == stop_state_mask, 1000,
				 PHY_STATUS_TIMEOUT_US);
	if (ret) {
		dev_err(csi->dev,
			"dw_mipi_csi: failed to wait phy lane stop state, status = 0x%x\n", val);
		goto err_power_off;
	}

	/* Re-assert control bits and add delay for stability */
	dw_write(csi, CSI_PHY_RSTZ, PHY_ENFORCEPLL | PHY_ENABLECLK | PHY_UNRSTZ | PHY_UNSHUTDOWNZ);

	msleep(500); /* Optional stabilization delay */

	/* Dump PHY status after enable */
	val = dw_read(csi, CSI_PHY_STATUS);
	dev_info(csi->dev, "dw_mipi_csi: DPHY enabled, status = 0x%x\n", val);

	return;

err_power_off:
	phy_power_off(csi->dphy);
}

/**
 * dw_mipi_csi_hal_vpg_init - Initialize CSI VPG (Video Pattern Generator) mode.
 * @csi: Pointer to the CSI controller context.
 * @pattern: Pattern type to be configured (H/V/OFF).
 *
 * This function configures internal VPG mode using fixed VGA timing.
 * It supports horizontal/vertical bar generation or disables the VPG block.
 *
 * Return: 0 on success, negative error code on failure.
 */
int dw_mipi_csi_hal_vpg_init(struct dw_mipi_csi *csi, enum dw_mipi_csi_pattern pattern)
{
	struct csi_mode_info *vpg_mode = NULL;
	int ret			       = 0;
	u32 val			       = 0;
	u32 vpg_en		       = VPG_EN;

	if (!csi) {
		pr_err("dw_mipi_csi: null csi pointer in hal_vpg_init\n");
		return -EINVAL;
	}

	val = dw_read(csi, CSI_VPG_MODE_CFG);

	switch (pattern) {
	case DW_MIPI_CSI_PATTERN_OFF:
		vpg_en = 0;

		break;

	case DW_MIPI_CSI_PATTERN_H:
		/* Set VPG orientation to horizontal bar */
		val |= VPG_ORIENTATION;
		break;

	case DW_MIPI_CSI_PATTERN_V:
		/* Set VPG orientation to vertical bar */
		val &= ~VPG_ORIENTATION;
		break;

	default:
		/* Unknown pattern type, keep default val */
		break;
	}

	/* Reset CSI and PHY blocks */
	dw_mipi_csi_hal_reset(csi);

	/* Set VPG pattern mode config */
	dw_write(csi, CSI_VPG_MODE_CFG, val);

	if (!vpg_en) {
		/* Pattern generation is disabled */
		dw_write(csi, CSI_VPG_CTRL, vpg_en);
		dw_mipi_csi_hal_disable(csi);
		return 0;
	}

	/* Configure standard VGA 640x480@60Hz timing */
	vpg_mode	      = &csi->vpg_mode;
	vpg_mode->width	      = 640;
	vpg_mode->height      = 480;
	vpg_mode->pixel_clock = 25200;

	vpg_mode->hdisplay    = 640;
	vpg_mode->hsync_start = 656;
	vpg_mode->hsync_end   = 752;
	vpg_mode->htotal      = 800;
	vpg_mode->hskew	      = 0;

	vpg_mode->vdisplay    = 480;
	vpg_mode->vsync_start = 490;
	vpg_mode->vsync_end   = 492;
	vpg_mode->vtotal      = 525;

	vpg_mode->refresh_rate = 60;
	vpg_mode->interlaced   = false;

	ret = dw_mipi_csi_hal_init(csi, CSI_VPG_ON);
	if (ret) {
		pr_err("dw_mipi_csi: hal_init failed in hal_vpg_init\n");
		return -EINVAL;
	}

	/* Configure CSI and PHY blocks with pattern generator timing */
	dw_mipi_csi_esc_config(csi);
	dw_mipi_csi_timeout_config(csi, vpg_mode);
	dw_mipi_csi_vpg_pkt_configure(csi);
	dw_mipi_csi_vpg_timing_config(csi, vpg_mode);
	dw_mipi_csi_apply_clk_continuous(csi);

	dw_mipi_csi_dphy_init(csi);
	dw_mipi_csi_dphy_timing_config(csi);
	dw_mipi_csi_dphy_interface_config(csi);

	/* Enable VPG and bring CSI out of reset */
	dw_write(csi, CSI_VPG_CTRL, vpg_en);
	dw_write(csi, CSI_RESETN, POWERUP);
	dw_mipi_csi_dphy_enable(csi);

	dw_mipi_csi_hal_irq_enable(csi);

	return 0;
}

/**
 * dw_mipi_csi_hal_init - Initialize MIPI CSI DPHY configuration.
 * @csi: Pointer to the CSI controller context.
 * @vpg_mode: If true, use VPG mode config; otherwise, use normal mode.
 *
 * This function prepares the default MIPI DPHY config using pixel clock
 * and bus format, validates it against the PHY, and stores the config.
 *
 * Return: 0 on success, negative error code on failure.
 */
int32_t dw_mipi_csi_hal_init(struct dw_mipi_csi *csi, enum dw_vpg_mode vpg_mode)
{
	struct phy_configure_opts_mipi_dphy dphy_cfg;
	struct csi_mode_info *mode   = NULL;
	int32_t ret		     = 0;
	unsigned long effective_pclk = 0;

	if (!csi) {
		pr_err("dw_mipi_csi: null csi pointer in hal_init\n");
		return -EINVAL;
	}

	if (!csi->dphy) {
		pr_err("dw_mipi_csi: null dphy pointer in hal_init\n");
		return -EINVAL;
	}

	pr_info("dw_mipi_csi: vpg_mode = %d\n", vpg_mode);

	/* Select mode config based on VPG enable */
	mode = vpg_mode ? &csi->vpg_mode : &csi->mode;

	if (csi->timing_margin_enable)
		effective_pclk = mode->pixel_clock * 11 / 10;
	else
		effective_pclk = mode->pixel_clock;

	/* Get default PHY timing parameters */
	phy_mipi_dphy_get_default_config(effective_pclk * KHZ, dw_bus_fmt_to_bpp(csi->bus_fmt),
					 csi->lanes, &dphy_cfg);

	/* Override HS clock rate if needed */
	csi_apply_hsclk_override(csi, &dphy_cfg);

	/* Validate config with physical layer */
	ret = phy_validate(csi->dphy, PHY_MODE_MIPI_DPHY, 0, (union phy_configure_opts *)&dphy_cfg);
	if (ret) {
		pr_err("phy_validate fail, ret %d\n", ret);
		return -EINVAL;
	}

	/* Save configuration and compute link rate in KHz */
	memcpy(&csi->dphy_cfg, &dphy_cfg, sizeof(dphy_cfg));
	csi->lane_link_rate = dphy_cfg.hs_clk_rate / KHZ;

	/* now apply user overrides if enabled */
	dw_mipi_csi_apply_dphy_timing_overrides(csi);

	return ret;
}

/**
 * dw_mipi_csi_hal_power_on - Enable CSI clocks and power domain.
 * @csi: Pointer to the CSI controller context.
 *
 * Return: 0 on success, negative error code on failure.
 */
int dw_mipi_csi_hal_power_on(struct dw_mipi_csi *csi)
{
	int32_t ret = 0;

	if (!csi || !csi->dev)
		return -EINVAL;

	pm_runtime_resume_and_get(csi->dev);

	if (csi->pclk) {
		ret = clk_prepare_enable(csi->pclk);
		if (ret) {
			pm_runtime_put(csi->dev);
			goto exit;
		}
	}

exit:
	return ret;
}

/**
 * dw_mipi_csi_hal_power_off - Disable CSI clocks and release power domain.
 * @csi: Pointer to the CSI controller context.
 */
void dw_mipi_csi_hal_power_off(struct dw_mipi_csi *csi)
{
	if (!csi || !csi->dev)
		return;

	if (csi->pclk)
		clk_disable_unprepare(csi->pclk);

	pm_runtime_put(csi->dev);
}

/**
 * dw_mipi_csi_hal_set_mode - Configure MIPI CSI controller with specified mode.
 * @csi: Pointer to CSI controller context.
 * @mode_info: Pointer to mode configuration structure.
 *
 * This function sets up the MIPI CSI controller including ESC settings,
 * timeout parameters, packet configuration, DPHY settings, and finally
 * enables both the controller and DPHY logic.
 *
 * Return: 0 on success, negative error code on failure.
 */
int dw_mipi_csi_hal_set_mode(struct dw_mipi_csi *csi, const struct csi_mode_info *mode_info)
{
	if (!csi || !mode_info)
		return -EINVAL;

	/* Reset CSI controller */
	dw_write(csi, CSI_RESETN, RESET);

	/* Configure escape clock parameters */
	dw_mipi_csi_esc_config(csi);

	/* Configure timeout settings */
	dw_mipi_csi_timeout_config(csi, mode_info);

	/* Configure IPI packet mode based on resolution and format */
	dw_mipi_csi_ipi_pkt_configure(csi);

	/* Configure IPI timing parameters (horizontal/vertical) */
	dw_mipi_csi_ipi_timing_config(csi, mode_info);

	dw_mipi_csi_apply_clk_continuous(csi);

	/* Initialize the DPHY interface hardware */
	dw_mipi_csi_dphy_init(csi);

	/* Set DPHY timing parameters */
	dw_mipi_csi_dphy_timing_config(csi);

	/* Set DPHY interface protocol settings */
	dw_mipi_csi_dphy_interface_config(csi);

	/* Enable the CSI controller */
	dw_mipi_csi_enable(csi);

	/* Enable the DPHY */
	dw_mipi_csi_dphy_enable(csi);

	/* Enable IRQ */
	dw_mipi_csi_hal_irq_enable(csi);

	return 0;
}

/**
 * dw_mipi_csi_hal_disable - Disable the MIPI CSI controller and PHY.
 * @csi: Pointer to the CSI controller context.
 *
 * This function resets the CSI controller and disables the PHY and
 * low-power clock control.
 */
void dw_mipi_csi_hal_disable(struct dw_mipi_csi *csi)
{
	if (!csi)
		return;

	dw_write(csi, CSI_LPCLK_CTRL, 0);

	/* Reset PHY */
	dw_write(csi, CSI_PHY_RSTZ, PHY_RSTZ);

	/* Reset CSI controller */
	dw_write(csi, CSI_RESETN, RESET);

	dw_mipi_csi_hal_irq_disable(csi);
}

/**
 * dw_mipi_csi_hal_reset - Perform CSI controller and PHY reset.
 * @csi: Pointer to the CSI controller context.
 *
 * This function performs a hardware-level reset on both the D-PHY and
 * the main CSI controller logic, including clock manager reset.
 */
void dw_mipi_csi_hal_reset(struct dw_mipi_csi *csi)
{
	if (!csi)
		return;

	/* Reset D-PHY block */
	dw_write(csi, CSI_PHY_RSTZ, PHY_RSTZ);

	/* Reset CSI controller */
	dw_write(csi, CSI_RESETN, RESET);

	/* Reset CSI clock manager */
	dw_write(csi, CSI_CLKMGR_CFG, RESET);
}

/**
 * dw_mipi_csi_hal_irq_enable - Enable interrupt masks for CSI blocks.
 * @csi: Pointer to the CSI controller context.
 *
 * This function iterates over the interrupt mask descriptors and enables
 * the required bits by writing the unmask value to each IRQ control register.
 */
void dw_mipi_csi_hal_irq_enable(struct dw_mipi_csi *csi)
{
	u32 i;

	if (!csi || !csi->base)
		return;

	for (i = 0; i < ARRAY_SIZE(csi_irq_masks); i++) {
		u32 val;

		/* Read current interrupt mask register */
		val = dw_read(csi, csi_irq_masks[i].reg_offset);

		/* Clear all bits first */
		val &= ~CSI_IRQ_MASK_ALL_BITS;

		/* Apply unmask bits */
		val |= csi_irq_masks[i].unmask_val;

		/* Write back updated mask value */
		dw_write(csi, csi_irq_masks[i].reg_offset, val);
	}
}

/**
 * dw_mipi_csi_hal_irq_disable - Mask all CSI interrupt sources.
 * @csi: Pointer to the CSI controller context.
 */
void dw_mipi_csi_hal_irq_disable(struct dw_mipi_csi *csi)
{
	u32 i;

	if (!csi || !csi->base)
		return;

	for (i = 0; i < ARRAY_SIZE(csi_irq_masks); i++) {
		u32 val;

		/* Read current IRQ mask register */
		val = dw_read(csi, csi_irq_masks[i].reg_offset);

		/* Clear all interrupt mask bits */
		val &= ~CSI_IRQ_MASK_ALL_BITS;

		/* Write back to disable all IRQs */
		dw_write(csi, csi_irq_masks[i].reg_offset, val);
	}
}

/**
 * dw_mipi_csi_hal_irq_handle - Handle CSI interrupt events.
 * @csi: Pointer to the CSI controller context.
 *
 * This function reads the main interrupt status register (auto-cleared)
 * and dispatches handlers for each sub-block that raised an interrupt.
 */
void dw_mipi_csi_hal_irq_handle(struct dw_mipi_csi *csi)
{
	u32 status;

	if (!csi || !csi->base)
		return;

	/* Read main interrupt status (clear-on-read) */
	status = dw_read(csi, CSI_INT_ST_MAIN);
	if (!status)
		return;

	/* VPG: Video Pattern Generator interrupt */
	if (status & BIT(0)) {
		u32 vpg = dw_read(csi, CSI_INT_ST_VPG);
		dw_mipi_csi_log_irq_fields(csi, vpg, vpg_irq_fields, ARRAY_SIZE(vpg_irq_fields));
	}

	/* IDI: Image Data Interface interrupt */
	if (status & BIT(1)) {
		u32 idi = dw_read(csi, CSI_INT_ST_IDI);
		dw_mipi_csi_log_irq_fields(csi, idi, idi_irq_fields, ARRAY_SIZE(idi_irq_fields));
	}

	/* IPI: Image Packet Interface interrupt (IPI0~IPI4) */
	if (status & BIT(2)) {
		u32 ipi = dw_read(csi, CSI_INT_ST_IPI);
		dw_mipi_csi_log_irq_fields(csi, ipi, ipi_irq_fields, ARRAY_SIZE(ipi_irq_fields));
	}

	/* PHY: Physical layer (D-PHY) interrupt */
	if (status & BIT(3)) {
		u32 phy = dw_read(csi, CSI_INT_ST_PHY);
		dw_mipi_csi_log_irq_fields(csi, phy, phy_irq_fields, ARRAY_SIZE(phy_irq_fields));
	}

	/* MT_IPI: Multi-transmission IPI interrupt */
	if (status & BIT(4)) {
		u32 mt_ipi = dw_read(csi, CSI_INT_ST_MT_IPI);
		dw_mipi_csi_log_irq_fields(csi, mt_ipi, mt_ipi_irq_fields,
					   ARRAY_SIZE(mt_ipi_irq_fields));
	}
}
