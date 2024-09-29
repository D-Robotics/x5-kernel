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

#ifndef _nano2D_user_hardware_h_
#define _nano2D_user_hardware_h_

#include "nano2D_option.h"
#include "nano2D_base.h"
#include "nano2D_dispatch.h"
#include "nano2D_user_buffer.h"
#include "nano2D_debug.h"
#include "nano2D_enum.h"

#define SPLIT_COLUMN            4
#define SPLIT_COLUMN_WIDTH      (1 << SPLIT_COLUMN)
#define SPLIT_COLUMN_WIDTH_MASK (~(SPLIT_COLUMN_WIDTH - 1))

#define gcdMULTI_SOURCE_NUM 8

#define N2D_BITSPERBYTE 8

#define MAX_LOOP_COUNT 0x7FFFFFFF

#define N2D_MAX_WIDTH  0xFFFF
#define N2D_MAX_HEIGHT 0xFFFF

#define N2D_MAX_CONTEXTID_COUNT     6
#define N2D_MAX_64BIT_ADDRESS_COUNT 10

#define gcvMAXTAPSIZE           9
#define gcvMAXTAPSIZE_OPF       5
#define gcvSUBPIXELINDEXBITS    5

#define gcvSUBPIXELCOUNT \
    (1 << gcvSUBPIXELINDEXBITS)

#define gcvSUBPIXELLOADCOUNT \
    (gcvSUBPIXELCOUNT / 2 + 1)

#define gcvWEIGHTSTATECOUNT \
    (((gcvSUBPIXELLOADCOUNT * gcvMAXTAPSIZE + 1) & ~1) / 2)

#define gcvKERNELTABLESIZE \
    (gcvSUBPIXELLOADCOUNT * gcvMAXTAPSIZE * sizeof(n2d_uint16_t))

#define gcvKERNELSTATES \
    (gcmALIGN(gcvKERNELTABLESIZE + 4, 8))

#define gcmGET_PRE_ROTATION(rotate) \
    (rotate)

typedef struct _gcsRECT * gcsRECT_PTR;


typedef enum _gce2D_SUPER_TILE_VERSION
{
    gcv2D_SUPER_TILE_VERSION_V1       = 1,
    gcv2D_SUPER_TILE_VERSION_V2       = 2,
    gcv2D_SUPER_TILE_VERSION_V3       = 3,
}
gce2D_SUPER_TILE_VERSION;

typedef enum _gce2D_PATTERN
{
    gcv2D_PATTERN_SOLID = 0,
    gcv2D_PATTERN_MONO,
    gcv2D_PATTERN_COLOR,
    gcv2D_PATTERN_INVALID
}
gce2D_PATTERN;

// typedef enum _gce2D_TRANSPARENCY
// {
//     gcv2D_OPAQUE = 0,
//     gcv2D_KEYED,
//     gcv2D_MASKED
// }
// gce2D_TRANSPARENCY;

typedef enum _gceSURF_PIXEL_ALPHA_MODE
{
    gcvSURF_PIXEL_ALPHA_STRAIGHT = 0,
    gcvSURF_PIXEL_ALPHA_INVERSED
}
gceSURF_PIXEL_ALPHA_MODE;

typedef enum _gceSURF_GLOBAL_ALPHA_MODE
{
    gcvSURF_GLOBAL_ALPHA_OFF = 0,
    gcvSURF_GLOBAL_ALPHA_ON,
    gcvSURF_GLOBAL_ALPHA_SCALE
}
gceSURF_GLOBAL_ALPHA_MODE;

typedef enum _gceSURF_PIXEL_COLOR_MODE
{
    gcvSURF_COLOR_STRAIGHT = 0,
    gcvSURF_COLOR_MULTIPLY
}
gceSURF_PIXEL_COLOR_MODE;

