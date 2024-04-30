/*
 * Copyright (C) 2019 Horizon Robotics
 *
 * Zhang Guoying <guoying.zhang horizon.ai>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */
#ifndef __BPU_CORE_H__
#define __BPU_CORE_H__ /*PRQA S 0603*/ /* Linux header define style */
#include <linux/interrupt.h>
#include <hobot_ion_iommu.h>
#if defined(CONFIG_PM_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
#include <linux/devfreq.h>
#endif

#include "bpu.h"
#include "bpu_prio.h"

#define DONE_FC_ID_DEPTH	(8)

enum bpu_core_cmd {
	/* get if bpu busy, 1:busy; 0:free*/
	BUSY_STATE = 0,
	/*
	 * get bpu work or not_work(dead or hung)
	 * return 1: working; 0: not work
	 */
	WORK_STATE,
	/* To update some val for state check */
	UPDATE_STATE,
	/* To get bpu core type(as pe type)*/
	TYPE_STATE,
	/* Make bpu hw process terminated */
	TERMINATE_RUN,
	STATUS_CMD_MAX,
};

enum core_pe_type {
    CORE_TYPE_UNKNOWN,
    CORE_TYPE_4PE,
    CORE_TYPE_1PE,
    CORE_TYPE_2PE,
    CORE_TYPE_ANY,
    CORE_TYPE_INVALID,
};

#if defined(CONFIG_PM_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
struct bpu_core_dvfs {
	struct devfreq *devfreq;
	struct thermal_cooling_device *cooling;
	struct devfreq_dev_profile profile;
	uint64_t rate;
	uint64_t volt;
	/* store freq level num */
	uint32_t level_num;
};
#endif

/**
 * struct core - bpu core structure
 * @node: node for bpu framework record list
 * @host: bind bpu framework
 * @miscdev: for misc device node
 * @open_counter: record bpu core dev file open number
 * @hw_id_counter: for hw id creater which in bpu_fc
 * @pend_flag: use for bpu core pending
 * @prio_sched: bind the bpu software prioity policy
 * @fc_buf_limit: limit the fc number which write to
 * 				  hw fifo at the same time, which can
 * 				  influence prio sched max time.
 * @irq: for interrupt
 * @base: bpu core register map base
 * @reserved_base: some platform need other place to
 * 				   ctrl bpu, like pmu
 * @done_hw_id: use to store bpu last done id which not
 * 				report, if reported, the value set to 0
 * 				HIGH 32bit store error status
 * @fc_base: which store bpu read fc fifo base alloc when
 * 			 core enable and free when core disable,
 * 			 number for diff level fc fifo.
 * @fc_base_addr: phy address for fc_base
 * @run_fc_fifo: record bpu fc which running by bpu core
 * @done_fc_fifo: record bpu fc which has been done by bpu core
 * @mutex_lock: lock to protect bpu core
 * @spin_lock: lock to protect bpu core
 * @user_list: to store bpu use which direct use the bpu core
 * @hw_ops: to bind callbacks for bpu hardware operation
 * @index: the bpu core index in bpu framework
 * @dvfs: for linxu dvfs mechanism
 * @running_task_num: record the core running fc number
 * @no_task_comp: for not task running on bpu core
 * @power_level:  > 0; auto change by governor;
 * 				  <=0: manual levle, 0 is highest
 * @hotplug: if hotplug = 1, powered off core's
 * 			 task will sched to other core
 * @hw_enabled: identify if bpu core hardware enabled
 * @tasklet: for interrupt bottom-half
 * @last_done_point: for statistics
 * @p_start_point: for statistics
 * @p_run_time: to record bpu core fcs process time in statistical period
 * @ratio: record bpu core running ratio
 * @reserved: for reserved function
 *
 * In bpu driver, a struct bpu_core represents unique BPU core, the
 * bpu core operations need use struct bpu_core
 */
struct bpu_core {
	struct list_head node;
	struct device *dev;
	struct bpu *host;
	bpu_core_hw_inst_t inst;
	bpu_hw_io_t *hw_io;
	int32_t index;
	uint64_t reserved[8];

	spinlock_t hw_io_spin_lock;
	struct miscdevice miscdev;
	atomic_t open_counter;

	atomic_t hw_id_counter[MAX_HW_FIFO_NUM];

	atomic_t pend_flag;

	struct bpu_prio *prio_sched;

	uint32_t irq;

	DECLARE_KFIFO(done_hw_ids, uint64_t, DONE_FC_ID_DEPTH);

