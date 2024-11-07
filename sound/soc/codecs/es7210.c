/*
 * ALSA SoC ES7210L adc driver
 *
 * Author:		David Yang, <yangxiaohua@everest-semi.com>
 * Copyright:	(C) 2021 Everest Semiconductor Co Ltd.,
 *
 * Based on sound/soc/codecs/es7210.c by David Yang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Notes:
 *	ES7210L is a 4-ch ADC with 1.8V power supply
 *
 */
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/initval.h>
#include <linux/regmap.h>
#include "es7210.h"

#define INVALID_GPIO -1
/* codec private data */
struct es7210_priv {
	struct regmap *regmap;
	struct i2c_client *i2c;
	/*
	 * enable or disable pdm dmic interface,
	 * must be initialized in i2c_probe()
	 * pdm_dmic_enanle=1, pdm dmic interface enabled
	 * pdm_dmic_enanle=0, pdm dmic interface disabled
	 */
	u8 pdm_dmic_1_2_enable;
	u8 pdm_dmic_3_4_enable;
	u8 pdm_dmic_5_6_enable;
	u8 pdm_dmic_7_8_enable;
	u8 pdm_dmic_9_10_enable;
	u8 pdm_dmic_11_12_enable;
	u8 pdm_dmic_13_14_enable;
	u8 pdm_dmic_15_16_enable;

	struct clk *mclk;
	unsigned int sysclk;
	struct snd_pcm_hw_constraint_list *sysclk_constraints;
	u8 tdm_mode;
	struct delayed_work pcm_pop_work;
	bool sclkinv;
	unsigned int mclk_lrck_ratio;
	u8 mastermode;

	int reset_ctl_gpio;

	int32_t channels;
	int32_t adc_dev;
};

static struct es7210_priv *es7210_private;
#if 0
static int es7210_set_gpio(bool level)
{
	struct es7210_priv *es7210 = es7210_private;
	pr_info("enter into %s, level = %d\n", __func__, level);
	if (!es7210)
		return 0;

	if (es7210 && es7210->reset_ctl_gpio != INVALID_GPIO) {
		gpio_set_value(es7210->reset_ctl_gpio, level);
	}
	return 0;
}
#endif

static const struct regmap_config es7210_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x7f,
};

static void es7210_tdm_init_codec(u8 mode);

static int es7210_read(u8 reg, u8 * rt_value, struct i2c_client *client)
{
	int ret;
	u8 read_cmd[3] = { 0 };
	u8 cmd_len = 0;

	read_cmd[0] = reg;
	cmd_len = 1;

	if (client->adapter == NULL)
		pr_info("es7210_read client->adapter==NULL\n");

	ret = i2c_master_send(client, read_cmd, cmd_len);
	if (ret != cmd_len) {
		pr_info("es7210_read error1\n");
		return -1;
	}

	ret = i2c_master_recv(client, rt_value, 1);
	if (ret != 1) {
		pr_info("es7210_read error2, ret = %d.\n", ret);
		return -1;
	}

	return 0;
}

static int es7210_write(u8 reg, unsigned char value, struct i2c_client *client)
{
	int ret = 0;
	u8 write_cmd[2] = { 0 };

	write_cmd[0] = reg;
	write_cmd[1] = value;

	ret = i2c_master_send(client, write_cmd, 2);
	if (ret != 2) {
		pr_info("es7210_write error->[REG-0x%02x,val-0x%02x]\n",
			   reg, value);
		return -1;
	}

	return 0;
}

static int es7210_update_bits(u8 reg, u8 mask, u8 value,
				  struct i2c_client *client)
{
	u8 val_old, val_new;

	es7210_read(reg, &val_old, client);
	val_new = (val_old & ~mask) | (value & mask);
	if (val_new != val_old)
		es7210_write(reg, val_new, client);

	return 0;
}

static int es7210_multi_chips_write(u8 reg, unsigned char value)
{
	u8 i;
	struct es7210_priv *es7210 = es7210_private;

	for (i = 0; i < es7210->adc_dev; i++) {
		es7210_write(reg, value, i2c_clt1[i]);
	}

	return 0;
}

static int es7210_multi_chips_update_bits(u8 reg, u8 mask, u8 value)
{
	u8 i;
	struct es7210_priv *es7210 = es7210_private;

	for (i = 0; i < es7210->adc_dev; i++) {
		es7210_update_bits(reg, mask, value, i2c_clt1[i]);
	}

	return 0;
}

static const DECLARE_TLV_DB_SCALE(mic_boost_tlv, 0, 300, 0);
static const DECLARE_TLV_DB_SCALE(direct_gain_tlv, -9550, 50, 0);
#if ES7210_CHANNELS_MAX > 0
static int es7210_micboost1_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC1_GAIN_REG43, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[0]);
	return 0;
}

static int es7210_micboost1_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC1_GAIN_REG43, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost2_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC2_GAIN_REG44, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[0]);
	return 0;
}

static int es7210_micboost2_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC2_GAIN_REG44, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost3_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC3_GAIN_REG45, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[0]);
	return 0;
}

static int es7210_micboost3_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC3_GAIN_REG45, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost4_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC4_GAIN_REG46, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[0]);
	return 0;
}

static int es7210_micboost4_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC4_GAIN_REG46, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_adc1_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC12_MUTE_REG15, 0x01,
			ucontrol->value.integer.value[0] & 0x01, i2c_clt1[0]);
	return 0;
}

static int es7210_adc1_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC12_MUTE_REG15, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc2_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC12_MUTE_REG15,0x02,
			(ucontrol->value.integer.value[0] & 0x01) << 1,
			i2c_clt1[0]);
	return 0;
}

static int es7210_adc2_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC12_MUTE_REG15, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = (val & 0x02) >> 1;
	return 0;
}

static int es7210_adc3_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC34_MUTE_REG14, 0x01,
			ucontrol->value.integer.value[0] & 0x01,
			i2c_clt1[0]);
	return 0;
}

static int es7210_adc3_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC34_MUTE_REG14, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc4_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC34_MUTE_REG14, 0x02,
			(ucontrol->value.integer.value[0] & 0x01) << 1,
			i2c_clt1[0]);
	return 0;
}

static int es7210_adc4_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC34_MUTE_REG14, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = (val & 0x02) >> 1;
	return 0;
}

static int es7210_adc12_suspend_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	if ((ucontrol->value.integer.value[0] & 0x01) == 1) {
		/* suspend */
		es7210_update_bits(ES7210_MIC12_PDN_REG4B, 0xff, 0xff,
				i2c_clt1[0]);
		es7210_update_bits(ES7210_CLK_ON_OFF_REG01, 0x0a, 0x0a,
				i2c_clt1[0]);
	} else {
		/* resume */
		es7210_update_bits(ES7210_MIC12_PDN_REG4B, 0xff, 0x00,
				i2c_clt1[0]);
		es7210_read(ES7210_CLK_ON_OFF_REG01, &val, i2c_clt1[0]);
		val &= 0x74;
		es7210_write(ES7210_CLK_ON_OFF_REG01, val, i2c_clt1[0]);
	}
	return 0;
}

static int es7210_adc12_suspend_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC12_PDN_REG4B, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc34_suspend_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	if ((ucontrol->value.integer.value[0] & 0x01) == 1) {
		/* suspend */
		es7210_update_bits(ES7210_MIC34_PDN_REG4C, 0xff, 0xff,
				i2c_clt1[0]);
		es7210_update_bits(ES7210_CLK_ON_OFF_REG01, 0x14, 0x14,
				i2c_clt1[0]);
	} else {
		/* resume */
		es7210_update_bits(ES7210_MIC34_PDN_REG4C, 0xff, 0x00,
				i2c_clt1[0]);
		es7210_read(ES7210_CLK_ON_OFF_REG01, &val, i2c_clt1[0]);
		val &= 0x6a;
		es7210_write(ES7210_CLK_ON_OFF_REG01, val, i2c_clt1[0]);
	}
	return 0;
}

static int es7210_adc34_suspend_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC34_PDN_REG4C, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}
#endif

#if ES7210_CHANNELS_MAX > 4
static int es7210_micboost5_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC1_GAIN_REG43, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[1]);
	return 0;
}

static int es7210_micboost5_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC1_GAIN_REG43, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost6_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC2_GAIN_REG44, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[1]);
	return 0;
}

static int es7210_micboost6_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC2_GAIN_REG44, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost7_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC3_GAIN_REG45, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[1]);
	return 0;
}

static int es7210_micboost7_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC3_GAIN_REG45, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost8_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC4_GAIN_REG46, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[1]);
	return 0;
}

static int es7210_micboost8_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC4_GAIN_REG46, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_adc5_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC12_MUTE_REG15, 0x01,
			ucontrol->value.integer.value[0] & 0x01, i2c_clt1[1]);
	return 0;
}

static int es7210_adc5_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC12_MUTE_REG15, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc6_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC12_MUTE_REG15, 0x02,
			   (ucontrol->value.integer.value[0] & 0x01) << 1,
			   i2c_clt1[1]);
	return 0;
}

static int es7210_adc6_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC12_MUTE_REG15, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = (val & 0x02) >> 1;
	return 0;
}

static int es7210_adc7_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC34_MUTE_REG14, 0x01,
			ucontrol->value.integer.value[0] & 0x01,
			i2c_clt1[1]);
	return 0;
}

static int es7210_adc7_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC34_MUTE_REG14, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc8_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC34_MUTE_REG14, 0x02,
			(ucontrol->value.integer.value[0] & 0x01) << 1,
			i2c_clt1[1]);
	return 0;
}

static int es7210_adc8_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC34_MUTE_REG14, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = (val & 0x02) >> 1;
	return 0;
}

static int es7210_adc56_suspend_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	if ((ucontrol->value.integer.value[0] & 0x01) == 1) {
		/* suspend */
		es7210_update_bits(ES7210_MIC12_PDN_REG4B, 0xff, 0xff,
				i2c_clt1[1]);
		es7210_update_bits(ES7210_CLK_ON_OFF_REG01, 0x0a, 0x0a,
				i2c_clt1[1]);
	} else {
		/* resume */
		es7210_update_bits(ES7210_MIC12_PDN_REG4B, 0xff, 0x00,
				i2c_clt1[1]);
		es7210_read(ES7210_CLK_ON_OFF_REG01, &val, i2c_clt1[1]);
		val &= 0x74;
		es7210_write(ES7210_CLK_ON_OFF_REG01, val, i2c_clt1[1]);
	}
	return 0;
}

