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
#define DEBUG
#define pr_fmt(fmt) "inv_mpu: " fmt

#include "../inv_mpu_iio.h"

#define DEF_ST_ACCEL_RESULT_SHIFT	1
#define SELF_TEST_ODR			6   /* 800Hz */
#define SELF_TEST_ODR_LP		7   /* 400Hz */
#define SELF_TEST_ACC_FS		3   /* +-2g */
#define SELF_TEST_GYR_FS		3   /* +-250dps */
#define SELF_TEST_ACC_BW_IND		BIT_ACC_FILT_BW_IND_180HZ
#define SELF_TEST_ACC_LPM_AVG		BIT_ACC_UI_AVG_IND_2X
#define SELF_TEST_GYR_BW_IND		BIT_GYR_UI_FLT_BW_180HZ
#define SELF_TEST_GYR_LPM_AVG		BIT_GYR_UI_AVG_IND_2X
#define SELF_TEST_PRECISION		1000
#define SELF_TEST_SAMPLE_NB		200

#define ST_OFF				0
#define ST_ON				1
#define SENS_LNM			0
#define SENS_LPM			1

#define RETRY_CNT_SELF_TEST_DMP		30
#define RETRY_WAIT_MS_SELF_TEST_DMP	100
#define SELF_TEST_GYRO_ST_LIM_DMP	7
#define SELF_TEST_ACCEL_ST_LIM_DMP	7
#define SELF_TEST_NUM_SAMPLE_DMP	0

struct recover_regs {
	/* Bank 0 */
	u8 pwr_mgmt_0;		/* REG_PWR_MGMT_0 */
	u8 int_source0;		/* REG_INT_SOURCE0 */
	u8 int_soruce1;		/* REG_INT_SOURCE1 */
	u8 gyro_config0;	/* REG_GYRO_CONFIG0 */
	u8 accel_config0;	/* REG_ACCEL_CONFIG0 */
	u8 gyro_config1;	/* REG_GYRO_CONFIG1 */
	u8 accel_config1;	/* REG_ACCEL_CONFIG1 */
	u8 fifo_config1;	/* REG_FIFO_CONFIG1 */
	u8 apex_config0;	/* REG_APEX_CONFIG0 */
	u8 apex_config1;	/* REG_APEX_CONFIG1 */
	/* MREG TOP1 */
	u8 fifo_config5;	/* REG_FIFO_CONFIG5_MREG_TOP1 */
	u8 int_source6;		/* REG_INT_SOURCE6_MREG_TOP1 */
	u8 st_config;		/* REG_ST_CONFIG_MREG_TOP1 */
	u8 selftest;		/* REG_SELFTEST_MREG_TOP1 */

	/* MREG TOP2 */
	u8 gos_user0;		/* REG_GOS_USER0_MREG_TOP1 */
	u8 gos_user1;		/* REG_GOS_USER1_MREG_TOP1 */
	u8 gos_user2;		/* REG_GOS_USER2_MREG_TOP1 */
	u8 gos_user3;		/* REG_GOS_USER3_MREG_TOP1 */
	u8 gos_user4;		/* REG_GOS_USER4_MREG_TOP1 */
	u8 gos_user5;		/* REG_GOS_USER5_MREG_TOP1 */
	u8 gos_user6;		/* REG_GOS_USER6_MREG_TOP1 */
	u8 gos_user7;		/* REG_GOS_USER7_MREG_TOP1 */
	u8 gos_user8;		/* REG_GOS_USER8_MREG_TOP1 */
};

static struct recover_regs save_regs;

