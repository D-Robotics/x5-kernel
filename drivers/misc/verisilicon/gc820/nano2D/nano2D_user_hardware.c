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
#include "nano2D_dispatch.h"
#include "nano2D_user_os.h"
#include "nano2D_user_hardware.h"
#include "nano2D_feature_database.h"
#include "nano2D_user_hardware_filter_blt_blocksize.h"
#include "nano2D_user_hardware_dec.h"
#include "nano2D_user_hardware_source.h"
#include "nano2D_user_hardware_target.h"
#include "series_common.h"


static n2d_uint32_t g_FlushCacheCmd[] =
{
    0x08050480, 0x00000000,
    0x00000020, 0x00000010,
    0x04000004, 0x00000000,
    0x0804048a, 0x00000000,
    0x00000020, 0x00000010,
    0x00002004, 0x00000000,
    0x08030497, 0x0030cccc,
    0x00000000, 0x00100002,
    0x0801049f, 0x00000000,
    0x080104af, 0x00000cc0,
    0x080204b4, 0x00000000,
    0x00000000, 0x00000000,
    0x20000100, 0x00000000,
    0x00000000, 0x00010001
};

void gcTrace(const char *format, ...)
{
    // static char buffer[256];
    // va_list args;
    // va_start(args, format);

    // vsnprintf(buffer, 255, format, args);
    // buffer[255] = 0;

    // printf("%s", buffer);
}

