/*
 * ALSA SoC ES7210 codec driver
 *
 * Author:      David Yang, <yangxiaohua@everest-semi.com>
 *		or
 *		<info@everest-semi.com>
 * Copyright:   (C) 2018 Everest Semiconductor Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES7210L_H
#define _ES7210L_H

#define ES7210_RESET_CTL_REG00		0x00
#define ES7210_CLK_ON_OFF_REG01		0x01
#define ES7210_MCLK_CTL_REG02		0x02
#define ES7210_MST_CLK_CTL_REG03	0x03
#define ES7210_MST_LRCDIVH_REG04	0x04
#define ES7210_MST_LRCDIVL_REG05	0x05
#define ES7210_DIGITAL_PDN_REG06	0x06
#define ES7210_ADC_OSR_REG07		0x07
#define ES7210_MODE_CFG_REG08		0x08

#define ES7210_TCT0_CHPINI_REG09	0x09
#define ES7210_TCT1_CHPINI_REG0A	0x0A
#define ES7210_CHIP_STA_REG0B		0x0B

#define ES7210_IRQ_CTL_REG0C		0x0C
#define ES7210_MISC_CTL_REG0D		0x0D
#define ES7210_MODE_CTL_REG0E		0x0E
#define ES7210_CLKDIV0_CTL_REG0F	0x0F
#define ES7210_DMIC_CTL_REG10		0x10

#define ES7210_SDP_CFG1_REG11		0x11
#define ES7210_SDP_CFG2_REG12		0x12

#define ES7210_ADC_AUTOMUTE_REG13	0x13
#define ES7210_ADC34_MUTE_REG14		0x14
#define ES7210_ADC12_MUTE_REG15		0x15

#define ES7210_ALC_SEL_REG16		0x16
#define ES7210_ALC_COM_CFG1_REG17	0x17
#define ES7210_ALC34_LVL_REG18		0x18
#define ES7210_ALC12_LVL_REG19		0x19
#define ES7210_ALC_COM_CFG2_REG1A	0x1A
#define ES7210_ALC4_MAX_GAIN_REG1B	0x1B
#define ES7210_ALC3_MAX_GAIN_REG1C	0x1C
#define ES7210_ALC2_MAX_GAIN_REG1D	0x1D
#define ES7210_ALC1_MAX_GAIN_REG1E	0x1E

#define ES7210_ADC34_HPF2_REG20		0x20
#define ES7210_ADC34_HPF1_REG21		0x21
#define ES7210_ADC12_HPF2_REG22		0x22
#define ES7210_ADC12_HPF1_REG23		0x23

#define ES7210_CHP_ID1_REG3D		0x3D
#define ES7210_CHP_ID0_REG3E		0x3E
#define ES7210_CHP_VER_REG3F		0x3F

#define ES7210_ANALOG_SYS_REG40		0x40

#define ES7210_MICBIAS12_REG41		0x41
#define ES7210_MICBIAS34_REG42		0x42
#define ES7210_MIC1_GAIN_REG43		0x43
#define ES7210_MIC2_GAIN_REG44		0x44
#define ES7210_MIC3_GAIN_REG45		0x45
#define ES7210_MIC4_GAIN_REG46		0x46
#define ES7210_MIC1_LP_REG47		0x47
#define ES7210_MIC2_LP_REG48		0x48
#define ES7210_MIC3_LP_REG49		0x49
#define ES7210_MIC4_LP_REG4A		0x4A
#define ES7210_MIC12_PDN_REG4B		0x4B
#define ES7210_MIC34_PDN_REG4C		0x4C
#define ES7210_MIC_GAIN 0x10

#define ENABLE          1
#define DISABLE         0

#define MIC_CHN_16      16
#define MIC_CHN_14      14
#define MIC_CHN_12      12
#define MIC_CHN_10      10
#define MIC_CHN_8       8
#define MIC_CHN_6       6
#define MIC_CHN_4       4
#define MIC_CHN_2       2

#define ES7210_CHANNELS_MAX     MIC_CHN_4

/*TDM mode is used when ES7210_TDM_ENABLE == 1*/
#define ES7210_TDM_ENABLE       ENABLE
#define ES7210_NFS_ENABLE       DISABLE


#if ES7210_CHANNELS_MAX == MIC_CHN_2
#define ADC_DEV_MAXNUM  1
#endif
#if ES7210_CHANNELS_MAX == MIC_CHN_4
#define ADC_DEV_MAXNUM  1
#endif
#if ES7210_CHANNELS_MAX == MIC_CHN_6
#define ADC_DEV_MAXNUM  2
#endif
#if ES7210_CHANNELS_MAX == MIC_CHN_8
#define ADC_DEV_MAXNUM  2
#endif
#if ES7210_CHANNELS_MAX == MIC_CHN_10
#define ADC_DEV_MAXNUM  3
#endif
#if ES7210_CHANNELS_MAX == MIC_CHN_12
#define ADC_DEV_MAXNUM  3
#endif
#if ES7210_CHANNELS_MAX == MIC_CHN_14
#define ADC_DEV_MAXNUM  4
#endif
#if ES7210_CHANNELS_MAX == MIC_CHN_16
#define ADC_DEV_MAXNUM  4
#endif

