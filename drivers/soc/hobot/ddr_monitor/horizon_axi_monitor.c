// SPDX-License-Identifier: GPL-2.0+
/*
 * rewrite Horizon Sunrise 5 AXI Monitor Driver
 *
 * Copyright (C) 2023 Beijing Horizon Robotics Co.,Ltd.
 * Dinggao Pan <dinggao.pan@horizon.cc>
 *
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/notifier.h>

#include "horizon_axi_monitor.h"

BLOCKING_NOTIFIER_HEAD(axi_monitor_chain_head);
EXPORT_SYMBOL_GPL(axi_monitor_chain_head);

#define AXI_MON_MINOR_NUM 1
#define PORT_NUM 5

static struct class *axi_mon_class;

#define AXI_MON_READ	_IOWR('m', 0, reg_t)
#define AXI_MON_WRITE	_IOW('m', 1, reg_t)
#define AXI_MON_CUR	_IOWR('m', 2, struct axi_mon_rec)
#define AXI_MON_SAMPLE_CONFIG_SET  _IOW('m', 3, struct axi_mon_cfg)

#define TOTAL_RECORD_NUM 400
#define TOTAL_RESULT_SIZE (400*1024)

struct axi_mon_rec* ddr_info = NULL;
struct axi_mon_rec* ddr_info_bc = NULL;
uint32_t cur_idx = 0;
uint32_t g_sample_number = 0;
uint32_t g_rec_num = 0;
uint32_t mon_period = 0;
module_param(mon_period, uint, 0644);

typedef struct axi_mon_cfg {
       uint32_t sample_period;
       uint32_t sample_number;
} axi_mon_cfg;

struct axi_portdata_s {
	uint32_t wdata_num;
	uint32_t rdata_num;
#ifdef CONFIG_HOBOT_XJ3
	uint32_t waddr_num;
	uint32_t waddr_cyc;
	uint32_t waddr_latency;
	uint32_t raddr_num;
	uint32_t raddr_cyc;
	uint32_t raddr_latency;
	uint32_t max_rtrans_cnt;
	uint32_t max_rvalid_cnt;
	uint32_t acc_rtrans_cnt;
	uint32_t min_rtrans_cnt;

	uint32_t max_wtrans_cnt;
	uint32_t max_wready_cnt;
	uint32_t acc_wtrans_cnt;
	uint32_t min_wtrans_cnt;
#else
	uint32_t rd_max_latency;
	uint32_t rd_min_latency;
	uint32_t wr_max_latency;
	uint32_t wr_min_latency;
	uint32_t rd_time_total;
	uint32_t wr_time_total;
	uint32_t rd_max_ost;
	uint32_t wr_max_ost;
	uint32_t rd_stop_ost;
	uint32_t wr_stop_ost;
#endif
};

struct axi_mon_rec {
	unsigned long long curtime;
	struct axi_portdata_s portdata[PORT_NUM];
	uint32_t rd_cmd_num;
	uint32_t wr_cmd_num;
#ifdef CONFIG_HOBOT_XJ3
	uint32_t mwr_cmd_num;
	uint32_t rdwr_swi_num;
	uint32_t war_haz_num;
	uint32_t raw_haz_num;
	uint32_t waw_haz_num;
	uint32_t act_cmd_num;
	uint32_t act_cmd_rd_num;
	uint32_t per_cmd_num;
	uint32_t per_cmd_rdwr_num;
#endif
	uint32_t real_period;
};

static int axi_monitor_chain_notify(struct notifier_block *nb, unsigned long mode, void *_unused)
{
	struct axi_monitor *axi_mon = container_of(&(*nb), struct axi_monitor, nb);
	axi_mon->axi_clk_hz = mode * 1000000;
	return 0;
}

static struct notifier_block axi_monitor_nb = {
	.notifier_call = axi_monitor_chain_notify,
};


static int inline axi_mon_alloc(void)
{
	if (!ddr_info) {
		/* allocate double size, below half for ddr_info bouncing buffer */
		ddr_info = vmalloc(sizeof(struct axi_mon_rec) * TOTAL_RECORD_NUM * 2);
		if (ddr_info == NULL) {
			pr_err("failed to allocate ddr_info buffer, size:%lx\n",
				sizeof(struct axi_mon_rec) * TOTAL_RECORD_NUM * 2);
			return -ENOMEM;
		}

		/*
		 * we have to use a boucing buffer since ddr_info can't be copied to used
		 * in atomic context in axi_mon_read.
		 */
		ddr_info_bc = ddr_info + TOTAL_RECORD_NUM;

		cur_idx = 0;
		g_rec_num = 0;
	}

	return 0;
}

