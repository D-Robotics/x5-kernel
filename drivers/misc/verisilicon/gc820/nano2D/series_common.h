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

#ifndef _series_common__
#define _series_common__

#include "common.h"
#include "chip.h"

struct SeriesDataFuncs{
    n2d_error_t (*gcSetNormalizationState) (n2d_user_hardware_t *Hardware, gcsSURF_INFO_PTR Surface, n2d_uint32_t *data, n2d_uint32_t *dataEx);
    n2d_void (*gcNormalizationGrayFormat) (n2d_uint32_t* dataEx);
};

n2d_error_t gcInitSeriesCommonData(void);

n2d_bool_t gcIsNotSuppotFromFormatSheet(
    n2d_string SheetName,
    n2d_buffer_format_t Format,
    n2d_tiling_t Tiling,
    n2d_cache_mode_t CacheMode,
    n2d_bool_t IsInput
    );

n2d_error_t gcSetNormalizationState(
    IN n2d_user_hardware_t *Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN n2d_uint32_t *data,
    IN n2d_uint32_t *dataEx
    );

n2d_void gcInitNormalizationState(
    IN n2d_state_t* state
    );

n2d_void  gcNormalizationGrayFormat(
    IN OUT n2d_uint32_t* dataEx);

#endif
