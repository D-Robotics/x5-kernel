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

#include "common.h"
#include "series_common.h"
#include "nano2D_user_hardware.h"
// #include "nano2D_feature_database.h"
#include "nano2D_user_hardware_dec.h"
#include "nano2D_user_query.h"


/*******************************************************************************
**
**  gcSetTargetTileStatus
**
**  Configure the source tile status.
**
**  INPUT:
**
**      n2d_user_hardware_t Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer to the target surface descriptor.
**
**  OUTPUT:
**
**      nothing
*/
static n2d_error_t gcSetTargetTileStatus(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Target
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t config;

    if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION] &&
        gcmHASCOMPRESSION(Target))
    {
        if (Target->tiling == N2D_SUPER_TILED_128B)
        {
            config = gcmSETFIELDVALUE(0, GCREG_DE_DST_TILE_STATUS_CONFIG, COMPRESSION_SIZE, Size128B);
        }
        else if (Target->tiling == N2D_SUPER_TILED_256B)
        {
            config = gcmSETFIELDVALUE(0, GCREG_DE_DST_TILE_STATUS_CONFIG, COMPRESSION_SIZE, Size256B);
        }
        else
        {
            config = GCREG_DE_DST_TILE_STATUS_CONFIG_ResetValue;
        }

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GCREG_DE_DST_TILE_STATUS_CONFIG_Address,
            config
            ));

        return N2D_SUCCESS;
    }
    else
    {
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GCREG_DE_DST_TILE_STATUS_CONFIG_Address,
            GCREG_DE_DST_TILE_STATUS_CONFIG_ResetValue
            ));
        return N2D_SUCCESS;
    }

on_error:
    /* Return the status. */
    return error;
}

n2d_bool_t gcIsYuv420SupportTileY(
    IN n2d_buffer_format_t Format)
{
    n2d_bool_t ret = N2D_FALSE;

    switch (Format) {
        case N2D_NV12:
        case N2D_NV21:
        case N2D_P010_MSB:
        case N2D_P010_LSB:
            ret = N2D_TRUE;
            break;

        default:
            ret = N2D_FALSE;
    }

    return ret;
}

n2d_error_t gcSetTargetColorKeyRange(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t ColorLow,
    IN n2d_uint32_t ColorHigh
    )
{
    n2d_error_t error;

    /* LoadState global color value. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_DEST_COLOR_KEY_Address,
        ColorLow
        ));

    /* LoadState global color value. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_DEST_COLOR_KEY_HIGH_Address,
        ColorHigh
        ));

on_error:
    return error;
}

n2d_error_t gcSetTargetGlobalColor(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t Color
    )
{
    n2d_error_t error = N2D_SUCCESS;

    /* LoadState global color value. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_GLOBAL_DEST_COLOR_Address,
        Color
        ));

on_error:
    return error;
}

n2d_error_t gcTranslateTargetTransparency(
    IN n2d_transparency_t APIValue,
    OUT n2d_uint32_t * HwValue
    )
{
    /* Dispatch on transparency. */
    switch (APIValue)
    {
#ifdef AQPE_TRANSPARENCY_Address
    case N2D_OPAQUE:
        *HwValue = AQPE_TRANSPARENCY_DESTINATION_OPAQUE;
        break;

    case N2D_MASKED:
        *HwValue = AQPE_TRANSPARENCY_DESTINATION_KEY;
        break;
#endif

    default:
        /* Not supported. */

        return N2D_NOT_SUPPORTED;
    }

    /* Success. */

    return N2D_SUCCESS;
}

