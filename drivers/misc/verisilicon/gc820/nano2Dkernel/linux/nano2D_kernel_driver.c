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
#include <asm/io.h>
#include <linux/kthread.h>

#include "nano2D_types.h"
#include "nano2D_dispatch.h"
#include "nano2D_kernel.h"
#include "nano2D_kernel_platform.h"
#include "nano2D_kernel_hardware.h"
#include "nano2D_kernel_event.h"
#include "nano2D_kernel_os.h"
#include "nano2D_user_linux.h"
#include "nano2D_kernel_db.h"
#include "nano2D_kernel_driver.h"

#include "nano2D.h"

#if USE_LINUX_PCIE
#include <linux/cdev.h>
#endif

MODULE_DESCRIPTION("Vivante Graphics Driver");
MODULE_LICENSE("GPL");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0)
MODULE_IMPORT_NS(DMA_BUF);
#endif

static int device_index[NANO2D_CORE_MAX] = {[0 ... NANO2D_CORE_MAX - 1] = -1};
module_param_array(device_index, int, NULL, 0644);
MODULE_PARM_DESC(device_index, "For multi devices, the device id of each core.");

static uint registerAPB = 0x800;
module_param(registerAPB, uint, 0644);
MODULE_PARM_DESC(registerAPB, "The offset of APB register to the register base address.");

static ulong register_base2D = 0x0;
module_param(register_base2D, ulong, 0644);
MODULE_PARM_DESC(register_base2D, "Address of AHB bus of the 2D register.");

static uint register_size2D = 0x20000;
module_param(register_size2D, uint, 0644);
MODULE_PARM_DESC(register_size2D, "Size of AHB bus address range of 2D register.");

static int irq2D = -1;
module_param(irq2D, int, 0644);
MODULE_PARM_DESC(irq2D, "Irq number of 2D.");

static int irqs[NANO2D_CORE_MAX] = {[0 ... NANO2D_CORE_MAX - 1] = -1};
module_param_array(irqs, int, NULL, 0644);
MODULE_PARM_DESC(irqs, "Array of IRQ numbers of multi-2D");

static ulong register_bases[NANO2D_CORE_MAX];
module_param_array(register_bases, ulong, NULL, 0644);
MODULE_PARM_DESC(register_bases, "Array of bases of bus address of register of multi-2D");

static ulong register_sizes[NANO2D_CORE_MAX];
module_param_array(register_sizes, ulong, NULL, 0644);
MODULE_PARM_DESC(register_sizes, "Array of sizes of bus address of register of multi-2D");

static int bars[NANO2D_CORE_MAX] = {[0 ... NANO2D_CORE_MAX - 1] = -1};
module_param_array(bars, int, NULL, 0644);
MODULE_PARM_DESC(bars, "Array of PCI bar numbers of multi-2D");

static int bar2D = -1;
module_param(bar2D, int, 0644);
MODULE_PARM_DESC(bar2D, "PCIE bar index of 2D.");

static ulong contiguous_base = 0x0;
module_param(contiguous_base, ulong, 0644);
MODULE_PARM_DESC(contiguous_base, "Contiguous base of reserved memory of 2D.");

static ulong contiguous_size = 0x0;
module_param(contiguous_size, ulong, 0644);
MODULE_PARM_DESC(contiguous_size, "Contiguous size of reserved memory of 2D.");

static ulong command_contiguous_base = 0x0;
module_param(command_contiguous_base, ulong, 0644);
MODULE_PARM_DESC(command_contiguous_base, "Command contiguous base of reserved memory of 2D.");

static ulong command_contiguous_size = 0x0;
module_param(command_contiguous_size, ulong, 0644);
MODULE_PARM_DESC(command_contiguous_size, "Command contiguous size of reserved memory of 2D.");

static uint polling = 0;
module_param(polling, uint, 0644);
MODULE_PARM_DESC(polling, "Default 0 means disable polling, 1 means polling register status when "
			  "the interrupt is not available.");

static bool iommu = 0;
module_param(iommu, bool, 0644);
MODULE_PARM_DESC(iommu, "Default 0 disable iommu/smmu, 1 means system has iommu/smmu and enabled.");

static bool recovery = 0;
module_param(recovery, bool, 0644);
MODULE_PARM_DESC(recovery, "Recover GPU from stuck(1: Enable, 0: Disable). If enable recovery, "
			   "N2D_GPU_TIMEOUT should be greater than 0.");

#define HEAP_NODE_USED 0xABBAF00D

static uint major		  = 199;
static struct class *device_class = NULL;

static n2d_linux_platform_t *platform		  = N2D_NULL;
static n2d_linux_module_parameters_t global_param = {0};
static struct n2d_gl_device *global_device;
static unsigned int dump_core_mask = 0x00000001;
static struct horizon_n2d_dev *global_n2d;

static int gpu_resume(struct platform_device *dev);
static int gpu_suspend(struct platform_device *dev, pm_message_t state);

struct client_data {
	n2d_device_t *device;
	int refcount;
};

static const char *isr_names[NANO2D_DEVICE_MAX][NANO2D_DEV_CORE_COUNT] = {
	{
		"galcore:dev0_2d0",
		"galcore:dev0_2d1",
		"galcore:dev0_2d2",
		"galcore:dev0_2d3",
		"galcore:dev0_2d4",
		"galcore:dev0_2d5",
		"galcore:dev0_2d6",
		"galcore:dev0_2d7",
	},
	{
		"galcore:dev1_2d0",
		"galcore:dev1_2d1",
		"galcore:dev1_2d2",
		"galcore:dev1_2d3",
		"galcore:dev1_2d4",
		"galcore:dev1_2d5",
		"galcore:dev1_2d6",
		"galcore:dev1_2d7",
	},
	{
		"galcore:dev2_2d0",
		"galcore:dev2_2d1",
		"galcore:dev2_2d2",
		"galcore:dev2_2d3",
		"galcore:dev2_2d4",
		"galcore:dev2_2d5",
		"galcore:dev2_2d6",
		"galcore:dev2_2d7",
	},
	{
		"galcore:dev3_2d0",
		"galcore:dev3_2d1",
		"galcore:dev3_2d2",
		"galcore:dev3_2d3",
		"galcore:dev3_2d4",
		"galcore:dev3_2d5",
		"galcore:dev3_2d6",
		"galcore:dev3_2d7",
	},
	{
		"galcore:dev4_2d0",
		"galcore:dev4_2d1",
		"galcore:dev4_2d2",
		"galcore:dev4_2d3",
		"galcore:dev4_2d4",
		"galcore:dev4_2d5",
		"galcore:dev4_2d6",
		"galcore:dev4_2d7",
	},
	{
		"galcore:dev5_2d0",
		"galcore:dev5_2d1",
		"galcore:dev5_2d2",
		"galcore:dev5_2d3",
		"galcore:dev5_2d4",
		"galcore:dev5_2d5",
		"galcore:dev5_2d6",
		"galcore:dev5_2d7",
	},
	{
		"galcore:dev6_2d0",
		"galcore:dev6_2d1",
		"galcore:dev6_2d2",
		"galcore:dev6_2d3",
		"galcore:dev6_2d4",
		"galcore:dev6_2d5",
		"galcore:dev6_2d6",
		"galcore:dev6_2d7",
	},
	{
		"galcore:dev7_2d0",
		"galcore:dev7_2d1",
		"galcore:dev7_2d2",
		"galcore:dev7_2d3",
		"galcore:dev7_2d4",
		"galcore:dev7_2d5",
		"galcore:dev7_2d6",
		"galcore:dev7_2d7",
	},
};

