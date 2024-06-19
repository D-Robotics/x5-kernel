// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef __PMU_H
#define __PMU_H

#include <linux/kernel.h>
#include <linux/pm_domain.h>

#define DSP_PCU_CTL			0x580
#define HIFI5_PCU_CTL			0x600
#define VIDEO_PCU_CTL			0x680
#define BPU_PCU_CTL			0x700
#define GPU_PCU_CTL			0x780

#define PCU_STATUS_OFFSET		0x18

#define MEM_REPAIR_RP			0x1020

#define NOC_IDLE_REQ			0x2000
#define NOC_IDLE_STATUS			0x2008
#define NOC_IDLEACK_TIMEOUT		0x201C
#define NOC_IDLE_TIMEOUT		0x2024
#define NOC_IDLEACK_STATUS		0x202C

#define APB_LPI_QREQ			0X3000
#define APB_LPI_QACCEPT			0x3004
#define APB_LPI_QDENY			0x3008
#define APB_LPI_QACTIVE			0x300C

#define HIFI5_NOC_MASK			(GENMASK(7, 6))
#define VIDEO_NOC_MASK			(GENMASK(23, 22))
#define BPU_NOC_MASK			(GENMASK(5, 3))
#define GPU_NOC_MASK			(GENMASK(1, 0))
#define GPU_QREQ_MASK			(GENMASK(8, 3))
#define BPU_QREQ_MASK			(GENMASK(29, 24))

#define DOMAIN(_name, o, n, a, f)	\
	{				\
		.name = _name,		\
		.offset = o,		\
		.noc_id = n,		\
		.apb_id = a,		\
		.flag = f,		\
 	}

struct drobot_pmu_info {
	int num_domains;
	const struct drobot_pm_info *domain_info;
};

struct drobot_pm_domain {
	bool defer_poweron;
	struct generic_pm_domain genpd;
	const struct drobot_pm_info *info;
	struct drobot_pmu *pmu;
	struct regulator *regulator;
	struct clk_bulk_data *clks;
	int num_clks;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
#endif
};

struct drobot_pmu {
	u32 flag;
	struct device *dev;
	void __iomem *base;
	struct device *idle;
	const struct drobot_pmu_info *info;
	struct genpd_onecell_data genpd_data;
	struct generic_pm_domain *domains[];
};

struct drobot_pm_info {
	const char *name;
	u32 offset;
	u32 flag;
	u8 noc_id;
	u8 apb_id;
};

#endif
