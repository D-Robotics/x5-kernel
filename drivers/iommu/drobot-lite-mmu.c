// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 *
 * Lite MMU only convert low 4G address to high region address
 *
 */

#include <linux/iommu.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_iommu.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#include "dma-iommu.h"
#include "drobot-lite-mmu.h"

static u8 initial_map_table[MAP_TABLE_SIZE];

static void __enable_iommu(struct lite_mmu_iommu *iommu, bool enable)
{
	u32 value = 0;

	if (enable)
		value = BIT_CTRL_ENABLE;

	if(value == readl( iommu->base + MMU_REG_CTRL))
		return;

	writel(value, iommu->base + MMU_REG_CTRL);
}

static void __update_iommu(struct lite_mmu_iommu *iommu, u8 *map_table)
{
	writel(map_table[0] | ((u32)map_table[1] << MAP_CTRL_SHIFT1) |
		       ((u32)map_table[2] << MAP_CTRL_SHIFT2) |
		       ((u32)map_table[3] << MAP_CTRL_SHIFT3),
	       iommu->base + MMU_REG_MAP_CTRL0);
	writel(map_table[4] | ((u32)map_table[5] << MAP_CTRL_SHIFT1) |
		       ((u32)map_table[6] << MAP_CTRL_SHIFT2) |
		       ((u32)map_table[7] << MAP_CTRL_SHIFT3),
	       iommu->base + MMU_REG_MAP_CTRL1);
	writel(map_table[8] | ((u32)map_table[9] << MAP_CTRL_SHIFT1) |
		       ((u32)map_table[10] << MAP_CTRL_SHIFT2) |
		       ((u32)map_table[11] << MAP_CTRL_SHIFT3),
	       iommu->base + MMU_REG_MAP_CTRL2);
	writel(map_table[12] | ((u32)map_table[13] << MAP_CTRL_SHIFT1) |
		       ((u32)map_table[14] << MAP_CTRL_SHIFT2) |
		       ((u32)map_table[15] << MAP_CTRL_SHIFT3),
	       iommu->base + MMU_REG_MAP_CTRL3);
}

static void __flush_domain(struct lite_mmu_domain *domain)
{
	struct lite_mmu_iommu *iommu;
	unsigned long flags;

	spin_lock_irqsave(&domain->map_table_lock, flags);

	list_for_each_entry (iommu, &domain->iommus, node) {
		__update_iommu(iommu, domain->map_table);
		__enable_iommu(iommu, true);
	}

	spin_unlock_irqrestore(&domain->map_table_lock, flags);
}

void lite_mmu_restore_mapping(struct device *dev)
{
	struct lite_mmu_iommu *iommu = dev_get_drvdata(dev);
	struct lite_mmu_domain *domain;

	if (!iommu->domain)
		return;

	domain = to_lite_mmu_domain(iommu->domain);
	__flush_domain(domain);
	__enable_iommu(iommu, true);

	return;
}

void lite_mmu_reset_mapping(struct device *dev)
{
	struct lite_mmu_iommu *iommu = dev_get_drvdata(dev);
	struct lite_mmu_domain *domain;
	unsigned long flags;

	if (!iommu->domain)
		return;

	domain = to_lite_mmu_domain(iommu->domain);
	spin_lock_irqsave(&domain->map_table_lock, flags);

	list_for_each_entry (iommu, &domain->iommus, node)
		__update_iommu(iommu, initial_map_table);

	spin_unlock_irqrestore(&domain->map_table_lock, flags);
}

static struct iommu_domain *lite_mmu_domain_alloc(unsigned int type)
{
	struct lite_mmu_domain *domain;
	u8 i;

	if (type != IOMMU_DOMAIN_UNMANAGED && type != IOMMU_DOMAIN_DMA)
		return NULL;

	domain = kzalloc(sizeof(*domain), GFP_KERNEL);
	if (!domain)
		return NULL;

