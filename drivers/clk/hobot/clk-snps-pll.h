// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef __CLK_SNPS_PLL_H
#define __CLK_SNPS_PLL_H

#include <linux/clk-provider.h>

#define MHZ (1000UL * 1000UL)
#define OUTPUT_NUM	2

struct pll_rate_table {
	unsigned long	rate;
	u16		prediv;
	u16		mint;
	u16		mfrac;
	u16		postdivp;
	u16		divvcop;
	u16		postdivr;
	u16		divvcor;
};

/**
 * struct drobot_clk_pll: information about pll clock
 * @id: platform specific id of the clock.
 * @name: name of this pll clock.
 * @parent_name: name of the parent clock.
 */
struct drobot_clk_pll {
	void __iomem	*cfg_reg;
	void __iomem	*internal_reg;
	const char	*name;
	const char	*parent_name;
	u8		rate_count;
	u32		flag;
	struct clk_hw	hw;
	const struct pll_rate_table *rate_table;
};

#define MINVCO1 	(2500 * MHZ)
#define MINVCO2 	(3750 * MHZ)
#define MINVCO3 	(4000 * MHZ)
#define MAXVCO1 	(3750 * MHZ)
#define MAXVCO2 	(5000 * MHZ)
#define MAXVCO3 	(6000 * MHZ)

#define PLL_FBDIV_OFFSET	(0x4)
#define PLL_POSTDIV_OFFSET	(0x8)
#define PLL_LOCK_OFFSET		(0xC)
#define PLL_ANAREG6		(0x14)
#define PLL_SCFRAC_CNT		(0x38)

#define PLL_LOWFREQ_SHIFT	(4)
#define PLL_VCOMODE_SHIFT	(5)
#define PLL_PREDIV_SHIFT	(6)

#define PLL_POSTDIVP_SHIFT	(2)
#define PLL_DIVVCOP_SHIFT	(8)
#define PLL_POSTDIVR_SHIFT	(12)
#define PLL_DIVVCOR_SHIFT	(18)
#define PLL_MINT_SHIFT		(16)


#define PLL_PREDIV_MASK		GENMASK(10, PLL_PREDIV_SHIFT)
#define PLL_DIVVCOP_MASK	GENMASK(11, PLL_DIVVCOP_SHIFT)
#define PLL_POSTDIVP_MASK	GENMASK(7, PLL_POSTDIVP_SHIFT)
#define PLL_DIVVCOR_MASK	GENMASK(21, PLL_DIVVCOR_SHIFT)
#define PLL_POSTDIVR_MASK	GENMASK(17, PLL_POSTDIVR_SHIFT)
#define PLL_MINT_MASK		GENMASK(25, PLL_MINT_SHIFT)
#define PLL_MFRAC_MASK		GENMASK(15, 0)
#define PLL_FBDIV_LOAD		BIT(26)
#define PLL_LOWFREQ_MASK	BIT(4)
#define PLL_VCOMODE_MASK	BIT(3)
#define PLL_FRAC_EN		BIT(0)
#define PLL_ANAREG6_EN		BIT(0)
#define PLL_CTRL_CFG0		0x3028

#define SET_PLL_PREDIV(x)	(((x) << PLL_PREDIV_SHIFT) & PLL_PREDIV_MASK)
#define SET_PLL_DIVVCOP(x)	(((x) << PLL_DIVVCOP_SHIFT) & PLL_DIVVCOP_MASK)
#define SET_PLL_POSTDIVP(x)	(((x) << PLL_POSTDIVP_SHIFT) & PLL_POSTDIVP_MASK)
#define SET_PLL_DIVVCOR(x)	(((x) << PLL_DIVVCOR_SHIFT) & PLL_DIVVCOR_MASK)
#define SET_PLL_POSTDIVR(x)	(((x) << PLL_POSTDIVR_SHIFT) & PLL_POSTDIVR_MASK)
#define SET_PLL_MINT(x)		(((x) << PLL_MINT_SHIFT) & PLL_MINT_MASK)

#define PLL_PWRON		(BIT(0) | BIT(1))
#define PLL_EN			(BIT(0) | BIT(1))
#define PLL_P_OUT		BIT(22)
#define PLL_R_OUT		BIT(23)
#define PLL_BYPASS		BIT(11)
#define PLL_LOCK		BIT(0)
#define PLL_FRAC_EN		BIT(0)
#define PLL_BYPASS_SYNC		(BIT(3) | BIT(4))
#define PLL_CTRL_DEFAULT	0x2838

struct clk_hw *clk_hw_register_pll(const char *name, const char *parent_name,
				void __iomem *cfg_reg, void __iomem *internal_reg,
				u32 flag, struct pll_rate_table *table, u8 rate_count);

#endif /* __CLK_PLL_H */
