/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/uio_driver.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#include "ipc_shm.h"
#include "ipc_os.h"
#include "mbox_platform.h"

#define UIO_DRIVER_NAME	"ipc-shm-uio"
#define DRIVER_VERSION	"1.0.0"
#define NODE_NAME_MAX_LEN (32)
#define MAX_UIO_DEVICE (32)
#define UIO_DTS_MBOX_INDEX (0)
#define DATA_NUM_MAX (7u)
#define MAX_UIO_NAME_LEN (32)
#ifndef BIT
#define BIT(n) (1 << n)
#endif /*BIT*/
#define SYNC_BIT (0)
#define WAIT_BIT (2)
#define SLEEP_WAIT_ACK (0)
#define SPIN_WAIT_ACK (BIT(WAIT_BIT))
#define DEFAULT_TIMEOUT (1)

/* ioctl*/
#define IPC_UIO_MAJOR (1)
#define IPC_UIO_MINOR (0)
#define IPC_IOC_MAGIC 'i'/**< ioctl magic number*/
#define IPC_IOC_NR (0)


/* IPC-UIO commands */
/* open mailbox, transform mailbox information by uio device */
#define IPC_UIO_CMD_RESERVED (0)
#define IPC_UIO_CMD_OPEN_TYPE (0)
#define IPC_UIO_CMD_OPENID_OPEN_MBOX (0)
#define IPC_UIO_CMD_OPENID_MBOX_IDX (1)
#define IPC_UIO_CMD_OPENID_TRANS_FLAGS (2)
#define IPC_UIO_CMD_OPENID_TIMEOUT (3)

/* close mailbox by uio device */
#define IPC_UIO_CMD_CLOSE_TYPE (1)

/* send notification by uio device */
#define IPC_UIO_CMD_NOTIFY_TYPE (2)

union uio_cmd_type{
	int32_t cmd;
	struct {
		uint8_t cmd_type;
		uint8_t cmd_id;
		int16_t data;
	} info;
};

/**
 * @struct ipc_ver_check_info
 * Define the descriptor of IPC version check
 * @NO{S17E09C06}
 */
struct ipc_ver_check_info {
	uint32_t major;/**< the major version number*/
	uint32_t minor;/**< the minor version number*/
};

/**
 * @struct ipc_uio_def_cfg
 * Define the descriptor of IPC default config
 * @NO{S17E09C06}
 */
struct ipc_uio_def_cfg {
	uint64_t local_shm_addr;/**< address of local share memory*/
	uint64_t remote_shm_addr;/**< address of remote share memory*/
	uint32_t shm_size;/**< size of share memory, remote size is equal to local size*/
	uint16_t num_bufs;/**< number of buffers*/
	uint32_t buf_size;/**< size of buffers*/
};

/**
 * @struct ipc_uio_priv
 * Define the descriptor of IPCF SHM UIO device data
 * @NO{S17E09C06}
 */
struct ipc_uio_priv {
	struct device *dev;
	atomic_t refcnt;
	struct uio_info info;
	int32_t mbox_chan_idx;
	struct mbox_client mclient;
	struct mbox_chan *mchan;
	int32_t trans_flags;
	uint32_t timeout;
	struct ipc_instance_cfg ipc_info;
	dev_t dev_num;
	struct cdev ipc_cdev;
	struct class *ipc_class;
	struct device *ipc_device;
};

static void *uio_priv[MAX_UIO_DEVICE] = {
	NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL,
};

void ipc_uio_rx_handler(struct mbox_client *cl, void *mssg)
{
	struct ipc_uio_priv *priv = dev_get_drvdata(cl->dev);

	uio_event_notify(&priv->info);
}

static int32_t ipc_shm_uio_open(struct uio_info *info, struct inode *inode)
{
	struct ipc_uio_priv *priv = info->priv;

	if (!atomic_dec_and_test(&priv->refcnt)) {
		dev_err(priv->dev, "%s device already opened\n", info->name);
		atomic_inc(&priv->refcnt);

		return -EBUSY;
	}

	return 0;
}

static int32_t ipc_shm_uio_release(struct uio_info *info, struct inode *inode)
{
	struct ipc_uio_priv *priv = info->priv;

	atomic_inc(&priv->refcnt);

	return 0;
}

