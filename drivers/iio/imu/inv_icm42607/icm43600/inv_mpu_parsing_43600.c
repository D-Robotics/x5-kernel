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
#include <linux/math64.h>

#include "../inv_mpu_iio.h"

static int inv_check_fsync(struct inv_mpu_state *st)
{
	u8 data[1];

	if (!st->chip_config.eis_enable)
		return 0;
	inv_plat_read(st, REG_INT_STATUS, 1, data);
	if (data[0] & BIT_INT_STATUS_FSYNC) {
		pr_debug("fsync\n");
		st->eis.eis_triggered = true;
		st->eis.fsync_delay = 1;
		st->eis.prev_state = 1;
		st->eis.frame_count++;
		st->eis.eis_frame = true;
	}
	st->header_count--;

	return 0;
}

static int inv_push_sensor(struct iio_dev *indio_dev, int ind, u64 t, s32 *raw)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	s32 calib[3] = { 0, 0, 0 };

	switch (ind) {
	case SENSOR_ACCEL:
		memcpy(st->last_raw_accel, raw, sizeof(st->last_raw_accel));
		st->last_raw_valid = true;
		inv_push_16bytes_buffer(indio_dev, ind, t, raw, 0);
		break;
	case SENSOR_TEMP:
		inv_check_fsync(st);
		break;
	case SENSOR_GYRO:
		memcpy(st->last_raw_gyro, raw, sizeof(st->last_raw_gyro));
		st->last_raw_valid = true;
		inv_push_gyro_data(indio_dev, raw, calib, t);
		break;
	default:
		break;
	}

	return 0;
}

static bool inv_validate_fifo_data(s16 *lsb)
{
	bool ret = true;

	if ((lsb[0] == -32768) && (lsb[1] == -32768) && (lsb[2] == -32768))
		ret = false;

	return ret;
}

