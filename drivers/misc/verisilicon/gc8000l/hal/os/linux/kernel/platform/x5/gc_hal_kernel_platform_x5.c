// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 */

/*
 *   dts node example:
 *   gpu_3d: gpu@53100000 {
 *       compatible = "verisilicon,galcore";
 *       reg = <0 0x53100000 0 0x40000>,
 *               <0 0x54100000 0 0x40000>;
 *       reg-names = "core_major", "core_3d1";
 *       interrupts = <GIC_SPI 64 IRQ_TYPE_LEVEL_HIGH>,
 *                   <GIC_SPI 65 IRQ_TYPE_LEVEL_HIGH>;
 *       interrupt-names = "core_major", "core_3d1";
 *       clocks = <&clk IMX_SC_R_GPU_0_PID0 IMX_SC_PM_CLK_PER>,
 *               <&clk IMX_SC_R_GPU_0_PID0 IMX_SC_PM_CLK_MISC>,
 *               <&clk IMX_SC_R_GPU_1_PID0 IMX_SC_PM_CLK_PER>,
 *               <&clk IMX_SC_R_GPU_1_PID0 IMX_SC_PM_CLK_MISC>;
 *       clock-names = "core_major", "core_major_sh", "core_3d1", "core_3d1_sh";
 *       assigned-clocks = <&clk IMX_SC_R_GPU_0_PID0 IMX_SC_PM_CLK_PER>,
 *                       <&clk IMX_SC_R_GPU_0_PID0 IMX_SC_PM_CLK_MISC>,
 *                       <&clk IMX_SC_R_GPU_1_PID0 IMX_SC_PM_CLK_PER>,
 *                       <&clk IMX_SC_R_GPU_1_PID0 IMX_SC_PM_CLK_MISC>;
 *       assigned-clock-rates = <700000000>, <850000000>, <800000000>, <1000000000>;
 *       power-domains = <&pd IMX_SC_R_GPU_0_PID0>, <&pd IMX_SC_R_GPU_1_PID0>;
 *       power-domain-names = "core_major", "core_3d1";
 *       contiguous-base = <0x0>;
 *       contiguous-size = <0x1000000>;
 *       status = "okay";
 *   };
 */

#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_platform.h"
#include "gc_hal_kernel_platform_x5.h"
#if gcdSUPPORT_DEVICE_TREE_SOURCE
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/clk.h>
#endif
#include <linux/of_irq.h>
#include <linux/pm.h>
#include <linux/pm_opp.h>
#include <linux/reset.h>

/* Disable MSI for internal FPGA build except PPC */
#if gcdFPGA_BUILD
#define USE_MSI 0
#else
#define USE_MSI 1
#endif

#define gcdMIXED_PLATFORM 0

gceSTATUS _AdjustParam(gcsPLATFORM *platform, gcsMODULE_PARAMETERS *Args);

gceSTATUS _GetGPUPhysical(IN gcsPLATFORM *platform, IN gctPHYS_ADDR_T CPUPhysical,
			  gctPHYS_ADDR_T *GPUPhysical);

#if gcdENABLE_MP_SWITCH
gceSTATUS _SwitchCoreCount(IN gcsPLATFORM *Platform, gctUINT32 *Count);
#endif

#if gcdENABLE_VIDEO_MEMORY_MIRROR
gceSTATUS _dmaCopy(IN gctPOINTER Object, IN gctPOINTER DstNode, IN gctPOINTER SrcNode,
		   IN gctUINT32 Reason);
#endif

#if gcdSUPPORT_DEVICE_TREE_SOURCE
static int gpu_parse_dt(struct platform_device *pdev, gcsMODULE_PARAMETERS *params);

static gceSTATUS _set_power(IN gcsPLATFORM *platform, IN gctUINT32 DevIndex, IN gceCORE GPU,
		     IN gctBOOL Enable);

static gceSTATUS _set_clock(gcsPLATFORM *platform, gctUINT32 DevIndex, gceCORE GPU, gctBOOL Enable);
#endif

#ifdef gcdENABLE_REGULATOR
#include <linux/regulator/consumer.h>
#define REGULATOR_ID  "gpu" //regulator id defined in dts
static gceSTATUS gc_regulator_init(IN gcsPLATFORM *Platform);
static gceSTATUS gc_regulator_set_voltage(IN gcsPLATFORM *Platform);
static gceSTATUS gc_regulator_deinit(IN gcsPLATFORM *Platform);
#endif