	struct regulator *regulator;
	struct clk *aclk;
	struct clk *mclk;
	struct clk *pe0clk;
	struct clk *pe1clk;
	struct clk *pe2clk;
	struct clk *pe3clk;
	struct reset_control *rst;

	uint64_t buffered_time[MAX_HW_FIFO_NUM];
	uint32_t fifo_created;
	DECLARE_KFIFO_PTR(run_fc_fifo[MAX_HW_FIFO_NUM], struct bpu_fc);/*PRQA S 1061*/ /*PRQA S 1062*/ /* Linux Macro */
	DECLARE_KFIFO_PTR(done_fc_fifo, struct bpu_fc);/*PRQA S 1061*/ /* Linux Macro */

	struct mutex mutex_lock;
	spinlock_t spin_lock;

	struct list_head user_list;

	uint32_t power_delay;

#if defined(CONFIG_PM_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
	struct bpu_core_dvfs *dvfs;
#else
	void *dvfs;
#endif
	int32_t running_task_num;
	struct completion no_task_comp;
	int32_t power_level;

	uint16_t hotplug;
	uint16_t hw_enabled;

	struct tasklet_struct tasklet;
	ktime_t last_done_point;
	ktime_t p_start_point;

	uint64_t p_run_time;
	uint32_t ratio;

	uint64_t last_err_point;
};

static inline int32_t BPU_CORE_PRIO_NUM(struct bpu_core *core)
{
	int32_t prio_num;
	unsigned long in_flags;

	if (core == NULL) {
		return 0;
	}

	spin_lock_irqsave(&core->hw_io_spin_lock, in_flags);
	if (core->hw_io == NULL) {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, in_flags);
		return 0;
	}

	prio_num = (int32_t)core->hw_io->prio_num;
	spin_unlock_irqrestore(&core->hw_io_spin_lock, in_flags);

	return prio_num;
}

static inline uint32_t BPU_CORE_TASK_ID_MAX(struct bpu_core *core)
{
	uint32_t task_id_max;
	unsigned long in_flags;

	if (core == NULL) {
		return 0xFFFFFFFFu;
	}

	spin_lock_irqsave(&core->hw_io_spin_lock, in_flags);
	if (core->hw_io == NULL) {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, in_flags);
		return 0xFFFFFFFFu;
	}

	task_id_max = core->hw_io->task_id_max;
	spin_unlock_irqrestore(&core->hw_io_spin_lock, in_flags);

	return task_id_max;
}


static inline uint32_t BPU_CORE_TASK_PRIO_ID(struct bpu_core *core, uint32_t prio, uint32_t id)
{
	uint32_t result, id_mask, prio_mask;
	unsigned long in_flags;

	if (core == NULL) {
		return 0;
	}

	spin_lock_irqsave(&core->hw_io_spin_lock, in_flags);
	if (core->hw_io == NULL) {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, in_flags);
		return 0;
	}

	id_mask = (0x1 << core->hw_io->prio_offset) - 1;
	prio_mask = core->hw_io->prio_num - 1;
	result = ((id) & id_mask) | ((prio & prio_mask) << core->hw_io->prio_offset);
	spin_unlock_irqrestore(&core->hw_io_spin_lock, in_flags);

	return result;
}

static inline uint32_t BPU_CORE_TASK_ID(struct bpu_core *core, uint32_t id)
{
	uint32_t result, id_mask;
	unsigned long in_flags;

	if (core == NULL) {
		return 0;
	}

	spin_lock_irqsave(&core->hw_io_spin_lock, in_flags);
	if (core->hw_io == NULL) {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, in_flags);
		return 0;
	}

	id_mask = (0x1 << core->hw_io->prio_offset) - 1;
	result = id & id_mask;
	spin_unlock_irqrestore(&core->hw_io_spin_lock, in_flags);

	return result;
}

static inline uint32_t BPU_CORE_TASK_PRIO(struct bpu_core *core, uint32_t id)
{
	uint32_t result;
	unsigned long in_flags;

	if (core == NULL) {
		return 0;
	}

	spin_lock_irqsave(&core->hw_io_spin_lock, in_flags);
	if (core->hw_io == NULL) {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, in_flags);
		return 0;
	}

	result = id >> core->hw_io->prio_offset;
	spin_unlock_irqrestore(&core->hw_io_spin_lock, in_flags);

	return result;
}

void bpu_core_push_done_id(struct bpu_core *core, uint64_t id);
int32_t bpu_core_hw_io_resource_alloc(struct bpu_core *core);
void bpu_core_hw_io_resource_free(struct bpu_core *core);
#endif
