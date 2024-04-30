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

#include "nano2D.h"
#include "nano2D_user_hardware.h"
#include "nano2D_user_hardware_dec.h"
#include "nano2D_user_hardware_target.h"
#include "nano2D_user_query.h"
#include "series_common.h"

/*******************************************************************************
**
**  gcSetSourceTileStatus
**
**  Configure the source tile status.
**
**  INPUT:
**
**      n2d_user_hardware_t Hardware
**          Pointer to an n2d_user_hardware_t object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer to the source surface object.
**
*/
static n2d_error_t gcSetSourceTileStatus(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_uint32_t RegGroupIndex,
    IN gcsSURF_INFO_PTR Source
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t regOffset = RegGroupIndex << 2, config;

    if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION] &&
        gcmHASCOMPRESSION(Source))
    {
        if (Source->cacheMode == N2D_CACHE_256)
        {
            config = gcmSETFIELDVALUE(0, GCREG_DE_SRC_TILE_STATUS_CONFIG, COMPRESSION_SIZE, Size256B);
        }
        else if (Source->tiling == N2D_LINEAR && (Source->format == N2D_NV12 || Source->format == N2D_NV21 ||
            Source->format == N2D_P010_LSB || Source->format == N2D_P010_MSB))
        {
            config = gcmSETFIELDVALUE(0, GCREG_DE_SRC_TILE_STATUS_CONFIG, COMPRESSION_SIZE, Size256B);
        }
        else
        {
            config = GCREG_DE_SRC_TILE_STATUS_CONFIG_ResetValue;
        }

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GCREG_DE_SRC_TILE_STATUS_CONFIG_Address + regOffset,
            config
            ));

        return N2D_SUCCESS;
    }
    else
    {
         return N2D_SUCCESS;
    }

on_error:
    /* Return the status. */
    return error;
}

static n2d_error_t gcTranslateMonoPack(
    IN n2d_monopack_t APIValue,
    OUT n2d_uint32_t * HwValue
    )
{
    /* Dispatch on monochrome packing. */
    switch (APIValue)
    {
    case N2D_PACKED8:
        *HwValue = AQDE_SRC_CONFIG_PACK_PACKED8;
        break;

    case N2D_PACKED16:
        *HwValue = AQDE_SRC_CONFIG_PACK_PACKED16;
        break;

    case N2D_PACKED32:
        *HwValue = AQDE_SRC_CONFIG_PACK_PACKED32;
        break;

    case N2D_UNPACKED:
        *HwValue = AQDE_SRC_CONFIG_PACK_UNPACKED;
        break;

    default:
        /* Not supported. */
        return N2D_NOT_SUPPORTED;
    }

    /* Success. */
    return N2D_SUCCESS;
}

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
    )
{
    n2d_error_t error;
    n2d_uint32_t datapack, config;

    N2D_ON_ERROR(gcTranslateMonoPack(DataPack, &datapack));

    if (ColorConvert == N2D_FALSE)
    {
        N2D_ON_ERROR(gcColorConvertToARGB8(
              DstFormat,
              1,
              &FgColor32,
              &FgColor32
              ));

        N2D_ON_ERROR(gcColorConvertToARGB8(
              DstFormat,
              1,
              &BgColor32,
              &BgColor32
              ));
    }

    /* LoadState(AQDE_SRC_ADDRESS_Address, 1), 0. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_SRC_ADDRESS_Address, 0
        ));

    /* Setup source configuration. transparency field is obsolete for PE 2.0. */
    config
        = (Stream ?
          gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, LOCATION,      STREAM)
        : gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, LOCATION,      MEMORY))
        | gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, SOURCE_FORMAT, MONOCHROME)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, PACK,          datapack)
        | gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, COLOR_CONVERT, ON)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, TRANSPARENCY,  Transparency)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, SRC_RELATIVE,  CoordRelative)
        | (MonoTransparency
            ? gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, MONO_TRANSPARENCY, FOREGROUND)
            : gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, MONO_TRANSPARENCY, BACKGROUND))
        |((SrctFormat == N2D_P010_LSB || SrctFormat == N2D_I010_LSB) ?
            gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, P010_BITS, P010_LSB) : 0);

    /* LoadState(AQDE_SRC_CONFIG, 1), config. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_SRC_CONFIG_Address, config
        ));

    /* LoadState(AQDE_SRC_COLOR_BG, 1), BgColor. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_SRC_COLOR_BG_Address, BgColor32
        ));

    /* LoadState(AQDE_SRC_COLOR_FG, 1), FgColor. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_SRC_COLOR_FG_Address, FgColor32
        ));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

static n2d_error_t gcTranslateSourceRotation(
    IN n2d_orientation_t APIValue,
    OUT n2d_uint32_t * HwValue
    )
{
    /* Dispatch on transparency. */
    switch (APIValue)
    {
    case N2D_0:
        *HwValue = AQDE_ROT_ANGLE_SRC_ROT0;
        break;

    case N2D_90:
        *HwValue = AQDE_ROT_ANGLE_SRC_ROT90;
        break;

    case N2D_180:
        *HwValue = AQDE_ROT_ANGLE_SRC_ROT180;
        break;

    case N2D_270:
        *HwValue = AQDE_ROT_ANGLE_SRC_ROT270;
        break;

    case N2D_FLIP_X:
        *HwValue = AQDE_ROT_ANGLE_SRC_FLIP_X;
        break;

    case N2D_FLIP_Y:
        *HwValue = AQDE_ROT_ANGLE_SRC_FLIP_Y;
        break;

    default:
        /* Not supported. */
        return N2D_NOT_SUPPORTED;
    }

    /* Success. */
    return N2D_SUCCESS;
}

static n2d_error_t gcSetSourceExtensionFormat(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t format,
    IN n2d_uint32_t formatEx
    )
{
    n2d_error_t error = N2D_SUCCESS;

    if (format == AQDE_SRC_CONFIG_SOURCE_FORMAT_EXTENSION)
    {
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQDE_SRC_CONFIG_EX_Address,
            gcmSETFIELD(0, AQDE_SRC_CONFIG_EX, EXTRA_SOURCE_FORMAT, formatEx)
            ));
    }

on_error:
    return error;
}

