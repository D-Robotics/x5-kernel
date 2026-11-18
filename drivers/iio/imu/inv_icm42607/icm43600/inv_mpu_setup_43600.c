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

static int inv_get_actual_duration(int rate)
{
	int duration_ns;

	if (rate > 400)
		duration_ns = 1250000;
	else if (rate > 200)
		duration_ns = 2500000;
	else if (rate > 100)
		duration_ns = 5000000;
	else if (rate > 50)
		duration_ns = 10000000;
	else if (rate > 25)
		duration_ns = 20000000;
	else if (rate > 12)
		duration_ns = 40000000;
	else if (rate > 6)
		duration_ns = 80000000;
	else if (rate > 3)
		duration_ns = 160000000;
	else
		duration_ns = 320000000;

	return duration_ns;
}

static int inv_calc_engine_dur(struct inv_mpu_state *st,
				struct inv_engine_info *ei)
{
	if (!ei->running_rate)
		return -EINVAL;
	ei->dur = ei->base_time / ei->orig_rate;
	ei->dur *= ei->divider;

	return 0;
}

int inv_turn_off_fifo(struct inv_mpu_state *st)
{
	int res;

	res = inv_plat_single_write(st,
		REG_FIFO_CONFIG1, BIT_FIFO_MODE_BYPASS);
	if (res)
		return res;
	res = inv_mreg_single_write(st, REG_FIFO_CONFIG5_MREG_TOP1, 0);

	return res;
}

int inv_turn_on_fifo(struct inv_mpu_state *st)
{
	uint8_t data[3];
	int r;
	u8 int_source0 = 0;
	u8 int_source1 = 0;
	u8 int_source6 = 0;
	u8 fifo_config5 = 0;
	u8 wom_config = 0;

	/* reset FIFO */
	r = inv_plat_single_write(st,
		REG_FIFO_CONFIG1, BIT_FIFO_MODE_BYPASS);
	if (r)
		return r;
	r = inv_mreg_single_write(st, REG_FIFO_CONFIG5_MREG_TOP1, 0);
	if (r)
		return r;
	r = inv_plat_read(st, REG_FIFO_BYTE_COUNT1, 2, data);
	if (r)
		return r;

	/* create register values */
	if (inv_get_apex_enabled(st)) {
		/* enable WOM as long as APEX feature is
		 * enabled to support DMP_POWER_SAVE_EN
		 */
		wom_config |= BIT_WOM_INT_MODE_AND | BIT_WOM_MODE_PREV | BIT_WOM_EN_ON;
	}

	if (st->gesture_only_on && (!st->batch.timeout)) {
		if (st->chip_config.stationary_detect_enable)
			st->gesture_int_count = STATIONARY_DELAY_THRESHOLD;
		else
			st->gesture_int_count = WOM_DELAY_THRESHOLD;

		r = inv_set_accel_intel(st);
		if (r)
			return r;
		int_source1 |= BIT_INT_WOM_XYZ_INT1_EN;
		wom_config |= BIT_WOM_INT_MODE_AND | BIT_WOM_MODE_PREV | BIT_WOM_EN_ON;
	}

	if (st->smd.on && st->smd_supported)
		int_source1 |= BIT_INT_SMD_INT1_EN;

	if (st->sensor[SENSOR_GYRO].on ||
		st->sensor[SENSOR_ACCEL].on) {
		if (st->batch.timeout) {
			if (!st->batch.fifo_wm_th)
				int_source0 |= BIT_INT_DRDY_INT_EN;
			else
				int_source0 |= BIT_INT_FIFO_THS_INT1_EN | BIT_INT_FIFO_FULL_INT1_EN;
		} else {
			int_source0 |= BIT_INT_DRDY_INT_EN;
			if (st->chip_config.eis_enable)
				int_source0 |= BIT_INT_FSYNC_INT1_EN;
		}
	}

	/* input mode streams via data registers: keep the FIFO bypassed */
	if (st->input_stream_on) {
		r = inv_plat_single_write(st, REG_INT_SOURCE0, int_source0);
		if (r)
			return r;
		if (int_source0 & BIT_INT_DRDY_INT_EN)
			r = inv_plat_single_write(st, REG_INT_SOURCE3,
					BIT_INT_DRDY_INT_EN);
		return r;
	}

	if (st->sensor[SENSOR_GYRO].on || st->sensor[SENSOR_ACCEL].on)
		fifo_config5 |= BIT_FIFO_ACCEL_EN | BIT_FIFO_GYRO_EN | BIT_WM_GT_TH;
	if (st->chip_config.high_res_mode)
		fifo_config5 |= BIT_FIFO_HIRES_EN;

	if (st->step_detector_l_on || st->step_detector_wake_l_on ||
			((st->step_counter_wake_l_on || st->step_counter_l_on)
			 && st->ped.int_mode))
		int_source6 |= BIT_INT_STEP_DET_INT1_EN;
	if (st->chip_config.tilt_enable)
		int_source6 |= BIT_INT_TLT_DET_INT1_EN;

	/* Update registers */
	r = inv_plat_single_write(st, REG_WOM_CONFIG, wom_config);
	if (r)
		return r;
	r = inv_mreg_single_write(st, REG_FIFO_CONFIG5_MREG_TOP1, fifo_config5);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_FIFO_CONFIG1, BIT_FIFO_MODE_NO_BYPASS);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_INT_SOURCE0, int_source0);
	if (r)
		return r;
	/* route DRDY to INT2 as well: module boards may wire the IRQ to INT2 */
	if (int_source0 & BIT_INT_DRDY_INT_EN) {
		r = inv_plat_single_write(st, REG_INT_SOURCE3, BIT_INT_DRDY_INT_EN);
		if (r)
			return r;
	}
	r = inv_plat_single_write(st, REG_INT_SOURCE1, int_source1);
	if (r)
		return r;
	r = inv_mreg_single_write(st, REG_INT_SOURCE6_MREG_TOP1, int_source6);

	return r;
}

