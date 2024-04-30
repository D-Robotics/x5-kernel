/****************************************************************************
 *
 *    The MIT License (MIT)
 *
 *    Copyright (c) 2012 - 2022 Vivante Corporation
 *
 *    Permission is hereby granted, free of charge, to any person obtaining a
 *    copy of this software and associated documentation files (the "Software"),
 *    to deal in the Software without restriction, including without limitation
 *    the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *    and/or sell copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following conditions:
 *
 *    The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *    DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 *
 *    The GPL License (GPL)
 *
 *    Copyright (C) 2012 - 2022 Vivante Corporation
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version 2
 *    of the License, or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *****************************************************************************
 *
 *    Note: This software is released under dual MIT and GPL licenses. A
 *    recipient may use this file under the terms of either the MIT license or
 *    GPL License. If you wish to use only one license not the other, you can
 *    indicate your decision by deleting one of the above license notices in your
 *    version of this file.
 *
 *****************************************************************************/

#ifndef _nano2d_kernel_db_h_
#define _nano2d_kernel_db_h_

#include "nano2D_kernel_common.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef enum n2d_db_type {
	N2D_UNKONW_TYPE,
	N2D_VIDMEM_TYPE,
	N2D_SIGNAL_TYPE,
} n2d_db_type_t;

typedef struct n2d_db_process_node {
	n2d_uint32_t process;
	n2d_int32_t allocate_count;
	n2d_hash_map_t *hash_map;
	struct n2d_db_process_node *next;
} n2d_db_process_node_t;

struct n2d_db {
	n2d_kernel_t *kernel;
	n2d_handle_bitmap_t *handle_bitmap;
	n2d_db_process_node_t *db_list;
	n2d_pointer mutex;
};

n2d_error_t n2d_kernel_db_get_handle(n2d_db_t *db, n2d_uint32_t *handle);

n2d_error_t n2d_kernel_db_free_handle(n2d_db_t *db, n2d_uint32_t handle);

n2d_error_t n2d_db_get_process_allocate_count(n2d_db_t *db, n2d_uint32_t process,
					      n2d_int32_t *count);

n2d_error_t n2d_kernel_db_insert(n2d_db_t *db, n2d_uint32_t process, n2d_uint32_t handle,
				 n2d_db_type_t type, n2d_uintptr_t data);

n2d_error_t n2d_kernel_db_remove(n2d_db_t *db, n2d_uint32_t process, n2d_uint32_t handle);

n2d_error_t n2d_kernel_db_get_remove(n2d_db_t *db, n2d_uint32_t process, n2d_uint32_t handle,
				     n2d_db_type_t *type, n2d_uintptr_t *data);

n2d_error_t n2d_kernel_db_get(n2d_db_t *db, n2d_uint32_t process, n2d_uint32_t handle,
			      n2d_db_type_t *type, n2d_uintptr_t *data);

n2d_error_t n2d_kernel_db_attach_process(n2d_db_t *db, n2d_uint32_t process, n2d_bool_t flag);

n2d_error_t n2d_kernel_db_construct(n2d_kernel_t *kernel, n2d_db_t **db);

n2d_error_t n2d_kernel_db_destroy(n2d_kernel_t *kernel, n2d_db_t *db);

#ifdef __cplusplus
}
#endif

#endif /* _nano2d_kernel_db_h_ */
