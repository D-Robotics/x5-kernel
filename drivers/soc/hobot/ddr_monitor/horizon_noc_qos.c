// SPDX-License-Identifier: GPL-2.0+
/*
 * rewrite Horizon Sunrise 5 Noc Qos Driver
 *
 * Copyright (C) 2023 Beijing Horizon Robotics Co.,Ltd.
 * Hualun Dong <hualun.dong@horizon.cc>
 *
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/of_platform.h>
#include <linux/clk-provider.h>

#define NOC_QOS_PRIORITY_OFFSET (0x08)

#define NOC_QOS_MODE_PRIORITY_OFFSET (0x0c - 0x08)

#define MAXPORTSNUM 5
#define MAXCLOCKSNUM 10

struct noc_qos{
	struct platform_device *pdev;
	struct device *dev;
	void __iomem *base;
	int rvalue;
	int wvalue;
	int mode;
	struct device_link * link;
	int ports_num;
	int clocks_num;
	int ports_off[MAXPORTSNUM];
	const char *clocks_names[MAXCLOCKSNUM];
	struct clk *clks[MAXCLOCKSNUM];
};

static int qos_clk_enable(struct noc_qos* nocqos)
{
	if(nocqos->clocks_num != 0)
	{
		for (size_t i = 0; i < nocqos->clocks_num; i++)
		{
			clk_prepare_enable(nocqos->clks[i]);
		}
	}
	return 0;
}

static int qos_clk_disable(struct noc_qos* nocqos)
{
	if(nocqos->clocks_num != 0)
	{
		for (size_t i = 0; i < nocqos->clocks_num; i++)
		{
			clk_disable_unprepare(nocqos->clks[i]);
		}
	}

	return 0;
}

static ssize_t priority_write_ctl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int priority = 0, ret = 0;
	struct noc_qos* nocqos = dev_get_drvdata(dev);

	ret = pm_runtime_resume_and_get(dev);
	if (ret) {
		dev_err(dev, "failed to resume qos %d\n", ret);
		return -EINVAL;
	}

	if(qos_clk_enable(nocqos)){
		dev_err(dev, "Failed to clk enable\n");
		return -1;
	}

	priority = readl(nocqos->base + NOC_QOS_PRIORITY_OFFSET);
	priority &= 0x07;

	if(qos_clk_disable(nocqos)){
		dev_err(dev, "Failed to clk disable\n");
		return -1;
	}

	ret = pm_runtime_put_sync_suspend(dev);
	if (ret) {
		dev_err(dev, "Failed to suspend %d\n", ret);
		return -EINVAL;
	}

	return sprintf(buf, "write_priority : %d\n", priority);
}

static ssize_t priority_write_ctl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int priority = 0, i;
	unsigned int zero = 0;
	unsigned int write_ctl_value = 0;
	struct noc_qos* nocqos = dev_get_drvdata(dev);
	int ports_num = nocqos->ports_num;

	ret = sscanf(buf, "%du", &write_ctl_value);
	if (write_ctl_value > 7) {
		dev_err(dev, "set value %d error,you should set 0~7\n", write_ctl_value);

		return count;
	}

	ret = pm_runtime_resume_and_get(dev);
	if (ret) {
		dev_err(dev, "failed to resume qos %d\n", ret);
		return -EINVAL;
	}

	if(qos_clk_enable(nocqos)){
		dev_err(dev, "Failed to clk enable\n");
		return -1;
	}

	nocqos->wvalue = write_ctl_value;
	nocqos->mode = 0;

	priority = readl(nocqos->base + NOC_QOS_PRIORITY_OFFSET);
	priority &= ~0x07;
	priority |= write_ctl_value;

	for(i = 0; i < ports_num; i++)
	{
		writel(zero, nocqos->base + nocqos->ports_off[i] + NOC_QOS_MODE_PRIORITY_OFFSET);
		writel(priority, nocqos->base + nocqos->ports_off[i]);
	}

	if(qos_clk_disable(nocqos)){
		dev_err(dev, "Failed to clk disable\n");
		return -1;
	}

	ret = pm_runtime_put_sync_suspend(dev);
	if (ret) {
		dev_err(dev, "Failed to suspend %d\n", ret);
		return -EINVAL;
	}

	return count;
}

static struct device_attribute priority_write_ctl = __ATTR(priority, 0664,
						   priority_write_ctl_show, priority_write_ctl_store);

static struct attribute *priority_write_qctrl_attrs[] = {
	&priority_write_ctl.attr,
	NULL,
};

static struct attribute_group priority_write_attr_group = {
	.name = "write_priority_qos_ctrl",
	.attrs = priority_write_qctrl_attrs,
};

static ssize_t priority_read_ctl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int priority = 0, ret = 0;
	struct noc_qos* nocqos = dev_get_drvdata(dev);

	ret = pm_runtime_resume_and_get(dev);
	if (ret) {
		dev_err(dev, "failed to resume qos %d\n", ret);
		return -EINVAL;
	}

	if(qos_clk_enable(nocqos)){
		dev_err(dev, "Failed to clk enable\n");
		return -1;
	}

	priority = readl(nocqos->base + NOC_QOS_PRIORITY_OFFSET);
	priority >>= 8;
	priority &= 0x07;

	if(qos_clk_disable(nocqos)){
		dev_err(dev, "Failed to clk disable\n");
		return -1;
	}

	ret = pm_runtime_put_sync_suspend(dev);
	if (ret) {
		dev_err(dev, "Failed to suspend %d\n", ret);
		return -EINVAL;
	}

	return sprintf(buf, "read_priority : %d\n", priority);
}

static ssize_t priority_read_ctl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int priority = 0, i;
	unsigned int zero = 0;
	unsigned int read_ctl_value = 0;
	struct noc_qos* nocqos = dev_get_drvdata(dev);
	int ports_num = nocqos->ports_num;

	ret = sscanf(buf, "%du", &read_ctl_value);
	if (read_ctl_value > 7) {
		dev_err(dev, "set value %d error,you should set 0~7\n", read_ctl_value);

		return count;
	}

	ret = pm_runtime_resume_and_get(dev);
	if (ret) {
		dev_err(dev, "failed to resume qos %d\n", ret);
		return -EINVAL;
	}

	if(qos_clk_enable(nocqos)){
		dev_err(dev, "Failed to clk enable\n");
		return -1;
	}

	nocqos->rvalue = read_ctl_value;
	nocqos->mode = 0;

	priority = readl(nocqos->base + NOC_QOS_PRIORITY_OFFSET);
	priority &= ~(0x07 << 8);
	priority |= (read_ctl_value << 8);

	for(i = 0; i < ports_num; i++)
	{
		writel(zero, nocqos->base + nocqos->ports_off[i] + NOC_QOS_MODE_PRIORITY_OFFSET);
		writel(priority, nocqos->base + nocqos->ports_off[i]);
	}

	if(qos_clk_disable(nocqos)){
		dev_err(dev, "Failed to clk disable\n");
		return -1;
	}

	ret = pm_runtime_put_sync_suspend(dev);
	if (ret) {
		dev_err(dev, "Failed to suspend %d\n", ret);
		return -EINVAL;
	}

	return count;
}

static struct device_attribute priority_read_ctl = __ATTR(priority, 0600,
						   priority_read_ctl_show, priority_read_ctl_store);

static struct attribute *priority_read_qctrl_attrs[] = {
	&priority_read_ctl.attr,
	NULL,
};

static struct attribute_group priority_read_attr_group = {
	.name = "read_priority_qos_ctrl",
	.attrs = priority_read_qctrl_attrs,
};

static const struct attribute_group *noc_qos_attr_groups[] = {
	&priority_write_attr_group,
	&priority_read_attr_group,
	NULL,
};

void noc_qos_restore(struct device *dev)
{
	unsigned int priority = 0, i;
	struct noc_qos* nocqos = dev_get_drvdata(dev);
	unsigned int mode = nocqos->mode;
	unsigned int read_ctl_value = nocqos->rvalue;
	unsigned int write_ctl_value = nocqos->wvalue;
	int ports_num = nocqos->ports_num;

	priority |= write_ctl_value;
	priority |= (read_ctl_value << 8);

	if(qos_clk_enable(nocqos)){
		dev_err(dev, "Failed to clk enable\n");
		return;
	}

	for(i = 0; i < ports_num; i++)
	{
		writel(mode, nocqos->base + nocqos->ports_off[i] + NOC_QOS_MODE_PRIORITY_OFFSET);
		writel(priority, nocqos->base + nocqos->ports_off[i]);
	}

	if(qos_clk_disable(nocqos)){
		dev_err(dev, "Failed to clk disable\n");
		return;
	}

	return;
}

static int __maybe_unused noc_qos_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused noc_qos_resume(struct device *dev)
{
	noc_qos_restore(dev);

	return 0;
}

static int __maybe_unused noc_qos_runtime_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused noc_qos_runtime_resume(struct device *dev)
{
	return noc_qos_resume(dev);
}

static const struct dev_pm_ops noc_qos_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(noc_qos_suspend, noc_qos_resume)
		SET_RUNTIME_PM_OPS(noc_qos_runtime_suspend, noc_qos_runtime_resume, NULL)};

struct device *drobot_get_qos_consumer_dev(const struct device_node *of_node)
{
	struct platform_device *pdev;
	struct device_node *np;

	np = of_parse_phandle(of_node, "consumer-dev", 0);
	if (!np)
		return NULL;

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev)
		return ERR_PTR(-ENODEV);

	return &pdev->dev;
}

static int noc_qos_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct noc_qos* nocqos = NULL;
	struct device *consumer_dev = NULL;
	int p_value = 0, m_value = 0;

	nocqos = devm_kzalloc(&pdev->dev, sizeof(struct noc_qos), GFP_KERNEL);
	if(!nocqos) {
		return -ENOMEM;
	}

	nocqos->dev = &pdev->dev;

	nocqos->base = devm_platform_ioremap_resource(pdev, 0);
	if(IS_ERR(nocqos->base))
		return PTR_ERR(nocqos->base);

	if (of_property_read_u32(dev->of_node, "clocks-num", &nocqos->clocks_num))
	{
		dev_err(dev, "qos clocks-num parse failed!\n");
		return -EINVAL;
	}

	if(nocqos->clocks_num != 0)
	{
		ret = of_property_read_string_array(dev->of_node, "clock-names",
							nocqos->clocks_names, nocqos->clocks_num);
		if (ret != nocqos->clocks_num) {
			dev_err(&pdev->dev, "failed to read clock-output-names\n");
			return -EINVAL;
		}

		for (size_t i = 0; i < nocqos->clocks_num; i++)
		{
			nocqos->clks[i] = devm_clk_get(&pdev->dev, nocqos->clocks_names[i]);
			if (IS_ERR(nocqos->clks[i]) || (nocqos->clks[i] == NULL)) {
				/* some platform not has aclk, so just report error info */
				nocqos->clks[i] = NULL;
				dev_err(&pdev->dev, "failed get %s\n", nocqos->clocks_names[i]);
			}
		}
	}

	consumer_dev = drobot_get_qos_consumer_dev(dev->of_node);
	if(!consumer_dev) {
		dev_info(dev, "qos doesn't have consumer device\n");
	} else if(IS_ERR(consumer_dev)) {
		dev_info(dev, "consumer device is disabled, so qos isn't supported\n");
		return -1;
	} else {
		nocqos->link = device_link_add(consumer_dev, dev, DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		if(IS_ERR(nocqos->link)){
			dev_err(consumer_dev, "%s:%d\n", __func__, __LINE__);
		}
	}

	if (of_property_read_u32(dev->of_node, "ports-num", &nocqos->ports_num))
	{
		dev_err(dev, "qos port-num parse failed!\n");
		return -EINVAL;
	}

	if (of_property_read_u32_array(dev->of_node, "ports-off", nocqos->ports_off, nocqos->ports_num))
	{
		dev_err(dev, "qos ports-off parse failed!\n");
		return -EINVAL;
	}

	ret = sysfs_create_groups(&pdev->dev.kobj, noc_qos_attr_groups);

	pm_runtime_get_noresume(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	if(qos_clk_enable(nocqos)){
		dev_err(dev, "Failed to clk enable\n");
		return -1;
	}

	p_value = readl(nocqos->base + NOC_QOS_PRIORITY_OFFSET);
	m_value = readl(nocqos->base + NOC_QOS_PRIORITY_OFFSET + NOC_QOS_MODE_PRIORITY_OFFSET);
	nocqos->rvalue = (p_value >> 8) & 0x07;
	nocqos->wvalue = p_value & 0x07;
	nocqos->mode = m_value;

	if(qos_clk_disable(nocqos)){
		dev_err(dev, "Failed to clk disable\n");
		return -1;
	}

	ret = pm_runtime_put_sync_suspend(dev);
	if (ret) {
		dev_err(dev, "Failed to suspend %d\n", ret);
		return -1;
	}
	dev_set_drvdata(&pdev->dev, nocqos);

	dev_info(dev, "noc qos init finished.");

	return 0;
}

static int noc_qos_remove(struct platform_device *pdev)
{
	struct noc_qos* nocqos = dev_get_drvdata(&pdev->dev);
	if(nocqos->link) {
		device_link_del(nocqos->link);
	}

	dev_info(&pdev->dev, "noc qos remove finished.");

	return 0;
}

static const struct of_device_id noc_qos_match[] = {
	{ .compatible = "d-robotics,noc_qos" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, noc_qos_match);

static struct platform_driver noc_qos_driver = {
	.probe	= noc_qos_probe,
	.remove = noc_qos_remove,
	.driver = {
		.name	= "noc_qos",
        .of_match_table = noc_qos_match,
        .pm = &noc_qos_pm_ops, 
	}
};

static int __init noc_qos_init(void)
{
	return platform_driver_register(&noc_qos_driver);
}
subsys_initcall(noc_qos_init);

MODULE_AUTHOR("Hualun Dong");
MODULE_DESCRIPTION("Horizon X5 Noc Qos Driver");
MODULE_LICENSE("GPL v2");