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
#include "series_common.h"

typedef enum n2d_planar
{
   N2D_DEC_PLANAR_ONE,
   N2D_DEC_PLANAR_TWO,
   N2D_DEC_PLANAR_THREE,
}
n2d_planar_t;

static n2d_bool_t gcDECIsOnePlanar(
    IN n2d_buffer_format_t Format
    )
{
    switch (Format)
    {
    case N2D_NV16:
    case N2D_NV12:
    case N2D_NV61:
    case N2D_NV21:
    case N2D_NV16_10BIT:
    case N2D_NV12_10BIT:
    case N2D_NV61_10BIT:
    case N2D_NV21_10BIT:
    case N2D_P010_MSB:
    case N2D_P010_LSB:
    case N2D_I420:
    case N2D_YV12:
    case N2D_I010:
    case N2D_I010_LSB:

        return N2D_FALSE;

    default:
        return N2D_TRUE;
    }
}

static n2d_uint32_t gcDECYUVFormatPlanar(
    IN n2d_buffer_format_t Format
    )
{
    switch (Format)
    {
    case N2D_YUYV:
    case N2D_UYVY:
        return 1;

    case N2D_NV16:
    case N2D_NV12:
    case N2D_NV61:
    case N2D_NV21:
    case N2D_NV16_10BIT:
    case N2D_NV12_10BIT:
    case N2D_NV61_10BIT:
    case N2D_NV21_10BIT:
    case N2D_P010_MSB:
    case N2D_P010_LSB:
        return 2;

    case N2D_I420:
    case N2D_YV12:
    case N2D_I010:
    case N2D_I010_LSB:
        return 3;

    default:
        return 0;
    }
}

// static n2d_error_t gcDECCheckSurface(
//     IN n2d_user_hardware_t *Hardware,
//     IN gcsSURF_INFO_PTR Surface
//     )
// {
//     n2d_error_t error = N2D_SUCCESS;

//     if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
//     {
//         n2d_uint32_t alignW = 1, alignH = 1;

//         if (Surface->tile_status_config & N2D_TSC_DEC_COMPRESSED)
//         {
//             switch (Surface->tiling)
//             {
//                 case N2D_TILED_8X8_XMAJOR:
//                     alignW = 16;
//                     alignH = 8;
//                     break;
//                 case N2D_TILED_8X4:
//                 case N2D_TILED_4X8:
//                     alignW = 8;
//                     alignH = 8;
//                     break;
//                 case N2D_TILED:
//                 case N2D_SUPER_TILED:
//                 case N2D_SUPER_TILED_128B:
//                 case N2D_SUPER_TILED_256B:
//                     alignW = 64;
//                     alignH = 64;
//                     break;
//                 case N2D_TILED_64X4:
//                     alignW = 64;
//                     alignH = 64;
//                     break;
//                 case N2D_TILED_32X4:
//                     alignW = 32;
//                     alignH = 64;
//                     break;
//                 default:
//                     N2D_ON_ERROR(N2D_NOT_SUPPORTED);
//             }
//         }
//         else if (Surface->tiling == N2D_TILED_8X8_YMAJOR)
//         {
//             if (Surface->format == N2D_NV12)
//             {
//                 alignW = 16;
//                 alignH = 64;
//             }
//             else if (Surface->format == N2D_P010_MSB || Surface->format == N2D_P010_LSB
//                     || Surface->format == N2D_I010 || Surface->format == N2D_I010_LSB)
//             {
//                 alignW = 8;
//                 alignH = 64;
//             }
//             else
//             {
//                 N2D_ON_ERROR(N2D_NOT_SUPPORTED);
//             }
//         }

//         if ((Surface->alignedWidth & (alignW - 1)) ||
//             (Surface->alignedHeight & (alignH - 1)))
//         {
//             N2D_ON_ERROR(N2D_NOT_ALIGNED);
//         }
//     }

// on_error:
//     return error;
// }

static n2d_uint32_t gcDECQueryStateCmdLen(
    IN gcs2D_State_PTR State,
    IN gce2D_COMMAND Command
    )
{
    n2d_uint32_t size;

    gcs2D_MULTI_SOURCE_PTR src;
    gcsSURF_INFO_PTR DstSurface = &State->dest.dstSurface;
    n2d_uint32_t i, srcMask = 0;

    size = 4;

    /* Dst command buffer size */
    if (DstSurface->tile_status_config & N2D_TSC_DEC_COMPRESSED)
    {
        size += 6 * 2;

        if (DstSurface->gpuAddress[1] && DstSurface->tile_status_buffer.gpu_addr[1])
            size += 6 * 2;

        if (DstSurface->gpuAddress[2] && DstSurface->tile_status_buffer.gpu_addr[2])
            size += 6 * 2;
    }
    else
    {
        size += 2 * 2;
    }

    if (Command == gcv2D_MULTI_SOURCE_BLT)
    {
        srcMask = State->srcMask;
    }
    else
    {
        srcMask = 1 << State->currentSrcIndex;
    }

    /* Src command buffer size */
    for (i = 0; i < gcdMULTI_SOURCE_NUM; i++)
    {
        if (!(srcMask & (1 << i)))
        {
            continue;
        }

        src = State->multiSrc + i;

        if (src->srcSurface.tile_status_config & N2D_TSC_DEC_COMPRESSED)
        {
            size += 10;

            if (src->srcSurface.gpuAddress[1] && src->srcSurface.tile_status_buffer.gpu_addr[1])
                size += 3 * 2;

            if (src->srcSurface.gpuAddress[2] && src->srcSurface.tile_status_buffer.gpu_addr[2])
                size += 3 * 2;
        }
        else
        {
            size += 6;

            if (gcDECYUVFormatPlanar(src->srcSurface.format))
                size += 2;
        }
    }

    return size;
}


