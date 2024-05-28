/*
 * Copyright (C) 2023 Shanghai GUA Technology Co., Ltd.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/control.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/iommu.h>
#include <linux/printk.h>
#include <linux/io.h>
#include "hb_ipc_interface.h"
#include "ipc_wrapper.h"
#include "gua_pcm.h"

#define MODULE_NAME "gua-ipc-wrapper-debug"

#define IPC_INST0_ACORE_ADDR	0xd3000000
#define IPC_INST0_DSP_ADDR		0xd3000400
#define IPC_INST0_BUF_SIZE		0x400
#define IPC_INST1_ACORE_ADDR	0xd3000800
#define IPC_INST1_DSP_ADDR		0xd3000c00
#define IPC_INST1_BUF_SIZE		0x400
#define IPC_INST2_ACORE_ADDR	0xd3001000
#define IPC_INST2_DSP_ADDR		0xd3004000
#define IPC_INST2_BUF_SIZE		0x3000

static ipc_wrapper_data_t *s_data;

static struct kobject *g_iwrap_kobj;

#define SYNC_TRANS (1)
#define SPIN_WAIT (0x4)

static int32_t clear_memory(phys_addr_t addr, uint32_t size)
{
	void __iomem *tmp = ioremap(addr, size);
	if (!tmp) {
		pr_err("AUDIO[%s][%d]: addr:0x%llx len:0x%x ioremap error!\n", __func__,__LINE__, (uint64_t)addr, size);
	}
	memset_io(tmp, 0x00, size);
	iounmap(tmp);
	pr_info("AUDIO[%s][%d]: zero set addr:0x%llx len:0x%x\n", __func__, __LINE__, (uint64_t)addr, size);
	return 0;
}

static int audio_msg_send(audio_poster_t *au_poster, uint8_t *payload, uint32_t payload_size)
{
	uint32_t inst_id;
	struct ipc_wrapper_rpmsg_s *send_msg;
	struct device *dev;
	int ret = 0;
	uint8_t *send_buf;

	// if (!s_data->linked_up) {
	// 	dev_err(NULL, "AUDIO[%s][%d] : HB-IPC instance(%d) disconnected!\n", __func__, __LINE__, AUDIO_HBIPC_INSTANCE_ID_0);
	// 	return -EFAULT;
	// }
	if (hb_ipc_is_remote_ready(AUDIO_HBIPC_INSTANCE_ID_0)) {
		dev_err(NULL, "AUDIO[%s][%d] : HB-IPC instance(%d) disconnected!\n", __func__, __LINE__, AUDIO_HBIPC_INSTANCE_ID_0);
		return -1;
	}

	if (NULL == au_poster || NULL == payload || payload_size <= 0) {
		return -EFAULT;
	} else if (!au_poster->registed) {
		return -EFAULT;
	}

	send_msg = (struct ipc_wrapper_rpmsg_s *)payload;
	inst_id = au_poster->ipc_inst_id;
	dev = &au_poster->pdev->dev;

	// struct pcm_rpmsg_s *tmp = (struct pcm_rpmsg_s*)payload;
	// dev_dbg(dev, "AUDIO : pcm_device:%d,rate:%d,channels:%d,format:%d,cmd:0x%x.\n",
	//  	tmp->param.pcm_device, tmp->param.sample_rate, tmp->param.channels, tmp->param.format, tmp->param.cmd);

	mutex_lock(&au_poster->send_lock);
	if (SMF_NOTIFICATION == send_msg->header.type) {
		if (hb_ipc_acquire_buf(inst_id, AUDIO_HBIPC_CHANNEL_ID_0, payload_size, &send_buf)) {
			dev_err(dev, "AUDIO : failed acquire buf, no memory!\n");
		}
		memcpy(send_buf, payload, payload_size);
		if (hb_ipc_send(inst_id, AUDIO_HBIPC_CHANNEL_ID_0, send_buf, payload_size)) {
			dev_err(dev, "AUDIO : failed send message!\n");
		}
	} else if (SMF_REQUEST == send_msg->header.type) {
		wait_queue_entry_t wait_queue;
		long timeout;
		init_waitqueue_entry(&wait_queue, current);
		add_wait_queue(&au_poster->waker, &wait_queue);
		au_poster->sync_response = false;
		if (hb_ipc_acquire_buf(inst_id, AUDIO_HBIPC_CHANNEL_ID_0, payload_size, &send_buf)) {
			dev_err(dev, "AUDIO : failed acquire buf, no memory!\n");
		}
		memcpy(send_buf, payload, payload_size);
		ret = hb_ipc_send(inst_id, AUDIO_HBIPC_CHANNEL_ID_0, send_buf, payload_size);
		if (ret) {
			dev_err(dev, "AUDIO : HB-IPC fail(%d) send message!\n", ret);
			ret = -EIO;
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&au_poster->waker, &wait_queue);
			mutex_unlock(&au_poster->send_lock);
			return ret;
		}
		set_current_state(TASK_KILLABLE);
		while (1) {
			if (0 != (ret = fatal_signal_pending(current))) {
				dev_err(dev, "AUDIO : fatal_signal_pending return, signal: %d\n", ret);
				// By wenxiang.yang@gua.com 01/03/2024
				// FIXME: should break due to TASK state and queue need updated.
				//        whereas crash would appear.
				// mutex_unlock(&au_poster->send_lock);
				break;
				// return 0;
			}

			if (au_poster->sync_response) {
				au_poster->sync_response = false;
				break;
			}

			timeout = schedule_timeout_killable(msecs_to_jiffies(AUDIO_HBIPC_RESPOND_TIMEOUT));
			if (timeout == 0) {
				dev_err(dev, "AUDIO : response timeout\n");
				ret = -EIO;
				break;
			} else {
				if (!au_poster->sync_response) {
					dev_warn(dev, "AUDIO : mistake to wake up!\n");
					continue;
				} else {
					au_poster->sync_response = false;
					break;
				}
			}
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&au_poster->waker, &wait_queue);
	} else {
		dev_err(dev, "AUDIO : msg type error:%d\n", send_msg->header.type);
	}
	mutex_unlock(&au_poster->send_lock);

	return ret;
}

void hbipc_data_chan_rx_cb(uint8_t *arg, int32_t instance, int32_t chan_id, uint8_t *buf,
	uint64_t size)
{
	struct pcm_rpmsg_r *msg;
	int pcm_device;

	// if (!s_data->linked_up) {
	// 	dev_err(NULL, "AUDIO[%s][%d] : HB-IPC instance(%d) disconnected!\n", __func__, __LINE__, AUDIO_HBIPC_INSTANCE_ID_0);
	// 	return;
	// }
	if (hb_ipc_is_remote_ready(AUDIO_HBIPC_INSTANCE_ID_0)) {
		dev_warn_ratelimited(NULL, "AUDIO[%s][%d] : HB-IPC instance(%d) disconnected!\n", __func__, __LINE__, AUDIO_HBIPC_INSTANCE_ID_0);
		return;
	}

	if ((AUDIO_HBIPC_INSTANCE_ID_0 != instance)
		|| (AUDIO_HBIPC_CHANNEL_ID_0 != chan_id)) {
		dev_err(NULL, "AUDIO[%s][%d] : HB-IPC instance(%d) ch(%d) unsupportted!\n", __func__, __LINE__, instance, chan_id);
		return;
	}

	dev_dbg(NULL, "AUDIO[%s][%d]: recv instance(%d) ch(%d) rx callback.\n", __func__,__LINE__, instance, chan_id);

	if (NULL == buf) {
		dev_err(NULL, "AUDIO[%s][%d] : HB-IPC callback buf nullptr!\n", __func__, __LINE__);
		return;
	}

	if (size <= 0) {
		dev_err(NULL, "AUDIO[%s][%d]: size invalid parameter: %#llx\n", __func__,__LINE__, size);
		return;
	}

	msg = (struct pcm_rpmsg_r *)buf;
	pcm_device = msg->param.pcm_device;

	if (msg->header.type == SMF_RESPONSE) {
		s_data->poster[chan_id].sync_response = true; //device bind
		wake_up(&s_data->poster[chan_id].waker);
		dev_dbg(NULL, "AUDIO : pcm[%d] cmd[%d] response\n", pcm_device, msg->param.cmd);
	} else {
		for (int i = 0; i < AUDIO_MAX_HBIPC_RECV; ++i) {
			if (s_data->poster[chan_id].recv_msg_cb[i] != NULL) {
				s_data->poster[chan_id].recv_msg_cb[i](buf, size, s_data->poster[chan_id].priv_data[i]);
			}
		}
	}

	if(hb_ipc_release_buf(instance, chan_id, buf))
		dev_err(NULL, "AUDIO[%s][%d]: hb_ipc_release_buf failed\n", __func__, __LINE__);
}

static struct ipc_instance_cfg test_cfg_instances = {
		.mode = 0, /**< work mode, 0 default mode, 1 custom mode*/
		.timeout = 100,
		.trans_flags = SYNC_TRANS | SPIN_WAIT,
		.mbox_chan_idx = 0,
		.info = {
			.def_cfg = {
				.recv_callback = hbipc_data_chan_rx_cb,
				.userdata  = NULL,
			},
		},
};

