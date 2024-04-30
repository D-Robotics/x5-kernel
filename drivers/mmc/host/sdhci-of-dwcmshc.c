// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Synopsys DesignWare Cores Mobile Storage Host Controller
 *
 * Copyright (C) 2018 Synaptics Incorporated
 *
 * Author: Jisheng Zhang <jszhang@kernel.org>
 */

#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/reset.h>
#include <linux/sizes.h>
#include <linux/gpio/consumer.h>

#include "sdhci-pltfm.h"

#define SDHCI_DWCMSHC_ARG2_STUFF	GENMASK(31, 16)

/* DWCMSHC specific Mode Select value */
#define DWCMSHC_CTRL_HS400		0x7

/* DWC IP vendor area 1 pointer */
#define DWCMSHC_P_VENDOR_AREA1		0xe8
#define DWCMSHC_AREA1_MASK		GENMASK(11, 0)
/* Offset inside the  vendor area 1 */
#define DWCMSHC_HOST_CTRL3		0x8
#define DWCMSHC_EMMC_CONTROL		0x2c
#define DWCMSHC_CARD_IS_EMMC		BIT(0)
#define DWCMSHC_ENHANCED_STROBE		BIT(8)
#define DWCMSHC_EMMC_ATCTRL		0x40

/* Rockchip specific Registers */
#define DWCMSHC_EMMC_DLL_CTRL		0x800
#define DWCMSHC_EMMC_DLL_RXCLK		0x804
#define DWCMSHC_EMMC_DLL_TXCLK		0x808
#define DWCMSHC_EMMC_DLL_STRBIN		0x80c
#define DECMSHC_EMMC_DLL_CMDOUT		0x810
#define DWCMSHC_EMMC_DLL_STATUS0	0x840
#define DWCMSHC_EMMC_DLL_START		BIT(0)
#define DWCMSHC_EMMC_DLL_LOCKED		BIT(8)
#define DWCMSHC_EMMC_DLL_TIMEOUT	BIT(9)
#define DWCMSHC_EMMC_DLL_RXCLK_SRCSEL	29
#define DWCMSHC_EMMC_DLL_START_POINT	16
#define DWCMSHC_EMMC_DLL_INC		8
#define DWCMSHC_EMMC_DLL_DLYENA		BIT(27)
#define DLL_TXCLK_TAPNUM_DEFAULT	0x10
#define DLL_TXCLK_TAPNUM_90_DEGREES	0xA
#define DLL_TXCLK_TAPNUM_FROM_SW	BIT(24)
#define DLL_STRBIN_TAPNUM_DEFAULT	0x8
#define DLL_STRBIN_TAPNUM_FROM_SW	BIT(24)
#define DLL_STRBIN_DELAY_NUM_SEL	BIT(26)
#define DLL_STRBIN_DELAY_NUM_OFFSET	16
#define DLL_STRBIN_DELAY_NUM_DEFAULT	0x16
#define DLL_RXCLK_NO_INVERTER		1
#define DLL_RXCLK_INVERTER		0
#define DLL_CMDOUT_TAPNUM_90_DEGREES	0x8
#define DLL_CMDOUT_TAPNUM_FROM_SW	BIT(24)
#define DLL_CMDOUT_SRC_CLK_NEG		BIT(28)
#define DLL_CMDOUT_EN_SRC_CLK_NEG	BIT(29)

#define DLL_LOCK_WO_TMOUT(x) \
	((((x) & DWCMSHC_EMMC_DLL_LOCKED) == DWCMSHC_EMMC_DLL_LOCKED) && \
	(((x) & DWCMSHC_EMMC_DLL_TIMEOUT) == 0))
#define RK35xx_MAX_CLKS 3

#define BOUNDARY_OK(addr, len) \
	((addr | (SZ_128M - 1)) == ((addr + len - 1) | (SZ_128M - 1)))

/* D-Robotics Sunrise5 specific macros */
#define X5_MAX_CLKS 2
#define X5_MST_DLL_OFFSET 0x0
#define X5_SLV_DLL_OFFSET 0x4
#define X5_DLL_OBS0_OFFSET 0xC
#define X5_DLL_PHASE_OFFSET (8u)

#define X5_EMMC_MST_DLL_DEFAULT 0x3AA40004
#define X5_EMMC_SLV_DLL_DEFAULT 0x00808080
#define X5_SD_DLL_DEFAULT 0x3AA40004


enum dwcmshc_rk_type {
	DWCMSHC_RK3568,
	DWCMSHC_RK3588,
};

struct rk35xx_priv {
	/* Rockchip specified optional clocks */
	struct clk_bulk_data rockchip_clks[RK35xx_MAX_CLKS];
	struct reset_control *reset;
	enum dwcmshc_rk_type devtype;
	u8 txclk_tapnum;
};

enum dwcmshc_x5_card_type {
	DWCMSCH_X5_EMMC,
	DWCMSCH_X5_SD
};

struct x5_priv {
	int mmc_fixed_voltage;
	struct clk	*card_clk;
	struct clk_bulk_data x5_clks[X5_MAX_CLKS];
	struct gpio_desc *voltage_gpio;
	struct gpio_desc *power_gpio;
	struct reset_control *reset;
	u8 mshc_ctrl_val;
	enum dwcmshc_x5_card_type card_type;
	void __iomem *dll_ctrl_base;
	int current_dll;
};

