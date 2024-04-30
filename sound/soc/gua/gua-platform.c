/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>
#include <sound/soc.h>
#include "gua_pcm.h"

extern int32_t sync_pipeline_state(uint16_t pcm_device, uint8_t stream, uint8_t state);

/* make sure : bind with dai_links & cpu_dais */
static struct gua_pcm_hw_info pcms_hw_info[] ={
	/* pcm0 Media */
	{
		.type = 2,
		.playback_hw = {
			.info = SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME,
			.period_bytes_min = 240 * 4, /* 5ms 2ch 16bit */
			.period_bytes_max = 960 * 24, /* 20ms 12ch 16bit */
			.periods_min = 2,
			.periods_max = 4,
			.channels_min = 2,
			.channels_max = 12,
			.fifo_size = 0,
			.buffer_bytes_max = 960 * 24 * 4, /* period_bytes_max * periods_max */
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.rate_min = 16000,
			.rate_max = 48000,
		},
		.capture_hw = {
			.info = SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME,
			.period_bytes_min = 240 * 4, /* 5ms 2ch 16bit */
			.period_bytes_max = 960 * 24, /* 20ms 12ch 16bit */
			.periods_min = 2,
			.periods_max = 4,
			.channels_min = 2,
			.channels_max = 12,
			.fifo_size = 0,
			.buffer_bytes_max = 960 * 24 * 4, /* period_bytes_max * periods_max */
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.rate_min = 16000,
			.rate_max = 48000,
		},
	},
	/* pcm1 VOIP */
	{
		.type = 2,
		.playback_hw = {
			.info = SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME,
			.period_bytes_min = 480 * 2, /* 10ms 1ch 16bit */
			.period_bytes_max = 960 * 4, /* 20ms 2ch 16bit */
			.periods_min = 2,
			.periods_max = 4,
			.channels_min = 1,
			.channels_max = 2,
			.fifo_size = 0,
			.buffer_bytes_max = 960 * 4 * 4, /* period_bytes_max * periods_max */
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.rate_min = 16000,
			.rate_max = 48000,
		},
		.capture_hw = {
			.info = SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME,
			.period_bytes_min = 480 * 2, /* 10ms 1ch 16bit */
			.period_bytes_max = 960 * 12, /* 20ms 6ch 16bit */
			.periods_min = 2,
			.periods_max = 4,
			.channels_min = 1,
			.channels_max = 6,
			.fifo_size = 0,
			.buffer_bytes_max = 960 * 12 * 4, /* period_bytes_max * periods_max */
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.rate_min = 16000,
			.rate_max = 48000,
		},
	},
	/* pcm2 VR */
	{
		.type = 2,
		.playback_hw = {
			.info = SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME,
			.period_bytes_min = 480 * 2, /* 10ms 1ch 16bit */
			.period_bytes_max = 960 * 4, /* 20ms 2ch 16bit */
			.periods_min = 2,
			.periods_max = 4,
			.channels_min = 1,
			.channels_max = 2,
			.fifo_size = 0,
			.buffer_bytes_max = 960 * 4 * 4, /* period_bytes_max * periods_max */
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.rate_min = 16000,
			.rate_max = 48000,
		},
		.capture_hw = {
			.info = SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME,
			.period_bytes_min = 480 * 2, /* 10ms 1ch 16bit */
			.period_bytes_max = 960 * 12, /* 20ms 6ch 16bit */
			.periods_min = 2,
			.periods_max = 4,
			.channels_min = 1,
			.channels_max = 6,
			.fifo_size = 0,
			.buffer_bytes_max = 960 * 12 * 4, /* period_bytes_max * periods_max */
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.rate_min = 16000,
			.rate_max = 48000,
		},
	},
	/* pcm3 ASR */
	{
		.type = 2,
		.playback_hw = {
			.info = SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME,
			.period_bytes_min = 480 * 2, /* 10ms 1ch 16bit */
			.period_bytes_max = 960 * 4, /* 20ms 2ch 16bit */
			.periods_min = 2,
			.periods_max = 4,
			.channels_min = 1,
			.channels_max = 2,
			.fifo_size = 0,
			.buffer_bytes_max = 960 * 4 * 4, /* period_bytes_max * periods_max */
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.rate_min = 16000,
			.rate_max = 48000,
		},
		.capture_hw = {
			.info = SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME,
			.period_bytes_min = 480 * 2, /* 10ms 1ch 16bit */
			.period_bytes_max = 960 * 12, /* 20ms 6ch 16bit */
			.periods_min = 2,
			.periods_max = 4,
			.channels_min = 1,
			.channels_max = 6,
			.fifo_size = 0,
			.buffer_bytes_max = 960 * 12 * 4, /* period_bytes_max * periods_max */
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.rate_min = 16000,
			.rate_max = 48000,
		},
	},
	/* pcm4 HISF */
	{
		.type = 2,
		.playback_hw = {
			.info = SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME,
			.period_bytes_min = 480 * 2, /* 10ms 1ch 16bit */
			.period_bytes_max = 960 * 4, /* 20ms 2ch 16bit */
			.periods_min = 2,
			.periods_max = 4,
			.channels_min = 1,
			.channels_max = 2,
			.fifo_size = 0,
			.buffer_bytes_max = 960 * 4 * 4, /* period_bytes_max * periods_max */
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.rate_min = 16000,
			.rate_max = 48000,
		},
		.capture_hw = {
			.info = SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME,
			.period_bytes_min = 480 * 2, /* 10ms 1ch 16bit */
			.period_bytes_max = 960 * 12, /* 20ms 6ch 16bit */
			.periods_min = 2,
			.periods_max = 4,
			.channels_min = 1,
			.channels_max = 6,
			.fifo_size = 0,
			.buffer_bytes_max = 960 * 12 * 4, /* period_bytes_max * periods_max */
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.rate_min = 16000,
			.rate_max = 48000,
		},
	},
};

