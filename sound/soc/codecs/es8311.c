// SPDX-License-Identifier: GPL-2.0-only
/*
 * es8311.c  --  ES8311/ES8312 ALSA SoC Audio Codec
 *
 * Copyright (C) 2018 Everest Semiconductor Co., Ltd
 *
 * Authors:  David Yang(yangxiaohua@everest-semi.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>

#include "es8311.h"
#ifndef CONFIG_OF
#define CONFIG_OF
#endif

/* component private data */

struct	es8311_private {
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct clk *mclk;
	unsigned int mclk_rate;
	int mastermode;
	bool sclkinv;
	bool mclkinv;
	bool dmic_enable;
	unsigned int mclk_src;
	enum snd_soc_bias_level bias_level;
};

struct es8311_private *es8311_data;
struct snd_soc_component *es8311_component;

static bool es8311_volatile_register(struct device *dev,
			unsigned int reg)
{
	if ((reg  <= 0xff))
		return true;
	else
		return false;
	}

static bool es8311_readable_register(struct device *dev,
			unsigned int reg)
{
	if ((reg  <= 0xff))
		return true;
	else
		return false;
	}

static bool es8311_writable_register(struct device *dev,
			unsigned int reg)
{
	if ((reg  <= 0xff))
		return true;
	else
		return false;
}

