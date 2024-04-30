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

#ifndef _nano2d_kernel_vidmem_h_
#define _nano2d_kernel_vidmem_h_

#include "nano2D_kernel_common.h"
#include "nano2D_kernel_hardware.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct n2d_mdl_map {
	n2d_uint32_t process;
	n2d_uintptr_t logical;
	n2d_uint32_t map_count;
	struct n2d_list_head link;
} n2d_mdl_map_t;

typedef struct n2d_vidmem_node {
	n2d_kernel_t *kernel;

	n2d_uint64_t address;
	n2d_uintptr_t logical;
	n2d_pointer klogical;
	/* handle: store the physical address or sgt table or page list */
	n2d_uint64_t handle;
	/* Export video memory node to dma_buf fd. */
	n2d_pointer dmabuf;
	n2d_uint32_t size;
	n2d_uint32_t flag;
	n2d_bool_t cacheable;
	n2d_uint32_t secure;
	n2d_map_type_t type;
	n2d_pointer ref;
	n2d_pointer mutex;
	struct n2d_list_head maps_head;
	n2d_pointer info;
	n2d_pointer pool;
} n2d_vidmem_node_t;

n2d_error_t n2d_kernel_vidmem_node_construct(n2d_kernel_t *kernel, n2d_vidmem_node_t **node);

n2d_error_t n2d_kernel_vidmem_node_destroy(n2d_kernel_t *kernel, n2d_vidmem_node_t *node);

n2d_error_t n2d_kernel_vidmem_node_inc(n2d_kernel_t *kernel, n2d_vidmem_node_t *node);

n2d_error_t n2d_kernel_vidmem_node_dec(n2d_kernel_t *kernel, n2d_vidmem_node_t *node);

n2d_error_t n2d_kernel_vidmem_node_map(n2d_kernel_t *kernel, n2d_uint32_t process,
				       n2d_map_type_t type, n2d_vidmem_node_t *node);

n2d_error_t n2d_kernel_vidmem_node_unmap(n2d_kernel_t *kernel, n2d_uint32_t process,
					 n2d_vidmem_node_t *node);

n2d_error_t n2d_kernel_vidmem_allocate(n2d_kernel_t *kernel, n2d_uint32_t u_size,
				       n2d_uint32_t alignment, n2d_uint32_t flag,
				       n2d_vidmem_type_t type, n2d_vidmem_pool_t pool_name,
				       n2d_vidmem_node_t **node);

n2d_error_t n2d_kernel_vidmem_free(n2d_kernel_t *kernel, n2d_vidmem_node_t *node);

n2d_error_t n2d_kernel_vidmem_node_get_physical(n2d_kernel_t *kernel, n2d_vidmem_node_t *node,
						n2d_uint64_t *physical);

n2d_mdl_map_t *n2d_kernel_vidmem_create_mdl_map(n2d_kernel_t *kernel, n2d_vidmem_node_t *node,
						n2d_uint32_t process);

n2d_error_t n2d_kernel_vidmem_destroy_mdl_map(n2d_kernel_t *kernel, n2d_vidmem_node_t *node,
					      n2d_mdl_map_t *mdl_map);

n2d_mdl_map_t *n2d_kernel_vidmem_find_mdl_map(n2d_kernel_t *kernel, n2d_vidmem_node_t *node,
					      n2d_uint32_t process);

n2d_error_t n2d_kernel_vidmem_export(n2d_kernel_t *kernel, n2d_vidmem_node_t *node,
				     n2d_uint32_t flags, n2d_pointer *dmabuf, n2d_int32_t *fd);

n2d_error_t n2d_kernel_vidmem_cache_op(n2d_kernel_t *kernel, n2d_vidmem_node_t *node,
				       n2d_cache_op_t op);

#ifdef __cplusplus
}
#endif

#endif /* _nano2d_kernel_vidmem_h_ */
