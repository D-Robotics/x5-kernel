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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/reset.h>

#include <linux/phy/phy.h>
#include <linux/phy/phy-mipi-dphy.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include "phy-snps-mipi-dphy.h"
#include "dphy-debugfs.h"

#define KHZ	      1000
#define MHZ	      (1000 * 1000UL)
#define MAX_LINK_RATE (1250 * MHZ)
#define MIN_LINK_RATE (40 * MHZ)
#define PLL_REF_CLK   (24576 * KHZ)

/* PLL constraint */
#define MAX_DM	625
#define MIN_DM	40
#define MAX_DN	16
#define MIN_DN	1
#define MAX_VCO (1250 * KHZ) /* KHz */
#define MIN_VCO (320 * KHZ)  /* KHz */

#define DISP_DPHY_CFG_0	   4
#define DISP_DPHY_PLL_CFG  8
#define DISP_PHY_TST_CTRL0 0x24
#define DISP_PHY_TST_CTRL1 0x28

#define PLL_PROP_CHARGE_PUMP_CTRL	  0xE
#define PLL_INT_CHARGE_PUMP_CTRL	  0xF
#define PLL_VCO_CTRL			  0x12
#define PLL_GMP_CTRL			  0x13
#define PLL_PHASE_ERR_CTRL		  0x14
#define PLL_LOCKING_FILTER		  0x15
#define PLL_UNLOCKING_FILTER		  0x16
#define PLL_INPUT_DIVIDER_RATION	  0x17
#define PLL_LOOP_DIVIDER_RATION		  0x18
#define PLL_INPUT_LOOP_DIVIDER_RATIO_CTRL 0x19
#define PLL_CHARGE_PUMP_BIAS_CTRL	  0x1C
#define PLL_LOCK_DETECTOR_MODE_SEL	  0x1D
#define PLL_ANALOG_PROG_CTRL		  0X1F
#define SLEW_RATE_FSM_OVERRIDE_CTRL	  0xA0
#define SLEW_RATE_DDL_LOOP_CONF_CTRL	  0xA3
#define LP_RX_BIAS_CTRL			  0x4A

#define TESTCLR_BIT    BIT(0)
#define TESTCLK_BIT    BIT(1)
#define TESTEN_BIT     BIT(16)
#define TESTDIN_MASK   GENMASK(7, 0)
#define TESTDOUT_MASK  GENMASK(15, 8)
#define TESTDOUT_SHIFT 8

#define HS_TX_CLK_TLP_CODE     0x60
#define HS_TX_CLK_PREPARE_CODE 0x61
#define HS_TX_CLK_ZERO_CODE    0x62
#define HS_TX_CLK_TRAIL_CODE   0x63
#define HS_TX_CLK_EXIT_CODE    0x64
#define HS_TX_CLK_POST_CODE    0x65

#define HS_TX_DAT_TLP_CODE     0x70
#define HS_TX_DAT_PREPARE_CODE 0x71
#define HS_TX_DAT_ZERO_CODE    0x72
#define HS_TX_DAT_TRAIL_CODE   0x73
#define HS_TX_DAT_EXIT_CODE    0x74

#define diff_abs(a, b) (((a) > (b)) ? ((a) - (b)) : ((b) - (a)))

struct range_table {
	unsigned int min;
	unsigned int max;
	u8 value;
};

static inline unsigned int ps_to_ui(unsigned long hs_clk_rate, unsigned int ps)
{
	/* ui_ps = ceil(1e12 / HS-CLK)  */
	unsigned long long ui_ps = ALIGN(PSEC_PER_SEC, hs_clk_rate);
	do_div(ui_ps, hs_clk_rate);

	/* ui_cnt = ceil(ps / ui_ps) */
	unsigned long long cnt	= ps;
	unsigned long long frac = do_div(cnt, ui_ps);
	if (frac)
		cnt++;
	pr_info("ps_to_ui: hs_clk_rate=%lu ps=%u ui_cnt=%llu\n", hs_clk_rate, ps, cnt);
	return (unsigned int)cnt;
}

/* Get lane byte clock cycles from picosecond */
static inline unsigned int ps_to_lbcc(unsigned long hs_clk_rate, unsigned int ps)
{
	unsigned long long ui, lbcc, frac;

	ui = ALIGN(PSEC_PER_SEC, hs_clk_rate);
	do_div(ui, hs_clk_rate);

	lbcc = ps / 8;
	frac = do_div(lbcc, ui);
	if (frac)
		lbcc++;
	pr_info("ps_to_lbcc: hs_clk_rate=%lu ps=%u lbcc=%llu\n", hs_clk_rate, ps, lbcc);

	return lbcc;
}

