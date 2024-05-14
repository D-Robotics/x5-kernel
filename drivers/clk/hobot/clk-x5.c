// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <dt-bindings/clock/drobot-x5-clock.h>
#include <dt-bindings/power/drobot-x5-power.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-snps-pll.h"
#include "clk.h"
#include "idle.h"

#define DSP_PLL_INTERNAL	0x1000
#define CPU_PLL_INTERNAL	0x2000
#define SYS0_PLL_INTERNAL	0x2100
#define SYS1_PLL_INTERNAL	0x2200
#define DISP_PLL_INTERNAL	0x2300
#define PIXEL_PLL_INTERNAL	0x2400

#define AON_CLK_ENB		0x0
#define DSP_CLK_ENB		0x10

#define HPS_CLK_GEN		0x1000

#define TOP_CLK_ENB		0x50
#define CPU_CLK_ENB		0x60
#define DDR_CLK_ENB		0x88
#define CAMERA_CLK_ENB		0x90
#define HPS_MIX_CLK_ENB		0x98
#define HSIO_CLK_ENB		0xA0
#define LSIO_0_CLK_ENB		0xA8
#define LSIO_1_CLK_ENB		0xB0

#define X5_PLL_RATE(_rate, _prediv, _mint, _mfrac, _postdivp, _divvcop, _postdivr, _divvcor)	\
{									\
	.rate	= _rate##U,						\
	.prediv = _prediv,						\
	.mint = _mint,							\
	.mfrac = _mfrac,						\
	.postdivp = _postdivp,						\
	.divvcop = _divvcop,						\
	.postdivr = _postdivr,						\
	.divvcor = _divvcor,						\
}

static struct pll_rate_table x5_pll_rates[] = {
	/* _rate, _prediv, _mint, _mfrac, _p, _divvcop, _r, _divvcor */
	X5_PLL_RATE(1800000000, 1, 0x96, 0x0, 1, 2, 1, 2),
	X5_PLL_RATE(1622250000, 1, 0x87, 0x3000, 1, 2, 3, 2),
	X5_PLL_RATE(1620000000, 1, 0x87, 0, 1, 2, 1, 2),
	X5_PLL_RATE(1500000000, 1, 0x7D, 0, 1, 2, 2, 2),
	X5_PLL_RATE(1347750000, 1, 0x70, 0x5000, 1, 2, 1, 2),
	X5_PLL_RATE(1200000000, 1, 0xC8, 0, 1, 4, 1, 4),
	X5_PLL_RATE(1050000000, 1, 0xAF, 0, 1, 4, 1, 4),
	X5_PLL_RATE(996000000,  1, 0xA6, 0, 1, 4, 2, 4),
	X5_PLL_RATE(339937500,  1, 0x71, 0x5000, 4, 2, 2, 2),
	X5_PLL_RATE(312375000,  1, 0x9C, 0x3000, 6, 2, 2, 2),
	X5_PLL_RATE(297000000,  1, 0xC6, 0, 2, 8, 2, 8),
	X5_PLL_RATE(251750000,  1, 0x7D, 0xE000, 6, 2, 2, 2),
	X5_PLL_RATE(65000000,   1, 0x82, 0, 6, 8, 6, 8),
};

struct x5_rate_list {
	u32		id;
	unsigned long	rate;
};

static struct x5_rate_list aon_gen_rates[] = {
	{X5_OSC_SEL,		24000000},
	{X5_AON_APB0_CLK,	180224000},
	{X5_AON_MEM_NOC_CLK,	540750000},
	{X5_AON_RTC_CLK,	24000000},
	{X5_AON_PMU_CNT_CLK,	24000000},
};

static struct x5_rate_list dsp_gen_rates[] = {
	{X5_DSP_NOC_CLK,	540750000},
	{X5_DSP_APB_CLK,	202781250},
	{X5_DSP_HIFI5_GCLK,	811008000},
	{X5_DSP_HIFI5_APB_CLK,	202781250},
	{X5_DSP_DMA_AXI_CLK,	405504000},
	{X5_DSP_PDM_HMCLKA_CLK,	12288000},
	{X5_DSP_TIMER_CLK,	1000000},
	{X5_DSP_HIFI5_CHILD_CLK,405504000},
	{X5_DSP_I2S0_SCLK,	12288000},
	{X5_DSP_I2S0_MCLK,	24576000},
	{X5_DSP_I2S1_SCLK,	12288000},
	{X5_DSP_I2S1_MCLK,	24576000},
};

static struct x5_rate_list soc_pll_rates0[] = {
	{X5_CPU_PLL_P,		1200000000},
	{X5_SYS0_PLL_P,		1500000000},
	{X5_SYS1_PLL_P,		996000000},
	{X5_DISP_PLL_P,		297000000},
	{X5_PIXEL_PLL_P,	297000000},
};

static struct x5_rate_list soc_pll_rates1[] = {
	{X5_CPU_PLL_P,		1200000000}, /* use low frequency to remove children */
	{X5_SYS0_PLL_P,		1500000000},
	{X5_SYS1_PLL_P,		996000000},
	{X5_DISP_PLL_P,		297000000},
	{X5_PIXEL_PLL_P,	1800000000},
};

static struct x5_rate_list soc_gen_rates[] = {
	{X5_TOP_NOC_CLK,		750000000},
	{X5_BPU_NOC_CLK,		750000000},
	{X5_CODEC_NOC_CLK,		750000000},
	{X5_GPU_NOC_CLK,		750000000},
	{X5_ROM_ACLK,			400000000},
	{X5_TOP_APB_CLK,		200000000},
	{X5_CPU_CORE_CLK,		1500000000},
	{X5_CPU_PCLK,			300000000},
	{X5_CPU_ATCLK,			300000000},
	{X5_CSS600_TSGEN_CLK,		600000000},
	{X5_CSS600_DBG_CLK,		150000000},
	{X5_CSS600_TRACE_CLK,		400000000},
	{X5_CSS600_TPIU_CLK,		200000000},
	{X5_CPU_NOC_CLK,		750000000},
	{X5_CAM_ISP8000_CORE_CLK,	600000000},
	{X5_CAM_ISP8000_AXI_CLK,	600000000},
	{X5_CAM_CSI0_IPI_CLK,		600000000},
	{X5_CAM_CSI1_IPI_CLK,		600000000},
	{X5_CAM_CSI2_IPI_CLK,		600000000},
	{X5_CAM_CSI3_IPI_CLK,		600000000},
	{X5_CAM_CSI_TX_CLK,		600000000},
	{X5_CAM_SIF_AXI_CLK,		600000000},
	{X5_CAM_DEWARP_CORE_CLK,	600000000},
	{X5_CAM_DEWARP_AXI_CLK,		600000000},
	{X5_CAM_VSE_CORE_CLK,		600000000},
	{X5_CAM_VSE_UPSCALE_CLK,	750000000},
	{X5_CAM_VSE_AXI_CLK,		750000000},
	{X5_DISP_BT1120_PIXEL_CLK,	27000000},
	{X5_DISP_DC8000_PIXEL_CLK,	27000000},
	{X5_DISP_SIF_ACLK,		600000000},
	{X5_DISP_BT1120_ACLK,		600000000},
	{X5_DISP_DC8000_ACLK,		600000000},
	{X5_GPU_GC820_CLK,		750000000},
	{X5_GPU_GC8000L_CLK,		996000000},
	{X5_GPU_GC820_ACLK,		750000000},
	{X5_GPU_GC8000L_ACLK,		996000000},
	{X5_BPU_MCLK_2X,		996000000},
	{X5_BPU_SYS_TIMER_CLK,		6000000},
	{X5_VIDEO_CODEC_CORE_CLK,	600000000},
	{X5_VIDEO_CODEC_BCLK,		500000000},
	{X5_VIDEO_JPEG_CORE_CLK,	500000000},
	{X5_HSIO_QSPI_BUS_CLK,		200000000},
	{X5_HSIO_QSPI_CORE_CLK,		200000000},
	{X5_HSIO_ENET_AXI_CLK,		400000000},
	{X5_HSIO_ENET_RGMII_CLK,	125000000},
	{X5_HSIO_ENET_PTP_REFCLK,	200000000},
	{X5_HSIO_ENET_REF_CLK,		50000000},
	{X5_HSIO_EMMC_AXI_CLK,		200000000},
	{X5_HSIO_24M_CLK,		24000000},
	{X5_HSIO_EMMC_CCLK,		200000000},
	{X5_HSIO_SDIO0_AXI_CLK,		200000000},
	{X5_HSIO_SDIO0_CCLK,		200000000},
	{X5_HSIO_SDIO1_AXI_CLK,		200000000},
	{X5_HSIO_SDIO1_CCLK,		200000000},
	{X5_HSIO_USB2_AXI_CLK,		400000000},
	{X5_HSIO_USB3_AXI_CLK,		400000000},
	{X5_SEC_AXI_CLK,		400000000},
	{X5_SEC_APB_CLK,		400000000},
	{X5_LSIO_LPWM0_CLK,		24000000},
	{X5_LSIO_LPWM1_CLK,		24000000},
	{X5_HPS_DMA_AXI_CLK,		400000000},
	{X5_TOP_TIMER0_CLK,		1000000},
	{X5_TOP_TIMER1_CLK,		1000000},
	{X5_LSIO_SENSOR0_CLK,		24000000},
	{X5_LSIO_SENSOR1_CLK,		24000000},
	{X5_LSIO_SENSOR2_CLK,		24000000},
	{X5_LSIO_SENSOR3_CLK,		24000000},
	{X5_BPU_NOC_PCLK,		200000000},
};

