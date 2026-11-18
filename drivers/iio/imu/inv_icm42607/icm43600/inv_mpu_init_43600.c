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
#include "../inv_mpu_iio.h"

static int inv_read_timebase(struct inv_mpu_state *st)
{
	st->eng_info[ENGINE_ACCEL].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_ACCEL].base_time_1k = NSEC_PER_SEC;
	st->eng_info[ENGINE_ACCEL].base_time_vr = NSEC_PER_SEC;
	/* talor expansion to calculate base time unit */
	st->eng_info[ENGINE_GYRO].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_GYRO].base_time_1k = NSEC_PER_SEC;
	st->eng_info[ENGINE_GYRO].base_time_vr = NSEC_PER_SEC;
	st->eng_info[ENGINE_I2C].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_I2C].base_time_1k = NSEC_PER_SEC;
	st->eng_info[ENGINE_I2C].base_time_vr = NSEC_PER_SEC;

	st->eng_info[ENGINE_ACCEL].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_GYRO].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_I2C].orig_rate = BASE_SAMPLE_RATE;

	return 0;
}

int inv_set_gyro_sf(struct inv_mpu_state *st)
{
	int result;
	u8 data;

	result = inv_plat_read(st, REG_GYRO_CONFIG0, 1, &data);
	if (result)
		return result;
	data &= ~BIT_GYRO_FSR;
	data |= (3 - st->chip_config.fsr) << SHIFT_GYRO_FS_SEL;
	result = inv_plat_single_write(st, REG_GYRO_CONFIG0,
				   data);
	return result;
}

int inv_set_accel_sf(struct inv_mpu_state *st)
{
	int result;
	u8 data;

	result = inv_plat_read(st, REG_ACCEL_CONFIG0, 1, &data);
	if (result)
		return result;
	data &= ~BIT_ACCEL_FSR;
	data |= (3 - st->chip_config.accel_fs) << SHIFT_ACCEL_FS_SEL;
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG0,
			   data);
	return result;
}

int inv_set_accel_intel(struct inv_mpu_state *st)
{
	int result = 0;
	int8_t val, accel_rate;

	if (st->eng_info[ENGINE_ACCEL].divider) {
		accel_rate = 800 / st->eng_info[ENGINE_ACCEL].divider;
	} else {
		/* use dummy rate */
#ifdef SUPPORT_ACCEL_LPM
		accel_rate = MPU_INIT_SENSOR_RATE_LPM;
#else
		accel_rate = MPU_INIT_SENSOR_RATE_LNM;
#endif
	}

	if (accel_rate > 50)
		val = WOM_THRESHOLD / (accel_rate / 50);
	else
		val = WOM_THRESHOLD;
	result |= inv_mreg_single_write(st, REG_ACCEL_WOM_X_THR_MREG_TOP1, val);
	result |= inv_mreg_single_write(st, REG_ACCEL_WOM_Y_THR_MREG_TOP1, val);
	result |= inv_mreg_single_write(st, REG_ACCEL_WOM_Z_THR_MREG_TOP1, val);
	if (result)
		return result;

	return result;
}

