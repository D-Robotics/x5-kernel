/*
 * Copyright (C) 2020 Horizon Robotics
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */
#define pr_fmt(fmt) "bpu: " fmt
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include "bpu_prio.h"
#include "bpu_ctrl.h"

/*
 * Provide bpu priority method, some soc can support
 * limited hardware priority, the other not support 
 * hardware priority.
 * bpu prio try to support software priority.
 */

/* break E1, no need care error return when use the function */
static void bpu_prio_sched(struct bpu_prio *prio)
{
	struct bpu_prio_node *tmp_prio_node;
	struct bpu_core *core;
	unsigned long flags;
	int32_t ret, i;

	core = prio->bind_core;

	for (i = (int32_t)prio->level_num - 1; i >= 0; i--) {
		tmp_prio_node = &prio->prios[i];
		if (tmp_prio_node->left_slice_num > 0) {
			ret = bpu_write_fc_to_core(core,
					&tmp_prio_node->residue_bpu_fc,
					tmp_prio_node->residue_bpu_fc.info.slice_num
					- (uint32_t)tmp_prio_node->left_slice_num);
			if (ret >= 0) {
				tmp_prio_node->left_slice_num -= ret;
			}
		} else {
			if (kfifo_len(&tmp_prio_node->buf_fc_fifo) == 0) {
				if (i == 0) {
					complete(&prio->no_task_comp);
				}
				continue;
			}
			ret = kfifo_get(&tmp_prio_node->buf_fc_fifo,
					&tmp_prio_node->residue_bpu_fc);
			if (ret <= 0) {
				continue;
			}
			ret = bpu_write_fc_to_core(core,
					&tmp_prio_node->residue_bpu_fc, 0);
			if (ret < 0) {
				ret = 0;
			}

			tmp_prio_node->left_slice_num =
				(int32_t)tmp_prio_node->residue_bpu_fc.info.slice_num - ret;
			if (tmp_prio_node->left_slice_num < 0) {
				tmp_prio_node->left_slice_num = 0;
			}
		}
		if (tmp_prio_node->left_slice_num == 0) {
			spin_lock_irqsave(&prio->spin_lock, flags);
			if (tmp_prio_node->buffered_time >= tmp_prio_node->residue_bpu_fc.info.process_time) {
				tmp_prio_node->buffered_time -= tmp_prio_node->residue_bpu_fc.info.process_time;
			} else {
				tmp_prio_node->buffered_time = 0;
			}
			spin_unlock_irqrestore(&prio->spin_lock, flags);
		}
		break;
	}
}

static void bpu_prio_tasklet(unsigned long data)
{
	struct bpu_prio *prio = (struct bpu_prio *)data;
	struct bpu_core *core;
	int32_t running_fc_num = 0;
	int32_t i, prio_num;

	if (prio == NULL) {
		return;
	}

	if (prio->inited == 0u) {
		return;
	}

	core = prio->bind_core;

	if (core == NULL) {
		pr_err("BPU Prio has not bind bpu core\n");
		return;
	}

	if (core->inst.task_buf_limit > 0) {
		prio_num = BPU_CORE_PRIO_NUM(core);
		for(i = 0; i < (int32_t)prio_num; i++) {
			running_fc_num += (int32_t)kfifo_len(&core->run_fc_fifo[i]);
		}

		if (running_fc_num >= core->inst.task_buf_limit) {
			return;
		}
	}

	bpu_prio_sched(prio);
}

/**
 * bpu_prio_init() - init software priority strategy for bpu core
 * @core: bpu core, could not be null
 * @levels: total priority level
 *
 * init software priority strategy which will bind to bpu core
 * to buffer and sort set prio tasks
 *
 * Return:
 * * != NULL	- valid bpu prio
 * * = NULL		- error
 */
struct bpu_prio *bpu_prio_init(struct bpu_core *core, uint32_t levels)
{
	struct bpu_prio *prio;
	uint32_t i, j;

	if (core == NULL) {
		pr_err("BPU Prio init for invalid BPU Core\n");
		return NULL;
	}

	prio = (struct bpu_prio *)kzalloc(sizeof(struct bpu_prio), GFP_KERNEL);
	if (prio == NULL) {
		dev_err(core->dev, "Create bpu prio failed!!\n");
		return NULL;
	}

	prio->bind_core = core;
	prio->level_num = levels;

