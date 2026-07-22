/*
 * Driver for the ZL380xx codec
 *
 * Copyright (c) 2016, Microsemi Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define DEBUG 1

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <sound/soc.h>

#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#ifndef VPROC_CODEC_MIXER_ENABLE
#define VPROC_CODEC_MIXER_ENABLE
#endif

#ifdef VPROC_CODEC_MIXER_ENABLE
#define VPROC_CODEC_MIXER_ENABLE_DMUTE
#include <linux/moduleparam.h>
#include "typedefs.h"

#include "ssl.h"
#include "chip.h"
#include "hbi.h"

hbi_device_id_t deviceId = 0;
module_param(deviceId, uint, S_IRUGO);
MODULE_PARM_DESC(dev_addr, "device Id (a value from 0 to VPROC_MAX_NUM_DEVS-1");

int zl380xx_reset_gpio_high(void);
int zl380xx_reset_gpio_low(void);
int zl380xx_drv_exit_lowpower(void);
int zl380xx_drv_enter_lowpower(void);
/*
static int zl380xx_control_write(struct snd_kcontrol *kcontrol,
                                 struct snd_ctl_elem_value *ucontrol);

static int zl380xx_control_read(struct snd_kcontrol *kcontrol,
                                struct snd_ctl_elem_value *ucontrol);

static int zl380xx_dac_control_write(struct snd_kcontrol *kcontrol,
                                     struct snd_ctl_elem_value *ucontrol);

static int zl380xx_dac_control_read(struct snd_kcontrol *kcontrol,
                                    struct snd_ctl_elem_value *ucontrol);

static int zl380xx_volume_control_write(struct snd_kcontrol *kcontrol,
                                     struct snd_ctl_elem_value *ucontrol);

static int zl380xx_volume_control_read(struct snd_kcontrol *kcontrol,
                                    struct snd_ctl_elem_value *ucontrol);
*/
struct _zl380xx_priv {
	hbi_handle_t handle;
	struct snd_soc_codec *codec;
	struct device *dev;
    struct i2c_client *i2c;
    struct i2c_device_id const *devid;
};

struct _zl380xx_priv zl380xx_priv;

struct s_zl380xx_dev{
    struct device_node *nd; /* 设备节点 */
    int gpio_num;		    /* gpio num */
};

struct s_zl380xx_dev zl380xx_dev;


#define DAC_VOL_STEPS 40
/*static int8_t zl380_vol_table[41] = {
    //1,   0,   -1,   -2,  -3,  -4,   -5,  -6,   -7,  -8,
    -9,  -10,  -11, -12, -13,  -14, -15, -16, -17, -18,
    -19, -21, -21, -22,  -23,  -24, -25, -26, -27, -28,
    -29, -30, -31, -32, -33,  -34, -35, -36, -37, -38,
    -39, -40, -41, -42, -43,  -44, -45, -47, -49, -51,
    //-46, -47, -48,-49, -50, -51, -52, -53,  -54, -55, -56, -57, -58,
    -90
};
*/
#ifdef VPROC_CODEC_MIXER_ENABLE_DMUTE
#if 0
static int zl380tw_mute_r(struct snd_soc_component *comp, int on){
    user_buffer_t buf[2];
    hbi_status_t status;
    reg_addr_t reg = ZL380xx_AEC_CTRL0_REG;
    u16 val;

    status = HBI_read(zl380xx_priv.handle, reg, buf, 2);
    if (status != HBI_STATUS_SUCCESS){
        return -EIO;
    }
    val = (buf[0] << 8) | buf[1];

    if (((val >> 7) & 1) == on){
        return 0;
    }
    val &= ~(1 << 7);
    val |= on << 7;

    buf[0] = val >> 8;
    buf[1] = val & 0xFF;

    status = HBI_write(zl380xx_priv.handle, reg, buf, 2);
    if (status != HBI_STATUS_SUCCESS){
        return -EIO;
    }
    return 0;
}
#endif
#if 0
/*zl380tw_mute_s() - function to Mute SOUT*/
static int zl380tw_mute_s(struct snd_soc_codec *codec, int on)
{
    u16 val;
    user_buffer_t buf[2];
    hbi_status_t status;
    reg_addr_t reg = ZL380xx_AEC_CTRL0_REG;

    status = HBI_read(zl380xx_priv.handle, reg, buf, 2);
    if (status != HBI_STATUS_SUCCESS)
    {
        return -EIO;
    }
    val = (buf[0] << 8) | buf[1];

    if (((val >> 8) & 1) == on)
    {
        return 0;
    }
    val &= ~(1 << 8);
    val |= on << 8;

    buf[0] = val >> 8;
    buf[1] = val & 0xFF;

    status = HBI_write(zl380xx_priv.handle, reg, buf, 2);
    if (status != HBI_STATUS_SUCCESS)
    {
        return -EIO;
    }
	return 0;
}
#endif

