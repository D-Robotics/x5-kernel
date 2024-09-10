// SPDX-License-Identifier: GPL-2.0
/*
 * sc230ai driver
 *
 * Copyright (C) 2017 Fuzhou Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 add poweron function.
 * V0.0X01.0X02 fix mclk issue when probe multiple camera.
 * V0.0X01.0X03 fix gain range.
 * V0.0X01.0X04 add enum_frame_interval function.
 * V0.0X01.0X05 add quick stream on/off.
 * V0.0X01.0X06 fix set vflip/hflip failed bug.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <linux/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>


#define DRIVER_VERSION			KERNEL_VERSION(0, 0x00, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define SC230AI_LANES	1

#define SC230AI_BITS_PER_SAMPLE		10
#define SC230AI_LINK_FREQ_371		371250000// 371.25Mbps
/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define PIXEL_RATE_WITH_371M_10BIT		(SC230AI_LINK_FREQ_371 * 2 * \
					SC230AI_LANES / SC230AI_BITS_PER_SAMPLE)
#define SC230AI_XVCLK_FREQ		24000000

#define CHIP_ID				0xcb34
#define SC230AI_REG_CHIP_ID		0x3107

#define SC230AI_REG_CTRL_MODE		0x0100
#define SC230AI_MODE_SW_STANDBY		0x0
#define SC230AI_MODE_STREAMING		BIT(0)

#define SC230AI_REG_EXPOSURE_H		0x3e00
#define SC230AI_REG_EXPOSURE_M		0x3e01
#define SC230AI_REG_EXPOSURE_L		0x3e02
#define SC230AI_REG_SEXPOSURE_H		0x3e22
#define SC230AI_REG_SEXPOSURE_M		0x3e04
#define SC230AI_REG_SEXPOSURE_L		0x3e05
#define SC230AI_EXPOSURE_MIN		2
#define	SC230AI_EXPOSURE_STEP		1
#define SC230AI_VTS_MIN			0x465
#define SC230AI_VTS_MAX			0x7fff

#define SC230AI_REG_DIG_GAIN		0x3e06
#define SC230AI_REG_DIG_FINE_GAIN	0x3e07
//#define SC230AI_REG_ANA_GAIN		0x3e08
#define SC230AI_REG_ANA_GAIN	0x3e09
#define SC230AI_REG_SDIG_GAIN		0x3e10
#define SC230AI_REG_SDIG_FINE_GAIN	0x3e11
//#define SC230AI_REG_SANA_GAIN		0x3e12
#define SC230AI_REG_SANA_GAIN	0x3e13
#define SC230AI_GAIN_BASE		3100
#define SC230AI_GAIN_MIN		0x00
#define SC230AI_GAIN_MAX		(960)  //960
#define SC230AI_GAIN_STEP		1
#define SC230AI_GAIN_DEFAULT		0x01  //0x0400
#define SC230AI_LGAIN			0
#define SC230AI_SGAIN			1

#define SC230AI_REG_GROUP_HOLD		0x3812
#define SC230AI_GROUP_HOLD_START	0x00
#define SC230AI_GROUP_HOLD_END		0x30

//#define SC230AI_REG_HIGH_TEMP_H		0x3974
//#define SC230AI_REG_HIGH_TEMP_L		0x3975

#define SC230AI_REG_TEST_PATTERN	0x4501
#define SC230AI_TEST_PATTERN_BIT_MASK	BIT(3)

#define SC230AI_REG_VTS_H		0x320e
#define SC230AI_REG_VTS_L		0x320f

#define SC230AI_FLIP_MIRROR_REG		0x3221

#define SC230AI_FETCH_EXP_H(VAL)		(((VAL) >> 12) & 0xF)
#define SC230AI_FETCH_EXP_M(VAL)		(((VAL) >> 4) & 0xFF)
#define SC230AI_FETCH_EXP_L(VAL)		(((VAL) & 0xF) << 4)

#define SC230AI_FETCH_AGAIN_H(VAL)		(((VAL) >> 8) & 0x03)
#define SC230AI_FETCH_AGAIN_L(VAL)		((VAL) & 0xFF)

#define SC230AI_FETCH_MIRROR(VAL, ENABLE)	(ENABLE ? VAL | 0x06 : VAL & 0xf9)
#define SC230AI_FETCH_FLIP(VAL, ENABLE)		(ENABLE ? VAL | 0x60 : VAL & 0x9f)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define SC230AI_REG_VALUE_08BIT		1
#define SC230AI_REG_VALUE_16BIT		2
#define SC230AI_REG_VALUE_24BIT		3

#define OF_CAMERA_SYNC_MODE		"horizon,camera-module-sync-mode"

#define OF_INTERNAL_MASTER_MODE		"internal_master"
#define OF_EXTERNAL_MASTER_MODE		"extern_master"
#define OF_SLAVE_MODE			"slave_mode"

#define SC230AI_NAME			"sc230ai"

static const char * const sc230ai_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define SC230AI_NUM_SUPPLIES ARRAY_SIZE(sc230ai_supply_names)

enum sc230ai_SYNC_MODE {
	NO_SYNC_MODE = 0,
	EXTERNAL_MASTER_MODE,
	INTERNAL_MASTER_MODE,
	SLAVE_MODE,
};

enum PAD_ID {
	PAD0 = 0,
	PAD1,
	PAD2,
	PAD3,
	PAD_MAX,
};


#define 	V4L2_MBUS_CSI2_CHANNEL_0   (1 << 4)

#define 	V4L2_MBUS_CSI2_CHANNEL_1   (1 << 5)

#define 	V4L2_MBUS_CSI2_CHANNEL_2   (1 << 6)

#define 	V4L2_MBUS_CSI2_CHANNEL_3   (1 << 7)
#define V4L2_MBUS_CSI2_CONTINUOUS_CLOCK   (1 << 8)

struct regval {
	u16 addr;
	u8 val;
};

struct sc230ai_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	const struct regval *reg_list;
};

struct sc230ai {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*pwen_gpio;	/* SOM board need for carrier borad */
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[SC230AI_NUM_SUPPLIES];

	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*exposure;
	struct v4l2_ctrl	*anal_gain;
	struct v4l2_ctrl	*digi_gain;
	struct v4l2_ctrl	*hblank;
	struct v4l2_ctrl	*vblank;
	struct v4l2_ctrl	*test_pattern;
	struct mutex		mutex;
	bool			streaming;
	bool			power_on;
	const struct sc230ai_mode *cur_mode;
	u32			cur_vts;
	bool			has_init_exp;
};

