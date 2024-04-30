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
#include "nano2D_kernel_hardware.h"
#include "nano2D_kernel_mmu.h"
#include "nano2D_driver_shared.h"

#if NANO2D_MMU_ENABLE
static n2d_mmu_config_t *global_mmu_config = N2D_NULL;

n2d_error_t _n2d_mmu_get_config(IN n2d_mmu_config_t **config)
{
	*config = global_mmu_config;

	return N2D_SUCCESS;
}

static n2d_uint32_t _mtlb_offset(n2d_uint32_t address)
{
	return (address & n2dMMU_MTLB_MASK) >> n2dMMU_MTLB_SHIFT;
}

static n2d_uint32_t _stlb_offset(n2d_uint32_t address)
{
	return (address & n2dMMU_STLB_4K_MASK) >> n2dMMU_STLB_4K_SHIFT;
}

static n2d_uint32_t _address_to_index(n2d_uint32_t address)
{
	return _mtlb_offset(address) * n2dMMU_STLB_4K_ENTRY_NUM + _stlb_offset(address);
}

static n2d_uint32_t _set_page(n2d_uint32_t page_address, n2d_uint32_t page_address_ext)
{
	return page_address |
	       (page_address_ext << 4)
	       /* writable */
	       | (1 << 2)
	       /*0 Ignore exception , 1 catch the exception*/
	       | (1 << 1)
	       /* Present */
	       | (1 << 0);
}

static n2d_void _write_page_entry(n2d_uint32_t *page_entry, n2d_uint32_t entry_value)
{
	*page_entry = entry_value;
}

static n2d_uint32_t _read_page_entry(n2d_uint32_t *page_entry)
{
	return *page_entry;
}

n2d_error_t _allocate_stlb(n2d_mmu_t *mmu, n2d_mmu_stlb_pt *stlb)
{
	n2d_mmu_stlb_pt _stlb = N2D_NULL;
	n2d_pointer pointer;
	n2d_error_t error;
	n2d_os_t *os		     = mmu->hardware->os;
	n2d_vidmem_node_t *stlb_node = N2D_NULL;
	n2d_uint64_t cpu_physical    = N2D_INVALID_ADDRESS;

	ONERROR(n2d_kernel_os_allocate(os, sizeof(n2d_mmu_stlb), &pointer));

	_stlb = (n2d_mmu_stlb_pt)pointer;

	_stlb->size	    = n2dMMU_STLB_4K_SIZE;
	_stlb->stlb_entries = n2dMMU_STLB_4K_ENTRY_NUM;

	ONERROR(n2d_kernel_vidmem_allocate(mmu->hardware->kernel, _stlb->size, 64,
					   N2D_ALLOC_FLAG_CONTIGUOUS, 0, 0, &stlb_node));
	ONERROR(n2d_kernel_vidmem_node_map(mmu->hardware->kernel, 0, N2D_MAP_TO_KERNEL, stlb_node));
	_stlb->logical = stlb_node->klogical;

	/* Get CPU physical address. */
	ONERROR(n2d_kernel_os_get_physical_from_handle(stlb_node->handle, 0, stlb_node->flag,
						       &cpu_physical));
	/* Get GPU physical from CPU physical. */
	ONERROR(n2d_kernel_os_cpu_to_gpu_phy(os, cpu_physical, &_stlb->phys_base));

	_stlb->stlb_node = stlb_node;

	if (_stlb->phys_base & 0x3F)
		ONERROR(N2D_NOT_ALIGNED);

	n2d_kernel_os_memory_fill(os, _stlb->logical, 0, _stlb->size);

	*stlb = _stlb;

	return N2D_SUCCESS;

on_error:
	if (stlb_node) {
		if (stlb_node->klogical)
			n2d_kernel_vidmem_node_unmap(mmu->hardware->kernel, 0, stlb_node);
		n2d_kernel_vidmem_free(mmu->hardware->kernel, stlb_node);
	}
	if (_stlb)
		n2d_kernel_os_free(os, _stlb);

	return error;
}

