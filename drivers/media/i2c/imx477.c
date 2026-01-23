// SPDX-License-Identifier: GPL-2.0
/*
 * A V4L2 driver for Sony IMX477 cameras.
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd
 *
 * Based on Sony imx219 camera driver
 * Copyright (C) 2019-2020 Raspberry Pi (Trading) Ltd
 */
#include <asm/unaligned.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>

static int dpc_enable = 1;
module_param(dpc_enable, int, 0644);
MODULE_PARM_DESC(dpc_enable, "Enable on-sensor DPC");

static int trigger_mode;
module_param(trigger_mode, int, 0644);
MODULE_PARM_DESC(trigger_mode, "Set vsync trigger mode: 1=source, 2=sink");

#define IMX477_REG_VALUE_08BIT		1
#define IMX477_REG_VALUE_16BIT		2

/* Chip ID */
#define IMX477_REG_CHIP_ID		0x0016
#define IMX477_CHIP_ID			0x0477
#define IMX378_CHIP_ID			0x0378

#define IMX477_REG_MODE_SELECT		0x0100
#define IMX477_MODE_STANDBY		0x00
#define IMX477_MODE_STREAMING		0x01

#define IMX477_REG_ORIENTATION		0x101

#define IMX477_XCLK_FREQ		24000000

#define IMX477_DEFAULT_LINK_FREQ	450000000

/* Pixel rate is fixed at 840MHz for all the modes */
#define IMX477_PIXEL_RATE		840000000

/* V_TIMING internal */
#define IMX477_REG_FRAME_LENGTH		0x0340
#define IMX477_FRAME_LENGTH_MAX		0xffdc

/* H_TIMING internal */
#define IMX477_REG_LINE_LENGTH		0x0342
#define IMX477_LINE_LENGTH_MAX		0xfff0

/* Long exposure multiplier */
#define IMX477_LONG_EXP_SHIFT_MAX	7
#define IMX477_LONG_EXP_SHIFT_REG	0x3100

/* Exposure control */
#define IMX477_REG_EXPOSURE		0x0202
#define IMX477_EXPOSURE_OFFSET		22
#define IMX477_EXPOSURE_MIN		4
#define IMX477_EXPOSURE_STEP		1
#define IMX477_EXPOSURE_DEFAULT		0x640
#define IMX477_EXPOSURE_MAX		(IMX477_FRAME_LENGTH_MAX - \
					 IMX477_EXPOSURE_OFFSET)

/* Analog gain control */
#define IMX477_REG_ANALOG_GAIN		0x0204
#define IMX477_ANA_GAIN_MIN		0
#define IMX477_ANA_GAIN_MAX		978
#define IMX477_ANA_GAIN_STEP		1
#define IMX477_ANA_GAIN_DEFAULT		0x0

/* Digital gain control */
#define IMX477_REG_DIGITAL_GAIN		0x020e
#define IMX477_DGTL_GAIN_MIN		0x0
#define IMX477_DGTL_GAIN_MAX		0x0
#define IMX477_DGTL_GAIN_DEFAULT	0x0
#define IMX477_DGTL_GAIN_STEP		1

/* Test Pattern Control */
#define IMX477_REG_TEST_PATTERN		0x0600
#define IMX477_TEST_PATTERN_DISABLE	0
#define IMX477_TEST_PATTERN_SOLID_COLOR	1
#define IMX477_TEST_PATTERN_COLOR_BARS	2
#define IMX477_TEST_PATTERN_GREY_COLOR	3
#define IMX477_TEST_PATTERN_PN9		4

/* Test pattern colour components */
#define IMX477_REG_TEST_PATTERN_R	0x0602
#define IMX477_REG_TEST_PATTERN_GR	0x0604
#define IMX477_REG_TEST_PATTERN_B	0x0606
#define IMX477_REG_TEST_PATTERN_GB	0x0608
#define IMX477_TEST_PATTERN_COLOUR_MIN	0
#define IMX477_TEST_PATTERN_COLOUR_MAX	0x0fff
#define IMX477_TEST_PATTERN_COLOUR_STEP	1
#define IMX477_TEST_PATTERN_R_DEFAULT	IMX477_TEST_PATTERN_COLOUR_MAX
#define IMX477_TEST_PATTERN_GR_DEFAULT	0
#define IMX477_TEST_PATTERN_B_DEFAULT	0
#define IMX477_TEST_PATTERN_GB_DEFAULT	0

/* Trigger mode */
#define IMX477_REG_MC_MODE		0x3f0b
#define IMX477_REG_MS_SEL		0x3041
#define IMX477_REG_XVS_IO_CTRL		0x3040
#define IMX477_REG_EXTOUT_EN		0x4b81

/* Embedded metadata stream structure */
#define IMX477_EMBEDDED_LINE_WIDTH 16384
#define IMX477_NUM_EMBEDDED_LINES 1
#define MEDIA_BUS_FMT_SENSOR_DATA MEDIA_BUS_FMT_SRGGB12_1X12

/* IMX477 native and active pixel array size. */
#define IMX477_NATIVE_WIDTH		4072U
#define IMX477_NATIVE_HEIGHT		3176U
#define IMX477_PIXEL_ARRAY_LEFT		8U
#define IMX477_PIXEL_ARRAY_TOP		16U
#define IMX477_PIXEL_ARRAY_WIDTH	4056U
#define IMX477_PIXEL_ARRAY_HEIGHT	3040U

struct imx477_reg {
	u16 address;
	u8 val;
};

struct imx477_reg_list {
	unsigned int num_of_regs;
	const struct imx477_reg *regs;
};

/* Mode : resolution and related config&values */
struct imx477_mode {
	/* Frame width */
	unsigned int width;

	/* Frame height */
	unsigned int height;

	/* H-timing in pixels */
	unsigned int line_length_pix;

	/* Highest possible framerate. */
	struct v4l2_fract timeperframe_min;

	/* Default framerate. */
	struct v4l2_fract timeperframe_default;

	/* Default register values */
	struct imx477_reg_list reg_list;
};

/* Link frequency setup */
enum {
	IMX477_LINK_FREQ_450MHZ,
	IMX477_LINK_FREQ_453MHZ,
	IMX477_LINK_FREQ_456MHZ,
};

static const s64 link_freqs[] = {
	[IMX477_LINK_FREQ_450MHZ] = 450000000,
	[IMX477_LINK_FREQ_453MHZ] = 453000000,
	[IMX477_LINK_FREQ_456MHZ] = 456000000,
};

/* 450MHz is the nominal "default" link frequency */
static const struct imx477_reg link_450Mhz_regs[] = {
	{0x030E, 0x00},
	{0x030F, 0x96},
};

static const struct imx477_reg link_453Mhz_regs[] = {
	{0x030E, 0x00},
	{0x030F, 0x97},
};

static const struct imx477_reg link_456Mhz_regs[] = {
	{0x030E, 0x00},
	{0x030F, 0x98},
};

static const struct imx477_reg_list link_freq_regs[] = {
	[IMX477_LINK_FREQ_450MHZ] = {
		.regs = link_450Mhz_regs,
		.num_of_regs = ARRAY_SIZE(link_450Mhz_regs)
	},
	[IMX477_LINK_FREQ_453MHZ] = {
		.regs = link_453Mhz_regs,
		.num_of_regs = ARRAY_SIZE(link_453Mhz_regs)
	},
	[IMX477_LINK_FREQ_456MHZ] = {
		.regs = link_456Mhz_regs,
		.num_of_regs = ARRAY_SIZE(link_456Mhz_regs)
	},
};