/*
 *  inv_reset_fifo() - Reset FIFO related registers.
 */
int inv_reset_fifo(struct inv_mpu_state *st, bool turn_off)
{
	int r, i;
	struct inv_timestamp_algo *ts_algo = &st->ts_algo;
	int accel_rate, gyro_rate;

	r = inv_turn_on_fifo(st);
	if (r)
		return r;

	ts_algo->last_run_time = get_time_ns();
	ts_algo->reset_ts = ts_algo->last_run_time;

	accel_rate = 800 / st->eng_info[ENGINE_ACCEL].divider;
	gyro_rate = 800 / st->eng_info[ENGINE_GYRO].divider;

	if (accel_rate >= 800)
		ts_algo->first_drop_samples[SENSOR_ACCEL] =
			FIRST_DROP_SAMPLES_ACC_800HZ;
	else if (accel_rate >= 200)
		ts_algo->first_drop_samples[SENSOR_ACCEL] =
			FIRST_DROP_SAMPLES_ACC_200HZ;
	else
		ts_algo->first_drop_samples[SENSOR_ACCEL] = 1;

	if (gyro_rate >= 800)
		ts_algo->first_drop_samples[SENSOR_GYRO] =
			FIRST_DROP_SAMPLES_GYR_800HZ;
	else if (gyro_rate >= 200)
		ts_algo->first_drop_samples[SENSOR_GYRO] =
			FIRST_DROP_SAMPLES_GYR_200HZ;
	else
		ts_algo->first_drop_samples[SENSOR_GYRO] = 1;

	st->last_temp_comp_time = ts_algo->last_run_time;
	st->left_over_size = 0;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		st->sensor[i].calib_flag = 0;
		st->sensor[i].sample_calib = 0;
		st->sensor[i].time_calib = ts_algo->last_run_time;
	}

	ts_algo->calib_counter = 0;

	return 0;
}

