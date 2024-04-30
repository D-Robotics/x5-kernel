// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/bits.h>
#include "clk.h"

#define CPU_NUMS			8
#define CCLK_PRE_DIV_SHIFT		(16)
#define CCLK_PRE_DIV_WIDTH		(3)
#define CCLK_POST_DIV_SHIFT		(0)
#define CCLK_POST_DIV_WIDTH		(6)
#define CCLK_PRE_DIV_BOUNDARY		(8)
#define CCLK_POST_DIV_BOUNDARY		(64)
#define CCLK_DIV_WIDTH			(CCLK_PRE_DIV_WIDTH + CCLK_POST_DIV_WIDTH)
#define CCLK_MUX_SHIFT			(24)
#define CCLK_PRE_DIV_MASK		GENMASK(18, CCLK_PRE_DIV_SHIFT)
#define CCLK_POST_DIV_MASK		GENMASK(5, CCLK_POST_DIV_SHIFT)
#define CCLK_MUX_MASK			GENMASK(26, CCLK_MUX_SHIFT)
#define CCLK_EN				BIT(28)

#define SET_CCLK_PREDIV(x)		(((x) << CCLK_PRE_DIV_SHIFT) & CCLK_PRE_DIV_MASK)
#define SET_CCLK_POSTDIV(x)		(((x) << CCLK_POST_DIV_SHIFT) & CCLK_POST_DIV_MASK)
#define SET_CCLK_MUX(x)			(((x) << CCLK_MUX_SHIFT) & CCLK_MUX_MASK)

#define to_clk_cpu(_hw)		container_of(_hw, struct clk_cpu, hw)

struct clk_cpu_div_table {
	u8 prediv;
	u8 postdiv;
};

struct clk_cpu {
	struct clk_hw hw;
	void __iomem *reg;
	u8 offset;
};

static void gen_set_clear(struct clk_cpu *cclk, u32 set, u32 clear)
{
	u32 val;
	u8 i;

	val = readl(cclk->reg);

	val &= ~clear;
	val |= set;

	for (i = 0; i < CPU_NUMS; i++)
		writel(val, cclk->reg + (i * 0x20));
}

static int clk_gen_endisable(struct clk_hw *hw, int enable)
{
	struct clk_cpu *cclk = to_clk_cpu(hw);
	u32 reg;
	u8 i;

	reg = readl(cclk->reg);

	if (enable)
		reg |= CCLK_EN;
	else
		reg &= ~CCLK_EN;


	for (i = 0; i < CPU_NUMS; i++)
		writel(reg, cclk->reg + (i * 0x20));

	return 0;
}

static unsigned long clk_cpu_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	struct clk_cpu *cclk = to_clk_cpu(hw);
	u32 value, div;
	u8 pre_div, post_div;

	value = readl(cclk->reg);
	pre_div = (value & CCLK_PRE_DIV_MASK) >> CCLK_PRE_DIV_SHIFT;
	post_div = (value & CCLK_POST_DIV_MASK) >> CCLK_POST_DIV_SHIFT;
	div = (pre_div + 1) * (post_div + 1);

	return DIV_ROUND_CLOSEST_ULL(((u64)parent_rate), div);
}

static bool _is_best_div(unsigned long rate, unsigned long now,
			 unsigned long best, unsigned long flags)
{
	return now <= rate && now > best;
}

static bool cal_div_table(unsigned int bestdiv, struct clk_cpu_div_table *table)
{
	u32 i, j, maxdiv;

	maxdiv = (1 << CCLK_DIV_WIDTH);

	if (bestdiv <= CCLK_PRE_DIV_BOUNDARY) {
		table->prediv = bestdiv - 1;
		table->postdiv = 0;
		return true;
	}
	if (bestdiv >= maxdiv ) {
		table->prediv = CCLK_PRE_DIV_BOUNDARY - 1;
		table->postdiv = CCLK_POST_DIV_BOUNDARY - 1;
		return true;
	}

	for (i = 1; i <= CCLK_PRE_DIV_BOUNDARY; i++) {
		for (j = 1; j <= CCLK_POST_DIV_BOUNDARY; j++) {
			if (i * j == bestdiv) {
				table->prediv  = i - 1;
				table->postdiv = j - 1;
				return true;
			} else if (i * j > bestdiv) {
				break;
			}
		}
	}

	return false;
}

