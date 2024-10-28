/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright (C) 2020-2023, Horizon Robotics Co., Ltd.
 *                    All rights reserved.
 *************************************************************************/
/**
 * @file hobot_vdsp_sys.c
 *
 * @NO{S05E05C01}
 * @ASIL{B}
 */

#include "hobot_vdsp.h"
#include "hobot_rpmsg.h"

#define SHELL_PREFIX "$"
#define SHELL_STR(cmd) (SHELL_PREFIX cmd)

static struct hobot_vdsp_dev_data *dev_to_hobot_vdsp_dev_data(struct device *dev)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);
	return pdata;
}

static int32_t vdsp_send_msg_byrpmsg(struct hobot_vdsp_dev_data *pdata, char *rpmsgmsg, size_t msgsize)
{
	int ret = 0;
	struct rpmsg_handle *localphandle = NULL;

	if (NULL == localphandle) {
		dev_err(pdata->dev, "vdsp%d rpmsg handler is NULL.\n", pdata->dsp_id);
		return -EFAULT;
	}

	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	ret = hb_rpmsg_send(localphandle, rpmsgmsg, msgsize);
	if (ret < 0) {
		dev_err(pdata->dev, "rpmsg_send %s ret %d.\n",
			rpmsgmsg, ret);
		return -EFAULT;
	}
	dev_dbg(pdata->dev, "vdsp%d %s success.\n", pdata->dsp_id, rpmsgmsg);

	return ret;
}

static int32_t vdsp_recv_msg_byrpmsg(struct hobot_vdsp_dev_data *pdata, char *rpmsgmsg, size_t msgsize)
{
	int32_t ret = 0;
	struct rpmsg_handle *localphandle = NULL;

	if (NULL == localphandle) {
		dev_err(pdata->dev, "rpmsg handler is NULL\n");
		return -EFAULT;
	}

	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	ret = hb_rpmsg_recv_timeout(localphandle, rpmsgmsg, msgsize, 3000);
	if (ret < 0) {
		dev_err(pdata->dev, "rpmsg_recv %s ret %d\n", rpmsgmsg, ret);
		return -EFAULT;
	}

	return ret;
}

static int32_t vdsp_send_wait_reply_msg_byrpmsg(
	struct hobot_vdsp_dev_data *pdata, char *send_msg, size_t send_msg_size, char *recv_msg, size_t recv_msg_size)
{
	int32_t ret = 0;

	ret = vdsp_send_msg_byrpmsg(pdata, send_msg, send_msg_size);
	if (ret < 0) {
		dev_err(pdata->dev, "vdsp%d send_msg_byrpmsg %s fail ret:%d\n", pdata->dsp_id, send_msg, ret);
		return -EFAULT;
	}

	ret = vdsp_recv_msg_byrpmsg(pdata, recv_msg, recv_msg_size);
	if (ret < 0) {
		dev_err(pdata->dev, "vdsp%d vdsp_recv_msg_byrpmsg fail ret:%d\n", pdata->dsp_id, ret);
		return -EFAULT;
	}

	return ret;
}

static ssize_t vdsp_ctrl_loglevel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	return (ssize_t)snprintf(buf, VDSP_PATH_BUF_SIZE, "vdsp%d log level=%d.", pdata->dsp_id, pdata->vdsp_state.loglevel);
}

static ssize_t vdsp_ctrl_loglevel_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t ret = 0;
	uint32_t value = 0;
	char msg[RPMSG_SIZE] = {0};
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &value) != 1) {
		dev_err(dev, "vdsp%d buf-%s param is invalid.\n", pdata->dsp_id, buf);
		return -EINVAL;
	}

	if ((value > DBG_TYPE_LEVEL)) {
		dev_err(pdata->dev, "%s invalid param\n", __func__);
		return -EINVAL;
	}

	snprintf(msg, RPMSG_SIZE, "%s%d", DSP_LOGLEVEL, value);

	ret = vdsp_send_msg_byrpmsg(pdata, msg, sizeof(msg));
	if (ret < 0) {
		dev_err(pdata->dev, "vdsp%d send_msg_byrpmsg %s error.\n",
			pdata->dsp_id, msg);
		return -EFAULT;
	}
	dev_info(pdata->dev, "vdsp%d config log level-%d success.\n", pdata->dsp_id, value);
	pdata->vdsp_state.loglevel = value;

	return (ssize_t)count;
}