struct dwcmshc_priv {
	struct clk	*bus_clk;
	int vendor_specific_area1; /* P_VENDOR_SPECIFIC_AREA reg */
	void *priv; /* pointer to SoC private stuff */
};

/*
 * If DMA addr spans 128MB boundary, we split the DMA transfer into two
 * so that each DMA transfer doesn't exceed the boundary.
 */
static void dwcmshc_adma_write_desc(struct sdhci_host *host, void **desc,
				    dma_addr_t addr, int len, unsigned int cmd)
{
	int tmplen, offset;

	if (likely(!len || BOUNDARY_OK(addr, len))) {
		sdhci_adma_write_desc(host, desc, addr, len, cmd);
		return;
	}

	offset = addr & (SZ_128M - 1);
	tmplen = SZ_128M - offset;
	sdhci_adma_write_desc(host, desc, addr, tmplen, cmd);

	addr += tmplen;
	len -= tmplen;
	sdhci_adma_write_desc(host, desc, addr, len, cmd);
}

static unsigned int dwcmshc_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	if (pltfm_host->clk)
		return sdhci_pltfm_clk_get_max_clock(host);
	else
		return pltfm_host->clock;
}

static void dwcmshc_check_auto_cmd23(struct mmc_host *mmc,
				     struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);

	/*
	 * No matter V4 is enabled or not, ARGUMENT2 register is 32-bit
	 * block count register which doesn't support stuff bits of
	 * CMD23 argument on dwcmsch host controller.
	 */
	if (mrq->sbc && (mrq->sbc->arg & SDHCI_DWCMSHC_ARG2_STUFF))
		host->flags &= ~SDHCI_AUTO_CMD23;
	else
		host->flags |= SDHCI_AUTO_CMD23;
}

static void dwcmshc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	dwcmshc_check_auto_cmd23(mmc, mrq);

	sdhci_request(mmc, mrq);
}

static void dwcmshc_set_uhs_signaling(struct sdhci_host *host,
				      unsigned int timing)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	u16 ctrl, ctrl_2;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	if ((timing == MMC_TIMING_MMC_HS200) ||
	    (timing == MMC_TIMING_UHS_SDR104))
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
	else if (timing == MMC_TIMING_UHS_SDR12)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
	else if ((timing == MMC_TIMING_UHS_SDR25) ||
		 (timing == MMC_TIMING_MMC_HS))
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
	else if (timing == MMC_TIMING_UHS_SDR50)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
	else if ((timing == MMC_TIMING_UHS_DDR50) ||
		 (timing == MMC_TIMING_MMC_DDR52))
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
	else if (timing == MMC_TIMING_MMC_HS400) {
		/* set CARD_IS_EMMC bit to enable Data Strobe for HS400 */
		ctrl = sdhci_readw(host, priv->vendor_specific_area1 + DWCMSHC_EMMC_CONTROL);
		ctrl |= DWCMSHC_CARD_IS_EMMC;
		sdhci_writew(host, ctrl, priv->vendor_specific_area1 + DWCMSHC_EMMC_CONTROL);

		ctrl_2 |= DWCMSHC_CTRL_HS400;
	}

	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);
}

static void dwcmshc_hs400_enhanced_strobe(struct mmc_host *mmc,
					  struct mmc_ios *ios)
{
	u32 vendor;
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	int reg = priv->vendor_specific_area1 + DWCMSHC_EMMC_CONTROL;

	vendor = sdhci_readl(host, reg);
	if (ios->enhanced_strobe)
		vendor |= DWCMSHC_ENHANCED_STROBE;
	else
		vendor &= ~DWCMSHC_ENHANCED_STROBE;

	sdhci_writel(host, vendor, reg);
}

static void x5_sdhci_sys_reset(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct x5_priv *priv = dwc_priv->priv;

	reset_control_assert(priv->reset);
	udelay(10);
	reset_control_deassert(priv->reset);
}

static unsigned int x5_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct x5_priv *x5_priv = dwc_priv->priv;

	return clk_get_rate(x5_priv->card_clk);
}

static void x5_sdhci_reset(struct sdhci_host *host, u8 mask)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct x5_priv *priv = dwc_priv->priv;

	if (mask & SDHCI_RESET_ALL && priv->reset) {
		x5_sdhci_sys_reset(host);
	}

	sdhci_reset(host, mask);
}

static int x5_send_tuning(struct sdhci_host *host, u32 opcode)
{
	unsigned int ctrl;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	if (host->mmc->ios.bus_width == MMC_BUS_WIDTH_8)
		sdhci_writew(host, SDHCI_MAKE_BLKSZ(host->sdma_boundary, 128),
			     SDHCI_BLOCK_SIZE);
	else
		sdhci_writew(host, SDHCI_MAKE_BLKSZ(host->sdma_boundary, 64),
			     SDHCI_BLOCK_SIZE);

	sdhci_writew(host, 1, SDHCI_BLOCK_COUNT);
	ctrl = sdhci_readw(host, SDHCI_TRANSFER_MODE);
	ctrl |= SDHCI_TRNS_READ;
	sdhci_writew(host, ctrl, SDHCI_TRANSFER_MODE);

	spin_unlock_irqrestore(&host->lock, flags);

	return mmc_send_tuning(host->mmc, opcode, NULL);
}

