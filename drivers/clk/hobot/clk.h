// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef __CLK_H
#define __CLK_H

#include <linux/clk-provider.h>
#include "pmu.h"

#define drobot_clk_hw_register_gate_no_idle(name, parent_name, reg, bit_idx, flags) \
	drobot_clk_hw_register_gate(name, parent_name, reg, bit_idx, flags, NULL, 0xff)

#define drobot_clk_register_generator_no_idle(name, parent_names, num_parents, reg) \
	drobot_clk_register_generator(name, parent_names, num_parents, reg, 0, NULL, 0xff)

#define drobot_clk_register_generator_flags_no_idle(name, parent_names, num_parents, reg, flags) \
	drobot_clk_register_generator(name, parent_names, num_parents, reg, flags, NULL, 0xff)

#define drobot_clk_register_gen_no_flags(name, parent_names, num_parents, reg, idle, iso_id) \
	drobot_clk_register_generator(name, parent_names, num_parents, reg, 0, idle, iso_id)

static inline struct clk_hw *drobot_clk_hw_mux(const char *name, const char * const *parents,
		int num_parents, unsigned long flags, void __iomem *reg, u8 shift, u8 width)
{
	return clk_hw_register_mux(NULL, name, parents, num_parents,
			flags, reg, shift,
			width, 0, NULL);
}

static inline struct clk_hw *drobot_clk_hw_divider(const char *name, const char *parent, unsigned long flags,
					void __iomem *reg, u8 shift, u8 width)
{
	return clk_hw_register_divider(NULL, name, parent, flags,
				       reg, shift, width, CLK_DIVIDER_ROUND_CLOSEST, NULL);
}

struct clk_hw *drobot_clk_hw_register_gate(const char *name, const char *parent_name, void __iomem *reg,
				       u8 bit_idx, unsigned long flags, struct device *idle,
				       u32 iso_id);

struct clk_hw *drobot_clk_register_generator(const char *name, const char *const *parent_names,
					 u8 num_parents, void __iomem *reg, unsigned long flags,
					 struct device *idle, u32 iso_id);

struct clk_hw *drobot_clk_register_i2s_generator(const char *name, const char *const *parent_names,
					     u8 num_parents, void __iomem *reg,
					     void __iomem *mux_reg, u8 offset, unsigned long flags,
					     struct device *idle, u32 iso_id);

struct clk_hw *drobot_clk_register_cpu(const char *name, const char * const *parent_names,
				u8 num_parents, void __iomem *reg, unsigned long flags);

#endif
