/*
 * Copyright (C) 2019 Horizon Robotics
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */
#define pr_fmt(fmt) "bpu: " fmt
#include <linux/device.h>
#include <linux/slab.h>
#include "bpu.h"
#include "bpu_core.h"
#include "bpu_ctrl.h"

static ssize_t bpu_core_ratio_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", bpu_core_ratio(core));
}

static ssize_t bpu_core_queue_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", kfifo_avail(&core->run_fc_fifo[0]));/*PRQA S 4461*/ /* Linux Macro */
}

static ssize_t bpu_core_fc_running_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	DECLARE_KFIFO_PTR(tmp_kfifo[MAX_HW_FIFO_NUM], struct bpu_fc);/*PRQA S 1061*/ /*PRQA S 1062*/ /* Linux Macro */
	struct bpu_fc tmp_fc;
	uint64_t now_point;
	uint32_t prio_num;
	int32_t i;
	int32_t ret;

	ret = sprintf(buf, "BPU Core[%d] limit(%d), prio buffer fcs[0x%x]\n",
			core->index, core->inst.task_buf_limit,
			bpu_prio_left_task_num(core->prio_sched));

	ret += sprintf(buf + ret, "%s\t%s\t\t%s\t%s\t%s\t%s\n",
					"index",
					"id:hwid",
					"group",
					"prio",
					"s_time",
					"p_time(us)");
	now_point = bpu_get_time_point();
	prio_num = BPU_CORE_PRIO_NUM(core);
	for (i = (int32_t)prio_num - 1; i >= 0; i--) {
		(void)memcpy((void *)&tmp_kfifo[i], (void *)&core->run_fc_fifo[i],
				sizeof(tmp_kfifo[i]));
		while (kfifo_len(&tmp_kfifo[i]) > 0) {
			if (kfifo_get(&tmp_kfifo[i], &tmp_fc) < 1) {;
				dev_err(core->dev,
						"Get running bpu fc failed in BPU Core%d\n",
						core->index);
				continue;
			}

			ret += sprintf(buf + ret,
					"%d\t%d:%d\t%d:%d\t%d\t%lld\t%lld\n",
					tmp_fc.index, tmp_fc.info.id, tmp_fc.hw_id,
					bpu_group_id(tmp_fc.info.g_id), bpu_group_user(tmp_fc.info.g_id),
					tmp_fc.info.priority,
					tmp_fc.start_point / NSEC_PER_MSEC,
					((int64_t)now_point - (int64_t)tmp_fc.start_point) / NSEC_PER_USEC);
		}
		ret += sprintf(buf + ret, "\n");
	}

	return ret;
}

static ssize_t bpu_core_fc_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	struct bpu_fc *tmp_fcs;
	int32_t i, len;
	int32_t ret;

	tmp_fcs = (struct bpu_fc *)kzalloc((sizeof(struct bpu_fc)
			* BPU_CORE_RECORE_NUM), GFP_KERNEL);
	if (tmp_fcs == NULL) {
		return sprintf(buf, "Alloc debug buffer failed!\n");
	}

	ret = sprintf(buf, "%s\t%s\t\t%s\t%s\t%s\t%s\t%s\n",
					"index",
					"id:hwid",
					"group",
					"prio",
					"s_time",
					"e_time",
					"r_time");

	len = kfifo_len(&core->done_fc_fifo);
	if (len <= 0) {
		kfree((void *)tmp_fcs);
		return ret;
	}

	len = kfifo_out_peek(&core->done_fc_fifo, tmp_fcs, len);/*PRQA S 4434*/ /*PRQA S 4461*/ /* Kernel Macro */
	for(i = 0; i < len; i++) {
		ret += sprintf(buf + ret,
				"%d\t%d:%d\t%d:%d\t%d\t%lld\t%lld\t%lldus\n",
				tmp_fcs[i].index, tmp_fcs[i].info.id, tmp_fcs[i].hw_id,
				bpu_group_id(tmp_fcs[i].info.g_id), bpu_group_user(tmp_fcs[i].info.g_id),
				tmp_fcs[i].info.priority,
				tmp_fcs[i].start_point / NSEC_PER_MSEC,
				tmp_fcs[i].end_point / NSEC_PER_MSEC,
				(tmp_fcs[i].end_point - tmp_fcs[i].start_point) / NSEC_PER_USEC);
	}

	kfree((void *)tmp_fcs);

	return ret;
}

