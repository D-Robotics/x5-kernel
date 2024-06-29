/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/

#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>
#include "hobot_vpu_utils.h"
#include "hobot_vpu_prio.h"
#include "hobot_vpu_reg.h"
#include "hobot_vpu_debug.h"
#include "osal.h"
#include <linux/wait.h>

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_prio_check_and_run(hb_vpu_prio_queue_t *vpq, hb_vpu_prio_cmd_data_t *prio_cmd, uint32_t prio)
{
	hb_vpu_filp_list_t *vpq_filp, *n;
	uint32_t in_list = 0;
	hb_vpu_dev_t *dev;

	if (vpq == NULL || prio_cmd == NULL || vpq->dev == NULL) {
		VPU_ERR("Invalid prio queue or dev.\n");
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev = vpq->dev;

	if (osal_fifo_out_spinlocked(&(vpq->prio_fifo[prio]), prio_cmd,
			(uint32_t)sizeof(hb_vpu_prio_cmd_data_t), &vpq->vpu_prio_lock) > 0U) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		osal_list_for_each_entry_safe(vpq_filp, n, &vpq->vpq_filp_head, list) {
			if (vpq_filp->filp == prio_cmd->filp) {
				in_list = 1;
				break;
			}
		}
		if (in_list != 0U) {
			osal_spin_lock(&vpq->vpu_prio_lock);
			vpq->prio_done_flags[prio_cmd->priority][prio_cmd->inst_idx] = 1;
			osal_spin_unlock(&vpq->vpu_prio_lock);
			VPU_INFO_DEV(dev->device, "wakeup prio:%d, inst:%d\n",
				prio_cmd->priority, prio_cmd->inst_idx);
			osal_wake_up(&vpq->cmd_wait_q[prio_cmd->priority]);
		} else {
			VPU_INFO_DEV(dev->device, "The command host process has quit, priority=%d, filp=%p.\n",
				prio_cmd->priority, prio_cmd->filp);
		}
	} else {
		VPU_INFO_DEV(dev->device, "Kfifo out prio: %d command failed.\n", prio);
	}

	return 0;
}

int32_t vpu_prio_set_command_to_fw(hb_vpu_prio_queue_t *vpq)
{
	int32_t i;
	int32_t ret = 0;
	hb_vpu_dev_t *dev;
	hb_vpu_prio_cmd_data_t prio_cmd;

	if (vpq == NULL || vpq->dev == NULL) {
		VPU_ERR("Invalid prio queue.\n");
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev = vpq->dev;

	for (i = (int32_t)PRIO_31; i >= (int32_t)PRIO_0; i--) {
		if (osal_fifo_is_empty(&vpq->prio_fifo[i]) != 0) {
			continue;
		} else {
			ret = vpu_prio_check_and_run(vpq, &prio_cmd, (uint32_t)i);
			if (ret != 0) {
				VPU_INFO_DEV(dev->device, "vpu_prio_check_and_run failed, ret=%d.\n", ret);
			}
			break;
		}
	}

	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
int32_t vpu_prio_set_enc_dec_pic(struct file *filp, void *vpu, u_long arg)
{
	int32_t ret = 0;
	hb_vpu_prio_cmd_data_t prio_cmd = {0, };
	hb_vpu_prio_enc_data_t enc_cmd = {0, };
	hb_vpu_prio_queue_t *vpq = NULL;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)vpu;

	if (dev == NULL || filp == NULL) {
		VPU_ERR("Invalid vpu device(%p) or filp(%p)\n", dev, filp);
		return -EINVAL;
	}
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&enc_cmd,
			(const void __user*)arg,
			sizeof(hb_vpu_prio_enc_data_t));
	if (ret != 0 || enc_cmd.inst_idx >= MAX_NUM_VPU_INSTANCE || enc_cmd.priority >= (uint32_t)MAX_PRIO_32) {
		VPU_ERR_DEV(dev->device, "Failed to copy from user or invalid params from user.\n");
		return ret;
	}

	if (dev->first_flag++ == 0 || dev->prio_flag == 0) {
		return 0;
	} else {
		prio_cmd.filp = filp;
		prio_cmd.core_idx = enc_cmd.core_idx;
		prio_cmd.codecMode = enc_cmd.codecMode;
		prio_cmd.inst_idx = enc_cmd.inst_idx;
		prio_cmd.priority = enc_cmd.priority;
		vpq = dev->prio_queue;

		if (osal_fifo_is_full(&vpq->prio_fifo[prio_cmd.priority]) != (bool)0) {
			VPU_INFO_DEV(dev->device, "Prio command fifo is full, prio=%d, inst_idx=%d.\n",
				prio_cmd.priority, prio_cmd.inst_idx);
			return 0;
		}
		osal_spin_lock(&vpq->vpu_prio_lock);
		osal_fifo_in(&vpq->prio_fifo[prio_cmd.priority], &prio_cmd, (uint32_t)sizeof(hb_vpu_prio_cmd_data_t));
		osal_spin_unlock(&vpq->vpu_prio_lock);
	}

	//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_wait_event_interruptible_timeout(vpq->cmd_wait_q[prio_cmd.priority],
		vpq->prio_done_flags[prio_cmd.priority][prio_cmd.inst_idx] == 1U, msecs_to_jiffies(5000));
	if (ret == 0) {
		VPU_INFO_DEV(dev->device, "Wait cmd finish timeout, prio=%d, inst_idx=%d.\n",
				prio_cmd.priority, prio_cmd.inst_idx);
		ret = (int32_t)osal_fifo_out_spinlocked(&(vpq->prio_fifo[prio_cmd.priority]),
				&prio_cmd, (uint32_t)sizeof(hb_vpu_prio_cmd_data_t), &vpq->vpu_prio_lock);
	}

	vpq->prio_done_flags[prio_cmd.priority][prio_cmd.inst_idx] = 0;

	return 0;
}