static const DECLARE_TLV_DB_SCALE(vdac_tlv, -9550, 50, 1);
static const DECLARE_TLV_DB_SCALE(vadc_tlv, -9550, 50, 1);
static const DECLARE_TLV_DB_SCALE(mic_pga_tlv, 0, 300, 1);
static const DECLARE_TLV_DB_SCALE(adc_scale_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_SCALE(alc_winsize_tlv, 0, 25, 0);
static const DECLARE_TLV_DB_SCALE(alc_maxlevel_tlv, -3600, 200, 0);
static const DECLARE_TLV_DB_SCALE(alc_minlevel_tlv, -3600, 200, 0);
static const DECLARE_TLV_DB_SCALE(alc_noisegate_tlv, -9600, 600, 0);
static const DECLARE_TLV_DB_SCALE(alc_noisegate_winsize_tlv, 2048, 2048, 0);
static const DECLARE_TLV_DB_SCALE(alc_automute_gain_tlv, 0, -400, 0);
static const DECLARE_TLV_DB_SCALE(adc_ramprate_tlv, 0, 25, 0);

static const char * const dmic_type_txt[] = {
	"dmic at high level",
	"dmic at low level"
};
static const struct soc_enum dmic_type =
SOC_ENUM_SINGLE(ES8311_ADC_REG15, 0, 2, dmic_type_txt);

static const char * const automute_type_txt[] = {
	"automute disabled",
	"automute enable"
};
static const struct soc_enum alc_automute_type =
SOC_ENUM_SINGLE(ES8311_ADC_REG18, 6, 2, automute_type_txt);

static const char * const dacdsm_mute_type_txt[] = {
	"mute to 8",
	"mute to 7/9"
};
static const struct soc_enum dacdsm_mute_type =
SOC_ENUM_SINGLE(ES8311_DAC_REG31, 7, 2, dacdsm_mute_type_txt);

static const char * const aec_type_txt[] = {
	"adc left, adc right",
	"adc left, null right",
	"null left, adc right",
	"null left, null right",
	"dac left, adc right",
	"adc left, dac right",
	"dac left, dac right",
	"N/A"
};
static const struct soc_enum aec_type =
SOC_ENUM_SINGLE(ES8311_GPIO_REG44, 4, 7, aec_type_txt);

static const char * const adc2dac_sel_txt[] = {
	"disable",
	"adc data to dac",
};
static const struct soc_enum adc2dac_sel =
SOC_ENUM_SINGLE(ES8311_GPIO_REG44, 7, 2, adc2dac_sel_txt);

static const char * const mclk_sel_txt[] = {
	"from mclk pin",
	"from bclk",
};
static const struct soc_enum mclk_src =
SOC_ENUM_SINGLE(ES8311_CLK_MANAGER_REG01, 7, 2, mclk_sel_txt);

/*
 * es8311 Controls
 */
static const struct snd_kcontrol_new es8311_snd_controls[] = {
	SOC_SINGLE_TLV("MIC PGA GAIN", ES8311_SYSTEM_REG14,
			0, 10, 0, mic_pga_tlv),
	SOC_SINGLE_TLV("ADC SCALE", ES8311_ADC_REG16,
			0, 7, 0, adc_scale_tlv),
	SOC_ENUM("DMIC TYPE", dmic_type),
	SOC_SINGLE_TLV("ADC RAMP RATE", ES8311_ADC_REG15,
			4, 15, 0, adc_ramprate_tlv),
	SOC_SINGLE("ADC SDP MUTE", ES8311_SDPOUT_REG0A, 6, 1, 0),
	SOC_SINGLE("ADC INVERTED", ES8311_ADC_REG16, 4, 1, 0),
	SOC_SINGLE("ADC SYNC", ES8311_ADC_REG16, 5, 1, 1),
	SOC_SINGLE("ADC RAM CLR", ES8311_ADC_REG16, 3, 1, 0),
	SOC_SINGLE_TLV("ADC VOLUME", ES8311_ADC_REG17,
			0, 255, 0, vadc_tlv),
	SOC_SINGLE("ALC ENABLE", ES8311_ADC_REG18, 7, 1, 0),
	SOC_ENUM("ALC AUTOMUTE TYPE", alc_automute_type),
	SOC_SINGLE_TLV("ALC WIN SIZE", ES8311_ADC_REG18,
			0, 15, 0, alc_winsize_tlv),
	SOC_SINGLE_TLV("ALC MAX LEVEL", ES8311_ADC_REG19,
			4, 15, 0, alc_maxlevel_tlv),
	SOC_SINGLE_TLV("ALC MIN LEVEL", ES8311_ADC_REG19,
			0, 15, 0, alc_minlevel_tlv),
	SOC_SINGLE_TLV("ALC AUTOMUTE WINSIZE", ES8311_ADC_REG1A,
			4, 15, 0, alc_noisegate_winsize_tlv),
	SOC_SINGLE_TLV("ALC AUTOMUTE GATE THRESHOLD", ES8311_ADC_REG1A,
			0, 15, 0, alc_noisegate_tlv),
	SOC_SINGLE_TLV("ALC AUTOMUTE VOLUME", ES8311_ADC_REG1B,
			5, 7, 0, alc_automute_gain_tlv),
	SOC_SINGLE("ADC FS MODE", ES8311_CLK_MANAGER_REG03, 6, 1, 0),
	SOC_SINGLE("DAC SDP MUTE", ES8311_SDPIN_REG09, 6, 1, 0),
	SOC_SINGLE("DAC DEM  MUTE", ES8311_DAC_REG31, 5, 1, 0),
	SOC_SINGLE("DAC INVERT", ES8311_DAC_REG31, 4, 1, 0),
	SOC_SINGLE("DAC RAM CLR", ES8311_DAC_REG31, 3, 1, 0),
	SOC_ENUM("DAC DSM MUTE", dacdsm_mute_type),
	SOC_SINGLE("DAC OFFSET", ES8311_DAC_REG33, 0, 255, 0),
	SOC_SINGLE_TLV("DAC VOLUME", ES8311_DAC_REG32,
			0, 255, 0, vdac_tlv),
	SOC_SINGLE("DRC ENABLE", ES8311_DAC_REG34, 7, 1, 0),
	SOC_SINGLE_TLV("DRC WIN SIZE",	ES8311_DAC_REG34,
			0, 15, 0, alc_winsize_tlv),
	SOC_SINGLE_TLV("DRC MAX LEVEL",	ES8311_DAC_REG35,
			4, 15, 0, alc_maxlevel_tlv),
	SOC_SINGLE_TLV("DRC MIN LEVEL",	ES8311_DAC_REG35,
			0, 15, 0, alc_minlevel_tlv),
	SOC_SINGLE_TLV("DAC RAMP RATE",	ES8311_DAC_REG37,
			4, 15, 0, adc_ramprate_tlv),
	SOC_ENUM("AEC MODE", aec_type),
	SOC_ENUM("ADC DATA TO DAC TEST MODE", adc2dac_sel),
	SOC_SINGLE("MCLK INVERT", ES8311_CLK_MANAGER_REG01, 6, 1, 0),
	SOC_SINGLE("BCLK INVERT", ES8311_CLK_MANAGER_REG06, 5, 1, 0),
	SOC_ENUM("MCLK SOURCE", mclk_src),
};

/*
 * DAPM Controls
 */
static const char * const es8311_dmic_mux_txt[] = {
	"DMIC DISABLE",
	"DMIC ENABLE"
};
static const unsigned int es8311_dmic_mux_values[] = {
	0, 1
};
static const struct soc_enum es8311_dmic_mux_enum =
	SOC_VALUE_ENUM_SINGLE(ES8311_SYSTEM_REG14, 6, 1,
			ARRAY_SIZE(es8311_dmic_mux_txt),
			es8311_dmic_mux_txt,
			es8311_dmic_mux_values);
static const struct snd_kcontrol_new es8311_dmic_mux_controls =
	SOC_DAPM_ENUM("DMIC ROUTE", es8311_dmic_mux_enum);
static const char * const es8311_adc_sdp_mux_txt[] = {
	"FROM EQUALIZER",
	"FROM ADC OUT",
};
static const unsigned int es8311_adc_sdp_mux_values[] = {
	0, 1
};
static const struct soc_enum es8311_adc_sdp_mux_enum =
	SOC_VALUE_ENUM_SINGLE(ES8311_ADC_REG1C, 6, 1,
			ARRAY_SIZE(es8311_adc_sdp_mux_txt),
			es8311_adc_sdp_mux_txt,
			es8311_adc_sdp_mux_values);
static const struct snd_kcontrol_new es8311_adc_sdp_mux_controls =
	SOC_DAPM_ENUM("ADC SDP ROUTE", es8311_adc_sdp_mux_enum);

/*
 * DAC data  soure
 */
static const char * const es8311_dac_data_mux_txt[] = {
	"SELECT SDP LEFT DATA",
	"SELECT SDP RIGHT DATA",
};
static const unsigned int  es8311_dac_data_mux_values[] = {
	0, 1
};
static const struct soc_enum  es8311_dac_data_mux_enum =
	SOC_VALUE_ENUM_SINGLE(ES8311_SDPIN_REG09, 7, 1,
			ARRAY_SIZE(es8311_dac_data_mux_txt),
			es8311_dac_data_mux_txt,
			es8311_dac_data_mux_values);
static const struct snd_kcontrol_new  es8311_dac_data_mux_controls =
	SOC_DAPM_ENUM("DAC SDP ROUTE", es8311_dac_data_mux_enum);

static const struct snd_soc_dapm_widget es8311_dapm_widgets[] = {
	/* Input*/
	SND_SOC_DAPM_INPUT("DMIC"),
	SND_SOC_DAPM_INPUT("AMIC"),

	SND_SOC_DAPM_PGA("INPUT PGA", ES8311_SYSTEM_REG0E,
			6, 1, NULL, 0),
	/* ADCs */
	SND_SOC_DAPM_ADC("MONO ADC", NULL, ES8311_SYSTEM_REG0E, 5, 1),
	/* Dmic MUX */
	SND_SOC_DAPM_MUX("DMIC MUX", SND_SOC_NOPM, 0, 0,
			&es8311_dmic_mux_controls),
	/* sdp MUX */
	SND_SOC_DAPM_MUX("SDP OUT MUX", SND_SOC_NOPM, 0, 0,
			&es8311_adc_sdp_mux_controls),
	/* Digital Interface */
	SND_SOC_DAPM_AIF_OUT("I2S OUT", "I2S1 Capture",  1,
			SND_SOC_NOPM, 0, 0),
	/* Render path	*/
	SND_SOC_DAPM_AIF_IN("I2S IN", "I2S1 Playback", 0,
			SND_SOC_NOPM, 0, 0),
	/*DACs SDP DATA SRC MUX */
	SND_SOC_DAPM_MUX("DAC SDP SRC MUX", ES8311_SDPIN_REG09, 7, 2,
			&es8311_dac_data_mux_controls),
	SND_SOC_DAPM_DAC("MONO DAC", NULL, SND_SOC_NOPM, 0, 0),
	/* Output Lines */
	SND_SOC_DAPM_OUTPUT("DIFFERENTIAL OUT"),

};


static const struct snd_soc_dapm_route es8311_dapm_routes[] = {
	/* record route map */
	{"INPUT PGA", NULL, "AMIC"},
	{"MONO ADC", NULL, "INPUT PGA"},
	{"DMIC MUX", "DMIC DISABLE", "MONO ADC"},
	{"DMIC MUX", "DMIC ENABLE", "DMIC"},
	{"SDP OUT MUX", "FROM ADC OUT", "DMIC MUX"},
	{"SDP OUT MUX", "FROM EQUALIZER", "DMIC MUX"},
	{"I2S OUT", NULL, "SDP OUT MUX"},
	/* playback route map */
	{"DAC SDP SRC MUX", "SELECT SDP LEFT DATA", "I2S IN"},
	{"DAC SDP SRC MUX", "SELECT SDP RIGHT DATA", "I2S IN"},
	{"MONO DAC", NULL, "DAC SDP SRC MUX"},
	{"DIFFERENTIAL OUT", NULL, "MONO DAC"},
};

struct _coeff_div {
	u32 mclk;	/* mclk frequency */
	u32 rate;	/* sample rate */
	u8 prediv;	/* the pre divider with range from 1 to 8 */
	u8 premulti;	/* the pre multiplier with x1, x2, x4 and x8 selection */
	u8 adcdiv;	/* adcclk divider */
	u8 dacdiv;	/* dacclk divider */
	u8 fsmode;	/* double speed or single speed, =0, ss, =1, ds */
	u8 lrck_h;	/* adclrck divider and daclrck divider */
	u8 lrck_l;
	u8 bclkdiv;	/* sclk divider */
	u8 adcosr;	/* adc osr */
	u8 dacosr;	/* dac osr */
	u8 adcscale;
};


/* component hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	//mclk	   rate   prediv  mult	adcdiv dacdiv fsmode lrch  lrcl  bckdiv osr  adcscale
	/* 8k */
	{12288000, 8000, 0x06, 0x01, 0x01, 0x01, 0x00, 0x05, 0xff, 0x04, 0x10, 0x20, 0x04},	//1536
	{18432000, 8000, 0x03, 0x01, 0x03, 0x03, 0x00, 0x08, 0xff, 0x18, 0x10, 0x20, 0x04},	//2304
	{16384000, 8000, 0x08, 0x01, 0x01, 0x01, 0x00, 0x07, 0xff, 0x04, 0x10, 0x20, 0x04},	//2048
	{8192000, 8000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x03, 0xff, 0x04, 0x10, 0x20, 0x04},	//1024
	{6144000, 8000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x02, 0xff, 0x04, 0x10, 0x20, 0x04},	//768
	{4096000, 8000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0x04, 0x10, 0x20, 0x04},	//512
	{3072000, 8000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x01, 0x7f, 0x04, 0x10, 0x20, 0x04},	//384
	{2048000, 8000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20, 0x04},	//256
	{1536000, 8000, 0x01, 0x04, 0x03, 0x03, 0x00, 0x00, 0xbf, 0x04, 0x10, 0x20, 0x04},	//192
	{1024000, 8000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x20, 0x04},	//128
	{12000000, 8000, 0x05, 0x04, 0x03, 0x03, 0x00, 0x05, 0xDB, 0x04, 0x19, 0x19, 0x01},	//1500
	/* 11.025k */
	{11289600, 11025, 0x04, 0x01, 0x01, 0x01, 0x00, 0x03, 0xff, 0x04, 0x10, 0x20, 0x04},	//1024
	{5644800, 11025, 0x02, 0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0x04, 0x10, 0x20, 0x04},	//512
	{2822400, 11025, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20, 0x04},	//256
	{1411200, 11025, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x20, 0x04},	//128

	/* 12k */
	{12288000, 12000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x03, 0xff, 0x04, 0x10, 0x20, 0x04},	//1024
	{6144000, 12000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0x04, 0x10, 0x20, 0x04},	//512
	{3072000, 12000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20, 0x04},	//256
	{1536000, 12000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x20, 0x04},	//128

	/* 16k */
	//{24576000, 16000, 0x06, 0x01, 0x01, 0x01, 0x00, 0x05, 0xff, 0x04, 0x10, 0x20, 0x04},	//1536
	{12288000, 16000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x02, 0xff, 0x04, 0x10, 0x20, 0x04},	//768
	{18432000, 16000, 0x03, 0x02, 0x03, 0x03, 0x00, 0x04, 0x7f, 0x0c, 0x10, 0x20, 0x04},	//1152
	{16384000, 16000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x03, 0xff, 0x04, 0x10, 0x20, 0x04},	//1024
	{8192000, 16000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0x04, 0x10, 0x20, 0x04},	//512
	{6144000, 16000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x01, 0x7f, 0x04, 0x10, 0x20, 0x04},	//384
	{4096000, 16000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20, 0x04},	//256
	{3072000, 16000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xbf, 0x04, 0x10, 0x20, 0x04},	//192
	{2048000, 16000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x20, 0x04},	//128
	{1536000, 16000, 0x01, 0x08, 0x03, 0x03, 0x00, 0x00, 0x5f, 0x02, 0x10, 0x20, 0x04},	//96
	{1024000, 16000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x02, 0x10, 0x20, 0x04},	//64
	{12000000, 16000, 0x05, 0x08, 0x03, 0x03, 0x00, 0x02, 0xED, 0x04, 0x19, 0x19, 0x01},	//750

	/* 22.05k */
	{11289600, 22050, 0x02, 0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0x04, 0x10, 0x20, 0x04},	//512
	{5644800, 22050, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20, 0x04},	//256
	{2822400, 22050, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x20, 0x04},	//128
	{1411200, 22050, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x02, 0x10, 0x20, 0x04},	//64

	/* 24k */
	//{24576000, 24000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x03, 0xff, 0x04, 0x10, 0x20, 0x04},	//1024
	{12288000, 24000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0x04, 0x10, 0x20, 0x04},	//512
	{18432000, 24000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x02, 0xff, 0x04, 0x10, 0x20, 0x04},	//768
	{6144000, 24000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20, 0x04},	//256
	{3072000, 24000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x20, 0x04},	//128
	{1536000, 24000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x02, 0x10, 0x20, 0x04},	//64
	{12000000, 24000, 0x05, 0x04, 0x01, 0x01, 0x00, 0x01, 0xF3, 0x04, 0x19, 0x19, 0x01},	//500

	/* 32k */
	//{24576000, 32000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x02, 0xff, 0x04, 0x10, 0x10, 0x04},	//768
	{12288000, 32000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x01, 0x7f, 0x04, 0x10, 0x10, 0x04},	//384
	{18432000, 32000, 0x03, 0x04, 0x03, 0x03, 0x00, 0x02, 0x3f, 0x0c, 0x10, 0x10, 0x04},	//576
	{16384000, 32000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0x04, 0x10, 0x10, 0x04},	//512
	{8192000, 32000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10, 0x04},	//256
	{6144000, 32000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xbf, 0x04, 0x10, 0x10, 0x04},	//192
	{4096000, 32000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x10, 0x04},	//128
	{3072000, 32000, 0x03, 0x08, 0x01, 0x01, 0x00, 0x00, 0x5f, 0x02, 0x10, 0x10, 0x04},	//96
	{2048000, 32000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x02, 0x10, 0x10, 0x04},	//64
	{1536000, 32000, 0x01, 0x08, 0x03, 0x03, 0x01, 0x00, 0x2f, 0x02, 0x10, 0x10, 0x04},	//48
	{1024000, 32000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0x1f, 0x02, 0x10, 0x10, 0x04},	//32

	/* 44.1k */
	//{22579200, 44100, 0x02, 0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0x04, 0x10, 0x10, 0x04},	  //512
	{11289600, 44100, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10, 0x04},	//256
	{5644800, 44100, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x10, 0x04},	//128
	{2822400, 44100, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x02, 0x10, 0x10, 0x04},	//64
	{1411200, 44100, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0x1f, 0x02, 0x10, 0x10, 0x04},	//32

	/* 48k */
	{24576000, 48000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0x04, 0x10, 0x10, 0x04},	//512
	{12288000, 48000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10, 0x04},	//256
	{18432000, 48000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x01, 0x7f, 0x04, 0x10, 0x10, 0x04},	//384
	{6144000, 48000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x10, 0x04},	//128
	{3072000, 48000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x02, 0x10, 0x10, 0x04},	//64
	{1536000, 48000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0x1f, 0x02, 0x10, 0x10, 0x04},	//32
	{12000000, 48000, 0x05, 0x08, 0x01, 0x01, 0x00, 0x00, 0xF9, 0x04, 0x19, 0x19, 0x01},	//250

	/* 64k */
	{12288000, 64000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xbf, 0x04, 0x10, 0x10, 0x04},	//192
	{18432000, 64000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x1f, 0x06, 0x12, 0x12, 0x03},	//288
	{16384000, 64000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10, 0x04},	//256
	{8192000, 64000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x10, 0x04},	//128
	{6144000, 64000, 0x03, 0x08, 0x01, 0x01, 0x01, 0x00, 0x5f, 0x02, 0x10, 0x10, 0x04},	//96
	{4096000, 64000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x02, 0x10, 0x10, 0x04},	//64
	{3072000, 64000, 0x03, 0x08, 0x01, 0x01, 0x01, 0x00, 0x2f, 0x02, 0x10, 0x10, 0x04},	//48
	{2048000, 64000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0x1f, 0x02, 0x10, 0x10, 0x04},	//32

	/* 88.2k */
	{11289600, 88200, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x10, 0x04},	//128
	{5644800, 88200, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x02, 0x10, 0x10, 0x04},	//64
	{2822400, 88200, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0x1f, 0x02, 0x10, 0x10, 0x04},	//32

	/* 96k */
	{12288000, 96000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x7f, 0x04, 0x10, 0x10, 0x04},	//128
	{18432000, 96000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xbf, 0x04, 0x10, 0x10, 0x04},	//192
	{6144000, 96000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x02, 0x10, 0x10, 0x04},	//64
	{3072000, 96000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0x1f, 0x02, 0x10, 0x10, 0x04},	//32
};
static inline int get_coeff(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}
	return -EINVAL;
}

