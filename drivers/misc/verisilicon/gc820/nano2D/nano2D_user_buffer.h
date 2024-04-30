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

#ifndef _nano2D_user_buffer_h_
#define _nano2D_user_buffer_h_

#ifdef __cplusplus
extern "C" {
#endif

#include "nano2D_driver_shared.h"
#include "nano2D_types.h"
#include "nano2D_enum.h"
#include "nano2D_user_base.h"


/* Command buffer object. */
/*
 * Initial (before put commands):
 *
 *    +-------------------------------------------
 * ...|0|
 *    +-------------------------------------------
 *    ^            ^
 *    |            |
 * startOffset   offset
 *
 *
 * After put command, in commit:
 *
 *    +------------------------------------------+
 * .. |0| .. commands .. |reservedTail| ..
 *    +------------------------------------------+
 *    ^                             ^
 *    |                             |
 * startOffset                    offset
 *
 *
 * Commit done, becomes initial state:
 *
 *    +------------------------------------------+-----------------
 * .. | .. commands .. |reservedTail|0           | ..
 *    +------------------------------------------+-----------------
 *                                               ^            ^
 *                                               |            |
 *                                          startOffset    offset
 *
 * reservedHead:
 * Select pipe commands.
 *
 * reservedTail:
 * Link
 *
 */
typedef struct _n2d_user_cmd_buf {
    n2d_uint32_t                    reserved_tail;
    n2d_uintptr_t                   handle;
    n2d_uintptr_t                   address;
    n2d_uintptr_t                   logical;
    n2d_size_t                      bytes;
    n2d_size_t                      start_offset;
    n2d_size_t                      offset;
    n2d_size_t                      free;
    n2d_uintptr_t                   last_reserve;
    n2d_size_t                      last_offset;
    n2d_pointer                     stall_signal;
    n2d_user_cmd_buf_t              *prev;
    n2d_user_cmd_buf_t              *next;
} n2d_user_cmd_buf;

typedef struct _n2d_user_cmd_buf_reserved_info {
    n2d_uint32_t                    alignment;
    n2d_uint32_t                    reserved_tail;
} n2d_user_cmd_buf_reserved_info;

typedef struct _n2d_user_cmd_location {
    n2d_uint64_t                    handle;
    n2d_uint64_t                    address;
    n2d_pointer                     logical;
    n2d_uint32_t                    offset;
    n2d_uint32_t                    size;
} n2d_user_cmd_location;

typedef struct _n2d_user_subcommit {
    n2d_uintptr_t                   event;
    n2d_user_cmd_location_t         cmd_buf;
} n2d_user_subcommit;

typedef struct _n2d_user_buffer {
    n2d_uint32_t                    count;
    n2d_uint32_t                    max_count;
    n2d_uint32_t                    bytes;
    n2d_uint32_t                    total_reserved;
    n2d_user_cmd_buf_t              *command_buffer_list;
    n2d_user_cmd_buf_t              *command_buffer_tail;
    n2d_user_cmd_buf_reserved_info  info;
} n2d_user_buffer;

n2d_error_t
gcInitCmdBuf(
    IN n2d_user_hardware_t  *hardware,
    IN n2d_size_t           max_size
    );

n2d_error_t
gcReserveCmdBuf(
    IN n2d_user_hardware_t  *hardware,
    IN n2d_size_t           bytes,
    OUT n2d_user_cmd_buf_t  **reserve
    );

n2d_error_t
gcCommitCmdBuf(
    IN n2d_user_hardware_t  *hardware
    );

n2d_error_t
gcDestroySurfaceCmdBuf(
    IN n2d_user_hardware_t  *hardware,
    IN n2d_user_buffer_t    *buffer
    );

#ifdef __cplusplus
}
#endif

#endif