static n2d_error_t gcFlushAppendCache(
    IN n2d_user_hardware_t * hardware)
{
    n2d_error_t error;
    n2d_buffer_t *buffer = &hardware->hw2DCacheFlushBuffer;
    n2d_uint32_t i;
    n2d_uint32_t *cmd_buf = (n2d_uint32_t *)hardware->cmdLogical;

    if ((buffer->width == 0) && (buffer->height == 0))
    {
        buffer->width       = 32;
        buffer->height      = 4;
        buffer->format      = N2D_RGB565;
        buffer->orientation = N2D_0;

        N2D_ON_ERROR(n2d_allocate(buffer));

        n2d_user_os_memory_fill(buffer->memory, 0, buffer->stride * buffer->height);

        g_FlushCacheCmd[1] = buffer->gpu;
        g_FlushCacheCmd[7] = g_FlushCacheCmd[1] + 0x40;
    }

    for (i = 0; i < N2D_COUNTOF(g_FlushCacheCmd); i++)
    {
        cmd_buf[hardware->cmdIndex++] = g_FlushCacheCmd[i];
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

static n2d_error_t gcFilFeatureDataBase(IN n2d_user_hardware_t * hardware)
{
    n2d_bool_t      *features       = hardware->features;
    n2d_uint32_t    chip_id         = hardware->chipModel;
    n2d_uint32_t    chip_version    = hardware->chipRevision;
    n2d_uint32_t    product_id      = hardware->productId;
    n2d_uint32_t    cid             = hardware->cid;

    n2d_feature_database *database = query_features(chip_id, chip_version, product_id, cid);
    n2d_user_os_memory_fill(features,0,sizeof(n2d_bool_t) * (N2D_FEATURE_COUNT - 1));
    if(!database)
    {
        /*if not match ,use hardware default parameter*/
        gcTrace("Not matching features,use hardware default parameter!\n");
        gcTrace("This Hw is chip_id/chip_version/product_id/cid = (0x%x/0x%x/0x%x/0x%x)\n",chip_id,chip_version,product_id,cid);
        return N2D_NOT_SUPPORTED;
    }

    features[N2D_FEATURE_YUV420_OUTPUT]             = database->N2D_YUV420_OUTPUT;
    features[N2D_FEATURE_2D_10BIT_OUTPUT_LINEAR]    = database->PE2D_LINEAR_YUV420_10BIT;
    features[N2D_FEATURE_MAJOR_SUPER_TILE]          = database->PE2D_MAJOR_SUPER_TILE;

    features[N2D_FEATURE_DEC400_COMPRESSION]        = database->G2D_DEC400EX;
    features[N2D_FEATURE_DEC400_FC]                 = database->N2D_FEATURE_DEC400_FC;

    features[N2D_FEATURE_ANDROID_ONLY]              = database->REG_AndroidOnly;
    features[N2D_FEATURE_2D_TILING]                 = database->REG_OnePass2DFilter || database->REG_DESupertile;
    features[N2D_FEATURE_2D_MINOR_TILING]           = database->REG_NewFeatures0;

    /*If increase the updated DEC, you should modify here*/
    features[N2D_FEATURE_DEC_COMPRESSION]           = features[N2D_FEATURE_DEC400_COMPRESSION];
    features[N2D_FEATURE_2D_MULTI_SOURCE_BLT]       =
    features[N2D_FEATURE_2D_POST_FLIP]              = database->REG_MultiSrcV2;
    features[N2D_FEATURE_BGR_PLANAR]                = database->RGB_PLANAR;
    features[N2D_FEATURE_SCALER]                    = database->REG_NoScaler == 0;
    features[N2D_FEATURE_2D_ONE_PASS_FILTER]        = database->REG_OnePass2DFilter;
    features[N2D_FEATURE_2D_OPF_YUV_OUTPUT]         = database->REG_DualPipeOPF;
    features[N2D_FEATURE_SEPARATE_SRC_DST]          = database->REG_SeperateSRCAndDstCache;

    features[N2D_FEATURE_AXI_FE]                    = database->N2D_FEATURE_AXI_FE;
    features[N2D_FEATURE_CSC_PROGRAMMABLE]          = database->N2D_FEATURE_CSC_PROGRAMMABLE;

    features[N2D_FEATURE_TRANSPARENCY_MASK]                 = database->N2D_FEATURE_MASK;
    features[N2D_FEATURE_TRANSPARENCY_COLORKEY]             = database->N2D_FEATURE_COLORKEY;

    hardware->hw2DBlockSize =
    hardware->hw2DQuad      = database->REG_DEEnhancements1 || database->REG_Compression2D;

    features[N2D_FEATURE_2D_ONE_PASS_FILTER]        |= hardware->hw2DQuad;

    features[N2D_FEATURE_2D_ALL_QUAD]               = hardware->hw2DQuad;

    features[N2D_FEATURE_NORMALIZATION]             = database->N2D_FEATURE_NORMALIZATION;
    features[N2D_FEATURE_NORMALIZATION_QUANTIZATION]= database->N2D_FEATURE_NORMALIZATION_QUANTIZATION;

    features[N2D_FEATURE_HISTOGRAM]                 = database->N2D_FEATURE_HISTOGRAM;
    features[N2D_FEATURE_BRIGHTNESS_SATURATION] = database->N2D_FEATURE_BRIGHTNESS_SATURATION;

    features[N2D_FEATURE_64BIT_ADDRESS]     = database->N2D_FEATURE_64BIT_ADDRESS;
    features[N2D_FEATURE_CONTEXT_ID]        = database->N2D_FEATURE_CONTEXT_ID;
    features[N2D_FEATURE_SECURE_BUFFER]     = database->N2D_FEATURE_SECURE_BUFFER;

    return N2D_SUCCESS;
}

static n2d_int32_t gcsValidRect(const gcsRECT_PTR Rect)
{
    return ((Rect->left   >= 0)
         && (Rect->top    >= 0)
         && (Rect->left   < Rect->right)
         && (Rect->top    < Rect->bottom)
         && (Rect->right  <= N2D_MAX_WIDTH)
         && (Rect->bottom <= N2D_MAX_HEIGHT));
}

// static n2d_error_t gcRectRelativeRotation(
//     IN n2d_orientation_t Orientation,
//     IN OUT n2d_orientation_t *Relation)
// {
//     n2d_orientation_t o = Orientation;
//     n2d_orientation_t t = *Relation;

//     switch (o)
//     {
//     case N2D_0:
//         break;

//     case N2D_90:
//         switch (t)
//         {
//         case N2D_0:
//             t = N2D_270;
//             break;

//         case N2D_90:
//             t = N2D_0;
//             break;

//         case N2D_180:
//             t = N2D_90;
//             break;

//         case N2D_270:
//             t = N2D_180;
//             break;

//         default:
//             return N2D_NOT_SUPPORTED;
//         }
//         break;

//     case N2D_180:
//         switch (t)
//         {
//         case N2D_0:
//             t = N2D_180;
//             break;

//         case N2D_90:
//             t = N2D_270;
//             break;

//         case N2D_180:
//             t = N2D_0;
//             break;

//         case N2D_270:
//             t = N2D_90;
//             break;

//         default:
//             return N2D_NOT_SUPPORTED;
//         }
//         break;

//     case N2D_270:
//         switch (t)
//         {
//         case N2D_0:
//             t = N2D_90;
//             break;

//         case N2D_90:
//             t = N2D_180;
//             break;

//         case N2D_180:
//             t = N2D_270;
//             break;

//         case N2D_270:
//             t = N2D_0;
//             break;

//         default:
//             return N2D_NOT_SUPPORTED;
//         }
//         break;

//     default:
//         return N2D_NOT_SUPPORTED;
//     }

//     *Relation = t;
//     return N2D_SUCCESS;
// }

// static n2d_error_t gcRotateRect(
//     IN OUT gcsRECT_PTR Rect,
//     IN n2d_orientation_t Rotation,
//     IN n2d_orientation_t toRotation,
//     IN n2d_int32_t SurfaceWidth,
//     IN n2d_int32_t SurfaceHeight
//     )
// {
//     n2d_error_t error;
//     n2d_int32_t temp;
//     n2d_orientation_t rot = Rotation;
//     n2d_orientation_t tRot = toRotation;


//     /* Verify the arguments. */
//     if ((Rect == N2D_NULL) || (Rect->right <= Rect->left)
//         || (Rect->bottom <= Rect->top))
//     {
//         N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//     }

//     if (tRot == N2D_90
//         || tRot == N2D_270)
//     {
//         temp = SurfaceWidth;
//         SurfaceWidth = SurfaceHeight;
//         SurfaceHeight = temp;
//     }

//     N2D_ON_ERROR(gcRectRelativeRotation(tRot, &rot));

//     switch (rot)
//     {
//     case N2D_0:
//         break;

//     case N2D_90:
//         if ((SurfaceWidth < Rect->bottom) || (SurfaceWidth < Rect->top))
//         {
//             N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//         }

//         temp = Rect->left;
//         Rect->left = SurfaceWidth - Rect->bottom;
//         Rect->bottom = Rect->right;
//         Rect->right = SurfaceWidth - Rect->top;
//         Rect->top = temp;

//         break;

//     case N2D_180:
//         if ((SurfaceWidth < Rect->right) || (SurfaceWidth < Rect->left)
//             || (SurfaceHeight < Rect->bottom) || (SurfaceHeight < Rect->top))
//         {
//             N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//         }

//         temp = Rect->left;
//         Rect->left = SurfaceWidth - Rect->right;
//         Rect->right = SurfaceWidth - temp;
//         temp = Rect->top;
//         Rect->top = SurfaceHeight - Rect->bottom;
//         Rect->bottom = SurfaceHeight - temp;

//         break;

//     case N2D_270:
//         if ((SurfaceHeight < Rect->right) || (SurfaceHeight < Rect->left))
//         {
//             N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//         }

//         temp = Rect->left;
//         Rect->left = Rect->top;
//         Rect->top = SurfaceHeight - Rect->right;
//         Rect->right = Rect->bottom;
//         Rect->bottom = SurfaceHeight - temp;

//         break;

//     default:

//         return N2D_NOT_SUPPORTED;
//     }


//     return N2D_SUCCESS;

// on_error:

//     return error;
// }

// static n2d_error_t gcGetRectWidth(
//     IN gcsRECT_PTR Rect,
//     OUT n2d_uint32_t * Width
//     )
// {
//     n2d_error_t error;

//     if ((Rect == N2D_NULL) || (Width == N2D_NULL))
//     {
//         N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//     }

//     /* Compute and return width. */
//     *Width = Rect->right - Rect->left;

//     /* Success. */
//     return N2D_SUCCESS;

// on_error:
//     return error;
// }

// static n2d_error_t gcGetRectheight(
//     IN gcsRECT_PTR Rect,
//     OUT n2d_uint32_t * Height
//     )
// {
//     n2d_error_t error;

//     if ((Rect == N2D_NULL) || (Height == N2D_NULL))
//     {
//         N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//     }

//     /* Compute and return height. */
//     *Height = Rect->bottom - Rect->top;

//     /* Success. */
//     return N2D_SUCCESS;

// on_error:
//     return error;
// }



static n2d_int32_t gcDrawRectangle(
    IN n2d_uint32_t * memory,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect)
{
    n2d_int32_t size = 0;

    if (SrcRect)
    {
        /* AQDE_SRC_ORIGIN_Address */
        memory[size++] = gcmSETFIELDVALUE(0,
                                     AQ_COMMAND_LOAD_STATE_COMMAND,
                                     OPCODE,
                                     LOAD_STATE)
                  | gcmSETFIELD(0,
                                AQ_COMMAND_LOAD_STATE_COMMAND,
                                COUNT,
                                2)
                  | gcmSETFIELD(0,
                                AQ_COMMAND_LOAD_STATE_COMMAND,
                                ADDRESS,
                                AQDE_SRC_ORIGIN_Address >> 2);

        /* AQDE_SRC_ORIGIN_Address */
        memory[size++]
            = gcmSETFIELD(0, AQDE_SRC_ORIGIN, X, SrcRect->left)
            | gcmSETFIELD(0, AQDE_SRC_ORIGIN, Y, SrcRect->top);

        /* AQDE_SRC_SIZE_Address */
        memory[size++]
            = gcmSETFIELD(0, AQDE_SRC_SIZE, X, SrcRect->right  - SrcRect->left)
            | gcmSETFIELD(0, AQDE_SRC_SIZE, Y, SrcRect->bottom - SrcRect->top);

        size++;
    }

    /* StartDE(RectCount). */
    memory[size++]
        = gcmSETFIELDVALUE(0, AQ_COMMAND_START_DE_COMMAND, OPCODE, START_DE)
        | gcmSETFIELD     (0, AQ_COMMAND_START_DE_COMMAND, COUNT,  1);
    size++;

    /* Append the rectangle. */
    if (DestRect)
    {
        memory[size++]
            = gcmSETFIELD(0, AQ_COMMAND_TOP_LEFT, X, DestRect->left)
            | gcmSETFIELD(0, AQ_COMMAND_TOP_LEFT, Y, DestRect->top);
        memory[size++]
            = gcmSETFIELD(0, AQ_COMMAND_BOTTOM_RIGHT, X, DestRect->right)
            | gcmSETFIELD(0, AQ_COMMAND_BOTTOM_RIGHT, Y, DestRect->bottom);
    }
    else
    {
        /* Set the max rect for multi src blit v2. */
        memory[size++] = 0;
        memory[size++] = 0x3FFF3FFF;
    }

    return size;
}

static n2d_uint32_t gcGetMaximumRectCount(
    void)
{
    return __gcmMASK(AQ_COMMAND_START_DE_COMMAND_COUNT);
}

// static n2d_float_t gcSincFilter(
//     n2d_float_t x,
//     n2d_int32_t radius
//     )
// {
//     n2d_float_t pit, pitd, f1, f2, result;
//     n2d_float_t fRadius = (n2d_float_t)radius;

//     if (x == 0.0f)
//     {
//         result = 1.0f;
//     }
//     else if ((x < -fRadius) || (x > fRadius))
//     {
//         result = 0.0f;
//     }
//     else
//     {
//         pit  = (n2d_float_t)(3.14159265358979323846f * x);
//         pitd = (n2d_float_t)(pit / fRadius);

//         f1 = (n2d_float_t)(n2d_user_os_math_sine(pit)  / pit);
//         f2 = (n2d_float_t)(n2d_user_os_math_sine(pitd) / pitd);

//         result = (n2d_float_t)(f1 * f2);
//     }

//     return result;
// }

/*
**calculate the size of command buffer for this operation
*/
static n2d_uint32_t gcCalCmdbufSize(
    IN n2d_user_hardware_t   *Hardware,
    IN gcs2D_State_PTR  State,
    IN gce2D_COMMAND    Command
    )
{
    n2d_uint32_t size = 0,srcMask = 0,srcCurrent= 0;
    n2d_uint32_t i = 0;
    gcs2D_MULTI_SOURCE_PTR src;
    n2d_bool_t    usePallete = N2D_FALSE;

    if (Command == gcv2D_MULTI_SOURCE_BLT)
    {
        srcMask = State->srcMask;
    }
    else
    {
        srcMask = 1 << State->currentSrcIndex;
    }

    for (i = 0; i < gcdMULTI_SOURCE_NUM; i++)
    {
        if (!(srcMask & (1 << i)))
        {
            continue;
        }

        srcCurrent++;
        src = State->multiSrc + i;
        if (src->srcSurface.format == N2D_INDEX8)
        {
            usePallete |= N2D_TRUE;
        }

    }

    /*count of U32*/
    size =
        /* common states. */
        60
        /* source. */
        + ((srcCurrent > 0) ? (srcCurrent * (Hardware->features[N2D_FEATURE_2D_MULTI_SOURCE_BLT] ? 56 : 40)) : 24)
        /*palette*/
        + (usePallete ? 258 : 0)
        /* tail*/
        + ((gcv2D_FILTER_BLT == Command)? 180 : 10);

#ifdef N2D_SUPPORT_NORMALIZATION
    size += Hardware->state.dest.normalizationInfo.enable ? 12 : 0;
#endif

#ifdef N2D_SUPPORT_BRIGHTNESS
    size += Hardware->state.dest.brightnessAndSaturationInfo.enable ? 8 : 0;
    size += Hardware->state.dest.brightnessAndSaturationInfo.sGammaLUT ? 512 : 0;
    size += Hardware->state.dest.brightnessAndSaturationInfo.vGammaLUT ? 512 : 0;
#endif

#ifdef N2D_SUPPORT_HISTOGRAM
    size += Hardware->state.dest.histogramCalcInfo.enable ? 32 : 0;
#endif
#ifdef N2D_SUPPORT_CONTEXT_ID
    size += N2D_MAX_CONTEXTID_COUNT << 1;
#endif
#ifdef N2D_SUPPORT_64BIT_ADDRESS
    size += N2D_MAX_64BIT_ADDRESS_COUNT << 1;
#endif

    size += gcGetCompressionCmdSize(Hardware,State,Command);

    /*
    * reserved for link command,need for kernel commit stage
    * if link size changed ,this size need change too
    */
    size += 2;

    return size;

}

// static n2d_float_t _calcBilinearFilter(
//     n2d_float_t x
//     )
// {
//     if(x < 0.0f)
//     {
//         x = -x;
//     }

//     return x > 1.0f ? 0.0f : (n2d_float_t)(1.0f - x);
// }

// static n2d_double_t _calcBicubicFilter(double t)
// {
//     const n2d_double_t B = 0.;
//     const n2d_double_t C = 0.75;
//     n2d_double_t tt = t * t;

//     if (t < 0)
//         t = -t;

//     if (t < 1.0)
//     {
//         t = (((12.0 - 9.0 * B - 6.0 * C) * (t * tt))
//            + ((-18.0 + 12.0 * B + 6.0 * C) * tt)
//            + (6.0 - 2 * B));
//         return(t / 6.0);
//     }
//     else if (t < 2.0)
//     {
//         t = (((-1.0 * B - 6.0 * C) * (t * tt))
//            + ((6.0 * B + 30.0 * C) * tt)
//            + ((-12.0 * B - 48.0 * C) * t)
//            + (8.0 * B + 24 * C));
//         return t / 6.0;
//     }

//     return 0.0;
// }

/*******************************************************************************
**
**  gcCalculateSyncTable
**
**  Calculate weight array for sync filter.
**
**  INPUT:
**
**
**      n2d_uint8_t KernelSize
**          New kernel size.
**
**      n2d_uint32_t SrcSize
**          The size in pixels of a source dimension (width or height).
**
**      n2d_uint32_t DstSize
**          The size in pixels of a destination dimension (width or height).
**
**  OUTPUT:
**
**      gcsFILTER_BLIT_ARRAY_PTR KernelInfo
**          Updated kernel structure and table.
*/
// static n2d_error_t gcCalculateSyncTable(
//     IN gceFILTER_TYPE FilterType,
//     IN n2d_uint8_t KernelSize,
//     IN n2d_uint32_t SrcSize,
//     IN n2d_uint32_t DestSize,
//     OUT gcsFILTER_BLIT_ARRAY_PTR KernelInfo
//     )
// {
//     n2d_error_t     error;

//     n2d_uint32_t    scaleFactor;
//     n2d_float_t     fScale;
//     n2d_int32_t     kernelHalf;
//     n2d_float_t     fSubpixelStep;
//     n2d_float_t     fSubpixelOffset;
//     n2d_uint32_t    subpixelPos;
//     n2d_int32_t     kernelPos;
//     n2d_int32_t     padding;
//     n2d_uint16_t    *kernelArray;
//     n2d_pointer     pointer;

//     /* Compute the scale factor. */
//     if (FilterType == gcvFILTER_SYNC)
//     {
//         scaleFactor = gcGetStretchFactor(N2D_FALSE, SrcSize, DestSize);
//     }
//     else
//     {
//         scaleFactor = gcGetStretchFactor(N2D_TRUE, SrcSize, DestSize);
//     }

//     do
//     {
//         /* Same kernel size and ratio as before? */
//         if ((KernelInfo->kernelSize  == KernelSize) &&
//             (KernelInfo->scaleFactor == scaleFactor))
//         {
//             /* Success. */
//             break;
//         }

//         /* Allocate the array if not allocated yet. */
//         if (KernelInfo->kernelStates == N2D_NULL)
//         {
//             /* Allocate the array. */
//             N2D_ON_ERROR(n2d_user_os_allocate(gcvKERNELSTATES, &pointer));

//             KernelInfo->kernelStates = (n2d_uint32_t*)pointer;
//         }

//         /* Store new parameters. */
//         KernelInfo->kernelSize  = KernelSize;
//         KernelInfo->scaleFactor = scaleFactor;

//         /* Compute the scale factor. */
//         fScale = ((n2d_float_t)DestSize) / ((n2d_float_t)SrcSize);

//         /* Adjust the factor for magnification. */
//         if (fScale > 1.0f)
//         {
//             fScale = 1.0f;
//         }

//         /* Calculate the kernel half. */
//         kernelHalf = (n2d_int32_t) (KernelInfo->kernelSize >> 1);

//         /* Calculate the subpixel step. */
//         fSubpixelStep = (n2d_float_t)(1.0f / ((n2d_float_t)gcvSUBPIXELCOUNT));

//         /* Init the subpixel offset. */
//         fSubpixelOffset = 0.5f;

//         /* Determine kernel padding size. */
//         padding = (gcvMAXTAPSIZE - KernelInfo->kernelSize) / 2;

//         /* Set initial kernel array pointer. */
//         kernelArray = (n2d_uint16_t *) (KernelInfo->kernelStates + 1);

//         /* Loop through each subpixel. */
//         for (subpixelPos = 0; subpixelPos < gcvSUBPIXELLOADCOUNT; subpixelPos++)
//         {
//             /* Define a temporary set of weights. */
//             n2d_float_t fSubpixelSet[gcvMAXTAPSIZE] = {0.0};

//             /* Init the sum of all weights for the current subpixel. */
//             n2d_float_t fWeightSum = 0.0f;
//             n2d_uint16_t weightSum = 0;
//             n2d_int16_t adjustCount, adjustFrom;
//             n2d_int16_t adjustment;

//             /* Compute weights. */
//             for (kernelPos = 0; kernelPos < gcvMAXTAPSIZE; kernelPos++)
//             {
//                 /* Determine the current index. */
//                 n2d_int32_t index = kernelPos - padding;

//                 /* Pad with zeros. */
//                 if ((index < 0) || (index >= KernelInfo->kernelSize))
//                 {
//                     fSubpixelSet[kernelPos] = 0.0f;
//                 }
//                 else
//                 {
//                     if (KernelInfo->kernelSize == 1)
//                     {
//                         fSubpixelSet[kernelPos] = 1.0f;
//                     }
//                     else
//                     {
//                         /* Compute the x position for filter function. */
//                         if (FilterType == gcvFILTER_SYNC)
//                         {
//                             n2d_float_t fX =
//                             ((n2d_float_t)(index - kernelHalf) + fSubpixelOffset) * fScale;

//                             /* Compute the weight. */
//                             fSubpixelSet[kernelPos] = gcSincFilter(fX, kernelHalf);
//                         }
//                         else
//                         {
//                             n2d_float_t fX = ((n2d_float_t)(index - kernelHalf ) + fSubpixelOffset);

//                             if (FilterType == gcvFILTER_BILINEAR)
//                             {
//                                 fSubpixelSet[kernelPos] = _calcBilinearFilter(fX);
//                             }
//                             else if (FilterType == gcvFILTER_BICUBIC)
//                             {
//                                 fSubpixelSet[kernelPos] = (n2d_float_t)_calcBicubicFilter((n2d_double_t)fX);
//                             }
//                         }

//                     }

//                     /* Update the sum of weights. */
//                     fWeightSum = fWeightSum + fSubpixelSet[kernelPos];
//                 }
//             }

//             /* Adjust weights so that the sum will be 1.0. */
//             for (kernelPos = 0; kernelPos < gcvMAXTAPSIZE; kernelPos++)
//             {
//                 /* Normalize the current weight. */
//                 n2d_float_t fWeight = fSubpixelSet[kernelPos] / fWeightSum;

//                 /* Convert the weight to fixed point and store in the table. */
//                 if (fWeight == 0.0f)
//                 {
//                     kernelArray[kernelPos] = 0x0000;
//                 }
//                 else if (fWeight >= 1.0f)
//                 {
//                     kernelArray[kernelPos] = 0x4000;
//                 }
//                 else if (fWeight <= -1.0f)
//                 {
//                     kernelArray[kernelPos] = 0xC000;
//                 }
//                 else
//                 {
//                     kernelArray[kernelPos] = (n2d_int16_t) (fWeight * 16384.0f);
//                 }

//                 weightSum += kernelArray[kernelPos];
//             }

//             /* Adjust the fixed point coefficients. */
//             adjustCount = 0x4000 - weightSum;
//             if (adjustCount < 0)
//             {
//                 adjustCount = -adjustCount;
//                 adjustment = -1;
//             }
//             else
//             {
//                 adjustment = 1;
//             }

//             adjustFrom = (gcvMAXTAPSIZE - adjustCount) / 2;

//             for (kernelPos = 0; kernelPos < adjustCount; kernelPos++)
//             {
//                 kernelArray[adjustFrom + kernelPos] += adjustment;
//             }

//             kernelArray += gcvMAXTAPSIZE;

//             /* Advance to the next subpixel. */
//             fSubpixelOffset = fSubpixelOffset - fSubpixelStep;
//         }

//         KernelInfo->kernelChanged = N2D_TRUE;
//     }
//     while(N2D_FALSE);

//     /* Success. */
//     return N2D_SUCCESS;

// on_error:
//     return error;
// }

/*******************************************************************************
**
**  gcCalculateBlurTable
**
**  Calculate weight array for blur filter.
**
**  INPUT:
**
**      gctUINT8 KernelSize
**          New kernel size.
**
**      n2d_uint32_t SrcSize
**          The size in pixels of a source dimension (width or height).
**
**      n2d_uint32_t DstSize
**          The size in pixels of a destination dimension (width or height).
**
**  OUTPUT:
**
**      gcsFILTER_BLIT_ARRAY_PTR KernelInfo
**          Updated kernel structure and table.
*/
// static n2d_error_t gcCalculateBlurTable(
//     IN n2d_uint8_t      KernelSize,
//     IN n2d_uint32_t     SrcSize,
//     IN n2d_uint32_t     DstSize,
//     OUT gcsFILTER_BLIT_ARRAY_PTR KernelInfo
//     )
// {
//     n2d_error_t     error;
//     n2d_uint32_t    scaleFactor;
//     n2d_uint32_t    subpixelPos;
//     n2d_uint32_t    kernelPos;
//     n2d_uint32_t    padding;
//     n2d_uint16_t    *kernelArray;
//     n2d_pointer     pointer = N2D_NULL;

//     if (KernelInfo->filterType != gcvFILTER_BLUR)
//     {
//       N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//     }

//     do
//     {

//         /* Compute the scale factor. */
//         scaleFactor = gcGetStretchFactor(N2D_FALSE, SrcSize, DstSize);

//         /* Same kernel size and ratio as before? */
//         if ((KernelInfo->kernelSize  == KernelSize) &&
//             (KernelInfo->scaleFactor == scaleFactor))
//         {
//             break;
//         }

//         /* Allocate the array if not allocated yet. */
//         if (KernelInfo->kernelStates == N2D_NULL)
//         {
//             /* Allocate the array. */
//             N2D_ON_ERROR(n2d_user_os_allocate(gcvKERNELSTATES, &pointer));

//             KernelInfo->kernelStates = (n2d_uint32_t*)pointer;
//         }

//         /* Store new parameters. */
//         KernelInfo->kernelSize  = KernelSize;
//         KernelInfo->scaleFactor = scaleFactor;

//         /* Determine kernel padding size. */
//         padding = (gcvMAXTAPSIZE - KernelInfo->kernelSize) / 2;

//         /* Set initial kernel array pointer. */
//         kernelArray = (n2d_uint16_t *) (KernelInfo->kernelStates + 1);

//         /* Loop through each subpixel. */
//         for (subpixelPos = 0; subpixelPos < gcvSUBPIXELLOADCOUNT; subpixelPos++)
//         {
//             /* Compute weights. */
//             for (kernelPos = 0; kernelPos < gcvMAXTAPSIZE; kernelPos++)
//             {
//                 /* Determine the current index. */
//                 n2d_uint32_t index = kernelPos - padding;

//                 /* Pad with zeros. */
//                 if ((index < 0) || (index >= KernelInfo->kernelSize))
//                 {
//                     *kernelArray++ = 0x0000;
//                 }
//                 else
//                 {
//                     if (KernelInfo->kernelSize == 1)
//                     {
//                         *kernelArray++ = 0x4000;
//                     }
//                     else
//                     {
//                         n2d_float_t fWeight;

//                         /* Compute the weight. */
//                         fWeight = 1.0f / (KernelInfo->kernelSize);
//                         *kernelArray++ = (n2d_int16_t)(fWeight * 16384.0f);
//                     }
//                 }
//             }
//         }

//         KernelInfo->kernelChanged = N2D_TRUE;
//     }
//     while (N2D_FALSE);

//     /* Success. */
//     return N2D_SUCCESS;

// on_error:
//     return error;
// }


// static n2d_error_t gcLoadFilterKernel(
//     IN n2d_user_hardware_t * Hardware,
//     IN gceFILTER_BLIT_KERNEL type,
//     IN gcsFILTER_BLIT_ARRAY_PTR Kernel
//     )
// {
//     n2d_error_t error;
//     if (Hardware->loadedKernel[type].kernelAddress == (n2d_uint32_t) ~0)
//     {
//         N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//     }

//     N2D_ON_ERROR(gcWriteRegWithLoadState(
//          Hardware,
//          Hardware->loadedKernel[type].kernelAddress, gcvWEIGHTSTATECOUNT,
//          Kernel->kernelStates + 1
//          ));

//     /* Success. */
//     return N2D_SUCCESS;

// on_error:
//     return error;
// }

/*******************************************************************************
**
**  gcTranslateXRGBFormat
**
**  Translate XRGB format according to XRGB hardware setting.
**
**  INPUT:
**
**      n2d_buffer_format_t InputFormat
**          Original format.
**
**  OUTPUT:
**
**      n2d_buffer_format_t* OutputFormat
**          Real format.
*/
n2d_bool_t gcTranslateXRGBFormat(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_buffer_format_t InputFormat,
    OUT n2d_buffer_format_t* OutputFormat
    )
{
    n2d_buffer_format_t format;

    format = InputFormat;

    if (!Hardware->enableXRGB)
    {
        switch (format)
        {
            case N2D_BGRX8888:
                format = N2D_BGRA8888;
                break;
            case N2D_RGBX8888:
                format = N2D_RGBA8888;
                break;
            case N2D_XBGR8888:
                format = N2D_ABGR8888;
                break;
            case N2D_XRGB8888:
                format = N2D_ARGB8888;
                break;

            case N2D_BGRX4444:
                format = N2D_BGRA4444;
                break;
            case N2D_RGBX4444:
                format = N2D_RGBA4444;
                break;
            case N2D_XBGR4444:
                format = N2D_ABGR4444;
                break;
            case N2D_XRGB4444:
                format = N2D_ARGB4444;
                break;

            case N2D_B5G5R5X1:
                format = N2D_B5G5R5A1;
                break;
            case N2D_R5G5B5X1:
                format = N2D_R5G5B5A1;
                break;
            case N2D_X1B5G5R5:
                format = N2D_A1B5G5R5;
                break;
            case N2D_X1R5G5B5:
                format = N2D_A1R5G5B5;
                break;

            default:
                break;
        }
    }

    *OutputFormat = format;

    /* Success. */
    return N2D_SUCCESS;
}

static n2d_bool_t gcIsDEC400Avaiable(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_tile_status_config_t tsConfig)
{
    if(Hardware->features[N2D_FEATURE_DEC400_COMPRESSION] &&
        (tsConfig & N2D_TSC_DEC_COMPRESSED))
        return N2D_TRUE;
    else
        return N2D_FALSE;
}

n2d_error_t gcConvertFormat(
    IN n2d_buffer_format_t  Format,
    OUT n2d_uint32_t        *BitsPerPixel
    )
{
    n2d_error_t  error;

    FormatInfo_t    *FormatInfo = N2D_NULL;
    N2D_ON_ERROR(gcGetFormatInfo(Format,&FormatInfo));

    if (BitsPerPixel != N2D_NULL)
    {
        *BitsPerPixel = FormatInfo->bpp;
    }


    /* Success. */
    return N2D_SUCCESS;

on_error:
    /* Return the error. */
    return error;
}

 /*******************************************************************************
 **
**  gcSetOPFBlockSize
**
**  Set bolck size for one-pass-filter.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      n2d_uint32_t HorTap/VerTap
**          Set to tap value.
**
**  OUTPUT:
**
**      Nothing.
*/
// static n2d_error_t gcSetOPFBlockSize(
//     IN n2d_user_hardware_t   *Hardware,
//     IN gcsSURF_INFO_PTR SrcSurface,
//     IN gcsSURF_INFO_PTR DstSurface,
//     IN gcsRECT_PTR      SrcRect,
//     IN gcsRECT_PTR      DstRect,
//     IN n2d_uint32_t HorTap,
//     IN n2d_uint32_t VerTap)
// {
//     n2d_error_t  error;
//     n2d_uint32_t vfactor, srcBPP, destBPP, tap, comb = 0;
//     n2d_uint32_t fvalue, adjust;
//     n2d_uint32_t configEx = ~0U;
//     n2d_orientation_t dstRot;
//     n2d_bool_t srcYUV420 = N2D_FALSE;
//     n2d_bool_t dstYUV420 = N2D_FALSE;
//     n2d_uint32_t width = 0, height = 0;
//     n2d_uint32_t horizontal = 0xFFFF, vertical = 0xFFFF;
//     gcsOPF_BLOCKSIZE_TABLE_PTR table;

//     do
//     {
//         if ((!Hardware->features[N2D_FEATURE_DEC400_COMPRESSION] &&
//             !gcIsDEC400Avaiable(Hardware, DstSurface->tile_status_config)) ||
//             DstSurface->tiling == N2D_LINEAR)
//         {
//             N2D_ON_ERROR(gcConvertFormat(SrcSurface->format, &srcBPP));
//             N2D_ON_ERROR(gcConvertFormat(DstSurface->format, &destBPP));

//             tap = N2D_MIN(HorTap, VerTap);
//             dstRot = DstSurface->rotation;

//             switch (SrcSurface->format)
//             {
//                 case N2D_NV12:
//                 case N2D_NV21:
//                 case N2D_NV16:
//                 case N2D_NV61:
//                 case N2D_YV12:
//                 case N2D_I420:
//                 case N2D_P010_MSB:
//                 case N2D_P010_LSB:
//                 case N2D_I010_LSB:
//                 case N2D_I010:
//                     srcYUV420 = N2D_TRUE;
//                     break;

//                 default:
//                     srcYUV420 = N2D_FALSE;
//                     break;
//             }

//             switch (DstSurface->format)
//             {
//                 case N2D_NV12:
//                 case N2D_NV21:
//                 case N2D_NV16:
//                 case N2D_NV61:
//                 case N2D_YV12:
//                 case N2D_I420:
//                 case N2D_P010_MSB:
//                 case N2D_P010_LSB:
//                 case N2D_I010_LSB:
//                 case N2D_I010:
//                     dstYUV420 = N2D_TRUE;
//                     break;

//                 default:
//                     dstYUV420 = N2D_FALSE;
//                     break;
//             }

//             vfactor = gcGetStretchFactor(
//                         N2D_TRUE,
//                         SrcRect->bottom - SrcRect->top,
//                         DstRect->bottom - DstRect->top);

//             adjust = vfactor & 0x1FFF;
//             fvalue = (vfactor & 0x7E000) >> 13;
//             if ((vfactor >> 16) >= 8)
//             {
//                 fvalue = 0x3F;
//             }
//             else if (adjust != 0 && fvalue < 0x3F)
//             {
//                 fvalue += 0x1;
//             }

//             if (!fvalue)
//                 N2D_ON_ERROR(N2D_INVALID_ARGUMENT);

//             /* Need to revert fvalue to get info from driver table. */
//             fvalue = 0x3F - fvalue;

//             if (dstRot == N2D_90 || dstRot == N2D_270)
//             {
//                 comb |= 0x1;
//             }
//             if (tap == 5)
//             {
//                 comb |= (0x1 << 1);
//             }

//             /* dst format */
//             if (dstYUV420)
//             {
//                 comb |= (0x1 << 3);
//             }
//             else if (destBPP == 32)
//             {
//                 comb |= (0x1 << 2);
//             }

//             /* src format */
//             if (srcYUV420)
//             {
//                 comb |= (0x1 << 5);
//             }
//             else if (srcBPP == 32)
//             {
//                 comb |= (0x1 << 4);
//             }

//             table = gcsOPF_TABLE_TABLE_ONEPIPE[comb];

//             configEx = gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, BLOCK_CONFIG, CUSTOMIZE);

//             if (table[fvalue].blockDirect)
//             {
//                 configEx &= gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, BLOCK_WALK_DIRECTION, BOTTOM_RIGHT) &
//                              gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, TILE_WALK_DIRECTION, BOTTOM_RIGHT);
//             }
//             else
//             {
//                 configEx &= gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, BLOCK_WALK_DIRECTION, RIGHT_BOTTOM) &
//                              gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, TILE_WALK_DIRECTION, RIGHT_BOTTOM);
//             }
//             if (table[fvalue].pixelDirect)
//             {
//                 configEx &= gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, PIXEL_WALK_DIRECTION, BOTTOM_RIGHT);
//             }
//             else
//             {
//                 configEx &= gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, PIXEL_WALK_DIRECTION, RIGHT_BOTTOM);
//             }

// #ifdef N2D_SUPPORT_HISTOGRAM
//             if (Hardware->state.dest.histogramCalcInfo.enable == N2D_TRUE)
//             {
//                 configEx &= gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, BLOCK_WALK_DIRECTION, RIGHT_BOTTOM);
//                 configEx &= gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, PIXEL_WALK_DIRECTION, RIGHT_BOTTOM);
//             }
// #endif

//             width = table[fvalue].width;
//             height = table[fvalue].height;
//             horizontal = table[fvalue].horizontal;
//             vertical = table[fvalue].vertical;

//             /* width under DUAL PIPE should be even number */
//             if (Hardware->features[N2D_FEATURE_2D_OPF_YUV_OUTPUT])
//             {
//                 if (width & 0x1)
//                 {
//                     width = (width == 1 ? 2 : width - 1);
//                 }
//             }

//             N2D_ON_ERROR(gcWriteRegWithLoadState32(
//                 Hardware,
//                 AQBW_CONFIG_Address,
//                 configEx
//                 ));

//             N2D_ON_ERROR(gcWriteRegWithLoadState32(
//                     Hardware,
//                     AQBW_BLOCK_MASK_Address,
//                     gcmSETFIELD(0, AQBW_BLOCK_MASK, HORIZONTAL, horizontal) |
//                     gcmSETFIELD(0, AQBW_BLOCK_MASK, VERTICAL, vertical)
//                     ));
//         }
//         else
//         {
//             switch (DstSurface->tiling)
//             {
//                 case N2D_TILED:
//                     width = 32;
//                     height = 8;
//                     break;
//                 case N2D_TILED_8X8_XMAJOR:
//                     width = 16;
//                     height = 8;
//                     break;
//                 case N2D_TILED_8X4:
//                 case N2D_TILED_4X8:
//                     width = 8;
//                     height = 8;
//                     break;
//                 case N2D_SUPER_TILED:
//                 case N2D_SUPER_TILED_128B:
//                 case N2D_SUPER_TILED_256B:
//                 case N2D_YMAJOR_SUPER_TILED:
//                     width = 8;
//                     height = 8;
//                     break;
//                 case N2D_TILED_64X4:
//                     width = 64;
//                     height = 8;
//                     break;
//                 case N2D_TILED_32X4:
//                     width = 32;
//                     height = 8;
//                     break;
//                 case N2D_TILED_8X8_YMAJOR:
//                     width = 16;
//                     height = 16;
//                     break;
//                 default:
//                     N2D_ON_ERROR(N2D_NOT_SUPPORTED);
//             }

//             N2D_ON_ERROR(gcWriteRegWithLoadState32(
//                 Hardware,
//                 AQBW_CONFIG_Address,
//                 gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, BLOCK_CONFIG, CUSTOMIZE)
//                 ));

//             N2D_ON_ERROR(gcWriteRegWithLoadState32(
//                 Hardware,
//                 AQBW_CONFIG_Address,
//                 gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, BLOCK_WALK_DIRECTION, RIGHT_BOTTOM) &
//                 gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, TILE_WALK_DIRECTION,  RIGHT_BOTTOM) &
//                 gcmSETMASKEDFIELDVALUE(AQBW_CONFIG, PIXEL_WALK_DIRECTION, RIGHT_BOTTOM)
//                 ));

//             N2D_ON_ERROR(gcWriteRegWithLoadState32(
//                 Hardware,
//                 AQBW_BLOCK_MASK_Address,
//                 gcmSETFIELD(0, AQBW_BLOCK_MASK, HORIZONTAL, 0xFFFF) |
//                 gcmSETFIELD(0, AQBW_BLOCK_MASK, VERTICAL, 0xFFFF)
//                 ));
//         }

//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             AQBW_BLOCK_SIZE_Address,
//             gcmSETFIELD(0, AQBW_BLOCK_SIZE, WIDTH, width) |
//             gcmSETFIELD(0, AQBW_BLOCK_SIZE, HEIGHT, height)
//             ));
//     }
//     while (N2D_FALSE);

//     return N2D_SUCCESS;
// on_error:
//     return error;
// }

n2d_error_t gcAllocateGpuMemory(
    IN n2d_user_hardware_t  *Hardware,
    IN n2d_size_t     Size,
    IN n2d_bool_t     forCmdBuf,
    OUT n2d_ioctl_interface_t    *Iface)
{
    n2d_error_t error;

    Iface->command = N2D_KERNEL_COMMAND_ALLOCATE;
    Iface->core    = (n2d_core_id_t)Hardware->coreIndex;

    Iface->u.command_allocate.size = (n2d_uint32_t)Size;

    if (forCmdBuf && n2d_is_feature_support(N2D_FEATURE_64BIT_ADDRESS))
    {
        Iface->u.command_allocate.pool = N2D_POOL_COMMAND;
        Iface->u.command_allocate.type = N2D_TYPE_COMMAND;
    }

    // n2d_get_device_index(&Iface->dev_id);

    /* Call the kernel. */
    N2D_ON_ERROR(n2d_user_os_ioctl(Iface));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcMapMemory(
    IN n2d_user_hardware_t   *Hardware,
    IN n2d_uintptr_t     Handle,
    OUT n2d_ioctl_interface_t    *Iface)
{
    n2d_error_t error;

    Iface->command = N2D_KERNEL_COMMAND_MAP;
    Iface->core    = (n2d_core_id_t)Hardware->coreIndex;

    Iface->u.command_map.handle = Handle;

    // n2d_get_device_index(&Iface->dev_id);

    /* Call the kernel. */
    N2D_ON_ERROR(n2d_user_os_ioctl(Iface));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcUnmapMemory(
    IN n2d_user_hardware_t  *Hardware,
    IN n2d_uintptr_t    Handle)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_ioctl_interface_t  iface = {0};

    if (Handle)
    {
        iface.command = N2D_KERNEL_COMMAND_UNMAP;
        iface.core = (n2d_core_id_t)Hardware->coreIndex;

        iface.u.command_unmap.handle = Handle;

        // n2d_get_device_index(&iface.dev_id);

        /* Call the kernel. */
        N2D_ON_ERROR(n2d_user_os_ioctl(&iface));
    }

on_error:
    return error;
}

n2d_error_t gcFreeGpuMemory(
    IN n2d_user_hardware_t  *Hardware,
    IN n2d_uintptr_t    Handle)
{
    n2d_error_t error;
    n2d_ioctl_interface_t       iface = {0};

    iface.command = N2D_KERNEL_COMMAND_FREE;
    iface.core    = (n2d_core_id_t)Hardware->coreIndex;

    iface.u.command_unmap.handle = Handle;

    // n2d_get_device_index(&iface.dev_id);

    /* Call the kernel. */
    N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcSubcommitCmd(
    IN n2d_user_hardware_t  *hardware,
    IN n2d_user_subcommit_t *subcommit)
{
    n2d_error_t error;
    n2d_ioctl_interface_t       iface = {0};

    iface.command = N2D_KERNEL_COMMAND_COMMIT;
    iface.core    = (n2d_core_id_t)hardware->coreIndex;

    iface.u.command_commit.handle   = subcommit->cmd_buf.handle;
    iface.u.command_commit.logical  = (n2d_uintptr_t)subcommit->cmd_buf.logical;
    iface.u.command_commit.address  = subcommit->cmd_buf.address;
    iface.u.command_commit.offset   = subcommit->cmd_buf.offset;
    iface.u.command_commit.size     = subcommit->cmd_buf.size;

    // n2d_get_device_index(&iface.dev_id);

    /* Call the kernel. */
    N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcCommitEvent(
    IN n2d_user_hardware_t  *Hardware,
    IN n2d_uintptr_t         Queue)
{
    n2d_error_t error;
    n2d_ioctl_interface_t       iface = {0};

    iface.command = N2D_KERNEL_COMMAND_EVENT_COMMIT;
    iface.core    = (n2d_core_id_t)Hardware->coreIndex;
    iface.u.command_event_commit.submit = N2D_TRUE;
    iface.u.command_event_commit.queue  = Queue;

    // n2d_get_device_index(&iface.dev_id);

    /* Call the kernel. */
    N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcCommitSignal(
    IN n2d_user_hardware_t* Hardware)
{
    n2d_user_event_t event = { 0 };
    n2d_error_t error = N2D_SUCCESS;

    /*commit eventid*/
    event.iface.command = N2D_KERNEL_COMMAND_SIGNAL;
    event.iface.core = Hardware->coreIndex;
    event.iface.u.command_signal.handle = (n2d_uintptr_t)Hardware->buffer.command_buffer_tail->stall_signal;
    event.iface.u.command_signal.from_kernel = N2D_FALSE;
    event.iface.u.command_signal.process = 0;
    event.next = 0;

    N2D_ON_ERROR(gcCommitEvent(Hardware, gcmPTR_TO_UINT64(&event)));

on_error:
    return error;
}

n2d_error_t gcWaitSignal(
    IN n2d_user_hardware_t  *Hardware,
    IN n2d_pointer          Stall_signal,
    IN n2d_uint32_t         Wait_time)
{
    n2d_error_t error;
    n2d_ioctl_interface_t       iface = {0};

    iface.command = N2D_KERNEL_COMMAND_USER_SIGNAL;
    iface.core    = (n2d_core_id_t)Hardware->coreIndex;
    iface.u.command_user_signal.command = N2D_USER_SIGNAL_WAIT;
    iface.u.command_user_signal.handle  = (n2d_uintptr_t)(Stall_signal);
    iface.u.command_user_signal.wait    = Wait_time;

    // n2d_get_device_index(&iface.dev_id);

    /* Call the kernel. */
    N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}


/*******************************************************************************
**
**  gcGetMaximumDataCount
**
**  Retrieve the maximum number of 32-bit data chunks for a single DE command.
**
**  INPUT:
**
**      Nothing
**
**  OUTPUT:
**
**      n2d_uint32_t
**          Data count value.
*/
n2d_uint32_t gcGetMaxDataCount(
    void
    )
{
    n2d_uint32_t result = 0;

    result = __gcmMASK(AQ_COMMAND_START_DE_COMMAND_DATA_COUNT);

    return result;
}

static n2d_error_t gcCheckTilingSupport(
    IN gcsSURF_INFO        *Surface,
    IN n2d_bool_t          IsSrc)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_string          sheetName;
    n2d_bool_t          isNotSupport = N2D_FALSE;

    /* An Initialized Surface should bypass tiling check */
    if (!(Surface->tiling || Surface->stride[0]))
    {
        return N2D_SUCCESS;
    }

    if (Surface->tile_status_config)
    {
        sheetName = "DEC_COMPRESSED";
    }
    else
    {
        sheetName = "TSC_DISABLE";
    }

    isNotSupport |= gcIsNotSuppotFromFormatSheet(sheetName,
        Surface->format, Surface->tiling, Surface->cacheMode, IsSrc);

    if (Surface->tile_status_buffer.enableFastClear)
    {
        isNotSupport |= gcIsNotSuppotFromFormatSheet("FAST_CLEAR",
            Surface->format, Surface->tiling, Surface->cacheMode, IsSrc);
    }

    if (isNotSupport)
    {
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcWriteRegWithLoadState(
    n2d_user_hardware_t * Hardware,
    n2d_uint32_t address,
    n2d_uint32_t count,
    n2d_pointer data)
{
    n2d_error_t error;
    n2d_uint32_t *memory;
    n2d_uint32_t *p;

    if (Hardware->cmdIndex & 1)
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    if (Hardware->cmdCount - Hardware->cmdIndex < gcmALIGN(1 + count, 2))
    {
        N2D_ON_ERROR(N2D_OUT_OF_MEMORY);
    }

    memory = (n2d_uint32_t *)Hardware->cmdLogical + Hardware->cmdIndex;

    memory[0] = gcmSETFIELDVALUE(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE, LOAD_STATE)
              | gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, count)
              | gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS, address >> 2);

    /* Update the index. */
    Hardware->cmdIndex += 1 + count;

    /* Copy the data. */
    p = (n2d_uint32_t *)data;
    memory++;
    while (count-- != 0)
    {
        *memory++ = *p++;
    }

    /* Alignment. */
    if (Hardware->cmdIndex & 1)
    {
        *memory++ = 0;
        Hardware->cmdIndex += 1;
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcWriteRegWithLoadState32(
    IN n2d_user_hardware_t *hardware,
    IN n2d_uint32_t Address,
    IN n2d_uint32_t Data
    )
{
    /* Call buffered load state to do it. */
    return gcWriteRegWithLoadState(hardware, Address, 1, &Data);
}

n2d_error_t gcClearRectangle(
    IN n2d_user_hardware_t * Hardware,
    IN gcs2D_State_PTR State,
    IN n2d_uint32_t RectCount,
    IN gcsRECT_PTR Rect
    )
{
    n2d_error_t error;

    /* Kick off 2D engine. */
    N2D_ON_ERROR(gcStartDE(
        Hardware, State, gcv2D_CLEAR, 0, N2D_NULL, RectCount, Rect
        ));

on_error:
    return error;
}

static n2d_bool_t gcNeedUserCSC(
    n2d_csc_mode_t Mode,
    n2d_buffer_format_t Format
    )
{
    if ((Mode == N2D_CSC_USER_DEFINED_CLAMP)
        || (Mode == N2D_CSC_USER_DEFINED))
    {
        switch (Format)
        {
            case N2D_YUYV:
            case N2D_UYVY:
            case N2D_YV12:
            case N2D_I420:
            case N2D_NV12:
            case N2D_NV21:
            case N2D_NV16:
            case N2D_NV61:
                return N2D_TRUE;
            default:
                return N2D_FALSE;
        }
    }

    return N2D_FALSE;
}


/*******************************************************************************
**
**  gcUploadCSCProgrammable
**
**  Update CSC Programmable tables.
**
**  INPUT:
**
**      n2d_user_hardware_t *Hardware
**          Pointer to an n2d_user_hardware_t * object.
**
**      n2d_bool_t YUV2RGB
**          Whether YUV to RGB; else RGB to YUV;
**
**      n2d_int32_t * Table
**          Coefficient table;
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcUploadCSCProgrammable(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_bool_t YUV2RGB,
    IN n2d_int32_t * Table
    )
{
    n2d_error_t error;
    n2d_uint32_t coef[8] = {0};
    n2d_int32_t i;

    /* Through load state command. */
    for (i = 0; i < 12; ++i)
    {
        if (i < 9)
        {
            coef[i >> 1] = (i & 1) ?
                gcmSETFIELD(coef[i >> 1], GCREG_DEYUV2_RGB_COEF0, COEFFICIENT1, Table[i])
              : gcmSETFIELD(coef[i >> 1], GCREG_DEYUV2_RGB_COEF0, COEFFICIENT0, Table[i]);
        }
        else
        {
            coef[i - 4] = gcmSETFIELD(0, GCREG_DEYUV2_RGB_COEF_D1, COEFFICIENT, Table[i]);
        }
    }

    if (YUV2RGB)
    {
        N2D_ON_ERROR(gcWriteRegWithLoadState(Hardware, GCREG_DEYUV2_RGB_COEF0_Address,
                                   8, coef));
    }
    else
    {
        N2D_ON_ERROR(gcWriteRegWithLoadState(Hardware, GCREG_DERGB2_YUV_COEF0_Address,
                                   8, coef));
    }

on_error:
    /* Return the error. */
    return error;
}

static n2d_error_t gcRenderBegin(
    IN n2d_user_hardware_t *Hardware)
{
    n2d_error_t error = N2D_SUCCESS;

    /*******************************************************************
    ** Select 2D pipe.
    */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQ_PIPE_SELECT_Address,
        gcmSETFIELDVALUE(0, AQ_PIPE_SELECT, PIPE, PIPE2D)
        ));

    /* Reset all src compression field. */

    if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
    {
        n2d_uint32_t config = GCREG_DE_SRC_EX_CONFIG_EX_ResetValue, i;

        config = gcmSETFIELDVALUE(config, GCREG_DE_SRC_EX_CONFIG_EX, DC_COMPRESSION, DISABLED);
        config = gcmSETFIELDVALUE(config, GCREG_DE_SRC_EX_CONFIG_EX, MASK_DC_COMPRESSION, ENABLED);

        for (i = 0; i < gcdMULTI_SOURCE_NUM; i++)
        {
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                i ? GCREG_DE_SRC_EX_CONFIG_EX_Address + (i << 2) : AQDE_SRC_EX_CONFIG_Address,
                config
                ));
        }
    }

#ifdef N2D_SUPPORT_64BIT_ADDRESS
    {
        n2d_uint32_t loop = 0;

        for (; loop <= N2D_MAX_64BIT_ADDRESS_COUNT; ++loop)
        {
            gcSetHigh32BitAddress(
                Hardware,
                (n2d_uint32_t)Hardware->state.dest.dstSurface.gpuAddrEx,
                N2D_ADDRESS_TS + loop);
        }
    }
#endif

#ifdef N2D_SUPPORT_SECURE_BUFFER
    gcSetSecureBufferState(Hardware);
#endif

#ifdef N2D_SUPPORT_CONTEXT_ID
    {
        n2d_uint32_t i = 0;
        for (; i < N2D_MAX_CONTEXTID_COUNT; ++i)
        {
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                AQ_CONTEXT_ID0_Address + (i << 0x2),
                Hardware->contextID[i]
            ));
        }
    }
#endif

on_error:

    return error;
}

