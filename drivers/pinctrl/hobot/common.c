// SPDX-License-Identifier: GPL-2.0
/*
 * Sunrise5 pinctrl core driver
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/slab.h>
#include <linux/regmap.h>

#include "core.h"
#include "../gpio/gpiolib.h"
#include "pinconf.h"
#include "pinmux.h"
#include "common.h"

#define HORIZON_PIN_SIZE 20

#define HORIZON_PULL_UP	  1
#define HORIZON_PULL_DOWN 0

/* GPIO control registers */
#define GPIO_SWPORT_DR	0x00
#define GPIO_SWPORT_DDR 0x04

/*
 * gpiolib gpio_direction_input callback function. The setting of the  pin
 * mux function as 'gpio input' will be handled by the pinctrl subsystem
 * interface.
 */
static inline int horizon_gpio_direction_input(struct gpio_chip *gc, unsigned int offset)
{
	return pinctrl_gpio_direction_input(gc->base + offset);
}

/*
 * gpiolib gpio_direction_output callback function. The setting of the pin
 * mux function as 'gpio output' will be handled by the pinctrl subsystem
 * interface.
 */
static inline int horizon_gpio_direction_output(struct gpio_chip *gc, unsigned int offset,
						int value)
{
	return pinctrl_gpio_direction_output(gc->base + offset);
}

static const struct gpio_chip horizon_gpio_chip = {
	.request	  = gpiochip_generic_request,
	.free		  = gpiochip_generic_free,
	.direction_input  = horizon_gpio_direction_input,
	.direction_output = horizon_gpio_direction_output,
	.owner		  = THIS_MODULE,
};

static const struct group_desc *horizon_pinctrl_find_group_by_name(struct pinctrl_dev *pctldev,
								   const char *name)
{
	const struct group_desc *grp = NULL;
	int i;

	for (i = 0; i < pctldev->num_groups; i++) {
		grp = pinctrl_generic_get_group(pctldev, i);
		if (grp && !strcmp(grp->name, name))
			break;
	}

	return grp;
}

static void horizon_pin_power_source(struct horizon_pinctrl *ipctl, unsigned int pin,
				  unsigned int flags, const struct horizon_pinctrl *info)
{
	void __iomem *mode_select_regs;
	unsigned int ms_reg_domain, ms_bits_offset = 0;
	int i;
	u32 val;

	for (i = 0; i < info->npins; i++) {
		if (info->pins[i].pin_id == pin) {
			ms_reg_domain  = info->pins[i].ms_reg_domain;
			ms_bits_offset = info->pins[i].ms_bits_offset;
			break;
		}
	}

	if (i >= info->npins)
		return;

	if (ms_bits_offset == INVALID_MS_BIT)
		return;

	if (ipctl->mscon)
		mode_select_regs = ipctl->mscon;
	else
		mode_select_regs = ipctl->base + ms_reg_domain;

	mutex_lock(&ipctl->mutex);
	val = readl(mode_select_regs);

	if (flags == HORIZON_IO_PAD_VOLTAGE_IP_CTRL) {
		val |= MS_BIT_CTRL;
	} else {
		if (flags / 2)
			val &= ~MS_BIT_CTRL;
		if (flags % 2)
			val |= ms_bits_offset;
		else
			val &= ~ms_bits_offset;
	}

	writel(val, mode_select_regs);
	mutex_unlock(&ipctl->mutex);
	dev_dbg(ipctl->dev, "write pin-%d ms_reg_domain val:0x%x\n", pin, val);
}

static void horizon_pin_pull_disable(struct horizon_pinctrl *ipctl, unsigned int pin,
				     const struct horizon_pinctrl *info)
{
	void __iomem *pull_en_regs;
	void __iomem *pull_up_regs;
	void __iomem *pull_down_regs;
	unsigned int val, reg_domain, pe_bits_offset, pu_bits_offset, pd_bits_offset = 0;
	int i;
	if (!info || !info->pins || !info->npins) {
		dev_err(ipctl->dev, "wrong pinctrl info\n");
	}
	for (i = 0; i < info->npins; i++) {
		if (info->pins[i].pin_id == pin) {
			reg_domain     = info->pins[i].reg_domain;
			pe_bits_offset = info->pins[i].pe_bits_offset;
			pu_bits_offset = info->pins[i].pu_bits_offset;
			pd_bits_offset = info->pins[i].pd_bits_offset;
			break;
		}
	}

	if (i >= info->npins)
		return;
	if (pe_bits_offset != INVALID_PULL_BIT) {
		pull_en_regs = ipctl->base + reg_domain;
		mutex_lock(&ipctl->mutex);
		val	     = readl(pull_en_regs);
		val &= ~pe_bits_offset;
		writel(val, pull_en_regs);
		mutex_unlock(&ipctl->mutex);
		dev_dbg(ipctl->dev, "write pin-%d pull en reg_domain: 0x%x val:0x%x\n", pin, reg_domain, val);
	} else {
		mutex_lock(&ipctl->mutex);
		pull_up_regs = ipctl->base + reg_domain;
		val	     = readl(pull_up_regs);
		val &= ~pu_bits_offset;
		writel(val, pull_up_regs);

		pull_down_regs = ipctl->base + reg_domain;
		val	       = readl(pull_down_regs);
		val &= ~pd_bits_offset;
		writel(val, pull_down_regs);
		mutex_unlock(&ipctl->mutex);

		dev_dbg(ipctl->dev, "write pin-%d pull up en reg_domain: 0x%x val:0x%x\n", pin, reg_domain,
			val);
		dev_dbg(ipctl->dev, "write pin-%d pull down en reg_domain: 0x%x val:0x%x\n", pin, reg_domain,
			val);
	}
}

