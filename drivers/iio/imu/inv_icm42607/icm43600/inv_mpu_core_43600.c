/*
 * Copyright (C) 2017-2021 InvenSense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "inv_mpu: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/version.h>

#include "../inv_mpu_iio.h"

static const struct inv_hw_s hw_info[INV_NUM_PARTS] = {
	[ICM43600] = {128, "icm43600"},
};

#ifndef SUPPORT_ONLY_BASIC_FEATURES
static char debug_reg_addr = 0x6;
#endif

const char sensor_l_info[][30] = {
	"SENSOR_L_ACCEL",
	"SENSOR_L_GYRO",
	"SENSOR_L_MAG",
	"SENSOR_L_ALS",
	"SENSOR_L_SIXQ",
	"SENSOR_L_THREEQ",
	"SENSOR_L_NINEQ",
	"SENSOR_L_PEDQ",
	"SENSOR_L_GEOMAG",
	"SENSOR_L_PRESSURE",
	"SENSOR_L_GYRO_CAL",
	"SENSOR_L_MAG_CAL",
	"SENSOR_L_EIS_GYRO",
	"SENSOR_L_ACCEL_WAKE",
	"SENSOR_L_GYRO_WAKE",
	"SENSOR_L_MAG_WAKE",
	"SENSOR_L_ALS_WAKE",
	"SENSOR_L_SIXQ_WAKE",
	"SENSOR_L_NINEQ_WAKE",
	"SENSOR_L_PEDQ_WAKE",
	"SENSOR_L_GEOMAG_WAKE",
	"SENSOR_L_PRESSURE_WAKE",
	"SENSOR_L_GYRO_CAL_WAKE",
	"SENSOR_L_MAG_CAL_WAKE",
	"SENSOR_L_NUM_MAX",
};

static int inv_set_accel_bias_reg(struct inv_mpu_state *st,
			int accel_bias, int axis)
{
	int result = 0;
	u8 w;
	int bias;

	/*
	 * accel_bias is 2g scaled by 1<<16
	 * convert to 16g
	 */
	bias = -(accel_bias / 8 / 65536);

	switch (axis) {
	case 0:
		/*
		 * X
		 * REG_GOS_USER4_MREG_TOP1 : AX[11:8] / GZ[11:8]
		 * REG_GOS_USER5_MREG_TOP1 : AX[7:0]
		 */
		result = inv_mreg_read(st, REG_GOS_USER4_MREG_TOP1, 1, &w);
		if (result)
			return result;
		w &= 0x0f;
		w |= (bias >> 4) & 0xf0;
		result = inv_mreg_single_write(st, REG_GOS_USER4_MREG_TOP1, w);
		if (result)
			return result;
		w = bias & 0xff;
		result = inv_mreg_single_write(st, REG_GOS_USER5_MREG_TOP1, w);
		if (result)
			return result;
		break;
	case 1:
		/*
		 * Y
		 * REG_GOS_USER6_MREG_TOP1: AY[7:0]
		 * REG_GOS_USER7_MREG_TOP1: AZ[11:8] / AY[11:8]
		 */
		w = bias & 0xff;
		result = inv_mreg_single_write(st, REG_GOS_USER6_MREG_TOP1, w);
		if (result)
			return result;
		result = inv_mreg_read(st, REG_GOS_USER7_MREG_TOP1, 1, &w);
		if (result)
			return result;
		w &= 0xf0;
		w |= (bias >> 8) & 0x0f;
		result = inv_mreg_single_write(st, REG_GOS_USER7_MREG_TOP1, w);
		if (result)
			return result;
		break;
	case 2:
		/*
		 * Z
		 * REG_GOS_USER7_MREG_TOP1: AZ[11:8] / AY[11:8]
		 * REG_GOS_USER8_MREG_TOP1: AZ[7:0]
		 */
		result = inv_mreg_read(st, REG_GOS_USER7_MREG_TOP1, 1, &w);
		if (result)
			return result;
		w &= 0x0f;
		w |= (bias >> 4) & 0xf0;
		result = inv_mreg_single_write(st, REG_GOS_USER7_MREG_TOP1, w);
		if (result)
			return result;
		w = bias & 0xff;
		result = inv_mreg_single_write(st, REG_GOS_USER8_MREG_TOP1, w);
		if (result)
			return result;
		break;
	default:
		result = -EINVAL;
	}

	return result;
}

static int inv_set_gyro_bias_reg(struct inv_mpu_state *st,
			const int gyro_bias, int axis)
{
	int result = 0;
	u8 w;
	int bias;

	/*
	 * gyro_bias is 2000dps scaled by 1<<16.
	 * convert to 1000dps
	 */
	bias = -(gyro_bias * 2 / 65536);

	switch (axis) {
	case 0:
		/*
		 * X
		 * REG_GOS_USER0_MREG_TOP1: GX[7:0]
		 * REG_GOS_USER1_MREG_TOP1: GY[11:8] / GX[11:8]
		 */
		w = bias & 0xff;
		result = inv_mreg_single_write(st, REG_GOS_USER0_MREG_TOP1, w);
		if (result)
			return result;
		result = inv_mreg_read(st, REG_GOS_USER1_MREG_TOP1, 1, &w);
		if (result)
			return result;
		w &= 0xf0;
		w |= (bias >> 8) & 0x0f;
		result = inv_mreg_single_write(st, REG_GOS_USER1_MREG_TOP1, w);
		if (result)
			return result;
		break;
	case 1:
		/*
		 * Y
		 * REG_GOS_USER1_MREG_TOP1: GY[11:8] / GX[11:8]
		 * REG_GOS_USER2_MREG_TOP1: GY[7:0]
		 */
		result = inv_mreg_read(st, REG_GOS_USER1_MREG_TOP1, 1, &w);
		if (result)
			return result;
		w &= 0x0f;
		w |= (bias >> 4) & 0xf0;
		result = inv_mreg_single_write(st, REG_GOS_USER1_MREG_TOP1, w);
		if (result)
			return result;
		w = bias & 0xff;
		result = inv_mreg_single_write(st, REG_GOS_USER2_MREG_TOP1, w);
		if (result)
			return result;
		break;
	case 2:
		/*
		 * Z
		 * REG_GOS_USER3_MREG_TOP1: GZ[7:0]
		 * REG_GOS_USER4_MREG_TOP1: AX[11:8] / GZ[11:8]
		 */
		w = bias & 0xff;
		result = inv_mreg_single_write(st, REG_GOS_USER3_MREG_TOP1, w);
		if (result)
			return result;
		result = inv_mreg_read(st, REG_GOS_USER4_MREG_TOP1, 1, &w);
		if (result)
			return result;
		w &= 0xf0;
		w |= (bias >> 8) & 0x0f;
		result = inv_mreg_single_write(st, REG_GOS_USER4_MREG_TOP1, w);
		if (result)
			return result;
		break;
	default:
		result = -EINVAL;
	}

	return result;
}