n2d_error_t _setup_process_address_space(n2d_mmu_t *mmu)
{
	n2d_error_t error;
	n2d_uint32_t *map;
	n2d_uint32_t flatmapping_entries;
	n2d_uint32_t free;
	n2d_uint32_t i;
	n2d_os_t *os = mmu->hardware->os;

	mmu->config->dynamic_mapping_start = 0;

	mmu->config->page_table_size = n2dMMU_STLB_4K_ENTRY_NUM * n2dMMU_PAGE_4K_SIZE;

	mmu->config->page_table_entries = mmu->config->page_table_size / sizeof(n2d_uint32_t);

	ONERROR(n2d_kernel_os_allocate(os, mmu->config->page_table_size,
				       (n2d_pointer *)&mmu->config->map_logical));

	map = mmu->config->map_logical;

	flatmapping_entries = 0;

	/* Cross over flat mapping area */
	map += flatmapping_entries;

	/* Initialize free area*/
	free = mmu->config->page_table_entries - flatmapping_entries;
	_write_page_entry(map, (free << 8) | n2dMMU_FREE);
	_write_page_entry(map + 1, ~0U);

	/* Initialize flat mapping area */

	mmu->config->heap_list	= flatmapping_entries;
	mmu->config->free_nodes = N2D_FALSE;

	for (i = 0; i < mmu->config->mtlb_entries; i++) {
		n2d_uint32_t mtlb_entry;
		n2d_mmu_stlb_pt stlb;
		n2d_mmu_stlb_pt *stlbs = (n2d_mmu_stlb_pt *)mmu->config->stlbs;

		ONERROR(_allocate_stlb(mmu, &stlb));

		mtlb_entry = stlb->phys_base | n2dMMU_MTLB_4K_PAGE | n2dMMU_MTLB_PRESENT |
			     (1 << 1) /* catch exception*/
			;

		/* Insert Slave TLB address to Master TLB entry.*/
		_write_page_entry((n2d_uint32_t *)mmu->config->mtlb_logical + i, mtlb_entry);

		stlbs[i] = stlb;
	}

	return N2D_SUCCESS;

on_error:
	if (mmu->config->map_logical)
		n2d_kernel_os_free(os, mmu->config->map_logical);
	return error;
}