static int es7210_adc56_suspend_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC12_PDN_REG4B, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc78_suspend_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	if ((ucontrol->value.integer.value[0] & 0x01) == 1) {
		/* suspend */
		es7210_update_bits(ES7210_MIC34_PDN_REG4C, 0xff, 0xff,
				i2c_clt1[1]);
		es7210_update_bits(ES7210_CLK_ON_OFF_REG01, 0x14, 0x14,
				i2c_clt1[1]);
	} else {
		/* resume */
		es7210_update_bits(ES7210_MIC34_PDN_REG4C, 0xff, 0x00,
				i2c_clt1[1]);
		es7210_read(ES7210_CLK_ON_OFF_REG01, &val, i2c_clt1[1]);
		val &= 0x6a;
		es7210_write(ES7210_CLK_ON_OFF_REG01, val, i2c_clt1[1]);
	}
	return 0;
}

static int es7210_adc78_suspend_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC34_PDN_REG4C, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}
#endif
#if ES7210_CHANNELS_MAX > 8
static int es7210_micboost9_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC1_GAIN_REG43, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[2]);
	return 0;
}

static int es7210_micboost9_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC1_GAIN_REG43, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost10_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC2_GAIN_REG44, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[2]);
	return 0;
}

static int es7210_micboost10_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC2_GAIN_REG44, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost11_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC3_GAIN_REG45, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[2]);
	return 0;
}

static int es7210_micboost11_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC3_GAIN_REG45, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost12_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC4_GAIN_REG46, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[2]);
	return 0;
}

static int es7210_micboost12_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC4_GAIN_REG46, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_adc9_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC12_MUTE_REG15, 0x01,
			ucontrol->value.integer.value[0] & 0x01, i2c_clt1[2]);
	return 0;
}

static int es7210_adc9_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC12_MUTE_REG15, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc10_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC12_MUTE_REG15, 0x02,
			(ucontrol->value.integer.value[0] & 0x01) << 1,
			i2c_clt1[2]);
	return 0;
}

static int es7210_adc10_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC12_MUTE_REG15, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = (val & 0x02) >> 1;
	return 0;
}

static int es7210_adc11_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC34_MUTE_REG14, 0x01,
			ucontrol->value.integer.value[0] & 0x01, i2c_clt1[2]);
	return 0;
}

static int es7210_adc11_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC34_MUTE_REG14, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc12_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC34_MUTE_REG14, 0x02,
			(ucontrol->value.integer.value[0] & 0x01) << 1,
			i2c_clt1[2]);
	return 0;
}

static int es7210_adc12_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC34_MUTE_REG14, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = (val & 0x02) >> 1;
	return 0;
}

static int es7210_adc910_suspend_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	if ((ucontrol->value.integer.value[0] & 0x01) == 1) {
		/* suspend */
		es7210_update_bits(ES7210_MIC12_PDN_REG4B, 0xff, 0xff,
				i2c_clt1[2]);
		es7210_update_bits(ES7210_CLK_ON_OFF_REG01, 0x0a, 0x0a,
				i2c_clt1[2]);
	} else {
		/* resume */
		es7210_update_bits(ES7210_MIC12_PDN_REG4B, 0xff, 0x00,
				i2c_clt1[2]);
		es7210_read(ES7210_CLK_ON_OFF_REG01, &val, i2c_clt1[2]);
		val &= 0x74;
		es7210_write(ES7210_CLK_ON_OFF_REG01, val, i2c_clt1[2]);
	}
	return 0;
}

static int es7210_adc910_suspend_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC12_PDN_REG4B, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc1112_suspend_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	if ((ucontrol->value.integer.value[0] & 0x01) == 1) {
		/* suspend */
		es7210_update_bits(ES7210_MIC34_PDN_REG4C, 0xff, 0xff,
				i2c_clt1[2]);
		es7210_update_bits(ES7210_CLK_ON_OFF_REG01, 0x14, 0x14,
				i2c_clt1[2]);
	} else {
		/* resume */
		es7210_update_bits(ES7210_MIC34_PDN_REG4C, 0xff, 0x00,
				i2c_clt1[2]);
		es7210_read(ES7210_CLK_ON_OFF_REG01, &val, i2c_clt1[2]);
		val &= 0x6a;
		es7210_write(ES7210_CLK_ON_OFF_REG01, val, i2c_clt1[2]);
	}
	return 0;
}

static int es7210_adc1112_suspend_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC34_PDN_REG4C, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}
#endif
#if ES7210_CHANNELS_MAX > 12
static int es7210_micboost13_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC1_GAIN_REG43, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[3]);
	return 0;
}

static int es7210_micboost13_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC1_GAIN_REG43, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost14_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC2_GAIN_REG44, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[3]);
	return 0;
}

static int es7210_micboost14_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC2_GAIN_REG44, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost15_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC3_GAIN_REG45, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[3]);
	return 0;
}

static int es7210_micboost15_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC3_GAIN_REG45, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_micboost16_setting_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_MIC4_GAIN_REG46, 0x0f,
			ucontrol->value.integer.value[0] & 0x0f, i2c_clt1[3]);
	return 0;
}

static int es7210_micboost16_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC4_GAIN_REG46, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val & 0x0f;
	return 0;
}

static int es7210_adc13_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC12_MUTE_REG15, 0x01,
			ucontrol->value.integer.value[0] & 0x01, i2c_clt1[3]);
	return 0;
}

static int es7210_adc13_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC12_MUTE_REG15, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc14_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC12_MUTE_REG15, 0x02,
			(ucontrol->value.integer.value[0] & 0x01) << 1,
			i2c_clt1[3]);
	return 0;
}

static int es7210_adc14_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC12_MUTE_REG15, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = (val & 0x02) >> 1;
	return 0;
}

static int es7210_adc15_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC34_MUTE_REG14, 0x01,
			ucontrol->value.integer.value[0] & 0x01, i2c_clt1[3]);
	return 0;
}

static int es7210_adc15_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC34_MUTE_REG14, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc16_mute_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ADC34_MUTE_REG14, 0x02,
			(ucontrol->value.integer.value[0] & 0x01) << 1,
			i2c_clt1[3]);
	return 0;
}

static int es7210_adc16_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ADC34_MUTE_REG14, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = (val & 0x02) >> 1;
	return 0;
}

static int es7210_adc1314_suspend_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	if ((ucontrol->value.integer.value[0] & 0x01) == 1) {
		/* suspend */
		es7210_update_bits(ES7210_MIC12_PDN_REG4B, 0xff, 0xff,
				i2c_clt1[3]);
		es7210_update_bits(ES7210_CLK_ON_OFF_REG01, 0x0a, 0x0a,
				i2c_clt1[3]);
	} else {
		/* resume */
		es7210_update_bits(ES7210_MIC12_PDN_REG4B, 0xff, 0x00,
				i2c_clt1[3]);
		es7210_read(ES7210_CLK_ON_OFF_REG01, &val, i2c_clt1[3]);
		val &= 0x74;
		es7210_write(ES7210_CLK_ON_OFF_REG01, val, i2c_clt1[3]);
	}
	return 0;
}

static int es7210_adc1314_suspend_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC12_PDN_REG4B, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}

static int es7210_adc1516_suspend_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	if ((ucontrol->value.integer.value[0] & 0x01) == 1) {
		/* suspend */
		es7210_update_bits(ES7210_MIC34_PDN_REG4C, 0xff, 0xff,
				i2c_clt1[3]);
		es7210_update_bits(ES7210_CLK_ON_OFF_REG01, 0x14, 0x14,
				i2c_clt1[3]);
	} else {
		/* resume */
		es7210_update_bits(ES7210_MIC34_PDN_REG4C, 0xff, 0x00,
				i2c_clt1[3]);
		es7210_read(ES7210_CLK_ON_OFF_REG01, &val, i2c_clt1[3]);
		val &= 0x6a;
		es7210_write(ES7210_CLK_ON_OFF_REG01, val, i2c_clt1[3]);
	}
	return 0;
}

static int es7210_adc1516_suspend_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_MIC34_PDN_REG4C, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val & 0x01;
	return 0;
}
#endif

#if ES7210_CHANNELS_MAX > 0
static int es7210_direct_gain_1_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC1_MAX_GAIN_REG1E, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[0]);
	return 0;
}

static int es7210_direct_gain_1_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC1_MAX_GAIN_REG1E, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_2_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC2_MAX_GAIN_REG1D, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[0]);
	return 0;
}

static int es7210_direct_gain_2_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC2_MAX_GAIN_REG1D, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_3_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC3_MAX_GAIN_REG1C, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[0]);
	return 0;
}

static int es7210_direct_gain_3_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC3_MAX_GAIN_REG1C, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_4_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC4_MAX_GAIN_REG1B, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[0]);
	return 0;
}

static int es7210_direct_gain_4_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC4_MAX_GAIN_REG1B, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_12_sameon_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC_COM_CFG2_REG1A, 0x01,
			ucontrol->value.integer.value[0], i2c_clt1[0]);
	return 0;
}

static int es7210_direct_gain_12_sameon_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC_COM_CFG2_REG1A, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_34_sameon_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC_COM_CFG2_REG1A, 0x02,
			ucontrol->value.integer.value[0] << 1, i2c_clt1[0]);
	return 0;
}

static int es7210_direct_gain_34_sameon_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC_COM_CFG2_REG1A, &val, i2c_clt1[0]);
	ucontrol->value.integer.value[0] = val >> 1;
	return 0;
}
#endif

#if ES7210_CHANNELS_MAX > 4
static int es7210_direct_gain_5_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC1_MAX_GAIN_REG1E, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[1]);
	return 0;
}

static int es7210_direct_gain_5_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC1_MAX_GAIN_REG1E, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_6_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC2_MAX_GAIN_REG1D, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[1]);
	return 0;
}

static int es7210_direct_gain_6_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC2_MAX_GAIN_REG1D, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_7_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC3_MAX_GAIN_REG1C, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[1]);
	return 0;
}

static int es7210_direct_gain_7_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC3_MAX_GAIN_REG1C, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_8_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC4_MAX_GAIN_REG1B, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[1]);
	return 0;
}

static int es7210_direct_gain_8_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC4_MAX_GAIN_REG1B, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_56_sameon_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC_COM_CFG2_REG1A, 0x01,
			ucontrol->value.integer.value[0], i2c_clt1[1]);
	return 0;
}

static int es7210_direct_gain_56_sameon_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC_COM_CFG2_REG1A, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_78_sameon_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC_COM_CFG2_REG1A, 0x02,
			ucontrol->value.integer.value[0] << 1, i2c_clt1[1]);
	return 0;
}

static int es7210_direct_gain_78_sameon_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC_COM_CFG2_REG1A, &val, i2c_clt1[1]);
	ucontrol->value.integer.value[0] = val >> 1;
	return 0;
}
#endif

#if ES7210_CHANNELS_MAX > 8
static int es7210_direct_gain_9_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC1_MAX_GAIN_REG1E, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[2]);
	return 0;
}

static int es7210_direct_gain_9_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC1_MAX_GAIN_REG1E, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_10_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC2_MAX_GAIN_REG1D, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[2]);
	return 0;
}

static int es7210_direct_gain_10_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC2_MAX_GAIN_REG1D, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_11_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC3_MAX_GAIN_REG1C, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[2]);
	return 0;
}

