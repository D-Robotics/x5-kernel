/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <asm/io.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/clk.h>

#include "mbox_common.h"
#include "mbox_os.h"
#include "hb_mbox.h"

#define MIN(x,y) ((x)<(y)?(x):(y))
#define NODE_NAME_MAX_LEN (50)

/*mailbox mode information*/
#define MBOX_NO_AUTO_ACK (0)/**< disable auto ack*/
#define MBOX_NO_AUTO_LINK (0)/**< disable auto link*/
#define MBOX_ACK_LINK_DEFAULT (MBOX_NO_AUTO_ACK|MBOX_NO_AUTO_LINK)/**< default disable auto ack and auto link*/
#define MBOX_IRQ_ENABLE (1)/**< local interrupt ennable*/
#define MBOX_IRQ_DEFAULT MBOX_IRQ_ENABLE/**< default interrupt*/

/*delay parameter*/
#define MBOX_DELAY_US (2)/**< delay time*/
#define MBOX_DELAY_FACTOR (500)/**< delay time factor*/
#define MBOX_TOOL_LEN (128)/**< mbox tool row length*/
#define MBOX_TOOL_OFFSET (2)/**< mbox tool offset row*/


/**
 * @enum mbox_dts_info_e
 * Define the descriptor of mbox dts information.
 * @NO{S17E09C06}
 */
enum mbox_dts_info_e {
	MBOX_CHAN_ID = 0,/**< mbox channel index*/
	SEND_MBOX_ID,/**< send mbox id*/
	RECV_MBOX_ID,/**< recv mbox id */
	SEND_SRC_ID,/**< source register id of send mbox*/
	SEND_DST_ID,/**< dest register id of send mbox*/
	RECV_SRC_ID,/**< source register id of recv mbox*/
	RECV_DST_ID,/**< dest register id of recv mbox*/
	LOCAL_INT_MODE,/**< local interrupt mode*/
	AUTO_ACK_LINK_MODE,/**< auto ack and auto link mode*/
	ALL_ARGS_NUM/**< number of all args*/
};

/**
 * @struct pl320_mbox
 * @brief Define the descriptor of pl320 mbox information.
 * @NO{S17E09C06}
 */
struct pl320_mbox {
	struct device *dev;/**< device*/
	struct mbox_chan chan[MAX_CHAN_PER_IPCM];/**< mailbox channel*/
	struct mbox_controller controller;/**< mailbox controller*/
	uint8_t ipcm_id;/**< ipcm id*/
	uint8_t num_data;/**< number of data register per ipcm*/
	uint8_t num_int;/**< number of interrupt per ipcm*/
	uint8_t num_mbox;/**< number of mailbox per ipcm*/
	uint8_t num_local_irq;/**< number of local irq per ipcm*/
	uint8_t num_mbox_chan;/**< number of mailbox channel per ipcm*/
	uint8_t *base;/**< ipcm base address*/
	struct clk *clk;/**< clk*/
};

/**
 * @struct mbox_chan_cfg
 * @brief Define the descriptor of mbox channel information.
 * @NO{S17E09C06}
 */
struct mbox_chan_cfg {
	uint8_t send_mbox_id;/**< used to send msg, be inited by local core*/
	uint8_t recv_mbox_id;/**< used to recv msg, be inited by remote core*/
	uint8_t local_id;/**< local core id*/
	uint8_t remote_id;/**< remote core id*/
	uint32_t local_ack_link_mode;/**< PL320 mode, including auto ack and auto link*/
	uint32_t trans_flags;/**< bit[0]: =0, async, =1, sync;
				  bit[1]: reserved;
				  bit[2]: =0, sleep waiting, =1, spin waiting;
				  bit[31:3]: reserved*/
	uint32_t rx_data[NUM_DATA];/**< recevied user data*/
	uint32_t size;/**< recevied userdata size*/
	uint32_t irq;/**< irq number*/
	struct pl320_mbox *parent;/**< pointer of struct pl320_mbox*/
	spinlock_t irq_spinlock;/**< irq spinlock*/
	//send_lock
	//recv_lock
	//ack_lock
	//mutex

};

static inline uint8_t *os_get_ipcmbase(struct mbox_chan_cfg *mchan)
{
	struct pl320_mbox *mbox = mchan->parent;

	return mbox->base;
}

