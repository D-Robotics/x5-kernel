// SPDX-License-Identifier: GPL-2.0-only

#include <linux/clk.h>
#include <linux/devfreq-event.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/irq.h>
#include <linux/pm_runtime.h>

#include "../x5-smcc.h"

/* dfi Monitor registers */
#define DFI_MON_BASE_ADDR             (priv->regs_0)
#define DFI_PM_WIND_CNT_CTRL          (DFI_MON_BASE_ADDR + 0x00)
#define DFI_PM_WIND_CNT_LOW           (DFI_MON_BASE_ADDR + 0x04)
#define DFI_PM_WIND_CNT_HIGH          (DFI_MON_BASE_ADDR + 0x08)
#define DFI_PM_EVENT_ACC_SEL          (DFI_MON_BASE_ADDR + 0x0C)
#define DFI_PM_SP_EVENT_CTRL          (DFI_MON_BASE_ADDR + 0x10)
#define DFI_PM_SP_EVENT_START_CTRL    (DFI_MON_BASE_ADDR + 0x14)
#define DFI_PM_SP_EVENT_START_CTRL_0  (DFI_MON_BASE_ADDR + 0x18)
#define DFI_PM_SP_EVENT_START_CTRL_1  (DFI_MON_BASE_ADDR + 0x1C)
#define DFI_PM_SP_EVENT_DEFINED_PAT_0 (DFI_MON_BASE_ADDR + 0x20)
#define DFI_PM_SP_EVENT_SNAPSHOT_CTRL (DFI_MON_BASE_ADDR + 0x24)
#define DFI_PM_SP_EVENT_FIFO_RDDATA_0 (DFI_MON_BASE_ADDR + 0x28)
#define DFI_PM_SP_EVENT_FIFO_RDDATA_1 (DFI_MON_BASE_ADDR + 0x2C)
#define DFI_PM_SP_EVENT_FIFO_RDDATA_2 (DFI_MON_BASE_ADDR + 0x30)
#define DFI_PM_SP_EVENT_FIFO_RDDATA_3 (DFI_MON_BASE_ADDR + 0x34)
#define DFI_PM_SP_EVENT_STATUS        (DFI_MON_BASE_ADDR + 0x38)
#define DFI_PM_EVENT_N_CTRL           (DFI_MON_BASE_ADDR + 0x3C)
#define DFI_PM_EVENT_N_START_CTRL     (DFI_MON_BASE_ADDR + 0x40)
#define DFI_PM_EVENT_N_START_CTRL_0   (DFI_MON_BASE_ADDR + 0x44)
#define DFI_PM_EVENT_N_START_CTRL_1   (DFI_MON_BASE_ADDR + 0x48)
#define DFI_PM_EVENT_N_END_CTRL       (DFI_MON_BASE_ADDR + 0x4C)
#define DFI_PM_EVENT_N_END_CTRL_0     (DFI_MON_BASE_ADDR + 0x50)
#define DFI_PM_EVENT_N_END_CTRL_1     (DFI_MON_BASE_ADDR + 0x54)
#define DFI_PM_EVENT_N_DEFINED_PAT_0  (DFI_MON_BASE_ADDR + 0x58)
#define DFI_PM_EVENT_N_DEFINED_PAT_1  (DFI_MON_BASE_ADDR + 0x5C)
#define DFI_PM_EVENT_N_COUNTER_LOW    (DFI_MON_BASE_ADDR + 0x60)
#define DFI_PM_EVENT_N_COUNTER_HIGH   (DFI_MON_BASE_ADDR + 0x64)
#define DFI_PM_DDR_SYS_INFO           (DFI_MON_BASE_ADDR + 0x68)
#define DFI_PM_DDR_MRR_CTRL           (DFI_MON_BASE_ADDR + 0x6C)
#define DFI_PM_MRR_DATA               (DFI_MON_BASE_ADDR + 0x70)
#define DFI_PM_READ_HANG_CHK          (DFI_MON_BASE_ADDR + 0x74)
#define DFI_PM_MASK_BYTE_CNT_0        (DFI_MON_BASE_ADDR + 0x78)
#define DFI_PM_MASK_BYTE_CNT_1        (DFI_MON_BASE_ADDR + 0x7C)
#define DFI_PM_INT_STATUS             (DFI_MON_BASE_ADDR + 0x80)
#define DFI_PM_INT_EN                 (DFI_MON_BASE_ADDR + 0x84)
#define DFI_PM_EVENT_CNT_OVFLOW_0     (DFI_MON_BASE_ADDR + 0x88)
#define DFI_PM_EVENT_CNT_OVFLOW_1     (DFI_MON_BASE_ADDR + 0x8c)
#define DFI_PM_EVENT_CNT_OVFLOW_2     (DFI_MON_BASE_ADDR + 0x90)
#define DFI_PM_EVENT_CNT_OVFLOW_3     (DFI_MON_BASE_ADDR + 0x94)
#define DFI_PM_EVENT_END_0            (DFI_MON_BASE_ADDR + 0x98)
#define DFI_PM_EVENT_END_1            (DFI_MON_BASE_ADDR + 0x9c)
#define DFI_PM_EVENT_END_2            (DFI_MON_BASE_ADDR + 0xa0)
#define DFI_PM_EVENT_END_3            (DFI_MON_BASE_ADDR + 0xa4)
#define DFI_PM_OTHER_CNT_OVFLOW       (DFI_MON_BASE_ADDR + 0xa8)
#define DFI_PM_OTHER_EVENT_END        (DFI_MON_BASE_ADDR + 0xac)

