// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/of.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include "virt-codec.h"

#define VIRT_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

static int dm_set_dai_sysclk(struct snd_soc_dai *codec_dai, int clk_id,
			     unsigned int freq, int dir)
{
	return 0;
}

static int dm_set_dai_clkdiv(struct snd_soc_dai *codec_dai, int div_id, int div)
{
	return 0;
}

static int dm_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	return 0;
}

static int dm_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	return 0;
}

static int dm_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	return 0;
}

static int dm_hw_free(struct snd_pcm_substream *substream,
		      struct snd_soc_dai *dai)
{
	return 0;
}

static int dm_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id, int source,
			  unsigned int freq_in, unsigned int freq_out)
{
	return 0;
}

static const struct snd_soc_dai_ops virt_codec_dai_ops = {
	.hw_params = dm_hw_params,
	.hw_free = dm_hw_free,
	.mute_stream = dm_mute,
	.set_fmt = dm_set_dai_fmt,
	.set_clkdiv = dm_set_dai_clkdiv,
	.set_sysclk = dm_set_dai_sysclk,
	.set_pll = dm_set_dai_pll,
	.no_capture_mute = 1,
};

static struct snd_soc_dai_driver virt_dai_driver = {
	.name = "virt-codec",
	.playback = {
		.stream_name	= "virt-codec Playback",
		.formats	= VIRT_FORMATS,
		.rates		= SNDRV_PCM_RATE_8000_48000,
		.channels_min	= 2,
		.channels_max	= 16,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 16,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = VIRT_FORMATS,
	},
	.ops = &virt_codec_dai_ops,
};

static int virt_suspend(struct snd_soc_component *component)
{
	return 0;
}

static int virt_resume(struct snd_soc_component *component)
{
	return 0;
}

static const struct snd_soc_component_driver virt_codec_componment_driver = {
	.name = "virt-codec",
	.idle_bias_on = 1,
	.use_pmdown_time = 1,
	.endianness = 1,
	.legacy_dai_naming = 1,
	.suspend = virt_suspend,
	.resume = virt_resume,
};

static int virt_codec_probe(struct platform_device *pdev)
{
	struct virt_codec *codec;

	codec = devm_kzalloc(&pdev->dev, sizeof(*codec), GFP_KERNEL);
	if (!codec)
		return -ENOMEM;

	codec->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, codec);

	return devm_snd_soc_register_component(
		&pdev->dev, &virt_codec_componment_driver, &virt_dai_driver, 1);
}

static const struct of_device_id virt_device_id[] = {
	{
		.compatible = "virt-codec",
	},
	{}
};
MODULE_DEVICE_TABLE(of, virt_device_id);

static struct platform_driver virt_codec_driver = {
	.driver = {
		.name = "virt-codec",
		.of_match_table = of_match_ptr(virt_device_id),
	},
	.probe = virt_codec_probe,
};
module_platform_driver(virt_codec_driver);

MODULE_DESCRIPTION("ASoC virtual codec driver");
MODULE_LICENSE("GPL");