#if 0
/*ALSA auto-handling of muting audio path when no audio is detected*/
static int zl380xx_mute(struct snd_soc_dai *codec_dai, int mute)
{
    struct snd_soc_component *comp = codec_dai->component;
    /*zl380tw_mute_s(codec, mute);*/ /*uncomment if you want to mute both send and receive paths*/
    return zl380tw_mute_r(comp, mute);
}
#endif
extern int drv_aw87390_open(void);
extern void drv_aw87390_close(void);

static int zl380xx_startup(struct snd_pcm_substream *stream,struct snd_soc_dai *dai)
{
    printk("<Bruce zl38063> %s Line %d: stream name:%s, dai name: %s\n", __FUNCTION__,__LINE__, stream->name, dai->name);

    zl380xx_reset_gpio_high();
    //zl380xx_drv_exit_lowpower();
    return 0;
}
static void zl380xx_shutdown(struct snd_pcm_substream *stream,struct snd_soc_dai *dai)
{
    printk("<Bruce zl38063> %s Line %d: stream name:%s, dai name: %s\n", __FUNCTION__,__LINE__, stream->name, dai->name);
    //zl380xx_mute(dai, 0);
    zl380xx_reset_gpio_low();
    //zl380xx_drv_enter_lowpower();
}
static int zl380xx_hw_params(struct snd_pcm_substream *stream,struct snd_pcm_hw_params *param, struct snd_soc_dai *dai)
{
    printk("<Bruce zl38063> %s Line %d: stream name:%s, dai name: %s\n", __FUNCTION__,__LINE__, stream->name, dai->name);
	return 0;
}
static int zl380xx_set_sysclk(struct snd_soc_dai *dai,
            int clk_id, unsigned int freq, int dir)
{
    printk("<Bruce zl38063> %s Line %d: dai name: %s\n", __FUNCTION__,__LINE__, dai->name);
	return 0;
}

int zl380xx_digital_mute(struct snd_soc_dai *dai, int mute)
{
    printk("<Bruce zl38063> %s Line %d: dai name: %s\n", __FUNCTION__,__LINE__, dai->name);
	return 0;
}
int zl380xx_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{
    printk("<Bruce zl38063> %s Line %d: dai name: %s\n", __FUNCTION__,__LINE__, dai->name);
	return 0;
}

static const struct snd_soc_dai_ops zl380xx_dai_ops = {
    //.digital_mute = zl380xx_mute,
    .startup = zl380xx_startup,
    .hw_params = zl380xx_hw_params,
    .shutdown = zl380xx_shutdown,
    .set_sysclk = zl380xx_set_sysclk,
    .mute_stream = zl380xx_mute_stream,
};
#endif

#endif

#define ZL380XX_HOST_RATE (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_44100)
#define ZL380XX_HOST_FORMATS SNDRV_PCM_FMTBIT_S16_LE
#define ZL380XX_DEV_RATE  (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_44100)
#define ZL380XX_DEV_FORMATS SNDRV_PCM_FMTBIT_S16_LE

