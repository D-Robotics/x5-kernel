/*
 * Horizon Robotics
 *
 *  Copyright (C) 2024 DRobotics Inc.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ":" fmt

#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/ion.h>
#include <linux/cma.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon-tee.h>
#include <dt-bindings/soc/drobot-x5-firewall.h>
#include "drobot_firewall.h"

static const uint32_t mpu_addr[] = {HORIZON_DDR_MPU_0, HORIZON_DDR_MPU_1, HORIZON_DDR_MPU_2,
                                    HORIZON_DDR_MPU_3, HORIZON_DDR_MPU_4};
static uint16_t firewall_en_bitmap = (1 << 0) | (1 << 8);
static struct mpu_region debug_mpu_region = {0};
uint64_t default_addr = 0;
uint32_t summary_all = 0;

static int32_t drobot_find_mpu_region_num(void)
{
        int index = 0;
        uint64_t num = firewall_en_bitmap;
        if (num == 0xffff)
                return -1;
        while (1) {
                if ((num & 1) == 0) {
                        break;
                }
                num >>= 1;
                index++;
        }
        return index;
}

// #define DUMP_MPU_INFO
static irqreturn_t mpu_protection_isr(int this_irq, void *data)
{
        uint32_t ret = 0;
        struct mpu_protection *mpu_prt = NULL;
        uint32_t reg = 0;
        int i = 0;
        uint32_t value = 0;
        uint32_t reg_value = 0;
        uint32_t value31_0 = 0;
        uint32_t value63_32 = 0;
        uint32_t mpu_int_sta = 0;
        uint64_t tmp_value = 0;

        mpu_prt = (struct mpu_protection *)data;

        /* 1. get which ddr mpu insterrupt */
        value = readl(mpu_prt->ddr_reg_base);
        value &= DDR_MPU_ALL_MASK;
        value = value >> DDR_MPU0_OFFSET;
        for (; value > 0; value = value >> 1) {
                /* 2. deal with ddr mpu isr */
                reg = MPU_MISSIONINTSTATUS0 + i * 0x100000;
                ret = regmap_read(mpu_prt->map, reg, &mpu_int_sta);
                if (ret) {
                        pr_err("%d: read MPU 0x%x failed\n", __LINE__, reg);
                        break;
                }
                if (mpu_int_sta & MPU_FIREWALL_ISR) {
                        pr_err("#########MPU ERROR: INVALID ACCESS########\n");
                        /* 3. get vio information */
                        reg = MPU_VIO_READ_MASTERID + i * 0x100000;
                        ret = regmap_read(mpu_prt->map, reg, &reg_value);
                        if (ret) {
                                pr_err("%d: read MPU 0x%x failed\n", __LINE__, reg);
                                reg = MPU_MISSIONINTSTATUS0 + i * 0x100000;
                                regmap_write(mpu_prt->map, reg, mpu_int_sta);
                                break;
                        }
                        if (reg_value & MPU_VIO_FLAG) {
                                pr_err("read violation master id:%d\n", reg_value & 0xff) ;

                                reg = MPU_VIO_READ_ADDR_31_0 + i * 0x100000;
                                ret = regmap_read(mpu_prt->map, reg, &value31_0);
                                if (ret) {
                                        pr_err("%d: read MPU 0x%x failed\n", __LINE__, reg);
                                        reg = MPU_MISSIONINTSTATUS0 + i * 0x100000;
                                        regmap_write(mpu_prt->map, reg, mpu_int_sta);
                                        break;
                                }

                                reg = MPU_VIO_READ_ADDR_63_32 + i * 0x100000;
                                ret = regmap_read(mpu_prt->map, reg, &value63_32);
                                if (ret) {
                                        pr_err("%d: read MPU 0x%x failed\n", __LINE__, reg);
                                        reg = MPU_MISSIONINTSTATUS0 + i * 0x100000;
                                        regmap_write(mpu_prt->map, reg, mpu_int_sta);
                                        break;
                                }
                                tmp_value =  (((uint64_t)(value63_32) << 32) | value31_0) + 0x80000000;
                                pr_err("read violation address 31_0:0x%08llx\n", tmp_value & 0xffffffff);
                                pr_err("read violation address 63_32:0x%08llx\n", (tmp_value >> 32) & 0xffffffff);
                        }

                        reg = MPU_VIO_WRITE_MASTERID + i * 0x100000;
                        ret = regmap_read(mpu_prt->map, reg, &reg_value);
                        if (ret) {
                                pr_err("%d: read MPU 0x%x failed\n", __LINE__, reg);
                                reg = MPU_MISSIONINTSTATUS0 + i * 0x100000;
                                regmap_write(mpu_prt->map, reg, mpu_int_sta);
                                break;
                        }
                        if (reg_value & MPU_VIO_FLAG) {
                                pr_err("write violation master id:%d\n", reg_value & 0xff);
                                reg = MPU_VIO_WRITE_ADDR_31_0 + i * 0x100000;
                                ret = regmap_read(mpu_prt->map, reg, &value31_0);
                                if (ret) {
                                        pr_err("%d: read MPU 0x%x failed\n", __LINE__, reg);
                                        reg = MPU_MISSIONINTSTATUS0 + i * 0x100000;
                                        regmap_write(mpu_prt->map, reg, mpu_int_sta);
                                        break;
                                }

                                reg = MPU_VIO_WRITE_ADDR_63_32 + i * 0x100000;
                                ret = regmap_read(mpu_prt->map, reg, &value63_32);
                                if (ret) {
                                        pr_err("%d: read MPU 0x%x failed\n", __LINE__, reg);
                                        reg = MPU_MISSIONINTSTATUS0 + i * 0x100000;
                                        regmap_write(mpu_prt->map, reg, mpu_int_sta);
                                        break;
                                }
                                tmp_value =  (((uint64_t)(value63_32) << 32) | value31_0) + 0x80000000;
                                pr_err("write violation address 31_0:0x%08llx\n", tmp_value & 0xffffffff);
                                pr_err("write violation address 63_32:0x%08llx\n", (tmp_value >> 32) & 0xffffffff);

                        }
                        pr_err("##########################################\n");

                }
                /* 4. clear interrupt status*/
                reg = MPU_MISSIONINTSTATUS0 + i * 0x100000;
                regmap_write(mpu_prt->map, reg, mpu_int_sta);
                i++;
        }

        return IRQ_HANDLED;
}

