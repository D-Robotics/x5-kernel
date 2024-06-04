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

#include "nano2D_option.h"
#include "nano2D_types.h"
#include "nano2D_kernel_event.h"
#include "nano2D_kernel_hardware.h"
#include "nano2D_kernel.h"

n2d_error_t n2d_kernel_event_interrupt(n2d_event_center_t *event_center, n2d_uint32_t data)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_uint32_t i	  = 0;
	n2d_int32_t old	  = 0;

	ONERROR(n2d_kernel_os_atom_set_mask(event_center->hardware->os, event_center->pending,
					    data));
	for (i = 0; i < event_center->queue_count; i++) {
		if (data & (1 << i))
			ONERROR(n2d_kernel_os_atom_dec(event_center->hardware->os,
						       event_center->pending_event_count, &old));
	}
on_error:
	return error;
}

n2d_error_t n2d_kernel_event_notify(n2d_event_center_t *event_center)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_os_t *os	  = event_center->hardware->os;
#if !NANO2D_WAIT_LINK_ONLY
	n2d_hardware_t *hardware = event_center->hardware;
#endif
	n2d_bool_t acquired	     = N2D_FALSE;
	n2d_uint32_t pending	     = 0;
	n2d_uint32_t i		     = 0;
	n2d_event_t *event_ptr	     = N2D_NULL;
	n2d_uint32_t mask	     = 0;
	n2d_ioctl_interface_t *iface = N2D_NULL;
	n2d_pointer signal	     = N2D_NULL;

	for (;;) {
		ONERROR(n2d_kernel_os_mutex_acquire(os, event_center->eventid_mutex, N2D_INFINITE));
		acquired = N2D_TRUE;
		ONERROR(n2d_kernel_os_atom_get(os, event_center->pending, (n2d_int32_t *)&pending));

#if NANO2D_WAIT_LINK_ONLY
		/* Regardless of the FE mode, Event 29 is not used. */
		if (pending & (1 << 28)) {
			/* Event ID 29 is not a normal event, but for invalidating pipe, FE end
			 * instruction triggered. */
			pending &= ~(1 << 28);
			ONERROR(n2d_kernel_os_atom_clear_mask(os, event_center->pending,
							      (n2d_uint32_t)1 << 28));
		}
#else
		if (hardware->fe_type == HW_FE_END) {
			if (pending & (1 << 28)) {
				/* Event ID 29 is not a normal event, but for invalidating pipe. */
				pending &= ~(1 << 28);
				ONERROR(n2d_kernel_os_atom_clear_mask(os, event_center->pending,
								      (n2d_uint32_t)1 << 28));
			}
		}
#endif
		if (pending == 0) {
			ONERROR(n2d_kernel_os_mutex_release(os, event_center->eventid_mutex));
			// acquired = N2D_FALSE;
			break;
		}

		if (pending & 0x40000000) {
			n2d_kernel_os_print("n2d core: MMU exception\n");
			pending &= 0xBFFFFFFF;
			mask = (n2d_uint32_t)1 << 30;
		}

		if (pending & 0x80000000) {
			n2d_kernel_os_print("n2d core: AXI BUS ERROR\n");
			pending &= 0x7FFFFFFF;
			mask = (n2d_uint32_t)1 << 31;
		}

		/* Find the oldest pending interrupt. */
		for (i = 0; i < event_center->queue_count; ++i) {
			if ((event_center->queue[i] != N2D_NULL) && (pending & gcmBIT(i))) {
				event_ptr	       = event_center->queue[i];
				mask		       = 1 << i;
				event_center->queue[i] = N2D_NULL;
				break;
			}
		}

		ONERROR(n2d_kernel_os_atom_clear_mask(os, event_center->pending, mask));

		ONERROR(n2d_kernel_os_mutex_release(os, event_center->eventid_mutex));
		acquired = N2D_FALSE;

		while (event_ptr) {
			n2d_event_t *next = event_ptr->next_ptr;
			iface		  = &event_ptr->iface;

			switch (iface->command) {
			case N2D_KERNEL_COMMAND_SIGNAL:
				if (iface->u.command_signal.process) {
					signal = (n2d_pointer)(iface->u.command_signal.handle);
					ONERROR(n2d_kernel_os_signal_signal(os, signal, N2D_TRUE));
				} else {
					ONERROR(n2d_kernel_os_user_signal_signal(
						os, iface->u.command_signal.handle, N2D_TRUE));
				}
				break;

			case N2D_KERNEL_COMMAND_UNMAP:
				break;

			default:
				break;
			}
#if !defined(EMULATOR) && !defined(LINUXEMULATOR)
			ONERROR(n2d_kernel_os_free(os, event_ptr));
#endif
			event_ptr = next;
		}
	}
	return error;