/* ddr int status */
#define DDR_INTR_BASE			(priv->regs_1)
#define DDR_INTR_STATUS			(DDR_INTR_BASE + 0xc)
#define DDR_INT_SOURCE_DFI_BIT		BIT(0)

#define DDR_BURST_LENGTH (16)
#define DDR_DATA_WIDTH   (32)

/* ddr subsystem reset */
#define DFI_SWR_BIT	BIT(0)
#define DFI_POR_BIT	BIT(3)
/*
 * The dfi controller can monitor DDR load. It has an upper and lower threshold
 * for the operating points. Whenever the usage leaves these bounds an event is
 * generated to indicate the DDR frequency should be changed.
 */
struct x5_dfi {
	struct devfreq_event_dev *edev;
	struct devfreq_event_desc *desc;
	struct device *dev;
	int    irq;
	struct clk *apb_clk;
	u32    apb_clk_rate;
	u32    interval;
	void __iomem *regs_0;
	void __iomem *regs_1;
	uint64_t write_num;
	uint64_t read_num;
};

static void dfi_monitor_init(struct x5_dfi *priv);

static int x5_get_curr_ddr_rate(unsigned long *rate)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HORIZON_SIP_DDR_DVFS_GET, 0, 0, 0, 0, 0, 0, 0, &res);
	/* Result should be strictly positive */
	if ((long)res.a0 <= 0)
		return -ENODEV;

	*rate = res.a0;
	return 0;
}

static int x5_dfi_disable(struct devfreq_event_dev *edev)
{
	return 0;
}

static int x5_dfi_enable(struct devfreq_event_dev *edev)
{
	struct x5_dfi *priv = devfreq_event_get_drvdata(edev);

	writel_relaxed(0x1, DFI_PM_WIND_CNT_CTRL);
	return 0;
}

static int x5_dfi_set_event(struct devfreq_event_dev *edev)
{
	return 0;
}

static int x5_dfi_get_event(struct devfreq_event_dev *edev,
				  struct devfreq_event_data *edata)
{
	struct x5_dfi *priv = devfreq_event_get_drvdata(edev);
	unsigned long cur_rate;
	int ret;

	/* need to get current ddr rate to calculator bw usage */
	ret = x5_get_curr_ddr_rate(&cur_rate);
	if (ret < 0)
		return 0;

	dev_dbg(priv->dev, "rate: %ld, current rd: %lld\n", cur_rate, priv->read_num);
	edata->load_count = (priv->read_num)/(cur_rate * 4 * 10000);
	edata->total_count = 100;
	dev_dbg(priv->dev, "percent=%ld\n", edata->load_count);

	return 0;
}

static void dfi_monitor_restart(struct x5_dfi *priv)
{
	writel_relaxed(0x0, DFI_PM_WIND_CNT_CTRL);
	dfi_monitor_init(priv);
	writel_relaxed(0x1, DFI_PM_WIND_CNT_CTRL);
}