static ssize_t vdsp_ctrl_wwdt_inject_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t ret = 0;
	uint32_t value = 0;
	char msg[RPMSG_SIZE] = {0};
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &value) != 1) {
		dev_err(dev, "vdsp%d buf-%s param is invalid.\n", pdata->dsp_id, buf);
		return -EINVAL;
	}

	if ((value != 1u)) {
		dev_err(pdata->dev, "%s invalid param\n", __func__);
		return -EINVAL;
	}

	snprintf(msg, RPMSG_SIZE, "%s", SHELL_STR("wwdt_inject"));

	ret = vdsp_send_msg_byrpmsg(pdata, msg, sizeof(msg));
	if (ret < 0) {
		dev_err(pdata->dev, "vdsp%d wwdt inject failed.\n",
			pdata->dsp_id);
		return -EFAULT;
	}
	dev_info(pdata->dev, "vdsp%d wwdt inject success.\n", pdata->dsp_id);

	return (ssize_t)count;
}


static ssize_t vdsp_ctrl_correctecc_inject_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t ret = 0;
	uint32_t value = 0;
	char msg[RPMSG_SIZE] = {0};
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &value) != 1) {
		dev_err(dev, "vdsp%d buf-%s param is invalid.\n", pdata->dsp_id, buf);
		return -EINVAL;
	}

	if ((value != 1u)) {
		dev_err(pdata->dev, "%s invalid param\n", __func__);
		return -EINVAL;
	}

	snprintf(msg, RPMSG_SIZE, "%s", DSP_CORRECTECC_INJECT);

	ret = vdsp_send_msg_byrpmsg(pdata, msg, sizeof(msg));
	if (ret < 0) {
		dev_err(pdata->dev, "vdsp%d wwdt inject failed.\n",
			pdata->dsp_id);
		return -EFAULT;
	}
	dev_info(pdata->dev, "vdsp%d wwdt inject success.\n", pdata->dsp_id);

	return (ssize_t)count;
}


static ssize_t vdsp_ctrl_arithmetic_inject_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t ret = 0;
	uint32_t value = 0;
	char msg[RPMSG_SIZE] = {0};
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &value) != 1) {
		dev_err(dev, "vdsp%d buf-%s param is invalid.\n", pdata->dsp_id, buf);
		return -EINVAL;
	}

	if ((value != 1u)) {
		dev_err(pdata->dev, "%s invalid param\n", __func__);
		return -EINVAL;
	}

	snprintf(msg, RPMSG_SIZE, "%s", DSP_ARITHMETIC_INJECT);

	ret = vdsp_send_msg_byrpmsg(pdata, msg, sizeof(msg));
	if (ret < 0) {
		dev_err(pdata->dev, "vdsp%d wwdt inject failed.\n",
			pdata->dsp_id);
		return -EFAULT;
	}
	dev_info(pdata->dev, "vdsp%d wwdt inject success.\n", pdata->dsp_id);

	return (ssize_t)count;
}


static ssize_t vdsp_ctrl_wwdt_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	return (ssize_t)snprintf(buf, VDSP_PATH_BUF_SIZE, "vdsp%d wwdt enable=%d.", pdata->dsp_id, pdata->vdsp_state.is_wwdt_enable);
}