struct drobot_clk_provider {
	struct device *idle;
	struct clk_hw_onecell_data clk_hw_data;
};

static const char *osc_src_sels[] = { "osc"};

static const char *aon_gen_src_sels[] = { "osc", "dsp_pll_r" };
static const char *aon_rtc_src_sels[] = { "osc"};
static const char *aon_apb0_src_sels[] = { "osc", "dsp_pll_r", "osc_sel" };

static const char *dsp_gen_src_sels[] = { "osc", "dsp_pll_p" };
static const char *i2s_gen_src_sels[] = { "osc", "dsp_pll_p" };
static const char *i2s0_s_src_sels[] = { "dsp_i2s0_child_sclk", "audio_externel" };
static const char *i2s1_s_src_sels[] = { "dsp_i2s1_child_sclk", "audio_externel" };
static const char *dsp_noc_gen_src_sels[] = { "osc", "dsp_pll_p", "dsp_pll_r" };
static const char *soc_gen_src_sels[] = { "osc", "cpu_pll_p", "cpu_pll_r", "sys0_pll_p", "sys0_pll_r", "sys1_pll_p", "sys1_pll_r" , "pixel_pll_r" };
static const char *apb_gen_src_sels[] = { "osc", "cpu_pll_p", "osc", "osc", "osc", "osc", "osc" , "osc" };
static const char *disp_gen_src_sels[] = { "osc", "disp_pll_p", "pixel_pll_p", "disp_pll_r", "pixel_pll_r" };

/* use osc clock to disable parent selection */
static const char *disp_no_pix_gen_src_sels[] = { "osc", "disp_pll_p", "osc", "disp_pll_r", "osc" };
static u32 gate_flags = CLK_SET_RATE_PARENT;

