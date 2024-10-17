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

#include "nano2D_kernel.h"
#include "nano2D_kernel_hardware.h"
#include "nano2D_kernel_db.h"
#include "nano2D_kernel_vidmem.h"
#include "nano2D_kernel_event.h"

static n2d_error_t do_open(n2d_kernel_t *kernel, n2d_uint32_t process)
{
	n2d_error_t error = N2D_SUCCESS;
	// n2d_uint32_t i = 0, j = 0;
	// n2d_uint32_t dev_num	  = kernel->dev_num;
	// n2d_uint32_t dev_core_num = kernel->dev_core_num;
	// n2d_int32_t dev_ref_old	  = -1;

	// gcmkASSERT(dev_core_num <= NANO2D_DEV_CORE_COUNT);

	ONERROR(n2d_kernel_db_attach_process(kernel->db, process, N2D_TRUE));
	//n2d_kernel_os_print("Attach process:%d\n", process);

	// n2d_kernel_os_atom_inc(kernel->os, kernel->dev_ref, &dev_ref_old);

	// if (dev_ref_old == 0) {
	// 	for (i = 0; i < dev_num; i++) {
	// 		for (j = 0; j < dev_core_num; j++)
	// 			ONERROR(n2d_kernel_hardware_start(kernel->sub_dev[i]->hardware[j]));
	// 	}
	// }

on_error:
	return error;
}

static n2d_error_t do_close(n2d_kernel_t *kernel, n2d_uint32_t process)
{
	n2d_error_t error = N2D_SUCCESS;
	// n2d_uint32_t i = 0, j = 0;
	// n2d_uint32_t dev_num	  = kernel->dev_num;
	// n2d_uint32_t dev_core_num = kernel->dev_core_num;
	// n2d_int32_t dev_ref_old	  = -1;

	// gcmkASSERT(dev_core_num <= NANO2D_DEV_CORE_COUNT);

	// n2d_kernel_os_atom_dec(kernel->os, kernel->dev_ref, &dev_ref_old);

	// if (dev_ref_old == 1) {
	// 	for (i = 0; i < dev_num; i++) {
	// 		for (j = 0; j < dev_core_num; j++)
	// 			ONERROR(n2d_kernel_hardware_stop(kernel->sub_dev[i]->hardware[j]));
	// 	}
	// }

	ONERROR(n2d_kernel_db_attach_process(kernel->db, process, N2D_FALSE));
	// n2d_kernel_os_print("Detach process:%d\n", process);

on_error:
	return error;
}

static n2d_error_t do_get_hw_info(n2d_kernel_t *kernel, n2d_uint32_t sub_dev_id, n2d_core_id_t core,
				  n2d_uint32_t process, n2d_kernel_command_get_hw_info_t *u)
{
	n2d_error_t error	 = N2D_SUCCESS;
	n2d_hardware_t *hardware = kernel->sub_dev[sub_dev_id]->hardware[core];

	gcmkASSERT(process >= 0);

	if (!hardware)
		ONERROR(N2D_INVALID_ARGUMENT);

	if (u) {
		u->dev_core_num = kernel->dev_core_num;
		u->device_num	= kernel->dev_num;

		ONERROR(n2d_kernel_hardware_query_chip_identity(
			hardware, &u->chipModel, &u->chipRevision, &u->productId, &u->cid,
			&u->chipFeatures, &u->chipMinorFeatures, &u->chipMinorFeatures1,
			&u->chipMinorFeatures2, &u->chipMinorFeatures3, &u->chipMinorFeatures4));
	}
on_error:
	return error;
}