static ssize_t bpu_core_users_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	struct bpu_user *tmp_user;
	struct list_head *pos, *pos_n;
	int32_t ret = 0;

	if (core == NULL) {
		return sprintf(buf, "core not inited!\n");
	}

	ret += sprintf(buf, "*User via BPU Core(%d)*\n", core->index);
	ret += sprintf(buf + ret, "%s\t\t%s\n", "user", "ratio");
	list_for_each_safe(pos, pos_n, &core->user_list) {
		tmp_user = (struct bpu_user *)list_entry(pos, struct bpu_user, node);/*PRQA S 0497*/ /* Linux Macro */
		if (tmp_user != NULL) {
			ret += sprintf(buf + ret, "%d\t\t%d\n",
				(uint32_t)(tmp_user->id & USER_MASK), bpu_user_ratio(tmp_user));
		}
	}

	return ret;
}

static ssize_t bpu_core_hotplug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", core->hotplug);
}

static ssize_t bpu_core_hotplug_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	int32_t hotplug_en;
	int32_t ret;

	ret = sscanf(buf, "%du", &hotplug_en);
	if (ret < 0) {
		return 0;
	}

	if (hotplug_en <= 0) {
		core->hotplug = 0;
	} else {
		core->hotplug = 1;
	}

	return (ssize_t)len;
}

static ssize_t bpu_core_power_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", core->hw_enabled);
}

static ssize_t bpu_core_power_en_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	int32_t power_en;
	int32_t ret;

	ret = sscanf(buf, "%du", &power_en);
	if (ret < 0) {
		return 0;
	}

	ret = mutex_lock_interruptible(&core->mutex_lock);
	if (ret < 0) {
		return 0;
	}
	if (power_en <= 0) {
		ret = bpu_core_disable(core);
	} else {
		ret = bpu_core_enable(core);
	}
	mutex_unlock(&core->mutex_lock);
	if (ret < 0) {
		return 0;
	}

	return (ssize_t)len;
}

static ssize_t bpu_core_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", core->power_level);
}

static ssize_t bpu_core_power_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	int32_t power_level;
	int32_t ret;

	ret = sscanf(buf, "%du", &power_level);
	if (ret < 0) {
		return 0;
	}

	ret = mutex_lock_interruptible(&core->mutex_lock);
	if (ret < 0) {
		return 0;
	}
	ret = bpu_core_set_freq_level(core, power_level);
	mutex_unlock(&core->mutex_lock);
	if (ret < 0) {
		return 0;
	}

	return (ssize_t)len;
}

static ssize_t bpu_core_limit_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", core->inst.task_buf_limit);
}

static ssize_t bpu_core_limit_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	int32_t fc_buf_limit;
	int32_t ret;

	ret = sscanf(buf, "%du", &fc_buf_limit);
	if (ret < 0) {
		return 0;
	}

	ret = bpu_core_set_limit(core, fc_buf_limit);
	if (ret != 0) {
		pr_err("Bpu core Set prio limit failed\n");
	}

	return (ssize_t)len;
}

static ssize_t bpu_core_hw_prio_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", core->inst.hw_prio_en);
}

static ssize_t bpu_core_hw_prio_en_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t len)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	int32_t ret;
	uint32_t tmp_val;

	ret = sscanf(buf, "%du", &tmp_val);
	if (ret < 0) {
		dev_err(core->dev, "BPU core%d sscanf hw_prio_en error\n",
				core->index);
		return 0;
	}

	if ((tmp_val) > 0u) {
		core->inst.hw_prio_en = 1;
	} else {
		core->inst.hw_prio_en = 0;
	}

	return (ssize_t)len;
}

#define HEXBITS (16u)
static ssize_t bpu_core_burst_len_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			(core->inst.burst_len * HEXBITS));
}

