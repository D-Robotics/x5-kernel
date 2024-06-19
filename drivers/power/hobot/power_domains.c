// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/err.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <dt-bindings/power/drobot-x5-power.h>
#include "pmu.h"
#include "idle.h"

#define to_drobot_pd(genpd) container_of(genpd, struct drobot_pm_domain, genpd)

#define PCU_ACK_DELAY			1000
#define PCU_ACK_TIMEOUT			3000
#define PWRDN_REQ_MASK			(BIT(5) | BIT(0))
#define PWRUP_REQ_MASK			BIT(4)
#define PWRDN_STATUS_MASK		BIT(1)
#define PWRUP_STATUS_MASK		BIT(0)
#define MEM_REPAIR_GO			BIT(19)
#define ALL_MEM_REPAIR_DONE		BIT(18)
#define POWER_UP_CNT_PRE_SCALER		20
#define PMU_PCU_PDNTIM			0x4
#define PMU_PCU_PUPTIM			0x8
#define PMU_PCU_CNT_SCA_TIM		0xc
#define PMU_PCU_CKTIN			0x10
#define PMU_PCU_DONETIN			0x14
#define PMU_PCU_STATUS			0x18
#define MEM_REPAIR_RP_REG		0x1020
#define DOMAIN_SIZE			0x80
#define SUSPEND_IN_PROGRESS		BIT(0)

#ifdef CONFIG_DEBUG_FS
#define PD_DIR_NAME "drobot_pd"

static struct dentry *drobot_pd_root;
#endif

static inline void pcu_set_bits(void __iomem *reg, u32 mask)
{
	writel(mask, reg);
}

static int pcu_set_domain(struct drobot_pm_domain *pd, bool on)
{
	void __iomem *reg = pd->pmu->base + pd->info->offset;
	u32 val, request, status;
	int ret;
 
 	if (on) {
		request = PWRUP_REQ_MASK;
		status	= PWRUP_STATUS_MASK;

 	} else {
		request = PWRDN_REQ_MASK;
		status	= PWRDN_STATUS_MASK;
 	}

	/* clear status */
	pcu_set_bits(reg + PMU_PCU_STATUS, status);

	/* set request */
	pcu_set_bits(reg, request);

	ret = readl_poll_timeout_atomic(reg + PMU_PCU_STATUS, val, (val & status), PCU_ACK_DELAY,
					PCU_ACK_TIMEOUT);

	if (ret) {
		pr_err("%s (%d): timeout to wait pcu status to 0x%x\n", __func__, ret, status);
		return ret;
	}

	return ret;
}

static inline int __connect_noc(struct drobot_pm_domain *pd)
{
	if (pd->info->noc_id != 0xff)
		return drobot_idle_request(pd->pmu->idle, pd->info->noc_id, false, false);
	return 0;
}

static inline int __disconnect_noc(struct drobot_pm_domain *pd)
{
	if (pd->info->noc_id != 0xff)
		return drobot_idle_request(pd->pmu->idle, pd->info->noc_id, true, false);

	return 0;
}

static inline int __connect_apb(struct drobot_pm_domain *pd)
{
	if (pd->info->apb_id != 0xff)
		return drobot_idle_request(pd->pmu->idle, pd->info->apb_id, false, false);

	return 0;
}

static inline int __disconnect_apb(struct drobot_pm_domain *pd)
{
	if (pd->info->apb_id != 0xff)
		return drobot_idle_request(pd->pmu->idle, pd->info->apb_id, true, false);

	return 0;
}

static int __memory_repair_status_check(struct drobot_pm_domain *pd, bool on)
{
	int ret;
	u32 val;
	u32 offset = pd->info->offset / DOMAIN_SIZE + 1;
	u32 mask = on ? (MEM_REPAIR_GO | ALL_MEM_REPAIR_DONE | BIT(offset)) : BIT(offset);
	u32 target = on ? mask : 0;

	if (on)
		mask = mask |MEM_REPAIR_GO | ALL_MEM_REPAIR_DONE;

	ret = readl_poll_timeout_atomic(pd->pmu->base + MEM_REPAIR_RP_REG, val, (val & mask) == target,
					PCU_ACK_DELAY, PCU_ACK_TIMEOUT);

	if (ret) {
		pr_err("%s (%d): timeout to wait memory repair status %x %x to 0x%x\n", __func__, ret, val, mask, target);
		return ret;
	}

	return ret;
}