n2d_error_t gcRenderEnd(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_bool_t SourceFlush)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t *cmd_buf = (n2d_uint32_t *)Hardware->cmdLogical;
    n2d_uint32_t delta = 0;
    n2d_uint32_t i =0;
    if (SourceFlush && Hardware->hw2DAppendCacheFlush)
    {
        N2D_ON_ERROR(gcFlushAppendCache(Hardware));
    }

    /* Flush the 2D pipe. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                AQ_FLUSH_Address,
                gcmSETFIELDVALUE(0,
                                 AQ_FLUSH,
                                 PE2D_CACHE,
                                 ENABLE)));

    /* Semaphore & Stall. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQ_SEMAPHORE_Address,
        gcmSETFIELDVALUE(0, AQ_SEMAPHORE, DESTINATION, PIXEL_ENGINE)
        | gcmSETFIELDVALUE(0, AQ_SEMAPHORE, SOURCE, FRONT_END)
        ));

    cmd_buf[Hardware->cmdIndex++]
        = gcmSETFIELDVALUE(0, STALL_COMMAND, OPCODE, STALL);

    cmd_buf[Hardware->cmdIndex++]
        = gcmSETFIELDVALUE(0, AQ_SEMAPHORE, DESTINATION, PIXEL_ENGINE)
        | gcmSETFIELDVALUE(0, AQ_SEMAPHORE, SOURCE, FRONT_END);

    /*flush dec tile cache when enable compression*/
    if( Hardware->hw2DCurrentRenderCompressed && Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
    {
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GC_TILE_CACHE_FLUSH_Address,
            gcmSETFIELDVALUE(0, GC_TILE_CACHE_FLUSH,FLUSH,ENABLE)
            ));

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GCREG_DEC400_CONTROL_Address,
            gcmSETFIELDVALUE(0, GCREG_DEC400_CONTROL, FLUSH, ENABLE)
            ));

        /* Flush the 2D pipe. */
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
                    Hardware,
                    AQ_FLUSH_Address,
                    gcmSETFIELDVALUE(0,
                                     AQ_FLUSH,
                                     PE2D_CACHE,
                                     ENABLE)));

        /* Semaphore & Stall. */
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQ_SEMAPHORE_Address,
            gcmSETFIELDVALUE(0, AQ_SEMAPHORE, DESTINATION, PIXEL_ENGINE)
            | gcmSETFIELDVALUE(0, AQ_SEMAPHORE, SOURCE, FRONT_END)
            ));

        cmd_buf[Hardware->cmdIndex++]
            = gcmSETFIELDVALUE(0, STALL_COMMAND, OPCODE, STALL);

        cmd_buf[Hardware->cmdIndex++]
            = gcmSETFIELDVALUE(0, AQ_SEMAPHORE, DESTINATION, PIXEL_ENGINE)
            | gcmSETFIELDVALUE(0, AQ_SEMAPHORE, SOURCE, FRONT_END);

    }

    delta = Hardware->cmdCount - Hardware->cmdIndex;
    if(delta & 1)
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    /* Append NOP.*/
    for(i = 0; i < delta; i += 2)
    {
        cmd_buf[Hardware->cmdIndex + i] = gcmSETFIELDVALUE(0, AQ_COMMAND_NOP_COMMAND, OPCODE, NOP);
        cmd_buf[Hardware->cmdIndex + i + 1] = 0;
    }

    /*dump capture*/
    N2D_DUMP_COMMAND((n2d_uint32_t*)Hardware->cmdLogical, Hardware->cmdIndex, Hardware);


    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

static n2d_error_t gcLoadPalette(
    IN n2d_user_hardware_t* Hardware,
    IN n2d_uint32_t FirstIndex,
    IN n2d_uint32_t IndexCount,
    IN n2d_pointer ColorTable,
    IN n2d_bool_t ColorConvert,
    IN n2d_buffer_format_t DstFormat,
    OUT n2d_bool_t *Program,
    OUT n2d_buffer_format_t *ConvertFormat
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_bool_t feature_no_index_pattern = 0;


    /*Verify hw feature whether support index_pattern & 2dpe20*/
    feature_no_index_pattern = gcmVERIFYFIELDVALUE(
        Hardware->chipMinorFeatures,
        GC_MINOR_FEATURES2, NO_INDEX_PATTERN, NONE);

    if (!feature_no_index_pattern)
    {
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
    {
        n2d_uint32_t address;

        if (*ConvertFormat != DstFormat)
        {
            *Program = N2D_TRUE;
        }

        if ((*Program) &&(ColorConvert == N2D_FALSE))
        {
            /* Pattern table is in destination format,
               convert it into ARGB8 format. */
            N2D_ON_ERROR(gcColorConvertToARGB8(
                DstFormat,
                IndexCount,
                (n2d_uint32_t*)ColorTable,
                (n2d_uint32_t*)ColorTable
                ));

            *Program = N2D_FALSE;
            *ConvertFormat = DstFormat;
        }

        /* Determine first address. */
        address = AQDEIndexColorTable32RegAddrs + FirstIndex;

        /* Upload the palette table. */
        N2D_ON_ERROR(gcWriteRegWithLoadState(
            Hardware, address << 2, IndexCount, ColorTable
            ));
    }
    else
    {
        /* Not supported by the software renderer. */
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

on_error:
    return error;
}

static n2d_error_t gcSetClearColor(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t Color,
    IN n2d_buffer_format_t DstFormat
    )
{
    // DstFormat = DstFormat;

    return gcWriteRegWithLoadState32(
                Hardware,
                AQDE_CLEAR_PIXEL_VALUE32_Address,
                Color
                );
}

n2d_error_t gcSetClipping(
    IN n2d_user_hardware_t * Hardware,
    IN gcsRECT_PTR Rect
    )
{
    n2d_error_t error;
    n2d_uint32_t data[2];
    gcsRECT clippedRect;

    /* Clip Rect coordinates in positive range. */
    clippedRect.left   = N2D_MAX(Rect->left, 0);
    clippedRect.top    = N2D_MAX(Rect->top, 0);
    clippedRect.right  = N2D_MAX(Rect->right, 0);
    clippedRect.bottom = N2D_MAX(Rect->bottom, 0);

    /* AQDE_CLIP_TOP_LEFT_Address */
    data[0]
        = gcmSETFIELD(0, AQDE_CLIP_TOP_LEFT, X, clippedRect.left)
        | gcmSETFIELD(0, AQDE_CLIP_TOP_LEFT, Y, clippedRect.top);

    /* AQDE_CLIP_BOTTOM_RIGHT_Address */
    data[1]
        = gcmSETFIELD(0, AQDE_CLIP_BOTTOM_RIGHT, X, clippedRect.right)
        | gcmSETFIELD(0, AQDE_CLIP_BOTTOM_RIGHT, Y, clippedRect.bottom);

    /* Load cllipping states. */
    N2D_ON_ERROR(gcWriteRegWithLoadState(
        Hardware, AQDE_CLIP_TOP_LEFT_Address, 2, data
        ));

on_error:
    /* Return the error. */
    return error;
}

static n2d_error_t gcLoadSolidColorPattern(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_bool_t ColorConvert,
    IN n2d_uint32_t Color,
    IN n2d_uint64_t Mask,
    IN n2d_buffer_format_t DstFormat
    )
{
    n2d_error_t error;

    n2d_uint32_t config;

    /* LoadState(AQDE_PATTERN_MASK, 2), Mask. */
    N2D_ON_ERROR(gcWriteRegWithLoadState(
        Hardware,
        AQDE_PATTERN_MASK_LOW_Address, 2, &Mask
        ));

    if (!ColorConvert)
    {
        /* Convert color to ARGB8 if it was specified in target format. */
        N2D_ON_ERROR(gcColorConvertToARGB8(
            DstFormat,
            1,
            &Color,
            &Color
            ));
    }

    /* LoadState(AQDE_PATTERN_FG_COLOR, 1), Color. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_PATTERN_FG_COLOR_Address, Color
        ));

    /* Setup pattern configuration. */
    config = gcmSETFIELDVALUE(0, AQDE_PATTERN_CONFIG, TYPE,          SOLID_COLOR)
            | gcmSETFIELD     (0, AQDE_PATTERN_CONFIG, COLOR_CONVERT, ColorConvert)
            | gcmSETFIELDVALUE(0, AQDE_PATTERN_CONFIG, INIT_TRIGGER,  INIT_ALL);

    /* LoadState(AQDE_PATTERN_CONFIG, 1), config. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_PATTERN_CONFIG_Address, config
        ));

on_error:
    /* Return the error. */
    return error;
}

n2d_error_t gcSetBitBlitMirror(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_bool_t HorizontalMirror,
    IN n2d_bool_t VerticalMirror,
    IN n2d_bool_t DstMirror
    )
{
    n2d_error_t error;

    n2d_uint32_t mirror;
    n2d_uint32_t config;

    /* Determine the mirror value. */
    if (HorizontalMirror)
    {
        if (VerticalMirror)
        {
            mirror = AQDE_ROT_ANGLE_SRC_MIRROR_MIRROR_XY;
        }
        else
        {
            mirror = AQDE_ROT_ANGLE_SRC_MIRROR_MIRROR_X;
        }
    }
    else
    {
        if (VerticalMirror)
        {
            mirror = AQDE_ROT_ANGLE_SRC_MIRROR_MIRROR_Y;
        }
        else
        {
            mirror = AQDE_ROT_ANGLE_SRC_MIRROR_NONE;
        }
    }

    /* Enable the mirror. */
    if (DstMirror)
    {
        config = gcmSETFIELDVALUE(~0, AQDE_ROT_ANGLE, MASK_DST_MIRROR, ENABLED);
        config = gcmSETFIELD(config, AQDE_ROT_ANGLE, DST_MIRROR, mirror);

        config = gcmSETFIELDVALUE(config, AQDE_ROT_ANGLE, SRC_MIRROR, NONE);
        config = gcmSETFIELDVALUE(config, AQDE_ROT_ANGLE, MASK_SRC_MIRROR, ENABLED);
    }
    else
    {
        config = gcmSETFIELDVALUE(~0, AQDE_ROT_ANGLE, MASK_SRC_MIRROR, ENABLED);
        config = gcmSETFIELD(config, AQDE_ROT_ANGLE, SRC_MIRROR, mirror);

        config = gcmSETFIELDVALUE(config, AQDE_ROT_ANGLE, DST_MIRROR, NONE);
        config = gcmSETFIELDVALUE(config, AQDE_ROT_ANGLE, MASK_DST_MIRROR, ENABLED);
    }

    /* Set mirror configuration. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_ROT_ANGLE_Address,
        config
        ));

    return N2D_SUCCESS;

on_error:
    return error;
}

static n2d_error_t gcSetDither(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_bool_t Enable
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t memory[2];

    /* Set states into temporary buffer. */
    memory[0] = Enable ? 0x6E4CA280 : AQPE_DITHER_LOW_ResetValue;
    memory[1] = Enable ? 0x5D7F91B3 : AQPE_DITHER_HIGH_ResetValue;

    /* Through load state command. */
    N2D_ON_ERROR(gcWriteRegWithLoadState(
        Hardware, AQPE_DITHER_LOW_Address, 2, memory
        ));

on_error:
    return error;
}

void gcGetResourceUsage(
    IN n2d_uint8_t FgRop,
    IN n2d_uint8_t BgRop,
    IN n2d_transparency_t SrcTransparency,
    OUT n2d_bool_t * UseSource,
    OUT n2d_bool_t * UsePattern,
    OUT n2d_bool_t * UseDestination
    )
{
    /* Determine whether we need the source for the operation. */
    if (UseSource != N2D_NULL)
    {
        if (SrcTransparency == N2D_KEYED)
        {
            *UseSource = N2D_TRUE;
        }
        else
        {
            /* Determine whether this is target only operation. */
            n2d_bool_t targetOnly
                =  ((FgRop == 0x00) && (BgRop == 0x00))     /* Blackness.    */
                || ((FgRop == 0x55) && (BgRop == 0x55))     /* Invert.       */
                || ((FgRop == 0xAA) && (BgRop == 0xAA))     /* No operation. */
                || ((FgRop == 0xFF) && (BgRop == 0xFF));    /* Whiteness.    */

            *UseSource
                = !targetOnly
                && ((((FgRop >> 2) & 0x33) != (FgRop & 0x33))
                ||  (((BgRop >> 2) & 0x33) != (BgRop & 0x33)));
        }
    }

    /* Determine whether we need the pattern for the operation. */
    if (UsePattern != N2D_NULL)
    {
        *UsePattern
            =  (((FgRop >> 4) & 0x0F) != (FgRop & 0x0F))
            || (((BgRop >> 4) & 0x0F) != (BgRop & 0x0F));
    }

    /* Determine whether we need the destination for the operation. */
    if (UseDestination != N2D_NULL)
    {
        *UseDestination
            =  (((FgRop >> 1) & 0x55) != (FgRop & 0x55))
            || (((BgRop >> 1) & 0x55) != (BgRop & 0x55));
    }
}

n2d_error_t gcTranslateCommand(
    IN gce2D_COMMAND APIValue,
    OUT n2d_uint32_t * HwValue
    )
{
    /* Dispatch on command. */
    switch (APIValue)
    {
    case gcv2D_CLEAR:
        *HwValue = AQDE_DEST_CONFIG_COMMAND_CLEAR;
        break;

    case gcv2D_LINE:
        *HwValue = AQDE_DEST_CONFIG_COMMAND_LINE;
        break;

    case gcv2D_BLT:
        *HwValue = AQDE_DEST_CONFIG_COMMAND_BIT_BLT;
        break;

    case gcv2D_STRETCH:
        *HwValue = AQDE_DEST_CONFIG_COMMAND_STRETCH_BLT;
        break;

    case gcv2D_HOR_FILTER:
        *HwValue = AQDE_DEST_CONFIG_COMMAND_HOR_FILTER_BLT;
        break;

    case gcv2D_VER_FILTER:
        *HwValue = AQDE_DEST_CONFIG_COMMAND_VER_FILTER_BLT;
        break;

#ifdef AQDE_DEST_CONFIG_COMMAND_MULTI_SOURCE_BLT
    case gcv2D_MULTI_SOURCE_BLT:
        *HwValue = AQDE_DEST_CONFIG_COMMAND_MULTI_SOURCE_BLT;
        break;
#endif

    default:
        /* Not supported. */
        return N2D_NOT_SUPPORTED;
    }

    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetMultiplyModes(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_pixel_color_multiply_mode_t SrcPremultiplySrcAlpha,
    IN n2d_pixel_color_multiply_mode_t DstPremultiplyDstAlpha,
    IN n2d_global_color_multiply_mode_t SrcPremultiplyGlobalMode,
    IN n2d_pixel_color_multiply_mode_t DstDemultiplyDstAlpha,
    IN n2d_uint32_t SrcGlobalColor
    )
{
    n2d_error_t error;

    n2d_uint32_t srcPremultiplySrcAlpha;
    n2d_uint32_t dstPremultiplyDstAlpha;
    n2d_uint32_t srcPremultiplyGlobalMode;
    n2d_uint32_t dstDemultiplyDstAlpha;

    /* Convert the multiply modes. */
    N2D_ON_ERROR(gcTranslatePixelColorMultiplyMode(
        SrcPremultiplySrcAlpha, &srcPremultiplySrcAlpha
        ));

    N2D_ON_ERROR(gcTranslatePixelColorMultiplyMode(
        DstPremultiplyDstAlpha, &dstPremultiplyDstAlpha
        ));

    N2D_ON_ERROR(gcTranslateGlobalColorMultiplyMode(
        SrcPremultiplyGlobalMode, &srcPremultiplyGlobalMode
        ));

    N2D_ON_ERROR(gcTranslatePixelColorMultiplyMode(
        DstDemultiplyDstAlpha, &dstDemultiplyDstAlpha
        ));

    /* LoadState pixel multiply modes. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_COLOR_MULTIPLY_MODES_Address,
          gcmSETFIELD(0, AQDE_COLOR_MULTIPLY_MODES, SRC_PREMULTIPLY,        srcPremultiplySrcAlpha)
        | gcmSETFIELD(0, AQDE_COLOR_MULTIPLY_MODES, DST_PREMULTIPLY,        dstPremultiplyDstAlpha)
        | gcmSETFIELD(0, AQDE_COLOR_MULTIPLY_MODES, SRC_GLOBAL_PREMULTIPLY, srcPremultiplyGlobalMode)
        | gcmSETFIELD(0, AQDE_COLOR_MULTIPLY_MODES, DST_DEMULTIPLY,         dstDemultiplyDstAlpha)
        | gcmSETFIELDVALUE(0, AQDE_COLOR_MULTIPLY_MODES, DST_DEMULTIPLY_ZERO, ENABLE)
        ));

    if (SrcPremultiplyGlobalMode != N2D_GLOBAL_COLOR_MULTIPLY_DISABLE)
    {
        /* Set source global color. */
        N2D_ON_ERROR(gcSetSourceGlobalColor(
            Hardware,
            SrcGlobalColor
            ));
    }

on_error:

    return error;
}

n2d_error_t gcDisableAlphaBlend(
    IN n2d_user_hardware_t * hardware
    )
{
    n2d_error_t error;

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        hardware,
        AQDE_ALPHA_CONTROL_Address,
        gcmSETFIELDVALUE(0, AQDE_ALPHA_CONTROL, ENABLE, OFF)
        ));

on_error:
    return error;
}

static n2d_error_t gcEnableAlphaBlend(
    IN n2d_user_hardware_t * Hardware,
    IN gceSURF_PIXEL_ALPHA_MODE SrcAlphaMode,
    IN gceSURF_PIXEL_ALPHA_MODE DstAlphaMode,
    IN gceSURF_GLOBAL_ALPHA_MODE SrcGlobalAlphaMode,
    IN gceSURF_GLOBAL_ALPHA_MODE DstGlobalAlphaMode,
    IN gceSURF_BLEND_FACTOR_MODE SrcFactorMode,
    IN gceSURF_BLEND_FACTOR_MODE DstFactorMode,
    IN gceSURF_PIXEL_COLOR_MODE SrcColorMode,
    IN gceSURF_PIXEL_COLOR_MODE DstColorMode,
    IN n2d_uint32_t SrcGlobalAlphaValue,
    IN n2d_uint32_t DstGlobalAlphaValue
    )
{
    n2d_error_t error;

    /* Define hardware components. */
    n2d_uint32_t srcAlphaMode = 0;
    n2d_uint32_t srcGlobalAlphaMode = 0;
    n2d_uint32_t srcFactorMode = 0;
    n2d_uint32_t srcFactorExpansion = 0;
    n2d_uint32_t srcColorMode = 0;
    n2d_uint32_t dstAlphaMode = 0;
    n2d_uint32_t dstGlobalAlphaMode = 0;
    n2d_uint32_t dstFactorMode = 0;
    n2d_uint32_t dstFactorExpansion = 0;
    n2d_uint32_t dstColorMode = 0;

    /* State array. */
    n2d_uint32_t states[2];

    /* Translate inputs. */
    N2D_ON_ERROR(gcTranslatePixelAlphaMode(
        SrcAlphaMode, &srcAlphaMode
        ));

    N2D_ON_ERROR(gcTranslatePixelAlphaMode(
        DstAlphaMode, &dstAlphaMode
        ));

    N2D_ON_ERROR(gcTranslateGlobalAlphaMode(
        SrcGlobalAlphaMode, &srcGlobalAlphaMode
        ));

    N2D_ON_ERROR(gcTranslateGlobalAlphaMode(
        DstGlobalAlphaMode, &dstGlobalAlphaMode
        ));

    N2D_ON_ERROR(gcTranslateAlphaFactorMode(
        N2D_TRUE, SrcFactorMode, &srcFactorMode, &srcFactorExpansion
        ));

    N2D_ON_ERROR(gcTranslateAlphaFactorMode(
        N2D_FALSE, DstFactorMode, &dstFactorMode, &dstFactorExpansion
        ));

    N2D_ON_ERROR(gcTranslatePixelColorMode(
        SrcColorMode, &srcColorMode
        ));

    N2D_ON_ERROR(gcTranslatePixelColorMode(
        DstColorMode, &dstColorMode
        ));

    /*
        Fill in the states.
    */

    /* Enable alpha blending and set global alpha values. */
    states[0] = gcmSETFIELDVALUE(0, AQDE_ALPHA_CONTROL, ENABLE, ON)
              | gcmSETFIELD(0, AQDE_ALPHA_CONTROL, SRC_VALUE,
                                                SrcGlobalAlphaValue)
              | gcmSETFIELD(0, AQDE_ALPHA_CONTROL, DST_VALUE,
                                                DstGlobalAlphaValue);

    /* Set alpha blending modes. */
    states[1] = gcmSETFIELD(0, AQDE_ALPHA_MODES, SRC_ALPHA_MODE,
                                              srcAlphaMode)
              | gcmSETFIELD(0, AQDE_ALPHA_MODES, DST_ALPHA_MODE,
                                              dstAlphaMode)
              | gcmSETFIELD(0, AQDE_ALPHA_MODES, GLOBAL_SRC_ALPHA_MODE,
                                              srcGlobalAlphaMode)
              | gcmSETFIELD(0, AQDE_ALPHA_MODES, GLOBAL_DST_ALPHA_MODE,
                                              dstGlobalAlphaMode)
              | gcmSETFIELD(0, AQDE_ALPHA_MODES, SRC_COLOR_MODE,
                                              srcColorMode)
              | gcmSETFIELD(0, AQDE_ALPHA_MODES, DST_COLOR_MODE,
                                              dstColorMode)
              | gcmSETFIELD(0, AQDE_ALPHA_MODES, SRC_BLENDING_MODE,
                                              srcFactorMode)
              | gcmSETFIELD(0, AQDE_ALPHA_MODES, DST_BLENDING_MODE,
                                              dstFactorMode);

    states[1] |= gcmSETFIELD(0, AQDE_ALPHA_MODES, SRC_ALPHA_FACTOR,
                                                  srcFactorExpansion)
              | gcmSETFIELD(0, AQDE_ALPHA_MODES, DST_ALPHA_FACTOR,
                                                  dstFactorExpansion);


    /* LoadState(AQDE_ALPHA_CONTROL, 2), states. */
    N2D_ON_ERROR(gcWriteRegWithLoadState(Hardware, AQDE_ALPHA_CONTROL_Address,
                                   2, states));

    if (SrcGlobalAlphaMode != gcvSURF_GLOBAL_ALPHA_OFF)
    {
        /* Set source global color. */
        N2D_ON_ERROR(gcSetSourceGlobalColor(
            Hardware,
            SrcGlobalAlphaValue
            ));
    }

    if (DstGlobalAlphaMode != gcvSURF_GLOBAL_ALPHA_OFF)
    {
        /* Set target global color. */
        N2D_ON_ERROR(gcSetTargetGlobalColor(
            Hardware,
            DstGlobalAlphaValue
            ));
    }

on_error:
    /* Return error. */
    return error;
}

n2d_error_t gcSetStretchFactors(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t HorFactor,
    IN n2d_uint32_t VerFactor
    )
{
    n2d_error_t error;
    n2d_uint32_t memory[2];

    /* Set states into temporary buffer. */
    memory[0] = HorFactor;
    memory[1] = VerFactor;

    /* Through load state command. */
    N2D_ON_ERROR(gcWriteRegWithLoadState(
        Hardware, AQDE_STRETCH_FACTOR_LOW_Address, 2, memory
        ));

on_error:
    return error;
}

n2d_uint32_t gcGetStretchFactor(
    IN n2d_bool_t GdiStretch,
    IN n2d_int32_t SrcSize,
    IN n2d_int32_t DestSize
    )
{
    n2d_uint32_t stretchFactor = 0;

    if (!GdiStretch && (SrcSize > 1) && (DestSize > 1))
    {
        stretchFactor = ((SrcSize - 1) << 16) / (DestSize - 1);
    }
    else if ((SrcSize > 0) && (DestSize > 0))
    {
        stretchFactor = (SrcSize << 16) / DestSize;
    }

    return stretchFactor;
}

n2d_error_t gcSetSuperTileVersion(
    IN n2d_user_hardware_t * Hardware,
    IN gce2D_SUPER_TILE_VERSION Version
    )
{
    n2d_error_t error;
    n2d_uint32_t value = 0;

    switch (Version)
    {
    case gcv2D_SUPER_TILE_VERSION_V1:
        value = GCREG_DE_GENERAL_CONFIG_SUPERTILE_VERSION_SUPER_TILE_V1;
        break;

    case gcv2D_SUPER_TILE_VERSION_V2:
        value = GCREG_DE_GENERAL_CONFIG_SUPERTILE_VERSION_SUPER_TILE_V2;
        break;

    case gcv2D_SUPER_TILE_VERSION_V3:
        value = GCREG_DE_GENERAL_CONFIG_SUPERTILE_VERSION_SUPER_TILE_V3;
        break;

    default:
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    /* Through load state command. */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_GENERAL_CONFIG_Address,
        gcmSETMASKEDFIELD(GCREG_DE_GENERAL_CONFIG, SUPERTILE_VERSION, value)
        ));

on_error:
    /* Return the error. */
    return error;
}


