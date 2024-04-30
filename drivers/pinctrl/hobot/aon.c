// SPDX-License-Identifier: GPL-2.0
/*
 * horizon aon pin controller driver
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <dt-bindings/pinctrl/horizon-aon-pinfunc.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include "core.h"
#include "../gpio/gpiolib.h"
#include "pinconf.h"
#include "pinmux.h"
#include "common.h"

/* The bitfield of each pin in schmitter trigger register */
#define ST_AON_BIT0		BIT(0)
#define ST_AON_BIT8		BIT(8)
#define ST_AON_BIT16	BIT(16)
#define ST_AON_BIT24	BIT(24)

/* Driving selector offset */
#define DS_AON_1	1
#define DS_AON_9	9
#define DS_AON_17	17
#define DS_AON_25	25

/* The bitfield of each pin in pull down enable register */
#define PD_AON_BIT5		BIT(5)
#define PD_AON_BIT13	BIT(13)
#define PD_AON_BIT21	BIT(21)
#define PD_AON_BIT29	BIT(29)

/* The bitfield of each pin in pull up enable register */
#define PU_AON_BIT6		BIT(6)
#define PU_AON_BIT14	BIT(14)
#define PU_AON_BIT22	BIT(22)
#define PU_AON_BIT30	BIT(30)

static const struct horizon_pin_desc horizon_aon_pins_desc[] = {
	_PIN(AON_GPIO0_PIN0,  "aon_gpio_pin0", AON_PINCTRL_0, INVALID_REG_DOMAIN, DS_AON_1,INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT6, PD_AON_BIT5, ST_AON_BIT0, INVALID_MS_BIT),
	_PIN(AON_GPIO0_PIN1,  "aon_gpio_pin1", AON_PINCTRL_0, INVALID_REG_DOMAIN, DS_AON_9,INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT14, PD_AON_BIT13, ST_AON_BIT8, INVALID_MS_BIT),
	_PIN(AON_GPIO0_PIN2,  "aon_gpio_pin2", AON_PINCTRL_0, INVALID_REG_DOMAIN, DS_AON_17,INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT22, PD_AON_BIT21, ST_AON_BIT16, INVALID_MS_BIT),
	_PIN(AON_GPIO0_PIN3,  "aon_gpio_pin3", AON_PINCTRL_0, INVALID_REG_DOMAIN, DS_AON_25,INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT30, PD_AON_BIT29, ST_AON_BIT24, INVALID_MS_BIT),
	_PIN(AON_GPIO0_PIN4,  "aon_gpio_pin4", AON_PINCTRL_1, INVALID_REG_DOMAIN, DS_AON_1, INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT6, PD_AON_BIT5, ST_AON_BIT0, INVALID_MS_BIT),
	_PIN(AON_PMIC_EN,  "aon_pmic_en", AON_PINCTRL_2, INVALID_REG_DOMAIN, DS_AON_1, INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT6, PD_AON_BIT5, ST_AON_BIT0, INVALID_MS_BIT),
	_PIN(AON_ENV_VDD,  "aon_env_vdd", AON_PINCTRL_2, INVALID_REG_DOMAIN, DS_AON_25, INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT30, PD_AON_BIT29, ST_AON_BIT24, INVALID_MS_BIT),
	_PIN(AON_ENV_CNN0,  "aon_env_cnn0", AON_PINCTRL_3, INVALID_REG_DOMAIN, DS_AON_1, INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT6, PD_AON_BIT5, ST_AON_BIT0, INVALID_MS_BIT),
	_PIN(AON_ENV_CNN1,  "aon_env_cnn1", AON_PINCTRL_3, INVALID_REG_DOMAIN, DS_AON_9, INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT14, PD_AON_BIT13, ST_AON_BIT8, INVALID_MS_BIT),
	_PIN(AON_HW_RESETN,  "aon_hw_resetn", AON_PINCTRL_3, INVALID_REG_DOMAIN, DS_AON_17, INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT22, PD_AON_BIT21, ST_AON_BIT16, INVALID_MS_BIT),
	_PIN(AON_RESETN_OUT,  "aon_reset_out", AON_PINCTRL_4, INVALID_REG_DOMAIN, DS_AON_1, INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT6, PD_AON_BIT5, ST_AON_BIT0, INVALID_MS_BIT),
	_PIN(AON_IRQ_OUT,  "aon_irq_out", AON_PINCTRL_4, INVALID_REG_DOMAIN, DS_AON_17, INVALID_PULL_BIT, INVALID_PULL_BIT, PU_AON_BIT22, PD_AON_BIT21, ST_AON_BIT16, INVALID_MS_BIT),
	_PIN(AON_XTAL_24M,  "aon_xtal_24m", AON_PINCTRL_5, INVALID_REG_DOMAIN, DS_AON_1,INVALID_PULL_BIT, INVALID_PULL_BIT, INVALID_PULL_BIT, INVALID_PULL_BIT, INVALID_PULL_BIT, INVALID_MS_BIT),
};

static const struct horizon_pinctrl horizon_aon_pinctrl_info = {
	.pins = horizon_aon_pins_desc,
	.npins = ARRAY_SIZE(horizon_aon_pins_desc),
};

static inline int horizon_aon_pinconf_get(struct pinctrl_dev *pctldev,
			  unsigned int pin, unsigned long *config)
{
	return horizon_pinconf_get(pctldev, pin, config, &horizon_aon_pinctrl_info);
}

static inline int horizon_aon_pinconf_set(struct pinctrl_dev *pctldev,
			  unsigned int pin, unsigned long *configs,
			   unsigned int num_configs)
{
	return horizon_pinconf_set(pctldev, pin, configs, num_configs, &horizon_aon_pinctrl_info);
}

static const struct pinconf_ops horizon_aon_pinconf_ops = {
	.is_generic = true,
	.pin_config_get = horizon_aon_pinconf_get,
	.pin_config_set = horizon_aon_pinconf_set,
};

static inline int horizon_aon_pinctrl_probe(struct platform_device *pdev)
{
	return horizon_pinctrl_probe(pdev, &horizon_aon_pinctrl_info, &horizon_aon_pinconf_ops);
}

static const struct of_device_id horizon_aon_pinctrl_of_match[] = {
	{
		.compatible = "d-robotics,x5-aon-iomuxc",
		.data = &horizon_aon_pinctrl_info, },
	{ }
};
MODULE_DEVICE_TABLE(of, horizon_aon_pinctrl_of_match);

static struct platform_driver horizon_aon_pinctrl_driver = {
	.probe =  horizon_aon_pinctrl_probe,
	.driver = {
		.name = "horizon-aon-pinctrl",
		.of_match_table = horizon_aon_pinctrl_of_match,
	},
};

static int __init horizon_aon_pinctrl_init(void)
{
	return platform_driver_register(&horizon_aon_pinctrl_driver);
}
arch_initcall(horizon_aon_pinctrl_init);

MODULE_DESCRIPTION("Horizon aon pinctrl driver");
MODULE_LICENSE("GPL v2");
