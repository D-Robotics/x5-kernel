/*
 * Horizon Robotics
 *
 *  Copyright (C) 2020 Horizon Robotics Inc.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef SOCINFO_H
#define SOCINFO_H

/* ddr manufacture */
#define DDR_MANU_HYNIX		0x1
#define DDR_MANU_MICRON		0x2
#define DDR_MANU_SAMSUNG	0x3
#define PIN_DDR_TYPE_SEL(x)	((x) & 0x1)

/* som type */
#define SOM_TYPE_X3		0x1
#define SOM_TYPE_J3		0x2

/* base board type */
#define BASE_BOARD_X3_DVB		0x1
#define BASE_BOARD_J3_DVB		0x2
#define BASE_BOARD_CVB			0x3
#define BASE_BOARD_X3_SDB		0x4

/* ddr type */
#define DDR_TYPE_LPDDR4		0x1
#define DDR_TYPE_LPDDR4X	0x2
#define DDR_TYPE_DDR4		0x3
#define DDR_TYPE_DDR3L		0x4

/* ddr frequency */
#define DDR_FREQC_667	0x1
#define DDR_FREQC_1600	0x2
#define DDR_FREQC_2133	0x3
#define DDR_FREQC_2666	0x4
#define DDR_FREQC_3200	0x5
#define DDR_FREQC_3733	0x6
#define DDR_FREQC_4266	0x7
#define DDR_FREQC_1866  0x8
#define DDR_FREQC_2400	0x9
#define DDR_FREQC_100	0xa
#define DDR_FREQC_3600	0xb
#define DDR_FREQC_2640	0xe

/* ddr capacity */
#define DDR_CAPACITY_1G		0x1
#define DDR_CAPACITY_2G		0x2
#define DDR_CAPACITY_4G		0x3

#define SEC_FLAG_REG_ADDR       0xA6000214
#define SEC_FLAG_SIZE           4
#define BOOT_COUNT_REG_ADDR     0xA600025c
#define BOOT_COUNT_SIZE         4

#define DDR_SIZE_BIT_OFFSET (4)
#define DDR_SIZE_MASK       (0xfL << DDR_SIZE_BIT_OFFSET)
#define DR_DDR_SIZE(x)      ((x & DDR_SIZE_MASK) >> DDR_SIZE_BIT_OFFSET)

#define DDR_TYPE_BIT_OFFSET (8)
#define DDR_TYPE_MASK       (0x3L << DDR_TYPE_BIT_OFFSET)
#define DR_DDR_TYPE(x)      ((x & DDR_TYPE_MASK) >> DDR_TYPE_BIT_OFFSET)

#define DDR_ECC_BIT_OFFSET (10)
#define DDR_ECC_MASK       (0x1L << DDR_ECC_BIT_OFFSET)
#define DR_DDR_ECC(x)      ((x & DDR_ECC_MASK) & DDR_ECC_BIT_OFFSET)

#define DDR_FREQ_BIT_OFFSET (12)
#define DDR_FREQ_MASK       (0xfL << DDR_FREQ_BIT_OFFSET)
#define DR_DDR_FREQ(x)      ((x & DDR_FREQ_MASK) >> DDR_FREQ_BIT_OFFSET)

#define DDR_RANK_BIT_OFFSET (16)
#define DDR_RANK_MASK       (0x3L << DDR_RANK_BIT_OFFSET)
#define DR_DDR_RANK(x)      ((x & DDR_RANK_MASK) >> DDR_RANK_BIT_OFFSET)

#define DDR_VENDOR_BIT_OFFSET (20)
#define DDR_VENDOR_MASK       (0xffl << DDR_VENDOR_BIT_OFFSET)
#define DR_DDR_VENDOR(x)      ((x & DDR_VENDOR_MASK) >> DDR_VENDOR_BIT_OFFSET)

#define DDR_INFO_VALID_BIT_OFFSET (31)
#define DDR_INFO_VALID_MASK       (0x1L << DDR_INFO_VALID_BIT_OFFSET)
enum hobot_board {
	X2_SVB = 100,
	J2_SVB = 200,
	J2SOM = 201,
	X2SOM1V8 = 102,
	X2SOM3V3 = 103,
	X2_DEV = 101,
	X2_SOMFULL = 104,
	X2_96BOARD = 105,
	X2_DEV512M = 106,
	X2_XIAOMI = 107,
	J2_Mono = 202,
	J2_DEV = 203,
	J2_SK = 204,
	J2_SAM = 205,
	J2_QuadJ2A = 301,
	J2_QuadJ2B,
	J2_QuadJ2C,
	J2_QuadJ2D,
	J2_Quad = 300,
	J2_mm = 400,
        J2_mm_s202 = 401,
	Unknown = 1000,
};

struct hobot_board_info {
	enum hobot_board generic_board_type;
	char *board_id_string;
};

#endif
