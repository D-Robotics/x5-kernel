/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/

#ifndef HOBOT_VPU_PRIO_H
#define HOBOT_VPU_PRIO_H

#include <linux/kfifo.h>
#include "osal.h"
// #include "hobot_vpu_reg.h"

#define MAX_PRIO_FIFO_LEN 32U

typedef enum {
	UNRUN,
	DONE,
} VPU_COMMAND_STATUS;

typedef enum mc_video_cmd_prio {
	PRIO_0,
	PRIO_1,
	PRIO_2,
	PRIO_3,
	PRIO_4,
	PRIO_5,
	PRIO_6,
	PRIO_7,
	PRIO_8,
	PRIO_9,
	PRIO_10,
	PRIO_11,
	PRIO_12,
	PRIO_13,
	PRIO_14,
	PRIO_15,
	PRIO_16,
	PRIO_17,
	PRIO_18,
	PRIO_19,
	PRIO_20,
	PRIO_21,
	PRIO_22,
	PRIO_23,
	PRIO_24,
	PRIO_25,
	PRIO_26,
	PRIO_27,
	PRIO_28,
	PRIO_29,
	PRIO_30,
	PRIO_31,
	MAX_PRIO_32,
} mc_video_cmd_prio_t;

typedef struct hb_vpu_filp_list {
	struct file *filp;
	osal_list_head_t list;
} hb_vpu_filp_list_t;

typedef struct hb_vpu_prio_cmd_data {
	uint32_t priority;
	uint32_t process_time;
	int32_t status;
	uint32_t inst_idx;
	uint32_t core_idx;
	int32_t codecMode;
	struct file *filp;
	uint32_t num;
} hb_vpu_prio_cmd_data_t;

typedef struct hb_vpu_prio_enc_data {
	uint32_t priority;
	uint32_t inst_idx;
	struct file *filp;
	uint32_t core_idx;
	int32_t codecMode;
} hb_vpu_prio_enc_data_t;

typedef struct hb_vpu_prio_queue {
	osal_fifo_t prio_fifo[MAX_PRIO_32]; /*PRQA S ALL*/
	osal_list_head_t vpq_filp_head;
	osal_spinlock_t vpu_prio_lock;
	osal_mutex_t vpu_prio_mutex;
	uint32_t queue_len;
	struct workqueue_struct *prio_workqueue;
	struct work_struct prio_work;
	osal_waitqueue_t cmd_wait_q[MAX_PRIO_32];
	void *dev;
	uint32_t prio_done_flags[MAX_PRIO_32][MAX_PRIO_FIFO_LEN];
	uint32_t num;
} hb_vpu_prio_queue_t;

int32_t vpu_prio_init(void *dev);
int32_t vpu_prio_set_enc_dec_pic(struct file *filp, void *dev, u_long arg);
void vpu_prio_deinit(void *dev);
int32_t vpu_prio_set_command_to_fw(hb_vpu_prio_queue_t *vpq);
int32_t check_prio_flag(void *dev);
#endif