static int32_t ipc_shm_uio_open_mbox(struct uio_info *dev_info, int32_t cmd)
{
	union uio_cmd_type uio_cmd;
	struct ipc_uio_priv *priv = (struct ipc_uio_priv *)(&dev_info->priv);
	struct mbox_client *pmclient = &priv->mclient;
	struct mbox_chan *mchan;
	int32_t err = 0;

	ipc_dbg("open start\n");
	if (!priv) {
		ipc_err("dev info invalid\n");

		return -EINVAL;
	}
	pmclient = &priv->mclient;
	if (pmclient == NULL || pmclient->dev == NULL) {
		ipc_err("pmclient or pmclient->dev invalid\n");

		return -EINVAL;
	}
	ipc_dbg("devname %s\n", dev_name(pmclient->dev));
	uio_cmd.cmd = cmd;
	switch (uio_cmd.info.cmd_id) {
	case IPC_UIO_CMD_OPENID_OPEN_MBOX:
		ipc_dbg("reuquest channel\n");
		mchan = mbox_request_channel(pmclient, UIO_DTS_MBOX_INDEX);
		if (IS_ERR(mchan)) {
			dev_err(priv->dev, "UIO get mailbox channel failed\n");

			return PTR_ERR(mchan);
		}
		err = hb_mbox_cfg_trans_flags(mchan, priv->trans_flags);
		if (err < 0) {
			dev_err(priv->dev, "UIO config transfer flags %d failed: %d\n", priv->trans_flags, err);

			return err;
		}
		ipc_dbg("open mbox success\n");

		break;
	case IPC_UIO_CMD_OPENID_MBOX_IDX:
		break;
	case IPC_UIO_CMD_OPENID_TRANS_FLAGS:
		priv->trans_flags = (int32_t)uio_cmd.info.data;
		if ((priv->trans_flags & BIT(WAIT_BIT)) == SPIN_WAIT_ACK) {
			pmclient->tx_block = false;
		}
		else {
			pmclient->tx_block = true;
		}

		pmclient->tx_block = true;//test fpga
		ipc_dbg("trans_flags %d\n", priv->trans_flags);

		break;
	case IPC_UIO_CMD_OPENID_TIMEOUT:
		if (uio_cmd.info.data == 0) {
			pmclient->tx_tout = DEFAULT_TIMEOUT;
			priv->timeout = DEFAULT_TIMEOUT;
		}
		else {
			pmclient->tx_tout = (uint32_t)uio_cmd.info.data;
			priv->timeout = (uint32_t)uio_cmd.info.data;
		}

		ipc_dbg("timeout %d\n", priv->timeout);

		break;
	default:
		break;
	}

	return 0;
}

static int32_t ipc_shm_uio_close_mbox(struct uio_info *dev_info, int32_t cmd)
{
	struct ipc_uio_priv *priv = (struct ipc_uio_priv *)(&dev_info->priv);

	if (priv->mchan == NULL) {
		dev_err(priv->dev, "UIO mailbox clsoe invalid parameter\n");

		return -EINVAL;
	}

	mbox_free_channel(priv->mchan);

	return 0;
}

static int32_t ipc_shm_uio_notify_mbox(struct uio_info *dev_info, int32_t cmd)
{
	int32_t err;
	struct ipc_uio_priv *priv = (struct ipc_uio_priv *)(&dev_info->priv);

	if (priv->mchan == NULL) {
		dev_err(priv->dev, "UIO mailbox notify invalid parameter\n");

		return -EINVAL;
	}
	err = mbox_send_message(priv->mchan, NULL);
	if (err < 0) {
		dev_err(priv->dev, "UIO mailbox notify failed\n");

		return err;
	}

	if ((priv->trans_flags & BIT(WAIT_BIT)) == SPIN_WAIT_ACK) {
		err = mbox_flush(priv->mchan, priv->timeout);
		if (err < 0) {
			ipc_err("ipc mailbox no ack : %d\n", err);

			return err;
		}
	}

	return 0;
}

static int32_t ipc_shm_uio_irqcontrol(struct uio_info *dev_info, int32_t cmd)
{
	union uio_cmd_type uio_cmd;
	int32_t err = 0;

	uio_cmd.cmd = cmd;

	ipc_dbg("uio_cmd type %d id %d data %d\n", uio_cmd.info.cmd_type, uio_cmd.info.cmd_id, uio_cmd.info.data);

	switch (uio_cmd.info.cmd_type) {
	case IPC_UIO_CMD_OPEN_TYPE:
		err = ipc_shm_uio_open_mbox(dev_info, cmd);

		break;
	case IPC_UIO_CMD_CLOSE_TYPE:
		err = ipc_shm_uio_close_mbox(dev_info, cmd);

		break;
	case IPC_UIO_CMD_NOTIFY_TYPE:
		err = ipc_shm_uio_notify_mbox(dev_info, cmd);

		break;
	default:
		break;
	}

	return err;
}

