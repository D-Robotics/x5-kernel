/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file ipc_hal.c
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/stddef.h>
#include <linux/kfifo.h>
#include <linux/errno.h>
#include <linux/types.h>
// #include <linux/mutex_rt.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <asm/uaccess.h>
#include <asm/memory.h>
#include <linux/pid.h>
#include <linux/debugfs.h>
#include <linux/sched.h>
#include <linux/ioctl.h>

#include "ipc_hal.h"

#define IPCF_IO_MASK_CHANNEL_CONFIG	_IO(IPCF_IOC_MAGIC, 8)
#define IPCF_IO_MASK_CHANNEL_DISABLE   _IO(IPCF_IOC_MAGIC, 9)
#define IPCF_IO_CHECK_VERSION          _IOWR(IPCF_IOC_MAGIC, 10, struct ipcf_ver_check_info)
#define IPCF__VER_MAJOR                (1)
#define IPCF__VER_MINOR                (0)

/**
 * @enum ins0_chan_e
 * Define the descriptor of instance0 channel index.
 * @NO{S17E09C06}
 */
enum ins0_chan_e {
	CPU2DSP_CH0 = 0,
	CPU2DSP_CH1,
	NUM_INS0_CHAN
};

struct ipc_shm_channel_t {
	/* task info */
	pid_t pid;
	char task_comm[TASK_COMM_LEN];

	/* dev info */
	uint32_t users;
	uint32_t channel_id;
	atomic_t state;
	rwlock_t channel_lock;  /* channel protect lock */

	/* tx */
	struct mutex txbuf_lock;	/* tx buf protect lock */

	/* rx */
	uint32_t rxfifo_type;
	struct kfifo rxfifo_data;
	DECLARE_KFIFO(rxfifo_len, uint32_t, LEN_FIFO_SIZE);
	struct semaphore sem_read;  /* callback ok completion */
	spinlock_t rxfifo_lock;	 /* rx fifo protect lock between callback and ioctl */

	/* statistic info */
	struct ipc_statistic_t datalink_tx;
	struct ipc_statistic_t datalink_rx;
};

struct ipc_shm_data_t {
	uint32_t users;
	uint8_t instance;  /* instance */

	struct mutex dev_lock;	/* dev lock */
	struct ipc_shm_channel_t channel[IPCF_HAL_CHANNEL_NUM_MAX]; // channel

	/* for debug */
	struct dentry *debugfs;
	int32_t rdump, wdump;
	int32_t tsdump;
	uint32_t dumplen;
};

static struct ipc_shm_data_t ipc_shm_data;
static struct class *hal_ipc_shm_class;
static struct device *hal_ipc_shm_device;

static uint32_t bufsiz = 4096;
module_param(bufsiz, uint, S_IRUGO);
static int32_t rdump = DEV_DUMP_DISABLE;
module_param(rdump, int, 0644);
static int32_t wdump = DEV_DUMP_DISABLE;
module_param(wdump, int, 0644);
static uint32_t dumplen = 0;
module_param(dumplen, uint, 0644);
static int32_t tsdump = DEV_DUMP_DISABLE;
module_param(tsdump, int, 0644);

static void data_callback(uint8_t *userdata, int32_t instance, int32_t chan_id,
			uint8_t *buf, uint64_t size);
static int32_t hal_channel_manage_free(struct ipc_shm_data_t *_ipc_shm_data,
					uint32_t channel_id);

/**
 * poolcfg0 -- Private read-only variable.
 */
static struct ipc_pool_info poolcfg0[NUM_INS0_CHAN] = {
	{
		/*0 CPU2MCU SPI0*/
		.num_bufs = 16,
		.buf_size = 256,
	},
	{
		/*1 CPU2MCU SPI1*/
		.num_bufs = 16,
		.buf_size = 256,
	},
};

/**
 * chancfg0 -- Private read-only variable.
 */
static struct ipc_channel_info chancfg0[NUM_INS0_CHAN] = {
	{
		/*0 CPU2DSP SPI0*/
		.num_pools = IPCF_HAL_POOL_NUM,
		.pools = &poolcfg0[CPU2DSP_CH0],
		.recv_callback = data_callback,
		.userdata = (uint8_t *)&ipc_shm_data.channel[CPU2DSP_CH0],
	},
	{
		/*1 CPU2DSP SPI1*/
		.num_pools = IPCF_HAL_POOL_NUM,
		.pools = &poolcfg0[CPU2DSP_CH1],
		.recv_callback = data_callback,
		.userdata = (uint8_t *)&ipc_shm_data.channel[CPU2DSP_CH1],
	},
};

/**
 * customcfg -- Private read-only variable.
 */
static struct ipc_instance_info_m1 customcfg[IPCF_INSTANCES_NUM] = {
	{
		/*0 CPU2MCU*/
		.local_shm_addr = IPCF_HAL_LOCAL_ADDR,
		.remote_shm_addr = IPCF_HAL_REMOTE_ADDR,
		.shm_size = IPCF_HAL_SHM_SIZE,
		.num_chans = NUM_INS0_CHAN,
		.chans = &chancfg0[0],
	},
};

/**
 * ipchal_usercfg -- Private read-only variable.
 */
static struct ipc_instance_cfg ipchal_usercfg = {
		/*0 CPU2MCU*/
		.mode = CUSTOM_MODE,
		.timeout = IPCF_HAL_TIMEOUT,
		.trans_flags = SYNC_TRANS | SPIN_WAIT,
		.mbox_chan_idx = IPCF_HAL_MBOX_IDX,
		// .info.custom_cfg = customcfg[0],
};

