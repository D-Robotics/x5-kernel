// SPDX-License-Identifier: GPL-2.0
/*
 * DW MIPI CSI-TX ioctl sub-module
 *
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/uaccess.h>
#include <linux/mfd/syscon.h>
#include <linux/media-bus-format.h>

#include "dw_mipi_csi.h"
#include "dw_mipi_csi_hal.h"
#include "dw_mipi_csi_ioctl.h" /* public ioctl structs */

/* data-format enum */
enum dpi_fmt {
	DPI_RGB565_1,
	DPI_RGB565_2,
	DPI_RGB565_3,
	DPI_RGB666_PACKED,
	DPI_RGB666,
	DPI_RGB888,
	DPI_YUV422,
};

/* private data */
struct csi_ioctl_dev {
	struct miscdevice mdev;
	struct mutex mtx;
	struct device *dev;
	struct dw_mipi_csi *csi;
	struct regmap *syscon;
	bool active;
	bool powered;
	struct csi_ioctl_cfg last_cfg;
};

/* route BT1120->CSI and force polarity=0 */
static void bt1120_route_ctrl(struct csi_ioctl_dev *iod, const struct csi_ioctl_cfg *cfg,
			      bool enable)
{
	u32 fmt;

	dev_info(iod->dev, "%s: bt1120_route_ctrl(enable=%d, bus_fmt=0x%x)\n", CSI_NODE_NAME,
		 enable, cfg->bus_fmt);

	/* enable/disable route */
	regmap_update_bits(iod->syscon, DISP_BT1120_2_CSITX_EN, BT1120_2_CSITX_EN,
			   enable ? BT1120_2_CSITX_EN : 0U);

	if (!enable) {
		dev_info(iod->dev, "%s: BT1120 route disabled\n", CSI_NODE_NAME);
		return;
	}

	/* set data format */
	fmt = DPI_YUV422; //we just hardcode it
	dev_info(iod->dev, "%s: setting DPI format=%u\n", CSI_NODE_NAME, fmt);
	regmap_update_bits(iod->syscon, DISP_CSITX_DPI_CFG, CSI_DPI_DATA_FORMAT, fmt);

	/* force VSYNC=0 */
	dev_info(iod->dev, "%s: forcing VSYNC=0\n", CSI_NODE_NAME);
	regmap_update_bits(iod->syscon, DISP_CSITX_DPI_CFG, CSI_DPI_VSYNC_POLARITY,
			   0U << CSI_DPI_VSYNC_POLARITY_SHIFT);

	/* force HSYNC=0 */
	dev_info(iod->dev, "%s: forcing HSYNC=0\n", CSI_NODE_NAME);
	regmap_update_bits(iod->syscon, DISP_CSITX_DPI_CFG, CSI_DPI_HSYNC_POLARITY,
			   0U << CSI_DPI_HSYNC_POLARITY_SHIFT);

	/* force DE=0 */
	dev_info(iod->dev, "%s: forcing DE=0\n", CSI_NODE_NAME);
	regmap_update_bits(iod->syscon, DISP_CSITX_DPI_CFG, CSI_DPI_DE_POLARITY, 0U);
}

/* file operations */
static int ioctl_open(struct inode *inode, struct file *filp)
{
	/* file->private_data == miscdevice set by misc_open() */
	struct miscdevice *mdev	  = filp->private_data;
	struct csi_ioctl_dev *iod = container_of(mdev, struct csi_ioctl_dev, mdev);
	struct miscdevice *mdev_i = inode->i_private;
	struct miscdevice *mdev_f = filp->private_data;

	pr_info("ioctl_open: inode->i_private = %p  filp->private_data = %p\n", mdev_i, mdev_f);
	pr_info("ioctl_open: iod=%p  iod->csi=%p\n", iod, iod->csi);
	dev_info(iod->dev, "%s: open()\n", CSI_NODE_NAME);

	if (!atomic_try_cmpxchg(&g_csi_usage, &(int){CSI_USAGE_IDLE}, CSI_USAGE_IOCTL)) {
		dev_info(iod->dev, "%s: device busy\n", CSI_NODE_NAME);
		return -EBUSY;
	}

	mutex_lock(&iod->mtx);
	iod->active = true;
	mutex_unlock(&iod->mtx);

	/* now replace private_data with our own context for later ioctls */
	filp->private_data = iod;

	dev_info(iod->dev, "%s: open success, ownership acquired\n", CSI_NODE_NAME);
	return 0;
}

static int ioctl_release(struct inode *inode, struct file *filp)
{
	struct csi_ioctl_dev *iod = filp->private_data;

	dev_info(iod->dev, "%s: release()\n", CSI_NODE_NAME);

	mutex_lock(&iod->mtx);
	if (iod->active) {
		dev_info(iod->dev, "%s: stopping HW\n", CSI_NODE_NAME);
		if (iod->powered) {
			dw_mipi_csi_hal_disable(iod->csi);
			iod->powered = false;
			dev_info(iod->dev, "%s: power_off done\n", CSI_NODE_NAME);
		}
		if (iod->last_cfg.bt1120_enable)
			bt1120_route_ctrl(iod, &iod->last_cfg, false);

		iod->active = false;
		atomic_set(&g_csi_usage, CSI_USAGE_IDLE);
		dev_info(iod->dev, "%s: released, usage idle\n", CSI_NODE_NAME);
	}
	mutex_unlock(&iod->mtx);

	return 0;
}

