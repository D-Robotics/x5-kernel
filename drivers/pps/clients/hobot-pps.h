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

	struct timer_list pps_timer;
	unsigned long pps_counter;
};

#endif //DRIVERS_PPS_CLIENTS_HOBOT_PPS_H_