static ssize_t dev_get_rdump(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	ssize_t ret;
	char *buf;
	size_t len = 0;

	buf = (char *)kzalloc(DEV_DUMP_BUFSIZE, GFP_KERNEL);
	if (buf == NULL) {
		shm_err("%s kzalloc failed\n", __func__);
		return 0;
	}

	len += (size_t)snprintf(&buf[len], DEV_DUMP_BUFSIZE - len, "rdump: %d\n",
			ipc_shm_data.rdump);
	ret = simple_read_from_buffer((void *)user_buf, count, ppos, (const void *)buf, len);
	kfree((const void *)buf);

	return ret;
}

static ssize_t dev_set_rdump(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	int32_t ret;

	ret = kstrtoint_from_user(buf, len, 10, &ipc_shm_data.rdump);
	if (ret != 0) {
		shm_err("%s prase event id failed\n", __func__);
		return -EINVAL;
	}

	return (ssize_t)len;
}

static const struct file_operations dev_rdump_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= dev_get_rdump,
	.write		= dev_set_rdump,
	.llseek		= default_llseek,
};

static ssize_t dev_get_wdump(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	ssize_t ret;
	char *buf;
	size_t len = 0;

	buf = (char *)kzalloc(DEV_DUMP_BUFSIZE, GFP_KERNEL);
	if (buf == NULL) {
		shm_err("%s kzalloc failed\n", __func__);
		return 0;
	}

	len += (size_t)snprintf(&buf[len], DEV_DUMP_BUFSIZE - len, "wdump: %d\n",
			ipc_shm_data.wdump);
	ret = simple_read_from_buffer((void *)user_buf, count, ppos, (const void *)buf, len);
	kfree((const void *)buf);

	return ret;
}

static ssize_t dev_set_wdump(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	int32_t ret;

	ret = kstrtoint_from_user(buf, len, 10, &ipc_shm_data.wdump);
	if (ret != 0) {
		shm_err("%s prase event id failed\n", __func__);
		return -EINVAL;
	}

	return (ssize_t)len;
}

static const struct file_operations dev_wdump_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= dev_get_wdump,
	.write		= dev_set_wdump,
	.llseek 	= default_llseek,
};

static ssize_t dev_get_dumplen(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	ssize_t ret;
	char *buf;
	size_t len = 0;

	buf = (char *)kzalloc(DEV_DUMP_BUFSIZE, GFP_KERNEL);
	if (buf == NULL) {
		shm_err("%s kzalloc failed\n", __func__);
		return 0;
	}

	len += (size_t)snprintf(&buf[len], DEV_DUMP_BUFSIZE - len, "dumplen: %d\n",
			ipc_shm_data.dumplen);
	ret = simple_read_from_buffer((void *)user_buf, count, ppos, (const void *)buf, len);
	kfree((const void *)buf);

	return ret;
}

static ssize_t dev_set_dumplen(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	int32_t ret;

	ret = kstrtouint_from_user(buf, len, 10, &ipc_shm_data.dumplen);
	if (ret != 0) {
		shm_err("%s prase event id failed\n", __func__);
		return -EINVAL;
	}

	return (ssize_t)len;
}

static const struct file_operations dev_dumplen_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= dev_get_dumplen,
	.write		= dev_set_dumplen,
	.llseek		= default_llseek,
};

static ssize_t dev_get_tsdump(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	ssize_t ret;
	char *buf;
	size_t len = 0;

	buf = (char *)kzalloc(DEV_DUMP_BUFSIZE, GFP_KERNEL);
	if (buf == NULL) {
		shm_err("%s kzalloc failed\n", __func__);
		return 0;
	}

	len += (size_t)snprintf(&buf[len], DEV_DUMP_BUFSIZE - len, "tsdump: %d\n",
			ipc_shm_data.tsdump);
	ret = simple_read_from_buffer((void *)user_buf, count, ppos, (const void *)buf, len);
	kfree((const void *)buf);

	return ret;
}

static ssize_t dev_set_tsdump(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	int32_t ret;

	ret = kstrtoint_from_user(buf, len, 10, &ipc_shm_data.tsdump);
	if (ret != 0) {
		shm_err("%s prase event id failed\n", __func__);
		return -EINVAL;
	}

	return (ssize_t)len;
}

static const struct file_operations dev_tsdump_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= dev_get_tsdump,
	.write		= dev_set_tsdump,
	.llseek 	= default_llseek,
};