static int32_t get_ipc_def_resource(struct platform_device *pdev,
		     struct device_node *ipc_node,
		     struct ipc_instance_info_m1 *def_info)
{
	int32_t err = 0, i, j;
	struct device *dev = &pdev->dev;
	struct device_node *chan_node;
	struct device_node *pool_node;
	char node_name[NODE_NAME_MAX_LEN];
	struct ipc_channel_info *chans, *chancfg;
	struct ipc_pool_info *pools, *poolcfg;

	if (!dev || !ipc_node || !def_info) {
		dev_err(dev,"%s invalid parameter\n", __func__);

		return -EINVAL;
	}

	err = of_property_read_s32(ipc_node, "num_chans", &def_info->num_chans);
	if (err != 0) {
		dev_err(dev, "ipc chan number read failed %d\n", err);

		return err;
	}

	if ((def_info->num_chans <= 0) ||
	    (def_info->num_chans > MAX_NUM_CHAN_PER_INSTANCE)) {
		dev_err(dev, "ipc num_chans invalid\n");

		return -EINVAL;
	}

	err = of_property_read_u32(ipc_node, "shm_size", &def_info->shm_size);
	if (err != 0) {
		dev_err(dev, "ipc shm_size read failed %d\n", err);

		return err;
	}

	if (def_info->shm_size == 0) {
		dev_err(dev, "ipc shm_size invalid\n");

		return -EINVAL;
	}
	err = of_property_read_u64(ipc_node, "local_shm_addr", &def_info->local_shm_addr);
	if (err != 0) {
		dev_err(dev, "ipc local_shm_addr read failed %d\n", err);

		return err;
	}

	if (def_info->local_shm_addr == 0) {
		dev_err(dev, "ipc local_shm_addr invalid\n");

		return -EINVAL;
	}

	err = of_property_read_u64(ipc_node, "remote_shm_addr", &def_info->remote_shm_addr);
	if (err != 0) {
		dev_err(dev, "ipc remote_shm_addr read failed %d\n", err);

		return err;
	}

	if (def_info->remote_shm_addr == 0) {
		dev_err(dev, "ipc remote_shm_addr invalid\n");

		return -EINVAL;
	}

	chans = devm_kzalloc(dev, sizeof(*chans) * def_info->num_chans, GFP_KERNEL);
	if (!chans) {
		dev_err(dev, "ipc channel alloc failed \n");

		return -ENOMEM;
	}

	def_info->chans = chans;


