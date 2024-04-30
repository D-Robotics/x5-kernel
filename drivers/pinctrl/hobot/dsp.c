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
#define ST_HIFI_UART5_TXD BIT(24)
#define ST_HIFI_UART5_RXD BIT(16)
#define ST_HIFI_I2C5_SDA  BIT(8)
#define ST_HIFI_I2C5_SCL  BIT(0)
#define ST_HIFI_I2S0_DI	  BIT(24)
#define ST_HIFI_I2S0_WS	  BIT(16)
#define ST_HIFI_I2S0_SCLK BIT(8)
#define ST_HIFI_I2S0_DO	  BIT(0)
#define ST_HIFI_I2S1_DI	  BIT(24)
#define ST_HIFI_I2S1_WS	  BIT(16)
#define ST_HIFI_I2S1_SCLK BIT(8)
#define ST_HIFI_I2S1_DO	  BIT(0)
#define ST_HIFI_PDM_CKO	  BIT(16)
#define ST_HIFI_I2S1_MCLK BIT(8)
#define ST_HIFI_I2S0_MCLK BIT(0)
#define ST_HIFI_PDM_IN3	  BIT(24)
#define ST_HIFI_PDM_IN2	  BIT(16)
#define ST_HIFI_PDM_IN1	  BIT(8)
#define ST_HIFI_PDM_IN0	  BIT(0)
#define ST_HIFI_SPI6_MOSI BIT(24)
#define ST_HIFI_SPI6_MISO BIT(16)
#define ST_HIFI_SPI6_SSN  BIT(8)
#define ST_HIFI_SPI6_SCLK BIT(0)
/* Driving selector offset */
#define DS_HIFI_UART5_TXD 25
#define DS_HIFI_UART5_RXD 17
#define DS_HIFI_I2C5_SDA  9
#define DS_HIFI_I2C5_SCL  1
#define DS_HIFI_I2S0_DI	  25
#define DS_HIFI_I2S0_WS	  17
#define DS_HIFI_I2S0_SCLK 9
#define DS_HIFI_I2S0_DO	  1
#define DS_HIFI_I2S1_DI	  25
#define DS_HIFI_I2S1_WS	  17
#define DS_HIFI_I2S1_SCLK 9
#define DS_HIFI_I2S1_DO	  1
#define DS_HIFI_PDM_CKO	  17
#define DS_HIFI_I2S1_MCLK 9
#define DS_HIFI_I2S0_MCLK 1
#define DS_HIFI_PDM_IN3	  25
#define DS_HIFI_PDM_IN2	  17
#define DS_HIFI_PDM_IN1	  9
#define DS_HIFI_PDM_IN0	  1
#define DS_HIFI_SPI6_MOSI 25
#define DS_HIFI_SPI6_MISO 17
#define DS_HIFI_SPI6_SSN  9
#define DS_HIFI_SPI6_SCLK 1
/* The bitfield of each pin in pull down enable register */
#define PD_HIFI_I2S1_DI	  BIT(29)
#define PD_HIFI_I2S1_WS	  BIT(21)
#define PD_HIFI_I2S1_SCLK BIT(13)
#define PD_HIFI_I2S1_DO	  BIT(5)
#define PD_HIFI_I2S1_MCLK BIT(13)
/* The bitfield of each pin in pull up enable register */
#define PU_HIFI_UART5_TXD BIT(31)
#define PU_HIFI_UART5_RXD BIT(23)
#define PU_HIFI_I2C5_SDA  BIT(15)
#define PU_HIFI_I2C5_SCL  BIT(7)
#define PU_HIFI_I2S0_DI	  BIT(31)
#define PU_HIFI_I2S0_WS	  BIT(23)
#define PU_HIFI_I2S0_SCLK BIT(15)
#define PU_HIFI_I2S0_DO	  BIT(7)
#define PU_HIFI_I2S1_DI	  BIT(30)
#define PU_HIFI_I2S1_WS	  BIT(22)
#define PU_HIFI_I2S1_SCLK BIT(14)
#define PU_HIFI_I2S1_DO	  BIT(6)
#define PU_HIFI_PDM_CKO	  BIT(23)
#define PU_HIFI_I2S1_MCLK BIT(14)
#define PU_HIFI_I2S0_MCLK BIT(7)
#define PU_HIFI_PDM_IN3	  BIT(31)
#define PU_HIFI_PDM_IN2	  BIT(23)
#define PU_HIFI_PDM_IN1	  BIT(15)
#define PU_HIFI_PDM_IN0	  BIT(7)
#define PU_HIFI_SPI6_MOSI BIT(31)
#define PU_HIFI_SPI6_MISO BIT(23)
#define PU_HIFI_SPI6_SSN  BIT(15)
#define PU_HIFI_SPI6_SCLK BIT(7)
/* The bitfield of each pin in pull enable register */
#define PE_HIFI_UART5_TXD BIT(30)
#define PE_HIFI_UART5_RXD BIT(22)
#define PE_HIFI_I2C5_SDA  BIT(14)
#define PE_HIFI_I2C5_SCL  BIT(6)
#define PE_HIFI_I2S0_DI	  BIT(30)
#define PE_HIFI_I2S0_WS	  BIT(22)
#define PE_HIFI_I2S0_SCLK BIT(14)
#define PE_HIFI_I2S0_DO	  BIT(6)
#define PE_HIFI_PDM_CKO	  BIT(22)
#define PE_HIFI_I2S0_MCLK BIT(6)
#define PE_HIFI_PDM_IN3	  BIT(30)
#define PE_HIFI_PDM_IN2	  BIT(22)
#define PE_HIFI_PDM_IN1	  BIT(14)
#define PE_HIFI_PDM_IN0	  BIT(6)
#define PE_HIFI_SPI6_MOSI BIT(30)
#define PE_HIFI_SPI6_MISO BIT(22)
#define PE_HIFI_SPI6_SSN  BIT(14)
#define PE_HIFI_SPI6_SCLK BIT(6)
/* The bitfield of each pin in pull select register */
#define PS_HIFI_UART5_TXD BIT(29)
#define PS_HIFI_UART5_RXD BIT(21)
#define PS_HIFI_I2C5_SDA  BIT(13)
#define PS_HIFI_I2C5_SCL  BIT(5)
#define PS_HIFI_I2S0_DI	  BIT(29)
#define PS_HIFI_I2S0_WS	  BIT(21)
#define PS_HIFI_I2S0_SCLK BIT(13)
#define PS_HIFI_I2S0_DO	  BIT(5)
#define PS_HIFI_PDM_CKO	  BIT(21)
#define PS_HIFI_I2S0_MCLK BIT(5)
#define PS_HIFI_PDM_IN3	  BIT(29)
#define PS_HIFI_PDM_IN2	  BIT(21)
#define PS_HIFI_PDM_IN1	  BIT(13)
#define PS_HIFI_PDM_IN0	  BIT(5)
#define PS_HIFI_SPI6_MOSI BIT(29)
#define PS_HIFI_SPI6_MISO BIT(21)
#define PS_HIFI_SPI6_SSN  BIT(13)
#define PS_HIFI_SPI6_SCLK BIT(5)
/* The bitfield of each pin in mode select register */
#define MS_HIFI_I2S1 BIT(31)