static int inv_push_43600_data(struct iio_dev *indio_dev, u8 *d)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	icm4x6xx_fifo_header_t header;
	s16 acc_lsb[3] = { 0, 0, 0 };
	s16 gyr_lsb[3] = { 0, 0, 0 };
	s32 acc[3], gyr[3];
	bool valid_acc = false;
	bool valid_gyr = false;

	header.Byte = *d;

	/* skip 1 byte header */
	if (header.bits.msg_bit)
		return 0;

	if (header.bits.accel_bit && !header.bits.gyro_bit) {
		/* only accel */
		acc_lsb[0] = (s16)be16_to_cpup((__be16 *) (&d[1]));
		acc_lsb[1] = (s16)be16_to_cpup((__be16 *) (&d[3]));
		acc_lsb[2] = (s16)be16_to_cpup((__be16 *) (&d[5]));
		valid_acc = inv_validate_fifo_data(acc_lsb);
	} else if (!header.bits.accel_bit && header.bits.gyro_bit) {
		/* only gyro */
		gyr_lsb[0] = (s16)be16_to_cpup((__be16 *) (&d[1]));
		gyr_lsb[1] = (s16)be16_to_cpup((__be16 *) (&d[3]));
		gyr_lsb[2] = (s16)be16_to_cpup((__be16 *) (&d[5]));
		valid_gyr = inv_validate_fifo_data(gyr_lsb);
	} else if ((header.bits.accel_bit && header.bits.gyro_bit) ||
			header.bits.twentybits_bit) {
		/* accel and gyro */
		acc_lsb[0] = (s16)be16_to_cpup((__be16 *) (&d[1]));
		acc_lsb[1] = (s16)be16_to_cpup((__be16 *) (&d[3]));
		acc_lsb[2] = (s16)be16_to_cpup((__be16 *) (&d[5]));
		gyr_lsb[0] = (s16)be16_to_cpup((__be16 *) (&d[7]));
		gyr_lsb[1] = (s16)be16_to_cpup((__be16 *) (&d[9]));
		gyr_lsb[2] = (s16)be16_to_cpup((__be16 *) (&d[11]));
		valid_acc = inv_validate_fifo_data(acc_lsb);
		valid_gyr = inv_validate_fifo_data(gyr_lsb);
	} else
		return 0;

	if (header.bits.twentybits_bit) {
		/* high resolution mode */
		acc[0] = (acc_lsb[0] << 4) | ((d[17] >> 4) & 0xf);
		acc[1] = (acc_lsb[1] << 4) | ((d[18] >> 4) & 0xf);
		acc[2] = (acc_lsb[2] << 4) | ((d[19] >> 4) & 0xf);
		gyr[0] = (gyr_lsb[0] << 4) | (d[17] & 0xf);
		gyr[1] = (gyr_lsb[1] << 4) | (d[18] & 0xf);
		gyr[2] = (gyr_lsb[2] << 4) | (d[19] & 0xf);
	} else {
		/* normal resolution mode */
		acc[0] = acc_lsb[0];
		acc[1] = acc_lsb[1];
		acc[2] = acc_lsb[2];
		gyr[0] = gyr_lsb[0];
		gyr[1] = gyr_lsb[1];
		gyr[2] = gyr_lsb[2];
	}

	/* send accel data */
	if (header.bits.accel_bit) {
		if (st->sensor[SENSOR_ACCEL].on && valid_acc) {
			inv_get_dmp_ts(st, SENSOR_ACCEL);
			if (st->sensor[SENSOR_ACCEL].send &&
					(!st->ts_algo.first_drop_samples[SENSOR_ACCEL])) {
				st->sensor[SENSOR_ACCEL].sample_calib++;
				inv_push_sensor(indio_dev, SENSOR_ACCEL,
						st->sensor[SENSOR_ACCEL].ts, acc);
			}
			if (st->ts_algo.first_drop_samples[SENSOR_ACCEL])
				st->ts_algo.first_drop_samples[SENSOR_ACCEL]--;
		}
	}

	/* send gyro data */
	if (header.bits.gyro_bit) {
		if (st->sensor[SENSOR_GYRO].on && valid_gyr) {
			inv_get_dmp_ts(st, SENSOR_GYRO);
			if (st->sensor[SENSOR_GYRO].send &&
					(!st->ts_algo.first_drop_samples[SENSOR_GYRO])) {
				st->sensor[SENSOR_GYRO].sample_calib++;
				inv_push_sensor(indio_dev, SENSOR_GYRO,
						st->sensor[SENSOR_GYRO].ts, gyr);
			}
			if (st->ts_algo.first_drop_samples[SENSOR_GYRO])
				st->ts_algo.first_drop_samples[SENSOR_GYRO]--;
		}
	}

	st->header_count--;

	return 0;
}

static int inv_prescan_fifo_data(struct inv_mpu_state *st, u8 *data, int len)
{
	int i;
	u8 *dptr;
	icm4x6xx_fifo_header_t header;

	dptr = data;
	while (dptr < (data + len)) {
		if (st->batch.pk_size >= 8)
			pr_debug("FIFO 0x00: %x %x %x %x %x %x %x %x\n",
				 dptr[0], dptr[1], dptr[2], dptr[3],
				 dptr[4], dptr[5], dptr[6], dptr[7]);
		if (st->batch.pk_size >= 16)
			pr_debug("FIFO 0x08: %x %x %x %x %x %x %x %x\n",
				 dptr[8], dptr[9], dptr[10], dptr[11],
				 dptr[12], dptr[13], dptr[14], dptr[15]);
		if (st->batch.pk_size >= 20)
			pr_debug("FIFO 0x10: %x %x %x %x\n",
				 dptr[16], dptr[17], dptr[18], dptr[19]);
		dptr += st->batch.pk_size;
	}

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].count = 0;

	dptr = data;
	while (dptr < (data + len)) {
		/* count the number of valid samples
		 * in the buffer for accel and gyro
		 */
		header.Byte = *dptr;
		if (header.bits.msg_bit) {
			pr_warn("Unexpected FIFO msg bit\n");
			return 0;
		}
		/* header */
		dptr++;
		/* accel */
		if (header.bits.accel_bit) {
			st->sensor[SENSOR_ACCEL].count++;
			dptr += st->sensor[SENSOR_ACCEL].sample_size;
		}
		/* gyro */
		if (header.bits.gyro_bit) {
			st->sensor[SENSOR_GYRO].count++;
			dptr += st->sensor[SENSOR_GYRO].sample_size;
		}
		/* temperature */
		if (header.bits.twentybits_bit)
			dptr += 2;
		else
			dptr += 1;
		/* timestamp */
		if ((header.bits.accel_bit && header.bits.gyro_bit) ||
				header.bits.twentybits_bit)
			dptr += 2;
		/* high resolution data */
		if (header.bits.twentybits_bit)
			dptr += 3;
	}

	return 0;
}