static int inv_save_setting(struct inv_mpu_state *st)
{
	int result = 0;

	/* Bank 0 */
	result |= inv_plat_read(st, REG_PWR_MGMT_0, 1,
			&save_regs.pwr_mgmt_0);
	result |= inv_plat_read(st, REG_INT_SOURCE0, 1,
			&save_regs.int_source0);
	result |= inv_plat_read(st, REG_INT_SOURCE1, 1,
			&save_regs.int_soruce1);
	result |= inv_plat_read(st, REG_GYRO_CONFIG0, 1,
			&save_regs.gyro_config0);
	result |= inv_plat_read(st, REG_ACCEL_CONFIG0, 1,
			&save_regs.accel_config0);
	result |= inv_plat_read(st, REG_GYRO_CONFIG1, 1,
			&save_regs.gyro_config1);
	result |= inv_plat_read(st, REG_ACCEL_CONFIG1, 1,
			&save_regs.accel_config1);
	result |= inv_plat_read(st, REG_FIFO_CONFIG1, 1,
			&save_regs.fifo_config1);
	result |= inv_plat_read(st, REG_APEX_CONFIG0, 1,
			&save_regs.apex_config0);
	result |= inv_plat_read(st, REG_APEX_CONFIG1, 1,
			&save_regs.apex_config1);

	result |= inv_set_idle(st);

	/* MREG TOP1 */
	result |= inv_mreg_read(st, REG_FIFO_CONFIG5_MREG_TOP1, 1,
			&save_regs.fifo_config5);
	result |= inv_mreg_read(st, REG_INT_SOURCE6_MREG_TOP1, 1,
			&save_regs.int_source6);
	result |= inv_mreg_read(st, REG_ST_CONFIG_MREG_TOP1, 1,
			&save_regs.st_config);
	result |= inv_mreg_read(st, REG_SELFTEST_MREG_TOP1, 1,
			&save_regs.selftest);

	/* MREG TOP2 */
	result |= inv_mreg_read(st, REG_GOS_USER0_MREG_TOP1, 1,
			&save_regs.gos_user0);
	result |= inv_mreg_read(st, REG_GOS_USER1_MREG_TOP1, 1,
			&save_regs.gos_user1);
	result |= inv_mreg_read(st, REG_GOS_USER2_MREG_TOP1, 1,
			&save_regs.gos_user2);
	result |= inv_mreg_read(st, REG_GOS_USER3_MREG_TOP1, 1,
			&save_regs.gos_user3);
	result |= inv_mreg_read(st, REG_GOS_USER4_MREG_TOP1, 1,
			&save_regs.gos_user4);
	result |= inv_mreg_read(st, REG_GOS_USER5_MREG_TOP1, 1,
			&save_regs.gos_user5);
	result |= inv_mreg_read(st, REG_GOS_USER6_MREG_TOP1, 1,
			&save_regs.gos_user6);
	result |= inv_mreg_read(st, REG_GOS_USER7_MREG_TOP1, 1,
			&save_regs.gos_user7);
	result |= inv_mreg_read(st, REG_GOS_USER8_MREG_TOP1, 1,
			&save_regs.gos_user8);

	return result;
}

