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

/* Drive Strength Current Mappings */
struct output_current {
	u32 ds;
	u32 current_value; /* output current in mA */
};

#define X5_CELL_TYPE_TOTAL (3u)
#define X5_CELL_LEVEL_TOTAL (2U)
#define X5_DS_TOTAL (16u)

/* This is a mapping of pre-measured typical current and register value.
 * The actual current may vary chip by chip.
 */
static const struct output_current output_current_mapping[X5_CELL_TYPE_TOTAL][X5_CELL_LEVEL_TOTAL][X5_DS_TOTAL] = {
	[AON_1V8] {
		/*output low*/
		{
			{.ds = 0, .current_value = 3},   {.ds = 1, .current_value = 6},
			{.ds = 2, .current_value = 9},   {.ds = 3, .current_value = 12},
			{.ds = 4, .current_value = 16},  {.ds = 5, .current_value = 19},
			{.ds = 6, .current_value = 21},  {.ds = 7, .current_value = 23},
			{.ds = 8, .current_value = 30},  {.ds = 9, .current_value = 32},
			{.ds = 10, .current_value = 34}, {.ds = 11, .current_value = 35},
			{.ds = 12, .current_value = 37}, {.ds = 13, .current_value = 39},
			{.ds = 14, .current_value = 40}, {.ds = 15, .current_value = 41},
		},
		/*output high*/
		{
			{.ds = 0, .current_value = 2}, {.ds = 1, .current_value = 5},
			{.ds = 2, .current_value = 7}, {.ds = 3, .current_value = 10},
			{.ds = 4, .current_value = 14}, {.ds = 5, .current_value = 16},
			{.ds = 6, .current_value = 18}, {.ds = 7, .current_value = 20},
			{.ds = 8, .current_value = 26}, {.ds = 9, .current_value = 27},
			{.ds = 10, .current_value = 29}, {.ds = 11, .current_value = 30},
			{.ds = 12, .current_value = 32}, {.ds = 13, .current_value = 33},
			{.ds = 14, .current_value = 34}, {.ds = 15, .current_value = 35},
		}
	},
	[SOC_1V8] {
		/*output low*/
		{
			{.ds = 0, .current_value = 1},   {.ds = 1, .current_value = 2},
			{.ds = 2, .current_value = 3},   {.ds = 3, .current_value = 5},
			{.ds = 4, .current_value = 6},   {.ds = 5, .current_value = 7},
			{.ds = 6, .current_value = 9},   {.ds = 7, .current_value = 10},
			{.ds = 8, .current_value = 12},  {.ds = 9, .current_value = 13},
			{.ds = 10, .current_value = 15}, {.ds = 11, .current_value = 16},
			{.ds = 12, .current_value = 18}, {.ds = 13, .current_value = 19},
			{.ds = 14, .current_value = 21}, {.ds = 15, .current_value = 22},
		},
		/*output high*/
		{
			{.ds = 0, .current_value = 1}, {.ds = 1, .current_value = 1},
			{.ds = 2, .current_value = 3}, {.ds = 3, .current_value = 4},
			{.ds = 4, .current_value = 6}, {.ds = 5, .current_value = 7},
			{.ds = 6, .current_value = 9}, {.ds = 7, .current_value = 10},
			{.ds = 8, .current_value = 11}, {.ds = 9, .current_value = 13},
			{.ds = 10, .current_value = 14}, {.ds = 11, .current_value = 16},
			{.ds = 12, .current_value = 17}, {.ds = 13, .current_value = 18},
			{.ds = 14, .current_value = 20}, {.ds = 15, .current_value = 21},
		}
	},
	[SOC_1V8_3V3] {
		/*output low*/
		{
			{.ds = 0, .current_value = 4},   {.ds = 1, .current_value = 7},
			{.ds = 2, .current_value = 9},   {.ds = 3, .current_value = 11},
			{.ds = 4, .current_value = 13},  {.ds = 5, .current_value = 15},
			{.ds = 6, .current_value = 18},  {.ds = 7, .current_value = 20},
			{.ds = 8, .current_value = 22},  {.ds = 9, .current_value = 24},
			{.ds = 10, .current_value = 26}, {.ds = 11, .current_value = 29},
			{.ds = 12, .current_value = 31}, {.ds = 13, .current_value = 33},
			{.ds = 14, .current_value = 35}, {.ds = 15, .current_value = 37},
		},
		/*output high*/
		{
			{.ds = 0, .current_value = 6}, {.ds = 1, .current_value = 10},
			{.ds = 2, .current_value = 13}, {.ds = 3, .current_value = 16},
			{.ds = 4, .current_value = 19}, {.ds = 5, .current_value = 22},
			{.ds = 6, .current_value = 25}, {.ds = 7, .current_value = 28},
			{.ds = 8, .current_value = 32}, {.ds = 9, .current_value = 35},
			{.ds = 10, .current_value = 38}, {.ds = 11, .current_value = 41},
			{.ds = 12, .current_value = 44}, {.ds = 13, .current_value = 47},
			{.ds = 14, .current_value = 51}, {.ds = 15, .current_value = 54},
		}
	}
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
				     unsigned int flags)
{
	void __iomem *mode_select_regs;
	unsigned int ms_reg_domain, ms_bits_offset = 0;
	int i;
	u32 val;

	for (i = 0; i < ipctl->npins; i++) {
		if (ipctl->pins[i].pin_id == pin) {
			ms_reg_domain  = ipctl->pins[i].ms_reg_domain;
			ms_bits_offset = ipctl->pins[i].ms_bits_offset;
			break;
		}
	}

	if (i >= ipctl->npins)
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

static void horizon_pin_pull_disable(struct horizon_pinctrl *ipctl, unsigned int pin)
{
	void __iomem *pull_en_regs;
	void __iomem *pull_up_regs;
	void __iomem *pull_down_regs;
	unsigned int val, reg_domain, pe_bits_offset, pu_bits_offset, pd_bits_offset = 0;
	int i;

	if (!ipctl->pins || !ipctl->npins) {
		dev_err(ipctl->dev, "wrong pinctrl info\n");
	}

	for (i = 0; i < ipctl->npins; i++) {
		if (ipctl->pins[i].pin_id == pin) {
			reg_domain     = ipctl->pins[i].reg_domain;
			pe_bits_offset = ipctl->pins[i].pe_bits_offset;
			pu_bits_offset = ipctl->pins[i].pu_bits_offset;
			pd_bits_offset = ipctl->pins[i].pd_bits_offset;
			break;
		}
	}

	if (i >= ipctl->npins)
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

static void horizon_pin_pull_en(struct horizon_pinctrl *ipctl, unsigned int pin, unsigned int flags)
{
	void __iomem *pull_en_regs;
	void __iomem *pull_select_regs;
	void __iomem *pull_up_regs;
	void __iomem *pull_down_regs;
	unsigned int val, reg_domain, pe_bits_offset, ps_bits_offset, pu_bits_offset,
		pd_bits_offset = 0;
	int i;

	for (i = 0; i < ipctl->npins; i++) {
		if (ipctl->pins[i].pin_id == pin) {
			reg_domain     = ipctl->pins[i].reg_domain;
			pe_bits_offset = ipctl->pins[i].pe_bits_offset;
			ps_bits_offset = ipctl->pins[i].ps_bits_offset;
			pu_bits_offset = ipctl->pins[i].pu_bits_offset;
			pd_bits_offset = ipctl->pins[i].pd_bits_offset;
			break;
		}
	}

	if (i >= ipctl->npins)
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
				  unsigned int flags)
{
	void __iomem *schmit_tg_regs;
	unsigned int val, reg_domain, st_bits_offset = 0;
	int i;

	for (i = 0; i < ipctl->npins; i++) {
		if (ipctl->pins[i].pin_id == pin) {
			reg_domain     = ipctl->pins[i].reg_domain;
			st_bits_offset = ipctl->pins[i].st_bits_offset;
			break;
		}
	}

	if (i >= ipctl->npins)
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
				    unsigned int arg)
{
	void __iomem *drv_str_regs;
	unsigned int val, reg_domain, ds_bits_offset = 0;
	int i;

	for (i = 0; i < ipctl->npins; i++) {
		if (ipctl->pins[i].pin_id == pin) {
			reg_domain     = ipctl->pins[i].reg_domain;
			ds_bits_offset = ipctl->pins[i].ds_bits_offset;
			break;
		}
	}

	if (i >= ipctl->npins)
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

	new_map = devm_kmalloc_array(ipctl->dev, map_num, sizeof(struct pinctrl_map), GFP_KERNEL);
	if (!new_map)
		return -ENOMEM;

	*map	  = new_map;
	*num_maps = map_num;

	/* create mux map */
	parent = of_get_parent(np);
	if (!parent)
		return -EINVAL;

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

static const struct pinctrl_ops horizon_pctrl_ops = {
	.get_groups_count = pinctrl_generic_get_group_count,
	.get_group_name	  = pinctrl_generic_get_group_name,
	.get_group_pins	  = pinctrl_generic_get_group_pins,
	.dt_node_to_map	  = horizon_dt_node_to_map,
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
	ipctl->mux_val[(int)pin_id] = val;
	val &= ~(MUX_ALT3 << horizon_pin->mux_reg_bit);
	val |= horizon_pin->mux_mod << horizon_pin->mux_reg_bit;
	writel(val, ipctl->base + horizon_pin->mux_reg_offset);
	mutex_unlock(&ipctl->mutex);
	dev_dbg(ipctl->dev, "set_pin[%d]: mux_reg_bit:%d mux_mod:%#x val:%#x ipctl->mux_val:%#x\n",
			pin_id,
			horizon_pin->mux_reg_bit,
			horizon_pin->mux_mod, val,
			ipctl->mux_val[(int)pin_id]);

	return 0;
}

static int horizon_pmx_restore_one_pin(struct horizon_pinctrl *ipctl,
				       struct horizon_pin *horizon_pin)
{
	unsigned int pin_id = horizon_pin->pin_id;
	unsigned int val    = 0;

	if (horizon_pin->mux_reg_offset == INVALID_PINMUX) {
		dev_dbg(ipctl->dev, "Pin(%d) does not support mux function\n", pin_id);
		return 0;
	}

	mutex_lock(&ipctl->mutex);
	val = readl(ipctl->base + horizon_pin->mux_reg_offset);
	val &= ~(MUX_ALT3 << horizon_pin->mux_reg_bit);
	val |= (MUX_ALT3 << horizon_pin->mux_reg_bit) &
		   (ipctl->mux_val[(int)pin_id]);
	writel(val, ipctl->base + horizon_pin->mux_reg_offset);
	mutex_unlock(&ipctl->mutex);

	dev_dbg(ipctl->dev, "Restore_pin[%d]: mux_reg_bit:%d, stored mux_val:%#x, val:%#x\n",
			pin_id,
			horizon_pin->mux_reg_bit,
			ipctl->mux_val[(int)pin_id],
			val);
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
	unsigned int gpio_pin;

	dev_dbg(ipctl->dev, "get pin-%d direction\n", pin);

	gpio_range = pinctrl_find_gpio_range_from_pin_nolock(pctldev, pin);
	if (!gpio_range) {
		dev_err(ipctl->dev, "pin-%d can not find corresponding gpio id\n", pin);
		return -ENOTSUPP;
	}
	gpio_pin = pin - gpio_range->pin_base;

	return gpiod_get_direction(gpiochip_get_desc(gpio_range->gc, gpio_pin));
}

static int horizon_gpio_set_direction(struct pinctrl_dev *pctldev, int pin, bool input,
				      int output_val)
{
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	struct pinctrl_gpio_range *gpio_range;
	unsigned int gpio_pin;

	dev_dbg(ipctl->dev, "set pin-%d direction to %s\n", pin, input ? "input" : "output");

	gpio_range = pinctrl_find_gpio_range_from_pin_nolock(pctldev, pin);
	if (!gpio_range) {
		dev_err(ipctl->dev, "pin-%d can not find corresponding gpio id\n", pin);
		return -ENOTSUPP;
	}
	gpio_pin = pin - gpio_range->pin_base;

	if (input)
		return gpiod_direction_input(gpiochip_get_desc(gpio_range->gc, gpio_pin));
	else
		return gpiod_direction_output(gpiochip_get_desc(gpio_range->gc, gpio_pin), output_val);
}

static int horizon_gpio_set_direction_ops(struct pinctrl_dev *pctldev,
					  struct pinctrl_gpio_range *range, unsigned int pin,
					  bool input)
{
	return horizon_gpio_set_direction(pctldev, pin, input, 0);
}

static int horizon_gpio_get_level(struct pinctrl_dev *pctldev, int pin)
{
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	struct pinctrl_gpio_range *gpio_range;
	unsigned int gpio_pin;

	dev_dbg(ipctl->dev, "get pin-%d level\n", pin);

	gpio_range = pinctrl_find_gpio_range_from_pin_nolock(pctldev, pin);
	if (!gpio_range) {
		dev_err(ipctl->dev, "pin-%d can not find corresponding gpio id\n", pin);
		return -ENOTSUPP;
	}
	gpio_pin = pin - gpio_range->pin_base;

	return gpiod_get_value(gpiochip_get_desc(gpio_range->gc, gpio_pin));
}

static struct horizon_pin *horizon_parse_gpio_pin(struct pinctrl_dev *pctldev,
						  struct pinctrl_gpio_range *range,
						  unsigned int pin)
{
	int pin_num;
	int ret = 0;
	unsigned int ngroups, selector = 0;
	struct group_desc *grp;
	struct horizon_pin *horizon_pin;
	const unsigned int *pins      = NULL;
	unsigned int num_pins	      = 0;
	const struct pinctrl_ops *ops = pctldev->desc->pctlops;
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);

	dev_dbg(ipctl->dev, "gpio request gpio pin %d\n", pin);
	pin_num = pin;
	ngroups = pctldev->num_groups;

	while (selector < ngroups) {
		grp = radix_tree_lookup(&pctldev->pin_group_tree, selector);
		if (strstr(grp->name, "gpio") != NULL) {
			ret = ops->get_group_pins(pctldev, selector, &pins, &num_pins);
			if (ret < 0)
				continue;
			for (int i = 0; i < num_pins; i++) {
				horizon_pin = (struct horizon_pin *)(grp->data);
				horizon_pin = &horizon_pin[i];
				if (pin_num == horizon_pin->pin_id) {
					dev_dbg(ipctl->dev, "find pin id %d for gpio request\n",
						 horizon_pin->pin_id);
					return horizon_pin;
				}
			}
		}
		selector++;
	}
	return NULL;
}

static int horizon_gpio_request_enable(struct pinctrl_dev *pctldev,
				       struct pinctrl_gpio_range *range, unsigned int pin)
{
	struct horizon_pin *horizon_pin;
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);

	horizon_pin = horizon_parse_gpio_pin(pctldev, range, pin);
	return horizon_pmx_set_one_pin(ipctl, horizon_pin);
}

static void horizon_gpio_disable_free(struct pinctrl_dev *pctldev, struct pinctrl_gpio_range *range,
				      unsigned int pin)
{
	struct horizon_pin *horizon_pin;
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);

	horizon_pin = horizon_parse_gpio_pin(pctldev, range, pin);
	dev_dbg(ipctl->dev, "horizon pinctrl free gpio request and restore pin func\n");
	horizon_pmx_restore_one_pin(ipctl, horizon_pin);
}