static int inv_process_43600_data(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int total_bytes, tmp, res, fifo_count, pk_size, i;
	u8 *dptr, *d;
	u8 int_status2;
	u8 data;
	bool done_flag;
	u8 reg_pwr_mgmt_0;
	bool idle_set = false;

	/* check INT_STATUS2 */
	res = inv_plat_read(st, REG_INT_STATUS2, 1, &int_status2);
	if (res)
		return res;

	/* SMD */
	if (st->smd.on && (int_status2 & BIT_INT_STATUS_SMD)) {
		sysfs_notify(&indio_dev->dev.kobj, NULL, "poll_smd");
		st->smd.on = false;
		st->trigger_state = EVENT_TRIGGER;
		inv_check_sensor_on(st);
		res = set_inv_enable(indio_dev);
		if (res)
			return res;
		st->wake_sensor_received = true;
	}

	/* WOM */
	if (st->gesture_only_on && (!st->batch.timeout)) {
		pr_debug("ges cnt=%d, statu=%x\n",
			st->gesture_int_count, int_status2);
		if (int_status2 & BIT_INT_STATUS_WOM_XYZ) {
			if (!st->gesture_int_count) {
				res = inv_plat_read(st,
					REG_INT_SOURCE0, 1, &data);
				if (res)
					return res;
				data |= BIT_INT_DRDY_INT_EN;
				res = inv_plat_single_write(st,
					REG_INT_SOURCE0, data);
				if (res)
					return res;
				data = 0;
				if (st->chip_config.gyro_enable ||
					st->chip_config.accel_enable)
					data |= (BIT_FIFO_ACCEL_EN |
						BIT_FIFO_GYRO_EN |
						BIT_WM_GT_TH);
				res = inv_mreg_single_write(st,
					REG_FIFO_CONFIG5_MREG_TOP1, data);
				if (res)
					return res;
				if (data) {
					res = inv_plat_single_write(st,
						REG_FIFO_CONFIG1, BIT_FIFO_MODE_NO_BYPASS);
					if (res)
						return res;
				}
				/*
				 * First time wake up from WOM
				 * we don't need data in the FIFO
				 */
				res = inv_reset_fifo(st, true);
				if (res)
					return res;
				if (st->chip_config.stationary_detect_enable) {
					st->gesture_int_count =
						STATIONARY_DELAY_THRESHOLD;
				} else {
					st->gesture_int_count =
						WOM_DELAY_THRESHOLD;
				}
				return res;
			}
			if (st->chip_config.stationary_detect_enable) {
				st->gesture_int_count =
					STATIONARY_DELAY_THRESHOLD;
			} else {
				st->gesture_int_count =
					WOM_DELAY_THRESHOLD;
			}
		} else {
			if (!st->gesture_int_count) {
				res = inv_plat_single_write(st,
						REG_FIFO_CONFIG1, BIT_FIFO_MODE_BYPASS);
				if (res)
					return res;
				res = inv_mreg_single_write(st,
					REG_FIFO_CONFIG5_MREG_TOP1, 0);
				if (res)
					return res;
				res = inv_plat_read(st,
					REG_INT_SOURCE0, 1, &data);
				if (res)
					return res;
				data &= ~BIT_INT_DRDY_INT_EN;
				res = inv_plat_single_write(st,
					REG_INT_SOURCE0, data);
				if (res)
					return res;

				return res;
			}
			st->gesture_int_count--;
			if (st->gesture_int_count < 0)
				st->gesture_int_count = 0;
		}
	}

	fifo_count = inv_get_last_run_time_non_dmp_record_mode(st);
	pr_debug("fifc= %d\n", fifo_count);
	if (!fifo_count) {
		pr_debug("REG_FIFO_COUNT_H size is 0\n");
		return 0;
	}
	pk_size = st->batch.pk_size;
	if (!pk_size)
		return -EINVAL;

	if (fifo_count >= (HARDWARE_FIFO_SIZE / st->batch.pk_size)) {
		pr_warn("fifo overflow pkt count=%d pkt sz=%d\n",
			fifo_count, st->batch.pk_size);
		return -EOVERFLOW;
	}

	fifo_count *= st->batch.pk_size;
	st->fifo_count = fifo_count;
	d = st->fifo_data_store;
	dptr = d;
	total_bytes = fifo_count;

	if (total_bytes > pk_size * MAX_FIFO_PACKET_READ) {
		/* set Idle bit when FIFO read will be
		 * more than one transaction
		 */
		res = inv_plat_read(st, REG_PWR_MGMT_0, 1, &reg_pwr_mgmt_0);
		if (res)
			return res;
		res = inv_plat_single_write(st, REG_PWR_MGMT_0,
				reg_pwr_mgmt_0 | BIT_IDLE | BIT_ACCEL_LP_CLK_SEL);
		if (res)
			return res;
		usleep_range(INV_ICM43600_MCLK_WAIT_US,
				INV_ICM43600_MCLK_WAIT_US + 1);
		idle_set = true;
	}

	while (total_bytes > 0) {
		if (total_bytes < pk_size * MAX_FIFO_PACKET_READ)
			tmp = total_bytes;
		else
			tmp = pk_size * MAX_FIFO_PACKET_READ;
		res = inv_plat_read(st, REG_FIFO_DATA_REG, tmp, dptr);
		if (res < 0) {
			pr_err("read REG_FIFO_R_W is failed\n");
			return res;
		}
		dptr += tmp;
		total_bytes -= tmp;
	}

	if (idle_set) {
		/* reset Idle bit */
		res = inv_plat_single_write(st, REG_PWR_MGMT_0, reg_pwr_mgmt_0);
		if (res)
			return res;
	}

	dptr = d;
	total_bytes = fifo_count;

	inv_prescan_fifo_data(st, dptr, total_bytes);

	st->header_count = 0;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on)
			st->header_count = max(st->header_count,
							st->sensor[i].count);
	}

	st->ts_algo.calib_counter++;
	inv_bound_timestamp(st);

	dptr = d;
	done_flag = false;

	while (!done_flag) {
		pr_debug("total=%d, pk=%d\n", total_bytes, pk_size);
		if (total_bytes >= pk_size) {
			res = inv_push_43600_data(indio_dev, dptr);
			if (res)
				return res;
			total_bytes -= pk_size;
			dptr += pk_size;
		} else {
			done_flag = true;
		}
	}

	return 0;
}

