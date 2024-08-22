#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <dt-bindings/pinctrl/horizon-hsio-pinfunc.h>
#include <dt-bindings/pinctrl/horizon-aon-pinfunc.h>
#include <dt-bindings/pinctrl/horizon-disp-pinfunc.h>
#include <dt-bindings/pinctrl/horizon-dsp-pinfunc.h>
#include <dt-bindings/pinctrl/horizon-lsio-pinfunc.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/gpio/driver.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include "gpiolib.h"
#include "gpiolib-of.h"
#include "gpiolib-acpi.h"
#include "gpiolib-cdev.h"
#include "gpiolib-sysfs.h"

static struct dentry *debug_dir;

struct pinmux_debug_info {
    const char *chip_name;
    const char *node_name;
    const char **pin_names;
    int num_pins;
};

// 定义每个控制器节点的引脚名称数组
static const char *dsp_pin_names[] = {
    "DSP_I2C7_SCL",
    "DSP_I2C7_SDA",
    "DSP_UART0_RXD",
    "DSP_UART0_TXD",
    "DSP_I2S0_MCLK",
    "DSP_I2S0_SCLK",
    "DSP_I2S0_WS",
    "DSP_I2S0_DI",
    "DSP_I2S0_DO",
    "DSP_I2S1_MCLK",
    "DSP_I2S1_SCLK",
    "DSP_I2S1_WS",
    "DSP_I2S1_DI",
    "DSP_I2S1_DO",
    "DSP_PDM_CKO",
    "DSP_PDM_IN0",
    "DSP_PDM_IN1",
    "DSP_PDM_IN2",
    "DSP_PDM_IN3",
    "DSP_SPI6_SCLK",
    "DSP_SPI6_SSN",
    "DSP_SPI6_MISO",
    "DSP_SPI6_MOSI",
};

static const char *lsio0_pin_names[] = {
    "LSIO_UART7_RX",
    "LSIO_UART7_TX",
    "LSIO_UART7_CTS",
    "LSIO_UART7_RTS",
    "LSIO_UART1_RX",
    "LSIO_UART1_TX",
    "LSIO_UART1_CTS",
    "LSIO_UART1_RTS",
    "LSIO_UART2_RX",
    "LSIO_UART2_TX",
    "LSIO_UART3_RX",
    "LSIO_UART3_TX",
    "LSIO_UART4_RX",
    "LSIO_UART4_TX",
    "LSIO_SPI0_SCLK",
    "LSIO_SPI1_SSN_1",
    "LSIO_SPI1_SCLK",
    "LSIO_SPI1_SSN",
    "LSIO_SPI1_MISO",
    "LSIO_SPI1_MOSI",
    "LSIO_SPI2_SCLK",
    "LSIO_SPI2_SSN",
    "LSIO_SPI2_MISO",
    "LSIO_SPI2_MOSI",
    "LSIO_SPI3_SCLK",
    "LSIO_SPI3_SSN",
    "LSIO_SPI3_MISO",
    "LSIO_SPI3_MOSI",
    "LSIO_SPI4_SCLK",
    "LSIO_SPI4_SSN",
    "LSIO_SPI4_MISO",
    "LSIO_SPI4_MOSI",
};

static const char *lsio1_pin_names[] = {
    "LSIO_SPI5_SCLK",
    "LSIO_SPI5_SSN",
    "LSIO_SPI5_MISO",
    "LSIO_SPI5_MOSI",
    "LSIO_SPI0_SSN",
    "LSIO_SPI0_MISO",
    "LSIO_SPI0_MOSI",
    "LSIO_I2C0_SCL",
    "LSIO_I2C0_SDA",
    "LSIO_I2C1_SCL",
    "LSIO_I2C1_SDA",
    "LSIO_I2C2_SCL",
    "LSIO_I2C2_SDA",
    "LSIO_I2C3_SCL",
    "LSIO_I2C3_SDA",
    "LSIO_I2C4_SCL",
    "LSIO_I2C4_SDA",
};

