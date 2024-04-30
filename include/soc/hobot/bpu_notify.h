/*
 * Copyright (C) 2020 Horizon Robotics
 *
 * Zhang Guoying <guoying.zhang@horizon.ai>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */
#ifndef __BPU_NOTIFY_H__
#define __BPU_NOTIFY_H__

#include <linux/types.h>
#include <linux/notifier.h>

enum bpu_notify_action {
	/* bpu power action, 1:on; 0:off*/
	POWER_OFF = 0,
	POWER_ON,

	ACTION_MAX,
};

int32_t bpu_notifier_register(struct notifier_block *nb);
int32_t bpu_notifier_unregister(struct notifier_block *nb);

#endif