static unsigned int clk_cpu_bestdiv(struct clk_hw *hw, struct clk_hw *parent,
					unsigned long rate,
					unsigned long *best_parent_rate,
					unsigned long flags)
{
	int i, bestdiv = 0;
	unsigned long parent_rate, best = 0, now, maxdiv;
	unsigned long parent_rate_saved = *best_parent_rate;

	maxdiv = (1 << CCLK_DIV_WIDTH);

	if (!(clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT)) {
		parent_rate = *best_parent_rate;
		bestdiv = DIV_ROUND_CLOSEST_ULL(((u64)parent_rate), rate);
		bestdiv = bestdiv == 0 ? 1 : bestdiv;
		bestdiv = bestdiv > maxdiv ? maxdiv : bestdiv;
		return bestdiv;
	}

	/*
	 * The maximum divider we can use without overflowing
	 * unsigned long in rate * i below
	 */
	maxdiv = min(ULONG_MAX / rate, maxdiv);

	for (i = 0; i <= maxdiv; i++) {
		if (rate * i == parent_rate_saved) {
			/*
			 * It's the most ideal case if the requested rate can be
			 * divided from parent clock without needing to change
			 * parent rate, so return the divider immediately.
			 */
			*best_parent_rate = parent_rate_saved;
			return i;
		}
		parent_rate = clk_hw_round_rate(parent, rate * i);
		now = DIV_ROUND_CLOSEST_ULL((u64)parent_rate, i);
		if (_is_best_div(rate, now, best, flags)) {
			bestdiv = i;
			best = now;
			*best_parent_rate = parent_rate;
		}
	}

	if (!bestdiv) {
		bestdiv = (1 << CCLK_DIV_WIDTH);
		*best_parent_rate = clk_hw_round_rate(parent, 1);
	}

	return bestdiv;

}

int clk_cpu_div_determine_rate(struct clk_hw *hw, struct clk_rate_request *req,
				unsigned long flags)
{
	int div;

	div = clk_cpu_bestdiv(hw, req->best_parent_hw, req->rate,
				&req->best_parent_rate, flags);

	req->rate = DIV_ROUND_CLOSEST_ULL((u64)req->best_parent_rate, div);

	return 0;
}

static int clk_gen_determine_rate_for_parent(struct clk_hw *hw,
					struct clk_rate_request *req,
					struct clk_hw *parent_hw,
					unsigned long flags)
{
	req->best_parent_hw = parent_hw;
	req->best_parent_rate = clk_hw_get_rate(parent_hw);

	return clk_cpu_div_determine_rate(hw, req, flags);
}

static int clk_cpu_determine_rate(struct clk_hw *hw,
					struct clk_rate_request *req)
{
	struct clk_hw *parent;
	unsigned long rate_diff;
	unsigned long best_rate_diff = ULONG_MAX;
	unsigned long best_rate = 0;
	int i, ret;
	unsigned long flags = clk_hw_get_flags(hw);

	req->best_parent_hw = NULL;
	if ((flags & CLK_SET_RATE_NO_REPARENT) && (flags & CLK_SET_RATE_PARENT)) {

		struct clk_rate_request tmp_req;

		parent = clk_hw_get_parent(hw);

		clk_hw_forward_rate_request(hw, req, parent, &tmp_req, req->rate);
		ret = clk_gen_determine_rate_for_parent(hw,
							&tmp_req,
							parent,
							flags);
		if (ret)
			return ret;

		req->rate = tmp_req.rate;
		req->best_parent_hw = tmp_req.best_parent_hw;
		req->best_parent_rate = tmp_req.best_parent_rate;

		return 0;
	}

	for (i = 0; i < clk_hw_get_num_parents(hw); i++) {
		struct clk_rate_request tmp_req;

		parent = clk_hw_get_parent_by_index(hw, i);
		if (!parent)
			continue;

		clk_hw_forward_rate_request(hw, req, parent, &tmp_req, req->rate);
		ret = clk_gen_determine_rate_for_parent(hw,
							&tmp_req,
							parent,
							flags);
		if (ret)
			continue;

		rate_diff = abs(req->rate - tmp_req.rate);

		if (!rate_diff || !req->best_parent_hw
				|| best_rate_diff > rate_diff) {
			req->best_parent_hw = parent;
			req->best_parent_rate = tmp_req.best_parent_rate;
			best_rate_diff = rate_diff;
			best_rate = tmp_req.rate;
		}

		if (!rate_diff)
			return 0;
	}
	req->rate = best_rate;
	return 0;
}

