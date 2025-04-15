/**
 * @section LICENSE
 * Copyright (c) 2017-2024 Bosch Sensortec GmbH All Rights Reserved.
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @file		bmi08x_iio_buffer.c
 * @date		2024-10-10
 * @version		v1.4.1
 *
 * @brief		BMA5xy Linux Driver IIO Buffer Test Source
 *
 */
#include <linux/version.h>

#include "bmi08x_driver.h"
#include "bs_log.h"

static u8 sensor_type = 2;

/**
 * bmi088_iio_trigger_h() - the trigger handler function
 * @irq: the interrupt number
 * @p: private data - always a pointer to the poll func.
 *
 */
static irqreturn_t bmi088_iio_trigger_h(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	u16 *data;
	int i, j;
	int rslt;
	struct bmi08_client_data *client_data = iio_priv(indio_dev);
	struct bmi08_sensor_data accel_data = { 0 };
	struct bmi08_sensor_data gyro_data = { 0 };

	data = kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (!data)
		goto done;

	if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength)) {
		for (i = 0, j = 0;
		     i < bitmap_weight(indio_dev->active_scan_mask,
				       indio_dev->masklength);
		     i++, j++) {
			j = find_next_bit(indio_dev->active_scan_mask,
					  indio_dev->masklength, j);
			if (sensor_type == 0 || sensor_type == 2) {
				rslt = bmi08a_get_data(&accel_data, &client_data->device);
				if (rslt)
					PERR("Failed to get accel sensor data %d", rslt);
				data[0] = accel_data.x;
				data[1] = accel_data.y;
				data[2] = accel_data.z;
			}
			if (sensor_type == 1 || sensor_type == 2) {
				rslt = bmi08g_get_data(&gyro_data, &client_data->device);
				if (rslt)
					PERR("Failed to get gyro sensor data %d", rslt);
				if (sensor_type == 1) {
					data[0] = gyro_data.x;
					data[1] = gyro_data.y;
					data[2] = gyro_data.z;
				} else {
					data[3] = gyro_data.x;
					data[4] = gyro_data.y;
					data[5] = gyro_data.z;
				}
			}
		}
	}
	/*lint -e534*/
	iio_push_to_buffers_with_timestamp(indio_dev, data,
					   iio_get_time_ns(indio_dev));
	/*lint +e534*/
	kfree(data);
done:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}
static const struct iio_buffer_setup_ops iio_bmi088_buffer_setup_ops = {
};
/**
 * bmi08x_iio_configure_buffer() - register buffer resources
 * @indo_dev: device instance state
 */
int bmi08x_iio_configure_buffer(struct iio_dev *indio_dev)
{
	int ret;
	struct iio_buffer *buffer;

	buffer = iio_kfifo_allocate();
	if (!buffer) {
		ret = -ENOMEM;
		goto error_ret;
	}

	iio_device_attach_buffer(indio_dev, buffer);

	indio_dev->setup_ops = &iio_bmi088_buffer_setup_ops;
// #if defined(BMI08_KERNEL_5_15) || defined(BMI08_KERNEL_6_1)
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 15, 0) || LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	indio_dev->pollfunc = iio_alloc_pollfunc(NULL,
						 bmi088_iio_trigger_h,
						 IRQF_ONESHOT,
						 indio_dev,
						 "%s-dev%d", indio_dev->name,
						 iio_device_id(indio_dev));
#else
	indio_dev->pollfunc = iio_alloc_pollfunc(NULL,
						 bmi088_iio_trigger_h,
						 IRQF_ONESHOT,
						 indio_dev,
						 "%s-dev%d", indio_dev->name,
						 indio_dev->id);
#endif
	if (!indio_dev->pollfunc) {
		ret = -ENOMEM;
		goto error_free_buffer;
	}

	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	return 0;

error_free_buffer:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
	iio_kfifo_free(indio_dev->buffer);
	return ret;
}

/**
 * bmi08x_iio_unconfigure_buffer() - release buffer resources
 * @indo_dev: device instance state
 */
void bmi08x_iio_unconfigure_buffer(struct iio_dev *indio_dev)
{
	iio_dealloc_pollfunc(indio_dev->pollfunc);
	iio_kfifo_free(indio_dev->buffer);
	indio_dev->buffer = NULL;
}