static int inv_turn_on_engine(struct inv_mpu_state *st)
{
	u8 v, w;
	int r;
	unsigned int wait_ms;
	int accel_rate, gyro_rate;

	accel_rate = 800 / st->eng_info[ENGINE_ACCEL].divider;
	gyro_rate = 800 / st->eng_info[ENGINE_GYRO].divider;
	r = inv_plat_read(st, REG_PWR_MGMT_0, 1, &v);
	if (r)
		return r;
	w = v & ~(BIT_GYRO_MODE_LNM | BIT_ACCEL_MODE_LNM);
	if (st->chip_config.gyro_enable) {
		/* gyro support low noise mode only */
		w |= BIT_GYRO_MODE_LNM;
	}
	if (st->chip_config.accel_enable ||
		inv_get_apex_enabled(st)) {
#ifdef SUPPORT_ACCEL_LPM
		if (accel_rate > ACC_LPM_MAX_RATE)
			w |= BIT_ACCEL_MODE_LNM;
		else
			w |= BIT_ACCEL_MODE_LPM;
#else
		w |= BIT_ACCEL_MODE_LNM;
#endif
	}
	r = inv_plat_single_write(st, REG_PWR_MGMT_0, w);
	if (r)
		return r;
	usleep_range(1000, 1001);
	wait_ms = 0;
	if (st->chip_config.gyro_enable && !(v & BIT_GYRO_MODE_LNM))
		wait_ms = INV_ICM43600_GYRO_START_TIME;
	if ((v & BIT_GYRO_MODE_LNM) && !st->chip_config.gyro_enable) {
		if (wait_ms < INV_ICM43600_GYRO_STOP_TIME)
			wait_ms = INV_ICM43600_GYRO_STOP_TIME;
	}
	if ((st->chip_config.accel_enable || inv_get_apex_enabled(st)) &&
		!(v & BIT_ACCEL_MODE_LNM)) {
		if (wait_ms < INV_ICM43600_ACCEL_START_TIME)
			wait_ms = INV_ICM43600_ACCEL_START_TIME;
	}
	if (wait_ms)
		msleep(wait_ms);

	if (st->chip_config.has_compass) {
		if (st->chip_config.compass_enable)
			r = st->slave_compass->resume(st);
		else
			r = st->slave_compass->suspend(st);
		if (r)
			return r;
	}

	return 0;
}

static int inv_setup_dmp_rate(struct inv_mpu_state *st)
{
	int i;

	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			st->cntl |= st->sensor[i].output;
			st->sensor[i].dur =
				st->eng_info[st->sensor[i].engine_base].dur;
			st->sensor[i].div = 1;
		}
	}

	return 0;
}

