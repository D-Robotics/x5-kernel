
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/completion.h>
#include <linux/of_address.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/delay.h>
#include "hb_ipc_interface.h"


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hobot IPC Sample Application Module");

#define MODULE_NAME "hobot-ipc-test"
#define MODULE_VER "0.1"

#define SYNC_TRANS (1)
#define SPIN_WAIT (0x4)
#define INSTANCE_NUM (3)

static struct kobject *g_ipc_kobj;
struct kobj_attribute g_test_cmd[INSTANCE_NUM];

const char g_usage[] = "echo 0 > /sys/kernel/hobot-ipc-test/ping";

void data_chan_rx_cb(uint8_t *arg, int32_t instance, int32_t chan_id, uint8_t *buf,
	uint64_t size)
{
	int32_t i = 0;

	if (buf == NULL) {
		pr_err("%s[%d]: buf invalid parameter\n", __func__,__LINE__);
		return;
	}

	if (size <= 0) {
		pr_err("%s[%d]: size invalid parameter: %#llx\n", __func__,__LINE__,size);
		return;
	}

	for (i = 0; i < size; i++) {
		if (buf[i] != 0x55)
			break;
	}
	if (i < size)
		pr_err("%s[%d] buffer verify failed !!! bif[%d] : %x\n", __func__,__LINE__, i, buf[i]);
	else
		pr_err("%s[%d] buffer verify succeed !!!\n", __func__,__LINE__);

	if(hb_ipc_release_buf(instance, chan_id, buf))
		pr_err("%s[%d]: hb_ipc_release_buf failed\n", __func__,__LINE__);

	pr_info("%s[%d]: recv release buf success\n", __func__,__LINE__);
}

static struct ipc_instance_cfg test_cfg_instances[INSTANCE_NUM] = {
	{
		.mode = 0, /**< work mode, 0 default mode, 1 custom mode*/
		.timeout = 10000,
		.trans_flags = SYNC_TRANS | SPIN_WAIT,
		.mbox_chan_idx = 0,
		.info = {
			.def_cfg = {
				.recv_callback = data_chan_rx_cb,
				.userdata  = NULL,
			},
		},
	},
	{
		.mode = 0, /**< work mode, 0 default mode, 1 custom mode*/
		.timeout = 10000,
		.trans_flags = SYNC_TRANS | SPIN_WAIT,
		.mbox_chan_idx = 0,
		.info = {
			.def_cfg = {
				.recv_callback = data_chan_rx_cb,
				.userdata  = NULL,
			},
		},
	},
	{
		.mode = 0, /**< work mode, 0 default mode, 1 custom mode*/
		.timeout = 10000,
		.trans_flags = SYNC_TRANS | SPIN_WAIT,
		.mbox_chan_idx = 0,
		.info = {
			.def_cfg = {
				.recv_callback = data_chan_rx_cb,
				.userdata  = NULL,
			},
		},
	},
};

static ssize_t show_test_cmd0(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return 0;
}

static int32_t clear_local(dma_addr_t addr, uint32_t size)
{
	void *va = NULL;
	int32_t ret = 0, i = 0;
	va = ioremap(addr, size);
	if (!va) {
		ret = -1;
		pr_err("%s ioremap 0x%llx:0x%x error\n", __func__, addr, size);
		return -1;
	}
	for (i = 0; i < size; i++) {
		writeb((char)0, va + i);
	}

	iounmap(va);
	return 0;
}

static ssize_t store_test_cmd0(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t count)
{
	uint32_t cmd = 0;
	int32_t ret;
	uint8_t *send_buf;
	uint32_t size = 32;
	int32_t ready_timeout = 0;
	int32_t instance = 0;

	if (buf == NULL) {
		pr_info("%s[%d]: ipc_test_sysfs_store buf is NULL\n", __func__,__LINE__);
		return -1;
	}

	if (count <= 0) {
		pr_info("%s[%d]: ipc_test_sysfs_store count :%ld\n", __func__,__LINE__,count);
		return -1;
	}

	ret = kstrtouint(buf, 10, &cmd);
	if (ret) {
		pr_err("%s Invalid Injection. %d\n", __FUNCTION__, ret);
		return -1;
	}

	pr_info("%s[%d]: ipc_test_sysfs_store input msg is: %d\n", __func__, __LINE__, cmd);

	switch (cmd) {
	case 0:
		if (hb_ipc_open_instance(instance, &test_cfg_instances[instance]))
			pr_info("%s[%d]: instance init failed", __func__,__LINE__);
		else
			pr_info("%s[%d]: instance init success", __func__,__LINE__);
		break;
	case 1:
		ready_timeout = 10;
		while ((hb_ipc_is_remote_ready(instance)) && ready_timeout) {
			msleep(1);
			ready_timeout--;
		}
		if (ready_timeout <= 0)
			pr_err("%s[%d]: is ready failed", __func__,__LINE__);
		else
			pr_info("%s[%d]: is ready success", __func__,__LINE__);
		break;
	case 2:
		if (hb_ipc_acquire_buf(instance, 0, size, &send_buf)) {
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
			memset(send_buf, 0x55, 100);
		}
		if (hb_ipc_send(instance, 0, send_buf, size))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		break;
	case 3:
		if (hb_ipc_acquire_buf(instance, 1, size, &send_buf)){ 
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
			memset(send_buf, 0x55, 100);
		}
		if (hb_ipc_send(instance, 1, send_buf, size))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		break;
	case 4:
		if (hb_ipc_acquire_buf(instance, 1, 1, &send_buf)) {
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
		}
		send_buf[0] = 0x24;
		if (hb_ipc_send(instance, 1, send_buf, 1))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		hb_ipc_close_instance(instance);
		break;
	case 5:
		if (hb_ipc_acquire_buf(instance, 1, 1, &send_buf)) {
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
		}
		send_buf[0] = 0x36;
		if (hb_ipc_send(instance, 1, send_buf, 1))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		break;
	case 6:
		clear_local(0x1ff00000, 0x10000);
		break;
	case 7:
		clear_local(0xd3000000, 0x800);
		break;
	default :
		pr_info("%s[%d]: please input valid command\n", __func__,__LINE__);
		break;
	}
	return count;
}

