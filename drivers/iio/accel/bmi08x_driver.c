/*
 * @section LICENSE
 * Copyright (c) 2019~2020 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi08x_driver.c
 * @date	 2022/06/22
 * @version	 2.0.0
 *
 * @brief	 BMI08X Linux Driver
 */

/*********************************************************************/
/* System header files */
/*********************************************************************/
#include <linux/types.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

/*********************************************************************/
/* Own header files */
/*********************************************************************/
#include "bmi08x_driver.h"
#include "bs_log.h"

/*********************************************************************/
/* Local macro definitions */
/*********************************************************************/
#define DRIVER_VERSION "2.0.1"
#define MS_TO_US(msec) UINT32_C((msec)*1000)
#define GRAVITY_EARTH (10)
/*********************************************************************/
/* Global data */
/*********************************************************************/
static struct bmi08x_sensor_data accel_data;
static struct bmi08x_sensor_data gyro_data;
static int64_t bmi_sync_data_time;
static u64 g_gyr_int_time = 0;
struct workqueue_struct *workqueue_test;

// static int lsb_to_mps2(int16_t val, int8_t g_range, uint8_t bit_width)
// {
//     int gravity;

//     int half_scale = ((1 << bit_width) / 2.0);

//     gravity = (int)((GRAVITY_EARTH * val * g_range) / half_scale);

//     return gravity;
// }

// static int lsb_to_dps(int16_t val, int dps, uint8_t bit_width)
// {
//     int half_scale = ((int)(1 << bit_width) / 2.0);

//     return (int)(dps / ((half_scale) + BMI08X_GYRO_RANGE_2000_DPS)) * (val);
// }

static inline int64_t get_ktime_timestamp(void)
{
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);

	return timespec64_to_ns(&ts);
}

/**
 * bmi08x_i2c_delay_ms - Adds a delay in units of millisecs.
 *
 * @msec: Delay value in millisecs.
 */
static void bmi08x_i2c_delay_us(u32 usec, void *intf_ptr)
{
	if (usec <= (MS_TO_US(20)))

		/* Delay range of usec to usec + 1 millisecs
		 * required due to kernel limitation
		 */
		usleep_range(usec, usec + 1000);
	else
		msleep(usec / 1000);
}

/**
 * check_error - check error code and print error message if rslt is non 0.
 *
 * @print_msg	: print message to print on if rslt is not 0.
 * @rslt			: error code return to be checked.
 */
static void check_error(char *print_msg, int rslt)
{
	if (rslt)
		PERR("%s failed with return code:%d\n", print_msg, rslt);
}

/**
 * bmi08x_uc_function_handle - Handles feature interrupts
 *
 * @client_data : Instance of client data.
 * @status : Interrupt register value
 */
static void bmi08x_uc_function_handle(
	struct bmi08x_client_data *client_data, u16 status)
{
	input_event(client_data->feat_input, EV_MSC, REL_FEAT_STATUS,
				(u32)(status));
	input_sync(client_data->feat_input);
}

/**
 * bmi08x_gyr_uc_function_handle - Handles feature interrupts
 *
 * @client_data : Instance of client data.
 * @status : Interrupt register value
 */
static void bmi08x_gyr_uc_function_handle(
	struct bmi08x_client_data *client_data, struct bmi08x_sensor_data *acc, struct bmi08x_sensor_data *gyr, u64 sync_time, u64 count)
{
	u64 time_mask = 0xFFFFFFFF;
	u32 sync_time_lsb = sync_time & time_mask;
	u32 sync_time_msb = (sync_time >> 32);
	input_event(client_data->gyr_feat_input, EV_MSC, REL_GYR_FEAT_STATUS, (u32)(acc->x));
	input_event(client_data->gyr_feat_input, EV_MSC, REL_GYR_FEAT_STATUS, (u32)(acc->y));
	input_event(client_data->gyr_feat_input, EV_MSC, REL_GYR_FEAT_STATUS, (u32)(acc->z));
	input_event(client_data->gyr_feat_input, EV_MSC, REL_GYR_FEAT_STATUS, (u32)(gyr->x));
	input_event(client_data->gyr_feat_input, EV_MSC, REL_GYR_FEAT_STATUS, (u32)(gyr->y));
	input_event(client_data->gyr_feat_input, EV_MSC, REL_GYR_FEAT_STATUS, (u32)(gyr->z));
	input_event(client_data->gyr_feat_input, EV_MSC, REL_GYR_FEAT_STATUS, sync_time_msb);
	input_event(client_data->gyr_feat_input, EV_MSC, REL_GYR_FEAT_STATUS, sync_time_lsb);
	input_event(client_data->gyr_feat_input, EV_MSC, REL_GYR_FEAT_STATUS, (u32)count);

	input_sync(client_data->gyr_feat_input);
}

/* enables or disables selected feature index from config function store*/
static int feature_config_set(struct bmi08x_client_data *client_data,
							  int config_func, int enable)
{
	int rslt;
	struct bmi08x_accel_int_channel_cfg accel_int_config;
	struct bmi08x_gyro_int_channel_cfg gyro_int_config;

	PDEBUG("config_func = %d, enable=%d", config_func, enable);
	if (config_func < 0 || config_func > BMI088_GYRO_FIFO_FULL)
		return -EINVAL;

	switch (config_func)
	{
	case BMI088_ACC_DATA_READY:
		accel_int_config.int_type = BMI08X_ACCEL_INT_DATA_RDY;
		client_data->acc_drdy_en = enable;
		break;
	case BMI088_ACC_FIFO_WM:
		accel_int_config.int_type = BMI08X_ACCEL_INT_FIFO_WM;
		client_data->acc_fifo_wm_en = enable;
		break;
	case BMI088_ACC_FIFO_FULL:
		accel_int_config.int_type = BMI08X_ACCEL_INT_FIFO_FULL;
		client_data->acc_fifo_full_en = enable;
		break;
	case BMI088_GYRO_DATA_READY:
		gyro_int_config.int_type = BMI08X_GYRO_INT_DATA_RDY;
		client_data->gyro_drdy_en = enable;
		break;
	case BMI088_GYRO_FIFO_WM:
		gyro_int_config.int_type = BMI08X_GYRO_INT_FIFO_WM;
		client_data->gyro_fifo_wm_en = enable;
		break;
	case BMI088_GYRO_FIFO_FULL:
		gyro_int_config.int_type = BMI08X_GYRO_INT_FIFO_FULL;
		client_data->gyro_fifo_full_en = enable;
		break;
	default:
		PERR("Invalid feature selection: %d", config_func);
		return -EINVAL;
	}

	if ((config_func == BMI088_ACC_DATA_READY) ||
		(config_func == BMI088_ACC_FIFO_WM) ||
		(config_func == BMI088_ACC_FIFO_FULL))
	{
		accel_int_config.int_channel = BMI08X_INT_CHANNEL_1;
		accel_int_config.int_pin_cfg.output_mode = BMI08X_INT_MODE_PUSH_PULL;
		accel_int_config.int_pin_cfg.lvl = BMI08X_INT_ACTIVE_HIGH;
		accel_int_config.int_pin_cfg.enable_int_pin = enable;
		rslt = bmi08a_set_int_config(
			(const struct bmi08x_accel_int_channel_cfg *)&accel_int_config,
			&client_data->device);
	}
	else
	{
		gyro_int_config.int_channel = BMI08X_INT_CHANNEL_3;
		gyro_int_config.int_pin_cfg.output_mode = BMI08X_INT_MODE_PUSH_PULL;
		gyro_int_config.int_pin_cfg.lvl = BMI08X_INT_ACTIVE_HIGH;
		gyro_int_config.int_pin_cfg.enable_int_pin = enable;
		rslt = bmi08g_set_int_config(
			(const struct bmi08x_gyro_int_channel_cfg *)&gyro_int_config,
			&client_data->device);
	}
	check_error("en/disabling interrupt", rslt);
	return rslt;
}

/**
 *	bmi08x_irq_work_func - Bottom half handler for feature interrupts.
 *	@work : Work data for the workqueue handler.
 */