static int inv_recover_setting(struct inv_mpu_state *st)
{
	int result = 0;

	result |= inv_set_idle(st);

	/* MREG TOP1 */
	result |= inv_mreg_single_write(st, REG_FIFO_CONFIG5_MREG_TOP1,
			save_regs.fifo_config5);
	result |= inv_mreg_single_write(st, REG_INT_SOURCE6_MREG_TOP1,
			save_regs.int_source6);
	result |= inv_mreg_single_write(st, REG_ST_CONFIG_MREG_TOP1,
			save_regs.st_config);
	result |= inv_mreg_single_write(st, REG_SELFTEST_MREG_TOP1,
			save_regs.selftest);

	/* MREG TOP2 */
	result |= inv_mreg_single_write(st, REG_GOS_USER0_MREG_TOP1,
			save_regs.gos_user0);
	result |= inv_mreg_single_write(st, REG_GOS_USER1_MREG_TOP1,
			save_regs.gos_user1);
	result |= inv_mreg_single_write(st, REG_GOS_USER2_MREG_TOP1,
			save_regs.gos_user2);
	result |= inv_mreg_single_write(st, REG_GOS_USER3_MREG_TOP1,
			save_regs.gos_user3);
	result |= inv_mreg_single_write(st, REG_GOS_USER4_MREG_TOP1,
			save_regs.gos_user4);
	result |= inv_mreg_single_write(st, REG_GOS_USER5_MREG_TOP1,
			save_regs.gos_user5);
	result |= inv_mreg_single_write(st, REG_GOS_USER6_MREG_TOP1,
			save_regs.gos_user6);
	result |= inv_mreg_single_write(st, REG_GOS_USER7_MREG_TOP1,
			save_regs.gos_user7);
	result |= inv_mreg_single_write(st, REG_GOS_USER8_MREG_TOP1,
			save_regs.gos_user8);

	/* Bank 0 */
	result |= inv_plat_single_write(st, REG_INT_SOURCE0,
			save_regs.int_source0);
	result |= inv_plat_single_write(st, REG_INT_SOURCE1,
			save_regs.int_soruce1);
	result |= inv_plat_single_write(st, REG_GYRO_CONFIG0,
			save_regs.gyro_config0);
	result |= inv_plat_single_write(st, REG_ACCEL_CONFIG0,
			save_regs.accel_config0);
	result |= inv_plat_single_write(st, REG_GYRO_CONFIG1,
			save_regs.gyro_config1);
	result |= inv_plat_single_write(st, REG_ACCEL_CONFIG1,
			save_regs.accel_config1);
	result |= inv_plat_single_write(st, REG_FIFO_CONFIG1,
			save_regs.fifo_config1);
	result |= inv_plat_single_write(st, REG_APEX_CONFIG0,
			save_regs.apex_config0);
	result |= inv_plat_single_write(st, REG_PWR_MGMT_0,
			save_regs.pwr_mgmt_0);
	/* Note: do not restore REG_APEX_CONFIG1 intentionally */

	return result;
}

static int inv_reset_offset_reg(struct inv_mpu_state *st)
{
	int result = 0;

	result |= inv_mreg_single_write(st, REG_GOS_USER0_MREG_TOP1, 0);
	result |= inv_mreg_single_write(st, REG_GOS_USER1_MREG_TOP1, 0);
	result |= inv_mreg_single_write(st, REG_GOS_USER2_MREG_TOP1, 0);
	result |= inv_mreg_single_write(st, REG_GOS_USER3_MREG_TOP1, 0);
	result |= inv_mreg_single_write(st, REG_GOS_USER4_MREG_TOP1, 0);
	result |= inv_mreg_single_write(st, REG_GOS_USER5_MREG_TOP1, 0);
	result |= inv_mreg_single_write(st, REG_GOS_USER6_MREG_TOP1, 0);
	result |= inv_mreg_single_write(st, REG_GOS_USER7_MREG_TOP1, 0);
	result |= inv_mreg_single_write(st, REG_GOS_USER8_MREG_TOP1, 0);

	return result;
}

static int inv_init_selftest(struct inv_mpu_state *st)
{
	int result;

	/* disable sensors, and enable RC clock */
	result = inv_plat_single_write(st, REG_PWR_MGMT_0,
			BIT_IDLE | BIT_ACCEL_LP_CLK_SEL);
	if (result)
		return result;

	/* disable interrupts */
	result = inv_plat_single_write(st, REG_INT_SOURCE0, 0);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_INT_SOURCE1, 0);
	if (result)
		return result;
	result = inv_mreg_single_write(st, REG_INT_SOURCE6_MREG_TOP1, 0);
	if (result)
		return result;

	/* disabled all apex features */
	result = inv_plat_single_write(st, REG_APEX_CONFIG1, BIT_DMP_ODR_50HZ);

	/* reset offset registers */
	result = inv_reset_offset_reg(st);
	if (result)
		return result;

	/* stop FIFO */
	result = inv_plat_single_write(st, REG_FIFO_CONFIG1,
			BIT_FIFO_MODE_BYPASS);
	if (result)
		return result;
	result = inv_mreg_single_write(st, REG_FIFO_CONFIG5_MREG_TOP1, 0);

	return result;
}