static int crm_dsp_clk_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct drobot_clk_provider *ctx;
	struct clk_hw **hws;
	void __iomem *base;
	int ret, i;

	ctx = devm_kzalloc(dev, struct_size(ctx, clk_hw_data.hws, X5_DSP_END_CLK), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	for (i = 0; i < X5_DSP_END_CLK; ++i)
		ctx->clk_hw_data.hws[i] = ERR_PTR(-ENOENT);

	ctx->clk_hw_data.num = X5_DSP_END_CLK;
	hws = ctx->clk_hw_data.hws;

	ctx->idle = drobot_idle_get_dev(dev->of_node);
	if (IS_ERR(ctx->idle)) {
		pr_err("%s(%ld): could not get idle dev\n", __func__, PTR_ERR(ctx->idle));
		return PTR_ERR(ctx->idle);
	}

	base = devm_platform_ioremap_resource(pdev, 0);
	if (WARN_ON(IS_ERR(base)))
		return PTR_ERR(base);

	hws[X5_DSP_PLL_P] = clk_hw_register_pll("dsp_pll_p", "osc", base,
			base + DSP_PLL_INTERNAL, PLL_P_OUT, x5_pll_rates, ARRAY_SIZE(x5_pll_rates));
	hws[X5_DSP_PLL_R] = clk_hw_register_pll("dsp_pll_r", "osc", base,
			base + DSP_PLL_INTERNAL, PLL_R_OUT, NULL, 0);

	clk_set_rate(hws[X5_DSP_PLL_P]->clk, 1622250000);
	clk_get_rate(hws[X5_DSP_PLL_R]->clk);

	hws[X5_DSP_NOC_CLK] = drobot_clk_register_generator_flags_no_idle("dsp_noc_clk", dsp_noc_gen_src_sels, ARRAY_SIZE(dsp_noc_gen_src_sels), base + 0x800, CLK_IS_CRITICAL);
	hws[X5_DSP_APB_CLK] = drobot_clk_register_generator("dsp_apb_clk", dsp_gen_src_sels, ARRAY_SIZE(dsp_gen_src_sels), base + 0x820, CLK_IS_CRITICAL, NULL, 0xff);
	hws[X5_DSP_HIFI5_GCLK] = drobot_clk_register_generator_no_idle("dsp_hifi5_gclk", dsp_gen_src_sels, ARRAY_SIZE(dsp_gen_src_sels), base + 0x840);
	hws[X5_DSP_HIFI5_APB_CLK] = drobot_clk_register_generator_no_idle("dsp_hifi5_apb_clk", dsp_gen_src_sels, ARRAY_SIZE(dsp_gen_src_sels), base + 0x860);
	hws[X5_DSP_DMA_AXI_CLK] = drobot_clk_register_gen_no_flags("dsp_dma_axi_clk", dsp_gen_src_sels, ARRAY_SIZE(dsp_gen_src_sels), base + 0x880, ctx->idle, ISO_CG_DSP_DMA);
	hws[X5_DSP_PDM_HMCLKA_CLK] = drobot_clk_register_generator_no_idle("dsp_pdm_hmclka_clk", dsp_gen_src_sels, ARRAY_SIZE(dsp_gen_src_sels), base + 0x8A0);

	hws[X5_DSP_I2S0_GEN_SCLK] = drobot_clk_register_i2s_generator("dsp_i2s0_gen_sclk", i2s_gen_src_sels, ARRAY_SIZE(i2s_gen_src_sels), base + 0x8C0, base + 0x1C, 0, CLK_SET_RATE_NO_REPARENT, ctx->idle, 0xff);
	hws[X5_DSP_I2S0_GEN_MCLK] = drobot_clk_register_i2s_generator("dsp_i2s0_gen_mclk", i2s_gen_src_sels, ARRAY_SIZE(i2s_gen_src_sels), base + 0x8E0, base + 0x1C, 0, CLK_SET_RATE_NO_REPARENT, ctx->idle, 0xff);
	hws[X5_DSP_I2S1_GEN_SCLK] = drobot_clk_register_i2s_generator("dsp_i2s1_gen_sclk", i2s_gen_src_sels, ARRAY_SIZE(i2s_gen_src_sels), base + 0x900, base + 0x1C, 2, CLK_SET_RATE_NO_REPARENT, ctx->idle, 0xff);
	hws[X5_DSP_I2S1_GEN_MCLK] = drobot_clk_register_i2s_generator("dsp_i2s1_gen_mclk", i2s_gen_src_sels, ARRAY_SIZE(i2s_gen_src_sels), base + 0x920, base + 0x1C, 2, CLK_SET_RATE_NO_REPARENT, ctx->idle, 0xff);
	hws[X5_DSP_TIMER_CLK] = drobot_clk_register_generator_no_idle("dsp_timer_clk", dsp_gen_src_sels, ARRAY_SIZE(dsp_gen_src_sels), base + 0x940);

	clk_set_parent(hws[X5_DSP_I2S0_GEN_SCLK]->clk, hws[X5_DSP_PLL_P]->clk);
	clk_set_parent(hws[X5_DSP_I2S0_GEN_MCLK]->clk, hws[X5_DSP_PLL_P]->clk);
	clk_set_parent(hws[X5_DSP_I2S1_GEN_SCLK]->clk, hws[X5_DSP_PLL_P]->clk);
	clk_set_parent(hws[X5_DSP_I2S1_GEN_MCLK]->clk, hws[X5_DSP_PLL_P]->clk);

	hws[X5_DSP_HIFI5_CHILD_CLK] = drobot_clk_hw_divider("dsp_hifi5_child_clk", "dsp_hifi5_gclk", 0, base + 0x850, 0, 2);
	hws[X5_DSP_I2S0_CHILD_SCLK] = drobot_clk_hw_divider("dsp_i2s0_child_sclk", "dsp_i2s0_gen_sclk", CLK_SET_RATE_PARENT, base + 0x8D0, 0, 5);
	hws[X5_DSP_I2S0_CHILD_MCLK] = drobot_clk_hw_divider("dsp_i2s0_child_mclk", "dsp_i2s0_gen_mclk", CLK_SET_RATE_PARENT, base + 0x8F0, 0, 5);
	hws[X5_DSP_I2S1_CHILD_SCLK] = drobot_clk_hw_divider("dsp_i2s1_child_sclk", "dsp_i2s1_gen_sclk", CLK_SET_RATE_PARENT, base + 0x910, 0, 5);
	hws[X5_DSP_I2S1_CHILD_MCLK] = drobot_clk_hw_divider("dsp_i2s1_child_mclk", "dsp_i2s1_gen_mclk", CLK_SET_RATE_PARENT, base + 0x930, 0, 5);

	hws[X5_DSP_SRAM0_CLK] = drobot_clk_hw_register_gate("dsp_sram0_clk", "dsp_noc_clk", base + DSP_CLK_ENB, 0, CLK_IS_CRITICAL, ctx->idle, ISO_CG_SRAM0);
	hws[X5_DSP_SRAM1_CLK] = drobot_clk_hw_register_gate("dsp_sram1_clk", "dsp_noc_clk", base + DSP_CLK_ENB, 1, CLK_IS_CRITICAL, ctx->idle, ISO_CG_SRAM1);

	hws[X5_DSP_HIFI5_CLK] = drobot_clk_hw_register_gate_no_idle("dsp_hifi5_clk", "dsp_hifi5_gclk", base + DSP_CLK_ENB, 2, gate_flags | CLK_IS_CRITICAL);
	hws[X5_DSP_HIFI5_BUS_CLK] = drobot_clk_hw_register_gate_no_idle("dsp_hifi5_bus_clk", "dsp_hifi5_child_clk", base + DSP_CLK_ENB, 3, gate_flags | CLK_IS_CRITICAL);

	hws[X5_DSP_DMA_PCLK] = drobot_clk_hw_register_gate_no_idle("dsp_dma_pclk", "dsp_apb_clk", base + DSP_CLK_ENB, 6, 0);
	hws[X5_DSP_PDM_APB_CLK] = drobot_clk_hw_register_gate_no_idle("dsp_pdm_apb_clk", "dsp_apb_clk", base + DSP_CLK_ENB, 8, 0);
	hws[X5_DSP_TIMER1_PCLK] = drobot_clk_hw_register_gate_no_idle("dsp_timer1_pclk", "dsp_apb_clk", base + DSP_CLK_ENB, 10, 0);
	hws[X5_DSP_GPIO_PCLK] = drobot_clk_hw_register_gate_no_idle("dsp_gpio_pclk", "dsp_apb_clk", base + DSP_CLK_ENB, 13, 0);
	hws[X5_DSP_UART_CLK] = drobot_clk_hw_register_gate_no_idle("dsp_uart_clk", "dsp_apb_clk", base + DSP_CLK_ENB, 14, CLK_IS_CRITICAL);

	hws[X5_DSP_I2S0_MUX_SCLK] = drobot_clk_hw_mux("dsp_i2s0_ssel", i2s0_s_src_sels, ARRAY_SIZE(i2s0_s_src_sels), CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT, base + 0x1C, 4, 1);
	hws[X5_DSP_I2S1_MUX_SCLK] = drobot_clk_hw_mux("dsp_i2s1_ssel", i2s1_s_src_sels, ARRAY_SIZE(i2s1_s_src_sels), CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT, base + 0x1C, 5, 1);

	clk_set_parent(hws[X5_DSP_I2S0_MUX_SCLK]->clk, hws[X5_DSP_I2S0_CHILD_SCLK]->clk);
	clk_set_parent(hws[X5_DSP_I2S1_MUX_SCLK]->clk, hws[X5_DSP_I2S1_CHILD_SCLK]->clk);

	hws[X5_DSP_I2S0_SCLK] = drobot_clk_hw_register_gate_no_idle("dsp_i2s0_sclk", "dsp_i2s0_ssel", base + DSP_CLK_ENB, 15, gate_flags);
	hws[X5_DSP_I2S0_MCLK] = drobot_clk_hw_register_gate_no_idle("dsp_i2s0_mclk", "dsp_i2s0_child_mclk", base + DSP_CLK_ENB, 16, gate_flags);
	hws[X5_DSP_I2S0_PCLK] = drobot_clk_hw_register_gate_no_idle("dsp_i2s0_pclk", "dsp_apb_clk", base + DSP_CLK_ENB, 17, 0);
	hws[X5_DSP_I2S1_SCLK] = drobot_clk_hw_register_gate_no_idle("dsp_i2s1_sclk", "dsp_i2s1_ssel", base + DSP_CLK_ENB, 18, gate_flags);
	hws[X5_DSP_I2S1_MCLK] = drobot_clk_hw_register_gate_no_idle("dsp_i2s1_mclk", "dsp_i2s1_child_mclk", base + DSP_CLK_ENB, 19, gate_flags);
	hws[X5_DSP_I2S1_PCLK] = drobot_clk_hw_register_gate_no_idle("dsp_i2s1_pclk", "dsp_apb_clk", base + DSP_CLK_ENB, 20, 0);
	hws[X5_DSP_I2C_PCLK] = drobot_clk_hw_register_gate_no_idle("dsp_i2c_pclk", "dsp_apb_clk", base + DSP_CLK_ENB, 21, 0);
	hws[X5_DSP_SPI_PCLK] = drobot_clk_hw_register_gate_no_idle("dsp_spi_pclk", "dsp_apb_clk", base + DSP_CLK_ENB, 22, 0);
	hws[X5_DSP_MAILBOX_ACLK] = drobot_clk_hw_register_gate_no_idle("dsp_mailbox_aclk", "dsp_noc_clk", base + DSP_CLK_ENB, 23, 0);

	for(i = 0; i < ARRAY_SIZE(dsp_gen_rates); i++)
		clk_set_rate(hws[dsp_gen_rates[i].id]->clk, dsp_gen_rates[i].rate);

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, &ctx->clk_hw_data);
	if (ret)
		return ret;

	return ret;
}