#define to_sc230ai(sd) container_of(sd, struct sc230ai, subdev)

static const struct regval sc230ai_global_regs[] = {
	{REG_NULL, 0x00},
};


/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 1008Mbps, lane
 */
static const struct regval sc230ai_1lane_30fps_1920x1080_regs[] = {
    	{0x0103,0x01},
        {0x0100,0x00},
        {0x36e9,0x80},
        {0x37f9,0x80},
        {0x300a,0x24},
        {0x3018,0x12},
        {0x3019,0x0e},
        {0x301f,0x62},
        {0x3032,0xa0},
        {0x320c,0x09},
        {0x320d,0x60},
        {0x3225,0x30},
        {0x3301,0x07},
        {0x3304,0x50},
        {0x3306,0x70},
        {0x3308,0x18},
        {0x3309,0x68},
        {0x330a,0x01},
        {0x330b,0x20},
        {0x3314,0x15},
        {0x331e,0x41},
        {0x331f,0x59},
        {0x3333,0x10},
        {0x3334,0x40},
        {0x335d,0x60},
        {0x335e,0x06},
        {0x335f,0x08},
        {0x3364,0x5e},
        {0x337c,0x02},
        {0x337d,0x0a},
        {0x3390,0x01},
        {0x3391,0x0b},
        {0x3392,0x0f},
        {0x3393,0x09},
        {0x3394,0x0d},
        {0x3395,0x60},
        {0x3396,0x48},
        {0x3397,0x49},
        {0x3398,0x4b},
        {0x3399,0x07},
        {0x339a,0x0a},
        {0x339b,0x0d},
        {0x339c,0x60},
        {0x33a2,0x04},
        {0x33ad,0x2c},
        {0x33af,0x40},
        {0x33b1,0x80},
        {0x33b3,0x40},
        {0x33b9,0x0a},
        {0x33f9,0x78},
        {0x33fb,0xa0},
        {0x33fc,0x4f},
        {0x33fd,0x5f},
        {0x349f,0x03},
        {0x34a6,0x4b},
        {0x34a7,0x5f},
        {0x34a8,0x30},
        {0x34a9,0x20},
        {0x34aa,0x01},
        {0x34ab,0x28},
        {0x34ac,0x01},
        {0x34ad,0x58},
        {0x34f8,0x7f},
        {0x34f9,0x10},
        {0x3614,0x01},
        {0x3630,0xc0},
        {0x3632,0x54},
        {0x3633,0x44},
        {0x363b,0x20},
        {0x363c,0x08},
        {0x3651,0x7f},
        {0x3670,0x09},
        {0x3674,0xb0},
        {0x3675,0x80},
        {0x3676,0x88},
        {0x367c,0x40},
        {0x367d,0x49},
        {0x3690,0x33},
        {0x3691,0x33},
        {0x3692,0x43},
        {0x369c,0x49},
        {0x369d,0x4f},
        {0x36ae,0x4b},
        {0x36af,0x4f},
        {0x36b0,0x87},
        {0x36b1,0x9b},
        {0x36b2,0xb7},
        {0x36d0,0x01},
        {0x36ea,0x09},
        {0x36eb,0x0c},
        {0x36ec,0x0c},
        {0x36ed,0x24},
        {0x3722,0x97},
        {0x3724,0x22},
        {0x3728,0x90},
        {0x37fa,0x09},
        {0x37fb,0x32},
        {0x37fc,0x10},
        {0x37fd,0x34},
        {0x3901,0x02},
        {0x3902,0xc5},
        {0x3904,0x04},
        {0x3907,0x00},
        {0x3908,0x41},
        {0x3909,0x00},
        {0x390a,0x00},
        {0x3933,0x84},
        {0x3934,0x0a},
        {0x3940,0x64},
        {0x3941,0x00},
        {0x3942,0x04},
        {0x3943,0x0b},
        {0x3e00,0x00},
        {0x3e01,0x2a},
        {0x3e02,0x20},
	{0x3e06,0x00},
	{0x3e07,0xa6},
	{0x3e09,0x40},
        {0x440e,0x02},
        {0x450d,0x11},
        {0x4819,0x0a},
        {0x481b,0x06},
        {0x481d,0x16},
        {0x481f,0x05},
        {0x4821,0x0b},
        {0x4823,0x05},
        {0x4825,0x05},
        {0x4827,0x05},
        {0x4829,0x09},
        {0x5010,0x01},
        {0x5787,0x08},
        {0x5788,0x03},
        {0x5789,0x00},
        {0x578a,0x10},
        {0x578b,0x08},
        {0x578c,0x00},
        {0x5790,0x08},
        {0x5791,0x04},
        {0x5792,0x00},
        {0x5793,0x10},
        {0x5794,0x08},
        {0x5795,0x00},
        {0x5799,0x06},
        {0x57ad,0x00},
        {0x5ae0,0xfe},
        {0x5ae1,0x40},
        {0x5ae2,0x3f},
        {0x5ae3,0x38},
        {0x5ae4,0x28},
        {0x5ae5,0x3f},
        {0x5ae6,0x38},
        {0x5ae7,0x28},
        {0x5ae8,0x3f},
        {0x5ae9,0x3c},
        {0x5aea,0x2c},
        {0x5aeb,0x3f},
        {0x5aec,0x3c},
        {0x5aed,0x2c},
        {0x5af4,0x3f},
        {0x5af5,0x38},
        {0x5af6,0x28},
        {0x5af7,0x3f},
        {0x5af8,0x38},
        {0x5af9,0x28},
        {0x5afa,0x3f},
        {0x5afb,0x3c},
        {0x5afc,0x2c},
        {0x5afd,0x3f},
        {0x5afe,0x3c},
        {0x5aff,0x2c},
        {0x36e9,0x53},
        {0x37f9,0x53},
	{REG_NULL, 0x00},
};

