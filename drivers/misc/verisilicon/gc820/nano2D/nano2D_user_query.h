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

#ifndef _nano2D_user_query_h_
#define _nano2D_user_query_h_

#ifdef __cplusplus
extern "C" {
#endif

#include "nano2D.h"
#include "nano2D_user_hardware.h"

typedef struct FormatInfo {
    n2d_const_string       formatName;
    n2d_buffer_format_t    format;
    n2d_formattype_t           formatType;
    n2d_uint8_t            bpp;
    n2d_uint8_t            planeNum;
    n2d_uint8_t            plane_bpp[MAX_PLANE];
    n2d_uint32_t           aWidthPixel;
    n2d_uint32_t           aHeightPixel;
    n2d_uint32_t           aStrideByte[MAX_PLANE];
    n2d_uint32_t           aAddressByte[MAX_PLANE];
}FormatInfo_t;

typedef struct FormatInfoArray {
    FormatInfo_t    *formatInfo;
    n2d_uint32_t    count;
}FormatInfoArray_t;

typedef struct AlignInfo {
    n2d_uint32_t           aWidthPixel;
    n2d_uint32_t           aHeightPixel;
    n2d_uint32_t           aStrideByte;
    n2d_uint32_t           aAddressByte[MAX_PLANE];
}AlignInfo_t;


typedef struct FormatTilingAlignInfo {

    n2d_buffer_format_t    format;
    n2d_const_string       formatName;
    AlignInfo_t            alignInfo;
}FormatTilingAlignInfo_t;

typedef struct FormatTilingAlignInfoArray {
    n2d_tiling_t                        tiling;
    FormatTilingAlignInfo_t             *formatTilingAlignInfo;
}FormatTilingAlignInfoArray_t;


n2d_error_t gcGetFormatInfo (
    IN n2d_buffer_format_t  Format,
    OUT FormatInfo_t        **FormatInfo
    );

#ifdef __cplusplus
}
#endif


#endif /* _nano2D_user_query_h_ */
