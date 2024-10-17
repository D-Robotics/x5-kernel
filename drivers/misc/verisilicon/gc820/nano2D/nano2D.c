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
#include "nano2D_user.h"
#include "series_common.h"

struct _gco2D
{
    n2d_uint32_t        coreCount;
    /* current core id */
    n2d_core_id_t       currentCoreId;

    n2d_user_hardware_t* hardware;
};
typedef struct _gco2D* gco2D;

struct _gcoTLS
{
    n2d_int32_t         s_running;
    n2d_uint32_t        deviceCount;
    n2d_device_id_t     currentDeviceIndex;
    gco2D               engine;
};

n2d_error_t gcTlsConstructor(gcoTLS* TLS)
{
    n2d_error_t error = N2D_SUCCESS;
    gcoTLS tls;
    n2d_pointer pointer;

    N2D_ON_ERROR(n2d_user_os_allocate(sizeof(struct _gcoTLS), &pointer));
    tls = (gcoTLS)pointer;
    n2d_user_os_memory_fill(tls, 0, sizeof(struct _gcoTLS));

    *TLS = tls;

on_error:
    return error;
}

#define gcmGETHARDWARE(Hardware) \
{ \
    gcoTLS __tls__;\
    N2D_ASSERT(n2d_user_os_get_tls(&__tls__) == N2D_SUCCESS);\
    Hardware = &__tls__->engine->hardware[__tls__->engine->currentCoreId];\
    N2D_ASSERT(Hardware != N2D_NULL); \
}

n2d_error_t gcCheckContext(void)
{
    gcoTLS tls;

    N2D_ASSERT(n2d_user_os_get_tls(&tls) == N2D_SUCCESS);
    return (tls->s_running > 0) ? N2D_SUCCESS : N2D_NO_CONTEXT;
}

static n2d_point_t gcRotatePoint(
    n2d_int32_t width,
    n2d_int32_t height,
    n2d_orientation_t orientation,
    n2d_point_t point)
{
    n2d_point_t ret;

    switch (orientation)
    {
    case N2D_90:
        ret.x = height - point.y;
        ret.y = point.x;
        break;

    case N2D_180:
        ret.x = width - point.x;
        ret.y = height - point.y;
        break;

    case N2D_270:
        ret.x = point.y;
        ret.y = width - point.x;
        break;

    case N2D_0:
    default:
        ret = point;
        break;
    }

    return ret;
}

static gcsRECT gcRotateRect(
    n2d_int32_t width,
    n2d_int32_t height,
    n2d_orientation_t orientation,
    gcsRECT rect,
    n2d_int32_t* newwidth,
    n2d_int32_t* newheight
)
{
    gcsRECT ret;
    n2d_point_t pt1, pt2;
    n2d_int32_t n = 0;

    switch (orientation)
    {
    case N2D_270:
        pt1.x = rect.right;
        pt1.y = rect.top;
        pt2.x = rect.left;
        pt2.y = rect.bottom;
        n = 1;
        break;

    case N2D_180:
        pt1.x = rect.right;
        pt1.y = rect.bottom;
        pt2.x = rect.left;
        pt2.y = rect.top;
        break;

    case N2D_90:
        pt1.x = rect.left;
        pt1.y = rect.bottom;
        pt2.x = rect.right;
        pt2.y = rect.top;
        n = 1;
        break;

    case N2D_0:
    default:
        pt1.x = rect.left;
        pt1.y = rect.top;
        pt2.x = rect.right;
        pt2.y = rect.bottom;
        break;
    }

    pt1 = gcRotatePoint(width, height, orientation, pt1);
    pt2 = gcRotatePoint(width, height, orientation, pt2);

    ret.left = pt1.x;
    ret.top = pt1.y;
    ret.right = pt2.x;
    ret.bottom = pt2.y;

    if (n == 1)
    {
        if (newwidth != N2D_NULL)
        {
            *newwidth = height;
        }

        if (newheight != N2D_NULL)
        {
            *newheight = width;
        }
    }

    return ret;
}

static gcsRECT gcRectIntersect(
    gcsRECT rect1,
    gcsRECT rect2)
{
    gcsRECT rect;

    if ((rect1.right < rect2.left) || (rect2.right < rect1.left) ||
        (rect1.bottom < rect2.top) || (rect2.bottom < rect1.top))
    {
        rect.left = 0;
        rect.right = 0;
        rect.top = 0;
        rect.bottom = 0;
    }
    else
    {
        rect.left = N2D_MAX(rect1.left, rect2.left);
        rect.right = N2D_MIN(rect1.right, rect2.right);
        rect.top = N2D_MAX(rect1.top, rect2.top);
        rect.bottom = N2D_MIN(rect1.bottom, rect2.bottom);
    }

    return rect;
}

static n2d_bool_t gcRectSame(
    gcsRECT rect1,
    gcsRECT rect2)
{
    return (rect1.left == rect2.left) &&
        (rect1.right == rect2.right) &&
        (rect1.top == rect2.top) &&
        (rect1.bottom == rect2.bottom);
}

static void gcRect2gcsRECT(
    gcsRECT* dst,
    n2d_rectangle_t* src)
{
    dst->left = src->x;
    dst->top = src->y;
    dst->right = src->x + src->width;
    dst->bottom = src->y + src->height;
}


