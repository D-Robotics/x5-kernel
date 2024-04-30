// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024, VeriSilicon Holdings Co., Ltd. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/media-bus-format.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_drv.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>
#include <sound/hdmi-codec.h>
#include <video/videomode.h>

#define MODE_1080P 0x10
#define DEBUG_MSEEAGE 0
#define USE_DDR_CLK 0

#define LT8618_CHIP_NAME "LT8618"
#define LT8618_CHIP_ADDR 0x3B
#define LT8618_GPIO_PLACEMENT_OFFSET 16U

#define LT8618_REG_START 0x0000
#define LT8618_REG_END 0x8585
#define LT8618_REG_PAGE 0x100
#define LT8618_REG_PAGE_SELECT 0xFF

#define LT8618_REG_ENABLE 0x80EE
#define LT8618_REG_CHIP_ENABLE_POS 0U
#define LT8618_REG_CHIP_ENABLE_MSK BIT_MASK(LT8618_REG_CHIP_ENABLE_POS)
#define CHIP_ENABLE 1U
#define ENABLE_REG_BANK 0x01

#define DDC_SPEED_10K 0x64
#define DDC_SPEED_50K 0x14
#define DDC_SPEED_100K 0x0A
#define DDC_SPEED_200K 0x05

/* General Registers */
#define LT8618_REG_CHIP_VERSION_BASE 0x8000
#define CHIP_VERSION_LEN 3U
#define LT8618_REG_CHIP_VERSION(x) (LT8618_REG_CHIP_VERSION_BASE + (x))
#define V_YEAR 0U
#define V_MONTH 1U
#define V_REVISION 2U

#define LT8618_REG_INPUT_VIDEO_TYPE 0x800A

#define LT8618_REG_INT_CFG 0x8210
#define INT_CFG_POLARITY_POS 0U
#define INT_CFG_POLARITY_MSK BIT_MASK(INT_CFG_POLARITY_POS)
#define INT_CFG_POLARITY_ACTIVE_HIGH 1U
#define INT_CFG_POLARITY_ACTIVE_LOW 0U

#define LT8618_REG_FREQ_METER2_BASE 0x821D
#define FREQ_METER2_LEN 3U
#define LT8618_REG_FREQ_METER2(x) (LT8618_REG_FREQ_METER2_BASE + (x))
#define FREQ_METER2_STATUS 0U
#define FREQ_METER2_FREQ_CHANGE_FLAG_POS 7U
#define FREQ_METER2_FREQ_CHANGE_FLAG_MSK \
	BIT_MASK(FREQ_METER2_FREQ_CHANGE_FLAG_POS)
#define FREQ_CHANGE 1U
#define FREQ_UNCHANGE 0U
#define FREQ_METER2_FREQ_DETECT_FLAG_POS 6U
#define FREQ_METER2_FREQ_DETECT_FLAG_MSK \
	BIT_MASK(FREQ_METER2_FREQ_DETECT_FLAG_POS)
#define FREQ_DETECT 1U
#define FREQ_UNDETECT 0U
#define FREQ_METER2_FREQ_STABLE_FLAG_POS 5U
#define FREQ_METER2_FREQ_STABLE_FLAG_MSK \
	BIT_MASK(FREQ_METER2_FREQ_STABLE_FLAG_POS)
#define FREQ_STABLE 1U
#define FREQ_UNSTABLE 0U
#define FREQ_METER2_PCLK_POS 0U
#define FREQ_METER2_PCLK_MSK (0x7 << FREQ_METER2_PCLK_POS)
#define FREQ_METER2_PCLK_H 1U
#define FREQ_METER2_PCLK_L 2U

#define LT8618_REG_INPUT_DATA_LANE_SEQ 0x8245
#define LT8618_REG_INPUT_VIDEO_SYNC_GEN 0x8247
#define LT8618_REG_INPUT_SIGNAL_SAMPLE_TYPE 0x824F
#define LT8618_REG_INPUT_SRC_SELECT 0x8250

#define LT8618_REG_VIDEO_CHECK_SELECT 0x8251
#define LT8618_REG_EMBEDDED_SYNC_MODE_INPUT_ENABLE 0x8248

#define LT8618_REG_LINK_STATUS 0x825E
#define LINK_STATUS_CEC_POS 3U
#define LINK_STATUS_CEC_MSK BIT_MASK(LINK_STATUS_CEC_POS)
#define LINK_STATUS_INPUT_POS 2U
#define LINK_STATUS_INPUT_MSK BIT_MASK(LINK_STATUS_INPUT_POS)
#define LINK_STATUS_OUTPUT_AC_POS 1U
#define LINK_STATUS_OUTPUT_AC_MSK BIT_MASK(LINK_STATUS_OUTPUT_AC_POS)
#define LINK_STATUS_OUTPUT_DC_POS 0U
#define LINK_STATUS_OUTPUT_DC_MSK BIT_MASK(LINK_STATUS_OUTPUT_DC_POS)
#define LINK_STATUS_STABLE 1U
#define LINK_STATUS_UNSTABLE 0U

#define LT8618_REG_INPUT_VIDEO_TIMING_BASE 0x8270
#define INPUT_VIDEO_TIMING_LEN 18U
#define LT8618_REG_INPUT_VIDEO_TIMING_PARAMETER \
	LT8618_REG_INPUT_VIDEO_TIMING_BASE
#define INPUT_VIDEO_TIMING_VS_POL_POS 1U
#define INPUT_VIDEO_TIMING_VS_POL_MSK BIT_MASK(INPUT_VIDEO_TIMING_HS_POL_POS)
#define INPUT_VIDEO_TIMING_VS_POL_P 1U
#define INPUT_VIDEO_TIMING_VS_POL_N 0U
#define INPUT_VIDEO_TIMING_HS_POL_POS 0U
#define INPUT_VIDEO_TIMING_HS_POL_MSK BIT_MASK(INPUT_VIDEO_TIMING_PCLK_POL_POS)
#define INPUT_VIDEO_TIMING_HS_POL_P 1U
#define INPUT_VIDEO_TIMING_HS_POL_N 0U
#define LT8618_REG_INPUT_VIDEO_TIMING_VSW 0x8271
#define LT8618_REG_INPUT_VIDEO_TIMING_HSW_H 0x8272
#define LT8618_REG_INPUT_VIDEO_TIMING_HSW_L 0x8273
#define LT8618_REG_INPUT_VIDEO_TIMING_VBP_L 0x8274
#define LT8618_REG_INPUT_VIDEO_TIMING_VFP_L 0x8275
#define LT8618_REG_INPUT_VIDEO_TIMING_HBP_H 0x8276
#define LT8618_REG_INPUT_VIDEO_TIMING_HBP_L 0x8277
#define LT8618_REG_INPUT_VIDEO_TIMING_HFP_H 0x8278
#define LT8618_REG_INPUT_VIDEO_TIMING_HFP_L 0x8279
#define LT8618_REG_INPUT_VIDEO_TIMING_VTO_H 0x827A
#define LT8618_REG_INPUT_VIDEO_TIMING_VTO_L 0x827B
#define LT8618_REG_INPUT_VIDEO_TIMING_HTO_H 0x827C
#define LT8618_REG_INPUT_VIDEO_TIMING_HTO_L 0x827D
#define LT8618_REG_INPUT_VIDEO_TIMING_VAC_H 0x827E
#define LT8618_REG_INPUT_VIDEO_TIMING_VAC_L 0x827F
#define LT8618_REG_INPUT_VIDEO_TIMING_HAC_H 0x8280
#define LT8618_REG_INPUT_VIDEO_TIMING_HAC_L 0x8281