// typedef enum _gce2D_PIXEL_COLOR_MULTIPLY_MODE
// {
//     gcv2D_COLOR_MULTIPLY_DISABLE = 0,
//     gcv2D_COLOR_MULTIPLY_ENABLE
// }
// gce2D_PIXEL_COLOR_MULTIPLY_MODE;

// typedef enum _gce2D_GLOBAL_COLOR_MULTIPLY_MODE
// {
//     gcv2D_GLOBAL_COLOR_MULTIPLY_DISABLE = 0,
//     gcv2D_GLOBAL_COLOR_MULTIPLY_ALPHA,
//     gcv2D_GLOBAL_COLOR_MULTIPLY_COLOR
// }
// gce2D_GLOBAL_COLOR_MULTIPLY_MODE;

typedef enum _gceSURF_BLEND_FACTOR_MODE
{
    gcvSURF_BLEND_ZERO = 0,
    gcvSURF_BLEND_ONE,
    gcvSURF_BLEND_STRAIGHT,
    gcvSURF_BLEND_INVERSED,
    gcvSURF_BLEND_COLOR,
    gcvSURF_BLEND_COLOR_INVERSED,
    gcvSURF_BLEND_SRC_ALPHA_SATURATED,
    gcvSURF_BLEND_STRAIGHT_NO_CROSS,
    gcvSURF_BLEND_INVERSED_NO_CROSS,
    gcvSURF_BLEND_COLOR_NO_CROSS,
    gcvSURF_BLEND_COLOR_INVERSED_NO_CROSS,
    gcvSURF_BLEND_SRC_ALPHA_SATURATED_CROSS
}
gceSURF_BLEND_FACTOR_MODE;

typedef enum n2d_monopack
{
    N2D_PACKED8 = 0,
    N2D_PACKED16,
    N2D_PACKED32,
    N2D_UNPACKED,
}
n2d_monopack_t;

typedef enum n2d_formattype
{
    N2D_FORMARTTYPE_RGBA = 0,
    N2D_FORMARTTYPE_YUV,
    N2D_FORMARTTYPE_INDEX,
    N2D_FORMARTTYPE_FLOAT,
}
n2d_formattype_t;

typedef enum n2d_planenum
{
   N2D_ONEPLANE      = 1,
   N2D_TWOPLANES     = 2,
   N2D_THREEPLANES   = 3,
}
n2d_planenum_t;

typedef enum n2d_plane
{
   N2D_FIRSTPLANE       = 0,
   N2D_SECONDPLANES     = 1,
   N2D_THRIDPLANES      = 2,
}
n2d_plane_t;

typedef struct n2d_hw_cmdbuf {
    n2d_uint32_t  * logical;
    n2d_uint32_t    gpu;
    n2d_uint32_t    count;
    n2d_uint32_t    index;
}n2d_hw_cmdbuf_t;

typedef struct _gcsRECT {
    n2d_int32_t left;
    n2d_int32_t top;
    n2d_int32_t right;
    n2d_int32_t bottom;
}gcsRECT;

typedef struct n2d_buffer_node {
    void            *memory;
    n2d_uintptr_t   physical;
    n2d_uint32_t    gpu;
    n2d_uint32_t    size;
}n2d_buffer_node_t;

typedef enum _gceFILTER_TYPE {
    gcvFILTER_SYNC = 0,
    gcvFILTER_BLUR,
    gcvFILTER_USER,
    gcvFILTER_BILINEAR,
    gcvFILTER_BICUBIC
}gceFILTER_TYPE;

typedef enum _gceFILTER_PASS_TYPE {
    gcvFILTER_HOR_PASS = 0,
    gcvFILTER_VER_PASS
}gceFILTER_PASS_TYPE;

typedef enum {
    gceFILTER_BLIT_TYPE_VERTICAL,
    gceFILTER_BLIT_TYPE_HORIZONTAL,
    gceFILTER_BLIT_TYPE_ONE_PASS,
}gceFILTER_BLIT_TYPE;

