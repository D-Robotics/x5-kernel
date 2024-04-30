/****************************************************************************
*
*    Copyright 2012 - 2022 Vivante Corporation, Santa Clara, California.
*    All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

#ifndef _nano2D_user_hardware_source_h_
#define _nano2D_user_hardware_source_h_

#ifdef __cplusplus
extern "C" {
#endif

#include "nano2D_option.h"
#include "nano2D_base.h"
#include "nano2D_dispatch.h"
#include "nano2D_user_buffer.h"

n2d_error_t gcSetMonochromeSource(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_uint8_t MonoTransparency,
    IN n2d_monopack_t DataPack,
    IN n2d_bool_t CoordRelative,
    IN n2d_uint32_t FgColor32,
    IN n2d_uint32_t BgColor32,
    IN n2d_bool_t ColorConvert,
    IN n2d_buffer_format_t SrctFormat,
    IN n2d_buffer_format_t DstFormat,
    IN n2d_bool_t Stream,
    IN n2d_uint32_t Transparency
    );

n2d_error_t gcSetMaskedSource(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO *Surface,
    IN n2d_bool_t CoordRelative,
    IN n2d_monopack_t MaskPack,
    IN n2d_uint32_t Transparency
    );

n2d_error_t gcSetSourceColorKeyRange(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t ColorLow,
    IN n2d_uint32_t ColorHigh,
    IN n2d_buffer_format_t SrcFormat
    );

n2d_error_t gcSetSourceGlobalColor(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t Color
    );

n2d_error_t gcTranslateSourceTransparency(
    IN n2d_transparency_t APIValue,
    OUT n2d_uint32_t * HwValue
    );

n2d_error_t gcTranslateSourceFormat(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_buffer_format_t APIValue,
    OUT n2d_uint32_t* HwValue,
    OUT n2d_uint32_t* HwValueEx,
    OUT n2d_uint32_t* HwSwizzleValue
    );

n2d_error_t gcSetSource(
    IN n2d_user_hardware_t * Hardware,
    IN gcsRECT_PTR SrcRect
    );

n2d_error_t gcSetMultiSourceTilingConfig(
    IN n2d_user_hardware_t  *Hardware,
    IN gcsSURF_INFO_PTR     Surface,
    OUT n2d_uint32_t        *Data
    );

n2d_error_t gcSetColorSource(
    IN n2d_user_hardware_t * Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_bool_t CoordRelative,
    IN n2d_uint32_t Transparency,
    IN n2d_csc_mode_t Mode,
    IN n2d_bool_t DeGamma,
    IN n2d_bool_t Filter
    );

n2d_error_t gcSetMultiSource(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t RegGroupIndex,
    IN n2d_uint32_t SrcIndex,
    IN gcs2D_State_PTR State,
    IN n2d_bool_t MultiDstRect
    );

#ifdef __cplusplus
}
#endif

#endif
