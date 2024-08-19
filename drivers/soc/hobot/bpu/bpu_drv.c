/*
 * Copyright (C) 2019 Horizon Robotics
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */

#define pr_fmt(fmt) "bpu: " fmt
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <asm/cacheflush.h>
#include <soc/hobot/bpu_notify.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/dma-map-ops.h>
#include "bpu.h"
#include "bpu_core.h"
#include "bpu_ctrl.h"

#define BPU_MAX_ID_NUM	(8192)
#ifdef CONFIG_PCIE_HOBOT_EP_AI
extern int32_t bpu_pac_notifier_register(struct notifier_block *nb);
extern int32_t bpu_pac_notifier_unregister(struct notifier_block *nb);

static void bpu_core_set_pac_feature_bit(struct bpu_core *core)
{
	pr_debug("bpu%d set pac feature bit\n", core->index);
	core->inst.reserved[4] |= BIT(BPU_PAC_BIT_NR_EN);
}

static int bpu_pac_dev_event(struct notifier_block *this,
		unsigned long event,
		void *ptr)
{
	struct list_head *pos, *pos_n;
	struct bpu_core *tmp_core;
	int32_t ret;

	list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
		tmp_core = (struct bpu_core*)pos;
		if (tmp_core != NULL) {
			if (event == 2) {
				(void)bpu_core_disable(tmp_core);
				tmp_core->inst.reserved[4] &= ~BIT(BPU_PAC_BIT_NR_PWR_STATE);
			} else if (event == 1) {
				tmp_core->inst.reserved[4] |= BIT(BPU_PAC_BIT_NR_PWR_STATE);
				(void)bpu_core_enable(tmp_core);
			} else if (event == 3) {
				(void)bpu_core_reset(tmp_core);
			}
		}
	}
	return 0;
}

static struct notifier_block bpu_pac_notifier = {
	.notifier_call  = bpu_pac_dev_event,
};

#else

int32_t bpu_pac_heap_range(uint64_t *base_addr, uint64_t *total_size)
{
	pr_warn_once("dummy %s, please check config CONFIG_PCIE_HOBOT_EP_AI\n", __func__);
	return 0;
}
EXPORT_SYMBOL(bpu_pac_heap_range);

void bpu_pac_raise_irq(uint32_t index)
{
	pr_warn_once("dummy %s, please check config CONFIG_PCIE_HOBOT_EP_AI\n", __func__);
}
EXPORT_SYMBOL(bpu_pac_raise_irq);

uint64_t bpu_pac_fifo_phyaddr(void)
{
	pr_warn_once("dummy %s, please check config CONFIG_PCIE_HOBOT_EP_AI\n", __func__);
	return 0;
}
EXPORT_SYMBOL(bpu_pac_fifo_phyaddr);

#endif

/**
 * bpu framework structure
 *
 * which store bpu framework information
 * the value should not be NULL, when bpu framework
 * module insmod.
 */
struct bpu *g_bpu;
/**
 * bpu framwork record hw io instance
 */
static bpu_hw_io_t *g_bpu_hw_io = NULL;

BLOCKING_NOTIFIER_HEAD(bpu_notifier_chain);/*PRQA S 0685*/ /*PRQA S 2850*/ /* Linux Macro */