	prio->prios = (struct bpu_prio_node *)kzalloc(sizeof(struct bpu_prio_node)
				* prio->level_num, GFP_KERNEL);
	if (prio->prios == NULL) {
		kfree((void *)prio);
		dev_err(core->dev, "Create bpu prio container failed!!\n");
		return NULL;
	}

	mutex_init(&prio->mutex_lock);/*PRQA S 3334*/ /* Linux Macro */
	spin_lock_init(&prio->spin_lock);/*PRQA S 3334*/ /* Linux Macro */
	init_completion(&prio->no_task_comp);

	for (i = 0u; i < prio->level_num; i++) {
		prio->prios[i].bpu_fc_fifo_buf =
			vmalloc(roundup_pow_of_two(MAX_HW_FIFO_CAP / (i + 1u))/*PRQA S 1891*/ /*PRQA S 4491*/ /* Linux Macro */
					* sizeof(struct bpu_fc));
		if (prio->prios[i].bpu_fc_fifo_buf == NULL) {
			for (j = 0; j < i; j++) {
				vfree(prio->prios[i].bpu_fc_fifo_buf);
			}

			kfree((void *)prio->prios);
			kfree((void *)prio);
			dev_err(core->dev, "Create bpu prio container buf failed!!\n");
			return NULL;
		}

		kfifo_init(&prio->prios[i].buf_fc_fifo,/*PRQA S 4461*/ /*PRQA S 4491*/ /*PRQA S 1891*/ /* Linux Macro */
				prio->prios[i].bpu_fc_fifo_buf,
				roundup_pow_of_two(MAX_HW_FIFO_CAP / (i + 1u))
				* sizeof(struct bpu_fc));

		prio->prios[i].level = (uint32_t)i;
		prio->prios[i].left_slice_num = 0;
	}

	tasklet_init(&prio->tasklet, bpu_prio_tasklet, (unsigned long)prio);
	prio->plug_in = 1;
	prio->inited = 1;

	return prio;
}
EXPORT_SYMBOL(bpu_prio_init);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_prio_exit() - exit software priority strategy
 * @prio: bpu prio
 *
 * when bpu priority not need, should be exited
 */
/* break E1, no need care error return when use the function */
void bpu_prio_exit(struct bpu_prio *prio)
{
	struct bpu_fc tmp_bpu_fc;
	uint32_t i;
	int32_t ret;

	if(prio == NULL) {
		return;
	}

	mutex_lock(&prio->mutex_lock);
	tasklet_kill(&prio->tasklet);

	for (i = 0u; i < prio->level_num; i++) {
		while (!kfifo_is_empty(&prio->prios[i].buf_fc_fifo)) {
			ret = kfifo_get(&prio->prios[i].buf_fc_fifo, &tmp_bpu_fc);
			if (ret < 1) {
				continue;
			}
			bpu_fc_clear(&tmp_bpu_fc);
		}

		if (prio->prios[i].left_slice_num > 0) {
			bpu_fc_clear(&prio->prios[i].residue_bpu_fc);
			prio->prios[i].left_slice_num = 0;
		}

		vfree(prio->prios[i].bpu_fc_fifo_buf);
	}

	prio->plug_in = 0;
	prio->inited = 0;
	kfree((void *)prio->prios);
	prio->prios = NULL;
	mutex_unlock(&prio->mutex_lock);
	kfree((void *)prio);
}
EXPORT_SYMBOL(bpu_prio_exit);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_prio_in() - push bpu fc task to software priority strategy
 * @prio: bpu prio, could not be null
 * @bpu_fc: bpu fc task in kernel
 *
 * push the bpu fc task to software priority strategy buffer
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_prio_in(struct bpu_prio *prio, const struct bpu_fc *bpu_fc)
{
	unsigned long flags;
	uint32_t level;
	int32_t ret;

	if (prio == NULL) {
		return -EINVAL;
	}

	level = bpu_fc->info.priority;

	if (level > prio->level_num) {
		level = prio->level_num - 1u;
	}

	ret = mutex_lock_interruptible(&prio->mutex_lock);
	if (ret < 0) {
		return ret;
	}
	if (prio->inited == 0u) {
		mutex_unlock(&prio->mutex_lock);
		return -EINVAL;
	}

	if (kfifo_is_full(&prio->prios[level].buf_fc_fifo)) {
		mutex_unlock(&prio->mutex_lock);
		return -EBUSY;
	}

	spin_lock_irqsave(&prio->spin_lock, flags);
	prio->prios[level].buffered_time += bpu_fc->info.process_time;
	spin_unlock_irqrestore(&prio->spin_lock, flags);
	ret = kfifo_in(&prio->prios[level].buf_fc_fifo, bpu_fc, 1);/*PRQA S 4461*/ /* Linux Macro */
	if (ret < 1) {
		spin_lock_irqsave(&prio->spin_lock, flags);
		if (prio->prios[level].buffered_time >= bpu_fc->info.process_time) {
			prio->prios[level].buffered_time -= bpu_fc->info.process_time;
		} else {
			prio->prios[level].buffered_time = 0;
		}
		spin_unlock_irqrestore(&prio->spin_lock, flags);
		mutex_unlock(&prio->mutex_lock);
		return -EBUSY;
	}

	bpu_prio_trig_out(prio);
	mutex_unlock(&prio->mutex_lock);

	return ret;
}
EXPORT_SYMBOL(bpu_prio_in);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_prio_trig_out() - trigger software priority strategy to dispatch task
 * @prio: bpu prio
 *
 * when triggered, priority strategy will choose optimal
 * task and try to set to bpu core hardware.
 */