static ssize_t dev_get_statistic(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	ssize_t ret;
	char *buf;
	size_t len = 0;
	int32_t ch = 0;

	buf = (char *)kzalloc(DEV_README_BUFSIZE, GFP_KERNEL);
	if (buf == NULL) {
		shm_err("%s kzalloc failed\n", __func__);

		return 0;
	}

	len += (size_t)snprintf(&buf[len], DEV_README_BUFSIZE - len,
							"DataLink:\n");
	len += (size_t)snprintf(&buf[len], DEV_README_BUFSIZE - len,
							"\t\t%10s %20s %10s %10s %10s\n",
							"pkg", "pkg_len", "err_acq", "err_shm_tx", "err_cb");
	for (ch = 0; ch < IPCF_HAL_CHANNEL_NUM_MAX; ch++) {
		len += (size_t)snprintf(&buf[len], DEV_README_BUFSIZE - len,
								"\tCH%d TX:\t%10u %20llu %10u %10u %10u\n", ch,
								ipc_shm_data.channel[ch].datalink_tx.packages,
								ipc_shm_data.channel[ch].datalink_tx.datalen,
								ipc_shm_data.channel[ch].datalink_tx.err_acq,
								ipc_shm_data.channel[ch].datalink_tx.err_shm_tx, 0);
		len += (size_t)snprintf(&buf[len], DEV_README_BUFSIZE - len,
								"\tCH%d RX:\t%10u %20llu %10u %10u %10u\n", ch,
								ipc_shm_data.channel[ch].datalink_rx.packages,
								ipc_shm_data.channel[ch].datalink_rx.datalen, 0, 0,
								ipc_shm_data.channel[ch].datalink_rx.err_cb);
	}

	ret = simple_read_from_buffer((void *)user_buf, count, ppos,
								(const void *)buf, len);
	kfree((const void *)buf);

	return ret;
}

static ssize_t dev_set_statistic(struct file *file, const char __user *buf,
				 size_t len, loff_t *ppos)
{
	int32_t ch = 0;

	for (ch = 0; ch < IPCF_HAL_CHANNEL_NUM_MAX; ch++) {
		memset(&ipc_shm_data.channel[ch].datalink_tx, 0, sizeof(struct ipc_statistic_t));
		memset(&ipc_shm_data.channel[ch].datalink_rx, 0, sizeof(struct ipc_statistic_t));
	}

	return (ssize_t)len;
}

static const struct file_operations dev_statistic_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= dev_get_statistic,
	.write		= dev_set_statistic,
	.llseek 	= default_llseek,
};

static int32_t dev_debugfs_init(struct ipc_shm_data_t *_ipc_shm_data)
{
	struct dentry *file;
	int32_t ret;
	char name[64] = {0};

	snprintf(name, sizeof(name), "ipcdrv");
	_ipc_shm_data->debugfs = debugfs_create_dir(name, NULL);
	if (IS_ERR((const void *)_ipc_shm_data->debugfs)) {
		shm_err("%s create dir[%s] failed\n", __func__, name);

		return PTR_ERR((const void *)_ipc_shm_data->debugfs);
	}

	_ipc_shm_data->rdump = rdump;
	file = debugfs_create_file("rdump", S_IFREG | S_IRUGO,
			_ipc_shm_data->debugfs, NULL, &dev_rdump_ops);
	if (IS_ERR((const void *)file)) {
		shm_err("%s create file[rdump] failed\n", __func__);
		ret = (int32_t)PTR_ERR((const void *)file);
		debugfs_remove_recursive(_ipc_shm_data->debugfs);
		_ipc_shm_data->debugfs = NULL;

		return ret;
	}

	_ipc_shm_data->wdump = wdump;
	file = debugfs_create_file("wdump", S_IFREG | S_IRUGO,
			_ipc_shm_data->debugfs, NULL, &dev_wdump_ops);
	if (IS_ERR((const void *)file)) {
		shm_err("%s create file[wdump] failed\n", __func__);
		ret = (int32_t)PTR_ERR((const void *)file);
		debugfs_remove_recursive(_ipc_shm_data->debugfs);
		_ipc_shm_data->debugfs = NULL;

		return ret;
	}

	_ipc_shm_data->dumplen = dumplen;
	file = debugfs_create_file("dumplen", S_IFREG | S_IRUGO,
			_ipc_shm_data->debugfs, NULL, &dev_dumplen_ops);
	if (IS_ERR((const void *)file)) {
		shm_err("%s create file[dumplen] failed\n", __func__);
		ret = (int32_t)PTR_ERR((const void *)file);
		debugfs_remove_recursive(_ipc_shm_data->debugfs);
		_ipc_shm_data->debugfs = NULL;

		return ret;
	}

	_ipc_shm_data->tsdump = tsdump;
	file = debugfs_create_file("tsdump", S_IFREG | S_IRUGO,
			_ipc_shm_data->debugfs, NULL, &dev_tsdump_ops);
	if (IS_ERR((const void *)file)) {
		shm_err("%s create file[tsdump] failed\n", __func__);
		ret = (int32_t)PTR_ERR((const void *)file);
		debugfs_remove_recursive(_ipc_shm_data->debugfs);
		_ipc_shm_data->debugfs = NULL;

		return ret;
	}

	file = debugfs_create_file("statistic", S_IFREG | S_IRUGO,
			_ipc_shm_data->debugfs, NULL, &dev_statistic_ops);
	if (IS_ERR((const void *)file)) {
		shm_err("%s create file[statistic] failed\n", __func__);
		ret = (int32_t)PTR_ERR((const void *)file);
		debugfs_remove_recursive(_ipc_shm_data->debugfs);
		_ipc_shm_data->debugfs = NULL;

		return ret;
	}

	return 0;
}

static void dev_debugfs_remove(struct ipc_shm_data_t *_ipc_shm_data)
{
	debugfs_remove_recursive(_ipc_shm_data->debugfs);
	_ipc_shm_data->debugfs = NULL;
}

static inline void dev_print_timestamp(char *str)
{
	struct timespec64 ts;
	ktime_get_real_ts64(&ts);

	if (str == NULL)
		shm_info("%09lld.%09ld", ts.tv_sec, ts.tv_nsec);
	else
		shm_info("%s: %09lld.%09ld", str, ts.tv_sec, ts.tv_nsec);

	return;
}

