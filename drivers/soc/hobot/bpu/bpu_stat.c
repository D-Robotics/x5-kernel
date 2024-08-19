/*
 * Copyright (C) 2019 Horizon Robotics
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */
#include "bpu.h"
#include "bpu_core.h"

#define P_RESET_COEF	(16u)

static uint64_t time_interval(const uint64_t p_old, const uint64_t p_new)
{
	return p_new - p_old;
}

uint64_t bpu_get_time_point(void)
{
	uint64_t tmp_point;

	tmp_point = (uint64_t)ktime_get_boottime();

	return tmp_point;
}
EXPORT_SYMBOL(bpu_get_time_point);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_core_update() - update the bpu core statistic by bpu fc task
 * @core: bpu core, could not be null
 * @bpu_fc: bpu fc task in kernel
 *
 * bpu time statistics mainly trigger by fc event
 */
/* break E1, no need error return logic inside the function */
void bpu_core_update(struct bpu_core *core, struct bpu_fc *fc)
{
	struct bpu_fc_group *tmp_fc_group;
	struct bpu_user *tmp_user;
	struct list_head *pos, *pos_n;
	uint64_t tmp_start_point;
	uint64_t tmp_time;
	unsigned long flags;
	unsigned long user_flags;
	int32_t prio;

	if ((core == NULL) || (fc == NULL)) {
		return;
	}

	prio = BPU_CORE_TASK_PRIO(core, fc->hw_id);
	spin_lock_irqsave(&core->spin_lock, flags);
	/* update the bpu core bufferd running time */
	if (core->buffered_time[prio] >= fc->info.process_time) {
		core->buffered_time[prio] -= fc->info.process_time;
	} else {
		core->buffered_time[prio] = 0;
	}

	fc->end_point = bpu_get_time_point();

	fc->info.process_time = time_interval((uint64_t)fc->start_point, (uint64_t)fc->end_point);

	/* update for start point for interval calc */
	if (fc->start_point < core->last_done_point) {
		tmp_start_point = (uint64_t)core->last_done_point;
	} else {
		tmp_start_point = (uint64_t)fc->start_point;
	}

	if (tmp_start_point >= (uint64_t)core->p_start_point) {
		tmp_time = time_interval(tmp_start_point, (uint64_t)fc->end_point);
	} else {
		tmp_time = time_interval((uint64_t)core->p_start_point, (uint64_t)fc->end_point);
	}

	core->p_run_time += tmp_time;

	/* update the global slowest time */
	if (tmp_time > g_bpu->slow_task_time) {
		g_bpu->slow_task_time = tmp_time;
	}

	spin_lock(&g_bpu->group_spin_lock);
	list_for_each_safe(pos, pos_n, &g_bpu->group_list) {
		tmp_fc_group = (struct bpu_fc_group *)pos;
		if (tmp_fc_group != NULL) {
			if (tmp_fc_group->id == fc->g_id) {
				tmp_fc_group->p_run_time += tmp_time;
			}
		}
	}
	spin_unlock(&g_bpu->group_spin_lock);

	spin_lock_irqsave(&g_bpu->user_spin_lock, user_flags);
	tmp_user = bpu_get_user(fc, &core->user_list);
	if (tmp_user != NULL) {
		tmp_user->p_run_time += tmp_time;
	}
	spin_unlock_irqrestore(&g_bpu->user_spin_lock, user_flags);

	core->last_done_point = fc->end_point;

	spin_unlock_irqrestore(&core->spin_lock, flags);
}
EXPORT_SYMBOL(bpu_core_update);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

