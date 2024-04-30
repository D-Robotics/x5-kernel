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

#include "nano2D_kernel_platform.h"

n2d_error_t _adjust_param(IN n2d_linux_platform_t *Platform,
			  OUT n2d_linux_module_parameters_t *Args);

n2d_error_t _get_gpu_physical(IN n2d_linux_platform_t *Platform, IN n2d_uint64_t CPUPhysical,
			      OUT n2d_uint64_t *GPUPhysical);

n2d_error_t _get_cpu_physical(IN n2d_linux_platform_t *Platform, IN n2d_uint64_t GPUPhysical,
			      OUT n2d_uint64_t *CPUPhysical);

n2d_error_t _set_power(IN n2d_linux_platform_t *Platform, IN n2d_int32_t GPU, IN n2d_bool_t Enable);

static struct n2d_linux_operations default_ops = {
	.adjust_param	  = _adjust_param,
	.get_gpu_physical = _get_gpu_physical,
	.get_cpu_physical = _get_cpu_physical,
	.set_power	  = _set_power,
};

#if USE_LINUX_PCIE

#define MAX_PCIE_DEVICE 4
#define MAX_PCIE_BAR	6

/* Which bar is for local memory */
#define LM_BAR 1
/* Local memory GPU base addr */
#define LM_GPU_BASE 0x100000000

typedef struct _gcsBARINFO {
	n2d_uint64_t base;
	n2d_int32_t size;
	n2d_pointer logical;
} gcsBARINFO, *gckBARINFO;

struct n2d_pcie_info {
	gcsBARINFO bar[MAX_PCIE_BAR];
	struct pci_dev *pdev;
};

struct n2d_platform_pcie {
	n2d_linux_platform_t base;
	struct n2d_pcie_info pcie_info[MAX_PCIE_DEVICE];
	n2d_uint32_t device_number;
};

struct n2d_platform_pcie default_platform = {
	.base =
		{
			.name = __FILE__,
			.ops  = &default_ops,
		},
};

void _QueryBarInfo(struct pci_dev *Pdev, n2d_uint64_t *BarAddr, n2d_uint32_t *BarSize,
		   n2d_uint32_t BarNum)
{
	n2d_uint32_t addr;
	n2d_uint32_t size;

	/* Read the bar address */
	if (pci_read_config_dword(Pdev, PCI_BASE_ADDRESS_0 + BarNum * 0x4, &addr) < 0)
		return;

	/* Read the bar size */
	if (pci_write_config_dword(Pdev, PCI_BASE_ADDRESS_0 + BarNum * 0x4, 0xffffffff) < 0)
		return;

	if (pci_read_config_dword(Pdev, PCI_BASE_ADDRESS_0 + BarNum * 0x4, &size) < 0)
		return;

	size &= 0xfffffff0;
	size = ~size;
	size += 1;

	/* Write back the bar address */
	if (pci_write_config_dword(Pdev, PCI_BASE_ADDRESS_0 + BarNum * 0x4, addr) < 0)
		return;

	printk("Bar%d addr=0x%x, size=0x%x, irq=%d\n", BarNum, addr, size, Pdev->irq);

	*BarAddr = addr;
	*BarSize = size;
}
#else
static n2d_linux_platform_t default_platform = {
	.name = __FILE__,
	.ops  = &default_ops,
};
#endif

n2d_error_t _adjust_param(IN n2d_linux_platform_t *Platform,
			  OUT n2d_linux_module_parameters_t *Args)
{
#if USE_LINUX_PCIE
	struct n2d_platform_pcie *pcie_platform = (struct n2d_platform_pcie *)Platform;
	n2d_uint32_t i;
	n2d_uint32_t dev_index, bar_index;

	for (dev_index = 0; dev_index < pcie_platform->device_number; dev_index++) {
		struct pci_dev *pcieDev = pcie_platform->pcie_info[dev_index].pdev;

		for (i = 0; i < MAX_PCIE_BAR; i++) {
			_QueryBarInfo(pcieDev, &pcie_platform->pcie_info[dev_index].bar[i].base,
				      &pcie_platform->pcie_info[dev_index].bar[i].size, i);
		}

		for (i = 0; i < NANO2D_CORE_MAX; i++) {
			if (Args->bars[i] != -1) {
				bar_index = Args->bars[i];
				/*bar1 to core0,bar2 to core1*/
				Args->irq_line[i] = pcieDev->irq;

				Args->register_sizes[i] = 0x20000;
				Args->register_bases_mapped[i] =
					pcie_platform->pcie_info[dev_index].bar[bar_index].logical =
						(n2d_pointer)pci_iomap(pcieDev, bar_index,
								       Args->register_sizes[i]);

				Args->register_bases[i] =
					(n2d_uint32_t)pcie_platform->pcie_info[dev_index]
						.bar[bar_index]
						.base;
			}
		}
	}

	Args->contiguous_requested = N2D_TRUE;

#else

	/*Non-PCIE devices currently only support single core*/
	Args->register_bases[0] = 0x8C000000;
	Args->register_sizes[0] = 0x20000;
	Args->irq_line[0]	= 89;

#endif
	return N2D_SUCCESS;
}