static struct snd_soc_dai_driver zl380xx_dai[] = {
    //host interface
	{
		.name = "ZL380xx_CIF",
		.playback = {
			.stream_name = "ZL380xx_CIF Receive",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ZL380XX_HOST_RATE,
			.formats = ZL380XX_HOST_FORMATS,
		},
		.capture = {
			.stream_name = "ZL380xx_CIF Transmit",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ZL380XX_HOST_RATE,
			.formats = ZL380XX_HOST_FORMATS,
		},
		.ops = &zl380xx_dai_ops,
        .symmetric_rate  = 1,
    },
    //speaker&mic inout
    {
        .name = "ZL380xx_DAP",
        .playback = {
            .stream_name = "ZL380xx_DAP Receive",
            .channels_min = 1,
            .channels_max = 2,
            .rates = ZL380XX_DEV_RATE,
            .formats = ZL380XX_DEV_FORMATS,
        },
        .capture = {
            .stream_name = "ZL380xx_DAP Transmit",
            .channels_min = 1,
            .channels_max = 2,
            .rates = ZL380XX_DEV_RATE,
            .formats = ZL380XX_DEV_FORMATS,
        },
        .ops = &zl380xx_dai_ops,
        .symmetric_rate  = 1,
    },
    //i2s out
    {
        .name = "ZL380xx_DAP_A",
        .playback = {
            .stream_name = "ZL380xx_DAP_A Receive",
            .channels_min = 1,
            .channels_max = 2,
            .rates = ZL380XX_HOST_RATE,
            .formats = ZL380XX_HOST_FORMATS,
        },
        .capture = {
            .stream_name = "ZL380xx_DAP_A Transmit",
            .channels_min = 1,
            .channels_max = 2,
            .rates = ZL380XX_HOST_RATE,
            .formats = ZL380XX_HOST_FORMATS,
        },
        .ops = &zl380xx_dai_ops,
        .symmetric_rate  = 1,
    }
};

#ifdef VPROC_CODEC_MIXER_ENABLE
static const struct snd_soc_dapm_route zl380xx_snd_routes[] = {
    { "ZL380xx_DAP Receive", NULL, "MICIN1" },
    { "ZL380xx_DAP Receive", NULL, "MICIN2" },
    { "ZL380xx_CIF RX",       NULL, "ZL380xx_CIF Receive" },
    { "ZL380xx_DAP TX",       NULL, "ZL380xx_CIF RX" },
    { "ZL380xx_DAP Transmit", NULL, "ZL380xx_DAP TX" },

    { "ZL380xx_DAP RX",       NULL, "ZL380xx_DAP Receive" },
    { "ZL380xx_CIF TX",       NULL, "ZL380xx_DAP RX" },
    { "ZL380xx_CIF Transmit", NULL, "ZL380xx_CIF TX" },

    { "ZL380xx_DAP_A TX",       NULL, "ZL380xx_CIF RX" },
    { "ZL380xx_DAP_A Transmit", NULL, "ZL380xx_DAP_A TX" },

    { "ZL380xx_DAP_A RX",       NULL, "ZL380xx_DAP_A Receive" },
    { "ZL380xx_CIF TX",       NULL, "ZL380xx_DAP_A RX" },
};

static const struct snd_soc_dapm_widget zl380xx_snd_widgets[] = {
    SND_SOC_DAPM_INPUT("MICIN1"),
    SND_SOC_DAPM_INPUT("MICIN2"),

    SND_SOC_DAPM_AIF_IN("ZL380xx_CIF RX", NULL, 0, SND_SOC_NOPM, 0, 0),
    SND_SOC_DAPM_AIF_OUT("ZL380xx_CIF TX", NULL, 0, SND_SOC_NOPM, 0, 0),
    SND_SOC_DAPM_AIF_IN("ZL380xx_DAP RX", NULL, 0, SND_SOC_NOPM, 0, 0),
    SND_SOC_DAPM_AIF_OUT("ZL380xx_DAP TX", NULL, 0, SND_SOC_NOPM, 0, 0),
    SND_SOC_DAPM_AIF_IN("ZL380xx_DAP_A RX", NULL, 0, SND_SOC_NOPM, 0, 0),
    SND_SOC_DAPM_AIF_OUT("ZL380xx_DAP_A TX", NULL, 0, SND_SOC_NOPM, 0, 0),
};

