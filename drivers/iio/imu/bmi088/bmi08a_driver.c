/**
 * @section LICENSE
 * Copyright (c) 2017-2024 Bosch Sensortec GmbH All Rights Reserved.
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @file		bmi08a_driver.c
 * @date		2024-10-10
 * @version		v1.4.1
 *
 * @brief		 BMI08A Linux Driver
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
#include "bmi08a_driver.h"
#include "bs_log.h"

/*********************************************************************/
/* Local macro definitions */
/*********************************************************************/
#define DRIVER_VERSION "1.4.1"
#define MS_TO_US(msec) UINT32_C((msec) * 1000)

/* Buffer size allocated to store raw FIFO data for accel */
#define BMI08_ACC_FIFO_RAW_DATA_BUFFER_SIZE 1024

/* Length of data to be read from FIFO for accel */
#define BMI08_ACC_FIFO_RAW_DATA_USER_LENGTH 1024

/* accel data lenght */
#define BMI08_ACC_DATA_LENGHT 8

/* Number of Accel frames to be extracted from FIFO */
#define BMI08_ACC_FIFO_WM_EXTRACTED_DATA_FRAME_COUNT 100
/* varaiable for fifo data frame */
u8 *fifo_data;
struct bmi08_sensor_data *bmi08_accel;
/*********************************************************************/
/* Global data */
/*********************************************************************/

/**
 * fifo_dataframe_display - sysfs callback which reads
 * acc sensor fifo data frames.
 *
 * @client_data : Instance of client data
 *
 * Return: Nothing
 */
static void fifo_dataframe_display(struct bmi08a_client_data *acc_client_data);
/**
 * bmi_fifo_init - Initialize memory for fifp data
 *
 */
static void bmi_fifo_init(void)
{
	fifo_data =
			kzalloc((sizeof(u8) *
			(BMI08_ACC_FIFO_RAW_DATA_BUFFER_SIZE * 2)),
			GFP_KERNEL);
	bmi08_accel =
			kzalloc((sizeof(u8) *
			(BMI08_ACC_DATA_LENGHT)),
			GFP_KERNEL);
}

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
static const struct iio_event_spec bmi08a_event = {
	.type = IIO_EV_TYPE_THRESH,
	.dir = IIO_EV_DIR_RISING,
	.mask_shared_by_type = BIT(IIO_EV_INFO_VALUE) |
						   BIT(IIO_EV_INFO_ENABLE)};

#define BMI08A_CHANNELS_CONFIG(device_type, si, mod, addr) \
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
		.event_spec = &bmi08a_event,                       \
		.num_event_specs = 1}

#define BMI08A_BYTE_FOR_PER_AXIS_CHANNEL 2

/* scan element definition */
enum BMI08A_AXIS_SCAN {
	BMI08A_SCAN_ACCL_X,
	BMI08A_SCAN_ACCL_Y,
	BMI08A_SCAN_ACCL_Z,
	BMI08A_SCAN_TIMESTAMP,
};

/*iio chan spec for  BMI08X sensor*/
static const struct iio_chan_spec bmi08a_iio_channels[] = {
	/*acc channel*/
	/*lint -e446*/
	BMI08A_CHANNELS_CONFIG(IIO_ACCEL, BMI08A_SCAN_ACCL_X,
						   IIO_MOD_X, BMI08_REG_ACCEL_X_LSB),
	BMI08A_CHANNELS_CONFIG(IIO_ACCEL, BMI08A_SCAN_ACCL_Y,
						   IIO_MOD_Y, BMI08_REG_ACCEL_Y_LSB),
	BMI08A_CHANNELS_CONFIG(IIO_ACCEL, BMI08A_SCAN_ACCL_Z,
						   IIO_MOD_Z, BMI08_REG_ACCEL_Z_LSB),

	/*lint +e446*/
	/*ap timestamp channel*/
	IIO_CHAN_SOFT_TIMESTAMP(BMI08A_SCAN_TIMESTAMP)

};
/*!
 *  @brief This API is used to enable bmi08 interrupt
 *
 *  @client_data : Instance of client data
 *
 *  @return void
 *
 */