/*******************************************************************************
**
**  gcGetCompressionCmdSize
**
**  Get command size of compression.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcoSURF SrcSurface
**          Pointer to the source surface object.
**
**      gcoSURF DstSurface
**          Pointer to the destination surface object.
**
**      n2d_uint32_t CompressNum
**          Compressed source surface number.
**
**      gce2D_COMMAND Command
**          2D command type.
**
**  OUTPUT:
**
**      n2d_uint32_t CmdSize
**          Command size.
*/
n2d_uint32_t gcGetCompressionCmdSize(
    IN n2d_user_hardware_t *Hardware,
    IN gcs2D_State_PTR State,
    IN gce2D_COMMAND Command
    )
{
    n2d_uint32_t size = 0;

    if (Hardware->features[N2D_FEATURE_DEC_COMPRESSION])
    {
        size = gcDECQueryStateCmdLen(
            State,
            Command);

        if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
        {
            size += 2 * gcdMULTI_SOURCE_NUM;
            /*reset 19 streams for read(16)/write(3) client*/
            size += (16 + 3) * 2 * 5;
        }
    }

    return size;
}

static n2d_error_t gcDEC400SetFormatConfig(
    IN n2d_buffer_format_t Format,
    IN n2d_planar_t Plane,
    IN n2d_bool_t IsRead,
    IN OUT n2d_uint32_t* Config
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t config = 0;

    N2D_ASSERT(Config != N2D_NULL);

    config = *Config;

    if (IsRead)
    {
        switch (Format)
        {
            case N2D_B5G5R5A1:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, A1RGB5);
                break;

            case N2D_B5G5R5X1:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, X1RGB5);
                break;

            case N2D_BGRA4444:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, ARGB4);
                break;

            case N2D_BGRX4444:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, XRGB4);
                break;

            case N2D_BGR565:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, R5G6B5);
                break;

            case N2D_BGRA8888:
            case N2D_ABGR8888:
            case N2D_RGBA8888:
            case N2D_ARGB8888:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, ARGB8);
                break;

            case N2D_BGRX8888:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, XRGB8);
                break;

            case N2D_A8:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, YUV_ONLY);
                break;

            case N2D_B10G10R10A2:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, A2R10G10B10);
                break;

            case N2D_UYVY:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, UYVY);
                break;

            case N2D_YUYV:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, YUY2);
                break;

            case N2D_I420:
            case N2D_YV12:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, YUV_ONLY);
                break;

            case N2D_NV12:
            case N2D_NV21:
            case N2D_NV16:
            case N2D_NV61:
            case N2D_NV12_10BIT:
            case N2D_NV21_10BIT:
            case N2D_NV16_10BIT:
            case N2D_NV61_10BIT:
            case N2D_P010_MSB:
            case N2D_P010_LSB:
                if (Plane == N2D_DEC_PLANAR_ONE)
                {
                    config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, YUV_ONLY);
                }
                else
                {
                    config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_FORMAT, UV_MIX);
                }
                break;

            default:
                N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        }
    }
    else /* Write */
    {
        switch (Format)
        {
            case N2D_B5G5R5A1:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, A1RGB5);
                break;

            case N2D_B5G5R5X1:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, X1RGB5);
                break;

            case N2D_BGRA4444:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, ARGB4);

                break;

            case N2D_BGRX4444:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, XRGB4);
                break;

            case N2D_BGR565:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, R5G6B5);
                break;

            case N2D_BGRA8888:
            case N2D_ABGR8888:
            case N2D_RGBA8888:
            case N2D_ARGB8888:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, ARGB8);
                break;

            case N2D_BGRX8888:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, XRGB8);
                break;

            case N2D_A8:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, YUV_ONLY);
                break;

            case N2D_B10G10R10A2:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, A2R10G10B10);
                break;

            case N2D_UYVY:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, UYVY);
                break;

            case N2D_YUYV:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, YUY2);
                break;

            case N2D_I420:
            case N2D_YV12:
                config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, YUV_ONLY);
                break;

            case N2D_NV12:
            case N2D_NV21:
            case N2D_NV16:
            case N2D_NV61:
            case N2D_NV12_10BIT:
            case N2D_NV21_10BIT:
            case N2D_NV16_10BIT:
            case N2D_NV61_10BIT:
                if (Plane == N2D_DEC_PLANAR_ONE)
                {
                    config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, YUV_ONLY);
                }
                else
                {
                    config = gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_FORMAT, UV_MIX);
                }
                break;

            default:
                N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        }
    }

on_error:
    *Config = config;
    return error;
}