static void ipcf_dump_data(char *str, uint8_t *buf, int32_t len)
{
	uint32_t i = 0, j = 0;
	uint32_t m = 32;
	uint32_t mul = len / m;
	uint32_t remain = len % m;
	uint8_t tmp_buf[256];
	uint32_t tmp_buf_size = 256;
	size_t tmp_len = 0;

	if (str != NULL)
		shm_info("dump info: %s len[%d] mul[%d] remain[%d]\n", str, len, mul, remain);

	for (i = 0; i < mul; i++)
		shm_info("0x%04x: %02X %02X %02X %02X %02X %02X %02X %02X "
			"%02X %02X %02X %02X %02X %02X %02X %02X "
			"%02X %02X %02X %02X %02X %02X %02X %02X "
			"%02X %02X %02X %02X %02X %02X %02X %02X\n", i * m,
			buf[i * m + 0], buf[i * m + 1], buf[i * m + 2], buf[i * m + 3],
			buf[i * m + 4], buf[i * m + 5], buf[i * m + 6], buf[i * m + 7],
			buf[i * m + 8], buf[i * m + 9], buf[i * m + 10], buf[i * m + 11],
			buf[i * m + 12], buf[i * m + 13], buf[i * m + 14], buf[i * m + 15],
			buf[i * m + 16], buf[i * m + 17], buf[i * m + 18], buf[i * m + 19],
			buf[i * m + 20], buf[i * m + 21], buf[i * m + 22], buf[i * m + 23],
			buf[i * m + 24], buf[i * m + 25], buf[i * m + 26], buf[i * m + 27],
			buf[i * m + 28], buf[i * m + 29], buf[i * m + 30], buf[i * m + 31]);

	if (remain > 0) {
		for (j = 0; j < remain; j++)
			tmp_len += snprintf(&tmp_buf[tmp_len], tmp_buf_size - tmp_len, "%02X ", buf[mul * m + j]);
		shm_info("0x%04x: %s\n", mul * m, tmp_buf);
	}

	return;
}

static int32_t hal_ipc_shm_init(struct ipc_shm_data_t *_ipc_shm_data)
{
	int32_t retval = 0;
	ipchal_usercfg.info.custom_cfg = customcfg[0];
	retval = hb_ipc_open_instance(IPCF_INSTANCES_ID0, &ipchal_usercfg);
	if (!retval) {
		shm_dbg("ipc_shm_init success, num instance %d num channel %d\n",
			 IPCF_INSTANCES_NUM, IPCF_HAL_CHANNEL_NUM_MAX);
	} else {
		shm_err("ipc_shm_init failed, ret %d!\n", retval);

		return retval;
	}

	return 0;
}

static void hal_ipc_shm_channel_init(struct ipc_shm_data_t *_ipc_shm_data)
{
	int32_t ch = 0;
	_ipc_shm_data->users++;
	_ipc_shm_data->instance = IPCF_INSTANCES_ID0;
	mutex_init(&_ipc_shm_data->dev_lock);

	for (ch = 0; ch < IPCF_HAL_CHANNEL_NUM_MAX; ch++) {
		rwlock_init(&_ipc_shm_data->channel[ch].channel_lock);
		mutex_init(&_ipc_shm_data->channel[ch].txbuf_lock);
		spin_lock_init(&_ipc_shm_data->channel[ch].rxfifo_lock);
		sema_init(&_ipc_shm_data->channel[ch].sem_read, 0);
		atomic_set(&_ipc_shm_data->channel[ch].state, 0);
	}
}

static void hal_ipc_shm_channel_free(struct ipc_shm_data_t *_ipc_shm_data)
{
	int32_t retval = 0, ch = 0;

	_ipc_shm_data->users = 0;
	_ipc_shm_data->instance = 0;

	for (ch = 0; ch < IPCF_HAL_CHANNEL_NUM_MAX; ch++) {
		retval = hal_channel_manage_free(_ipc_shm_data, ch);
		if (retval) {
			shm_err("channel %d free failed, ret %d.\n", ch, retval);
		}
	}
}

