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

#define FUNC_POINTER(func)    \
    (n2d_pointer)func

/*******************************************************************************
**
**  gcSetCSCConfig
**
**  config csc paratmeter to driver.
**
**  INPUT
**
**      StateConfig
**          n2d_cscCom_t.
**
*/
n2d_error_t gcSetCSCConfig(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
)
{
    n2d_error_t error;
    n2d_state_t* state = &Hardware->state;
    n2d_csc_config_t* cscCom = &StateConfig->config.csc;

    if (cscCom->cscMode == N2D_CSC_USER_DEFINED || cscCom->cscMode & N2D_CSC_SET_FULL_RANGE)
    {
        state->dstCSCMode = N2D_CSC_USER_DEFINED;
    }
    else
    {
        state->dstCSCMode = cscCom->cscMode;
    }

    /* set userdefine's and full range mode csc table */
    if ((cscCom->userCSCMode == N2D_CSC_YUV_TO_RGB
        || cscCom->userCSCMode == N2D_CSC_RGB_TO_YUV)
        && ((cscCom->cscMode & N2D_CSC_USER_DEFINED)
            || (cscCom->cscMode & N2D_CSC_USER_DEFINED_CLAMP)
            || (cscCom->cscMode & N2D_CSC_SET_FULL_RANGE)))
    {
        n2d_int32_t i = 0;
        n2d_int32_t* cscTable = cscCom->cscTable;
        n2d_int32_t* table = N2D_NULL;
        n2d_int32_t BT601_FULL_RANGE_CSC[12]
                    = { 306, 601, 117, -173, -340, 511, 511, -429, -84, 0, 524288, 524288 };
        n2d_int32_t BT709_FULL_RANGE_CSC[12]
                    = { 217, 732, 74, -117, -394, 511, 511, 0, 0, 0, 524288, 524288 };
        n2d_int32_t BT2020_FULL_RANGE_CSC[12]
                    = { 269, 694, 61, -143, -369, 512, 512, -471, -41, 0, 524288, 524288 };

        if (cscCom->userCSCMode == N2D_CSC_YUV_TO_RGB)
        {
            table = state->cscYUV2RGB;
        }
        else
        {
            table = state->cscRGB2YUV;
        }

        if (cscCom->cscMode & N2D_CSC_SET_FULL_RANGE)
        {

            switch (cscCom->cscMode & (~N2D_CSC_SET_FULL_RANGE))
            {
            case N2D_CSC_BT601:
                ;
                for (i = 0; i < N2D_CSC_PROGRAMMABLE_SIZE; ++i)
                {
                    table[i] = BT601_FULL_RANGE_CSC[i];
                }
                break;

            case N2D_CSC_BT709:
                ;
                for (i = 0; i < N2D_CSC_PROGRAMMABLE_SIZE; ++i)
                {
                    table[i] = BT709_FULL_RANGE_CSC[i];
                }
                break;

            case N2D_CSC_BT2020:
                ;
                for (i = 0; i < N2D_CSC_PROGRAMMABLE_SIZE; ++i)
                {
                    table[i] = BT2020_FULL_RANGE_CSC[i];
                }
                break;

            default:
                N2D_ON_ERROR(N2D_NOT_SUPPORTED);
            }
        }
        else
        {
            for (i = 0; i < N2D_CSC_PROGRAMMABLE_SIZE; ++i)
            {
                if (i < 9)
                {
                    if (cscTable[i] > N2D_INT16_MAX || cscTable[i] < N2D_INT16_MIN)
                    {
                        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
                    }
                }
                else
                {
                    if (cscTable[i] > N2D_INT25_MAX || cscTable[i] < N2D_INT25_MIN)
                    {
                        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
                    }
                }
            }

            for (i = 0; i < N2D_CSC_PROGRAMMABLE_SIZE; ++i)
            {
                table[i] = cscTable[i];
            }
        }
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}

/*******************************************************************************
**
**  gcSetMaskPackConfig
**
**  config monoblit paratmeter to driver.
**
**  INPUT
**
**      StateConfig
**          maskPackConfig.
**
*/
n2d_error_t gcSetMaskPackConfig(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
    )
{
    n2d_error_t error;
    n2d_maskpack_config_t       *maskPack = &StateConfig->config.maskPack;

    if((maskPack == N2D_NULL) || (maskPack->streamBits == N2D_NULL)
        || (maskPack->streamOffset.x < 0) || (maskPack->streamOffset.x > (n2d_int32_t)maskPack->streamWidth)
        || (maskPack->streamOffset.y < 0) || (maskPack->streamOffset.y > (n2d_int32_t)maskPack->streamHeight)
        || (maskPack->streamWidth > (maskPack->streamStride << 3)))
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    n2d_user_os_memory_copy(&Hardware->state.multiSrc[Hardware->state.currentSrcIndex].maskPackConfig,maskPack,N2D_SIZEOF(n2d_maskpack_config_t));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

/*******************************************************************************
**
**  gcSetNormalizationConfig
**
**  config parameters related to normalization.
**
**  INPUT
**
**      StateConfig
**           n2d_normalization_info_t
**
*/
#ifdef N2D_SUPPORT_NORMALIZATION
n2d_error_t gcSetNormalizationConfig(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
    )
{
    n2d_state_t *state = &Hardware->state;

    n2d_user_os_memory_copy(&state->dest.normalizationInfo,
        &StateConfig->config.normalizationInfo, N2D_SIZEOF(n2d_normalization_info_t));
    /* Success. */
    return N2D_SUCCESS;
}
#endif

/*******************************************************************************
**
**  gcSetDitherConfig
**
**  config parameters related to dither.
**
**  INPUT
**
**      StateConfig
**           n2d_dither_info_t
**
*/
n2d_error_t gcSetDitherConfig(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
    )
{
    n2d_state_t *state = &Hardware->state;

    state->dest.ditherInfo.enable = StateConfig->config.ditherInfo.enable;

    return N2D_SUCCESS;
}

#ifdef N2D_SUPPORT_HISTOGRAM

n2d_error_t gcSetHistogramEnable(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
)
{
    n2d_state_t *state = &Hardware->state;

    if (StateConfig->state == N2D_SET_HISTOGRAM_CALC)
    {
        state->dest.histogramCalcInfo.enable = StateConfig->config.histogramCalc.enable;

        /* Histogram calculation and equalization should not be enabled at the same time */
        if (state->dest.histogramCalcInfo.enable)
        {
            state->dest.histogramEqualInfo.enable = N2D_FALSE;
        }
    }
    else
    {
        state->dest.histogramEqualInfo.enable = StateConfig->config.histogramEqual.enable;

        /* Histogram calculation and equalization should not be enabled at the same time */
        if (state->dest.histogramEqualInfo.enable)
        {
            state->dest.histogramCalcInfo.enable = N2D_FALSE;
        }
    }

    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetHistogramCalcMemMode(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
)
{
    n2d_state_t *state = &Hardware->state;

    state->dest.histogramCalcInfo.memMode = StateConfig->config.histogramCalc.memMode;

    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetHistogramCalcComb(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
)
{
    n2d_state_t *state = &Hardware->state;

    state->dest.histogramCalcInfo.mode = StateConfig->config.histogramCalc.mode;
    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetHistogramHistSize(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
)
{
    n2d_state_t *state = &Hardware->state;

    state->dest.histogramCalcInfo.histSize = StateConfig->config.histogramCalc.histSize;
    /* Success. */
    return N2D_SUCCESS;

}
n2d_error_t gcSetHistogramEqualComb(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
)
{
    n2d_state_t *state = &Hardware->state;

    state->dest.histogramEqualInfo.mode = StateConfig->config.histogramEqual.mode;
    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetHistogramEqualOption(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
)
{
    n2d_state_t *state = &Hardware->state;

    state->dest.histogramEqualInfo.option = StateConfig->config.histogramEqual.option;
    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetBrightnessAndSaturationEnable(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
    )
{
    n2d_state_t *state = &Hardware->state;

    state->dest.brightnessAndSaturationInfo.enable = StateConfig->config.brightnessAndSaturation.enable;
    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetBrightnessAndSaturationMode(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
    )
{
    n2d_state_t *state = &Hardware->state;

    state->dest.brightnessAndSaturationInfo.mode = StateConfig->config.brightnessAndSaturation.mode;
    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetBrightnessAndSaturationFormat(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
    )
{
    n2d_state_t *state = &Hardware->state;

    state->dest.brightnessAndSaturationInfo.srcFormat = StateConfig->config.brightnessAndSaturation.srcFormat;
    state->dest.brightnessAndSaturationInfo.dstFormat = StateConfig->config.brightnessAndSaturation.dstFormat;

    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetBrightnessAndSaturationGammaModeFactor(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
    )
{
    Hardware->state.dest.brightnessAndSaturationInfo.sGammaLUT
        = Hardware->state.dest.brightnessAndSaturationInfo.vGammaLUT
        = N2D_NULL;

    if (StateConfig->config.brightnessAndSaturation.sGammaLUT)
    {
        Hardware->state.dest.brightnessAndSaturationInfo.sGammaLUT
            = (n2d_uint32_t*)malloc(N2D_BRIGHTNESS_SATURATION_GAMMA_LUT_COUNT * N2D_SIZEOF(n2d_uint32_t));
        n2d_user_os_memory_copy(Hardware->state.dest.brightnessAndSaturationInfo.sGammaLUT,
            StateConfig->config.brightnessAndSaturation.sGammaLUT,
            sizeof(n2d_uint32_t) * N2D_BRIGHTNESS_SATURATION_GAMMA_LUT_COUNT);
    }

    if (StateConfig->config.brightnessAndSaturation.vGammaLUT)
    {
        Hardware->state.dest.brightnessAndSaturationInfo.vGammaLUT
            = (n2d_uint32_t*)malloc(N2D_BRIGHTNESS_SATURATION_GAMMA_LUT_COUNT * N2D_SIZEOF(n2d_uint32_t));
        n2d_user_os_memory_copy(Hardware->state.dest.brightnessAndSaturationInfo.vGammaLUT,
            StateConfig->config.brightnessAndSaturation.vGammaLUT,
            sizeof(n2d_uint32_t) * N2D_BRIGHTNESS_SATURATION_GAMMA_LUT_COUNT);
    }

    /* Success. */
    return N2D_SUCCESS;
}

n2d_error_t gcSetBrightnessAndSaturationLinearModeFactor(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
    )
{
    n2d_state_t *state = &Hardware->state;

    state->dest.brightnessAndSaturationInfo.linearAddFactor
        = StateConfig->config.brightnessAndSaturation.linearAddFactor;
    state->dest.brightnessAndSaturationInfo.linearMulFactor
        = StateConfig->config.brightnessAndSaturation.linearMulFactor;

    /* Success. */
    return N2D_SUCCESS;
}

/*******************************************************************************
**
**  gcSetHistogramCalcConfig
**
**  config parameters related to histogram calculation.
**
**  INPUT
**
**      StateConfig
**           n2d_histogram_calc_config_t
**
*/
n2d_error_t gcSetHistogramCalcConfig(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
    )
{
    n2d_error_t error = N2D_SUCCESS;

    N2D_ON_ERROR(gcSetHistogramEnable(Hardware, StateConfig));
    N2D_ON_ERROR(gcSetHistogramCalcMemMode(Hardware, StateConfig));
    N2D_ON_ERROR(gcSetHistogramCalcComb(Hardware, StateConfig));
    N2D_ON_ERROR(gcSetHistogramHistSize(Hardware, StateConfig));

on_error:
    return error;
}

/*******************************************************************************
**
**  gcSetHistogramEqualConfig
**
**  config parameters related to histogram equalization.
**
**  INPUT
**
**      StateConfig
**           n2d_histogram_equal_config_t
**
*/
n2d_error_t gcSetHistogramEqualConfig(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
    )
{
    n2d_error_t error = N2D_SUCCESS;

    N2D_ON_ERROR(gcSetHistogramEnable(Hardware, StateConfig));
    N2D_ON_ERROR(gcSetHistogramEqualComb(Hardware, StateConfig));
    N2D_ON_ERROR(gcSetHistogramEqualOption(Hardware, StateConfig));

on_error:
    return error;
}
#endif

/*******************************************************************************
**
**  gcSetBrightnessConfig
**
**  config parameters related to brightness and saturation.
**
**  INPUT
**
**      StateConfig
**           n2d_brightness_saturation_config_t
**
*/
#ifdef N2D_SUPPORT_BRIGHTNESS
n2d_error_t gcSetBrightnessConfig(
    n2d_user_hardware_t *Hardware,
    n2d_state_config_t  *StateConfig
    )
{
    n2d_error_t error = N2D_SUCCESS;

    N2D_ON_ERROR(gcSetBrightnessAndSaturationEnable(Hardware, StateConfig));
    N2D_ON_ERROR(gcSetBrightnessAndSaturationMode(Hardware, StateConfig));
    N2D_ON_ERROR(gcSetBrightnessAndSaturationFormat(Hardware, StateConfig));

    if (StateConfig->config.brightnessAndSaturation.mode == N2D_BRIGHTNESS_SATURATION_MODE_LINEAR)
    {
        N2D_ON_ERROR(gcSetBrightnessAndSaturationLinearModeFactor(Hardware, StateConfig));
    }
    else if (StateConfig->config.brightnessAndSaturation.mode == N2D_BRIGHTNESS_SATURATION_MODE_GAMMA)
    {
        N2D_ON_ERROR(gcSetBrightnessAndSaturationGammaModeFactor(Hardware, StateConfig));
    }

on_error:
    return error;
}
#endif

/*******************************************************************************
**
**  gcSetContextID
**
**  set contextID to driver.
**
**  INPUT
**
**      StateConfig
**           n2d_contextID_config_t
**
*/
#ifdef N2D_SUPPORT_CONTEXT_ID
n2d_error_t gcSetContextID(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    n2d_error_t error = N2D_SUCCESS;

    n2d_uint32_t id = StateConfig->config.contextID.high << 0x10
                    | StateConfig->config.contextID.low;
    Hardware->contextID[StateConfig->config.contextID.index] = id;
    return error;
}
#endif

/*******************************************************************************
**
**  gcSetFastClear
**
**  set fastclear enable or not. If enable, set fc value.
**
**  INPUT
**
**      StateConfig
**           n2d_contextID_config_t
**
*/
n2d_error_t gcSetFastClear(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    n2d_error_t error = N2D_SUCCESS;

    n2d_user_os_memory_copy(&Hardware->state.fastclear,
        &StateConfig->config.fastclear,
        N2D_SIZEOF(n2d_fastclear_config_t));

    return error;
}

/*******************************************************************************
**
**  gcSetMultisourceIndex
**
**  set buffer index of multisource blit.
**
**  INPUT
**
**      StateConfig
**           n2d_multisource_index_config_t
**
*/
n2d_error_t gcSetMultisourceIndex(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    n2d_error_t error = N2D_SUCCESS;

    N2D_ASSERT(StateConfig->config.multisourceIndex < gcdMULTI_SOURCE_NUM
        && StateConfig->config.multisourceIndex >= 0);

    Hardware->state.currentSrcIndex = StateConfig->config.multisourceIndex;

    return error;
}

/*******************************************************************************
**
**  gcSetClipRectangle
**
**  Set clipping rectangle for the current source corresponding to destination.
**      only support multi-source
**
**  INPUT:
**
**      n2d_rectangle_t *cliprect
**          clipping rectangle.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetClipRectangle(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    n2d_error_t     error = N2D_SUCCESS;
    gcsRECT         rect = {0};

    rect.left = StateConfig->config.clipRect.x;
    rect.top = StateConfig->config.clipRect.y;
    rect.right = rect.left + StateConfig->config.clipRect.width;
    rect.bottom = rect.top + StateConfig->config.clipRect.height;

    Hardware->state.multiSrc[Hardware->state.currentSrcIndex].clipRect = rect;
    Hardware->state.dstClipRect = rect;

    return error;
}

n2d_error_t gcSetBlendState(
    IN n2d_user_hardware_t* Hardware,
    IN gceSURF_PIXEL_ALPHA_MODE SrcAlphaMode,
    IN gceSURF_PIXEL_ALPHA_MODE DstAlphaMode,
    IN gceSURF_GLOBAL_ALPHA_MODE SrcGlobalAlphaMode,
    IN gceSURF_GLOBAL_ALPHA_MODE DstGlobalAlphaMode,
    IN gceSURF_PIXEL_COLOR_MODE SrcColorMode,
    IN gceSURF_PIXEL_COLOR_MODE DstColorMode
)
{
    n2d_error_t error;
    gcs2D_MULTI_SOURCE_PTR src = &Hardware->state.multiSrc[Hardware->state.currentSrcIndex];

    if ((SrcColorMode == gcvSURF_COLOR_MULTIPLY)
        || (DstColorMode == gcvSURF_COLOR_MULTIPLY))
    {
        n2d_pixel_color_multiply_mode_t srcPremultiply = N2D_COLOR_MULTIPLY_DISABLE;
        n2d_pixel_color_multiply_mode_t dstPremultiply = N2D_COLOR_MULTIPLY_DISABLE;
        n2d_global_color_multiply_mode_t srcPremultiplyGlobal = N2D_GLOBAL_COLOR_MULTIPLY_DISABLE;

        if (SrcColorMode == gcvSURF_COLOR_MULTIPLY)
        {
            if (SrcAlphaMode != gcvSURF_PIXEL_ALPHA_STRAIGHT)
            {
                N2D_ON_ERROR(N2D_NOT_SUPPORTED);
            }

            if ((SrcGlobalAlphaMode == gcvSURF_GLOBAL_ALPHA_OFF)
                || (SrcGlobalAlphaMode == gcvSURF_GLOBAL_ALPHA_SCALE))
            {
                srcPremultiply = N2D_COLOR_MULTIPLY_ENABLE;
            }

            if ((SrcGlobalAlphaMode == gcvSURF_GLOBAL_ALPHA_ON)
                || (SrcGlobalAlphaMode == gcvSURF_GLOBAL_ALPHA_SCALE))
            {
                srcPremultiplyGlobal = N2D_GLOBAL_COLOR_MULTIPLY_ALPHA;
            }
        }

        if (DstColorMode == gcvSURF_COLOR_MULTIPLY)
        {
            if (DstAlphaMode != gcvSURF_PIXEL_ALPHA_STRAIGHT)
            {
                N2D_ON_ERROR(N2D_NOT_SUPPORTED);
            }

            if (DstGlobalAlphaMode == gcvSURF_GLOBAL_ALPHA_OFF)
            {
                dstPremultiply = N2D_COLOR_MULTIPLY_ENABLE;
            }
            else
            {
                N2D_ON_ERROR(N2D_NOT_SUPPORTED);
            }
        }

        if (srcPremultiply != N2D_COLOR_MULTIPLY_DISABLE)
        {
            src->srcPremultiplyMode = srcPremultiply;
        }

        if (srcPremultiplyGlobal != N2D_GLOBAL_COLOR_MULTIPLY_DISABLE)
        {
            src->srcPremultiplyGlobalMode = srcPremultiplyGlobal;
        }

        if (dstPremultiply != N2D_COLOR_MULTIPLY_DISABLE)
        {
            src->dstPremultiplyMode = dstPremultiply;
        }
    }

    src->srcColorMode = gcvSURF_COLOR_STRAIGHT;
    src->dstColorMode = gcvSURF_COLOR_STRAIGHT;

    src->srcAlphaMode = SrcAlphaMode;
    src->dstAlphaMode = DstAlphaMode;
    src->srcGlobalAlphaMode = SrcGlobalAlphaMode;
    src->dstGlobalAlphaMode = DstGlobalAlphaMode;
    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}


/*******************************************************************************
**
**  gcSetGlobalAlpha
**
**  Set source and destination global alpha mode and value.
**
**  INPUT:
**
**      n2d_global_alpha_t src and dst.
**      n2d_uint32_t src and dst value.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetGlobalAlpha(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    n2d_error_t error = N2D_SUCCESS;
    gcs2D_MULTI_SOURCE_PTR src = N2D_NULL;
    static const gceSURF_GLOBAL_ALPHA_MODE global_alpha_modes[] = {
        gcvSURF_GLOBAL_ALPHA_OFF,
        gcvSURF_GLOBAL_ALPHA_ON,
        gcvSURF_GLOBAL_ALPHA_SCALE
    };

    gceSURF_GLOBAL_ALPHA_MODE SrcGlobalAlphaMode
        = global_alpha_modes[StateConfig->config.globalAlpha.srcMode];
    gceSURF_GLOBAL_ALPHA_MODE DstGlobalAlphaMode
        = global_alpha_modes[StateConfig->config.globalAlpha.dstMode];

    src = &Hardware->state.multiSrc[Hardware->state.currentSrcIndex];

    src->srcGlobalColor = (src->srcGlobalColor & 0x00FFFFFF)
        | (((n2d_uint32_t)StateConfig->config.globalAlpha.srcValue & 0xFF) << 24);
    src->dstGlobalColor = (src->dstGlobalColor & 0x00FFFFFF)
        | (((n2d_uint32_t)StateConfig->config.globalAlpha.dstValue & 0xFF) << 24);

    N2D_ON_ERROR(gcSetBlendState(Hardware, src->srcAlphaMode, src->dstAlphaMode, SrcGlobalAlphaMode,
        DstGlobalAlphaMode, src->srcColorMode, src->dstColorMode));

on_error:
    return error;
}

/*******************************************************************************
**
**  gcSetBlendMode
**
**  Set alphablend for the current source corresponding to destination.
**      only support multi-source
**
**  INPUT:
**
**      n2d_blend_t blend
**          alphablend mode.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetBlendMode(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    n2d_error_t    error = N2D_SUCCESS;

    N2D_ON_ERROR(gcSetAlphaBlend(
        Hardware->state.multiSrc[Hardware->state.currentSrcIndex].srcSurface.format,
        N2D_RGBA8888,
        StateConfig->config.alphablendMode));

on_error:
    return error;
}

/*******************************************************************************
**
**  gcSetMultiSrcAndDstRectangle
**
**  Set source's and destination's rectangle.
**      only support multi-source
**
**  INPUT:
**
**      n2d_buffer_t *source
**          source buffer.
**
**      n2d_rectangle_t *srcRect
**          Pointer to a valid source rectangle.
**
**      n2d_rectangle_t *dstRect
**          Pointer to a valid destination rectangle
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetMultiSrcAndDstRectangle(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    n2d_error_t    error = N2D_SUCCESS;

    N2D_ON_ERROR(n2d_set_source(
        StateConfig->config.multisrcAndDstRect.source,
        &StateConfig->config.multisrcAndDstRect.srcRect,
        &StateConfig->config.multisrcAndDstRect.dstRect));

on_error:
    return error;
}

/*******************************************************************************
**
**  gcSetROP
**
**  Set the ROP for source and destination.
**
**  INPUT:
**
**      n2d_uint8_t fg_rop
**          Foreground ROP.
**
**      n2d_uint8_t bg_rop
**          Background ROP.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetROP(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    Hardware->state.multiSrc[Hardware->state.currentSrcIndex].fgRop =
        StateConfig->config.rop.fg_rop & 0xff;
    Hardware->state.multiSrc[Hardware->state.currentSrcIndex].bgRop =
        StateConfig->config.rop.bg_rop & 0xff;

    return N2D_SUCCESS;
}

/*******************************************************************************
**
**  gcSetTransparency
**
**  Set the transparency for source, destination and pattern.
**  Pattern not currently supported, only open interface.
**
**  INPUT:
**
**      n2d_transparency_t src
**          Source Transparency.
**
**      n2d_transparency_t dst
**          Destination Transparency.
**
**      n2d_transparency_t pat
**          Pattern Transparency.
**
**  OUTPUT:
**
**      Nothing.
*/

n2d_error_t gcSetTransparency(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    n2d_error_t error;

    if (!(n2d_is_feature_support(N2D_FEATURE_TRANSPARENCY_MASK)
        || n2d_is_feature_support(N2D_FEATURE_TRANSPARENCY_COLORKEY)))
    {
        if (StateConfig->config.transparency.src != N2D_OPAQUE ||
            StateConfig->config.transparency.dst != N2D_OPAQUE ||
            StateConfig->config.transparency.pat != N2D_OPAQUE)
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }
    }

    Hardware->state.multiSrc[Hardware->state.currentSrcIndex].srcTransparency
        = StateConfig->config.transparency.src;
    Hardware->state.multiSrc[Hardware->state.currentSrcIndex].dstTransparency
        = StateConfig->config.transparency.dst;
    /*Not currently supported, only open interface*/
    Hardware->state.multiSrc[Hardware->state.currentSrcIndex].patTransparency
        = N2D_OPAQUE;
    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

/*******************************************************************************
**
**  n2d_set_source_colorkey
**
**  Set the source color key range.
**  Color channel values should specified in the range allowed by the source format.
**  Lower color key's color channel values should be less than or equal to
**  the corresponding color channel value of ColorKeyHigh.
**  When target format is A8, only Alpha components are used. Otherwise, Alpha
**  components are not used.
**
**
**  INPUT:
**
**      gco2D Engine
**          Pointer to the gco2D object.
**
**      n2d_uint32_t colorkeyLow
**          The low color key value in A8R8G8B8 format.
**
**      n2d_uint32_t colorkeyHigh
**          The high color key value in A8R8G8B8 format.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetSourceColorKey(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    N2D_ASSERT(StateConfig->config.srcColorKey.colorkeyHigh
                >= StateConfig->config.srcColorKey.colorkeyLow);

    Hardware->state.multiSrc[Hardware->state.currentSrcIndex].srcColorKeyLow
        = StateConfig->config.srcColorKey.colorkeyLow;
    Hardware->state.multiSrc[Hardware->state.currentSrcIndex].srcColorKeyHigh
        = StateConfig->config.srcColorKey.colorkeyHigh;

    return N2D_SUCCESS;
}

/*******************************************************************************
**
**  gcSetDestinationColorKey
**
**  Set the destination color key range.
**  Color channel values should specified in the range allowed by the target format.
**  Lower color key's color channel values should be less than or equal to
**  the corresponding color channel value of ColorKeyHigh.
**  When target format is A8, only Alpha components are used. Otherwise, Alpha
**  components are not used.
**
**  INPUT:
**
**      n2d_uint32_t colorkeylow
**          The low color key value in A8R8G8B8 format.
**
**      n2d_uint32_t colorkeyhig
**          The high color key value in A8R8G8B8 format.
**
**  OUTPUT:
**
**      Nothing.
*/

n2d_error_t gcSetDestinationColorKey(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    N2D_ASSERT(StateConfig->config.dstColorKey.colorkeyHigh
        >= StateConfig->config.dstColorKey.colorkeyLow);

    Hardware->state.dstColorKeyLow = StateConfig->config.dstColorKey.colorkeyLow;
    Hardware->state.dstColorKeyHigh = StateConfig->config.dstColorKey.colorkeyHigh;

    return N2D_SUCCESS;
}

/*******************************************************************************
**
**  gcSetMultiplyMode
**
**  Set the source and target pixel multiply modes.
**
**  INPUT:
**
**      n2d_pixel_color_multiply_mode_t src_premultiply_src_alpha
**          Source color premultiply with Source Alpha.
**
**      n2d_pixel_color_multiply_mode_t dst_premultiply_dst_alpha
**          Destination color premultiply with Destination Alpha.
**
**      n2d_global_color_multiply_mode_t src_premultiply_global_mode
**          Source color premultiply with Global color's Alpha or Color.
**
**      n2d_pixel_color_multiply_mode_t dst_demultiply_dst_alpha
**          Destination color demultiply with Destination Alpha.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetMultiplyMode(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    gcs2D_MULTI_SOURCE_PTR curSrc = N2D_NULL;

    curSrc = &Hardware->state.multiSrc[Hardware->state.currentSrcIndex];

    curSrc->srcPremultiplyMode = StateConfig->config.pixelMultiplyMode.srcPremult;
    curSrc->dstPremultiplyMode = StateConfig->config.pixelMultiplyMode.dstPremult;
    curSrc->srcPremultiplyGlobalMode = StateConfig->config.pixelMultiplyMode.srcGlobal;
    curSrc->dstDemultiplyMode = StateConfig->config.pixelMultiplyMode.dstDemult;

    /* Success. */
    return N2D_SUCCESS;
}

/*******************************************************************************
**
**  gcSetKernelSize
**
**  Set kernel size.
**
**  INPUT:
**
**      n2d_uint8_t horkernelsize
**          Kernel size for the horizontal pass.
**
**      n2d_uint8_t verkernelsize
**          Kernel size for the vertical pass.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetKernelSize(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
)
{
    n2d_error_t  error = N2D_SUCCESS;
    n2d_state_t* state = N2D_NULL;
    n2d_uint32_t maxTapSize = gcvMAXTAPSIZE_OPF;

    if (n2d_is_feature_support(N2D_FEATURE_SCALER))
    {
        maxTapSize = gcvMAXTAPSIZE;
    }

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT((StateConfig->config.kernelSize.horSize & 0x1)
        && (StateConfig->config.kernelSize.horSize <= maxTapSize));
    gcmVERIFY_ARGUMENT((StateConfig->config.kernelSize.verSize & 0x1)
        && (StateConfig->config.kernelSize.verSize <= maxTapSize));

    state = &Hardware->state;
    /* Set sizes. */
    state->filterState.newHorKernelSize = StateConfig->config.kernelSize.horSize;
    state->filterState.newVerKernelSize = StateConfig->config.kernelSize.verSize;

on_error:
    return error;
}

/*******************************************************************************
**
**  gcSetFilterType
**
**  Set filter type.
**
**  INPUT:
**
**      n2d_filter_type_t FilterType
**          Filter type for the filter blit.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetFilterType(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig
    )
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_state_t* state = N2D_NULL;

    state = &Hardware->state;

    switch (StateConfig->config.filterType)
    {
    case N2D_FILTER_SYNC:
        state->filterState.newFilterType = gcvFILTER_SYNC;
        break;

    case N2D_FILTER_BLUR:
        state->filterState.newFilterType = gcvFILTER_BLUR;
        break;

    case N2D_FILTER_USER:
        state->filterState.newFilterType = gcvFILTER_USER;
        break;

    case N2D_FILTER_BILINEAR:
        state->filterState.newFilterType = gcvFILTER_BILINEAR;
        break;

    case N2D_FILTER_BICUBIC:
        state->filterState.newFilterType = gcvFILTER_BICUBIC;
        break;

    default:
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

on_error:
    return error;
}

/*******************************************************************************
**
**  gcSetu8Tou10cvtMode
**
**  Set conversion mode of uint8 to uint10.
**
**  INPUT:
**
**      n2d_u8_to_u10_conversion_mode_t u8Tou10cvtMode
**          cvt mode for u8 to u10.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcSetu8Tou10cvtMode(
    n2d_user_hardware_t* Hardware,
    n2d_state_config_t* StateConfig)
{
    n2d_error_t error = N2D_SUCCESS;

    n2d_user_os_memory_copy(&Hardware->state.dest.u8Tu10_cvtMode,
        &StateConfig->config.u8Tou10cvtMode,
        N2D_SIZEOF(n2d_u8_to_u10_conversion_mode_t));

    return error;
}


/**
************************************************** N2D_GET ****************************************
**/

#ifdef N2D_SUPPORT_HISTOGRAM

n2d_uint32_t gcGetHistogramCalcModeChannelCount(
    n2d_hist_calc_combination_t mode)
{
    n2d_uint32_t channel = 0;

    if (mode & N2D_HIST_CALC_RGB)
    {
        channel += mode & N2D_HIST_CALC_R ? 1 : 0;
        channel += mode & N2D_HIST_CALC_G ? 1 : 0;
        channel += mode & N2D_HIST_CALC_B ? 1 : 0;
    }
    else if(mode & N2D_HIST_CALC_GRAY)
    {
        channel += mode == N2D_HIST_CALC_GRAY_GRAY_GRADIENT ? 2 : 1;
    }

    return channel;
}

/*******************************************************************************
**
**  gcGetHistogramArraySize
**
**  get size of histogram calculation result array.
**
**  INPUT
**          Nothing
**
**  OUTPUT
**          n2d_uint32_t
**
*/
n2d_error_t gcGetHistogramArraySize(
    n2d_user_hardware_t     *Hardware,
    n2d_get_state_config_t  *StateConfig
    )
{
    n2d_error_t error = N2D_SUCCESS;

    n2d_uint32_t channel = gcGetHistogramCalcModeChannelCount(
        Hardware->state.dest.histogramCalcInfo.mode);

    StateConfig->config.histogramArraySize
        = Hardware->state.dest.histogramCalcInfo.histSize * channel;

    return error;
}

/*******************************************************************************
**
**  gcGetHistogramArray
**
**  get pointer of histogram calculation result array.
**
**  INPUT
**          n2d_histogram_array_t.size
**
**  OUTPUT
**          n2d_histogram_array_t.array
**
*/
n2d_error_t gcGetHistogramArray(
    n2d_user_hardware_t     *Hardware,
    n2d_get_state_config_t  *StateConfig
    )
{
    n2d_error_t error = N2D_SUCCESS;

    if (Hardware->state.dest.histogramCalcInfo.memMode == N2D_PLANAR)
    {
        n2d_uint32_t channel = gcGetHistogramCalcModeChannelCount(
            Hardware->state.dest.histogramCalcInfo.mode);

        n2d_size_t offset = StateConfig->config.histogramArray.size / channel;

        if (channel > 2)
        {
            memcpy(StateConfig->config.histogramArray.array,
                Hardware->state.dest.dstSurface.histogram_buffer[2].memory,
                offset * N2D_SIZEOF(n2d_uint32_t));
        }

        if (channel > 1)
        {
            n2d_uint32_t location = channel == 2 ? 0 : 1;
            memcpy(StateConfig->config.histogramArray.array + offset * location,
                Hardware->state.dest.dstSurface.histogram_buffer[1].memory,
                offset * N2D_SIZEOF(n2d_uint32_t));
        }

        if (channel > 0)
        {
            n2d_uint32_t location = channel == 3 ? 2 :
                channel == 2 ? 1 : 0;
            memcpy(StateConfig->config.histogramArray.array + offset * location,
                Hardware->state.dest.dstSurface.histogram_buffer[0].memory,
                offset * N2D_SIZEOF(n2d_uint32_t));
        }

    }
    else
    {
        memcpy(StateConfig->config.histogramArray.array,
           Hardware->state.dest.dstSurface.histogram_buffer[0].memory,
           StateConfig->config.histogramArray.size * sizeof(n2d_uint32_t));
    }

    return error;
}

#endif

/*******************************************************************************
**
**  gcGetGPUCoreCount
**
**  get GPU core count.
**
**  INPUT
**          n2d_uint32_t pointer
**
**  OUTPUT
**          n2d_uint32_t core count
**
*/
n2d_error_t gcGetGPUCoreCount(
    n2d_user_hardware_t* Hardware,
    n2d_get_state_config_t* StateConfig
)
{
    n2d_get_core_count(&StateConfig->config.coreCount);

    return N2D_SUCCESS;
}

/*******************************************************************************
**
**  gcGetGPUCoreIndex
**
**  get GPU current core index.
**
**  INPUT
**          n2d_core_id_t pointer
**
**  OUTPUT
**          n2d_core_id_t core index
**
*/
n2d_error_t gcGetGPUCoreIndex(
    n2d_user_hardware_t* Hardware,
    n2d_get_state_config_t* StateConfig)
{
    n2d_get_core_index(&StateConfig->config.coreIndex);

    return N2D_SUCCESS;
}

/*******************************************************************************
**
**  gcGetDeviceIndex
**
**  Query device index.
**
**  INPUT:
**
**      n2d_device_id_t pointer
**
**  OUTPUT:
**
**      n2d_device_id_t device index/id.
*/
n2d_error_t gcGetDeviceIndex(
    n2d_user_hardware_t* Hardware,
    n2d_get_state_config_t* StateConfig)
{
    n2d_get_device_index(&StateConfig->config.deviceIndex);

    return N2D_SUCCESS;
}

/*******************************************************************************
**
**  gcGetDeviceCount
**
**  Query device count.
**
**  INPUT:
**
**      n2d_uint32_t pointer
**
**  OUTPUT:
**
**      n2d_uint32_t device count.
*/
n2d_error_t gcGetDeviceCount(
    n2d_user_hardware_t* Hardware,
    n2d_get_state_config_t* StateConfig)
{
    n2d_get_device_count(&StateConfig->config.deviceCount);

    return N2D_SUCCESS;
}

/**************************************************** N2D STATE KEY ******************************************/

struct n2d_set_state_config_func setStateConfigFunc[] =
{
    {N2D_SET_CSC,                       FUNC_POINTER(gcSetCSCConfig)},
    {N2D_SET_MASKPACK,                  FUNC_POINTER(gcSetMaskPackConfig)},

#ifdef N2D_SUPPORT_NORMALIZATION
    {N2D_SET_NORMALIZATION,             FUNC_POINTER(gcSetNormalizationConfig)},
#endif
    {N2D_SET_DITHER,                    FUNC_POINTER(gcSetDitherConfig)},

#ifdef N2D_SUPPORT_HISTOGRAM
    {N2D_SET_HISTOGRAM_CALC,            FUNC_POINTER(gcSetHistogramCalcConfig)},
    {N2D_SET_HISTOGRAM_EQUAL,           FUNC_POINTER(gcSetHistogramEqualConfig)},
#endif

#ifdef N2D_SUPPORT_BRIGHTNESS
    {N2D_SET_BRIGHTNESS,                FUNC_POINTER(gcSetBrightnessConfig)},
#endif

#ifdef N2D_SUPPORT_CONTEXT_ID
    {N2D_SET_CONTEXT_ID,                FUNC_POINTER(gcSetContextID)},
#endif
    {N2D_SET_FAST_CLEAR,                FUNC_POINTER(gcSetFastClear)},
    {N2D_SET_MULTISOURCE_INDEX,         FUNC_POINTER(gcSetMultisourceIndex)},
    {N2D_SET_CLIP_RECTANGLE,            FUNC_POINTER(gcSetClipRectangle)},
    {N2D_SET_GLOBAL_ALPHA,              FUNC_POINTER(gcSetGlobalAlpha)},
    {N2D_SET_ALPHABLEND_MODE,           FUNC_POINTER(gcSetBlendMode)},
    {N2D_SET_MULTISRC_DST_RECTANGLE,    FUNC_POINTER(gcSetMultiSrcAndDstRectangle)},
    {N2D_SET_ROP,                       FUNC_POINTER(gcSetROP)},
    {N2D_SET_TRANSPARENCY,              FUNC_POINTER(gcSetTransparency)},
    {N2D_SET_SRC_COLORKEY,              FUNC_POINTER(gcSetSourceColorKey)},
    {N2D_SET_DST_COLORKEY,              FUNC_POINTER(gcSetDestinationColorKey)},
    {N2D_SET_PIXEL_MULTIPLY_MODE,       FUNC_POINTER(gcSetMultiplyMode)},
    {N2D_SET_KERNEL_SIZE,               FUNC_POINTER(gcSetKernelSize)},
    {N2D_SET_FILTER_TYPE,               FUNC_POINTER(gcSetFilterType)},
    {N2D_SET_U8TOU10CVT_MODE,           FUNC_POINTER(gcSetu8Tou10cvtMode)},
};

struct n2d_get_state_config_func getStateConfigFunc[] =
{
#ifdef N2D_SUPPORT_HISTOGRAM
    {N2D_GET_HISTOGRAM_ARRAY_SIZE,      FUNC_POINTER(gcGetHistogramArraySize)},
    {N2D_GET_HISTOGRAM_ARRAY,           FUNC_POINTER(gcGetHistogramArray)},
#endif
    {N2D_GET_CORE_COUNT,                FUNC_POINTER(gcGetGPUCoreCount)},
    {N2D_GET_CORE_INDEX,                FUNC_POINTER(gcGetGPUCoreIndex)},
    {N2D_GET_DEVICE_COUNT,              FUNC_POINTER(gcGetDeviceCount)},
    {N2D_GET_DEVICE_INDEX,              FUNC_POINTER(gcGetDeviceIndex)},
};

#define SET_ENUM_FUNC(State)\
    setStateConfigFunc[State].Func

#define GET_ENUM_FUNC(State)\
    getStateConfigFunc[State].Func

n2d_error_t gcGetStateConfigFunc(
    n2d_state_type_t SetState,
    n2d_get_state_type_t GetState,
    n2d_pointer* Func,
    n2d_bool_t  IsSet
)
{
    n2d_error_t error;

    /* Check the context. */
    N2D_ON_ERROR(gcCheckContext());

    if (IsSet)
    {
        if (SetState >= N2D_COUNTOF(setStateConfigFunc) || SetState < 0)
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }

        *Func = SET_ENUM_FUNC(SetState);
    }
    else
    {
        if (GetState >= N2D_COUNTOF(getStateConfigFunc) || GetState < 0)
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }

        *Func = GET_ENUM_FUNC(GetState);
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}