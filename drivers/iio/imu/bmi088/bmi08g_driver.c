/**
 * @section LICENSE
 * Copyright (c) 2017-2024 Bosch Sensortec GmbH All Rights Reserved.
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @file		bmi08g_driver.c
 * @date		2024-10-10
 * @version		v1.4.1
 *
 * @brief		 BMI08G Linux Driver
 *
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
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

/*********************************************************************/
/* Own header files */
/*********************************************************************/
#include "bmi08g_driver.h"
#include "bs_log.h"

/*********************************************************************/
/* Local macro definitions */
/*********************************************************************/
#define DRIVER_VERSION "1.4.1"
#define MS_TO_US(msec) UINT32_C((msec) * 1000)


/* Buffer size allocated to store raw FIFO data */
#define BMI08_GYR_FIFO_RAW_DATA_BUFFER_SIZE 600

/* Length of data to be read from FIFO */
#define BMI08_GYR_FIFO_RAW_DATA_USER_LENGTH 600

/* Number of Gyro frames to be extracted from FIFO */
#define BMI08_GYR_FIFO_EXTRACTED_DATA_FRAME_COUNT 100

/* gyro data lenght */
#define BMI08_GYR_DATA_LENGHT 50
/*********************************************************************/
/* Global data */
/*********************************************************************/
/**
 * fifo_dataframe_display - sysfs callback which reads
 * gyroscope sensor fifo data frames.
 *
 * @client_data : Instance of client data
 *
 * Return: Nothing
 */
static void fifo_dataframe_display(struct bmi08g_client_data *client_data);

/**
 * gyro_odr_settings - Handles invalid odr settings
 *
 * client_data : Instance of client data.
 *
 * @None
 */
static void gyro_odr_settings(struct bmi08g_client_data *client_data);

/**
 * bmi08_i2c_delay_ms - Adds a delay in units of millisecs.
 *
 * @msec: Delay value in millisecs.
 */
static void bmi08_i2c_delay_us(u32 usec, void *intf_ptr)
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
static const struct iio_event_spec bmi08g_event = {
	.type = IIO_EV_TYPE_THRESH,
	.dir = IIO_EV_DIR_RISING,
	.mask_shared_by_type = BIT(IIO_EV_INFO_VALUE) |
						   BIT(IIO_EV_INFO_ENABLE)};

#define BMI08G_CHANNELS_CONFIG(device_type, si, mod, addr) \
	{                                                      \
		.type = device_type,                               \
		.modified = 1,                                     \
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),      \
		.scan_index = si,                                  \
		.channel2 = mod,                                   \
		.address = addr,                                   \
		.scan_type = {                                     \
			.sign = 's',                                   \
			.realbits = 16,                                \
			.shift = 0,                                    \
			.storagebits = 16,                             \
			.endianness = IIO_LE,                          \
		},                                                 \
		.event_spec = &bmi08g_event,                       \
		.num_event_specs = 1}

#define BMI08G_BYTE_FOR_PER_AXIS_CHANNEL 2

/* scan element definition */
enum BMI08G_AXIS_SCAN {
	BMI08G_SCAN_GYRO_X,
	BMI08G_SCAN_GYRO_Y,
	BMI08G_SCAN_GYRO_Z,
	BMI08G_SCAN_TIMESTAMP,
};

/*iio chan spec for  BMI08G sensor*/
static const struct iio_chan_spec bmi08g_iio_channels[] = {
	/*lint -e446*/
	/*gyro channel*/
	BMI08G_CHANNELS_CONFIG(IIO_ANGL_VEL, BMI08G_SCAN_GYRO_X,
						   IIO_MOD_X, BMI08_REG_GYRO_X_LSB),
	BMI08G_CHANNELS_CONFIG(IIO_ANGL_VEL, BMI08G_SCAN_GYRO_Y,
						   IIO_MOD_Y, BMI08_REG_GYRO_Y_LSB),
	BMI08G_CHANNELS_CONFIG(IIO_ANGL_VEL, BMI08G_SCAN_GYRO_Z,
						   IIO_MOD_Z, BMI08_REG_GYRO_Z_LSB),
	/*lint +e446*/
	/*ap timestamp channel*/
	IIO_CHAN_SOFT_TIMESTAMP(BMI08G_SCAN_TIMESTAMP)

};

/*!
 * @brief sysfs write callback which performs the iio generic buffer test
 *
 * @param[in] dev	: Device instance.
 * @param[in] attr	: Instance of device attribute file.
 * @param[in] buf	: Instance of the data buffer which serves as input.
 * @param[in] count : Number of characters in the buffer `buf`.
 *
 * @return Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t iio_generic_buffer_store(struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count)
{
	int rslt;
	unsigned long iio_test;
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);

	/* Base of decimal number system is 10 */
	rslt = kstrtoul(buf, 10, &iio_test);

	if (rslt || (iio_test != 0 && iio_test != 1)) {
		PERR("iio_generic_buffer : invalid input");
		return -EIO;
	}

	if (iio_test)
		/*lint -e534*/
		bmi08x_iio_allocate_trigger(input, 1);
	/*lint +e534*/
	else
		bmi08x_iio_deallocate_trigger(input);

	return count;
}