int32_t hb_mbox_cfg_trans_flags(struct mbox_chan *chan, int32_t trans_flags)
{
	struct mbox_chan_cfg *mchan;
	uint8_t *ipcmbase;

	if (!chan || !chan->con_priv || trans_flags < 0) {
		mbox_err("invalid paramter\n");

		return -EINVAL;
	}

	mchan = (struct mbox_chan_cfg *)chan->con_priv;
	mchan->trans_flags = trans_flags;
	ipcmbase = os_get_ipcmbase(mchan);

	if (!(trans_flags & MBOX_SPIN_WAIT)) {
		/*enable ack interrupt*/
		com_enable_irq(ipcmbase, mchan->send_mbox_id, mchan->local_id);
	} else {
		/*disable ack interrupt*/
		com_disable_irq(ipcmbase, mchan->send_mbox_id, mchan->local_id);
	}

	/*enable remote recv interrupt*/
	com_enable_irq(ipcmbase, mchan->send_mbox_id, mchan->remote_id);

	return 0;
}

EXPORT_SYMBOL(hb_mbox_cfg_trans_flags);

static irqreturn_t hb_mbox_softirq(int irq, void *arg)
{
	struct mbox_chan *chan = (struct mbox_chan *)arg;
	struct mbox_chan_cfg *mchan;
	uint8_t *ipcmbase;
	uint8_t *data_addr;
	int32_t i = 0;
	uint32_t len = 0;

	mbox_dbg("info: pl320 handler thread irq %d\n", irq);
	if (!chan || !chan->con_priv) {
		mbox_dbg("invalid parameter\n");
		return IRQ_NONE;
	}

	mchan = (struct mbox_chan_cfg *)chan->con_priv;
	ipcmbase = os_get_ipcmbase(mchan);
	mbox_dbg("ipcm %p local %d recvmbox %d sendmbox %d mis %d sendreg %d, mchan %p, chan %p, arg %p\n",
		ipcmbase, mchan->local_id, mchan->recv_mbox_id, mchan->send_mbox_id,
		com_get_ipcmmis(ipcmbase, mchan->local_id, 3),
		com_get_sendreg(ipcmbase, mchan->recv_mbox_id), mchan, chan, arg);
	if ((com_get_ipcmmis(ipcmbase, mchan->local_id, 3) & BIT(mchan->recv_mbox_id)) != 0 &&
	     com_get_sendreg(ipcmbase, mchan->recv_mbox_id) == SEND_NOTIFY){
		mbox_dbg("pl320 get recv irq\n");
		// (void)com_clear_sendreg(ipcmbase, mchan->recv_mbox_id);
		data_addr = com_get_mboxdata_addr(ipcmbase, mchan->recv_mbox_id, 1);
		len = com_get_mboxdata_level(ipcmbase, mchan->recv_mbox_id);
		for (i = 0; i < len; i++) {
			mchan->rx_data[i] = os_read_register(data_addr);
		}
		(void)com_clear_sendreg(ipcmbase, mchan->recv_mbox_id);
		if (mchan->trans_flags & MBOX_SYNC_TRANS) {
			(void)mbox_chan_received_data(chan,mchan->rx_data);
			(void)com_ack_notify(ipcmbase, mchan->recv_mbox_id);
		} else {
			(void)com_ack_notify(ipcmbase, mchan->recv_mbox_id);
			(void)mbox_chan_received_data(chan,mchan->rx_data);
		}

		return IRQ_HANDLED;
	}
	if ((com_get_ipcmmis(ipcmbase, mchan->local_id, 1) & BIT(mchan->send_mbox_id)) != 0 &&
	     com_get_sendreg(ipcmbase, mchan->send_mbox_id) == ACK_NOTIFY){
		mbox_dbg("pl320 get ack irq\n");
		(void)com_clear_sendreg(ipcmbase, mchan->send_mbox_id);
		mbox_chan_txdone(chan, 0);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static irqreturn_t hb_mbox_handler(int32_t irq, void *arg)
{
	mbox_dbg("%s\n", __FUNCTION__);
	return IRQ_WAKE_THREAD;
}

static int pl320_send_data(struct mbox_chan *chan, void *data)
{
	struct mbox_chan_cfg *mchan;
	uint8_t *ipcmbase;
	uint8_t *data_addr;
	uint32_t *pdata = (uint32_t *)data;
	int32_t i = 0;

	mbox_dbg("pl320 senddata start\n");
	if (!chan || !chan->con_priv || !data) {
		mbox_err("pl320 send data invalid paramter\n");

		return -EINVAL;
	}

	mchan = (struct mbox_chan_cfg *)chan->con_priv;
	ipcmbase = os_get_ipcmbase(mchan);
	if (!ipcmbase) {
		mbox_err("ipcmbase is NULL\n");

		return -EINVAL;
	}

	if ((com_get_src(ipcmbase, mchan->send_mbox_id) & BIT(mchan->local_id)) == 0){
		mbox_err("mailbox channel send mbox %d invalid parameter\n",
			  mchan->send_mbox_id);

		return -EINVAL;
	}

	data_addr = com_get_mboxdata_addr(ipcmbase, mchan->send_mbox_id, 0);
	for (i = 0; i < NUM_DATA; i++) {
		(void)os_write_register(data_addr, pdata[i]);
	}

	(void)com_tx_notify(ipcmbase, mchan->send_mbox_id);
	mbox_dbg("info: pl320 senddata success\n");

	return 0;
}

static int pl320_startup(struct mbox_chan *chan)
{
	int32_t err;
	struct mbox_chan_cfg *mchan = (struct mbox_chan_cfg *)chan->con_priv;
	uint8_t *ipcmbase;

	mbox_dbg("info: pl320 startup\n");
	ipcmbase = os_get_ipcmbase(mchan);
	if (!ipcmbase) {
		mbox_err("ipcmbase is NULL\n");

		return -EINVAL;
	}

	if (com_get_src(ipcmbase, mchan->send_mbox_id) != 0) {
		mbox_err("mailbox channel send mbox %d busy\n", mchan->send_mbox_id);

		return -EBUSY;
	}

	err = com_set_src(ipcmbase, mchan->send_mbox_id, mchan->local_id);
	if (err < 0) {
		mbox_err("taken src register failed: %d\n", err);
	
		return err;
	}
	mbox_dbg("%s src = 0x%x, id = %d, local_id = 0x%lx\n", __FUNCTION__, com_get_src(ipcmbase, mchan->send_mbox_id), mchan->send_mbox_id, BIT(mchan->local_id));
	
	if ((com_get_src(ipcmbase, mchan->send_mbox_id) & BIT(mchan->local_id)) == 0) {
		mbox_err("mailbox channel send mbox %d was occupied\n", mchan->send_mbox_id);

		return -EBUSY;
	}

	(void)com_set_dst(ipcmbase, mchan->send_mbox_id, mchan->remote_id);
	(void)com_set_mode(ipcmbase, mchan->send_mbox_id, mchan->local_ack_link_mode);
	//enable mbox irq
	(void)com_enable_irq(ipcmbase, mchan->send_mbox_id, mchan->local_id);
	(void)com_enable_irq(ipcmbase, mchan->send_mbox_id, mchan->remote_id);
	mbox_dbg("txmbox %d, txsrc %d, txdst %d, txmod %d, ipcmbase %p, mchan %p, true %d, chan %p\n",
	 	  mchan->send_mbox_id, mchan->local_id, mchan->remote_id,
		  mchan->local_ack_link_mode, ipcmbase, mchan, true, chan);

	return 0;
}

static void pl320_shutdown(struct mbox_chan *chan)
{
	int32_t err;
	struct mbox_chan_cfg *mchan = (struct mbox_chan_cfg *)chan->con_priv;
	uint8_t *ipcmbase;

	mbox_dbg("pl320 shutdown\n");
	ipcmbase = os_get_ipcmbase(mchan);
	if (!ipcmbase) {
		mbox_err("ipcmbase is NULL\n");

		return;
	}

	if ((com_get_src(ipcmbase, mchan->send_mbox_id) & BIT(mchan->local_id)) != 0) {
		err = com_clear_src(ipcmbase, mchan->send_mbox_id, mchan->local_id);
		if (err < 0){
			mbox_err("clear src register failed: %d\n", err);

			return;
		}
	}

	if (com_get_src(ipcmbase, mchan->send_mbox_id) != 0){
		mbox_err("mailbox free failed\n");

		return;
	}
}

static int32_t flush_mbox(struct mbox_chan_cfg *mchan)
{
	uint8_t *ipcmbase;

	ipcmbase = os_get_ipcmbase(mchan);
	if (!ipcmbase) {
		mbox_err("ipcmbase is NULL\n");

		return -EINVAL;
	}

	if ((com_get_ipcmris(ipcmbase, mchan->local_id, 1) & BIT(mchan->send_mbox_id)) != 0 &&
	     com_get_sendreg(ipcmbase, mchan->send_mbox_id) == ACK_NOTIFY){
		mbox_dbg("pl320 check chan ack valid\n");
		(void)com_clear_sendreg(ipcmbase, mchan->send_mbox_id);

		return BIT(mchan->send_mbox_id);
	}

	return 0;
}

static int32_t pl320_flush(struct mbox_chan *chan, unsigned long timeout)
{
	int32_t err;
	uint64_t expire;
	struct mbox_chan_cfg *mchan;
	uint64_t atomic_delay = 0;

	if (!chan || !chan->con_priv) {
		mbox_err("invalid channel\n");

		return -EINVAL;
	}

	mchan = (struct mbox_chan_cfg *)chan->con_priv;
	if (!(mchan->trans_flags & MBOX_SPIN_WAIT)) {
		mbox_err("channel is configed to sleep wait mode, disbale use flush API\n");

		return -EINVAL;
	}

	expire = msecs_to_jiffies(timeout) + jiffies;
	for (;;) {
		/* check ack */
		err = flush_mbox(mchan);
		if (err != 0)
			break;

		udelay(MBOX_DELAY_US);
		atomic_delay += MBOX_DELAY_US;
		if (atomic_delay > timeout * MBOX_DELAY_FACTOR) {
			err = -ETIME;
			mbox_err("wait ack timeout %ld\n", timeout);
			break;
		}
	}

	mbox_chan_txdone(chan, err);
	if (err > 0) {
		return 0;
	}

	return err;
}

struct mbox_chan_ops pl320_chan_ops = {
	.send_data = pl320_send_data,
	.startup = pl320_startup,
	.shutdown = pl320_shutdown,
	.flush = pl320_flush,
};

static ssize_t hb_pl320_tool_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pl320_mbox *mbox = NULL;
	struct platform_device *pdev = container_of(dev,struct platform_device,dev);
	int32_t mbox_id = 0;
	uint8_t *ipcmbase = NULL;
	uint32_t reg_src, reg_dst, reg_mset, reg_send;
	int32_t err = 0;
	char reg_dump[MBOX_TOOL_LEN];

	mbox_dbg("%s mbox debug tool show entry\n", dev_name(&pdev->dev));
	mbox = (struct pl320_mbox *)platform_get_drvdata(pdev);
	if (!mbox || !(mbox->base)) {
		mbox_err("%s mbox or mbox->base is NULL\n", dev_name(&pdev->dev));

		return -EINVAL;
	}

	err = snprintf(buf + mbox_id * MBOX_TOOL_LEN * MBOX_TOOL_OFFSET,
			MBOX_TOOL_LEN,
			"%s\n", dev_name(&pdev->dev));
	if (err < 0){
		mbox_err("%s snprintf dev_name failed: %d\n", dev_name(&pdev->dev), err);

		return err;
	}
	ipcmbase = mbox->base;
	for (mbox_id = 0; mbox_id < NUM_MBOX; mbox_id++) {
		reg_src = com_get_src(ipcmbase, mbox_id);
		reg_dst = com_get_dst(ipcmbase, mbox_id);
		reg_mset = com_get_irq_cfgstatus(ipcmbase, mbox_id);
		reg_send = com_get_sendreg(ipcmbase, mbox_id);
		if (reg_src) {
			err = snprintf(buf + mbox_id * MBOX_TOOL_LEN * MBOX_TOOL_OFFSET + MBOX_TOOL_LEN,
					MBOX_TOOL_LEN,
					"\tmbox_id[%02d]\tstatus: used\t",
					mbox_id);
			if (err < 0){
				mbox_err("%s mbox_id[%d] snprintf failed: %d\n", dev_name(&pdev->dev), mbox_id, err);

				return err;
			}
		} else {
			err = snprintf(buf + mbox_id * MBOX_TOOL_LEN * MBOX_TOOL_OFFSET + MBOX_TOOL_LEN,
					MBOX_TOOL_LEN,
					"\tmbox_id[%02d]\tstatus: unused\t",
					mbox_id);
			if (err < 0){
				mbox_err("%s mbox_id[%d] snprintf failed: %d\n", dev_name(&pdev->dev), mbox_id, err);

				return err;
			}
		}

		memset(reg_dump, '\0', MBOX_TOOL_LEN);
		err = snprintf(reg_dump, MBOX_TOOL_LEN,
				"src[%08x]\tdst[%08x]\tmset[%08x]\tsend[%08x]\n",
				reg_src, reg_dst, reg_mset, reg_send);
		if (err < 0){
			mbox_err("%s mbox_id[%d] snprintf register failed: %d\n", dev_name(&pdev->dev), mbox_id, err);

			return err;
		}
		strncat(buf + mbox_id * MBOX_TOOL_LEN * MBOX_TOOL_OFFSET + MBOX_TOOL_LEN,
			reg_dump, MBOX_TOOL_LEN);
	}

	mbox_dbg("%s mbox debug tool show exit\n", dev_name(&pdev->dev));

	return (ssize_t)(mbox_id * MBOX_TOOL_LEN * MBOX_TOOL_OFFSET);
}

static ssize_t hb_pl320_tool_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mbox_dbg("mbox debug tool store\n");

	return count;
}