static int _bias_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		goto bias_store_fail;
	switch (this_attr->address) {
	case ATTR_ACCEL_X_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 0);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[0] = data;
		break;
	case ATTR_ACCEL_Y_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 1);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[1] = data;
		break;
	case ATTR_ACCEL_Z_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 2);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[2] = data;
		break;
	case ATTR_GYRO_X_OFFSET:
		result = inv_set_gyro_bias_reg(st, data, 0);
		if (result)
			goto bias_store_fail;
		st->input_gyro_bias[0] = data;
		break;
	case ATTR_GYRO_Y_OFFSET:
		result = inv_set_gyro_bias_reg(st, data, 1);
		if (result)
			goto bias_store_fail;
		st->input_gyro_bias[1] = data;
		break;
	case ATTR_GYRO_Z_OFFSET:
		result = inv_set_gyro_bias_reg(st, data, 2);
		if (result)
			goto bias_store_fail;
		st->input_gyro_bias[2] = data;
		break;
	default:
		break;
	}

bias_store_fail:
	if (result)
		return result;

	return count;
}

static ssize_t inv_bias_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	mutex_lock(&st->lock);
	result = _bias_store(dev, attr, buf, count);
	mutex_unlock(&st->lock);

	return result;
}

#ifndef SUPPORT_ONLY_BASIC_FEATURES
static ssize_t inv_debug_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	switch (this_attr->address) {
	case ATTR_DMP_LP_EN_OFF:
		st->chip_config.lp_en_mode_off = !!data;
		break;
	case ATTR_DMP_CLK_SEL:
		st->chip_config.clk_sel = !!data;
		break;
	case ATTR_DEBUG_REG_ADDR:
		debug_reg_addr = data;
		break;
	case ATTR_DEBUG_REG_WRITE:
		inv_plat_single_write(st, debug_reg_addr, data);
		break;
	}
	return count;
}
#endif

static int _misc_attr_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	switch (this_attr->address) {
	case ATTR_GYRO_SCALE:
		if (data > 3)
			return -EINVAL;
		st->chip_config.fsr = data;
		result = inv_set_gyro_sf(st);
		return result;
	case ATTR_ACCEL_SCALE:
		if (data > 3)
			return -EINVAL;
		st->chip_config.accel_fs = data;
		result = inv_set_accel_sf(st);
		return result;
	case ATTR_DMP_PED_INT_MODE:
		st->ped.int_mode = !!data;
		return 0;
	default:
		return -EINVAL;
	}
	st->trigger_state = MISC_TRIGGER;
	result = set_inv_enable(indio_dev);

	return result;
}

/*
 * inv_misc_attr_store() -  calling this function
 */
static ssize_t inv_misc_attr_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	mutex_lock(&st->lock);
	result = _misc_attr_store(dev, attr, buf, count);
	mutex_unlock(&st->lock);
	if (result)
		return result;

	return count;
}

static ssize_t inv_sensor_rate_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int rate = st->sensor_l[this_attr->address].rate;

	switch (rate) {
	case 12:
		rate = 12500;
		break;
	case 6:
		rate = 6250;
		break;
	case 3:
		rate = 3125;
		break;
	default:
		rate *= 1000;
		break;
	}

	return snprintf(buf, MAX_WR_SZ, "%d.%03d\n", rate / 1000, rate % 1000);
}

static ssize_t inv_sensor_rate_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data, fract, rate, ind;
	int result;

	result = iio_str_to_fixpoint(buf, 1000, &data, &fract);
	if (result)
		return -EINVAL;
	if (data <= 0) {
		pr_err("sensor_rate_store: invalid data=%d\n", data);
		return -EINVAL;
	}
	ind = this_attr->address;
	rate = inv_rate_convert(st, ind, data);
	pr_debug("sensor [%s] requested  rate %d input [%d]\n",
						sensor_l_info[ind], rate, data);
	if (rate == st->sensor_l[ind].rate)
		return count;
	mutex_lock(&st->lock);
	st->sensor_l[ind].rate = rate;
	st->trigger_state = DATA_TRIGGER;
	inv_check_sensor_on(st);
	result = set_inv_enable(indio_dev);
	pr_debug("%s rate %d div %d\n", sensor_l_info[ind],
				st->sensor_l[ind].rate, st->sensor_l[ind].div);
	mutex_unlock(&st->lock);

	return count;
}

static ssize_t inv_sensor_on_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	return snprintf(buf, MAX_WR_SZ, "%d\n",
		st->sensor_l[this_attr->address].on);
}

static ssize_t inv_sensor_on_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data, on, ind;
	int result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	if (data < 0) {
		pr_err("sensor_on_store: invalid data=%d\n", data);
		return -EINVAL;
	}
	ind = this_attr->address;
	on = !!data;

	pr_debug("sensor [%s] requested  %s, input [%d]\n",
			sensor_l_info[ind], (on == 1) ? "On" : "Off", data);

	if (on == st->sensor_l[ind].on) {
		pr_debug("sensor [%s] is already %s, input [%d]\n",
			sensor_l_info[ind], (on == 1) ? "On" : "Off", data);
		return count;
	}

	mutex_lock(&st->lock);
	st->sensor_l[ind].on = on;
	st->trigger_state = RATE_TRIGGER;
	inv_check_sensor_on(st);
	result = set_inv_enable(indio_dev);
	mutex_unlock(&st->lock);
	if (result)
		return result;

	pr_debug("Sensor [%s] is %s by sysfs\n",
				sensor_l_info[ind], (on == 1) ? "On" : "Off");
	return count;
}