static const char *hsio0_pin_names[] = {
    "HSIO_ENET_MDC",
    "HSIO_ENET_MDIO",
    "HSIO_ENET_TXD_0",
    "HSIO_ENET_TXD_1",
    "HSIO_ENET_TXD_2",
    "HSIO_ENET_TXD_3",
    "HSIO_ENET_TXEN",
    "HSIO_ENET_TX_CLK",
    "HSIO_ENET_RX_CLK",
    "HSIO_ENET_RXD_0",
    "HSIO_ENET_RXD_1",
    "HSIO_ENET_RXD_2",
    "HSIO_ENET_RXD_3",
    "HSIO_ENET_RXDV",
    "HSIO_ENET_PHY_CLK",
    "HSIO_SD_WP",
    "HSIO_SD_CLK",
    "HSIO_SD_CMD",
    "HSIO_SD_CDN",
    "HSIO_SD_DATA0",
    "HSIO_SD_DATA1",
    "HSIO_SD_DATA2",
    "HSIO_SD_DATA3",
    "HSIO_SDIO_WP",
    "HSIO_SDIO_CLK",
    "HSIO_SDIO_CMD",
    "HSIO_SDIO_CDN",
    "HSIO_SDIO_DATA0",
    "HSIO_SDIO_DATA1",
    "HSIO_SDIO_DATA2",
    "HSIO_SDIO_DATA3",
};


static const char *hsio1_pin_names[] = {
    "HSIO_QSPI_SSN0",  
    "HSIO_QSPI_SSN1", 
    "HSIO_QSPI_SCLK", 
    "HSIO_QSPI_DATA0", 
    "HSIO_QSPI_DATA1",
    "HSIO_QSPI_DATA2", 
    "HSIO_QSPI_DATA3", 
    "HSIO_EMMC_CLK",
    "HSIO_EMMC_CMD",
    "HSIO_EMMC_DATA0",
    "HSIO_EMMC_DATA1",
    "HSIO_EMMC_DATA2",
    "HSIO_EMMC_DATA3",
    "HSIO_EMMC_DATA4",
    "HSIO_EMMC_DATA5",
    "HSIO_EMMC_DATA6",
    "HSIO_EMMC_DATA7",
    "HSIO_EMMC_RSTN",
}; 

static const char *aon_pin_names[] = {
    "AON_GPIO0_PIN0",
    "AON_GPIO0_PIN1",
    "AON_GPIO0_PIN2",
    "AON_GPIO0_PIN3",
    "AON_GPIO0_PIN4",
    "AON_ENV_VDD",
    "AON_ENV_CNN0",
    "AON_ENV_CNN1",
    "AON_PMIC_EN",
    "AON_HW_RESETN",
    "AON_RESETN_OUT",
    "AON_IRQ_OUT",
    "AON_XTAL_24M",
};

static struct pinmux_debug_info pinmux_debug_infos[] = {
    {"31000000.gpio", "aon_iomuxc", aon_pin_names, ARRAY_SIZE(aon_pin_names)},
    {"35060000.gpio", "hsio_iomuxc", hsio0_pin_names, ARRAY_SIZE(hsio0_pin_names)},
    {"35070000.gpio", "hsio_iomuxc", hsio1_pin_names, ARRAY_SIZE(hsio1_pin_names)},
    {"32150000.gpio", "dsp_iomuxc", dsp_pin_names, ARRAY_SIZE(dsp_pin_names)},
    {"34120000.gpio", "lsio_iomuxc", lsio0_pin_names, ARRAY_SIZE(lsio0_pin_names)},
    {"34130000.gpio", "lsio_iomuxc", lsio1_pin_names, ARRAY_SIZE(lsio1_pin_names)},
};