#define X5_TUNING_MAX 128
#define TUNING_ITERATION_TO_PHASE(i)	(i)
static int x5_set_dll(struct sdhci_host *host, int degrees)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct x5_priv *priv = dwc_priv->priv;
	u32 dll_reg_val;
	u16 sdhci_clk_reg_val;
	ktime_t timeout;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	sdhci_clk_reg_val = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	sdhci_clk_reg_val &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, sdhci_clk_reg_val, SDHCI_CLOCK_CONTROL);

	dll_reg_val = ioread32(priv->dll_ctrl_base + X5_SLV_DLL_OFFSET);
	dll_reg_val &= 0xFFFF80FF;
	dll_reg_val |= (degrees << X5_DLL_PHASE_OFFSET);
	iowrite32(dll_reg_val, priv->dll_ctrl_base + X5_SLV_DLL_OFFSET);

	timeout = ktime_add_ms(ktime_get(), 150);
	while (1) {
		bool timedout = ktime_after(ktime_get(), timeout);

		dll_reg_val = ioread32(priv->dll_ctrl_base + X5_DLL_OBS0_OFFSET);
		if (((dll_reg_val & 0x7F00) >> X5_DLL_PHASE_OFFSET) == degrees)
			break;
		if (timedout) {
			pr_err("%s: execute tuning dll obs never stabilized.\n",
			       mmc_hostname(host->mmc));
			return -1;
		}
		udelay(10);
	}

	sdhci_clk_reg_val |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, sdhci_clk_reg_val, SDHCI_CLOCK_CONTROL);
	priv->current_dll = degrees;

	spin_unlock_irqrestore(&host->lock, flags);
	return 0;
}

static int dwcmshc_x5_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct x5_priv *priv = dwc_priv->priv;
	struct mmc_host *mmc = host->mmc;
	int ret = 0;
	int i;
	bool v, prev_v = 0, first_v;
	struct range_t {
		int start;
		int end;	/* inclusive */
	};
	struct range_t *ranges;
	unsigned int range_count = 0;
	int longest_range_len = -1;
	int longest_range = -1;
	int middle_phase;

	dev_dbg(mmc_dev(mmc), "do dwcmshc_x5_execute_tuning\n");
	ranges = kmalloc_array(X5_TUNING_MAX / 2 + 1, sizeof(*ranges), GFP_KERNEL);
	if (!ranges)
		return -ENOMEM;

	/* Try each phase and extract good ranges */
	for (i = 0; i < X5_TUNING_MAX; i++) {
		x5_set_dll(host, TUNING_ITERATION_TO_PHASE(i));

		v = !x5_send_tuning(host, opcode);

		if (i == 0)
			first_v = v;

		if ((!prev_v) && v) {
			range_count++;
			ranges[range_count - 1].start = i;
		}

		if (v) {
			ranges[range_count - 1].end = i;
		} else if (i < X5_TUNING_MAX - 2) {
			/*
			 * No need to check too close to an invalid
			 * one since testing bad phases is slow. Skip
			 * the adjacent phase but always test the last phase.
			 */
			i++;
		}

		prev_v = v;
	}

	if (range_count == 0) {
		dev_err(mmc_dev(mmc), "All sample phases bad!");
		ret = -EIO;
		goto free;
	}

	/* wrap around case, merge the end points */
	if ((range_count > 1) && first_v && v) {
		ranges[0].start = ranges[range_count - 1].start;
		range_count--;
	}

	/* Find the longest range */
	for (i = 0; i < range_count; i++) {
		int len = (ranges[i].end - ranges[i].start + 1);

		if (len < 0)
			len += X5_TUNING_MAX;

		if (longest_range_len < len) {
			longest_range_len = len;
			longest_range = i;
		}
	}

	middle_phase = ranges[longest_range].start + longest_range_len / 2;
	middle_phase %= X5_TUNING_MAX;

	x5_set_dll(host, TUNING_ITERATION_TO_PHASE(middle_phase));
	dev_dbg(mmc_dev(mmc),
		"Got longest(%d) sample range[%d,%d], current phase:%d\n",
		longest_range_len,
		TUNING_ITERATION_TO_PHASE(ranges[longest_range].start),
		TUNING_ITERATION_TO_PHASE(ranges[longest_range].end),
		priv->current_dll);

free:
	kfree(ranges);
	/* set retuning period to enable retuning*/
	mmc->retune_period = 5;
	return ret;
}

static void dwcmshc_x5_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct x5_priv *x5_priv = dwc_priv->priv;
	u16 sdhci_clk_reg_val;

	host->mmc->actual_clock = 0;
	clk_disable_unprepare(x5_priv->card_clk);
	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);
	if (clock == 0) {
		clk_prepare_enable(x5_priv->card_clk);
		return;
	}

	sdhci_clk_reg_val = sdhci_calc_clk(host, clock, &host->mmc->actual_clock);
	sdhci_writew(host, sdhci_clk_reg_val, SDHCI_CLOCK_CONTROL);
	clk_prepare_enable(x5_priv->card_clk);
	sdhci_enable_clk(host, sdhci_clk_reg_val);

	sdhci_writeb(host, x5_priv->mshc_ctrl_val,
				 dwc_priv->vendor_specific_area1 + DWCMSHC_HOST_CTRL3);

	/* Make sure if dts configured fixed-emmc-drive-type,
	 * sdhci controller will also be configured for the same
	 * fixed-emmc-drive-type on Sunrise5 SoC.
	 */
	dev_dbg(mmc_dev(host->mmc), "Get fixed-drv-type: %d\n", host->mmc->fixed_drv_type);
	if (host->mmc->fixed_drv_type >= MMC_SET_DRIVER_TYPE_B
		&& host->mmc->fixed_drv_type <= MMC_SET_DRIVER_TYPE_D) {
		host->mmc->ios.drv_type = host->mmc->fixed_drv_type;
		dev_dbg(mmc_dev(host->mmc), "Set ios.drv_type:%d\n", host->mmc->ios.drv_type);
	}
}