static long clk_cpu_round_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long *prate)
{
	struct clk_rate_request req;
	struct clk_hw *parent = clk_hw_get_parent(hw);
	int ret;

	if (!rate) {
		pr_err("%s: Invalid rate : %lu for generator clk %s\n", __func__,
			rate, clk_hw_get_name(hw));
		return rate;
	}

	clk_hw_init_rate_request(hw, &req, rate);
	req.best_parent_rate = *prate;
	req.best_parent_hw = parent;

	ret = clk_cpu_determine_rate(hw, &req);
	if (ret)
		return ret;

	*prate = req.best_parent_rate;

	return req.rate;
}

static int clk_cpu_set_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long parent_rate)
{
	struct clk_cpu *cclk = to_clk_cpu(hw);
	struct clk_cpu_div_table table;
	unsigned int div;

	if (!rate) {
		pr_err("%s: Invalid rate : %lu for generator clk %s\n", __func__,
			rate, clk_hw_get_name(hw));
		return -EINVAL;
	}

	div = DIV_ROUND_CLOSEST_ULL((u64)parent_rate, rate);

	if (!cal_div_table(div, &table)) {
		pr_err("%s: Invalid rate : %lu for generator clk %s\n", __func__,
			rate, clk_hw_get_name(hw));
		return -EINVAL;
	}

	gen_set_clear(cclk, SET_CCLK_PREDIV(table.prediv), CCLK_PRE_DIV_MASK);
	gen_set_clear(cclk, SET_CCLK_POSTDIV(table.postdiv), CCLK_POST_DIV_MASK);

	return 0;
}

static int clk_cpu_enable(struct clk_hw *hw)
{
	clk_gen_endisable(hw, 1);

	return 0;
}

static void clk_cpu_disable(struct clk_hw *hw)
{
	clk_gen_endisable(hw, 0);

}

int clk_cpu_is_enabled(struct clk_hw *hw)
{
	struct clk_cpu *cclk = to_clk_cpu(hw);
	u32 val;

	val = readl(cclk->reg) & CCLK_EN;

	return (!!val);
}

static u8 clk_cpu_get_parent(struct clk_hw *hw)
{
	struct clk_cpu *cclk = to_clk_cpu(hw);
	u32 val;

	val = (readl(cclk->reg) & CCLK_MUX_MASK) >> CCLK_MUX_SHIFT;

	return val;
}

static int clk_cpu_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_cpu *cclk = to_clk_cpu(hw);

	gen_set_clear(cclk, SET_CCLK_MUX(index), CCLK_MUX_MASK);

	return 0;
}

static const struct clk_ops clk_cpu_ops = {
	.recalc_rate = clk_cpu_recalc_rate,
	.round_rate = clk_cpu_round_rate,
	.determine_rate = clk_cpu_determine_rate,
	.set_rate = clk_cpu_set_rate,
	.enable = clk_cpu_enable,
	.disable = clk_cpu_disable,
	.is_enabled = clk_cpu_is_enabled,
	.get_parent = clk_cpu_get_parent,
	.set_parent = clk_cpu_set_parent,
};

/**
 * Register a clock generator.
 * Most clock generator have a form like
 *
 * src1 --|--\
 *        |M |--[GATE]-[PREDIV]-[POSTDIV]-
 * src2 --|--/
 *
 */
struct clk_hw *drobot_clk_register_cpu(const char *name, const char * const *parent_names,
				u8 num_parents, void __iomem *reg, unsigned long flags)
{
	struct clk_init_data init;
	struct clk_hw *hw = ERR_PTR(-ENOMEM);
	struct clk_cpu *cclk;
	int ret;

	cclk = kzalloc(sizeof(*cclk), GFP_KERNEL);
	if (!cclk)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &clk_cpu_ops;
	init.parent_names = parent_names;
	init.num_parents = num_parents;
	init.flags = flags;

	cclk->reg = reg;
	cclk->hw.init = &init;

	hw = &cclk->hw;
	ret = clk_hw_register(NULL, hw);
	if (ret) {
		kfree(cclk);
		hw = ERR_PTR(ret);
	}

	return hw;
}
