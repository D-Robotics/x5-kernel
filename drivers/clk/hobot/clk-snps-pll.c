// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include "clk-snps-pll.h"

#define to_clk_pll(_hw) container_of(_hw, struct drobot_clk_pll, hw)

static u32 pll_get_divvco(unsigned int reg_value)
{
	switch (reg_value) {
	/*divvco register value mapping*/
	case 0x0:
		return 2;
	case 0x8:
		return 4;
	case 0xc:
		return 8;
	case 0xd:
		return 16;
	case 0xe:
		return 32;
	case 0xf:
		return 64;
	default:
		return 0xff;
	}
}

static u32 pll_get_divvco_reg(unsigned int value)
{
	switch (value) {
	/*divvco value mapping*/
	case 2:
		return 0x0;
	case 4:
		return 0x8;
	case 8:
		return 0xc;
	case 16:
		return 0xd;
	case 32:
		return 0xe;
	case 64:
		return 0xf;
	default:
		return 0xff;
	}
}

static void pll_get_params(struct drobot_clk_pll *pll,
			struct pll_rate_table *rate)
{
	unsigned int pll_cfg, pll_fbdiv, pll_postdiv;

	pll_cfg = readl(pll->cfg_reg);
	pll_fbdiv = readl(pll->cfg_reg + PLL_FBDIV_OFFSET);
	pll_postdiv = readl(pll->cfg_reg + PLL_POSTDIV_OFFSET);

	rate->prediv = ((pll_cfg & PLL_PREDIV_MASK) >> PLL_PREDIV_SHIFT) + 1;

	if (pll->flag & PLL_P_OUT) {
		rate->divvcop = pll_get_divvco((pll_postdiv & PLL_DIVVCOP_MASK) >> PLL_DIVVCOP_SHIFT);
		rate->postdivp = ((pll_postdiv & PLL_POSTDIVP_MASK) >> PLL_POSTDIVP_SHIFT) + 1;
	} else {
		rate->divvcor = pll_get_divvco((pll_postdiv & PLL_DIVVCOR_MASK) >> PLL_DIVVCOR_SHIFT);
		rate->postdivr = ((pll_postdiv & PLL_POSTDIVR_MASK) >> PLL_POSTDIVR_SHIFT) + 1;
	}

	rate->mint = ((pll_fbdiv & PLL_MINT_MASK) >> PLL_MINT_SHIFT) + 16;
	rate->mfrac = (pll_fbdiv & PLL_MFRAC_MASK);
}

static void pll_wait_lock(struct drobot_clk_pll *pll)
{
	int i = 0;

	while (!(readl(pll->cfg_reg + PLL_LOCK_OFFSET) & PLL_LOCK)) {
		cpu_relax();
		udelay(10);
		i++;
		if (i > 1000) {
			pr_err("%s: lock timeout for pll clk %s\n", __func__, __clk_get_name(pll->hw.clk));
			break;
		}
	}
}

static int pll_is_enabled(struct clk_hw *hw)
{
	struct drobot_clk_pll *pll = to_clk_pll(hw);
	u32 tmp, chan_state;

	if (pll->flag & PLL_P_OUT) {
		tmp = readl(pll->cfg_reg + PLL_POSTDIV_OFFSET) & PLL_P_OUT;
		chan_state = (tmp == PLL_P_OUT) ? 1 : 0;
	} else {
		tmp = readl(pll->cfg_reg + PLL_POSTDIV_OFFSET) & PLL_R_OUT;
		chan_state = (tmp == PLL_R_OUT) ? 1 : 0;
	}
	return chan_state;
}

static int pll_enable(struct clk_hw *hw)
{
	struct drobot_clk_pll *pll = to_clk_pll(hw);
	u32 tmp;

	tmp = readl(pll->cfg_reg + PLL_POSTDIV_OFFSET) | PLL_P_OUT | PLL_R_OUT;
	writel(tmp, pll->cfg_reg + PLL_POSTDIV_OFFSET);

	pr_err("%s: pll clk %s\n", __func__, __clk_get_name(hw->clk));
	return 0;
}

static unsigned long pll_recalc_rate(struct clk_hw *hw,
			unsigned long parent_rate)
{
	struct drobot_clk_pll *pll = to_clk_pll(hw);
	struct pll_rate_table cur;
	u64 fvco = parent_rate;

	if(!pll_is_enabled(hw))
		return 0;

	pll_get_params(pll, &cur);

	if (cur.mfrac != 0) {
		fvco = (parent_rate * cur.mint) + ((parent_rate * cur.mfrac) >> 16);
	} else {
		fvco = (parent_rate * cur.mint);
	}

	if (pll->flag & PLL_P_OUT)
		fvco = DIV_ROUND_CLOSEST_ULL(fvco, cur.prediv * cur.postdivp * cur.divvcop);
	else
		fvco = DIV_ROUND_CLOSEST_ULL(fvco, cur.prediv * cur.postdivr * cur.divvcor);

	return (unsigned long)fvco;
}

static long pll_round_rate_table(struct clk_hw *hw, unsigned long rate, unsigned long *parent_rate)
{
	struct drobot_clk_pll *pll = to_clk_pll(hw);

	const struct pll_rate_table *rate_table = pll->rate_table;
	int i;

	/* Assuming rate_table is in descending order */
	for (i = 0; i < pll->rate_count; i++) {
		if (rate >= rate_table[i].rate)
			return rate_table[i].rate;
	}

	/* return minimum supported value */
	return rate_table[i - 1].rate;
}