void mpu_region_reg_dump(struct regmap *map, uint32_t region_reg_base)
{
        uint32_t value = 0;
        uint32_t s_value31_0 = 0;
        uint32_t s_value63_32 = 0;
        uint32_t e_value31_0 = 0;
        uint32_t e_value63_32 = 0;
        uint64_t r_tmp_value = 0;
        uint64_t w_tmp_value = 0;

        regmap_read(map, region_reg_base + MPU_START_ADDR_31_0, &s_value31_0);
        regmap_read(map, region_reg_base + MPU_START_ADDR_63_32, &s_value63_32);
        regmap_read(map, region_reg_base + MPU_END_ADDR_31_0, &e_value31_0);
        regmap_read(map, region_reg_base + MPU_END_ADDR_63_32, &e_value63_32);

        if (s_value31_0 != 0 || s_value63_32 != 0 || e_value31_0 != 0 || e_value63_32 != 0) {
                r_tmp_value =  (((uint64_t)(s_value63_32) << 32) | s_value31_0) + 0x80000000;
                s_value31_0 = r_tmp_value & 0xffffffff;
                s_value63_32 = (r_tmp_value  >> 32) & 0xffffffff;

                w_tmp_value =  (((uint64_t)(e_value63_32) << 32) | e_value31_0) + 0x80000000;
                e_value31_0 = w_tmp_value & 0xffffffff;
                e_value63_32 = (w_tmp_value  >> 32) & 0xffffffff;
        }

        pr_err("dump [MPU_START_ADDR_31_0:0x%x]:0x%08x\n", HORIZON_DDR_MPU_0 + region_reg_base + MPU_START_ADDR_31_0, s_value31_0);
        pr_err("dump [MPU_START_ADDR_63_32:0x%x]:0x%08x\n", HORIZON_DDR_MPU_0 + region_reg_base + MPU_START_ADDR_63_32, s_value63_32);
        pr_err("dump [MPU_END_ADDR_31_0:0x%x]:0x%08x\n", HORIZON_DDR_MPU_0 + region_reg_base + MPU_END_ADDR_31_0, e_value31_0);
        pr_err("dump [MPU_END_ADDR_63_32:0x%x]:0x%08x\n", HORIZON_DDR_MPU_0 + region_reg_base + MPU_END_ADDR_63_32, e_value63_32);

        /*
        * although read and write id is 256bit, but
        * in x5, 32bit is enough
        */
        for (int i = 0; i < 1; i++) {
                regmap_read(map, region_reg_base + MPU_RD_DISABLE_31_0 + 4 * i, &value);
                pr_err("dump [MPU_RD_DISABLE_31_0:0x%x]:0x%08x\n",
                        HORIZON_DDR_MPU_0 + region_reg_base + MPU_RD_DISABLE_31_0 + 4 * i, value);
                regmap_read(map, region_reg_base + MPU_RD_DISABLE_63_32 + 4 * i, &value);
                pr_err("dump [MPU_RD_DISABLE_63_33:0x%x]:0x%08x\n",
                        HORIZON_DDR_MPU_0 + region_reg_base + MPU_RD_DISABLE_63_32 + 4 * i, value);
        }


        for (int i = 0; i < 1; i++) {
                regmap_read(map, region_reg_base + MPU_WR_DISABLE_31_0 + 4 * i, &value);
                pr_err("dump [MPU_WR_DISABLE_31_0:0x%x]:0x%08x\n",
                        HORIZON_DDR_MPU_0 + region_reg_base + MPU_WR_DISABLE_31_0 + 4 * i, value);
                regmap_read(map, region_reg_base + MPU_WR_DISABLE_63_32 + 4 * i, &value);
                pr_err("dump [MPU_WR_DISABLE_31_0:0x%x]:0x%08x\n",
                        HORIZON_DDR_MPU_0 + region_reg_base + MPU_WR_DISABLE_63_32 + 4 * i, value);
        }

        regmap_read(map, region_reg_base + MPU_NS_DISABLE, &value);
        pr_err("dump [MPU_NS_DISABLE: 0x%x]:0x%08x\n", HORIZON_DDR_MPU_0 + region_reg_base + MPU_NS_DISABLE, value);
        regmap_read(map, region_reg_base + MPU_CFG_REG_LOCK, &value);
        pr_err("dump [MPU_CFG_REG_LOCK: 0x%x]:0x%08x\n", HORIZON_DDR_MPU_0 + region_reg_base + MPU_CFG_REG_LOCK, value);
        pr_err("\n");
}