static void __timing_configuration(struct drobot_pm_domain *pd)
{
	u32 val;
	void __iomem *reg = pd->pmu->base + pd->info->offset;

	writel(0x4 << POWER_UP_CNT_PRE_SCALER, reg + PMU_PCU_CNT_SCA_TIM);

	val = readl(reg + PMU_PCU_PUPTIM);
	val = val | 0x19000000;
	writel(val, reg + PMU_PCU_PUPTIM);

	val = readl(reg + PMU_PCU_CKTIN);
	val = val | 0x1919;
	writel(val, reg + PMU_PCU_CKTIN);

	val = readl(reg + PMU_PCU_DONETIN);
	val = val | 0x36;
	writel(val, reg + PMU_PCU_DONETIN);
}

static int __pcu_on(struct drobot_pm_domain *pd)
{
	int ret;

    __timing_configuration(pd);

	ret = pcu_set_domain(pd, true);
	if (ret)
		return ret;

	ret = __connect_apb(pd);
	if (ret)
		goto err_connect_apb;
 
	ret = __connect_noc(pd);
	if (ret)
		goto err_connect_noc;

	ret = __memory_repair_status_check(pd, true);
	if (ret)
		goto err_connect_noc;

	return 0;

err_connect_noc:
	__disconnect_apb(pd);

err_connect_apb:
	pcu_set_domain(pd, false);

	return ret;
}

static int __pd_power_on(struct drobot_pm_domain *pd)
{
	int ret;

	if (pd->regulator) {
		if (pd->pmu->flag & SUSPEND_IN_PROGRESS) {
			pd->defer_poweron = true;
			return 0;
		} else {
			ret = regulator_enable(pd->regulator);
			if (ret)
				return ret;
		}
	}

	ret = __pcu_on(pd);
	if (ret)
		goto err_pcu;

	return 0;

err_pcu:
	if (pd->regulator)
		regulator_disable(pd->regulator);

	return ret;
}

static int __pcu_off(struct drobot_pm_domain *pd)
{
	int ret;
 
	ret = __disconnect_noc(pd);
	if (ret)
		return ret;

	ret = __disconnect_apb(pd);
	if (ret)
		goto err_disconnect_apb;

	ret = pcu_set_domain(pd, false);
	if (ret)
		goto err_pcu;

	ret = __memory_repair_status_check(pd, false);
	if (ret)
		goto err_mem;
 
 	return 0;

err_mem:
	pcu_set_domain(pd, true);

err_pcu:
	__connect_apb(pd);

err_disconnect_apb:
	__connect_noc(pd);

	return ret;
}

static int __pd_power_off(struct drobot_pm_domain *pd)
{
	int ret;

	ret = __pcu_off(pd);
	if (ret)
		return ret;

	if (pd->regulator) {
		if (pd->pmu->flag & SUSPEND_IN_PROGRESS) {
			return 0;
		} else {
			ret = regulator_disable(pd->regulator);
			if (ret)
				goto err_regulator;
		}

	}

	return 0;

err_regulator:
	__pcu_on(pd);

	return ret;
}

static int drobot_pd_power_on(struct generic_pm_domain *genpd)
{
	struct drobot_pm_domain *pd = to_drobot_pd(genpd);
	pr_debug("%s power on\n", pd->info->name);

	return __pd_power_on(pd);
}

static int drobot_pd_power_off(struct generic_pm_domain *genpd)
{
	struct drobot_pm_domain *pd = to_drobot_pd(genpd);
	pr_debug("%s power off\n", pd->info->name);

	return __pd_power_off(pd);
}

#ifdef CONFIG_DEBUG_FS
static int drobot_pd_debugfs_set(void *data, u64 val)
{
	struct generic_pm_domain *genpd = data;
	struct drobot_pm_domain *pd		= to_drobot_pd(genpd);
	int ret;

	if (val)
		ret = __pd_power_on(pd);
	else
		ret = __pd_power_off(pd);
	if (!ret)
		genpd->status = val ? GENPD_STATE_ON : GENPD_STATE_OFF;

	return ret;
}

static int drobot_pd_debugfs_get(void *data, u64 *val)
{
	struct generic_pm_domain *genpd = data;

	*val = !genpd->status;

	return 0;
}
 
DEFINE_DEBUGFS_ATTRIBUTE(drobot_pd_debugfs_fops, drobot_pd_debugfs_get, drobot_pd_debugfs_set, "%llu\n");
#endif