static inline void phy_write(struct snps_dphy *dphy, u32 reg, u32 value)
{
	writel(value, dphy->base + reg);
}

static inline u32 phy_read(struct snps_dphy *dphy, u32 reg)
{
	u32 value = readl(dphy->base + reg);

	return value;
}

static void dphy_write_control_1(struct snps_dphy *dphy, u8 code, u8 val)
{
	phy_write(dphy, DISP_PHY_TST_CTRL1, BIT(16) | code);
	phy_write(dphy, DISP_PHY_TST_CTRL0, BIT(1));
	phy_write(dphy, DISP_PHY_TST_CTRL0, 0);
	phy_write(dphy, DISP_PHY_TST_CTRL1, val);
	phy_write(dphy, DISP_PHY_TST_CTRL0, BIT(1));
	phy_write(dphy, DISP_PHY_TST_CTRL0, 0);
	udelay(10);
}

static void dphy_write_control_2(struct snps_dphy *dphy, u32 code, u32 val0, u32 val1)
{
	dphy_write_control_1(dphy, code, val0);

	phy_write(dphy, DISP_PHY_TST_CTRL1, val1);
	phy_write(dphy, DISP_PHY_TST_CTRL0, BIT(1));
	phy_write(dphy, DISP_PHY_TST_CTRL0, 0);
}

static void dphy_test_reset(struct snps_dphy *dphy)
{
	/* Apply test reset pulse: testclr = 1, then 0 */
	phy_write(dphy, DISP_PHY_TST_CTRL0, 1);
	phy_write(dphy, DISP_PHY_TST_CTRL0, 0);
}

/* Read TESTDOUT for a given test-code (datasheet-compliant sequence) */
static u8 dphy_test_read_code(struct snps_dphy *dphy, u8 code)
{
	u32 val = 0U;

	/* Step 1: preload TESTDIN with code and set testen = 1 (BIT(16))   */
	val = TESTEN_BIT | (code & TESTDIN_MASK);
	phy_write(dphy, DISP_PHY_TST_CTRL1, val);
	udelay(1);

	/* Step 2: raise testclk; rising edge happens while testen = 1      */
	phy_write(dphy, DISP_PHY_TST_CTRL0, TESTCLK_BIT);
	udelay(1);

	/* Step 3: pull testclk low to latch the test-code                   */
	phy_write(dphy, DISP_PHY_TST_CTRL0, 0U);
	udelay(1);

	/* Step 4: clear testen by writing code only                         */
	val = code & TESTDIN_MASK;
	phy_write(dphy, DISP_PHY_TST_CTRL1, val);
	udelay(1);

	/* Step 5: wait for TESTDOUT to settle                               */
	udelay(500);

	/* Step 6: read TESTDOUT[15:8]                                       */
	val = phy_read(dphy, DISP_PHY_TST_CTRL1);

	return (u8)((val & TESTDOUT_MASK) >> TESTDOUT_SHIFT);
}

static void find_best_rate(struct pll_info *info)
{
	unsigned int dp, dm, dn;
	unsigned long long vco, diff;

	if (info->fout > 320 * KHZ)
		dp = 1;
	else if (info->fout > 160 * KHZ)
		dp = 2;
	else if (info->fout > 80 * KHZ)
		dp = 4;
	else
		dp = 8;

	vco  = dp * info->fout;
	diff = vco;

	for (dn = MIN_DN; dn <= MAX_DN; dn++) {
		for (dm = MAX_DM; dm >= MIN_DM; dm--) {
			if (diff_abs(dm * info->input / dn, vco) < diff) {
				diff	 = diff_abs(dm * info->input / dn, vco);
				info->dm = dm;
				info->dn = dn;
			}

			if (!diff)
				break;
		}

		if (!diff)
			break;
	}

	if (!info->dn)
		info->dn = 1;

	vco	   = info->dm * info->input / info->dn;
	info->fout = vco / dp;

	info->dm -= 2;
	info->dn -= 1;

	pr_debug("%d:%d\n", info->input, info->fout);
	pr_debug("dm = %d, dn = %d,\n", info->dm, info->dn);
}