static void bmi08x_irq_work_func(struct work_struct *work)
{
	struct bmi08x_client_data *client_data = container_of(work,
														  struct bmi08x_client_data, irq_work);
	int rslt = 0;
	struct bmi08x_sensor_data accel_data;

	mutex_lock(&client_data->lock);
	rslt = bmi08a_get_data_int_status(&client_data->acc_int_status,
									  &client_data->device);
	mutex_unlock(&client_data->lock);
	if (client_data->acc_int_status != 0)
	{
		PINFO("ACC INT status:0x%x\n", client_data->acc_int_status);
		if ((client_data->acc_int_status & BMI08X_ACCEL_DATA_READY_INT) &&
			(client_data->data_sync_en != 1))
		{
			PINFO("ACC DRDY INT ocurred\n");
			mutex_lock(&client_data->lock);
			rslt = bmi08a_get_data(&accel_data, &client_data->device);
			mutex_unlock(&client_data->lock);
			check_error("get acc data", rslt);
			PINFO("ACC X:%d Y:%d Z:%d\n",
				  accel_data.x, accel_data.y, accel_data.z);
			rslt = feature_config_set(client_data, BMI088_ACC_DATA_READY,
									  BMI08X_DISABLE);
		}
		if (client_data->acc_int_status & BMI08X_ACCEL_FIFO_FULL_INT)
		{
			PINFO("ACC FIFO FULL INT occurred\n");
			rslt = feature_config_set(client_data, BMI088_ACC_FIFO_FULL,
									  BMI08X_DISABLE);
		}
		if (client_data->acc_int_status & BMI08X_ACCEL_FIFO_WM_INT)
		{
			PINFO("ACC FIFO WM INT occurred\n");
			rslt = feature_config_set(client_data, BMI088_ACC_FIFO_WM,
									  BMI08X_DISABLE);
		}
		check_error("acc INT handle", rslt);
	}
	if (client_data->acc_int_status != 0)
		bmi08x_uc_function_handle(client_data, client_data->acc_int_status);
}

/* enables or disables data sync interrupt configurations*/
static int data_sync_int_config(struct bmi08x_client_data *client_data,
								int enable)
{
	struct bmi08x_int_cfg int_config;
	struct bmi08x_data_sync_cfg sync_cfg;
	int rslt;

	mutex_lock(&client_data->lock);
	if (enable)
	{
		client_data->device.accel_cfg.range = BMI088_ACCEL_RANGE_24G;
		client_data->device.gyro_cfg.range = BMI08X_GYRO_RANGE_2000_DPS;
		sync_cfg.mode = BMI08X_ACCEL_DATA_SYNC_MODE_400HZ;
	}
	else
		sync_cfg.mode = BMI08X_ACCEL_DATA_SYNC_MODE_OFF;
	rslt = bmi08a_configure_data_synchronization(sync_cfg,
												 &client_data->device);
	check_error("config data sync", rslt);
	int_config.accel_int_config_1.int_channel = BMI08X_INT_CHANNEL_2;
	int_config.accel_int_config_1.int_type = BMI08X_ACCEL_SYNC_INPUT;
	int_config.accel_int_config_1.int_pin_cfg.output_mode =
		BMI08X_INT_MODE_PUSH_PULL;
	int_config.accel_int_config_1.int_pin_cfg.lvl = BMI08X_INT_ACTIVE_HIGH;
	int_config.accel_int_config_1.int_pin_cfg.enable_int_pin = enable;

	int_config.accel_int_config_2.int_channel = BMI08X_INT_CHANNEL_1;
	int_config.accel_int_config_2.int_type = BMI08X_ACCEL_INT_SYNC_DATA_RDY;
	int_config.accel_int_config_2.int_pin_cfg.output_mode =
		BMI08X_INT_MODE_PUSH_PULL;
	int_config.accel_int_config_2.int_pin_cfg.lvl = BMI08X_INT_ACTIVE_HIGH;
	int_config.accel_int_config_2.int_pin_cfg.enable_int_pin = enable;

	int_config.gyro_int_config_1.int_channel = BMI08X_INT_CHANNEL_4;
	int_config.gyro_int_config_1.int_type = BMI08X_GYRO_INT_DATA_RDY;
	int_config.gyro_int_config_1.int_pin_cfg.enable_int_pin = enable;
	int_config.gyro_int_config_1.int_pin_cfg.lvl = BMI08X_INT_ACTIVE_HIGH;
	int_config.gyro_int_config_1.int_pin_cfg.output_mode =
		BMI08X_INT_MODE_PUSH_PULL;

	int_config.gyro_int_config_2.int_channel = BMI08X_INT_CHANNEL_3;
	int_config.gyro_int_config_2.int_type = BMI08X_GYRO_INT_DATA_RDY;
	int_config.gyro_int_config_2.int_pin_cfg.enable_int_pin = enable;
	int_config.gyro_int_config_2.int_pin_cfg.lvl = BMI08X_INT_ACTIVE_HIGH;
	int_config.gyro_int_config_2.int_pin_cfg.output_mode =
		BMI08X_INT_MODE_PUSH_PULL;

	rslt += bmi08a_set_data_sync_int_config(&int_config, &client_data->device);
	client_data->data_sync_en = enable;
	check_error("data sync interrupt config", rslt);
	mutex_unlock(&client_data->lock);
	return rslt;
}

/**
 *	bmi08x_gyr_irq_work_func - Bottom half handler for feature interrupts.
 *	@work : Work data for the workqueue handler.
 */
// defined but not used
// static void bmi08x_gyr_irq_work_func(struct work_struct *work)
// {
// 	struct bmi08x_client_data *client_data = container_of(work,
// 														  struct bmi08x_client_data, gyr_irq_work);
// 	int rslt = 0;
// 	// u64 start_time = get_ktime_timestamp();
// 	rslt = bmi08a_get_synchronized_data(&accel_data, &gyro_data,
// 										&client_data->device);
// 	// u64 end_time = get_ktime_timestamp();
// 	// u64 dt = end_time - start_time;
// 	// if (dt)
// 	// {
// 	// 	pr_err("get_data=%lld,num:%lld\n", dt,client_data->gyr_int_count);
// 	// }

// 	// start_time = get_ktime_timestamp();
// 	bmi08x_gyr_uc_function_handle(client_data, &accel_data, &gyro_data, client_data->gyr_int_time, client_data->gyr_int_count);
// 	// end_time = get_ktime_timestamp();
// 	// pr_err("push_data:%lld\n", end_time - start_time);
// 	//  mutex_lock(&client_data->lock);
// 	//  rslt = bmi08g_get_data_int_status(&client_data->int_status,
// 	//  								  &client_data->device);
// 	//  mutex_unlock(&client_data->lock);

// 	// if (client_data->int_status != 0)
// 	// {
// 	// 	if (likely((client_data->int_status & BMI08X_GYRO_DATA_READY_INT) &&
// 	// 			   (client_data->data_sync_en == 1)))
// 	// 	{
// 	// 		mutex_lock(&client_data->lock);

// 	// 		mutex_unlock(&client_data->lock);
// 	// 	}
// 	// 	else if (unlikely(client_data->int_status & BMI08X_GYRO_DATA_READY_INT))
// 	// 	{
// 	// 		// PINFO("GYRO DRDY INT Occurred\n");
// 	// 		// mutex_lock(&client_data->lock);
// 	// 		// rslt = bmi08g_get_data(&gyro_data, &client_data->device);
// 	// 		// mutex_unlock(&client_data->lock);
// 	// 		// check_error("get gyro data", rslt);
// 	// 		// PINFO("GYRO X:%d Y:%d Z:%d\n",
// 	// 		// 	  gyro_data.x, gyro_data.y, gyro_data.z);
// 	// 		// rslt = feature_config_set(client_data, BMI088_GYRO_DATA_READY,
// 	// 		// 						  BMI08X_DISABLE);
// 	// 	}
// 	// 	if (unlikely(client_data->int_status & BMI08X_GYRO_FIFO_FULL_INT))
// 	// 	{
// 	// 		// PINFO("GYRO FIFO INT Occurred\n");
// 	// 		// if (client_data->gyro_fifo_full_en)
// 	// 		// 	rslt = feature_config_set(client_data, BMI088_GYRO_FIFO_FULL,
// 	// 		// 							  BMI08X_DISABLE);
// 	// 		// if (client_data->gyro_fifo_wm_en)
// 	// 		// 	rslt = feature_config_set(client_data, BMI088_GYRO_FIFO_WM,
// 	// 		// 							  BMI08X_DISABLE);
// 	// 	}
// 	// 	check_error("gyro INT handle", rslt);
// 	// }