static int gua_pcm_hw_params(struct snd_soc_component *component, struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct gua_audio_info *au_info = dev_get_drvdata(cpu_dai->dev);
	struct pcm_info *pcm_info = &au_info->pcm_info;
	audio_poster_t *au_poster;
	struct pcm_rpmsg *rpmsg;
	int pcm_device = substream->pcm->device;
	int ret, id;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		rpmsg = &pcm_info->data[pcm_device].message[PCM_TX_HW_PARAM];
		rpmsg->send_msg.param.cmd = PCM_TX_HW_PARAM;
		id = pcm_info->data[pcm_device].poster_id[0];
	} else {
		rpmsg = &pcm_info->data[pcm_device].message[PCM_RX_HW_PARAM];
		rpmsg->send_msg.param.cmd = PCM_RX_HW_PARAM;
		id = pcm_info->data[pcm_device].poster_id[1];
	}
	au_poster = &au_info->au_warpper->poster[id];

	rpmsg->send_msg.param.sample_rate = params_rate(params);

	if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
		rpmsg->send_msg.param.format = AUDIO_FORMAT_I_S16LE;
	else if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE)
		rpmsg->send_msg.param.format = AUDIO_FORMAT_I_S24LE;
	else
		rpmsg->send_msg.param.format = AUDIO_FORMAT_I_S32LE;

	if (params_channels(params) == 1)
		rpmsg->send_msg.param.channels = AUDIO_CHANNEL_MONO;
	else
		rpmsg->send_msg.param.channels = AUDIO_CHANNEL_STEREO;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = params_buffer_bytes(params);
	rpmsg->send_msg.header.type = SMF_REQUEST;
	rpmsg->send_msg.param.pcm_device = pcm_device;

	dev_info(rtd->dev, "hw params. rate:%d, period-size:%d, periods:%d, buffer-size:%d, dma-bytes:%d\n", params_rate(params), params_period_size(params), params_periods(params), params_buffer_size(params), params_buffer_bytes(params));
	ret = au_info->au_warpper->send_msg(au_poster, (void *)&rpmsg->send_msg, sizeof(struct pcm_rpmsg_s));

	return ret;
}

static int gua_pcm_hw_free(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static snd_pcm_uframes_t gua_pcm_pointer(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct gua_audio_info *au_info = dev_get_drvdata(cpu_dai->dev);
	struct pcm_info *pcm_info = &au_info->pcm_info;
	unsigned int pos = 0;
	struct pcm_rpmsg *rpmsg;
	int pointer = 0;
	int pcm_device = substream->pcm->device;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		rpmsg = &pcm_info->data[pcm_device].message[PCM_TX_PERIOD_DONE];
	} else {
		rpmsg = &pcm_info->data[pcm_device].message[PCM_RX_PERIOD_DONE];
	}

	pointer = rpmsg->recv_msg.param.hw_pointer;
	pos = pointer * snd_pcm_lib_period_bytes(substream);

	return bytes_to_frames(substream->runtime, pos);
}

static void dma_transfer_done(struct snd_pcm_substream *substream)
{
	snd_pcm_period_elapsed(substream);
}

