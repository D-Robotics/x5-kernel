// SPDX-License-Identifier: GPL-2.0+
/*
 * rewrite Horizon Sunrise 5 AXI Monitor Driver
 *
 * Copyright (C) 2023 Beijing Horizon Robotics Co.,Ltd.
 * Dinggao Pan <dinggao.pan@horizon.cc>
 *
 */
#ifndef _HORIZON_AXI_MONITOR_H
#define _HORIZON_AXI_MONITOR_H

#include <linux/module.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/timer.h>

#define AXI_DATA_WIDTH 128
#define SESSION_TIME_MAX_CNT ((uint64_t)(GENMASK(35, 0)))
#define SESSION_TIME_HIGH_MASK (GENMASK(3, 0))

/* Register Offsets */
/* Common Regiters */
#define START				0x4 /*< RW: Start AXI Monitor measure */
#define TIME_DAT_LOW		0x8 /*< RW: Lower 32 bits for measure window */
#define TIME_DAT_HIGH		0xC /*< RW: Higher 4 bits for measure window */
#define DONE				0x10 /*< RO: Measure window reached */
#define INT_STATUS			0x14 /*< RW: AXI Monitor IRQ status */
#define INT_EN				0x18 /*< RW: AXI Monitor IRQ enable  */
#define PORT_STATUS1		0x1C /*< RO: AXI Port timeout status */
#define PORT_STATUS2		0x20 /*< RO: AXI Port Error and done status */
#define BUS_CMD_TO			0x24 /*< RW: AXI Bus CMD timeout cycles */

/* Port Registers */
#define PORT_REG_OFFSET(port_n) ((port_n + 1) * 0x70) /*< Calculates offset for port_n */
#if IS_ENABLED(CONFIG_HOBOT_AXI_ADVANCED)
/** Control type of read events measured */
#define PORT_RD_EVENT_CTRL(port_n) (PORT_REG_OFFSET(port_n) + 0x0)
/** Control type of write events measured */
#define PORT_WR_EVENT_CTRL(port_n) (PORT_REG_OFFSET(port_n) + 0x4)
/** Control ID/User filter */
#define PORT_IDUSER_FILTER_CTRL(port_n) (PORT_REG_OFFSET(port_n) + 0x8)
/** Read transaction IDUSR to be filtered */
#define PORT_RD_IDUSER_FILTER(port_n) (PORT_REG_OFFSET(port_n) + 0xC)
/** Write transaction IDUSR to be filtered */
#define PORT_WR_IDUSER_FILTER(port_n) (PORT_REG_OFFSET(port_n) + 0x10)
#endif
/** Enable events */
#define PORT_EVENT_EN(port_n) (PORT_REG_OFFSET(port_n) + 0x14)
/** Reset statistics, used before any measurement starts */
#define PORT_RESET(port_n) (PORT_REG_OFFSET(port_n) + 0x18)

/* Port Statistics */
#define PORT_RD_BW(port_n) (PORT_REG_OFFSET(port_n) + 0x1C) /*< Read bandwidth */
#define PORT_WR_BW(port_n) (PORT_REG_OFFSET(port_n) + 0x20) /*< Write bandwidth */
/** Total read commands */
#define PORT_RD_CMD_NUM(port_n) (PORT_REG_OFFSET(port_n) + 0x24)
/** Total write commands */
#define PORT_WR_CMD_NUM(port_n) (PORT_REG_OFFSET(port_n) + 0x28)
/** Maximum read latency recorded */
#define PORT_RD_MAX_LATENCY(port_n) (PORT_REG_OFFSET(port_n) + 0x2C)
/** Minimum read latency recorded */
#define PORT_RD_MIN_LATENCY(port_n) (PORT_REG_OFFSET(port_n) + 0x30)
/** Maximum write latency recorded */
#define PORT_WR_MAX_LATENCY(port_n) (PORT_REG_OFFSET(port_n) + 0x34)
/** Minimum write latency recorded */
#define PORT_WR_MIN_LATENCY(port_n) (PORT_REG_OFFSET(port_n) + 0x38)
/** Total time spent on read transactions, unit: cycles */
#define PORT_RD_TIME_TOTAL(port_n) (PORT_REG_OFFSET(port_n) + 0x3C)
/** Total time spent on write transactions, unit: cycles */
#define PORT_WR_TIME_TOTAL(port_n) (PORT_REG_OFFSET(port_n) + 0x40)
/** Maximum outstanding counts recorded */
#define PORT_MAX_OST(port_n) (PORT_REG_OFFSET(port_n) + 0x44)
/** Outstanding counts at the end of measurement */
#define PORT_END_OST(port_n) (PORT_REG_OFFSET(port_n) + 0x48)
#if IS_ENABLED(CONFIG_HOBOT_AXI_ADVANCED)
/** Total write channel idle time */
#define PORT_WR_IDLE(port_n) (PORT_REG_OFFSET(port_n) + 0x4C)
/** Total write channel slave not ready when cmd ready time */
#define PORT_WR_CMD_SLV_BUSY(port_n) (PORT_REG_OFFSET(port_n) + 0x50)
/** Total write channel salve not ready when data ready time */
#define PORT_WR_DAT_SLV_BUSY(port_n) (PORT_REG_OFFSET(port_n) + 0x54)
/** Total write channel slave ready data not ready time */
#define PORT_WR_DAT_INVL(port_n) (PORT_REG_OFFSET(port_n) + 0x58)
/** Total read channel idle time */
#define PORT_RD_IDLE(port_n) (PORT_REG_OFFSET(port_n) + 0x5C)
/** Total read channel slave not ready when cmd ready time */
#define PORT_RD_CMD_SLV_BUSY(port_n) (PORT_REG_OFFSET(port_n) + 0x60)
/** Total read channel master not ready when data ready time */
#define PORT_RD_DAT_MST_BUSY(port_n) (PORT_REG_OFFSET(port_n) + 0x64)
/** Total read channel slave ready data not ready time */
#define PORT_RD_DAT_INVL(port_n) (PORT_REG_OFFSET(port_n) + 0x68)
#endif
#define PORT_BUS_STATUS(port_n) (PORT_REG_OFFSET(port_n) + 0x6C)

