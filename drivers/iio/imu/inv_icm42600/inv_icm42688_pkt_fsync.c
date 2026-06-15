/*
 * Copyright (C) 2026 D-Robotics Co., Ltd.
 * Author: fuhua.wang <fuhua.wang@d-robotics.cc>
 * Modified by: fuhua.wang <fuhua.wang@d-robotics.cc>
 *
 * ICM42688 FSYNC packet sysfs interface.
 *
 * Registers a dedicated IIO device and exposes pkt_fsync sysfs attribute
 * for reading FSYNC-aligned IMU snapshot without the FIFO buffer path.
 */
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#include "inv_icm42600.h"
#include "inv_icm42600_timestamp.h"
#include "inv_icm42688_pkt_fsync.h"

static const struct iio_chan_spec inv_icm42600_fsync_channels[] = {
    {
        .type = IIO_TIMESTAMP,
        .scan_index = 0,
        .scan_type = {
            .sign = 's',
            .realbits = 64,
            .storagebits = 64,
            .endianness = IIO_CPU,
        },
    },
};

static const struct iio_info inv_icm42600_fsync_info = {
    .attrs = &inv_icm42688_pkt_fsync_attr_group,
};

struct iio_dev *inv_icm42600_fsync_init(struct inv_icm42600_state *st)
{
    struct device *dev = regmap_get_device(st->map);
    const char *name;
    struct iio_dev *indio_dev;
    int ret;

    name = devm_kasprintf(dev, GFP_KERNEL, "%s-fsync", st->name);
    if (!name)
        return ERR_PTR(-ENOMEM);

    indio_dev = devm_iio_device_alloc(dev, 0);
    if (!indio_dev)
        return ERR_PTR(-ENOMEM);

    iio_device_set_drvdata(indio_dev, st);
    indio_dev->name = name;
    indio_dev->info = &inv_icm42600_fsync_info;
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->channels = inv_icm42600_fsync_channels;
    indio_dev->num_channels = ARRAY_SIZE(inv_icm42600_fsync_channels);

    ret = devm_iio_device_register(dev, indio_dev);
    if (ret)
        return ERR_PTR(ret);

    return indio_dev;
}

static int inv_icm42688_read_pkt(struct inv_icm42600_state *st,
				 struct inv_icm42688_fsync_pkt *pkt)
{
	struct device *dev = regmap_get_device(st->map);
	struct inv_icm42600_sensor_conf conf = INV_ICM42600_SENSOR_CONF_INIT;
	__be64 raw[2];
	u64 p0, p1;
	int ret;

	pm_runtime_get_sync(dev);
	mutex_lock(&st->lock);

	conf.mode = INV_ICM42600_SENSOR_MODE_LOW_NOISE;
	ret = inv_icm42600_set_gyro_conf(st, &conf, NULL);
	if (ret)
		goto out;

	ret = inv_icm42600_set_accel_conf(st, &conf, NULL);
	if (ret)
		goto out;

	ret = inv_icm42600_set_temp_conf(st, true, NULL);
	if (ret)
		goto out;

	ret = regmap_bulk_read(st->map,
			       INV_ICM42600_REG_TEMP_DATA,
			       raw, sizeof(raw));
	if (ret)
		goto out;

	p0 = be64_to_cpu(raw[0]);
	p1 = be64_to_cpu(raw[1]);

	/* accel */
	pkt->accel_x = (p0 >> 32) & 0xFFFF;
	pkt->accel_y = (p0 >> 16) & 0xFFFF;
	pkt->accel_z = (p0 >> 0)  & 0xFFFF;

	/* gyro */
	pkt->gyro_x  = (p1 >> 48) & 0xFFFF;
	pkt->gyro_y  = (p1 >> 32) & 0xFFFF;
	pkt->gyro_z  = (p1 >> 16) & 0xFFFF;

	/* fsync timestamp */
	pkt->fsync_ts = p1 & 0xFFFF;

out:
	mutex_unlock(&st->lock);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);
	return ret;
}

/* ---------- sysfs show ---------- */

ssize_t inv_icm42688_pkt_fsync_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm42600_state *st = iio_device_get_drvdata(indio_dev);
	struct inv_icm42688_fsync_pkt pkt;
	u64 hi, lo;
	int ret;

	ret = iio_device_claim_direct_mode(indio_dev);
	if (ret)
		return ret;

	ret = inv_icm42688_read_pkt(st, &pkt);

	iio_device_release_direct_mode(indio_dev);
	if (ret)
		return ret;

	hi = ((u64)pkt.fsync_ts << 48) |
	     ((u64)pkt.accel_x  << 32) |
	     ((u64)pkt.accel_y  << 16) |
	     ((u64)pkt.accel_z);

	lo = ((u64)pkt.gyro_x << 48) |
	     ((u64)pkt.gyro_y << 32) |
	     ((u64)pkt.gyro_z << 16);

	/* Emit as a logical 128-bit value (hi64 lo64) */
	return sysfs_emit(buf, "0x%016llx 0x%016llx\n", hi, lo);
}



/* ---------- attribute ---------- */

static IIO_DEVICE_ATTR(pkt_fsync, 0444,
		       inv_icm42688_pkt_fsync_show, NULL, 0);

static struct attribute *inv_icm42688_pkt_fsync_attrs[] = {
	&iio_dev_attr_pkt_fsync.dev_attr.attr,
	NULL,
};

const struct attribute_group inv_icm42688_pkt_fsync_attr_group = {
	.attrs = inv_icm42688_pkt_fsync_attrs,
};
