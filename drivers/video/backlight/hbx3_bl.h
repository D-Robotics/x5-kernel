#ifndef _HOBOT_BACKLIGHT_H_
#define _HOBOT_BACKLIGHT_H_

#define LOG_INFO(fmt,arg...) pr_info("hobot-backlight: %s: "fmt, __func__, ##arg);
#define LOG_ERR(fmt,arg...) pr_err("hobot-backlight: %s: "fmt, __func__, ##arg);

#define MAX_I2C_LEN 255

struct hobot_backlight_controller_data {
	struct device *dev;
	struct i2c_client *client;
};

#endif
