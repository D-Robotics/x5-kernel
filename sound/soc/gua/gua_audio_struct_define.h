/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __GUA_AUDIO_STRUCT_DEFINE_H
#define __GUA_AUDIO_STRUCT_DEFINE_H

/*!
 * @file gua_audio_struct_define.h
 * @addtogroup gua_audio_struct_define
 * @{
 */

typedef struct gua_audio_eq {
	uint8_t band;
	int8_t gain;
} gua_audio_eq_t;

typedef enum {
	AUDIO_POWER_TO_NORMAL = 0,
	AUDIO_POWER_IN_NORMAL,
	AUDIO_POWER_TO_LOW_POWER,
	AUDIO_POWER_IN_LOW_POWER,
	AUDIO_POWER_TO_AWAKE_1,
	AUDIO_POWER_IN_AWAKE_1,
	AUDIO_POWER_TO_AWAKE_2,
	AUDIO_POWER_IN_AWAKE_2,
	AUDIO_POWER_TO_SHUTDOWN,
	AUDIO_POWER_IN_SHUTDOWN,
} audio_power_state_t;

typedef struct _gua_audio_power {
	uint8_t state; /*< @see audio_power_state_t */
} gua_audio_power_t;

/*! @} */
#endif /* __GUA_AUDIO_STRUCT_DEFINE_H */