static int32_t hal_channel_manage_init(struct ipc_shm_data_t *_ipc_shm_data,
					uint32_t channel_id, uint32_t fifo_size,
					uint32_t fifo_type)
{
	struct task_struct *task = current;
	unsigned long spinlock_flags;

	if (channel_id >= IPCF_HAL_CHANNEL_NUM_MAX) {
		shm_err("%s(%d) %s invalid param channel id, must be loss than %d.\n",
				current->comm, task_pid_nr(current), __func__, IPCF_HAL_CHANNEL_NUM_MAX);
		return -EINVAL;
	}

	atomic_set(&_ipc_shm_data->channel[channel_id].state, 0); /* disable channel */

	write_lock(&_ipc_shm_data->channel[channel_id].channel_lock);
	_ipc_shm_data->channel[channel_id].channel_id = channel_id;
	snprintf(_ipc_shm_data->channel[channel_id].task_comm, TASK_COMM_LEN, task->comm);
	_ipc_shm_data->channel[channel_id].pid = task->pid; 
	_ipc_shm_data->channel[channel_id].rxfifo_type = fifo_type;

	if (!fifo_size)
		fifo_size = DATA_FIFO_SIZE;
	spin_lock_irqsave(&_ipc_shm_data->channel[channel_id].rxfifo_lock, spinlock_flags);
	if (kfifo_alloc(&_ipc_shm_data->channel[channel_id].rxfifo_data, fifo_size, GFP_KERNEL)) {
		shm_err("[Ins %d channel %d] %s(%d) rx fifo alloc failed.\n",
				_ipc_shm_data->instance, _ipc_shm_data->channel[channel_id].channel_id, 
				_ipc_shm_data->channel[channel_id].task_comm, _ipc_shm_data->channel[channel_id].pid);
		spin_unlock_irqrestore(&_ipc_shm_data->channel[channel_id].rxfifo_lock, spinlock_flags);
		write_unlock(&_ipc_shm_data->channel[channel_id].channel_lock);

		return -ENOMEM;
	}

	INIT_KFIFO(_ipc_shm_data->channel[channel_id].rxfifo_len);
	spin_unlock_irqrestore(&_ipc_shm_data->channel[channel_id].rxfifo_lock, spinlock_flags);
	write_unlock(&_ipc_shm_data->channel[channel_id].channel_lock);

	atomic_set(&_ipc_shm_data->channel[channel_id].state, 1); /* enable channel */

	shm_info("[Ins %d channel %d] %s(%d) channel register success.\n",
			_ipc_shm_data->instance, _ipc_shm_data->channel[channel_id].channel_id,
			_ipc_shm_data->channel[channel_id].task_comm, _ipc_shm_data->channel[channel_id].pid);

	return 0;
}

static int32_t hal_channel_manage_free(struct ipc_shm_data_t *_ipc_shm_data,
					uint32_t channel_id)
{
	unsigned long spinlock_flags;

	if (channel_id >= IPCF_HAL_CHANNEL_NUM_MAX) {
		shm_err("%s(%d) %s invalid param channel id, must be loss than %d.\n",
			 current->comm, task_pid_nr(current), __func__, IPCF_HAL_CHANNEL_NUM_MAX);

		return -EINVAL;
	}

	atomic_set(&_ipc_shm_data->channel[channel_id].state, 0); /* disable channel */

	write_lock(&_ipc_shm_data->channel[channel_id].channel_lock);
	_ipc_shm_data->channel[channel_id].channel_id = 0;
	memset(_ipc_shm_data->channel[channel_id].task_comm, 0, TASK_COMM_LEN);
	_ipc_shm_data->channel[channel_id].pid = 0; 
	_ipc_shm_data->channel[channel_id].rxfifo_type = 0;

	spin_lock_irqsave(&_ipc_shm_data->channel[channel_id].rxfifo_lock, spinlock_flags);
	kfifo_reset(&_ipc_shm_data->channel[channel_id].rxfifo_len);
	kfifo_free(&_ipc_shm_data->channel[channel_id].rxfifo_data);
	spin_unlock_irqrestore(&_ipc_shm_data->channel[channel_id].rxfifo_lock, spinlock_flags);

	sema_init(&_ipc_shm_data->channel[channel_id].sem_read, 0);
	memset(&_ipc_shm_data->channel[channel_id].datalink_tx, 0, sizeof(struct ipc_statistic_t));
	memset(&_ipc_shm_data->channel[channel_id].datalink_rx, 0, sizeof(struct ipc_statistic_t));
	write_unlock(&_ipc_shm_data->channel[channel_id].channel_lock);

	shm_info("[Ins %d channel %d] %s(%d) channel un-register success.\n",
		 _ipc_shm_data->instance, channel_id, current->comm, task_pid_nr(current));

	return 0;
}

static void data_callback(uint8_t *userdata, int32_t instance, int32_t chan_id,
			  uint8_t *buf, uint64_t size)
{
	int32_t retval = 0;
	uint32_t avail = 0;
	bool save_sta = false;
	unsigned long spinlock_flags = 0;
	int32_t dumplen = 0;
	struct ipc_shm_channel_t *channel = (struct ipc_shm_channel_t *)userdata;

	if (hb_ipc_is_remote_ready(instance))
		return;

	if (size <= 0)
		return;

	if (channel == NULL || buf == NULL || size > IPCF_HAL_BUF_SIZE) {
		retval = hb_ipc_release_buf(instance, chan_id, buf);

		return;
	}

	if (!atomic_read(&channel->state)) {
		retval = hb_ipc_release_buf(instance, chan_id, buf);
		shm_dbg("[Ins %d channel %d] %s channel is not enable\n", instance, chan_id, __func__);
		shm_dbg("channel_id %d, pid %d\n", channel->channel_id, channel->pid);

		return;
	}

	/* Judge whether fifo has free space before fifo in */
	spin_lock_irqsave(&channel->rxfifo_lock, spinlock_flags);
	avail = kfifo_avail(&channel->rxfifo_data);
	if (size > avail || kfifo_is_full(&channel->rxfifo_len)) {
		channel->datalink_rx.err_cb++;
	} else {
		kfifo_in(&channel->rxfifo_data, buf, size);
		kfifo_put(&channel->rxfifo_len, size);
		save_sta = true;
		channel->datalink_rx.packages++;
		channel->datalink_rx.datalen += size;
	}
	spin_unlock_irqrestore(&channel->rxfifo_lock, spinlock_flags);

	if (save_sta) {
		up(&channel->sem_read);
		shm_dbg("[Ins %d channel %d] success fifo in len %llu\n", instance, chan_id, size);
	} else {
		shm_log_ratelimited(KERN_ERR, "[Ins %d channel %d] failed fifo in len %llu avail %u cnt %u.\n",
					instance, chan_id, size, avail, channel->datalink_rx.err_cb);
	}

	/* for debug */
	if (ipc_shm_data.tsdump >= IPCF_HAL_CHANNEL_NUM_MIN)
		dev_print_timestamp("rx callback");
	if (ipc_shm_data.rdump == chan_id) {
		if (0 < ipc_shm_data.dumplen && ipc_shm_data.dumplen < size)
			dumplen = ipc_shm_data.dumplen;
		else
			dumplen = size;
		shm_info("[Ins %d channel %d] callback size %llu\n", instance, chan_id, size);
		ipcf_dump_data("callback rx", buf, dumplen);
	}

	retval = hb_ipc_release_buf(instance, chan_id, buf);
}