static int inv_check_l_step(struct inv_mpu_state *st)
{
	if (st->step_counter_l_on || st->step_counter_wake_l_on)
		st->ped.on = true;
	else
		st->ped.on = false;

	return 0;
}

static int _send_pedo_steps(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;
	int step;
	int update;

	result = inv_get_43600_pedometer_steps(st,
		&step, &update);
	if (result) {
		pr_info("Failed to read step count\n");
		return result;
	}
	inv_send_steps(indio_dev, step, get_time_ns());
	st->prev_steps = step;

	return 0;
}

static int inv_send_pedo_steps(struct iio_dev *indio_dev)
{
	_send_pedo_steps(indio_dev);

	return 0;
}

static int _basic_attr_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data;
	int result;
	u32 power_on_data;
	bool send_ped = false;

	result = kstrtoint(buf, 10, &data);
	if (result || (data < 0))
		return -EINVAL;

	switch (this_attr->address) {
	case ATTR_DMP_PED_ON:
		if ((!!data) == st->ped.on)
			return count;
		st->ped.on = !!data;
		break;
	case ATTR_DMP_SMD_ENABLE:
		if ((!!data) == st->smd.on) {
			pr_debug("SMD is %s\n  Same",
						st->smd.on ? "On" : "Off");
			return count;
		}
		st->smd.on = !!data;
		pr_debug("SMD is %s\n", st->smd.on ? "On" : "Off");
		break;
	case ATTR_DMP_TILT_ENABLE:
		if ((!!data) == st->chip_config.tilt_enable)
			return count;
		st->chip_config.tilt_enable = !!data;
		pr_info("Tile %s\n",
			st->chip_config.tilt_enable ==
			1 ? "Enabled" : "Disabled");
		break;
	case ATTR_DMP_STATIONARY_DETECT_ENABLE:
		if ((!!data) == st->chip_config.stationary_detect_enable)
			return count;
		st->chip_config.stationary_detect_enable = !!data;
		pr_info("Stationary Detect %s\n",
			st->chip_config.stationary_detect_enable ==
			1 ? "Enabled" : "Disabled");
		break;
	case ATTR_DMP_MOTION_DETECT_ENABLE:
		if ((!!data) == st->chip_config.motion_detect_enable)
			return count;
		st->chip_config.motion_detect_enable = !!data;
		pr_info("Motion Detect %s\n",
			st->chip_config.motion_detect_enable ==
			1 ? "Enabled" : "Disabled");
		break;
	case ATTR_IN_POWER_ON:
		{
			u8 p0[2];
			u8 p1[2];

			power_on_data = (u32)data;
			p0[0] = (power_on_data & 0xff);
			p0[1] = ((power_on_data >> 8) & 0xff);
			p1[0] = ((power_on_data >> 16) & 0xff);
			p1[1] = ((power_on_data >> 24) & 0xff);

			if (st->bus_type == INV_BUS_SPI) {
				struct spi_transfer power_on;
				struct spi_message msg;

				memset(&power_on, 0,
					sizeof(struct spi_transfer));
				power_on.bits_per_word = 8;
				power_on.len = 2;
				power_on.tx_buf = p0;
				power_on.rx_buf = p1;
				spi_message_init(&msg);
				spi_message_add_tail(&power_on, &msg);
				spi_sync(to_spi_device(st->dev), &msg);

			} else if (st->bus_type == INV_BUS_I2C) {
				struct i2c_msg msgs[2];

				p0[0] &= 0x7f;
				msgs[0].addr = st->i2c_addr;
				msgs[0].flags = 0;	/* write */
				msgs[0].buf = &p0[0];
				msgs[0].len = 1;
				msgs[1].addr = st->i2c_addr;
				msgs[1].flags = I2C_M_RD;
				msgs[1].buf = &p1[1];
				msgs[1].len = 1;
				result = i2c_transfer(st->sl_handle, msgs, 2);
				if (result < 2)
					return -EIO;
			}
			st->power_on_data = ((p0[0] << 24) | (p0[1] << 16) |
							(p1[0] << 8) | p1[1]);
			return count;
		}
	case ATTR_DMP_EIS_ENABLE:
		if ((!!data) == st->chip_config.eis_enable)
			return count;
		st->chip_config.eis_enable = !!data;
		pr_info("Eis %s\n",
			st->chip_config.eis_enable == 1 ? "Enable" : "Disable");
		break;
	case ATTR_DMP_STEP_DETECTOR_ON:
		st->step_detector_l_on = !!data;
		break;
	case ATTR_DMP_STEP_DETECTOR_WAKE_ON:
		st->step_detector_wake_l_on = !!data;
		break;
	case ATTR_DMP_STEP_COUNTER_ON:
		st->step_counter_l_on = !!data;
		if (st->step_counter_l_on)
			send_ped = true;
		break;
	case ATTR_DMP_STEP_COUNTER_WAKE_ON:
		st->step_counter_wake_l_on = !!data;
		if (st->step_counter_wake_l_on)
			send_ped = true;
		break;
	case ATTR_DMP_STEP_COUNTER_SEND:
		if (st->step_counter_l_on || st->step_counter_wake_l_on)
			inv_send_pedo_steps(indio_dev);
		return count;
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		if (data == st->batch.timeout)
			return count;
		st->batch.timeout = data;
		break;
	case ATTR_HIGH_RES_MODE:
		if (!!data == st->chip_config.high_res_mode)
			return count;
		st->chip_config.high_res_mode = !!data;
		break;
	default:
		return -EINVAL;
	};
	inv_check_l_step(st);
	inv_check_sensor_on(st);

	st->trigger_state = EVENT_TRIGGER;
	result = set_inv_enable(indio_dev);
	if (result)
		return result;

	/* send the first step count after all configuration is done */
	if (send_ped)
		inv_send_pedo_steps(indio_dev);

	return count;
}

