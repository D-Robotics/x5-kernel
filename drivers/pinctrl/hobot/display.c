// SPDX-License-Identifier: GPL-2.0
/*
 * Sunrise5 pinctrl display subsystem driver
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <dt-bindings/pinctrl/horizon-disp-pinfunc.h>
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

/* Driving selector offset */
#define DS_DISP_BT1120 0

/* The bitfield of each pin in schmitter trigger register */
#define ST_DISP_BT1120 BIT(6)

/* The bitfield of each pin in pull up enable register */
#define PU_DISP_BT1120 BIT(4)

/* The bitfield of each pin in pull down enable register */
#define PD_DISP_BT1120 BIT(5)

/* The bitfield of each pin in mode select register */
#define MS_DISP_BT1120_DATA0_3	     BIT(0)
#define MS_DISP_BT1120_DATA4_7	     BIT(1)
#define MS_DISP_BT1120_DATA8_11	     BIT(2)
#define MS_DISP_BT1120_DATA12_15_CLK BIT(3)

static const struct horizon_pin_desc horizon_disp_pins_desc[] = {
	_PIN(DISP_BT1120_DATA0, "disp_bt1120_data0", DISP_IORING_SDIO_BT1120_DATA0,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA0_3, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA1, "disp_bt1120_data1", DISP_IORING_SDIO_BT1120_DATA1,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA0_3, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA2, "disp_bt1120_data2", DISP_IORING_SDIO_BT1120_DATA2,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA0_3, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA3, "disp_bt1120_data3", DISP_IORING_SDIO_BT1120_DATA3,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA0_3, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA4, "disp_bt1120_data4", DISP_IORING_SDIO_BT1120_DATA4,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA4_7, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA5, "disp_bt1120_data5", DISP_IORING_SDIO_BT1120_DATA5,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA4_7, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA6, "disp_bt1120_data6", DISP_IORING_SDIO_BT1120_DATA6,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA4_7, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA7, "disp_bt1120_data7", DISP_IORING_SDIO_BT1120_DATA7,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA4_7, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA8, "disp_bt1120_data8", DISP_IORING_SDIO_BT1120_DATA8,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA8_11, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA9, "disp_bt1120_data9", DISP_IORING_SDIO_BT1120_DATA9,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA8_11, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA10, "disp_bt1120_data10", DISP_IORING_SDIO_BT1120_DATA10,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA8_11, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA11, "disp_bt1120_data11", DISP_IORING_SDIO_BT1120_DATA11,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA8_11, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA12, "disp_bt1120_data12", DISP_IORING_SDIO_BT1120_DATA12,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA12_15_CLK, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA13, "disp_bt1120_data13", DISP_IORING_SDIO_BT1120_DATA13,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA12_15_CLK, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA14, "disp_bt1120_data14", DISP_IORING_SDIO_BT1120_DATA14,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA12_15_CLK, SOC_1V8_3V3),
	_PIN(DISP_BT1120_DATA15, "disp_bt1120_data15", DISP_IORING_SDIO_BT1120_DATA15,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA12_15_CLK, SOC_1V8_3V3),
	_PIN(DISP_BT1120_O_PIXELCLK, "disp_bt1120_o_pixelclk", DISP_IORING_SDIO_BT1120_CLK,
	     DISP_IORING_SDIO_BT1120_MS, DS_DISP_BT1120, INVALID_PULL_BIT, INVALID_PULL_BIT,
	     PU_DISP_BT1120, PD_DISP_BT1120, ST_DISP_BT1120, MS_DISP_BT1120_DATA12_15_CLK, SOC_1V8_3V3),
};

static struct horizon_pinctrl horizon_disp_pinctrl_info = {
	.pins  = horizon_disp_pins_desc,
	.npins = ARRAY_SIZE(horizon_disp_pins_desc),
};

static inline int horizon_disp_pinctrl_probe(struct platform_device *pdev)
{
	return horizon_pinctrl_probe(pdev, &horizon_disp_pinctrl_info, &horizon_pinconf_ops);
}

static const struct of_device_id horizon_disp_pinctrl_of_match[] = {
	{
		.compatible = "d-robotics,x5-disp-iomuxc",
		.data	    = &horizon_disp_pinctrl_info,
	},
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, horizon_disp_pinctrl_of_match);

static struct platform_driver horizon_disp_pinctrl_driver = {
	.probe = horizon_disp_pinctrl_probe,
	.driver =
		{
			.name		= "horizon-disp-pinctrl",
			.of_match_table = horizon_disp_pinctrl_of_match,
		},
};

static int __init horizon_disp_pinctrl_init(void)
{
	return platform_driver_register(&horizon_disp_pinctrl_driver);
}
arch_initcall(horizon_disp_pinctrl_init);

MODULE_DESCRIPTION("Horizon disp pinctrl driver");
MODULE_LICENSE("GPL v2");