static int axi_mon_open(struct inode *inodep, struct file *filep)
{
	struct axi_monitor *axi_mon;
	int ret;

	axi_mon = container_of(inodep->i_cdev, struct axi_monitor, cdev);
	filep->private_data = axi_mon;

	mutex_lock(&axi_mon->ops_mutex);
	ret = axi_mon_alloc();
	mutex_unlock(&axi_mon->ops_mutex);
	return ret;
}

static int axi_mon_release(struct inode *inodep, struct file *filep)
{
	struct axi_monitor *axi_mon;
	axi_mon = (struct axi_monitor *)filep->private_data;
	axi_mon_start(axi_mon, 0);

	return 0;
}

static ssize_t axi_mon_read(struct file *filep, char *user_bufp, size_t len, loff_t *poff)
{
	struct axi_monitor *axi_mon;
	unsigned long flags;
	long ret;
	long offset = 0;
	int rec_num;

	axi_mon = (struct axi_monitor *)filep->private_data;

	wait_event_interruptible(axi_mon->wq_head, g_rec_num >= g_sample_number);
	mutex_lock(&axi_mon->ops_mutex);
	spin_lock_irqsave(&axi_mon->ops_lock, flags);
	if (ddr_info == NULL || ddr_info_bc == NULL) {
		spin_unlock_irqrestore(&axi_mon->ops_lock, flags);
		mutex_unlock(&axi_mon->ops_mutex);
		return -EFAULT;
	}
	if (cur_idx >= g_sample_number) {
		memcpy(ddr_info_bc, &ddr_info[cur_idx - g_sample_number],
			g_sample_number * sizeof(struct axi_mon_rec));
	} else {
		offset = (g_sample_number - cur_idx) * sizeof(struct axi_mon_rec);
		memcpy((void *)ddr_info_bc, &ddr_info[TOTAL_RECORD_NUM - (g_sample_number - cur_idx)], offset);
		memcpy((void *)ddr_info_bc + offset, &ddr_info[0], cur_idx * sizeof(struct axi_mon_rec));
	}
	rec_num = g_sample_number;
	g_rec_num = 0;
	spin_unlock_irqrestore(&axi_mon->ops_lock, flags);

	ret = copy_to_user(user_bufp, ddr_info_bc, rec_num * sizeof(struct axi_mon_rec));
	if (ret) {
		pr_err("%s:%d copy to user error :%ld\n", __func__, __LINE__, ret);
		mutex_unlock(&axi_mon->ops_mutex);
		return -EFAULT;
	}
	mutex_unlock(&axi_mon->ops_mutex);
	ret = rec_num * sizeof(struct axi_mon_rec);

	return ret;
}

static ssize_t axi_mon_write(struct file *filep, const char *user_bufp, size_t len, loff_t *poff)
{
	pr_debug("\n");

	return 0;
}