// 	// if (client_data->int_status != 0)
// 	//  bmi08x_gyr_uc_function_handle(client_data, client_data->acc_int_status);
// }

/**
 * bmi08x_irq_handle - IRQ handler function.
 * @irq : Number of irq line.
 * @handle : Instance of client data.
 *
 * Return : Status of IRQ function.
 */
// defined but not used
// static irqreturn_t bmi08x_irq_handle(int irq, void *handle)
// {
// 	struct bmi08x_client_data *client_data = handle;
// 	if (schedule_work(&client_data->irq_work))
// 		return IRQ_HANDLED;
// 	return IRQ_HANDLED;
// }

// /**
//  * bmi08x_gyr_irq_handle - IRQ handler function.
//  * @irq : Number of irq line.
//  * @handle : Instance of client data.
//  *
//  * Return : Status of IRQ function.
//  */
// static irqreturn_t bmi08x_gyr_irq_handle(int irq, void *handle)
// {
// 	struct bmi08x_client_data *client_data = handle;
// 	client_data->gyr_int_time = get_ktime_timestamp();
// 	client_data->gyr_int_count = g_gyr_int_time++;
// 	// bmi08a_get_synchronized_data(&accel_data, &gyro_data,&client_data->device);
// 	// bmi08x_gyr_uc_function_handle(client_data, &accel_data, &gyro_data, client_data->gyr_int_time, client_data->gyr_int_count);
// 	if (queue_work(workqueue_test, &client_data->gyr_irq_work))
// 		// //if (schedule_work(&client_data->gyr_irq_work))
// 		return IRQ_HANDLED;
// 	pr_err("irq_schedule_work add fail,int num:%lld\n", client_data->gyr_int_count);
// 	return IRQ_HANDLED;
// }

/**
 * bmi08x_gyr_irq_handle - IRQ handler function.
 * @irq : Number of irq line.
 * @handle : Instance of client data.
 *
 * Return : Status of IRQ function.
 */
static irqreturn_t bmi08x_gyr_irq_handle(int irq, void *handle)
{
	struct bmi08x_client_data *client_data = handle;
	//uint8_t cmd = 0;
	//bmi08a_set_regs(BMI08X_REG_ACCEL_INT1_INT2_MAP_DATA,&cmd,8,&client_data->device);
	client_data->gyr_int_time = get_ktime_timestamp();
	client_data->gyr_int_count = g_gyr_int_time++;
	//mutex_lock(&client_data->lock);
	bmi08a_get_synchronized_data(&accel_data, &gyro_data,
										&client_data->device);
	//mutex_unlock(&client_data->lock);
	bmi08x_gyr_uc_function_handle(client_data, &accel_data, &gyro_data, client_data->gyr_int_time, client_data->gyr_int_count);
	//cmd = 0x04;
	//bmi08a_set_regs(BMI08X_REG_ACCEL_INT1_INT2_MAP_DATA,&cmd,8,&client_data->device);
	return IRQ_HANDLED;
}




/**
 * bmi08x_request_irq - Allocates interrupt resources and enables the
 * interrupt line and IRQ handling.
 *
 * @client_data: Instance of the client data.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * Any Negative value - Error.
 */
static int bmi08x_request_irq(struct bmi08x_client_data *client_data)
{
	int rslt = 0;
	pr_err("irq request %d\n", client_data->IRQ);
	rslt = devm_request_threaded_irq(client_data->dev, client_data->IRQ,
									 NULL, bmi08x_gyr_irq_handle, IRQF_ONESHOT|IRQF_TRIGGER_RISING|IRQF_NO_THREAD, SENSOR_NAME_FEAT, client_data);
	if (rslt < 0)
	{
		PERR("request_irq failed with rslt:%d", rslt);
		return -EIO;
	}
	PINFO("ACC IRQ requested for pin : %d\n", client_data->IRQ);
	INIT_WORK(&client_data->irq_work, bmi08x_irq_work_func);
	return rslt;
}

/**
 * bmi08x_gyr_request_irq - Allocates interrupt resources and enables the
 * interrupt line and IRQ handling.
 *
 * @client_data: Instance of the client data.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * Any Negative value - Error.
 */
static int bmi08x_gyr_request_irq(struct bmi08x_client_data *client_data)
{
	int rslt = 0;

	PINFO("GYR IRQ requested for pin : %d\n", client_data->GYR_IRQ);
	// rslt = request_irq(client_data->GYR_IRQ, bmi08x_gyr_irq_handle,
	// 				   IRQF_TRIGGER_RISING,
	// 				   SENSOR_NAME, client_data);
	// rslt = devm_request_threaded_irq(client_data->dev, client_data->GYR_IRQ,
	// 								 NULL, bmi08x_gyr_irq_handle, IRQF_ONESHOT|IRQF_TRIGGER_RISING, SENSOR_NAME, client_data);
	if (rslt < 0)
	{
		PERR("gyr_request_irq failed with rslt:%d", rslt);
		return -EIO;
	}
	// workqueue_test = alloc_workqueue("workqueue_test", WQ_UNBOUND | WQ_HIGHPRI, WQ_UNBOUND_MAX_ACTIVE);
	// INIT_WORK(&client_data->gyr_irq_work, bmi08x_gyr_irq_work_func);
	return rslt;
}

/**
 * chip_id_show - sysfs callback for reading the chip id of the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t chip_id_show(struct device *dev,
							struct device_attribute *attr, char *buf)
{
	u8 acc_chip_id = 0;
	u8 gyr_chip_id = 0;
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	rslt = bmi08a_get_regs(BMI08X_REG_ACCEL_CHIP_ID, &acc_chip_id,
						   1, &client_data->device);
	check_error("get acc chip id", rslt);
	rslt = bmi08g_get_regs(BMI08X_REG_GYRO_CHIP_ID, &gyr_chip_id,
						   1, &client_data->device);
	check_error("get gyr chip id", rslt);
	return scnprintf(buf, PAGE_SIZE, "chip_id acc:0x%x gyr:0x%x\n",
					 acc_chip_id, gyr_chip_id);
}

/**
 * driver_version_show - sysfs read callback which provides the driver
 * version.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t driver_version_show(struct device *dev,
								   struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
					 "Driver version: %s\n", DRIVER_VERSION);
}

/**
 * avail_sensor_show - sysfs read callback which provides the sensor-id
 * to the user.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t avail_sensor_show(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, 32, "bmi088\n");
}

/**
 * temperature_show - sysfs callback which reads temperature raw value.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t temperature_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	s32 sensor_temp;

	rslt = bmi08a_get_sensor_temperature(&client_data->device, &sensor_temp);
	check_error("get temperature data", rslt);

	return scnprintf(buf, PAGE_SIZE, "temperature:%d\n", sensor_temp);
}

/**
 * acc_selftest_store - sysfs write callback which performs the self test
 * in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t acc_selftest_store(struct device *dev,
								  struct device_attribute *attr,
								  const char *buf, size_t count)
{
	int rslt;
	unsigned long selftest;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	rslt = kstrtoul(buf, 10, &selftest);
	check_error("acc_selftest receive input", rslt);

	if (selftest == 1)
	{
		mutex_lock(&client_data->lock);
		/* Perform accelerometer self-test */
		rslt = bmi08a_perform_selftest(&client_data->device);
		if (rslt)
			PDEBUG("accel self test failed with return code:%d", rslt);
		else
			PDEBUG("accel self test success");
		mutex_unlock(&client_data->lock);
	}
	else
	{
		PERR("Invalid input use: echo 1 > acc_selftest");
		rslt = -EINVAL;
		return rslt;
	}

	return count;
}

/**
 * gyr_op_mode_show - sysfs callback which tells what is the power mode
 * of the gyroscope
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 *
 * @note Gyro operational modes
 *	Gyro power mode normal		 : 0
 *	Gyro power mode deep suspend : 1
 *	Gyro power mode suspend		 : 2
 */