static ssize_t bpu_core_burst_len_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	int32_t ret;
	uint32_t tmp_val;

	ret = sscanf(buf, "%du", &tmp_val);
	if (ret < 0) {
		dev_err(core->dev, "BPU core%d sscanf burst error\n",
				core->index);
		return 0;
	}

	if ((tmp_val % HEXBITS) > 0u) {
		dev_err(core->dev, "burst len must align 16");
	}

	core->inst.burst_len = (uint64_t)tmp_val / HEXBITS;

	dev_info(core->dev, "BPU core%d set burst len:%u\n",
			core->inst.index, core->inst.burst_len);

	return (ssize_t)len;
}

static DEVICE_ATTR(ratio, S_IRUGO, bpu_core_ratio_show, NULL);/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
static DEVICE_ATTR(queue, S_IRUGO, bpu_core_queue_show, NULL);/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
static DEVICE_ATTR(fc_time, S_IRUGO, bpu_core_fc_time_show, NULL);/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
static DEVICE_ATTR(fc_running, S_IRUGO, bpu_core_fc_running_show, NULL);/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
static DEVICE_ATTR(users, S_IRUGO, bpu_core_users_show, NULL);/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */

/*
 * core power level >0: kernel dvfs;
 * 0:highest; -1,-2...: different work power level */
static DEVICE_ATTR(power_level, S_IRUGO | S_IWUSR,/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
		bpu_core_power_show, bpu_core_power_store);

/* power on/off, > 0 power on; <= 0 power off*/
static DEVICE_ATTR(power_enable, S_IRUGO | S_IWUSR,/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
		bpu_core_power_en_show, bpu_core_power_en_store);

/* power on/off, > 0 power on; <= 0 power off*/
static DEVICE_ATTR(hotplug, S_IRUGO | S_IWUSR,/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
		bpu_core_hotplug_show, bpu_core_hotplug_store);

static DEVICE_ATTR(limit, S_IRUGO | S_IWUSR,/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
		bpu_core_limit_show, bpu_core_limit_store);

static DEVICE_ATTR(hw_prio_en, S_IRUGO | S_IWUSR,/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
		bpu_core_hw_prio_en_show, bpu_core_hw_prio_en_store);

static DEVICE_ATTR(burst_len, S_IRUGO | S_IWUSR,/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
		bpu_core_burst_len_show, bpu_core_burst_len_store);

static struct attribute *bpu_core_attrs[] = {
	&dev_attr_ratio.attr,
	&dev_attr_queue.attr,
	&dev_attr_fc_time.attr,
	&dev_attr_fc_running.attr,
	&dev_attr_power_level.attr,
	&dev_attr_power_enable.attr,
	&dev_attr_hotplug.attr,
	&dev_attr_users.attr,
	&dev_attr_limit.attr,
	&dev_attr_hw_prio_en.attr,
	&dev_attr_burst_len.attr,
	NULL,
};

static struct attribute_group bpu_core_attr_group = {
	.attrs = bpu_core_attrs,
};

static ssize_t bpu_ratio_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	if (g_bpu == NULL) {
		return sprintf(buf, "bpu not inited!\n");
	}

	return sprintf(buf, "%d\n", bpu_ratio(g_bpu));
}

static ssize_t bpu_core_num_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct list_head *pos, *pos_n;
	int32_t core_num = 0;

	if (g_bpu == NULL) {
		return sprintf(buf, "bpu not inited!\n");
	}

	list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
		core_num++;
	}

	return sprintf(buf, "%d\n", core_num);
}

static ssize_t bpu_group_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct bpu_fc_group *tmp_group;
	struct list_head *pos, *pos_n;
	int32_t ret = 0;

	if (g_bpu == NULL) {
		return sprintf(buf, "bpu not inited!\n");
	}
	ret += sprintf(buf, "%s\t\t%s\t%s\n", "group", "prop", "ratio");
	list_for_each_safe(pos, pos_n, &g_bpu->group_list) {
		tmp_group = (struct bpu_fc_group *)list_entry(pos, struct bpu_fc_group, node);/*PRQA S 0497*/ /* Linux Macro */
		if (tmp_group != NULL) {
			ret += sprintf(buf + ret, "%d(%d)\t\t%d\t%d\n",
				bpu_group_id(tmp_group->id),
				bpu_group_user(tmp_group->id),
				tmp_group->proportion,
				bpu_fc_group_ratio(tmp_group));
		}
	}

	return ret;
}