static int drobot_pm_add_one_domain(struct drobot_pmu *pmu, struct device_node *node)
{
	const struct drobot_pm_info *pd_info;
	struct drobot_pm_domain *pd;
	u32 id;
	bool boot_on;
	int ret;

	ret = of_property_read_u32(node, "reg", &id);
	if (ret) {
		dev_err(pmu->dev, "%pOFn: failed to retrieve domain id (reg): %d\n", node, ret);
		return -EINVAL;
	}

	if (id >= pmu->info->num_domains) {
		dev_err(pmu->dev, "%pOFn: invalid domain id %d\n", node, id);
		return -EINVAL;
	}

	boot_on = of_property_read_bool(node, "boot-on");

	pd_info = &pmu->info->domain_info[id];

	pd = devm_kzalloc(pmu->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;

	pd->info = pd_info;
	pd->pmu = pmu;

	if (strncmp(pd_info->name, "bpu", strlen("bpu") + 1) == 0 ||
		strncmp(pd_info->name, "gpu", strlen("gpu") + 1) == 0) {
		pd->regulator = devm_regulator_get_optional(pmu->dev, pd_info->name);
		if (IS_ERR(pd->regulator)) {
			if (PTR_ERR(pd->regulator) == -EPROBE_DEFER)
				return -EPROBE_DEFER;
			dev_dbg(pmu->dev, "Failed to get domain's regulator\n");
			pd->regulator = NULL;
		}
	} else {
		pd->regulator = NULL;
	}


	if (boot_on && pd->regulator) {
		ret = regulator_enable(pd->regulator);
		if (ret)
			return ret;
	}

	pd->genpd.name	    = pd_info->name;
	pd->genpd.power_off = drobot_pd_power_off;
	pd->genpd.power_on = drobot_pd_power_on;
	pd->genpd.flags	    = pd_info->flag;
	ret		    = pm_genpd_init(&pd->genpd, NULL, !boot_on);

	pmu->genpd_data.domains[id] = &pd->genpd;

#ifdef CONFIG_DEBUG_FS
	pd->dentry = debugfs_create_file(pd_info->name, 0644, drobot_pd_root, &pd->genpd,
					 &drobot_pd_debugfs_fops);
#endif

	return 0;
}

static void drobot_pm_remove_one_domain(struct drobot_pm_domain *pd)
{
	int ret;

	ret = pm_genpd_remove(&pd->genpd);
	if (ret < 0)
		dev_err(pd->pmu->dev, "failed to remove domain '%s' : %d - state may be inconsistent\n",
			pd->genpd.name, ret);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove(pd->dentry);
#endif
}

static void drobot_pm_domain_cleanup(struct drobot_pmu *pmu)
{
	struct generic_pm_domain *genpd;
	struct drobot_pm_domain *pd;
	int i;

	for (i = pmu->genpd_data.num_domains - 1; i >= 0; i--) {
		genpd = pmu->genpd_data.domains[i];
		if (genpd) {
			pd = to_drobot_pd(genpd);
			drobot_pm_remove_one_domain(pd);
			pmu->genpd_data.domains[i] = NULL;
		}
	}
}

static int drobot_pm_add_subdomain(struct drobot_pmu *pmu, struct device_node *parent)
{
	struct device_node *np;
	struct generic_pm_domain *child_domain, *parent_domain;
	int ret;

	for_each_child_of_node(parent, np) {
		u32 idx;

		ret = of_property_read_u32(parent, "reg", &idx);
		if (ret) {
			dev_err(pmu->dev, "%pOFn: failed to retrieve domain id (reg): %d\n", parent,
				ret);
			goto err_out;
		}
		parent_domain = pmu->genpd_data.domains[idx];

		ret = drobot_pm_add_one_domain(pmu, np);
		if (ret) {
			dev_err(pmu->dev, "failed to handle node %pOFn: %d\n", np, ret);
			goto err_out;
		}

		ret = of_property_read_u32(np, "reg", &idx);
		if (ret) {
			dev_err(pmu->dev, "%pOFn: failed to retrieve domain id (reg): %d\n", np,
				ret);
			goto err_out;
		}
		child_domain = pmu->genpd_data.domains[idx];

		ret = pm_genpd_add_subdomain(parent_domain, child_domain);
		if (ret) {
			dev_err(pmu->dev, "%s failed to add subdomain %s: %d\n",
				parent_domain->name, child_domain->name, ret);
			goto err_out;
		} else {
			dev_dbg(pmu->dev, "%s add subdomain: %s\n",
				parent_domain->name, child_domain->name);
		}

		drobot_pm_add_subdomain(pmu, np);
	}

	return 0;

err_out:
	of_node_put(np);
	return ret;
}

static int drobot_pm_domain_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *node;
	struct drobot_pmu *pmu;
	const struct drobot_pmu_info *pmu_info;
	void __iomem *base;
	int ret;

	if (!np) {
		dev_err(dev, "device tree node not found\n");
		return -ENODEV;
	}

#ifdef CONFIG_DEBUG_FS
	if (!drobot_pd_root)
		drobot_pd_root = debugfs_create_dir(PD_DIR_NAME, NULL);
#endif

	base = devm_platform_ioremap_resource(pdev, 0);
	if (!base) {
		pr_err("%s: could not map pcu region\n", __func__);
		return -EINVAL;
	}

	pmu_info = of_device_get_match_data(dev);
	if (!pmu_info)
		return -EINVAL;

	pmu = devm_kzalloc(dev, struct_size(pmu, domains, pmu_info->num_domains), GFP_KERNEL);
	if (!pmu)
		return -ENOMEM;

	pmu->dev = &pdev->dev;

	pmu->info = pmu_info;
	pmu->base = base;
	pmu->flag = 0;

	pmu->genpd_data.domains	    = pmu->domains;
	pmu->genpd_data.num_domains = pmu_info->num_domains;

	pmu->idle = drobot_idle_get_dev(dev->of_node);
	if (IS_ERR(pmu->idle)) {
		pr_err("%s(%ld): could not get idle dev\n", __func__, PTR_ERR(pmu->idle));
		return PTR_ERR(pmu->idle);
	}

	ret = -ENODEV;

	for_each_available_child_of_node (np, node) {
		ret = drobot_pm_add_one_domain(pmu, node);
		if (ret) {
			if (ret == -EPROBE_DEFER)
				dev_dbg(dev, "need retry to handle node %pOFn: %d, \n", node, ret);
			else
				dev_err(dev, "failed to handle node %pOFn: %d\n", node, ret);
			of_node_put(node);
			goto err_out;
		}

		ret = drobot_pm_add_subdomain(pmu, node);
		if (ret < 0) {
			dev_err(dev, "failed to handle subdomain node %pOFn: %d\n", node, ret);
			of_node_put(node);
			goto err_out;
		}
	}

	if (ret) {
		dev_dbg(dev, "no power domains defined\n");
		goto err_out;
	}

	ret = of_genpd_add_provider_onecell(np, &pmu->genpd_data);
	if (ret) {
		dev_err(dev, "failed to add provider: %d\n", ret);
		goto err_out;
	}
	dev_set_drvdata(dev, pmu);

	return 0;

err_out:
	drobot_pm_domain_cleanup(pmu);
	return ret;
}

