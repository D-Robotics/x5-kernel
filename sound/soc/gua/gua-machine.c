/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/control.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/dma-mapping.h>
#include "gua_pcm.h"

/* the data segment used by SSF for interaction between acore and DSP, such as logs and shared data. */
#define SSF_DATA_SIZE (0x01000000)

/* make sure : bind with pcms_hw_info & cpu_dais */
static struct snd_soc_dai_link gua_dai_links[] = {
	{
		.name = "LINK0",
		.stream_name = "Media-Stream",
		.num_cpus = 1,
		.num_platforms = 1,
		.num_codecs = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.playback_only = false,
		.capture_only = false,
	},
	{
		.name = "LINK1",
		.stream_name = "Phone-Stream",
		.num_cpus = 1,
		.num_platforms = 1,
		.num_codecs = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.playback_only = false,
		.capture_only = false,
	},
	{
		.name = "LINK2",
		.stream_name = "VR-Stream",
		.num_cpus = 1,
		.num_platforms = 1,
		.num_codecs = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.playback_only = false,
		.capture_only = false,
	},
	{
		.name = "LINK3",
		.stream_name = "ASR-Stream",
		.num_cpus = 1,
		.num_platforms = 1,
		.num_codecs = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.playback_only = false,
		.capture_only = false,
	},
	{
		.name = "LINK4",
		.stream_name = "HISF-Stream",
		.num_cpus = 1,
		.num_platforms = 1,
		.num_codecs = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.playback_only = false,
		.capture_only = false,
	},
	{
                .name = "LINK5",
                .stream_name = "PDM-stream",
                .num_cpus = 1,
                .num_platforms = 1,
                .num_codecs = 1,
                .dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
                .playback_only = false,
                .capture_only = false,
        },
};
/* make sure : bind with cpu_dais in file:gua-cpu-dai.c */
const char *cpu_names[] = {
	"CPU DAI0",
	"CPU DAI1",
	"CPU DAI2",
	"CPU DAI3",
	"CPU DAI4",
	"CPU DAI5",
};

struct gua_audio_data {
	struct snd_soc_dai_link *dai_links;
	unsigned int num_dai_links;
	struct snd_soc_card card;
};

static int gua_machine_probe(struct platform_device *pdev)
{
	struct device_node *cpu_np;
	struct platform_device *cpu_pdev;
	struct device *dev = &pdev->dev;
	struct gua_audio_data *data;
	struct gua_audio_info *au_info;
	struct snd_soc_dai_link_component *dlc;
	int i, ret;
	dma_addr_t phy_dma;

	dlc = devm_kzalloc(&pdev->dev, 3 * sizeof(*dlc), GFP_KERNEL);
	if (!dlc)
		return -ENOMEM;

	cpu_np = of_parse_phandle(pdev->dev.of_node, "cpu-dai", 0);
	if (!cpu_np) {
		dev_err(&pdev->dev, "AUDIO : cpu dai phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	data->dai_links = gua_dai_links;
	data->num_dai_links = ARRAY_SIZE(gua_dai_links);

	cpu_pdev = of_find_device_by_node(cpu_np);
	if (!cpu_pdev) {
		dev_err(&pdev->dev, "AUDIO : failed to find cpu-dai device\n");
		ret = -EINVAL;
		goto fail;
	}

	au_info = platform_get_drvdata(cpu_pdev);

	au_info->mem_info.mem_addr = dma_alloc_coherent(dev, SSF_DATA_SIZE, &phy_dma, GFP_KERNEL);
	if(!au_info->mem_info.mem_addr)
		return -ENOMEM;
	au_info->mem_info.phy_addr_base = phy_dma;
	au_info->mem_info.mem_size = 0x1000000;
	dev_dbg(&pdev->dev, "AUDIO : Share Memory. phy_addr 0x%llx, base-addr:%px, mem-size:%d\n", phy_dma, au_info->mem_info.mem_addr, au_info->mem_info.mem_size);

	for (i = 0; i < data->num_dai_links; i++) {
		data->dai_links[i].cpus = &dlc[0];
		data->dai_links[i].platforms = &dlc[1];
		data->dai_links[i].codecs = &dlc[2];
		data->dai_links[i].cpus->dai_name = cpu_names[i];
		data->dai_links[i].platforms->of_node = cpu_np;
		data->dai_links[i].codecs->dai_name = "snd-soc-dummy-dai";
		data->dai_links[i].codecs->name = "snd-soc-dummy";
	}

	data->card.num_links = data->num_dai_links;
	data->card.dai_link = data->dai_links;

	data->card.dev = &pdev->dev;
	data->card.owner = THIS_MODULE;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto fail;

	platform_set_drvdata(pdev, &data->card);
	snd_soc_card_set_drvdata(&data->card, data);
	ret = devm_snd_soc_register_card(&pdev->dev, &data->card);
	if (ret) {
		dev_err(&pdev->dev, "AUDIO : sound card regist failed (%d)\n", ret);
		goto fail;
	}

	dev_info(&pdev->dev, "AUDIO : ALSA machine driver probe succ\n");
fail:
	if (cpu_np)
		of_node_put(cpu_np);
	return ret;
}

static const struct of_device_id alsa_machine_dt_ids[] = {
	{ .compatible = "gua,sound-card", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, alsa_machine_dt_ids);

static struct platform_driver alsa_machine_driver = {
	.driver = {
		.name = "machine-driver",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = alsa_machine_dt_ids,
	},
	.probe = gua_machine_probe,
};
module_platform_driver(alsa_machine_driver);

MODULE_AUTHOR("GuaSemi, Inc.");
MODULE_DESCRIPTION("GuaSemi audio ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:ipc-wrapper");
