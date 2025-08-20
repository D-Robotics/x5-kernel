/**
 * @section LICENSE
 * Copyright (c) 2019~2020 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi08x_i2c.c
 * @date	 2022/06/22
 * @version	 2.0.0
 *
 * @brief	 bmi08x I2C Driver Intf
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "bmi08x_driver.h"
#include "bs_log.h"

#define BMI_I2C_WRITE_DELAY_TIME (1)
#define BMI_MAX_RETRY_I2C_XFER (10)

struct i2c_client *acc_client;
struct i2c_client *gyro_client;
int acc_irq_pin;
int gyr_irq_pin;
static int gpio_acc = -1;
static int gpio_gyr = -1;
static inline void hobot_exit_gpio(int gpio)
{
	gpio_free(gpio);
}

static int hobot_gpio2irq(struct device *dev, int gpio,const char* name)
{
	int ret = -1;
	if ((!gpio_is_valid(gpio)) || gpio_request(gpio, name))
	{
		ret = -EPROBE_DEFER;
		goto error;
	}
	ret = gpio_direction_input(gpio);
	if (ret < 0)
	{
		dev_err(dev, "%s gpio_dir_input(%d) ERR:%d\n", __func__, gpio, ret);
		goto error;

	}
	ret = gpio_to_irq(gpio);
	if (ret <= 0)
	{
		dev_err(dev, "%s gpio_to_irq(%d) ERR:%d\n",
				__func__, gpio, ret);
		goto error;
	}
	pr_err("The irq number is:%d\n",ret);
	return ret;

error:
	hobot_exit_gpio(gpio);
	return ret;
}

/**
 * bmi08x_i2c_read - The I2C read function.
 *
 * @client : Instance of the I2C client
 * @reg_addr : The register address from where the data is read.
 * @data : The pointer to buffer to return data.
 * @len : The number of bytes to be read
 *
 * Return : Status of the function.
 * * 0 - OK
 * * negative value - Error.
 */
static s8 bmi08x_i2c_read(struct i2c_client *client, u8 reg_addr,
						  u8 *data, u8 len)
{
	s32 retry;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg_addr,
		},

		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	for (retry = 0; retry < BMI_MAX_RETRY_I2C_XFER; retry++)
	{
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
					 BMI_I2C_WRITE_DELAY_TIME * 1000);
	}

	if (retry >= BMI_MAX_RETRY_I2C_XFER)
	{
		PERR("I2C xfer error");
		return -EIO;
	}

	return 0;
}

/**
 * bmi08x_i2c_write - The I2C write function.
 *
 * @client : Instance of the I2C client
 * @reg_addr : The register address to start writing the data.
 * @data : The pointer to buffer holding data to be written.
 * @len : The number of bytes to write.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * negative value - Error.
 */
static s8 bmi08x_i2c_write(struct i2c_client *client, u8 reg_addr,
						   const u8 *data, u8 len)
{
	s32 retry;

	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = len + 1,
		.buf = NULL,
	};
	msg.buf = kmalloc(len + 1, GFP_KERNEL);
	if (!msg.buf)
	{
		PERR("Allocate memory failed\n");
		return -ENOMEM;
	}
	msg.buf[0] = reg_addr;
	memcpy(&msg.buf[1], data, len);
	for (retry = 0; retry < BMI_MAX_RETRY_I2C_XFER; retry++)
	{
		if (i2c_transfer(client->adapter, &msg, 1) > 0)
			break;
		usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
					 BMI_I2C_WRITE_DELAY_TIME * 1000);
	}
	kfree(msg.buf);
	if (retry >= BMI_MAX_RETRY_I2C_XFER)
	{
		PERR("I2C xfer error");
		return -EIO;
	}

	return 0;
}

/**
 * bmi08x_i2c_read_wrapper - The I2C read function pointer used by BMI08x API.
 *
 * @dev_addr : I2c Device address
 * @reg_addr : The register address to read the data.
 * @data : The pointer to buffer to return data.
 * @len : The number of bytes to be read
 *
 * Return : Status of the function.
 * * 0 - OK
 * * negative value - Error.
 */
static s8 bmi08x_i2c_read_wrapper(u8 reg_addr, u8 *data,
								  u32 len, void *intf_ptr)
{
	int err = 0;

	err = bmi08x_i2c_read(intf_ptr, reg_addr, data, len);
	return err;
}

/**
 * bmi08x_i2c_write_wrapper - The I2C write function pointer used by BMI08x API.
 *
 * @dev_addr : I2c Device address
 * @reg_addr : The register address to start writing the data.
 * @data : The pointer to buffer which holds the data to be written.
 * @len : The number of bytes to be written.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * negative value - Error.
 */
static s8 bmi08x_i2c_write_wrapper(u8 reg_addr, const u8 *data,
								   u32 len, void *intf_ptr)
{
	int err = 0;

	err = bmi08x_i2c_write(intf_ptr, reg_addr, data, len);
	return err;
}