	for (i = 0; i < def_info->num_chans; i++){
		memset(node_name, '\0', sizeof(node_name));
		err = snprintf(node_name, NODE_NAME_MAX_LEN, "ipc_channel%d", i);
		if (err < 0){
			dev_err(dev, "ipc channel %d node name generate failed %d\n", i, err);

			return err;
		}
		chan_node = of_get_child_by_name(ipc_node, node_name);
		if (!chan_node){
			dev_err(dev, "ipc channel %d child node get failed\n", i);

			return -ENODEV;
		}
		chancfg = def_info->chans + i;
		err = of_property_read_s32(chan_node, "num_pools", &chancfg->num_pools);
		if (err != 0) {
			dev_err(dev, "ipc channel %d num_pools read failed %d\n", i, err);

			goto err_chan_node;
		}

		if (chancfg->num_pools <= 0 ||
		    chancfg->num_pools > MAX_NUM_POOL_PER_CHAN) {
			dev_err(dev, "ipc num_pools invalid\n");
			err = -EINVAL;

			goto err_chan_node;
		}
		/** get ipc pool resource*/
		pools = devm_kzalloc(dev, sizeof(*pools) * chancfg->num_pools, GFP_KERNEL);
		if (!pools) {
			dev_err(dev, "ipc alloc pools memory failed\n");
			err = -ENOMEM;

			goto err_chan_node;
		}
		chancfg->pools = pools;
		for (j = 0; j < chancfg->num_pools; j++) {
			memset(node_name, '\0', sizeof(node_name));
			err = snprintf(node_name, NODE_NAME_MAX_LEN, "ipc_pool%d", j);
			if (err < 0){
				dev_err(dev, "ipc pool %d node name generate failed %d\n", j, err);

				goto err_chan_node;
			}

			pool_node = of_get_child_by_name(chan_node, node_name);
			if (!pool_node){
				dev_err(dev, "ipc pool %d child node get failed: %d\n", j, err);
				err = -ENODEV;

				goto err_chan_node;
			}

			poolcfg = pools + j;
			err = of_property_read_u16(pool_node, "num_bufs", &poolcfg->num_bufs);
			if (err != 0) {
				dev_err(dev, "ipc pool %d num_bufs read failed %d\n", j, err);

				goto err_pool_node;
			}

			if (poolcfg->num_bufs == 0 ||
			    poolcfg->num_bufs > MAX_NUM_BUF_PER_POOL) {
				dev_err(dev, "ipc num_bufs invalid\n");
				err = -EINVAL;

				goto err_pool_node;
			}

			err = of_property_read_u32(pool_node, "buf_size", &poolcfg->buf_size);
			if (err != 0) {
				dev_err(dev, "ipc pool %d buf_size read failed %d\n", j, err);

				goto err_pool_node;
			}

			if (poolcfg->buf_size == 0) {
				dev_err(dev, "ipc buf_size invalid\n");
				err = -EINVAL;

				goto err_pool_node;
			}

			of_node_put(pool_node);

		}

		of_node_put(chan_node);
	}

	return 0;

err_pool_node:
	of_node_put(pool_node);

err_chan_node:
	of_node_put(chan_node);

	return err;
}

static int32_t ioctl_check_version(struct ipc_ver_check_info __user* pver)
{
	int32_t ret = 0;
	struct ipc_ver_check_info ver = {
		.major = IPC_UIO_MAJOR,
		.minor = IPC_UIO_MINOR,
	};

	if (0 == access_ok(pver, sizeof(struct ipc_ver_check_info))) {
		ipc_err("pver is invalid\n");

		return -EINVAL;
	}

	ret = copy_to_user(pver, &ver, sizeof(struct ipc_ver_check_info));
	if (ret < 0) {
		ipc_err("copy_to_user failed, error: %d\n", ret);

		return -EFAULT;
	}

	return 0;
}

static int32_t ipc_chrdev_open(struct inode *inode, struct file *filp)
{
	int32_t i;
	uint32_t major;
	struct ipc_uio_priv *priv;

	for (i = 0; i < MAX_UIO_DEVICE; i++) {
		if (uio_priv[i] == NULL)
			continue;

		priv = (struct ipc_uio_priv *)uio_priv[i];
		major = MAJOR(priv->dev_num);
		if (major == MAJOR(inode->i_rdev)) {
			filp->private_data = (void *)&priv->ipc_info.info.custom_cfg;

			return 0;
		}
	}

	pr_err("ipc char device open failed\n");

	return -ENODEV;
}

static ssize_t ipc_chrdev_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	// Implement read logic here
	struct ipc_uio_def_cfg uio_def_info;
	struct ipc_instance_info_m1 *def_info = (struct ipc_instance_info_m1 *)filp->private_data;

	uio_def_info.local_shm_addr = def_info->local_shm_addr;
	uio_def_info.remote_shm_addr = def_info->remote_shm_addr;
	uio_def_info.shm_size = def_info->shm_size;
	uio_def_info.num_bufs = def_info->chans->pools->num_bufs;
	uio_def_info.buf_size = def_info->chans->pools->buf_size;

	if (len < sizeof(struct ipc_uio_def_cfg)) {
		pr_err("ipc uio read length invalid\n");
		return -EINVAL;
	}

	if (copy_to_user(buf, &uio_def_info, sizeof(struct ipc_uio_def_cfg)))
		return -EFAULT;

	return sizeof(struct ipc_uio_def_cfg);
}

static int32_t ipc_chrdev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t ipc_chrdev_ioctl(struct file *filp, uint32_t cmd, unsigned long arg)
{
	int ret = 0;
	if(_IOC_TYPE(cmd)!=IPC_IOC_MAGIC){
		return -EINVAL;
	}
	// cmd direction
	switch(_IOC_NR(cmd)){
	case IPC_IOC_NR :
		ret = ioctl_check_version((struct ipc_ver_check_info __user*)arg);
		break;
	default :
		ret = -EINVAL;
		ipc_err("cmd is invalid\n");
	}

	return ret;
}

