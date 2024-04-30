
/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "gua_pcm.h"

struct gua_audio_info *g_audio_info;

void link_audio_info(struct gua_audio_info *audio_info)
{
	g_audio_info = audio_info;
}
EXPORT_SYMBOL(link_audio_info);

int32_t sync_pipeline_state(uint16_t pcm_device, uint8_t stream, uint8_t state)
{
	struct pcm_rpmsg *rpmsg = NULL;
	audio_poster_t *au_poster = NULL;
	struct pcm_info *pcm_info = &g_audio_info->pcm_info;
	int id = 0;

	dev_info(NULL, "%s enter\n", __func__);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		id = pcm_info->data[pcm_device].poster_id[0];
	} else {
		id = pcm_info->data[pcm_device].poster_id[1];
	}
	au_poster = &g_audio_info->au_warpper->poster[id];
	rpmsg = &pcm_info->data[pcm_device].message[state];
	rpmsg->send_msg.header.type = SMF_REQUEST;
	rpmsg->send_msg.param.cmd = state;
	rpmsg->send_msg.param.pcm_device = pcm_device;

	return g_audio_info->au_warpper->send_msg(au_poster,
												(void *)&rpmsg->send_msg,
												sizeof(struct pcm_rpmsg_s));
}
EXPORT_SYMBOL(sync_pipeline_state);

MODULE_AUTHOR("GuaSemi, Inc.");
MODULE_DESCRIPTION("GuaSemi audio driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-rpmsg");