static const struct snd_kcontrol_new zl380xx_snd_controls[] = {
#if 0
    SOC_SINGLE_EXT("DAC", 0, 0, DAC_VOL_STEPS, 0,
                   zl380xx_volume_control_read, zl380xx_volume_control_write),

    SOC_SINGLE_EXT("DAC1 GAIN INA", ZL380xx_CP_DAC1_GAIN_REG, 0, DAC_VOL_STEPS, 0,
                   zl380xx_dac_control_read, zl380xx_dac_control_write),
    SOC_SINGLE_EXT("DAC2 GAIN INA", ZL380xx_CP_DAC2_GAIN_REG, 0, DAC_VOL_STEPS, 0,
                   zl380xx_dac_control_read, zl380xx_dac_control_write),

    SOC_SINGLE_EXT("DAC1 GAIN INB", ZL380xx_CP_DAC1_GAIN_REG, 8, DAC_VOL_STEPS, 0,
                   zl380xx_dac_control_read, zl380xx_dac_control_write),
    SOC_SINGLE_EXT("DAC2 GAIN INB", ZL380xx_CP_DAC2_GAIN_REG, 8, DAC_VOL_STEPS, 0,
                   zl380xx_dac_control_read, zl380xx_dac_control_write),
#endif

#if 0

    SOC_SINGLE_EXT("MIC SOUT MUTE", ZL380xx_AEC_CTRL0_REG, 8, 1, 0,
                   zl380xx_control_read, zl380xx_control_write),

    SOC_SINGLE_EXT("MUTE SPEAKER ROUT", ZL380xx_AEC_CTRL0_REG, 7, 1, 0,
                   zl380xx_control_read, zl380xx_control_write),
    SOC_SINGLE_EXT("AEC MIC GAIN", ZL380xx_DIG_MIC_GAIN_REG, 0, 0x7, 0,
                    zl380xx_control_read, zl380xx_control_write),
	SOC_SINGLE_EXT("AEC ROUT GAIN", ZL380xx_ROUT_GAIN_CTRL_REG, 0, 0x78, 0,
		            zl380xx_control_read, zl380xx_control_write),
	SOC_SINGLE_EXT("AEC ROUT GAIN EXT", ZL380xx_ROUT_GAIN_CTRL_REG, 7, 0x7, 0,
				zl380xx_control_read, zl380xx_control_write),

    SOC_SINGLE_EXT("MIC SOUT GAIN", ZL380xx_SOUT_GAIN_CTRL_REG, 8, 0xf, 0,
                   zl380xx_control_read, zl380xx_control_write),
#endif

};
/*
static int zl380xx_control_read(
    struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
    struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
    unsigned int reg = mc->reg;
    unsigned int shift = mc->shift;
    unsigned int mask = mc->max;
    unsigned int invert = mc->invert;
    unsigned char buf[2];
    hbi_status_t status = HBI_STATUS_SUCCESS;
    unsigned int val = 0;

    status = HBI_read(zl380xx_priv.handle, reg, buf, 2);
    dev_info(component->dev,"reg 0x%03X val received 0x%x 0x%x\n", reg, buf[0], buf[1]);
    val = buf[0];
    val = (val << 8) | buf[1];

    ucontrol->value.integer.value[0] = ((val >> shift) & mask);

    if (invert)
        ucontrol->value.integer.value[0] = mask - ucontrol->value.integer.value[0];

    return 0;
}

static int zl380xx_control_write(struct snd_kcontrol *kcontrol,
                                 struct snd_ctl_elem_value *ucontrol)
{

    struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
    struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
    reg_addr_t reg = mc->reg;
    unsigned int shift = mc->shift;
    unsigned int mask = mc->max;
    unsigned int invert = mc->invert;
    unsigned int val = (ucontrol->value.integer.value[0] & mask);
    unsigned int valt = 0;
    user_buffer_t buf[2];
    hbi_status_t status;

    if (invert)
        val = mask - val;

    status = HBI_read(zl380xx_priv.handle, reg, buf, 2);
    if (status != HBI_STATUS_SUCCESS){
        return -EIO;
    }

    valt = buf[0];
    valt = (valt << 8) | buf[1];

    if (((valt >> shift) & mask) == val){
        return 0;
    }

    valt &= ~(mask << shift);
    valt |= val << shift;

    buf[0] = valt >> 8;
    buf[1] = valt & 0xFF;

    status = HBI_write(zl380xx_priv.handle, reg, buf, 2);

    dev_info(component->dev,"reg 0x%03X val write 0x%x 0x%x\n", reg, buf[0], buf[1]);

    if (status != HBI_STATUS_SUCCESS){
        return -EIO;
    }
    return 0;
}

static int zl380xx_dac_control_read(struct snd_kcontrol *kcontrol,
                                    struct snd_ctl_elem_value *ucontrol)
{
    struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
    struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
    unsigned int reg = mc->reg;
    unsigned char buf[2];
    hbi_status_t status = HBI_STATUS_SUCCESS;
    int8_t volume = 0;
    int i;

    status = HBI_read(zl380xx_priv.handle, reg, buf, 2);
    dev_info(component->dev,"dac reg 0x%03X val received 0x%x 0x%x\n", reg, buf[0], buf[1]);
    if ((mc->shift) != 0){
        volume = *(char *)(buf);
    }else{
        volume = *(char *)(buf + 1);
    }

    for (i = 0; i <= DAC_VOL_STEPS; i++){
        if (volume >= zl380_vol_table[i]){
            break;
        }
    }

    dev_info(component->dev,"volume %d, zl380_vol_table[i]=%d, zl380_vol_table[20]=%d, i=%d\n", volume, zl380_vol_table[i], zl380_vol_table[20], i);

    ucontrol->value.integer.value[0] = DAC_VOL_STEPS - i;

#if 0
    if (invert)
        ucontrol->value.integer.value[0] = mask - ucontrol->value.integer.value[0];
#endif
    return 0;
}

static int zl380xx_dac_control_write(struct snd_kcontrol *kcontrol,
                                     struct snd_ctl_elem_value *ucontrol)
{

    struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
    struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
    reg_addr_t reg = mc->reg;
    unsigned int val = (ucontrol->value.integer.value[0]);
    int8_t valt = 0;
    char *valtp;
    user_buffer_t buf[2];
    hbi_status_t status;

    status = HBI_read(zl380xx_priv.handle, reg, buf, 2);
    if (status != HBI_STATUS_SUCCESS){
        return -EIO;
    }

    if (val > DAC_VOL_STEPS){
        val = DAC_VOL_STEPS;
    }

    valt = zl380_vol_table[DAC_VOL_STEPS - val];

    if ((mc->shift) != 0){
        valtp = (char *)(buf);
    }else{
        valtp = (char *)(buf + 1);
    }

    if ((*valtp) == valt){
        return 0;
    }

    *valtp = valt;

    status = HBI_write(zl380xx_priv.handle, reg, buf, 2);
    dev_info(component->dev,"dac reg 0x%03X val write 0x%x 0x%x\n", reg, buf[0], buf[1]);

    if (status != HBI_STATUS_SUCCESS){
        return -EIO;
    }
    return 0;
}

static int zl380xx_volume_control_read(struct snd_kcontrol *kcontrol,
                                       struct snd_ctl_elem_value *ucontrol)
{
    struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
    struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
    unsigned int reg = mc->reg;
    unsigned char buf1[2];
    unsigned char buf2[2];
    hbi_status_t status = HBI_STATUS_SUCCESS;
    int8_t volume = 0;
    int i;

    status = HBI_read(zl380xx_priv.handle, ZL380xx_CP_DAC1_GAIN_REG, buf1, 2);
    dev_info(component->dev,"dac1 reg 0x%03X val received 0x%x 0x%x\n", reg, buf1[0], buf1[1]);

    status = HBI_read(zl380xx_priv.handle, ZL380xx_CP_DAC2_GAIN_REG, buf2, 2);
    dev_info(component->dev,"dac2 reg 0x%03X val received 0x%x 0x%x\n", reg, buf2[0], buf2[1]);


    volume = (buf1[0] + buf1[1] + buf2[0] + buf2[1]) / 4;
    for (i = 0; i <= DAC_VOL_STEPS; i++){
        if (volume >= zl380_vol_table[i]){
            break;
        }
    }

    dev_info(component->dev,"volume %d, zl380_vol_table[i]=%d, zl380_vol_table[20]=%d, i=%d\n", volume, zl380_vol_table[i], zl380_vol_table[20], i);

    ucontrol->value.integer.value[0] = DAC_VOL_STEPS - i;

#if 0
    if (invert)
        ucontrol->value.integer.value[0] = mask - ucontrol->value.integer.value[0];
#endif

    return 0;
}

static int zl380xx_volume_control_write(struct snd_kcontrol *kcontrol,
                                        struct snd_ctl_elem_value *ucontrol)
{

    struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
    struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
    reg_addr_t reg = mc->reg;
    unsigned int val = (ucontrol->value.integer.value[0]);
    int8_t valt = 0;
    user_buffer_t buf[2];
    hbi_status_t status;

    status = HBI_read(zl380xx_priv.handle, reg, buf, 2);
    if (status != HBI_STATUS_SUCCESS){
        return -EIO;
    }

    if (val > DAC_VOL_STEPS){
        val = DAC_VOL_STEPS;
    }

    valt = zl380_vol_table[DAC_VOL_STEPS - val];

    buf[0] = valt;
    buf[1] = valt;

    status = HBI_write(zl380xx_priv.handle, ZL380xx_CP_DAC1_GAIN_REG, buf, 2);
    dev_info(component->dev,"dac1 reg 0x%03X val write 0x%x 0x%x\n", reg, buf[0], buf[1]);
    status = HBI_write(zl380xx_priv.handle, ZL380xx_CP_DAC2_GAIN_REG, buf, 2);
    dev_info(component->dev,"dac2 reg 0x%03X val write 0x%x 0x%x\n", reg, buf[0], buf[1]);

    if (status != HBI_STATUS_SUCCESS){
        return -EIO;
    }
    return 0;
}
*/
int zl380xx_add_controls(struct snd_soc_component *component)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 0, 0))
    return snd_soc_add_controls(codec, zl380xx_snd_controls,
                                ARRAY_SIZE(zl380xx_snd_controls));