static long axi_mon_ioctl(struct file *filep, uint32_t cmd, unsigned long arg)
{
	struct axi_monitor *axi_mon;
	void __iomem *iomem;
	uint32_t temp = 0;
	int cur = 0;
	reg_t reg;
	unsigned long flags;
	struct axi_mon_rec tmp;
	struct axi_mon_cfg sample_config;

	axi_mon = (struct axi_monitor *)filep->private_data;
	iomem = axi_mon->base;
	sample_config.sample_period = 1;
	sample_config.sample_number = 50;

	if ( NULL == iomem ) {
		pr_err("axi_mon no iomem\n");
		return -EINVAL;
	}

	switch (cmd) {
	case AXI_MON_READ:
		if (!arg) {
			pr_err("reg read error, reg should not be NULL");
			return -EINVAL;
		}
		if (copy_from_user((void *)&reg, (void __user *)arg, sizeof(reg))) {
			pr_err("reg read error, copy data from user failed\n");
			return -EINVAL;
		}
		reg.value = readl(iomem + reg.offset);
		if ( copy_to_user((void __user *)arg, (void *)&reg, sizeof(reg)) ) {
			pr_err("reg read error, copy data to user failed\n");
			return -EINVAL;
		}
		break;
	case AXI_MON_WRITE:
		if ( !arg ) {
			pr_err("reg write error, reg should not be NULL");
			return -EINVAL;
		}
		if (copy_from_user((void *)&reg, (void __user *)arg, sizeof(reg))) {
			pr_err("reg write error, copy data from user failed\n");
			return -EINVAL;
		}
		writel(reg.value, iomem + reg.offset );
		break;
	case AXI_MON_CUR:
		if (!arg) {
			pr_err("get cur error\n");
			return -EINVAL;
		}
		mutex_lock(&axi_mon->ops_mutex);
		spin_lock_irqsave(&axi_mon->ops_lock, flags);
		cur  = (cur_idx - 1 + TOTAL_RECORD_NUM) % TOTAL_RECORD_NUM;
		if (ddr_info == NULL || ddr_info_bc == NULL) {
			spin_unlock_irqrestore(&axi_mon->ops_lock, flags);
			mutex_unlock(&axi_mon->ops_mutex);
			return -EFAULT;
		}
		memcpy(&tmp, (void *)(ddr_info + cur), sizeof(struct axi_mon_rec));
		spin_unlock_irqrestore(&axi_mon->ops_lock, flags);
		if (copy_to_user((void __user *)arg, &tmp, sizeof(struct axi_mon_rec))) {
			pr_err("get cur error, copy data to user failed\n");
			mutex_unlock(&axi_mon->ops_mutex);
			return -EINVAL;
		}
		mutex_unlock(&axi_mon->ops_mutex);
		break;
	case AXI_MON_SAMPLE_CONFIG_SET:
		if (!arg) {
			dev_err(&axi_mon->pdev->dev,
					"sampletime set arg should not be NULL\n");
			return -EINVAL;
		}
		if (copy_from_user((void *)&sample_config,
					(void __user *)arg, sizeof(axi_mon_cfg))) {
			dev_err(&axi_mon->pdev->dev,
					"sampletime set copy_from_user failed\n");
			return -EINVAL;
		}
		/* sample_period's unit is ms */
		temp = sample_config.sample_period;
		g_sample_number = sample_config.sample_number;
		if (g_sample_number > TOTAL_RECORD_NUM)
			g_sample_number = TOTAL_RECORD_NUM;
		mon_period = temp;
		axi_mon_start(axi_mon, 1);

		break;
	default:

		break;
	}
	return 0;
}

struct file_operations axi_mon_fops = {
	.owner			= THIS_MODULE,
	.open			= axi_mon_open,
	.read			= axi_mon_read,
	.write			= axi_mon_write,
	.release		= axi_mon_release,
	.unlocked_ioctl = axi_mon_ioctl,
};


static inline uint32_t axi_mon_readl(struct axi_monitor *axi_mon, uint32_t offset) {
	return readl(axi_mon->base + offset);
}

static inline void axi_mon_writel(struct axi_monitor *axi_mon, uint32_t offset, uint32_t val) {
	writel(val, axi_mon->base + offset);
}