static ssize_t hal_ipc_shm_write(struct file *file, const char __user * buf,
				size_t count, loff_t *ppos)
{
	int32_t retval = 0;
	uint8_t *tx_buf = NULL;
	struct {
		uint32_t channel_id;
		uint64_t buffer;
		uint32_t len;
	} data;

	/* copy data info from user space write() */
	if (copy_from_user(&data, (const uint8_t __user *)buf, count)) {
		shm_err("%s(%d) %s copy_from_user failed, data fail\n",
			current->comm, task_pid_nr(current), __func__);

		return -EPERM;
	}

	/* param check */
	if (data.channel_id >= IPCF_HAL_CHANNEL_NUM_MAX) {
		shm_log_ratelimited(KERN_ERR, "%s(%d) %s invalid param channel id, must be loss than %d.\n",
				    current->comm, task_pid_nr(current), __func__, IPCF_HAL_CHANNEL_NUM_MAX);

		return -EINVAL;
	}

	if (!atomic_read(&ipc_shm_data.channel[data.channel_id].state)) {
		shm_log_ratelimited(KERN_ERR, "%s(%d) %s channel is not enable.\n",
				    current->comm, task_pid_nr(current), __func__);

		return -EAGAIN;
	}

	if (data.len > IPCF_HAL_BUF_SIZE) {
		shm_log_ratelimited(KERN_ERR, "%s(%d) tx data len %u to long, must be %d.\n",
				    current->comm, task_pid_nr(current), data.len, IPCF_HAL_BUF_SIZE);

		return -EOVERFLOW;
	}

	/* write buf, need lock TX Buffer */
	mutex_lock(&ipc_shm_data.channel[data.channel_id].txbuf_lock);
	retval = hb_ipc_acquire_buf(IPCF_INSTANCES_ID0, data.channel_id, data.len, &tx_buf);
	if (!tx_buf || retval) {
		ipc_shm_data.channel[data.channel_id].datalink_tx.err_acq++;
		shm_log_ratelimited(KERN_ERR, "[Ins %d channel %d] %s(%d) can't obtain buf size %u from channel cnt %u.\n",
				ipc_shm_data.instance, data.channel_id, current->comm, task_pid_nr(current), data.len,
				ipc_shm_data.channel[data.channel_id].datalink_tx.err_acq);
		mutex_unlock(&ipc_shm_data.channel[data.channel_id].txbuf_lock);

		return -EAGAIN;
	}
	if (copy_from_user(tx_buf, (const uint8_t __user *)data.buffer, data.len)) {
		shm_err("[Ins %d channel %d] %s(%d) copy_from_user failed, buffer fail.\n",
				ipc_shm_data.instance, data.channel_id, current->comm, task_pid_nr(current));
		mutex_unlock(&ipc_shm_data.channel[data.channel_id].txbuf_lock);

		return -EPERM;
	}

	retval = hb_ipc_send(IPCF_INSTANCES_ID0, data.channel_id, tx_buf, data.len);
	if (retval < 0) {
		ipc_shm_data.channel[data.channel_id].datalink_tx.err_shm_tx++;
		shm_log_ratelimited(KERN_ERR, "[Ins %d channel %d] %s(%d) tx size %u failed, ret %d cnt %u.\n",
				ipc_shm_data.instance, data.channel_id, current->comm, task_pid_nr(current), data.len, retval,
				ipc_shm_data.channel[data.channel_id].datalink_tx.err_shm_tx);
		mutex_unlock(&ipc_shm_data.channel[data.channel_id].txbuf_lock);

		return -EBADE;
	}
	ipc_shm_data.channel[data.channel_id].datalink_tx.packages++;
	ipc_shm_data.channel[data.channel_id].datalink_tx.datalen += data.len;

	/*for debug */
	if (ipc_shm_data.tsdump >= IPCF_HAL_CHANNEL_NUM_MIN)
		dev_print_timestamp("tx write");
	if (ipc_shm_data.wdump == data.channel_id) {
		shm_info("[Ins %d channel %d] tx size %u", ipc_shm_data.instance, data.channel_id, data.len);
		if (0 < ipc_shm_data.dumplen && ipc_shm_data.dumplen < data.len)
			dumplen = ipc_shm_data.dumplen;
		else
			dumplen = data.len;
		ipcf_dump_data("tx data", tx_buf, dumplen);
	}

	mutex_unlock(&ipc_shm_data.channel[data.channel_id].txbuf_lock);

	shm_dbg("[Ins %d channel %d] %s(%d) tx size %u success, ret %d.\n",
			ipc_shm_data.instance, data.channel_id, current->comm,
			task_pid_nr(current), data.len, retval);

	return retval ? retval : data.len;
}

