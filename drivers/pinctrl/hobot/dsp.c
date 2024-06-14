// SPDX-License-Identifier: GPL-2.0
/*
 * Sunrise5 pinctrl DSP subsystem driver
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <dt-bindings/pinctrl/horizon-dsp-pinfunc.h>
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
#define ST_DSP_UART0_TXD BIT(24)
#define ST_DSP_UART0_RXD BIT(16)
#define ST_DSP_I2C7_SDA  BIT(8)
#define ST_DSP_I2C7_SCL  BIT(0)
#define ST_DSP_I2S0_DI	  BIT(24)
#define ST_DSP_I2S0_WS	  BIT(16)
#define ST_DSP_I2S0_SCLK BIT(8)
#define ST_DSP_I2S0_DO	  BIT(0)
#define ST_DSP_I2S1_DI	  BIT(24)
#define ST_DSP_I2S1_WS	  BIT(16)
#define ST_DSP_I2S1_SCLK BIT(8)
#define ST_DSP_I2S1_DO	  BIT(0)
#define ST_DSP_PDM_CKO	  BIT(16)
#define ST_DSP_I2S1_MCLK BIT(8)
#define ST_DSP_I2S0_MCLK BIT(0)
#define ST_DSP_PDM_IN3	  BIT(24)
#define ST_DSP_PDM_IN2	  BIT(16)
#define ST_DSP_PDM_IN1	  BIT(8)
#define ST_DSP_PDM_IN0	  BIT(0)
#define ST_DSP_SPI6_MOSI BIT(24)
#define ST_DSP_SPI6_MISO BIT(16)
#define ST_DSP_SPI6_SSN  BIT(8)
#define ST_DSP_SPI6_SCLK BIT(0)
/* Driving selector offset */
#define DS_DSP_UART0_TXD 25
#define DS_DSP_UART0_RXD 17
#define DS_DSP_I2C7_SDA  9
#define DS_DSP_I2C7_SCL  1
#define DS_DSP_I2S0_DI	  25
#define DS_DSP_I2S0_WS	  17
#define DS_DSP_I2S0_SCLK 9
#define DS_DSP_I2S0_DO	  1
#define DS_DSP_I2S1_DI	  25
#define DS_DSP_I2S1_WS	  17
#define DS_DSP_I2S1_SCLK 9
#define DS_DSP_I2S1_DO	  1
#define DS_DSP_PDM_CKO	  17
#define DS_DSP_I2S1_MCLK 9
#define DS_DSP_I2S0_MCLK 1
#define DS_DSP_PDM_IN3	  25
#define DS_DSP_PDM_IN2	  17
#define DS_DSP_PDM_IN1	  9
#define DS_DSP_PDM_IN0	  1
#define DS_DSP_SPI6_MOSI 25
#define DS_DSP_SPI6_MISO 17
#define DS_DSP_SPI6_SSN  9
#define DS_DSP_SPI6_SCLK 1
/* The bitfield of each pin in pull down enable register */
#define PD_DSP_I2S1_DI	  BIT(29)
#define PD_DSP_I2S1_WS	  BIT(21)
#define PD_DSP_I2S1_SCLK BIT(13)
#define PD_DSP_I2S1_DO	  BIT(5)
#define PD_DSP_I2S1_MCLK BIT(13)
/* The bitfield of each pin in pull up enable register */
#define PU_DSP_UART0_TXD BIT(31)
#define PU_DSP_UART0_RXD BIT(23)
#define PU_DSP_I2C7_SDA  BIT(15)
#define PU_DSP_I2C7_SCL  BIT(7)
#define PU_DSP_I2S0_DI	  BIT(31)
#define PU_DSP_I2S0_WS	  BIT(23)
#define PU_DSP_I2S0_SCLK BIT(15)
#define PU_DSP_I2S0_DO	  BIT(7)
#define PU_DSP_I2S1_DI	  BIT(30)
#define PU_DSP_I2S1_WS	  BIT(22)
#define PU_DSP_I2S1_SCLK BIT(14)
#define PU_DSP_I2S1_DO	  BIT(6)
#define PU_DSP_PDM_CKO	  BIT(23)
#define PU_DSP_I2S1_MCLK BIT(14)
#define PU_DSP_I2S0_MCLK BIT(7)
#define PU_DSP_PDM_IN3	  BIT(31)
#define PU_DSP_PDM_IN2	  BIT(23)
#define PU_DSP_PDM_IN1	  BIT(15)
#define PU_DSP_PDM_IN0	  BIT(7)
#define PU_DSP_SPI6_MOSI BIT(31)
#define PU_DSP_SPI6_MISO BIT(23)
#define PU_DSP_SPI6_SSN  BIT(15)
#define PU_DSP_SPI6_SCLK BIT(7)
/* The bitfield of each pin in pull enable register */
#define PE_DSP_UART0_TXD BIT(30)
#define PE_DSP_UART0_RXD BIT(22)
#define PE_DSP_I2C7_SDA  BIT(14)
#define PE_DSP_I2C7_SCL  BIT(6)
#define PE_DSP_I2S0_DI	  BIT(30)
#define PE_DSP_I2S0_WS	  BIT(22)
#define PE_DSP_I2S0_SCLK BIT(14)
#define PE_DSP_I2S0_DO	  BIT(6)
#define PE_DSP_PDM_CKO	  BIT(22)
#define PE_DSP_I2S0_MCLK BIT(6)
#define PE_DSP_PDM_IN3	  BIT(30)
#define PE_DSP_PDM_IN2	  BIT(22)
#define PE_DSP_PDM_IN1	  BIT(14)
#define PE_DSP_PDM_IN0	  BIT(6)
#define PE_DSP_SPI6_MOSI BIT(30)
#define PE_DSP_SPI6_MISO BIT(22)
#define PE_DSP_SPI6_SSN  BIT(14)
#define PE_DSP_SPI6_SCLK BIT(6)
/* The bitfield of each pin in pull select register */
#define PS_DSP_UART0_TXD BIT(29)
#define PS_DSP_UART0_RXD BIT(21)
#define PS_DSP_I2C7_SDA  BIT(13)
#define PS_DSP_I2C7_SCL  BIT(5)
#define PS_DSP_I2S0_DI	  BIT(29)
#define PS_DSP_I2S0_WS	  BIT(21)
#define PS_DSP_I2S0_SCLK BIT(13)
#define PS_DSP_I2S0_DO	  BIT(5)
#define PS_DSP_PDM_CKO	  BIT(21)
#define PS_DSP_I2S0_MCLK BIT(5)
#define PS_DSP_PDM_IN3	  BIT(29)
#define PS_DSP_PDM_IN2	  BIT(21)
#define PS_DSP_PDM_IN1	  BIT(13)
#define PS_DSP_PDM_IN0	  BIT(5)
#define PS_DSP_SPI6_MOSI BIT(29)
#define PS_DSP_SPI6_MISO BIT(21)
#define PS_DSP_SPI6_SSN  BIT(13)
#define PS_DSP_SPI6_SCLK BIT(5)
/* The bitfield of each pin in mode select register */
#define MS_DSP_I2S1 BIT(31)