static n2d_error_t gcDEC400SetTilingConfig(
    IN n2d_tiling_t Tiling,
    IN n2d_cache_mode_t CacheMode,
    IN n2d_buffer_format_t Format,
    IN n2d_planar_t Plane,
    IN n2d_bool_t IsRead,
    IN OUT n2d_uint32_t* Config
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t config = 0;

    if (Config == N2D_NULL)
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }
    else
    {
        n2d_uint32_t bitsPerPixel;
        N2D_ON_ERROR(gcConvertFormat(Format, &bitsPerPixel));
        config = *Config;

        switch (Tiling)
        {
            case N2D_LINEAR:
                if ((Format == N2D_NV12 || Format == N2D_NV21) && IsRead)
                {
                    if (Plane == N2D_DEC_PLANAR_ONE)
                    {
                        config = gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, RASTER256X1);
                    }
                    else
                    {
                        config = gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, RASTER128X1);
                    }
                }
                else if ((Format == N2D_P010_MSB || Format == N2D_P010_LSB) && IsRead)
                {
                    if (Plane == N2D_DEC_PLANAR_ONE)
                    {
                        config = gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, RASTER128X1);
                    }
                    else
                    {
                        config = gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, RASTER64X1);
                    }
                }
                else if (CacheMode == N2D_CACHE_128)
                {
                    if (bitsPerPixel == 16)
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, RASTER64X1) :
                                              gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, RASTER64X1);
                    }
                    else if (bitsPerPixel == 32)
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, RASTER32X1) :
                                           gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, RASTER32X1);
                    }
                }
                else if (CacheMode == N2D_CACHE_256)
                {
                    if (bitsPerPixel == 16)
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, RASTER128X1) :
                                              gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, RASTER128X1);
                    }
                    else if (bitsPerPixel == 32)
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, RASTER64X1) :
                                              gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, RASTER64X1);
                    }
                }
                else
                {
                    N2D_ON_ERROR(N2D_NOT_SUPPORTED);
                }
                break;

            case N2D_TILED:
                if ((Format == N2D_P010_MSB || Format == N2D_P010_LSB) && IsRead)
                {
                    if (Plane == N2D_DEC_PLANAR_ONE)
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE32X4) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE32X4);
                    }
                    else
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE16X4) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE16X4);
                    }
                }
                else if((Format == N2D_NV12 || Format == N2D_NV21) && IsRead)
                {
                    if (Plane == N2D_DEC_PLANAR_ONE)
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE64X4) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE64X4);
                    }
                    else
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE32X4) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE32X4);
                    }
                }
                else
                {
                    N2D_ON_ERROR(N2D_NOT_SUPPORTED);
                }
                break;
            case N2D_TILED_4X8:
                if (bitsPerPixel == 32)
                    config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE4X8) :
                                      gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE4X8);
                break;

            case N2D_TILED_8X4:
                if (bitsPerPixel == 32)
                    config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE8X4) :
                                      gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE8X4);
                break;

            case N2D_TILED_32X4:
                if ((Format == N2D_P010_MSB || Format == N2D_P010_LSB) && IsRead)
                {
                    if (Plane == N2D_DEC_PLANAR_ONE)
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE32X4) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE32X4);
                    }
                    else
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE16X4) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE16X4);
                    }
                }
                else
                {
                    N2D_ON_ERROR(N2D_NOT_SUPPORTED);
                }
                break;

            case N2D_TILED_64X4:
                if((Format == N2D_NV12 || Format == N2D_NV21) && IsRead)
                {
                    if (Plane == N2D_DEC_PLANAR_ONE)
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE64X4) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE64X4);
                    }
                    else
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE32X4) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE32X4);
                    }
                }
                else
                {
                    N2D_ON_ERROR(N2D_NOT_SUPPORTED);
                }
                break;

            case N2D_SUPER_TILED:
            case N2D_SUPER_TILED_128B:
            case N2D_SUPER_TILED_256B:
                if((bitsPerPixel == 16) || (bitsPerPixel == 32) || (Format == N2D_YUYV || Format == N2D_UYVY))
                {
                    if((CacheMode == N2D_CACHE_128) || (Tiling == N2D_SUPER_TILED_128B))
                    {
                        if(bitsPerPixel == 16)
                            config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE8X8_XMAJOR) :
                                              gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE8X8_XMAJOR);
                        else
                            config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE8X4) :
                                              gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE8X4);
                    }
                    else
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE8X8_XMAJOR) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE8X8_XMAJOR);
                }
                break;

            case N2D_YMAJOR_SUPER_TILED:
                if(bitsPerPixel == 32)
                {
                    if(CacheMode == N2D_CACHE_128)
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE4X8) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE4X8);
                    }
                    else
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE8X8_YMAJOR) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE8X8_YMAJOR);
                }
                break;

            case N2D_TILED_8X8_XMAJOR:
                if((bitsPerPixel == 16) ||  (bitsPerPixel == 32) || (Format == N2D_YUYV || Format == N2D_UYVY))
                {
                    if (Plane == N2D_DEC_PLANAR_ONE)
                    {
                        config = IsRead ? gcmSETFIELDVALUE(config,  GCREG_DEC400_READ_CONFIG, TILE_MODE, TILE8X8_XMAJOR) :
                                          gcmSETFIELDVALUE(config, GCREG_DEC400_WRITE_CONFIG, TILE_MODE, TILE8X8_XMAJOR);
                    }
                }
                else
                {
                    N2D_ON_ERROR(N2D_NOT_SUPPORTED);
                }
                break;

            default:
                N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        }
    }

    *Config = config;

on_error:
    return error;
}

static n2d_error_t gcDECUploadData(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_uint32_t Address,
    IN n2d_uintptr_t Data
    )
{
    n2d_error_t error = N2D_SUCCESS;

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        Address,
        (n2d_uint32_t)Data    // TODO, how to handle X64 Data address?
        ));

on_error:
    return error;
}

static n2d_error_t gcSetDEC400FastClearValue(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_uint32_t StreamRegOffset,
    IN n2d_uint32_t FastClearValue
    )
{
    n2d_error_t error = N2D_SUCCESS;

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DEC400_FAST_CLEAR_VALUE_Address + StreamRegOffset,
        FastClearValue
        ));

on_error:
    return error;
}