static void acc_enable_bmi08_interrupt(struct bmi08a_client_data *client_data)
{
	s8 rslt;
	struct bmi08_accel_int_channel_cfg accel_int_config;

	/* Set accel interrupt pin configuration */
	accel_int_config.int_channel = BMI08_INT_CHANNEL_1;
	accel_int_config.int_type = BMI08_ACCEL_INT_FIFO_WM;
	accel_int_config.int_pin_cfg.output_mode =
		BMI08_INT_MODE_PUSH_PULL;
	accel_int_config.int_pin_cfg.lvl = BMI08_INT_ACTIVE_HIGH;
	accel_int_config.int_pin_cfg.enable_int_pin = BMI08_ENABLE;

	/* Enable accel data ready interrupt channel */
	rslt = bmi08a_set_int_config(
		(const struct bmi08_accel_int_channel_cfg *)&accel_int_config,
		&client_data->device);
	check_error("bmi08a_set_int_config", rslt);
}

/*!
 *  @brief This API is used to disable bmi08 interrupt
 *
 *  @client_data : Instance of client data
 *
 *  @return void
 *
 */
static void acc_disable_bmi08_interrupt(struct bmi08a_client_data *client_data)
{
	s8 rslt;
	struct bmi08_accel_int_channel_cfg accel_int_config;

	/* Set accel interrupt pin configuration */
	accel_int_config.int_channel = BMI08_INT_CHANNEL_1;
	accel_int_config.int_type = BMI08_ACCEL_INT_FIFO_WM;
	accel_int_config.int_pin_cfg.output_mode =
		BMI08_INT_MODE_PUSH_PULL;
	accel_int_config.int_pin_cfg.lvl = BMI08_INT_ACTIVE_HIGH;
	accel_int_config.int_pin_cfg.enable_int_pin = BMI08_DISABLE;

	/* Disable accel data ready interrupt channel */
	rslt = bmi08a_set_int_config(
		(const struct bmi08_accel_int_channel_cfg *)&accel_int_config,
		&client_data->device);
	check_error("bmi08a_set_int_config", rslt);
}
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
		bmi08x_iio_allocate_trigger(input, 0);
	/*lint +e534*/
	else
		bmi08x_iio_deallocate_trigger(input);

	return count;
}

/* enables or disables selected feature index from config function store*/
static int acc_feature_config_set(struct bmi08a_client_data *client_data,
							  int config_func, int enable)
{
	int rslt;
	struct bmi08_accel_int_channel_cfg accel_int_config;

	PDEBUG("config_func = %d, enable=%d", config_func, enable);
	if (config_func < 0 || config_func > BMI08_ACCEL_INT_FIFO_FULL)
		return -EINVAL;

	switch (config_func) {
	case BMI088_ACC_DATA_READY:
		accel_int_config.int_type = BMI08_ACCEL_INT_DATA_RDY;
		client_data->acc_drdy_en = enable;
		break;
	case BMI088_ACC_FIFO_WM:
		accel_int_config.int_type = BMI08_ACCEL_INT_FIFO_WM;
		client_data->acc_fifo_wm_en = enable;
		break;
	case BMI088_ACC_FIFO_FULL:
		accel_int_config.int_type = BMI08_ACCEL_INT_FIFO_FULL;
		client_data->acc_fifo_full_en = enable;
		break;
	default:
		PERR("Invalid feature selection: %d", config_func);
		return -EINVAL;
	}

	accel_int_config.int_pin_cfg.output_mode =
		BMI08_INT_MODE_PUSH_PULL;
	accel_int_config.int_pin_cfg.lvl = BMI08_INT_ACTIVE_HIGH;
	accel_int_config.int_pin_cfg.enable_int_pin = enable;
	accel_int_config.int_channel = BMI08_INT_CHANNEL_1;
	rslt = bmi08a_set_int_config(
		(const struct bmi08_accel_int_channel_cfg *)&accel_int_config,
		&client_data->device);

	check_error("en/disabling interrupt", rslt);
	return rslt;
}

/**
 *	bmi08_irq_work_func - Bottom half handler for feature interrupts.
 *	@work : Work data for the workqueue handler.
 */
