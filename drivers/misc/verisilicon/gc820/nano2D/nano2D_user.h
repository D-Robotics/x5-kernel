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

#ifndef _nano2D_user_h__
#define _nano2D_user_h__

#include "nano2D_base.h"
#include "nano2D_user_base.h"
#include "series_common.h"

#ifdef __cplusplus
extern "C" {
#endif


#define DEFINE_SET_FUNC(Func)  \
n2d_error_t \
(Func)( \
    n2d_user_hardware_t *Hardware,  \
    n2d_state_config_t  *StateConfig);\

#define DEFINE_GET_FUNC(Func)  \
n2d_error_t \
(Func)( \
    n2d_user_hardware_t     *Hardware,  \
    n2d_get_state_config_t  *StateConfig);\

struct n2d_set_state_config_func
{
    n2d_state_type_t     State;
    n2d_pointer     Func;
};

struct n2d_get_state_config_func
{
    n2d_get_state_type_t     State;
    n2d_pointer         Func;
};

n2d_error_t
gcGetStateConfigFunc(
    n2d_state_type_t State,
    n2d_get_state_type_t GetState,
    n2d_pointer *Func,
    n2d_bool_t  IsSet);

#ifdef N2D_SUPPORT_HISTOGRAM
n2d_uint32_t
gcGetHistogramCalcModeChannelCount(
    n2d_hist_calc_combination_t mode);
#endif

n2d_error_t
gcSetBlendState(
    n2d_user_hardware_t* Hardware,
    gceSURF_PIXEL_ALPHA_MODE SrcAlphaMode,
    gceSURF_PIXEL_ALPHA_MODE DstAlphaMode,
    gceSURF_GLOBAL_ALPHA_MODE SrcGlobalAlphaMode,
    gceSURF_GLOBAL_ALPHA_MODE DstGlobalAlphaMode,
    gceSURF_PIXEL_COLOR_MODE SrcColorMode,
    gceSURF_PIXEL_COLOR_MODE DstColorMode);

n2d_error_t
gcSetAlphaBlend(
    n2d_buffer_format_t srcfmt,
    n2d_buffer_format_t dstfmt,
    n2d_blend_t blend);

n2d_error_t
n2d_set_source(
    n2d_buffer_t* src_buf,
    n2d_rectangle_t* src_rect,
    n2d_rectangle_t* dst_rect);

n2d_error_t
gcCheckContext(void);

n2d_void
n2d_get_core_count(
    n2d_uint32_t* val);

n2d_void
n2d_get_core_index(
    n2d_core_id_t* val);

n2d_void
n2d_get_device_count(
    n2d_uint32_t* val);

n2d_void
n2d_get_device_index(
    n2d_device_id_t* val);

#ifdef __cplusplus
}
#endif

#endif