static n2d_error_t gcEnableAlphaBlend(
    IN n2d_user_hardware_t* Hardware,
    IN gceSURF_BLEND_FACTOR_MODE SrcFactorMode,
    IN gceSURF_BLEND_FACTOR_MODE DstFactorMode
)
{
    gcs2D_MULTI_SOURCE_PTR src = &Hardware->state.multiSrc[Hardware->state.currentSrcIndex];

    gcSetBlendState(Hardware,
        src->srcAlphaMode,
        src->dstAlphaMode,
        src->srcGlobalAlphaMode,
        src->dstGlobalAlphaMode,
        src->srcColorMode,
        src->dstColorMode
    );

    src->enableAlpha = N2D_TRUE;
    src->srcFactorMode = SrcFactorMode;
    src->dstFactorMode = DstFactorMode;
    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetAlphaBlend(
    n2d_buffer_format_t srcfmt,
    n2d_buffer_format_t dstfmt,
    n2d_blend_t blend)
{
    n2d_error_t error;
    n2d_bool_t src_color = N2D_TRUE;
    n2d_bool_t dst_color = N2D_TRUE;

    n2d_user_hardware_t* hardware = N2D_NULL;

    gcs2D_MULTI_SOURCE* src = N2D_NULL;

    gcmGETHARDWARE(hardware);

    src = &hardware->state.multiSrc[hardware->state.currentSrcIndex];

    if (blend == N2D_BLEND_NONE)
    {
        src->enableAlpha = N2D_FALSE;

        return N2D_SUCCESS;
    }

    switch (blend)
    {
    case N2D_BLEND_SRC_OVER:
        gcEnableAlphaBlend(
            hardware,
            gcvSURF_BLEND_ONE,
            gcvSURF_BLEND_INVERSED
        );
        break;

    case N2D_BLEND_DST_OVER:
        gcEnableAlphaBlend(
            hardware,
            gcvSURF_BLEND_INVERSED,
            gcvSURF_BLEND_ONE
        );

        break;

    case N2D_BLEND_SRC_IN:
        gcEnableAlphaBlend(
            hardware,
            gcvSURF_BLEND_STRAIGHT,
            gcvSURF_BLEND_ZERO
        );
        dst_color = N2D_FALSE;
        break;

    case N2D_BLEND_DST_IN:
        gcEnableAlphaBlend(
            hardware,
            gcvSURF_BLEND_ZERO,
            gcvSURF_BLEND_STRAIGHT
        );
        src_color = N2D_FALSE;
        break;

    case N2D_BLEND_ADDITIVE:
        gcEnableAlphaBlend(
            hardware,
            gcvSURF_BLEND_ONE,
            gcvSURF_BLEND_ONE
        );
        break;

    case N2D_BLEND_SUBTRACT:
        gcEnableAlphaBlend(
            hardware,
            gcvSURF_BLEND_ONE,
            gcvSURF_BLEND_INVERSED
        );
        src_color = N2D_FALSE;
        break;

    default:
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        break;
    }

    /* Check the format. */
    if (src_color)
    {
        if (srcfmt == N2D_A8)
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }
    }

    if (dst_color)
    {
        if (dstfmt == N2D_A8)
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

// static void gcResetState(n2d_void)
// {
//     n2d_state_t* state = N2D_NULL;
//     n2d_uint32_t srcGlobalColor, dstGlobalColor;
//     gceSURF_PIXEL_ALPHA_MODE srcAlphaMode, dstAlphaMode;
//     gceSURF_PIXEL_COLOR_MODE srcColorMode, dstColorMode;
//     gceSURF_GLOBAL_ALPHA_MODE srcGlobalAlphaMode, dstGlobalAlphaMode;
//     /*palette state*/
//     n2d_uint32_t        paletteIndexCount;
//     n2d_uint32_t        paletteFirstIndex;
//     n2d_bool_t          paletteConvert;
//     n2d_bool_t          paletteProgram;
//     void* paletteTable;
//     gcs2D_MULTI_SOURCE* currentSrc = N2D_NULL;
//     n2d_user_hardware_t* hardware = N2D_NULL;

//     gcmGETHARDWARE(hardware);

//     state = &hardware->state;
//     currentSrc = &state->multiSrc[state->currentSrcIndex];

//     srcGlobalAlphaMode = currentSrc->srcGlobalAlphaMode;
//     dstGlobalAlphaMode = currentSrc->dstGlobalAlphaMode;
//     srcGlobalColor = currentSrc->srcGlobalColor;
//     dstGlobalColor = currentSrc->dstGlobalColor;

//     srcAlphaMode = currentSrc->srcAlphaMode;
//     dstAlphaMode = currentSrc->dstAlphaMode;
//     srcColorMode = currentSrc->srcColorMode;
//     dstColorMode = currentSrc->srcColorMode;
//     paletteIndexCount = state->paletteIndexCount;
//     paletteFirstIndex = state->paletteFirstIndex;
//     paletteConvert = state->paletteConvert;
//     paletteProgram = state->paletteProgram;
//     paletteTable = state->paletteTable;
//     paletteConvert = state->paletteConvert;

//     n2d_user_os_memory_fill(state, 0, sizeof(*state));

//     currentSrc->srcGlobalAlphaMode = srcGlobalAlphaMode;
//     currentSrc->dstGlobalAlphaMode = dstGlobalAlphaMode;
//     currentSrc->srcGlobalColor = srcGlobalColor;
//     currentSrc->dstGlobalColor = dstGlobalColor;

//     currentSrc->srcAlphaMode = srcAlphaMode;
//     currentSrc->dstAlphaMode = dstAlphaMode;
//     currentSrc->srcColorMode = srcColorMode;
//     currentSrc->srcColorMode = dstColorMode;

//     state->paletteIndexCount = paletteIndexCount;
//     state->paletteFirstIndex = paletteFirstIndex;
//     state->paletteConvert = paletteConvert;
//     state->paletteProgram = paletteProgram;
//     state->paletteTable = paletteTable;
//     state->paletteConvert = paletteConvert;
// }

static n2d_error_t gcCheckSrcFormat(
    n2d_buffer_format_t format,
    n2d_bool_t* is_yuv)
{
    n2d_error_t error;
    n2d_bool_t yuv = N2D_FALSE;

    switch (format)
    {
    case N2D_RGBA8888:
    case N2D_RGBX8888:
    case N2D_R10G10B10A2:
    case N2D_R5G5B5A1:
    case N2D_R5G5B5X1:
    case N2D_RGBA4444:
    case N2D_RGBX4444:
    case N2D_RGB565:
    case N2D_BGRA8888:
    case N2D_BGRX8888:
    case N2D_B10G10R10A2:
    case N2D_B5G5R5A1:
    case N2D_B5G5R5X1:
    case N2D_BGRA4444:
    case N2D_BGRX4444:
    case N2D_BGR565:
    case N2D_ABGR8888:
    case N2D_XBGR8888:
    case N2D_A2B10G10R10:
    case N2D_A1B5G5R5:
    case N2D_X1B5G5R5:
    case N2D_ABGR4444:
    case N2D_XBGR4444:
    case N2D_ARGB8888:
    case N2D_XRGB8888:
    case N2D_A2R10G10B10:
    case N2D_A1R5G5B5:
    case N2D_X1R5G5B5:
    case N2D_ARGB4444:
    case N2D_XRGB4444:
    case N2D_A8:
    case N2D_INDEX8:
        break;

    case N2D_YUYV:
    case N2D_UYVY:
    case N2D_YV12:
    case N2D_I420:
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
    case N2D_I010:
    case N2D_I010_LSB:

        yuv = N2D_TRUE;
        break;

    case N2D_GRAY8:
    case N2D_RGB888:
    case N2D_RGB888_PLANAR:
        if (n2d_is_feature_support(N2D_FEATURE_BRIGHTNESS_SATURATION))
        {
            break;
        }
        else
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
	    break;
	}
    default:
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    if (is_yuv != N2D_NULL)
    {
        *is_yuv = yuv;
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

static n2d_error_t gcCheckDstFormat(
    n2d_buffer_format_t format,
    n2d_uint32_t* planenum)
{
    n2d_user_hardware_t* hardware = N2D_NULL;
    n2d_error_t     error;
    n2d_uint32_t    planes = 0;

    gcmGETHARDWARE(hardware);

    switch (format)
    {
    case N2D_RGBA8888:
    case N2D_RGBX8888:
    case N2D_R10G10B10A2:
    case N2D_R5G5B5A1:
    case N2D_R5G5B5X1:
    case N2D_RGBA4444:
    case N2D_RGBX4444:
    case N2D_RGB565:
    case N2D_BGRA8888:
    case N2D_BGRX8888:
    case N2D_B10G10R10A2:
    case N2D_B5G5R5A1:
    case N2D_B5G5R5X1:
    case N2D_BGRA4444:
    case N2D_BGRX4444:
    case N2D_BGR565:
    case N2D_ABGR8888:
    case N2D_XBGR8888:
    case N2D_A2B10G10R10:
    case N2D_A1B5G5R5:
    case N2D_X1B5G5R5:
    case N2D_ABGR4444:
    case N2D_XBGR4444:
    case N2D_ARGB8888:
    case N2D_XRGB8888:
    case N2D_A2R10G10B10:
    case N2D_A1R5G5B5:
    case N2D_X1R5G5B5:
    case N2D_ARGB4444:
    case N2D_XRGB4444:
    case N2D_YUYV:
    case N2D_UYVY:
    case N2D_A8:
    case N2D_RGB888:
    case N2D_RGB888I:
    case N2D_RGB161616I:
    case N2D_ARGB16161616I:
    case N2D_ABGR16161616I:
    case N2D_ARGB8888I:
    case N2D_ABGR8888I:
    case N2D_ARGB16161616F:
    case N2D_ABGR16161616F:
    case N2D_ARGB32323232F:
    case N2D_ABGR32323232F:
    case N2D_GRAY8I:
    case N2D_GRAY8:
    case N2D_GRAY16F:
    case N2D_GRAY32F:
    case N2D_RGB161616F:
    case N2D_RGB323232F:
    case N2D_BGR888:
        planes = 1;
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
        planes = 2;
        break;

    case N2D_YV12:
    case N2D_I420:
    case N2D_I010:
    case N2D_I010_LSB:
    case N2D_RGB161616F_PLANAR:
    case N2D_RGB323232F_PLANAR:
    case N2D_RGB888_PLANAR:
    case N2D_RGB888I_PLANAR:
    case N2D_RGB161616I_PLANAR:
        planes = 3;
        break;

    case N2D_AYUV:
        if (!hardware->features[N2D_FEATURE_BGR_PLANAR])
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        planes = 3;
        break;

    default:
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    if (planenum)
    {
        *planenum = planes;
    }
    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

static n2d_error_t gcCheckTile(
    n2d_tiling_t      tiling)
{
    n2d_error_t error;

    switch (tiling)
    {
    case N2D_LINEAR:
        break;

    case N2D_TILED:
    case N2D_TILED_8X4:
    case N2D_TILED_4X8:
    case N2D_MULTI_TILED:
    case N2D_MULTI_SUPER_TILED:
    case N2D_TILED_8X8_XMAJOR:
    case N2D_TILED_8X8_YMAJOR:
    case N2D_SUPER_TILED:
    case N2D_YMAJOR_SUPER_TILED:
    case N2D_SUPER_TILED_128B:
    case N2D_SUPER_TILED_256B:
        if (n2d_is_feature_support(N2D_FEATURE_2D_TILING) != N2D_TRUE)
        {
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        }
        break;

    case N2D_MINOR_TILED:
        if (n2d_is_feature_support(N2D_FEATURE_2D_MINOR_TILING) != N2D_TRUE ||
            n2d_is_feature_support(N2D_FEATURE_ANDROID_ONLY) == N2D_TRUE)
        {
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        }
        break;

    case N2D_TILED_32X4:
    case N2D_TILED_64X4:
        if ((n2d_is_feature_support(N2D_FEATURE_DEC400_COMPRESSION) != N2D_TRUE))
        {
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        }
        break;

    default:
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

static n2d_bool_t _isMultiPlaneYUV(
    n2d_buffer_format_t format)
{
    FormatInfo_t* FormatInfo = N2D_NULL;

    gcGetFormatInfo(format, &FormatInfo);

    return FormatInfo->formatType == N2D_FORMARTTYPE_YUV && FormatInfo->planeNum > 1;
}

static n2d_error_t gcMonoBlit(
    n2d_user_hardware_t* Hardware,
    n2d_buffer_t* Destination,
    n2d_rectangle_t* DstRect,
    n2d_blend_t             blend
)
{
    n2d_error_t error;
    gcsSURF_INFO* dst;
    gcs2D_MULTI_SOURCE* src;
    gcsRECT* rect;
    gcsRECT dRect, srcOrgRect = {0};

    n2d_int32_t     dst_width, dst_height, i;
    n2d_pointer     streamBits = N2D_NULL;
    n2d_uint32_t    maxLines, lineSize, lines, streamStride;

    n2d_uint32_t    pw = 0, ph = 0;
    n2d_bool_t      ret = N2D_FALSE;
    FormatInfo_t* FormatInfo = N2D_NULL;

    dst = &Hardware->state.dest.dstSurface;
    src = &Hardware->state.multiSrc[Hardware->state.currentSrcIndex];
    N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(dst, Destination));

    dst_width = DstRect->width;
    dst_height = DstRect->height;

    N2D_ON_ERROR(gcGetFormatInfo(Destination->format, &FormatInfo));

    if (FormatInfo->planeNum > 1 && blend)
    {
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    rect = &src->srcRect;
    srcOrgRect = *rect;
    gcRect2gcsRECT(&dRect, DstRect);

    Hardware->state.dstClipRect = dRect;

    ret = N2D_TRUE;
    rect->left = src->maskPackConfig.streamOffset.x & 7;
    rect->top = 0;
    rect->right = rect->left + dst_width;
    rect->bottom = dst_height;

    if (rect->right <= 8)
    {
        src->srcMonoPack = N2D_PACKED8;
        pw = 1;
        ph = 4;
    }
    else if (rect->right <= 16)
    {
        src->srcMonoPack = N2D_PACKED16;
        pw = 2;
        ph = 2;
    }
    else
    {
        src->srcMonoPack = N2D_UNPACKED;
        pw = gcmALIGN(rect->right, 32) >> 3;
        ph = 1;
    }

    src->srcFgColor = src->maskPackConfig.fgColor;
    src->srcBgColor = src->maskPackConfig.bgColor;
    src->srcRelativeCoord = N2D_FALSE;
    src->srcColorConvert = N2D_TRUE;
    src->srcStream = N2D_TRUE;
    src->srcSurface.format = N2D_INDEX1;
    src->srcType = N2D_SOURCE_MASKED_MONO;

    streamStride = src->maskPackConfig.streamStride;

    streamBits = (n2d_uint8_t*)src->maskPackConfig.streamBits +
        (n2d_size_t)src->maskPackConfig.streamOffset.y * streamStride + (src->maskPackConfig.streamOffset.x >> 3);
    maxLines = ((gcGetMaxDataCount() << 2) / pw) & (~(ph - 1));
    lineSize = gcmALIGN(rect->right, 8) >> 3;
    lines = rect->bottom;
    rect->bottom = 0;

    N2D_ON_ERROR(gcSetAlphaBlend(
        N2D_RGBA8888,
        Destination->format,
        blend));

    src->fgRop = Hardware->fgrop;
    src->bgRop = Hardware->bgrop;

    do {
        n2d_uint8_t* buffer;
        dRect.top += rect->bottom;

        rect->bottom = N2D_MIN(maxLines, lines);
        dRect.bottom = dRect.top + rect->bottom;

        /* Call lower layer to form a StartDE command. */
        N2D_ON_ERROR(gcStartDEStream(
            Hardware,
            &Hardware->state,
            &dRect,
            pw * gcmALIGN(rect->bottom, ph),
            (n2d_pointer*)&buffer
        ));

        for (i = 0; i < rect->bottom; i++)
        {
            n2d_user_os_memory_copy(buffer, streamBits, lineSize);
            buffer += pw;
            streamBits = (n2d_uint8_t*)streamBits + streamStride;
        }

        lines -= rect->bottom;

    } while (lines > 0);

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}

static n2d_error_t gcMaskBlit(
    n2d_user_hardware_t* Hardware,
    n2d_buffer_t* Destination,
    n2d_rectangle_t* DstRect,
    n2d_buffer_t* Source,
    n2d_rectangle_t* SrcRect,
    n2d_blend_t             blend
)
{
    n2d_error_t error;
    gcsSURF_INFO* dst;
    gcs2D_MULTI_SOURCE* src;
    gcsRECT* rect;
    gcsRECT dRect, srcOrgRect = {0};

    n2d_int32_t     dst_width, dst_height, i;
    n2d_pointer     streamBits = N2D_NULL;
    n2d_uint32_t    maxLines, lineSize, lines, streamStride;

    n2d_uint32_t    bpp = 0, pw = 0, ph = 0;
    n2d_bool_t      ret = N2D_FALSE;
    n2d_int32_t  offset = 0;
    n2d_uint32_t addValue = 0;
    FormatInfo_t* FormatInfo = N2D_NULL;

    dst = &Hardware->state.dest.dstSurface;
    src = &Hardware->state.multiSrc[Hardware->state.currentSrcIndex];
    N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(dst, Destination));

    dst_width = DstRect->width;
    dst_height = DstRect->height;

    N2D_ON_ERROR(gcGetFormatInfo(Destination->format, &FormatInfo));

    if (FormatInfo->planeNum > 1 && blend)
    {
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    bpp = FormatInfo->bpp;
    rect = &src->srcRect;
    srcOrgRect = *rect;
    gcRect2gcsRECT(&dRect, DstRect);

    Hardware->state.dstClipRect = dRect;

    ret = N2D_TRUE;
    rect->left = src->maskPackConfig.streamOffset.x & 7;
    rect->top = 0;
    rect->right = rect->left + dst_width;
    rect->bottom = dst_height;

    if (rect->right <= 8)
    {
        src->srcMonoPack = N2D_PACKED8;
        pw = 1;
        ph = 4;
    }
    else if (rect->right <= 16)
    {
        src->srcMonoPack = N2D_PACKED16;
        pw = 2;
        ph = 2;
    }
    else
    {
        src->srcMonoPack = N2D_UNPACKED;
        pw = gcmALIGN(rect->right, 32) >> 3;
        ph = 1;
    }

    if (SrcRect->width != DstRect->width ||
        SrcRect->height != DstRect->height)
    {
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    gcmVERIFY_ARGUMENT(Source != N2D_NULL);

    bpp >>= 3;

    N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(&src->srcSurface, Source));

    offset = SrcRect->x - rect->left;
    switch (src->srcSurface.rotation)
    {
    case N2D_0:
        addValue = SrcRect->y * src->srcSurface.stride[0] + offset * bpp;
        break;

    case N2D_90:
        addValue = offset * src->srcSurface.stride[0];
        src->srcSurface.alignedWidth -= SrcRect->y;
        break;

    case N2D_180:
        src->srcSurface.alignedWidth -= offset;
        src->srcSurface.alignedHeight -= SrcRect->y;
        break;

    case N2D_270:
        addValue = SrcRect->y * bpp;
        src->srcSurface.alignedHeight -= offset;
        break;

    default:
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    src->srcSurface.gpuAddress[0] += addValue;

    src->srcRelativeCoord = N2D_FALSE;
    src->srcStream = N2D_FALSE;
    src->srcType = N2D_SOURCE_MASKED;
    src->srcTransparency = N2D_MASKED;

    streamStride = src->maskPackConfig.streamStride;

    streamBits = (n2d_uint8_t*)src->maskPackConfig.streamBits
        + src->maskPackConfig.streamOffset.y * streamStride
        + (src->maskPackConfig.streamOffset.x >> 3);
    maxLines = ((gcGetMaxDataCount() << 2) / pw) & (~(ph - 1));
    lineSize = gcmALIGN(rect->right, 8) >> 3;
    lines = rect->bottom;
    rect->bottom = 0;

    N2D_ON_ERROR(gcSetAlphaBlend(
        N2D_RGBA8888,
        Destination->format,
        blend));

    do {
        n2d_uint8_t* buffer;

        switch (src->srcSurface.rotation)
        {
        case N2D_0:
            offset = src->srcSurface.stride[0] * rect->bottom;
            break;

        case N2D_90:
            src->srcSurface.alignedWidth -= rect->bottom;
            break;

        case N2D_180:
            src->srcSurface.alignedHeight -= rect->bottom;
            break;

        case N2D_270:
            offset = rect->bottom * bpp;
            break;

        default:
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        }

        src->srcSurface.gpuAddress[0] += offset;
        dRect.top += rect->bottom;
        rect->bottom = N2D_MIN(maxLines, lines);
        dRect.bottom = dRect.top + rect->bottom;

        /* Call lower layer to form a StartDE command. */
        N2D_ON_ERROR(gcStartDEStream(
            Hardware,
            &Hardware->state,
            &dRect,
            pw * gcmALIGN(rect->bottom, ph),
            (n2d_pointer*)&buffer
        ));

        for (i = 0; i < rect->bottom; i++)
        {
            n2d_user_os_memory_copy(buffer, streamBits, lineSize);
            buffer += pw;
            streamBits = (n2d_uint8_t*)streamBits + streamStride;
        }

        lines -= rect->bottom;

    } while (lines > 0);

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}

static n2d_void _gcInitIdentity(
    n2d_user_hardware_t* hardware,
    n2d_kernel_command_get_hw_info_t* hw_info)
{
    hardware->chipModel = hw_info->chipModel;
    hardware->chipRevision = hw_info->chipRevision;
    hardware->productId = hw_info->productId;
    hardware->cid = hw_info->cid;
    hardware->chipFeatures = hw_info->chipFeatures;
    hardware->chipMinorFeatures = hw_info->chipMinorFeatures;
    hardware->chipMinorFeatures1 = hw_info->chipMinorFeatures1;
    hardware->chipMinorFeatures2 = hw_info->chipMinorFeatures2;
    hardware->chipMinorFeatures3 = hw_info->chipMinorFeatures3;
    hardware->chipMinorFeatures4 = hw_info->chipMinorFeatures4;
}

static n2d_error_t _gcInitEngine(
    gco2D engine,
    n2d_kernel_command_get_hw_info_t* hw_info)
{
    n2d_uint32_t i = 0;
    n2d_error_t error = N2D_SUCCESS;

    for (i = 0; i < engine->coreCount; i++)
    {
        n2d_user_hardware_t* hardware = N2D_NULL;

        hardware = &engine->hardware[i];

        _gcInitIdentity(hardware, hw_info);

        /*Set Core Index*/
        hardware->coreIndex = i;
        /* Initialize the hardware. */
        N2D_ON_ERROR(gcInitHardware(hardware));
    }

on_error:
    return error;
}

static n2d_error_t _gcFreeEngine(
    gco2D engine)
{
    n2d_uint32_t    i;
    n2d_error_t error = N2D_SUCCESS;

    for (i = 0; i < engine->coreCount; ++i)
    {
        n2d_user_hardware_t* hardware = N2D_NULL;

        hardware = &engine->hardware[i];

        if (hardware->state.paletteTable)
        {
            N2D_ON_ERROR(n2d_user_os_free(hardware->state.paletteTable));
            hardware->state.paletteTable = N2D_NULL;
        }

        if (hardware->secureBuffer)
        {
            n2d_user_os_free(hardware->secureBuffer);
        }

        N2D_FreeKernelArray(hardware->state.filterState);

        /* Close the hardware. */
        N2D_ON_ERROR(gcDeinitHardware(hardware));

    }

    N2D_ON_ERROR(n2d_user_os_free(engine->hardware));
    engine->hardware = N2D_NULL;

on_error:
    return error;
}

n2d_error_t n2d_open(
    n2d_void)
{
    n2d_error_t error;
    n2d_pointer pointer;
    gcoTLS tls;

    N2D_ON_ERROR(n2d_user_os_get_tls(&tls));

    if (tls->s_running == 0)
    {
        n2d_ioctl_interface_t       iface = {0};
        n2d_kernel_command_get_hw_info_t* hw_info = N2D_NULL;

        iface.command = N2D_KERNEL_COMMAND_OPEN;
        N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

        iface.command = N2D_KERNEL_COMMAND_GET_HW_INFO;
        /*default use device 0 and core 0 */
        iface.dev_id = N2D_DEVICE_0;
        iface.core   = N2D_CORE_0;

        /* Call the kernel .*/
        N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

        hw_info = &iface.u.command_get_hw_info;

        tls->deviceCount = hw_info->device_num;

#if N2D_TRACE
        gcTrace("%s(%d)\n", __FUNCTION__, __LINE__);
#endif

        N2D_ON_ERROR(n2d_user_os_allocate(sizeof(struct _gco2D), &pointer));
        tls->engine = (struct _gco2D*)pointer;
        n2d_user_os_memory_fill(tls->engine, 0, sizeof(struct _gco2D));

        tls->engine->coreCount = hw_info->dev_core_num;

        /* Setup the the engine object. */
        N2D_ON_ERROR(n2d_user_os_allocate(sizeof(n2d_user_hardware_t) * tls->engine->coreCount, &pointer));
        tls->engine->hardware = pointer;
        n2d_user_os_memory_fill(pointer, 0, sizeof(n2d_user_hardware_t) * tls->engine->coreCount);

        N2D_ON_ERROR(gcInitChipFuncData());

        N2D_ON_ERROR(_gcInitEngine(tls->engine, hw_info));
    }

    tls->s_running++;

    /* Success. */
    return N2D_SUCCESS;

on_error:

    N2D_ON_ERROR(_gcFreeEngine(tls->engine));
    N2D_ON_ERROR(n2d_user_os_free(tls->engine));
    tls->engine = N2D_NULL;

    return error;
}
EXPORT_SYMBOL(n2d_open);

n2d_error_t n2d_close(
    n2d_void)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_user_hardware_t* hardware = N2D_NULL;
    gcoTLS tls;

    N2D_ON_ERROR(n2d_user_os_get_tls(&tls));

    /* Check the context. */
    N2D_ON_ERROR(gcCheckContext());

    if (tls->s_running == 1)
    {
        n2d_ioctl_interface_t iface = {0};
        /* Close the dump file. */
        gcmGETHARDWARE(hardware)
        N2D_DUMP_CLOSE(hardware);
        N2D_ON_ERROR(_gcFreeEngine(tls->engine));
        N2D_ON_ERROR(n2d_user_os_free(tls->engine));
        tls->engine = N2D_NULL;

        iface.command = N2D_KERNEL_COMMAND_CLOSE;
        N2D_ON_ERROR(n2d_user_os_ioctl(&iface));
    }

    tls->s_running--;

    N2D_ON_ERROR(n2d_user_os_free_tls(&tls));

on_error:
    return error;
}
EXPORT_SYMBOL(n2d_close);

n2d_error_t n2d_allocate(
    n2d_buffer_t* buffer)
{
    n2d_error_t error;

    n2d_user_hardware_t* hardware = N2D_NULL;

    /* Check the buffer. */
    if ((buffer == N2D_NULL) ||
        (buffer->width <= 0) || (buffer->width > N2D_MAX_WIDTH) ||
        (buffer->height <= 0) || (buffer->height > N2D_MAX_HEIGHT))
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    /* Check the context. */
    N2D_ON_ERROR(gcCheckContext());

    gcmGETHARDWARE(hardware);

    N2D_ON_ERROR(gcAllocSurfaceBuffer(hardware, buffer));

    /* Compute the surface placement parameters. */
    N2D_ON_ERROR(gcAdjustSurfaceBufferParameters(hardware, buffer));

    /*tileStatusNode*/
    if (buffer->tile_status_config != N2D_TSC_DISABLE)
    {
        N2D_ON_ERROR(gcAllocSurfaceTileStatusBuffer(hardware, buffer));
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:

    n2d_free(buffer);
    return error;
}

n2d_error_t n2d_free(
    n2d_buffer_t* buffer)
{
    n2d_error_t error;
    n2d_user_hardware_t* hardware = NULL;

    /* Check the buffer. */
    if ((N2D_NULL == buffer) || (N2D_INVALID_HANDLE == buffer->handle))
    {
        return N2D_SUCCESS;
    }

    /* Check the context. */
    N2D_ON_ERROR(gcCheckContext());

    gcmGETHARDWARE(hardware);

    N2D_ON_ERROR(gcUnmapMemory(hardware, buffer->handle));
    N2D_ON_ERROR(gcFreeGpuMemory(hardware, buffer->handle));
    buffer->handle = N2D_INVALID_HANDLE;

    /*tilestatus buffer*/
    if (N2D_INVALID_HANDLE != buffer->tile_status_buffer.handle[0])
    {
        /*plane 1*/
        N2D_ON_ERROR(gcUnmapMemory(hardware, buffer->tile_status_buffer.handle[0]));
        N2D_ON_ERROR(gcFreeGpuMemory(hardware, buffer->tile_status_buffer.handle[0]));
        buffer->tile_status_buffer.handle[0] = N2D_INVALID_HANDLE;

        if (N2D_INVALID_HANDLE != buffer->tile_status_buffer.handle[1])
        {
            /*plane 2*/
            N2D_ON_ERROR(gcUnmapMemory(hardware, buffer->tile_status_buffer.handle[1]));
            N2D_ON_ERROR(gcFreeGpuMemory(hardware, buffer->tile_status_buffer.handle[1]));
            buffer->tile_status_buffer.handle[1] = N2D_INVALID_HANDLE;
            if (N2D_INVALID_HANDLE != buffer->tile_status_buffer.handle[2])
            {
                /*plane 3*/
                N2D_ON_ERROR(gcUnmapMemory(hardware, buffer->tile_status_buffer.handle[2]));
                N2D_ON_ERROR(gcFreeGpuMemory(hardware, buffer->tile_status_buffer.handle[2]));
                buffer->tile_status_buffer.handle[2] = N2D_INVALID_HANDLE;
            }
        }
    }

#ifdef N2D_SUPPORT_HISTOGRAM
    /*calc buffer*/
    if (N2D_INVALID_HANDLE != hardware->state.dest.dstSurface.histogram_buffer[0].handle)
    {
        /* plane 1 */
        N2D_ON_ERROR(gcUnmapMemory(hardware,
            hardware->state.dest.dstSurface.histogram_buffer[0].handle));
        N2D_ON_ERROR(gcFreeGpuMemory(hardware,
            hardware->state.dest.dstSurface.histogram_buffer[0].handle));
        hardware->state.dest.dstSurface.histogram_buffer[0].handle = N2D_INVALID_HANDLE;

        if (N2D_INVALID_HANDLE != hardware->state.dest.dstSurface.histogram_buffer[1].handle)
        {
            /* plane 2 */
            N2D_ON_ERROR(gcUnmapMemory(hardware,
                hardware->state.dest.dstSurface.histogram_buffer[1].handle));
            N2D_ON_ERROR(gcFreeGpuMemory(hardware,
                hardware->state.dest.dstSurface.histogram_buffer[1].handle));
            hardware->state.dest.dstSurface.histogram_buffer[1].handle = N2D_INVALID_HANDLE;

            if (N2D_INVALID_HANDLE != hardware->state.dest.dstSurface.histogram_buffer[2].handle)
            {
                /* plane 3 */
                N2D_ON_ERROR(gcUnmapMemory(hardware,
                    hardware->state.dest.dstSurface.histogram_buffer[2].handle));
                N2D_ON_ERROR(gcFreeGpuMemory(hardware,
                    hardware->state.dest.dstSurface.histogram_buffer[2].handle));
                hardware->state.dest.dstSurface.histogram_buffer[2].handle = N2D_INVALID_HANDLE;
            }
        }
    }
#endif

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}
EXPORT_SYMBOL(n2d_free);

n2d_error_t _mapMemory(n2d_uintptr_t handle, n2d_uintptr_t* gpu, n2d_pointer* memory)
{
    n2d_user_hardware_t* hardware = N2D_NULL;
    n2d_ioctl_interface_t iface = {0};
    n2d_error_t error = N2D_SUCCESS;
    gcoTLS tls;

    gcmGETHARDWARE(hardware);

    N2D_ON_ERROR(n2d_user_os_get_tls(&tls));

    iface.core = tls->engine->currentCoreId;

    if (handle)
    {
        N2D_ON_ERROR(gcMapMemory(hardware, handle, &iface));

        *gpu = (n2d_uint32_t)iface.u.command_map.address;
        *memory = (n2d_pointer)iface.u.command_map.logical;
    }

on_error:
    return error;
}

n2d_error_t n2d_wrap(n2d_user_memory_desc_t* memDesc, n2d_uintptr_t* handle)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_ioctl_interface_t iface = {0};
    gcoTLS tls;

    N2D_ON_ERROR(n2d_user_os_get_tls(&tls));

    iface.command = N2D_KERNEL_COMMAND_WRAP_USER_MEMORY;
    iface.core = tls->engine->currentCoreId;

    iface.u.command_wrap_user_memory.fd_handle = memDesc->handle;
    iface.u.command_wrap_user_memory.flag = memDesc->flag;
    iface.u.command_wrap_user_memory.logical = memDesc->logical;
    iface.u.command_wrap_user_memory.physical = memDesc->physical;
    iface.u.command_wrap_user_memory.size = memDesc->size;

    n2d_get_device_index(&iface.dev_id);

    /* Call kernel driver. */
    N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

    /* Get wrapped handle. */
    *handle = iface.u.command_wrap_user_memory.handle;

on_error:
    return error;
}
EXPORT_SYMBOL(n2d_wrap);

n2d_error_t n2d_map(
    n2d_buffer_t* buffer)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t bits = 0;
    n2d_user_hardware_t* hardware = N2D_NULL;

    /* Check the buffer. */
    if ((buffer == N2D_NULL) ||
        (buffer->width <= 0) || (buffer->width > N2D_MAX_WIDTH) ||
        (buffer->height <= 0) || (buffer->height > N2D_MAX_HEIGHT))
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    /* Check the context. */
    N2D_ON_ERROR(gcCheckContext());

    gcmGETHARDWARE(hardware);

    /* Set the stride. */
    if (buffer->stride == 0)
    {
        N2D_ON_ERROR(gcConvertFormat(
            buffer->format,
            &bits));

        buffer->stride = gcmALIGN(buffer->width * bits / 8, 64);
    }

    /* Since the memory of three planes is continuous, we just need to map once.
       Then adjust the memory address and assign to each plane */
    N2D_ON_ERROR(_mapMemory(buffer->handle, &buffer->gpu, &buffer->memory));

    /* For tilestatus, Y and UV each have a section of memory, we need to map separately */
    N2D_ON_ERROR(_mapMemory(buffer->tile_status_buffer.handle[0],
        &buffer->tile_status_buffer.gpu_addr[0], &buffer->tile_status_buffer.memory[0]));
    N2D_ON_ERROR(_mapMemory(buffer->tile_status_buffer.handle[1],
        &buffer->tile_status_buffer.gpu_addr[1], &buffer->tile_status_buffer.memory[1]));

    N2D_ON_ERROR(gcAdjustSurfaceBufferParameters(hardware, buffer));

on_error:
    return error;
}
EXPORT_SYMBOL(n2d_map);

n2d_error_t n2d_unmap(
    n2d_buffer_t* buffer)
{
    n2d_error_t error;
    n2d_user_hardware_t* hardware = N2D_NULL;
    /* Check the buffer. */
    if ((buffer == N2D_NULL) || (buffer->handle == N2D_INVALID_HANDLE))
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    /* Check the context. */
    N2D_ON_ERROR(gcCheckContext());

    gcmGETHARDWARE(hardware);

    N2D_ON_ERROR(gcUnmapMemory(hardware, buffer->handle));
    N2D_ON_ERROR(gcUnmapMemory(hardware, buffer->tile_status_buffer.handle[0]));
    N2D_ON_ERROR(gcUnmapMemory(hardware, buffer->tile_status_buffer.handle[1]));

    buffer->handle = N2D_INVALID_HANDLE;
    buffer->tile_status_buffer.handle[0] = N2D_INVALID_HANDLE;
    buffer->tile_status_buffer.handle[1] = N2D_INVALID_HANDLE;

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t n2d_commit(
    n2d_void)
{
    n2d_error_t error;

    N2D_ON_ERROR(n2d_commit_ex(N2D_TRUE));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}
EXPORT_SYMBOL(n2d_commit);

n2d_error_t n2d_fill(
    n2d_buffer_t* destination,
    n2d_rectangle_t* rectangle,
    n2d_color_t color,
    n2d_blend_t blend)
{
    n2d_error_t error;
    gcsRECT rect;
    gcsSURF_INFO* dst;
    n2d_state_t* state;
    n2d_uint32_t    planes;
    n2d_user_hardware_t* hardware = N2D_NULL;
    /* Check the context. */
    N2D_ON_ERROR(gcCheckContext());

    /* Check the buffer. */
    if (destination == N2D_NULL)
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    gcmGETHARDWARE(hardware);

    N2D_ON_ERROR(gcCheckDstFormat(destination->format, &planes));

    N2D_ON_ERROR(gcCheckTile(destination->tiling));

    state = &hardware->state;
    dst = &state->dest.dstSurface;


    N2D_ON_ERROR(gcSetAlphaBlend(
        N2D_RGBA8888,
        destination->format,
        blend));

    /* Set the clipping area and dest rectangle. */
    rect.left = 0;
    rect.top = 0;
    rect.right = destination->width;
    rect.bottom = destination->height;

    rect = gcRotateRect(
        destination->width,
        destination->height,
        destination->orientation,
        rect,
        N2D_NULL,
        N2D_NULL);

    state->dstClipRect = rect;

    if (rectangle != N2D_NULL)
    {
        gcRect2gcsRECT(&rect, rectangle);
    }

    N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(dst, destination));

    /* Set the target format. */
    state->dest.dstSurface.format = destination->format;

    state->clearColor = color;

    /* Clear. */
    N2D_ON_ERROR(gcClearRectangle(
        hardware,
        state,
        1,
        &rect
    ));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t n2d_blit(
    n2d_buffer_t* destination,
    n2d_rectangle_t* destination_rectangle,
    n2d_buffer_t* source,
    n2d_rectangle_t* source_rectangle,
    n2d_blend_t     blend)
{
    n2d_error_t   error;
    gcsRECT rect, tmprect;
    gcsSURF_INFO* dst;
    n2d_state_t* state;
    n2d_int32_t src_width, src_height;
    n2d_int32_t dst_width, dst_height;
    n2d_bool_t      is_src_yuv;
    n2d_uint32_t    planes;
    gcs2D_MULTI_SOURCE* currentSrc;
    n2d_user_hardware_t* hardware = N2D_NULL;
    gce2D_COMMAND       command = gcv2D_BLT;

    /* Check the context. */
    N2D_ON_ERROR(gcCheckContext());

    gcmGETHARDWARE(hardware);

    /* Check the argument. */
    gcmVERIFY_ARGUMENT(destination != N2D_NULL);

    /*maskblit,*/
    // if ((&hardware->state.multiSrc[hardware->state.currentSrcIndex].maskPackConfig) != N2D_NULL
    //     && hardware->features[N2D_FEATURE_TRANSPARENCY_MASK])
    if (hardware->features[N2D_FEATURE_TRANSPARENCY_MASK])
    {
        if (source == N2D_NULL)
        {
            N2D_ON_ERROR(gcMonoBlit(hardware, destination, destination_rectangle, blend));
            return N2D_SUCCESS;
        }
        else if (source->srcType == N2D_SOURCE_MASKED)
        {
            N2D_ON_ERROR(gcMaskBlit(hardware, destination, destination_rectangle, source, source_rectangle, blend));
            return N2D_SUCCESS;
        }
    }

    gcmVERIFY_ARGUMENT(source != N2D_NULL);

    /* don't support rotation/blend when dst is multiplane YUV format */
    if (_isMultiPlaneYUV(destination->format) && (N2D_0 != destination->orientation || N2D_BLEND_NONE != blend))
    {
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    N2D_ON_ERROR(gcCheckSrcFormat(source->format, &is_src_yuv));
    N2D_ON_ERROR(gcCheckTile(source->tiling));

    N2D_ON_ERROR(gcCheckDstFormat(destination->format, &planes));
    N2D_ON_ERROR(gcCheckTile(destination->tiling));

    state = &hardware->state;
    dst = &state->dest.dstSurface;
    currentSrc = &state->multiSrc[state->currentSrcIndex];

    N2D_ON_ERROR(gcSetAlphaBlend(
        N2D_RGBA8888,
        destination->format,
        blend));

    N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(dst, destination));
    dst_width = destination->width;
    dst_height = destination->height;

    rect.left = 0;
    rect.top = 0;
    rect.right = dst_width;
    rect.bottom = dst_height;

    rect = gcRotateRect(
        dst_width,
        dst_height,
        destination->orientation,
        rect,
        &dst_width,
        &dst_height
    );

    state->dstClipRect = rect;

    if (destination_rectangle != N2D_NULL)
    {
        tmprect = rect;

        dst_width = destination_rectangle->width;
        dst_height = destination_rectangle->height;

        gcRect2gcsRECT(&rect, destination_rectangle);

        rect = gcRectIntersect(rect, tmprect);

    }

    N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(&currentSrc->srcSurface, source));

    src_width = source->width;
    src_height = source->height;

    currentSrc->srcRect.left = 0;
    currentSrc->srcRect.top = 0;
    currentSrc->srcRect.right = src_width;
    currentSrc->srcRect.bottom = src_height;

    currentSrc->srcRect = gcRotateRect(
        src_width,
        src_height,
        source->orientation,
        currentSrc->srcRect,
        &src_width,
        &src_height);

    if (source_rectangle != N2D_NULL)
    {
        tmprect = currentSrc->srcRect;

        src_width = source_rectangle->width;
        src_height = source_rectangle->height;

        gcRect2gcsRECT(&currentSrc->srcRect, source_rectangle);

        currentSrc->srcRect = gcRectIntersect(currentSrc->srcRect, tmprect);

    }

    currentSrc->srcType = N2D_SOURCE_DEFAULT;

    /* Set the target format. */
    state->dest.dstSurface.format = destination->format;

    if ((src_width != dst_width) || (src_height != dst_height))
    {
        currentSrc->horFactor =
            gcGetStretchFactor(N2D_FALSE, src_width, dst_width);

        currentSrc->verFactor =
            gcGetStretchFactor(N2D_FALSE, src_height, dst_height);

        command = gcv2D_STRETCH;
    }

    hardware->isStretchMultisrcStretchBlit = 1;
    state->srcMask = 1 << state->currentSrcIndex;
    state->multiSrc[state->currentSrcIndex].dstRect = rect;
    command = gcv2D_MULTI_SOURCE_BLT;

    N2D_ON_ERROR(gcStartDE(
        hardware,
        state,
        command,
        0,
        N2D_NULL,
        1,
        &rect
    ));

    hardware->isStretchMultisrcStretchBlit = 0;
    state->multiSrc[state->currentSrcIndex].dstRect.left = 0;
    state->multiSrc[state->currentSrcIndex].dstRect.top = 0;
    state->multiSrc[state->currentSrcIndex].dstRect.right = 0;
    state->multiSrc[state->currentSrcIndex].dstRect.bottom = 0;

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}
EXPORT_SYMBOL(n2d_blit);

n2d_error_t n2d_line(
    n2d_buffer_t* destination,
    n2d_point_t start,
    n2d_point_t end,
    n2d_rectangle_t* clip,
    n2d_color_t color,
    n2d_blend_t blend)
{
    n2d_error_t error;
    gcsRECT position, rect;
    gcsSURF_INFO* dst;
    n2d_state_t* state;
    n2d_uint32_t    planes;
    gcs2D_MULTI_SOURCE* currentSrc;
    n2d_user_hardware_t* hardware = N2D_NULL;

    /* Check the context. */
    N2D_ON_ERROR(gcCheckContext());

    gcmGETHARDWARE(hardware);

    /* Check the buffer. */
    if (destination == N2D_NULL)
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    N2D_ON_ERROR(gcCheckDstFormat(destination->format, &planes));
    N2D_ON_ERROR(gcCheckTile(destination->tiling));

    state = &hardware->state;
    dst = &state->dest.dstSurface;

    N2D_ON_ERROR(gcSetAlphaBlend(
        N2D_RGBA8888,
        destination->format,
        blend));

    position.left = start.x;
    position.top = start.y;
    position.right = end.x;
    position.bottom = end.y;

    N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(dst, destination));

    rect.left = 0;
    rect.top = 0;
    rect.right = destination->width;
    rect.bottom = destination->height;

    rect = gcRotateRect(
        destination->width,
        destination->height,
        destination->orientation,
        rect,
        N2D_NULL,
        N2D_NULL);

    if (clip != N2D_NULL)
    {
        gcRect2gcsRECT(&state->dstClipRect, clip);

        state->dstClipRect = gcRectIntersect(state->dstClipRect, rect);
    }
    else
    {
        state->dstClipRect = rect;
    }

    currentSrc = &state->multiSrc[state->currentSrcIndex];
    /* Set the solid color using a monochrome stream. */
    currentSrc->srcType = N2D_SOURCE_MASKED_MONO;
    currentSrc->srcStream = N2D_FALSE;
    currentSrc->srcRect.left = 0;
    currentSrc->srcRect.right = 0;
    currentSrc->srcRect.top = 0;
    currentSrc->srcRect.bottom = 0;

    currentSrc->fgRop = 0xcc;
    currentSrc->bgRop = 0xcc;

    /* Set the target format. */
    state->dest.dstSurface.format = destination->format;

    /* Draw the lines. */
    N2D_ON_ERROR(gcStartDELine(
        hardware,
        state,
        gcv2D_LINE,
        1,
        &position,
        1,
        (n2d_uint32_t*)&color
    ));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t n2d_set_source(
    n2d_buffer_t* source,
    n2d_rectangle_t* src_rect,
    n2d_rectangle_t* to_dst)
{
    n2d_error_t             error;
    n2d_state_t* state;
    gcsRECT                 rect, tmprect;
    n2d_int32_t             src_width, src_height;
    n2d_bool_t              is_yuv;
    gcs2D_MULTI_SOURCE* currentSrc;
    n2d_user_hardware_t* hardware = N2D_NULL;

    gcmGETHARDWARE(hardware);

    N2D_ASSERT(source != N2D_NULL);
    N2D_ASSERT(to_dst != N2D_NULL);

    if ((hardware->state.dest.dstSurface.tile_status_config == N2D_TSC_DEC_COMPRESSED)
        && !hardware->features[N2D_FEATURE_ANDROID_ONLY]
        && ((source->format != N2D_BGRA8888) && (source->format != N2D_BGRX8888)))
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    N2D_ON_ERROR(gcCheckSrcFormat(source->format, &is_yuv));
    N2D_ON_ERROR(gcCheckTile(source->tiling));

    state = &hardware->state;
    currentSrc = &state->multiSrc[state->currentSrcIndex];

    N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(&currentSrc->srcSurface, source));

    src_width = source->width;
    src_height = source->height;

    rect.left = 0;
    rect.top = 0;
    rect.right = src_width;
    rect.bottom = src_height;

    currentSrc->srcRect = gcRotateRect(
        src_width,
        src_height,
        source->orientation,
        rect,
        &src_width,
        &src_height);

    if (src_rect != N2D_NULL)
    {
        tmprect = currentSrc->srcRect;

        src_width = src_rect->width;
        src_height = src_rect->height;

        gcRect2gcsRECT(&currentSrc->srcRect, src_rect);

        tmprect = gcRectIntersect(currentSrc->srcRect, tmprect);

        if (!gcRectSame(currentSrc->srcRect, tmprect))
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }
    }

    currentSrc->srcType = N2D_SOURCE_DEFAULT;

    /* config in n2d_draw_state*/
    currentSrc->fgRop = hardware->fgrop;
    currentSrc->bgRop = hardware->bgrop;

    currentSrc->enableGDIStretch = N2D_TRUE;

    gcRect2gcsRECT(&rect, to_dst);

    currentSrc->dstRect = rect;

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}


/*******************************************************************************
**
**  n2d_multisource_blit
**
**  Multi blit with multi sources.
**
**  INPUT:
**
**      n2d_buffer_t *destination
**          destination buffer.
**
**      n2d_int32_t sourceMask
**          Indicate which source of the total 8 would be used to do
**          MultiSrcBlit(composition). Bit N represents the source index N.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t n2d_multisource_blit(
    n2d_buffer_t* destination,
    n2d_int32_t     sourceMask)
{
    n2d_error_t     error;
    gcsRECT         rect, tmprect, dstrect;
    gcsSURF_INFO* dst, * src;
    n2d_state_t* state;
    n2d_int32_t     dst_width, dst_height;
    n2d_uint32_t    planes;
    n2d_int32_t     maxSrc = 0, i = 0;
    n2d_user_hardware_t* hardware = N2D_NULL;

    gcmGETHARDWARE(hardware);

    /* Check the context. */
    N2D_ON_ERROR(gcCheckContext());

    /* Check the argument. */
    gcmVERIFY_ARGUMENT(destination != N2D_NULL);

    if ((hardware->state.dest.dstSurface.tile_status_config == N2D_TSC_DEC_COMPRESSED)
        && !hardware->features[N2D_FEATURE_ANDROID_ONLY]
        && ((destination->format != N2D_BGRA8888) && (destination->format != N2D_BGRX8888)))
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    if (hardware->features[N2D_FEATURE_2D_MULTI_SOURCE_BLT])
    {
        maxSrc = 8;
    }
    else
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    if ((maxSrc > gcdMULTI_SOURCE_NUM)
        || (sourceMask & (~0U << maxSrc))
        || !(sourceMask & (~(~0U << maxSrc))))
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    N2D_ON_ERROR(gcCheckDstFormat(destination->format, &planes));
    N2D_ON_ERROR(gcCheckTile(destination->tiling));

    state = &hardware->state;
    dst = &state->dest.dstSurface;

    N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(dst, destination));
    /* Set the target format. */
    state->dest.dstSurface.format = destination->format;

    dst_width = destination->width;
    dst_height = destination->height;

    rect.left = 0;
    rect.top = 0;
    rect.right = dst_width;
    rect.bottom = dst_height;

    rect = gcRotateRect(
        dst_width,
        dst_height,
        destination->orientation,
        rect,
        &dst_width,
        &dst_height
    );

    for (i = 0; i < maxSrc; i++)
    {
        if (!(sourceMask & (1 << i)))
        {
            continue;
        }

        src = &hardware->state.multiSrc[i].srcSurface;
        dstrect = hardware->state.multiSrc[i].dstRect;
        tmprect = gcRectIntersect(dstrect, rect);

        if (!gcRectSame(dstrect, tmprect))
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }

    }

    state->srcMask = sourceMask;

    /* Set the source. */
    N2D_ON_ERROR(gcStartDE(
        hardware,
        state,
        gcv2D_MULTI_SOURCE_BLT,
        0,
        N2D_NULL,
        0,
        N2D_NULL
    ));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}

/*******************************************************************************
**
**  n2d_filterblit
**
**  Filter blit.
**
**  INPUT:
**
**      n2d_buffer_t *destination,
**          destination buffer.
**
**      n2d_buffer_t *destination
**          Coordinates of the entire destination image.
**
**      n2d_rectangle_t *destination_subrectangle
**          Coordinates of a sub area within the destination to render.
**          If destination_subrectangle is N2D_NULL, the complete image will be rendered
**          using coordinates set by DstRect.
**          If destination_subrectangle is not N2D_NULL and DstSubRect and DstRect are
**          no equal, DstSubRect is assumed to be within DstRect and
**          will be used to render the sub area only.
**
**      n2d_buffer_t *source
**          source buffer.
**
**      n2d_rectangle_t *source_rectangle
**          Coordinates of the entire source image.
**
**      n2d_blend_t blend
**          alphablend mode.
**
**  OUTPUT:
**
**      Nothing.
*/
// n2d_error_t n2d_filterblit(
//     n2d_buffer_t* destination,
//     n2d_rectangle_t* destination_rectangle,
//     n2d_rectangle_t* destination_subrectangle,
//     n2d_buffer_t* source,
//     n2d_rectangle_t* source_rectangle,
//     n2d_blend_t         blend
// )
// {
//     n2d_error_t         error;
//     gcsSURF_INFO* dst = N2D_NULL;
//     n2d_state_t* state = N2D_NULL;
//     n2d_bool_t          is_src_yuv = N2D_FALSE;
//     gcs2D_MULTI_SOURCE* currentSrc = N2D_NULL;
//     gcsRECT             dstSubRect = {0};
//     gcsRECT_PTR         pDstSubRect = N2D_NULL;
//     n2d_user_hardware_t* hardware = N2D_NULL;

//     gcmGETHARDWARE(hardware);

//     /* Check the context. */
//     N2D_ON_ERROR(gcCheckContext());

//     /* Check the argument. */
//     gcmVERIFY_ARGUMENT(source != N2D_NULL);
//     gcmVERIFY_ARGUMENT(destination != N2D_NULL);
//     gcmVERIFY_ARGUMENT(source_rectangle != N2D_NULL);
//     gcmVERIFY_ARGUMENT(destination_rectangle != N2D_NULL);

//     /* don't support rotation/blend when dst is multiplane YUV format */
//     if (_isMultiPlaneYUV(destination->format) && (N2D_0 != destination->orientation || N2D_BLEND_NONE != blend))
//     {
//         N2D_ON_ERROR(N2D_NOT_SUPPORTED);
//     }

//     N2D_ON_ERROR(gcCheckSrcFormat(source->format, &is_src_yuv));
//     N2D_ON_ERROR(gcCheckTile(source->tiling));

//     N2D_ON_ERROR(gcCheckDstFormat(destination->format, N2D_NULL));
//     N2D_ON_ERROR(gcCheckTile(destination->tiling));

//     state = &hardware->state;
//     dst = &state->dest.dstSurface;
//     currentSrc = &state->multiSrc[state->currentSrcIndex];

//     N2D_ON_ERROR(gcSetAlphaBlend(
//         N2D_RGBA8888,
//         destination->format,
//         blend));

//     N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(dst, destination));
//     gcRect2gcsRECT(&state->dstClipRect, destination_rectangle);

//     N2D_ON_ERROR(gcConvertBufferToSurfaceBuffer(&currentSrc->srcSurface, source));
//     gcRect2gcsRECT(&currentSrc->srcRect, source_rectangle);

//     currentSrc->srcType = N2D_SOURCE_DEFAULT;
//     currentSrc->fgRop = hardware->fgrop;
//     currentSrc->bgRop = hardware->bgrop;

//     /* Set the target format. */
//     state->dest.dstSurface.format = destination->format;

//     if (N2D_NULL != destination_subrectangle) {
//         gcRect2gcsRECT(&dstSubRect, destination_subrectangle);
//         pDstSubRect = &dstSubRect;
//     }

//     N2D_ON_ERROR(gcFilterBlit(
//         hardware,
//         state,
//         &currentSrc->srcSurface,
//         &state->dest.dstSurface,
//         &currentSrc->srcRect,
//         &state->dstClipRect,
//         pDstSubRect
//     ));

//     /* Success. */
//     return N2D_SUCCESS;

// on_error:
//     return error;
// }


/*******************************************************************************
**
**  n2d_load_palette
**
**  Load 256-entry color table for INDEX8 source surfaces.
**
**  INPUT:
**
**      n2d_uint32_t first_index
**          The index to start loading from (0..255).
**
**      n2d_uint32_t index_count
**          The number of indices to load (first_index + index_count <= 256).
**
**      n2d_pointer color_table
**          Pointer to the color table to load. The value of the pointer should
**          be set to the first value to load no matter what the value of
**          FirstIndex is. The table must consist of 32-bit entries that contain
**          color values in either ARGB8 or the destination color format
**          (see color_convert).
**
**      n2d_bool_t color_convert
**          If set to N2D_TRUE, the 32-bit values in the table are assumed to be
**          in ARGB8 format and will be converted by the hardware to the
**          destination format as needed.
**          If set to N2D_FALSE, the 32-bit values in the table are assumed to be
**          preconverted to the destination format.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t n2d_load_palette(
    n2d_uint32_t first_index,
    n2d_uint32_t index_count,
    n2d_pointer color_table,
    n2d_bool_t  color_convert
)
{
    n2d_error_t error;
    n2d_state_t* state;
    n2d_user_hardware_t* hardware = N2D_NULL;

    gcmGETHARDWARE(hardware);

    N2D_ASSERT(hardware != N2D_NULL);

    /* Check the context. */
    N2D_ON_ERROR(gcCheckContext());

    /* Verify the arguments. */
    if (color_table == N2D_NULL)
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    if ((index_count > 256) || (first_index > 256))
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }


    state = &hardware->state;

    if (state->paletteTable == N2D_NULL)
    {
        n2d_pointer pointer = N2D_NULL;
        N2D_ON_ERROR(n2d_user_os_allocate(sizeof(n2d_uint32_t) * 256, &pointer));
        state->paletteTable = pointer;
    }

    n2d_user_os_memory_copy(state->paletteTable, color_table, index_count * 4);
    state->paletteIndexCount = index_count;
    state->paletteFirstIndex = first_index;
    state->paletteConvert = color_convert;
    state->paletteProgram = N2D_TRUE;

    return N2D_SUCCESS;

on_error:
    /* Return status. */
    return error;
}

