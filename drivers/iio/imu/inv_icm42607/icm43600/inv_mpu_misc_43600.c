/*
 * Copyright (C) 2018-2021 InvenSense, Inc.
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

#include <linux/delay.h>
#include "../inv_mpu_iio.h"

/**
 * inv_set_idle() - Set Idle bit in PWR_MGMT_0 register
 * @st: struct inv_mpu_state.
 *
 * Set ACCEL_LP_CLK_SEL as well when necessary with a proper wait
 *
 * Return: 0 when successful.
 */
int inv_set_idle(struct inv_mpu_state *st)
{
	int ret;
	u8 reg_pwr_mgmt_0;
	u8 d;
	unsigned long delay_us;

	ret = inv_plat_read(st, REG_PWR_MGMT_0, 1, &reg_pwr_mgmt_0);
	if (ret)
		return ret;

	/* set Idle bit.
	 * when accel LPM is already enabled, set ACCEL_LP_CLK_SEL bit as well.
	 */
	d = reg_pwr_mgmt_0;
	d |= BIT_IDLE;
	if ((d & BIT_ACCEL_MODE_MASK) == BIT_ACCEL_MODE_LPM &&
			(d & BIT_GYRO_MODE_MASK) == 0) {
		d |= BIT_ACCEL_LP_CLK_SEL;
		delay_us = USEC_PER_SEC /
			st->eng_info[ENGINE_ACCEL].running_rate;
	} else
		delay_us = INV_ICM43600_MCLK_WAIT_US;

	if (reg_pwr_mgmt_0 != d) {
		ret = inv_plat_single_write(st, REG_PWR_MGMT_0, d);
		usleep_range(delay_us, delay_us + 10);
	}

	return ret;
}

/**
 * inv_reset_idle() - Reset Idle bit in PWR_MGMT_0 register
 * @st: struct inv_mpu_state.
 *
 * Reset ACCEL_LP_CLK_SEL as well
 *
 * Return: 0 when successful.
 */
int inv_reset_idle(struct inv_mpu_state *st)
{
	int ret;
	u8 reg_pwr_mgmt_0;
	u8 d;

	ret = inv_plat_read(st, REG_PWR_MGMT_0, 1, &reg_pwr_mgmt_0);
	if (ret)
		return ret;

	/* reset Idle bit.
	 * note that ACCEL_LP_CLK_SEL bit is reset as well here.
	 */
	d = reg_pwr_mgmt_0;
	d &= ~(BIT_IDLE | BIT_ACCEL_LP_CLK_SEL);
	if (reg_pwr_mgmt_0 != d)
		ret = inv_plat_single_write(st, REG_PWR_MGMT_0, d);

	return ret;
}

/**
 * inv_mreg_single_write() - Single byte write to MREG area.
 * @st: struct inv_mpu_state.
 * @addr: MREG register address including bank in upper byte.
 * @data: data to write.
 *
 * Return: 0 when successful.
 */
int inv_mreg_single_write(struct inv_mpu_state *st, int addr, u8 data)
{
	int ret;
	u8 reg_pwr_mgmt_0;

	ret = inv_plat_read(st, REG_PWR_MGMT_0, 1, &reg_pwr_mgmt_0);
	if (ret)
		return ret;

	ret = inv_set_idle(st);
	if (ret)
		return ret;

	ret = inv_plat_single_write(st, REG_BLK_SEL_W, (addr >> 8) & 0xff);
	usleep_range(INV_ICM43600_BLK_SEL_WAIT_US,
			INV_ICM43600_BLK_SEL_WAIT_US + 1);
	if (ret)
		goto restore_bank;

	ret = inv_plat_single_write(st, REG_MADDR_W, addr & 0xff);
	usleep_range(INV_ICM43600_MADDR_WAIT_US,
			INV_ICM43600_MADDR_WAIT_US + 1);
	if (ret)
		goto restore_bank;

	ret = inv_plat_single_write(st, REG_M_W, data);
	usleep_range(INV_ICM43600_M_RW_WAIT_US,
			INV_ICM43600_M_RW_WAIT_US + 1);
	if (ret)
		goto restore_bank;

restore_bank:
	ret |= inv_plat_single_write(st, REG_BLK_SEL_W, 0);
	usleep_range(INV_ICM43600_BLK_SEL_WAIT_US,
			INV_ICM43600_BLK_SEL_WAIT_US + 1);

	ret |= inv_plat_single_write(st, REG_PWR_MGMT_0, reg_pwr_mgmt_0);

	return ret;
}