/**
 * gyro_odr_settings - Handles invalid odr settings
 *
 * @client_data : Instance of client data.
 * @None
 */
static void gyro_odr_settings(struct bmi08g_client_data *client_data)
{
	int rslt;

	rslt = bmi08g_get_meas_conf(&client_data->device);
	check_error("get meas gyro config", rslt);
	if (client_data->device.gyro_cfg.odr == BMI08_GYRO_BW_532_ODR_2000_HZ ||
		client_data->device.gyro_cfg.odr == BMI08_GYRO_BW_230_ODR_2000_HZ) {
		client_data->device.gyro_cfg.odr = BMI08_GYRO_BW_32_ODR_100_HZ;
		rslt = bmi08g_set_meas_conf(&client_data->device);
		check_error("set meas gyro conf", rslt);
	}
}
/* enables or disables selected feature index from config function store*/
static int feature_config_set(struct bmi08g_client_data *client_data,
							  int config_func, int enable)
{
	int rslt;
	struct bmi08_gyro_int_channel_cfg gyro_int_config;

	PDEBUG("config_func = %d, enable=%d", config_func, enable);
	if (config_func < 0 || config_func > BMI088_GYRO_FIFO_FULL)
		return -EINVAL;

	switch (config_func) {
	case BMI088_GYRO_DATA_READY:
		gyro_int_config.int_type = BMI08_GYRO_INT_DATA_RDY;
		client_data->gyro_drdy_en = enable;
		break;
	case BMI088_GYRO_FIFO_WM:
		gyro_int_config.int_type = BMI08_GYRO_INT_FIFO_WM;
		client_data->gyro_fifo_wm_en = enable;
		break;
	case BMI088_GYRO_FIFO_FULL:
		gyro_int_config.int_type = BMI08_GYRO_INT_FIFO_FULL;
		client_data->gyro_fifo_full_en = enable;
		break;
	default:
		PERR("Invalid feature selection: %d", config_func);
		return -EINVAL;
	}


	if (config_func != BMI088_GYRO_DATA_READY)
		gyro_odr_settings(client_data);

	gyro_int_config.int_channel = BMI08_INT_CHANNEL_3;
	gyro_int_config.int_pin_cfg.output_mode = BMI08_INT_MODE_PUSH_PULL;
	gyro_int_config.int_pin_cfg.lvl = BMI08_INT_ACTIVE_HIGH;
	gyro_int_config.int_pin_cfg.enable_int_pin = enable;
	rslt = bmi08g_set_int_config(
		(const struct bmi08_gyro_int_channel_cfg *)&gyro_int_config,
		&client_data->device);
	check_error("en/disabling interrupt", rslt);
	return rslt;
}

/* enables or disables data sync interrupt configurations*/
static int gyro_data_sync_int_config(struct bmi08g_client_data *client_data,
								int enable)
{
	struct bmi08_int_cfg int_config;
	struct bmi08_data_sync_cfg sync_cfg;
	int rslt;

	mutex_lock(&client_data->lock);
	if (enable) {
		client_data->device.gyro_cfg.range = BMI08_GYRO_RANGE_2000_DPS;
		sync_cfg.mode = BMI08_ACCEL_DATA_SYNC_MODE_2000HZ;
	} else
		sync_cfg.mode = BMI08_ACCEL_DATA_SYNC_MODE_OFF;
	rslt = bmi08a_configure_data_synchronization(sync_cfg,
												 &client_data->device);
	check_error("config data sync", rslt);
	int_config.accel_int_config_1.int_channel = BMI08_INT_CHANNEL_2;
	int_config.accel_int_config_1.int_type = BMI08_ACCEL_SYNC_INPUT;
	int_config.accel_int_config_1.int_pin_cfg.output_mode =
		BMI08_INT_MODE_PUSH_PULL;
	int_config.accel_int_config_1.int_pin_cfg.lvl = BMI08_INT_ACTIVE_HIGH;
	int_config.accel_int_config_1.int_pin_cfg.enable_int_pin = enable;

	int_config.accel_int_config_2.int_channel = BMI08_INT_CHANNEL_1;
	int_config.accel_int_config_2.int_type = BMI08_ACCEL_INT_SYNC_DATA_RDY;
	int_config.accel_int_config_2.int_pin_cfg.output_mode =
		BMI08_INT_MODE_PUSH_PULL;
	int_config.accel_int_config_2.int_pin_cfg.lvl = BMI08_INT_ACTIVE_HIGH;
	int_config.accel_int_config_2.int_pin_cfg.enable_int_pin = enable;

	int_config.gyro_int_config_1.int_channel = BMI08_INT_CHANNEL_4;
	int_config.gyro_int_config_1.int_type = BMI08_GYRO_INT_DATA_RDY;
	int_config.gyro_int_config_1.int_pin_cfg.enable_int_pin = enable;
	int_config.gyro_int_config_1.int_pin_cfg.lvl = BMI08_INT_ACTIVE_HIGH;
	int_config.gyro_int_config_1.int_pin_cfg.output_mode =
		BMI08_INT_MODE_PUSH_PULL;

	int_config.gyro_int_config_2.int_channel = BMI08_INT_CHANNEL_3;
	int_config.gyro_int_config_2.int_type = BMI08_GYRO_INT_DATA_RDY;
	int_config.gyro_int_config_2.int_pin_cfg.enable_int_pin = enable;
	int_config.gyro_int_config_2.int_pin_cfg.lvl = BMI08_INT_ACTIVE_HIGH;
	int_config.gyro_int_config_2.int_pin_cfg.output_mode =
		BMI08_INT_MODE_PUSH_PULL;

	rslt += bmi08a_set_data_sync_int_config(&int_config, &client_data->device);
	client_data->data_sync_en = enable;
	check_error("data sync interrupt config", rslt);
	mutex_unlock(&client_data->lock);
	return rslt;
}