n2d_error_t gcTranslateGlobalColorMultiplyMode(
    IN  n2d_global_color_multiply_mode_t APIValue,
    OUT n2d_uint32_t * HwValue
    )
{

    /* Dispatch on transparency. */
    switch (APIValue)
    {
#ifdef AQDE_COLOR_MULTIPLY_MODES_Address
    case N2D_GLOBAL_COLOR_MULTIPLY_DISABLE:
        *HwValue = AQDE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_DISABLE;
        break;

    case N2D_GLOBAL_COLOR_MULTIPLY_ALPHA:
        *HwValue = AQDE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_ALPHA;
        break;

    case N2D_GLOBAL_COLOR_MULTIPLY_COLOR:
        *HwValue = AQDE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_COLOR;
        break;
#endif

    default:
        /* Not supported. */
        return N2D_NOT_SUPPORTED;
    }

    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcTranslatePixelColorMultiplyMode(
    IN  n2d_pixel_color_multiply_mode_t APIValue,
    OUT n2d_uint32_t * HwValue
    )
{

    /* Dispatch on transparency. */
    switch (APIValue)
    {
#ifdef AQDE_COLOR_MULTIPLY_MODES_Address
    case N2D_COLOR_MULTIPLY_DISABLE:
        *HwValue = AQDE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE;
        break;

    case N2D_COLOR_MULTIPLY_ENABLE:
        *HwValue = AQDE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE;
        break;
#endif

    default:
        /* Not supported. */
        return N2D_NOT_SUPPORTED;
    }

    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcTranslatePixelColorMode(
    IN gceSURF_PIXEL_COLOR_MODE APIValue,
    OUT n2d_uint32_t* HwValue
    )
{

#ifdef AQ_DRAWING_ENGINE_ALPHA_BLENDING_COLOR_MODE_NORMAL
    /* Dispatch on command. */
    switch (APIValue)
    {
    case gcvSURF_COLOR_STRAIGHT:
        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_COLOR_MODE_NORMAL;
        break;

    case gcvSURF_COLOR_MULTIPLY:
        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_COLOR_MODE_MULTIPLY;
        break;

    default:
        /* Not supported. */

        return N2D_NOT_SUPPORTED;
    }

    /* Success. */

    return N2D_SUCCESS;
#else
    /* Not supported. */
    return N2D_NOT_SUPPORTED;
#endif
}

n2d_error_t gcTranslateAlphaFactorMode(
    IN  n2d_bool_t                   IsSrcFactor,
    IN  gceSURF_BLEND_FACTOR_MODE APIValue,
    OUT n2d_uint32_t*                HwValue,
    OUT n2d_uint32_t*                FactorExpansion
    )
{
    n2d_error_t error;


    /* Set default value for the factor expansion. */
    if (IsSrcFactor)
    {
        *FactorExpansion = AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR_DISABLED;
    }
    else
    {
        *FactorExpansion = AQDE_ALPHA_MODES_DST_ALPHA_FACTOR_DISABLED;
    }

    /* Dispatch on command. */
    switch (APIValue)
    {
    case gcvSURF_BLEND_ZERO:
        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_ZERO;
        break;

    case gcvSURF_BLEND_ONE:
        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_ONE;
        break;

    case gcvSURF_BLEND_STRAIGHT:
        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_NORMAL;
        break;

    case gcvSURF_BLEND_STRAIGHT_NO_CROSS:

        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_NORMAL;

        if (IsSrcFactor)
        {
            *FactorExpansion = AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR_ENABLED;
        }
        else
        {
            *FactorExpansion = AQDE_ALPHA_MODES_DST_ALPHA_FACTOR_ENABLED;
        }
        break;

    case gcvSURF_BLEND_INVERSED:
        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_INVERSED;
        break;

    case gcvSURF_BLEND_INVERSED_NO_CROSS:

        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_INVERSED;
        if (IsSrcFactor)
        {
            *FactorExpansion = AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR_ENABLED;
        }
        else
        {
            *FactorExpansion = AQDE_ALPHA_MODES_DST_ALPHA_FACTOR_ENABLED;
        }
        break;

    case gcvSURF_BLEND_COLOR:

        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_COLOR;
        break;

    case gcvSURF_BLEND_COLOR_NO_CROSS:

        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_COLOR;
        if (IsSrcFactor)
        {
            *FactorExpansion = AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR_ENABLED;
        }
        else
        {
            *FactorExpansion = AQDE_ALPHA_MODES_DST_ALPHA_FACTOR_ENABLED;
        }
        break;

    case gcvSURF_BLEND_COLOR_INVERSED:

        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_COLOR_INVERSED;
        break;

    case gcvSURF_BLEND_COLOR_INVERSED_NO_CROSS:

        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_COLOR_INVERSED;

        if (IsSrcFactor)
        {
            *FactorExpansion = AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR_ENABLED;
        }
        else
        {
            *FactorExpansion = AQDE_ALPHA_MODES_DST_ALPHA_FACTOR_ENABLED;
        }
        break;

    case gcvSURF_BLEND_SRC_ALPHA_SATURATED:

        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_SATURATED_ALPHA;
        break;

    case gcvSURF_BLEND_SRC_ALPHA_SATURATED_CROSS:

        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_SATURATED_DEST_ALPHA;

        if (IsSrcFactor)
        {
            *FactorExpansion = AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR_ENABLED;
        }
        else
        {
            *FactorExpansion = AQDE_ALPHA_MODES_DST_ALPHA_FACTOR_ENABLED;
        }
        break;

    default:
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    /* Return the error. */
    return error;
}

n2d_error_t gcTranslateGlobalAlphaMode(
    IN gceSURF_GLOBAL_ALPHA_MODE APIValue,
    OUT n2d_uint32_t* HwValue
    )
{

#ifdef AQ_DRAWING_ENGINE_ALPHA_BLENDING_GLOBAL_ALPHA_MODE_NORMAL
    /* Dispatch on command. */
    switch (APIValue)
    {
    case gcvSURF_GLOBAL_ALPHA_OFF:
        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_GLOBAL_ALPHA_MODE_NORMAL;
        break;

    case gcvSURF_GLOBAL_ALPHA_ON:
        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_GLOBAL_ALPHA_MODE_GLOBAL;
        break;

    case gcvSURF_GLOBAL_ALPHA_SCALE:
        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_GLOBAL_ALPHA_MODE_SCALED;
        break;

    default:
        /* Not supported. */
        return N2D_NOT_SUPPORTED;
    }

    /* Success. */
    return N2D_SUCCESS;
#else
    /* Not supported. */
    return N2D_NOT_SUPPORTED;
#endif
}

n2d_error_t gcTranslatePixelAlphaMode(
    IN gceSURF_PIXEL_ALPHA_MODE APIValue,
    OUT n2d_uint32_t* HwValue
    )
{
    /* Dispatch on command. */
    switch (APIValue)
    {
    case gcvSURF_PIXEL_ALPHA_STRAIGHT:
        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_ALPHA_MODE_NORMAL;
        break;

    case gcvSURF_PIXEL_ALPHA_INVERSED:
        *HwValue = AQ_DRAWING_ENGINE_ALPHA_BLENDING_ALPHA_MODE_INVERSED;
        break;

    default:
        /* Not supported. */
        return N2D_NOT_SUPPORTED;
    }

    /* Success. */
    return N2D_SUCCESS;

}

/*******************************************************************************
**
**  gco2D_GetPixelAlignment
**
**  Returns the pixel alignment of the surface.
**
**  INPUT:
**
**      gceSURF_FORMAT Format
**          Pixel format.
**
**  OUTPUT:
**
**      gcsPOINT_PTR Alignment
**          Pointer to the pixel alignment values.
*/
// static n2d_error_t gcGetPixelAlignment(
//     n2d_buffer_format_t Format,
//     n2d_point_t *       Alignment
//     )
// {
//     n2d_error_t error;

//     const n2d_uint32_t BITS_PER_CACHELINE = 64 * 8;

//     n2d_uint32_t bpp;
//     N2D_ON_ERROR(gcConvertFormat(Format, &bpp));
//     /* Determine horizontal alignment. */
//     Alignment->x = BITS_PER_CACHELINE / bpp;

//     /* Vertical alignment for GC600 is simple. */
//     Alignment->y = 1;

//     /* Success. */
//     return N2D_SUCCESS;

// on_error:
//     /* Return the error. */
//     return error;
// }

n2d_error_t gcQueryFBPPs(
    IN n2d_buffer_format_t  Format,
    OUT n2d_uint32_t         *BppArray
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t    i = 0;
    FormatInfo_t    *FormatInfo = N2D_NULL;
    N2D_ON_ERROR(gcGetFormatInfo(Format,&FormatInfo));

    if (BppArray != N2D_NULL) {
        while(i < FormatInfo->planeNum) {
            BppArray[i] = FormatInfo->plane_bpp[i] / N2D_BITSPERBYTE;
            ++i;
        }
        while(i < N2D_THREEPLANES) {
            BppArray[i] = 0;
            ++i;
        }
    }

on_error:
    return error;
}

static n2d_error_t gcTranslateDFBColorKeyMode(
    IN  n2d_bool_t APIValue,
    OUT n2d_uint32_t * HwValue
    )
{

    if (APIValue == N2D_TRUE)
    {
        *HwValue = AQPE_TRANSPARENCY_DFB_COLOR_KEY_ENABLED;
    }
    else
    {
        *HwValue = AQPE_TRANSPARENCY_DFB_COLOR_KEY_DISABLED;
    }

    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcTranslatePatternTransparency(
    IN n2d_transparency_t APIValue,
    OUT n2d_uint32_t * HwValue
    )
{
    /* Dispatch on transparency. */
    switch (APIValue)
    {
    case N2D_OPAQUE:
        *HwValue = AQPE_TRANSPARENCY_PATTERN_OPAQUE;
        break;

    case N2D_MASKED:
        *HwValue = AQPE_TRANSPARENCY_PATTERN_MASK;
        break;

    default:
        /* Not supported. */

        return N2D_NOT_SUPPORTED;
    }

    /* Success. */
    return N2D_SUCCESS;
}

static n2d_error_t gcSetTransparencyModes(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_transparency_t SrcTransparency,
    IN n2d_transparency_t DstTransparency,
    IN n2d_transparency_t PatTransparency,
    IN n2d_uint8_t FgRop,
    IN n2d_uint8_t BgRop,
    IN n2d_bool_t EnableDFBColorKeyMode
    )
{
    n2d_error_t error = N2D_INVALID_ARGUMENT;

    {
        n2d_uint32_t srcTransparency;
        n2d_uint32_t dstTransparency;
        n2d_uint32_t patTransparency;
        n2d_uint32_t dfbColorKeyMode = 0;
        n2d_uint32_t transparencyMode;

        /* Compatible with PE1.0. */
        if (/*!Hardware->features[gcvFEATURE_ANDROID_ONLY]*/ 1
            && (PatTransparency == N2D_OPAQUE)
            && ((((FgRop >> 4) & 0x0F) != (FgRop & 0x0F))
            || (((BgRop >> 4) & 0x0F) != (BgRop & 0x0F))))
        {
            PatTransparency = N2D_MASKED;
        }

        /* Convert the transparency modes. */
        N2D_ON_ERROR(gcTranslateSourceTransparency(
            SrcTransparency, &srcTransparency
            ));

        N2D_ON_ERROR(gcTranslateTargetTransparency(
            DstTransparency, &dstTransparency
            ));

        N2D_ON_ERROR(gcTranslatePatternTransparency(
            PatTransparency, &patTransparency
            ));

        N2D_ON_ERROR(gcTranslateDFBColorKeyMode(
            EnableDFBColorKeyMode, &dfbColorKeyMode
            ));

        /* LoadState transparency modes.
           Enable Source or Destination read when
           respective Color key is turned on. */
        transparencyMode = gcmSETFIELD(0, AQPE_TRANSPARENCY, SOURCE,      srcTransparency)
                         | gcmSETFIELD(0, AQPE_TRANSPARENCY, DESTINATION, dstTransparency)
                         | gcmSETFIELD(0, AQPE_TRANSPARENCY, PATTERN,     patTransparency)
                         | gcmSETFIELD(0, AQPE_TRANSPARENCY, USE_SRC_OVERRIDE,
                            (srcTransparency == AQPE_TRANSPARENCY_SOURCE_KEY))
                         | gcmSETFIELD(0, AQPE_TRANSPARENCY, USE_DST_OVERRIDE,
                            (dstTransparency == AQPE_TRANSPARENCY_DESTINATION_KEY));

        transparencyMode |= gcmSETFIELD(0, AQPE_TRANSPARENCY, DFB_COLOR_KEY, dfbColorKeyMode);

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQPE_TRANSPARENCY_Address,
            transparencyMode
            ));
    }

on_error:
    /* Return error. */
    return error;
}


n2d_error_t gcColorConvertToARGB8(
    IN n2d_buffer_format_t Format,
    IN n2d_uint32_t NumColors,
    IN n2d_uint32_t*   Color,
    OUT n2d_uint32_t*   Color32
    )
{
    n2d_uint32_t colorR, colorG, colorB, colorA;
    n2d_uint32_t i;

    for (i = 0; i < NumColors; i++)
    {
        n2d_uint32_t color = Color[i];
        n2d_uint32_t * color32 = &Color32[i];

        switch(Format)
        {
        case N2D_RGBA8888:
            /* Extract colors. */
            colorR = (color & 0x000000FF);
            colorG = (color & 0x0000FF00) >>  8;
            colorB = (color & 0x00FF0000) >> 16;
            colorA = (color & 0xFF000000) >> 24;
            break;

        case N2D_R5G5B5A1:
            /* Extract colors. */
            colorB = (color & 0x7C00) >> 10;
            colorG = (color &  0x3E0) >>  5;
            colorR = (color &   0x1F);
            colorA = (color & 0x8000) >> 15;

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = colorA ? 0xFF : 0x00;
            break;

        case N2D_RGBA4444:
            /* Extract colors. */
            colorB = (color &  0xF00) >>  8;
            colorG = (color &   0xF0) >>  4;
            colorR = (color &    0xF);
            colorA = (color & 0xF000) >> 12;

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = colorA | (colorA << 4);
            break;

        case N2D_RGB565:
            /* Extract colors. */
            colorB = (color & 0xF800) >> 11;
            colorG = (color &  0x7E0) >>  5;
            colorR = (color &   0x1F);

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 2) | (colorG >> 4);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = 0xFF;
            break;

        case N2D_BGRA8888:
        case N2D_BGRX8888:
            /* No color conversion needed. */
            *color32 = color;
            continue;

        case N2D_B5G5R5A1:
            /* Extract colors. */
            colorB = (color &   0x1F);
            colorG = (color &  0x3E0) >>  5;
            colorR = (color & 0x7C00) >> 10;
            colorA = (color & 0x8000) >> 15;

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = colorA ? 0xFF : 0x00;
            break;

        case N2D_B5G5R5X1:
            /* Extract colors. */

            colorB = (color &   0x1F);
            colorG = (color &  0x3E0) >>  5;
            colorR = (color & 0x7C00) >> 10;

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = 0xFF;
            break;

        case N2D_BGRA4444:
            /* Extract colors. */
            colorB = (color &    0xF);
            colorG = (color &   0xF0) >>  4;
            colorR = (color &  0xF00) >>  8;
            colorA = (color & 0xF000) >> 12;

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = colorA | (colorA << 4);
            break;

        case N2D_BGRX4444:
            /* Extract colors. */
            colorB = (color &    0xF);
            colorG = (color &   0xF0) >>  4;
            colorR = (color &  0xF00) >>  8;

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = 0xFF;
            break;

        case N2D_BGR565:
            /* Extract colors. */
            colorB = (color &   0x1F);
            colorG = (color &  0x7E0) >>  5;
            colorR = (color & 0xF800) >> 11;

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 2) | (colorG >> 4);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = 0xFF;
            break;

        case N2D_ABGR8888:
            /* Extract colors. */
            colorB = (color & 0x0000FF00) >>  8;
            colorG = (color & 0x00FF0000) >> 16;
            colorR = (color & 0xFF000000) >> 24;
            colorA = (color & 0x000000FF);
            break;

        case N2D_A1B5G5R5:
            /* Extract colors. */
            colorB = (color & 0x003E) >>  1;
            colorG = (color & 0x07C0) >>  6;
            colorR = (color & 0xF800) >> 11;
            colorA = (color & 0x0001);

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = colorA ? 0xFF : 0x00;
            break;

        case N2D_ABGR4444:
            /* Extract colors. */
            colorB = (color &   0xF0) >>  4;
            colorG = (color &  0xF00) >>  8;
            colorR = (color & 0xF000) >> 12;
            colorA = (color &    0xF);

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = colorA | (colorA << 4);
            break;

        case N2D_ARGB8888:
            /* Extract colors. */
            colorB = (color & 0xFF000000) >> 24;
            colorG = (color & 0x00FF0000) >> 16;
            colorR = (color & 0x0000FF00) >>  8;
            colorA = (color & 0x000000FF);
            break;

        case N2D_A1R5G5B5:
            /* Extract colors. */
            colorB = (color & 0xF800) >> 11;
            colorG = (color & 0x07C0) >>  6;
            colorR = (color & 0x003E) >>  1;
            colorA = (color & 0x0001);

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = colorA ? 0xFF : 0x00;
            break;

        case N2D_ARGB4444:
            /* Extract colors. */
            colorB = (color & 0xF000) >> 12;
            colorG = (color &  0xF00) >>  8;
            colorR = (color &   0xF0) >>  4;
            colorA = (color &    0xF);

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = colorA | (colorA << 4);
            break;

        default:
            return N2D_NOT_SUPPORTED;
        }

        /* Assemble. */
        *color32 =
           (colorA << 24)
         | (colorR << 16)
         | (colorG <<  8)
         | colorB;
    }

    return N2D_SUCCESS;
}

static n2d_error_t gcTranslatePatternFormat(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_buffer_format_t APIValue,
    OUT n2d_uint32_t* HwValue,
    OUT n2d_uint32_t* HwSwizzleValue
    )
{
    n2d_error_t error;
    n2d_uint32_t formatEx;

    N2D_ON_ERROR(gcTranslateSourceFormat(Hardware, APIValue, HwValue, &formatEx, HwSwizzleValue));

    /* Check if format is supported as pattern. */
    switch (*HwValue)
    {
    case AQDE_SRC_CONFIG_FORMAT_X4R4G4B4:
    case AQDE_SRC_CONFIG_FORMAT_A4R4G4B4:
    case AQDE_SRC_CONFIG_FORMAT_X1R5G5B5:
    case AQDE_SRC_CONFIG_FORMAT_A1R5G5B5:
    case AQDE_SRC_CONFIG_FORMAT_R5G6B5:
    case AQDE_SRC_CONFIG_FORMAT_X8R8G8B8:
    case AQDE_SRC_CONFIG_FORMAT_A8R8G8B8:
        break;

    default:
        /* Not supported. */
        *HwValue = *HwSwizzleValue = 0;
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

on_error:
    /* Return the error. */
    return error;
}


static n2d_error_t gcGetFastClearValue(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_buffer_t        *Buffer,
    OUT n2d_uint32_t       *ClearValue
)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t clearValue = 0;

    if (Hardware->state.fastclear.enable && Hardware->features[N2D_FEATURE_DEC400_FC])
    {
        /*When the data in ts is all 1, HW automatically enables fc*/
        clearValue = 0x11;
        Buffer->tile_status_buffer.enableFastClear = N2D_TRUE;
        Buffer->tile_status_buffer.fastClearValue = Hardware->state.fastclear.value;
    }

    *ClearValue = (clearValue & 0xFF);

    /* Return the error. */
    return error;
}

// static n2d_bool_t gcIsYUVFormat(
//     n2d_buffer_format_t format)
// {
//     n2d_bool_t ret = N2D_FALSE;

//     FormatInfo_t *FormatInfo = N2D_NULL;
//     gcGetFormatInfo(format,&FormatInfo);

//     if(N2D_FORMARTTYPE_YUV == FormatInfo->formatType)
//         ret = N2D_TRUE;

//     return ret;
// }

static n2d_error_t gcLoadColorPattern(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t OriginX,
    IN n2d_uint32_t OriginY,
    IN n2d_uint32_t Address,
    IN n2d_buffer_format_t Format,
    IN n2d_uint64_t Mask
    )
{
    n2d_error_t error;

    {
        n2d_uint32_t format, swizzle, config;
        /* Convert the format. */
        N2D_ON_ERROR(gcTranslatePatternFormat(Hardware, Format, &format, &swizzle));

        /* LoadState(AQDE_PATTERN_MASK_LOW, 2), Mask. */
        N2D_ON_ERROR(gcWriteRegWithLoadState(
            Hardware,
            AQDE_PATTERN_MASK_LOW_Address, 2, &Mask
            ));

        /* LoadState(AQDE_PATTERN_ADDRESS, 1), Address. */
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQDE_PATTERN_ADDRESS_Address, Address
            ));

        /* Setup pattern configuration. */
        config
            = gcmSETFIELD     (0, AQDE_PATTERN_CONFIG, ORIGIN_X,     OriginX)
            | gcmSETFIELD     (0, AQDE_PATTERN_CONFIG, ORIGIN_Y,     OriginY)
            | gcmSETFIELDVALUE(0, AQDE_PATTERN_CONFIG, TYPE,         PATTERN)
            | gcmSETFIELD     (0, AQDE_PATTERN_CONFIG, FORMAT,       format)
            | gcmSETFIELD     (0, AQDE_PATTERN_CONFIG, PATTERN_FORMAT, format)
            | gcmSETFIELDVALUE(0, AQDE_PATTERN_CONFIG, INIT_TRIGGER, INIT_ALL);

        /* LoadState(AQDE_PATTERN_CONFIG, 1), config. */
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQDE_PATTERN_CONFIG_Address, config
            ));
    }

on_error:
    /* Return the error. */
    return error;
}

static n2d_bool_t gcIsYUVFormatSupportDEC400(
    n2d_buffer_format_t format)
{
    n2d_bool_t ret = N2D_FALSE;

    switch (format)
    {
    case N2D_NV12:
    case N2D_P010_MSB:
    case N2D_P010_LSB:
        ret = N2D_TRUE;
        break;

    default:
        ret = N2D_FALSE;
    }

    return ret;
}

n2d_error_t gcGetSurfaceBufferSize(
    IN n2d_user_hardware_t      *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_bool_t       only_getsize,
    OUT n2d_uint32_t    *Offset,
    OUT n2d_uint32_t    *SizeArray)
{
    n2d_error_t         error;
    n2d_uint32_t        alignedWidth,  alignedHeight, alignedStride;
    n2d_uint32_t        bits, size[3] = {0},offset = 64;
    n2d_buffer_format_t format;
    n2d_tiling_t        tiling;
    n2d_uint32_t         fbpps[3] = {0};

    alignedWidth= Surface->alignedWidth;
    alignedHeight = Surface->alignedHeight;
    format = Surface->format;
    tiling = Surface->tiling;
    N2D_ON_ERROR(gcConvertFormat(format, &bits));

    fbpps[0] = gcmALIGN(bits, 8) / 8;

    if(!only_getsize)
    {
        /* Align alignedWidth and alignedHeight. */
        switch (tiling)
        {
        case N2D_TILED:
            alignedWidth = gcmALIGN(alignedWidth, 8);
            alignedHeight = gcmALIGN(alignedHeight, 8);
            break;

        case N2D_MULTI_TILED:
            alignedWidth = gcmALIGN(alignedWidth, 16);
            alignedHeight = gcmALIGN(alignedHeight, 16);
            break;

        case N2D_SUPER_TILED:
        case N2D_SUPER_TILED_128B:
        case N2D_SUPER_TILED_256B:
        case N2D_YMAJOR_SUPER_TILED:
            alignedWidth = gcmALIGN(alignedWidth, 64);
            alignedHeight = gcmALIGN(alignedHeight, 64);
            break;

        case N2D_MULTI_SUPER_TILED:
            alignedWidth = gcmALIGN(alignedWidth, 64);
            alignedHeight = gcmALIGN(alignedHeight, 128);
            break;

        case N2D_TILED_8X8_XMAJOR:
            alignedWidth = gcmALIGN(alignedWidth, 16);
            alignedHeight = gcmALIGN(alignedHeight, 8);
            break;

        case N2D_TILED_8X8_YMAJOR:
            if (format == N2D_NV12)
            {
                alignedWidth = gcmALIGN(alignedWidth, 16);
                alignedHeight = gcmALIGN(alignedHeight, 64);
            }
            else if (format == N2D_P010_MSB || format == N2D_P010_LSB)
            {
                alignedWidth = gcmALIGN(alignedWidth, 8);
                alignedHeight = gcmALIGN(alignedHeight, 64);
            }
            break;

        case N2D_TILED_8X4:
        case N2D_TILED_4X8:
            alignedWidth = gcmALIGN(alignedWidth, 8);
            alignedHeight = gcmALIGN(alignedHeight, 8);
            break;

        case N2D_TILED_32X4:
            alignedWidth = gcmALIGN(alignedWidth, 64);
            alignedHeight = gcmALIGN(alignedHeight,64);
            break;

        case N2D_TILED_64X4:
            alignedWidth = gcmALIGN(alignedWidth, 64);
            alignedHeight = gcmALIGN(alignedHeight, 64);
            break;

        default:
            if (format == N2D_I010 || format == N2D_I010_LSB)
            {
                alignedWidth = gcmALIGN(alignedWidth, 32);
                alignedHeight = gcmALIGN(alignedHeight, 1);
            }
            else
            {
                alignedWidth = gcmALIGN(alignedWidth, 16);
                alignedHeight = gcmALIGN(alignedHeight, 1);
            }
            break;
        }

    }

    if (Hardware->features[N2D_FEATURE_DEC_COMPRESSION])
    {
        if(Surface->tile_status_config & N2D_TSC_DEC_COMPRESSED)
        {
          alignedWidth  = gcmALIGN(alignedWidth, 64);
          alignedHeight = gcmALIGN(alignedHeight, tiling == N2D_LINEAR ? 1 : 64);
          offset = 256;
          if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION] &&
                (Surface->tile_status_config & N2D_TSC_DEC_COMPRESSED))
          {
              if (format == N2D_P010_MSB || format == N2D_P010_LSB)
              {
                  offset = 512;
              }
          }
        }
        else
        {
            if(format == N2D_NV12 ||format == N2D_NV21 || format == N2D_P010_MSB || format == N2D_P010_LSB ||
                 format == N2D_YV12 || format == N2D_I420)
            {
                alignedWidth  = gcmALIGN(alignedWidth, 64);
                alignedHeight = gcmALIGN(alignedHeight, 64);
                if(tiling == N2D_TILED)
                {
                    offset = 256;
                }
            }
        }
    }
    else if (Surface->tile_status_config != N2D_TSC_DISABLE)
    {
        offset = 256;
    }

    alignedStride = (n2d_uint32_t)(alignedWidth * fbpps[0]);

    if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION] &&
            (Surface->tile_status_config & N2D_TSC_DEC_COMPRESSED))
    {
        if (format == N2D_P010_MSB || format == N2D_P010_LSB)
        {
            alignedStride = gcmALIGN(alignedStride, 64);
        }
        else if (format == N2D_NV12 || format == N2D_NV21)
        {
            alignedStride = gcmALIGN(alignedStride, 64);
        }
    }

    if (n2d_is_feature_support(N2D_FEATURE_YUV420_OUTPUT) &&
             (format == N2D_NV12 || format == N2D_NV21 || format == N2D_YV12 ||
              format == N2D_NV16 || format == N2D_NV61 || format == N2D_I420))
    {
        alignedStride = gcmALIGN(alignedStride, 64);
    }

    if (format == N2D_NV12_10BIT || format == N2D_NV21_10BIT ||
        format == N2D_NV16_10BIT || format == N2D_NV61_10BIT)
    {
        alignedStride = gcmALIGN(alignedStride, 80);
    }

    if (format == N2D_RGB888_PLANAR || format == N2D_AYUV)
    {
        alignedStride = gcmALIGN(alignedStride, 32);
        offset = 32;
    }

    size[0] = alignedStride * alignedHeight;

    if(only_getsize)
    {
        FormatInfo_t *FormatInfo = N2D_NULL;

        N2D_ON_ERROR(gcGetFormatInfo(format,&FormatInfo));

        N2D_ON_ERROR(gcQueryFBPPs(format, fbpps));

        /*calculate each plane size*/
        switch(FormatInfo->planeNum)
        {
            case N2D_THREEPLANES:
                size[0] = (n2d_uint32_t)(alignedWidth * alignedHeight * fbpps[0]);
                /*420*/
                if(format == N2D_YV12 || format == N2D_I420 || format == N2D_I010 || format == N2D_I010_LSB )
                {
                    size[1] = (n2d_uint32_t)(alignedWidth * alignedHeight / 4 * fbpps[1]);
                }
                else if(format == N2D_AYUV || format == N2D_RGB888_PLANAR || format == N2D_RGB888I_PLANAR ||
                    format == N2D_RGB161616I_PLANAR || format == N2D_RGB161616F_PLANAR || format == N2D_RGB323232F_PLANAR)
                {
                    size[1] = size[0];
                }
                else
                {
                    N2D_ON_ERROR(N2D_NOT_SUPPORTED);
                }

                size[2] = size[1];
                break;
            case N2D_TWOPLANES:
                size[0] = (n2d_uint32_t)(alignedWidth * alignedHeight * fbpps[0]);
                /*422*/
                if(format == N2D_NV16 || format == N2D_NV61 || format == N2D_NV16_10BIT || format == N2D_NV61_10BIT )
                {
                    size[1] =(n2d_uint32_t)( alignedWidth * alignedHeight  * fbpps[1]);
                }
                /*420*/
                else if(format == N2D_NV12 || format == N2D_NV21 || format == N2D_P010_MSB || format == N2D_P010_LSB||
                        format == N2D_NV12_10BIT || format == N2D_NV21_10BIT || format == N2D_P010_MSB || format == N2D_P010_LSB)
                {
                    size[1] = (n2d_uint32_t)( alignedWidth * alignedHeight * fbpps[1]);
                }
                else
                {
                    N2D_ON_ERROR(N2D_NOT_SUPPORTED);
                }
                break;
            case N2D_ONEPLANE:
                size[0] = alignedStride * alignedHeight;
                break;
            default:
                N2D_ON_ERROR(N2D_NOT_SUPPORTED);

        }
    }
    else
    {
        Surface->alignedHeight = alignedHeight;
        Surface->alignedWidth  = alignedWidth;
        Surface->stride[0]     = alignedStride;

        if(Offset != N2D_NULL)
        {
            *Offset = offset;
        }
    }

    if(SizeArray != N2D_NULL)
    {
        SizeArray[0] = size[0];
        SizeArray[1] = size[1];
        SizeArray[2] = size[2];
    }

    return N2D_SUCCESS;

on_error:
    /* Return the error. */
    return error;
}