static int dwcmshc_x5_clk_init(struct sdhci_host *host, struct dwcmshc_priv *dwc_priv)
{
	struct x5_priv *priv = dwc_priv->priv;
	int err;

	priv->x5_clks[0].id = "axi";
	priv->x5_clks[1].id = "timer";
	err = devm_clk_bulk_get_optional(mmc_dev(host->mmc), X5_MAX_CLKS,
					 priv->x5_clks);
	if (err) {
		dev_err(mmc_dev(host->mmc), "failed to get clocks %d\n", err);
		return err;
	}

	err = clk_bulk_prepare_enable(X5_MAX_CLKS, priv->x5_clks);
	if (err) {
		dev_err(mmc_dev(host->mmc), "failed to enable clocks %d\n", err);
		return err;
	}

	priv->card_clk = devm_clk_get(mmc_dev(host->mmc), "card");
	if (!IS_ERR(priv->card_clk))
		clk_prepare_enable(priv->card_clk);

	return 0;
}

static void dwcmshc_rk3568_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct rk35xx_priv *priv = dwc_priv->priv;
	u8 txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT;
	u32 extra, reg;
	int err;

	host->mmc->actual_clock = 0;

	if (clock == 0) {
		/* Disable interface clock at initial state. */
		sdhci_set_clock(host, clock);
		return;
	}

	/* Rockchip platform only support 375KHz for identify mode */
	if (clock <= 400000)
		clock = 375000;

	err = clk_set_rate(pltfm_host->clk, clock);
	if (err)
		dev_err(mmc_dev(host->mmc), "fail to set clock %d", clock);

	sdhci_set_clock(host, clock);

	/* Disable cmd conflict check */
	reg = dwc_priv->vendor_specific_area1 + DWCMSHC_HOST_CTRL3;
	extra = sdhci_readl(host, reg);
	extra &= ~BIT(0);
	sdhci_writel(host, extra, reg);

	if (clock <= 52000000) {
		/* Disable DLL and reset both of sample and drive clock */
		sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_CTRL);
		sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_RXCLK);
		sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_TXCLK);
		sdhci_writel(host, 0, DECMSHC_EMMC_DLL_CMDOUT);
		/*
		 * Before switching to hs400es mode, the driver will enable
		 * enhanced strobe first. PHY needs to configure the parameters
		 * of enhanced strobe first.
		 */
		extra = DWCMSHC_EMMC_DLL_DLYENA |
			DLL_STRBIN_DELAY_NUM_SEL |
			DLL_STRBIN_DELAY_NUM_DEFAULT << DLL_STRBIN_DELAY_NUM_OFFSET;
		sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_STRBIN);
		return;
	}

	/* Reset DLL */
	sdhci_writel(host, BIT(1), DWCMSHC_EMMC_DLL_CTRL);
	udelay(1);
	sdhci_writel(host, 0x0, DWCMSHC_EMMC_DLL_CTRL);

	/*
	 * We shouldn't set DLL_RXCLK_NO_INVERTER for identify mode but
	 * we must set it in higher speed mode.
	 */
	extra = DWCMSHC_EMMC_DLL_DLYENA;
	if (priv->devtype == DWCMSHC_RK3568)
		extra |= DLL_RXCLK_NO_INVERTER << DWCMSHC_EMMC_DLL_RXCLK_SRCSEL;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_RXCLK);

	/* Init DLL settings */
	extra = 0x5 << DWCMSHC_EMMC_DLL_START_POINT |
		0x2 << DWCMSHC_EMMC_DLL_INC |
		DWCMSHC_EMMC_DLL_START;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_CTRL);
	err = readl_poll_timeout(host->ioaddr + DWCMSHC_EMMC_DLL_STATUS0,
				 extra, DLL_LOCK_WO_TMOUT(extra), 1,
				 500 * USEC_PER_MSEC);
	if (err) {
		dev_err(mmc_dev(host->mmc), "DLL lock timeout!\n");
		return;
	}

	extra = 0x1 << 16 | /* tune clock stop en */
		0x2 << 17 | /* pre-change delay */
		0x3 << 19;  /* post-change delay */
	sdhci_writel(host, extra, dwc_priv->vendor_specific_area1 + DWCMSHC_EMMC_ATCTRL);

	if (host->mmc->ios.timing == MMC_TIMING_MMC_HS200 ||
	    host->mmc->ios.timing == MMC_TIMING_MMC_HS400)
		txclk_tapnum = priv->txclk_tapnum;

	if ((priv->devtype == DWCMSHC_RK3588) && host->mmc->ios.timing == MMC_TIMING_MMC_HS400) {
		txclk_tapnum = DLL_TXCLK_TAPNUM_90_DEGREES;

		extra = DLL_CMDOUT_SRC_CLK_NEG |
			DLL_CMDOUT_EN_SRC_CLK_NEG |
			DWCMSHC_EMMC_DLL_DLYENA |
			DLL_CMDOUT_TAPNUM_90_DEGREES |
			DLL_CMDOUT_TAPNUM_FROM_SW;
		sdhci_writel(host, extra, DECMSHC_EMMC_DLL_CMDOUT);
	}

	extra = DWCMSHC_EMMC_DLL_DLYENA |
		DLL_TXCLK_TAPNUM_FROM_SW |
		DLL_RXCLK_NO_INVERTER << DWCMSHC_EMMC_DLL_RXCLK_SRCSEL |
		txclk_tapnum;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_TXCLK);

	extra = DWCMSHC_EMMC_DLL_DLYENA |
		DLL_STRBIN_TAPNUM_DEFAULT |
		DLL_STRBIN_TAPNUM_FROM_SW;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_STRBIN);
}