#ifdef DUMP_MPU_INFO
void dump_mpu_config(struct mpu_region *mpu)
{
        pr_info("region_start_31_0:0x%x\n", mpu->region_start_31_0);
        pr_info("region_start_63_32:0x%x\n", mpu->region_start_63_32);
        pr_info("region_end_31_0:0x%x\n", mpu->region_end_31_0);
        pr_info("region_end_63_32:0x%x\n", mpu->region_end_63_32);
        pr_info("disable_rd_userid_31_0:0x%x\n", mpu->disable_rd_userid_31_0);
        pr_info("disable_rd_userid_63_32:0x%x\n", mpu->disable_rd_userid_63_32);
        pr_info("disable_wr_userid_31_0:0x%x\n", mpu->disable_wr_userid_31_0);
        pr_info("disable_wr_userid_63_32:0x%x\n", mpu->disable_wr_userid_63_32);
        pr_info("disable_ns:0x%x\n", mpu->disable_ns);
        pr_info("lock:0x%x\n", mpu->lock);
        }
#endif

void mpu_config(struct mpu_protection *mpu_prt, uint32_t regmap_base, struct mpu_region *region)
{
        struct regmap *map = mpu_prt->map;
        uint32_t region_reg_base = regmap_base + MPU_REGION_STEP * region->region_number;
        uint32_t default_addr_high = 0;
        uint32_t default_addr_low = 0;

        default_addr_high = (region->default_addr - 0x80000000) >> 32;
        default_addr_low = (region->default_addr - 0x80000000) & 0xffffffff;

#ifdef DUMP_MPU_INFO
        pr_err("default_addr_high:0x%x\n", default_addr_high);
        pr_err("default_addr_low:0x%x\n", default_addr_low);
        dump_mpu_config(region);
#endif

        /* disable ns config */
        regmap_write(map, region_reg_base + MPU_NS_DISABLE, region->disable_ns);

        /* region config */
        regmap_write(map, region_reg_base + MPU_START_ADDR_31_0, region->region_start_31_0);
        regmap_write(map, region_reg_base + MPU_START_ADDR_63_32, region->region_start_63_32);

        regmap_write(map, region_reg_base + MPU_END_ADDR_31_0, region->region_end_31_0);
                        // return;

        regmap_write(map, region_reg_base + MPU_END_ADDR_63_32, region->region_end_63_32);

        /* disable user id config */
        regmap_write(map, region_reg_base + MPU_RD_DISABLE_31_0, region->disable_rd_userid_31_0);
        regmap_write(map, region_reg_base + MPU_RD_DISABLE_63_32, region->disable_rd_userid_63_32);
        for (int i = 0; i < 6; i++)
                regmap_write(map, region_reg_base + MPU_RD_DISABLE_95_64 + 4 * i, 0xffffffff);

        regmap_write(map, region_reg_base + MPU_WR_DISABLE_31_0, region->disable_wr_userid_31_0);
        regmap_write(map, region_reg_base + MPU_WR_DISABLE_63_32, region->disable_wr_userid_63_32);

        regmap_write(map, regmap_base + MPU_READ_DEFAULT_ADDR_31_0, default_addr_low);
        regmap_write(map, regmap_base + MPU_READ_DEFAULT_ADDR_63_32, default_addr_high);
        regmap_write(map, regmap_base + MPU_WRITE_DEFAULT_ADDR_31_0, default_addr_low);
        regmap_write(map, regmap_base + MPU_WRITE_DEFAULT_ADDR_63_32, default_addr_high);
        for (int i = 0; i < 6; i++)
                regmap_write(map, region_reg_base + MPU_WR_DISABLE_95_64 + 4 * i, 0xffffffff);

        /* lock config */
        regmap_write(map, region_reg_base + MPU_CFG_REG_LOCK, region->lock);
#ifdef DUMP_MPU_INFO
        mpu_region_reg_dump(map, region_reg_base);
#endif
        firewall_en_bitmap |= (1UL << region->region_number);
}

