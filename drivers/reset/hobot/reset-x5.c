// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <dt-bindings/reset/drobot-x5-reset.h>
#include <dt-bindings/power/drobot-x5-power.h>
#include <pmu.h>
#include "idle.h"

#define AON_SW_RST		0x0004
#define SYS_SW_RST		0x0014
#define DSP_SW_RST		0x0014
#define TOP_SW_RST		0x0054
#define SUBSYS_SW_RST		0x0058
#define CPU_SW_RST		0x0064
#define DDR_SW_RST		0x008c
#define CAMERA_SW_RST		0x0094
#define HPS_MIX_SW_RST		0x009c
#define HSIO_SW_RST		0x00a4
#define LSIO_0_SW_RST		0x00ac
#define LSIO_1_SW_RST		0x00b4
#define AON_ABNORMAL_SW_RST	0x00bc
#define DSP_ABNORMAL_SW_RST	0x00c0

#define RESET_DIR_NAME "drobot_reset"

struct drobot_rst_signal {
	const char *name;
	u32 offset;
	u32 mask;
	u32 iso_id;
};

struct drobot_rst_variant {
	const struct drobot_rst_signal *signals;
	unsigned int signals_num;
};

struct drobot_rst {
	void __iomem *base;
	struct device *idle;
	const struct drobot_rst_variant *variant;
	struct reset_controller_dev rcdev;
	spinlock_t lock;
#ifdef CONFIG_DEBUG_FS
	struct drobot_rst_node *nodes;
	struct list_head head;
#endif
};

#ifdef CONFIG_DEBUG_FS
struct drobot_rst_node {
	struct drobot_rst *rst;
	const struct drobot_rst_signal *signal;
};

static struct dentry *drobot_reset_root;
static struct list_head reset_list;
#endif

static const struct drobot_rst_signal aon_rst_signals[AON_RESET_NUM] = {
	[AON_PMU_CNT_RESET] = {"AON_PMU_CNT", AON_SW_RST, BIT(0), 0xff},
	[AON_GPIO_RESET]    = {"AON_GPIO", AON_SW_RST, BIT(2), 0xff},
	[AON_RTC_RESET]	    = {"AON_RTC", AON_SW_RST, BIT(3) | BIT(4), 0xff},
};

static const struct drobot_rst_signal dsp_rst_signals[DSP_RESET_NUM] = {
	[DSP_HIFI5_RESET]      = {"DSP_HIFI5", DSP_SW_RST, BIT(3), ISO_PD_DSP_HIFI5},
	[DSP_DMA_RESET]        = {"DSP_DMA", DSP_SW_RST, BIT(6) | BIT(7), ISO_CG_DSP_DMA},
	[DSP_PDM_RESET]        = {"DSP_PDM", DSP_SW_RST, BIT(8), 0xff},
	[DSP_TIMER_RESET]      = {"DSP_TIMER", DSP_SW_RST, BIT(9) | BIT(10), 0xff},
	[DSP_GPIO_APB_RESET]   = {"DSP_GPIO_APB", DSP_SW_RST, BIT(13), 0xff},
	[DSP_UART_RESET]       = {"DSP_UART", DSP_SW_RST, BIT(14), 0xff},
	[DSP_I2S0_RESET]       = {"DSP_I2S0", DSP_SW_RST, BIT(15) | BIT(16), 0xff},
	[DSP_I2S1_RESET]       = {"DSP_I2S1", DSP_SW_RST, BIT(17) | BIT(18), 0xff},
	[DSP_I2C_RESET]        = {"DSP_I2C", DSP_SW_RST, BIT(19), 0xff},
	[DSP_SPI_RESET]        = {"DSP_SPI", DSP_SW_RST, BIT(20), 0xff},
	[DSP_MAILBOX_RESET]    = {"DSP_MAILBOX", DSP_SW_RST, BIT(21), 0xff},
	[DSP_HIFI5_DBG_RESET]  = {"DSP_HIFI5_DBG", DSP_SW_RST, BIT(4), 0xff},
	[DSP_HIFI5_ATCL_RESET] = {"DSP_HIFI5_ATCL", DSP_SW_RST, BIT(2) | BIT(31), 0xff},
};