static int inv_set_odr_and_filter(struct inv_mpu_state *st, int a_d, int g_d)
{
	int result;
	int accel_hz, gyro_hz;
	bool apex_only;
	u8 a_odr, a_lpm_filter, a_lnm_filter;
	u8 g_odr, g_lpm_filter, g_lnm_filter;
	u8 data;

	accel_hz = 800 / (a_d + 1);
	gyro_hz = 800 / (g_d + 1);

	/* accel ODR and filter */
	if (accel_hz > 400) {
		a_odr = BIT_SENSOR_ODR_800HZ;
		a_lnm_filter = BIT_ACC_FILT_BW_IND_180HZ;
		a_lpm_filter = BIT_ACC_UI_AVG_IND_2X;
	} else if (accel_hz > 200) {
		a_odr = BIT_SENSOR_ODR_400HZ;
		a_lnm_filter = BIT_ACC_FILT_BW_IND_180HZ;
		a_lpm_filter = BIT_ACC_UI_AVG_IND_8X;
	} else if (accel_hz > 100) {
		a_odr = BIT_SENSOR_ODR_200HZ;
		a_lnm_filter = BIT_ACC_FILT_BW_IND_121HZ;
		a_lpm_filter = BIT_ACC_UI_AVG_IND_16X;
	} else if (accel_hz > 50) {
		a_odr = BIT_SENSOR_ODR_100HZ;
		a_lnm_filter = BIT_ACC_FILT_BW_IND_53HZ;
		a_lpm_filter = BIT_ACC_UI_AVG_IND_32X;
	} else if (accel_hz > 25) {
		a_odr = BIT_SENSOR_ODR_50HZ;
		a_lnm_filter = BIT_ACC_FILT_BW_IND_25HZ;
		a_lpm_filter = BIT_ACC_UI_AVG_IND_64X;
	} else if (accel_hz > 12) {
		a_odr = BIT_SENSOR_ODR_25HZ;
		a_lnm_filter = BIT_ACC_FILT_BW_IND_16HZ;
		a_lpm_filter = BIT_ACC_UI_AVG_IND_64X;
	} else if (accel_hz > 6) {
		a_odr = BIT_SENSOR_ODR_12HZ;
		a_lnm_filter = BIT_ACC_FILT_BW_IND_16HZ;
		a_lpm_filter = BIT_ACC_UI_AVG_IND_64X;
	} else if (accel_hz > 3) {
		a_odr = BIT_SENSOR_ODR_6HZ;
		a_lnm_filter = BIT_ACC_FILT_BW_IND_16HZ;
		a_lpm_filter = BIT_ACC_UI_AVG_IND_64X;
	} else {
		a_odr = BIT_SENSOR_ODR_3HZ;
		a_lnm_filter = BIT_ACC_FILT_BW_IND_16HZ;
		a_lpm_filter = BIT_ACC_UI_AVG_IND_64X;
	}

	/* only apex is enabled */
	if (inv_get_apex_enabled(st) && !st->chip_config.accel_enable &&
			!st->gesture_only_on)
		apex_only = true;
	else
		apex_only = false;

	if (apex_only) {
		a_lnm_filter = BIT_ACC_FILT_BW_IND_180HZ;
		a_lpm_filter = BIT_ACC_UI_AVG_IND_2X;
	}

	/* gyro ODR and filter */
	if (gyro_hz > 400) {
		g_odr = BIT_SENSOR_ODR_800HZ;
		g_lnm_filter = BIT_GYR_UI_FLT_BW_180HZ;
		g_lpm_filter = BIT_GYR_UI_AVG_IND_2X;
	} else if (gyro_hz > 200) {
		g_odr = BIT_SENSOR_ODR_400HZ;
		g_lnm_filter = BIT_GYR_UI_FLT_BW_180HZ;
		g_lpm_filter = BIT_GYR_UI_AVG_IND_8X;
	} else if (gyro_hz > 100) {
		g_odr = BIT_SENSOR_ODR_200HZ;
		g_lnm_filter = BIT_GYR_UI_FLT_BW_121HZ;
		g_lpm_filter = BIT_GYR_UI_AVG_IND_16X;
	} else if (gyro_hz > 50) {
		g_odr = BIT_SENSOR_ODR_100HZ;
		g_lnm_filter = BIT_GYR_UI_FLT_BW_53HZ;
		g_lpm_filter = BIT_GYR_UI_AVG_IND_32X;
	} else if (gyro_hz > 25) {
		g_odr = BIT_SENSOR_ODR_50HZ;
		g_lnm_filter = BIT_GYR_UI_FLT_BW_25HZ;
		g_lpm_filter = BIT_GYR_UI_AVG_IND_64X;
	} else if (gyro_hz > 12) {
		g_odr = BIT_SENSOR_ODR_25HZ;
		g_lnm_filter = BIT_GYR_UI_FLT_BW_16HZ;
		g_lpm_filter = BIT_GYR_UI_AVG_IND_64X;
	} else if (gyro_hz > 6) {
		g_odr = BIT_SENSOR_ODR_12HZ;
		g_lnm_filter = BIT_GYR_UI_FLT_BW_16HZ;
		g_lpm_filter = BIT_GYR_UI_AVG_IND_64X;
	} else if (gyro_hz > 3) {
		g_odr = BIT_SENSOR_ODR_6HZ;
		g_lnm_filter = BIT_GYR_UI_FLT_BW_16HZ;
		g_lpm_filter = BIT_GYR_UI_AVG_IND_64X;
	} else {
		g_odr = BIT_SENSOR_ODR_3HZ;
		g_lnm_filter = BIT_GYR_UI_FLT_BW_16HZ;
		g_lpm_filter = BIT_GYR_UI_AVG_IND_64X;
	}

	/* update registers */
	data = ((3 - st->chip_config.accel_fs) << SHIFT_ACCEL_FS_SEL) | a_odr;
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG0, data);
	if (result)
		return result;

	data = ((3 - st->chip_config.fsr) << SHIFT_GYRO_FS_SEL) | g_odr;
	result = inv_plat_single_write(st, REG_GYRO_CONFIG0, data);
	if (result)
		return result;

	data = a_lpm_filter | a_lnm_filter;
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG1, data);
	if (result)
		return result;

	data = g_lpm_filter | g_lnm_filter;
	result = inv_plat_single_write(st, REG_GYRO_CONFIG1, data);

	return result;
}