static void horizon_pin_pull_en(struct horizon_pinctrl *ipctl, unsigned int pin, unsigned int flags,
				const struct horizon_pinctrl *info)
{
	void __iomem *pull_en_regs;
	void __iomem *pull_select_regs;
	void __iomem *pull_up_regs;
	void __iomem *pull_down_regs;
	unsigned int val, reg_domain, pe_bits_offset, ps_bits_offset, pu_bits_offset,
		pd_bits_offset = 0;
	int i;

	for (i = 0; i < info->npins; i++) {
		if (info->pins[i].pin_id == pin) {
			reg_domain     = info->pins[i].reg_domain;
			pe_bits_offset = info->pins[i].pe_bits_offset;
			ps_bits_offset = info->pins[i].ps_bits_offset;
			pu_bits_offset = info->pins[i].pu_bits_offset;
			pd_bits_offset = info->pins[i].pd_bits_offset;
			break;
		}
	}

	if (i >= info->npins)
		return;

	/* We have two regs(marked as domain) for horizon pins pull en setting */
	if (pe_bits_offset != INVALID_PULL_BIT) {
		pull_en_regs	 = ipctl->base + reg_domain;
		pull_select_regs = ipctl->base + reg_domain;
		mutex_lock(&ipctl->mutex);
		val		 = readl(pull_en_regs);
		val |= pe_bits_offset;
		writel(val, pull_en_regs);

		val = readl(pull_select_regs);

		if (flags)
			val |= ps_bits_offset;
		else
			val &= ~ps_bits_offset;

		writel(val, pull_select_regs);
		mutex_unlock(&ipctl->mutex);
		dev_dbg(ipctl->dev, "write pin-%d pull en reg_domain: 0x%x val:0x%x\n", pin, reg_domain, val);
		dev_dbg(ipctl->dev, "write pin-%d pull select reg_domain: 0x%x val:0x%x\n", pin, reg_domain,
			val);
	} else {
		pull_up_regs = ipctl->base + reg_domain;
		mutex_lock(&ipctl->mutex);
		val	     = readl(pull_up_regs);
		if (flags)
			val |= pu_bits_offset;
		else
			val &= ~pu_bits_offset;

		writel(val, pull_up_regs);

		pull_down_regs = ipctl->base + reg_domain;
		val	       = readl(pull_down_regs);
		if (flags)
			val &= ~pd_bits_offset;
		else
			val |= pd_bits_offset;

		writel(val, pull_down_regs);
		mutex_unlock(&ipctl->mutex);
		dev_dbg(ipctl->dev, "write pin-%d pull up reg_domain: 0x%x val:0x%x\n", pin, reg_domain, val);
		dev_dbg(ipctl->dev, "write pin-%d pull down reg_domain: 0x%x val:0x%x\n", pin, reg_domain, val);
	}
}

static void horizon_pin_schmit_en(struct horizon_pinctrl *ipctl, unsigned int pin,
				  unsigned int flags, const struct horizon_pinctrl *info)
{
	void __iomem *schmit_tg_regs;
	unsigned int val, reg_domain, st_bits_offset = 0;
	int i;

	/* We have two regs(marked as domain) for
	 * horizon pins schmitter trigger setting
	 */
	for (i = 0; i < info->npins; i++) {
		if (info->pins[i].pin_id == pin) {
			reg_domain     = info->pins[i].reg_domain;
			st_bits_offset = info->pins[i].st_bits_offset;
			break;
		}
	}

	if (i >= info->npins)
		return;

	schmit_tg_regs = ipctl->base + reg_domain;
	mutex_lock(&ipctl->mutex);
	val	       = readl(schmit_tg_regs);

	if (flags)
		val |= st_bits_offset;
	else
		val &= ~st_bits_offset;

	writel(val, schmit_tg_regs);
	mutex_unlock(&ipctl->mutex);
	dev_dbg(ipctl->dev, "write pin-%d schmit en reg_domain: 0x%x val:0x%x\n", pin, reg_domain, val);
}