static const struct drobot_rst_signal soc_rst_signals[SOC_RESET_NUM] = {
	[SOC_WDT_APB_RESET]    = {"SOC_WDT_APB", TOP_SW_RST, BIT(4), 0xff},
	[SOC_DMA_RESET]	       = {"SOC_DMA", TOP_SW_RST, BIT(5) | BIT(6), ISO_CG_DMA},
	[SOC_TOP_TIMER0_RESET] = {"SOC_TIMER0", TOP_SW_RST, BIT(7) | BIT(8), 0xff},
	[SOC_TOP_TIMER1_RESET] = {"SOC_TIMER1", TOP_SW_RST, BIT(9) | BIT(10), 0xff},
	[SOC_TOP_PVT_RESET]    = {"SOC_PVT", TOP_SW_RST, BIT(11), 0xff},

	[CAM_ISP8000_RESET] = {"ISP8000", CAMERA_SW_RST, BIT(0), ISO_CG_ISP},
	[CAM_CSI0_RESET]    = {"CSI0", CAMERA_SW_RST, BIT(1), 0xff},
	[CAM_CSI1_RESET]    = {"CSI1", CAMERA_SW_RST, BIT(2), 0xff},
	[CAM_CSI2_RESET]    = {"CSI2", CAMERA_SW_RST, BIT(3), 0xff},
	[CAM_CSI3_RESET]    = {"CSI3", CAMERA_SW_RST, BIT(4), 0xff},
	[CAM_GDC_RESET]	    = {"GDC", CAMERA_SW_RST, BIT(5), ISO_CG_DW230},
	[CAM_DEWARP_RESET]  = {"DEWARP", CAMERA_SW_RST, BIT(7), ISO_CG_DW230},
	[CAM_SIF0_RESET]    = {"SIF0", CAMERA_SW_RST, BIT(11), ISO_CG_SIF},
	[CAM_SIF1_RESET]    = {"SIF1", CAMERA_SW_RST, BIT(12), ISO_CG_SIF},
	[CAM_SIF2_RESET]    = {"SIF2", CAMERA_SW_RST, BIT(13), ISO_CG_SIF},
	[CAM_SIF3_RESET]    = {"SIF3", CAMERA_SW_RST, BIT(14), ISO_CG_SIF},

	[DISP_CSI_TX_RESET]	= {"CSI_TX", HPS_MIX_SW_RST, BIT(0), 0xff},
	[DISP_DSI_RESET]	= {"DSI_TX", HPS_MIX_SW_RST, BIT(1), 0xff},
	[DISP_DC8000NANO_RESET] = {"DC8000NANO", HPS_MIX_SW_RST, BIT(2) | BIT(3), ISO_CG_DC8000},
	[DISP_BT1120_RESET]	= {"BT1120", HPS_MIX_SW_RST, BIT(4), ISO_CG_BT1120},
	[DISP_SIF_RESET]	= {"DISP_SIF", HPS_MIX_SW_RST, BIT(5), ISO_CG_DISP_SIF},
	[DISP_GPIO_P_RESET]	= {"DISP_GPIO", HPS_MIX_SW_RST, BIT(6), 0xff},
	[GPU_GC8000_RESET] = {"GC8000", HPS_MIX_SW_RST, BIT(7) | BIT(8) | BIT(9), ISO_CG_GPU3D},
	[GPU_GC820_RESET]  = {"GC820", HPS_MIX_SW_RST, BIT(10) | BIT(11) | BIT(12), ISO_CG_GPU2D},
	[BPU_RESET] = {"BPU", HPS_MIX_SW_RST, BIT(16) | BIT(17) | BIT(18) | BIT(19), ISO_PD_BPU},
	[VIDEO_JPEG_RESET]  = {"VIDEO_JPEG", HPS_MIX_SW_RST, BIT(20) | BIT(21) | BIT(22),
			       ISO_CG_JPEG},
	[VIDEO_CODEC_RESET] = {"VIDEO_CODEC", HPS_MIX_SW_RST, BIT(23) | BIT(24) | BIT(25) | BIT(26),
			       ISO_CG_VIDEO},

	[HSIO_QSPI_RESET]  = {"QSPI", HSIO_SW_RST, BIT(0), ISO_CG_QSPI_AXIS},
	[HSIO_ENET_RESET]  = {"ETH", HSIO_SW_RST, BIT(1), ISO_CG_GMAC},
	[HSIO_EMMC_RESET]  = {"EMMC", HSIO_SW_RST, BIT(2), ISO_CG_EMMC},
	[HSIO_SD_RESET]	   = {"SD", HSIO_SW_RST, BIT(3), ISO_CG_SD},
	[HSIO_SDIO_RESET]  = {"SDIO", HSIO_SW_RST, BIT(4), ISO_CG_SDIO},
	[HSIO_USB2_RESET]  = {"USB2", HSIO_SW_RST, BIT(5), ISO_CG_USB2},
	[HSIO_USB3_RESET]  = {"USB3", HSIO_SW_RST, BIT(6), ISO_CG_USB3},
	[HSIO_GPIO0_RESET] = {"HS-GPIO0", HSIO_SW_RST, BIT(9), 0xff},
	[HSIO_GPIO1_RESET] = {"HS-GPIO1", HSIO_SW_RST, BIT(10), 0xff},

	[LSIO_UART1_RESET]     = {"UART1", LSIO_0_SW_RST, BIT(1) | BIT(6), 0xff},
	[LSIO_UART2_RESET]     = {"UART2", LSIO_0_SW_RST, BIT(2) | BIT(7), 0xff},
	[LSIO_UART3_RESET]     = {"UART3", LSIO_0_SW_RST, BIT(3) | BIT(8), 0xff},
	[LSIO_UART4_RESET]     = {"UART4", LSIO_0_SW_RST, BIT(4) | BIT(9), 0xff},
	[LSIO_SPI0_RESET]      = {"SPI0", LSIO_0_SW_RST, BIT(10), 0xff},
	[LSIO_SPI1_RESET]      = {"SPI1", LSIO_0_SW_RST, BIT(11), 0xff},
	[LSIO_SPI2_RESET]      = {"SPI2", LSIO_0_SW_RST, BIT(12), 0xff},
	[LSIO_SPI3_RESET]      = {"SPI3", LSIO_0_SW_RST, BIT(13), 0xff},
	[LSIO_SPI4_RESET]      = {"SPI4", LSIO_0_SW_RST, BIT(14), 0xff},
	[LSIO_SPI5_RESET]      = {"SPI5", LSIO_0_SW_RST, BIT(15), 0xff},
	[LSIO_LPWM0_RESET]     = {"LPWM0", LSIO_0_SW_RST, BIT(16) | BIT(18), 0xff},
	[LSIO_LPWM1_RESET]     = {"LPWM1", LSIO_0_SW_RST, BIT(17) | BIT(19), 0xff},
	[LSIO_GPIO0_APB_RESET] = {"LS-GPIO0", LSIO_0_SW_RST, BIT(20), 0xff},
	[LSIO_GPIO1_APB_RESET] = {"LS-GPIO1", LSIO_0_SW_RST, BIT(21), 0xff},
	[LSIO_I2C0_APB_RESET]  = {"I2C0", LSIO_0_SW_RST, BIT(22), 0xff},
	[LSIO_I2C1_APB_RESET]  = {"I2C1", LSIO_0_SW_RST, BIT(23), 0xff},
	[LSIO_I2C2_APB_RESET]  = {"I2C2", LSIO_0_SW_RST, BIT(24), 0xff},
	[LSIO_I2C3_APB_RESET]  = {"I2C3", LSIO_0_SW_RST, BIT(25), 0xff},
	[LSIO_I2C4_APB_RESET]  = {"I2C4", LSIO_0_SW_RST, BIT(26), 0xff},
	[LSIO_PWM0_APB_RESET]  = {"PWM0", LSIO_0_SW_RST, BIT(27), 0xff},
	[LSIO_PWM1_APB_RESET]  = {"PWM1", LSIO_0_SW_RST, BIT(28), 0xff},
	[LSIO_PWM2_APB_RESET]  = {"PWM2", LSIO_0_SW_RST, BIT(29), 0xff},
	[LSIO_PWM3_APB_RESET]  = {"PWM3", LSIO_0_SW_RST, BIT(30), 0xff},

	[LSIO_ADC_RESET]      = {"ADC", LSIO_1_SW_RST, BIT(0) | BIT(1), 0xff},
	[LSIO_UART5_RESET]    = {"UART5", LSIO_1_SW_RST, BIT(2) | BIT(4), 0xff},
	[LSIO_UART6_RESET]    = {"UART6", LSIO_1_SW_RST, BIT(3) | BIT(5), 0xff},
	[LSIO_UART7_RESET]     = {"UART7", LSIO_0_SW_RST, BIT(0) | BIT(5), 0xff},
	[LSIO_I2C5_APB_RESET] = {"I2C5", LSIO_1_SW_RST, BIT(6), 0xff},
	[LSIO_I2C6_APB_RESET] = {"I2C6", LSIO_1_SW_RST, BIT(7), 0xff},
};