static void axi_calculate_statistics(struct axi_monitor *axi_mon)
{
	uint64_t apb_cycles, axi_cycles, rd_bw_raw, wr_bw_raw, full_bw_raw,
			rd_cmd_num, wr_cmd_num, rd_max_latency, rd_min_latency, wr_max_latency,
			wr_min_latency, rd_time_total, wr_time_total, rd_max_ost, wr_max_ost, 
			rd_stop_ost, wr_stop_ost;
	uint32_t reg_ok_to, time_low, time_high, int_status;
	struct list_head *port_entry;
	struct axi_port *port;

	/* Poll for register Okay */
	reg_ok_to = 500000; /* 100ms reg stable time */
	while (reg_ok_to) {
		int_status = axi_mon_readl(axi_mon, INT_STATUS);
		if (int_status & REG_OK) {
			break;
		}
		udelay(100);
		reg_ok_to -= 100;
	}

	time_low = axi_mon_readl(axi_mon, TIME_DAT_LOW);
	time_high = axi_mon_readl(axi_mon, TIME_DAT_HIGH);
	apb_cycles = (uint64_t)(((time_high & SESSION_TIME_HIGH_MASK) << 32) | \
							   time_low);
	axi_cycles = axi_mon->axi_clk_hz / axi_mon->apb_clk_hz * apb_cycles;

	/* Actual time in millisecs */
	axi_mon->actual_time = (apb_cycles * 1000) / axi_mon->apb_clk_hz;
	if (axi_mon->port_status.bits.port_overflow) {
		dev_dbg(&axi_mon->pdev->dev,
				 "Performance counter overflowed, timer stopped early\n");
	}

	dev_dbg(&axi_mon->pdev->dev,
			 "Monitor session ended, total time: %llu millisesc\n",
			  axi_mon->actual_time);

	mutex_lock(&axi_mon->ops_mutex);
	full_bw_raw = AXI_DATA_WIDTH * axi_cycles / 8;
	axi_mon->full_bw = full_bw_raw / axi_mon->actual_time * 1000;
	if (ddr_info != NULL) {
		ddr_info[cur_idx].curtime = ktime_to_ms(ktime_get());
		ddr_info[cur_idx].real_period = axi_mon->actual_time;
		ddr_info[cur_idx].rd_cmd_num = full_bw_raw;
		ddr_info[cur_idx].wr_cmd_num = full_bw_raw;
	}
	list_for_each(port_entry, &axi_mon->mon_ports_list) {
		port = list_entry(port_entry, struct axi_port, entry);
		rd_bw_raw = axi_mon_readl(axi_mon, PORT_RD_BW(port->idx));
		wr_bw_raw = axi_mon_readl(axi_mon, PORT_WR_BW(port->idx));
		rd_cmd_num = axi_mon_readl(axi_mon, PORT_RD_CMD_NUM(port->idx));
		wr_cmd_num = axi_mon_readl(axi_mon, PORT_WR_CMD_NUM(port->idx));
		rd_max_latency = axi_mon_readl(axi_mon, PORT_RD_MAX_LATENCY(port->idx));
		rd_min_latency = axi_mon_readl(axi_mon, PORT_RD_MIN_LATENCY(port->idx));
		wr_max_latency = axi_mon_readl(axi_mon, PORT_WR_MAX_LATENCY(port->idx));
		wr_min_latency = axi_mon_readl(axi_mon, PORT_WR_MIN_LATENCY(port->idx));
		rd_time_total = axi_mon_readl(axi_mon, PORT_RD_TIME_TOTAL(port->idx));
		wr_time_total = axi_mon_readl(axi_mon, PORT_WR_TIME_TOTAL(port->idx));
		rd_max_ost = (axi_mon_readl(axi_mon, PORT_MAX_OST(port->idx)) >> 16 & 0xffff);
		wr_max_ost = (axi_mon_readl(axi_mon, PORT_MAX_OST(port->idx)) & 0xffff);
		rd_stop_ost = (axi_mon_readl(axi_mon, PORT_END_OST(port->idx)) >> 16 & 0xffff);
		wr_stop_ost = (axi_mon_readl(axi_mon, PORT_END_OST(port->idx)) & 0xffff);

		port->rd_bw = rd_bw_raw / axi_mon->actual_time * 1000;
		port->wr_bw = wr_bw_raw / axi_mon->actual_time * 1000;
		port->rd_bw_percent = port->rd_bw * 100 / axi_mon->full_bw;
		port->wr_bw_percent = port->wr_bw * 100 / axi_mon->full_bw;
		if (ddr_info != NULL) {
			ddr_info[cur_idx].portdata[port->idx].rdata_num = rd_bw_raw;
			ddr_info[cur_idx].portdata[port->idx].wdata_num = wr_bw_raw;
			ddr_info[cur_idx].rd_cmd_num = rd_cmd_num;
			ddr_info[cur_idx].wr_cmd_num = wr_cmd_num;
			ddr_info[cur_idx].portdata[port->idx].rd_max_latency = rd_max_latency;
			ddr_info[cur_idx].portdata[port->idx].rd_min_latency = rd_min_latency;
			ddr_info[cur_idx].portdata[port->idx].wr_max_latency = wr_max_latency;
			ddr_info[cur_idx].portdata[port->idx].wr_min_latency = wr_min_latency;
			ddr_info[cur_idx].portdata[port->idx].rd_time_total = rd_time_total;
			ddr_info[cur_idx].portdata[port->idx].wr_time_total = wr_time_total;
			ddr_info[cur_idx].portdata[port->idx].rd_max_ost = rd_max_ost;
			ddr_info[cur_idx].portdata[port->idx].wr_max_ost = wr_max_ost;
			ddr_info[cur_idx].portdata[port->idx].rd_stop_ost = rd_stop_ost;
			ddr_info[cur_idx].portdata[port->idx].wr_stop_ost = wr_stop_ost;
		}
		dev_dbg(&axi_mon->pdev->dev,
			 "port%d-%s rec-%d RDBW:%#x, WRBW:%#x\n",
			  port->idx, port->name, cur_idx,
			  ddr_info[cur_idx].portdata[port->idx].rdata_num,
			  ddr_info[cur_idx].portdata[port->idx].wdata_num);
	}
	axi_mon->busy = IDLE;

	cur_idx = ((cur_idx + 1) % TOTAL_RECORD_NUM);
	g_rec_num++;
	mutex_unlock(&axi_mon->ops_mutex);

	if (g_rec_num >= g_sample_number) {
		wake_up_interruptible(&axi_mon->wq_head);
	}

	axi_mon_start(axi_mon, 1);

}