static void horizon_pin_drv_str_set(struct horizon_pinctrl *ipctl, unsigned int pin,
				    unsigned int arg, const struct horizon_pinctrl *info)
{
	void __iomem *drv_str_regs;
	unsigned int val, reg_domain, ds_bits_offset = 0;
	int i;

	/* We have two regs(marked as domain) for
	 * horizon pins schmitter trigger setting
	 */
	for (i = 0; i < info->npins; i++) {
		if (info->pins[i].pin_id == pin) {
			reg_domain     = info->pins[i].reg_domain;
			ds_bits_offset = info->pins[i].ds_bits_offset;
			break;
		}
	}

	if (i >= info->npins)
		return;

	drv_str_regs = ipctl->base + reg_domain;

	mutex_lock(&ipctl->mutex);
	val = readl(drv_str_regs);
	val &= ~(PIN_DS_BIT_MASK << ds_bits_offset);
	val |= arg << ds_bits_offset;
	writel(val, drv_str_regs);
	mutex_unlock(&ipctl->mutex);
	dev_dbg(ipctl->dev, "write pin-%d drv str reg_domain: 0x%x arg:%#x val:0x%x\n", pin, reg_domain, arg, val);
}

static int horizon_dt_node_to_map(struct pinctrl_dev *pctldev, struct device_node *np,
				  struct pinctrl_map **map, unsigned int *num_maps)
{
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	const struct group_desc *grp;
	struct pinctrl_map *new_map;
	struct device_node *parent;
	struct horizon_pin *pin;
	int map_num = 1;
	int i, j;

	/*
	 * first find the group of this node and check if we need create
	 * config maps for pins
	 */
	grp = horizon_pinctrl_find_group_by_name(pctldev, np->name);
	if (!grp) {
		dev_err(ipctl->dev, "unable to find group for node %pOFn\n", np);
		return -EINVAL;
	}

	for (i = 0; i < grp->num_pins; i++) {
		pin = &((struct horizon_pin *)(grp->data))[i];
		map_num++;
	}

	new_map = kmalloc_array(map_num, sizeof(struct pinctrl_map), GFP_KERNEL);
	if (!new_map)
		return -ENOMEM;

	*map	  = new_map;
	*num_maps = map_num;

	/* create mux map */
	parent = of_get_parent(np);
	if (!parent) {
		kfree(new_map);
		return -EINVAL;
	}
	new_map[0].type		     = PIN_MAP_TYPE_MUX_GROUP;
	new_map[0].data.mux.function = parent->name;
	new_map[0].data.mux.group    = np->name;
	of_node_put(parent);

	/* create config map */
	new_map++;
	for (i = j = 0; i < grp->num_pins; i++) {
		pin = &((struct horizon_pin *)(grp->data))[i];

		/* create a new map */
		new_map[j].type			     = PIN_MAP_TYPE_CONFIGS_PIN;
		new_map[j].data.configs.group_or_pin = pin_get_name(pctldev, pin->pin_id);
		new_map[j].data.configs.configs	     = pin->config;
		new_map[j].data.configs.num_configs  = pin->num_configs;
		j++;
		dev_dbg(pctldev->dev, "Group %s num %d and pin_id = %d\n", (*map)->data.mux.group,
			i, pin->pin_id);
	}
	dev_dbg(pctldev->dev, "Maps: function %s group %s num %d\n", (*map)->data.mux.function,
		(*map)->data.mux.group, map_num);

	return 0;
}

static void horizon_dt_free_map(struct pinctrl_dev *pctldev, struct pinctrl_map *map,
				unsigned int num_maps)
{
	kfree(map);
}

static const struct pinctrl_ops horizon_pctrl_ops = {
	.get_groups_count = pinctrl_generic_get_group_count,
	.get_group_name	  = pinctrl_generic_get_group_name,
	.get_group_pins	  = pinctrl_generic_get_group_pins,
	.dt_node_to_map	  = horizon_dt_node_to_map,
	.dt_free_map	  = horizon_dt_free_map,
};