n2d_error_t gcGetSurfaceTileStatusBufferSize(
    IN gcsSURF_INFO_PTR Surface,
    OUT n2d_uint32_t *tssize)
{
    n2d_error_t error;
    n2d_buffer_format_t             format = Surface->format;
    n2d_tiling_t                    tiling = Surface->tiling;
    n2d_uint32_t                     bpp,fBpps[3];
    /*only support two plane compression currently*/
    n2d_uint32_t                    size[2] = {0};
    n2d_uint32_t                    total = 0, tsbit = 0;
    n2d_uint32_t                    alignedWidth = Surface->alignedWidth;
    n2d_uint32_t                    alignedHeight = Surface->alignedHeight;

    /*tileStatusNode*/
    if (Surface->tile_status_config != N2D_TSC_DISABLE)
    {
        FormatInfo_t *FormatInfo = N2D_NULL;
        N2D_ON_ERROR(gcGetFormatInfo(format,&FormatInfo));

        bpp = FormatInfo->bpp  / N2D_BITSPERBYTE;

        N2D_ON_ERROR(gcQueryFBPPs(format,fBpps));

        /*one plane*/
        if (N2D_ONEPLANE == FormatInfo->planeNum)
        {
            static n2d_uint32_t tileBitsArray[3][5] =
            {
           /* 16x4 8x8 8x4 4x8 4x4 */
                {8, 8, 8, 8, 8}, /* 16 aligned */
                {4, 4, 4, 4, 4}, /* 32 aligned */
                {4, 4, 4, 4, 4}, /* 64 aligned */
            };

            switch (tiling)
            {
                case N2D_LINEAR:
                    if (Surface->cacheMode == N2D_CACHE_128)
                    {
                        if (bpp == 4)
                        {
                            total = 32;
                            tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][2];
                        }
                        else if (bpp == 2)
                        {
                            total = 64;
                            tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][1];
                        }
                    }
                    else if (Surface->cacheMode == N2D_CACHE_256)
                    {
                        if (bpp == 4)
                        {
                            total = 64;
                            tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][1];
                        }
                        else if (bpp == 2)
                        {
                            total = 128;
                            tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][0];
                        }
                    }
                    break;
                case N2D_TILED_8X8_XMAJOR:
                case N2D_SUPER_TILED_256B:
                case N2D_SUPER_TILED:
                case N2D_YMAJOR_SUPER_TILED:
                    total = 32;
                    tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][1];
                    break;

                case N2D_SUPER_TILED_128B:
                    if (bpp == 4)
                    {
                        total = 32;
                        tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][2];
                    }
                    else
                    {
                        total = 64;
                        tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][1];
                    }
                    break;

                case N2D_TILED_8X4:
                    total = 32;
                    tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][2];
                    break;

                case N2D_TILED_4X8:
                    total = 32;
                    tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][3];
                    break;

                case N2D_TILED_32X4:
                    total = 128;
                    tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][3];
                    break;

                default:
                    N2D_ON_ERROR(N2D_NOT_SUPPORTED);
                    break;
            }

            if (total && tsbit)
            {
                size[0] = (n2d_uint32_t)(alignedWidth * alignedHeight / total * tsbit / 8);
            }
        }
        else
        {
            static n2d_uint32_t tileBitsArray[3][3] =
            {
                /* 256 128 64 */
                {4, 4, 2}, /* 16 aligned */
                {4, 2, 1}, /* 32 aligned */
                {2, 1, 1}, /* 64 aligned */
            };

            if ((format == N2D_P010_MSB || format == N2D_P010_LSB))
            {
                if (tiling == N2D_TILED_32X4 || tiling == N2D_LINEAR)
                {
                    total = 128;
                    tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][0];
                }
                else if (tiling == N2D_TILED)
                {
                    total = 16;
                    tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][0];
                }
            }
            else if ((format == N2D_NV12 || format == N2D_NV21))
            {
                switch (tiling)
                {
                case N2D_TILED_64X4:
                    total = 64 * 4;
                    /* Y tile status size = 64 * 4 * 1 = 256 */
                    tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][0];
                    break;
                case N2D_LINEAR:
                    total = 128;
                    tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][0];
                    break;
                case N2D_TILED:
                    total = 16;
                    tsbit = tileBitsArray[NANO2D_COMPRESSION_DEC400_ALIGN_MODE][0];
                    break;
                default:
                    break;
                }
            }

            if (total && tsbit)
            {
                size[0] = (n2d_uint32_t)(alignedWidth * alignedHeight / total * tsbit / 8);
            }

            /*2plane, 420*/
            if((format == N2D_P010_MSB || format == N2D_P010_LSB) || (format == N2D_NV12  || format == N2D_NV21 ) )
            {
                size[1] = size[0] / 2;
            }
        }

   }

    if(tssize != N2D_NULL)
    {
        tssize[0] = size[0];
        tssize[1] = size[1];
    }

    return N2D_SUCCESS;

on_error:
    /* Return the error. */
    return error;
}