/*******************************************************************************
**
**  n2d_is_feature_support
**
**  query the feature support information.
**
**  INPUT:
**
**      n2d_feature_t feature
**          N2D hardware feature name.
**
**
**  RETURN:
**
**  n2d_bool_t,
**      N2D_TRUE, support the queried feature
**      N2D_FALSE, cannot support the feature
*/
n2d_bool_t n2d_is_feature_support(
    n2d_feature_t feature)
{
    n2d_user_hardware_t* hardware = N2D_NULL;

    gcmGETHARDWARE(hardware);

    if (feature >= N2D_FEATURE_COUNT || feature < N2D_ZERO)
        return N2D_FALSE;

    return hardware->features[feature];
}

n2d_void n2d_get_core_count(n2d_uint32_t* val)
{
    gcoTLS tls;

    N2D_ASSERT(n2d_user_os_get_tls(&tls) == N2D_SUCCESS);
    *val = tls->engine->coreCount;
}

n2d_void n2d_get_core_index(n2d_core_id_t* val)
{
    gcoTLS tls;

    N2D_ASSERT(n2d_user_os_get_tls(&tls) == N2D_SUCCESS);
    *val = tls->engine->currentCoreId;
}

n2d_void n2d_get_device_count(n2d_uint32_t* val)
{
    gcoTLS tls;

    N2D_ASSERT(n2d_user_os_get_tls(&tls) == N2D_SUCCESS);
    *val = tls->deviceCount;
}