static int horizon_pmx_set_one_pin(struct horizon_pinctrl *ipctl, struct horizon_pin *horizon_pin)
{
	unsigned int pin_id = horizon_pin->pin_id;
	unsigned int val    = 0;

	if (horizon_pin->mux_reg_offset == INVALID_PINMUX) {
		dev_dbg(ipctl->dev, "Pin(%d) does not support mux function\n",
			pin_id);
		return 0;
	}

	/* we will parse attribute value on pinconf set, skip here */
	/* set mux per pin */
	mutex_lock(&ipctl->mutex);
	val = readl(ipctl->base + horizon_pin->mux_reg_offset);
	val &= ~(MUX_ALT3<< horizon_pin->mux_reg_bit);
	val |= horizon_pin->mux_mod << horizon_pin->mux_reg_bit;
	writel(val, ipctl->base + horizon_pin->mux_reg_offset);
	mutex_unlock(&ipctl->mutex);
	dev_dbg(ipctl->dev, "Write: mux_reg_bit %x mux_mod:%x val 0x%x\n", horizon_pin->mux_reg_bit,
		horizon_pin->mux_mod, val);
	dev_dbg(ipctl->dev, "Pinctrl set pin %d\n", pin_id);

	return 0;
}

static int horizon_pmx_set(struct pinctrl_dev *pctldev, unsigned int selector, unsigned int group)
{
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	struct function_desc *func;
	struct group_desc *grp;
	struct horizon_pin *pin;
	unsigned int npins;
	int i, err;

	/*
	 * Configure the mux mode for each pin in the group for a specific
	 * function.
	 */
	dev_dbg(ipctl->dev, "Pin mux set, get group(%d) first, selector %d\n", group, selector);
	grp = pinctrl_generic_get_group(pctldev, group);
	if (!grp)
		return -EINVAL;

	func = pinmux_generic_get_function(pctldev, selector);
	if (!func)
		return -EINVAL;

	npins = grp->num_pins;

	dev_dbg(ipctl->dev, "Enable function %s group %s\n", func->name, grp->name);

	for (i = 0; i < npins; i++) {
		pin = &((struct horizon_pin *)(grp->data))[i];
		err = horizon_pmx_set_one_pin(ipctl, pin);
		if (err)
			return err;
	}

	return 0;
}

static int horizon_gpio_get_direction(struct pinctrl_dev *pctldev, int pin)
{
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	struct pinctrl_gpio_range *gpio_range;
	unsigned int val, gpio, gpio_port;
	void __iomem *reg_base;

	dev_dbg(ipctl->dev, "get pin = %d direction\n", pin);
	gpio_range = pinctrl_find_gpio_range_from_pin_nolock(pctldev, pin);
	if (!gpio_range) {
		dev_err(ipctl->dev, "pin = %d can not find corresponding gpio id\n", pin);
		return -ENOTSUPP;
	}
	gpio = pin - gpio_range->pin_base;
	if (ipctl->gpio_bank_num > 1)
		gpio_port = (gpio_range->npins >= 31) ? 0 : 1;
	else
		gpio_port = 0;
	dev_dbg(ipctl->dev, "map pin%d to gpio[%d] - %d\n", pin, gpio_port, gpio);

	reg_base = ipctl->gpio_bank_base[gpio_port];
	mutex_lock(&ipctl->mutex);
	val	 = readl(reg_base + GPIO_SWPORT_DDR);
	mutex_unlock(&ipctl->mutex);
	return !!(val & BIT(gpio));
}

static int horizon_gpio_set_direction(struct pinctrl_dev *pctldev, int pin, bool input,
				      bool output_val)
{
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	unsigned int val, gpio, gpio_port;
	void __iomem *reg_base;
	struct pinctrl_gpio_range *gpio_range;

	dev_info(ipctl->dev, "set pin = %d direction to %s\n", pin, input ? "input" : "output");
	gpio_range = pinctrl_find_gpio_range_from_pin_nolock(pctldev, pin);
	if (!gpio_range) {
		dev_err(ipctl->dev, "pin = %d can not find corresponding gpio id\n", pin);
		return -ENOTSUPP;
	}
	gpio = pin - gpio_range->pin_base;
	if (ipctl->gpio_bank_num > 1)
		gpio_port = (gpio_range->npins >= 31) ? 0 : 1;
	else
		gpio_port = 0;
	dev_info(ipctl->dev, "map pin%d to gpio[%d] - %d\n", pin, gpio_port, gpio);

	mutex_lock(&ipctl->mutex);
	reg_base = ipctl->gpio_bank_base[gpio_port];

	val = readl(reg_base + GPIO_SWPORT_DDR);
	if (input)
		val &= ~BIT(gpio);
	else
		val |= BIT(gpio);
	writel(val, reg_base + GPIO_SWPORT_DDR);

	/* for output direction, we should handle output value: low/high */
	if (!input) {
		val = readl(reg_base + GPIO_SWPORT_DR);
		if (output_val)
			val |= BIT(gpio);
		else
			val &= ~BIT(gpio);
		writel(val, reg_base + GPIO_SWPORT_DR);
	}
	mutex_unlock(&ipctl->mutex);

	return 0;
}