static ssize_t gyr_op_mode_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	rslt = bmi08g_get_power_mode(&client_data->device);
	check_error("gyro get power mode", rslt);
	if (client_data->device.gyro_cfg.power == BMI08X_GYRO_PM_NORMAL)
		return scnprintf(buf, PAGE_SIZE,
						 "gyr_op_mode : BMI08X_GYRO_PM_NORMAL\n");
	else if (client_data->device.gyro_cfg.power == BMI08X_GYRO_PM_DEEP_SUSPEND)
		return scnprintf(buf, PAGE_SIZE,
						 "gyr_op_mode : BMI08X_GYRO_PM_DEEP_SUSPEND\n");
	else if (client_data->device.gyro_cfg.power == BMI08X_GYRO_PM_SUSPEND)
		return scnprintf(buf, PAGE_SIZE,
						 "gyr_op_mode : BMI08X_GYRO_PM_SUSPEND\n");
	return scnprintf(buf, PAGE_SIZE,
					 "invalid gyro power mode read\n");
}

/**
 * gyr_op_mode_store - sysfs callback which sets the power mode of the
 * gyroscope
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyr_op_mode_store(struct device *dev,
								 struct device_attribute *attr, const char *buf, size_t count)
{
	int rslt;
	unsigned long op_mode;

	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	rslt = kstrtoul(buf, 10, &op_mode);
	check_error("gyr_op_mode receive input", rslt);
	if (op_mode == 0)
		client_data->device.gyro_cfg.power = BMI08X_GYRO_PM_SUSPEND;
	else if (op_mode == 1)
		client_data->device.gyro_cfg.power = BMI08X_GYRO_PM_NORMAL;
	else if (op_mode == 2)
		client_data->device.gyro_cfg.power = BMI08X_GYRO_PM_DEEP_SUSPEND;
	else
	{
		PERR("pwr mode Invalid input:\n0->normal 1->deep_suspend2->suspend");
		return -EINVAL;
	}
	mutex_lock(&client_data->lock);
	rslt = bmi08g_set_power_mode(&client_data->device);
	mutex_unlock(&client_data->lock);
	check_error("gyro set power mode", rslt);
	return count;
}

/**
 * gyr_val_show - sysfs read callback which gives the
 * raw gyro value from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyr_val_show(struct device *dev,
							struct device_attribute *attr, char *buf)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	struct bmi08x_sensor_data gyro_data;

	mutex_lock(&client_data->lock);
	rslt = bmi08g_get_data(&gyro_data, &client_data->device);
	mutex_unlock(&client_data->lock);
	check_error("get gyro data", rslt);

	return scnprintf(buf, PAGE_SIZE, "%d %d %d\n",
					 gyro_data.x, gyro_data.y, gyro_data.z);
}

/**
 * gyr_config_show - sysfs read callback which reads
 * gyroscope configuration parameters.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyr_config_show(struct device *dev,
							   struct device_attribute *attr, char *buf)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	mutex_lock(&client_data->lock);
	rslt = bmi08g_get_meas_conf(&client_data->device);
	check_error("get meas gyro config", rslt);
	mutex_unlock(&client_data->lock);
	return scnprintf(buf, PAGE_SIZE, "gyro_cfg odr:%d bw:%d range:%d\n",
					 client_data->device.gyro_cfg.odr,
					 client_data->device.gyro_cfg.bw,
					 client_data->device.gyro_cfg.range);
}

/**
 * gyr_config_store - sysfs write callback which sets
 * gyroscope configuration parameters.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyr_config_store(struct device *dev,
								struct device_attribute *attr, const char *buf, size_t count)
{
	int rslt;
	unsigned int data[3] = {0};
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	rslt = sscanf(buf, "%11d %11d %11d ", &data[0], &data[1], &data[2]);
	if (rslt != 3)
	{
		PINFO("Invalid input\nuse echo odr bandwidth range > gyr_conf");
		return -EIO;
	}
	mutex_lock(&client_data->lock);
	rslt = bmi08g_get_meas_conf(&client_data->device);
	check_error("get meas gyro conf", rslt);
	client_data->device.gyro_cfg.odr = (u8)data[0];
	client_data->device.gyro_cfg.bw = (u8)data[1];
	client_data->device.gyro_cfg.range = (u8)data[2];
	rslt = bmi08g_set_meas_conf(&client_data->device);
	check_error("set meas gyro conf", rslt);
	mutex_unlock(&client_data->lock);
	return count;
}

/**
 * acc_op_mode_show - sysfs callback which tells whether accelerometer is
 * enabled or disabled.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 * @note Accel enable/disable
 *	 Power mode active	: 1
 *	 Power mode suspend : 0
 */
static ssize_t acc_op_mode_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	rslt = bmi08a_get_power_mode(&client_data->device);
	check_error("acc get power mode", rslt);
	if (client_data->device.accel_cfg.power == BMI08X_ACCEL_PM_ACTIVE)
		return scnprintf(buf, PAGE_SIZE,
						 "accel_op_mode : BMI08X_ACCEL_PM_ACTIVE\n");
	else if (client_data->device.accel_cfg.power == BMI08X_ACCEL_PM_SUSPEND)
		return scnprintf(buf, PAGE_SIZE,
						 "acc_op_mode : BMI08X_ACCEL_PM_SUSPEND\n");
	return scnprintf(buf, PAGE_SIZE,
					 "invalid accel power mode read\n");
}

