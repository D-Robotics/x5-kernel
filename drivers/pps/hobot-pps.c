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
#include "hobot-pps.h"
#define HOBOT_PPS_NAME "hobot-pps"

#if defined(CONFIG_HOBOT_FUSA_DIAG)
void pps_send_diag_error_event(u16 id, u8 sub_id, u16 module_id, u16 line_num)
{
	s32 ret;
	struct diag_event event;

	memset(&event, 0, sizeof(event));
	event.module_id = module_id;
	event.event_prio = (u8)DiagMsgLevel1;
	event.event_sta = (u8)DiagEventStaFail;
	event.event_id = id;
	event.fchm_err_code = FUSA_SW_ERR_CODE;
	event.env_len = FUSA_ENV_LEN;
	event.payload[0] = sub_id;
	event.payload[1] = 0;
	event.payload[2] = (u8)(line_num & MASK_8BIT);
	event.payload[3] = (u8)(line_num >> BIT_8);
	ret = diagnose_send_event(&event);
	if (ret != 0)
		pr_err("%s fail %d\n", __func__, ret);
}
#else
void pps_send_diag_error_event(u16 id, u8 sub_id, u8 pri_data, u16 line_num)
{
	/* NULL */
}
#endif

/**
 * @NO{S21E02C03U}
 * @brief pps irq handler
 *
 * @param[in] irq: irq number
 * @param[in] *pps_dev_data: pps device data
 *
 * @retval "= 0": success
 * @retval <0: failure
 *
 * @data_read None
 * @data_updated None
 *
 * @compatibility HW: J6
 * @compatibility SW: v1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
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

/**
 * @NO{S21E02C03U}
 * @brief pps pinctrl set
 *
 * @param[in] *pdev: pointer of platform_device
 *
 * @retval "= 0": success
 * @retval <0: failure
 *
 * @data_read None
 * @data_updated None
 *
 * @compatibility HW: J6
 * @compatibility SW: v1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
#if 0
//static int32_t hobot_pps_pinctrl_set(struct platform_device *pdev)
//{
//	struct pinctrl_state *pin_pps_mux;
//	struct pinctrl *pinctrl;
//	int32_t ret;
//	//struct hobot_pps_device_data *data = (struct hobot_pps_device_data *)platform_get_drvdata(pdev);

//	pinctrl = devm_pinctrl_get(&pdev->dev);
//	if ( IS_ERR((void *)pinctrl) ) {
//		dev_err(&pdev->dev, "devm_pinctrl_get error\n");
//		//pps_send_diag_error_event((u16)EventIdPinctrlErr, (u8)ERR_SEQ_0, data->safety_moduleId, __LINE__);
//		return EPROBE_DEFER;
//	}

//	pin_pps_mux = pinctrl_lookup_state(pinctrl, "default");
//	if ( IS_ERR((void *)pin_pps_mux) ) {
//		dev_err(&pdev->dev, "pin_pps_mux in pinctrl state error\n");
//		//pps_send_diag_error_event((u16)EventIdPinctrlErr, (u8)ERR_SEQ_1, data->safety_moduleId, __LINE__);
//		return EPROBE_DEFER;
//	}
//	ret = pinctrl_select_state(pinctrl, pin_pps_mux);
//	if (ret != 0) {
//	    dev_err(&pdev->dev, "pinctrl_select_state error\n");
//		//pps_send_diag_error_event((u16)EventIdPinctrlErr, (u8)ERR_SEQ_2, data->safety_moduleId, __LINE__);
//	    return ret;
//	}
//	return 0;
//}
#endif
/**
 * E1:There will be no errors, so no return value is required.
 */
/**
 * @NO{S21E02C03U}
 * @brief pps source info init
 *
 * @param[in] *pdev: pointer of platform_device
 * @param[in] *data: pointer of hobot_pps_device_data
 *
 * @retval "= 0": success
 * @retval <0: failure
 *
 * @data_read None
 * @data_updated None
 *
 * @compatibility HW: J6
 * @compatibility SW: v1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static void hobot_pps_source_info_init(const struct platform_device *pdev,
	struct hobot_pps_device_data *data)
{
	/* initialize PPS specific parts of the bookkeeping data structure. */
	data->info.mode = (uint32_t)PPS_CAPTUREASSERT | (uint32_t)PPS_OFFSETASSERT |
		(uint32_t)PPS_ECHOASSERT | (uint32_t)PPS_CANWAIT | (uint32_t)PPS_TSFMT_TSPEC;
	if (data->capture_clear) {
		data->info.mode |= (uint32_t)PPS_CAPTURECLEAR | (uint32_t)PPS_OFFSETCLEAR |  /*PRQA S 1821,4446*/ /*Linux struct*/
			(uint32_t)PPS_ECHOCLEAR;
	}
	data->info.owner = THIS_MODULE;
	(void)snprintf(data->info.name, PPS_MAX_NAME_LEN - 1, "%s.%d",
			pdev->name, pdev->id);
}

