/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunrise5 pinctrl core driver
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef __DRIVERS_PINCTRL_HORIZON_H
#define __DRIVERS_PINCTRL_HORIZON_H

#include <linux/types.h>
#include <dt-bindings/pinctrl/horizon-pinfunc.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinmux.h>

/* horizon pin description */
struct horizon_pin_desc {
	unsigned int pin_id;
	const char *name;
	unsigned int reg_domain;
	unsigned int ms_reg_domain;
	unsigned int ds_bits_offset;
	unsigned int pe_bits_offset;
	unsigned int ps_bits_offset;
	unsigned int pu_bits_offset;
	unsigned int pd_bits_offset;
	unsigned int st_bits_offset;
	unsigned int ms_bits_offset;
	unsigned int cell_type;
};

#ifndef _PIN
#define _PIN(id, nm, reg, ms_reg, ds, pe, ps, pu, pd, st, ms, ct)      \
	{                                                                         \
		.pin_id = id, .name = nm, .reg_domain = reg, .ms_reg_domain = ms_reg, \
		.ds_bits_offset = ds, .pe_bits_offset = pe, .ps_bits_offset = ps,     \
		.pu_bits_offset = pu, .pd_bits_offset = pd, .st_bits_offset = st,     \
		.ms_bits_offset = ms, .cell_type = ct,                         \
	}
#endif

#define INVALID_PULL_BIT   BIT(9)
#define INVALID_MS_BIT	   BIT(10)
#define INVALID_REG_DOMAIN 0x0
#define MS_BIT_CTRL	   BIT(0)
#define PIN_DS_BIT_MASK GENMASK(3,0)

/**
 * struct horizon_pin - describes a horizon pin
 * @mux_mod: pads mux mode config
 * @mux_reg_offset:  mux mode register offset
 * @mux_reg_bit:  mux mode register bit
 * @pin_id: pin id
 * @config: the config for this pin
 * @num_config: the number of configs
 */
struct horizon_pin {
	unsigned int mux_mod;
	unsigned int pin_id;
	unsigned int mux_reg_offset;
	unsigned int mux_reg_bit;
	unsigned long *config;
	unsigned int num_configs;
};

/**
 * @dev:   pointer back to containing device
 * @pctl:  pointer to pinctrl device
 * @pins:  pointer to pinctrl pin description device
 * @npins: number of pins in description device
 * @base:  address of the controller memory
 * @input_sel_base:  address of the insel controller memory
 * @pin_regs:  abstract of a horizon pin
 * @group_index:  index of a group
 * @mutex: mutex
 */
struct horizon_pinctrl {
	struct device *dev;
	struct pinctrl_dev *pctl;
	struct gpio_chip gpio_chip;
	const struct horizon_pin_desc *pins;
	unsigned int npins;
	void __iomem *base;
	void __iomem *mscon;
	unsigned int gpio_pin_num;
	struct horizon_pin *pin_regs;
	unsigned int group_index;
	struct mutex mutex; /* mutex */
	unsigned int *mux_val;
	const __be32 *phandle;
	struct dwapb_gpio *gpio[];
};

extern const struct pinmux_ops horizon_pmx_ops;
extern const struct dev_pm_ops horizon_pinctrl_pm_ops;

int horizon_pinconf_get(struct pinctrl_dev *pctldev, unsigned int pin, unsigned long *config,
			const struct horizon_pinctrl *info);
int horizon_pinconf_set(struct pinctrl_dev *pctldev, unsigned int pin, unsigned long *configs,
			unsigned int num_configs, const struct horizon_pinctrl *info);
int horizon_pinctrl_probe(struct platform_device *pdev, const struct horizon_pinctrl *info,
			  const struct pinconf_ops *ops);

#endif /* __DRIVERS_PINCTRL_HORIZON_H */