static inline struct drobot_rst *to_drobot_rst(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct drobot_rst, rcdev);
}

static inline void drobot_rst_set_bits(struct drobot_rst *drobot_rst, u32 offset, u32 mask)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&drobot_rst->lock, flags);
	val = readl(drobot_rst->base + offset);
	val |= mask;
	writel(val, drobot_rst->base + offset);
	spin_unlock_irqrestore(&drobot_rst->lock, flags);
}

static inline void drobot_rst_clear_bits(struct drobot_rst *drobot_rst, u32 offset, u32 mask)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&drobot_rst->lock, flags);
	val = readl(drobot_rst->base + offset);
	val &= ~mask;
	writel(val, drobot_rst->base + offset);
	spin_unlock_irqrestore(&drobot_rst->lock, flags);
}

static int drobot_rst_set_signal(struct drobot_rst *drobot_rst, const struct drobot_rst_signal *signal, bool assert)
{
	int ret = 0;

	if (assert) {
		if (signal->iso_id != 0xff) {
			ret = drobot_idle_request(drobot_rst->idle, signal->iso_id, true, false);
			if (ret) {
				pr_err("%s: idle request failed\n", __func__);
				return ret;
			}
		}

		drobot_rst_set_bits(drobot_rst, signal->offset, signal->mask);
	} else {
		drobot_rst_clear_bits(drobot_rst, signal->offset, signal->mask);

		if (signal->iso_id != 0xff) {
			ret = drobot_idle_request(drobot_rst->idle, signal->iso_id, false, false);
			if (ret) {
				pr_err("%s: idle connect failed\n", __func__);
				return ret;
			}
		}
	}

	return ret;
}

