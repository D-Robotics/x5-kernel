// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/bits.h>
#include "clk.h"
#include "idle.h"

struct drobot_clk_gate {
	u8 bit_idx;
	struct clk_hw hw;
	void __iomem *reg;
	struct device *idle;
	u32 iso_id;
};

#define to_drobot_clk_gate(_hw) container_of(_hw, struct drobot_clk_gate, hw)

static int clk_gate_endisable(struct clk_hw *hw, int enable)
{
	struct drobot_clk_gate *gate = to_drobot_clk_gate(hw);
	u32 reg;
	int ret = 0;

	reg = readl(gate->reg);
	if ((reg & BIT(gate->bit_idx)) == (enable << gate->bit_idx))
		return ret;

	if (enable)
		reg |= BIT(gate->bit_idx);
	else
		reg &= ~BIT(gate->bit_idx);

	if (!enable)
		if (gate->iso_id != 0xff)
			ret = drobot_idle_request(gate->idle, gate->iso_id, true);

	writel(reg, gate->reg);

	if (enable)
		if (gate->iso_id != 0xff)
			ret = drobot_idle_request(gate->idle, gate->iso_id, false);

	return ret;
}

static int drobot_clk_gate_enable(struct clk_hw *hw)
{
	return clk_gate_endisable(hw, 1);
}

static void drobot_clk_gate_disable(struct clk_hw *hw)
{
	clk_gate_endisable(hw, 0);
}

static int drobot_clk_gate_is_enabled(struct clk_hw *hw)
{
	u32 reg;
	struct drobot_clk_gate *gate = to_drobot_clk_gate(hw);

	reg = readl(gate->reg);

	reg &= BIT(gate->bit_idx);

	return reg ? 1 : 0;
}

const struct clk_ops drobot_clk_gate_ops = {
	.enable = drobot_clk_gate_enable,
	.disable = drobot_clk_gate_disable,
	.is_enabled = drobot_clk_gate_is_enabled,
};

struct clk_hw *drobot_clk_hw_register_gate(const char *name, const char *parent_name, void __iomem *reg,
				       u8 bit_idx, unsigned long flags, struct device *idle,
				       u32 iso_id)
{
	struct drobot_clk_gate *gate;
	struct clk_hw *hw;
	struct clk_init_data init = {};
	int ret = -EINVAL;

	/* allocate the gate */
	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &drobot_clk_gate_ops;
	init.flags = flags;
	init.parent_names = parent_name ? &parent_name : NULL;
	if (parent_name)
		init.num_parents = 1;
	else
		init.num_parents = 0;

	gate->reg = reg;
	gate->bit_idx = bit_idx;
	gate->hw.init = &init;
	gate->idle = idle;
	gate->iso_id = iso_id;

	hw = &gate->hw;
	ret = clk_hw_register(NULL, hw);
	if (ret) {
		kfree(gate);
		hw = ERR_PTR(ret);
	}

	return hw;
}