static const struct imx477_reg imx477_common_regs[] = {
	{0x0100, 0x00},
	{0x0136, 0x18},
	{0x0137, 0x00},
	{0x0138, 0x01},
	{0xe000, 0x00},
	{0xe07a, 0x01},
	{0x0808, 0x02},
	{0x4ae9, 0x18},
	{0x4aea, 0x08},
	{0xf61c, 0x04},
	{0xf61e, 0x04},
	{0x4ae9, 0x21},
	{0x4aea, 0x80},
	{0x38a8, 0x1f},
	{0x38a9, 0xff},
	{0x38aa, 0x1f},
	{0x38ab, 0xff},
	{0x55d4, 0x00},
	{0x55d5, 0x00},
	{0x55d6, 0x07},
	{0x55d7, 0xff},
	{0x55e8, 0x07},
	{0x55e9, 0xff},
	{0x55ea, 0x00},
	{0x55eb, 0x00},
	{0x574c, 0x07},
	{0x574d, 0xff},
	{0x574e, 0x00},
	{0x574f, 0x00},
	{0x5754, 0x00},
	{0x5755, 0x00},
	{0x5756, 0x07},
	{0x5757, 0xff},
	{0x5973, 0x04},
	{0x5974, 0x01},
	{0x5d13, 0xc3},
	{0x5d14, 0x58},
	{0x5d15, 0xa3},
	{0x5d16, 0x1d},
	{0x5d17, 0x65},
	{0x5d18, 0x8c},
	{0x5d1a, 0x06},
	{0x5d1b, 0xa9},
	{0x5d1c, 0x45},
	{0x5d1d, 0x3a},
	{0x5d1e, 0xab},
	{0x5d1f, 0x15},
	{0x5d21, 0x0e},
	{0x5d22, 0x52},
	{0x5d23, 0xaa},
	{0x5d24, 0x7d},
	{0x5d25, 0x57},
	{0x5d26, 0xa8},
	{0x5d37, 0x5a},
	{0x5d38, 0x5a},
	{0x5d77, 0x7f},
	{0x7b75, 0x0e},
	{0x7b76, 0x0b},
	{0x7b77, 0x08},
	{0x7b78, 0x0a},
	{0x7b79, 0x47},
	{0x7b7c, 0x00},
	{0x7b7d, 0x00},
	{0x8d1f, 0x00},
	{0x8d27, 0x00},
	{0x9004, 0x03},
	{0x9200, 0x50},
	{0x9201, 0x6c},
	{0x9202, 0x71},
	{0x9203, 0x00},
	{0x9204, 0x71},
	{0x9205, 0x01},
	{0x9371, 0x6a},
	{0x9373, 0x6a},
	{0x9375, 0x64},
	{0x991a, 0x00},
	{0x996b, 0x8c},
	{0x996c, 0x64},
	{0x996d, 0x50},
	{0x9a4c, 0x0d},
	{0x9a4d, 0x0d},
	{0xa001, 0x0a},
	{0xa003, 0x0a},
	{0xa005, 0x0a},
	{0xa006, 0x01},
	{0xa007, 0xc0},
	{0xa009, 0xc0},
	{0x3d8a, 0x01},
	{0x4421, 0x04},
	{0x7b3b, 0x01},
	{0x7b4c, 0x00},
	{0x9905, 0x00},
	{0x9907, 0x00},
	{0x9909, 0x00},
	{0x990b, 0x00},
	{0x9944, 0x3c},
	{0x9947, 0x3c},
	{0x994a, 0x8c},
	{0x994b, 0x50},
	{0x994c, 0x1b},
	{0x994d, 0x8c},
	{0x994e, 0x50},
	{0x994f, 0x1b},
	{0x9950, 0x8c},
	{0x9951, 0x1b},
	{0x9952, 0x0a},
	{0x9953, 0x8c},
	{0x9954, 0x1b},
	{0x9955, 0x0a},
	{0x9a13, 0x04},
	{0x9a14, 0x04},
	{0x9a19, 0x00},
	{0x9a1c, 0x04},
	{0x9a1d, 0x04},
	{0x9a26, 0x05},
	{0x9a27, 0x05},
	{0x9a2c, 0x01},
	{0x9a2d, 0x03},
	{0x9a2f, 0x05},
	{0x9a30, 0x05},
	{0x9a41, 0x00},
	{0x9a46, 0x00},
	{0x9a47, 0x00},
	{0x9c17, 0x35},
	{0x9c1d, 0x31},
	{0x9c29, 0x50},
	{0x9c3b, 0x2f},
	{0x9c41, 0x6b},
	{0x9c47, 0x2d},
	{0x9c4d, 0x40},
	{0x9c6b, 0x00},
	{0x9c71, 0xc8},
	{0x9c73, 0x32},
	{0x9c75, 0x04},
	{0x9c7d, 0x2d},
	{0x9c83, 0x40},
	{0x9c94, 0x3f},
	{0x9c95, 0x3f},
	{0x9c96, 0x3f},
	{0x9c97, 0x00},
	{0x9c98, 0x00},
	{0x9c99, 0x00},
	{0x9c9a, 0x3f},
	{0x9c9b, 0x3f},
	{0x9c9c, 0x3f},
	{0x9ca0, 0x0f},
	{0x9ca1, 0x0f},
	{0x9ca2, 0x0f},
	{0x9ca3, 0x00},
	{0x9ca4, 0x00},
	{0x9ca5, 0x00},
	{0x9ca6, 0x1e},
	{0x9ca7, 0x1e},
	{0x9ca8, 0x1e},
	{0x9ca9, 0x00},
	{0x9caa, 0x00},
	{0x9cab, 0x00},
	{0x9cac, 0x09},
	{0x9cad, 0x09},
	{0x9cae, 0x09},
	{0x9cbd, 0x50},
	{0x9cbf, 0x50},
	{0x9cc1, 0x50},
	{0x9cc3, 0x40},
	{0x9cc5, 0x40},
	{0x9cc7, 0x40},
	{0x9cc9, 0x0a},
	{0x9ccb, 0x0a},
	{0x9ccd, 0x0a},
	{0x9d17, 0x35},
	{0x9d1d, 0x31},
	{0x9d29, 0x50},
	{0x9d3b, 0x2f},
	{0x9d41, 0x6b},
	{0x9d47, 0x42},
	{0x9d4d, 0x5a},
	{0x9d6b, 0x00},
	{0x9d71, 0xc8},
	{0x9d73, 0x32},
	{0x9d75, 0x04},
	{0x9d7d, 0x42},
	{0x9d83, 0x5a},
	{0x9d94, 0x3f},
	{0x9d95, 0x3f},
	{0x9d96, 0x3f},
	{0x9d97, 0x00},
	{0x9d98, 0x00},
	{0x9d99, 0x00},
	{0x9d9a, 0x3f},
	{0x9d9b, 0x3f},
	{0x9d9c, 0x3f},
	{0x9d9d, 0x1f},
	{0x9d9e, 0x1f},
	{0x9d9f, 0x1f},
	{0x9da0, 0x0f},
	{0x9da1, 0x0f},
	{0x9da2, 0x0f},
	{0x9da3, 0x00},
	{0x9da4, 0x00},
	{0x9da5, 0x00},
	{0x9da6, 0x1e},
	{0x9da7, 0x1e},
	{0x9da8, 0x1e},
	{0x9da9, 0x00},
	{0x9daa, 0x00},
	{0x9dab, 0x00},
	{0x9dac, 0x09},
	{0x9dad, 0x09},
	{0x9dae, 0x09},
	{0x9dc9, 0x0a},
	{0x9dcb, 0x0a},
	{0x9dcd, 0x0a},
	{0x9e17, 0x35},
	{0x9e1d, 0x31},
	{0x9e29, 0x50},
	{0x9e3b, 0x2f},
	{0x9e41, 0x6b},
	{0x9e47, 0x2d},
	{0x9e4d, 0x40},
	{0x9e6b, 0x00},
	{0x9e71, 0xc8},
	{0x9e73, 0x32},
	{0x9e75, 0x04},
	{0x9e94, 0x0f},
	{0x9e95, 0x0f},
	{0x9e96, 0x0f},
	{0x9e97, 0x00},
	{0x9e98, 0x00},
	{0x9e99, 0x00},
	{0x9ea0, 0x0f},
	{0x9ea1, 0x0f},
	{0x9ea2, 0x0f},
	{0x9ea3, 0x00},
	{0x9ea4, 0x00},
	{0x9ea5, 0x00},
	{0x9ea6, 0x3f},
	{0x9ea7, 0x3f},
	{0x9ea8, 0x3f},
	{0x9ea9, 0x00},
	{0x9eaa, 0x00},
	{0x9eab, 0x00},
	{0x9eac, 0x09},
	{0x9ead, 0x09},
	{0x9eae, 0x09},
	{0x9ec9, 0x0a},
	{0x9ecb, 0x0a},
	{0x9ecd, 0x0a},
	{0x9f17, 0x35},
	{0x9f1d, 0x31},
	{0x9f29, 0x50},
	{0x9f3b, 0x2f},
	{0x9f41, 0x6b},
	{0x9f47, 0x42},
	{0x9f4d, 0x5a},
	{0x9f6b, 0x00},
	{0x9f71, 0xc8},
	{0x9f73, 0x32},
	{0x9f75, 0x04},
	{0x9f94, 0x0f},
	{0x9f95, 0x0f},
	{0x9f96, 0x0f},
	{0x9f97, 0x00},
	{0x9f98, 0x00},
	{0x9f99, 0x00},
	{0x9f9a, 0x2f},
	{0x9f9b, 0x2f},
	{0x9f9c, 0x2f},
	{0x9f9d, 0x00},
	{0x9f9e, 0x00},
	{0x9f9f, 0x00},
	{0x9fa0, 0x0f},
	{0x9fa1, 0x0f},
	{0x9fa2, 0x0f},
	{0x9fa3, 0x00},
	{0x9fa4, 0x00},
	{0x9fa5, 0x00},
	{0x9fa6, 0x1e},
	{0x9fa7, 0x1e},
	{0x9fa8, 0x1e},
	{0x9fa9, 0x00},
	{0x9faa, 0x00},
	{0x9fab, 0x00},
	{0x9fac, 0x09},
	{0x9fad, 0x09},
	{0x9fae, 0x09},
	{0x9fc9, 0x0a},
	{0x9fcb, 0x0a},
	{0x9fcd, 0x0a},
	{0xa14b, 0xff},
	{0xa151, 0x0c},
	{0xa153, 0x50},
	{0xa155, 0x02},
	{0xa157, 0x00},
	{0xa1ad, 0xff},
	{0xa1b3, 0x0c},
	{0xa1b5, 0x50},
	{0xa1b9, 0x00},
	{0xa24b, 0xff},
	{0xa257, 0x00},
	{0xa2ad, 0xff},
	{0xa2b9, 0x00},
	{0xb21f, 0x04},
	{0xb35c, 0x00},
	{0xb35e, 0x08},
	{0x0112, 0x0c},
	{0x0113, 0x0c},
	{0x0114, 0x01},
	{0x0350, 0x00},
	{0xbcf1, 0x02},
	{0x3ff9, 0x01},
};

