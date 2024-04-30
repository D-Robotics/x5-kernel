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

#include "chip.h"
#include "common.h"
#include "series_common.h"

struct SeriesDataFuncs gSeriesDataFuncs;

n2d_void gcInitNormalizationState(n2d_state_t* state)
{
    state->dest.normalizationInfo.enable = N2D_FALSE;
    state->dest.normalizationInfo.meanValue.r = 0x42f80000;
    state->dest.normalizationInfo.meanValue.g = 0x42e80000;
    state->dest.normalizationInfo.meanValue.b = 0x42d00000;
    state->dest.normalizationInfo.stdReciprocal.r = 0x3c8d3dcb;
    state->dest.normalizationInfo.stdReciprocal.g = 0x3c8fb823;
    state->dest.normalizationInfo.stdReciprocal.b = 0x3c8fb823;
    state->dest.normalizationInfo.stepReciprocal = 0;
    state->dest.normalizationInfo.maxMinReciprocal.r = 0x3B808080;
    state->dest.normalizationInfo.maxMinReciprocal.g = 0x3B808080;
    state->dest.normalizationInfo.maxMinReciprocal.b = 0x3B808080;
    state->dest.normalizationInfo.minValue.r = 0;
    state->dest.normalizationInfo.minValue.g = 0;
    state->dest.normalizationInfo.minValue.b = 0;
    return;
}

n2d_error_t gcSetNormalizationState(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_uint32_t *data,
    IN n2d_uint32_t *dataEx
    )
{
    n2d_error_t error = N2D_SUCCESS;

    if (gSeriesDataFuncs.gcSetNormalizationState)
    {
        error = gSeriesDataFuncs.gcSetNormalizationState(Hardware,Surface,data,dataEx);
    }

    return error;
}

#ifdef NANO2D_PATCH_NORMALIZATION_GARY_FORMAT
n2d_void gcNormalizationGrayFormat(
    IN OUT n2d_uint32_t* dataEx)
{
    if (gcHandleNormalizationGrayFormat)
    {
        gcHandleNormalizationGrayFormat(dataEx);
    }
}
#endif

/*** Normalization ***/
n2d_buffer_format_t normalizationFormat[] = {
    N2D_RGB888,
    N2D_RGB888_PLANAR,
    N2D_RGB888I,
    N2D_RGB888I_PLANAR,
    N2D_RGB161616I,
    N2D_RGB161616I_PLANAR,
    N2D_RGB161616F,
    N2D_RGB161616F_PLANAR,
    N2D_RGB323232F,
    N2D_RGB323232F_PLANAR,
    N2D_GRAY16F,
    N2D_GRAY32F,
    N2D_GRAY8,
    N2D_GRAY8I,
    N2D_BGR888,
    N2D_ARGB8888,
    N2D_ABGR8888,
    N2D_ARGB16161616I,
    N2D_ABGR16161616I,
    N2D_ARGB8888I,
    N2D_ABGR8888I,
    N2D_ARGB16161616F,
    N2D_ABGR16161616F,
    N2D_ARGB32323232F,
    N2D_ABGR32323232F
};

n2d_error_t _setMeanValue(IN n2d_user_hardware_t *Hardware)
{
    n2d_error_t error = N2D_SUCCESS;

    /* Load dst meanvalue */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_MEAN_FOR_CHANNEL_R_Address,
        Hardware->state.dest.normalizationInfo.meanValue.r
        ));
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_MEAN_FOR_CHANNEL_G_Address,
        Hardware->state.dest.normalizationInfo.meanValue.g
        ));
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_MEAN_FOR_CHANNEL_B_Address,
        Hardware->state.dest.normalizationInfo.meanValue.b
        ));

on_error:
    return error;
}

n2d_error_t _setStdReciprocal(IN n2d_user_hardware_t *Hardware)
{
    n2d_error_t error = N2D_SUCCESS;

    /* Load dst std reciprocal value */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_STD_RECIPROCAL_FOR_CHANNEL_R_Address,
        Hardware->state.dest.normalizationInfo.stdReciprocal.r
        ));
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_STD_RECIPROCAL_FOR_CHANNEL_G_Address,
        Hardware->state.dest.normalizationInfo.stdReciprocal.g
        ));
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_STD_RECIPROCAL_FOR_CHANNEL_B_Address,
        Hardware->state.dest.normalizationInfo.stdReciprocal.b
        ));

