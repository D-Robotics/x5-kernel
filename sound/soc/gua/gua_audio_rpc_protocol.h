/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __GUA_AUDIO_RPC_PROTOCOL_H
#define __GUA_AUDIO_RPC_PROTOCOL_H

/*!
 * @file gua_audio_rpc_protocol.h
 * @addtogroup gua_audio_rpc_protocol
 * @{
 */

typedef enum GUA_AUDIO_RPC_GROUP {
	GROUP_MIN = 0,
	GROUP_PCM = GROUP_MIN,
	GROUP_POWER = 253,
	GROUP_CONTROL = 254,
	GROUP_MAX,
} gua_audio_rpc_group_t;

typedef enum GUA_AUDIO_RPC_DIRECTION {
	DIRECTION_MIN = 0,
	DIRECTION_A_TO_HIFI0 = DIRECTION_MIN,
	DIRECTION_HIFI0_TO_A,
	DIRECTION_MAX,
} gua_audio_rpc_direction_t;

#define SYNC true

typedef enum GUA_AUDIO_RPC_TYPE {
	TYPE_MIN = 0,
	TYPE_NOTIFY = TYPE_MIN,
	TYPE_REQUEST,
	TYPE_RESPONSE,
	TYPE_MAX,
} gua_audio_rpc_type_t;

typedef enum GUA_AUDIO_RPC_PRIORITY {
	PRIORITY_MIN = 0,
	PRIORITY_LOW = PRIORITY_MIN,
	PRIORITY_MIDDLE,
	PRIORITY_HIGH,
	PRIORITY_MAX,
} gua_audio_rpc_priority_t;

typedef struct gua_audio_rpc_header {
	uint8_t group;
	uint8_t direction;
	uint8_t sync;
	uint8_t type;
	uint8_t priority;
	uint8_t serial_id;
	uint32_t uuid;
	uint8_t payload_size;
} gua_audio_rpc_header_t;

typedef struct gua_audio_rpc_data {
	gua_audio_rpc_header_t header;
	uint8_t body[0];
} gua_audio_rpc_data_t;

// TODO define where?
#define UUID_CURRENT 0xFFFFFFF
#define UUID_EQ 0x21212121
#define UUID_POWER 0x14000400

#define GUA_MISC_MSG_MAX_SIZE 256U

/*! @} */
#endif /* __GUA_AUDIO_RPC_PROTOCOL_H */
