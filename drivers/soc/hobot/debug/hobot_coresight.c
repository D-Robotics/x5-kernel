/*
 * Horizon Robotics
 *
 *  Author:     Neil Zhang <zhangwm@marvell.com>
 *  Copyright 	(C) 2020 Horizon Robotics Inc.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/of.h>
#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/cpu_pm.h>
#include <linux/platform_device.h>
#include <soc/hobot/hobot_coresight.h>

void coresight_dump_pcsr(u32 cpu)
{
	arch_enable_access(cpu);
	arch_dump_pcsr(cpu);
}

void coresight_trigger_panic(int cpu)
{
	arch_enable_access(cpu);
	arch_dump_pcsr(cpu);

	/* Halt the dest cpu */
	pr_emerg("Going to halt cpu%d\n", cpu);
	if (arch_halt_cpu(cpu)) {
		pr_emerg("Cannot halt cpu%d\n", cpu);
		return;
	}
	pr_emerg("Going to insert inst on cpu%d\n", cpu);
	arch_insert_inst(cpu);

	panic_on_oops = 1;	/* force panic */

	/* Restart target cpu */
	pr_emerg("Going to restart cpu%d\n", cpu);
	arch_restart_cpu(cpu);
}

static int arm_coresight_probe(struct platform_device *pdev)
{
	arch_coresight_init();
	return 0;
}

static struct of_device_id arm_coresight_dt_ids[] = {
	{ .compatible = "hobot,coresight", },
	{}
};

static struct platform_driver arm_coresight_driver = {
	.probe		= arm_coresight_probe,
	.driver		= {
		.name	= "arm-coresight",
		.of_match_table = of_match_ptr(arm_coresight_dt_ids),
	},
};

static int __init arm_coresight_init(void)
{
	return platform_driver_register(&arm_coresight_driver);
}

arch_initcall(arm_coresight_init);