static n2d_error_t gcDECSetSrcDEC400Compression(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_tile_status_config_t tile_status_config,
    IN n2d_uint32_t ReadId
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_bool_t enable, onePlanar;
    n2d_buffer_format_t format ;
    n2d_uint32_t address,endAddress;
    n2d_uint32_t config = 0, configEx = 0;
    n2d_uint32_t regOffset[3] = {0}, size[3] = {0};

    enable = tile_status_config & N2D_TSC_DEC_COMPRESSED;
    onePlanar = gcDECIsOnePlanar(Surface->format);
    N2D_ON_ERROR(gcGetSurfaceBufferSize(Hardware, Surface,N2D_TRUE,N2D_NULL,size));

    if (enable)
    {
        if (( onePlanar && ReadId > 7) ||
            (!onePlanar && ReadId > 3))
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }
    }

    /* Non-DEC_Compression or Only DEC TPC DeTile */
    if (!enable)
    {
        regOffset[0] = (ReadId + 8)  << 2;
        /* UV of DEC400 use 1 2 3 6 */
        if (ReadId == 3)
            regOffset[1] = 6 << 2;
        else
            regOffset[1] = (ReadId + 1) << 2;

        /* Disable DEC 2D & 3D compression. */
        config = gcmSETFIELDVALUE(0, GCREG_DEC400_READ_CONFIG, COMPRESSION_ENABLE, DISABLE);

        N2D_ON_ERROR(gcDECUploadData(
        Hardware,
        GCREG_DEC400_READ_CONFIG_Address + regOffset[0],
        config
        ));

        if (ReadId < 4)
        {
            N2D_ON_ERROR(gcDECUploadData(
                Hardware,
                GCREG_DEC400_READ_CONFIG_Address + regOffset[1],
                config
                ));
        }

    }
    else
    {
        regOffset[0] = (ReadId + 8)  << 2;
        /* UV of DEC400 use 1 2 3 6 */
        if (ReadId == 3)
            regOffset[1] = 6 << 2;
        else
            regOffset[1] = (ReadId + 1) << 2;

        N2D_ON_ERROR(gcTranslateXRGBFormat(
            Hardware,
            Surface->format,
            &format
        ));

        config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_ENABLE, ENABLE);
        config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_SIZE, SIZE64_BYTE);

#if !NANO2D_COMPRESSION_DEC400_ALIGN_MODE
        config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_ALIGN_MODE, ALIGN16_BYTE);
#elif NANO2D_COMPRESSION_DEC400_ALIGN_MODE == 1
        config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_ALIGN_MODE, ALIGN32_BYTE);
#elif NANO2D_COMPRESSION_DEC400_ALIGN_MODE == 2
        config = gcmSETFIELDVALUE(config, GCREG_DEC400_READ_CONFIG, COMPRESSION_ALIGN_MODE, ALIGN64_BYTE);
#endif

        N2D_ON_ERROR(gcDEC400SetFormatConfig(format, N2D_DEC_PLANAR_ONE, N2D_TRUE, &config));

        N2D_ON_ERROR(gcDEC400SetTilingConfig(
            Surface->tiling,
            Surface->cacheMode,
            format,
            N2D_DEC_PLANAR_ONE,
            N2D_TRUE,
            &config));

        if (format == N2D_P010_MSB || format == N2D_P010_LSB)
        {
            configEx = gcmSETFIELDVALUE(0, GCREG_DEC400_READ_EX_CONFIG, BIT_DEPTH,  BIT10) |
                              gcmSETFIELDVALUE(0, GCREG_DEC400_READ_EX_CONFIG, INTEL_P010, ENABLE);
        }
        else
        {
            configEx = gcmSETFIELDVALUE(0, GCREG_DEC400_READ_EX_CONFIG, BIT_DEPTH, BIT8);
        }

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_EX_CONFIG_Address + regOffset[0],
            configEx
        ));

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_CONFIG_Address + regOffset[0],
            config
        ));

        address = Surface->gpuAddress[0];

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_BUFFER_BASE_Address + regOffset[0],
            address
        ));

        endAddress = address + size[0] - 1;
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_BUFFER_END_Address + regOffset[0],
            endAddress
        ));

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_CACHE_BASE_Address + regOffset[0],
            Surface->tile_status_buffer.gpu_addr[0]
        ));

        if (Surface->tile_status_buffer.enableFastClear && Hardware->features[N2D_FEATURE_DEC400_FC])
        {
            N2D_ON_ERROR(gcSetDEC400FastClearValue(Hardware,regOffset[0],Surface->tile_status_buffer.fastClearValue));
        }

        if (!onePlanar && Surface->gpuAddress[1] && Surface->tile_status_buffer.gpu_addr[1])
        {
            N2D_ON_ERROR(gcDEC400SetFormatConfig(format, N2D_DEC_PLANAR_TWO, N2D_TRUE, &config));
            N2D_ON_ERROR(gcDEC400SetTilingConfig(
                Surface->tiling,
                Surface->cacheMode,
                format,
                N2D_DEC_PLANAR_TWO,
                N2D_TRUE,
                &config));

            if (format == N2D_P010_MSB || format == N2D_P010_LSB)
            {
                configEx = gcmSETFIELDVALUE(0, GCREG_DEC400_READ_EX_CONFIG, BIT_DEPTH,  BIT10) |
                                  gcmSETFIELDVALUE(0, GCREG_DEC400_READ_EX_CONFIG, INTEL_P010, ENABLE);
            }
            else
            {
                 configEx = gcmSETFIELDVALUE(0, GCREG_DEC400_READ_EX_CONFIG, BIT_DEPTH, BIT8);
            }

            N2D_ON_ERROR(gcDECUploadData(
                Hardware,
                GCREG_DEC400_READ_EX_CONFIG_Address + regOffset[1],
                configEx
            ));

            N2D_ON_ERROR(gcDECUploadData(
                Hardware,
                GCREG_DEC400_READ_CONFIG_Address + regOffset[1],
                config
            ));

            address = Surface->gpuAddress[1];
            N2D_ON_ERROR(gcDECUploadData(
                Hardware,
                GCREG_DEC400_READ_BUFFER_BASE_Address + regOffset[1],
                address
            ));
            endAddress = address + size[1] - 1;
            N2D_ON_ERROR(gcDECUploadData(
                Hardware,
                GCREG_DEC400_READ_BUFFER_END_Address + regOffset[1],
                endAddress
           ));

            N2D_ON_ERROR(gcDECUploadData(
                Hardware,
                GCREG_DEC400_READ_CACHE_BASE_Address + regOffset[1],
                Surface->tile_status_buffer.gpu_addr[1]
            ));
        }
    }

