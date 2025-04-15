/**
 * @section LICENSE
 * Copyright (c) 2017-2024 Bosch Sensortec GmbH All Rights Reserved.
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @file		bmi08a_driver.h
 * @date		2024-10-10
 * @version		v1.4.1
 *
 * @brief		 bmi08x Linux Driver
 *
 */

#ifndef BMI08A_DRIVER_H
#define BMI08A_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************/
/* System header files */
/*********************************************************************/
#include <linux/types.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/workqueue.h>
#include <linux/gpio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/events.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/iio/sw_device.h>
#include <linux/export.h>
#include <linux/bitmap.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/syscalls.h>
#include <linux/iio/sw_trigger.h>
#include <linux/regulator/consumer.h>

#include "bmi08x.h"


/*********************************************************************/
/* Macro definitions */
/*********************************************************************/
/** Name of the device driver and accel input device*/
#define SENSOR_NAME	  "bmi08a"

#define REL_HW_STATUS				(2)
#define REL_FEAT_STATUS				(1)
#define REL_GYR_FEAT_STATUS			(3)
/* default sampling frequency - 100Hz */
#define HRTIMER_DEFAULT_SAMPLING_FREQUENCY 100

enum bmi088a_config_func {
	BMI088_ACC_DATA_READY,
	BMI088_ACC_FIFO_WM,
	BMI088_ACC_FIFO_FULL,
};
struct iio_hrtimer_info {
	struct iio_sw_trigger swt;
	struct hrtimer timer;
	unsigned long sampling_frequency;
	ktime_t period;
};
/**
 *	struct bmi08a_client_data - Client structure which holds sensor-specific
 *	information.
 */
struct bmi08a_client_data {
	struct bmi08_dev device;
	struct device *dev;
	struct iio_trigger *bmi_input;
	struct iio_trigger *feat_input;
	struct iio_trigger *gyr_feat_input;
	struct regulator *vdd;
	struct regulator *vddio;
	struct regmap *regmap;
	int IRQ;
	struct mutex lock;
	struct work_struct irq_work;
	struct iio_hrtimer_info *trig_info;
	u16 fw_version;
	int acc_reg_sel;
	int acc_reg_len;
	atomic_t in_suspend;
	u8 sensor_init;
	u8 acc_drdy_en;
	u8 acc_fifo_wm_en;
	u8 acc_fifo_full_en;
	u16 acc_fifo_wm;
	u8 data_sync_en;
	u8 int_status;
	u8 acc_int_status;
};

/*********************************************************************/
/* Function prototype declarations */
/*********************************************************************/
/*extern the iio_dev of three devices*/
extern struct iio_dev *data_iio_private;
/**
 * bmi08x_iio_configure_buffer() - register buffer resources
 * @indo_dev: device instance state
 */
int bmi08x_iio_configure_buffer(struct iio_dev *indio_dev);
/**
 * bmi08x_iio_unconfigure_buffer() - release buffer resources
 * @indo_dev: device instance state
 */
void bmi08x_iio_unconfigure_buffer(struct iio_dev *indio_dev);
/**
 * bmi08x_iio_allocate_trigger() - register trigger resources
 * @indo_dev: device instance state
 */
int bmi08x_iio_allocate_trigger(struct iio_dev *indio_dev, u8 type);
/**
 * bmi08x_iio_deallocate_trigger() - release trigger resources
 * @indo_dev: device instance state
 */
void bmi08x_iio_deallocate_trigger(struct iio_dev *indio_dev);
/**
 * bmi08a_probe - This is the probe function for bmi08x sensor.
 * Called from the I2C driver probe function to initialize the accel sensor
 *
 * @client_data : Structure instance of client data.
 * @dev : Structure instance of device.
 *
 * Return : Result of execution status
 * * 0 - Success
 * * negative value -> Error
 */
int bmi08a_probe(struct iio_dev *data_iio_private);

/**
 * bmi08a_remove - This function removes the driver from the device.
 *
 * @dev : Structure instance of device.
 *
 * Return : Result of execution status
 * * 0 - Success
 * * negative value -> Error
 */
int bmi08a_remove(struct iio_dev *data_iio_private);

#ifdef __cplusplus
}
#endif

#endif /* BMI08A_DRIVER_H_	*/