static const struct sc230ai_mode supported_modes[] = {
	{
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 30,
			.denominator = 30,
		},
		.exp_def = 0x32,	//default value
		.hts_def = 0x898,
		.vts_def = 0x465,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = sc230ai_1lane_30fps_1920x1080_regs,
    }
};

static const s64 link_freq_menu_items[] = {
	SC230AI_LINK_FREQ_371
};

static const char * const sc230ai_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Write registers up to 4 at a time */
static int sc230ai_write_reg(struct i2c_client *client, u16 reg,
			    u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 2;
	val_i = 4 - len;

	while (val_i < 4) {
		buf[buf_i++] = val_p[val_i++];
	}

	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

static int sc230ai_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = sc230ai_write_reg(client, regs[i].addr,
					SC230AI_REG_VALUE_08BIT, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int sc230ai_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
			    u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if (len > 4 || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}

static int sc230ai_set_gain_reg(struct sc230ai *sc230ai, u32 gain, int mode)
{
	u8 ana_coarse_gain_reg = 0x00, d_fine_gain_reg = 0x80, d_gain_reg = 0x00;
	int ret = 0;
	
	if (gain < SC230AI_GAIN_MIN)
		gain = SC230AI_GAIN_MIN;
	if (gain > SC230AI_GAIN_MAX)
		gain = SC230AI_GAIN_MAX;

	if (gain < 128) {
		ana_coarse_gain_reg = 0x00;
		d_fine_gain_reg = 128 + gain;
	} else if (gain < 199) {
		ana_coarse_gain_reg = 0x01;
		d_fine_gain_reg = gain;
	} else if (gain < 199 + 127) {
		ana_coarse_gain_reg = 0x40;
		d_fine_gain_reg = 128 + (gain - 199);
	} else if (gain < 199 + 127 * 2) {
		ana_coarse_gain_reg = 0x48;
		d_fine_gain_reg = 128 + (gain - 199 - 127);
	} else if (gain < 199 + 127 * 3) {
		ana_coarse_gain_reg = 0x49;
		d_fine_gain_reg = 128 + (gain - 199 - 127 * 2);
	} else if (gain < 199 + 127 * 4) {
		ana_coarse_gain_reg = 0x4B;
		d_fine_gain_reg = 128 + (gain - 199 - 127 * 3);
	} else if (gain < 199 + 127 * 5) {
		ana_coarse_gain_reg = 0x4F;
		d_fine_gain_reg = 128 + (gain - 199 - 127 * 4);
	} else {	//199 + 127 * 6 = 961
		ana_coarse_gain_reg = 0x5F;
		d_fine_gain_reg = 128 + (gain - 199 - 127 * 5);
	}

	dev_dbg(&sc230ai->client->dev, "%s d_gain_reg = 0x%x, d_fine_gain_reg = 0x%x, ana_coarse_gain_reg = 0x%x \n", 
	__func__, d_gain_reg, d_fine_gain_reg, ana_coarse_gain_reg);

	ret = sc230ai_write_reg(sc230ai->client,
				SC230AI_REG_DIG_GAIN,
				SC230AI_REG_VALUE_08BIT,
				d_gain_reg & 0xF);
	ret |= sc230ai_write_reg(sc230ai->client,
				 SC230AI_REG_DIG_FINE_GAIN,
				 SC230AI_REG_VALUE_08BIT,
				 d_fine_gain_reg);
	ret |= sc230ai_write_reg(sc230ai->client,
				 SC230AI_REG_ANA_GAIN,
				 SC230AI_REG_VALUE_08BIT,
				 ana_coarse_gain_reg);
	return ret;
}

static int sc230ai_get_reso_dist(const struct sc230ai_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct sc230ai_mode *
sc230ai_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = sc230ai_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int sc230ai_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc230ai *sc230ai = to_sc230ai(sd);
	const struct sc230ai_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&sc230ai->mutex);

	mode = sc230ai_find_best_fit(fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
	} else {
		sc230ai->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc230ai->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc230ai->vblank, SC230AI_VTS_MIN - mode->height,
					 SC230AI_VTS_MAX - mode->height,
					 1, vblank_def);
	}

	mutex_unlock(&sc230ai->mutex);

	return 0;
}

