/*
 * Copyright (C) 2019 Horizon Robotics
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */
#define pr_fmt(fmt) "bpu: " fmt
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include "bpu.h"
#include "bpu_core.h"
#include "bpu_ctrl.h"

#define DEFAULT_SCHED_SEED (2u)
#define STATE_SHIFT		(32u)

static uint8_t bpu_recovery_work_inited = 0u;
static struct workqueue_struct *bpu_recovery_workqueue;
static struct work_struct bpu_recovery_work;
static struct bpu_core *recovery_core;

/**
 * bpu_sched_seed_update() - Dynamic adjustment BPU schedule seed
 *
 * BPU schedule seed use for ratio or other schedule strategy
 */
/* break E1, no need care error return when use the function */
void bpu_sched_seed_update(void)
{
	struct bpu_core *tmp_core;
	struct list_head *pos, *pos_n;
	int32_t run_fc_num = 0;
	int32_t prio_num, i;

	list_for_each_safe(pos, pos_n, &g_bpu->core_list) {
		tmp_core = (struct bpu_core *)(void *)pos;
		if (tmp_core != NULL) {
			prio_num = BPU_CORE_PRIO_NUM(tmp_core);
			for (i = 0; i < prio_num; i++) {
				run_fc_num += (int32_t)kfifo_len(&tmp_core->run_fc_fifo[i]);
			}
		}
	}

	if (run_fc_num > 0) {
		if (g_bpu->sched_seed < (uint32_t)HZ) {
			g_bpu->sched_seed++;
		}
		if (g_bpu->sched_seed > (uint32_t)HZ) {
			g_bpu->sched_seed = HZ;
		}
	} else {
		g_bpu->sched_seed = DEFAULT_SCHED_SEED;
	}
}
EXPORT_SYMBOL(bpu_sched_seed_update);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

static void bpu_recovery_work_func(struct work_struct *work)
{
	struct bpu_core *core;
	unsigned long flags;
	int32_t ret;

	core = recovery_core;
	if (recovery_core == NULL) {
		return;
	}

	mutex_lock(&core->mutex_lock);
	if (core->hw_enabled <= 0u) {
		mutex_unlock(&core->mutex_lock);
		return;
	}

	(void)bpu_core_pend_on(core);

	spin_lock_irqsave(&core->hw_io_spin_lock, flags);
	if (core->hw_io != NULL) {
		if (core->hw_io->ops.reset!= NULL) {
			spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
			(void)core->hw_io->ops.reset(&core->inst);

			ret = bpu_core_process_recover(core);
			if (ret != 0) {
				dev_err(core->dev, "BPU core%d recover failed\n", core->index);
			}
		} else {
			spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
		}
	} else {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
	}

	core->last_err_point = bpu_get_time_point();

	(void)bpu_core_pend_off(core);

	spin_lock_irqsave(&core->hw_io_spin_lock, flags);
	if (core->hw_io != NULL) {
		if (core->hw_io->ops.cmd!= NULL) {
			(void)core->hw_io->ops.cmd(&core->inst, (uint32_t)UPDATE_STATE);
		}
	}
	spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);

	mutex_unlock(&core->mutex_lock);
}