static u8 get_vco_cntrl(unsigned int fout)
{
	if (fout > 1100 * KHZ)
		return 1;
	else if (fout > 630 * KHZ)
		return 3;
	else if (fout > 420 * KHZ)
		return 7;
	else if (fout > 320 * KHZ)
		return 0xf;
	else if (fout > 210 * KHZ)
		return 0x17;
	else if (fout > 160 * KHZ)
		return 0x1f;
	else if (fout > 105 * KHZ)
		return 0x27;
	else if (fout > 80 * KHZ)
		return 0x2f;
	else if (fout > 52500)
		return 0x37;
	else
		return 0x3f;
}

static struct range_table hsfreqrange[] = {
	{
		80000,
		97125,
		0x0,
	},
	{
		80000,
		107625,
		0x10,
	},
	{
		83125,
		118125,
		0x20,
	},
	{
		92625,
		128625,
		0x30,
	},
	{
		102125,
		139125,
		0x1,
	},
	{
		111625,
		149625,
		0x11,
	},
	{
		121125,
		160125,
		0x21,
	},
	{
		130625,
		170625,
		0x31,
	},
	{
		140125,
		181125,
		0x2,
	},
	{
		149625,
		191625,
		0x12,
	},
	{
		159125,
		202125,
		0x22,
	},
	{
		168625,
		212625,
		0x32,
	},
	{
		182875,
		228375,
		0x3,
	},
	{
		197125,
		244125,
		0x13,
	},
	{
		211375,
		259875,
		0x23,
	},
	{
		225625,
		275625,
		0x33,
	},
	{
		249375,
		301875,
		0x4,
	},
	{
		273125,
		328125,
		0x14,
	},
	{
		296875,
		354375,
		0x25,
	},
	{
		320625,
		380625,
		0x35,
	},
	{
		368125,
		433125,
		0x5,
	},
	{
		415625,
		485625,
		0x16,
	},
	{
		463125,
		538125,
		0x26,
	},
	{
		510625,
		590625,
		0x37,
	},
	{
		558125,
		643125,
		0x7,
	},
	{
		605625,
		695625,
		0x18,
	},
	{
		653125,
		748125,
		0x28,
	},
	{
		700625,
		800625,
		0x39,
	},
	{
		748125,
		853125,
		0x9,
	},
	{
		795625,
		905625,
		0x19,
	},
	{
		843125,
		958125,
		0x29,
	},
	{
		890625,
		1010625,
		0x3A,
	},
	{
		938125,
		1063125,
		0xA,
	},
	{
		985625,
		1115625,
		0x1A,
	},
	{
		1033125,
		1168125,
		0x2A,
	},
	{
		1080625,
		1220625,
		0x3B,
	},
	{
		1128125,
		1273125,
		0xB,
	},
	{
		1175625,
		1325625,
		0x1B,
	},
	{
		1223125,
		1378125,
		0x2B,
	},
	{
		1270625,
		1430625,
		0x3C,
	},
	{
		1318125,
		1483125,
		0xC,
	},
	{
		1365625,
		1535625,
		0x1C,
	},
	{
		1413125,
		1588125,
		0x2C,
	},
	{
		1460625,
		1640625,
		0x3D,
	},
	{
		1508125,
		1693125,
		0xD,
	},
	{
		1555625,
		1745625,
		0x1D,
	},
	{
		1603125,
		1798125,
		0x2E,
	},
	{
		1650625,
		1850625,
		0x3E,
	},
	{
		1698125,
		1903125,
		0xE,
	},
	{
		1745625,
		1955625,
		0x1E,
	},
	{
		1793125,
		2008125,
		0x2F,
	},
	{
		1840625,
		2060625,
		0x3F,
	},
	{
		1888125,
		2113125,
		0xF,
	},
	{
		1935625,
		2165625,
		0x40,
	},
	{
		1983125,
		2218125,
		0x41,
	},
	{
		2030625,
		2270625,
		0x42,
	},
	{
		2078125,
		2323125,
		0x43,
	},
	{
		2125625,
		2375625,
		0x44,
	},
	{
		2173125,
		2428125,
		0x45,
	},
	{
		2220625,
		2480625,
		0x46,
	},
	{
		2268125,
		2500000,
		0x47,
	},
	{
		2315625,
		2500000,
		0x48,
	},
	{
		2363125,
		2500000,
		0x49,
	},

};