static void print_pin_info(struct seq_file *m, const char *node_name, const char *pin_name) {
    if (strncmp(node_name, "hsio_gpio0", 9) == 0 ||
        strncmp(node_name, "hsio_gpio1", 9) == 0 ||
        strncmp(node_name, "aon_gpio", 8) == 0 ||
        strncmp(node_name, "lsio_gpio0", 9) == 0 ||
        strncmp(node_name, "lsio_gpio1", 9) == 0 ||
        strncmp(node_name, "dsp_gpio0", 9) == 0 ) {
        seq_printf(m, "PinNode: %s  PinName: %s\n", node_name, pin_name);
    }
}

static void process_node(struct seq_file *m, struct device_node *np, const char **pin_names, int num_pin_names,const char *parentname) {
    struct device_node *child;
    const __be32 *pins;
    int pin_count;
    int len;

    for_each_child_of_node(np, child) {
        pins = of_get_property(child, "horizon,pins", &len);
        if (!pins) {
            seq_printf(m, "Node: %s  No 'horizon,pins' property found.\n", child->name);
            continue;
        }

        pin_count = len / sizeof(__be32);
        if (pin_count > 0) {
            int pin_value = be32_to_cpup(pins);
            if(!strcmp(parentname, "35070000.gpio")){
                pin_value-=31;
            }
            if(!strcmp(parentname, "34130000.gpio")){
                pin_value-=32;
            }
            if (pin_value >= 0 && pin_value < num_pin_names) {
                print_pin_info(m, child->name, pin_names[pin_value]);
            }
        }
    }
}

static int gpiochip_match_name(struct gpio_chip *gc, void *data)
{
    const char *name = data;
    return !strcmp(gc->label, name);
}

static struct gpio_chip *find_chip_by_name(const char *name)
{
    return gpiochip_find((void *)name, gpiochip_match_name);
}

static int pinmux_debug_show(struct seq_file *m, void *v) {
    struct pinmux_debug_info *info = m->private;
    struct device_node *np;
    struct gpio_chip *gc;
    struct gpio_device *gdev;
    struct device *parent;

    np = of_find_node_by_name(NULL, info->node_name);
    if (!np) {
        seq_printf(m, "Node %s not found\n", info->node_name);
        return -EINVAL;
    }

    gc = find_chip_by_name(info->chip_name);
    if (!gc) {
        seq_printf(m, "GPIO chip %s not found\n", info->chip_name);
        return -EINVAL;
    }
    gdev = gc->gpiodev;

    if (!gdev) {
		seq_printf(m, "%s: (dangling chip)",dev_name(&gdev->dev));
		return 0;
	}

	seq_printf(m, "%s: GPIOs %d-%d",dev_name(&gdev->dev),gdev->base, gdev->base + gdev->ngpio - 1);
	parent = gc->parent;
	if (parent)
		seq_printf(m, ", parent: %s/%s\n",
			   parent->bus ? parent->bus->name : "no-bus",
			   dev_name(parent));
    process_node(m, np, info->pin_names, info->num_pins,dev_name(parent));

    return 0;
}

static int pinmux_debug_open(struct inode *inode, struct file *file) {
    return single_open(file, pinmux_debug_show, inode->i_private);
}

static const struct file_operations pinmux_debug_fops = {
    .owner      = THIS_MODULE,
    .open       = pinmux_debug_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};

static int __init pinmux_debug_init(void) {
    int i;
    debug_dir = debugfs_create_dir("gpio_debug", NULL);
    if (!debug_dir) {
        pr_err("Failed to create debugfs directory\n");
        return -ENOMEM;
    }

    for (i = 0; i < ARRAY_SIZE(pinmux_debug_infos); i++) {
        struct pinmux_debug_info *info = &pinmux_debug_infos[i];
        debugfs_create_file(info->chip_name, 0444, debug_dir, info, &pinmux_debug_fops);
    }

    return 0;
}

static void __exit pinmux_debug_exit(void) {
    debugfs_remove_recursive(debug_dir);
}

module_init(pinmux_debug_init);
module_exit(pinmux_debug_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jinbao.he <jinbao.he@d-robotics.cc>");
MODULE_DESCRIPTION("A simple driver to parse pinmux-gpio.dtsi");
MODULE_VERSION("1.0");
