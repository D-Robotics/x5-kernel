/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm_params.h>
#include "gua_pcm.h"
#include "gua_audio_rpc_protocol.h"

extern void link_audio_info(struct gua_audio_info *audio_info);

#define GUA_AUDIO_RATES	(SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_16000|SNDRV_PCM_RATE_48000)
#define GUA_AUDIO_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE)

static const unsigned int gua_rpmsg_rates[] = {
	8000, 16000, 44100, 32000, 48000, 96000, 192000,
};

static const struct snd_pcm_hw_constraint_list gua_rpmsg_rate_constraints = {
	.count = ARRAY_SIZE(gua_rpmsg_rates),
	.list = gua_rpmsg_rates,
};

static int gua_dai_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *cpu_dai)
{
	int ret;

	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE, &gua_rpmsg_rate_constraints);

	return ret;
}

static const struct snd_soc_dai_ops gua_rpmsg_dai_ops = {
	.startup = gua_dai_startup,
};

/* make sure : bind with dai_links & pcms_hw_info */
static struct snd_soc_dai_driver gua_cpu_dai[] = {
	{
		.name ="CPU DAI0",
		.id = 0,
		.playback = {
			.stream_name = "Media-Play",
			.channels_min = 2,
			.channels_max = 12,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.capture = {
			.stream_name = "Mic-Record",
			.channels_min = 2,
			.channels_max = 12,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.ops = &gua_rpmsg_dai_ops,
	},
	{
		.name ="CPU DAI1",
		.id = 1,
		.playback = {
			.stream_name = "Phone-Donwlink",
			.channels_min = 1,
			.channels_max = 2,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.capture = {
			.stream_name = "Phone-Uplink",
			.channels_min = 1,
			.channels_max = 6,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.ops = &gua_rpmsg_dai_ops,
	},
	{
		.name ="CPU DAI2",
		.id = 1,
		.playback = {
			.stream_name = "VR-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.capture = {
			.stream_name = "VR-Record",
			.channels_min = 1,
			.channels_max = 6,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.ops = &gua_rpmsg_dai_ops,
	},
	{
		.name ="CPU DAI3",
		.id = 1,
		.playback = {
			.stream_name = "ASR-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.capture = {
			.stream_name = "ASR-Record",
			.channels_min = 1,
			.channels_max = 6,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.ops = &gua_rpmsg_dai_ops,
	},
	{
		.name ="CPU DAI4",
		.id = 1,
		.playback = {
			.stream_name = "Hisf-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.capture = {
			.stream_name = "Hisf-Record",
			.channels_min = 1,
			.channels_max = 6,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.ops = &gua_rpmsg_dai_ops,
	},
	{
		.name ="CPU DAI5",
		.id = 1,
		.playback = {
			.stream_name = "PDM-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.capture = {
			.stream_name = "PDM-Record",
			.channels_min = 1,
			.channels_max = 8,
			.rates = GUA_AUDIO_RATES,
			.formats = GUA_AUDIO_FORMATS,
		},
		.ops = &gua_rpmsg_dai_ops,
	},
};

static const struct snd_soc_component_driver gua_cpu_dai_component = {
	.name = "cpu-dai-component",
};

static const struct of_device_id gua_cpu_dai_ids[] = {
	{ .compatible = "gua,cpu-dai"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, gua_cpu_dai_ids);

static void rpmsg_audio_work(struct work_struct *work)
{
	struct work_of_rpmsg *work_of_rpmsg;
	struct gua_audio_info *au_info;
	struct pcm_info *pcm_info;
	audio_poster_t *au_poster;
	unsigned long flags;
	int pcm_device, stream;
	int ret;

	work_of_rpmsg = container_of(work, struct work_of_rpmsg, work);
	pcm_device = work_of_rpmsg->msg.send_msg.param.pcm_device;
	au_info = work_of_rpmsg->au_info;
	pcm_info = &au_info->pcm_info;

	(PCM_TX_PERIOD_DONE >= work_of_rpmsg->msg.send_msg.param.cmd) ? (stream = 0) : (stream = 1);
	if (!au_info->au_warpper->poster[pcm_info->data[pcm_device].poster_id[stream]].registed) {
		dev_err(pcm_info->dev, "AUDIO : pcm[%d][%d] has no poster\n", pcm_device, stream);
		return;
	}
	au_poster = &au_info->au_warpper->poster[pcm_info->data[pcm_device].poster_id[stream]];

	ret = au_info->au_warpper->send_msg(au_poster, (void *)&work_of_rpmsg->msg.send_msg, sizeof(struct pcm_rpmsg_s));
	if (ret) {
		dev_err(pcm_info->dev, "AUDIO : pcm msg workqueue send error\n");
		return;
	}

	spin_lock_irqsave(&pcm_info->wq_lock, flags);
	pcm_info->work_read_index++;
	pcm_info->work_read_index %= WORK_MAX_NUM;
	spin_unlock_irqrestore(&pcm_info->wq_lock, flags);
	dev_dbg(pcm_info->dev, "AUDIO : send workqueue. cmd : %d", work_of_rpmsg->msg.send_msg.param.cmd);
}

int pcm_msg_recv_cb(uint8_t *payload, uint32_t payload_size, void *priv) {
	struct pcm_rpmsg_r *msg = (struct pcm_rpmsg_r *)payload;
	struct pcm_rpmsg *rpmsg;
	int pcm_device;
	struct gua_audio_info *au_info;
	struct pcm_info *pcm_info;
	int ret;
	gua_audio_rpc_data_t *power_msg = (gua_audio_rpc_data_t*)&(((struct ipc_wrapper_rpmsg_r *)payload)->body);

	au_info = (struct gua_audio_info *)priv;
	pcm_info = &au_info->pcm_info;
	pcm_device = msg->param.pcm_device;
	// pcm_device = 0; /* TODO */

	if (SMF_NOTIFICATION != msg->header.type) {
		ret = -EACCES;
		dev_err(pcm_info->dev, "AUDIO : pcm msg callback type error\n");
		return ret;
	}

	if (power_msg->header.group == GROUP_POWER) {
		dev_dbg(pcm_info->dev, "AUDIO : pcm msg: GROUP_POWER, return\n");
		return 0;
	}

	if (msg->param.cmd == PCM_TX_PERIOD_DONE) {
		mutex_lock(&pcm_info->data[pcm_device].pointer_lock[0]);
		/* TODO */
		// if ((msg->param.hw_pointer - pcm_info->data[pcm_device].hw_pointer[0] - 1) % pcm_info->data[pcm_device].periods[0]) {
		// 	dev_warn(pcm_info->dev, "AUDIO : hw_pointer index error\n");
		// }
		pcm_info->data[pcm_device].hw_pointer[0] = msg->param.hw_pointer;

		rpmsg = &pcm_info->data[pcm_device].message[PCM_TX_PERIOD_DONE];
		rpmsg->recv_msg.param.hw_pointer++;
		rpmsg->recv_msg.param.hw_pointer %= pcm_info->data[pcm_device].periods[0];
		mutex_unlock(&pcm_info->data[pcm_device].pointer_lock[0]);
		pcm_info->dma_message_cb(pcm_info->data[pcm_device].substream[0]);
	} else if (msg->param.cmd == PCM_RX_PERIOD_DONE) {
		if (pcm_info->data[pcm_device].periods[1] <= 0) {
			dev_err(pcm_info->dev, "Invalid paramter pcm_device(%d) hw_pointer(%d) periods(%d)\n",
				pcm_device, msg->param.hw_pointer, pcm_info->data[pcm_device].periods[1]);

			return -EINVAL;
		}
		mutex_lock(&pcm_info->data[pcm_device].pointer_lock[1]);
		/* TODO */
		// if ((msg->param.hw_pointer - pcm_info->data[pcm_device].hw_pointer[1] - 1) % pcm_info->data[pcm_device].periods[1]) {
		// 	dev_warn(pcm_info->dev, "AUDIO : hw_pointer index error\n");
		// }
		pcm_info->data[pcm_device].hw_pointer[1] = msg->param.hw_pointer;

		rpmsg = &pcm_info->data[pcm_device].message[PCM_RX_PERIOD_DONE];
		rpmsg->recv_msg.param.hw_pointer++;
		rpmsg->recv_msg.param.hw_pointer %= pcm_info->data[pcm_device].periods[1];
		mutex_unlock(&pcm_info->data[pcm_device].pointer_lock[1]);
		pcm_info->dma_message_cb(pcm_info->data[pcm_device].substream[1]);
	} else {
		dev_err(pcm_info->dev, "AUDIO : receive cmd[%d] error\n", msg->param.cmd);
	}
	dev_dbg(pcm_info->dev, "AUDIO : hardware period done\n");

	return 0;
}

static int gua_cpu_dai_probe(struct platform_device *pdev)
{
	struct gua_audio_info *au_info;
	struct pcm_info *pcm_info;
	int ret;
	int i, j, n, dai_num;
	struct device_node *wrapper_np;
	struct platform_device *wrapper_pdev;
	ipc_wrapper_data_t *wrapper_data;
	unsigned int pcm_poster_mapping[GUA_MAX_PCM_DEVICE] = {0xFFFFFFFF};
	unsigned char mapping_mask = 0x00;

	au_info = devm_kzalloc(&pdev->dev, sizeof(struct gua_audio_info), GFP_KERNEL);
	if (!au_info)
		return -ENOMEM;

	au_info->pdev = pdev;
	pcm_info = &au_info->pcm_info;
	dai_num = ARRAY_SIZE(gua_cpu_dai);

	if (dai_num > GUA_MAX_PCM_DEVICE || dai_num < 0) {
		dev_err(&pdev->dev, "AUDIO : dai link counts error:%d\n", dai_num);
		return -EINVAL;
	}

	/* initialize workqueue */
	pcm_info->rpmsg_wq = alloc_ordered_workqueue("rpmsg_audio", WQ_HIGHPRI | WQ_UNBOUND | WQ_FREEZABLE);
	if (pcm_info->rpmsg_wq == NULL) {
		dev_err(&pdev->dev, "AUDIO : workqueue create failed\n");
		return -ENOMEM;
	}

	wrapper_np = of_parse_phandle(pdev->dev.of_node, "msg-wrapper", 0);
	if (!wrapper_np) {
		dev_err(&pdev->dev, "AUDIO : audio wrapper phandle missing or invalid\n");
		return -EINVAL;
	}
	wrapper_pdev = of_find_device_by_node(wrapper_np);
	if (!wrapper_pdev) {
		dev_err(&pdev->dev, "AUDIO : failed to find audio wrapper device\n");
		return -EINVAL;
	}
	wrapper_data = platform_get_drvdata(wrapper_pdev);
	au_info->au_warpper = wrapper_data;

	for (i = 0; i < GUA_MAX_PCM_DEVICE; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "pcm-rpmsg-channel-mapping", i, &pcm_poster_mapping[i]);
		if (ret) {
			break;
		}
	}
	if (dai_num != i) {
		dev_err(&pdev->dev, "AUDIO : pcm & poster counts not match, dts:%d\n", i);
		return -EINVAL;
	}

	for (i = 0; i < AUDIO_MAX_HBIPC_ID_SHIFT; i++) {
		mapping_mask |= (1<<i);
	}

	for (j = 0; j < dai_num; j++) {
		for (i = 0; i < PCM_CMD_TOTAL_NUM; i++) {
			pcm_info->data[j].message[i].send_msg.header.category = GUA_AUDIO_CATE;
			pcm_info->data[j].message[i].send_msg.header.type = SMF_REQUEST;	/* SYNC CMD */
			pcm_info->data[j].message[i].send_msg.header.payload_len = (uint16_t)sizeof(struct pcm_rpmsg_s);
			pcm_info->data[j].message[i].send_msg.param.pcm_device = j;			/* bind with pcm device */
		}
		pcm_info->data[j].message[PCM_TX_PERIOD_DONE].send_msg.header.type = SMF_NOTIFICATION;		/* PEROID CMD ASYNC */
		pcm_info->data[j].message[PCM_RX_PERIOD_DONE].send_msg.header.type = SMF_NOTIFICATION;		/* PEROID CMD ASYNC */
		pcm_info->data[j].poster_id[0] = mapping_mask & pcm_poster_mapping[j];
		pcm_info->data[j].poster_id[1] = mapping_mask & (pcm_poster_mapping[j]>>AUDIO_MAX_HBIPC_ID_SHIFT);

		for (n = 0; n < 2; n++) {
			pcm_info->data[j].hw_pointer[n] = 0;
			au_info->au_warpper->poster[pcm_info->data[j].poster_id[n]].poster_type = PCM_POSTER;
			au_info->au_warpper->poster[pcm_info->data[j].poster_id[n]].priv_data[1] = au_info;
			au_info->au_warpper->poster[pcm_info->data[j].poster_id[n]].recv_msg_cb[1] = pcm_msg_recv_cb;
		}
		mutex_init(&pcm_info->data[j].pointer_lock[0]);
		mutex_init(&pcm_info->data[j].pointer_lock[1]);
	}

	spin_lock_init(&pcm_info->wq_lock);
	pcm_info->dev = &pdev->dev;

	if (of_device_is_compatible(pdev->dev.of_node, "gua,cpu-dai")) {
		for (i = 0; i < ARRAY_SIZE(gua_cpu_dai); i++)
		{
			gua_cpu_dai[i].playback.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_16000;
			gua_cpu_dai[i].playback.formats = GUA_AUDIO_FORMATS;
			gua_cpu_dai[i].capture.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_16000;
			gua_cpu_dai[i].capture.formats = GUA_AUDIO_FORMATS;
		}
	}

	pcm_info->work_write_index = 1;
	pcm_info->work_read_index = 0;
	for (i = 0; i < WORK_MAX_NUM; i++) {
		INIT_WORK(&pcm_info->work_list[i].work, rpmsg_audio_work);
		pcm_info->work_list[i].au_info = au_info;
	}

	platform_set_drvdata(pdev, au_info);
	pm_runtime_enable(&pdev->dev);

	ret = devm_snd_soc_register_component(&pdev->dev, &gua_cpu_dai_component, gua_cpu_dai, ARRAY_SIZE(gua_cpu_dai));
	if (ret) {
		dev_err(&pdev->dev, "AUDIO : fail to regist cpu-dai component\n");
		return ret;
	}
	dev_info(&pdev->dev, "AUDIO : ALSA cpu-dai driver probe succ\n");

	link_audio_info(au_info);

	return gua_alsa_platform_register(&pdev->dev);
}

static int gua_cpu_dai_remove(struct platform_device *pdev)
{
	struct gua_audio_info *au_info = platform_get_drvdata(pdev);
	struct pcm_info *pcm_info = &au_info->pcm_info;

	if (pcm_info->rpmsg_wq)
		destroy_workqueue(pcm_info->rpmsg_wq);

	return 0;
}

static struct platform_driver gua_cpu_dai_driver = {
	.probe = gua_cpu_dai_probe,
	.remove	= gua_cpu_dai_remove,
	.driver = {
		.name = "cpu_dai_driver",
		.of_match_table = gua_cpu_dai_ids,
	},
};

module_platform_driver(gua_cpu_dai_driver);

MODULE_DESCRIPTION("GuaSemi Soc alsa cpu dai driver Interface");
MODULE_AUTHOR("AUDIO <audio.multimedia@guasemi.com>");
MODULE_LICENSE("GPL");