static int drobot_rst_get_signal(struct drobot_rst *drobot_rst, const struct drobot_rst_signal *signal)
{
	return !!(readl(drobot_rst->base + signal->offset) & signal->mask);
}

static int drobot_reset_assert(struct reset_controller_dev *rcdev,
			     unsigned long id)
{
	struct drobot_rst *drobot_rst		     = to_drobot_rst(rcdev);
	const struct drobot_rst_variant *variant = drobot_rst->variant;

	return drobot_rst_set_signal(drobot_rst, &variant->signals[id], true);
}

static int drobot_reset_deassert(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	struct drobot_rst *drobot_rst		     = to_drobot_rst(rcdev);
	const struct drobot_rst_variant *variant = drobot_rst->variant;

	return drobot_rst_set_signal(drobot_rst, &variant->signals[id], false);
}

static int drobot_reset_status(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct drobot_rst *drobot_rst		     = to_drobot_rst(rcdev);
	const struct drobot_rst_variant *variant = drobot_rst->variant;

	return drobot_rst_get_signal(drobot_rst, &variant->signals[id]);
}

static const struct reset_control_ops drobot_reset_ops = {
	.assert	  = drobot_reset_assert,
	.deassert = drobot_reset_deassert,
	.status	  = drobot_reset_status,
};

static const struct drobot_rst_variant variant_aon = {
	.signals     = aon_rst_signals,
	.signals_num = ARRAY_SIZE(aon_rst_signals),
};

static const struct drobot_rst_variant variant_dsp = {
	.signals     = dsp_rst_signals,
	.signals_num = ARRAY_SIZE(dsp_rst_signals),
};

static const struct drobot_rst_variant variant_soc = {
	.signals     = soc_rst_signals,
	.signals_num = ARRAY_SIZE(soc_rst_signals),
};

#ifdef CONFIG_DEBUG_FS
static int drobot_reset_debugfs_set(void *data, u64 val)
{
	struct drobot_rst_node *node = data;

	return drobot_rst_set_signal(node->rst, node->signal, !!val);
}

