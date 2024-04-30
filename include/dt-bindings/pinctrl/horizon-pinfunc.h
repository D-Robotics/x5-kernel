/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunrise5 pinctrl core definitions
 *
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef __HORIZON_PINFUNC_H
#define __HORIZON_PINFUNC_H

#define INVALID_PINMUX    0x0

/* The macro defines the mux mode of horizon pinctrl */
#define MUX_ALT0 0x0
#define MUX_ALT1 0x1
#define MUX_ALT2 0x2
#define MUX_ALT3 0x3

/* The macro defines pinmux bit offset in horizon pinctrl */
#define BIT_OFFSET0 0
#define BIT_OFFSET2 2
#define BIT_OFFSET4 4
#define BIT_OFFSET6 6
#define BIT_OFFSET8 8
#define BIT_OFFSET10 10
#define BIT_OFFSET12 12
#define BIT_OFFSET14 14
#define BIT_OFFSET16 16
#define BIT_OFFSET18 18
#define BIT_OFFSET20 20
#define BIT_OFFSET22 22
#define BIT_OFFSET24 24
#define BIT_OFFSET26 26
#define BIT_OFFSET28 28
#define BIT_OFFSET30 30

#define  HORIZON_IO_PAD_VOLTAGE_3V3 0
#define  HORIZON_IO_PAD_VOLTAGE_1V8 1
#define  HORIZON_IO_PAD_VOLTAGE_IP_CTRL 2
#define  HORIZON_IO_PAD_CTRL_VOLTAGE_1V8 3
#define  HORIZON_IO_PAD_CTRL_VOLTAGE_3V3 4
#endif /* __HORIZON_PINFUNC_H */