n2d_error_t _n2d_mmu_create_config(n2d_mmu_t *mmu)
{
	n2d_error_t error;
	n2d_pointer pointer;
	n2d_mmu_config_t *config	   = N2D_NULL;
	n2d_os_t *os			   = mmu->hardware->os;
	n2d_vidmem_node_t *mtlb_node	   = N2D_NULL;
	n2d_vidmem_node_t *safe_page_node  = N2D_NULL;
	n2d_vidmem_node_t *page_array_node = N2D_NULL;
	n2d_uint64_t cpu_physical	   = N2D_INVALID_ADDRESS;
	n2d_uint32_t flag		   = N2D_ALLOC_FLAG_CONTIGUOUS;

	n2d_vidmem_node_t *node = N2D_NULL;
	n2d_int32_t buf_size	= 256;

	ONERROR(n2d_kernel_os_allocate(os, sizeof(n2d_mmu_config_t), &pointer));
	n2d_kernel_os_memory_fill(os, pointer, 0, sizeof(n2d_mmu_config_t));

	config		  = (n2d_mmu_config_t *)pointer;
	mmu->config	  = config;
	config->ref_count = 1;

	{
		config->init_mode    = n2dMMU_INIT_FROM_CMD;
		config->mtlb_bytes   = n2dMMU_MTLB_SIZE;
		config->mtlb_entries = config->mtlb_bytes / sizeof(n2d_uint32_t);

		/* Allocate MTLB */
		ONERROR(n2d_kernel_vidmem_allocate(mmu->hardware->kernel, config->mtlb_bytes, 4096,
						   flag, 0, 0, &mtlb_node));
		ONERROR(n2d_kernel_vidmem_node_map(mmu->hardware->kernel, 0, N2D_MAP_TO_KERNEL,
						   mtlb_node));
		config->mtlb_logical = mtlb_node->klogical;

		/* Get CPU physical address. */
		ONERROR(n2d_kernel_os_get_physical_from_handle(mtlb_node->handle, 0,
							       mtlb_node->flag, &cpu_physical));
		/* Get GPU physical from CPU physical. */
		ONERROR(n2d_kernel_os_cpu_to_gpu_phy(os, cpu_physical, &config->mtlb_physical));

		config->mtlb_node = mtlb_node;

		if (config->mtlb_physical & 0xFFF)
			ONERROR(N2D_NOT_ALIGNED);

		ONERROR(n2d_kernel_os_allocate(os, sizeof(n2d_pointer) * config->mtlb_entries,
					       &config->stlbs));

		n2d_kernel_os_memory_fill(os, (n2d_uint8_t *)config->stlbs, 0,
					  config->mtlb_entries * sizeof(n2d_pointer));

		ONERROR(_setup_process_address_space(mmu));

		/* Allocate safe page */
		ONERROR(n2d_kernel_vidmem_allocate(mmu->hardware->kernel, 4096, 64, flag, 0, 0,
						   &safe_page_node));
		ONERROR(n2d_kernel_vidmem_node_map(mmu->hardware->kernel, 0, N2D_MAP_TO_KERNEL,
						   safe_page_node));
		config->safe_page_logical = safe_page_node->klogical;

		ONERROR(n2d_kernel_os_get_physical_from_handle(
			safe_page_node->handle, 0, safe_page_node->flag, &cpu_physical));
		ONERROR(n2d_kernel_os_cpu_to_gpu_phy(os, cpu_physical,
						     &config->safe_page_physical));

		config->safe_page_node = safe_page_node;

		if (config->safe_page_physical & 0x3F)
			ONERROR(N2D_NOT_ALIGNED);

		/* Allocate page table array*/
		ONERROR(n2d_kernel_vidmem_allocate(mmu->hardware->kernel, 1024, 64, flag, 0, 0,
						   &page_array_node));
		ONERROR(n2d_kernel_vidmem_node_map(mmu->hardware->kernel, 0, N2D_MAP_TO_KERNEL,
						   page_array_node));
		config->page_table_array_logical = page_array_node->klogical;

		ONERROR(n2d_kernel_os_get_physical_from_handle(
			page_array_node->handle, 0, page_array_node->flag, &cpu_physical));
		ONERROR(n2d_kernel_os_cpu_to_gpu_phy(os, cpu_physical,
						     &config->page_table_array_physical));

		config->page_array_node = page_array_node;

		if (config->page_table_array_physical & 0x3F)
			ONERROR(N2D_NOT_ALIGNED);

		/* Allocate mmu init buffer node */
		if (!mmu->hardware->mmu_pd_mode)
			flag |= N2D_ALLOC_FLAG_4G;
		ONERROR(n2d_kernel_vidmem_allocate(mmu->hardware->kernel, buf_size, 64, flag,
						   N2D_TYPE_COMMAND, 0, &node));
		ONERROR(n2d_kernel_vidmem_node_map(mmu->hardware->kernel, 0, N2D_MAP_TO_KERNEL,
						   node));
		mmu->config->mmu_init_buffer_logical = node->klogical;

		ONERROR(n2d_kernel_os_get_physical_from_handle(node->handle, 0, node->flag,
							       &cpu_physical));
		ONERROR(n2d_kernel_os_cpu_to_gpu_phy(os, cpu_physical,
						     &mmu->config->mmu_init_buffer_physical));

		mmu->config->node = node;

		ONERROR(n2d_kernel_os_mutex_create(os, &config->mutex));

		ONERROR(n2d_kernel_os_atom_construct(os, &config->page_dirty));

		ONERROR(n2d_kernel_os_atom_set(os, config->page_dirty, 1));
	}

	return N2D_SUCCESS;

on_error:
	if (mtlb_node) {
		if (mtlb_node->klogical)
			n2d_kernel_vidmem_node_unmap(mmu->hardware->kernel, 0, mtlb_node);
		n2d_kernel_vidmem_free(mmu->hardware->kernel, mtlb_node);
		config->mtlb_node = N2D_NULL;
	}
	if (safe_page_node) {
		if (safe_page_node->klogical)
			n2d_kernel_vidmem_node_unmap(mmu->hardware->kernel, 0, safe_page_node);
		n2d_kernel_vidmem_free(mmu->hardware->kernel, safe_page_node);
		config->safe_page_node = N2D_NULL;
	}
	if (page_array_node) {
		if (page_array_node->klogical)
			n2d_kernel_vidmem_node_unmap(mmu->hardware->kernel, 0, page_array_node);
		n2d_kernel_vidmem_free(mmu->hardware->kernel, page_array_node);
		config->page_array_node = N2D_NULL;
	}
	if (mmu->config->node) {
		if (mmu->config->node->klogical)
			n2d_kernel_vidmem_node_unmap(mmu->hardware->kernel, 0, mmu->config->node);
		n2d_kernel_vidmem_free(mmu->hardware->kernel, mmu->config->node);
	}
	if (config && config->stlbs)
		n2d_kernel_os_free(os, config->stlbs);
	if (config)
		n2d_kernel_os_free(os, config);

	return error;
}

