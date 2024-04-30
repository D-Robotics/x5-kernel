/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __DC_8000_NANO_TYPE_H__
#define __DC_8000_NANO_TYPE_H__

/** @addtogroup DC
 *  vs dc 8x00 data types.
 *  @ingroup DRIVER
 *
 *  @{
 */

/**
 * display controller dc8000 nano family version
 */
enum dc_8000_nano_chip_ver {
	DC_8000_NANO_VER_5544,
};

/**
 * dc8000 nano color format
 */
enum dc_8000_nano_color_format {
	DC_8000_NANO_FORMAT_YUY2 = 0,
	DC_8000_NANO_FORMAT_A4R4G4B4,
	DC_8000_NANO_FORMAT_A1R5G5B5,
	DC_8000_NANO_FORMAT_R5G6B5,
	DC_8000_NANO_FORMAT_A8R8G8B8,
	DC_8000_NANO_FORMAT_R8G8B8,
	DC_8000_NANO_FORMAT_NV12 = 7,
};

/**
 * dc8000 nano tile mode
 */
enum dc_8000_nano_tile_mode {
	DC_8000_NANO_LINEAR,
	DC_8000_NANO_TILE_4X4,
	DC_8000_NANO_TILE_8X8,
};

/**
 * dc8000 nano write-back format
 */
enum dc_8000_nano_wb_format {
	DC_8000_NANO_WB_FORMAT_X8R8G8B8,
};

/**
 * dc8000 nano color gamut
 */
enum dc_8000_nano_color_gamut {
	DC_8000_NANO_COLOR_GAMUT_601 = 0,
	DC_8000_NANO_COLOR_GAMUT_709 = 1,
};

/** @} */

#endif /* __DC_8000_NANO_TYPE_H__ */
