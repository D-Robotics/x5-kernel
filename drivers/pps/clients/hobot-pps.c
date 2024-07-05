/*
 * pps-gpio.c -- PPS client driver using GPIO
 *
 * Copyright (C) 2020 Hobot Robotics. 
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/timer.h>
#include <linux/jiffies.h>
#include "hobot-pps.h"
#define HOBOT_PPS_NAME "hobot-pps"

//#define REAL_PPS_ENABLE
#define TIMER_INTERVAL 1

/*
 * Report the PPS event
 */
#ifdef REAL_PPS_ENABLE
static irqreturn_t hobot_pps_irq_handler(int32_t irq, void *pps_dev_data)
{
	struct hobot_pps_device_data *data;
	struct pps_event_time ts;

	/* Get the time stamp first */
	pps_get_ts(&ts);

	data = (struct hobot_pps_device_data *)pps_dev_data;

	pps_event(data->pps, &ts, PPS_CAPTUREASSERT, NULL);

	return IRQ_HANDLED;
}

static int32_t hobot_pps_pinctrl_set(struct platform_device *pdev)
{
	struct pinctrl_state *pin_pps_mux;
	struct pinctrl *pinctrl;
	int32_t ret = 0;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if ( IS_ERR((void *)pinctrl) ) {
		dev_err(&pdev->dev, "devm_pinctrl_get error\n");
		return -EPROBE_DEFER;
	}

	pin_pps_mux = pinctrl_lookup_state(pinctrl, "default");
	if ( IS_ERR((void *)pin_pps_mux) ) {
		dev_err(&pdev->dev, "pin_pps_mux in pinctrl state error\n");
		return -EPROBE_DEFER;
	}
	ret = pinctrl_select_state(pinctrl, pin_pps_mux);
	if (ret) {
	    dev_err(&pdev->dev, "pinctrl_select_state error\n");
	    return ret;
	}
	return ret;
}
#else
static void pps_timer_callback(struct timer_list *timer) {
	struct hobot_pps_device_data *data = from_timer(data, timer, pps_timer);
	struct pps_event_time ts;

	pps_get_ts(&ts);

	pps_event(data->pps, &ts, PPS_CAPTUREASSERT, NULL);
	data->pps_counter++;

	mod_timer(&data->pps_timer, jiffies + TIMER_INTERVAL * HZ);
}
#endif

/**
 * E1:There will be no errors, so no return value is required.
 */
static void hobot_pps_source_info_init(const struct platform_device *pdev,
	struct hobot_pps_device_data *data)
{
	/* initialize PPS specific parts of the bookkeeping data structure. */
//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS]
	data->info.mode = (uint32_t)PPS_CAPTUREASSERT | (uint32_t)PPS_OFFSETASSERT |
		(uint32_t)PPS_ECHOASSERT | (uint32_t)PPS_CANWAIT | (uint32_t)PPS_TSFMT_TSPEC;
	if (data->capture_clear) {
//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS]
//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS]
		data->info.mode |= (uint32_t)PPS_CAPTURECLEAR | (uint32_t)PPS_OFFSETCLEAR |  /*PRQA S 1821,4446*/ /*Linux struct*/
			(uint32_t)PPS_ECHOCLEAR;
	}
	data->info.owner = THIS_MODULE;
	(void)snprintf(data->info.name, PPS_MAX_NAME_LEN - 1, "%s.%d",
			pdev->name, pdev->id);
}

static int32_t hobot_pps_register_source(struct platform_device *pdev,
		struct hobot_pps_device_data *data)
{
	uint32_t pps_default_params;
	int32_t ret = 0;
#ifdef CONFIG_PREEMPT_RT
	struct irq_desc *desc;
	struct irqaction *action;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};
