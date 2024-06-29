// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 */

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/clk-provider.h>
#include <linux/pm_runtime.h>
#include <linux/mod_devicetable.h>
#include <hobot_ion_iommu.h>
#include "nano2D_kernel_platform.h"

static n2d_error_t _adjust_param(IN n2d_linux_platform_t *Platform,
				 OUT n2d_linux_module_parameters_t *Args);

static n2d_error_t _get_gpu_physical(IN n2d_linux_platform_t *Platform, IN n2d_uint64_t CPUPhysical,
				     IN n2d_uint32_t size, OUT n2d_uint64_t *GPUPhysical);

static n2d_error_t _get_cpu_physical(IN n2d_linux_platform_t *Platform, IN n2d_uint64_t GPUPhysical,
				     OUT n2d_uint64_t *CPUPhysical);

static n2d_error_t _set_power(IN n2d_linux_platform_t *Platform, IN n2d_int32_t GPU,
			      IN n2d_bool_t Enable);

static n2d_error_t _setClock(IN n2d_linux_platform_t *Platform, IN n2d_int32_t GPU,
			     IN n2d_bool_t Enable);

static n2d_error_t _getPower(IN n2d_linux_platform_t *Platform);

static n2d_error_t _putPower(IN n2d_linux_platform_t *Platform);

static n2d_error_t _reset(IN n2d_linux_platform_t *Platform, IN n2d_bool_t Assert);

static struct n2d_linux_operations default_ops = {
	.adjust_param	  = _adjust_param,
	.get_gpu_physical = _get_gpu_physical,
	.get_cpu_physical = _get_cpu_physical,
	.set_power	  = _set_power,
	.setClock	  = _setClock,
	.getPower     = _getPower,
	.putPower     = _putPower,
	.reset        = _reset,
};

static n2d_linux_platform_t default_platform = {
	.name = __FILE__,
	.ops  = &default_ops,
};

static int gpu_parse_dt(struct platform_device *pdev, n2d_linux_module_parameters_t *params)
{
	struct resource *res;
	struct device_node *root = pdev->dev.of_node;
	struct device_node *iommu;
	const u32 *value;
	int ret = 0;

	/* parse the irqs config */
	ret = of_irq_get(root, 0);
	if (ret)
		params->irq_line[0] = ret;
	else
		return -EINVAL;

	/* parse the registers config */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		params->register_bases[0] = res->start;
		params->register_sizes[0] = res->end - res->start + 1;
	} else {
		return -EINVAL;
	}
	pr_info("%s: register base(%llx), irq(%d).\n", __func__, params->register_bases[0],
		params->irq_line[0]);

	/* parse the contiguous mem */
	value = of_get_property(root, "contiguous-size", NULL);
	if (value && *value != 0) {
		u32 addr;
		u32 size;

		of_property_read_u32(root, "contiguous-size", &size);
		params->contiguous_size = size;
		of_property_read_u32(root, "contiguous-base", &addr);
		params->contiguous_base = addr;
		pr_info("%s: parse continuous base(%llx), size(%llx).\n", __func__,
			params->contiguous_base, params->contiguous_size);
	}

	iommu = of_parse_phandle(root, "iommus", 0);
	if (iommu)
		params->iommu = N2D_TRUE;
	of_node_put(iommu);

	return 0;
}

static void enable_clk(n2d_linux_platform_t *Platform)
{
	int i;

	for (i = 0; i < Platform->clock.clk_num; i++) {
		struct clk *clock = Platform->clock.clk_info[i].clk;
		clk_prepare_enable(clock);
	}
}

static void disable_clk(n2d_linux_platform_t *Platform)
{
	int i;

	for (i = Platform->clock.clk_num - 1; i >= 0; i--) {
		struct clk *clock = Platform->clock.clk_info[i].clk;
		clk_disable_unprepare(clock);
	}
}

static int get_power(struct platform_device *pdev)
{
	int err = 0;
	struct device *dev;

	if (!pdev)
		return -ENODEV;

	dev = &pdev->dev;

	err = pm_runtime_get_sync(dev);
	if (err < 0) {
		dev_err(dev, "pm_runtime_get_sync failed %d\n", err);
		return err;
	}

	return 0;
}

static int put_power(struct platform_device *pdev)
{
	int err = 0;
	struct device *dev;

	if (!pdev)
		return -ENODEV;

	dev = &pdev->dev;

	err = pm_runtime_put_sync(dev);
	if (err < 0) {
		dev_err(dev, "pm_runtime_put failed %d\n", err);
		return err;
	}

	return 0;
}

n2d_error_t _adjust_param(IN n2d_linux_platform_t *Platform,
			  OUT n2d_linux_module_parameters_t *Args)
{
	/* add power related if 2d has power control */
	return gpu_parse_dt(Platform->device, Args);
}