static struct _gcsPLATFORM_OPERATIONS default_ops = {
	.adjustParam	= _AdjustParam,
	.getGPUPhysical = _GetGPUPhysical,
#if gcdENABLE_MP_SWITCH
	.switchCoreCount = _SwitchCoreCount,
#endif
#if gcdENABLE_VIDEO_MEMORY_MIRROR
	.dmaCopy = _dmaCopy,
#endif
#if gcdSUPPORT_DEVICE_TREE_SOURCE
	.setPower = _set_power,
	.setClock = _set_clock,
#endif
};

static const char *core_names[] = {
	"core_major", "core_3d1",  "core_3d2", "core_3d3",  "core_3d4",	 "core_3d5",  "core_3d6",
	"core_3d7",   "core_3d8",  "core_3d9", "core_3d10", "core_3d11", "core_3d12", "core_3d13",
	"core_3d14",  "core_3d15", "core_2d",  "core_2d1",  "core_2d2",	 "core_2d3",  "core_vg",
#if gcdDEC_ENABLE_AHB
	"core_dec",
#endif
};

#define MAX_OPP_NUM  10
static gceSTATUS gpu_get_opp_table(gcsPLATFORM *platform)
{
	struct platform_device *pdev = platform->device;
	struct device *dev = &pdev->dev;
	struct device_node *root = dev->of_node;
	struct device_node *opp_node = of_parse_phandle(root, "operating-points-v2", 0);
	struct device_node *node;
	struct gpu_power_domain *gpd = &platform->gpd;
	int i = 0, err = 0;
	int count = 0;

	if (!opp_node)
		return 0;

	err = dev_pm_opp_of_add_table(dev);
	if (err) {
		dev_warn(dev, "Failed to add opp table %d.\n", err);
		return err;
	}

	count = dev_pm_opp_get_opp_count(dev);
	if (count <= 0) {
		pr_warn("no pre opp count available for this device.");
		gpd->opp_num = MAX_OPP_NUM;
	} else {
		gpd->opp_num = count;
	}
	gpd->opp_table = kmalloc_array(gpd->opp_num, sizeof(struct gcs_devfreq_opp), GFP_KERNEL);
	if (!gpd->opp_table)
		return -ENOMEM;

	pr_info("start to add opp table.\n");
	for_each_available_child_of_node(opp_node, node) {
		u64 opp_freq;
#ifdef gcdENABLE_REGULATOR
		u32 opp_volts;
#endif
		err = of_property_read_u64(node, "opp-hz", &opp_freq);
		if (err) {
			dev_warn(dev, "Failed to read opp-hz property with error %d\n", err);
			continue;
		}
		gpd->opp_table[i].opp_freq = opp_freq;
		gpd->opp_table[i].core_mask = 0;

#ifdef gcdENABLE_REGULATOR
		err = of_property_read_u32(node,
						"opp-microvolt", &opp_volts); //fixed 1 values
		if (err < 0) {
			dev_warn(dev, "Failed to read opp-microvolt property with error %d\n", err);
		}

		gpd->opp_table[i].opp_volts = opp_volts;
		pr_info("opp item value %lld, %d.\n", gpd->opp_table[i].opp_freq, gpd->opp_table[i].opp_volts);
#endif
		i++;
	}

	gpd->opp_num = i;
	pr_info("finish to add opp table %d.\n", gpd->opp_num);

	return 0;
}

static gceSTATUS gpu_put_opp_table(gcsPLATFORM *platform)
{
	kfree(platform->gpd.opp_table);

	return 0;
}

gceSTATUS _set_clock(gcsPLATFORM *Platform, gctUINT32 DevIndex, gceCORE GPU, gctBOOL Enable)
{
	int i = 0;

	if (Enable) {
		for (i = 0; i < Platform->gpd.clk_num; i++) {
			if (Platform->gpd.core_clk[i]) {
				clk_prepare(Platform->gpd.core_clk[i]);
				clk_enable(Platform->gpd.core_clk[i]);
			}
		}
	} else {
		for (i = Platform->gpd.clk_num - 1; i >= 0; i--) {
			if (Platform->gpd.core_clk[i]) {
				clk_disable(Platform->gpd.core_clk[i]);
				clk_unprepare(Platform->gpd.core_clk[i]);
			}
		}
	}

	return gcvSTATUS_OK;
}