/**
 *	bmi08_gyr_irq_work_func - Bottom half handler for feature interrupts.
 *	@work : Work data for the workqueue handler.
 */
static void bmi08_gyr_irq_work_func(struct work_struct *work)
{
	/*lint -e26 -e10 -e124 -e40*/
	struct bmi08g_client_data *g_client_data = container_of(work,
								struct bmi08g_client_data, gyr_irq_work);
	/*lint +e26  +e10 +e124 +e40*/
	int rslt = 0;
	struct bmi08_sensor_data accel_data;
	struct bmi08_sensor_data gyro_data;

	mutex_lock(&g_client_data->lock);
	rslt = bmi08g_get_data_int_status(&g_client_data->int_status,
									  &g_client_data->device);
	mutex_unlock(&g_client_data->lock);

	if (g_client_data->int_status != 0) {
		PINFO("INT status:0x%x\n", g_client_data->int_status);
		if ((g_client_data->int_status & BMI08_GYRO_DATA_READY_INT) &&
			(g_client_data->data_sync_en == 1)) {
			PINFO("DATA SYNC INT Occurred\n");
			mutex_lock(&g_client_data->lock);
			rslt = bmi08a_get_synchronized_data(&accel_data, &gyro_data,
												&g_client_data->device);
			if (rslt)
				PERR("bmi08a_get_synchronized_data failed with rslt:%d", rslt);
			else
				PINFO("%d %d %d\n",
					  gyro_data.x,
					  gyro_data.y,
					  gyro_data.z);
			mutex_unlock(&g_client_data->lock);
			rslt = gyro_data_sync_int_config(g_client_data, BMI08_DISABLE);
		} else if (g_client_data->int_status & BMI08_GYRO_DATA_READY_INT) {
			PINFO("GYRO DRDY INT Occurred\n");
			mutex_lock(&g_client_data->lock);
			rslt = bmi08g_get_data(&gyro_data, &g_client_data->device);
			mutex_unlock(&g_client_data->lock);
			check_error("get gyro data", rslt);
			PINFO("GYRO X:%d Y:%d Z:%d\n",
				  gyro_data.x, gyro_data.y, gyro_data.z);
			rslt = feature_config_set(g_client_data, BMI088_GYRO_DATA_READY,
									  BMI08_DISABLE);
		}
		if ((g_client_data->int_status & BMI08_GYRO_FIFO_FULL_INT) &&
			(g_client_data->gyro_fifo_full_en == BMI08_ENABLE)) {
			PINFO("GYRO FIFO FULL INT occurred\n");
			fifo_dataframe_display(g_client_data);
		}
		if ((g_client_data->int_status & BMI08_GYRO_FIFO_WM_INT) &&
			(g_client_data->gyro_fifo_wm_en == BMI08_ENABLE)) {
			PINFO("GYRO FIFO WM INT occurred\n");
			fifo_dataframe_display(g_client_data);
		}
		check_error("gyro INT handle", rslt);
	}
}

/**
 * bmi08_gyr_irq_handle - IRQ handler function.
 * @irq : Number of irq line.
 * @handle : Instance of client data.
 *
 * Return : Status of IRQ function.
 */
static irqreturn_t bmi08_gyr_irq_handle(int irq, void *handle)
{
	struct bmi08g_client_data *g_client_data = handle;

	if (schedule_work(&g_client_data->gyr_irq_work))
		return IRQ_HANDLED;
	return IRQ_HANDLED;
}


/**
 * gyr_request_irq - Allocates interrupt resources and enables the
 * interrupt line and IRQ handling.
 *
 * @g_client_data: Instance of the client data.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * Any Negative value - Error.
 */