/* Load dst cscMode state. */
n2d_error_t gcLoadDstCSCMode(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_csc_mode_t Mode,
    IN n2d_int32_t * CscRGB2YUV,
    OUT n2d_uint32_t* swizzle
    )
{
    n2d_error_t error = N2D_SUCCESS;

    switch (Mode)
    {
    case N2D_CSC_BT601:
        *swizzle = gcmSETFIELDVALUE(*swizzle, GCREG_DEST_CONFIG_EX, YUV, 601);
        break;

    case N2D_CSC_BT709:
        *swizzle = gcmSETFIELDVALUE(*swizzle, GCREG_DEST_CONFIG_EX, YUV, 709);
        break;

    case N2D_CSC_BT2020:
        *swizzle = gcmSETFIELDVALUE(*swizzle, GCREG_DEST_CONFIG_EX, YUV, 2020);
        break;

    case N2D_CSC_USER_DEFINED_CLAMP:
        *swizzle = gcmSETFIELDVALUE(*swizzle, GCREG_DEST_CONFIG_EX, YUV_CLAMP, ENABLED);
        *swizzle = gcmSETFIELDVALUE(*swizzle, GCREG_DEST_CONFIG_EX, MASK_YUV_CLAMP, ENABLED);
        /*no need break;*/
	    fallthrough;

    case N2D_CSC_USER_DEFINED:
	    *swizzle = gcmSETFIELDVALUE(*swizzle, GCREG_DEST_CONFIG_EX, YUV, UserDefined);

	    N2D_ON_ERROR(gcUploadCSCProgrammable(Hardware, N2D_FALSE, CscRGB2YUV));

	    break;

    default:
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

on_error:
    /* Return the error. */
    return error;
}

static n2d_error_t gcLoadMultiPlaneState(
    IN n2d_user_hardware_t  *Hardware,
    IN FormatInfo_t *FormatInfo,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_uint32_t *size
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t  planeBpp, planeStrideWidth;

    N2D_ASSERT(size != N2D_NULL);

    switch (FormatInfo->planeNum)
    {
        case N2D_THREEPLANES:
            /*dump 3rd plane*/
            N2D_DUMP_SURFACE(Surface->uv_memory[1], Surface->gpuAddress[2], size[2], N2D_FALSE, Hardware);
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_PLANE3_ADDRESS_Address,
                Surface->gpuAddress[2]));

            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_PLANE3_STRIDE_Address,
                Surface->stride[2]));

            planeBpp = gcmALIGN(FormatInfo->plane_bpp[2], 8) / 8;
            planeStrideWidth = Surface->stride[2] / planeBpp;
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_DST_PLANE2_ALIGN_CONFIG_Address,
                gcmSETFIELD(0, GCREG_DE_DST_PLANE0_ALIGN_CONFIG, STRIDE_WIDTH, planeStrideWidth)));
            /*Please don't add break*/
	        fallthrough;

        case N2D_TWOPLANES:
            /*dump 2nd plane*/
            N2D_DUMP_SURFACE(Surface->uv_memory[0], Surface->gpuAddress[1], size[1], N2D_FALSE, Hardware);
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_PLANE2_ADDRESS_Address,
                Surface->gpuAddress[1]));

            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_PLANE2_STRIDE_Address,
                Surface->stride[1]));

            planeBpp = gcmALIGN(FormatInfo->plane_bpp[1], 8) / 8;
            planeStrideWidth = Surface->stride[1] / planeBpp;

            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_DST_PLANE1_ALIGN_CONFIG_Address,
                gcmSETFIELD(0, GCREG_DE_DST_PLANE0_ALIGN_CONFIG, STRIDE_WIDTH, planeStrideWidth)));
            break;

        default:
            break;
        }
on_error:
    /* Return the error. */
    return error;
}