const struct pinmux_ops horizon_pmx_ops = {
	.get_functions_count = pinmux_generic_get_function_count,
	.get_function_name   = pinmux_generic_get_function_name,
	.get_function_groups = pinmux_generic_get_function_groups,
	.set_mux	     = horizon_pmx_set,
};

int horizon_pinconf_get(struct pinctrl_dev *pctldev, unsigned int pin, unsigned long *config,
			const struct horizon_pinctrl *info)
{
	/* Get the pin config settings for a specific pin */
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	enum pin_config_param param   = pinconf_to_config_param(*config);
	unsigned int arg, val, reg_domain, ds_bits_offset, pe_bits_offset, ps_bits_offset,
		pu_bits_offset, pd_bits_offset, st_bits_offset;
	void __iomem *pull_en_regs;
	void __iomem *pull_select_regs;
	void __iomem *pull_up_regs;
	void __iomem *pull_down_regs;
	void __iomem *schmit_tg_regs;
	void __iomem *drv_str_regs;
	int i;
	if (!info || !info->pins || !info->npins) {
		dev_err(ipctl->dev, "wrong pinctrl info\n");
		return -EINVAL;
	}
	/* We have two regs(marked as domain) for
	 * horizon pins schmitter trigger setting
	 */
	for (i = 0; i < info->npins; i++) {
		if (info->pins[i].pin_id == pin) {
			reg_domain     = info->pins[i].reg_domain;
			ds_bits_offset = info->pins[i].ds_bits_offset;
			pe_bits_offset = info->pins[i].pe_bits_offset;
			ps_bits_offset = info->pins[i].ps_bits_offset;
			pu_bits_offset = info->pins[i].pu_bits_offset;
			pd_bits_offset = info->pins[i].pd_bits_offset;
			st_bits_offset = info->pins[i].st_bits_offset;
			break;
		}
	}

	if (i >= info->npins)
		return -ENOTSUPP;

	pull_en_regs	 = ipctl->base + reg_domain;
	pull_select_regs = ipctl->base + reg_domain;
	pull_up_regs	 = ipctl->base + reg_domain;
	pull_down_regs	 = ipctl->base + reg_domain;
	schmit_tg_regs	 = ipctl->base + reg_domain;
	drv_str_regs	 = ipctl->base + reg_domain;

	mutex_lock(&ipctl->mutex);
	/* Convert register value to pinconf value */
	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		/* horizon do not support those configs */
		mutex_unlock(&ipctl->mutex);
		return -ENOTSUPP;
	case PIN_CONFIG_BIAS_PULL_UP:
		if (pe_bits_offset != INVALID_PULL_BIT) {
			val = readl(pull_en_regs);
			val &= pe_bits_offset;
			if (val) {
				val = readl(pull_select_regs);
				val &= ps_bits_offset;
				arg = val;
			} else {
				arg = 0;
			}
		} else {
			val = readl(pull_up_regs);
			val &= pu_bits_offset;
			arg = val;
		}
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		if (pe_bits_offset != INVALID_PULL_BIT) {
			val = readl(pull_en_regs);
			val &= pe_bits_offset;
			if (val) {
				val = readl(pull_select_regs);
				val &= ps_bits_offset;
				arg = val;
			} else {
				arg = 0;
			}
		} else {
			val = readl(pull_down_regs);
			val &= pd_bits_offset;
			arg = val;
		}
		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		val = readl(schmit_tg_regs);
		val &= st_bits_offset;
		arg = val;
		break;
	case PIN_CONFIG_OUTPUT:
	case PIN_CONFIG_INPUT_ENABLE:
		mutex_unlock(&ipctl->mutex);
		arg = horizon_gpio_get_direction(pctldev, pin);
		mutex_lock(&ipctl->mutex);
		if (arg == -ENOTSUPP) {
			mutex_unlock(&ipctl->mutex);
			return arg;
		}
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		/* Lower 4 bits represent pin drive strength */
		val = readl(drv_str_regs);
		val = (val >> ds_bits_offset) & 0xf;
		arg = val & 0xf;
		break;
	default:
		mutex_unlock(&ipctl->mutex);
		return -ENOTSUPP;
	}
	mutex_unlock(&ipctl->mutex);
	*config = pinconf_to_config_packed(param, arg);

	dev_dbg(ipctl->dev, "horizon gpio pinconf get\n");
	return 0;
}

