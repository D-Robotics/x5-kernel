/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __GUA_AUDIO_IPC_H
#define __GUA_AUDIO_IPC_H

#ifdef FSL_RTOS_FREE_RTOS
#include <stdint.h>
#endif

/**
 * @brief Enumeration defining the Commands of audio pcm messages.
 */
typedef enum {
	PCM_TX_OPEN			= 0x0,		/*!< Command for tx stream pcm open*/
	PCM_TX_START		= 0x1,		/*!< Command for tx stream pcm start*/
	PCM_TX_PAUSE		= 0x2,		/*!< Command for tx stream pcm pause*/
	PCM_TX_RESTART		= 0x3,		/*!< Command for tx stream pcm restart*/
	PCM_TX_STOP			= 0x4,		/*!< Command for tx stream pcm stop*/
	PCM_TX_CLOSE		= 0x5,		/*!< Command for tx stream pcm close*/
	PCM_TX_HW_PARAM		= 0x6,		/*!< Command for tx stream pcm hw param*/
	PCM_TX_BUFFER		= 0x7,		/*!< Command for tx stream pcm set buffer*/
	PCM_TX_SUSPEND		= 0x8,		/*!< Command for tx stream pcm suspend*/
	PCM_TX_RESUME		= 0x9,		/*!< Command for tx stream pcm resume*/
	PCM_TX_PERIOD_DONE	= 0xA,		/*!< Command for tx stream pcm period done*/

	PCM_RX_OPEN			= 0xB,		/*!< Command for rx stream pcm open*/
	PCM_RX_START		= 0xC,		/*!< Command for rx stream  pcm start*/
	PCM_RX_PAUSE		= 0xD,		/*!< Command for rx stream  pcm pause*/
	PCM_RX_RESTART		= 0xE,		/*!< Command for rx stream  pcm restart*/
	PCM_RX_STOP			= 0xF,		/*!< Command for rx stream  pcm stop*/
	PCM_RX_CLOSE		= 0x10,		/*!< Command for rx stream  pcm close*/
	PCM_RX_HW_PARAM		= 0x11,		/*!< Command for rx stream  pcm hw param*/
	PCM_RX_BUFFER		= 0x12,		/*!< Command for rx stream pcm set buffer*/
	PCM_RX_SUSPEND		= 0x13,		/*!< Command for rx stream pcm suspend*/
	PCM_RX_RESUME		= 0x14,		/*!< Command for rx stream pcm resume*/
	PCM_RX_PERIOD_DONE	= 0x15,		/*!< Command for rx stream pcm period done*/

	PCM_CMD_TOTAL_NUM	= 0x16,		/*!< CMD MAX */
} pcm_cmd_enum_t;

/**
 * @brief Enumeration defining the priority levels for SMF messages.
 */
enum {
	SMF_PRIORITY_LOW	= 0,		/**< Low priority level. */
	SMF_PRIORITY_NORMAL	= 1,		/**< Normal priority level. */
	SMF_PRIORITY_URGENT	= 2,		/**< Urgent priority level. */
};

/**
 * @brief Enumeration defining the types for SMF messages.
 */

enum {
	SMF_REQUEST 		= 0x00U,	/*!< Request message */
	SMF_RESPONSE,					/*!< Response message for certain Request */
	SMF_NOTIFICATION,				/*!< Notification message that doesn't require response */
	SMF_PROC 			= 0x40,		/*!< Local procedure */
};

/**
 * @brief Enumeration defining the service category IDs for SMF messages.
 */
enum {
	SMF_CONNECTION_SERVICE_CATEGORY_ID	= 0,	/**< Connection service category ID. */
	SMF_AUDIO_DSP_OUTPUT_SERVICE_ID		= 3,	/**< Audio DSP output service ID. */
};

typedef enum {
	AUDIO_CHANNEL_NONE		= 0x0,
	AUDIO_CHANNEL_MONO		= 0x1,
	AUDIO_CHANNEL_STEREO	= 0x2,
	AUDIO_CHANNEL_5P1		= 0x6,
	AUDIO_CHANNEL_7P1		= 0x8,
	AUDIO_CHANNEL_7P1P4		= 0xC,
} audio_channel_enum_t;

/**
 * @brief Enum representing the audio format types aligned with Linux in the SMF Audio service.
 */
