// tpa2016d2.c - Linux kernel driver for TI TPA2016D2
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/kernel.h>

/* Register definitions */
#define TPA2016D2_REG_CONTROL           0x01
#define TPA2016D2_REG_ATK               0x02
#define TPA2016D2_REG_HLD               0x03
#define TPA2016D2_REG_REL               0x04
#define TPA2016D2_REG_GAIN              0x05
#define TPA2016D2_REG_AGC               0x06
#define TPA2016D2_REG_UNLIMIT           0x07

/* Bit definitions */
#define TPA2016D2_CONTROL_OUTPUT_EN     BIT(7)
#define TPA2016D2_AGC_EN                BIT(0)

struct tpa2016d2_data {
    struct regmap *regmap;
};

static bool tpa2016d2_readable_reg(struct device *dev, unsigned int reg)
{
    return reg >= 1 && reg <= 7;
}

static bool tpa2016d2_writable_reg(struct device *dev, unsigned int reg)
{
    return reg >= 1 && reg <= 7;
}

static const struct regmap_config tpa2016d2_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = 7,
    .readable_reg = tpa2016d2_readable_reg,
    .writeable_reg = tpa2016d2_writable_reg,
    .cache_type = REGCACHE_RBTREE,
};

static int tpa2016d2_probe(struct i2c_client *client)
{
    struct device *dev = &client->dev;
    struct tpa2016d2_data *data;
    int ret;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
        dev_err(dev, "SMBus byte data not supported\n");
        return -EIO;
    }

    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->regmap = devm_regmap_init_i2c(client, &tpa2016d2_regmap_config);
    if (IS_ERR(data->regmap)) {
        ret = PTR_ERR(data->regmap);
        dev_err(dev, "Failed to allocate regmap: %d\n", ret);
        return ret;
    }

    i2c_set_clientdata(client, data);

    /* Optional: Enable output by default */
    ret = regmap_update_bits(data->regmap, TPA2016D2_REG_CONTROL,
                             TPA2016D2_CONTROL_OUTPUT_EN,
                             TPA2016D2_CONTROL_OUTPUT_EN);
    if (ret)
        dev_warn(dev, "Failed to enable output: %d\n", ret);

    dev_info(dev, "TPA2016D2 probed successfully\n");
    return 0;
}

static void tpa2016d2_remove(struct i2c_client *client)
{
    struct tpa2016d2_data *data = i2c_get_clientdata(client);

    /* Disable output on remove */
    regmap_update_bits(data->regmap, TPA2016D2_REG_CONTROL,
                       TPA2016D2_CONTROL_OUTPUT_EN, 0);
}

static const struct of_device_id tpa2016d2_of_match[] = {
    { .compatible = "ti,tpa2016d2", },
    { }
};
MODULE_DEVICE_TABLE(of, tpa2016d2_of_match);

static const struct i2c_device_id tpa2016d2_id[] = {
    { "tpa2016d2", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, tpa2016d2_id);

static struct i2c_driver tpa2016d2_driver = {
    .driver = {
        .name = "tpa2016d2",
        .of_match_table = tpa2016d2_of_match,
    },
    .probe_new = tpa2016d2_probe,
    .remove = tpa2016d2_remove,
    .id_table = tpa2016d2_id,
};

module_i2c_driver(tpa2016d2_driver);

MODULE_AUTHOR("Your Name <your@email.com>");
MODULE_DESCRIPTION("Texas Instruments TPA2016D2 Amplifier Driver");
MODULE_LICENSE("GPL");