/*
 * inv_basic_attr_store()
 */
static ssize_t inv_basic_attr_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	mutex_lock(&st->lock);
	result = _basic_attr_store(dev, attr, buf, count);
	mutex_unlock(&st->lock);

	return result;
}

/*
 * inv_attr_show()
 */
static ssize_t inv_attr_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	s8 *m;

	switch (this_attr->address) {
	case ATTR_GYRO_SCALE:
		{
			const s16 gyro_scale[] = { 250, 500, 1000, 2000 };
			s16 fsr;

			/* fifo data will be 2000dps FSR regardless of
			 * FSR configuration when high resolution mode
			 */
			if (st->chip_config.high_res_mode)
				fsr = 2000;
			else
				fsr = gyro_scale[st->chip_config.fsr];

			return snprintf(buf, MAX_WR_SZ, "%d\n", fsr);
		}
	case ATTR_ACCEL_SCALE:
		{
			const s16 accel_scale[] = { 2, 4, 8, 16 };
			s16 fsr;

			/* fifo data will be 16g FSR regardless of
			 * FSR configuration when high resolution mode
			 */
			if (st->chip_config.high_res_mode)
				fsr = 16;
			else
				fsr = accel_scale[st->chip_config.accel_fs];

			return snprintf(buf, MAX_WR_SZ, "%d\n", fsr);
		}
	case ATTR_GYRO_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->chip_config.gyro_enable);
	case ATTR_ACCEL_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->chip_config.accel_enable);
	case ATTR_IN_POWER_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->power_on_data);
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->batch.timeout);
	case ATTR_DMP_PED_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->ped.on);
	case ATTR_DMP_PED_INT_MODE:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->ped.int_mode);
	case ATTR_DMP_SMD_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->smd.on);
	case ATTR_DMP_TILT_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->chip_config.tilt_enable);
	case ATTR_DMP_STATIONARY_DETECT_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->chip_config.stationary_detect_enable);
	case ATTR_DMP_MOTION_DETECT_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->chip_config.motion_detect_enable);
	case ATTR_DMP_EIS_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->chip_config.eis_enable);
	case ATTR_DMP_LP_EN_OFF:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->chip_config.lp_en_mode_off);
	case ATTR_DMP_STEP_COUNTER_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->step_counter_l_on);
	case ATTR_DMP_STEP_COUNTER_WAKE_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->step_counter_wake_l_on);
	case ATTR_DMP_STEP_DETECTOR_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->step_detector_l_on);
	case ATTR_DMP_STEP_DETECTOR_WAKE_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->step_detector_wake_l_on);
	case ATTR_GYRO_MATRIX:
		m = st->plat_data.orientation;
		return snprintf(buf, MAX_WR_SZ, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_ACCEL_MATRIX:
		m = st->plat_data.orientation;
		return snprintf(buf, MAX_WR_SZ, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_GYRO_SF:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->gyro_sf);
	case ATTR_ANGLVEL_X_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->gyro_st_bias[0]);
	case ATTR_ANGLVEL_Y_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->gyro_st_bias[1]);
	case ATTR_ANGLVEL_Z_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->gyro_st_bias[2]);
	case ATTR_GYRO_LP_MODE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->gyro_lp_mode);
	case ATTR_ACCEL_LP_MODE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->accel_lp_mode);
	case ATTR_ACCEL_X_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->accel_st_bias[0]);
	case ATTR_ACCEL_Y_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->accel_st_bias[1]);
	case ATTR_ACCEL_Z_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->accel_st_bias[2]);
	case ATTR_GYRO_X_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->input_gyro_bias[0]);
	case ATTR_GYRO_Y_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->input_gyro_bias[1]);
	case ATTR_GYRO_Z_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->input_gyro_bias[2]);
	case ATTR_ACCEL_X_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->input_accel_bias[0]);
	case ATTR_ACCEL_Y_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->input_accel_bias[1]);
	case ATTR_ACCEL_Z_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->input_accel_bias[2]);
	case ATTR_HIGH_RES_MODE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			st->chip_config.high_res_mode);
	default:
		return -EPERM;
	}
}

static ssize_t inv_self_test(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int res;

	mutex_lock(&st->lock);
	res = inv_hw_self_test(st);
	set_inv_enable(indio_dev);
	mutex_unlock(&st->lock);

	return snprintf(buf, MAX_WR_SZ, "%d\n", res);
}

/*
 * inv_smd_show() -  calling this function showes smd interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_smd_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_WR_SZ, "1\n");
}

#ifndef SUPPORT_ONLY_BASIC_FEATURES
/*
 *  inv_temperature_show() - Read temperature data directly from registers.
 */
static ssize_t inv_temperature_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return -ENODEV;
}

/*
 * inv_ped_show() -  calling this function showes pedometer interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_ped_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_WR_SZ, "1\n");
}

/*
 * inv_tilt_show() -  calling this function showes tilt interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_tilt_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_WR_SZ, "1\n");
}
#endif

/*
 *  inv_reg_dump_show() - Register dump for testing.
 */
static ssize_t inv_reg_dump_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int i;
	char data;
	int bytes_printed = 0;
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);

	mutex_lock(&st->lock);

	/* BANK 0 */
	bytes_printed += snprintf(buf + bytes_printed,
			MAX_WR_SZ - bytes_printed, "BANK 0\n");
	for (i = 0; i <= 0x7F; i++) {
		/* don't read fifo r/w register */
		if (i == REG_FIFO_DATA_REG)
			data = 0;
		else
			inv_plat_read(st, i, 1, &data);
		bytes_printed += snprintf(buf + bytes_printed,
				MAX_WR_SZ - bytes_printed,
				"%#2x: %#2x\n", i, data);
	}

	/* MREG TOP1 */
	inv_set_idle(st);
	bytes_printed += snprintf(buf + bytes_printed,
			MAX_WR_SZ - bytes_printed, "\nMREG TOP1\n");
	for (i = REG_TMST_CONFIG1_MREG_TOP1; i <= REG_ST_STATUS2_MREG_TOP1; i++) {
		inv_mreg_read(st, i, 1, &data);
		bytes_printed += snprintf(buf + bytes_printed,
				MAX_WR_SZ - bytes_printed,
				"%#4x: %#2x\n", i, data);
	}
	inv_reset_idle(st);

	mutex_unlock(&st->lock);

	return bytes_printed;
}

