/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#ifndef _LINUX_SOC_HOBOT_X5_SEAMLESS_DISPLAY_H
#define _LINUX_SOC_HOBOT_X5_SEAMLESS_DISPLAY_H

#include <linux/kconfig.h>
#include <linux/types.h>

bool x5_seamless_display_active(void);

#if IS_ENABLED(CONFIG_X5_SEAMLESS_DISPLAY)
/**
 * x5_chosen_has_simple_framebuffer - True if /chosen has a simple-framebuffer child.
 * U-Boot seamless handoff injects this when the FB reserve path succeeds; it may be
 * present even when d-robotics,seamless-display-state was not set (env/FDT skew).
 */
bool x5_chosen_has_simple_framebuffer(void);
#endif

#endif /* _LINUX_SOC_HOBOT_X5_SEAMLESS_DISPLAY_H */