on_error:
    return error;
}

n2d_error_t
gcDECSetDstDEC400Compression(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_tile_status_config_t tile_status_config,
    IN n2d_bool_t   enableAlpha
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_buffer_format_t format;
    n2d_uint32_t address,endAddress;
    n2d_bool_t enable, onePlanar;
    n2d_uint32_t configR = 0, configW = 0, configEx = 0;
    n2d_uint32_t regOffsetR[3]={0}, regOffsetW[3]={0},size[3] = {0};
    int itmp = 0;

    regOffsetR[0] = 0 << 2;
    regOffsetW[0] = 1 << 2;
    regOffsetW[1] = 3 << 2;

    N2D_ON_ERROR(gcTranslateXRGBFormat(
        Hardware,
        Surface->format,
        &format
        ));

    enable = tile_status_config & N2D_TSC_DEC_COMPRESSED;
    onePlanar = gcDECIsOnePlanar(Surface->format);

    N2D_ON_ERROR(gcGetSurfaceBufferSize(Hardware, Surface,N2D_TRUE,N2D_NULL,size));
    /* Dst only need to disable 2D DEC compression. */
    /* Set DST read client. */

    if(enable && enableAlpha)
    {
        configR = gcmSETFIELDVALUE(configR, GCREG_DEC400_READ_CONFIG, COMPRESSION_ENABLE, ENABLE);

        N2D_ON_ERROR(gcDEC400SetFormatConfig(format, N2D_DEC_PLANAR_ONE, N2D_TRUE, &configR));

        N2D_ON_ERROR(gcDEC400SetTilingConfig(
            Surface->tiling,
            Surface->cacheMode,
            format,
            N2D_DEC_PLANAR_ONE,
            N2D_TRUE,
            &configR));

        if (format == N2D_P010_MSB || format == N2D_P010_LSB)
        {
            configEx = gcmSETFIELDVALUE(0, GCREG_DEC400_READ_EX_CONFIG, BIT_DEPTH,  BIT10) |
                           gcmSETFIELDVALUE(0, GCREG_DEC400_READ_EX_CONFIG, INTEL_P010, ENABLE);
        }
        else
        {
            configEx = gcmSETFIELDVALUE(0, GCREG_DEC400_READ_EX_CONFIG, BIT_DEPTH, BIT8);
        }

        configR = gcmSETFIELDVALUE(configR, GCREG_DEC400_READ_CONFIG, COMPRESSION_SIZE, SIZE64_BYTE);

#if !NANO2D_COMPRESSION_DEC400_ALIGN_MODE
        configR = gcmSETFIELDVALUE(configR, GCREG_DEC400_READ_CONFIG, COMPRESSION_ALIGN_MODE, ALIGN16_BYTE);
#elif NANO2D_COMPRESSION_DEC400_ALIGN_MODE == 1
        configR = gcmSETFIELDVALUE(configR, GCREG_DEC400_READ_CONFIG, COMPRESSION_ALIGN_MODE, ALIGN32_BYTE);
#elif NANO2D_COMPRESSION_DEC400_ALIGN_MODE == 2
        configR = gcmSETFIELDVALUE(configR, GCREG_DEC400_READ_CONFIG, COMPRESSION_ALIGN_MODE, ALIGN64_BYTE);
#endif

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_CONFIG_Address + regOffsetR[0],
            configR
        ));

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_EX_CONFIG_Address + regOffsetR[0],
            configEx
        ));

        address = Surface->gpuAddress[0];

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_BUFFER_BASE_Address + regOffsetR[0],
            address
        ));

        endAddress = address + size[0] - 1;
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_BUFFER_END_Address + regOffsetR[0],
            endAddress
        ));

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_CACHE_BASE_Address + regOffsetR[0],
            Surface->tile_status_buffer.gpu_addr[0]
        ));

        if (Surface->tile_status_buffer.enableFastClear && Hardware->features[N2D_FEATURE_DEC400_FC])
        {
            N2D_ON_ERROR(gcSetDEC400FastClearValue(Hardware,regOffsetR[0],Surface->tile_status_buffer.fastClearValue));
        }

    }

    /* Set DST write client. */
    if(enable)
    {
        configW = gcmSETFIELDVALUE(configW, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_ENABLE, ENABLE);

        N2D_ON_ERROR(gcDEC400SetFormatConfig(format, N2D_DEC_PLANAR_ONE, N2D_FALSE, &configW));

        N2D_ON_ERROR(gcDEC400SetTilingConfig(
            Surface->tiling,
            Surface->cacheMode,
            format,
            N2D_DEC_PLANAR_ONE,
            N2D_FALSE, &configW));

        if (format == N2D_P010_MSB || format == N2D_P010_LSB)
        {
            configEx = gcmSETFIELDVALUE(0, GCREG_DEC400_WRITE_EX_CONFIG, BIT_DEPTH,  BIT10) |
                       gcmSETFIELDVALUE(0, GCREG_DEC400_WRITE_EX_CONFIG, INTEL_P010, ENABLE);
        }
        else
        {
            configEx = gcmSETFIELDVALUE(0, GCREG_DEC400_WRITE_EX_CONFIG, BIT_DEPTH, BIT8);
        }

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_EX_CONFIG_Address + regOffsetW[0],
            configEx
            ));

        configW = gcmSETFIELDVALUE(configW, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_SIZE, SIZE64_BYTE);
        itmp =NANO2D_COMPRESSION_DEC400_ALIGN_MODE;