static ssize_t inv_flush_batch_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;

	mutex_lock(&st->lock);
	result = inv_flush_batch_data(indio_dev, data);
	mutex_unlock(&st->lock);

	return count;
}

static const struct iio_chan_spec inv_mpu_channels[] = {
	{
		.type = IIO_ACCEL,
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 64,
			.storagebits = 64,
		},
	},
	/* Standard raw channels: accel/gyro x XYZ */
	{
		.type = IIO_ACCEL,
		.modified = 1,
		.channel2 = IIO_MOD_X,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_index = -1,
	}, {
		.type = IIO_ACCEL,
		.modified = 1,
		.channel2 = IIO_MOD_Y,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_index = -1,
	}, {
		.type = IIO_ACCEL,
		.modified = 1,
		.channel2 = IIO_MOD_Z,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_index = -1,
	}, {
		.type = IIO_ANGL_VEL,
		.modified = 1,
		.channel2 = IIO_MOD_X,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_index = -1,
	}, {
		.type = IIO_ANGL_VEL,
		.modified = 1,
		.channel2 = IIO_MOD_Y,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_index = -1,
	}, {
		.type = IIO_ANGL_VEL,
		.modified = 1,
		.channel2 = IIO_MOD_Z,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_index = -1,
	},
};

static DEVICE_ATTR(poll_smd, S_IRUGO,
			inv_smd_show, NULL);
static DEVICE_ATTR(poll_pedometer, S_IRUGO,
			inv_ped_show, NULL);
static DEVICE_ATTR(poll_tilt, S_IRUGO,
			inv_tilt_show, NULL);

/* special run time sysfs entry, read only */
static DEVICE_ATTR(debug_reg_dump, S_IRUGO | S_IWUSR,
			inv_reg_dump_show, NULL);
#ifndef SUPPORT_ONLY_BASIC_FEATURES
static DEVICE_ATTR(out_temperature, S_IRUGO | S_IWUSR,
			inv_temperature_show, NULL);
#endif
static DEVICE_ATTR(misc_self_test, S_IRUGO | S_IWUSR,
			inv_self_test, NULL);

static IIO_DEVICE_ATTR(info_anglvel_matrix, S_IRUGO,
			inv_attr_show, NULL,
			ATTR_GYRO_MATRIX);
static IIO_DEVICE_ATTR(info_accel_matrix, S_IRUGO,
			inv_attr_show, NULL,
			ATTR_ACCEL_MATRIX);

static IIO_DEVICE_ATTR(info_gyro_sf, S_IRUGO,
			inv_attr_show, NULL,
			ATTR_GYRO_SF);
/* write only sysfs */
static DEVICE_ATTR(misc_flush_batch, S_IWUSR,
			NULL, inv_flush_batch_store);

/* sensor on/off sysfs control */
static IIO_DEVICE_ATTR(in_accel_enable, S_IRUGO | S_IWUSR,
			inv_sensor_on_show, inv_sensor_on_store,
			SENSOR_L_ACCEL);
static IIO_DEVICE_ATTR(in_anglvel_enable, S_IRUGO | S_IWUSR,
			inv_sensor_on_show, inv_sensor_on_store,
			SENSOR_L_GYRO);
#ifndef SUPPORT_ONLY_BASIC_FEATURES
static IIO_DEVICE_ATTR(in_eis_enable, S_IRUGO | S_IWUSR,
			inv_sensor_on_show, inv_sensor_on_store,
			SENSOR_L_EIS_GYRO);
#endif
static IIO_DEVICE_ATTR(in_accel_wake_enable, S_IRUGO | S_IWUSR,
			inv_sensor_on_show, inv_sensor_on_store,
			SENSOR_L_ACCEL_WAKE);
static IIO_DEVICE_ATTR(in_anglvel_wake_enable, S_IRUGO | S_IWUSR,
			inv_sensor_on_show, inv_sensor_on_store,
			SENSOR_L_GYRO_WAKE);

/* sensor rate sysfs control */
static IIO_DEVICE_ATTR(in_accel_rate, S_IRUGO | S_IWUSR,
			inv_sensor_rate_show, inv_sensor_rate_store,
			SENSOR_L_ACCEL);
static IIO_DEVICE_ATTR(in_anglvel_rate, S_IRUGO | S_IWUSR,
			inv_sensor_rate_show, inv_sensor_rate_store,
			SENSOR_L_GYRO);
#ifndef SUPPORT_ONLY_BASIC_FEATURES
static IIO_DEVICE_ATTR(in_eis_rate, S_IRUGO | S_IWUSR,
			inv_sensor_rate_show, inv_sensor_rate_store,
			SENSOR_L_EIS_GYRO);
#endif
static IIO_DEVICE_ATTR(in_accel_wake_rate, S_IRUGO | S_IWUSR,
			inv_sensor_rate_show,
			inv_sensor_rate_store,
			SENSOR_L_ACCEL_WAKE);
static IIO_DEVICE_ATTR(in_anglvel_wake_rate, S_IRUGO | S_IWUSR,
			inv_sensor_rate_show, inv_sensor_rate_store,
			SENSOR_L_GYRO_WAKE);

static IIO_DEVICE_ATTR(misc_batchmode_timeout, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_DMP_BATCHMODE_TIMEOUT);

/* engine scale */
static IIO_DEVICE_ATTR(in_accel_scale, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_misc_attr_store,
			ATTR_ACCEL_SCALE);
static IIO_DEVICE_ATTR(in_anglvel_scale, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_misc_attr_store,
			ATTR_GYRO_SCALE);

#ifndef SUPPORT_ONLY_BASIC_FEATURES
static IIO_DEVICE_ATTR(debug_lp_en_off, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_debug_store,
			ATTR_DMP_LP_EN_OFF);