#else
    return snd_soc_add_component_controls(component, zl380xx_snd_controls,
                                      ARRAY_SIZE(zl380xx_snd_controls));
#endif
}

extern int hbi_drv_init(struct i2c_client *pClient, const struct i2c_device_id *pDeviceId);

#define  LOW_POWER_ENABLE
//extern volatile unsigned char gflag_zl38063_lowpower;
unsigned char gflag_zl38063_lowpower = 0;
//extern volatile unsigned char gflag_zl38063_mode;
unsigned char gflag_zl38063_mode;
//extern volatile unsigned char gflag_zl38063_select;
unsigned char gflag_zl38063_select;

int zl380xx_drv_exit_lowpower(void)
{
    printk("<Bruce kernel> %s  Line : %d. gflag_zl38063_mode : %d -- gflag_zl38063_lowpower : 0x%X \r\n",__FUNCTION__,__LINE__,gflag_zl38063_mode,gflag_zl38063_lowpower);
    printk("<Bruce kernel> %s  Line : %d. gflag_zl38063_select : %d \r\n",__FUNCTION__,__LINE__,gflag_zl38063_select);
#ifdef LOW_POWER_ENABLE
    if((gflag_zl38063_mode == 0) || (0x55 == gflag_zl38063_lowpower)|| (1 == gflag_zl38063_select))
    {
        mdelay(500);
        gpio_set_value(zl380xx_dev.gpio_num, 0);
        printk("<Bruce kernel> %s  Line : %d. ---1111111111--- \r\n",__FUNCTION__,__LINE__);
    }
#endif
    return 0;
}

