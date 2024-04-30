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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <asm/io.h>
#include <linux/mman.h>
#include <linux/mm_types.h>
#if defined(CONFIG_X86) && (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
#include <asm/set_memory.h>
#endif

#include "nano2D_types.h"
#include "nano2D_dispatch.h"
#include "nano2D_kernel_platform.h"
#include "nano2D_kernel_hardware.h"
#include "nano2D_kernel_driver.h"
#include "nano2D_kernel.h"
#include "nano2D_kernel_linux.h"
#include "nano2D_kernel_os.h"
#include "nano2D_user_linux.h"
#include "nano2D_kernel_db.h"
#include "nano2D_driver_shared.h"

#define _get_page_count(size, offset) \
	((((size) + ((offset) & ~PAGE_MASK)) + PAGE_SIZE - 1) >> PAGE_SHIFT)

#define HEAP_NODE_USED 0xABBAF00D
#define verbose	       0

typedef struct n2d_map_info {
	struct page **pages;
	n2d_uint32_t pageCount;
} n2d_map_info_t;

static atomic_t allocate_count = ATOMIC_INIT(0);

n2d_pointer n2d_kmalloc(size_t size, gfp_t flags)
{
	atomic_inc(&allocate_count);
	return kmalloc(size, flags);
}

void n2d_kfree(const n2d_pointer logical)
{
	atomic_dec(&allocate_count);
	kfree(logical);
}