static IIO_DEVICE_ATTR(debug_clock_sel, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_debug_store,
			ATTR_DMP_CLK_SEL);
static IIO_DEVICE_ATTR(debug_reg_write, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_debug_store,
			ATTR_DEBUG_REG_WRITE);
static IIO_DEVICE_ATTR(debug_reg_write_addr, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_debug_store,
			ATTR_DEBUG_REG_ADDR);
#endif

static IIO_DEVICE_ATTR(in_accel_x_st_calibbias, S_IRUGO | S_IWUSR,
			inv_attr_show, NULL, ATTR_ACCEL_X_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_y_st_calibbias, S_IRUGO | S_IWUSR,
			inv_attr_show, NULL, ATTR_ACCEL_Y_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_z_st_calibbias, S_IRUGO | S_IWUSR,
			inv_attr_show, NULL, ATTR_ACCEL_Z_ST_CALIBBIAS);

static IIO_DEVICE_ATTR(in_anglvel_x_st_calibbias, S_IRUGO | S_IWUSR,
			inv_attr_show, NULL, ATTR_ANGLVEL_X_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_y_st_calibbias, S_IRUGO | S_IWUSR,
			inv_attr_show, NULL, ATTR_ANGLVEL_Y_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_z_st_calibbias, S_IRUGO | S_IWUSR,
			inv_attr_show, NULL, ATTR_ANGLVEL_Z_ST_CALIBBIAS);

static IIO_DEVICE_ATTR(in_accel_x_offset, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_bias_store, ATTR_ACCEL_X_OFFSET);
static IIO_DEVICE_ATTR(in_accel_y_offset, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_bias_store, ATTR_ACCEL_Y_OFFSET);
static IIO_DEVICE_ATTR(in_accel_z_offset, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_bias_store, ATTR_ACCEL_Z_OFFSET);

static IIO_DEVICE_ATTR(in_anglvel_x_offset, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_bias_store, ATTR_GYRO_X_OFFSET);
static IIO_DEVICE_ATTR(in_anglvel_y_offset, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_bias_store, ATTR_GYRO_Y_OFFSET);
static IIO_DEVICE_ATTR(in_anglvel_z_offset, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_bias_store, ATTR_GYRO_Z_OFFSET);

static IIO_DEVICE_ATTR(info_gyro_lp_mode, S_IRUGO | S_IWUSR,
			inv_attr_show, NULL, ATTR_GYRO_LP_MODE);
static IIO_DEVICE_ATTR(info_accel_lp_mode, S_IRUGO | S_IWUSR,
			inv_attr_show, NULL, ATTR_ACCEL_LP_MODE);

#ifndef SUPPORT_ONLY_BASIC_FEATURES
static IIO_DEVICE_ATTR(in_step_detector_enable, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_DMP_STEP_DETECTOR_ON);
static IIO_DEVICE_ATTR(in_step_detector_wake_enable, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_DMP_STEP_DETECTOR_WAKE_ON);
static IIO_DEVICE_ATTR(in_step_counter_enable, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_DMP_STEP_COUNTER_ON);
static IIO_DEVICE_ATTR(in_step_counter_wake_enable, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_DMP_STEP_COUNTER_WAKE_ON);
static IIO_DEVICE_ATTR(in_step_counter_send, S_IRUGO | S_IWUSR,
		NULL, inv_basic_attr_store,
		ATTR_DMP_STEP_COUNTER_SEND);
static IIO_DEVICE_ATTR(event_smd_enable, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_DMP_SMD_ENABLE);

static IIO_DEVICE_ATTR(event_tilt_enable, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_DMP_TILT_ENABLE);

static IIO_DEVICE_ATTR(event_eis_enable, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_DMP_EIS_ENABLE);

static IIO_DEVICE_ATTR(event_stationary_detect_enable, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_DMP_STATIONARY_DETECT_ENABLE);

static IIO_DEVICE_ATTR(event_motion_detect_enable, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_DMP_MOTION_DETECT_ENABLE);

static IIO_DEVICE_ATTR(in_power_on, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_IN_POWER_ON);

static IIO_DEVICE_ATTR(in_high_res_mode, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store,
			ATTR_HIGH_RES_MODE);

static IIO_DEVICE_ATTR(params_pedometer_int_on, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_misc_attr_store, ATTR_DMP_PED_INT_ON);
static IIO_DEVICE_ATTR(event_pedometer_enable, S_IRUGO | S_IWUSR,
			inv_attr_show, inv_basic_attr_store, ATTR_DMP_PED_ON);
static IIO_DEVICE_ATTR(params_pedometer_int_mode, S_IRUGO | S_IWUSR,
		inv_attr_show, inv_misc_attr_store, ATTR_DMP_PED_INT_MODE);
#endif

static const struct attribute *inv_raw_attributes[] = {
	&dev_attr_debug_reg_dump.attr,
#ifndef SUPPORT_ONLY_BASIC_FEATURES
	&dev_attr_out_temperature.attr,
#endif
	&dev_attr_misc_flush_batch.attr,
	&dev_attr_misc_self_test.attr,
#ifndef SUPPORT_ONLY_BASIC_FEATURES
	&iio_dev_attr_in_power_on.dev_attr.attr,
#endif
	&iio_dev_attr_in_high_res_mode.dev_attr.attr,
	&iio_dev_attr_in_accel_enable.dev_attr.attr,
	&iio_dev_attr_in_accel_wake_enable.dev_attr.attr,
	&iio_dev_attr_info_accel_matrix.dev_attr.attr,
	&iio_dev_attr_in_accel_scale.dev_attr.attr,
	&iio_dev_attr_misc_batchmode_timeout.dev_attr.attr,
	&iio_dev_attr_in_accel_rate.dev_attr.attr,
	&iio_dev_attr_in_accel_wake_rate.dev_attr.attr,
	&iio_dev_attr_info_accel_lp_mode.dev_attr.attr,
};

