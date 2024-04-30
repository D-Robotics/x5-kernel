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

#ifndef _nano2D_kernel_hardware_h_
#define _nano2D_kernel_hardware_h_

#include "nano2D_kernel_base.h"
#include "nano2D_types.h"
#include "nano2D_kernel_mmu.h"
#include "nano2D_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_FIFO_SIZE 8

typedef enum n2d_kernel_power_state {
	N2D_POWER_INVALID = 0,
	N2D_POWER_ON,
	N2D_POWER_OFF,
	N2D_POWER_IDLE,
	N2D_POWER_SUSPEND,
} n2d_kernel_power_state_t;

typedef enum n2d_fe_type {
	HW_FE_WAIT_LINK,
	HW_FE_END,
} n2d_fe_type_t;

typedef struct n2d_cmd_info {
	n2d_uint64_t address;
	n2d_uint32_t size;
} n2d_cmd_info_t;

struct n2d_cmd_buf {
	n2d_vidmem_node_t *node;
	n2d_uint32_t *logical;
	n2d_uint64_t address;
	n2d_uint64_t physical;
	n2d_uint32_t *wl_current_logical;
	n2d_uint64_t wl_current_address;
	n2d_uint32_t core;
	n2d_uint32_t size;
	n2d_uint32_t offset;
	n2d_pointer commit_mutex;
	n2d_cmd_info_t *executed_cmd_fifo;
	n2d_uint32_t fifo_index;
};

struct n2d_hardware {
	n2d_uint32_t core; /* global core id. */
	n2d_os_t *os;
	n2d_kernel_t *kernel;
	n2d_sub_device_t *sub_dev;
#if NANO2D_MMU_ENABLE
	n2d_mmu_t mmu;
	n2d_bool_t mmu_pd_mode;
#endif
	n2d_fe_type_t fe_type;
	n2d_cmd_buf_t cmd_buf;
	n2d_bool_t support_64bit;
	n2d_event_center_t *event_center;
	n2d_pointer power_mutex;
	n2d_pointer featureDatabase;
	n2d_bool_t running;
	n2d_uint32_t event_count;
	n2d_kernel_power_state_t power_state;
#if N2D_GPU_TIMEOUT
	n2d_pointer monitor_timer;
	n2d_uint32_t timeout;
	/* Flag to quit monitor timer. */
	n2d_bool_t monitor_timer_stop;
	/* Monitor states. */
	n2d_bool_t monitoring;
	n2d_uint32_t timer;

	n2d_uint32_t total_writes;
#endif
};

n2d_error_t n2d_kernel_hardware_query_chip_identity(
	n2d_hardware_t *hardware, n2d_uint32_t *chipModel, n2d_uint32_t *chipRevision,
	n2d_uint32_t *productId, n2d_uint32_t *cid, n2d_uint32_t *chipFeatures,
	n2d_uint32_t *chipMinorFeatures, n2d_uint32_t *chipMinorFeatures1,
	n2d_uint32_t *chipMinorFeatures2, n2d_uint32_t *chipMinorFeatures3,
	n2d_uint32_t *chipMinorFeatures4);

n2d_error_t n2d_kernel_hardware_interrupt(n2d_hardware_t *hardware);

n2d_error_t n2d_kernel_hardware_notify(n2d_hardware_t *hardware);

n2d_error_t n2d_kernel_hardware_initialize(n2d_hardware_t *hardware);

n2d_error_t n2d_kernel_hardware_deinitialize(n2d_hardware_t *hardware);

n2d_error_t n2d_kernel_hardware_start(n2d_hardware_t *hardware);

n2d_error_t n2d_kernel_hardware_stop(n2d_hardware_t *hardware);

n2d_error_t n2d_kernel_hardware_event(n2d_hardware_t *hardware, n2d_uint32_t *logical,
				      n2d_uint32_t eventid, n2d_uint32_t *size);

n2d_error_t n2d_kernel_hardware_wait_link(n2d_hardware_t *hardware, n2d_uint32_t *logical,
					  n2d_uint64_t address, n2d_uint32_t *size);

n2d_error_t n2d_kernel_hardware_link(n2d_hardware_t *hardware, n2d_uint32_t *logical,
				     n2d_uint64_t address, n2d_uint32_t bytes, n2d_uint32_t *size);

n2d_error_t n2d_kernel_hardware_nop(n2d_hardware_t *hardware, n2d_uint32_t *logical,
				    n2d_uint32_t *size);

n2d_error_t n2d_kernel_hardware_end(n2d_hardware_t *hardware, n2d_uint32_t *logical,
				    n2d_uint32_t *size);

n2d_error_t n2d_kernel_hardware_commit(n2d_hardware_t *hardware, n2d_uint32_t process,
				       n2d_kernel_command_commit_t *iface);

n2d_error_t n2d_kernel_hardware_set_power(n2d_hardware_t *hardware, n2d_kernel_power_state_t state);

n2d_error_t n2d_kernel_harware_dump_command(n2d_hardware_t *hardware);

n2d_error_t n2d_kernel_hardware_dump_gpu_state(n2d_hardware_t *hardware);

n2d_error_t n2d_kernel_hardware_recovery(n2d_hardware_t *hardware);

n2d_error_t n2d_kernel_hardware_construct(n2d_kernel_t *kernel, n2d_uint32_t core,
					  n2d_hardware_t **hardware);

n2d_error_t n2d_kernel_hardware_destroy(n2d_hardware_t *hardware);

#if NANO2D_MMU_ENABLE
n2d_error_t n2d_kernel_hardware_mmu_config(n2d_hardware_t *hardware, IN n2d_mmu_t *Mmu);

n2d_error_t n2d_kernel_hardware_mmu_flush(n2d_hardware_t *hardware, n2d_mmu_t *mmu,
					  n2d_bool_t *need_flush, n2d_uint32_t *size);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _nano2D_kernel_hardware_h_ */
