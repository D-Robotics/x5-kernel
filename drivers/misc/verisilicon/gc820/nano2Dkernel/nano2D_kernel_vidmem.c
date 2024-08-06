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

#include "nano2D_kernel_os.h"
#include "nano2D_kernel_vidmem.h"
#include "nano2D_driver_shared.h"

n2d_error_t n2d_kernel_vidmem_node_construct(n2d_kernel_t *kernel, n2d_vidmem_node_t **node)
{
	n2d_error_t error	 = N2D_SUCCESS;
	n2d_pointer pointer	 = N2D_NULL;
	n2d_vidmem_node_t *_node = N2D_NULL;

	ONERROR(n2d_kernel_os_allocate(kernel->os, sizeof(n2d_vidmem_node_t), &pointer));
	if (!pointer)
		ONERROR(N2D_OUT_OF_MEMORY);
	ONERROR(n2d_kernel_os_memory_fill(kernel->os, pointer, 0, sizeof(n2d_vidmem_node_t)));
	_node = (n2d_vidmem_node_t *)pointer;
	ONERROR(n2d_kernel_os_atom_construct(kernel->os, &_node->ref));
	ONERROR(n2d_kernel_os_atom_set(kernel->os, _node->ref, 1));
	ONERROR(n2d_kernel_os_mutex_create(kernel->os, &_node->mutex));
	N2D_INIT_LIST_HEAD(&_node->maps_head);
	_node->kernel = kernel;
	*node	      = _node;
	return error;

on_error:
	if (pointer)
		n2d_kernel_os_free(kernel->os, pointer);
	return error;
}

n2d_error_t n2d_kernel_vidmem_node_destroy(n2d_kernel_t *kernel, n2d_vidmem_node_t *node)
{
	if (!node)
		return N2D_INVALID_ARGUMENT;
	if (node->ref)
		n2d_kernel_os_atom_destroy(kernel->os, node->ref);
	if (node->mutex)
		n2d_kernel_os_mutex_delete(kernel->os, node->mutex);
	n2d_kernel_os_free(kernel->os, node);

	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_vidmem_node_inc(n2d_kernel_t *kernel, n2d_vidmem_node_t *node)
{
	n2d_int32_t old = 0;
	return n2d_kernel_os_atom_inc(kernel->os, node->ref, &old);
}

n2d_error_t n2d_kernel_vidmem_node_dec(n2d_kernel_t *kernel, n2d_vidmem_node_t *node)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_int32_t old	  = 0;

	ONERROR(n2d_kernel_os_atom_dec(kernel->os, node->ref, &old));
	if (old == 1) {
		; /* todo free the memory */
	}
on_error:
	return error;
}

n2d_error_t n2d_kernel_vidmem_node_map(n2d_kernel_t *kernel, n2d_uint32_t process,
				       n2d_map_type_t type, n2d_vidmem_node_t *node)
{
	n2d_error_t error    = N2D_SUCCESS;
	n2d_bool_t get_mutex = N2D_FALSE;

	ONERROR(n2d_kernel_os_mutex_acquire(kernel->os, node->mutex, N2D_INFINITE));

	get_mutex = N2D_TRUE;
	if ((type & N2D_MAP_TO_USER) && (!node->logical)) {
#if !defined(EMULATOR) && !defined(LINUXEMULATOR)
		n2d_mdl_map_t *mdl_map = n2d_kernel_vidmem_find_mdl_map(kernel, node, process);
		if (mdl_map) {
			node->logical = mdl_map->logical;
		} else {
			mdl_map = n2d_kernel_vidmem_create_mdl_map(kernel, node, process);
			ONERROR(n2d_kernel_os_map_cpu(kernel->os, process, node->handle,
						      node->klogical, node->size, node->flag, type,
						      (n2d_pointer *)&node->logical));
			mdl_map->logical = node->logical;
		}
		mdl_map->map_count++;
#else
		/* Emulator has mapped when allocate. */
		node->logical = (n2d_uintptr_t)node->klogical;
#endif
	}

	if ((type & N2D_MAP_TO_KERNEL) && (!node->klogical))
		ONERROR(n2d_kernel_os_map_cpu(kernel->os, process, node->handle, N2D_NULL,
					      node->size, node->flag, type, &node->klogical));

	if ((type & N2D_MAP_TO_GPU) && (!node->address)) {
		/* linux, because use share page table ,so the core is 0 */
		ONERROR(n2d_kernel_os_map_gpu(kernel->os, 0, node->handle, node->size, node->flag,
					      &node->address));
	}

	node->type = type;
	ONERROR(n2d_kernel_vidmem_node_inc(kernel, node));
	n2d_kernel_os_mutex_release(kernel->os, node->mutex);

	return error;
on_error:
	if (get_mutex)
		n2d_kernel_os_mutex_release(kernel->os, node->mutex);

	return error;
}