	spin_lock_init(&domain->iommus_lock);
	spin_lock_init(&domain->map_table_lock);
	INIT_LIST_HEAD(&domain->iommus);

	/* linear map from 0 */
	for (i = 0; i < MAP_TABLE_SIZE; i++) {
		domain->map_table[i]   = i;
		initial_map_table[i]   = i;
		domain->map_ref_cnt[i] = 0;
	}

	domain->domain.geometry.aperture_start = 0;
	domain->domain.geometry.aperture_end   = DMA_BIT_MASK(32);
	domain->domain.geometry.force_aperture = true;

	return &domain->domain;
}

static void lite_mmu_domain_free(struct iommu_domain *iommu_domain)
{
	struct lite_mmu_domain *domain = to_lite_mmu_domain(iommu_domain);

	WARN_ON(!list_empty(&domain->iommus));

	if (iommu_domain->type == IOMMU_DOMAIN_DMA)
		iommu_put_dma_cookie(iommu_domain);

	kfree(domain);
}

static void lite_mmu_detach_device(struct iommu_domain *iommu_domain, struct device *dev)
{
	struct lite_mmu_iommu *iommu   = dev_iommu_priv_get(dev);
	struct lite_mmu_domain *domain = to_lite_mmu_domain(iommu_domain);
	unsigned long flags;

	if (!iommu)
		return;

	/* lite mmu is already detached */
	if (iommu->domain != iommu_domain)
		return;

	iommu->domain = NULL;
	spin_lock_irqsave(&domain->iommus_lock, flags);
	list_del(&iommu->node);
	spin_unlock_irqrestore(&domain->iommus_lock, flags);

	__enable_iommu(iommu, false);
}

static int lite_mmu_attach_device(struct iommu_domain *iommu_domain, struct device *dev)
{
	struct lite_mmu_iommu *iommu   = dev_iommu_priv_get(dev);
	struct lite_mmu_domain *domain = to_lite_mmu_domain(iommu_domain);
	unsigned long flags;
	int32_t ret = 0;

	if (!iommu)
		return -ENODEV;

	/* lite mmu is already attached */
	if (iommu->domain == iommu_domain)
		return 0;

	if (iommu->domain)
		lite_mmu_detach_device(iommu->domain, dev);

	iommu->domain = iommu_domain;

	INIT_LIST_HEAD(&iommu->node);
	spin_lock_irqsave(&domain->iommus_lock, flags);
	list_add_tail(&iommu->node, &domain->iommus);
	spin_unlock_irqrestore(&domain->iommus_lock, flags);

	ret = pm_runtime_resume_and_get(iommu->dev);
	if (ret) {
		dev_err(iommu->dev, "failed to resume iommu\n");
		return ret;
	}

	/* sync map table to HW */
	spin_lock_irqsave(&domain->map_table_lock, flags);
	__update_iommu(iommu, domain->map_table);
	spin_unlock_irqrestore(&domain->map_table_lock, flags);
	__enable_iommu(iommu, true);
	ret = pm_runtime_put_sync_suspend(iommu->dev);
	if (ret) {
		dev_err(dev, "Failed to suspend %d\n", ret);
		return ret;
	}
	return 0;
}