n2d_error_t gcAdjustSurfaceBufferParameters(
    IN n2d_user_hardware_t *Hardware,
    n2d_buffer_t *buffer)
{
    n2d_uint32_t fBPPs[3];
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t offset = 0,align = 0;
    FormatInfo_t *FormatInfo = N2D_NULL;

    N2D_ON_ERROR(gcQueryFBPPs(buffer->format, fBPPs));
    N2D_ON_ERROR(gcGetFormatInfo(buffer->format,&FormatInfo));

    switch (buffer->format)
    {
    case N2D_YV12:
    case N2D_I420:
        buffer->stride = gcmALIGN(buffer->alignedw * fBPPs[0], 64);
        /*  WxH Y plane followed by (W/2)x(H/2) V and U planes. */
        if(Hardware->features[N2D_FEATURE_YUV420_OUTPUT])
        {
            buffer->uvstride[0] =
            buffer->uvstride[1] = gcmALIGN(buffer->stride >> 1, 32);
        }
        else
        {
            buffer->uvstride[0] =
            buffer->uvstride[1] = buffer->stride;
        }
        buffer->uv_memory[0] = (n2d_pointer)((n2d_uintptr_t)(buffer->memory)
            + (n2d_size_t)buffer->stride * buffer->alignedh);
        buffer->uv_memory[1] = (n2d_pointer)((n2d_uintptr_t)
            (buffer->uv_memory[0]) + buffer->uvstride[0] * buffer->alignedh / 2);
        buffer->uv_gpu[0]    = buffer->gpu + buffer->stride * buffer->alignedh;
        buffer->uv_gpu[1]    = buffer->uv_gpu[0] + buffer->uvstride[0] * buffer->alignedh / 2;
        break;

    case N2D_NV12_10BIT:
    case N2D_NV21_10BIT:
        buffer->stride = gcmALIGN(buffer->alignedw * fBPPs[0], 80);
        /*  WxH Y plane followed by (W)x(H/2) interleaved U/V plane. */
        buffer->uvstride[0]  =
        buffer->uvstride[1]  = buffer->stride;
        buffer->uv_memory[0] =
        buffer->uv_memory[1] = (n2d_pointer)((n2d_uintptr_t)(buffer->memory)
            + (n2d_size_t)buffer->stride * buffer->alignedh);
        buffer->uv_gpu[0]    =
        buffer->uv_gpu[1]    = buffer->gpu + buffer->stride * buffer->alignedh;
        break;

    case N2D_NV12:
    case N2D_NV21:
    case N2D_NV16:
    case N2D_NV61:
        buffer->stride = gcmALIGN(buffer->alignedw * fBPPs[0], 64);
        /*  WxH Y plane followed by WxH interleaved U/V(V/U) plane. */
        buffer->uvstride[0]  =
        buffer->uvstride[1]  = buffer->stride;
        buffer->uv_memory[0] =
        buffer->uv_memory[1] = (n2d_pointer)((n2d_uintptr_t)(buffer->memory)
            + (n2d_size_t)buffer->stride * buffer->alignedh);
        buffer->uv_gpu[0]    =
        buffer->uv_gpu[1]    = buffer->gpu + buffer->stride * buffer->alignedh;
        break;

    case N2D_NV16_10BIT:
    case N2D_NV61_10BIT:
        buffer->stride = gcmALIGN(buffer->alignedw * fBPPs[0], 40);
        /*  WxH Y plane followed by WxH interleaved U/V(V/U) plane. */
        buffer->uvstride[0]  =
        buffer->uvstride[1]  = buffer->stride;
        buffer->uv_memory[0] =
        buffer->uv_memory[1] = (n2d_pointer)((n2d_uintptr_t)(buffer->memory)
            + (n2d_size_t)buffer->stride * buffer->alignedh);
        buffer->uv_gpu[0]    =
        buffer->uv_gpu[1]    = buffer->gpu + buffer->stride * buffer->alignedh;
        break;

    case N2D_P010_MSB:
    case N2D_P010_LSB:
        buffer->stride = gcmALIGN(buffer->alignedw * fBPPs[0], 128);
        /*  WxH Y plane followed by (W)x(H/2) interleaved U/V plane. */
        buffer->uvstride[0]  =
        buffer->uvstride[1]  = buffer->stride;
        buffer->uv_memory[0] =
        buffer->uv_memory[1] = (n2d_pointer)((n2d_uintptr_t)(buffer->memory)
            + (n2d_size_t)buffer->stride * buffer->alignedh);
        buffer->uv_gpu[0]    =
        buffer->uv_gpu[1]    = buffer->gpu + buffer->stride * buffer->alignedh;
        break;

    case N2D_I010:
    case N2D_I010_LSB:
        buffer->stride = gcmALIGN(buffer->alignedw * fBPPs[0], 128);
        /*  WxH Y plane followed by (W/2)x(H/2) V and U planes. */
        buffer->uvstride[0]  =
        buffer->uvstride[1]  = buffer->stride / 2;
        buffer->uv_memory[0] = (n2d_pointer)((n2d_uintptr_t)(buffer->memory)
            + (n2d_size_t)buffer->stride * buffer->alignedh);
        buffer->uv_memory[1] = (n2d_pointer)((n2d_uintptr_t)(buffer->uv_memory[0]) + buffer->uvstride[0] * buffer->alignedh / 2);
        buffer->uv_gpu[0]    = buffer->gpu + buffer->stride * buffer->alignedh;
        buffer->uv_gpu[1]    = buffer->uv_gpu[0] + buffer->uvstride[0] * buffer->alignedh / 2;
        break;

    case N2D_AYUV:
        align = 32;
        buffer->stride = gcmALIGN(buffer->alignedw * fBPPs[0], 32);
        buffer->uvstride[0]  =
        buffer->uvstride[1]  = buffer->stride;
        buffer->uv_gpu[0]    = gcmALIGN(buffer->gpu + buffer->stride * buffer->alignedh,align);
        offset = (n2d_uint32_t)(buffer->uv_gpu[0] - (buffer->gpu + buffer->stride * buffer->alignedh));
        buffer->uv_memory[0] = (n2d_pointer)((n2d_uintptr_t)(buffer->memory)
            + (n2d_size_t)buffer->stride * buffer->alignedh + offset);
        buffer->uv_gpu[1]    = gcmALIGN(buffer->uv_gpu[0] + buffer->uvstride[0] * buffer->alignedh,align);
        offset = (n2d_uint32_t)(buffer->uv_gpu[1] - (buffer->uv_gpu[0] + buffer->uvstride[0] * buffer->alignedh));
        buffer->uv_memory[1] = (n2d_pointer)((n2d_uintptr_t)(buffer->uv_memory[0])
            + (n2d_size_t)buffer->uvstride[0] * buffer->alignedh + offset);
        break;

    case N2D_RGB888_PLANAR:
    case N2D_RGB888I_PLANAR:
    case N2D_RGB161616I_PLANAR:
    case N2D_RGB161616F_PLANAR:
    case N2D_RGB323232F_PLANAR:
        align = 32;
        buffer->stride = gcmALIGN(buffer->alignedw * fBPPs[0], 16);
        buffer->uvstride[0]  =
        buffer->uvstride[1]  = buffer->stride;
        buffer->uv_gpu[0]    = gcmALIGN(buffer->gpu + buffer->stride * buffer->alignedh,align);
        offset = (n2d_uint32_t)(buffer->uv_gpu[0] - (buffer->gpu + buffer->stride * buffer->alignedh));
        buffer->uv_memory[0] = (n2d_pointer)((n2d_uintptr_t)(buffer->memory)
            + (n2d_size_t)buffer->stride * buffer->alignedh + offset);
        buffer->uv_gpu[1]    = gcmALIGN(buffer->uv_gpu[0] + buffer->uvstride[0] * buffer->alignedh,align);
        offset = (n2d_uint32_t)(buffer->uv_gpu[1] - (buffer->uv_gpu[0] + buffer->uvstride[0] * buffer->alignedh));
        buffer->uv_memory[1] = (n2d_pointer)((n2d_uintptr_t)(buffer->uv_memory[0])
            + (n2d_size_t)buffer->uvstride[0] * buffer->alignedh + offset);
        break;

    default:
        buffer->uvstride[0] = buffer->uvstride[1] = 0;
        break;
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcAllocSurfaceBuffer(
    n2d_user_hardware_t  *hardware,
    n2d_buffer_t    *buffer)
{
    n2d_error_t  error;
    n2d_uint32_t size[3], offset = 64, actualSize = 0;
    n2d_uintptr_t  handle = 0;
    n2d_ioctl_interface_t   iface = {0};
    gcsSURF_INFO            surface;

    surface.alignedWidth  = buffer->width;
    surface.alignedHeight = buffer->height;
    surface.format        = buffer->format;
    surface.tiling        = buffer->tiling;
    surface.tile_status_config = buffer->tile_status_config;

    N2D_ON_ERROR(gcGetSurfaceBufferSize(hardware, &surface,N2D_FALSE,&offset,size));

    buffer->srcType = N2D_SOURCE_DEFAULT;

    /*This stride only applies to one planar buffer and the multiplanar buffer needs to be recalculated later*/
    buffer->stride   = surface.stride[0];
    buffer->alignedw = surface.alignedWidth;
    buffer->alignedh = surface.alignedHeight;

    N2D_ON_ERROR(gcAllocateGpuMemory(hardware, (n2d_size_t)size[0] + 2048, N2D_FALSE, &iface));
    buffer->handle = handle = iface.u.command_allocate.handle;
    actualSize = iface.u.command_allocate.size;

    n2d_user_os_memory_fill(&iface, 0, N2D_SIZEOF(n2d_ioctl_interface_t));

    iface.u.command_map.handle = handle;
    N2D_ON_ERROR(gcMapMemory(hardware,handle,&iface));

#ifdef N2D_SUPPORT_SECURE_BUFFER
    gcSaveSecureBufferState(
        hardware,
        (n2d_uint32_t)iface.u.command_map.address,
        (n2d_bool_t)iface.u.command_map.secure);
#endif

    /* Align the address. */
    buffer->gpu = gcmALIGN(iface.u.command_map.address, offset);

#ifdef N2D_SUPPORT_64BIT_ADDRESS
    buffer->gpuEx = (n2d_uintptr_t)(iface.u.command_map.address >> 0x20);
#endif

    offset = (n2d_uint32_t)(buffer->gpu - iface.u.command_map.address);
    /* Zero the memory. */
    n2d_user_os_memory_fill((n2d_pointer)iface.u.command_map.logical, 0, actualSize);
    /* Align the memory address. */
    buffer->memory = (n2d_uint8_t *)(iface.u.command_map.logical) + offset;

    /* Success. */
    return N2D_SUCCESS;

on_error:

    return error;
}

n2d_error_t gcAllocSurfaceTileStatusBuffer(
    n2d_user_hardware_t *hardware,
    n2d_buffer_t *buffer)
{
    n2d_uintptr_t           handle = 0;
    n2d_ioctl_interface_t   iface = {0};
    gcsSURF_INFO            surface;
    n2d_error_t     error;
    n2d_uint32_t    size[2] = {0}, offset = 64, clearValue = 0, actualSize = 0;

    N2D_ON_ERROR(gcGetFastClearValue(hardware,buffer,&clearValue));

    /* support N2D_FEATURE_DEC400_COMPRESSION*/
    if (gcIsDEC400Avaiable(hardware, buffer->tile_status_config))
    {
        /*Plane 1*/
        surface.alignedWidth  = buffer->alignedw;
        surface.alignedHeight = buffer->alignedh;
        surface.format        = buffer->format;
        surface.tiling        = buffer->tiling;
        surface.tile_status_config       = buffer->tile_status_config;
        surface.cacheMode     = buffer->cacheMode;

        N2D_ON_ERROR(gcGetSurfaceTileStatusBufferSize(&surface,size));
        N2D_ON_ERROR(gcAllocateGpuMemory(hardware,
            (n2d_size_t)size[0] + offset, N2D_FALSE , &iface));

        actualSize = iface.u.command_allocate.size;

        buffer->tile_status_buffer.handle[0] = handle = iface.u.command_allocate.handle;
        n2d_user_os_memory_fill(&iface, 0, N2D_SIZEOF(n2d_ioctl_interface_t));

        iface.u.command_map.handle = handle;
        N2D_ON_ERROR(gcMapMemory(hardware,handle,&iface));

        /* Zero the memory. */
        n2d_user_os_memory_fill((n2d_pointer)iface.u.command_map.logical, (n2d_uint8_t)clearValue, actualSize);
        /* Align the address. */
        buffer->tile_status_buffer.gpu_addr[0] = gcmALIGN(iface.u.command_map.address, offset);

#ifdef N2D_SUPPORT_64BIT_ADDRESS
        buffer->ts_gpuAddrEx = (n2d_uintptr_t)(iface.u.command_map.address >> 0x20);
#endif
        offset = (n2d_uint32_t)(buffer->tile_status_buffer.gpu_addr[0] - iface.u.command_map.address);
        buffer->tile_status_buffer.memory[0] = (n2d_uint8_t *)(iface.u.command_map.logical) + offset;

        if(gcIsYUVFormatSupportDEC400(buffer->format))
        {
            /*Plane 2*/
            offset = 64;
            n2d_user_os_memory_fill(&iface, 0, N2D_SIZEOF(n2d_ioctl_interface_t));
            N2D_ON_ERROR(gcAllocateGpuMemory(hardware,
                (n2d_size_t)size[1] + offset, N2D_FALSE , &iface));

            actualSize = iface.u.command_allocate.size;

            buffer->tile_status_buffer.handle[1] = handle = iface.u.command_allocate.handle;

            n2d_user_os_memory_fill(&iface, 0, N2D_SIZEOF(n2d_ioctl_interface_t));

            iface.u.command_map.handle = handle;
            N2D_ON_ERROR(gcMapMemory(hardware,handle,&iface));

            /* Zero the memory. */
            n2d_user_os_memory_fill((n2d_pointer)iface.u.command_map.logical, (n2d_uint8_t)clearValue, actualSize);
            /* Align the address. */
            buffer->tile_status_buffer.gpu_addr[1] = gcmALIGN(iface.u.command_map.address, offset);
            offset = (n2d_uint32_t)(buffer->tile_status_buffer.gpu_addr[1] - iface.u.command_map.address);
            buffer->tile_status_buffer.memory[1] = (n2d_uint8_t *)(iface.u.command_map.logical) + offset;
        }

    }
    else
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcConvertBufferToSurfaceBuffer(
    gcsSURF_INFO_PTR    dst,
    n2d_buffer_t        *src)
{
    n2d_error_t error;

    if(N2D_NULL == dst ||  N2D_NULL == src)
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);

    dst->stride[0]      = src->stride;
    dst->stride[1]      = src->uvstride[0];
    dst->stride[2]      = src->uvstride[1];
    dst->gpuAddress[0]  = src->gpu;
    dst->gpuAddress[1]  = src->uv_gpu[0];
    dst->gpuAddress[2]  = src->uv_gpu[1];
    dst->memory         = src->memory;
    dst->uv_memory[0]   = src->uv_memory[0];
    dst->uv_memory[1]   = src->uv_memory[1];
    dst->alignedWidth   = src->alignedw;
    dst->alignedHeight  = src->alignedh;
    dst->tiling         = src->tiling;
    dst->cacheMode      = src->cacheMode;
    dst->tile_status_config   = src->tile_status_config;
    n2d_user_os_memory_copy(&dst->tile_status_buffer,&src->tile_status_buffer,sizeof(n2d_tile_status_buffer_t));
    dst->format = src->format;
    dst->rotation = src->orientation;

#ifdef N2D_SUPPORT_64BIT_ADDRESS
    dst->gpuAddrEx = src->gpuEx;
    dst->ts_gpuAddrEx = src->ts_gpuAddrEx;
#endif

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

static n2d_error_t gcResetCmdBuffer(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_bool_t CleanCmd)
{
    if (CleanCmd)
    {
        Hardware->cmdIndex = 0;
    }

    return N2D_SUCCESS;
}

/*
* check if it supports one pass filter
*/
// static n2d_error_t gcCheckOPF(
//     IN n2d_user_hardware_t   *Hardware,
//     IN gcsSURF_INFO_PTR SrcSurface,
//     IN gcs2D_State_PTR  State,
//     OUT n2d_bool_t      *UseOPF
//     )
// {
//     n2d_error_t error;
//     n2d_bool_t  res = N2D_FALSE;

//     if(!Hardware->features[N2D_FEATURE_SCALER] &&
//         !Hardware->features[N2D_FEATURE_2D_ONE_PASS_FILTER])
//     {
//         N2D_ON_ERROR(N2D_NOT_SUPPORTED);
//     }
//     else
//     {
//         if(Hardware->features[N2D_FEATURE_YUV420_OUTPUT])
//         {
//             Hardware->notAdjustRotation = N2D_TRUE;
//         }

//         if(SrcSurface->rotation != N2D_0 || SrcSurface->format == N2D_A8)
//         {
//             res = N2D_FALSE;
//         }
//         else if(Hardware->hw2DQuad
//                 && ((State->filterState.newHorKernelSize == 5 ||State->filterState.newHorKernelSize == 3
//                 || State->filterState.newHorKernelSize == 1)
//                 && (State->filterState.newVerKernelSize == 5 || State->filterState.newVerKernelSize == 3
//                 || State->filterState.newVerKernelSize == 1)))
//         {
//             res = N2D_TRUE;
//         }

//         if(UseOPF != N2D_NULL)
//         {
//            *UseOPF = res;
//         }

//     }

//     /* Success. */
//     return N2D_SUCCESS;

// on_error:
//     return error;
// }

// static n2d_error_t gcStartVR(
//     IN n2d_user_hardware_t * Hardware,
//     IN gcs2D_State_PTR State,
//     IN gceFILTER_BLIT_TYPE type,
//     IN gcsFILTER_BLIT_ARRAY_PTR HorKernel,
//     IN gcsFILTER_BLIT_ARRAY_PTR VerKernel,
//     IN gcsSURF_INFO_PTR SrcSurface,
//     IN gcsRECT_PTR SrcRect,
//     IN n2d_point_t * SrcOrigin,
//     IN gcsSURF_INFO_PTR DstSurface,
//     IN gcsRECT_PTR DstRect,
//     IN n2d_bool_t PrePass
//     )
// {
//     n2d_error_t error;
//     gcs2D_MULTI_SOURCE_PTR curSrc = &State->multiSrc[State->currentSrcIndex];
//     n2d_uint32_t blitType = 0;
//     n2d_uint32_t memory[4];
//     n2d_uint32_t anyCompress = 0;
//     n2d_user_cmd_buf_t        *cmd_buf = N2D_NULL;
//     n2d_bool_t  inCSC = N2D_FALSE, outCSC = N2D_FALSE;

//     N2D_PERF_TIME_STAMP_PRINT(__FUNCTION__, "START");

//     N2D_ASSERT(Hardware != N2D_NULL);

//     /*current cmdBuf logical base startmemory and offset of last use*/
//     Hardware->cmdCount   = gcCalCmdbufSize(Hardware,State,gcv2D_FILTER_BLT);
//     Hardware->cmdIndex   = 0;
//     N2D_ON_ERROR(gcReserveCmdBuf(Hardware,Hardware->cmdCount * N2D_SIZEOF(n2d_uint32_t),&cmd_buf));
//     Hardware->cmdLogical = (n2d_pointer)(cmd_buf->last_reserve);

//     N2D_ON_ERROR(gcRenderBegin(Hardware));

//     inCSC = outCSC = gcNeedUserCSC(State->dstCSCMode, DstSurface->format);
//     inCSC = inCSC || gcNeedUserCSC(curSrc->srcCSCMode, curSrc->srcSurface.format);

//     if(Hardware->features[N2D_FEATURE_2D_10BIT_OUTPUT_LINEAR])
//     {
//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             AQPE_CONTROL_Address,
//             gcmSETMASKEDFIELDVALUE(AQPE_CONTROL, YUVRGB, ENABLED)
//             ));
//     }
//     else
//     {
//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             AQPE_CONTROL_Address,
//             gcmSETMASKEDFIELDVALUE(AQPE_CONTROL, YUVRGB, DISABLED)
//             ));
//     }

//     /* Set alphablend states: if filter plus alpha is not supported, make
//        sure the alphablend is disabled.
//     */
//     if (!PrePass && curSrc->enableAlpha)
//     {
//         /* Set alphablend states */
//         N2D_ON_ERROR(gcEnableAlphaBlend(
//                 Hardware,
//                 curSrc->srcAlphaMode,
//                 curSrc->dstAlphaMode,
//                 curSrc->srcGlobalAlphaMode,
//                 curSrc->dstGlobalAlphaMode,
//                 curSrc->srcFactorMode,
//                 curSrc->dstFactorMode,
//                 curSrc->srcColorMode,
//                 curSrc->dstColorMode,
//                 curSrc->srcGlobalColor,
//                 curSrc->dstGlobalColor
//                 ));
//     }
//     else
//     {
//         /* LoadState(AQDE_ALPHA_CONTROL, AlphaOff). */
//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             AQDE_ALPHA_CONTROL_Address,
//             gcmSETFIELDVALUE(0, AQDE_ALPHA_CONTROL, ENABLE, OFF)));
//     }

//     /* Set mirror state. */
//     if (!PrePass)
//     {
//         N2D_ON_ERROR(gcSetBitBlitMirror(
//             Hardware,
//             curSrc->horMirror,
//             curSrc->verMirror,
//             N2D_TRUE
//             ));

//         /* Set multiply modes. */
//         N2D_ON_ERROR(gcSetMultiplyModes(
//             Hardware,
//             curSrc->srcPremultiplyMode,
//             curSrc->dstPremultiplyMode,
//             curSrc->srcPremultiplyGlobalMode,
//             curSrc->dstDemultiplyMode,
//             curSrc->srcGlobalColor
//             ));
//     }
//     else
//     {
//         N2D_ON_ERROR(gcSetBitBlitMirror(
//             Hardware,
//             N2D_FALSE,
//             N2D_FALSE,
//             N2D_TRUE
//             ));

//         /* Disable multiply. */
//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             AQDE_COLOR_MULTIPLY_MODES_Address,
//             AQDE_COLOR_MULTIPLY_MODES_ResetValue));
//     }

//     /* Set 2D dithering. */
//     N2D_ON_ERROR(gcSetDither(
//         Hardware,
//         0       /* Disabled */
//         ));

// #ifdef AQVR_CONFIG_EX_Address

//     if(Hardware->features[N2D_FEATURE_2D_OPF_YUV_OUTPUT])
//     {
//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             AQVR_CONFIG_EX_Address,
//             gcmSETMASKEDFIELDVALUE(AQVR_CONFIG_EX, DISABLE_DUAL_PIPE_OPF, DISABLED)
//             ));
//     }
//     else
//     {
//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             AQVR_CONFIG_EX_Address,
//             gcmSETMASKEDFIELDVALUE(AQVR_CONFIG_EX, DISABLE_DUAL_PIPE_OPF, ENABLED)
//             ));
//     }
// #endif

//     switch (type)
//     {
//     case gceFILTER_BLIT_TYPE_VERTICAL:
//         blitType = AQVR_CONFIG_START_VERTICAL_BLIT;

//         N2D_ON_ERROR(gcLoadFilterKernel(
//             Hardware,
//             gcvFILTER_BLIT_KERNEL_UNIFIED,
//             VerKernel
//             ));

//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             AQDE_STRETCH_FACTOR_HIGH_Address,
//             VerKernel->scaleFactor
//             ));

// #ifdef AQVR_CONFIG_EX_Address
//         {
//             n2d_uint32_t srcbpp, dstbpp;
//             n2d_uint32_t configEx;

//             N2D_ON_ERROR(gcConvertFormat(SrcSurface->format, &srcbpp));

//             N2D_ON_ERROR(gcConvertFormat(DstSurface->format, &dstbpp));

//             if (((gcmGET_PRE_ROTATION(SrcSurface->rotation) == N2D_90)
//                 || (gcmGET_PRE_ROTATION(SrcSurface->rotation) == N2D_270)
//                 || (gcmGET_PRE_ROTATION(DstSurface->rotation) == N2D_90)
//                 || (gcmGET_PRE_ROTATION(DstSurface->rotation) == N2D_270))
//                 && (srcbpp != 32 || dstbpp != 32))
//             {
//                 configEx = gcmSETMASKEDFIELDVALUE(AQVR_CONFIG_EX, VERTICAL_LINE_WIDTH, PIXELS16);
//             }
//             else
//             {
//                 configEx = gcmSETMASKEDFIELDVALUE(AQVR_CONFIG_EX, VERTICAL_LINE_WIDTH, AUTO);
//             }

//             N2D_ON_ERROR(gcWriteRegWithLoadState32(
//                 Hardware,
//                 AQVR_CONFIG_EX_Address,
//                 configEx
//                 ));
//         }
// #endif
//         break;

//     case gceFILTER_BLIT_TYPE_HORIZONTAL:
//         blitType = AQVR_CONFIG_START_HORIZONTAL_BLIT;

//         N2D_ON_ERROR(gcLoadFilterKernel(
//             Hardware,
//             gcvFILTER_BLIT_KERNEL_UNIFIED,
//             HorKernel
//             ));

//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             AQDE_STRETCH_FACTOR_LOW_Address,
//             HorKernel->scaleFactor
//             ));

//         break;

//     case gceFILTER_BLIT_TYPE_ONE_PASS:

// #ifdef AQVR_CONFIG_EX_Address
//     {
//         n2d_uint8_t horTap, verTap;
//         n2d_uint32_t configEx;

//         if (Hardware->features[N2D_FEATURE_2D_OPF_YUV_OUTPUT])
//         {
//             horTap = (HorKernel->kernelSize == 1 ? 3 : HorKernel->kernelSize);
//             verTap = (VerKernel->kernelSize == 1 ? 3 : VerKernel->kernelSize);
//         }
//         else
//         {
//             horTap = HorKernel->kernelSize;
//             verTap = VerKernel->kernelSize;
//         }

//         if (SrcSurface->tiling != N2D_LINEAR &&
//             Hardware->features[N2D_FEATURE_SEPARATE_SRC_DST])
//         {
//             if (horTap == 7 || horTap == 9)
//                 horTap = 5;

//             if (verTap == 7 || verTap == 9)
//                 verTap = 5;
//         }

//         N2D_ON_ERROR(gcSetOPFBlockSize(
//             Hardware,
//             SrcSurface, DstSurface,
//             SrcRect, DstRect,
//             horTap, verTap));

//         configEx =
//             gcmSETMASKEDFIELD(AQVR_CONFIG_EX, FILTER_TAP, N2D_MAX(horTap, verTap));

//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             AQVR_CONFIG_EX_Address,
//             configEx
//             ));
//     }

//     blitType = AQVR_CONFIG_START_ONE_PASS_BLIT;

//     /* Uploading two kernel tables. */
//     if (HorKernel->kernelStates != N2D_NULL)
//     {
//         N2D_ON_ERROR(gcLoadFilterKernel(
//             Hardware,
//             gcvFILTER_BLIT_KERNEL_HORIZONTAL,
//             HorKernel
//             ));
//     }

//     if (VerKernel->kernelStates != N2D_NULL)
//     {
//         N2D_ON_ERROR(gcLoadFilterKernel(
//             Hardware,
//             gcvFILTER_BLIT_KERNEL_VERTICAL,
//             VerKernel
//             ));
//     }

//     N2D_ON_ERROR(gcWriteRegWithLoadState32(
//         Hardware,
//         AQDE_STRETCH_FACTOR_LOW_Address,
//         HorKernel->scaleFactor
//         ));

//     N2D_ON_ERROR(gcWriteRegWithLoadState32(
//         Hardware,
//         AQDE_STRETCH_FACTOR_HIGH_Address,
//         VerKernel->scaleFactor
//         ));

// #else
//     N2D_ON_ERROR(N2D_NOT_SUPPORTED);
// #endif
//     break;

//     default:
//         N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//     }

//     if(Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
//     {
//         gcDECResetDEC400Stream(Hardware);
//     }

//     N2D_ON_ERROR(gcCheckTilingSupport(SrcSurface, N2D_TRUE));

//     /* Set source. */
//     N2D_ON_ERROR(gcSetColorSource(
//         Hardware,
//         SrcSurface,
//         N2D_FALSE,
//         0,
//         curSrc->srcCSCMode,
//         0, /* src->srcDeGamma, */
//         N2D_TRUE
//         ));

//     /* Set DE block info*/
//     if(Hardware->hw2DBlockSize)
//     {
//         n2d_uint32_t power2BlockWidth = 3;
//         n2d_uint32_t power2BlockHeight = 3;
//         n2d_uint32_t srcBpp,dstBpp;
//         anyCompress = gcmHASCOMPRESSION(DstSurface) ? 0x100 : 0;

//         N2D_ON_ERROR(gcConvertFormat(SrcSurface->format,&srcBpp));
//         N2D_ON_ERROR(gcConvertFormat(DstSurface->format,&dstBpp));

//         /* change YUV bpp to 16 anyway. */
//         srcBpp = srcBpp == 12 ? 16 : srcBpp;
//         dstBpp = dstBpp == 12 ? 16 : dstBpp;
//         if ((Hardware->features[N2D_FEATURE_DEC400_COMPRESSION]) &&
//             (anyCompress & 0x100) &&
//             DstSurface->tiling != N2D_TILED_8X8_YMAJOR)
//         {
//             switch (DstSurface->tiling)
//             {
//                 case N2D_TILED_8X8_XMAJOR:
//                     power2BlockWidth = 4;
//                     power2BlockHeight = 3;
//                     break;
//                 case N2D_TILED_8X4:
//                 case N2D_TILED_4X8:
//                     power2BlockWidth = 3;
//                     power2BlockHeight = 3;
//                     break;
//                 case N2D_TILED:
//                     power2BlockWidth = 2;
//                     power2BlockHeight = 3;
//                     if(DstSurface->format == N2D_NV12)
//                     {
//                         power2BlockWidth = 6;
//                         power2BlockHeight = 3;
//                     } else if ((DstSurface->format == N2D_P010_MSB) || (DstSurface->format == N2D_P010_LSB))
//                     {
//                         power2BlockWidth = 5;
//                         power2BlockHeight = 3;
//                     }
//                     break;
//                 case N2D_SUPER_TILED:
//                 case N2D_SUPER_TILED_128B:
//                 case N2D_SUPER_TILED_256B:
//                 case N2D_YMAJOR_SUPER_TILED:
//                     power2BlockWidth = 6;
//                     power2BlockHeight = 6;
//                     break;
//                 case N2D_TILED_64X4:
//                     power2BlockWidth = 6;
//                     power2BlockHeight = 3;
//                     break;
//                 case N2D_TILED_32X4:
//                     power2BlockWidth = 5;
//                     power2BlockHeight = 3;
//                     break;
//                 default:
//                     N2D_ON_ERROR(N2D_NOT_SUPPORTED);
//             }
//         }

//         /* Config block size. */
//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             GCREG_DE_BLOCK_SIZE_Address,
//             gcmSETFIELD(0, GCREG_DE_BLOCK_SIZE, WIDTH_BIT_CNT, power2BlockWidth)
//             | gcmSETFIELD(0, GCREG_DE_BLOCK_SIZE, HEIGHT_BIT_CNT, power2BlockHeight)
//         ));
//     }

//     /* Set src global color for A8 source. */
//     if (curSrc->srcSurface.format == N2D_A8)
//     {
//         N2D_ON_ERROR(gcSetSourceGlobalColor(
//             Hardware,
//             curSrc->srcGlobalColor
//             ));
//     }

//     /*--------------------------------------------------------------------*/
//     memory[0]
//         = gcmSETFIELD(0, AQVR_SOURCE_IMAGE_LOW, LEFT, SrcRect->left)
//         | gcmSETFIELD(0, AQVR_SOURCE_IMAGE_LOW, TOP, SrcRect->top);
//     memory[1]
//         = gcmSETFIELD(0, AQVR_SOURCE_IMAGE_HIGH, RIGHT, SrcRect->right)
//         | gcmSETFIELD(0, AQVR_SOURCE_IMAGE_HIGH, BOTTOM, SrcRect->bottom);
//     memory[2] = SrcOrigin->x;
//     memory[3] = SrcOrigin->y;

//     N2D_ON_ERROR(gcWriteRegWithLoadState(
//         Hardware,
//         AQVR_SOURCE_IMAGE_LOW_Address,
//         4,
//         memory
//         ));

//     N2D_ON_ERROR(gcCheckTilingSupport(DstSurface, N2D_FALSE));

//     /* Set destination. */
//     N2D_ON_ERROR(gcSetTarget(
//         Hardware,
//         DstSurface,
//         N2D_TRUE,
//         State->dstCSCMode,
//         State->cscRGB2YUV,
//         N2D_NULL,   /* Gamma */
//         N2D_FALSE,  /* GdiStretch, */
//         curSrc->enableAlpha,  /* enableAlpha, */
//         memory
//         ));

//     N2D_ON_ERROR(gcSetCompression(
//         Hardware,
//         gcmHASCOMPRESSION(SrcSurface),
//         gcmHASCOMPRESSION(DstSurface)));

//     if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
//     {
//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             GCREG_DE_GENERAL_CONFIG_Address,
//             gcmSETMASKEDFIELDVALUE(GCREG_DE_GENERAL_CONFIG, PE_CACHE_MODE2, AUTO)
//             ));

//         N2D_ON_ERROR(gcWriteRegWithLoadState32(
//             Hardware,
//             GCREG_DE_GENERAL_CONFIG_Address,
//             gcmSETMASKEDFIELDVALUE(GCREG_DE_GENERAL_CONFIG, PE_SRC_CACHE_MODE2, AUTO)
//             ));
//     }

//     switch (type)
//     {
//     case gceFILTER_BLIT_TYPE_VERTICAL:
//         memory[0] |= gcmSETFIELDVALUE(0, AQDE_DEST_CONFIG, COMMAND, VER_FILTER_BLT);
//         break;
//     case gceFILTER_BLIT_TYPE_HORIZONTAL:
//         memory[0] |= gcmSETFIELDVALUE(0, AQDE_DEST_CONFIG, COMMAND, HOR_FILTER_BLT);
//         break;
//     case gceFILTER_BLIT_TYPE_ONE_PASS:
//         memory[0] |= gcmSETFIELDVALUE(0, AQDE_DEST_CONFIG, COMMAND, ONE_PASS_FILTER_BLT);
//         break;
//     }

//     memory[0] |= ((DstSurface->format == N2D_P010_LSB || DstSurface->format == N2D_I010_LSB)
//                   ? gcmSETFIELDVALUE(0, AQDE_DEST_CONFIG, P010_BITS, P010_LSB)
//                   : 0);

//     N2D_ON_ERROR(gcWriteRegWithLoadState32(
//         Hardware,
//         AQDE_DEST_CONFIG_Address,
//         memory[0]
//         ));

//     if (inCSC)
//     {
//         N2D_ON_ERROR(gcUploadCSCProgrammable(
//             Hardware,
//             N2D_TRUE,
//             State->cscYUV2RGB
//             ));
//     }

//     memory[0]
//         = gcmSETFIELD(0, AQVR_TARGET_WINDOW_LOW, LEFT, DstRect->left)
//         | gcmSETFIELD(0, AQVR_TARGET_WINDOW_LOW, TOP, DstRect->top);
//     memory[1]
//         = gcmSETFIELD(0, AQVR_TARGET_WINDOW_HIGH, RIGHT, DstRect->right)
//         | gcmSETFIELD(0, AQVR_TARGET_WINDOW_HIGH, BOTTOM, DstRect->bottom);

//     N2D_ON_ERROR(gcWriteRegWithLoadState(
//         Hardware,
//         AQVR_TARGET_WINDOW_LOW_Address,
//         2,
//         memory
//         ));

//     N2D_ON_ERROR(gcSetSuperTileVersion(
//         Hardware,
//         gcv2D_SUPER_TILE_VERSION_V3
//         ));

//     /*******************************************************************
//     ** Setup ROP.
//     */

//     N2D_ON_ERROR(gcWriteRegWithLoadState32(
//         Hardware,
//         AQDE_ROP_Address,
//           gcmSETFIELDVALUE(0, AQDE_ROP, TYPE,   ROP3)
//         | gcmSETFIELD     (0, AQDE_ROP, ROP_BG, 0xCC)
//         | gcmSETFIELD     (0, AQDE_ROP, ROP_FG, 0xCC)
//         ));

//     N2D_ON_ERROR(gcWriteRegWithLoadState32(
//         Hardware,
//         AQVR_CONFIG_Address,
//         gcmSETFIELDVALUE(0, AQVR_CONFIG, MASK_START, ENABLED) |
//         gcmSETFIELD     (0, AQVR_CONFIG,      START, blitType)
//         ));

//     N2D_ON_ERROR(gcRenderEnd(Hardware, N2D_TRUE));

//     N2D_PERF_TIME_STAMP_PRINT(__FUNCTION__, "END");

//     /* Success. */
//     return N2D_SUCCESS;

// on_error:
//     if (Hardware != N2D_NULL)
//     {
//         if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
//         {
//             /* Reset command buffer. */
//             gcResetCmdBuffer(Hardware, N2D_IS_ERROR(error));
//         }
//     }

//     return error;
// }

// n2d_error_t gcFilterBlit(
//     IN n2d_user_hardware_t * Hardware,
//     IN gcs2D_State_PTR State,
//     IN gcsSURF_INFO_PTR SrcSurface,
//     IN gcsSURF_INFO_PTR DstSurface,
//     IN gcsRECT_PTR SrcRect,
//     IN gcsRECT_PTR DestRect,
//     IN gcsRECT_PTR DestSubRect
//     )
// {
//     n2d_error_t         error;
//     gcsSURF_INFO        tempSurf;
//     n2d_buffer_t        tmpbuffer = {0};
//     n2d_buffer_format_t srcFormat, dstFormat;
//     n2d_point_t srcRectSize;
//     n2d_point_t dstRectSize;

//     n2d_bool_t horPass  = N2D_FALSE,verPass     = N2D_FALSE;
//     n2d_bool_t useOPF   = N2D_FALSE;
//     n2d_bool_t srcIsYuv = N2D_FALSE,dstIsYuv = N2D_FALSE;

//     gcsRECT ssRect, dRect, sRect, dsRect;
//     gcsFILTER_BLIT_ARRAY_PTR horKernel = N2D_NULL;
//     gcsFILTER_BLIT_ARRAY_PTR verKernel = N2D_NULL;

//     /* Determine final destination subrectangle. */
//     if (DestSubRect != N2D_NULL)
//     {
//         if (DestSubRect->left >= DestRect->right
//             || DestSubRect->right > DestRect->right
//             || DestSubRect->top >= DestRect->bottom
//             || DestSubRect->bottom > DestRect->bottom)
//         {
//             N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//         }

//         dsRect.left   = DestRect->left + DestSubRect->left;
//         dsRect.top    = DestRect->top  + DestSubRect->top;
//         dsRect.right  = DestRect->left + DestSubRect->right;
//         dsRect.bottom = DestRect->top  + DestSubRect->bottom;
//     }
//     else
//     {
//         dsRect = *DestRect;
//     }

//     dstFormat = DstSurface->format;
//     srcFormat = SrcSurface->format;

// /*----------------------------------------------------------------------------*/
// /*------------------------- Rotation optimization. ----------------------------*/

//     srcIsYuv = gcIsYUVFormat(srcFormat);
//     dstIsYuv = gcIsYUVFormat(dstFormat);

//     if(Hardware->features[N2D_FEATURE_YUV420_OUTPUT])
//     {
//         if(dstIsYuv)
//         {
//             /* Dest rect horizontal with YUV422 format must be even. */
//             if((dsRect.left & 0x1) || (dsRect.right &0x1))
//             {
//                 N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//             }

//             /* Dest rect horizontal and vertical with YUV420 format must be even. */
//             if(dstFormat != N2D_YUYV && dstFormat != N2D_UYVY &&
//                 ((dsRect.top & 0x1) || (dsRect.bottom & 0x1)))
//             {
//                 N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
//             }
//         }
//     }

//     N2D_ON_ERROR(gcCheckOPF(Hardware, SrcSurface, State, &useOPF));

//     dRect = *DestRect;
//     sRect = *SrcRect;

//     if(!Hardware->notAdjustRotation && !useOPF &&
//         State->filterState.newFilterType != gcvFILTER_USER)
//     {
//         n2d_orientation_t rot, rot2;
//         n2d_uint8_t horKernelSize, verKernelSize;

//         dRect = *DestRect;
//         sRect = *SrcRect;

//         horPass = (sRect.right - sRect.left) != (dRect.right - dRect.left);
//         verPass = (sRect.bottom - sRect.top) != (dRect.bottom - dRect.top);

//         if (State->filterState.newFilterType == gcvFILTER_BLUR)
//         {
//             horKernelSize = State->filterState.horBlurFilterKernel.kernelSize;
//             verKernelSize = State->filterState.verBlurFilterKernel.kernelSize;
//         }
//         else
//         {
//             horKernelSize = State->filterState.horSyncFilterKernel.kernelSize;
//             verKernelSize = State->filterState.verSyncFilterKernel.kernelSize;
//         }

//         rot2 = rot = DstSurface->rotation;
//         N2D_ON_ERROR(gcRectRelativeRotation(SrcSurface->rotation,&rot));
//         N2D_ON_ERROR(gcRectRelativeRotation(rot,&rot2));

//         if((horPass && verPass && (((srcIsYuv) && (srcFormat != N2D_NV21)
//             && (srcFormat != N2D_NV61)) || (Hardware->features[N2D_FEATURE_2D_ONE_PASS_FILTER] &&
//             (horKernelSize == 5 || horKernelSize == 3) && (verKernelSize == 5 || verKernelSize == 3)))) ||
//             ((rot == N2D_90 || rot == N2D_270) && ((horPass && !verPass && (rot2 == N2D_90 || rot2 == N2D_270))||
//             (!horPass && verPass && (rot2 == N2D_0 || rot2 == N2D_180)))))
//         {
//             rot2 = N2D_0;
//         }
//         else
//         {
//             rot = N2D_0;
//             rot2 = SrcSurface->rotation;
//             N2D_ON_ERROR(gcRectRelativeRotation(DstSurface->rotation,&rot2));
//         }

//         N2D_ON_ERROR(gcRotateRect(
//             &sRect,
//             SrcSurface->rotation,
//             rot2,
//             SrcSurface->alignedWidth,
//             SrcSurface->alignedHeight));

//         SrcSurface->rotation = rot2;

//         rot2 =  DstSurface->rotation;
//         N2D_ON_ERROR(gcRectRelativeRotation(rot,&rot2));

//         N2D_ON_ERROR(gcRotateRect(
//             &dRect,
//             DstSurface->rotation,
//             rot,
//             DstSurface->alignedWidth,
//             DstSurface->alignedHeight));

//         N2D_ON_ERROR(gcRotateRect(
//             &dsRect,
//             DstSurface->rotation,
//             rot,
//             DstSurface->alignedWidth,
//             DstSurface->alignedHeight));

//         DstSurface->rotation = rot;

//     }

// /*----------------------------------------------------------------------------*/
// /*------------------------- Compute rectangle sizes. -------------------------*/

//     N2D_ON_ERROR(gcGetRectWidth(&sRect, (n2d_uint32_t*)&srcRectSize.x));
//     N2D_ON_ERROR(gcGetRectheight(&sRect, (n2d_uint32_t*)&srcRectSize.y));
//     N2D_ON_ERROR(gcGetRectWidth(&dRect, (n2d_uint32_t*)&dstRectSize.x));
//     N2D_ON_ERROR(gcGetRectheight(&dRect, (n2d_uint32_t*)&dstRectSize.y));

// /*----------------------------------------------------------------------------*/
// /*--------------------------- Update kernel arrays. --------------------------*/

//     if (State->filterState.newFilterType == gcvFILTER_SYNC ||
//         State->filterState.newFilterType == gcvFILTER_BILINEAR ||
//         State->filterState.newFilterType == gcvFILTER_BICUBIC)
//     {
//         horPass = (srcRectSize.x != dstRectSize.x);
//         verPass = (srcRectSize.y != dstRectSize.y);

//         if (!verPass && !horPass)
//         {
//             if(gcmGET_PRE_ROTATION(SrcSurface->rotation) == N2D_90
//                 || gcmGET_PRE_ROTATION(SrcSurface->rotation) == N2D_270
//                 || gcmGET_PRE_ROTATION(DstSurface->rotation) == N2D_90
//                 || gcmGET_PRE_ROTATION(DstSurface->rotation) == N2D_270)
//             {
//                 verPass = N2D_TRUE;
//             }
//             else
//             {
//                 horPass = N2D_TRUE;
//             }
//         }

//         if (State->filterState.newFilterType == gcvFILTER_SYNC)
//         {
//             /* Set the proper kernel array for sync filter. */
//             horKernel = &State->filterState.horSyncFilterKernel;
//             verKernel = &State->filterState.verSyncFilterKernel;
//         }
//         else if (State->filterState.newFilterType == gcvFILTER_BILINEAR)
//         {
//             /* Set the proper kernel array for bilinear filter. */
//             horKernel = &State->filterState.horBilinearFilterKernel;
//             verKernel = &State->filterState.verBilinearFilterKernel;
//         }
//         else if (State->filterState.newFilterType == gcvFILTER_BICUBIC)
//         {
//             /* Set the proper kernel array for bicubic filter. */
//             horKernel = &State->filterState.horBicubicFilterKernel;
//             verKernel = &State->filterState.verBicubicFilterKernel;
//         }

//         /* Recompute the table if necessary. */
//         N2D_ON_ERROR(gcCalculateSyncTable(
//             State->filterState.newFilterType,
//             State->filterState.newHorKernelSize,
//             srcRectSize.x,
//             dstRectSize.x,
//             horKernel
//             ));

//         N2D_ON_ERROR(gcCalculateSyncTable(
//             State->filterState.newFilterType,
//             State->filterState.newVerKernelSize,
//             srcRectSize.y,
//             dstRectSize.y,
//             verKernel
//             ));
//     }
//     else if(State->filterState.newFilterType == gcvFILTER_BLUR)
//     {
//         /* Always do both passes for blur. */
//         horPass = verPass = N2D_TRUE;

//         /* Set the proper kernel array for blur filter. */
//         horKernel = &State->filterState.horBlurFilterKernel;
//         verKernel = &State->filterState.verBlurFilterKernel;

//         /* Recompute the table if necessary. */
//         N2D_ON_ERROR(gcCalculateBlurTable(
//             State->filterState.newHorKernelSize,
//             srcRectSize.x,
//             dstRectSize.x,
//             horKernel
//             ));

//         N2D_ON_ERROR(gcCalculateBlurTable(
//             State->filterState.newVerKernelSize,
//             srcRectSize.y,
//             dstRectSize.y,
//             verKernel
//             ));

//     }
//     else if(State->filterState.newFilterType == gcvFILTER_USER)
//     {
//         n2d_uint32_t scaleFactor;

//         /* Do the pass(es) according to user settings. */
//         horPass = State->filterState.horUserFilterPass;
//         verPass = State->filterState.verUserFilterPass;

//         /* Set the proper kernel array for user defined filter. */
//         horKernel = &State->filterState.horUserFilterKernel;
//         verKernel = &State->filterState.verUserFilterKernel;

//         /* Set the kernel size and scale factors. */
//         scaleFactor = gcGetStretchFactor(
//             State->multiSrc[State->currentSrcIndex].enableGDIStretch,
//             srcRectSize.x, dstRectSize.x);
//         horKernel->kernelSize  = State->filterState.newHorKernelSize;
//         horKernel->scaleFactor = scaleFactor;

//         scaleFactor = gcGetStretchFactor(
//             State->multiSrc[State->currentSrcIndex].enableGDIStretch,
//             srcRectSize.y, dstRectSize.y);
//         verKernel->kernelSize  = State->filterState.newVerKernelSize;
//         verKernel->scaleFactor = scaleFactor;

//     }
//     else
//     {
//         N2D_ON_ERROR(N2D_NOT_SUPPORTED);
//     }

// /*----------------------------------------------------------------------------*/
// /*------------------- Determine the source sub rectangle. --------------------*/

//     /* Compute the source sub rectangle that exactly represents
//        the destination sub rectangle. */
//     ssRect.left   = dsRect.left   - dRect.left;
//     ssRect.right  = dsRect.right  - dRect.left;
//     ssRect.top    = dsRect.top    - dRect.top;
//     ssRect.bottom = dsRect.bottom - dRect.top;

//     ssRect.left *= horKernel->scaleFactor;
//     ssRect.top *= verKernel->scaleFactor;
//     ssRect.right
//         = (ssRect.right - 1)
//         * horKernel->scaleFactor + (1 << 16);
//     ssRect.bottom
//         = (ssRect.bottom - 1)
//         * verKernel->scaleFactor + (1 << 16);

//     if (State->filterState.newFilterType == gcvFILTER_BILINEAR ||
//         State->filterState.newFilterType == gcvFILTER_BICUBIC)
//     {
//         ssRect.left   += horKernel->scaleFactor / 2 + (8 << 7);
//         ssRect.top    += verKernel->scaleFactor / 2 + (8 << 7);
//         ssRect.right  += 0x00008000;
//         ssRect.bottom += 0x00008000;
//     }
//     else if (!State->multiSrc[State->currentSrcIndex].enableGDIStretch)
//     {
//         /*  Before rendering each destination pixel, the HW will select the
//             corresponding source center pixel to apply the kernel around.
//             To make this process precise we need to add 0.5 to source initial
//             coordinates here; this will make HW pick the next source pixel if
//             the fraction is equal or greater then 0.5. */
//         ssRect.left   += 0x00008000;
//         ssRect.top    += 0x00008000;
//         ssRect.right  += 0x00008000;
//         ssRect.bottom += 0x00008000;
//     }

//     /*----------------------------------------------------------------------------*/
//     /*--------------- Process negative rectangle for filterblit. -----------------*/

//     if (dRect.left < 0 || dsRect.top < 0 ||
//         sRect.left < 0 || sRect.top < 0)
//     {
//         n2d_uint64_t fixedTmp;
//         n2d_uint32_t deltaDstX=0, deltaDstY=0, deltaSrcX=0, deltaSrcY=0, srcX, srcY, dstX, dstY, tmp=0;
//         n2d_uint32_t reverseFactor;

//         if (dsRect.left < 0)
//         {
//             deltaDstX = 0 - dsRect.left;
//         }
//         if (dsRect.top < 0)
//         {
//             deltaDstY = 0 - dsRect.top;
//         }
//         if (sRect.left < 0)
//         {
//             deltaSrcX = 0 - sRect.left;
//         }
//         if (sRect.top < 0)
//         {
//             deltaSrcY = 0 - sRect.top;
//         }

//         if (dsRect.left < 0 || sRect.left < 0)
//         {
//             srcX = deltaDstX * horKernel->scaleFactor;

//             if ((srcX >> 16) >= deltaSrcX)
//             {
//                 sRect.left += (srcX >> 16);
//                 ssRect.left += srcX & 0xFFFF;
//                 dsRect.left = 0;
//             }
//             else
//             {
//                 reverseFactor = gcGetStretchFactor(N2D_FALSE, dstRectSize.x, srcRectSize.x);
//                 dstX = deltaSrcX * reverseFactor;
//                 if (dstX & 0xFFFF)
//                 {
//                     /* ceil dst rect */
//                     tmp =  (dstX + 0x00010000) >> 16;
//                     dsRect.left += tmp;
//                     fixedTmp = (((n2d_uint64_t)tmp << 16) - dstX) * horKernel->scaleFactor;
//                     tmp = (n2d_uint32_t)((((fixedTmp >> 32) & 0xFFFF) << 16) | ((fixedTmp & 0xFFFFFFFFFFFFULL) >> 16));
//                     sRect.left = tmp >> 16;
//                 }
//                 else
//                 {
//                     sRect.left = 0;
//                 }
//                 ssRect.left += tmp & 0xFFFF;
//             }
//         }

//         if (dsRect.top < 0 || sRect.top < 0)
//         {
//             srcY = deltaDstY * verKernel->scaleFactor;

//             if ((srcY >> 16) >= deltaSrcY)
//             {
//                 sRect.top += (srcY >> 16);
//                 ssRect.top += srcY & 0xFFFF;
//                 dsRect.top = 0;
//             }
//             else
//             {
//                 reverseFactor = gcGetStretchFactor(N2D_FALSE, dstRectSize.y, srcRectSize.y);
//                 dstY = deltaSrcY * reverseFactor;
//                 if (dstY & 0xFFFF)
//                 {
//                     /* ceil dst rect */
//                     tmp =  (dstY + 0x00010000) >> 16;
//                     dsRect.top += tmp;
//                     fixedTmp = (((n2d_uint64_t)tmp << 16) - dstY) * verKernel->scaleFactor;
//                     tmp = (n2d_uint32_t)((((fixedTmp >> 32) & 0xFFFF) << 16) | ((fixedTmp & 0xFFFFFFFFFFFFULL) >> 16));
//                     sRect.top = tmp >> 16;
//                 }
//                 else
//                 {
//                     sRect.top = 0;
//                 }
//                 ssRect.top += tmp & 0xFFFF;
//             }
//         }
//     }

// /*----------------------------------------------------------------------------*/
// /*------------------------- Do the one pass blit. ----------------------------*/

//     if(Hardware->features[N2D_FEATURE_2D_ONE_PASS_FILTER] &&
//         ((useOPF || ((horKernel->kernelSize == 5 || horKernel->kernelSize == 3)
//              && (verKernel->kernelSize == 5 || verKernel->kernelSize == 3)
//              && horPass && verPass))
//              && (SrcSurface->rotation == N2D_0) && (srcFormat != N2D_A8)
//              && (!State->multiSrc[State->currentSrcIndex].enableAlpha
//              || Hardware->features[N2D_FEATURE_2D_OPF_YUV_OUTPUT]
//              || ((dstFormat != N2D_YUYV) && (dstFormat != N2D_UYVY)))))
//     {
//         /* Determine the source origin. */
//         n2d_point_t srcOrigin;

//         srcOrigin.x = (sRect.left << 16) + ssRect.left;
//         srcOrigin.y = (sRect.top  << 16) + ssRect.top;

//         /* Start the blit. */
//         N2D_ON_ERROR(gcStartVR(
//             Hardware,
//             State,
//             gceFILTER_BLIT_TYPE_ONE_PASS,
//             horKernel,
//             verKernel,
//             SrcSurface,
//             &sRect,
//             &srcOrigin,
//             DstSurface,
//             &dsRect,
//             N2D_FALSE));

//     }

// /*----------------------------------------------------------------------------*/
// /*------------------ Do the blit with the temporary buffer. ------------------*/

//     else if (horPass && verPass)
//     {
//         n2d_int32_t         horKernelHalf;
//         n2d_int32_t         leftExtra;
//         n2d_int32_t         rightExtra;
//         n2d_point_t         srcOrigin;
//         n2d_point_t         tmpRectSize;
//         n2d_buffer_format_t tmpFormat;
//         n2d_point_t         tmpAlignment;
//         n2d_uint32_t        tmpHorCoordMask;
//         n2d_uint32_t        tmpVerCoordMask;
//         n2d_point_t         tmpOrigin;
//         gcsRECT             tmpRect;
//         n2d_point_t         tmpBufRectSize;

//         /* In partial filter blit cases, the vertical pass has to render
//            more pixel information to the left and to the right of the
//            temporary image so that the horizontal pass has its necessary
//            kernel information on the edges of the image. */
//         horKernelHalf = horKernel->kernelSize >> 1;

//         leftExtra  = ssRect.left >> 16;
//         rightExtra = srcRectSize.x - (ssRect.right >> 16);

//         if (leftExtra > horKernelHalf)
//             leftExtra = horKernelHalf;

//         if (rightExtra > horKernelHalf)
//             rightExtra = horKernelHalf;

//         /* Determine the source origin. */
//         srcOrigin.x = ((sRect.left - leftExtra) << 16) + ssRect.left;
//         srcOrigin.y = (sRect.top << 16) + ssRect.top;

//         /* Determine the size of the temporary image. */
//         tmpRectSize.x
//             = leftExtra
//             + ((ssRect.right >> 16) - (ssRect.left >> 16))
//             + rightExtra;

//         tmpRectSize.y
//             = dsRect.bottom - dsRect.top;

//         /* Determine the destination origin. */
//         tmpRect.left = srcOrigin.x >> 16;
//         tmpRect.top  = 0;

//         if (srcIsYuv)
//         {
//             if ((SrcSurface->rotation == N2D_0)
//                 && (srcFormat != N2D_NV21)
//                 && (srcFormat != N2D_NV61)
//                 && (((srcFormat != N2D_NV12)
//                 && (srcFormat != N2D_NV16))))
//             {
//                 if (srcFormat == N2D_UYVY ||
//                     srcFormat == N2D_YUYV)
//                 {
//                     tmpFormat = srcFormat;
//                 }
//                 else
//                 {
//                     tmpFormat = N2D_YUYV;
//                 }

//                 tmpRectSize.x = gcmALIGN(tmpRectSize.x, 2);
//                 tmpRect.left = gcmALIGN(tmpRect.left, 2);
//             }
//             else
//             {
//                 tmpFormat = dstFormat;
//             }
//         }
//         else
//         {
//             if ((srcFormat == N2D_INDEX8) || (srcFormat == N2D_A8))
//             {
//                 tmpFormat = N2D_BGRA8888;
//             }
//             else
//             {
//                 tmpFormat = srcFormat;
//             }
//         }

//         /* Re-check rectagnle align. */
//         if (srcIsYuv && gcIsYUVFormat(tmpFormat)&&
//             ((sRect.left & 0x1) || (sRect.right & 0x1)))
//         {
//             tmpFormat = dstFormat;

//             if (!gcIsYUVFormat(tmpFormat))
//             {
//                 tmpFormat = N2D_BGRA8888;
//             }
//         }

//         N2D_ON_ERROR(gcGetPixelAlignment(
//             tmpFormat,
//             &tmpAlignment
//             ));

//         tmpHorCoordMask = tmpAlignment.x - 1;
//         tmpVerCoordMask = tmpAlignment.y - 1;

//         /* Align the temporary destination. */
//         tmpRect.left &= tmpHorCoordMask;
//         tmpRect.top  &= tmpVerCoordMask;

//         /* Determine the bottom right corner of the destination. */
//         tmpRect.right  = tmpRect.left + tmpRectSize.x;
//         tmpRect.bottom = tmpRect.top  + tmpRectSize.y;

//         /* Determine the source origin. */
//         tmpOrigin.x
//             = ((leftExtra + tmpRect.left) << 16)
//             + (ssRect.left & 0xFFFF);
//         tmpOrigin.y
//             = (tmpRect.top << 16)
//             + (ssRect.top & 0xFFFF);

//         /* Determine the size of the temporaty surface. */
//         tmpBufRectSize.x = gcmALIGN(tmpRect.right,  tmpAlignment.x);
//         tmpBufRectSize.y = gcmALIGN(tmpRect.bottom, tmpAlignment.y);

//         tmpbuffer.format = tmpFormat;
//         tmpbuffer.width  = tmpBufRectSize.x;
//         tmpbuffer.height = tmpBufRectSize.y;
//         tmpbuffer.tiling = N2D_LINEAR;

//         /* Allocate the temporary buffer. */
//         N2D_ON_ERROR(gcAllocSurfaceBuffer(N2D_NULL, &tmpbuffer));
//         /* Compute the surface placement parameters. */
//         N2D_ON_ERROR(gcAdjustSurfaceBufferParameters(Hardware, &tmpbuffer));

//         N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(&tempSurf,&tmpbuffer));

//         /*******************************************************************
//         ** Program the vertical pass.
//         */

//         N2D_ON_ERROR(gcStartVR(
//             Hardware,
//             State,
//             gceFILTER_BLIT_TYPE_VERTICAL,
//             N2D_NULL,
//             verKernel,
//             SrcSurface,
//             &sRect,
//             &srcOrigin,
//             &tempSurf,
//             &tmpRect,
//             N2D_TRUE));

//         /*******************************************************************
//         ** Program the second pass.
//         */
//         if ((DstSurface->rotation == N2D_90) || (DstSurface->rotation == N2D_270))
//         {
//             n2d_orientation_t rot;

//             rot = tempSurf.rotation;
//             N2D_ON_ERROR(gcRectRelativeRotation(DstSurface->rotation, &rot));

//             N2D_ON_ERROR(gcRotateRect(
//                 &tmpRect,
//                 tempSurf.rotation,
//                 rot,
//                 tempSurf.alignedWidth,
//                 tempSurf.alignedHeight));

//             if (rot == N2D_90)
//             {
//                 tmpOrigin.x
//                     = (tmpRect.left << 16)
//                     + (ssRect.top & 0xFFFF);

//                 tmpOrigin.y
//                     = ((rightExtra + tmpRect.top) << 16)
//                     + (ssRect.right & 0xFFFF);
//             }
//             else
//             {
//                 tmpOrigin.x
//                     = (tmpRect.left << 16)
//                     + (ssRect.bottom & 0xFFFF);

//                 tmpOrigin.y
//                     = ((leftExtra + tmpRect.top) << 16)
//                     + (ssRect.left & 0xFFFF);
//             }

//             tempSurf.rotation = rot;

//             N2D_ON_ERROR(gcRotateRect(
//                 &dsRect,
//                 DstSurface->rotation,
//                 N2D_0,
//                 DstSurface->alignedWidth,
//                 DstSurface->alignedHeight));

//             DstSurface->rotation = N2D_0;

//             N2D_ON_ERROR(gcStartVR(
//                 Hardware,
//                 State,
//                 gceFILTER_BLIT_TYPE_VERTICAL,
//                 N2D_NULL,
//                 horKernel,
//                 &tempSurf,
//                 &tmpRect,
//                 &tmpOrigin,
//                 DstSurface,
//                 &dsRect,
//                 N2D_FALSE));
//         }
//         else
//         {
//             N2D_ON_ERROR(gcStartVR(
//                 Hardware,
//                 State,
//                 gceFILTER_BLIT_TYPE_HORIZONTAL,
//                 horKernel,
//                 N2D_NULL,
//                 &tempSurf,
//                 &tmpRect,
//                 &tmpOrigin,
//                 DstSurface,
//                 &dsRect,
//                 N2D_FALSE));
//         }
//     /*To prevent tempSurf from being released,commit is required here*/
//     N2D_ON_ERROR(n2d_commit());
//     }

// /*----------------------------------------------------------------------------*/
// /*---------------------------- One pass only blit. -------------------------*/

//     else if (horPass || verPass)
//     {
//         /* Determine the source origin. */
//         n2d_point_t srcOrigin;

//         srcOrigin.x = (sRect.left << 16) + ssRect.left;
//         srcOrigin.y = (sRect.top  << 16) + ssRect.top;

//         /* Start the blit. */
//         N2D_ON_ERROR(gcStartVR(
//             Hardware,
//             State,
//             horPass ? gceFILTER_BLIT_TYPE_HORIZONTAL : gceFILTER_BLIT_TYPE_VERTICAL,
//             horKernel,
//             verKernel,
//             SrcSurface,
//             &sRect,
//             &srcOrigin,
//             DstSurface,
//             &dsRect,
//             N2D_FALSE
//             ));
//     }
//     else
//     {

//         N2D_ON_ERROR(N2D_NOT_SUPPORTED);
//     }

// on_error:

//     if(N2D_INVALID_HANDLE != tmpbuffer.handle)
//     {
//         error = n2d_free(&tmpbuffer);
//     }

//     return error;
// }

static n2d_error_t gcSetGPUState(
    IN n2d_user_hardware_t * Hardware,
    IN gcs2D_State_PTR State,
    IN gce2D_COMMAND Command,
    IN n2d_bool_t  MultiDstRect
    )
{
    n2d_error_t error;
    n2d_buffer_format_t dstFormat;
    n2d_uint32_t command, destConfig;
    n2d_bool_t useSource, useDest, usePattern;
    gcs2D_MULTI_SOURCE_PTR src;
    n2d_uint8_t fgRop = 0, bgRop = 0;
    n2d_bool_t flushCache = N2D_TRUE, setPattern = N2D_TRUE, uploadPaletteTable = N2D_TRUE;
    n2d_uint32_t srcMask = 0, srcCurrent = 0, i;
    n2d_bool_t anyRot = N2D_FALSE,anyStretch = N2D_FALSE;
    n2d_bool_t anySrcTiled = N2D_FALSE,anyDstTiled = N2D_FALSE;
    n2d_uint32_t dstBpp;
    n2d_bool_t anyCompress = N2D_FALSE, anyAlphaBlending = N2D_FALSE;
    n2d_uint32_t data = 0;

    n2d_bool_t  uploadSrcCSC = N2D_FALSE;

    Hardware->enableXRGB = State->enableXRGB;

    /* Convert the command. */
    N2D_ON_ERROR(gcTranslateCommand(
        Command, &command
        ));

    dstFormat = State->dest.dstSurface.format;

    N2D_ON_ERROR(gcRenderBegin(Hardware));

    if(Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
    {
        gcDECResetDEC400Stream(Hardware);
    }

    /* Compute bits per pixel. */
    N2D_ON_ERROR(gcConvertFormat(dstFormat, &dstBpp));

    /* check alphablending enabled */
    if (Command == gcv2D_MULTI_SOURCE_BLT)
    {
        for(i = 0; i < gcdMULTI_SOURCE_NUM; i++)
        {
            if (!(State->srcMask& (1 << i)))
            {
                continue;
            }
            src = State->multiSrc + i;
            anyAlphaBlending |= src->enableAlpha;

            /*Reset source configex.*/
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_SRC_CONFIG_EX_Address + 0x4 * i,
                GCREG_DE_SRC_CONFIG_EX_ResetValue
                ));

        }
    } else
    {
        src = State->multiSrc + State->currentSrcIndex;
        anyAlphaBlending = src->enableAlpha;

        /*Reset source configex.*/
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GCREG_DE_SRC_CONFIG_EX_Address,
            GCREG_DE_SRC_CONFIG_EX_ResetValue
            ));

    }

    N2D_ON_ERROR(gcCheckTilingSupport(&State->dest.dstSurface, N2D_FALSE));

    /* Set target surface */
    N2D_ON_ERROR(gcSetTarget(
        Hardware,
        &State->dest.dstSurface,
        N2D_FALSE,
        State->dstCSCMode,
        State->cscRGB2YUV,
        N2D_NULL,   /* Gamma */
        N2D_FALSE,   /* State->multiSrc.enableGDIStretch, */
        anyAlphaBlending,
        &destConfig
        ));

    anyDstTiled = State->dest.dstSurface.tiling != N2D_LINEAR;

    anyRot = (gcmGET_PRE_ROTATION(State->dest.dstSurface.rotation) == N2D_270)
        || (gcmGET_PRE_ROTATION(State->dest.dstSurface.rotation) == N2D_90);

    anyCompress = gcmHASCOMPRESSION(&State->dest.dstSurface) ? 0x100 : 0;

    destConfig |= gcmSETFIELD(0, AQDE_DEST_CONFIG, COMMAND, command);

    if (Hardware->hw2DBlockSize || Hardware->hw2DQuad)
    {
        if (Hardware->hw2DBlockSize)
        {
            /*080104C9*/
            N2D_ON_ERROR(gcWriteRegWithLoadState32(
                Hardware,
                GCREG_DE_BLOCK_SIZE_Address,
                  gcmSETFIELD(0, GCREG_DE_BLOCK_SIZE, WIDTH_BIT_CNT, 3)
                | gcmSETFIELD(0, GCREG_DE_BLOCK_SIZE, HEIGHT_BIT_CNT, 3)
                ));
        }

        if (Hardware->hw2DQuad)
        {
            destConfig = gcmSETFIELDVALUE(
                                destConfig,
                                AQDE_DEST_CONFIG,
                                ALL_QUAD,
                                DISABLED);

            destConfig = gcmSETFIELDVALUE(
                                destConfig,
                                AQDE_DEST_CONFIG,
                                STRETCH_QUAD,
                                DISABLED);
        }
    }

    /* Set target color key range. */
    N2D_ON_ERROR(gcSetTargetColorKeyRange(
        Hardware,
        State->dstColorKeyLow,
        State->dstColorKeyHigh
        ));

    if (Command == gcv2D_CLEAR)
    {
        /* Set clear color. */
        N2D_ON_ERROR(gcSetClearColor(
            Hardware,
            State->clearColor,
            dstFormat
            ));

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQDE_ROT_ANGLE_Address,
            gcmSETFIELD(gcmSETFIELDVALUE(~0, AQDE_ROT_ANGLE, MASK_SRC, ENABLED),
                AQDE_ROT_ANGLE, SRC, AQDE_ROT_ANGLE_SRC_ROT0)
            ));
    }

    N2D_ON_ERROR(gcSetClipping(
        Hardware,
        &State->dstClipRect));

    /* Set 2D dithering. */
    N2D_ON_ERROR(gcSetDither(
        Hardware,
        Hardware->state.dest.ditherInfo.enable
        ));

    N2D_ON_ERROR(gcSetSuperTileVersion(
        Hardware,
        gcv2D_SUPER_TILE_VERSION_V3
        ));

    if (Command == gcv2D_MULTI_SOURCE_BLT)
    {
        srcMask = State->srcMask;
    }
    else
    {
        srcMask = 1 << State->currentSrcIndex;
    }

    for (i = 0; i < gcdMULTI_SOURCE_NUM; i++)
    {
        if (!(srcMask & (1 << i)))
        {
            continue;
        }

        src = &State->multiSrc[State->currentSrcIndex];

        fgRop = src->fgRop;
        bgRop = src->bgRop;

        /* Determine the resource usage. */
        gcGetResourceUsage(
            fgRop, bgRop,
            src->srcTransparency,
            &useSource, &usePattern, &useDest
            );

        N2D_ON_ERROR(gcCheckTilingSupport(&src->srcSurface, N2D_TRUE));

        if (flushCache)
        {
            /*******************************************************************
            ** Chips with byte write capability don't fetch the destination
            ** if it's not needed for the current operation. If the primitive(s)
            ** that follow overlap with the previous primitive causing a cache
            ** hit and they happen to use the destination to ROP with, it will
            ** cause corruption since the destination was not fetched in the
            ** first place.
            **
            ** Currently the hardware does not track this kind of case so we
            ** have to flush whenever we see a use of source or destination.
            */

            /* Flush 2D cache if needed. */
            if (1 /*Hardware->byteWrite*/ && (useSource || useDest))
            {
                /* Flush the current pipe. */
                N2D_ON_ERROR(gcWriteRegWithLoadState32(
                                              Hardware,
                                              AQ_FLUSH_Address,
                                              gcmSETFIELDVALUE(0,
                                                               AQ_FLUSH,
                                                               PE2D_CACHE,
                                                               ENABLE)));

                flushCache = N2D_FALSE;
            }
        }

        if (Command == gcv2D_STRETCH)
        {
            /* Set stretch factors. */
            N2D_ON_ERROR(gcSetStretchFactors(
                Hardware,
                src->horFactor,
                src->verFactor
                ));
        }

        if (usePattern && setPattern)
        {
            setPattern = N2D_FALSE;

            if (!useSource)
            {
                N2D_ON_ERROR(gcWriteRegWithLoadState32(
                    Hardware,
                    AQPE_CONTROL_Address,
                    gcmSETMASKEDFIELDVALUE(AQPE_CONTROL, YUVRGB, ENABLED)
                    ));
            }

            switch (State->brushType)
            {
            case gcv2D_PATTERN_SOLID:
                N2D_ON_ERROR(gcLoadSolidColorPattern(
                    Hardware,
                    State->brushColorConvert,
                    State->brushFgColor,
                    State->brushMask,
                    dstFormat
                    ));
                break;

            case gcv2D_PATTERN_COLOR:
                N2D_ON_ERROR(gcLoadColorPattern(
                    Hardware,
                    State->brushOriginX,
                    State->brushOriginY,
                    State->brushAddress,
                    State->brushFormat,
                    State->brushMask
                    ));
                break;

            case gcv2D_PATTERN_MONO:
            default:
                N2D_ON_ERROR(N2D_NOT_SUPPORTED);
            }
        }

        if (!uploadSrcCSC)
        {
            uploadSrcCSC = gcNeedUserCSC(src->srcCSCMode, src->srcSurface.format);
        }


        /* the old src registers. */
        if (Command != gcv2D_MULTI_SOURCE_BLT)
        {
            /* Set transparency mode */
            N2D_ON_ERROR(gcSetTransparencyModes(
                Hardware,
                src->srcTransparency,
                src->dstTransparency,
                src->patTransparency,
                fgRop,
                bgRop,
                src->enableDFBColorKeyMode
                ));

                /* Set YUV2RGB */
                if (Hardware->features[N2D_FEATURE_2D_10BIT_OUTPUT_LINEAR])
                {
                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                        Hardware,
                        AQPE_CONTROL_Address,
                        gcmSETMASKEDFIELDVALUE(AQPE_CONTROL, YUVRGB, ENABLED)
                    ));
                }
                else
                {
                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                        Hardware,
                        AQPE_CONTROL_Address,
                        gcmSETMASKEDFIELDVALUE(AQPE_CONTROL, YUVRGB, DISABLED)
                    ));
                }

            /* Set target global color. */
            N2D_ON_ERROR(gcSetTargetGlobalColor(
                Hardware,
                src->dstGlobalColor
                ));

            if (useSource)
            {
                if (Command == gcv2D_CLEAR)
                {
                    data = gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, LOCATION, MEMORY) |(
                        ((State->multiSrc[State->currentSrcIndex].srcSurface.format) == N2D_P010_LSB ||
                         (State->multiSrc[State->currentSrcIndex].srcSurface.format) == N2D_I010_LSB) ?
                         gcmSETFIELDVALUE(0, AQDE_SRC_CONFIG, P010_BITS, P010_LSB) : 0);

                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                        Hardware,
                        AQDE_SRC_CONFIG_Address,
                        data
                        ));
                }
                else
                {
                    n2d_uint32_t transparency = AQDE_SRC_CONFIG_TRANSPARENCY_OPAQUE;

                    if (src->srcSurface.format == N2D_INDEX8)
                    {
                        N2D_ON_ERROR(gcLoadPalette(
                            Hardware,
                            State->paletteFirstIndex,
                            State->paletteIndexCount,
                            State->paletteTable,
                            State->paletteConvert,
                            dstFormat,
                            &State->paletteProgram,
                            &State->paletteConvertFormat
                            ));
                    }

                    /* Set src global color for A8 source. */
                    if (src->srcSurface.format == N2D_A8)
                    {
                        N2D_ON_ERROR(gcSetSourceGlobalColor(
                            Hardware,
                            src->srcGlobalColor
                            ));
                    }

                    /* Use the src rect for the parameter setting stage */
                    N2D_ON_ERROR(gcSetSource(
                        Hardware,
                        &src->srcRect
                        ));

                    if (!anyCompress)
                    {
                        anyCompress = (src->srcSurface.tile_status_config == N2D_TSC_DEC_COMPRESSED);

                    }

                    if (!anyRot)
                    {
                        anyRot =
                            (gcmGET_PRE_ROTATION(src->srcSurface.rotation) == N2D_270)
                             || (gcmGET_PRE_ROTATION(src->srcSurface.rotation) == N2D_90);
                    }

                    anySrcTiled = (src->srcSurface.tiling != N2D_LINEAR);

                    switch (src->srcType)
                    {
                    case N2D_SOURCE_MASKED_MONO:
                        N2D_ON_ERROR(gcSetMonochromeSource(
                            Hardware,
                            (n2d_uint8_t)src->srcMonoTransparencyColor,
                            src->srcMonoPack,
                            src->srcRelativeCoord,
                            src->srcFgColor,
                            src->srcBgColor,
                            src->srcColorConvert,
                            src->srcSurface.format,
                            dstFormat,
                            src->srcStream,
                            transparency
                            ));
                        break;

                    case N2D_SOURCE_DEFAULT:
                        N2D_ON_ERROR(gcSetColorSource(
                            Hardware,
                            &src->srcSurface,
                            src->srcRelativeCoord,
                            transparency,
                            src->srcCSCMode,
                            0, /* src->srcDeGamma, */
                            N2D_FALSE
                            ));

                            if ((Command == gcv2D_BLT)
                                || (Command == gcv2D_STRETCH))
                            {
                                if (Hardware->hw2DBlockSize )
                                {
                                    n2d_uint32_t power2BlockWidth = 3;
                                    n2d_uint32_t power2BlockHeight = 3;
                                    n2d_uint32_t srcBpp;

                                    N2D_ON_ERROR(gcConvertFormat(src->srcSurface.format, &srcBpp));

                                    /* change YUV bpp to 16 anyway. */
                                    srcBpp = srcBpp == 12 ? 16 : srcBpp;
                                    dstBpp = dstBpp == 12 ? 16 : dstBpp;

                                    if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION] &&
                                             (anyCompress & 0x100) &&
                                             State->dest.dstSurface.tiling != N2D_TILED_8X8_YMAJOR)
                                    {
                                        switch (State->dest.dstSurface.tiling)
                                        {
                                            case N2D_TILED_8X8_XMAJOR:
                                                power2BlockWidth = 4;
                                                power2BlockHeight = 3;
                                                break;
                                            case N2D_TILED_8X4:
                                            case N2D_TILED_4X8:
                                                power2BlockWidth = 3;
                                                power2BlockHeight = 3;
                                                break;
                                            case N2D_TILED:
                                                power2BlockWidth = 2;
                                                power2BlockHeight = 3;
                                                if(State->dest.dstSurface.format == N2D_NV12)
                                                {
                                                    power2BlockWidth = 6;
                                                    power2BlockHeight = 3;
                                                } else if (State->dest.dstSurface.format == N2D_P010_MSB ||
                                                           State->dest.dstSurface.format == N2D_P010_LSB)
                                                {
                                                    power2BlockWidth = 5;
                                                    power2BlockHeight = 3;
                                                }
                                                break;
                                            case N2D_SUPER_TILED:
                                            case N2D_SUPER_TILED_128B:
                                            case N2D_SUPER_TILED_256B:
                                                power2BlockWidth = 6;
                                                power2BlockHeight = 6;
                                                break;
                                            case N2D_YMAJOR_SUPER_TILED:
                                            case N2D_LINEAR:
                                                power2BlockWidth = 3;
                                                power2BlockHeight = 3;
                                                break;
                                            case N2D_TILED_64X4:
                                                power2BlockWidth = 6;
                                                power2BlockHeight = 3;
                                                break;
                                            case N2D_TILED_32X4:
                                                power2BlockWidth = 5;
                                                power2BlockHeight = 3;
                                                break;
                                            default:
                                                N2D_ON_ERROR(N2D_NOT_SUPPORTED);
                                        }
                                    }
                                    else
                                    {
                                        if (anyRot)
                                        {
                                            if ((srcBpp == 16) && (dstBpp == 16))
                                            {
                                                power2BlockWidth = 5;
                                                power2BlockHeight = 5;
                                            }
                                            else if ((srcBpp == 16) && (dstBpp == 32))
                                            {
                                                power2BlockWidth = 5;
                                                power2BlockHeight = 4;
                                            }
                                            else if ((srcBpp == 32) && (dstBpp == 32))
                                            {
                                                power2BlockWidth = 4;
                                                power2BlockHeight = 4;
                                            }
                                            else if ((srcBpp == 32) && (dstBpp == 16))
                                            {
                                                power2BlockWidth = 4;
                                                power2BlockHeight = 5;
                                            }
                                        }
                                        else if (anySrcTiled)
                                        {
                                            power2BlockWidth = 3;
                                            power2BlockHeight = 3;
                                        }
                                        else
                                        {
                                            power2BlockWidth = 7;
                                            power2BlockHeight = 3;
                                        }
                                    }

                                    /* Config block size. */
                                    N2D_ON_ERROR(gcWriteRegWithLoadState32(
                                        Hardware,
                                        GCREG_DE_BLOCK_SIZE_Address,
                                          gcmSETFIELD(0, GCREG_DE_BLOCK_SIZE, WIDTH_BIT_CNT, power2BlockWidth)
                                        | gcmSETFIELD(0, GCREG_DE_BLOCK_SIZE, HEIGHT_BIT_CNT, power2BlockHeight)
                                        ));
                                }

                                if (Hardware->hw2DQuad && anyRot)
                                {
                                    /* Config quad. */
                                    if (Command == gcv2D_STRETCH)
                                    {
                                        destConfig = gcmSETFIELDVALUE(
                                                        destConfig,
                                                        AQDE_DEST_CONFIG,
                                                        STRETCH_QUAD,
                                                        ENABLED)
                                                    | gcmSETFIELDVALUE(
                                                        destConfig,
                                                        AQDE_DEST_CONFIG,
                                                        ALL_QUAD,
                                                        ENABLED);
                                    }
                                    else
                                    {
                                        if (dstFormat != N2D_YUYV &&
                                            dstFormat != N2D_UYVY)
                                        {
                                            destConfig = gcmSETFIELDVALUE(
                                                            destConfig,
                                                            AQDE_DEST_CONFIG,
                                                            ALL_QUAD,
                                                            ENABLED)
                                                        | gcmSETFIELDVALUE(
                                                            destConfig,
                                                            AQDE_DEST_CONFIG,
                                                            STRETCH_QUAD,
                                                            DISABLED);
                                        }
                                        else
                                        {
                                            destConfig = gcmSETFIELDVALUE(
                                                            destConfig,
                                                            AQDE_DEST_CONFIG,
                                                            ALL_QUAD,
                                                            DISABLED)
                                                        | gcmSETFIELDVALUE(
                                                            destConfig,
                                                            AQDE_DEST_CONFIG,
                                                            STRETCH_QUAD,
                                                            DISABLED);
                                        }
                                    }
                                }
                            }

                        /* Set src color key range */
                        if (src->srcTransparency == N2D_KEYED)
                        {
                            N2D_ON_ERROR(gcSetSourceColorKeyRange(
                                Hardware,
                                src->srcColorKeyLow,
                                src->srcColorKeyHigh,
                                src->srcSurface.format
                                ));
                        }
                        break;

                    case N2D_SOURCE_MASKED:
                         N2D_ON_ERROR(gcSetMaskedSource(
                            Hardware,
                            &src->srcSurface,
                            src->srcRelativeCoord,
                            src->srcMonoPack,
                            transparency
                            ));
                         break;
                    default:
                        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
                    }
                }
            }

            if (src->enableAlpha)
            {
                /* Set alphablend states */
                N2D_ON_ERROR(gcEnableAlphaBlend(
                    Hardware,
                    src->srcAlphaMode,
                    src->dstAlphaMode,
                    src->srcGlobalAlphaMode,
                    src->dstGlobalAlphaMode,
                    src->srcFactorMode,
                    src->dstFactorMode,
                    src->srcColorMode,
                    src->dstColorMode,
                    src->srcGlobalColor,
                    src->dstGlobalColor
                    ));
            }
            else
            {
                N2D_ON_ERROR(gcDisableAlphaBlend(Hardware));
            }

            /* Set multiply modes. */
            N2D_ON_ERROR(gcSetMultiplyModes(
                Hardware,
                src->srcPremultiplyMode,
                src->dstPremultiplyMode,
                src->srcPremultiplyGlobalMode,
                src->dstDemultiplyMode,
                src->srcGlobalColor
                ));

            /* Set mirror state. */
            N2D_ON_ERROR(gcSetBitBlitMirror(
                Hardware,
                State->multiSrc[State->currentSrcIndex].horMirror,
                State->multiSrc[State->currentSrcIndex].verMirror,
                !anyCompress
                ));
        }
        /* the multi src registers. */
        else
        {
            if (useSource)
            {
                if (gcmHASCOMPRESSION(&src->srcSurface))
                {
                    anyCompress |= 1 << i;
                }

                if (!anyRot)
                {
                    anyRot =
                        (gcmGET_PRE_ROTATION(src->srcSurface.rotation) == N2D_270)
                         || (gcmGET_PRE_ROTATION(src->srcSurface.rotation) == N2D_90);
                }
            }

            /* Load the palette for Index8 source.
               It is shared by all the multi source. */
            if (uploadPaletteTable
                && (N2D_INDEX8 == src->srcSurface.format))
            {
                N2D_ON_ERROR(gcLoadPalette(
                    Hardware,
                    State->paletteFirstIndex,
                    State->paletteIndexCount,
                    State->paletteTable,
                    State->paletteConvert,
                    dstFormat,
                    &State->paletteProgram,
                    &State->paletteConvertFormat
                    ));

                uploadPaletteTable = N2D_FALSE;
            }

            if (Hardware->features[N2D_FEATURE_2D_MULTI_SOURCE_BLT])
            {
                /* 8x source setting. */
                N2D_ON_ERROR(gcSetMultiSource(
                    Hardware,
                    srcCurrent,
                    i,
                    State,
                    MultiDstRect));

                if ((src->srcRect.right  - src->srcRect.left != src->dstRect.right  - src->dstRect.left ||
                     src->srcRect.bottom  - src->srcRect.top != src->dstRect.bottom  - src->dstRect.top))
                {
                    anyStretch = N2D_TRUE;
                }
            }

            anySrcTiled = src->srcSurface.tiling != N2D_LINEAR;
        }
        srcCurrent++;
    }

    if (Command == gcv2D_MULTI_SOURCE_BLT)
    {
        n2d_uint32_t horBlk = 0, verBlk = 0;
        n2d_uint32_t config = ~0U;

        if ((Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])&&
                 (anyCompress & 0x100) &&
                 State->dest.dstSurface.tiling != N2D_TILED_8X8_YMAJOR)
        {
            switch (State->dest.dstSurface.tiling)
            {
                case N2D_TILED_8X8_XMAJOR:
                    horBlk = GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL16;
                    verBlk = GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE8;
                    break;
                case N2D_TILED_8X4:
                case N2D_TILED_4X8:
                    horBlk = GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL8;
                    verBlk = GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE8;
                    break;
                case N2D_SUPER_TILED:
                case N2D_SUPER_TILED_128B:
                case N2D_SUPER_TILED_256B:
                    horBlk = GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL64;
                    verBlk = GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE64;
                    break;
                case N2D_TILED_64X4:
                    horBlk = GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL64;
                    verBlk = GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE8;
                    break;
                case N2D_TILED_32X4:
                    horBlk = GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL32;
                    verBlk = GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE8;
                    break;
                default:
                    N2D_ON_ERROR(N2D_NOT_SUPPORTED);
            }
        }
        else
        {
            if (anySrcTiled || anyDstTiled)
            {
                horBlk = GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL16;
                verBlk = GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE16;
            }
            else if (anyRot)
            {

                horBlk = GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL16;
                verBlk = GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE16;

            }
            else
            {

                horBlk = GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL128;
                verBlk = GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE1;
            }
        }

        config = gcmSETFIELD(
                    GCREG_DE_MULTI_SOURCE_ResetValue,
                    GCREG_DE_MULTI_SOURCE,
                    MAX_SOURCE,
                    srcCurrent - 1)
                 | gcmSETFIELD(
                    GCREG_DE_MULTI_SOURCE_ResetValue,
                    GCREG_DE_MULTI_SOURCE,
                    HORIZONTAL_BLOCK,
                    horBlk)
                | gcmSETFIELD(
                    GCREG_DE_MULTI_SOURCE_ResetValue,
                    GCREG_DE_MULTI_SOURCE,
                    VERTICAL_BLOCK,
                    verBlk);

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GCREG_DE_MULTI_SOURCE_Address,
            config
            ));

        if (Hardware->hw2DQuad)
        {
            if (anySrcTiled || anyDstTiled)
            {
                destConfig = gcmSETFIELDVALUE(
                                destConfig,
                                AQDE_DEST_CONFIG,
                                ALL_QUAD,
                                DISABLED);
            }
            else if (anyRot)
            {
                if (anyStretch)
                {
                    destConfig = gcmSETFIELDVALUE(
                                    destConfig,
                                    AQDE_DEST_CONFIG,
                                    ALL_QUAD,
                                    DISABLED)
                                | gcmSETFIELDVALUE(
                                    destConfig,
                                    AQDE_DEST_CONFIG,
                                    STRETCH_QUAD,
                                    ENABLED);
                }
                else
                {
                    destConfig = gcmSETFIELDVALUE(
                                    destConfig,
                                    AQDE_DEST_CONFIG,
                                    ALL_QUAD,
                                    ENABLED)
                                | gcmSETFIELDVALUE(
                                    destConfig,
                                    AQDE_DEST_CONFIG,
                                    STRETCH_QUAD,
                                    DISABLED);
                }
            }
        }
    }

    /* Always enable new walker.*/
   destConfig = gcmSETFIELDVALUE(
                    destConfig,
                    AQDE_DEST_CONFIG,
                    WALKER_V2,
                    ENABLED);

    N2D_ON_ERROR(gcSetCompression(
        Hardware,
        anyCompress & 0xFF,
        gcmHASCOMPRESSION(&State->dest.dstSurface)));

    if (Hardware->features[N2D_FEATURE_DEC400_COMPRESSION])
    {
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GCREG_DE_GENERAL_CONFIG_Address,
            gcmSETMASKEDFIELDVALUE(GCREG_DE_GENERAL_CONFIG, PE_CACHE_MODE2, AUTO)
            ));

        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GCREG_DE_GENERAL_CONFIG_Address,
            gcmSETMASKEDFIELDVALUE(GCREG_DE_GENERAL_CONFIG, PE_SRC_CACHE_MODE2, AUTO)
            ));
    }

    destConfig |= ((State->dest.dstSurface.format == N2D_P010_LSB ||
                    State->dest.dstSurface.format == N2D_I010_LSB) ?
                     gcmSETFIELDVALUE(0, AQDE_DEST_CONFIG, P010_BITS, P010_LSB) : 0);

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_DEST_CONFIG_Address,
        destConfig
        ));

    /*config csc*/
    if(uploadSrcCSC || gcNeedUserCSC(State->dstCSCMode, State->dest.dstSurface.format))
    {
        N2D_ON_ERROR(gcUploadCSCProgrammable(
            Hardware,
            N2D_TRUE,
            State->cscYUV2RGB
            ));
    }

    /*******************************************************************
    ** Setup ROP.
    */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        AQDE_ROP_Address,
          gcmSETFIELDVALUE(0, AQDE_ROP, TYPE,   ROP4)
        | gcmSETFIELD     (0, AQDE_ROP, ROP_BG, bgRop)
        | gcmSETFIELD     (0, AQDE_ROP, ROP_FG, fgRop)
        ));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    /* Return the error. */
    return error;
}