int inv_config_apex_gestures(struct inv_mpu_state *st)
{
	int result = 0;
	int8_t rw;
	int8_t dmp_power_save_time = 2;
	int8_t low_energy_amp_th = 10;
	int8_t pedo_amp_th_sel = 8;
	int8_t pedo_step_cnt_th_sel = 5;
	int8_t pedo_step_det_th_sel = 2;
	int8_t pedo_sb_timer_th_sel = 4;
	int8_t pedo_hi_enrgy_th_sel = 1;
	int8_t tilt_wait_time = 1; /* default: 2 */
	int8_t smd_sensitivity = 0;
	int8_t sensitivity_mode = 0;

	/* REG_APEX_CONFIG2_MREG_TOP1 */
	rw = dmp_power_save_time & 0x0f;
	rw |= (low_energy_amp_th << 4) & 0xf0;
	result = inv_mreg_single_write(st, REG_APEX_CONFIG2_MREG_TOP1, rw);
	if (result)
		return result;

	/* REG_APEX_CONFIG3_MREG_TOP1 */
	rw = (pedo_amp_th_sel << 4) & 0xf0;
	rw |= pedo_step_cnt_th_sel & 0x0f;
	result = inv_mreg_single_write(st, REG_APEX_CONFIG3_MREG_TOP1, rw);
	if (result)
		return result;

	/* REG_APEX_CONFIG4_MREG_TOP1 */
	rw = (pedo_step_det_th_sel << 5) & 0xe0;
	rw |= (pedo_sb_timer_th_sel << 2) & 0x1c;
	rw |= pedo_hi_enrgy_th_sel & 0x03;
	result = inv_mreg_single_write(st, REG_APEX_CONFIG4_MREG_TOP1, rw);
	if (result)
		return result;

	/* REG_APEX_CONFIG5_MREG_TOP1 */
	result = inv_mreg_read(st, REG_APEX_CONFIG5_MREG_TOP1, 1, &rw);
	if (result)
		return result;
	rw &= 0x3f;
	rw |= (tilt_wait_time << 6) & 0xc0;
	result = inv_mreg_single_write(st, REG_APEX_CONFIG5_MREG_TOP1, rw);
	if (result)
		return result;

	/* REG_APEX_CONFIG9_MREG_TOP1 */
	result = inv_mreg_read(st, REG_APEX_CONFIG9_MREG_TOP1, 1, &rw);
	if (result)
		return result;
	rw &= 0xf0;
	rw |= (smd_sensitivity << 1) & 0x0e;
	rw |= (sensitivity_mode << 0) & 0x01;
	result = inv_mreg_single_write(st, REG_APEX_CONFIG9_MREG_TOP1, rw);

	return result;
}

static void inv_init_sensor_struct(struct inv_mpu_state *st)
{
	int i;

#ifdef SUPPORT_ACCEL_LPM
	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = MPU_INIT_SENSOR_RATE_LPM;
#else
	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = MPU_INIT_SENSOR_RATE_LNM;
#endif
	st->sensor[SENSOR_GYRO].rate = MPU_INIT_SENSOR_RATE_LNM;

	st->sensor[SENSOR_ACCEL].sample_size = BYTES_PER_SENSOR;
	st->sensor[SENSOR_TEMP].sample_size = BYTES_FOR_TEMP;
	st->sensor[SENSOR_GYRO].sample_size = BYTES_PER_SENSOR;

	st->sensor_l[SENSOR_L_SIXQ].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_PEDQ].base = SENSOR_GYRO;

	st->sensor_l[SENSOR_L_SIXQ_WAKE].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_PEDQ_WAKE].base = SENSOR_GYRO;

	st->sensor[SENSOR_ACCEL].a_en = true;
	st->sensor[SENSOR_GYRO].a_en = false;

	st->sensor[SENSOR_ACCEL].g_en = false;
	st->sensor[SENSOR_GYRO].g_en = true;

	st->sensor[SENSOR_ACCEL].c_en = false;
	st->sensor[SENSOR_GYRO].c_en = false;

	st->sensor[SENSOR_ACCEL].p_en = false;
	st->sensor[SENSOR_GYRO].p_en = false;

	st->sensor[SENSOR_ACCEL].engine_base = ENGINE_ACCEL;
	st->sensor[SENSOR_GYRO].engine_base = ENGINE_GYRO;

	st->sensor_l[SENSOR_L_ACCEL].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GESTURE_ACCEL].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GYRO].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_GYRO_CAL].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_EIS_GYRO].base = SENSOR_GYRO;

	st->sensor_l[SENSOR_L_ACCEL_WAKE].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GYRO_WAKE].base = SENSOR_GYRO;

	st->sensor_l[SENSOR_L_GYRO_CAL_WAKE].base = SENSOR_GYRO;

	st->sensor_l[SENSOR_L_ACCEL].header = ACCEL_HDR;
	st->sensor_l[SENSOR_L_GESTURE_ACCEL].header = ACCEL_HDR;
	st->sensor_l[SENSOR_L_GYRO].header = GYRO_HDR;
	st->sensor_l[SENSOR_L_GYRO_CAL].header = GYRO_CALIB_HDR;

	st->sensor_l[SENSOR_L_EIS_GYRO].header = EIS_GYRO_HDR;
	st->sensor_l[SENSOR_L_SIXQ].header = SIXQUAT_HDR;
	st->sensor_l[SENSOR_L_THREEQ].header = LPQ_HDR;
	st->sensor_l[SENSOR_L_NINEQ].header = NINEQUAT_HDR;
	st->sensor_l[SENSOR_L_PEDQ].header = PEDQUAT_HDR;

	st->sensor_l[SENSOR_L_ACCEL_WAKE].header = ACCEL_WAKE_HDR;
	st->sensor_l[SENSOR_L_GYRO_WAKE].header = GYRO_WAKE_HDR;
	st->sensor_l[SENSOR_L_GYRO_CAL_WAKE].header = GYRO_CALIB_WAKE_HDR;
	st->sensor_l[SENSOR_L_MAG_WAKE].header = COMPASS_WAKE_HDR;
	st->sensor_l[SENSOR_L_MAG_CAL_WAKE].header = COMPASS_CALIB_WAKE_HDR;
	st->sensor_l[SENSOR_L_SIXQ_WAKE].header = SIXQUAT_WAKE_HDR;
	st->sensor_l[SENSOR_L_NINEQ_WAKE].header = NINEQUAT_WAKE_HDR;
	st->sensor_l[SENSOR_L_PEDQ_WAKE].header = PEDQUAT_WAKE_HDR;

	st->sensor_l[SENSOR_L_ACCEL].wake_on = false;
	st->sensor_l[SENSOR_L_GYRO].wake_on = false;
	st->sensor_l[SENSOR_L_GYRO_CAL].wake_on = false;
	st->sensor_l[SENSOR_L_MAG].wake_on = false;
	st->sensor_l[SENSOR_L_MAG_CAL].wake_on = false;
	st->sensor_l[SENSOR_L_EIS_GYRO].wake_on = false;
	st->sensor_l[SENSOR_L_SIXQ].wake_on = false;
	st->sensor_l[SENSOR_L_NINEQ].wake_on = false;
	st->sensor_l[SENSOR_L_PEDQ].wake_on = false;

	st->sensor_l[SENSOR_L_ACCEL_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_GYRO_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_GYRO_CAL_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_MAG_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_SIXQ_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_NINEQ_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_PEDQ_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_GESTURE_ACCEL].wake_on = true;
}