/* MMU Construct */
n2d_error_t n2d_mmu_construct(n2d_hardware_t *hardware, n2d_mmu_t *mmu)
{
	n2d_error_t error	       = N2D_SUCCESS;
	n2d_mmu_config_t *share_config = N2D_NULL;

	if (mmu->config)
		return N2D_SUCCESS;

	mmu->hardware = hardware;
	ONERROR(_n2d_mmu_get_config(&share_config));
	if (share_config) {
		mmu->config = share_config;
		mmu->config->ref_count++;
	} else {
		ONERROR(_n2d_mmu_create_config(mmu));
		global_mmu_config = mmu->config;
	}

	return N2D_SUCCESS;

on_error:
	return error;
}

n2d_error_t n2d_mmu_destroy(n2d_mmu_t *mmu)
{
	n2d_error_t error      = N2D_SUCCESS;
	n2d_uint32_t i	       = 0;
	n2d_mmu_stlb_pt *stlbs = (n2d_mmu_stlb_pt *)mmu->config->stlbs;
	n2d_mmu_stlb_pt stlb;
	n2d_os_t *os = mmu->hardware->os;

	mmu->config->ref_count--;

	if (mmu->config->ref_count == 0) {
		for (i = 0; i < mmu->config->mtlb_entries; i++) {
			stlb = stlbs[i];
			if (stlb && stlb->stlb_node) {
				if (stlb->stlb_node->klogical)
					ONERROR(n2d_kernel_vidmem_node_unmap(mmu->hardware->kernel,
									     0, stlb->stlb_node));
				ONERROR(n2d_kernel_vidmem_free(mmu->hardware->kernel,
							       stlb->stlb_node));
				stlb->stlb_node = N2D_NULL;
			}
			n2d_kernel_os_free(mmu->hardware->os, stlb);
			stlbs[i] = N2D_NULL;
		}

		if (mmu->config->map_logical)
			n2d_kernel_os_free(mmu->hardware->os, mmu->config->map_logical);

		if (stlbs) {
			n2d_kernel_os_free(mmu->hardware->os, stlbs);
			mmu->config->stlbs = N2D_NULL;
		}

		if (mmu->config->mtlb_node) {
			if (mmu->config->mtlb_node->klogical)
				ONERROR(n2d_kernel_vidmem_node_unmap(mmu->hardware->kernel, 0,
								     mmu->config->mtlb_node));
			ONERROR(n2d_kernel_vidmem_free(mmu->hardware->kernel,
						       mmu->config->mtlb_node));
			mmu->config->mtlb_node = N2D_NULL;
		}

		if (mmu->config->safe_page_node) {
			if (mmu->config->safe_page_node->klogical)
				ONERROR(n2d_kernel_vidmem_node_unmap(mmu->hardware->kernel, 0,
								     mmu->config->safe_page_node));
			ONERROR(n2d_kernel_vidmem_free(mmu->hardware->kernel,
						       mmu->config->safe_page_node));
			mmu->config->safe_page_node = N2D_NULL;
		}

		if (mmu->config->page_array_node) {
			if (mmu->config->page_array_node->klogical)
				ONERROR(n2d_kernel_vidmem_node_unmap(mmu->hardware->kernel, 0,
								     mmu->config->page_array_node));
			ONERROR(n2d_kernel_vidmem_free(mmu->hardware->kernel,
						       mmu->config->page_array_node));
			mmu->config->page_array_node = N2D_NULL;
		}

		if (mmu->config->node) {
			if (mmu->config->node->klogical)
				ONERROR(n2d_kernel_vidmem_node_unmap(mmu->hardware->kernel, 0,
								     mmu->config->node));
			ONERROR(n2d_kernel_vidmem_free(mmu->hardware->kernel, mmu->config->node));
			mmu->config->node = N2D_NULL;
		}

		if (mmu->config->mutex) {
			n2d_kernel_os_mutex_delete(os, mmu->config->mutex);
			mmu->config->mutex = N2D_NULL;
		}

		if (mmu->config->page_dirty) {
			n2d_kernel_os_atom_destroy(os, mmu->config->page_dirty);
			mmu->config->page_dirty = N2D_NULL;
		}

		if (mmu->config) {
			ONERROR(n2d_kernel_os_free(os, mmu->config));
			mmu->config = N2D_NULL;
		}
	} else {
		return N2D_SUCCESS;
	}

	return N2D_SUCCESS;
on_error:
	return error;
}