static const struct horizon_pin_desc horizon_dsp_pins_desc[] = {
	_PIN(HIFI_UART5_TXD, "hifi_uart5_txd", DSP_PINCTRL_0, INVALID_REG_DOMAIN, DS_HIFI_UART5_TXD,
	     PE_HIFI_UART5_TXD, PS_HIFI_UART5_TXD, PU_HIFI_UART5_TXD, INVALID_PULL_BIT,
	     ST_HIFI_UART5_TXD, INVALID_MS_BIT),
	_PIN(HIFI_UART5_RXD, "hifi_uart5_rxd", DSP_PINCTRL_0, INVALID_REG_DOMAIN, DS_HIFI_UART5_RXD,
	     PE_HIFI_UART5_RXD, PS_HIFI_UART5_RXD, PU_HIFI_UART5_RXD, INVALID_PULL_BIT,
	     ST_HIFI_UART5_RXD, INVALID_MS_BIT),
	_PIN(HIFI_I2C5_SDA, "hifi_i2c5_sda", DSP_PINCTRL_0, INVALID_REG_DOMAIN, DS_HIFI_I2C5_SDA,
	     PE_HIFI_I2C5_SDA, PS_HIFI_I2C5_SDA, PU_HIFI_I2C5_SDA, INVALID_PULL_BIT,
	     ST_HIFI_I2C5_SDA, INVALID_MS_BIT),
	_PIN(HIFI_I2C5_SCL, "hifi_i2c5_scl", DSP_PINCTRL_0, INVALID_REG_DOMAIN, DS_HIFI_I2C5_SCL,
	     PE_HIFI_I2C5_SCL, PS_HIFI_I2C5_SCL, PU_HIFI_I2C5_SCL, INVALID_PULL_BIT,
	     ST_HIFI_I2C5_SCL, INVALID_MS_BIT),
	_PIN(HIFI_I2S0_DI, "hifi_i2s0_di", DSP_PINCTRL_1, INVALID_REG_DOMAIN, DS_HIFI_I2S0_DI,
	     PE_HIFI_I2S0_DI, PS_HIFI_I2S0_DI, PU_HIFI_I2S0_DI, INVALID_PULL_BIT, ST_HIFI_I2S0_DI,
	     INVALID_MS_BIT),
	_PIN(HIFI_I2S0_WS, "hifi_i2s0_ws", DSP_PINCTRL_1, INVALID_REG_DOMAIN, DS_HIFI_I2S0_WS,
	     PE_HIFI_I2S0_WS, PS_HIFI_I2S0_WS, PU_HIFI_I2S0_WS, INVALID_PULL_BIT, ST_HIFI_I2S0_WS,
	     INVALID_MS_BIT),
	_PIN(HIFI_I2S0_SCLK, "hifi_i2s0_sclk", DSP_PINCTRL_1, INVALID_REG_DOMAIN, DS_HIFI_I2S0_SCLK,
	     PE_HIFI_I2S0_SCLK, PS_HIFI_I2S0_SCLK, PU_HIFI_I2S0_SCLK, INVALID_PULL_BIT,
	     ST_HIFI_I2S0_SCLK, INVALID_MS_BIT),
	_PIN(HIFI_I2S0_DO, "hifi_i2s0_do", DSP_PINCTRL_1, INVALID_REG_DOMAIN, DS_HIFI_I2S0_DO,
	     PE_HIFI_I2S0_DO, PS_HIFI_I2S0_DO, PU_HIFI_I2S0_DO, INVALID_PULL_BIT, ST_HIFI_I2S0_DO,
	     INVALID_MS_BIT),
	_PIN(HIFI_I2S1_DI, "hifi_i2s1_di", DSP_PINCTRL_2, DSP_PINCTRL_2, DS_HIFI_I2S1_DI,
	     INVALID_PULL_BIT, INVALID_PULL_BIT, PU_HIFI_I2S1_DI, PD_HIFI_I2S1_DI, ST_HIFI_I2S1_DI,
	     MS_HIFI_I2S1),
	_PIN(HIFI_I2S1_WS, "hifi_i2s1_ws", DSP_PINCTRL_2, DSP_PINCTRL_2, DS_HIFI_I2S1_WS,
	     INVALID_PULL_BIT, INVALID_PULL_BIT, PU_HIFI_I2S1_WS, PD_HIFI_I2S1_WS, ST_HIFI_I2S1_WS,
	     MS_HIFI_I2S1),
	_PIN(HIFI_I2S1_SCLK, "hifi_i2s1_sclk", DSP_PINCTRL_2, DSP_PINCTRL_2, DS_HIFI_I2S1_SCLK,
	     INVALID_PULL_BIT, INVALID_PULL_BIT, PU_HIFI_I2S1_SCLK, PD_HIFI_I2S1_SCLK,
	     ST_HIFI_I2S1_SCLK, MS_HIFI_I2S1),
	_PIN(HIFI_I2S1_DO, "hifi_i2s1_do", DSP_PINCTRL_2, DSP_PINCTRL_2, DS_HIFI_I2S1_DO,
	     INVALID_PULL_BIT, INVALID_PULL_BIT, PU_HIFI_I2S1_DO, PD_HIFI_I2S1_DO, ST_HIFI_I2S1_DO,
	     MS_HIFI_I2S1),
	_PIN(HIFI_PDM_CKO, "hifi_pdm_cko", DSP_PINCTRL_3, INVALID_REG_DOMAIN, DS_HIFI_PDM_CKO,
	     PE_HIFI_PDM_CKO, PS_HIFI_PDM_CKO, PU_HIFI_PDM_CKO, INVALID_PULL_BIT, ST_HIFI_PDM_CKO,
	     INVALID_MS_BIT),
	_PIN(HIFI_I2S1_MCLK, "hifi_i2s1_mclk", DSP_PINCTRL_3, DSP_PINCTRL_2, DS_HIFI_I2S1_MCLK,
	     INVALID_PULL_BIT, INVALID_PULL_BIT, PU_HIFI_I2S1_MCLK, PD_HIFI_I2S1_MCLK,
	     ST_HIFI_I2S1_MCLK, MS_HIFI_I2S1),
	_PIN(HIFI_I2S0_MCLK, "hifi_i2s0_mclk", DSP_PINCTRL_3, INVALID_REG_DOMAIN, DS_HIFI_I2S0_MCLK,
	     PE_HIFI_I2S0_MCLK, PS_HIFI_I2S0_MCLK, PU_HIFI_I2S0_MCLK, INVALID_PULL_BIT,
	     ST_HIFI_I2S0_MCLK, INVALID_MS_BIT),
	_PIN(HIFI_PDM_IN3, "hifi_pdm_in3", DSP_PINCTRL_4, INVALID_REG_DOMAIN, DS_HIFI_PDM_IN3,
	     PE_HIFI_PDM_IN3, PS_HIFI_PDM_IN3, PU_HIFI_PDM_IN3, INVALID_PULL_BIT, ST_HIFI_PDM_IN3,
	     INVALID_MS_BIT),
	_PIN(HIFI_PDM_IN2, "hifi_pdm_in2", DSP_PINCTRL_4, INVALID_REG_DOMAIN, DS_HIFI_PDM_IN2,
	     PE_HIFI_PDM_IN2, PS_HIFI_PDM_IN2, PU_HIFI_PDM_IN2, INVALID_PULL_BIT, ST_HIFI_PDM_IN2,
	     INVALID_MS_BIT),
	_PIN(HIFI_PDM_IN1, "hifi_pdm_in1", DSP_PINCTRL_4, INVALID_REG_DOMAIN, DS_HIFI_PDM_IN1,
	     PE_HIFI_PDM_IN1, PS_HIFI_PDM_IN1, PU_HIFI_PDM_IN1, INVALID_PULL_BIT, ST_HIFI_PDM_IN1,
	     INVALID_MS_BIT),
	_PIN(HIFI_PDM_IN0, "hifi_pdm_in0", DSP_PINCTRL_4, INVALID_REG_DOMAIN, DS_HIFI_PDM_IN0,
	     PE_HIFI_PDM_IN0, PS_HIFI_PDM_IN0, PU_HIFI_PDM_IN0, INVALID_PULL_BIT, ST_HIFI_PDM_IN0,
	     INVALID_MS_BIT),
	_PIN(HIFI_SPI6_MOSI, "hifi_spi6_mosi", DSP_PINCTRL_5, INVALID_REG_DOMAIN, DS_HIFI_SPI6_MOSI,
	     PE_HIFI_SPI6_MOSI, PS_HIFI_SPI6_MOSI, PU_HIFI_SPI6_MOSI, INVALID_PULL_BIT,
	     ST_HIFI_SPI6_MOSI, INVALID_MS_BIT),
	_PIN(HIFI_SPI6_MISO, "hifi_spi6_miso", DSP_PINCTRL_5, INVALID_REG_DOMAIN, DS_HIFI_SPI6_MISO,
	     PE_HIFI_SPI6_MISO, PS_HIFI_SPI6_MISO, PU_HIFI_SPI6_MISO, INVALID_PULL_BIT,
	     ST_HIFI_SPI6_MISO, INVALID_MS_BIT),
	_PIN(HIFI_SPI6_SSN, "hifi_spi6_ssn", DSP_PINCTRL_5, INVALID_REG_DOMAIN, DS_HIFI_SPI6_SSN,
	     PE_HIFI_SPI6_SSN, PS_HIFI_SPI6_SSN, PU_HIFI_SPI6_SSN, INVALID_PULL_BIT,
	     ST_HIFI_SPI6_SSN, INVALID_MS_BIT),
	_PIN(HIFI_SPI6_SCLK, "hifi_spi6P_sclk", DSP_PINCTRL_5, INVALID_REG_DOMAIN,
	     DS_HIFI_SPI6_SCLK, PE_HIFI_SPI6_SCLK, PS_HIFI_SPI6_SCLK, PU_HIFI_SPI6_SCLK,
	     INVALID_PULL_BIT, ST_HIFI_SPI6_SCLK, INVALID_MS_BIT),
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
