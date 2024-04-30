#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>

#include "hb_mbox.h"

static struct mbox_test_dev {
	struct mbox_chan *mchan;
	struct mbox_client pmclient;
} g_mbox_test;

static void ipc_os_handler(struct mbox_client *cl, void *mssg)
{
	pr_err("%s\n", __FUNCTION__);
}

static ssize_t store_test_cmd(struct device *dev,/*PRQA S ALL*/
	struct device_attribute *attr, const char *buf, size_t count)/*PRQA S ALL*/
{
	void *mbox_reg;
	uint32_t test_data[7];
	uint32_t cmd = 0, val = 0;
	int32_t ret = 0, i = 0, j = 0;;

	for (int i = 0; i < 7; i++) {
		test_data[i] = i;
	}
	mbox_reg = NULL;
	ret = kstrtouint(buf, 10, &cmd);
	if (ret) {
		pr_err("%s Invalid Injection. %d\n", __FUNCTION__, ret);
		return -1;
	}
	if (cmd == 1) {
		g_mbox_test.pmclient.rx_callback = ipc_os_handler;
		g_mbox_test.pmclient.dev= dev;
		g_mbox_test.pmclient.tx_prepare = NULL;
		g_mbox_test.pmclient.knows_txdone = false;
		g_mbox_test.pmclient.tx_done = NULL;

		g_mbox_test.mchan = mbox_request_channel(&g_mbox_test.pmclient, 0);
		if (IS_ERR(g_mbox_test.mchan)) {
			pr_err("ipc get mailbox channel failed\n");
			return PTR_ERR(g_mbox_test.mchan);
		}
		pr_info("%s request_all_chan ret = %d\n", __FUNCTION__, ret);
	} else if (cmd == 2) {
		mbox_free_channel(g_mbox_test.mchan);
		g_mbox_test.mchan = NULL;
		pr_info("%s release_all_chan\n", __FUNCTION__);
	} else if (cmd == 3) {
		ret = mbox_send_message(g_mbox_test.mchan, &test_data);
		if (ret) {
			pr_err("%s mbox_send_message failed, %d\n", __FUNCTION__, ret);
			return -1;
		}
		// ret = mbox_flush(g_mbox_test.mchan, timeout);
		pr_info("%s mbox_send_message\n", __FUNCTION__);
	} else if (cmd == 0) {
		mbox_reg = devm_ioremap(dev, 0x33000000, 0x1000);
		if (mbox_reg) {
			for (j = 0; j < 2; j ++) {
				for (i = 0; i <= 0x34; i +=4) {
					val = readl(mbox_reg + i + j * 0x40);
					pr_err("%s mailbox[%d][0x%x] = 0x%x\n", __FUNCTION__, j, i, val);
				}
			}
			iounmap(mbox_reg);
		} else {
			pr_err("iormap mailbox failed\n");
		}
	} else {
		pr_info("%s Not right command of %d\n", __FUNCTION__, cmd);
	}
	return count;
}

static ssize_t show_test_cmd(struct device *dev,/*PRQA S ALL*/
	struct device_attribute *attr, char *buf)/*PRQA S ALL*/
{
	return 0;
}
static DEVICE_ATTR(test_cmd, 0644, show_test_cmd, store_test_cmd); /*PRQA S ALL*/


static int32_t mbox_test_probe(struct platform_device *pdev)
{
	int32_t ret = 0;
	pr_err("mbox_test_probe\n");
	ret = device_create_file(&pdev->dev, &dev_attr_test_cmd);
	if (ret != 0) {
		pr_err("BUG: %s Can not creat test kobject\n", __FUNCTION__);
		return ret;
	}
	pr_err("mbox_test_probe done\n");
	return 0;
}

static int32_t mbox_test_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_test_cmd);
	return 0;
}

static const struct of_device_id hobot_mbox_of_id_table[] = {
	{ .compatible = "hobot,hobot-mbox-test"},
	{}
};
MODULE_DEVICE_TABLE(of, hobot_mbox_of_id_table);

static struct platform_driver mbox_test_driver = {
	.probe		= mbox_test_probe,
	.remove		= mbox_test_remove,
	.driver = {
		.name	= "hobot-mbox-test",
		.of_match_table = of_match_ptr(hobot_mbox_of_id_table),
	},
};

static int32_t __init mbox_test_damc_init(void)
{
	return platform_driver_register(&mbox_test_driver);
}
rootfs_initcall(mbox_test_damc_init);

static void __exit mbox_test_damc_exit(void)
{
	platform_driver_unregister(&mbox_test_driver);
}
module_exit(mbox_test_damc_exit);


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Synopsys Hobot Peripheral DMA Controller platform driver");
MODULE_AUTHOR("Eugeniy Paltsev <Eugeniy.Paltsev@synopsys.com>");
MODULE_AUTHOR("Horizon Robotics, Inc.");