static ssize_t hal_ipc_shm_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	int32_t retval = 0;
	uint32_t frame_len = 0, copied = 0;
	struct {
		uint32_t channel_id;
		uint64_t buffer;
		uint32_t len;
		int32_t timeout;
		uint32_t pkg_num;
	} data;

	/* copy data info from user space write() */
	if (copy_from_user(&data, (const uint8_t __user *)buf, count)) {
		shm_err("[%s %d] %s copy_from_user failed, data fail\n",
			current->comm, task_pid_nr(current), __func__);

		return -EPERM;
	}

	/* param check */
	if (data.channel_id >= IPCF_HAL_CHANNEL_NUM_MAX) {
		shm_log_ratelimited(KERN_ERR, "%s(%d) %s invalid param channel id, must be loss than %d.\n",
				current->comm, task_pid_nr(current), __func__, IPCF_HAL_CHANNEL_NUM_MAX);

		return -EINVAL;
	}

	/* channel state check */
	if (!atomic_read(&ipc_shm_data.channel[data.channel_id].state)) {
		shm_log_ratelimited(KERN_ERR, "%s(%d) %s channel is not enable.\n",
				current->comm, task_pid_nr(current), __func__);

		return -EAGAIN;
	}

	if (unlikely(data.timeout == 0)) {
		if (down_trylock(&ipc_shm_data.channel[data.channel_id].sem_read)) return 0;
	} else if (unlikely(data.timeout < 0)) {
		if (down_interruptible(&ipc_shm_data.channel[data.channel_id].sem_read)) return -ERESTART;
	} else {
		data.timeout = data.timeout > 10000 ? 10000 : data.timeout;
		if (down_timeout(&ipc_shm_data.channel[data.channel_id].sem_read,
				msecs_to_jiffies(data.timeout))) {
			shm_dbg("[Ins %d channel %d] %s(%d) read timeout %d.\n",
				ipc_shm_data.instance, data.channel_id, current->comm,
				task_pid_nr(current), data.timeout);

			return -ETIMEDOUT;
		}
	}

	/* channel state check */
	if (!atomic_read(&ipc_shm_data.channel[data.channel_id].state)) {
		shm_log_ratelimited(KERN_ERR, "%s(%d) %s channel is not enable.\n",
				current->comm, task_pid_nr(current), __func__);

		return -EAGAIN;
	}

	// get frame len from len_fifo
	read_lock(&ipc_shm_data.channel[data.channel_id].channel_lock);
	if (!kfifo_peek(&ipc_shm_data.channel[data.channel_id].rxfifo_len, &frame_len)) {
		shm_err("[Ins %d channel %d] %s(%d) rx fifo no data.\n",
			ipc_shm_data.instance, data.channel_id, current->comm,
			task_pid_nr(current));
		read_unlock(&ipc_shm_data.channel[data.channel_id].channel_lock);

		return -EINVAL;
	}

	if (!kfifo_get(&ipc_shm_data.channel[data.channel_id].rxfifo_len, &frame_len)) {
		shm_err("[Ins %d channel %d] %s(%d) get rx fifo len failed.\n",
			ipc_shm_data.instance, data.channel_id, current->comm,
			task_pid_nr(current));
		read_unlock(&ipc_shm_data.channel[data.channel_id].channel_lock);

		return -EFAULT;
	}

	if (data.len < frame_len) {
		kfifo_out_throw(&ipc_shm_data.channel[data.channel_id].rxfifo_data,
						frame_len);
		shm_log_ratelimited(KERN_ERR, "[Ins %d channel %d] %s(%d) rx fifo data len too long, throw it.\n",
				ipc_shm_data.instance, data.channel_id, current->comm,
				task_pid_nr(current));
		read_unlock(&ipc_shm_data.channel[data.channel_id].channel_lock);

		return -EMSGSIZE;
	}

	retval = kfifo_to_user(&ipc_shm_data.channel[data.channel_id].rxfifo_data,
							(char __user *)data.buffer, frame_len, &copied);
	read_unlock(&ipc_shm_data.channel[data.channel_id].channel_lock);

	return retval < 0 ? retval : copied;
}

static int32_t ioctl_check_version(struct ipcf_ver_check_info __user* pver)
{
	int32_t ret = 0;
	struct ipcf_ver_check_info ver = {
		.major = IPCF__VER_MAJOR,
		.minor = IPCF__VER_MINOR,
	};

	if (0 == access_ok(pver, sizeof(struct ipcf_ver_check_info))) {
		shm_err("pver is invalid\n");

		return -EINVAL;
	}

	ret = copy_to_user(pver, &ver, sizeof(struct ipcf_ver_check_info));
	if (ret < 0) {
		shm_err("copy_to_user failed, error: %d\n", ret);

		return -EFAULT;
	}

	return 0;
}