n2d_error_t n2d_kernel_vidmem_node_unmap(n2d_kernel_t *kernel, n2d_uint32_t process,
					 n2d_vidmem_node_t *node)
{
	n2d_error_t error	= N2D_SUCCESS;
	n2d_bool_t get_mutex	= N2D_FALSE;
	n2d_bool_t reserve_pool = N2D_FALSE;
	n2d_bool_t dma_pool	= N2D_FALSE;
	n2d_int32_t ref		= 0;

	ONERROR(n2d_kernel_os_atom_get(kernel->os, node->ref, &ref));
	/* If node ref is 1, no need to unmap, maybe double unmap in user layer. */
	if (ref == 1)
		ONERROR(N2D_INVALID_ARGUMENT);

	ONERROR(n2d_kernel_os_mutex_acquire(kernel->os, node->mutex, N2D_INFINITE));
	get_mutex = N2D_TRUE;
	if (node->type & N2D_MAP_TO_USER) {
		n2d_mdl_map_t *mdl_map = n2d_kernel_vidmem_find_mdl_map(kernel, node, process);
		if (mdl_map) {
			mdl_map->map_count--;
			if (mdl_map->map_count == 0 && mdl_map->logical) {
				ONERROR(n2d_kernel_os_unmap_cpu(kernel->os, process,
								(n2d_pointer)mdl_map->logical,
								node->size, node->type));
				ONERROR(n2d_kernel_vidmem_destroy_mdl_map(kernel, node, mdl_map));
			}
		}
	}

	reserve_pool = (node->flag & N2D_ALLOC_FLAG_RESERVED_ALLOCATOR) ==
		       N2D_ALLOC_FLAG_RESERVED_ALLOCATOR;
	dma_pool = (node->flag & N2D_ALLOC_FLAG_DMA_ALLOCATOR) == N2D_ALLOC_FLAG_DMA_ALLOCATOR;
	if ((node->type & N2D_MAP_TO_KERNEL) && (node->klogical)) {
		if (!reserve_pool && !dma_pool)
			ONERROR(n2d_kernel_os_unmap_cpu(kernel->os, process, node->klogical,
							node->size, node->type));
		node->klogical = N2D_NULL;
	}

	if ((node->type & N2D_MAP_TO_GPU) && (node->address)) {
		ONERROR(n2d_kernel_os_unmap_gpu(kernel->os, 0, node->address, node->size));
		node->address = 0;
	}
    if ((node->flag & N2D_ALLOC_FLAG_WRAP_USER) && (node->handle)) {
               ONERROR(n2d_kernel_os_free_wrapped_mem(kernel->os, node->flag, node->handle));
               node->handle = 0;
    }
	ONERROR(n2d_kernel_vidmem_node_dec(kernel, node));
on_error:
	if (get_mutex)
		n2d_kernel_os_mutex_release(kernel->os, node->mutex);
	return error;
}

n2d_error_t n2d_kernel_vidmem_allocate(n2d_kernel_t *kernel, n2d_uint32_t u_size,
				       n2d_uint32_t alignment, n2d_uint32_t flag,
				       n2d_vidmem_type_t type, n2d_vidmem_pool_t pool_name,
				       n2d_vidmem_node_t **node)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_bool_t iommu;
	n2d_pointer memory	 = N2D_NULL;
	n2d_uint64_t physical	 = 0;
	n2d_vidmem_node_t *_node = N2D_NULL;
	n2d_uint32_t size	 = u_size;
	n2d_pointer pool	 = N2D_NULL;
	n2d_bool_t skip		 = N2D_FALSE;

	iommu = kernel->device->iommu;

	if (pool_name == N2D_POOL_UNKNOWN)
		pool_name = N2D_POOL_SYSTEM;
	else
		skip = N2D_TRUE;

#if !NANO2D_MMU_ENABLE
	if (!kernel->sub_dev[0]->hardware[0]->support_64bit)
		flag |= N2D_ALLOC_FLAG_4G;