on_error:
    return error;
}

n2d_error_t _setMinValue(IN n2d_user_hardware_t *Hardware)
{
    n2d_error_t error = N2D_SUCCESS;

    /* Load dst maxmin min value */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_MIN_VALUE_FOR_CHANNEL_R_Address,
        Hardware->state.dest.normalizationInfo.minValue.r
        ));
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_MIN_VALUE_FOR_CHANNEL_G_Address,
        Hardware->state.dest.normalizationInfo.minValue.g
        ));
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_MIN_VALUE_FOR_CHANNEL_B_Address,
        Hardware->state.dest.normalizationInfo.minValue.b
        ));

on_error:
    return error;
}

n2d_error_t _setMaxminReciprocal(IN n2d_user_hardware_t *Hardware)
{
    n2d_error_t error = N2D_SUCCESS;

    /* Load dst std reciprocal value */
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_R_Address,
        Hardware->state.dest.normalizationInfo.maxMinReciprocal.r
        ));
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_G_Address,
        Hardware->state.dest.normalizationInfo.maxMinReciprocal.g
        ));
    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B_Address,
        Hardware->state.dest.normalizationInfo.maxMinReciprocal.b
        ));

on_error:
    return error;
}

n2d_error_t _setNormalizationInfo(
    IN n2d_user_hardware_t* Hardware,
    IN n2d_bool_t is4ChanelFormat)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t normalization_mode_value =
        Hardware->state.dest.normalizationInfo.normalizationMode & 0x3;

    if (is4ChanelFormat)
    {
        normalization_mode_value = gcmSETFIELDVALUE(
            normalization_mode_value,
            GCREG_DE_DST_NORMALIZATION_MODE,
            KEEP_ALPHA,
            ENABLE);

        normalization_mode_value = gcmSETFIELD(
            normalization_mode_value,
            GCREG_DE_DST_NORMALIZATION_MODE,
            ALPHA_VALUE,
            Hardware->state.dest.normalizationInfo.keepAlphaValue);
    }

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_NORMALIZATION_MODE_Address,
        normalization_mode_value));

    switch (Hardware->state.dest.normalizationInfo.normalizationMode)
    {
    case N2D_NORMALIZATION_Z_SCORE:
        N2D_ON_ERROR(_setMeanValue(Hardware));
        N2D_ON_ERROR(_setStdReciprocal(Hardware));
        break;
    case N2D_NORMALIZATION_MIN_MAX:
        N2D_ON_ERROR(_setMinValue(Hardware));
        N2D_ON_ERROR(_setMaxminReciprocal(Hardware));
        break;
    default:
        error = N2D_INVALID_ARGUMENT;
        break;
    }
on_error:
    return error;
}

n2d_error_t _setStepReciprocal(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t stepReciprocal = Hardware->state.dest.normalizationInfo.stepReciprocal;

    /* load typical value */
    if (!stepReciprocal)
    {
        if (Hardware->state.dest.normalizationInfo.normalizationMode == N2D_NORMALIZATION_Z_SCORE)
        {
            stepReciprocal = (Surface->format == N2D_RGB161616I
                || Surface->format == N2D_ARGB16161616I || Surface->format == N2D_ABGR16161616I) ?
                0x46414400 : Surface->format == N2D_RGB161616I_PLANAR ?
                0x46414400 : 0x423fc2f7;
        }
        /* normalizationMode == N2D_NORMALIZATION_MIN_MAX */
        else
        {
            stepReciprocal = (Surface->format == N2D_RGB161616I
                || Surface->format == N2D_ARGB16161616I || Surface->format == N2D_ABGR16161616I) ?
                0x477FFF00 : Surface->format == N2D_RGB161616I_PLANAR ?
                0x477FFF00 : 0x437f0000;
        }
    }

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_STEP_RECIPROCAL_Address,
        stepReciprocal
        ));

on_error:
    return error;
}

