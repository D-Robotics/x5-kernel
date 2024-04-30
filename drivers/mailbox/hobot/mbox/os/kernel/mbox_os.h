/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

#ifndef MBOX_OS__H
#define MBOX_OS__H

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <asm/io.h>
#include <linux/io.h>
#include <linux/mailbox_controller.h>

#include "mbox_platform.h"

#ifndef BIT
#define BIT(n)				(1<<n)
#endif /*BIT*/

/* convenience wrappers for printing errors and debug messages */
#define mbox_fmt(fmt) "mbox-drv: %s() [%d]: "fmt
#define mbox_err(fmt, ...)	pr_err("mbox-drv: %s()[%d] error: "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define mbox_dbg(fmt, ...)	pr_debug("mbox-drv: %s()[%d] debug: "fmt, __func__, __LINE__, ##__VA_ARGS__)

static inline uint32_t os_read_register(uint8_t *addr)
{
	uint32_t val;
	val = readl((void __iomem *)addr);
	// pr_err("%s read 0x%llx val is 0x%x\n", __FUNCTION__, addr, val);
	return val;
}

static inline void os_write_register(uint8_t *addr, uint32_t val)
{
	// pr_err("%s write 0x%x to 0x%llx\n", __FUNCTION__, val, addr);
	writel(val, (void __iomem *)addr);
}

#endif /*MBOX_OS__H*/
