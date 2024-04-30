// SPDX-License-Identifier: GPL-2.0

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_device.h>

struct clk_external_io {
	struct		clk_hw hw;
	unsigned long	rate;
};

#define to_clk_external_io(_hw) container_of(_hw, struct clk_external_io, hw)

static long clk_external_io_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long* parent_rate)
{
	return rate;
}

static int clk_external_io_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	to_clk_external_io(hw)->rate = rate;
	return 0;
}

static unsigned long clk_external_io_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	return to_clk_external_io(hw)->rate;
}

const struct clk_ops clk_external_io_ops = {
	.recalc_rate = clk_external_io_recalc_rate,
	.round_rate = clk_external_io_round_rate,
	.set_rate = clk_external_io_set_rate,
};
EXPORT_SYMBOL_GPL(clk_external_io_ops);

static void devm_clk_hw_register_external_io_rate_release(struct device *dev, void *res)
{
	struct clk_external_io *fix = res;

	/*
	 * We can not use clk_hw_unregister_external_io_rate, since it will kfree()
	 * the hw, resulting in double free. Just unregister the hw and let
	 * devres code kfree() it.
	 */
	clk_hw_unregister(&fix->hw);
}

struct clk_hw *__clk_hw_register_external_io_rate(struct device *dev,
		struct device_node *np, const char *name,
		unsigned long rate, bool devm)
{
	struct clk_external_io *fixed;
	struct clk_hw *hw;
	struct clk_init_data init = {};
	int ret = -EINVAL;

	/* allocate fixed-rate clock */
	if (devm)
		fixed = devres_alloc(devm_clk_hw_register_external_io_rate_release,
				     sizeof(*fixed), GFP_KERNEL);
	else
		fixed = kzalloc(sizeof(*fixed), GFP_KERNEL);
	if (!fixed)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &clk_external_io_ops;
	init.num_parents = 0;

	/* struct clk_external_io assignments */
	fixed->rate = rate;
	fixed->hw.init = &init;

	/* register the clock */
	hw = &fixed->hw;
	if (dev || !np)
		ret = clk_hw_register(dev, hw);
	else
		ret = of_clk_hw_register(np, hw);
	if (ret) {
		if (devm)
			devres_free(fixed);
		else
			kfree(fixed);
		hw = ERR_PTR(ret);
	} else if (devm)
		devres_add(dev, fixed);

	return hw;
}
EXPORT_SYMBOL_GPL(__clk_hw_register_external_io_rate);

struct clk *clk_register_external_io_rate(struct device *dev, const char *name,
		unsigned long rate)
{
	struct clk_hw *hw;

	hw = __clk_hw_register_external_io_rate(dev, NULL, name, rate, false);
	if (IS_ERR(hw))
		return ERR_CAST(hw);
	return hw->clk;
}
EXPORT_SYMBOL_GPL(clk_register_external_io_rate);

void clk_unregister_external_io_rate(struct clk *clk)
{
	struct clk_hw *hw;

	hw = __clk_get_hw(clk);
	if (!hw)
		return;

	clk_unregister(clk);
	kfree(to_clk_external_io(hw));
}
EXPORT_SYMBOL_GPL(clk_unregister_external_io_rate);

void clk_hw_unregister_external_io_rate(struct clk_hw *hw)
{
	struct clk_external_io *fixed;

	fixed = to_clk_external_io(hw);

	clk_hw_unregister(hw);
	kfree(fixed);
}
EXPORT_SYMBOL_GPL(clk_hw_unregister_external_io_rate);

#ifdef CONFIG_OF
static struct clk_hw *_of_external_io_clk_setup(struct device_node *node)
{
	struct clk_hw *hw;
	const char *clk_name = node->name;
	u32 rate;
	int ret;

	if (of_property_read_u32(node, "clock-frequency", &rate))
		return ERR_PTR(-EIO);

	of_property_read_string(node, "clock-output-names", &clk_name);

	hw = __clk_hw_register_external_io_rate(NULL, node, clk_name, rate, false);
	if (IS_ERR(hw))
		return hw;

	ret = of_clk_add_hw_provider(node, of_clk_hw_simple_get, hw);
	if (ret) {
		clk_hw_unregister_external_io_rate(hw);
		return ERR_PTR(ret);
	}

	return hw;
}

/**
 * of_external_io_clk_setup() - Setup function for simple fixed rate clock
 * @node:	device node for the clock
 */
void __init of_external_io_clk_setup(struct device_node *node)
{
	_of_external_io_clk_setup(node);
}
CLK_OF_DECLARE(fixed_clk, "external-io-clock", of_external_io_clk_setup);

static int of_external_io_clk_remove(struct platform_device *pdev)
{
	struct clk_hw *hw = platform_get_drvdata(pdev);

	of_clk_del_provider(pdev->dev.of_node);
	clk_hw_unregister_external_io_rate(hw);

	return 0;
}

static int of_external_io_clk_probe(struct platform_device *pdev)
{
	struct clk_hw *hw;

	/*
	 * This function is not executed when of_external_io_clk_setup
	 * succeeded.
	 */
	hw = _of_external_io_clk_setup(pdev->dev.of_node);
	if (IS_ERR(hw))
		return PTR_ERR(hw);

	platform_set_drvdata(pdev, hw);

	return 0;
}

static const struct of_device_id of_external_io_clk_ids[] = {
	{ .compatible = "external-io-clock" },
	{ }
};

static struct platform_driver of_external_io_clk_driver = {
	.driver = {
		.name = "of_external_io_clk",
		.of_match_table = of_external_io_clk_ids,
	},
	.probe = of_external_io_clk_probe,
	.remove = of_external_io_clk_remove,
};
builtin_platform_driver(of_external_io_clk_driver);
#endif