typedef enum {
    gcvFILTER_BLIT_KERNEL_UNIFIED,
    gcvFILTER_BLIT_KERNEL_VERTICAL,
    gcvFILTER_BLIT_KERNEL_HORIZONTAL,
    gcvFILTER_BLIT_KERNEL_NUM,
}gceFILTER_BLIT_KERNEL;

typedef struct _gcsFILTER_BLIT_ARRAY {
    gceFILTER_TYPE              filterType;
    n2d_uint8_t                 kernelSize;
    n2d_uint32_t                scaleFactor;
    n2d_bool_t                  kernelChanged;
    n2d_uint32_t              * kernelStates;
}gcsFILTER_BLIT_ARRAY;
typedef gcsFILTER_BLIT_ARRAY *  gcsFILTER_BLIT_ARRAY_PTR;

typedef struct _gcsOPF_BLOCKSIZE_TABLE {
    n2d_uint8_t width;
    n2d_uint8_t height;
    n2d_uint8_t blockDirect;
    n2d_uint8_t pixelDirect;
    n2d_uint16_t horizontal;
    n2d_uint16_t vertical;
}gcsOPF_BLOCKSIZE_TABLE;
typedef gcsOPF_BLOCKSIZE_TABLE *  gcsOPF_BLOCKSIZE_TABLE_PTR;

typedef struct _gcsSURF_INFO {
    n2d_buffer_format_t     format;
    n2d_orientation_t       rotation;
    gcsRECT                 rect;
    n2d_uint32_t            alignedWidth;
    n2d_uint32_t            alignedHeight;
    n2d_tiling_t            tiling;
    n2d_cache_mode_t        cacheMode;
    n2d_pointer             memory;
    n2d_pointer             uv_memory[MAX_UV_PLANE];
    n2d_tile_status_config_t    tile_status_config;
    n2d_uint32_t                gpuAddress[MAX_PLANE];
    n2d_uint32_t                stride[MAX_PLANE];
    n2d_tile_status_buffer_t    tile_status_buffer;

#ifdef N2D_SUPPORT_HISTOGRAM
    n2d_histogram_calc_buffer_t histogram_buffer[MAX_PLANE];
#endif

#ifdef N2D_SUPPORT_64BIT_ADDRESS
    n2d_uintptr_t            gpuAddrEx;
    n2d_uintptr_t            ts_gpuAddrEx;
#endif
}gcsSURF_INFO;
typedef struct _gcsSURF_INFO *          gcsSURF_INFO_PTR;

typedef struct _gcs2D_MULTI_SOURCE {
    n2d_source_type_t       srcType;
    gcsSURF_INFO            srcSurface;
    n2d_monopack_t          srcMonoPack;
    n2d_maskpack_config_t   maskPackConfig;
    n2d_uint32_t srcMonoTransparencyColor;
    n2d_bool_t   srcColorConvert;
    n2d_uint32_t srcFgColor;
    n2d_uint32_t srcBgColor;
    n2d_uint32_t srcColorKeyLow;
    n2d_uint32_t srcColorKeyHigh;
    n2d_bool_t      srcRelativeCoord;
    n2d_bool_t      srcStream;
    gcsRECT         srcRect;
    n2d_csc_mode_t srcCSCMode;

    n2d_transparency_t srcTransparency;
    n2d_transparency_t dstTransparency;
    n2d_transparency_t patTransparency;

    n2d_bool_t enableDFBColorKeyMode;

    n2d_uint8_t fgRop;
    n2d_uint8_t bgRop;

    n2d_bool_t enableAlpha;
    gceSURF_PIXEL_ALPHA_MODE  srcAlphaMode;
    gceSURF_PIXEL_ALPHA_MODE  dstAlphaMode;
    gceSURF_GLOBAL_ALPHA_MODE srcGlobalAlphaMode;
    gceSURF_GLOBAL_ALPHA_MODE dstGlobalAlphaMode;
    gceSURF_BLEND_FACTOR_MODE srcFactorMode;
    gceSURF_BLEND_FACTOR_MODE dstFactorMode;
    gceSURF_PIXEL_COLOR_MODE  srcColorMode;
    gceSURF_PIXEL_COLOR_MODE  dstColorMode;
    n2d_pixel_color_multiply_mode_t srcPremultiplyMode;
    n2d_pixel_color_multiply_mode_t dstPremultiplyMode;
    n2d_global_color_multiply_mode_t srcPremultiplyGlobalMode;
    n2d_pixel_color_multiply_mode_t dstDemultiplyMode;
    n2d_uint32_t srcGlobalColor;
    n2d_uint32_t dstGlobalColor;

    gcsRECT     clipRect;
    gcsRECT     dstRect;

    n2d_bool_t     enableGDIStretch;
    n2d_uint32_t   horFactor;
    n2d_uint32_t   verFactor;

    n2d_bool_t horMirror;
    n2d_bool_t verMirror;

} gcs2D_MULTI_SOURCE, *gcs2D_MULTI_SOURCE_PTR;