static void hbipc_audio_work(struct work_struct *work)
{
	ipc_wrapper_data_t *data;
	struct device *dev;
	int32_t i;

	data = container_of(work, struct ipc_wrapper_data, work);
	dev = &data->pdev->dev;

	dev_dbg(dev, "AUDIO : hbipc_audio_work enter.\n");

	// int32_t ready_timeout = 10000, count = 0;
	// while ((hb_ipc_is_remote_ready(0)) && ready_timeout) {
	// 	msleep(1);
	// 	ready_timeout--;
	// 	count++;
	// }

	// if(count >= 10000) {
	// 	dev_err(dev, "AUDIO : hbipc_audio_work linked(time used %d ms) time out!\n", count);
	// 	return;
	// }
	// dev_dbg(dev, "AUDIO : hbipc_audio_work linked(time used %d ms) to remote success.\n", count);

	for (i = 0; i < 1; i++) {
		// data->poster[i].rpdev = rpdev;
		data->poster[i].pdev = data->pdev;
		data->poster[i].ipc_inst_id = AUDIO_HBIPC_INSTANCE_ID_0;
		data->poster[i].poster_id = i;
		data->poster[i].registed = true;
		data->poster[i].sync_response = false;
		init_waitqueue_head(&data->poster[i].waker);
		mutex_init(&data->poster[i].send_lock);
	}

	// data->linked_up = true;
}