static u8 get_hsfreqrange(unsigned int fout, struct range_table table[], size_t s)
{
	unsigned int i;
	unsigned int hs_rate = fout * 2;

	for (i = 0; i < s; i++)
		if (hs_rate > table[i].min && hs_rate <= table[i].max)
			return table[i].value;

	return table[0].value;
}

static inline u8 get_cfgfreqrange(unsigned int ref_clk)
{
	WARN_ON(ref_clk < 17 * KHZ);

	return (ref_clk / KHZ - 17) * 4;
}

static void config_pll(struct snps_dphy *dphy, struct pll_info *info)
{
	static const u8 read_codes[] = {
		HS_TX_CLK_TLP_CODE,   HS_TX_CLK_PREPARE_CODE, HS_TX_CLK_ZERO_CODE,
		HS_TX_CLK_TRAIL_CODE, HS_TX_CLK_EXIT_CODE,    HS_TX_CLK_POST_CODE,
		HS_TX_DAT_TLP_CODE,   HS_TX_DAT_PREPARE_CODE, HS_TX_DAT_ZERO_CODE,
		HS_TX_DAT_TRAIL_CODE, HS_TX_DAT_EXIT_CODE,
	};
	u8 code	 = 0;
	u8 val	 = 0;
	size_t i = 0;
	u32 cnt	 = 0;

	// Clear test interface
	dphy_test_reset(dphy);

	phy_write(dphy, DISP_DPHY_CFG_0,
		  (get_hsfreqrange(info->fout, hsfreqrange, ARRAY_SIZE(hsfreqrange)) << 8) |
			  get_cfgfreqrange(dphy->ref_clk_rate));

	dphy_write_control_1(dphy, SLEW_RATE_DDL_LOOP_CONF_CTRL, 0);
	dphy_write_control_1(dphy, SLEW_RATE_FSM_OVERRIDE_CTRL, 2);

	dphy_write_control_1(dphy, PLL_ANALOG_PROG_CTRL, 1);
	dphy_write_control_1(dphy, LP_RX_BIAS_CTRL, 0x40);

	phy_write(dphy, DISP_DPHY_PLL_CFG, 0x11);

	dphy_write_control_2(dphy, PLL_PHASE_ERR_CTRL, 2, 0x80);
	dphy_write_control_1(dphy, PLL_LOCKING_FILTER, 0x60);
	dphy_write_control_1(dphy, PLL_UNLOCKING_FILTER, 0x3);
	dphy_write_control_1(dphy, PLL_LOCK_DETECTOR_MODE_SEL, 0x2);

	dphy_write_control_1(dphy, PLL_INPUT_LOOP_DIVIDER_RATIO_CTRL, BIT(5) | BIT(4));

	dphy_write_control_2(dphy, PLL_LOOP_DIVIDER_RATION,
			     BIT(7) | ((info->dm & GENMASK(9, 5)) >> 5), info->dm & GENMASK(4, 0));

	dphy_write_control_1(dphy, PLL_INPUT_DIVIDER_RATION, info->dn & GENMASK(3, 0));

	dphy_write_control_1(dphy, PLL_VCO_CTRL, get_vco_cntrl(info->fout));

	dphy_write_control_1(dphy, PLL_CHARGE_PUMP_BIAS_CTRL, 0x10);
	dphy_write_control_1(dphy, PLL_GMP_CTRL, 0);
	dphy_write_control_1(dphy, PLL_INT_CHARGE_PUMP_CTRL, 0);

	dphy_write_control_1(dphy, PLL_PROP_CHARGE_PUMP_CTRL,
			     (info->fout > 1150 * KHZ) ? 0xe : 0xd);

	if (dphy->dbg_write_en) {
		/* HS clock TLP */
		cnt = ps_to_lbcc(dphy->cfg.hs_clk_rate, dphy->cfg.lpx);
		pr_info("write HS_TX_CLK_TLP_CODE: code=0x%02x clk_rate=%lu ps=lpx(%u) cnt=%u "
			"val=0x%02x\n",
			HS_TX_CLK_TLP_CODE, dphy->cfg.hs_clk_rate, dphy->cfg.lpx, cnt, cnt);
		dphy_write_control_1(dphy, HS_TX_CLK_TLP_CODE, cnt);

		/* HS clock PREPARE */
		cnt = ps_to_lbcc(dphy->cfg.hs_clk_rate, dphy->cfg.clk_prepare);
		val = BIT(6) | cnt;
		pr_info("write HS_TX_CLK_PREPARE_CODE: code=0x%02x clk_rate=%lu ps=clk_prepare(%u) "
			" cnt=%uval=0x%02x\n",
			HS_TX_CLK_PREPARE_CODE, dphy->cfg.hs_clk_rate, dphy->cfg.clk_prepare, cnt,
			val);
		dphy_write_control_1(dphy, HS_TX_CLK_PREPARE_CODE, val);

		/* HS clock ZERO */
		cnt = ps_to_lbcc(dphy->cfg.hs_clk_rate, dphy->cfg.clk_zero);
		val = BIT(7) | cnt;
		pr_info("write HS_TX_CLK_ZERO_CODE: code=0x%02x clk_rate=%lu ps=clk_zero(%u) "
			"cnt=%u "
			"val=0x%02x\n",
			HS_TX_CLK_ZERO_CODE, dphy->cfg.hs_clk_rate, dphy->cfg.clk_zero, cnt, val);
		dphy_write_control_1(dphy, HS_TX_CLK_ZERO_CODE, val);

		/* HS clock TRAIL */
		cnt = ps_to_lbcc(dphy->cfg.hs_clk_rate, dphy->cfg.clk_trail);
		val = BIT(6) | cnt;
		pr_info("write HS_TX_CLK_TRAIL_CODE: code=0x%02x clk_rate=%lu ps=clk_trail(%u) "
			"cnt=%u "
			"val=0x%02x\n",
			HS_TX_CLK_TRAIL_CODE, dphy->cfg.hs_clk_rate, dphy->cfg.clk_trail, cnt, val);
		dphy_write_control_1(dphy, HS_TX_CLK_TRAIL_CODE, val);

		/* HS clock EXIT */
		cnt = ps_to_lbcc(dphy->cfg.hs_clk_rate, dphy->cfg.hs_exit);
		val = BIT(7) | BIT(6) | cnt;
		pr_info("write HS_TX_CLK_EXIT_CODE: code=0x%02x clk_rate=%lu ps=hs_exit(%u) cnt=%u "
			"val=0x%02x\n",
			HS_TX_CLK_EXIT_CODE, dphy->cfg.hs_clk_rate, dphy->cfg.hs_exit, cnt, val);
		dphy_write_control_1(dphy, HS_TX_CLK_EXIT_CODE, val);

		/* HS clock POST */
		cnt = ps_to_lbcc(dphy->cfg.hs_clk_rate, dphy->cfg.clk_post);
		val = BIT(6) | cnt;
		pr_info("write HS_TX_CLK_POST_CODE: code=0x%02x clk_rate=%lu ps=clk_post(%u) "
			"cnt=%u "
			"val=0x%02x\n",
			HS_TX_CLK_POST_CODE, dphy->cfg.hs_clk_rate, dphy->cfg.clk_post, cnt, val);
		dphy_write_control_1(dphy, HS_TX_CLK_POST_CODE, val);

		/* HS data TLP */
		cnt = ps_to_lbcc(dphy->cfg.hs_clk_rate, dphy->cfg.lpx);
		pr_info("write HS_TX_DAT_TLP_CODE: code=0x%02x clk_rate=%lu ps=lpx(%u) cnt=%u "
			"val=0x%02x\n",
			HS_TX_DAT_TLP_CODE, dphy->cfg.hs_clk_rate, dphy->cfg.lpx, cnt, cnt);
		dphy_write_control_1(dphy, HS_TX_DAT_TLP_CODE, cnt);

		/* HS data PREPARE */
		cnt = ps_to_lbcc(dphy->cfg.hs_clk_rate, dphy->cfg.hs_prepare);
		val = BIT(6) | cnt;
		pr_info("write HS_TX_DAT_PREPARE_CODE: code=0x%02x clk_rate=%lu ps=hs_prepare(%u) "
			"cnt=%u "
			"val=0x%02x\n",
			HS_TX_DAT_PREPARE_CODE, dphy->cfg.hs_clk_rate, dphy->cfg.hs_prepare, cnt,
			val);
		dphy_write_control_1(dphy, HS_TX_DAT_PREPARE_CODE, val);

		/* HS data ZERO */
		cnt = ps_to_lbcc(dphy->cfg.hs_clk_rate, dphy->cfg.hs_zero);
		val = BIT(7) | cnt;
		pr_info("write HS_TX_DAT_ZERO_CODE: code=0x%02x clk_rate=%lu ps=hs_zero(%u) cnt=%u "
			"val=0x%02x\n",
			HS_TX_DAT_ZERO_CODE, dphy->cfg.hs_clk_rate, dphy->cfg.hs_zero, cnt, val);
		dphy_write_control_1(dphy, HS_TX_DAT_ZERO_CODE, val);

		/* HS data TRAIL */
		cnt = ps_to_lbcc(dphy->cfg.hs_clk_rate, dphy->cfg.hs_trail);
		val = BIT(6) | cnt;
		pr_info("write HS_TX_DAT_TRAIL_CODE: code=0x%02x clk_rate=%lu ps=hs_trail(%u) "
			"cnt=%u "
			"val=0x%02x\n",
			HS_TX_DAT_TRAIL_CODE, dphy->cfg.hs_clk_rate, dphy->cfg.hs_trail, cnt, val);
		dphy_write_control_1(dphy, HS_TX_DAT_TRAIL_CODE, val);

		/* HS data EXIT */
		cnt = ps_to_lbcc(dphy->cfg.hs_clk_rate, dphy->cfg.hs_exit);
		val = BIT(7) | BIT(6) | cnt;
		pr_info("write HS_TX_DAT_EXIT_CODE: code=0x%02x clk_rate=%lu ps=hs_exit(%u) cnt=%u "
			"val=0x%02x\n",
			HS_TX_DAT_EXIT_CODE, dphy->cfg.hs_clk_rate, dphy->cfg.hs_exit, cnt, val);
		dphy_write_control_1(dphy, HS_TX_DAT_EXIT_CODE, val);
	}
	// udelay(50);
	// dphy_test_reset(dphy);
	if (dphy->dbg_read_en) {
		for (i = 0; i < ARRAY_SIZE(read_codes); i++) {
			code = read_codes[i];
			val  = dphy_test_read_code(dphy, code);
			pr_info("%s: test code 0x%02x -> 0x%02x\n", __func__, code, val);
			udelay(1);
		}
	}
}