static int gyr_request_irq(struct bmi08g_client_data *g_client_data)
{
	int rslt = 0;

	PINFO("GYR IRQ requested for pin : %d\n", g_client_data->GYR_IRQ);
	rslt = request_irq(g_client_data->GYR_IRQ, bmi08_gyr_irq_handle,
					   IRQF_TRIGGER_RISING,
					   SENSOR_NAME, g_client_data);
	if (rslt < 0) {
		PERR("request irq failed with rslt:%d", rslt);
		return -EIO;
	}
	INIT_WORK(&g_client_data->gyr_irq_work, bmi08_gyr_irq_work_func);
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
	u8 gyr_chip_id = 0;
	u8 rslt;
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	rslt = bmi08g_get_regs(BMI08_REG_GYRO_CHIP_ID, &gyr_chip_id,
						   1, &g_client_data->device);
	check_error("get gyr chip id", rslt);
	return scnprintf(buf, PAGE_SIZE, "chip_id gyr:0x%x\n",
					gyr_chip_id);
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
	return scnprintf(buf, 32, "bmi08g\n");
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	rslt = bmi08g_get_power_mode(&g_client_data->device);
	check_error("gyro get power mode", rslt);
	if (g_client_data->device.gyro_cfg.power == BMI08_GYRO_PM_NORMAL)
		return scnprintf(buf, PAGE_SIZE,
						 "gyr_op_mode : BMI08_GYRO_PM_NORMAL\n");
	else if (g_client_data->device.gyro_cfg.power == BMI08_GYRO_PM_DEEP_SUSPEND)
		return scnprintf(buf, PAGE_SIZE,
						 "gyr_op_mode : BMI08_GYRO_PM_DEEP_SUSPEND\n");
	else if (g_client_data->device.gyro_cfg.power == BMI08_GYRO_PM_SUSPEND)
		return scnprintf(buf, PAGE_SIZE,
						 "gyr_op_mode : BMI08_GYRO_PM_SUSPEND\n");
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
								 struct device_attribute *attr,
								 const char *buf, size_t count)
{
	int rslt;
	unsigned long op_mode;

	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	rslt = kstrtoul(buf, 10, &op_mode);
	check_error("gyr_op_mode receive input", rslt);
	if (op_mode == 0)
		g_client_data->device.gyro_cfg.power = BMI08_GYRO_PM_SUSPEND;
	else if (op_mode == 1)
		g_client_data->device.gyro_cfg.power = BMI08_GYRO_PM_NORMAL;
	else if (op_mode == 2)
		g_client_data->device.gyro_cfg.power = BMI08_GYRO_PM_DEEP_SUSPEND;
	else {
		PERR("pwr mode Invalid input:\n0->suspend 1->normal 2->deep_suspend");
		return -EINVAL;
	}
	mutex_lock(&g_client_data->lock);
	rslt = bmi08g_set_power_mode(&g_client_data->device);
	mutex_unlock(&g_client_data->lock);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);
	struct bmi08_sensor_data gyro_data;

	mutex_lock(&g_client_data->lock);
	rslt = bmi08g_get_data(&gyro_data, &g_client_data->device);
	mutex_unlock(&g_client_data->lock);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	mutex_lock(&g_client_data->lock);
	rslt = bmi08g_get_meas_conf(&g_client_data->device);
	check_error("get meas gyro config", rslt);
	mutex_unlock(&g_client_data->lock);
	return scnprintf(buf, PAGE_SIZE, "gyro_cfg odr:%d bw:%d range:%d\n",
					 g_client_data->device.gyro_cfg.odr,
					 g_client_data->device.gyro_cfg.bw,
					 g_client_data->device.gyro_cfg.range);
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
								struct device_attribute *attr,
								const char *buf, size_t count)
{
	int rslt;
	unsigned int data[3] = {0};
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	rslt = sscanf(buf, "%11d %11d %11d ", &data[0], &data[1], &data[2]);
	if (rslt != 3) {
		PINFO("Invalid input\nuse echo odr bandwidth range > gyr_conf");
		return -EIO;
	}
	mutex_lock(&g_client_data->lock);
	rslt = bmi08g_get_meas_conf(&g_client_data->device);
	check_error("get meas gyro conf", rslt);
	g_client_data->device.gyro_cfg.odr = (u8)data[0];
	g_client_data->device.gyro_cfg.bw = (u8)data[1];
	g_client_data->device.gyro_cfg.range = (u8)data[2];
	rslt = bmi08g_set_meas_conf(&g_client_data->device);
	check_error("set meas gyro conf", rslt);
	mutex_unlock(&g_client_data->lock);
	return count;
}

