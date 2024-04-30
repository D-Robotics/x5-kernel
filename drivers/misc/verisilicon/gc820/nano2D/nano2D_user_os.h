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

#ifndef _nano2D_user_os_h__
#define _nano2D_user_os_h__

#ifdef __cplusplus
extern "C" {
#endif

n2d_error_t
n2d_user_os_allocate(
    n2d_uint32_t size,
    n2d_pointer *memory);

n2d_error_t
n2d_user_os_free(
    n2d_pointer memory);

n2d_void
n2d_user_os_memory_fill(
    n2d_pointer memory,
    n2d_uint8_t filler,
    n2d_uint32_t bytes);

n2d_void
n2d_user_os_memory_copy(
    n2d_pointer dst,
    n2d_pointer src,
    n2d_uint32_t size);

// n2d_float_t
// n2d_user_os_math_sine(
//     n2d_float_t x);

n2d_error_t
n2d_user_os_ioctl(
    n2d_ioctl_interface_t *Data);

// n2d_uint64_t
// n2d_user_os_get_ticks(
//     n2d_void);

n2d_error_t
n2d_user_os_get_tls(
    gcoTLS* TLS);

n2d_error_t
n2d_user_os_free_tls(
    gcoTLS* TLS);

#ifdef __cplusplus
}
#endif

#endif