static const struct imx477_reg imx477_image_quality[] = {
	{0x3D8A, 0x01},
	{0x7B3B, 0x01},
	{0x7B4C, 0x00},
	{0x9905, 0x00},
	{0x9907, 0x00},
	{0x9909, 0x00},
	{0x990B, 0x00},
	{0x9944, 0x3C},
	{0x9947, 0x3C},
	{0x994A, 0x8C},
	{0x994B, 0x50},
	{0x994C, 0x1B},
	{0x994D, 0x8C},
	{0x994E, 0x50},
	{0x994F, 0x1B},
	{0x9950, 0x8C},
	{0x9951, 0x1B},
	{0x9952, 0x0A},
	{0x9953, 0x8C},
	{0x9954, 0x1B},
	{0x9955, 0x0A},
	{0x9A13, 0x04},
	{0x9A14, 0x04},
	{0x9A19, 0x00},
	{0x9A1C, 0x04},
	{0x9A1D, 0x04},
	{0x9A26, 0x05},
	{0x9A27, 0x05},
	{0x9A2C, 0x01},
	{0x9A2D, 0x03},
	{0x9A2F, 0x05},
	{0x9A30, 0x05},
	{0x9A41, 0x00},
	{0x9A46, 0x00},
	{0x9A47, 0x00},
	{0x9C17, 0x35},
	{0x9C1D, 0x31},
	{0x9C29, 0x50},
	{0x9C3B, 0x2F},
	{0x9C41, 0x6B},
	{0x9C47, 0x2D},
	{0x9C4D, 0x40},
	{0x9C6B, 0x00},
	{0x9C71, 0xC8},
	{0x9C73, 0x32},
	{0x9C75, 0x04},
	{0x9C7D, 0x2D},
	{0x9C83, 0x40},
	{0x9C94, 0x3F},
	{0x9C95, 0x3F},
	{0x9C96, 0x3F},
	{0x9C97, 0x00},
	{0x9C98, 0x00},
	{0x9C99, 0x00},
	{0x9C9A, 0x3F},
	{0x9C9B, 0x3F},
	{0x9C9C, 0x3F},
	{0x9CA0, 0x0F},
	{0x9CA1, 0x0F},
	{0x9CA2, 0x0F},
	{0x9CA3, 0x00},
	{0x9CA4, 0x00},
	{0x9CA5, 0x00},
	{0x9CA6, 0x1E},
	{0x9CA7, 0x1E},
	{0x9CA8, 0x1E},
	{0x9CA9, 0x00},
	{0x9CAA, 0x00},
	{0x9CAB, 0x00},
	{0x9CAC, 0x09},
	{0x9CAD, 0x09},
	{0x9CAE, 0x09},
	{0x9CBD, 0x50},
	{0x9CBF, 0x50},
	{0x9CC1, 0x50},
	{0x9CC3, 0x40},
	{0x9CC5, 0x40},
	{0x9CC7, 0x40},
	{0x9CC9, 0x0A},
	{0x9CCB, 0x0A},
	{0x9CCD, 0x0A},
	{0x9D17, 0x35},
	{0x9D1D, 0x31},
	{0x9D29, 0x50},
	{0x9D3B, 0x2F},
	{0x9D41, 0x6B},
	{0x9D47, 0x42},
	{0x9D4D, 0x5A},
	{0x9D6B, 0x00},
	{0x9D71, 0xC8},
	{0x9D73, 0x32},
	{0x9D75, 0x04},
	{0x9D7D, 0x42},
	{0x9D83, 0x5A},
	{0x9D94, 0x3F},
	{0x9D95, 0x3F},
	{0x9D96, 0x3F},
	{0x9D97, 0x00},
	{0x9D98, 0x00},
	{0x9D99, 0x00},
	{0x9D9A, 0x3F},
	{0x9D9B, 0x3F},
	{0x9D9C, 0x3F},
	{0x9D9D, 0x1F},
	{0x9D9E, 0x1F},
	{0x9D9F, 0x1F},
	{0x9DA0, 0x0F},
	{0x9DA1, 0x0F},
	{0x9DA2, 0x0F},
	{0x9DA3, 0x00},
	{0x9DA4, 0x00},
	{0x9DA5, 0x00},
	{0x9DA6, 0x1E},
	{0x9DA7, 0x1E},
	{0x9DA8, 0x1E},
	{0x9DA9, 0x00},
	{0x9DAA, 0x00},
	{0x9DAB, 0x00},
	{0x9DAC, 0x09},
	{0x9DAD, 0x09},
	{0x9DAE, 0x09},
	{0x9DC9, 0x0A},
	{0x9DCB, 0x0A},
	{0x9DCD, 0x0A},
	{0x9E17, 0x35},
	{0x9E1D, 0x31},
	{0x9E29, 0x50},
	{0x9E3B, 0x2F},
	{0x9E41, 0x6B},
	{0x9E47, 0x2D},
	{0x9E4D, 0x40},
	{0x9E6B, 0x00},
	{0x9E71, 0xC8},
	{0x9E73, 0x32},
	{0x9E75, 0x04},
	{0x9E94, 0x0F},
	{0x9E95, 0x0F},
	{0x9E96, 0x0F},
	{0x9E97, 0x00},
	{0x9E98, 0x00},
	{0x9E99, 0x00},
	{0x9EA0, 0x0F},
	{0x9EA1, 0x0F},
	{0x9EA2, 0x0F},
	{0x9EA3, 0x00},
	{0x9EA4, 0x00},
	{0x9EA5, 0x00},
	{0x9EA6, 0x3F},
	{0x9EA7, 0x3F},
	{0x9EA8, 0x3F},
	{0x9EA9, 0x00},
	{0x9EAA, 0x00},
	{0x9EAB, 0x00},
	{0x9EAC, 0x09},
	{0x9EAD, 0x09},
	{0x9EAE, 0x09},
	{0x9EC9, 0x0A},
	{0x9ECB, 0x0A},
	{0x9ECD, 0x0A},
	{0x9F17, 0x35},
	{0x9F1D, 0x31},
	{0x9F29, 0x50},
	{0x9F3B, 0x2F},
	{0x9F41, 0x6B},
	{0x9F47, 0x42},
	{0x9F4D, 0x5A},
	{0x9F6B, 0x00},
	{0x9F71, 0xC8},
	{0x9F73, 0x32},
	{0x9F75, 0x04},
	{0x9F94, 0x0F},
	{0x9F95, 0x0F},
	{0x9F96, 0x0F},
	{0x9F97, 0x00},
	{0x9F98, 0x00},
	{0x9F99, 0x00},
	{0x9F9A, 0x2F},
	{0x9F9B, 0x2F},
	{0x9F9C, 0x2F},
	{0x9F9D, 0x00},
	{0x9F9E, 0x00},
	{0x9F9F, 0x00},
	{0x9FA0, 0x0F},
	{0x9FA1, 0x0F},
	{0x9FA2, 0x0F},
	{0x9FA3, 0x00},
	{0x9FA4, 0x00},
	{0x9FA5, 0x00},
	{0x9FA6, 0x1E},
	{0x9FA7, 0x1E},
	{0x9FA8, 0x1E},
	{0x9FA9, 0x00},
	{0x9FAA, 0x00},
	{0x9FAB, 0x00},
	{0x9FAC, 0x09},
	{0x9FAD, 0x09},
	{0x9FAE, 0x09},
	{0x9FC9, 0x0A},
	{0x9FCB, 0x0A},
	{0x9FCD, 0x0A},
	{0xA14B, 0xFF},
	{0xA151, 0x0C},
	{0xA153, 0x50},
	{0xA155, 0x02},
	{0xA157, 0x00},
	{0xA1AD, 0xFF},
	{0xA1B3, 0x0C},
	{0xA1B5, 0x50},
	{0xA1B9, 0x00},
	{0xA24B, 0xFF},
	{0xA257, 0x00},
	{0xA2AD, 0xFF},
	{0xA2B9, 0x00},
	{0xB21F, 0x04},
	{0xB35C, 0x00},
	{0xB35E, 0x08},
};
static const struct imx477_reg imx477_990p_10fps_setting[] = {
	{0x420b, 0x01},
	{0x990c, 0x00},
	{0x990d, 0x08},
	{0x9956, 0x8c},
	{0x9957, 0x64},
	{0x9958, 0x50},
	{0x9a48, 0x06},
	{0x9a49, 0x06},
	{0x9a4a, 0x06},
	{0x9a4b, 0x06},
	{0x9a4c, 0x06},
	{0x9a4d, 0x06},
	{0x0112, 0x0a},
	{0x0113, 0x0a},
	{0x0114, 0x01},
	{0x0342, 0x1a},
	{0x0343, 0x08},
	{0x0340, 0x04},
	{0x0341, 0x1a},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x02},
	{0x0347, 0x10},
	{0x0348, 0x0f},
	{0x0349, 0xd7},
	{0x034a, 0x09},
	{0x034b, 0xcf},
	{0x00e3, 0x00},
	{0x00e4, 0x00},
	{0x00fc, 0x0a},
	{0x00fd, 0x0a},
	{0x00fe, 0x0a},
	{0x00ff, 0x0a},
	{0xe013, 0x00},
	{0x0220, 0x00},
	{0x0221, 0x11},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x01},
	{0x0901, 0x22},
	{0x0902, 0x02},
	{0x3140, 0x02},
	{0x3c00, 0x00},
	{0x3c01, 0x01},
	{0x3c02, 0x9c},
	{0x3f0d, 0x00},
	{0x5748, 0x00},
	{0x5749, 0x00},
	{0x574a, 0x00},
	{0x574b, 0xa4},
	{0x7b75, 0x0e},
	{0x7b76, 0x09},
	{0x7b77, 0x08},
	{0x7b78, 0x06},
	{0x7b79, 0x34},
	{0x7b53, 0x00},
	{0x9369, 0x73},
	{0x936b, 0x64},
	{0x936d, 0x5f},
	{0x9304, 0x03},
	{0x9305, 0x80},
	{0x9e9a, 0x2f},
	{0x9e9b, 0x2f},
	{0x9e9c, 0x2f},
	{0x9e9d, 0x00},
	{0x9e9e, 0x00},
	{0x9e9f, 0x00},
	{0xa2a9, 0x27},
	{0xa2b7, 0x03},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x0408, 0x01},
	{0x0409, 0x5c},
	{0x040a, 0x00},
	{0x040b, 0x00},
	{0x040c, 0x05},
	{0x040d, 0x34},
	{0x040e, 0x03},
	{0x040f, 0xde},
	{0x034c, 0x05},
	{0x034d, 0x00},
	{0x034e, 0x03},
	{0x034f, 0xc0},
	{0x0301, 0x05},
	{0x0303, 0x02},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0xaf},
	{0x0309, 0x0a},
	{0x030b, 0x02},
	{0x030d, 0x02},
	{0x030e, 0x00},
	{0x030f, 0x96},
	{0x0310, 0x01},
	{0x0820, 0x07},
	{0x0821, 0x08},
	{0x0822, 0x00},
	{0x0823, 0x00},
	{0x080a, 0x00},
	{0x080b, 0x7f},
	{0x080c, 0x00},
	{0x080d, 0x4f},
	{0x080e, 0x00},
	{0x080f, 0x77},
	{0x0810, 0x00},
	{0x0811, 0x5f},
	{0x0812, 0x00},
	{0x0813, 0x57},
	{0x0814, 0x00},
	{0x0815, 0x4f},
	{0x0816, 0x01},
	{0x0817, 0x27},
	{0x0818, 0x00},
	{0x0819, 0x3f},
	{0xe04c, 0x00},
	{0xe04d, 0x5f},
	{0xe04e, 0x00},
	{0xe04f, 0x1f},
	{0x3e20, 0x01},
	{0x3e37, 0x00},
	{0x3f50, 0x00},
	{0x3f56, 0x00},
	{0x3f57, 0xbf},
};

