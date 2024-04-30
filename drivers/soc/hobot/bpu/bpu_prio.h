/*
 * Copyright (C) 2020 Horizon Robotics
 *
 * Zhang Guoying <guoying.zhang horizon.ai>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */
#ifndef __BPU_PRIO_H__
#define __BPU_PRIO_H__ /*PRQA S 0603*/ /* Linux header define style */
#include "bpu_core.h"

struct bpu_prio_node {
	uint32_t level;
	/* store raw fc data of the priority */
	DECLARE_KFIFO_PTR(buf_fc_fifo, struct bpu_fc);/*PRQA S 1061*/ /*PRQA S 1062*/ /* Linux Macro */
	void *bpu_fc_fifo_buf;
	/*
	 * left part fc set to hw fifo, residue_bpu_fc to
	 * store the head bpu_fc raw fc data part, if
	 * left_slice_num equal 0, mean the bpu_fc has
	 * been set to hw bpu fifo down, and the bpu_fc
	 * can to core run_fc_fifo to wait process done
	 * if left_slice_num > 0, next sched need
	 * continue set the left part.
	 */
	struct bpu_fc residue_bpu_fc;
	int32_t left_slice_num;
	uint64_t buffered_time;
};

struct bpu_prio {
	struct bpu_core *bind_core;
	/* to find the high task and set to hw_tail */
	struct tasklet_struct tasklet;
	struct mutex mutex_lock;
	spinlock_t spin_lock;
	/* prios emptyp completion */
	struct completion no_task_comp;
	uint16_t inited;
	uint16_t plug_in;
	/* array to store support prios */
	struct bpu_prio_node *prios;
	uint32_t level_num;
};

struct bpu_prio *bpu_prio_init(struct bpu_core *core, uint32_t levels);
void bpu_prio_exit(struct bpu_prio *prio);
int32_t bpu_prio_in(struct bpu_prio *prio, const struct bpu_fc *bpu_fc);
void bpu_prio_trig_out(struct bpu_prio *prio);
bool bpu_prio_is_plug_in(const struct bpu_prio *prio);
void bpu_prio_plug_in(struct bpu_prio *prio);
void bpu_prio_plug_out(struct bpu_prio *prio);
int32_t bpu_prio_wait_empty(struct bpu_prio *prio, int32_t jiffes);
uint32_t bpu_prio_left_task_num(struct bpu_prio *prio);

#endif