typedef enum {
	AUDIO_FORMAT_I_S16LE = 0U,	/**< Interleaved audio format: 16-bit signed little-endian. */
	AUDIO_FORMAT_I_S24LE = 1U,	/**< Interleaved audio format: 24-bit signed little-endian. */
	AUDIO_FORMAT_I_S32LE = 2U,	/**< Interleaved audio format: 32-bit signed little-endian. */
	AUDIO_FORMAT_NOI_S16LE,		/**< Non-interleaved audio format: 16-bit signed little-endian. */
	AUDIO_FORMAT_NOI_S24LE,		/**< Non-interleaved audio format: 24-bit signed little-endian. */
	AUDIO_FORMAT_NOI_S32LE,		/**< Non-interleaved audio format: 32-bit signed little-endian. */

	AUDIO_FORMAT_I_F16LE,		/**< Interleaved audio format: 16-bit float little-endian. */
	AUDIO_FORMAT_I_F24LE,		/**< Interleaved audio format: 24-bit float little-endian. */
	AUDIO_FORMAT_I_F32LE,		/**< Interleaved audio format: 32-bit float little-endian. */
	AUDIO_FORMAT_NOI_F16LE,		/**< Non-interleaved audio format: 16-bit float little-endian. */
	AUDIO_FORMAT_NOI_F24LE,		/**< Non-interleaved audio format: 24-bit float little-endian. */
	AUDIO_FORMAT_NOI_F32LE,		/**< Non-interleaved audio format: 32-bit float little-endian. */


	AUDIO_FORMAT_I_S16BE,		/**< Interleaved audio format: 16-bit signed big-endian. */
	AUDIO_FORMAT_I_S24BE,		/**< Interleaved audio format: 24-bit signed big-endian. */
	AUDIO_FORMAT_I_S32BE,		/**< Interleaved audio format: 32-bit signed big-endian. */
	AUDIO_FORMAT_NOI_S16BE,		/**< Non-interleaved audio format: 16-bit signed big-endian. */
	AUDIO_FORMAT_NOI_S24BE,		/**< Non-interleaved audio format: 24-bit signed big-endian. */
	AUDIO_FORMAT_NOI_S32BE,		/**< Non-interleaved audio format: 32-bit signed big-endian. */


	AUDIO_FORMAT_I_F16BE,		/**< Interleaved audio format: 16-bit float big-endian. */
	AUDIO_FORMAT_I_F24BE,		/**< Interleaved audio format: 24-bit float big-endian. */
	AUDIO_FORMAT_I_F32BE,		/**< Interleaved audio format: 32-bit float big-endian. */
	AUDIO_FORMAT_NOI_F16BE,		/**< Non-interleaved audio format: 16-bit float big-endian. */
	AUDIO_FORMAT_NOI_F24BE,		/**< Non-interleaved audio format: 24-bit float big-endian. */
	AUDIO_FORMAT_NOI_F32BE,		/**< Non-interleaved audio format: 32-bit float big-endian. */
} audio_format_enum_t;

/**
 * @brief Enum representing the audio sample rate types for SMF.
 */
typedef enum {
	AUDIO_SAMPLERATE_8000 = 0U,	/**< Audio sample rate: 8000 Hz. */
	AUDIO_SAMPLERATE_16000,		/**< Audio sample rate: 16000 Hz. */
	AUDIO_SAMPLERATE_32000,		/**< Audio sample rate: 32000 Hz. */
	AUDIO_SAMPLERATE_44100,		/**< Audio sample rate: 44100 Hz. */
	AUDIO_SAMPLERATE_48000,		/**< Audio sample rate: 48000 Hz. */
	AUDIO_SAMPLERATE_96000,		/**< Audio sample rate: 96000 Hz. */
	AUDIO_SAMPLERATE_192000,	/**< Audio sample rate: 192000 Hz. */
} audio_rate_enum_t;

#pragma pack(push)
#pragma pack(4)

/**
 * @brief SMF communication packet head
 */
typedef struct smf_packet_head {
	uint8_t category;			/*!< SMF category id.Each application should define it's own id in smf.h. */
	uint8_t type;				/*!< SMF message type field. request/notify/response  */
	uint8_t src_id;				/*!< SMF message send core id field */
	uint8_t sink_id;			/*!< SMF message sink core id field */
	uint16_t payload_len;		/*!< SMF message payload len field */
	uint16_t priority;			/*!< SMF message priority field */
} smf_packet_head_t;

/* struct of send message */
struct ipc_wrapper_rpmsg_s {
	smf_packet_head_t header;
	void *body;
};

/* struct of send message */
struct ipc_wrapper_rpmsg_r {
	smf_packet_head_t header;
	void *body;
};

/**
 * @brief Structure defining pcm message for ap to dsp protocol.
 */
typedef struct pcm_msg_a2d {
	uint8_t pcm_device;		/*!< pcm device id field */  
	uint8_t cmd;			/*!< audio service command to execute field */
	uint8_t sw_pointer;		/*!< ap writed DMA index field */
	uint8_t format;			/*!< pcm audio data field */
	uint8_t channels;		/*!< pcm channel&mask field */
	uint8_t reserved[3U];	/*!< reserved field */
	uint32_t sample_rate;	/*!< pcm sample rate field */
	uint32_t buffer_addr;	/*!< DMA buffer begin address field */
	uint32_t buffer_size;	/*!< DMA buffer size field */
	uint32_t period_size;	/*!< period size per call field */
} pcm_msg_a2d_t;

/**
 * @brief Structure defining pcm message for dsp to ap protocol.
 */
typedef struct pcm_msg_d2a {
	uint8_t pcm_device;	/*!< pcm device id field */ 
	uint8_t cmd;		/*!< audio service command to execute field */
	uint8_t hw_pointer;	/*!< dsp hw read index field */
	uint8_t result;		/*!< audio service command to execute field */
} pcm_msg_d2a_t;

typedef struct {
	smf_packet_head_t header;
	void *body;
} ipc_wrapper_rpmsg_r;


#pragma pack(pop)

#endif /* __GUA_AUDIO_IPC_H */
