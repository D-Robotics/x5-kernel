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

#ifndef _nano2d_kernel_os_h_
#define _nano2d_kernel_os_h_

#include <linux/spinlock.h>
#include <linux/wait.h>
#include "nano2D_types.h"
#include "nano2D_enum.h"
#include "nano2D_engine.h"
#include "nano2D_dispatch.h"
#include "nano2D_kernel_base.h"
#include "nano2D_kernel_linux.h"

#ifdef __cplusplus
extern "C" {
#endif

struct n2d_os {
	n2d_device_t *device;
	n2d_kernel_t *kernel;
	/* workqueue for os timer. */
#if N2D_GPU_TIMEOUT
	struct workqueue_struct *workqueue;
#endif
	spinlock_t signal_lock;

	/* Alloc reserved memory mutex for multi thread. */
	n2d_pointer alloc_mutex;
};

typedef struct n2d_signal {
	volatile n2d_uint32_t done;
	spinlock_t lock;
	wait_queue_head_t wait;
	n2d_bool_t manual_reset;
	atomic_t ref;
	n2d_uint32_t process;
	n2d_uint32_t id;
} n2d_signal_t;

/* All local variables or pointers are allocated using this function
   to facilitate the later check for memory leaks.
*/
n2d_pointer n2d_kmalloc(size_t size, gfp_t flags);

void n2d_kfree(const n2d_pointer);

n2d_error_t n2d_check_allocate_count(n2d_int32_t *count);

n2d_pointer n2d_kernel_os_usleep(n2d_os_t *os, n2d_uint_t time);

n2d_uint64_t n2d_kernel_os_page_to_phys(n2d_os_t *os, n2d_pointer *pages, n2d_uint32_t index);

n2d_error_t n2d_kernel_os_atom_construct(n2d_os_t *os, n2d_pointer *atom);

n2d_error_t n2d_kernel_os_atom_destroy(n2d_os_t *os, n2d_pointer atom);

n2d_error_t n2d_kernel_os_atom_get(n2d_os_t *os, n2d_pointer atom, n2d_int32_t *value);

n2d_error_t n2d_kernel_os_atom_set(n2d_os_t *os, n2d_pointer atom, n2d_int32_t value);

n2d_error_t n2d_kernel_os_atom_set_mask(n2d_os_t *os, n2d_pointer atom, n2d_uint32_t mask);

n2d_error_t n2d_kernel_os_atom_clear_mask(n2d_os_t *os, n2d_pointer atom, n2d_uint32_t mask);

n2d_error_t n2d_kernel_os_atom_inc(n2d_os_t *os, n2d_pointer atom, n2d_int32_t *old);

n2d_error_t n2d_kernel_os_atom_dec(n2d_os_t *os, n2d_pointer atom, n2d_int32_t *old);

n2d_error_t n2d_kernel_os_mutex_create(n2d_os_t *os, n2d_pointer *mutex);

n2d_error_t n2d_kernel_os_mutex_delete(n2d_os_t *os, n2d_pointer mutex);

n2d_error_t n2d_kernel_os_mutex_acquire(n2d_os_t *os, n2d_pointer mutex, n2d_uint32_t timeout);

n2d_error_t n2d_kernel_os_mutex_release(n2d_os_t *os, n2d_pointer mutex);

n2d_error_t n2d_kernel_os_allocate(n2d_os_t *os, n2d_uint32_t size, n2d_pointer *memory);

n2d_error_t n2d_kernel_os_free(n2d_os_t *os, n2d_pointer memory);

n2d_error_t n2d_kernel_os_memory_fill(n2d_os_t *os, n2d_pointer memory, n2d_uint8_t filler,
				      n2d_uint32_t size);

n2d_error_t n2d_kernel_os_memory_copy(n2d_os_t *os, n2d_pointer dst, n2d_pointer source,
				      n2d_uint32_t size);

n2d_error_t n2d_kernel_os_memory_read(n2d_os_t *os, n2d_pointer logical, n2d_uint32_t *data);

n2d_error_t n2d_kernel_os_memory_write(n2d_os_t *os, n2d_pointer logical, n2d_uint32_t data);

n2d_error_t n2d_kernel_os_copy_to_user(n2d_os_t *os, n2d_pointer ker_ptr, n2d_pointer user_ptr,
				       n2d_uint32_t size);

n2d_error_t n2d_kernel_os_copy_from_user(n2d_os_t *os, n2d_pointer ker_ptr, n2d_pointer user_ptr,
					 n2d_uint32_t size);

void n2d_kernel_os_delay(n2d_os_t *os, n2d_uint32_t milliseconds);

n2d_error_t n2d_kernel_os_get_contiguous_pool(n2d_os_t *os, n2d_vidmem_pool_t name,
					      n2d_pointer *pool);

n2d_error_t n2d_kernel_os_allocate_contiguous(n2d_os_t *os, n2d_pointer pool, n2d_uint32_t *size,
					      n2d_uint32_t flag, n2d_pointer *logical,
					      n2d_uint64_t *physical, n2d_uint32_t aligned);

n2d_error_t n2d_kernel_os_free_contiguous(n2d_os_t *os, n2d_pointer pool, n2d_pointer logical,
					  n2d_uint64_t physical);

n2d_error_t n2d_kernel_os_allocate_noncontiguous(n2d_os_t *os, n2d_uint32_t *size,
						 n2d_uint32_t flag, n2d_uint64_t *handle);

n2d_error_t n2d_kernel_os_free_noncontiguous(n2d_os_t *os, n2d_uint32_t size, n2d_uint64_t handle);

n2d_error_t n2d_kernel_os_dma_alloc(n2d_os_t *os, n2d_uint32_t *size, n2d_uint32_t flag,
				    n2d_pointer *kvaddr, n2d_uint64_t *physical);

n2d_error_t n2d_kernel_os_dma_free(n2d_os_t *os, n2d_uint32_t size, n2d_pointer kvaddr,
				   n2d_uint64_t physical);

n2d_error_t n2d_kernel_os_allocate_paged_memory(n2d_os_t *os, n2d_uint32_t *size, n2d_uint32_t flag,
						n2d_uint64_t *handle);

n2d_error_t n2d_kernel_os_free_paged_memory(n2d_os_t *os, n2d_uint32_t size, n2d_uint32_t flag,
					    n2d_uint64_t handle);

n2d_error_t n2d_kernel_os_unmap_cpu(n2d_os_t *os, n2d_uint32_t process, n2d_pointer logical,
				    n2d_uint32_t size, n2d_map_type_t type);

n2d_error_t n2d_kernel_os_map_cpu(n2d_os_t *os, n2d_uint32_t process, n2d_uint64_t handle,
				  n2d_pointer klogical, n2d_uint32_t size, n2d_uint32_t flag,
				  n2d_map_type_t type, n2d_pointer *logical);

n2d_error_t n2d_kernel_os_unmap_gpu(n2d_os_t *os, n2d_uint32_t core, n2d_uint32_t address,
				    n2d_uint32_t size);

n2d_error_t n2d_kernel_os_map_gpu(n2d_os_t *os, n2d_uint32_t core, n2d_uint64_t handle,
				  n2d_uint32_t size, n2d_uint32_t flag, n2d_uint64_t *address);

n2d_uint32_t n2d_kernel_os_peek_with_core(n2d_os_t *os, n2d_core_id_t core, n2d_uint32_t address);

void n2d_kernel_os_poke_with_core(n2d_os_t *os, n2d_core_id_t core, n2d_uint32_t address,
				  n2d_uint32_t data);

n2d_uint32_t n2d_kernel_os_get_base_address(n2d_os_t *os);

void n2d_kernel_os_trace(char *format, ...);

void n2d_kernel_os_print(char *format, ...);

n2d_error_t n2d_kernel_os_set_gpu_power(n2d_os_t *os, n2d_bool_t power);

n2d_error_t n2d_kernel_os_set_gpu_clock(n2d_os_t *os, n2d_bool_t clock);

n2d_error_t n2d_kernel_os_signal_wait(n2d_os_t *os, n2d_pointer signal, n2d_bool_t interruptible,
				      n2d_uint32_t wait);

n2d_error_t n2d_kernel_os_signal_signal(n2d_os_t *os, n2d_pointer signal, n2d_bool_t state);

n2d_error_t n2d_kernel_os_signal_destroy(n2d_os_t *os, n2d_pointer signal);

n2d_error_t n2d_kernel_os_signal_create(n2d_os_t *os, n2d_bool_t manual_reset, n2d_pointer *signal);

n2d_error_t n2d_kernel_os_user_signal_create(n2d_os_t *os, n2d_bool_t manual_reset,
					     n2d_uintptr_t *handle);

n2d_error_t n2d_kernel_os_user_signal_destroy(n2d_os_t *os, n2d_uintptr_t handle);

n2d_error_t n2d_kernel_os_user_signal_signal(n2d_os_t *os, n2d_uintptr_t handle, n2d_bool_t state);

n2d_error_t n2d_kernel_os_user_signal_wait(n2d_os_t *os, n2d_uintptr_t handle, n2d_uint32_t wait);

n2d_error_t n2d_kernel_os_get_process(n2d_os_t *os, n2d_uint32_t *process);

n2d_error_t n2d_kernel_os_wrap_memory(n2d_os_t *os, n2d_vidmem_node_t **node,
				      n2d_kernel_command_wrap_user_memory_t *u);

n2d_error_t n2d_kernel_os_free_wrapped_mem(n2d_os_t *os, n2d_uint32_t flag, n2d_uint64_t handle);

n2d_error_t n2d_kernel_os_get_sgt(n2d_os_t *os, n2d_vidmem_node_t *node, n2d_pointer *sgt_p);

n2d_error_t n2d_kernel_os_memory_mmap(n2d_os_t *os, n2d_vidmem_node_t *node, n2d_pointer vma);

n2d_error_t n2d_kernel_os_vidmem_export(n2d_os_t *os, n2d_vidmem_node_t *node, n2d_uint32_t flag,
					n2d_pointer *dmabuf, n2d_int32_t *fd);

n2d_error_t n2d_kernel_os_get_physical_from_handle(n2d_uint64_t handle, n2d_size_t offset,
						   n2d_uint32_t flag, n2d_uint64_t *physical_addr);

n2d_error_t n2d_kernel_os_cpu_to_gpu_phy(n2d_os_t *os, n2d_uint64_t cpu_phy, n2d_uint64_t *gpu_phy);

n2d_error_t n2d_kernel_os_gpu_to_cpu_phy(n2d_os_t *os, n2d_uint64_t gpu_phy, n2d_uint64_t *cpu_phy);

n2d_error_t n2d_kernel_os_cache_flush(n2d_os_t *os, n2d_uint64_t handle);

n2d_error_t n2d_kernel_os_cache_clean(n2d_os_t *os, n2d_uint64_t handle);

n2d_error_t n2d_kernel_os_cache_invalidate(n2d_os_t *os, n2d_uint64_t handle);

n2d_error_t n2d_kernel_os_memory_barrier(n2d_os_t *os, n2d_pointer logical);

#if N2D_GPU_TIMEOUT
/* Create a timer. */
n2d_error_t n2d_kernel_os_creat_timer(n2d_os_t *os, n2d_timer_func function, n2d_pointer data,
				      n2d_pointer *timer);

/* Destroy a timer. */
n2d_error_t n2d_kernel_os_destroy_timer(n2d_os_t *os, n2d_pointer timer);

/* Start a timer. */
n2d_error_t n2d_kernel_os_start_timer(n2d_os_t *os, n2d_pointer timer, n2d_uint32_t delay);

/* Stop a timer. */
n2d_error_t n2d_kernel_os_stop_timer(n2d_os_t *os, n2d_pointer timer);
#endif

n2d_error_t n2d_kernel_os_construct(n2d_device_t *device, n2d_os_t **os);

n2d_error_t n2d_kernel_os_destroy(n2d_os_t *os);

n2d_error_t n2d_kernel_os_get_page_size(n2d_os_t *os, n2d_uint32_t *page_size);

#ifdef __cplusplus
}
#endif

#endif /* _nano2d_kernel_os_h_ */