void axi_mon_start(struct axi_monitor *axi_mon, uint32_t start)
{
	struct device *dev = &axi_mon->pdev->dev;
	if (start) {
		if (mon_period != 0) {
			axi_mon->session_time = mon_period;
		}
		if (axi_mon->session_time == 0) {
			dev_dbg(dev, "Using default session time 1000 millisecs\n");
			axi_mon->session_time = 1000;
		}
		dev_dbg(dev, "Session start for %llu millisec\n", axi_mon->session_time);
		mutex_lock(&axi_mon->ops_mutex);
		axi_mon->busy = BUSY;
		mutex_unlock(&axi_mon->ops_mutex);
#if IS_ENABLED(CONFIG_HOBOT_AXI_ADVANCED)
		/* TODO: Add advanced configurations here */
#endif
	hrtimer_start(&axi_mon->hrtimer, ktime_set(axi_mon->session_time / 1000, (axi_mon->session_time % 1000) * 1000000),HRTIMER_MODE_REL);
	} else {
		hrtimer_cancel(&axi_mon->hrtimer);
		mutex_lock(&axi_mon->ops_mutex);
		axi_mon->busy = IDLE;
		if (ddr_info != NULL) {
			vfree(ddr_info);
			ddr_info = NULL;
			ddr_info_bc = NULL;
		}
		mutex_unlock(&axi_mon->ops_mutex);
	}
	axi_mon_writel(axi_mon, START, start);

}

