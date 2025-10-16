/*
 * Horizon Robotics
 *
 *  Copyright (C) 2020 Horizon Robotics Inc.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/kobject.h>

#include "socinfo.h"

#define SOCINFO_NAME		"socinfo"
#define BUF_LEN		128

struct _reg_info {
	void __iomem *map;
	/* addr, size, offset, mask */
	u32 reg_info[4];
};

struct _DDR_vendor_info {
	u8 vendor_id;
	char *vendor_name;
};

const char *soc_gen;
const char *soc_name;
const char *hw_name;
const char *board_version;
const char *hw_info;
const char *bootdevice_name;
const char *soc_uid;
const char *ddr_vendor;
const char *ddr_type;
const char *ddr_freq;
const char *ddr_size;
const char *board_id;
const char *sec_chip;
const char *sec_boot;

static void __iomem *sec_flag_addr;
struct _reg_info g_bak_slot_info;
struct _reg_info g_boot_count;

const static struct _DDR_vendor_info ddr_vendor_info[] = {
	{
		.vendor_id = 0x1,
		.vendor_name = "Samsung",
	},
	{
		.vendor_id = 0x5,
		.vendor_name = "Nanya",
	},
	{
		.vendor_id = 0x6,
		.vendor_name = "SK hynix",
	},
	{
		.vendor_id = 0x8,
		.vendor_name = "Winbond",
	},
	{
		.vendor_id = 0x9,
		.vendor_name = "ESMT",
	},
	{
		.vendor_id = 0x13,
		.vendor_name = "cxmt",
	},
	{
		.vendor_id = 0x1A,
		.vendor_name = "Xi'an UniIC Semiconductors",
	},
	{
		.vendor_id = 0x1B,
		.vendor_name = "ISSI",
	},
	{
		.vendor_id = 0x1C,
		.vendor_name = "JSC",
	},
	{
		.vendor_id = 0xC5,
		.vendor_name = "SINKER",
	},
	{
		.vendor_id = 0xE5,
		.vendor_name = "Dosilicon Co,Ltd",
	},
	{
		.vendor_id = 0xF8,
		.vendor_name = "Fidelix",
	},
	{
		.vendor_id = 0xF9,
		.vendor_name = "Ultra Memory",
	},
	{
		.vendor_id = 0xFD,
		.vendor_name = "AP Memory",
	},
	{
		.vendor_id = 0xFF,
		.vendor_name = "Micron",
	},
	{
		.vendor_id = 0x00,
		.vendor_name = "unkown",
	},
};

const char *ddr_freq_array[] = {"unkown", "3200", "3733", "4266"};
const char *ddr_type_array[] = {"unkown", "LPDDR4", "LPDDR4X"};
const char *ddr_size_array[] = {"1024MB", "2048MB", "4096MB", "8192MB", "6144MB"};

ssize_t soc_gen_show(struct class *class,
		     struct class_attribute *attr, char *buf)
{
	if (!buf || !soc_gen)
		return 0;
	snprintf(buf, BUF_LEN, "%s\n", soc_gen);

	return strlen(buf);
}

ssize_t soc_name_show(struct class *class,
		      struct class_attribute *attr, char *buf)
{
	if (!buf || !soc_name)
		return 0;
	snprintf(buf, BUF_LEN, "%s\n", soc_name);

	return strlen(buf);
}

ssize_t hw_name_show(struct class *class,
		     struct class_attribute *attr, char *buf)
{
	if (!buf || !hw_name)
		return 0;
	snprintf(buf, BUF_LEN, "%s\n", hw_name);

	return strlen(buf);
}

ssize_t board_version_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	if (!buf || !board_version)
		return 0;
	snprintf(buf, BUF_LEN, "%s\n", board_version);

	return strlen(buf);
}

ssize_t hw_info_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	if (!buf || !hw_info || !ddr_vendor || !ddr_size || !ddr_freq)
		return 0;
	snprintf(buf, BUF_LEN, "%s_%s_%s_%s\n",
			 hw_info, ddr_vendor, ddr_size, ddr_freq);

	return strlen(buf);
}

ssize_t bootdevice_name_show(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	if (!buf || !bootdevice_name)
		return 0;
	snprintf(buf, BUF_LEN, "%s\n", bootdevice_name);

	return strlen(buf);
}

ssize_t soc_uid_show(struct class *class,
		     struct class_attribute *attr, char *buf)
{
	if (!buf || !soc_uid)
		return 0;
	snprintf(buf, BUF_LEN, "%s\n", soc_uid);

	return strlen(buf);
}

ssize_t ddr_vender_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	if (!buf || !ddr_vendor)
		return 0;
	snprintf(buf, BUF_LEN, "%s\n", ddr_vendor);

	return strlen(buf);
}

ssize_t ddr_type_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	if (!buf || !ddr_type)
		return 0;
	snprintf(buf, BUF_LEN, "%s\n", ddr_type);

	return strlen(buf);
}