static inline u32 get_mapping_current(unsigned int cell_type, bool level, u32 ds)
{
	return output_current_mapping[cell_type][level][ds].current_value;
}

const struct pinmux_ops horizon_pmx_ops = {
	.get_functions_count = pinmux_generic_get_function_count,
	.get_function_name   = pinmux_generic_get_function_name,
	.get_function_groups = pinmux_generic_get_function_groups,
	.set_mux	     = horizon_pmx_set,
	.gpio_request_enable = horizon_gpio_request_enable,
	.gpio_disable_free   = horizon_gpio_disable_free,
	.gpio_set_direction  = horizon_gpio_set_direction_ops,
};

int horizon_pinconf_get(struct pinctrl_dev *pctldev, unsigned int pin, unsigned long *config)
{
	/* Get the pin config settings for a specific pin */
	struct horizon_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	enum pin_config_param param   = pinconf_to_config_param(*config);
	unsigned int arg, val, reg_domain, ds_bits_offset, pe_bits_offset, ps_bits_offset,
		pu_bits_offset, pd_bits_offset, st_bits_offset, cell_type;
	void __iomem *pull_en_regs;
	void __iomem *pull_select_regs;
	void __iomem *pull_up_regs;
	void __iomem *pull_down_regs;
	void __iomem *schmit_tg_regs;
	void __iomem *drv_str_regs;
	bool level;
	int i;

	if (!ipctl->pins || !ipctl->npins) {
		dev_err(ipctl->dev, "wrong pinctrl info\n");
		return -EINVAL;
	}

	for (i = 0; i < ipctl->npins; i++) {
		if (ipctl->pins[i].pin_id == pin) {
			reg_domain     = ipctl->pins[i].reg_domain;
			ds_bits_offset = ipctl->pins[i].ds_bits_offset;
			pe_bits_offset = ipctl->pins[i].pe_bits_offset;
			ps_bits_offset = ipctl->pins[i].ps_bits_offset;
			pu_bits_offset = ipctl->pins[i].pu_bits_offset;
			pd_bits_offset = ipctl->pins[i].pd_bits_offset;
			st_bits_offset = ipctl->pins[i].st_bits_offset;
			cell_type      = ipctl->pins[i].cell_type;
			break;
		}
	}

	if (i >= ipctl->npins)
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
		level = horizon_gpio_get_level(pctldev, pin);
		if (horizon_gpio_get_direction(pctldev, pin))
			arg = get_mapping_current(cell_type, level, val);
		else
			arg = 0;
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
			unsigned int num_configs)
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
			horizon_pin_power_source(ipctl, pin, arg);
			break;
		case PIN_CONFIG_BIAS_DISABLE:
			horizon_pin_pull_disable(ipctl, pin);
			break;
		case PIN_CONFIG_DRIVE_OPEN_DRAIN:
			/* horizon do not support those configs */
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
			horizon_pin_pull_en(ipctl, pin, HORIZON_PULL_UP);
			break;
		case PIN_CONFIG_BIAS_PULL_DOWN:
			horizon_pin_pull_en(ipctl, pin, HORIZON_PULL_DOWN);
			break;
		case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
			horizon_pin_schmit_en(ipctl, pin, arg);
			break;
		case PIN_CONFIG_DRIVE_STRENGTH:
			horizon_pin_drv_str_set(ipctl, pin, arg);
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