static irqreturn_t axi_monitor_isr(int irq, void *data)
{
	struct axi_monitor *axi_mon = (struct axi_monitor *)data;
	unsigned long irq_flags;

	spin_lock_irqsave(&axi_mon->irq_lock, irq_flags);
	/* End axi monitor session */
	axi_mon_writel(axi_mon, START, 0x0);
	axi_mon->int_status = axi_mon_readl(axi_mon, INT_STATUS);
	axi_mon_writel(axi_mon, INT_STATUS, axi_mon->int_status);
	axi_mon->port_to_status.port_to_status_val = axi_mon_readl(axi_mon, PORT_STATUS1);
	axi_mon->port_status.port_status_val = axi_mon_readl(axi_mon, PORT_STATUS2);
	spin_unlock_irqrestore(&axi_mon->irq_lock, irq_flags);

	hrtimer_cancel(&axi_mon->hrtimer);
	tasklet_schedule(&axi_mon->tasklet);
	return IRQ_HANDLED;
}
static void axi_mon_tasklet(struct tasklet_struct *t)
{
	struct axi_monitor *axi_mon = from_tasklet(axi_mon, t, tasklet);
	axi_mon_writel(axi_mon, START, 0x0);
	axi_calculate_statistics(axi_mon);
}

static enum hrtimer_restart monitor_session_end_cb(struct hrtimer *hrtimer)
{
	struct axi_monitor *axi_mon = container_of(hrtimer, struct axi_monitor, hrtimer);
	axi_mon_writel(axi_mon, START, 0x0);
	axi_calculate_statistics(axi_mon);
	return HRTIMER_NORESTART;
}

static int axi_mon_parse_of(struct platform_device *pdev, struct axi_monitor *axi_mon)
{
	struct device_node *np;
	struct device *dev = &pdev->dev;
	struct axi_port *port;
	int32_t i;

	if (!axi_mon) {
		return -ENODEV;
	}
	np = dev->of_node;

	axi_mon->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(axi_mon->base)) {
		dev_err(dev, "Unable to ioremap register\n");
		return PTR_ERR(axi_mon->base);
	}

	if (of_property_read_u32(np, "axi-num", &axi_mon->ports_num))
	{
		dev_err(dev, "axi-num parse failed!\n");
		return -EINVAL;
	}
	dev_info(dev, "Got axi-port num:%d\n", axi_mon->ports_num);
	for (i = 0; i < axi_mon->ports_num; i++) {
		port = kzalloc(sizeof(struct axi_port), GFP_KERNEL);
		if (port == NULL) {
			return -ENOMEM;
		}
		if (of_property_read_string_index(np, "axi-names", i, &port->name)) {
			dev_err(dev, "Mismatched axi-num and axi-names in devicetree!\n");
			return -EINVAL;
		}
		port->idx = i;
		INIT_LIST_HEAD(&port->entry);
		spin_lock(&axi_mon->ops_lock);
		list_add_tail(&port->entry, &axi_mon->mon_ports_list);
		spin_unlock(&axi_mon->ops_lock);
		dev_info(dev, "Got new axi-port:%s, idx:%d\n", port->name, port->idx);
		/* Enable R/W bandwidth stat by default */
		axi_mon_writel(axi_mon, PORT_EVENT_EN(i),
					   (RD_BW_EN | WR_BW_EN));
	}

	return 0;
}