static int inv_setup_sensors(struct inv_mpu_state *st,
	int sensor, int st_mode, int pwr_mode)
{
	int result;
	u8 val;

	/* disable sensors */
	val = BIT_IDLE | BIT_ACCEL_LP_CLK_SEL;
	result = inv_plat_single_write(st, REG_PWR_MGMT_0, val);

	/* self-test mode set */
	val = 0;
	if (st_mode == ST_ON) {
		if (sensor == SENSOR_ACCEL)
			val |= BIT_EN_AX_ST | BIT_EN_AY_ST | BIT_EN_AZ_ST;
		else if (sensor == SENSOR_GYRO)
			val |= BIT_EN_GX_ST | BIT_EN_GY_ST | BIT_EN_GZ_ST;
	}
	result = inv_mreg_single_write(st, REG_SELFTEST_MREG_TOP1, val);
	if (result)
		return result;

	/* set rate */
	if (pwr_mode == SENS_LPM) {
		result = inv_plat_single_write(st, REG_GYRO_CONFIG0,
				(SELF_TEST_GYR_FS << SHIFT_GYRO_FS_SEL) |
				(SELF_TEST_ODR_LP << SHIFT_ODR_CONF));
		if (result)
			return result;
		result = inv_plat_single_write(st, REG_ACCEL_CONFIG0,
				(SELF_TEST_ACC_FS << SHIFT_ACCEL_FS_SEL) |
				(SELF_TEST_ODR_LP << SHIFT_ODR_CONF));
		if (result)
			return result;
	} else {
		result = inv_plat_single_write(st, REG_GYRO_CONFIG0,
				(SELF_TEST_GYR_FS << SHIFT_GYRO_FS_SEL) |
				(SELF_TEST_ODR << SHIFT_ODR_CONF));
		if (result)
			return result;
		result = inv_plat_single_write(st, REG_ACCEL_CONFIG0,
				(SELF_TEST_ACC_FS << SHIFT_ACCEL_FS_SEL) |
				(SELF_TEST_ODR << SHIFT_ODR_CONF));
		if (result)
			return result;
	}

	/* set filter */
	result = inv_plat_single_write(st, REG_GYRO_CONFIG1,
		SELF_TEST_GYR_LPM_AVG | SELF_TEST_GYR_BW_IND);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG1,
		SELF_TEST_ACC_LPM_AVG | SELF_TEST_ACC_BW_IND);
	if (result)
		return result;

	/* turn on sensors */
	val = 0;
	if (sensor == SENSOR_ACCEL) {
		if (pwr_mode == SENS_LPM)
			val |= BIT_ACCEL_MODE_LPM;
		else
			val |= BIT_ACCEL_MODE_LNM;
	} else if (sensor == SENSOR_GYRO) {
		if (pwr_mode == SENS_LPM)
			val |= BIT_GYRO_MODE_LPM;
		else
			val |= BIT_GYRO_MODE_LNM;
	}
	val |= BIT_IDLE | BIT_ACCEL_LP_CLK_SEL;
	result = inv_plat_single_write(st, REG_PWR_MGMT_0, val);

	msleep(200);

	return result;
}

static int inv_calc_avg_with_samples(struct inv_mpu_state *st, int sensor,
		int avg[3], int count, int pwr_mode)
{
	int result = 0;
	int i, t;
	u8 data[6];
	u8 reg;
	s32 sum[3] = { 0 };

	if (sensor == SENSOR_ACCEL)
		reg = REG_ACCEL_DATA_X0_UI;
	else if (sensor == SENSOR_GYRO)
		reg = REG_GYRO_DATA_X0_UI;
	else
		return -EINVAL;

	for (i = 0; i < count; i++) {
		result = inv_plat_read(st, reg, 6, data);
		if (result)
			return result;

		/* convert 8-bit to 16-bit */
		for (t = 0; t < 3; t++)
			sum[t] += (s16)be16_to_cpup((__be16 *)(&data[t * 2]));

		if (pwr_mode == SENS_LPM)
			usleep_range(2500, 2501); /* 400Hz */
		else
			usleep_range(1250, 1251); /* 800Hz */
	}

	for (t = 0; t < 3; t++)
		avg[t] = (int)(sum[t] / count * SELF_TEST_PRECISION);

	return result;
}