static int inv_set_batch(struct inv_mpu_state *st)
{
	int res = 0;
	u32 w;
	u32 running_rate;

	if (st->sensor[SENSOR_ACCEL].on || st->sensor[SENSOR_GYRO].on) {
		if (st->chip_config.high_res_mode)
			st->batch.pk_size = 20;
		else
			st->batch.pk_size = 16;
	} else
		st->batch.pk_size = 0;
	if (st->sensor[SENSOR_GYRO].on && !st->sensor[SENSOR_ACCEL].on)
		running_rate = st->eng_info[ENGINE_GYRO].running_rate;
	else if (!st->sensor[SENSOR_GYRO].on && st->sensor[SENSOR_ACCEL].on)
		running_rate = st->eng_info[ENGINE_ACCEL].running_rate;
	else
		running_rate = st->eng_info[ENGINE_GYRO].running_rate;
	if (st->batch.timeout) {
		w = st->batch.timeout * running_rate
					* st->batch.pk_size / 1000;
		if (w > MAX_BATCH_FIFO_SIZE)
			w = MAX_BATCH_FIFO_SIZE;
	} else {
		w = 0;
	}
	st->batch.fifo_wm_th = w;
	pr_debug("running= %d, pksize=%d, to=%d w=%d\n",
		running_rate, st->batch.pk_size, st->batch.timeout, w);

	res = inv_plat_single_write(st, REG_FIFO_CONFIG2, w & 0xff);
	if (res)
		return res;
	res = inv_plat_single_write(st, REG_FIFO_CONFIG3, (w >> 8) & 0xff);

	return res;
}

static int inv_set_rate(struct inv_mpu_state *st)
{
	int g_d, a_d, result;

	result = inv_setup_dmp_rate(st);
	if (result)
		return result;

	g_d = st->eng_info[ENGINE_GYRO].divider - 1;
	a_d = st->eng_info[ENGINE_ACCEL].divider - 1;

	result = inv_set_odr_and_filter(st, a_d, g_d);
	if (result)
		return result;

	result = inv_set_batch(st);

	return result;
}