static const struct imx477_reg imx477_1520p_10fps_setting[] = {
	{0x0342, 0x31},
	{0x0343, 0xc4},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x0f},
	{0x0349, 0xd7},
	{0x034a, 0x0b},
	{0x034b, 0xdf},
	{0x0220, 0x00},
	{0x0221, 0x11},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x01},
	{0x0901, 0x12},
	{0x0902, 0x02},
	{0x3140, 0x02},
	{0x3c00, 0x00},
	{0x3c01, 0x03},
	{0x3c02, 0xa2},
	{0x3f0d, 0x01},
	{0x5748, 0x07},
	{0x5749, 0xff},
	{0x574a, 0x00},
	{0x574b, 0x00},
	{0x7b53, 0x01},
	{0x9369, 0x73},
	{0x936b, 0x64},
	{0x936d, 0x5f},
	{0x9304, 0x00},
	{0x9305, 0x00},
	{0x9e9a, 0x2f},
	{0x9e9b, 0x2f},
	{0x9e9c, 0x2f},
	{0x9e9d, 0x00},
	{0x9e9e, 0x00},
	{0x9e9f, 0x00},
	{0xa2a9, 0x60},
	{0xa2b7, 0x00},
	{0x0401, 0x01},
	{0x0404, 0x00},
	{0x0405, 0x20},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040a, 0x00},
	{0x040b, 0x00},
	{0x040c, 0x0f},
	{0x040d, 0xd8},
	{0x040e, 0x0b},
	{0x040f, 0xe0},
	{0x034c, 0x07},
	{0x034d, 0xe0},
	{0x034e, 0x05},
	{0x034f, 0xf0},
	{0x0301, 0x05},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x01},
	{0x0307, 0x5e},
	{0x0309, 0x0c},
	{0x030b, 0x02},
	{0x030d, 0x02},
	{0x030e, 0x00},
	{0x030f, 0x96},
	{0x0310, 0x01},
	{0x0820, 0x07},
	{0x0821, 0x08},
	{0x0822, 0x00},
	{0x0823, 0x00},
	{0x080a, 0x00},
	{0x080b, 0x7f},
	{0x080c, 0x00},
	{0x080d, 0x4f},
	{0x080e, 0x00},
	{0x080f, 0x77},
	{0x0810, 0x00},
	{0x0811, 0x5f},
	{0x0812, 0x00},
	{0x0813, 0x57},
	{0x0814, 0x00},
	{0x0815, 0x4f},
	{0x0816, 0x01},
	{0x0817, 0x27},
	{0x0818, 0x00},
	{0x0819, 0x3f},
	{0xe04c, 0x00},
	{0xe04d, 0x7f},
	{0xe04e, 0x00},
	{0xe04f, 0x1f},
	{0x3e20, 0x01},
	{0x3e37, 0x00},
	{0x3f50, 0x00},
	{0x3f56, 0x01},
	{0x3f57, 0x6c},
};

static const struct imx477_reg imx477_3000p_10fps_setting[] = {
	{0x0342, 0x5d},
	{0x0343, 0xc0},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x0f},
	{0x0349, 0xd7},
	{0x034a, 0x0b},
	{0x034b, 0xdf},
	{0x00e3, 0x00},
	{0x00e4, 0x00},
	{0x00fc, 0x0a},
	{0x00fd, 0x0a},
	{0x00fe, 0x0a},
	{0x00ff, 0x0a},
	{0x0220, 0x00},
	{0x0221, 0x11},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x00},
	{0x0901, 0x11},
	{0x0902, 0x02},
	{0x3140, 0x02},
	{0x3c00, 0x00},
	{0x3c01, 0x03},
	{0x3c02, 0xa2},
	{0x3f0d, 0x01},
	{0x5748, 0x07},
	{0x5749, 0xff},
	{0x574a, 0x00},
	{0x574b, 0x00},
	{0x7b75, 0x0a},
	{0x7b76, 0x0c},
	{0x7b77, 0x07},
	{0x7b78, 0x06},
	{0x7b79, 0x3c},
	{0x7b53, 0x01},
	{0x9369, 0x5a},
	{0x936b, 0x55},
	{0x936d, 0x28},
	{0x9304, 0x00},
	{0x9305, 0x00},
	{0x9e9a, 0x2f},
	{0x9e9b, 0x2f},
	{0x9e9c, 0x2f},
	{0x9e9d, 0x00},
	{0x9e9e, 0x00},
	{0x9e9f, 0x00},
	{0xa2a9, 0x60},
	{0xa2b7, 0x00},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040a, 0x00},
	{0x040b, 0x00},
	{0x040c, 0x0f},
	{0x040d, 0xd8},
	{0x040e, 0x0b},
	{0x040f, 0xe0},
	{0x034c, 0x0f},
	{0x034d, 0xA0},
	{0x034e, 0x0b},
	{0x034f, 0xB8},
	{0x0301, 0x05},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x01},
	{0x0307, 0x5e},
	{0x0309, 0x0c},
	{0x030b, 0x02},
	{0x030d, 0x02},
	{0x030e, 0x00},
	{0x030f, 0x96},
	{0x0310, 0x01},
	{0x0820, 0x07},
	{0x0821, 0x08},
	{0x0822, 0x00},
	{0x0823, 0x00},
	{0x080a, 0x00},
	{0x080b, 0x7f},
	{0x080c, 0x00},
	{0x080d, 0x4f},
	{0x080e, 0x00},
	{0x080f, 0x77},
	{0x0810, 0x00},
	{0x0811, 0x5f},
	{0x0812, 0x00},
	{0x0813, 0x57},
	{0x0814, 0x00},
	{0x0815, 0x4f},
	{0x0816, 0x01},
	{0x0817, 0x27},
	{0x0818, 0x00},
	{0x0819, 0x3f},
	{0xe04c, 0x00},
	{0xe04d, 0x7f},
	{0xe04e, 0x00},
	{0xe04f, 0x1f},
	{0x3e20, 0x01},
	{0x3e37, 0x00},
	{0x3f50, 0x00},
	{0x3f56, 0x02},
	{0x3f57, 0xae},
};

static const struct imx477_reg imx477_1080p_50fps_setting[] = {
	{0x0340, 0x05}, // frame_length
	{0x0341, 0x21}, 
	{0x0342, 0x0B}, // line_length
	{0x0343, 0xA0}, 
	{0x0342, 0x31},
	{0x0343, 0xc4},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x01},
	{0x0347, 0xb8},
	{0x0348, 0x0f},
	{0x0349, 0xd7},
	{0x034a, 0x0a},
	{0x034b, 0x27},
	{0x0220, 0x00},
	{0x0221, 0x11},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x01},
	{0x0901, 0x12},
	{0x0902, 0x02},
	{0x3140, 0x02},
	{0x3c00, 0x00},
	{0x3c01, 0x03},
	{0x3c02, 0xa2},
	{0x3f0d, 0x01},
	{0x5748, 0x07},
	{0x5749, 0xff},
	{0x574a, 0x00},
	{0x574b, 0x00},
	{0x7b53, 0x01},
	{0x9369, 0x73},
	{0x936b, 0x64},
	{0x936d, 0x5f},
	{0x9304, 0x00},
	{0x9305, 0x00},
	{0x9e9a, 0x2f},
	{0x9e9b, 0x2f},
	{0x9e9c, 0x2f},
	{0x9e9d, 0x00},
	{0x9e9e, 0x00},
	{0x9e9f, 0x00},
	{0xa2a9, 0x60},
	{0xa2b7, 0x00},
	{0x0401, 0x01},
	{0x0404, 0x00},
	{0x0405, 0x20},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040a, 0x00},
	{0x040b, 0x00},
	{0x040c, 0x0f},
	{0x040d, 0xd8},
	{0x040e, 0x04},
	{0x040f, 0x38},
	{0x034c, 0x07},
	{0x034d, 0x80},
	{0x034e, 0x04},
	{0x034f, 0x38},
	{0x0301, 0x05},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x01},
	{0x0307, 0x5e},
	{0x0309, 0x0c},
	{0x030b, 0x02},
	{0x030d, 0x02},
	{0x030e, 0x00},
	{0x030f, 0x96},
	{0x0310, 0x01},
	{0x0820, 0x07},
	{0x0821, 0x08},
	{0x0822, 0x00},
	{0x0823, 0x00},
	{0x080a, 0x00},
	{0x080b, 0x7f},
	{0x080c, 0x00},
	{0x080d, 0x4f},
	{0x080e, 0x00},
	{0x080f, 0x77},
	{0x0810, 0x00},
	{0x0811, 0x5f},
	{0x0812, 0x00},
	{0x0813, 0x57},
	{0x0814, 0x00},
	{0x0815, 0x4f},
	{0x0816, 0x01},
	{0x0817, 0x27},
	{0x0818, 0x00},
	{0x0819, 0x3f},
	{0xe04c, 0x00},
	{0xe04d, 0x7f},
	{0xe04e, 0x00},
	{0xe04f, 0x1f},
	{0x3e20, 0x01},
	{0x3e37, 0x00},
	{0x3f50, 0x00},
	{0x3f56, 0x01},
	{0x3f57, 0x6c},
};