n2d_error_t n2d_check_allocate_count(n2d_int32_t *count)
{
	*count = atomic_read(&allocate_count);
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_free(n2d_os_t *os, n2d_pointer memory)
{
	/* Free the mmeory. */
	if (memory != N2D_NULL)
		n2d_kfree(memory);

	/* Success. */
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_allocate(n2d_os_t *os, n2d_uint32_t size, n2d_pointer *memory)
{
	n2d_error_t error;

	if ((size == 0) || (memory == N2D_NULL))
		ONERROR(N2D_INVALID_ARGUMENT);

	/* Allocate memory. */
	*memory = n2d_kmalloc(size, GFP_KERNEL);

	if (*memory == N2D_NULL)
		ONERROR(N2D_OUT_OF_MEMORY);

	/* Success. */
	return N2D_SUCCESS;

on_error:
	return error;
}

static void _split_node(struct heap_node *node, unsigned long size)
{
	struct heap_node *split;

	/* Allocate a new node. */
	split = n2d_kmalloc(sizeof(struct heap_node), GFP_KERNEL);
	if (split == NULL)
		return;

	/* Fill in the data of this node of the remaining size. */
	split->offset = node->offset + size;
	split->size   = node->size - size;
	split->status = 0;

	/* Add the new node behind the current node. */
	list_add(&split->list, &node->list);

	/* Adjust the size of the current node. */
	node->size = size;
}

n2d_uint64_t n2d_kernel_os_page_to_phys(n2d_os_t *os, n2d_pointer *pages, n2d_uint32_t index)
{
	return (n2d_uint64_t)page_to_phys(((struct page **)pages)[index]);
}

n2d_pointer n2d_kernel_os_usleep(n2d_os_t *os, n2d_uint_t time)
{
	udelay(time);

	return N2D_NULL;
}

n2d_error_t n2d_kernel_os_atom_construct(n2d_os_t *os, n2d_pointer *atom)
{
	n2d_error_t error = N2D_SUCCESS;
	/* Allocate the atom. */
	ONERROR(n2d_kernel_os_allocate(os, N2D_SIZEOF(atomic_t), atom));
	/* Initialize the atom. */
	atomic_set((atomic_t *)*atom, 0);
on_error:
	/* Return the status. */
	return error;
}

n2d_error_t n2d_kernel_os_atom_destroy(n2d_os_t *os, n2d_pointer atom)
{
	n2d_error_t error = N2D_SUCCESS;
	/* Free the atom. */
	ONERROR(n2d_kernel_os_free(os, atom));
on_error:
	return error;
}

n2d_error_t n2d_kernel_os_atom_get(n2d_os_t *os, n2d_pointer atom, n2d_int32_t *value)
{
	/* Return the current value of atom. */
	*value = atomic_read((atomic_t *)atom);
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_atom_set(n2d_os_t *os, n2d_pointer atom, n2d_int32_t value)
{
	/* Set the current value of atom. */
	atomic_set((atomic_t *)atom, value);
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_atom_set_mask(n2d_os_t *os, n2d_pointer atom, n2d_uint32_t mask)
{
	n2d_uint32_t oval, nval;

	do {
		oval = atomic_read((atomic_t *)atom);
		nval = oval | mask;
	} while (atomic_cmpxchg((atomic_t *)atom, oval, nval) != oval);

	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_atom_clear_mask(n2d_os_t *os, n2d_pointer atom, n2d_uint32_t mask)
{
	n2d_uint32_t oval, nval;

	do {
		oval = atomic_read((atomic_t *)atom);
		nval = oval & ~mask;
	} while (atomic_cmpxchg((atomic_t *)atom, oval, nval) != oval);

	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_atom_inc(n2d_os_t *os, n2d_pointer atom, n2d_int32_t *old)
{
	*old = atomic_inc_return((atomic_t *)atom) - 1;
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_atom_dec(n2d_os_t *os, n2d_pointer atom, n2d_int32_t *old)
{
	*old = atomic_dec_return((atomic_t *)atom) + 1;
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_mutex_create(n2d_os_t *os, n2d_pointer *mutex)
{
	n2d_error_t error = N2D_SUCCESS;

	if (mutex == N2D_NULL)
		return N2D_INVALID_ARGUMENT;

	error = n2d_kernel_os_allocate(os, sizeof(struct mutex), mutex);

	if (error == N2D_SUCCESS)
		mutex_init(*(struct mutex **)mutex);

	return error;
}

n2d_error_t n2d_kernel_os_mutex_delete(n2d_os_t *os, n2d_pointer mutex)
{
	n2d_error_t error;

	if (mutex == N2D_NULL)
		return N2D_INVALID_ARGUMENT;

	mutex_destroy((struct mutex *)mutex);

	ONERROR(n2d_kernel_os_free(os, mutex));

	/* Success. */
	return N2D_SUCCESS;

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_mutex_acquire(n2d_os_t *os, n2d_pointer mutex, n2d_uint32_t timeout)
{
	if (mutex == N2D_NULL)
		return N2D_INVALID_ARGUMENT;

	if (timeout == N2D_INFINITE) {
		mutex_lock(mutex);

		return N2D_SUCCESS;
	}

	for (;;) {
		/* Try to acquire the mutex. */
		if (mutex_trylock(mutex)) {
			/* Success. */
			return N2D_SUCCESS;
		}

		if (timeout-- == 0)
			break;

		/* Wait for 1 millisecond. */
		n2d_kernel_os_usleep(os, 1000);
	}

	return N2D_TIMEOUT;
}

n2d_error_t n2d_kernel_os_mutex_release(n2d_os_t *os, n2d_pointer mutex)
{
	if (mutex == N2D_NULL)
		return N2D_INVALID_ARGUMENT;

	mutex_unlock(mutex);

	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_memory_fill(n2d_os_t *os, n2d_pointer memory, n2d_uint8_t filler,
				      n2d_uint32_t size)
{
	memset(memory, filler, size);
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_memory_copy(n2d_os_t *os, n2d_pointer dst, n2d_pointer source,
				      n2d_uint32_t size)
{
	memcpy(dst, source, size);
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_memory_read(n2d_os_t *os, n2d_pointer logical, n2d_uint32_t *data)
{
	n2d_error_t error  = N2D_SUCCESS;
	n2d_uint32_t _data = 0;

	/* Write memory. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	if (access_ok((void __user *)logical, 4))
#else
	if (access_ok(VERIFY_READ, logical, 4))
#endif
	{
		/* User address. */
		if (get_user(_data, (unsigned int __user *)logical))
			ONERROR(N2D_INVALID_ARGUMENT);
	} else {
		/* don't check the virtual address, maybe it come from io memory or reserved memory
		 */
		/* Kernel address. */
		_data = *(n2d_uint32_t *)logical;
	}

	if (data)
		*data = _data;

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_memory_write(n2d_os_t *os, n2d_pointer logical, n2d_uint32_t data)
{
	n2d_error_t error = N2D_SUCCESS;

	/* Write memory. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	if (access_ok((void __user *)logical, 4))
#else
	if (access_ok(VERIFY_WRITE, logical, 4))
#endif
	{
		/* User address. */
		if (put_user(data, (unsigned int __user *)logical))
			ONERROR(N2D_INVALID_ARGUMENT);
	} else {
		/* don't check the virtual address, maybe it come from io memory or reserved memory
		 */
		/* Kernel address. */
		*(n2d_uint32_t *)logical = data;
	}

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_copy_to_user(n2d_os_t *os, n2d_pointer ker_ptr, n2d_pointer user_ptr,
				       n2d_uint32_t size)
{
	/* Copy data to user. */
	if (copy_to_user((void __user *)user_ptr, ker_ptr, size) != 0) {
		/* Could not copy all the bytes. */
		return N2D_OUT_OF_RESOURCES;
	}
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_copy_from_user(n2d_os_t *os, n2d_pointer ker_ptr, n2d_pointer user_ptr,
					 n2d_uint32_t size)
{
	/* Copy data from user. */
	if (copy_from_user(ker_ptr, (void __user *)user_ptr, size) != 0) {
		/* Could not copy all the bytes. */
		return N2D_OUT_OF_RESOURCES;
	}

	return N2D_SUCCESS;
}

void n2d_kernel_os_delay(n2d_os_t *os, n2d_uint32_t milliseconds)
{
	if (milliseconds > 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28)
		ktime_t delay = ktime_set((milliseconds / MSEC_PER_SEC),
					  (milliseconds % MSEC_PER_SEC) * NSEC_PER_MSEC);
		__set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_hrtimeout(&delay, HRTIMER_MODE_REL);
#else
		msleep(milliseconds);
#endif
	}
}

n2d_error_t n2d_kernel_os_allocate_paged_memory(n2d_os_t *os, n2d_uint32_t *size, n2d_uint32_t flag,
						n2d_uint64_t *handle)
{
	n2d_error_t error	= N2D_SUCCESS;
	n2d_uint32_t contiguous = flag & N2D_ALLOC_FLAG_CONTIGUOUS;
	n2d_uint32_t bytes	= *size;
	struct device *dev	= (struct device *)os->device->dev;
	dma_addr_t dma_addr	= 0;
	int order, num_pages = 0;
	struct page *contiguous_pages = N2D_NULL;
	gfp_t gfp		      = GFP_KERNEL | __GFP_HIGHMEM | __GFP_NOWARN;

	if (contiguous) {
		order	  = get_order(bytes);
		num_pages = gcmGET_PAGE_COUNT(bytes, PAGE_SIZE);
		if (order >= MAX_ORDER) {
			error = N2D_OUT_OF_MEMORY;
			goto on_error;
		}

		gfp &= ~__GFP_HIGHMEM;
		if (flag & N2D_ALLOC_FLAG_4G)
			gfp |= __GFP_DMA32;

		contiguous_pages = alloc_pages(gfp, order);

		if (contiguous_pages == N2D_NULL)
			ONERROR(N2D_OUT_OF_MEMORY);

		dma_addr = dma_map_page(dev, contiguous_pages, 0, num_pages * PAGE_SIZE,
					DMA_BIDIRECTIONAL);

		if (dma_mapping_error(dev, dma_addr)) {
			__free_pages(contiguous_pages, get_order(bytes));
			ONERROR(N2D_OUT_OF_MEMORY);
		}

#if defined(CONFIG_X86)
		if (!PageHighMem(contiguous_pages)) {
#if NANO2D_ENABLE_WRITEBUFFER
			if (set_memory_wc((unsigned long)page_address(contiguous_pages),
					  num_pages) != 0)
				n2d_kernel_os_print("%s(%d): failed to set_memory_wc\n", __func__,
						    __LINE__);
#else
			if (set_memory_uc((unsigned long)page_address(contiguous_pages),
					  num_pages) != 0)
				n2d_kernel_os_print("%s(%d): failed to set_memory_uc\n", __func__,
						    __LINE__);
#endif
		}
#endif
		*size	= num_pages * PAGE_SIZE;
		*handle = dma_addr;
	} else {
		ONERROR(N2D_NOT_SUPPORTED);
	}

	return error;
on_error:
	if (contiguous_pages)
		__free_pages(contiguous_pages, get_order(bytes));
	return error;
}

n2d_error_t n2d_kernel_os_free_paged_memory(n2d_os_t *os, n2d_uint32_t size, n2d_uint32_t flag,
					    n2d_uint64_t handle)
{
	n2d_error_t error	      = N2D_SUCCESS;
	n2d_uint32_t contiguous	      = flag & N2D_ALLOC_FLAG_CONTIGUOUS;
	n2d_uint32_t bytes	      = size;
	struct device *dev	      = (struct device *)os->device->dev;
	dma_addr_t dma_addr	      = 0;
	int num_pages		      = 0;
	struct page *contiguous_pages = N2D_NULL;

	if (contiguous) {
		num_pages = gcmGET_PAGE_COUNT(bytes, PAGE_SIZE);
		n2d_kernel_os_get_physical_from_handle(handle, 0, flag, &dma_addr);
		contiguous_pages = pfn_to_page(dma_addr >> PAGE_SHIFT);

		dma_unmap_page(dev, dma_addr, num_pages << PAGE_SHIFT, DMA_FROM_DEVICE);
#if defined(CONFIG_X86)
		if (!PageHighMem(contiguous_pages))
			set_memory_wb((unsigned long)page_address(contiguous_pages), num_pages);
#endif
		__free_pages(contiguous_pages, get_order(num_pages * PAGE_SIZE));
	} else {
		ONERROR(n2d_kernel_os_free_noncontiguous(os, size, handle));
	}

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_get_contiguous_pool(n2d_os_t *os, n2d_vidmem_pool_t name,
					      n2d_pointer *pool)
{
	n2d_error_t error	  = N2D_SUCCESS;
	struct memory_heap *_pool = N2D_NULL;

	switch (name) {
	case N2D_POOL_SYSTEM:
		_pool = &os->device->heap;
		break;
	case N2D_POOL_COMMAND:
		_pool = &os->device->command_heap;
		break;
	default:
		error = N2D_NOT_FOUND;
	}
	*pool = (n2d_pointer)_pool;

	return error;
}

n2d_error_t n2d_kernel_os_allocate_contiguous(n2d_os_t *os, n2d_pointer pool, n2d_uint32_t *size,
					      n2d_uint32_t flag, n2d_pointer *logical,
					      n2d_uint64_t *physical, n2d_uint32_t aligned)
{
	n2d_error_t error	 = N2D_SUCCESS;
	n2d_bool_t get_mutex	 = N2D_FALSE;
	struct memory_heap *heap = (struct memory_heap *)pool;
	n2d_uint32_t _size	 = *size;
	unsigned long aligned_size;
	struct heap_node *pos;
	n2d_uintptr_t aligned64 = aligned;
	n2d_bool_t limit_4G	= ((flag & N2D_ALLOC_FLAG_4G) != 0);

	*logical  = N2D_NULL;
	*physical = 0;

	ONERROR(n2d_kernel_os_mutex_acquire(os, os->alloc_mutex, N2D_INFINITE));
	get_mutex = N2D_TRUE;

	if (!heap || heap->free == 0) {
		n2d_kernel_os_mutex_release(os, os->alloc_mutex);
		return N2D_OUT_OF_MEMORY;
	}
	if (limit_4G && !heap->limit_4G) {
		n2d_kernel_os_mutex_release(os, os->alloc_mutex);
		return N2D_NOT_SUPPORTED;
	}

	/* Align the size to 64 bytes, address to aligned 64 bytes */
	aligned_size = gcmALIGN(_size, 64);

	/*at least 64 bytes aligned*/
	if (aligned < 64)
		aligned = 64;

	aligned64 = aligned;

	aligned_size += aligned;

	/* Check if there is enough free memory available. */
	if (aligned_size > heap->free)
		ONERROR(N2D_OUT_OF_MEMORY);

	/* Walk the heap backwards. */
	list_for_each_entry_reverse (pos, &heap->list, list) {
		/* Check if the current node is free and is big enough. */
		if (pos->status == 0 && pos->size >= aligned_size) {
			/* See if we the current node is big enough to split. */
			if (pos->size - aligned_size >= 64)
				_split_node(pos, aligned_size);

			/* Mark the current node as used. */
			pos->status = HEAP_NODE_USED;

			/* Return the logical/physical address. */
			/* Both addresses need to start from an aligned place*/
#if USE_ARCH64
			/*64bit os*/
			*logical = (n2d_pointer)((((n2d_uintptr_t)heap->klogical) + pos->offset +
						  (aligned64 - 1)) &
						 ~(aligned64 - 1));
#else
			*logical = (n2d_pointer)((((n2d_uintptr_t)heap->klogical) + pos->offset +
						  (aligned - 1)) &
						 ~(aligned - 1));
#endif
			*physical =
				(heap->physical + pos->offset + (aligned64 - 1)) & ~(aligned64 - 1);
			/* Subtract the node size from free count. */
			os->device->heap.free -= aligned_size;

			*size = aligned_size;
			/* Success. */
			n2d_kernel_os_mutex_release(os, os->alloc_mutex);
			return N2D_SUCCESS;
		}
	}

on_error:
	/* Out of memory. */
	if (get_mutex)
		n2d_kernel_os_mutex_release(os, os->alloc_mutex);
	return N2D_OUT_OF_MEMORY;
}

n2d_error_t n2d_kernel_os_free_contiguous(n2d_os_t *os, n2d_pointer pool, n2d_pointer logical,
					  n2d_uint64_t physical)
{
	n2d_error_t error;
	struct memory_heap *heap = (struct memory_heap *)pool;
	struct heap_node *pos, *node = NULL;

	/* Find the node. */
	list_for_each_entry_reverse (pos, &heap->list, list) {
		/* Check if the current node is free and is big enough. */
		if ((pos->status == HEAP_NODE_USED) &&
		    ((pos->offset + heap->physical) <= physical) &&
		    ((pos->offset + heap->physical + pos->size) > physical)) {
			node = pos;
			goto found;
		}
	}

	ONERROR(N2D_OUT_OF_RESOURCES);

found:
	/* Mark node as free. */
	node->status = 0;

	/* Add node size to free count. */
	heap->free += node->size;

	/* Check if next node is free. */
	pos = node;
	list_for_each_entry_continue (pos, &heap->list, list) {
		if (pos->status == 0) {
			/* Merge the nodes. */
			node->size += pos->size;

			/* Delete the next node from the list. */
			list_del(&pos->list);
			n2d_kfree(pos);
		}
		break;
	}

	/* Check if the previous node is free. */
	pos = node;
	list_for_each_entry_continue_reverse (pos, &heap->list, list) {
		if (pos->status == 0) {
			/* Merge the nodes. */
			pos->size += node->size;

			/* Delete the current node from the list. */
			list_del(&node->list);
			n2d_kfree(node);
		}
		break;
	}

	/* Success. */
	return N2D_SUCCESS;

on_error:
	return error;
}

static n2d_error_t _free_non_contiguous(struct page **pages, n2d_uint32_t num_pages)
{
	int i;

	gcmkASSERT(pages != N2D_NULL);

	for (i = 0; i < num_pages; i++)
		__free_page(pages[i]);

	kfree(pages);

	return N2D_SUCCESS;
}

__maybe_unused static n2d_error_t _alloc_non_contiguous(non_cgs_desc_t *priv,
							n2d_uint32_t num_pages, gfp_t gfp)
{
	n2d_error_t error   = N2D_SUCCESS;
	struct page **pages = N2D_NULL;
	struct page *p	    = N2D_NULL;

	n2d_uint32_t i = 0, size = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	if (num_pages > totalram_pages())
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	if (num_pages > totalram_pages)
#else
	if (num_pages > num_physpages)
#endif
		ONERROR(N2D_INVALID_ARGUMENT);

	size = num_pages * sizeof(struct page *);

	pages = kmalloc(size, GFP_KERNEL | __GFP_NOWARN);
	if (!pages)
		ONERROR(N2D_OUT_OF_MEMORY);

	for (i = 0; i < num_pages; i++) {
		p = alloc_page(gfp);

		if (!p) {
			_free_non_contiguous(pages, i);
			ONERROR(N2D_OUT_OF_MEMORY);
		}
		pages[i] = p;
	}

	priv->non_cgs_pages = pages;

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_allocate_noncontiguous(n2d_os_t *os, n2d_uint32_t *size,
						 n2d_uint32_t flag, n2d_uint64_t *handle)
{
	n2d_error_t error = N2D_SUCCESS;
#if NANO2D_MMU_ENABLE
	n2d_uint32_t result    = 0;
	n2d_uint32_t num_pages = 0;
	non_cgs_desc_t *desc   = N2D_NULL;
	struct device *dev     = (struct device *)os->device->dev;
	n2d_uint32_t gfp       = GFP_KERNEL | __GFP_HIGHMEM | __GFP_NOWARN;
	n2d_uint32_t i;

	num_pages = gcmGET_PAGE_COUNT(*size, PAGE_SIZE);

	if (flag & N2D_ALLOC_FLAG_4G) {
		gfp &= ~__GFP_HIGHMEM;
		gfp |= __GFP_DMA32;
	}

	ONERROR(n2d_kernel_os_allocate(os, sizeof(non_cgs_desc_t), (n2d_pointer *)&desc));
	ONERROR(_alloc_non_contiguous(desc, num_pages, gfp));

	result = sg_alloc_table_from_pages(&desc->sgt, desc->non_cgs_pages, num_pages, 0,
					   num_pages << PAGE_SHIFT, gfp);

	if (result < 0) {
		_free_non_contiguous(desc->non_cgs_pages, num_pages);
		ONERROR(N2D_OUT_OF_MEMORY);
	}

	/* dma_map_sg only is used to flush cache here. */
	result = dma_map_sg(dev, desc->sgt.sgl, desc->sgt.nents, DMA_BIDIRECTIONAL);
	if (result != desc->sgt.nents) {
		_free_non_contiguous(desc->non_cgs_pages, num_pages);
		sg_free_table(&desc->sgt);
		ONERROR(N2D_OUT_OF_MEMORY);
	}

#if defined(CONFIG_X86)
#if NANO2D_ENABLE_WRITEBUFFER
	if (set_pages_array_wc(desc->non_cgs_pages, num_pages) != 0)
		n2d_kernel_os_print("%s(%d): failed to set_memory_wc\n", __func__, __LINE__);
#else
	if (set_pages_array_uc(desc->non_cgs_pages, num_pages) != 0)
		n2d_kernel_os_print("%s(%d): failed to set_memory_uc\n", __func__, __LINE__);
#endif
#endif

	for (i = 0; i < num_pages; i++) {
		struct page *page;

		page = desc->non_cgs_pages[i];

		SetPageReserved(page);
	}

	*size	= num_pages * PAGE_SIZE;
	*handle = (n2d_uint64_t)desc;

	return N2D_SUCCESS;

#else
	n2d_kernel_os_print("[n2d error]: Alloc noncontiguous memory when disable MMU...\n");
	ONERROR(N2D_INVALID_ARGUMENT);
#endif
on_error:
	return error;
}

n2d_error_t n2d_kernel_os_free_noncontiguous(n2d_os_t *os, n2d_uint32_t size, n2d_uint64_t handle)
{
	n2d_error_t error = N2D_SUCCESS;
#if NANO2D_MMU_ENABLE
	n2d_uint32_t i;
	struct page *page;
	n2d_uint32_t num_pages = 0;
	non_cgs_desc_t *desc   = (non_cgs_desc_t *)handle;
	struct device *dev     = (struct device *)os->device->dev;

	gcmkASSERT(desc != N2D_NULL);

	num_pages = gcmGET_PAGE_COUNT(size, PAGE_SIZE);

	dma_unmap_sg(dev, desc->sgt.sgl, desc->sgt.nents, DMA_FROM_DEVICE);
	sg_free_table(&desc->sgt);

	for (i = 0; i < num_pages; i++) {
		page = desc->non_cgs_pages[i];
		ClearPageReserved(page);
	}

#if defined(CONFIG_X86)
	set_pages_array_wb(desc->non_cgs_pages, num_pages);
#endif
	_free_non_contiguous(desc->non_cgs_pages, num_pages);
	n2d_kernel_os_free(os, desc);
#else
	n2d_kernel_os_print("[n2d error]: Free noncontiguous memory when disable MMU...\n");
	ONERROR(N2D_INVALID_ARGUMENT);
on_error:
#endif
	return error;
}

n2d_error_t n2d_kernel_os_dma_alloc(n2d_os_t *os, n2d_uint32_t *size, n2d_uint32_t flag,
				    n2d_pointer *kvaddr, n2d_uint64_t *physical)
{
	n2d_uint32_t _size     = *size;
	gfp_t gfp	       = GFP_KERNEL | __GFP_NOWARN;
	struct device *dev     = (struct device *)os->device->dev;
	n2d_pointer _kvaddr    = N2D_NULL;
	dma_addr_t dma_addr    = 0;
	n2d_uint32_t num_pages = 0;

#if defined(CONFIG_X86)
	n2d_uint32_t ret = 0;
#endif

	num_pages = gcmGET_PAGE_COUNT(_size, PAGE_SIZE);

	if (flag & N2D_ALLOC_FLAG_4G)
		gfp |= __GFP_DMA32;

#if defined CONFIG_MIPS || defined CONFIG_CPU_CSKYV2 || defined CONFIG_PPC || \
	defined CONFIG_ARM64 || !NANO2D_ENABLE_WRITEBUFFER
	_kvaddr = dma_alloc_coherent(dev, num_pages * PAGE_SIZE, &dma_addr, gfp);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
	_kvaddr = dma_alloc_wc(dev, num_pages * PAGE_SIZE, &dma_addr, gfp);
#else
	_kvaddr = dma_alloc_writecombine(dev, num_pages * PAGE_SIZE, &dma_addr, gfp);
#endif
#endif

	if (!dma_addr)
		return N2D_OUT_OF_MEMORY;

#if defined(CONFIG_X86)
#if NANO2D_ENABLE_WRITEBUFFER
	ret = set_memory_wc((unsigned long)_kvaddr, num_pages);
	if (ret != 0)
		n2d_kernel_os_print("%s(%d): failed to set_memory_wc, ret = %d\n", __func__,
				    __LINE__, ret);
#else
	if (set_memory_uc((unsigned long)_kvaddr, num_pages) != 0)
		n2d_kernel_os_print("%s(%d): failed to set_memory_uc\n", __func__, __LINE__);
#endif
#endif

	*kvaddr	  = _kvaddr;
	*physical = dma_addr;
	*size	  = num_pages * PAGE_SIZE;

	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_dma_free(n2d_os_t *os, n2d_uint32_t size, n2d_pointer kvaddr,
				   n2d_uint64_t physical)
{
	n2d_error_t error      = N2D_SUCCESS;
	struct device *dev     = (struct device *)os->device->dev;
	n2d_uint32_t num_pages = 0;

	num_pages = gcmGET_PAGE_COUNT(size, PAGE_SIZE);

#if defined CONFIG_MIPS || defined CONFIG_CPU_CSKYV2 || defined CONFIG_PPC || \
	defined CONFIG_ARM64 || !NANO2D_ENABLE_WRITEBUFFER
	dma_free_coherent(dev, num_pages * PAGE_SIZE, kvaddr, physical);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
	dma_free_wc(dev, num_pages * PAGE_SIZE, kvaddr, physical);
#else
	dma_free_writecombine(dev, num_pages * PAGE_SIZE, kvaddr, physical);
#endif
#endif

	return error;
}

static n2d_error_t _map_to_kernel(n2d_os_t *os, n2d_uint64_t handle, n2d_uint32_t size,
				  n2d_uint32_t flag, n2d_map_type_t type, n2d_pointer *logical)
{
	n2d_error_t error     = N2D_SUCCESS;
	n2d_pointer addr      = NULL;
	n2d_int32_t num_pages = 0;
	struct page **pages   = NULL;
	n2d_bool_t free	      = N2D_FALSE;
	pgprot_t pgprot;
	n2d_uint32_t offset = 0;
	n2d_uint32_t bytes  = 0;

	pr_info("%s: flag = 0x%x.\n", __func__, flag);
	if (flag & N2D_ALLOC_FLAG_WRAP_USER) { /* already mapped kernel */
		return error;
	}
	/*
	 * DMA-allocator no need to map to kernel, just consider contiguous or not.
	 * Reserved memory has been mapped, so, only memory from gfp need map to kernel.
	 */
	if (flag & N2D_ALLOC_FLAG_CONTIGUOUS) {
		int i;
		n2d_pointer pointer	      = N2D_NULL;
		struct page *contiguous_pages = N2D_NULL;

		offset		 = handle & (PAGE_SIZE - 1);
		bytes		 = size + offset;
		num_pages	 = gcmGET_PAGE_COUNT(bytes, PAGE_SIZE);
		contiguous_pages = pfn_to_page(handle >> PAGE_SHIFT);

		ONERROR(n2d_kernel_os_allocate(os, sizeof(struct page *) * num_pages, &pointer));
		pages = pointer;

		for (i = 0; i < num_pages; i++)
			pages[i] = nth_page(contiguous_pages, i);

		free = N2D_TRUE;
	} else if (flag & N2D_ALLOC_FLAG_NOCONTIGUOUS) {
		/* Get noncontiguous desc. */
		non_cgs_desc_t *desc = (non_cgs_desc_t *)handle;

		num_pages = gcmGET_PAGE_COUNT(size, PAGE_SIZE);
		pages	  = desc->non_cgs_pages;
	} else {
		ONERROR(N2D_INVALID_ARGUMENT);
	}

#if NANO2D_ENABLE_WRITEBUFFER
	pgprot = pgprot_writecombine(PAGE_KERNEL);
#else
	pgprot = pgprot_noncached(PAGE_KERNEL);
#endif

	addr = vmap(pages, num_pages, 0, pgprot);

	if (free)
		n2d_kernel_os_free(os, pages);

	if (addr) {
		/* Append offset in page. */
		*logical = (n2d_pointer)((n2d_uint8_t *)addr + offset);
		return N2D_SUCCESS;
	} else {
		return N2D_OUT_OF_MEMORY;
	}

on_error:
	return error;
}

static n2d_error_t _unmap_to_kernel(n2d_os_t *os, n2d_pointer logical)
{
	vunmap((void *)((n2d_uintptr_t)logical & PAGE_MASK));

	return N2D_SUCCESS;
}

static n2d_error_t _unmap_to_user(n2d_os_t *os, n2d_uint32_t process, n2d_pointer logical,
				  n2d_uint32_t size, n2d_map_type_t type)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_pointer _logical;
	n2d_uint32_t bytes;
	n2d_size_t offset = (n2d_uintptr_t)logical & (PAGE_SIZE - 1);

	if (unlikely(!current->mm))
		return error;

	_logical = (n2d_pointer)((n2d_uint8_t *)logical - offset);
	bytes	 = gcmGET_PAGE_COUNT(size + offset, PAGE_SIZE) * PAGE_SIZE;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	if (vm_munmap((unsigned long)_logical, bytes) < 0)
		printk("%s: vm_munmap failed\n", __func__);
#else
	down_write(&current_mm_mmap_sem);
	if (do_munmap(current->mm, (unsigned long)_logical, bytes) < 0)
		printk("%s: do_munmap failed\n", __func__);

	up_write(&current_mm_mmap_sem);
#endif

	return error;
}

/*
 * Now we support contiguous and noncontiguous memory.
 */
static n2d_error_t _map_to_user(n2d_os_t *os, n2d_uint32_t process, n2d_uint64_t handle,
				n2d_pointer klogical, n2d_uint32_t size, n2d_uint32_t flag,
				n2d_map_type_t type, n2d_pointer *logical)
{
	n2d_error_t error      = N2D_SUCCESS;
	struct device *dev     = (struct device *)os->device->dev;
	n2d_pointer _logical   = N2D_NULL;
	n2d_uint64_t physical  = N2D_INVALID_ADDRESS;
	n2d_uint32_t offset    = 0;
	n2d_size_t bytes       = 0;
	n2d_uint32_t num_pages = 0, pfn = 0;

	ONERROR(n2d_kernel_os_get_physical_from_handle(handle, 0, flag, &physical));

	if (!(flag & N2D_ALLOC_FLAG_NOCONTIGUOUS))
		offset = physical & (PAGE_SIZE - 1);

	bytes = size + offset;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	_logical = (n2d_pointer)vm_mmap(NULL, 0L, bytes, PROT_READ | PROT_WRITE,
					MAP_SHARED | MAP_NORESERVE, 0);
#else
	down_write(&current_mm_mmap_sem);
	_logical =
		(n2d_pointer)do_mmap_pgoff(NULL, 0L, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, 0);
	up_write(&current_mm_mmap_sem);
#endif

	if (IS_ERR(_logical)) {
		logical = N2D_NULL;
		ONERROR(N2D_OUT_OF_MEMORY);
	}

	down_write(&current_mm_mmap_sem);

	do {
		struct vm_area_struct *vma = find_vma(current->mm, (unsigned long)_logical);
		if (vma == N2D_NULL)
			ONERROR(N2D_OUT_OF_RESOURCES);

		num_pages = gcmGET_PAGE_COUNT(bytes, PAGE_SIZE);

		/* Make this mapping non-cached. */
		vma->vm_flags |= n2dVM_FLAGS;

#if NANO2D_ENABLE_WRITEBUFFER
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
#else
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#endif

		/* Map wrapped dma_buf physical to user space. */
		if (flag & N2D_ALLOC_FLAG_WRAP_USER) {
			n2d_wrap_desc_t *w_desc = (n2d_wrap_desc_t *)handle;

			if (w_desc->flag == N2D_ALLOC_FLAG_DMABUF) {
				/* Memory wrapped in is allocated by dma_alloc_API. On support
				 * contiguous. */
				n2d_uint32_t i;
				unsigned long start = vma->vm_start;

				for (i = 0; i < num_pages; i++) {
					pfn = w_desc->dmabuf_desc.pagearray[i] >> PAGE_SHIFT;
					if (remap_pfn_range(vma, start, pfn, PAGE_SIZE,
							    vma->vm_page_prot) < 0) {
						error = N2D_OUT_OF_MEMORY;
						break;
					}
					start += PAGE_SIZE;
				}

				offset = w_desc->dmabuf_desc.sgt->sgl->offset;
				break;
			} else {
				/* Wrapped user memory no need map_to_user again, there is a bug. */
				ONERROR(N2D_INVALID_ARGUMENT);
			}
		}

		if (flag & N2D_ALLOC_FLAG_DMA_ALLOCATOR) {
			gcmkASSERT(klogical != N2D_NULL);
#if defined CONFIG_MIPS || defined CONFIG_CPU_CSKYV2 || defined CONFIG_PPC || \
	defined CONFIG_ARM64 || !NANO2D_ENABLE_WRITEBUFFER
			if (dma_mmap_coherent(dev, vma, klogical, physical,
					      num_pages << PAGE_SHIFT) < 0)
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
			if (dma_mmap_wc(dev, vma, klogical, physical, num_pages << PAGE_SHIFT) < 0)
#else
			if (dma_mmap_writecombine(dev, vma, klogical, physical,
						  num_pages << PAGE_SHIFT) < 0)
#endif
#endif
				error = N2D_OUT_OF_MEMORY;
		} else if (flag & N2D_ALLOC_FLAG_CONTIGUOUS) {
			pfn = (physical >> PAGE_SHIFT);
			if (remap_pfn_range(vma, vma->vm_start, pfn, num_pages << PAGE_SHIFT,
					    vma->vm_page_prot) < 0)
				error = N2D_OUT_OF_MEMORY;
		} else if (flag & N2D_ALLOC_FLAG_NOCONTIGUOUS) {
			n2d_uint32_t i;
			unsigned long start  = vma->vm_start;
			non_cgs_desc_t *desc = (non_cgs_desc_t *)handle;

			for (i = 0; i < num_pages; i++) {
				pfn = page_to_pfn(desc->non_cgs_pages[i]);
				if (remap_pfn_range(vma, start, pfn, PAGE_SIZE, vma->vm_page_prot) <
				    0) {
					error = N2D_OUT_OF_MEMORY;
					break;
				}
				start += PAGE_SIZE;
			}

		} else {
			error = N2D_INVALID_ARGUMENT;
		}

	} while (N2D_FALSE);

	up_write(&current_mm_mmap_sem);

	if (N2D_IS_SUCCESS(error))
		*logical = (n2d_pointer)((n2d_uint8_t *)_logical + offset);
on_error:
	if (N2D_IS_ERROR(error) && _logical)
		n2d_kernel_os_unmap_cpu(os, process, _logical, size, type);

	return error;
}

n2d_error_t n2d_kernel_os_map_cpu(n2d_os_t *os, n2d_uint32_t process, n2d_uint64_t handle,
				  n2d_pointer klogical, n2d_uint32_t size, n2d_uint32_t flag,
				  n2d_map_type_t type, n2d_pointer *logical)
{
	n2d_error_t error = N2D_SUCCESS;

	if (type & N2D_MAP_TO_KERNEL)
		ONERROR(_map_to_kernel(os, handle, size, flag, type, logical));
	else if (type & N2D_MAP_TO_USER)
		ONERROR(_map_to_user(os, process, handle, klogical, size, flag, type, logical));
	else
		ONERROR(N2D_INVALID_ARGUMENT);

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_unmap_cpu(n2d_os_t *os, n2d_uint32_t process, n2d_pointer logical,
				    n2d_uint32_t size, n2d_map_type_t type)
{
	n2d_error_t error = N2D_SUCCESS;

	if (type & N2D_MAP_TO_KERNEL)
		ONERROR(_unmap_to_kernel(os, logical));
	else if (type & N2D_MAP_TO_USER)
		ONERROR(_unmap_to_user(os, process, logical, size, type));
	else
		ONERROR(N2D_INVALID_ARGUMENT);

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_unmap_gpu(n2d_os_t *os, n2d_uint32_t core, n2d_uint32_t address,
				    n2d_uint32_t size)
{
	n2d_error_t error = N2D_SUCCESS;
#if NANO2D_MMU_ENABLE
	n2d_hardware_t *hardware = os->kernel->sub_dev[0]->hardware[core];
	n2d_uint32_t page_count	 = 0;
	n2d_mmu_t *mmu		 = &hardware->mmu;
	n2d_uint32_t offset, page_size;

	n2d_kernel_os_get_page_size(os, &page_size);
	offset = address & (page_size - 1);
	address -= offset;
	page_count = gcmGET_PAGE_COUNT(size + offset, PAGE_SIZE);

	ONERROR(n2d_mmu_unmap_memory(mmu, address, page_count));

on_error:
#endif
	return error;
}

n2d_error_t n2d_kernel_os_map_gpu(n2d_os_t *os, n2d_uint32_t core, n2d_uint64_t handle,
				  n2d_uint32_t size, n2d_uint32_t flag, n2d_uint64_t *address)
{
	n2d_error_t error	  = N2D_SUCCESS;
	n2d_uint32_t contiguous	  = flag & N2D_ALLOC_FLAG_CONTIGUOUS;
	n2d_uint64_t cpu_physical = N2D_INVALID_ADDRESS;
	n2d_uint64_t gpu_physical = N2D_INVALID_ADDRESS;

#if NANO2D_MMU_ENABLE
	/* linux, because use share page table, so the core is first device's first core. */
	n2d_hardware_t *hardware = os->kernel->sub_dev[0]->hardware[core];
	n2d_mmu_t *mmu		 = &hardware->mmu;
	n2d_uint32_t page_count	 = 0;
	n2d_uint32_t page_size = 0, offset = 0;
	n2d_uint32_t _address = 0;

	if (!mmu) {
		pr_warn("[n2d kernel]mmu is null...\n");
		ONERROR(N2D_INVALID_ARGUMENT);
	}

	n2d_kernel_os_get_page_size(os, &page_size);
#else
	if (!contiguous) {
		pr_warn("[n2d kernel]error: memory is not contiguous and mmu is disable...\n");
		ONERROR(N2D_NOT_SUPPORTED);
	}
#endif

	if (contiguous) {
		/*
		 * Map contiguous memory. return physical if disable MMU,
		 * otherwise, write MMU page table.
		 */
		ONERROR(n2d_kernel_os_get_physical_from_handle(handle, 0, flag, &cpu_physical));
		ONERROR(n2d_kernel_os_cpu_to_gpu_phy(os, cpu_physical, &gpu_physical));
#if NANO2D_MMU_ENABLE
		offset = gpu_physical & (page_size - 1);
		gpu_physical -= offset;
		page_count = gcmGET_PAGE_COUNT(size + offset, page_size);

		ONERROR(n2d_mmu_allocate_pages(mmu, page_count, &_address));
		ONERROR(n2d_mmu_map_memory(mmu, gpu_physical, page_count, flag, _address));
		*address = _address + offset;
#else
		*address = gpu_physical;
#endif
	} else {
#if NANO2D_MMU_ENABLE
		/* If non-contiguous and disable MMU, will return error. So MMU must be enabled
		 * here. */
		n2d_uint32_t map_addr	= 0;
		n2d_uint32_t map_offset = 0;
		n2d_uint32_t i;

		page_count = gcmGET_PAGE_COUNT(size, page_size);

		ONERROR(n2d_mmu_allocate_pages(mmu, page_count, &_address));
		map_addr = _address;

		/* Get offset in page of base physical address. */
		ONERROR(n2d_kernel_os_get_physical_from_handle(handle, 0, flag, &cpu_physical));
		ONERROR(n2d_kernel_os_cpu_to_gpu_phy(os, cpu_physical, &gpu_physical));
		offset = gpu_physical & (page_size - 1);

		for (i = 0; i < page_count; i++) {
			ONERROR(n2d_kernel_os_get_physical_from_handle(handle, map_offset, flag,
								       &cpu_physical));
			ONERROR(n2d_kernel_os_cpu_to_gpu_phy(os, cpu_physical, &gpu_physical));
			gpu_physical &= ~((n2d_uint64_t)page_size - 1);
			ONERROR(n2d_mmu_map_memory(mmu, gpu_physical, 1, flag, map_addr));
			map_addr += page_size;
			map_offset += page_size;
		}
		*address = _address + offset;
#endif
	}

	return N2D_SUCCESS;
on_error:
#if NANO2D_MMU_ENABLE
	if (_address)
		n2d_mmu_free_pages(mmu, _address, page_count);
#endif
	return error;
}

n2d_uint32_t n2d_kernel_os_peek_with_core(n2d_os_t *os, n2d_uint32_t core, n2d_uint32_t address)
{
	return *(uint32_t *)(((uint8_t *)os->device->register_bases_mapped[core]) + address);
}

void n2d_kernel_os_poke_with_core(n2d_os_t *os, n2d_uint32_t core, n2d_uint32_t address,
				  n2d_uint32_t data)
{
	/* Write data to the GPU register. */
	*(uint32_t *)(((uint8_t *)os->device->register_bases_mapped[core]) + address) = data;
}

n2d_uint32_t n2d_kernel_os_get_base_address(n2d_os_t *os)
{
	/* return moduleParam.baseAddress; */
	return 0;
}

__printf(1, 2) void n2d_kernel_os_trace(char *format, ...)
{
	static char buffer[128];
	va_list args;
	va_start(args, format);

	if (verbose) {
		vsnprintf(buffer, sizeof(buffer) - 1, format, args);
		buffer[sizeof(buffer) - 1] = 0;
	}

	va_end(args);
}

__printf(1, 2) void n2d_kernel_os_print(char *format, ...)
{
	static char buffer[128];
	va_list args;
	va_start(args, format);

	vsnprintf(buffer, sizeof(buffer) - 1, format, args);
	buffer[sizeof(buffer) - 1] = 0;
	printk(buffer);
	va_end(args);
}

n2d_error_t n2d_kernel_os_set_gpu_clock(n2d_os_t *os, n2d_bool_t clock)
{
	n2d_error_t error	       = N2D_SUCCESS;
	n2d_linux_platform_t *platform = N2D_NULL;

	platform = os->device->platform;

	if (platform && platform->ops->setClock)
		ONERROR(platform->ops->setClock(platform, 0, clock));

on_error:
	return error;
}


n2d_error_t n2d_kernel_os_set_gpu_power(n2d_os_t *os, n2d_bool_t power)
{
	n2d_error_t error	       = N2D_SUCCESS;
	n2d_linux_platform_t *platform = N2D_NULL;

	platform = os->device->platform;

	if (platform && platform->ops->set_power)
		ONERROR(platform->ops->set_power(platform, 0, power));

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_get_process(n2d_os_t *os, n2d_uint32_t *process)
{
	n2d_uint32_t pid = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	pid = task_tgid_vnr(current);
#else
	pid = current->tgid;
#endif
	*process = pid;
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_signal_create(n2d_os_t *os, n2d_bool_t manual_reset, n2d_pointer *signal)
{
	n2d_error_t error     = N2D_SUCCESS;
	n2d_signal_t *_signal = N2D_NULL;
	n2d_pointer pointer   = N2D_NULL;
	n2d_db_t *db	      = os->kernel->db;

	/* Create an event structure. */
	ONERROR(n2d_kernel_os_allocate(os, sizeof(n2d_signal_t), &pointer));

	if (pointer == N2D_NULL)
		ONERROR(N2D_OUT_OF_MEMORY);

	_signal = (n2d_signal_t *)pointer;

	/* Save the process ID. */
	n2d_kernel_os_get_process(os, &_signal->process);

	_signal->done = 0;
	init_waitqueue_head(&_signal->wait);
	spin_lock_init(&_signal->lock);
	_signal->manual_reset = manual_reset;

	atomic_set(&_signal->ref, 1);

	ONERROR(n2d_kernel_db_get_handle(db, &_signal->id));
	/* all signal is stored to process 0 */
	ONERROR(n2d_kernel_db_insert(db, 0, _signal->id, N2D_SIGNAL_TYPE,
				     (n2d_uintptr_t)(_signal)));

	*signal = gcmINT2PTR(_signal->id);

on_error:
	if (N2D_IS_ERROR(error) && _signal)
		n2d_kernel_os_free(os, _signal);

	return error;
}

n2d_error_t n2d_kernel_os_signal_destroy(n2d_os_t *os, n2d_pointer signal)
{
	n2d_error_t error     = N2D_SUCCESS;
	n2d_db_t *db	      = os->kernel->db;
	n2d_uintptr_t data    = 0;
	n2d_signal_t *_signal = N2D_NULL;

	ONERROR(n2d_kernel_db_get(db, 0, (n2d_uintptr_t)(signal), N2D_NULL, &data));
	_signal = (n2d_signal_t *)(n2d_uintptr_t)data;

	if (atomic_dec_and_test(&_signal->ref)) {
		ONERROR(n2d_kernel_db_remove(db, 0, _signal->id));
		ONERROR(n2d_kernel_db_free_handle(db, _signal->id));
		/* Free the sgianl. */
		n2d_kernel_os_free(os, _signal);
	}

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_signal_signal(n2d_os_t *os, n2d_pointer signal, n2d_bool_t state)
{
	n2d_error_t error = 0;
	n2d_signal_t *_signal;
	n2d_uintptr_t data;
	unsigned long flags = 0;
	n2d_db_t *db	    = os->kernel->db;

	error = n2d_kernel_db_get(db, 0, (n2d_uintptr_t)(signal), N2D_NULL, &data);

	spin_lock_irqsave(&os->signal_lock, flags);

	_signal = (n2d_signal_t *)(n2d_uintptr_t)data;

	if (N2D_IS_ERROR(error)) {
		spin_unlock_irqrestore(&os->signal_lock, flags);
		ONERROR(error);
	}

	atomic_inc(&_signal->ref);
	spin_unlock_irqrestore(&os->signal_lock, flags);
	ONERROR(error);

	spin_lock(&_signal->lock);

	if (state) {
		_signal->done = 1;
		wake_up(&_signal->wait);
	} else {
		_signal->done = 0;
	}

	spin_unlock(&_signal->lock);

	if (atomic_dec_and_test(&_signal->ref)) {
		ONERROR(n2d_kernel_db_remove(db, 0, _signal->id));
		/* Free the sgianl. */
		n2d_kernel_os_free(os, _signal);
	}

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_signal_wait(n2d_os_t *os, n2d_pointer signal, n2d_bool_t interruptible,
				      n2d_uint32_t wait)
{
	n2d_error_t error;
	n2d_signal_t *_signal = N2D_NULL;
	n2d_db_t *db	      = os->kernel->db;
	int done;
	n2d_uintptr_t data = 0;

	ONERROR(n2d_kernel_db_get(db, 0, (n2d_uintptr_t)(signal), N2D_NULL, &data));
	_signal = (n2d_signal_t *)(n2d_uintptr_t)data;

	spin_lock(&_signal->lock);
	done = _signal->done;
	spin_unlock(&_signal->lock);

	if (done) {
		error = N2D_SUCCESS;

		if (!_signal->manual_reset)
			_signal->done = 0;
	} else if (wait == 0) {
		error = N2D_TIMEOUT;
	} else {
		/* Convert wait to milliseconds. */
		long timeout =
			(wait == N2D_INFINITE) ? MAX_SCHEDULE_TIMEOUT : msecs_to_jiffies(wait);

		long ret;

		if (interruptible)
			ret = wait_event_interruptible_timeout(_signal->wait, _signal->done,
							       timeout);
		else
			ret = wait_event_timeout(_signal->wait, _signal->done, timeout);

		if (likely(ret > 0)) {
			error = N2D_SUCCESS;
			if (!_signal->manual_reset) {
				/* Auto reset. */
				_signal->done = 0;
			}
		} else {
			error = (ret == -ERESTARTSYS) ? N2D_INTERRUPTED : N2D_TIMEOUT;
		}
	}
on_error:
	return error;
}

n2d_error_t n2d_kernel_os_user_signal_create(n2d_os_t *os, n2d_bool_t manual_reset,
					     n2d_uintptr_t *handle)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_pointer signal;

	/* Create a new signal. */
	ONERROR(n2d_kernel_os_signal_create(os, manual_reset, &signal));
	*handle = (n2d_uintptr_t)signal;

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_user_signal_destroy(n2d_os_t *os, n2d_uintptr_t handle)
{
	return n2d_kernel_os_signal_destroy(os, (n2d_pointer)handle);
}

n2d_error_t n2d_kernel_os_user_signal_signal(n2d_os_t *os, n2d_uintptr_t handle, n2d_bool_t state)
{
	return n2d_kernel_os_signal_signal(os, (n2d_pointer)handle, state);
}

n2d_error_t n2d_kernel_os_user_signal_wait(n2d_os_t *os, n2d_uintptr_t handle, n2d_uint32_t wait)
{
	return n2d_kernel_os_signal_wait(os, (n2d_pointer)handle, N2D_TRUE, wait);
}

n2d_error_t n2d_kernel_os_get_page_size(n2d_os_t *os, n2d_uint32_t *page_size)
{
	*page_size = PAGE_SIZE;

	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_os_get_physical_from_handle(n2d_uint64_t handle, n2d_size_t offset,
						   n2d_uint32_t flag, n2d_uint64_t *physical_addr)
{
	n2d_error_t error     = N2D_SUCCESS;
	n2d_uint64_t physical = 0;

	if (flag & N2D_ALLOC_FLAG_WRAP_USER) {
		n2d_wrap_desc_t *_desc = (n2d_wrap_desc_t *)handle;
		n2d_uint32_t offset_inpage;
		n2d_uint32_t index;

		if (_desc->flag & N2D_ALLOC_FLAG_USERMEMORY) {
			unsigned long _offset = offset + _desc->um_desc.offset;
			offset_inpage	      = _offset & ~PAGE_MASK;
			index		      = _offset / PAGE_SIZE;

			if (index >= _desc->um_desc.page_count)
				ONERROR(N2D_INVALID_ARGUMENT);

			switch (_desc->um_desc.type) {
			case UM_PHYSICAL_MAP:
				physical =
					_desc->um_desc.physical + (n2d_uint64_t)index * PAGE_SIZE;
				break;
			case UM_PAGE_MAP:
				physical = page_to_phys(_desc->um_desc.pages[index]);
				break;
			case UM_PFN_MAP:
				physical = (n2d_uint64_t)_desc->um_desc.pfns[index] << PAGE_SHIFT;
				break;
			}

			physical += offset_inpage;

		} else if (_desc->flag & N2D_ALLOC_FLAG_DMABUF) {
			offset_inpage = offset & ~PAGE_MASK;
			index	      = offset / PAGE_SIZE;

			physical = _desc->dmabuf_desc.pagearray[index] + offset_inpage;
		} else {
			ONERROR(N2D_INVALID_ARGUMENT);
		}

	} else if (flag & N2D_ALLOC_FLAG_NOCONTIGUOUS) {
		non_cgs_desc_t *n_desc = (non_cgs_desc_t *)handle;
		n2d_uint32_t i	       = offset / PAGE_SIZE;

		physical = page_to_phys(n_desc->non_cgs_pages[i]);
	} else {
		physical = handle;
	}

	*physical_addr = physical;

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_cpu_to_gpu_phy(n2d_os_t *os, n2d_uint64_t cpu_phy, n2d_uint64_t *gpu_phy)
{
	n2d_error_t error	       = N2D_SUCCESS;
	n2d_linux_platform_t *platform = N2D_NULL;

	platform = os->device->platform;

	if (platform && platform->ops->get_gpu_physical)
		ONERROR(platform->ops->get_gpu_physical(platform, cpu_phy, gpu_phy));
	else
		*gpu_phy = cpu_phy;

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_gpu_to_cpu_phy(n2d_os_t *os, n2d_uint64_t gpu_phy, n2d_uint64_t *cpu_phy)
{
	n2d_error_t error	       = N2D_SUCCESS;
	n2d_linux_platform_t *platform = N2D_NULL;

	platform = os->device->platform;

	if (platform && platform->ops->get_cpu_physical)
		ONERROR(platform->ops->get_cpu_physical(platform, gpu_phy, cpu_phy));
	else
		*cpu_phy = gpu_phy;

on_error:
	return error;
}

static n2d_error_t _import_dma_buf(n2d_os_t *os, struct device *device, n2d_wrap_desc_t *w_desc)
{
	n2d_error_t error		      = N2D_SUCCESS;
	struct sg_table *sgt		      = NULL;
	struct dma_buf_attachment *attachment = NULL;
	struct dma_buf *dmabuf		      = w_desc->dmabuf_desc.usr_dmabuf;
	int npages			      = 0;
	unsigned long *pagearray	      = NULL;
	int i, j, k = 0;
	struct scatterlist *s;

	get_dma_buf(dmabuf);
	attachment = dma_buf_attach(dmabuf, device);
	if (!attachment)
		ONERROR(N2D_NOT_SUPPORTED);

	sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);

	if (!sgt)
		ONERROR(N2D_NOT_SUPPORTED);

#if !NANO2D_MMU_ENABLE
	if (sgt->nents != 1)
		ONERROR(N2D_NOT_SUPPORTED);
#endif
	/* Prepare page array. */
	for_each_sg (sgt->sgl, s, sgt->orig_nents, i)
		npages += (sg_dma_len(s) + PAGE_SIZE - 1) / PAGE_SIZE;

	ONERROR(n2d_kernel_os_allocate(os, npages * sizeof(*pagearray), (n2d_pointer *)&pagearray));

	/* Fill page array. */
	for_each_sg (sgt->sgl, s, sgt->orig_nents, i) {
		for (j = 0; j < (sg_dma_len(s) + PAGE_SIZE - 1) / PAGE_SIZE; j++)
			pagearray[k++] = sg_dma_address(s) + j * PAGE_SIZE;
	}

	w_desc->dmabuf_desc.dmabuf     = dmabuf;
	w_desc->dmabuf_desc.pagearray  = pagearray;
	w_desc->dmabuf_desc.attachment = attachment;
	w_desc->dmabuf_desc.sgt	       = sgt;
	w_desc->dmabuf_desc.npages     = npages;
	w_desc->is_contiguous	       = (sgt->nents == 1) ? N2D_TRUE : N2D_FALSE;
	w_desc->num_pages	       = npages;

	return error;
on_error:
	if (pagearray)
		kfree(pagearray);
	return error;
}

static n2d_error_t _import_physical(n2d_wrap_desc_t *w_desc, n2d_uint64_t physical)
{
	n2d_error_t error = N2D_SUCCESS;

	w_desc->um_desc.type	    = UM_PHYSICAL_MAP;
	w_desc->um_desc.physical    = physical & PAGE_MASK;
	w_desc->um_desc.chunk_count = 1;

	return error;
}

static n2d_error_t _import_pfns(n2d_device_t *device, n2d_wrap_desc_t *w_desc,
				n2d_uintptr_t user_memory, n2d_uint32_t pageCount)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_uint32_t i;
	int result;
	struct device *dev = (struct device *)device->dev;
	struct vm_area_struct *vma;
	unsigned long *pfns;
	int *refs;
	struct page **pages	= N2D_NULL;
	n2d_uint32_t page_count = 0;
	n2d_uint32_t pfn_count	= pageCount;
	n2d_uintptr_t memory	= user_memory;

	if (!current->mm)
		ONERROR(N2D_OUT_OF_RESOURCES);

	down_read(&current_mm_mmap_sem);
	vma = find_vma(current->mm, memory);
	if (!vma)
		ONERROR(N2D_OUT_OF_RESOURCES);

	pfns = kzalloc(pfn_count * sizeof(unsigned long), GFP_KERNEL | __GFP_NOWARN);
	if (!pfns)
		ONERROR(N2D_OUT_OF_MEMORY);

	refs = kzalloc(pfn_count * sizeof(int), GFP_KERNEL | __GFP_NOWARN);
	if (!refs) {
		kfree(pfns);
		ONERROR(N2D_OUT_OF_MEMORY);
	}

	pages = kzalloc(pfn_count * sizeof(void *), GFP_KERNEL | __GFP_NOWARN);
	if (!pages) {
		kfree(pfns);
		kfree(refs);
		ONERROR(N2D_OUT_OF_MEMORY);
	}

	for (i = 0; i < pfn_count; i++) {
		/* protect pfns[i] */
		spinlock_t *ptl;
		pgd_t *pgd;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
		p4d_t *p4d;
#endif
		pud_t *pud;
		pmd_t *pmd;
		pte_t *pte;

		pgd = pgd_offset(current->mm, memory);
		if (pgd_none(*pgd) || pgd_bad(*pgd))
			goto err;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
		p4d = p4d_offset(pgd, memory);
		if (p4d_none(READ_ONCE(*p4d)))
			goto err;

		pud = pud_offset(p4d, memory);
#elif (defined(CONFIG_X86)) && LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
		pud = pud_offset((p4d_t *)pgd, memory);
#elif (defined(CONFIG_CPU_CSKYV2)) && LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
		pud = pud_offset((p4d_t *)pgd, memory);
#else
		pud = pud_offset(pgd, memory);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0) */
		if (pud_none(*pud) || pud_bad(*pud))
			goto err;

		pmd = pmd_offset(pud, memory);
		if (pmd_none(*pmd) || pmd_bad(*pmd))
			goto err;

		pte = pte_offset_map_lock(current->mm, pmd, memory, &ptl);

		if (!pte_present(*pte)) {
			pte_unmap_unlock(pte, ptl);
			goto err;
		}

		pfns[i] = pte_pfn(*pte);
		pte_unmap_unlock(pte, ptl);

		/* Advance to next. */
		memory += PAGE_SIZE;
	}

	for (i = 0; i < pfn_count; i++) {
		if (pfn_valid(pfns[i])) {
			struct page *page = pfn_to_page(pfns[i]);

			refs[i]	 = get_page_unless_zero(page);
			pages[i] = page;
			page_count++;
		}
	}

	if (page_count != pfn_count)
		pr_warn("[n2d kernel]warning: page_count != pfn_count\n");

	w_desc->um_desc.chunk_count = 1;
	for (i = 1; i < pfn_count; i++) {
		if (pfns[i] != pfns[i - 1] + 1)
			++w_desc->um_desc.chunk_count;
	}

	/* if physical from reserved memory */
	if (device->contiguous_base) {
		n2d_uint64_t phy_addr;

		phy_addr = (n2d_uint64_t)pfns[0] << PAGE_SHIFT;
		if (phy_addr >= device->contiguous_base &&
		    phy_addr < device->contiguous_base + device->contiguous_size)
			w_desc->um_desc.alloc_from_res = 1;
	}

	if (!w_desc->um_desc.alloc_from_res)
		printk("[n2d kernel]wrapped: user memory is not from reserved memory.\n");

	/* Map pages to sg table. */
	w_desc->um_desc.pfns_valid = 0;
	if (page_count == pfn_count && !w_desc->um_desc.alloc_from_res) {
		result = sg_alloc_table_from_pages(&w_desc->um_desc.sgt, pages, pfn_count,
						   memory & ~PAGE_MASK, pfn_count * PAGE_SIZE,
						   GFP_KERNEL | __GFP_NOWARN);
		if (unlikely(result < 0)) {
			pr_warn("[n2d core]: %s: sg_alloc_table_from_pages failed\n", __func__);
			goto err;
		}

		result = dma_map_sg(dev, w_desc->um_desc.sgt.sgl, w_desc->um_desc.sgt.nents,
				    DMA_TO_DEVICE);
		if (unlikely(result != w_desc->um_desc.sgt.nents)) {
			sg_free_table(&w_desc->um_desc.sgt);
			pr_warn("[n2d core]: %s: dma_map_sg failed\n", __func__);
			goto err;
		}
		w_desc->um_desc.pfns_valid = 1;
	}

	kfree(pages);
	pages = N2D_NULL;

	w_desc->um_desc.type = UM_PFN_MAP;
	w_desc->um_desc.pfns = pfns;
	w_desc->um_desc.refs = refs;

	return error;

err:
	kfree(pfns);
	kfree(refs);
	kfree(pages);
	printk("[n2d kernel]error: not find pte...\n");
	ONERROR(N2D_INVALID_ARGUMENT);

on_error:
	return error;
}

static n2d_error_t _import_pages(n2d_device_t *device, n2d_wrap_desc_t *w_desc,
				 n2d_uintptr_t user_memory, n2d_uint32_t pageCount, n2d_size_t size,
				 unsigned long flags)
{
	n2d_error_t error = N2D_SUCCESS;
	int i;
	int result;
	struct device *dev     = (struct device *)device->dev;
	n2d_size_t page_count  = pageCount;
	n2d_uintptr_t addr     = user_memory;
	n2d_size_t _size       = size;
	unsigned long vm_flags = flags;
	struct page **pages    = N2D_NULL;

	if ((addr & (cache_line_size() - 1)) || (_size & (cache_line_size() - 1))) {
		/* Not cpu cacheline size aligned, can not support. */
		printk("[n2d kernel]error: cpu cacheline is not aligned...\n");
		ONERROR(N2D_NOT_ALIGNED);
	}

	pages = kcalloc(page_count, sizeof(void *), GFP_KERNEL | __GFP_NOWARN);
	if (!pages)
		ONERROR(N2D_OUT_OF_MEMORY);

	down_read(&current_mm_mmap_sem);
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 6, 0)
	result = pin_user_pages(addr & PAGE_MASK, page_count,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
	result = get_user_pages(current, current->mm, addr & PAGE_MASK, page_count,
#else
	result = get_user_pages(addr & PAGE_MASK, page_count,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0) || defined(CONFIG_PPC)
				(vm_flags & VM_WRITE) ? FOLL_WRITE : 0,
#else
				(vm_flags & VM_WRITE) ? 1 : 0, 0,
#endif
				pages, NULL);
	up_read(&current_mm_mmap_sem);

	if (result < page_count) {
		for (i = 0; i < result; i++) {
			if (pages[i])
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 6, 0)
				unpin_user_page(pages[i]);
#else
				put_page(pages[i]);
#endif
		}

		kfree(pages);
		pages = N2D_NULL;
		printk("[n2d kernel]error: user pages incorrect...\n");
		ONERROR(N2D_INVALID_ARGUMENT);
	}

	w_desc->um_desc.chunk_count = 1;
	for (i = 1; i < page_count; i++) {
		if (page_to_pfn(pages[i]) != page_to_pfn(pages[i - 1]) + 1)
			++w_desc->um_desc.chunk_count;
	}

	/* if physical from reserved memory */
	if (device->contiguous_base) {
		n2d_uint64_t phy_addr;

		phy_addr = (n2d_uint64_t)page_to_pfn(pages[0]) << PAGE_SHIFT;
		if (phy_addr >= device->contiguous_base &&
		    phy_addr < device->contiguous_base + device->contiguous_size)
			w_desc->um_desc.alloc_from_res = 1;
	}

	/* TODO: Map pages to sg table. */
	if (!w_desc->um_desc.alloc_from_res)
		printk("[n2d kernel]This user memory is not from reserved memory.\n");

	/* Map pages to sg table. */
	if (!w_desc->um_desc.alloc_from_res) {
		result = sg_alloc_table_from_pages(&w_desc->um_desc.sgt, pages, page_count,
						   addr & ~PAGE_MASK, page_count * PAGE_SIZE,
						   GFP_KERNEL | __GFP_NOWARN);
		if (unlikely(result < 0)) {
			pr_warn("[n2d core]: %s: sg_alloc_table_from_pages failed\n", __func__);
			ONERROR(N2D_INVALID_ARGUMENT);
		}

		result = dma_map_sg(dev, w_desc->um_desc.sgt.sgl, w_desc->um_desc.sgt.nents,
				    DMA_TO_DEVICE);
		if (unlikely(result != w_desc->um_desc.sgt.nents)) {
			sg_free_table(&w_desc->um_desc.sgt);
			pr_warn("[n2d core]: %s: dma_map_sg failed\n", __func__);
			ONERROR(N2D_INVALID_ARGUMENT);
		}

		/* TODO: iommu */
		dma_sync_sg_for_cpu(dev, w_desc->um_desc.sgt.sgl, w_desc->um_desc.sgt.nents,
				    DMA_FROM_DEVICE);
	}

	w_desc->um_desc.type  = UM_PAGE_MAP;
	w_desc->um_desc.pages = pages;

	return error;
on_error:
	if (pages)
		kfree(pages);
	return error;
}

n2d_error_t n2d_kernel_os_get_sgt(n2d_os_t *os, n2d_vidmem_node_t *node, n2d_pointer *sgt_p)
{
	n2d_error_t error      = N2D_SUCCESS;
	n2d_uint64_t physical  = N2D_INVALID_ADDRESS;
	n2d_uint32_t bytes     = node->size;
	n2d_uint32_t num_pages = PAGE_ALIGN(bytes) >> PAGE_SHIFT;
	n2d_uint32_t flag      = node->flag;
	struct sg_table *sgt   = N2D_NULL;
	struct page *page      = N2D_NULL;
	struct page **pages    = N2D_NULL;
	n2d_uint32_t i;
	n2d_bool_t is_contiguous = flag & N2D_ALLOC_FLAG_CONTIGUOUS;

	/* Only reserved, gfp and dma allocator support to get sgt. */
	if ((flag & 0x70000000) == 0)
		ONERROR(N2D_NOT_SUPPORTED);

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL | __GFP_NOWARN);
	if (!sgt)
		ONERROR(N2D_OUT_OF_MEMORY);

	ONERROR(n2d_kernel_os_get_physical_from_handle(node->handle, 0, flag, &physical));

	if (flag & N2D_ALLOC_FLAG_RESERVED_ALLOCATOR) {
		page = pfn_to_page(physical >> PAGE_SHIFT);

		if (sg_alloc_table(sgt, 1, GFP_KERNEL))
			ONERROR(N2D_GENERIC_IO);

		sg_set_page(sgt->sgl, page, PAGE_ALIGN(bytes), 0);

		*sgt_p = (n2d_pointer)sgt;

		return N2D_SUCCESS;
	} else if (flag & N2D_ALLOC_FLAG_GFP_ALLOCATOR) {
		if (is_contiguous) {
			pages = kmalloc_array(num_pages, sizeof(struct page *),
					      GFP_KERNEL | __GFP_NOWARN);
			if (!pages)
				ONERROR(N2D_OUT_OF_MEMORY);

			page = pfn_to_page(physical >> PAGE_SHIFT);

			for (i = 0; i < num_pages; ++i)
				pages[i] = nth_page(page, i);
		} else {
			non_cgs_desc_t *desc = (non_cgs_desc_t *)node->handle;

			pages = desc->non_cgs_pages;
		}
	} else if (flag & N2D_ALLOC_FLAG_DMA_ALLOCATOR) {
		/* TODO: Add support for iommu. */
		pages = kmalloc_array(num_pages, sizeof(struct page *), GFP_KERNEL | __GFP_NOWARN);
		if (!pages)
			ONERROR(N2D_OUT_OF_MEMORY);
#if !defined(phys_to_page)
		page = virt_to_page(node->klogical);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
		page = phys_to_page(node->handle);
#else
		page = phys_to_page(dma_to_phys(os->device->dev, node->handle));
#endif
		for (i = 0; i < num_pages; ++i)
			pages[i] = nth_page(page, i);
	}

	/* Here we only need to get the sgt of GFP and DMA allocator, reserved memory returned
	 * above. */
	if (sg_alloc_table_from_pages(sgt, pages, num_pages, 0, bytes, GFP_KERNEL) < 0)
		ONERROR(N2D_GENERIC_IO);

	*sgt_p = (n2d_pointer)sgt;

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_memory_mmap(n2d_os_t *os, n2d_vidmem_node_t *node, n2d_pointer vma_p)
{
	n2d_error_t error	   = N2D_SUCCESS;
	struct vm_area_struct *vma = vma_p;
	n2d_uint64_t physical	   = N2D_INVALID_ADDRESS;
	n2d_uint32_t skip_pages	   = vma->vm_pgoff;
	n2d_uint32_t num_pages	   = PAGE_ALIGN(vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
	n2d_uint32_t flag	   = node->flag;
	n2d_bool_t is_contiguous   = flag & N2D_ALLOC_FLAG_CONTIGUOUS;

	ONERROR(n2d_kernel_os_get_physical_from_handle(node->handle, 0, flag, &physical));

#if NANO2D_ENABLE_WRITEBUFFER
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
#else
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#endif

	if (flag & N2D_ALLOC_FLAG_RESERVED_ALLOCATOR) {
		unsigned long pfn = (physical >> PAGE_SHIFT) + skip_pages;

		vma->vm_flags |= n2dVM_FLAGS;

		if (remap_pfn_range(vma, vma->vm_start, pfn, num_pages << PAGE_SHIFT,
				    vma->vm_page_prot) < 0) {
			n2d_kernel_os_print("%s(%d): remap_pfn_range error.\n", __func__, __LINE__);
			ONERROR(N2D_OUT_OF_MEMORY);
		}
	} else if (flag & N2D_ALLOC_FLAG_GFP_ALLOCATOR) {
		vma->vm_flags |= n2dVM_FLAGS;

		/* Now map all the vmalloc pages to this user address. */
		if (is_contiguous) {
			/* map kernel memory to user space.. */
			if (remap_pfn_range(vma, vma->vm_start,
					    (physical >> PAGE_SHIFT) + skip_pages,
					    num_pages << PAGE_SHIFT, vma->vm_page_prot) < 0) {
				n2d_kernel_os_print("%s(%d): remap_pfn_range error.\n", __func__,
						    __LINE__);
				ONERROR(N2D_OUT_OF_MEMORY);
			}
		} else {
			n2d_uint32_t i;
			unsigned long start  = vma->vm_start;
			non_cgs_desc_t *desc = (non_cgs_desc_t *)node->handle;

			for (i = 0; i < num_pages; ++i) {
				unsigned long pfn =
					page_to_pfn(desc->non_cgs_pages[i + skip_pages]);

				if (remap_pfn_range(vma, start, pfn, PAGE_SIZE, vma->vm_page_prot) <
				    0) {
					n2d_kernel_os_print("%s(%d): remap_pfn_range error.\n",
							    __func__, __LINE__);
					ONERROR(N2D_OUT_OF_MEMORY);
				}

				start += PAGE_SIZE;
			}
		}
	} else if (flag & N2D_ALLOC_FLAG_DMA_ALLOCATOR) {
		n2d_int32_t result = 0;
		struct device *dev = (struct device *)os->kernel->device->dev;

		/* map kernel memory to user space. */
#if NANO2D_ENABLE_WRITEBUFFER
#if defined CONFIG_MIPS || defined CONFIG_CPU_CSKYV2 || defined CONFIG_PPC
		result = remap_pfn_range(vma, vma->vm_start, (physical >> PAGE_SHIFT) + skip_pages,
					 num_pages << PAGE_SHIFT,
					 pgprot_writecombine(vma->vm_page_prot));
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
		result =
			dma_mmap_wc(dev, vma, node->klogical + (skip_pages << PAGE_SHIFT),
				    physical + (skip_pages << PAGE_SHIFT), num_pages << PAGE_SHIFT);
#else
		result = dma_mmap_writecombine(
			dev, vma, node->klogical + (skip_pages << PAGE_SHIFT),
			physical + (skip_pages << PAGE_SHIFT), num_pages << PAGE_SHIFT);
#endif
#endif
#else
		result = dma_mmap_coherent(dev, vma, node->klogical + (skip_pages << PAGE_SHIFT),
					   physical + (skip_pages << PAGE_SHIFT),
					   num_pages << PAGE_SHIFT);
#endif /* #if NANO2D_ENABLE_WRITEBUFFER */

		if (result < 0) {
			n2d_kernel_os_print("%s(%d): dma_mmap_attrs error.\n", __func__, __LINE__);
			ONERROR(N2D_OUT_OF_MEMORY);
		}
	}
on_error:
	return error;
}

#if defined(CONFIG_DMA_SHARED_BUFFER)
/*******************************************************************************
 ** Code for dma_buf ops
 ******************************************************************************/
static struct sg_table *_dmabuf_map(struct dma_buf_attachment *attachment,
				    enum dma_data_direction direction)
{
	n2d_error_t error	= N2D_SUCCESS;
	struct sg_table *sgt	= N2D_NULL;
	struct dma_buf *dmabuf	= attachment->dmabuf;
	n2d_vidmem_node_t *node = dmabuf->priv;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	DEFINE_DMA_ATTRS(attrs);
#else
	unsigned long attrs = 0;
#endif

	ONERROR(n2d_kernel_os_get_sgt(node->kernel->os, node, (n2d_pointer *)&sgt));

	if (node->flag & N2D_ALLOC_FLAG_RESERVED_ALLOCATOR) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
#else
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;
#endif
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	if (dma_map_sg_attrs(attachment->dev, sgt->sgl, sgt->nents, direction, &attrs) == 0)
#else
	if (dma_map_sg_attrs(attachment->dev, sgt->sgl, sgt->nents, direction, attrs) == 0)
#endif
	{
		sg_free_table(sgt);
		kfree(sgt);
		sgt = N2D_NULL;
		ONERROR(N2D_GENERIC_IO);
	}

	return sgt;

on_error:
	n2d_kernel_os_print("ERROR: dma_buf ops(map_dma_buf) error(%d)\n", error);
	return N2D_NULL;
}

static void _dmabuf_unmap(struct dma_buf_attachment *attachment, struct sg_table *sgt,
			  enum dma_data_direction direction)
{
	struct dma_buf *dmabuf	= attachment->dmabuf;
	n2d_vidmem_node_t *node = dmabuf->priv;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	DEFINE_DMA_ATTRS(attrs);
#else
	unsigned long attrs = 0;
#endif

	if (node->flag & N2D_ALLOC_FLAG_RESERVED_ALLOCATOR) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
#else
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;
#endif
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	dma_unmap_sg_attrs(attachment->dev, sgt->sgl, sgt->nents, direction, &attrs);
#else
	dma_unmap_sg_attrs(attachment->dev, sgt->sgl, sgt->nents, direction, attrs);
#endif

	sg_free_table(sgt);
	kfree(sgt);
}

static int _dmabuf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	n2d_error_t error	= N2D_SUCCESS;
	n2d_vidmem_node_t *node = dmabuf->priv;

	ONERROR(n2d_kernel_os_memory_mmap(node->kernel->os, node, (n2d_pointer)vma));

	return error;
on_error:
	n2d_kernel_os_print("ERROR: dma_buf ops(mmap) error(%d)\n", error);
	return error;
}

static void _dmabuf_release(struct dma_buf *dmabuf)
{
	n2d_vidmem_node_t *node = dmabuf->priv;

	n2d_kernel_vidmem_node_dec(node->kernel, node);

	return;
}
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 5, 7)
static void *_dmabuf_kmap(struct dma_buf *dmabuf, unsigned long offset)
{
	n2d_error_t error	= N2D_SUCCESS;
	n2d_vidmem_node_t *node = dmabuf->priv;
	n2d_pointer kvaddr	= N2D_NULL;

	offset = (offset << PAGE_SHIFT);

	if (node->klogical) {
		/* Reserved or dma memory, already mapped whole memory. */
		kvaddr = (n2d_uint8_t *)node->klogical;
	} else {
		/* GFP memory need map to kernel. */
		ONERROR(_map_to_kernel(node->kernel->os, node->handle, node->size, node->flag,
				       N2D_MAP_TO_KERNEL, &kvaddr));
		node->klogical = kvaddr;
	}

	kvaddr = (n2d_pointer)((n2d_uint8_t *)kvaddr + offset);

	return kvaddr;
on_error:
	n2d_kernel_os_print("ERROR: dma_buf ops(_dmabuf_kmap) error(%d)\n", error);
	return N2D_NULL;
}

static void _dmabuf_kunmap(struct dma_buf *dmabuf, unsigned long offset, void *ptr)
{
	n2d_vidmem_node_t *node = dmabuf->priv;
	n2d_pointer kvaddr	= (n2d_pointer)((n2d_uint8_t *)ptr - (offset << PAGE_SHIFT));

	if (!node->klogical)
		_unmap_to_kernel(node->kernel->os, kvaddr);
}
#endif

static struct dma_buf_ops _dmabuf_ops = {
	.map_dma_buf   = _dmabuf_map,
	.unmap_dma_buf = _dmabuf_unmap,
	.mmap	       = _dmabuf_mmap,
	.release       = _dmabuf_release,
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 5, 7)
/* VIV: delete map/unmap function*/
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
	.map = _dmabuf_kmap,
	.unmap = _dmabuf_kunmap,
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	.map_atomic   = _dmabuf_kmap,
	.unmap_atomic = _dmabuf_kunmap,
	.map	      = _dmabuf_kmap,
	.unmap	      = _dmabuf_kunmap,
#else
	.kmap_atomic   = _dmabuf_kmap,
	.kunmap_atomic = _dmabuf_kunmap,
	.kmap	       = _dmabuf_kmap,
	.kunmap	       = _dmabuf_kunmap,
#endif
};
#endif /* defined(CONFIG_DMA_SHARED_BUFFER) */

n2d_error_t n2d_kernel_os_vidmem_export(n2d_os_t *os, n2d_vidmem_node_t *node, n2d_uint32_t flags,
					n2d_pointer *dmabuf, n2d_int32_t *fd)
{
#if defined(CONFIG_DMA_SHARED_BUFFER)
	n2d_error_t error	= N2D_SUCCESS;
	struct dma_buf *_dmabuf = N2D_NULL;

	_dmabuf = node->dmabuf;
	if (!_dmabuf) {
		/* Export */
		n2d_uint32_t bytes = node->size;

		bytes = bytes & ~(PAGE_SIZE - 1);

		{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
			DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

			exp_info.ops   = &_dmabuf_ops;
			exp_info.size  = bytes;
			exp_info.flags = flags;
			exp_info.priv  = node;
			_dmabuf	       = dma_buf_export(&exp_info);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
			_dmabuf = dma_buf_export(node, &_dmabuf_ops, bytes, flags, NULL);
#else
			       _dmabuf = dma_buf_export(node, &_dmabuf_ops, bytes, flags);
#endif
		}

		if (IS_ERR(_dmabuf))
			ONERROR(N2D_GENERIC_IO);

		node->dmabuf = _dmabuf;
	}

	if (dmabuf)
		*dmabuf = node->dmabuf;

	if (fd) {
		n2d_int32_t _fd = dma_buf_fd(_dmabuf, flags);

		if (_fd < 0)
			ONERROR(N2D_GENERIC_IO);

		*fd = _fd;
	}

on_error:
	return error;
#else
	n2d_kernel_os_print("[n2d]The kernel did NOT support CONFIG_DMA_SHARED_BUFFER\n");
	return N2D_NOT_SUPPORTED;
#endif
}

n2d_error_t n2d_kernel_os_wrap_memory(n2d_os_t *os, n2d_vidmem_node_t **r_node,
				      n2d_kernel_command_wrap_user_memory_t *u)
{
	n2d_error_t error    = N2D_SUCCESS;
	n2d_device_t *device = os->device;
	struct device *dev   = (struct device *)os->device->dev;
	n2d_wrap_desc_t *w_desc;
	n2d_size_t size	 = 0;
	n2d_bool_t found = N2D_FALSE;

	/* allocate um_desc and init */
	ONERROR(n2d_kernel_os_allocate(os, sizeof(*w_desc), (n2d_pointer *)&w_desc));
	n2d_kernel_os_memory_fill(os, w_desc, 0, sizeof(*w_desc));

	/* wrap user memory */
	if (u->flag & N2D_ALLOC_FLAG_DMABUF) {
#if defined(CONFIG_DMA_SHARED_BUFFER)
		/* wrap dma buf */
		int fd		       = (int)u->fd_handle;
		struct dma_buf *dmabuf = NULL;

		if (fd >= 0) {
			dmabuf = dma_buf_get(fd);
			if (IS_ERR(dmabuf))
				ONERROR(N2D_INVALID_ARGUMENT);

			u->fd_handle		       = -1;
			w_desc->dmabuf_desc.usr_dmabuf = dmabuf;
			size			       = dmabuf->size;

			dma_buf_put(dmabuf);
		} else {
			pr_err("Wrap memory: invalid dmabuf from kernel.\n");
			ONERROR(N2D_INVALID_ARGUMENT);
		}

		if (dmabuf->ops == &_dmabuf_ops) {
			n2d_bool_t referenced	  = N2D_FALSE;
			n2d_vidmem_node_t *f_node = (n2d_vidmem_node_t *)dmabuf->priv;

			do {
				/* Reference the node. */
				error = n2d_kernel_vidmem_node_inc(os->kernel, f_node);
				if (error == N2D_SUCCESS) {
					referenced = N2D_TRUE;
					found	   = N2D_TRUE;
				}

				*r_node		= f_node;
				(*r_node)->size = (n2d_uint32_t)dmabuf->size;

				/* The node is our own node, no desc required. */
				n2d_kernel_os_free(os, w_desc);
			} while (N2D_FALSE);

			if (N2D_IS_ERROR(error) && referenced)
				n2d_kernel_vidmem_node_dec(os->kernel, f_node);
		}
#endif
	}

	if (!found) {
		n2d_vidmem_node_t *node = N2D_NULL;

		ONERROR(n2d_kernel_vidmem_node_construct(os->kernel, &node));

		if (u->flag & N2D_ALLOC_FLAG_DMABUF) {
			ONERROR(_import_dma_buf(os, dev, w_desc));
			w_desc->flag = N2D_ALLOC_FLAG_DMABUF;
		} else if (u->flag & N2D_ALLOC_FLAG_USERMEMORY) {
			/* wrap user memory */
			unsigned long vm_flags	   = 0;
			struct vm_area_struct *vma = NULL;
			n2d_uintptr_t start, end, memory;

			n2d_uint64_t physical;
			n2d_size_t pageCount;

			physical = u->physical;
			memory	 = (physical != N2D_INVALID_ADDRESS) ? (n2d_uintptr_t)physical :
									     u->logical;
			size	 = u->size;

			/* Get the number of required pages. */
			end	  = (memory + size + PAGE_SIZE - 1) >> PAGE_SHIFT;
			start	  = memory >> PAGE_SHIFT;
			pageCount = end - start;

			/* Overflow. */
			if ((memory + size) < memory)
				ONERROR(N2D_INVALID_ARGUMENT);

			/* check all vma */
			memory = u->logical;
			if (memory) {
				n2d_uintptr_t vaddr = 0;

				down_read(&current_mm_mmap_sem);
				vma = find_vma(current->mm, memory);
				up_read(&current_mm_mmap_sem);

				/* No such memory, or across vmas. */
				if (!vma)
					ONERROR(N2D_INVALID_ARGUMENT);

#ifdef CONFIG_ARM
				/* coherent cache in case vivt or vipt-aliasing cache. */
				__cpuc_flush_user_range(memory, memory + size, vma->vm_flags);
#endif
				vm_flags = vma->vm_flags;
				vaddr	 = vma->vm_end;

				down_read(&current_mm_mmap_sem);
				while (vaddr < memory + size) {
					vma = find_vma(current->mm, vaddr);

					if (!vma) {
						/* No such memory. */
						up_read(&current_mm_mmap_sem);
						ONERROR(N2D_INVALID_ARGUMENT);
					}

					if ((vma->vm_flags & VM_PFNMAP) != (vm_flags & VM_PFNMAP)) {
						/* Can not support different map type: both PFN and
						 * PAGE detected. */
						up_read(&current_mm_mmap_sem);
						ONERROR(N2D_NOT_SUPPORTED);
					}

					vaddr = vma->vm_end;
				}
				up_read(&current_mm_mmap_sem);
			} /* if memory */

			/* wrap user memory */
			if (physical != N2D_INVALID_ADDRESS) {
				error = _import_physical(w_desc, physical);
			} else {
				if (vm_flags & VM_PFNMAP)
					error = _import_pfns(device, w_desc, memory, pageCount);
				else
					error = _import_pages(device, w_desc, memory, pageCount,
							      size, vm_flags);
			}

			w_desc->um_desc.vm_flags   = vm_flags;
			w_desc->um_desc.user_vaddr = u->logical;
			w_desc->um_desc.size	   = size;
			w_desc->um_desc.offset	   = (physical != N2D_INVALID_ADDRESS) ?
								   (physical & ~PAGE_MASK) :
								   (memory & ~PAGE_MASK);
			w_desc->um_desc.page_count = pageCount;
			w_desc->is_contiguous	   = (w_desc->um_desc.chunk_count == 1);
			w_desc->num_pages	   = pageCount;
			w_desc->flag		   = N2D_ALLOC_FLAG_USERMEMORY;
		} else {
			ONERROR(N2D_NOT_SUPPORTED);
		}

		if (w_desc->is_contiguous)
			node->flag |= N2D_ALLOC_FLAG_CONTIGUOUS;

		if (w_desc->dmabuf_desc.usr_dmabuf) {
		}
		// xxx_get_flag(w_desc->dmabuf_desc.usr_dmabuf, &node->secure);

		node->handle  = (n2d_uint64_t)w_desc;
		node->logical = u->logical;
		node->size    = size ? size : w_desc->num_pages * PAGE_SIZE;
		node->flag |= N2D_ALLOC_FLAG_WRAP_USER;

		/*
		 * TODO: Now wrapped memory is default cacheable.
		 */
		node->cacheable = N2D_TRUE;

		*r_node = node;
	}
	return error;
on_error:
	if (w_desc)
		n2d_kernel_os_free(os, w_desc);
	return error;
}

n2d_error_t n2d_kernel_os_free_wrapped_mem(n2d_os_t *os, n2d_uint32_t flag, n2d_uint64_t handle)
{
	n2d_error_t error	= N2D_SUCCESS;
	n2d_wrap_desc_t *w_desc = (n2d_wrap_desc_t *)handle;
	n2d_uint32_t w_flag	= w_desc->flag;

	if (!(flag & N2D_ALLOC_FLAG_WRAP_USER))
		ONERROR(N2D_INVALID_ARGUMENT);

	if (w_flag & N2D_ALLOC_FLAG_USERMEMORY) {
		/* free wrapped user memory */
		int i;
		struct page **pages = w_desc->um_desc.pages;

		switch (w_desc->um_desc.type) {
		case UM_PHYSICAL_MAP:
			break;

		case UM_PAGE_MAP:

			for (i = 0; i < w_desc->um_desc.page_count; i++) {
				if (pages[i] && !PageReserved(pages[i]))
					SetPageDirty(pages[i]);

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 6, 0)
				unpin_user_page(pages[i]);
#else
				put_page(pages[i]);
#endif
			}

			kfree(pages);
			break;

		case UM_PFN_MAP:
			for (i = 0; i < w_desc->um_desc.page_count; i++) {
				if (w_desc->um_desc.pfns[i] && pfn_valid(w_desc->um_desc.pfns[i])) {
					struct page *page = pfn_to_page(w_desc->um_desc.pfns[i]);

					if (page && !PageReserved(page))
						SetPageDirty(page);

					if (w_desc->um_desc.refs[i])
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 6, 0)
						unpin_user_page(page);
#else
						put_page(page);
#endif
				}
			}

			kfree(w_desc->um_desc.pfns);
			kfree(w_desc->um_desc.refs);
			break;
		}

		n2d_kernel_os_free(os, w_desc);
	} else if (w_flag & N2D_ALLOC_FLAG_DMABUF) {
		/* free wrapped dma_buf */
		dma_buf_unmap_attachment(w_desc->dmabuf_desc.attachment, w_desc->dmabuf_desc.sgt,
					 DMA_BIDIRECTIONAL);

		dma_buf_detach(w_desc->dmabuf_desc.dmabuf, w_desc->dmabuf_desc.attachment);

		dma_buf_put(w_desc->dmabuf_desc.dmabuf);

		n2d_kernel_os_free(os, w_desc->dmabuf_desc.pagearray);
		n2d_kernel_os_free(os, w_desc);
	} else {
		ONERROR(N2D_INVALID_ARGUMENT);
	}

on_error:
	return error;
}

/* Cache operation. We only need to consider wrapped memory here. */
static n2d_error_t _cache_operation(n2d_os_t *os, n2d_uint64_t handle, n2d_cache_op_t operation)
{
	n2d_error_t error	= N2D_SUCCESS;
	n2d_wrap_desc_t *w_desc = N2D_NULL;
	n2d_uint32_t flag	= 0;
	struct sg_table *sgt	= NULL;
	enum dma_data_direction dir;
	struct device *dev = (struct device *)os->device->dev;

	w_desc = (n2d_wrap_desc_t *)handle;
	flag   = w_desc->flag;

	if (flag == N2D_ALLOC_FLAG_DMABUF) {
		sgt = w_desc->dmabuf_desc.sgt;

	} else if (flag == N2D_ALLOC_FLAG_USERMEMORY) {
		n2d_uint32_t um_type = w_desc->um_desc.type;

		if (um_type == UM_PHYSICAL_MAP || w_desc->um_desc.alloc_from_res) {
			_memory_barrier();
			return N2D_SUCCESS;
		}

		if (um_type == UM_PFN_MAP && w_desc->um_desc.pfns_valid == 0) {
			_memory_barrier();
			return N2D_SUCCESS;
		}
#ifdef CONFIG_ARM
		/* coherent cache in case vivt or vipt-aliasing cache. */
		__cpuc_flush_user_range(w_desc->um_desc.user_vaddr,
					w_desc->um_desc.user_vaddr + w_desc->um_desc.size,
					w_desc->um_desc.vm_flags);
#endif
		sgt = &w_desc->um_desc.sgt;
	} else {
		ONERROR(N2D_INVALID_ARGUMENT);
	}

	switch (operation) {
	case N2D_CACHE_CLEAN:
		dir = DMA_TO_DEVICE;
		dma_sync_sg_for_device(dev, sgt->sgl, sgt->nents, dir);
		break;
	case N2D_CACHE_FLUSH:
		dir = DMA_TO_DEVICE;
		dma_sync_sg_for_device(dev, sgt->sgl, sgt->nents, dir);
		dir = DMA_FROM_DEVICE;
		dma_sync_sg_for_cpu(dev, sgt->sgl, sgt->nents, dir);
		break;
	case N2D_CACHE_INVALIDATE:
		dir = DMA_FROM_DEVICE;
		dma_sync_sg_for_cpu(dev, sgt->sgl, sgt->nents, dir);
		break;
	default:
		return N2D_INVALID_ARGUMENT;
	}

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_cache_flush(n2d_os_t *os, n2d_uint64_t handle)
{
	n2d_error_t error = N2D_SUCCESS;

	ONERROR(_cache_operation(os, handle, N2D_CACHE_FLUSH));

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_cache_clean(n2d_os_t *os, n2d_uint64_t handle)
{
	n2d_error_t error = N2D_SUCCESS;

	ONERROR(_cache_operation(os, handle, N2D_CACHE_CLEAN));

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_cache_invalidate(n2d_os_t *os, n2d_uint64_t handle)
{
	n2d_error_t error = N2D_SUCCESS;

	ONERROR(_cache_operation(os, handle, N2D_CACHE_INVALIDATE));

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_memory_barrier(n2d_os_t *os, n2d_pointer logical)
{
	if (logical != N2D_NULL) {
		/* PCIe order-preserving mode, need ensure data is written into DDR by reading. */
		n2d_kernel_os_memory_read(os, logical, N2D_NULL);
	}

	/* memory barrier. */
	_memory_barrier();

	return N2D_SUCCESS;
}

#if N2D_GPU_TIMEOUT
/* Timer function API. */
static void _timer_function(struct work_struct *work)
{
	/* There should use container_of, but work is the first number of timer, so can typecast. */
	n2d_timer_t *timer = (n2d_timer_t *)work;

	n2d_timer_func function = timer->function;

	function(timer->data);
}

/* Create a timer. */
n2d_error_t n2d_kernel_os_creat_timer(n2d_os_t *os, n2d_timer_func function, n2d_pointer data,
				      n2d_pointer *timer)
{
	n2d_error_t error    = N2D_SUCCESS;
	n2d_timer_t *pointer = N2D_NULL;

	ONERROR(n2d_kernel_os_allocate(os, sizeof(n2d_timer_t), (n2d_pointer)&pointer));

	pointer->function = function;
	pointer->data	  = data;

	INIT_DELAYED_WORK(&pointer->work, _timer_function);

	*timer = pointer;

on_error:
	return error;
}

/* Destroy a timer. */
n2d_error_t n2d_kernel_os_destroy_timer(n2d_os_t *os, n2d_pointer timer)
{
	n2d_timer_t *_timer = (n2d_timer_t *)timer;

	gcmkASSERT(_timer != N2D_NULL);

	cancel_delayed_work_sync(&_timer->work);

	n2d_kernel_os_free(os, _timer);

	return N2D_SUCCESS;
}

/* Start a timer. */
n2d_error_t n2d_kernel_os_start_timer(n2d_os_t *os, n2d_pointer timer, n2d_uint32_t delay)
{
	n2d_timer_t *_timer = (n2d_timer_t *)timer;

	gcmkASSERT(timer != N2D_NULL);
	gcmkASSERT(delay > 0);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	mod_delayed_work(os->workqueue, &_timer->work, msecs_to_jiffies(delay));
	return N2D_SUCCESS;
#else
	n2d_kernel_os_print("LINUX_VERSION_CODE < 3.7.0, not support!\n");
	return N2D_NOT_SUPPORTED;
#endif
}

/* Stop a timer. */
n2d_error_t n2d_kernel_os_stop_timer(n2d_os_t *os, n2d_pointer timer)
{
	n2d_timer_t *_timer = (n2d_timer_t *)timer;

	gcmkASSERT(_timer != N2D_NULL);

	cancel_delayed_work(&_timer->work);

	return N2D_SUCCESS;
}
#endif

n2d_error_t n2d_kernel_os_construct(n2d_device_t *device, n2d_os_t **os)
{
	n2d_error_t error   = N2D_SUCCESS;
	n2d_pointer pointer = N2D_NULL;
	n2d_os_t *_os	    = N2D_NULL;

	pointer = n2d_kmalloc(sizeof(n2d_os_t), GFP_KERNEL);
	if (!pointer)
		return N2D_OUT_OF_MEMORY;

	_os	    = (n2d_os_t *)pointer;
	_os->device = device;
	spin_lock_init(&_os->signal_lock);

	ONERROR(n2d_kernel_os_mutex_create(N2D_NULL, &_os->alloc_mutex));

#if N2D_GPU_TIMEOUT
	/* Create a workqueue for os timer. */
	_os->workqueue = create_singlethread_workqueue("n2d os timer workqueue");
#endif

	*os = _os;

on_error:
	return error;
}

n2d_error_t n2d_kernel_os_destroy(n2d_os_t *os)
{
	n2d_error_t error = N2D_SUCCESS;

#if N2D_GPU_TIMEOUT
	/* Wait for all works done. */
	flush_workqueue(os->workqueue);

	/* Destroy work queue. */
	destroy_workqueue(os->workqueue);
#endif

	ONERROR(n2d_kernel_os_mutex_delete(N2D_NULL, os->alloc_mutex));

	if (os)
		n2d_kfree(os);

on_error:
	return error;
}