gceSTATUS _set_power(IN gcsPLATFORM *Platform, IN gctUINT32 DevIndex, IN gceCORE GPU,
		     IN gctBOOL Enable)
{
	if (Enable) {
#ifdef gcdENABLE_REGULATOR
		gc_regulator_init(Platform);
#endif
		pm_runtime_get_sync(&Platform->device->dev);
	}
	else {
		pm_runtime_put(&Platform->device->dev);
#ifdef gcdENABLE_REGULATOR
		gc_regulator_deinit(Platform);
#endif
	}

	return gcvSTATUS_OK;
}

static int gpu_remove_power_domains(struct _gcsPLATFORM *platform)
{
	struct gpu_power_domain *gpd = &platform->gpd;
	int i = 0;

	for (i = gpd->clk_num - 1; i >= 0; i--) {
		if (gpd->core_clk[i]) {
			clk_put(gpd->core_clk[i]);
			gpd->core_clk[i] = NULL;
		}
	}

	reset_control_assert(platform->rst);

	if (gpd->power_dev) {
		pm_runtime_disable(gpd->power_dev);
		dev_pm_domain_detach(gpd->power_dev, true);
	}

	return 0;
}

static int gpu_add_power_domains(gcsPLATFORM *platform)
{
	struct platform_device *pdev = platform->device;
	struct device *dev = &pdev->dev;
	struct device_node *root = dev->of_node;
	struct gpu_power_domain *gpd = &platform->gpd;
	int    ret = 0, i = 0;

	/* x5 only have one power domain*/
	memset(gpd, 0, sizeof(struct gpu_power_domain));

	ret = dev_pm_domain_attach(dev, true);
	if (ret)
		goto error;

	gpd->power_dev = dev;

	for (i = 0; i < MAX_CLK_NUM; i++) {
		gpd->core_clk[i] = of_clk_get(root, i);
		if (IS_ERR_OR_NULL(gpd->core_clk[i])) {
			ret = PTR_ERR(gpd->core_clk[i]);
			gpd->core_clk[i] = NULL;
			break;
		}
	}
	gpd->clk_num = i;
	if (gpd->clk_num == 0)
		goto error;

	pm_runtime_enable(&pdev->dev);

	platform->rst = devm_reset_control_get(dev, "gc8000l_sw_rst");

	if (IS_ERR(platform->rst)) {
		dev_err(dev, "failed to get reset control.\n");
		return -EINVAL;
	}

	reset_control_deassert(platform->rst);

	return 0;

error:
	if (gpd->power_dev)
		dev_pm_domain_detach(gpd->power_dev, true);
	return ret;
}

/* regulator simple govorner */
#ifdef gcdENABLE_REGULATOR

static gceSTATUS gc_regulator_init(IN gcsPLATFORM *Platform) {
	gceSTATUS status	      = gcvSTATUS_OK;
	struct device *galcore_device = NULL;
	gcsMODULE_PARAMETERS *params = &Platform->params;

	if (!Platform || !Platform->device)
		return gcvSTATUS_INVALID_ADDRESS;

	if (params->regulator) {
		return gcvSTATUS_OK;
	}

	galcore_device = &Platform->device->dev;

	params->regulator = regulator_get_optional(galcore_device, REGULATOR_ID);
	if (IS_ERR_OR_NULL(params->regulator)) {
		status = PTR_ERR(params->regulator);
		params->regulator = NULL;
		return status;
	}

	//if (!regulator_is_enabled(params->regulator))
	WARN_ON(regulator_enable(params->regulator));

	pr_info("Regulators get successfully.\n");
	return 0;
}

static gceSTATUS gc_regulator_set_voltage(IN gcsPLATFORM *Platform) {
	gceSTATUS status	      = gcvSTATUS_OK;
	gcsMODULE_PARAMETERS *params = &Platform->params;
	struct gpu_power_domain *gpd = &Platform->gpd;

	if (!params->regulator) {
		pr_info("error regulator.\n");
		return gcvSTATUS_INVALID_ADDRESS;
	}

	if (!gpd->opp_table) {
		pr_info("error opp table.\n");
		return 0;
	}

	params->voltage = gpd->opp_table[0].opp_volts;

	pr_info("Regulators set voltage value %d.\n", params->voltage);

	status = regulator_set_voltage(params->regulator, params->voltage, params->voltage);
	if (status) {
		pr_err("Failed to set voltage (%d) (target %u)\n", status, params->voltage);
		return status;
	}

	pr_info("Regulators set voltage successfully.\n");
	return 0;
}