/*
* RANGE1: kernel ro section
*/
static int ddr_mpu_protect(struct mpu_protection *mpu_prt, struct mpu_region *region)
{
        int32_t i = 0;
        uint32_t regmap_base = 0;

        if (strncmp(region->name, "kernel", strlen("kernel") + 1) == 0) {
                region->region_start_31_0 = (uint32_t)virt_to_phys(_stext) - 0x80000000;
                region->region_end_31_0 = (uint32_t)virt_to_phys(__end_rodata) - 0x80000000 - 1;
        }
        if (region->base == HORIZON_DDR_MPU_0) {
                for (i = 0; i < ARRAY_SIZE(mpu_addr); i++) {
                        regmap_base = mpu_addr[i] - HORIZON_DDR_MPU_0;
                        mpu_config(mpu_prt, regmap_base, region);
                }
        } else {
                mpu_config(mpu_prt, regmap_base, region);
        }
        return 0;
}

static int drobot_parse_dts(struct platform_device *pdev, struct device_node *np, struct mpu_region *region)
{
        int32_t ret;
        struct device_node *default_np = NULL;
        struct resource resource = {0};

        if (region == NULL) {
                pr_err("NO firewall region space to  parse DeviceTree\n");
                return -ENODEV;
        }

        if (np == NULL || pdev == NULL) {
                pr_err("firewall device not Bind Device\n");
                return -ENODEV;
        }

        ret = of_property_read_u32(np, "region_start_31_0", &region->region_start_31_0);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall region_start_31_0\n");
                return ret;
        }
        ret = of_property_read_u32(np, "region_start_63_32", &region->region_start_63_32);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall region_start_63_32\n");
                return ret;
        }
        ret = of_property_read_u32(np, "region_end_31_0", &region->region_end_31_0);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall region_end_31_0\n");
                return ret;
        }
        ret = of_property_read_u32(np, "region_end_63_32", &region->region_end_63_32);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall region_end_63_32\n");
                return ret;
        }
        ret = of_property_read_u32(np, "disable_rd_userid_31_0", &region->disable_rd_userid_31_0);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall disable_rd_userid_31_0\n");
                return ret;
        }
        ret = of_property_read_u32(np, "disable_rd_userid_63_32", &region->disable_rd_userid_63_32);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall disable_rd_userid_63_32\n");
                return ret;
        }
        ret = of_property_read_u32(np, "disable_wr_userid_31_0", &region->disable_wr_userid_31_0);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall disable_wr_userid_31_0\n");
                return ret;
        }
        ret = of_property_read_u32(np, "disable_wr_userid_63_32", &region->disable_wr_userid_63_32);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall disable_wr_userid_63_32\n");
                return ret;
        }
        ret = of_property_read_u32(np, "disable_ns", &region->disable_ns);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall disable_ns\n");
                return ret;
        }
        ret = of_property_read_u32(np, "lock", &region->lock);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall lock\n");
                return ret;
        }
        ret = of_property_read_string(np, "region_name", &region->name);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall region name\n");
                return ret;
        }
        ret = of_property_read_u32(np, "base", &region->base);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall base\n");
                return ret;
        }
        ret = of_property_read_u32(np, "region_number", &region->region_number);
        if (ret < 0) {
                dev_err(&pdev->dev, "Can't get firewall base\n");
                return ret;
        }
        default_np = of_parse_phandle(np, "memory-region", 0);
        if (!default_np) {
                dev_err(&pdev->dev, "No %s specified\n", "memory-region");
                return -1;
        }
        ret = of_address_to_resource(default_np, 0, &resource);
        if (ret) {
                dev_err(&pdev->dev, "No memory address assigned to the region\n");
                return ret;
        }

        region->default_addr = resource.start;
        default_addr = resource.start;
        return 0;
}