#ifndef SUPPORT_ONLY_BASIC_FEATURES
static const struct attribute *inv_debug_attributes[] = {
	&iio_dev_attr_debug_lp_en_off.dev_attr.attr,
	&iio_dev_attr_debug_clock_sel.dev_attr.attr,
	&iio_dev_attr_debug_reg_write.dev_attr.attr,
	&iio_dev_attr_debug_reg_write_addr.dev_attr.attr,
};
#endif

static const struct attribute *inv_gyro_attributes[] = {
	&iio_dev_attr_info_anglvel_matrix.dev_attr.attr,
	&iio_dev_attr_in_anglvel_enable.dev_attr.attr,
	&iio_dev_attr_in_anglvel_rate.dev_attr.attr,
#ifndef SUPPORT_ONLY_BASIC_FEATURES
	&iio_dev_attr_in_eis_enable.dev_attr.attr,
#endif
	&iio_dev_attr_in_anglvel_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_anglvel_scale.dev_attr.attr,
#ifndef SUPPORT_ONLY_BASIC_FEATURES
	&iio_dev_attr_in_eis_rate.dev_attr.attr,
#endif
	&iio_dev_attr_in_anglvel_wake_rate.dev_attr.attr,
	&iio_dev_attr_info_gyro_sf.dev_attr.attr,
	&iio_dev_attr_info_gyro_lp_mode.dev_attr.attr,
};

static const struct attribute *inv_bias_attributes[] = {
	&iio_dev_attr_in_accel_x_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_x_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_y_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_z_offset.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_offset.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_offset.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_offset.dev_attr.attr,
};

#ifndef SUPPORT_ONLY_BASIC_FEATURES
static const struct attribute *inv_pedometer_attributes[] = {
	&dev_attr_poll_pedometer.attr,
	&dev_attr_poll_tilt.attr,
	&iio_dev_attr_params_pedometer_int_on.dev_attr.attr,
	&iio_dev_attr_params_pedometer_int_mode.dev_attr.attr,
	&iio_dev_attr_event_pedometer_enable.dev_attr.attr,
	&iio_dev_attr_event_tilt_enable.dev_attr.attr,
	&iio_dev_attr_event_eis_enable.dev_attr.attr,
	&iio_dev_attr_event_stationary_detect_enable.dev_attr.attr,
	&iio_dev_attr_event_motion_detect_enable.dev_attr.attr,
	&iio_dev_attr_in_step_counter_enable.dev_attr.attr,
	&iio_dev_attr_in_step_counter_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_step_counter_send.dev_attr.attr,
	&iio_dev_attr_in_step_detector_enable.dev_attr.attr,
	&iio_dev_attr_in_step_detector_wake_enable.dev_attr.attr,
};
#endif

static const struct attribute *inv_smd_attributes[] = {
	&dev_attr_poll_smd.attr,
	&iio_dev_attr_event_smd_enable.dev_attr.attr,
};

static struct attribute *inv_attributes[ARRAY_SIZE(inv_raw_attributes) +
#ifndef SUPPORT_ONLY_BASIC_FEATURES
					ARRAY_SIZE(inv_debug_attributes) +
#endif
					ARRAY_SIZE(inv_gyro_attributes) +
					ARRAY_SIZE(inv_bias_attributes) +
#ifndef SUPPORT_ONLY_BASIC_FEATURES
					ARRAY_SIZE(inv_pedometer_attributes) +
#endif
					ARRAY_SIZE(inv_smd_attributes) +
					 + 1];

static const struct attribute_group inv_attribute_group = {
	.name = "mpu",
	.attrs = inv_attributes
};

/*
 * Input streaming, sample_imu_input compatible (same frame as icm42688):
 * EV_MSC(0x04) x 9 - 6 axis raw, 64-bit irq timestamp, irq count.
 */
#define ICM42607_MSC_DATA	0x04

static void inv_icm42607_input_report(struct inv_mpu_state *st, u64 irq_ts,
				      s16 ax, s16 ay, s16 az,
				      s16 gx, s16 gy, s16 gz)
{
	struct input_dev *input = st->input;

	st->irq_count++;
	input_event(input, EV_MSC, ICM42607_MSC_DATA, (u32)(s16)ax);
	input_event(input, EV_MSC, ICM42607_MSC_DATA, (u32)(s16)ay);
	input_event(input, EV_MSC, ICM42607_MSC_DATA, (u32)(s16)az);
	input_event(input, EV_MSC, ICM42607_MSC_DATA, (u32)(s16)gx);
	input_event(input, EV_MSC, ICM42607_MSC_DATA, (u32)(s16)gy);
	input_event(input, EV_MSC, ICM42607_MSC_DATA, (u32)(s16)gz);
	input_event(input, EV_MSC, ICM42607_MSC_DATA,
		    (u32)((irq_ts >> 32) & 0xFFFFFFFF));
	input_event(input, EV_MSC, ICM42607_MSC_DATA, (u32)(irq_ts & 0xFFFFFFFF));
	input_event(input, EV_MSC, ICM42607_MSC_DATA, (u32)st->irq_count);
	input_sync(input);
}

static int inv_icm42607_input_drdy(struct inv_mpu_state *st)
{
	s16 raw[6];
	u64 irq_ts;
	int result;

	result = inv_plat_read(st, REG_ACCEL_DATA_X0_UI, sizeof(raw), (u8 *)raw);
	if (result)
		return result;

	/* serve in_*_raw from the same sample */
	st->last_raw_accel[0] = raw[0];
	st->last_raw_accel[1] = raw[1];
	st->last_raw_accel[2] = raw[2];
	st->last_raw_gyro[0] = raw[3];
	st->last_raw_gyro[1] = raw[4];
	st->last_raw_gyro[2] = raw[5];
	st->last_raw_valid = true;

	/* input mode bypasses the FIFO: feed IIO buffer directly */
	if (st->buffer_on && st->indio && st->trig) {
		s32 buf[4];
		s64 t = get_time_ns();

		buf[0] = raw[0];
		buf[1] = raw[1];
		buf[2] = raw[2];
		buf[3] = t & 0xFFFFFFFF;
		iio_push_to_buffers(st->indio, buf);
		inv_push_timestamp(st->indio, t);
	}

	irq_ts = get_time_ns();
	inv_icm42607_input_report(st, irq_ts,
				  be16_to_cpu(raw[0]), be16_to_cpu(raw[1]),
				  be16_to_cpu(raw[2]), be16_to_cpu(raw[3]),
				  be16_to_cpu(raw[4]), be16_to_cpu(raw[5]));
	return 0;
}