ssize_t ddr_freq_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	if (!buf || !ddr_freq)
		return 0;
	snprintf(buf, BUF_LEN, "%s Mbps\n", ddr_freq);

	return strlen(buf);
}

ssize_t ddr_size_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	if (!buf || !ddr_size)
		return 0;
	snprintf(buf, BUF_LEN, "%s\n", ddr_size);

	return strlen(buf);
}

ssize_t board_id_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	if (!buf || !board_id)
		return 0;

	snprintf(buf, BUF_LEN, "%s\n", board_id);

	return strlen(buf);
}

ssize_t sec_chip_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	if (!buf || !sec_chip)
		return 0;
	snprintf(buf, BUF_LEN, "%s\n", sec_chip);

	return strlen(buf);
}

ssize_t sec_boot_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	if (!buf || !sec_boot)
	return 0;
	snprintf(buf, BUF_LEN, "%s\n", sec_boot);

	return strlen(buf);
}

ssize_t bak_slot_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	uint32_t bak_slot = 0;
	if (!buf)
		return 0;
	bak_slot = (readl(g_bak_slot_info.map) & g_bak_slot_info.reg_info[3]) >> g_bak_slot_info.reg_info[2];
	snprintf(buf, BUF_LEN, "%d\n", bak_slot);

	return strlen(buf);
}

ssize_t bak_slot_store(struct class *class, struct class_attribute *attr,
				const char *buf, size_t count)
{
	uint32_t cur_boot_count = 0;
	uint32_t old_boot_count = 0;

	sscanf(buf, "%du", &cur_boot_count);
	old_boot_count = readl(g_boot_count.map) & ~g_boot_count.reg_info[3];
	cur_boot_count = old_boot_count | (cur_boot_count << g_boot_count.reg_info[2]);
	writel(cur_boot_count, g_boot_count.map);
	return count;
}

static struct class_attribute soc_gen_attribute =
	__ATTR(soc_gen, 0444, soc_gen_show, NULL);

static struct class_attribute soc_name_attribute =
	__ATTR(soc_name, 0444, soc_name_show, NULL);

static struct class_attribute hw_name_attribute =
	__ATTR(hw_name, 0444, hw_name_show, NULL);

static struct class_attribute board_version_attribute =
	__ATTR(board_version, 0444, board_version_show, NULL);

static struct class_attribute hw_info_attribute =
	__ATTR(hw_info, 0444, hw_info_show, NULL);

static struct class_attribute bootdevice_name_attribute =
	__ATTR(bootdevice_name, 0444, bootdevice_name_show, NULL);

static struct class_attribute soc_uid_attribute =
	__ATTR(soc_uid, 0444, soc_uid_show, NULL);

static struct class_attribute ddr_vender_attribute =
	__ATTR(ddr_vendor, 0444, ddr_vender_show, NULL);

static struct class_attribute ddr_type_attribute =
	__ATTR(ddr_type, 0444, ddr_type_show, NULL);

static struct class_attribute ddr_freq_attribute =
	__ATTR(ddr_freq, 0444, ddr_freq_show, NULL);

static struct class_attribute ddr_size_attribute =
	__ATTR(ddr_size, 0444, ddr_size_show, NULL);

static struct class_attribute board_id_attribute =
	__ATTR(board_id, 0444, board_id_show, NULL);

static struct class_attribute sec_chip_attribute =
	__ATTR(sec_chip, 0444, sec_chip_show, NULL);

	static struct class_attribute sec_boot_attribute =
	__ATTR(sec_boot, 0444, sec_boot_show, NULL);

static struct class_attribute bak_slot_attribute =
	__ATTR(bak_slot, 0644, bak_slot_show, bak_slot_store);

static struct attribute *socinfo_attributes[] = {
	&soc_gen_attribute.attr,
	&soc_name_attribute.attr,
	&hw_name_attribute.attr,
	&board_version_attribute.attr,
	&hw_info_attribute.attr,
	&bootdevice_name_attribute.attr,
	&soc_uid_attribute.attr,
	&ddr_vender_attribute.attr,
	&ddr_type_attribute.attr,
	&ddr_freq_attribute.attr,
	&ddr_size_attribute.attr,
	&board_id_attribute.attr,
	&sec_chip_attribute.attr,
	&sec_boot_attribute.attr,
	&bak_slot_attribute.attr,
	NULL
};

static const struct attribute_group socinfo_group = {
	.attrs = socinfo_attributes,
};

static const struct attribute_group *socinfo_attr_group[] = {
	&socinfo_group,
	NULL,
};

static struct class socinfo_class = {
	.name = SOCINFO_NAME,
	.class_groups = socinfo_attr_group,
};

/* Match table for of_platform binding */
static const struct of_device_id socinfo_of_match[] = {
	{ .compatible = "hobot,boardinfo", },
	{}
};