on_error:
	if (acquired)
		n2d_kernel_os_mutex_release(os, event_center->eventid_mutex);
	return error;
}

n2d_error_t n2d_kernel_event_clear(n2d_event_center_t *event_center)
{
	n2d_error_t error	     = N2D_SUCCESS;
	n2d_os_t *os		     = event_center->hardware->os;
	n2d_bool_t acquired	     = N2D_FALSE;
	n2d_uint32_t i		     = 0;
	n2d_event_t *event_ptr	     = N2D_NULL;
	n2d_ioctl_interface_t *iface = N2D_NULL;
	n2d_pointer signal	     = N2D_NULL;

	ONERROR(n2d_kernel_os_atom_set(os, event_center->pending_event_count, 0));
	ONERROR(n2d_kernel_os_atom_set(os, event_center->pending, 0));

	for (;;) {
		ONERROR(n2d_kernel_os_mutex_acquire(os, event_center->eventid_mutex, N2D_INFINITE));
		acquired = N2D_TRUE;

		event_ptr = N2D_NULL;
		/* Find the oldest pending interrupt. */
		for (i = 0; i < event_center->queue_count; ++i) {
			if ((event_center->queue[i] != N2D_NULL)) {
				event_ptr	       = event_center->queue[i];
				event_center->queue[i] = N2D_NULL;
				break;
			}
		}

		if (!event_ptr)
			break;

		ONERROR(n2d_kernel_os_mutex_release(os, event_center->eventid_mutex));
		acquired = N2D_FALSE;

		while (event_ptr) {
			n2d_event_t *next = event_ptr->next_ptr;

			iface = &event_ptr->iface;

			switch (iface->command) {
			case N2D_KERNEL_COMMAND_SIGNAL:
				if (iface->u.command_signal.process) {
					signal = (n2d_pointer)(iface->u.command_signal.handle);
					ONERROR(n2d_kernel_os_signal_signal(os, signal, N2D_TRUE));
				} else {
					ONERROR(n2d_kernel_os_user_signal_signal(
						os, iface->u.command_signal.handle, N2D_TRUE));
				}
				break;

			case N2D_KERNEL_COMMAND_UNMAP:
				break;

			default:
				break;
			}
#if !defined(EMULATOR) && !defined(LINUXEMULATOR)
			ONERROR(n2d_kernel_os_free(os, event_ptr));
#endif
			event_ptr = next;
		}
	}

on_error:
	if (acquired)
		n2d_kernel_os_mutex_release(os, event_center->eventid_mutex);
	return error;
}

n2d_error_t n2d_kernel_event_add(n2d_event_center_t *event_center, n2d_ioctl_interface_t *iface)
{
	n2d_error_t error	 = N2D_SUCCESS;
	n2d_pointer pointer	 = N2D_NULL;
	n2d_event_t *event_ptr	 = N2D_NULL;
	n2d_hardware_t *hardware = event_center->hardware;
	n2d_bool_t get_mutex	 = N2D_FALSE;

	if (iface->command == N2D_KERNEL_COMMAND_SIGNAL ||
	    iface->command == N2D_KERNEL_COMMAND_UNMAP) {
		; /* do nothing */
	} else {
		ONERROR(N2D_INVALID_ARGUMENT);
	}

	ONERROR(n2d_kernel_os_allocate(hardware->os, sizeof(n2d_event_t), &pointer));
	if (!pointer)
		ONERROR(N2D_OUT_OF_MEMORY);
	event_ptr = (n2d_event_t *)pointer;
	ONERROR(n2d_kernel_os_memory_copy(hardware->os, &event_ptr->iface, iface,
					  sizeof(n2d_ioctl_interface_t)));
	ONERROR(n2d_kernel_os_get_process(hardware->os, &event_ptr->process));
	event_ptr->next_ptr = N2D_NULL;

	ONERROR(n2d_kernel_os_mutex_acquire(hardware->os, event_center->mutex, N2D_INFINITE));
	// get_mutex = N2D_TRUE;

	if (event_center->event_tail) {
		event_center->event_tail->next_ptr = event_ptr;
		event_center->event_tail	   = event_ptr;
	} else {
		event_center->event_head = event_center->event_tail = event_ptr;
	}

	n2d_kernel_os_mutex_release(hardware->os, event_center->mutex);

	return error;
on_error:
	if (pointer)
		n2d_kernel_os_free(hardware->os, pointer);
	if (get_mutex)
		n2d_kernel_os_mutex_release(hardware->os, event_center->mutex);
	return error;
}

n2d_error_t n2d_kernel_event_dump(n2d_event_center_t *event_center)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_uint32_t i	  = 0;

	n2d_kernel_os_print("Event list:\n");
	for (i = 0; i < 29; i++) {
		if (event_center->queue[i] == N2D_NULL)
			n2d_kernel_os_print("%d: null\n", i);
		else
			n2d_kernel_os_print("%d: wait\n", i);
	}
	return error;
}