static int crm_aon_clk_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct drobot_clk_provider *ctx;
	struct clk_hw **hws;
	void __iomem *base;
	int ret, i;

	ctx = devm_kzalloc(dev, struct_size(ctx, clk_hw_data.hws, X5_AON_END_CLK), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	for (i = 0; i < X5_AON_END_CLK; ++i)
		ctx->clk_hw_data.hws[i] = ERR_PTR(-ENOENT);

	ctx->clk_hw_data.num = X5_AON_END_CLK;
	hws = ctx->clk_hw_data.hws;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (WARN_ON(IS_ERR(base)))
		return PTR_ERR(base);

	hws[X5_OSC_SEL] = drobot_clk_hw_mux("osc_sel", osc_src_sels, ARRAY_SIZE(osc_src_sels), 0, base + 0x10, 0, 1);

	hws[X5_AON_APB0_CLK] = drobot_clk_register_generator_flags_no_idle("aon_apb0_clk", aon_apb0_src_sels, ARRAY_SIZE(aon_apb0_src_sels), base + 0x800, CLK_IS_CRITICAL);
	hws[X5_AON_MEM_NOC_CLK] = drobot_clk_register_generator_flags_no_idle("aon_mem_noc_clk", aon_gen_src_sels, ARRAY_SIZE(aon_gen_src_sels), base + 0x820, CLK_IS_CRITICAL);
	hws[X5_AON_RTC_CLK] = drobot_clk_register_generator_flags_no_idle("aon_rtc_clk", aon_rtc_src_sels, ARRAY_SIZE(aon_rtc_src_sels), base + 0x840, CLK_IS_CRITICAL);
	hws[X5_AON_PMU_CNT_CLK] = drobot_clk_register_generator_flags_no_idle("aon_pmu_cnt_clk", aon_rtc_src_sels, ARRAY_SIZE(aon_rtc_src_sels), base + 0x860, CLK_IS_CRITICAL);

	hws[X5_AON_PMU_PCLK] = drobot_clk_hw_register_gate_no_idle("aon_pmu_pclk", "aon_apb0_clk", base + AON_CLK_ENB, 0, CLK_IS_CRITICAL);
	hws[X5_AON_SRAM_CLK] = drobot_clk_hw_register_gate_no_idle("aon_sram_clk", "aon_mem_noc_clk", base + AON_CLK_ENB, 2, CLK_IS_CRITICAL);
	hws[X5_AON_GPIO_PCLK] = drobot_clk_hw_register_gate_no_idle("aon_gpio_pclk", "aon_apb0_clk", base + AON_CLK_ENB, 3, CLK_IS_CRITICAL);
	hws[X5_AON_RTC_PCLK] = drobot_clk_hw_register_gate_no_idle("aon_rtc_pclk", "aon_apb0_clk", base + AON_CLK_ENB, 5, CLK_IS_CRITICAL);
	hws[X5_AON_SLCR_CLK] = drobot_clk_hw_register_gate_no_idle("aon_slcr_clk", "aon_apb0_clk", base + AON_CLK_ENB, 6, CLK_IS_CRITICAL);

	for(i = 0; i < ARRAY_SIZE(aon_gen_rates); i++)
		clk_set_rate(hws[aon_gen_rates[i].id]->clk, aon_gen_rates[i].rate);

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, &ctx->clk_hw_data);
	if (ret)
		return ret;

	return 0;
}