int zl380xx_drv_enter_lowpower(void)
{
    printk("<Bruce kernel> %s  Line : %d. gflag_zl38063_mode : 0x%d -- gflag_zl38063_lowpower : 0x%X \r\n",__FUNCTION__,__LINE__,gflag_zl38063_mode,gflag_zl38063_lowpower);
    printk("<Bruce kernel> %s  Line : %d. gflag_zl38063_select : %d \r\n",__FUNCTION__,__LINE__,gflag_zl38063_select);
#ifdef LOW_POWER_ENABLE
    if((gflag_zl38063_mode == 0) || (0x55 == gflag_zl38063_lowpower)|| (1 == gflag_zl38063_select))
    {
        gpio_set_value(zl380xx_dev.gpio_num, 1);
        mdelay(200);
        printk("<Bruce kernel> %s  Line : %d. ----2222222222----\r\n",__FUNCTION__,__LINE__);
    }
#endif
    return 0;
}
int drv_zl380xx_reset(void)
{
    printk("<Bruce kernel> %s  begin Line : %d. \r\n",__FUNCTION__,__LINE__);
    gpio_set_value(zl380xx_dev.gpio_num, 0);
    mdelay(10);
    gpio_set_value(zl380xx_dev.gpio_num, 1);
    mdelay(10);
    printk("<Bruce kernel> %s  end Line : %d. \r\n",__FUNCTION__,__LINE__);

    return 0;
}
int zl380xx_reset_gpio_high(void)
{
#if 0
    gpio_set_value(zl380xx_dev.gpio_num, 1);
    printk("zl380xx_reset_gpio_high.\r\n");
    drv_aw87390_open();
#endif
    return 0;
}