static n2d_error_t do_allocate(n2d_kernel_t *kernel, n2d_core_id_t core, n2d_uint32_t process,
			       n2d_kernel_command_allocate_t *u)
{
	n2d_error_t error	= N2D_SUCCESS;
	n2d_uint32_t flag	= u->flag;
	n2d_uint32_t handle	= 0;
	n2d_vidmem_node_t *node = N2D_NULL;
	n2d_uint32_t size	= u->size;

	gcmkASSERT(kernel != N2D_NULL);
	gcmkASSERT(core >= 0);

	if (flag == N2D_ALLOC_FLAG_NONE) {
		/* Try alloc contiguous memory firstly, if OOM, try non-contiguous. */
		error = n2d_kernel_vidmem_allocate(kernel, size, 0, N2D_ALLOC_FLAG_CONTIGUOUS,
						   u->type, u->pool, &node);
		if (N2D_OUT_OF_MEMORY == error)
			ONERROR(n2d_kernel_vidmem_allocate(kernel, size, 0,
							   N2D_ALLOC_FLAG_NOCONTIGUOUS, u->type,
							   u->pool, &node));
	} else {
		ONERROR(n2d_kernel_vidmem_allocate(kernel, size, 0, N2D_ALLOC_FLAG_CONTIGUOUS | flag, u->type, u->pool, &node));
	}

	ONERROR(n2d_kernel_db_get_handle(kernel->db, &handle));
	ONERROR(n2d_kernel_db_insert(kernel->db, process, handle, N2D_VIDMEM_TYPE,
				     (n2d_uintptr_t)(node)));
	u->handle = handle;
	u->size	  = node->size;

	/* Success. */
	return N2D_SUCCESS;

on_error:
	if (node)
		n2d_kernel_vidmem_free(kernel, node);
	if (handle)
		n2d_kernel_db_free_handle(kernel->db, handle);
	return error;
}

static n2d_error_t do_free(n2d_kernel_t *kernel, n2d_core_id_t core, n2d_uint32_t process,
			   n2d_kernel_command_free_t *u)
{
	n2d_error_t error;
	n2d_vidmem_node_t *node = N2D_NULL;
	n2d_uintptr_t data	= 0;
	n2d_int32_t ref		= 0;

	gcmkASSERT(kernel != N2D_NULL);
	gcmkASSERT(core >= 0);

	ONERROR(n2d_kernel_db_get(kernel->db, process, u->handle, N2D_NULL, &data));

	node = (n2d_vidmem_node_t *)(n2d_uintptr_t)data;
	ONERROR(n2d_kernel_os_atom_get(kernel->os, node->ref, &ref));

	/*
	 * If the vidmem node is wrapped, then it may have different handles, so we should free
	 * handle here. But the node is freed only when ref == 1.
	 */
	ONERROR(n2d_kernel_db_remove(kernel->db, process, u->handle));
	ONERROR(n2d_kernel_db_free_handle(kernel->db, u->handle));

	if (ref == 1)
		ONERROR(n2d_kernel_vidmem_free(kernel, node));

on_error:
	return error;
}

static n2d_error_t do_map(n2d_kernel_t *kernel, n2d_core_id_t core, n2d_uint32_t process,
			  n2d_kernel_command_map_t *u)
{
	n2d_error_t error;
	n2d_vidmem_node_t *node = N2D_NULL;
	n2d_uintptr_t data	= 0;
	n2d_map_type_t type;

	gcmkASSERT(kernel != N2D_NULL);
	gcmkASSERT(core >= 0);

	ONERROR(n2d_kernel_db_get(kernel->db, process, u->handle, N2D_NULL, &data));
	node = (n2d_vidmem_node_t *)(n2d_uintptr_t)data;

	if (node->flag & N2D_ALLOC_FLAG_FROM_USER) {
		type = N2D_MAP_TO_GPU | N2D_MAP_TO_USER;
	} else {
		type = N2D_MAP_TO_GPU | N2D_MAP_TO_KERNEL;
	}

	ONERROR(n2d_kernel_vidmem_node_map(kernel, process, type, node));
	ONERROR(n2d_kernel_vidmem_node_get_physical(kernel, node, &u->physical));

	if (node->flag & N2D_ALLOC_FLAG_FROM_USER) {
		u->logical = node->logical;
	} else {
		u->logical = (n2d_uintptr_t)(node->klogical); /* move user logical to kernel, so user klogical */
	}

	u->address = node->address;
	u->secure  = node->secure;

on_error:
	return error;
}

static n2d_error_t do_unmap(n2d_kernel_t *kernel, n2d_core_id_t core, n2d_uint32_t process,
			    n2d_kernel_command_unmap_t *u)
{
	n2d_error_t error;
	n2d_vidmem_node_t *node = N2D_NULL;
	n2d_uintptr_t data	= 0;

	gcmkASSERT(kernel != N2D_NULL);
	gcmkASSERT(core >= 0);

	ONERROR(n2d_kernel_db_get(kernel->db, process, u->handle, N2D_NULL, &data));
	node = (n2d_vidmem_node_t *)(n2d_uintptr_t)data;
	ONERROR(n2d_kernel_vidmem_node_unmap(kernel, process, node));

on_error:
	return error;
}