n2d_error_t _setQuantization(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    OUT n2d_bool_t* is4ChanelFormat,
    IN OUT n2d_uint32_t *Data,
    IN OUT n2d_uint32_t *DataEx
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t data = *Data, dataEx = *DataEx;
    n2d_bool_t bypassQuantization = N2D_TRUE;
    n2d_uint32_t fourChannelNormalizationStride = Hardware->state.dest.dstSurface.stride[0];

    fourChannelNormalizationStride = fourChannelNormalizationStride / 4 * 3;

    data = gcmSETFIELDVALUE(data, AQDE_DEST_CONFIG, FORMAT, EXTENSION);

    switch (Surface->format)
    {
    case N2D_RGB161616F_PLANAR:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, RGB_F16F16F16_PLANAR);
        bypassQuantization = N2D_TRUE;
        break;
    case N2D_RGB161616F:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, RGB_F16F16F16_PACKED);
        bypassQuantization = N2D_TRUE;
        break;
    case N2D_RGB323232F_PLANAR:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, RGB_F32F32F32_PLANAR);
        bypassQuantization = N2D_TRUE;
        break;
    case N2D_RGB323232F:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, RGB_F32F32F32_PACKED);
        bypassQuantization = N2D_TRUE;
        break;
    case N2D_GRAY16F:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, MASK_YUV, ENABLED);
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, GRAY_F16);

#ifdef NANO2D_PATCH_NORMALIZATION_GARY_FORMAT
        gcNormalizationGrayFormat(&dataEx);
#endif

        bypassQuantization = N2D_TRUE;
        break;
    case N2D_GRAY32F:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, MASK_YUV, ENABLED);
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, GRAY_F32);

#ifdef NANO2D_PATCH_NORMALIZATION_GARY_FORMAT
        gcNormalizationGrayFormat(&dataEx);
#endif
        bypassQuantization = N2D_TRUE;
        break;

    case N2D_RGB888:
        data   = gcmSETFIELDVALUE(data, AQDE_DEST_CONFIG, FORMAT, RGB888_PACKED);
        bypassQuantization = N2D_FALSE;
        break;
    case N2D_RGB888_PLANAR:
        data   = gcmSETFIELDVALUE(data, AQDE_DEST_CONFIG, FORMAT, RGB888_PLANAR);
        bypassQuantization = N2D_FALSE;
        break;
    case N2D_RGB888I:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, INT8_PACKED);
        bypassQuantization = N2D_FALSE;
        break;
    case N2D_RGB888I_PLANAR:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, INT8_PLANAR);
        bypassQuantization = N2D_FALSE;
        break;
    case N2D_RGB161616I:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, INT16_PACKED);
        bypassQuantization = N2D_FALSE;
        break;
    case N2D_RGB161616I_PLANAR:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, INT16_PLANAR);
        bypassQuantization = N2D_FALSE;
        break;

    case N2D_BGR888:
        data = gcmSETFIELDVALUE(data, AQDE_DEST_CONFIG, FORMAT, RGB888_PACKED);
        bypassQuantization = N2D_FALSE;
        break;
    case N2D_ARGB8888:
    case N2D_ABGR8888:
        data = gcmSETFIELDVALUE(data, AQDE_DEST_CONFIG, FORMAT, RGB888_PACKED);
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQDE_DEST_STRIDE_Address,
            fourChannelNormalizationStride));
        bypassQuantization = N2D_FALSE;
        *is4ChanelFormat = N2D_TRUE;
        break;
    case N2D_ARGB16161616I:
    case N2D_ABGR16161616I:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, INT16_PACKED);
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQDE_DEST_STRIDE_Address,
            fourChannelNormalizationStride));
        bypassQuantization = N2D_FALSE;
        *is4ChanelFormat = N2D_TRUE;
        break;
    case N2D_ARGB8888I:
    case N2D_ABGR8888I:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, INT8_PACKED);
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQDE_DEST_STRIDE_Address,
            fourChannelNormalizationStride));
        bypassQuantization = N2D_FALSE;
        *is4ChanelFormat = N2D_TRUE;
        break;
    case N2D_ARGB16161616F:
    case N2D_ABGR16161616F:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, RGB_F16F16F16_PACKED);
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQDE_DEST_STRIDE_Address,
            fourChannelNormalizationStride));
        bypassQuantization = N2D_TRUE;
        *is4ChanelFormat = N2D_TRUE;
        break;
    case N2D_ARGB32323232F:
    case N2D_ABGR32323232F:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, RGB_F32F32F32_PACKED);
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            AQDE_DEST_STRIDE_Address,
            fourChannelNormalizationStride));
        bypassQuantization = N2D_TRUE;
        *is4ChanelFormat = N2D_TRUE;
        break;
    case N2D_GRAY8:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, MASK_YUV, ENABLED);
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, GRAY_U8);