int32_t bpu_device_add_group(struct device *dev, const struct attribute_group *grp)
{
	return device_add_group(dev, grp);
}
EXPORT_SYMBOL(bpu_device_add_group);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

void bpu_device_remove_group(struct device *dev, const struct attribute_group *grp)
{
	device_remove_group(dev, grp);
}
EXPORT_SYMBOL(bpu_device_remove_group);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

static ssize_t bpu_users_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct bpu_user *tmp_user;
	struct bpu_core *tmp_core;
	struct list_head *pos, *pos_n;
	struct list_head *bpos, *bpos_n;
	int32_t ret = 0;

	if (g_bpu == NULL) {
		return sprintf(buf, "bpu not inited!\n");
	}

	ret += sprintf(buf, "*User via BPU Bus*\n");
	ret += sprintf(buf + ret, "%s\t\t%s\n", "user", "ratio");
	list_for_each_safe(pos, pos_n, &g_bpu->user_list) {
		tmp_user = (struct bpu_user *)list_entry(pos, struct bpu_user, node);/*PRQA S 0497*/ /* Linux Macro */
		if (tmp_user != NULL) {
			ret += sprintf(buf + ret, "%d\t\t%d\n",
				(uint32_t)(tmp_user->id & USER_MASK), bpu_user_ratio(tmp_user));
		}
	}

	list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
		tmp_core = (struct bpu_core *)list_entry(pos, struct bpu_core, node);/*PRQA S 0497*/ /* Linux Macro */
		if (tmp_core != NULL) {
			ret += sprintf(buf + ret,
					"*User via BPU Core(%d)*\n",
					tmp_core->index);
			list_for_each_safe(bpos, bpos_n, &tmp_core->user_list) {
				tmp_user = (struct bpu_user *)list_entry(bpos, struct bpu_user, node);/*PRQA S 0497*/ /* Linux Macro */
				if (tmp_user != NULL) {
					ret += sprintf(buf + ret, "%d\t\t%d\n",
							(uint32_t)(tmp_user->id & USER_MASK),
							bpu_user_ratio(tmp_user));
				}
			}
		}
	}

	return ret;
}
static struct kobj_attribute ratio_info = __ATTR(ratio, S_IRUGO, bpu_ratio_show, NULL);/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
static struct kobj_attribute core_num_info = __ATTR(core_num, S_IRUGO, bpu_core_num_show, NULL);/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
static struct kobj_attribute group_info = __ATTR(group, S_IRUGO, bpu_group_show, NULL);/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */
static struct kobj_attribute users_info = __ATTR(users, S_IRUGO, bpu_users_show, NULL);/*PRQA S 0636*/ /*PRQA S 4501*/ /* Linux Macro */

static struct attribute *bpu_attrs[] = {
	&ratio_info.attr,
	&core_num_info.attr,
	&group_info.attr,
	&users_info.attr,
	NULL,
};

static struct attribute_group bpu_attr_group = {
	.attrs = bpu_attrs,
};

static const struct attribute_group *bpu_attr_groups[] = {
	&bpu_attr_group,
	NULL,
};

struct bus_type bpu_subsys = {
	.name = "bpu",
};

/**
 * bpu_core_create_sys() - create sysfs node for bpu core
 * @core: bpu core, could not be null
 *
 * bpu framework provides create sysfs node for bpu core
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_create_sys(struct bpu_core *core)
{
	char core_name[10];
	int32_t ret;

	if (core == NULL) {
		return -ENODEV;
	}

	if (g_bpu->bus == NULL) {
		dev_err(core->dev, "BPU not register bus for sys\n");
		return -ENODEV;
	}

	ret = sprintf(core_name, "bpu%d", core->index);
	if (ret < 0) {
		dev_err(core->dev, "Create debug name failed\n");
		return ret;
	}

	ret = device_add_group(core->dev, &bpu_core_attr_group);
	if (ret < 0) {
		dev_err(core->dev, "Create bpu core debug group failed\n");
		return ret;
	}

	ret = sysfs_create_link(&bpu_subsys.dev_root->kobj,
			&core->dev->kobj, core_name);
	if (ret != 0) {
		device_remove_group(core->dev, &bpu_core_attr_group);
		dev_err(core->dev, "Create link to bpu bus failed\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(bpu_core_create_sys);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_discard_sys() - remove sysfs node for bpu core
 * @core: bpu core, could not be null
 *
 * when bpu core remove, all bpu core sysfs nodes need be
 * remove.
 */