int inv_mpu_input_drdy(struct inv_mpu_state *st)
{
	return inv_icm42607_input_drdy(st);
}

static void inv_icm42607_input_unregister(void *data)
{
	struct inv_mpu_state *st = data;

	if (st->input)
		input_unregister_device(st->input);
}

int inv_mpu_input_init(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	st->input = devm_input_allocate_device(st->dev);
	if (!st->input)
		return -ENOMEM;

	st->input->name = "icm42607-sensor";
	st->input->phys = "icm42607/input0";
	st->input->id.bustype =
		(st->bus_type == INV_BUS_SPI) ? BUS_SPI : BUS_I2C;
	st->input->dev.parent = st->dev;
	input_set_capability(st->input, EV_MSC, ICM42607_MSC_DATA);
	input_set_events_per_packet(st->input, 100);

	result = input_register_device(st->input);
	if (result)
		return result;

	/* Force 800Hz stream + DRDY on INT1, synced via the standard path. */
	mutex_lock(&st->lock);
	st->sensor_l[SENSOR_L_ACCEL].rate = 800;
	st->sensor_l[SENSOR_L_ACCEL].on = true;
	st->sensor_l[SENSOR_L_GYRO].rate = 800;
	st->sensor_l[SENSOR_L_GYRO].on = true;
	st->trigger_state = DATA_TRIGGER;
	inv_check_sensor_on(st);
	result = inv_set_power(st, true);
	if (!result)
		result = set_inv_enable(indio_dev);
	mutex_unlock(&st->lock);
	if (result) {
		input_unregister_device(st->input);
		st->input = NULL;
		return result;
	}

	result = devm_add_action(st->dev, inv_icm42607_input_unregister, st);
	if (result) {
		input_unregister_device(st->input);
		st->input = NULL;
		return result;
	}

	st->input_stream_on = true;
	dev_info(st->dev, "icm42607-sensor input ready, DRDY on INT1\n");
	return 0;
}
EXPORT_SYMBOL_GPL(inv_mpu_input_init);

/*
 * in_*_raw: latest cached sample (LSB against configured FSR).
 */
static int inv_mpu_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int idx;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (!st->last_raw_valid || !st->resume_state)
			return -EBUSY;
		if (chan->channel2 < IIO_MOD_X || chan->channel2 > IIO_MOD_Z)
			return -EINVAL;
		idx = chan->channel2 - IIO_MOD_X;
		if (chan->type == IIO_ACCEL)
			*val = st->last_raw_accel[idx];
		else if (chan->type == IIO_ANGL_VEL)
			*val = st->last_raw_gyro[idx];
		else
			return -EINVAL;
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
}

static const struct iio_info mpu_info = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	.driver_module = THIS_MODULE,
#endif
	.attrs = &inv_attribute_group,
	.read_raw = &inv_mpu_read_raw,
};

/*
 *  inv_check_chip_type() - check and setup chip type.
 */
int inv_check_chip_type(struct iio_dev *indio_dev, const char *name)
{
	int result;
	int t_ind;
	struct inv_chip_config_s *conf;
	struct mpu_platform_data *plat;
	struct inv_mpu_state *st;

	st = iio_priv(indio_dev);
	conf = &st->chip_config;
	plat = &st->plat_data;

	if (!strcmp(name, "icm43600")) {
		st->chip_type = ICM43600;
		st->apex_supported = true;
		st->smd_supported = true;
	} else
		return -EPERM;
	st->chip_config.has_gyro = 1;

	st->hw = &hw_info[st->chip_type];
	st->indio = indio_dev;
	result = inv_mpu_initialize(st);
	if (result)
		return result;

	t_ind = 0;
	memcpy(&inv_attributes[t_ind], inv_raw_attributes,
				sizeof(inv_raw_attributes));
	t_ind += ARRAY_SIZE(inv_raw_attributes);

#ifndef SUPPORT_ONLY_BASIC_FEATURES
	memcpy(&inv_attributes[t_ind], inv_pedometer_attributes,
				sizeof(inv_pedometer_attributes));
	t_ind += ARRAY_SIZE(inv_pedometer_attributes);
#endif

	memcpy(&inv_attributes[t_ind], inv_gyro_attributes,
				sizeof(inv_gyro_attributes));
	t_ind += ARRAY_SIZE(inv_gyro_attributes);

	memcpy(&inv_attributes[t_ind], inv_bias_attributes,
				sizeof(inv_bias_attributes));
	t_ind += ARRAY_SIZE(inv_bias_attributes);

#ifndef SUPPORT_ONLY_BASIC_FEATURES
	memcpy(&inv_attributes[t_ind], inv_debug_attributes,
				sizeof(inv_debug_attributes));
	t_ind += ARRAY_SIZE(inv_debug_attributes);
#endif

	memcpy(&inv_attributes[t_ind], inv_smd_attributes,
				sizeof(inv_smd_attributes));
	t_ind += ARRAY_SIZE(inv_smd_attributes);

	inv_attributes[t_ind] = NULL;

	indio_dev->name = st->hw->name;
	indio_dev->channels = inv_mpu_channels;
	indio_dev->num_channels = ARRAY_SIZE(inv_mpu_channels);

	indio_dev->info = &mpu_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	/* currentmode removed in 5.19, core sets it from modes */

	return result;
}
EXPORT_SYMBOL_GPL(inv_check_chip_type);

int inv_create_dmp_sysfs(struct iio_dev *ind)
{
	/* dummy */
	return 0;
}
EXPORT_SYMBOL_GPL(inv_create_dmp_sysfs);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense device ICM40xxx driver");
MODULE_LICENSE("GPL");