static int32_t bpu_core_stat_reset(struct bpu_core *core)
{
	uint64_t tmp_start_point;
	struct list_head *pos, *pos_n;
	struct bpu_user *tmp_user;
	uint64_t tmp_time;
	unsigned long flags;
	unsigned long user_flags;

	if (core == NULL) {
		return -EINVAL;
	}

	spin_lock_irqsave(&core->spin_lock, flags);

	tmp_start_point = bpu_get_time_point();
	core->p_run_time /= P_RESET_COEF;

	/* reset the core user p_run_time */
	list_for_each_safe(pos, pos_n, &core->user_list) {
		tmp_user = (struct bpu_user *)list_entry(pos, struct bpu_user, node);/*PRQA S 0497*/ /* Linux Macro */
		if (tmp_user != NULL) {
			spin_lock_irqsave(&tmp_user->spin_lock, user_flags);
			tmp_user->p_run_time /= P_RESET_COEF;
			spin_unlock_irqrestore(&tmp_user->spin_lock, user_flags);
		}
	}

	/* use last reset interval coefficient */
	if (core->p_start_point / NSEC_PER_SEC != 0) {
		tmp_time = tmp_start_point
			- (time_interval((uint64_t)core->p_start_point,
					tmp_start_point) / P_RESET_COEF);

		core->p_start_point = tmp_time;
	} else {
		core->p_start_point = tmp_start_point;
	}

	spin_unlock_irqrestore(&core->spin_lock, flags);

	return 0;
}

int32_t bpu_stat_reset(struct bpu *bpu)
{
	struct list_head *pos, *pos_n;
	struct bpu_core *tmp_core;
	struct bpu_fc_group *tmp_fc_group;
	struct bpu_user *tmp_user;
	unsigned long group_flags;
	unsigned long tmp_user_flags;
	unsigned long bpu_flags;
	unsigned long user_flags;
	int32_t ret;

	/* reset the global slowest time in reset period */
	bpu->slow_task_time = 0u;
	/* reset the bpu core */
	list_for_each_safe(pos, pos_n, &bpu->core_list) {
		tmp_core = (struct bpu_core *)list_entry(pos, struct bpu_core, node);/*PRQA S 0497*/ /* Linux Macro */
		if (tmp_core != NULL) {
			ret = bpu_core_stat_reset(tmp_core);
			if (ret != 0) {
				dev_err(tmp_core->dev, "bpu core[%d] stat reset failed\n", tmp_core->index);
				return ret;
			}
		}
	}

	spin_lock_irqsave(&bpu->spin_lock, bpu_flags);
	spin_lock(&bpu->group_spin_lock);
	/* reset the bpu group p_run_time */
	list_for_each_safe(pos, pos_n, &bpu->group_list) {
		tmp_fc_group = (struct bpu_fc_group *)list_entry(pos, struct bpu_fc_group, node);/*PRQA S 0497*/ /* Linux Macro */
		if (tmp_fc_group != NULL) {
			spin_lock_irqsave(&tmp_fc_group->spin_lock, group_flags);
			tmp_fc_group->p_run_time /= P_RESET_COEF;
			spin_unlock_irqrestore(&tmp_fc_group->spin_lock, group_flags);
		}
	}
	spin_unlock(&bpu->group_spin_lock);

	spin_lock_irqsave(&bpu->user_spin_lock, user_flags);
	/* reset the bpu user p_run_time */
	list_for_each_safe(pos, pos_n, &bpu->user_list) {
		tmp_user = (struct bpu_user *)list_entry(pos, struct bpu_user, node);/*PRQA S 0497*/ /* Linux Macro */
		if (tmp_user != NULL) {
			spin_lock_irqsave(&tmp_user->spin_lock, tmp_user_flags);
			tmp_user->p_run_time /= P_RESET_COEF;
			spin_unlock_irqrestore(&tmp_user->spin_lock, tmp_user_flags);
		}
	}
	spin_unlock_irqrestore(&bpu->user_spin_lock, user_flags);
	spin_unlock_irqrestore(&bpu->spin_lock, bpu_flags);

	return 0;
}

/**
 * bpu_core_ratio() - get process ratio of bpu core
 * @core: bpu core, could not be null
 *
 * Return:
 * * >=0			- bpu core ratio
 */