MODULE_DEVICE_TABLE(of, socinfo_of_match);

static int read_from_property(struct platform_device *pdev,
			      const char *propname, const char **buf)
{
	int ret = 0;

	ret = of_property_read_string(pdev->dev.of_node, propname,
				      buf);
	if (ret != 0) {
		pr_err("of_property_read_string %s error\n", propname);
	}

	return ret;
}

static int socinfo_probe(struct platform_device *pdev)
{
	int ret = 0, i;
	struct resource *resource = NULL;
	static void __iomem *ddr_info_addr = NULL;
	uint32_t ddr_info = 0;

	dev_info(&pdev->dev, "Start socinfo probe.\n");

	if (read_from_property(pdev, "soc_gen", &soc_gen) ||
		read_from_property(pdev, "soc_name", &soc_name) ||
		read_from_property(pdev, "hw_name", &hw_name) ||
		read_from_property(pdev, "board_version", &board_version) ||
		read_from_property(pdev, "hw_info", &hw_info) ||
		read_from_property(pdev, "bootdevice_name", &bootdevice_name) ||
		read_from_property(pdev, "soc_uid", &soc_uid) ||
		read_from_property(pdev, "board_id", &board_id) ||
		read_from_property(pdev, "sec_chip", &sec_chip) ||
		read_from_property(pdev, "sec_boot", &sec_boot)) {
		return -1;
	}

	ret = of_property_read_u32_array(pdev->dev.of_node,
					 "bak_slot_reg",
					 g_bak_slot_info.reg_info,
					 ARRAY_SIZE(g_bak_slot_info.reg_info));
	if (ret != 0) {
		pr_err("of_property_read_u32_array error\n");
		return ret;
	}

	g_bak_slot_info.map = ioremap(g_bak_slot_info.reg_info[0],
					 g_bak_slot_info.reg_info[1]);
	if (!g_bak_slot_info.map)
		return -ENOMEM;

	ret = of_property_read_u32_array(pdev->dev.of_node,
					 "boot_count_reg",
					 g_boot_count.reg_info,
					 ARRAY_SIZE(g_boot_count.reg_info));
	if (ret != 0) {
		pr_err("of_property_read_u32_array error\n");
		return ret;
	}

	g_boot_count.map = ioremap(g_boot_count.reg_info[0],
					 g_boot_count.reg_info[1]);
	if (!g_boot_count.map)
		return -ENOMEM;

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (resource == NULL) {
		dev_err(&pdev->dev, "Can't get ddr info resource error\n");
		return -ENODEV;
	}

	ddr_info_addr = devm_ioremap_resource(&pdev->dev, resource);
	if (IS_ERR(ddr_info_addr)) {
		dev_err(&pdev->dev, "Can't get ddr info resource\n");
		return (int32_t)PTR_ERR(ddr_info_addr);
	}
	ddr_info = readl(ddr_info_addr);
	ddr_freq = ddr_freq_array[DR_DDR_FREQ(ddr_info)];
	ddr_size = ddr_size_array[DR_DDR_SIZE(ddr_info)];
	ddr_type = ddr_type_array[DR_DDR_TYPE(ddr_info)];
	for (i = 0; i < ARRAY_SIZE(ddr_vendor_info); i++) {
		if (DR_DDR_VENDOR(ddr_info) == ddr_vendor_info[i].vendor_id ||
			i == ARRAY_SIZE(ddr_vendor_info) - 1) {
			ddr_vendor = ddr_vendor_info[i].vendor_name;
			break;
		}
	}

	ret = class_register(&socinfo_class);
	dev_info(&pdev->dev, "Socinfo probe end with retval: %d\n", ret);

	if (ret < 0)
		return ret;

	return 0;
}

static int socinfo_remove(struct platform_device *pdev)
{
	iounmap(sec_flag_addr);
	iounmap(g_boot_count.map);
	iounmap(g_bak_slot_info.map);
	class_unregister(&socinfo_class);
	return 0;
}

static struct platform_driver socinfo_platform_driver = {
	.probe   = socinfo_probe,
	.remove  = socinfo_remove,
	.driver  = {
		.name = SOCINFO_NAME,
		.of_match_table = socinfo_of_match,
		},
};

static int __init socinfo_init(void)
{
	int retval = 0;
	pr_info("Start socinfo driver init\n");
	/* Register the platform driver */
	retval = platform_driver_register(&socinfo_platform_driver);
	if (retval)
		pr_err("Unable to register platform driver\n");

	return retval;
}

static void __exit socinfo_exit(void)
{
	/* Unregister the platform driver */
	platform_driver_unregister(&socinfo_platform_driver);

}

module_init(socinfo_init);
module_exit(socinfo_exit);

MODULE_DESCRIPTION("Driver for SOCINFO");
MODULE_AUTHOR("Horizon Inc.");
MODULE_LICENSE("GPL");