n2d_void n2d_get_device_index(n2d_device_id_t* val)
{
    gcoTLS tls;

    N2D_ASSERT(n2d_user_os_get_tls(&tls) == N2D_SUCCESS);
    *val = tls->currentDeviceIndex;
}

/*******************************************************************************
**
**  n2d_switch_device
**
**  switch to device that next use.
**
**  INPUT:
**
**      which device next use
**
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t n2d_switch_device(
    n2d_device_id_t deviceId)
{
    gcoTLS tls;
    n2d_error_t error = N2D_SUCCESS;

    N2D_ON_ERROR(n2d_user_os_get_tls(&tls));
    N2D_ASSERT(deviceId >= 0 && deviceId < (n2d_int32_t)tls->deviceCount);

    tls->currentDeviceIndex = deviceId;

on_error:
    return error;
}

/*******************************************************************************
**
**  n2d_switch_core
**
**  switch to core that next use.
**
**  INPUT:
**
**      which core next use
**
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t n2d_switch_core(
    n2d_core_id_t coreId)
{
    gcoTLS tls;
    n2d_error_t error = N2D_SUCCESS;

    N2D_ON_ERROR(n2d_user_os_get_tls(&tls));
    N2D_ASSERT(coreId >= 0 && coreId < (n2d_int32_t)tls->engine->coreCount);

    tls->engine->currentCoreId = coreId;

on_error:
    return error;
}

/*******************************************************************************
**
**  n2d_commit_ex
**
**  commit command buffer to HW.
**
**  INPUT
**
**      stall
**          Whether to wait for HW to complete
**
*/
n2d_error_t n2d_commit_ex(
    n2d_bool_t stall)
{
    n2d_error_t error;
    n2d_user_hardware_t* hardware = N2D_NULL;

    gcmGETHARDWARE(hardware);

    N2D_PERF_TIME_STAMP_PRINT(__FUNCTION__, "START");

    N2D_ON_ERROR(gcCommitCmdBuf(hardware));

    if (stall) {
        N2D_ON_ERROR(gcCommitSignal(hardware));
        N2D_ON_ERROR(gcWaitSignal(hardware, hardware->buffer.command_buffer_tail->stall_signal, N2D_INFINITE));
    }

    N2D_PERF_TIME_STAMP_PRINT(__FUNCTION__, "END");

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}
EXPORT_SYMBOL(n2d_commit_ex);

