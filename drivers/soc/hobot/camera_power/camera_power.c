#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/cdev.h>

#define NAME_LEN 32
struct camera_power {
        struct platform_device *pdev;
        struct miscdevice miscdev;
        struct regulator *regulator;
};

static int camera_power_open(struct inode *inode, struct file *filp)
{
        int ret = 0;
        struct camera_power *power = NULL;

        power = container_of(filp->private_data, struct camera_power, miscdev);
        if (power == NULL) {
                pr_err("get camera power data failed\n");
                return -1;
        }
        ret = regulator_enable(power->regulator);

        if (ret) {
                dev_err(power->miscdev.this_device, "enable regulator failed, return!\n");
        }

        return ret;
}

static int camera_power_release(struct inode *inode, struct file *filp)
{
        int ret = 0;
        struct camera_power *power = NULL;

        power = container_of(filp->private_data, struct camera_power, miscdev);
        if (power == NULL) {
                pr_err("get camera power data failed\n");
                return -1;
        }
        ret = regulator_disable(power->regulator);

        if (ret) {
                dev_err(power->miscdev.this_device, "disable regulator failed, return!\n");
        }

        return ret;
}

static const struct file_operations camera_power_fops = {
        .owner = THIS_MODULE,
        .open = camera_power_open,
        .release = camera_power_release,
};

static int camera_power_probe(struct platform_device *pdev)
{
        int ret = 0;
        struct camera_power *power = NULL;
        struct device *dev = &pdev->dev;
        int32_t id = 0;
        char name[NAME_LEN] = {0};

        dev_info(dev, "%s\n", __func__);

        power = devm_kzalloc(dev, sizeof(struct camera_power), GFP_KERNEL);
        if (power == NULL) {
                dev_err(dev, "Out of memory\n");
                return -ENOMEM;
        }
        power->regulator = devm_regulator_get_optional(dev, "power");

        if (IS_ERR(power->regulator)) {
                if (PTR_ERR(power->regulator) == -EPROBE_DEFER)
                        return -EPROBE_DEFER;
                dev_dbg(dev, "Failed to get camera power's regulator\n");
                power->regulator = NULL;
        }
        ret = of_property_read_u32(dev->of_node, "id", &id);
        if (ret) {
                dev_err(dev, "get device id failed\n");
                return -1;
        }

        ret = snprintf(name, NAME_LEN, "camera_power%d", id);
        if (ret <= 0) {
                dev_err(dev, "camera power name create failed\n");
        }
        power->miscdev.minor	= MISC_DYNAMIC_MINOR;
        power->miscdev.name	= kstrdup(name, GFP_KERNEL);
        power->miscdev.fops	= &camera_power_fops;
        ret = misc_register(&power->miscdev);
        if (ret != 0) {
                pr_err("camera_power register failed\n");
                if (power->regulator) {
                        regulator_put(power->regulator);
                        power->regulator = NULL;
                }
                return ret;
        }
        dev_set_drvdata(&pdev->dev, power);
        dev_info(dev, "%s done\n", __func__);
        return ret;
}

static int camera_power_remove(struct platform_device *pdev)
{
        struct camera_power *power = NULL;

        pr_info("camera_power remove\n");

        power = dev_get_drvdata(&pdev->dev);
        if (power == NULL) {
                dev_err(&pdev->dev,"get driver data error\n");
                return -1;
        }

        misc_deregister(&power->miscdev);
        if (power->regulator) {
                regulator_put(power->regulator);
                power->regulator = NULL;
        }

        return 0;
}

static const struct of_device_id camera_power_match[] = {
        { .compatible = "d-robotics,camera-power" },
        { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, camera_power_match);
static struct platform_driver camera_power_driver = {
        .probe	= camera_power_probe,
        .remove = camera_power_remove,
        .driver = {
                .name	= "camera_power",
                .of_match_table = camera_power_match,
        }
};
module_platform_driver(camera_power_driver);
MODULE_AUTHOR("Ming Yu");
MODULE_DESCRIPTION("D-robotic X5 camera power Driver");
MODULE_LICENSE("GPL v2");