/*
 * if PLL not be used, use internal clk1 for mclk,otherwise, use internal clk2 for PLL source.
 */
static int es8311_set_dai_sysclk(struct snd_soc_dai *dai,
			int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_component *component = dai->component;
	struct es8311_private *es8311 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "Enter into %s()\n", __func__);
	switch (freq) {
	case 11289600:
	case 22579200:
	case 5644800:
	case 2822400:
	case 1411200:
	case 12288000:
	case 16384000:
	case 18432000:
	case 24576000:
	case 8192000:
	case 6144000:
	case 4096000:
	case 2048000:
	case 3072000:
	case 1536000:
	case 1024000:
	case 12000000:
		es8311->mclk_rate = freq;
	}

	return 0;
}

static int es8311_set_dai_fmt(struct snd_soc_dai *component_dai, unsigned int fmt)
{
	struct snd_soc_component *component = component_dai->component;
	struct es8311_private *es8311 = snd_soc_component_get_drvdata(component);
	u8 iface = 0;
	u8 adciface = 0;
	u8 daciface = 0;

	dev_dbg(component->dev, "Enter into %s()\n", __func__);
	iface	 = snd_soc_component_read(component, ES8311_RESET_REG00);
	adciface = snd_soc_component_read(component, ES8311_SDPOUT_REG0A);
	daciface = snd_soc_component_read(component, ES8311_SDPIN_REG09);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:	/* MASTER MODE */
		dev_dbg(component->dev, "%s mastermode\n", __func__);
		es8311->mastermode = 1;
		dev_dbg(component->dev, "ES8311 in Master mode\n");
		iface |= 0x40;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:	/* SLAVE MODE */
		es8311->mastermode = 0;
		dev_dbg(component->dev, "ES8311 in Slave mode\n");
		iface &= 0xBF;
		break;
	default:
		return -EINVAL;
	}
	snd_soc_component_write(component, ES8311_RESET_REG00, iface);


	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		dev_dbg(component->dev, "ES8311 in I2S Format\n");
		adciface &= 0xFC;
		daciface &= 0xFC;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		return -EINVAL;
	case SND_SOC_DAIFMT_LEFT_J:
		dev_dbg(component->dev, "ES8311 in LJ Format\n");
		adciface &= 0xFC;
		daciface &= 0xFC;
		adciface |= 0x01;
		daciface |= 0x01;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		dev_dbg(component->dev, "ES8311 in DSP-A Format\n");
		adciface &= 0xDC;
		daciface &= 0xDC;
		adciface |= 0x03;
		daciface |= 0x03;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		dev_dbg(component->dev, "ES8311 in DSP-B Format\n");
		adciface &= 0xDC;
		daciface &= 0xDC;
		adciface |= 0x23;
		daciface |= 0x23;
		break;
	default:
		return -EINVAL;
	}

	iface	 = snd_soc_component_read(component, ES8311_CLK_MANAGER_REG06);
	/* clock inversion */
	if (((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_I2S) ||
			((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_LEFT_J)) {
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:

			iface	 &= 0xDF;
			adciface &= 0xDF;
			daciface &= 0xDF;
			break;
		case SND_SOC_DAIFMT_IB_IF:
			iface	 |= 0x20;
			adciface |= 0x20;
			daciface |= 0x20;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			iface	 |= 0x20;
			adciface &= 0xDF;
			daciface &= 0xDF;
			break;
		case SND_SOC_DAIFMT_NB_IF:
			iface	 &= 0xDF;
			adciface |= 0x20;
			daciface |= 0x20;
			break;
		default:
			return -EINVAL;
		}
	}

	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG06, iface);
	snd_soc_component_write(component, ES8311_SDPOUT_REG0A, adciface);

	snd_soc_component_write(component, ES8311_SDPIN_REG09, daciface);
	return 0;
}
static int es8311_pcm_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	return 0;
}