int32_t bpu_sched_recover_core(struct bpu_core *core)
{
	unsigned long flags;

	if (core == NULL) {
		return -ENODEV;
	}

	if ((bpu_get_time_point() < core->last_err_point + NSEC_PER_SEC * 2)
			&& (core->last_err_point != 0u)) {
		core->last_err_point = bpu_get_time_point();
		return -EINVAL;
	}

	if (core->hw_enabled > 0u) {
		spin_lock_irqsave(&core->hw_io_spin_lock, flags);
		if (core->hw_io != NULL) {
			if (core->hw_io->ops.cmd != NULL) {
				(void)core->hw_io->ops.cmd(&core->inst, (uint32_t)TERMINATE_RUN);
			}
		}
		spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
	}

	recovery_core = core;
	spin_lock_irqsave(&g_bpu->sched_spin_lock, flags);
	if (bpu_recovery_work_inited > 0) {
		queue_work(bpu_recovery_workqueue, &bpu_recovery_work);
	}
	spin_unlock_irqrestore(&g_bpu->sched_spin_lock, flags);

	return 0;
}
EXPORT_SYMBOL(bpu_sched_recover_core);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/* break E1, no need care error return when use the function */
static void bpu_sched_check_to_core(const struct bpu *bpu)
{
	struct bpu_core *tmp_core;
	struct list_head *pos, *pos_n;
	uint64_t tmp_done_hw_id;
	unsigned long flags;
	int32_t ret;

	if (bpu == NULL) {
		return;
	}

	list_for_each_safe(pos, pos_n, &bpu->core_list) {
		tmp_core = (struct bpu_core *)(void *)pos;
		if (tmp_core != NULL) {

			if (tmp_core->hw_enabled == 0u) {
				continue;
			}

			spin_lock_irqsave(&tmp_core->hw_io_spin_lock, flags);
			if (tmp_core->hw_io == NULL) {
				spin_unlock_irqrestore(&tmp_core->hw_io_spin_lock, flags);
				continue;
			}

			if (tmp_core->hw_io->ops.cmd != NULL) {
				if (tmp_core->hw_io->ops.cmd(&tmp_core->inst, (uint32_t)WORK_STATE) > 0) {
					(void)tmp_core->hw_io->ops.cmd(&tmp_core->inst, (uint32_t)UPDATE_STATE);
					spin_unlock_irqrestore(&tmp_core->hw_io_spin_lock, flags);
					continue;
				}
			}
			spin_unlock_irqrestore(&tmp_core->hw_io_spin_lock, flags);

			if (bpu_sched_recover_core(tmp_core) != 0) {
				/* when found bpu hung, report to user */
				tmp_done_hw_id = ((uint64_t)(-1) << STATE_SHIFT);
				if (kfifo_is_full(&tmp_core->done_hw_ids)) {
					kfifo_skip(&tmp_core->done_hw_ids);
				}
				kfifo_put(&tmp_core->done_hw_ids, tmp_done_hw_id);
				tasklet_schedule(&tmp_core->tasklet);

				if (g_bpu->extra_ops != NULL) {
					if (g_bpu->extra_ops->report != NULL) {
						ret = g_bpu->extra_ops->report(tmp_core, 0, (uint64_t)-1, 2);
						if (ret < 0) {
							dev_err(tmp_core->dev, "bpu core report fusa info failed\n");
						}
					}
				}
			}
		}
	}
}

static void bpu_sched_worker(struct timer_list *t)
{
	struct bpu *bpu = from_timer(bpu, t, sched_timer);/*PRQA S 0497*/ /* Linux Macro */
	uint32_t tmp_reset_count;
	int32_t ret;

	if (bpu == NULL) {
		pr_err("No bpu to sched!\n");
		return;
	}

	bpu->stat_reset_count++;
	tmp_reset_count = (uint32_t)HZ / bpu->sched_seed;
	if (bpu->stat_reset_count >= tmp_reset_count) {
		ret = bpu_stat_reset(bpu);
		if (ret != 0) {
			pr_err("Bpu stat reset failed!\n");
		}

		/* judge wether bpu core dead, if dead, reset and recovery */
		bpu_sched_check_to_core(bpu);

		bpu->stat_reset_count = 0;
	}

	bpu->sched_timer.expires = jiffies + bpu->sched_seed;
	add_timer(&bpu->sched_timer);

	if (bpu->sched_seed < (uint32_t)HZ) {
		bpu->sched_seed++;
	}
	if (bpu->sched_seed > (uint32_t)HZ) {
		bpu->sched_seed = HZ;
	}
}

int32_t bpu_sched_start(struct bpu *bpu)
{
	unsigned long flags;

	if (bpu == NULL) {
		return -EINVAL;
	}

	spin_lock_irqsave(&bpu->sched_spin_lock, flags);
	timer_setup(&bpu->sched_timer, bpu_sched_worker, 0);
	bpu->sched_seed = HZ;
	bpu->stat_reset_count = 0;

	bpu->sched_timer.expires = jiffies + DEFAULT_SCHED_SEED;
	add_timer(&bpu->sched_timer);
	bpu_recovery_workqueue = alloc_workqueue("bpu_recovery_workqueue", 0, 0);
	INIT_WORK(&bpu_recovery_work, bpu_recovery_work_func);
	bpu_recovery_work_inited = 1u;
	spin_unlock_irqrestore(&bpu->sched_spin_lock, flags);

	return 0;
}

/* break E1, no need care error return when use the function */
void bpu_sched_stop(struct bpu *bpu)
{
	int32_t ret;
	unsigned long flags;

	if (bpu == NULL) {
		return;
	}

	spin_lock_irqsave(&bpu->sched_spin_lock, flags);
	bpu_recovery_work_inited = 0u;
	ret = del_timer_sync(&bpu->sched_timer);
	spin_unlock_irqrestore(&bpu->sched_spin_lock, flags);
	destroy_workqueue(bpu_recovery_workqueue);
	if (ret == 0) {
		pr_debug("del no sched timer\n");/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */
	}
}

MODULE_DESCRIPTION("BPU and Cores sched for process");
MODULE_AUTHOR("Zhang Guoying <guoying.zhang@horizon.ai>");
MODULE_LICENSE("GPL v2");