int zl380xx_reset_gpio_low(void)
{
#if 0
    drv_aw87390_close();
    gpio_set_value(zl380xx_dev.gpio_num, 0);
    printk("zl380xx_reset_gpio_low.\r\n");
#endif
    return 0;
}

static int zl380xx_reset_gpio_init(struct device *dev)
{
    int res = 0;
    const char *str;
    enum of_gpio_flags gpio_flags;

    zl380xx_dev.nd = dev->of_node;

    /* 获取设备节点的属性 */
    res = of_property_read_string(zl380xx_dev.nd, "status", &str);
    if (res < 0) {
	    printk("zl380xx_reset_gpio init: read_string fail\r\n");
	    return -2;
	}
	printk("zl380xx_reset_gpio init: status = %s\r\n", str);

	/* 从设备树中获取节点对应的GPIO编号以及标志 */
	zl380xx_dev.gpio_num = of_get_named_gpio_flags(zl380xx_dev.nd, "reset-gpio", 0, &gpio_flags);
	if(!gpio_is_valid(zl380xx_dev.gpio_num)){
	    printk("zl380xx_reset_gpio init: get gpio fail\r\n");
	    return -3;
	}
    printk("zl380xx_reset_gpio init: gpio_flags = %d\r\n", gpio_flags);

	/* 申请GPIO */
	res =  gpio_request(zl380xx_dev.gpio_num, "reset_gpio");
	if(res < 0){
	    printk("zl380xx_reset_gpio init: gpio request fail\r\n");
	    return -4;
	}

	/* 设置GPIO的方向为输出，并且设置具体的数值 */
	res = gpio_direction_output(zl380xx_dev.gpio_num, 1);
	if(res < 0){
	    printk("zl380xx_reset_gpio init: gpio direction set fail\r\n");
	    gpio_free(zl380xx_dev.gpio_num);
	    return -5;
	}

	printk("zl380xx_reset_gpio init ok.");

	return 0;
}
static int zl380xx_lowpower_gpio_init(struct device *dev)
{
 #if 1//def LOW_POWER_ENABLE
    int res = 0;
    const char *str;
    enum of_gpio_flags gpio_flags;

	// /* 查找设备节点 */
	// zl380xx_dev.nd = of_find_node_by_name(NULL, "zl38xx0");
	// if(zl380xx_dev.nd == NULL){
	//     printk("zl380xx_reset_gpio init: find zl380xx_dev fail\r\n");
	//     return -1;
	// }
    // printk("zl380xx_reset_gpio init: node name = %s\r\n", zl380xx_dev.nd->name);

    zl380xx_dev.nd = dev->of_node;

    /* 获取设备节点的属性 */
    res = of_property_read_string(zl380xx_dev.nd, "status", &str);
    if (res < 0) {
	    printk("zl380xx_reset_gpio init: read_string fail\r\n");
	    return -2;
	}
	printk("zl380xx_lowpower_gpio init: status = %s\r\n", str);

	/* 从设备树中获取节点对应的GPIO编号以及标志 */
	//zl380xx_dev.gpio_num = of_get_named_gpio_flags(zl380xx_dev.nd, "reset-gpio", 0, &gpio_flags);
    zl380xx_dev.gpio_num = of_get_named_gpio_flags(zl380xx_dev.nd, "low_power-gpio", 0, &gpio_flags);
	if(!gpio_is_valid(zl380xx_dev.gpio_num)){
	    printk("zl380xx_lowpower_gpio init: get gpio fail\r\n");
	    return -3;
	}
    printk("zl380xx_lowpower_gpio init: zl380xx_dev.gpio_num : %d   gpio_flags = %d\r\n", zl380xx_dev.gpio_num, gpio_flags);

	/* 申请GPIO */
	res =  gpio_request(zl380xx_dev.gpio_num, "lowpower_gpio");
	if(res < 0){
	    printk("zl380xx_reset_gpio init: gpio request fail\r\n");
	    return -4;
	}

	/* 设置GPIO的方向为输出，并且设置具体的数值 */
	res = gpio_direction_output(zl380xx_dev.gpio_num, 0);
	if(res < 0){
	    printk("zl380xx_lowpower_gpio init: gpio direction set fail\r\n");
	    gpio_free(zl380xx_dev.gpio_num);
	    return -5;
	}
	//zl380xx_reset_gpio_low();
    //zl380xx_drv_enter_lowpower();

	printk("zl380xx_lowpower_gpio init ok.");
#endif
	return 0;
}