static int es8311_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct es8311_private *es8311 = snd_soc_component_get_drvdata(component);
	u16 iface;
	int coeff;
	u8 regv, datmp;

	dev_dbg(component->dev, "Enter into %s()\n", __func__);
	/* we need mclk rate to configure registers. Set MCLK here if failed
	 * to get mclk from set_sysclk.
	 *
	 * If the es8311->mclk_rate is a constant value, for example 12.288M,
	 * set es8311->mclk_rate = 12288000;
	 * else if es8311->mclk_rate is dynamic, for example 128Fs,
	 * set es8311->mclk_rate = 128 * params_rate(params);
	 */
	if (es8311->mclk_src == ES8311_BCLK_PIN) {
		/*
		 * Here 64 is ratio of BCLK/LRCK.
		 * If BCLK/LRCK isn't 64, please change it according to actual ratio.
		 */
		snd_soc_component_update_bits(component,
				ES8311_CLK_MANAGER_REG01, 0x80, 0x80);
		//es8311->mclk_rate = 64 * params_rate(params);
	}

	switch(params_rate(params)) {
		case 44100:
		case 22050:
				es8311->mclk_rate = 11289600;
				break;
		case 8000:
		case 16000:
		case 32000:
		case 48000:
		case 96000:
				es8311->mclk_rate = 12288000;
				break;
		default:
				es8311->mclk_rate = 12288000;
				break;
	}

	dev_dbg(component->dev, "%s, mclk = %d, lrck = %d\n", __func__,
			es8311->mclk_rate, params_rate(params));

	coeff = get_coeff(es8311->mclk_rate, params_rate(params));
	if (coeff < 0) {
		dev_err(component->dev, "Unable to configure sample rate %dHz with %dHz MCLK\n",
			params_rate(params), es8311->mclk_rate);
		return -EINVAL;
	}
	/*
	 * set clock parammeters
	 */
	if (coeff >= 0) {
		regv = snd_soc_component_read(component,
				ES8311_CLK_MANAGER_REG02) & 0x07;

		regv |= (coeff_div[coeff].prediv - 1) << 5;
		datmp = 0;
		switch (coeff_div[coeff].premulti) {
		case 1:
			datmp = 0;
			break;
		case 2:
			datmp = 1;
			break;
		case 4:
			datmp = 2;
			break;
		case 8:
			datmp = 3;
			break;
		default:
			break;
		}
		regv |= (datmp) << 3;
		snd_soc_component_write(component, ES8311_CLK_MANAGER_REG02, regv);

		regv = snd_soc_component_read(component,
				ES8311_CLK_MANAGER_REG05) & 0x00;
		regv |= (coeff_div[coeff].adcdiv - 1) << 4;
		regv |= (coeff_div[coeff].dacdiv - 1) << 0;
		snd_soc_component_write(component, ES8311_CLK_MANAGER_REG05, regv);

		regv = snd_soc_component_read(component,
				ES8311_CLK_MANAGER_REG03) & 0x80;
		regv |= coeff_div[coeff].fsmode << 6;
		regv |= coeff_div[coeff].adcosr << 0;
		snd_soc_component_write(component, ES8311_CLK_MANAGER_REG03, regv);

		regv = snd_soc_component_read(component,
				ES8311_CLK_MANAGER_REG04) & 0x80;
		regv |= coeff_div[coeff].dacosr << 0;
		snd_soc_component_write(component, ES8311_CLK_MANAGER_REG04, regv);

		regv = snd_soc_component_read(component,
				ES8311_CLK_MANAGER_REG07) & 0xf0;
		regv |= coeff_div[coeff].lrck_h << 0;
		snd_soc_component_write(component, ES8311_CLK_MANAGER_REG07, regv);

		regv = snd_soc_component_read(component,
				ES8311_CLK_MANAGER_REG08) & 0x00;
		regv |= coeff_div[coeff].lrck_l << 0;
		snd_soc_component_write(component, ES8311_CLK_MANAGER_REG08, regv);

		regv = snd_soc_component_read(component,
				ES8311_CLK_MANAGER_REG06) & 0xE0;
		if (coeff_div[coeff].bclkdiv < 19)
			regv |= (coeff_div[coeff].bclkdiv - 1) << 0;
		else
			regv |= coeff_div[coeff].bclkdiv << 0;

		snd_soc_component_write(component, ES8311_CLK_MANAGER_REG06, regv);

		regv = snd_soc_component_read(component, ES8311_ADC_REG16) & 0x38;
		regv |= (coeff_div[coeff].adcscale) << 0;
		snd_soc_component_write(component, ES8311_ADC_REG16, regv);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		iface = snd_soc_component_read(component,
				ES8311_SDPIN_REG09) & 0xE3;
		/* bit size */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			iface |= 0x0c;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			iface |= 0x04;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			iface |= 0x10;
			break;
		}
		/* set iface */
		snd_soc_component_write(component, ES8311_SDPIN_REG09, iface);
	} else {
		iface = snd_soc_component_read(component,
				ES8311_SDPOUT_REG0A) & 0xE3;
		/* bit size */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			iface |= 0x0c;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			iface |= 0x04;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			iface |= 0x10;
			break;
		}
		/* set iface */
		snd_soc_component_write(component, ES8311_SDPOUT_REG0A, iface);
	}

	iface = snd_soc_component_read(component, ES8311_SDPIN_REG09);
        iface &= 0x7f;
	snd_soc_component_write(component, ES8311_SDPIN_REG09, iface);

	return 0;
}