#define LT8618_REG_DDC_BASE 0x8502
#define LT8618_REG_DDC_COUNT 5U
#define LT8618_REG_DDC_CMD 0x8507
#define LT8618_REG_DDC_CMD_POS 0U
#define LT8618_REG_DDC_CMD_MSK (0x7 << LT8618_REG_DDC_CMD_POS)
#define DDC_CMD_ABORT 0U
#define DDC_CMD_READ_BURST 1U
#define DDC_CMD_WRITE_BURST 2U
#define DDC_CMD_READ_SEEK 3U
#define DDC_CMD_READ_EDDC 4U
#define DDC_CMD_RESET 6U

#define LT8618_REG_DDC_STATUS 0x8540
#define LT8618_REG_DDC_BUS_LOST_POS 6U
#define LT8618_REG_DDC_BUS_LOST_MSK BIT_MASK(LT8618_REG_DDC_BUS_LOST_POS)
#define DDC_BUS_LOST 1U
#define LT8618_REG_DDC_BUS_BUSY_POS 5U
#define LT8618_REG_DDC_BUS_BUSY_MSK BIT_MASK(LT8618_REG_DDC_BUS_BUSY_POS)
#define DDC_BUS_BUSY 1U
#define LT8618_REG_DDC_BUS_NACK_POS 4U
#define LT8618_REG_DDC_BUS_NACK_MSK BIT_MASK(LT8618_REG_DDC_BUS_NACK_POS)
#define DDC_BUS_NACK 1U
#define LT8618_REG_DDC_BUS_DOING_POS 2U
#define LT8618_REG_DDC_BUS_DOING_MSK BIT_MASK(LT8618_REG_DDC_BUS_DOING_POS)
#define DDC_BUS_DOING 1U
#define LT8618_REG_DDC_BUS_DONE_POS 1U
#define LT8618_REG_DDC_BUS_DONE_MSK BIT_MASK(LT8618_REG_DDC_BUS_DONE_POS)
#define DDC_BUS_DONE 1U

#define LT8618_REG_FIFO_STATUS 0x8582
#define LT8618_REG_FIFO_DATA_COUNT_POS 2U
#define LT8618_REG_FIFO_DATA_COUNT_MSK (0x3F << LT8618_REG_FIFO_DATA_COUNT_POS)
#define LT8618_REG_FIFO_FULL_POS 1U
#define LT8618_REG_FIFO_FULL_MSK BIT_MASK(LT8618_REG_FIFO_FULL_POS)
#define FIFO_FULL 1U
#define LT8618_REG_FIFO_EMPTY_POS 0U
#define LT8618_REG_FIFO_EMPTY_MSK BIT_MASK(LT8618_REG_FIFO_EMPTY_POS)
#define FIFO_EMPTY 1U

#define LT8618_REG_FIFO_CONTENT 0x8583
#define LT8618_FIFO_MAX_LENGTH 32U

#define TIMING_MAX 200U
#define PHY_LT8618_TIMING_MAX 146U

#define NAME_SIZE_MAX 50U
#define DDC_FIFO_SIZE_MAX 32U


struct reg_mask_seq {
	unsigned int reg;
	unsigned int mask;
	unsigned int val;
};

#define FEATURE_BIT_MAX 2

enum bridge_phy_feature {
	SUPPORT_HPD = BIT(0),
	SUPPORT_DDC = BIT(1),
	SUPPORT_HDMI_AUX = BIT(2),
};

static const char *const feature_str[] = {
	[SUPPORT_HPD] = "HPD",
	[SUPPORT_DDC] = "DDC",
	[SUPPORT_HDMI_AUX] = "HDMI_AUX",
};

enum hdmi_mode {
	HDMI_MODE_NORMAL = 0,
	HDMI_MODE_DVI,
};

enum input_video_type {
	RGB_WITH_SYNC_DE = 0,
	RGB_WITHOUT_SYNC,
	RGB_WITHOUT_DE,
	RGB_BT1120 = 7,
};

enum input_data_lane_seq {
	DATA_LANE_SEQ_BGR = 0,
	DATA_LANE_SEQ_BRG = 3,
	DATA_LANE_SEQ_GBR,
	DATA_LANE_SEQ_GRB,
	DATA_LANE_SEQ_RBG,
	DATA_LANE_SEQ_RGB,
};

enum latch_clock_type {
	FULL_PERIOD,
	HALF_PERIOD,
};

enum input_signal_sample_type {
	SDR_CLK = 0,
	DDR_CLK,
};

enum color_space_convert {
	CSC_NONE,
	CSC_RGB2YUV,
	CSC_YUV2RGB,
};

enum hpd_status {
	hpd_status_plug_off = 0,
	hpd_status_plug_on = 1,
};

enum int_type {
	interrupt_all = 0,
	interrupt_hpd = 1,
	interrupt_max = 0xff,
};

struct ddc_status {
	struct mutex ddc_bus_mutex;
	bool ddc_bus_idle;
	bool ddc_bus_error;
	bool ddc_fifo_empty;
};

enum lt8618_chip_version {
	LT8618_VER_Unknown = 0,
	LT8618_VER_U1,
	LT8618_VER_U2,
	LT8618_VER_U3,
};

enum lt8618_pll_level {
	LT8618_PLL_LEVEL_LOW = 0,
	LT8618_PLL_LEVEL_MIDDLE,
	LT8618_PLL_LEVEL_HIGH,
};

enum lt8618_ddc_cmd {
	CMD_ABORT = 0x0,
	CMD_READ_BURST = 0x1,
	CMD_WRITE_BURST = 0x2,
	CMD_READ_SEEK = 0x3,
	CMD_READ_EDDC = 0x4,
	CMD_RESET = 0x6,
};

struct bridge_mode_config {
	bool edid_read;
	u8 edid_buf[256];

	struct {
		u32 bus_formats; /* like MEDIA_BUS_FMT_XXX */

		enum input_signal_sample_type input_signal_type;

		struct drm_display_mode *mode;
		struct videomode vmode;
	} input_mode;
	struct {
		u32 color_formats; /* like DRM_COLOR_FORMAT_XXX */
		enum hdmi_mode hdmi_output_mode;
	} output_mode;
};

struct lt8618 {
	struct i2c_client *client;
	struct regmap *regmap;
	struct drm_bridge bridge;
	struct mutex lt8618_mutex;
	struct gpio_desc *reset_gpio;
	struct drm_connector connector;
	struct i2c_mux_core *i2cmux;
	struct regulator_bulk_data supplies[2];

