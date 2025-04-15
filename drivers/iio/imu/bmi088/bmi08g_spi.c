/**
 * @section LICENSE
 * Copyright (c) 2017-2024 Bosch Sensortec GmbH All Rights Reserved.
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @file		bmi08g_spi.c
 * @date		2024-10-10
 * @version		v1.4.1
 *
 * @brief		BMI088 SPI bus Driver
 *
 */

/*********************************************************************/
/* system header files */
/*********************************************************************/
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/input.h>


#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/of_irq.h>

#include <linux/version.h>

/*********************************************************************/
/* own header files */
/*********************************************************************/
#include "bmi08g_driver.h"
#include "bs_log.h"

/*********************************************************************/
/* Local macro definitions */
/*********************************************************************/
#define BMI_MAX_BUFFER_SIZE		32

/*********************************************************************/
/* global variables */
/*********************************************************************/
static struct spi_device *bmi_spi_client;
struct iio_dev *g_iio_spi_dev;

/*!
 * @brief define spi block wirte function
 *
 * @param[in] reg_addr register address
 * @param[in] data the pointer of data buffer
 * @param[in] len block size need to write
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
 */
static s8 bmi08g_spi_write_block(u8 reg_addr,
					const u8 *data, u8 len)
{
	struct spi_device *client = bmi_spi_client;
	u8 buffer[BMI_MAX_BUFFER_SIZE + 1];
	struct spi_transfer xfer = {
		.tx_buf = buffer,
		.len = len + 1,
	};
	struct spi_message msg;

	if (len > BMI_MAX_BUFFER_SIZE)
		return -EINVAL;

	buffer[0] = reg_addr&0x7F;/* write: MSB = 0 */
	memcpy(&buffer[1], data, len);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	return spi_sync(client, &msg);
}

/*!
 * @brief define spi block read function
 *
 * @param[in] reg_addr register address
 * @param[out] data the pointer of data buffer
 * @param[in] len block size need to read
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
 */
static s8 bmi08g_spi_read_block(u8 reg_addr,
							u8 *data, uint16_t len)
{
	struct spi_device *client = bmi_spi_client;
	u8 reg = reg_addr | 0x80;/* read: MSB = 1 */
	struct spi_transfer xfer[2] = {
		[0] = {
			.tx_buf = &reg,
			.len = 1,
		},
		[1] = {
			.rx_buf = data,
			.len = len,
		}
	};
	struct spi_message msg;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer[0], &msg);
	spi_message_add_tail(&xfer[1], &msg);
	return spi_sync(client, &msg);
}

/**
 * bmi08g_spi_write_wrapper - The SPI write function pointer used by BMI088 API.
 *
 * @reg_addr : The register address to start writing the data.
 * @data : The pointer to buffer which holds the data to be written.
 * @len : The number of bytes to be written.
 * @intf_ptr  : Void pointer that can enable the linking of descriptors
 *									for interface related call backs.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * negative value - Error.
 */
static s8 bmi08g_spi_write_wrapper(u8 reg_addr, const u8 *data,
						u32 len, void *intf_ptr)
{
	s8 err;

	err = bmi08g_spi_write_block(reg_addr, data, len);
	return err;
}

/**
 * bmi08g_spi_read_wrapper - The SPI read function pointer used by BMI088 API.
 *
 * @reg_addr : The register address to read the data.
 * @data : The pointer to buffer to return data.
 * @len : The number of bytes to be read
 *
 * Return : Status of the function.
 * * 0 - OK
 * * negative value - Error.
 */
static s8 bmi08g_spi_read_wrapper(u8 reg_addr, u8 *data,
						u32 len, void *intf_ptr)
{
	s8 err;

	err = bmi08g_spi_read_block(reg_addr, data, len);
	return err;
}

/*!
 * @brief sensor probe function via spi bus
 *
 * @param[in] client the pointer of spi client
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
 */