int horizon_pinconf_set(struct pinctrl_dev *pctldev, unsigned int pin, unsigned long *configs,
			unsigned int num_configs, const struct horizon_pinctrl *info)
{
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	unsigned int arg;
	int i, ret = 0;
	enum pin_config_param param;

	if (num_configs == 0) {
		dev_err(ipctl->dev, "No pinconfigs to set\n");
		return -EINVAL;
	}

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg   = pinconf_to_config_argument(configs[i]);
		dev_dbg(ipctl->dev, "horizon pinconf pin-%d, param=%d, arg=%d\n", pin, param, arg);

		switch (param) {
		case PIN_CONFIG_POWER_SOURCE:
			horizon_pin_power_source(ipctl, pin, arg, info);
			break;
		case PIN_CONFIG_BIAS_DISABLE:
			horizon_pin_pull_disable(ipctl, pin, info);
			break;
		case PIN_CONFIG_DRIVE_OPEN_DRAIN:
			/* horizon do not support those configs */
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
			horizon_pin_pull_en(ipctl, pin, HORIZON_PULL_UP, info);
			break;
		case PIN_CONFIG_BIAS_PULL_DOWN:
			horizon_pin_pull_en(ipctl, pin, HORIZON_PULL_DOWN, info);
			break;
		case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
			horizon_pin_schmit_en(ipctl, pin, arg, info);
			break;
		case PIN_CONFIG_DRIVE_STRENGTH:
			horizon_pin_drv_str_set(ipctl, pin, arg, info);
			break;
		case PIN_CONFIG_OUTPUT:
			ret = horizon_gpio_set_direction(pctldev, pin, false, arg);
			break;
		case PIN_CONFIG_INPUT_ENABLE:
			ret = horizon_gpio_set_direction(pctldev, pin, true, arg);
			break;
		default:
			dev_err(ipctl->dev, "horizon pinconf pin %d param:%d not support\n", pin,
				param);
			return -ENOTSUPP;
		}
	} /* For each config */

	return ret;
}

static void horizon_pinctrl_parse_pin(struct horizon_pinctrl *ipctl, unsigned int *pin,
				      struct horizon_pin *horizon_pin, const __be32 **list_p,
				      struct device_node *np)
{
	const __be32 *list = *list_p;

	horizon_pin->pin_id	    = be32_to_cpu(*list++);
	horizon_pin->mux_reg_offset = be32_to_cpu(*list++);
	horizon_pin->mux_reg_bit    = be32_to_cpu(*list++);
	horizon_pin->mux_mod	    = be32_to_cpu(*list++);

	*pin = horizon_pin->pin_id;

	*list_p = list;
	dev_dbg(ipctl->dev, "Pin-%d with mux_mode %x\n", horizon_pin->pin_id, horizon_pin->mux_mod);
}

static int horizon_pinctrl_parse_groups(struct device_node *np, struct group_desc *grp,
					struct horizon_pinctrl *ipctl, u32 index)
{
	struct horizon_pin *horizon_pin;
	int size, ret, i;
	const __be32 *list;

	dev_dbg(ipctl->dev, "Group(%d): %pOFn\n", index, np);

	/* Initialise group */
	grp->name = np->name;

	/*
	 * The binding format is horizon,pins = <pin_id, mux_reg_bit, mux mode, pinconf_attributes>,
	 * do sanity check and calculate pins number
	 *
	 * Note: for generic 'pinmux' case, there's no CONFIG part in
	 * the binding format.
	 */
	list = of_get_property(np, "horizon,pins", &size);
	if (!list) {
		return 0;
	}

	if (!size || size % sizeof(*list)) {
		dev_err(ipctl->dev, "Invalid horizon,pins or property in node %pOF size:%d\n", np,
			size);
		return -EINVAL;
	}

	grp->num_pins = size / HORIZON_PIN_SIZE;
	grp->data = devm_kcalloc(ipctl->dev, grp->num_pins, sizeof(struct horizon_pin), GFP_KERNEL);
	grp->pins = devm_kcalloc(ipctl->dev, grp->num_pins, sizeof(unsigned int), GFP_KERNEL);
	if (!grp->pins || !grp->data)
		return -ENOMEM;

	for (i = 0; i < grp->num_pins; i++) {
		const __be32 *phandle;
		struct device_node *np_config;

		horizon_pin = &((struct horizon_pin *)(grp->data))[i];
		horizon_pinctrl_parse_pin(ipctl, &grp->pins[i], horizon_pin, &list, np);

		phandle = list++;
		if (!phandle)
			return -EINVAL;
		np_config = of_find_node_by_phandle(be32_to_cpup(phandle));
		ret = pinconf_generic_parse_dt_config(np_config, ipctl->pctl, &horizon_pin->config,
						      &horizon_pin->num_configs);
		if (ret)
			return ret;
		dev_dbg(ipctl->dev, "Group(%d): i:%d get pins:%d\n", index, i, grp->pins[i]);
	}