static int snps_dphy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	struct snps_dphy *dphy				    = phy_get_drvdata(phy);
	const struct phy_configure_opts_mipi_dphy *dphy_cfg = &opts->mipi_dphy;
	struct pll_info info;

	memcpy(&dphy->cfg, opts, sizeof(struct phy_configure_opts_mipi_dphy));

	info.input = dphy->ref_clk_rate;
	info.fout  = dphy_cfg->hs_clk_rate / KHZ / 2;
	find_best_rate(&info);

	config_pll(dphy, &info);

	msleep(500);

	return 0;
}

static int snps_dphy_validate(struct phy *phy, enum phy_mode mode, int submode,
			      union phy_configure_opts *opts)
{
	struct phy_configure_opts_mipi_dphy *dphy_cfg = &opts->mipi_dphy;
	const struct snps_dphy *dphy		      = phy_get_drvdata(phy);
	unsigned long long ui;
	struct pll_info info;
	int ret = 0;

	if (dphy_cfg->hs_clk_rate / 2 > MAX_LINK_RATE ||
	    dphy_cfg->hs_clk_rate / 2 < MIN_LINK_RATE) {
		pr_err("snps_dphy_validate: hs_clk_rate=%lu out of range\n", dphy_cfg->hs_clk_rate);
		return -EINVAL;
	}