static void rk35xx_sdhci_reset(struct sdhci_host *host, u8 mask)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct rk35xx_priv *priv = dwc_priv->priv;

	if (mask & SDHCI_RESET_ALL && priv->reset) {
		reset_control_assert(priv->reset);
		udelay(1);
		reset_control_deassert(priv->reset);
	}

	sdhci_reset(host, mask);
}

static const struct sdhci_ops sdhci_dwcmshc_ops = {
	.set_clock		= sdhci_set_clock,
	.set_bus_width		= sdhci_set_bus_width,
	.set_uhs_signaling	= dwcmshc_set_uhs_signaling,
	.get_max_clock		= dwcmshc_get_max_clock,
	.reset			= sdhci_reset,
	.adma_write_desc	= dwcmshc_adma_write_desc,
};

static const struct sdhci_ops sdhci_dwcmshc_rk35xx_ops = {
	.set_clock		= dwcmshc_rk3568_set_clock,
	.set_bus_width		= sdhci_set_bus_width,
	.set_uhs_signaling	= dwcmshc_set_uhs_signaling,
	.get_max_clock		= sdhci_pltfm_clk_get_max_clock,
	.reset			= rk35xx_sdhci_reset,
	.adma_write_desc	= dwcmshc_adma_write_desc,
};

static const struct sdhci_ops sdhci_dwcmshc_x5_sd_ops = {
	.set_clock		= dwcmshc_x5_set_clock,
	.set_bus_width		= sdhci_set_bus_width,
	.set_uhs_signaling	= dwcmshc_set_uhs_signaling,
	.get_max_clock		= x5_get_max_clock,
	.reset			= sdhci_reset,
	.adma_write_desc	= dwcmshc_adma_write_desc,
	.platform_execute_tuning = dwcmshc_x5_execute_tuning,
};

static const struct sdhci_ops sdhci_dwcmshc_x5_emmc_ops = {
	.set_clock		= dwcmshc_x5_set_clock,
	.set_bus_width		= sdhci_set_bus_width,
	.set_uhs_signaling	= dwcmshc_set_uhs_signaling,
	.get_max_clock		= x5_get_max_clock,
	.reset			= x5_sdhci_reset,
	.adma_write_desc	= dwcmshc_adma_write_desc,
	.platform_execute_tuning = dwcmshc_x5_execute_tuning,
};

static const struct sdhci_pltfm_data sdhci_dwcmshc_pdata = {
	.ops = &sdhci_dwcmshc_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

static const struct sdhci_pltfm_data sdhci_dwcmshc_x5_sd_pdata = {
	.ops = &sdhci_dwcmshc_x5_sd_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

static const struct sdhci_pltfm_data sdhci_dwcmshc_x5_emmc_pdata = {
	.ops = &sdhci_dwcmshc_x5_emmc_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

#ifdef CONFIG_ACPI
static const struct sdhci_pltfm_data sdhci_dwcmshc_bf3_pdata = {
	.ops = &sdhci_dwcmshc_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
		   SDHCI_QUIRK2_ACMD23_BROKEN,
};
#endif

static const struct sdhci_pltfm_data sdhci_dwcmshc_rk35xx_pdata = {
	.ops = &sdhci_dwcmshc_rk35xx_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN |
		  SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
		   SDHCI_QUIRK2_CLOCK_DIV_ZERO_BROKEN,
};

static int dwcmshc_x5_start_signal_voltage_switch(struct mmc_host *mmc,
						  struct mmc_ios *ios)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct x5_priv *x5_priv = priv->priv;
	int state = 0;

	if (x5_priv->mmc_fixed_voltage == 3300)
		ios->signal_voltage = MMC_SIGNAL_VOLTAGE_330;
	else if (x5_priv->mmc_fixed_voltage == 1800)
		ios->signal_voltage = MMC_SIGNAL_VOLTAGE_180;

	if(!IS_ERR_OR_NULL(x5_priv->voltage_gpio)) {
		if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330)
			state = 0;
		else if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			state = 1;

		dev_info(mmc_dev(host->mmc), "Set io-voltage gpio %s\n", state ? "Active" : "Inactive");
		gpiod_set_value_cansleep(x5_priv->voltage_gpio, state);
	}

	return sdhci_start_signal_voltage_switch(mmc, ios);
}

static void dwcmshc_x5_toggle_sd_power(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct x5_priv *x5_priv = priv->priv;
	u16 toggle_interval_us = 1000;

	if (!IS_ERR_OR_NULL(x5_priv->power_gpio)) {
		dev_dbg(mmc_dev(mmc), "Toggling power-gpio with interval %u us\n",
				toggle_interval_us);
		gpiod_set_value_cansleep(x5_priv->power_gpio, 0);
		udelay(toggle_interval_us);
		gpiod_set_value_cansleep(x5_priv->power_gpio, 1);
	}
	return;
}

static int dwcmshc_rk35xx_init(struct sdhci_host *host, struct dwcmshc_priv *dwc_priv)
{
	int err;
	struct rk35xx_priv *priv = dwc_priv->priv;

	priv->reset = devm_reset_control_array_get_optional_exclusive(mmc_dev(host->mmc));
	if (IS_ERR(priv->reset)) {
		err = PTR_ERR(priv->reset);
		dev_err(mmc_dev(host->mmc), "failed to get reset control %d\n", err);
		return err;
	}

	priv->rockchip_clks[0].id = "axi";
	priv->rockchip_clks[1].id = "block";
	priv->rockchip_clks[2].id = "timer";
	err = devm_clk_bulk_get_optional(mmc_dev(host->mmc), RK35xx_MAX_CLKS,
					 priv->rockchip_clks);
	if (err) {
		dev_err(mmc_dev(host->mmc), "failed to get clocks %d\n", err);
		return err;
	}

	err = clk_bulk_prepare_enable(RK35xx_MAX_CLKS, priv->rockchip_clks);
	if (err) {
		dev_err(mmc_dev(host->mmc), "failed to enable clocks %d\n", err);
		return err;
	}

	if (of_property_read_u8(mmc_dev(host->mmc)->of_node, "rockchip,txclk-tapnum",
				&priv->txclk_tapnum))
		priv->txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT;

	/* Disable cmd conflict check */
	sdhci_writel(host, 0x0, dwc_priv->vendor_specific_area1 + DWCMSHC_HOST_CTRL3);
	/* Reset previous settings */
	sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_TXCLK);
	sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_STRBIN);

	return 0;
}