static n2d_error_t n2d_kernel_event_query_eventid(n2d_event_center_t *event_center,
						  n2d_uint32_t wait, n2d_uint32_t *eventid)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_uint32_t id = 0, i = 0;
	n2d_hardware_t *hardware = event_center->hardware;
	n2d_uint32_t count	 = event_center->queue_count;

	if (N2D_NULL == eventid) {
		ONERROR(N2D_INVALID_ARGUMENT);
	}

	while (1) {
		ONERROR(n2d_kernel_os_mutex_acquire(hardware->os, event_center->eventid_mutex,
						    N2D_INFINITE));
		id = event_center->last_eventid;
		for (i = 0; i < count; i++) {
			if (event_center->queue[id] == N2D_NULL) {
				*eventid		   = id;
				event_center->last_eventid = (id + 1) % count;
				n2d_kernel_os_mutex_release(hardware->os,
							    event_center->eventid_mutex);
				return N2D_SUCCESS;
			}
			id = (id + 1) % count;
		}
		n2d_kernel_os_mutex_release(hardware->os, event_center->eventid_mutex);

		if (wait) {
			n2d_kernel_os_delay(hardware->os, 1);
			wait--;
		} else {
			ONERROR(N2D_NOT_FOUND);
		}
	}

on_error:
	return error;
}

n2d_error_t n2d_kernel_event_submit(n2d_event_center_t *event_center, n2d_uint32_t wait)
{
	n2d_error_t error	 = N2D_SUCCESS;
	n2d_uint32_t id		 = 0;
	n2d_hardware_t *hardware = event_center->hardware;
	n2d_cmd_buf_t *cmd_buf	 = &hardware->cmd_buf;
	n2d_uint32_t link_size, wait_link_size, event_size;
	n2d_uint32_t *logical;
	n2d_uint64_t address, event_address;
	n2d_bool_t get_event_mutex  = N2D_FALSE;
	n2d_bool_t get_commit_mutex = N2D_FALSE;
	n2d_int32_t old		    = 0;

	ONERROR(n2d_kernel_os_mutex_acquire(hardware->os, event_center->mutex, N2D_INFINITE));
	get_event_mutex = N2D_TRUE;

	if (event_center->event_head) {
		ONERROR(n2d_kernel_event_query_eventid(event_center, wait, &id));
		event_center->queue[id]	 = event_center->event_head;
		event_center->event_head = event_center->event_tail = N2D_NULL;

		ONERROR(n2d_kernel_os_atom_inc(hardware->os, event_center->pending_event_count,
					       &old));

		ONERROR(n2d_kernel_os_mutex_acquire(hardware->os, cmd_buf->commit_mutex,
						    N2D_INFINITE));
		get_commit_mutex = N2D_TRUE;

		ONERROR(n2d_kernel_hardware_event(hardware, N2D_NULL, 0, &event_size));
		ONERROR(n2d_kernel_hardware_link(hardware, N2D_NULL, 0, 0, &link_size));
		ONERROR(n2d_kernel_hardware_wait_link(hardware, N2D_NULL, 0, &wait_link_size));

		if ((cmd_buf->size - cmd_buf->offset) < (wait_link_size + event_size)) {
			cmd_buf->offset = 0;
		}

		logical	      = (n2d_uint32_t *)((n2d_uint8_t *)cmd_buf->logical + cmd_buf->offset);
		event_address = cmd_buf->address + cmd_buf->offset;
		ONERROR(n2d_kernel_hardware_event(hardware, logical, id, N2D_NULL));
		cmd_buf->offset += event_size;

		logical = (n2d_uint32_t *)((n2d_uint8_t *)cmd_buf->logical + cmd_buf->offset);
		address = cmd_buf->address + cmd_buf->offset;
		ONERROR(n2d_kernel_hardware_wait_link(hardware, logical, address, N2D_NULL));
		cmd_buf->offset += wait_link_size;

		ONERROR(n2d_kernel_hardware_link(hardware, cmd_buf->wl_current_logical,
						 event_address, event_size + wait_link_size,
						 N2D_NULL));

		cmd_buf->wl_current_logical = logical;
		cmd_buf->wl_current_address = address;

		n2d_kernel_os_mutex_release(hardware->os, cmd_buf->commit_mutex);
		// get_commit_mutex = N2D_FALSE;
	}

	n2d_kernel_os_mutex_release(hardware->os, event_center->mutex);
	// get_event_mutex = N2D_FALSE;

	return error;

on_error:
	if (get_event_mutex)
		n2d_kernel_os_mutex_release(hardware->os, event_center->mutex);
	if (get_commit_mutex)
		n2d_kernel_os_mutex_release(hardware->os, cmd_buf->commit_mutex);
	return error;
}