/**
 * hobot_smccc_smc() - Method to call firmware via SMC.
 * @a0: Argument passed to Secure EL3.
 * @a1: Argument passed to Secure EL3.
 * @a2: Argument passed to Secure EL3.
 * @a3: Argument passed to Secure EL3.
 * @a4: Argument passed to Secure EL3.
 * @a5: Argument passed to Secure EL3.
 * @a6: Argument passed to Secure EL3.
 * @a7: Argument passed to Secure EL3.
 * @res: return code stored in.
 *
 * This function call arm_smccc_smc directly.
 *
 * Return: return value stored in res argument.
 */
static void drobot_smccc_smc(unsigned long a0, unsigned long a1,
			    unsigned long a2, unsigned long a3,
			    unsigned long a4, unsigned long a5,
			    unsigned long a6, unsigned long a7,
			    struct arm_smccc_res *res)
{
	arm_smccc_smc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

static int is_match_regmap_version(void)
{
        struct arm_smccc_res res;
        uint32_t major_ver = 0;
        uint32_t minor_ver = 0;
        bool match = 0;

        drobot_smccc_smc(0xC200000B, 0x10, 0, 0,
                        0, 0, 0, 0, &res);
        if (res.a0 != 0) {
                pr_err("get regmap version failed\n");
                return 0;
        }
        major_ver = REGMAP_TEE_MAJOR_VERSION(res.a1);
        minor_ver = REGMAP_TEE_MINOR_VERSION(res.a1);
        match = REGMAP_TEE_VERSION_MATCH(major_ver, minor_ver);
        if (match == 0) {
                pr_err("BL31 and Linux regmap version is not matched\n");
                pr_err("BL31 regmap version is %d.%d\n", major_ver, minor_ver);
                pr_err("Linux regmap version is %d.%d\n", REGMAP_TEE_VER_MAJOR, REGMAP_TEE_VER_MINOR);
        }
        return match;

}
static ssize_t firewall_summary_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct mpu_protection *mpu_prt = (struct mpu_protection *)dev_get_drvdata(dev);
        uint32_t i = 0;
        uint32_t j = 0;
        uint32_t region_reg_base = 0;
        uint32_t value31_0 = 0;
        uint32_t value63_32 = 0;
        uint64_t tmp_value = 0;

