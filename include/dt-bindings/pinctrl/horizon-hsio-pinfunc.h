/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunrise5 pinctrl high-speed IO subsystem definitions
 *
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef __HORIZON_HSIO_PINFUNC_H
#define __HORIZON_HSIO_PINFUNC_H

/* Horizon hsio pin ctrl regs */
#define HSIO_PINCTRL_0 0x18
#define HSIO_PINCTRL_1 0x1c
#define HSIO_PINCTRL_2 0x20
#define HSIO_PINCTRL_3 0x24
#define HSIO_PINCTRL_4 0x28
#define HSIO_PINCTRL_5 0x2c
#define HSIO_PINCTRL_6 0x30
#define HSIO_PINCTRL_7 0x34
#define HSIO_PINCTRL_8 0x38
#define HSIO_PINCTRL_9 0x3c
#define HSIO_PINCTRL_10 0x40
#define HSIO_PINCTRL_11 0x44
#define HSIO_PINCTRL_12 0x48
#define HSIO_PINCTRL_13 0x4c
#define HSIO_PINCTRL_14 0x50
#define HSIO_PINCTRL_15 0x54

#define HSIO_PINMUX_0 0x58
#define HSIO_PINMUX_1 0x5c
#define HSIO_PINMUX_2 0x60
#define HSIO_PINMUX_3 0xb8

/* horizon hsio pin id */
#define HSIO_ENET_MDC    0
#define HSIO_ENET_MDIO    1
#define HSIO_ENET_TXD_0    2
#define HSIO_ENET_TXD_1    3
#define HSIO_ENET_TXD_2    4
#define HSIO_ENET_TXD_3    5
#define HSIO_ENET_TXEN    6
#define HSIO_ENET_TX_CLK    7
#define HSIO_ENET_RX_CLK    8
#define HSIO_ENET_RXD_0    9
#define HSIO_ENET_RXD_1    10
#define HSIO_ENET_RXD_2    11
#define HSIO_ENET_RXD_3    12
#define HSIO_ENET_RXDV    13
#define HSIO_ENET_PHY_CLK    14
#define HSIO_SD_WP    15
#define HSIO_SD_CLK    16
#define HSIO_SD_CMD    17
#define HSIO_SD_CDN    18
#define HSIO_SD_DATA0    19
#define HSIO_SD_DATA1    20
#define HSIO_SD_DATA2    21
#define HSIO_SD_DATA3    22
#define HSIO_SDIO_WP    23
#define HSIO_SDIO_CLK    24
#define HSIO_SDIO_CMD    25
#define HSIO_SDIO_CDN    26
#define HSIO_SDIO_DATA0    27
#define HSIO_SDIO_DATA1    28
#define HSIO_SDIO_DATA2    29
#define HSIO_SDIO_DATA3    30
#define HSIO_QSPI_SSN0    31
#define HSIO_QSPI_SSN1    32
#define HSIO_QSPI_SCLK    33
#define HSIO_QSPI_DATA0    34
#define HSIO_QSPI_DATA1    35
#define HSIO_QSPI_DATA2    36
#define HSIO_QSPI_DATA3    37
#define HSIO_EMMC_CLK    38
#define HSIO_EMMC_CMD    39
#define HSIO_EMMC_DATA0    40
#define HSIO_EMMC_DATA1    41
#define HSIO_EMMC_DATA2    42
#define HSIO_EMMC_DATA3    43
#define HSIO_EMMC_DATA4    44
#define HSIO_EMMC_DATA5    45
#define HSIO_EMMC_DATA6    46
#define HSIO_EMMC_DATA7    47
#define HSIO_EMMC_RSTN    48

#endif /* __HORIZON_HSIO_PINFUNC_H */