static void dwcmshc_rk35xx_postinit(struct sdhci_host *host, struct dwcmshc_priv *dwc_priv)
{
	/*
	 * Don't support highspeed bus mode with low clk speed as we
	 * cannot use DLL for this condition.
	 */
	if (host->mmc->f_max <= 52000000) {
		dev_info(mmc_dev(host->mmc), "Disabling HS200/HS400, frequency too low (%d)\n",
			 host->mmc->f_max);
		host->mmc->caps2 &= ~(MMC_CAP2_HS200 | MMC_CAP2_HS400);
		host->mmc->caps &= ~(MMC_CAP_3_3V_DDR | MMC_CAP_1_8V_DDR);
	}
}

static const struct of_device_id sdhci_dwcmshc_dt_ids[] = {
	{
		.compatible = "rockchip,rk3588-dwcmshc",
		.data = &sdhci_dwcmshc_rk35xx_pdata,
	},
	{
		.compatible = "rockchip,rk3568-dwcmshc",
		.data = &sdhci_dwcmshc_rk35xx_pdata,
	},
	{
		.compatible = "horizon,x5-dwcmshc-sd",
		.data = &sdhci_dwcmshc_x5_sd_pdata,
	},
	{
		.compatible = "horizon,x5-dwcmshc-emmc",
		.data = &sdhci_dwcmshc_x5_emmc_pdata,
	},
	{
		.compatible = "snps,dwcmshc-sdhci",
		.data = &sdhci_dwcmshc_pdata,
	},
	{},
};
MODULE_DEVICE_TABLE(of, sdhci_dwcmshc_dt_ids);

#ifdef CONFIG_ACPI
static const struct acpi_device_id sdhci_dwcmshc_acpi_ids[] = {
	{
		.id = "MLNXBF30",
		.driver_data = (kernel_ulong_t)&sdhci_dwcmshc_bf3_pdata,
	},
	{}
};
#endif