static int inv_enable_apex_gestures(struct inv_mpu_state *st)
{
	int result;
	u8 w, r;
	unsigned int wait_ms = 0;

	result = inv_plat_read(st, REG_APEX_CONFIG1, 1, &r);
	if (result)
		return result;

	w = BIT_DMP_ODR_50HZ;
	if (st->step_detector_l_on ||
		st->step_detector_wake_l_on ||
		st->step_counter_l_on ||
		st->step_counter_wake_l_on ||
		st->smd.on)
		w |= BIT_DMP_PEDO_EN;
	if (st->chip_config.tilt_enable)
		w |= BIT_DMP_TILT_EN;
	if (st->smd.on)
		w |= BIT_DMP_SMD_EN;

	if (r != w) {
		if (!(r & BIT_DMP_PEDO_EN) && (w & BIT_DMP_PEDO_EN)) {
			/* give DMP some time to push the latest step count
			 * to data register
			 */
			wait_ms = 50;
		}
		if (!(r & (BIT_DMP_PEDO_EN | BIT_DMP_TILT_EN | BIT_DMP_SMD_EN)) &&
			(w & (BIT_DMP_PEDO_EN | BIT_DMP_TILT_EN | BIT_DMP_SMD_EN))) {
			/* first time to enable apex features in DMP */
			result = inv_plat_single_write(st, REG_APEX_CONFIG0, BIT_DMP_INIT_EN);
			if (result)
				return result;
			msleep(50);
			result = inv_plat_single_write(st, REG_APEX_CONFIG1, w);
			if (result)
				return result;
			if (wait_ms)
				msleep(wait_ms);
#ifndef NOT_SET_DMP_POWER_SAVE
			result = inv_plat_single_write(st, REG_APEX_CONFIG0, BIT_DMP_POWER_SAVE_EN);
			if (result)
				return result;
#endif
		} else if (!(w & (BIT_DMP_PEDO_EN | BIT_DMP_TILT_EN | BIT_DMP_SMD_EN))) {
			/* disable all apex features in DMP */
			result = inv_plat_single_write(st, REG_APEX_CONFIG1, w);
			if (result)
				return result;
			result = inv_plat_single_write(st, REG_APEX_CONFIG0,
					BIT_DMP_SRAM_RESET_APEX | BIT_DMP_POWER_SAVE_EN);
			if (result)
				return result;
			usleep_range(1000, 1001);
		} else {
			/* update apex features in DMP, additionally enalbe or disable
			 * some but at least one is still enabled
			 */
			result = inv_plat_single_write(st, REG_APEX_CONFIG1, w);
			if (result)
				return result;
			if (wait_ms)
				msleep(wait_ms);
		}

		if (!(r & BIT_DMP_PEDO_EN) && (w & BIT_DMP_PEDO_EN))
			st->apex_data.step_reset_last_val = true;
		else
			st->apex_data.step_reset_last_val = false;
	}

	return result;
}

static int inv_determine_engine(struct inv_mpu_state *st)
{
	int i;
	bool a_en, g_en;
	int accel_rate, gyro_rate;

	a_en = false;
	g_en = false;
	gyro_rate = MPU_INIT_SENSOR_RATE_LNM;
#ifdef SUPPORT_ACCEL_LPM
	accel_rate = MPU_INIT_SENSOR_RATE_LPM;
#else
	accel_rate = MPU_INIT_SENSOR_RATE_LNM;
#endif
	/*
	 * loop the streaming sensors to see which engine needs to be turned on
	 */
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			a_en |= st->sensor[i].a_en;
			g_en |= st->sensor[i].g_en;
		}
	}

	if (st->chip_config.eis_enable) {
		g_en = true;
		st->eis.frame_count = 0;
		st->eis.fsync_delay = 0;
		st->eis.gyro_counter = 0;
		st->eis.voting_count = 0;
		st->eis.voting_count_sub = 0;
		gyro_rate = BASE_SAMPLE_RATE;
	} else {
		st->eis.eis_triggered = false;
		st->eis.prev_state = false;
	}

	accel_rate = st->sensor[SENSOR_ACCEL].rate;
	gyro_rate  = max(gyro_rate, st->sensor[SENSOR_GYRO].rate);

	if (g_en)
		st->ts_algo.clock_base = ENGINE_GYRO;
	else
		st->ts_algo.clock_base = ENGINE_ACCEL;

	st->eng_info[ENGINE_GYRO].running_rate = gyro_rate;
	st->eng_info[ENGINE_ACCEL].running_rate = accel_rate;

	if (st->chip_config.eis_enable) {
		st->eng_info[ENGINE_GYRO].divider = 1;
		st->eng_info[ENGINE_ACCEL].divider = 1;
		for (i = 0 ; i < SENSOR_L_NUM_MAX ; i++) {
			if (st->sensor_l[i].on) {
				st->sensor_l[i].counter = 0;
				if (st->sensor_l[i].rate)
					st->sensor_l[i].div =
					    ((BASE_SAMPLE_RATE /
						 st->eng_info[ENGINE_GYRO].divider) /
						 st->sensor_l[i].rate);
				else
					st->sensor_l[i].div = 0xffff;
			}
		}
	} else {
		st->eng_info[ENGINE_GYRO].divider =
			inv_get_actual_duration(st->eng_info[ENGINE_GYRO].running_rate) / 1250000;
		st->eng_info[ENGINE_ACCEL].divider =
			inv_get_actual_duration(st->eng_info[ENGINE_ACCEL].running_rate) / 1250000;
	}

	for (i = 0 ; i < SENSOR_L_NUM_MAX ; i++)
		st->sensor_l[i].counter = 0;

	inv_calc_engine_dur(st, &st->eng_info[ENGINE_GYRO]);
	inv_calc_engine_dur(st, &st->eng_info[ENGINE_ACCEL]);

	pr_debug("gen: %d aen: %d grate: %d arate: %d\n",
				g_en, a_en, gyro_rate, accel_rate);

	st->chip_config.gyro_enable = g_en;
	st->chip_config.accel_enable = a_en;

	return 0;
}

