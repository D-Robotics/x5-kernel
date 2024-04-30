/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __DC_8000_NANO_H__
#define __DC_8000_NANO_H__

/** @addtogroup DC
 *  vs dc 8x00 data types.
 *  @ingroup DRIVER
 *
 *  @{
 */

#define __vsFIELDSTART(reg_field) \
	(0 ? reg_field)

#define __vsFIELDEND(reg_field) \
	(1 ? reg_field)

#define VS_SET(reg, field, value) (((u32)(value)) << __vsFIELDSTART(reg##_##field))

#define VS_MASK(reg, field) \
	((u32)(GENMASK(__vsFIELDEND(reg##_##field), __vsFIELDSTART(reg##_##field))))

#define VS_SET_FIELD(data, reg, field, value) \
	(((data) & ~VS_MASK(reg, field)) | VS_SET(reg, field, (value)))

extern const struct dc_hw_funcs dc_8000_nano_funcs;

/** @} */

#endif /* __DC_8000_NANO_H__ */
