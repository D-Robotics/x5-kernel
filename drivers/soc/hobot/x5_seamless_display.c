// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/soc/hobot/x5_seamless_display.h>

#define X5_SEAMLESS_STATE_UNKNOWN	(-1)
#define X5_SEAMLESS_STATE_DISABLED	0
#define X5_SEAMLESS_STATE_ENABLED	1

static int x5_seamless_state = X5_SEAMLESS_STATE_UNKNOWN;

static int x5_read_chosen_seamless_state(void)
{
#if IS_ENABLED(CONFIG_OF)
	u32 state;

	if (!of_chosen)
		return X5_SEAMLESS_STATE_UNKNOWN;

	if (!of_property_read_u32(of_chosen, "d-robotics,seamless-display-state", &state))
		return state == 1 ? X5_SEAMLESS_STATE_ENABLED : X5_SEAMLESS_STATE_DISABLED;
#endif

	return X5_SEAMLESS_STATE_UNKNOWN;
}

static int __init x5_seamless_display_init_state(void)
{
	int state;
	const char *source;

	state = x5_read_chosen_seamless_state();
	if (state == X5_SEAMLESS_STATE_UNKNOWN) {
		state = X5_SEAMLESS_STATE_DISABLED;
		source = "default-disabled";
	} else {
		source = "/chosen:d-robotics,seamless-display-state";
	}

	WRITE_ONCE(x5_seamless_state, state);
	pr_info("x5_seamless_display: %s (source=%s)\n",
		state == X5_SEAMLESS_STATE_ENABLED ? "enabled" : "disabled", source);

	return 0;
}
early_initcall(x5_seamless_display_init_state);

bool x5_seamless_display_active(void)
{
	int state = READ_ONCE(x5_seamless_state);

	if (state != X5_SEAMLESS_STATE_UNKNOWN)
		return state == X5_SEAMLESS_STATE_ENABLED;

	state = x5_read_chosen_seamless_state();
	if (state == X5_SEAMLESS_STATE_UNKNOWN)
		state = X5_SEAMLESS_STATE_DISABLED;

	cmpxchg(&x5_seamless_state, X5_SEAMLESS_STATE_UNKNOWN, state);
	state = READ_ONCE(x5_seamless_state);

	return state == X5_SEAMLESS_STATE_ENABLED;
}
EXPORT_SYMBOL_GPL(x5_seamless_display_active);

bool x5_chosen_has_simple_framebuffer(void)
{
	struct device_node *chosen, *child;
	bool found = false;

#if IS_ENABLED(CONFIG_OF)
	chosen = of_find_node_by_path("/chosen");
	if (!chosen)
		return false;

	for_each_child_of_node(chosen, child) {
		if (of_device_is_compatible(child, "simple-framebuffer")) {
			found = true;
			of_node_put(child);
			break;
		}
	}
	of_node_put(chosen);
#endif
	return found;
}
EXPORT_SYMBOL_GPL(x5_chosen_has_simple_framebuffer);