static int axi_monitor_probe(struct platform_device *pdev)
{
	int ret;
	struct axi_monitor *axi_mon;
	struct device *dev = &pdev->dev;

	axi_mon = devm_kzalloc(dev, sizeof(struct axi_monitor), GFP_KERNEL);
	if (axi_mon == NULL) {
		dev_err(dev, "Out of memory\n");
		return -ENOMEM;
	}

	axi_mon->axi_clk_hz = 800000000;
	dev_info(dev, "AXI-Clk: %lluHz\n", axi_mon->axi_clk_hz);

	axi_mon->nb = axi_monitor_nb;
	blocking_notifier_chain_register(&axi_monitor_chain_head, &axi_mon->nb);

	axi_mon->apb_clk_hz = 200000000;
	dev_info(dev, "APB-Clk: %lluHz\n", axi_mon->apb_clk_hz);

	/* session_time is stored in millisecs */
	axi_mon->session_time_max = ((SESSION_TIME_MAX_CNT / axi_mon->apb_clk_hz) \
								 * 1000);
	dev_info(dev, "Maximum session time %llu millisecs\n",
			 axi_mon->session_time_max);

	INIT_LIST_HEAD(&axi_mon->mon_ports_list);
	init_waitqueue_head(&axi_mon->wq_head);
	mutex_init(&axi_mon->ops_mutex);
	spin_lock_init(&axi_mon->ops_lock);
	spin_lock_init(&axi_mon->ops_lock);
	spin_lock_init(&axi_mon->irq_lock);
	tasklet_setup(&axi_mon->tasklet, axi_mon_tasklet);
	hrtimer_init(&axi_mon->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_HARD);
	axi_mon->hrtimer.function = monitor_session_end_cb;
	ret = axi_mon_parse_of(pdev, axi_mon);
	if (ret < 0) {
		dev_err(dev, "Failed to parse device tree!\n");
		goto error;
	}

	dev_set_name(dev, "axi-monitor");
	axi_mon->pdev = pdev;
	axi_mon->irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(dev, axi_mon->irq, axi_monitor_isr,
				IRQF_TRIGGER_HIGH, dev_name(dev), axi_mon);
	if (ret < 0) {
		dev_err(dev, "Could not request IRQ\n");
		goto error;
	}

	axi_mon_writel(axi_mon, INT_EN, INT_ALL);

	/* Fill in the data structures */
	if (axi_mon_class == NULL) {
		axi_mon_class = class_create(THIS_MODULE, dev_name(dev));
		if (IS_ERR(axi_mon_class)) {
			dev_err(dev, "Unable to create class\n");
			ret = PTR_ERR(axi_mon_class);
			goto error;
		}
	}

	if(alloc_chrdev_region(&axi_mon->devt, 0, AXI_MON_MINOR_NUM, dev_name(dev))) {
		goto err_alloc;
	}
	cdev_init(&axi_mon->cdev, &axi_mon_fops);
	axi_mon->cdev.owner = THIS_MODULE;

	ret = cdev_add(&axi_mon->cdev, axi_mon->devt, AXI_MON_MINOR_NUM);
	if (ret < 0) {
		dev_err(dev, "Cannot add cdev\n");
		goto err_cdev_add;
	}
	axi_mon->cdev_dev = device_create(axi_mon_class,
			NULL, axi_mon->devt, NULL, "axi_mon");
	if (IS_ERR(axi_mon->cdev_dev)) {
		dev_err(dev, "device_create failed\n");
		return PTR_ERR_OR_ZERO(axi_mon->cdev_dev);
	}
#if IS_ENABLED(CONFIG_HOBOT_AXI_MONITOR_DEBUGFS)
	hb_axi_monitor_create_debugfs(axi_mon);
#endif

	platform_set_drvdata(pdev, axi_mon);
	dev_info(dev, "DDR Perf Monitor probed successfully.\n");

	return 0;

err_alloc:
	unregister_chrdev_region(axi_mon->devt, 1);
err_cdev_add:
	cdev_del(&axi_mon->cdev);
error:
	hrtimer_cancel(&axi_mon->hrtimer);
	class_destroy(axi_mon_class);
	return ret;
}

static int axi_monitor_remove(struct platform_device *pdev)
{
	struct axi_monitor *axi_mon = platform_get_drvdata(pdev);

	if (axi_mon == NULL)
		return 0;

	hrtimer_cancel(&axi_mon->hrtimer);
#if IS_ENABLED(CONFIG_HOBOT_AXI_MONITOR_DEBUGFS)
	debugfs_remove_recursive(axi_mon->debugfs_root);
#endif
	cdev_device_del(&axi_mon->cdev, &axi_mon->pdev->dev);
	if (axi_mon_class != NULL) {
		class_destroy(axi_mon_class);
		axi_mon_class = NULL;
	}

	return 0;
}

static const struct of_device_id axi_monitor_match[] = {
	{ .compatible = "d-robotics,axi-monitor" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, axi_monitor_match);

static struct platform_driver axi_monitor_driver = {
	.probe	= axi_monitor_probe,
	.remove = axi_monitor_remove,
	.driver = {
		.name	= "axi_monitor",
		.of_match_table = axi_monitor_match,
	}
};
module_platform_driver(axi_monitor_driver);

MODULE_AUTHOR("Dinggao Pan");
MODULE_DESCRIPTION("Horizon X5 AXI Perf Monitor Driver");
MODULE_LICENSE("GPL v2");