static n2d_error_t gcSetTargetTilingConfigDefault(
    IN gcsSURF_INFO_PTR     Surface,
    OUT n2d_uint32_t        *DestConfig)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t destConfig = 0;

    if (DestConfig == N2D_NULL)
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    /*Extract common configuration*/
    /*Disable this bit only when linear*/
    destConfig |= gcmSETFIELDVALUE(0,
                               AQDE_DEST_CONFIG,
                               TILED,
                               ENABLED);

    switch (Surface->tiling) {
        case N2D_LINEAR:
            destConfig = gcmSETFIELDVALUE(destConfig,
                                       AQDE_DEST_CONFIG,
                                       TILED,
                                       DISABLED);
            break;

        case N2D_TILED:
            destConfig |= gcmSETFIELDVALUE(0,
                               AQDE_DEST_CONFIG,
                               TILE_MODE,
                               TILED4X4
                               );
            break;

        case N2D_SUPER_TILED:
        case N2D_SUPER_TILED_128B:
        case N2D_SUPER_TILED_256B:
            destConfig = gcmSETFIELDVALUE(destConfig,
                                        AQDE_DEST_CONFIG,
                                        TILE_MODE,
                                        SUPER_TILED_XMAJOR
                                        );
            break;

        case N2D_YMAJOR_SUPER_TILED:
            destConfig = gcmSETFIELDVALUE(destConfig,
                                       AQDE_DEST_CONFIG,
                                       TILE_MODE,
                                       SUPER_TILED_YMAJOR
                                       );
            break;

        case N2D_TILED_8X8_XMAJOR:
            destConfig |=  gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE8X8_XMAJOR
                                       );
            break;

        case N2D_TILED_8X8_YMAJOR:
            if (Surface->format == N2D_NV12 || Surface->format == N2D_NV21) {
                destConfig |= gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE64X4
                                       );
            } else if (Surface->format == N2D_P010_MSB  || Surface->format == N2D_P010_LSB
                    || Surface->format == N2D_I010 || Surface->format == N2D_I010_LSB) {
                destConfig |= gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE32X4
                                       );
            }
            break;

        case N2D_TILED_8X4:
            destConfig |= gcmSETFIELDVALUE(0,
                               AQDE_DEST_CONFIG,
                               DEC400_TILE_MODE,
                               TILE8X4
                               );
            break;

        case N2D_TILED_4X8:
            destConfig |= gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE4X8
                                       );
            break;

        case N2D_TILED_32X4:
            destConfig |= gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE32X4
                                       );
            break;

        case N2D_TILED_64X4:
            destConfig |= gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE64X4
                                       );
            break;

        case N2D_MINOR_TILED:
            destConfig |= gcmSETFIELDVALUE(0, AQDE_DEST_CONFIG, TILED,       ENABLED)
                        | gcmSETFIELDVALUE(0, AQDE_DEST_CONFIG, MINOR_TILED, ENABLED);
            break;

        default:
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    *DestConfig |= destConfig;

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}