n2d_error_t _get_gpu_physical(IN n2d_linux_platform_t *Platform, IN n2d_uint64_t CPUPhysical,
			      IN n2d_uint32_t size, OUT n2d_uint64_t *GPUPhysical)
{
#if IS_ENABLED(CONFIG_DROBOT_LITE_MMU)
	struct device *dev = &Platform->device->dev;
	n2d_uint64_t gpu_physical = 0;

	ion_iommu_map_ion_phys(dev, (phys_addr_t)CPUPhysical, ALIGN(size, PAGE_SIZE), (dma_addr_t *)&gpu_physical, 0);
	pr_debug("%s: CPUPhysical = 0x%llx, gpu_physical = 0x%llx.\n", __func__, CPUPhysical, gpu_physical);

	*GPUPhysical = gpu_physical;
#else
	*GPUPhysical = CPUPhysical;
#endif

	return N2D_SUCCESS;
}

n2d_error_t _get_cpu_physical(IN n2d_linux_platform_t *Platform, IN n2d_uint64_t GPUPhysical,
			      OUT n2d_uint64_t *CPUPhysical)
{
	*CPUPhysical = GPUPhysical;
	return N2D_SUCCESS;
}

n2d_error_t _getPower(IN n2d_linux_platform_t *Platform)
{
	int i, ret;
	struct device *dev    = &Platform->device->dev;
	struct n2d_clk *clock = &Platform->clock;
	struct n2d_clk_info *clk_info;

	clock->clk_num = of_property_count_strings(dev->of_node, "clock-names");
	if (clock->clk_num > 0) {
		clock->clk_info = devm_kcalloc(dev, clock->clk_num, sizeof(*clk_info), GFP_KERNEL);
		if (!clock->clk_info)
			return -ENOMEM;
	} else {
		dev_warn(dev, "no clock for this device.\n");
		return -EINVAL;
	}

	for (i = 0; i < clock->clk_num; i++) {
		clk_info = &clock->clk_info[i];
		ret	 = of_property_read_string_index(dev->of_node, "clock-names", i,
							 &clk_info->clk_name);
		if (ret) {
			dev_err(dev, "Failed to get clock name id = %d\n", i);
			return ret;
		}
		clk_info->clk = devm_clk_get(dev, clk_info->clk_name);
		if (IS_ERR(clk_info->clk)) {
			dev_err(dev, "devm_clk_get (%d)%s failed.\n", i, clk_info->clk_name);
			return PTR_ERR(clk_info->clk);
		} else {
			dev_info(dev, "clk name: %s, freq = %ld.\n", clk_info->clk_name,
				 clk_get_rate(clk_info->clk));
		}
	}

	Platform->rst = devm_reset_control_get(dev, "gc820_sw_rst");

	if (!Platform->rst) {
		dev_err(dev, "failed to get reset control.\n");
		return -EINVAL;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return N2D_SUCCESS;
}

n2d_error_t _putPower(IN n2d_linux_platform_t *Platform)
{
	int i;
	struct device *dev = &Platform->device->dev;

	for (i = 0; i < Platform->clock.clk_num; i++) {
		devm_clk_put(dev, Platform->clock.clk_info[i].clk);
		Platform->clock.clk_info[i].clk = NULL;
	}

	devm_kfree(dev, Platform->clock.clk_info);

	pm_runtime_disable(dev);

	return N2D_SUCCESS;
}

n2d_error_t _set_power(IN n2d_linux_platform_t *Platform, IN n2d_int32_t GPU, IN n2d_bool_t Enable)
{
	if (Enable)
		return get_power(Platform->device);
	else
		return put_power(Platform->device);
}

n2d_error_t _setClock(IN n2d_linux_platform_t *Platform, IN n2d_int32_t GPU, IN n2d_bool_t Enable)
{
	if (Enable)
		enable_clk(Platform);
	else
		disable_clk(Platform);

	return N2D_SUCCESS;
}

n2d_error_t _reset(IN n2d_linux_platform_t *Platform, IN n2d_bool_t Assert)
{
	struct reset_control *rst = Platform->rst;

	if (Assert)
		return reset_control_assert(rst);
	else
		return reset_control_deassert(rst);
}

static const struct of_device_id gpu_dt_ids[] = {{
							 .compatible = "verisilicon,gc820",
						 },
						 {/* sentinel */}};

MODULE_DEVICE_TABLE(of, gpu_dt_ids);

n2d_int32_t n2d_kernel_platform_init(struct platform_driver *pdrv, n2d_linux_platform_t **platform)
{
	pdrv->driver.of_match_table = gpu_dt_ids;

	*platform = (n2d_linux_platform_t *)&default_platform;

	return 0;
}

n2d_int32_t n2d_kernel_platform_terminate(n2d_linux_platform_t *platform)
{
	/* disable power */
	return 0;
}

void n2d_kernel_os_query_operations(n2d_linux_operations_t **ops)
{
	*ops = &default_ops;
}