/*!
 * @brief BMI08X probe function via i2c bus
 *
 * @param client the pointer of i2c client
 * @param id the pointer of i2c device id
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
 */
static int bmi08x_i2c_probe(struct i2c_client *client,
							const struct i2c_device_id *id)
{
	pr_err("bmi088:i2c start\n");
	int err = 0;

	static u8 dev_id;
	struct bmi08x_client_data *client_data = NULL;
	PINFO("client->name:%s / addr: 0x%x", client->name, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		PERR("i2c_check_functionality error!");
		return -EIO;
	}
	client_data = kzalloc(sizeof(struct bmi08x_client_data),
						  GFP_KERNEL);
	if (client_data == NULL)
		return -ENOMEM;

	client_data->device.read_write_len = 32;
	client_data->device.read = bmi08x_i2c_read_wrapper;
	client_data->device.write = bmi08x_i2c_write_wrapper;
	client_data->device.intf = BMI08X_I2C_INTF;
	client_data->device.variant = BMI088_VARIANT;
	client_data->dev = &client->dev;

	if (client->addr == BMI08X_ACCEL_I2C_ADDR_SECONDARY)
	{
		acc_client = client;
		gpio_acc = of_get_named_gpio(client->dev.of_node,"accel_irq_gpio", 0);
		pr_err("ACC GPIO NUM:%d\n",gpio_acc);
		acc_irq_pin = hobot_gpio2irq(&client->dev,gpio_acc,client->name);
	}

	if (client->addr == BMI08X_GYRO_I2C_ADDR_SECONDARY)
	{
		gyro_client = client;
		gpio_gyr = of_get_named_gpio(client->dev.of_node,"gyro_irq_gpio", 0);
		pr_err("GYR GPIO NUM:%d\n",gpio_gyr);
		gyr_irq_pin = hobot_gpio2irq(&client->dev,gpio_gyr,client->name);
	}
	dev_id++;
	if (dev_id == 2)
	{
		client_data->device.intf_ptr_accel = acc_client;
		client_data->device.intf_ptr_gyro = gyro_client;
		client_data->IRQ = acc_irq_pin;
		client_data->GYR_IRQ = gyr_irq_pin;

		return bmi08x_probe(client_data, client_data->dev);
	}

	return err;
}

/**
 *	bmi08x_i2c_remove - Callback called when device is unbinded.
 *	@client : Instance of I2C client device.
 *
 *	Return : Status of the suspend function.
 *	* 0 - OK.
 *	* Negative value - Error.
 */
// static int bmi08x_i2c_remove(struct i2c_client *client)
// {
// 	int err = 0;

// 	err = bmi08x_remove(&client->dev);
// 	free_irq(acc_irq_pin,&client->dev);
// 	free_irq(gyr_irq_pin,&client->dev);
// 	gpio_free(gpio_gyr);
// 	gpio_free(gpio_acc);
// 	return err;
// }

static void bmi08x_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = bmi08x_remove(&client->dev);
	free_irq(acc_irq_pin,&client->dev);
	free_irq(gyr_irq_pin,&client->dev);
	gpio_free(gpio_gyr);
	gpio_free(gpio_acc);
	// return err;
}

static const struct i2c_device_id bmi08x_id[] = {
	{SENSOR_NAME, 0},
	{}};

MODULE_DEVICE_TABLE(i2c, bmi08x_id);

static const struct of_device_id bmi160_of_match[] = {
	{
		.compatible = "bosch-sensortec,bmi08x",
	},
	{
		.compatible = "bmi08x",
	},
	{
		.compatible = "bmi08xa",
	},
	{
		.compatible = "bmi08xg",
	},
	{}};
MODULE_DEVICE_TABLE(of, bmi160_of_match);

static struct i2c_driver bmi_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
		.of_match_table = bmi160_of_match,
	},
	.class = I2C_CLASS_HWMON,
	.id_table = bmi08x_id,
	.probe = bmi08x_i2c_probe,
	.remove = bmi08x_i2c_remove,
};
/**
 *	bmi08x_i2c_init - I2C driver init function.
 */
static int __init bmi_i2c_init(void)
{
	pr_err("bmi088:i2c init\n");
	return i2c_add_driver(&bmi_i2c_driver);
}
/**
 *	bmi08x_i2c_exit - I2C driver exit function.
 */
static void __exit bmi_i2c_exit(void)
{
	i2c_del_driver(&bmi_i2c_driver);
}

MODULE_AUTHOR("contact@bosch-sensortec.com");
MODULE_DESCRIPTION("BMI088 SENSOR DRIVER");
MODULE_LICENSE("GPL v2");
/*lint -e19*/
module_init(bmi_i2c_init);
module_exit(bmi_i2c_exit);
/*lint +e19*/