static int sc230ai_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc230ai *sc230ai = to_sc230ai(sd);
	const struct sc230ai_mode *mode = sc230ai->cur_mode;

	mutex_lock(&sc230ai->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&sc230ai->mutex);

	return 0;
}

static int sc230ai_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct sc230ai *sc230ai = to_sc230ai(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = sc230ai->cur_mode->bus_fmt;

	return 0;
}

static int sc230ai_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_frame_size_enum *fse)
{

	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != supported_modes[0].bus_fmt)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int sc230ai_enable_test_pattern(struct sc230ai *sc230ai, u32 pattern)
{
	u32 val = 0;
	int ret = 0;

	ret = sc230ai_read_reg(sc230ai->client, SC230AI_REG_TEST_PATTERN,
			       SC230AI_REG_VALUE_08BIT, &val);
	if (pattern)
		val |= SC230AI_TEST_PATTERN_BIT_MASK;
	else
		val &= ~SC230AI_TEST_PATTERN_BIT_MASK;

	ret |= sc230ai_write_reg(sc230ai->client, SC230AI_REG_TEST_PATTERN,
				 SC230AI_REG_VALUE_08BIT, val);
	return ret;
}

static int sc230ai_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct sc230ai *sc230ai = to_sc230ai(sd);
	const struct sc230ai_mode *mode = sc230ai->cur_mode;

	mutex_lock(&sc230ai->mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&sc230ai->mutex);

	return 0;
}