static DEVICE_ATTR(mbox_tool, S_IRUGO | S_IWUSR, hb_pl320_tool_show, hb_pl320_tool_store);

#define HORIZON_SIP_MAILBOX_ENABLE_SET 0xc2000009
#define HORIZON_SIP_MAILBOX_ENABLE_GET 0xc200000a

enum ipc_interruot_id {
	IPC_SRC_SEC_ID = 0,
	IPC_SRC_NSEC_ID,
	IPC_DST_SEC_ID,
	IPC_DST_NSEC_ID,
};

static int32_t mbox_enable_src(uint8_t mbox_id)
{
	uint32_t val = 0;
	struct arm_smccc_res res;

	// val = os_read_register(addr);
	// if (val != 0) {
	// 	mbox_err("src access denied\n");

	// 	return -EACCES;
	// }
	// val |= BIT(mbox_id);
	// (void)os_write_register(addr, val);

	arm_smccc_smc(HORIZON_SIP_MAILBOX_ENABLE_GET, 0, 0, 0, 0, 0, 0, 0, &res);
	val = res.a0;
	val |= BIT(mbox_id);

	arm_smccc_smc(HORIZON_SIP_MAILBOX_ENABLE_SET, val, 0, 0, 0, 0, 0, 0, &res);	
	pr_debug("%s write mailbox enable 0x%lx\n", __FUNCTION__, res.a0);

	return 0;
}