static n2d_error_t do_commit(n2d_kernel_t *kernel, n2d_uint32_t dev_id, n2d_core_id_t core,
			     n2d_uint32_t process, n2d_kernel_command_commit_t *u)
{
	n2d_error_t error;
	n2d_hardware_t *hardware = N2D_NULL;

	gcmkASSERT(kernel != N2D_NULL);
	gcmkASSERT(core >= 0);
	gcmkASSERT(process >= 0);

	hardware = kernel->sub_dev[dev_id]->hardware[core];

	ONERROR(n2d_kernel_hardware_commit(hardware, process, u));

	/* Success. */
	return N2D_SUCCESS;

on_error:
	u->ret = error;
	return error;
}

static n2d_error_t do_user_signal(n2d_kernel_t *kernel, n2d_core_id_t core, n2d_uint32_t process,
				  n2d_kernel_command_user_signal_t *u)
{
	n2d_error_t error = N2D_SUCCESS;

	gcmkASSERT(kernel != N2D_NULL);
	gcmkASSERT(core >= 0);
	gcmkASSERT(process >= 0);

	switch (u->command) {
	case N2D_USER_SIGNAL_CREATE:
		/* Create a signal used in the user space. */
		ONERROR(n2d_kernel_os_user_signal_create(kernel->os, u->manual_reset, &u->handle));
		// todo add database
		break;
	case N2D_USER_SIGNAL_DESTROY:
		// todo remove database
		/* Destroy the signal. */
		ONERROR(n2d_kernel_os_user_signal_destroy(kernel->os, u->handle));
		break;

	case N2D_USER_SIGNAL_SIGNAL:
		/* Signal the signal. */
		ONERROR(n2d_kernel_os_user_signal_signal(kernel->os, u->handle, u->state));
		break;

	case N2D_USER_SIGNAL_WAIT:
		/* Wait on the signal. */
		error = n2d_kernel_os_user_signal_wait(kernel->os, u->handle, u->wait);
		break;
	}

on_error:
	return error;
}

static n2d_error_t do_event_commit(n2d_kernel_t *kernel, n2d_uint32_t dev_id, n2d_core_id_t core,
				   n2d_uint32_t process, n2d_kernel_command_event_commit_t *u)
{
	n2d_error_t error		 = N2D_SUCCESS;
	n2d_hardware_t *hardware	 = N2D_NULL;
	n2d_event_center_t *event_center = N2D_NULL;
	n2d_user_event_t *queue		 = gcmUINT64_TO_PTR(u->queue);
	n2d_bool_t submit		 = u->submit;

	gcmkASSERT(kernel != N2D_NULL);
	gcmkASSERT(core >= 0);
	gcmkASSERT(process >= 0);

	hardware     = kernel->sub_dev[dev_id]->hardware[core];
	event_center = hardware->event_center;

	ONERROR(n2d_kernel_event_commit(event_center, queue, submit));

on_error:
	return error;
}

static n2d_error_t do_wrap_user_memory(n2d_kernel_t *kernel, n2d_core_id_t core,
				       n2d_uint32_t process,
				       n2d_kernel_command_wrap_user_memory_t *u)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_vidmem_node_t *node;
	n2d_uint32_t handle = 0; /* return to user */

	gcmkASSERT(kernel != N2D_NULL);
	gcmkASSERT(core >= 0);
	gcmkASSERT(process >= 0);

	/* Attach */
	ONERROR(n2d_kernel_os_wrap_memory(kernel->os, &node, u));

	ONERROR(n2d_kernel_db_get_handle(kernel->db, &handle));
	ONERROR(n2d_kernel_db_insert(kernel->db, process, handle, N2D_VIDMEM_TYPE,
				     (n2d_uintptr_t)(node)));

	u->handle = handle;

	return N2D_SUCCESS;
on_error:
	if (node)
		n2d_kernel_vidmem_free(kernel, node);
	if (handle)
		n2d_kernel_db_free_handle(kernel->db, handle);
	return error;
}

