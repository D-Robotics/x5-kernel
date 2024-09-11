/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/atomic.h>
#include <linux/capability.h>
#include <linux/clk.h>
#include <linux/crypto.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>

#include "hb_ipc_interface.h"
#include "ipc_wrapper.h"
#include "gua_audio_ipc.h"
#include "gua_misc_drv.h"
#include "gua_audio_rpc_protocol.h"

/* the data segment used by IPC for interaction between acore and DSP, such as logs and shared data. */
#define IPC_DATA_SIZE (0x800000)

typedef struct _control {
	smf_packet_head_t header;
	u8 payload[0];
} control_msg_s_t;

// index: core, value: channel
static u32 g_core_to_channel_map[DIRECTION_MAX];
static audio_debug_info_t g_debug_info;
// buffer to check when callback, FIXME buf in audio_rpc_wrapper
#define CTRL_BUF_MAX_COUNT	4
static s8 g_cb_buf[CTRL_BUF_MAX_COUNT][GUA_MISC_MSG_MAX_SIZE] = {0};
static atomic_t g_buf_idx;

static LIST_HEAD(g_misc_info_list);
static DECLARE_WAIT_QUEUE_HEAD(g_q_wait);
static atomic_t g_cb_is_valid;

typedef struct gua_audio_misc_info {
	char name[20];
	struct device *dev;
	struct miscdevice misc_dev;
	/* for status info */
	struct attribute_group attr_group;
	void *driver_data;
	struct list_head list;
	ipc_wrapper_data_t *ipc_wrapper;
} gua_audio_misc_info_t;

inline static s32 init_core_to_channel_map(u8 core, u32 channel)
{
	if (core >= DIRECTION_MAX) {
		return EINVAL;
	}
	g_core_to_channel_map[core] = channel;
	return 0;
}

inline static s32 get_channel_from_core(u8 core)
{
	if (core >= DIRECTION_MAX) {
		return -1;
	}
	return g_core_to_channel_map[core];
}

static s32 gua_audio_misc_open(struct inode *inode, struct file *file)
{
	gua_audio_misc_info_t *misc_info = NULL;
	s32 minor = iminor(inode);

	list_for_each_entry(misc_info, &g_misc_info_list, list) {
		if (misc_info->misc_dev.minor == minor) {
			file->private_data = misc_info;
			atomic_set(&g_cb_is_valid, false);
			atomic_set(&g_buf_idx, 0);
			return 0;
		}
	}

	return -ENODEV;
}

static s32 gua_audio_misc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static s32 ipc_wrapper_recv_cb(uint8_t *payload, uint32_t payload_size, void *priv) {
	gua_audio_rpc_data_t *msg = NULL;
	dev_dbg(NULL, "%s\n",	__func__);
	if (payload == NULL) {
		dev_err(NULL, "wrapper recv cb msg NULL\n");
		return -EINVAL;
	}
	msg = (gua_audio_rpc_data_t*)&(((struct ipc_wrapper_rpmsg_r *)payload)->body);
	if (msg == NULL) {
		dev_err(NULL, "wrapper recv cb msg body NULL\n");
		return -EINVAL;
	}
	if (msg->header.group != GROUP_CONTROL && msg->header.group != GROUP_POWER) {
		dev_dbg(NULL, "%s group:%d\n", __func__, msg->header.group); // FIXME: horizon yang@<wenxiang.yang@guasemi.com>
		return -EINVAL;
	}

	atomic_inc(&g_buf_idx);
	atomic_cmpxchg(&g_buf_idx, CTRL_BUF_MAX_COUNT, 0);
	memcpy(g_cb_buf[atomic_read(&g_buf_idx)], msg, msg->header.payload_size + sizeof(msg->header));
	atomic_set(&g_cb_is_valid, true);

	wake_up_interruptible(&g_q_wait);
	return 0;
}

static ipc_wrapper_data_t* get_ipc_wrapper(struct file *file)
{
	gua_audio_misc_info_t *misc_info = (gua_audio_misc_info_t *)(file->private_data);
	ipc_wrapper_data_t *wrapper_data = NULL;

	if (IS_ERR_OR_NULL(misc_info)) {
		dev_err(NULL, "misc_info is NULL\n");
		return NULL;
	}
	wrapper_data = misc_info->ipc_wrapper;
	if (IS_ERR_OR_NULL(wrapper_data)) {
		dev_err(misc_info->dev, "wrapper data is NULL\n");
		return NULL;
	}
	return wrapper_data;
}

