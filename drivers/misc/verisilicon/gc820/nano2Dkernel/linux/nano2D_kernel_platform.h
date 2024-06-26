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

#ifndef __nano2D_kernel_platform_h__
#define __nano2D_kernel_platform_h__

#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/version.h>

#if USE_LINUX_PCIE
#include <linux/pci.h>
#endif

#include "nano2D_option.h"
#include "nano2D_types.h"
#include "nano2D_enum.h"

#define N2D_DEVICE_NAME "nano2d"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
#if USE_LINUX_PCIE
#define gcdIRQF_FLAG (IRQF_SHARED)
#else
#define gcdIRQF_FLAG (IRQF_TRIGGER_HIGH)
#endif
#else
#ifdef gcdIRQ_SHARED
#define gcdIRQF_FLAG (IRQF_DISABLED | IRQF_SHARED | IRQF_TRIGGER_HIGH)
#else
#define gcdIRQF_FLAG (IRQF_DISABLED | IRQF_TRIGGER_HIGH)
#endif
#endif

typedef struct n2d_linux_module_parameters {
	n2d_uint64_t register_bases[NANO2D_CORE_MAX];
	n2d_pointer register_bases_mapped[NANO2D_CORE_MAX];
	n2d_uint32_t register_sizes[NANO2D_CORE_MAX];

	n2d_uint32_t bars[NANO2D_CORE_MAX];

	n2d_int32_t irq_line[NANO2D_CORE_MAX];

	n2d_int32_t device_index[NANO2D_CORE_MAX];

	n2d_bool_t iommu;
	n2d_bool_t recovery;

	n2d_uint64_t contiguous_size;
	n2d_uint64_t contiguous_base;
	n2d_bool_t contiguous_requested;

	n2d_uint64_t command_contiguous_size;
	n2d_uint64_t command_contiguous_base;
	n2d_bool_t command_contiguous_requested;

	n2d_uint32_t base_address;
	n2d_uint32_t registerAPB;
} n2d_linux_module_parameters_t;

typedef struct n2d_linux_platform n2d_linux_platform_t;

typedef struct n2d_linux_operations {
	/*******************************************************************************
	**
	**  needAddDevice
	**
	**  Determine whether platform_device is created by initialization code.
	**  If platform_device is created by BSP, return gcvFLASE here.
	*/
	n2d_bool_t (*needAddDevice)(IN n2d_linux_platform_t *Platform);

	/*******************************************************************************
	**
	**  adjust_param
	**
	**  Override content of arguments, if a argument is not changed here, it will
	**  keep as default value or value set by insmod command line.
	*/
	n2d_error_t (*adjust_param)(IN n2d_linux_platform_t *Platform,
				    OUT n2d_linux_module_parameters_t *Args);

	/*******************************************************************************
	**
	**  adjustDriver
	**
	**  Override content of platform_driver which will be registered.
	*/
	n2d_error_t (*adjustDriver)(IN n2d_linux_platform_t *Platform);

	/*******************************************************************************
	**
	**  getPower
	**
	**  Prepare power and clock operation.
	*/
	n2d_error_t (*getPower)(IN n2d_linux_platform_t *Platform);

	/*******************************************************************************
	**
	**  putPower
	**
	**  Finish power and clock operation.
	*/
	n2d_error_t (*putPower)(IN n2d_linux_platform_t *Platform);

	/*******************************************************************************
	**
	**  allocPriv
	**
	**  Construct platform private data.
	*/
	n2d_error_t (*allocPriv)(IN n2d_linux_platform_t *Platform);

	/*******************************************************************************
	**
	**  freePriv
	**
	**  free platform private data.
	*/
	n2d_error_t (*freePriv)(IN n2d_linux_platform_t *Platform);

	/*******************************************************************************
	**
	**  setPower
	**
	**  Set power state of specified GPU.
	**
	**  INPUT:
	**
	**      n2d_int32_t GPU
	**          GPU need to config.
	**
	**      gceBOOL Enable
	**          Enable or disable power.
	*/
	n2d_error_t (*set_power)(IN n2d_linux_platform_t *Platform, IN n2d_int32_t GPU,
				 IN n2d_bool_t Enable);

	/*******************************************************************************
	**
	**  setClock
	**
	**  Set clock state of specified GPU.
	**
	**  INPUT:
	**
	**      n2d_int32_t GPU
	**          GPU need to config.
	**
	**      gceBOOL Enable
	**          Enable or disable clock.
	*/
	n2d_error_t (*setClock)(IN n2d_linux_platform_t *Platform, IN n2d_int32_t GPU,
				IN n2d_bool_t Enable);

	/*******************************************************************************
	**
	**  reset
	**
	**  Reset GPU outside.
	**
	**  INPUT:
	**
	**      gceBOOL Assert
	**          Assert or deassert reset.
	*/
	n2d_error_t (*reset)(IN n2d_linux_platform_t *Platform, IN n2d_bool_t Assert);

	/*******************************************************************************
	**
	**  getGPUPhysical
	**
	**  Convert CPU physical address to GPU physical address if they are
	**  different.
	*/
	n2d_error_t (*get_gpu_physical)(IN n2d_linux_platform_t *Platform,
					IN n2d_uint64_t CPUPhysical, IN n2d_uint32_t size, n2d_uint64_t *GPUPhysical);

	/*******************************************************************************
	**
	**  get_cpu_physical
	**
	**  Convert GPU physical address to CPU physical address if they are
	**  different.
	*/
	n2d_error_t (*get_cpu_physical)(IN n2d_linux_platform_t *Platform,
					IN n2d_uint64_t GPUPhysical, OUT n2d_uint64_t *CPUPhysical);

	/*******************************************************************************
	**
	**  adjustProt
	**
	**  Override Prot flag when mapping paged memory to userspace.
	*/
	n2d_error_t (*adjustProt)(IN struct vm_area_struct *vma);

	/*******************************************************************************
	**
	**  shrinkMemory
	**
	**  Do something to collect memory, eg, act as oom killer.
	*/
	n2d_error_t (*shrinkMemory)(IN n2d_linux_platform_t *Platform);

	/*******************************************************************************
	**
	**  cache
	**
	**  Cache operation.
	*/
	n2d_error_t (*cache)(IN n2d_linux_platform_t *Platform, IN n2d_uint32_t ProcessID,
			     IN void *Handle, IN n2d_uint32_t Physical, IN void *Logical,
			     IN n2d_size_t Bytes, IN n2d_uint32_t Operation);
} n2d_linux_operations_t;

struct n2d_clk_info {
	const char *clk_name;
	struct clk *clk;
};

struct n2d_clk {
	struct n2d_clk_info *clk_info;
	int clk_num;
};

struct n2d_linux_platform {
	struct platform_device *device;
	struct platform_driver *driver;

	const char *name;
	n2d_linux_operations_t *ops;
	void *priv;
	struct n2d_clk clock;
	struct reset_control *rst;
};

void n2d_kernel_os_query_operations(n2d_linux_operations_t **ops);

int n2d_kernel_platform_init(struct platform_driver *pdrv, n2d_linux_platform_t **platform);
int n2d_kernel_platform_terminate(n2d_linux_platform_t *platform);

#endif /* __nano2D_kernel_platform_h__ */
