# SPDX-License-Identifier: GPL-2.0
#
# Makefile for industrial I/O accelerometer drivers
#

# When adding new entries keep the list in alphabetical order
obj-$(CONFIG_ADIS16201) += adis16201.o
obj-$(CONFIG_ADIS16209) += adis16209.o
obj-$(CONFIG_ADXL313) += adxl313_core.o
obj-$(CONFIG_ADXL313_I2C) += adxl313_i2c.o
obj-$(CONFIG_ADXL313_SPI) += adxl313_spi.o
obj-$(CONFIG_ADXL345) += adxl345_core.o
obj-$(CONFIG_ADXL345_I2C) += adxl345_i2c.o
obj-$(CONFIG_ADXL345_SPI) += adxl345_spi.o
obj-$(CONFIG_ADXL355) += adxl355_core.o
obj-$(CONFIG_ADXL355_I2C) += adxl355_i2c.o
obj-$(CONFIG_ADXL355_SPI) += adxl355_spi.o
obj-$(CONFIG_ADXL367) += adxl367.o
obj-$(CONFIG_ADXL367_I2C) += adxl367_i2c.o
obj-$(CONFIG_ADXL367_SPI) += adxl367_spi.o
obj-$(CONFIG_ADXL372) += adxl372.o
obj-$(CONFIG_ADXL372_I2C) += adxl372_i2c.o
obj-$(CONFIG_ADXL372_SPI) += adxl372_spi.o
obj-$(CONFIG_BMA180) += bma180.o
obj-$(CONFIG_BMA220) += bma220_spi.o
obj-$(CONFIG_BMA400) += bma400_core.o
obj-$(CONFIG_BMA400_I2C) += bma400_i2c.o
obj-$(CONFIG_BMA400_SPI) += bma400_spi.o
obj-$(CONFIG_BMC150_ACCEL) += bmc150-accel-core.o
obj-$(CONFIG_BMC150_ACCEL_I2C) += bmc150-accel-i2c.o
obj-$(CONFIG_BMC150_ACCEL_SPI) += bmc150-accel-spi.o
obj-$(CONFIG_BMI088_ACCEL) += bmi088-accel-core.o
obj-$(CONFIG_BMI088_ACCEL_SPI) += bmi088-accel-spi.o
obj-$(CONFIG_DA280)	+= da280.o
obj-$(CONFIG_DA311)	+= da311.o
obj-$(CONFIG_DMARD06)	+= dmard06.o
obj-$(CONFIG_DMARD09)	+= dmard09.o
obj-$(CONFIG_DMARD10)	+= dmard10.o
obj-$(CONFIG_FXLS8962AF)	+= fxls8962af-core.o
obj-$(CONFIG_FXLS8962AF_I2C)	+= fxls8962af-i2c.o
obj-$(CONFIG_FXLS8962AF_SPI)	+= fxls8962af-spi.o
obj-$(CONFIG_HID_SENSOR_ACCEL_3D) += hid-sensor-accel-3d.o
obj-$(CONFIG_KXCJK1013) += kxcjk-1013.o
obj-$(CONFIG_KXSD9)	+= kxsd9.o
obj-$(CONFIG_KXSD9_SPI)	+= kxsd9-spi.o
obj-$(CONFIG_KXSD9_I2C)	+= kxsd9-i2c.o
obj-$(CONFIG_MC3230)	+= mc3230.o

obj-$(CONFIG_MMA7455)		+= mma7455_core.o
obj-$(CONFIG_MMA7455_I2C)	+= mma7455_i2c.o
obj-$(CONFIG_MMA7455_SPI)	+= mma7455_spi.o

obj-$(CONFIG_MMA7660)	+= mma7660.o

obj-$(CONFIG_MMA8452)	+= mma8452.o

obj-$(CONFIG_MMA9551_CORE)	+= mma9551_core.o
obj-$(CONFIG_MMA9551)		+= mma9551.o
obj-$(CONFIG_MMA9553)		+= mma9553.o

obj-$(CONFIG_MSA311)		+= msa311.o

obj-$(CONFIG_MXC4005)		+= mxc4005.o
obj-$(CONFIG_MXC6255)		+= mxc6255.o

obj-$(CONFIG_SCA3000)		+= sca3000.o
obj-$(CONFIG_SCA3300)		+= sca3300.o

obj-$(CONFIG_STK8312)		+= stk8312.o
obj-$(CONFIG_STK8BA50)		+= stk8ba50.o

obj-$(CONFIG_IIO_CROS_EC_ACCEL_LEGACY) += cros_ec_accel_legacy.o

obj-$(CONFIG_IIO_SSP_SENSORS_COMMONS) += ssp_accel_sensor.o

obj-$(CONFIG_IIO_ST_ACCEL_3AXIS) += st_accel.o
st_accel-y := st_accel_core.o
st_accel-$(CONFIG_IIO_BUFFER) += st_accel_buffer.o

obj-$(CONFIG_IIO_ST_ACCEL_I2C_3AXIS) += st_accel_i2c.o
obj-$(CONFIG_IIO_ST_ACCEL_SPI_3AXIS) += st_accel_spi.o

bmi088-objs := bmi08x_i2c.o bs_log.o bmi08g.o bmi08a.o bmi08x_driver.o
obj-$(CONFIG_BMI08X) += bmi088.o