n2d_error_t n2d_mmu_dump_page_table(n2d_mmu_t *mmu)
{
	n2d_error_t error	   = N2D_SUCCESS;
	n2d_uint32_t *mtlb_logical = N2D_NULL;
	n2d_uint32_t *stlb_logical = N2D_NULL;
	n2d_uint32_t mtlb_entry, stlb_entry;
	n2d_uint64_t page_physical = ~0ull;
	n2d_mmu_stlb_pt stlb_desc  = N2D_NULL;
	n2d_mmu_stlb_pt *stlbs	   = (n2d_mmu_stlb_pt *)mmu->config->stlbs;
	n2d_uint32_t i = 0, j = 0;

	mtlb_logical = mmu->config->mtlb_logical;

	n2d_kernel_os_print("MMU page table:\n");
	n2d_kernel_os_print("  MTLB physical base address: 0x%llX\n", mmu->config->mtlb_physical);
	for (i = 0; i < mmu->config->mtlb_entries; i++) {
		mtlb_entry = *(mtlb_logical + i);
		n2d_kernel_os_print("  MTLB entry[%d]: 0x%08X\n", i, mtlb_entry);
		stlb_desc = stlbs[i];
		if (stlb_desc) {
			n2d_kernel_os_print("    STLB physical base address: 0x%llX\n",
					    stlb_desc->phys_base);
			stlb_logical = stlb_desc->logical;
			for (j = 0; j < stlb_desc->stlb_entries; j++) {
				stlb_entry = *(stlb_logical + j);
				if (stlb_entry) {
					page_physical = ((n2d_uint64_t)stlb_entry & 0XFF0) << 28 |
							((n2d_uint64_t)stlb_entry & ~0XFFF);
					n2d_kernel_os_print("    STLB entry[%d] -> phy addr: "
							    "0x%08X -> 0x%010lX\n",
							    j, stlb_entry, page_physical);
				}
			}
		}
		stlb_entry = 0;
		stlb_desc  = N2D_NULL;
	}
	return error;
}

n2d_error_t n2d_mmu_get_page_entry(n2d_mmu_t *mmu, n2d_uint32_t address, n2d_uint32_t **page_table)
{
	n2d_error_t error;
	n2d_mmu_stlb_pt stlb;
	n2d_mmu_stlb_pt *stlbs	 = (n2d_mmu_stlb_pt *)mmu->config->stlbs;
	n2d_uint32_t mtlb_offset = _mtlb_offset(address);
	n2d_uint32_t stlb_offset = _stlb_offset(address);
	n2d_uint32_t mtlb_entry;

	stlb = stlbs[mtlb_offset];

	if (stlb == N2D_NULL) {
		ONERROR(_allocate_stlb(mmu, &stlb));

		mtlb_entry = stlb->phys_base | n2dMMU_MTLB_4K_PAGE | n2dMMU_MTLB_PRESENT;

		_write_page_entry((n2d_uint32_t *)mmu->config->mtlb_logical + mtlb_offset,
				  mtlb_entry);

		stlbs[mtlb_offset] = stlb;
	}

	*page_table = &stlb->logical[stlb_offset];

	return N2D_SUCCESS;

on_error:
	return error;
}

n2d_error_t _link(n2d_mmu_t *mmu, n2d_uint32_t index, n2d_uint32_t node)
{
	if (index >= mmu->config->page_table_entries) {
		mmu->config->heap_list = node;
	} else {
		n2d_uint32_t *map = mmu->config->map_logical;

		switch (n2dENTRY_TYPE(_read_page_entry(&map[index]))) {
		case n2dMMU_SINGLE:
			/* Previous is a single node, link to it*/
			_write_page_entry(&map[index], (node << 8) | n2dMMU_SINGLE);
			break;
		case n2dMMU_FREE:
			/* Link to FREE TYPE node */
			_write_page_entry(&map[index + 1], node);
			break;
		default:
			n2d_kernel_os_print("MMU table corrupted at index %u!", index);
			return N2D_ERROR_HEAP_CORRUPTED;
		}
	}

	return N2D_SUCCESS;
}