static ssize_t vdsp_ctrl_wwdt_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t value = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &value) != 1) {
		dev_err(dev, "vdsp%d buf-%s param is invalid.\n", pdata->dsp_id, buf);
		return -EINVAL;
	}

	if (WWDT_ENABLE == value) {
		dev_err(pdata->dev, "%s dsp%d wwdt cannot reinit in RI-2023.11.\n", __func__, pdata->dsp_id);
		return -EINVAL;
	}

	if ((WWDT_DISABLE != value)) {
		dev_err(pdata->dev, "%s invalid param\n", __func__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_02
	pdata->vdsp_state.is_wwdt_enable = value;

	return (ssize_t)count;
}

static ssize_t vdsp_ctrl_thread_loading_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	return (ssize_t)snprintf(buf, VDSP_PATH_BUF_SIZE, "vdsp%d thread loading on=%d.", pdata->dsp_id, pdata->vdsp_state.is_trace_on);
}

static ssize_t vdsp_thread_loading_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t ret = 0;
	char value[RPMSG_SIZE];
	char msg[RPMSG_SIZE] = {0};
	uint32_t ison = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	if (sscanf(buf, "%s", (char *)&value) != 1) {
		dev_err(dev, "vdsp%d buf-%s param is invalid.\n", pdata->dsp_id, buf);
		return -EINVAL;
	}
	if((strcmp(value, "on") != 0) && (strcmp(value, "off") != 0)) {
		pr_err("This is invalid cmd\n");
		return -EINVAL;
	}

	if(strcmp(value, "on") == 0) {
		strncpy(msg, DSP_THREAD_ON, strlen(DSP_THREAD_ON)+1U);
		ison = 1;
	} else {
		strncpy(msg, DSP_THREAD_OFF, strlen(DSP_THREAD_OFF)+1U);
	}

	ret = vdsp_send_msg_byrpmsg(pdata, msg, sizeof(msg));
	if (ret < 0) {
		dev_err(pdata->dev, "vdsp%d send_msg_byrpmsg %s error.\n",
			pdata->dsp_id, msg);
		return -EFAULT;
	}
	dev_info(pdata->dev, "vdsp%d config %s success.\n", pdata->dsp_id, msg);
	pdata->vdsp_state.is_trace_on = ison;

	return (ssize_t)count;
}

// heart beat
static ssize_t vdsp_ctrl_heartbeat_enable_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int32_t rc;
	struct hobot_vdsp_dev_data *pdata = dev_to_hobot_vdsp_dev_data(dev);

	char send_msg[RPMSG_SIZE] = SHELL_STR("heartbeat_enable_get");
	char recv_msg[RPMSG_SIZE] = {};

	rc = vdsp_send_wait_reply_msg_byrpmsg(pdata, send_msg, sizeof(send_msg), recv_msg, sizeof(recv_msg));
	if (rc < 0) {
		return -EFAULT;
	}
	return (ssize_t)snprintf(buf, PAGE_SIZE, "%s", recv_msg);
}

static ssize_t vdsp_ctrl_heartbeat_enable_store(
	struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t rc;
	struct hobot_vdsp_dev_data *pdata = dev_to_hobot_vdsp_dev_data(dev);

	char send_msg[RPMSG_SIZE] = {};

	snprintf(send_msg, sizeof(send_msg), SHELL_STR("heartbeat_enable_set %s"), buf);

	rc = vdsp_send_msg_byrpmsg(pdata, send_msg, sizeof(send_msg));
	if (rc < 0) {
		return -EFAULT;
	}
	return (ssize_t)count;
}

static ssize_t vdsp_ctrl_heartbeat_interval_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int32_t rc;
	struct hobot_vdsp_dev_data *pdata = dev_to_hobot_vdsp_dev_data(dev);

	char send_msg[RPMSG_SIZE] = SHELL_STR("heartbeat_interval_get");
	char recv_msg[RPMSG_SIZE] = {};

	rc = vdsp_send_wait_reply_msg_byrpmsg(pdata, send_msg, sizeof(send_msg), recv_msg, sizeof(recv_msg));
	if (rc < 0) {
		return -EFAULT;
	}
	return (ssize_t)snprintf(buf, PAGE_SIZE, "%s", recv_msg);
}