static enum hrtimer_restart iio_hrtimer_trig_handler(struct hrtimer *timer)
{
	struct iio_hrtimer_info *info;
	/*lint -e26 -e10 -e516 -e124*/
	info = container_of(timer, struct iio_hrtimer_info, timer);
	/*lint +e26  +e10 +e516 +e124*/
	/*lint -e534*/
	hrtimer_forward_now(timer, info->period);
	/*lint +e534*/
	iio_trigger_poll(info->swt.trigger);

	return HRTIMER_RESTART;
}

int iio_trig_hrtimer_set_state(struct iio_trigger *trig, bool state)
{
	struct iio_hrtimer_info *trig_info;

	trig_info = iio_trigger_get_drvdata(trig);
	if (state)
		hrtimer_start(&trig_info->timer, trig_info->period,
								HRTIMER_MODE_REL_HARD);
	else
		/*lint -e534*/
		hrtimer_cancel(&trig_info->timer);
		/*lint +e534*/

	return 0;
}

static const struct iio_trigger_ops iio_hrtimer_trigger_ops = {
	/*lint -e546*/
	.set_trigger_state = iio_trig_hrtimer_set_state,
	/*lint +e546*/
};
/**
 * bmi08x_iio_allocate_trigger() - register trigger resources
 * @indo_dev: device instance state
 */
int bmi08x_iio_allocate_trigger(struct iio_dev *indio_dev, u8 type)
{
	struct bmi08_client_data *sdata = iio_priv(indio_dev);
	int ret = 0;

	if (!sdata->trig_info) {
		sdata->trig_info = kzalloc(sizeof(*sdata->trig_info), GFP_KERNEL);
		if (!sdata->trig_info)
			return -ENOMEM;
// #if defined(BMI08_KERNEL_5_15) || defined(BMI08_KERNEL_6_1)
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 15, 0) || LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		sdata->trig_info->swt.trigger =
		iio_trigger_alloc(indio_dev->dev.parent,
			"%s-dev%d", indio_dev->name, iio_device_id(indio_dev));
#else
		sdata->trig_info->swt.trigger = iio_trigger_alloc("%s-dev%d",
								indio_dev->name, indio_dev->id);
#endif
		if (!sdata->trig_info->swt.trigger) {
			ret = -ENOMEM;
			goto err_free_trig_info;
		}
		iio_trigger_set_drvdata(sdata->trig_info->swt.trigger,
											sdata->trig_info);
		sdata->trig_info->swt.trigger->ops = &iio_hrtimer_trigger_ops;
		hrtimer_init(&sdata->trig_info->timer, CLOCK_MONOTONIC,
				HRTIMER_MODE_REL_HARD);
		sdata->trig_info->timer.function = iio_hrtimer_trig_handler;
		sdata->trig_info->sampling_frequency =
								HRTIMER_DEFAULT_SAMPLING_FREQUENCY;
		sdata->trig_info->period = ns_to_ktime(10000000);
		ret = iio_trigger_register(sdata->trig_info->swt.trigger);
		if (ret)
			goto err_free_trigger;
		if (type == 0)
			sensor_type = 0;
		if (type == 1)
			sensor_type = 1;
		if (type == 2)
			sensor_type = 2;
	}
	return ret;
err_free_trigger:
	iio_trigger_free(sdata->trig_info->swt.trigger);
err_free_trig_info:
	kfree(sdata->trig_info);
	return ret;
}
/**
 * bmi08x_iio_deallocate_trigger() - release trigger resources
 * @indo_dev: device instance state
 */
void bmi08x_iio_deallocate_trigger(struct iio_dev *indio_dev)
{
	struct bmi08_client_data *sdata = iio_priv(indio_dev);

	if (sdata->trig_info) {
		iio_trigger_unregister(sdata->trig_info->swt.trigger);
		/* cancel the timer after unreg to make sure no one rearms it */
		/*lint -e534*/
		hrtimer_cancel(&sdata->trig_info->timer);
		/*lint +e534*/
		iio_trigger_free(sdata->trig_info->swt.trigger);
		kfree(sdata->trig_info);
		sdata->trig_info = NULL;
	}
}