static uint32_t imx477_gain_lut[] = {
	0x000,
	0x014,
	0x02B,
	0x040,
	0x055,
	0x069,
	0x07D,
	0x090,
	0x0A3,
	0x0B5,
	0x0C8,
	0x0D9,
	0x0EA,
	0x0FB,
	0x10C,
	0x11C,
	0x12C,
	0x13B,
	0x14B,
	0x159,
	0x168,
	0x177,
	0x184,
	0x192,
	0x19F,
	0x1AC,
	0x1B9,
	0x1C5,
	0x1D1,
	0x1DD,
	0x1E9,
	0x1F5,
	0x200,
	0x20B,
	0x216,
	0x220,
	0x22A,
	0x235,
	0x23F,
	0x248,
	0x252,
	0x25B,
	0x264,
	0x26D,
	0x275,
	0x27E,
	0x286,
	0x28E,
	0x296,
	0x29E,
	0x2A5,
	0x2AD,
	0x2B4,
	0x2BB,
	0x2C2,
	0x2C9,
	0x2CF,
	0x2D6,
	0x2DD,
	0x2E2,
	0x2E9,
	0x2EF,
	0x2F5,
	0x2FA,
	0x300,
	0x306,
	0x30B,
	0x310,
	0x315,
	0x31A,
	0x31F,
	0x324,
	0x329,
	0x32D,
	0x332,
	0x336,
	0x33A,
	0x33F,
	0x343,
	0x347,
	0x34B,
	0x34F,
	0x353,
	0x357,
	0x35A,
	0x35D,
	0x361,
	0x365,
	0x368,
	0x36B,
	0x36F,
	0x372,
	0x375,
	0x377,
	0x37A,
	0x37D,
	0x380,
	0x383,
	0x386,
	0x388,
	0x38B,
	0x38E,
	0x38F,
	0x392,
	0x394,
	0x397,
	0x399,
	0x39B,
	0x39D,
	0x39F,
	0x3A2,
	0x3A4,
	0x3A5,
	0x3A7,
	0x3A9,
	0x3AB,
	0x3AD,
	0x3AF,
	0x3B1,
	0x3B2,
	0x3B4,
	0x3B5,
	0x3B7,
	0x3B9,
	0x3BA,
	0x3BC,
	0x3BE,
	0x3BE,
	0x3C0,
	0x3C1,
	0x3C3,
	0x3C4,
	0x3C5,
	0x3C7,
	0x3C8,
	0x3CA,
	0x3CA,
	0x3CB,
	0x3CC,
	0x3CE,
	0x3CF,
	0x3D0,
	0x3D1,
	0x3D2,
};

/* Mode configs */
static const struct imx477_mode supported_modes[] = {
	{
		/* 4000x3000 RAW12 10fps mode */
		.width = 4000,
		.height = 3000,
		.line_length_pix = 0x5dc0,
		.timeperframe_min = {
			.numerator = 100,
			.denominator = 1000
		},
		.timeperframe_default = {
			.numerator = 100,
			.denominator = 1000
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(imx477_3000p_10fps_setting),
			.regs = imx477_3000p_10fps_setting,
		},
	},
	{
		/* 2016x1520 RAW12 40fps mode */
		.width = 2016,
		.height = 1520,
		.line_length_pix = 0x31c4,
		.timeperframe_min = {
			.numerator = 100,
			.denominator = 4000
		},
		.timeperframe_default = {
			.numerator = 100,
			.denominator = 4000
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(imx477_1520p_10fps_setting),
			.regs = imx477_1520p_10fps_setting,
		},
	},
	{
		/* 1080p RAW12 50fps mode */
		.width = 1920,
		.height = 1080,
		.line_length_pix = 0x31c4,
		.timeperframe_min = {
			.numerator = 100,
			.denominator = 5000
		},
		.timeperframe_default = {
			.numerator = 100,
			.denominator = 5000
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(imx477_1080p_50fps_setting),
			.regs = imx477_1080p_50fps_setting,
		},
	},
	// {
	// 	/* 1280x960 RAW12 30fps mode */
	// 	.width = 1280,
	// 	.height = 960,
	// 	.line_length_pix = 0x31c4,
	// 	.timeperframe_min = {
	// 		.numerator = 100,
	// 		.denominator = 3000
	// 	},
	// 	.timeperframe_default = {
	// 		.numerator = 100,
	// 		.denominator = 3000
	// 	},
	// 	.reg_list = {
	// 		.num_of_regs = ARRAY_SIZE(imx477_990p_10fps_setting),
	// 		.regs = imx477_990p_10fps_setting,
	// 	},
	// }
};

static const char * const imx477_test_pattern_menu[] = {
	"Disabled",
	"Color Bars",
	"Solid Color",
	"Grey Color Bars",
	"PN9"
};

static const int imx477_test_pattern_val[] = {
	IMX477_TEST_PATTERN_DISABLE,
	IMX477_TEST_PATTERN_COLOR_BARS,
	IMX477_TEST_PATTERN_SOLID_COLOR,
	IMX477_TEST_PATTERN_GREY_COLOR,
	IMX477_TEST_PATTERN_PN9,
};

/* regulator supplies */
static const char * const imx477_supply_name[] = {
	/* Supplies can be enabled in any order */
	"VANA",  /* Analog (2.8V) supply */
	"VDIG",  /* Digital Core (1.05V) supply */
	"VDDL",  /* IF (1.8V) supply */
};

#define IMX477_NUM_SUPPLIES ARRAY_SIZE(imx477_supply_name)

/*
 * Initialisation delay between XCLR low->high and the moment when the sensor
 * can start capture (i.e. can leave software standby), given by T7 in the
 * datasheet is 8ms.  This does include I2C setup time as well.
 *
 * Note, that delay between XCLR low->high and reading the CCI ID register (T6
 * in the datasheet) is much smaller - 600us.
 */
#define IMX477_XCLR_MIN_DELAY_US	8000
#define IMX477_XCLR_DELAY_RANGE_US	1000

struct imx477_compatible_data {
	unsigned int chip_id;
	struct imx477_reg_list extra_regs;
};

struct imx477 {
	struct v4l2_subdev sd;
	struct media_pad pad;

	unsigned int fmt_code;

	struct clk *xclk;
	u32 xclk_freq;

	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[IMX477_NUM_SUPPLIES];

	struct v4l2_ctrl_handler ctrl_handler;
	/* V4L2 Controls */
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *vflip;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;

	unsigned int link_freq_idx;

	/* Current mode */
	const struct imx477_mode *mode;

	/* Trigger mode */
	int trigger_mode_of;

	/*
	 * Mutex for serialized access:
	 * Protect sensor module set pad format and start/stop streaming safely.
	 */
	struct mutex mutex;

	/* Streaming on/off */
	bool streaming;

	/* Rewrite common registers on stream on? */
	bool common_regs_written;

	/* Current long exposure factor in use. Set through V4L2_CID_VBLANK */
	unsigned int long_exp_shift;

	/* Any extra information related to different compatible sensors */
	const struct imx477_compatible_data *compatible_data;
};

static inline struct imx477 *to_imx477(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct imx477, sd);
}

/* Read registers up to 2 at a time */
static int imx477_read_reg(struct imx477 *imx477, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx477->sd);
	struct i2c_msg msgs[2];
	u8 addr_buf[2] = { reg >> 8, reg & 0xff };
	u8 data_buf[4] = { 0, };
	int ret;

	if (len > 4)
		return -EINVAL;

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = ARRAY_SIZE(addr_buf);
	msgs[0].buf = addr_buf;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_buf[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = get_unaligned_be32(data_buf);

	return 0;
}

/* Write registers up to 2 at a time */
static int imx477_write_reg(struct imx477 *imx477, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx477->sd);
	u8 buf[6];

	if (len > 4)
		return -EINVAL;

	put_unaligned_be16(reg, buf);
	put_unaligned_be32(val << (8 * (4 - len)), buf + 2);
	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

/* Write a list of registers */
static int imx477_write_regs(struct imx477 *imx477,
			     const struct imx477_reg *regs, u32 len)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx477->sd);
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = imx477_write_reg(imx477, regs[i].address, 1, regs[i].val);
		if (ret) {
			dev_err_ratelimited(&client->dev,
					    "Failed to write reg 0x%4.4x. error = %d\n",
					    regs[i].address, ret);

			return ret;
		}
	}

	return 0;
}