static int bmi08g_spi_probe(struct spi_device *client)
{
	int status;
	int err = 0;
	struct bmi08g_client_data *g_client_data = NULL;

	if (bmi_spi_client == NULL)
		bmi_spi_client = client;
	else{
		PERR("This driver does not support multiple clients!\n");
		return -EBUSY;
	}
	client->bits_per_word = 8;
	status = spi_setup(client);
	if (status < 0) {
		PERR("spi_setup failed!\n");
		return status;
	}
	g_client_data = kzalloc(sizeof(struct bmi08g_client_data), GFP_KERNEL);
	if (g_client_data == NULL) {
		PERR("no memory available");
		err = -ENOMEM;
		goto exit_err_clean;
	}
	g_iio_spi_dev = devm_iio_device_alloc(&client->dev, sizeof(*g_client_data));
	if (!g_iio_spi_dev)
		return -ENOMEM;
	g_client_data = iio_priv(g_iio_spi_dev);
	g_client_data->device.read_write_len = 32;
	g_client_data->device.read = bmi08g_spi_read_wrapper;
	g_client_data->device.write = bmi08g_spi_write_wrapper;
	g_client_data->device.intf = BMI08_SPI_INTF;
	g_client_data->device.variant = BMI088_VARIANT;
	g_client_data->dev = &client->dev;
	g_iio_spi_dev->dev.parent = &client->dev;
	// g_iio_spi_dev->name = "bmi08g";
	g_iio_spi_dev->name = "bmi088_gyro";
	g_iio_spi_dev->modes = INDIO_DIRECT_MODE;
	g_client_data->device.read_write_len = 32;
	g_client_data->device.intf = BMI08_SPI_INTF;
	g_client_data->device.intf_ptr_gyro = client;
	g_client_data->GYR_IRQ = client->irq;
	dev_set_drvdata(&client->dev, g_iio_spi_dev);

	return bmi08g_probe(g_iio_spi_dev);
exit_err_clean:
	if (err)
		bmi_spi_client = NULL;
	if (g_iio_spi_dev)
		iio_device_free(g_iio_spi_dev);
	return err;
}

/*!
 * @brief remove bmi spi client
 *
 * @param[in] client the pointer of spi client
 *
 * @return zero
 * @retval zero
 */
// #ifndef BMI08_KERNEL_6_1
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 1, 0)
static int bmi08g_spi_remove(struct spi_device *client)
{
	int err = 0;
#else
static void bmi08g_spi_remove(struct spi_device *client)
{
#endif

// #ifndef BMI08_KERNEL_6_1
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 1, 0)
		err = bmi08g_remove(g_iio_spi_dev);
#else
		bmi08g_remove(g_iio_spi_dev);
#endif
	bmi_spi_client = NULL;

// #ifndef BMI08_KERNEL_6_1
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 1, 0)
	return err;
#endif
}

/*!
 * @brief register spi device id
 */
static const struct spi_device_id bmi08g_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, bmi08g_id);

/*!
 * @brief register bmp3 device id match
 */
static const struct of_device_id bmi08g_of_match[] = {
	// { .compatible = "bmi08g", },
	{ .compatible = "bmi088_gyro", },
	{ }
};
MODULE_DEVICE_TABLE(of, bmi08g_of_match);

/*!
 * @brief register spi driver hooks
 */
static struct spi_driver bmi08g_spi_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = SENSOR_NAME,
		.of_match_table = bmi08g_of_match,
	},
	.id_table = bmi08g_id,
	.probe	  = bmi08g_spi_probe,
	.remove	  = bmi08g_spi_remove,
};

/*!
 * @brief initialize bmi spi module
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
 */
static int __init bmi08g_spi_init(void)
{
	return spi_register_driver(&bmi08g_spi_driver);
}

/*!
 * @brief remove bmi spi module
 *
 * @return no return value
 */
static void __exit bmi08g_spi_exit(void)
{
	spi_unregister_driver(&bmi08g_spi_driver);
}


MODULE_AUTHOR("Contact <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("BMI088 SENSOR SPI DRIVER");
MODULE_LICENSE("GPL v2");
/*lint -e19*/
module_init(bmi08g_spi_init);
module_exit(bmi08g_spi_exit);
/*lint +e19*/