static int sc230ai_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct sc230ai *sc230ai = to_sc230ai(sd);
	struct device *dev = sd->dev;
	int ret = -1;
	s64 vblank_def;
	u32 fps_set, current_fps;

	fps_set = DIV_ROUND_CLOSEST(fi->interval.denominator, fi->interval.numerator);
	dev_info(dev, "%s set fps = %u\n", __func__, fps_set);

	mutex_lock(&sc230ai->mutex);

	current_fps = DIV_ROUND_CLOSEST(sc230ai->cur_mode->max_fps.denominator, 
            sc230ai->cur_mode->max_fps.numerator);
	vblank_def = sc230ai->cur_mode->vts_def * current_fps / fps_set - sc230ai->cur_mode->height;

	ret = __v4l2_ctrl_s_ctrl(sc230ai->vblank, vblank_def);
	mutex_unlock(&sc230ai->mutex);
	if (ret < 0) {
		dev_err(dev, "%s __v4l2_ctrl_s_ctrl error - %d\n", __func__, ret);
	}

	return 0;
}

static int __sc230ai_start_stream(struct sc230ai *sc230ai)
{
/* FIXME customer may need ctrl_handler  or set sensor time */
#if 0
	int ret = 0;

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&sc230ai->ctrl_handler);
	if (ret)
		return ret;
#endif

	// FIXME
	(void)sc230ai_write_array(sc230ai->client, sc230ai->cur_mode->reg_list);

	dev_info(&sc230ai->client->dev, "start stream\n");
	return sc230ai_write_reg(sc230ai->client, SC230AI_REG_CTRL_MODE,
				 SC230AI_REG_VALUE_08BIT, SC230AI_MODE_STREAMING);
}

static int __sc230ai_stop_stream(struct sc230ai *sc230ai)
{
	sc230ai->has_init_exp = false;
	dev_info(&sc230ai->client->dev, "stop stream\n");
	return sc230ai_write_reg(sc230ai->client, SC230AI_REG_CTRL_MODE,
				 SC230AI_REG_VALUE_08BIT, SC230AI_MODE_SW_STANDBY);
}

static int sc230ai_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sc230ai *sc230ai = to_sc230ai(sd);
	struct i2c_client *client = sc230ai->client;
	int ret = 0;

	mutex_lock(&sc230ai->mutex);
	on = !!on;
	if (on == sc230ai->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __sc230ai_start_stream(sc230ai);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sc230ai_stop_stream(sc230ai);
		pm_runtime_put(&client->dev);
	}

	sc230ai->streaming = on;

unlock_and_return:
	mutex_unlock(&sc230ai->mutex);

	return ret;
}

static int sc230ai_s_power(struct v4l2_subdev *sd, int on)
{
	struct sc230ai *sc230ai = to_sc230ai(sd);
	struct i2c_client *client = sc230ai->client;
	int ret = 0;

	mutex_lock(&sc230ai->mutex);

	/* If the power state is not modified - no work to do. */
	if (sc230ai->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = sc230ai_write_array(sc230ai->client, sc230ai_global_regs);
		if (ret) {
			v4l2_err(sd, "could not set init registers\n");
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		sc230ai->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sc230ai->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sc230ai->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 sc230ai_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, SC230AI_XVCLK_FREQ / 1000 / 1000);
}

static int __sc230ai_power_on(struct sc230ai *sc230ai)
{
	int ret;
	u32 delay_us;
	struct device *dev = &sc230ai->client->dev;

	if (!IS_ERR(sc230ai->pwen_gpio))
		gpiod_set_value_cansleep(sc230ai->pwen_gpio, 1);
	usleep_range(500,1000);

	if (!IS_ERR(sc230ai->reset_gpio))
		gpiod_set_value_cansleep(sc230ai->reset_gpio, 0);

	ret = regulator_bulk_enable(SC230AI_NUM_SUPPLIES, sc230ai->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		return ret;
	}

	if (!IS_ERR(sc230ai->reset_gpio))
		gpiod_set_value_cansleep(sc230ai->reset_gpio, 1);

	usleep_range(500, 1000);
	if (!IS_ERR(sc230ai->pwdn_gpio))
		gpiod_set_value_cansleep(sc230ai->pwdn_gpio, 1);

	usleep_range(4500, 5000);

	if (!IS_ERR(sc230ai->xvclk)) {
		ret = clk_set_rate(sc230ai->xvclk, SC230AI_XVCLK_FREQ);
		if (ret < 0)
			dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
		if (clk_get_rate(sc230ai->xvclk) != SC230AI_XVCLK_FREQ)
			dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
		ret = clk_prepare_enable(sc230ai->xvclk);
		if (ret < 0) {
			dev_err(dev, "Failed to enable xvclk\n");
			return ret;
		}
	}

	if (!IS_ERR(sc230ai->reset_gpio))
		usleep_range(6000, 8000);
	else
		usleep_range(12000, 16000);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = sc230ai_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;
}

static void __sc230ai_power_off(struct sc230ai *sc230ai)
{
	if (!IS_ERR(sc230ai->pwdn_gpio))
		gpiod_set_value_cansleep(sc230ai->pwdn_gpio, 0);

	if (!IS_ERR(sc230ai->xvclk))
		clk_disable_unprepare(sc230ai->xvclk);

	if (!IS_ERR(sc230ai->reset_gpio))
		gpiod_set_value_cansleep(sc230ai->reset_gpio, 0);

	if (!IS_ERR(sc230ai->pwen_gpio))
		gpiod_set_value_cansleep(sc230ai->pwen_gpio, 0);

	regulator_bulk_disable(SC230AI_NUM_SUPPLIES, sc230ai->supplies);
}

static int sc230ai_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc230ai *sc230ai = to_sc230ai(sd);

	return __sc230ai_power_on(sc230ai);
}