n2d_error_t _add_free(n2d_mmu_t *mmu, n2d_uint32_t index, n2d_uint32_t node, n2d_uint32_t count)
{
	n2d_uint32_t *map = mmu->config->map_logical;

	if (count == 1) {
		/* Initialize a single page node */
		_write_page_entry(map + node, n2dSINGLE_PAGE_NODE_INITIALIZE | n2dMMU_SINGLE);
	} else {
		/* Initialize the FREE node*/
		_write_page_entry(map + node, (count << 8) | n2dMMU_FREE);
		_write_page_entry(map + node + 1, ~0U);
	}

	return _link(mmu, index, node);
}

/* Collect free nodes */
n2d_error_t _Collect(n2d_mmu_t *mmu)
{
	n2d_error_t error;
	n2d_uint32_t *map = mmu->config->map_logical;
	/* Free nodes count */
	n2d_uint32_t count    = 0;
	n2d_uint32_t start    = 0;
	n2d_uint32_t previous = ~0U;
	n2d_uint32_t i;

	mmu->config->heap_list	= ~0U;
	mmu->config->free_nodes = N2D_FALSE;

	/* Walk the entire page table */
	for (i = 0; i < mmu->config->page_table_entries; i++) {
		switch (n2dENTRY_TYPE(_read_page_entry(&map[i]))) {
		case n2dMMU_SINGLE:
			if (count++ == 0) {
				/* Set new start node */
				start = i;
			}
			break;
		case n2dMMU_FREE:
			if (count == 0) {
				/* Set new start node */
				start = i;
			}

			count += _read_page_entry(&map[i]) >> 8;
			/* Advance the index of the page table */
			i += (_read_page_entry(&map[i]) >> 8) - 1;
			break;
		case n2dMMU_USED:
			/* Meet used node, start to collect */
			if (count > 0) {
				/* Add free node to list*/
				ONERROR(_add_free(mmu, previous, start, count));
				/* Reset previous unused node index */
				previous = start;
				count	 = 0;
			}
			break;

		default:
			n2d_kernel_os_print("MMU page table corrupted at index %u!", i);
			return N2D_ERROR_HEAP_CORRUPTED;
		}
	}

	/* If left node is an open node. */
	if (count > 0) {
		ONERROR(_add_free(mmu, previous, start, count));
	}

	return N2D_SUCCESS;

on_error:
	return error;
}

n2d_error_t _fill_page_table(n2d_uint32_t *page_table, n2d_uint32_t page_count,
			     n2d_uint32_t entry_value)
{
	n2d_uint32_t i;

	for (i = 0; i < page_count; i++) {
		_write_page_entry(page_table + i, entry_value);
	}

	return N2D_SUCCESS;
}