static ssize_t vdsp_ctrl_heartbeat_interval_store(
	struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t ret;
	struct hobot_vdsp_dev_data *pdata = dev_to_hobot_vdsp_dev_data(dev);

	char send_msg[RPMSG_SIZE] = {};

	snprintf(send_msg, sizeof(send_msg), SHELL_STR("heartbeat_interval_set %s"), buf);

	ret = vdsp_send_msg_byrpmsg(pdata, send_msg, sizeof(send_msg));
	if (ret < 0) {
		return -EFAULT;
	}
	return (ssize_t)count;
}

static ssize_t vdsp_ctrl_heartbeat_inject_store(
	struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t ret;
	struct hobot_vdsp_dev_data *pdata = dev_to_hobot_vdsp_dev_data(dev);

	char send_msg[RPMSG_SIZE] = {};

	snprintf(send_msg, sizeof(send_msg), SHELL_STR("heartbeat_inject %s"), buf);

	ret = vdsp_send_msg_byrpmsg(pdata, send_msg, sizeof(send_msg));
	if (ret < 0) {
		return -EFAULT;
	}
	return (ssize_t)count;
}

// loading
static ssize_t vdsp_ctrl_thread_loading_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int32_t ret = 0;
	char send_msg[RPMSG_SIZE] = SHELL_STR("loading");
	char recv_msg[RPMSG_SIZE] = {};
	struct hobot_vdsp_dev_data *pdata = dev_to_hobot_vdsp_dev_data(dev);

	ret = vdsp_send_wait_reply_msg_byrpmsg(pdata, send_msg, sizeof(send_msg), recv_msg, sizeof(recv_msg));
	if (ret < 0) {
		dev_warn(pdata->dev, "loading show failed ret:%d\n", ret);
		return -EFAULT;
	}

	return (ssize_t)snprintf(buf, VDSP_PATH_BUF_SIZE, "%s", recv_msg);
}

static ssize_t vdsp_ctrl_uart_switch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	return (ssize_t)snprintf(buf, VDSP_PATH_BUF_SIZE, "vdsp%d uart=%d.", pdata->dsp_id, pdata->vdsp_state.uart_switch);
}

static ssize_t vdsp_uart_switch_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t ret = 0;
	char value[RPMSG_SIZE];
	char msg[RPMSG_SIZE] = {0};
	uint32_t uart = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	if (sscanf(buf, "%s", (char *)&value) != 1) {
		dev_err(dev, "vdsp%d buf-%s param is invalid.\n", pdata->dsp_id, buf);
		return -EINVAL;
	}
	if((strcmp(value, UART_COM1_NUM) != 0) && (strcmp(value, UART_COM2_NUM) != 0) && (strcmp(value, UART_COM3_NUM) != 0)) {
		pr_err("This is invalid cmd\n");
		return -EINVAL;
	}

	snprintf(msg, RPMSG_SIZE, "%s%s", DSP_UART_SWITCH, value);

	ret = vdsp_send_msg_byrpmsg(pdata, msg, sizeof(msg));
	if (ret < 0) {
		dev_err(pdata->dev, "vdsp%d send_msg_byrpmsg %s error.\n",
			pdata->dsp_id, msg);
		return -EFAULT;
	}
	dev_info(pdata->dev, "vdsp%d switch uart%s success.\n", pdata->dsp_id, value);

	if(strcmp(value, UART_COM1_NUM) == 0) {
		uart = 1;
	} else if(strcmp(value, UART_COM2_NUM) == 0) {
		uart = 2;
	} else {
		uart = 3;
	}
	pdata->vdsp_state.uart_switch = uart;

	return (ssize_t)count;
}

static ssize_t vdsp_ctrl_dump_clk_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	// struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	return (ssize_t)0;
}

static ssize_t vdsp_ctrl_dump_mpu_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	// struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	return (ssize_t)0;
}

static ssize_t vdsp_ctrl_dump_dtcm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	// struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	return (ssize_t)0;
}

static ssize_t vdsp_ctrl_dump_wwdt_sta_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	// struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	return (ssize_t)0;
}

static ssize_t vdsp_ctrl_dump_rpmsg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	// struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	return (ssize_t)0;
}