/*******************************************************************************
**
**  n2d_set
**
**  Configure various parameters,the parameter type can refer to n2d_state_type_t
**  the parameter type can refer to n2d_state_type_t
**
**  INPUT :
**      n2d_state_config_t
**          various parameters
**  RETURN:
**      n2d_error_t
**          error type
**
*/
n2d_error_t n2d_set(
    n2d_state_config_t* config)
{
    n2d_error_t  error = N2D_SUCCESS;
    n2d_user_hardware_t* hardware = N2D_NULL;
    n2d_error_t(*func)(n2d_user_hardware_t*, n2d_pointer) = N2D_NULL;

    gcmGETHARDWARE(hardware);

    if (N2D_NULL == config)
    {
        return N2D_INVALID_ARGUMENT;
    }

    N2D_ON_ERROR(gcGetStateConfigFunc(config->state,
        (n2d_get_state_type_t)N2D_ZERO, (n2d_pointer*)&func, N2D_TRUE));

    N2D_ON_ERROR(func(hardware, config));

on_error:
    /* Return status. */
    return error;

}

/*******************************************************************************
**
**  n2d_get
**
**  Configure various parameters,the parameter type can refer to n2d_get_state_type_t
**  the parameter type can refer to n2d_get_state_type_t
**
**  INPUT :
**      n2d_state_config_t
**          various parameters
**  RETURN:
**      n2d_error_t
**          error type
**
*/
n2d_error_t n2d_get(
    n2d_get_state_config_t* config)
{
    n2d_error_t  error = N2D_SUCCESS;
    n2d_user_hardware_t* hardware = N2D_NULL;
    n2d_error_t(*func)(n2d_user_hardware_t*, n2d_pointer) = N2D_NULL;

    gcmGETHARDWARE(hardware);

    if (N2D_NULL == config)
    {
        return N2D_INVALID_ARGUMENT;
    }

    N2D_ON_ERROR(gcGetStateConfigFunc((n2d_state_type_t)N2D_ZERO,
        config->state, (n2d_pointer*)&func, N2D_FALSE));

    N2D_ON_ERROR(func(hardware, config));

on_error:
    /* Return status. */
    return error;

}

