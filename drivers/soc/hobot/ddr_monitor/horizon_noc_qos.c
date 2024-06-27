// SPDX-License-Identifier: GPL-2.0+
/*
 * rewrite Horizon Sunrise 5 Noc Qos Driver
 *
 * Copyright (C) 2023 Beijing Horizon Robotics Co.,Ltd.
 * Hualun Dong <hualun.dong@horizon.cc>
 *
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/delay.h>

#define NOC_QOS_PRIORITY_OFFSET0 (0x08)
#define NOC_QOS_PRIORITY_OFFSET1 (0x88)
#define NOC_QOS_PRIORITY_OFFSET2 (0x108)
#define NOC_QOS_PRIORITY_OFFSET3 (0x188)
#define NOC_QOS_PRIORITY_OFFSET4 (0x208)
#define NOC_QOS_PRIORITY_OFFSET5 (0x288)
#define NOC_QOS_PRIORITY_OFFSET6 (0x308)
#define NOC_QOS_PRIORITY_OFFSET7 (0x388)
#define NOC_QOS_PRIORITY_OFFSET8 (0x408)
#define NOC_QOS_PRIORITY_OFFSET9 (0x488)
#define NOC_QOS_PRIORITY_OFFSET10 (0x508)
#define NOC_QOS_PRIORITY_OFFSET11 (0x588)
#define NOC_QOS_PRIORITY_OFFSET12 (0x608)
#define NOC_QOS_PRIORITY_OFFSET13 (0x688)
#define NOC_QOS_PRIORITY_OFFSET14 (0x708)

#define NOC_QOS_MODE_OFFSET0 (0x0c)
#define NOC_QOS_MODE_OFFSET1 (0x8c)
#define NOC_QOS_MODE_OFFSET2 (0x10c)
#define NOC_QOS_MODE_OFFSET3 (0x18c)
#define NOC_QOS_MODE_OFFSET4 (0x20c)
#define NOC_QOS_MODE_OFFSET5 (0x28c)
#define NOC_QOS_MODE_OFFSET6 (0x30c)
#define NOC_QOS_MODE_OFFSET7 (0x38c)
#define NOC_QOS_MODE_OFFSET8 (0x40c)
#define NOC_QOS_MODE_OFFSET9 (0x48c)
#define NOC_QOS_MODE_OFFSET10 (0x50c)
#define NOC_QOS_MODE_OFFSET11 (0x58c)
#define NOC_QOS_MODE_OFFSET12 (0x60c)
#define NOC_QOS_MODE_OFFSET13 (0x68c)
#define NOC_QOS_MODE_OFFSET14 (0x70c)

//CPU
#define ACE0_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET0
#define ACE1_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET1
#define PERIF_QOS_PRIORITY_OFFSET        NOC_QOS_PRIORITY_OFFSET2
#define ACE0_QOS_MODE_OFFSET         	 NOC_QOS_MODE_OFFSET0
#define ACE1_QOS_MODE_OFFSET         	 NOC_QOS_MODE_OFFSET1
#define PERIF_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET2
//BPU
#define BPU_QOS_PRIORITY_OFFSET          NOC_QOS_PRIORITY_OFFSET0
#define BPU_QOS_MODE_OFFSET          	 NOC_QOS_MODE_OFFSET0
//VIN
#define BT1120_QOS_PRIORITY_OFFSET       NOC_QOS_PRIORITY_OFFSET0
#define DC8000_QOS_PRIORITY_OFFSET       NOC_QOS_PRIORITY_OFFSET1
#define DW230_AXI0_QOS_PRIORITY_OFFSET   NOC_QOS_PRIORITY_OFFSET2
#define DW230_AXI1_QOS_PRIORITY_OFFSET   NOC_QOS_PRIORITY_OFFSET3
#define DW230_AXI2_QOS_PRIORITY_OFFSET   NOC_QOS_PRIORITY_OFFSET4
#define ISP_AXI5_QOS_PRIORITY_OFFSET     NOC_QOS_PRIORITY_OFFSET5
#define ISP_AXI4_QOS_PRIORITY_OFFSET     NOC_QOS_PRIORITY_OFFSET6
#define ISP_AXI3_QOS_PRIORITY_OFFSET     NOC_QOS_PRIORITY_OFFSET7
#define ISP_AXI1_QOS_PRIORITY_OFFSET     NOC_QOS_PRIORITY_OFFSET9
#define SIF0_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET10
#define SIF1_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET11
#define SIF2_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET12
#define SIF3_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET13
#define SIF_DISP_QOS_PRIORITY_OFFSET     NOC_QOS_PRIORITY_OFFSET14
#define BT1120_QOS_MODE_OFFSET       	 NOC_QOS_MODE_OFFSET0
#define DC8000_QOS_MODE_OFFSET       	 NOC_QOS_MODE_OFFSET1
#define DW230_AXI0_QOS_MODE_OFFSET  	 NOC_QOS_MODE_OFFSET2
#define DW230_AXI1_QOS_MODE_OFFSET  	 NOC_QOS_MODE_OFFSET3
#define DW230_AXI2_QOS_MODE_OFFSET  	 NOC_QOS_MODE_OFFSET4
#define ISP_AXI5_QOS_MODE_OFFSET    	 NOC_QOS_MODE_OFFSET5
#define ISP_AXI4_QOS_MODE_OFFSET    	 NOC_QOS_MODE_OFFSET6
#define ISP_AXI3_QOS_MODE_OFFSET    	 NOC_QOS_MODE_OFFSET7
#define ISP_AXI1_QOS_MODE_OFFSET    	 NOC_QOS_MODE_OFFSET9
#define SIF0_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET10
#define SIF1_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET11
#define SIF2_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET12
#define SIF3_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET13
#define SIF_DISP_QOS_MODE_OFFSET    	 NOC_QOS_MODE_OFFSET14
//CODEC
#define VIDEO_QOS_PRIORITY_OFFSET        NOC_QOS_PRIORITY_OFFSET0
#define JPEG_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET1
#define VIDEO_QOS_MODE_OFFSET       	 NOC_QOS_MODE_OFFSET0
#define JPEG_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET1
//GPU
#define GPU2D_QOS_PRIORITY_OFFSET        NOC_QOS_PRIORITY_OFFSET0
#define GPU3D0__QOS_PRIORITY_OFFSET      NOC_QOS_PRIORITY_OFFSET1
#define GPU3D1__QOS_PRIORITY_OFFSET      NOC_QOS_PRIORITY_OFFSET2
#define GPU2D_QOS_MODE_OFFSET       	 NOC_QOS_MODE_OFFSET0
#define GPU3D0__QOS_MODE_OFFSET     	 NOC_QOS_MODE_OFFSET1
#define GPU3D1__QOS_MODE_OFFSET     	 NOC_QOS_MODE_OFFSET2
//HSIO
#define DMA0_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET0
#define EMMC_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET1
#define GMAC_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET2
#define SD_QOS_PRIORITY_OFFSET           NOC_QOS_PRIORITY_OFFSET3
#define SDIO_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET4
#define SECURITY_QOS_PRIORITY_OFFSET     NOC_QOS_PRIORITY_OFFSET5
#define USB2_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET6
#define USB3_QOS_PRIORITY_OFFSET         NOC_QOS_PRIORITY_OFFSET7
#define ETR_QOS_PRIORITY_OFFSET          NOC_QOS_PRIORITY_OFFSET8
#define DMA0_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET0
#define EMMC_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET1
#define GMAC_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET2
#define SD_QOS_MODE_OFFSET          	 NOC_QOS_MODE_OFFSET3
#define SDIO_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET4
#define SECURITY_QOS_MODE_OFFSET    	 NOC_QOS_MODE_OFFSET5
#define USB2_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET6
#define USB3_QOS_MODE_OFFSET        	 NOC_QOS_MODE_OFFSET7
#define ETR_QOS_MODE_OFFSET         	 NOC_QOS_MODE_OFFSET8
//HIFI5
#define HIFI5_QOS_PRIORITY_OFFSET        NOC_QOS_PRIORITY_OFFSET0
#define DSP_QOS_PRIORITY_OFFSET          NOC_QOS_PRIORITY_OFFSET1
#define HIFI5_QOS_MODE_OFFSET       	 NOC_QOS_MODE_OFFSET0
#define DSP_QOS_MODE_OFFSET         	 NOC_QOS_MODE_OFFSET1

struct noc_qos{
	struct platform_device *pdev;
	struct device *dev;
	void __iomem *regs_cpu;
	void __iomem *regs_bpu;
	void __iomem *regs_vin;
	void __iomem *regs_codec;
	void __iomem *regs_gpu;
	void __iomem *regs_hsio;
	void __iomem *regs_hifi5;
	struct mutex ops_lock;
};

struct noc_qos* nocqos = NULL;

static ssize_t cpu_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_cpu + ACE0_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "cpu_priority : %d\n", tmp);
}

static ssize_t cpu_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);
		return count;
	}
	writel(zero, nocqos->regs_cpu + ACE0_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_cpu + ACE1_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_cpu + PERIF_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_cpu + ACE0_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_cpu + ACE0_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_cpu + ACE1_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_cpu + PERIF_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t bpu_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_bpu + BPU_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "bpu_priority : %d\n", tmp);
}

static ssize_t bpu_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_bpu + BPU_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_bpu + BPU_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_bpu + BPU_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t bt1120_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + BT1120_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "bt1120_priority : %d\n", tmp);
}

static ssize_t dc8000_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + DC8000_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "dc8000_priority : %d\n", tmp);
}

static ssize_t dw230_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + DW230_AXI0_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "dw230_priority : %d\n", tmp);
}

static ssize_t isp_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + ISP_AXI1_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "isp_priority : %d\n", tmp);
}

static ssize_t sif_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + SIF0_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "sif_priority : %d\n", tmp);
}

static ssize_t sifdisp_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + SIF_DISP_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "sif_disp_priority : %d\n", tmp);
}

static ssize_t bt1120_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + BT1120_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + BT1120_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_vin + BT1120_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t dc8000_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + DC8000_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + DC8000_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_vin + DC8000_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t dw230_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + DW230_AXI0_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + DW230_AXI1_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + DW230_AXI2_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + DW230_AXI0_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_vin + DW230_AXI0_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + DW230_AXI1_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + DW230_AXI2_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t isp_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + ISP_AXI5_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + ISP_AXI4_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + ISP_AXI3_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + ISP_AXI1_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + ISP_AXI1_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_vin + ISP_AXI1_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + ISP_AXI3_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + ISP_AXI4_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + ISP_AXI5_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t sif_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + SIF0_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + SIF1_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + SIF2_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + SIF3_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + SIF0_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_vin + SIF0_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + SIF1_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + SIF2_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + SIF3_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t sifdisp_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + SIF_DISP_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + SIF_DISP_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_vin + SIF_DISP_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t video_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_codec + VIDEO_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "video_priority : %d\n", tmp);
}

static ssize_t video_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_codec + VIDEO_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_codec + VIDEO_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_codec + VIDEO_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t jpeg_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_codec + JPEG_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "jpeg_priority : %d\n", tmp);
}

static ssize_t jpeg_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_codec + JPEG_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_codec + JPEG_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_codec + JPEG_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t gpu_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_gpu + GPU2D_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "gpu_priority : %d\n", tmp);
}

static ssize_t gpu_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_gpu + GPU2D_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_gpu + GPU3D0__QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_gpu + GPU3D1__QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_gpu + GPU2D_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_gpu + GPU2D_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_gpu + GPU3D0__QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_gpu + GPU3D1__QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t dma0_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + DMA0_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "dma0_priority : %d\n", tmp);
}

static ssize_t dma0_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + DMA0_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + DMA0_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_hsio + DMA0_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t emmc_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + EMMC_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "emmc_priority : %d\n", tmp);
}

static ssize_t emmc_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + EMMC_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + EMMC_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_hsio + EMMC_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t gmac_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + GMAC_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "gmac_priority : %d\n", tmp);
}

static ssize_t gmac_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + GMAC_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + GMAC_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_hsio + GMAC_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t sd_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + SD_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "sd_priority : %d\n", tmp);
}

static ssize_t sd_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + SD_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + SD_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_hsio + SD_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t sdio_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + SDIO_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "sdio_priority : %d\n", tmp);
}

static ssize_t sdio_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + SDIO_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + SDIO_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_hsio + SDIO_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t security_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + SECURITY_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "security_priority : %d\n", tmp);
}

static ssize_t security_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + SECURITY_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + SECURITY_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_hsio + SECURITY_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t usb2_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + USB2_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "usb2_priority : %d\n", tmp);
}

static ssize_t usb2_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + USB2_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + USB2_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_hsio + USB2_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t usb3_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + USB3_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "usb3_priority : %d\n", tmp);
}

static ssize_t usb3_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + USB3_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + USB3_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_hsio + USB3_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t etr_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + ETR_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "etr_priority : %d\n", tmp);
}

static ssize_t etr_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + ETR_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + ETR_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_hsio + ETR_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t hifi5_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hifi5 + HIFI5_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "hifi5_priority : %d\n", tmp);
}

static ssize_t hifi5_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hifi5 + HIFI5_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hifi5 + HIFI5_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_hifi5 + HIFI5_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t dsp_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hifi5 + DSP_QOS_PRIORITY_OFFSET);
	tmp &= 0x07;

	return sprintf(buf, "dsp_priority : %d\n", tmp);
}

static ssize_t dsp_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hifi5 + DSP_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hifi5 + DSP_QOS_PRIORITY_OFFSET);
	tmp &= ~0x07;
	tmp |= read_ctl_value;
	writel(tmp, nocqos->regs_hifi5 + DSP_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static void print_noc_qos_warning_info(char *buf, int *len)
{
	*len += sprintf(buf + *len, "****************************************************\n");
	*len += sprintf(buf + *len, "All priorities aren't allowed to be configured in one time.\n");
	*len += sprintf(buf + *len, "The bpu is turned off by default and can only be operated on related registers when it is turned on.\n");
	*len += sprintf(buf + *len, "****************************************************\n");
}

static ssize_t all_priority_write_ctl_show(struct device_driver *drv, char *buf)
{
	int len = 0;

	print_noc_qos_warning_info(buf, &len);

	len += cpu_priority_write_ctl_show(drv, buf + len);
	len += bt1120_priority_write_ctl_show(drv, buf + len);
	len += dc8000_priority_write_ctl_show(drv, buf + len);
	len += dw230_priority_write_ctl_show(drv, buf + len);
	len += isp_priority_write_ctl_show(drv, buf + len);
	len += sif_priority_write_ctl_show(drv, buf + len);
	len += sifdisp_priority_write_ctl_show(drv, buf + len);
	len += video_priority_write_ctl_show(drv, buf + len);
	len += jpeg_priority_write_ctl_show(drv, buf + len);
	len += gpu_priority_write_ctl_show(drv, buf + len);
	len += dma0_priority_write_ctl_show(drv, buf + len);
	len += emmc_priority_write_ctl_show(drv, buf + len);
	len += gmac_priority_write_ctl_show(drv, buf + len);
	len += sd_priority_write_ctl_show(drv, buf + len);
	len += sdio_priority_write_ctl_show(drv, buf + len);
	len += security_priority_write_ctl_show(drv, buf + len);
	len += usb2_priority_write_ctl_show(drv, buf + len);
	len += usb3_priority_write_ctl_show(drv, buf + len);
	len += etr_priority_write_ctl_show(drv, buf + len);
	len += hifi5_priority_write_ctl_show(drv, buf + len);
	len += dsp_priority_write_ctl_show(drv, buf + len);

	return len;
}

static ssize_t all_priority_write_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	return count;
}

static struct driver_attribute cpu_priority_write_ctl = __ATTR(cpu, 0664,
						   cpu_priority_write_ctl_show, cpu_priority_write_ctl_store);
static struct driver_attribute bpu_priority_write_ctl = __ATTR(bpu, 0664,
						   bpu_priority_write_ctl_show, bpu_priority_write_ctl_store);
static struct driver_attribute bt1120_priority_write_ctl = __ATTR(bt1120, 0664,
						   bt1120_priority_write_ctl_show, bt1120_priority_write_ctl_store);
static struct driver_attribute dc8000_priority_write_ctl = __ATTR(dc8000, 0664,
						   dc8000_priority_write_ctl_show, dc8000_priority_write_ctl_store);
static struct driver_attribute dw230_priority_write_ctl = __ATTR(dw230, 0664,
						   dw230_priority_write_ctl_show, dw230_priority_write_ctl_store);
static struct driver_attribute isp_priority_write_ctl = __ATTR(isp, 0664,
						   isp_priority_write_ctl_show, isp_priority_write_ctl_store);
static struct driver_attribute sif_priority_write_ctl = __ATTR(sif, 0664,
						   sif_priority_write_ctl_show, sif_priority_write_ctl_store);
static struct driver_attribute sifdisp_priority_write_ctl = __ATTR(sifdisp, 0664,
						   sifdisp_priority_write_ctl_show, sifdisp_priority_write_ctl_store);
static struct driver_attribute video_priority_write_ctl = __ATTR(video, 0664,
						   video_priority_write_ctl_show, video_priority_write_ctl_store);
static struct driver_attribute jpeg_priority_write_ctl = __ATTR(jpeg, 0664,
						   jpeg_priority_write_ctl_show, jpeg_priority_write_ctl_store);
static struct driver_attribute gpu_priority_write_ctl = __ATTR(gpu, 0664,
						   gpu_priority_write_ctl_show, gpu_priority_write_ctl_store);
static struct driver_attribute dma0_priority_write_ctl = __ATTR(dma0, 0664,
						   dma0_priority_write_ctl_show, dma0_priority_write_ctl_store);
static struct driver_attribute emmc_priority_write_ctl = __ATTR(emmc, 0664,
						   emmc_priority_write_ctl_show, emmc_priority_write_ctl_store);
static struct driver_attribute gmac_priority_write_ctl = __ATTR(gmac, 0664,
						   gmac_priority_write_ctl_show, gmac_priority_write_ctl_store);
static struct driver_attribute sd_priority_write_ctl = __ATTR(sd, 0664,
						   sd_priority_write_ctl_show, sd_priority_write_ctl_store);
static struct driver_attribute sdio_priority_write_ctl = __ATTR(sdio, 0664,
						   sdio_priority_write_ctl_show, sdio_priority_write_ctl_store);
static struct driver_attribute security_priority_write_ctl = __ATTR(security, 0664,
						   security_priority_write_ctl_show, security_priority_write_ctl_store);
static struct driver_attribute usb2_priority_write_ctl = __ATTR(usb2, 0664,
						   usb2_priority_write_ctl_show, usb2_priority_write_ctl_store);
static struct driver_attribute usb3_priority_write_ctl = __ATTR(usb3, 0664,
						   usb3_priority_write_ctl_show, usb3_priority_write_ctl_store);
static struct driver_attribute etr_priority_write_ctl = __ATTR(etr, 0664,
						   etr_priority_write_ctl_show, etr_priority_write_ctl_store);
static struct driver_attribute hifi5_priority_write_ctl = __ATTR(hifi5, 0664,
						   hifi5_priority_write_ctl_show, hifi5_priority_write_ctl_store);
static struct driver_attribute dsp_priority_write_ctl = __ATTR(dsp, 0664,
						   dsp_priority_write_ctl_show, dsp_priority_write_ctl_store);
static struct driver_attribute all_priority_write_ctl = __ATTR(all, 0664,
						   all_priority_write_ctl_show, all_priority_write_ctl_store);

static struct attribute *priority_write_qctrl_attrs[] = {
	&cpu_priority_write_ctl.attr,
	&bpu_priority_write_ctl.attr,
	&bt1120_priority_write_ctl.attr,
	&dc8000_priority_write_ctl.attr,
	&dw230_priority_write_ctl.attr,
	&isp_priority_write_ctl.attr,
	&sif_priority_write_ctl.attr,
	&sifdisp_priority_write_ctl.attr,
	&video_priority_write_ctl.attr,
	&jpeg_priority_write_ctl.attr,
	&gpu_priority_write_ctl.attr,
	&dma0_priority_write_ctl.attr,
	&emmc_priority_write_ctl.attr,
	&gmac_priority_write_ctl.attr,
	&sd_priority_write_ctl.attr,
	&sdio_priority_write_ctl.attr,
	&security_priority_write_ctl.attr,
	&usb2_priority_write_ctl.attr,
	&usb3_priority_write_ctl.attr,
	&etr_priority_write_ctl.attr,
	&hifi5_priority_write_ctl.attr,
	&dsp_priority_write_ctl.attr,
	&all_priority_write_ctl.attr,
	NULL,
};

static struct attribute_group priority_write_attr_group = {
	.name = "write_priority_qos_ctrl",
	.attrs = priority_write_qctrl_attrs,
};

static ssize_t cpu_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_cpu + ACE0_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "cpu_priority : %d\n", tmp);
}

static ssize_t cpu_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_cpu + ACE0_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_cpu + ACE1_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_cpu + PERIF_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_cpu + ACE0_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_cpu + ACE0_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_cpu + ACE1_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_cpu + PERIF_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t bpu_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_bpu + BPU_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "bpu_priority : %d\n", tmp);
}

static ssize_t bpu_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_bpu + BPU_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_bpu + BPU_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_bpu + BPU_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t bt1120_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + BT1120_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "vin_priority : %d\n", tmp);
}

static ssize_t dc8000_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + DC8000_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "dc8000_priority : %d\n", tmp);
}

static ssize_t dw230_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + DW230_AXI0_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "dw230_priority : %d\n", tmp);
}

static ssize_t isp_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + ISP_AXI1_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "isp_priority : %d\n", tmp);
}

static ssize_t sif_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + SIF0_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "sif_priority : %d\n", tmp);
}

static ssize_t sifdisp_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_vin + SIF_DISP_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "sif_disp_priority : %d\n", tmp);
}

static ssize_t bt1120_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + BT1120_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + BT1120_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_vin + BT1120_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t dc8000_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + DC8000_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + DC8000_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_vin + DC8000_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t dw230_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + DW230_AXI0_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + DW230_AXI1_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + DW230_AXI2_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + DW230_AXI0_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_vin + DW230_AXI0_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + DW230_AXI1_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + DW230_AXI2_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t isp_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + ISP_AXI5_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + ISP_AXI4_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + ISP_AXI3_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + ISP_AXI1_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + ISP_AXI1_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_vin + ISP_AXI1_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + ISP_AXI3_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + ISP_AXI4_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + ISP_AXI5_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t sif_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + SIF0_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + SIF1_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + SIF2_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_vin + SIF3_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + SIF0_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_vin + SIF0_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + SIF1_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + SIF2_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_vin + SIF3_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t sifdisp_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_vin + SIF_DISP_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_vin + SIF_DISP_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_vin + SIF_DISP_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t video_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_codec + VIDEO_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "video_priority : %d\n", tmp);
}

static ssize_t video_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_codec + VIDEO_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_codec + VIDEO_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_codec + VIDEO_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t jpeg_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_codec + JPEG_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "jpeg_priority : %d\n", tmp);
}

static ssize_t jpeg_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_codec + JPEG_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_codec + JPEG_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_codec + JPEG_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t gpu_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_gpu + GPU2D_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "gpu_priority : %d\n", tmp);
}

static ssize_t gpu_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_gpu + GPU2D_QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_gpu + GPU3D0__QOS_MODE_OFFSET);
	writel(zero, nocqos->regs_gpu + GPU3D1__QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_gpu + GPU2D_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_gpu + GPU2D_QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_gpu + GPU3D0__QOS_PRIORITY_OFFSET);
	writel(tmp, nocqos->regs_gpu + GPU3D1__QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t dma0_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + DMA0_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "dma0_priority : %d\n", tmp);
}

static ssize_t dma0_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + DMA0_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + DMA0_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_hsio + DMA0_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t emmc_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + EMMC_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "emmc_priority : %d\n", tmp);
}

static ssize_t emmc_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + EMMC_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + EMMC_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_hsio + EMMC_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t gmac_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + GMAC_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "gmac_priority : %d\n", tmp);
}

static ssize_t gmac_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + GMAC_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + GMAC_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_hsio + GMAC_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t sd_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + SD_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "sd_priority : %d\n", tmp);
}

static ssize_t sd_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + SD_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + SD_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_hsio + SD_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t sdio_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + SDIO_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "sdio_priority : %d\n", tmp);
}

static ssize_t sdio_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + SDIO_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + SDIO_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_hsio + SDIO_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t security_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + SECURITY_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "security_priority : %d\n", tmp);
}

static ssize_t security_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + SECURITY_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + SECURITY_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_hsio + SECURITY_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t usb2_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + USB2_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "usb2_priority : %d\n", tmp);
}

static ssize_t usb2_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + USB2_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + USB2_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_hsio + USB2_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t usb3_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + USB3_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "usb3_priority : %d\n", tmp);
}

static ssize_t usb3_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + USB3_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + USB3_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_hsio + USB3_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t etr_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hsio + ETR_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "etr_priority : %d\n", tmp);
}

static ssize_t etr_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hsio + ETR_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hsio + ETR_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_hsio + ETR_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t hifi5_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hifi5 + HIFI5_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "hifi5_priority : %d\n", tmp);
}

static ssize_t hifi5_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hifi5 + HIFI5_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hifi5 + HIFI5_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_hifi5 + HIFI5_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t dsp_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	unsigned int tmp;

	tmp = readl(nocqos->regs_hifi5 + DSP_QOS_PRIORITY_OFFSET);
	tmp >>= 8;
	tmp &= 0x07;

	return sprintf(buf, "dsp_priority : %d\n", tmp);
}

static ssize_t dsp_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int tmp;
	unsigned int zero = 0;
	unsigned int read_ctl_value;

	mutex_lock(&nocqos->ops_lock);;
	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		pr_err("set value %d error,you should set 0~7\n", read_ctl_value);
		mutex_unlock(&nocqos->ops_lock);;
		return count;
	}
	writel(zero, nocqos->regs_hifi5 + DSP_QOS_MODE_OFFSET);
	tmp = readl(nocqos->regs_hifi5 + DSP_QOS_PRIORITY_OFFSET);
	tmp &= ~(0x07 << 8);
	tmp |= (read_ctl_value << 8);
	writel(tmp, nocqos->regs_hifi5 + DSP_QOS_PRIORITY_OFFSET);
	mutex_unlock(&nocqos->ops_lock);;

	return count;
}

static ssize_t all_priority_read_ctl_show(struct device_driver *drv, char *buf)
{
	int len = 0;

	print_noc_qos_warning_info(buf, &len);

	len += cpu_priority_read_ctl_show(drv, buf + len);
	len += bt1120_priority_read_ctl_show(drv, buf + len);
	len += dc8000_priority_read_ctl_show(drv, buf + len);
	len += dw230_priority_read_ctl_show(drv, buf + len);
	len += isp_priority_read_ctl_show(drv, buf + len);
	len += sif_priority_read_ctl_show(drv, buf + len);
	len += sifdisp_priority_read_ctl_show(drv, buf + len);
	len += video_priority_read_ctl_show(drv, buf + len);
	len += jpeg_priority_read_ctl_show(drv, buf + len);
	len += gpu_priority_read_ctl_show(drv, buf + len);
	len += dma0_priority_read_ctl_show(drv, buf + len);
	len += emmc_priority_read_ctl_show(drv, buf + len);
	len += gmac_priority_read_ctl_show(drv, buf + len);
	len += sd_priority_read_ctl_show(drv, buf + len);
	len += sdio_priority_read_ctl_show(drv, buf + len);
	len += security_priority_read_ctl_show(drv, buf + len);
	len += usb2_priority_read_ctl_show(drv, buf + len);
	len += usb3_priority_read_ctl_show(drv, buf + len);
	len += etr_priority_read_ctl_show(drv, buf + len);
	len += hifi5_priority_read_ctl_show(drv, buf + len);
	len += dsp_priority_read_ctl_show(drv, buf + len);

	return len;
}

static ssize_t all_priority_read_ctl_store(struct device_driver *drv,
				   const char *buf, size_t count)
{
	return count;
}

static struct driver_attribute cpu_priority_read_ctl = __ATTR(cpu, 0664,
						   cpu_priority_read_ctl_show, cpu_priority_read_ctl_store);
static struct driver_attribute bpu_priority_read_ctl = __ATTR(bpu, 0664,
						   bpu_priority_read_ctl_show, bpu_priority_read_ctl_store);
static struct driver_attribute bt1120_priority_read_ctl = __ATTR(bt1120, 0664,
						   bt1120_priority_read_ctl_show, bt1120_priority_read_ctl_store);
static struct driver_attribute dc8000_priority_read_ctl = __ATTR(dc8000, 0664,
						   dc8000_priority_read_ctl_show, dc8000_priority_read_ctl_store);
static struct driver_attribute dw230_priority_read_ctl = __ATTR(dw230, 0664,
						   dw230_priority_read_ctl_show, dw230_priority_read_ctl_store);
static struct driver_attribute isp_priority_read_ctl = __ATTR(isp, 0664,
						   isp_priority_read_ctl_show, isp_priority_read_ctl_store);
static struct driver_attribute sif_priority_read_ctl = __ATTR(sif, 0664,
						   sif_priority_read_ctl_show, sif_priority_read_ctl_store);
static struct driver_attribute sifdisp_priority_read_ctl = __ATTR(sifdisp, 0664,
						   sifdisp_priority_read_ctl_show, sifdisp_priority_read_ctl_store);
static struct driver_attribute video_priority_read_ctl = __ATTR(video, 0664,
						   video_priority_read_ctl_show, video_priority_read_ctl_store);
static struct driver_attribute jpeg_priority_read_ctl = __ATTR(jpeg, 0664,
						   jpeg_priority_read_ctl_show, jpeg_priority_read_ctl_store);
static struct driver_attribute gpu_priority_read_ctl = __ATTR(gpu, 0664,
						   gpu_priority_read_ctl_show, gpu_priority_read_ctl_store);
static struct driver_attribute dma0_priority_read_ctl = __ATTR(dma0, 0664,
						   dma0_priority_read_ctl_show, dma0_priority_read_ctl_store);
static struct driver_attribute emmc_priority_read_ctl = __ATTR(emmc, 0664,
						   emmc_priority_read_ctl_show, emmc_priority_read_ctl_store);
static struct driver_attribute gmac_priority_read_ctl = __ATTR(gmac, 0664,
						   gmac_priority_read_ctl_show, gmac_priority_read_ctl_store);
static struct driver_attribute sd_priority_read_ctl = __ATTR(sd, 0664,
						   sd_priority_read_ctl_show, sd_priority_read_ctl_store);
static struct driver_attribute sdio_priority_read_ctl = __ATTR(sdio, 0664,
						   sdio_priority_read_ctl_show, sdio_priority_read_ctl_store);
static struct driver_attribute security_priority_read_ctl = __ATTR(security, 0664,
						   security_priority_read_ctl_show, security_priority_read_ctl_store);
static struct driver_attribute usb2_priority_read_ctl = __ATTR(usb2, 0664,
						   usb2_priority_read_ctl_show, usb2_priority_read_ctl_store);
static struct driver_attribute usb3_priority_read_ctl = __ATTR(usb3, 0664,
						   usb3_priority_read_ctl_show, usb3_priority_read_ctl_store);
static struct driver_attribute etr_priority_read_ctl = __ATTR(etr, 0664,
						   etr_priority_read_ctl_show, etr_priority_read_ctl_store);
static struct driver_attribute hifi5_priority_read_ctl = __ATTR(hifi5, 0664,
						   hifi5_priority_read_ctl_show, hifi5_priority_read_ctl_store);
static struct driver_attribute dsp_priority_read_ctl = __ATTR(dsp, 0664,
						   dsp_priority_read_ctl_show, dsp_priority_read_ctl_store);
static struct driver_attribute all_priority_read_ctl = __ATTR(all, 0664,
						   all_priority_read_ctl_show, all_priority_read_ctl_store);

static struct attribute *priority_read_qctrl_attrs[] = {
	&cpu_priority_read_ctl.attr,
	&bpu_priority_read_ctl.attr,
	&bt1120_priority_read_ctl.attr,
	&dc8000_priority_read_ctl.attr,
	&dw230_priority_read_ctl.attr,
	&isp_priority_read_ctl.attr,
	&sif_priority_read_ctl.attr,
	&sifdisp_priority_read_ctl.attr,
	&video_priority_read_ctl.attr,
	&jpeg_priority_read_ctl.attr,
	&gpu_priority_read_ctl.attr,
	&dma0_priority_read_ctl.attr,
	&emmc_priority_read_ctl.attr,
	&gmac_priority_read_ctl.attr,
	&sd_priority_read_ctl.attr,
	&sdio_priority_read_ctl.attr,
	&security_priority_read_ctl.attr,
	&usb2_priority_read_ctl.attr,
	&usb3_priority_read_ctl.attr,
	&etr_priority_read_ctl.attr,
	&hifi5_priority_read_ctl.attr,
	&dsp_priority_read_ctl.attr,
	&all_priority_read_ctl.attr,
	NULL,
};

static struct attribute_group priority_read_attr_group = {
	.name = "read_priority_qos_ctrl",
	.attrs = priority_read_qctrl_attrs,
};

static const struct attribute_group *noc_qos_attr_groups[] = {
	&priority_write_attr_group,
	&priority_read_attr_group,
	NULL,
};

static int noc_qos_probe(struct platform_device *pdev)
{

	nocqos = devm_kzalloc(&pdev->dev, sizeof(struct noc_qos), GFP_KERNEL);
	if(!nocqos)
	{
		return -ENOMEM;
	}

	nocqos->dev = &pdev->dev;

	nocqos->regs_cpu = devm_platform_ioremap_resource(pdev, 0);
	if(IS_ERR(nocqos->regs_cpu))
		return PTR_ERR(nocqos->regs_cpu);

	nocqos->regs_vin = devm_platform_ioremap_resource(pdev, 1);
	if(IS_ERR(nocqos->regs_vin))
		return PTR_ERR(nocqos->regs_vin);

	nocqos->regs_bpu = devm_platform_ioremap_resource(pdev, 2);
	if(IS_ERR(nocqos->regs_bpu))
		return PTR_ERR(nocqos->regs_bpu);

	nocqos->regs_codec = devm_platform_ioremap_resource(pdev, 3);
	if(IS_ERR(nocqos->regs_codec))
		return PTR_ERR(nocqos->regs_codec);

	nocqos->regs_gpu = devm_platform_ioremap_resource(pdev, 4);
	if(IS_ERR(nocqos->regs_gpu))
		return PTR_ERR(nocqos->regs_gpu);

	nocqos->regs_hsio = devm_platform_ioremap_resource(pdev, 5);
	if(IS_ERR(nocqos->regs_gpu))
		return PTR_ERR(nocqos->regs_gpu);

	nocqos->regs_hifi5 = devm_platform_ioremap_resource(pdev, 6);
	if(IS_ERR(nocqos->regs_gpu))
		return PTR_ERR(nocqos->regs_gpu);

	mutex_init(&nocqos->ops_lock);

	pr_info("noc qos init finished.");

	return 0;
}

static int noc_qos_remove(struct platform_device *pdev)
{
	devm_kfree(&pdev->dev, nocqos);
	nocqos = NULL;

	pr_info("noc qos remove finished.");

	return 0;
}

static const struct of_device_id noc_qos_match[] = {
	{ .compatible = "d-robotics,noc_qos" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, noc_qos_match);

static struct platform_driver noc_qos_driver = {
	.probe	= noc_qos_probe,
	.remove = noc_qos_remove,
	.driver = {
		.name	= "noc_qos",
        .of_match_table = noc_qos_match,
        .groups = noc_qos_attr_groups,
	}
};
module_platform_driver(noc_qos_driver);

MODULE_AUTHOR("Hualun Dong");
MODULE_DESCRIPTION("Horizon X5 Noc Qos Driver");
MODULE_LICENSE("GPL v2");