static int es8311_set_bias_level(struct snd_soc_component *component,
			enum snd_soc_bias_level level)
{
	int regv;
	struct es8311_private *es8311 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "Enter into %s(), level = %d\n", __func__, level);
	switch (level) {
	case SND_SOC_BIAS_ON:
		dev_dbg(component->dev, "%s on\n", __func__);
		snd_soc_component_write(component, ES8311_GP_REG45, 0x00);
		if (es8311->mclk_src == ES8311_MCLK_PIN) {
			snd_soc_component_write(component,
					ES8311_CLK_MANAGER_REG01, 0x30);
		} else {
			snd_soc_component_write(component,
					ES8311_CLK_MANAGER_REG01, 0xB0);
		}
		//snd_soc_component_write(component, ES8311_ADC_REG16, 0x24);
		snd_soc_component_write(component, ES8311_SYSTEM_REG0B, 0x00);
		snd_soc_component_write(component, ES8311_SYSTEM_REG0C, 0x00);
		if (ES8311_AVDD == ES8311_1V8) {
			snd_soc_component_write(component,
					ES8311_SYSTEM_REG10, 0x61);
			snd_soc_component_write(component,
					ES8311_SYSTEM_REG11, 0x7B);
		} else {
			snd_soc_component_write(component,
					ES8311_SYSTEM_REG10, 0x03);
			snd_soc_component_write(component,
					ES8311_SYSTEM_REG11, 0x57);
		}

		if (es8311->mclk_src == ES8311_MCLK_PIN) {
			snd_soc_component_write(component,
					ES8311_CLK_MANAGER_REG01, 0x3F);
		} else {
			snd_soc_component_write(component,
					ES8311_CLK_MANAGER_REG01, 0xBF);
		}
		if (es8311->mclkinv == true) {
			snd_soc_component_update_bits(component,
					ES8311_CLK_MANAGER_REG01, 0x40, 0x40);
		} else {
			snd_soc_component_update_bits(component,
					ES8311_CLK_MANAGER_REG01, 0x40, 0x00);
		}
		if (es8311->sclkinv == true) {
			snd_soc_component_update_bits(component,
					ES8311_CLK_MANAGER_REG06, 0x20, 0x20);
		} else {
			snd_soc_component_update_bits(component,
					ES8311_CLK_MANAGER_REG06, 0x20, 0x00);
		}

		//digital reset
		snd_soc_component_write(component, ES8311_RESET_REG00, 0x1f);
		usleep_range(1000, 2000);
		if (es8311->mastermode == 1) {
			snd_soc_component_write(component,
					ES8311_RESET_REG00, 0xC0);
		} else {
			snd_soc_component_write(component,
					ES8311_RESET_REG00, 0x80);
		}
		usleep_range(1500, 3000);

		snd_soc_component_write(component, ES8311_SYSTEM_REG0D, 0x01);

		regv = snd_soc_component_read(component, ES8311_SYSTEM_REG14) & 0xCF;
		regv |= 0x1A;
		snd_soc_component_write(component, ES8311_SYSTEM_REG14, regv);

		if (es8311->dmic_enable == true) {
			snd_soc_component_update_bits(component, ES8311_SYSTEM_REG14,
					0x40, 0x40);
		} else {
			snd_soc_component_update_bits(component, ES8311_SYSTEM_REG14,
					0x40, 0x00);
		}
		snd_soc_component_write(component, ES8311_SYSTEM_REG12, 0x00);
		snd_soc_component_write(component, ES8311_SYSTEM_REG13, 0x10);
		snd_soc_component_write(component, ES8311_SYSTEM_REG0E, 0x02);
		snd_soc_component_write(component, ES8311_SYSTEM_REG0F, 0x7F);
		snd_soc_component_write(component, ES8311_ADC_REG15, 0x40);
		snd_soc_component_write(component, ES8311_ADC_REG1B, 0x0A);
		snd_soc_component_write(component, ES8311_ADC_REG1C, 0x6A);
		snd_soc_component_write(component, ES8311_DAC_REG37, 0x48);
		//snd_soc_component_write(component, ES8311_ADC_REG17, 0xBF);
		//snd_soc_component_write(component, ES8311_DAC_REG32, 0xBF);
		break;
	case SND_SOC_BIAS_PREPARE:
		dev_dbg(component->dev, "%s prepare\n", __func__);
		break;
	case SND_SOC_BIAS_STANDBY:
		dev_dbg(component->dev, "%s standby\n", __func__);
		if (es8311->bias_level == SND_SOC_BIAS_PREPARE) {
			//snd_soc_component_write(component, ES8311_DAC_REG32, 0x00);
			//snd_soc_component_write(component, ES8311_ADC_REG17, 0x00);
			snd_soc_component_write(component, ES8311_SYSTEM_REG0E, 0xFF);
			snd_soc_component_write(component, ES8311_SYSTEM_REG12, 0x02);
			snd_soc_component_write(component, ES8311_SYSTEM_REG14, 0x00);
			snd_soc_component_write(component, ES8311_SYSTEM_REG0D, 0xF9);
			snd_soc_component_write(component, ES8311_ADC_REG15, 0x00);
			snd_soc_component_write(component, ES8311_DAC_REG37, 0x08);
			snd_soc_component_write(component, ES8311_CLK_MANAGER_REG02, 0x10);
			snd_soc_component_write(component, ES8311_RESET_REG00, 0x00);
			snd_soc_component_write(component, ES8311_RESET_REG00, 0x1F);
			snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0x30);
			snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0x00);
			snd_soc_component_write(component, ES8311_GP_REG45, 0x00);
		}
		break;
	case SND_SOC_BIAS_OFF:
		dev_dbg(component->dev, "%s off\n", __func__);
		if (es8311->bias_level == SND_SOC_BIAS_STANDBY) {
			//snd_soc_component_write(component, ES8311_DAC_REG32, 0x00);
			//snd_soc_component_write(component, ES8311_ADC_REG17, 0x00);
			snd_soc_component_write(component, ES8311_SYSTEM_REG0E, 0xFF);
			snd_soc_component_write(component, ES8311_SYSTEM_REG12, 0x02);
			snd_soc_component_write(component, ES8311_SYSTEM_REG14, 0x00);
			snd_soc_component_write(component, ES8311_SYSTEM_REG0D, 0xF9);
			snd_soc_component_write(component, ES8311_ADC_REG15, 0x00);
			snd_soc_component_write(component, ES8311_DAC_REG37, 0x08);
			snd_soc_component_write(component, ES8311_CLK_MANAGER_REG02, 0x10);
			snd_soc_component_write(component, ES8311_RESET_REG00, 0x00);
			snd_soc_component_write(component, ES8311_RESET_REG00, 0x1F);
			snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0x30);
			snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0x00);
			snd_soc_component_write(component, ES8311_GP_REG45, 0x00);
			snd_soc_component_write(component, ES8311_SYSTEM_REG0D, 0xFC);
			snd_soc_component_write(component, ES8311_CLK_MANAGER_REG02, 0x00);
		}
		break;
	}
	es8311->bias_level = level;
	return 0;
}

