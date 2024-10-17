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

#ifndef _nano2d_kernel_h_
#define _nano2d_kernel_h_

#include "nano2D_kernel_os.h"
#include "nano2D_kernel_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

struct n2d_sub_device {
	n2d_uint32_t id; /*sub device id.*/

	n2d_kernel_t *kernel;
	n2d_hardware_t *hardware[NANO2D_DEV_CORE_COUNT];
};

struct n2d_kernel {
	n2d_uint32_t dev_num;
	n2d_uint32_t dev_core_num;
	n2d_pointer dev_ref;
	n2d_pointer power_ref;

	n2d_os_t *os;
	n2d_device_t *device;
	n2d_sub_device_t *sub_dev[NANO2D_DEVICE_MAX];
	n2d_db_t *db;
};

/* This is the function to call from the driver to interface with the address. */
n2d_error_t n2d_kernel_dispatch(n2d_kernel_t *kernel, n2d_kernel_command_t command,
				n2d_device_id_t dev_id, n2d_core_id_t core, n2d_pointer u);

n2d_error_t n2d_kernel_construct(n2d_device_t *device, n2d_kernel_t **kernel);

n2d_error_t n2d_kernel_destroy(n2d_kernel_t *kernel);

#ifdef __cplusplus
}
#endif

#endif /* _nano2d_kernel_h_ */