/*******************************************************************************
**
**  n2d_delogo
**
**  Blur a specific area
**
**  INPUT:
**      n2d_buffer_t* src
**          buffer containing image information to be processed
**
**      n2d_rectangle_t logo_rect
**          target processing area
**
**  RETURN:
**      n2d_error_t
**          error type
**
*/
// n2d_error_t  n2d_delogo(n2d_buffer_t* src, n2d_rectangle_t logo_rect)
// {
//     n2d_error_t error = N2D_SUCCESS;
//     n2d_double_t scale_factor = 0.2;
//     n2d_buffer_t buffer = {0};
//     n2d_rectangle_t buffer_rect;

//     buffer.width = (n2d_int32_t)(logo_rect.width * scale_factor);
//     buffer.height = (n2d_int32_t)(logo_rect.height * scale_factor);
//     buffer.format = src->format;
//     buffer.orientation = N2D_0;
//     buffer.tiling = N2D_LINEAR;
//     buffer.tile_status_config = N2D_TSC_DISABLE;

//     N2D_ON_ERROR(n2d_allocate(&buffer));

//     buffer_rect.x = 0;
//     buffer_rect.y = 0;
//     buffer_rect.width = buffer.width;
//     buffer_rect.height = buffer.height;

//     N2D_ON_ERROR(n2d_filterblit(&buffer, &buffer_rect, N2D_NULL, src, &logo_rect, N2D_BLEND_NONE));