static long hal_ipc_shm_ioctl(struct file *file, uint32_t cmd, unsigned long arg)
{
	int32_t	retval = 0;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != IPCF_IOC_MAGIC)
		return -ENOTTY;

	/* use the dev lock here for triple duty:
	 *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
	 */
	mutex_lock(&ipc_shm_data.dev_lock);

	switch (cmd) {
	/* read requests */
	case IPCF_IO_MASK_CHANNEL_CONFIG: {
		struct {
			uint32_t channel_id;
			uint32_t fifo_size;
			uint32_t fifo_type;
		} config;

		if (copy_from_user(&config.channel_id, (void __user *)arg, sizeof(uint32_t))) {
			shm_err("%s(%d) %s ioctl enable ipcf message channel id failed\n",
				current->comm, task_pid_nr(current), __func__);
			retval = -EFAULT;
			break;
		}
		/* param check */
		if (config.channel_id >= IPCF_HAL_CHANNEL_NUM_MAX) {
			shm_err("%s(%d) %s invalid param channel id, must be loss than %d.\n",
				current->comm, task_pid_nr(current), __func__, IPCF_HAL_CHANNEL_NUM_MAX);
			retval = -EINVAL;
			break;
		}

		arg += sizeof(uint32_t);
		if (copy_from_user(&config.fifo_size, (void __user *)arg, sizeof(uint32_t))) {
			shm_err("%s(%d) %s ioctl ipcf message channel size failed\n",
				current->comm, task_pid_nr(current), __func__);
			retval = -EFAULT;
			break;
		}
		arg += sizeof(uint32_t);
		if (copy_from_user(&config.fifo_type, (void __user *)arg, sizeof(uint32_t))) {
			shm_err("%s(%d) %s ioctl ipcf message channel type failed\n",
				current->comm, task_pid_nr(current), __func__);
			retval = -EFAULT;
			break;
		}

		retval = hal_channel_manage_init(&ipc_shm_data, config.channel_id, config.fifo_size, config.fifo_type);
		if (retval) {
			shm_err("%s(%d) %s channel %u manage init failed, ret %d.\n",
				current->comm, task_pid_nr(current), __func__, config.channel_id, retval);
		}
		break;
	}
	case IPCF_IO_MASK_CHANNEL_DISABLE: {
		uint32_t channel_id = 0;

		if (copy_from_user(&channel_id, (void __user *)arg, sizeof(uint32_t))) {
			shm_err("%s(%d) %s ioctl disable ipcf message channel id failed\n",
				current->comm, task_pid_nr(current), __func__);
			retval = -EFAULT;
			break;
		}
		/* param check */
		if (channel_id >= IPCF_HAL_CHANNEL_NUM_MAX) {
			shm_err("%s(%d) %s invalid param channel id, must be loss than %d.\n",
				current->comm, task_pid_nr(current), __func__, IPCF_HAL_CHANNEL_NUM_MAX);
			retval = -EINVAL;
			break;
		}

		if ((retval = hal_channel_manage_free(&ipc_shm_data, channel_id))) {
			shm_err("%s(%d) %s channel %u manage free failed, ret %d.\n",
				current->comm, task_pid_nr(current), __func__, channel_id, retval);
		}
		break;
	}
	case IPCF_IO_CHECK_VERSION: {
		retval = ioctl_check_version((struct ipcf_ver_check_info __user *)arg);
		break;
	}
	default:
		break;
	}
	mutex_unlock(&ipc_shm_data.dev_lock);

	return retval;
}

static struct file_operations hal_ipc_shm_flops = {
	.owner = THIS_MODULE,
	.write = hal_ipc_shm_write,
	.read  = hal_ipc_shm_read,
	.unlocked_ioctl = hal_ipc_shm_ioctl,
	.llseek = no_llseek,
};


static int32_t __init hal_ipc_shm_drv_init(void)
{
	int32_t ret;

	shm_dbg("IPCF device start initializ...\n");
	ret = register_chrdev(DEVICE_MAJOR, DEVICE_NAME, &hal_ipc_shm_flops);
	if (ret < 0) {
		shm_err("IPCF device can't register major number.\n");

		return ret;
	}

	hal_ipc_shm_class = class_create(THIS_MODULE, "ipcdrv0");
	hal_ipc_shm_device = device_create(hal_ipc_shm_class, NULL, MKDEV(DEVICE_MAJOR, 0), NULL, "ipcdrv");
	ret = dev_debugfs_init(&ipc_shm_data);
	if (ret) {
		shm_err("IPCF debugfs init failed.\n");

		return ret;
	}

	/* ipc channel init */
	hal_ipc_shm_channel_init(&ipc_shm_data);

	/* ipc init */
	ret = hal_ipc_shm_init(&ipc_shm_data);
	if( ret != 0 ) {
		shm_err("IPCF init failed.\n");

		return ret;
	}

	shm_info("IPCF device init success.\n");

	return 0;
}

static void __exit hal_ipc_shm_drv_exit(void)
{
	shm_info("IPCF device remove....\n");
	/* channel free */
	hal_ipc_shm_channel_free(&ipc_shm_data);

	/* ipc free */
	hb_ipc_close_instance(IPCF_INSTANCES_ID0);
	dev_debugfs_remove(&ipc_shm_data);
	device_unregister(hal_ipc_shm_device);
	class_destroy(hal_ipc_shm_class);
	unregister_chrdev(DEVICE_MAJOR, DEVICE_NAME);

	shm_info("IPCF device removed.\n");
}

module_init(hal_ipc_shm_drv_init);
module_exit(hal_ipc_shm_drv_exit);

MODULE_VERSION("1.0.0");
MODULE_DESCRIPTION("User mode Dummy IPCF Hal device interface");
MODULE_LICENSE("GPL v2");