#if !NANO2D_COMPRESSION_DEC400_ALIGN_MODE
        configW = gcmSETFIELDVALUE(configW, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_ALIGN_MODE, ALIGN16_BYTE);
#elif NANO2D_COMPRESSION_DEC400_ALIGN_MODE == 1
        configW = gcmSETFIELDVALUE(configW, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_ALIGN_MODE, ALIGN32_BYTE);
#elif NANO2D_COMPRESSION_DEC400_ALIGN_MODE == 2
        configW = gcmSETFIELDVALUE(configW, GCREG_DEC400_WRITE_CONFIG, COMPRESSION_ALIGN_MODE, ALIGN64_BYTE);
#endif

        address = Surface->gpuAddress[0];
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_BUFFER_BASE_Address + regOffsetW[0],
            address
            ));

        endAddress = address + size[0] - 1;
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_BUFFER_END_Address + regOffsetW[0],
            endAddress
            ));

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_CACHE_BASE_Address + regOffsetW[0],
            Surface->tile_status_buffer.gpu_addr[0]
            ));

        N2D_ON_ERROR(gcDECUploadData(
        Hardware,
        GCREG_DEC400_WRITE_CONFIG_Address + regOffsetW[0],
        configW
        ));
    }

    /*planar 2*/
    if (enable && !onePlanar && Surface->gpuAddress[1] && Surface->tile_status_buffer.gpu_addr[1])
    {
        N2D_ON_ERROR(gcDEC400SetFormatConfig(format, N2D_DEC_PLANAR_TWO, N2D_TRUE, &configW));

        N2D_ON_ERROR(gcDEC400SetTilingConfig(
            Surface->tiling,
            Surface->cacheMode,
            format,
            N2D_DEC_PLANAR_TWO,
            N2D_FALSE,
            &configW));

        if (format == N2D_P010_MSB || format == N2D_P010_LSB)
        {
            configEx = gcmSETFIELDVALUE(0, GCREG_DEC400_WRITE_EX_CONFIG, BIT_DEPTH,  BIT10) |
                       gcmSETFIELDVALUE(0, GCREG_DEC400_WRITE_EX_CONFIG, INTEL_P010, ENABLE);
        }
        else
        {
            configEx = gcmSETFIELDVALUE(0, GCREG_DEC400_WRITE_EX_CONFIG, BIT_DEPTH, BIT8);
        }

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_EX_CONFIG_Address + regOffsetW[1],
            configEx
            ));

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_CONFIG_Address + regOffsetW[1],
            configW
            ));

        address = Surface->gpuAddress[1];
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_BUFFER_BASE_Address + regOffsetW[1],
            address
            ));

        endAddress = address + size[1] - 1;
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_BUFFER_END_Address + regOffsetW[1],
            endAddress
            ));

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_CACHE_BASE_Address + regOffsetW[1],
            Surface->tile_status_buffer.gpu_addr[1]
            ));

    }

on_error:
    return error;
}