static int zl380xx_component_probe(struct snd_soc_component *component)
{
    hbi_status_t status;
    hbi_dev_cfg_t cfg;
    int ret = 0;
    dev_info(component->dev,"ubt test %s! \n",__func__);

    ret = hbi_drv_init(zl380xx_priv.i2c, (const struct i2c_device_id *)zl380xx_priv.devid);
    if(ret != 0){
        dev_err(component->dev,"Error hbi_drv_init\n");
        return -1;
    }

    status = HBI_init(NULL, zl380xx_priv.i2c, zl380xx_priv.devid);
    if (status != HBI_STATUS_SUCCESS){
        dev_err(component->dev,"Error in HBI_init()\n");
        return -1;
    }

    cfg.pDevName = NULL;
    cfg.deviceId = deviceId;

    status = HBI_open(&(zl380xx_priv.handle), &cfg);
    if (status != HBI_STATUS_SUCCESS){
        dev_err(component->dev,"Error in HBI_open()\n");
        HBI_term();
        return -1;
    }

    return 0;
}

static void zl380xx_component_remove(struct snd_soc_component *component)
{
    hbi_status_t status;

    status = HBI_close(zl380xx_priv.handle);
    status = HBI_term();
}
#endif
static struct snd_soc_component_driver soc_component_dev_zl380xx = {
#ifdef VPROC_CODEC_MIXER_ENABLE
    .probe = zl380xx_component_probe,
    .remove = zl380xx_component_remove,
	//.controls		= zl380xx_snd_controls,
	//.num_controls		= ARRAY_SIZE(zl380xx_snd_controls),
	.dapm_widgets		= zl380xx_snd_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(zl380xx_snd_widgets),
	.dapm_routes		= zl380xx_snd_routes,
	.num_dapm_routes	= ARRAY_SIZE(zl380xx_snd_routes),
#endif
};

static int zl380xx_probe(struct i2c_client *i2c,
    const struct i2c_device_id *pDeviceId)
{
    int ret = 0;
	printk("========================== zl380xx_probe \n");
    dev_info(&i2c->dev,"zl380xx_probe adapter::addr = %s::0x%02x\n", i2c->adapter->name, i2c->addr);
    memset(&zl380xx_priv, 0, sizeof(struct _zl380xx_priv));
    zl380xx_priv.dev = &i2c->dev;
    zl380xx_priv.i2c = i2c;
    zl380xx_priv.devid = pDeviceId;
    dev_set_drvdata(zl380xx_priv.dev, &zl380xx_priv);
    printk("zl380xx_probe in\n");

    zl380xx_reset_gpio_init(zl380xx_priv.dev);
    zl380xx_lowpower_gpio_init(zl380xx_priv.dev);

    ret = snd_soc_register_component(&i2c->dev, &soc_component_dev_zl380xx,
						zl380xx_dai, ARRAY_SIZE(zl380xx_dai));

    //zl380xx_reset_gpio_low();
    //zl380xx_drv_exit_lowpower();

    return ret;
}
/*
static int zl380xx_remove(struct i2c_client *i2c){
    snd_soc_unregister_codec(&i2c->dev);
    return 0;
}
*/
static struct of_device_id zl380xx_of_match[] = {
	{ .compatible = "microsemi,zl38xx0",}, /*Change this "microsemi,zl38xx0" accordingly*/
	{ .compatible = "microsemi,zl38xx1",}, /*Change this "microsemi,zl38xx1" accordingly*/
	{},
};
MODULE_DEVICE_TABLE(of, zl380xx_of_match);

static struct i2c_device_id zl380xx_device_id[] = {
    {"zl380xx0", 0 },
    {}
};
#define ZL38063_DEV_NAME "zl380xx0"
static struct i2c_driver zl380xx_driver = {
    .id_table = zl380xx_device_id,
    .probe = zl380xx_probe,
   // .remove = zl380xx_remove,
    .driver = {
        /* name field should be equalto module name and without spaces */
        .name = ZL38063_DEV_NAME,
        .owner = THIS_MODULE,
        .of_match_table = zl380xx_of_match,
    }
};

module_i2c_driver(zl380xx_driver);

MODULE_DESCRIPTION("ASoC zl380xx codec driver");
MODULE_AUTHOR("Jean Bony");
MODULE_LICENSE("GPL v2");
