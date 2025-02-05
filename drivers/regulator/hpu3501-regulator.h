/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2022 Horizon Robotics
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */

#ifndef DRIVERS_REGULATOR_HPU3501_H_
#define DRIVERS_REGULATOR_HPU3501_H_

#include <linux/regulator/machine.h>

#define HPU3501_EN_PIN_RMPR 0X6
#define HPU3501_OCP_CFG1R 0x8
#define HPU3501_OCP_CFG2R 0x9

#define HPU3501_ON_OFF_CTRL 0x10
#define HPU3501_BUCK1_ENA_BIT 0x1
#define HPU3501_BUCK2_ENA_BIT 0x2
#define HPU3501_BUCK3_ENA_BIT 0x4
#define HPU3501_BUCK4_ENA_BIT 0x8
#define HPU3501_BUCK5_ENA_BIT 0x10
#define HPU3501_LDO1_ENA_BIT 0x20
#define HPU3501_LDO2_CFG1_ENA_BIT 0x40
#define HPU3501_LDO2_CFG2_ENA_BIT 0x40
#define HPU3501_LDO3_ENA_BIT 0x80
#define HPU3501_POWER_OFF 0

#define HPU3501_FAULT_CFGR 0Xa

#define HPU3501_BUCK1_VSET 0x18
#define HPU3501_BUCK2_VSET 0x1a
#define HPU3501_BUCK3_VSET 0x1c
#define HPU3501_BUCK4_VSET 0x1e
#define HPU3501_BUCK5_VSET 0x20
#define HPU3501_LDO1_VSET 0x22
#define HPU3501_LDO2_CFG1_VSET 0x24
#define HPU3501_LDO2_CFG2_VSET 0x25
#define HPU3501_LDO3_VSET 0x27
#define HPU3501_REG_0X53 0x53
/* VSET - [7:0] */
#define HPU3501_VSET_MASK8 0xff

#define HPU3501_VOLTAGE_NUM256 256

enum {
	ID_BUCK1,
	ID_BUCK2,
	ID_BUCK3,
	ID_BUCK4,
	ID_BUCK5,

	ID_LDO1,
	ID_LDO2_CFG1,
	ID_LDO2_CFG2,
	ID_LDO3,
};

/**
 * master: true master pmic, false slave pmic
 * en_pin_map: rigister 0x06 configuration
 * fault_cfgr: rigister 0x0a configuration
 * ocp_cfg1r: rigister 0x08 configuration
 * ocp_cfg2r: rigister 0x09 configuration
 */
struct hpu3501_regulator {
	bool master;
	u32 en_pin_map;
	u32 fault_cfgr;
	u32 ocp_cfg1r;
	u32 ocp_cfg2r;
	u32 reg0x53;
};

/**
 * hpu3501_regulator_data - regulator data
 * @id: regulator id
 * @name: regulator name
 * @init_data: regulator init data
 * @of_node: device tree node (optional)
 */
struct hpu3501_regulator_data {
	int id;
	const char *name;
	struct regulator_init_data *init_data;
	struct device_node *of_node;
};

/**
 * hpu3501_platform_data - platform data for hpu3501
 * @num_regulators: number of regulators used
 * @regulators: pointer to regulators used
 */
struct hpu3501_platform_data {
	int num_regulators;
	struct hpu3501_regulator_data *regulators;
};

#endif // DRIVERS_REGULATOR_HPU3501_H_
