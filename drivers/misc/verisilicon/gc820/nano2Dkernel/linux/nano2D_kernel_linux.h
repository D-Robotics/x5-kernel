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

#ifndef _nano2D_kernel_linux_h_
#define _nano2D_kernel_linux_h_

#include <linux/version.h>
#include <linux/dma-buf.h>
#include "nano2D_option.h"

#ifdef __cplusplus
extern "C" {
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#define n2dVM_FLAGS (VM_IO | VM_DONTCOPY | VM_DONTEXPAND | VM_DONTDUMP)
#else
#define n2dVM_FLAGS (VM_IO | VM_DONTCOPY | VM_DONTEXPAND | VM_RESERVED)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define current_mm_mmap_sem current->mm->mmap_lock
#else
#define current_mm_mmap_sem current->mm->mmap_sem
#endif

/* user memory map type*/
enum um_desc_type {
	UM_PHYSICAL_MAP,
	UM_PAGE_MAP,
	UM_PFN_MAP,
};

/* Descriptor of parsing user memory, just use in wrapping process */
typedef struct n2d_wrap_desc {
	n2d_bool_t is_contiguous;
	n2d_uint32_t flag;
	n2d_uint32_t num_pages;

	union {
		/* N2D_ALLOC_FLAG_DMABUF */
		struct {
			/* parse user dma_buf fd */
			n2d_pointer usr_dmabuf;

			/* Descriptor of a dma_buf imported. */
			struct dma_buf *dmabuf;
			struct sg_table *sgt;
			struct dma_buf_attachment *attachment;
			unsigned long *pagearray;

			int npages;
			int pid;
			struct list_head list;
		} dmabuf_desc;

		/* N2D_ALLOC_FLAG_USERMEMORY */
		struct {
			/* record user data */
			n2d_uintptr_t user_vaddr;
			n2d_size_t size;

			/* UM_PHYSICAL_MAP. */
			n2d_uint64_t physical;

			/* save wrapped data */
			int type;
			union {
				/* UM_PAGE_MAP. */
				struct {
					struct page **pages;
				};

				/* UM_PFN_MAP. */
				struct {
					unsigned long *pfns;
					int *refs;
					int pfns_valid;
				};
			};

			/* TODO: Map pages to sg table. */
			struct sg_table sgt;

			/* contiguous chunks, does not include padding pages. */
			int chunk_count;
			unsigned int alloc_from_res;

			unsigned long vm_flags;
			n2d_size_t offset;
			n2d_uint32_t page_count;
		} um_desc;
	};
} n2d_wrap_desc_t;

typedef struct non_cgs_desc {
	/* Pointer to a array of pointers to page. */
	struct page **non_cgs_pages;

	struct sg_table sgt;
} non_cgs_desc_t;

static inline void _memory_barrier(void)
{
#if defined(CONFIG_ARM) && (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34))
	dsb();
#else
	/* memory barrier */
	mb();
#endif
}

/*
 * VIV: Timer API.
 */
#if N2D_GPU_TIMEOUT
typedef void (*n2d_timer_func)(n2d_pointer);

typedef struct n2d_timer {
	struct delayed_work work;
	n2d_timer_func function;
	n2d_pointer data;
} n2d_timer_t;
#endif

#ifdef __cplusplus
}
#endif

#endif