static ssize_t vdsp_ctrl_dump_all_rsc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	// struct hobot_vdsp_dev_data *pdata = (struct hobot_vdsp_dev_data *)dev_get_drvdata(dev);

	return (ssize_t)0;
}

/* sysfs for sensor devices' param */
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(correctecc_inject, (S_IWUSR | S_IRUGO), NULL, vdsp_ctrl_correctecc_inject_store);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(arithmetic_inject, (S_IWUSR | S_IRUGO), NULL, vdsp_ctrl_arithmetic_inject_store);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(vdsp_loglevel, (S_IWUSR | S_IRUGO), vdsp_ctrl_loglevel_show, vdsp_ctrl_loglevel_store);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(wwdt_enable, (S_IWUSR | S_IRUGO), vdsp_ctrl_wwdt_enable_show, vdsp_ctrl_wwdt_enable_store);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(wwdt_inject, (S_IWUSR | S_IRUGO), NULL, vdsp_ctrl_wwdt_inject_store);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(dspthread, (S_IWUSR | S_IRUGO), vdsp_ctrl_thread_loading_on_show, vdsp_thread_loading_on_store);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(loading, (S_IWUSR | S_IRUGO), vdsp_ctrl_thread_loading_show, NULL);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(uart_switch, (S_IWUSR | S_IRUGO), vdsp_ctrl_uart_switch_show, vdsp_uart_switch_store);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(dump_clk, (S_IWUSR | S_IRUGO), vdsp_ctrl_dump_clk_show, NULL);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(dump_mpu, (S_IWUSR | S_IRUGO), vdsp_ctrl_dump_mpu_show, NULL);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(dump_dtcm, (S_IWUSR | S_IRUGO), vdsp_ctrl_dump_dtcm_show, NULL);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(dump_wwdt_sta, (S_IWUSR | S_IRUGO), vdsp_ctrl_dump_wwdt_sta_show, NULL);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(dump_rpmsg, (S_IWUSR | S_IRUGO), vdsp_ctrl_dump_rpmsg_show, NULL);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(dump_all_rsc, (S_IWUSR | S_IRUGO), vdsp_ctrl_dump_all_rsc_show, NULL);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(heartbeat_enable, S_IRUGO | S_IWUSR, vdsp_ctrl_heartbeat_enable_show, vdsp_ctrl_heartbeat_enable_store);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(heartbeat_interval, S_IRUGO | S_IWUSR, vdsp_ctrl_heartbeat_interval_show, vdsp_ctrl_heartbeat_interval_store);
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(heartbeat_inject, S_IRUGO | S_IWUSR, NULL, vdsp_ctrl_heartbeat_inject_store);

static struct attribute *vdsp_ctrl_attr[] = {
	&dev_attr_correctecc_inject.attr,
	&dev_attr_arithmetic_inject.attr,
	&dev_attr_vdsp_loglevel.attr,
	&dev_attr_wwdt_enable.attr,
	&dev_attr_wwdt_inject.attr,
	&dev_attr_dspthread.attr,
	&dev_attr_loading.attr,
	&dev_attr_uart_switch.attr,
	&dev_attr_dump_clk.attr,
	&dev_attr_dump_mpu.attr,
	&dev_attr_dump_dtcm.attr,
	&dev_attr_dump_wwdt_sta.attr,
	&dev_attr_dump_rpmsg.attr,
	&dev_attr_dump_all_rsc.attr,
	&dev_attr_heartbeat_enable.attr,
	&dev_attr_heartbeat_interval.attr,
	&dev_attr_heartbeat_inject.attr,
	NULL,
};

//coverity[misra_c_2012_rule_8_4_violation:SUPPRESS], ## violation reason SYSSW_V_8.4_01
const struct attribute_group vdsp_ctrl_attr_group = {
	.name = __stringify(vdsp_ctrl),
	.attrs = vdsp_ctrl_attr,
};

MODULE_DESCRIPTION("Hobot VDSP Acore Drivers");
MODULE_LICENSE("GPL");
