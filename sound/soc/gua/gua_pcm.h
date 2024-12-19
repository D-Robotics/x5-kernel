/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __GUA_AUDIO_PCM_H
#define __GUA_AUDIO_PCM_H

#include <linux/pm_qos.h>
#include <linux/interrupt.h>
#include <sound/dmaengine_pcm.h>
#include "ipc_wrapper.h"
#include "gua_audio_ipc.h"

/*!
 * @file gua_pcm.h
 * @addtogroup audio_pcm
 * @{
 */

#define WORK_MAX_NUM				0x10
#define	GUA_MAX_PCM_DEVICE			32
#define GUA_AUDIO_CATE				SMF_AUDIO_DSP_OUTPUT_SERVICE_ID
#define AUDIO_SUBSTEAM_TYPE_NUM		2

/* struct of send message */
struct pcm_rpmsg_s {
	smf_packet_head_t header;
	pcm_msg_a2d_t param;
};

/* struct of receive message */
struct pcm_rpmsg_r {
	smf_packet_head_t header;
	pcm_msg_d2a_t param;
};

/* struct of pcm message */
struct pcm_rpmsg {
	struct pcm_rpmsg_s send_msg;
	struct pcm_rpmsg_r recv_msg;
};

/**
 * @brief Structure defining workqueue data.
 * 
 * It contains GUA audio information, message(to send) and work task.
 */
struct work_of_rpmsg {
	struct gua_audio_info *au_info;		/*!< cpu-dai platform device data */
	struct pcm_rpmsg msg;				/*!< message for work to send*/
	struct work_struct work;			/*!< work task */
};


/**
 * @brief Structure defining workqueue data.
 *
 * It contains work task, type and private data.
 */
struct gua_work {
	struct work_struct work;	/*!< work task */
	int task_type;			/*!< work task type */
	void *data;			/*!< work private data */
};

/**
 * @brief Structure defining PCM data.
 * 
 * PCM data records its peroids, binded poster id, up/down substreams, r/w hw_pointers and ect.
 */
struct pcm_device_data {
	struct snd_pcm_substream *substream[AUDIO_SUBSTEAM_TYPE_NUM];	/*!< substream pointer of up/down streams */
	int periods[AUDIO_SUBSTEAM_TYPE_NUM];							/*!< periods of up/down streams */
	int poster_id[AUDIO_SUBSTEAM_TYPE_NUM];							/*!< sposter id of up/down streams */
	int hw_pointer[AUDIO_SUBSTEAM_TYPE_NUM];						/*!< hifi read pointer of up/down streams */
	int ack_drop_count[AUDIO_SUBSTEAM_TYPE_NUM];					/*!< ack error counts of up/down streams */
	struct mutex pointer_lock[AUDIO_SUBSTEAM_TYPE_NUM];				/*!< hifi read pointer lock of up/down streams */
	struct pcm_rpmsg message[PCM_CMD_TOTAL_NUM];					/*!< message array of PCM device */
};

/**
 * @brief Structure defining PCM data.
 * 
 * PCM information records all the sound card related PCM matters.
 * PCM information is binded with ALSA cpu-dai driver, 
 * and has a workqueue to send ASYNC message with its r/w index and spinlock.
 * In Pcm information, every PCM device has one PCM data.
 */
struct pcm_info {
	struct device *dev;								/*!< for print debug */
	struct workqueue_struct *rpmsg_wq;				/*!< workqueue for sending ASYNC message */
	struct work_of_rpmsg work_list[WORK_MAX_NUM];	/*!< worklist for sending */
	struct workqueue_struct *gua_wq;	/*!< workqueue for trigger handler */
	struct gua_work *gua_work;	/*!< work info for trigger */
	int work_write_index;							/*!< workueue work in index */
	int work_read_index;							/*!< workueue work out index */
	spinlock_t wq_lock;								/*!< workueue index lock */
	struct pcm_device_data data[GUA_MAX_PCM_DEVICE];	/*!< PCM info data(up/down) */
	void (*dma_message_cb)(struct snd_pcm_substream *substream);	/*!< message receive callback */
};

/**
 * @brief Structure defining audio share memory information.
 * 
 * PCM data records its virtual memory address, memory size and physical memory address.
 */
struct audio_mem_info {
	unsigned char *mem_addr;	/*!< virtual address of share memory for A core <-> DSP */
	int mem_size;				/*!< share memory size */
	int phy_addr_base;			/*!< physical address of share memory */
};

/**
 * @brief Structure defining driver data for cpu-dai platform device.
 * 
 * GUA audio information records PCM information, memory information and ipc_wrapper device data pointer.
 */
struct gua_audio_info {
	struct platform_device *pdev;		/*!< cpu-dai platform device data */
	struct pcm_info pcm_info;			/*!< PCM information in sound card */
	struct audio_mem_info mem_info;		/*!< share memory information */
	ipc_wrapper_data_t *au_warpper;	/*!< pointer of ipc_wrapper platform device data */

	char *tmp_buf; /*!< tmp_buf which used buffer dump */
};

/**
 * @brief Structure defining PCM hardware information.
 */
struct gua_pcm_hw_info {
	int type;								/*!< 0:playback only, 1:capture onle, 2:both support */
	struct snd_pcm_hardware playback_hw;	/*!< playback hardware information */
	int pb_addr_offset;						/*!< playback memory address offset */
	struct snd_pcm_hardware capture_hw;		/*!< capture hardware information */
	int cp_addr_offset;						/*!< capture memory address offset */
};

int gua_alsa_platform_register(struct device *dev);

/*! @} */
#endif /* __GUA_AUDIO_PCM_H */