int inv_get_43600_pedometer_steps(struct inv_mpu_state *st,
	int *ped, int *update)
{
	u8 r[2];
	uint32_t cur_step_cnt;
	int result;

	*update = false;
	result = inv_plat_read(st,
		REG_APEX_DATA0, 2, &r[0]);
	if (result)
		return result;

	cur_step_cnt = (uint32_t)((r[1] << 8) | r[0]);
	if (st->apex_data.step_reset_last_val) {
		st->apex_data.step_reset_last_val = false;
		st->apex_data.step_cnt_last_val = cur_step_cnt;
	}
	if (cur_step_cnt !=
		st->apex_data.step_cnt_last_val) {
		if (cur_step_cnt < st->apex_data.step_cnt_last_val) {
			/* overflow */
			st->apex_data.step_cnt_total +=
				cur_step_cnt + (0xFFFF - st->apex_data.step_cnt_last_val);
		} else {
			st->apex_data.step_cnt_total +=
				cur_step_cnt - st->apex_data.step_cnt_last_val;
		}
	}
	st->apex_data.step_cnt_last_val = cur_step_cnt;
	*ped = st->apex_data.step_cnt_total;
	*update = true;

	return result;
}

static int inv_process_apex_gesture(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	u8 int_status3;
	s16 s[3] = {0,};
	u64 t;
	uint32_t cur_step_cnt;
	int ped_update;
	int result;

	if (inv_get_apex_enabled(st)) {
		t = get_time_ns();

		/* check INT_STATUS3 */
		result = inv_plat_read(st, REG_INT_STATUS3, 1, &int_status3);
		if (result)
			return result;

		/* Tilt */
		if (int_status3 & BIT_INT_STATUS_TILT_DET) {
			if (st->chip_config.tilt_enable) {
				sysfs_notify(&indio_dev->dev.kobj, NULL, "poll_tilt");
				st->wake_sensor_received = true;
			}
		}

		/* Step detector */
		if (int_status3 & BIT_INT_STATUS_STEP_DET) {
			if (st->step_detector_l_on) {
				inv_push_8bytes_buffer(indio_dev,
					STEP_DETECTOR_HDR, t, s);
			}
			if (st->step_detector_wake_l_on) {
				inv_push_8bytes_buffer(indio_dev,
					STEP_DETECTOR_WAKE_HDR, t, s);
				st->wake_sensor_received = true;
			}
			if (st->ped.int_mode &&
				(st->step_counter_l_on || st->step_counter_wake_l_on)) {
				result = inv_get_43600_pedometer_steps(st,
					&cur_step_cnt, &ped_update);
				if (result)
					return result;
				if (ped_update)
					inv_send_steps(indio_dev, st->apex_data.step_cnt_total, t);
				if (st->step_counter_wake_l_on)
					st->wake_sensor_received = true;
			}
		}
	}
	return 0;
}

