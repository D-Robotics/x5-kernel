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
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regmap.h>
#include <linux/mfd/hpu3501.h>
#include <linux/delay.h>

#include "./hpu3501-regulator.h"

extern int hpu3501_write(struct device *dev, int reg, uint8_t val);

#ifdef CONFIG_OF
static struct of_regulator_match hpu3501_matches[] = {
	[ID_BUCK1] = { .name = "BUCK1" },
	[ID_BUCK2] = { .name = "BUCK2" },
	[ID_BUCK3] = { .name = "BUCK3" },
	[ID_BUCK4] = { .name = "BUCK4" },
	[ID_BUCK5] = { .name = "BUCK5" },

	[ID_LDO1] = { .name = "LDO1" },
	[ID_LDO2_CFG1] = { .name = "LDO2_CFG1" },
	[ID_LDO2_CFG2] = { .name = "LDO2_CFG2" },
	[ID_LDO3] = { .name = "LDO3" },
};

static int hpu3501_pdata_from_dt(struct device *dev,
				 struct hpu3501_platform_data *pdata)
{
	int matched, i, num_matches;
	struct device_node *np;
	struct hpu3501_regulator_data *regulator;
	struct of_regulator_match *matches;

	matches = hpu3501_matches;
	num_matches = ARRAY_SIZE(hpu3501_matches);

	np = of_get_child_by_name(dev->of_node, "hpu3501-regulator");
	if (!np) {
		dev_err(dev, "missing 'hpu3501-regulator' subnode in DT, %d\n",
			-EINVAL);
		return -EINVAL;
	}

	np = of_get_child_by_name(np, "regulators");
	if (!np) {
		dev_err(dev, "missing 'regulators' subnode in DT, %d\n",
			-EINVAL);
		return -EINVAL;
	}

	matched = of_regulator_match(dev, np, matches, num_matches);
	of_node_put(np);
	if (matched <= 0) {
		dev_err(dev, "regulators not match, %d\n", matched);
		return matched;
	}

	pdata->regulators =
		devm_kzalloc(dev,
			     sizeof(struct hpu3501_regulator_data) * matched,
			     GFP_KERNEL);
	if (!pdata->regulators) {
		dev_err(dev, "malloc for regulators fail, %d\n", -ENOMEM);
		return -ENOMEM;
	}

	pdata->num_regulators = matched;
	regulator = pdata->regulators;

	for (i = 0; i < num_matches; i++) {
		if (!matches[i].init_data)
			continue;
		regulator->id = i;
		regulator->name = matches[i].name;
		regulator->init_data = matches[i].init_data;
		regulator->of_node = matches[i].of_node;
		regulator++;
	}

	return 0;
}
#else
static int hpu3501_pdata_from_dt(struct device *dev,
				 struct hpu3501_platform_data *pdata)
{
	return -ENODEV;
}
#endif

/* Buck1-Buck4: step 5mv, range: 0v and [0.6v, 1.275v] */
static const struct linear_range hpu3501_voltage_ranges1[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 0, 0),
	REGULATOR_LINEAR_RANGE(600000, 1, 120, 0),
	REGULATOR_LINEAR_RANGE(605000, 121, 255, 5000),
};

/* Buck5: step 15mv, range: 0v and [0.6v, 3.825v] */
static const struct linear_range hpu3501_voltage_ranges2[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 39, 0),
	REGULATOR_LINEAR_RANGE(600000, 40, 255, 15000),
};

/* Ldo1: step 10mv, range: 0v and [0.6v, 1.3v] */
static const struct linear_range hpu3501_voltage_ranges3[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 0, 0),
	REGULATOR_LINEAR_RANGE(600000, 1, 60, 0),
	REGULATOR_LINEAR_RANGE(610000, 61, 129, 10000),
	REGULATOR_LINEAR_RANGE(1300000, 130, 255, 0),
};

/* Ldo2-Ldo3: step 20mv, range: 0v and [1.2v, 3.7v] */
static const struct linear_range hpu3501_voltage_ranges4[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 0, 0),
	REGULATOR_LINEAR_RANGE(1200000, 1, 60, 0),
	REGULATOR_LINEAR_RANGE(1220000, 61, 184, 20000),
	REGULATOR_LINEAR_RANGE(3700000, 185, 255, 0),
};

/**
 * dr_regulator_enable_regmap - standard enable() for regmap users
 *
 * @rdev: regulator to operate on
 *
 * Regulators that use regmap for their register I/O can set the
 * enable_reg and enable_mask fields in their descriptor and then use
 * this as their enable() operation, saving some code.
 */