/* break E1, no need care error return when use the function */
void bpu_core_discard_sys(const struct bpu_core *core)
{
	char core_name[10];
	int32_t ret;

	if (core == NULL) {
		return;
	}

	ret = sprintf(core_name, "bpu%d", core->index);
	if (ret > 0) {
		sysfs_remove_link(&bpu_subsys.dev_root->kobj, core_name);
		device_remove_group(core->dev, &bpu_core_attr_group);
	}

	return;
}
EXPORT_SYMBOL(bpu_core_discard_sys);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/* create bus sub system in /sys/devices/system/ */
int32_t bpu_sys_system_init(struct bpu *bpu)
{
	int32_t ret;

	if (bpu == NULL) {
		pr_err("no device for bpu subsystem\n");
		return -ENODEV;
	}

	ret = subsys_system_register(&bpu_subsys, bpu_attr_groups);
	if (ret != 0) {
		pr_err("failed to register bpu subsystem\n");
	}

	bpu->bus = &bpu_subsys;

	return ret;
}

/* break E1, no need care error return when use the function */
void bpu_sys_system_exit(const struct bpu *bpu)
{
	if (bpu == NULL) {
		return;
	}

	if (bpu->bus != NULL) {
		bus_unregister(bpu->bus);
	}
}

#ifdef CONFIG_FAULT_INJECTION_ATTR
int bpu_core_fault_injection_store(struct device *dev,
		const char *buf, size_t len)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	int32_t ret;

	if (core->hw_enabled > 0u) {
		pr_err("Working BPU Core(%d) Can not Do Fualt Injection\n", core->index);
		return len;
	}
	/* use reserved[3] to store fault inject bits*/
	ret = sscanf(buf, "%lldu", &core->reserved[3]);
	if (ret < 0) {
		return 0;
	}

	ret = mutex_lock_interruptible(&core->mutex_lock);
	if (ret < 0) {
		return 0;
	}
	bpu_core_enable(core);
	bpu_core_disable(core);
	mutex_unlock(&core->mutex_lock);
	core->reserved[3] = 0u;

	return len;
}

static const char *bpu_core_fault_info[] = {
	"1 CSR Fault Inject test(core0:332 or 335 core1:376 or 379)",
	"2 ECC Fault Inject test(core0:331 or 332 core1:375 or 376)",
	"4 Datapath Parity Fault Inject test(core0:332 or 334 core1:376 or 378)",
	"8 Subsys CSR Fault Inject(core0:297 core1:341)",
	"Add value to run the corresponding tests"
};

int bpu_core_fault_injection_show(struct device *dev,
		char *buf, size_t len)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	int32_t count = 0;
	uint32_t i;

	if (sizeof(bpu_core_fault_info) > len) {
		dev_err(dev, "bpu core(%d) bad fault info size\n", core->index);
		return 0;
	}

	for (i = 0; i < sizeof(bpu_core_fault_info) / sizeof(bpu_core_fault_info[0]); i++) {
		count += sprintf(&buf[count], "%s\n", bpu_core_fault_info[i]);
	}

	return count;
}
EXPORT_SYMBOL(bpu_core_fault_injection_store);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */
EXPORT_SYMBOL(bpu_core_fault_injection_show);/*PRQA S 0777*/ /*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

int32_t bpu_property_read_u32(const struct device_node *np,
		const char *propname,
		u32 *out_value)
{
	return of_property_read_u32(np, propname, out_value);
}
EXPORT_SYMBOL(bpu_property_read_u32);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */
#endif