n2d_error_t gcStartDE(
    IN n2d_user_hardware_t *Hardware,
    IN gcs2D_State_PTR State,
    IN gce2D_COMMAND Command,
    IN n2d_uint32_t SrcRectCount,
    IN gcsRECT_PTR SrcRect,
    IN n2d_uint32_t DestRectCount,
    IN gcsRECT_PTR DestRect
    )
{
    n2d_error_t error = N2D_SUCCESS;
    SPLIT_RECT_MODE smode = SPLIT_RECT_MODE_NONE;

    N2D_PERF_TIME_STAMP_PRINT(__FUNCTION__, "START");

    if (DestRect != N2D_NULL)
    {
        if ((DestRectCount < 1) || ((Command != gcv2D_CLEAR)
            && (Command != gcv2D_BLT) && (Command != gcv2D_STRETCH)
            && (Command != gcv2D_MULTI_SOURCE_BLT))
            || ((!SrcRect || ((SrcRectCount != DestRectCount)))
                && (SrcRect || SrcRectCount != 0)))
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }
    }
    else if (Hardware->features[N2D_FEATURE_2D_MULTI_SOURCE_BLT])
    {
        if (DestRectCount != 0)
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }
    }
    else
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    if(Hardware->features[N2D_FEATURE_DEC400_COMPRESSION] &&
        ((State->dest.dstSurface.format == N2D_NV12) || (State->dest.dstSurface.format == N2D_NV21) ) &&
        State->dest.dstSurface.tiling == N2D_TILED_8X8_XMAJOR)
    {
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    do
    {
        n2d_uint32_t idx;

        /* Validate rectangle coordinates. */
        if (SrcRect)
        {
            for (idx = 0; idx < SrcRectCount; idx++)
            {
                if (!gcsValidRect(SrcRect + idx))
                {
                    N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
                }
            }
        }

        for (idx = 0; idx < DestRectCount; idx++)
        {
            if (!gcsValidRect(DestRect + idx))
            {
                N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
            }
        }

        if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
        {
            n2d_uint32_t    loopCount = 0;
            n2d_uint32_t    *memory = N2D_NULL;
            n2d_user_cmd_buf_t        *cmd_buf = N2D_NULL;

            /*current cmdBuf logical base startmemory and offset of last use*/
            Hardware->cmdCount   = gcCalCmdbufSize(Hardware,State,Command);
            Hardware->cmdIndex   = 0;

            N2D_ON_ERROR(gcReserveCmdBuf(Hardware,Hardware->cmdCount * N2D_SIZEOF(n2d_uint32_t),&cmd_buf));

            Hardware->cmdLogical = (n2d_pointer)(cmd_buf->last_reserve);

            N2D_ON_ERROR(gcSetGPUState(Hardware, State, Command, DestRect == N2D_NULL));

            /*******************************************************************
            ** Allocate and configure StartDE command buffer.
            */

            if (SrcRect == N2D_NULL)
            {
                memory = (n2d_uint32_t *)Hardware->cmdLogical + Hardware->cmdIndex;

                do
                {
                    /* Draw a dest rectangle. */
                    memory += gcDrawRectangle(memory, N2D_NULL, DestRect);

                    if (DestRect)
                    {
                        --DestRectCount;
                        ++DestRect;
                    }

                    loopCount++;
                }
                while (DestRectCount && loopCount < MAX_LOOP_COUNT);

                Hardware->cmdIndex = (n2d_uint32_t)(memory - (n2d_uint32_t *)Hardware->cmdLogical);
            }
            else
            {
                memory = (n2d_uint32_t *)Hardware->cmdLogical + Hardware->cmdIndex;

                do
                {

                    memory += gcDrawRectangle(memory, SrcRect, DestRect);

                    /* Move to next SrcRect and DestRect. */
                    SrcRect++;
                    DestRect++;
                    loopCount++;
                }
                while (--DestRectCount && loopCount < MAX_LOOP_COUNT);

                Hardware->cmdIndex = (n2d_uint32_t)(memory - (n2d_uint32_t *)Hardware->cmdLogical);
            }

            N2D_ON_ERROR(gcRenderEnd(Hardware, N2D_TRUE));
        }
    }
    while (N2D_FALSE);

    N2D_PERF_TIME_STAMP_PRINT(__FUNCTION__, "END");

on_error:

    if (smode != SPLIT_RECT_MODE_NONE && Hardware!= N2D_NULL)
    {
        n2d_int32_t *p = 0;
        *p = 0;
    }

    if (Hardware != N2D_NULL)
    {
        if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
        {
            /* Reset command buffer. */
            gcResetCmdBuffer(Hardware, N2D_IS_ERROR(error));
        }
    }

    /* Return result. */
    return error;
}