static gceSTATUS gc_regulator_deinit(IN gcsPLATFORM *Platform) {
	gcsMODULE_PARAMETERS *params = &Platform->params;

	if (!params || !params->regulator)
		return gcvSTATUS_INVALID_ADDRESS;

	if (regulator_is_enabled(params->regulator))
		WARN_ON(regulator_disable(params->regulator));

	regulator_put(params->regulator);
	params->regulator = NULL;

	pr_info("Regulators put successfully.\n");
	return 0;
}

#endif

static int gpu_parse_dt(struct platform_device *pdev, gcsMODULE_PARAMETERS *params)
{
	struct device_node *root = pdev->dev.of_node;
	struct resource *res;
	gctUINT32 i, data;
	const gctUINT32 *value;
	int irq;

	gcmSTATIC_ASSERT(gcvCORE_COUNT == gcmCOUNTOF(core_names),
			 "core_names array does not match core types");

	/* parse the irqs config */
	irq = of_irq_get(root, 0);
	if (irq) {
		params->irqs[0] = irq;
	} else {
		return -EINVAL;
	}

	/* parse the registers config */
	for (i = 0; i < gcvCORE_COUNT; i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, core_names[i]);
		if (res) {
			params->registerBases[i] = res->start;
			params->registerSizes[i] = res->end - res->start + 1;
		}
	}

	/* parse the contiguous mem */
	value = of_get_property(root, "contiguous-size", gcvNULL);
	if (value && *value != 0) {
		gctUINT64 addr;

		of_property_read_u64(root, "contiguous-base", &addr);
		params->contiguousSize = *value;
		params->contiguousBase = addr;
	}

	value = of_get_property(root, "contiguous-requested", gcvNULL);
	if (value)
		params->contiguousRequested = *value ? gcvTRUE : gcvFALSE;

	/* parse the external mem */
	value = of_get_property(root, "external-size", gcvNULL);
	if (value && *value != 0) {
		gctUINT64 addr;

		of_property_read_u64(root, "external-base", &addr);
		params->externalSize[0] = *value;
		params->externalBase[0] = addr;
	}

	value = of_get_property(root, "phys-size", gcvNULL);
	if (value && *value != 0) {
		gctUINT64 addr;

		of_property_read_u64(root, "base-address", &addr);
		params->physSize    = *value;
		params->baseAddress = addr;
	}

	value = of_get_property(root, "phys-size", gcvNULL);
	if (value)
		params->bankSize = *value;

	value = of_get_property(root, "recovery", gcvNULL);
	if (value)
		params->recovery = *value;

	/* enable power management*/
	value = of_get_property(root, "power-management", gcvNULL);
	if (value)
		params->powerManagement = *value;

	value = of_get_property(root, "enable-mmu", gcvNULL);
	if (value)
		params->enableMmu = *value;

	for (i = 0; i < gcvCORE_3D_MAX; i++) {
		data = 0;
		of_property_read_u32_index(root, "user-cluster-masks", i, &data);
		if (data)
			params->userClusterMasks[i] = data;
	}

	value = of_get_property(root, "stuck-dump", gcvNULL);
	if (value)
		params->stuckDump = *value;

	value = of_get_property(root, "show-args", gcvNULL);
	if (value)
		params->showArgs = *value;

	value = of_get_property(root, "mmu-page-table-pool", gcvNULL);
	if (value)
		params->mmuPageTablePool = *value;

	value = of_get_property(root, "mmu-dynamic-map", gcvNULL);
	if (value)
		params->mmuDynamicMap = *value;

	value = of_get_property(root, "all-map-in-one", gcvNULL);
	if (value)
		params->allMapInOne = *value;

	value = of_get_property(root, "isr-poll-mask", gcvNULL);
	if (value)
		params->isrPoll = *value;

	return 0;
}

static const struct of_device_id gpu_dt_ids[] = {{
							 .compatible = "verisilicon,galcore",
						 },

						 {/* sentinel */}};

MODULE_DEVICE_TABLE(of, gpu_dt_ids);

static struct _gcsPLATFORM default_platform = {
	.name = __FILE__,
	.ops  = &default_ops,
};

gceSTATUS _AdjustParam(IN gcsPLATFORM *Platform, OUT gcsMODULE_PARAMETERS *Args)
{
	gpu_parse_dt(Platform->device, Args);

	gpu_get_opp_table(Platform);

#ifdef gcdENABLE_REGULATOR  //only set voltage at power stage one time
	{
		gceSTATUS err = 0;

		/* set voltage */
		err = gc_regulator_init(Platform);
		if (!err) {
			err = gc_regulator_set_voltage(Platform);
			if (err) {
				pr_err("set regulation voltage failed.");
			}
		}
	}
#endif
	gpu_add_power_domains(Platform);
	return gcvSTATUS_OK;
}