int32_t check_prio_flag(void *vpu)
{
	uint32_t i;
	int32_t flag = 0;
	int32_t prio[MAX_NUM_VPU_INSTANCE] = {0,};
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)vpu;

	if (dev->vpu_open_ref_count > 1) {
		for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
			if (dev->inst_prio[i] >= 0)
				prio[dev->inst_prio[i]] = 1;
		}
		for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
			flag += prio[i];
		}
	}

	return flag > 1 ? 1 : 0;
}

int32_t vpu_prio_init(void *vpu)
{
	uint32_t i, j;
	int32_t ret;
	hb_vpu_prio_queue_t *vpq = NULL;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)vpu;

	if (dev == NULL) {
		VPU_ERR("Invalid vpu device\n");
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vpq = (hb_vpu_prio_queue_t *)devm_kzalloc(dev->device, sizeof(hb_vpu_prio_queue_t), GFP_KERNEL);
	if (vpq == NULL) {
		VPU_ERR_DEV(dev->device, "Not enough memory for prio queue.\n");
		return -ENOMEM;
	}

	dev->prio_queue = vpq;

	for (i = 0; i < (uint32_t)MAX_PRIO_32; i++) {
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		ret = osal_fifo_alloc(&vpq->prio_fifo[i],
			MAX_PRIO_FIFO_LEN * (uint32_t)sizeof(hb_vpu_prio_cmd_data_t), GFP_KERNEL);
		if (ret) {
			VPU_ERR_DEV(dev->device, "Failed to kfifo_alloc prio fifo 0x%x\n", ret);
			for (j = 0; j < i; j++) {
				osal_fifo_free(&vpq->prio_fifo[j]);
			}
			osal_kfree(vpq);
			return -ENOMEM;
		}

		osal_waitqueue_init(&vpq->cmd_wait_q[i]);
	}

	osal_list_head_init(&vpq->vpq_filp_head);
	osal_spin_init(&vpq->vpu_prio_lock);
	osal_mutex_init(&vpq->vpu_prio_mutex);

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		dev->inst_prio[i] = -1;
	}

	vpq->dev = dev;

	return 0;
}

void vpu_prio_deinit(void *vpu)
{
	uint32_t i;
	hb_vpu_prio_queue_t *vpq = NULL;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)vpu;

	if (dev == NULL) {
		VPU_ERR("Invalid vpu device\n");
		return;
	}

	vpq = dev->prio_queue;

	for (i = 0; i < (uint32_t)MAX_PRIO_32; i++) {
		osal_fifo_free(&vpq->prio_fifo[i]);
	}

	osal_kfree(vpq);
}