static n2d_error_t gcSetTargetTilingConfigWithDEC400Module(
    IN gcsSURF_INFO_PTR     Surface,
    OUT n2d_uint32_t        *DestConfig)
{
    n2d_error_t error;
    n2d_uint32_t destConfig = 0;

    if (DestConfig == N2D_NULL) {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    /*Extract common configuration*/
    /*Disable this bit only when linear*/
    destConfig |= gcmSETFIELDVALUE(0,
                               AQDE_DEST_CONFIG,
                               TILED,
                               ENABLED);

    /*non supertile X/Y major */
    if (!(Surface->tiling & N2D_SUPER_TILED)
        && ((Surface->tile_status_config & N2D_TSC_DEC_COMPRESSED) || gcIsYuv420SupportTileY(Surface->format))) {
       destConfig |= gcmSETFIELDVALUE(0,
                                  AQDE_DEST_CONFIG,
                                  TILE_MODE,
                                  DEC400
                                  );
    }

    switch (Surface->tiling) {
        case N2D_LINEAR:
            destConfig = gcmSETFIELDVALUE(destConfig,
                                       AQDE_DEST_CONFIG,
                                       TILED,
                                       DISABLED);
            break;

        case N2D_TILED:
            destConfig |= gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE4X4
                                       );
            break;

        case N2D_SUPER_TILED:
        case N2D_SUPER_TILED_128B:
        case N2D_SUPER_TILED_256B:
            destConfig = gcmSETFIELDVALUE(destConfig,
                                        AQDE_DEST_CONFIG,
                                        TILE_MODE,
                                        SUPER_TILED_XMAJOR
                                        );
            break;

        case N2D_YMAJOR_SUPER_TILED:
            destConfig = gcmSETFIELDVALUE(destConfig,
                                       AQDE_DEST_CONFIG,
                                       TILE_MODE,
                                       SUPER_TILED_YMAJOR
                                       );
            break;

        case N2D_TILED_8X8_XMAJOR:
            destConfig |=  gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE8X8_XMAJOR
                                       );
            break;

        case N2D_TILED_8X8_YMAJOR:
            if (Surface->format == N2D_NV12 || Surface->format == N2D_NV21) {
                destConfig |= gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE64X4
                                       );
            } else if (Surface->format == N2D_P010_MSB  || Surface->format == N2D_P010_LSB
                    || Surface->format == N2D_I010 || Surface->format == N2D_I010_LSB) {
                destConfig |= gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE32X4
                                       );
            }
            break;

        case N2D_TILED_8X4:
            destConfig |= gcmSETFIELDVALUE(0,
                               AQDE_DEST_CONFIG,
                               DEC400_TILE_MODE,
                               TILE8X4
                               );
            break;

        case N2D_TILED_4X8:
            destConfig |= gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE4X8
                                       );
            break;

        case N2D_TILED_32X4:
            destConfig |= gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE32X4
                                       );
            break;

        case N2D_TILED_64X4:
            destConfig |= gcmSETFIELDVALUE(0,
                                       AQDE_DEST_CONFIG,
                                       DEC400_TILE_MODE,
                                       TILE64X4
                                       );
            break;

        case N2D_MINOR_TILED:
            destConfig |= gcmSETFIELDVALUE(0, AQDE_DEST_CONFIG, MINOR_TILED, ENABLED);
            break;

        default:
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    *DestConfig |= destConfig;

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}

n2d_error_t gcSetTargetTilingConfig(
    IN n2d_user_hardware_t  *Hardware,
    IN gcsSURF_INFO_PTR     Surface,
    OUT n2d_uint32_t        *DestConfig)
{
    n2d_error_t error;

    if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION]) {
        N2D_ON_ERROR(gcSetTargetTilingConfigWithDEC400Module(Surface, DestConfig));
    } else {
        N2D_ON_ERROR(gcSetTargetTilingConfigDefault(Surface, DestConfig));
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}

