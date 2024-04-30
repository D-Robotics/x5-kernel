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

#ifndef _nano2d_debug_h__
#define _nano2d_debug_h__

n2d_error_t n2d_dump_surface(n2d_pointer memory, n2d_uint32_t physics, n2d_uint32_t size, n2d_bool_t IsSrc, n2d_user_hardware_t* Hardware);
#if NANO2D_DUMP
    #define N2D_DUMP_SURFACE(memory, physics, size, IsSrc, Hardware) \
             n2d_dump_surface(memory, physics, size, IsSrc, Hardware)
#else
    #define N2D_DUMP_SURFACE(memory, physics, size, IsSrc, Hardware)
#endif

n2d_error_t n2d_dump_command(n2d_uint32_t *memory, n2d_uint32_t size, n2d_user_hardware_t* Hardware);
#if NANO2D_DUMP
    #define N2D_DUMP_COMMAND(memory, size, Hardware) \
             n2d_dump_command(memory, size, Hardware)
#else
    #define N2D_DUMP_COMMAND(memory, size, Hardware)
#endif

n2d_error_t n2d_dump_close_file(n2d_user_hardware_t* Hardware);
#if NANO2D_DUMP
    #define N2D_DUMP_CLOSE(Hardware) \
             n2d_dump_close_file(Hardware)
#else
    #define N2D_DUMP_CLOSE(Hardware)
#endif

n2d_void _n2d_printf(n2d_const_string format, ...);
#define NANO2D_PRINT(...) \
         _n2d_printf(__VA_ARGS__)

#if NANO2D_PERF_TIMER
n2d_void n2d_time_stamp_print(n2d_const_string function_name, n2d_string order);
    #define N2D_PERF_TIME_STAMP_PRINT(function_name, order) \
             n2d_time_stamp_print(function_name, order)

#else
    #define N2D_PERF_TIME_STAMP_PRINT(function_name, order)

#endif

#endif