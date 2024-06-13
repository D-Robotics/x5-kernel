
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunrise5 pinctrl high-speed IO subsystem definitions
 *
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef __HORIZON_DSP_PINFUNC_H
#define __HORIZON_DSP_PINFUNC_H
/* Horizon DSP pin ctrl regs */
#define DSP_PINCTRL_0 (0x1C - 0x14)
#define DSP_PINCTRL_1 (0x20 - 0x14)
#define DSP_PINCTRL_2 (0x24 - 0x14)
#define DSP_PINCTRL_3 (0x28 - 0x14)
#define DSP_PINCTRL_4 (0x2C - 0x14)
#define DSP_PINCTRL_5 (0x30 - 0x14)
#define DSP_PINMUX_0  (0x34 - 0x14)
#define DSP_PINMUX_1  (0x38 - 0x14)
#define DSP_PINCTRL_6 (0x3C - 0x14)

/* horizon dsp pin id */
#define DSP_I2C5_SCL  0
#define DSP_I2C5_SDA  1
#define DSP_UART5_RXD 2
#define DSP_UART5_TXD 3
#define DSP_I2S0_MCLK 4
#define DSP_I2S0_SCLK 5
#define DSP_I2S0_WS   6
#define DSP_I2S0_DI   7
#define DSP_I2S0_DO   8
#define DSP_I2S1_MCLK 9
#define DSP_I2S1_SCLK 10
#define DSP_I2S1_WS   11
#define DSP_I2S1_DI   12
#define DSP_I2S1_DO   13
#define DSP_PDM_CKO   14
#define DSP_PDM_IN0   15
#define DSP_PDM_IN1   16
#define DSP_PDM_IN2   17
#define DSP_PDM_IN3   18
#define DSP_SPI6_SCLK 19
#define DSP_SPI6_SSN  20
#define DSP_SPI6_MISO 21
#define DSP_SPI6_MOSI 22

#endif /* __HORIZON_DSP_PINFUNC_H */