static n2d_error_t gcTranslateTargetRotation(
    IN n2d_orientation_t APIValue,
    OUT n2d_uint32_t * HwValue
    )
{
    /* Dispatch on transparency. */
    switch (APIValue)
    {
    case N2D_0:
        *HwValue = AQDE_ROT_ANGLE_DST_ROT0;
        break;

    case N2D_90:
        *HwValue = AQDE_ROT_ANGLE_DST_ROT90;
        break;

    case N2D_180:
        *HwValue = AQDE_ROT_ANGLE_DST_ROT180;
        break;

    case N2D_270:
        *HwValue = AQDE_ROT_ANGLE_DST_ROT270;
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

static n2d_error_t gcSetDestinationExtensionFormat(
    IN n2d_uint32_t formatEx,
    IN OUT n2d_uint32_t *data
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t ret = *data;
    ret = gcmSETFIELD(ret, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, formatEx);

    *data = ret;

    return error;
}

n2d_error_t gcTranslateTargetFormat(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_buffer_format_t APIValue,
    IN n2d_bool_t EnableXRGB,
    OUT n2d_uint32_t* HwValue,
    OUT n2d_uint32_t* HwValueEx,
    OUT n2d_uint32_t* HwSwizzleValue
    )
{
    n2d_error_t error;
    n2d_uint32_t swizzle_argb, swizzle_rgba, swizzle_abgr, swizzle_bgra;
    n2d_uint32_t swizzle_uv, swizzle_vu, hwvalueex;

    Hardware->enableXRGB = EnableXRGB;

    swizzle_argb = AQDE_SRC_CONFIG_SWIZZLE_ARGB;
    swizzle_rgba = AQDE_SRC_CONFIG_SWIZZLE_RGBA;
    swizzle_abgr = AQDE_SRC_CONFIG_SWIZZLE_ABGR;
    swizzle_bgra = AQDE_SRC_CONFIG_SWIZZLE_BGRA;

    swizzle_uv = AQPE_CONTROL_UV_SWIZZLE_UV;
    swizzle_vu = AQPE_CONTROL_UV_SWIZZLE_VU;

    hwvalueex = AQDE_SRC_CONFIG_EX_EXTRA_SOURCE_FORMAT_RGB_F16F16F16_PLANAR;

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

    case N2D_RGB888:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_RGB888_PACKED;
        break;

    case N2D_BGR888:
        *HwValue = AQDE_SRC_CONFIG_SOURCE_FORMAT_RGB888_PACKED;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_ARGB8888I:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_INT8_PACKED;
        break;

    case N2D_RGB888I:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_INT8_PACKED;
        break;

    case N2D_ABGR8888I:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_INT8_PACKED;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_RGB888I_PLANAR:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_INT8_PLANAR;
        break;

    case N2D_ARGB16161616I:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_INT16_PACKED;
        break;

    case N2D_RGB161616I:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_INT16_PACKED;
        break;

    case N2D_ABGR16161616I:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_INT16_PACKED;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_RGB161616I_PLANAR:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_INT16_PLANAR;
        break;

    case N2D_ARGB16161616F:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_RGB_F16F16F16_PACKED;
        break;

    case N2D_RGB161616F:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_RGB_F16F16F16_PACKED;
        break;

    case N2D_ABGR16161616F:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_RGB_F16F16F16_PACKED;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_RGB161616F_PLANAR:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_RGB_F16F16F16_PLANAR;
        break;

    case N2D_ARGB32323232F:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_RGB_F32F32F32_PACKED;
        break;

    case N2D_RGB323232F:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_RGB_F32F32F32_PACKED;
        break;

    case N2D_ABGR32323232F:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_RGB_F32F32F32_PACKED;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case N2D_RGB323232F_PLANAR:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_RGB_F32F32F32_PLANAR;
        break;

    case N2D_GRAY8:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_GRAY_U8;
        break;

    case N2D_GRAY8I:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_GRAY_I8;
        break;

    case N2D_GRAY16F:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_GRAY_F16;
        break;

    case N2D_GRAY32F:
        *HwValue = AQDE_DEST_CONFIG_FORMAT_EXTENSION;
        hwvalueex = GCREG_DEST_CONFIG_EX_EXTRA_FORMAT_GRAY_F32;
        break;

    default:
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    *HwValueEx = hwvalueex;
    return N2D_SUCCESS;
on_error:
    return error;
}

static n2d_error_t gcSetUint8ToUint10ConversionMode(
     IN n2d_user_hardware_t* Hardware,
     OUT n2d_uint32_t* DataEx)
{
    n2d_uint32_t dataEx = *DataEx;
    n2d_error_t  error = N2D_SUCCESS;

    switch (Hardware->state.dest.u8Tu10_cvtMode)
    {
    case N2D_ADD_LOWER_BITS:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, CONVERSION_CONFIG, ADD_LOWER_BITS);
        break;
    case N2D_NOT_ADD_LOWER_BITS:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, CONVERSION_CONFIG, NOT_ADD_LOWER_BITS);
        break;
    case N2D_NOT_ADD_HIGHER_BITS:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, CONVERSION_CONFIG, NOT_ADD_HIGHER_BITS);
        break;
    default:
        error = N2D_INVALID_ARGUMENT;
        break;
    }

    *DataEx = dataEx;
    return error;
}