        pr_err("firewall_en_bitmap:0x%x\n", firewall_en_bitmap);
        for (i = 0; i < sizeof(firewall_en_bitmap) * 8; i++) {
                if (((1UL << i) & firewall_en_bitmap) || summary_all) {
                        region_reg_base = MPU_REGION_STEP * i;
                        pr_err("-------------dump firewall_%d-----------------\n", i);
                        mpu_region_reg_dump(mpu_prt->map, region_reg_base);
                }
        }
        if (summary_all) {
                /* dump all (5) firewall infromation */
                for (j = 1; j < ARRAY_SIZE(mpu_addr); j++) {
                        for (i = 0; i < sizeof(firewall_en_bitmap) * 8; i++) {
                                pr_err("-------------dump firewall_%d %d-----------------\n", j, i);
                                region_reg_base = mpu_addr[j] - HORIZON_DDR_MPU_0;
                                region_reg_base += MPU_REGION_STEP * i;
                                mpu_region_reg_dump(mpu_prt->map, region_reg_base);
                        }
                }

        }
        pr_err("-------------dump firewall_default address----------\n");
        regmap_read(mpu_prt->map, MPU_READ_DEFAULT_ADDR_31_0, &value31_0);
        regmap_read(mpu_prt->map, MPU_READ_DEFAULT_ADDR_63_32, &value63_32);
        if (value31_0 != 0 || value63_32 != 0)
                tmp_value =  (((uint64_t)(value63_32) << 32) | value31_0) + 0x80000000;

        pr_err("read default address low:0x%08llx\n", tmp_value & 0xffffffff);
        pr_err("read default address high:0x%08llx\n", (tmp_value >> 32) & 0xffffffff);

        regmap_read(mpu_prt->map, MPU_WRITE_DEFAULT_ADDR_31_0, &value31_0);
        regmap_read(mpu_prt->map, MPU_WRITE_DEFAULT_ADDR_63_32, &value63_32);
        if (value31_0 != 0 || value63_32 != 0)
                tmp_value =  (((uint64_t)(value63_32) << 32) | value31_0) + 0x80000000;

        pr_err("write default address low:0x%08llx\n", tmp_value & 0xffffffff);
        pr_err("write default address high:0x%08llx\n", (tmp_value >> 32) & 0xffffffff);
	return 0;
}

static ssize_t firewall_summary_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
        int32_t ret;
        uint32_t tmp_val;
        ret = sscanf(buf, "%d", &tmp_val);
        if (ret < 0) {
                dev_err(dev, "get firewall summary mode failed\n");
                return 0;
        }
        summary_all = tmp_val;
        return (ssize_t)len;
}