static u32 gua_audio_misc_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &g_q_wait, wait);
	return atomic_read(&g_cb_is_valid) ? POLLIN | POLLRDNORM : 0;
}

static ssize_t gua_audio_misc_write(struct file *file, const char __user *buf,
					size_t count, loff_t *offset)
{
	s8 local_buf[GUA_MISC_MSG_MAX_SIZE] = {0}; // 1. no malloc and free. 2. XXX expand buf size?
	control_msg_s_t *msg = (control_msg_s_t*)local_buf;
	s32 channel = 0;
	ssize_t ret = 0;
	ipc_wrapper_data_t *wrapper_data = get_ipc_wrapper(file);

	if (IS_ERR_OR_NULL(wrapper_data)) {
		return 0;
	}

	channel = get_channel_from_core(0);
	// XXX check channel?

	// FIXME adapt for x5's rpc protocol, optimize it in GUA platform
	msg->header.category = SMF_AUDIO_DSP_OUTPUT_SERVICE_ID;
	msg->header.type = SMF_NOTIFICATION;
	msg->header.payload_len = count;
	if (copy_from_user(msg->payload, buf, count) != 0) {
		dev_err(NULL, "wrapper misc write copy failed\n");
		return 0;
	}

	ret = wrapper_data->send_msg(&wrapper_data->poster[channel], (void*)msg, count + sizeof(msg->header));
	return ret;
}

static ssize_t gua_audio_misc_read(struct file *file, char __user *buf,
					size_t count, loff_t *offset)
{
	s8 local_buf[GUA_MISC_MSG_MAX_SIZE] = {0}; // 1. no malloc and free. 2. XXX expand buf size?
	ipc_wrapper_data_t *wrapper_data = get_ipc_wrapper(file);
	gua_audio_rpc_data_t *rpc_data = NULL;
	gua_audio_rpc_data_t *msg = NULL;
	size_t actual_count = 0U;

	if (IS_ERR_OR_NULL(wrapper_data)) {
		return 0;
	}

	if (copy_from_user(local_buf, buf, count) != 0) {
		dev_err(NULL, "wrapper rpc misc read copy_to_user failed\n");
		return 0;
	}
	rpc_data = (gua_audio_rpc_data_t *)local_buf;

	if (rpc_data->header.uuid == UUID_CURRENT) {
		wait_event_interruptible(g_q_wait, atomic_read(&g_cb_is_valid));
		msg = (gua_audio_rpc_data_t *)&(g_cb_buf[atomic_read(&g_buf_idx)][0]);
		actual_count = sizeof(msg->header) + msg->header.payload_size;
		if (copy_to_user(buf, (u8*)msg, actual_count) != 0) {
			dev_err(NULL, "wrapper rpc misc read copy_to_user failed\n");
		}
		atomic_set(&g_cb_is_valid, false);
	} else {
		// TODO other logic
	}

	return actual_count; // FIXME for X5, optimize it that all buf in audio_rpc_wrapper

	// return wrapper_data->read(&wrapper_data->poster[channel], buf, count);
}

static ssize_t gua_audio_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;

	switch (cmd) {
	case GUA_MISC_IOCTL_CMD_READADDR:
		if (copy_to_user((void __user *)arg, &g_debug_info.phy_mem_addr, sizeof(g_debug_info.phy_mem_addr))) {
			dev_err(NULL, "copy_to_user failed\n");
			return -EFAULT;
		}
		break;
	case GUA_MISC_IOCTL_CMD_READSIZE:
		if (copy_to_user((void __user *)arg, &g_debug_info.phy_mem_size, sizeof(g_debug_info.phy_mem_size))) {
			dev_err(NULL, "copy_to_user failed\n");
			return -EFAULT;
		}
		break;
	case GUA_MISC_IOCTL_CMD_LOGDUMP:
		// TODO: send msg to dsp
		break;
	case GUA_MISC_IOCTL_CMD_DATADUMP:
		// TODO: send msg to dsp
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int gua_audio_misc_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long kpaddr = 0;

	kpaddr = (unsigned long)g_debug_info.phy_mem_addr;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, kpaddr >> PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		dev_err(NULL, "remap_pfn_range failed\n");
		return -EAGAIN;
	}
	return 0;
}