#define I2S			0
#define DSP  		1

#define ES7210_TDM_1LRCK_DSPA                 0
#define ES7210_TDM_1LRCK_DSPB                 1
#define ES7210_TDM_1LRCK_I2S                  2
#define ES7210_TDM_1LRCK_LJ                   3
#define ES7210_TDM_NLRCK_DSPA                 4
#define ES7210_TDM_NLRCK_DSPB                 5
#define ES7210_TDM_NLRCK_I2S                  6
#define ES7210_TDM_NLRCK_LJ                   7
#define ES7210_NORMAL_I2S                     8
#define ES7210_NORMAL_LJ                      9
#define ES7210_NORMAL_DSPA                    10
#define ES7210_NORMAL_DSPB                    11

#define ES7210_WORK_MODE    ES7210_TDM_1LRCK_DSPA

#define RATIO_MCLK_LRCK_64   64
#define RATIO_MCLK_LRCK_128  128
#define RATIO_MCLK_LRCK_192  192
#define RATIO_MCLK_LRCK_256  256
#define RATIO_MCLK_LRCK_384  384
#define RATIO_MCLK_LRCK_512  512
#define RATIO_MCLK_LRCK_768  768
#define RATIO_MCLK_LRCK_896  896
#define RATIO_MCLK_LRCK_1024 1024
#define RATIO_MCLK_LRCK_1152  1152
#define RATIO_MCLK_LRCK_1280  1280
#define RATIO_MCLK_LRCK_1408  1408
#define RAITO_MCLK_LRCK_1536 1536
#define RATIO_MCLK_LRCK_1664  1664
#define RATIO_MCLK_LRCK_1792  1792
#define RATIO_MCLK_LRCK_1920  1920
#define RATIO_MCLK_LRCK_2048 2048
#define RATIO_MCLK_LRCK_3072  3072
#define RATIO_MCLK_LRCK_4096  4096

#define RATIO_MCLK_LRCK  RATIO_MCLK_LRCK_768

#define ES7210_I2C_BUS_NUM              1
#define ES7210_CODEC_RW_TEST_EN         0
/* reset ES7210 when in idle time */
#define ES7210_IDLE_RESET_EN            1
/* ES7210 match method select: 0: i2c_detect, 1:of_device_id */
#define ES7210_MATCH_DTS_EN             1

 /* In master mode, 0 : MCLK from pad    1 : MCLK from clock doubler */
#define FROM_PAD_PIN                  0
#define FROM_CLOCK_DOUBLE_PIN         1
#define ES7210_MCLK_SOURCE            FROM_PAD_PIN


struct i2c_client *i2c_clt1[ADC_DEV_MAXNUM];

int es7210_init_reg = 0;
struct es7210_reg_config {
	unsigned char reg_addr;
	unsigned char reg_v;
};
static const struct es7210_reg_config es7210_tdm_reg_common_cfg1[] = {
	{0x09, 0x20},
	{0x0A, 0x10},
	{0x23, 0x2a},
	{0x22, 0x0a},
	{0x21, 0x2a},
	{0x20, 0x0a},
};

static const struct es7210_reg_config es7210_tdm_reg_fmt_cfg[] = {
	{0x11, 0x83},
	{0x12, 0x01},
};

static const struct es7210_reg_config es7210_tdm_reg_common_cfg2[] = {
	{0x40, 0xC3},
	{0x41, 0x71},
	{0x42, 0x71},
	{0x47, 0x26},
	{0x48, 0x06},
	{0x49, 0x26},
	{0x4A, 0x06},
	{0x43, 0x10},
	{0x44, 0x10},
	{0x45, 0x10},
	{0x46, 0x10},
	{0x07, 0x20},
};

static const struct es7210_reg_config es7210_tdm_reg_mclk_cfg[] = {
	{0x02, 0xC1},
};

static const struct es7210_reg_config es7210_tdm_reg_common_cfg3[] = {
	{0x06, 0x00},
	{0x4B, 0x0F},
	{0x4C, 0x0F},
	{0x00, 0x71},
	{0x00, 0x41},
};

struct es7210_mclklrck_ratio_config {
	int ratio;
	unsigned char nfs;
	unsigned char channels;
	unsigned char reg02_v;
	unsigned char reg06_v;
};