typedef struct _gcs2D_DEST
{
    gcsSURF_INFO                   dstSurface;

    n2d_dither_info_t              ditherInfo;

    n2d_normalization_info_t       normalizationInfo;

    n2d_u8_to_u10_conversion_mode_t u8Tu10_cvtMode;

#ifdef N2D_SUPPORT_HISTOGRAM
    n2d_histogram_calc_config_t    histogramCalcInfo;
    n2d_histogram_equal_config_t   histogramEqualInfo;
#endif

#ifdef N2D_SUPPORT_BRIGHTNESS
    n2d_brightness_saturation_config_t brightnessAndSaturationInfo;
#endif
} gcs2D_DEST, *gcs2D_DEST_PTR;

typedef struct _gcsFilterBlitState
{
    gceFILTER_TYPE          newFilterType;
    n2d_uint8_t             newHorKernelSize;
    n2d_uint8_t             newVerKernelSize;
    n2d_bool_t              horUserFilterPass;
    n2d_bool_t              verUserFilterPass;
    gcsFILTER_BLIT_ARRAY    horSyncFilterKernel;
    gcsFILTER_BLIT_ARRAY    verSyncFilterKernel;
    gcsFILTER_BLIT_ARRAY    horBlurFilterKernel;
    gcsFILTER_BLIT_ARRAY    verBlurFilterKernel;
    gcsFILTER_BLIT_ARRAY    horUserFilterKernel;
    gcsFILTER_BLIT_ARRAY    verUserFilterKernel;
    gcsFILTER_BLIT_ARRAY    horBilinearFilterKernel;
    gcsFILTER_BLIT_ARRAY    verBilinearFilterKernel;
    gcsFILTER_BLIT_ARRAY    horBicubicFilterKernel;
    gcsFILTER_BLIT_ARRAY    verBicubicFilterKernel;
} gcsFilterBlitState;

typedef struct _n2d_state {
    gcs2D_DEST          dest;
    gcs2D_MULTI_SOURCE  multiSrc[gcdMULTI_SOURCE_NUM];

    n2d_uint32_t        dstColorKeyLow;
    n2d_uint32_t        dstColorKeyHigh;

    n2d_uint32_t        currentSrcIndex;

    n2d_uint32_t        srcMask;

    n2d_color_t         clearColor;

    gcsRECT             dstClipRect;

    n2d_csc_mode_t      dstCSCMode;
    n2d_int32_t         cscYUV2RGB[N2D_CSC_PROGRAMMABLE_SIZE];
    n2d_int32_t         cscRGB2YUV[N2D_CSC_PROGRAMMABLE_SIZE];

    gce2D_PATTERN       brushType;
    n2d_uint32_t        brushOriginX;
    n2d_uint32_t        brushOriginY;
    n2d_uint32_t        brushColorConvert;
    n2d_uint32_t        brushFgColor;
    n2d_uint32_t        brushBgColor;
    n2d_uint64_t        brushBits;
    n2d_uint64_t        brushMask;
    n2d_uint32_t        brushAddress;
    n2d_buffer_format_t brushFormat;

    n2d_uint32_t        paletteIndexCount;
    n2d_uint32_t        paletteFirstIndex;
    n2d_bool_t          paletteConvert;
    n2d_bool_t          paletteProgram;
    void              * paletteTable;
    n2d_buffer_format_t paletteConvertFormat;

    gcsFilterBlitState  filterState;

    n2d_bool_t          enableXRGB;

    n2d_bool_t          grayYUV2RGB;
    n2d_bool_t          grayRGB2YUV;

    n2d_fastclear_config_t fastclear;
}
n2d_state_t, gcs2D_State, *gcs2D_State_PTR;