static void pll_set_regs(struct drobot_clk_pll *pll,
			const struct pll_rate_table *rate_table,
			unsigned long parent_rate)
{
	u32 cfg_reg, fbdiv_reg = 0, postdiv_reg = 0;
	u64 fvco = parent_rate;

	fbdiv_reg = 0;
	postdiv_reg = 0;

	cfg_reg = PLL_CTRL_CFG0;
	fvco = (parent_rate * rate_table->mint) + ((parent_rate * rate_table->mfrac) >> 16);

	 /* Change PLL parameter values */
	if (fvco <= 3750 * MHZ) {
		cfg_reg |= PLL_LOWFREQ_MASK;
	} else if (fvco <= 4500 * MHZ) {
		cfg_reg &= ~PLL_LOWFREQ_MASK;
	} else {
		cfg_reg &= ~PLL_VCOMODE_MASK;
		cfg_reg |= PLL_LOWFREQ_MASK;
	}
	cfg_reg &= ~PLL_PREDIV_MASK;
	cfg_reg |= SET_PLL_PREDIV(rate_table->prediv - 1);

	fbdiv_reg |= SET_PLL_MINT(rate_table->mint - 16);
	fbdiv_reg |= PLL_MFRAC_MASK & rate_table->mfrac;
	fbdiv_reg |= PLL_FBDIV_LOAD;

	postdiv_reg |= PLL_EN;
	postdiv_reg |= SET_PLL_DIVVCOP(pll_get_divvco_reg(rate_table->divvcop));
	postdiv_reg |= SET_PLL_POSTDIVP(rate_table->postdivp - 1);
	postdiv_reg |= SET_PLL_DIVVCOR(pll_get_divvco_reg(rate_table->divvcor));
	postdiv_reg |= SET_PLL_POSTDIVR(rate_table->postdivr - 1);

	writel(0, pll->cfg_reg + PLL_POSTDIV_OFFSET);
	writel(PLL_BYPASS, pll->cfg_reg);

	writel(fbdiv_reg, pll->cfg_reg + PLL_FBDIV_OFFSET);
	writel(cfg_reg, pll->cfg_reg);
	writel(PLL_FRAC_EN, pll->internal_reg + PLL_SCFRAC_CNT);
	if (fvco >= 4000 * MHZ)
		writel(PLL_BYPASS_SYNC, pll->internal_reg + PLL_ANAREG6);
	writel((cfg_reg | PLL_PWRON), pll->cfg_reg);
	writel(postdiv_reg, pll->cfg_reg + PLL_POSTDIV_OFFSET);
}

static const struct pll_rate_table *get_pll_settings(struct drobot_clk_pll *pll, unsigned long rate)
{
	const struct pll_rate_table  *rate_table = pll->rate_table;
	int i;

	for (i = 0; i < pll->rate_count; i++) {
		if (rate >= rate_table[i].rate)
			return &rate_table[i];
	}

	/* return minimum supported value */
	return &rate_table[i - 1];
}
static int pll_set_rate_table(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	struct drobot_clk_pll *pll = to_clk_pll(hw);
	const struct pll_rate_table *rate_table;

	/* Get required rate settings from table */
	if (rate == pll_recalc_rate(hw, parent_rate)) {
		return 0;
	}
	rate_table = get_pll_settings(pll, rate);
	if (!rate_table) {
		pr_err("%s: Invalid rate : %lu for pll clk %s\n", __func__,
			rate, __clk_get_name(hw->clk));
		return -EINVAL;
	}

	pll_set_regs(pll, rate_table, parent_rate);

	pll_wait_lock(pll);
	pll_enable(hw);

	return 0;
}

const struct clk_ops drobot_pll_table_clk_ops = {
	.recalc_rate = pll_recalc_rate,
	.round_rate = pll_round_rate_table,
	.set_rate = pll_set_rate_table,
	.is_enabled = pll_is_enabled,
};

const struct clk_ops drobot_pll_norate_clk_ops = {
	.recalc_rate = pll_recalc_rate,
	.is_enabled = pll_is_enabled,
};

struct clk_hw *clk_hw_register_pll(const char *name, const char *parent_name,
				void __iomem *cfg_reg, void __iomem *internal_reg,
				u32 flag, struct pll_rate_table *table, u8 rate_count)
{
	struct clk_hw *hw;
	struct drobot_clk_pll *pll;
	struct clk_init_data init;
	int ret;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	if (table) {
		init.ops = &drobot_pll_table_clk_ops;
		pll->rate_count = rate_count;
		pll->rate_table = kmemdup(table,
					pll->rate_count *
					sizeof(struct pll_rate_table),
					GFP_KERNEL);
		WARN(!pll->rate_table,
			"%s: could not allocate rate table for %s\n",
			__func__, name);
	} else {
		init.ops = &drobot_pll_norate_clk_ops;
	}

	init.parent_names = &parent_name;
	init.num_parents = 1;
	init.name = name;
	init.flags = CLK_GET_RATE_NOCACHE | CLK_IS_CRITICAL;

	/* struct pll assignments */
	pll->cfg_reg = cfg_reg;
	pll->internal_reg = internal_reg;

	pll->flag = flag;
	pll->hw.init = &init;

	hw = &pll->hw;
	/* register the clock */
	ret = clk_hw_register(NULL, hw);
	if (ret) {
		kfree(pll);
		hw = ERR_PTR(ret);
	}

	return hw;
}