static struct mbox_chan *pl320_chan_xlate(struct mbox_controller *controller,
					   const struct of_phandle_args *spec)
{
	struct pl320_mbox *mbox = (struct pl320_mbox *)dev_get_drvdata(controller->dev);
	struct mbox_chan_cfg *mchan;
	struct mbox_chan *chan;

	if ((spec->args[MBOX_CHAN_ID] >= MAX_CHAN_PER_IPCM) ||
		(spec->args[SEND_MBOX_ID] >= NUM_MBOX) ||
		(spec->args[RECV_MBOX_ID] >= NUM_MBOX) ||
		(spec->args[SEND_SRC_ID] >= NUM_INT) ||
		(spec->args[SEND_DST_ID] >= NUM_INT) ||
		(spec->args[RECV_SRC_ID] >= NUM_INT) ||
		(spec->args[RECV_DST_ID] >= NUM_INT) ||
		(spec->args[LOCAL_INT_MODE] > MBOX_IRQ_DEFAULT) ||
		(spec->args[AUTO_ACK_LINK_MODE] > MBOX_ACK_LINK_DEFAULT)) {
		dev_err(mbox->dev, "Invalid mailbox channel parameter  %d\n", spec->args[MBOX_CHAN_ID]);

		return ERR_PTR(-EINVAL);
	}

	chan = mbox->chan + spec->args[MBOX_CHAN_ID];
	mchan = (struct mbox_chan_cfg *)chan->con_priv;
	mchan->send_mbox_id = spec->args[SEND_MBOX_ID];
	mchan->recv_mbox_id = spec->args[RECV_MBOX_ID];
	mchan->local_id = spec->args[SEND_SRC_ID];
	mchan->remote_id = spec->args[SEND_DST_ID];
	mchan->local_ack_link_mode = spec->args[AUTO_ACK_LINK_MODE];
	dev_dbg(mbox->dev, "mbox_info sendmbox %d, recvmbox %d, local_id %d remote_id %d linkmod %d\n",
		mchan->send_mbox_id, mchan->recv_mbox_id, mchan->local_id, mchan->remote_id,
		mchan->local_ack_link_mode);

	mbox_enable_src(mchan->send_mbox_id);
	mbox_enable_src(mchan->recv_mbox_id);

	return chan;
}