typedef enum _gce2D_COMMAND {
    gcv2D_CLEAR = 0,
    gcv2D_LINE,
    gcv2D_BLT,
    gcv2D_STRETCH,
    gcv2D_HOR_FILTER,
    gcv2D_VER_FILTER,
    gcv2D_MULTI_SOURCE_BLT,
    gcv2D_FILTER_BLT,
}
gce2D_COMMAND;

typedef enum _SPLIT_RECT_MODE {
    SPLIT_RECT_MODE_NONE,
    SPLIT_RECT_MODE_COLUMN,
    SPLIT_RECT_MODE_LINE
} SPLIT_RECT_MODE;

typedef struct _n2d_command_buffer {
    n2d_uint32_t    *memory;
    n2d_uint32_t    gpu;
    n2d_uintptr_t   physical;
    n2d_uint32_t    index;
    n2d_uint32_t    count;
    n2d_int32_t     eventId;
    n2d_uint32_t    handle;
}
n2d_command_buffer;

typedef struct _n2d_secure_buffer {
    n2d_bool_t      isSecureBuffer;
    n2d_uint64_t    address;
}
n2d_secure_buffer_t;

typedef struct _n2d_user_hardware {
    n2d_uint32_t        chipModel;
    n2d_uint32_t        chipRevision;
    n2d_uint32_t        productId;
    n2d_uint32_t        cid;
    n2d_uint32_t        chipFeatures;
    n2d_uint32_t        chipMinorFeatures;
    n2d_uint32_t        chipMinorFeatures1;
    n2d_uint32_t        chipMinorFeatures2;
    n2d_uint32_t        chipMinorFeatures3;
    n2d_uint32_t        chipMinorFeatures4;

    n2d_uint32_t        coreIndex;

    n2d_uint32_t        hw2DEngine;
    n2d_uint32_t        sw2DEngine;

    n2d_state_t         state;
    n2d_transparency_t  transparency;
    n2d_color_t         color;
    n2d_uint8_t         fgrop;
    n2d_uint8_t         bgrop;

    n2d_uint32_t        hw2DMultiSrcV2;

    n2d_uint32_t        chip2DControl;
    n2d_uint32_t        hw2DSplitRect;

    n2d_uint32_t        hw2DAppendCacheFlush;
    n2d_buffer_t        hw2DCacheFlushBuffer;

    n2d_orientation_t   srcRot;
    n2d_orientation_t   dstRot;
    int                 horMirror;
    int                 verMirror;

    n2d_bool_t          srcRelated;

    struct __gcsLOADED_KERNEL_INFO
    {
        gceFILTER_TYPE              type;
        n2d_uint8_t                 kernelSize;
        n2d_uint32_t                scaleFactor;
        n2d_uint32_t                kernelAddress;
    } loadedKernel[gcvFILTER_BLIT_KERNEL_NUM];

    n2d_user_buffer_t               buffer;

    n2d_pointer                     cmdLogical;
    n2d_uint32_t                    cmdCount;
    n2d_uint32_t                    cmdIndex;

    n2d_bool_t                     enableXRGB;
    n2d_bool_t                     notAdjustRotation;

    n2d_bool_t                     features[N2D_FEATURE_COUNT];
    n2d_bool_t                     hw2DCurrentRenderCompressed;
    n2d_bool_t                     hw2DBlockSize;
    n2d_bool_t                     hw2DQuad;
    n2d_uint32_t                   contextID[N2D_MAX_CONTEXTID_COUNT];
    n2d_secure_buffer_t*           secureBuffer;

#if NANO2D_DUMP
    long                            g_line;
    FILE*                           fp;
#endif

    n2d_bool_t                     isStretchMultisrcStretchBlit;
}n2d_user_hardware;