/* break E1, no need care error return when use the function */
void bpu_prio_trig_out(struct bpu_prio *prio)
{
	if (prio == NULL) {
		return;
	}

	if (prio->inited == 0u) {
		return;
	}

	tasklet_schedule(&prio->tasklet);
}
EXPORT_SYMBOL(bpu_prio_trig_out);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

uint32_t bpu_prio_left_task_num(struct bpu_prio *prio)
{
	uint32_t left_num = 0u;
	uint32_t slice_task_left_num = 0u;
	int32_t ret;
	uint32_t i;

	if (prio == NULL) {
		return 0;
	}

	if (prio->inited == 0u) {
		return 0;
	}

	ret = mutex_lock_interruptible(&prio->mutex_lock);
	if (ret < 0) {
		return 0;
	}
	for (i = 0u; i < prio->level_num; i++) {
		left_num += kfifo_len(&prio->prios[i].buf_fc_fifo);
		if (prio->prios[i].left_slice_num > 0) {
			slice_task_left_num += 1;
		}
	}
	mutex_unlock(&prio->mutex_lock);

	return left_num + slice_task_left_num;
}

bool bpu_prio_is_plug_in(const struct bpu_prio *prio)
{
	if (prio == NULL) {
		return false;/*PRQA S 1294*/ /* Linux bool define */
	}

	if (prio->inited == 0u) {
		return false;/*PRQA S 1294*/ /* Linux bool define */
	}

	if (prio->plug_in > 0u) {
		return true;/*PRQA S 1294*/ /* Linux bool define */
	}

	return false;/*PRQA S 1294*/ /* Linux bool define */
}

/**
 * bpu_prio_plug_in() - make software priority strategy inline
 * @prio: bpu prio, could not be null
 *
 * when prio init, default prio inline
 */
/* break E1, no need care error return when use the function */
void bpu_prio_plug_in(struct bpu_prio *prio)
{
	if (prio == NULL) {
		return;
	}

	if (prio->inited == 0u) {
		return;
	}

	prio->plug_in = 1u;
}
EXPORT_SYMBOL(bpu_prio_plug_in);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_prio_plug_out() - make software priority strategy offline
 * @prio: bpu prio, could not be null
 *
 * when make prio offline, bpu task can't push to priority strategy
 */
/* break E1, no need care error return when use the function */
void bpu_prio_plug_out(struct bpu_prio *prio)
{
	if (prio == NULL) {
		return;
	}

	if (prio->inited == 0u) {
		return;
	}

	prio->plug_in = 0u;
}
EXPORT_SYMBOL(bpu_prio_plug_out);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */

/**
 * bpu_prio_wait_empty() - try to wait all task in prio to be processed
 * @prio: bpu prio, could not be null
 * @jiffes: wait timeout jiffes
 *
 * to wait all task be processed by bpu core in jiffes
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_prio_wait_empty(struct bpu_prio *prio, int32_t jiffes)
{
	uint32_t left_num;

	if (prio == NULL) {
		return 0;
	}

	if (prio->inited == 0u) {
		return 0;
	}

	left_num = bpu_prio_left_task_num(prio);
	if (left_num > 0u) {
		if(wait_for_completion_timeout(&prio->no_task_comp, (uint64_t)jiffes) == 0u) {
			/* timeout, may need wait again*/
			return -EAGAIN;
		}
	}

	return 0;
}
EXPORT_SYMBOL(bpu_prio_wait_empty);/*PRQA S 0307*/ /*PRQA S 0779*/ /* Linux Macro */