/**
 * inv_mreg_read() - Multiple byte read from MREG area.
 * @st: struct inv_mpu_state.
 * @addr: MREG register start address including bank in upper byte.
 * @len: length to read in byte.
 * @data: pointer to store read data.
 *
 * Return: 0 when successful.
 */
int inv_mreg_read(struct inv_mpu_state *st, int addr, int len, u8 *data)
{
	int ret;
	u8 reg_pwr_mgmt_0;

	ret = inv_plat_read(st, REG_PWR_MGMT_0, 1, &reg_pwr_mgmt_0);
	if (ret)
		return ret;

	ret = inv_set_idle(st);
	if (ret)
		return ret;

	ret = inv_plat_single_write(st, REG_BLK_SEL_R, (addr >> 8) & 0xff);
	usleep_range(INV_ICM43600_BLK_SEL_WAIT_US,
			INV_ICM43600_BLK_SEL_WAIT_US + 1);
	if (ret)
		goto restore_bank;

	ret = inv_plat_single_write(st, REG_MADDR_R, addr & 0xff);
	usleep_range(INV_ICM43600_MADDR_WAIT_US,
			INV_ICM43600_MADDR_WAIT_US + 1);
	if (ret)
		goto restore_bank;

	ret = inv_plat_read(st, REG_M_R, len, data);
	usleep_range(INV_ICM43600_M_RW_WAIT_US,
			INV_ICM43600_M_RW_WAIT_US + 1);
	if (ret)
		goto restore_bank;

restore_bank:
	ret |= inv_plat_single_write(st, REG_BLK_SEL_R, 0);
	usleep_range(INV_ICM43600_BLK_SEL_WAIT_US,
			INV_ICM43600_BLK_SEL_WAIT_US + 1);

	ret |= inv_plat_single_write(st, REG_PWR_MGMT_0, reg_pwr_mgmt_0);

	return ret;
}

/**
 * inv_get_apex_enabled() - Check if any APEX feature is enabled
 * @st: struct inv_mpu_state.
 *
 * Return: true when any is enabled, otherwise false.
 */
bool inv_get_apex_enabled(struct inv_mpu_state *st)
{
	if (!(st->apex_supported))
		return false;

	if (st->step_detector_l_on ||
		st->step_detector_wake_l_on ||
		st->step_counter_l_on ||
		st->step_counter_wake_l_on)
		return true;
	if (st->chip_config.tilt_enable)
		return true;
	if (st->smd.on)
		return true;

	return false;
}

/**
 * inv_get_apex_odr() - Get min accel ODR according to enabled APEX feature
 * @st: struct inv_mpu_state.
 *
 * Return: min accel ODR in Hz
 */
int inv_get_apex_odr(struct inv_mpu_state *st)
{
	int odr_hz = 0;

#ifdef SUPPORT_ACCEL_LPM
	odr_hz = MPU_INIT_SENSOR_RATE_LPM;
#else
	odr_hz = MPU_INIT_SENSOR_RATE_LNM;
#endif
	if (st->apex_supported) {
		/* returns min accel rate for each algorithm */
		if (st->step_detector_l_on ||
			st->step_detector_wake_l_on ||
			st->step_counter_l_on ||
			st->step_counter_wake_l_on ||
			st->chip_config.tilt_enable ||
			st->smd.on)
			odr_hz = 50;
	}

	return odr_hz;
}
