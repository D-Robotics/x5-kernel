/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __GUA_MISC_DRV_H__
#define __GUA_MISC_DRV_H__

#include <linux/ioctl.h>

/* ioctl cmd */
// TODO: find more formal defs
#define GUA_MISC_MAGIC "g"

#define GUA_MISC_IOCTL_CMD_READADDR 0x1234
#define GUA_MISC_IOCTL_CMD_READSIZE 0x5678
#define GUA_MISC_IOCTL_CMD_LOGDUMP 0xa5a5
#define GUA_MISC_IOCTL_CMD_DATADUMP 0x5a5a
typedef struct audio_debug_info {
	uint8_t core_id;
	uint32_t phy_mem_size;
	uint32_t phy_mem_addr; // dsp only use 32bit phy addr
} audio_debug_info_t;

#endif
