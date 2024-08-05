/**
 * @section LICENSE
 * Copyright (c) 2019~2020 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi08x_driver.h
 * @date	 2022/06/22
 * @version	 2.0.0
 *
 * @brief	 bmi08x Linux Driver
 */

#ifndef BMI08X_DRIVER_H
#define BMI08X_DRIVER_H

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

#include "bmi08x.h"

/*********************************************************************/
/* Macro definitions */
/*********************************************************************/
/** Name of the device driver and accel input device*/
#define SENSOR_NAME	  "bmi08x"
#define SENSOR_NAME_FEAT "bmi08x_acc_feat"
#define SENSOR_NAME_GYR_FEAT "bmi08x_gyr_feat"

#define REL_HW_STATUS				(2)
#define REL_FEAT_STATUS				(1)
#define REL_GYR_FEAT_STATUS			(3)


enum bmi088_config_func {
	BMI088_ACC_DATA_READY,
	BMI088_ACC_FIFO_WM,
	BMI088_ACC_FIFO_FULL,
	BMI088_GYRO_DATA_READY,
	BMI088_GYRO_FIFO_WM,
	BMI088_GYRO_FIFO_FULL,
};

/**
 *	struct bmi08x_client_data - Client structure which holds sensor-specific
 *	information.
 */
struct bmi08x_client_data {
	struct bmi08x_dev device;
	struct device *dev;
	struct input_dev *bmi_input;
	struct input_dev *feat_input;
	struct input_dev *gyr_feat_input;
	struct tasklet_struct irq_tasklet;
	u64 gyr_int_time;
	u64 gyr_int_count;
	int IRQ;
	int GYR_IRQ;
	struct mutex lock;
	u8 gpio_pin;
	struct work_struct irq_work;
	struct work_struct gyr_irq_work;
	u16 fw_version;
	int acc_reg_sel;
	int acc_reg_len;
	int gyro_reg_sel;
	int gyro_reg_len;
	atomic_t in_suspend;
	u8 sensor_init;
	u8 acc_drdy_en;
	u8 acc_fifo_wm_en;
	u8 acc_fifo_full_en;
	u16 acc_fifo_wm;
	u8 gyro_drdy_en;
	u8 gyro_fifo_wm_en;
	u8 gyro_fifo_full_en;
	u8 data_sync_en;
	u8 int_status;
	u8 acc_int_status;
};

/*********************************************************************/
/* Function prototype declarations */
/*********************************************************************/
/**
 * bmi08xa_probe - This is the probe function for bmi08x sensor.
 * Called from the I2C driver probe function to initialize the accel sensor
 *
 * @client_data : Structure instance of client data.
 * @dev : Structure instance of device.
 *
 * Return : Result of execution status
 * * 0 - Success
 * * negative value -> Error
 */
int bmi08x_probe(struct bmi08x_client_data *client_data, struct device *dev);

/**
 * bmi08x_remove - This function removes the driver from the device.
 *
 * @dev : Structure instance of device.
 *
 * Return : Result of execution status
 * * 0 - Success
 * * negative value -> Error
 */
int bmi08x_remove(struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* BMI08X_DRIVER_H_	*/