/**
 * acc_op_mode_store - sysfs callback which enables or disables the
 * accelerometer.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t acc_op_mode_store(struct device *dev,
								 struct device_attribute *attr, const char *buf, size_t count)
{
	int rslt;
	unsigned long op_mode;

	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	rslt = kstrtoul(buf, 10, &op_mode);
	check_error("gyr_op_mode receive input", rslt);

	if (op_mode == 1)
		client_data->device.accel_cfg.power = BMI08X_ACCEL_PM_ACTIVE;
	else if (op_mode == 0)
		client_data->device.accel_cfg.power = BMI08X_ACCEL_PM_SUSPEND;
	else
	{
		PERR("pwr mode Invalid input:\n0->active 3->suspend");
		return -EINVAL;
	}
	mutex_lock(&client_data->lock);
	rslt = bmi08a_set_power_mode(&client_data->device);
	mutex_unlock(&client_data->lock);
	check_error("acc set power mode", rslt);
	return count;
}

/**
 * acc_val_show - sysfs read callback which gives the
 * raw accelerometer value from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t acc_val_show(struct device *dev,
							struct device_attribute *attr, char *buf)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	struct bmi08x_sensor_data accel_data;

	mutex_lock(&client_data->lock);
	rslt = bmi08a_get_data(&accel_data, &client_data->device);
	mutex_unlock(&client_data->lock);
	check_error("get acc data", rslt);
	return scnprintf(buf, PAGE_SIZE, "%d %d %d\n",
					 accel_data.x, accel_data.y, accel_data.z);
}

/**
 * acc_config_show - sysfs read callback which reads
 * accelerometer configuration parameters.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t acc_config_show(struct device *dev,
							   struct device_attribute *attr, char *buf)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	mutex_lock(&client_data->lock);
	rslt = bmi08a_get_meas_conf(&client_data->device);
	check_error("get acc meas conf", rslt);
	mutex_unlock(&client_data->lock);
	return scnprintf(buf, PAGE_SIZE, "acc_conf odr:%d bw:%d range:%d\n",
					 client_data->device.accel_cfg.odr,
					 client_data->device.accel_cfg.bw,
					 client_data->device.accel_cfg.range);
}

/**
 * acc_config_store - sysfs write callback which sets
 * accelerometer configuration parameters.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t acc_config_store(struct device *dev,
								struct device_attribute *attr, const char *buf, size_t count)
{
	int rslt;
	unsigned int data[3] = {0};
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	rslt = sscanf(buf, "%11d %11d %11d ", &data[0], &data[1], &data[2]);
	if (rslt != 3)
	{
		PINFO("Invalid input\nuse echo odr bandwidth range > acc_conf");
		return -EIO;
	}
	mutex_lock(&client_data->lock);
	rslt = bmi08a_get_meas_conf(&client_data->device);
	check_error("get acc meas conf", rslt);
	client_data->device.accel_cfg.odr = (u8)data[0];
	client_data->device.accel_cfg.bw = (u8)data[1];
	client_data->device.accel_cfg.range = (u8)data[2];
	rslt = bmi08a_set_meas_conf(&client_data->device);
	check_error("set acc meas conf", rslt);
	mutex_unlock(&client_data->lock);

	return count;
}

/**
 * sensor_init_show - sysfs read callback which reads the
 * sensor initialization state.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t sensor_init_show(struct device *dev,
								struct device_attribute *atte, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	if (client_data->sensor_init == 1)
		return scnprintf(buf, PAGE_SIZE, "sensor initilized\n");
	else
		return scnprintf(buf, PAGE_SIZE, "sensor not initilized\n");
}

/**
 * sensor_init_store - sysfs write callback which does sesnor
 * initialization.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t sensor_init_store(struct device *dev,
								 struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	unsigned long usr_option;
	int rslt = 0;

	rslt = kstrtoul(buf, 10, &usr_option);
	check_error("sensor_init input receive", rslt);

	if (usr_option != 1)
	{
		PERR("Invalid argument\n use: echo 1 > sensor_init");
		return -EIO;
	}
	mutex_lock(&client_data->lock);
	rslt = bmi08a_init(&client_data->device);
	if (rslt)
	{
		PERR("accel sensor init failed wtih rslt:%d", rslt);
		mutex_unlock(&client_data->lock);
		return -EIO;
	}
	PINFO("accel initilized\n");

	rslt = bmi08g_init(&client_data->device);
	if (rslt)
	{
		PERR("gyro sensor init failed wtih rslt:%d", rslt);
		mutex_unlock(&client_data->lock);
		return -EIO;
	}
	PINFO("gyro initilized\n");

	rslt = bmi08a_soft_reset(&client_data->device);
	if (rslt)
	{
		PERR("sensor soft reset failed wtih rslt:%d", rslt);
		mutex_unlock(&client_data->lock);
		return -EIO;
	}
	PINFO("soft reset done");

	bmi08x_i2c_delay_us(MS_TO_US(50), &client_data->device.intf_ptr_accel);
	rslt = bmi08a_load_config_file(&client_data->device);
	if (rslt)
	{
		PERR("load config failed wtih rslt:%d", rslt);
		mutex_unlock(&client_data->lock);
		return -EIO;
	}
	PINFO("config stream loaded successfully\n");

	bmi08x_i2c_delay_us(MS_TO_US(10), &client_data->device.intf_ptr_accel);

	client_data->device.accel_cfg.power = BMI08X_ACCEL_PM_ACTIVE;
	rslt |= bmi08a_set_power_mode(&client_data->device);
	if (rslt < 0)
	{
		PINFO("ACCEL POWER MODE SET FIALED");
		mutex_unlock(&client_data->lock);
		return -EIO;
	}
	PINFO("Accel power mode set to NORMAL");

	client_data->device.gyro_cfg.power = BMI08X_GYRO_PM_NORMAL;
	rslt |= bmi08g_set_power_mode(&client_data->device);

	if (rslt < 0)
	{
		PINFO("GYRO POWER MODE SET FIALED");
		mutex_unlock(&client_data->lock);
		return -EIO;
	}
	PINFO("Gyro power mode set to NORMAL");
	mutex_unlock(&client_data->lock);
	if (!rslt)
	{
		PINFO("Sensor successfully initilized\n");
		PINFO("Acc chip ID : 0x%x, Gyro chip ID : 0x%x\n",
			  client_data->device.accel_chip_id,
			  client_data->device.gyro_chip_id);
		client_data->sensor_init = 1;
	}
	client_data->data_sync_en = 0;
	return count;
}

/**
 * data_sync_show - sysfs read callback which gives the
 * raw accelerometer nd gyro value from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t data_sync_show(struct device *dev,
							  struct device_attribute *attr, char *buf)
{
	int rslt;
	// int acc_x, acc_y, acc_z;
	// int gyr_x, gyr_y, gyr_z;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	if (client_data->data_sync_en == 1)
	{
		// mutex_lock(&client_data->lock);
		// acc_x = lsb_to_mps2(accel_data.x,24,16);
		// acc_y = lsb_to_mps2(accel_data.y,24,16);
		// acc_z = lsb_to_mps2(accel_data.z,24,16);
		// gyr_x = lsb_to_dps(gyro_data.x, 2000, 16);
		// gyr_y = lsb_to_dps(gyro_data.y, 2000, 16);
		// gyr_z = lsb_to_dps(gyro_data.z, 2000, 16);
		// mutex_unlock(&client_data->lock);
		// rslt = scnprintf(buf, PAGE_SIZE, "%d %d %d %d %d %d %lld\n",
		// 				 acc_x,
		// 				 acc_y,
		// 				 acc_z,
		// 				 gyr_x,
		// 				 gyr_y,
		// 				 gyr_z,
		// 				 bmi_sync_data_time);
		mutex_lock(&client_data->lock);
		rslt = scnprintf(buf, PAGE_SIZE, "%d %d %d %d %d %d %lld\n",
						 accel_data.x,
						 accel_data.y,
						 accel_data.z,
						 gyro_data.x,
						 gyro_data.y,
						 gyro_data.z,
						 bmi_sync_data_time);
		mutex_unlock(&client_data->lock);
	}
	else
	{
		rslt = scnprintf(buf, PAGE_SIZE, "Need to echo 1 > data_sync first!\n");
	}
	return rslt;
}

/**
 * data_sync_store - sysfs read callback which gives the
 * raw accelerometer nd gyro value from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t data_sync_store(struct device *dev,
							   struct device_attribute *attr, const char *buf, size_t count)
{
	int rslt;
	long unsigned int sync_enable = 0;
	struct input_dev *input = to_input_dev(dev);

	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	rslt = kstrtoul(buf, 10, &sync_enable);
	check_error("acc soft reset receive input", rslt);
	if (sync_enable == 1 && client_data->data_sync_en != 1)
	{
		rslt = data_sync_int_config(client_data, BMI08X_ENABLE);
		if (rslt == BMI08X_OK)
			return count;
		return -EIO;
	}
	else
	{
		PERR("Invalid input or already enable data sync!\n use: echo 1 > data_sync");
		return -EINVAL;
	}
}

/**
 * acc_soft_reset_store - sysfs write callback which performs sensor soft reset
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t acc_soft_reset_store(struct device *dev,
									struct device_attribute *attr,
									const char *buf, size_t count)
{
	int rslt;
	unsigned long soft_reset;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	/* Base of decimal number system is 10 */
	rslt = kstrtoul(buf, 10, &soft_reset);
	check_error("acc soft reset receive input", rslt);

	if (soft_reset)
	{
		rslt = bmi08a_soft_reset(&client_data->device);
		check_error("acc soft reset", rslt);
	}
	else
	{
		PERR("Invalid input\n use: echo 1 > acc_soft_reset");
		return -EINVAL;
	}
	PINFO("soft reset success\n");
	return count;
}