static int es7210_direct_gain_11_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC3_MAX_GAIN_REG1C, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_12_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC4_MAX_GAIN_REG1B, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[2]);
	return 0;
}

static int es7210_direct_gain_12_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC4_MAX_GAIN_REG1B, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_9_10_sameon_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC_COM_CFG2_REG1A,0x01,
			ucontrol->value.integer.value[0], i2c_clt1[2]);
	return 0;
}

static int es7210_direct_gain_9_10_sameon_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC_COM_CFG2_REG1A, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_11_12_sameon_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC_COM_CFG2_REG1A, 0x02,
			ucontrol->value.integer.value[0] << 1, i2c_clt1[2]);
	return 0;
}

static int es7210_direct_gain_11_12_sameon_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC_COM_CFG2_REG1A, &val, i2c_clt1[2]);
	ucontrol->value.integer.value[0] = val >> 1;
	return 0;
}
#endif

#if ES7210_CHANNELS_MAX > 12
static int es7210_direct_gain_13_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC1_MAX_GAIN_REG1E, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[3]);
	return 0;
}

static int es7210_direct_gain_13_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC1_MAX_GAIN_REG1E, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_14_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC2_MAX_GAIN_REG1D, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[3]);
	return 0;
}

static int es7210_direct_gain_14_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC2_MAX_GAIN_REG1D, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_15_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC3_MAX_GAIN_REG1C, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[3]);
	return 0;
}

static int es7210_direct_gain_15_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC3_MAX_GAIN_REG1C, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_16_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC4_MAX_GAIN_REG1B, 0xff,
			ucontrol->value.integer.value[0], i2c_clt1[3]);
	return 0;
}

static int es7210_direct_gain_16_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC4_MAX_GAIN_REG1B, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_13_14_sameon_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC_COM_CFG2_REG1A, 0x01,
			ucontrol->value.integer.value[0], i2c_clt1[3]);
	return 0;
}

static int es7210_direct_gain_13_14_sameon_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC_COM_CFG2_REG1A, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int es7210_direct_gain_15_16_sameon_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	es7210_update_bits(ES7210_ALC_COM_CFG2_REG1A, 0x02,
			ucontrol->value.integer.value[0] << 1, i2c_clt1[3]);
	return 0;
}

static int es7210_direct_gain_15_16_sameon_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 val;
	es7210_read(ES7210_ALC_COM_CFG2_REG1A, &val, i2c_clt1[3]);
	ucontrol->value.integer.value[0] = val >> 1;
	return 0;
}
#endif