n2d_error_t gcTranslateSourceFormat(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_buffer_format_t APIValue,
    OUT n2d_uint32_t* HwValue,
    OUT n2d_uint32_t* HwValueEx,
    OUT n2d_uint32_t* HwSwizzleValue
    )
{
    n2d_error_t error;
    n2d_uint32_t swizzle_argb, swizzle_rgba, swizzle_abgr, swizzle_bgra;
    n2d_uint32_t swizzle_uv, swizzle_vu, hwvalueex;

    swizzle_argb = AQDE_SRC_CONFIG_SWIZZLE_ARGB;
    swizzle_rgba = AQDE_SRC_CONFIG_SWIZZLE_RGBA;
    swizzle_abgr = AQDE_SRC_CONFIG_SWIZZLE_ABGR;
    swizzle_bgra = AQDE_SRC_CONFIG_SWIZZLE_BGRA;

    swizzle_uv = AQPE_CONTROL_UV_SWIZZLE_UV;
    swizzle_vu = AQPE_CONTROL_UV_SWIZZLE_VU;

    hwvalueex = GCREG_DE_SRC_CONFIG_EX_EXTRA_SOURCE_FORMAT_RGB_F16F16F16_PLANAR;

    /* Default values. */
    *HwSwizzleValue = swizzle_argb;

    /* Dispatch on format. */
    switch (APIValue)
    {
    case N2D_RGBA8888:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A8R8G8B8;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case N2D_RGBX8888:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X8R8G8B8;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A8R8G8B8;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case N2D_R10G10B10A2:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_A2R10G10B10;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case N2D_R5G5B5A1:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A1R5G5B5;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case N2D_R5G5B5X1:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X1R5G5B5;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A1R5G5B5;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case N2D_RGBA4444:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A4R4G4B4;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case N2D_RGBX4444:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X4R4G4B4;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A4R4G4B4;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case N2D_RGB565:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_R5G6B5;
        break;

    case N2D_BGRA8888:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A8R8G8B8;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case N2D_BGRX8888:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X8R8G8B8;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A8R8G8B8;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case N2D_B10G10R10A2:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_A2R10G10B10;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case N2D_B5G5R5A1:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A1R5G5B5;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case N2D_B5G5R5X1:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X1R5G5B5;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A1R5G5B5;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case N2D_BGRA4444:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A4R4G4B4;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case N2D_BGRX4444:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X4R4G4B4;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A4R4G4B4;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case N2D_BGR565:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_R5G6B5;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_ABGR8888:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A8R8G8B8;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_XBGR8888:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X8R8G8B8;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A8R8G8B8;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_A2B10G10R10:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_A2R10G10B10;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_A1B5G5R5:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A1R5G5B5;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_X1B5G5R5:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X1R5G5B5;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A1R5G5B5;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_ABGR4444:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A4R4G4B4;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_XBGR4444:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X4R4G4B4;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A4R4G4B4;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_ARGB8888:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A8R8G8B8;
        break;

    case N2D_XRGB8888:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X8R8G8B8;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A8R8G8B8;
        break;

    case N2D_A2R10G10B10:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_A2R10G10B10;
        break;

    case N2D_A1R5G5B5:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A1R5G5B5;
        break;

    case N2D_X1R5G5B5:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X1R5G5B5;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A1R5G5B5;
        break;

    case N2D_ARGB4444:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_A4R4G4B4;
        break;

    case N2D_XRGB4444:
        if (Hardware->enableXRGB)
            *HwValue = AQDE_SRC_CONFIG_FORMAT_X4R4G4B4;
        else
            *HwValue = AQDE_SRC_CONFIG_FORMAT_A4R4G4B4;
        break;

    case N2D_A8:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_A8;
        break;

    case N2D_INDEX8:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_INDEX8;
        break;

    case N2D_RGB888_PLANAR:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_RGB888_PLANAR;
        break;

    case N2D_YUYV:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_YUY2;
        *HwSwizzleValue = swizzle_uv;
        break;

    case N2D_UYVY:
        *HwValue = AQDE_SRC_CONFIG_FORMAT_UYVY;
        *HwSwizzleValue = swizzle_uv;
        break;

    case N2D_YV12:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_YV12;
        *HwSwizzleValue = swizzle_vu;
        break;

    case N2D_I420:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_YV12;
        *HwSwizzleValue = swizzle_uv;
        break;

    case N2D_NV12:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_NV12;
        *HwSwizzleValue = swizzle_uv;
        break;

    case N2D_NV21:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_NV12;
        *HwSwizzleValue = swizzle_vu;
        break;

    case N2D_NV16:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_NV16;
        *HwSwizzleValue = swizzle_uv;
        break;

    case N2D_NV61:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_NV16;
        *HwSwizzleValue = swizzle_vu;
        break;

    case N2D_NV12_10BIT:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_NV12_10BIT;
        *HwSwizzleValue = swizzle_uv;
        break;

    case N2D_NV21_10BIT:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_NV12_10BIT;
        *HwSwizzleValue = swizzle_vu;
        break;

    case N2D_NV16_10BIT:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_NV16_10BIT;
        *HwSwizzleValue = swizzle_uv;
        break;

    case N2D_NV61_10BIT:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_NV16_10BIT;
        *HwSwizzleValue = swizzle_vu;
        break;

    case N2D_P010_MSB:
    case N2D_P010_LSB:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_P010;
        *HwSwizzleValue = swizzle_uv;
        break;

    case N2D_I010:
    case N2D_I010_LSB:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_I010;
        *HwSwizzleValue = swizzle_uv;
        break;

    case N2D_AYUV:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_AYUV;
        *HwSwizzleValue = swizzle_uv;
        break;

    case N2D_GRAY8:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_EXTENSION;
        hwvalueex = GCREG_DE_SRC_CONFIG_EX_EXTRA_SOURCE_FORMAT_GRAY_U8;
        break;

    case N2D_RGB888:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_RGB888_PACKED;
        break;

    default:
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    *HwValueEx = hwvalueex;

    /* Success. */
    return N2D_SUCCESS;

on_error:
    /* Return the error. */
    return error;
}


/*******************************************************************************
**
**  gcSetMaskedSource
**
**  Configure masked color source.
**
**  INPUT:
**
**      n2d_user_hardware_t *Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcoSURF Surface
**          Pointer to the source surface object.
**
**      n2d_bool_t CoordRelative
**          If gcvFALSE, the source origin represents absolute pixel coordinate
**          within the source surface.  If gcvTRUE, the source origin represents
**          the offset from the destination origin.
**
**      n2d_monopack_t MaskPack
**          Determines how many horizontal pixels are there per each 32-bit
**          chunk of monochrome mask.  For example, if set to gcvSURF_PACKED8,
**          each 32-bit chunk is 8-pixel wide, which also means that it defines
**          4 vertical lines of pixel mask.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetMaskedSource(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO *Surface,
    IN n2d_bool_t CoordRelative,
    IN n2d_monopack_t MaskPack,
    IN n2d_uint32_t Transparency
    )
{
    n2d_error_t error;
    n2d_uint32_t format, swizzle, maskpack, formatEx;
    n2d_uint32_t data[4];
    n2d_uint32_t srcRot = 0;
    n2d_uint32_t value;
    /* Determine color swizzle. */
    n2d_uint32_t rgbaSwizzle;
    FormatInfo_t *FormatInfo = N2D_NULL;

    /* Convert the format. */
    N2D_ON_ERROR(gcTranslateSourceFormat(Hardware, Surface->format, &format, &formatEx, &swizzle));

    /* Convert the data packing. */
    N2D_ON_ERROR(gcTranslateMonoPack(MaskPack, &maskpack));
    N2D_ON_ERROR(gcGetFormatInfo(Surface->format,&FormatInfo));

    /* Determine color swizzle. */
    if (N2D_FORMARTTYPE_YUV == FormatInfo->formatType)
    {
        rgbaSwizzle = AQDE_SRC_CONFIG_SWIZZLE_ARGB;
    }
    else
    {
        rgbaSwizzle = swizzle;
    }

    data[0] = Surface->gpuAddress[0];

    /* Dump the memory. */
/*
    N2D_DUMP_SURFACE(Surface->memory, Surface->gpuAddress[0], size[0], N2D_TRUE, Hardware);
*/
    /* AQDE_SRC_STRIDE_Address */
    data[1]
        = Surface->stride[0];

    /* AQDE_SRC_ROTATION_CONFIG_Address */
    data[2]
        = gcmSETFIELD     (0, AQDE_SRC_ROTATION_CONFIG, WIDTH,    Surface->alignedWidth)
        | gcmSETFIELDVALUE(0, AQDE_SRC_ROTATION_CONFIG, ROTATION, NORMAL);

    /* AQDE_SRC_CONFIG_Address. transparency field is obsolete for PE 2.0. */
    data[3]
        = gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, LOCATION,      STREAM)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, TRANSPARENCY,  Transparency)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, FORMAT,        format)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, SOURCE_FORMAT, format)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, SWIZZLE,       rgbaSwizzle)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, PACK,          maskpack)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, SRC_RELATIVE,  CoordRelative);

    /* Load source states. */
    N2D_ON_ERROR(gcWriteRegWithLoadState(
        Hardware,
        AQDE_SRC_ADDRESS_Address, 4,
        data
        ));

    N2D_ON_ERROR(gcTranslateSourceRotation(Surface->rotation, &srcRot));

    /* Flush the 2D pipe before writing to the rotation register. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQ_FLUSH_Address,
        gcmSETFIELDVALUE(0, AQ_FLUSH, PE2D_CACHE, ENABLE)
        ));

    /* Load source height. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_SRC_ROTATION_HEIGHT_Address,
        gcmSETFIELD(0, AQDE_SRC_ROTATION_HEIGHT, HEIGHT, Surface->alignedHeight)
        ));

    /* Enable src mask. */
    value = gcmSETFIELDVALUE(~0, AQDE_ROT_ANGLE, MASK_SRC, ENABLED);

    /* Set src rotation. */
    value = gcmSETFIELD(value, AQDE_ROT_ANGLE, SRC, srcRot);

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware, AQDE_ROT_ANGLE_Address, value
        ));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcSetSourceColorKeyRange(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t ColorLow,
    IN n2d_uint32_t ColorHigh,
    IN n2d_buffer_format_t SrcFormat
    )
{
    n2d_error_t error;

    if (SrcFormat == N2D_INDEX8)
    {
        ColorLow = (ColorLow & 0xFF) << 24;
        ColorHigh = (ColorHigh & 0xFF) << 24;
    }

    /* LoadState source color key. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_SRC_COLOR_BG_Address,
        ColorLow
        ));

    /* LoadState source color key. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_SRC_COLOR_KEY_HIGH_Address,
        ColorHigh
        ));

on_error:
    return error;
}

n2d_error_t gcSetSourceGlobalColor(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t Color
    )
{
    n2d_error_t error = N2D_SUCCESS;

    /* LoadState global color value. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_GLOBAL_SRC_COLOR_Address,
        Color
        ));

on_error:
    return error;
}

n2d_error_t gcTranslateSourceTransparency(
    IN n2d_transparency_t APIValue,
    OUT n2d_uint32_t * HwValue
    )
{
    /* Dispatch on transparency. */
    switch (APIValue)
    {
#ifdef AQPE_TRANSPARENCY_Address
    case N2D_OPAQUE:
        *HwValue = AQPE_TRANSPARENCY_SOURCE_OPAQUE;
        break;

    case N2D_KEYED:
        *HwValue = AQPE_TRANSPARENCY_SOURCE_KEY;
        break;

    case N2D_MASKED:
        *HwValue = AQPE_TRANSPARENCY_SOURCE_MASK;
        break;
#endif

    default:
        /* Not supported. */
        return N2D_NOT_SUPPORTED;
    }

    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetSource(
    IN n2d_user_hardware_t * Hardware,
    IN gcsRECT_PTR SrcRect
    )
{
    n2d_error_t error;
    n2d_uint32_t data[2];

    /* AQDE_SRC_ORIGIN_Address */
    data[0]
        = gcmSETFIELD(0, AQDE_SRC_ORIGIN, X, SrcRect->left)
        | gcmSETFIELD(0, AQDE_SRC_ORIGIN, Y, SrcRect->top);

    /* AQDE_SRC_SIZE_Address */
    data[1]
        = gcmSETFIELD(0, AQDE_SRC_SIZE, X, SrcRect->right  - SrcRect->left)
        | gcmSETFIELD(0, AQDE_SRC_SIZE, Y, SrcRect->bottom - SrcRect->top);

    N2D_ON_ERROR(gcWriteRegWithLoadState(
        Hardware,
        AQDE_SRC_ORIGIN_Address, 2,
        data
        ));

on_error:
    /* Return the error. */

    return error;
}

static n2d_error_t gcSetSourceTilingConfigDefault(
    IN n2d_user_hardware_t  *Hardware,
    IN gcsSURF_INFO_PTR     Surface,
    OUT n2d_uint32_t        *SrcConfig,
    OUT n2d_uint32_t        *SrcConfigEx)
{
    n2d_error_t error;
    n2d_uint32_t srcConfig = 0,srcConfigEx = 0;

    if (N2D_NULL == SrcConfig || N2D_NULL == SrcConfigEx) {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    srcConfig = gcmSETFIELDVALUE(srcConfig,
                               AQDE_SRC_CONFIG,
                               TILED,
                               ENABLED
                               );

    switch (Surface->tiling) {
        case N2D_LINEAR:
            srcConfig = gcmSETFIELDVALUE(srcConfig,
                                       AQDE_SRC_CONFIG,
                                       TILED,
                                       DISABLED);

            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_TILED:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, TILE_MODE, TILED4X4);
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_SUPER_TILED:
        case N2D_SUPER_TILED_128B:
        case N2D_SUPER_TILED_256B:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, TILE_MODE, SUPER_TILED_XMAJOR);
            srcConfigEx |= gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, SUPER_TILED, ENABLED);
            break;

        case N2D_YMAJOR_SUPER_TILED:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, TILE_MODE, SUPER_TILED_YMAJOR);
            srcConfigEx = gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, SUPER_TILED, ENABLED);
            break;

        case N2D_TILED_8X8_XMAJOR:
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE8X8_XMAJOR);
            break;

        case N2D_TILED_8X8_YMAJOR:
            if (Surface->format == N2D_NV12 || Surface->format == N2D_NV21) {
                srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE64X4);
            } else if (Surface->format == N2D_P010_MSB || Surface->format == N2D_P010_LSB
                    || Surface->format == N2D_I010 || Surface->format == N2D_I010_LSB) {
                srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE32X4);
            }
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_TILED_8X4:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE8X4);
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_TILED_4X8:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE4X8);
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_TILED_32X4:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE32X4);
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_TILED_64X4:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE64X4);
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_MULTI_TILED:
            srcConfigEx = gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, MULTI_TILED, ENABLED);
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                AQDE_SRC_EX_ADDRESS_Address,
                Surface->gpuAddress[1]
                ));

            break;

        case N2D_MULTI_SUPER_TILED:
            srcConfigEx = gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, SUPER_TILED, ENABLED)
                     | gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, MULTI_TILED, ENABLED);

            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                AQDE_SRC_EX_ADDRESS_Address,
                Surface->gpuAddress[1]
                ));

            break;

        case N2D_MINOR_TILED:
            srcConfigEx = gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, MINOR_TILED, ENABLED);
            break;

        default:
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    *SrcConfig |= srcConfig;
    *SrcConfigEx |= srcConfigEx;

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