static int dwcmshc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_host *host;
	struct dwcmshc_priv *priv;
	struct rk35xx_priv *rk_priv = NULL;
	struct x5_priv *x5_priv = NULL;
	const struct sdhci_pltfm_data *pltfm_data;
	int err;
	struct resource *res;
	u32 extra;

	pltfm_data = device_get_match_data(&pdev->dev);
	if (!pltfm_data) {
		dev_err(&pdev->dev, "Error: No device match data found\n");
		return -ENODEV;
	}

	host = sdhci_pltfm_init(pdev, pltfm_data,
				sizeof(struct dwcmshc_priv));
	if (IS_ERR(host))
		return PTR_ERR(host);

	/*
	 * extra adma table cnt for cross 128M boundary handling.
	 */
	extra = DIV_ROUND_UP_ULL(dma_get_required_mask(dev), SZ_128M);
	if (extra > SDHCI_MAX_SEGS)
		extra = SDHCI_MAX_SEGS;
	host->adma_table_cnt += extra;

	pltfm_host = sdhci_priv(host);
	priv = sdhci_pltfm_priv(pltfm_host);

	/* X5 soc axi clk need open first */
	if (pltfm_data == &sdhci_dwcmshc_x5_sd_pdata || pltfm_data == &sdhci_dwcmshc_x5_emmc_pdata) {
		x5_priv = devm_kzalloc(&pdev->dev, sizeof(struct x5_priv), GFP_KERNEL);
		if (!x5_priv) {
			err = -ENOMEM;
			goto free_pltfm;
		}

		priv->priv = x5_priv;

		err = dwcmshc_x5_clk_init(host, priv);
		if (err)
			goto free_pltfm;
	}

	if (dev->of_node) {
		pltfm_host->clk = devm_clk_get(dev, "core");
		if (IS_ERR(pltfm_host->clk)) {
			err = PTR_ERR(pltfm_host->clk);
			dev_err(dev, "failed to get core clk: %d\n", err);
			goto free_pltfm;
		}
		err = clk_prepare_enable(pltfm_host->clk);
		if (err)
			goto free_pltfm;

		priv->bus_clk = devm_clk_get(dev, "bus");
		if (!IS_ERR(priv->bus_clk))
			clk_prepare_enable(priv->bus_clk);
	}

	err = mmc_of_parse(host->mmc);
	if (err)
		goto err_clk;

	sdhci_get_of_property(pdev);

	priv->vendor_specific_area1 =
		sdhci_readl(host, DWCMSHC_P_VENDOR_AREA1) & DWCMSHC_AREA1_MASK;

	host->mmc_host_ops.request = dwcmshc_request;
	host->mmc_host_ops.hs400_enhanced_strobe = dwcmshc_hs400_enhanced_strobe;

	if (pltfm_data == &sdhci_dwcmshc_rk35xx_pdata) {
		rk_priv = devm_kzalloc(&pdev->dev, sizeof(struct rk35xx_priv), GFP_KERNEL);
		if (!rk_priv) {
			err = -ENOMEM;
			goto err_clk;
		}

		if (of_device_is_compatible(pdev->dev.of_node, "rockchip,rk3588-dwcmshc"))
			rk_priv->devtype = DWCMSHC_RK3588;
		else
			rk_priv->devtype = DWCMSHC_RK3568;

		priv->priv = rk_priv;

		err = dwcmshc_rk35xx_init(host, priv);
		if (err)
			goto err_clk;
	}

	if (pltfm_data == &sdhci_dwcmshc_x5_sd_pdata || pltfm_data == &sdhci_dwcmshc_x5_emmc_pdata) {
		if (of_device_is_compatible(dev->of_node, "horizon,x5-dwcmshc-emmc"))
			x5_priv->card_type = DWCMSCH_X5_EMMC;
		else
			x5_priv->card_type = DWCMSCH_X5_SD;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		x5_priv->dll_ctrl_base = devm_ioremap(dev, res->start, resource_size(res));
		if (IS_ERR(x5_priv->dll_ctrl_base)) {
			dev_err(dev, "Failed to get mmc/sd/sdio dll ctrl regbase!\n");
			goto err_clk;
		}

		if (x5_priv->card_type == DWCMSCH_X5_SD) {
			if (!device_property_read_u32
				(dev, "mmc-fixed-voltage", &x5_priv->mmc_fixed_voltage))
				dev_info(dev, "mmc set to fixed voltage %d\n", x5_priv->mmc_fixed_voltage);
			else
				x5_priv->mmc_fixed_voltage = 0;

			x5_priv->voltage_gpio = devm_gpiod_get_optional(&pdev->dev, "voltage", GPIOD_OUT_LOW);
			if (IS_ERR(x5_priv->voltage_gpio))
				dev_warn(dev, "can not parse voltage gpio\n");

			x5_priv->power_gpio = devm_gpiod_get_optional(&pdev->dev, "power", GPIOD_OUT_LOW);
			if (IS_ERR(x5_priv->voltage_gpio))
				dev_warn(dev, "can not parse power gpio\n");
			else
				/* For warm boot, SD card need to be powered down */
				dwcmshc_x5_toggle_sd_power(host->mmc);

			host->mmc_host_ops.start_signal_voltage_switch = dwcmshc_x5_start_signal_voltage_switch;
			/* TODO: Add power for reboot reset SD card slot */
			x5_priv->reset = devm_reset_control_get_optional(mmc_dev(host->mmc), "sd_rst");
			if (IS_ERR(x5_priv->reset)) {
				err = PTR_ERR(x5_priv->reset);
				dev_err(mmc_dev(host->mmc), "failed to get reset control %d\n", err);
				return err;
			}
		} else {
			/* x5_priv->card_type == DWCMSCH_X5_EMMC */
			x5_priv->reset = devm_reset_control_get_optional(mmc_dev(host->mmc), "emmc_rst");
			if (IS_ERR(x5_priv->reset)) {
				err = PTR_ERR(x5_priv->reset);
				dev_err(mmc_dev(host->mmc), "failed to get reset control %d\n", err);
				return err;
			}
		}
		x5_sdhci_sys_reset(host);

		/* Configure default dll value */
		iowrite32(X5_EMMC_MST_DLL_DEFAULT, x5_priv->dll_ctrl_base + X5_MST_DLL_OFFSET);
		iowrite32(X5_EMMC_SLV_DLL_DEFAULT, x5_priv->dll_ctrl_base + X5_SLV_DLL_OFFSET);
		/* Handle DWC MSHC_CTRL */
		x5_priv->mshc_ctrl_val = 0x0;
		if (device_property_read_bool(dev, "dwcmshc,no-cmd-conflict-check")) {
			x5_priv->mshc_ctrl_val &= ~BIT(0);
		}
		if (device_property_read_bool(dev, "dwcmshc,positive-edge-drive")) {
			x5_priv->mshc_ctrl_val |= BIT(6);
		}
		if (device_property_read_bool(dev, "dwcmshc,negative-edge-sample")) {
			x5_priv->mshc_ctrl_val |= BIT(7);
		}
		sdhci_writeb(host, x5_priv->mshc_ctrl_val, priv->vendor_specific_area1 + DWCMSHC_HOST_CTRL3);
		dev_info(dev, "MSHC_CTRL:%#x", x5_priv->mshc_ctrl_val);
	}

	host->mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY;

	err = sdhci_setup_host(host);
	if (err)
		goto err_clk;

	if (rk_priv)
		dwcmshc_rk35xx_postinit(host, priv);

	err = __sdhci_add_host(host);
	if (err)
		goto err_setup_host;

	return 0;

