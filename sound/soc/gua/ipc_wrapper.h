/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __ipc_wrapper_H
#define __ipc_wrapper_H

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/sched/signal.h>

/*!
 * @file ipc_wrapper.h
 * @addtogroup ipc_wrapper
 * @{
 */

#define AUDIO_HBIPC_INSTANCE_ID_0 		0
#define AUDIO_HBIPC_INSTANCE_ID_1 		1
#define AUDIO_HBIPC_INSTANCE_ID_2 		2
#define AUDIO_HBIPC_CHANNEL_ID_0		0
#define AUDIO_HBIPC_CHANNEL_ID_1		1

#define AUDIO_MAX_HBIPC_ID_SHIFT		4
#define AUDIO_MAX_HBIPC_POSTER_NUM		(1<<AUDIO_MAX_HBIPC_ID_SHIFT)

#define AUDIO_HBIPC_RESPOND_TIMEOUT		1000

#define AUDIO_MAX_HBIPC_RECV		5

/**
 * @brief Enumeration defining the poster type for audio message.
 */
typedef enum poster {
	PCM_POSTER = 0,		/*!< poster for pcm device */
	CTLR_POSTER,		/*!< poster for audio comtrol */
	TOTAL_NUM
} poster_t;

/**
 * @brief Structrue defining the poster for audio message.
 * 
 * The poster has a waker for its sync message, and private pointer for its callback.
 */
typedef struct audio_poster {
	struct platform_device *pdev;
	uint32_t ipc_inst_id;			/*!< instance id instead of instance pointer of ipc instance */

	void *priv_data[AUDIO_MAX_HBIPC_RECV];				/*!< message receive callback private data pointer */
	poster_t poster_type;			/*!< poster type */
	wait_queue_head_t waker;		/*!< waker for sync message */
	struct mutex send_lock;			/*!< lock for send message */
	bool sync_response;				/*!< response of sync message */
	bool registed;					/*!< poster registed or not, true/false */
	int poster_id;					/*!< poster index, bind with HBIPC channel */
	int (*recv_msg_cb[AUDIO_MAX_HBIPC_RECV])(uint8_t *payload, uint32_t payload_size, void *priv);	/*!< callback function of receive message */
} audio_poster_t;

/**
 * @brief Structrue defining the driver data for ipc_wrapper device.
 * 
 * The wrapper has a send_msg function for the whole audio driver(all posters).
 */
typedef struct ipc_wrapper_data {
	struct platform_device *pdev;						/*!< audio wrapper platform device pointer */
	bool linked_up;
	uint32_t inst_id;
	struct workqueue_struct *hbipc_wq;
	struct work_struct work;
	audio_poster_t poster[AUDIO_MAX_HBIPC_POSTER_NUM];	/*!< own audio posters */
	int (*send_msg)(audio_poster_t *au_poster, uint8_t *payload, uint32_t payload_size);	/*!< function for sending message for all posters */
} ipc_wrapper_data_t;

/*! @} */
#endif /* __ipc_wrapper_H */