#define N2D_DestroyKernelArray(KernelInfo)                                     \
    if (N2D_NULL != KernelInfo.kernelStates )                                  \
    {                                                                          \
        if(n2d_user_os_free(KernelInfo.kernelStates))                          \
        {                                                                      \
            gcTrace("Failed to free kernel table.!\n");                      \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            KernelInfo.kernelStates = N2D_NULL;                                \
        }                                                                      \
                                                                               \
        KernelInfo.kernelStates = N2D_NULL;                                    \
    }

#define N2D_FreeKernelArray(State)                                             \
        N2D_DestroyKernelArray(State.horSyncFilterKernel)                      \
        N2D_DestroyKernelArray(State.verSyncFilterKernel)                      \
        N2D_DestroyKernelArray(State.horBlurFilterKernel)                      \
        N2D_DestroyKernelArray(State.verBlurFilterKernel)                      \
        N2D_DestroyKernelArray(State.horUserFilterKernel)                      \
        N2D_DestroyKernelArray(State.verUserFilterKernel)                      \
        N2D_DestroyKernelArray(State.horBilinearFilterKernel)                  \
        N2D_DestroyKernelArray(State.verBilinearFilterKernel)                  \
        N2D_DestroyKernelArray(State.horBicubicFilterKernel)                   \
        N2D_DestroyKernelArray(State.verBicubicFilterKernel)                   \

#define gcmHASCOMPRESSION(Surface) \
    ( \
        ((Surface)->tile_status_config & N2D_TSC_DEC_COMPRESSED) \
    )


n2d_uint32_t
gcGetMaxDataCount(
    void
    );

n2d_error_t
gcWriteRegWithLoadState(
    n2d_user_hardware_t * Hardware,
    n2d_uint32_t address,
    n2d_uint32_t count,
    n2d_pointer data
    );

n2d_error_t
gcUploadCSCProgrammable(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_bool_t YUV2RGB,
    IN n2d_int32_t * Table
    );

n2d_error_t
gcClearRectangle(
    IN n2d_user_hardware_t * Hardware,
    IN gcs2D_State_PTR State,
    IN n2d_uint32_t RectCount,
    IN gcsRECT_PTR Rect);

n2d_error_t
gcWriteRegWithLoadState(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t Address,
    IN n2d_uint32_t Count,
    n2d_pointer Data
    );

n2d_error_t
gcWriteRegWithLoadState32(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t Address,
    IN n2d_uint32_t Data
    );

n2d_error_t
gcoHARDWARE_SetDither2D(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_bool_t Enable
    );

void gcGetResourceUsage(
    IN n2d_uint8_t FgRop,
    IN n2d_uint8_t BgRop,
    IN n2d_transparency_t SrcTransparency,
    OUT n2d_bool_t * UseSource,
    OUT n2d_bool_t * UsePattern,
    OUT n2d_bool_t * UseDestination
    );

n2d_error_t gcTranslateCommand(
    IN gce2D_COMMAND APIValue,
    OUT n2d_uint32_t * HwValue
    );

n2d_error_t gcSetMultiplyModes(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_pixel_color_multiply_mode_t SrcPremultiplySrcAlpha,
    IN n2d_pixel_color_multiply_mode_t DstPremultiplyDstAlpha,
    IN n2d_global_color_multiply_mode_t SrcPremultiplyGlobalMode,
    IN n2d_pixel_color_multiply_mode_t DstDemultiplyDstAlpha,
    IN n2d_uint32_t SrcGlobalColor
    );

