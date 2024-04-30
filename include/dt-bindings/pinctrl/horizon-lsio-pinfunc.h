/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunrise5 pinctrl low-speed IO subsystem definitions
 *
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef __HORIZON_LSIO_PINFUNC_H
#define __HORIZON_LSIO_PINFUNC_H

/* Horizon lsio pin ctrl regs */
#define LSIO_PINCTRL_0 0x8
#define LSIO_PINCTRL_1 0xc
#define LSIO_PINCTRL_2 0x10
#define LSIO_PINCTRL_3 0x18
#define LSIO_PINCTRL_4 0x1c
#define LSIO_PINCTRL_5 0x20
#define LSIO_PINCTRL_6 0x24
#define LSIO_PINCTRL_7 0x28
#define LSIO_PINCTRL_8 0x2c
#define LSIO_PINCTRL_9 0x30
#define LSIO_PINCTRL_10 0x34
#define LSIO_PINCTRL_11 0x38
#define LSIO_PINCTRL_12 0x3c
#define LSIO_PINCTRL_13 0x40
#define LSIO_PINCTRL_14 0x44

#define LSIO_PINMUX_0 0x78
#define LSIO_PINMUX_1 0x7c
#define LSIO_PINMUX_2 0x80
#define LSIO_PINMUX_3 0x84

/* horizon lsio pin id */
#define LSIO_UART0_RX 		0
#define LSIO_UART0_TX 		1
#define LSIO_UART0_CTS 		2
#define LSIO_UART0_RTS 		3
#define LSIO_UART1_RX 		4
#define LSIO_UART1_TX 		5
#define LSIO_UART1_CTS 		6
#define LSIO_UART1_RTS 		7
#define LSIO_UART2_RX 		8
#define LSIO_UART2_TX 		9
#define LSIO_UART3_RX 		10
#define LSIO_UART3_TX 		11
#define LSIO_UART4_RX 		12
#define LSIO_UART4_TX 		13
#define LSIO_SPI0_SCLK 		14
#define LSIO_SPI1_SSN_1 	15
#define LSIO_SPI1_SCLK 		16
#define LSIO_SPI1_SSN 		17
#define LSIO_SPI1_MISO 		18
#define LSIO_SPI1_MOSI 		19
#define LSIO_SPI2_SCLK 		20
#define LSIO_SPI2_SSN 		21
#define LSIO_SPI2_MISO 		22
#define LSIO_SPI2_MOSI 		23
#define LSIO_SPI3_SCLK 		24
#define LSIO_SPI3_SSN 		25
#define LSIO_SPI3_MISO 		26
#define LSIO_SPI3_MOSI 		27
#define LSIO_SPI4_SCLK 		28
#define LSIO_SPI4_SSN 		29
#define LSIO_SPI4_MISO 		30
#define LSIO_SPI4_MOSI 		31
#define LSIO_SPI5_SCLK 		32
#define LSIO_SPI5_SSN 		33
#define LSIO_SPI5_MISO 		34
#define LSIO_SPI5_MOSI 		35
#define LSIO_SPI0_SSN 		36
#define LSIO_SPI0_MISO 		37
#define LSIO_SPI0_MOSI 		38
#define LSIO_I2C0_SCL		39
#define LSIO_I2C0_SDA		40
#define LSIO_I2C1_SCL		41
#define LSIO_I2C1_SDA		42
#define LSIO_I2C2_SCL		43
#define LSIO_I2C2_SDA		44
#define LSIO_I2C3_SCL		45
#define LSIO_I2C3_SDA		46
#define LSIO_I2C4_SCL		47
#define LSIO_I2C4_SDA		48

#endif /* __HORIZON_LSIO_PINFUNC_H */