static long ioctl_unlocked(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct csi_ioctl_dev *iod = filp->private_data;
	struct csi_ioctl_cfg cfg;
	long ret = 0;

	dev_info(iod->dev, "%s: ioctl cmd=0x%x\n", CSI_NODE_NAME, cmd);

	if (_IOC_TYPE(cmd) != CSI_IOC_MAGIC) {
		dev_info(iod->dev, "%s: invalid magic\n", CSI_NODE_NAME);
		return -ENOTTY;
	}

	if (copy_from_user(&cfg, (void __user *)arg, sizeof(cfg))) {
		dev_info(iod->dev, "%s: copy_from_user failed\n", CSI_NODE_NAME);
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&iod->mtx))
		return -ERESTARTSYS;

	switch (cmd) {
	case CSI_IOC_INIT:
		dev_info(iod->dev, "%s: CSI_IOC_INIT\n", CSI_NODE_NAME);

		/* 1. init bt1120 to csi-tx data path */
		bt1120_route_ctrl(iod, &cfg, cfg.bt1120_enable != 0);

		/* 2. copy user parameters */
		iod->csi->lanes	  = cfg.lanes ? cfg.lanes : 4;
		iod->csi->bus_fmt = cfg.bus_fmt ? cfg.bus_fmt : DW_BUS_FMT_RGB888;
		memcpy(&iod->csi->mode, &cfg.mode, sizeof(cfg.mode));

		/* 3. init */
		iod->powered = true;
		ret	     = dw_mipi_csi_hal_init(iod->csi, CSI_VPG_OFF);
		if (ret) {
			iod->powered = false;
		}
		break;

	case CSI_IOC_SET_MODE:
		dev_info(iod->dev, "%s: CSI_IOC_SET_MODE, bt1120=%u\n", CSI_NODE_NAME,
			 cfg.bt1120_enable);
		if (!iod->powered) {
			dev_info(iod->dev, "%s: not powered, reject set_mode\n", CSI_NODE_NAME);
			ret = -EACCES;
			break;
		}
		ret = dw_mipi_csi_hal_set_mode(iod->csi, &cfg.mode);
		dev_info(iod->dev, "%s: set_mode returned %ld\n", CSI_NODE_NAME, ret);
		if (ret)
			break;

		memcpy(&iod->last_cfg, &cfg, sizeof(cfg));
		dev_info(iod->dev, "%s: mode set and route updated\n", CSI_NODE_NAME);
		break;

	default:
		dev_info(iod->dev, "%s: unknown ioctl\n", CSI_NODE_NAME);
		ret = -ENOTTY;
	}

	mutex_unlock(&iod->mtx);
	return ret;
}

static const struct file_operations csi_fops = {
	.owner		= THIS_MODULE,
	.open		= ioctl_open,
	.release	= ioctl_release,
	.unlocked_ioctl = ioctl_unlocked,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ioctl_unlocked,
#endif
};

/* create / destroy called by main dev probe/remove */
int dw_csi_ioctl_create(struct dw_mipi_csi *csi)
{
	struct csi_ioctl_dev *iod;
	int ret;

	dev_info(csi->dev, "%s: dw_csi_ioctl_create()\n", CSI_NODE_NAME);

	iod = devm_kzalloc(csi->dev, sizeof(*iod), GFP_KERNEL);
	if (!iod)
		return -ENOMEM;

	mutex_init(&iod->mtx);
	iod->dev    = csi->dev;
	iod->csi    = csi;
	iod->syscon = syscon_regmap_lookup_by_phandle(csi->dev->of_node, "verisilicon,syscon");
	if (IS_ERR(iod->syscon)) {
		ret = PTR_ERR(iod->syscon);
		dev_info(csi->dev, "%s: syscon lookup failed %d\n", CSI_NODE_NAME, ret);
		return ret;
	}

	iod->active  = false;
	iod->powered = false;

	iod->mdev.minor	 = MISC_DYNAMIC_MINOR;
	iod->mdev.name	 = CSI_NODE_NAME;
	iod->mdev.fops	 = &csi_fops;
	iod->mdev.parent = csi->dev;

	ret = misc_register(&iod->mdev);
	if (ret) {
		dev_info(csi->dev, "%s: misc_register failed %d\n", CSI_NODE_NAME, ret);
		return ret;
	}

	csi->ioctl_priv = iod;
	dev_info(csi->dev, "%s: create done\n", CSI_NODE_NAME);
	return 0;
}
EXPORT_SYMBOL_GPL(dw_csi_ioctl_create);

void dw_csi_ioctl_destroy(struct dw_mipi_csi *csi)
{
	struct csi_ioctl_dev *iod = csi->ioctl_priv;

	dev_info(csi->dev, "%s: dw_csi_ioctl_destroy()\n", CSI_NODE_NAME);

	if (iod) {
		misc_deregister(&iod->mdev);
		csi->ioctl_priv = NULL;
		dev_info(csi->dev, "%s: destroy done\n", CSI_NODE_NAME);
	}
}
EXPORT_SYMBOL_GPL(dw_csi_ioctl_destroy);

MODULE_DESCRIPTION("DW MIPI CSI-TX ioctl sub-module");
MODULE_LICENSE("GPL");