static int lite_mmu_map(struct iommu_domain *iommu_domain, unsigned long _iova, phys_addr_t paddr,
			size_t size, int prot, gfp_t gfp)
{
	struct lite_mmu_domain *domain = to_lite_mmu_domain(iommu_domain);
	unsigned long flags;
	phys_addr_t start, end;
	u8 start_idx, end_idx;
	bool dirty = false;

	/* size does not over 256MB */
	WARN_ON(size > MAP_ENTRY_SIZE);

	spin_lock_irqsave(&domain->map_table_lock, flags);

	start	  = _iova;
	end	  = _iova + size - 1U;
	start_idx = __get_index_from_va(start);
	end_idx	  = __get_index_from_va(end);

	if (!domain->map_ref_cnt[start_idx]) {
		domain->map_table[start_idx] = __get_map_value(paddr);
		dirty			     = true;
	}
	domain->map_ref_cnt[start_idx]++;

	if (start_idx != end_idx) {
		if (!domain->map_ref_cnt[end_idx]) {
			domain->map_table[end_idx] = __get_map_value(paddr + size - 1U);
			dirty			   = true;
		}
		domain->map_ref_cnt[end_idx]++;
	}

	spin_unlock_irqrestore(&domain->map_table_lock, flags);
	pr_debug("%s: Mapping Phys:%#llx(%#lx), start:[%#x]-%#x, end:[%#x]-%#x\n",
			 __func__, paddr, size,
			 start_idx, domain->map_table[start_idx],
			 end_idx, domain->map_table[end_idx]);
	if (dirty)
		__flush_domain(domain);
	return 0;
}

static size_t lite_mmu_unmap(struct iommu_domain *iommu_domain, unsigned long _iova, size_t size,
			     struct iommu_iotlb_gather *gather)
{
	struct lite_mmu_domain *domain = to_lite_mmu_domain(iommu_domain);
	unsigned long flags;
	unsigned long start, end;
	u8 start_idx, end_idx;
	bool dirty = false;

	spin_lock_irqsave(&domain->map_table_lock, flags);

	start	  = _iova;
	end	  = _iova + size - 1U;
	start_idx = __get_index_from_va(start);
	end_idx	  = __get_index_from_va(end);

	if (domain->map_ref_cnt[start_idx] == 1) {
		domain->map_table[start_idx] = start_idx;
		dirty			     = true;
	}
	if (domain->map_ref_cnt[start_idx])
		domain->map_ref_cnt[start_idx]--;

	if (start_idx != end_idx) {
		if (domain->map_ref_cnt[end_idx] == 1) {
			domain->map_table[end_idx] = end_idx;
			dirty			   = true;
		}
		if (domain->map_ref_cnt[end_idx])
			domain->map_ref_cnt[end_idx]--;
	}

	spin_unlock_irqrestore(&domain->map_table_lock, flags);

	if (dirty)
		__flush_domain(domain);

	return size;
}

static int lite_mmu_def_domain_type(struct device *dev)
{
	return IOMMU_DOMAIN_UNMANAGED;
}

static struct iommu_device *lite_mmu_probe_device(struct device *dev)
{
	struct lite_mmu_iommu *iommu = dev_iommu_priv_get(dev);

	if (!iommu)
		return ERR_PTR(-ENODEV);

	iommu->link = device_link_add(dev, iommu->dev, DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);

	return &iommu->iommu;
}

static void lite_mmu_release_device(struct device *dev)
{
	struct lite_mmu_iommu *iommu = dev_iommu_priv_get(dev);

	device_link_del(iommu->link);
}

static phys_addr_t lite_mmu_iova_to_phys(struct iommu_domain *iommu_domain, dma_addr_t iova)
{
	struct lite_mmu_domain *domain = to_lite_mmu_domain(iommu_domain);
	phys_addr_t phys;
	unsigned long flags;
	u8 index;

	spin_lock_irqsave(&domain->map_table_lock, flags);

	index = __get_index_from_va(iova);
	phys  = ((phys_addr_t)domain->map_table[index]) << MAP_ADDR_SHIFT | iova;

	spin_unlock_irqrestore(&domain->map_table_lock, flags);

	return phys;
}

static struct iommu_group *lite_mmu_device_group(struct device *dev)
{
	struct lite_mmu_iommu *iommu = dev_iommu_priv_get(dev);

	return iommu_group_ref_get(iommu->group);
}

static int lite_mmu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	struct platform_device *iommu_dev = of_find_device_by_node(args->np);
	struct lite_mmu_iommu *iommu;

	if (!iommu_dev)
		return -ENODEV;

	iommu = platform_get_drvdata(iommu_dev);

	dev_iommu_priv_set(dev, (void *)iommu);

	return 0;
}