//     N2D_ON_ERROR(n2d_filterblit(src, &logo_rect, N2D_NULL, &buffer, &buffer_rect, N2D_BLEND_NONE));

//     N2D_ON_ERROR(n2d_commit());

//     n2d_free(&buffer);

// on_error:
//     return error;
// }

n2d_error_t _export_dmabuf(n2d_uintptr_t handle, n2d_uint32_t flag, n2d_uintptr_t* fd)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_ioctl_interface_t iface = {0};
    gcoTLS tls;

    N2D_ON_ERROR(n2d_user_os_get_tls(&tls));

    if (handle)
    {
        iface.core = tls->engine->currentCoreId;
        iface.command = N2D_KERNEL_COMMAND_EXPORT_VIDMEM;

        iface.u.command_export_vidmem.flags = flag;
        iface.u.command_export_vidmem.handle = handle;

        n2d_get_device_index(&iface.dev_id);

        /* Call the kernel. */
        N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

        *fd = iface.u.command_export_vidmem.fd;
    }

on_error:
    return error;
}

/*******************************************************************************
**
**  n2d_export_dmabuf
**
**  Export dma buf fd
**
**  INPUT:
**      n2d_buffer_t* buf
**          target buffer need to be exported
**
**      n2d_uint32_t flag
**          fd file flag
**
**  OUTPUT:
**      n2d_export_memory_t* fd
**          data structure of fd. include fd, uv fd and ts fd.
**
**  RETURN:
**      n2d_error_t
**          error type
**
*/
n2d_error_t n2d_export_dma_buf(n2d_buffer_t* buf, n2d_uint32_t flag, n2d_export_memory_t* fd)
{
    n2d_error_t error = N2D_SUCCESS;

    N2D_ASSERT(buf != N2D_NULL);

    N2D_ON_ERROR(_export_dmabuf(buf->handle, flag, &fd->fd));
    N2D_ON_ERROR(_export_dmabuf(buf->uv_handle[0], flag, &fd->uv_fd[0]));
    N2D_ON_ERROR(_export_dmabuf(buf->uv_handle[1], flag, &fd->uv_fd[1]));
    N2D_ON_ERROR(_export_dmabuf(buf->tile_status_buffer.handle[0], flag, &fd->ts_fd[0]));
    N2D_ON_ERROR(_export_dmabuf(buf->tile_status_buffer.handle[1], flag, &fd->ts_fd[1]));
    N2D_ON_ERROR(_export_dmabuf(buf->tile_status_buffer.handle[2], flag, &fd->ts_fd[2]));

on_error:
    return error;
}