uint32_t bpu_core_ratio(struct bpu_core *core)
{
	uint64_t tmp_point;
	uint64_t pass_time;
	unsigned long flags;
	uint64_t ratio;

	spin_lock_irqsave(&core->spin_lock, flags);
	tmp_point = bpu_get_time_point();

	pass_time = time_interval((uint64_t)core->p_start_point, tmp_point);
	if (core->p_run_time > pass_time) {
		core->p_run_time = pass_time;
	}

	ratio = core->p_run_time * PERSENT / pass_time;
	spin_unlock_irqrestore(&core->spin_lock, flags);

	core->ratio = (uint32_t)ratio;

	return (uint32_t)ratio;
}
EXPORT_SYMBOL(bpu_core_ratio);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

uint32_t bpu_fc_group_ratio(struct bpu_fc_group *group)
{
	struct list_head *pos, *pos_n;
	uint64_t tmp_point;
	struct bpu_core *tmp_core;
	uint64_t pass_time = 0;
	unsigned long flags;
	uint64_t ratio = 0;

	if (group == NULL) {
		return 0;
	}

	spin_lock_irqsave(&group->spin_lock, flags);
	tmp_point = bpu_get_time_point();

	list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
		tmp_core = (struct bpu_core *)list_entry(pos, struct bpu_core, node);/*PRQA S 0497*/ /* Linux Macro */
		if (bpu_core_is_online(tmp_core)) {
			pass_time += time_interval((uint64_t)tmp_core->p_start_point, tmp_point);
		}
	}

	if (pass_time != 0u) {
		if (group->p_run_time > pass_time) {
			group->p_run_time = pass_time;
		}

		ratio = group->p_run_time * PERSENT / pass_time;
	}
	spin_unlock_irqrestore(&group->spin_lock, flags);

	return (uint32_t)ratio;
}

uint32_t bpu_user_ratio(struct bpu_user *user)
{
	struct list_head *pos, *pos_n;
	uint64_t tmp_point;
	struct bpu_core *tmp_core;
	uint64_t pass_time = 0;
	unsigned long flags;
	uint64_t ratio = 0u;

	if (user == NULL) {
		return 0;
	}

	spin_lock_irqsave(&user->spin_lock, flags);
	tmp_point = bpu_get_time_point();

	list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
		tmp_core = (struct bpu_core *)list_entry(pos, struct bpu_core, node);/*PRQA S 0497*/ /* Linux Macro */
		if (bpu_core_is_online(tmp_core)) {
			pass_time += time_interval((uint64_t)tmp_core->p_start_point, tmp_point);
		}
	}

	if (pass_time != 0u) {
		if (user->p_run_time > pass_time) {
			user->p_run_time = pass_time;
		}

		ratio = user->p_run_time * PERSENT / pass_time;
	}
	spin_unlock_irqrestore(&user->spin_lock, flags);

	return (uint32_t)ratio;
}

/**
 * bpu_ratio() - get process ratio of bpu framework
 * @bpu: bpu framework, could not be null
 *
 * bpu framework process ratio from average of bpu core
 *
 * Return:
 * * >=0			- bpu framework ratio
 */
uint32_t bpu_ratio(struct bpu *bpu)
{
	struct list_head *pos, *pos_n;
	struct bpu_core *tmp_core;
	uint32_t ratio = 0u;
	uint32_t core_num = 0u;

	list_for_each_safe(pos, pos_n, &bpu->core_list) {
		tmp_core = (struct bpu_core *)list_entry(pos, struct bpu_core, node);/*PRQA S 0497*/ /* Linux Macro */
		if (bpu_core_is_online(tmp_core)) {
			ratio += bpu_core_ratio(tmp_core);
			core_num++;
		}
	}
	if (core_num == 0u) {
		return 0;
	}

	bpu->ratio = ratio / core_num;

	return bpu->ratio;
}
EXPORT_SYMBOL(bpu_ratio);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