static const struct snd_kcontrol_new es7210_snd_controls[] = {
#if ES7210_CHANNELS_MAX > 0
	SOC_SINGLE_EXT_TLV("PGA1_setting", ES7210_MIC1_GAIN_REG43, 0, 0x0f, 0,
			es7210_micboost1_setting_get,
			es7210_micboost1_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA2_setting", ES7210_MIC2_GAIN_REG44, 0, 0x0f, 0,
			es7210_micboost2_setting_get,
			es7210_micboost2_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA3_setting", ES7210_MIC3_GAIN_REG45, 0, 0x0f, 0,
			es7210_micboost3_setting_get,
			es7210_micboost3_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA4_setting", ES7210_MIC4_GAIN_REG46, 0, 0x0f, 0,
			es7210_micboost4_setting_get,
			es7210_micboost4_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT("ADC1_MUTE", ES7210_ADC12_MUTE_REG15, 0, 1, 0,
			   es7210_adc1_mute_get, es7210_adc1_mute_set),
	SOC_SINGLE_EXT("ADC2_MUTE", ES7210_ADC12_MUTE_REG15, 1, 1, 0,
			   es7210_adc2_mute_get, es7210_adc2_mute_set),
	SOC_SINGLE_EXT("ADC3_MUTE", ES7210_ADC34_MUTE_REG14, 0, 1, 0,
			   es7210_adc3_mute_get, es7210_adc3_mute_set),
	SOC_SINGLE_EXT("ADC4_MUTE", ES7210_ADC34_MUTE_REG14, 1, 1, 0,
			   es7210_adc4_mute_get, es7210_adc4_mute_set),
	SOC_SINGLE_EXT_TLV("ADC1_DIRECT_GAIN", ES7210_ALC1_MAX_GAIN_REG1E,
			0, 0xff, 0, es7210_direct_gain_1_get,
			es7210_direct_gain_1_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC2_DIRECT_GAIN", ES7210_ALC2_MAX_GAIN_REG1D,
			0, 0xff, 0, es7210_direct_gain_2_get,
			es7210_direct_gain_2_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC3_DIRECT_GAIN", ES7210_ALC3_MAX_GAIN_REG1C,
			0, 0xff, 0, es7210_direct_gain_3_get,
			es7210_direct_gain_3_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC4_DIRECT_GAIN", ES7210_ALC4_MAX_GAIN_REG1B,
			0, 0xff, 0,
			es7210_direct_gain_4_get, es7210_direct_gain_4_set,
			direct_gain_tlv),
	SOC_SINGLE_EXT("DIRECT_1_2_GAIN_SAME_ON", ES7210_ALC_COM_CFG2_REG1A,
			0, 1, 0, es7210_direct_gain_12_sameon_get,
			es7210_direct_gain_12_sameon_set),
	SOC_SINGLE_EXT("DIRECT_3_4_GAIN_SAME_ON",ES7210_ALC_COM_CFG2_REG1A,
			1, 1, 0, es7210_direct_gain_34_sameon_get,
			es7210_direct_gain_34_sameon_set),
	SOC_SINGLE_EXT("ADC12_SUSPEND", ES7210_MIC12_PDN_REG4B, 0, 1, 0,
			es7210_adc12_suspend_get, es7210_adc12_suspend_set),
	SOC_SINGLE_EXT("ADC34_SUSPEND",ES7210_MIC34_PDN_REG4C, 0, 1, 0,
			es7210_adc34_suspend_get, es7210_adc34_suspend_set),
#endif

#if ES7210_CHANNELS_MAX > 4
	SOC_SINGLE_EXT_TLV("PGA5_setting", ES7210_MIC1_GAIN_REG43, 0, 0x0f, 0,
			es7210_micboost5_setting_get,
			es7210_micboost5_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA6_setting", ES7210_MIC2_GAIN_REG44, 0, 0x0f, 0,
			es7210_micboost6_setting_get,
			es7210_micboost6_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA7_setting", ES7210_MIC3_GAIN_REG45, 0, 0x0f, 0,
			es7210_micboost7_setting_get,
			es7210_micboost7_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA8_setting", ES7210_MIC4_GAIN_REG46, 0, 0x0f, 0,
			es7210_micboost8_setting_get,
			es7210_micboost8_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT("ADC5_MUTE", ES7210_ADC12_MUTE_REG15, 0, 1, 0,
			es7210_adc5_mute_get, es7210_adc5_mute_set),
	SOC_SINGLE_EXT("ADC6_MUTE", ES7210_ADC12_MUTE_REG15, 1, 1, 0,
			es7210_adc6_mute_get, es7210_adc6_mute_set),
	SOC_SINGLE_EXT("ADC7_MUTE", ES7210_ADC34_MUTE_REG14, 0, 1, 0,
			es7210_adc7_mute_get, es7210_adc7_mute_set),
	SOC_SINGLE_EXT("ADC8_MUTE", ES7210_ADC34_MUTE_REG14, 1, 1, 0,
			es7210_adc8_mute_get, es7210_adc8_mute_set),
	SOC_SINGLE_EXT_TLV("ADC5_DIRECT_GAIN", ES7210_ALC1_MAX_GAIN_REG1E,
			0, 0xff, 0, es7210_direct_gain_5_get,
			es7210_direct_gain_5_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC6_DIRECT_GAIN", ES7210_ALC2_MAX_GAIN_REG1D,
			0, 0xff, 0, es7210_direct_gain_6_get,
			es7210_direct_gain_6_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC7_DIRECT_GAIN", ES7210_ALC3_MAX_GAIN_REG1C,
			0, 0xff, 0, es7210_direct_gain_7_get,
			es7210_direct_gain_7_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC8_DIRECT_GAIN", ES7210_ALC4_MAX_GAIN_REG1B,
			0, 0xff, 0, es7210_direct_gain_8_get,
			es7210_direct_gain_8_set, direct_gain_tlv),
	SOC_SINGLE_EXT("DIRECT_5_6_GAIN_SAME_ON", ES7210_ALC_COM_CFG2_REG1A,
			0, 1, 0, es7210_direct_gain_56_sameon_get,
			es7210_direct_gain_56_sameon_set),
	SOC_SINGLE_EXT("DIRECT_7_8_GAIN_SAME_ON", ES7210_ALC_COM_CFG2_REG1A,
			1, 1, 0, es7210_direct_gain_78_sameon_get,
			es7210_direct_gain_78_sameon_set),
	SOC_SINGLE_EXT("ADC56_SUSPEND", ES7210_MIC12_PDN_REG4B, 0, 1, 0,
			es7210_adc56_suspend_get, es7210_adc56_suspend_set),
	SOC_SINGLE_EXT("ADC78_SUSPEND",ES7210_MIC34_PDN_REG4C, 0, 1, 0,
			es7210_adc78_suspend_get, es7210_adc78_suspend_set),
#endif
#if ES7210_CHANNELS_MAX > 8
	SOC_SINGLE_EXT_TLV("PGA9_setting", ES7210_MIC1_GAIN_REG43, 0, 0x0f, 0,
			es7210_micboost9_setting_get,
			es7210_micboost9_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA10_setting", ES7210_MIC2_GAIN_REG44, 0, 0x0f, 0,
			es7210_micboost10_setting_get,
			es7210_micboost10_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA11_setting", ES7210_MIC3_GAIN_REG45, 0, 0x0f, 0,
			es7210_micboost11_setting_get,
			es7210_micboost11_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA12_setting", ES7210_MIC4_GAIN_REG46, 0, 0x0f, 0,
			es7210_micboost12_setting_get,
			es7210_micboost12_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT("ADC9_MUTE", ES7210_ADC12_MUTE_REG15, 0, 1, 0,
			es7210_adc9_mute_get,es7210_adc9_mute_set),
	SOC_SINGLE_EXT("ADC10_MUTE", ES7210_ADC12_MUTE_REG15, 1, 1, 0,
			es7210_adc10_mute_get, es7210_adc10_mute_set),
	SOC_SINGLE_EXT("ADC11_MUTE", ES7210_ADC34_MUTE_REG14, 0, 1, 0,
			es7210_adc11_mute_get, es7210_adc11_mute_set),
	SOC_SINGLE_EXT("ADC12_MUTE", ES7210_ADC34_MUTE_REG14, 1, 1, 0,
			es7210_adc12_mute_get, es7210_adc12_mute_set),
	SOC_SINGLE_EXT_TLV("ADC9_DIRECT_GAIN", ES7210_ALC1_MAX_GAIN_REG1E,
			0, 0xff, 0, es7210_direct_gain_9_get,
			es7210_direct_gain_9_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC10_DIRECT_GAIN", ES7210_ALC2_MAX_GAIN_REG1D,
			0, 0xff, 0, es7210_direct_gain_10_get,
			es7210_direct_gain_10_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC11_DIRECT_GAIN", ES7210_ALC3_MAX_GAIN_REG1C,
			0, 0xff, 0, es7210_direct_gain_11_get,
			es7210_direct_gain_11_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC12_DIRECT_GAIN", ES7210_ALC4_MAX_GAIN_REG1B,
			0, 0xff, 0, es7210_direct_gain_12_get,
			es7210_direct_gain_12_set, direct_gain_tlv),
	SOC_SINGLE_EXT("DIRECT_9_10_GAIN_SAME_ON", ES7210_ALC_COM_CFG2_REG1A,
			0, 1, 0, es7210_direct_gain_9_10_sameon_get,
			es7210_direct_gain_9_10_sameon_set),
	SOC_SINGLE_EXT("DIRECT_11_12_GAIN_SAME_ON", ES7210_ALC_COM_CFG2_REG1A,
			1, 1, 0, es7210_direct_gain_11_12_sameon_get,
			es7210_direct_gain_11_12_sameon_set),
	SOC_SINGLE_EXT("ADC910_SUSPEND", ES7210_MIC12_PDN_REG4B, 0, 1, 0,
			es7210_adc910_suspend_get, es7210_adc910_suspend_set),
	SOC_SINGLE_EXT("ADC1112_SUSPEND", ES7210_MIC34_PDN_REG4C, 0, 1, 0,
			es7210_adc1112_suspend_get, es7210_adc1112_suspend_set),
#endif
#if ES7210_CHANNELS_MAX > 12
	SOC_SINGLE_EXT_TLV("PGA13_setting", ES7210_MIC1_GAIN_REG43, 0, 0x0f, 0,
			es7210_micboost13_setting_get,
			es7210_micboost13_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA14_setting", ES7210_MIC2_GAIN_REG44, 0, 0x0f, 0,
			es7210_micboost14_setting_get,
			es7210_micboost14_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA15_setting", ES7210_MIC3_GAIN_REG45, 0, 0x0f, 0,
			es7210_micboost15_setting_get,
			es7210_micboost15_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("PGA16_setting", ES7210_MIC4_GAIN_REG46, 0, 0x0f, 0,
			es7210_micboost16_setting_get,
			es7210_micboost16_setting_set, mic_boost_tlv),
	SOC_SINGLE_EXT("ADC13_MUTE", ES7210_ADC12_MUTE_REG15, 0, 1, 0,
			es7210_adc13_mute_get, es7210_adc13_mute_set),
	SOC_SINGLE_EXT("ADC14_MUTE", ES7210_ADC12_MUTE_REG15, 1, 1, 0,
			es7210_adc14_mute_get, es7210_adc14_mute_set),
	SOC_SINGLE_EXT("ADC15_MUTE", ES7210_ADC34_MUTE_REG14, 0, 1, 0,
			es7210_adc15_mute_get, es7210_adc15_mute_set),
	SOC_SINGLE_EXT("ADC16_MUTE", ES7210_ADC34_MUTE_REG14, 1, 1, 0,
			es7210_adc16_mute_get, es7210_adc16_mute_set),
	SOC_SINGLE_EXT_TLV("ADC13_DIRECT_GAIN", ES7210_ALC1_MAX_GAIN_REG1E,
			0, 0xff, 0, es7210_direct_gain_13_get,
			es7210_direct_gain_13_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC14_DIRECT_GAIN", ES7210_ALC2_MAX_GAIN_REG1D,
			0, 0xff, 0, es7210_direct_gain_14_get,
			es7210_direct_gain_14_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC15_DIRECT_GAIN", ES7210_ALC3_MAX_GAIN_REG1C,
			0, 0xff, 0, es7210_direct_gain_15_get,
			es7210_direct_gain_15_set, direct_gain_tlv),
	SOC_SINGLE_EXT_TLV("ADC16_DIRECT_GAIN", ES7210_ALC4_MAX_GAIN_REG1B,
			0, 0xff, 0, es7210_direct_gain_16_get,
			es7210_direct_gain_16_set, direct_gain_tlv),
	SOC_SINGLE_EXT("DIRECT_13_14_GAIN_SAME_ON", ES7210_ALC_COM_CFG2_REG1A,
			0, 1, 0, es7210_direct_gain_13_14_sameon_get,
			es7210_direct_gain_13_14_sameon_set),
	SOC_SINGLE_EXT("DIRECT_15_16_GAIN_SAME_ON", ES7210_ALC_COM_CFG2_REG1A,
			1, 1, 0, es7210_direct_gain_15_16_sameon_get,
			es7210_direct_gain_15_16_sameon_set),
	SOC_SINGLE_EXT("ADC1314_SUSPEND", ES7210_MIC12_PDN_REG4B, 0, 1, 0,
			es7210_adc1314_suspend_get, es7210_adc1314_suspend_set),
	SOC_SINGLE_EXT("ADC1516_SUSPEND", ES7210_MIC34_PDN_REG4C, 0, 1, 0,
			es7210_adc1516_suspend_get, es7210_adc1516_suspend_set),
#endif

};

struct _coeff_div{
	u32 mclk;
	u32 sr_rate;
	u8	ss_ds;
	u8	dll_power;
	u8	div_mul;
	u8	osr;
	u8	lrck_h;
	u8	lrck_l;
	u8	bclkdiv;
};

static const struct _coeff_div coeff_div[] ={
	/* 24.576MHz */
	{24576000, 96000, 0, 0x04, 0xC1, 0x20, 0x01, 0x00, 0x04},
	{24576000, 48000, 0, 0x04, 0x81, 0x20, 0x02, 0x00, 0x08},
	{24576000, 32000, 0, 0x04, 0xC3, 0x20, 0x03, 0x00, 0x0C},
	{24576000, 24000, 0, 0x04, 0x82, 0x20, 0x04, 0x00, 0x10},
	{24576000, 16000, 0, 0x04, 0x83, 0x20, 0x06, 0x00, 0x18},
	{24576000, 12000, 0, 0x04, 0x84, 0x20, 0x08, 0x00, 0x20},

	/* 12.288MHz */
	{12288000, 96000, 0, 0x00, 0x42, 0x20, 0x00, 0x80, 0x02},
	{12288000, 64000, 0, 0x00, 0x03, 0x20, 0x00, 0xc0, 0x03},
	{12288000, 48000, 0, 0x04, 0xC1, 0x20, 0x01, 0x00, 0x04},
	{12288000, 32000, 0, 0x00, 0x03, 0x20, 0x01, 0x80, 0x06},
	{12288000, 24000, 0, 0x04, 0x81, 0x20, 0x02, 0x00, 0x08},
	{12288000, 16000, 0, 0x04, 0xC3, 0x20, 0x03, 0x00, 0x0c},
	{12288000, 12000, 0, 0x04, 0x82, 0x20, 0x04, 0x00, 0x10},
	{12288000, 8000 , 0, 0x04, 0x83, 0x20, 0x06, 0x00, 0x18},

	/* 6.144MHz */
	{6144000 , 96000, 0, 0x00, 0x41, 0x20, 0x00, 0x40, 0x01},
	{6144000 , 48000, 0, 0x00, 0x01, 0x20, 0x00, 0x80, 0x02},
	{6144000 , 32000, 0, 0x00, 0x43, 0x20, 0x00, 0xC0, 0x03},
	{6144000 , 24000, 0, 0x04, 0xC1, 0x20, 0x01, 0x00, 0x04},
	{6144000 , 16000, 0, 0x00, 0x03, 0x20, 0x01, 0x80, 0x06},
	{6144000 , 12000, 0, 0x04, 0x81, 0x20, 0x02, 0x00, 0x08},
	{6144000 , 8000 , 0, 0x04, 0xC3, 0x20, 0x03, 0x00, 0x0C},

	/* 3.072MHz */
	{3072000 , 96000, 1, 0x00, 0x41, 0x20, 0x00, 0x20, 0x01},
	{3072000 , 48000, 0, 0x00, 0x41, 0x20, 0x00, 0x40, 0x01},
	{3072000 , 24000, 0, 0x00, 0x01, 0x20, 0x00, 0x80, 0x02},
	{3072000 , 16000, 0, 0x00, 0x43, 0x20, 0x00, 0xC0, 0x03},
	{3072000 , 12000, 0, 0x04, 0xC1, 0x20, 0x01, 0x00, 0x04},
	{3072000 , 8000 , 0, 0x00, 0x03, 0x20, 0x01, 0x80, 0x06},

	/* 1.536MHz */
	{1536000 , 24000, 0, 0x00, 0x41, 0x20, 0x00, 0x40, 0x01},
	{1536000 , 12000, 0, 0x00, 0x01, 0x20, 0x00, 0x80, 0x02},
	{1536000 , 8000 , 0, 0x00, 0x43, 0x20, 0x00, 0xC0, 0x03},

	/* 32.768MHZ */
	{32768000, 64000, 0, 0x04, 0x81, 0x20, 0x02, 0x00, 0x08},
	{32768000, 32000, 0, 0x04, 0x82, 0x20, 0x04, 0x00, 0x10},
	{32768000, 16000, 0, 0x04, 0x84, 0x20, 0x08, 0x00, 0x20},

	/* 16.384MHZ */
	{16384000, 64000, 0, 0x04, 0xC1, 0x20, 0x01, 0x00, 0x04},
	{16384000, 32000, 0, 0x04, 0x81, 0x20, 0x02, 0x00, 0x08},
	{16384000, 16000, 0, 0x04, 0x82, 0x20, 0x04, 0x00, 0x10},
	{16384000, 8000 , 0, 0x04, 0x84, 0x20, 0x08, 0x00, 0x20},

	/* 8.192MHZ */
	{8192000, 64000, 0, 0x00, 0x42, 0x20, 0x00, 0x80, 0x02},
	{8192000, 32000, 0, 0x04, 0xC1, 0x20, 0x01, 0x00, 0x04},
	{8192000, 16000, 0, 0x04, 0x81, 0x20, 0x02, 0x00, 0x08},
	{8192000, 8000 , 0, 0x04, 0x82, 0x20, 0x04, 0x00, 0x10},

	/* 4.096MHZ */
	{4096000, 64000, 0, 0x00, 0x41, 0x20, 0x00, 0x40, 0x01},
	{4096000, 32000, 0, 0x00, 0x01, 0x20, 0x00, 0x80, 0x02},
	{4096000, 16000, 0, 0x04, 0xC1, 0x20, 0x01, 0x00, 0x04},
	{4096000, 8000 , 0, 0x04, 0x81, 0x20, 0x02, 0x00, 0x08},

	/* 22.5792MHZ */
	{22579200, 88200, 0, 0x04, 0xC1, 0x20, 0x01, 0x00, 0x04},
	{22579200, 44100, 0, 0x04, 0x81, 0x20, 0x02, 0x00, 0x08},
	{22579200, 22050, 0, 0x04, 0x82, 0x20, 0x04, 0x00, 0x10},
	{22579200, 11025, 0, 0x04, 0x84, 0x20, 0x08, 0x00, 0x20},

	/* 11.2896MHZ */
	{11289600, 88200, 0, 0x00, 0x42, 0x20, 0x00, 0x80, 0x02},
	{11289600, 44100, 0, 0x04, 0xC1, 0x20, 0x01, 0x00, 0x04},
	{11289600, 22050, 0, 0x04, 0x81, 0x20, 0x02, 0x00, 0x08},
	{11289600, 11025, 0, 0x04, 0x82, 0x20, 0x04, 0x00, 0x10},

	/* 5.6448MHZ */
	{5644800, 88200, 0, 0x00, 0x41, 0x20, 0x00, 0x40, 0x01},
	{5644800, 44100, 0, 0x00, 0x01, 0x20, 0x00, 0x80, 0x02},
	{5644800, 22050, 0, 0x04, 0xC1, 0x20, 0x01, 0x00, 0x04},
	{5644800, 11025, 0, 0x04, 0x81, 0x20, 0x02, 0x00, 0x08},

	/* 19200MHZ */
	{19200000, 96000, 1, 0x00, 0x45, 0x28, 0x00, 0xc8, 0x03},
	{19200000, 64000, 0, 0x00, 0x45, 0x1E, 0x01, 0x2c, 0x04},
	{19200000, 48000, 0, 0x00, 0x45, 0x28, 0x01, 0x90, 0x06},
	{19200000, 32000, 0, 0x00, 0x05, 0x1E, 0x02, 0x58, 0x09},
	{19200000, 24000, 0, 0x00, 0x0A, 0x28, 0x03, 0x20, 0x0C},
	{19200000, 16000, 0, 0x00, 0x0A, 0x1E, 0x04, 0x80, 0x12},
	{19200000, 12000, 0, 0x00, 0x54, 0x28, 0x06, 0x40, 0x19},
	{19200000, 8000 , 0, 0x00, 0x5E, 0x28, 0x09, 0x60, 0x25},

};

static inline int get_coeff(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].sr_rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}

	return -EINVAL;
}

/* The set of rates we can generate from the above for each SYSCLK */

static unsigned int rates_12288[] = {
	8000, 12000, 16000, 24000, 32000, 48000, 64000, 96000,
};

static unsigned int rates_8192[] = {
	8000, 16000, 32000, 64000,
};

static unsigned int rates_112896[] = {
	11025, 22050, 44100, 88200
};

static struct snd_pcm_hw_constraint_list constraints_12288 = {
	.count = ARRAY_SIZE(rates_12288),
	.list = rates_12288,
};

static struct snd_pcm_hw_constraint_list constraints_8192 = {
	.count = ARRAY_SIZE(rates_8192),
	.list = rates_8192,
};

static struct snd_pcm_hw_constraint_list constraints_112896 = {
	.count = ARRAY_SIZE(rates_112896),
	.list = rates_112896,
};

/*
 * Note that this should be called from init rather than from hw_params.
 */
static int es7210_set_dai_sysclk(struct snd_soc_dai *dai,
				 int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_component *component = dai->component;
	struct es7210_priv *es7210 = snd_soc_component_get_drvdata(component);

	pr_info("enter into %s\n", __func__);
	freq = 24576000;
	switch (freq) {
	case 5644800:
	case 11289600:
	case 22579200:
		es7210->sysclk_constraints = &constraints_112896;
		es7210->sysclk = freq;
		return 0;
	case 3072000:
	case 6144000:
	case 12288000:
	case 24576000:
		es7210->sysclk_constraints = &constraints_12288;
		es7210->sysclk = freq;
		return 0;
	case 4096000:
	case 8192000:
	case 16384000:
	case 32768000:
		es7210->sysclk_constraints = &constraints_8192;
		es7210->sysclk = freq;
		return 0;
	}
	return 0;
}

static int es7210_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{

	struct snd_soc_component *component = dai->component;
	struct es7210_priv *es7210 = snd_soc_component_get_drvdata(component);
	u8 i;
	u8 adciface = 0;
	u8 tdmiface = 0;
	u8 clksel = 0;
	pr_info("enter into %s\n", __func__);

	for (i = 0; i< es7210->adc_dev; i++) {
		/* set master/slave audio interface */
		switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBM_CFM:	/* MASTER MODE */
			pr_info("%s Master mode\n", __func__);
			es7210->mastermode = 1;
			if(i == 0 ) {
				es7210_update_bits(ES7210_MST_CLK_CTL_REG03,
						0x80, 0x00, i2c_clt1[i]);
				es7210_update_bits(ES7210_CLK_ON_OFF_REG01,
						0x60, 0x40, i2c_clt1[i]);
				es7210_write(ES7210_RESET_CTL_REG00,
						0x71, i2c_clt1[i]);
				es7210_write(ES7210_RESET_CTL_REG00,
						0x01, i2c_clt1[i]);
				es7210_update_bits(ES7210_MODE_CFG_REG08,
						0x01, 0x01, i2c_clt1[i]);
			} else {
				es7210_update_bits(ES7210_CLK_ON_OFF_REG01,
						0x60, 0x20, i2c_clt1[i]);
				es7210_write(ES7210_RESET_CTL_REG00,
						0x71, i2c_clt1[i]);
				es7210_write(ES7210_RESET_CTL_REG00,
						0x41, i2c_clt1[i]);
				es7210_update_bits(ES7210_MODE_CFG_REG08,
						0x01, 0x00, i2c_clt1[i]);
			}
			break;
		case SND_SOC_DAIFMT_CBS_CFS:	/* SLAVE MODE */
			es7210->mastermode = 0;
			pr_info("%s Slave mode\n", __func__);
			es7210_update_bits(ES7210_CLK_ON_OFF_REG01,
					0x60, 0x20, i2c_clt1[i]);
			es7210_write(ES7210_RESET_CTL_REG00,
					0x71, i2c_clt1[i]);
			es7210_write(ES7210_RESET_CTL_REG00,
					0x41, i2c_clt1[i]);
			es7210_update_bits(ES7210_MODE_CFG_REG08,
					0x01, 0x00, i2c_clt1[i]);
			break;
		default:
			return -EINVAL;
		}

		es7210_read(ES7210_SDP_CFG1_REG11, &adciface, i2c_clt1[i]);
		es7210_read(ES7210_SDP_CFG2_REG12, &tdmiface, i2c_clt1[i]);
		/* interface format */
		switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			adciface &= 0xFC;
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			return -EINVAL;
		case SND_SOC_DAIFMT_LEFT_J:
			adciface &= 0xFC;
			adciface |= 0x01;
			break;
		case SND_SOC_DAIFMT_DSP_A:
			adciface &= 0xEC;
			adciface |= 0x03;
			break;
		case SND_SOC_DAIFMT_DSP_B:
			adciface &= 0xEC;
			adciface |= 0x13;
			break;
		default:
			return -EINVAL;
		}

		if ((ES7210_TDM_ENABLE == ENABLE) &&
				(ES7210_NFS_ENABLE == DISABLE)) {

			pr_info("%s 1FS TDM Mode\n", __func__);
			switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
			case SND_SOC_DAIFMT_I2S:
				tdmiface &= 0xFC;
				tdmiface |= 0x02;
				break;
			case SND_SOC_DAIFMT_RIGHT_J:
				return -EINVAL;
			case SND_SOC_DAIFMT_LEFT_J:
				tdmiface &= 0xFC;
				tdmiface |= 0x02;
				break;
			case SND_SOC_DAIFMT_DSP_A:
				tdmiface &= 0xFC;
				tdmiface |= 0x01;
				break;
			case SND_SOC_DAIFMT_DSP_B:
				tdmiface &= 0xFC;
				tdmiface |= 0x01;
				break;
			default:
				return -EINVAL;
			}
		}

		if ((ES7210_TDM_ENABLE == ENABLE) &&
				(ES7210_NFS_ENABLE == ENABLE)) {
			pr_info("%s NFS TDM Mode\n", __func__);
			if((fmt & SND_SOC_DAIFMT_FORMAT_MASK) ==
					SND_SOC_DAIFMT_RIGHT_J) {
				return -EINVAL;
			} else {
				tdmiface &= 0xFC;
				tdmiface |= 0x03;
			}
		}

		es7210_read(ES7210_MODE_CFG_REG08, &clksel, i2c_clt1[i]);
		/* clock inversion */
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			adciface &= 0xef;
			clksel &= 0xf7;
			break;
		case SND_SOC_DAIFMT_IB_IF:
			adciface |= 0x10;
			clksel |= 0x08;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			adciface &= 0xef;
			clksel |= 0x08;
			break;
		case SND_SOC_DAIFMT_NB_IF:
			adciface |= 0x10;
			clksel &= 0xf7;
			break;
		default:
			return -EINVAL;
		}

		es7210_write(ES7210_SDP_CFG1_REG11, adciface, i2c_clt1[i]);
		es7210_write(ES7210_SDP_CFG2_REG12, tdmiface, i2c_clt1[i]);
	}

	return 0;
}