static ssize_t show_test_cmd1(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return 0;
}

static ssize_t store_test_cmd1(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t count)
{
	uint32_t cmd = 0;
	int32_t ret;
	uint8_t *send_buf;
	uint32_t size = 32;
	int32_t ready_timeout = 0;
	int32_t instance = 1;

	if (buf == NULL) {
		pr_info("%s[%d]: ipc_test_sysfs_store buf is NULL\n", __func__,__LINE__);
		return -1;
	}

	if (count <= 0) {
		pr_info("%s[%d]: ipc_test_sysfs_store count :%ld\n", __func__,__LINE__,count);
		return -1;
	}

	ret = kstrtouint(buf, 10, &cmd);
	if (ret) {
		pr_err("%s Invalid Injection. %d\n", __FUNCTION__, ret);
		return -1;
	}

	pr_info("%s[%d]: ipc_test_sysfs_store input msg is: %d\n", __func__, __LINE__, cmd);

	switch (cmd) {
	case 0:
		if (hb_ipc_open_instance(instance, &test_cfg_instances[instance]))
			pr_info("%s[%d]: instance init failed", __func__,__LINE__);
		else
			pr_info("%s[%d]: instance init success", __func__,__LINE__);
		break;
	case 1:
		ready_timeout = 10;
		while ((hb_ipc_is_remote_ready(instance)) && ready_timeout) {
			msleep(1);
			ready_timeout--;
		}
		if (ready_timeout <= 0)
			pr_err("%s[%d]: is ready failed", __func__,__LINE__);
		else
			pr_info("%s[%d]: is ready success", __func__,__LINE__);
		break;
	case 2:
		if (hb_ipc_acquire_buf(instance, 0, size, &send_buf)) {
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
			memset(send_buf, 0x55, 100);
		}
		if (hb_ipc_send(instance, 0, send_buf, size))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		break;
	case 3:
		if (hb_ipc_acquire_buf(instance, 1, size, &send_buf)) {
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
			memset(send_buf, 0x55, 100);
		}
		if (hb_ipc_send(instance, 1, send_buf, size))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		break;
	case 4:
		if (hb_ipc_acquire_buf(instance, 1, 1, &send_buf)) {
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
		}
		send_buf[0] = 0x24;
		if (hb_ipc_send(instance, 1, send_buf, 1))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		hb_ipc_close_instance(instance);
		break;
	case 5:
		if (hb_ipc_acquire_buf(instance, 1, 1, &send_buf)) {
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
		}
		send_buf[0] = 0x36;
		if (hb_ipc_send(instance, 1, send_buf, 1))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		break;
	case 6:
		clear_local(0x1ff00000, 0x10000);
		break;
	case 7:
		clear_local(0xd3000800, 0x800);
		break;
	default :
		pr_info("%s[%d]: please input valid command\n", __func__,__LINE__);
		break;
	}
	return count;
}

static ssize_t show_test_cmd2(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return 0;
}