/**
 * gyro_sensor_init - sesnor initialization.
 *
 * @dev: Device instance.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static int gyro_sensor_init(struct bmi08g_client_data *g_client_data)
{
	int rslt = 0;

	rslt = bmi08g_init(&g_client_data->device);
	if (rslt) {
		PERR("gyro sensor init failed wtih rslt:%d", rslt);
		return -EIO;
	}
	PINFO("gyro initilized\n");
	rslt = bmi08g_soft_reset(&g_client_data->device);
	if (rslt) {
		PERR("sensor soft reset failed wtih rslt:%d", rslt);
		return -EIO;
	}

	bmi08_i2c_delay_us(MS_TO_US(300), &g_client_data->device.intf_ptr_gyro);

	g_client_data->device.gyro_cfg.power = BMI08_GYRO_PM_NORMAL;
	rslt |= bmi08g_set_power_mode(&g_client_data->device);

	if (rslt < 0) {
		PINFO("GYRO POWER MODE SET FIALED");
		return -EIO;
	}
	PINFO("Gyro power mode set to NORMAL");
	g_client_data->data_sync_en = 0;
	return rslt;
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

	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	rslt = gyro_data_sync_int_config(g_client_data, BMI08_ENABLE);
	if (rslt == BMI08_OK)
		return scnprintf(buf, PAGE_SIZE, "data sync configuration success\n");
	return scnprintf(buf, PAGE_SIZE, "data sync config failed\n");
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	return scnprintf(buf, PAGE_SIZE,
					 "gyr_data_ready%d=%d\ngyr_fifo_wm%d=%d\ngyr_fifo_full%d=%d\n",
					 BMI088_GYRO_DATA_READY, g_client_data->gyro_drdy_en,
					 BMI088_GYRO_FIFO_WM, g_client_data->gyro_fifo_wm_en,
					 BMI088_GYRO_FIFO_FULL, g_client_data->gyro_fifo_full_en);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	rslt = sscanf(buf, "%11d %11d", &config_func, &enable);

	if (rslt != 2) {
		PERR("Invalid argument\n");
		PERR("use: echo feature_idx enable/disable > config_function");
		return -EINVAL;
	}

	rslt = feature_config_set(g_client_data, config_func, enable);
	check_error("en/disable feature", rslt);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	/* Base of decimal number system is 10 */
	rslt = kstrtoul(buf, 10, &soft_reset);
	check_error("get gyr_soft_reset input", rslt);

	if (soft_reset) {
		/* Perform soft reset */
		rslt = bmi08g_soft_reset(&g_client_data->device);
		check_error("gyro soft reset", rslt);
	} else {
		PERR("invalid input\nuse: echo 1 > gyr_soft_reset");
		return -EINVAL;
	}
	return count;
}
/**
 * fifo_dataframe_display - sysfs callback which reads
 * gyroscope sensor fifo data frames.
 *
 * @client_data : Instance of client data
 *
 * Return: Nothing
 */