static int sc230ai_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc230ai *sc230ai = to_sc230ai(sd);

	__sc230ai_power_off(sc230ai);

	return 0;
}

static int sc230ai_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sc230ai *sc230ai = to_sc230ai(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct sc230ai_mode *def_mode = &supported_modes[0];

	mutex_lock(&sc230ai->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sc230ai->mutex);
	/* No crop or compose */

	return 0;
}

static int sc230ai_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	return 0;
}

static const struct dev_pm_ops sc230ai_pm_ops = {
	SET_RUNTIME_PM_OPS(sc230ai_runtime_suspend,
			   sc230ai_runtime_resume, NULL)
};

static const struct v4l2_subdev_internal_ops sc230ai_internal_ops = {
	.open = sc230ai_open,
};

static const struct v4l2_subdev_core_ops sc230ai_core_ops = {
	.s_power = sc230ai_s_power,
#if 0
	.ioctl = sc230ai_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sc230ai_compat_ioctl32,
#endif
#endif
};

static const struct v4l2_subdev_video_ops sc230ai_video_ops = {
	.s_stream = sc230ai_s_stream,
	.g_frame_interval = sc230ai_g_frame_interval,
	.s_frame_interval = sc230ai_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops sc230ai_pad_ops = {
	.enum_mbus_code = sc230ai_enum_mbus_code,
	.enum_frame_size = sc230ai_enum_frame_sizes,
	.enum_frame_interval = sc230ai_enum_frame_interval,
	.get_fmt = sc230ai_get_fmt,
	.set_fmt = sc230ai_set_fmt,
};

static const struct v4l2_subdev_ops sc230ai_subdev_ops = {
	.core	= &sc230ai_core_ops,
	.video	= &sc230ai_video_ops,
	.pad	= &sc230ai_pad_ops,
};

static int sc230ai_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sc230ai *sc230ai = container_of(ctrl->handler,
					       struct sc230ai, ctrl_handler);
	struct i2c_client *client = sc230ai->client;
	s64 max;
	int ret = 0;
	u32 val = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = sc230ai->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(sc230ai->exposure,
					 sc230ai->exposure->minimum, max,
					 sc230ai->exposure->step,
					 sc230ai->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&sc230ai->client->dev, "%s set exposure %d \n", __func__, ctrl->val);
		ctrl->val = ctrl->val * 2;
		/* 4 least significant bits of expsoure are fractional part */
		ret = sc230ai_write_reg(sc230ai->client,
					SC230AI_REG_EXPOSURE_H,
					SC230AI_REG_VALUE_08BIT,
					SC230AI_FETCH_EXP_H(ctrl->val));
		ret |= sc230ai_write_reg(sc230ai->client,
					 SC230AI_REG_EXPOSURE_M,
					 SC230AI_REG_VALUE_08BIT,
					 SC230AI_FETCH_EXP_M(ctrl->val));
		ret |= sc230ai_write_reg(sc230ai->client,
					 SC230AI_REG_EXPOSURE_L,
					 SC230AI_REG_VALUE_08BIT,
					 SC230AI_FETCH_EXP_L(ctrl->val));
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&sc230ai->client->dev, "%s set gain %d \n", __func__, ctrl->val);
		ret = sc230ai_set_gain_reg(sc230ai, ctrl->val, SC230AI_LGAIN);
		break;
	case V4L2_CID_VBLANK:
		ret = sc230ai_write_reg(sc230ai->client,
					SC230AI_REG_VTS_H,
					SC230AI_REG_VALUE_08BIT,
					(ctrl->val + sc230ai->cur_mode->height)
					>> 8);
		ret |= sc230ai_write_reg(sc230ai->client,
					 SC230AI_REG_VTS_L,
					 SC230AI_REG_VALUE_08BIT,
					 (ctrl->val + sc230ai->cur_mode->height)
					 & 0xff);
		sc230ai->cur_vts = ctrl->val + sc230ai->cur_mode->height;
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = sc230ai_enable_test_pattern(sc230ai, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = sc230ai_read_reg(sc230ai->client, SC230AI_FLIP_MIRROR_REG,
				       SC230AI_REG_VALUE_08BIT, &val);
		ret |= sc230ai_write_reg(sc230ai->client, SC230AI_FLIP_MIRROR_REG,
					 SC230AI_REG_VALUE_08BIT,
					 SC230AI_FETCH_MIRROR(val, ctrl->val));
		if (ctrl->val)
			ret |= sc230ai_write_reg(sc230ai->client, 0x3211,
						 SC230AI_REG_VALUE_08BIT,
						 1);
		else
			ret |= sc230ai_write_reg(sc230ai->client, 0x3211,
						 SC230AI_REG_VALUE_08BIT,
						 0);
		break;
	case V4L2_CID_VFLIP:
		ret = sc230ai_read_reg(sc230ai->client, SC230AI_FLIP_MIRROR_REG,
				       SC230AI_REG_VALUE_08BIT, &val);
		ret |= sc230ai_write_reg(sc230ai->client, SC230AI_FLIP_MIRROR_REG,
					 SC230AI_REG_VALUE_08BIT,
					 SC230AI_FETCH_FLIP(val, ctrl->val));
		if (ctrl->val)
			ret |= sc230ai_write_reg(sc230ai->client, 0x3213,
						 SC230AI_REG_VALUE_08BIT,
						 7);
		else
			ret |= sc230ai_write_reg(sc230ai->client, 0x3213,
						 SC230AI_REG_VALUE_08BIT,
						 8);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops sc230ai_ctrl_ops = {
	.s_ctrl = sc230ai_set_ctrl,
};

static int sc230ai_initialize_controls(struct sc230ai *sc230ai)
{
	const struct sc230ai_mode *mode;
	struct v4l2_ctrl_handler *handler;
	struct v4l2_ctrl *ctrl;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &sc230ai->ctrl_handler;
	mode = sc230ai->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &sc230ai->mutex;

	ctrl = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
				      0, 0, link_freq_menu_items);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			  0, PIXEL_RATE_WITH_371M_10BIT, 1, PIXEL_RATE_WITH_371M_10BIT);

	h_blank = mode->hts_def - mode->width;
	sc230ai->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					    h_blank, h_blank, 1, h_blank);
	if (sc230ai->hblank)
		sc230ai->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	sc230ai->vblank = v4l2_ctrl_new_std(handler, &sc230ai_ctrl_ops,
					    V4L2_CID_VBLANK, SC230AI_VTS_MIN - mode->height,
					    SC230AI_VTS_MAX - mode->height,
					    1, vblank_def);

	exposure_max = mode->vts_def - 8;
	
	sc230ai->exposure = v4l2_ctrl_new_std(handler, &sc230ai_ctrl_ops,
					      V4L2_CID_EXPOSURE, SC230AI_EXPOSURE_MIN,
					      exposure_max, SC230AI_EXPOSURE_STEP,
					      mode->exp_def);

	sc230ai->anal_gain = v4l2_ctrl_new_std(handler, &sc230ai_ctrl_ops,
					       V4L2_CID_ANALOGUE_GAIN, SC230AI_GAIN_MIN,
					       SC230AI_GAIN_MAX, SC230AI_GAIN_STEP,
					       SC230AI_GAIN_DEFAULT);
	sc230ai->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
							    &sc230ai_ctrl_ops,
					V4L2_CID_TEST_PATTERN,
					ARRAY_SIZE(sc230ai_test_pattern_menu) - 1,
					0, 0, sc230ai_test_pattern_menu);

	v4l2_ctrl_new_std(handler, &sc230ai_ctrl_ops,
				V4L2_CID_HFLIP, 0, 1, 1, 0);

	v4l2_ctrl_new_std(handler, &sc230ai_ctrl_ops,
				V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&sc230ai->client->dev,
			"%d Failed to init controls(%d)\n", __LINE__,ret);
		goto err_free_handler;
	}

	sc230ai->subdev.ctrl_handler = handler;
	sc230ai->has_init_exp = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int sc230ai_check_sensor_id(struct sc230ai *sc230ai,
				   struct i2c_client *client)
{
	struct device *dev = &sc230ai->client->dev;
	u32 id = 0;
	int ret;