/**
 * @NO{S21E02C03U}
 * @brief pps register source
 *
 * @param[in] *pdev: pointer of platform_device
 * @param[in] *data: pointer of hobot_pps_device_data
 *
 * @retval "= 0": success
 * @retval <0: failure
 *
 * @data_read None
 * @data_updated None
 *
 * @compatibility HW: J6
 * @compatibility SW: v1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t hobot_pps_register_source(struct platform_device *pdev,
		struct hobot_pps_device_data *data)
{
	uint32_t pps_default_params;
	int32_t ret;
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
		//pps_send_diag_error_event((u16)EventIdRegisterErr, (u8)ERR_SEQ_0, data->safety_moduleId, __LINE__);
		return -EINVAL;
	}
	dev_info(&pdev->dev, "register PPS source name:%s\n",data->info.name);

	/* register IRQ interrupt handler */
	ret = devm_request_irq(&pdev->dev, (uint32_t)data->irq, hobot_pps_irq_handler,
			IRQF_TRIGGER_RISING, data->info.name, (void *)data);
	if (ret != 0) {
		pps_unregister_source(data->pps);
		dev_err(&pdev->dev, "failed to request IRQ %d\n", data->irq);
		//pps_send_diag_error_event((u16)EventIdReqIrqErr, (u8)ERR_SEQ_0, data->safety_moduleId, __LINE__);
		return -EINVAL;
	}
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


/**
 * @NO{S21E02C03U}
 * @brief pps probe
 *
 * @param[in] *pdev: pointer of platform_device
 *
 * @retval "= 0": success
 * @retval <0: failure
 *
 * @data_read None
 * @data_updated None
 *
 * @compatibility HW: J6
 * @compatibility SW: v1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t hobot_pps_probe(struct platform_device *pdev)
{
	struct hobot_pps_device_data *data;
	int32_t ret;
	//struct device_node *np = pdev->dev.of_node;

	//ret = hobot_pps_pinctrl_set(pdev);
	//if (ret != 0) {
	//	return ret;
	//}


	/* allocate space for device info */
	data = (struct hobot_pps_device_data *)devm_kzalloc(&pdev->dev, sizeof(struct hobot_pps_device_data),
			GFP_KERNEL);
	if (data == NULL) {
		dev_err(&pdev->dev, "devm_kzalloc error\n");
		//pps_send_diag_error_event((u16)EventIdPPSAddrErr, (u8)ERR_SEQ_0, moduleId, __LINE__);
		return -ENOMEM;
	}


	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0) {
		dev_err(&pdev->dev, "platform_get_irq error:%d\n", data->irq);
		//pps_send_diag_error_event((u16)EventIdGetIrqErr, (u8)ERR_SEQ_0, data->safety_moduleId, __LINE__);
		return data->irq;
	}

	hobot_pps_source_info_init(pdev, data);

	ret = hobot_pps_register_source(pdev, data);
	if (ret != 0) {
		dev_info(data->pps->dev, "hobot_pps_register_source error\n");
		//pps_send_diag_error_event((u16)EventIdRegisterErr, (u8)ERR_SEQ_1, data->safety_moduleId, __LINE__);
	}

	platform_set_drvdata(pdev, (void *)data);
	dev_info(data->pps->dev, "Registered IRQ %d as PPS source\n",
		 data->irq);

	return 0;
}

/**
 * @NO{S21E02C03U}
 * @brief pps remove
 *
 * @param[in] *pdev: pointer of platform_device
 *
 * @retval "= 0": success
 * @retval <0: failure
 *
 * @data_read None
 * @data_updated None
 *
 * @compatibility HW: J6
 * @compatibility SW: v1.0.0
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t hobot_pps_remove(struct platform_device *pdev)
{
	struct hobot_pps_device_data *data = (struct hobot_pps_device_data *)platform_get_drvdata(pdev);

	pps_unregister_source(data->pps);
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
