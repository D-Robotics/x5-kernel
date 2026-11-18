#
# Makefile for Invensense ICM-42607 driver (inv_icm42607).
# Core module built from vendor inv-mpu-iio sources; object file
# names kept to ease diffing against upstream TDK release
# invensense-linux-driver-icm43600-10.0.2.
#

obj-$(CONFIG_INV_ICM42607) += inv-icm42607.o

inv-icm42607-objs += inv_mpu_common.o
inv-icm42607-objs += inv_mpu_ring.o
inv-icm42607-objs += inv_mpu_timestamp.o
inv-icm42607-objs += inv_mpu_dts.o
inv-icm42607-objs += icm43600/inv_mpu_init_43600.o
inv-icm42607-objs += icm43600/inv_mpu_core_43600.o
inv-icm42607-objs += icm43600/inv_mpu_parsing_43600.o
inv-icm42607-objs += icm43600/inv_mpu_setup_43600.o
inv-icm42607-objs += icm43600/inv_mpu_misc_43600.o
inv-icm42607-objs += icm43600/inv_mpu_selftest_43600.o

obj-$(CONFIG_INV_ICM42607_I2C) += inv-icm42607-i2c.o
inv-icm42607-i2c-objs := inv_mpu_i2c.o