static int32_t get_mbox_dev_resource(struct platform_device *pdev, struct pl320_mbox *mbox)
{
	int32_t err, i, j, irq;
	struct device *dev = &pdev->dev;
	struct mbox_chan_cfg *pchan, *mchan;
	struct device_node *mailbox_node;
	uint8_t int_arry[MAX_LOCAL_IRQ];

	mailbox_node = dev->of_node;
	if (!mailbox_node){
		dev_err(dev,"no pl320 mailbox device ndoe\n");

		return -ENODEV;
	}

	err = of_property_read_u8(mailbox_node, "ipcm_id", &mbox->ipcm_id);
	if ((err != 0) || (mbox->ipcm_id >= NUM_IPCM)) {
		dev_err(dev, "ipcm_id read failed %d\n", err);

		return err;
	}

	err = of_property_read_u8(mailbox_node, "num_mbox_chan", &mbox->num_mbox_chan);
	if ((err != 0) || (mbox->num_mbox_chan > MAX_CHAN_PER_IPCM)) {
		dev_err(dev, "num_mbox_chan read failed %d\n", err);

		return err;
	}

	err = of_property_read_u8(mailbox_node, "num_local_irq", &mbox->num_local_irq);
	if ((err != 0) || (mbox->num_local_irq > MAX_LOCAL_IRQ)) {
		dev_err(dev, "num_local_irq read failed %d\n", err);

		return err;
	}

	err = of_property_read_variable_u8_array(mailbox_node, "int_id", int_arry, 1, MAX_LOCAL_IRQ);
	if (err < 0) {
		dev_err(dev, "int_id arry read failed %d\n", err);

		return err;
	}

	mbox->num_data = NUM_DATA;
	mbox->num_int = NUM_INT;
	mbox->num_mbox = NUM_MBOX;
	mbox->base = (uint8_t *)devm_ioremap_resource(dev, platform_get_resource(pdev, IORESOURCE_MEM, 0));
	dev_info(dev, "%s, mbox->base = %p\n", __FUNCTION__, mbox->base);
	if (IS_ERR(mbox->base)) {
		dev_err(dev, "ioremap failed\n");

		return PTR_ERR(mbox->base);
	}

	pchan = devm_kzalloc(dev, mbox->num_mbox_chan * sizeof(struct mbox_chan_cfg), GFP_KERNEL);
	if (!pchan){
		dev_err(dev,"no memory for mailbox channel\n");

		return -ENOMEM;
	}
	for (i = 0; i < mbox->num_mbox_chan; i++){
		mchan = pchan + i;
		mchan->irq = platform_get_irq(pdev, i * mbox->num_local_irq);
		mchan->parent = (void *)mbox;
		mbox->chan[i].con_priv = (void *)mchan;
		dev_dbg(&pdev->dev, "mbox_info irq %d, devname %s, arg %p\n",
			mchan->irq, dev_name(dev), &mbox->chan[i]);
		for (j = 0; j < mbox->num_local_irq; j++) {
			irq = platform_get_irq(pdev, i * mbox->num_local_irq + j);
			dev_dbg(&pdev->dev, "platform_get_irq irq %d, form %d\n", irq, i * mbox->num_local_irq + j);
			err = devm_request_threaded_irq(dev, irq,
						hb_mbox_handler,
						hb_mbox_softirq,
						IRQF_ONESHOT,
						dev_name(dev),
						(void *)&mbox->chan[i]);
		}
		if (err) {
			dev_err(&pdev->dev, "Failed to request mailbox irq %d: %d\n", mchan->irq, err);

			return err;
		}
		spin_lock_init(&mchan->irq_spinlock);
	}
	dev_dbg(&pdev->dev,"ipcmid %d, numchan %d, numlocalirq %d\n",
		mbox->ipcm_id, mbox->num_mbox_chan, mbox->num_local_irq);

	return 0;
}