/**
 * sensor_time_show - sysfs callback which reads sensor time.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t sensor_time_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	u32 sensor_time;

	rslt = bmi08a_get_sensor_time(&client_data->device, &sensor_time);
	check_error("get sensor time", rslt);

	return scnprintf(buf, PAGE_SIZE, "sensor_time:%d\n", sensor_time);
}

/**
 * acc_fifo_config_show - sysfs callback which reads sensor fifo configuration.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t acc_fifo_config_show(struct device *dev,
									struct device_attribute *attr, char *buf)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	struct bmi08x_accel_fifo_config config;
	u16 acc_fifo_wm;

	rslt = bmi08a_get_fifo_config(&config, &client_data->device);
	check_error("get fifo config", rslt);
	rslt = bmi08a_get_fifo_wm(&acc_fifo_wm, &client_data->device);
	check_error("get fifo wm", rslt);
	return scnprintf(buf, PAGE_SIZE,
					 "acc_en:%d mode:%d wm:%d\n",
					 config.accel_en, config.mode, acc_fifo_wm);
}

/**
 * acc_fifo_config_store - sysfs write callback which sets acc fifo config.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t acc_fifo_config_store(struct device *dev,
									 struct device_attribute *attr,
									 const char *buf, size_t count)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	struct bmi08x_accel_fifo_config config;
	unsigned int data[4] = {0};
	u16 fifo_wm;

	rslt = sscanf(buf, "%11d %11d %11d", &data[0], &data[1], &data[2]);
	if (rslt != 3)
	{
		PERR("Invalid argument\n"
			 "usage echo accel_en mode wm > acc_fifo_config");
		return -EINVAL;
	}

	rslt = bmi08a_get_fifo_config(&config, &client_data->device);
	check_error("get fifo config", rslt);
	config.accel_en = (u8)data[0];
	config.mode = (u8)data[1];
	fifo_wm = (u16)data[2];

	mutex_lock(&client_data->lock);
	rslt = bmi08a_set_fifo_config(&config, &client_data->device);
	check_error("set fifo config", rslt);
	rslt = bmi08a_set_fifo_wm(fifo_wm, &client_data->device);
	check_error("set fifo wm", rslt);
	mutex_unlock(&client_data->lock);
	return count;
}

/**
 * acc_fifo_dataframe_show - sysfs callback which reads sensor fifo data frames.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t acc_fifo_dataframe_show(struct device *dev,
									   struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	struct bmi08x_fifo_frame fifo_frame = {0};
	struct bmi08x_sensor_data bmi08x_accel[100] = {{0}};
	u8 fifo_data[1024] = {0};
	u16 idx = 0;
	u16 accel_length = 100;
	u16 fifo_length;
	int rslt;

	fifo_frame.data = fifo_data;
	fifo_frame.length = 1024;
	accel_length = 100;
	rslt = bmi08a_get_fifo_length(&fifo_length, &client_data->device);
	check_error("bmi08a_get_fifo_length", rslt);
	PINFO("FIFO buffer size : %d\n", fifo_frame.length);
	PINFO("FIFO length available : %d\n\n", fifo_length);
	PINFO("Requested data frames before parsing: %d\n", accel_length);
	rslt = bmi08a_read_fifo_data(&fifo_frame, &client_data->device);
	check_error("bmi08a_read_fifo_data", rslt);

	rslt = bmi08a_extract_accel(bmi08x_accel, &accel_length, &fifo_frame,
								&client_data->device);
	check_error("bmi08a_extract_accel", rslt);
	PINFO("Parsed accelerometer frames: %d\n", accel_length);

	for (idx = 0; idx < accel_length; idx++)
	{
		PINFO("ACCEL[%d] X : %d\t Y : %d\t Z : %d\n",
			  idx,
			  bmi08x_accel[idx].x,
			  bmi08x_accel[idx].y,
			  bmi08x_accel[idx].z);
	}
	return rslt;
}

/**
 * config_function_show - sysfs callback which gives feature interrupt
 * settings configured.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t config_function_show(struct device *dev,
									struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	return scnprintf(buf, PAGE_SIZE,
					 "acc_data_ready%d=%d\nacc_fifo_wm%d=%d\nacc_fifo_full%d=%d\n"
					 "gyr_data_ready%d=%d\ngyr_fifo_wm%d=%d\ngyr_fifo_full%d=%d\n",
					 BMI088_ACC_DATA_READY, client_data->acc_drdy_en,
					 BMI088_ACC_FIFO_WM, client_data->acc_fifo_wm_en,
					 BMI088_ACC_FIFO_FULL, client_data->acc_fifo_full_en,
					 BMI088_GYRO_DATA_READY, client_data->gyro_drdy_en,
					 BMI088_GYRO_FIFO_WM, client_data->gyro_fifo_wm_en,
					 BMI088_GYRO_FIFO_FULL, client_data->gyro_fifo_full_en);
}

/**
 * config_function_store - sysfs callback which sets feature interrupt
 * configurations of the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t config_function_store(struct device *dev,
									 struct device_attribute *attr,
									 const char *buf, size_t count)
{
	int rslt;
	int config_func = 0;
	int enable = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	rslt = sscanf(buf, "%11d %11d", &config_func, &enable);

	if (rslt != 2)
	{
		PERR("Invalid argument\n");
		PERR("use: echo feature_idx enable/disable > config_function");
		return -EINVAL;
	}

	rslt = feature_config_set(client_data, config_func, enable);
	check_error("en/disable feature", rslt);
	return count;
}

/**
 * acc_reg_sel_show - sysfs read callback which provides the register
 * address selected.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t acc_reg_sel_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	return scnprintf(buf, 64, "reg=0X%02X, len=%d\n",
					 client_data->acc_reg_sel, client_data->acc_reg_len);
}

/**
 * acc_reg_sel_store - sysfs write callback which stores the register
 * address to be selected.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t acc_reg_sel_store(struct device *dev,
								 struct device_attribute *attr, const char *buf,
								 size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	int rslt;

	rslt = sscanf(buf, "%11X %11d",
				  &client_data->acc_reg_sel, &client_data->acc_reg_len);
	if ((rslt != 2) || (client_data->acc_reg_len > 128) || (client_data->acc_reg_sel > 127))
	{
		PERR("Invalid argument");
		return -EINVAL;
	}
	return count;
}

/**
 * acc_reg_val_show - sysfs read callback which shows the register
 * value which is read from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t acc_reg_val_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	int rslt;
	u8 reg_data[128];
	int i;
	int pos;

	if ((client_data->acc_reg_len > 128) || (client_data->acc_reg_sel > 127))
	{
		PERR("Invalid argument");
		return -EINVAL;
	}
	mutex_lock(&client_data->lock);
	rslt = bmi08a_get_regs(client_data->acc_reg_sel, reg_data,
						   client_data->acc_reg_len, &client_data->device);
	mutex_unlock(&client_data->lock);
	check_error("get register data", rslt);
	pos = 0;
	for (i = 0; i < client_data->acc_reg_len; ++i)
	{
		pos += scnprintf(buf + pos, 16, "%02X", reg_data[i]);
		buf[pos++] = (i + 1) % 16 == 0 ? '\n' : ' ';
	}
	if (buf[pos - 1] == ' ')
		buf[pos - 1] = '\n';
	return pos;
}

/**
 * acc_reg_val_store - sysfs write callback which stores the register
 * value which is to be written in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t acc_reg_val_store(struct device *dev,
								 struct device_attribute *attr, const char *buf,
								 size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	int rslt;
	u8 reg_data[128] = {
		0,
	};
	int i, j, status, digit;

	status = 0;
	mutex_lock(&client_data->lock);
	/* Lint -save -e574 */
	for (i = j = 0; i < count && j < client_data->acc_reg_len; ++i)
	{
		/* Lint -restore */
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' ||
			buf[i] == '\r')
		{
			status = 0;
			++j;
			continue;
		}
		digit = buf[i] & 0x10 ? (buf[i] & 0xF) : ((buf[i] & 0xF) + 9);
		PDEBUG("digit is %d", digit);
		switch (status)
		{
		case 2:
			// ++j; /* Fall thru */
		case 0:
			if (status == 2) {
				++j;
			}
			reg_data[j] = digit;
			status = 1;
			break;
		case 1:
			reg_data[j] = reg_data[j] * 16 + digit;
			status = 2;
			break;
		}
	}
	if (status > 0)
		++j;
	if (j > client_data->acc_reg_len)
		j = client_data->acc_reg_len;
	else if (j < client_data->acc_reg_len)
	{
		PERR("Invalid argument");
		mutex_unlock(&client_data->lock);
		return -EINVAL;
	}
	PDEBUG("Reg data read as");
	for (i = 0; i < j; ++i)
		PDEBUG("%d", reg_data[i]);
	rslt = bmi08a_set_regs(client_data->acc_reg_sel, reg_data,
						   client_data->acc_reg_len, &client_data->device);
	mutex_unlock(&client_data->lock);
	check_error("set register data", rslt);
	return count;
}

/**
 * gyr_soft_reset_store - sysfs write callback which performs sensor soft reset
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyr_soft_reset_store(struct device *dev,
									struct device_attribute *attr,
									const char *buf, size_t count)
{
	int rslt;
	unsigned long soft_reset;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	/* Base of decimal number system is 10 */
	rslt = kstrtoul(buf, 10, &soft_reset);
	check_error("get gyr_soft_reset input", rslt);

	if (soft_reset)
	{
		/* Perform soft reset */
		rslt = bmi08g_soft_reset(&client_data->device);
		check_error("gyro soft reset", rslt);
	}
	else
	{
		PERR("invalid input\nuse: echo 1 > gyr_soft_reset");
		return -EINVAL;
	}
	return count;
}

