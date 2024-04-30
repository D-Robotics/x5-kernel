/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 */

#ifndef _gc_hal_kernel_platform_x5_h_
#define _gc_hal_kernel_platform_x5_h_

#if gcdSUPPORT_DEVICE_TREE_SOURCE
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include "gc_hal.h"
#include "gc_hal_driver.h"
#include "gc_hal_kernel.h"
#include "gc_hal_kernel_platform.h"

struct gcsPOWER_DOMAIN {
	struct generic_pm_domain base;
	gceCORE core_id;
	gctUINT32 flags;
	struct platform_device *pdev;
};

struct gcs_devfreq_opp {
	u64 opp_freq;
	u32 opp_volts;   /* min voltage, max voltage*/
	u64 core_mask;      /* not used for gpu, for dts extension */
};

#endif
#endif