static void fifo_dataframe_display(struct bmi08g_client_data *client_data)
{
	struct bmi08_fifo_frame fifo_frame = {0};
	struct bmi08_sensor_data bmi08_gyro[BMI08_GYR_DATA_LENGHT] = { {0} };
	struct bmi08_gyr_fifo_config gyr_conf = {0};
	int rslt;
	u8 fifo_data[BMI08_GYR_FIFO_RAW_DATA_BUFFER_SIZE] = {0};
	u16 idx = 0;
	u16 gyro_length;

	fifo_frame.data = fifo_data;
	fifo_frame.length = BMI08_GYR_FIFO_RAW_DATA_USER_LENGTH;
	gyro_length = BMI08_GYR_FIFO_EXTRACTED_DATA_FRAME_COUNT;
	rslt = bmi08g_get_fifo_config(&gyr_conf, &client_data->device);
	check_error("bmi08g_get_fifo_config", rslt);
	rslt = bmi08g_get_fifo_length(&gyr_conf, &fifo_frame);
	check_error("bmi08g_get_fifo_length", rslt);
	rslt = bmi08g_read_fifo_data(&fifo_frame, &client_data->device);
	check_error("bmi08g_read_fifo_data", rslt);

	PINFO("FIFO length available : %d\n\n", fifo_frame.length);
	PINFO("Requested data frames before parsing: %d\n", gyro_length);
	bmi08g_extract_gyro(bmi08_gyro, &gyro_length, &gyr_conf, &fifo_frame);
	check_error("bmi08g_extract_gyro", rslt);

	PINFO("Parsed gyroscope frames: %d\n", gyr_conf.frame_count);
	for (idx = 0; idx < gyr_conf.frame_count; idx++) {
		PINFO("GYRO[%d] X : %d\t Y : %d\t Z : %d\n",
			  idx,
			  bmi08_gyro[idx].x,
			  bmi08_gyro[idx].y,
			  bmi08_gyro[idx].z);
	}
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	gyro_odr_settings(g_client_data);
	fifo_dataframe_display(g_client_data);
	return scnprintf(buf, PAGE_SIZE, "gyr fifo data read successfully\n");
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);
	struct bmi08_gyr_fifo_config config;

	rslt = bmi08g_get_fifo_config(&config, &g_client_data->device);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);
	struct bmi08_gyr_fifo_config config;
	unsigned int data[3] = {0};

	rslt = sscanf(buf, "%11d %11d  %11d	%11d", &data[0], &data[1], &data[2],
				  &data[3]);

	if (rslt != 3) {
		PERR("Invalid argument\n"
			 "usage echo tag wm_lvl mode > gyr_fifo_config");
		return -EINVAL;
	}

	config.tag = (u8)data[0];
	config.wm_level = (u8)data[1];
	config.mode = (u8)data[2];

	mutex_lock(&g_client_data->lock);
	rslt = bmi08g_set_fifo_config(&config, &g_client_data->device);
	mutex_unlock(&g_client_data->lock);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	rslt = kstrtoul(buf, 10, &selftest);
	if (rslt)
		return rslt;
	if (selftest == 1) {
		mutex_lock(&g_client_data->lock);
		rslt = bmi08g_perform_selftest(&g_client_data->device);
		if (rslt)
			PINFO("gyro self test failed with rslt code:%d", rslt);
		else
			PINFO("gyro self test success");
		mutex_unlock(&g_client_data->lock);
	} else {
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);

	return scnprintf(buf, 64, "reg=0X%02X, len=%d\n",
					 g_client_data->gyro_reg_sel, g_client_data->gyro_reg_len);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);
	int rslt;

	rslt = sscanf(buf, "%11X %11d",
				  &g_client_data->gyro_reg_sel,
				  &g_client_data->gyro_reg_len);
	if ((rslt != 2) || (g_client_data->gyro_reg_len > 128) ||
					(g_client_data->gyro_reg_sel > 127)) {
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);
	int rslt;
	u8 reg_data[128];
	int i;
	int pos;

	if ((g_client_data->gyro_reg_len > 128) ||
		(g_client_data->gyro_reg_sel > 127)) {
		PERR("Invalid argument");
		return -EINVAL;
	}
	mutex_lock(&g_client_data->lock);
	rslt = bmi08g_get_regs(g_client_data->gyro_reg_sel, reg_data,
						   g_client_data->gyro_reg_len, &g_client_data->device);
	mutex_unlock(&g_client_data->lock);
	check_error("read gyro register data", rslt);
	pos = 0;
	for (i = 0; i < g_client_data->gyro_reg_len; ++i) {
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08g_client_data *g_client_data = iio_priv(input);
	int rslt;
	u8 reg_data[128] = {
		0,
	};
	int i, j, status, digit;

	status = 0;
	mutex_lock(&g_client_data->lock);
	/* Lint -save -e574 */
	for (i = j = 0; i < count && j < g_client_data->gyro_reg_len; ++i) {
		/* Lint -restore */
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' ||
			buf[i] == '\r') {
			status = 0;
			++j;
			continue;
		}
		digit = buf[i] & 0x10 ? (buf[i] & 0xF) : ((buf[i] & 0xF) + 9);
		PDEBUG("digit is %d", digit);
		switch (status) {
		case 2:
			++j; /* Fall thru */
			fallthrough;
		case 0:
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
	if (j > g_client_data->gyro_reg_len)
		j = g_client_data->gyro_reg_len;
	else if (j < g_client_data->gyro_reg_len) {
		PERR("Invalid argument");
		mutex_unlock(&g_client_data->lock);
		return -EINVAL;
	}
	PDEBUG("Reg data read as");
	for (i = 0; i < j; ++i)
		PDEBUG("%d", reg_data[i]);
	rslt = bmi08g_set_regs(g_client_data->gyro_reg_sel, reg_data,
						   g_client_data->gyro_reg_len,
						   &g_client_data->device);
	mutex_unlock(&g_client_data->lock);
	check_error("set gyro register data", rslt);
	return count;
}

static int bmi08x_read_axis_data(struct iio_dev *indio_dev, u8 reg_address,
								 s16 *data)
{
	int ret;
	u8 v_data_u8r[2] = {0, 0};
	struct bmi08g_client_data *g_client_data = iio_priv(indio_dev);

	ret = bmi08g_get_regs(reg_address, v_data_u8r,
						2, &g_client_data->device);
	if (ret < 0)
		return ret;
	*data = (s16)((((s16)((s8)v_data_u8r[1])) << 8) | (v_data_u8r[0]));
	return 0;
}

static int bmi08x_read_raw(struct iio_dev *indio_dev,
						   struct iio_chan_spec const *ch, int *val,
						   int *val2, long mask)
{
	int ret, result;
	s16 tval = 0;

	switch (mask) {
	case 0: {
		result = 0;
		ret = IIO_VAL_INT;
		mutex_lock(&indio_dev->mlock);
		switch (ch->type) {
		case IIO_ANGL_VEL:
			result = bmi08x_read_axis_data(indio_dev,
										   ch->address, &tval);
			*val = tval;
			break;
		default:
			ret = -EINVAL;
			break;
		}
		mutex_unlock(&indio_dev->mlock);
		if (result < 0)
			return result;
		return ret;
	}

	default:
		return -EINVAL;
	}
}
static IIO_DEVICE_ATTR(chip_id, 0444, chip_id_show, NULL, 0);
static IIO_DEVICE_ATTR(driver_version, 0444, driver_version_show, NULL, 0);
static IIO_DEVICE_ATTR(data_sync, 0444, data_sync_show, NULL, 0);
static IIO_DEVICE_ATTR(avail_sensor, 0444, avail_sensor_show, NULL, 0);
static IIO_DEVICE_ATTR(gyr_val, 0444, gyr_val_show, NULL, 0);
static IIO_DEVICE_ATTR(gyr_fifo_dataframe, 0444, gyr_fifo_dataframe_show,
					   NULL, 0);
static IIO_DEVICE_ATTR(config_function, 0644, config_function_show,
					   config_function_store, 0);
static IIO_DEVICE_ATTR(gyr_op_mode, 0644, gyr_op_mode_show,
					   gyr_op_mode_store, 0);
static IIO_DEVICE_ATTR(gyr_config, 0644, gyr_config_show, gyr_config_store, 0);
static IIO_DEVICE_ATTR(gyr_reg_sel, 0644, gyr_reg_sel_show,
					   gyr_reg_sel_store, 0);
static IIO_DEVICE_ATTR(gyr_reg_val, 0644, gyr_reg_val_show,
					   gyr_reg_val_store, 0);
static IIO_DEVICE_ATTR(gyr_fifo_config, 0644, gyr_fifo_config_show,
					   gyr_fifo_config_store, 0);
static IIO_DEVICE_ATTR(gyr_soft_reset, 0200, NULL, gyr_soft_reset_store, 0);
static IIO_DEVICE_ATTR(gyr_selftest, 0200, NULL, gyr_selftest_store, 0);
static IIO_DEVICE_ATTR(iio_generic_buffer, 0200, NULL,
					   iio_generic_buffer_store, 0);

static struct attribute *bmi08_attributes[] = {
	&iio_dev_attr_chip_id.dev_attr.attr,
	&iio_dev_attr_driver_version.dev_attr.attr,
	&iio_dev_attr_data_sync.dev_attr.attr,
	&iio_dev_attr_avail_sensor.dev_attr.attr,
	&iio_dev_attr_config_function.dev_attr.attr,
	&iio_dev_attr_gyr_op_mode.dev_attr.attr,
	&iio_dev_attr_gyr_val.dev_attr.attr,
	&iio_dev_attr_gyr_soft_reset.dev_attr.attr,
	&iio_dev_attr_gyr_config.dev_attr.attr,
	&iio_dev_attr_gyr_selftest.dev_attr.attr,
	&iio_dev_attr_gyr_reg_sel.dev_attr.attr,
	&iio_dev_attr_gyr_reg_val.dev_attr.attr,
	&iio_dev_attr_gyr_fifo_dataframe.dev_attr.attr,
	&iio_dev_attr_gyr_fifo_config.dev_attr.attr,
	&iio_dev_attr_iio_generic_buffer.dev_attr.attr,
	NULL};
static struct attribute_group bmi08_attribute_group = {
	.attrs = bmi08_attributes};
static const struct iio_info bmi08g_acc_iio_info = {
	.attrs = &bmi08_attribute_group,
	/*lint -e546*/
	.read_raw = &bmi08x_read_raw,
	/*lint +e546*/
};
#ifdef BMI08_REGULATOR_ENABLE
static void bmi088_disable_regulator_action(void *_data)
{
	struct bmi08g_client_data *g_client_data = NULL;

	g_client_data->dev = _data;
	PINFO("Disabling regulators");
	if (g_client_data->vddio)
		regulator_disable(g_client_data->vddio);
	if (g_client_data->vdd)
		regulator_disable(g_client_data->vdd);
}

static int bmi088_enable_regulator_action(struct iio_dev *bmi08x_iio_private)
{
	int err;
	struct bmi08g_client_data *g_client_data;

	g_client_data = iio_priv(bmi08x_iio_private);
	g_client_data->vdd = devm_regulator_get(&bmi08x_iio_private->dev, "vdd");
	if (IS_ERR(g_client_data->vdd)) {
		PERR("Failed to get vdd regulator\n");
		return PTR_ERR(g_client_data->vdd);
	}

	g_client_data->vddio = devm_regulator_get(&bmi08x_iio_private->dev,
							"vddio");
	if (IS_ERR(g_client_data->vddio)) {
		PERR("Failed to get vddio regulator\n");
		return PTR_ERR(g_client_data->vddio);
	}

	err = regulator_enable(g_client_data->vdd);
	if (err) {
		PERR("Failed to enable vdd regulator\n");
		return err;
	}
	err = regulator_enable(g_client_data->vddio);
	if (err) {
		PERR("Failed to enable vddio regulator\n");
		regulator_disable(g_client_data->vdd);
		return err;
	}
	err = devm_add_action_or_reset(g_client_data->dev,
								   bmi088_disable_regulator_action,
								   g_client_data->dev);
	PINFO("last %d", err);
	if (err) {
		PERR("Failed to setup regulator cleanup action %d\n", err);
		return err;
	}
	return 0;
}
#endif
/**
 * bmi08_probe - Does Sensor initialization
 * @dev: Device instance
 * @client_data : Instance of client data.
 */
int bmi08g_probe(struct iio_dev *bmi08x_iio_private)
{
	int rslt = 0;
	struct bmi08g_client_data *g_client_data;

	PINFO("BMI088 Gyro probe");
	g_client_data = iio_priv(bmi08x_iio_private);
	if (g_client_data == NULL) {
		PERR("client_data NULL\n");
		return -EINVAL;
	}
#ifdef BMI08_REGULATOR_ENABLE
	rslt = bmi088_enable_regulator_action(bmi08x_iio_private);
	if (rslt) {
		PERR("bmi088_enable_regulator_action NULL\n");
		return -EINVAL;
	}
#endif
	g_client_data->device.delay_us = bmi08_i2c_delay_us;
	bmi08x_iio_private->channels = bmi08g_iio_channels;
	bmi08x_iio_private->num_channels = ARRAY_SIZE(bmi08g_iio_channels);
	bmi08x_iio_private->info = &bmi08g_acc_iio_info;
	bmi08x_iio_private->modes = INDIO_DIRECT_MODE;
	/*lint -e86*/
	mutex_init(&g_client_data->lock);
	/*lint +e86*/
	rslt = bmi08x_iio_configure_buffer(bmi08x_iio_private);
	if (rslt) {
		PERR("bmi08x iio_device_register %d", rslt);
		goto exit_err_clean;
	}
	rslt = iio_device_register(bmi08x_iio_private);
	if (rslt) {
		PERR("bmi08x iio_device_register %d", rslt);
		goto exit_err_clean;
	}
	rslt = gyro_sensor_init(g_client_data);
	if (rslt) {
		PERR("Sensor initilization failed %d", rslt);
		goto exit_err_clean;
	}
	if (rslt) {
		PINFO("Sensor successfully initilized\n");
		PINFO("Gyro chip ID : 0x%x\n", g_client_data->device.gyro_chip_id);
		g_client_data->sensor_init = 1;
	}

	rslt = gyr_request_irq(g_client_data);
	if (rslt < 0) {
		PERR("GYR Request irq failed");
		goto exit_err_clean;
	}
	PINFO("GYR IRQ requested");
	PINFO("sensor %s probed successfully", SENSOR_NAME);

	return 0;

exit_err_clean:
	bmi08x_iio_unconfigure_buffer(bmi08x_iio_private);
	if (bmi08x_iio_private)
			iio_device_unregister(bmi08x_iio_private);
	PERR("bmi08x : error occured in probe\n");
	return rslt;
}
/* Lint -save -e19 */
EXPORT_SYMBOL(bmi08g_probe);
/* Lint -restore */

/** * bmi08_remove - This function removes the driver from the device.
 * @dev : Instance of the device.
 *
 * Return : Status of the suspend function.
 * * 0 - OK.
 * * Negative value : Error.
 */
int bmi08g_remove(struct iio_dev *bmi08x_iio_private)
{
	int rslt = 0;
	struct bmi08g_client_data *g_client_data;

	g_client_data = iio_priv(bmi08x_iio_private);
	if (g_client_data != NULL) {
		bmi08_i2c_delay_us(MS_TO_US(300),
						   &g_client_data->device.intf_ptr_gyro);
#ifdef BMI08_REGULATOR_ENABLE
		if (g_client_data->vdd) {
			regulator_disable(g_client_data->vdd);
			regulator_put(g_client_data->vdd);
			g_client_data->vdd = NULL;
		}
		if (g_client_data->vddio) {
			regulator_disable(g_client_data->vddio);
			regulator_put(g_client_data->vddio);
			g_client_data->vddio = NULL;
		}
#endif
		bmi08x_iio_unconfigure_buffer(bmi08x_iio_private);
		if (bmi08x_iio_private)
			iio_device_unregister(bmi08x_iio_private);
		(void)cancel_work_sync(&g_client_data->gyr_irq_work);
		(void)free_irq(g_client_data->GYR_IRQ, g_client_data);

	}

	return rslt;
}

/* Lint -save -e19 */
EXPORT_SYMBOL(bmi08g_remove);
/* Lint -restore */
