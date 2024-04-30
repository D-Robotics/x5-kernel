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

#ifndef _common_h__
#define _common_h__

#include "nano2D.h"
#include "nano2D_user_base.h"
#include "nano2D_user_hardware.h"
#include "nano2D_user_os.h"
#include "nano2D_user_query.h"
#include "nano2D_engine.h"

typedef struct _Comb {
    n2d_buffer_format_t  format;
    n2d_tiling_t       tiling;
    n2d_tile_status_config_t tsc;
    n2d_cache_mode_t     CacheMode;
} Comb;

// static n2d_bool_t Comb_IsSame(Comb* left, Comb* right)
// {
//     return (left->format == right->format) &&
//         (left->tiling == right->tiling) &&
//         (left->tsc == right->tsc) &&
//         (left->CacheMode == right->CacheMode);
// }

typedef struct{
    n2d_buffer_format_t  format;

    n2d_string    supports;
} D4vFormat;

typedef struct {
    n2d_string    name;
    n2d_tile_status_config_t tsc;

    n2d_int_t       TILINGs_count;
    n2d_tiling_t*   TILINGs;

    n2d_int_t       formats_count;
    D4vFormat*      formats;

    n2d_int_t       caches_count;
    D4vFormat*      caches;


} D4vSheet;

n2d_error_t gcInitChipFuncData(void);

n2d_error_t gcInitChipSpecificData(void);

#endif