static struct file_operations ipc_chrdev_fops = {
	.owner = THIS_MODULE,
	.open = ipc_chrdev_open,
	.read = ipc_chrdev_read,
	.release = ipc_chrdev_release,
	.unlocked_ioctl = ipc_chrdev_ioctl,
	.compat_ioctl = ipc_chrdev_ioctl,
};

static int32_t ipc_shm_uio_probe(struct platform_device *pdev)
{
	struct ipc_uio_priv *priv;
	struct device *dev = &pdev->dev;
	struct device_node *ipc_uio_node;
	struct ipc_instance_info_m1 *def_info;
	char *uio_name;
	int32_t err;
	int32_t ipc_uio_id;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (IS_ERR_OR_NULL(priv)) {
		dev_err(dev, "ipc uio dev alloc failed\n");

		return -ENOMEM;
	}

	uio_name = devm_kzalloc(&pdev->dev, MAX_UIO_NAME_LEN, GFP_KERNEL);
	if (IS_ERR_OR_NULL(uio_name)) {
		dev_err(dev, "ipc uio dev name alloc failed\n");

		return -ENOMEM;
	}

	if (!of_device_is_compatible(dev->of_node, "hobot,ipc-uio")){
		dev_err(dev, "ipc dev not found node: %s\n", "hobot,ipc-uio");

		return -ENODEV;
	}

	ipc_uio_node = dev->of_node;
	def_info = &priv->ipc_info.info.custom_cfg;
	err = get_ipc_def_resource(pdev, ipc_uio_node, def_info);
	if (err != 0) {
		dev_err(dev, "ipc channel initialize failed %d\n", err);

		return err;
	}

	platform_set_drvdata(pdev, priv);
	priv->dev = dev;
	dev_set_drvdata(dev, priv);

	/* generate uio name: UIO_DRIVER_NAME + mbox_chan_idx*/
	err = of_property_read_s32(ipc_uio_node, "mbox_chan_idx", &priv->mbox_chan_idx);
	if (err != 0) {
		dev_err(dev, "ipc-uio mbox_chan_idx read failed %d\n", err);

		return err;
	}

	if (priv->mbox_chan_idx < 0) {
		dev_err(dev, "ipc-uio mbox_chan_idx invalid\n");

		return -EINVAL;
	}

	memset(uio_name, '\0', MAX_UIO_NAME_LEN);
	err = snprintf(uio_name, MAX_UIO_NAME_LEN, UIO_DRIVER_NAME"%d", priv->mbox_chan_idx);
	if (err < 0){
		dev_err(dev, "ipc uio dev name generate failed %d\n", err);

		return err;
	}

	/* Register UIO device */
	atomic_set(&priv->refcnt, 1);
	priv->info.version = DRIVER_VERSION;
	priv->info.name = uio_name;
	priv->info.irq = UIO_IRQ_CUSTOM;
	priv->info.irq_flags = 0;
	priv->info.handler = NULL;
	priv->info.irqcontrol = ipc_shm_uio_irqcontrol;
	priv->info.open = ipc_shm_uio_open;
	priv->info.release = ipc_shm_uio_release;
	priv->info.priv = priv;

	err = uio_register_device(priv->dev, &priv->info);
	if (err) {
		dev_err(priv->dev, "UIO registration failed\n");

		return err;
	}

	/* Register ipc char device*/
	// Allocate device numbers
	err = alloc_chrdev_region(&priv->dev_num, 0, 1, uio_name);
	if (err < 0) {
		dev_err(&pdev->dev, "ipc char device %s register failed\n", uio_name);
		uio_unregister_device(&priv->info);

		return err;
	}

	// Initialize cdev structure
	cdev_init(&priv->ipc_cdev, &ipc_chrdev_fops);

	// Add cdev to kernel
	cdev_add(&priv->ipc_cdev, priv->dev_num, 1);
	if (err < 0) {
		dev_err(&pdev->dev, "ipc char device %s add failed\n", uio_name);
		unregister_chrdev_region(priv->dev_num, 1);
		uio_unregister_device(&priv->info);

		return err;
	}

	// Create class and device
	priv->ipc_class = class_create(THIS_MODULE, uio_name);
	if (IS_ERR(priv->ipc_class)) {
		dev_err(&pdev->dev, "ipc creat class failed\n");
		err = PTR_ERR(priv->ipc_class);
		cdev_del(&priv->ipc_cdev);
		unregister_chrdev_region(priv->dev_num, 1);
		uio_unregister_device(&priv->info);

		return err;
	}

	priv->ipc_device = device_create(priv->ipc_class, NULL, priv->dev_num, NULL, uio_name);
	if (IS_ERR(priv->ipc_device)) {
		dev_err(&pdev->dev, "ipc creat device failed\n");
		err = PTR_ERR(priv->ipc_device);
		class_destroy(priv->ipc_class);
		cdev_del(&priv->ipc_cdev);
		unregister_chrdev_region(priv->dev_num, 1);
		uio_unregister_device(&priv->info);

		return err;
	}

	dev_info(&pdev->dev, "ipc char device %s loaded\n", uio_name);

	/*set uio_priv*/
	err = of_property_read_s32(ipc_uio_node, "ipc_uio_id", &ipc_uio_id);
	if (err != 0) {
		dev_err(dev, "ipc-uio ipc_uio_id read failed %d\n", err);
		device_destroy(priv->ipc_class, priv->dev_num);
		class_destroy(priv->ipc_class);
		cdev_del(&priv->ipc_cdev);
		unregister_chrdev_region(priv->dev_num, 1);
		uio_unregister_device(&priv->info);

		return err;
	}

	if (ipc_uio_id < 0 || ipc_uio_id >= MAX_UIO_DEVICE) {
		dev_err(dev, "ipc-uio ipc_uio_id invalid\n");
		err = -EINVAL;
		device_destroy(priv->ipc_class, priv->dev_num);
		class_destroy(priv->ipc_class);
		cdev_del(&priv->ipc_cdev);
		unregister_chrdev_region(priv->dev_num, 1);
		uio_unregister_device(&priv->info);

		return err;
	}

	priv->mclient.rx_callback = ipc_uio_rx_handler;
	priv->mclient.dev= dev;
	priv->mclient.tx_prepare = NULL;
	priv->mclient.knows_txdone = false;
	priv->mclient.tx_done = NULL;
	uio_priv[ipc_uio_id] = (void *)priv;
	dev_info(&pdev->dev, "device ready\n");
	//test info
	ipc_dbg("local %#llx remote %#llx  shmsize %d  numchan %d  numpool %d  numbuf %d  bufsize %d\n",
		def_info->local_shm_addr, def_info->remote_shm_addr, def_info->shm_size, def_info->num_chans,
		def_info->chans->num_pools,def_info->chans->pools->num_bufs, def_info->chans->pools->buf_size);

	return 0;
}