static ssize_t store_test_cmd2(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t count)
{
	uint32_t cmd = 0;
	int32_t ret;
	uint8_t *send_buf;
	uint32_t size = 32;
	int32_t ready_timeout = 0;
	int32_t instance = 2;

	if (buf == NULL) {
		pr_info("%s[%d]: ipc_test_sysfs_store buf is NULL\n", __func__,__LINE__);
		return -1;
	}

	if (count <= 0) {
		pr_info("%s[%d]: ipc_test_sysfs_store count :%ld\n", __func__,__LINE__,count);
		return -1;
	}

	ret = kstrtouint(buf, 10, &cmd);
	if (ret) {
		pr_err("%s Invalid Injection. %d\n", __FUNCTION__, ret);
		return -1;
	}

	pr_info("%s[%d]: ipc_test_sysfs_store input msg is: %d\n", __func__, __LINE__, cmd);

	switch (cmd) {
	case 0:
		if (hb_ipc_open_instance(instance, &test_cfg_instances[instance]))
			pr_info("%s[%d]: instance init failed", __func__,__LINE__);
		else
			pr_info("%s[%d]: instance init success", __func__,__LINE__);
		break;
	case 1:
		ready_timeout = 10;
		while ((hb_ipc_is_remote_ready(instance)) && ready_timeout) {
			msleep(1);
			ready_timeout--;
		}
		if (ready_timeout <= 0)
			pr_err("%s[%d]: is ready failed", __func__,__LINE__);
		else
			pr_info("%s[%d]: is ready success", __func__,__LINE__);
		break;
	case 2:
		if (hb_ipc_acquire_buf(instance, 0, size, &send_buf)) {
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
			memset(send_buf, 0x55, 100);
		}
		if (hb_ipc_send(instance, 0, send_buf, size))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		break;
	case 3:
		if (hb_ipc_acquire_buf(instance, 1, size, &send_buf)) {
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
			memset(send_buf, 0x55, 100);
		}
		if (hb_ipc_send(instance, 1, send_buf, size))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		break;
	case 4:
		if (hb_ipc_acquire_buf(instance, 1, 1, &send_buf)) {
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
		}
		send_buf[0] = 0x24;
		if (hb_ipc_send(instance, 1, send_buf, 1))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		hb_ipc_close_instance(instance);
		break;
	case 5:
		if (hb_ipc_acquire_buf(instance, 1, 1, &send_buf)) {
			pr_info("%s[%d]: failed acquire buf, no memory\n", __func__,__LINE__);
			break;
		} else {
			pr_info("%s[%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
		}
		send_buf[0] = 0x36;
		if (hb_ipc_send(instance, 1, send_buf, 1))
			pr_info("%s[%d]: failed send message\n", __func__,__LINE__);
		else 
			pr_info("%s[%d]: success send message\n", __func__,__LINE__);
		break;
	case 6:
		clear_local(0x1ff00000, 0x10000);
		break;
	case 7:
		clear_local(0xd3001000, 0x6000);
		break;
	default :
		pr_info("%s[%d]: please input valid command\n", __func__,__LINE__);
		break;
	}
	return count;
}

static int32_t init_sysfs(void)
{
	int32_t err = 0;
	struct kobj_attribute test_cmd0 = __ATTR(test_cmd0, 0644, show_test_cmd0, store_test_cmd0);
	struct kobj_attribute test_cmd1 = __ATTR(test_cmd1, 0644, show_test_cmd1, store_test_cmd1);
	struct kobj_attribute test_cmd2 = __ATTR(test_cmd2, 0644, show_test_cmd2, store_test_cmd2);

	g_test_cmd[0] = test_cmd0;
	g_test_cmd[1] = test_cmd1;
	g_test_cmd[2] = test_cmd2;
	/* create ipc-sample folder in sys/kernel */
	g_ipc_kobj = kobject_create_and_add(MODULE_NAME, kernel_kobj);
	if (!g_ipc_kobj) {
		pr_info("ipc-sample folder creation failed %d\n", err);
		return -ENOMEM;
	}

	/* create sysfs file for ipc sample ping command */
	err = sysfs_create_file(g_ipc_kobj, &g_test_cmd[0].attr);
	if (err) {
		pr_info("sysfs file for test cmd 0 failed %d\n", err);
		goto err_kobj_free;
	}

	err = sysfs_create_file(g_ipc_kobj, &g_test_cmd[1].attr);
	if (err) {
		pr_info("sysfs file for test cmd 1 failed %d\n", err);
		goto err_kobj_free;
	}

	err = sysfs_create_file(g_ipc_kobj, &g_test_cmd[2].attr);
	if (err) {
		pr_info("sysfs file for test cmd 2 failed %d\n", err);
		goto err_kobj_free;
	}

	return 0;

err_kobj_free:
	kobject_put(g_ipc_kobj);
	return err;
}

static void free_sysfs(void)
{
	sysfs_remove_file(g_ipc_kobj, &g_test_cmd[0].attr);
	sysfs_remove_file(g_ipc_kobj, &g_test_cmd[1].attr);
	sysfs_remove_file(g_ipc_kobj, &g_test_cmd[2].attr);
	kobject_put(g_ipc_kobj);
}

static int32_t __init sample_mod_init(void)
{
	int32_t err = 0;

	pr_info("module version "MODULE_VER" init\n");

	err = init_sysfs();
	if (err)
		return err;

	return 0;
}


static void __exit sample_mod_exit(void)
{
	free_sysfs();
	pr_info("module version "MODULE_VER" exit\n");
}

module_init(sample_mod_init);
module_exit(sample_mod_exit);