	u8 chip_version;
	bool sink_is_hdmi;

	struct bridge_mode_config mode_config;

	u8 fifo_length;
	u8 edid_buf[EDID_LENGTH];
	u8 fifo_buf[DDC_FIFO_SIZE_MAX];
};


static inline struct lt8618 *bridge_to_lt8618(struct drm_bridge *bridge)
{
	return container_of(bridge, struct lt8618, bridge);
}

static inline struct lt8618 *connector_to_lt8618(struct drm_connector *con)
{
	return container_of(con, struct lt8618, connector);
}

/**
 * @brief lt8618 val sequence
 */
static const u8 lt8618_pll_range_timing[][3][3] = {
	{
		{ 0x00, 0x9e, 0xaa }, 	// < 50MHz
		{ 0x00, 0x9e, 0x99 },	// 50 ~ 100M
		{ 0x00, 0x9e, 0x88 },	// > 100M
	},
	{
		{ 0x00, 0x94, 0xaa },
		{ 0x01, 0x94, 0x99 },
		{ 0x03, 0x94, 0x88 },
	},
};

/**
 * @brief lt8618 regmap sequence
 */
static const struct reg_sequence lt8618_sw_reset_seq[] = {
	/* Reset MIPI Rx logic */
	{ 0x8011, 0x00 },
	/* Reset TTL video process */
	{ 0x8013, 0xF1 },
	{ 0x8013, 0xF9 },
};

static const struct reg_sequence lt8618_input_analog_seq[] = {
	/* TTL mode */
	{ 0x8102, 0x66 },
	{ 0x810A, 0x06 },
	{ 0x8115, 0x06 },
	/* for U2 */
	//{ 0x814E, 0xA8 },
	/* Frequency meter 2 timer cycle for sys_clk */
	/* 0x61A8 = 25000, 0x6978 = 27000, 0x77EC=30700 0x7530=30000, 0xEA60=60000*/
	{ 0x821B, 0x69 },
	{ 0x821C, 0x78 },
};

static const struct reg_sequence lt8618_output_analog_seq[] = {
	/* HDMI TX PLL */
	{ 0x8123, 0x40 },
	{ 0x8124, 0x62 },
	{ 0x8126, 0x55 },
	/* U3 SDR/DDR fixed phase */
	{ 0x8129, 0x04 },
};

static const struct reg_sequence lt8618_pll_cfg_seq[] = {
	{ 0x82de, 0x00 },
	{ 0x82de, 0xC0 },

	{ 0x8016, 0xF1 },
	/* TX PLL sw_rst_n */
	{ 0x8018, 0xDC },
	{ 0x8018, 0xFC },
	{ 0x8016, 0xF3 },
};

static bool lt8618_register_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case LT8618_REG_PAGE_SELECT:
		return false;
	default:
		return true;
	}
}

static bool lt8618_register_readable(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x00:
		return false;
	case LT8618_REG_PAGE_SELECT:
	default:
		return true;
	}
}

static const struct regmap_range lt8618_rw_regs_range[] = {
	regmap_reg_range(LT8618_REG_START, LT8618_REG_END),
};

static const struct regmap_range lt8618_wo_regs_range[] = {
	regmap_reg_range(0x00FF, 0x00FF),
};

static const struct regmap_range lt8618_ro_regs_range[] = {
	regmap_reg_range(LT8618_REG_CHIP_VERSION_BASE,
			 LT8618_REG_CHIP_VERSION_BASE + CHIP_VERSION_LEN),
};

static const struct regmap_range lt8618_vo_regs_range[] = {
	regmap_reg_range(LT8618_REG_START, LT8618_REG_END),
};

/**
 * @section LT8618 driver initialize
 */
static const struct regmap_access_table lt8618_read_table = {
	.yes_ranges = lt8618_rw_regs_range,
	.n_yes_ranges = ARRAY_SIZE(lt8618_rw_regs_range),
};

static const struct regmap_access_table lt8618_write_table = {
	.yes_ranges = lt8618_rw_regs_range,
	.n_yes_ranges = ARRAY_SIZE(lt8618_rw_regs_range),
	.no_ranges = lt8618_ro_regs_range,
	.n_no_ranges = ARRAY_SIZE(lt8618_ro_regs_range),
};

static const struct regmap_access_table lt8618_volatile_table = {
	.yes_ranges = lt8618_rw_regs_range,
	.n_yes_ranges = ARRAY_SIZE(lt8618_rw_regs_range),
};

static const struct regmap_range_cfg lt8618_regmap_range_cfg[] = {
	{
		.name = "lt8618_registers",
		.range_min = LT8618_REG_START,
		.range_max = LT8618_REG_END,
		.window_start = LT8618_REG_START,
		.window_len = LT8618_REG_PAGE,
		.selector_reg = LT8618_REG_PAGE_SELECT,
		.selector_mask = 0xFF,
		.selector_shift = 0,
	},
};

static const struct regmap_config lt8618_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.reg_stride = 1,
	.max_register = LT8618_REG_END,
	.ranges = lt8618_regmap_range_cfg,
	.num_ranges = ARRAY_SIZE(lt8618_regmap_range_cfg),

	.fast_io = false,
	.cache_type = REGCACHE_RBTREE,

	.volatile_reg = lt8618_register_volatile,
	.readable_reg = lt8618_register_readable,
	.rd_table = &lt8618_read_table,
	.wr_table = &lt8618_write_table,
};

static bool lt8618_chip_id_verify(struct lt8618 *lt8618)
{
	u8 version_val[3];

	regmap_bulk_read(lt8618->regmap, LT8618_REG_CHIP_VERSION_BASE,
			 version_val, CHIP_VERSION_LEN);
	if (version_val[0] != 0x17 || version_val[1] != 0x02) {
		return false;
	}

	if (version_val[2] == 0xE1)
		lt8618->chip_version = LT8618_VER_U2;
	else if (version_val[2] == 0xE2)
		lt8618->chip_version = LT8618_VER_U3;
	else
		lt8618->chip_version = LT8618_VER_Unknown;

	lt8618->chip_version = LT8618_VER_U3;

	return true;
}

static void lt8618_reset(struct lt8618 *lt8618)
{
	if (lt8618->reset_gpio) {
		gpiod_set_value_cansleep(lt8618->reset_gpio, 0);
		msleep(100);
		gpiod_set_value_cansleep(lt8618->reset_gpio, 1);
		msleep(100);
	}
}

