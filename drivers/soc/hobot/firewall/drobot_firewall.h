// SPDX-License-Identifier: GPL-2.0+
/*
 * drobotices firewall driver
 *
 * Copyright (C) 2023 Beijing  DRobotics Co.,Ltd.
 * Ming Yu <ming.yu@horizon.cc>
 *
 */
#ifndef _HORIZON_FIREWALL_H
#define _HORIZON_FIREWALL_H

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

/* MPU specification */
#define MPU_REGION_STEP   0x60
#define MPU_REGION_NUMBER 16

#define MPU_START_ADDR_31_0          0x00
#define MPU_START_ADDR_63_32         0x04
#define MPU_END_ADDR_31_0            0x08
#define MPU_END_ADDR_63_32           0x0c
#define MPU_RD_DISABLE_31_0          0X10
#define MPU_RD_DISABLE_63_32         0X14
#define MPU_RD_DISABLE_95_64         0X18
#define MPU_WR_DISABLE_31_0          0X30
#define MPU_WR_DISABLE_63_32         0X34
#define MPU_WR_DISABLE_95_64         0X38
#define MPU_NS_DISABLE               0X50
#define MPU_CFG_REG_LOCK             0X54
#define MPU_READ_DEFAULT_ADDR_31_0   0x1800
#define MPU_READ_DEFAULT_ADDR_63_32  0x1804
#define MPU_WRITE_DEFAULT_ADDR_31_0  0x1808
#define MPU_WRITE_DEFAULT_ADDR_63_32 0x180c

#define MPU_VIO_READ_ADDR_31_0       0x1820
#define MPU_VIO_READ_ADDR_63_32      0x1824
#define MPU_VIO_READ_MASTERID        0x1828
#define MPU_VIO_WRITE_ADDR_31_0      0x182c
#define MPU_VIO_WRITE_ADDR_63_32     0x1830
#define MPU_VIO_WRITE_MASTERID       0x1834

#define MPU_PASSWD                   0x1eac
#define MPU_MISSIONINTMASK0          0x1eb4
#define MPU_MISSIONINTSTATUSSET0     0x1eb8
#define MPU_MISSIONINTSTATUS0        0x1ebc
#define MPU_FIREWALL_ISR_OFFSET      2
#define MPU_FIREWALL_ISR             (1 << MPU_FIREWALL_ISR_OFFSET)

#define MPU_VIO_FLAG_OFFSET          8
#define MPU_VIO_FLAG                 (1 << MPU_VIO_FLAG_OFFSET)

#define MPU_NS_DISABLE_READ  BIT(0)
#define MPU_NS_DISABLE_WRITE BIT(1)

#define MPU_PASSWD_VALUE 0x95F2D303
#define DDR_INTR_STATUS			(0x3830000c)
#define DDR_MPU0_OFFSET         8
#define DDR_MPU1_OFFSET         9
#define DDR_MPU2_OFFSET         10
#define DDR_MPU3_OFFSET         11
#define DDR_MPU4_OFFSET         12
#define DDR_MPU0_MASK           (1 << DDR_MPU0_OFFSET)
#define DDR_MPU1_MASK           (1 << DDR_MPU1_OFFSET)
#define DDR_MPU2_MASK           (1 << DDR_MPU2_OFFSET)
#define DDR_MPU3_MASK           (1 << DDR_MPU3_OFFSET)
#define DDR_MPU4_MASK           (1 << DDR_MPU4_OFFSET)
#define DDR_MPU_ALL_MASK        (DDR_MPU0_MASK | DDR_MPU1_MASK | \
                                DDR_MPU2_MASK | DDR_MPU3_MASK  | \
                                DDR_MPU4_MASK)
struct mpu_region {
        const char *name;
        uint32_t base;
        uint32_t region_number;
        uint32_t region_start_31_0;
        uint32_t region_start_63_32;
        uint32_t region_end_31_0;
        uint32_t region_end_63_32;
        uint32_t disable_rd_userid_31_0;
        uint32_t disable_rd_userid_63_32;
        uint32_t disable_wr_userid_31_0;
        uint32_t disable_wr_userid_63_32;
        uint32_t disable_ns;
        uint32_t lock;
};

struct mpu_protection {
        int irq;
        struct regmap *map;
        void __iomem *ddr_reg_base;
        struct mpu_region *region;
        uint32_t region_count;
        struct workqueue_struct *wq;
};

#endif /* _HORIZON_FIREWALL_H */