const struct pinconf_ops horizon_pinconf_ops = {
	.is_generic	= true,
	.pin_config_get = horizon_pinconf_get,
	.pin_config_set = horizon_pinconf_set,
};

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
	dev_dbg(ipctl->dev, "Pin_id = %d\n", horizon_pin->pin_id);

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
	func->num_group_names = ipctl->pctl->num_groups;
	if (func->num_group_names == 0) {
		dev_err(ipctl->dev, "No groups defined in %pOF\n", np);
		return -EINVAL;
	}

	group_names = devm_kcalloc(ipctl->dev, func->num_group_names, sizeof(char *), GFP_KERNEL);
	if (!group_names)
		return -ENOMEM;

	for_each_child_of_node (np, child) {
		if (of_property_read_bool(child, "horizon,pins")) {
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
	struct device_node *np = pdev->dev.of_node;
	unsigned int size      = 0;

	ipctl->phandle = of_get_property(np, "horizon,gpio-banks", &size);
	if (!ipctl->phandle || !size) {
		dev_err(ipctl->dev, "no horizon,gpio-banks in node %pOF\n", np);
		return -EINVAL;
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
	pctl->num_groups   = 0;
	if (flat_funcs) {
		for_each_child_of_node (np, child) {
			if (of_property_read_bool(child, "horizon,pins"))
				pctl->num_groups++;
		}
	} else {
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
 */
static inline void horizon_free_resources(struct horizon_pinctrl *ipctl)
{
	if (ipctl->pctl)
		pinctrl_unregister(ipctl->pctl);
}

int horizon_pinctrl_probe(struct platform_device *pdev, struct horizon_pinctrl *info,
			  const struct pinconf_ops *ops)
{
	struct pinctrl_desc *horizon_pinctrl_desc;
	struct pinctrl_pin_desc *pin_desc;
	struct horizon_pinctrl *ipctl;
	struct resource *res;
	int ret, i;

	/* Create device for this driver */
	ipctl = info;

	ipctl->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ipctl->base))
		return PTR_ERR(ipctl->base);
	res		 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res)
		ipctl->mscon = ioremap(res->start, resource_size(res));

	/* parse horizon gpio bank */
	ret = horizon_pinctrl_parse_gpio_bank(pdev, ipctl);


	pin_desc = devm_kzalloc(&pdev->dev, ipctl->npins * sizeof(*pin_desc), GFP_KERNEL);
	if (!pin_desc)
		return -ENOMEM;

	ipctl->mux_val =
		devm_kzalloc(&pdev->dev, ipctl->npins * sizeof(*ipctl->mux_val), GFP_KERNEL);
	if (!ipctl->mux_val)
		return -ENOMEM;

	/* Get pinctrl pin desc from horizon pin desc */
	for (i = 0; i < ipctl->npins; i++) {
		pin_desc[i].number = ipctl->pins[i].pin_id;
		pin_desc[i].name   = ipctl->pins[i].name;
	}

	horizon_pinctrl_desc = devm_kzalloc(&pdev->dev, sizeof(*horizon_pinctrl_desc), GFP_KERNEL);
	if (!horizon_pinctrl_desc)
		return -ENOMEM;

	/* Initial a pinctrl description device */
	horizon_pinctrl_desc->name    = dev_name(&pdev->dev);
	horizon_pinctrl_desc->pins    = pin_desc;
	horizon_pinctrl_desc->npins   = ipctl->npins;
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