static void ddrmon_status_chk(struct x5_dfi *priv)
{
	uint32_t status;
	uint64_t cnt_high, cnt_low;
	uint64_t window_time, window_cnt_value, event_cnt_value;

	dev_dbg(priv->dev, "DDRMON Status Chk\n");
	//check status
	status = readl_relaxed(DFI_PM_EVENT_N_COUNTER_HIGH);
	if (status != 0)
		dev_dbg(priv->dev, "DDRMON EVENT 0&1 COUNT OVERFLOW is %x\n", (status >> 31));

	status = readl_relaxed(DFI_PM_OTHER_EVENT_END);
	if ((status) != 0x1)
		dev_dbg(priv->dev, "DDRMON SP EVENT END FLAG is %x\n", (status >> 31));

	status = readl_relaxed(DFI_PM_OTHER_CNT_OVFLOW);
	if ((status & 0x1) == 0x1)
		dev_dbg(priv->dev, "DDRMON WINDOW COUTN IS OVERFLOW\n");
	else
		dev_dbg(priv->dev, "DDRMON WINDOW COUTN IS NOT OVERFLOW\n");

	status = (status >> 2);
	if ((status & 0x1) == 0x1)
		dev_dbg(priv->dev, "DDRMON MASK BYTE CNT OVERFLOW is %x\n", (status & 0x1));

	//data static
	status   = readl_relaxed(DFI_PM_WIND_CNT_HIGH);
	cnt_high = (status & 0xFF);
	status   = readl_relaxed(DFI_PM_WIND_CNT_LOW);
	cnt_low  = status;
	dev_dbg(priv->dev, "DDRMON_WIND_HIGH=0x%llx\n", cnt_high);
	dev_dbg(priv->dev, "DDRMON WIND_LOW=0x%llx\n", cnt_low);

	window_cnt_value = (cnt_high << 32) | cnt_low;
	dev_dbg(priv->dev, "window_cnt_value=0x%llx\n", window_cnt_value);

	// calculate dfi bw
	window_time = window_cnt_value / priv->apb_clk_rate;
	dev_dbg(priv->dev, "window_time=%lld s\n", window_time);

	//1.Event 0 counter, write
	writel_relaxed(0, DFI_PM_EVENT_ACC_SEL);
	status   = readl_relaxed(DFI_PM_EVENT_N_COUNTER_HIGH);
	cnt_high = (status & 0xFFF);
	status   = readl_relaxed(DFI_PM_EVENT_N_COUNTER_LOW);
	cnt_low  = status;
	event_cnt_value = (cnt_high << 32) + cnt_low;
	dev_dbg(priv->dev, "Event.0 counter_high=0x%llx\n", cnt_high);
	dev_dbg(priv->dev, "Event.0 counter_low=0x%llx\n", cnt_low);
	dev_dbg(priv->dev, "event_cnt_value=0x%llx\n", event_cnt_value);

	priv->write_num = event_cnt_value * DDR_BURST_LENGTH * (DDR_DATA_WIDTH / 8);
	priv->write_num = priv->write_num / window_time;
	dev_dbg(priv->dev, "write_num=%lld per second\n", priv->write_num);

	//2. Event 1 counter, read
	writel_relaxed(1, DFI_PM_EVENT_ACC_SEL);
	status   = readl_relaxed(DFI_PM_EVENT_N_COUNTER_HIGH);
	cnt_high = (status & 0xFFF);
	status   = readl_relaxed(DFI_PM_EVENT_N_COUNTER_LOW);
	cnt_low  = status;
	event_cnt_value = (cnt_high << 32) + cnt_low;

	dev_dbg(priv->dev, "Event.1 counter high=0x%llx\n", cnt_high);
	dev_dbg(priv->dev, "Event.1 counter low=0x%llx\n", cnt_low);
	dev_dbg(priv->dev, "event_cnt_value=0x%llx\n", event_cnt_value);

	priv->read_num = event_cnt_value * DDR_BURST_LENGTH * (DDR_DATA_WIDTH / 8);
	priv->read_num = priv->read_num / window_time;
	dev_dbg(priv->dev, "read_num=%lld  per second\n", priv->read_num);

	dfi_monitor_restart(priv);
}

static irqreturn_t isr_handler(int irq, void *dev_id)
{
	struct x5_dfi *priv = dev_id;
	u32 status;

	status = readl_relaxed(DDR_INTR_STATUS);
	if (status & DDR_INT_SOURCE_DFI_BIT) {
		dev_dbg(priv->dev, "dfi interrupt: %x\n", readl_relaxed(DFI_PM_INT_STATUS));
		writel_relaxed(0x7, DFI_PM_INT_STATUS);
		ddrmon_status_chk(priv);
	}

	return IRQ_HANDLED;
}

//ddr_mon related case
static void ddrmon_event_set(struct x5_dfi *priv,
			     uint32_t event_n,
			     uint32_t mon_type,
			     uint32_t end_enable,
			     uint32_t end_type)
{
	//monitor select event
	writel_relaxed(event_n, DFI_PM_EVENT_ACC_SEL);

	//monitor enable start event
	if (end_enable == 0x1) {
		writel_relaxed(0x3, DFI_PM_EVENT_N_CTRL);

		writel_relaxed(mon_type, DFI_PM_EVENT_N_START_CTRL_0); //cfg start event
		writel_relaxed(end_type, DFI_PM_EVENT_N_END_CTRL_0);   //cfg end event
	} else {
		writel_relaxed(0x1, DFI_PM_EVENT_N_CTRL);
		writel_relaxed(mon_type, DFI_PM_EVENT_N_START_CTRL_0); //cfg start event
	}
}