static const struct iommu_ops lite_mmu_ops = {
	.domain_alloc	    = lite_mmu_domain_alloc,
	.probe_device	    = lite_mmu_probe_device,
	.release_device	    = lite_mmu_release_device,
	.device_group	    = lite_mmu_device_group,
	.pgsize_bitmap	    = LITE_MMU_PGSIZE_BITMAP,
	.of_xlate	    = lite_mmu_of_xlate,
	.def_domain_type = lite_mmu_def_domain_type,
	.owner		    = THIS_MODULE,
	.default_domain_ops = &(const struct iommu_domain_ops){
		.attach_dev   = lite_mmu_attach_device,
		.detach_dev   = lite_mmu_detach_device,
		.map	      = lite_mmu_map,
		.unmap	      = lite_mmu_unmap,
		.iova_to_phys = lite_mmu_iova_to_phys,
		.free	      = lite_mmu_domain_free,
	}};

static int lite_mmu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lite_mmu_iommu *iommu;
	int ret = 0;

	iommu = devm_kzalloc(dev, sizeof(*iommu), GFP_KERNEL);
	if (!iommu)
		return -ENOMEM;

	platform_set_drvdata(pdev, iommu);
	iommu->dev = dev;

	iommu->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(iommu->base))
		return PTR_ERR(iommu->base);

	pm_runtime_get_noresume(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	iommu->clk = devm_clk_get_optional(dev, "mmu_clk");
	if (IS_ERR(iommu->clk)) {
		dev_err(dev, "failed to get clk source\n");
		return PTR_ERR(iommu->clk);
	}

	ret = clk_prepare_enable(iommu->clk);
	if (ret) {
		dev_err(dev, "Failed to enable clock %d\n", ret);
		goto err_pm_stop;
	}

	iommu->group = iommu_group_alloc();
	if (IS_ERR(iommu->group)) {
		ret = PTR_ERR(iommu->group);
		goto err_disable_clk;
	}

	ret = iommu_device_sysfs_add(&iommu->iommu, dev, NULL, dev_name(dev));
	if (ret)
		goto err_put_group;

	ret = iommu_device_register(&iommu->iommu, &lite_mmu_ops, dev);
	if (ret)
		goto err_remove_sysfs;

	ret = pm_runtime_put_sync_suspend(dev);
	if (ret) {
		dev_err(dev, "Failed to suspend %d\n", ret);
		goto err_remove_sysfs;
	}

	return 0;

err_remove_sysfs:
	iommu_device_sysfs_remove(&iommu->iommu);
err_put_group:
	iommu_group_put(iommu->group);
err_disable_clk:
	clk_disable_unprepare(iommu->clk);

err_pm_stop:
	pm_runtime_disable(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_put_noidle(dev);

	return ret;
}

static int __maybe_unused lite_mmu_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused lite_mmu_resume(struct device *dev)
{
	lite_mmu_restore_mapping(dev);

	return 0;
}

static int __maybe_unused litemmu_pm_runtime_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused litemmu_pm_runtime_resume(struct device *dev)
{
	return lite_mmu_resume(dev);
}

static const struct dev_pm_ops lite_mmu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lite_mmu_suspend, lite_mmu_resume)
		SET_RUNTIME_PM_OPS(litemmu_pm_runtime_suspend, litemmu_pm_runtime_resume, NULL)};

static const struct of_device_id lite_mmu_dt_ids[] = {
	{ .compatible = "d-robotics,lite_mmu" },
	{ /* sentinel */ }
};

static struct platform_driver lite_mmu_driver = {
	.probe = lite_mmu_probe,
	.driver = {
		.name = "lite_mmu",
		.of_match_table = lite_mmu_dt_ids,
		.pm = &lite_mmu_pm_ops,
	},
};

static int __init lite_mmu_init(void)
{
	return platform_driver_register(&lite_mmu_driver);
}
subsys_initcall(lite_mmu_init);