static n2d_error_t gcSetSourceTilingConfigWithDEC400Module(
    IN n2d_user_hardware_t  *Hardware,
    IN gcsSURF_INFO_PTR     Surface,
    OUT n2d_uint32_t        *SrcConfig,
    OUT n2d_uint32_t        *SrcConfigEx)
{
    n2d_error_t error;
    n2d_uint32_t srcConfig = 0,srcConfigEx = 0;

    if (N2D_NULL == SrcConfig || N2D_NULL == SrcConfigEx) {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    srcConfig = gcmSETFIELDVALUE(srcConfig,
                               AQDE_SRC_CONFIG,
                               TILED,
                               ENABLED
                               );

    /*non supertile X/Y major */
    if (!(Surface->tiling & N2D_SUPER_TILED)
        && ((Surface->tile_status_config & N2D_TSC_DEC_COMPRESSED) || gcIsYuv420SupportTileY(Surface->format))) {
        srcConfig = gcmSETFIELDVALUE(srcConfig, AQDE_SRC_CONFIG, TILE_MODE, DEC400);
    }

    switch (Surface->tiling) {
        case N2D_LINEAR:
            srcConfig = gcmSETFIELDVALUE(srcConfig,
                                       AQDE_SRC_CONFIG,
                                       TILED,
                                       DISABLED);
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;

            break;

        case N2D_TILED:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE,TILE4X4);
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_SUPER_TILED:
        case N2D_SUPER_TILED_128B:
        case N2D_SUPER_TILED_256B:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, TILE_MODE, SUPER_TILED_XMAJOR);
            srcConfigEx |= gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, SUPER_TILED, ENABLED);
            break;

        case N2D_YMAJOR_SUPER_TILED:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, TILE_MODE, SUPER_TILED_YMAJOR);
            srcConfigEx = gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, SUPER_TILED, DISABLED);
            break;

        case N2D_TILED_8X8_XMAJOR:
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE8X8_XMAJOR);
            break;

        case N2D_TILED_8X8_YMAJOR:
            if (Surface->format == N2D_NV12 || Surface->format == N2D_NV21) {
                srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE64X4);
            } else if (Surface->format == N2D_P010_MSB || Surface->format == N2D_P010_LSB
                    || Surface->format == N2D_I010 || Surface->format == N2D_I010_LSB) {
                srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE32X4);
            }
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_TILED_8X4:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE8X4);
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_TILED_4X8:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE4X8);
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_TILED_32X4:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE32X4);
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_TILED_64X4:
            srcConfig |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DEC400_TILE_MODE, TILE64X4);
            srcConfigEx = AQDE_SRC_EX_CONFIG_ResetValue;
            break;

        case N2D_MULTI_TILED:
            srcConfigEx = gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, MULTI_TILED, ENABLED);
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                AQDE_SRC_EX_ADDRESS_Address,
                Surface->gpuAddress[1]
                ));
            break;

        case N2D_MULTI_SUPER_TILED:
            srcConfigEx = gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, SUPER_TILED, ENABLED)
                     | gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, MULTI_TILED, ENABLED);
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                AQDE_SRC_EX_ADDRESS_Address,
                Surface->gpuAddress[1]
                ));
            break;

        case N2D_MINOR_TILED:
            srcConfigEx = gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, MINOR_TILED, ENABLED);
            break;

        default:
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }


    *SrcConfig |= srcConfig;
    *SrcConfigEx |= srcConfigEx;

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

static n2d_error_t gcSetMultiSourceTilingConfigDefault(
    IN gcsSURF_INFO_PTR     Surface,
    OUT n2d_uint32_t        *Data)
{
    n2d_error_t error;
    n2d_uint32_t *data = Data;

    if (N2D_NULL == Data) {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    data[0] = gcmSETFIELDVALUE(data[0],
                               GCREG_DE_SRC_CONFIG_EX,
                               TILED,
                               ENABLED
                               );

    switch (Surface->tiling) {
        case N2D_LINEAR:
            data[0] = gcmSETFIELDVALUE(data[0],
                                       GCREG_DE_SRC_CONFIG_EX,
                                       TILED,
                                       DISABLED);

            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        case N2D_TILED:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, TILE_MODE, TILED4X4);
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        case N2D_SUPER_TILED:
        case N2D_SUPER_TILED_128B:
        case N2D_SUPER_TILED_256B:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, TILE_MODE, SUPER_TILED_XMAJOR);
            data[1] |= gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, SUPER_TILED, ENABLED);
            break;

        case N2D_YMAJOR_SUPER_TILED:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, TILE_MODE, SUPER_TILED_YMAJOR);
            data[1] = gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, SUPER_TILED, DISABLED);
            break;

        case N2D_TILED_8X8_XMAJOR:
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE8X8_XMAJOR);
            break;

        case N2D_TILED_8X8_YMAJOR:
            if (Surface->format == N2D_NV12 || Surface->format == N2D_NV21) {
                data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE64X4);
            } else if (Surface->format == N2D_P010_MSB || Surface->format == N2D_P010_LSB
                    || Surface->format == N2D_I010 || Surface->format == N2D_I010_LSB) {
                data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE32X4);
            }
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        case N2D_TILED_8X4:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE8X4);
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        case N2D_TILED_4X8:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE4X8);
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        case N2D_TILED_32X4:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE32X4);
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        case N2D_TILED_64X4:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE64X4);
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        default:
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