static const struct horizon_pin_desc horizon_dsp_pins_desc[] = {
	_PIN(DSP_UART0_TXD, "dsp_uart0_txd", DSP_PINCTRL_0, INVALID_REG_DOMAIN, DS_DSP_UART0_TXD,
	     PE_DSP_UART0_TXD, PS_DSP_UART0_TXD, PU_DSP_UART0_TXD, INVALID_PULL_BIT,
	     ST_DSP_UART0_TXD, INVALID_MS_BIT),
	_PIN(DSP_UART0_RXD, "dsp_uart0_rxd", DSP_PINCTRL_0, INVALID_REG_DOMAIN, DS_DSP_UART0_RXD,
	     PE_DSP_UART0_RXD, PS_DSP_UART0_RXD, PU_DSP_UART0_RXD, INVALID_PULL_BIT,
	     ST_DSP_UART0_RXD, INVALID_MS_BIT),
	_PIN(DSP_I2C7_SDA, "dsp_i2c7_sda", DSP_PINCTRL_0, INVALID_REG_DOMAIN, DS_DSP_I2C7_SDA,
	     PE_DSP_I2C7_SDA, PS_DSP_I2C7_SDA, PU_DSP_I2C7_SDA, INVALID_PULL_BIT,
	     ST_DSP_I2C7_SDA, INVALID_MS_BIT),
	_PIN(DSP_I2C7_SCL, "dsp_i2c7_scl", DSP_PINCTRL_0, INVALID_REG_DOMAIN, DS_DSP_I2C7_SCL,
	     PE_DSP_I2C7_SCL, PS_DSP_I2C7_SCL, PU_DSP_I2C7_SCL, INVALID_PULL_BIT,
	     ST_DSP_I2C7_SCL, INVALID_MS_BIT),
	_PIN(DSP_I2S0_DI, "dsp_i2s0_di", DSP_PINCTRL_1, INVALID_REG_DOMAIN, DS_DSP_I2S0_DI,
	     PE_DSP_I2S0_DI, PS_DSP_I2S0_DI, PU_DSP_I2S0_DI, INVALID_PULL_BIT, ST_DSP_I2S0_DI,
	     INVALID_MS_BIT),
	_PIN(DSP_I2S0_WS, "dsp_i2s0_ws", DSP_PINCTRL_1, INVALID_REG_DOMAIN, DS_DSP_I2S0_WS,
	     PE_DSP_I2S0_WS, PS_DSP_I2S0_WS, PU_DSP_I2S0_WS, INVALID_PULL_BIT, ST_DSP_I2S0_WS,
	     INVALID_MS_BIT),
	_PIN(DSP_I2S0_SCLK, "dsp_i2s0_sclk", DSP_PINCTRL_1, INVALID_REG_DOMAIN, DS_DSP_I2S0_SCLK,
	     PE_DSP_I2S0_SCLK, PS_DSP_I2S0_SCLK, PU_DSP_I2S0_SCLK, INVALID_PULL_BIT,
	     ST_DSP_I2S0_SCLK, INVALID_MS_BIT),
	_PIN(DSP_I2S0_DO, "dsp_i2s0_do", DSP_PINCTRL_1, INVALID_REG_DOMAIN, DS_DSP_I2S0_DO,
	     PE_DSP_I2S0_DO, PS_DSP_I2S0_DO, PU_DSP_I2S0_DO, INVALID_PULL_BIT, ST_DSP_I2S0_DO,
	     INVALID_MS_BIT),
	_PIN(DSP_I2S1_DI, "dsp_i2s1_di", DSP_PINCTRL_2, DSP_PINCTRL_2, DS_DSP_I2S1_DI,
	     INVALID_PULL_BIT, INVALID_PULL_BIT, PU_DSP_I2S1_DI, PD_DSP_I2S1_DI, ST_DSP_I2S1_DI,
	     MS_DSP_I2S1),
	_PIN(DSP_I2S1_WS, "dsp_i2s1_ws", DSP_PINCTRL_2, DSP_PINCTRL_2, DS_DSP_I2S1_WS,
	     INVALID_PULL_BIT, INVALID_PULL_BIT, PU_DSP_I2S1_WS, PD_DSP_I2S1_WS, ST_DSP_I2S1_WS,
	     MS_DSP_I2S1),
	_PIN(DSP_I2S1_SCLK, "dsp_i2s1_sclk", DSP_PINCTRL_2, DSP_PINCTRL_2, DS_DSP_I2S1_SCLK,
	     INVALID_PULL_BIT, INVALID_PULL_BIT, PU_DSP_I2S1_SCLK, PD_DSP_I2S1_SCLK,
	     ST_DSP_I2S1_SCLK, MS_DSP_I2S1),
	_PIN(DSP_I2S1_DO, "dsp_i2s1_do", DSP_PINCTRL_2, DSP_PINCTRL_2, DS_DSP_I2S1_DO,
	     INVALID_PULL_BIT, INVALID_PULL_BIT, PU_DSP_I2S1_DO, PD_DSP_I2S1_DO, ST_DSP_I2S1_DO,
	     MS_DSP_I2S1),
	_PIN(DSP_PDM_CKO, "dsp_pdm_cko", DSP_PINCTRL_3, INVALID_REG_DOMAIN, DS_DSP_PDM_CKO,
	     PE_DSP_PDM_CKO, PS_DSP_PDM_CKO, PU_DSP_PDM_CKO, INVALID_PULL_BIT, ST_DSP_PDM_CKO,
	     INVALID_MS_BIT),
	_PIN(DSP_I2S1_MCLK, "dsp_i2s1_mclk", DSP_PINCTRL_3, DSP_PINCTRL_2, DS_DSP_I2S1_MCLK,
	     INVALID_PULL_BIT, INVALID_PULL_BIT, PU_DSP_I2S1_MCLK, PD_DSP_I2S1_MCLK,
	     ST_DSP_I2S1_MCLK, MS_DSP_I2S1),
	_PIN(DSP_I2S0_MCLK, "dsp_i2s0_mclk", DSP_PINCTRL_3, INVALID_REG_DOMAIN, DS_DSP_I2S0_MCLK,
	     PE_DSP_I2S0_MCLK, PS_DSP_I2S0_MCLK, PU_DSP_I2S0_MCLK, INVALID_PULL_BIT,
	     ST_DSP_I2S0_MCLK, INVALID_MS_BIT),
	_PIN(DSP_PDM_IN3, "dsp_pdm_in3", DSP_PINCTRL_4, INVALID_REG_DOMAIN, DS_DSP_PDM_IN3,
	     PE_DSP_PDM_IN3, PS_DSP_PDM_IN3, PU_DSP_PDM_IN3, INVALID_PULL_BIT, ST_DSP_PDM_IN3,
	     INVALID_MS_BIT),
	_PIN(DSP_PDM_IN2, "dsp_pdm_in2", DSP_PINCTRL_4, INVALID_REG_DOMAIN, DS_DSP_PDM_IN2,
	     PE_DSP_PDM_IN2, PS_DSP_PDM_IN2, PU_DSP_PDM_IN2, INVALID_PULL_BIT, ST_DSP_PDM_IN2,
	     INVALID_MS_BIT),
	_PIN(DSP_PDM_IN1, "dsp_pdm_in1", DSP_PINCTRL_4, INVALID_REG_DOMAIN, DS_DSP_PDM_IN1,
	     PE_DSP_PDM_IN1, PS_DSP_PDM_IN1, PU_DSP_PDM_IN1, INVALID_PULL_BIT, ST_DSP_PDM_IN1,
	     INVALID_MS_BIT),
	_PIN(DSP_PDM_IN0, "dsp_pdm_in0", DSP_PINCTRL_4, INVALID_REG_DOMAIN, DS_DSP_PDM_IN0,
	     PE_DSP_PDM_IN0, PS_DSP_PDM_IN0, PU_DSP_PDM_IN0, INVALID_PULL_BIT, ST_DSP_PDM_IN0,
	     INVALID_MS_BIT),
	_PIN(DSP_SPI6_MOSI, "dsp_spi6_mosi", DSP_PINCTRL_5, INVALID_REG_DOMAIN, DS_DSP_SPI6_MOSI,
	     PE_DSP_SPI6_MOSI, PS_DSP_SPI6_MOSI, PU_DSP_SPI6_MOSI, INVALID_PULL_BIT,
	     ST_DSP_SPI6_MOSI, INVALID_MS_BIT),
	_PIN(DSP_SPI6_MISO, "dsp_spi6_miso", DSP_PINCTRL_5, INVALID_REG_DOMAIN, DS_DSP_SPI6_MISO,
	     PE_DSP_SPI6_MISO, PS_DSP_SPI6_MISO, PU_DSP_SPI6_MISO, INVALID_PULL_BIT,
	     ST_DSP_SPI6_MISO, INVALID_MS_BIT),
	_PIN(DSP_SPI6_SSN, "dsp_spi6_ssn", DSP_PINCTRL_5, INVALID_REG_DOMAIN, DS_DSP_SPI6_SSN,
	     PE_DSP_SPI6_SSN, PS_DSP_SPI6_SSN, PU_DSP_SPI6_SSN, INVALID_PULL_BIT,
	     ST_DSP_SPI6_SSN, INVALID_MS_BIT),
	_PIN(DSP_SPI6_SCLK, "dsp_spi6P_sclk", DSP_PINCTRL_5, INVALID_REG_DOMAIN,
	     DS_DSP_SPI6_SCLK, PE_DSP_SPI6_SCLK, PS_DSP_SPI6_SCLK, PU_DSP_SPI6_SCLK,
	     INVALID_PULL_BIT, ST_DSP_SPI6_SCLK, INVALID_MS_BIT),
};
static const struct horizon_pinctrl horizon_dsp_pinctrl_info = {
	.pins  = horizon_dsp_pins_desc,
	.npins = ARRAY_SIZE(horizon_dsp_pins_desc),
};
static inline int horizon_dsp_pinconf_get(struct pinctrl_dev *pctldev, unsigned int pin,
					  unsigned long *config)
{
	return horizon_pinconf_get(pctldev, pin, config, &horizon_dsp_pinctrl_info);
}
static inline int horizon_dsp_pinconf_set(struct pinctrl_dev *pctldev, unsigned int pin,
					  unsigned long *configs, unsigned int num_configs)
{
	return horizon_pinconf_set(pctldev, pin, configs, num_configs, &horizon_dsp_pinctrl_info);
}
static const struct pinconf_ops horizon_dsp_pinconf_ops = {
	.is_generic	= true,
	.pin_config_get = horizon_dsp_pinconf_get,
	.pin_config_set = horizon_dsp_pinconf_set,
};
static inline int horizon_dsp_pinctrl_probe(struct platform_device *pdev)
{
	return horizon_pinctrl_probe(pdev, &horizon_dsp_pinctrl_info, &horizon_dsp_pinconf_ops);
}
static const struct of_device_id horizon_dsp_pinctrl_of_match[] = {
	{
		.compatible = "d-robotics,horizon-dsp-iomuxc",
		.data	    = &horizon_dsp_pinctrl_info,
	},
	{}};
MODULE_DEVICE_TABLE(of, horizon_dsp_pinctrl_of_match);
static struct platform_driver horizon_dsp_pinctrl_driver = {
	.probe = horizon_dsp_pinctrl_probe,
	.driver =
		{
			.name		= "horizon-dsp-pinctrl",
			.of_match_table = horizon_dsp_pinctrl_of_match,
		},
};
static int __init horizon_dsp_pinctrl_init(void)
{
	return platform_driver_register(&horizon_dsp_pinctrl_driver);
}
arch_initcall(horizon_dsp_pinctrl_init);
MODULE_DESCRIPTION("Horizon dsp pinctrl driver");
MODULE_LICENSE("GPL v2");
