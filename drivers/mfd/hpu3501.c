// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Horizon Robotics
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/mfd/hpu3501.h>

static const struct mfd_cell hpu3501_devs[] = {
	{
		.name = "hpu3501-regulator",
	},
	{
		.name = "hpu3501-rtc",
	},
};

static const struct regmap_config hpu3501_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static inline struct hpu3501_dev *dev_to_hpu3501(struct device *dev)
{
	return i2c_get_clientdata(to_i2c_client(dev));
}

int hpu3501_write(struct device *dev, int reg, uint8_t val)
{
	struct hpu3501_dev *hpu3501 = dev_to_hpu3501(dev);

	return regmap_write(hpu3501->regmap, reg, val);
}
EXPORT_SYMBOL_GPL(hpu3501_write);

int hpu3501_read(struct device *dev, int reg, uint8_t *val)
{
	struct hpu3501_dev *hpu3501 = dev_to_hpu3501(dev);
	unsigned int rval;
	int ret;

	ret = regmap_read(hpu3501->regmap, reg, &rval);
	if (!ret)
		*val = rval;
	return ret;
}
EXPORT_SYMBOL_GPL(hpu3501_read);

int hpu3501_writes(struct device *dev, int reg, int len, uint8_t *val)
{
	struct hpu3501_dev *hpu3501 = dev_to_hpu3501(dev);

	return regmap_bulk_write(hpu3501->regmap, reg, val, len);
}
EXPORT_SYMBOL_GPL(hpu3501_writes);

int hpu3501_reads(struct device *dev, int reg, int len, uint8_t *val)
{
	struct hpu3501_dev *hpu3501 = dev_to_hpu3501(dev);

	return regmap_bulk_read(hpu3501->regmap, reg, val, len);
}
EXPORT_SYMBOL_GPL(hpu3501_reads);

int hpu3501_set_bits(struct device *dev, int reg, uint8_t bit_mask)
{
	struct hpu3501_dev *hpu3501 = dev_to_hpu3501(dev);

	return regmap_update_bits(hpu3501->regmap, reg, bit_mask, bit_mask);
}
EXPORT_SYMBOL_GPL(hpu3501_set_bits);

int hpu3501_clr_bits(struct device *dev, int reg, uint8_t bit_mask)
{
	struct hpu3501_dev *hpu3501 = dev_to_hpu3501(dev);

	return regmap_update_bits(hpu3501->regmap, reg, bit_mask, 0);
}
EXPORT_SYMBOL_GPL(hpu3501_clr_bits);

static int hpu3501_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	int ret;

	struct hpu3501_dev *hpu3501;

	hpu3501 =
		devm_kzalloc(&i2c->dev, sizeof(struct hpu3501_dev), GFP_KERNEL);
	if (!hpu3501)
		return -ENOMEM;

	i2c_set_clientdata(i2c, hpu3501);
	hpu3501->dev = &i2c->dev;
	hpu3501->i2c_client = i2c;

	hpu3501->regmap = devm_regmap_init_i2c(i2c, &hpu3501_regmap_config);
	if (IS_ERR(hpu3501->regmap)) {
		ret = PTR_ERR(hpu3501->regmap);
		dev_err(&i2c->dev, "regmap init failed: %d\n", ret);
		return ret;
	}

	ret = devm_mfd_add_devices(hpu3501->dev, PLATFORM_DEVID_AUTO,
				   hpu3501_devs, ARRAY_SIZE(hpu3501_devs), NULL,
				   0, NULL);

	return ret;
}

static const struct i2c_device_id hpu3501_i2c_id[] = { { "hpu3501", 0 }, {} };
MODULE_DEVICE_TABLE(i2c, hpu3501_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id hpu3501_of_match[] = {
	{
		.compatible = "hobot, hpu3501",
	},
	{},
};
MODULE_DEVICE_TABLE(of, hpu3501_of_match);
#endif

static struct i2c_driver hpu3501_i2c_driver = {
	.driver = {
		   .name = "hpu3501",
		   .of_match_table = of_match_ptr(hpu3501_of_match),
	},
	.probe = hpu3501_i2c_probe,
	.id_table = hpu3501_i2c_id,
};

static int __init hpu3501_i2c_init(void)
{
	return i2c_add_driver(&hpu3501_i2c_driver);
}

/* init early so consumer devices can complete system boot */
subsys_initcall(hpu3501_i2c_init);

static void __exit hpu3501_i2c_exit(void)
{
	i2c_del_driver(&hpu3501_i2c_driver);
}
module_exit(hpu3501_i2c_exit);

MODULE_DESCRIPTION("hpu3501 multi-function driver");
MODULE_AUTHOR("chaohang.cheng <chaohang.cheng@horizon.ai>");
MODULE_LICENSE("GPL v2");