/********************************************************************/
#ifdef CONFIG_DEBUG_FS
#if NANO2D_MMU_ENABLE
static int mmuinfo_show(struct seq_file *m, void *data)
{
	n2d_mmu_t *mmu = &global_device->kernel->sub_dev[0]->hardware[0]->mmu;
	n2d_uint32_t i, line, left;
	n2d_uint32_t ext_mtlb, config;
	n2d_uint32_t *buf	   = (n2d_uint32_t *)mmu->config->mmu_init_buffer_logical;
	n2d_uint32_t reserve_bytes = mmu->config->node->size;
	n2d_uint64_t physical;
	n2d_uint64_t descriptor = 0;

	physical = mmu->config->mtlb_physical;
	config	 = (n2d_uint32_t)(physical & 0xFFFFFFFF);
	ext_mtlb = (n2d_uint32_t)(physical >> 32);
	config |= gcmSETFIELDVALUE(0, GCREG_MMU_CONFIGURATION, MODE, MODE4_K);

	descriptor = ((n2d_uint64_t)ext_mtlb << 32) | (n2d_uint64_t)config;

	n2d_kernel_os_print("MMU descriptor: 0x%016X\n", descriptor);

	line = reserve_bytes / 32;
	left = reserve_bytes % 32;
	gcmkASSERT(left == 0);

	n2d_kernel_os_print("MMU init cmd buffer:\n");
	for (i = 0; i < line; i++) {
		n2d_kernel_os_print("  %08X %08X %08X %08X %08X %08X %08X %08X\n", buf[0], buf[1],
				    buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
		buf += 8;
	}

	/* Print MMU page table. */
	n2d_mmu_dump_page_table(mmu);

	return 0;
}
#endif
static int dump_trigger_show(struct seq_file *m, void *data)
{
	int i = 0, j = 0;

	for (i = 0; i < global_device->kernel->dev_num; i++) {
		for (j = 0; j < global_device->kernel->dev_core_num; j++) {
			if (global_device->kernel->sub_dev[i]->hardware[j])
				n2d_kernel_hardware_dump_gpu_state(
					global_device->kernel->sub_dev[i]->hardware[j]);
		}
	}

	return 0;
}

static int dump_trigger_store(const char __user *buf, size_t count, void *data)
{
	unsigned int mask = 0;

	sscanf((__force char const *)buf, "%d", &mask);

	if (mask) {
		dump_core_mask = mask;
	} else {
		n2d_kernel_os_print("Each bit represents a core!\n");
	}

	return count;
}

static n2d_debug_info_t info_list[] = {
#if NANO2D_MMU_ENABLE
	{"mmu", mmuinfo_show},
#endif
	{"dump_trigger", dump_trigger_show, dump_trigger_store},
};
#endif

static n2d_error_t create_debug_fs(struct n2d_gl_device *gl_device)
{
	n2d_error_t error = N2D_SUCCESS;

#ifdef CONFIG_DEBUG_FS
	n2d_debugfs_dir_t *dir = &gl_device->debuugfs_dir;

	ONERROR(n2d_debugfs_dir_init(dir, N2D_NULL, "n2d"));
	ONERROR(n2d_debugfs_dir_crestefiles(dir, info_list, N2D_COUNTOF(info_list), gl_device));
#endif

on_error:
	return error;
}

static n2d_error_t destroy_debug_fs(struct n2d_gl_device *gl_device)
{
#ifdef CONFIG_DEBUG_FS
	n2d_debugfs_dir_t *dir = &gl_device->debuugfs_dir;

	if (dir->root) {
		n2d_debugfs_dir_removefiles(dir, info_list, N2D_COUNTOF(info_list));

		n2d_debugfs_dir_deinit(dir);
	}
#endif
	return N2D_SUCCESS;
}

/********************************************************************/
static irqreturn_t isr_routine(int irq, void *ctxt)
{
	n2d_error_t error      = N2D_SUCCESS;
	n2d_device_t *device   = global_device;
	n2d_device_id_t dev_id = 0;
	n2d_core_id_t core_id  = 0;
	/*
	 * VIV: match the input of request_irq function.
	 */
	n2d_core_id_t core = (n2d_core_id_t)gcmPTR2INT(ctxt) - 1;

	/* Find this core's sub device id and core id. */
	dev_id	= core / device->kernel->dev_core_num;
	core_id = core % device->kernel->dev_core_num;

	/* Call kernel interrupt notification. */
	error = n2d_kernel_hardware_interrupt(device->kernel->sub_dev[dev_id]->hardware[core_id]);

	if (N2D_IS_SUCCESS(error)) {
		up(&device->semas[core]);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int polling_isr_state_routine(void *ctxt)
{
	n2d_error_t error	 = N2D_SUCCESS;
	n2d_device_t *device	 = global_device;
	n2d_core_id_t core	 = (n2d_core_id_t)gcmPTR2INT(ctxt);
	n2d_hardware_t *hardware = N2D_NULL;
	n2d_uint32_t data	 = 0;
	n2d_device_id_t dev_id	 = 0;
	n2d_core_id_t core_id	 = 0;

	/* Find this core's sub device id and core id. */
	dev_id	 = core / device->kernel->dev_core_num;
	core_id	 = core % device->kernel->dev_core_num;
	hardware = device->kernel->sub_dev[dev_id]->hardware[core_id];

	error = n2d_kernel_hardware_interrupt(hardware);
	/* Read irq status register. */

	for (;;) {
		if (unlikely(global_device->kill_thread)) {
			/* The daemon exits. */
			while (!kthread_should_stop()) {
				n2d_kernel_os_delay(global_device->os, 1);
			}
			return 0;
		}

		data = n2d_kernel_os_peek_with_core(hardware->os, hardware->core, 0x00010);

		if (data) {
			/* Inform event center of the interrupt. */
			error = n2d_kernel_event_interrupt(hardware->event_center, data);
		}

		if (N2D_IS_SUCCESS(error))
			up(&device->semas[core]);
	}

	/* Return the error. */
	return error;
}

static int thread_routine(void *ctxt)
{
	n2d_device_t *device   = global_device;
	n2d_core_id_t core     = (n2d_core_id_t)gcmPTR2INT(ctxt);
	n2d_device_id_t dev_id = 0;
	n2d_core_id_t core_id  = 0;

	/* Find this core's sub device id and core id. */
	dev_id	= core / device->kernel->dev_core_num;
	core_id = core % device->kernel->dev_core_num;

	for (;;) {
		int down;

		down = down_interruptible(&device->semas[core]);
		if (down && down != -EINTR)
			return down;

		if (unlikely(global_device->kill_thread)) {
			/* The daemon exits. */
			while (!kthread_should_stop()) {
				n2d_kernel_os_delay(global_device->os, 1);
			}
			return 0;
		}

		n2d_kernel_hardware_notify(device->kernel->sub_dev[dev_id]->hardware[core_id]);
	}
}

static n2d_error_t _start_thread(n2d_device_t *device, n2d_device_id_t dev_id,
				 n2d_core_id_t core_id)
{
	n2d_error_t error = N2D_SUCCESS;
	struct task_struct *task;
	n2d_uint64_t global_core_id = 0;

	/* Find this core's global core id. */
	global_core_id = dev_id * NANO2D_DEV_CORE_COUNT + core_id;

	if (device->kernel->sub_dev[dev_id]->hardware[core_id] != N2D_NULL) {
		/* Start the kernel thread. */
		task = kthread_run(thread_routine, (void *)global_core_id, "galcore_deamon/%lld",
				   global_core_id);

		if (IS_ERR(task))
			ONERROR(N2D_GENERIC_IO);

		device->thread_task[global_core_id] = task;
		/*VIV: Set highest non RT priority, same as work queues.*/
		set_user_nice(task, -20);
	}
on_error:
	return error;
}

static int _stop_thread(n2d_device_t *device, n2d_device_id_t dev_id, n2d_core_id_t core_id)
{
	n2d_uint32_t global_core_id = dev_id * NANO2D_DEV_CORE_COUNT + core_id;

	device->kill_thread = N2D_TRUE;
	up(&device->semas[global_core_id]);
	kthread_stop(device->thread_task[global_core_id]);
	device->thread_task[global_core_id] = N2D_NULL;

	return N2D_SUCCESS;
}

static n2d_error_t _setup_isr(n2d_device_t *device, n2d_device_id_t dev_id, n2d_core_id_t core_id)
{
	n2d_int32_t ret	  = 0;
	n2d_error_t error = N2D_SUCCESS;
	irq_handler_t handler;
	n2d_uint32_t global_core_id = dev_id * NANO2D_DEV_CORE_COUNT + core_id;

	handler = isr_routine;

	/*
	 * VIV: Avoid request_irq parameter dev_id is null. here add 1 to core id.
	 */
	ret = request_irq(device->irq_line[global_core_id], handler, IRQF_SHARED,
			  isr_names[dev_id][core_id], (void *)(uintptr_t)(global_core_id + 1));

	device->irq_enabled[global_core_id] = 1;

	if (ret != 0)
		ONERROR(N2D_GENERIC_IO);

on_error:
	return error;
}

static int _release_isr(n2d_device_t *device, n2d_device_id_t dev_id, n2d_core_id_t core_id)
{
	n2d_uint32_t global_core_id = dev_id * NANO2D_DEV_CORE_COUNT + core_id;
	/*
	 * VIV: Avoid free_irq parameter dev_id is null. here add 1 to core id.
	 */
	free_irq(device->irq_line[global_core_id], (void *)(uintptr_t)(global_core_id + 1));

	return N2D_SUCCESS;
}

static n2d_error_t _start_thread_polling_irq_status(n2d_device_t *device, n2d_device_id_t dev_id,
						    n2d_core_id_t core_id)
{
	n2d_error_t error = N2D_SUCCESS;
	struct task_struct *task;
	/* This core's global core id. */
	n2d_uint64_t global_core_id = dev_id * NANO2D_DEV_CORE_COUNT + core_id;

	if (device->kernel->sub_dev[dev_id]->hardware[core_id] != N2D_NULL) {
		/* Start the kernel thread. */
		task = kthread_run(polling_isr_state_routine, (void *)global_core_id,
				   "polling_irq_status_deamon/%lld", global_core_id);

		if (IS_ERR(task))
			ONERROR(N2D_GENERIC_IO);

		device->thread_task_polling_irq_state[global_core_id] = task;
		/*VIV: Set highest non RT priority, same as work queues.*/
		set_user_nice(task, -20);
	}
on_error:
	return error;
}

static int _stop_thread_polling_irq_status(n2d_device_t *device, n2d_device_id_t dev_id,
					   n2d_core_id_t core_id)
{
	n2d_uint32_t global_core_id = dev_id * NANO2D_DEV_CORE_COUNT + core_id;

	device->kill_thread = N2D_TRUE;
	kthread_stop(device->thread_task_polling_irq_state[global_core_id]);
	device->thread_task_polling_irq_state[global_core_id] = N2D_NULL;

	return N2D_SUCCESS;
}

static int start_device(n2d_device_t *device)
{
	n2d_error_t error	    = N2D_SUCCESS;
	n2d_device_id_t dev_id	    = 0;
	n2d_core_id_t core_id	    = 0;
	n2d_uint32_t global_core_id = 0;

	for (dev_id = 0; dev_id < NANO2D_DEVICE_MAX; dev_id++) {
		for (core_id = 0; core_id < NANO2D_DEV_CORE_COUNT; core_id++) {
			if (device->irq_line[global_core_id] == -1)
				continue;

			gcmkASSERT(global_core_id == (dev_id * NANO2D_DEV_CORE_COUNT + core_id));

			sema_init(&device->semas[global_core_id], 0);
			ONERROR(_start_thread(device, dev_id, core_id));

			if (polling)
				ONERROR(_start_thread_polling_irq_status(device, dev_id, core_id));
			else
				ONERROR(_setup_isr(device, dev_id, core_id));

			global_core_id++;
		}
	}
	return 0;
on_error:
	return -1;
}

static int stop_device(n2d_device_t *device)
{
	n2d_device_id_t dev_id	    = 0;
	n2d_core_id_t core_id	    = 0;
	n2d_uint32_t global_core_id = 0;

	for (dev_id = 0; dev_id < NANO2D_DEVICE_MAX; dev_id++) {
		for (core_id = 0; core_id < NANO2D_DEV_CORE_COUNT; core_id++) {
			if (device->irq_line[global_core_id] == -1)
				continue;

			gcmkASSERT(global_core_id == (dev_id * NANO2D_DEV_CORE_COUNT + core_id));

			if (polling)
				_stop_thread_polling_irq_status(device, dev_id, core_id);
			else
				_release_isr(device, dev_id, core_id);

			_stop_thread(device, dev_id, core_id);

			global_core_id++;
		}
	}

	return N2D_SUCCESS;
}

static int init_param(n2d_linux_module_parameters_t *param)
{
	int i = 0;

	if (!param)
		return N2D_INVALID_ARGUMENT;

	for (i = 0; i < NANO2D_CORE_MAX; i++) {
		param->irq_line[i]	 = -1;
		param->bars[i]		 = -1;
		param->device_index[i]	 = -1;
		param->register_bases[i] = 0;
	}

	param->contiguous_size	       = 0;
	param->contiguous_base	       = 0;
	param->command_contiguous_size = 0;
	param->command_contiguous_base = 0;

	return N2D_SUCCESS;
}

static int sync_input_param(n2d_linux_module_parameters_t *param)
{
	int i = 0;

	param->iommu	= iommu;
	param->recovery = recovery;

	if (irq2D != -1) {
		param->irq_line[0]	 = irq2D;
		param->bars[0]		 = bar2D;
		param->register_bases[0] = register_base2D;
		param->register_sizes[0] = register_size2D;
	} else {
		for (i = 0; i < NANO2D_CORE_MAX; i++) {
			if (irqs[i] != -1) {
				param->irq_line[i]	 = irqs[i];
				param->bars[i]		 = bars[i];
				param->device_index[i]	 = device_index[i];
				param->register_bases[i] = register_bases[i];
				param->register_sizes[i] = register_sizes[i];
			}
		}
	}

	param->registerAPB	       = registerAPB;
	param->contiguous_base	       = contiguous_base;
	param->contiguous_size	       = contiguous_size;
	param->command_contiguous_base = command_contiguous_base;
	param->command_contiguous_size = command_contiguous_size;

	return N2D_SUCCESS;
}

static int sync_param(n2d_linux_module_parameters_t *param)
{
	int i = 0;

	if (global_device == NULL || param == NULL)
		return 0;

	global_device->iommu	= param->iommu;
	global_device->recovery = param->recovery;

	for (i = 0; i < NANO2D_CORE_MAX; i++) {
		global_device->register_bases[i]	= param->register_bases[i];
		global_device->register_bases_mapped[i] = param->register_bases_mapped[i];
		global_device->register_sizes[i]	= param->register_sizes[i];
		global_device->bars[i]			= param->bars[i];
		global_device->device_index[i]		= param->device_index[i];
		global_device->irq_line[i]		= param->irq_line[i];
	}

	global_device->contiguous_size	    = param->contiguous_size;
	global_device->contiguous_base	    = param->contiguous_base;
	global_device->contiguous_requested = param->contiguous_requested;

	global_device->command_contiguous_size	    = param->command_contiguous_size;
	global_device->command_contiguous_base	    = param->command_contiguous_base;
	global_device->command_contiguous_requested = param->command_contiguous_requested;

	global_device->base_address = param->base_address;
	global_device->registerAPB  = param->registerAPB;

	return N2D_SUCCESS;
}

static void show_param(n2d_linux_module_parameters_t *param, n2d_device_t *device)
{
	int i		       = 0;
	n2d_device_id_t dev_id = 0;
	n2d_core_id_t core_id  = 0;
	n2d_uint32_t core_num  = device->core_num;

	/* Find this core's sub device id and core id. */
	// dev_id	= core_num / device->kernel->dev_core_num;
	// core_id = core_num % device->kernel->dev_core_num;

	printk("Insmod parameters:\n");
	for (i = 0; i < core_num; i++) {
		dev_id	= i / device->kernel->dev_core_num;
		core_id = i % device->kernel->dev_core_num;

		printk("device[%d] core[%d]\n", dev_id, core_id);
		printk("  irq line:%d\n", param->irq_line[i]);
		printk("  register base:0x%llx\n", (n2d_uint64_t)param->register_bases[i]);
		printk("  register size:0x%llx\n", (n2d_uint64_t)param->register_sizes[i]);
	}
	printk("\n");
	if (param->iommu)
		printk("  iommu enabled\n");
	if (param->recovery)
		printk("  recovery enabled\n");

	printk("  contiguous base:0x%llx\n", param->contiguous_base);
	printk("  contiguous size:0x%llx\n", param->contiguous_size);
	printk("  command contiguous base:0x%llx\n", param->command_contiguous_base);
	printk("  command contiguous size:0x%llx\n", param->command_contiguous_size);
	printk("\n");
}

static int setup_contiguous_memory(struct memory_heap *heap, n2d_uint64_t base, n2d_size_t size)
{
	n2d_error_t error	= N2D_SUCCESS;
	struct resource *region = NULL;
	struct heap_node *node;

	if (base == 0 || size == 0) {
		heap->free = 0;
		return N2D_SUCCESS;
	}

	region = request_mem_region(base, size, "nano2d contiguous memory");

	if (!region) {
		printk("request mem %s(0x%llx - 0x%llx) failed\n", "nano2d contiguous memory", base,
		       base + size - 1);

		ONERROR(N2D_OUT_OF_RESOURCES);
	}

	heap->physical = base;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
	heap->klogical = (__force void *)ioremap(base, size);
#else
	heap->klogical = ioremap_nocache(base, size);
#endif

	if (!heap->klogical) {
		release_mem_region(base, size);
		n2d_kernel_os_print("map contiguous memory(base:0x%lx size:0x%x) failed.\n", base,
				    size);
		ONERROR(N2D_OUT_OF_RESOURCES);
	}

	n2d_kernel_os_trace("allocated a %uB heap at physical:0x%08x, virtual:0x%llx\n", size, base,
			    (n2d_uintptr_t)heap->klogical);

	/* Create the heap. */
	INIT_LIST_HEAD(&heap->list);
	heap->size = heap->free = size;
	heap->address		= 0;
	if ((base + size) <= 0xFFFFFFFF)
		heap->limit_4G = N2D_TRUE;
	else
		heap->limit_4G = N2D_FALSE;

	node = n2d_kmalloc(sizeof(struct heap_node), GFP_KERNEL);
	if (node == NULL) {
		n2d_kernel_os_print("allocate heap node failed.\n");
		ONERROR(N2D_OUT_OF_RESOURCES);
	}

	node->offset = 0;
	node->size   = size;
	node->status = 0;
	list_add(&node->list, &heap->list);

on_error:
	return error;
}

static int release_contiguous_memory(struct memory_heap *heap)
{
	struct heap_node *pos;
	struct heap_node *n;

	if (heap->physical == 0 || heap->size == 0)
		return N2D_SUCCESS;

	if (heap->klogical != NULL) {
		release_mem_region(heap->physical, heap->size);
		iounmap((void __iomem *)heap->klogical);
	}

	/* Process each node. */
	list_for_each_entry_safe (pos, n, &heap->list, list) {
		/* Remove it from the linked list. */
		list_del(&pos->list);

		/* Free up the memory. */
		n2d_kfree(pos);
	}

	return N2D_SUCCESS;
}

static int drv_open(struct inode *inode, struct file *file)
{
	n2d_error_t error = N2D_SUCCESS;
	gpu_resume(N2D_NULL);
	ONERROR(n2d_kernel_dispatch(global_device->kernel, N2D_KERNEL_COMMAND_OPEN, 0, 0,
				    N2D_NULL));

on_error:
	return error;
}

static int drv_release(struct inode *inode, struct file *file)
{
	int allocate_count = 0;
	pm_message_t state = {0};

	n2d_kernel_dispatch(global_device->kernel, N2D_KERNEL_COMMAND_CLOSE, 0, 0, N2D_NULL);
	n2d_check_allocate_count(&allocate_count);
	if (allocate_count)
		n2d_kernel_os_print("Allocated %d memory\n", allocate_count);

	gpu_suspend(N2D_NULL, state);

	return 0;
}

static long drv_ioctl(struct file *file, unsigned int ioctl_code, unsigned long arg)
{
	int ret = 0;
	n2d_error_t error;
	n2d_ioctl_interface_t _iface, *iface;

	iface = &_iface;
	memset(iface, 0, sizeof(n2d_ioctl_interface_t));

	/*user data converted to kernel data*/
	if (copy_from_user(iface, (void __user *)arg, sizeof(n2d_ioctl_interface_t))) {
		n2d_kernel_os_print("ioctl: failed to read data.\n");
		return -ENOTTY;
	}

	if (iface->command == N2D_KERNEL_COMMAND_OPEN || iface->command == N2D_KERNEL_COMMAND_CLOSE)
		return 0;

	iface->error = n2d_kernel_dispatch(global_device->kernel, iface->command, iface->dev_id,
					   iface->core, &iface->u);
	if (iface->error == N2D_INTERRUPTED) {
		ret = -ERESTARTSYS;
		ONERROR(iface->error);
	}

	/*kernel data converted to user data*/
	if (copy_to_user((void __user *)arg, iface, sizeof(n2d_ioctl_interface_t))) {
		n2d_kernel_os_print("ioctl: failed to write data.\n");
		return -ENOTTY;
	}

	return 0;

on_error:
	return ret;
}

int drv_ioctl_internal(n2d_ioctl_interface_t *iface)
{
	int ret = 0;
	n2d_error_t error;

	/*user data converted to kernel data*/
	// if (copy_from_user(iface, (void __user *)arg, sizeof(n2d_ioctl_interface_t))) {
	// 	n2d_kernel_os_print("ioctl: failed to read data.\n");
	// 	return -ENOTTY;
	// }

	if (iface->command == N2D_KERNEL_COMMAND_OPEN || iface->command == N2D_KERNEL_COMMAND_CLOSE)
		return 0;

	iface->error = n2d_kernel_dispatch(global_device->kernel, iface->command, iface->dev_id,
					   iface->core, &iface->u);
	if (iface->error == N2D_INTERRUPTED) {
		ret = -ERESTARTSYS;
		ONERROR(iface->error);
	}

	/*kernel data converted to user data*/
	// if (copy_to_user((void __user *)arg, iface, sizeof(n2d_ioctl_interface_t))) {
	// 	n2d_kernel_os_print("ioctl: failed to write data.\n");
	// 	return -ENOTTY;
	// }

	return 0;

on_error:
	return ret;
}

static ssize_t drv_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
	return 0;
}

static int drv_mmap(struct file *file, struct vm_area_struct *vm)
{
	return 0;
}

static struct file_operations file_operations = {
	.owner		= THIS_MODULE,
	.open		= drv_open,
	.release	= drv_release,
	.read		= drv_read,
	.unlocked_ioctl = drv_ioctl,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 8, 18)
#ifdef HAVE_COMPAT_IOCTL
	.compat_ioctl = drv_ioctl,
#endif
#else
	.compat_ioctl = drv_ioctl,
#endif
	.mmap = drv_mmap,
};

int aux_open(struct vs_n2d_aux *aux)
{
	struct device *dev = aux->dev;
	n2d_error_t error = N2D_SUCCESS;

	gcmkASSERT(dev == global_device->dev);

	ONERROR(n2d_kernel_dispatch(global_device->kernel, N2D_KERNEL_COMMAND_OPEN, 0, 0,
				    N2D_NULL));
on_error:
	return error;
}
EXPORT_SYMBOL(aux_open);

int aux_close(struct vs_n2d_aux *aux)
{
	struct device *dev = aux->dev;
	int allocate_count = 0;

	gcmkASSERT(dev == global_device->dev);

	n2d_kernel_dispatch(global_device->kernel, N2D_KERNEL_COMMAND_CLOSE, 0, 0, N2D_NULL);
	n2d_check_allocate_count(&allocate_count);
	if (allocate_count)
		n2d_kernel_os_print("Allocated %d memory\n", allocate_count);

	return 0;
}
EXPORT_SYMBOL(aux_close);

// static struct vpf_device global_vpf;
static int n2d_vnode_open(struct vio_video_ctx *vctx)
{
	struct vpf_device *vpf = vctx->dev;
	struct horizon_n2d_dev *n2d     = vpf->ip_dev;
	struct n2d_gl_device *g_device = n2d->galcore;
	n2d_error_t error = N2D_SUCCESS;

	gcmkASSERT(g_device == global_device);
	gpu_resume(N2D_NULL);

	ONERROR(n2d_kernel_dispatch(global_device->kernel, N2D_KERNEL_COMMAND_OPEN, 0, 0,
				    N2D_NULL));
on_error:
	return error;
}

static int n2d_vnode_close(struct vio_video_ctx *vctx)
{
	struct vpf_device *vpf = vctx->dev;
	struct horizon_n2d_dev *n2d     = vpf->ip_dev;
	struct n2d_gl_device *g_device = n2d->galcore;
	int allocate_count = 0;
	pm_message_t state = {0};

	gcmkASSERT(g_device == global_device);

	n2d_kernel_dispatch(global_device->kernel, N2D_KERNEL_COMMAND_CLOSE, 0, 0, N2D_NULL);
	n2d_check_allocate_count(&allocate_count);
	if (allocate_count)
		n2d_kernel_os_print("Allocated %d memory\n", allocate_count);

	gpu_suspend(N2D_NULL, state);

	return 0;
}

/* set buffer attr */
static int n2d_vnode_set_attr(struct vio_video_ctx *vctx, unsigned long arg)
{
	s32 ret = 0;
	u64 copy_ret;
	struct n2d_subdev *n2d_sub;
	struct vio_subdev *vdev;

	vdev = vctx->vdev;
	n2d_sub = container_of(vdev, struct n2d_subdev, vdev);
	copy_ret = copy_from_user((void *)&n2d_sub->config, (void __user *)arg, sizeof(struct n2d_config));
	if (copy_ret != 0u) {
		vio_err("%s: failed to copy from user, ret = %lld\n", __func__, copy_ret);
		return -EFAULT;
	}

	if (vdev->id == VNODE_ID_SRC)
		vdev->leader = 1;

	osal_set_bit((s32)VIO_SUBDEV_DMA_OUTPUT, &vdev->state);

	return ret;
}

static int n2d_vnode_get_attr(struct vio_video_ctx *vctx, unsigned long arg)
{
	s32 ret = 0;
	u64 copy_ret;
	struct n2d_subdev *subdev;
	struct vio_subdev *vdev;

	vdev = vctx->vdev;
	subdev = container_of(vdev, struct n2d_subdev, vdev);/*PRQA S 2810,0497*/

	copy_ret = osal_copy_to_app((void __user *) arg, (void *)&subdev->config, sizeof(struct n2d_config));
	if (copy_ret != 0u) {
		vio_err("%s: failed to copy from user, ret = %lld\n", __func__, copy_ret);
		return -EFAULT;
	}
	vio_info("%s done", __func__);

	return ret;
}

static struct vio_version_info n2d_version = {
	.major = 1,
	.minor = 0
};

static s32 n2d_get_version(struct vio_version_info *version)
{
	s32 ret = 0;

	(void)memcpy(version, &n2d_version, sizeof(struct vio_version_info));

	return ret;
}

static int n2d_vnode_get_buf_attr(struct vio_video_ctx *vctx, struct vbuf_group_info *group_attr)
{
	int ret = 0;
	struct vio_subdev *vdev;
	struct n2d_subdev *subdev;
	struct n2d_config *config;
	int ch = 0;

	vdev = vctx->vdev;
	subdev = container_of(vdev, struct n2d_subdev, vdev);
	config = &subdev->config;
	ch     = vdev->id; /* assume vdev->id is channel number */

	pr_info("%s: ch = %d.\n", __func__, ch);

	if (vctx->id < VNODE_ID_CAP) {
		group_attr->bit_map = 1;
		group_attr->info[0].buf_attr.format = MEM_PIX_FMT_NV12;
		group_attr->info[0].buf_attr.planecount = 2;
		group_attr->info[0].buf_attr.width	   = config->input_width[ch];
		group_attr->info[0].buf_attr.height	   = config->input_height[ch];
		group_attr->info[0].buf_attr.wstride	   = config->input_stride[ch];
		group_attr->info[0].buf_attr.vstride	   = config->input_height[ch];
	} else {
		group_attr->bit_map = 1;
		group_attr->info[0].buf_attr.format = config->output_format;
		group_attr->info[0].buf_attr.planecount = 2;
		group_attr->info[0].buf_attr.width = config->output_width;
		group_attr->info[0].buf_attr.height	   = config->output_height;
		group_attr->info[0].buf_attr.wstride	   = config->output_stride;
		group_attr->info[0].buf_attr.vstride	   = config->output_height;
		if (config->output_format == MEM_PIX_FMT_ARGB) {
			group_attr->info[0].buf_attr.planecount = 1;
			/* temp set wstride to width make it compatible with libhbmem.so's size calc */
			// group_attr->info[0].buf_attr.wstride	= config->output_width * 4;
		}

		// group_attr->is_contig  = 1; /* now use hbn_vnode_set_ochn_buf_attr, is_contig got from that param */
	}

	vio_info("[C%d][V%d] %s: wstride %d  vstride %d format = %d\n", vctx->ctx_id, vctx->id, __func__,
		group_attr->info[0].buf_attr.wstride, group_attr->info[0].buf_attr.vstride, group_attr->info[0].buf_attr.format);
	return ret;
}

static int n2d_allow_bind(struct vio_subdev *vdev, struct vio_subdev *remote_vdev, u8 online_mode)
{
	enum vio_bind_type bind_type;

	bind_type = CHN_BIND_M2M;
	if (osal_test_bit((s32)VIO_SUBDEV_BIND_DONE, &vdev->state) != 0)
		bind_type = CHN_BIND_REP;

	return bind_type;
}

static int do_csc(struct vio_node *vnode, struct n2d_config *config)
{
	n2d_buffer_t src = {0}, dst = {0};
	n2d_uintptr_t handle = 0;
	n2d_user_memory_desc_t memDesc = {0};
	n2d_error_t error	       = N2D_SUCCESS;
	n2d_state_config_t csc_com = {0};

	/* check size based on format/width, height */
	memDesc.flag = N2D_WRAP_FROM_USERMEMORY;
	memDesc.physical = config->in_buffer_addr[0][0]; /* assume the buffer is contiguous */
	memDesc.size	 = config->input_stride[0] * config->input_height[0]; /* assume buffer is aligned */
	N2D_ON_ERROR(n2d_wrap(&memDesc, &handle));

	src.width = config->input_width[0];
	src.height = config->input_height[0];
	src.stride = config->input_stride[0]; /*???*/
	src.format = N2D_NV12;
	src.handle = handle;
	src.alignedh = config->input_height[0];
	src.alignedw = config->input_stride[0];
	src.tiling   = N2D_LINEAR;
	N2D_ON_ERROR(n2d_map(&src));

	memDesc.flag = N2D_WRAP_FROM_USERMEMORY;
	// memDesc.logical = N2D_NULL;
	memDesc.physical = config->out_buffer_addr[0];
	memDesc.size	 = config->output_stride * config->output_height * 4;
	N2D_ON_ERROR(n2d_wrap(&memDesc, &handle));

	dst.width = config->output_width;
	dst.height = config->output_height;
	dst.stride = config->output_stride * 4; /*???*/
	dst.format = N2D_ARGB8888;
	dst.handle = handle;
	dst.alignedh = config->output_height;
	dst.alignedw = config->output_width;
	dst.tiling   = N2D_LINEAR;
	N2D_ON_ERROR(n2d_map(&dst));

	csc_com.state = N2D_SET_CSC;
	csc_com.config.csc.cscMode = N2D_CSC_BT709;
	N2D_ON_ERROR(n2d_set(&csc_com));
	N2D_ON_ERROR(n2d_blit(&dst, N2D_NULL, &src, N2D_NULL, N2D_BLEND_NONE));
	N2D_ON_ERROR(n2d_commit());

	vio_frame_done(vnode->ich_subdev[0]);
	vio_frame_done(vnode->och_subdev[0]);

on_error:
	n2d_free(&src);
	n2d_free(&dst);
	return error;
}

static int do_stitch(struct vio_node *vnode, struct n2d_config *config)
{
	n2d_buffer_t src[4] = {0}, dst = {0};
	n2d_uintptr_t handle = 0;
	n2d_user_memory_desc_t memDesc = {0};
	n2d_error_t error	       = N2D_SUCCESS;
	// n2d_rectangle_t rectSrc, rectDst;
	n2d_rectangle_t rectDst;
	n2d_int32_t vOffset = 0, hOffset = 0;
	// n2d_state_config_t clipRect = {N2D_SET_CLIP_RECTANGLE, {0}};
    // n2d_state_config_t srcIndex = {N2D_SET_MULTISOURCE_INDEX, {0}};
    // n2d_state_config_t blend = {N2D_SET_ALPHABLEND_MODE, {0}};
    // n2d_state_config_t multisrcAndDstRect = {N2D_SET_MULTISRC_DST_RECTANGLE, {0}};
    // n2d_state_config_t rop = {N2D_SET_ROP, {0}};
    // n2d_state_config_t globalAlpha = {N2D_SET_GLOBAL_ALPHA, {0}};
    n2d_int32_t i = 0, srcNum = N2D_IN_MAX;

	// blend.config.alphablendMode = N2D_BLEND_NONE;
	// globalAlpha.config.globalAlpha.dstMode = N2D_GLOBAL_ALPHA_ON;
	// globalAlpha.config.globalAlpha.srcMode = N2D_GLOBAL_ALPHA_ON;
	// globalAlpha.config.globalAlpha.srcValue = 0xAA;
	// globalAlpha.config.globalAlpha.dstValue = 0xAA;
	// rop.config.rop.bg_rop = rop.config.rop.fg_rop = 0xCC;

	for (i = 0; i < srcNum; i++) {
		memDesc.flag = N2D_WRAP_FROM_USERMEMORY;
		memDesc.physical = config->in_buffer_addr[i][0]; /* assume the buffer is contiguous */
		memDesc.size	 = config->input_stride[i] * config->input_height[i]; /* assume buffer is aligned */
		N2D_ON_ERROR(n2d_wrap(&memDesc, &handle));

		src[i].width = config->input_width[i];
		src[i].height = config->input_height[i];
		src[i].stride = config->input_stride[i]; /*???*/
		src[i].format = N2D_NV12;
		src[i].handle = handle;
		src[i].alignedh = config->input_height[i];
		src[i].alignedw = config->input_width[i];
		src[i].tiling	= N2D_LINEAR;
		src[i].orientation = N2D_0;
		src[i].tile_status_config = N2D_TSC_DISABLE;
		N2D_ON_ERROR(n2d_map(&src[i]));
	}

	memDesc.flag = N2D_WRAP_FROM_USERMEMORY;
	// memDesc.logical = N2D_NULL;
	memDesc.physical = config->out_buffer_addr[0];
	memDesc.size	 = config->output_stride * config->output_height;
	N2D_ON_ERROR(n2d_wrap(&memDesc, &handle));

	dst.width = config->output_width;
	dst.height = config->output_height;
	dst.stride = config->output_stride; /*???*/
	dst.format = N2D_NV12;
	dst.handle = handle;
	dst.alignedh = config->output_height;
	dst.alignedw = config->output_width;
	dst.tiling   = N2D_LINEAR;
	dst.orientation = N2D_0;
	dst.tile_status_config = N2D_TSC_DISABLE;
	N2D_ON_ERROR(n2d_map(&dst));

	// clipRect.config.clipRect.x = clipRect.config.clipRect.y = 0;
    // clipRect.config.clipRect.width = dst.width;
    // clipRect.config.clipRect.height = dst.height;

    hOffset = dst.width / 2;
    vOffset = dst.height / 2;
    for (i = 0; i < srcNum; i++) {
	    // srcIndex.config.multisourceIndex = i;
	    // N2D_ON_ERROR(n2d_set(&srcIndex));

	    switch (i % N2D_IN_MAX) {
	    case 0:
		    rectDst.x			= 0;
		    rectDst.y			= 0;
		    rectDst.width		= hOffset;
		    rectDst.height		= vOffset;
		    // blend.config.alphablendMode = N2D_BLEND_NONE;
		    break;

	    case 1:
		    rectDst.x			= hOffset;
		    rectDst.y			= 0;
		    rectDst.width		= hOffset;
		    rectDst.height		= vOffset;
		    // blend.config.alphablendMode = N2D_BLEND_NONE;
		    break;

	    case 2:
		    rectDst.x			= 0;
		    rectDst.y			= vOffset;
		    rectDst.width		= hOffset;
		    rectDst.height		= vOffset;
		    // blend.config.alphablendMode = N2D_BLEND_NONE;
		    break;

	    case 3:
		    rectDst.x			= hOffset;
		    rectDst.y			= vOffset;
		    rectDst.width		= hOffset;
		    rectDst.height		= vOffset;
		    // blend.config.alphablendMode = N2D_BLEND_NONE;
		    break;
	    }

	    // rectSrc.x = rectSrc.y = 0;
	    // rectSrc.width	  = src[i].width;
	    // rectSrc.height	  = src[i].height;

	    // multisrcAndDstRect.config.multisrcAndDstRect.source = &src[i];
	    // memcpy(&multisrcAndDstRect.config.multisrcAndDstRect.srcRect, &rectSrc,
		//    N2D_SIZEOF(n2d_rectangle_t));
	    // memcpy(&multisrcAndDstRect.config.multisrcAndDstRect.dstRect, &rectDst,
		//    N2D_SIZEOF(n2d_rectangle_t));

	    // N2D_ON_ERROR(n2d_set(&globalAlpha));
	    // N2D_ON_ERROR(n2d_set(&clipRect));
	    // N2D_ON_ERROR(n2d_set(&blend));
	    // N2D_ON_ERROR(n2d_set(&multisrcAndDstRect));
	    // N2D_ON_ERROR(n2d_set(&rop));

		N2D_ON_ERROR(n2d_blit(&dst, &rectDst, &src[i], N2D_NULL, N2D_BLEND_NONE));
    }

	// N2D_ON_ERROR(n2d_multisource_blit(&dst,0x0F));

	N2D_ON_ERROR(n2d_commit());

on_error:
	for (i = 0; i < srcNum; i++) {
		n2d_free(&src[i]);
		vio_frame_done(vnode->ich_subdev[i]);
	}

	n2d_free(&dst);
	vio_frame_done(vnode->och_subdev[0]);

	return error;
}

static int do_overlay(struct vio_node *vnode, struct n2d_config *config)
{
	n2d_buffer_t src1 = {0} ,src = {0}, dst = {0};
	n2d_uintptr_t handle = 0;
	n2d_user_memory_desc_t memDesc = {0};
	n2d_error_t error	       = N2D_SUCCESS;
	n2d_rectangle_t rect;

	/* check size based on format/width, height */
	memDesc.flag = N2D_WRAP_FROM_USERMEMORY;
	memDesc.physical = config->in_buffer_addr[0][0]; /* assume the buffer is contiguous */
	memDesc.size	 = config->input_stride[0] * config->input_height[0]; /* assume buffer is aligned */
	N2D_ON_ERROR(n2d_wrap(&memDesc, &handle));

	src.width = config->input_width[0];
	src.height = config->input_height[0];
	src.stride = config->input_stride[0]; /*???*/
	src.format = N2D_NV12;
	src.handle = handle;
	src.alignedh = config->input_height[0];
	src.alignedw = config->input_stride[0];
	src.tiling   = N2D_LINEAR;
	N2D_ON_ERROR(n2d_map(&src));

	memDesc.flag = N2D_WRAP_FROM_USERMEMORY;
	memDesc.physical = config->in_buffer_addr[1][0]; /* assume the buffer is contiguous */
	memDesc.size	 = config->input_stride[1] * config->input_height[1]; /* assume buffer is aligned */
	N2D_ON_ERROR(n2d_wrap(&memDesc, &handle));

	src1.width = config->input_width[1];
	src1.height = config->input_height[1];
	src1.stride = config->input_stride[1]; /*???*/
	src1.format = N2D_NV12;
	src1.handle = handle;
	src1.alignedh = config->input_height[1];
	src1.alignedw = config->input_stride[1];
	src1.tiling   = N2D_LINEAR;
	N2D_ON_ERROR(n2d_map(&src1));

	memDesc.flag = N2D_WRAP_FROM_USERMEMORY;
	// memDesc.logical = N2D_NULL;
	memDesc.physical = config->out_buffer_addr[0];
	memDesc.size	 = config->output_stride * config->output_height;
	N2D_ON_ERROR(n2d_wrap(&memDesc, &handle));

	dst.width = config->output_width;
	dst.height = config->output_height;
	dst.stride = config->output_stride; /*???*/
	dst.format = N2D_NV12;
	dst.handle = handle;
	dst.alignedh = config->output_height;
	dst.alignedw = config->output_stride;
	dst.tiling   = N2D_LINEAR;
	N2D_ON_ERROR(n2d_map(&dst));

	rect.x = config->overlay_x; /* add dst rect */
	rect.y = config->overlay_y;
	rect.width = src1.width;
	rect.height = src1.height;

	N2D_ON_ERROR(n2d_blit(&dst, N2D_NULL, &src, N2D_NULL, N2D_BLEND_NONE));
	N2D_ON_ERROR(n2d_blit(&dst, &rect, &src1, N2D_NULL, N2D_BLEND_NONE));
	N2D_ON_ERROR(n2d_commit());

    vio_frame_done(vnode->ich_subdev[0]);
	vio_frame_done(vnode->ich_subdev[1]);
	vio_frame_done(vnode->och_subdev[0]);

on_error:
	n2d_free(&src);
	n2d_free(&src1);
	n2d_free(&dst);

	return error;
}

static int do_scale(struct vio_node *vnode, struct n2d_config *config)
{
	n2d_buffer_t src = {0}, dst = {0};
	n2d_uintptr_t handle = 0;
	n2d_user_memory_desc_t memDesc = {0};
	n2d_error_t error	       = N2D_SUCCESS;

	/* check size based on format/width, height */
	memDesc.flag = N2D_WRAP_FROM_USERMEMORY;
	memDesc.physical = config->in_buffer_addr[0][0]; /* assume the buffer is contiguous */
	memDesc.size	 = config->input_stride[0] * config->input_height[0] * 3 / 2; /* assume buffer is aligned */
	vio_info("%s(%d): physical = %llx, size = %llx.\n", __func__, __LINE__, memDesc.physical, memDesc.size);
	N2D_ON_ERROR(n2d_wrap(&memDesc, &handle));

	src.width = config->input_width[0];
	src.height = config->input_height[0];
	src.stride = config->input_stride[0];
	src.format = N2D_NV12;
	src.handle = handle;
	src.alignedh = config->input_height[0];
	src.alignedw = config->input_stride[0];
	vio_info("%s(%d): width = %d, height = %d, format = %d, handle = %lld, stride = %d.\n",
		 __func__, __LINE__, src.width, src.height, src.format, src.handle, src.stride);
	N2D_ON_ERROR(n2d_map(&src));
	src.tiling	= N2D_LINEAR;

	memDesc.flag = N2D_WRAP_FROM_USERMEMORY;
	// memDesc.logical = N2D_NULL;
	memDesc.physical = config->out_buffer_addr[0];
	memDesc.size	 = config->output_stride * config->output_height * 3 / 2;
	vio_info("%s(%d): physical = %llx, size = %llx.\n", __func__, __LINE__, memDesc.physical, memDesc.size);
	N2D_ON_ERROR(n2d_wrap(&memDesc, &handle));

	dst.width = config->output_width;
	dst.height = config->output_height;
	dst.stride = config->output_stride; /*???*/
	dst.format = N2D_NV12;
	dst.handle = handle;
	dst.alignedh = config->output_height;
	dst.alignedw = config->output_stride;
	vio_info("%s(%d): width = %d, height = %d, format = %d, handle = %lld, stride = %d.\n",
		 __func__, __LINE__, dst.width, dst.height, dst.format, dst.handle, dst.stride);
	N2D_ON_ERROR(n2d_map(&dst));
	dst.tiling = N2D_LINEAR;

	N2D_ON_ERROR(n2d_blit(&dst, N2D_NULL, &src, N2D_NULL, N2D_BLEND_NONE));
	N2D_ON_ERROR(n2d_commit());

	vio_frame_done(vnode->ich_subdev[0]);
	vio_frame_done(vnode->och_subdev[0]);

on_error:
	n2d_free(&src);
	n2d_free(&dst);
	return error;
}

static int n2d_process(struct vio_node *vnode, struct n2d_config *config)
{
	n2d_error_t error	       = N2D_SUCCESS;

	error = n2d_open();
	if (N2D_IS_ERROR(error)) {
		vio_err("open context failed! error=%d\n", error);
		goto on_error;
	}

	switch (config->command)
	{
	case scale:
		N2D_ON_ERROR(do_scale(vnode, config));
		break;
	case overlay:
		N2D_ON_ERROR(do_overlay(vnode, config));
		break;
	case stitch:
		N2D_ON_ERROR(do_stitch(vnode, config));
		break;
	case csc:
		N2D_ON_ERROR(do_csc(vnode, config));
		break;
	default:
		vio_err("Not supported command: %d\n", config->command);
		break;
	}

on_error:
	error = n2d_close();
	if (N2D_IS_ERROR(error)) {
		vio_err("close context failed! error=%d\n", error);
	}

	return error;
}

// static int n2d_push_work(struct n2d_subnode *n2d_sub)
// {
// 	return 0;
// }

static void n2d_judge_triger_worker(struct vio_subdev *vdev, struct vio_frame *frame)
{
	int ret = 0;
	long long timestamp_gap = 0;
	struct vio_node *vnode = vdev->vnode;
	struct n2d_subdev *subdev = container_of(vdev, struct n2d_subdev, vdev);
	struct n2d_subnode *n2d_sub;

	vio_info("%s: vdev->id = %d.\n", __func__, vdev->id);
	n2d_sub = container_of(subdev, struct n2d_subnode, subdev[vdev->id]);

	vio_info("%s: n2d_sub->frame_state = %ld\n", __func__, n2d_sub->frame_state);
	if (n2d_sub->frame_state == 0UL) {
		n2d_sub->cur_timestamp = frame->frameinfo.frameid.timestamps;
		set_bit((s32)vdev->id, &n2d_sub->frame_state);
		n2d_sub->synced_src_frames[vdev->id] = frame;
		vio_info("%s: curtimestamp from %d.\n", __func__, frame->frameinfo.frameid.frame_id);
	} else {
		timestamp_gap = n2d_sub->cur_timestamp - frame->frameinfo.frameid.timestamps;
		if (timestamp_gap > TS_MAX_GAP) {
			vio_warn("timestamp not increase, drop frame(%d), timestamp gap(%lld)\n", frame->frameinfo.frameid.frame_id, timestamp_gap);
			vio_frame_ndone(vdev);
		} else if (timestamp_gap < -TS_MAX_GAP) {
			vio_warn("The next frame(%d) has arrived! force trigger process.\n", frame->frameinfo.frameid.frame_id);
			// ret = n2d_push_work(n2d_sub);
			n2d_sub->frame_state = 0; /* clear frame state */
			if (ret == 0) {
				vio_group_start_trigger(vnode, frame); /* call frame_work ?? */
			}
		} else {
			if (test_bit(vdev->id, &n2d_sub->frame_state) == 0) {
				set_bit(vdev->id, &n2d_sub->frame_state);
				n2d_sub->synced_src_frames[vdev->id] = frame;
				vio_info("%s: got frame from %d.\n", __func__, vdev->id);
			} else {
				vio_warn("timestamp repeat, drop frame(%d)\n",
					 frame->frameinfo.frameid.frame_id);
				vio_frame_ndone(vdev);
			}
		}
	}

	vio_info("%s: frame_state = %ld, ninputs = %d.\n", __func__, n2d_sub->frame_state, subdev->config.ninputs);
	if (((n2d_sub->frame_state == 0xF) && (subdev->config.ninputs == 4)) || (
		(n2d_sub->frame_state == 0x3) && (subdev->config.ninputs == 2)) || (
		(n2d_sub->frame_state == 0x1) && (subdev->config.ninputs == 1))) {
		// ret = n2d_push_work(n2d_sub);
		n2d_sub->frame_state = 0;
		if (ret == 0) {
			vio_info("%s: trigger frame work.\n", __func__);
			vio_group_start_trigger(vnode, frame);
		}
	}
}

/* todo: need to djuge input number when do overlay & stitch */
static void n2d_frame_work(struct vio_node *vnode)
{
	int ret = 0;
	u64 flags = 0;
	struct n2d_subdev *n2d_sub;
	struct vio_subdev *vdev, *och_vdev;
	struct vio_framemgr *framemgr, *out_framemgr;
	struct vio_frame *frame, *out_frame;
	struct n2d_config *config;
	int i, j;

	vdev = vnode->ich_subdev[0];
	och_vdev = vnode->och_subdev[0];
	n2d_sub	 = container_of(vdev, struct n2d_subdev, vdev);
	framemgr = &vdev->framemgr;
	out_framemgr = &och_vdev->framemgr;
	config	     = &n2d_sub->config;
	vio_dbg("[S%d][C%d] %s start, rcount %d\n", vnode->flow_id, vnode->ctx_id, __func__, atomic_read(&vnode->rcount));/*PRQA S 0685,1294*/

	vio_info("%s: config->command = %d.\n", __func__, config->command);

	for (i = 0; i < n2d_sub->config.ninputs; i++) {
		vdev = vnode->ich_subdev[i];
		framemgr = &vdev->framemgr;
		vio_e_barrier_irqs(framemgr, flags);
		frame = peek_frame(framemgr, FS_REQUEST);
		vio_x_barrier_irqr(framemgr, flags);
		if (frame != NULL) {
			config->in_buffer_addr[i][0] = frame->vbuf.iommu_paddr[0][0];
			config->in_buffer_addr[i][1] = frame->vbuf.iommu_paddr[0][1];
			if (config->in_buffer_addr[i][0] == 0 || config->in_buffer_addr[i][1] == 0) {
				vio_frame_ndone(vdev);
				return;
			}

			vio_e_barrier_irqs(framemgr, flags);
			trans_frame(framemgr, frame, FS_PROCESS);
			vio_x_barrier_irqr(framemgr, flags);
			vio_fps_calculate(&vdev->fdebug, &vnode->frameid);
			memcpy(&vnode->frameid, &frame->frameinfo.frameid, sizeof(struct frame_id_desc));

			vio_info("[S%d][C%d] in  0x%llx, 0x%llx.\n", vnode->flow_id,
			vnode->ctx_id, config->in_buffer_addr[i][0], config->in_buffer_addr[i][1]);
		} else {
			framemgr_print_queues(framemgr);
			vio_err("[S%d][C%d] %s no src frame\n", vnode->flow_id, vnode->ctx_id, __func__);
			goto err;
		}
	}

	vio_e_barrier_irqs(out_framemgr, flags);
	out_frame = peek_frame(out_framemgr, FS_REQUEST);
	vio_x_barrier_irqr(out_framemgr, flags);
	if (out_frame != NULL) {
		config->out_buffer_addr[0] = out_frame->vbuf.iommu_paddr[0][0];
		config->out_buffer_addr[1] = out_frame->vbuf.iommu_paddr[0][1];
		vio_e_barrier_irqs(out_framemgr, flags);
		trans_frame(out_framemgr, out_frame, FS_PROCESS);
		vio_x_barrier_irqr(out_framemgr, flags);

		vio_info("[S%d][C%d] out 0x%llx, 0x%llx\n", vnode->flow_id, vnode->ctx_id,
			config->out_buffer_addr[0], config->out_buffer_addr[1]);

		vio_set_stat_info(vnode->flow_id, N2D_MODULE, STAT_FS, 0);
		ret = n2d_process(vnode, config);
		vio_set_stat_info(vnode->flow_id, N2D_MODULE, STAT_FE, 0);

		if (ret < 0) {
			vio_err("[S%d][C%d] %s failed to process frame\n", vnode->flow_id,
				vnode->ctx_id, __func__);
			for (j = 0; j < i; j++) {
				vio_frame_ndone(vnode->ich_subdev[j]);
			}
			vio_frame_ndone(och_vdev);
		}
	} else {
		vio_warn("[S%d][C%d] %s no output frame\n", vnode->flow_id, vnode->ctx_id, __func__);
		framemgr_print_queues(out_framemgr);
		vio_frame_done(vdev);//src frame invalid run;
	}

	vio_set_hw_free(vnode);

	vio_dbg("[S%d][C%d] %s done\n", vnode->flow_id, vnode->ctx_id, __func__);/*PRQA S 0685,1294*/
	return;

err:
	for (j = 0; j < i; j++) {
		vio_frame_ndone(vnode->ich_subdev[j]);
	}
	vio_set_hw_free(vnode);
}

static struct vio_common_ops n2d_vnode_vops = {
	.open		    = n2d_vnode_open,
	.close		    = n2d_vnode_close,
	.video_set_attr	    = n2d_vnode_set_attr,
	.video_get_attr = n2d_vnode_get_attr,
	.video_get_buf_attr = n2d_vnode_get_buf_attr,
	.video_get_version = n2d_get_version,
};

static int horizon_n2d_device_node_init(struct horizon_n2d_dev *n2d)
{
	int ret = 0;
	int i = 0, j = 0;
	char name[32];
	struct vio_node *vnode = n2d->vnode;

	n2d->galcore = global_device;

	n2d->gtask.no_worker  = 1;
	n2d->gtask.id	      = N2D_MODULE;

	for (i = 0; i < VIO_MAX_STREAM; i++) {
		vnode[i].flow_id = INVALID_FLOW_ID;
		vnode[i].ctx_id = i;
		for (j = 0; j < N2D_IN_MAX; j++) {
			vnode[i].ich_subdev[j] = &n2d->n2d_sub[i].subdev[j].vdev;
			vnode[i].ich_subdev[j]->vdev_work = n2d_judge_triger_worker;
			vnode[i].ich_subdev[j]->vnode = &vnode[i];
			vnode[i].active_ich	      |= 1 << j;
		}

		vnode[i].och_subdev[0] = &n2d->n2d_sub[i].subdev[N2D_IN_MAX].vdev;
		vnode[i].active_och    = 1;
		vnode[i].id = N2D_MODULE;
		vnode[i].och_subdev[0]->vnode = &vnode[i];

		vnode[i].gtask = &n2d->gtask;
		vnode[i].allow_bind = n2d_allow_bind;
		vnode[i].frame_work = n2d_frame_work;
	}

	/* Register vpf device. */
	for (i = 0; i < N2D_CH_MAX; i++) {
		n2d->vps[i].vps_ops = &n2d_vnode_vops;
		n2d->vps[i].ip_dev = n2d;
		n2d->vps[i].vnode     = vnode;
		n2d->vps[i].max_ctx   = VIO_MAX_STREAM;
		n2d->vps[i].iommu_dev = (struct device *)(n2d->galcore->dev);
		n2d->vps[i].owner     = THIS_MODULE;

		if (i < N2D_IN_MAX) {
			n2d->vps[i].vid = VNODE_ID_SRC + i; /* ?? ctx->id ?? vdev->id ?? ch */
			snprintf(name, sizeof(name), "n2d_ichn%d", i);
		} else {
			n2d->vps[i].vid = VNODE_ID_CAP;
			snprintf(name, sizeof(name), "n2d_ochn0");
		}

		ret = vio_register_device_node(name, &n2d->vps[i]);
		if (ret < 0)
			goto err;
	}


	return ret;

err:
	for (j = 0; j < i; j++)
		vio_unregister_device_node(&n2d->vps[j]);

	return ret;
}

static int drv_init(void)
{
	n2d_error_t error = N2D_SUCCESS;
	int i;
	unsigned long physical;
	unsigned int size;

	for (i = 0; i < NANO2D_CORE_MAX; i++) {
		if (global_device->irq_line[i] == -1)
			continue;

		physical = global_device->register_bases[i];
		size	 = global_device->register_sizes[i];

		if (physical != 0) {
			if (!global_device->register_bases_mapped[i]) {
				if (!request_mem_region(physical, size, "nano2d register region")) {
					n2d_kernel_os_print("claim gpu:%d registers failed.\n", i);
					return -1;
				}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
				/* Map the GPU registers. */
				global_device->register_bases_mapped[i] =
					(__force n2d_pointer)ioremap(physical, size);
#else
				global_device->register_bases_mapped[i] =
					(n2d_pointer)ioremap_nocache(physical, size);
#endif

				if (!global_device->register_bases_mapped[i]) {
					n2d_kernel_os_print("map the gpu:%d registers:0x%llx "
							    "size:0x%x failed.\n",
							    i, physical, (n2d_uint32_t)size);
					return -1;
				}
				global_device->reg_mem_requested[i] = 0;
			} else {
				global_device->reg_mem_requested[i] =
					(n2d_uint64_t)global_device->register_bases_mapped[i];
			}
		}
		global_device->core_num++;
	}

	if (global_device->contiguous_size > 0)
		ONERROR(setup_contiguous_memory(&global_device->heap,
						global_device->contiguous_base,
						global_device->contiguous_size));
	if (global_device->command_contiguous_size > 0)
		ONERROR(setup_contiguous_memory(&global_device->command_heap,
						global_device->command_contiguous_base,
						global_device->command_contiguous_size));

	ONERROR(n2d_kernel_os_construct(global_device, &global_device->os));
	ONERROR(n2d_kernel_construct(global_device, &global_device->kernel));

	ONERROR(start_device(global_device));

	major = register_chrdev(0, N2D_DEVICE_NAME, &file_operations);
	if (major < 0) {
		n2d_kernel_os_print("register_chrdev failed.\n");
		return -1;
	}

	/* Create the graphics class. */
	device_class = class_create(THIS_MODULE, "nano2d_class");
	if (device_class == NULL) {
		n2d_kernel_os_print("class_create failed.\n");
		return -1;
	}

	/* Create the deviceex. */
	if (device_create(device_class, NULL, MKDEV(major, 0), NULL, N2D_DEVICE_NAME) == NULL) {
		n2d_kernel_os_print("device_create failed.\n");
		return -1;
	}

	n2d_kernel_os_print("create /dev/%s device.\n", N2D_DEVICE_NAME);

	return 0;
on_error:
	n2d_kernel_os_trace("create device failed\n");
	return -1;
}

static void drv_exit(void)
{
	int i = 0;

	destroy_debug_fs(global_device);
	stop_device(global_device);
	n2d_kernel_destroy(global_device->kernel);
	n2d_kernel_os_destroy(global_device->os);
	if (global_device->contiguous_size > 0)
		release_contiguous_memory(&global_device->heap);
	if (global_device->command_contiguous_size > 0)
		release_contiguous_memory(&global_device->command_heap);

	for (i = 0; i < NANO2D_CORE_MAX; i++) {
		if (global_device->irq_line[i] == -1)
			continue;
		if (global_device->reg_mem_requested[i] == 0) {
			if (global_device->register_bases_mapped[i] != NULL) {
				/* Unmap the GPU registers. */
				iounmap((void __iomem *)global_device->register_bases_mapped[i]);
				release_mem_region((n2d_uint64_t)global_device->register_bases[i],
						   global_device->register_sizes[i]);
			}
		}
	}

	if (device_class != NULL) {
		device_destroy(device_class, MKDEV(major, 0));
		/* Destroy the class. */
		class_destroy(device_class);
		unregister_chrdev(major, N2D_DEVICE_NAME);
	}

	n2d_kernel_os_print("nano2D exit.\n");
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
static int gpu_probe(struct platform_device *pdev)
#else
static int __devinit gpu_probe(struct platform_device *pdev)
#endif
{
	n2d_error_t error;
	n2d_uint32_t ret;
	static u64 dma_mask  = 0;
	struct horizon_n2d_dev *n2d;
	struct device *dev_p = &(pdev->dev);
	struct vs_n2d_aux *aux;
	pm_message_t state = {0};

	platform->device = pdev;

	global_device = n2d_kmalloc(sizeof(n2d_device_t), GFP_KERNEL);
	if (NULL == global_device) {
		n2d_kernel_os_print("allocate device structure failed.\n");
		return -1;
	}
	memset(global_device, 0, sizeof(n2d_device_t));

#if NANO2D_MMU_ENABLE
	dma_mask = DMA_BIT_MASK(40);
#else
	dma_mask = DMA_BIT_MASK(32);
#endif
	/* Power and clock. */
	if (platform->ops->getPower) {
		ONERROR(platform->ops->getPower(platform));

		ONERROR(platform->ops->set_power(platform, 1, N2D_TRUE));

		ONERROR(platform->ops->setClock(platform, 1, N2D_TRUE));

		ONERROR(platform->ops->reset(platform, N2D_FALSE)); /* reset deassert */
	}

	init_param(&global_param);

	sync_input_param(&global_param);

	/* Override default module param. */
	if (platform->ops->adjust_param) {
		/* Power and clock. */
		platform->ops->adjust_param(platform, &global_param);
	}

	ONERROR(sync_param(&global_param));

	if (global_param.iommu)
		dma_mask = DMA_BIT_MASK(32);

	dev_p->dma_mask		 = &dma_mask;
	dev_p->coherent_dma_mask = dma_mask;

	global_device->dev	= (n2d_pointer)dev_p;
	global_device->platform = platform;

	ret = drv_init();
	if (ret != 0)
		return ret;

	gpu_suspend(pdev, state);
	ONERROR(create_debug_fs(global_device));

	show_param(&global_param, global_device);

	n2d = n2d_kmalloc(sizeof(struct horizon_n2d_dev), GFP_KERNEL);
	if (NULL == n2d) {
		n2d_kernel_os_print("allocate n2d_dev structure failed.\n");
		return -1;
	}
	memset(n2d, 0, sizeof(struct horizon_n2d_dev));

	ret = horizon_n2d_device_node_init(n2d);
	if (ret != 0)
		return ret;

	global_n2d = n2d;

	aux = devm_kzalloc(dev_p, sizeof(*aux), GFP_KERNEL);
	if (!aux)
		goto on_error;

	dev_set_drvdata(dev_p, aux);

	aux->dev = dev_p;

	return 0;

on_error:
	if (global_device)
		n2d_kfree(global_device);

	return 1;
}

static void horizon_n2d_device_node_deinit(struct horizon_n2d_dev *n2d)
{
	int i = 0;

	for (i = 0; i < N2D_CH_MAX; i++) {
		vio_unregister_device_node(&n2d->vps[i]);
	}

	n2d_kfree(n2d);
	n2d = N2D_NULL;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
static int gpu_remove(struct platform_device *pdev)
#else
static int __devexit gpu_remove(struct platform_device *pdev)
#endif
{
	int allocate_count = 0;
	struct horizon_n2d_dev *n2d = global_n2d;

	dev_set_drvdata(&pdev->dev, NULL);

	horizon_n2d_device_node_deinit(n2d);

	drv_exit();

	if (global_device) {
		n2d_kfree(global_device);
		global_device = N2D_NULL;
	}

	/* Power and clock. */
	if (platform->ops->putPower) {
		platform->ops->reset(platform, N2D_TRUE); /* reset assert */
		platform->ops->putPower(platform);
	}

	n2d_check_allocate_count(&allocate_count);
	if (allocate_count)
		n2d_kernel_os_print("The remaining %d memory are not freed\n", allocate_count);

	return 0;
}

static int gpu_suspend(struct platform_device *dev, pm_message_t state)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_uint32_t i = 0, j = 0;

	for (i = 0; i < global_device->kernel->dev_num; i++) {
		gcmkASSERT(global_device->kernel->sub_dev[i]->id == i);

		for (j = 0; j < global_device->kernel->dev_core_num; j++)
			ONERROR(n2d_kernel_hardware_set_power(
				global_device->kernel->sub_dev[i]->hardware[j], N2D_POWER_OFF));
	}

on_error:
	return error;
}

static int gpu_resume(struct platform_device *dev)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_uint32_t i = 0, j = 0;

	for (i = 0; i < global_device->kernel->dev_num; i++) {
		gcmkASSERT(global_device->kernel->sub_dev[i]->id == i);

		for (j = 0; j < global_device->kernel->dev_core_num; j++)
			ONERROR(n2d_kernel_hardware_set_power(
				global_device->kernel->sub_dev[i]->hardware[j], N2D_POWER_ON));
	}

on_error:
	return error;
}

#ifdef CONFIG_PM
#ifdef CONFIG_PM_SLEEP
static int gpu_system_suspend(struct device *dev)
{
	pm_message_t state = {0};
	return gpu_suspend(to_platform_device(dev), state);
}

static int gpu_system_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops default_gpu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(gpu_system_suspend, gpu_system_resume)};
#endif

