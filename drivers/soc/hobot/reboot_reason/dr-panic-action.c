/*
 * Copyright (c) 2023 D-Robotics
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

#define BIT_OFFSET 12

struct syscon_panic_action {
	struct regmap *map;
	u32 offset;
	u32 mask;
};

static const char * const dr_panic_action_desp[] = {
	"normal", "ubootonce", "udumpfastboot"
};

struct syscon_panic_action *syscon_pc;
static struct kobject *k_obj;

static ssize_t dr_panic_action_show(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
	int ret;
	u32 value = -1;
	const char * const *srd = dr_panic_action_desp;

	ret = regmap_read(syscon_pc->map, syscon_pc->offset, &value);
	if (ret < 0) {
		pr_err("read panic action bits failed\n");
		return -1;
	}

	value = (value & syscon_pc->mask) >> BIT_OFFSET;

	return sprintf(buf, "%d: %s\n", value, srd[value]);
}

static ssize_t dr_panic_action_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf, size_t count)
{
	int ret, i;
	u32 value = -1;
	const char * const *srd = dr_panic_action_desp;
	int srd_n = ARRAY_SIZE(dr_panic_action_desp);

	for (i = 0; i < (srd_n - 1); i++) {
		if (strncmp(buf, srd[i], sizeof(*srd[i])) == 0)
			value = i;
	}
	if (value == -1)
		ret = kstrtouint(buf, 0, &value);

	ret = regmap_update_bits(syscon_pc->map, syscon_pc->offset,
			syscon_pc->mask, value << BIT_OFFSET);

	if (ret < 0) {
		pr_err("update panic action bits failed\n");
		return -1;
	}

	return count;
}

static DEVICE_ATTR(panic_action, S_IWUSR | S_IRUSR, dr_panic_action_show, dr_panic_action_store);

static int dr_panic_action_probe(struct platform_device *pdev)
{
	syscon_pc = devm_kzalloc(&pdev->dev, sizeof(*syscon_pc), GFP_KERNEL);
	if (!syscon_pc)
		return -ENOMEM;

	syscon_pc->map = syscon_node_to_regmap(pdev->dev.parent->of_node);
	if (IS_ERR(syscon_pc->map))
		return PTR_ERR(syscon_pc->map);

	if (of_property_read_u32(pdev->dev.of_node, "offset", &syscon_pc->offset))
		return -EINVAL;

	if (of_property_read_u32(pdev->dev.of_node, "mask", &syscon_pc->mask))
		return -EINVAL;

	platform_set_drvdata(pdev, syscon_pc);

	k_obj = kobject_create_and_add("hobot-swinfo", kernel_kobj);
	if (k_obj) {
		if (sysfs_create_file(k_obj, &dev_attr_panic_action.attr)) {
			pr_warn("panic action sys node create error\n");
			kobject_put(k_obj);
			k_obj = NULL;
		}
	} else {
		pr_warn("panic action sys node create error\n");
	}

	return 0;
}

static int dr_panic_action_remove(struct platform_device *pdev)
{
	if (k_obj) {
		sysfs_remove_file(k_obj, &dev_attr_panic_action.attr);
		kobject_put(k_obj);
	}
	return 0;
}

static const struct of_device_id dr_panic_action_of_match[] = {
	{ .compatible = "d-robotics,panic-action" },
	{}
};

MODULE_DEVICE_TABLE(of, dr_panic_action_of_match);

static struct platform_driver dr_panic_action_driver = {
	.probe = dr_panic_action_probe,
	.remove = dr_panic_action_remove,
	.driver = {
		.name = "dr-panic-action",
		.of_match_table = dr_panic_action_of_match,
	},
};

module_platform_driver(dr_panic_action_driver);

MODULE_AUTHOR("Zhiwei01.li <Zhiwei01.li@horizon.cc");
MODULE_DESCRIPTION("D-Robotics panic action driver");
MODULE_LICENSE("GPL v2");