static const struct file_operations gua_audio_misc_f_ops = {
	.owner = THIS_MODULE,
	.open = gua_audio_misc_open,
	.poll = gua_audio_misc_poll,
	.read = gua_audio_misc_read,
	.write = gua_audio_misc_write,
	.release = gua_audio_misc_release,
	.mmap = gua_audio_misc_mmap,
	.unlocked_ioctl = gua_audio_misc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gua_audio_misc_ioctl,
#endif
};

static struct miscdevice gua_audio_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gua-dsp-misc-dev",
	.fops = &gua_audio_misc_f_ops,
};

static ssize_t show_gua_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	gua_audio_misc_info_t *gua_info = dev_get_drvdata(dev);
	size_t count = 0;

	count += sprintf(buf, "[%s] dump audio status:\n", gua_info->name);

	return count;
}

static ssize_t store_gua_status(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	gua_audio_misc_info_t *gua_info = dev_get_drvdata(dev);
	size_t ret = 0;
	u32 rw_flag = 0;
	u32 input_status_group = 0;
	u32 input_status_cmd = 0;

	ret = sscanf(buf, "%u,%u,%u", &rw_flag, &input_status_group, &input_status_cmd);
	dev_info(dev, "[%s] ret:%lu, rw_flag:%u, status_group:%u, status_cmd:%u\n",
			gua_info->name, ret, rw_flag, input_status_group, input_status_cmd);
	ret = count;

	return ret;
}

static DEVICE_ATTR(status_debug, 0644, show_gua_status, store_gua_status);

static struct attribute *gua_dev_debug_attrs[] = {
	&dev_attr_status_debug.attr,
	NULL,
};

static struct attribute_group gua_dev_debug_attr_group = {
	.name = "audio_misc_debug",
	.attrs = gua_dev_debug_attrs,
};