static int32_t ipc_shm_uio_remove(struct platform_device *pdev)
{
	struct ipc_uio_priv *priv = platform_get_drvdata(pdev);

	device_destroy(priv->ipc_class, priv->dev_num);
	class_destroy(priv->ipc_class);
	cdev_del(&priv->ipc_cdev);
	unregister_chrdev_region(priv->dev_num, 1);
	uio_unregister_device(&priv->info);
	dev_info(&pdev->dev, "device removed\n");

	return 0;
}

static const struct of_device_id ipc_uio_ids[] = {
	{ .compatible = "hobot,ipc-uio", },
	{}
};
MODULE_DEVICE_TABLE(of, ipc_uio_ids);

static struct platform_driver ipc_uio_driver = {
	.driver = {
		.name = UIO_DRIVER_NAME,
		.of_match_table = ipc_uio_ids,
	},
	.probe = ipc_shm_uio_probe,
	.remove = ipc_shm_uio_remove,
};

static int __init hb_ipc_uio_init(void)
{
	pr_info("hb_ipc_uio_init...\n");
	return platform_driver_register(&ipc_uio_driver);
}

static void __exit hb_ipc_uio_exit(void)
{
	pr_info("hb_ipc_uio_exit\n");
	platform_driver_unregister(&ipc_uio_driver);
}

module_init(hb_ipc_uio_init);
module_exit(hb_ipc_uio_exit);

MODULE_AUTHOR("Hobot");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS(UIO_DRIVER_NAME);
MODULE_DESCRIPTION("Hobot IPC UIO Driver");
MODULE_VERSION(DRIVER_VERSION);