static int inv_run_dmp_selftest(struct inv_mpu_state *st, int sensor)
{
	int result;
	int i;
	u8 val;
	bool done = false;
	bool pass = false;

	/* disable sensors, and enable RC clock */
	result = inv_plat_single_write(st, REG_PWR_MGMT_0,
			BIT_IDLE | BIT_ACCEL_LP_CLK_SEL);
	if (result)
		return result;

	/* initialize SRAM */
	result = inv_plat_single_write(st, REG_APEX_CONFIG0,
			BIT_DMP_SRAM_RESET_APEX);
	if (result)
		return result;

	usleep_range(1000, 1200);

	/* Reload self-test data from OTP */
	inv_mreg_single_write(st, REG_OTP_CONFIG_MREG_TOP1, BIT_OTP_COPY_ST_DATA);
	inv_mreg_single_write(st, REG_OTP_CTRL7_MREG_OTP, 0x04);
	usleep_range(200, 300);
	inv_mreg_single_write(st, REG_OTP_CTRL7_MREG_OTP, 0x04 | BIT_OTP_RELOAD);
	usleep_range(100, 200);
	inv_mreg_single_write(st, REG_OTP_CONFIG_MREG_TOP1, BIT_OTP_COPY_NORMAL);
	inv_mreg_single_write(st, REG_OTP_CTRL7_MREG_OTP, 0x04 | BIT_OTP_PWR_DOWN);
	usleep_range(200, 300);

	/* configure DMP self-test */
	result = inv_mreg_single_write(st, REG_ST_CONFIG_MREG_TOP1,
			(SELF_TEST_GYRO_ST_LIM_DMP << SHIFT_GYRO_ST_LIM) |
			(SELF_TEST_ACCEL_ST_LIM_DMP << SHIFT_ACCEL_ST_LIM) |
			(SELF_TEST_NUM_SAMPLE_DMP << SHIFT_ST_NUM_SAMPLE));
	if (result)
		return result;

	/* start DMP self-test */
	if (sensor == SENSOR_ACCEL)
		val = BIT_ACCEL_ST_EN;
	else if (sensor == SENSOR_GYRO)
		val = BIT_GYRO_ST_EN;
	else
		return -EINVAL;

	result = inv_mreg_single_write(st, REG_SELFTEST_MREG_TOP1, val);
	if (result)
		return result;

	/* wait for pass/fail result from DMP */
	for (i = 0; i < RETRY_CNT_SELF_TEST_DMP; i++) {
		msleep(RETRY_WAIT_MS_SELF_TEST_DMP);
		if (sensor == SENSOR_ACCEL)
			result = inv_mreg_read(st, REG_ST_STATUS1_MREG_TOP1,
					1, &val);
		else if (sensor == SENSOR_GYRO)
			result = inv_mreg_read(st, REG_ST_STATUS2_MREG_TOP1,
					1, &val);
		if (result)
			return result;

		pr_debug("st_status = 0x%02x\n", val);

		if (sensor == SENSOR_ACCEL) {
			if (val & BIT_DMP_ACCEL_ST_DONE) {
				if ((val & BIT_DMP_AX_ST_PASS) &&
					(val & BIT_DMP_AY_ST_PASS) &&
					(val & BIT_DMP_AZ_ST_PASS))
					pass = true;
				done = true;
				break;
			}
		} else if (sensor == SENSOR_GYRO) {
			if (val & BIT_DMP_GYRO_ST_DONE) {
				if ((val & BIT_DMP_GX_ST_PASS) &&
					(val & BIT_DMP_GY_ST_PASS) &&
					(val & BIT_DMP_GZ_ST_PASS))
					pass = true;
				done = true;
				break;
			}
		}
	}
	pr_debug("dmp status read cnt = %d\n", i);
	pr_info("sensor %d: done=%d, pass=%d\n", sensor, done, pass);

	/* disable DMP self-test */
	result = inv_mreg_single_write(st, REG_SELFTEST_MREG_TOP1, 0);
	if (result)
		return result;

	if (done && pass)
		result = 0;
	else
		result = -1;

	return result;
}