#endif

	ONERROR(n2d_kernel_vidmem_node_construct(kernel, &_node));

	if (!iommu) {
		/*
		 * VIV: When alloc contiguous memory, we follow the order:
		 *      reserved memory >> dma allocator >> alloc_pages.
		 */
		if (flag & N2D_ALLOC_FLAG_CONTIGUOUS) {
			ONERROR(n2d_kernel_os_get_contiguous_pool(kernel->os, pool_name, &pool));
			error = n2d_kernel_os_allocate_contiguous(kernel->os, pool, &size, flag,
								  &memory, &physical, alignment);
			if (N2D_IS_SUCCESS(error)) {
				flag |= N2D_ALLOC_FLAG_RESERVED_ALLOCATOR;
				_node->klogical = memory;
				_node->pool	= pool;
			} else if (error == N2D_OUT_OF_MEMORY && !skip) {
				error = n2d_kernel_os_dma_alloc(kernel->os, &size, flag, &memory,
								&physical);
				if (N2D_IS_SUCCESS(error)) {
					flag |= N2D_ALLOC_FLAG_DMA_ALLOCATOR;
					/* CPU logical in kernel space. */
					_node->klogical = memory;
				} else if (error == N2D_OUT_OF_MEMORY && !skip) {
					error = n2d_kernel_os_allocate_paged_memory(
						kernel->os, &size, flag, &physical);
					if (N2D_IS_SUCCESS(error)) {
						flag |= N2D_ALLOC_FLAG_GFP_ALLOCATOR;
						_node->klogical = N2D_NULL;
					} else {
						n2d_kernel_os_print(
							"Allocate contiguous memory fail!\n");
						goto on_error;
					}
				}
			}
			_node->handle = physical;
		} else if (flag & N2D_ALLOC_FLAG_NOCONTIGUOUS) {
			/* alloc non-contiguous memory from GFP. */
			ONERROR(n2d_kernel_os_allocate_noncontiguous(kernel->os, &size, flag,
								     &physical));
			_node->klogical = N2D_NULL;
			flag |= N2D_ALLOC_FLAG_GFP_ALLOCATOR;
			/* This physical is non-contiguous desc pointer. */
			_node->handle = physical;
		} else {
			ONERROR(N2D_INVALID_ARGUMENT);
		}
	} else {
		/* Only use dma_alloc_* to alloc memory when enable IOMMU */
		ONERROR(n2d_kernel_os_dma_alloc(kernel->os, &size, flag, &memory, &physical));

		flag |= N2D_ALLOC_FLAG_DMA_ALLOCATOR;
		flag |= N2D_ALLOC_FLAG_CONTIGUOUS;
		/* CPU logical in kernel space. */
		_node->klogical = memory;
		/* dma_handle */
		_node->handle = physical;
	}

	_node->size = size;
	_node->flag = flag;
	_node->type = N2D_MAP_UNKNOWN;
	*node	    = _node;

	return error;
on_error:
	if (memory)
		n2d_kernel_os_free_contiguous(kernel->os, _node->pool, memory, physical);
	if (_node)
		n2d_kernel_vidmem_node_destroy(kernel, _node);
	return error;
}

n2d_error_t n2d_kernel_vidmem_free(n2d_kernel_t *kernel, n2d_vidmem_node_t *node)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_bool_t iommu;
	n2d_int32_t ref = 0;

	iommu = kernel->device->iommu;

	ONERROR(n2d_kernel_os_atom_get(kernel->os, node->ref, &ref));
	if (ref > 1)
		ONERROR(N2D_INVALID_ARGUMENT);

	if (!iommu) {
		if (node->flag & N2D_ALLOC_FLAG_RESERVED_ALLOCATOR)
			ONERROR(n2d_kernel_os_free_contiguous(kernel->os, node->pool,
							      node->klogical, node->handle));
		else if (node->flag & N2D_ALLOC_FLAG_DMA_ALLOCATOR)
			ONERROR(n2d_kernel_os_dma_free(kernel->os, node->size, node->klogical,
						       node->handle));
		else if (node->flag & N2D_ALLOC_FLAG_WRAP_USER)
			ONERROR(n2d_kernel_os_free_wrapped_mem(kernel->os, node->flag,
							       node->handle));
		else
			ONERROR(n2d_kernel_os_free_paged_memory(kernel->os, node->size, node->flag,
								node->handle));
	} else {
		ONERROR(n2d_kernel_os_dma_free(kernel->os, node->size, node->klogical,
					       node->handle));
	}

	ONERROR(n2d_kernel_vidmem_node_destroy(kernel, node));

on_error:
	return error;
}