static void pcm_pop_work_events(struct work_struct *work)
{
	pr_info("enter into %s\n", __func__);
	es7210_multi_chips_update_bits(ES7210_ADC34_MUTE_REG14, 0x03, 0x00);
	es7210_multi_chips_update_bits(ES7210_ADC12_MUTE_REG15, 0x03, 0x00);
	es7210_init_reg = 1;
}

static int es7210_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	pr_info("enter into %s, mute = %d\n", __func__, mute);
	if (mute) {
		es7210_multi_chips_update_bits(ES7210_ADC34_MUTE_REG14,
				0x03, 0x03);
		es7210_multi_chips_update_bits(ES7210_ADC12_MUTE_REG15,
				0x03, 0x03);
	} else {
		es7210_multi_chips_update_bits(ES7210_ADC34_MUTE_REG14,
				0x03, 0x00);
		es7210_multi_chips_update_bits(ES7210_ADC12_MUTE_REG15,
				0x03, 0x00);
	}
	return 0;
}

static int es7210_pcm_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct es7210_priv *es7210 = snd_soc_component_get_drvdata(component);

	pr_info("Enter into %s()\n", __func__);
	if (es7210_init_reg == 0) {
		schedule_delayed_work(&es7210->pcm_pop_work,
					  msecs_to_jiffies(100));
	}

	es7210_tdm_init_codec(es7210->tdm_mode);

	/* Setting MIC GAIN */
	if (es7210->channels > 0) {
		es7210_write(ES7210_MIC1_GAIN_REG43, 0x1e, i2c_clt1[0]);
		es7210_write(ES7210_MIC2_GAIN_REG44, 0x1e, i2c_clt1[0]);
		es7210_write(ES7210_MIC3_GAIN_REG45, 0x1e, i2c_clt1[0]);
		es7210_write(ES7210_MIC4_GAIN_REG46, 0x1e, i2c_clt1[0]);
	}

	return 0;
}