n2d_error_t gcSetTarget(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_bool_t Filter,
    IN n2d_csc_mode_t Mode,
    IN n2d_int32_t * CscRGB2YUV,
    IN n2d_uint32_t * GammaTable,
    IN n2d_bool_t GdiStretch,
    IN n2d_bool_t enableAlpha,
    OUT n2d_uint32_t * DestConfig
    )
{
    n2d_error_t error;
    n2d_uint32_t format, formatEx, swizzle, destConfig,strideWidth[3];
    n2d_uint32_t data[3], config = ~0U;
    n2d_uint32_t rotated = 0;
    n2d_bool_t  fullRot = 1;
    n2d_uint32_t fBPPs[3];
    n2d_uint32_t size[3] = {0};
    FormatInfo_t *FormatInfo = N2D_NULL;

    /* Check the rotation capability. */
    if (!fullRot && gcmGET_PRE_ROTATION(Surface->rotation) != N2D_0)
    {
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    if (fullRot)
    {
        rotated = N2D_FALSE;
    }
    else
    {
        /* Determine 90 degree rotation enable field. */
        if (gcmGET_PRE_ROTATION(Surface->rotation) == N2D_0)
        {
            rotated = N2D_FALSE;
        }
        else if (gcmGET_PRE_ROTATION(Surface->rotation) == N2D_90)
        {
            rotated = N2D_TRUE;
        }
        else
        {
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        }
    }

    N2D_ON_ERROR(gcGetSurfaceBufferSize(Hardware, Surface, N2D_TRUE, N2D_NULL, size));

    /*dump 1st plane*/
    N2D_DUMP_SURFACE(Surface->memory, Surface->gpuAddress[0], size[0], N2D_FALSE, Hardware);

    data[0] = Surface->gpuAddress[0];
    data[1] = Surface->stride[0];

    /* AQDE_DEST_ROTATION_CONFIG_Address */
    data[2]
        = gcmSETFIELD(0, AQDE_DEST_ROTATION_CONFIG, WIDTH,    Surface->alignedWidth)
        | gcmSETFIELD(0, AQDE_DEST_ROTATION_CONFIG, ROTATION, rotated);

    /* LoadState(AQDE_DEST_ADDRESS, 3), Address, Stride, rotation. */
    N2D_ON_ERROR(gcWriteRegWithLoadState(
        Hardware,
        AQDE_DEST_ADDRESS_Address, 3,
        data
        ));

    N2D_ON_ERROR(gcGetFormatInfo(Surface->format,&FormatInfo));

    N2D_ON_ERROR(gcQueryFBPPs(Surface->format, fBPPs));
    strideWidth[0] = Surface->alignedWidth;

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_PLANE0_ALIGN_CONFIG_Address,
        gcmSETFIELD(0, GCREG_DE_SRC_PLANE0_ALIGN_CONFIG, STRIDE_WIDTH, strideWidth[0])));

    if (fullRot)
    {
        n2d_uint32_t dstRot = 0;
        n2d_uint32_t value;

        N2D_ON_ERROR(gcTranslateTargetRotation(gcmGET_PRE_ROTATION(Surface->rotation), &dstRot));

        if (!Filter)
        {
            /* Flush the 2D pipe before writing to the rotation register. */
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                AQ_FLUSH_Address,
                gcmSETFIELDVALUE(0, AQ_FLUSH, PE2D_CACHE, ENABLE)
                ));
        }

        /* Load target height. */
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQDE_DST_ROTATION_HEIGHT_Address,
            gcmSETFIELD(0, AQDE_DST_ROTATION_HEIGHT, HEIGHT, Surface->alignedHeight)
            ));

        /* AQDE_ROT_ANGLE_Address */
        {
            /* Enable dst mask. */
            value = gcmSETFIELDVALUE(~0, AQDE_ROT_ANGLE, MASK_DST, ENABLED);

            /* Set dst rotation. */
            value = gcmSETFIELD(value, AQDE_ROT_ANGLE, DST, dstRot);
        }

        if (Hardware->features[N2D_FEATURE_2D_POST_FLIP])
        {
            /*If flip is added later,need to modify this configure */
            value = gcmSETFIELDVALUE(value,AQDE_ROT_ANGLE, DST_FLIP,NONE);

            value = gcmSETFIELDVALUE(value, AQDE_ROT_ANGLE, MASK_DST_FLIP, ENABLED);
        }

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware, AQDE_ROT_ANGLE_Address, value
            ));
    }

    /*******************************************************************
    ** Set the destination configuration register.
    */

    N2D_ON_ERROR(gcTranslateTargetFormat(
        Hardware, Surface->format, Hardware->enableXRGB, &format, &formatEx, &swizzle));

    /* Set endian control */
    destConfig = gcmSETFIELDVALUE(0,
                              AQDE_DEST_CONFIG,
                              ENDIAN_CONTROL,
                              NO_SWAP);

    data[0] = 0xFFFFF;

    if (N2D_FORMARTTYPE_YUV == FormatInfo->formatType || FormatInfo->planeNum > 1)
    {
        swizzle = gcmSETFIELD(data[0], GCREG_DEST_CONFIG_EX, UV_SWIZZLE, swizzle);
        swizzle = gcmSETFIELDVALUE(swizzle, GCREG_DEST_CONFIG_EX, MASK_UV_SWIZZLE, ENABLED);

        N2D_ON_ERROR(gcLoadDstCSCMode(Hardware, Mode,CscRGB2YUV,&swizzle));

        data[0] = gcmSETFIELDVALUE(swizzle, GCREG_DEST_CONFIG_EX, MASK_YUV, ENABLED);
    }

    if (FormatInfo->planeNum > 1)
    {
        N2D_ON_ERROR(gcLoadMultiPlaneState(Hardware, FormatInfo, Surface, size));
    }
    else
    {
        destConfig |= gcmSETFIELD(0, AQDE_DEST_CONFIG, SWIZZLE, swizzle);
    }

    destConfig |= gcmSETFIELD(0, AQDE_DEST_CONFIG, FORMAT, format);

    /* Set compression related config for target. */
    N2D_ON_ERROR(gcSetTargetCompression(Hardware, Surface,enableAlpha, &data[0]));

