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

#ifndef _nano2D_kernel_event_h_
#define _nano2D_kernel_event_h_

#include "nano2D_kernel_base.h"
#include "nano2D_kernel.h"
#include "nano2D_driver_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

struct n2d_event {
	struct n2d_event *next_ptr;
	n2d_uint32_t process;
	n2d_ioctl_interface_t iface;
};

struct n2d_event_center {
	n2d_uint32_t core;
	n2d_hardware_t *hardware;
	n2d_pointer pending;
	n2d_pointer pending_event_count;
	n2d_event_t *event_head;
	n2d_event_t *event_tail;
	n2d_event_t *queue[29];
	n2d_uint32_t queue_count;
	n2d_uint32_t last_eventid;
	n2d_pointer mutex;
	n2d_pointer eventid_mutex;
};

n2d_error_t n2d_kernel_event_notify(n2d_event_center_t *event_center);

n2d_error_t n2d_kernel_event_clear(n2d_event_center_t *event_center);

n2d_error_t n2d_kernel_event_interrupt(n2d_event_center_t *event_center, n2d_uint32_t data);

n2d_error_t n2d_kernel_event_dump(n2d_event_center_t *event_center);

n2d_error_t n2d_kernel_query_eventid(n2d_event_center_t *event_center, n2d_uint32_t *eventid);

n2d_error_t n2d_kernel_event_add(n2d_event_center_t *event_center, n2d_ioctl_interface_t *iface);

n2d_error_t n2d_kernel_event_submit(n2d_event_center_t *event_center, n2d_uint32_t wait);

n2d_error_t n2d_kernel_event_commit(n2d_event_center_t *event_center, n2d_user_event_t *queue,
				    n2d_bool_t commit_true);

n2d_error_t n2d_kernel_event_center_construct(n2d_hardware_t *hardware,
					      n2d_event_center_t **event_center);

n2d_error_t n2d_kernel_event_center_destroy(n2d_hardware_t *hardware,
					    n2d_event_center_t *event_center);
#ifdef __cplusplus
}
#endif

#endif /* _nano2D_kernel_event_h_ */
