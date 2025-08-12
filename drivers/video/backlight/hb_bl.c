/***************************************************************************
 *						COPYRIGHT NOTICE
 *			   Copyright 2023 Horizon Robotics, Inc.
 *					   All rights reserved.
 ***************************************************************************/

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "hb_bl.h"

#define BL_DEBUG 0
static struct hobot_backlight_controller_data *controller;
static int connected = 0;
static int lcd_bright_level = 0;
static struct backlight_device *bl = NULL;

#define MAX_BRIGHENESS (255)

static int is_hex(char num) {
  // 0-9, a-f, A-F
  if ((47 < num && num < 58) || (64 < num && num < 71) ||
      (96 < num && num < 103))
    return 1;
  return 0;
}

static int string_to_byte(const char *source, unsigned char *destination,
                          int size) {
  int i = 0, counter = 0;
  char c[3] = {0};
  unsigned char bytes;

  if (size % 2 == 1) return -EINVAL;

  for (i = 0; i < size; i++) {
    if (!is_hex(source[i])) {
      return -EINVAL;
    }
    if (0 == i % 2) {
      c[0] = source[i];
      c[1] = source[i + 1];
      sscanf(c, "%hhx", &bytes);
      destination[counter] = bytes;
      counter++;
    }
  }
  return 0;
}

static int send_cmds(struct i2c_client *client, const char *buf) {
  int ret, size = strlen(buf), retry = 5;
  unsigned char byte_cmd[100];

  if ((size % 2) != 0) {
    LOG_ERR("size should be even\n");
    return -EINVAL;
  }

  LOG_INFO("%s\n", buf);

  string_to_byte(buf, byte_cmd, size);

  while (retry-- > 0) {
    ret = i2c_master_send(client, byte_cmd, size / 2);
    if (ret <= 0) {
      LOG_INFO("send command: data[0]:0x%2x,data[1]:0x%2x failed, ret = %d, retry again!\n",byte_cmd[0],byte_cmd[1],ret);
    } else
      break;
  }

  if (ret <= 0) {
    return ret != 0 ? ret : -ECOMM;
  }

  msleep(20);
  return 0;
}

static int recv_cmds(struct i2c_client *client, char *buf, int size) {
  int ret;

  ret = i2c_master_recv(client, buf, size);
  if (ret <= 0) {
    LOG_ERR("receive commands failed, %d\n", ret);
    return ret != 0 ? ret : -ECOMM;
  }
  msleep(20);
  return 0;
}

static int init_cmd_check(struct hobot_backlight_controller_data *controller) {
  int ret;
  char recv_buf[1] = {0};

  ret = send_cmds(controller->client, "80");
  if (ret < 0) goto error;

  recv_cmds(controller->client, recv_buf, 1);
  if (ret < 0) goto error;

  LOG_INFO("recv_cmds: 0x%X\n", recv_buf[0]);
  if (recv_buf[0] != 0xDE && recv_buf[0] != 0xC3) {
    LOG_ERR("read wrong info\n");
    ret = -EINVAL;
    goto error;
  }
  return 0;

error:
  return ret;
}

int controller_screen_power_off(void) {
  if (!connected) return -ENODEV;

  LOG_INFO("\n");
  send_cmds(controller->client, "8500");
  msleep(10);

  return 0;
}
EXPORT_SYMBOL_GPL(controller_screen_power_off);

int controller_screen_power_up(void) {
  if (!connected) return -ENODEV;

  send_cmds(controller->client, "8501");
  send_cmds(controller->client, "8104");
  send_cmds(controller->client, "8601");
  return 0;
}
EXPORT_SYMBOL_GPL(controller_screen_power_up);

int controller_set_bright(int bright) {
  unsigned char cmd[2];
  int ret;

  if (!connected) return -ENODEV;

  if (bright > 0xff || bright < 0) return -EINVAL;

  if (BL_DEBUG) LOG_INFO("set bright = 0x%x\n", bright);

  cmd[0] = 0x86;
  cmd[1] = bright;

  ret = i2c_master_send(controller->client, cmd, 2);
  if (ret <= 0) {
    LOG_INFO("send command failed, ret = %d\n", ret);
    return ret != 0 ? ret : -ECOMM;
  }

  lcd_bright_level = bright;

  return 0;
}
EXPORT_SYMBOL_GPL(controller_set_bright);

int controller_get_brightness(void) { return lcd_bright_level; }
EXPORT_SYMBOL_GPL(controller_get_brightness);