static n2d_error_t gcDESetSrcDECCompression(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_tile_status_config_t tile_status_config,
    IN n2d_uint32_t ReadId
    )
{
    n2d_error_t error = N2D_SUCCESS;
    if(Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
    {
        N2D_ON_ERROR(gcDECSetSrcDEC400Compression(Hardware,Surface,tile_status_config, ReadId));
    }

on_error:
    return error;
}

static n2d_error_t gcDECSetDstDECCompression(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_tile_status_config_t tile_status_config,
    IN n2d_bool_t enableAlpha
    )
{
    n2d_error_t error = N2D_SUCCESS;

    if(Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
    {
        N2D_ON_ERROR(gcDECSetDstDEC400Compression(Hardware,Surface,tile_status_config,enableAlpha));
    }

on_error:
        return error;

}

static n2d_error_t gcDECEnableDEC400Compression(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_bool_t Enable
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t config;

    config = gcmSETFIELDVALUE(GCREG_DEC400_CONTROL_ResetValue, GCREG_DEC400_CONTROL, CONFIGURE_MODE, LOAD_STATE);

    if (Enable)
    {
        config = gcmSETFIELDVALUE(config, GCREG_DEC400_CONTROL, DISABLE_COMPRESSION, DISABLE);
        /*flush dec tilsstatus cache when enable dec*/
        config |= gcmSETFIELDVALUE(config, GCREG_DEC400_CONTROL, FLUSH, ENABLE);
    }
    else
    {
        config = gcmSETFIELDVALUE(config, GCREG_DEC400_CONTROL, DISABLE_COMPRESSION, ENABLE);
    }
    config = gcmSETFIELDVALUE(config, GCREG_DEC400_CONTROL, DISABLE_HW_FLUSH, DISABLE);

    N2D_ON_ERROR(gcDECUploadData(
        Hardware,
        GCREG_DEC400_CONTROL_Address,
        config
        ));

on_error:
    return error;
}

static n2d_error_t gcDECEnableDECCompression(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_bool_t Enable
    )
{
    n2d_error_t error = N2D_SUCCESS;

    if(Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
        N2D_ON_ERROR(gcDECEnableDEC400Compression(Hardware,Enable));

on_error:
    return error;
}

/*******************************************************************************
**
**  gcSetCompression
**
**  Configure the source tile error.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**  OUTPUT:
**
**      None
*/
n2d_error_t gcSetCompression(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_uint32_t SrcCompress,
    IN n2d_bool_t DstCompress
    )
{
    n2d_error_t error = N2D_SUCCESS;

    if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
    {
        n2d_uint32_t config = ~0U;

        /* Always enable DC_COMPRESSION because control it inside. */
        config = gcmSETMASKEDFIELDVALUE(GCREG_DE_GENERAL_CONFIG, BY_PASS_DC_COMPRESSION, DISABLED);

        if (SrcCompress || DstCompress)
        {
            N2D_ON_ERROR(gcDECEnableDECCompression(Hardware, N2D_TRUE));
        }
        else
        {
            N2D_ON_ERROR(gcDECEnableDECCompression(Hardware, N2D_FALSE));
        }

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GCREG_DE_GENERAL_CONFIG_Address,
            config
            ));
    }

    Hardware->hw2DCurrentRenderCompressed = SrcCompress || DstCompress;

on_error:
    return error;
}


/*******************************************************************************
**
**  _SetSourceCompression
**
**  Configure compression related for source.
**
**  INPUT:
**
**      n2d_user_hardware_t Hardware
**          Pointer to an n2d_user_hardware_t object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer to the destination surface object.
**
**      n2d_uint32_t_PTR Index
**          Surface index in source group.
**
**      n2d_bool_t MultiSrc
**          Multisrc blit or not.
**
**      n2d_uint32_t_PTR Config
**          Pointer to the original config.
**
**  OUTPUT:
**
**      error.
*/
n2d_error_t gcSetSourceCompression(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_uint32_t Index,
    IN n2d_bool_t MultiSrc,
    IN OUT n2d_uint32_t *Config
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t config = 0, size[2] = {0};

    if (Config != N2D_NULL)
        config = *Config;

    if (Hardware->features[N2D_FEATURE_DEC_COMPRESSION])
    {
        if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
        {
            if (Surface->tile_status_config & N2D_TSC_DEC_COMPRESSED)
            {
                if (!MultiSrc)
                {
                    config = gcmSETFIELDVALUE(config, AQDE_SRC_EX_CONFIG, DC_COMPRESSION, ENABLED);
                    config = gcmSETFIELDVALUE(config, AQDE_SRC_EX_CONFIG, MASK_DC_COMPRESSION, ENABLED);
                }
                else
                {
                    config = gcmSETFIELDVALUE(config, GCREG_DE_SRC_EX_CONFIG_EX, DC_COMPRESSION, ENABLED);
                    config = gcmSETFIELDVALUE(config, GCREG_DE_SRC_EX_CONFIG_EX, MASK_DC_COMPRESSION, ENABLED);
                }
            }
            else
            {
                if (!MultiSrc)
                {
                    config = gcmSETFIELDVALUE(config, AQDE_SRC_EX_CONFIG, DC_COMPRESSION, DISABLED);
                    config = gcmSETFIELDVALUE(config, AQDE_SRC_EX_CONFIG, MASK_DC_COMPRESSION, ENABLED);
                }
                else
                {
                    config = gcmSETFIELDVALUE(config, GCREG_DE_SRC_EX_CONFIG_EX, DC_COMPRESSION, DISABLED);
                    config = gcmSETFIELDVALUE(config, GCREG_DE_SRC_EX_CONFIG_EX, MASK_DC_COMPRESSION, ENABLED);
                }
            }
        }

        N2D_ON_ERROR(gcDESetSrcDECCompression(
            Hardware,
            Surface,
            Surface->tile_status_config,
            Index
            ));

        if (Surface->tile_status_config & N2D_TSC_DEC_COMPRESSED)
        {
            N2D_ON_ERROR(gcGetSurfaceTileStatusBufferSize(Surface, size));
            /*dump tile status 1st plane*/
            N2D_DUMP_SURFACE(Surface->tile_status_buffer.memory[0],
                Surface->tile_status_buffer.gpu_addr[0], size[0], N2D_TRUE, Hardware);
            if(Surface->tile_status_buffer.memory[1])
            {
                /*dump tile status 2nd plane*/
                N2D_DUMP_SURFACE(Surface->tile_status_buffer.memory[1],
                    Surface->tile_status_buffer.gpu_addr[1], size[1], N2D_TRUE, Hardware);
            }
        }

    }

    if (Config != N2D_NULL)
        *Config = config;

on_error:
    return error;
}



/*******************************************************************************
**
**  _SetTargetCompression
**
**  Configure compression related for destination.
**
**  INPUT:
**
**      n2d_user_hardware_t Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcoSURF Surface
**          Pointer to the destination surface descriptor.
**
**      n2d_uint32_t_PTR Config
**          Pointer to the original config.
**
**  OUTPUT:
**
**      error.
*/
n2d_error_t gcSetTargetCompression(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_bool_t EnableAlpha,
    IN OUT n2d_uint32_t *Config
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t config = 0,size[2] = {0};

    if (Config != N2D_NULL)
        config = *Config;

    if (Hardware->features[N2D_FEATURE_DEC_COMPRESSION])
    {
        if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
        {

            if(Surface->tile_status_config & N2D_TSC_DEC_COMPRESSED)
            {
                config = gcmSETFIELDVALUE(config, GCREG_DEST_CONFIG_EX, DC_COMPRESSION, ENABLED);
                config = gcmSETFIELDVALUE(config, GCREG_DEST_CONFIG_EX, MASK_DC_COMPRESSION, ENABLED);
            }
            else
            {
                config = gcmSETFIELDVALUE(config, GCREG_DEST_CONFIG_EX, DC_COMPRESSION, DISABLED);
                config = gcmSETFIELDVALUE(config, GCREG_DEST_CONFIG_EX, MASK_DC_COMPRESSION, ENABLED);
            }

        }

        N2D_ON_ERROR(gcDECSetDstDECCompression(
                    Hardware,
                    Surface,
                    Surface->tile_status_config,
                    EnableAlpha
                    ));

        if (Surface->tile_status_config & N2D_TSC_DEC_COMPRESSED)
        {
            N2D_ON_ERROR(gcGetSurfaceTileStatusBufferSize(Surface, size));
            /*dump tile status 1st plane*/
            N2D_DUMP_SURFACE(Surface->tile_status_buffer.memory[0],
                Surface->tile_status_buffer.gpu_addr[0], size[0], N2D_FALSE, Hardware);
            if(Surface->tile_status_buffer.memory[1])
            {
                /*dump tile status 2nd plane*/
                N2D_DUMP_SURFACE(Surface->tile_status_buffer.memory[1],
                    Surface->tile_status_buffer.gpu_addr[1], size[1], N2D_FALSE, Hardware);
            }
        }

    }

    if (Config != N2D_NULL)
        *Config = config;

on_error:

    return error;
}

n2d_error_t gcDECResetDEC400Stream(
    IN n2d_user_hardware_t *Hardware
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t config,configEx;
    n2d_uint32_t iter = 0;
    n2d_uint32_t regOffsetR[3]={0}, regOffsetW[3]={0};
    if(!Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
        return error;

    /*reset read config*/
    config = GCREG_DEC400_READ_CONFIG_ResetValue;
    configEx = GCREG_DEC400_READ_EX_CONFIG_ResetValue;
    regOffsetR[0] = 0 << 2;
    regOffsetW[0] = 1 << 2;
    regOffsetW[1] = 3 << 2;

    for(iter = 0; iter < 16; iter ++)
    {
        if(iter == 0)
            continue;
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_CONFIG_Address + (iter << 2),
            config
        ));
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_EX_CONFIG_Address + (iter << 2),
            configEx
        ));
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_BUFFER_BASE_Address + (iter << 2),
            GCREG_DEC400_READ_BUFFER_BASE_ResetValue
        ));
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_BUFFER_END_Address + (iter << 2),
            GCREG_DEC400_READ_BUFFER_END_ResetValue
        ));
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_READ_CACHE_BASE_Address + (iter << 2),
            GCREG_DEC400_READ_CACHE_BASE_ResetValue
        ));
    }

    /*reset write stream*/
    N2D_ON_ERROR(gcDECUploadData(
        Hardware,
        GCREG_DEC400_READ_CONFIG_Address + regOffsetR[0],
        GCREG_DEC400_READ_CONFIG_ResetValue
    ));
    N2D_ON_ERROR(gcDECUploadData(
        Hardware,
        GCREG_DEC400_READ_EX_CONFIG_Address + regOffsetR[0],
        GCREG_DEC400_READ_EX_CONFIG_ResetValue
    ));
    N2D_ON_ERROR(gcDECUploadData(
        Hardware,
        GCREG_DEC400_READ_BUFFER_BASE_Address + regOffsetR[0],
        GCREG_DEC400_READ_BUFFER_BASE_ResetValue
    ));
    N2D_ON_ERROR(gcDECUploadData(
        Hardware,
        GCREG_DEC400_READ_BUFFER_END_Address + regOffsetR[0],
        GCREG_DEC400_READ_BUFFER_END_ResetValue
    ));
    N2D_ON_ERROR(gcDECUploadData(
        Hardware,
        GCREG_DEC400_READ_CACHE_BASE_Address + regOffsetR[0],
        GCREG_DEC400_READ_CACHE_BASE_ResetValue
    ));

    for(iter = 0; iter < 2; iter ++)
    {
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_CONFIG_Address + regOffsetW[iter],
            GCREG_DEC400_WRITE_CONFIG_ResetValue
        ));
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_EX_CONFIG_Address + regOffsetW[iter],
            GCREG_DEC400_WRITE_EX_CONFIG_ResetValue
        ));
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_BUFFER_BASE_Address + regOffsetW[iter],
            GCREG_DEC400_WRITE_BUFFER_BASE_ResetValue
        ));
        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_BUFFER_END_Address + regOffsetW[iter],
            GCREG_DEC400_WRITE_BUFFER_END_ResetValue
        ));

        N2D_ON_ERROR(gcDECUploadData(
            Hardware,
            GCREG_DEC400_WRITE_CACHE_BASE_Address + regOffsetW[iter],
            GCREG_DEC400_WRITE_CACHE_BASE_ResetValue
        ));
    }

on_error:

    return error;

}