static n2d_error_t gcSetMultiSourceTilingConfigWithDEC400Module(
    IN gcsSURF_INFO_PTR     Surface,
    OUT n2d_uint32_t        *Data
    )
{
    n2d_error_t error;
    n2d_uint32_t *data= Data;

    if (N2D_NULL == Data) {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    data[0] = gcmSETFIELDVALUE(data[0],
                               GCREG_DE_SRC_CONFIG_EX,
                               TILED,
                               ENABLED
                               );

    /*non supertile X/Y major */
    if (!(Surface->tiling & N2D_SUPER_TILED)
        && ((Surface->tile_status_config & N2D_TSC_DEC_COMPRESSED) || gcIsYuv420SupportTileY(Surface->format))) {
        data[0] = gcmSETFIELDVALUE(data[0], GCREG_DE_SRC_CONFIG_EX, TILE_MODE, DEC400);
    }

    switch (Surface->tiling) {
        case N2D_LINEAR:
            data[0] = gcmSETFIELDVALUE(data[0],
                                       GCREG_DE_SRC_CONFIG_EX,
                                       TILED,
                                       DISABLED);
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;

            break;

        case N2D_TILED:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE,TILE4X4);
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        case N2D_SUPER_TILED:
        case N2D_SUPER_TILED_128B:
        case N2D_SUPER_TILED_256B:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, TILE_MODE, SUPER_TILED_XMAJOR);
            data[1] |= gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, SUPER_TILED, ENABLED);
            break;

        case N2D_YMAJOR_SUPER_TILED:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, TILE_MODE, SUPER_TILED_YMAJOR);
            data[1] = gcmSETFIELDVALUE(0, AQDE_SRC_EX_CONFIG, SUPER_TILED, ENABLED);
            break;

        case N2D_TILED_8X8_XMAJOR:
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE8X8_XMAJOR);
            break;

        case N2D_TILED_8X8_YMAJOR:
            if (Surface->format == N2D_NV12 || Surface->format == N2D_NV21) {
                data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE64X4);
            } else if (Surface->format == N2D_P010_MSB || Surface->format == N2D_P010_LSB
                    || Surface->format == N2D_I010 || Surface->format == N2D_I010_LSB) {
                data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE32X4);
            }
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        case N2D_TILED_8X4:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE8X4);
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        case N2D_TILED_4X8:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE4X8);
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        case N2D_TILED_32X4:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE32X4);
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        case N2D_TILED_64X4:
            data[0] |= gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, DEC400_TILE_MODE, TILE64X4);
            data[1] = GCREG_DE_SRC_CONFIG_EX_ResetValue;
            break;

        default:
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcSetSourceTilingConfig(
    IN n2d_user_hardware_t  *Hardware,
    IN gcsSURF_INFO_PTR     Surface,
    OUT n2d_uint32_t        *SrcConfig,
    OUT n2d_uint32_t        *SrcConfigEx)
{
    n2d_error_t error;

    if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION]) {
        N2D_ON_ERROR(gcSetSourceTilingConfigWithDEC400Module(Hardware, Surface, SrcConfig, SrcConfigEx));
    } else {
        N2D_ON_ERROR(gcSetSourceTilingConfigDefault(Hardware, Surface, SrcConfig, SrcConfigEx));
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}

n2d_error_t gcSetMultiSourceTilingConfig(
    IN n2d_user_hardware_t  *Hardware,
    IN gcsSURF_INFO_PTR     Surface,
    OUT n2d_uint32_t        *Data)
{
    n2d_error_t error;

    if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION]) {
        N2D_ON_ERROR(gcSetMultiSourceTilingConfigWithDEC400Module(Surface, Data));
    } else {
        N2D_ON_ERROR(gcSetMultiSourceTilingConfigDefault(Surface, Data));
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}

n2d_error_t gcSetColorSource(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_bool_t CoordRelative,
    IN n2d_uint32_t Transparency,
    IN n2d_csc_mode_t Mode,
    IN n2d_bool_t DeGamma,
    IN n2d_bool_t Filter
    )
{
    n2d_error_t error;
    n2d_uint32_t format, swizzle,strideWidth[3];
    n2d_uint32_t data[4], configEx = 0, formatEx;
    /*gcregPEControl reset value*/
    n2d_uint32_t peControl[8] = {0};
    n2d_uint32_t rotated = 0;
    n2d_uint32_t rgbaSwizzle, uvSwizzle;
    n2d_bool_t   fullRot;
    n2d_uint32_t  fBPPs[3];
    n2d_uint32_t size[3] = {0};
    FormatInfo_t *FormatInfo = N2D_NULL;

    fullRot = 1;

    /* Check the rotation capability. */
    if (!fullRot && gcmGET_PRE_ROTATION(Surface->rotation) != N2D_0)
    {
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    /* Convert the format. */
    N2D_ON_ERROR(gcTranslateSourceFormat(Hardware, Surface->format, &format, &formatEx, &swizzle));

    N2D_ON_ERROR(gcGetFormatInfo(Surface->format,&FormatInfo));

    /* Determine color swizzle. */
    if (N2D_FORMARTTYPE_YUV == FormatInfo->formatType)
    {
        rgbaSwizzle = AQDE_SRC_CONFIG_SWIZZLE_ARGB;
        uvSwizzle   = swizzle;
    }
    else
    {
        rgbaSwizzle = swizzle;
        uvSwizzle   = AQPE_CONTROL_UV_SWIZZLE_UV;
    }

    if (fullRot)
    {
        rotated = N2D_FALSE;
    }

    N2D_ON_ERROR(gcQueryFBPPs(Surface->format, fBPPs));

    N2D_ON_ERROR(gcGetSurfaceBufferSize(Hardware, Surface, N2D_TRUE, N2D_NULL, size));

    /*dump 1st plane*/
    N2D_DUMP_SURFACE(Surface->memory, Surface->gpuAddress[0], size[0], N2D_TRUE, Hardware);

    data[0] = Surface->gpuAddress[0];
    data[1] = Surface->stride[0];

    /* AQDE_SRC_ROTATION_CONFIG_Address */
    data[2]
        = gcmSETFIELD(0, AQDE_SRC_ROTATION_CONFIG, WIDTH,    Surface->alignedWidth)
        | gcmSETFIELD(0, AQDE_SRC_ROTATION_CONFIG, ROTATION, rotated);

    /* AQDE_SRC_CONFIG_Address; transparency field is obsolete for PE 2.0. */
    data[3]
        = gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, LOCATION,      MEMORY)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, TRANSPARENCY,  Transparency)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, FORMAT,        format)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, SOURCE_FORMAT, format)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, SWIZZLE,       rgbaSwizzle)
        | gcmSETFIELD     (0, AQDE_SRC_CONFIG, SRC_RELATIVE,  CoordRelative)
        | ((Surface->format == N2D_P010_LSB || Surface->format == N2D_I010_LSB) ?
            gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, P010_BITS, P010_LSB) : 0);

    strideWidth[0] = Surface->alignedWidth;
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_SRC_PLANE0_ALIGN_CONFIG_Address,
        gcmSETFIELD(0, GCREG_DE_SRC_PLANE0_ALIGN_CONFIG, STRIDE_WIDTH, strideWidth[0])));

    N2D_ON_ERROR(gcSetSourceTilingConfig(Hardware, Surface, &data[3], &configEx));

    N2D_ON_ERROR(gcSetSourceTileStatus(
        Hardware,
        0,
        Surface
        ));

    /* Set compression related config for source. */
    N2D_ON_ERROR(gcSetSourceCompression(Hardware, Surface, 0, N2D_FALSE, &configEx));

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_SRC_EX_CONFIG_Address,
        configEx
        ));

    if (DeGamma)
    {
        data[3] |= gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, DE_GAMMA, ENABLED);
    }

    /* Load source states. */
    N2D_ON_ERROR(gcWriteRegWithLoadState(
        Hardware,
        AQDE_SRC_ADDRESS_Address, 4,
        data
        ));

    switch (FormatInfo->planeNum)
    {
        case N2D_THREEPLANES:
            /*dump 3rd plane*/
            N2D_DUMP_SURFACE(Surface->uv_memory[1], Surface->gpuAddress[2], size[2], N2D_TRUE, Hardware);
            data[0] = Surface->gpuAddress[2];

            strideWidth[0] >>= 1;

            data[1] = (n2d_uint32_t)(strideWidth[0] * fBPPs[0]);
            N2D_ON_ERROR(gcWriteRegWithLoadState(
                Hardware,
                VPLANE_ADDRESS_Address,
                2,
                data));

            strideWidth[2] = Surface->stride[2];
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_SRC_PLANE2_ALIGN_CONFIG_Address,
                gcmSETFIELD(0, GCREG_DE_SRC_PLANE2_ALIGN_CONFIG, STRIDE_WIDTH, strideWidth[2])));
            /*Please don't add break*/
            fallthrough;

        case N2D_TWOPLANES:
            /*dump 2nd plane*/
            N2D_DUMP_SURFACE(Surface->uv_memory[0], Surface->gpuAddress[1], size[1], N2D_TRUE, Hardware);

            data[0] = Surface->gpuAddress[1];
            data[1] = (n2d_uint32_t)(strideWidth[0] * fBPPs[0]);

            N2D_ON_ERROR(gcWriteRegWithLoadState(
                Hardware,
                UPLANE_ADDRESS_Address,
                2,
                data));

            strideWidth[1] = Surface->stride[1];

            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_SRC_PLANE1_ALIGN_CONFIG_Address,
                gcmSETFIELD(0, GCREG_DE_SRC_PLANE1_ALIGN_CONFIG, STRIDE_WIDTH, strideWidth[0])));
            break;

        default:
            break;
    }

    N2D_ON_ERROR(gcSetSourceExtensionFormat(Hardware, format, formatEx));

    if (fullRot)
    {
        n2d_uint32_t srcRot = 0;
        n2d_uint32_t value;

        N2D_ON_ERROR(gcTranslateSourceRotation(gcmGET_PRE_ROTATION(Surface->rotation), &srcRot));

        if (!Filter)
        {
            /* Flush the 2D pipe before writing to the rotation register. */
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                AQ_FLUSH_Address,
                gcmSETFIELDVALUE(0,
                                 AQ_FLUSH,
                                 PE2D_CACHE,
                                 ENABLE)));
        }

        /* Load source height. */
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQDE_SRC_ROTATION_HEIGHT_Address,
            gcmSETFIELD(0, AQDE_SRC_ROTATION_HEIGHT, HEIGHT, Surface->alignedHeight)
            ));

        /* AQDE_ROT_ANGLE_Address */
        {
            /* Enable src mask. */
            value = gcmSETFIELDVALUE(~0, AQDE_ROT_ANGLE, MASK_SRC, ENABLED);

            /* Set src rotation. */
            value = gcmSETFIELD(value, AQDE_ROT_ANGLE, SRC, srcRot);
        }

        if (Hardware->features[N2D_FEATURE_2D_POST_FLIP])
        {
            /*If flip is added later,need to modify this configure */
            value = gcmSETFIELDVALUE(value,AQDE_ROT_ANGLE, SRC_FLIP,NONE);
            value = gcmSETFIELDVALUE(value, AQDE_ROT_ANGLE, MASK_SRC_FLIP, ENABLED);
        }

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
                    Hardware,
                    AQDE_ROT_ANGLE_Address,
                    value
                    ));
    }

    /* Load source UV swizzle and YUV mode state. */
    uvSwizzle = gcmSETMASKEDFIELD(AQPE_CONTROL, UV_SWIZZLE, uvSwizzle);

    switch (Mode)
    {
    case N2D_CSC_BT601:
        uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, AQPE_CONTROL, YUV, 601);
        break;

    case N2D_CSC_BT709:
        uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, AQPE_CONTROL, YUV, 709);
        break;

    case N2D_CSC_BT2020:
        uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, AQPE_CONTROL, YUV, 2020);
        break;

    case N2D_CSC_USER_DEFINED_CLAMP:
        uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, AQPE_CONTROL, YUV_CLAMP, ENABLED);
        uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, AQPE_CONTROL, MASK_YUV_CLAMP, ENABLED);
        /*no need break;*/
        fallthrough;

    case N2D_CSC_USER_DEFINED:
        uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, AQPE_CONTROL, YUV, UserDefined);
        break;

    default:
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, AQPE_CONTROL, MASK_YUV, ENABLED);

    if (Hardware->state.grayRGB2YUV)
    {
        uvSwizzle = gcmSETMASKEDFIELDVALUE(AQPE_CONTROL, YUVRGB, ENABLED);
    }

    if (Hardware->state.grayYUV2RGB)
    {
        uvSwizzle = gcmSETMASKEDFIELDVALUE(AQPE_CONTROL, GRAY_YUVRGB, ENABLED);
    }

    /*set gcregPEControl*/
    if(Filter)
    {
        peControl[0] = uvSwizzle;
        N2D_ON_ERROR(gcWriteRegWithLoadState(
            Hardware,
            GCREG_PE_CONTROL_EX_Address, 8,
            peControl
            ));
    }

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQPE_CONTROL_Address,
        uvSwizzle
        ));