static int gua_pcm_open(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_pcm_hardware pcm_hw_info;
	struct gua_audio_info *au_info = dev_get_drvdata(cpu_dai->dev);
	struct pcm_info *pcm_info = &au_info->pcm_info;
	int pcm_device = substream->pcm->device;
	int cmd;
	int ret, id;

	dev_info(rtd->dev, "AUDIO : pcm open. pcm[%d] stream[%d]\n", substream->pcm->device, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sync_pipeline_state(pcm_device, SNDRV_PCM_STREAM_PLAYBACK, PCM_TX_OPEN);
		pcm_hw_info = pcms_hw_info[substream->pcm->device].playback_hw;
		cmd = PCM_TX_PERIOD_DONE;
		pcm_info->data[pcm_device].message[cmd].send_msg.param.sw_pointer = 0;
		pcm_info->data[pcm_device].message[cmd].recv_msg.param.hw_pointer = 0;
	} else {
		sync_pipeline_state(pcm_device, SNDRV_PCM_STREAM_CAPTURE, PCM_RX_OPEN);
		pcm_hw_info = pcms_hw_info[substream->pcm->device].capture_hw;
		cmd = PCM_RX_PERIOD_DONE;
		pcm_info->data[pcm_device].message[cmd].send_msg.param.sw_pointer = 0;
		pcm_info->data[pcm_device].message[cmd].recv_msg.param.hw_pointer = 0;
		id = pcm_info->data[pcm_device].poster_id[1];
	}
	snd_soc_set_runtime_hwparams(substream, &pcm_hw_info);

	ret = snd_pcm_hw_constraint_integer(substream->runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		return ret;
	}
	pcm_info->data[pcm_device].ack_drop_count[substream->stream] = 0;

	return ret;
}

static int gua_dai_trigger_stop(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct gua_audio_info *au_info = dev_get_drvdata(cpu_dai->dev);
	struct pcm_info *pcm_info = &au_info->pcm_info;
	int cmd;
	int pcm_device = substream->pcm->device;

	dev_info(rtd->dev, "AUDIO : pcm trigger stop\n");

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sync_pipeline_state(pcm_device, SNDRV_PCM_STREAM_PLAYBACK, PCM_TX_STOP);
	} else {
		sync_pipeline_state(pcm_device, SNDRV_PCM_STREAM_CAPTURE, PCM_RX_STOP);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		cmd = PCM_TX_PERIOD_DONE;
		pcm_info->data[pcm_device].message[cmd].send_msg.param.sw_pointer = 0;
		pcm_info->data[pcm_device].message[cmd].recv_msg.param.hw_pointer = 0;
	} else {
		cmd = PCM_RX_PERIOD_DONE;
		pcm_info->data[pcm_device].message[cmd].send_msg.param.sw_pointer = 0;
		pcm_info->data[pcm_device].message[cmd].recv_msg.param.hw_pointer = 0;
	}

	return 0;
}

static int gua_pcm_close(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct gua_audio_info *au_info = dev_get_drvdata(cpu_dai->dev);
	struct pcm_info *pcm_info = &au_info->pcm_info;
	int pcm_device = substream->pcm->device;
	int ret = 0;

	dev_info(rtd->dev, "AUDIO : pcm close\n");

	ret = gua_dai_trigger_stop(component, substream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sync_pipeline_state(pcm_device, SNDRV_PCM_STREAM_PLAYBACK, PCM_TX_CLOSE);
	} else {
		sync_pipeline_state(pcm_device, SNDRV_PCM_STREAM_CAPTURE, PCM_RX_CLOSE);
	}

	flush_workqueue(pcm_info->rpmsg_wq);

	rtd->dai_link->ignore_suspend = 0;

	if (pcm_info->data[pcm_device].ack_drop_count[substream->stream]) {
		dev_warn(rtd->dev, "AUDIO : message is dropped, number is %d\n", pcm_info->data[pcm_device].ack_drop_count[substream->stream]);
	}

	return ret;
}