/*
 *  inv_read_fifo() - Transfer data from FIFO to ring buffer.
 */
irqreturn_t inv_read_fifo(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	result = wait_event_interruptible_timeout(st->wait_queue,
					st->resume_state, msecs_to_jiffies(300));
	if (result <= 0)
		goto exit_handled;

	mutex_lock(&st->lock);
	st->wake_sensor_received = false;

	if (st->apex_supported) {
		result = inv_process_apex_gesture(indio_dev);
		if (result)
			goto err_reset_fifo;
	}

	result = inv_process_43600_data(indio_dev);
	if (result)
		goto err_reset_fifo;
	mutex_unlock(&st->lock);

	if (st->wake_sensor_received)
#ifdef CONFIG_HAS_WAKELOCK
		wake_lock_timeout(&st->wake_lock, msecs_to_jiffies(200));
#else
		__pm_wakeup_event(st->wake_lock, 200); /* 200 msecs */
#endif
	goto exit_handled;

err_reset_fifo:
	if ((!st->chip_config.gyro_enable) &&
		(!st->chip_config.accel_enable) &&
		(!st->chip_config.slave_enable) &&
		(!st->chip_config.pressure_enable) &&
		(!inv_get_apex_enabled(st))) {
		mutex_unlock(&st->lock);
		goto exit_handled;
	}

	pr_err("error to reset fifo\n");
	inv_reset_fifo(st, true);
	mutex_unlock(&st->lock);

exit_handled:
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

int inv_flush_batch_data(struct iio_dev *indio_dev, int data)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);

	if (st->chip_config.gyro_enable ||
		st->chip_config.accel_enable ||
		st->chip_config.slave_enable ||
		st->chip_config.pressure_enable ||
		inv_get_apex_enabled(st)) {
		st->wake_sensor_received = false;
		inv_process_43600_data(indio_dev);
		if (st->wake_sensor_received)
#ifdef CONFIG_HAS_WAKELOCK
			wake_lock_timeout(&st->wake_lock, msecs_to_jiffies(200));
#else
			__pm_wakeup_event(st->wake_lock, 200); /* 200 msecs */
#endif
	}
	inv_push_marker_to_buffer(indio_dev, END_MARKER, data);

	return 0;
}