static int es7210_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct es7210_priv *es7210 = snd_soc_component_get_drvdata(component);
	u8 clksel;
	u8 i, coeff;
	u8 regv;

	pr_info("Enter into %s()\n", __func__);

	switch(params_rate(params)) {
	case 44100:
	case 22050:
		es7210->sysclk = 11289600;
		break;
	case 16000:
	case 32000:
	case 48000:
	case 96000:
		es7210->sysclk = 24576000;
		break;
	default:
		es7210->sysclk = 12288000;
		break;
	}

	coeff = get_coeff(es7210->sysclk, params_rate(params));
	if (coeff < 0) {
		pr_info("Unable to configure sample rate %dHz with %dHz MCLK",
			params_rate(params), es7210->sysclk) ;
		return coeff;
	}

	if((es7210->tdm_mode  != ES7210_TDM_NLRCK_DSPA) &&
			(es7210->tdm_mode  != ES7210_TDM_NLRCK_DSPB) &&
			(es7210->tdm_mode  != ES7210_TDM_NLRCK_I2S) &&
			(es7210->tdm_mode  != ES7210_TDM_NLRCK_LJ)) {

		regv = coeff_div[coeff].ss_ds << 1;
		es7210_multi_chips_update_bits(ES7210_MODE_CFG_REG08,
				0x02, regv);
		es7210_multi_chips_write(ES7210_DIGITAL_PDN_REG06,
				coeff_div[coeff].dll_power);
		es7210_multi_chips_write(ES7210_MCLK_CTL_REG02,
				coeff_div[coeff].div_mul);
		es7210_multi_chips_write(ES7210_ADC_OSR_REG07,
				coeff_div[coeff].osr);
		es7210_multi_chips_write(ES7210_MST_LRCDIVH_REG04,
				coeff_div[coeff].lrck_h);
		es7210_multi_chips_write(ES7210_MST_LRCDIVL_REG05,
				coeff_div[coeff].lrck_l);
		es7210_multi_chips_write(ES7210_MST_CLK_CTL_REG03,
				coeff_div[coeff].bclkdiv);
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		es7210_multi_chips_update_bits(ES7210_SDP_CFG1_REG11,
				0xe0, 0x60);
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		es7210_multi_chips_update_bits(ES7210_SDP_CFG1_REG11,
				0xe0, 0x20);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		es7210_multi_chips_update_bits(ES7210_SDP_CFG1_REG11,
				0xe0, 0x00);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		es7210_multi_chips_update_bits(ES7210_SDP_CFG1_REG11,
				0xe0, 0x80);
		break;
	default:
		es7210_multi_chips_update_bits(ES7210_SDP_CFG1_REG11,
				0xe0, 0x60);
		break;
	}

	for (i = 0; i < es7210->adc_dev; i++) {
		if (params_rate(params) >= 64000) {
			es7210_read(ES7210_MODE_CFG_REG08, &clksel, i2c_clt1[i]);
			clksel |= 0x1 << 1;
			es7210_write(ES7210_MODE_CFG_REG08, clksel, i2c_clt1[i]);
		}
	}

	return 0;
}

static const struct snd_soc_dapm_widget es7210_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("INPUT"),
	SND_SOC_DAPM_ADC("ADC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT", "I2S Capture", 0, SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_dapm_route es7210_dapm_routes[] = {
	{"ADC", NULL, "INPUT"},
	{"SDOUT", NULL, "ADC"},
};

#define es7210_RATES SNDRV_PCM_RATE_8000_96000

#define es7210_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops es7210_ops = {
	.startup = es7210_pcm_startup,
	.hw_params = es7210_pcm_hw_params,
	.set_fmt = es7210_set_dai_fmt,
	.set_sysclk = es7210_set_dai_sysclk,
	.mute_stream = es7210_mute,
};


static struct snd_soc_dai_driver es7210_dai0 = {
	.name = "ES7210 4CH ADC 0",
	.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = MIC_CHN_16,
			.rates = es7210_RATES,
			.formats = es7210_FORMATS,
			},
	.ops = &es7210_ops,
	.symmetric_rate = 1,
	.symmetric_sample_bits = 1,
};


static struct snd_soc_dai_driver *es7210_dai[] = {
	&es7210_dai0,
};

static int es7210_set_bias_level(struct snd_soc_component *component,
				enum snd_soc_bias_level level)
{
	struct es7210_priv *es7210 = snd_soc_component_get_drvdata(component);
	u8 i = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
		dev_dbg(&es7210->i2c->dev, "%s on\n", __func__);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		dev_dbg(&es7210->i2c->dev, "%s standby\n", __func__);
		es7210_multi_chips_update_bits(ES7210_DIGITAL_PDN_REG06,
				0x03, 0x00);
		for (i = 0;i < es7210->adc_dev; i++) {
			if (es7210->mastermode == 1) {
				if(i == 0) {
					es7210_write(ES7210_CLK_ON_OFF_REG01,
							0x40, i2c_clt1[i]);
				} else {
					es7210_write(ES7210_CLK_ON_OFF_REG01,
							0x20, i2c_clt1[i]);
				}
			} else {
				es7210_write(ES7210_CLK_ON_OFF_REG01,
						0x20, i2c_clt1[i]);
			}
		}
		es7210_multi_chips_write(ES7210_ANALOG_SYS_REG40, 0x42);
		es7210_multi_chips_write(ES7210_CHIP_STA_REG0B, 0x02);
		es7210_multi_chips_write(ES7210_MIC12_PDN_REG4B, 0x00);
		es7210_multi_chips_write(ES7210_MIC34_PDN_REG4C, 0x00);
		break;
	case SND_SOC_BIAS_OFF:
		pr_info("%s off\n", __func__);
		es7210_multi_chips_write(ES7210_MIC12_PDN_REG4B, 0xff);
		es7210_multi_chips_write(ES7210_MIC34_PDN_REG4C, 0xff);
		es7210_multi_chips_write(ES7210_CHIP_STA_REG0B, 0xD0);
		es7210_multi_chips_write(ES7210_ANALOG_SYS_REG40, 0x80);
		es7210_multi_chips_write(ES7210_CLK_ON_OFF_REG01, 0x7f);
		es7210_multi_chips_update_bits(ES7210_DIGITAL_PDN_REG06,
				0x03, 0x03);
		es7210_multi_chips_update_bits(ES7210_MIC1_GAIN_REG43,
				0x10, 0x00);
		es7210_multi_chips_update_bits(ES7210_MIC2_GAIN_REG44,
				0x10, 0x00);
		es7210_multi_chips_update_bits(ES7210_MIC3_GAIN_REG45,
				0x10, 0x00);
		es7210_multi_chips_update_bits(ES7210_MIC4_GAIN_REG46,
				0x10, 0x00);
		break;
	}

	return 0;
}

static int es7210_suspend(struct snd_soc_component *component)
{
	pr_info("enter into %s\n", __func__);
	es7210_set_bias_level(component, SND_SOC_BIAS_OFF);
	return 0;
}

static int es7210_resume(struct snd_soc_component *component)
{
	pr_info("enter into %s\n", __func__);
	es7210_set_bias_level(component, SND_SOC_BIAS_STANDBY);
	return 0;
}