gceSTATUS _GetGPUPhysical(IN gcsPLATFORM *Platform, IN gctPHYS_ADDR_T CPUPhysical,
			  OUT gctPHYS_ADDR_T *GPUPhysical)
{
	*GPUPhysical = CPUPhysical;

	return gcvSTATUS_OK;
}

#if gcdENABLE_MP_SWITCH
gceSTATUS _SwitchCoreCount(IN gcsPLATFORM *Platform, OUT gctUINT32 *Count)
{
	*Count = Platform->coreCount;

	return gcvSTATUS_OK;
}
#endif

#if gcdENABLE_VIDEO_MEMORY_MIRROR
gceSTATUS _dmaCopy(IN gctPOINTER Object, IN gctPOINTER DstNode, IN gctPOINTER SrcNode,
		   IN gctUINT32 Reason)
{
	gceSTATUS status	    = gcvSTATUS_OK;
	gckKERNEL kernel	    = (gckKERNEL)Object;
	gckVIDMEM_NODE dst_node_obj = (gckVIDMEM_NODE)DstNode;
	gckVIDMEM_NODE src_node_obj = (gckVIDMEM_NODE)SrcNode;
	gctPOINTER src_ptr = gcvNULL, dst_ptr = gcvNULL;
	gctSIZE_T size0, size1;
	gctPOINTER src_mem_handle = gcvNULL, dst_mem_handle = gcvNULL;
	gctBOOL src_need_unmap = gcvFALSE, dst_need_unmap = gcvFALSE;

	gcsPLATFORM *platform = kernel->os->device->platform;

	status = gckVIDMEM_NODE_GetSize(kernel, src_node_obj, &size0);
	if (status)
		return status;

	status = gckVIDMEM_NODE_GetSize(kernel, dst_node_obj, &size1);
	if (status)
		return status;

	status = gckVIDMEM_NODE_GetMemoryHandle(kernel, src_node_obj, &src_mem_handle);
	if (status)
		return status;

	status = gckVIDMEM_NODE_GetMemoryHandle(kernel, dst_node_obj, &dst_mem_handle);
	if (status)
		return status;

	status = gckVIDMEM_NODE_GetMapKernel(kernel, src_node_obj, &src_ptr);
	if (status)
		return status;

	if (!src_ptr) {
		gctSIZE_T offset;

		status = gckVIDMEM_NODE_GetOffset(kernel, src_node_obj, &offset);
		if (status)
			return status;

		status = gckOS_CreateKernelMapping(kernel->os, src_mem_handle, offset, size0,
						   &src_ptr);

		if (status)
			goto error;

		src_need_unmap = gcvTRUE;
	}

	status = gckVIDMEM_NODE_GetMapKernel(kernel, dst_node_obj, &dst_ptr);
	if (status)
		return status;

	if (!dst_ptr) {
		gctSIZE_T offset;

		status = gckVIDMEM_NODE_GetOffset(kernel, dst_node_obj, &offset);
		if (status)
			return status;

		status = gckOS_CreateKernelMapping(kernel->os, dst_mem_handle, offset, size1,
						   &dst_ptr);

		if (status)
			goto error;

		dst_need_unmap = gcvTRUE;
	}

	gckOS_MemCopy(dst_ptr, src_ptr, gcmMIN(size0, size1));

error:
	if (src_need_unmap && src_ptr)
		gckOS_DestroyKernelMapping(kernel->os, src_mem_handle, src_ptr);

	if (dst_need_unmap && dst_ptr)
		gckOS_DestroyKernelMapping(kernel->os, dst_mem_handle, dst_ptr);

	return status;
}
#endif

int gckPLATFORM_Init(struct platform_driver *pdrv, struct _gcsPLATFORM **platform)
{
	int ret = 0;

	pdrv->driver.of_match_table = gpu_dt_ids;

	*platform = (gcsPLATFORM *)&default_platform;
	return ret;
}

int gckPLATFORM_Terminate(struct _gcsPLATFORM *platform)
{
	gpu_remove_power_domains(platform);

	gpu_put_opp_table(platform);

#ifdef gcdENABLE_REGULATOR
	gc_regulator_deinit(platform);
#endif

	return 0;
}