	ret = sc230ai_read_reg(client, SC230AI_REG_CHIP_ID,
			       SC230AI_REG_VALUE_16BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected chip id(0x%04x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected chip id (0x%04x)\n", id);

	return 0;
}

static int sc230ai_init_sensor(struct sc230ai *sc230ai)
{
	struct device *dev = &sc230ai->client->dev;
	int ret;

	ret = sc230ai_write_array(sc230ai->client, sc230ai->cur_mode->reg_list);
	if (ret)
		return ret;

	dev_info(dev, "%s end \n", __func__);

	return 0;
}

static int sc230ai_configure_regulators(struct sc230ai *sc230ai)
{
	unsigned int i;

	for (i = 0; i < SC230AI_NUM_SUPPLIES; i++)
		sc230ai->supplies[i].supply = sc230ai_supply_names[i];

	return devm_regulator_bulk_get(&sc230ai->client->dev,
				       SC230AI_NUM_SUPPLIES,
				       sc230ai->supplies);
}

static int sc230ai_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sc230ai *sc230ai;
	struct v4l2_subdev *sd;
	int ret = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	sc230ai = devm_kzalloc(dev, sizeof(*sc230ai), GFP_KERNEL);
	if (!sc230ai)
		return -ENOMEM;

	sc230ai->client = client;
	sc230ai->cur_mode = &supported_modes[0];

	sc230ai->xvclk = devm_clk_get(dev, "xclk");
	if (IS_ERR(sc230ai->xvclk)) {
		dev_err(dev, "Failed to get xclk\n");
		//return -EINVAL;
	}

	sc230ai->pwen_gpio = devm_gpiod_get_optional(dev, "pwen", GPIOD_OUT_LOW);
	if (IS_ERR(sc230ai->pwen_gpio))
		dev_warn(dev, "Failed to get pwen-gpios \n");

	sc230ai->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(sc230ai->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	sc230ai->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(sc230ai->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = sc230ai_configure_regulators(sc230ai);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&sc230ai->mutex);

	sd = &sc230ai->subdev;
	v4l2_i2c_subdev_init(sd, client, &sc230ai_subdev_ops);
	ret = sc230ai_initialize_controls(sc230ai);
	if (ret)
		goto err_destroy_mutex;

	ret = __sc230ai_power_on(sc230ai);
	if (ret)
		goto err_free_handler;

	ret = sc230ai_check_sensor_id(sc230ai, client);
	if (ret)
		goto err_power_off;

	ret = sc230ai_init_sensor(sc230ai);
	if (ret)
		goto err_power_off;

	sd->internal_ops = &sc230ai_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;

	sc230ai->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sc230ai->pad);
	if (ret < 0)
		goto err_power_off;

	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	dev_info(dev, "%s end \n", __func__);

	return 0;

err_clean_entity:
	media_entity_cleanup(&sd->entity);
err_power_off:
	__sc230ai_power_off(sc230ai);
err_free_handler:
	v4l2_ctrl_handler_free(&sc230ai->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&sc230ai->mutex);

	return ret;
}

static void sc230ai_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc230ai *sc230ai = to_sc230ai(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&sc230ai->ctrl_handler);
	mutex_destroy(&sc230ai->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sc230ai_power_off(sc230ai);
	pm_runtime_set_suspended(&client->dev);

}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sc230ai_of_match[] = {
	{ .compatible = "smartsens,sc230ai" },
	{},
};
MODULE_DEVICE_TABLE(of, sc230ai_of_match);
#endif

static const struct i2c_device_id sc230ai_match_id[] = {
	{ "smartsens,sc230ai", 0 },
	{ },
};

static struct i2c_driver sc230ai_i2c_driver = {
	.driver = {
		.name = SC230AI_NAME,
		.pm = &sc230ai_pm_ops,
		.of_match_table = of_match_ptr(sc230ai_of_match),
	},
	.probe		= sc230ai_probe,
	.remove		= sc230ai_remove,
	.id_table	= sc230ai_match_id,
};

module_i2c_driver(sc230ai_i2c_driver);

MODULE_DESCRIPTION("smartsens sc230ai sensor driver");
MODULE_LICENSE("GPL v2");