static struct platform_driver gpu_driver = {.probe = gpu_probe,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
					    .remove = gpu_remove,
#else
					    .remove = __devexit_p(gpu_remove),
#endif

					    .suspend = gpu_suspend,
					    .resume  = gpu_resume,

					    .driver = {
						    .name = N2D_DEVICE_NAME,
#ifdef CONFIG_PM
						    .pm = &default_gpu_pm_ops,
#endif
					    }};

static int gpu_init(void)
{
	int ret;

	ret = n2d_kernel_platform_init(&gpu_driver, &platform);
	if (ret || !platform) {
		printk(KERN_ERR "galcore: Soc platform init failed.\n");
		return -ENODEV;
	}

	n2d_kernel_os_query_operations(&platform->ops);

	ret = platform_driver_register(&gpu_driver);

	if (ret) {
		printk(KERN_ERR "galcore: gpu_init() failed to register driver!\n");
		n2d_kernel_platform_terminate(platform);
		platform = NULL;
		return -ENODEV;
	}

	platform->driver = &gpu_driver;

	return 0;
}

static void gpu_exit(void)
{
	platform_driver_unregister(platform->driver);
	n2d_kernel_platform_terminate(platform);
	platform = N2D_NULL;
}

module_init(gpu_init);
module_exit(gpu_exit);