static n2d_error_t do_export_vidmem(n2d_kernel_t *kernel, n2d_core_id_t core, n2d_uint32_t process,
				    n2d_kernel_export_video_memory_t *u)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_vidmem_node_t *node;
	n2d_uintptr_t data = 0;
	n2d_int32_t fd	   = -1;

	gcmkASSERT(kernel != N2D_NULL);
	gcmkASSERT(core >= 0);
	gcmkASSERT(process >= 0);

	ONERROR(n2d_kernel_db_get(kernel->db, process, u->handle, N2D_NULL, &data));
	node = (n2d_vidmem_node_t *)(n2d_uintptr_t)data;

	/* Only reserved, gfp and dma allocator support export vidmem. */
	if ((node->flag & 0x70000000) == 0)
		ONERROR(N2D_NOT_SUPPORTED);

	ONERROR(n2d_kernel_vidmem_export(kernel, node, u->flags, N2D_NULL, &fd));

	u->fd = fd;
on_error:
	return error;
}

static n2d_error_t do_cache(n2d_kernel_t *kernel, n2d_core_id_t core, n2d_uint32_t process,
			    n2d_kernel_command_cache_t *u)
{
	n2d_error_t error = N2D_SUCCESS;

	n2d_vidmem_node_t *node = N2D_NULL;
	n2d_uintptr_t data	= 0;

	gcmkASSERT(kernel != N2D_NULL);
	gcmkASSERT(core >= 0);

	ONERROR(n2d_kernel_db_get(kernel->db, process, u->handle, N2D_NULL, &data));
	node = (n2d_vidmem_node_t *)(n2d_uintptr_t)data;

	if (!(node->flag & N2D_ALLOC_FLAG_WRAP_USER)) {
		/* VIV: Now we only need to operate wrapped memory's cache, other types of
		 *      memory are uncacheable.
		 */
		return N2D_SUCCESS;
	}

	ONERROR(n2d_kernel_vidmem_cache_op(kernel, node, u->operation));

on_error:
	return error;
}

n2d_error_t n2d_kernel_dispatch(n2d_kernel_t *kernel, n2d_kernel_command_t command,
				n2d_device_id_t dev_id, n2d_core_id_t core, n2d_pointer u)
{
	n2d_error_t error    = N2D_SUCCESS;
	n2d_uint32_t process = 0;

	ONERROR(n2d_kernel_os_get_process(kernel->os, &process));

	/* Dispatch the command. */
	switch (command) {
	case N2D_KERNEL_COMMAND_OPEN:
		ONERROR(do_open(kernel, process));
		break;

	case N2D_KERNEL_COMMAND_CLOSE:
		ONERROR(do_close(kernel, process));
		break;

	case N2D_KERNEL_COMMAND_ALLOCATE:
		ONERROR(do_allocate(kernel, core, process, u));
		break;

	case N2D_KERNEL_COMMAND_FREE:
		ONERROR(do_free(kernel, core, process, u));
		break;

	case N2D_KERNEL_COMMAND_MAP:
		ONERROR(do_map(kernel, core, process, u));
		break;

	case N2D_KERNEL_COMMAND_UNMAP:
		ONERROR(do_unmap(kernel, core, process, u));
		break;

	case N2D_KERNEL_COMMAND_COMMIT:
		ONERROR(do_commit(kernel, dev_id, core, process, u));
		break;

	case N2D_KERNEL_COMMAND_USER_SIGNAL:
		error = do_user_signal(kernel, core, process, u);
		break;

	case N2D_KERNEL_COMMAND_EVENT_COMMIT:
		ONERROR(do_event_commit(kernel, dev_id, core, process, u));
		break;

	case N2D_KERNEL_COMMAND_GET_HW_INFO:
		ONERROR(do_get_hw_info(kernel, dev_id, core, process, u));
		break;

	case N2D_KERNEL_COMMAND_WRAP_USER_MEMORY:
		ONERROR(do_wrap_user_memory(kernel, core, process, u));
		break;

	case N2D_KERNEL_COMMAND_EXPORT_VIDMEM:
		ONERROR(do_export_vidmem(kernel, core, process, u));
		break;

	case N2D_KERNEL_COMMAND_CACHE:
		ONERROR(do_cache(kernel, core, process, u));
		break;

	default:
		ONERROR(N2D_INVALID_ARGUMENT);
		break;
	}

on_error:
	return error;
}

