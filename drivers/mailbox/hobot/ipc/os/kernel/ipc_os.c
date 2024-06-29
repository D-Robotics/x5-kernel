/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file ipc_os.c
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/mutex.h>

#include "ipc_os.h"
#include "mbox_platform.h"

#define NODE_NAME_MAX_LEN	(32)
// #define BPU_INSTANCE		(32)
#define DEFAULT_TIMEOUT		(1)
#define NUM_DATA		(7)/**< data number*/
#define SUPPORT_DEF_MODE	(1)
#define NOSUPPORT_DEF_MODE	(0)
#define NO_DEF_MODE		(0x6e646566u)
#define DEV_MEM_START		(0x20000000u)/**< device memory start address*/
#define DEV_MEM_END		(0x3fffffffu)/**< device memory end address*/

/**
 * @struct ipc_os_priv_instance
 * Define the descriptor of OS specific private data each instance
 * @NO{S17E09C06}
 */
struct ipc_os_priv_instance {
	int32_t shm_size;/**< local/remote shared memory size*/
	uint64_t local_phys_shm;/**< local shared memory physical address*/
	uint64_t remote_phys_shm;/**< remote shared memory physical address*/
	uint64_t local_virt_shm;/**< local shared memory virtual address*/
	uint64_t remote_virt_shm;/**< remote shared memory virtual address*/
	int32_t timeout;/**< blocking time, 0 default blocking time, >0 specific blocking time*/
	int32_t trans_flags;/**< transmission flags*/
	int32_t mbox_chan_idx;/**< mailbox channel index*/
	int32_t state;/**< state to indicate whether instance is initialized*/
	struct mbox_client mclient;/**< mailbox client data*/
	struct mbox_chan *mchan;/**< mailbox channel pointer*/
	struct ipc_dev_instance *ipc_dev;/**< private data*/
	struct mutex notify_mutex_lock;/** notify mutex lock*/
	struct mutex ipc_init_mutex;/**< lock when opening and closing*/
};

struct mbox_share_res {
	int32_t user_cnt;
	struct mbox_chan *mchan;
} g_mbox_res[MAX_MBOX_IDX];

/**
 * @struct ipc_os_priv
 * Define the descriptor of OS specific private data
 * @NO{S17E09C06}
 */
static struct ipc_os_priv {
	struct ipc_os_priv_instance id[MAX_NUM_INSTANCE];/**< private data per instance*/
	int32_t (*rx_cb)(int32_t instance, int32_t budget);/**< upper layer rx callback*/
} priv;

/* sotfirq routine for deferred interrupt handling */
static void ipc_shm_softirq(unsigned long arg)
{
	unsigned long budget = IPC_SOFTIRQ_BUDGET;
	// int32_t instance = (int32_t)arg;
	uint8_t i = 0;

	for (i = 0; i < MAX_NUM_INSTANCE; i ++) {
		if ((priv.id[i].state != IPC_SHM_INSTANCE_ENABLED) ||
			(priv.id[i].mbox_chan_idx == IPC_MBOX_NONE))
			continue;

		/* call upper layer callback */
		(void)priv.rx_cb(i, budget);
	}
}