static int drobot_reset_debugfs_get(void *data, u64 *val)
{
	struct drobot_rst_node *node = data;

	*val = drobot_rst_get_signal(node->rst, node->signal);

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(drobot_reset_debugfs_fops, drobot_reset_debugfs_get, drobot_reset_debugfs_set,
			 "%llu\n");

static void reset_show_one(struct seq_file *s, struct drobot_rst *rst)
{
	const struct drobot_rst_variant *variant = rst->variant;
	int i;

	for (i = 0; i < variant->signals_num; i++) {
		const struct drobot_rst_signal *signal = &variant->signals[i];

		seq_printf(s, "%-15s    %d\n", signal->name, drobot_rst_get_signal(rst, signal));
	}
}

static int reset_summary_show(struct seq_file *s, void *data)
{
	struct drobot_rst *rst;

	seq_puts(s, "                  state\n");
	seq_puts(s, "-----------------------\n");
	list_for_each_entry (rst, &reset_list, head)
		reset_show_one(s, rst);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(reset_summary);

static void drobot_reset_init_debugfs(struct drobot_rst *drobot_rst)
{
	const struct drobot_rst_variant *variant = drobot_rst->variant;
	int i;

	for (i = 0; i < variant->signals_num; i++) {
		const struct drobot_rst_signal *signal = &variant->signals[i];
		struct drobot_rst_node *node	   = &drobot_rst->nodes[i];

		if (!signal->name)
			continue;

		node->rst    = drobot_rst;
		node->signal = signal;
		debugfs_create_file(signal->name, 0644, drobot_reset_root, node,
				    &drobot_reset_debugfs_fops);
	}
}
#endif

static int drobot_reset_probe(struct platform_device *pdev)
{
	struct drobot_rst *drobot_rst;
	struct device *dev		     = &pdev->dev;
	struct device_node *np		     = dev->of_node;
	const struct drobot_rst_variant *variant = of_device_get_match_data(dev);
	void __iomem *base;

#ifdef CONFIG_DEBUG_FS
	if (!drobot_reset_root) {
		drobot_reset_root = debugfs_create_dir(RESET_DIR_NAME, NULL);
		INIT_LIST_HEAD(&reset_list);

		debugfs_create_file("reset_summary", 0444, drobot_reset_root, NULL,
				    &reset_summary_fops);
	}
#endif

	drobot_rst = devm_kzalloc(dev, sizeof(*drobot_rst), GFP_KERNEL);
	if (!drobot_rst)
		return -ENOMEM;

	base = of_iomap(np, 0);
	if (!base) {
		pr_err("%s: could not map reset region\n", __func__);
		return -EINVAL;
	}
	spin_lock_init(&drobot_rst->lock);
	drobot_rst->variant		= variant;
	drobot_rst->base = base;
	drobot_rst->rcdev.owner     = THIS_MODULE;
	drobot_rst->rcdev.nr_resets = variant->signals_num;
	drobot_rst->rcdev.ops	= &drobot_reset_ops;
	drobot_rst->rcdev.of_node   = dev->of_node;

	drobot_rst->idle = drobot_idle_get_dev(dev->of_node);
	if (IS_ERR(drobot_rst->idle)) {
		pr_err("%s(%ld): could not get idle dev\n", __func__, PTR_ERR(drobot_rst->idle));
		return PTR_ERR(drobot_rst->idle);
	}

#ifdef CONFIG_DEBUG_FS
	drobot_rst->nodes =
		devm_kzalloc(dev, sizeof(*drobot_rst->nodes) * variant->signals_num, GFP_KERNEL);
	if (!drobot_rst->nodes)
		return -ENOMEM;

	drobot_reset_init_debugfs(drobot_rst);

	list_add_tail(&drobot_rst->head, &reset_list);
#endif

	dev_set_drvdata(dev, drobot_rst);

	return devm_reset_controller_register(dev, &drobot_rst->rcdev);
}

static const struct of_device_id drobot_reset_dt_ids[] = {
	{ .compatible = "horizon,aon-reset", .data = &variant_aon },
	{ .compatible = "horizon,dsp-reset", .data = &variant_dsp },
	{ .compatible = "horizon,soc-reset", .data = &variant_soc },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, drobot_reset_dt_ids);

static struct platform_driver drobot_reset_driver = {
	.probe	= drobot_reset_probe,
	.driver = {
		.name		= "drobot-reset",
		.of_match_table	= drobot_reset_dt_ids,
	},
};

static int __init drobot_reset_drv_register(void)
{
	return platform_driver_register(&drobot_reset_driver);
}
postcore_initcall(drobot_reset_drv_register);