n2d_error_t n2d_kernel_construct(n2d_device_t *device, n2d_kernel_t **kernel)
{
	n2d_error_t error	  = N2D_SUCCESS;
	n2d_pointer pointer	  = N2D_NULL;
	n2d_kernel_t *_kernel	  = N2D_NULL;
	n2d_sub_device_t *sub_dev = N2D_NULL;
	n2d_uint32_t i = 0, j = 0;
	n2d_uint32_t dev_core_num   = 0; /* cores count per device. */
	n2d_uint32_t dev_num	    = 0; /* device number. */
	n2d_uint32_t global_core_id = 0;
	n2d_int32_t pre_value	    = 0;

	/* Parse sub device info. */
	for (i = 0; i < NANO2D_CORE_MAX; i++) {
		if (device->device_index[i] == -1)
			continue;

		pre_value = device->device_index[j];

		if (device->device_index[i] != pre_value) {
			dev_num++;
			j = i;
		}
	}
	dev_num++;

	/* The number of cores for each device should be same. */
	if (device->core_num % dev_num)
		ONERROR(N2D_INVALID_ARGUMENT);
	dev_core_num = device->core_num / dev_num;

	gcmkASSERT(dev_num <= NANO2D_DEVICE_MAX);
	gcmkASSERT(dev_core_num <= NANO2D_DEV_CORE_COUNT);

	ONERROR(n2d_kernel_os_allocate(device->os, sizeof(n2d_kernel_t), &pointer));
	if (!pointer)
		ONERROR(N2D_OUT_OF_MEMORY);

	_kernel		    = (n2d_kernel_t *)pointer;
	_kernel->device	    = device;
	_kernel->os	    = device->os;
	_kernel->os->kernel = _kernel;

	_kernel->dev_num      = dev_num;
	_kernel->dev_core_num = dev_core_num;

	ONERROR(n2d_kernel_os_atom_construct(_kernel->os, &_kernel->dev_ref));
	ONERROR(n2d_kernel_os_atom_set(_kernel->os, _kernel->dev_ref, 0));
	ONERROR(n2d_kernel_os_atom_construct(_kernel->os, &_kernel->power_ref));
	ONERROR(n2d_kernel_os_atom_set(_kernel->os, _kernel->power_ref, 0));

	ONERROR(n2d_kernel_db_construct(_kernel, &_kernel->db));
	if (!_kernel->db)
		ONERROR(N2D_OUT_OF_MEMORY);

	for (i = 0; i < dev_num; i++) {
		ONERROR(n2d_kernel_os_allocate(device->os, sizeof(n2d_sub_device_t), &pointer));
		sub_dev		= (n2d_sub_device_t *)pointer;
		sub_dev->kernel = _kernel;
		sub_dev->id	= i;

		/* Construct hardware will use kernel->sub_dev. */
		_kernel->sub_dev[i] = sub_dev;

		/* construct hardware for each sub_device. */
		for (j = 0; j < dev_core_num; j++) {
			ONERROR(n2d_kernel_hardware_construct(_kernel, global_core_id,
							      &sub_dev->hardware[j]));
			sub_dev->hardware[j]->sub_dev = sub_dev;
			global_core_id++;
		}
	}

	*kernel = _kernel;

	return error;
on_error:
	if (pointer)
		n2d_kernel_os_free(device->os, pointer);
	return error;
}

n2d_error_t n2d_kernel_destroy(n2d_kernel_t *kernel)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_uint32_t i = 0, j = 0;
	n2d_os_t *os = kernel->os;

	/* Free sub device and it's hardware. */
	for (i = 0; i < kernel->dev_num; i++) {
		gcmkASSERT(kernel->sub_dev[i]->id == i);

		for (j = 0; j < kernel->dev_core_num; j++)
			ONERROR(n2d_kernel_hardware_destroy(kernel->sub_dev[i]->hardware[j]));

		ONERROR(n2d_kernel_os_free(os, kernel->sub_dev[i]));
	}

	if (kernel->db)
		ONERROR(n2d_kernel_db_destroy(kernel, kernel->db));
	if (kernel->power_ref)
		n2d_kernel_os_atom_destroy(kernel->os, kernel->power_ref);
	if (kernel->dev_ref)
		n2d_kernel_os_atom_destroy(kernel->os, kernel->dev_ref);
	if (kernel)
		ONERROR(n2d_kernel_os_free(os, kernel));

on_error:
	return error;
}