#endif

	pps_default_params = (uint32_t)PPS_CAPTUREASSERT | (uint32_t)PPS_OFFSETASSERT;
	if (data->capture_clear) {
		pps_default_params |= (uint32_t)PPS_CAPTURECLEAR | (uint32_t)PPS_OFFSETCLEAR;
	}
	data->pps = pps_register_source(&data->info, (int32_t)pps_default_params);
	if (data->pps == NULL) {
		dev_err(&pdev->dev, "failed to register IRQ %d as PPS source\n",
			data->irq);
		return -EINVAL;
	}
	dev_info(&pdev->dev, "register PPS source name:%s\n",data->info.name);

	/* register IRQ interrupt handler */
#ifdef REAL_PPS_ENABLE
	ret = devm_request_irq(&pdev->dev, (uint32_t)data->irq, hobot_pps_irq_handler,
			IRQF_TRIGGER_RISING, data->info.name, (void *)data);
	if (ret != 0) {
		pps_unregister_source(data->pps);
		dev_err(&pdev->dev, "failed to request IRQ %d\n", data->irq);
		return -EINVAL;
	}
#endif

#ifdef CONFIG_PREEMPT_RT
	desc = irq_to_desc((u32)data->irq);
	action = desc->action;
	if (action->thread != NULL) {
		ret = sched_setscheduler(action->thread, SCHED_FIFO, &param);
		if (ret < 0) {
			dev_err(&pdev->dev, "sched_setscheduler is fail(%d)\n", ret);
		}
	}
#endif
	return ret;
}

static int32_t hobot_pps_probe(struct platform_device *pdev)
{
	struct hobot_pps_device_data *data;
	int32_t ret = 0;

#ifdef REAL_PPS_ENABLE
	ret = hobot_pps_pinctrl_set(pdev);
	if (ret) {
		return ret;
	}
#endif

	/* allocate space for device info */
	data = (struct hobot_pps_device_data *)devm_kzalloc(&pdev->dev, sizeof(struct hobot_pps_device_data),
			GFP_KERNEL);
	if (data == NULL) {
		dev_err(&pdev->dev, "devm_kzalloc error\n");
		return -ENOMEM;
	}

#ifdef REAL_PPS_ENABLE
	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0) {
		dev_err(&pdev->dev, "platform_get_irq error:%d\n", data->irq);
		return data->irq;
	}
#else
	timer_setup(&data->pps_timer, pps_timer_callback, 0);
	mod_timer(&data->pps_timer, jiffies + TIMER_INTERVAL * HZ);
#endif

	hobot_pps_source_info_init(pdev, data);

	ret = hobot_pps_register_source(pdev, data);
	if (ret) {
		dev_info(data->pps->dev, "hobot_pps_register_source error\n");
	}

	platform_set_drvdata(pdev, (void *)data);
	dev_info(data->pps->dev, "Registered IRQ %d as PPS source\n",
		 data->irq);

	return ret;
}

static int32_t hobot_pps_remove(struct platform_device *pdev)
{
	struct hobot_pps_device_data *data = (struct hobot_pps_device_data *)platform_get_drvdata(pdev);

	pps_unregister_source(data->pps);
#ifndef REAL_PPS_ENABLE
	del_timer(&data->pps_timer);
#endif
	dev_info(&pdev->dev, "removed IRQ %d as PPS source\n", data->irq);
	return 0;
}

static const struct of_device_id hobot_pps_dt_ids[] = {
	{ .compatible = "hobot-pps", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, hobot_pps_dt_ids);/*PRQA S 0605*/

static struct platform_driver hobot_pps_driver = {
	.probe		= hobot_pps_probe,
	.remove		= hobot_pps_remove,
	.driver		= {
		.name	= HOBOT_PPS_NAME,
		.of_match_table	= hobot_pps_dt_ids,
	},
};

module_platform_driver(hobot_pps_driver);/*PRQA S 0307,0605*/
MODULE_AUTHOR("James Nuss <jamesnuss@nanometrics.ca>");
MODULE_DESCRIPTION("Use AP_SYNC and GPS_PPS pin as PPS source");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