	info.input = dphy->ref_clk_rate;
	info.fout  = dphy_cfg->hs_clk_rate / KHZ / 2;
	find_best_rate(&info);

	dphy_cfg->hs_clk_rate = info.fout * KHZ * 2;

	if (dphy_cfg->hs_clk_rate / 2 > MAX_LINK_RATE ||
	    dphy_cfg->hs_clk_rate / 2 < MIN_LINK_RATE) {
		pr_err("snps_dphy_validate: adjusted hs_clk_rate=%lu out of range\n",
		       dphy_cfg->hs_clk_rate);
		return -EINVAL;
	}

	ui = ALIGN(PSEC_PER_SEC, dphy_cfg->hs_clk_rate);
	do_div(ui, dphy_cfg->hs_clk_rate);

	dphy_cfg->hs_trail = max(1 * 8 * ui, 60000 + 1 * 4 * ui);

	ret = phy_mipi_dphy_config_validate(dphy_cfg);
	if (ret) {
		pr_err("snps_dphy_validate: phy_mipi_dphy_config_validate fail! ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int snps_dphy_power_on(struct phy *phy)
{
	return 0;
}

static int snps_dphy_power_off(struct phy *phy)
{
	return 0;
}

static const struct phy_ops snps_dphy_ops = {
	.configure = snps_dphy_configure,
	.validate  = snps_dphy_validate,
	.power_on  = snps_dphy_power_on,
	.power_off = snps_dphy_power_off,
	.owner	   = THIS_MODULE,
};

static int snps_dphy_probe(struct platform_device *pdev)
{
	struct phy_provider *phy_provider;
	struct snps_dphy *dphy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;

	dphy = devm_kzalloc(dev, sizeof(*dphy), GFP_KERNEL);
	if (!dphy)
		return -ENOMEM;

	res	   = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dphy->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(dphy->base))
		return -ENODEV;

	dphy->ref_clk = devm_clk_get(dev, "ref-clk");
	if (IS_ERR(dphy->ref_clk))
		return PTR_ERR(dphy->ref_clk);

	ret = clk_prepare_enable(dphy->ref_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare/enable ref_clk\n");
		return ret;
	}

	dphy->ref_clk_rate = clk_get_rate(dphy->ref_clk) / KHZ;

	dphy->cfg_clk = devm_clk_get_optional(dev, "cfg-clk");
	if (IS_ERR(dphy->cfg_clk)) {
		dev_err(dev, "failed to get cfg_clk\n");
		ret = PTR_ERR(dphy->cfg_clk);
		goto err_disable_ref_clk;
	}

	ret = clk_prepare_enable(dphy->cfg_clk);
	if (ret < 0) {
		dev_err(dev, "failed to prepare/enable cfg_clk\n");
		goto err_disable_ref_clk;
	}
	snps_dphy_debugfs_init(dphy);

	dphy->phy = devm_phy_create(dev, NULL, &snps_dphy_ops);
	if (IS_ERR(dphy->phy)) {
		dev_err(dev, "failed to create PHY\n");
		return PTR_ERR(dphy->phy);
	}

	phy_set_drvdata(dphy->phy, dphy);

	dev_set_drvdata(dev, dphy);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(phy_provider);

err_disable_ref_clk:
	clk_disable_unprepare(dphy->ref_clk);
	return ret;
}

static const struct of_device_id snps_dphy_of_match[] = {
	{.compatible = "snps,mipi-dphy"},
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, snps_dphy_of_match);

static int snps_dphy_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct snps_dphy *dphy;

	dphy = dev_get_drvdata(dev);
	snps_dphy_debugfs_exit(dphy);

	clk_disable_unprepare(dphy->cfg_clk);

	clk_disable_unprepare(dphy->ref_clk);

	dev_set_drvdata(dev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dphy_suspend(struct device *dev)
{
	struct snps_dphy *dphy;

	dphy = dev_get_drvdata(dev);

	clk_disable_unprepare(dphy->cfg_clk);

	clk_disable_unprepare(dphy->ref_clk);

	return 0;
}

static int dphy_resume(struct device *dev)
{
	struct snps_dphy *dphy;
	int ret;

	dphy = dev_get_drvdata(dev);

	ret = clk_prepare_enable(dphy->ref_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare/enable ref_clk\n");
		return ret;
	}

	ret = clk_prepare_enable(dphy->cfg_clk);
	if (ret < 0) {
		dev_err(dev, "failed to prepare/enable cfg_clk\n");
		goto err_disable_ref_clk;
	}

	return 0;

err_disable_ref_clk:
	clk_disable_unprepare(dphy->ref_clk);
	return ret;
}
#endif

static const struct dev_pm_ops dphy_pm_ops = {SET_SYSTEM_SLEEP_PM_OPS(dphy_suspend, dphy_resume)};

static struct platform_driver snps_dphy_driver = {
	.probe	= snps_dphy_probe,
	.remove = snps_dphy_remove,
	.driver =
		{
			.name		= "snps-mipi-dphy",
			.of_match_table = of_match_ptr(snps_dphy_of_match),
			.pm		= &dphy_pm_ops,
		},
};
module_platform_driver(snps_dphy_driver);

MODULE_DESCRIPTION("Synopsys MIPI D-PHY Driver");
MODULE_LICENSE("GPL");