static const struct es7210_mclklrck_ratio_config es7210_1fs_ratio_cfg[] = {
	//ratio, nfs, channels, reg02_v, reg06_v
	{64, 0, 0, 0x41, 0x00},
	{128, 0, 0, 0x01, 0x00},
	{192, 0, 0, 0x43, 0x00},
	{256, 0, 0, 0xc1, 0x04},
	{384, 0, 0, 0x03, 0x00},
	{512, 0, 0, 0x81, 0x04},
	{768, 0, 0, 0xc3, 0x04},
	{896, 0, 0, 0x07, 0x00},
	{1024, 0, 0, 0x82, 0x04},
	{1152, 0, 0, 0x09, 0x00},
	{1280, 0, 0, 0x0a, 0x00},
	{1408, 0, 0, 0x0b, 0x00},
	{1536, 0, 0, 0xc6, 0x04},
	{1664, 0, 0, 0x0d, 0x00},
	{1792, 0, 0, 0xc7, 0x04},
	{1920, 0, 0, 0x0f, 0x00},
	{2048, 0, 0, 0x84, 0x04},
	{3072, 0, 0, 0x86, 0x04},
	{4096, 0, 0, 0x88, 0x04},
};

static const struct es7210_mclklrck_ratio_config es7210_nfs_ratio_cfg[] = {
	//ratio, nfs, channels, reg02_v, reg06_v
	{32, 1, 4, 0x41, 0x00},
	{32, 1, 8, 0x01, 0x00},
	{32, 1, 12, 0x43, 0x00},
	{32, 1, 16, 0xC1, 0x04},

	{64, 1, 4, 0x01, 0x00},
	{64, 1, 6, 0x43, 0x00},
	{64, 1, 8, 0xC1, 0x04},
	{64, 1, 10, 0x45, 0x00},
	{64, 1, 12, 0x03, 0x00},
	{64, 1, 14, 0x47, 0x00},
	{64, 1, 16, 0x81, 0x04},

	{96, 1, 4, 0x43, 0x00},
	{96, 1, 8, 0x03, 0x00},
	{96, 1, 12, 0x49, 0x00},
	{96, 1, 16, 0xc3, 0x04},

	{128, 1, 4, 0xc1, 0x04},
	{128, 1, 6, 0x03, 0x00},
	{128, 1, 8, 0x81, 0x04},
	{128, 1, 10, 0x05, 0x00},
	{128, 1, 12, 0xc3, 0x04},
	{128, 1, 14, 0x07, 0x00},
	{128, 1, 16, 0x82, 0x04},

	{192, 1, 4, 0x03, 0x00},
	{192, 1, 6, 0x49, 0x00},
	{192, 1, 8, 0xc3, 0x04},
	{192, 1, 10, 0x4f, 0x00},
	{192, 1, 12, 0x09, 0x00},
	{192, 1, 14, 0x55, 0x00},
	{192, 1, 16, 0x83, 0x04},

	{256, 1, 4, 0x81, 0x04},
	{256, 1, 6, 0xc3, 0x04},
	{256, 1, 8, 0x82, 0x04},
	{256, 1, 10, 0xc5, 0x04},
	{256, 1, 12, 0x83, 0x04},
	{256, 1, 14, 0xc7, 0x04},
	{256, 1, 16, 0x84, 0x04},

	{384, 1, 4, 0xc3, 0x04},
	{384, 1, 6, 0x09, 0x00},
	{384, 1, 8, 0x83, 0x04},
	{384, 1, 10, 0x0f, 0x00},
	{384, 1, 12, 0xc9, 0x04},
	{384, 1, 14, 0x15, 0x00},
	{384, 1, 16, 0x86, 0x04},

	{512, 1, 4, 0x82, 0x04},
	{512, 1, 6, 0x83, 0x04},
	{512, 1, 8, 0x84, 0x04},
	{512, 1, 10, 0x85, 0x04},
	{512, 1, 12, 0x86, 0x04},
	{512, 1, 14, 0x87, 0x04},
	{512, 1, 16, 0x88, 0x04},

	{768, 1, 4, 0x83, 0x04},
	{768, 1, 6, 0xC9, 0x04},
	{768, 1, 8, 0x86, 0x04},
	{768, 1, 10, 0xCf, 0x04},
	{768, 1, 12, 0x89, 0x04},
	{768, 1, 14, 0xD5, 0x04},
	{768, 1, 16, 0x8C, 0x04},

	{1024, 1, 4, 0x84, 0x04},
	{1024, 1, 6, 0x86, 0x04},
	{1024, 1, 8, 0x88, 0x04},
	{1024, 1, 10, 0x8a, 0x04},
	{1024, 1, 12, 0x8c, 0x04},
	{1024, 1, 14, 0x8e, 0x04},
	{1024, 1, 16, 0x90, 0x04},

};
#endif	/* _ES7210_H_ */
