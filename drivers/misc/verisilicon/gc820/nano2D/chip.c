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

#include"common.h"
#include"chip.h"

struct ChipDataFuncs gChipDataFuncs;

/* Compare two strings. */
// n2d_error_t gcStrCmp(
//     IN n2d_const_string String1,
//     IN n2d_const_string String2
//     )
// {
//     int result;
//     n2d_error_t error = N2D_SUCCESS;

//     /* Perform a compare. */
//     result = strcmp(String1, String2);

//     /* Determine the status. */
//     error = (result == 0) ? N2D_SUCCESS : N2D_INVALID_ARGUMENT;

//     return error;
// }


n2d_bool_t gcIsSuppotFromFormatSheet_gc820_0x218(n2d_string SheetName, n2d_buffer_format_t Format, n2d_tiling_t Tiling, n2d_cache_mode_t CacheMode, n2d_bool_t IsInput)
{
    /* Support all tile mode When DEC400 disabled */
    // return gcStrCmp(SheetName, "TSC_DISABLE") == N2D_SUCCESS ? N2D_FALSE : N2D_TRUE;
    return N2D_FALSE;
}

n2d_error_t gcInitChipSpecificData(void)
{
    gChipDataFuncs.gcIsNotSuppotFromFormatSheet = gcIsSuppotFromFormatSheet_gc820_0x218;

    return N2D_SUCCESS;
}

n2d_bool_t gcIsNotSuppotFromFormatSheet(n2d_string SheetName,n2d_buffer_format_t Format,n2d_tiling_t Tiling, n2d_cache_mode_t CacheMode,n2d_bool_t IsInput)
{
    n2d_bool_t status = N2D_TRUE;

    if(gChipDataFuncs.gcIsNotSuppotFromFormatSheet)
    {
        status = gChipDataFuncs.gcIsNotSuppotFromFormatSheet(SheetName,Format,Tiling,CacheMode,IsInput);
    }

    /*temporary for not support*/
    return status;
}