static s32 gua_audio_misc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	gua_audio_misc_info_t *gua_misc_priv = NULL;
	struct device_node *np_root = pdev->dev.of_node;
	const char *full_name = of_node_full_name(np_root);
	s32 ret = 0;
	struct device_node *wrapper_np = NULL;
	struct platform_device *wrapper_pdev = NULL;
	ipc_wrapper_data_t *wrapper_data = NULL;
	u32 core = 0U;
	u32 channel = 0U;
	u32 core_channel_pair_size = 0;
	u32 i = 0;
	struct device_node *ipc_np;
	struct platform_device *ipc_pdev;
	struct ipc_dev_instance *ipc_dev;
	struct ipc_instance_cfg *ipc_cfg;
	struct ipc_instance_info_m1 *info;

	gua_misc_priv = devm_kzalloc(dev, sizeof(*gua_misc_priv), GFP_KERNEL);
	if (IS_ERR_OR_NULL(gua_misc_priv)) {
		dev_err(dev, "kzalloc for gua_audio_misc_info failed.\n");
		return -ENOMEM;
	}

	// get info from np
	memcpy(gua_misc_priv->name, full_name, strlen(full_name));

	// get dev
	gua_misc_priv->dev = dev;
	dev_set_drvdata(&pdev->dev, gua_misc_priv);
	memcpy(&gua_misc_priv->misc_dev, &gua_audio_misc_dev, sizeof(gua_audio_misc_dev));

	// create sysfs
	ret = sysfs_create_group(&pdev->dev.kobj, &gua_dev_debug_attr_group);
	if (ret != 0) {
		dev_warn(&pdev->dev, "failed to create attr group\n");
		goto err_sysfs_create;
	}

	// register misc dev
	ret = misc_register(&gua_misc_priv->misc_dev);
	if (ret != 0) {
		dev_err(&pdev->dev, "dsp register dev error(%d).\n", ret);
		goto err_misc_register_device;
	}

	list_add_tail(&gua_misc_priv->list, &g_misc_info_list);

	wrapper_np = of_parse_phandle(pdev->dev.of_node, "msg-wrapper", 0);
	if (IS_ERR_OR_NULL(wrapper_np)) {
		dev_err(&pdev->dev, "audio wrapper phandle missing or invalid\n");
		return -EINVAL;
	}
	wrapper_pdev = of_find_device_by_node(wrapper_np);
	if (IS_ERR_OR_NULL(wrapper_pdev)) {
		dev_err(&pdev->dev, "failed to find audio wrapper device\n");
		ret = -EINVAL;
		goto err_misc_dts;
	}
	wrapper_data = platform_get_drvdata(wrapper_pdev);

	gua_misc_priv->ipc_wrapper = wrapper_data;

	core_channel_pair_size
		= of_property_count_u32_elems(pdev->dev.of_node, "control-rpmsg-channel-mapping") >> 1;
	for (i = 0; i < core_channel_pair_size; ++i) {
		of_property_read_u32_index(
			pdev->dev.of_node, "control-rpmsg-channel-mapping", i << 1, &core);
		of_property_read_u32_index(
			pdev->dev.of_node, "control-rpmsg-channel-mapping", (i << 1) + 1, &channel);
		if (init_core_to_channel_map(core, channel) != 0) {
			dev_err(&pdev->dev, "wrapper map core(%d) to channel(%d) failed\n", core, channel);
		}
		gua_misc_priv->ipc_wrapper->poster[channel].priv_data[0] = gua_misc_priv;
		gua_misc_priv->ipc_wrapper->poster[channel].recv_msg_cb[0] = ipc_wrapper_recv_cb;
	}

	// get reserved memory region for log/data dump, currently only have one region
	ipc_np = of_parse_phandle(pdev->dev.of_node, "gua-ipc", 0);
	if (!ipc_np) {
		dev_err(&pdev->dev, "get gua-ipc error\n");
		return -1;
	}
	ipc_pdev = of_find_device_by_node(ipc_np);
	if (!ipc_pdev) {
		dev_err(&pdev->dev, "find gua-ipc error\n");
		return -1;
	}

	ipc_dev = (struct ipc_dev_instance *)platform_get_drvdata(ipc_pdev);
	ipc_cfg = &ipc_dev->ipc_info;
	info = &ipc_dev->ipc_info.info.custom_cfg;

	g_debug_info.phy_mem_addr = (uint32_t)info->ipc_phy_base;
	g_debug_info.phy_mem_size = IPC_DATA_SIZE;

	dev_dbg(&pdev->dev,
		 "AUDIO: allocate reserved memory for log/data dump, phy_addr_base : 0x%x, mem_size : 0x%x.\n",
		 g_debug_info.phy_mem_addr, g_debug_info.phy_mem_size);

	platform_set_drvdata(pdev, gua_misc_priv);

	atomic_set(&g_cb_is_valid, false);

	return 0;

err_misc_register_device:
	sysfs_remove_group(&pdev->dev.kobj, &gua_dev_debug_attr_group);

err_misc_dts:
err_sysfs_create:
	devm_kfree(&pdev->dev, gua_misc_priv);
	return ret;
}

static s32 gua_audio_misc_remove(struct platform_device *pdev)
{
	gua_audio_misc_info_t *gua_misc_priv = dev_get_drvdata(&pdev->dev);

	if (IS_ERR_OR_NULL(gua_misc_priv)) {
		dev_err(&pdev->dev, "gua_misc_priv is null.\n");
		return -EFAULT;
	}

	misc_deregister(&gua_misc_priv->misc_dev);
	sysfs_remove_group(&pdev->dev.kobj, &gua_dev_debug_attr_group);

	list_del(&gua_misc_priv->list);
	dev_info(&pdev->dev, "%s: %s remove finished!\n", __func__, gua_misc_priv->name);

	devm_kfree(&pdev->dev, gua_misc_priv);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static const struct of_device_id gua_audio_misc_ids[] = {
	{ .compatible = "gua,misc-hifi-core" },
	{}
};

static struct platform_driver gua_audio_misc_driver = {
	.probe	= gua_audio_misc_probe,
	.remove	= gua_audio_misc_remove,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "gua-dsp-misc-drv",
		.of_match_table	= gua_audio_misc_ids,
	},
};
module_platform_driver(gua_audio_misc_driver);

MODULE_DESCRIPTION("gua misc driver");
MODULE_LICENSE("GPL");