#ifdef CONFIG_PM_SLEEP
static __maybe_unused int pd_suspend(struct device *dev)
{
	struct drobot_pmu *pmu;

	pmu = dev_get_drvdata(dev);
	pmu->flag |= SUSPEND_IN_PROGRESS;

	return 0;
}

static __maybe_unused int pd_resume(struct device *dev)
{
	struct drobot_pmu *pmu;
	int i;

	pmu = dev_get_drvdata(dev);

	if (pmu->flag & SUSPEND_IN_PROGRESS)
		pmu->flag &= (~SUSPEND_IN_PROGRESS);
	else
		return 0;

	for (i = 0; i < pmu->info->num_domains; i++) {
		struct generic_pm_domain *genpd = pmu->genpd_data.domains[i];
		struct drobot_pm_domain *pd = to_drobot_pd(genpd);

		if (pd->defer_poweron == true) {
			__pd_power_on(pd);
			pd->defer_poweron = false;
		}
	}

	return 0;
}
#endif

static const struct dev_pm_ops pd_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pd_suspend, pd_resume)
};

static const struct drobot_pm_info x5_pm_domains[] = {
	[X5_DSP]       = DOMAIN("dsp", 0x580, ISO_PD_DSP_TOP, 0xff, GENPD_FLAG_ALWAYS_ON),
	[X5_DSP_HIFI5] = DOMAIN("hifi5", 0x600, ISO_PD_DSP_HIFI5, 0xff, GENPD_FLAG_ACTIVE_WAKEUP),
	[X5_VIDEO]     = DOMAIN("video", 0x680, ISO_PD_VIDEO, ISO_Q_VIDEO, 0),
	[X5_BPU]       = DOMAIN("bpu", 0x700, ISO_PD_BPU, ISO_Q_BPU, 0),
	[X5_GPU]       = DOMAIN("gpu", 0x780, ISO_PD_GPU, ISO_Q_GPU, 0),
	[X5_ISP]       = DOMAIN("isp", 0x800, ISO_CG_ISP, 0xff, 0),
};

static const struct drobot_pmu_info x5_pmu = {
	.num_domains = ARRAY_SIZE(x5_pm_domains),
	.domain_info = x5_pm_domains,
};

static const struct of_device_id drobot_pm_domain_dt_match[] = {
	{
		.compatible = "horizon,x5-power-controller",
		.data = (void *)&x5_pmu,
	},
	{ /* sentinel */ },
};

static struct platform_driver drobot_pm_domain_driver = {
	.probe = drobot_pm_domain_probe,
	.driver = {
		.name   = "dr-power-domain",
		.of_match_table = drobot_pm_domain_dt_match,
		.suppress_bind_attrs = true,
		.pm = &pd_pm_ops,
	},
};

static int __init drobot_pm_domain_drv_register(void)
{
	return platform_driver_register(&drobot_pm_domain_driver);
}
postcore_initcall(drobot_pm_domain_drv_register);