static ssize_t firewall_addr_start(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
        int32_t ret;
        uint64_t tmp_val;

        ret = sscanf(buf, "%llx", &tmp_val);
        if (ret < 0) {
                dev_err(dev, "get firewall disable_ns failed\n");
                return 0;
        }
        if (tmp_val < 0x80000000) {
                dev_err(dev, "valid firewall end ddr address, x5 ddr start is 0x80000000\n");
                return 0;
        }
        debug_mpu_region.region_start_63_32 = (tmp_val - 0x80000000) >> 32;
        debug_mpu_region.region_start_31_0 = (tmp_val - 0x80000000) & 0xffffffff;
        return (ssize_t)len;
}

static ssize_t firewall_addr_end(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
        int32_t ret;
        uint64_t tmp_val;

        ret = sscanf(buf, "%llx", &tmp_val);
        if (ret < 0) {
                dev_err(dev, "get firewall disable_ns failed\n");
                return 0;
        }
        if (tmp_val < 0x80000000) {
                dev_err(dev, "valid firewall end ddr address, x5 ddr start is 0x80000000\n");
                return 0;
        }
        debug_mpu_region.region_end_63_32 = (tmp_val - 0x80000000) >> 32;
        debug_mpu_region.region_end_31_0 = (tmp_val - 0x80000000) & 0xffffffff;
        return (ssize_t)len;
}

static ssize_t firewall_disable_rd_id_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
        int32_t ret;
        uint64_t tmp_val = 0;
        ret = sscanf(buf, "%llx", &tmp_val);
        if (ret < 0) {
                dev_err(dev, "get firewall disable_ns failed\n");
                return 0;
        }
        debug_mpu_region.disable_rd_userid_31_0 = tmp_val & 0xffffffff;
        debug_mpu_region.disable_rd_userid_63_32 = (tmp_val >> 32) & 0xffffffff;
        return (ssize_t)len;
}

static ssize_t firewall_disable_wr_id_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
        int32_t ret;
        uint64_t tmp_val = 0;
        ret = sscanf(buf, "%llx", &tmp_val);
        if (ret < 0) {
                dev_err(dev, "get firewall disable_ns failed\n");
                return 0;
        }
        debug_mpu_region.disable_wr_userid_31_0 = tmp_val & 0xffffffff;
        debug_mpu_region.disable_wr_userid_63_32 = (tmp_val >> 32) & 0xffffffff;
        return (ssize_t)len;
}

static ssize_t firewall_disable_ns_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
        int32_t ret;
        uint32_t tmp_val;
        ret = sscanf(buf, "%xu", &tmp_val);
        if (ret < 0) {
                dev_err(dev, "get firewall disable_ns failed\n");
                return 0;
        }
        debug_mpu_region.disable_ns = tmp_val;
        return (ssize_t)len;
}

static ssize_t firewall_enable_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
        struct mpu_protection *mpu_prt = (struct mpu_protection *)dev_get_drvdata(dev);
        int32_t ret;

        debug_mpu_region.base = HORIZON_DDR_MPU_0;
        debug_mpu_region.name = "sys_debug";
        debug_mpu_region.default_addr = default_addr;
        ret = drobot_find_mpu_region_num();
        if (ret == -1) {
                dev_err(dev, "can not find avilable firewall\n");
                return -1;
        }
        debug_mpu_region.region_number = ret;
        ddr_mpu_protect(mpu_prt, &debug_mpu_region);
        memset((void *)&debug_mpu_region, 0 , sizeof(debug_mpu_region));

	return (ssize_t)len;
}

static DEVICE_ATTR(summary, S_IRUGO | S_IWUSR, firewall_summary_show, firewall_summary_store);

static DEVICE_ATTR(addr_start, S_IWUSR, NULL, firewall_addr_start);

static DEVICE_ATTR(addr_end, S_IWUSR, NULL, firewall_addr_end);

static DEVICE_ATTR(disable_rd_id, S_IWUSR, NULL, firewall_disable_rd_id_store);

static DEVICE_ATTR(disable_wr_id, S_IWUSR, NULL, firewall_disable_wr_id_store);

