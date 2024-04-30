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

#ifndef _nano2D_kernel_driver_h_
#define _nano2D_kernel_driver_h_

#include <linux/list.h>
#include <linux/semaphore.h>
#include "nano2D_option.h"
#include "nano2D_kernel_platform.h"
#include "nano2D_kernel_os.h"
#include "nano2D_kernel.h"
#include "nano2D_kernel_debugfs.h"
#include "hobot_vpf_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

struct heap_node {
	struct list_head list;
	n2d_uint32_t offset;
	n2d_size_t size;
	n2d_int32_t status;
};

struct memory_heap {
	n2d_uint32_t free;
	struct list_head list;
	n2d_bool_t limit_4G;
	n2d_pointer klogical;
	n2d_uint64_t physical;
	n2d_uint64_t address;
	n2d_size_t size;
};

struct n2d_gl_device {
	n2d_uint64_t register_bases[NANO2D_CORE_MAX];
	n2d_pointer register_bases_mapped[NANO2D_CORE_MAX];
	n2d_uint32_t register_sizes[NANO2D_CORE_MAX];
	n2d_uint64_t reg_mem_requested[NANO2D_CORE_MAX];

	n2d_linux_platform_t *platform;
	n2d_uint32_t bars[NANO2D_CORE_MAX];

	n2d_int32_t irq_line[NANO2D_CORE_MAX];

	n2d_int32_t device_index[NANO2D_CORE_MAX];

	n2d_bool_t iommu;
	n2d_bool_t recovery;

#ifdef CONFIG_DEBUG_FS
	n2d_debugfs_dir_t debuugfs_dir;
#endif
	n2d_size_t contiguous_size;
	n2d_uint64_t contiguous_base;
	n2d_pointer contiguous_logical;
	n2d_bool_t contiguous_requested;

	n2d_size_t command_contiguous_size;
	n2d_uint64_t command_contiguous_base;
	n2d_bool_t command_contiguous_requested;

	n2d_uint32_t base_address;
	n2d_uint32_t registerAPB;
	n2d_bool_t irq_enabled[NANO2D_CORE_MAX];
	n2d_int_t core_num;
	n2d_bool_t kill_thread;

	n2d_os_t *os;
	n2d_kernel_t *kernel;

	n2d_pointer dev;
	struct task_struct *thread_task[NANO2D_CORE_MAX];
	struct semaphore semas[NANO2D_CORE_MAX];

	struct task_struct *thread_task_polling_irq_state[NANO2D_CORE_MAX];

	struct memory_heap heap;
	struct memory_heap command_heap;
};

#define N2D_CH_MAX 5
#define N2D_IN_MAX 4
#define TS_MAX_GAP 2000

typedef enum n2d_command {
	/* 0 */ scale,
	/* 1 */ overlay,
	/* 2 */ stitch,
	/* 3 */ csc
} n2d_command_t;

struct n2d_config {
	u64 config_addr;   //n2d config address
	u32 config_size;   //n2d config size in 32bit
	u32 ninputs;	   // n2d number of inputs
	u32 input_width[N2D_IN_MAX];  //n2d input width resolution
	u32 input_height[N2D_IN_MAX]; //n2d input height resolution
	u32 input_stride[N2D_IN_MAX];  //n2d input stride (pixel)
	u32 output_width;  //n2d output width resolution
	u32 output_height; //n2d output height resolution
	u32 output_stride;  //n2d output stride (pixel)
	u32 output_format;  //n2d output format
	u8  div_width;     //use in dividing UV dimensions; actually a shift right
	u8  div_height;    //use in dividing UV dimensions; actually a shift right
	u32 total_planes;
	u8 sequential_mode; //sequential processing
	u64 in_buffer_addr[N2D_IN_MAX][VIO_BUFFER_MAX_PLANES];
	u64 out_buffer_addr[VIO_BUFFER_MAX_PLANES];
	n2d_command_t command;
};

struct n2d_subdev {
	struct vio_subdev vdev;
	struct n2d_config config;
};

struct n2d_subnode {
	struct n2d_subdev subdev[N2D_CH_MAX];
	struct horizon_n2d_dev *n2d;
	// struct fps_debug fps;
	unsigned long frame_state;
	unsigned long synced_state;
	long cur_timestamp;
	struct vio_frame *synced_src_frames[N2D_IN_MAX];
	u8 bin_iommu_map;
};

struct horizon_n2d_dev {
	struct n2d_gl_device *galcore;
	struct vio_group_task gtask;
	struct vpf_device vps[N2D_CH_MAX]; /* set 4 as source channel, 1 as output */
	struct n2d_subnode n2d_sub[VIO_MAX_STREAM];
	struct vio_node vnode[VIO_MAX_STREAM];
	// struct n2d_config config[VIO_MAX_STREAM];
};

struct vs_n2d_aux {
	struct device *dev;
	struct n2d_config config;
};

int drv_ioctl_internal(n2d_ioctl_interface_t *iface);

int aux_open(struct vs_n2d_aux *aux);
int aux_close(struct vs_n2d_aux *aux);
#ifdef __cplusplus
}
#endif

#endif