/*
* to initialize es7210 for tdm mode
*/
static void es7210_tdm_init_codec(u8 mode)
{
	int cnt, channel, i;
	struct es7210_priv *es7210 = es7210_private;

	for (cnt = 0;
		 cnt < sizeof(es7210_tdm_reg_common_cfg1) /
		 sizeof(es7210_tdm_reg_common_cfg1[0]); cnt++) {
		es7210_multi_chips_write(
				es7210_tdm_reg_common_cfg1[cnt].reg_addr,
				es7210_tdm_reg_common_cfg1 [cnt].reg_v);
	}
	switch (mode) {
	case ES7210_TDM_1LRCK_DSPA:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x83);
		es7210_multi_chips_write(ES7210_SDP_CFG2_REG12, 0x01);
		break;
	case ES7210_TDM_1LRCK_DSPB:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x93);
		es7210_multi_chips_write(ES7210_SDP_CFG2_REG12, 0x01);
		break;
	case ES7210_TDM_1LRCK_I2S:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x80);
		es7210_multi_chips_write(ES7210_SDP_CFG2_REG12, 0x02);
		break;
	case ES7210_TDM_1LRCK_LJ:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x81);
		es7210_multi_chips_write(ES7210_SDP_CFG2_REG12, 0x02);
		break;
	case ES7210_TDM_NLRCK_DSPA:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x83);
		for (cnt = 0; cnt < es7210->adc_dev; cnt++) {
			if (cnt == 0) {
				es7210_write(ES7210_SDP_CFG2_REG12,
						0x07, i2c_clt1[cnt]);
			} else {
				es7210_write(ES7210_SDP_CFG2_REG12,
						0x03, i2c_clt1[cnt]);
			}
		}
		break;
	case ES7210_TDM_NLRCK_DSPB:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x93);
		for (cnt = 0; cnt < es7210->adc_dev; cnt++) {
			if (cnt == 0) {
				es7210_write(ES7210_SDP_CFG2_REG12,
						0x07, i2c_clt1[cnt]);
			} else {
				es7210_write(ES7210_SDP_CFG2_REG12,
						0x03, i2c_clt1[cnt]);
			}
		}
		break;
	case ES7210_TDM_NLRCK_I2S:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x80);
		for (cnt = 0; cnt < es7210->adc_dev; cnt++) {
			if (cnt == 0) {
				es7210_write(ES7210_SDP_CFG2_REG12,
						0x07, i2c_clt1[cnt]);
			} else {
				es7210_write(ES7210_SDP_CFG2_REG12,
						0x03, i2c_clt1[cnt]);
			}
		}
		break;
	case ES7210_TDM_NLRCK_LJ:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x81);
		for (cnt = 0; cnt < es7210->adc_dev; cnt++) {
			if (cnt == 0) {
				es7210_write(ES7210_SDP_CFG2_REG12,
						0x07, i2c_clt1[cnt]);
			} else {
				es7210_write(ES7210_SDP_CFG2_REG12,
						0x03, i2c_clt1[cnt]);
			}
		}
		break;
	case ES7210_NORMAL_I2S:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x80);
		es7210_multi_chips_write(ES7210_SDP_CFG2_REG12, 0x00);
		break;
	case ES7210_NORMAL_LJ:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x81);
		es7210_multi_chips_write(ES7210_SDP_CFG2_REG12, 0x00);
		break;
	case ES7210_NORMAL_DSPA:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x83);
		es7210_multi_chips_write(ES7210_SDP_CFG2_REG12, 0x00);
		break;
	case ES7210_NORMAL_DSPB:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x93);
		es7210_multi_chips_write(ES7210_SDP_CFG2_REG12, 0x00);
		break;
	default:
		es7210_multi_chips_write(ES7210_SDP_CFG1_REG11, 0x80);
		es7210_multi_chips_write(ES7210_SDP_CFG2_REG12, 0x00);
		break;

	}
	for (cnt = 0; cnt < sizeof(es7210_tdm_reg_common_cfg2) /
		sizeof(es7210_tdm_reg_common_cfg2[0]); cnt++) {
		es7210_multi_chips_write(
				es7210_tdm_reg_common_cfg2[cnt].reg_addr,
				es7210_tdm_reg_common_cfg2[cnt].reg_v);
	}
	switch (mode) {
	case ES7210_TDM_1LRCK_DSPA:
	case ES7210_TDM_1LRCK_DSPB:
	case ES7210_TDM_1LRCK_I2S:
	case ES7210_TDM_1LRCK_LJ:
		es7210_multi_chips_write(ES7210_MCLK_CTL_REG02, 0xc1);
		break;
	case ES7210_TDM_NLRCK_DSPA:
	case ES7210_TDM_NLRCK_DSPB:
	case ES7210_TDM_NLRCK_I2S:
	case ES7210_TDM_NLRCK_LJ:
		channel = es7210->channels / 2;
		channel &= 0x0f;
		channel = channel << 4;
		pr_info("ADC Analg channel register = 0x%2x\n", channel);
		es7210_multi_chips_write(ES7210_MODE_CFG_REG08, channel);
		es7210_multi_chips_write(ES7210_MCLK_CTL_REG02, 0xC1);
		break;
	default:
		es7210_multi_chips_write(ES7210_MCLK_CTL_REG02, 0xC1);
		break;
	}
	for (cnt = 0; cnt < sizeof(es7210_tdm_reg_common_cfg3) /
		 sizeof(es7210_tdm_reg_common_cfg3[0]); cnt++) {
		es7210_multi_chips_write(
				es7210_tdm_reg_common_cfg3[cnt].reg_addr,
				es7210_tdm_reg_common_cfg3[cnt].reg_v);
	}
	/*
	 * Mute All ADC
	 */
	es7210_multi_chips_update_bits(ES7210_ADC34_MUTE_REG14, 0x03, 0x03);
	es7210_multi_chips_update_bits(ES7210_ADC12_MUTE_REG15, 0x03, 0x03);
	/*
	 * Set Direct DB Gain
	 */
	es7210_multi_chips_update_bits(ES7210_ALC_COM_CFG2_REG1A, 0x03, 0x00);
	es7210_multi_chips_write(ES7210_ALC1_MAX_GAIN_REG1E, 0xBF);
	es7210_multi_chips_write(ES7210_ALC2_MAX_GAIN_REG1D, 0xBF);
	es7210_multi_chips_write(ES7210_ALC3_MAX_GAIN_REG1C, 0xBF);
	es7210_multi_chips_write(ES7210_ALC4_MAX_GAIN_REG1B, 0xBF);

	es7210_multi_chips_write(ES7210_RESET_CTL_REG00, 0x73);
	msleep(10);
	es7210_multi_chips_write(ES7210_RESET_CTL_REG00, 0x71);
	for (i = 0; i < es7210->adc_dev; i++) {
		if (es7210->mastermode == 1) {
			if(i == 0) {
				es7210_write(ES7210_RESET_CTL_REG00,
						0x01, i2c_clt1[i]);
			} else {
				es7210_write(ES7210_RESET_CTL_REG00,
						0x41, i2c_clt1[i]);
			}
		} else {
			es7210_write(ES7210_RESET_CTL_REG00,
					0x41, i2c_clt1[i]);
		}
	}
}


static int es7210_probe(struct snd_soc_component *component)
{
	struct es7210_priv *es7210 = snd_soc_component_get_drvdata(component);
	int index;
	/*
	component->control_data = devm_regmap_init_i2c(es7210->i2c,
						&es7210_regmap_config);
	ret = PTR_RET(component->control_data);

	if (ret < 0) {
		pr_err("Failed to set cache I/O: %d\n", ret);
		return ret;
	}
	*/
	pr_info("enter into %s!\n", __func__);

#if 0
	es7210->mclk = devm_clk_get(component->dev, "mclk");
	if (IS_ERR(es7210->mclk)) {
		dev_err(component->dev, "%s,unable to get mclk\n", __func__);
		return PTR_ERR(es7210->mclk);
	}
	if (!es7210->mclk)
		dev_err(component->dev, "%s, assuming static mclk\n", __func__);

	ret = clk_prepare_enable(es7210->mclk);
	if (ret) {
		dev_err(component->dev, "%s, unable to enable mclk\n", __func__);
		return ret;
	}
#endif

#if 0
	//reset gpio level = low, reset es7210
	es7210_set_gpio(0);
	msleep(20);
	es7210_set_gpio(1);
#endif

	/* initialize es7210 */
	es7210_tdm_init_codec(es7210->tdm_mode);

	/*
	* set clock ratio according to es7210_config.h
	* Correct configuration must be set in es7210_config.h.
	* Default TDM-DSPA, 32bit, 4 channel, mclk/lrck rato =256, NFS Disabled
	*/
	switch (ES7210_WORK_MODE) {
	case ES7210_TDM_NLRCK_I2S:
	case ES7210_TDM_NLRCK_LJ:
	case ES7210_TDM_NLRCK_DSPA:
	case ES7210_TDM_NLRCK_DSPB:
		for (index = 0; index < sizeof(es7210_nfs_ratio_cfg) /
			 sizeof(es7210_nfs_ratio_cfg[0]); index++) {
			if ((es7210->mclk_lrck_ratio == es7210_nfs_ratio_cfg[index].ratio) &&
					(es7210_nfs_ratio_cfg[index].channels == es7210->channels))
				break;
		}
		es7210_multi_chips_write(0x02,
				es7210_nfs_ratio_cfg[index].reg02_v);
		es7210_multi_chips_write(0x06,
				es7210_nfs_ratio_cfg[index].reg06_v);
		break;
	default:
		for (index = 0; index < sizeof(es7210_1fs_ratio_cfg) /
			 sizeof(es7210_1fs_ratio_cfg[0]); index++) {
			if (es7210->mclk_lrck_ratio == es7210_1fs_ratio_cfg[index].ratio)
				break;
		}
		es7210_multi_chips_write(0x02,
				es7210_1fs_ratio_cfg[index].reg02_v);
		es7210_multi_chips_write(0x06,
				es7210_1fs_ratio_cfg[index].reg06_v);
		break;

	}

	/* Disable or Enable PDM DMIC interface */
	if (es7210->channels > 0) {
		if (es7210->pdm_dmic_1_2_enable == ENABLE) {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
					0x40, 0x40, i2c_clt1[0]);
		} else {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
					0x40, 0x00, i2c_clt1[0]);
		}
		if (es7210->pdm_dmic_3_4_enable == ENABLE) {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
					0x80, 0x80, i2c_clt1[0]);
		} else {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
					0x80, 0x00, i2c_clt1[0]);
		}
		if (es7210->pdm_dmic_1_2_enable == ENABLE ||
			es7210->pdm_dmic_3_4_enable == ENABLE) {
			es7210_update_bits(ES7210_MODE_CTL_REG0E,
					0x01, 0x01, i2c_clt1[0]);
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
					0x24, 0x24, i2c_clt1[0]);
		}
	}

	if (es7210->channels > 4) {
		if (es7210->pdm_dmic_5_6_enable == ENABLE) {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x40, 0x40, i2c_clt1[1]);
		} else {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x40, 0x00, i2c_clt1[1]);
		}

		if (es7210->pdm_dmic_7_8_enable == ENABLE) {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x80, 0x80, i2c_clt1[1]);
		} else {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x80, 0x00, i2c_clt1[1]);
		}
		if (es7210->pdm_dmic_1_2_enable == ENABLE ||
				es7210->pdm_dmic_3_4_enable == ENABLE) {
			es7210_update_bits(ES7210_MODE_CTL_REG0E,
				0x01, 0x01, i2c_clt1[1]);
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x24, 0x24, i2c_clt1[1]);
		}
	}

	if (es7210->channels > 8) {
		if (es7210->pdm_dmic_9_10_enable == ENABLE) {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x40, 0x40, i2c_clt1[2]);
		} else {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x40, 0x00, i2c_clt1[2]);
		}
		if (es7210->pdm_dmic_11_12_enable == ENABLE) {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x80, 0x80, i2c_clt1[2]);
		} else {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x80, 0x00, i2c_clt1[2]);
		}
		if (es7210->pdm_dmic_1_2_enable == ENABLE ||
				es7210->pdm_dmic_3_4_enable == ENABLE) {
			es7210_update_bits(ES7210_MODE_CTL_REG0E,
				0x01, 0x01, i2c_clt1[2]);
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x24, 0x24, i2c_clt1[2]);
		}
	}

	if (es7210->channels > 12) {
		if (es7210->pdm_dmic_13_14_enable == ENABLE) {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x40, 0x40, i2c_clt1[3]);
		} else {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x40, 0x00, i2c_clt1[3]);
		}
		if (es7210->pdm_dmic_15_16_enable == ENABLE) {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x80, 0x80, i2c_clt1[3]);
		} else {
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x80, 0x00, i2c_clt1[3]);
		}
		if (es7210->pdm_dmic_1_2_enable == ENABLE ||
				es7210->pdm_dmic_3_4_enable == ENABLE) {
			es7210_update_bits(ES7210_MODE_CTL_REG0E,
				0x01, 0x01, i2c_clt1[4]);
			es7210_update_bits(ES7210_DMIC_CTL_REG10,
				0x24, 0x24, i2c_clt1[4]);
		}
	}

	if (es7210->sclkinv == true) {
		es7210_multi_chips_update_bits(ES7210_MODE_CFG_REG08,
				0x08, 0x08);
	}

	/* set first es7210 PGA GAIN */
	es7210_multi_chips_write(ES7210_MIC1_GAIN_REG43, ES7210_MIC_GAIN);
	es7210_multi_chips_write(ES7210_MIC2_GAIN_REG44, ES7210_MIC_GAIN);
	es7210_multi_chips_write(ES7210_MIC3_GAIN_REG45, ES7210_MIC_GAIN);
	es7210_multi_chips_write(ES7210_MIC4_GAIN_REG46, ES7210_MIC_GAIN);
	es7210_multi_chips_write(ES7210_ADC34_MUTE_REG14, 0x3c);
	es7210_multi_chips_write(ES7210_ADC12_MUTE_REG15, 0x3c);
	es7210_multi_chips_write(ES7210_ALC_COM_CFG1_REG17, 0x10);

	/* Schedule a delay work-quenue to avoid pop noise */
	INIT_DELAYED_WORK(&es7210->pcm_pop_work, pcm_pop_work_events);

	return 0;
}