static int crm_hps_clk_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct drobot_clk_provider *ctx;
	struct clk_hw **hws;
	void __iomem *base;
	int ret, i;
	u32 pll_match = 0;

	ctx = devm_kzalloc(dev, struct_size(ctx, clk_hw_data.hws, X5_HPS_END_CLK), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	for (i = 0; i < X5_HPS_END_CLK; ++i)
		ctx->clk_hw_data.hws[i] = ERR_PTR(-ENOENT);

	ctx->clk_hw_data.num = X5_HPS_END_CLK;
	hws = ctx->clk_hw_data.hws;

	device_property_read_u32(dev, "pll-table", &pll_match);

	ctx->idle = drobot_idle_get_dev(dev->of_node);
	if (IS_ERR(ctx->idle)) {
		pr_err("%s(%ld): could not get idle dev\n", __func__, PTR_ERR(ctx->idle));
		return PTR_ERR(ctx->idle);
	}

	base = devm_platform_ioremap_resource(pdev, 0);
	if (WARN_ON(IS_ERR(base)))
		return PTR_ERR(base);

	hws[X5_CPU_PLL_P] = clk_hw_register_pll("cpu_pll_p", "osc", base,
			base + CPU_PLL_INTERNAL, PLL_P_OUT, x5_pll_rates, ARRAY_SIZE(x5_pll_rates));
	hws[X5_CPU_PLL_R] = clk_hw_register_pll("cpu_pll_r", "osc", base,
			base + CPU_PLL_INTERNAL, PLL_R_OUT, NULL, 0);

	hws[X5_SYS0_PLL_P] = clk_hw_register_pll("sys0_pll_p", "osc", base + 0x10,
			base + SYS0_PLL_INTERNAL, PLL_P_OUT, x5_pll_rates, ARRAY_SIZE(x5_pll_rates));
	hws[X5_SYS0_PLL_R] = clk_hw_register_pll("sys0_pll_r", "osc", base + 0x10,
			base + SYS0_PLL_INTERNAL, PLL_R_OUT, NULL, 0);

	hws[X5_SYS1_PLL_P] = clk_hw_register_pll("sys1_pll_p", "osc", base + 0x20,
			base + SYS1_PLL_INTERNAL, PLL_P_OUT, x5_pll_rates, ARRAY_SIZE(x5_pll_rates));
	hws[X5_SYS1_PLL_R] = clk_hw_register_pll("sys1_pll_r", "osc", base + 0x20,
			base + SYS1_PLL_INTERNAL, PLL_R_OUT, NULL, 0);

	hws[X5_DISP_PLL_P] = clk_hw_register_pll("disp_pll_p", "osc", base + 0x30,
			base + DISP_PLL_INTERNAL, PLL_P_OUT, x5_pll_rates, ARRAY_SIZE(x5_pll_rates));
	hws[X5_DISP_PLL_R] = clk_hw_register_pll("disp_pll_r", "osc", base + 0x30,
			base + DISP_PLL_INTERNAL, PLL_R_OUT, NULL, 0);

	hws[X5_PIXEL_PLL_P] = clk_hw_register_pll("pixel_pll_p", "osc", base + 0x40,
			base + PIXEL_PLL_INTERNAL, PLL_P_OUT, x5_pll_rates, ARRAY_SIZE(x5_pll_rates));
	hws[X5_PIXEL_PLL_R] = clk_hw_register_pll("pixel_pll_r", "osc", base + 0x40,
			base + PIXEL_PLL_INTERNAL, PLL_R_OUT, NULL, 0);

	if(pll_match == 0)
		for(i = 0; i < ARRAY_SIZE(soc_pll_rates0); i++)
			clk_set_rate(hws[soc_pll_rates0[i].id]->clk, soc_pll_rates0[i].rate);
	else
		for(i = 0; i < ARRAY_SIZE(soc_pll_rates1); i++)
			clk_set_rate(hws[soc_pll_rates1[i].id]->clk, soc_pll_rates1[i].rate);

	for(i = X5_CPU_PLL_P; i <= X5_PIXEL_PLL_R; i++)
		clk_get_rate(hws[i]->clk);

	hws[X5_TOP_NOC_CLK] = drobot_clk_register_generator_flags_no_idle("top_noc_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN, CLK_IS_CRITICAL);
	hws[X5_BPU_NOC_CLK] = drobot_clk_register_generator_flags_no_idle("bpu_noc_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x20, CLK_IS_CRITICAL);
	hws[X5_CODEC_NOC_CLK] = drobot_clk_register_generator_flags_no_idle("codec_noc_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x40, CLK_IS_CRITICAL);
	hws[X5_GPU_NOC_CLK] = drobot_clk_register_generator_flags_no_idle("gpu_noc_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x60, CLK_IS_CRITICAL);
	hws[X5_ROM_ACLK] = drobot_clk_register_generator_flags_no_idle("rom_aclk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x80, CLK_IS_CRITICAL);
	hws[X5_TOP_APB_CLK] = drobot_clk_register_generator_flags_no_idle("top_apb_clk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0xA0, CLK_IS_CRITICAL);

	hws[X5_WDT_PCLK] = drobot_clk_hw_register_gate_no_idle("wdt_pclk", "top_apb_clk", base + TOP_CLK_ENB, 4, 0);
	hws[X5_DMA_PCLK] = drobot_clk_hw_register_gate_no_idle("dma_pclk", "top_apb_clk", base + TOP_CLK_ENB, 5, 0);
	hws[X5_TOP_TIMER0_PCLK] = drobot_clk_hw_register_gate_no_idle("top_timer0_pclk", "top_apb_clk", base + TOP_CLK_ENB, 7, 0);
	hws[X5_TOP_TIMER1_PCLK] = drobot_clk_hw_register_gate_no_idle("top_timer1_pclk", "top_apb_clk", base + TOP_CLK_ENB, 9, 0);
	hws[X5_TOP_PVT_PCLK] = drobot_clk_hw_register_gate_no_idle("top_pvt_clk", "top_apb_clk", base + TOP_CLK_ENB, 11, 0);
	hws[X5_HISPEED_MS_CFG_CLK] = drobot_clk_hw_register_gate_no_idle("hispeed_master_apb_slave_cfg_clk", "top_apb_clk", base + TOP_CLK_ENB, 15, CLK_IS_CRITICAL);

	hws[X5_CPU_CORE_CLK] = drobot_clk_register_cpu("cpu_core_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0xC0, CLK_IS_CRITICAL);
	hws[X5_CPU_SCLK] = drobot_clk_register_generator_flags_no_idle("cpu_sclk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x1C0, CLK_IS_CRITICAL);
	hws[X5_CPU_PCLK] = drobot_clk_register_generator_flags_no_idle("cpu_pclk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x1E0, CLK_IS_CRITICAL);
	hws[X5_CPU_ATCLK] = drobot_clk_register_generator_flags_no_idle("cpu_atclk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x200, CLK_IS_CRITICAL);

 	hws[X5_CSS600_TSGEN_CLK] = drobot_clk_register_generator_flags_no_idle("css600_tsgen_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x240, CLK_IS_CRITICAL);
	hws[X5_CSS600_DBG_CLK] = drobot_clk_register_generator_flags_no_idle("css600_dbg_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x260, CLK_IS_CRITICAL);
	hws[X5_CSS600_TRACE_CLK] = drobot_clk_register_generator_flags_no_idle("css600_trace_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x280, CLK_IS_CRITICAL);
	hws[X5_CSS600_TPIU_CLK] = drobot_clk_register_generator_flags_no_idle("css600_tpiu_clk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0x2A0, CLK_IS_CRITICAL);
	hws[X5_CPU_NOC_CLK] = drobot_clk_register_generator_flags_no_idle("cpu_noc_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x2E0, CLK_IS_CRITICAL);

	hws[X5_CAM_ISP8000_CORE_CLK] = drobot_clk_register_gen_no_flags("isp_core_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x300, ctx->idle, ISO_CG_ISP);
	hws[X5_CAM_ISP8000_AXI_CLK] = drobot_clk_register_generator_flags_no_idle("isp_axi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x320, CLK_IS_CRITICAL);
	hws[X5_CAM_CSI0_IPI_CLK] = drobot_clk_register_generator_no_idle("csi0_ipi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x340);
	hws[X5_CAM_CSI1_IPI_CLK] = drobot_clk_register_generator_no_idle("csi1_ipi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x360);
	hws[X5_CAM_CSI2_IPI_CLK] = drobot_clk_register_generator_no_idle("csi2_ipi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x380);
	hws[X5_CAM_CSI3_IPI_CLK] = drobot_clk_register_generator_no_idle("csi3_ipi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x3A0);
	hws[X5_CAM_CSI_TX_CLK] = drobot_clk_register_generator_no_idle("csi_tx_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x3C0);
	hws[X5_CAM_SIF_AXI_CLK] = drobot_clk_register_gen_no_flags("sif_axi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x3E0, ctx->idle, ISO_CG_SIF);
	hws[X5_CAM_DEWARP_CORE_CLK] = drobot_clk_register_generator_no_idle("dewarp_core_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x420);
	hws[X5_CAM_DEWARP_AXI_CLK] = drobot_clk_register_generator_no_idle("dewarp_axi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x400);
	hws[X5_CAM_VSE_CORE_CLK] = drobot_clk_register_gen_no_flags("vse_core_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x440, ctx->idle, ISO_CG_DW230);
	hws[X5_CAM_VSE_UPSCALE_CLK] = drobot_clk_register_gen_no_flags("vse_upscale_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x460, ctx->idle, ISO_CG_DW230);
	hws[X5_CAM_VSE_AXI_CLK] = drobot_clk_register_gen_no_flags("vse_axi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x480, ctx->idle, ISO_CG_DW230);

	hws[X5_CAM_ISP8000_CLK] = drobot_clk_hw_register_gate("isp_clk", "isp_core_clk", base + CAMERA_CLK_ENB, 0, gate_flags, ctx->idle, ISO_CG_ISP);
	hws[X5_CAM_ISP8000_MCM_CLK] = drobot_clk_hw_register_gate("isp_mcm_clk", "isp_core_clk", base + CAMERA_CLK_ENB, 1, gate_flags, ctx->idle, ISO_CG_ISP);
	hws[X5_CAM_ISP8000_S_HCLK] = drobot_clk_hw_register_gate_no_idle("isp8000_s_hclk", "top_apb_clk", base + CAMERA_CLK_ENB, 3, 0);
	hws[X5_CAM_CSI0_PCLK] = drobot_clk_hw_register_gate_no_idle("csi0_pclk", "top_apb_clk", base + CAMERA_CLK_ENB, 8, 0);
	hws[X5_CAM_CSI1_PCLK] = drobot_clk_hw_register_gate_no_idle("csi1_pclk", "top_apb_clk", base + CAMERA_CLK_ENB, 9, 0);
	hws[X5_CAM_CSI2_PCLK] = drobot_clk_hw_register_gate_no_idle("csi2_pclk", "top_apb_clk", base + CAMERA_CLK_ENB, 10, 0);
	hws[X5_CAM_CSI3_PCLK] = drobot_clk_hw_register_gate_no_idle("csi3_pclk", "top_apb_clk", base + CAMERA_CLK_ENB, 11, 0);
	hws[X5_CAM_CSI0_CFG_CLK] = drobot_clk_hw_register_gate_no_idle("csi0_cfg_clk", "osc", base + CAMERA_CLK_ENB, 12, gate_flags);
	hws[X5_CAM_CSI1_CFG_CLK] = drobot_clk_hw_register_gate_no_idle("csi1_cfg_clk", "osc", base + CAMERA_CLK_ENB, 13, gate_flags);
	hws[X5_CAM_CSI2_CFG_CLK] = drobot_clk_hw_register_gate_no_idle("csi2_cfg_clk", "osc", base + CAMERA_CLK_ENB, 14, gate_flags);
	hws[X5_CAM_CSI3_CFG_CLK] = drobot_clk_hw_register_gate_no_idle("csi3_cfg_clk", "osc", base + CAMERA_CLK_ENB, 15, gate_flags);
	hws[X5_CAM_SIF_PCLK] = drobot_clk_hw_register_gate_no_idle("sif_pclk", "top_apb_clk", base + CAMERA_CLK_ENB, 17, 0);
	hws[X5_CAM_DEWARP_HCLK] = drobot_clk_hw_register_gate_no_idle("dewarp_hclk", "top_apb_clk", base + CAMERA_CLK_ENB, 22, 0);

	if (pll_match == 0) {
		hws[X5_DISP_BT1120_PIXEL_CLK] = drobot_clk_register_generator("bt1120_pixel_clk", disp_gen_src_sels, ARRAY_SIZE(disp_gen_src_sels),
			base + HPS_CLK_GEN + 0x4A0, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, ctx->idle, ISO_CG_BT1120);
		hws[X5_DISP_DC8000_PIXEL_CLK] = drobot_clk_register_generator("dc8000_pixel_clk", disp_gen_src_sels, ARRAY_SIZE(disp_gen_src_sels),
			base + HPS_CLK_GEN + 0x4C0, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, ctx->idle, ISO_CG_DC8000);
	} else {
		hws[X5_DISP_BT1120_PIXEL_CLK] = drobot_clk_register_generator("bt1120_pixel_clk", disp_no_pix_gen_src_sels, ARRAY_SIZE(disp_no_pix_gen_src_sels),
			base + HPS_CLK_GEN + 0x4A0, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, ctx->idle, ISO_CG_BT1120);
		hws[X5_DISP_DC8000_PIXEL_CLK] = drobot_clk_register_generator("dc8000_pixel_clk", disp_no_pix_gen_src_sels, ARRAY_SIZE(disp_no_pix_gen_src_sels),
			base + HPS_CLK_GEN + 0x4C0, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, ctx->idle, ISO_CG_DC8000);
	}

	hws[X5_DISP_SIF_ACLK] = drobot_clk_register_gen_no_flags("disp_sif_aclk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x4E0, ctx->idle, ISO_CG_DISP_SIF);
	hws[X5_DISP_BT1120_ACLK] = drobot_clk_register_gen_no_flags("bt1120_aclk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x500, ctx->idle, ISO_CG_BT1120);
	hws[X5_DISP_DC8000_ACLK] = drobot_clk_register_gen_no_flags("dc8000_aclk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x520, ctx->idle, ISO_CG_DC8000);

	if (pll_match == 0) {
		clk_set_parent(hws[X5_DISP_BT1120_PIXEL_CLK]->clk, hws[X5_DISP_PLL_P]->clk);
		clk_set_parent(hws[X5_DISP_DC8000_PIXEL_CLK]->clk, hws[X5_PIXEL_PLL_P]->clk);
	} else {
		clk_set_parent(hws[X5_DISP_BT1120_PIXEL_CLK]->clk, hws[X5_DISP_PLL_P]->clk);
		clk_set_parent(hws[X5_DISP_DC8000_PIXEL_CLK]->clk, hws[X5_DISP_PLL_P]->clk);
	}

	hws[X5_DISP_CSI_PCLK] = drobot_clk_hw_register_gate_no_idle("disp_csi_pclk", "top_apb_clk", base + HPS_MIX_CLK_ENB, 0, 0);
	hws[X5_DISP_DSI_PCLK] = drobot_clk_hw_register_gate_no_idle("disp_dsi_pclk", "top_apb_clk", base + HPS_MIX_CLK_ENB, 1, 0);
	hws[X5_DISP_DC8000_PCLK] = drobot_clk_hw_register_gate_no_idle("dc8000_pclk", "top_apb_clk", base + HPS_MIX_CLK_ENB, 2, 0);
	hws[X5_DISP_BT1120_PCLK] = drobot_clk_hw_register_gate_no_idle("bt1120_pclk", "top_apb_clk", base + HPS_MIX_CLK_ENB, 4, 0);
	hws[X5_DISP_DPHY_CFG_CLK] = drobot_clk_hw_register_gate_no_idle("dphy_cfg_clk", "osc", base + HPS_MIX_CLK_ENB, 6, gate_flags);
	hws[X5_DISP_SIF_PCLK] = drobot_clk_hw_register_gate_no_idle("disp_sif_pclk", "top_apb_clk", base + HPS_MIX_CLK_ENB, 8, 0);
	hws[X5_DISP_GPIO_PCLK] = drobot_clk_hw_register_gate_no_idle("disp_gpio_pclk", "top_apb_clk", base + HPS_MIX_CLK_ENB, 9, 0);

	hws[X5_GPU_GC820_CLK] = drobot_clk_register_gen_no_flags("gc820_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x540, ctx->idle, ISO_CG_GPU2D);
	hws[X5_GPU_GC8000L_CLK] = drobot_clk_register_gen_no_flags("gc8000l_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x560, ctx->idle, ISO_CG_GPU3D);
	hws[X5_GPU_GC820_ACLK] = drobot_clk_register_generator_flags_no_idle("gc820_aclk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x580, CLK_IS_CRITICAL);
	hws[X5_GPU_GC8000L_ACLK] = drobot_clk_register_generator_flags_no_idle("gc8000l_aclk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x5A0, CLK_IS_CRITICAL);

	hws[X5_GPU_PCLK] = drobot_clk_hw_register_gate_no_idle("gpu_pclk", "top_apb_clk", base + HPS_MIX_CLK_ENB, 12, 0);
	hws[X5_GPU_NOC_PCLK] = drobot_clk_hw_register_gate_no_idle("gpu_noc_pclk", "top_apb_clk", base + HPS_MIX_CLK_ENB, 16, CLK_IS_CRITICAL);

	hws[X5_BPU_MCLK_2X] = drobot_clk_register_generator_flags_no_idle("bpu_mclk_2x", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x5C0, CLK_IS_CRITICAL);
	hws[X5_BPU_SYS_TIMER_CLK] = drobot_clk_register_generator("bpu_timer_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x5E0, CLK_IGNORE_UNUSED, ctx->idle, ISO_PD_BPU);

	hws[X5_BPU_APB_CLK] = drobot_clk_hw_register_gate_no_idle("bpu_apb_clk", "bpu_mclk_2x", base + HPS_MIX_CLK_ENB, 17, gate_flags | CLK_IGNORE_UNUSED);
	hws[X5_BPU_MCLK_1X_CLK] = drobot_clk_hw_register_gate("bpu_mclk_1x_clk", "bpu_mclk_2x", base + HPS_MIX_CLK_ENB, 18, gate_flags | CLK_IGNORE_UNUSED, ctx->idle, ISO_PD_BPU);
	hws[X5_BPU_MCLK_2X_CLK] = drobot_clk_hw_register_gate("bpu_mclk_2x_clk", "bpu_mclk_2x", base + HPS_MIX_CLK_ENB, 19, gate_flags | CLK_IGNORE_UNUSED, ctx->idle, ISO_PD_BPU);

	hws[X5_VIDEO_CODEC_CORE_CLK] = drobot_clk_register_generator_no_idle("codec_core_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x600);
	hws[X5_VIDEO_CODEC_BCLK] = drobot_clk_register_gen_no_flags("codec_bclk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x620, ctx->idle, ISO_CG_VIDEO);
	hws[X5_VIDEO_JPEG_CORE_CLK] = drobot_clk_register_generator_no_idle("jpeg_core_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x640);

	hws[X5_VIDEO_JPEG_CCLK] = drobot_clk_hw_register_gate("jpeg_cclk", "jpeg_core_clk", base + HPS_MIX_CLK_ENB, 23, gate_flags, ctx->idle, ISO_CG_JPEG);
	hws[X5_VIDEO_JPEG_ACLK] = drobot_clk_hw_register_gate_no_idle("jpeg_aclk", "jpeg_core_clk", base + HPS_MIX_CLK_ENB, 24, gate_flags | CLK_IS_CRITICAL);
	hws[X5_VIDEO_JPEG_PCLK] = drobot_clk_hw_register_gate_no_idle("jpeg_pclk", "top_apb_clk", base + HPS_MIX_CLK_ENB, 25, 0);
	hws[X5_VIDEO_CODEC_CCLK] = drobot_clk_hw_register_gate("codec_cclk", "codec_core_clk", base + HPS_MIX_CLK_ENB, 26, gate_flags, ctx->idle, ISO_CG_VIDEO);
	hws[X5_VIDEO_CODEC_ACLK] = drobot_clk_hw_register_gate_no_idle("codec_aclk", "codec_core_clk", base + HPS_MIX_CLK_ENB, 28, gate_flags | CLK_IS_CRITICAL);
 	hws[X5_VIDEO_CODEC_PCLK] = drobot_clk_hw_register_gate_no_idle("codec_pclk", "top_apb_clk", base + HPS_MIX_CLK_ENB, 29, 0);
	hws[X5_VIDEO_NOC_PCLK] = drobot_clk_hw_register_gate_no_idle("video_noc_pclk", "top_apb_clk", base + HPS_MIX_CLK_ENB, 30, CLK_IS_CRITICAL);

	hws[X5_HSIO_QSPI_BUS_CLK] = drobot_clk_register_gen_no_flags("qspi_bus_clk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0x660, ctx->idle, ISO_CG_QSPI_AXIS);
	hws[X5_HSIO_QSPI_CORE_CLK] = drobot_clk_register_gen_no_flags("qspi_core_clk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0x680, ctx->idle, ISO_CG_QSPI_AXIS);

	hws[X5_HSIO_ENET_AXI_CLK] = drobot_clk_register_gen_no_flags("enet_axi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x6A0, ctx->idle, ISO_CG_GMAC);
	hws[X5_HSIO_ENET_RGMII_CLK] = drobot_clk_register_generator_no_idle("enet_rgmii_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x6C0);
	hws[X5_HSIO_ENET_PTP_REFCLK] = drobot_clk_register_generator_no_idle("enet_ptp_bclk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0x6E0);
	hws[X5_HSIO_ENET_REF_CLK] = drobot_clk_register_generator_no_idle("enet_ref_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x700);

	hws[X5_HSIO_ENET_PCLK] = drobot_clk_hw_register_gate_no_idle("enet_pclk", "top_apb_clk", base + HSIO_CLK_ENB, 4, 0);

	hws[X5_HSIO_24M_CLK] = drobot_clk_register_generator_flags_no_idle("hsio_24m_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x7E0, CLK_IS_CRITICAL);
	hws[X5_HSIO_EMMC_AXI_CLK] = drobot_clk_register_generator_no_idle("emmc_axi_clk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0x720);
	hws[X5_HSIO_EMMC_CCLK] = drobot_clk_register_gen_no_flags("emmc_cclk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0x740, ctx->idle, ISO_CG_EMMC);

	hws[X5_HSIO_EMMC_ACLK] = drobot_clk_hw_register_gate("emmc_aclk", "emmc_axi_clk", base + HSIO_CLK_ENB, 8, gate_flags, ctx->idle, ISO_CG_EMMC);
	hws[X5_HSIO_EMMC_HCLK] = drobot_clk_hw_register_gate_no_idle("emmc_hclk", "top_apb_clk", base + HSIO_CLK_ENB, 9, 0);
	hws[X5_HSIO_EMMC_BCLK] = drobot_clk_hw_register_gate("emmc_bclk", "emmc_axi_clk", base + HSIO_CLK_ENB, 10, gate_flags, ctx->idle, ISO_CG_EMMC);
	hws[X5_HSIO_EMMC_TCLK] = drobot_clk_hw_register_gate("emmc_tclk", "hsio_24m_clk", base + HSIO_CLK_ENB, 12, gate_flags, ctx->idle, ISO_CG_EMMC);

	hws[X5_HSIO_SDIO0_AXI_CLK] = drobot_clk_register_generator_no_idle("sdio0_axi_clk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0x760);
	hws[X5_HSIO_SDIO0_CCLK] = drobot_clk_register_gen_no_flags("sdio0_cclk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0x780, ctx->idle, ISO_CG_SD);

	hws[X5_HSIO_SDIO0_ACLK] = drobot_clk_hw_register_gate("sdio0_aclk", "sdio0_axi_clk", base + HSIO_CLK_ENB, 13, gate_flags, ctx->idle, ISO_CG_SD);
	hws[X5_HSIO_SDIO0_HCLK] = drobot_clk_hw_register_gate_no_idle("sdio0_hclk", "top_apb_clk", base + HSIO_CLK_ENB, 14, 0);
	hws[X5_HSIO_SDIO0_BCLK] = drobot_clk_hw_register_gate("sdio0_bclk", "sdio0_axi_clk", base + HSIO_CLK_ENB, 15, gate_flags, ctx->idle, ISO_CG_SD);
	hws[X5_HSIO_SDIO0_TCLK] = drobot_clk_hw_register_gate("sdio0_tclk", "hsio_24m_clk", base + HSIO_CLK_ENB, 17, gate_flags, ctx->idle, ISO_CG_SD);

	hws[X5_HSIO_SDIO1_AXI_CLK] = drobot_clk_register_generator_no_idle("sdio1_axi_clk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0x7A0);
	hws[X5_HSIO_SDIO1_CCLK] = drobot_clk_register_gen_no_flags("sdio1_cclk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0x7C0, ctx->idle, ISO_CG_SDIO);

	hws[X5_HSIO_SDIO1_ACLK] = drobot_clk_hw_register_gate("sdio1_aclk", "sdio1_axi_clk", base + HSIO_CLK_ENB, 18, gate_flags, ctx->idle, ISO_CG_SDIO);
	hws[X5_HSIO_SDIO1_HCLK] = drobot_clk_hw_register_gate_no_idle("sdio1_hclk", "top_apb_clk", base + HSIO_CLK_ENB, 19, 0);
	hws[X5_HSIO_SDIO1_BCLK] = drobot_clk_hw_register_gate("sdio1_bclk", "sdio1_axi_clk", base + HSIO_CLK_ENB, 20, gate_flags, ctx->idle, ISO_CG_SDIO);
	hws[X5_HSIO_SDIO1_TCLK] = drobot_clk_hw_register_gate("sdio1_tclk", "hsio_24m_clk", base + HSIO_CLK_ENB, 22, gate_flags, ctx->idle, ISO_CG_SDIO);

	hws[X5_HSIO_USB2_AXI_CLK] = drobot_clk_register_gen_no_flags("usb2_axi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x800, ctx->idle, ISO_CG_USB2);
	hws[X5_HSIO_USB3_AXI_CLK] = drobot_clk_register_gen_no_flags("usb3_axi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x820, ctx->idle, ISO_CG_USB3);

	hws[X5_HSIO_USB2_PCLK] = drobot_clk_hw_register_gate_no_idle("usb2_pclk", "top_apb_clk", base + HSIO_CLK_ENB, 24, 0);
	hws[X5_HSIO_USB3_PCLK] = drobot_clk_hw_register_gate_no_idle("usb3_pclk", "top_apb_clk", base + HSIO_CLK_ENB, 26, 0);
	hws[X5_HSIO_USB2_CTRL_REF_CLK] = drobot_clk_hw_register_gate("usb2_ctrl_ref_clk", "osc", base + HSIO_CLK_ENB, 27, 0, ctx->idle, ISO_CG_USB2);
	hws[X5_HSIO_USB3_CTRL_REF_CLK] = drobot_clk_hw_register_gate("usb3_ctrl_ref_clk", "osc", base + HSIO_CLK_ENB, 28, 0, ctx->idle, ISO_CG_USB3);
	hws[X5_HSIO_USB3_SUSPEND_CLK] = drobot_clk_hw_register_gate("usb3_suspend_clk", "osc", base + HSIO_CLK_ENB, 29, 0, ctx->idle, ISO_CG_USB3);

	hws[X5_HSIO_GPIO0_PCLK] = drobot_clk_hw_register_gate_no_idle("hsio_gpio0_pclk", "top_apb_clk", base + HSIO_CLK_ENB, 30, 0);
	hws[X5_HSIO_GPIO1_PCLK] = drobot_clk_hw_register_gate_no_idle("hsio_gpio1_pclk", "top_apb_clk", base + HSIO_CLK_ENB, 31, 0);

	hws[X5_SEC_AXI_CLK] = drobot_clk_register_generator("sec_axi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x840, CLK_IS_CRITICAL, ctx->idle, ISO_CG_SECURE);
	hws[X5_SEC_APB_CLK] = drobot_clk_register_generator("sec_apb_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x860, CLK_IS_CRITICAL, ctx->idle, ISO_CG_SECURE);
	hws[X5_LSIO_LPWM0_CLK] = drobot_clk_register_generator_no_idle("lpwm0_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x880);
	hws[X5_LSIO_LPWM1_CLK] = drobot_clk_register_generator_no_idle("lpwm1_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x8A0);
	hws[X5_HPS_DMA_AXI_CLK] = drobot_clk_register_gen_no_flags("hps_dma_axi_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x8C0, ctx->idle, ISO_CG_DMA);
	hws[X5_TOP_TIMER0_CLK] = drobot_clk_register_generator_no_idle("top_timer0_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x8E0);
	hws[X5_TOP_TIMER1_CLK] = drobot_clk_register_generator_no_idle("top_timer1_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x900);

	hws[X5_LSIO_SENSOR0_CLK] = drobot_clk_register_generator_flags_no_idle("lsio_sensor0_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x920, CLK_IGNORE_UNUSED);
	hws[X5_LSIO_SENSOR1_CLK] = drobot_clk_register_generator_flags_no_idle("lsio_sensor1_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x940, CLK_IGNORE_UNUSED);
	hws[X5_LSIO_SENSOR2_CLK] = drobot_clk_register_generator_flags_no_idle("lsio_sensor2_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x960, CLK_IGNORE_UNUSED);
	hws[X5_LSIO_SENSOR3_CLK] = drobot_clk_register_generator_flags_no_idle("lsio_sensor3_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x980, CLK_IGNORE_UNUSED);
	hws[X5_BPU_NOC_PCLK] = drobot_clk_register_generator_flags_no_idle("bpu_noc_pclk", apb_gen_src_sels, ARRAY_SIZE(apb_gen_src_sels), base + HPS_CLK_GEN + 0x9A0, CLK_IS_CRITICAL);
 	hws[X5_LSIO_ADC_CLK] = drobot_clk_register_generator_no_idle("adc_clk", soc_gen_src_sels, ARRAY_SIZE(soc_gen_src_sels), base + HPS_CLK_GEN + 0x9C0);

	hws[X5_LSIO_UART0_CLK] = drobot_clk_hw_register_gate_no_idle("uart0_clk", "top_apb_clk", base + LSIO_0_CLK_ENB, 0, 0);
	hws[X5_LSIO_UART1_CLK] = drobot_clk_hw_register_gate_no_idle("uart1_clk", "top_apb_clk", base + LSIO_0_CLK_ENB, 1, 0);
	hws[X5_LSIO_UART2_CLK] = drobot_clk_hw_register_gate_no_idle("uart2_clk", "top_apb_clk", base + LSIO_0_CLK_ENB, 2, 0);
	hws[X5_LSIO_UART3_CLK] = drobot_clk_hw_register_gate_no_idle("uart3_clk", "top_apb_clk", base + LSIO_0_CLK_ENB, 3, 0);
	hws[X5_LSIO_UART4_CLK] = drobot_clk_hw_register_gate_no_idle("uart4_clk", "top_apb_clk", base + LSIO_0_CLK_ENB, 4, 0);
	hws[X5_LSIO_UART0_PCLK] = drobot_clk_hw_register_gate_no_idle("uart0_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 5, 0);
	hws[X5_LSIO_UART1_PCLK] = drobot_clk_hw_register_gate_no_idle("uart1_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 6, 0);
	hws[X5_LSIO_UART2_PCLK] = drobot_clk_hw_register_gate_no_idle("uart2_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 7, 0);
	hws[X5_LSIO_UART3_PCLK] = drobot_clk_hw_register_gate_no_idle("uart3_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 8, 0);
	hws[X5_LSIO_UART4_PCLK] = drobot_clk_hw_register_gate_no_idle("uart4_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 9, 0);

	hws[X5_LSIO_SPI0_CLK] = drobot_clk_hw_register_gate_no_idle("spi0_clk", "top_apb_clk", base + LSIO_0_CLK_ENB, 10, 0);
	hws[X5_LSIO_SPI1_CLK] = drobot_clk_hw_register_gate_no_idle("spi1_clk", "top_apb_clk", base + LSIO_0_CLK_ENB, 11, 0);
	hws[X5_LSIO_SPI2_CLK] = drobot_clk_hw_register_gate_no_idle("spi2_clk", "top_apb_clk", base + LSIO_0_CLK_ENB, 12, 0);
	hws[X5_LSIO_SPI3_CLK] = drobot_clk_hw_register_gate_no_idle("spi3_clk", "top_apb_clk", base + LSIO_0_CLK_ENB, 13, 0);
	hws[X5_LSIO_SPI4_CLK] = drobot_clk_hw_register_gate_no_idle("spi4_clk", "top_apb_clk", base + LSIO_0_CLK_ENB, 14, 0);
	hws[X5_LSIO_SPI5_CLK] = drobot_clk_hw_register_gate_no_idle("spi5_clk", "top_apb_clk", base + LSIO_0_CLK_ENB, 15, 0);

	hws[X5_LSIO_LPWM0_PCLK] = drobot_clk_hw_register_gate_no_idle("lpwm0_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 18, 0);
	hws[X5_LSIO_LPWM1_PCLK] = drobot_clk_hw_register_gate_no_idle("lpwm1_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 19, 0);
	hws[X5_LSIO_GPIO0_PCLK] = drobot_clk_hw_register_gate_no_idle("gpio0_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 20, 0);
	hws[X5_LSIO_GPIO1_PCLK] = drobot_clk_hw_register_gate_no_idle("gpio1_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 21, 0);

	hws[X5_LSIO_I2C0_PCLK] = drobot_clk_hw_register_gate_no_idle("i2c0_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 22, 0);
	hws[X5_LSIO_I2C1_PCLK] = drobot_clk_hw_register_gate_no_idle("i2c1_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 23, 0);
	hws[X5_LSIO_I2C2_PCLK] = drobot_clk_hw_register_gate_no_idle("i2c2_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 24, 0);
	hws[X5_LSIO_I2C3_PCLK] = drobot_clk_hw_register_gate_no_idle("i2c3_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 25, 0);
	hws[X5_LSIO_I2C4_PCLK] = drobot_clk_hw_register_gate_no_idle("i2c4_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 26, 0);

	hws[X5_LSIO_PWM0_PCLK] = drobot_clk_hw_register_gate_no_idle("pwm0_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 27, 0);
	hws[X5_LSIO_PWM1_PCLK] = drobot_clk_hw_register_gate_no_idle("pwm1_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 28, 0);
	hws[X5_LSIO_PWM2_PCLK] = drobot_clk_hw_register_gate_no_idle("pwm2_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 29, 0);
	hws[X5_LSIO_PWM3_PCLK] = drobot_clk_hw_register_gate_no_idle("pwm3_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 30, 0);

	hws[X5_LSIO_PCLK] = drobot_clk_hw_register_gate_no_idle("lsio_pclk", "top_apb_clk", base + LSIO_0_CLK_ENB, 31, CLK_IS_CRITICAL);

	hws[X5_LSIO_ADC_PCLK] = drobot_clk_hw_register_gate_no_idle("adc_pclk", "top_apb_clk", base + LSIO_1_CLK_ENB, 0, 0);
	hws[X5_LSIO_UART5_CLK] = drobot_clk_hw_register_gate_no_idle("uart5_clk", "top_apb_clk", base + LSIO_1_CLK_ENB, 2, 0);
	hws[X5_LSIO_UART6_CLK] = drobot_clk_hw_register_gate_no_idle("uart6_clk", "top_apb_clk", base + LSIO_1_CLK_ENB, 3, 0);
	hws[X5_LSIO_UART5_PCLK] = drobot_clk_hw_register_gate_no_idle("uart5_pclk", "top_apb_clk", base + LSIO_1_CLK_ENB, 4, 0);
	hws[X5_LSIO_UART6_PCLK] = drobot_clk_hw_register_gate_no_idle("uart6_pclk", "top_apb_clk", base + LSIO_1_CLK_ENB, 5, 0);
	hws[X5_LSIO_I2C5_PCLK] = drobot_clk_hw_register_gate_no_idle("i2c5_pclk", "top_apb_clk", base + LSIO_1_CLK_ENB, 6, 0);
	hws[X5_LSIO_I2C6_PCLK] = drobot_clk_hw_register_gate_no_idle("i2c6_pclk", "top_apb_clk", base + LSIO_1_CLK_ENB, 7, 0);

	for(i = 0; i < ARRAY_SIZE(soc_gen_rates); i++)
		clk_set_rate(hws[soc_gen_rates[i].id]->clk, soc_gen_rates[i].rate);

	if (pll_match == 0) {
		clk_set_rate(hws[X5_CPU_SCLK]->clk, 1200000000);
	} else {
		clk_set_rate(hws[X5_CPU_SCLK]->clk, 1200000000);
		// clk_set_rate(hws[X5_CPU_PLL_P]->clk, 1800000000);
	}

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, &ctx->clk_hw_data);
	if (ret)
		return ret;

	return ret;
}

static int x5_clk_probe(struct platform_device *pdev)
{
	int (*probe)(struct platform_device *pdev);

	probe = of_device_get_match_data(&pdev->dev);

	if (probe)
		return probe(pdev);

	return 0;
}

static const struct of_device_id x5_clk_dt_ids[] = {
	{ .compatible = "horizon, x5-dsp-clk", .data = crm_dsp_clk_init },
	{ .compatible = "horizon, x5-aon-clk", .data = crm_aon_clk_init },
	{ .compatible = "horizon, x5-hps-clk", .data = crm_hps_clk_init },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, x5_clk_dt_ids);

static struct platform_driver x5_clk_driver = {
	.probe	= x5_clk_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.suppress_bind_attrs = true,
		.of_match_table	= x5_clk_dt_ids,
	},
};

static int __init drobot_clk_drv_register(void)
{
	return platform_driver_register(&x5_clk_driver);
}
postcore_initcall(drobot_clk_drv_register);