static int gua_pcm_set_buffer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct gua_audio_info *au_info = dev_get_drvdata(cpu_dai->dev);
	struct pcm_info *pcm_info = &au_info->pcm_info;
	audio_poster_t *au_poster;
	struct pcm_rpmsg *rpmsg;
	int pcm_device = substream->pcm->device;
	int id;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		rpmsg = &pcm_info->data[pcm_device].message[PCM_TX_BUFFER];
		rpmsg->send_msg.param.cmd = PCM_TX_BUFFER;
		id = pcm_info->data[pcm_device].poster_id[0];
	} else {
		rpmsg = &pcm_info->data[pcm_device].message[PCM_RX_BUFFER];
		rpmsg->send_msg.param.cmd = PCM_RX_BUFFER;
		id = pcm_info->data[pcm_device].poster_id[1];
	}
	au_poster = &au_info->au_warpper->poster[id];
	rpmsg->send_msg.param.pcm_device = pcm_device;
	rpmsg->send_msg.param.buffer_addr = substream->runtime->dma_addr;
	rpmsg->send_msg.param.buffer_size = snd_pcm_lib_buffer_bytes(substream);
	rpmsg->send_msg.param.period_size = snd_pcm_lib_period_bytes(substream);
	rpmsg->send_msg.param.sw_pointer = 0;
	pcm_info->data[pcm_device].periods[substream->stream] = rpmsg->send_msg.param.buffer_size / rpmsg->send_msg.param.period_size;

	dev_info(rtd->dev, "AUDIO : pcm set buffer\n");
	return au_info->au_warpper->send_msg(au_poster, (void *)&rpmsg->send_msg, sizeof(struct pcm_rpmsg_s));
}

static int gua_dai_trigger_start(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int pcm_device = substream->pcm->device;

	dev_info(rtd->dev, "AUDIO : pcm trigger start\n");

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sync_pipeline_state(pcm_device, SNDRV_PCM_STREAM_PLAYBACK, PCM_TX_START);
	} else {
		sync_pipeline_state(pcm_device, SNDRV_PCM_STREAM_CAPTURE, PCM_RX_START);
	}

	return 0;
}

static int gua_pcm_prepare(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret;

	dev_info(rtd->dev, "AUDIO : pcm prepare\n");

	if ((runtime->access == SNDRV_PCM_ACCESS_RW_INTERLEAVED ||
		runtime->access == SNDRV_PCM_ACCESS_RW_NONINTERLEAVED)) {
		rtd->dai_link->ignore_suspend = 1;
	} else {
		rtd->dai_link->ignore_suspend = 0;
	}

	ret = gua_pcm_set_buffer(substream);
	ret |= gua_dai_trigger_start(substream);

	return ret;
}

int gua_pcm_trigger(struct snd_soc_component *component, struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret = 0;

	dev_info(rtd->dev, "AUDIO : pcm trigger cmd[%d], with no message to DSP\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

int gua_pcm_ack(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
	/*send the hw_avail size through rpmsg*/
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct gua_audio_info *au_info = dev_get_drvdata(cpu_dai->dev);
	struct pcm_info *pcm_info = &au_info->pcm_info;
	struct pcm_rpmsg *rpmsg;
	int pointer = 0;
	unsigned long flags;
	int pcm_device = substream->pcm->device;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		rpmsg = &pcm_info->data[pcm_device].message[PCM_TX_PERIOD_DONE];
		rpmsg->send_msg.param.cmd = PCM_TX_PERIOD_DONE;
	} else {
		rpmsg = &pcm_info->data[pcm_device].message[PCM_RX_PERIOD_DONE];
		rpmsg->send_msg.param.cmd = PCM_RX_PERIOD_DONE;
	}

	rpmsg->send_msg.header.type = SMF_NOTIFICATION;
	rpmsg->send_msg.param.pcm_device = pcm_device;

	pointer = (frames_to_bytes(runtime, runtime->control->appl_ptr) %
				snd_pcm_lib_buffer_bytes(substream));
	pointer = pointer / snd_pcm_lib_period_bytes(substream);

	if (pointer != rpmsg->send_msg.param.sw_pointer) {
		rpmsg->send_msg.param.sw_pointer = pointer;
		spin_lock_irqsave(&pcm_info->wq_lock, flags);
		if (pcm_info->work_write_index != pcm_info->work_read_index) {
			int index = pcm_info->work_write_index;
			memcpy(&pcm_info->work_list[index].msg, rpmsg, sizeof(struct pcm_rpmsg_s));
			queue_work(pcm_info->rpmsg_wq, &pcm_info->work_list[index].work);
			pcm_info->work_write_index++;
			pcm_info->work_write_index %= WORK_MAX_NUM;
		} else {
			pcm_info->data[pcm_device].ack_drop_count[substream->stream]++;
		}
		spin_unlock_irqrestore(&pcm_info->wq_lock, flags);
	} else {
		dev_err(rtd->dev, "AUDIO : pcm ack error. pointer:%d\n", pointer);
	}
	return 0;
}

static int pcm_buffer_alloc(struct snd_soc_pcm_runtime *rtd, int stream, int device_num)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct gua_audio_info *au_info = dev_get_drvdata(cpu_dai->dev);
	int size, offset;

	if (SNDRV_PCM_STREAM_PLAYBACK == stream) {
		size = pcms_hw_info[device_num].playback_hw.buffer_bytes_max;
		offset = pcms_hw_info[device_num].pb_addr_offset;
	} else {
		size = pcms_hw_info[device_num].capture_hw.buffer_bytes_max;
		offset = pcms_hw_info[device_num].cp_addr_offset;
	}

	buf->dev.type = SNDRV_DMA_TYPE_DEV_WC;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	if ((offset + size) > au_info->mem_info.mem_size) {
		int avil_size = au_info->mem_info.mem_size - offset;
		dev_err(cpu_dai->dev, "no enough share buffer! request-size:%d, avilable-size:%d, total-size:%d\n", size, avil_size, au_info->mem_info.mem_size);
		return -ENOMEM;
	}
	buf->area = au_info->mem_info.mem_addr + offset;
	buf->addr = au_info->mem_info.phy_addr_base + offset;
	if (!buf->area) {
		dev_err(cpu_dai->dev, "alloc pcm buffer-addr error!\n");
		return -ENOMEM;
	}
	buf->bytes = size;

	return 0;
}