static void bmi08_irq_work_func(struct work_struct *work)
{
	/*lint -e26 -e10 -e124 -e40*/
	struct bmi08a_client_data *acc_client_data = container_of(work,
						struct bmi08a_client_data, irq_work);
	/*lint +e26  +e10 +e124 +e40*/
	int rslt = 0;
	struct bmi08_sensor_data accel_data;

	mutex_lock(&acc_client_data->lock);
	rslt = bmi08a_get_data_int_status(&acc_client_data->acc_int_status,
									  &acc_client_data->device);
	mutex_unlock(&acc_client_data->lock);
	if (acc_client_data->acc_int_status != 0) {
		if ((acc_client_data->acc_int_status & BMI08_ACCEL_DATA_READY_INT) &&
			(acc_client_data->data_sync_en != 1)) {
			PINFO("ACC DRDY INT ocurred\n");
			mutex_lock(&acc_client_data->lock);
			rslt = bmi08a_get_data(&accel_data, &acc_client_data->device);
			mutex_unlock(&acc_client_data->lock);
			check_error("get acc data", rslt);
			PINFO("ACC X:%d Y:%d Z:%d\n",
				  accel_data.x, accel_data.y, accel_data.z);
			rslt = acc_feature_config_set(acc_client_data,
					BMI088_ACC_DATA_READY, BMI08_DISABLE);
		}
		if ((acc_client_data->acc_int_status & BMI08_ACCEL_FIFO_FULL_INT) &&
			(acc_client_data->acc_fifo_full_en == BMI08_ENABLE)) {
			PINFO("ACC FIFO FULL INT occurred\n");
			fifo_dataframe_display(acc_client_data);
		}
		if ((acc_client_data->acc_int_status & BMI08_ACCEL_FIFO_WM_INT) &&
			(acc_client_data->acc_fifo_wm_en == BMI08_ENABLE)) {
			PINFO("ACC FIFO WM INT occurred\n");
			acc_disable_bmi08_interrupt(acc_client_data);
			PINFO("disable interrupt");
			fifo_dataframe_display(acc_client_data);
			PINFO("enable interrupt");
			acc_enable_bmi08_interrupt(acc_client_data);
		}
		check_error("acc INT handle", rslt);
	}
}
/**
 * bmi08_irq_handle - IRQ handler function.
 * @irq : Number of irq line.
 * @handle : Instance of client data.
 *
 * Return : Status of IRQ function.
 */
static irqreturn_t bmi08_irq_handle(int irq, void *handle)
{
	struct bmi08a_client_data *acc_client_data = handle;

	if (schedule_work(&acc_client_data->irq_work))
		return IRQ_HANDLED;
	return IRQ_HANDLED;
}


/**
 * acc_bmi08_request_irq - Allocates interrupt resources and enables the
 * interrupt line and IRQ handling.
 *
 * @client_data: Instance of the client data.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * Any Negative value - Error.
 */