// #ifdef N2D_SUPPORT_HISTOGRAM
// static n2d_error_t _setCalcHistMode(
//     n2d_buffer_t* src,
//     n2d_int32_t dims,
//     n2d_int32_t* channels,
//     n2d_state_config_t* histogram)
// {
//     n2d_error_t error = N2D_SUCCESS;
//     FormatInfo_t* srcFormatInfo = N2D_NULL;

//     N2D_ON_ERROR(gcGetFormatInfo(src->format, &srcFormatInfo));

//     /*only support RGBA(bpp=32), RGB(bpp=24) and GRAY8 calcHist at present*/
//     if (src->format == N2D_GRAY8)
//     {
//         N2D_ASSERT(dims == 1 && *channels == 0);
//         histogram->config.histogramCalc.mode = N2D_HIST_CALC_GRAY;
//     }
//     else if (srcFormatInfo->formatType == N2D_FORMARTTYPE_RGBA
//         && (srcFormatInfo->bpp == 24 || srcFormatInfo->bpp == 32))
//     {
//         histogram->config.histogramCalc.mode = N2D_HIST_CALC_RGB;
//     }
//     else
//     {
//         N2D_ON_ERROR(N2D_NOT_SUPPORTED);
//     }

// on_error:
//     return error;
// }

// static n2d_void _adjustOutputHistArray(
//     n2d_buffer_format_t format,
//     n2d_int32_t dims,
//     n2d_int32_t* channels,
//     n2d_float_t** ranges,
//     n2d_uint32_t* histSize,
//     n2d_uint32_t* calcHistArray,
//     n2d_uint32_t* outputHistArray)
// {
//     n2d_uint32_t rangeValue = 0;
//     n2d_uint32_t* bCalcHistArray = calcHistArray;
//     n2d_uint32_t* gCalcHistArray = calcHistArray + *histSize;
//     n2d_uint32_t* rCalcHistArray = calcHistArray + *histSize * 2;

//     /*According dims, ranges and channels to adjust output hist array*/
//     while (dims--)
//     {
//         if (format != N2D_GRAY8)
//         {
//             if (*channels == 0)
//             {
//                 calcHistArray = bCalcHistArray;
//             }
//             else if (*channels == 1)
//             {
//                 calcHistArray = gCalcHistArray;
//             }
//             else
//             {
//                 calcHistArray = rCalcHistArray;
//             }
//         }

//         rangeValue = (n2d_uint32_t)(*(*ranges + 1) - **ranges + 1);
//         calcHistArray += (n2d_uint32_t)(**ranges);

//         n2d_user_os_memory_copy(
//             outputHistArray,
//             calcHistArray,
//             rangeValue * N2D_SIZEOF(n2d_uint32_t));

//         if (dims > 0)
//         {
//             outputHistArray += rangeValue;
//             ranges++;
//             channels++;
//         }
//     }
// }
// #endif

/*******************************************************************************
**
**  n2d_calcHist
**
**  Calculation histogram
**
**  INPUT:
**      n2d_buffer_t* src
**          buffer containing image information to be processed
**
**      n2d_int32_t* channels
**          the dims channel used to calculation
**
**      n2d_int32_t  dims
**          the histogram dimension
**
**      n2d_int32_t* histSize
**          histogram bin size
**
**      n2d_float_t** ranges
**           the histogram bin ranges in each dimension
**
**  OUTPUT:
**      n2d_uint32_t* hist
**          output histogram array
**
**  RETURN:
**      n2d_error_t
**          error type
**
*/
// n2d_error_t n2d_calcHist(
//     n2d_buffer_t* src,
//     n2d_int32_t* channels,
//     n2d_uint32_t* hist,
//     n2d_int32_t dims,
//     n2d_uint32_t* histSize,
//     n2d_float_t** ranges)
// {
// #ifdef N2D_SUPPORT_HISTOGRAM
//     n2d_error_t error = N2D_SUCCESS;
//     n2d_state_config_t histogram;
//     n2d_get_state_config_t histArray = {N2D_GET_HISTOGRAM_ARRAY_SIZE, {0}};
//     n2d_buffer_t dst = {0};
//     n2d_uint32_t* calcHistArray = N2D_NULL;
//     n2d_uint32_t* outputHistArray = hist;
//     n2d_uint32_t index = 0;

//     N2D_ASSERT(dims <= 3 && dims >= 0);
//     while (dims--)
//     {
//         /*check the range of ranges and channels*/
//         N2D_ASSERT((*(*ranges + 1) > **ranges) && **ranges >= 0
//             && (n2d_uint32_t)(*(*ranges + 1) < *histSize));
//         N2D_ASSERT(*channels >= 0 && *channels <= 2);

//         if (dims > 0)
//         {
//             ranges++;
//             channels++;
//         }
//         index++;
//     }

//     /*recovery the parameters*/
//     ranges -= (index - 1);
//     channels -= (index - 1);
//     dims += (index + 1);

//     dst.width = src->alignedw;
//     dst.height = src->alignedh;
//     dst.format = N2D_RGB888;
//     dst.orientation = N2D_0;
//     dst.tiling = N2D_LINEAR;
//     dst.tile_status_config = N2D_TSC_DISABLE;
//     dst.cacheMode = N2D_CACHE_128;

//     N2D_ON_ERROR(n2d_allocate(&dst));

//     memset(&histogram, 0x0, sizeof(n2d_state_config_t));
//     histogram.state = N2D_SET_HISTOGRAM_CALC;
//     histogram.config.histogramCalc.enable = N2D_TRUE;
//     histogram.config.histogramCalc.memMode = N2D_PLANAR;
//     histogram.config.histogramCalc.histSize = *histSize;

//     N2D_ON_ERROR(_setCalcHistMode(src, dims, channels, &histogram));
//     N2D_ON_ERROR(n2d_set(&histogram));
//     N2D_ON_ERROR(n2d_blit(&dst, N2D_NULL, src, N2D_NULL, N2D_BLEND_NONE));
//     N2D_ON_ERROR(n2d_commit());

//     histArray.state = N2D_GET_HISTOGRAM_ARRAY_SIZE;
//     n2d_get(&histArray);

//     histArray.config.histogramArray.size = histArray.config.histogramArraySize;
//     histArray.config.histogramArray.array =
//         (n2d_uint32_t*)malloc(histArray.config.histogramArray.size * sizeof(n2d_uint32_t));
//     if (histArray.config.histogramArray.array)
//     {
//         memset(histArray.config.histogramArray.array,
//             0x0,
//             histArray.config.histogramArray.size * sizeof(n2d_uint32_t));
//     }

//     calcHistArray = histArray.config.histogramArray.array;
//     histArray.state = N2D_GET_HISTOGRAM_ARRAY;
//     N2D_ON_ERROR(n2d_get(&histArray));

//     _adjustOutputHistArray(src->format, dims, channels,
//         ranges, histSize, calcHistArray, outputHistArray);

// on_error:
//     if (histArray.config.histogramArray.array != N2D_NULL)
//     {
//         free(histArray.config.histogramArray.array);
//         histArray.config.histogramArray.array = N2D_NULL;
//     }
//     n2d_free(&dst);
//     return error;
// #else
//     NANO2D_PRINT("Not support histogram calculation!!!\n");
//     return N2D_NOT_SUPPORTED;
// #endif
// }

/*******************************************************************************
**
**  n2d_equalizeHist
**
**  Equalize histogram
**
**  INPUT:
**      n2d_buffer_t* src
**          buffer containing image information to be processed
**
**  OUTPUT:
**      n2d_buffer_t* dst
**          output result
**
**  RETURN:
**      n2d_error_t
**          error type
**
*/
n2d_error_t n2d_equalizeHist(n2d_buffer_t* src, n2d_buffer_t* dst)
{
#ifdef N2D_SUPPORT_HISTOGRAM
    n2d_error_t error = N2D_SUCCESS;
    n2d_state_config_t histogram;
    FormatInfo_t* srcFormatInfo = N2D_NULL;
    FormatInfo_t* dstFormatInfo = N2D_NULL;

    N2D_ON_ERROR(gcGetFormatInfo(src->format, &srcFormatInfo));
    N2D_ON_ERROR(gcGetFormatInfo(dst->format, &dstFormatInfo));

    if (srcFormatInfo->formatType == dstFormatInfo->formatType)
    {
        /*only support RGBA(bpp=32), RGB(bpp=24) and GRAY8 equalizeHist at present*/
        if (!((src->format == N2D_GRAY8 && dst->format == N2D_GRAY8)
            || (srcFormatInfo->formatType == N2D_FORMARTTYPE_RGBA
                && (srcFormatInfo->bpp == 24 || srcFormatInfo->bpp == 32)
                && (dstFormatInfo->bpp == 24 || dstFormatInfo->bpp == 32))))
        {
            N2D_ON_ERROR(N2D_NOT_SUPPORTED);
        }
    }
    else
    {
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    histogram.state = N2D_SET_HISTOGRAM_EQUAL;
    histogram.config.histogramEqual.enable = N2D_TRUE;
    histogram.config.histogramEqual.mode = (dstFormatInfo->formatType == N2D_FORMARTTYPE_YUV) ?
        N2D_HIST_EQUAL_GRAY : N2D_HIST_EQUAL_RGB;
    histogram.config.histogramEqual.option = N2D_HIST_EQUAL_MAPPING_TABLE;

    N2D_ON_ERROR(n2d_set(&histogram));
    N2D_ON_ERROR(n2d_blit(dst, N2D_NULL, src, N2D_NULL, N2D_BLEND_NONE));

    histogram.config.histogramEqual.option = N2D_HIST_EQUAL_EQUALIZATION;

    N2D_ON_ERROR(n2d_set(&histogram));
    N2D_ON_ERROR(n2d_blit(dst, N2D_NULL, src, N2D_NULL, N2D_BLEND_NONE));
    N2D_ON_ERROR(n2d_commit());

on_error:
    return error;
#else
    NANO2D_PRINT("Not support histogram equalization!!!\n");
    return N2D_NOT_SUPPORTED;
#endif
}
