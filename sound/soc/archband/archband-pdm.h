/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef __ARCHBAND_PDM_H
#define __ARCHBAND_PDM_H

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/types.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm.h>

#define TWO_CHANNEL_SUPPORT 2
#define FOUR_CHANNEL_SUPPORT 4
#define SIX_CHANNEL_SUPPORT 6
#define EIGHT_CHANNEL_SUPPORT 8

#define ARB_CORE_NUMS 4

#define PDM_PCM_CORE_0 0
#define PDM_PCM_CORE_1 1
#define PDM_PCM_CORE_2 2
#define PDM_PCM_CORE_3 3

#define PDM_CORE_CONF(n) (0x40 + (n)*4)

#define PDM_CONF_CHSET_MASK GENMASK(1, 0)
#define PDM_CONF_CHSET_VAL(val) ((val) << 0)
#define PDM_CONF_LR_SWAP_MASK BIT(2)
#define PDM_CONF_LR_SWAP_VAL(val) ((val) << 2)
#define PDM_CONF_SOFT_MUTE_MASK BIT(3)
#define PDM_CONF_SOFT_MUTE_VAL(val) ((val) << 3)
#define PDM_CONF_DB_STEP_MASK BIT(4)
#define PDM_CONF_DB_STEP_VAL(val) ((val) << 4)
#define PDM_CONF_S_CYCLE_MASK GENMASK(7, 5)
#define PDM_CONF_S_CYCLE_VAL(val) ((val) << 5)
#define PDM_CONF_HPF_CUTOFF_MASK GENMASK(11, 8)
#define PDM_CONF_HPF_CUTOFF_VAL(val) ((val) << 8)
#define PDM_CONF_HPF_EN_MASK BIT(12)
#define PDM_CONF_HPF_EN_VAL(val) ((val) << 12)
#define PDM_CONF_PGA_L_MASK GENMASK(21, 17)
#define PDM_CONF_PGA_L_VAL(val) ((val) << 17)
#define PDM_CONF_PGA_R_MASK GENMASK(29, 25)
#define PDM_CONF_PGA_R_VAL(val) ((val) << 25)

#define PDM_CTRL_CORE_CONF(n) (0x50 + (n)*4)

#define PDM_CTRL_CORE_EN_MASK GENMASK(3, 0)
#define PDM_CTRL_CORE_EN_VAL(val) ((val) << 0)
#define PDM_CTRL_USE_CASE_MASK GENMASK(6, 4)
#define PDM_CTRL_USE_CASE_VAL(val) ((val) << 4)
#define PDM_CTRL_ACTIVE_MASK BIT(31)
#define PDM_CTRL_ACTIVE_VAL(val) ((val) << 31)

#define PDM_WRAPPER_BASE (0x8000)

#define PDM_WRAPPER_PCM_INTR_EN (PDM_WRAPPER_BASE + 0x0)
#define PDM_WARPPER_PCM_INTR_EN_MASK GENMASK(3, 0)
#define PDM_WARPPER_PCM_INTR_EN_VAL(val) ((val) << 0)

#define PDM_WRAPPER_PCM_INTR_STS (PDM_WRAPPER_BASE + 0x04)

#define PDM_WRAPPER_PCM_CHAN_CTRL (PDM_WRAPPER_BASE + 0x08)
#define PDM_WARPPER_PCM_MUTE_MASK GENMASK(7, 0)
#define PDM_WARPPER_PCM_MUTE_VAL(val) ((val) << 0)

#define PDM_WRAPPER_CORE_DMA_CR(n) (PDM_WRAPPER_BASE + (n)*0x30 + 0x10)
#define PDM_WARPPER_TRIGGER_LEVEL_MASK GENMASK(3, 0)
#define PDM_WARPPER_TRIGGER_LEVEL_VAL(val) ((val) << 0)
#define PDM_WRAPPER_CORE_DMA_EN_MASK BIT(4)
#define PDM_WRAPPER_CORE_DMA_EN_VAL(val) ((val) << 4)

#define PDM_WRAPPER_CORE_DATA_CNT(n) (PDM_WRAPPER_BASE + (n)*0x30 + 0x14)

#define PDM_WRAPPER_CORE_INTR_EN(n) (PDM_WRAPPER_BASE + (n)*0x30 + 0x18)
#define PDM_WRAPPER_CORE_INTR_EN_MASK GENMASK(3, 0)
#define PDM_WRAPPER_CORE_INTR_EN_VAL(val) ((val) << 0)

#define PDM_WRAPPER_CORE_INTR_STS(n) (PDM_WRAPPER_BASE + (n)*0x30 + 0x1c)

#define PDM_WRAPPER_CORE_DATA(n) (PDM_WRAPPER_BASE + 0xb0 + (n)*4)

#define USE_CASE_48K_OSR 1
#define USE_CASE_48K 2
#define USE_CASE_32K_OSR 3
#define USE_CASE_32K 4
#define USE_CASE_16K_OSR 5
#define USE_CASE_16K 6
#define USE_CASE_44P1K_OSR 7

union arb_pdm_snd_dma_data {
	struct snd_dmaengine_dai_dma_data dt;
};

struct pdm_clk_config_data {
	int chan_nr;
	u32 data_width;
	u32 sample_rate;
	unsigned int hmclk_rate;
	int usecase;
};

struct arb_pdm_dev {
	void __iomem *pdm_base;
	void __iomem *ctl_base;
	struct regmap *ctrl_base_reg;
	unsigned int config_reg;
	struct clk *clk;
	struct clk *pclk;
	struct device *dev;
	struct pdm_clk_config_data config;

	int core_nums;
	u32 fifo_th;
	union arb_pdm_snd_dma_data capture_dma_data;
	bool use_pio;
	bool osr;

	struct snd_pcm_substream __rcu *rx_substream;
	unsigned int rx_ptr;
};

#endif /* __ARCHBAND_PDM_H */