static int acc_bmi08_request_irq(struct bmi08a_client_data *acc_client_data)
{
	int rslt = 0;

	rslt = request_irq(acc_client_data->IRQ, bmi08_irq_handle,
					   IRQF_TRIGGER_RISING,
					   SENSOR_NAME, acc_client_data);
	if (rslt < 0) {
		PERR("request_irq failed with rslt:%d", rslt);
		return -EIO;
	}
	PINFO("ACC IRQ requested for pin : %d\n", acc_client_data->IRQ);
	INIT_WORK(&acc_client_data->irq_work, bmi08_irq_work_func);
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
	u8 rslt;
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);

	rslt = bmi08a_get_set_regs(BMI08_REG_ACCEL_CHIP_ID, &acc_chip_id,
							   1, &acc_client_data->device, GET_FUNC);
	check_error("get acc chip id", rslt);

	return scnprintf(buf, PAGE_SIZE, "chip_id acc:0x%x\n",
					 acc_chip_id);
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
	return scnprintf(buf, 32, "bmi08a\n");
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);
	s32 sensor_temp;

	rslt = bmi08a_get_sensor_temperature(&acc_client_data->device,
						&sensor_temp);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);

	rslt = kstrtoul(buf, 10, &selftest);
	check_error("acc_selftest receive input", rslt);

	if (selftest == 1) {
		mutex_lock(&acc_client_data->lock);
		/* Perform accelerometer self-test */
		rslt = bmi08xa_perform_selftest(&acc_client_data->device);
		if (rslt)
			PINFO("accel self test failed with return code:%d", rslt);
		else
			PINFO("accel self test success");
		mutex_unlock(&acc_client_data->lock);
	} else {
		PERR("Invalid input use: echo 1 > acc_selftest");
		rslt = -EINVAL;
		return rslt;
	}

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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);

	rslt = bmi08a_get_power_mode(&acc_client_data->device);
	check_error("acc get power mode", rslt);
	if (acc_client_data->device.accel_cfg.power == BMI08_ACCEL_PM_ACTIVE)
		return scnprintf(buf, PAGE_SIZE,
						 "accel_op_mode : BMI08_ACCEL_PM_ACTIVE\n");
	else if (acc_client_data->device.accel_cfg.power == BMI08_ACCEL_PM_SUSPEND)
		return scnprintf(buf, PAGE_SIZE,
						 "acc_op_mode : BMI08_ACCEL_PM_SUSPEND\n");
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
								 struct device_attribute *attr,
								 const char *buf, size_t count)
{
	int rslt;
	unsigned long op_mode;

	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);

	rslt = kstrtoul(buf, 10, &op_mode);
	check_error("acc_op_mode receive input", rslt);

	if (op_mode == 1)
		acc_client_data->device.accel_cfg.power = BMI08_ACCEL_PM_ACTIVE;
	else if (op_mode == 0)
		acc_client_data->device.accel_cfg.power = BMI08_ACCEL_PM_SUSPEND;
	else {
		PERR("pwr mode Invalid input:\n0->suspend 1->active");
		return -EINVAL;
	}
	mutex_lock(&acc_client_data->lock);
	rslt = bmi08a_set_power_mode(&acc_client_data->device);
	mutex_unlock(&acc_client_data->lock);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);
	struct bmi08_sensor_data accel_data;

	mutex_lock(&acc_client_data->lock);
	rslt = bmi08a_get_data(&accel_data, &acc_client_data->device);
	mutex_unlock(&acc_client_data->lock);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);

	mutex_lock(&acc_client_data->lock);
	rslt = bmi08a_get_meas_conf(&acc_client_data->device);
	check_error("get acc meas conf", rslt);
	mutex_unlock(&acc_client_data->lock);
	return scnprintf(buf, PAGE_SIZE, "acc_conf odr:%d bw:%d range:%d\n",
					 acc_client_data->device.accel_cfg.odr,
					 acc_client_data->device.accel_cfg.bw,
					 acc_client_data->device.accel_cfg.range);
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
								struct device_attribute *attr,
								const char *buf, size_t count)
{
	int rslt;
	unsigned int data[3] = {0};
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);

	rslt = sscanf(buf, "%11d %11d %11d ", &data[0], &data[1], &data[2]);
	if (rslt != 3) {
		PINFO("Invalid input\nuse echo odr bandwidth range > acc_conf");
		return -EIO;
	}
	mutex_lock(&acc_client_data->lock);
	rslt = bmi08a_get_meas_conf(&acc_client_data->device);
	check_error("get acc meas conf", rslt);
	acc_client_data->device.accel_cfg.odr = (u8)data[0];
	acc_client_data->device.accel_cfg.bw = (u8)data[1];
	acc_client_data->device.accel_cfg.range = (u8)data[2];
	rslt = bmi08xa_set_meas_conf(&acc_client_data->device);
	check_error("set acc meas conf", rslt);
	mutex_unlock(&acc_client_data->lock);

	return count;
}
/**
 * accel_sensor_init - sesnor initialization.
 *
 * @dev: Device instance.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static int accel_sensor_init(struct bmi08a_client_data *client_data)
{
	int rslt = 0;

	rslt = bmi08xa_init(&client_data->device);
	if (rslt) {
		PERR("accel sensor init failed wtih rslt:%d", rslt);
		return -EIO;
	}

	rslt = bmi08a_soft_reset(&client_data->device);
	if (rslt) {
		PERR("sensor soft reset failed wtih rslt:%d", rslt);
		return -EIO;
	}

	bmi08_i2c_delay_us(MS_TO_US(300), &client_data->device.intf_ptr_accel);
	rslt = bmi08a_load_config_file(&client_data->device);
	if (rslt) {
		PERR("load config failed wtih rslt:%d", rslt);
		return -EIO;
	}
	PINFO("config stream loaded successfully\n");

	bmi08_i2c_delay_us(MS_TO_US(10), &client_data->device.intf_ptr_accel);

	client_data->device.accel_cfg.power = BMI08_ACCEL_PM_ACTIVE;
	rslt |= bmi08a_set_power_mode(&client_data->device);
	if (rslt < 0) {
		PINFO("ACCEL POWER MODE SET FIALED");
		return -EIO;
	}
	PINFO("Accel power mode set to NORMAL");
	client_data->data_sync_en = 0;
	return rslt;
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);

	/* Base of decimal number system is 10 */
	rslt = kstrtoul(buf, 10, &soft_reset);
	check_error("acc soft reset receive input", rslt);

	if (soft_reset) {
		rslt = bmi08a_soft_reset(&acc_client_data->device);
		check_error("acc soft reset", rslt);
	} else {
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);
	u32 sensor_time;

	rslt = bmi08a_get_sensor_time(&acc_client_data->device, &sensor_time);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);
	struct bmi08_accel_fifo_config config;
	u16 acc_fifo_wm;

	rslt = bmi08a_get_set_fifo_config(&config, &acc_client_data->device,
						GET_FUNC);
	check_error("get fifo config", rslt);
	rslt = bmi08a_get_set_fifo_wm(&acc_fifo_wm, &acc_client_data->device,
						GET_FUNC);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);
	struct bmi08_accel_fifo_config config;
	unsigned int data[4] = {0};
	u16 fifo_wm;

	rslt = sscanf(buf, "%11d %11d %11d", &data[0], &data[1], &data[2]);
	if (rslt != 3) {
		PERR("Invalid argument\n"
			 "usage echo accel_en mode wm > acc_fifo_config");
		return -EINVAL;
	}

	rslt = bmi08a_get_set_fifo_config(&config, &acc_client_data->device,
									GET_FUNC);
	check_error("get fifo config", rslt);
	config.accel_en = (u8)data[0];
	config.mode = (u8)data[1];
	fifo_wm = (u16)data[2];

	mutex_lock(&acc_client_data->lock);
	rslt = bmi08a_get_set_fifo_config(&config, &acc_client_data->device,
							SET_FUNC);
	check_error("set fifo config", rslt);
	rslt = bmi08a_get_set_fifo_wm(&fifo_wm, &acc_client_data->device, SET_FUNC);
	check_error("set fifo wm", rslt);
	mutex_unlock(&acc_client_data->lock);
	return count;
}
/**
 * fifo_dataframe_display - sysfs callback which reads
 * sensor fifo data frames.
 *
 * @dev: Device instance
 *
 * Return: None
 */