	dev_dbg(ipctl->dev, "Group(%d): %pOFn %d pins in\n", index, np, grp->num_pins);

	return 0;
}

static int horizon_pinctrl_parse_functions(struct device_node *np, struct horizon_pinctrl *ipctl,
					   u32 index)
{
	struct pinctrl_dev *pctl = ipctl->pctl;
	struct device_node *child;
	struct function_desc *func;
	struct group_desc *grp;
	const char **group_names;
	u32 i = 0;

	dev_dbg(pctl->dev, "Parse function(%d): %pOFn\n", index, np);

	func = pinmux_generic_get_function(pctl, index);
	if (!func)
		return -EINVAL;

	/* Initialise function */
	func->name	      = np->name;
	func->num_group_names = of_get_child_count(np);
	if (func->num_group_names == 0) {
		dev_err(ipctl->dev, "No groups defined in %pOF\n", np);
		return -EINVAL;
	}

	group_names = devm_kcalloc(ipctl->dev, func->num_group_names, sizeof(char *), GFP_KERNEL);
	if (!group_names)
		return -ENOMEM;

	for_each_child_of_node (np, child) {
		group_names[i] = child->name;

		grp = devm_kzalloc(ipctl->dev, sizeof(struct group_desc), GFP_KERNEL);
		if (!grp) {
			of_node_put(child);
			return -ENOMEM;
		}

		mutex_lock(&ipctl->mutex);
		radix_tree_insert(&pctl->pin_group_tree, ipctl->group_index++, grp);
		mutex_unlock(&ipctl->mutex);

		horizon_pinctrl_parse_groups(child, grp, ipctl, i++);
	}
	func->group_names = group_names;

	return 0;
}

/*
 * Check if the DT contains pins in the direct child nodes. This indicates the
 * newer DT format to store pins. This function returns true if the first found
 * horizon,pins property is in a child of np. Otherwise false is returned.
 */
static bool horizon_pinctrl_dt_is_flat_functions(struct device_node *np)
{
	struct device_node *function_np;
	struct device_node *pinctrl_np;

	for_each_child_of_node (np, function_np) {
		if (of_property_read_bool(function_np, "horizon,pins")) {
			of_node_put(function_np);
			return true;
		}

		for_each_child_of_node (function_np, pinctrl_np) {
			if (of_property_read_bool(pinctrl_np, "horizon,pins")) {
				of_node_put(pinctrl_np);
				of_node_put(function_np);
				return false;
			}
		}
	}

	return true;
}

static int horizon_pinctrl_parse_gpio_bank(struct platform_device *pdev,
					   struct horizon_pinctrl *ipctl)
{
	struct device_node *sub_np, *np = pdev->dev.of_node;
	const __be32 *phandle;
	unsigned int bank_num, size = 0;
	int i;

	phandle = of_get_property(np, "horizon,gpio-banks", &size);
	if (!phandle || !size) {
		dev_err(ipctl->dev, "no horizon,gpio-banks in node %pOF\n", np);
		return -EINVAL;
	}
	bank_num = size / sizeof(*phandle);
	/* allocate gpio banks base array */
	ipctl->gpio_bank_base =
		devm_kzalloc(&pdev->dev, bank_num * sizeof(*ipctl->gpio_bank_base), GFP_KERNEL);
	ipctl->gpio_bank_num = bank_num;
	if (!ipctl->gpio_bank_base)
		return -ENOMEM;

	dev_dbg(&pdev->dev, "gpio bank phandle size is %d, number is %d\n", size, bank_num);
	for (i = 0; i < bank_num; i++, phandle++) {
		sub_np			 = of_find_node_by_phandle(be32_to_cpup(phandle));
		ipctl->gpio_bank_base[i] = of_iomap(sub_np, 0);
	}

	return 0;
}