static void ipc_os_handler(struct mbox_client *cl, void *mssg)
{
	int32_t *ptr = dev_get_drvdata(cl->dev);

	ipc_shm_softirq((unsigned long)(*ptr));
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief OS specific initialization code
 *
 * @param[in] instance: instance id
 * @param[in] cfg: configuration parameters
 * @param[in] rx_cb: rx callback to be called from Rx softirq
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_os_init(int32_t instance, const struct ipc_shm_cfg *cfg,
		int32_t (*rx_cb)(int32_t, int32_t))
{
	int32_t err;

	/* check parameter */
	if (!rx_cb) {
		ipc_err("rx_cb is NULL\n");

		return -EINVAL;
	}

	if ((instance >= MAX_NUM_INSTANCE) || (instance < 0)) {
		ipc_err("instance %d is invalid\n", instance);

		return -EINVAL;
	}

	mutex_lock(&priv.id[instance].ipc_init_mutex);
	if (priv.id[instance].state == IPC_SHM_INSTANCE_ENABLED) {
		ipc_err("instance %d has opened\n", instance);
		mutex_unlock(&priv.id[instance].ipc_init_mutex);

		return -EINVAL;
	}

	priv.id[instance].timeout = cfg->timeout;
	priv.id[instance].trans_flags = cfg->trans_flags;
	priv.id[instance].mbox_chan_idx = cfg->mbox_chan_idx;
	err = ipc_os_mbox_open(instance);
	if (err < 0) {
		ipc_err("ipc mbox open failed: %d\n", err);
		mutex_unlock(&priv.id[instance].ipc_init_mutex);

		return err;
	}

	/* request and map local physical shared memory */
	// if (instance >= BPU_INSTANCE) {
	// 	priv.id[instance].local_virt_shm = cfg->local_shm_addr;
	// 	priv.id[instance].remote_virt_shm = cfg->remote_shm_addr;
	// } else {
		if ((cfg->local_shm_addr >= DEV_MEM_START &&
		    cfg->local_shm_addr <= DEV_MEM_END) ||
		    (cfg->remote_shm_addr >= DEV_MEM_START &&
		    cfg->remote_shm_addr <= DEV_MEM_END)) {
			ipc_err("ipc share memory no support device_mem, \
				invalid address range: %#x ~ %#x\n",
				DEV_MEM_START, DEV_MEM_END);
			ipc_os_mbox_close(instance);
			mutex_unlock(&priv.id[instance].ipc_init_mutex);

			return -EINVAL;
		}
		else {
			priv.id[instance].local_virt_shm = (uint64_t)ioremap_wc(cfg->local_shm_addr,
										cfg->shm_size);
			if (!priv.id[instance].local_virt_shm) {
				ipc_err("local share memory ioremap failed\n");
				ipc_os_mbox_close(instance);
				mutex_unlock(&priv.id[instance].ipc_init_mutex);

				return -ENOMEM;
			}

			priv.id[instance].remote_virt_shm = (uint64_t)ioremap_wc(cfg->remote_shm_addr,
										 cfg->shm_size);
			if (!priv.id[instance].remote_virt_shm)
			{
				ipc_err("remote share memory ioremap failed\n");
				iounmap((void *)cfg->local_shm_addr);
				ipc_os_mbox_close(instance);
				mutex_unlock(&priv.id[instance].ipc_init_mutex);

				return -ENOMEM;
			}
		}
	// }

	ipc_dbg("ioremap local %#llx, remote %#llx\n",
		 priv.id[instance].local_virt_shm, priv.id[instance].remote_virt_shm);

	/* save params */
	priv.id[instance].shm_size = cfg->shm_size;
	priv.id[instance].local_phys_shm = cfg->local_shm_addr;
	priv.id[instance].remote_phys_shm = cfg->remote_shm_addr;
	priv.rx_cb = rx_cb;
	memset((void *)priv.id[instance].local_virt_shm, 0, priv.id[instance].shm_size);
	priv.id[instance].state = IPC_SHM_INSTANCE_ENABLED;
	mutex_unlock(&priv.id[instance].ipc_init_mutex);

	return 0;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief free OS specific resources
 *
 * @param[in] instance: instance id
 *
 * @return None
 *
 * @callgraph
 * @callergraph
 * @design
 */
void ipc_os_free(int32_t instance)
{
	if ((instance >= MAX_NUM_INSTANCE) || (instance < 0)) {
		ipc_err("instance %d is invalid\n", instance);

		return;
	}
	mutex_lock(&priv.id[instance].ipc_init_mutex);
	if (priv.id[instance].state == IPC_SHM_INSTANCE_DISABLED) {
		mutex_unlock(&priv.id[instance].ipc_init_mutex);
		return;
	}
	ipc_os_mbox_close(instance);
	memset((void *)priv.id[instance].local_virt_shm, 0, priv.id[instance].shm_size);
	memset((void *)priv.id[instance].remote_virt_shm, 0, priv.id[instance].shm_size);
	// if (instance < BPU_INSTANCE) {
		iounmap((void *)priv.id[instance].local_virt_shm);
		iounmap((void *)priv.id[instance].remote_virt_shm);
	// }
	priv.id[instance].state = IPC_SHM_INSTANCE_DISABLED;
	mutex_unlock(&priv.id[instance].ipc_init_mutex);
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief get local shared mem address
 *
 * @param[in] instance: instance id
 *
 * @return local shared mem address pointer
 *
 * @callgraph
 * @callergraph
 * @design
 */
uint64_t ipc_os_get_local_shm(int32_t instance)
{
	return priv.id[instance].local_virt_shm;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief get remote shared mem address
 *
 * @param[in] instance: instance id
 *
 * @return remote shared mem address pointer
 *
 * @callgraph
 * @callergraph
 * @design
 */
uint64_t ipc_os_get_remote_shm(int32_t instance)
{
	return priv.id[instance].remote_virt_shm;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief get remote shared mem address
 *
 * @param[in] instance: instance id
 * @param[in] cfg: config information
 *
 * @return pointer to default mode information or NULL
 *
 * @callgraph
 * @callergraph
 * @design
 */
struct ipc_instance_cfg *ipc_os_get_def_info(int32_t instance, const struct ipc_instance_cfg *cfg)
{
	struct ipc_dev_instance* ipc_dev = priv.id[instance].ipc_dev;

	if (ipc_dev->ipc_info.mode == NO_DEF_MODE) {
		ipc_err("instance %d no support default mode\n", instance);

		return NULL;
	}

	return &ipc_dev->ipc_info;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief poll the channels for available messages to process
 *
 * @param[in] instance: instance id
 *
 * @retval ">=0": number of messages processed
 * @retval "<0": error code
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_os_poll_channels(int32_t instance)
{
	/* the softirq will handle rx operation if rx interrupt is configured */
	if (priv.id[instance].mbox_chan_idx != IPC_MBOX_NONE) {
		ipc_err("instance %d used mailbox channel\n", instance);

		return -EINVAL;
	}

	if (priv.rx_cb == NULL) {
		ipc_err("rx_cb is NULL\n");

		return -EINVAL;
	}

	return priv.rx_cb(instance, IPC_SOFTIRQ_BUDGET);
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief open mailbox
 *
 * @param[in] instance: instance id
 *
 * @retval "0": success
 * @retval "!0": error code
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_os_mbox_open(int32_t instance)
{
	struct ipc_os_priv_instance *cfg = &priv.id[instance];
	struct mbox_client *pmclient = &cfg->mclient;
	struct mbox_chan *mchan;
	int32_t err;

	ipc_dbg("mbox open start\n");
	if (cfg->mbox_chan_idx == IPC_MBOX_NONE) {

		return -1;//unused mailbox
	}

	if (cfg->trans_flags & SPIN_WAIT) {
		pmclient->tx_block = false;
	} else {
		pmclient->tx_block = true;
	}

	if (cfg->timeout == 0) {
		pmclient->tx_tout = DEFAULT_TIMEOUT;
	} else {
		pmclient->tx_tout = cfg->timeout;
	}

	g_mbox_res[cfg->mbox_chan_idx].user_cnt ++;
	if (g_mbox_res[cfg->mbox_chan_idx].user_cnt > 1) {
		cfg->mchan = g_mbox_res->mchan;
		return 0;
	}

	pmclient->rx_callback = ipc_os_handler;
	pmclient->tx_prepare = NULL;
	pmclient->knows_txdone = false;
	pmclient->tx_done = NULL;

	mchan = mbox_request_channel(pmclient, 0);
	if (IS_ERR(mchan)) {
		ipc_err("ipc get mailbox channel failed: %ld\n", PTR_ERR(mchan));

		return PTR_ERR(mchan);
	}


	cfg->mchan = mchan;
	g_mbox_res->mchan = mchan;
	err = hb_mbox_cfg_trans_flags(mchan, cfg->trans_flags);
	if (err < 0) {
		ipc_err("ipc config transfer flags %d failed: %d\n", cfg->trans_flags, err);

		return err;
	}

	mutex_init(&cfg->notify_mutex_lock);
	ipc_dbg("mbox open success\n");

	return 0;

}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief close mailbox
 *
 * @param[in] instance: instance id
 *
 * @retval "0": success
 * @retval "!0": error code
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_os_mbox_close(int32_t instance)
{
	if (priv.id[instance].mbox_chan_idx == IPC_MBOX_NONE) {
		return 0;//unused mailbox
	}

	if (priv.id[instance].mchan == NULL) {
		ipc_err("chan is NULL\n");

		return 0;
	}

	g_mbox_res[priv.id[instance].mbox_chan_idx].user_cnt --;
	ipc_dbg("user_cnt[%d] = %d\n", priv.id[instance].mbox_chan_idx, g_mbox_res[priv.id[instance].mbox_chan_idx].user_cnt);
	if (g_mbox_res[priv.id[instance].mbox_chan_idx].user_cnt > 0)
		return 0;

	mbox_free_channel(priv.id[instance].mchan);
	mutex_destroy(&priv.id[instance].notify_mutex_lock);

	return 0;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief notify remote device by mailbox
 *
 * @param[in] instance: instance id
 *
 * @retval "0": success
 * @retval "!0": error code
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_os_mbox_notify(int32_t instance)
{
	int err = 0;
	uint32_t tmp_data[NUM_DATA] = {0, 0, 0, 0, 0, 0, NUM_DATA};

	if (priv.id[instance].mbox_chan_idx == IPC_MBOX_NONE) {
		return 0;//unused mailbox
	}

	if (priv.id[instance].mchan == NULL) {
		ipc_err("ipc instance %d: mailbox uninitialized\n", instance);

		return -EINVAL;
	}

	mutex_lock(&priv.id[instance].notify_mutex_lock);
	err = mbox_send_message(priv.id[instance].mchan, tmp_data);
	if (err < 0) {
		mutex_unlock(&priv.id[instance].notify_mutex_lock);
		ipc_err("ipc instance %d: mailbox notify failed: %d\n", instance, err);
		return err;
	}

	if (priv.id[instance].trans_flags & SPIN_WAIT) {
		err = mbox_flush(priv.id[instance].mchan, priv.id[instance].timeout);

		if (err < 0) {
			ipc_err("ipc instance %d: mailbox no ack : %d\n", instance, err);
			mutex_unlock(&priv.id[instance].notify_mutex_lock);
			return err;
		}
	}
	mutex_unlock(&priv.id[instance].notify_mutex_lock);
	return 0;
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
	struct device_node *node;
	struct resource res;
	uint32_t local_offset;
	uint32_t remote_offset;

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

	node = of_parse_phandle(ipc_node, "shm-addr", 0);
	err = of_address_to_resource(node, 0, &res);
	if (err) {
		dev_err(dev, "Get adsp_ipc_reserved failed\n");
		return err;
	}
	err = of_property_read_u32(ipc_node, "local-offset", &local_offset);
	if (err) {
		dev_err(dev, "Get local-offset failed\n");
		return err;
	}
	err = of_property_read_u32(ipc_node, "remote-offset", &remote_offset);
	if (err) {
		dev_err(dev, "Get remote-offset failed\n");
		return err;
	}

	def_info->local_shm_addr = res.start + local_offset;
	def_info->remote_shm_addr = res.start + remote_offset;

	dev_dbg(dev, "base(0x%llx) local(0x%llx) remote(0x%llx)\n",
		res.start, def_info->local_shm_addr, def_info->remote_shm_addr);

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
			of_node_put(chan_node);

			return err;
		}

		if (chancfg->num_pools <= 0 ||
		    chancfg->num_pools > MAX_NUM_POOL_PER_CHAN) {
			dev_err(dev, "ipc num_pools invalid\n");
			err = -EINVAL;
			of_node_put(chan_node);

			return err;
		}
		/** get ipc pool resource*/
		pools = devm_kzalloc(dev, sizeof(*pools) * chancfg->num_pools, GFP_KERNEL);
		if (!pools) {
			dev_err(dev, "ipc alloc pools memory failed\n");
			err = -ENOMEM;
			of_node_put(chan_node);

			return err;
		}
		chancfg->pools = pools;
		for (j = 0; j < chancfg->num_pools; j++) {
			memset(node_name, '\0', sizeof(node_name));
			err = snprintf(node_name, NODE_NAME_MAX_LEN, "ipc_pool%d", j);
			if (err < 0){
				dev_err(dev, "ipc pool %d node name generate failed %d\n", j, err);
				of_node_put(chan_node);

				return err;
			}
			pool_node = of_get_child_by_name(chan_node, node_name);
			if (!pool_node){
				dev_err(dev, "ipc pool %d child node get failed: %d\n", j, err);
				err = -ENODEV;
				of_node_put(chan_node);

				return err;
			}
			poolcfg = pools + j;
			err = of_property_read_u16(pool_node, "num_bufs", &poolcfg->num_bufs);
			if (err != 0) {
				dev_err(dev, "ipc pool %d num_bufs read failed %d\n", j, err);
				of_node_put(pool_node);
				of_node_put(chan_node);

				return err;
			}

			if (poolcfg->num_bufs == 0 ||
			    poolcfg->num_bufs > MAX_NUM_BUF_PER_POOL) {
				dev_err(dev, "ipc num_bufs invalid\n");
				err = -EINVAL;
				of_node_put(pool_node);
				of_node_put(chan_node);

				return err;
			}

			err = of_property_read_u32(pool_node, "buf_size", &poolcfg->buf_size);
			if (err != 0) {
				dev_err(dev, "ipc pool %d buf_size read failed %d\n", j, err);
				of_node_put(pool_node);
				of_node_put(chan_node);

				return err;
			}

			if (poolcfg->buf_size == 0) {
				dev_err(dev, "ipc buf_size invalid\n");
				err = -EINVAL;
				of_node_put(pool_node);
				of_node_put(chan_node);

				return err;
			}

			of_node_put(pool_node);
		}

		of_node_put(chan_node);
	}

	return 0;
}

static int32_t hb_ipc_probe(struct platform_device *pdev)
{
	struct ipc_dev_instance *ipc_dev;
	struct device *dev = &pdev->dev;
	struct device_node *ipc_node;// mailbox node
	struct ipc_instance_info_m1 *def_info;
	int32_t def_mode = NOSUPPORT_DEF_MODE;
	int32_t err;

	ipc_dev = devm_kzalloc(dev, sizeof(*ipc_dev), GFP_KERNEL);
	if (!ipc_dev) {
		dev_err(dev, "ipc dev alloc failed\n");

		return -ENOMEM;
	}

	ipc_node = dev->of_node;
	ipc_dev->dev = dev;

	err = of_property_read_s32(ipc_node, "instance", &ipc_dev->instance);
	if (err != 0) {
		dev_err(dev, "ipc instance read failed %d\n", err);

		return err;
	}

	if ((ipc_dev->instance < 0) || (ipc_dev->instance >= MAX_NUM_INSTANCE)) {
		dev_err(dev, "ipc instance %d is invalid\n", ipc_dev->instance);

		return -EINVAL;
	}

	platform_set_drvdata(pdev, ipc_dev);
	priv.id[ipc_dev->instance].ipc_dev = ipc_dev;
	priv.id[ipc_dev->instance].mclient.dev = dev;
	dev_set_drvdata(dev, &ipc_dev->instance);
	err = of_property_read_s32(ipc_node, "def_mode", &def_mode);
	if (err != 0 || def_mode != SUPPORT_DEF_MODE) {
		ipc_dev->ipc_info.mode = NO_DEF_MODE;
	} else {
		ipc_dev->ipc_info.mode = def_mode;
		def_info = &ipc_dev->ipc_info.info.custom_cfg;
		err = get_ipc_def_resource(pdev, ipc_node, def_info);
		if (err != 0) {
			dev_err(dev, "ipc channel initialize failed %d\n", err);

			return err;
		}
	}

	mutex_init(&priv.id[ipc_dev->instance].ipc_init_mutex);

	return 0;
}

static int32_t hb_ipc_remove(struct platform_device *pdev)
{
	struct ipc_dev_instance *ipc_dev = (struct ipc_dev_instance *)platform_get_drvdata(pdev);

	mutex_destroy(&priv.id[ipc_dev->instance].ipc_init_mutex);

	return 0;
}

static const struct of_device_id hb_ipc_of_match[] = {
	{ .compatible = "hobot,hobot-ipc", },
	{/* end */}
};
MODULE_DEVICE_TABLE(of, hb_ipc_of_match);

static struct platform_driver hb_ipc_driver = {
	.driver = {
		.name	= "hobot-ipc",
		.of_match_table = hb_ipc_of_match,
	},
	.probe = hb_ipc_probe,
	.remove = hb_ipc_remove,
};

static int __init hb_ipc_init(void)
{
	pr_info("hb_ipc_init...\n");
	return platform_driver_register(&hb_ipc_driver);
}

static void __exit hb_ipc_exit(void)
{
	pr_info("hb_ipc_exit\n");
	platform_driver_unregister(&hb_ipc_driver);
}

arch_initcall(hb_ipc_init);
module_exit(hb_ipc_exit);

EXPORT_SYMBOL(hb_ipc_open_instance);
EXPORT_SYMBOL(hb_ipc_close_instance);
EXPORT_SYMBOL(hb_ipc_acquire_buf);
EXPORT_SYMBOL(hb_ipc_release_buf);
EXPORT_SYMBOL(hb_ipc_send);
EXPORT_SYMBOL(hb_ipc_is_remote_ready);
EXPORT_SYMBOL(hb_ipc_poll_instance);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hobot IPC Driver");
MODULE_VERSION("1.0.0");
MODULE_AUTHOR("HOBOT");
