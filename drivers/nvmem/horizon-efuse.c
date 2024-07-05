
// SPDX-License-Identifier: GPL-2.0-only
/*
 * Horizon eFuse Driver
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#define REG_EFUSE_MODULE_ID 	0x0000
#define REG_EFUSE_MODULE_KEY0 	0x0004
#define REG_EFUSE_MODULE_KEY1 	0x0008
#define REG_EFUSE_MODULE_KEY2 	0x000c
#define REG_EFUSE_MODULE_KEY3 	0x0010
#define REG_EFUSE_DEVICE_ID 	0x0014
#define REG_EFUSE_LCS 		0x0048
#define REG_EFUSE_LOCK_CTRL 	0x004c
#define REG_EFUSE_NONSEC_0 		0x0050
#define REG_EFUSE_NONSEC_1 		0x0054
#define REG_EFUSE_NONSEC_2 		0x0058
#define REG_EFUSE_NONSEC_3 		0x005c
#define REG_EFUSE_NONSEC_4 		0x0060
#define REG_EFUSE_NONSEC_5 		0x0064
#define REG_EFUSE_NONSEC_6 		0x0068
#define REG_EFUSE_NONSEC_7 		0x006c
#define REG_EFUSE_NONSEC_8 		0x0070
#define REG_EFUSE_NONSEC_9 		0x0074
#define BANK_MASK			GENMASK(9, 0)
#define HORIZON_EFUSE_NBYTES		4

struct horizon_efuse_chip {
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
};

static int horizon_efuse_read(void *context, unsigned int offset,
				      void *val, size_t bytes)
{
	struct horizon_efuse_chip *efuse = context;
	u8 *out_value, *read_ptr;
	u32 read_value;
	int ret = 0;
	ret = clk_prepare_enable(efuse->clk);
	if (ret < 0) {
		dev_err(efuse->dev, "failed to prepare/enable efuse clk\n");
		return ret;
	}
	out_value = (u8 *)kzalloc(bytes, GFP_KERNEL);
	if (IS_ERR_OR_NULL(out_value))
		return -ENOMEM;

	read_value = readl(efuse->base + offset);
	read_value &= ~BANK_MASK;
	writel(read_value, efuse->base + offset);
	read_ptr = out_value;
	while (bytes) {
		*read_ptr = readb(efuse->base + offset);
		read_ptr++;
		bytes--;
	}
	memcpy(val, out_value, bytes);
	kfree(out_value);
	clk_disable_unprepare(efuse->clk);
	return ret;
}

static struct nvmem_config econfig = {
	.name = "horizon-efuse",
	.stride = 1,
	.word_size = 1,
	.read_only = true,
};

static const struct of_device_id horizon_efuse_match[] = {
	{
		.compatible = "horizon,tef12fcll64x32hd18phrm-efuse",
		.data = (void *)&horizon_efuse_read,
	},
	{},
};
MODULE_DEVICE_TABLE(of, horizon_efuse_match);

static int horizon_efuse_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct nvmem_device *nvmem;
	struct horizon_efuse_chip *efuse;
	const void *data;
	struct device *dev = &pdev->dev;
	data = of_device_get_match_data(dev);
	if (!data) {
		dev_err(dev, "failed to get match data\n");
		return -EINVAL;
	}
	efuse = devm_kzalloc(dev, sizeof(struct horizon_efuse_chip),
			     GFP_KERNEL);
	if (!efuse)
		return -ENOMEM;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	efuse->base = ioremap(res->start, resource_size(res));
	if (IS_ERR(efuse->base))
		return PTR_ERR(efuse->base);
	efuse->clk = devm_clk_get(dev, "pclk_efuse");
	if (IS_ERR(efuse->clk))
		return PTR_ERR(efuse->clk);
	efuse->dev = dev;
	if (of_property_read_u32(dev->of_node, "horizon,efuse-size",
				 &econfig.size))
		econfig.size = resource_size(res);
	econfig.reg_read = horizon_efuse_read;
	econfig.priv = efuse;
	econfig.dev = efuse->dev;
	nvmem = devm_nvmem_register(dev, &econfig);
	return PTR_ERR_OR_ZERO(nvmem);
}

static struct platform_driver horizon_efuse_driver = {
	.probe = horizon_efuse_probe,
	.driver = {
		.name = "horizon-efuse",
		.of_match_table = horizon_efuse_match,
	},
};

module_platform_driver(horizon_efuse_driver);
MODULE_DESCRIPTION("horizon_efuse driver");
MODULE_LICENSE("GPL v2");