n2d_error_t n2d_kernel_vidmem_node_get_physical(n2d_kernel_t *kernel, n2d_vidmem_node_t *node,
						n2d_uint64_t *physical)
{
	n2d_error_t error = N2D_SUCCESS;

	gcmkASSERT(kernel != N2D_NULL);

	if (node->flag & N2D_ALLOC_FLAG_CONTIGUOUS || node->flag & N2D_ALLOC_FLAG_WRAP_USER)
		ONERROR(n2d_kernel_os_get_physical_from_handle(node->handle, 0, node->flag,
							       physical));
	else
		*physical = N2D_INVALID_ADDRESS;

on_error:
	return error;
}

gcmINLINE n2d_mdl_map_t *n2d_kernel_vidmem_create_mdl_map(n2d_kernel_t *kernel,
							  n2d_vidmem_node_t *node,
							  n2d_uint32_t process)
{
	n2d_mdl_map_t *mdl_map = N2D_NULL;
	n2d_error_t error      = N2D_SUCCESS;
	n2d_pointer pointer    = N2D_NULL;

	ONERROR(n2d_kernel_os_allocate(kernel->os, sizeof(struct n2d_mdl_map), &pointer));
	mdl_map = (n2d_mdl_map_t *)pointer;
	if (mdl_map == N2D_NULL) {
		ONERROR(N2D_OUT_OF_MEMORY);
	}

	mdl_map->process   = process;
	mdl_map->logical   = N2D_ZERO;
	mdl_map->map_count = 0;

	n2d_list_add(&mdl_map->link, &node->maps_head);

on_error:
	return mdl_map;
}

n2d_error_t n2d_kernel_vidmem_destroy_mdl_map(n2d_kernel_t *kernel, n2d_vidmem_node_t *node,
					      n2d_mdl_map_t *mdl_map)
{
	gcmkASSERT(node != N2D_NULL);

	n2d_list_del(&mdl_map->link);
	n2d_kernel_os_free(kernel->os, mdl_map);

	return N2D_SUCCESS;
}

n2d_mdl_map_t *n2d_kernel_vidmem_find_mdl_map(n2d_kernel_t *kernel, n2d_vidmem_node_t *node,
					      n2d_uint32_t process)
{
	n2d_mdl_map_t *mdl_map = N2D_NULL;

	gcmkASSERT(kernel != N2D_NULL);
	gcmkASSERT(node != N2D_NULL);
	gcmkASSERT(process >= 0);

#if !defined(EMULATOR) && !defined(LINUXEMULATOR)
	if (node) {
		n2d_mdl_map_t *iter = N2D_NULL;
		n2d_list_for_each_entry(iter, &node->maps_head, link)
		{
			if (iter->process == process) {
				mdl_map = iter;
				break;
			}
		}
	}
#endif

	return mdl_map;
}

n2d_error_t n2d_kernel_vidmem_export(n2d_kernel_t *kernel, n2d_vidmem_node_t *node,
				     n2d_uint32_t flags, n2d_pointer *dmabuf, n2d_int32_t *fd)
{
	n2d_error_t error = N2D_SUCCESS;

	ONERROR(n2d_kernel_os_vidmem_export(kernel->os, node, flags, dmabuf, fd));
	n2d_kernel_vidmem_node_inc(kernel, node);

on_error:
	return error;
}

n2d_error_t n2d_kernel_vidmem_cache_op(n2d_kernel_t *kernel, n2d_vidmem_node_t *node,
				       n2d_cache_op_t op)
{
	n2d_error_t error = N2D_SUCCESS;

	/*
	 * TODO:
	 * Now only wrapped user memory is cacheable. Need to add other node's cache op.
	 */
	if (node->cacheable) {
		switch (op) {
		case N2D_CACHE_FLUSH:
			error = n2d_kernel_os_cache_flush(kernel->os, node->handle);
			break;
		case N2D_CACHE_CLEAN:
			error = n2d_kernel_os_cache_clean(kernel->os, node->handle);
			break;
		case N2D_CACHE_INVALIDATE:
			error = n2d_kernel_os_cache_invalidate(kernel->os, node->handle);
			break;
		case N2D_CACHE_MEMORY_BARRIER:
			error = n2d_kernel_os_memory_barrier(kernel->os, N2D_NULL);
			break;
		default:
			ONERROR(N2D_INVALID_ARGUMENT);
			break;
		}
	}

#if NANO2D_PCIE_ORDER
	/* PCIe order-preserving mode, need ensure data is written into DDR by reading. */
	ONERROR(n2d_kernel_os_memory_barrier(kernel->os, node->klogical));
#endif

on_error:
	return error;
}