n2d_error_t _get_gpu_physical(IN n2d_linux_platform_t *Platform, IN n2d_uint64_t CPUPhysical,
			      OUT n2d_uint64_t *GPUPhysical)
{
	/* Need customer to implement.
	#if USE_LINUX_PCIE
	    struct n2d_platform_pcie *pcie_platform = (struct n2d_platform_pcie *)Platform;
	    n2d_uint64_t base_addr = pcie_platform->pcie_info.bar[LM_BAR].base;
	    n2d_uint32_t size = pcie_platform->pcie_info.bar[LM_BAR].size;

	    if (base_addr) {
		if ((CPUPhysical >= base_addr) && (CPUPhysical < (base_addr + size)))
		    *GPUPhysical = CPUPhysical - base_addr + LM_GPU_BASE;
		else
		    *GPUPhysical = CPUPhysical;
	    } else {
		*GPUPhysical = CPUPhysical;
	    }
	#else */
	*GPUPhysical = CPUPhysical;
	/* #endif */

	return N2D_SUCCESS;
}

n2d_error_t _get_cpu_physical(IN n2d_linux_platform_t *Platform, IN n2d_uint64_t GPUPhysical,
			      OUT n2d_uint64_t *CPUPhysical)
{
	/*
	#if USE_LINUX_PCIE
	    Need customer to implement.
	#else */
	*CPUPhysical = GPUPhysical;
	/* #endif */
	return N2D_SUCCESS;
}

n2d_error_t _set_power(IN n2d_linux_platform_t *Platform, IN n2d_int32_t GPU, IN n2d_bool_t Enable)
{
	/*
	 * TODO: Need customer to implement.
	 */
	return N2D_SUCCESS;
}

#if USE_LINUX_PCIE
static const struct pci_device_id vivpci_ids[] = {{.class	= 0x000000,
						   .class_mask	= 0x000000,
						   .vendor	= 0x10ee,
						   .device	= 0x7012,
						   .subvendor	= PCI_ANY_ID,
						   .subdevice	= PCI_ANY_ID,
						   .driver_data = 0},
						  {/* End: all zeroes */}};

static n2d_int32_t gpu_sub_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	static u64 dma_mask = DMA_BIT_MASK(40);
#else
	static u64 dma_mask = DMA_40BIT_MASK;
#endif

	if (pci_enable_device(pdev))
		printk(KERN_ERR "galcore: pci_enable_device() failed.\n");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	if (dma_set_mask(&pdev->dev, dma_mask))
		printk(KERN_ERR "galcore: Failed to set DMA mask.\n");
#else
	if (pci_set_dma_mask(pdev, dma_mask))
		printk(KERN_ERR "galcore: Failed to set DMA mask.\n");
#endif

	pci_set_master(pdev);

	if (pci_request_regions(pdev, "nano2d"))
		printk(KERN_ERR "galcore: Failed to get ownership of BAR region.\n");

	default_platform.pcie_info[default_platform.device_number++].pdev = pdev;
	return 0;
}

static void gpu_sub_remove(struct pci_dev *pdev)
{
	pci_set_drvdata(pdev, NULL);

	pci_clear_master(pdev);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	return;
}

static struct pci_driver gpu_pci_subdriver = {.name	= N2D_DEVICE_NAME,
					      .id_table = vivpci_ids,
					      .probe	= gpu_sub_probe,
					      .remove	= gpu_sub_remove};

#endif

static struct platform_device *default_dev;

n2d_int32_t n2d_kernel_platform_init(struct platform_driver *pdrv, n2d_linux_platform_t **platform)
{
	n2d_int32_t ret;
	default_dev = platform_device_alloc(pdrv->driver.name, -1);

	if (!default_dev) {
		printk(KERN_ERR "galcore: platform_device_alloc failed.\n");
		return -ENOMEM;
	}

	/* Add device */
	ret = platform_device_add(default_dev);
	if (ret) {
		printk(KERN_ERR "galcore: platform_device_add failed.\n");
		goto on_error;
	}

	*platform = (n2d_linux_platform_t *)&default_platform;

#if USE_LINUX_PCIE
	ret = pci_register_driver(&gpu_pci_subdriver);
#endif

	return 0;

on_error:

	platform_device_put(default_dev);

	return ret;
}

n2d_int32_t n2d_kernel_platform_terminate(n2d_linux_platform_t *platform)
{
	if (default_dev) {
		platform_device_unregister(default_dev);
		default_dev = NULL;
	}

#if USE_LINUX_PCIE
	{
		n2d_uint32_t dev_index;
		struct n2d_platform_pcie *pcie_platform = (struct n2d_platform_pcie *)platform;
		for (dev_index = 0; dev_index < pcie_platform->device_number; dev_index++) {
			n2d_uint32_t i;
			for (i = 0; i < MAX_PCIE_BAR; i++) {
				if (pcie_platform->pcie_info[dev_index].bar[i].logical != 0) {
					pci_iounmap(
						pcie_platform->pcie_info[dev_index].pdev,
						pcie_platform->pcie_info[dev_index].bar[i].logical);
				}
			}
		}

		pci_unregister_driver(&gpu_pci_subdriver);
	}
#endif

	return 0;
}

void n2d_kernel_os_query_operations(n2d_linux_operations_t **ops)
{
	*ops = &default_ops;
}