static int es8311_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_component *component = dai->component;

	dev_dbg(component->dev, "Enter into %s(), tristate = %d\n", __func__, tristate);
	if (tristate) {
		snd_soc_component_update_bits(component,
				ES8311_CLK_MANAGER_REG07, 0x30, 0x30);
	} else {
		snd_soc_component_update_bits(component,
				ES8311_CLK_MANAGER_REG07, 0x30, 0x00);
	}
	return 0;
}

static int es8311_mute(struct snd_soc_dai *dai, int mute, int direction)
{
	struct snd_soc_component *component = dai->component;

	dev_dbg(component->dev, "Enter into %s(), mute = %d\n", __func__, mute);

	if (mute) {
		snd_soc_component_write(component, ES8311_SYSTEM_REG12,
				0x02);
		snd_soc_component_update_bits(component, ES8311_DAC_REG31,
				0x60, 0x60);
		//snd_soc_component_write(component, ES8311_DAC_REG32, 0x00);
	} else {
		snd_soc_component_update_bits(component, ES8311_DAC_REG31,
				0x60, 0x00);
		snd_soc_component_write(component, ES8311_SYSTEM_REG12,
				0x00);
		//snd_soc_component_write(component, ES8311_DAC_REG32, 0xbf);
	}
	return 0;
}

#define es8311_RATES SNDRV_PCM_RATE_8000_96000

#define es8311_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops es8311_ops = {
	.startup = es8311_pcm_startup,
	.hw_params = es8311_pcm_hw_params,
	.set_fmt = es8311_set_dai_fmt,
	.set_sysclk = es8311_set_dai_sysclk,
	.mute_stream = es8311_mute,
	.set_tristate = es8311_set_tristate,
};

static struct snd_soc_dai_driver es8311_dai = {
	.name = "ES8311 HiFi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = es8311_RATES,
		.formats = es8311_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = es8311_RATES,
		.formats = es8311_FORMATS,
	},
	.ops = &es8311_ops,
	.symmetric_rate = 1,
};

static int es8311_suspend(struct snd_soc_component *component)
{
	dev_dbg(component->dev, "Enter into %s()\n", __func__);
	//snd_soc_component_write(component, ES8311_DAC_REG32, 0x00);
	//snd_soc_component_write(component, ES8311_ADC_REG17, 0x00);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0E, 0xFF);
	snd_soc_component_write(component, ES8311_SYSTEM_REG12, 0x02);
	snd_soc_component_write(component, ES8311_SYSTEM_REG14, 0x00);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0D, 0xF9);
	snd_soc_component_write(component, ES8311_ADC_REG15, 0x00);
	snd_soc_component_write(component, ES8311_DAC_REG37, 0x08);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG02, 0x10);
	snd_soc_component_write(component, ES8311_RESET_REG00, 0x00);
	snd_soc_component_write(component, ES8311_RESET_REG00, 0x1F);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0x30);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0x00);
	snd_soc_component_write(component, ES8311_GP_REG45, 0x00);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0D, 0xFC);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG02, 0x00);
	return 0;
}