/**
 * gyr_fifo_dataframe_show - sysfs callback which reads
 * gyroscope sensor fifo data frames.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyr_fifo_dataframe_show(struct device *dev,
									   struct device_attribute *attr, char *buf)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	struct bmi08x_fifo_frame fifo_frame = {0};
	struct bmi08x_sensor_data bmi08x_gyro[100] = {{0}};
	struct bmi08x_gyr_fifo_config gyr_conf = {0};
	u8 fifo_data[600] = {0};
	u16 idx = 0;
	u16 gyro_length;

	fifo_frame.data = fifo_data;
	fifo_frame.length = 600;
	gyro_length = 100;
	rslt = bmi08g_get_fifo_config(&gyr_conf, &client_data->device);
	check_error("bmi08g_get_fifo_config", rslt);
	rslt = bmi08g_get_fifo_length(&gyr_conf, &fifo_frame);
	check_error("bmi08g_get_fifo_length", rslt);
	rslt = bmi08g_read_fifo_data(&fifo_frame, &client_data->device);
	check_error("bmi08g_read_fifo_data", rslt);

	PINFO("FIFO length available : %d\n\n", fifo_frame.length);
	PINFO("Requested data frames before parsing: %d\n", gyro_length);
	bmi08g_extract_gyro(bmi08x_gyro, &gyro_length, &gyr_conf, &fifo_frame);
	check_error("bmi08g_extract_gyro", rslt);

	PINFO("Parsed gyroscope frames: %d\n", gyr_conf.frame_count);
	for (idx = 0; idx < gyr_conf.frame_count; idx++)
	{
		PINFO("GYRO[%d] X : %d\t Y : %d\t Z : %d\n",
			  idx,
			  bmi08x_gyro[idx].x,
			  bmi08x_gyro[idx].y,
			  bmi08x_gyro[idx].z);
	}
	return rslt;
}

/**
 * gyr_fifo_config_show - sysfs callback which reads sensor fifo configuration.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyr_fifo_config_show(struct device *dev,
									struct device_attribute *attr, char *buf)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	struct bmi08x_gyr_fifo_config config;

	rslt = bmi08g_get_fifo_config(&config, &client_data->device);
	check_error("get gyro fifo config", rslt);

	return scnprintf(buf, PAGE_SIZE,
					 "tag:%d wm_lvl:%d mode:%d\n",
					 config.tag, config.wm_level, config.mode);
}

/**
 * gyr_fifo_config_store - sysfs callback which writes gyroscope
 * sensor fifo configuration.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyr_fifo_config_store(struct device *dev,
									 struct device_attribute *attr,
									 const char *buf, size_t count)
{
	int rslt;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	struct bmi08x_gyr_fifo_config config;
	unsigned int data[3] = {0};

	rslt = sscanf(buf, "%11d %11d  %11d	%11d", &data[0], &data[1], &data[2],
				  &data[3]);

	if (rslt != 3)
	{
		PERR("Invalid argument\n"
			 "usage echo tag wm_lvl mode > gyr_fifo_config");
		return -EINVAL;
	}

	config.tag = (u8)data[0];
	config.wm_level = (u8)data[1];
	config.mode = (u8)data[2];

	mutex_lock(&client_data->lock);
	rslt = bmi08g_set_fifo_config(&config, &client_data->device);
	mutex_unlock(&client_data->lock);
	check_error("set gyro fifo config", rslt);
	return count;
}

/**
 * gyr_selftest_store - sysfs write callback which performs the self test
 * in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyr_selftest_store(struct device *dev,
								  struct device_attribute *attr,
								  const char *buf, size_t count)
{
	int rslt;
	unsigned long selftest;
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	rslt = kstrtoul(buf, 10, &selftest);
	if (rslt)
		return rslt;
	if (selftest == 1)
	{
		mutex_lock(&client_data->lock);
		rslt = bmi08g_perform_selftest(&client_data->device);
		if (rslt)
			PDEBUG("gyro self test failed with rslt code:%d", rslt);
		else
			PDEBUG("gyro self test success");
		mutex_unlock(&client_data->lock);
	}
	else
	{
		PERR("Invalid input\nuse: echo 1 > gyr_selftest");
		return -EINVAL;
	}
	return count;
}

/**
 * gyr_reg_sel_show - sysfs read callback which provides the register
 * address selected.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyr_reg_sel_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);

	return scnprintf(buf, 64, "reg=0X%02X, len=%d\n",
					 client_data->gyro_reg_sel, client_data->gyro_reg_len);
}

/**
 * gyr_reg_sel_store - sysfs write callback which stores the register
 * address to be selected.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyr_reg_sel_store(struct device *dev,
								 struct device_attribute *attr, const char *buf,
								 size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	int rslt;

	rslt = sscanf(buf, "%11X %11d",
				  &client_data->gyro_reg_sel, &client_data->gyro_reg_len);
	if ((rslt != 2) || (client_data->gyro_reg_len > 128) || (client_data->gyro_reg_sel > 127))
	{
		PERR("Invalid argument");
		return -EINVAL;
	}
	return count;
}

/**
 * gyr_reg_val_show - sysfs read callback which shows the register
 * value which is read from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyr_reg_val_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	int rslt;
	u8 reg_data[128];
	int i;
	int pos;

	if ((client_data->gyro_reg_len > 128) ||
		(client_data->gyro_reg_sel > 127))
	{
		PERR("Invalid argument");
		return -EINVAL;
	}
	mutex_lock(&client_data->lock);
	rslt = bmi08g_get_regs(client_data->gyro_reg_sel, reg_data,
						   client_data->gyro_reg_len, &client_data->device);
	mutex_unlock(&client_data->lock);
	check_error("read gyro register data", rslt);
	pos = 0;
	for (i = 0; i < client_data->gyro_reg_len; ++i)
	{
		pos += scnprintf(buf + pos, 16, "%02X", reg_data[i]);
		buf[pos++] = (i + 1) % 16 == 0 ? '\n' : ' ';
	}
	if (buf[pos - 1] == ' ')
		buf[pos - 1] = '\n';
	return pos;
}

/**
 * gyr_reg_val_store - sysfs write callback which stores the register
 * value which is to be written in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyr_reg_val_store(struct device *dev,
								 struct device_attribute *attr, const char *buf,
								 size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi08x_client_data *client_data = input_get_drvdata(input);
	int rslt;
	u8 reg_data[128] = {
		0,
	};
	int i, j, status, digit;

	status = 0;
	mutex_lock(&client_data->lock);
	/* Lint -save -e574 */
	for (i = j = 0; i < count && j < client_data->gyro_reg_len; ++i)
	{
		/* Lint -restore */
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' ||
			buf[i] == '\r')
		{
			status = 0;
			++j;
			continue;
		}
		digit = buf[i] & 0x10 ? (buf[i] & 0xF) : ((buf[i] & 0xF) + 9);
		PDEBUG("digit is %d", digit);
		switch (status)
		{
		case 2:
			// ++j; /* Fall thru */
		case 0:
			if (status == 2) {
				++j;
			}
			reg_data[j] = digit;
			status = 1;
			break;
		case 1:
			reg_data[j] = reg_data[j] * 16 + digit;
			status = 2;
			break;
		}
	}
	if (status > 0)
		++j;
	if (j > client_data->gyro_reg_len)
		j = client_data->gyro_reg_len;
	else if (j < client_data->gyro_reg_len)
	{
		PERR("Invalid argument");
		mutex_unlock(&client_data->lock);
		return -EINVAL;
	}
	PDEBUG("Reg data read as");
	for (i = 0; i < j; ++i)
		PDEBUG("%d", reg_data[i]);
	rslt = bmi08g_set_regs(client_data->gyro_reg_sel, reg_data,
						   client_data->gyro_reg_len, &client_data->device);
	mutex_unlock(&client_data->lock);
	check_error("set gyro register data", rslt);
	return count;
}

static DEVICE_ATTR_RW(sensor_init);
static DEVICE_ATTR_RO(chip_id);
static DEVICE_ATTR_RO(driver_version);
static DEVICE_ATTR_RW(data_sync);
static DEVICE_ATTR_RO(sensor_time);
static DEVICE_ATTR_RO(temperature);
static DEVICE_ATTR_RO(avail_sensor);
static DEVICE_ATTR_RW(config_function);