static void es7210_remove(struct snd_soc_component *component)
{
	es7210_set_bias_level(component, SND_SOC_BIAS_OFF);
}

static struct snd_soc_component_driver soc_component_dev_es7210 = {
	.probe = es7210_probe,
	.remove = es7210_remove,
	.suspend = es7210_suspend,
	.resume = es7210_resume,
	.set_bias_level = es7210_set_bias_level,
	.idle_bias_on = 1,

	.controls = es7210_snd_controls,
	.num_controls = ARRAY_SIZE(es7210_snd_controls),
	.dapm_widgets = es7210_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(es7210_dapm_widgets),
	.dapm_routes = es7210_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(es7210_dapm_routes),
};

static ssize_t es7210_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int val = 0, flag = 0;
	u8 i = 0, reg, num, value_w, value_r;
	struct es7210_priv *es7210 = dev_get_drvdata(dev);

	val = simple_strtol(buf, NULL, 16);
	flag = (val >> 16) & 0xFF;

	if (flag) {
		reg = (val >> 8) & 0xFF;
		value_w = val & 0xFF;
		pr_info("\nWrite start REG:0x%02x,val:0x%02x, count:0x%02x\n",
			   reg, value_w, flag);
		while (flag--) {
			es7210_write(reg, value_w, es7210->i2c);
			pr_info("Write 0x%02x to REG:0x%02x\n", value_w, reg);
			reg++;
		}
	} else {
		reg = (val >> 8) & 0xFF;
		num = val & 0xff;
		pr_info("\nRead: start REG:0x%02x,count:0x%02x\n", reg, num);
		do {
			value_r = 0;
			es7210_read(reg, &value_r, es7210->i2c);
			pr_info("REG[0x%02x]: 0x%02x;  ", reg, value_r);
			reg++;
			i++;
			if ((i == num) || (i % 4 == 0))
				pr_info("\n");
		} while (i < num);
	}

	return count;
}

static ssize_t es7210_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	pr_info("echo flag|reg|val > es7210\n");
	pr_info("write star address=0x90,value=0x3c");
	pr_info("count=4:echo 4903c >es7210\n");
	return 0;
}

static DEVICE_ATTR(es7210, 0644, es7210_show, es7210_store);

static struct attribute *es7210_debug_attrs[] = {
	&dev_attr_es7210.attr,
	NULL,
};

static struct attribute_group es7210_debug_attr_group = {
	.name = "es7210_debug",
	.attrs = es7210_debug_attrs,
};

/*
 * If the i2c layer weren't so broken, we could pass this kind of data
 * around
 */
static int es7210_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *i2c_id)
{
	struct es7210_priv *es7210;
	int ret;
	unsigned int val;

	pr_info("enter into %s!\n", __func__);

	es7210 = devm_kzalloc(&i2c->dev,
				  sizeof(struct es7210_priv), GFP_KERNEL);
	if (es7210 == NULL)
		return -ENOMEM;
	es7210->i2c = i2c;
	es7210->sclkinv = false;
	es7210->tdm_mode = ES7210_WORK_MODE;
	es7210->mclk_lrck_ratio = RATIO_MCLK_LRCK;
	es7210->mastermode = 0;
	es7210->pdm_dmic_1_2_enable = DISABLE;
	es7210->pdm_dmic_3_4_enable = DISABLE;
	es7210->pdm_dmic_5_6_enable = DISABLE;
	es7210->pdm_dmic_7_8_enable = DISABLE;
	es7210->pdm_dmic_9_10_enable = DISABLE;
	es7210->pdm_dmic_11_12_enable = DISABLE;
	es7210->pdm_dmic_13_14_enable = DISABLE;
	es7210->pdm_dmic_15_16_enable = DISABLE;

	ret = of_property_read_u32(i2c->dev.of_node, "channels", &es7210->channels);
	if (ret) {
		dev_err(&i2c->dev, "read channels error\n");
		return ret;
	}
	ret = of_property_read_u32(i2c->dev.of_node, "adc_dev_num", &es7210->adc_dev);
	if (ret) {
		dev_err(&i2c->dev, "read adc_dev error\n");
		return ret;
	}
	es7210->regmap = devm_regmap_init_i2c(i2c, &es7210_regmap_config);
	if (IS_ERR(es7210->regmap)) {
		ret = PTR_ERR(es7210->regmap);
		dev_err(&i2c->dev, "regmap_init() failed: %d\n", ret);
		return ret;
	}

	ret = regmap_read(es7210->regmap, ES7210_CHP_ID1_REG3D, &val);
	if (ret < 0) {
		dev_err(&i2c->dev, "failed to read i2c at addr %X\n",
			   i2c->addr);
		return ret;
	}
	if (val != 0x72) {
		dev_err(&i2c->dev, "device at addr %X is not an es7210L\n",
			   i2c->addr);
		return -ENODEV;
	}
	ret = regmap_read(es7210->regmap, ES7210_CHP_ID0_REG3E, &val);
	if (ret < 0) {
		dev_err(&i2c->dev, "failed to read i2c at addr %X\n",
			   i2c->addr);
		return ret;
	}
	if (val != 0x10) {
		dev_err(&i2c->dev, "device at addr %X is not an es7210L\n",
			   i2c->addr);
		return -ENODEV;
	}

	ret = regmap_read(es7210->regmap, ES7210_CHP_VER_REG3F, &val);
	pr_info("%s, ES7210L Version = %d!\n", __func__, val);

	es7210_private = es7210;
#if 0
	es7210->reset_ctl_gpio = of_get_named_gpio_flags(i2c->dev.of_node,
							   "reset_ctl_gpio", 0,
							   &flags);
	if (es7210->reset_ctl_gpio < 0) {
		dev_info(&i2c->dev, "Can not read property reset_ctl_gpio\n");
		es7210->reset_ctl_gpio = INVALID_GPIO;
	} else {
		ret = devm_gpio_request_one(&i2c->dev, es7210->reset_ctl_gpio,
						GPIOF_OUT_INIT_LOW, NULL);
		if (ret != 0) {
			dev_err(&i2c->dev, "Failed to request reset_ctl_gpio\n");
			return ret;
		}
		es7210_set_gpio(0);
	}
#endif

	dev_set_drvdata(&i2c->dev, es7210);
	if (i2c_id->driver_data < es7210->adc_dev) {
		i2c_clt1[i2c_id->driver_data] = i2c;
		ret = snd_soc_register_component(&i2c->dev, &soc_component_dev_es7210,
						 es7210_dai[i2c_id->driver_data],
						 1);
		if (ret < 0) {
			kfree(es7210);
			return ret;
		}
	}
	ret = sysfs_create_group(&i2c->dev.kobj, &es7210_debug_attr_group);
	if (ret)
		pr_info("failed to create attr group\n");
	return ret;
}

static void es7210_i2c_remove(struct i2c_client *i2c)
{
	sysfs_remove_group(&i2c->dev.kobj, &es7210_debug_attr_group);
	snd_soc_unregister_component(&i2c->dev);
}

#if !ES7210_MATCH_DTS_EN
static int es7210_i2c_detect(struct i2c_client *client,
				 struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if (adapter->nr == ES7210_I2C_BUS_NUM) {
		if (client->addr == 0x40) {
			strlcpy(info->type, "MicArray_0", I2C_NAME_SIZE);
			return 0;
		} else if (client->addr == 0x43) {
			strlcpy(info->type, "MicArray_1", I2C_NAME_SIZE);
			return 0;
		} else if (client->addr == 0x42) {
			strlcpy(info->type, "MicArray_2", I2C_NAME_SIZE);
			return 0;
		} else if (client->addr == 0x41) {
			strlcpy(info->type, "MicArray_3", I2C_NAME_SIZE);
			return 0;
		}
	}

	return -ENODEV;
}

static const unsigned short es7210_i2c_addr[] = {
	0x40,
	0x42,
	0x43,
	0x41,
	I2C_CLIENT_END,
};
#endif

#if ES7210_MATCH_DTS_EN
/*
 * device tree source or i2c_board_info both use to
 * transfer hardware information to linux kernel,
 * use one of them wil be OK
 */
static const struct of_device_id es7210_dt_ids[] = {
	{.compatible = "MicArray_0",},
	{.compatible = "MicArray_1",},
	{.compatible = "MicArray_2",},
	{.compatible = "MicArray_3",},
};
#endif

static const struct i2c_device_id es7210_i2c_id[] = {
	{"MicArray_0", 0},
	{"MicArray_2", 1},
	{"MicArray_1", 2},
	{"MicArray_3", 3},
	{}
};

MODULE_DEVICE_TABLE(i2c, es7210_i2c_id);

static struct i2c_driver es7210_i2c_driver = {
	.driver = {
		   .name = "es7210",
		   .owner = THIS_MODULE,
#if ES7210_MATCH_DTS_EN
		   .of_match_table = es7210_dt_ids,
#endif
		   },
	.probe = es7210_i2c_probe,
	.remove = es7210_i2c_remove,
	.class = I2C_CLASS_HWMON,
	.id_table = es7210_i2c_id,
#if !ES7210_MATCH_DTS_EN
	.address_list = es7210_i2c_addr,
	.detect = es7210_i2c_detect,
#endif
};

static int __init es7210_modinit(void)
{
	int ret;
#if !ES7210_MATCH_DTS_EN
	int i;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
#endif
	pr_info("%s enter\n", __func__);

#if !ES7210_MATCH_DTS_EN
/*
* Notes:
* if the device has been declared in DTS tree,
* here don't need to create new i2c device with i2c_board_info.
*/
	adapter = i2c_get_adapter(ES7210_I2C_BUS_NUM);
	if (!adapter) {
		pr_info("i2c_get_adapter() fail!\n");
		return -ENODEV;
	}
	pr_info("%s() begin0000", __func__);

	for (i = 0; i < ADC_DEV_MAXNUM; i++) {
		client = i2c_new_device(adapter, &es7210_i2c_board_info[i]);
		pr_info("%s() i2c_new_device\n", __func__);
		if (!client)
			return -ENODEV;
	}
	i2c_put_adapter(adapter);
#endif
	ret = i2c_add_driver(&es7210_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register es7210 i2c driver : %d \n", ret);
	return ret;
}

/*
* here, late_initcall is used in RK3308-firefly SDK, becase ES7210 driver
* needs to be installed after others codec driver in this SDK.
* if your SDK doesn't have this request, module_init is preferred
*/
//late_initcall(es7210_modinit);
module_init(es7210_modinit);
static void __exit es7210_exit(void)
{
	i2c_del_driver(&es7210_i2c_driver);
}

module_exit(es7210_exit);
MODULE_DESCRIPTION("ASoC ES7210 audio adc driver");
MODULE_AUTHOR("David Yang <yangxiaohua@everest-semi.com>");
MODULE_LICENSE("GPL v2");