static int lt8618_afe_high(struct lt8618 *lt8618)
{
	/* HDMI_TX_Phy */
	regmap_write(lt8618->regmap, 0x8130, 0xEA);

	/* DC mode */
	regmap_write(lt8618->regmap, 0x8131, 0x44);
	regmap_write(lt8618->regmap, 0x8132, 0x4A);
	regmap_write(lt8618->regmap, 0x8133, 0x0B);

	regmap_write(lt8618->regmap, 0x8134, 0x00);
	regmap_write(lt8618->regmap, 0x8135, 0x00);
	regmap_write(lt8618->regmap, 0x8136, 0x00);
	regmap_write(lt8618->regmap, 0x8137, 0x44);
	regmap_write(lt8618->regmap, 0x813F, 0x0F);

	regmap_write(lt8618->regmap, 0x8140, 0xb0);
	regmap_write(lt8618->regmap, 0x8141, 0x68);
	regmap_write(lt8618->regmap, 0x8142, 0x68);
	regmap_write(lt8618->regmap, 0x8143, 0x68);
	regmap_write(lt8618->regmap, 0x8144, 0x0A);

	return 0;
}

static int lt8618_afe_set_tx(struct lt8618 *lt8618, bool enable)
{
	/* HDMI_TX_Phy */
	if (enable)
		regmap_write(lt8618->regmap, 0x8130, 0xEA);
	else
		regmap_write(lt8618->regmap, 0x8130, 0x00);

	return 0;
}

static int lt8618_audio_enable(struct lt8618 *lt8618)
{
	/* SPDIF 48KHz 32bit */
	regmap_write(lt8618->regmap, 0x82d6, 0x8e);
	regmap_write(lt8618->regmap, 0x82d7, 0x00);
	regmap_write(lt8618->regmap, 0x8406, 0x0c);
	regmap_write(lt8618->regmap, 0x8407, 0x10);
	regmap_write(lt8618->regmap, 0x840f, 0x2b);
	regmap_write(lt8618->regmap, 0x8434, 0xd4);

	regmap_write(lt8618->regmap, 0x8435, (u8)(6144 / 0x10000));
	regmap_write(lt8618->regmap, 0x8436, (u8)((6144 & 0x00ffff) / 0x1000));
	regmap_write(lt8618->regmap, 0x8437, (u8)(6144 & 0x0000FF));
	regmap_write(lt8618->regmap, 0x843c, 0x21);

	return 0;
}

/**
 * @section LT8618 INT and HPD functions
 */
static enum hpd_status lt8618_get_hpd_status(struct lt8618 *lt8618)
{
	unsigned int val;

	regmap_write(lt8618->regmap, 0x80ee, 0x01); // enable IIC

	regmap_read(lt8618->regmap, LT8618_REG_LINK_STATUS, &val);
	if (test_bit(LINK_STATUS_OUTPUT_DC_POS, (unsigned long *)&val) ==
	    LINK_STATUS_STABLE) {
		return hpd_status_plug_on;
	}

	return hpd_status_plug_off;
}

static enum drm_connector_status
lt8618_connector_detect(struct drm_connector *connector, bool force)
{
	struct lt8618 *lt8618 = connector_to_lt8618(connector);
	unsigned int status;

	status = lt8618_get_hpd_status(lt8618);

	return (status) ?
	       connector_status_connected : connector_status_disconnected;
}