static int horizon_pinctrl_probe_dt(struct platform_device *pdev, struct horizon_pinctrl *ipctl)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	struct pinctrl_dev *pctl = ipctl->pctl;
	u32 nfuncs = 0, i = 0;
	bool flat_funcs;

	if (!np)
		return -ENODEV;

	flat_funcs = horizon_pinctrl_dt_is_flat_functions(np);

	if (flat_funcs) {
		nfuncs = 1;
	} else {
		nfuncs = of_get_child_count(np);
		if (nfuncs == 0) {
			dev_err(&pdev->dev, "No functions defined\n");
			return -EINVAL;
		}
	}

	for (i = 0; i < nfuncs; i++) {
		struct function_desc *function;

		function = devm_kzalloc(&pdev->dev, sizeof(*function), GFP_KERNEL);
		if (!function)
			return -ENOMEM;

		mutex_lock(&ipctl->mutex);
		radix_tree_insert(&pctl->pin_function_tree, i, function);
		mutex_unlock(&ipctl->mutex);
	}

	pctl->num_functions = nfuncs;

	ipctl->group_index = 0;
	if (flat_funcs) {
		pctl->num_groups = of_get_child_count(np);
	} else {
		pctl->num_groups = 0;
		for_each_child_of_node (np, child)
			pctl->num_groups += of_get_child_count(child);
	}

	if (flat_funcs) {
		horizon_pinctrl_parse_functions(np, ipctl, 0);
	} else {
		i = 0;
		for_each_child_of_node (np, child)
			horizon_pinctrl_parse_functions(child, ipctl, i++);
	}

	return 0;
}

/*
 * horizon_free_resources() - free memory used by this driver
 * @info: info driver instance
 */
static void horizon_free_resources(struct horizon_pinctrl *ipctl)
{
	if (ipctl->pctl)
		pinctrl_unregister(ipctl->pctl);
}

int horizon_pinctrl_probe(struct platform_device *pdev, const struct horizon_pinctrl *info,
			  const struct pinconf_ops *ops)
{
	struct pinctrl_desc *horizon_pinctrl_desc;
	struct pinctrl_pin_desc *pin_desc;
	struct horizon_pinctrl *ipctl;
	struct resource *res;
	int ret, i;

	/* Create device for this driver */
	ipctl = devm_kzalloc(&pdev->dev, sizeof(*ipctl), GFP_KERNEL);
	if (!ipctl)
		return -ENOMEM;

	ipctl->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ipctl->base))
		return PTR_ERR(ipctl->base);
	res		 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res)
		ipctl->mscon = ioremap(res->start, resource_size(res));

	/* parse horizon gpio bank */
	ret = horizon_pinctrl_parse_gpio_bank(pdev, ipctl);
	if (ret) {
		dev_err(ipctl->dev, "horizon parse gpio bank err\n");
		return -EINVAL;
	}

	ipctl->npins = info->npins;

	pin_desc = devm_kzalloc(&pdev->dev, ipctl->npins * sizeof(*pin_desc), GFP_KERNEL);
	if (!pin_desc)
		return -ENOMEM;

	/* Get pinctrl pin desc from horizon pin desc */
	for (i = 0; i < info->npins; i++) {
		pin_desc[i].number = info->pins[i].pin_id;
		pin_desc[i].name   = info->pins[i].name;
	}

	horizon_pinctrl_desc = devm_kzalloc(&pdev->dev, sizeof(*horizon_pinctrl_desc), GFP_KERNEL);
	if (!horizon_pinctrl_desc)
		return -ENOMEM;

	/* Initial a pinctrl description device */
	horizon_pinctrl_desc->name    = dev_name(&pdev->dev);
	horizon_pinctrl_desc->pins    = pin_desc;
	horizon_pinctrl_desc->npins   = info->npins;
	horizon_pinctrl_desc->pctlops = &horizon_pctrl_ops;
	horizon_pinctrl_desc->pmxops  = &horizon_pmx_ops;
	horizon_pinctrl_desc->confops = ops;
	horizon_pinctrl_desc->owner   = THIS_MODULE;

	mutex_init(&ipctl->mutex);

	ipctl->dev = &pdev->dev;
	platform_set_drvdata(pdev, ipctl);
	ret = devm_pinctrl_register_and_init(&pdev->dev, horizon_pinctrl_desc, ipctl, &ipctl->pctl);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't register D-Robotics pinctrl driver\n");
		goto free;
	}

	/* register gpio chip use gpiolib */
	ipctl->gpio_chip	= horizon_gpio_chip;
	ipctl->gpio_chip.ngpio	= ipctl->gpio_bank_num;
	ipctl->gpio_chip.label	= dev_name(&pdev->dev);
	ipctl->gpio_chip.parent = &pdev->dev;
	ipctl->gpio_chip.base	= -1;

	ret = gpiochip_add_data(&ipctl->gpio_chip, ipctl);
	if (ret)
		return -EINVAL;

	ret = horizon_pinctrl_probe_dt(pdev, ipctl);
	if (ret) {
		dev_err(&pdev->dev, "Fail to probe dt properties\n");
		goto free;
	}

	dev_info(&pdev->dev, "Initialized D-Robotics pinctrl driver\n");
	return pinctrl_enable(ipctl->pctl);

free:
	horizon_free_resources(ipctl);
	return ret;
}