static void fifo_dataframe_display(struct bmi08a_client_data *client_data)
{
	s8 rslt;
	u16 wml = 0;
	struct bmi08_fifo_frame fifo_frame = {0};
	u16 accel_length = BMI08_ACC_FIFO_WM_EXTRACTED_DATA_FRAME_COUNT;
	u16 idx = 0;
	u16 fifo_length;

	/* Update FIFO structure */
	fifo_frame.data = fifo_data;
	fifo_frame.length = BMI08_ACC_FIFO_RAW_DATA_USER_LENGTH;
	accel_length = BMI08_ACC_FIFO_WM_EXTRACTED_DATA_FRAME_COUNT;
	rslt = bmi08a_get_fifo_length(&fifo_length, &client_data->device);
	check_error("bmi08a_get_fifo_length", rslt);
	if (client_data->acc_fifo_wm_en == BMI08_ENABLE) {
		rslt = bmi08a_get_set_fifo_wm(&wml, &client_data->device, GET_FUNC);
		check_error("bmi08a_get_set_fifo_wm", rslt);
		PINFO("Watermark level : %d\n", wml);
	}

	PINFO("FIFO buffer size : %d\n", fifo_frame.length);
	PINFO("FIFO length available : %d\n", fifo_length);

	PINFO("Requested data frames before parsing: %d\n", accel_length);
	if (rslt == BMI08_OK) {
		/* Read FIFO data */
		rslt = bmi08a_read_fifo_data(&fifo_frame, &client_data->device);
		check_error("bmi08a_read_fifo_data", rslt);

		(void)bmi08a_extract_accel(bmi08_accel, &accel_length,
								   &fifo_frame, &client_data->device);

		PINFO("Parsed accelerometer frames: %d\n", accel_length);
		for (idx = 0; idx < accel_length; idx++)
			PINFO("ACCEL[%d] X : %d\t Y : %d\t Z : %d\n",
				  idx,
				  bmi08_accel[idx].x,
				  bmi08_accel[idx].y,
				  bmi08_accel[idx].z);
	}
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);

	fifo_dataframe_display(acc_client_data);
	return scnprintf(buf, PAGE_SIZE, "acc fifo data read successfully\n");
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
	struct bmi08a_client_data *acc_client_data = iio_priv(input);

	return scnprintf(buf, PAGE_SIZE,
					 "acc_data_ready%d=%d\nacc_fifo_wm%d=%d\nacc_fifo_full%d=%d\n",
					 BMI088_ACC_DATA_READY, acc_client_data->acc_drdy_en,
					 BMI088_ACC_FIFO_WM, acc_client_data->acc_fifo_wm_en,
					 BMI088_ACC_FIFO_FULL, acc_client_data->acc_fifo_full_en);
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
	struct bmi08a_client_data *acc_client_data = iio_priv(input);

	rslt = sscanf(buf, "%11d %11d", &config_func, &enable);

	if (rslt != 2) {
		PERR("Invalid argument\n");
		PERR("use: echo feature_idx enable/disable > config_function");
		return -EINVAL;
	}

	rslt = acc_feature_config_set(acc_client_data, config_func, enable);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);

	return scnprintf(buf, 64, "reg=0X%02X, len=%d\n",
					 acc_client_data->acc_reg_sel,
					 acc_client_data->acc_reg_len);
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);
	int rslt;

	rslt = sscanf(buf, "%11X %11d",
				  &acc_client_data->acc_reg_sel,
				  &acc_client_data->acc_reg_len);
	if ((rslt != 2) || (acc_client_data->acc_reg_len > 128)
				|| (acc_client_data->acc_reg_sel > 127)) {
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);
	int rslt;
	u8 reg_data[128];
	int i;
	int pos;

	if ((acc_client_data->acc_reg_len > 128) ||
	(acc_client_data->acc_reg_sel > 127)) {
		PERR("Invalid argument");
		return -EINVAL;
	}
	mutex_lock(&acc_client_data->lock);
	rslt = bmi08a_get_set_regs(acc_client_data->acc_reg_sel, reg_data,
							   acc_client_data->acc_reg_len,
							   &acc_client_data->device, GET_FUNC);
	mutex_unlock(&acc_client_data->lock);
	check_error("get register data", rslt);
	pos = 0;
	for (i = 0; i < acc_client_data->acc_reg_len; ++i) {
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
	// struct iio_dev *input = dev_get_drvdata(dev);
	struct iio_dev *input = dev_to_iio_dev(dev);
	struct bmi08a_client_data *acc_client_data = iio_priv(input);
	int rslt;
	u8 reg_data[128] = {
		0,
	};
	int i, j, status, digit;

	status = 0;
	mutex_lock(&acc_client_data->lock);
	/* Lint -save -e574 */
	for (i = j = 0; i < count && j < acc_client_data->acc_reg_len; ++i) {
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
	if (j > acc_client_data->acc_reg_len)
		j = acc_client_data->acc_reg_len;
	else if (j < acc_client_data->acc_reg_len) {
		PERR("Invalid argument");
		mutex_unlock(&acc_client_data->lock);
		return -EINVAL;
	}
	PDEBUG("Reg data read as");
	for (i = 0; i < j; ++i)
		PDEBUG("%d", reg_data[i]);
	rslt = bmi08a_get_set_regs(acc_client_data->acc_reg_sel, reg_data,
							   acc_client_data->acc_reg_len,
							   &acc_client_data->device,
							   SET_FUNC);
	mutex_unlock(&acc_client_data->lock);
	check_error("set register data", rslt);
	return count;
}

static int bmi08x_read_axis_data(struct iio_dev *indio_dev, u8 reg_address,
								 s16 *data)
{
	int ret;
	u8 v_data_u8r[2] = {0, 0};
	struct bmi08a_client_data *acc_client_data = iio_priv(indio_dev);

	ret = bmi08a_get_set_regs(reg_address, v_data_u8r,
								  2, &acc_client_data->device, GET_FUNC);
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
		case IIO_ACCEL:
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
#if 0
static IIO_DEVICE_ATTR(data_sync, 0444, data_sync_show, NULL, 0);
#endif
static IIO_DEVICE_ATTR(sensor_time, 0444, sensor_time_show, NULL, 0);
static IIO_DEVICE_ATTR(temperature, 0444, temperature_show, NULL, 0);
static IIO_DEVICE_ATTR(avail_sensor, 0444, avail_sensor_show, NULL, 0);
static IIO_DEVICE_ATTR(acc_val, 0444, acc_val_show, NULL, 0);
static IIO_DEVICE_ATTR(acc_fifo_dataframe, 0444, acc_fifo_dataframe_show,
					   NULL, 0);
static IIO_DEVICE_ATTR(acc_op_mode, 0644, acc_op_mode_show,
					   acc_op_mode_store, 0);
static IIO_DEVICE_ATTR(config_function, 0644, config_function_show,
					   config_function_store, 0);
static IIO_DEVICE_ATTR(acc_config, 0644, acc_config_show, acc_config_store, 0);
static IIO_DEVICE_ATTR(acc_reg_sel, 0644, acc_reg_sel_show,
					   acc_reg_sel_store, 0);
static IIO_DEVICE_ATTR(acc_reg_val, 0644, acc_reg_val_show,
					   acc_reg_val_store, 0);
static IIO_DEVICE_ATTR(acc_fifo_config, 0644, acc_fifo_config_show,
					   acc_fifo_config_store, 0);
static IIO_DEVICE_ATTR(acc_soft_reset, 0200, NULL, acc_soft_reset_store, 0);
static IIO_DEVICE_ATTR(acc_selftest, 0200, NULL, acc_selftest_store, 0);
static IIO_DEVICE_ATTR(iio_generic_buffer, 0200, NULL,
					   iio_generic_buffer_store, 0);

static struct attribute *bmi08_attributes[] = {
	&iio_dev_attr_chip_id.dev_attr.attr,
	&iio_dev_attr_driver_version.dev_attr.attr,
#if 0
	&iio_dev_attr_data_sync.dev_attr.attr,
#endif
	&iio_dev_attr_sensor_time.dev_attr.attr,
	&iio_dev_attr_temperature.dev_attr.attr,
	&iio_dev_attr_avail_sensor.dev_attr.attr,
	&iio_dev_attr_config_function.dev_attr.attr,

	&iio_dev_attr_acc_op_mode.dev_attr.attr,
	&iio_dev_attr_acc_val.dev_attr.attr,
	&iio_dev_attr_acc_soft_reset.dev_attr.attr,
	&iio_dev_attr_acc_config.dev_attr.attr,
	&iio_dev_attr_acc_selftest.dev_attr.attr,
	&iio_dev_attr_acc_reg_sel.dev_attr.attr,
	&iio_dev_attr_acc_reg_val.dev_attr.attr,
	&iio_dev_attr_acc_fifo_dataframe.dev_attr.attr,
	&iio_dev_attr_acc_fifo_config.dev_attr.attr,

	&iio_dev_attr_iio_generic_buffer.dev_attr.attr,
	NULL};
static struct attribute_group bmi08a_attribute_group = {
	.attrs = bmi08_attributes};
static const struct iio_info bmi08a_acc_iio_info = {
	.attrs = &bmi08a_attribute_group,
	/*lint -e546*/
	.read_raw = &bmi08x_read_raw,
	/*lint +e546*/
};
#ifdef BMI08_REGULATOR_ENABLE
static void bmi088_disable_regulator_action(void *_data)
{
	struct bmi08a_client_data *acc_client_data = NULL;

	acc_client_data->dev = _data;
	PINFO("Disabling regulators");
	if (acc_client_data->vddio)
		regulator_disable(acc_client_data->vddio);
	if (client_data->vdd)
		regulator_disable(acc_client_data->vdd);
}

static int bmi088_enable_regulator_action(struct iio_dev *bmi08x_iio_private)
{
	int err;
	struct bmi08a_client_data *acc_client_data;

	acc_client_data = iio_priv(bmi08x_iio_private);
	acc_client_data->vdd = devm_regulator_get(&bmi08x_iio_private->dev, "vdd");
	if (IS_ERR(acc_client_data->vdd)) {
		PERR("Failed to get vdd regulator\n");
		return PTR_ERR(acc_client_data->vdd);
	}
	acc_client_data->vddio = devm_regulator_get(&bmi08x_iio_private->dev,
							"vddio");
	if (IS_ERR(acc_client_data->vddio)) {
		PERR("Failed to get vddio regulator\n");
		return PTR_ERR(acc_client_data->vddio);
	}
	err = regulator_enable(acc_client_data->vdd);
	if (err) {
		PERR("Failed to enable vdd regulator\n");
		return err;
	}
	err = regulator_enable(acc_client_data->vddio);
	if (err) {
		PERR("Failed to enable vddio regulator\n");
		regulator_disable(acc_client_data->vdd);
		return err;
	}

	err = devm_add_action_or_reset(acc_client_data->dev,
								   bmi088_disable_regulator_action,
								   acc_client_data->dev);
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
int bmi08a_probe(struct iio_dev *bmi08x_iio_private)
{
	int rslt = 0;
	struct bmi08a_client_data *client_data;

	PINFO("BMI088 Accel probe");
	client_data = iio_priv(bmi08x_iio_private);
	if (client_data == NULL) {
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
	client_data->device.delay_us = bmi08_i2c_delay_us;
	bmi08x_iio_private->channels = bmi08a_iio_channels;
	bmi08x_iio_private->num_channels = ARRAY_SIZE(bmi08a_iio_channels);
	bmi08x_iio_private->info = &bmi08a_acc_iio_info;
	bmi08x_iio_private->modes = INDIO_DIRECT_MODE;
	/*lint -e86*/
	mutex_init(&client_data->lock);
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
	rslt = accel_sensor_init(client_data);
	if (rslt) {
		PERR("Sensor initilization failed %d", rslt);
		goto exit_err_clean;
	}
	PINFO("Acc chip ID : 0x%x", client_data->device.accel_chip_id);
	client_data->sensor_init = 1;
	rslt = acc_bmi08_request_irq(client_data);
	if (rslt < 0) {
		PERR("ACC Request irq failed");
		goto exit_err_clean;
	}
	PINFO("ACC IRQ requested");

	PINFO("sensor %s probed successfully", SENSOR_NAME);
	bmi_fifo_init();
	return 0;

exit_err_clean:
	bmi08x_iio_unconfigure_buffer(bmi08x_iio_private);
	if (bmi08x_iio_private)
		iio_device_unregister(bmi08x_iio_private);
	PERR("bmi08x : error occured in probe\n");
	return rslt;
}
/* Lint -save -e19 */
EXPORT_SYMBOL(bmi08a_probe);
/* Lint -restore */

/**
 * bmi08_remove - This function removes the driver from the device.
 * @dev : Instance of the device.
 *
 * Return : Status of the suspend function.
 * * 0 - OK.
 * * Negative value : Error.
 */
int bmi08a_remove(struct iio_dev *bmi08x_iio_private)
{
	int rslt = 0;
	struct bmi08a_client_data *client_data;

	client_data = iio_priv(bmi08x_iio_private);
	if (client_data != NULL) {
		bmi08_i2c_delay_us(MS_TO_US(300),
			&client_data->device.intf_ptr_gyro);
		bmi08x_iio_unconfigure_buffer(bmi08x_iio_private);
		if (bmi08x_iio_private)
			iio_device_unregister(bmi08x_iio_private);
		(void)cancel_work_sync(&client_data->irq_work);
		(void)free_irq(client_data->IRQ, client_data);
		if (fifo_data != NULL)
			kfree(fifo_data);
		if (bmi08_accel != NULL)
			kfree(bmi08_accel);
#ifdef BMI08_REGULATOR_ENABLE
		if (client_data->vdd) {
			regulator_disable(client_data->vdd);
			regulator_put(client_data->vdd);
			client_data->vdd = NULL;
		}
		if (client_data->vddio) {
			regulator_disable(client_data->vddio);
			regulator_put(client_data->vddio);
			client_data->vddio = NULL;
		}
#endif
	}
	return rslt;
}

/* Lint -save -e19 */
EXPORT_SYMBOL(bmi08a_remove);
/* Lint -restore */
