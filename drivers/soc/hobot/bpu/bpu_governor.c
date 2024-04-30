/*
 *  linux/drivers/devfreq/governor_simpleondemand.c
 *
 *  Copyright (C) 2011 Samsung Electronics
 *	MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <linux/slab.h>
#include <governor.h>
#include "bpu_core.h"
#include "bpu_ctrl.h"

#define PRECENT				(100u)
#define DFT_BUSY_THRES		(80u)
#define DIFFTAIL			(5u)
#define	DFT_TIME_THRES		(0u)

struct ondemand_data {
	struct bpu_core *core;
	uint32_t busy_thres;
	uint32_t diff_tail;
	uint32_t time_thres;
	bool plug_off;
};

static int bpu_ondemand_func(struct devfreq *df, unsigned long *freq)
{
	struct ondemand_data *data = df->data;
	struct bpu *bpu;
	struct bpu_core *core;
	uint32_t i, cur_state = 0u;
	uint64_t cur_rate;
	/* the whole bpu (include all cores) ratio*/
	uint32_t b_ratio;
	struct list_head *pos, *pos_n;
	struct bpu_core *tmp_core;
	uint32_t open_core_num = 0u;

	if (data == NULL) {
		return -EINVAL;
	}

	core = data->core;
	if (core == NULL) {
		return -EINVAL;
	}

	bpu = core->host;
	if (bpu == NULL) {
		return -EINVAL;
	}

#if defined(CONFIG_PM_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
	b_ratio = bpu_ratio(bpu);
	cur_rate = core->dvfs->rate;

	for (i = 0u; i < core->dvfs->profile.max_state; i++) {
		if (cur_rate == core->dvfs->profile.freq_table[i]) {
			cur_state = i;
			break;
		}
	}

	list_for_each_safe(pos, pos_n, &bpu->core_list) {
		tmp_core = (struct bpu_core *)list_entry(pos, struct bpu_core, node);
		if (bpu_core_is_online(tmp_core)) {
			open_core_num++;
		}
	}

	if ((core->ratio < (data->busy_thres - data->diff_tail))
			&& (((bpu->slow_task_time / (SECTOUS / SECTOMS)) < data->time_thres)
				|| (data->time_thres == 0))) {
		/* to lower the freq */
		if (cur_state > 0u) {
			cur_state--;
		} else if ((core->hotplug > 0u) && (core->index > 0u) && (open_core_num > 1u)) {
			/* still can lower when lowest, plugoff core if enable hutplug */
			if (b_ratio < (data->busy_thres / 2)) {
				bpu_core_disable(core);
				data->plug_off = true;
				*freq = core->dvfs->profile.freq_table[0u];
				return 0;
			}
		}
	}

	if ((core->ratio > (data->busy_thres + data->diff_tail))
			|| (((bpu->slow_task_time / (SECTOUS / SECTOMS)) > data->time_thres)
				&& (data->time_thres != 0))) {
		/* if plug off before, first plug on */
		if (data->plug_off && (core->hotplug > 0u)) {
			bpu_core_enable(core);
			data->plug_off = false;
			*freq = core->dvfs->profile.freq_table[0u];
			return 0;
		}

		if (cur_state < core->dvfs->profile.max_state - 1) {
			cur_state++;
		}
	}

	*freq = core->dvfs->profile.freq_table[cur_state];
#endif

	return 0;
}

static ssize_t store_busy_thres(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct ondemand_data *data;
	uint32_t wanted;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;

	sscanf(buf, "%u", &wanted);
	if (wanted > PRECENT) {
		wanted = PRECENT;
	}

	if (wanted < DIFFTAIL) {
		wanted = DIFFTAIL;
	}

	data->busy_thres = wanted;
	mutex_unlock(&devfreq->lock);

	return count;
}