static void ddrmon_event_select(struct x5_dfi *priv)
{
	uint32_t pval;

	//test1 ----event set
	//event0. ddrmon_write bandwidth static
	ddrmon_event_set(priv, 0, 0x8, 0x0, 0x0); //config ddr_monitor

	// ddrmon_real_write  bandwidth static
	pval = readl_relaxed(DFI_PM_MASK_BYTE_CNT_0);
	pval |= 0x1;
	writel_relaxed(pval, DFI_PM_MASK_BYTE_CNT_0); //enable count mask byte number
	//event1. read cmd to dfi_rddata_valid latency
	ddrmon_event_set(priv, 1, 0x09, 0x0, 0x0); //config ddr_monitor
}

static void dfi_monitor_init(struct x5_dfi *priv)
{
	uint64_t wid_time = priv->apb_clk_rate * priv->interval;

	dev_dbg(priv->dev, "wid_time=0x%llx\n", wid_time);
	dev_dbg(priv->dev, "wid_low=0x%llx\n", wid_time & 0xffffffff);
	dev_dbg(priv->dev, "wid_high=0x%llx\n", (wid_time >> 32) & 0xffffffff);

	//config ddr_monitor SYS_info
	writel_relaxed(0x2040, DFI_PM_DDR_SYS_INFO);

	//config monitor window
	writel_relaxed(wid_time & 0xffffffff, DFI_PM_WIND_CNT_LOW);
	writel_relaxed((wid_time >> 32) & 0xffffffff, DFI_PM_WIND_CNT_HIGH);

	//enable all kind of interrupt
	writel_relaxed(0x7, DFI_PM_INT_EN);

	ddrmon_event_select(priv);

	/* auto enable it */
	writel_relaxed(0x1, DFI_PM_WIND_CNT_CTRL);
}

static const struct devfreq_event_ops x5_dfi_ops = {
	.disable = x5_dfi_disable,
	.enable = x5_dfi_enable,
	.get_event = x5_dfi_get_event,
	.set_event = x5_dfi_set_event,
};

static const struct of_device_id x5_dfi_id_match[] = {
	{ .compatible = "horizon,x5-dfi" },
	{ /*sentinel */ },
};

MODULE_DEVICE_TABLE(of, x5_dfi_id_match);

static int x5_dfi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct x5_dfi *priv;
	struct devfreq_event_desc *desc;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	priv = devm_kzalloc(dev, sizeof(struct x5_dfi), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;

	/* dfi monitor base */
	priv->regs_0 = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->regs_0))
		return PTR_ERR(priv->regs_0);

	/* ddr intr base */
	priv->regs_1 = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(priv->regs_1))
		return PTR_ERR(priv->regs_1);

	/* request irq */
	priv->irq = platform_get_irq(pdev, 0);
	if (priv->irq < 0)
		return priv->irq; /* -ENXIO */

	ret = devm_request_irq(priv->dev, priv->irq, isr_handler,
			       IRQF_SHARED, KBUILD_MODNAME, priv);
	if (ret)
		return ret;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	priv->apb_clk = devm_clk_get(priv->dev, "apb-clk");
	if (IS_ERR(priv->apb_clk))
		return PTR_ERR(priv->apb_clk);

	/* apb clock rate will be used for wid time calculate */
	priv->apb_clk_rate = clk_get_rate(priv->apb_clk);
	device_property_read_u32(priv->dev, "interval", &priv->interval);

	dev_info(priv->dev, "apb clk rate: %u, interval: %u\n",
		 priv->apb_clk_rate, priv->interval);

	dfi_monitor_init(priv);

	desc->ops = &x5_dfi_ops;
	desc->driver_data = priv;
	desc->name = np->name;
	priv->desc = desc;

	priv->edev = devm_devfreq_event_add_edev(&pdev->dev, desc);
	if (IS_ERR(priv->edev)) {
		dev_err(&pdev->dev,
			"failed to add devfreq-event device\n");
		return PTR_ERR(priv->edev);
	}

	platform_set_drvdata(pdev, priv);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static __maybe_unused int vsi_dfi_suspend(struct device *dev)
{
	return pm_runtime_force_suspend(dev);
}

static __maybe_unused int vsi_dfi_resume(struct device *dev)
{
	struct x5_dfi *priv = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_force_resume(dev);
	if (ret) {
		dev_err(dev, "failed to resume pwm, error %d\n", ret);
		return ret;
	}

	dfi_monitor_restart(priv);

	return 0;
}
#endif

static const struct dev_pm_ops vsi_dfi_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(vsi_dfi_suspend, vsi_dfi_resume)
};

static struct platform_driver x5_dfi_driver = {
	.probe	= x5_dfi_probe,
	.driver = {
		.name	= "x5-dfi",
		.of_match_table = x5_dfi_id_match,
		.pm = &vsi_dfi_pm_ops,
	},
};
module_platform_driver(x5_dfi_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("X5 DFI driver");