static void imx477_set_default_format(struct imx477 *imx477)
{
	/* Set default mode to max resolution */
	imx477->mode = &supported_modes[0];
	imx477->fmt_code = MEDIA_BUS_FMT_SENSOR_DATA;
}

static int imx477_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx477 *imx477 = to_imx477(sd);
	struct v4l2_mbus_framefmt *try_fmt_img =
		v4l2_subdev_get_try_format(sd, fh->state, 0);

	mutex_lock(&imx477->mutex);

	/* Initialize try_fmt for the image pad */
	try_fmt_img->width = supported_modes[0].width;
	try_fmt_img->height = supported_modes[0].height;
	try_fmt_img->code = MEDIA_BUS_FMT_SENSOR_DATA;
	try_fmt_img->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx477->mutex);

	return 0;
}

static void imx477_adjust_exposure_range(struct imx477 *imx477)
{
	int exposure_max, exposure_def;

	/* Honour the VBLANK limits when setting exposure. */
	exposure_max = imx477->mode->height + imx477->vblank->val -
		       IMX477_EXPOSURE_OFFSET;
	exposure_def = min(exposure_max, imx477->exposure->val);
	__v4l2_ctrl_modify_range(imx477->exposure, imx477->exposure->minimum,
				 exposure_max, imx477->exposure->step,
				 exposure_def);
}

static int imx477_set_frame_length(struct imx477 *imx477, unsigned int val)
{
	int ret = 0;

	imx477->long_exp_shift = 0;

	while (val > IMX477_FRAME_LENGTH_MAX) {
		imx477->long_exp_shift++;
		val >>= 1;
	}

	ret = imx477_write_reg(imx477, IMX477_REG_FRAME_LENGTH,
			       IMX477_REG_VALUE_16BIT, val);
	if (ret)
		return ret;

	return imx477_write_reg(imx477, IMX477_LONG_EXP_SHIFT_REG,
				IMX477_REG_VALUE_08BIT, imx477->long_exp_shift);
}

static int imx477_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx477 *imx477 =
		container_of(ctrl->handler, struct imx477, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&imx477->sd);
	int ret = 0;

	/*
	 * The VBLANK control may change the limits of usable exposure, so check
	 * and adjust if necessary.
	 */
	if (ctrl->id == V4L2_CID_VBLANK)
		imx477_adjust_exposure_range(imx477);

	/*
	 * Applying V4L2 control value only happens
	 * when power is up for streaming
	 */
	if (pm_runtime_get_if_in_use(&client->dev) == 0)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = imx477_write_reg(imx477, IMX477_REG_ANALOG_GAIN,
				       IMX477_REG_VALUE_16BIT, imx477_gain_lut[ctrl->val]);
		break;
	case V4L2_CID_EXPOSURE:
		ret = imx477_write_reg(imx477, IMX477_REG_EXPOSURE,
				       IMX477_REG_VALUE_16BIT, ctrl->val >>
							imx477->long_exp_shift);
		break;
	case V4L2_CID_DIGITAL_GAIN:
		//ret = imx477_write_reg(imx477, IMX477_REG_DIGITAL_GAIN,
				       //IMX477_REG_VALUE_16BIT, ctrl->val);
		ret=0;
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = imx477_write_reg(imx477, IMX477_REG_TEST_PATTERN,
				       IMX477_REG_VALUE_16BIT,
				       imx477_test_pattern_val[ctrl->val]);
		break;
	case V4L2_CID_TEST_PATTERN_RED:
		ret = imx477_write_reg(imx477, IMX477_REG_TEST_PATTERN_R,
				       IMX477_REG_VALUE_16BIT, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN_GREENR:
		ret = imx477_write_reg(imx477, IMX477_REG_TEST_PATTERN_GR,
				       IMX477_REG_VALUE_16BIT, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN_BLUE:
		ret = imx477_write_reg(imx477, IMX477_REG_TEST_PATTERN_B,
				       IMX477_REG_VALUE_16BIT, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN_GREENB:
		ret = imx477_write_reg(imx477, IMX477_REG_TEST_PATTERN_GB,
				       IMX477_REG_VALUE_16BIT, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
		ret = imx477_write_reg(imx477, IMX477_REG_ORIENTATION, 1,
				       imx477->hflip->val |
				       imx477->vflip->val << 1);
		break;
	case V4L2_CID_VBLANK:
		ret = imx477_set_frame_length(imx477,
					      imx477->mode->height + ctrl->val);
		break;
	case V4L2_CID_HBLANK:
		ret = imx477_write_reg(imx477, IMX477_REG_LINE_LENGTH, 2,
				       imx477->mode->width + ctrl->val);
		break;
	default:
		dev_info(&client->dev,
			 "ctrl(id:0x%x,val:0x%x) is not handled\n",
			 ctrl->id, ctrl->val);
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx477_ctrl_ops = {
	.s_ctrl = imx477_set_ctrl,
};

static int imx477_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{

	if (code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SENSOR_DATA;

	return 0;
}

static int imx477_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SENSOR_DATA)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static void imx477_reset_colorspace(struct v4l2_mbus_framefmt *fmt)
{
	fmt->colorspace = V4L2_COLORSPACE_RAW;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(true,
							  fmt->colorspace,
							  fmt->ycbcr_enc);
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
}

static void imx477_update_pad_format(struct imx477 *imx477,
					   const struct imx477_mode *mode,
					   struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	imx477_reset_colorspace(&fmt->format);
}

static int imx477_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx477 *imx477 = to_imx477(sd);

	mutex_lock(&imx477->mutex);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(&imx477->sd, sd_state,
						   fmt->pad);
		/* update the code which could change due to vflip or hflip: */
		try_fmt->code = fmt->pad == MEDIA_BUS_FMT_SENSOR_DATA;
		fmt->format = *try_fmt;
	} else {
			imx477_update_pad_format(imx477, imx477->mode,fmt);
			fmt->format.code = MEDIA_BUS_FMT_SENSOR_DATA;
	}

	mutex_unlock(&imx477->mutex);
	return 0;
}

static
unsigned int imx477_get_frame_length(const struct imx477_mode *mode,
				     const struct v4l2_fract *timeperframe)
{
	u64 frame_length;

	frame_length = (u64)timeperframe->numerator * IMX477_PIXEL_RATE;
	do_div(frame_length,
	       (u64)timeperframe->denominator * mode->line_length_pix);

	if (WARN_ON(frame_length > IMX477_FRAME_LENGTH_MAX))
		frame_length = IMX477_FRAME_LENGTH_MAX;

	return max_t(unsigned int, frame_length, mode->height);
}

static void imx477_set_framing_limits(struct imx477 *imx477)
{
	unsigned int frm_length_min, frm_length_default, hblank_min;
	const struct imx477_mode *mode = imx477->mode;

	frm_length_min = imx477_get_frame_length(mode, &mode->timeperframe_min);
	frm_length_default =
		     imx477_get_frame_length(mode, &mode->timeperframe_default);

	/* Default to no long exposure multiplier. */
	imx477->long_exp_shift = 0;

	/* Update limits and set FPS to default */
	__v4l2_ctrl_modify_range(imx477->vblank, frm_length_min - mode->height,
				 ((1 << IMX477_LONG_EXP_SHIFT_MAX) *
					IMX477_FRAME_LENGTH_MAX) - mode->height,
				 1, frm_length_default - mode->height);

	/* Setting this will adjust the exposure limits as well. */
	__v4l2_ctrl_s_ctrl(imx477->vblank, frm_length_default - mode->height);

	hblank_min = mode->line_length_pix - mode->width;
	__v4l2_ctrl_modify_range(imx477->hblank, hblank_min,
				 IMX477_LINE_LENGTH_MAX, 1, hblank_min);
	__v4l2_ctrl_s_ctrl(imx477->hblank, hblank_min);
}

static int imx477_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt;
	const struct imx477_mode *mode;
	struct imx477 *imx477 = to_imx477(sd);

	mutex_lock(&imx477->mutex);

	/* Bayer order varies with flips */
	fmt->format.code = MEDIA_BUS_FMT_SENSOR_DATA;

	mode = v4l2_find_nearest_size(supported_modes,
						ARRAY_SIZE(supported_modes),
						width, height,
						fmt->format.width,
						fmt->format.height);
	imx477_update_pad_format(imx477, mode, fmt);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_get_try_format(sd, sd_state,
								fmt->pad);
		*framefmt = fmt->format;
	} else if (imx477->mode != mode) {
		imx477->mode = mode;
		imx477->fmt_code = fmt->format.code;
		imx477_set_framing_limits(imx477);
	}

	mutex_unlock(&imx477->mutex);

	return 0;
}