err_setup_host:
	sdhci_cleanup_host(host);
err_clk:
	clk_disable_unprepare(pltfm_host->clk);
	clk_disable_unprepare(priv->bus_clk);
	if (x5_priv)
		clk_bulk_disable_unprepare(X5_MAX_CLKS,
					   x5_priv->x5_clks);
	if (rk_priv)
		clk_bulk_disable_unprepare(RK35xx_MAX_CLKS,
					   rk_priv->rockchip_clks);
free_pltfm:
	sdhci_pltfm_free(pdev);
	return err;
}

static int dwcmshc_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct rk35xx_priv *rk_priv = priv->priv;
	struct x5_priv *x5_priv = priv->priv;
	const struct sdhci_pltfm_data *pltfm_data = device_get_match_data(&pdev->dev);

	sdhci_remove_host(host, 0);

	clk_disable_unprepare(pltfm_host->clk);
	clk_disable_unprepare(priv->bus_clk);
	if (pltfm_data == &sdhci_dwcmshc_rk35xx_pdata) {
		if (rk_priv)
			clk_bulk_disable_unprepare(RK35xx_MAX_CLKS,
						rk_priv->rockchip_clks);
	}

	if (pltfm_data == &sdhci_dwcmshc_x5_sd_pdata ||
	    pltfm_data == &sdhci_dwcmshc_x5_emmc_pdata) {
		if (x5_priv)
			clk_bulk_disable_unprepare(X5_MAX_CLKS,
						x5_priv->x5_clks);
	}
	sdhci_pltfm_free(pdev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dwcmshc_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_pltfm_data *pltfm_data = device_get_match_data(dev);
	struct rk35xx_priv *rk_priv = priv->priv;
	struct x5_priv *x5_priv = priv->priv;
	int ret;

	ret = sdhci_suspend_host(host);
	if (ret)
		return ret;

	clk_disable_unprepare(pltfm_host->clk);
	if (!IS_ERR(priv->bus_clk))
		clk_disable_unprepare(priv->bus_clk);

	if (pltfm_data == &sdhci_dwcmshc_rk35xx_pdata) {
		if (rk_priv)
			clk_bulk_disable_unprepare(RK35xx_MAX_CLKS,
						rk_priv->rockchip_clks);
	}

	if (pltfm_data == &sdhci_dwcmshc_x5_sd_pdata ||
	    pltfm_data == &sdhci_dwcmshc_x5_emmc_pdata) {
		if (x5_priv) {
			clk_disable_unprepare(x5_priv->card_clk);
			clk_bulk_disable_unprepare(X5_MAX_CLKS,
						x5_priv->x5_clks);
		}
	}

	return ret;
}

static int dwcmshc_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct rk35xx_priv *rk_priv = priv->priv;
	struct x5_priv *x5_priv = priv->priv;
	const struct sdhci_pltfm_data *pltfm_data = device_get_match_data(dev);
	int ret;

	/* Make sure axi clock enable first */
	if (pltfm_data == &sdhci_dwcmshc_x5_sd_pdata ||
	    pltfm_data == &sdhci_dwcmshc_x5_emmc_pdata) {
		if (x5_priv) {
			ret = clk_bulk_prepare_enable(X5_MAX_CLKS,
						x5_priv->x5_clks);
			if (ret)
				return ret;

			ret = clk_prepare_enable(x5_priv->card_clk);
			if (ret)
				return ret;
		}
	}

	ret = clk_prepare_enable(pltfm_host->clk);
	if (ret)
		return ret;

	if (!IS_ERR(priv->bus_clk)) {
		ret = clk_prepare_enable(priv->bus_clk);
		if (ret)
			return ret;
	}

	if (pltfm_data == &sdhci_dwcmshc_rk35xx_pdata) {
		if (rk_priv) {
			ret = clk_bulk_prepare_enable(RK35xx_MAX_CLKS,
						rk_priv->rockchip_clks);
			if (ret)
				return ret;
		}
	}

	return sdhci_resume_host(host);
}
#endif

static SIMPLE_DEV_PM_OPS(dwcmshc_pmops, dwcmshc_suspend, dwcmshc_resume);

static struct platform_driver sdhci_dwcmshc_driver = {
	.driver	= {
		.name	= "sdhci-dwcmshc",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = sdhci_dwcmshc_dt_ids,
		.acpi_match_table = ACPI_PTR(sdhci_dwcmshc_acpi_ids),
		.pm = &dwcmshc_pmops,
	},
	.probe	= dwcmshc_probe,
	.remove	= dwcmshc_remove,
};
module_platform_driver(sdhci_dwcmshc_driver);

MODULE_DESCRIPTION("SDHCI platform driver for Synopsys DWC MSHC");
MODULE_AUTHOR("Jisheng Zhang <jszhang@kernel.org>");
MODULE_LICENSE("GPL v2");