static int32_t hb_pl320_probe(struct platform_device *pdev)
{
	int32_t err;
	struct device *dev = &pdev->dev;
	struct pl320_mbox *mbox;

	dev_dbg(dev, "hb_pl320 initing\n");
	if (!of_device_is_compatible(dev->of_node, "hobot,hobot-pl320")){
		dev_err(dev,"no node for hobot-pl320\n");

		return -ENODEV;
	}
	/* Allocate memory for mbox */
	mbox = devm_kzalloc(dev, sizeof(*mbox), GFP_KERNEL);
	if (!mbox){
		dev_err(dev,"no memory for mailbox\n");

		return -ENOMEM;
	}

	err = get_mbox_dev_resource(pdev, mbox);
	if(err){
		dev_err(dev,"get resource of mailbox failed\n");

		return err;
	}

	mbox->dev = dev;
	mbox->controller.dev = dev;
	mbox->controller.chans = mbox->chan;
	mbox->controller.num_chans = MAX_CHAN_PER_IPCM;
	mbox->controller.ops = &pl320_chan_ops;
	mbox->controller.of_xlate = pl320_chan_xlate;
	/*tx_done intterrupt*/
	mbox->controller.txdone_irq = true;
	mbox->controller.txdone_poll = false;

	mbox->clk = devm_clk_get(dev, "mailbox_aclk");
	if (IS_ERR(mbox->clk)) {
		err = PTR_ERR(mbox->clk);
		dev_err(dev, "Failed to get clock: %d\n", err);
		return err;
	}

	err = clk_prepare_enable(mbox->clk);
	if (err) {
		dev_err(dev, "Failed to enable clock: %d\n", err);
		return err;
	}

	platform_set_drvdata(pdev, mbox);
	err = devm_mbox_controller_register(dev, &mbox->controller);
	if (err) {
		dev_err(dev, "Failed to register mailboxes %d\n", err);
		goto err_disable_unprepare;
	}

	err = device_create_file(dev, &dev_attr_mbox_tool);
	if (err < 0) {
		dev_err(dev,"device_create_file failed: %d\n", err);
		goto err_disable_unprepare;
	}

	dev_info(dev, "PL320 Mailbox registered\n");

	return 0;

err_disable_unprepare:
	clk_disable_unprepare(mbox->clk);
	return err;
}