/* Start streaming */
static int imx477_start_streaming(struct imx477 *imx477)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx477->sd);
	const struct imx477_reg_list *reg_list, *freq_regs;
	const struct imx477_reg_list *extra_regs;
	int ret, tm;

	printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (!imx477->common_regs_written) {
		ret = imx477_write_regs(imx477, imx477_common_regs,
					ARRAY_SIZE(imx477_common_regs));
		if (!ret) {
			extra_regs = &imx477->compatible_data->extra_regs;
			ret = imx477_write_regs(imx477,	extra_regs->regs,
						extra_regs->num_of_regs);
		}

		if (!ret) {
			/* Update the link frequency registers */
			freq_regs = &link_freq_regs[imx477->link_freq_idx];
			ret = imx477_write_regs(imx477, freq_regs->regs,
						freq_regs->num_of_regs);
		}

		if (ret) {
			dev_err(&client->dev, "%s failed to set common settings\n",
				__func__);
			return ret;
		}
		imx477->common_regs_written = true;
	}

	/* Apply default values of current mode */
	reg_list = &imx477->mode->reg_list;
	ret = imx477_write_regs(imx477, reg_list->regs, reg_list->num_of_regs);
	if (ret) {
		dev_err(&client->dev, "%s failed to set mode\n", __func__);
		return ret;
	}

	/* Set on-sensor DPC. */
	imx477_write_reg(imx477, 0x0b05, IMX477_REG_VALUE_08BIT, !!dpc_enable);
	imx477_write_reg(imx477, 0x0b06, IMX477_REG_VALUE_08BIT, !!dpc_enable);

	/* Apply customized values from user */
	ret =  __v4l2_ctrl_handler_setup(imx477->sd.ctrl_handler);
	if (ret)
		return ret;

	/* Set vsync trigger mode: 0=standalone, 1=source, 2=sink */
	tm = (imx477->trigger_mode_of >= 0) ? imx477->trigger_mode_of : trigger_mode;
	imx477_write_reg(imx477, IMX477_REG_MC_MODE,
			 IMX477_REG_VALUE_08BIT, (tm > 0) ? 1 : 0);
	imx477_write_reg(imx477, IMX477_REG_MS_SEL,
			 IMX477_REG_VALUE_08BIT, (tm <= 1) ? 1 : 0);
	imx477_write_reg(imx477, IMX477_REG_XVS_IO_CTRL,
			 IMX477_REG_VALUE_08BIT, (tm == 1) ? 1 : 0);
	imx477_write_reg(imx477, IMX477_REG_EXTOUT_EN,
			 IMX477_REG_VALUE_08BIT, (tm == 1) ? 1 : 0);

	/* set stream on register */
	return imx477_write_reg(imx477, IMX477_REG_MODE_SELECT,
				IMX477_REG_VALUE_08BIT, IMX477_MODE_STREAMING);
}

/* Stop streaming */
static void imx477_stop_streaming(struct imx477 *imx477)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx477->sd);
	int ret;

	printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	/* set stream off register */
	ret = imx477_write_reg(imx477, IMX477_REG_MODE_SELECT,
			       IMX477_REG_VALUE_08BIT, IMX477_MODE_STANDBY);
	if (ret)
		dev_err(&client->dev, "%s failed to set stream\n", __func__);

	/* Stop driving XVS out (there is still a weak pull-up) */
	imx477_write_reg(imx477, IMX477_REG_EXTOUT_EN,
			 IMX477_REG_VALUE_08BIT, 0);
}

static int imx477_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx477 *imx477 = to_imx477(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	mutex_lock(&imx477->mutex);
	if (imx477->streaming == enable) {
		mutex_unlock(&imx477->mutex);
		return 0;
	}

	if (enable) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto err_unlock;
		}

		/*
		 * Apply default & customized values
		 * and then start streaming.
		 */
		ret = imx477_start_streaming(imx477);
		if (ret)
			goto err_rpm_put;
	} else {
		imx477_stop_streaming(imx477);
		pm_runtime_put(&client->dev);
	}

	imx477->streaming = enable;

	/* vflip and hflip cannot change during streaming */
	__v4l2_ctrl_grab(imx477->vflip, enable);
	__v4l2_ctrl_grab(imx477->hflip, enable);

	mutex_unlock(&imx477->mutex);

	return ret;

err_rpm_put:
	pm_runtime_put(&client->dev);
err_unlock:
	mutex_unlock(&imx477->mutex);

	return ret;
}

/* Power/clock management functions */
static int imx477_power_on(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx477 *imx477 = to_imx477(sd);
	int ret;
	printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	ret = regulator_bulk_enable(IMX477_NUM_SUPPLIES,
				    imx477->supplies);
	if (ret) {
		dev_err(&client->dev, "%s: failed to enable regulators\n",
			__func__);
		return ret;
	}

	ret = clk_prepare_enable(imx477->xclk);
	if (ret) {
		dev_err(&client->dev, "%s: failed to enable clock\n",
			__func__);
		goto reg_off;
	}

	gpiod_set_value_cansleep(imx477->reset_gpio, 1);
	usleep_range(IMX477_XCLR_MIN_DELAY_US,
		     IMX477_XCLR_MIN_DELAY_US + IMX477_XCLR_DELAY_RANGE_US);

	return 0;

reg_off:
	regulator_bulk_disable(IMX477_NUM_SUPPLIES, imx477->supplies);
	return ret;
}

static int imx477_power_off(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx477 *imx477 = to_imx477(sd);
	printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	gpiod_set_value_cansleep(imx477->reset_gpio, 0);
	regulator_bulk_disable(IMX477_NUM_SUPPLIES, imx477->supplies);
	clk_disable_unprepare(imx477->xclk);

	/* Force reprogramming of the common registers when powered up again. */
	imx477->common_regs_written = false;

	return 0;
}

static int __maybe_unused imx477_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx477 *imx477 = to_imx477(sd);

	if (imx477->streaming)
		imx477_stop_streaming(imx477);

	return 0;
}

static int __maybe_unused imx477_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx477 *imx477 = to_imx477(sd);
	int ret;

	if (imx477->streaming) {
		ret = imx477_start_streaming(imx477);
		if (ret)
			goto error;
	}

	return 0;

error:
	imx477_stop_streaming(imx477);
	imx477->streaming = 0;
	return ret;
}

static int imx477_get_regulators(struct imx477 *imx477)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx477->sd);
	unsigned int i;

	for (i = 0; i < IMX477_NUM_SUPPLIES; i++)
		imx477->supplies[i].supply = imx477_supply_name[i];

	return devm_regulator_bulk_get(&client->dev,
				       IMX477_NUM_SUPPLIES,
				       imx477->supplies);
}

/* Verify chip ID */
static int imx477_identify_module(struct imx477 *imx477, u32 expected_id)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx477->sd);
	int ret;
	u32 val;

	ret = imx477_read_reg(imx477, IMX477_REG_CHIP_ID,
			      IMX477_REG_VALUE_16BIT, &val);
	if (ret) {
		dev_err(&client->dev, "failed to read chip id %x, with error %d\n",
			expected_id, ret);
		return ret;
	}

	if (val != expected_id) {
		dev_err(&client->dev, "chip id mismatch: %x!=%x\n",
			expected_id, val);
		return -EIO;
	}

	dev_info(&client->dev, "Device found is imx%x\n", val);

	return 0;
}

static int imx477_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *fi)
{
	struct imx477 *imx477 = to_imx477(sd);
	const struct imx477_mode *mode = imx477->mode;

	mutex_lock(&imx477->mutex);
	fi->interval = mode->timeperframe_default;
	mutex_unlock(&imx477->mutex);

	return 0;
}

static const struct v4l2_subdev_core_ops imx477_core_ops = {
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops imx477_video_ops = {
	.s_stream = imx477_set_stream,
	.g_frame_interval = imx477_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx477_pad_ops = {
	.enum_mbus_code = imx477_enum_mbus_code,
	.get_fmt = imx477_get_pad_format,
	.set_fmt = imx477_set_pad_format,
	.enum_frame_size = imx477_enum_frame_size,
};

static const struct v4l2_subdev_ops imx477_subdev_ops = {
	.core = &imx477_core_ops,
	.video = &imx477_video_ops,
	.pad = &imx477_pad_ops,
};

static const struct v4l2_subdev_internal_ops imx477_internal_ops = {
	.open = imx477_open,
};

/* Initialize control handlers */
static int imx477_init_controls(struct imx477 *imx477)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	struct i2c_client *client = v4l2_get_subdevdata(&imx477->sd);
	struct v4l2_fwnode_device_properties props;
	unsigned int i;
	int ret;

	ctrl_hdlr = &imx477->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 16);
	if (ret)
		return ret;

	mutex_init(&imx477->mutex);
	ctrl_hdlr->lock = &imx477->mutex;

	/* By default, PIXEL_RATE is read only */
	imx477->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &imx477_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       IMX477_PIXEL_RATE,
					       IMX477_PIXEL_RATE, 1,
					       IMX477_PIXEL_RATE);
	if (imx477->pixel_rate)
		imx477->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* LINK_FREQ is also read only */
	imx477->link_freq =
		v4l2_ctrl_new_int_menu(ctrl_hdlr, &imx477_ctrl_ops,
				       V4L2_CID_LINK_FREQ, 0, 0,
				       &link_freqs[imx477->link_freq_idx]);
	if (imx477->link_freq)
		imx477->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/*
	 * Create the controls here, but mode specific limits are setup
	 * in the imx477_set_framing_limits() call below.
	 */
	imx477->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx477_ctrl_ops,
					   V4L2_CID_VBLANK, 0, 0xffff, 1, 0);
	imx477->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx477_ctrl_ops,
					   V4L2_CID_HBLANK, 0, 0xffff, 1, 0);

	imx477->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &imx477_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX477_EXPOSURE_MIN,
					     IMX477_EXPOSURE_MAX,
					     IMX477_EXPOSURE_STEP,
					     IMX477_EXPOSURE_DEFAULT);

	v4l2_ctrl_new_std(ctrl_hdlr, &imx477_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  IMX477_ANA_GAIN_MIN, ARRAY_SIZE(imx477_gain_lut),
			  IMX477_ANA_GAIN_STEP, IMX477_ANA_GAIN_DEFAULT);

	v4l2_ctrl_new_std(ctrl_hdlr, &imx477_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  IMX477_DGTL_GAIN_MIN, IMX477_DGTL_GAIN_MAX,
			  IMX477_DGTL_GAIN_STEP, IMX477_DGTL_GAIN_DEFAULT);

	imx477->hflip = v4l2_ctrl_new_std(ctrl_hdlr, &imx477_ctrl_ops,
					  V4L2_CID_HFLIP, 0, 1, 1, 0);
	if (imx477->hflip)
		imx477->hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	imx477->vflip = v4l2_ctrl_new_std(ctrl_hdlr, &imx477_ctrl_ops,
					  V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (imx477->vflip)
		imx477->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &imx477_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(imx477_test_pattern_menu) - 1,
				     0, 0, imx477_test_pattern_menu);
	for (i = 0; i < 4; i++) {
		/*
		 * The assumption is that
		 * V4L2_CID_TEST_PATTERN_GREENR == V4L2_CID_TEST_PATTERN_RED + 1
		 * V4L2_CID_TEST_PATTERN_BLUE   == V4L2_CID_TEST_PATTERN_RED + 2
		 * V4L2_CID_TEST_PATTERN_GREENB == V4L2_CID_TEST_PATTERN_RED + 3
		 */
		v4l2_ctrl_new_std(ctrl_hdlr, &imx477_ctrl_ops,
				  V4L2_CID_TEST_PATTERN_RED + i,
				  IMX477_TEST_PATTERN_COLOUR_MIN,
				  IMX477_TEST_PATTERN_COLOUR_MAX,
				  IMX477_TEST_PATTERN_COLOUR_STEP,
				  IMX477_TEST_PATTERN_COLOUR_MAX);
		/* The "Solid color" pattern is white by default */
	}

	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&client->dev, "%s control init failed (%d)\n",
			__func__, ret);
		goto error;
	}

	ret = v4l2_fwnode_device_parse(&client->dev, &props);
	if (ret)
		goto error;

	ret = v4l2_ctrl_new_fwnode_properties(ctrl_hdlr, &imx477_ctrl_ops,
					      &props);
	if (ret)
		goto error;

	imx477->sd.ctrl_handler = ctrl_hdlr;

	mutex_lock(&imx477->mutex);

	/* Setup exposure and frame/line length limits. */
	imx477_set_framing_limits(imx477);

	mutex_unlock(&imx477->mutex);

	return 0;