static void gua_pcm_free(struct snd_soc_component *component, struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = SNDRV_PCM_STREAM_PLAYBACK;
			stream < SNDRV_PCM_STREAM_LAST; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		buf->area = NULL;
	}
}

static int gua_pcm_new(struct snd_soc_component *component, struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct gua_audio_info *au_info = dev_get_drvdata(cpu_dai->dev);
	int ret;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		au_info->pcm_info.data[pcm->device].substream[0] = pcm->streams[0].substream;
		ret = pcm_buffer_alloc(rtd, 0, pcm->device);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		au_info->pcm_info.data[pcm->device].substream[1] = pcm->streams[1].substream;
		ret = pcm_buffer_alloc(rtd, 1, pcm->device);
		if (ret)
			goto out;
	}

out:
	/* free buffers in case of error */
	if (ret)
		gua_pcm_free(component, pcm);

	return ret;
}

static void pcm_addr_offset_calc(void)
{
	int pcm_num = ARRAY_SIZE(pcms_hw_info);
	int i, offset;

	offset = 0;
	for (i = 0; i < pcm_num; i++) {
		if (0 == pcms_hw_info[i].type) {
			pcms_hw_info[i].pb_addr_offset = offset;
			offset += pcms_hw_info[i].playback_hw.buffer_bytes_max;
		} else if (1 == pcms_hw_info[i].type) {
			pcms_hw_info[i].cp_addr_offset = offset;
			offset += pcms_hw_info[i].capture_hw.buffer_bytes_max;
		} else {
			pcms_hw_info[i].pb_addr_offset = offset;
			offset += pcms_hw_info[i].playback_hw.buffer_bytes_max;
			pcms_hw_info[i].cp_addr_offset = offset;
			offset += pcms_hw_info[i].capture_hw.buffer_bytes_max;
		}
	}
}

static struct snd_soc_component_driver gua_soc_component = {
	.name		= "platform-component",
	.open		= gua_pcm_open,
	.close		= gua_pcm_close,
	// .ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= gua_pcm_hw_params,
	.hw_free	= gua_pcm_hw_free,
	.trigger	= gua_pcm_trigger,
	.pointer	= gua_pcm_pointer,
	.ack		= gua_pcm_ack,
	.prepare	= gua_pcm_prepare,
	.pcm_construct	= gua_pcm_new,
	.pcm_destruct	= gua_pcm_free,
};

int gua_alsa_platform_register(struct device *dev)
{
	struct gua_audio_info *au_info = dev_get_drvdata(dev);
	struct snd_soc_component *component;
	struct pcm_info *p_info = &au_info->pcm_info;
	int ret;

	p_info->dma_message_cb = dma_transfer_done;

	pcm_addr_offset_calc();

	ret = devm_snd_soc_register_component(dev, &gua_soc_component, NULL, 0);
	if (ret)
		return ret;

	component = snd_soc_lookup_component(dev, "platform-component");
	if (!component)
		return -EINVAL;

	dev_info(dev, "AUDIO : ALSA platform driver probe succ\n");

	return 0;
}
EXPORT_SYMBOL_GPL(gua_alsa_platform_register);
MODULE_LICENSE("GPL");