#ifdef N2D_SUPPORT_NORMALIZATION

    N2D_ON_ERROR(gcSetNormalizationState(Hardware, Surface, &destConfig, &data[0]));
#endif

    N2D_ON_ERROR(gcSetUint8ToUint10ConversionMode(Hardware, &data[0]));

#ifdef N2D_SUPPORT_HISTOGRAM

    N2D_ON_ERROR(gcSetHistogramState(Hardware));
    N2D_ON_ERROR(gcSetBrightnessAndSaturationState(Hardware));
#endif

    N2D_ON_ERROR(gcSetDestinationExtensionFormat(formatEx, &data[0]));

    if (data[0] != ~0U)
    {
        data[0] = gcmSETFIELDVALUE(data[0], GCREG_DEST_CONFIG_EX, ROUNDING_TO_NEAREST, ENABLE);
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware, GCREG_DEST_CONFIG_EX_Address, data[0]));
    }

    if (config != ~0U)
    {
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware, GCREG3RD_PARTY_COMPRESSION_DST_CONFIG_Address, config));
    }

    N2D_ON_ERROR(gcSetTargetTilingConfig(Hardware,Surface,&destConfig));

    N2D_ON_ERROR(gcSetTargetTileStatus(
        Hardware,
        Surface
        ));

    if (!Filter)
    {
        destConfig |= gcmSETFIELD(
                        0,
                        AQDE_DEST_CONFIG,
                        GDI_STRE,
                        GdiStretch ?
                            AQDE_DEST_CONFIG_GDI_STRE_ENABLED
                          : AQDE_DEST_CONFIG_GDI_STRE_DISABLED);
    }

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DEYUV_CONVERSION_Address,
        gcmSETFIELDVALUE(0, GCREG_DEYUV_CONVERSION, ENABLE, OFF)));

    if (GammaTable != N2D_NULL)
    {
        /* Reserve */
    }

    destConfig = gcmSETFIELDVALUE(
                        destConfig,
                        AQDE_DEST_CONFIG,
                        MONO_LINE_V2_DISABLED,
                        DISABLED);

    *DestConfig = destConfig;

on_error:
    /* Return the error. */
    return error;
}