/*
 *  set_inv_enable() - enable function.
 */
int set_inv_enable(struct iio_dev *indio_dev)
{
	int result;
	struct inv_mpu_state *st = iio_priv(indio_dev);

	inv_set_idle(st);

	inv_stop_interrupt(st);
	inv_turn_off_fifo(st);
	inv_determine_engine(st);

	result = inv_set_rate(st);
	if (result) {
		pr_err("inv_set_rate error\n");
		return result;
	}

	result = inv_turn_on_engine(st);
	if (result) {
		pr_err("inv_turn_on_engine error\n");
		return result;
	}

	if (st->apex_supported) {
		result = inv_enable_apex_gestures(st);
		if (result) {
			pr_err("inv_enable_apex_gestures error\n");
			return result;
		}
	}

	result = inv_reset_fifo(st, false);
	if (result)
		return result;

	inv_reset_idle(st);

	if ((!st->chip_config.gyro_enable) &&
		(!st->chip_config.accel_enable) &&
		(!inv_get_apex_enabled(st))) {
		inv_set_power(st, false);
		return 0;
	}

	return result;
}

static int inv_save_interrupt_config(struct inv_mpu_state *st)
{
	int res;

	res = inv_plat_read(st, REG_INT_SOURCE0, 1, &st->int_en);
	if (res)
		return res;
	res = inv_plat_read(st, REG_INT_SOURCE1, 1, &st->int_en_2);
	if (res)
		return res;
	res = inv_mreg_read(st, REG_INT_SOURCE6_MREG_TOP1, 1, &st->int_en_6);

	return res;
}

int inv_stop_interrupt(struct inv_mpu_state *st)
{
	int res;

	res = inv_save_interrupt_config(st);
	if (res)
		return res;

	res = inv_plat_single_write(st, REG_INT_SOURCE0, 0);
	if (res)
		return res;
	res = inv_plat_single_write(st, REG_INT_SOURCE1, 0);
	if (res)
		return res;
	res = inv_mreg_single_write(st, REG_INT_SOURCE6_MREG_TOP1, 0);

	return res;
}

int inv_restore_interrupt(struct inv_mpu_state *st)
{
	int res;

	res = inv_plat_single_write(st, REG_INT_SOURCE0, st->int_en);
	if (res)
		return res;
	res = inv_plat_single_write(st, REG_INT_SOURCE1, st->int_en_2);
	if (res)
		return res;
	res = inv_mreg_single_write(st, REG_INT_SOURCE6_MREG_TOP1, st->int_en_6);

	return res;
}

int inv_stop_stream_interrupt(struct inv_mpu_state *st)
{
	int res;

	res = inv_save_interrupt_config(st);
	if (res)
		return res;

	res = inv_plat_single_write(st, REG_INT_SOURCE0, 0);

	return res;
}

int inv_restore_stream_interrupt(struct inv_mpu_state *st)
{
	int res;

	res = inv_plat_single_write(st, REG_INT_SOURCE0, st->int_en);

	return res;
}

/* dummy function for 20608D */
int inv_enable_pedometer_interrupt(struct inv_mpu_state *st, bool en)
{
	return 0;
}

int inv_dmp_read(struct inv_mpu_state *st, int off, int size, u8 *buf)
{
	return 0;
}

int inv_firmware_load(struct inv_mpu_state *st)
{
	return 0;
}
