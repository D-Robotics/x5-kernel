/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunrise5 pinctrl aon subsystem definitions
 *
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef __HORIZON_AON_PINFUNC_H
#define __HORIZON_AON_PINFUNC_H
/* Horizon AON pin ctrl regs */
#define AON_PINCTRL_0	   0x0
#define AON_PINCTRL_1	   0x4
#define AON_PINCTRL_2	   0x8
#define AON_PINCTRL_3	   0xc
#define AON_PINCTRL_4	   0x10
#define AON_PINCTRL_5	   0x14
#define AON_PINMUX_0	   0x8

/* horizon AON pin id */
#define AON_GPIO0_PIN0 0
#define AON_GPIO0_PIN1 1
#define AON_GPIO0_PIN2 2
#define AON_GPIO0_PIN3 3
#define AON_GPIO0_PIN4 4
#define AON_ENV_VDD    5
#define AON_ENV_CNN0   6
#define AON_ENV_CNN1   7
#define AON_PMIC_EN    8
#define AON_HW_RESETN  9
#define AON_RESETN_OUT 10
#define AON_IRQ_OUT    11
#define AON_XTAL_24M   12
#endif /* __HORIZON_AON_PINFUNC_H */