static DEVICE_ATTR_RW(acc_op_mode);
static DEVICE_ATTR_RO(acc_val);
static DEVICE_ATTR_WO(acc_soft_reset);
static DEVICE_ATTR_RW(acc_config);
static DEVICE_ATTR_WO(acc_selftest);
static DEVICE_ATTR_RW(acc_reg_sel);
static DEVICE_ATTR_RW(acc_reg_val);
static DEVICE_ATTR_RO(acc_fifo_dataframe);
static DEVICE_ATTR_RW(acc_fifo_config);

static DEVICE_ATTR_RW(gyr_op_mode);
static DEVICE_ATTR_RO(gyr_val);
static DEVICE_ATTR_WO(gyr_soft_reset);
static DEVICE_ATTR_RW(gyr_config);
static DEVICE_ATTR_WO(gyr_selftest);
static DEVICE_ATTR_RW(gyr_reg_sel);
static DEVICE_ATTR_RW(gyr_reg_val);
static DEVICE_ATTR_RO(gyr_fifo_dataframe);
static DEVICE_ATTR_RW(gyr_fifo_config);

static struct attribute *bmi08x_attributes[] = {
	&dev_attr_sensor_init.attr,
	&dev_attr_chip_id.attr,
	&dev_attr_driver_version.attr,
	&dev_attr_data_sync.attr,
	&dev_attr_sensor_time.attr,
	&dev_attr_temperature.attr,
	&dev_attr_avail_sensor.attr,
	&dev_attr_config_function.attr,

	&dev_attr_acc_op_mode.attr,
	&dev_attr_acc_val.attr,
	&dev_attr_acc_soft_reset.attr,
	&dev_attr_acc_config.attr,
	&dev_attr_acc_selftest.attr,
	&dev_attr_acc_reg_sel.attr,
	&dev_attr_acc_reg_val.attr,
	&dev_attr_acc_fifo_dataframe.attr,
	&dev_attr_acc_fifo_config.attr,

	&dev_attr_gyr_op_mode.attr,
	&dev_attr_gyr_val.attr,
	&dev_attr_gyr_soft_reset.attr,
	&dev_attr_gyr_config.attr,
	&dev_attr_gyr_selftest.attr,
	&dev_attr_gyr_reg_sel.attr,
	&dev_attr_gyr_reg_val.attr,
	&dev_attr_gyr_fifo_dataframe.attr,
	&dev_attr_gyr_fifo_config.attr,
	NULL};

static struct attribute_group bmi08x_attribute_group = {
	.attrs = bmi08x_attributes};

/**
 * bmi08x_feat_input_init - Register the feature input device in the
 * system.
 * @client_data : Instance of client data.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * Any Negative value - Error.
 */
static int bmi08x_feat_input_init(struct bmi08x_client_data *client_data)
{
	struct input_dev *dev;
	int rslt = 0;

	dev = input_allocate_device();
	if (dev == NULL)
		return -ENOMEM;
	dev->name = SENSOR_NAME_FEAT;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_MSC, REL_FEAT_STATUS);
	input_set_drvdata(dev, client_data);
	rslt = input_register_device(dev);
	if (rslt < 0)
	{
		input_free_device(dev);
		return rslt;
	}
	client_data->feat_input = dev;
	return 0;
}

/**
 * bmi08x_feat_input_destroy - Un-register the feature input device from
 * the system.
 *
 * @client_data :Instance of client data.
 */
static void bmi08x_feat_input_destroy(struct bmi08x_client_data *client_data)
{
	struct input_dev *dev = client_data->feat_input;

	input_unregister_device(dev);
}

/**
 * bmi08x_gyr_feat_input_init - Register the feature input device in the
 * system.
 * @client_data : Instance of client data.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * Any Negative value - Error.
 */
static int bmi08x_gyr_feat_input_init(struct bmi08x_client_data *client_data)
{
	struct input_dev *dev;
	int rslt = 0;

	dev = input_allocate_device();
	if (dev == NULL)
		return -ENOMEM;
	dev->name = SENSOR_NAME_GYR_FEAT;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_MSC, REL_GYR_FEAT_STATUS);
	input_set_events_per_packet(dev, 100);
	input_set_drvdata(dev, client_data);
	rslt = input_register_device(dev);
	if (rslt < 0)
	{
		input_free_device(dev);
		return rslt;
	}
	client_data->gyr_feat_input = dev;
	return 0;
}

/**
 * bmi08x_gyr_feat_input_destroy - Un-register the feature input device from
 * the system.
 *
 * @client_data :Instance of client data.
 */
static void bmi08x_gyr_feat_input_destroy(
	struct bmi08x_client_data *client_data)
{
	struct input_dev *dev = client_data->gyr_feat_input;

	input_unregister_device(dev);
}

/**
 * bmi08x_input_init - Register the input device in the
 * system.
 * @client_data : Instance of client data.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * Any Negative value - Error.
 */
static int bmi08x_input_init(struct bmi08x_client_data *client_data)
{
	struct input_dev *dev;
	int rslt = 0;

	dev = input_allocate_device();
	if (dev == NULL)
		return -ENOMEM;

	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;
	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_capability(dev, EV_MSC, REL_HW_STATUS);
	input_set_drvdata(dev, client_data);
	rslt = input_register_device(dev);
	if (rslt < 0)
	{
		input_free_device(dev);
		return rslt;
	}
	client_data->bmi_input = dev;
	return 0;
}

/**
 * bmi08x_input_destroy - Un-register the Accelerometer input device from
 * the system.
 *
 * @client_data :Instance of client data.
 */
static void bmi08x_input_destroy(struct bmi08x_client_data *client_data)
{
	struct input_dev *dev = client_data->bmi_input;

	input_unregister_device(dev);
}

/**
 * bmi08x_probe - Does Sensor initialization
 * @dev: Device instance
 * @client_data : Instance of client data.
 */
int bmi08x_probe(struct bmi08x_client_data *client_data, struct device *dev)
{
	pr_err("bmi088:START\n");
	int rslt = 0;

	dev_set_drvdata(dev, client_data);

	if ((rslt) || (client_data == NULL))
		goto exit_err_clean;

	client_data->dev = dev;
	client_data->device.delay_us = bmi08x_i2c_delay_us;
	/*lint -e86*/
	mutex_init(&client_data->lock);
	/*lint +e86*/
	rslt = bmi08x_input_init(client_data);
	if (rslt < 0)
		goto exit_err_clean;

	rslt = sysfs_create_group(&client_data->bmi_input->dev.kobj,
							  &bmi08x_attribute_group);
	if (rslt < 0)
		goto exit_err_clean;

	rslt = bmi08x_feat_input_init(client_data);
	if (rslt < 0)
		goto exit_err_clean;

	rslt = bmi08x_gyr_feat_input_init(client_data);
	if (rslt < 0)
		goto exit_err_clean;

	rslt = bmi08x_request_irq(client_data);
	if (rslt < 0)
	{
		PERR("ACC Request irq failed");
		return -EIO;
	}
	PINFO("ACC IRQ requested");

	rslt = bmi08x_gyr_request_irq(client_data);
	if (rslt < 0)
	{
		PERR("GYR Request irq failed");
		return -EIO;
	}
	PINFO("GYR IRQ requested");

	PINFO("sensor %s probed successfully", SENSOR_NAME);

	if (client_data == NULL)
		PINFO("NULL client_data");

	if (rslt < 0)
		goto exit_err_clean;

	return 0;

exit_err_clean:
	if (rslt)
	{
		if (client_data != NULL)
			kfree(client_data);
		return rslt;
	}
	return rslt;
}
/* Lint -save -e19 */
EXPORT_SYMBOL(bmi08x_probe);
/* Lint -restore */

/**
 * bmi08x_remove - This function removes the driver from the device.
 * @dev : Instance of the device.
 *
 * Return : Status of the suspend function.
 * * 0 - OK.
 * * Negative value : Error.
 */
int bmi08x_remove(struct device *dev)
{
	int rslt = 0;
	struct bmi08x_client_data *client_data = dev_get_drvdata(dev);

	if (client_data != NULL)
	{
		sysfs_remove_group(&client_data->bmi_input->dev.kobj,
						   &bmi08x_attribute_group);
		bmi08x_input_destroy(client_data);
		bmi08x_feat_input_destroy(client_data);
		bmi08x_gyr_feat_input_destroy(client_data);
		kfree(client_data);
	}
	return rslt;
}

/* Lint -save -e19 */
EXPORT_SYMBOL(bmi08x_remove);
/* Lint -restore */
