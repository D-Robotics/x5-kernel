/*
 * Copyright (c) 2023 horizon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/panic_notifier.h>

struct syscon_reboot_reason {
	struct regmap *map;
	u32 offset;
	u32 panic_magic;
	u32 mask;
	struct notifier_block panic_blk;
};

static int reset_reason_panic_handler(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	int ret = 0;
	struct syscon_reboot_reason *syscon_rbm =
		container_of(this, struct syscon_reboot_reason, panic_blk);

	ret = regmap_update_bits(syscon_rbm->map, syscon_rbm->offset,
	                         syscon_rbm->mask, syscon_rbm->panic_magic);
	if (ret < 0) {
		pr_err("update reboot panic bits failed\n");
		return -1;
	}
	return NOTIFY_DONE;
}

static int syscon_reboot_reason_probe(struct platform_device *pdev)
{
	struct syscon_reboot_reason *syscon_rbm;

	syscon_rbm = devm_kzalloc(&pdev->dev, sizeof(*syscon_rbm), GFP_KERNEL);
	if (!syscon_rbm)
		return -ENOMEM;

	syscon_rbm->panic_blk.notifier_call = reset_reason_panic_handler;
	atomic_notifier_chain_register(&panic_notifier_list,
				&syscon_rbm->panic_blk);

	syscon_rbm->map = syscon_node_to_regmap(pdev->dev.parent->of_node);
	if (IS_ERR(syscon_rbm->map))
		return PTR_ERR(syscon_rbm->map);

	if (of_property_read_u32(pdev->dev.of_node, "offset",
	    &syscon_rbm->offset))
		return -EINVAL;

	if (of_property_read_u32(pdev->dev.of_node, "mask",
	    &syscon_rbm->mask))
		return -EINVAL;

	if (of_property_read_u32(pdev->dev.of_node, "panic_magic",
	    &syscon_rbm->panic_magic))
		return -EINVAL;

	return 0;
}

static const struct of_device_id syscon_reboot_reason_of_match[] = {
	{ .compatible = "syscon-reboot-reason" },
	{}
};
MODULE_DEVICE_TABLE(of, syscon_reboot_reason_of_match);

static struct platform_driver syscon_reboot_reason_driver = {
	.probe = syscon_reboot_reason_probe,
	.driver = {
		.name = "syscon-reboot-reason",
		.of_match_table = syscon_reboot_reason_of_match,
	},
};
module_platform_driver(syscon_reboot_reason_driver);

MODULE_AUTHOR("Ming.yu <ming.yu@horizon.cc");
MODULE_DESCRIPTION("SYSCON reboot reason driver");
MODULE_LICENSE("GPL v2");