static DEVICE_ATTR(disable_ns, S_IWUSR, NULL, firewall_disable_ns_store);

static DEVICE_ATTR(enable, S_IWUSR, NULL, firewall_enable_store);

static struct attribute *firewall_attrs[] = {
        &dev_attr_summary.attr,
        &dev_attr_addr_start.attr,
        &dev_attr_addr_end.attr,
        &dev_attr_disable_rd_id.attr,
        &dev_attr_disable_wr_id.attr,
        &dev_attr_disable_ns.attr,
        &dev_attr_enable.attr,
        NULL,
};

static struct attribute_group firewall_attr_group = {
        .name = "debug",
        .attrs = firewall_attrs,
};

static int drobot_firewall_probe(struct platform_device *pdev)
{
        int ret = 0;
        struct mpu_protection *mpu_prt = NULL;
        int32_t irq = 0;
        struct device_node *np = pdev->dev.of_node;
        struct device_node *node = NULL;
        uint32_t i = 0;

        ret = is_match_regmap_version();
        if (ret == 0) {
               return -1;
        }
        mpu_prt = devm_kzalloc(&pdev->dev, sizeof(struct mpu_protection), GFP_KERNEL);
        if(!mpu_prt) {
                return -ENOMEM;
        }

        mpu_prt->map = syscon_tee_node_to_regmap(np);
        if (IS_ERR(mpu_prt->map))
                return PTR_ERR(mpu_prt->map);

        for_each_available_child_of_node(np, node) {
                mpu_prt->region_count++;
        }
        mpu_prt->region = devm_kzalloc(&pdev->dev, sizeof(struct mpu_region) * mpu_prt->region_count, GFP_KERNEL);
        if(!mpu_prt->region) {
                return -ENOMEM;
        }
        for_each_available_child_of_node (np, node) {
                ret = drobot_parse_dts(pdev, node, &mpu_prt->region[i]);
                if (ret) {
                        dev_err(&pdev->dev, "parse dts failed\n");
                        return ret;
                }
                ddr_mpu_protect(mpu_prt, &mpu_prt->region[i]);
                i++;
        }
        mpu_prt->ddr_reg_base = devm_ioremap(&pdev->dev, DDR_INTR_STATUS, 4);
        if (IS_ERR(mpu_prt->ddr_reg_base)) {
                dev_err(&pdev->dev, "Can't get ddr int status register\n");
                return (int32_t)PTR_ERR(mpu_prt->ddr_reg_base);
        }
        mpu_prt->irq = platform_get_irq(pdev, 0);
        if (irq < 0) {
                pr_info("sec_fw irq is not in dts\n");
                return -ENODEV;
        }

        ret = request_irq(mpu_prt->irq, mpu_protection_isr, IRQF_TRIGGER_HIGH,
                                        "mpu_prt", mpu_prt);
        if (ret) {
                dev_err(&pdev->dev, "Could not request mpu_prt irq %d\n",
                                mpu_prt->irq);
                return -ENODEV;
        }
        dev_set_drvdata(&pdev->dev, mpu_prt);
        ret = device_add_group(&pdev->dev, &firewall_attr_group);
        if (ret < 0) {
                dev_err(&pdev->dev, "Create firewall debug group failed\n");
                return ret;
        }
        return 0;
}

int drobot_firewall_remove(struct platform_device *pdev)
{
        //free
        return  0;
}

static const struct of_device_id firewall_match[] = {
        { .compatible = "d-robotics,firewall" },
        { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, firewall_match);

static struct platform_driver firewall_driver = {
        .probe	= drobot_firewall_probe,
        .remove = drobot_firewall_remove,
        .driver = {
                .name	= "firewall",
                .of_match_table = firewall_match,
        }
};
module_platform_driver(firewall_driver);

MODULE_AUTHOR("Ming Yu");
MODULE_DESCRIPTION("Horizon X5 firewall Driver");
MODULE_LICENSE("GPL v2");