n2d_error_t n2d_mmu_allocate_pages(n2d_mmu_t *mmu, n2d_uint32_t page_count, n2d_uint32_t *address)
{
	n2d_error_t error;
	n2d_bool_t gotIt    = N2D_FALSE;
	n2d_bool_t acquired = N2D_FALSE;
	n2d_uint32_t *map;
	n2d_uint32_t index    = 0;
	n2d_uint32_t previous = ~0U;
	n2d_uint32_t _address;
	n2d_uint32_t mtlb_offset, stlb_offset;
	n2d_uint32_t left;
	n2d_os_t *os = mmu->hardware->os;

	if (page_count == 0 || page_count > mmu->config->page_table_entries) {
		ONERROR(N2D_INVALID_ARGUMENT);
	}

	n2d_kernel_os_mutex_acquire(os, mmu->config->mutex, N2D_INFINITE);
	acquired = N2D_TRUE;

	for (map = mmu->config->map_logical; !gotIt;) {
		for (index = mmu->config->heap_list;
		     !gotIt && (index < mmu->config->page_table_entries);) {
			switch (n2dENTRY_TYPE(_read_page_entry(&map[index]))) {
			case n2dMMU_SINGLE:
				if (page_count == 1) {
					gotIt = N2D_TRUE;
				} else {
					/* Move to next node */
					previous = index;
					index	 = _read_page_entry(&map[index]) >> 8;
				}
				break;
			case n2dMMU_FREE:
				if (page_count <= (_read_page_entry(&map[index]) >> 8)) {
					gotIt = N2D_TRUE;
				} else {
					/* Move to next node */
					previous = index;
					index	 = _read_page_entry(&map[index + 1]);
				}
				break;
			default:
				/* Only link SINGLE and FREE node */
				n2d_kernel_os_print("MMU table corrupted at index %u!", index);
				ONERROR(N2D_ERROR_HEAP_CORRUPTED);
			}
		}

		/* If out of index */
		if (index >= mmu->config->page_table_entries) {
			if (mmu->config->free_nodes) {
				/* Collect the free node */
				ONERROR(_Collect(mmu));
			} else {
				ONERROR(N2D_OUT_OF_RESOURCES);
			}
		}
	}

	switch (n2dENTRY_TYPE(_read_page_entry(&map[index]))) {
	case n2dMMU_SINGLE:
		/* Unlink single node from node list */
		ONERROR(_link(mmu, previous, _read_page_entry(&map[index]) >> 8));
		break;

	case n2dMMU_FREE:
		left = (_read_page_entry(&map[index]) >> 8) - page_count;
		switch (left) {
		case 0:
			/* Unlink the entire FREE type node */
			ONERROR(_link(mmu, previous, _read_page_entry(&map[index + 1])));
			break;
		case 1:
			/* Keep the map[index] as a single node, mark the left as used */
			_write_page_entry(&map[index],
					  (_read_page_entry(&map[index + 1]) << 8) | n2dMMU_SINGLE);
			index++;
			break;
		default:
			/* FREE type node left */
			_write_page_entry(&map[index], (left << 8) | n2dMMU_FREE);
			index += left;
			break;
		}
		break;
	default:
		/* Only link SINGLE and FREE node */
		n2d_kernel_os_print("MMU table corrupted at index %u!", index);
		ONERROR(N2D_ERROR_HEAP_CORRUPTED);
	}

	/* Mark node as used */
	ONERROR(_fill_page_table(&map[index], page_count, n2dMMU_USED));

	n2d_kernel_os_mutex_release(os, mmu->config->mutex);

	mtlb_offset = index / n2dMMU_STLB_4K_ENTRY_NUM;
	stlb_offset = index % n2dMMU_STLB_4K_ENTRY_NUM;

	_address = (mtlb_offset << n2dMMU_MTLB_SHIFT) | (stlb_offset << n2dMMU_STLB_4K_SHIFT);

	if (address != N2D_NULL) {
		*address = _address;
	}

	return N2D_SUCCESS;

on_error:
	if (acquired) {
		n2d_kernel_os_mutex_release(os, mmu->config->mutex);
	}

	return error;
}

n2d_error_t n2d_mmu_free_pages(n2d_mmu_t *mmu, n2d_uint32_t address, n2d_uint32_t page_count)
{
	n2d_error_t error;
	n2d_uint32_t *node;
	n2d_bool_t acquired = N2D_FALSE;
	n2d_os_t *os	    = mmu->hardware->os;

	if (page_count == 0) {
		ONERROR(N2D_INVALID_ARGUMENT);
	}

	node = mmu->config->map_logical + _address_to_index(address);

	n2d_kernel_os_mutex_acquire(os, mmu->config->mutex, N2D_INFINITE);
	acquired = N2D_TRUE;

	if (page_count == 1) {
		/* Mark the Single page node free */
		_write_page_entry(node, n2dSINGLE_PAGE_NODE_INITIALIZE | n2dMMU_SINGLE);
	} else {
		/* Mark the FREE type node free */
		_write_page_entry(node, (page_count << 8) | n2dMMU_FREE);
		_write_page_entry(node + 1, ~0U);
	}

	mmu->config->free_nodes = N2D_TRUE;

	n2d_kernel_os_mutex_release(os, mmu->config->mutex);

	return N2D_SUCCESS;

on_error:
	if (acquired) {
		n2d_kernel_os_mutex_release(os, mmu->config->mutex);
	}

	return error;
}

n2d_error_t n2d_mmu_set_page(n2d_mmu_t *mmu, n2d_uint64_t page_address, n2d_uint32_t *page_entry)
{
	n2d_uint32_t phy, ext_phy;

	gcmkASSERT(mmu != N2D_NULL);

	if (page_entry == N2D_NULL || (page_address & 0xFFF)) {
		return N2D_INVALID_ARGUMENT;
	}

	phy	= (n2d_uint32_t)(page_address & 0xFFFFFFFF);
	ext_phy = (n2d_uint32_t)((page_address >> 32) & 0xFF);
	_write_page_entry(page_entry, _set_page(phy, ext_phy));

	return N2D_SUCCESS;
}