#ifdef NANO2D_PATCH_NORMALIZATION_GARY_FORMAT
        gcNormalizationGrayFormat(&dataEx);
#endif
        bypassQuantization = N2D_FALSE;
        break;
    case N2D_GRAY8I:
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, MASK_YUV, ENABLED);
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, EXTRA_FORMAT, GRAY_I8);

#ifdef NANO2D_PATCH_NORMALIZATION_GARY_FORMAT
        gcNormalizationGrayFormat(&dataEx);
#endif
        bypassQuantization = N2D_FALSE;
        break;

    default:
        error = N2D_INVALID_ARGUMENT;
        break;
    }

    N2D_ON_ERROR(gcWriteRegWithLoadState32(
        Hardware,
        GCREG_DE_DST_QUANTIZATION_BYPASS_Address,
        (bypassQuantization << 0x1) & 0x2
        ));

    if (bypassQuantization)
    {
        data = gcmSETFIELDVALUE(data, AQDE_DEST_CONFIG, FORMAT, EXTENSION);
    }
    else
    {
        N2D_ON_ERROR(_setStepReciprocal(Hardware, Surface));
    }

    *Data = data;
    *DataEx = dataEx;

on_error:
    return error;
}

n2d_error_t _isNormalizationFormat(IN gcsSURF_INFO_PTR Surface)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t i = 0;
    n2d_uint32_t support_quantity = N2D_NORMALIZATION_FORMT_QUANTITY;
    n2d_uint32_t index = 0x01;

    for (i = 0; i < N2D_COUNTOF(normalizationFormat); ++i, index <<= 1)
    {
        if (!(support_quantity & index))
        {
            continue;
        }
        if (Surface->format == normalizationFormat[i])
        {
            break;
        }
    }

    if (i == N2D_COUNTOF(normalizationFormat))
    {
        N2D_ON_ERROR(N2D_NOT_SUPPORTED);
    }

    return N2D_SUCCESS;

on_error:
    NANO2D_PRINT("NOT_SUPPORTED_FORMAT\n");
    return error;
}

n2d_error_t gcSetNormalizationState_gc820(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN OUT n2d_uint32_t *Data,
    IN OUT n2d_uint32_t *DataEx
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t data = *Data, dataEx = *DataEx;
    n2d_bool_t is4ChanelFormat = N2D_FALSE;

    if (!n2d_is_feature_support(N2D_FEATURE_NORMALIZATION) ||
        !n2d_is_feature_support(N2D_FEATURE_NORMALIZATION_QUANTIZATION))
    {
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, NORMALIZATION, DISABLE);
        return N2D_SUCCESS;
    }

    if (Hardware->state.dest.normalizationInfo.enable)
    {
        N2D_ON_ERROR(_isNormalizationFormat(Surface));
        /* signed int RGB format and gray format must to do normalization */
        dataEx = gcmSETFIELDVALUE(dataEx, GCREG_DEST_CONFIG_EX, NORMALIZATION, ENABLE);
        N2D_ON_ERROR(_setQuantization(Hardware, Surface, &is4ChanelFormat, &data, &dataEx));
        N2D_ON_ERROR(_setNormalizationInfo(Hardware, is4ChanelFormat));

        *Data = data;
        *DataEx = dataEx;
    }
    else
    {
        N2D_ON_ERROR(gcWriteRegWithLoadState32(
            Hardware,
            GCREG_DE_DST_NORMALIZATION_MODE_Address,
            GCREG_DE_DST_NORMALIZATION_MODE_ResetValue));
    }

on_error:
    return error;
}

/* Init */
n2d_error_t gcInitSeriesCommonData(void)
{
    n2d_user_os_memory_fill(&gSeriesDataFuncs, 0, sizeof(gSeriesDataFuncs));

    gSeriesDataFuncs.gcSetNormalizationState = gcSetNormalizationState_gc820;

    return N2D_SUCCESS;
}