static int controller_bl_get_brightness(struct backlight_device *bd) {
  return lcd_bright_level;
}

int controller_bl_update_status(struct backlight_device *bd) {
  int brightness = bd->props.brightness;

  if (brightness > MAX_BRIGHENESS) brightness = MAX_BRIGHENESS;

  if (brightness <= 0) brightness = 1;

  if (bd->props.power != FB_BLANK_UNBLANK) brightness = 0;

  if (bd->props.state & BL_CORE_SUSPENDED) brightness = 0;

  LOG_INFO(
      "brightness=%d power=%d fb_blank=%d state =%d  bd->props.brightness=%d\n",
      brightness, bd->props.power, bd->props.fb_blank, bd->props.state,
      bd->props.brightness);
  return controller_set_bright(brightness);
}

static const struct backlight_ops controller_bl_ops = {
    .get_brightness = controller_bl_get_brightness,  // actual_brightness_show
    .update_status = controller_bl_update_status,
    .options = BL_CORE_SUSPENDRESUME,
};

struct backlight_device *controller_get_backlightdev(void) {
  if (!connected) {
    printk("controller_get_backlightdev is not ready\n");
    return NULL;
  }
  return bl;
}

static ssize_t controller_bl_show(struct device *dev,
                                  struct device_attribute *attr, char *buf) {
  if (BL_DEBUG) LOG_INFO("get bright = 0x%x\n", lcd_bright_level);

  return sprintf(buf, "%d\n", lcd_bright_level);
}

static ssize_t controller_bl_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count) {
  int value;

  value = simple_strtoul(buf, NULL, 0);

  if ((value < 0) || (value > MAX_BRIGHENESS)) {
    LOG_ERR("Invalid value for backlight setting, value = %d\n", value);
  } else
    controller_set_bright(value);

  return strnlen(buf, count);
}
static DEVICE_ATTR(controller_bl, S_IRUGO | S_IWUSR, controller_bl_show,
                   controller_bl_store);

int controller_is_connected(void) { return connected; }
EXPORT_SYMBOL_GPL(controller_is_connected);

static int hobot_bl_probe(struct i2c_client *client,
                          const struct i2c_device_id *id) {
  struct hobot_backlight_controller_data *controller_data;
  int ret;
  struct backlight_properties props;

  LOG_INFO("address = 0x%x\n", client->addr);

  if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
    LOG_ERR("I2C check functionality failed\n");
    return -ENODEV;
  }

  controller_data =
      devm_kzalloc(&client->dev,sizeof(struct hobot_backlight_controller_data), GFP_KERNEL);
  if (controller_data == NULL) {
    LOG_ERR("no memory for device\n");
    return -ENOMEM;
  }

  controller_data->client = client;
  i2c_set_clientdata(client, controller_data);
  controller = controller_data;

  ret = init_cmd_check(controller_data);
  if (ret < 0) {
    goto error;
  }
  connected = 1;

  memset(&props, 0, sizeof(props));
  props.type = BACKLIGHT_RAW;
  props.max_brightness = MAX_BRIGHENESS;

  bl = devm_backlight_device_register(&client->dev,"panel_backlight", NULL, NULL,
                                 &controller_bl_ops, &props);
  if (IS_ERR(bl)) {
    pr_err("unable to register backlight device\n");
  }

  ret = device_create_file(&client->dev, &dev_attr_controller_bl);
  if (ret != 0) {
    dev_err(&client->dev, "Failed to create controller_bl sysfs files %d\n",
            ret);
    return ret;
  }

  controller_screen_power_up();

  return 0;

error:
  LOG_INFO("Failed to load backlight driver!");
  // kfree(controller_data);
  return ret;
}

static void hobot_bl_remove(struct i2c_client *client) {
  connected = 0;
  device_remove_file(&client->dev, &dev_attr_controller_bl);
  return;
}

static const struct i2c_device_id controller_id[] = {
    {"hobot_bl_controller", 0},
    {},
};

static struct i2c_driver backlight_controller_driver = {
    .driver =
        {
            .name = "hobot_bl_controller",
        },
    .probe = hobot_bl_probe,
    .remove = hobot_bl_remove,
    .id_table = controller_id,
};
module_i2c_driver(backlight_controller_driver);

MODULE_AUTHOR("jiale01.luo <jiale01.luo@horizon.ai>");
MODULE_DESCRIPTION("Backlight controller driver for Hobot X3 SoC");
MODULE_LICENSE("GPL v2");