/* Interrupt bits */
#define AXI_CMD_TIMEOUT BIT(3)
#define AXI_WR_RESP_ERR BIT(2)
#define REG_OK BIT(1)
#define CNT_OVERFLOW BIT(0)
#define INT_ALL GENMASK(3,2) | BIT(0)

/* Event enable bits */
#define RD_BW_EN BIT(9)
#define RD_CMD_CNT_EN BIT(8)
#define RD_LATENCY_EN BIT(7)
#define RD_TOTAL_CYCLE_EN BIT(6)
#define RD_THROTTLE_EN BIT(5)
#define WR_BW_EN BIT(4)
#define WR_CMD_CNT_EN BIT(3)
#define WR_LATENCY_EN BIT(2)
#define WR_TOTAL_CYCLE_EN BIT(1)
#define WR_THROTTLE_EN BIT(0)

typedef enum axi_monitor_busy
{
	IDLE = 0,
	BUSY = 1
} aximon_busy_t;

#if IS_ENABLED(CONFIG_HOBOT_AXI_ADVANCED)
typedef struct axi_mon_port_conf {

} aximon_port_conf_t;
#endif

/**
 * @struct axi_port
 *
 * @brief
 */
struct axi_port {
#if IS_ENABLED(CONFIG_HOBOT_AXI_ADVANCED)
	struct dentry *debugfs_dentry;
	aximon_port_conf_t port_conf;
#endif
	struct list_head entry;
	const char *name;
	uint16_t idx;
	uint64_t rd_bw;
	uint32_t rd_bw_percent;
	uint64_t wr_bw;
	uint32_t wr_bw_percent;
};

struct axi_monitor_conf {
	uint64_t time;
#if IS_ENABLED(CONFIG_HOBOT_AXI_ADVANCED)
	uint64_t axi_bus_timeout;
#endif
};

typedef union {
	struct {
		uint32_t port_rd_to:16; /* Bit [15:0] */
		uint32_t port_wr_to:16; /* Bit [31:16] */
	} bits;
	uint32_t port_to_status_val;
} port_to_status_t;

typedef union {
	struct {
		uint32_t port_overflow:16; /* Bit [15:0] */
		uint32_t port_err:16; /* Bit [31:16] */
	} bits;
	uint32_t port_status_val;
} port_status_t;

struct axi_monitor {
	struct platform_device *pdev;
	struct cdev cdev;
	struct device *cdev_dev;
	dev_t devt;
	void __iomem *base;
	struct clk *apb_clk;
	uint64_t apb_clk_hz;
	struct clk *axi_clk;
	uint64_t axi_clk_hz;
	int irq;
	uint32_t int_status;
	port_to_status_t port_to_status;
	port_status_t port_status;
#if IS_ENABLED(CONFIG_HOBOT_AXI_MONITOR_DEBUGFS)
	struct dentry *debugfs_root;
#endif
	struct hrtimer  hrtimer;
	struct tasklet_struct tasklet;
	uint32_t ports_num;
	struct list_head mon_ports_list;
	wait_queue_head_t wq_head;
	spinlock_t ops_lock;
	spinlock_t irq_lock;
	struct mutex ops_mutex;
	struct axi_monitor_conf conf;
	aximon_busy_t busy;
	uint64_t session_time;
	uint64_t session_time_max;
	uint64_t actual_time;
	uint64_t full_bw;
	struct notifier_block nb;
};

typedef struct _reg_s {
	uint32_t offset;
	uint32_t value;
} reg_t;

int hb_axi_monitor_create_debugfs(struct axi_monitor *axi_mon);
void axi_mon_start(struct axi_monitor *dev, uint32_t start);
#endif /* _HORIZON_AXI_MONITOR_H */