static ssize_t store_iwrap_status(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t count)
{
	uint32_t cmd = 0;
	int32_t ret;
	uint8_t *send_buf;
	uint32_t size = 100;
	int32_t ready_timeout = 0;

	if (buf == NULL || count <= 0) {
		pr_err("AUDIO[%s][%d]: %s sysfs buf is NULL or count is zero.\n", __func__, __LINE__, MODULE_NAME);
		return -1;
	}

	ret = kstrtouint(buf, 10, &cmd);
	if (ret) {
		pr_err("AUDIO[%s][%d]: Invalid Injection (ret %d)\n", __func__, __LINE__, ret);
		return -1;
	}

	pr_debug("AUDIO[%s][%d]: input msg is: %d\n", __func__, __LINE__, cmd);

	switch (cmd) {
	case 0:
		if (hb_ipc_open_instance(AUDIO_HBIPC_INSTANCE_ID_0, &test_cfg_instances))
			pr_err("AUDIO[%s][%d]: HB-IPC instance 0 init failed", __func__,__LINE__);
		else
			pr_debug("AUDIO[%s][%d]: HB-IPC instance 0 init success", __func__,__LINE__);
		break;
	case 1:
		ready_timeout = 10;
		while ((hb_ipc_is_remote_ready(AUDIO_HBIPC_INSTANCE_ID_0)) && ready_timeout) {
			msleep(1);
			ready_timeout--;
		}
		if (ready_timeout <= 0) {
			pr_err("AUDIO[%s][%d]: HB-IPC instance 0 is ready failed", __func__,__LINE__);
		} else {
			pr_debug("AUDIO[%s][%d]: HB-IPC instance 0 is ready success", __func__,__LINE__);
		}
		break;
	case 2:
		if (hb_ipc_acquire_buf(AUDIO_HBIPC_INSTANCE_ID_0, AUDIO_HBIPC_CHANNEL_ID_0, size, &send_buf)) {
			pr_err("AUDIO[%s][%d]: failed acquire buf, no memory\n", __func__,__LINE__);
		} else {
			pr_debug("AUDIO[%s][%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
			memset(send_buf, 0x55, 100);
		}
		if (hb_ipc_send(AUDIO_HBIPC_INSTANCE_ID_0, AUDIO_HBIPC_CHANNEL_ID_0, send_buf, size)) {
			pr_err("AUDIO[%s][%d]: failed send message\n", __func__,__LINE__);
		} else {
			pr_debug("AUDIO[%s][%d]: success send message\n", __func__,__LINE__);
		}
		break;
	case 3:
		if (hb_ipc_acquire_buf(AUDIO_HBIPC_INSTANCE_ID_0, AUDIO_HBIPC_CHANNEL_ID_1, size, &send_buf)) {
			pr_err("AUDIO[%s][%d]: failed acquire buf, no memory\n", __func__,__LINE__);
		} else {
			pr_debug("AUDIO[%s][%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
			memset(send_buf, 0x55, 100);
		}
		if (hb_ipc_send(AUDIO_HBIPC_INSTANCE_ID_0, AUDIO_HBIPC_CHANNEL_ID_1, send_buf, size)) {
			pr_err("AUDIO[%s][%d]: failed send message\n", __func__,__LINE__);
		} else {
			pr_debug("AUDIO[%s][%d]: success send message\n", __func__,__LINE__);
		}
		break;
	case 4:
		if (hb_ipc_acquire_buf(AUDIO_HBIPC_INSTANCE_ID_0, AUDIO_HBIPC_CHANNEL_ID_1, 1, &send_buf)) {
			pr_info("AUDIO[%s][%d]: failed acquire buf, no memory\n", __func__,__LINE__);
		} else {
			pr_debug("AUDIO[%s][%d]: success acquire buf 0x%px\n", __func__,__LINE__, send_buf);
		}
		send_buf[0] = 0x24;
		if (hb_ipc_send(AUDIO_HBIPC_INSTANCE_ID_0, AUDIO_HBIPC_CHANNEL_ID_1, send_buf, 1)) {
			pr_info("AUDIO[%s][%d]: failed send message\n", __func__,__LINE__);
		} else {
			pr_debug("AUDIO[%s][%d]: success send message\n", __func__,__LINE__);
		}

		hb_ipc_close_instance(AUDIO_HBIPC_INSTANCE_ID_0);
		break;
	case 5:
		hb_ipc_close_instance(AUDIO_HBIPC_INSTANCE_ID_0);
		break;
	case 6:
		// clear ipc inst 0 dsp memory only.
		clear_memory(IPC_INST0_DSP_ADDR, IPC_INST0_BUF_SIZE);
		break;
	case 7:
		// clear ipc inst 0 memory all.
		clear_memory(IPC_INST0_ACORE_ADDR, IPC_INST0_BUF_SIZE * 2);
		break;
	default :
		pr_info("AUDIO[%s][%d]: please input valid command\n", __func__,__LINE__);
		break;
	}
	return count;
}

struct kobj_attribute dev_attr_iwrap_debug = __ATTR(iwrap_debug, 0644, NULL, store_iwrap_status);

static int32_t init_iwrap_sysfs(void)
{
	int32_t err = 0;

	/* create ipc-sample folder in sys/kernel */
	g_iwrap_kobj = kobject_create_and_add(MODULE_NAME, kernel_kobj);
	if (!g_iwrap_kobj) {
		pr_err("AUDIO: %s folder creation failed %d\n", MODULE_NAME, err);
		return -ENOMEM;
	}

	/* create sysfs file for ipc sample ping command */
	err = sysfs_create_file(g_iwrap_kobj, &dev_attr_iwrap_debug.attr);
	if (err) {
		pr_err("AUDIO: sysfs file for %s creation failed %d\n", MODULE_NAME, err);
		goto err_kobj_free;
	}

	return 0;

err_kobj_free:
	kobject_put(g_iwrap_kobj);
	return err;
}

static void free_iwrap_sysfs(void)
{
	sysfs_remove_file(g_iwrap_kobj, &dev_attr_iwrap_debug.attr);
	kobject_put(g_iwrap_kobj);
}

static int ipc_wrapper_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	ipc_wrapper_data_t *data;
	int ret = 0, i;

	dev_info(dev, "AUDIO : audio wrapper probe\n");

	data = devm_kzalloc(&pdev->dev, sizeof(ipc_wrapper_data_t), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		dev_err(dev, "AUDIO : audio wrapper probe failed\n");
		return ret;
	}
	data->pdev = pdev;
	data->linked_up = false;
	data->inst_id = 0;
	data->send_msg = audio_msg_send;
	for (i = 0; i < AUDIO_MAX_HBIPC_POSTER_NUM; i++) {
		data->poster[i].poster_id = -1;
		data->poster[i].poster_type = -1;
		data->poster[i].priv_data[0] = NULL;
		data->poster[i].priv_data[1] = NULL;
		data->poster[i].registed = false;
	}
	s_data = data;
	platform_set_drvdata(pdev, data);

	// create sysfs
	ret = init_iwrap_sysfs();
	if (ret) {
		dev_err(&pdev->dev, "AUDIO : HB-IPC sysfs create failed\n");
		return ret;
	}

	data->hbipc_wq = alloc_ordered_workqueue("hbipc_audio", WQ_HIGHPRI | WQ_UNBOUND | WQ_FREEZABLE);
	if (data->hbipc_wq == NULL) {
		dev_err(&pdev->dev, "AUDIO : HB-IPC workqueue create failed\n");
		return -ENOMEM;
	}

	INIT_WORK(&data->work, hbipc_audio_work);
	queue_work(data->hbipc_wq, &data->work);

	return ret;
}

static int ipc_wrapper_remove(struct platform_device *pdev)
{
	ipc_wrapper_data_t *data = platform_get_drvdata(pdev);
	flush_workqueue(data->hbipc_wq);
	if (data->hbipc_wq)
		destroy_workqueue(data->hbipc_wq);

	free_iwrap_sysfs();

	dev_info(&pdev->dev, "AUDIO : audio wrapper remove exit.\n");

	return 0;
}

static const struct of_device_id ipc_wrapper_dt_ids[] = {
	{ .compatible = "gua,ipc-wrapper", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ipc_wrapper_dt_ids);

static struct platform_driver ipc_wrapper_driver = {
	.driver = {
		.name = "ipc-wrapper",
		.owner = THIS_MODULE,
		.of_match_table = ipc_wrapper_dt_ids,
	},
	.probe = ipc_wrapper_probe,
	.remove = ipc_wrapper_remove,
};
module_platform_driver(ipc_wrapper_driver);

MODULE_AUTHOR("GuaSemi, Inc.");
MODULE_DESCRIPTION("GuaSemi audio wrapper driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gua-hbipc");

