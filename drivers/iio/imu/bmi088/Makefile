#
# Makefile for Bosch sensor driver.
#

obj-$(CONFIG_BMI08X_SUPPORT_SPI_BUS) += bmi08a_spi_driver.o
obj-$(CONFIG_BMI08X_SUPPORT_SPI_BUS) += bmi08g_spi_driver.o

bmi08a_spi_driver-objs := bmi08a_spi.o bs_log.o bmi08a_driver.o bmi08a.o bmi08g.o bmi08xa.o bmi08x_iio_buffer.o
bmi08g_spi_driver-objs := bmi08g_spi.o bmi08g_driver.o bmi08a.o bmi08g.o bmi08xa.o bmi08x_iio_buffer.o

obj-$(CONFIG_BMI08X_SUPPORT_I2C_BUS) += bmi08x_i2c_driver.o
bmi08x_i2c_driver-objs := bmi08x_i2c.o bs_log.o bmi08x_driver.o bmi08a.o bmi08g.o bmi08xa.o bmi08x_iio_buffer.o