static ssize_t show_busy_thres(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct ondemand_data *data;
	ssize_t ret;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;

	ret = sprintf(buf, "%u\n", data->busy_thres);
	mutex_unlock(&devfreq->lock);

	return ret;
}

static ssize_t store_time_thres(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct ondemand_data *data;
	uint32_t wanted;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;

	/* the time is ms*/
	sscanf(buf, "%u", &wanted);
	data->time_thres = wanted;
	mutex_unlock(&devfreq->lock);

	return count;
}

static ssize_t show_time_thres(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct ondemand_data *data;
	ssize_t ret;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;

	ret = sprintf(buf, "%u\n", data->time_thres);
	mutex_unlock(&devfreq->lock);

	return ret;
}

static DEVICE_ATTR(busy_thres, 0644, show_busy_thres, store_busy_thres);
static DEVICE_ATTR(time_thres, 0644, show_time_thres, store_time_thres);

static struct attribute *dev_entries[] = {
	&dev_attr_busy_thres.attr,
	&dev_attr_time_thres.attr,
	NULL,
};

static const struct attribute_group dev_attr_group = {
	.name	= "ondemand",
	.attrs	= dev_entries,
};

static int bpu_ondemand_env_init(struct devfreq *devfreq)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(devfreq->dev.parent);
	struct ondemand_data *data;

	data = kzalloc(sizeof(struct ondemand_data), GFP_KERNEL);
	if (!data) {
		return -ENOMEM;
	}

	data->core = core;
	data->busy_thres = DFT_BUSY_THRES;
	data->diff_tail = DIFFTAIL;
	data->time_thres = DFT_TIME_THRES;
	data->plug_off = false;

	devfreq->data = data;

	return sysfs_create_group(&devfreq->dev.kobj, &dev_attr_group);
}

static void bpu_ondemand_env_exit(struct devfreq *devfreq)
{
	struct bpu_core *core;
	/*
	 * Remove the sysfs entry, unless this is being called after
	 * device_del(), which should have done this already via kobject_del().
	 */
	if (devfreq->dev.kobj.sd)
		sysfs_remove_group(&devfreq->dev.kobj, &dev_attr_group);

	core = ((struct ondemand_data *)devfreq->data)->core;
	kfree(devfreq->data);
	devfreq->data = core;
}

static int bpu_ondemand_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	int ret = 0;

	switch (event) {
	case DEVFREQ_GOV_START:
		ret = bpu_ondemand_env_init(devfreq);
		if (ret == 0) {
			devfreq_monitor_start(devfreq);
		}
		break;

	case DEVFREQ_GOV_STOP:
		devfreq_monitor_stop(devfreq);
		bpu_ondemand_env_exit(devfreq);
		break;

	case DEVFREQ_GOV_UPDATE_INTERVAL:
		devfreq_update_interval(devfreq, (unsigned int *)data);
		break;

	case DEVFREQ_GOV_SUSPEND:
		devfreq_monitor_suspend(devfreq);
		break;

	case DEVFREQ_GOV_RESUME:
		devfreq_monitor_resume(devfreq);
		break;

	default:
		break;
	}

	return ret;
}

static struct devfreq_governor devfreq_bpu_ondemand = {
	.name = "bpu_ondemand",
	.get_target_freq = bpu_ondemand_func,
	.event_handler = bpu_ondemand_handler,
};

int __init devfreq_bpu_ondemand_init(void)
{
	return devfreq_add_governor(&devfreq_bpu_ondemand);
}

void __exit devfreq_bpu_ondemand_exit(void)
{
	int ret;

	ret = devfreq_remove_governor(&devfreq_bpu_ondemand);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);

	return;
}

module_init(devfreq_bpu_ondemand_init);
module_exit(devfreq_bpu_ondemand_exit);
MODULE_DESCRIPTION("Driver for Horizon BPU OnDemand Governor");
MODULE_AUTHOR("Zhang Guoying<guoying.zhang@horizon.ai>");
MODULE_LICENSE("GPL v2");