n2d_error_t gcStartDELine(
    IN n2d_user_hardware_t * Hardware,
    IN gcs2D_State_PTR State,
    IN gce2D_COMMAND Command,
    IN n2d_uint32_t LineCount,
    IN gcsRECT_PTR DestRect,
    IN n2d_uint32_t ColorCount,
    IN n2d_uint32_t * Color32
    )
{
    n2d_error_t error = N2D_SUCCESS;

    do
    {
        n2d_uint32_t idx;

        /* Validate line coordinates. */
        for (idx = 0; idx < LineCount; idx++)
        {
            if ((DestRect[idx].left < 0)
                || (DestRect[idx].top < 0)
                || (DestRect[idx].right < 0)
                || (DestRect[idx].bottom < 0)
                || (DestRect[idx].left > N2D_MAX_WIDTH)
                || (DestRect[idx].top > N2D_MAX_HEIGHT)
                || (DestRect[idx].right > N2D_MAX_WIDTH)
                || (DestRect[idx].bottom > N2D_MAX_HEIGHT))
            {
                N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
            }
        }

        if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
        {
            n2d_uint32_t i, maxLineCount, leftLineCount;
            n2d_uint32_t colorConfig[2], lastProgrammedColor = 0;
            n2d_uint32_t * memory;
            gcsRECT_PTR currentDestRect;
            n2d_uint32_t loopCount = 0;
            n2d_user_cmd_buf_t        *cmd_buf = N2D_NULL;

            Hardware->cmdCount   = gcCalCmdbufSize(Hardware,State,Command);
            Hardware->cmdIndex   = 0;

            N2D_ON_ERROR(gcReserveCmdBuf(Hardware,Hardware->cmdCount * N2D_SIZEOF(n2d_uint32_t),&cmd_buf));

            Hardware->cmdLogical = (n2d_pointer)(cmd_buf->last_reserve);
            N2D_ON_ERROR(gcSetGPUState(Hardware, State, Command, N2D_FALSE));

            /*******************************************************************
            ** Allocate and configure StartDE command buffer.  Subdivide into
            ** multiple commands if leftLineCount exceeds maxLineCount.
            */

            maxLineCount = gcGetMaximumRectCount();
            leftLineCount = LineCount;
            currentDestRect = DestRect;

            if (ColorCount)
            {
                /* Set last programmed color different from *Color32,
                   so that the first color always gets programmed. */
                lastProgrammedColor = *Color32 + 1;
            }

            do
            {
                /* Render upto maxRectCount rectangles. */
                n2d_uint32_t lineCount = (leftLineCount < maxLineCount)
                                    ? leftLineCount
                                    : maxLineCount;

                /* Program color for each line. */
                if (ColorCount && (lastProgrammedColor != *Color32))
                {

                    /* Background color. */
                    colorConfig[0] = *Color32;

                    /* Foreground color. */
                    colorConfig[1] = *Color32;

                    /* Save last programmed color. */
                    lastProgrammedColor = *Color32;

                    /* LoadState(AQDE_SRC_COLOR_BG, 2), BgColor, FgColor. */
                    N2D_ON_ERROR(gcWriteRegWithLoadState(
                        Hardware,
                        AQDE_SRC_COLOR_BG_Address, 2,
                        colorConfig
                        ));
                }

                /* Find the number of following lines with same color. */
                if (ColorCount > 1)
                {
                    n2d_uint32_t sameColoredLines = 1;

                    Color32++;

                    while (sameColoredLines < lineCount)
                    {
                        if (lastProgrammedColor != *Color32)
                        {
                            break;
                        }

                        Color32++;
                        sameColoredLines++;
                    }

                    lineCount = sameColoredLines;
                }

                /* StartDE(RectCount). */
                memory = (n2d_uint32_t *)Hardware->cmdLogical + Hardware->cmdIndex;
                *memory++ = gcmSETFIELDVALUE(0,
                                             AQ_COMMAND_START_DE_COMMAND,
                                             OPCODE,
                                             START_DE)
                          | gcmSETFIELD(0,
                                        AQ_COMMAND_START_DE_COMMAND,
                                        COUNT,
                                        lineCount);
                memory++;

                /* Append the rectangles. */
                for (i = 0; i < lineCount; i++)
                {
                    *memory++ = gcmSETFIELD(0,
                                            AQ_COMMAND_TOP_LEFT,
                                            X,
                                            currentDestRect[i].left)
                              | gcmSETFIELD(0,
                                            AQ_COMMAND_TOP_LEFT,
                                            Y,
                                            currentDestRect[i].top);
                    *memory++ = gcmSETFIELD(0,
                                            AQ_COMMAND_BOTTOM_RIGHT,
                                            X,
                                            currentDestRect[i].right)
                              | gcmSETFIELD(0,
                                            AQ_COMMAND_BOTTOM_RIGHT,
                                            Y,
                                            currentDestRect[i].bottom);
                }

                Hardware->cmdIndex += 2 + lineCount * 2;

                leftLineCount -= lineCount;
                currentDestRect += lineCount;
                loopCount++;
            }
            while (leftLineCount && loopCount < MAX_LOOP_COUNT);

            N2D_ON_ERROR(gcRenderEnd(Hardware, N2D_TRUE));
        }
    }
    while (N2D_FALSE);

on_error:
    if (Hardware != N2D_NULL)
    {
        if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
        {
            /* Reset command buffer. */
            gcResetCmdBuffer(Hardware, N2D_IS_ERROR(error));
        }
    }

    /* Return result. */
    return error;
}

/*******************************************************************************
**
**  gcStartDEStream
**
**  Start a DE command with a monochrome stream source.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsRECT_PTR DestRect
**          Pointer to the destination rectangles.
**
**      n2d_uint32_t FgRop
**      n2d_uint32_t BgRop
**          Foreground and background ROP codes.
**
**      n2d_uint32_t StreamSize
**          Size of the stream in bytes.
**
**  OUTPUT:
**
**      n2d_pointer * StreamBits
**          Pointer to an allocated buffer for monochrome data.
*/
n2d_error_t gcStartDEStream(
    IN n2d_user_hardware_t  *Hardware,
    IN gcs2D_State_PTR      State,
    IN gcsRECT_PTR          DestRect,
    IN n2d_uint32_t         StreamSize,
    OUT n2d_pointer         *StreamBits
    )
{
    n2d_error_t error = N2D_SUCCESS;

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(DestRect != N2D_NULL);
    gcmVERIFY_ARGUMENT(StreamBits != N2D_NULL);

    do
    {
        n2d_uint32_t dataCount;
        n2d_uint32_t * memory;
        n2d_uint32_t dataSize = (2 + 2) * sizeof(n2d_uint32_t) + StreamSize;/* Determine the command size. */
        n2d_user_cmd_buf_t        *cmd_buf = N2D_NULL;

        if (!gcsValidRect(DestRect))
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }

        /*current cmdBuf logical base startmemory and offset of last use*/
        Hardware->cmdCount   = gcCalCmdbufSize(Hardware,State,gcv2D_BLT) + (gcmALIGN(dataSize, 8) >> 2);
        Hardware->cmdIndex   = 0;

        N2D_ON_ERROR(gcReserveCmdBuf(Hardware,Hardware->cmdCount * N2D_SIZEOF(n2d_uint32_t),&cmd_buf));

        Hardware->cmdLogical = (n2d_pointer)(cmd_buf->last_reserve);

        N2D_ON_ERROR(gcSetGPUState(Hardware, State, gcv2D_BLT, N2D_FALSE));

        /* Determine the data count. */
        dataCount = StreamSize >> 2;
        memory = (n2d_uint32_t *)Hardware->cmdLogical + Hardware->cmdIndex;
        /* StartDE(DataCount). */
        *memory++
            = gcmSETFIELDVALUE(0, AQ_COMMAND_START_DE_COMMAND, OPCODE,     START_DE)
            | gcmSETFIELD     (0, AQ_COMMAND_START_DE_COMMAND, COUNT,      1)
            | gcmSETFIELD     (0, AQ_COMMAND_START_DE_COMMAND, DATA_COUNT, dataCount);
        memory++;

        /* Append the rectangle. */
        *memory++
            = gcmSETFIELD(0, AQ_COMMAND_TOP_LEFT, X, DestRect->left)
            | gcmSETFIELD(0, AQ_COMMAND_TOP_LEFT, Y, DestRect->top);
        *memory++
            = gcmSETFIELD(0, AQ_COMMAND_BOTTOM_RIGHT, X, DestRect->right)
            | gcmSETFIELD(0, AQ_COMMAND_BOTTOM_RIGHT, Y, DestRect->bottom);

        /* Set the stream location. */
        *StreamBits = memory;

        Hardware->cmdIndex += gcmALIGN(dataSize, 8) >> 2;

        N2D_ON_ERROR(gcRenderEnd(Hardware, N2D_TRUE));

    }
    while (N2D_FALSE);

    /* Success. */
    return N2D_SUCCESS;

on_error:
    /* Return the error. */
    return error;
}

static n2d_void _gcInitFilterBlitArray(gcsFILTER_BLIT_ARRAY* array, gceFILTER_TYPE type)
{
    n2d_user_os_memory_fill(array, N2D_ZERO, N2D_SIZEOF(gcsFILTER_BLIT_ARRAY));
    array->filterType = type;
    array->kernelChanged = N2D_TRUE;
}

static n2d_void _gcInitFilterBlitState(gcsFilterBlitState* state)
{
    /*default filterblit state*/
    state->newFilterType = gcvFILTER_SYNC;
    state->newHorKernelSize = 5;
    state->newVerKernelSize = 5;
    state->horUserFilterPass = N2D_TRUE;
    state->verUserFilterPass = N2D_TRUE;

    _gcInitFilterBlitArray(&state->horSyncFilterKernel, gcvFILTER_SYNC);
    _gcInitFilterBlitArray(&state->verSyncFilterKernel, gcvFILTER_SYNC);
    _gcInitFilterBlitArray(&state->horBlurFilterKernel, gcvFILTER_BLUR);
    _gcInitFilterBlitArray(&state->verBlurFilterKernel, gcvFILTER_BLUR);
    _gcInitFilterBlitArray(&state->horUserFilterKernel, gcvFILTER_USER);
    _gcInitFilterBlitArray(&state->verUserFilterKernel, gcvFILTER_USER);
    _gcInitFilterBlitArray(&state->horBilinearFilterKernel, gcvFILTER_BILINEAR);
    _gcInitFilterBlitArray(&state->verBilinearFilterKernel, gcvFILTER_BILINEAR);
    _gcInitFilterBlitArray(&state->horBicubicFilterKernel, gcvFILTER_BICUBIC);
    _gcInitFilterBlitArray(&state->verBicubicFilterKernel, gcvFILTER_BICUBIC);
}

n2d_error_t gcInitHardware(
    n2d_user_hardware_t * hardware)
{
    n2d_uint32_t    corecount = 1, i, j;
    n2d_error_t     error;

    if (hardware == N2D_NULL)
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    /* Determine whether 2D hardware is present. */
    hardware->hw2DEngine = gcmVERIFYFIELDVALUE(
        hardware->chipFeatures, GC_FEATURES, PIPE_2D, AVAILABLE);

    hardware->hw2DEngine = N2D_TRUE;

    /* Don't force software by default. */
    hardware->sw2DEngine = N2D_FALSE;

    hardware->hw2DMultiSrcV2 = gcmVERIFYFIELDVALUE(
        hardware->chipMinorFeatures4,
        GC_MINOR_FEATURES4, MULTI_SRC_V2, AVAILABLE);

    hardware->chip2DControl = 0;
    hardware->hw2DSplitRect = !(hardware->chip2DControl & 0x100);

    if (hardware->chipModel == 0x320)
    {
        hardware->hw2DAppendCacheFlush = gcmVERIFYFIELDVALUE(
            hardware->chipMinorFeatures2,
            GC_MINOR_FEATURES2, FLUSH_FIXED_2D, NONE);

        n2d_user_os_memory_fill(
            &hardware->hw2DCacheFlushBuffer,
            0,
            sizeof(hardware->hw2DCacheFlushBuffer));
    }
    else
    {
        hardware->hw2DAppendCacheFlush = N2D_FALSE;
    }

    gcFilFeatureDataBase(hardware);

    /***********************************************************************
    ** Initialize filter blit states.
    */
    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_UNIFIED].type = gcvFILTER_SYNC;
    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_UNIFIED].kernelSize = 0;
    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_UNIFIED].scaleFactor = 0;
    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_UNIFIED].kernelAddress = AQDE_FILTER_KERNEL_Address;

    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_VERTICAL].type = gcvFILTER_SYNC;
    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_VERTICAL].kernelSize = 0;
    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_VERTICAL].scaleFactor = 0;
    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_VERTICAL].kernelAddress = AQDE_VERTI_FILTER_KERNEL_Address;

    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_HORIZONTAL].type = gcvFILTER_SYNC;
    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_HORIZONTAL].kernelSize = 0;
    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_HORIZONTAL].scaleFactor = 0;
    hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_HORIZONTAL].kernelAddress = AQDE_HORI_FILTER_KERNEL_Address;

    // n2d_get_core_count(&corecount);

    for (i = 0; i < corecount; ++i)
    {
        /* Initialize chip related data */
#ifdef N2D_SUPPORT_SECURE_BUFFER
        gcInitSecureBufferState(hardware);
#endif
#ifdef N2D_SUPPORT_NORMALIZATION
        gcInitNormalizationState(&hardware->state);
#endif
#ifdef N2D_SUPPORT_HISTOGRAM
        gcInitHistogramState(&hardware->state);
#endif
#ifdef N2D_SUPPORT_BRIGHTNESS
        gcInitBrightnessState(&hardware->state);
#endif

        /*default parameter*/
        hardware->fgrop = 0xcc;
        hardware->bgrop = 0xcc;

        _gcInitFilterBlitState(&hardware->state.filterState);

        hardware->state.dstCSCMode = N2D_CSC_BT709;

        hardware->state.dest.u8Tu10_cvtMode = N2D_ADD_LOWER_BITS;

        hardware->state.currentSrcIndex = 0;

        for (j = 0; j < gcdMULTI_SOURCE_NUM; ++j)
        {
            gcs2D_MULTI_SOURCE_PTR src;

            /*default alpha-blend state.*/
            src = &hardware->state.multiSrc[j];
            src->srcGlobalColor &= 0x00FFFFFF;
            src->dstGlobalColor &= 0x00FFFFFF;
            src->srcGlobalAlphaMode = gcvSURF_GLOBAL_ALPHA_OFF;
            src->dstGlobalAlphaMode = gcvSURF_GLOBAL_ALPHA_OFF;
            src->srcAlphaMode = gcvSURF_PIXEL_ALPHA_STRAIGHT;
            src->dstAlphaMode = gcvSURF_PIXEL_ALPHA_STRAIGHT;
            src->srcColorMode = gcvSURF_COLOR_STRAIGHT;
            src->dstColorMode = gcvSURF_COLOR_STRAIGHT;
            src->srcCSCMode = N2D_CSC_BT709;

            src->fgRop = 0xcc;
            src->bgRop = 0xcc;
        }
    }


    N2D_ON_ERROR(gcInitCmdBuf(hardware,NANO2D_CMDBUFFER_SIZE));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t gcDeinitHardware(
    n2d_user_hardware_t * Hardware)
{
    n2d_error_t                 error;
    n2d_buffer_t                *buffer;

    if (Hardware == N2D_NULL)
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    buffer = &Hardware->hw2DCacheFlushBuffer;

    if (Hardware->hw2DAppendCacheFlush &&
       (buffer->width != 0) && (buffer->height != 0))
    {
        N2D_ON_ERROR(n2d_free(buffer));
    }

    N2D_ON_ERROR(gcDestroySurfaceCmdBuf(Hardware, &Hardware->buffer));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}