on_error:
    /* Return the error. */
    return error;
}

/*******************************************************************************
**
**  gcSetMultiSource
**
**  Configure 8x color source.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcoSURF Surface
**          Pointer to the source surface descriptor.
**
**      n2d_bool_t CoordRelative
**          If N2D_FALSE, the source origin represents absolute pixel coordinate
**          within the source surface.  If N2D_TRUE, the source origin represents
**          the offset from the destination origin.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetMultiSource(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t RegGroupIndex,
    IN n2d_uint32_t SrcIndex,
    IN gcs2D_State_PTR State,
    IN n2d_bool_t MultiDstRect)
{
    n2d_error_t     error;
    n2d_uint32_t    format, swizzle, formatEx;
    n2d_uint32_t    data[4];
    n2d_uint32_t    rgbaSwizzle, uvSwizzle; /* Determine color swizzle. */
    gcs2D_MULTI_SOURCE_PTR src = &State->multiSrc[SrcIndex];
    gcsSURF_INFO    *Surface = &src->srcSurface;
    n2d_uint32_t    regOffset = RegGroupIndex << 2;
    n2d_bool_t      setSrcRect = N2D_TRUE;
    n2d_uint32_t    address, strideWidth[3];
    n2d_uint32_t     fBPPs[3];
    n2d_uint32_t    size[3] = {0};
    FormatInfo_t         *FormatInfo = N2D_NULL;

    N2D_ON_ERROR(gcGetFormatInfo(Surface->format,&FormatInfo));

    /* Convert the format. */
    N2D_ON_ERROR(gcTranslateSourceFormat(Hardware, Surface->format, &format, &formatEx, &swizzle));

    if (N2D_FORMARTTYPE_YUV == FormatInfo->formatType)
    {
        rgbaSwizzle = GCREG_DE_SRC_CONFIG_EX_SWIZZLE_ARGB;
        uvSwizzle   = swizzle;
    }
    else
    {
        rgbaSwizzle = swizzle;
        uvSwizzle   = GCREG_PE_CONTROL_EX_UV_SWIZZLE_UV;
    }

    N2D_ON_ERROR(gcGetSurfaceBufferSize(Hardware, Surface, N2D_TRUE, N2D_NULL, size));

    /*dump 1st plane*/
    N2D_DUMP_SURFACE(Surface->memory, Surface->gpuAddress[0], size[0], N2D_TRUE, Hardware);

    address = src->srcSurface.gpuAddress[0];

    /* Load source address. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_SRC_ADDRESS_EX_Address + regOffset : AQDE_SRC_ADDRESS_Address,
        address
        ));

#ifdef N2D_SUPPORT_64BIT_ADDRESS
    gcSetHigh32BitAddress(Hardware, (n2d_uint32_t)Surface->gpuAddrEx, N2D_ADDRESS_SRC0 + RegGroupIndex);
#endif

    /* Load source stride. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_SRC_STRIDE_EX_Address + regOffset : AQDE_SRC_STRIDE_Address,
        Surface->stride[0]
        ));

    /* Load source width; rotation is not supported. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_SRC_ROTATION_CONFIG_EX_Address + regOffset : AQDE_SRC_ROTATION_CONFIG_Address,
        gcmSETFIELD(0, GCREG_DE_SRC_ROTATION_CONFIG_EX, WIDTH, Surface->alignedWidth)
        ));

    N2D_ON_ERROR(gcQueryFBPPs(Surface->format, fBPPs));
    strideWidth[0] = (n2d_uint32_t)(Surface->stride[0] / fBPPs[0]);

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_SRC_PLANE0_ALIGN_CONFIG_Address + regOffset,
        gcmSETFIELD(0, GCREG_DE_SRC_PLANE0_ALIGN_CONFIG, STRIDE_WIDTH, strideWidth[0])
        ));

    /* Load source config */
    data[0] =gcmSETFIELDVALUE (0, GCREG_DE_SRC_CONFIG_EX, LOCATION,            MEMORY)
           | gcmSETFIELD      (0, GCREG_DE_SRC_CONFIG_EX, SOURCE_FORMAT,       format)
           | gcmSETFIELD      (0, GCREG_DE_SRC_CONFIG_EX, SWIZZLE,             rgbaSwizzle)
           | (src->srcRelativeCoord ?
              gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, SRC_RELATIVE,        RELATIVE)
             :gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, SRC_RELATIVE,        ABSOLUTE));

    N2D_ON_ERROR(gcSetMultiSourceTilingConfig(Hardware,Surface,data));

    N2D_ON_ERROR(gcSetSourceTileStatus(
        Hardware,
        RegGroupIndex,
        Surface
        ));

    /* Set compression related config for source. */
    N2D_ON_ERROR(gcSetSourceCompression(Hardware, Surface, RegGroupIndex, N2D_TRUE, &data[1]));

    if(RegGroupIndex)
    {
        data[0] |= ((Surface->format == N2D_P010_LSB || Surface->format == N2D_I010_LSB)
                   ? gcmSETFIELDVALUE(0, GCREG_DE_SRC_CONFIG_EX, P010_BITS, P010_LSB) : 0);
    }
    else
    {
        data[0] |= ((Surface->format == N2D_P010_LSB || Surface->format == N2D_I010_LSB)
                   ? gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, P010_BITS, P010_LSB) : 0);
    }

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_SRC_CONFIG_EX_Address + regOffset : AQDE_SRC_CONFIG_Address,
        data[0]
        ));

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_SRC_EX_CONFIG_EX_Address + regOffset : AQDE_SRC_EX_CONFIG_Address,
        data[1]
        ));

    N2D_ON_ERROR(gcTranslateSourceRotation(gcmGET_PRE_ROTATION(Surface->rotation), &data[0]));

    /* Enable src rotation. */
    data[1] = gcmSETFIELDVALUE(~0, GCREG_DE_ROT_ANGLE_EX, MASK_SRC, ENABLED);

    /* Set src rotation. */
    data[1] = gcmSETFIELD(data[1], GCREG_DE_ROT_ANGLE_EX, SRC, data[0]);

    data[0] = GCREG_DE_ROT_ANGLE_EX_SRC_MIRROR_NONE;
    if (src->horMirror)
    {
        data[0] |= GCREG_DE_ROT_ANGLE_EX_SRC_MIRROR_MIRROR_X;
    }

    if (src->verMirror)
    {
        data[0] |= GCREG_DE_ROT_ANGLE_EX_SRC_MIRROR_MIRROR_Y;
    }

    data[1] = gcmSETFIELD(data[1], GCREG_DE_ROT_ANGLE_EX, SRC_MIRROR, data[0]);
    data[1] = gcmSETFIELDVALUE(data[1], GCREG_DE_ROT_ANGLE_EX, MASK_SRC_MIRROR, ENABLED);

    data[1] = gcmSETFIELDVALUE(data[1], GCREG_DE_ROT_ANGLE_EX, DST_MIRROR, NONE);
    data[1] = gcmSETFIELDVALUE(data[1], GCREG_DE_ROT_ANGLE_EX, MASK_DST_MIRROR, ENABLED);

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                RegGroupIndex ? GCREG_DE_ROT_ANGLE_EX_Address + regOffset : AQDE_ROT_ANGLE_Address,
                data[1]
                ));

    N2D_ON_ERROR(gcSetSourceExtensionFormat(Hardware, format, formatEx));

    /* Load source height. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_SRC_ROTATION_HEIGHT_EX_Address + regOffset : AQDE_SRC_ROTATION_HEIGHT_Address,
        gcmSETFIELD(0, GCREG_DE_SRC_ROTATION_HEIGHT_EX, HEIGHT, Surface->alignedHeight)
        ));

    if (N2D_FORMARTTYPE_YUV == FormatInfo->formatType)
    {
        switch (FormatInfo->planeNum)
        {
            case N2D_THREEPLANES:
                /*dump 3rd plane*/
                N2D_DUMP_SURFACE(Surface->uv_memory[1], Surface->gpuAddress[2], size[2], N2D_TRUE, Hardware);

                /* Load source V plane address. */
                N2D_ON_ERROR(gcWriteRegWithLoadState32(
                    Hardware,
                    RegGroupIndex ? GCREG_DE_ADDRESS_VEX_Address + regOffset : VPLANE_ADDRESS_Address,
                    Surface->gpuAddress[2]
                    ));

                /* Load source V plane stride. */
                N2D_ON_ERROR(gcWriteRegWithLoadState32(
                    Hardware,
                    RegGroupIndex ? GCREG_DE_STRIDE_VEX_Address + regOffset : VPLANE_STRIDE_Address,
                    Surface->stride[2]
                    ));

                strideWidth[2] = Surface->stride[2] / fBPPs[2];

                N2D_ON_ERROR(gcWriteRegWithLoadState32(
                    Hardware,
                    GCREG_DE_SRC_PLANE2_ALIGN_CONFIG_Address + regOffset,
                    gcmSETFIELD(0, GCREG_DE_SRC_PLANE2_ALIGN_CONFIG, STRIDE_WIDTH, strideWidth[2])
                    ));
                fallthrough;

            case N2D_TWOPLANES:
                /*dump 2nd plane*/
                N2D_DUMP_SURFACE(Surface->uv_memory[0], Surface->gpuAddress[1], size[1], N2D_TRUE, Hardware);

                /* Load source U plane address. */
                N2D_ON_ERROR(gcWriteRegWithLoadState32(
                    Hardware,
                    RegGroupIndex ? GCREG_DE_ADDRESS_UEX_Address + regOffset : UPLANE_ADDRESS_Address,
                    Surface->gpuAddress[1]
                    ));

                /* Load source U plane stride. */
                N2D_ON_ERROR(gcWriteRegWithLoadState32(
                    Hardware,
                    RegGroupIndex ? GCREG_DE_STRIDE_UEX_Address + regOffset : UPLANE_STRIDE_Address,
                    Surface->stride[1]
                    ));

                strideWidth[1] = Surface->stride[1] / fBPPs[1];

                N2D_ON_ERROR(gcWriteRegWithLoadState32(
                    Hardware,
                    GCREG_DE_SRC_PLANE1_ALIGN_CONFIG_Address + regOffset,
                    gcmSETFIELD(0, GCREG_DE_SRC_PLANE1_ALIGN_CONFIG, STRIDE_WIDTH, strideWidth[1])
                    ));

                break;

            default:
                break;
        }

        uvSwizzle = gcmSETMASKEDFIELD(GCREG_PE_CONTROL_EX, UV_SWIZZLE, uvSwizzle);

        switch (src->srcCSCMode)
        {
        case N2D_CSC_BT601:
            uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, GCREG_PE_CONTROL_EX, YUV, 601);
            break;

        case N2D_CSC_BT709:
            uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, GCREG_PE_CONTROL_EX, YUV, 709);
            break;

        case N2D_CSC_BT2020:
            uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, GCREG_PE_CONTROL_EX, YUV, 2020);
            break;

        case N2D_CSC_USER_DEFINED_CLAMP:
            uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, GCREG_PE_CONTROL_EX, YUV_CLAMP, ENABLED);
            uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, GCREG_PE_CONTROL_EX, MASK_YUV_CLAMP, ENABLED);
            /*no need break;*/
            fallthrough;

        case N2D_CSC_USER_DEFINED:
            uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, GCREG_PE_CONTROL_EX, YUV, UserDefined);
            break;

        default:
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        }

        uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, GCREG_PE_CONTROL_EX, MASK_YUV, ENABLED);

        if (src->enableAlpha || Hardware->features[N2D_FEATURE_2D_10BIT_OUTPUT_LINEAR])
        {
            uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, GCREG_PE_CONTROL_EX, YUVRGB, ENABLED);
        }
        else
        {
            uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, GCREG_PE_CONTROL_EX, YUVRGB, DISABLED);
        }

        uvSwizzle = gcmSETFIELDVALUE(uvSwizzle, GCREG_PE_CONTROL_EX, MASK_YUVRGB, ENABLED);

        /* Load source UV swizzle and YUV mode state. */
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            RegGroupIndex ? GCREG_PE_CONTROL_EX_Address + regOffset : AQPE_CONTROL_Address,
            uvSwizzle
            ));
    }

    /* Load source color key. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_SRC_COLOR_BG_EX_Address + regOffset : AQDE_SRC_COLOR_BG_Address,
        Surface->format == N2D_INDEX8 ?
            ((src->srcColorKeyLow) & 0xFF) << 24
            : src->srcColorKeyLow
        ));

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_SRC_COLOR_KEY_HIGH_EX_Address + regOffset : AQDE_SRC_COLOR_KEY_HIGH_Address,
        Surface->format == N2D_INDEX8 ?
            ((src->srcColorKeyHigh) & 0xFF) << 24
            : src->srcColorKeyHigh
        ));

    /*******************************************************************
    ** Setup ROP.
    */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_ROP_EX_Address + regOffset : AQDE_ROP_Address,
        gcmSETFIELDVALUE(0, GCREG_DE_ROP_EX, TYPE,   ROP4)
          | gcmSETFIELD (0, GCREG_DE_ROP_EX, ROP_BG, src->bgRop)
          | gcmSETFIELD (0, GCREG_DE_ROP_EX, ROP_FG, src->fgRop)
        ));

    /* Convert the transparency modes. */
    /* Compatible with PE1.0. */
    if (!Hardware->features[N2D_FEATURE_ANDROID_ONLY] &&
        (src->patTransparency == N2D_OPAQUE)
                && ((((src->fgRop >> 4) & 0x0F) != (src->fgRop & 0x0F))
                || (((src->bgRop >> 4) & 0x0F) != (src->bgRop & 0x0F))))
    {
        data[2] = N2D_MASKED;
    }
    else
    {
        data[2] = src->patTransparency;
    }

    N2D_ON_ERROR(gcTranslateSourceTransparency(
        src->srcTransparency, &data[0]
        ));

    N2D_ON_ERROR(gcTranslateTargetTransparency(
        src->dstTransparency, &data[1]
        ));

    N2D_ON_ERROR(gcTranslatePatternTransparency(
        data[2], &data[2]
        ));

    /* LoadState transparency modes.
       Enable Source or Destination read when
       respective Color key is turned on. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_PE_TRANSPARENCY_EX_Address + regOffset : AQPE_TRANSPARENCY_Address,
        gcmSETFIELD(0, GCREG_PE_TRANSPARENCY_EX, SOURCE,      data[0])
                     | gcmSETFIELD(0, GCREG_PE_TRANSPARENCY_EX, DESTINATION, data[1])
                     | gcmSETFIELD(0, GCREG_PE_TRANSPARENCY_EX, PATTERN,     data[2])
                     | gcmSETFIELD(0, GCREG_PE_TRANSPARENCY_EX, USE_SRC_OVERRIDE,
                            (data[0] == GCREG_PE_TRANSPARENCY_EX_SOURCE_KEY))
                     | gcmSETFIELD(0, GCREG_PE_TRANSPARENCY_EX, USE_DST_OVERRIDE,
                            (data[1] == GCREG_PE_TRANSPARENCY_EX_DESTINATION_KEY))
        ));

    if (src->enableAlpha)
    {
        n2d_uint32_t srcAlphaMode = 0;
        n2d_uint32_t srcGlobalAlphaMode = 0;
        n2d_uint32_t srcFactorMode = 0;
        n2d_uint32_t srcFactorExpansion = 0;
        n2d_uint32_t dstAlphaMode = 0;
        n2d_uint32_t dstGlobalAlphaMode = 0;
        n2d_uint32_t dstFactorMode = 0;
        n2d_uint32_t dstFactorExpansion = 0;

        /* Translate inputs. */
        N2D_ON_ERROR(gcTranslatePixelAlphaMode(
            src->srcAlphaMode, &srcAlphaMode
            ));

        N2D_ON_ERROR(gcTranslatePixelAlphaMode(
            src->dstAlphaMode, &dstAlphaMode
            ));

        N2D_ON_ERROR(gcTranslateGlobalAlphaMode(
            src->srcGlobalAlphaMode, &srcGlobalAlphaMode
            ));

        N2D_ON_ERROR(gcTranslateGlobalAlphaMode(
            src->dstGlobalAlphaMode, &dstGlobalAlphaMode
            ));

        N2D_ON_ERROR(gcTranslateAlphaFactorMode(
            N2D_TRUE, src->srcFactorMode, &srcFactorMode, &srcFactorExpansion
            ));

        N2D_ON_ERROR(gcTranslateAlphaFactorMode(
            N2D_FALSE, src->dstFactorMode, &dstFactorMode, &dstFactorExpansion
            ));

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            RegGroupIndex ? GCREG_DE_ALPHA_CONTROL_EX_Address + regOffset : AQDE_ALPHA_CONTROL_Address,
            gcmSETFIELDVALUE(0, GCREG_DE_ALPHA_CONTROL_EX, ENABLE, ON)
            ));

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            RegGroupIndex ? GCREG_DE_ALPHA_MODES_EX_Address + regOffset : AQDE_ALPHA_MODES_Address,
                gcmSETFIELD(0, GCREG_DE_ALPHA_MODES_EX, SRC_ALPHA_MODE,
                                              srcAlphaMode)
              | gcmSETFIELD(0, GCREG_DE_ALPHA_MODES_EX, DST_ALPHA_MODE,
                                              dstAlphaMode)
              | gcmSETFIELD(0, GCREG_DE_ALPHA_MODES_EX, GLOBAL_SRC_ALPHA_MODE,
                                              srcGlobalAlphaMode)
              | gcmSETFIELD(0, GCREG_DE_ALPHA_MODES_EX, GLOBAL_DST_ALPHA_MODE,
                                              dstGlobalAlphaMode)
              | gcmSETFIELD(0, GCREG_DE_ALPHA_MODES_EX, SRC_BLENDING_MODE,
                                              srcFactorMode)
              | gcmSETFIELD(0, GCREG_DE_ALPHA_MODES_EX, DST_BLENDING_MODE,
                                              dstFactorMode)
            ));
    }
    else
    {
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            RegGroupIndex ? GCREG_DE_ALPHA_CONTROL_EX_Address + regOffset : AQDE_ALPHA_CONTROL_Address,
            gcmSETFIELDVALUE(0, GCREG_DE_ALPHA_CONTROL, ENABLE, OFF)
            ));
    }

    /* Load global src color value. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_GLOBAL_SRC_COLOR_EX_Address + regOffset : AQDE_GLOBAL_SRC_COLOR_Address,
        src->srcGlobalColor
        ));

    /* Load global dst color value. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_GLOBAL_DEST_COLOR_EX_Address + regOffset : AQDE_GLOBAL_DEST_COLOR_Address,
        src->dstGlobalColor
        ));

    /* Convert the multiply modes. */
    N2D_ON_ERROR(gcTranslatePixelColorMultiplyMode(
        src->srcPremultiplyMode, &data[0]
        ));

    N2D_ON_ERROR(gcTranslatePixelColorMultiplyMode(
        src->dstPremultiplyMode, &data[1]
        ));

    N2D_ON_ERROR(gcTranslateGlobalColorMultiplyMode(
        src->srcPremultiplyGlobalMode, &data[2]
        ));

    N2D_ON_ERROR(gcTranslatePixelColorMultiplyMode(
        src->dstDemultiplyMode, &data[3]
        ));

    /* LoadState pixel multiply modes. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        RegGroupIndex ? GCREG_DE_COLOR_MULTIPLY_MODES_EX_Address + regOffset : AQDE_COLOR_MULTIPLY_MODES_Address,
          gcmSETFIELD(0, GCREG_DE_COLOR_MULTIPLY_MODES_EX, SRC_PREMULTIPLY,        data[0])
        | gcmSETFIELD(0, GCREG_DE_COLOR_MULTIPLY_MODES_EX, DST_PREMULTIPLY,        data[1])
        | gcmSETFIELD(0, GCREG_DE_COLOR_MULTIPLY_MODES_EX, SRC_GLOBAL_PREMULTIPLY, data[2])
        | gcmSETFIELD(0, GCREG_DE_COLOR_MULTIPLY_MODES_EX, DST_DEMULTIPLY,         data[3])
        | gcmSETFIELDVALUE(0, GCREG_DE_COLOR_MULTIPLY_MODES_EX, DST_DEMULTIPLY_ZERO, ENABLE)
        ));

    if (Hardware->features[N2D_FEATURE_2D_MULTI_SOURCE_BLT])
    {
        gcsRECT drect, clip;
        n2d_uint32_t config;
        n2d_int32_t w, h;
        n2d_int32_t sw = src->srcRect.right - src->srcRect.left;
        n2d_int32_t dw = src->dstRect.right - src->dstRect.left;
        n2d_int32_t sh = src->srcRect.bottom - src->srcRect.top;
        n2d_int32_t dh = src->dstRect.bottom - src->dstRect.top;

        /* Clip Rect coordinates in positive range. */
        if (!(!MultiDstRect && dw == 0 && dh == 0))
        {
            if (Hardware->isStretchMultisrcStretchBlit)
            {
                src->clipRect = src->dstRect;
            }

            clip.left   = N2D_MAX(src->clipRect.left, 0);
            clip.top    = N2D_MAX(src->clipRect.top, 0);
            clip.right  = N2D_MAX(src->clipRect.right, 0);
            clip.bottom = N2D_MAX(src->clipRect.bottom, 0);

            if ((sw != dw) || (sh != dh))
            {
                n2d_bool_t initErr = N2D_FALSE;
                gcsRECT srect;
                n2d_int32_t hfact = gcGetStretchFactor(src->enableGDIStretch, sw, dw);
                n2d_int32_t vfact = gcGetStretchFactor(src->enableGDIStretch, sh, dh);
                n2d_int32_t hInit = 0;
                n2d_int32_t vInit = 0;

                config = gcmSETFIELDVALUE(~0, GCREG_MULTI_SRC_CONFIG, SRC_CMD, STRETCH_BLT);

                N2D_ON_ERROR(gcWriteRegWithLoadState32(
                    Hardware,
                    RegGroupIndex ? GCREG_DE_STRETCH_FACTOR_LOW_Address + regOffset : AQDE_STRETCH_FACTOR_LOW_Address,
                    hfact
                    ));

                N2D_ON_ERROR(gcWriteRegWithLoadState32(
                    Hardware,
                    RegGroupIndex ? GCREG_DE_STRETCH_FACTOR_HIGH_Address + regOffset : AQDE_STRETCH_FACTOR_HIGH_Address,
                    vfact
                    ));

                if (src->enableGDIStretch)
                {
                    config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, GDI_STRE, ENABLED);
                }
                else
                {
                    config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, GDI_STRE, DISABLED);

                    --dw; --dh; --sw; --sh;
                }

                config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, MASK_GDI_STRE, ENABLED);

                if (src->dstRect.left < src->clipRect.left)
                {
                    n2d_int32_t delta = src->clipRect.left - src->dstRect.left;

                    initErr = N2D_TRUE;

                    drect.left = src->clipRect.left;

                    if (src->enableGDIStretch)
                    {
                        srect.left = (delta * hfact) >> 16;
                        hInit += srect.left * dw - delta * sw;
                        srect.left +=  src->srcRect.left;
                    }
                    else
                    {
                        srect.right = delta * hfact;

                        if (srect.right & 0x8000)
                        {
                            hInit += dw << 1;
                            srect.left = 1;
                        }
                        else
                        {
                            srect.left = 0;
                        }

                        srect.right >>= 16;
                        hInit += (srect.right * dw - delta * sw) << 1;
                        srect.left +=  src->srcRect.left + srect.right;
                    }
                }
                else
                {
                    drect.left = src->dstRect.left;
                    srect.left = src->srcRect.left;
                }

                if (src->dstRect.right > src->clipRect.right)
                {
                    initErr = N2D_TRUE;
                    drect.right = src->clipRect.right;
                    srect.right = src->srcRect.right
                        - (((src->dstRect.right - src->clipRect.right) * hfact) >> 16);
                }
                else
                {
                    drect.right = src->dstRect.right;
                    srect.right = src->srcRect.right;
                }

                if (src->dstRect.top < src->clipRect.top)
                {
                    n2d_int32_t delta = src->clipRect.top - src->dstRect.top;

                    initErr = N2D_TRUE;

                    drect.top = src->clipRect.top;

                    if (src->enableGDIStretch)
                    {
                        srect.top = (delta * vfact) >> 16;
                        vInit += srect.top * dh - delta * sh;
                        srect.top += src->srcRect.top;
                    }
                    else
                    {
                        srect.bottom = delta * vfact;

                        if (srect.bottom & 0x8000)
                        {
                            vInit += dh << 1;
                            srect.top = 1;
                        }
                        else
                        {
                            srect.top = 0;
                        }

                        srect.bottom >>= 16;
                        vInit += (srect.bottom * dh - delta * sh) << 1;
                        srect.top += src->srcRect.top + srect.bottom;
                    }
                }
                else
                {
                    drect.top = src->dstRect.top;
                    srect.top = src->srcRect.top;
                }

                if (src->dstRect.bottom > src->clipRect.bottom)
                {
                    initErr = N2D_TRUE;
                    drect.bottom = src->clipRect.bottom;
                    srect.bottom = src->srcRect.bottom
                        - (((src->dstRect.bottom - src->clipRect.bottom) * vfact) >> 16);
                }
                else
                {
                    drect.bottom = src->dstRect.bottom;
                    srect.bottom = src->srcRect.bottom;
                }

                if (initErr)
                {
                    n2d_int32_t hSou, hNor, vSou, vNor;

                    if (src->enableGDIStretch)
                    {
                        hNor  =  (hfact >> 16) * dw - sw;
                        hSou  =  hNor + dw;
                        hInit += hSou;

                        vNor  =  (vfact >> 16) * dh - sh;
                        vSou  =  vNor + dh;
                        vInit += vSou;
                    }
                    else
                    {
                        hNor  =  ((hfact >> 16) * dw - sw) << 1;
                        hSou  =  hNor + (dw << 1);
                        hInit += hNor + dw;

                        vNor  =  ((vfact >> 16) * dh - sh) << 1;
                        vSou  =  vNor + (dh << 1);
                        vInit += vNor + dh;
                    }

                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                        Hardware,
                        GCREG_DE_SRC_VINITIAL_ERROR_Address + regOffset,
                        vInit
                        ));

                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                        Hardware,
                        GCREG_DE_SRC_VSOUTH_DELTA_Address + regOffset,
                        vSou
                        ));

                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                        Hardware,
                        GCREG_DE_SRC_VNORTH_DELTA_Address + regOffset,
                        vNor
                        ));

                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                        Hardware,
                        GCREG_DE_SRC_HINITIAL_ERROR_Address + regOffset,
                        hInit
                        ));

                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                        Hardware,
                        GCREG_DE_SRC_HSOUTH_DELTA_Address + regOffset,
                        hSou
                        ));

                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                        Hardware,
                        GCREG_DE_SRC_HNORTH_DELTA_Address + regOffset,
                        hNor
                        ));

                    /* load src origin. */
                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                        Hardware,
                        RegGroupIndex ? GCREG_DE_SRC_ORIGIN_EX_Address + regOffset : AQDE_SRC_ORIGIN_Address,
                        gcmSETFIELD  (0, GCREG_DE_SRC_ORIGIN_EX, X, srect.left)
                        | gcmSETFIELD(0, GCREG_DE_SRC_ORIGIN_EX, Y, srect.top)
                        ));

                    /* load src rect size. */
                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                        Hardware,
                        RegGroupIndex ? GCREG_DE_SRC_SIZE_EX_Address + regOffset : AQDE_SRC_SIZE_Address,
                        gcmSETFIELD  (0, GCREG_DE_SRC_SIZE_EX, X, srect.right - srect.left)
                        | gcmSETFIELD(0, GCREG_DE_SRC_SIZE_EX, Y, srect.bottom - srect.top)
                        ));

                    setSrcRect = N2D_FALSE;

                    config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, INITIAL_ERROR, ENABLED);
                }
                else
                {
                    config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, INITIAL_ERROR, DISABLED);
                }

                config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, MASK_INITIAL_ERROR, ENABLED);

                if (!Hardware->isStretchMultisrcStretchBlit)
                {
                    clip = drect;
                }

            }
            else
            {
                drect = src->dstRect;
                config = gcmSETFIELDVALUE(~0, GCREG_MULTI_SRC_CONFIG, SRC_CMD, BIT_BLT);

                config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, INITIAL_ERROR, DISABLED);
                config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, MASK_INITIAL_ERROR, ENABLED);
            }

            config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, MASK_SRC_CMD, ENABLED);
        }
        else
        {
            clip.left   = N2D_MAX(State->dstClipRect.left,   0);
            clip.top    = N2D_MAX(State->dstClipRect.top,    0);
            clip.right  = N2D_MAX(State->dstClipRect.right,  0);
            clip.bottom = N2D_MAX(State->dstClipRect.bottom, 0);

            drect  = src->srcRect;

            config = gcmSETFIELDVALUE(~0,     GCREG_MULTI_SRC_CONFIG, SRC_CMD, BIT_BLT);
            config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, MASK_SRC_CMD, ENABLED);
            config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, INITIAL_ERROR, DISABLED);
            config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, MASK_INITIAL_ERROR, ENABLED);
            config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, GDI_STRE, DISABLED);
            config = gcmSETFIELDVALUE(config, GCREG_MULTI_SRC_CONFIG, MASK_GDI_STRE, ENABLED);
        }

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GCREG_MULTI_SRC_CONFIG_Address + regOffset,
            config
            ));

        /* Set clipping rect. */
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            RegGroupIndex ? GCREG_DE_CLIP_TOP_LEFT_Address + regOffset : AQDE_CLIP_TOP_LEFT_Address,
              gcmSETFIELD(0, GCREG_DE_CLIP_TOP_LEFT, X, clip.left)
            | gcmSETFIELD(0, GCREG_DE_CLIP_TOP_LEFT, Y, clip.top)
            ));

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            RegGroupIndex ? GCREG_DE_CLIP_BOTTOM_RIGHT_Address + regOffset : AQDE_CLIP_BOTTOM_RIGHT_Address,
              gcmSETFIELD(0, GCREG_DE_CLIP_BOTTOM_RIGHT, X, clip.right)
            | gcmSETFIELD(0, GCREG_DE_CLIP_BOTTOM_RIGHT, Y, clip.bottom)
            ));

        {
            if ((gcmGET_PRE_ROTATION(State->dest.dstSurface.rotation) == N2D_90)
                || (gcmGET_PRE_ROTATION(State->dest.dstSurface.rotation) == N2D_270))
            {
                w = State->dest.dstSurface.alignedHeight;
                h = State->dest.dstSurface.alignedWidth;
            }
            else
            {
                w = State->dest.dstSurface.alignedWidth;
                h = State->dest.dstSurface.alignedHeight;
            }

            /* Set dest rect. */
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_DEST_RECT_TOP_LEFT_Address + regOffset,
                gcmSETFIELD  (0, GCREG_DE_DEST_RECT_TOP_LEFT, X, drect.left)
                | gcmSETFIELD(0, GCREG_DE_DEST_RECT_TOP_LEFT, Y, drect.top)
                ));

            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_DEST_RECT_BOTTOM_RIGHT_Address + regOffset,
                gcmSETFIELD  (0, GCREG_DE_DEST_RECT_BOTTOM_RIGHT, X, drect.right)
                | gcmSETFIELD(0, GCREG_DE_DEST_RECT_BOTTOM_RIGHT, Y, drect.bottom)
                ));
        }
    }

    if (setSrcRect)
    {
        /* load src origin. */
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            RegGroupIndex ? GCREG_DE_SRC_ORIGIN_EX_Address + regOffset : AQDE_SRC_ORIGIN_Address,
            gcmSETFIELD  (0, GCREG_DE_SRC_ORIGIN_EX, X, src->srcRect.left)
            | gcmSETFIELD(0, GCREG_DE_SRC_ORIGIN_EX, Y, src->srcRect.top)
            ));

        /* load src rect size. */
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            RegGroupIndex ? GCREG_DE_SRC_SIZE_EX_Address + regOffset : AQDE_SRC_SIZE_Address,
            gcmSETFIELD  (0, GCREG_DE_SRC_SIZE_EX, X, src->srcRect.right  - src->srcRect.left)
            | gcmSETFIELD(0, GCREG_DE_SRC_SIZE_EX, Y, src->srcRect.bottom - src->srcRect.top)
            ));
    }
    /* Success. */
    return N2D_SUCCESS;

on_error:

    return error;
}
