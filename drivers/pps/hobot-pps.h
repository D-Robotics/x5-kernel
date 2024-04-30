/*
*Copyright (C) 2020 Hobot Robotics.
*/
#ifndef DRIVERS_PPS_CLIENTS_HOBOT_PPS_H_
#define DRIVERS_PPS_CLIENTS_HOBOT_PPS_H_
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/pps_kernel.h>
#include <linux/pinctrl/consumer.h>
#include <uapi/linux/sched/types.h>

#if defined(CONFIG_HOBOT_FUSA_DIAG)
#include <linux/hobot_diag.h>
#endif
#ifdef CONFIG_PPS_STL
#include "./hobot-pps-stl.h"
#endif

#define MASK_8BIT 0xffu
#define BIT_8 8u


/* Info for each registered platform device */
struct hobot_pps_device_data {
	int32_t irq;			/* IRQ used as PPS source */
	struct pps_device *pps;		/* PPS source device */
	struct pps_source_info info;	/* PPS source information */
	bool assert_falling_edge;
	bool capture_clear;
	uint32_t gpio_pin;
	uint16_t safety_moduleId;
#ifdef CONFIG_PPS_STL
	struct pps_stl_ops *hobot_pps_stl_ops;
	struct tasklet_struct hobot_pps_stl_tasklet;
#endif
};

#endif //DRIVERS_PPS_CLIENTS_HOBOT_PPS_H_
