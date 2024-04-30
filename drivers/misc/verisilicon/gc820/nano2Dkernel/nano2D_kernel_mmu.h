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

#ifndef _nano2D_kernel_mmu_h_
#define _nano2D_kernel_mmu_h_

#include "nano2D_types.h"
#include "nano2D_dispatch.h"
#include "nano2D_option.h"
#include "nano2D_kernel_base.h"
#include "nano2D_kernel_vidmem.h"

#ifdef __cplusplus
extern "C" {
#endif
#if NANO2D_MMU_ENABLE

#define n2dMMU_MTLB_SHIFT    22
#define n2dMMU_STLB_4K_SHIFT 12

#define n2dMMU_MTLB_BITS    (32 - n2dMMU_MTLB_SHIFT)
#define n2dMMU_PAGE_4K_BITS n2dMMU_STLB_4K_SHIFT
#define n2dMMU_STLB_4K_BITS (32 - n2dMMU_MTLB_BITS - n2dMMU_PAGE_4K_BITS)

#define n2dMMU_MTLB_ENTRY_NUM	 (1 << n2dMMU_MTLB_BITS)
#define n2dMMU_MTLB_SIZE	 (n2dMMU_MTLB_ENTRY_NUM << 2)
#define n2dMMU_STLB_4K_ENTRY_NUM (1 << n2dMMU_STLB_4K_BITS)
#define n2dMMU_STLB_4K_SIZE	 (n2dMMU_STLB_4K_ENTRY_NUM << 2)
#define n2dMMU_PAGE_4K_SIZE	 (1 << n2dMMU_STLB_4K_SHIFT)

#define n2dMMU_MTLB_MASK    (~((1U << n2dMMU_MTLB_SHIFT) - 1))
#define n2dMMU_STLB_4K_MASK ((~0U << n2dMMU_STLB_4K_SHIFT) ^ n2dMMU_MTLB_MASK)
#define n2dMMU_PAGE_4K_MASK (n2dMMU_PAGE_4K_SIZE - 1)

/* Page offset definitions. */
#define n2dMMU_OFFSET_4K_BITS (32 - n2dMMU_MTLB_BITS - n2dMMU_STLB_4K_BITS)
#define n2dMMU_OFFSET_4K_MASK ((1U << n2dMMU_OFFSET_4K_BITS) - 1)

#define n2dMMU_MTLB_PRESENT   0x00000001
#define n2dMMU_MTLB_EXCEPTION 0x00000002
#define n2dMMU_MTLB_4K_PAGE   0x00000000

#define gcdMMU_STLB_PRESENT   0x00000001
#define gcdMMU_STLB_EXCEPTION 0x00000002
#define gcdMMU_STLB_SECURITY  (1 << 4)

#define n2dENTRY_TYPE(x)	       (x & 0xF0)
#define n2dSINGLE_PAGE_NODE_INITIALIZE (~((1U << 8) - 1))

#define mmuGetPageCount(size, offset)                                                \
	((((size) + ((offset) & ~n2dMMU_PAGE_4K_MASK)) + n2dMMU_PAGE_4K_SIZE - 1) >> \
	 n2dMMU_STLB_4K_SHIFT)

typedef enum _n2dMMU_TYPE {
	n2dMMU_USED   = (0 << 4),
	n2dMMU_SINGLE = (1 << 4),
	n2dMMU_FREE   = (2 << 4),
} n2dMMU_TYPE;

typedef enum _n2dMMU_INIT_MODE {
	n2dMMU_INIT_FROM_REG,
	n2dMMU_INIT_FROM_CMD,
} n2dMMU_INIT_MODE;

typedef struct _n2d_mmu_stlb {
	n2d_vidmem_node_t *stlb_node;
	n2d_uint32_t *logical;
	n2d_uint32_t size;
	n2d_uint64_t phys_base;
	n2d_uint32_t page_count;
	n2d_uint32_t stlb_entries;
} n2d_mmu_stlb, *n2d_mmu_stlb_pt;

typedef struct n2d_mmu_config {
	n2d_uint32_t ref_count;

	n2d_vidmem_node_t *mtlb_node;
	n2d_uint32_t *mtlb_logical;
	n2d_uint32_t mtlb_bytes;
	n2d_uint64_t mtlb_physical;
	n2d_uint32_t mtlb_entries;

	n2d_vidmem_node_t *safe_page_node;
	n2d_pointer safe_page_logical;
	n2d_uint64_t safe_page_physical;

	n2d_vidmem_node_t *page_array_node;
	n2d_pointer page_table_array_logical;
	n2d_uint64_t page_table_array_physical;

	n2d_vidmem_node_t *node;
	n2d_pointer mmu_init_buffer_logical;
	n2d_uint64_t mmu_init_buffer_physical;

	n2d_uint32_t dynamic_mapping_start;

	n2d_pointer stlbs;

	n2d_uint32_t page_table_entries;
	n2d_uint32_t page_table_size;
	n2d_uint32_t heap_list;

	n2d_uint32_t *map_logical;
	n2d_bool_t free_nodes;
	n2d_pointer mutex;
	n2d_pointer page_dirty;
	n2dMMU_INIT_MODE init_mode;

	n2d_bool_t page_table_changed;
} n2d_mmu_config_t;

struct n2d_mmu {
	n2d_hardware_t *hardware;
	n2d_mmu_config_t *config;
};

typedef struct _n2d_mmu_table_array_entry {
	n2d_uint32_t low;
	n2d_uint32_t high;

} n2d_mmu_table_array_entry;

n2d_error_t n2d_mmu_destroy(n2d_mmu_t *mmu);
n2d_error_t n2d_mmu_construct(n2d_hardware_t *hardware, n2d_mmu_t *mmu);

n2d_error_t n2d_mmu_allocate_pages(n2d_mmu_t *mmu, n2d_uint32_t page_count, n2d_uint32_t *address);

n2d_error_t n2d_mmu_free_pages(n2d_mmu_t *mmu, n2d_uint32_t address, n2d_uint32_t page_count);

n2d_error_t n2d_mmu_dump_page_table(n2d_mmu_t *mmu);

n2d_error_t n2d_mmu_get_page_entry(n2d_mmu_t *mmu, n2d_uint32_t address, n2d_uint32_t **page_table);

n2d_error_t n2d_mmu_set_page(n2d_mmu_t *mmu, n2d_uint64_t page_address, n2d_uint32_t *page_entry);

n2d_error_t n2d_mmu_map_memory(n2d_mmu_t *mmu, n2d_uint64_t physical, n2d_uint32_t page_count,
			       n2d_uint32_t flag, n2d_uint32_t gpu_address);

n2d_error_t n2d_mmu_unmap_memory(n2d_mmu_t *mmu, n2d_uint32_t gpu_address, n2d_uint32_t page_count);

n2d_error_t n2d_mmu_get_physical(n2d_mmu_t *mmu, n2d_uint32_t gpu_address, n2d_uint64_t *physical);

n2d_error_t n2d_mmu_enable(n2d_mmu_t *mmu);

#endif /*NANO2D_MMU_ENABLE*/
#ifdef __cplusplus
}
#endif

#endif /* _nano2D_kernel_mmu_h_ */