n2d_error_t gcDisableAlphaBlend(
    IN n2d_user_hardware_t * Hardware
    );

n2d_error_t gcSetStretchFactors(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_uint32_t HorFactor,
    IN n2d_uint32_t VerFactor
    );

n2d_uint32_t gcGetStretchFactor(
    IN n2d_bool_t GdiStretch,
    IN n2d_int32_t SrcSize,
    IN n2d_int32_t DestSize
    );

n2d_error_t
gcSetSuperTileVersion(
    IN n2d_user_hardware_t * Hardware,
    IN gce2D_SUPER_TILE_VERSION Version
    );

n2d_error_t gcTranslateGlobalColorMultiplyMode(
    IN  n2d_global_color_multiply_mode_t APIValue,
    OUT n2d_uint32_t * HwValue
    );

n2d_error_t gcTranslatePixelColorMultiplyMode(
    IN  n2d_pixel_color_multiply_mode_t APIValue,
    OUT n2d_uint32_t * HwValue
    );

n2d_error_t gcTranslatePixelColorMode(
    IN gceSURF_PIXEL_COLOR_MODE APIValue,
    OUT n2d_uint32_t* HwValue
    );

n2d_error_t gcTranslateAlphaFactorMode(
    IN  n2d_bool_t                   IsSrcFactor,
    IN  gceSURF_BLEND_FACTOR_MODE APIValue,
    OUT n2d_uint32_t*                HwValue,
    OUT n2d_uint32_t*                FactorExpansion
    );

n2d_error_t gcTranslateGlobalAlphaMode(
    IN gceSURF_GLOBAL_ALPHA_MODE APIValue,
    OUT n2d_uint32_t* HwValue
    );

n2d_error_t gcTranslatePixelAlphaMode(
    IN gceSURF_PIXEL_ALPHA_MODE APIValue,
    OUT n2d_uint32_t* HwValue
    );

n2d_error_t gcConvertFormat(
    IN n2d_buffer_format_t Format,
    OUT n2d_uint32_t* BitsPerPixel
    );

n2d_error_t gcQueryFBPPs(
    IN n2d_buffer_format_t Format,
    OUT n2d_uint32_t* BppArray
    );

n2d_error_t gcTranslatePatternTransparency(
    IN n2d_transparency_t APIValue,
    OUT n2d_uint32_t * HwValue
    );

n2d_error_t gcColorConvertToARGB8(
    IN n2d_buffer_format_t Format,
    IN n2d_uint32_t NumColors,
    IN n2d_uint32_t*   Color,
    OUT n2d_uint32_t*   Color32
    );

// n2d_error_t gcFilterBlit(
//     IN n2d_user_hardware_t * Hardware,
//     IN gcs2D_State_PTR State,
//     IN gcsSURF_INFO_PTR SrcSurface,
//     IN gcsSURF_INFO_PTR DestSurface,
//     IN gcsRECT_PTR SrcRect,
//     IN gcsRECT_PTR DestRect,
//     IN gcsRECT_PTR DestSubRect
//     );

n2d_error_t gcRenderEnd(
    IN n2d_user_hardware_t * Hardware,
    IN n2d_bool_t SourceFlush
    );

n2d_error_t gcStartDE(
    IN n2d_user_hardware_t * Hardware,
    IN gcs2D_State_PTR State,
    IN gce2D_COMMAND Command,
    IN n2d_uint32_t SrcRectCount,
    IN gcsRECT_PTR SrcRect,
    IN n2d_uint32_t DestRectCount,
    IN gcsRECT_PTR DestRect
    );