error:
	v4l2_ctrl_handler_free(ctrl_hdlr);
	mutex_destroy(&imx477->mutex);

	return ret;
}

static void imx477_free_controls(struct imx477 *imx477)
{
	v4l2_ctrl_handler_free(imx477->sd.ctrl_handler);
	mutex_destroy(&imx477->mutex);
}

static int imx477_check_hwcfg(struct device *dev, struct imx477 *imx477)
{
	struct fwnode_handle *endpoint;
	struct v4l2_fwnode_endpoint ep_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	int ret = -EINVAL;
	int i;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		return -EINVAL;
	}

	if (v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep_cfg)) {
		dev_err(dev, "could not parse endpoint\n");
		goto error_out;
	}

	/* Check the number of MIPI CSI2 data lanes */
	if (ep_cfg.bus.mipi_csi2.num_data_lanes != 2) {
		dev_err(dev, "only 2 data lanes are currently supported\n");
		goto error_out;
	}

	/* Check the link frequency set in device tree */
	if (!ep_cfg.nr_of_link_frequencies) {
		dev_err(dev, "link-frequency property not found in DT\n");
		goto error_out;
	}

	for (i = 0; i < ARRAY_SIZE(link_freqs); i++) {
		if (link_freqs[i] == ep_cfg.link_frequencies[0]) {
			imx477->link_freq_idx = i;
			break;
		}
	}

	if (i == ARRAY_SIZE(link_freqs)) {
		dev_err(dev, "Link frequency not supported: %lld\n",
			ep_cfg.link_frequencies[0]);
			ret = -EINVAL;
			goto error_out;
	}

	ret = 0;

error_out:
	v4l2_fwnode_endpoint_free(&ep_cfg);
	fwnode_handle_put(endpoint);

	return ret;
}

static const struct imx477_compatible_data imx477_compatible = {
	.chip_id = IMX477_CHIP_ID,
	.extra_regs = {
		.num_of_regs = 0,
		.regs = NULL
	}
};

static const struct imx477_reg imx378_regs[] = {
	{0x3e35, 0x01},
	{0x4421, 0x08},
	{0x3ff9, 0x00},
};

static const struct imx477_compatible_data imx378_compatible = {
	.chip_id = IMX378_CHIP_ID,
	.extra_regs = {
		.num_of_regs = ARRAY_SIZE(imx378_regs),
		.regs = imx378_regs
	}
};

static const struct of_device_id imx477_dt_ids[] = {
	{ .compatible = "sony,imx477", .data = &imx477_compatible },
	{ .compatible = "sony,imx378", .data = &imx378_compatible },
	{ /* sentinel */ }
};

static int imx477_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct imx477 *imx477;
	const struct of_device_id *match;
	int ret;
	u32 tm_of;

	imx477 = devm_kzalloc(&client->dev, sizeof(*imx477), GFP_KERNEL);
	if (!imx477)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&imx477->sd, client, &imx477_subdev_ops);

	match = of_match_device(imx477_dt_ids, dev);
	if (!match)
		return -ENODEV;
	imx477->compatible_data =
		(const struct imx477_compatible_data *)match->data;

	/* Check the hardware configuration in device tree */
	if (imx477_check_hwcfg(dev, imx477))
		return -EINVAL;

	/* Default the trigger mode from OF to -1, which means invalid */
	ret = of_property_read_u32(dev->of_node, "trigger-mode", &tm_of);
	imx477->trigger_mode_of = (ret == 0) ? tm_of : -1;

	/* Get system clock (xclk) */
	imx477->xclk = devm_clk_get(dev, NULL);
	if (IS_ERR(imx477->xclk)) {
		dev_err(dev, "failed to get xclk\n");
		return PTR_ERR(imx477->xclk);
	}

	imx477->xclk_freq = clk_get_rate(imx477->xclk);
	if (imx477->xclk_freq != IMX477_XCLK_FREQ) {
		dev_err(dev, "xclk frequency not supported: %d Hz\n",
			imx477->xclk_freq);
		return -EINVAL;
	}

	ret = imx477_get_regulators(imx477);
	if (ret) {
		dev_err(dev, "failed to get regulators\n");
		return ret;
	}

	/* Request optional enable pin */
	imx477->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						     GPIOD_OUT_HIGH);

	/*
	 * The sensor must be powered for imx477_identify_module()
	 * to be able to read the CHIP_ID register
	 */
	ret = imx477_power_on(dev);
	if (ret)
		return ret;

	ret = imx477_identify_module(imx477, imx477->compatible_data->chip_id);
	if (ret)
		goto error_power_off;

	/* Initialize default format */
	imx477_set_default_format(imx477);

	/* sensor doesn't enter LP-11 state upon power up until and unless
	 * streaming is started, so upon power up switch the modes to:
	 * streaming -> standby
	 */
	ret = imx477_write_reg(imx477, IMX477_REG_MODE_SELECT,
		IMX477_REG_VALUE_08BIT, IMX477_MODE_STREAMING);
	if (ret < 0)
	goto error_power_off;
	usleep_range(100, 110);

	/* put sensor back to standby mode */
	ret = imx477_write_reg(imx477, IMX477_REG_MODE_SELECT,
		IMX477_REG_VALUE_08BIT, IMX477_MODE_STANDBY);
	if (ret < 0)
	goto error_power_off;

	/* Stop driving XVS out (there is still a weak pull-up) */
	imx477_write_reg(imx477, IMX477_REG_EXTOUT_EN,
		IMX477_REG_VALUE_08BIT, 0);
		
	usleep_range(100, 110);

	printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	/* Enable runtime PM and turn off the device */
	pm_runtime_set_active(dev);
	pm_runtime_get_noresume(dev);
	pm_runtime_enable(dev);
	printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	/* This needs the pm runtime to be registered. */
	ret = imx477_init_controls(imx477);
	if (ret)
		goto error_power_off;

	/* Initialize subdev */
	imx477->sd.internal_ops = &imx477_internal_ops;
	imx477->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
			    V4L2_SUBDEV_FL_HAS_EVENTS;
	imx477->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pads */
	imx477->pad.flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&imx477->sd.entity, 1, &imx477->pad);
	if (ret) {
		dev_err(dev, "failed to init entity pads: %d\n", ret);
		goto error_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&imx477->sd);
	if (ret < 0) {
		dev_err(dev, "failed to register sensor sub-device: %d\n", ret);
		goto error_media_entity;
	}

	printk("######################## %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	return 0;

error_media_entity:
	media_entity_cleanup(&imx477->sd.entity);

error_handler_free:
	imx477_free_controls(imx477);

error_power_off:
	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);
	imx477_power_off(&client->dev);

	return ret;
}

static void imx477_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx477 *imx477 = to_imx477(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	imx477_free_controls(imx477);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		imx477_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);
}

MODULE_DEVICE_TABLE(of, imx477_dt_ids);

static const struct dev_pm_ops imx477_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(imx477_suspend, imx477_resume)
	SET_RUNTIME_PM_OPS(imx477_power_off, imx477_power_on, NULL)
};

static struct i2c_driver imx477_i2c_driver = {
	.driver = {
		.name = "imx477",
		.of_match_table	= imx477_dt_ids,
		.pm = &imx477_pm_ops,
	},
	.probe_new = imx477_probe,
	.remove = imx477_remove,
};

module_i2c_driver(imx477_i2c_driver);

MODULE_AUTHOR("Naushir Patuck <naush@raspberrypi.com>");
MODULE_DESCRIPTION("Sony IMX477 sensor driver");
MODULE_LICENSE("GPL v2");