static int32_t bpu_check_fc_run_time_from_core(struct bpu_core *core,
		struct bpu_fc_run_time *fc_run_time, uint64_t check_point)
{
	struct bpu_fc tmp_bpu_fc;
	struct bpu_fc *match_bpu_fc = NULL;
	uint64_t tmp_start_point;
	struct bpu_user *tmp_user;
	unsigned long flags;
	unsigned long user_flags;
	uint32_t user_id;
	int32_t ret, i;
	int32_t prio_num;

	if (core == NULL) {
		return -EINVAL;
	}

	user_id = (uint32_t)task_pid_nr(current->group_leader);
	prio_num = BPU_CORE_PRIO_NUM(core);
	spin_lock_irqsave(&core->spin_lock, flags);
	for (i = 0; i < (int32_t)prio_num; i++) {
		if (kfifo_is_empty(&core->run_fc_fifo[i])) {
			continue;
		}
		ret = kfifo_peek(&core->run_fc_fifo[i], &tmp_bpu_fc);
		if (ret < 1) {
			continue;
		}

		spin_lock_irqsave(&g_bpu->user_spin_lock, user_flags);
		tmp_user = bpu_get_user(&tmp_bpu_fc, &core->user_list);
		if (tmp_user == NULL) {
			spin_unlock_irqrestore(&g_bpu->user_spin_lock, user_flags);
			continue;
		}

		if ((tmp_bpu_fc.info.core_mask == fc_run_time->core_mask)
				&& (user_id == (uint32_t)(tmp_user->id & USER_MASK))) {
			if ((tmp_bpu_fc.info.id == fc_run_time->id) || (fc_run_time->id == 0)) {
				match_bpu_fc = &tmp_bpu_fc;
				spin_unlock_irqrestore(&g_bpu->user_spin_lock, user_flags);
				break;
			}
		}
		spin_unlock_irqrestore(&g_bpu->user_spin_lock, user_flags);
	}

	if (match_bpu_fc == NULL) {
		fc_run_time->run_time = 0u;
		ret = 0;
	} else {
		if (tmp_bpu_fc.start_point < core->last_done_point) {
			tmp_start_point = (uint64_t)core->last_done_point;
		} else {
			tmp_start_point = (uint64_t)tmp_bpu_fc.start_point;
		}

		fc_run_time->run_time = (uint32_t)(time_interval(tmp_start_point, check_point) / NSEC_PER_USEC);
		ret = 1;
	}
	spin_unlock_irqrestore(&core->spin_lock, flags);

	return ret;
}

int32_t bpu_check_fc_run_time(struct bpu_core *core,
		struct bpu_fc_run_time *fc_run_time)
{
	uint64_t tmp_check_point;
	struct list_head *pos, *pos_n;
	struct bpu_core *tmp_core;
	int32_t ret = 0;

	tmp_check_point = bpu_get_time_point();