static const struct drm_connector_funcs lt8618_connector_funcs = {
	.detect = lt8618_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static u8 lt8618_get_fifo_data_count(struct lt8618 *lt8618)
{
	u8 val;

	regmap_read(lt8618->regmap, LT8618_REG_FIFO_STATUS,
		    (unsigned int *)&val);

	return val >> 2;
}

static int lt8618_ddc_fifo_fetch(struct lt8618 *lt8618, u8 *buf, u8 block,
				 size_t len, size_t offset)
{
	u8 *pfifo;
	size_t pos;
	unsigned int i, retry;
	u8 ddc_status, fifo_status, fifo_data_count;
	bool ddc_idle, fifo_empty;
	u8 ddc_cfg[5];
	u8 status = 0;

	pfifo = buf;
	pos = offset + 128 * block;

	if (len > LT8618_FIFO_MAX_LENGTH || pos > EDID_LENGTH * 2) {
		DRM_ERROR("[EDID]: Failed to request DDC FIFO!\n");
		DRM_ERROR("Invalid len or pos {%ldB,%ld}\n ", len, pos);
		return -EOVERFLOW;
	}

	status = lt8618_get_hpd_status(lt8618);
	if (status != hpd_status_plug_on) {
		DRM_ERROR("[EDID]: Failed to fetch FIFO, connector plug off\n");
		return -ENODEV;
	}

	ddc_cfg[0] = 0x0A, ddc_cfg[1] = 0xC9, ddc_cfg[2] = 0xA0;
	ddc_cfg[3] = pos, ddc_cfg[4] = len;
	regmap_bulk_write(lt8618->regmap, LT8618_REG_DDC_BASE, ddc_cfg, ARRAY_SIZE(ddc_cfg));

	regmap_write(lt8618->regmap, LT8618_REG_DDC_CMD, 0x36);
	regmap_write(lt8618->regmap, LT8618_REG_DDC_CMD, 0x34);
	regmap_write(lt8618->regmap, LT8618_REG_DDC_CMD, 0x37);

	ddc_idle = fifo_empty = 0;
	for (retry = 0; retry < 5; retry++) {
		schedule_timeout(msecs_to_jiffies(1));
		regmap_read(lt8618->regmap, LT8618_REG_DDC_STATUS,
			    (unsigned int *)&ddc_status);
		ddc_idle = test_bit(LT8618_REG_DDC_BUS_DONE_POS,
				    (unsigned long *)&ddc_status);
		if (ddc_idle)
			break;
		DRM_DEBUG("FIFO fetch not complete, status=%x\n", ddc_status);
	}

	for (i = 0; i < len; i++) {
		regmap_read(lt8618->regmap, LT8618_REG_FIFO_CONTENT,
			    (unsigned int *)pfifo);
		if (DEBUG_MSEEAGE) {
			fifo_data_count = lt8618_get_fifo_data_count(lt8618);
			DRM_DEBUG("FIFO[%2d]=%02x, remaining=%2d\n", i, *pfifo,
				  fifo_data_count);
			regmap_read(lt8618->regmap, LT8618_REG_FIFO_STATUS,
				    (unsigned int *)&fifo_status);
			if (test_bit(LT8618_REG_FIFO_EMPTY_POS,
			    (unsigned long *)&fifo_status) == FIFO_EMPTY) {
				DRM_DEBUG("FIFO is empty, read back %dB\n", i);
				break;
			}
		}
		pfifo++;
	}

	return 0;
}

static int lt8618_get_edid_block(void *data, u8 *buf, unsigned int block,
				 size_t len)
{
	int ret;
	u8 *pfifo, *pbuf;
	size_t len_max, seek;
	unsigned int i, batch_num;
	struct lt8618 *lt8618;

	lt8618 = data;
	pfifo = lt8618->fifo_buf;
	pbuf = lt8618->edid_buf;
	batch_num = len / LT8618_FIFO_MAX_LENGTH;
	DRM_DEBUG("[EDID]: Request EDID block-%d %ldB, divided %d batches\n",
		block, len, batch_num);
	if (len > EDID_LENGTH) {
		ret = -EOVERFLOW;
		DRM_ERROR("[EDID]: Failed to request EDID %ldB!\n", len);
		return ret;
	}

	len_max = LT8618_FIFO_MAX_LENGTH;
	for (i = 0; i < batch_num; i++) {
		seek = i * LT8618_FIFO_MAX_LENGTH;
		ret = lt8618_ddc_fifo_fetch(lt8618, pfifo, block, len_max, seek);
		if (ret) {
			DRM_ERROR("[EDID]: Failed to fetched FIFO batches[%d/%d], ret=%d\n",
				i + 1, batch_num, ret);

			return -EIO;
		}
		DRM_DEBUG("[EDID]: Fetched EDID block-%d[%d/%d] %ldByete\n",
			  block, i + 1, batch_num, len_max);
		memcpy(pbuf, pfifo, len_max);
		pbuf += len_max;
	}
	memcpy(buf, lt8618->edid_buf, len);

	return 0;
}

static struct edid *lt8618_get_edid(struct lt8618 *lt8618,
				     struct drm_connector *connector)
{
	struct edid *edid;

	mutex_lock(&lt8618->lt8618_mutex);

	edid = drm_do_get_edid(connector, lt8618_get_edid_block, lt8618);
	if (edid) {
		if (drm_detect_hdmi_monitor(edid))
			lt8618->sink_is_hdmi = true;
		else
			lt8618->sink_is_hdmi = false;
	}

	mutex_unlock(&lt8618->lt8618_mutex);

	return edid;
}

static struct edid *lt8618_bridge_get_edid(struct drm_bridge *bridge,
					    struct drm_connector *connector)
{
	struct lt8618 *lt8618 = bridge_to_lt8618(bridge);

	return lt8618_get_edid(lt8618, connector);
}

static int lt8618_get_modes(struct drm_connector *connector)
{
	struct lt8618 *lt8618 = connector_to_lt8618(connector);
	struct edid *edid;
	unsigned int count;

	edid = lt8618_get_edid(lt8618, connector);
	if (edid) {
		drm_connector_update_edid_property(connector, edid);
		count = drm_add_edid_modes(connector, edid);
		DRM_DEBUG("Update %d edid method\n", count);
		kfree(edid);
	} else {
		count = drm_add_modes_noedid(connector, 1920, 1080);
		drm_set_preferred_mode(connector, 1024, 768);
		DRM_DEBUG("Update default edid method\n");
	}

	return count;
}

static int lt8618_phase_config(struct lt8618 *lt8618)
{
	u8 temp = 0;
	unsigned int read_val = 0;

	u8 OK_CNT = 0x00;
	u8 OK_CNT_1 = 0x00;
	u8 OK_CNT_2 = 0x00;
	u8 OK_CNT_3 = 0x00;
	u8 Jump_CNT = 0x00;
	u8 Jump_Num = 0x00;
	u8 Jump_Num_1 = 0x00;
	u8 Jump_Num_2 = 0x00;
	u8 Jump_Num_3 = 0x00;
	bool temp0_ok = 0;
	bool temp9_ok = 0;
	bool b_OK = 0;

	regmap_write(lt8618->regmap, 0x8013, 0xf1);
	msleep(5);
	regmap_write(lt8618->regmap, 0x8013, 0xf9); // Reset TTL video process
	msleep(10);

	for (temp = 0; temp < 0x0a; temp++) {
		regmap_write(lt8618->regmap, 0x8127, (0x60 + temp));

		if (USE_DDR_CLK) {
			regmap_write(lt8618->regmap, 0x814d, 0x05);
			msleep(5);
			regmap_write(lt8618->regmap, 0x814d, 0x0d);
		} else {
			regmap_write(lt8618->regmap, 0x814d, 0x01);
			msleep(5);
			regmap_write(lt8618->regmap, 0x814d, 0x09);
		}
		msleep(10);
		regmap_read(lt8618->regmap, 0x8150, &read_val);
		read_val &= 0x01;

		if (read_val == 0) {
			OK_CNT++;

			if (b_OK == 0) {
				b_OK = 1;
				Jump_CNT++;

				if (Jump_CNT == 1) {
					Jump_Num_1 = temp;
				} else if (Jump_CNT == 3) {
					Jump_Num_2 = temp;
				} else if (Jump_CNT == 5) {
					Jump_Num_3 = temp;
				}
			}

			if (Jump_CNT == 1) {
				OK_CNT_1++;
			} else if (Jump_CNT == 3) {
				OK_CNT_2++;
			} else if (Jump_CNT == 5) {
				OK_CNT_3++;
			}

			if (temp == 0) {
				temp0_ok = 1;
			}
			if (temp == 9) {
				Jump_CNT++;
				temp9_ok = 1;
			}
		} else {
			if (b_OK) {
				b_OK = 0;
				Jump_CNT++;
			}
		}
	}

	if ((Jump_CNT == 0) || (Jump_CNT > 6)) {
		pr_debug("\r\ncali phase fail ......");
		return 0;
	}

	if ((temp9_ok == 1) && (temp0_ok == 1)) {
		if (Jump_CNT == 6) {
			OK_CNT_3 = OK_CNT_3 + OK_CNT_1;
			OK_CNT_1 = 0;
		} else if (Jump_CNT == 4) {
			OK_CNT_2 = OK_CNT_2 + OK_CNT_1;
			OK_CNT_1 = 0;
		}
	}
	if (Jump_CNT >= 2) {
		if (OK_CNT_1 >= OK_CNT_2) {
			if (OK_CNT_1 >= OK_CNT_3) {
				OK_CNT = OK_CNT_1;
				Jump_Num = Jump_Num_1;
			} else {
				OK_CNT = OK_CNT_3;
				Jump_Num = Jump_Num_3;
			}
		} else {
			if (OK_CNT_2 >= OK_CNT_3) {
				OK_CNT = OK_CNT_2;
				Jump_Num = Jump_Num_2;
			} else {
				OK_CNT = OK_CNT_3;
				Jump_Num = Jump_Num_3;
			}
		}
	}

	if (USE_DDR_CLK)
		regmap_write(lt8618->regmap, 0x814d, 0x0d);
	else
		regmap_write(lt8618->regmap, 0x814d, 0x09);

	if ((Jump_CNT == 2) || (Jump_CNT == 4) || (Jump_CNT == 6)) {
		regmap_write(lt8618->regmap, 0x8127, (0x60 + (Jump_Num + (OK_CNT / 2)) % 0x0a));
	} else if (OK_CNT >= 0x09) {
		regmap_write(lt8618->regmap, 0x8127, 0x65);
	}

	return 1;
}

static enum drm_mode_status lt8618_mode_valid(struct drm_connector *connector,
					       struct drm_display_mode *mode)
{
	/* TODO: check mode */
	return MODE_OK;
}

static const struct drm_connector_helper_funcs lt8618_connector_helper_funcs = {
	.get_modes = lt8618_get_modes,
	.mode_valid =lt8618_mode_valid,
};

static int lt8618_video_input_timing_check(struct lt8618 *lt8618)
{
	int hs_pol, vs_pol;
	int horiz_total, verti_total;
	int pixel_clk;
	u8 input_video_parameters[INPUT_VIDEO_TIMING_LEN];
	u8 freq_meter2[FREQ_METER2_LEN];
	struct videomode vmode = {};
	struct drm_display_mode mode = {};

	regmap_bulk_read(lt8618->regmap, LT8618_REG_INPUT_VIDEO_TIMING_BASE,
			 &input_video_parameters, INPUT_VIDEO_TIMING_LEN);
	hs_pol = test_bit(INPUT_VIDEO_TIMING_HS_POL_POS,
			  (unsigned long *)&input_video_parameters);
	vs_pol = test_bit(INPUT_VIDEO_TIMING_VS_POL_POS,
			  (unsigned long *)&input_video_parameters);
	vmode.flags |= BIT(hs_pol);
	vmode.flags |= BIT(vs_pol + 2);
	vmode.vsync_len = input_video_parameters[1];
	vmode.hsync_len =
		(input_video_parameters[2] << 8) + input_video_parameters[3];
	vmode.vback_porch = input_video_parameters[4];
	vmode.vfront_porch = input_video_parameters[5];
	vmode.hback_porch =
		(input_video_parameters[6] << 8) + input_video_parameters[7];
	vmode.hfront_porch =
		(input_video_parameters[8] << 8) + input_video_parameters[9];
	verti_total =
		(input_video_parameters[10] << 8) + input_video_parameters[11];
	horiz_total =
		(input_video_parameters[12] << 8) + input_video_parameters[13];
	vmode.vactive =
		(input_video_parameters[14] << 8) + input_video_parameters[15];
	vmode.hactive =
		(input_video_parameters[16] << 8) + input_video_parameters[17];

	regmap_bulk_read(lt8618->regmap, LT8618_REG_FREQ_METER2_BASE, &freq_meter2,
			 FREQ_METER2_LEN);
	pixel_clk = ((freq_meter2[FREQ_METER2_STATUS] & 0x0F) << 16) +
		    (freq_meter2[FREQ_METER2_PCLK_H] << 8) +
		    freq_meter2[FREQ_METER2_PCLK_L];
	/* Raw data is in kHz */
	pixel_clk *= 1000;

	drm_display_mode_from_videomode((const struct videomode *)&vmode,
					&mode);

	drm_mode_debug_printmodeline(&mode);

	return 0;
}

static int lt8618_hdmi_output_mode(struct lt8618 *lt8618, enum hdmi_mode mode)
{
	if (mode)
		/* bit7 = 0 : DVI output; bit7 = 1: HDMI output */
		regmap_write(lt8618->regmap, 0x82D6, 0x0E);
	else
		regmap_write(lt8618->regmap, 0x82D6, 0x8E);

	return 0;
}

/* LT8618SXB supports YUV422, YUV444, RGB888
 * color space convert except YUV420
 */
static int lt8618_hdmi_csc(struct lt8618 *lt8618)
{
	/* enable ycbcr convert to rgb*/
	regmap_write(lt8618->regmap, 0x82B9, 0x18);

	return 0;
}

static int lt8618_hdmi_config(struct lt8618 *lt8618)
{
	enum hdmi_mode mode;

	mode = lt8618->mode_config.output_mode.hdmi_output_mode;

	lt8618_hdmi_csc(lt8618);
	mode = HDMI_MODE_NORMAL;
	lt8618_hdmi_output_mode(lt8618, mode);
	lt8618_audio_enable(lt8618);

	return 0;
}

static void lt8618_bridge_enable(struct drm_bridge *bridge)
{
	struct lt8618 *lt8618 = bridge_to_lt8618(bridge);

	lt8618_afe_high(lt8618);

	lt8618_phase_config(lt8618);

	lt8618_video_input_timing_check(lt8618);
}

static void lt8618_bridge_disable(struct drm_bridge *bridge)
{
	struct lt8618 *lt8618 = bridge_to_lt8618(bridge);

	lt8618_afe_set_tx(lt8618, FALSE);
}

static int lt8618_bridge_attach(struct drm_bridge *bridge,
				 enum drm_bridge_attach_flags flags)
{
	struct lt8618 *lt8618 = bridge_to_lt8618(bridge);
	struct drm_device *drm = bridge->dev;
	int ret;

	regmap_write(lt8618->regmap, 0x80ee, 0x01); // enable IIC

	if (!bridge->encoder) {
		pr_err("Parent encoder object not found\n");
		return -ENODEV;
	}

	if (flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)
		return drm_bridge_attach(bridge->encoder, NULL,
					 bridge, flags);

	drm_connector_helper_add(&lt8618->connector,
				 &lt8618_connector_helper_funcs);

	if (!drm_core_check_feature(drm, DRIVER_ATOMIC)) {
		dev_err(&lt8618->client->dev,
			"lt8618 driver is only compatible with DRM devices supporting atomic updates\n");
		return -ENOTSUPP;
	}

	ret = drm_connector_init(drm, &lt8618->connector,
				 &lt8618_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret)
		return ret;

	if (lt8618->client->irq > 0)
		lt8618->connector.polled = DRM_CONNECTOR_POLL_HPD;
	else
		lt8618->connector.polled = DRM_CONNECTOR_POLL_CONNECT;

	lt8618->connector.interlace_allowed = true;

	drm_connector_attach_encoder(&lt8618->connector, bridge->encoder);

	return 0;
}

static void lt8618_video_output_analog(struct lt8618 *lt8618)
{
	regmap_multi_reg_write(lt8618->regmap, lt8618_output_analog_seq,
				ARRAY_SIZE(lt8618_output_analog_seq));
}

static int lt8618_video_output_pll_range(struct lt8618 *lt8618)
{
	int vic;
	u8 ver, lv;
	unsigned int i;
	struct drm_display_mode *mode;
	u8 pll_range_cgf_regs[] = { 0x25, 0x2c, 0x2d };
	struct reg_sequence pll_range_cgf[3] = {};

	mode = lt8618->mode_config.input_mode.mode;
	drm_mode_debug_printmodeline(mode);
	ver = lt8618->chip_version;
	lv = mode->clock / 50000;
	if (lv > 2)
		lv = 2;

	vic = drm_match_cea_mode(mode);

	for (i = 0; i < ARRAY_SIZE(pll_range_cgf_regs); i++) {
		pll_range_cgf[i].reg = 0x8100 + pll_range_cgf_regs[i];
		pll_range_cgf[i].def =
			lt8618_pll_range_timing[0][lv][i];
	}
	regmap_multi_reg_write(lt8618->regmap, pll_range_cgf,
			       ARRAY_SIZE(pll_range_cgf));

	return 0;
}

static int lt8618_video_output_pll_cfg(struct lt8618 *lt8618)
{
	u8 clk_type;
	unsigned int i;
	unsigned int val;
	unsigned int lock, cali_val, cali_done;
	struct reg_sequence pll_cfg[] = {
		{ 0x814d, 0x09},
		{ 0x8127, 0x60 },
		{ 0x8128, 0x88 },
	};

	clk_type = lt8618->mode_config.input_mode.input_signal_type;

	if (USE_DDR_CLK)
		clk_type = DDR_CLK;

	if (clk_type == SDR_CLK)
	{
		if (lt8618->chip_version == LT8618_VER_U2)
			pll_cfg[2].def = 0x00;
	}
	else if (clk_type == DDR_CLK)
	{
		if (lt8618->chip_version == LT8618_VER_U2) {
			regmap_read(lt8618->regmap, 0x812c, &val);
			val &= 0x7F;
			val = val * 2 | 0x80;
			regmap_write(lt8618->regmap, 0x812c, val);
			pll_cfg[0].def = 0x04;
		} else if (lt8618->chip_version == LT8618_VER_U3)
			pll_cfg[0].def = 0x05;
	}
	regmap_multi_reg_write(lt8618->regmap, pll_cfg, ARRAY_SIZE(pll_cfg));

	/* sw_en_txpll_cal_en */
	regmap_read(lt8618->regmap, 0x812B, &val);
	val &= 0xFD;
	regmap_write(lt8618->regmap, 0x812B, val);

	/* sw_en_txpll_iband_set */
	regmap_read(lt8618->regmap, 0x812E, &val);
	val &= 0xFE;
	regmap_write(lt8618->regmap, 0x812E, val);

	/* txpll _sw_rst_n */
	regmap_multi_reg_write(lt8618->regmap, lt8618_pll_cfg_seq,
			       ARRAY_SIZE(lt8618_pll_cfg_seq));

	if (lt8618->chip_version == LT8618_VER_U3) {
		regmap_write(lt8618->regmap, 0x812A, 0x10 * clk_type);
		regmap_write(lt8618->regmap, 0x812A, 0x10 * clk_type + 0x20);
	}

	for (i = 0; i < 5; i++) {
		msleep(100);
		/* pll lock logic reset */
		regmap_write(lt8618->regmap, 0x8016, 0xE3);
		regmap_write(lt8618->regmap, 0x8016, 0xF3);
		regmap_read(lt8618->regmap, 0x8215, &lock);
		regmap_read(lt8618->regmap, 0x82EA, &cali_val);
		regmap_read(lt8618->regmap, 0x82EB, &cali_done);
		lock &= 0x80;
		cali_done &= 0x80;

		if (lock && cali_done && (cali_val != 0xff))
			pr_info("TXPLL LOCK.\n");
		else {
			regmap_write(lt8618->regmap, 0x8016, 0xF1);
			/* txpll _sw_rst_n */
			regmap_write(lt8618->regmap, 0x8018, 0xDC);
			regmap_write(lt8618->regmap, 0x8018, 0xFC);
			regmap_write(lt8618->regmap, 0x8016, 0xF3);
		}
	}

	return 0;
}

static int lt8618_video_output_cfg(struct lt8618 *lt8618)
{
	lt8618_video_output_analog(lt8618);
	lt8618_video_output_pll_range(lt8618);
	lt8618_video_output_pll_cfg(lt8618);

	return 0;
}

static void LT8618SXB_AVI_setting(struct lt8618 *lt8618)
{
	//AVI
	u8 AVI_PB0 = 0x00;
	u8 AVI_PB1 = 0x00;
	u8 AVI_PB2 = 0x00;
	u8 VIC_Num;

	// VIC_NUM; 720p@60: 0x04; 1080p@60: 0x10; 4k@30: 0x5F
	VIC_Num = 0x5F;

	AVI_PB1 = 0x30;
	AVI_PB2 = 0x19;

	AVI_PB0 = ((AVI_PB1 + AVI_PB2 + VIC_Num) <= 0x6f) ?
		(0x6f - AVI_PB1 - AVI_PB2 - VIC_Num) :
		(0x16f - AVI_PB1 - AVI_PB2 - VIC_Num);

	// PB0, avi packet checksum
	regmap_write(lt8618->regmap ,0x8443, AVI_PB0);
	// PB1, color space; YUV444 0x50; YUV422 0x30; RGB 0x10
	regmap_write(lt8618->regmap ,0x8444, AVI_PB1);
	// PB2, picture aspect rate; 0x19:4:3; 0x2A : 16:9
	regmap_write(lt8618->regmap ,0x8445, AVI_PB2);
	// PB4, vic; 0x10: 1080P; 0x04 : 720P
	regmap_write(lt8618->regmap ,0x8447, VIC_Num);

	//VS_IF, 4k 30hz need send VS_IF packet. Please refer to hdmi1.4 spec 8.2.3
	if (VIC_Num == 95) {
		regmap_write(lt8618->regmap ,0x843d, 0x2a); //UD1 infoframe enable

		regmap_write(lt8618->regmap ,0x8474, 0x81);
		regmap_write(lt8618->regmap ,0x8475, 0x01);
		regmap_write(lt8618->regmap ,0x8476, 0x05);
		regmap_write(lt8618->regmap ,0x8477, 0x49);
		regmap_write(lt8618->regmap ,0x8478, 0x03);
		regmap_write(lt8618->regmap ,0x8479, 0x0c);
		regmap_write(lt8618->regmap ,0x847a, 0x00);
		regmap_write(lt8618->regmap ,0x847b, 0x20);
		regmap_write(lt8618->regmap ,0x847c, 0x01);
	} else {
		regmap_write(lt8618->regmap ,0x843d, 0x0a); //UD1 infoframe disable
	}
}

static int lt8618_video_output_timing(struct lt8618 *lt8618,
				      const struct drm_display_mode *mode)
{
	unsigned int i;
	u8 video_timing_arr[30];
	struct videomode *vmode = &lt8618->mode_config.input_mode.vmode;
	u32 timing[15] = {0};

	drm_mode_debug_printmodeline(mode);
	drm_display_mode_to_videomode(mode, vmode);

	timing[0] = vmode->hactive;
	timing[1] = vmode->hfront_porch;
	timing[2] = vmode->hsync_len;
	timing[3] = 0; // v_hset
	timing[4] = 0; // h_right_threshold,interlace mode
	timing[5] = 0; // h_left_threshold,interlace mode
	timing[6] = mode->htotal;
	timing[7] = vmode->hactive;
	timing[8] = vmode->hfront_porch;
	timing[9] = vmode->hback_porch;
	timing[10] = vmode->hsync_len;
	timing[11] = vmode->vactive;
	timing[12] = vmode->vfront_porch;
	timing[13] = vmode->vback_porch;
	timing[14] = vmode->vsync_len;

	for (i = 0; i < ARRAY_SIZE(video_timing_arr); i++) {
		video_timing_arr[i] = i % 2 ? timing[i / 2] & 0xFF :
						    (timing[i / 2] >> 8) & 0xFF;
	}

	regmap_bulk_write(lt8618->regmap, 0x8220, video_timing_arr,
			  ARRAY_SIZE(video_timing_arr));

	return 0;
}

static int lt8618_video_input_param(struct lt8618 *lt8618,
				    enum input_video_type video_type,
				    enum input_data_lane_seq lane_seq,
				    enum input_signal_sample_type signal_type)
{
	const struct reg_sequence lt8618_video_input_param_seq[] = {
		/* RGB_SYNV_DE 0x800a*/
		{ LT8618_REG_INPUT_VIDEO_TYPE, video_type * 0x10 + 0x80 },
		/* Input video signal sync type 0x8247*/
		{ LT8618_REG_INPUT_VIDEO_SYNC_GEN, (video_type & 0x3) * 0x40 + 0x07 },
		/* RGB lane sequence 0x8245*/
		{ LT8618_REG_INPUT_DATA_LANE_SEQ, lane_seq * 0x10 },
		/* Input single sample type 0x824f*/
		{ LT8618_REG_INPUT_SIGNAL_SAMPLE_TYPE, signal_type * 0x40 },
		/* Input single source select 0x8250*/
		{ LT8618_REG_INPUT_SRC_SELECT, 0x00 },
		/* vedeo check clk/data select 0x8251*/
		{ LT8618_REG_VIDEO_CHECK_SELECT, 0x42 },
		/* Embedded sync mode input enable. 0x8248*/
		{ LT8618_REG_EMBEDDED_SYNC_MODE_INPUT_ENABLE, 0x08 },
	};

	regmap_multi_reg_write(lt8618->regmap, lt8618_video_input_param_seq,
			       ARRAY_SIZE(lt8618_video_input_param_seq));

	if (USE_DDR_CLK)
		regmap_write(lt8618->regmap, 0x824f, 0x80);

	return 0;
}

static int lt8618_video_input_cfg(struct lt8618 *lt8618)
{
	regmap_multi_reg_write(lt8618->regmap, lt8618_input_analog_seq,
				ARRAY_SIZE(lt8618_input_analog_seq));

	/* BT1120 input, without sync & de */
	lt8618_video_input_param(lt8618, RGB_BT1120, DATA_LANE_SEQ_RBG,
				 SDR_CLK);

	return 0;
}

static void lt8618_bridge_mode_set(struct drm_bridge *bridge,
			   const struct drm_display_mode *mode,
			   const struct drm_display_mode *adj_mode)
{
	int vic;
	struct lt8618 *lt8618 = bridge_to_lt8618(bridge);
	vic = drm_match_cea_mode(mode);

	lt8618->mode_config.input_mode.mode =
		drm_mode_duplicate(bridge->dev, mode);

	lt8618_video_input_cfg(lt8618);

	lt8618_video_output_cfg(lt8618);

	lt8618_hdmi_csc(lt8618);

	LT8618SXB_AVI_setting(lt8618);
	lt8618_video_output_timing(lt8618, mode);
}

static const struct drm_bridge_funcs lt8618_bridge_funcs = {
	.attach = lt8618_bridge_attach,
	.mode_set = lt8618_bridge_mode_set,
	.disable = lt8618_bridge_disable,
	.enable = lt8618_bridge_enable,
	.get_edid = lt8618_bridge_get_edid,
};

static const struct drm_bridge_timings default_lt8618_timings = {
	.input_bus_flags = DRM_BUS_FLAG_PIXDATA_SAMPLE_NEGEDGE
		 | DRM_BUS_FLAG_SYNC_SAMPLE_NEGEDGE
		 | DRM_BUS_FLAG_DE_HIGH,
};

static void lt8618_sw_reset(struct lt8618 *lt8618)
{
	regmap_multi_reg_write(lt8618->regmap, lt8618_sw_reset_seq,
				ARRAY_SIZE(lt8618_sw_reset_seq));
}

static void lt8618_sw_enable(struct lt8618 *lt8618)
{
	regmap_update_bits(lt8618->regmap, LT8618_REG_ENABLE,
			   LT8618_REG_CHIP_ENABLE_MSK, ENABLE_REG_BANK);
}

static int lt8618_sw_init(struct lt8618 *lt8618)
{
	lt8618_video_input_cfg(lt8618);
	lt8618_hdmi_config(lt8618);

	return 0;
}

static void lt8618_init(struct lt8618 *lt8618)
{
	struct device *dev = &lt8618->client->dev;
	u8 ret;

	lt8618_reset(lt8618);

	regmap_write(lt8618->regmap, 0x80ee, 0x01); // enable IIC

	ret = lt8618_chip_id_verify(lt8618);

	lt8618->bridge.funcs = &lt8618_bridge_funcs;
	lt8618->bridge.of_node = dev->of_node;
	lt8618->bridge.timings = &default_lt8618_timings;
	lt8618->bridge.ops = DRM_BRIDGE_OP_DETECT | DRM_BRIDGE_OP_EDID;

	if (lt8618->client->irq > 0)
		lt8618->bridge.ops |= DRM_BRIDGE_OP_HPD;

	drm_bridge_add(&lt8618->bridge);

	lt8618_sw_enable(lt8618);
	lt8618_sw_reset(lt8618);

	lt8618_sw_init(lt8618);
}

static int lt8618_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	int ret = 0;
	struct lt8618 *lt8618;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	lt8618 = devm_kzalloc(&client->dev, sizeof(struct lt8618),
					 GFP_KERNEL);
	if (!lt8618)
		return -ENOMEM;
	lt8618->client = client;
	mutex_init(&lt8618->lt8618_mutex);

	lt8618->regmap = devm_regmap_init_i2c(client, &lt8618_regmap_config);
	if (IS_ERR(lt8618->regmap))
		return PTR_ERR(lt8618->regmap);

	lt8618->reset_gpio = devm_gpiod_get_optional(&client->dev, "rst", GPIOD_OUT_LOW);
	if (IS_ERR(lt8618->reset_gpio))
	{
		/* lt8618->reset_gpio GPIO not available */
		pr_info("optional-gpio not found\n");
		goto err;
	}

	if (ret != 0)
	{
		pr_err("not found lt8618sxb device, exit probe!!!\n");
		goto err;
	}

	i2c_set_clientdata(client, lt8618);

	client->flags = I2C_CLIENT_SCCB;

	lt8618_init(lt8618);

	return 0;

err:
	if (lt8618)
	{
		devm_kfree(&client->dev, lt8618);
		lt8618 = NULL;
	}
	return ret;
}

static void lt8618_remove(struct i2c_client *client)
{
	struct lt8618 *lt8618 = i2c_get_clientdata(client);
	if (lt8618)
		devm_kfree(&client->dev, lt8618);
}

static const struct of_device_id lt8618_dt_ids[] = {
	{ .compatible = "lontium,lt8618", },
	{ }
};
MODULE_DEVICE_TABLE(of, lt8618_dt_ids);

static const struct i2c_device_id lt8618_i2c_ids[] = {
	{ "lt8618", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, lt8618_i2c_ids);

static struct i2c_driver lt8618_driver = {
	.probe = lt8618_probe,
	.remove = lt8618_remove,
	.driver = {
		.name = "lt8618",
		.of_match_table = lt8618_dt_ids,
	},
	.id_table = lt8618_i2c_ids,
};
module_i2c_driver(lt8618_driver);

MODULE_DESCRIPTION("lt8618 RGB -> HDMI bridges");
MODULE_LICENSE("GPL");