static int es8311_resume(struct snd_soc_component *component)
{
	struct es8311_private *es8311 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "Enter into %s()\n", __func__);
	snd_soc_component_write(component, ES8311_GP_REG45, 0x00);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0x30);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG02, 0x00);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG03, 0x10);
	snd_soc_component_write(component, ES8311_ADC_REG16, 0x24);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG04, 0x10);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG05, 0x00);
	snd_soc_component_write(component, ES8311_SDPIN_REG09, 0x00);
	snd_soc_component_write(component, ES8311_SDPOUT_REG0A, 0x00);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0B, 0x00);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0C, 0x00);

	if (ES8311_AVDD == ES8311_1V8) {
		snd_soc_component_write(component, ES8311_SYSTEM_REG10, 0x61);
		snd_soc_component_write(component, ES8311_SYSTEM_REG11, 0x7B);
	} else {
		snd_soc_component_write(component, ES8311_SYSTEM_REG10, 0x03);
		snd_soc_component_write(component, ES8311_SYSTEM_REG11, 0x57);
	}
	if (es8311->mclk_src == ES8311_MCLK_PIN) {
		snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0x3F);
		snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0xBF);
	}
	if (es8311->mastermode == 1)
		snd_soc_component_write(component, ES8311_RESET_REG00, 0xC0);
	else
		snd_soc_component_write(component, ES8311_RESET_REG00, 0x80);

	usleep_range(1500, 3000);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0D, 0x01);

	if (es8311->mclkinv == true) {
		snd_soc_component_update_bits(component, ES8311_CLK_MANAGER_REG01,
				0x40, 0x40);
	} else {
		snd_soc_component_update_bits(component, ES8311_CLK_MANAGER_REG01,
				0x40, 0x00);
	}
	if (es8311->sclkinv == true) {
		snd_soc_component_update_bits(component, ES8311_CLK_MANAGER_REG06,
				0x20, 0x20);
	} else {
		snd_soc_component_update_bits(component, ES8311_CLK_MANAGER_REG06,
				0x20, 0x00);
	}
	snd_soc_component_write(component, ES8311_SYSTEM_REG14, 0x1A);
	if (es8311->dmic_enable == true) {
		snd_soc_component_update_bits(component, ES8311_SYSTEM_REG14,
				0x40, 0x40);
	} else {
		snd_soc_component_update_bits(component, ES8311_SYSTEM_REG14,
				0x40, 0x00);
	}
	snd_soc_component_write(component, ES8311_SYSTEM_REG12, 0x00);
	snd_soc_component_write(component, ES8311_SYSTEM_REG13, 0x10);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0E, 0x02);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0F, 0x7F);
	snd_soc_component_write(component, ES8311_ADC_REG15, 0x40);
	snd_soc_component_write(component, ES8311_ADC_REG1B, 0x0A);
	snd_soc_component_write(component, ES8311_ADC_REG1C, 0x6A);
	snd_soc_component_write(component, ES8311_DAC_REG37, 0x48);
	snd_soc_component_write(component, ES8311_ADC_REG17, 0xBF);
	snd_soc_component_write(component, ES8311_DAC_REG32, 0xBF);

	return 0;
}

static int es8311_probe(struct snd_soc_component *component)
{
	int ret = 0;
	struct es8311_private *es8311 = es8311_data;

	dev_dbg(component->dev, "Enter into %s()\n", __func__);

	snd_soc_component_set_drvdata(component, es8311);
	if (component == NULL) {
		dev_err(component->dev, "Codec device not registered\n");
		return -ENODEV;
	}
	es8311_component = component;
	es8311->component = component;

	es8311->mastermode = 0;
	es8311->mclk_src = ES8311_MCLK_SOURCE;
	/* Enable the following code if there is no mclk.
	 * a clock named "mclk" need to be defined in the dts (see sample dts)
	 *
	 * No need to enable the following code to get mclk if:
	 * 1. sclk/bclk is used as mclk
	 * 2. mclk is controled by soc I2S
	 */
#if 0
	if (es8311->mclk_src == ES8311_MCLK_PIN) {
		es8311->mclk = devm_clk_get(component->dev, "mclk");
		if (IS_ERR(es8311->mclk)) {
			dev_err(component->dev, "%s,unable to get mclk\n", __func__);
			return PTR_ERR(es8311->mclk);
		}
		if (!es8311->mclk)
			dev_err(component->dev, "%s, assuming static mclk\n", __func__);

		ret = clk_prepare_enable(es8311->mclk);
		if (ret) {
			dev_err(component->dev, "%s, unable to enable mclk\n", __func__);
			return ret;
		}
	}
#endif
	snd_soc_component_write(component, ES8311_GP_REG45, 0x00);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0x30);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG02, 0x00);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG03, 0x10);
	snd_soc_component_write(component, ES8311_ADC_REG16, 0x24);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG04, 0x10);
	snd_soc_component_write(component, ES8311_CLK_MANAGER_REG05, 0x00);
	snd_soc_component_write(component, ES8311_SDPIN_REG09, 0x00);
	snd_soc_component_write(component, ES8311_SDPOUT_REG0A, 0x00);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0B, 0x00);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0C, 0x00);
	if (ES8311_AVDD == ES8311_1V8) {
		snd_soc_component_write(component, ES8311_SYSTEM_REG10, 0x61);
		snd_soc_component_write(component, ES8311_SYSTEM_REG11, 0x7B);
	} else {
		snd_soc_component_write(component, ES8311_SYSTEM_REG10, 0x03);
		snd_soc_component_write(component, ES8311_SYSTEM_REG11, 0x57);
	}

	if (es8311->mclk_src == ES8311_MCLK_PIN)
		snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0x3F);
	else
		snd_soc_component_write(component, ES8311_CLK_MANAGER_REG01, 0xBF);

	if (es8311->mastermode == 1)
		snd_soc_component_write(component, ES8311_RESET_REG00, 0xC0);
	else
		snd_soc_component_write(component, ES8311_RESET_REG00, 0x80);

	usleep_range(1500, 3000);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0D, 0x01);

	if (es8311->mclkinv == true) {
		snd_soc_component_update_bits(component, ES8311_CLK_MANAGER_REG01,
				0x40, 0x40);
	} else {
		snd_soc_component_update_bits(component, ES8311_CLK_MANAGER_REG01,
				0x40, 0x00);
	}
	if (es8311->sclkinv == true) {
		snd_soc_component_update_bits(component, ES8311_CLK_MANAGER_REG06,
				0x20, 0x20);
	} else {
		snd_soc_component_update_bits(component, ES8311_CLK_MANAGER_REG06,
				0x20, 0x00);
	}
	snd_soc_component_write(component, ES8311_SYSTEM_REG14, 0x1A);
	if (es8311->dmic_enable == true) {
		snd_soc_component_update_bits(component, ES8311_SYSTEM_REG14,
				0x40, 0x40);
	} else {
		snd_soc_component_update_bits(component, ES8311_SYSTEM_REG14,
				0x40, 0x00);
	}
	snd_soc_component_write(component, ES8311_SYSTEM_REG12, 0x00);
	snd_soc_component_write(component, ES8311_SYSTEM_REG13, 0x10);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0E, 0x02);
	snd_soc_component_write(component, ES8311_SYSTEM_REG0F, 0x7F);
	snd_soc_component_write(component, ES8311_ADC_REG15, 0x40);
	snd_soc_component_write(component, ES8311_ADC_REG1B, 0x0A);
	snd_soc_component_write(component, ES8311_ADC_REG1C, 0x6A);
	snd_soc_component_write(component, ES8311_DAC_REG37, 0x48);
	snd_soc_component_write(component, ES8311_ADC_REG17, 0xBF);
	snd_soc_component_write(component, ES8311_DAC_REG32, 0xBF);
	msleep(100);
	es8311_set_bias_level(component, SND_SOC_BIAS_STANDBY);

	return ret;
}

static void es8311_remove(struct snd_soc_component *component)
{
	es8311_set_bias_level(component, SND_SOC_BIAS_OFF);
}