	/* if not not specified core, check from all registered bpu cores*/
	if (core == NULL) {
		spin_lock(&g_bpu->spin_lock);
		list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
			tmp_core = (struct bpu_core *)list_entry(pos, struct bpu_core, node);/*PRQA S 0497*/ /* Linux Macro */
			if (tmp_core != NULL) {
				ret = bpu_check_fc_run_time_from_core(tmp_core, fc_run_time, tmp_check_point);
				if (ret > 0) {
					break;
				}
			}
		}
		spin_unlock(&g_bpu->spin_lock);
	} else {
		ret = bpu_check_fc_run_time_from_core(core, fc_run_time, tmp_check_point);
	}

	return ret;
}
EXPORT_SYMBOL(bpu_check_fc_run_time);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/* Get all buffered fc estimate time */
uint64_t bpu_core_bufferd_time(struct bpu_core *core, uint32_t level)
{
	struct bpu_fc tmp_bpu_fc;
	uint64_t tmp_time = 0;
	uint64_t tmp_run_point;
	uint32_t hw_prio, highest_prio;
	unsigned long flags;
	int32_t ret, i;
	int32_t prio_num;

	if (core == NULL) {
		return 0;
	}

	if (level > core->prio_sched->level_num) {
		level = core->prio_sched->level_num - 1u;
	}

	for (i = level; i < (int32_t)core->prio_sched->level_num; i++) {
		tmp_time += core->prio_sched->prios[i].buffered_time;
	}

	prio_num = BPU_CORE_PRIO_NUM(core);
	if (((int32_t)level > prio_num - 1) && (prio_num > 0)) {
		hw_prio = prio_num - 1;
	} else {
		hw_prio = level;
	}

	highest_prio = hw_prio;
	/* Accumulates all high priorities buffered */
	spin_lock_irqsave(&core->spin_lock, flags);
	for (i = hw_prio; i < (int32_t)prio_num; i++) {
		tmp_time += core->buffered_time[i];
		if (!kfifo_is_empty(&core->run_fc_fifo[i])) {
			highest_prio = (uint32_t)i;
		}
	}

	ret = kfifo_peek(&core->run_fc_fifo[highest_prio], &tmp_bpu_fc);
	spin_unlock_irqrestore(&core->spin_lock, flags);
	if (ret > 0) {
		/*
		 * calculate the estimate left running time,
		 * (estimate time - passed time)
		 */
		tmp_run_point = time_interval((uint64_t)tmp_bpu_fc.start_point, bpu_get_time_point()) / NSEC_PER_USEC;
		if (tmp_bpu_fc.info.process_time < tmp_run_point) {
			tmp_run_point = tmp_bpu_fc.info.process_time;
		}

		if (tmp_time > tmp_run_point) {
			tmp_time -= tmp_run_point;
		} else {
			tmp_time = 0;
		}
	}

	return tmp_time;
}
EXPORT_SYMBOL(bpu_core_bufferd_time);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * @NO{S04E02C01I}
 * @brief get the bpu core last done time
 *
 * @param[in] core: bpu core struct pointer
 *
 * @return the time stamp value
 *
 * @data_read None
 * @data_updated None
 * @compatibility None
 *
 * @callgraph
 * @callergraph
 * @design
 */
uint64_t bpu_core_last_done_time(struct bpu_core *core)
{
	if (core == NULL) {
		return 0;
	}

	return core->last_done_point;
}
EXPORT_SYMBOL(bpu_core_last_done_time);

/**
 * @NO{S04E02C01I}
 * @brief get bpu core buffer task estimate time
 *
 * @param[in] core: bpu core struct pointer
 *
 * @return total buffer task estimate time
 *
 * @data_read None
 * @data_updated None
 * @compatibility None
 *
 * @callgraph
 * @callergraph
 * @design
 */
uint64_t bpu_core_pending_task_est_time(struct bpu_core *core)
{
	struct bpu_fc tmp_bpu_fc;
	int32_t i, ret;
	int32_t prio_num;

	if (core == NULL) {
		return 0;
	}

	prio_num = (int32_t)core->hw_io->prio_num;
	for (i = prio_num - 1; i >= 0; i--) {
		ret = kfifo_peek((struct kfifo *) &core->run_fc_fifo[i], &tmp_bpu_fc);/*PRQA S 0478*/ /*PRQA S 0311*/ /* Linux Macro */
		if (ret > 0) {
			break;
		}
	}

	if (i < 0) {
		return 0;
	}

	if (tmp_bpu_fc.info.process_time <= 0) {
		/* use default process time */
		tmp_bpu_fc.info.process_time = SECTOUS;
	}

	return tmp_bpu_fc.info.process_time;
}
EXPORT_SYMBOL(bpu_core_pending_task_est_time);

MODULE_DESCRIPTION("BPU and Cores statistics related realize");
MODULE_AUTHOR("Zhang Guoying <guoying.zhang@horizon.ai>");
MODULE_LICENSE("GPL v2");
