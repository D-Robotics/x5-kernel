/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef INV_ICM42688_PKT_FSYNC_H_
#define INV_ICM42688_PKT_FSYNC_H_

#include <linux/iio/iio.h>

struct inv_icm42600_state;

struct inv_icm42688_fsync_pkt {
	u16 fsync_ts;
	u16 accel_x;
	u16 accel_y;
	u16 accel_z;
	u16 gyro_x;
	u16 gyro_y;
	u16 gyro_z;
};

/* sysfs show */
ssize_t inv_icm42688_pkt_fsync_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf);

/* attribute group */
extern const struct attribute_group inv_icm42688_pkt_fsync_attr_group;

#endif