static int inv_do_selftest_dmp(struct inv_mpu_state *st, int sensor,
		int bias_lnm[3], int bias_lpm[3], bool lp_mode)
{
	u16 result;
	int i;
	int lsb[3] = { 0 };
	int lsb_lp[3] = { 0 };

	/* factory cal */
	result = inv_setup_sensors(st, sensor, ST_OFF, SENS_LNM);
	if (result)
		return result;
	result = inv_calc_avg_with_samples(st, sensor, lsb,
			SELF_TEST_SAMPLE_NB, SENS_LNM);
	if (result)
		return result;
	pr_info("sensor %d: LNM bias(LSB): %d, %d, %d\n",
			sensor, lsb[0], lsb[1], lsb[2]);

	if (lp_mode) {
		result = inv_setup_sensors(st, sensor, ST_OFF, SENS_LPM);
		if (result)
			return result;
		result = inv_calc_avg_with_samples(st, sensor, lsb_lp,
				SELF_TEST_SAMPLE_NB, SENS_LPM);
		if (result)
			return result;
		pr_info("sensor %d: LPM bias(LSB): %d, %d, %d\n",
				sensor, lsb_lp[0], lsb_lp[1], lsb_lp[2]);
	}

	/* set bias output values */
	for (i = 0; i < 3; i++) {
		bias_lnm[i] = lsb[i];
		bias_lpm[i] = lsb_lp[i];
	}

	/* self-test by DMP */
	result = inv_run_dmp_selftest(st, sensor);

	return result;
}

/*
 *  inv_hw_self_test() - main function to do hardware self test
 */
int inv_hw_self_test(struct inv_mpu_state *st)
{
	int result;
	int gyro_bias_lnm[3] = { 0 };
	int accel_bias_lnm[3] = { 0 };
	int gyro_bias_lpm[3] = { 0 };
	int accel_bias_lpm[3] = { 0 };
	int accel_result = 0;
	int gyro_result = 0;
	int i;

	/* save registers */
	/* idle bit will be set in this function */
	result = inv_save_setting(st);
	if (result)
		goto test_fail;

	/* initialization for self-test */
	result = inv_init_selftest(st);
	if (result)
		goto test_fail;

	/* accel self-test */
	pr_info("accel self-test\n");
	result = inv_do_selftest_dmp(st, SENSOR_ACCEL,
			accel_bias_lnm, accel_bias_lpm, true);
	if (result == 0) {
		/* pass */
		accel_result = 1;
		/* output LPM bias */
		for (i = 0; i < 3; i++)
			st->accel_st_bias[i] =
			    accel_bias_lpm[i] / SELF_TEST_PRECISION;
	}

	/* gyro self-test */
	pr_info("gyro self-test\n");
	result = inv_do_selftest_dmp(st, SENSOR_GYRO,
			gyro_bias_lnm, gyro_bias_lpm, false);
	if (result == 0) {
		/* pass */
		gyro_result = 1;
		/* output LNM bias */
		for (i = 0; i < 3; i++)
			st->gyro_st_bias[i] =
			    gyro_bias_lnm[i] / SELF_TEST_PRECISION;
	}

test_fail:
	/* initialize SRAM */
	result = inv_plat_single_write(st, REG_APEX_CONFIG0,
			BIT_DMP_SRAM_RESET_APEX);
	if (result)
		return result;

	usleep_range(1000, 1200);

	/* Note:
	 * Keep all APEX features disabled in APEX_CONFIG1 register
	 * to reconfigure the features because SRAM is reset
	 * in self-test process.
	 */
	/* restore registers */
	inv_recover_setting(st);
	st->apex_data.step_reset_last_val = true;

	return (accel_result << DEF_ST_ACCEL_RESULT_SHIFT) | gyro_result;
}