static const struct snd_soc_component_driver soc_component_dev_es8311 = {
	.probe = es8311_probe,
	.remove = es8311_remove,
	.suspend = es8311_suspend,
	.resume = es8311_resume,
	.set_bias_level = es8311_set_bias_level,
	.suspend_bias_off = 1,
	.idle_bias_on = 1,

	.controls = es8311_snd_controls,
	.num_controls = ARRAY_SIZE(es8311_snd_controls),
	.dapm_widgets = es8311_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(es8311_dapm_widgets),
	.dapm_routes = es8311_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(es8311_dapm_routes),
};

static struct regmap_config es8311_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = ES8311_MAX_REGISTER,

	.volatile_reg = es8311_volatile_register,
	.writeable_reg = es8311_writable_register,
	.readable_reg  = es8311_readable_register,
	.cache_type = REGCACHE_RBTREE,
};

#ifdef CONFIG_OF
static const struct of_device_id es8311_if_dt_ids[] = {
	{.compatible = "everest,es8311", },
	{ }
};
MODULE_DEVICE_TABLE(of, es8311_if_dt_ids);
#endif

static void es8311_i2c_shutdown(struct i2c_client *i2c)
{

}

static u32 cur_reg;

static ssize_t es8311_show(struct device *dev,
			struct device_attribute *attr, char *_buf)
{
	int ret;

	ret = sprintf(_buf, "%s(): get 0x%04x=0x%04x\n", __func__, cur_reg,
			snd_soc_component_read(es8311_component, cur_reg));
	return ret;
}

static ssize_t es8311_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0, flag = 0;
	u8 i = 0, reg, num, value_w, value_r;

	val = simple_strtol(buf, NULL, 16);
	flag = (val >> 16) & 0xFF;

	if (flag) {
		reg = (val >> 8) & 0xFF;
		value_w = val & 0xFF;
		dev_info(dev, "\nWrite: start REG:0x%02x,val:0x%02x,count:0x%02x\n",
				reg, value_w, flag);
		while (flag--) {
			snd_soc_component_write(es8311_component, reg, value_w);
			dev_dbg(dev, "Write 0x%02x to REG:0x%02x\n", value_w, reg);
			reg++;
		}
	} else {
		reg = (val >> 8) & 0xFF;
		num = val & 0xff;
		dev_info(dev, "\nRead: start REG:0x%02x,count:0x%02x\n", reg, num);
		do {
			value_r = 0;
			value_r = snd_soc_component_read(es8311_component, reg);
			dev_info(dev, "REG[0x%02x]: 0x%02x;\n", reg, value_r);
			reg++;
			i++;
		} while (i < num);
	}

	return count;
}

static DEVICE_ATTR(es8311, 0664, es8311_show, es8311_store);

static struct attribute *es8311_debug_attrs[] = {
	&dev_attr_es8311.attr,
	NULL,
};

static struct attribute_group es8311_debug_attr_group = {
	.name	= "es8311_debug",
	.attrs	= es8311_debug_attrs,
};

static int32_t es8311_i2c_suspend(struct device *dev)
{
	int32_t ret = 0;
	dev_info(dev, "%s\n", __func__);
	return ret;
}

static int32_t es8311_i2c_resume(struct device *dev)
{
	int32_t ret = 0;
	dev_info(dev, "%s\n", __func__);
	return ret;
}

static const struct dev_pm_ops es8311_i2c_pm_ops = {
	.suspend = es8311_i2c_suspend,
	.resume = es8311_i2c_resume,
};

static int es8311_i2c_probe(struct i2c_client *i2c_client,
					const struct i2c_device_id *id)
{
	struct es8311_private *es8311;
	int ret = -1;
	unsigned int val;

	dev_dbg(&i2c_client->dev, "Enter into %s\n", __func__);
	es8311 = devm_kzalloc(&i2c_client->dev,
			sizeof(*es8311), GFP_KERNEL);
	if (es8311 == NULL)
		return -ENOMEM;

	es8311->dmic_enable = false;	 // dmic interface disabled
	/* the edge of lrck is always at the falling edge of mclk */
	es8311->mclkinv = false;
	/* the edge of lrck is always at the falling edge of sclk */
	es8311->sclkinv = false;

	i2c_set_clientdata(i2c_client, es8311);
	es8311->regmap = devm_regmap_init_i2c(i2c_client, &es8311_regmap);
	if (IS_ERR(es8311->regmap)) {
		ret = PTR_ERR(es8311->regmap);
		dev_err(&i2c_client->dev, "regmap_init() failed: %d\n", ret);
		return ret;
	}
	/* verify that we have an es8311 */
	ret = regmap_read(es8311->regmap, ES8311_CHD1_REGFD, &val);
	if (ret < 0) {
		dev_err(&i2c_client->dev, "failed to read i2c at addr %X\n",
			   i2c_client->addr);
		return ret;
	}
	/* The first ID should be 0x83 */
	if (val != 0x83) {
		dev_err(&i2c_client->dev, "device at addr %X is not an es8311\n",
			   i2c_client->addr);
		return -ENODEV;
	}
	ret = regmap_read(es8311->regmap, ES8311_CHD2_REGFE, &val);
	/* The NEXT ID should be 0x11 */
	if (val != 0x11) {
		dev_err(&i2c_client->dev, "device at addr %X is not an es8311\n",
			   i2c_client->addr);
		return -ENODEV;
	}
	es8311_data = es8311;

	ret =  snd_soc_register_component(&i2c_client->dev,
			&soc_component_dev_es8311,
			&es8311_dai,
			1);
	if (ret < 0) {
		kfree(es8311);
		return ret;
	}

	dev_dbg(&i2c_client->dev, "Enter into %s-----4\n", __func__);
	ret = sysfs_create_group(&i2c_client->dev.kobj,
				&es8311_debug_attr_group);
	if (ret)
		pr_err("failed to create attr group\n");

	return ret;
}

static void es8311_i2c_remove(struct i2c_client *i2c) {
	sysfs_remove_group(&i2c->dev.kobj, &es8311_debug_attr_group);
	snd_soc_unregister_component(&i2c->dev);
}

static const struct i2c_device_id es8311_i2c_id[] = {
	{"es8311", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, es8311_i2c_id);

static struct i2c_driver es8311_i2c_driver = {
	.driver = {
		.name	= "es8311",
		.owner	= THIS_MODULE,
		.of_match_table = es8311_if_dt_ids,
		.pm = &es8311_i2c_pm_ops,
	},
	.shutdown = es8311_i2c_shutdown,
	.probe = es8311_i2c_probe,
	.remove = es8311_i2c_remove,
	.id_table	= es8311_i2c_id,
};

static int __init es8311_init(void)
{
	int ret;

	ret = i2c_add_driver(&es8311_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register es8311 i2c driver\n");
	return ret;
}

static void __exit es8311_exit(void)
{
	return i2c_del_driver(&es8311_i2c_driver);
}

late_initcall(es8311_init);
module_exit(es8311_exit);

MODULE_DESCRIPTION("ASoC es8311 driver");
MODULE_AUTHOR("David Yang <yangxiaohua@everest-semi.com>");
MODULE_LICENSE("GPL");