n2d_error_t n2d_mmu_get_phy_from_page(n2d_mmu_t *mmu, n2d_uint64_t *page_address,
				      n2d_uint32_t *page_entry)
{
	n2d_uint32_t data, phy, ext_phy;

	gcmkASSERT(mmu != N2D_NULL);

	if (page_entry == N2D_NULL) {
		return N2D_INVALID_ARGUMENT;
	}

	data	      = _read_page_entry(page_entry);
	phy	      = (n2d_uint32_t)(data & (~n2dMMU_PAGE_4K_MASK));
	ext_phy	      = (n2d_uint32_t)((data & 0x00000FF0) >> 4);
	*page_address = ((n2d_uint64_t)phy) | (((n2d_uint64_t)ext_phy) << 32);

	return N2D_SUCCESS;
}

n2d_error_t n2d_mmu_map_memory(n2d_mmu_t *mmu, n2d_uint64_t physical, n2d_uint32_t page_count,
			       n2d_uint32_t flag, n2d_uint32_t gpu_address)
{
	n2d_uint32_t _gpu_address;
	n2d_error_t error;
	n2d_uint32_t i;
	n2d_os_t *os = mmu->hardware->os;

	gcmkASSERT(flag >= 0);

	_gpu_address = gpu_address;

	/* Fill in page table */
	for (i = 0; i < page_count; i++) {
		n2d_uint64_t _physical;
		n2d_uint32_t *page_entry;

		/*if (flag & N2D_ALLOC_FLAG_CONTIGUOUS)
		{
		    _physical = physical + i * n2dMMU_PAGE_4K_SIZE;
		}
		else
		{
		    _physical = n2d_kernel_os_page_to_phys(os,(n2d_pointer*)physical, i);
		}*/

		_physical = physical + i * n2dMMU_PAGE_4K_SIZE;

		ONERROR(n2d_mmu_get_page_entry(mmu, _gpu_address, &page_entry));

		/* Write the page address to the page entry */
		ONERROR(n2d_mmu_set_page(mmu, _physical, page_entry));

		/* Get next page */
		_gpu_address += n2dMMU_PAGE_4K_SIZE;
	}
	ONERROR(n2d_kernel_os_atom_set(os, mmu->config->page_dirty, 1));

	return N2D_SUCCESS;

on_error:
	return error;
}

n2d_error_t n2d_mmu_unmap_memory(n2d_mmu_t *mmu, n2d_uint32_t gpu_address, n2d_uint32_t page_count)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_os_t *os	  = mmu->hardware->os;

	ONERROR(n2d_mmu_free_pages(mmu, gpu_address, page_count));
	ONERROR(n2d_kernel_os_atom_set(os, mmu->config->page_dirty, 1));

on_error:
	return error;
}

n2d_error_t n2d_mmu_get_physical(n2d_mmu_t *mmu, n2d_uint32_t gpu_address, n2d_uint64_t *physical)
{
	n2d_error_t error;
	n2d_os_t *os = mmu->hardware->os;
	n2d_uint32_t *page_entry;
	n2d_uint64_t _physical = 0;
	n2d_uint32_t offset, mask, page_size;

	n2d_kernel_os_get_page_size(os, &page_size);
	offset = gpu_address & (page_size - 1);
	mask   = ~(page_size - 1);

	ONERROR(n2d_mmu_get_page_entry(mmu, gpu_address, &page_entry));

	/* Read the page address from the page entry */
	if (physical) {
		ONERROR(n2d_mmu_get_phy_from_page(mmu, &_physical, page_entry));
		*physical = (_physical & mask) + offset;
	}

	return N2D_SUCCESS;

on_error:
	return error;
}

n2d_error_t n2d_mmu_enable(n2d_mmu_t *mmu)
{
	n2d_error_t error = N2D_SUCCESS;

	ONERROR(n2d_kernel_hardware_mmu_config(mmu->hardware, mmu));

	n2d_kernel_os_print("n2d mmu is enabled.\n");
on_error:
	return error;
}

#endif