static int inv_init_config(struct inv_mpu_state *st)
{
	int res, i;

	st->batch.overflow_on = 0;
	st->chip_config.fsr = MPU_INIT_GYRO_SCALE;
	st->chip_config.accel_fs = MPU_INIT_ACCEL_SCALE;
	st->ped.int_thresh = MPU_INIT_PED_INT_THRESH;
	st->ped.step_thresh = MPU_INIT_PED_STEP_THRESH;
	st->chip_config.low_power_gyro_on = 1;
	st->eis.count_precision = NSEC_PER_MSEC;
	st->firmware = 0;
	st->fifo_count_mode = BYTE_MODE;

	st->eng_info[ENGINE_GYRO].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_ACCEL].base_time = NSEC_PER_SEC;

	inv_init_sensor_struct(st);
	res = inv_read_timebase(st);
	if (res)
		return res;

	res = inv_set_gyro_sf(st);
	if (res)
		return res;
	res = inv_set_accel_sf(st);
	if (res)
		return res;
	res =  inv_set_accel_intel(st);
	if (res)
		return res;

	if (st->apex_supported) {
		res =  inv_config_apex_gestures(st);
		if (res)
			return res;
	}

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].ts = 0;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].previous_ts = 0;

	return res;
}

int inv_mpu_initialize(struct inv_mpu_state *st)
{
	u8 v;
	int result;

	/* WHOAMI gate: 0x61=ICM-42607, 0x47=ICM-42688 */
	result = inv_plat_read(st, REG_WHO_AM_I, 1, &v);
	if (result)
		return result;
	pr_info("whoami= %x\n", v);
	if (v != ICM43600_WHOAMI) {
		if (v == 0x47)
			pr_err("inv-mpu-iio: ICM-42688 detected, use inv_icm42600 driver instead\n");
		else
			pr_err("inv-mpu-iio: WHOAMI=0x%02x, expected 0x%02x (ICM-42607)\n",
			       v, ICM43600_WHOAMI);
		return -ENODEV;
	}

	/* reset */
	result = inv_plat_single_write(st, REG_SIGNAL_PATH_RESET,
				       BIT_SOFT_RESET_CHIP_CONFIG);
	if (result)
		return result;
	usleep_range(1000, 2000);

	/* check reset done bit in interrupt status */
	result = inv_plat_read(st, REG_INT_STATUS, 1, &v);
	if (result)
		return result;
	if (!(v & BIT_INT_STATUS_RESET_DONE)) {
		pr_err("reset done status bit missing (%x)\n", v);
		return -ENODEV;
	}

	/* SPI or I2C only
	 * FIFO count  : byte mode, big endian
	 * sensor data : big endian
	 */
	v = st->i2c_dis;
	v |= BIT_FIFO_COUNT_ENDIAN;
	v |= BIT_SENSOR_DATA_ENDIAN;
	result = inv_plat_single_write(st, REG_INTF_CONFIG0, v);
	if (result)
		return result;

	/* configure clock */
	v = BIT_GYRO_AFSR_MODE_DYN | BIT_CLK_SEL_PLL;
	if (st->i2c_dis != BIT_SIFS_CFG_I2C_ONLY)
		v |= BIT_I3C_SDR_EN | BIT_I3C_DDR_EN;
	result = inv_plat_single_write(st, REG_INTF_CONFIG1, v);
	if (result)
		return result;

	/* initialize DMP */
	v = BIT_DMP_SRAM_RESET_APEX;
#ifndef NOT_SET_DMP_POWER_SAVE
	v |= BIT_DMP_POWER_SAVE_EN;
#endif
	result = inv_plat_single_write(st, REG_APEX_CONFIG0, v);
	if (result)
		return result;
	usleep_range(1000, 1001);

	v = BIT_DMP_ODR_50HZ;
	result = inv_plat_single_write(st, REG_APEX_CONFIG1, v);
	if (result)
		return result;

	/* enable chip timestamp */
	v = BIT_TMST_EN;
	result = inv_mreg_single_write(st, REG_TMST_CONFIG1_MREG_TOP1, v);
	if (result)
		return result;

	/* INT pin configuration
	 * REG_INT_CONFIG (0x06) bit layout (DS-000398 p48):
	 *   [5]=INT2_MODE [4]=INT2_DRIVE_CIRCUIT [3]=INT2_POLARITY
	 *   [2]=INT1_MODE [1]=INT1_DRIVE_CIRCUIT [0]=INT1_POLARITY
	 * Both INT1 and INT2: push-pull, active-high, pulsed -> 0x1B.
	 * NOTE: the module wires the IRQ line from INT2; leaving INT2 in
	 * open-drain (reset default) with no board pull-up lets the line
	 * float and pick up noise -> IRQ storm / softlockup.
	 */
	BUILD_BUG_ON(INT_POLARITY != 1 || INT_DRIVE_CIRCUIT != 1 || INT_MODE != 0);
	v = (INT_MODE << 5) | (INT_DRIVE_CIRCUIT << 4) | (INT_POLARITY << 3) |
		(INT_MODE << 2) | (INT_DRIVE_CIRCUIT << 1) | (INT_POLARITY << 0);
	pr_info("inv_mpu: INT_CONFIG=0x%02x (push-pull, active-high, pulse)\n", v);
	result = inv_plat_single_write(st, REG_INT_CONFIG_REG, v);
	if (result)
		return result;

	/* disable FIFO */
	result = inv_plat_single_write(st, REG_FIFO_CONFIG1,
		BIT_FIFO_MODE_BYPASS);
	if (result)
		return result;
	result = inv_mreg_single_write(st, REG_FIFO_CONFIG5_MREG_TOP1, 0);
	if (result)
		return result;

	/* disable sensors */
	result = inv_plat_single_write(st, REG_PWR_MGMT_0, 0);
	if (result)
		return result;

	result = inv_init_config(st);
	if (result)
		return result;

	st->chip_config.lp_en_mode_off = 0;

	result = inv_set_power(st, false);

	pr_info("%s: initialize result is %d....\n", __func__, result);
	return 0;
}