n2d_error_t gcStartDELine(
    IN n2d_user_hardware_t * Hardware,
    IN gcs2D_State_PTR State,
    IN gce2D_COMMAND Command,
    IN n2d_uint32_t LineCount,
    IN gcsRECT_PTR DestRect,
    IN n2d_uint32_t ColorCount,
    IN n2d_uint32_t * Color32
    );

n2d_error_t gcStartDEStream(
    IN n2d_user_hardware_t *Hardware,
    IN gcs2D_State_PTR      State,
    IN gcsRECT_PTR          DestRect,
    IN n2d_uint32_t         StreamSize,
    OUT n2d_pointer         *StreamBits
    );

n2d_error_t
gcInitHardware(
    n2d_user_hardware_t * hardware
    );

n2d_error_t
gcDeinitHardware(
    n2d_user_hardware_t * hardware
    );

n2d_bool_t
gcTranslateXRGBFormat(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_buffer_format_t InputFormat,
    OUT n2d_buffer_format_t* OutputFormat
    );

n2d_error_t
gcGetSurfaceBufferSize(
    IN n2d_user_hardware_t      *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_bool_t       only_getsize,
    OUT n2d_uint32_t    *offset,
    OUT n2d_uint32_t    *SizeArray
    );

n2d_error_t
gcGetSurfaceTileStatusBufferSize(
    IN gcsSURF_INFO_PTR Surface,
    OUT n2d_uint32_t *tssize
    );

n2d_error_t
gcAdjustSurfaceBufferParameters(
    IN n2d_user_hardware_t   *Hardware,
    n2d_buffer_t    *buffer
    );

n2d_error_t
gcAllocSurfaceBuffer(
    n2d_user_hardware_t  *hardware,
    n2d_buffer_t    *buffer
    );

n2d_error_t
gcAllocSurfaceTileStatusBuffer(
    n2d_user_hardware_t  *hardware,
    n2d_buffer_t    *buffer
    );

n2d_error_t
gcConvertBufferToSurfaceBuffer(
    gcsSURF_INFO *dst,
    n2d_buffer_t *src
    );

void
gcTrace(const char *format,
    ...
    );

n2d_error_t
gcAllocateGpuMemory(
    IN n2d_user_hardware_t  *Hardware,
    IN n2d_size_t      Size,
    IN n2d_bool_t      forCmdBuf,
    OUT n2d_ioctl_interface_t    *Iface
    );

n2d_error_t
gcMapMemory(
    IN n2d_user_hardware_t  *Hardware,
    IN n2d_uintptr_t     Handle,
    OUT n2d_ioctl_interface_t    *Iface
    );

n2d_error_t
gcUnmapMemory(
    IN n2d_user_hardware_t  *Hardware,
    IN n2d_uintptr_t    Handle
    );

n2d_error_t
gcFreeGpuMemory(
    IN n2d_user_hardware_t  *Hardware,
    IN n2d_uintptr_t    Handle
    );

n2d_error_t
gcSubcommitCmd(
    IN n2d_user_hardware_t  *hardware,
    IN n2d_user_subcommit_t *subcommit
    );

n2d_error_t
gcCommitEvent(
    IN n2d_user_hardware_t  *Hardware,
    IN n2d_uintptr_t         Queue
    );

n2d_error_t
gcCommitSignal(
    IN n2d_user_hardware_t* Hardware);

n2d_error_t
gcWaitSignal(
    IN n2d_user_hardware_t  *Hardware,
    IN n2d_pointer          Stall_signal,
    IN n2d_uint32_t         Wait_time
    );

n2d_error_t
n2d_user_os_signal_create(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_bool_t ManualReset,
    OUT n2d_pointer *Signal
    );

n2d_error_t
n2d_user_os_signal_destroy(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_pointer Signal
    );

n2d_error_t
n2d_user_os_signal_signal(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_pointer Signal,
    IN n2d_bool_t State
    );

n2d_error_t
n2d_user_os_signal_wait(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_pointer Signal,
    IN n2d_uint32_t Wait
    );

#endif