int dr_regulator_enable_regmap(struct regulator_dev *rdev)
{
	unsigned int val;

	if (rdev->desc->enable_is_inverted) {
		val = rdev->desc->disable_val;
	} else {
		val = rdev->desc->enable_val;
		if (!val)
			val = rdev->desc->enable_mask;
	}

	return regmap_write_bits(rdev->regmap, rdev->desc->enable_reg,
				  rdev->desc->enable_mask, val);
}

int dr_regulator_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
    /*
    * 1. get enable bit
    * 2. set voltage
    * 3.a if step1 is enabled, end
    * 3.b if step1 is disabled, enable it and disable it to backto disable status
    */
    int is_enabled = 0;
    int ret = 0;

    is_enabled = regulator_is_enabled_regmap(rdev);
    ret = regulator_set_voltage_sel_regmap(rdev, sel);
    if (ret) {
        pr_err("set voltage failed\n");
        return ret;
    }
    if (is_enabled == 0) {
        ret = dr_regulator_enable_regmap(rdev);
        mdelay(1);
        ret = regulator_disable_regmap(rdev);

        mdelay(1);
    }
    return ret;
}



static const struct regulator_ops hpu3501_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = dr_regulator_set_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

#define HPU3501_REG(_name, _id, _linear, _step, _vset_mask)                    \
	[ID_##_id] = {                                                         \
		.name = _name,                                                 \
		.id = ID_##_id,                                                \
		.type = REGULATOR_VOLTAGE,                                     \
		.ops = &hpu3501_ops,                                           \
		.n_voltages = HPU3501_VOLTAGE_NUM##_step,                      \
		.linear_ranges = hpu3501_voltage_ranges##_linear,              \
		.n_linear_ranges =                                             \
			ARRAY_SIZE(hpu3501_voltage_ranges##_linear),           \
		.vsel_reg = HPU3501##_##_id##_VSET,                            \
		.vsel_mask = HPU3501_VSET_MASK##_vset_mask,                    \
		.enable_reg = HPU3501_ON_OFF_CTRL,                             \
		.enable_mask = HPU3501##_##_id##_ENA_BIT,                      \
		.disable_val = HPU3501_POWER_OFF,                              \
		.owner = THIS_MODULE,                                          \
	}

/*
 * name:regulator name used
 * id: regulator id, define in hpu3501-regulator.h without prefix "ID_"
 * linear: which linear id to chose, refer to hpu3501_voltage_rangesxx array above
 * step: how many step this regulator have, refer to datasheet
 * vset_mask: refer to datasheet, when set register how many bits are used to control
 */
static const struct regulator_desc hpu3501_regulators[] = {
	HPU3501_REG("BUCK1", BUCK1, 1, 256, 8),
	HPU3501_REG("BUCK2", BUCK2, 1, 256, 8),
	HPU3501_REG("BUCK3", BUCK3, 1, 256, 8),
	HPU3501_REG("BUCK4", BUCK4, 1, 256, 8),
	HPU3501_REG("BUCK5", BUCK5, 2, 256, 8),

	HPU3501_REG("LDO1", LDO1, 3, 256, 8),
	HPU3501_REG("LDO2_CFG1", LDO2_CFG1, 4, 256, 8),
	HPU3501_REG("LDO2_CFG2", LDO2_CFG2, 4, 256, 8),
	HPU3501_REG("LDO3", LDO3, 4, 256, 8),
};

static struct hpu3501_regulator_data *
hpu3501_get_regulator_data(int id, struct hpu3501_platform_data *pdata)
{
	int i;

	if (!pdata)
		return NULL;

	for (i = 0; i < pdata->num_regulators; i++) {
		if (pdata->regulators[i].id == id)
			return &pdata->regulators[i];
	}

	return NULL;
}

static void hpu3501_board_init(struct device *parent,
			       struct hpu3501_regulator *hpu3501_regulator)
{
	if (hpu3501_regulator->en_pin_map != 0)
		hpu3501_write(parent, HPU3501_EN_PIN_RMPR,
			      hpu3501_regulator->en_pin_map);

	if (hpu3501_regulator->fault_cfgr != 0)
		hpu3501_write(parent, HPU3501_FAULT_CFGR,
			      hpu3501_regulator->fault_cfgr);

	if (hpu3501_regulator->ocp_cfg1r != 0)
		hpu3501_write(parent, HPU3501_OCP_CFG1R,
			      hpu3501_regulator->ocp_cfg1r);

	if (hpu3501_regulator->ocp_cfg2r != 0)
		hpu3501_write(parent, HPU3501_OCP_CFG2R,
			      hpu3501_regulator->ocp_cfg2r);
}

static int hpu3501_regulator_probe(struct platform_device *pdev)
{
	struct hpu3501_dev *hpu3501_dev = dev_get_drvdata(pdev->dev.parent);
	struct device *pa_dev = hpu3501_dev->dev;
	const struct regulator_desc *regulators;
	struct hpu3501_platform_data pdata_of, *pdata = NULL;
	struct device *dev = &pdev->dev;
	struct hpu3501_regulator *hpu3501_regulator = NULL;
	int i, ret, num_regulators;
	struct device_node *np;

	pdata = dev_get_platdata(dev);

	regulators = hpu3501_regulators;
	num_regulators = ARRAY_SIZE(hpu3501_regulators);

	hpu3501_regulator =
		devm_kzalloc(dev, sizeof(struct hpu3501_regulator), GFP_KERNEL);
	if (!hpu3501_regulator) {
		dev_err(dev, "No memory for hpu3501_regulator : %d\n", -ENOMEM);
		return -ENOMEM;
	}

	/* dts parse */
	if (pa_dev->of_node && !pdata) {
		ret = hpu3501_pdata_from_dt(pa_dev, &pdata_of);
		if (ret < 0) {
			dev_err(pa_dev, "Get pdata_of from dt error, %d\n",
				ret);
			return ret;
		}

		pdata = &pdata_of;

		np = of_get_child_by_name(pa_dev->of_node, "hpu3501-regulator");
		if (!np) {
			dev_err(dev, "no hpu3501-regulator subnode in DT, %d\n",
				-EINVAL);
			return -EINVAL;
		}

		hpu3501_regulator->master = of_property_read_bool(np, "master");

		if (of_property_read_u32(np, "en_pin_map",
					 &hpu3501_regulator->en_pin_map)) {
			dev_info(dev, "en_pin_map will be default value\n");
		}

		if (of_property_read_u32(np, "fault_cfgr",
					 &hpu3501_regulator->fault_cfgr)) {
			dev_info(dev, "fault_cfgr will be default value\n");
		}

		if (of_property_read_u32(np, "ocp_cfg1r",
					 &hpu3501_regulator->ocp_cfg1r)) {
			dev_info(dev, "ocp_cfg1r will be default value\n");
		}

		if (of_property_read_u32(np, "ocp_cfg2r",
					 &hpu3501_regulator->ocp_cfg2r)) {
			dev_info(dev, "ocp_cfg2r will be default value\n");
		}
	} else {
		dev_err(pa_dev, "dts node get failed\n");
		return -ENODEV;
	}

	/* Finally register devices */
	for (i = 0; i < num_regulators; i++) {
		const struct regulator_desc *desc = &regulators[i];
		struct regulator_config config = {};
		struct hpu3501_regulator_data *rdata;
		struct regulator_dev *rdev;

		config.dev = dev;
		config.driver_data = hpu3501_regulator;

		rdata = hpu3501_get_regulator_data(desc->id, pdata);
		if (!rdata)
			continue;

		config.init_data = rdata->init_data;
		config.of_node = rdata->of_node;

		rdev = devm_regulator_register(dev, desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(dev, "failed to register %s\n", desc->name);
			return PTR_ERR_OR_ZERO(rdev);
		}
	}

	hpu3501_board_init(pa_dev, hpu3501_regulator);

	return 0;
}

static struct platform_driver hpu3501_regulator_driver = {
		.driver = {
				.name = "hpu3501-regulator",
			},
		.probe = hpu3501_regulator_probe,
};

static int __init hpu3501_regulator_init(void)
{
	return platform_driver_register(&hpu3501_regulator_driver);
}
subsys_initcall(hpu3501_regulator_init);

static void __exit hpu3501_regulator_cleanup(void)
{
	platform_driver_unregister(&hpu3501_regulator_driver);
}
module_exit(hpu3501_regulator_cleanup);

MODULE_DESCRIPTION("hobot-pmic hpu3501 voltage regulator driver");
MODULE_AUTHOR("chaohang.cheng <chaohang.cheng@horizon.ai>");
MODULE_LICENSE("GPL v2");