int32_t bpu_hw_io_register(bpu_hw_io_t *hw_io)
{
	struct list_head *pos, *pos_n;
	struct bpu_core *tmp_core = NULL;
	unsigned long flags;

	if (hw_io == NULL) {
		pr_err("%s: Register invalid bpu hw io\n", __func__);
		return -EINVAL;
	}

	if ((hw_io->arch <= ARCH_UNKNOWN) || (hw_io->arch >= ARCH_MAX)) {
		pr_err("%s: Register invalid bpu hw io arch(%d)\n",
				__func__, hw_io->arch);
		return -EINVAL;
	}

	if ((hw_io->ops.write_fc == NULL) || (hw_io->ops.read_fc == NULL)) {
		pr_err("%s: Register invalid bpu hw io ops\n", __func__);
		return -EINVAL;
	}

	if (g_bpu_hw_io != NULL) {
		pr_err("%s: Old Register bpu hw io not unregister\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&g_bpu->mutex_lock);
	g_bpu_hw_io = hw_io;
	list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
		tmp_core = (struct bpu_core *)list_entry(pos, struct bpu_core, node);
		if (tmp_core != NULL) {
			mutex_lock(&tmp_core->mutex_lock);
			spin_lock_irqsave(&tmp_core->hw_io_spin_lock, flags);
			if (tmp_core->inst.hw_io_ver != hw_io->version) {
				pr_err("%s: Register invalid [0x%x] bpu hw io version(0x%x)\n",
						__func__, tmp_core->inst.hw_io_ver, hw_io->version);
				spin_unlock_irqrestore(&tmp_core->hw_io_spin_lock, flags);
				mutex_unlock(&tmp_core->mutex_lock);
				g_bpu_hw_io = NULL;
				mutex_unlock(&g_bpu->mutex_lock);
				return -EINVAL;
			}
			tmp_core->hw_io = g_bpu_hw_io;
			spin_unlock_irqrestore(&tmp_core->hw_io_spin_lock, flags);
			mutex_unlock(&tmp_core->mutex_lock);
		}
	}
	mutex_unlock(&g_bpu->mutex_lock);

	return 0;
}
EXPORT_SYMBOL(bpu_hw_io_register);

int32_t bpu_hw_io_unregister(bpu_hw_io_t *hw_io)
{
	struct list_head *pos, *pos_n;
	struct bpu_core *tmp_core = NULL;
	unsigned long flags;

	if (g_bpu_hw_io != hw_io) {
		pr_err("%s: Register invalid bpu hw io\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&g_bpu->mutex_lock);
	g_bpu_hw_io = NULL;
	list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
		tmp_core = (struct bpu_core *)list_entry(pos, struct bpu_core, node);
		if (tmp_core != NULL) {
			mutex_lock(&tmp_core->mutex_lock);
			spin_lock_irqsave(&tmp_core->hw_io_spin_lock, flags);
			tmp_core->hw_io = g_bpu_hw_io;
			spin_unlock_irqrestore(&tmp_core->hw_io_spin_lock, flags);
			mutex_unlock(&tmp_core->mutex_lock);
		}
	}
	mutex_unlock(&g_bpu->mutex_lock);

	return 0;
}
EXPORT_SYMBOL(bpu_hw_io_unregister);

/**
 * bpu_fc_clear() - reset bpu_fc and release fc data
 * @fc: bpu fc in kernel
 *
 * fc_data need release when bpu_fc not use
 */
void bpu_fc_clear(struct bpu_fc *fc)
{
	if (fc != NULL) {
		if (fc->fc_data != NULL) {
			vfree((void *)fc->fc_data);
			fc->fc_data = NULL;
			fc->info.length = 0;
		}
	}
}
EXPORT_SYMBOL(bpu_fc_clear);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

static int32_t bpu_fc_bind_user(struct bpu_fc *fc, struct bpu_user *user)
{
	int32_t ret;

	if ((fc == NULL) || (user == NULL)) {
		pr_err("%s[%d]:bpu bind invalid fc or user\n", __func__, __LINE__);
		ret = -EINVAL;
	} else {
		/*
		 * use the pointer's point to user for judging
		 * whether valid of user after file released
		 */
		fc->user = user->p_file_private;
		fc->user_id = user->id;
		ret = 0;
	}

	return ret;
}

struct bpu_user *bpu_get_user(struct bpu_fc *fc,
		const struct list_head *user_list)
{
	struct list_head *pos, *pos_n;
	struct list_head *bpos, *bpos_n;
	struct bpu_user *tmp_user = NULL;
	struct bpu_core *tmp_core = NULL;

	if (fc == NULL)
		return NULL;

	if (fc->user == NULL)
		return NULL;

	if (user_list == NULL)
		return NULL;

	list_for_each_safe(pos, pos_n, user_list) {
		tmp_user = (struct bpu_user *)list_entry(pos, struct bpu_user, node);/*PRQA S 0497*/ /* Linux Macro */
		if (tmp_user != NULL) {
			if (tmp_user->id == fc->user_id) {
				return tmp_user;
			}
		}
	}

	if (g_bpu == NULL)
		return NULL;

	/* for g_bpu user's task */
	list_for_each_safe(pos, pos_n, &g_bpu->user_list) {
		tmp_user = (struct bpu_user *)list_entry(pos, struct bpu_user, node);/*PRQA S 0497*/ /* Linux Macro */
		if (tmp_user != NULL) {
			if (tmp_user->id == fc->user_id) {
				return tmp_user;
			}
		}
	}

	/* for hotplug user's task */
	list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
		tmp_core = (struct bpu_core *)list_entry(pos, struct bpu_core, node);/*PRQA S 0497*/ /* Linux Macro */
		if (tmp_core != NULL) {
			if (&tmp_core->user_list == user_list)
				continue;
			list_for_each_safe(bpos, bpos_n, &tmp_core->user_list) {
				tmp_user = (struct bpu_user *)list_entry(bpos, struct bpu_user, node);/*PRQA S 0497*/ /* Linux Macro */
				if (tmp_user != NULL) {
					if (tmp_user->id == fc->user_id) {
						return tmp_user;
					}
				}
			}
		}
	}

	return NULL;
}
EXPORT_SYMBOL(bpu_get_user);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

static struct bpu_fc_group *bpu_find_group(uint32_t group_id)
{
	struct bpu_fc_group *tmp_group;
	struct bpu_fc_group *group = NULL;
	struct list_head *pos, *pos_n;
	unsigned long flags;

	spin_lock_irqsave(&g_bpu->group_spin_lock, flags);
	list_for_each_safe(pos, pos_n, &g_bpu->group_list) {
		tmp_group = (struct bpu_fc_group *)(void *)pos;
		if (tmp_group != NULL) {
			if (tmp_group->id == group_id) {
				group = tmp_group;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&g_bpu->group_spin_lock, flags);

	return group;
}

static struct bpu_fc_group *bpu_create_group(uint32_t group_id)
{
	struct bpu_fc_group *tmp_group;
	unsigned long flags;

	tmp_group = bpu_find_group(group_id);
	if (tmp_group == NULL) {
		tmp_group = (struct bpu_fc_group *)kzalloc(sizeof(struct bpu_fc_group), GFP_KERNEL);
		if (tmp_group == NULL) {
			pr_err("BPU create group[%d] failed\n", group_id);
			return tmp_group;
		}
		tmp_group->id = group_id;

		spin_lock_init(&tmp_group->spin_lock);/*PRQA S 3334*/ /* Linux Macro */
		spin_lock_irqsave(&g_bpu->group_spin_lock, flags);
		list_add((struct list_head *)tmp_group, &g_bpu->group_list);
		spin_unlock_irqrestore(&g_bpu->group_spin_lock, flags);
	} else {
		pr_info("BPU already have group[%d]\n", group_id);
	}

	return tmp_group;
}

/* break E1, no error return logic inside the function */
static void bpu_delete_group(uint32_t group_id)
{
	struct bpu_fc_group *tmp_group;
	unsigned long flags;

	tmp_group = bpu_find_group(group_id);
	if (tmp_group != NULL) {
		spin_lock_irqsave(&g_bpu->group_spin_lock, flags);
		list_del((struct list_head *)tmp_group);
		spin_unlock_irqrestore(&g_bpu->group_spin_lock, flags);
		kfree((void *)tmp_group);
	}
}

static int32_t bpu_fc_bind_group(struct bpu_fc *fc, uint32_t group_id)
{
	struct bpu_fc_group *group;

	if (g_bpu == NULL) {
		pr_err("%s[%d]:bpu not inited\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (fc == NULL) {
		pr_err("%s[%d]:bpu bind invalid fc\n", __func__, __LINE__);
		return -EINVAL;
	}

	group = bpu_find_group(group_id);
	if (group != NULL) {
		fc->g_id = group_id;
	} else {
		pr_err("bpu fc find no correspond group\n");
		return -ENODEV;
	}

	return 0;
}

/* create bpu_fc from user fc info*/
static int32_t bpu_fc_create_from_user(struct bpu_fc *fc,
		const struct user_bpu_fc *user_fc, const void *data)
{
	int32_t ret;

	if (user_fc == NULL) {
		return -EINVAL;
	}

	if (fc == NULL) {
		pr_err("bpu user fc need have space to place\n");
		return -EINVAL;
	}

	fc->info = *user_fc;
	fc->fc_data = NULL;
	ret = bpu_fc_bind_group(fc, user_fc->g_id);
	if (ret != 0) {
		pr_err("bpu fc bind group failed\n");
		return ret;
	}

	/* to confirm fc bind mode */
	if ((fc->info.priority & (~USER_PRIO_MASK)) == 0u) {
		fc->bind = false;/*PRQA S 1294*/ /* Linux bool define */
	} else {
		fc->bind = true;/*PRQA S 1294*/ /* Linux bool define */
	}
	fc->info.priority = fc->info.priority & USER_PRIO_MASK;

	if ((data != NULL) && (user_fc->length > 0u)) {
		fc->fc_data = vmalloc(user_fc->length);
		if (fc->fc_data == NULL) {
			pr_err("create bpu fc mem failed\n");
			return -ENOMEM;
		}

		if (copy_from_user(fc->fc_data, (void __user *)data,/*PRQA S 0311*/ /* Linux Macro */
					user_fc->length) != 0u) {
			vfree((void *)fc->fc_data);
			fc->fc_data = NULL;
			pr_err("%s: copy bpu fc data failed from userspace\n", __func__);
			return -EFAULT;
		}
	}

	return (int32_t)user_fc->length;
}

static int32_t bpu_write_prepare(struct bpu_user *user,
		const struct user_bpu_fc *header,
		const char __user *buf, size_t len,
		struct bpu_fc *bpu_fc)
{
	int32_t tmp_raw_fc_len;
	int32_t ret;

	if ((user == NULL) || (header == NULL) ||(buf == NULL) || (len == 0u)) {
		pr_err("Write bpu buffer error\n");
		return -EINVAL;
	}

	tmp_raw_fc_len = bpu_fc_create_from_user(bpu_fc,
		header, buf);
	if (tmp_raw_fc_len <= 0) {
		pr_err("bpu fc from user error\n");
		return -EINVAL;
	}

	bpu_fc->start_point = bpu_get_time_point();

	ret = bpu_fc_bind_user(bpu_fc, user);
	if (ret < 0) {
		bpu_fc_clear(bpu_fc);
		pr_err("bpu buffer bind user error\n");
		return ret;
	}

	return tmp_raw_fc_len + (int32_t)sizeof(struct user_bpu_fc);
}

static struct bpu_user *bpu_get_valid_user(
		const struct bpu_core *core,
		struct bpu_fc *bpu_fc)
{
	struct bpu_user *tmp_user;

	tmp_user = bpu_get_user(bpu_fc, &core->user_list);
	if (tmp_user == NULL) {
		/* no user, so report fake complete */
		return NULL;
	}

	if (tmp_user->is_alive == 0u) {
		dev_err(core->dev, "bpu user now is not alive\n");
		return NULL;
	}

	return tmp_user;
}

static uint32_t bpu_fc_confirm_hw_id(struct bpu_core *core,
		struct bpu_fc *bpu_fc, uint32_t offpos)
{
	uint32_t prio = 0;
	int32_t prio_num = BPU_CORE_PRIO_NUM(core);
	uint32_t task_id_max = BPU_CORE_TASK_ID_MAX(core);

	/*
	 * if hw ip has hardware prio, core->inst.hw_prio_en store hw_prio_en
	 * if hw_prio_en is 0; only use 0 hw id counter
	 */
	if (core->inst.hw_prio_en != 0) {
		prio = bpu_fc->info.priority;
	}

	if ((prio >= prio_num) && (prio_num > 0)) {
		prio = prio_num - 1u;
	}

	/* olny fc first set need set hw_id, the left set not need */
	if (offpos == 0u) {
		if (bpu_fc->info.id != 0u) {
			bpu_fc->hw_id = (uint32_t)atomic_read(&core->hw_id_counter[prio]);
			/* task_id_max for sched trig */
			if (bpu_fc->hw_id >= BPU_CORE_TASK_ID(core, task_id_max - 1)) {
				atomic_set(&core->hw_id_counter[prio], 1);
			} else {
				atomic_inc(&core->hw_id_counter[prio]);
			}
			/* use the hw_id to tell soc id and priority */
			bpu_fc->hw_id = BPU_CORE_TASK_PRIO_ID(core, prio, bpu_fc->hw_id);
		} else {
			bpu_fc->hw_id = 0;
		}
	}

	return prio;
}

/*
 * write the user fc buffer to real core and bind user
 */
int32_t bpu_write_fc_to_core(struct bpu_core *core,
		struct bpu_fc *bpu_fc, uint32_t offpos)
{
	struct bpu_user *tmp_user;
	unsigned long flags;
	unsigned long user_flags;
	unsigned long hwio_flags;
	int32_t ret;
	int32_t write_fc_num;
	uint32_t prio;

	if ((core == NULL) || (bpu_fc == NULL)) {
		pr_err("Write bpu buffer error\n");
		return -EINVAL;
	}

	if (bpu_core_is_pending(core) != 0) {
		dev_dbg(core->dev, "bpu core now pending\n");/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */
		return -EBUSY;
	}

	/* write data to bpu task range */
	spin_lock_irqsave(&core->hw_io_spin_lock, hwio_flags);
	if (core->hw_io == NULL) {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, hwio_flags);
			bpu_fc_clear(bpu_fc);
			dev_err(core->dev, "No BPU HW IO for Core to use!\n");
			return -ENODEV;
	}

	if (core->hw_io->ops.write_fc != NULL) {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, hwio_flags);
		prio = bpu_fc_confirm_hw_id(core, bpu_fc, offpos);

		spin_lock_irqsave(&core->spin_lock, flags);
		spin_lock_irqsave(&g_bpu->user_spin_lock, user_flags);
		tmp_user = bpu_get_valid_user(core, bpu_fc);
		if (tmp_user == NULL) {
			spin_unlock_irqrestore(&g_bpu->user_spin_lock, user_flags);
			spin_unlock_irqrestore(&core->spin_lock, flags);
			/* no user, so report fake complete */
			ret = (int32_t)bpu_fc->info.slice_num - (int32_t)offpos;
			bpu_fc_clear(bpu_fc);
			bpu_prio_trig_out(core->prio_sched);
			return ret;
		}

		spin_lock_irqsave(&core->hw_io_spin_lock, hwio_flags);
		if (core->hw_io != NULL) {
			if (core->hw_io->ops.write_fc != NULL) {
				write_fc_num = core->hw_io->ops.write_fc(&core->inst, bpu_fc, offpos);
			}
		} else {
			spin_unlock_irqrestore(&core->hw_io_spin_lock, hwio_flags);
			spin_unlock_irqrestore(&g_bpu->user_spin_lock, user_flags);
			spin_unlock_irqrestore(&core->spin_lock, flags);
			bpu_fc_clear(bpu_fc);
			dev_err(core->dev, "No BPU HW IO for Core to use!\n");
			return -ENODEV;
		}
		spin_unlock_irqrestore(&core->hw_io_spin_lock, hwio_flags);

		if (write_fc_num != ((int32_t)bpu_fc->info.slice_num - (int32_t)offpos)) {
			/* write raw task to hw fifo not complete or error */
			spin_unlock_irqrestore(&g_bpu->user_spin_lock, user_flags);
			spin_unlock_irqrestore(&core->spin_lock, flags);
			return write_fc_num;
		}
		if (bpu_fc->info.id != 0u) {
			tmp_user->running_task_num++;
			core->running_task_num++;
			ret = kfifo_in(&core->run_fc_fifo[prio],/*PRQA S 4461*/ /* Linux Macro */
					bpu_fc, 1);
			if (ret < 1) {
				core->running_task_num--;
				tmp_user->running_task_num--;
				spin_unlock_irqrestore(&g_bpu->user_spin_lock, user_flags);
				spin_unlock_irqrestore(&core->spin_lock, flags);
				dev_err(core->dev, "bpu request to fifo failed\n");
				return -EBUSY;
			}
			core->buffered_time[prio] += bpu_fc->info.process_time;
		} else {
			bpu_fc_clear(bpu_fc);
		}
		spin_unlock_irqrestore(&g_bpu->user_spin_lock, user_flags);
		spin_unlock_irqrestore(&core->spin_lock, flags);
	} else {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, hwio_flags);
		bpu_fc_clear(bpu_fc);
		dev_err(core->dev, "no real bpu to process\n");
		return -ENODEV;
	}

	return write_fc_num;
}

/**
 * bpu_core_is_online() - check if bpu core online
 * @core: bpu core, can't be null
 *
 * Check if bpu core can be set task, if not online,
 * task can't set to bpu core hardware.
 *
 * Return:
 * * true			- bpu core online
 * * false			- bpu core not online
 */
bool bpu_core_is_online(const struct bpu_core *core)
{
	if (core == NULL) {
		return false;/*PRQA S 1294*/ /* Linux bool define */
	}

	if (core->hw_enabled == 0u) {
		return false;/*PRQA S 1294*/ /* Linux bool define */
	}

	return bpu_prio_is_plug_in(core->prio_sched);
}
EXPORT_SYMBOL(bpu_core_is_online);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

static struct bpu_core *bpu_opt_core(const struct bpu *bpu, uint64_t core_mask)
{
	struct bpu_core *tmp_core, *tmp_opt_core = NULL;
	struct list_head *pos, *pos_n;
	int32_t tmp_val, tmp_last_val = -1;
	int32_t ret;

	if (bpu == NULL) {
		return NULL;
	}

	list_for_each_safe(pos, pos_n, &bpu->core_list) {
		tmp_core = (struct bpu_core*)pos;
		if (bpu_core_is_online(tmp_core)) {
			if ((core_mask & ((uint64_t)0x1u << (uint32_t)tmp_core->index)) != 0u) {
				ret = mutex_lock_interruptible(&tmp_core->mutex_lock);
				if (ret < 0) {
					return NULL;
				}
				if ((atomic_read(&tmp_core->open_counter) != 0) &&
						(tmp_core->hw_enabled != 0u)) {
					tmp_val = kfifo_avail(&tmp_core->run_fc_fifo[0]);/*PRQA S 4461*/ /* Linux Macro */
					if (tmp_val > tmp_last_val) {
						tmp_opt_core = tmp_core;
						tmp_last_val = tmp_val;
					}
				}
				mutex_unlock(&tmp_core->mutex_lock);
			}
		}
	}

	return tmp_opt_core;
}

static int32_t bpu_fc_to_prio_sched(const struct bpu_core *core,
		struct bpu_fc *bpu_fc, uint64_t core_mask,
		uint64_t run_c_mask)
{
	struct bpu_core *tmp_core;
	uint64_t tmp_core_mask = core_mask;
	uint64_t tmp_run_c_mask = run_c_mask;
	int32_t ret;

	if (!bpu_core_is_online(core)) {
		/*
		 * choose optimal core according to core stauts
		 * FIXME: need find core according core_mask flag
		 */
		ret = mutex_lock_interruptible(&g_bpu->mutex_lock);
		if (ret < 0) {
			return ret;
		}
		if (tmp_run_c_mask != 0u) {
			tmp_core_mask &= tmp_run_c_mask;
		}

		if (core != NULL) {
			/* if core support hotplug choose other cores */
			if ((tmp_core_mask == ((uint64_t)0x1 << (uint32_t)core->index))
					&& (core->hotplug != 0u)) {
				tmp_core_mask = ALL_CORE_MASK;
			}
		}
		tmp_core = bpu_opt_core(g_bpu, tmp_core_mask);
		if (tmp_core == NULL) {
			mutex_unlock(&g_bpu->mutex_lock);
			pr_err("BPU has no suitable core, 0x%llx!", tmp_run_c_mask);
			return -ENODEV;
		}
		mutex_unlock(&g_bpu->mutex_lock);
		ret = bpu_prio_in(tmp_core->prio_sched, bpu_fc);
	} else {
		bpu_fc->info.core_mask = ((uint64_t)0x1u << (uint32_t)core->index);
		ret = bpu_prio_in(core->prio_sched, bpu_fc);
	}

	return ret;
}

/**
 * bpu_write_with_user() - write user data to bpu core with user
 * @core: bpu core, could be null
 * @user: bpu user, which represent a user in bpu driver
 * @buf: userspace data buffer
 * @len: userspace set buffer len
 *
 * write user data to special bpu core, if bpu core is
 * null, choose the bpu core which much more leisure.
 *
 * Return:
 * * >=0		- The actual length of the bpu core is written
 * * <0			- error code
 */
int32_t bpu_write_with_user(const struct bpu_core *core,
			struct bpu_user *user,
			const char __user buf[], size_t len)
{
	uint64_t tmp_core_mask;
	uint64_t tmp_run_c_mask;
	struct bpu_fc tmp_bpu_fc;
	struct user_bpu_fc header;
	uint32_t use_len = 0;
	int32_t prepare_len;
	int32_t ret;

	if ((user == NULL) || (buf == NULL) || (len == 0u)) {
		pr_err("Write bpu buffer error\n");
		return -EINVAL;
	}


	if (len <= sizeof(struct user_bpu_fc)) {
		pr_err("BPU: Write invalied data\n");
		return -EINVAL;
	}

	while ((len - use_len) > (uint32_t)sizeof(struct user_bpu_fc)) {
		if (copy_from_user(&header, &buf[use_len],
					sizeof(struct user_bpu_fc)) != 0) {
			pr_err("%s: copy data failed from userspace when bpu write\n", __func__);
			return -EFAULT;
		}
		tmp_core_mask = header.core_mask;
		tmp_run_c_mask = header.run_c_mask;

		prepare_len = bpu_write_prepare(user, &header,
				&buf[use_len + sizeof(struct user_bpu_fc)],
				len - use_len, &tmp_bpu_fc);
		if (prepare_len <= 0) {
			pr_err("BPU prepare user write data!");
			return -EINVAL;
		}
		ret = bpu_fc_to_prio_sched(core, &tmp_bpu_fc,
				tmp_core_mask, tmp_run_c_mask);
		if (ret < 0) {
			pr_err("write bpu fc to core failed\n");
			return ret;
		}

		use_len += (uint32_t)prepare_len;
	}

	return (int32_t)use_len;
}
EXPORT_SYMBOL(bpu_write_with_user);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_read_with_user() - read bpu process result data with user
 * @core: bpu core, could be null
 * @user: bpu user, which represent a user in bpu driver
 * @buf: the buffer to put data
 * @len: max read buf len
 *
 * read the bpu process result data from the underlying buffer
 * which specified user
 *
 * Return:
 * * >=0		- The actual length of get data
 * * <0			- error code
 */
int32_t bpu_read_with_user(struct bpu_core *core,
			struct bpu_user *user,
			const char __user *buf, size_t len)
{
	int32_t ret, copied;

	if ((user == NULL) || (buf == NULL) || (len == 0u)) {
		return 0;
	}

	if (kfifo_initialized(&user->done_fcs) == 0) {
		return -EINVAL;
	}

	ret = mutex_lock_interruptible(&user->mutex_lock);
	if (ret < 0) {
		return ret;
	}

	ret = kfifo_to_user(&user->done_fcs, (void __user *)buf, len, &copied);/*PRQA S 0311*/ /*PRQA S 0674*/ /*PRQA S 4434*/ /*PRQA S 4461*/ /* Linux Macro */
	if (ret < 0) {
		mutex_unlock(&user->mutex_lock);
		return ret;
	}
	mutex_unlock(&user->mutex_lock);

	return copied;
}
EXPORT_SYMBOL(bpu_read_with_user);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

int32_t bpu_notifier_register(struct notifier_block *nb)
{
	return (int32_t)blocking_notifier_chain_register(&bpu_notifier_chain, nb);
}
EXPORT_SYMBOL(bpu_notifier_register);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

int32_t bpu_notifier_unregister(struct notifier_block *nb)
{
	return (int32_t)blocking_notifier_chain_unregister(&bpu_notifier_chain, nb);
}
EXPORT_SYMBOL(bpu_notifier_unregister);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

int32_t bpu_notifier_notify(uint64_t val, void *v)
{
	return (int32_t)blocking_notifier_call_chain(&bpu_notifier_chain,
			(unsigned long)val, v);
}

uint16_t bpu_core_avl_cap(struct bpu_core *core)
{
	uint32_t i;
	uint16_t cap = 0u;
	uint32_t prio_left;
	int32_t prio_num;

	if (core == NULL) {
		return 0u;
	}

	prio_num = BPU_CORE_PRIO_NUM(core);

	for (i = 0u; i < prio_num; i++) {
		if (i == 0) {
			cap = kfifo_avail(&core->run_fc_fifo[i]);/*PRQA S 4461*/ /* Linux Macro */
		} else {
			cap = min((uint16_t)kfifo_avail(&core->run_fc_fifo[0]), cap);/*PRQA S 0478*/ /*PRQA S 4434*/ /*PRQA S 4461*/ /* Linux Macro */
		}
	}

	prio_left = bpu_prio_left_task_num(core->prio_sched);

	if ((uint32_t)cap >= prio_left) {
		cap -= (uint16_t)prio_left;
	} else {
		cap = 0u;
	}

	return cap;
}
EXPORT_SYMBOL(bpu_core_avl_cap);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

static long bpu_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct list_head *pos, *pos_n;
	struct bpu_fc_group *group;
	struct bpu_core *tmp_core;

	struct bpu_group tmp_group;
	struct bpu_fc_run_time tmp_fc_run_time;
	uint32_t ratio;
	uint64_t core;
	uint16_t cap;
	uint32_t limit;
	int32_t ret = 0;

	switch (cmd) {
	case BPU_SET_GROUP:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		if (copy_from_user(&tmp_group, (void __user *)arg, _IOC_SIZE(cmd))) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("%s: BPU: copy data failed from userspace\n", __func__);
			return -EFAULT;
		}

		ret = mutex_lock_interruptible(&g_bpu->mutex_lock);
		if (ret < 0) {
			return ret;
		}
		group = bpu_find_group(tmp_group.group_id);
		if (group != NULL) {
			if (tmp_group.prop <= 0u) {
				bpu_delete_group(tmp_group.group_id);
				mutex_unlock(&g_bpu->mutex_lock);
				break;
			}
			group->proportion = (int32_t)tmp_group.prop;
		} else {
			if (tmp_group.prop <= 0u) {
				mutex_unlock(&g_bpu->mutex_lock);
				break;
			}
			group = bpu_create_group(tmp_group.group_id);
			if (group != NULL) {
				group->proportion = (int32_t)tmp_group.prop;
			} else {
				mutex_unlock(&g_bpu->mutex_lock);
				pr_err("BPU create group(%d)(%d) prop(%d) error\n",
						bpu_group_id(tmp_group.group_id),
						bpu_group_user(tmp_group.group_id),
						tmp_group.prop);
				return -EINVAL;
			}
		}
		mutex_unlock(&g_bpu->mutex_lock);
		pr_debug("BPU set group(%d)(%d) prop [%d]\n",/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */
				bpu_group_id(tmp_group.group_id),
				bpu_group_user(tmp_group.group_id),
				tmp_group.prop);
		break;
	case BPU_GET_GROUP:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		if (copy_from_user(&tmp_group, (void __user *)arg, _IOC_SIZE(cmd))) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("%s: BPU: copy data failed from userspace\n", __func__);
			return -EFAULT;
		}

		ret = mutex_lock_interruptible(&g_bpu->mutex_lock);
		if (ret < 0) {
			return ret;
		}
		group = bpu_find_group(tmp_group.group_id);
		if (group != NULL) {
			tmp_group.prop = (uint32_t)group->proportion;
			tmp_group.ratio = bpu_fc_group_ratio(group);
		}
		mutex_unlock(&g_bpu->mutex_lock);

		if (copy_to_user((void __user *)arg, &tmp_group, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("BPU: copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	case BPU_GET_RATIO:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		ratio = bpu_ratio(g_bpu);
		if (copy_to_user((void __user *)arg, &ratio, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("BPU: copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	case BPU_GET_CAP:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		cap = 0;
		ret = mutex_lock_interruptible(&g_bpu->mutex_lock);
		if (ret < 0) {
			return ret;
		}
		list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
			tmp_core = (struct bpu_core*)pos;
			if (tmp_core != NULL) {
				ret = mutex_lock_interruptible(&tmp_core->mutex_lock);
				if (ret < 0) {
					mutex_unlock(&g_bpu->mutex_lock);
					return ret;
				}
				cap = max(bpu_core_avl_cap(tmp_core), cap);/*PRQA S 0478*/ /*PRQA S 4434*/ /* Linux Macro */
				mutex_unlock(&tmp_core->mutex_lock);
			}
		}
		mutex_unlock(&g_bpu->mutex_lock);
		if (copy_to_user((void __user *)arg, &cap, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("PBU: copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	case BPU_OPT_CORE:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		if (copy_from_user(&core, (void __user *)arg, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("%s: BPU: copy data failed from userspace\n", __func__);
			return -EFAULT;
		}
		ret = mutex_lock_interruptible(&g_bpu->mutex_lock);
		if (ret < 0) {
			return ret;
		}
		tmp_core = bpu_opt_core(g_bpu, core);
		if (tmp_core == NULL) {
			mutex_unlock(&g_bpu->mutex_lock);
			pr_err("Can't find an optimal BPU Core\n");
			return -EFAULT;
		}
		core = (uint32_t)tmp_core->index;
		mutex_unlock(&g_bpu->mutex_lock);

		if (copy_to_user((void __user *)arg, &core, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("BPU: copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	case BPU_RESET:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
			tmp_core = (struct bpu_core*)pos;
			if (tmp_core != NULL) {
				ret = mutex_lock_interruptible(&tmp_core->mutex_lock);
				if (ret < 0) {
					return ret;
				}
				ret = bpu_core_reset(tmp_core);
				mutex_unlock(&tmp_core->mutex_lock);
				if (ret != 0) {
					pr_err("BPU Core reset failed\n");
				}
			}
		}
		break;
	case BPU_SET_LIMIT:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		if (copy_from_user(&limit, (void __user *)arg, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("%s: BPU: copy data failed from userspace\n", __func__);
			return -EFAULT;
		}
		list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
			tmp_core = (struct bpu_core*)pos;
			if (tmp_core != NULL) {
				ret = bpu_core_set_limit(tmp_core, (int32_t)limit);
				if (ret != 0) {
					pr_err("BPU Core set prio limit failed\n");
				}
			}
		}
		break;
	case BPU_FC_RUN_TIME:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		if (copy_from_user(&tmp_fc_run_time, (void __user *)arg, _IOC_SIZE(cmd))) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("%s: copy data failed from userspace\n", __func__);
			return -EFAULT;
		}

		ret = bpu_check_fc_run_time(NULL, &tmp_fc_run_time);
		if (ret < 0) {
			tmp_fc_run_time.run_time = 0;
		}

		if (copy_to_user((void __user *)arg, &tmp_fc_run_time, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	default:
		pr_err("%s: BPU invalid ioctl argument\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static unsigned int bpu_poll(struct file *filp, poll_table *wait)
{
	struct bpu_user *user = (struct bpu_user *)filp->private_data;
	uint32_t mask = 0;

	poll_wait(filp, &user->poll_wait, wait);
	if (mutex_lock_interruptible(&user->mutex_lock) < 0) {
		return mask;
	}

	if (kfifo_len(&user->done_fcs) != 0) {
		mask |= POLLIN | POLLRDNORM;
	}

	mutex_unlock(&user->mutex_lock);

	return mask;
}

static ssize_t bpu_read(struct file *filp,
		char __user *buf, size_t len, loff_t *f_pos)
{
	struct bpu_user *user = (struct bpu_user *)filp->private_data;

	return bpu_read_with_user(NULL, user, buf, len);
}

static ssize_t bpu_write(struct file *filp,
		const char __user *buf, size_t len, loff_t *f_pos)
{
	struct bpu_user *user = (struct bpu_user *)filp->private_data;

	return bpu_write_with_user(NULL, user, buf, len);
}

static int bpu_open(struct inode *inode, struct file *filp)
{
	struct bpu *bpu = (struct bpu *)container_of(filp->private_data, struct bpu, miscdev);/*PRQA S 0497*/ /* Linux Macro */
	struct bpu_user *user;
	unsigned long flags;
	unsigned long user_flags;
	int32_t ret;

	mutex_lock(&bpu->mutex_lock);
	if (atomic_read(&bpu->open_counter) == 0) {
		/* first open init something files */
		ret = bpu_sched_start(bpu);
		if (ret != 0) {
			mutex_unlock(&bpu->mutex_lock);
			pr_err("BPU sched start failed\n");
			return -EFAULT;
		}
	}

	user = (struct bpu_user *)kzalloc(sizeof(struct bpu_user), GFP_KERNEL);
	if (user == NULL) {
		mutex_unlock(&bpu->mutex_lock);
		pr_err("BPU: Can't alloc user mem");
		return -ENOMEM;
	}

	user->id = (uint64_t)((uint32_t)task_pid_nr(current->group_leader)
				| ((uint64_t)prandom_u32_max(255) << USER_RANDOM_SHIFT));

	/* init fifo which report to userspace */
	ret = kfifo_alloc(&user->done_fcs, BPU_CORE_RECORE_NUM, GFP_KERNEL);
	if (ret != 0) {
		mutex_unlock(&bpu->mutex_lock);
		kfree((void *)user);
		pr_err("BPU: Can't alloc user fifo dev mem");
		return ret;
	}

	init_waitqueue_head(&user->poll_wait);
	user->host = (void *)bpu;
	user->p_file_private = &filp->private_data;
	spin_lock_irqsave(&bpu->spin_lock, flags);
	spin_lock_irqsave(&bpu->user_spin_lock, user_flags);
	list_add((struct list_head *)user, &g_bpu->user_list);
	spin_unlock_irqrestore(&bpu->user_spin_lock, user_flags);
	spin_unlock_irqrestore(&bpu->spin_lock, flags);
	spin_lock_init(&user->spin_lock);/*PRQA S 3334*/ /* Linux Macro */
	mutex_init(&user->mutex_lock);/*PRQA S 3334*/ /* Linux Macro */
	init_completion(&user->no_task_comp);
	user->is_alive = 1;
	user->version = 0;
	user->running_task_num = 0;
	/* replace user the private to store user */
	filp->private_data = user;

	atomic_inc(&bpu->open_counter);
	mutex_unlock(&bpu->mutex_lock);

	return ret;
}

static int bpu_release(struct inode *inode, struct file *filp)
{
	struct bpu_user *user = (struct bpu_user *)filp->private_data;
	struct bpu *bpu = (struct bpu *)user->host;
	struct list_head *pos, *pos_n;
	struct bpu_fc_group *tmp_group;
	unsigned long flags;
	unsigned long user_flags;

	mutex_lock(&bpu->mutex_lock);
	user->is_alive = 0;

	atomic_dec(&bpu->open_counter);

	if (atomic_read(&bpu->open_counter) == 0) {
		/* release the real bpu*/
		bpu_sched_stop(bpu);

		spin_lock_irqsave(&bpu->spin_lock, flags);
		spin_lock(&bpu->group_spin_lock);
		list_for_each_safe(pos, pos_n, &g_bpu->group_list) {
			tmp_group = (struct bpu_fc_group *)pos;
			if (tmp_group != NULL) {
				list_del((struct list_head *)tmp_group);
				kfree((void *)tmp_group);
			}
		}
		spin_unlock(&bpu->group_spin_lock);
		spin_unlock_irqrestore(&bpu->spin_lock, flags);
	} else {
		/* delete the user special group*/
		spin_lock_irqsave(&bpu->spin_lock, flags);
		spin_lock(&bpu->group_spin_lock);
		list_for_each_safe(pos, pos_n, &g_bpu->group_list) {
			tmp_group = (struct bpu_fc_group *)pos;
			if (tmp_group != NULL) {
				if (bpu_group_user(tmp_group->id) == (uint32_t)(user->id & USER_MASK)) {
					list_del((struct list_head *)tmp_group);
					kfree((void *)tmp_group);
				}
			}
		}
		spin_unlock(&bpu->group_spin_lock);
		spin_unlock_irqrestore(&bpu->spin_lock, flags);
	}

	spin_lock_irqsave(&bpu->spin_lock, flags);
	spin_lock_irqsave(&g_bpu->user_spin_lock, user_flags);
	list_del((struct list_head *)user);
	spin_unlock_irqrestore(&g_bpu->user_spin_lock, user_flags);
	kfifo_free(&user->done_fcs);
	kfree((void *)user);
	filp->private_data = NULL;
	spin_unlock_irqrestore(&bpu->spin_lock, flags);
	mutex_unlock(&bpu->mutex_lock);

	return 0;
}

static const struct file_operations bpu_fops = {
	.owner		= THIS_MODULE,
	.open		= bpu_open,
	.release	= bpu_release,
	.read		= bpu_read,
	.write		= bpu_write,
	.poll		= bpu_poll,
	.unlocked_ioctl = bpu_ioctl,
	.compat_ioctl = bpu_ioctl,
};

/**
 * bpu_core_register() - register bpu core to bpu framework
 * @core: bpu core, could not be null
 *
 * all bpu core need register to bpu framework, bpu framework
 * can use record to access and control each bpu core
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_register(struct bpu_core *core)
{
	unsigned long flags;

	if (g_bpu == NULL) {
		pr_err("bpu not inited!\n");
		return -ENODEV;
	}

	if (core == NULL) {
		pr_err("BPU: Register Invalid core!\n");
		return -EINVAL;
	}

	/*
	 * add to bpu core list
	 * only add/del when bpu core module insmod, so not use
	 * lock protect when list for each. (violate checklist B13)
	 */
	mutex_lock(&g_bpu->mutex_lock);
	if (g_bpu_hw_io != NULL) {
		if (core->inst.hw_io_ver != g_bpu_hw_io->version) {
			pr_err("%s: Register invalid [0x%x] bpu core version(0x%x)\n",
					__func__, g_bpu_hw_io->version, core->inst.hw_io_ver);
			mutex_unlock(&g_bpu->mutex_lock);
			return -EINVAL;
		}
	}
	list_add_tail((struct list_head *)core, &g_bpu->core_list);
	core->host = g_bpu;
	spin_lock_irqsave(&core->hw_io_spin_lock, flags);
	core->hw_io = g_bpu_hw_io;
	spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
	mutex_unlock(&g_bpu->mutex_lock);

#ifdef CONFIG_PCIE_HOBOT_EP_AI
	bpu_core_set_pac_feature_bit(core);
#endif

	return 0;
}
EXPORT_SYMBOL(bpu_core_register);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_core_unregister() - unregister bpu core from bpu framework
 * @core: bpu core
 *
 * when not use bpu core, need unregister from bpu framework
 */
void bpu_core_unregister(struct bpu_core *core)
{
	unsigned long flags;

	if (g_bpu == NULL) {
		pr_err("bpu not inited!\n");
		return;
	}

	if (core == NULL) {
		pr_err("BPU: Unregister Invalid core!\n");
		return;
	}

	/* del to bpu core list */
	mutex_lock(&g_bpu->mutex_lock);
	core->host = NULL;
	spin_lock_irqsave(&core->hw_io_spin_lock, flags);
	core->hw_io = NULL;
	spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
	list_del((struct list_head *)core);
	mutex_unlock(&g_bpu->mutex_lock);
}
EXPORT_SYMBOL(bpu_core_unregister);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_extra_ops_register() - register bpu core extend operation
 * @ops: the operation, could not be null
 *
 * registed operations will be used when bpu hw operations.
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_extra_ops_register(struct bpu_extra_ops *ops)
{
	if (g_bpu == NULL) {
		return -ENODEV;
	}

	if (g_bpu->extra_ops != NULL) {
		return -EBUSY;
	}

	g_bpu->extra_ops = ops;

	return 0;
}
EXPORT_SYMBOL(bpu_extra_ops_register);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_extra_ops_unregister() - unregister bpu core extend operation
 * @ops: the operation, could not be null
 *
 * unregisted operations which not need by bpu hw operations.
 */
/* break E1, no need care error return when use the function */
void bpu_extra_ops_unregister(const struct bpu_extra_ops *ops)
{
	if (g_bpu != NULL) {
		if (g_bpu->extra_ops == ops) {
			g_bpu->extra_ops = NULL;
		}
	}
}
EXPORT_SYMBOL(bpu_extra_ops_unregister);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_core_driver_register() - register bpu core driver
 * @driver: platform driver struct, could not be null
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_driver_register(struct platform_driver *driver)
{
	return platform_driver_register(driver);
}
EXPORT_SYMBOL(bpu_core_driver_register);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_core_driver_unregister() - unregister bpu core driver
 * @driver: platform driver struct, could not be null
 */
void bpu_core_driver_unregister(struct platform_driver *driver)
{
	platform_driver_unregister(driver);
}
EXPORT_SYMBOL(bpu_core_driver_unregister);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

static int __init bpu_init(void)
{
	struct bpu *bpu;
	int32_t ret;

	bpu = (struct bpu *)kzalloc(sizeof(struct bpu), GFP_KERNEL);
	if (bpu == NULL) {
		pr_err("BPU: Can't alloc bpu mem\n");
		return -ENOMEM;
	}

	mutex_init(&bpu->mutex_lock);/*PRQA S 3334*/ /* Linux Macro */
	spin_lock_init(&bpu->spin_lock);/*PRQA S 3334*/ /* Linux Macro */
	INIT_LIST_HEAD(&bpu->core_list);
	INIT_LIST_HEAD(&bpu->user_list);
	spin_lock_init(&bpu->user_spin_lock);/*PRQA S 3334*/ /* Linux Macro */
	INIT_LIST_HEAD(&bpu->group_list);
	spin_lock_init(&bpu->group_spin_lock);/*PRQA S 3334*/ /* Linux Macro */
	mutex_init(&bpu->sched_mutex_lock);

	bpu->miscdev.minor = MISC_DYNAMIC_MINOR;
	bpu->miscdev.name = "bpu";
	bpu->miscdev.fops = &bpu_fops;

	ret = misc_register(&bpu->miscdev);
	if (ret != 0) {
		kfree((void *)bpu);
		pr_err("Register bpu device failed\n");
		return ret;
	}

	atomic_set(&bpu->open_counter, 0);

	ret = bpu_sys_system_init(bpu);
	if (ret != 0) {
		misc_deregister(&bpu->miscdev);
		kfree((void *)bpu);
		pr_err("Register bpu sub system failed\n");
		return ret;
	}

	g_bpu = bpu;
#ifdef CONFIG_PCIE_HOBOT_EP_AI
	bpu_pac_notifier_register(&bpu_pac_notifier);
#endif

	return 0;
}

static void __exit bpu_exit(void)
{
	struct bpu *bpu = g_bpu;

	if (bpu != NULL) {
#ifdef CONFIG_PCIE_HOBOT_EP_AI
		bpu_pac_notifier_unregister(&bpu_pac_notifier);
#endif
		bpu_sys_system_exit(bpu);
		misc_deregister(&bpu->miscdev);

		kfree((void *)bpu);

		g_bpu = NULL;
	}
}

module_init(bpu_init);/*PRQA S 0605*/ /* Linux Macro */
module_exit(bpu_exit);/*PRQA S 0605*/ /* Linux Macro */

MODULE_DESCRIPTION("Driver for Horizon BPU");
MODULE_AUTHOR("Zhang Guoying <guoying.zhang@horizon.ai>");
MODULE_LICENSE("GPL v2");
