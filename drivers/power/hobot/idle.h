// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef __IDLE_H__
#define __IDLE_H__

#include <linux/device.h>

/**
 * @brief - get idle device
 *
 * @param of_node the of node contans "idle-dev"
 */
struct device *drobot_idle_get_dev(const struct device_node *of_node);

/**
 * @brief - send idle request
 *
 * @param dev idle device pointer
 * @param idle_id idle item index
 * @param idle request state
 */
int drobot_idle_request(struct device *dev, u8 idle_id, bool idle);

#endif
