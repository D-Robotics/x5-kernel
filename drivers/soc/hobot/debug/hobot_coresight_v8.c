/*
 * Horizon Robotics
 *
 *  Author:	Neil Zhang <zhangwm@marvell.com>
 *  Copyright 	(C) 2020 Horizon Robotics Inc.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/percpu.h>
#include <linux/of_address.h>

#define EDITR           0x84
#define EDSCR           0x88
#define EDRCR           0x90
#define EDPCSRlo        0xA0
#define EDPCSRhi        0xAC
#define EDPRSR          0x314
#define EDLAR           0xFB0
#define EDCIDR          0xFF0
#define CTIDEVTYPE      0xFCC
#define CTI_CTRL        0x0
#define CTI_INTACK      0x10
#define CTI_IN0EN       0x20
#define CTI_APP_PULSE   0x1c
#define CTI_OUT0EN      0xA0
#define CTI_OUT1EN      0xA4
#define CTI_GATE        0x140
#define CTI_LOCK        0xfb0
#define CTI_DEVID       0xfc8
#define DBG_REG(cpu, offset)    (debug_base[cpu] + offset)
#define CTI_REG(cpu, offset)    (cti_base[cpu] + offset)

static void __iomem *debug_base[NR_CPUS];
static void __iomem *cti_base[NR_CPUS];

void arch_enable_access(u32 cpu)
{
	writel(0xC5ACCE55, DBG_REG(cpu, EDLAR));
}

void arch_dump_pcsr(u32 cpu)
{
	u32 pcsrhi, pcsrlo;
	u64 pcsr;
	int i;

	pr_emerg("=========== dump PCSR for cpu%d ===========\n", cpu);
	for (i = 0; i < 8; i++) {
		pcsrlo = readl_relaxed(DBG_REG(cpu, EDPCSRlo));
		pcsrhi = readl_relaxed(DBG_REG(cpu, EDPCSRhi));
		pcsr = pcsrhi;
		pcsr = (pcsr << 32) | pcsrlo;
		pr_emerg("PCSR of cpu%d is 0x%llx\n", cpu, pcsr);
		udelay(20);
	}
}

static inline void cti_enable_access(u32 cpu)
{
	writel(0xC5ACCE55, CTI_REG(cpu, CTI_LOCK));
}

int arch_halt_cpu(u32 cpu)
{
	u32 timeout, val;

	/* Enable Halt Debug mode */
	val = readl(DBG_REG(cpu, EDSCR));
	val |= (0x1 << 14);
	writel(val, DBG_REG(cpu, EDSCR));

	/* Enable CTI access */
	cti_enable_access(cpu);

	/* Enable CTI */
	writel(0x1, CTI_REG(cpu, CTI_CTRL));

	/* Set output channel0 */
	val = readl(CTI_REG(cpu, CTI_OUT0EN)) | 0x1;
	writel(val, CTI_REG(cpu, CTI_OUT0EN));

	/* Trigger pulse event */
	writel(0x1, CTI_REG(cpu, CTI_APP_PULSE));

	/* Wait the cpu halted */
	timeout = 10000;

	do {
		val = readl(DBG_REG(cpu, EDPRSR));
		if (val & (0x1 << 4))
			break;
	} while (--timeout);

	if (!timeout)
		return -1;

	return 0;
}

void arch_insert_inst(u32 cpu)
{
	u32 timeout, val;

	/* msr dlr_el0, xzr */
	writel(0xD51B453F, DBG_REG(cpu, EDITR));

	/* Wait until the ITR become empty. */
	timeout = 10000;
	do {
		val = readl(DBG_REG(cpu, EDSCR));
		if (val & (0x1 << 24))
			break;
	} while (--timeout);
	if (!timeout)
		pr_emerg("Cannot execute instructions on cpu%d\n", cpu);

	if (val & (0x1 << 6))
		pr_emerg("Occurred exception in debug state on cpu%d\n", cpu);
}

void arch_restart_cpu(u32 cpu)
{
	u32 timeout, val;

	/* Disable Halt Debug Mode */
	val = readl(DBG_REG(cpu, EDSCR));
	val &= ~(0x1 << 14);
	writel(val, DBG_REG(cpu, EDSCR));

	/* Enable CTI access */
	cti_enable_access(cpu);

	/* Enable CTI */
	writel(0x1, CTI_REG(cpu, CTI_CTRL));

	/* ACK the outut event */
	writel(0x1, CTI_REG(cpu, CTI_INTACK));

	/* Set output channel1 */
	val = readl(CTI_REG(cpu, CTI_OUT1EN)) | 0x2;
	writel(val, CTI_REG(cpu, CTI_OUT1EN));

	/* Trigger pulse event */
	writel(0x2, CTI_REG(cpu, CTI_APP_PULSE));

	/* Wait the cpu become running */
	timeout = 10000;
	do {
		val = readl(DBG_REG(cpu, EDPRSR));
		if (!(val & (0x1 << 4)))
			break;
	} while (--timeout);

	if (!timeout)
		pr_emerg("Cannot restart cpu%d\n", cpu);
}

static int __init coresight_parse_dbg_dt(void)
{
	int i;
	char name[64];
	struct device_node *node;

	for (i = 0; i < NR_CPUS; i++) {
		snprintf(name, sizeof(name), "hobot,coresight-dbg%d", i);
		node = of_find_compatible_node(NULL, NULL, name);
		if (!node) {
			pr_err("Failed to find DBG %d node!\n", i);
			return -ENODEV;
		}
		debug_base[i] = of_iomap(node, 0);
		if (!debug_base[i]) {
			pr_err("Failed to map coresight debug %d register\n", i);
			return -ENOMEM;
		}
		snprintf(name, sizeof(name), "hobot,coresight-cti%d", i);
		node = of_find_compatible_node(NULL, NULL, name);
		if (!node) {
			pr_err("Failed to find CTI %d node!\n", i);
			return -ENODEV;
		}
		cti_base[i] = of_iomap(node, 0);
		if (!cti_base[i]) {
			pr_err("Failed to map coresight cti %d register\n", i);
			return -ENOMEM;
		}
	}

	return 0;
}

void __init arch_coresight_init(void)
{
	coresight_parse_dbg_dt();
	return;
}