n2d_error_t n2d_kernel_event_commit(n2d_event_center_t *event_center, n2d_user_event_t *queue,
				    n2d_bool_t commit_true)
{
	n2d_error_t error	 = N2D_SUCCESS;
	n2d_hardware_t *hardware = event_center->hardware;
	n2d_os_t *os		 = hardware->os;
	n2d_user_event_t *next	 = queue;
	n2d_user_event_t _event, *event = N2D_NULL;

	/* FE_END mode: signal user signal, notify the cmd_buf is completed */
	n2d_ioctl_interface_t *iface = N2D_NULL;
	n2d_pointer signal	     = N2D_NULL;

	if (!next)
		ONERROR(N2D_INVALID_ARGUMENT);

	while (next) {
		if (is_vmalloc_addr(next)) { /* kernel logical addr */
			event = next;
		} else {
			event = &_event;
			ONERROR(n2d_kernel_os_copy_from_user(os, event, next, sizeof(n2d_user_event_t)));
		}

		if (hardware->fe_type == HW_FE_WAIT_LINK) {
			ONERROR(n2d_kernel_event_add(event_center, &event->iface));
		} else {
			iface = &event->iface;

			switch (iface->command) {
			case N2D_KERNEL_COMMAND_SIGNAL:
				if (iface->u.command_signal.process) {
					signal = (n2d_pointer)(iface->u.command_signal.handle);
					ONERROR(n2d_kernel_os_signal_signal(os, signal, N2D_TRUE));
				} else {
					ONERROR(n2d_kernel_os_user_signal_signal(
						os, iface->u.command_signal.handle, N2D_TRUE));
				}
				break;

			case N2D_KERNEL_COMMAND_UNMAP:
				break;

			default:
				break;
			}
		}
		next = gcmUINT64_TO_PTR(event->next);
	}

	if (hardware->fe_type == HW_FE_WAIT_LINK) {
		if (commit_true)
			ONERROR(n2d_kernel_event_submit(event_center, N2D_INFINITE));
	}
on_error:
	return error;
}

n2d_error_t n2d_kernel_event_center_construct(n2d_hardware_t *hardware,
					      n2d_event_center_t **event_center)
{
	n2d_error_t error		  = N2D_SUCCESS;
	n2d_pointer pointer		  = N2D_NULL;
	n2d_event_center_t *_event_center = N2D_NULL;
	n2d_int32_t i			  = 0;

	ONERROR(n2d_kernel_os_allocate(hardware->os, sizeof(n2d_event_center_t), &pointer));
	if (!pointer)
		ONERROR(N2D_OUT_OF_MEMORY);

	ONERROR(n2d_kernel_os_memory_fill(hardware->os, pointer, 0, sizeof(n2d_event_center_t)));

	_event_center		    = (n2d_event_center_t *)pointer;
	_event_center->core	    = hardware->core;
	_event_center->hardware	    = hardware;
	_event_center->last_eventid = 0;
	_event_center->event_head = _event_center->event_tail = N2D_NULL;
#if NANO2D_WAIT_LINK_ONLY
	_event_center->queue_count = 28;
#else
	_event_center->queue_count = (hardware->fe_type == HW_FE_END) ? 28 : 29;
#endif
	for (i = 0; i < 29; i++) {
		_event_center->queue[i] = N2D_NULL;
	}
	ONERROR(n2d_kernel_os_atom_construct(hardware->os, &_event_center->pending));
	ONERROR(n2d_kernel_os_atom_construct(hardware->os, &_event_center->pending_event_count));
	ONERROR(n2d_kernel_os_mutex_create(hardware->os, &_event_center->mutex));
	ONERROR(n2d_kernel_os_mutex_create(hardware->os, &_event_center->eventid_mutex));
	*event_center = _event_center;

	return error;
on_error:
	if (pointer)
		n2d_kernel_os_free(hardware->os, pointer);
	return error;
}

n2d_error_t n2d_kernel_event_center_destroy(n2d_hardware_t *hardware,
					    n2d_event_center_t *event_center)
{
	if (!event_center)
		return N2D_SUCCESS;

	if (event_center->mutex)
		n2d_kernel_os_mutex_delete(hardware->os, event_center->mutex);
	if (event_center->eventid_mutex)
		n2d_kernel_os_mutex_delete(hardware->os, event_center->eventid_mutex);

	if (event_center->pending)
		n2d_kernel_os_atom_destroy(hardware->os, event_center->pending);
	if (event_center->pending_event_count)
		n2d_kernel_os_atom_destroy(hardware->os, event_center->pending_event_count);

	n2d_kernel_os_free(hardware->os, event_center);

	return N2D_SUCCESS;
}