static int32_t hb_pl320_remove(struct platform_device *pdev)
{
	struct pl320_mbox *mbox = platform_get_drvdata(pdev);
	if (!mbox) {
		return -EINVAL;
	}

	clk_disable_unprepare(mbox->clk);

	device_remove_file(&pdev->dev, &dev_attr_mbox_tool);
	dev_info(&pdev->dev, "hb_pl320 removed\n");

	return 0;
}

static void hb_pl320_shutdown(struct platform_device *pdev)
{
	;
}

static const struct of_device_id hb_pl320_ids[] = {
	{
		.compatible = "hobot,hobot-pl320",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, hb_pl320_ids);

static struct platform_driver hb_pl320_driver = {
	.driver = {
		.name = "hobot-pl320",
		.of_match_table = hb_pl320_ids,
	},
	.probe = hb_pl320_probe,
	.remove = hb_pl320_remove,
	.shutdown = hb_pl320_shutdown,
};

static int __init hb_pl320_mbox_init(void)
{
	pr_info("hb_pl320_mbox_init...\n");
	return platform_driver_register(&hb_pl320_driver);
}

static void __exit hb_pl320_mbox_exit(void)
{
	pr_info("hb_pl320_mbox_exit\n");
	platform_driver_unregister(&hb_pl320_driver);
}

core_initcall(hb_pl320_mbox_init);
module_exit(hb_pl320_mbox_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PL320 mailbox driver for HOBOT");
MODULE_VERSION("1.0.0");
MODULE_AUTHOR("HOBOT");
