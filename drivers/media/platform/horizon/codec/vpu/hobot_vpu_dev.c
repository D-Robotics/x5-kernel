/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#define pr_fmt(fmt)    "hobot_vpu_dev: " fmt

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/eventpoll.h>
#include <linux/debugfs.h>
#include <linux/sched/signal.h>
#define RESERVED_WORK_MEMORY
#ifdef RESERVED_WORK_MEMORY
#include <linux/of_address.h>
#endif
#if defined(CONFIG_HOBOT_FUSA_DIAG)
#include <linux/hobot_diag.h>
#endif
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
#include <linux/kfifo.h>
#endif

#include "osal.h"
#include "hobot_vpu_ctl.h"
#include "hobot_vpu_debug.h"
#include "hobot_vpu_pm.h"
#include "hobot_vpu_reg.h"
#include "hobot_vpu_utils.h"
#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
#include "hobot_vpu_prio.h"
#endif

#define VPU_DRIVER_API_VERSION_MAJOR 1
#define VPU_DRIVER_API_VERSION_MINOR 2

#define PTHREAD_MUTEX_T_HANDLE_SIZE 4
#define MAX_FILE_PATH 256
#define DEFAULT_CODE_SIZE_MASK 0xFFFU
#define DEFAULT_REMAP_SIZE_MASK 0x1FFU
#define CODE_SIZE_TIMES 2U
#define CODE_SIZE_OFFSET 12
#define DEFAULT_REMAP_REG_VAL 0x80000000U
#define REMAP_REG_OFFSET_1 12U
#define REMAP_REG_OFFSET_2 16U
#define REMAP_REG_OFFSET_3 11U
#define REG_SIZE 4
#define COMMAND_REG_NUM 64
#define TIMEOUT_MS_TO_NS 1000000UL
#define VPU_ION_TYPE 19U
#define VPU_ION_TYPE_BIT_OFFSET 16

#define VPU_UNAVAILABLE_ADDRESS_START 0xFFFF0000ULL
#define VPU_UNAVAILABLE_ADDRESS_END 0xFFFFFFFFULL
#define VPU_AVAILABLE_DMA_ADDRESS_MASK 0xF00000000ULL
#define MAX_RETRY_ALLOC  5

#define VPU_GET_SEM_TIMEOUT    (120UL * (unsigned long)HZ)
#define VPU_GET_INST_SEM_TIMEOUT    (1UL * (unsigned long)HZ)

/**
 * Purpose: VPU log debug information switch
 * Value: [0, ]
 * Range: hobot_vpu_dev.c
 * Attention: NA
 */
static int32_t vpu_debug_info = 0;
/**
 * Purpose: VPU performance report switch
 * Value: [0, 1]
 * Range: hobot_vpu_dev.c
 * Attention: NA
 */
static int32_t vpu_pf_bw_debug = 0;
/**
 * Purpose: VPU frequency
 * Value: (0, MAX_VPU_FREQ]
 * Range: hobot_vpu_dev.c
 * Attention: NA
 */
static unsigned long vpu_clk_freq = MAX_VPU_FREQ; /* PRQA S 5209 */

// #define VPU_SUPPORT_RESERVED_VIDEO_MEMORY

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
// #define VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (62*1024*1024)
// #define VPU_DRAM_PHYSICAL_BASE 0x60000000	// 0x86C00000
#define VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (128*1024*1024)
#define VPU_DRAM_PHYSICAL_BASE 0xd6000000
static video_mm_t s_vmem = { 0 };
static hb_vpu_drv_buffer_t s_video_memory = { 0 };
#endif

/* this definition is only for chipsnmedia FPGA board env */
/* so for SOC env of customers can be ignored */

/*for kernel up to 3.7.0 version*/
#ifndef VM_RESERVED
#define VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

// PRQA S 0339,4501,4543,3219,4542,4464,0636,0605,0315,4548,0634,3432,1840,5209 ++
module_param(vpu_clk_freq, ulong, 0644);
module_param(vpu_debug_info, int, 0644);
module_param(vpu_pf_bw_debug, int, 0644);
// PRQA S 0339,4501,4543,3219,4542,4464,0636,0605,0315,4548,0634,3432,1840,5209 --

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
static hb_vpu_dev_t *vpu_pac;
static bool ai_mode;

enum vpu_pac_ctrl {
	VPU_NONE = 0,
	VPU_RES,
	VPU_CLKON,
	VPU_CLKOFF,
	VPU_HWRST,
	VPU_IRQ,
	VPU_IRQDONE,
	VPU_NOUSE,
};

int32_t vpu_pac_notifier_register(struct notifier_block *nb);
int32_t vpu_pac_notifier_unregister(struct notifier_block *nb);
void vpu_pac_irq_raise(uint32_t cmd, void *res);
void vpu_pac_ctrl_raise(uint32_t cmd, void *res);
bool hobot_fun_ai_mode_available(void);

static int32_t vpu_pac_dev_event(struct notifier_block *this,
		unsigned long event,
		void *ptr)
{
	int32_t ret;

	if (event == VPU_CLKON) {
		ret = hb_vpu_clk_enable(vpu_pac, vpu_pac->vpu_freq);
		if (ret != 0) {
			VPU_ERR_DEV(vpu_pac->device, "Failed to enable vpu clock, ret=%d.\n", ret);
			return NOTIFY_BAD;
		}
	} else if (event == VPU_CLKOFF) {
		ret = hb_vpu_clk_disable(vpu_pac);
		if (ret != 0) {
			VPU_ERR_DEV(vpu_pac->device, "Failed to disable vpu clock, ret=%d.\n", ret);
			return NOTIFY_BAD;
		}
	} else if (event == VPU_HWRST) {
		ret = hb_vpu_hw_reset();
		if (ret != 0) {
			VPU_ERR_DEV(vpu_pac->device, "Failed to hw reset vpu, ret=%d.\n", ret);
			return NOTIFY_BAD;
		}
	} else if (event == VPU_IRQDONE) {
		enable_irq(vpu_pac->irq);
	} else {
		VPU_ERR_DEV(vpu_pac->device, "Invalid vpu event, event=%ld.\n", event);
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}

static struct notifier_block vpu_pac_notifier = {
	.notifier_call  = vpu_pac_dev_event,
};
#endif

#if defined(CONFIG_HOBOT_FUSA_DIAG)
#define MASK_8BIT 0xffu
#define BIT_8 8u
#define FUSA_SW_ERR_CODE 0xffffu
#define FUSA_ENV_LEN 4u
static void vpu_send_diag_error_event(u16 id, u8 sub_id, u8 pri_data, u16 line_num)
{
	s32 ret;
	struct diag_event event;

	memset(&event, 0, sizeof(event));
	event.module_id = ModuleDiag_vpu;
	event.event_prio = (u8)DiagMsgLevel1;
	event.event_sta = (u8)DiagEventStaFail;
	event.event_id = id;
	event.fchm_err_code = FUSA_SW_ERR_CODE;
	event.env_len = FUSA_ENV_LEN;
	event.payload[0] = sub_id;
	event.payload[1] = pri_data;
	event.payload[2] = line_num & MASK_8BIT;
	event.payload[3] = line_num >> BIT_8;
	ret = diagnose_send_event(&event);
	if (ret != 0)
		VPU_ERR("%s fail %d\n", __func__, ret);
}
#else
static void vpu_send_diag_error_event(u16 id, u8 sub_id, u8 pri_data, u16 line_num)
{
	/* NULL */
}
#endif

#if defined(USE_SHARE_SEM_BT_KERNEL_AND_USER)
//coverity[HIS_metric_violation:SUPPRESS]
static void *get_mutex_base(const hb_vpu_dev_t *dev, uint32_t core, hb_vpu_mutex_t type)
{
	uint32_t instance_pool_size_per_core;
	void *vip_base;
	void *vdi_mutexes_base;
	void *jdi_mutex;

	if ((core > MAX_NUM_VPU_CORE) || (dev->instance_pool.base == 0U)) {
		VPU_ERR_DEV(dev->device, "Invalid parameters.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return NULL;
	}

	/* s_instance_pool.size  assigned to the size of all core once call
	VDI_IOCTL_GET_INSTANCE_POOL by user. */
	instance_pool_size_per_core = (dev->instance_pool.size/MAX_NUM_VPU_CORE);
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vip_base = (void *)(dev->instance_pool.base + ((uint64_t)instance_pool_size_per_core /* PRQA S 1891 */
		* (uint64_t)core));
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vdi_mutexes_base = (vip_base + (instance_pool_size_per_core - /* PRQA S 0497 */
		(sizeof(void *) * VDI_NUM_LOCK_HANDLES)));
	if (type == VPUDRV_MUTEX_VPU) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		jdi_mutex = (void *)(vdi_mutexes_base + ((size_t)VPUDRV_MUTEX_VPU * sizeof(void *)));
	} else if (type == VPUDRV_MUTEX_DISP_FALG) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		jdi_mutex = (void *)(vdi_mutexes_base + ((size_t)VPUDRV_MUTEX_DISP_FALG * sizeof(void *)));
	} else if (type == VPUDRV_MUTEX_VMEM) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		jdi_mutex = (void *)(vdi_mutexes_base + ((size_t)VPUDRV_MUTEX_VMEM * sizeof(void *)));
	} else if (type == VPUDRV_MUTEX_RESET) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		jdi_mutex = (void *)(vdi_mutexes_base + ((size_t)VPUDRV_MUTEX_RESET * sizeof(void *)));
	} else if (type == VPUDRV_MUTEX_REV1) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		jdi_mutex = (void *)(vdi_mutexes_base + ((size_t)VPUDRV_MUTEX_REV1 * sizeof(void *)));
	} else {
		VPU_ERR_DEV(dev->device, "unknown MUTEX_TYPE type=%d\n", type);
		vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return NULL;
	}
	return jdi_mutex;
}

static int32_t vdi_lock(hb_vpu_dev_t *dev, uint32_t core, hb_vpu_mutex_t type)
{
	int32_t ret;
	void *mutex;
	int32_t count;
	int32_t sync_ret;
	int32_t sync_val = current->tgid;
	volatile int32_t *sync_lock_ptr = NULL;

	mutex = get_mutex_base(dev, core, type);
	if (mutex == NULL) {
		VPU_ERR_DEV(dev->device, "Fail to get mutex base, core=%d, type=%d\n", core, type);
		return -EINVAL;
	}

	ret = 0;
	count = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	sync_lock_ptr = (volatile int32_t *)mutex;

	while((sync_ret = (int32_t)__sync_val_compare_and_swap(sync_lock_ptr, 0,
		sync_val)) != 0) {
		count++;
		if (count > (VPU_LOCK_BUSY_CHECK_TIMEOUT)) {
			VPU_ERR_DEV(dev->device, "Failed to get lock type=%d, sync_ret=%d, sync_val=%d, "
				"sync_ptr=%d, pid=%d, tgid=%d \n",
				type, sync_ret, sync_val, (int32_t)*sync_lock_ptr,
				current->pid, current->tgid);
			ret = -ETIME;
			break;
		}
		osal_mdelay(1U);
	}

	return ret;
}

static int32_t vdi_unlock(hb_vpu_dev_t *dev, uint32_t core, hb_vpu_mutex_t type)
{
	void *mutex;
	volatile int32_t *sync_lock_ptr = NULL;

	mutex = get_mutex_base(dev, core, type);
	if (mutex == NULL) {
		VPU_ERR_DEV(dev->device, "Fail to get mutex base, core=%d, type=%d\n", core, type);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	sync_lock_ptr = (volatile int32_t *)mutex;
	__sync_lock_release(sync_lock_ptr);

	return 0;
}

static int32_t vdi_lock_release(struct file *filp, hb_vpu_dev_t *dev, hb_vpu_instance_pool_t *vip,
				uint32_t core, uint32_t inst_idx, hb_vpu_mutex_t type)
{
	void *mutex;
	volatile int32_t *sync_lock_ptr;
	int32_t sync_val = current->tgid;
	unsigned long flags_mp;

	mutex = get_mutex_base(dev, core, type);
	if (mutex == NULL) {
		VPU_ERR_DEV(dev->device, "Fail to get mutex base, core=%d, type=%d\n", core, type);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	sync_lock_ptr = (volatile int32_t *)mutex;

	if (*sync_lock_ptr == sync_val) {
		VPU_INFO_DEV(dev->device, "Warning: Free core=%d, type=%d, sync_lock=%d, current_pid=%d, "
			"tgid=%d, sync_val=%d\n", core, type,
			(volatile int32_t)*sync_lock_ptr, current->pid, current->tgid,
			sync_val);
		if (type == VPUDRV_MUTEX_VPU) {
			VPU_INFO_DEV(dev->device, "Warning: Free pendingInst=%pK, pendingInstIdxPlus1=%d\n",
				vip->pendingInst, vip->pendingInstIdxPlus1);
			vip->pendingInst = NULL;
			vip->pendingInstIdxPlus1 = 0;

			osal_spin_lock_irqsave(&dev->irq_spinlock, (uint64_t *)&flags_mp);
#ifdef SUPPORT_MULTI_INST_INTR
			if (dev->interrupt_flag[inst_idx] == 1) {
#else
			if (dev->interrupt_flag == 1) {
#endif
				if (dev->irq_trigger == 1) {
					// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
					//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
					VPU_DBG_DEV(dev->device, "Warning: vpu_lock_release ignore irq.\n");
					// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
					enable_irq(dev->irq);
					dev->irq_trigger = 0;
					dev->poll_int_event[inst_idx]--;
					dev->vpu_irqenable = 1;
				}
#ifdef SUPPORT_MULTI_INST_INTR
				dev->interrupt_flag[inst_idx] = 0;
#else
				dev->interrupt_flag = 0;
#endif
			} else {
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#ifndef SUPPORT_MULTI_INST_INTR
				if ((VPU_READL(WAVE_VPU_BUSY_STATUS) != 0U) || (VPU_READL(WAVE_VPU_VPU_INT_STS) != 0U)) {
					dev->ignore_irq = 1;
					VPU_DBG_DEV(dev->device, "vpu_lock_release ignore irq.\n");
				}
#endif
			}
			osal_spin_unlock_irqrestore(&dev->irq_spinlock, (uint64_t *)&flags_mp);
#ifndef SUPPORT_MULTI_INST_INTR
			timeout = jiffies + (50UL * (unsigned long)HZ) / 1000UL;
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			while (VPU_READL(WAVE_VPU_BUSY_STATUS) != 0U) { /* PRQA S 1006,0497,1843,1021 */
				if (time_after(jiffies, timeout)) { /* PRQA S 3469,3415,4394,4558,4115,1021,1020,2996 */
					VPU_ERR_DEV(dev->device, "Wait Interrupt BUSY timeout pid %d\n", current->pid);
					osal_spin_lock_irqsave(&dev->irq_spinlock, (uint64_t *)&flags_mp);
					dev->ignore_irq = 0;
					osal_spin_unlock_irqrestore(&dev->irq_spinlock, (uint64_t *)&flags_mp);
					break;
				}
			}
#endif
		}
		(void)__sync_lock_release(sync_lock_ptr); /* PRQA S 3335 */
	}

	return 0;
}

#elif defined(USE_MUTEX_IN_KERNEL_SPACE)
static int32_t vdi_lock_base(struct file *filp, hb_vpu_dev_t *dev, hb_vpu_mutex_t type,
			int32_t fromUserspace)
{
	int32_t ret;
	if (type >= VPUDRV_MUTEX_VPU && type < VPUDRV_MUTEX_MAX) {
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
#ifndef CONFIG_PREEMPT_RT
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
		VPU_DBG_DEV(dev->device, "[+] type=%d, LOCK pid %lld, pid=%d, tgid=%d, "
			"fromuser %d.\n",
			type, dev->current_vdi_lock_pid[type], current->pid,
			current->tgid, fromUserspace);
#else
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
		VPU_DBG_DEV(dev->device, "[+] type=%d, semcnt %d LOCK pid %lld, pid=%d, tgid=%d, "
			"fromuser %d.\n",
			type, type == VPUDRV_MUTEX_VPU ?
			dev->vpu_vdi_sem.count : dev->vpu_vdi_vmem_sem.count,
			dev->current_vdi_lock_pid[type], current->pid,
			current->tgid, fromUserspace);
#endif
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	}

	if (type == VPUDRV_MUTEX_VPU) {
#ifndef CONFIG_PREEMPT_RT
		ret = mutex_lock_interruptible(&dev->vpu_vdi_mutex);
#else
		ret = osal_sema_down_interruptible(&dev->vpu_vdi_sem);
#endif
		if (fromUserspace == 0) {
			if (ret == -EINTR) {
#ifndef CONFIG_PREEMPT_RT
				ret = mutex_lock_killable(&dev->vpu_vdi_mutex);
#else
				ret = down_killable(&dev->vpu_vdi_sem);
#endif
			}
		}
	} else if (type == VPUDRV_MUTEX_DISP_FALG) {
#ifndef CONFIG_PREEMPT_RT
		ret = mutex_lock_interruptible(&dev->vpu_vdi_disp_mutex);
#else
		ret = osal_sema_down_interruptible(&dev->vpu_vdi_disp_sem);
#endif
		if (fromUserspace == 0) {
			if (ret == -EINTR) {
#ifndef CONFIG_PREEMPT_RT
				ret = mutex_lock_killable(&dev->vpu_vdi_disp_mutex);
#else
				ret = down_killable(&dev->vpu_vdi_disp_sem);
#endif
			}
		}
	} else if (type == VPUDRV_MUTEX_RESET) {
#ifndef CONFIG_PREEMPT_RT
		ret = mutex_lock_interruptible(&dev->vpu_vdi_reset_mutex);
#else
		ret = osal_sema_down_interruptible(&dev->vpu_vdi_reset_sem);
#endif
		if (fromUserspace == 0) {
			if (ret == -EINTR) {
#ifndef CONFIG_PREEMPT_RT
				ret = mutex_lock_killable(&dev->vpu_vdi_reset_mutex);
#else
				ret = down_killable(&dev->vpu_vdi_reset_sem);
#endif
			}
		}
	} else if (type == VPUDRV_MUTEX_VMEM) {
#ifndef CONFIG_PREEMPT_RT
		ret = mutex_lock_interruptible(&dev->vpu_vdi_vmem_mutex);
#else
		ret = osal_sema_down_interruptible(&dev->vpu_vdi_vmem_sem);
#endif
		if (fromUserspace == 0) {
			if (ret == -EINTR) {
#ifndef CONFIG_PREEMPT_RT
				ret = mutex_lock_killable(&dev->vpu_vdi_vmem_mutex);
#else
				ret = down_killable(&dev->vpu_vdi_vmem_sem);
#endif
			}
		}
	} else {
		VPU_ERR_DEV(dev->device, "unknown MUTEX_TYPE type=%d\n", type);
		vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	if (ret == 0) {
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		dev->current_vdi_lock_pid[type] = (int64_t)filp;
	} else {
		VPU_ERR_DEV(dev->device, "down_interruptible error ret=%d\n", ret);
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
#ifndef CONFIG_PREEMPT_RT
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
	VPU_DBG_DEV(dev->device, "[-] type=%d, LOCK pid %lld, pid=%d, tgid=%d, "
		"fromuser %d ret = %d.\n",
		type, dev->current_vdi_lock_pid[type], current->pid,
		current->tgid, fromUserspace, ret);
#else
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
	VPU_DBG_DEV(dev->device, "[-] type=%d, semcnt %d LOCK pid %lld, pid=%d, tgid=%d, "
		"fromuser %d ret = %d.\n",
		type, type == VPUDRV_MUTEX_VPU ?
		dev->vpu_vdi_sem.count : dev->vpu_vdi_vmem_sem.count,
		dev->current_vdi_lock_pid[type], current->pid,
		current->tgid, fromUserspace, ret);
#endif
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

static int32_t vdi_lock_user(struct file *filp, hb_vpu_dev_t *dev, hb_vpu_mutex_t type)
{
	return vdi_lock_base(filp, dev, type, 1);
}

#if 0
static int32_t vdi_lock(struct file *filp, hb_vpu_dev_t *dev, hb_vpu_mutex_t type)
{
	return vdi_lock_base(filp, dev, type, 0);
}
#endif

static int32_t vdi_unlock(hb_vpu_dev_t *dev, hb_vpu_mutex_t type)
{
	if (type >= VPUDRV_MUTEX_VPU && type < VPUDRV_MUTEX_MAX) {
		dev->current_vdi_lock_pid[type] = 0;
	} else {
		VPU_ERR_DEV(dev->device, "unknown MUTEX_TYPE type=%d\n", type);
		vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
#ifndef CONFIG_PREEMPT_RT
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
	VPU_DBG_DEV(dev->device, "[+] type=%d, LOCK pid %lld pid=%d, tgid=%d\n",
		type, dev->current_vdi_lock_pid[type], current->pid, current->tgid);
#else
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
	VPU_DBG_DEV(dev->device, "[+] type=%d, semcnt %d LOCK pid %lld pid=%d, tgid=%d\n",
		type, type == VPUDRV_MUTEX_VPU ?
		dev->vpu_vdi_sem.count : dev->vpu_vdi_vmem_sem.count,
		dev->current_vdi_lock_pid[type], current->pid, current->tgid);
#endif
	//coverity[misra_c_2012_rule_15_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (type == VPUDRV_MUTEX_VPU) {
#ifndef CONFIG_PREEMPT_RT
		osal_mutex_unlock(&dev->vpu_vdi_mutex);
#else
		osal_sema_up(&dev->vpu_vdi_sem);
#endif
	} else if (type == VPUDRV_MUTEX_DISP_FALG) {
#ifndef CONFIG_PREEMPT_RT
		osal_mutex_unlock(&dev->vpu_vdi_disp_mutex);
#else
		osal_sema_up(&dev->vpu_vdi_disp_sem);
#endif
	} else if (type == VPUDRV_MUTEX_RESET) {
#ifndef CONFIG_PREEMPT_RT
		osal_mutex_unlock(&dev->vpu_vdi_reset_mutex);
#else
		osal_sema_up(&dev->vpu_vdi_reset_sem);
#endif
	} else if (type == VPUDRV_MUTEX_VMEM) {
#ifndef CONFIG_PREEMPT_RT
		osal_mutex_unlock(&dev->vpu_vdi_vmem_mutex);
#else
		osal_sema_up(&dev->vpu_vdi_vmem_sem);
#endif
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
#ifndef CONFIG_PREEMPT_RT
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
	VPU_DBG_DEV(dev->device, "[-] type=%d, LOCK pid %lld pid=%d, tgid=%d\n",
		type, dev->current_vdi_lock_pid[type], current->pid, current->tgid);
#else
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
	VPU_DBG_DEV(dev->device, "[-] type=%d, semcnt %d LOCK pid %lld pid=%d, tgid=%d\n",
		type, type == VPUDRV_MUTEX_VPU ?
		dev->vpu_vdi_sem.count : dev->vpu_vdi_vmem_sem.count,
		dev->current_vdi_lock_pid[type], current->pid, current->tgid);
#endif
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return 0;
}

static int32_t vdi_lock_release(struct file *filp, hb_vpu_dev_t *dev, hb_vpu_instance_pool_t *vip,
				uint32_t core, uint32_t inst_idx, hb_vpu_mutex_t type)
{
	unsigned long flags_mp; /* PRQA S 5209 */
	/* vpu wait timeout to 50 msec */
	unsigned long timeout; /* PRQA S 5209,1840 */
	if ((type < VPUDRV_MUTEX_VPU) || (type >= VPUDRV_MUTEX_MAX)) {
		VPU_ERR_DEV(dev->device, "unknown MUTEX_TYPE type=%d\n", type);
		return -EINVAL;
	}
	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (dev->current_vdi_lock_pid[type] == (int64_t)filp) {
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
		VPU_DBG_DEV(dev->device, "MUTEX_TYPE: %d, VDI_LOCK_PID: %lld, current->pid: %d, "
			"current->tgid=%d, filp=%pK\n", type, dev->current_vdi_lock_pid[type],
			current->pid, current->tgid, filp);

		// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
		if (type == VPUDRV_MUTEX_VPU) {
			VPU_DBG_DEV(dev->device, "Free pendingInst=%pK, pendingInstIdxPlus1=%d\n",
				vip->pendingInst, vip->pendingInstIdxPlus1);
			vip->pendingInst = NULL;
			vip->pendingInstIdxPlus1 = 0;

			// PRQA S 1021,3473,1020,3432,2996,3200 ++
			osal_spin_lock_irqsave(&dev->irq_spinlock, (uint64_t *)&flags_mp);
			// PRQA S 1021,3473,1020,3432,2996,3200 --
#ifdef SUPPORT_MULTI_INST_INTR
			if (dev->interrupt_flag[inst_idx] == 1) {
#else
			if (dev->interrupt_flag == 1) {
#endif
				if (dev->irq_trigger == 1) {
					// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
					//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
					VPU_DBG_DEV(dev->device, "vpu_lock_release ignore irq.\n");
					// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
					enable_irq(dev->irq);
					dev->irq_trigger = 0;
					dev->poll_int_event[inst_idx]--;
					dev->vpu_irqenable = 1;
				}
#ifdef SUPPORT_MULTI_INST_INTR
				dev->interrupt_flag[inst_idx] = 0;
#else
				dev->interrupt_flag = 0;
#endif
			} else {
#ifndef SUPPORT_MULTI_INST_INTR
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				if ((VPU_READL(WAVE_VPU_BUSY_STATUS) != 0U) || (VPU_READL(WAVE_VPU_VPU_INT_STS) != 0U)) {
					dev->ignore_irq = 1;
					VPU_DBG_DEV(dev->device, "vpu_lock_release ignore irq.\n");
				}
#endif
			}
			osal_spin_unlock_irqrestore(&dev->irq_spinlock, (uint64_t *)&flags_mp);
#ifndef SUPPORT_MULTI_INST_INTR
			timeout = jiffies + (50UL * (unsigned long)HZ) / 1000UL;
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			while (VPU_READL(WAVE_VPU_BUSY_STATUS) != 0U) { /* PRQA S 1006,0497,1843,1021 */
				if (time_after(jiffies, timeout)) { /* PRQA S 3469,3415,4394,4558,4115,1021,1020,2996 */
					VPU_ERR_DEV(dev->device, "Wait Interrupt BUSY timeout pid %d\n", current->pid);
					osal_spin_lock_irqsave(&dev->irq_spinlock, (uint64_t *)&flags_mp);
					dev->ignore_irq = 0;
					osal_spin_unlock_irqrestore(&dev->irq_spinlock, (uint64_t *)&flags_mp);
					break;
				}
			}
#endif
		}
		vdi_unlock(dev, type);
	}

	return 0;
}

#else
static int32_t vdi_lock(hb_vpu_dev_t *dev, hb_vpu_mutex_t type)
{
	return 0;
}

static int32_t vdi_unlock(hb_vpu_dev_t *dev, hb_vpu_mutex_t type)
{
	return 0;
}
#endif

#ifdef USE_VPU_CLOSE_INSTANCE_ONCE_ABNORMAL_RELEASE
static hb_vpu_instance_pool_t *get_instance_pool_handle(
		hb_vpu_dev_t *dev, uint32_t core)
{
	uint32_t instance_pool_size_per_core;
	void *vip_base;

	if (core > MAX_NUM_VPU_CORE)
		return NULL;

	if (dev->instance_pool.base == 0ULL) {
		return NULL;
	}
	/* instance_pool.size  assigned to the size of all core once call
	VDI_IOCTL_GET_INSTANCE_POOL by user. */
	instance_pool_size_per_core = (dev->instance_pool.size/MAX_NUM_VPU_CORE);
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vip_base = (void *)(dev->instance_pool.base +
			((uint64_t)instance_pool_size_per_core * (uint64_t)core));

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	return (hb_vpu_instance_pool_t *)vip_base;
}
#define FIO_TIMEOUT         100

static void WriteVpuFIORegister(hb_vpu_dev_t *dev, uint32_t core,
				uint32_t addr, uint32_t data)
{
	uint32_t ctrl;
	uint32_t count = 0;
	if (dev == NULL) {
		return;
	}
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_VPU_FIO_DATA, data);
	ctrl  = (addr&0xffffU);
	ctrl |= (1U<<16U);    /* write operation */
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_VPU_FIO_CTRL_ADDR, ctrl);

	count = FIO_TIMEOUT;
	while (count--) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		ctrl = VPU_READL(W5_VPU_FIO_CTRL_ADDR);
		if (ctrl & 0x80000000) {
			break;
		}
	}
}
static uint32_t ReadVpuFIORegister(hb_vpu_dev_t *dev, uint32_t core, uint32_t addr)
{
	uint32_t ctrl;
	uint32_t count = 0;
	uint32_t data  = 0xffffffff;

	ctrl  = (addr&0xffffU);
	ctrl |= (0U<<16U);    /* read operation */
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_VPU_FIO_CTRL_ADDR, ctrl);
	count = FIO_TIMEOUT;
	while (count--) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		ctrl = VPU_READL(W5_VPU_FIO_CTRL_ADDR);
		if (ctrl & 0x80000000) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			data = VPU_READL(W5_VPU_FIO_DATA);
			break;
		}
	}

	return data;
}

static int32_t vpuapi_wait_reset_busy(hb_vpu_dev_t *dev, uint32_t core)
{
	int32_t ret;
	uint32_t val;
	uint32_t product_code;
	unsigned long timeout = jiffies + (unsigned long)VPU_BUSY_CHECK_TIMEOUT;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER);
	while (1) {
		if (PRODUCT_CODE_W_SERIES(product_code)) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			val = VPU_READL(W5_VPU_RESET_STATUS);
		} else {
			return -EINVAL;
		}
		if (val == 0U) {
			ret = (int32_t)VPUAPI_RET_SUCCESS;
			break;
		}
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		if (time_after(jiffies, timeout)) {
			VPU_ERR_DEV(dev->device, "vpuapi_wait_reset_busy after BUSY timeout");
			ret = (int32_t)VPUAPI_RET_TIMEOUT;
			break;
		}
		osal_udelay(0U);	// delay more to give idle time to OS;
	}

	return ret;
}

static int32_t vpuapi_wait_vpu_busy(hb_vpu_dev_t *dev, uint32_t core, unsigned long timeout)
{
	int32_t ret;
	uint32_t val = 0;
	uint32_t cmd;
	uint32_t pc;
	uint32_t product_code;
	//unsigned long timeout = jiffies + VPU_BUSY_CHECK_TIMEOUT;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER);
	while(1) {
		if (PRODUCT_CODE_W_SERIES(product_code)) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			val = VPU_READL(W5_VPU_BUSY_STATUS);
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			cmd = VPU_READL(W5_COMMAND);
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			pc = VPU_READL(W5_VCPU_CUR_PC);
		} else {
			return -EINVAL;
		}
		if (val == 0U) {
			ret = (int32_t)VPUAPI_RET_SUCCESS;
			break;
		}
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		if (time_after(jiffies, timeout)) {
			VPU_ERR_DEV(dev->device, "vpuapi_wait_vpu_busy timeout cmd=0x%x, pc=0x%x\n", cmd, pc);
			ret = (int32_t)VPUAPI_RET_TIMEOUT;
			break;
		}
		osal_udelay(0U);	// delay more to give idle time to OS;
	}

	return ret;
}

static int32_t vpuapi_wait_bus_busy(hb_vpu_dev_t *dev, uint32_t core,
				uint32_t bus_busy_reg_addr)
{
	int32_t ret;
	uint32_t val;
	uint32_t product_code;
	unsigned long timeout = jiffies + (unsigned long)VPU_BUSY_CHECK_TIMEOUT;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER);
	ret = (int32_t)VPUAPI_RET_SUCCESS;
	while (1) {
		if (PRODUCT_CODE_W_SERIES(product_code)) {
			val = ReadVpuFIORegister(dev, core, bus_busy_reg_addr);
			if (val == 0x3fU)
				break;
		} else {
			return -EINVAL;
		}
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		if (time_after(jiffies, timeout)) {
			VPU_ERR_DEV(dev->device, "vpuapi_wait_bus_busy timeout\n");
			ret = (int32_t)VPUAPI_RET_TIMEOUT;
			break;
		}
		osal_udelay(0U);	// delay more to give idle time to OS;
	}

	return ret;
}

// PARAMETER
/// mode 0 => wake
/// mode 1 => sleep
// return
//coverity[HIS_metric_violation:SUPPRESS]
static int32_t wave_sleep_wake(hb_vpu_dev_t *dev, uint32_t core, int32_t mode)
{
	uint32_t val;
	unsigned long timeout = jiffies + (unsigned long)VPU_BUSY_CHECK_TIMEOUT;

	if (mode == VPU_SLEEP_MODE) {
		if (vpuapi_wait_vpu_busy(dev, core, timeout) == (int32_t)VPUAPI_RET_TIMEOUT) {
			return (int32_t)VPUAPI_RET_TIMEOUT;
		}

		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_VPU_BUSY_STATUS, 1);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_COMMAND, (uint32_t)W5_SLEEP_VPU);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_VPU_HOST_INT_REQ, 1);

		if (vpuapi_wait_vpu_busy(dev, core, timeout) == (int32_t)VPUAPI_RET_TIMEOUT) {
			return (int32_t)VPUAPI_RET_TIMEOUT;
		}

		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		if (VPU_READL(W5_RET_SUCCESS) == 0U) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			val = VPU_READL(W5_RET_FAIL_REASON);
			if (val == WAVE5_SYSERR_VPU_STILL_RUNNING) {
				return (int32_t)VPUAPI_RET_STILL_RUNNING;
			} else {
				return (int32_t)VPUAPI_RET_FAILURE;
			}
		}
	} else {
		uint32_t remapSize;
		uint32_t codeBase;
		uint32_t codeSize;

		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_PO_CONF, 0);

		codeBase = (uint32_t)dev->common_memory.phys_addr;
		codeSize = (WAVE5_MAX_CODE_BUF_SIZE&~0xfffU);
		remapSize = (codeSize >> 12) & 0x1ffU;

		val = 0x80000000U | (WAVE5_UPPER_PROC_AXI_ID<<20) |
			(W5_REMAP_CODE_INDEX << 12) | (0U << 16) | (1U<<11) | remapSize;

		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_VPU_REMAP_CTRL,     val);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_VPU_REMAP_VADDR,    0x00000000);    /* DO NOT CHANGE! */
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_VPU_REMAP_PADDR,    codeBase);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_ADDR_CODE_BASE,     codeBase);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_CODE_SIZE,          codeSize);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_CODE_PARAM,         (WAVE5_UPPER_PROC_AXI_ID << 4) | 0U);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_HW_OPTION, 0);

		// encoder
		val  = (1U<<INT_WAVE5_ENC_SET_PARAM);
		val |= (1U<<INT_WAVE5_ENC_PIC);
		val |= (1U<<INT_WAVE5_BSBUF_FULL);
#ifdef SUPPORT_SOURCE_RELEASE_INTERRUPT
		val |= (1U<<INT_WAVE5_ENC_SRC_RELEASE);
#endif
		// decoder
		val |= (1U<<INT_WAVE5_INIT_SEQ);
		val |= (1U<<INT_WAVE5_DEC_PIC);
		val |= (1U<<INT_WAVE5_BSBUF_EMPTY);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_VPU_VINT_ENABLE,  val);

		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		val = VPU_READL(W5_VPU_RET_VPU_CONFIG0);
		if (((val>>16)&1U) == 1U) {
			val = ((WAVE5_PROC_AXI_ID<<28)  |
					(WAVE5_PRP_AXI_ID<<24)   |
					(WAVE5_FBD_Y_AXI_ID<<20) |
					(WAVE5_FBC_Y_AXI_ID<<16) |
					(WAVE5_FBD_C_AXI_ID<<12) |
					(WAVE5_FBC_C_AXI_ID<<8)  |
					(WAVE5_PRI_AXI_ID<<4)    |
					(WAVE5_SEC_AXI_ID<<0));
			WriteVpuFIORegister(dev, core, W5_BACKBONE_PROG_AXI_ID, val);
		}

		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_VPU_BUSY_STATUS, 1);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_COMMAND, (uint32_t)W5_WAKEUP_VPU);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_VPU_REMAP_CORE_START, 1);

		if (vpuapi_wait_vpu_busy(dev, core, timeout) == (int32_t)VPUAPI_RET_TIMEOUT) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_ERR_DEV(dev->device, "timeout pc=0x%x\n", VPU_READL(W5_VCPU_CUR_PC));
			return (int32_t)VPUAPI_RET_TIMEOUT;
		}

		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		val = VPU_READL(W5_RET_SUCCESS);
		if (val == 0U) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_ERR_DEV(dev->device, "VPUAPI_RET_FAILURE pc=0x%x \n", VPU_READL(W5_VCPU_CUR_PC));
			return (int32_t)VPUAPI_RET_FAILURE;
		}
	}

	return (int32_t)VPUAPI_RET_SUCCESS;
}

static int32_t wave_close_instance(struct file *filp, hb_vpu_dev_t *dev, uint32_t core, uint32_t inst)
{
	int32_t ret;
	uint32_t error_reason = 0;
	unsigned long timeout = jiffies + (unsigned long)VPU_DEC_TIMEOUT;

	VPU_DBG_DEV(dev->device, "[+][%02d] wave_close_instance\n", inst);
	if (vpu_check_is_decoder(dev, core, inst) == 1) {
		(void)vpuapi_dec_set_stream_end(filp, dev, core, inst);
		(void)vpuapi_dec_clr_all_disp_flag(filp, dev, core, inst);
	}
	while ((ret = vpuapi_close(filp, dev, core, inst)) == (int32_t)VPUAPI_RET_STILL_RUNNING) {
		ret = vpuapi_get_output_info(filp, dev, core, inst, &error_reason);
		if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
			VPU_INFO_DEV(dev->device, "inst[%02d] vpuapi_get_output_info ret=%d\n", inst, ret);
		} else {
			if ((error_reason & 0xf0000000)) {
				VPU_INFO_DEV(dev->device, "inst[%02d] need to do soft reset\n", inst);
				//if (vpu_do_sw_reset(filp, dev, core, inst, error_reason) == VPUAPI_RET_TIMEOUT) {
					break;
				//}
			}
		}

		if (vpu_check_is_decoder(dev, core, inst) == 1) {
			(void)vpuapi_dec_set_stream_end(filp, dev, core, inst);
			(void)vpuapi_dec_clr_all_disp_flag(filp, dev, core, inst);
		}

		osal_msleep(10U);	// delay for vpuapi_close
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		if (time_after(jiffies, timeout)) {
			VPU_ERR_DEV(dev->device, "inst[%02d] vpuapi_close flow timeout ret=%d\n", inst, ret);
			return 0;
		}
	}
	VPU_DBG_DEV(dev->device, "[-][%02d] wave_close_instance, ret = %d\n", inst, ret);

	return ret;
}

int32_t vpu_check_is_decoder(hb_vpu_dev_t *dev, uint32_t core, uint32_t inst)
{
	uint32_t is_decoder;
	unsigned char *codec_inst;
	hb_vpu_instance_pool_t *vip = get_instance_pool_handle(dev, core);

	if (vip == NULL) {
		return 0;
	}

	codec_inst = &vip->codec_inst_pool[inst][0];
	// indicates isDecoder in CodecInst structure in vpuapifunc.h
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	codec_inst = codec_inst + (sizeof(uint32_t) * 7U);
	memcpy((void*)&is_decoder, (const void*)codec_inst, 4);

	return (is_decoder == 1U)?1:0;
}

int32_t _vpu_close_instance(struct file *filp, hb_vpu_dev_t *dev, uint32_t core, uint32_t inst)
{
	uint32_t product_code;
	int32_t success;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER);
	if (PRODUCT_CODE_W_SERIES(product_code)) {
		success = wave_close_instance(filp, dev, core, inst);
	} else {
		VPU_ERR_DEV(dev->device, "vpu_close_instance Unknown product id : %08x\n", product_code);
		success = VPUAPI_RET_FAILURE;
	}
	return success;
}

#if defined(CONFIG_PM)
//coverity[HIS_metric_violation:SUPPRESS]
int32_t vpu_sleep_wake(struct file *filp, hb_vpu_dev_t *dev, uint32_t core, int32_t mode)
{
	uint32_t inst;
	int32_t ret;
	uint32_t product_code;
	uint32_t intr_reason_in_q;
	uint32_t interrupt_flag_in_q = 0;
	uint32_t error_reason = (uint32_t)VPUAPI_RET_SUCCESS;
	unsigned long timeout = jiffies + (unsigned long)VPU_DEC_TIMEOUT;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER);
	if (PRODUCT_CODE_W_SERIES(product_code)) {
		if (mode == VPU_SLEEP_MODE) {
			while((ret = wave_sleep_wake(dev, core, VPU_SLEEP_MODE)) ==
				(int32_t)VPUAPI_RET_STILL_RUNNING) {
				for (inst = 0; inst < MAX_NUM_VPU_INSTANCE; inst++) {
					intr_reason_in_q = 0;
#ifdef SUPPORT_MULTI_INST_INTR
					//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS]
					//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					interrupt_flag_in_q = kfifo_out_spinlocked(&dev->interrupt_pending_q[inst],
						&intr_reason_in_q, sizeof(uint32_t), &dev->vpu_kfifo_lock);
#endif
					if (interrupt_flag_in_q > 0U) {
						ret = vpuapi_get_output_info(filp, dev, core, inst, &error_reason);
						if (ret == (int32_t)VPUAPI_RET_SUCCESS) {
							if ((error_reason & 0xf0000000)) {
								if (vpu_do_sw_reset(filp, dev, core, inst, error_reason)
									== (int32_t)VPUAPI_RET_TIMEOUT) {
									break;
								}
							}
						}
					}
				}
				for (inst = 0; inst < MAX_NUM_VPU_INSTANCE; inst++) {
				}
				osal_mdelay(10U);
				//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				if (time_after(jiffies, timeout)) {
					return (int32_t)VPUAPI_RET_TIMEOUT;
				}
			}
		} else {
			ret = wave_sleep_wake(dev, core, VPU_WAKE_MODE);
		}
	} else {
		VPU_ERR_DEV(dev->device, "vpu_sleep_wake Unknown product id : %08x\n", product_code);
		ret = (int32_t)VPUAPI_RET_FAILURE;
	}
	return ret;
}
#endif
// PARAMETER
// reset_mode
// 0 : safely
// 1 : force
//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpuapi_sw_reset(struct file *filp, hb_vpu_dev_t *dev, uint32_t core, uint32_t inst, int32_t reset_mode)
{
	uint32_t val = 0;
	uint32_t product_code;
	int32_t ret;
	uint32_t supportDualCore;
	uint32_t supportBackbone;
	uint32_t supportVcoreBackbone;
	uint32_t supportVcpuBackbone;
// #if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
// 	uint32_t regSwUartStatus;
// #endif

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER);
	if (!PRODUCT_CODE_W_SERIES(product_code)) {
		VPU_ERR_DEV(dev->device, "Doesn't support swreset for coda \n");
		return (int32_t)VPUAPI_RET_INVALID_PARAM;
	}

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_VPU_BUSY_STATUS, 0);

	if (reset_mode == 0) {
		ret = wave_sleep_wake(dev, core, VPU_SLEEP_MODE);
		VPU_DBG_DEV(dev->device, "Sleep done ret=%d\n", ret);
		if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
			return ret;
		}
	}

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	val = VPU_READL(W5_VPU_RET_VPU_CONFIG0);
	if (((val>>16) & 0x1U) == 0x01U) {
		supportBackbone = 1U;
	} else {
		supportBackbone = 0U;
	}
	if (((val>>22) & 0x1U) == 0x01U) {
		supportVcoreBackbone = 1U;
	} else {
		supportVcoreBackbone = 0U;
	}
	if (((val>>28) & 0x1U) == 0x01U) {
		supportVcpuBackbone = 1U;
	} else {
		supportVcpuBackbone = 0U;
	}

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	val = VPU_READL(W5_VPU_RET_VPU_CONFIG1);
	if (((val>>26) & 0x1U) == 0x01U) {
		supportDualCore = 1U;
	} else {
		supportDualCore = 0U;
	}

	if (supportBackbone == 1U) {
		if (supportDualCore == 1U) {
			WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCORE0, 0x7);
			if (vpuapi_wait_bus_busy(dev, core, W5_BACKBONE_BUS_STATUS_VCORE0) !=
				(int32_t)VPUAPI_RET_SUCCESS) {
				WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCORE0, 0x00);
				return (int32_t)VPUAPI_RET_TIMEOUT;
			}

			WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCORE1, 0x7);
			if (vpuapi_wait_bus_busy(dev, core, W5_BACKBONE_BUS_STATUS_VCORE1) !=
				(int32_t)VPUAPI_RET_SUCCESS) {
				WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCORE1, 0x00);
				return (int32_t)VPUAPI_RET_TIMEOUT;
			}
		} else {
			if (supportVcoreBackbone == 1U) {
				if (supportVcpuBackbone == 1U) {
					// Step1 : disable request
					WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCPU, 0xFF);

					// Step2 : Waiting for completion of bus transaction
					if (vpuapi_wait_bus_busy(dev, core, W5_BACKBONE_BUS_STATUS_VCPU)
						!= (int32_t)VPUAPI_RET_SUCCESS) {
						WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCPU, 0x00);
						return (int32_t)VPUAPI_RET_TIMEOUT;
					}
				}
				// Step1 : disable request
				WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCORE0, 0x7);

				// Step2 : Waiting for completion of bus transaction
				if (vpuapi_wait_bus_busy(dev, core, W5_BACKBONE_BUS_STATUS_VCORE0)
					!= (int32_t)VPUAPI_RET_SUCCESS) {
					WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCORE0, 0x00);
					return (int32_t)VPUAPI_RET_TIMEOUT;
				}
			} else {
				// Step1 : disable request
				WriteVpuFIORegister(dev, core, W5_COMBINED_BACKBONE_BUS_CTRL, 0x7);

				// Step2 : Waiting for completion of bus transaction
				if (vpuapi_wait_bus_busy(dev, core, W5_COMBINED_BACKBONE_BUS_STATUS)
					!= (int32_t)VPUAPI_RET_SUCCESS) {
					WriteVpuFIORegister(dev, core, W5_COMBINED_BACKBONE_BUS_CTRL, 0x00);
					return (int32_t)VPUAPI_RET_TIMEOUT;
				}
			}
		}
	} else {
		// Step1 : disable request
		WriteVpuFIORegister(dev, core, W5_GDI_BUS_CTRL, 0x100);

		// Step2 : Waiting for completion of bus transaction
		if (vpuapi_wait_bus_busy(dev, core, W5_GDI_BUS_STATUS) != (int32_t)VPUAPI_RET_SUCCESS) {
			WriteVpuFIORegister(dev, core, W5_GDI_BUS_CTRL, 0x00);
			return (int32_t)VPUAPI_RET_TIMEOUT;
		}
	}

	val = W5_RST_BLOCK_ALL;
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_VPU_RESET_REQ, val);

	if (vpuapi_wait_reset_busy(dev, core) != (int32_t)VPUAPI_RET_SUCCESS) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_VPU_RESET_REQ, 0);
		return (int32_t)VPUAPI_RET_TIMEOUT;
	}

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_VPU_RESET_REQ, 0);

	// Step3 : must clear GDI_BUS_CTRL after done SW_RESET
	if (supportBackbone == 1U) {
		if (supportDualCore == 1U) {
			WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCORE0, 0x00);
			WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCORE1, 0x00);
		} else {
			if (supportVcoreBackbone == 1U) {
				if (supportVcpuBackbone == 1U) {
					WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCPU, 0x00);
				}
				WriteVpuFIORegister(dev, core, W5_BACKBONE_BUS_CTRL_VCORE0, 0x00);
			} else {
				WriteVpuFIORegister(dev, core, W5_COMBINED_BACKBONE_BUS_CTRL, 0x00);
			}
		}
	} else {
		WriteVpuFIORegister(dev, core, W5_GDI_BUS_CTRL, 0x00);
	}

	ret = wave_sleep_wake(dev, core, VPU_WAKE_MODE);
	return ret;
}

static int32_t wave_issue_command(hb_vpu_dev_t *dev, uint32_t core, uint32_t inst,
				uint32_t cmd, unsigned long timeout)
{
	int32_t ret;
	uint32_t codec_mode;
	unsigned char *codec_inst;
	hb_vpu_instance_pool_t *vip = get_instance_pool_handle(dev, core);

	if (vip == NULL) {
		return (int32_t)VPUAPI_RET_INVALID_PARAM;
	}

	codec_inst = &vip->codec_inst_pool[inst][0];
	// indicates codecMode in CodecInst structure in vpuapifunc.h
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	codec_inst = codec_inst + (sizeof(uint32_t) * 3U);
	memcpy((void*)&codec_mode, (const void*)codec_inst, 4);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_CMD_INSTANCE_INFO, (codec_mode << 16)|(inst&0xffffU));
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_VPU_BUSY_STATUS, 1);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_COMMAND, cmd);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_VPU_HOST_INT_REQ, 1);

	ret = vpuapi_wait_vpu_busy(dev, core, timeout);
	return ret;
}

static int32_t wave_send_query_command(hb_vpu_dev_t *dev,
			uint32_t core, uint32_t inst, uint32_t queryOpt)
{
	int32_t ret;
	unsigned long timeout = jiffies + (unsigned long)VPU_BUSY_CHECK_TIMEOUT;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_QUERY_OPTION, queryOpt);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_VPU_BUSY_STATUS, 1);
	ret = wave_issue_command(dev, core, inst, (uint32_t)W5_QUERY, timeout);
	if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
		VPU_DBG_DEV(dev->device, "inst[%02d] Query unsuccess for busy ret=%d\n", inst, ret);
		return ret;
	}

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (VPU_READL(W5_RET_SUCCESS) == 0U) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_DBG_DEV(dev->device, "inst[%02d] Query success but reason=%d\n", inst, VPU_READL(W5_RET_DEC_ERR_INFO));
		return (int32_t)VPUAPI_RET_FAILURE;
	}

	return (int32_t)VPUAPI_RET_SUCCESS;
}

//coverity[HIS_metric_violation:SUPPRESS]
int32_t vpuapi_get_output_info(struct file *filp, hb_vpu_dev_t *dev, uint32_t core,
		uint32_t inst, uint32_t *error_reason)
{
	int32_t ret = (int32_t)VPUAPI_RET_SUCCESS;
	uint32_t val;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_CMD_DEC_ADDR_REPORT_BASE, 0);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_CMD_DEC_REPORT_SIZE,      0);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_CMD_DEC_REPORT_PARAM,     0);

	ret = wave_send_query_command(dev, core, inst, (uint32_t)GET_RESULT);
	if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
		VPU_INFO_DEV(dev->device, "inst[%02d] query ret=%d\n", inst, ret);
		return ret;
	}

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "[+][%02d] success=%d, fail_reason=0x%x, error_reason=0x%x\n",
		inst,
		VPU_READL(W5_RET_DEC_DECODING_SUCCESS),
		VPU_READL(W5_RET_FAIL_REASON),
		VPU_READL(W5_RET_DEC_ERR_INFO));
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	val = VPU_READL(W5_RET_DEC_DECODING_SUCCESS);
	if ((val & 0x01U) == 0U) {
#ifdef SUPPORT_SW_UART
		*error_reason = 0;
#else
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		*error_reason = VPU_READL(W5_RET_DEC_ERR_INFO);
#endif
	} else {
		*error_reason = 0x00;
	}

	VPU_DBG_DEV(dev->device, "[-][%02d] ret=%d\n", inst, ret);
	return ret;
}

int32_t vpuapi_dec_clr_all_disp_flag(struct file *filp, hb_vpu_dev_t *dev, uint32_t core, uint32_t inst)
{
	int32_t ret = (int32_t)VPUAPI_RET_SUCCESS;
	uint32_t val;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_CMD_DEC_CLR_DISP_IDC, 0xffffffff);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_WRITEL(W5_CMD_DEC_SET_DISP_IDC, 0);
	ret = wave_send_query_command(dev, core, inst, (uint32_t)UPDATE_DISP_FLAG);
	if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
		VPU_INFO_DEV(dev->device, "inst[%02d] query ret=%d\n", inst, ret);
		return ret;
	}

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	val = VPU_READL(W5_RET_SUCCESS);
	if (val == 0U) {
		ret = (int32_t)VPUAPI_RET_FAILURE;
	}

	if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
		VPU_INFO_DEV(dev->device, "inst[%02d] ret=%d\n", inst, ret);
	}
	return ret;
}

int32_t vpuapi_dec_set_stream_end(struct file *filp, hb_vpu_dev_t *dev, uint32_t core, uint32_t inst)
{
	int32_t ret = (int32_t)VPUAPI_RET_SUCCESS;
	uint32_t val;
	uint32_t product_code;
	unsigned long timeout = jiffies + (unsigned long)VPU_BUSY_CHECK_TIMEOUT;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER);
	if (PRODUCT_CODE_W_SERIES(product_code)) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_WRITEL(W5_BS_OPTION, (1U/*STREAM END*/<<1) | (1U/*explictEnd*/));
		// keep not to be changed
		// WriteVpuRegister(core, W5_BS_WR_PTR, pDecInfo->streamWrPtr);

		ret = wave_issue_command(dev, core, inst, (uint32_t)W5_UPDATE_BS, timeout);
		if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
			VPU_INFO_DEV(dev->device, "inst[%02d] ret=%d\n", inst, ret);
			return ret;
		}

		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		val = VPU_READL(W5_RET_SUCCESS);
		if (val == 0U) {
			ret = (int32_t)VPUAPI_RET_FAILURE;
			VPU_ERR_DEV(dev->device, "inst[%02d] ret=%d\n", inst, ret);
			return ret;
		}
	} else {
		ret = (int32_t)VPUAPI_RET_FAILURE;
	}

	if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
		VPU_INFO_DEV(dev->device, "inst[%02d] ret=%d\n", inst, ret);
	}
	return ret;
}

//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
int32_t vpuapi_close(struct file *filp, hb_vpu_dev_t *dev, uint32_t core, uint32_t inst)
{
	int32_t ret = (int32_t)VPUAPI_RET_SUCCESS;
	uint32_t val;
	uint32_t product_code;
	unsigned long timeout = jiffies + (unsigned long)VPU_BUSY_CHECK_TIMEOUT;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER);

	if (PRODUCT_CODE_W_SERIES(product_code)) {
		ret = wave_issue_command(dev, core, inst, (uint32_t)W5_DESTROY_INSTANCE, timeout);
		if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
			VPU_DBG_DEV(dev->device, "inst[%02d] ret=%d\n", inst, ret);
			return ret;
		}

		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		val = VPU_READL(W5_RET_SUCCESS);
		if (val == 0U) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			val = VPU_READL(W5_RET_FAIL_REASON);
			if (val == WAVE5_SYSERR_VPU_STILL_RUNNING) {
				ret = (int32_t)VPUAPI_RET_STILL_RUNNING;
			} else {
				VPU_ERR_DEV(dev->device, "inst[%02d] command result=%d\n", inst, val);
				ret = (int32_t)VPUAPI_RET_FAILURE;
			}
		} else {
			ret = (int32_t)VPUAPI_RET_SUCCESS;
		}
	} else {
			ret = (int32_t)VPUAPI_RET_FAILURE;
	}

	if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
		VPU_DBG_DEV(dev->device, "inst[%02d] ret=%d\n", inst, ret);
	}

	return ret;
}

//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
int32_t vpu_do_sw_reset(struct file *filp,
	hb_vpu_dev_t *dev, uint32_t core, uint32_t inst, uint32_t error_reason)
{
	int32_t ret;
	hb_vpu_instance_pool_t *vip = get_instance_pool_handle(dev, core);

	if (vip == NULL)
		return (int32_t)VPUAPI_RET_FAILURE;

	ret = (int32_t)VPUAPI_RET_SUCCESS;
	if (error_reason & 0xf0000000) {
		ret = vpuapi_sw_reset(filp, dev, core, inst, 0);
		if (ret == (int32_t)VPUAPI_RET_STILL_RUNNING) {
			VPU_DBG_DEV(dev->device, "VPU is still running\n");
		} else if (ret == (int32_t)VPUAPI_RET_SUCCESS) {
			VPU_DBG_DEV(dev->device, "success\n");
		} else {
			VPU_ERR_DEV(dev->device, "Fail result=0x%x\n", ret);
		}
	}

	osal_mdelay(10U);
	return ret;
}
#endif

static int32_t vpu_check_physical_address(hb_vpu_dev_t *dev,
				phys_addr_t phys_addr, size_t size)
{
	uint64_t mask = phys_addr & VPU_AVAILABLE_DMA_ADDRESS_MASK;
	phys_addr_t unavail_phys_start = mask | VPU_UNAVAILABLE_ADDRESS_START;
	phys_addr_t unavail_phys_end = mask | VPU_UNAVAILABLE_ADDRESS_END;

	if (((unavail_phys_start >= phys_addr) &&
		(unavail_phys_start <= (phys_addr + size))) ||
		((unavail_phys_end >= phys_addr) &&
		(unavail_phys_end <= (phys_addr + size)))) {
		VPU_ERR_DEV(dev->device, "Invalid address region [0x%llx, 0x%llx] "
			"which include unavaliable address region[0x%llx, 0x%llx].\n",
			phys_addr, phys_addr + size,
			unavail_phys_start, unavail_phys_end);
		return -EFAULT;
	}

	return 0;
}

#ifdef RESERVED_WORK_MEMORY
#if defined(CONFIG_HOBOT_FPGA_J5) || defined(CONFIG_HOBOT_J5) || defined(CONFIG_HOBOT_FPGA_HAPS_J5)
#else
static void *vpu_vmap_work_buffer_memory(hb_vpu_work_resmem_t *reserved)
{
	void *vaddr = NULL;
	struct page **pages;
	uint32_t i, page_count = 0;
	pgprot_t pgprot;

	VPU_DEBUG("%s phys=0x%llx, size=%d\n", __func__, reserved->base, reserved->size);
	page_count = reserved->size / PAGE_SIZE;	// must be align.
	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = reserved->base + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}

	pgprot = pgprot_writecombine(PAGE_KERNEL);	// nocached
	vaddr = vmap(pages, page_count, VM_MAP, pgprot);
	if (vaddr == NULL) {
		VPU_ERR("%s Failed to vmap, page_num=%d\n", __func__, page_count);
		kfree(pages);
		return NULL;
	}

	kfree(pages);
	return vaddr;
}
static void vpu_vunmap_work_buffer_memory(hb_vpu_work_resmem_t *reserved)
{
	if (reserved->vaddr != NULL) {
		vunmap(reserved->vaddr);
		reserved->vaddr = NULL;
	}
}
// Notes: can't operate other instance work buffer
static void vpu_reset_work_buffer_memory(hb_vpu_work_resmem_t *reserved, hb_vpu_drv_buffer_t *work_buf)
{
	uint64_t offset = work_buf->phys_addr - reserved->phys_addr;
	memset(reserved->vaddr + (uint32_t)offset, 0, work_buf->size);
}
#endif
#endif

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_alloc_dma_buffer(hb_vpu_dev_t *dev, hb_vpu_drv_buffer_t * vb)
{
#ifndef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	struct ion_handle * handle = NULL;
	struct ion_handle * pre_handle[MAX_RETRY_ALLOC] = {NULL, NULL, NULL, NULL, NULL};
	int32_t ret, i = 0, j;
	size_t size;
	void * vaddr;
	uint32_t memtypes;
#ifdef RESERVED_WORK_MEMORY
	uint32_t inst_idx;
#endif
#endif

	if ((vb == NULL) || (dev == NULL)) {
		VPU_ERR("Invalid parameters.(dev=%pK, vb=%pK)\n", dev, vb);
		vpu_send_diag_error_event((u16)EventIdVPUDevAllocDMABufErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	vb->phys_addr = (uint64_t)vmem_alloc(&s_vmem, vb->size, 0);
	if (vb->phys_addr == (uint64_t)-1) {
		VPU_ERR_DEV(dev->device, "Physical memory allocation error size=%d\n", vb->size);
		vpu_send_diag_error_event((u16)EventIdVPUDevAllocDMABufErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -ENOMEM;
	}

	vb->base = (uint64_t)(s_video_memory.base +
					(vb->phys_addr - s_video_memory.phys_addr));
#else
#if 0
	vb->base = (uint64_t)dma_alloc_coherent(dev->device, PAGE_ALIGN(vb->size), /* PRQA S 3469,1840,1020,4491 */
							(dma_addr_t *) (&vb->phys_addr),
							GFP_DMA | GFP_KERNEL);
	if ((void *)(vb->base) == NULL) {
		VPU_ERR_DEV(dev->device, "Physical memory allocation error size=%d\n", vb->size);
		return -ENOMEM;
	}
#endif
	if (dev->vpu_ion_client == NULL) {
		VPU_ERR_DEV(dev->device, "VPU ion client is NULL.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAllocDMABufErr, (u8)ERR_SEQ_3, 0, __LINE__);
		return -EINVAL;
	}

	memtypes = (vb->flags >> MEM_TYPE_OFFSET) & MEM_TYPE_MASK;
	if ((memtypes == (uint32_t)DEC_BS) || (memtypes == (uint32_t)ENC_BS) ||
		(memtypes == (uint32_t)ENC_SRC) || (memtypes == (uint32_t)DEC_FB_LINEAR)) {
		vb->flags |= ((uint32_t)ION_FLAG_CACHED | (uint32_t)ION_FLAG_CACHED_NEEDS_SYNC);
	}
#ifdef RESERVED_WORK_MEMORY
	inst_idx = (vb->flags >> INST_IDX_OFFSET) & INST_IDX_MASK;
#endif
	do {
#ifdef RESERVED_WORK_MEMORY
		if (memtypes == DEC_WORK) {
			if (vb->size == dev->dec_work_memory[inst_idx].size) {
				size = vb->size;
				vb->phys_addr = dev->dec_work_memory[inst_idx].phys_addr;
				vb->iova = dev->dec_work_memory[inst_idx].iova;
				/* reset work buffer memory to zero */
#if defined(CONFIG_HOBOT_FPGA_J5) || defined(CONFIG_HOBOT_J5) || defined(CONFIG_HOBOT_FPGA_HAPS_J5)
#else
				vpu_reset_work_buffer_memory(&dev->codec_mem_reserved2, &dev->dec_work_memory[inst_idx]);
				VPU_DBG_DEV(dev->device, "inst[%02d] DEC_WORK memory reset.\n", inst_idx);
#endif
			} else {
				VPU_ERR_DEV(dev->device, "Invalid DEC_WORK size, size = %d.\n", vb->size);
				return -EINVAL;
			}
		} else if (memtypes == ENC_WORK) {
			if (vb->size == dev->enc_work_memory[inst_idx].size) {
				size = vb->size;
				vb->phys_addr = dev->enc_work_memory[inst_idx].phys_addr;
				vb->iova = dev->enc_work_memory[inst_idx].iova;
				/* reset work buffer memory to zero */
#if defined(CONFIG_HOBOT_FPGA_J5) || defined(CONFIG_HOBOT_J5) || defined(CONFIG_HOBOT_FPGA_HAPS_J5)
#else
				vpu_reset_work_buffer_memory(&dev->codec_mem_reserved2, &dev->enc_work_memory[inst_idx]);
				VPU_DBG_DEV(dev->device, "inst[%02d] ENC_WORK memory reset.\n", inst_idx);
#endif
			} else {
				VPU_ERR_DEV(dev->device, "Invalid ENC_WORK size, size = %d.\n", vb->size);
				return -EINVAL;
			}
		} else {
#endif
			handle = ion_alloc(dev->vpu_ion_client, vb->size, 0,
				ION_HEAP_TYPE_CMA_RESERVED_MASK, vb->flags);
			if (IS_ERR_OR_NULL(handle)) {
				VPU_ERR_DEV(dev->device, "failed to allocate ion memory, ret = %pK.\n", handle);
				vpu_send_diag_error_event((u16)EventIdVPUDevAllocDMABufErr, (u8)ERR_SEQ_4, 0, __LINE__);
				ret = (int32_t)PTR_ERR(handle);
				break;
			}

			ret = ion_phys(dev->vpu_ion_client, handle->id,
				(phys_addr_t *)&vb->phys_addr, &size);
			if (ret != 0) {
				VPU_ERR_DEV(dev->device, "failed to get ion phys address(ret=%d).\n", ret);
				vpu_send_diag_error_event((u16)EventIdVPUDevAllocDMABufErr, (u8)ERR_SEQ_5, 0, __LINE__);
				ion_free(dev->vpu_ion_client, handle);
				break;
			}
#ifdef RESERVED_WORK_MEMORY
		}
#endif

		ret = vpu_check_physical_address(dev, vb->phys_addr, size);
		if (ret == 0) {
			break;
		}
		pre_handle[i] = handle;
		i++;
	} while (i < MAX_RETRY_ALLOC);

	for (j = 0; j < i; j++) {
		if (pre_handle[j] != NULL) {
			ion_free(dev->vpu_ion_client, pre_handle[j]);
		}
	}
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to allocate ion memory after %d times, ret = %d.\n",
			i, ret);
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_6, 0, __LINE__);
		return ret;
	}

#ifdef RESERVED_WORK_MEMORY
	if ((memtypes != DEC_WORK) && (memtypes != ENC_WORK)) {
#endif
		vaddr = ion_map_kernel(dev->vpu_ion_client, handle);
		if (IS_ERR_OR_NULL(vaddr)) {
			VPU_ERR_DEV(dev->device, "failed to get ion virt address(ret=%d).\n", (int32_t)PTR_ERR(vaddr));
			ion_free(dev->vpu_ion_client, handle);
			vpu_send_diag_error_event((u16)EventIdVPUDevAllocDMABufErr, (u8)ERR_SEQ_7, 0, __LINE__);
			return -EFAULT;
		}

		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		vb->base = (uint64_t)vaddr;
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		vb->handle = (int64_t)handle;
		vb->fd = 0;
		vb->share_id = handle->share_id;
#ifdef RESERVED_WORK_MEMORY
	}
#endif
#endif
	return 0;
}

static int32_t vpu_free_dma_buffer(hb_vpu_dev_t *dev, hb_vpu_drv_buffer_t * vb)
{
#ifndef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	struct ion_handle *handle;
#endif
#ifdef RESERVED_WORK_MEMORY
	uint32_t memtypes;
#endif

	if ((vb == NULL) || (dev == NULL)) {
		VPU_ERR("Invalid parameters.(dev=%pK, vb=%pK)\n", dev, vb);
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	if (vb->base != 0UL)
		vmem_free(&s_vmem, vb->phys_addr, 0);
#else
#if 0
	if (vb->base != 0UL)
		dma_free_coherent(dev->device, PAGE_ALIGN(vb->size), (void *)vb->base, /* PRQA S 3469,1840,1020,4491 */
			vb->phys_addr);
#endif
#ifdef RESERVED_WORK_MEMORY
	memtypes = (vb->flags >> MEM_TYPE_OFFSET) & MEM_TYPE_MASK;
	if ((memtypes != DEC_WORK) && (memtypes != ENC_WORK)) {
#endif
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		handle = (struct ion_handle *)vb->handle;
		ion_unmap_kernel(dev->vpu_ion_client, handle);
		ion_free(dev->vpu_ion_client, handle);
#ifdef RESERVED_WORK_MEMORY
	}
#endif
#endif

	return 0;
}

static void vpu_test_interrupt_flag_and_enableirq(hb_vpu_dev_t *dev, uint32_t inst)
{
	// PRQA S 1021,3473,1020,3432,2996,3200 --
	if (dev->interrupt_flag[inst] == 1) {
		if (dev->irq_trigger == 1) {
			enable_irq(dev->irq);
			dev->irq_trigger = 0;
			dev->vpu_irqenable = 1;
			VPU_DBG_DEV(dev->device, "inst[%02d] "
				"interrupt_flag not empty, turn on irqenable\n", inst);
		}
		dev->interrupt_flag[inst] = 0;
	}
}

/* code review E1: No need to return value */
//coverity[HIS_metric_violation:SUPPRESS]
static void vpu_clear_instance_status(hb_vpu_dev_t *dev, hb_vpu_instance_list_t *vil)
{
#ifdef SUPPORT_MULTI_INST_INTR
	unsigned long flags_mp; /* PRQA S 5209 */
#endif

	if (dev->inst_open_flag[vil->inst_idx] != 0) {
		dev->vpu_open_ref_count--;
	}
	dev->inst_open_flag[vil->inst_idx] = 0;
	osal_list_del(&vil->list);
	if (osal_test_bit(vil->inst_idx, (const volatile uint64_t*)dev->vpu_crash_inst_bitmap) == 0) {
		(void)osal_test_and_clear_bit(vil->inst_idx, (volatile uint64_t *)dev->vpu_inst_bitmap); /* PRQA S 4446 */
		osal_sema_up(&dev->vpu_free_inst_sem);
	}
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev->poll_event[vil->inst_idx] = LLONG_MIN;
	osal_wake_up(&dev->poll_wait_q[vil->inst_idx]); /*PRQA S 3469*/
	osal_wake_up(&dev->poll_int_wait_q[vil->inst_idx]); /*PRQA S 3469*/

	(void)memset((void *)&dev->vpu_ctx[vil->inst_idx], 0x00,
		sizeof(dev->vpu_ctx[vil->inst_idx]));
	(void)memset((void *)&dev->vpu_status[vil->inst_idx], 0x00,
		sizeof(dev->vpu_status[vil->inst_idx]));
#ifdef SUPPORT_MULTI_INST_INTR
	// PRQA S 1021,3473,1020,3432,2996,3200 ++
	osal_spin_lock_irqsave(&dev->irq_spinlock, (uint64_t *)&flags_mp);
	vpu_test_interrupt_flag_and_enableirq(dev, vil->inst_idx);
	osal_spin_unlock_irqrestore(&dev->irq_spinlock, (uint64_t *)&flags_mp);
#endif
	osal_kfree((void *)vil);
}

static void vpu_force_free_lock(hb_vpu_dev_t *dev, uint32_t core)
{
#if defined(USE_MUTEX_IN_KERNEL_SPACE)
	int32_t i;

	for (i = 0; i < (int32_t)VPUDRV_MUTEX_MAX; i++) {
		if (dev->current_vdi_lock_pid[i] != 0) {
			VPU_DBG_DEV(dev->device, "RELEASE MUTEX_TYPE: %d, VDI_LOCK_PID: %lld, current->pid: %d, current->tgid: %d.\n",
			i, dev->current_vdi_lock_pid[i], current->pid, current->tgid);
			vdi_unlock(dev, i);
		}
	}
#elif defined(USE_SHARE_SEM_BT_KERNEL_AND_USER)
#endif
}

static void vpu_free_lock(struct file *filp, hb_vpu_dev_t *dev,
		hb_vpu_instance_pool_t *vip, uint32_t core, uint32_t inst_idx)
{
#if defined(USE_MUTEX_IN_KERNEL_SPACE) || defined(USE_SHARE_SEM_BT_KERNEL_AND_USER)
	int32_t ret;
#else
	uint32_t instance_pool_size_per_core;
	void *vdi_mutexes_base;
	uint32_t PTHREAD_MUTEX_T_DESTROY_VALUE = 0xdead10cc;
#endif

#if defined(USE_MUTEX_IN_KERNEL_SPACE) || defined(USE_SHARE_SEM_BT_KERNEL_AND_USER)
	ret = vdi_lock_release(filp, dev, vip, core, inst_idx, VPUDRV_MUTEX_VPU);
	if (ret != 0) {
		vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}
	ret = vdi_lock_release(filp, dev, vip, core, inst_idx, VPUDRV_MUTEX_DISP_FALG);
	if (ret != 0) {
		vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_1, 0, __LINE__);
	}
	ret = vdi_lock_release(filp, dev, vip, core, inst_idx, VPUDRV_MUTEX_RESET);
	if (ret != 0) {
		vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_2, 0, __LINE__);
	}
	ret = vdi_lock_release(filp, dev, vip, core, inst_idx, VPUDRV_MUTEX_VMEM);
	if (ret != 0) {
		vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_3, 0, __LINE__);
	}
#else
	instance_pool_size_per_core = (dev->instance_pool.size / MAX_NUM_VPU_CORE);
	vdi_mutexes_base = (vip +
		(instance_pool_size_per_core -
		(uint32_t)PTHREAD_MUTEX_T_HANDLE_SIZE * VDI_NUM_LOCK_HANDLES));
	VPU_DBG_DEV(dev->device, "force to destroy "
		"vdi_mutexes_base=%pK in userspace \n",
		vdi_mutexes_base);
	if (vdi_mutexes_base) {
		uint32_t i;
		for (i = 0; i < VDI_NUM_LOCK_HANDLES; i++) {
			memcpy((uint32_t *)vdi_mutexes_base,
				&PTHREAD_MUTEX_T_DESTROY_VALUE,
				PTHREAD_MUTEX_T_HANDLE_SIZE);
			vdi_mutexes_base +=
				PTHREAD_MUTEX_T_HANDLE_SIZE;
		}
	}
#endif
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_free_instances(struct file *filp, uint64_t *crash_inst_mask)
{
	hb_vpu_instance_list_t *vil, *n;
	hb_vpu_instance_pool_t *vip = NULL;
	void *vip_base;
	uint32_t instance_pool_size_per_core;
#ifdef USE_VPU_CLOSE_INSTANCE_ONCE_ABNORMAL_RELEASE
#else
	uint32_t core;
	unsigned long timeout = jiffies + HZ; /* PRQA S 5209 */
#endif
	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;
	uint32_t find_instance = 0U;
#ifdef USE_VPU_CLOSE_INSTANCE_ONCE_ABNORMAL_RELEASE
#else
	int32_t product_code;
	int32_t cmd_destroy = W5_DESTROY_INSTANCE;
	uint64_t inst_reg = W5_CMD_INSTANCE_INFO;
	uint64_t suc_reg = W5_RET_SUCCESS;
	uint64_t fail_reg = W5_RET_FAIL_REASON;
#endif

	if ((filp == NULL) || (filp->private_data == NULL)) {
		VPU_ERR("failed to free instances, filp is null.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_vpu_priv_t *)filp->private_data;
	dev = priv->vpu_dev;
	if (dev == NULL) {
		VPU_ERR("failed to free instances, dev is null.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}
	/* s_instance_pool.size assigned to the size of all core once call VDI_IOCTL_GET_INSTANCE_POOL by user. */
	instance_pool_size_per_core = (dev->instance_pool.size / MAX_NUM_VPU_CORE);

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(vil, n, &dev->inst_list_head, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		if (vil->filp == filp) {
			find_instance = 1U;
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			vip_base = (void *)(dev->instance_pool.base + /* PRQA S 1891 */
						((uint64_t)instance_pool_size_per_core * (uint64_t)vil->core_idx));
#ifdef SUPPORT_MULTI_INST_INTR
			VPU_INFO_DEV(dev->device, "vpu_free_instances detect instance crash instIdx=%d, "
				"coreIdx=%u, vip_base=%pK, instance_pool_size_per_core=%u, "
				"interrupt_flag=%d, pid=%d, tgid=%d, filp=%pK\n",
				vil->inst_idx, vil->core_idx,
				vip_base, instance_pool_size_per_core,
				dev->interrupt_flag[vil->inst_idx], current->pid, current->tgid, filp);
#else
			VPU_INFO_DEV(dev->device, "vpu_free_instances detect instance crash instIdx=%d, "
				"coreIdx=%u, vip_base=%pK, instance_pool_size_per_core=%u, "
				"interrupt_flag=%d, pid=%d, tgid=%d, filp=%pK\n",
				vil->inst_idx, vil->core_idx,
				vip_base, instance_pool_size_per_core,
				dev->interrupt_flag, current->pid, current->tgid, filp);
#endif
#ifdef USE_VPU_CLOSE_INSTANCE_ONCE_ABNORMAL_RELEASE
#else
			core = vil->core_idx;
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER); /* PRQA S 1006,0497,1843,1021 */
			if (product_code == WAVE420_CODE) {
				inst_reg = W4_INST_INDEX;
				cmd_destroy = W4_CMD_FINI_SEQ;
				suc_reg = W4_RET_SUCCESS;
				fail_reg = W4_RET_FAIL_REASON;
			}
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(inst_reg, /* PRQA S 0497,1021,1006 */
				(-1 << 16)|(vil->inst_idx&MAX_VPU_INSTANCE_IDX));
			VPU_ISSUE_COMMAND(product_code, vil->core_idx, cmd_destroy); /* PRQA S 0497,1006,1021 */
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			while (VPU_READL(WAVE_VPU_BUSY_STATUS)) { /* PRQA S 1006,0497,1843,1021 */
				if (time_after(jiffies, timeout)) { /* PRQA S 3469,3415,4394,4558,4115,1021,1020,2996 */
					VPU_ERR_DEV(dev->device, "Timeout to do command %d\n",
						cmd_destroy);
					break;
				}
			}
#ifdef SUPPORT_MULTI_INST_INTR
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			if (VPU_READL(suc_reg) == 0) { /* PRQA S 1006,0497,1843,1021 */
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				int32_t error = VPU_READL(fail_reg); /* PRQA S 1006,0497,1843,1021 */
				if (error == 0x1000 && product_code == WAVE521_CODE) {
					VPU_WRITEL(W5_QUERY_OPTION, GET_RESULT); /* PRQA S 0497,1021,1006 */
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					VPU_WRITEL(W5_CMD_INSTANCE_INFO, /* PRQA S 0497,1021,1006 */
						(-1 << 16)|(vil->inst_idx&MAX_VPU_INSTANCE_IDX));
					VPU_ISSUE_COMMAND(product_code, vil->core_idx, W5_QUERY); /* PRQA S 0497,1006,1021 */
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					while (VPU_READL(WAVE_VPU_BUSY_STATUS)) { /* PRQA S 1006,0497,1843,1021 */
						if (time_after(jiffies, timeout)) { /* PRQA S 3469,3415,4394,4558,4115,1021,1020,2996 */
							VPU_ERR_DEV(dev->device, "Timeout to do command %d\n",
								cmd_destroy);
							break;
						}
					}
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					VPU_WRITEL(W5_CMD_INSTANCE_INFO, /* PRQA S 0497,1021,1006 */
						(-1 << 16)|(vil->inst_idx&MAX_VPU_INSTANCE_IDX));
					VPU_ISSUE_COMMAND(product_code, vil->core_idx, /* PRQA S 0497,1006,1021 */
						W5_DESTROY_INSTANCE);
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					while (VPU_READL(WAVE_VPU_BUSY_STATUS)) { /* PRQA S 1006,0497,1843,1021 */
						if (time_after(jiffies, timeout)) { /* PRQA S 3469,3415,4394,4558,4115,1021,1020,2996 */
							VPU_ERR_DEV(dev->device, "Timeout to do command %d\n",
								W5_DESTROY_INSTANCE);
							break;
						}
					}
				} else {
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					VPU_ERR_DEV(dev->device, "Command %d failed [0x%x]\n", cmd_destroy, VPU_READL(fail_reg));
				}
			}
#endif
#endif
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			vip = (hb_vpu_instance_pool_t *) vip_base;
			if (vip != NULL) {
				/* only first 4 byte is key point(inUse of CodecInst in vpuapi)
				   to free the corresponding instance. */
				(void)memset((void *)&vip->codec_inst_pool[vil->inst_idx], 0x00, PTHREAD_MUTEX_T_HANDLE_SIZE);
				(void)vpu_free_lock(filp, dev, vip, vil->core_idx, vil->inst_idx);
#ifdef USE_VPU_CLOSE_INSTANCE_ONCE_ABNORMAL_RELEASE
				if (dev->inst_open_flag[vil->inst_idx] != 0) {
					osal_set_bit(vil->inst_idx, crash_inst_mask);
					osal_set_bit(vil->inst_idx, (volatile uint64_t *)dev->vpu_crash_inst_bitmap); /* PRQA S 4446 */
				}
#endif
			}
			vpu_clear_instance_status(dev, vil);
		}
	}

	// only for core 0?
	// Note clear the lock for the core index 0 during pre-initialzing.
	vip_base = (void *)(dev->instance_pool.base);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vip = (hb_vpu_instance_pool_t *) vip_base;
	if (find_instance == 1U && vip == NULL) {
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		(void)vpu_free_lock(filp, dev, vip, 0, 0);
	}
	return 0;
}

/* code review E1: No need to return value */
//coverity[HIS_metric_violation:SUPPRESS]
static void vpu_free_dma_list(struct file *filp,
				const hb_vpu_dev_t *dev)
{
	hb_vpu_ion_dma_list_t *dma_tmp, *dma_n;
	hb_vpu_phy_dma_list_t *phy_tmp, *phy_n;

	// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 ++
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(dma_tmp, dma_n, &dev->dma_data_list_head, list) {
		if (dma_tmp->filp == filp) {
			osal_list_del(&dma_tmp->list);
			ion_iommu_unmap_ion_fd(dev->device,
					&dma_tmp->ion_dma_data);
			osal_kfree((void *)dma_tmp);
		}
	}

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(phy_tmp, phy_n, &dev->phy_data_list_head, list) {
		if (phy_tmp->filp == filp) {
			osal_list_del(&phy_tmp->list);
			ion_iommu_unmap_ion_phys(dev->device,
					phy_tmp->iova, phy_tmp->size);
			osal_kfree((void *)phy_tmp);
		}
	}
	// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 --
}

/* code review E1: No need to return value */
static void vpu_free_drv_buffer(struct file *filp,
				hb_vpu_dev_t *dev)
{
	hb_vpu_drv_buffer_pool_t *pool, *n;
	hb_vpu_drv_buffer_t vb;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(pool, n, &dev->vbp_head, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		if (pool->filp == filp) {
			vb = pool->vb;
			if (vb.base != 0UL) {
				(void)vpu_free_dma_buffer(dev, &vb);
				osal_list_del(&pool->list);
				osal_kfree((void *)pool);
			}
		}
	}
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_free_buffers(struct file *filp)
{
	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	if (filp == NULL || filp->private_data == NULL) {
		VPU_ERR("failed to free vpu buffers, filp is null.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_vpu_priv_t *)filp->private_data;
	dev = priv->vpu_dev;

	if (dev == NULL) {
		VPU_ERR("failed to free vpu buffers, dev is null.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}

	vpu_free_dma_list(filp, dev);
	vpu_free_drv_buffer(filp, dev);

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	return 0;
}

#ifdef SUPPORT_MULTI_INST_INTR
static inline uint32_t vpu_filter_inst_idx(uint32_t reg_val)
{
	uint32_t inst_idx;
	uint32_t i;

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if (((reg_val >> i) & 0x01U) == 1U)
			break;
	}
	inst_idx = i;

	return inst_idx;
}

//coverity[HIS_metric_violation:SUPPRESS]
static uint32_t vpu_get_inst_idx(hb_vpu_dev_t * dev, uint32_t * reason,
			    uint32_t empty_inst, uint32_t done_inst, uint32_t seq_inst)
{
	uint32_t inst_idx;
	uint32_t reg_val;
	uint32_t int_reason;

	int_reason = *reason;
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "int_reason=0x%x, empty_inst=0x%x, done_inst=0x%x, seq_inst=0x%x\n",
		int_reason, empty_inst, done_inst, seq_inst);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	if (int_reason & (1U << INT_WAVE5_DEC_PIC)) {
		reg_val = (done_inst & MAX_VPU_INSTANCE_IDX);
		inst_idx = vpu_filter_inst_idx(reg_val);
		*reason = (1U << INT_WAVE5_DEC_PIC);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_DBG_DEV(dev->device, "inst[%02d] W5_RET_QUEUE_CMD_DONE_INST"
			" DEC_PIC reg_val=0x%x\n", inst_idx, reg_val);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
		return inst_idx;
	}

	if (int_reason & (1U << INT_WAVE5_BSBUF_EMPTY)) {
		reg_val = (empty_inst & MAX_VPU_INSTANCE_IDX);
		inst_idx = vpu_filter_inst_idx(reg_val);
		*reason = (1U << INT_WAVE5_BSBUF_EMPTY);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_DBG_DEV(dev->device, "inst[%02d] W5_RET_BS_EMPTY_INST"
			" BSBUF_EMPTY reg_val=0x%x\n", inst_idx, reg_val);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
		return inst_idx;
	}

	if (int_reason & (1U << INT_WAVE5_INIT_SEQ)) {
		reg_val = (seq_inst & MAX_VPU_INSTANCE_IDX);
		inst_idx = vpu_filter_inst_idx(reg_val);
		*reason = (1U << INT_WAVE5_INIT_SEQ);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_DBG_DEV(dev->device, "inst[%02d] W5_RET_QUEUE_CMD_DONE_INST"
				" INIT_SEQ reg_val=0x%x\n", inst_idx, reg_val);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
		return inst_idx;
	}

	if (int_reason & (1U << INT_WAVE5_ENC_SET_PARAM)) {
		reg_val = (seq_inst & MAX_VPU_INSTANCE_IDX);
		inst_idx = vpu_filter_inst_idx(reg_val);
		*reason = (1U << INT_WAVE5_ENC_SET_PARAM);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_DBG_DEV(dev->device, "inst[%02d] W5_RET_QUEUE_CMD_DONE_INST"
				" ENC_SET_PARAM reg_val=0x%x\n", inst_idx, reg_val);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
		return inst_idx;
	}

#ifdef SUPPORT_SOURCE_RELEASE_INTERRUPT
	if (int_reason & (1U << INT_WAVE5_ENC_SRC_RELEASE)) {
		reg_val = (done_inst & MAX_VPU_INSTANCE_IDX);
		inst_idx = vpu_filter_inst_idx(reg_val);
		*reason = (1 << INT_WAVE5_ENC_SRC_RELEASE);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
		VPU_DBG_DEV(dev->device, "inst[%02d] W5_RET_QUEUE_CMD_DONE_INST"
			"ENC_SRC_RELEASE reg_val=0x%x\n", inst_idx, reg_val);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
		return inst_idx;
	}
#endif

	inst_idx = MAX_VPU_INSTANCE_IDX;
	*reason = 0;
	VPU_ERR_DEV(dev->device, "UNKNOWN INTERRUPT REASON: %08x\n", int_reason);

	return inst_idx;
}
#endif

//coverity[HIS_metric_violation:SUPPRESS]
static irqreturn_t vpu_irq_handler(int32_t irq, void *dev_id) /* PRQA S 3206 */
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *) dev_id;

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	ai_mode = hobot_fun_ai_mode_available();
	if (ai_mode) {
		disable_irq_nosync((uint32_t)dev->irq);
		vpu_pac_irq_raise(VPU_IRQ, NULL);
	} else {
#else
		/* this can be removed. it also work in VPU_WaitInterrupt of API function */
		uint32_t core;
		uint32_t product_code;
#ifdef SUPPORT_MULTI_INST_INTR
		uint32_t intr_reason = 0;
		uint32_t intr_inst_index = 0;
		int32_t inst_arr[MAX_NUM_VPU_INSTANCE] = {0, };
		int32_t inst_reason[MAX_NUM_VPU_INSTANCE] = {0, };
		uint32_t idx, inst_cnt = 0;
#endif

#ifdef VPU_IRQ_CONTROL
		osal_spin_lock(&dev->irq_spinlock);
		disable_irq_nosync((uint32_t)dev->irq);
		dev->irq_trigger = 1;
		dev->vpu_irqenable = 0;
		osal_spin_unlock(&dev->irq_spinlock);
#endif

		for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
			if (dev->bit_fm_info[core].size == 0U) {
				/* it means that we didn't get an information the current core
				from API layer. No core activated. */
				VPU_ERR_DEV(dev->device, "bit_fm_info[core].size is zero\n");
				// here we suppose it only has one core
				osal_spin_lock(&dev->irq_spinlock);
				enable_irq(dev->irq);
				dev->irq_trigger = 0;
				dev->vpu_irqenable = 1;
				osal_spin_unlock(&dev->irq_spinlock);
				return IRQ_HANDLED;
			}
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER); /* PRQA S 1006,0497,1843,1021 */

			if (PRODUCT_CODE_W_SERIES(product_code)) { /* PRQA S 3469,1843 */
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				if (VPU_READL(WAVE_VPU_VPU_INT_STS) != 0U) { /* PRQA S 1006,0497,1843,1021 */
#ifdef SUPPORT_MULTI_INST_INTR
					uint32_t empty_inst;
					uint32_t done_inst;
					uint32_t seq_inst;
					uint32_t i, reason, reason_clr;
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					reason = VPU_READL(WAVE_VPU_INT_REASON);
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					empty_inst = VPU_READL(W5_RET_BS_EMPTY_INST); /* PRQA S 1006,0497,1843,1021 */
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					done_inst = VPU_READL(W5_RET_QUEUE_CMD_DONE_INST); /* PRQA S 1006,0497,1843,1021 */
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					seq_inst =	VPU_READL(W5_RET_SEQ_DONE_INSTANCE_INFO); /* PRQA S 1006,0497,1843,1021 */
					reason_clr	= reason;

					// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
					//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
					//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					VPU_DBG_DEV(dev->device, "vpu_irq_handler reason=0x%x, "
						"empty_inst=0x%x, done_inst=0x%x, other_inst=0x%x \n",
						reason, empty_inst, done_inst,
						seq_inst);
					// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

					for (i=0; i < MAX_NUM_VPU_INSTANCE; i++) {
						if (0U == empty_inst && 0U == done_inst && 0U == seq_inst)
							break;
						intr_reason = reason;
						if (intr_reason == 0U) {
							VPU_DBG_DEV(dev->device, "r=0x%x, e=0x%x, d=0x%x, o=0x%x \n",
								intr_reason, empty_inst, done_inst, seq_inst);
							break;
						}
						intr_inst_index =
							vpu_get_inst_idx(dev, &intr_reason,
								empty_inst, done_inst,
								seq_inst);
						if (intr_inst_index < MAX_NUM_VPU_INSTANCE) {
							if (intr_reason ==
								(1U << INT_WAVE5_BSBUF_EMPTY)) {
								empty_inst =
									empty_inst & ~((uint32_t)1U << intr_inst_index);
								//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS]
								VPU_WRITEL(W5_RET_BS_EMPTY_INST, /* PRQA S 0497,1021,1006 */
									empty_inst);
								if (0U == empty_inst) {
									reason &= ~(1U << INT_WAVE5_BSBUF_EMPTY);
								}
								// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
								//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
								//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS]
								//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS]
								VPU_DBG_DEV(dev->device, "inst[%02d] "
									"W5_RET_BS_EMPTY_INST Clear empty_inst=0x%x\n",
									intr_inst_index, empty_inst);
								// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
							}
							if (intr_reason == (1U << INT_WAVE5_DEC_PIC)) {
								done_inst = done_inst & ~((uint32_t)1U << intr_inst_index);
								//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS]
								VPU_WRITEL(W5_RET_QUEUE_CMD_DONE_INST, done_inst); /* PRQA S 0497,1021,1006 */
								if (0U == done_inst) {
									reason &= ~(1U << INT_WAVE5_DEC_PIC);
								}
								// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
								//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
								//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS]
								//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS]
								VPU_DBG_DEV(dev->device, "inst[%02d] "
									"W5_RET_QUEUE_CMD_DONE_INST Clear done_inst=0x%x\n",
									intr_inst_index, done_inst);
								// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
							}
							if ((intr_reason == (1U << INT_WAVE5_INIT_SEQ)) ||
								(intr_reason == (1U << INT_WAVE5_ENC_SET_PARAM))) {
								seq_inst = seq_inst & ~((uint32_t)1U << intr_inst_index);
								//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS]
								VPU_WRITEL(W5_RET_SEQ_DONE_INSTANCE_INFO, seq_inst); /* PRQA S 0497,1021,1006 */
								if (0U == seq_inst) {
									reason &= ~(1U << INT_WAVE5_INIT_SEQ
										| 1U << INT_WAVE5_ENC_SET_PARAM);
								}
								// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
								//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS]
								//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS]
								//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS]
								VPU_DBG_DEV(dev->device, "inst[%02d] "
									"W5_RET_SEQ_DONE_INSTANCE_INFO Clear seq_inst=0x%x\n",
									intr_inst_index, seq_inst);
								// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
							}
							inst_arr[intr_inst_index]++;
							inst_reason[intr_inst_index] = intr_reason;
							inst_cnt++;
						} else {
							VPU_DBG_DEV(dev->device, "inst[%02d] "
								"intr_inst_index is wrong\n", intr_inst_index);
						}
					}
					if (0U != reason)
						VPU_ERR_DEV(dev->device, "INTERRUPT REASON REMAINED: %08x\n", reason);
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					VPU_WRITEL(W5_VPU_INT_REASON_CLEAR, reason_clr); /* PRQA S 0497,1021,1006 */
#else
					dev->interrupt_reason =
						VPU_READL(WAVE_VPU_INT_REASON); /* PRQA S 1006,0497,1843,1021 */
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					VPU_WRITEL(WAVE_VPU_INT_REASON_CLEAR, /* PRQA S 0497,1021,1006 */
						dev->interrupt_reason);
#endif
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					VPU_WRITEL(WAVE_VPU_VINT_CLEAR, 0x1); /* PRQA S 0497,1021,1006 */
				} else {
					osal_spin_lock(&dev->irq_spinlock);
					if (dev->irq_trigger == 1) {
						enable_irq(dev->irq);
						dev->irq_trigger = 0;
						dev->vpu_irqenable = 1;
						VPU_DBG_DEV(dev->device, "VPU_INT_STS is zero, turn on irqenable.\n");
					}
					osal_spin_unlock(&dev->irq_spinlock);
				}
			} else {
				VPU_ERR_DEV(dev->device, "Unknown product id : %08x\n", product_code);
				continue;
			}
#ifdef SUPPORT_MULTI_INST_INTR
			for (idx = 0; idx < MAX_NUM_VPU_INSTANCE; idx++) {
				if (inst_arr[idx] > 0) {
					// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
					//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					VPU_DBG_DEV(dev->device, "inst[%02d] product: 0x%08x intr_reason: 0x%08x ignore_irq %d inst_cnt %d\n",
						idx, product_code, inst_reason[idx], dev->ignore_irq, inst_cnt);
					// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
				}
			}
#else
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_DBG_DEV(dev->device, "product: 0x%08x intr_reason: 0x%08llx\n",
				product_code, dev->interrupt_reason);
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
#endif
		}

		/* notify the interrupt to user space */
		if (dev->async_queue != NULL)
			kill_fasync(&dev->async_queue, SIGIO, POLL_IN);

#ifdef SUPPORT_MULTI_INST_INTR
		osal_spin_lock(&dev->irq_spinlock);
		for (idx = 0; idx < MAX_NUM_VPU_INSTANCE; idx++) {
			if ((inst_arr[idx] > 0) && (dev->ignore_irq == 0)) {
				// dev->interrupt_flag[idx] = 1;
				// osal_wake_up(&dev->interrupt_wait_q[idx]);

				osal_spin_lock(&dev->poll_int_wait_q[idx].lock);
				dev->poll_int_event[idx] += inst_arr[idx];
				dev->total_poll[idx] += inst_arr[idx];
				//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				VPU_DBG_DEV(dev->device, "inst[%02d] poll_int_event[%02d]=%lld, total_poll=%lld.\n",
					idx, idx, dev->poll_int_event[idx], dev->total_poll[idx]);
				osal_spin_unlock(&dev->poll_int_wait_q[idx].lock);
				osal_wake_up(&dev->poll_int_wait_q[idx]);
				if (!osal_fifo_is_full(&dev->interrupt_pending_q[idx])) {
					if (inst_reason[idx] == (1U << INT_WAVE5_ENC_PIC)) {
						uint32_t ll_intr_reason = ((uint32_t)1U <<
							INT_WAVE5_ENC_PIC);
						osal_fifo_in(&dev->interrupt_pending_q[idx],
							&ll_intr_reason, (uint32_t)sizeof(uint32_t));
					} else {
						osal_fifo_in(&dev->interrupt_pending_q[idx],
							&inst_reason[idx], (uint32_t)sizeof(uint32_t));
					}
					dev->interrupt_flag[idx] = 1;
					osal_wake_up(&dev->interrupt_wait_q[idx]);
				} else {
					VPU_ERR_DEV(dev->device, "inst[%02d] kfifo_is_full kfifo_count=%d \n",
						idx, osal_fifo_len(&dev->interrupt_pending_q[idx]));
				}

#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
				(void)vpu_prio_set_command_to_fw(dev->prio_queue);
#endif
			}

			if ((inst_arr[idx] > 0) &&
				((osal_test_bit(idx, (const volatile uint64_t*)dev->vpu_inst_bitmap) == 0) ||
				 (osal_test_bit(idx, (const volatile uint64_t*)dev->vpu_crash_inst_bitmap) == 1))) {	// need vpu_lock?
				dev->interrupt_flag[idx] = 0;
				inst_cnt--;
				if (inst_cnt <= 0) {
					dev->ignore_irq = 1;
					VPU_DBG_DEV(dev->device, "set ignore_irq = 1\n");
				}
			}
		}

		if (dev->ignore_irq != 0) {
			if (dev->vpu_irqenable == 0) {
				enable_irq(dev->irq);
			}
			dev->irq_trigger = 0;
			dev->vpu_irqenable = 1;
			dev->ignore_irq = 0;
			VPU_DBG_DEV(dev->device, "turn on irqenable\n");
		}
		osal_spin_unlock(&dev->irq_spinlock);
#else
		osal_spin_lock(&dev->irq_spinlock);
		if (dev->ignore_irq == 0) {
			osal_spin_lock(&dev->poll_int_wait_q.lock);
			dev->poll_int_event++;
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_DBG_DEV(dev->device, "vpu_irq_handler poll_int_event=%lld.\n", dev->poll_int_event);
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
			dev->total_poll++;
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_DBG_DEV(dev->device, "vpu_irq_handler total_poll=%lld.\n", dev->total_poll);
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
			osal_spin_unlock(&dev->poll_int_wait_q.lock);
			osal_wake_up(&dev->poll_int_wait_q); /*PRQA S 3469*/
			dev->interrupt_flag = 1;
			osal_wake_up(&dev->interrupt_wait_q); /*PRQA S 3469*/
		} else {
			if (dev->irq_trigger == 1) {
				// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
				//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				VPU_DBG_DEV(dev->device, "vpu_irq_handler ignore irq.\n");
				// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
				enable_irq(dev->irq);
				dev->irq_trigger = 0;
				dev->interrupt_flag = 0;
				dev->ignore_irq = 0;
			}
		}
		osal_spin_unlock(&dev->irq_spinlock);
#endif
#endif
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	} //for else
#endif
	return IRQ_HANDLED;
}

static hb_vpu_driver_data_t vpu_drv_data = {
	.fw_name = "vpu.bin",
};

static const struct of_device_id vpu_of_match[] = {
	{
	 .compatible = "hobot,hobot_vpu",
	 .data = (const void *)&vpu_drv_data,
	 },
	{
	 .compatible = "cm, vpu",
	 .data = NULL,
	 },
	{}, /* PRQA S 1041 */
};

static int32_t vpu_parse_dts(const struct device_node *np, const hb_vpu_dev_t * dev)
{
	hb_vpu_platform_data_t *pdata = dev->plat_data;
	int32_t ret;
	if (np == NULL) {
		VPU_ERR_DEV(dev->device, "Invalid device node\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "ip_ver", &pdata->ip_ver);
	if (ret != 0) {
		pdata->ip_ver = 0U;
	}
	ret = of_property_read_u32(np, "clock_rate", &pdata->clock_rate);
	if (ret != 0) {
		pdata->clock_rate = 0U;
	}
	ret = of_property_read_u32(np, "min_rate", &pdata->min_rate);
	if (ret != 0) {
		pdata->min_rate = 0U;
	}

	return 0;
}

static const hb_vpu_driver_data_t *vpu_get_drv_data(const struct platform_device *pdev)
{
	const hb_vpu_driver_data_t *drv_data = NULL;

	if (pdev->dev.of_node != NULL) {
		const struct of_device_id *id;
		id = of_match_node(of_match_ptr(vpu_of_match), pdev->dev.of_node); /* PRQA S 3469 */
		if (id != NULL) {
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			drv_data = (const hb_vpu_driver_data_t *) id->data;
		} else {
			//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			drv_data = (const hb_vpu_driver_data_t *) /* PRQA S 0306 */
				platform_get_device_id(pdev)->driver_data; /* PRQA S 3469 */
		}
	}
	return drv_data;
}

static int32_t vpu_user_open_instance(hb_vpu_dev_t *dev, uint32_t core, uint32_t inst)
{
	int32_t ret;
	uint32_t val;
	unsigned long timeout = jiffies + (unsigned long)VPU_CREAT_INST_CHECK_TIMEOUT;

	ret = wave_issue_command(dev, core, inst, (uint32_t)W5_CREATE_INSTANCE, timeout);
	if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
		if (ret == (int32_t)VPUAPI_RET_TIMEOUT) {
			ret = (int32_t)RETCODE_VPU_RESPONSE_TIMEOUT;
		} else if (ret == (int32_t)VPUAPI_RET_INVALID_PARAM) {
			ret = (int32_t)RETCODE_INVALID_PARAM;
		} else {
			ret = (int32_t)RETCODE_FAILURE;
		}
		VPU_ERR_DEV(dev->device, "Failed to issue open instance command ret = %d\n", ret);
		return ret;
	}

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	val = VPU_READL(W5_RET_SUCCESS);
	if (val == 0U) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		val = VPU_READL(W5_RET_FAIL_REASON);

		if (vpu_check_is_decoder(dev, core, inst) == 0) {
			if (val != WAVE5_SYSERR_QUEUEING_FAIL)
				VPU_ERR_DEV(dev->device, "Failed to open encoder FAIL_REASON = 0x%x\n", val);
			if (val == 2U)
				ret = (int32_t)RETCODE_INVALID_SFS_INSTANCE;
			else if (val == WAVE5_SYSERR_WATCHDOG_TIMEOUT)
				ret = (int32_t)RETCODE_VPU_RESPONSE_TIMEOUT;
			else
				ret = (int32_t)RETCODE_FAILURE;
		} else {
			VPU_ERR_DEV(dev->device, "Failed to open decoder FAIL_REASON = 0x%x\n", val);
			ret = (int32_t)RETCODE_FAILURE;
		}
	} else {
		ret = (int32_t)RETCODE_SUCCESS;
	}

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_user_close_instance(hb_vpu_dev_t *dev, uint32_t core, uint32_t inst)
{
	int32_t ret = (int32_t)RETCODE_VPU_STILL_RUNNING;
	uint32_t val;
	unsigned char *codec_inst;
	unsigned long timeout = jiffies + (unsigned long)VPU_BUSY_CHECK_TIMEOUT;
	hb_vpu_instance_pool_t *vip = get_instance_pool_handle(dev, core);
	if (vip == NULL) {
		VPU_ERR_DEV(dev->device, "Invalid instance pool\n");
		return (int32_t)RETCODE_INVALID_PARAM;
	}

	while (ret == (int32_t)RETCODE_VPU_STILL_RUNNING) {
		ret = wave_issue_command(dev, core, inst, (uint32_t)W5_DESTROY_INSTANCE, timeout);
		if (ret != (int32_t)VPUAPI_RET_SUCCESS) {
			if (ret == (int32_t)VPUAPI_RET_TIMEOUT) {
				ret = (int32_t)RETCODE_VPU_RESPONSE_TIMEOUT;
			} else if (ret == (int32_t)VPUAPI_RET_INVALID_PARAM) {
				ret = (int32_t)RETCODE_INVALID_PARAM;
			} else {
				ret = (int32_t)RETCODE_FAILURE;
			}
			VPU_ERR_DEV(dev->device, "Failed to issue close instance command ret = %d\n", ret);
			return ret;
		}

		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		val = VPU_READL(W5_RET_SUCCESS);
		if (val == 0U) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			val = VPU_READL(W5_RET_FAIL_REASON);
			if (vpu_check_is_decoder(dev, core, inst) == 0) {
				if (val != WAVE5_SYSERR_VPU_STILL_RUNNING)
					VPU_ERR_DEV(dev->device, "Failed to close encoder FAIL_REASON = 0x%x\n", val);

				if (val == WAVE5_SYSERR_VPU_STILL_RUNNING) {
					ret = (int32_t)RETCODE_VPU_STILL_RUNNING;
				} else {
					ret = (int32_t)RETCODE_FAILURE;
				}
			} else {
				if (val != WAVE5_SYSERR_QUEUEING_FAIL && val != WAVE5_SYSERR_VPU_STILL_RUNNING)
					VPU_ERR_DEV(dev->device, "Failed to close decoder FAIL_REASON = 0x%x\n", val);

				if (val == WAVE5_SYSERR_VPU_STILL_RUNNING)
					ret = (int32_t)RETCODE_VPU_STILL_RUNNING;
				else if (val == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
					ret = (int32_t)RETCODE_MEMORY_ACCESS_VIOLATION;
				else if (val == WAVE5_SYSERR_WATCHDOG_TIMEOUT)
					ret = (int32_t)RETCODE_VPU_RESPONSE_TIMEOUT;
				else if (val == WAVE5_SYSERR_VLC_BUF_FULL)
					ret = (int32_t)RETCODE_VLC_BUF_FULL;
				else
					ret = (int32_t)RETCODE_FAILURE;
			}
		} else {
			ret = (int32_t)RETCODE_SUCCESS;
		}
	}
	codec_inst = &vip->codec_inst_pool[inst][0];
	// indicates codecMode in CodecInst structure in vpuapifunc.h
	memset((void*)(&codec_inst), 0x00, (size_t)4);
	return ret;
}

#define VPU_ENABLE_CLOCK
static int32_t vpu_open(struct inode *inode, struct file *filp) /* PRQA S 3673 */
{
#ifdef VPU_ENABLE_CLOCK
	int32_t ret = 0;
#endif
	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;
	uint64_t freq = 0;

	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	dev = osal_list_entry(inode->i_cdev, hb_vpu_dev_t, cdev); /* PRQA S 3432,2810,0306,0662,1021,0497 */
	if (dev == NULL) {
		VPU_ERR("failed to get vpu dev data\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	/* code review D6: Dynamic alloc memory to record different instance index */
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_vpu_priv_t *)osal_kzalloc(sizeof(hb_vpu_priv_t), OSAL_KMALLOC_ATOMIC);
	if (priv == NULL) {
		VPU_ERR_DEV(dev->device, "failed to alloc memory.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -ENOMEM;
	}

	osal_spin_lock(&dev->vpu_spinlock);
	if (dev->open_count == 0U) {
		dev->vpu_freq = vpu_clk_freq;

		/* power-domain might sleep or schedule, needs do spin_unlock */
		osal_spin_unlock(&dev->vpu_spinlock);
		hb_vpu_pm_ctrl(dev, 0, 1);
		osal_spin_lock(&dev->vpu_spinlock);
	}
	freq = dev->vpu_freq;
	osal_spin_unlock(&dev->vpu_spinlock);

#ifdef VPU_ENABLE_CLOCK
	ret = hb_vpu_clk_enable(dev, freq);
	if (ret != 0) {
		osal_kfree((void *)priv);
		VPU_ERR_DEV(dev->device, "failed to enable clock.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevClkEnableErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
#endif

	osal_spin_lock(&dev->vpu_spinlock);
	dev->open_count++;
	priv->vpu_dev = dev;
	priv->inst_index = MAX_VPU_INSTANCE_IDX;
	priv->is_irq_poll = 0;
	filp->private_data = (void *)priv;
	osal_spin_unlock(&dev->vpu_spinlock);

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return 0;
}

static int32_t vpu_alloc_physical_memory(struct file *filp,
				hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_drv_buffer_pool_t *vbp;
	hb_vpu_drv_buffer_t tmp;
	int32_t ret;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&tmp,
			(const void __user*)arg,
			sizeof(hb_vpu_drv_buffer_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to copy from user.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	ret = osal_sema_down_interruptible(&dev->vpu_sem);
	if (ret == 0) {
		/* code review D6: Dynamic alloc memory to record allocated physical memory */
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		vbp = (hb_vpu_drv_buffer_pool_t *)osal_kzalloc(sizeof(*vbp), OSAL_KMALLOC_ATOMIC);
		if (vbp == NULL) {
			osal_sema_up(&dev->vpu_sem);
			VPU_ERR_DEV(dev->device, "failed to allocate memory.\n");
			vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -ENOMEM;
		}
		vbp->vb = tmp;

		ret = vpu_alloc_dma_buffer(dev, &(vbp->vb));
		if (ret != 0) {
			osal_kfree((void *)vbp);
			osal_sema_up(&dev->vpu_sem);
			VPU_ERR_DEV(dev->device, "failed to allocate dma buffer.\n");
			return -ENOMEM;
		}
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&(vbp->vb),
				sizeof(hb_vpu_drv_buffer_t));
		if (ret != 0) {
			(void)vpu_free_dma_buffer(dev, &(vbp->vb));
			osal_kfree((void *)vbp);
			osal_sema_up(&dev->vpu_sem);
			VPU_ERR_DEV(dev->device, "failed to copy to user.\n");
			vpu_send_diag_error_event((u16)EventIdVPUDevSetUserErr, (u8)ERR_SEQ_1, 0, __LINE__);
			return -EFAULT;
		}

		vbp->filp = filp;
		osal_spin_lock(&dev->vpu_spinlock);
		osal_list_add(&vbp->list, &dev->vbp_head);
		osal_spin_unlock(&dev->vpu_spinlock);

		osal_sema_up(&dev->vpu_sem);
	}

	return ret;
}

static int32_t vpu_free_physical_memory(
				hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_drv_buffer_pool_t *vbp, *n;
	hb_vpu_drv_buffer_t vb;
	int32_t ret;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&vb,
			(const void __user *) arg,
			sizeof(hb_vpu_drv_buffer_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to copy from user.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EACCES;
	}

	ret = osal_sema_down_interruptible(&dev->vpu_sem);
	if (ret == 0) {
		if (vb.base != 0UL) {
			(void)vpu_free_dma_buffer(dev, &vb);
		}

		osal_spin_lock(&dev->vpu_spinlock);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		osal_list_for_each_entry_safe(vbp, n, &dev->vbp_head, /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
			list) {
			if (vbp->vb.base == vb.base) {
				osal_list_del(&vbp->list);
				osal_kfree((void *)vbp);
				break;
			}
		}
		osal_spin_unlock(&dev->vpu_spinlock);

		osal_sema_up(&dev->vpu_sem);
	}

	return ret;
}

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
static int32_t vpu_get_reserved_memory_info(hb_vpu_dev_t *dev, u_long arg)
{
	int32_t ret = 0;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	if (s_video_memory.base != 0) {
		ret = osal_copy_to_app((void __user *)arg,
				(void *)&s_video_memory,
				sizeof(hb_vpu_drv_buffer_t));
		if (ret != 0) {
			VPU_ERR_DEV(dev->device, "failed to copy to user.\n");
			vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
			ret = -EFAULT;
		}
	} else {
		VPU_ERR_DEV(dev->device, "invalid memory base.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		ret = -EFAULT;
	}
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return ret;
}
#endif

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t calc_vpu_loading(hb_vpu_dev_t *dev, uint32_t intr_inst_index)
{
	osal_time_t ts;
	media_codec_context_t *context = &(dev->vpu_ctx[intr_inst_index].context);
	mc_h264_h265_output_frame_info_t *frame_infos =	&(dev->vpu_status[intr_inst_index].frame_info);

	osal_spin_lock(&dev->vpu_ctx_spinlock);
	if (context->encoder) {
		dev->venc_loading += (uint64_t)context->video_enc_params.width * (uint64_t)context->video_enc_params.height;
	} else {
		dev->vdec_loading += (uint64_t)frame_infos->display_height * (uint64_t)frame_infos->display_width;
	}

	osal_time_get_real(&ts);
	if ((uint64_t)ts.tv_sec >= (uint64_t)dev->vpu_loading_ts.tv_sec + dev->vpu_loading_setting) {
		dev->vpu_loading_ts = ts;
		dev->vpu_loading = dev->venc_loading * VPU_MAX_RADIO/VPU_MAX_VENC_CAPACITY/dev->vpu_loading_setting
			+ dev->vdec_loading * VPU_MAX_RADIO/VPU_MAX_VDEC_CAPACITY/dev->vpu_loading_setting;
		if (dev->vpu_loading > VPU_MAX_RADIO) {
			dev->vpu_loading = VPU_MAX_RADIO;
		}
		dev->venc_loading = 0;
		dev->vdec_loading = 0;
	}
	osal_spin_unlock(&dev->vpu_ctx_spinlock);
	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_wait_interrupt(hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_drv_intr_t info;
#ifdef SUPPORT_MULTI_INST_INTR
	uint32_t intr_inst_index;
	uint32_t intr_reason_in_q;
	uint32_t interrupt_flag_in_q;
#endif
#ifdef SUPPORT_TIMEOUT_RESOLUTION
	ktime_t kt;
#endif

	unsigned long flags_mp; /* PRQA S 5209 */
	int32_t ret;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&info, (const void __user *) arg,
			sizeof(hb_vpu_drv_intr_t));
	if (ret != 0 || info.intr_inst_index < 0 || info.intr_inst_index >= (int32_t)MAX_NUM_VPU_INSTANCE) {
		VPU_ERR_DEV(dev->device, "failed to copy from user or invalid params from user.\n");
		return -EFAULT;
	}
#ifdef SUPPORT_MULTI_INST_INTR
	intr_inst_index = (uint32_t)info.intr_inst_index;

	intr_reason_in_q = 0;
	interrupt_flag_in_q =
		osal_fifo_out_spinlocked(&dev->interrupt_pending_q[intr_inst_index],
				&intr_reason_in_q, (uint32_t)sizeof(uint32_t),
				&dev->irq_spinlock);
	if (interrupt_flag_in_q > 0U) {
		dev->interrupt_reason[intr_inst_index] =
			intr_reason_in_q;
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_DBG_DEV(dev->device, "inst[%02d] Interrupt Remain : "
			"intr_reason_in_q=0x%x, interrupt_flag_in_q=%d\n",
			intr_inst_index, intr_reason_in_q,
			interrupt_flag_in_q);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	} else {
#endif
#ifdef SUPPORT_MULTI_INST_INTR
#ifdef SUPPORT_TIMEOUT_RESOLUTION
	kt = ktime_set(0, info.timeout * TIMEOUT_MS_TO_NS);
	//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_9_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = wait_event_interruptible_hrtimeout /* PRQA S ALL */
		(dev->interrupt_wait_q[intr_inst_index],
		dev->interrupt_flag[intr_inst_index] != 0, kt);
#else
	ret = (int32_t) osal_wait_event_interruptible_timeout /* PRQA S ALL */
		(dev->interrupt_wait_q[intr_inst_index],
		dev->interrupt_flag[intr_inst_index] != 0,
		msecs_to_jiffies(info.timeout)); /* PRQA S 3432 */
#endif
#else
#ifdef SUPPORT_TIMEOUT_RESOLUTION
	kt = ktime_set(0, info.timeout * TIMEOUT_MS_TO_NS);
	ret = wait_event_interruptible_hrtimeout /* PRQA S ALL */
		(dev->interrupt_wait_q, dev->interrupt_flag != 0, kt);
#else
	ret = (int32_t)osal_wait_event_interruptible_timeout /* PRQA S ALL */
		(dev->interrupt_wait_q, dev->interrupt_flag != 0,
		msecs_to_jiffies(info.timeout)); /* PRQA S 3432 */
#endif
#endif
#ifdef SUPPORT_TIMEOUT_RESOLUTION
	if (ret == -ETIME) {
		return ret;
	}
#else
	if (ret == 0) {
		ret = -ETIME;
		return ret;
	}
#endif
#if 1
	if (signal_pending(current) != 0) {
		ret = -ERESTARTSYS;
		VPU_INFO_DEV(dev->device, "INSTANCE NO: [%02d] ERESTARTSYS\n", info.intr_inst_index);
		return ret;
	}
#endif

#ifdef SUPPORT_MULTI_INST_INTR
	intr_reason_in_q = 0;
	interrupt_flag_in_q =
		osal_fifo_out_spinlocked(&dev->interrupt_pending_q[intr_inst_index],
			&intr_reason_in_q, (uint32_t)sizeof(uint32_t),
			&dev->irq_spinlock);
	if (interrupt_flag_in_q > 0U) {
		dev->interrupt_reason[intr_inst_index] = intr_reason_in_q;
	} else {
		dev->interrupt_reason[intr_inst_index] = 0;
	}
#endif
#ifdef SUPPORT_MULTI_INST_INTR
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "inst[%02d] interrupt_flag_in_q(%d), s_interrupt_flag(%d),"
		  "reason(0x%08llx)\n", intr_inst_index, interrupt_flag_in_q,
		  dev->interrupt_flag[intr_inst_index],
		  dev->interrupt_reason[intr_inst_index]);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
#else
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "inst[%02d] s_interrupt_flag(%d), reason(0x%08llx)\n",
		info.intr_inst_index, dev->interrupt_flag, dev->interrupt_reason);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
#endif

#ifdef SUPPORT_MULTI_INST_INTR
	}
#endif

	osal_spin_lock_irqsave(&dev->irq_spinlock, (uint64_t *)&flags_mp);
#ifdef SUPPORT_MULTI_INST_INTR
	info.intr_reason = (int32_t)dev->interrupt_reason[intr_inst_index];
	dev->interrupt_flag[intr_inst_index] = 0;
	dev->interrupt_reason[intr_inst_index] = 0;
#else
	info.intr_reason = (int32_t)dev->interrupt_reason;
	dev->interrupt_flag = 0;
	dev->interrupt_reason = 0;
#endif

#ifdef VPU_IRQ_CONTROL
	if (dev->irq_trigger == 1) {
		enable_irq(dev->irq);
		dev->irq_trigger = 0;
		dev->vpu_irqenable = 1;
	}
#endif

	// PRQA S 1021,3473,1020,3432,2996,3200 ++
	osal_spin_lock_irqsave(&dev->poll_int_wait_q[info.intr_inst_index].lock, (uint64_t *)&flags_mp);
	// PRQA S 1021,3473,1020,3432,2996,3200 --
	dev->poll_int_event[info.intr_inst_index]--;
	dev->total_release[info.intr_inst_index]++;
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "inst[%02d] poll_int_event[%02d]=%lld, total_release=%lld.\n",
		info.intr_inst_index, info.intr_inst_index, dev->poll_int_event[info.intr_inst_index], dev->total_release[info.intr_inst_index]);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	osal_spin_unlock_irqrestore(&dev->poll_int_wait_q[info.intr_inst_index].lock, (uint64_t *)&flags_mp);
	osal_spin_unlock_irqrestore(&dev->irq_spinlock, (uint64_t *)&flags_mp);

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&info,
			sizeof(hb_vpu_drv_intr_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to copy to user.\n");
		return -EFAULT;
	}

	(void)calc_vpu_loading(dev, (uint32_t)info.intr_inst_index);

	return ret;
}

#ifdef USE_MUTEX_IN_KERNEL_SPACE
static int32_t vpu_vdi_lock(struct file *filp, hb_vpu_dev_t *dev, u_long arg)
{
	uint32_t mutex_type;
	int32_t ret = 0;

	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app(&mutex_type, (uint32_t *)arg,
			sizeof(uint32_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to copy from user.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	ret = vdi_lock_user(filp, dev, (hb_vpu_mutex_t)mutex_type);

	return ret;
}

static int32_t vpu_vdi_unlock(struct file *filp, hb_vpu_dev_t *dev, u_long arg)
{
	uint32_t mutex_type;
	int32_t ret = 0;

	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app(&mutex_type, (uint32_t *)arg,
			sizeof(uint32_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to copy from user.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	ret = vdi_unlock(dev, (hb_vpu_mutex_t)mutex_type);
	if (ret !=0) {
		vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}

	return ret;
}
#endif

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_set_clock_gate(const hb_vpu_dev_t *dev, u_long arg)
{
	uint32_t clkgate;
	int32_t ret;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	if (dev == NULL) {
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&clkgate, (const void __user *) arg,
			sizeof(uint32_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to copy from user.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
#ifdef VPU_SUPPORT_CLOCK_CONTROL
	if (clkgate) {
		ret = hb_vpu_clk_enable(dev, dev->vpu_freq);
		if (ret != 0) {
			VPU_ERR_DEV(dev->device, "Failed to enable vpu clock.\n");
			vpu_send_diag_error_event((u16)EventIdVPUDevClkEnableErr, (u8)ERR_SEQ_0, 0, __LINE__);
		}
	} else {
		ret = hb_vpu_clk_disable(dev);
		if (ret != 0) {
			VPU_ERR_DEV(dev->device, "Failed to disable vpu clock.\n");
			vpu_send_diag_error_event((u16)EventIdVPUDevClkDisableErr, (u8)ERR_SEQ_0, 0, __LINE__);
		}
	}
#endif
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_get_instance_pool(hb_vpu_dev_t *dev, u_long arg)
{
	int32_t ret;
	uint32_t size = 0U;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	ret = osal_sema_down_interruptible(&dev->vpu_sem);
	if (ret == 0) {
		if (dev->instance_pool.base != 0U) {
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			ret = (int32_t)osal_copy_to_app((void __user *)arg,
					(void *)&dev->instance_pool,
					sizeof(hb_vpu_drv_buffer_t));
			if (ret != 0) {
				ret = -EFAULT;
				vpu_send_diag_error_event((u16)EventIdVPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
				VPU_ERR_DEV(dev->device, "failed to copy from user.\n");
			}
		} else {
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			ret = (int32_t)osal_copy_from_app((void *)&dev->instance_pool,
					(const void __user *) arg,
					sizeof(hb_vpu_drv_buffer_t));
			if (ret == 0) {
				//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				size = PAGE_ALIGN(dev->instance_pool.size);
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
				//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				dev->instance_pool.base = (uint64_t)osal_malloc((uint64_t)size);/* PRQA S 3469,1840,1020,4491 */
				dev->instance_pool.phys_addr =
					dev->instance_pool.base;

				if (dev->instance_pool.base != 0U)
#else
				if (vpu_alloc_dma_buffer(dev, &dev->instance_pool, dev) == 0)
#endif
				{
					/*clearing memory */
					//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					(void)memset((void *)dev->instance_pool.base, 0x0, size); /* PRQA S 3469,1840,1020,4491 */
					//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					ret = (int32_t)osal_copy_to_app((void __user *)arg,
							(void *)&dev->instance_pool,
							sizeof(hb_vpu_drv_buffer_t));
					if (ret == 0) {
						/* success to get memory for instance pool */
						osal_sema_up(&dev->vpu_sem);
						return ret;
					} else {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
						//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
						osal_free((void *)dev->instance_pool.base);
#else
						(void)vpu_free_dma_buffer(dev, &dev->instance_pool);
#endif
						dev->instance_pool.base = 0UL;
					}
				}
				ret = -EFAULT;
				VPU_ERR_DEV(dev->device, "failed to allocate dma buffer.\n");
				vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
			} else {
				VPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
				vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
			}
		}
		osal_sema_up(&dev->vpu_sem);
	}
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_get_common_memory(hb_vpu_dev_t *dev, u_long arg)
{
	int32_t ret;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	if (dev->common_memory.base != 0U) {
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		ret = (int32_t)osal_copy_to_app((void __user *)arg,
				(void *)&dev->common_memory,
				sizeof(hb_vpu_drv_buffer_t));
		if (ret != 0) {
			ret = -EFAULT;
			VPU_ERR_DEV(dev->device, "failed to copy from user.\n");
			vpu_send_diag_error_event((u16)EventIdVPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		}
	} else {
		ret = -EFAULT;
		VPU_ERR_DEV(dev->device, "No available common buffer.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_open_instance(struct file *filp,
				hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_drv_inst_t inst_info;
	hb_vpu_instance_list_t *vil_tmp, *n;
	int32_t ret = 0;
	int32_t prio = 0;
	uint32_t found = 0U;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	VPU_INFO_DEV(dev->device, "[+]VDI_IOCTL_OPEN_INSTANCE\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_from_app((void *)&inst_info, (const void __user *) arg,
		sizeof(hb_vpu_drv_inst_t)) != 0UL) {
		VPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
	prio = inst_info.inst_open_count;
	if ((inst_info.core_idx >= MAX_NUM_VPU_CORE) || prio < (int32_t)PRIO_0 || prio > (int32_t)PRIO_31 ||
		(inst_info.inst_idx >= MAX_NUM_VPU_INSTANCE)) {
#else
	if ((inst_info.core_idx >= MAX_NUM_VPU_CORE) ||	(inst_info.inst_idx >= MAX_NUM_VPU_INSTANCE)) {
#endif
		VPU_ERR_DEV(dev->device, "Invalid instance id(%d) or core id(%d) or prio(%d).\n",
			inst_info.inst_idx, inst_info.core_idx, prio);
		vpu_send_diag_error_event((u16)EventIdVPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	ret = vpu_user_open_instance(dev, inst_info.core_idx, inst_info.inst_idx);
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "vpu_user_open_instance fail ret: %d\n", ret);
		return ret;
	}

	osal_spin_lock(&dev->vpu_spinlock);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(vil_tmp, n, /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
				 &dev->inst_list_head, list) {
		if ((vil_tmp->inst_idx == inst_info.inst_idx) && (vil_tmp->core_idx ==
			inst_info.core_idx)) {
			found = 1U;
			break;
		}
	}

	if (0U == found) {
		osal_spin_unlock(&dev->vpu_spinlock);
		(void)vpu_user_close_instance(dev, inst_info.core_idx, inst_info.inst_idx);
		VPU_ERR_DEV(dev->device, "Failed to find instance index %d in list.\n", inst_info.inst_idx);
		vpu_send_diag_error_event((u16)EventIdVPUDevInstIdxErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}

	/* counting the current open instance number */
	inst_info.inst_open_count = 0;
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(vil_tmp, n, &dev->inst_list_head, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		if (vil_tmp->core_idx == inst_info.core_idx)
			inst_info.inst_open_count++;
	}
	osal_spin_unlock(&dev->vpu_spinlock);

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_to_app((void __user *)arg, (void *)&inst_info,
			sizeof(hb_vpu_drv_inst_t)) != 0UL) {
		VPU_ERR_DEV(dev->device, "failed to copy to user.\n");
		(void)vpu_user_close_instance(dev, inst_info.core_idx, inst_info.inst_idx);
		vpu_send_diag_error_event((u16)EventIdVPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	osal_spin_lock(&dev->irq_spinlock);
#ifdef SUPPORT_MULTI_INST_INTR
	vpu_test_interrupt_flag_and_enableirq(dev, inst_info.inst_idx);
	osal_fifo_reset(&dev->interrupt_pending_q[inst_info.inst_idx]);
#else
	dev->interrupt_reason = 0;
#endif
	osal_spin_unlock(&dev->irq_spinlock);

	osal_spin_lock(&dev->vpu_spinlock);
	// only clear the last vpu_stats, the vpu_contex has been set.
	(void)memset((void *)&dev->vpu_status[inst_info.inst_idx], 0x00,
		sizeof(dev->vpu_status[inst_info.inst_idx]));

	/* flag just for that vpu is in opened or closed */
	dev->vpu_open_ref_count++;
	dev->inst_open_flag[inst_info.inst_idx] = 1;
#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
	dev->inst_prio[inst_info.inst_idx] = prio;
	dev->prio_flag = check_prio_flag(dev);
#endif
	osal_spin_unlock(&dev->vpu_spinlock);

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	VPU_INFO_DEV(dev->device, "[-]VDI_IOCTL_OPEN_INSTANCE core_idx[%d] "
		"inst[%02d], prio=%d, s_vpu_open_ref_count=%d, inst_open_count=%d\n",
		(int32_t)inst_info.core_idx,
		(int32_t)inst_info.inst_idx, prio,
		dev->vpu_open_ref_count,
		inst_info.inst_open_count);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_close_instance(hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_drv_inst_t inst_info;
	hb_vpu_instance_list_t *vil, *n;
	uint32_t found = 0;
	int32_t ret = 0;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	VPU_INFO_DEV(dev->device, "[+]VDI_IOCTL_CLOSE_INSTANCE\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_from_app((void *)&inst_info, (const void __user *) arg,
		sizeof(hb_vpu_drv_inst_t)) != 0UL) {
		VPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	if ((inst_info.core_idx >= MAX_NUM_VPU_CORE) ||
		(inst_info.inst_idx >= MAX_NUM_VPU_INSTANCE)) {
		VPU_ERR_DEV(dev->device, "Invalid instance id(%d) or core id(%d).\n",
			inst_info.inst_idx, inst_info.core_idx);
		vpu_send_diag_error_event((u16)EventIdVPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	ret = vpu_user_close_instance(dev, inst_info.core_idx, inst_info.inst_idx);
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "vpu_user_close_instance fail ret: %d\n", ret);
		// return -EINVAL;
	}

	osal_spin_lock(&dev->vpu_spinlock);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(vil, n, &dev->inst_list_head, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		if ((vil->inst_idx == inst_info.inst_idx) &&
			(vil->core_idx == inst_info.core_idx)) {
			found = 1;
			break;
		}
	}

	if (0U == found) {
		osal_spin_unlock(&dev->vpu_spinlock);
		VPU_ERR_DEV(dev->device, "Failed to find instance index %d.\n", inst_info.inst_idx);
		vpu_send_diag_error_event((u16)EventIdVPUDevInstIdxErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}

#ifdef SUPPORT_MULTI_INST_INTR
	osal_fifo_reset(&dev->interrupt_pending_q[inst_info.inst_idx]);
#endif

	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev->poll_event[inst_info.inst_idx] = LLONG_MIN;
	osal_wake_up(&dev->poll_wait_q[inst_info.inst_idx]); /*PRQA S 3469*/
	osal_wake_up(&dev->poll_int_wait_q[inst_info.inst_idx]); /*PRQA S 3469*/

	/* flag just for that vpu is in opened or closed */
	dev->vpu_open_ref_count--;
	inst_info.inst_open_count = dev->vpu_open_ref_count;
	dev->inst_open_flag[inst_info.inst_idx] = 0;
#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
	if (dev->vpu_open_ref_count == 0) dev->first_flag = 0;
	dev->inst_prio[inst_info.inst_idx] = -1;
	dev->prio_flag = check_prio_flag(dev);
	(void)vpu_prio_reset_fifo(dev, inst_info.inst_idx);
#endif
	osal_spin_unlock(&dev->vpu_spinlock);

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_to_app((void __user *)arg, (void *)&inst_info, sizeof(hb_vpu_drv_inst_t)) != 0UL) {
		vpu_send_diag_error_event((u16)EventIdVPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	VPU_INFO_DEV(dev->device, "[-]VDI_IOCTL_CLOSE_INSTANCE core[%d] "
		"inst[%02d], s_vpu_open_ref_count=%d, inst_open_count=%d\n",
		(int32_t)inst_info.core_idx,
		(int32_t)inst_info.inst_idx,
		dev->vpu_open_ref_count,
		inst_info.inst_open_count);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_get_instance_num(hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_drv_inst_t inst_info;
	int32_t ret;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "[+]VDI_IOCTL_GET_INSTANCE_NUM\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&inst_info,
			(const void __user *) arg,
			sizeof(hb_vpu_drv_inst_t));
	if (ret != 0) {
		ret = -EFAULT;
		VPU_ERR_DEV(dev->device, "failed to copy from user.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return ret;
	}

	osal_spin_lock(&dev->vpu_spinlock);
	inst_info.inst_open_count = dev->vpu_open_ref_count;
	osal_spin_unlock(&dev->vpu_spinlock);

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&inst_info,
			sizeof(hb_vpu_drv_inst_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to copy to user.\n");
		ret = -EFAULT;
		vpu_send_diag_error_event((u16)EventIdVPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "VDI_IOCTL_GET_INSTANCE_NUM core_idx=%d, "
		"inst_idx=%d, open_count=%d\n",
		(int32_t)inst_info.core_idx,
		(int32_t)inst_info.inst_idx,
		inst_info.inst_open_count);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_get_register_info(const hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_drv_buffer_t reg_buf;
	int32_t ret;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "[+]VDI_IOCTL_GET_REGISTER_INFO\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	(void)memset((void *)&reg_buf, 0x00, sizeof(reg_buf));
	reg_buf.phys_addr = dev->vpu_mem->start;
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	reg_buf.virt_addr = (uint64_t)dev->regs_base;
	reg_buf.size = (uint32_t)resource_size(dev->vpu_mem);
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&reg_buf,
			sizeof(hb_vpu_drv_buffer_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to copy to user.\n");
		ret = -EFAULT;
		vpu_send_diag_error_event((u16)EventIdVPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "[-]VDI_IOCTL_GET_REGISTER_INFO "
		"vpu_register.phys_addr==0x%llx, s_vpu_register.virt_addr=0x%llx,"
		"s_vpu_register.size=%d\n", reg_buf.phys_addr,
		reg_buf.virt_addr, reg_buf.size);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

static void vpu_instance_status_display(hb_vpu_dev_t *dev)
{
	uint64_t all_alloc_inst_mask = 0, all_crash_inst_mask = 0, all_free_inst_mask = 0;
	osal_spin_lock(&dev->vpu_spinlock);
	all_alloc_inst_mask = *dev->vpu_inst_bitmap;
	all_crash_inst_mask = *dev->vpu_crash_inst_bitmap;
	all_free_inst_mask = ((1ULL << MAX_NUM_VPU_INSTANCE) - 1) & (~all_alloc_inst_mask);
	osal_spin_unlock(&dev->vpu_spinlock);
	VPU_DBG_DEV(dev->device, "VPU INST STAT (free=0x%llx, alloc=0x%llx, crash=0x%llx)\n",
		all_free_inst_mask, all_alloc_inst_mask, all_crash_inst_mask);
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_alloc_instance_id(struct file *filp,
				hb_vpu_dev_t *dev, u_long arg)
{
	uint32_t inst_index;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_priv_t *priv = (hb_vpu_priv_t *)filp->private_data;
	hb_vpu_instance_list_t *vil, *vil_tmp, *n;
	hb_vpu_drv_inst_t inst_info;
	int32_t ret = 0;
	uint64_t timeout = jiffies + VPU_GET_INST_SEM_TIMEOUT;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "[+]VDI_IOCTL_ALLOCATE_INSTANCE_ID\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_from_app((void *)&inst_info, (const void __user *) arg,
		sizeof(hb_vpu_drv_inst_t)) != 0UL) {
		VPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	ret = osal_sema_down_timeout(&dev->vpu_free_inst_sem, (uint32_t)timeout);
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "Fail to get inst sema fail(%d).\n", ret);
		vpu_instance_status_display(dev);
		return -ETIME;
	}
	osal_spin_lock(&dev->vpu_spinlock);
	inst_index = (uint32_t)osal_find_first_zero_bit((const uint64_t *)dev->vpu_inst_bitmap,
					MAX_NUM_VPU_INSTANCE);
	if (inst_index >= MAX_NUM_VPU_INSTANCE) {
		osal_sema_up(&dev->vpu_free_inst_sem);		// Notes: should never arrive here
		osal_spin_unlock(&dev->vpu_spinlock);
		VPU_ERR_DEV(dev->device, "No available id space.\n");
		vpu_instance_status_display(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOMEM;
	}
	osal_set_bit(inst_index, (volatile uint64_t *)dev->vpu_inst_bitmap); /* PRQA S 4446 */
	dev->poll_event[inst_index] = 0;
	priv->inst_index = inst_index; // it's useless
	inst_info.inst_idx = inst_index;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(vil_tmp, n, /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		&dev->inst_list_head, list) {
		if (vil_tmp->inst_idx == inst_index) {
			(void)osal_test_and_clear_bit(inst_index, (volatile uint64_t *)dev->vpu_inst_bitmap); /* PRQA S 4446 */
			osal_sema_up(&dev->vpu_free_inst_sem);
			osal_spin_unlock(&dev->vpu_spinlock);
			VPU_ERR_DEV(dev->device, "Failed to allocate instance id due to existing id(%d)\n",
				(int32_t)inst_index);
			vpu_send_diag_error_event((u16)EventIdVPUDevInstExistErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EINVAL;
		}
	}

	/* code review D6: Dynamic alloc memory to record instance information */
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vil = (hb_vpu_instance_list_t *)osal_kzalloc(sizeof(*vil), OSAL_KMALLOC_ATOMIC);
	if (vil == NULL) {
		(void)osal_test_and_clear_bit(inst_index, (volatile uint64_t *)dev->vpu_inst_bitmap); /* PRQA S 4446 */
		osal_sema_up(&dev->vpu_free_inst_sem);
		osal_spin_unlock(&dev->vpu_spinlock);
		VPU_ERR_DEV(dev->device, "Failed to allocate memory\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOMEM;
	}

	vil->inst_idx = inst_index;
	vil->core_idx = inst_info.core_idx;
	vil->filp = filp;

	osal_list_add(&vil->list, &dev->inst_list_head);
	osal_spin_unlock(&dev->vpu_spinlock);

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_to_app((void __user *)arg, (void *)&inst_info,
			sizeof(inst_info)) != 0UL) {
		ret = -EFAULT;
		osal_spin_lock(&dev->vpu_spinlock);
		osal_list_del(&vil->list);
		osal_kfree(vil);
		(void)osal_test_and_clear_bit(inst_index, (volatile uint64_t *)dev->vpu_inst_bitmap); /* PRQA S 4446 */
		osal_sema_up(&dev->vpu_free_inst_sem);
		osal_spin_unlock(&dev->vpu_spinlock);
		VPU_ERR_DEV(dev->device, "failed to copy from user.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return ret;
	}

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "[-]VDI_IOCTL_ALLOCATE_INSTANCE_ID id = [%02d]\n", inst_index);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_free_instance_id(struct file *filp,
				hb_vpu_dev_t *dev, u_long arg)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_priv_t *priv = (hb_vpu_priv_t *)filp->private_data;
	hb_vpu_drv_inst_t inst_info;
	hb_vpu_instance_list_t *vil, *n;
	uint32_t found = 0;
	int32_t ret = 0;
	unsigned long flags_mp;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "[+]VDI_IOCTL_FREE_INSTANCE_ID\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_from_app((void *)&inst_info, (const void __user *)arg,
			sizeof(hb_vpu_drv_inst_t)) != 0UL) {
		VPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	if ((inst_info.inst_idx >= MAX_NUM_VPU_INSTANCE) ||
		(inst_info.core_idx >= MAX_NUM_VPU_CORE)) {
		VPU_ERR_DEV(dev->device, "Invalid instance id(%d) or core id(%d).\n",
			inst_info.inst_idx, inst_info.core_idx);
		vpu_send_diag_error_event((u16)EventIdVPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	osal_spin_lock(&dev->vpu_spinlock);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(vil, n, &dev->inst_list_head, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		if ((vil->inst_idx == inst_info.inst_idx) &&
			(vil->core_idx == inst_info.core_idx)) {
			osal_list_del(&vil->list);
			osal_kfree((void *)vil);
			found = 1;
			break;
		}
	}

	if (0U == found) {
		osal_spin_unlock(&dev->vpu_spinlock);
		VPU_ERR_DEV(dev->device, "Failed to find instance index %d.\n", inst_info.inst_idx);
		vpu_send_diag_error_event((u16)EventIdVPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	osal_clear_bit(inst_info.inst_idx, (volatile uint64_t *)dev->vpu_inst_bitmap); /* PRQA S 4446 */
	osal_sema_up(&dev->vpu_free_inst_sem);
	priv->inst_index = MAX_VPU_INSTANCE_IDX;
	(void)memset((void *)&dev->vpu_ctx[inst_info.inst_idx], 0x00,
		sizeof(dev->vpu_ctx[inst_info.inst_idx]));
	(void)memset((void *)&dev->vpu_status[inst_info.inst_idx], 0x00,
		sizeof(dev->vpu_status[inst_info.inst_idx]));
	osal_spin_unlock(&dev->vpu_spinlock);

	osal_spin_lock_irqsave(&dev->irq_spinlock, (uint64_t *)&flags_mp);
	vpu_test_interrupt_flag_and_enableirq(dev, inst_info.inst_idx);
	osal_spin_unlock_irqrestore(&dev->irq_spinlock, (uint64_t *)&flags_mp);

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "[-]VDI_IOCTL_FREE_INSTANCE_ID clear id = [%02d]\n", inst_info.inst_idx);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_poll_wait_instance(struct file *filp,
				hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_drv_intr_t info;
	hb_vpu_priv_t *priv;
	uint32_t intr_inst_index;
	int32_t ret;
	unsigned long flags_mp; /* PRQA S 5209 */

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&info, (const void __user *) arg,
			sizeof(hb_vpu_drv_intr_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "VDI_IOCTL_POLL_WAIT_INSTANCE copy from user fail.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	intr_inst_index = (uint32_t)info.intr_inst_index;
	if (intr_inst_index >= MAX_NUM_VPU_INSTANCE) {
		VPU_ERR_DEV(dev->device, "Invalid instance no %d.\n", intr_inst_index);
		vpu_send_diag_error_event((u16)EventIdVPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	ret = osal_sema_down_interruptible(&dev->vpu_sem);
	if (ret == 0) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		priv = (hb_vpu_priv_t *)filp->private_data;
		if (info.intr_reason == 0) {
			osal_spin_lock(&dev->poll_wait_q[intr_inst_index].lock);
			priv->inst_index = intr_inst_index;
			priv->is_irq_poll = 0;
			osal_spin_unlock(&dev->poll_wait_q[intr_inst_index].lock);
		} else if (info.intr_reason == (int32_t)VPU_INST_INTERRUPT) {
			// PRQA S 1021,3473,1020,3432,2996,3200 ++
			osal_spin_lock_irqsave(&dev->poll_int_wait_q[intr_inst_index].lock, (uint64_t *)&flags_mp);
			// PRQA S 1021,3473,1020,3432,2996,3200 --
			priv->inst_index = intr_inst_index;
			priv->is_irq_poll = 1;
			osal_spin_unlock_irqrestore(&dev->poll_int_wait_q[intr_inst_index].lock, (uint64_t *)&flags_mp);
		} else if ((info.intr_reason == (int32_t)VPU_ENC_PIC_DONE) ||
			(info.intr_reason == (int32_t)VPU_DEC_PIC_DONE) ||
			(info.intr_reason == (int32_t)VPU_INST_CLOSED)) {
			osal_spin_lock(&dev->poll_wait_q[intr_inst_index].lock);
			dev->poll_event[intr_inst_index]++;
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_DBG_DEV(dev->device, "ioctl poll wait event dev->poll_event[%u]=%lld.\n",
				intr_inst_index, dev->poll_event[intr_inst_index]);
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
			osal_spin_unlock(&dev->poll_wait_q[intr_inst_index].lock);
			osal_wake_up(&dev->poll_wait_q[intr_inst_index]); /*PRQA S 3469*/
		} else {
			VPU_ERR_DEV(dev->device, "VDI_IOCTL_POLL_WAIT_INSTANCE invalid instance reason"
				"(%d) or index(%d).\n",
				info.intr_reason, intr_inst_index);
			vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
			ret = -EINVAL;
		}
		osal_sema_up(&dev->vpu_sem);
	}

	return ret;
}

static int32_t vpu_set_ctx_info(hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_ctx_info_t *info;
	uint32_t inst_index;
	int32_t ret;
	/* code review D6: Dynamic alloc memory to copy large context */
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	info = (hb_vpu_ctx_info_t *)osal_kzalloc(sizeof(hb_vpu_ctx_info_t), OSAL_KMALLOC_ATOMIC);
	if (info == NULL) {
		VPU_ERR_DEV(dev->device, "failed to allocate ctx info.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOMEM;
	}

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)info, (const void __user *) arg,
				sizeof(hb_vpu_ctx_info_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "VDI_IOCTL_SET_CTX_INFO copy from user fail.\n");
		osal_kfree((void *)info);
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	inst_index = (uint32_t)info->context.instance_index;
	if (inst_index >= MAX_NUM_VPU_INSTANCE) {
		VPU_ERR_DEV(dev->device, "Invalid instance index %d.\n", inst_index);
		osal_kfree((void *)info);
		vpu_send_diag_error_event((u16)EventIdVPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	osal_spin_lock(&dev->vpu_ctx_spinlock);
	dev->vpu_ctx[inst_index] = *info;
	osal_spin_unlock(&dev->vpu_ctx_spinlock);
	osal_kfree((void *)info);

	return ret;
}

static int32_t vpu_set_status_info(hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_status_info_t info;
	uint32_t inst_index;
	int32_t ret;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&info, (const void __user *) arg,
				sizeof(hb_vpu_status_info_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "VDI_IOCTL_SET_STATUS_INFO copy from user fail.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	inst_index = info.inst_idx;
	if (inst_index >= MAX_NUM_VPU_INSTANCE) {
		VPU_ERR_DEV(dev->device, "Invalid instance index %d.\n", inst_index);
		vpu_send_diag_error_event((u16)EventIdVPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	osal_spin_lock(&dev->vpu_ctx_spinlock);
	dev->vpu_status[inst_index] = info;
	osal_spin_unlock(&dev->vpu_ctx_spinlock);

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_do_map_ion_fd(struct file *filp,
				hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_ion_fd_map_t dma_map;
	hb_vpu_ion_dma_list_t *ion_dma_data, *vil_tmp, *n;
	int32_t ret = 0;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&dma_map, (const void __user *) arg,
				sizeof(hb_vpu_ion_fd_map_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "VDI_IOCTL_MAP_ION_FD copy from user fail.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	/* code review D6: Dynamic alloc memory to record mapped ion fd */
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ion_dma_data = (hb_vpu_ion_dma_list_t *)osal_kzalloc(sizeof(*ion_dma_data), OSAL_KMALLOC_ATOMIC);
	if (ion_dma_data == NULL) {
		VPU_ERR_DEV(dev->device, "failed to allocate memory.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOMEM;
	}

	osal_spin_lock(&dev->vpu_spinlock);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(vil_tmp, n, /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		&dev->dma_data_list_head, list) {
		if ((vil_tmp->fd == dma_map.fd) &&
			(vil_tmp->filp == filp)) {
			osal_kfree((void *)ion_dma_data);
			VPU_ERR_DEV(dev->device, "Failed to map ion handle due to same fd(%d), "
				"same filp(0x%pK) and same tgid(%d).\n",
				dma_map.fd, filp, current->tgid);
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			osal_list_for_each_entry_safe(vil_tmp, n, &dev->dma_data_list_head, list) {
				VPU_ERR_DEV(dev->device, "Existing List items fd(%d), "
					"filp(%pK).\n", vil_tmp->fd, vil_tmp->filp);
			}
			osal_spin_unlock(&dev->vpu_spinlock);
			vpu_send_diag_error_event((u16)EventIdVPUDevDmaMapExistErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EINVAL;
		}
	}
	osal_spin_unlock(&dev->vpu_spinlock);

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (ion_iommu_map_ion_fd(dev->device,
		&ion_dma_data->ion_dma_data, dev->vpu_ion_client, dma_map.fd, IOMMU_READ | IOMMU_WRITE) < 0) {
		osal_kfree((void *)ion_dma_data);
		VPU_ERR_DEV(dev->device, "Failed to map ion handle.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevDmaMapExistErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EFAULT;
	}
	ion_dma_data->fd = dma_map.fd;
	ion_dma_data->filp = filp;

	dma_map.iova = ion_dma_data->ion_dma_data.dma_addr;
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&dma_map,
			sizeof(dma_map));
	if (ret != 0) {
		ion_iommu_unmap_ion_fd(dev->device,
			&ion_dma_data->ion_dma_data);
		osal_kfree((void *)ion_dma_data);
		VPU_ERR_DEV(dev->device, "failed to copy to user\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevSetUserErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return EFAULT;
	}
	osal_spin_lock(&dev->vpu_spinlock);
	osal_list_add(&ion_dma_data->list, &dev->dma_data_list_head);
	osal_spin_unlock(&dev->vpu_spinlock);

	return ret;
}

static int32_t vpu_map_ion_fd(struct file *filp,
				hb_vpu_dev_t *dev, u_long arg)
{
	int32_t ret;

	ret = osal_sema_down_interruptible(&dev->vpu_sem);
	if (ret == 0) {
		ret = vpu_do_map_ion_fd(filp, dev, arg);
		osal_sema_up(&dev->vpu_sem);
	}

	return ret;
}

static int32_t vpu_unmap_ion_fd(struct file *filp,
				hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_ion_fd_map_t dma_map;
	hb_vpu_ion_dma_list_t *vil_tmp, *n;
	int32_t found = 0;
	int32_t ret;

	ret = osal_sema_down_interruptible(&dev->vpu_sem);
	if (ret == 0) {
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		ret = (int32_t)osal_copy_from_app((void *)&dma_map, (const void __user *) arg,
					sizeof(dma_map));
		if (ret != 0) {
			VPU_ERR_DEV(dev->device, "VDI_IOCTL_UNMAP_ION_FD copy from user fail.\n");
			osal_sema_up(&dev->vpu_sem);
			vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EFAULT;
		}
		osal_spin_lock(&dev->vpu_spinlock);
		// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 ++
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		osal_list_for_each_entry_safe(vil_tmp, n, &dev->dma_data_list_head, list) {
			if ((vil_tmp->fd == dma_map.fd) &&
				(vil_tmp->filp == filp)) {
				osal_list_del(&vil_tmp->list);
				found = 1;
				break;
			}
		}
		// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 --
		osal_spin_unlock(&dev->vpu_spinlock);

		if (0 == found) {
			ret = -EINVAL;
			VPU_ERR_DEV(dev->device, "failed to find ion fd.\n");
			vpu_send_diag_error_event((u16)EventIdVPUDevDmaMapFindErr, (u8)ERR_SEQ_0, 0, __LINE__);
		} else {
			ion_iommu_unmap_ion_fd(dev->device,
				&vil_tmp->ion_dma_data);
			osal_kfree((void *)vil_tmp);
		}
		osal_sema_up(&dev->vpu_sem);
	}

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_do_map_ion_phy(struct file *filp,
				hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_ion_phys_map_t dma_map;
	hb_vpu_phy_dma_list_t *phy_dma_data, *vil_tmp, *n;
	int32_t ret = 0;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&dma_map, (const void __user *) arg,
				sizeof(hb_vpu_ion_phys_map_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "VDI_IOCTL_MAP_ION_PHYS copy from user fail.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	osal_spin_lock(&dev->vpu_spinlock);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(vil_tmp, n, /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		&dev->phy_data_list_head, list) {
		if ((vil_tmp->phys_addr == dma_map.phys_addr) &&
			(vil_tmp->size == dma_map.size) &&
			(vil_tmp->filp == filp)) {
			osal_spin_unlock(&dev->vpu_spinlock);
			VPU_ERR_DEV(dev->device, "Failed to map ion phy due to same phys(0x%llx) "
				"same size(%d) and same filp(0x%pK)\n.",
				dma_map.phys_addr, vil_tmp->size, filp);
			vpu_send_diag_error_event((u16)EventIdVPUDevDmaMapExistErr, (u8)ERR_SEQ_1, 0, __LINE__);
			return -EINVAL;
		}
	}
	osal_spin_unlock(&dev->vpu_spinlock);

#ifdef RESERVED_WORK_MEMORY
#if defined(CONFIG_HOBOT_FPGA_J5) || defined(CONFIG_HOBOT_J5) || defined(CONFIG_HOBOT_FPGA_HAPS_J5)
	if ((dma_map.phys_addr >= (dev->codec_mem_reserved.end)) || (dma_map.phys_addr < dev->codec_mem_reserved.start)) {
#else
	if ((dma_map.phys_addr >= (dev->codec_mem_reserved2.base + dev->codec_mem_reserved2.size)) || (dma_map.phys_addr < dev->codec_mem_reserved2.base)) {
#endif
#endif
		if (ion_check_in_heap_carveout(dma_map.phys_addr, dma_map.size) < 0) {
			VPU_ERR_DEV(dev->device, "Invalid ion physical address(addr=0x%llx, size=%u), not in ion range.\n",
				dma_map.phys_addr, dma_map.size);
			vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EINVAL;
		}
#ifdef RESERVED_WORK_MEMORY
	}
#endif

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (ion_iommu_map_ion_phys(dev->device,
		dma_map.phys_addr, dma_map.size, &dma_map.iova, IOMMU_READ | IOMMU_WRITE) < 0) {
		VPU_ERR_DEV(dev->device, "Failed to map ion phys.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevIonMapErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	/* code review D6: Dynamic alloc memory to record mapped physical address */
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	phy_dma_data = (hb_vpu_phy_dma_list_t *)osal_kzalloc(sizeof(*phy_dma_data), OSAL_KMALLOC_ATOMIC);
	if (phy_dma_data == NULL) {
		ion_iommu_unmap_ion_phys(dev->device,
			dma_map.iova, dma_map.size);
		VPU_ERR_DEV(dev->device, "failed to allocate memory.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -ENOMEM;
	}

	phy_dma_data->phys_addr = dma_map.phys_addr;
	phy_dma_data->size = dma_map.size;
	phy_dma_data->iova = dma_map.iova;
	phy_dma_data->filp = filp;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&dma_map,
			sizeof(dma_map));
	if (ret != 0) {
		ion_iommu_unmap_ion_phys(dev->device,
			phy_dma_data->iova, phy_dma_data->size);
		osal_kfree((void *)phy_dma_data);
		VPU_ERR_DEV(dev->device, "failed to copy to user\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	osal_spin_lock(&dev->vpu_spinlock);
	osal_list_add(&phy_dma_data->list, &dev->phy_data_list_head);
	osal_spin_unlock(&dev->vpu_spinlock);
	return ret;
}

static int32_t vpu_map_ion_phys(struct file *filp, hb_vpu_dev_t *dev, u_long arg)
{
	int32_t ret;

	ret = osal_sema_down_interruptible(&dev->vpu_sem);
	if (ret == 0) {
		ret = vpu_do_map_ion_phy(filp, dev, arg);
		osal_sema_up(&dev->vpu_sem);
	}

	return ret;
}

static int32_t vpu_unmap_ion_phys(struct file *filp, hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_ion_phys_map_t dma_map;
	hb_vpu_phy_dma_list_t *vil_tmp, *n;
	int32_t found = 0;
	int32_t ret;

	ret = osal_sema_down_interruptible(&dev->vpu_sem);
	if (ret == 0) {
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		ret = (int32_t)osal_copy_from_app((void *)&dma_map, (const void __user *) arg,
				sizeof(hb_vpu_ion_phys_map_t));
		if (ret != 0) {
			VPU_ERR_DEV(dev->device,
				"VDI_IOCTL_UNMAP_ION_PHYS copy from user fail.\n");
			osal_sema_up(&dev->vpu_sem);
			vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EFAULT;
		}

		osal_spin_lock(&dev->vpu_spinlock);
		// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 ++
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		osal_list_for_each_entry_safe(vil_tmp, n, &dev->phy_data_list_head, list) {
			if ((vil_tmp->iova == dma_map.iova) &&
				(vil_tmp->size == dma_map.size) &&
				(vil_tmp->filp == filp)) {
				osal_list_del(&vil_tmp->list);
				found = 1;
				break;
			}
		}
		// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 --
		osal_spin_unlock(&dev->vpu_spinlock);
		if (0 == found) {
			ret = -EINVAL;
			VPU_ERR_DEV(dev->device, "failed to find ion phys, dma_map.iova=0x%llx, dma_map.size=%d.\n", dma_map.iova, dma_map.size);
			vpu_send_diag_error_event((u16)EventIdVPUDevDmaMapFindErr, (u8)ERR_SEQ_0, 0, __LINE__);
		} else {
			ion_iommu_unmap_ion_phys(dev->device, dma_map.iova, dma_map.size);
			osal_kfree((void *)vil_tmp);
		}
		osal_sema_up(&dev->vpu_sem);
	}

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_get_version_info(hb_vpu_dev_t *dev, u_long arg)
{
	hb_vpu_version_info_data_t version_data;
	int32_t ret;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "[+]VDI_IOCTL_GET_VERSION_INFO\n");

	(void)memset((void *)&version_data, 0x00, sizeof(version_data));
	version_data.major = VPU_DRIVER_API_VERSION_MAJOR;
	version_data.minor = VPU_DRIVER_API_VERSION_MINOR;
	//coverity[misra_c_2012_rule_15_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&version_data,
			sizeof(hb_vpu_version_info_data_t));
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
	}

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(dev->device, "[-]VDI_IOCTL_GET_VERSION_INFO "
		"version_data.major==%d, version_data.minor=%d,",
		version_data.major, version_data.minor);

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static long vpu_ioctl(struct file *filp, u_int cmd, u_long arg) /* PRQA S 5209 */
{
	int32_t ret;
	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_vpu_priv_t *)filp->private_data;
	dev = priv->vpu_dev;
	if (dev == NULL) {
		VPU_ERR("failed to get vpu dev data\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_alloc_physical_memory(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_FREE_PHYSICALMEMORY: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_free_physical_memory(dev, arg);
		break;
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY /* PRQA S 4513,4599,4542,4543,1841,0499 */
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO:
		ret = vpu_get_reserved_memory_info(dev, arg);
		break;
#endif
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_WAIT_INTERRUPT: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_wait_interrupt(dev, arg);
		break;
#ifdef USE_MUTEX_IN_KERNEL_SPACE
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_VDI_LOCK: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_vdi_lock(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_VDI_UNLOCK: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_vdi_unlock(filp, dev, arg);
		break;
#endif
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_SET_CLOCK_GATE: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_set_clock_gate(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_GET_INSTANCE_POOL: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_get_instance_pool(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_GET_COMMON_MEMORY: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_get_common_memory(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_OPEN_INSTANCE: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_open_instance(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_CLOSE_INSTANCE: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_close_instance(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_GET_INSTANCE_NUM: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_get_instance_num(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_RESET: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = hb_vpu_hw_reset();
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_GET_REGISTER_INFO: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_get_register_info(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_ALLOCATE_INSTANCE_ID: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_alloc_instance_id(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_FREE_INSTANCE_ID: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_free_instance_id(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_POLL_WAIT_INSTANCE: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_poll_wait_instance(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_SET_CTX_INFO: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_set_ctx_info(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_SET_STATUS_INFO: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_set_status_info(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_MAP_ION_FD: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_map_ion_fd(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_UNMAP_ION_FD: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_unmap_ion_fd(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_MAP_ION_PHYS: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_map_ion_phys(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_UNMAP_ION_PHYS: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = vpu_unmap_ion_phys(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_GET_VERSION_INFO:
		ret = vpu_get_version_info(dev, arg);
		break;
#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case VDI_IOCTL_SET_ENC_DEC_PIC:
		ret = vpu_prio_set_enc_dec_pic(filp, dev, arg);
		break;
#endif
	default:
		{
			VPU_ERR_DEV(dev->device, "No such IOCTL, cmd is %d\n", cmd);
			ret = -EINVAL;
			vpu_send_diag_error_event((u16)EventIdVPUDevIoctlCmdErr, (u8)ERR_SEQ_0, 0, __LINE__);
		}
		break;
	}

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static ssize_t vpu_read(struct file *filp, char __user * buf, size_t len, /* PRQA S 3206 */
			loff_t * ppos) /* PRQA S 3206 */
{
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return -EINVAL;
}

//coverity[HIS_metric_violation:SUPPRESS]
static ssize_t vpu_write(struct file *filp, const char __user * buf, size_t len, /* PRQA S 3673 */
			 loff_t * ppos) /* PRQA S 3206 */
{
	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_vpu_priv_t *)filp->private_data;
	dev = priv->vpu_dev;

	if ((dev == NULL) || (buf == NULL)) {
		VPU_ERR("failed to get vpu dev(%pK) data or buf(%pK) is NULL\n",
			dev, buf);
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	if (len == sizeof(hb_vpu_drv_firmware_t)) {
		hb_vpu_drv_firmware_t *bit_firmware_info;

		/* code review D6: Dynamic alloc memory to copy large info */
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		bit_firmware_info = (hb_vpu_drv_firmware_t *)
			osal_kzalloc(sizeof(hb_vpu_drv_firmware_t), OSAL_KMALLOC_ATOMIC);
		if (bit_firmware_info == NULL) {
			VPU_ERR_DEV(dev->device, "vpu_write bit_firmware_info allocation error \n");
			vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EFAULT;
		}

		if (osal_copy_from_app((void *)bit_firmware_info, (const void __user *)buf, len) != 0UL) {
			VPU_ERR_DEV(dev->device, "vpu_write copy_from_user error "
				"for bit_firmware_info\n");
			osal_kfree((void *)bit_firmware_info);
			vpu_send_diag_error_event((u16)EventIdVPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EFAULT;
		}

		if (bit_firmware_info->size == sizeof(hb_vpu_drv_firmware_t)) {
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_DBG_DEV(dev->device, "vpu_write set bit_firmware_info coreIdx=0x%x, "
				"reg_base_offset=0x%x size=0x%x, bit_code[0]=0x%x\n",
				bit_firmware_info->core_idx,
				(int32_t)bit_firmware_info->reg_base_offset,
				bit_firmware_info->size,
				bit_firmware_info->bit_code[0]);
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

			if (bit_firmware_info->core_idx >= MAX_NUM_VPU_CORE) {
				VPU_ERR_DEV(dev->device, "vpu_write coreIdx[%d] is exceeded than "
					"MAX_NUM_VPU_CORE[%d]\n",
					bit_firmware_info->core_idx,
					MAX_NUM_VPU_CORE);
				osal_kfree((void *)bit_firmware_info);
				vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
				return -ENODEV;
			}

			(void)memcpy((void *)
				&dev->bit_fm_info[bit_firmware_info->core_idx],
				(void *)bit_firmware_info,
				sizeof(hb_vpu_drv_firmware_t));
			osal_kfree((void *)bit_firmware_info);
			return (ssize_t)len;
		}

		osal_kfree((void *)bit_firmware_info);
	}

	VPU_ERR_DEV(dev->device, "vpu_write wrong length(%zu), should be %lu \n",
		len, sizeof(hb_vpu_drv_firmware_t));
	vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_1, 0, __LINE__);
	return -EINVAL;
}

/* code review E1: No need to return value */
//coverity[HIS_metric_violation:SUPPRESS]
static void vpu_release_internal_res(hb_vpu_dev_t *dev,
			struct file *filp)
{
	uint32_t i, j;
	uint32_t open_count;
	hb_vpu_instance_pool_t *vip;
	int32_t ret;
	uint64_t local_crash_inst_mask = 0;

	osal_spin_lock(&dev->vpu_spinlock);	//check this place
	/* found and free the not closed instance by user applications */
	(void)vpu_free_instances(filp, &local_crash_inst_mask);
	osal_spin_unlock(&dev->vpu_spinlock);


#ifdef USE_VPU_CLOSE_INSTANCE_ONCE_ABNORMAL_RELEASE
	uint32_t core = 0;
	int32_t down_vpu_sem_finish = 1;
	uint64_t timeout = jiffies + VPU_GET_SEM_TIMEOUT;
	uint64_t remain_inst_mask = local_crash_inst_mask;
	while (remain_inst_mask != 0U) {
		VPU_DBG_DEV(dev->device, "tgid(%d) has crash_inst(mask=0x%llx), remain=0x%llx.\n",
			current->tgid, local_crash_inst_mask, remain_inst_mask);
		if (down_vpu_sem_finish == 0) {
			ret = osal_sema_down_timeout(&dev->vpu_sem, (uint32_t)timeout);
			if (ret == 0) {
				down_vpu_sem_finish = 1;
			} else {
				VPU_ERR_DEV(dev->device, "Fail to wait semaphore(ret=%d).\n", ret);
				continue;
			}
		}

#ifdef USE_SHARE_SEM_BT_KERNEL_AND_USER
		ret = vdi_lock(dev, core, VPUDRV_MUTEX_VPU);
#else
		ret = vdi_lock_user(filp, dev, VPUDRV_MUTEX_VPU);
#endif
		if (ret != 0) {
			VPU_DBG_DEV(dev->device, "tgid(%d) can't fetch lock,"
				"crash_inst(mask=0x%llx) close will delay.\n", current->tgid, remain_inst_mask);
			down_vpu_sem_finish = 0;
			osal_sema_up(&dev->vpu_sem);
			msleep(10);
			continue;
		}

		for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
			if (osal_test_bit(i, &remain_inst_mask) == 0)
				continue;

			ret = _vpu_close_instance((struct file *)filp, dev, core, i);
			if (ret != VPUAPI_RET_SUCCESS) {
				VPU_INFO_DEV(dev->device, "Warning: inst[%02d],"
					"_vpu_close_instance ret = %d\n", i, ret);
				osal_clear_bit(i, &remain_inst_mask);
			} else {
				osal_spin_lock(&dev->vpu_spinlock);
				(void)osal_test_and_clear_bit(i, (volatile uint64_t *)dev->vpu_crash_inst_bitmap);
				(void)osal_test_and_clear_bit(i, (volatile uint64_t *)dev->vpu_inst_bitmap); /* PRQA S 4446 */
				osal_sema_up(&dev->vpu_free_inst_sem);
				osal_spin_unlock(&dev->vpu_spinlock);
				osal_clear_bit(i, &remain_inst_mask);
				VPU_INFO_DEV(dev->device, "inst[%02d] close success.\n", i);
				vpu_instance_status_display(dev);
				break;
			}
		}
#ifdef USE_SHARE_SEM_BT_KERNEL_AND_USER
		vdi_unlock(dev, core, VPUDRV_MUTEX_VPU);
#else
		vdi_unlock(dev, VPUDRV_MUTEX_VPU);
#endif
	}
#endif

	/* found and free the not handled buffer by user applications */
	(void)vpu_free_buffers(filp);

	osal_spin_lock(&dev->vpu_spinlock);
	dev->open_count--;
	open_count = dev->open_count;
	osal_spin_unlock(&dev->vpu_spinlock);
	if (open_count == 0U) {
		if (dev->instance_pool.base != 0UL) {
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_DBG_DEV(dev->device, "free instance pool(address=0x%llx)\n",
				dev->instance_pool.base);
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
			// In case of abnormal quitting during initializing
			//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			vip = (hb_vpu_instance_pool_t *)dev->instance_pool.base;

#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			osal_free((void *)dev->instance_pool.base);
#else
			(void)vpu_free_dma_buffer(dev, &dev->instance_pool);
#endif
			dev->instance_pool.base = 0;
		}
		(void)vpu_force_free_lock(dev, 0);

		for (j = 0; j < MAX_NUM_VPU_INSTANCE; j++) {
			// TODO(lei.zhu) should clear bit during every close
			osal_spin_lock(&dev->vpu_spinlock); //check this place
			osal_spin_lock(&dev->irq_spinlock); //check this place
			// (void)osal_test_and_clear_bit(j, (volatile uint64_t *)dev->vpu_inst_bitmap); /* PRQA S 4446 */
			osal_fifo_reset(&dev->interrupt_pending_q[j]);
			dev->inst_open_flag[j] = 0;
			dev->total_poll[j] = 0;
			dev->total_release[j] = 0;
			dev->poll_int_event[j] = 0;
			osal_spin_unlock(&dev->irq_spinlock);
			osal_spin_unlock(&dev->vpu_spinlock);
		}
		VPU_DBG_DEV(dev->device, "irqenable = %llu\n", dev->vpu_irqenable);
	}

}

static int32_t vpu_release(struct inode *inode, struct file *filp) /* PRQA S 3673 */
{
	int32_t ret;
	uint64_t timeout;
	uint32_t open_count;

	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	dev = osal_list_entry(inode->i_cdev, hb_vpu_dev_t, cdev); /* PRQA S 3432,2810,0306,0662,1021,0497 */
	if (dev == NULL) {
		VPU_ERR("failed to get vpu dev data.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	priv = (hb_vpu_priv_t *)filp->private_data;

	timeout = jiffies + VPU_GET_SEM_TIMEOUT;
	ret = osal_sema_down_timeout(&dev->vpu_sem, (uint32_t)timeout);
	if (ret == 0) {
		vpu_release_internal_res(dev, filp);
		osal_kfree((void *)priv);
		filp->private_data = NULL;
		osal_sema_up(&dev->vpu_sem);
	} else {
		VPU_ERR_DEV(dev->device, "Fail to wait semaphore(ret=%d).\n", ret);
		vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}

#ifdef VPU_ENABLE_CLOCK
	if (hb_vpu_clk_disable(dev) != 0) {
		VPU_ERR_DEV(dev->device, "failed to disable clock.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevClkDisableErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}
#endif

	open_count = dev->open_count;
	if (open_count == 0U)
		hb_vpu_pm_ctrl(dev, 0, 0);

	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return ret;
}

static int32_t vpu_fasync(int32_t fd, struct file *filp, int32_t mode)
{
	int32_t ret;
	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	priv = (hb_vpu_priv_t *)filp->private_data;
	dev = priv->vpu_dev;
	if (dev == NULL) {
		VPU_ERR("failed to get vpu dev data\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	ret = fasync_helper(fd, filp, mode, &dev->async_queue);
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_map_to_register(struct file *filp, struct vm_area_struct *vm)
{
	uint64_t pfn;
	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;
	int32_t ret;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	if ((filp == NULL) || (vm == NULL)) {
		VPU_ERR("failed to map register, filp or vm is null.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_vpu_priv_t *)filp->private_data;
	dev = priv->vpu_dev;

	if (dev == NULL) {
		VPU_ERR("failed to map register, dev is null.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vm->vm_flags |= (unsigned long)VM_IO | (unsigned long)VM_RESERVED; /* PRQA S 4542,1841 */
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot); /* PRQA S 3469 */
	pfn = dev->vpu_mem->start >> PAGE_SHIFT;
	ret = (remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end - vm->vm_start,
			      vm->vm_page_prot) != 0) ? -EAGAIN : 0;
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_map_to_physical_memory(struct file *filp,
				struct vm_area_struct *vm)
{
	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;
	int32_t ret;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	if ((filp == NULL) || (vm == NULL)) {
		VPU_ERR("failed to map register, filp or vm is null.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_vpu_priv_t *)filp->private_data;
	dev = priv->vpu_dev;

	if (dev == NULL) {
		VPU_ERR("failed to map register, dev is null.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vm->vm_flags |= (unsigned long)VM_IO | (unsigned long)VM_RESERVED; /* PRQA S 4542,1841 */
	vm->vm_page_prot = pgprot_writecombine(vm->vm_page_prot); /* PRQA S 3469 */
	ret = (remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff,
			vm->vm_end - vm->vm_start,
			vm->vm_page_prot) != 0) ? -EAGAIN : 0;
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to map physical memory(ret=%d)\n", ret);
		vpu_send_diag_error_event((u16)EventIdVPUDevMmapErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_map_to_instance_pool_memory(struct file *filp,
					   struct vm_area_struct *vm)
{
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
	int32_t ret;
	int64_t length;
	uint64_t start;
	char *vmalloc_area_ptr;
	uint64_t pfn;
	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;
	uint64_t _page_size_u = PAGE_SIZE;
	int64_t _page_size = (int64_t)_page_size_u;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	if ((filp == NULL) || (vm == NULL)) {
		VPU_ERR("failed to map instances, filp or vm is null.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_vpu_priv_t *)filp->private_data;
	dev = priv->vpu_dev;

	if (dev == NULL) {
		VPU_ERR("failed to map  instances, dev is null.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}

	length = (int64_t)vm->vm_end - (int64_t)vm->vm_start;
	start = vm->vm_start;
	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vmalloc_area_ptr = (char *)dev->instance_pool.base; /* PRQA S 0306 */

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vm->vm_flags |= (unsigned long)VM_RESERVED; /* PRQA S 4542,1841 */

	/* loop over all pages, map it page individually */
	while (length > 0) {
		pfn = vmalloc_to_pfn((void *)vmalloc_area_ptr);
		ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE,
			PAGE_SHARED);
		if (ret < 0) {
			VPU_ERR_DEV(dev->device, "failed to map instance pool(ret=%d)\n", ret);
			vpu_send_diag_error_event((u16)EventIdVPUDevMmapErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return ret;
		}
		start += PAGE_SIZE;
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		vmalloc_area_ptr += PAGE_SIZE; /* PRQA S 0488 */
		length -= _page_size;
	}
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	return 0;
#else
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	vm->vm_flags |= VM_RESERVED;
	ret = remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff,
			vm->vm_end - vm->vm_start,
			vm->vm_page_prot) ? -EAGAIN : 0;
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to map physical memory(ret=%d)\n", ret);
		vpu_send_diag_error_event((u16)EventIdVPUDevMmapErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return ret;
#endif
}

/*!
 * @brief memory map interface for vpu file operation
 * @return  0 on success or negative error code on error
 */
static int32_t vpu_mmap(struct file *filp, struct vm_area_struct *vm) /* PRQA S 3673 */
{
	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_vpu_priv_t *)filp->private_data;
	dev = priv->vpu_dev;

#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
	if (vm->vm_pgoff == 0U)
		return vpu_map_to_instance_pool_memory(filp, vm);

	if (vm->vm_pgoff == (dev->vpu_mem->start >> PAGE_SHIFT))
		return vpu_map_to_register(filp, vm);

	return vpu_map_to_physical_memory(filp, vm);
#else
	if (vm->vm_pgoff) {
		if (vm->vm_pgoff == (dev->instance_pool.phys_addr >> PAGE_SHIFT))
			return vpu_map_to_instance_pool_memory(filp, vm);

		return vpu_map_to_physical_memory(filp, vm);
	}

	return vpu_map_to_register(filp, vm);
#endif
}

//coverity[HIS_metric_violation:SUPPRESS]
static uint32_t vpu_poll(struct file *filp, struct poll_table_struct *wait) /* PRQA S 5209 */
{
	hb_vpu_dev_t *dev;
	hb_vpu_priv_t *priv;
	uint32_t mask = 0;
	int64_t count;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_vpu_priv_t *)filp->private_data;
	dev = priv->vpu_dev;
	if (priv->inst_index >= MAX_NUM_VPU_INSTANCE) {
		VPU_ERR_DEV(dev->device, "Invalid instance index %d\n", priv->inst_index);
		vpu_send_diag_error_event((u16)EventIdVPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return EPOLLERR;
	}
	if (priv->is_irq_poll == 0U) {
		poll_wait(filp, &dev->poll_wait_q[priv->inst_index], wait);
		osal_spin_lock(&dev->poll_wait_q[priv->inst_index].lock);
		if (dev->poll_event[priv->inst_index] > 0) {
			//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			mask = (uint32_t)EPOLLIN | (uint32_t)EPOLLET; /* PRQA S 4399,0499 */
			dev->poll_event[priv->inst_index]--;
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		} else if (dev->poll_event[priv->inst_index] == LLONG_MIN) {
			mask = (uint32_t)EPOLLHUP;
			dev->poll_event[priv->inst_index] = 0;
		} else {
			mask = 0U;
		}
		osal_spin_unlock(&dev->poll_wait_q[priv->inst_index].lock);
	} else {
		poll_wait(filp, &dev->poll_int_wait_q[priv->inst_index], wait);
		count = dev->poll_int_event[priv->inst_index];
		if (count > 0) {
			//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			mask = (uint32_t)EPOLLIN | (uint32_t)EPOLLET; /* PRQA S 4399,0499 */
		}
	}
	return mask;
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
static struct file_operations vpu_fops = {
	.owner = THIS_MODULE,
	.open = vpu_open,
	.read = vpu_read,
	.write = vpu_write,
	.unlocked_ioctl = vpu_ioctl,
	.release = vpu_release,
	.fasync = vpu_fasync,
	.mmap = vpu_mmap,
	.poll = vpu_poll,
};

// PRQA S 0662 ++
//////////////// venc
static const char *get_profile(const hb_vpu_ctx_info_t *vpu_ctx)
{
	const char * prof_str;
	if (vpu_ctx->context.codec_id == MEDIA_CODEC_ID_H264) {
		switch (vpu_ctx->context.video_enc_params.h264_enc_config.h264_profile)
		 {
			case MC_H264_PROFILE_BP:
				prof_str = "bp";
				break;
			case MC_H264_PROFILE_MP:
				prof_str = "mp";
				break;
			case MC_H264_PROFILE_EXTENDED:
				prof_str = "extended";
				break;
			case MC_H264_PROFILE_HP:
				prof_str = "hp";
				break;
			case MC_H264_PROFILE_HIGH10:
				prof_str = "high10";
				break;
			case MC_H264_PROFILE_HIGH422:
				prof_str = "high422";
				break;
			case MC_H264_PROFILE_HIGH444:
				prof_str = "high444";
				break;
			default:
				prof_str = "unspecified";
				break;
		}
	} else if (vpu_ctx->context.codec_id == MEDIA_CODEC_ID_H265) {
		switch (vpu_ctx->context.video_enc_params.h265_enc_config.main_still_picture_profile_enable)
		 {
			case (hb_bool)0:
				prof_str = "Main";
				break;
			case (hb_bool)1:
				prof_str = "Main Still Picture";
				break;
			default:
				prof_str = "unspecified";
				break;
		}
	} else {
		prof_str = "unspecified";
	}

	return prof_str;
}

//coverity[HIS_metric_violation:SUPPRESS]
static const char *get_level(const hb_vpu_ctx_info_t *vpu_ctx)
{
	const char * level_str;
	if (vpu_ctx->context.codec_id == MEDIA_CODEC_ID_H264) {
		switch (vpu_ctx->context.video_enc_params.h264_enc_config.h264_level) {
			case MC_H264_LEVEL1:
				level_str = "level1";
				break;
			case MC_H264_LEVEL1b:
				level_str = "level1b";
				break;
			case MC_H264_LEVEL1_1:
				level_str = "level1_1";
				break;
			case MC_H264_LEVEL1_2:
				level_str = "level1_2";
				break;
			case MC_H264_LEVEL1_3:
				level_str = "level1_3";
				break;
			case MC_H264_LEVEL2:
				level_str = "level2";
				break;
			case MC_H264_LEVEL2_1:
				level_str = "level2_1";
				break;
			case MC_H264_LEVEL2_2:
				level_str = "level2_2";
				break;
			case MC_H264_LEVEL3:
				level_str = "level3";
				break;
			case MC_H264_LEVEL3_1:
				level_str = "level3_1";
				break;
			case MC_H264_LEVEL3_2:
				level_str = "level3_2";
				break;
			case MC_H264_LEVEL4:
				level_str = "level4";
				break;
			case MC_H264_LEVEL4_1:
				level_str = "level4_1";
				break;
			case MC_H264_LEVEL4_2:
				level_str = "level4_2";
				break;
			case MC_H264_LEVEL5:
				level_str = "level5";
				break;
			case MC_H264_LEVEL5_1:
				level_str = "level5_1";
				break;
			case MC_H264_LEVEL5_2:
				level_str = "level5_2";
				break;
			default:
				level_str = "unspecified";
				break;
		}
	} else if (vpu_ctx->context.codec_id == MEDIA_CODEC_ID_H265) {
		switch (vpu_ctx->context.video_enc_params.h265_enc_config.h265_level) {
			case MC_H265_LEVEL1:
				level_str = "level1";
				break;
			case MC_H265_LEVEL2:
				level_str = "level2";
				break;
			case MC_H265_LEVEL2_1:
				level_str = "level2_1";
				break;
			case MC_H265_LEVEL3:
				level_str = "level3";
				break;
			case MC_H265_LEVEL3_1:
				level_str = "level3_1";
				break;
			case MC_H265_LEVEL4:
				level_str = "level4";
				break;
			case MC_H265_LEVEL4_1:
				level_str = "level4_1";
				break;
			case MC_H265_LEVEL5:
				level_str = "level5";
				break;
			case MC_H265_LEVEL5_1:
				level_str = "level5_1";
				break;
			default:
				level_str = "unspecified";
				break;
		}
	} else {
		level_str = "---";
	}

	return level_str;
}

static const char *get_codec(const hb_vpu_ctx_info_t *vpu_ctx)
{
	const char * codec_str;
	switch (vpu_ctx->context.codec_id) {
		case MEDIA_CODEC_ID_H264:
			codec_str = "h264";
			break;
		case MEDIA_CODEC_ID_H265:
			codec_str = "h265";
			break;
		case MEDIA_CODEC_ID_MJPEG:
			codec_str = "mjpg";
			break;
		case MEDIA_CODEC_ID_JPEG:
			codec_str = "jpeg";
			break;
		default:
			codec_str = "unspecified";
			break;
	}
	return codec_str;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t rcparam_show(struct seq_file *s, const hb_vpu_ctx_info_t *vpu_ctx) {
	int32_t ret = 0;
	const mc_rate_control_params_t *rc =
		&(vpu_ctx->context.video_enc_params.rc_params);
	if (rc->mode == MC_AV_RC_MODE_H264CBR) {
		seq_printf(s, "%7d %7s %12d %8d %8d %10d %13d %15d %18d %8d %8d "
			"%8d %8d %8d %8d %13d %12d %13d %12d\n",
			vpu_ctx->context.instance_index,
			"h264cbr",
			rc->h264_cbr_params.intra_period,
			rc->h264_cbr_params.intra_qp,
			rc->h264_cbr_params.bit_rate,
			rc->h264_cbr_params.frame_rate,
			rc->h264_cbr_params.initial_rc_qp,
			rc->h264_cbr_params.vbv_buffer_size,
			rc->h264_cbr_params.mb_level_rc_enalbe,
			rc->h264_cbr_params.min_qp_I,
			rc->h264_cbr_params.max_qp_I,
			rc->h264_cbr_params.min_qp_P,
			rc->h264_cbr_params.max_qp_P,
			rc->h264_cbr_params.min_qp_B,
			rc->h264_cbr_params.max_qp_B,
			rc->h264_cbr_params.hvs_qp_enable,
			rc->h264_cbr_params.hvs_qp_scale,
			rc->h264_cbr_params.qp_map_enable,
			rc->h264_cbr_params.max_delta_qp);
	} else if (rc->mode == MC_AV_RC_MODE_H264VBR) {
		seq_printf(s, "%7d %7s %12d %8d %10d %13d\n",
			vpu_ctx->context.instance_index,
			"h264vbr",
			rc->h264_vbr_params.intra_period,
			rc->h264_vbr_params.intra_qp,
			rc->h264_vbr_params.frame_rate,
			rc->h264_vbr_params.qp_map_enable);
	} else if (rc->mode == MC_AV_RC_MODE_H264AVBR) {
		seq_printf(s, "%7d %8s %12d %8d %8d %10d %13d %15d %18d %8d %8d "
			"%8d %8d %8d %8d %13d %12d %13d %12d\n",
			vpu_ctx->context.instance_index,
			"h264avbr",
			rc->h264_avbr_params.intra_period,
			rc->h264_avbr_params.intra_qp,
			rc->h264_avbr_params.bit_rate,
			rc->h264_avbr_params.frame_rate,
			rc->h264_avbr_params.initial_rc_qp,
			rc->h264_avbr_params.vbv_buffer_size,
			rc->h264_avbr_params.mb_level_rc_enalbe,
			rc->h264_avbr_params.min_qp_I,
			rc->h264_avbr_params.max_qp_I,
			rc->h264_avbr_params.min_qp_P,
			rc->h264_avbr_params.max_qp_P,
			rc->h264_avbr_params.min_qp_B,
			rc->h264_avbr_params.max_qp_B,
			rc->h264_avbr_params.hvs_qp_enable,
			rc->h264_avbr_params.hvs_qp_scale,
			rc->h264_avbr_params.qp_map_enable,
			rc->h264_avbr_params.max_delta_qp);
	} else if (rc->mode == MC_AV_RC_MODE_H264FIXQP) {
		seq_printf(s, "%7d %9s %12d %10d %10d %10d %10d\n",
			vpu_ctx->context.instance_index,
			"h264fixqp",
			rc->h264_fixqp_params.intra_period,
			rc->h264_fixqp_params.frame_rate,
			rc->h264_fixqp_params.force_qp_I,
			rc->h264_fixqp_params.force_qp_P,
			rc->h264_fixqp_params.force_qp_B);
	} else if (rc->mode == MC_AV_RC_MODE_H264QPMAP) {
		seq_printf(s, "%7d %9s %12d %10d %18d\n",
			vpu_ctx->context.instance_index,
			"h264qpmap",
			rc->h264_qpmap_params.intra_period,
			rc->h264_qpmap_params.frame_rate,
			rc->h264_qpmap_params.qp_map_array_count);
	} else if (rc->mode == MC_AV_RC_MODE_H265CBR) {
		seq_printf(s, "%7d %7s %12d %8d %8d %10d %13d %15d %19d %8d %8d "
			"%8d %8d %8d %8d %13d %12d %13d %12d\n",
			vpu_ctx->context.instance_index,
			"h265cbr",
			rc->h265_cbr_params.intra_period,
			rc->h265_cbr_params.intra_qp,
			rc->h265_cbr_params.bit_rate,
			rc->h265_cbr_params.frame_rate,
			rc->h265_cbr_params.initial_rc_qp,
			rc->h265_cbr_params.vbv_buffer_size,
			rc->h265_cbr_params.ctu_level_rc_enalbe,
			rc->h265_cbr_params.min_qp_I,
			rc->h265_cbr_params.max_qp_I,
			rc->h265_cbr_params.min_qp_P,
			rc->h265_cbr_params.max_qp_P,
			rc->h265_cbr_params.min_qp_B,
			rc->h265_cbr_params.max_qp_B,
			rc->h265_cbr_params.hvs_qp_enable,
			rc->h265_cbr_params.hvs_qp_scale,
			rc->h265_cbr_params.qp_map_enable,
			rc->h265_cbr_params.max_delta_qp);
	} else if (rc->mode == MC_AV_RC_MODE_H265VBR) {
		seq_printf(s, "%7d %7s %12d %8d %10d %13d\n",
			vpu_ctx->context.instance_index,
			"h265vbr",
			rc->h265_vbr_params.intra_period,
			rc->h265_vbr_params.intra_qp,
			rc->h265_vbr_params.frame_rate,
			rc->h265_vbr_params.qp_map_enable);
	} else if (rc->mode == MC_AV_RC_MODE_H265AVBR) {
		seq_printf(s, "%7d %7s %12d %8d %8d %10d %13d %15d %19d %8d %8d "
			"%8d %8d %8d %8d %13d %12d %13d %12d\n",
			vpu_ctx->context.instance_index,
			"h265avbr",
			rc->h265_avbr_params.intra_period,
			rc->h265_avbr_params.intra_qp,
			rc->h265_avbr_params.bit_rate,
			rc->h265_avbr_params.frame_rate,
			rc->h265_avbr_params.initial_rc_qp,
			rc->h265_avbr_params.vbv_buffer_size,
			rc->h265_avbr_params.ctu_level_rc_enalbe,
			rc->h265_avbr_params.min_qp_I,
			rc->h265_avbr_params.max_qp_I,
			rc->h265_avbr_params.min_qp_P,
			rc->h265_avbr_params.max_qp_P,
			rc->h265_avbr_params.min_qp_B,
			rc->h265_avbr_params.max_qp_B,
			rc->h265_avbr_params.hvs_qp_enable,
			rc->h265_avbr_params.hvs_qp_scale,
			rc->h265_avbr_params.qp_map_enable,
			rc->h265_avbr_params.max_delta_qp);
	} else if (rc->mode == MC_AV_RC_MODE_H265FIXQP) {
		seq_printf(s, "%7d %9s %12d %10d %10d %10d %10d\n",
			vpu_ctx->context.instance_index,
			"h265fixqp",
			rc->h265_fixqp_params.intra_period,
			rc->h265_fixqp_params.frame_rate,
			rc->h265_fixqp_params.force_qp_I,
			rc->h265_fixqp_params.force_qp_P,
			rc->h265_fixqp_params.force_qp_B);
	} else if (rc->mode == MC_AV_RC_MODE_H265QPMAP) {
		seq_printf(s, "%7d %9s %12d %10d %18d\n",
			vpu_ctx->context.instance_index,
			"h265qpmap",
			rc->h265_qpmap_params.intra_period,
			rc->h265_qpmap_params.frame_rate,
			rc->h265_qpmap_params.qp_map_array_count);
	} else {
		VPU_ERR("Invalid rate control mode %d\n", rc->mode);
		vpu_send_diag_error_event((u16)EventIdVPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
		ret = -1;
	}

	seq_printf(s, "\n");

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_venc_show_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "----encode enc param----\n");
				seq_printf(s, "%7s %7s %11s %11s %5s %6s %7s %10s %15s %11s "
				"%10s %6s %6s\n", "enc_idx", "enc_id", "profile", "level",
				"width", "height", "pix_fmt", "fbuf_count", "extern_buf_flag",
				"bsbuf_count", "bsbuf_size", "mirror", "rotate");
			}
			seq_printf(s, "%7d %7s %11s %11s %5d %6d %7d "\
				"%10d %15d %11d %10d %6d %6d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				get_profile(&dev->vpu_ctx[i]),
				get_level(&dev->vpu_ctx[i]),
				dev->vpu_ctx[i].context.video_enc_params.width,
				dev->vpu_ctx[i].context.video_enc_params.height,
				dev->vpu_ctx[i].context.video_enc_params.pix_fmt,
				dev->vpu_ctx[i].context.video_enc_params.frame_buf_count,
				dev->vpu_ctx[i].context.video_enc_params.external_frame_buf,
				dev->vpu_ctx[i].context.video_enc_params.bitstream_buf_count,
				dev->vpu_ctx[i].context.video_enc_params.bitstream_buf_size,
				dev->vpu_ctx[i].context.video_enc_params.mir_direction,
				dev->vpu_ctx[i].context.video_enc_params.rot_degree);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264cbr_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	output = 0;
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.video_enc_params.rc_params.mode
				== MC_AV_RC_MODE_H264CBR)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264cbr param----\n");
				seq_printf(s, "%7s %7s %12s %8s %8s %10s %13s %15s %18s %8s "
					"%8s %8s %8s %8s %8s %13s %12s %13s %12s\n",
					"enc_idx", "rc_mode", "intra_period", "intra_qp",
					"bit_rate",	"frame_rate", "initial_rc_qp",
					"vbv_buffer_size", "mb_level_rc_enalbe", "min_qp_I",
					"max_qp_I", "min_qp_P", "max_qp_P", "min_qp_B", "max_qp_B",
					"hvs_qp_enable", "hvs_qp_scale",
					"qp_map_enable", "max_delta_qp");
			}
			(void)rcparam_show(s, &(dev->vpu_ctx[i]));
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264vbr_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.video_enc_params.rc_params.mode
				== MC_AV_RC_MODE_H264VBR)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264vbr param----\n");
				seq_printf(s, "%7s %7s %12s %8s %10s %13s\n",
					"enc_idx", "rc_mode", "intra_period", "intra_qp",
					"frame_rate", "qp_map_enable\n");
			}
			(void)rcparam_show(s, &(dev->vpu_ctx[i]));
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264avbr_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.video_enc_params.rc_params.mode
				== MC_AV_RC_MODE_H264AVBR)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264avbr param----\n");
				seq_printf(s, "%7s %8s %12s %8s %8s %10s %13s %15s %18s %8s "
					"8s %8s %8s %8s %8s %13s %12s %13s %12s\n",
					"enc_idx", "rc_mode", "intra_period", "intra_qp", "bit_rate"
					"frame_rate", "initial_rc_qp", "vbv_buffer_size",
					"mb_level_rc_enalbe", "min_qp_I", "max_qp_I", "min_qp_P",
					"max_qp_P", "min_qp_B", "max_qp_B",	"hvs_qp_enable",
					"hvs_qp_scale", "qp_map_enable", "max_delta_qp");
			}
			(void)rcparam_show(s, &(dev->vpu_ctx[i]));
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264fixqp_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.video_enc_params.rc_params.mode
				== MC_AV_RC_MODE_H264FIXQP)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264fixqp param----\n");
				seq_printf(s, "%7s %9s %12s %10s %10s %10s %10s\n",
					"enc_idx", "rc_mode", "intra_period", "frame_rate",
					"force_qp_I", "force_qp_P", "force_qp_B");
			}
			(void)rcparam_show(s, &(dev->vpu_ctx[i]));
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264qpmap_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.video_enc_params.rc_params.mode
				== MC_AV_RC_MODE_H264QPMAP)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264qpmap param----\n");
				seq_printf(s, "%7s %9s %12s %10s %18s\n", "enc_idx", "rc_mode",
					"intra_period", "frame_rate", "qp_map_array_count");
			}
			(void)rcparam_show(s, &(dev->vpu_ctx[i]));
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265cbr_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.video_enc_params.rc_params.mode
				== MC_AV_RC_MODE_H265CBR)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265cbr param----\n");
				seq_printf(s, "%7s %7s %12s %8s %8s %10s %13s %15s %19s %8s "
					"%8s %8s %8s %8s %8s %13s %12s %13s %12s\n",
					"enc_idx", "rc_mode", "intra_period", "intra_qp",
					"bit_rate",	"frame_rate", "initial_rc_qp",
					"vbv_buffer_size", "ctu_level_rc_enalbe", "min_qp_I",
					"max_qp_I", "min_qp_P", "max_qp_P", "min_qp_B", "max_qp_B",
					"hvs_qp_enable", "hvs_qp_scale",
					"qp_map_enable", "max_delta_qp");
			}
			(void)rcparam_show(s, &(dev->vpu_ctx[i]));
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265vbr_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.video_enc_params.rc_params.mode
				== MC_AV_RC_MODE_H265VBR)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265vbr param----\n");
				seq_printf(s, "%7s %7s %12s %8s %10s %13s\n",
					"enc_idx", "rc_mode", "intra_period", "intra_qp",
					"frame_rate", "qp_map_enable\n");
			}
			(void)rcparam_show(s, &(dev->vpu_ctx[i]));
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265avbr_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.video_enc_params.rc_params.mode
				== MC_AV_RC_MODE_H265AVBR)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265avbr param----\n");
				seq_printf(s, "%7s %7s %12s %8s %8s %10s %13s %15s %19s %8s "
					"%8s %8s %8s %8s %8s %13s %12s %13s %12s\n",
					"enc_idx", "rc_mode", "intra_period", "intra_qp",
					"bit_rate",	"frame_rate", "initial_rc_qp",
					"vbv_buffer_size", "ctu_level_rc_enalbe", "min_qp_I",
					"max_qp_I", "min_qp_P", "max_qp_P", "min_qp_B", "max_qp_B",
					"hvs_qp_enable", "hvs_qp_scale",
					"qp_map_enable", "max_delta_qp");
			}
			(void)rcparam_show(s, &(dev->vpu_ctx[i]));
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265fixqp_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.video_enc_params.rc_params.mode
				== MC_AV_RC_MODE_H265FIXQP)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265fixqp param----\n");
				seq_printf(s, "%7s %9s %12s %10s %10s %10s %10s\n",
					"enc_idx", "rc_mode", "intra_period", "frame_rate",
					"force_qp_I", "force_qp_P", "force_qp_B");
			}
			(void)rcparam_show(s, &(dev->vpu_ctx[i]));
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265qpmap_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.video_enc_params.rc_params.mode
				== MC_AV_RC_MODE_H265QPMAP)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265qpmap param----\n");
				seq_printf(s, "%7s %9s %12s %10s %18s\n", "enc_idx", "rc_mode",
					"intra_period", "frame_rate", "qp_map_array_count");
			}
			(void)rcparam_show(s, &(dev->vpu_ctx[i]));
		}
	}

	return 0;
}

static int32_t vpu_venc_show_gop_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	mc_video_gop_params_t *gop;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode gop param----\n");
				seq_printf(s, "%7s %7s %14s %15s %21s\n", "enc_idx", "enc_id",
				"gop_preset_idx", "custom_gop_size", "decoding_refresh_type");
			}
			gop =
				&(dev->vpu_ctx[i].context.video_enc_params.gop_params);
			seq_printf(s, "%7d %7s %14d %15d %21d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				gop->gop_preset_idx,
				gop->custom_gop_size,
				gop->decoding_refresh_type);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_intrarefresh_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode intra refresh----\n");
				seq_printf(s, "%7s %7s %18s %17s\n", "enc_idx", "enc_id",
					"intra_refresh_mode", "intra_refresh_arg");
			}
			seq_printf(s, "%7d %7s %18d %17d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				dev->vpu_ctx[i].intra_refr.intra_refresh_mode,
				dev->vpu_ctx[i].intra_refr.intra_refresh_arg);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_longtermref_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode longterm ref----\n");
				seq_printf(s, "%7s %7s %12s %19s %25s\n", "enc_idx", "enc_id",
					"use_longterm", "longterm_pic_period",
					"longterm_pic_using_period");
			}
			seq_printf(s, "%7d %7s %12d %19d %25d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				dev->vpu_ctx[i].ref_mode.use_longterm,
				dev->vpu_ctx[i].ref_mode.longterm_pic_period,
				dev->vpu_ctx[i].ref_mode.longterm_pic_using_period);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_roi_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder)) {
			mc_video_roi_params_t *roi_params =
						&(dev->vpu_ctx[i].roi_params);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode roi_params----\n");
				seq_printf(s, "%7s %7s %10s %19s\n", "enc_idx",
					"enc_id", "roi_enable", "roi_map_array_count");
			}
			seq_printf(s, "%7d %7s %10d %19d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				roi_params->roi_enable,
				roi_params->roi_map_array_count);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_modedec_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H265)) {
			mc_video_mode_decision_params_t *mode_decision =
						&(dev->vpu_ctx[i].mode_decision);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode mode_decision 1----\n");
				seq_printf(s, "%7s %7s %20s %15s %15s %15s %15s %28s "
					"%24s %27s %28s %24s %27s %28s %24s %27s\n", "enc_idx",
					"enc_id", "mode_decision_enable", "pu04_delta_rate",
					"pu08_delta_rate", "pu16_delta_rate",
					"pu32_delta_rate", "pu04_intra_planar_delta_rate",
					"pu04_intra_dc_delta_rate",
					"pu04_intra_angle_delta_rate",
					"pu08_intra_planar_delta_rate", "pu08_intra_dc_delta_rate",
					"pu08_intra_angle_delta_rate",
					"pu16_intra_planar_delta_rate", "pu16_intra_dc_delta_rate",
					"pu16_intra_angle_delta_rate");
			}
			seq_printf(s, "%7d %7s %20d %15d %15d %15d %15d %28d "
				"%24d %27d %28d %24d %27d %28d %24d %27d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				mode_decision->mode_decision_enable,
				mode_decision->pu04_delta_rate,
				mode_decision->pu08_delta_rate,
				mode_decision->pu16_delta_rate,
				mode_decision->pu32_delta_rate,
				mode_decision->pu04_intra_planar_delta_rate,
				mode_decision->pu04_intra_dc_delta_rate,
				mode_decision->pu04_intra_angle_delta_rate,
				mode_decision->pu08_intra_planar_delta_rate,
				mode_decision->pu08_intra_dc_delta_rate,
				mode_decision->pu08_intra_angle_delta_rate,
				mode_decision->pu16_intra_planar_delta_rate,
				mode_decision->pu16_intra_dc_delta_rate,
				mode_decision->pu16_intra_angle_delta_rate);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_modedec2_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H265)) {
			mc_video_mode_decision_params_t *mode_decision =
						&(dev->vpu_ctx[i].mode_decision);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode mode_decision 2----\n");
				seq_printf(s, "%7s %7s %28s %24s %27s %21s %21s %21s "
					"%21s %21s %21s %21s %21s %21s\n", "enc_idx",
					"enc_id", "pu32_intra_planar_delta_rate",
					"pu32_intra_dc_delta_rate",
					"pu32_intra_angle_delta_rate", "cu08_intra_delta_rate",
					"cu08_inter_delta_rate", "cu08_merge_delta_rate",
					"cu16_intra_delta_rate", "cu16_inter_delta_rate",
					"cu16_merge_delta_rate", "cu32_intra_delta_rate",
					"cu32_inter_delta_rate", "cu32_merge_delta_rate");
			}
			seq_printf(s, "%7d %7s %28d %24d %27d %21d %21d %21d "
				"%21d %21d %21d %21d %21d %21d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				mode_decision->pu32_intra_planar_delta_rate,
				mode_decision->pu32_intra_dc_delta_rate,
				mode_decision->pu32_intra_angle_delta_rate,
				mode_decision->cu08_intra_delta_rate,
				mode_decision->cu08_inter_delta_rate,
				mode_decision->cu08_merge_delta_rate,
				mode_decision->cu16_intra_delta_rate,
				mode_decision->cu16_inter_delta_rate,
				mode_decision->cu16_merge_delta_rate,
				mode_decision->cu32_intra_delta_rate,
				mode_decision->cu32_inter_delta_rate,
				mode_decision->cu32_merge_delta_rate);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264entropy_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	mc_h264_entropy_params_t *entropy_params;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H264)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264 entropy params----\n");
				seq_printf(s, "%7s %7s %19s\n", "enc_idx",
					"enc_id", "entropy_coding_mode");
			}
			entropy_params =
					&(dev->vpu_ctx[i].entropy_params);
			seq_printf(s, "%7d %7s %19s\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				((entropy_params->entropy_coding_mode == 0U) ? "CAVLC" : "CABAC"));
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264slice_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	mc_video_slice_params_t *slice_params;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H264)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264 slice params----\n");
				seq_printf(s, "%7s %7s %15s %14s\n", "enc_idx", "enc_id",
					"h264_slice_mode", "h264_slice_arg");
			}
			slice_params =
					&(dev->vpu_ctx[i].slice_params);
			seq_printf(s, "%7d %7s %15d %14d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				slice_params->h264_slice.h264_slice_mode,
				slice_params->h264_slice.h264_slice_arg);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264deblfilter_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H264)) {
			mc_video_deblk_filter_params_t *deblk_filter =
					&(dev->vpu_ctx[i].deblk_filter);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264 deblk filter----\n");
				seq_printf(s, "%7s %7s %29s %26s %22s\n", "enc_idx",
					"enc_id", "disable_deblocking_filter_idc",
					"slice_alpha_c0_offset_div2", "slice_beta_offset_div2");
			}
			seq_printf(s, "%7d %7s %29d %26d %22d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				deblk_filter->h264_deblk.disable_deblocking_filter_idc,
				deblk_filter->h264_deblk.slice_alpha_c0_offset_div2,
				deblk_filter->h264_deblk.slice_beta_offset_div2);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264vui_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}


	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H264)) {
			mc_h264_vui_params_t *vui =
				&(dev->vpu_ctx[i].vui_params.h264_vui);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264 vui----\n");
				seq_printf(s,
					"%7s %7s %30s %16s %9s %10s %26s %25s %30s %12s "
					"%21s %31s %16s %24s %19s %28s %21s %14s %25s %26s\n",
					"enc_idx", "enc_id", "aspect_ratio_info_present_flag",
					"aspect_ratio_idc", "sar_width", "sar_height",
					"overscan_info_present_flag", "overscan_appropriate_flag",
					"video_signal_type_present_flag", "video_format", "video_full_range_flag",
					"colour_description_present_flag", "colour_primaries",
					"transfer_characteristics", "matrix_coefficients",
					"vui_timing_info_present_flag", "vui_num_units_in_tick",
					"vui_time_scale", "vui_fixed_frame_rate_flag", "bitstream_restriction_flag");
			}
			seq_printf(s, "%7d %7s %30d %16d %9d %10d %26d %25d %30d %12d "
				"%21d %31d %16d %24d %19d %28d %21d %14d %25d %26d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				vui->aspect_ratio_info_present_flag,
				vui->aspect_ratio_idc,
				vui->sar_width,
				vui->sar_height,
				vui->overscan_info_present_flag,
				vui->overscan_appropriate_flag,
				vui->video_signal_type_present_flag,
				vui->video_format,
				vui->video_full_range_flag,
				vui->colour_description_present_flag,
				vui->colour_primaries,
				vui->transfer_characteristics,
				vui->matrix_coefficients,
				vui->vui_timing_info_present_flag,
				vui->vui_num_units_in_tick,
				vui->vui_time_scale,
				vui->vui_fixed_frame_rate_flag,
				vui->bitstream_restriction_flag);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265vui_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H265)) {
			mc_h265_vui_params_t *vui =
				&(dev->vpu_ctx[i].vui_params.h265_vui);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265 vui----\n");
				seq_printf(s,
					"%7s %7s %30s %16s %9s %10s %26s %25s %30s %12s %21s %31s "
					"%16s %24s %19s %28s %21s %14s %35s %33s %26s\n",
					"enc_idx",
					"enc_id",
					"aspect_ratio_info_present_flag",
					"aspect_ratio_idc",
					"sar_width",
					"sar_height",
					"overscan_info_present_flag",
					"overscan_appropriate_flag",
					"video_signal_type_present_flag",
					"video_format",
					"video_full_range_flag",
					"colour_description_present_flag",
					"colour_primaries",
					"transfer_characteristics",
					"matrix_coefficients",
					"vui_timing_info_present_flag",
					"vui_num_units_in_tick",
					"vui_time_scale",
					"vui_poc_proportional_to_timing_flag",
					"vui_num_ticks_poc_diff_one_minus1",
					"bitstream_restriction_flag");
			}
			seq_printf(s, "%7d %7s %30d %16d %9d %10d %26d %25d %30d %12d %21d %31d "
				"%16d %24d %19d %28d %21d %14d %35d %33d %26d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				vui->aspect_ratio_info_present_flag,
				vui->aspect_ratio_idc,
				vui->sar_width,
				vui->sar_height,
				vui->overscan_info_present_flag,
				vui->overscan_appropriate_flag,
				vui->video_signal_type_present_flag,
				vui->video_format,
				vui->video_full_range_flag,
				vui->colour_description_present_flag,
				vui->colour_primaries,
				vui->transfer_characteristics,
				vui->matrix_coefficients,
				vui->vui_timing_info_present_flag,
				vui->vui_num_units_in_tick,
				vui->vui_time_scale,
				vui->vui_poc_proportional_to_timing_flag,
				vui->vui_num_ticks_poc_diff_one_minus1,
				vui->bitstream_restriction_flag);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_3dnr_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder)) {
			mc_video_3dnr_enc_params_t *noise_rd = &(dev->vpu_ctx[i].noise_rd);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode 3dnr----\n");
				seq_printf(s,
					"%7s %7s %11s %12s %12s %13s %16s %17s %17s %16s %17s %17s "
					"%15s %16s %16s\n",
					"enc_idx",
					"enc_id",
					"nr_y_enable",
					"nr_cb_enable",
					"nr_cr_enable",
					"nr_est_enable",
					"nr_intra_weightY",
					"nr_intra_weightCb",
					"nr_intra_weightCr",
					"nr_inter_weightY",
					"nr_inter_weightCb",
					"nr_inter_weightCr",
					"nr_noise_sigmaY",
					"nr_noise_sigmaCb",
					"nr_noise_sigmaCr");
			}
			seq_printf(s,
				"%7d %7s %11d %12d %12d %13d %16d %17d %17d %16d %17d %17d "
				"%15d %16d %16d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				noise_rd->nr_y_enable,
				noise_rd->nr_cb_enable,
				noise_rd->nr_cr_enable,
				noise_rd->nr_est_enable,
				noise_rd->nr_intra_weightY,
				noise_rd->nr_intra_weightCb,
				noise_rd->nr_intra_weightCr,
				noise_rd->nr_inter_weightY,
				noise_rd->nr_inter_weightCb,
				noise_rd->nr_inter_weightCr,
				noise_rd->nr_noise_sigmaY,
				noise_rd->nr_noise_sigmaCb,
				noise_rd->nr_noise_sigmaCr);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_smart_bg_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder)) {
			mc_video_smart_bg_enc_params_t *smart_bg = &(dev->vpu_ctx[i].smart_bg);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode smart bg----\n");
				seq_printf(s,
					"%7s %7s %16s %17s %22s %12s %11s %13s\n",
					"enc_idx",
					"enc_id",
					"bg_detect_enable",
					"bg_threshold_diff",
					"bg_threshold_mean_diff",
					"bg_lambda_qp",
					"bg_delta_qp",
					"s2fme_disable");
			}
			seq_printf(s,"%7d %7s %16d %17d %22d %12d %11d %13d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				smart_bg->bg_detect_enable,
				smart_bg->bg_threshold_diff,
				smart_bg->bg_threshold_mean_diff,
				smart_bg->bg_lambda_qp,
				smart_bg->bg_delta_qp,
				smart_bg->s2fme_disable);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264vuitiming_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H264)) {
			mc_h264_timing_params_t *timing =
					&(dev->vpu_ctx[i].vui_timing.h264_timing);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264 timing----\n");
				seq_printf(s, "%7s %7s %21s %14s %21s\n", "enc_idx",
					"enc_id", "vui_num_units_in_tick",
					"vui_time_scale", "fixed_frame_rate_flag");
			}
			seq_printf(s, "%7d %7s %21d %14d %21d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				timing->vui_num_units_in_tick,
				timing->vui_time_scale,
				timing->fixed_frame_rate_flag);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264intrapred_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H264)) {
			mc_h264_intra_pred_params_t *intra_pred_params =
					&(dev->vpu_ctx[i].pred_unit.h264_intra_pred);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264_intra_pred----\n");
				seq_printf(s, "%7s %7s %27s\n", "enc_idx",
					"enc_id", "constrained_intra_pred_flag");
			}
			seq_printf(s, "%7d %7s %27d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				intra_pred_params->constrained_intra_pred_flag);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h264transform_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H264)) {
			mc_h264_transform_params_t *h264_transform =
					&(dev->vpu_ctx[i].transform_params.h264_transform);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h264_transform----\n");
				seq_printf(s, "%7s %7s %20s %19s %19s %24s\n", "enc_idx",
					"enc_id", "transform_8x8_enable", "chroma_cb_qp_offset",
					"chroma_cr_qp_offset", "user_scaling_list_enable");
			}
			seq_printf(s, "%7d %7s %20d %19d %19d %24d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				h264_transform->transform_8x8_enable,
				h264_transform->chroma_cb_qp_offset,
				h264_transform->chroma_cr_qp_offset,
				h264_transform->user_scaling_list_enable);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265transform_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H265)) {
			mc_h265_transform_params_t *h265_transform =
					&(dev->vpu_ctx[i].transform_params.h265_transform);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265_transform----\n");
				seq_printf(s, "%7s %7s %19s %19s %24s\n", "enc_idx",
					"enc_id", "chroma_cb_qp_offset",
					"chroma_cr_qp_offset", "user_scaling_list_enable");
			}
			seq_printf(s, "%7d %7s %19d %19d %24d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				h265_transform->chroma_cb_qp_offset,
				h265_transform->chroma_cr_qp_offset,
				h265_transform->user_scaling_list_enable);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265predunit_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H265)) {
			mc_h265_pred_unit_params_t *pred_unit_params =
					&(dev->vpu_ctx[i].pred_unit.h265_pred_unit);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265_pred_unit----\n");
				seq_printf(s, "%7s %7s %16s %27s %35s %13s\n", "enc_idx",
					"enc_id", "intra_nxn_enable", "constrained_intra_pred_flag",
					"strong_intra_smoothing_enabled_flag", "max_num_merge");
			}
			seq_printf(s, "%7d %7s %16d %27d %35d %13d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				pred_unit_params->intra_nxn_enable,
				pred_unit_params->constrained_intra_pred_flag,
				pred_unit_params->strong_intra_smoothing_enabled_flag,
				pred_unit_params->max_num_merge);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265vuitiming_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H265)) {
			mc_h265_timing_params_t *timing =
					&(dev->vpu_ctx[i].vui_timing.h265_timing);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265 timing----\n");
				seq_printf(s, "%7s %7s %21s %14s %33s\n", "enc_idx",
					"enc_id", "vui_num_units_in_tick",
					"vui_time_scale", "vui_num_ticks_poc_diff_one_minus1");
			}
			seq_printf(s, "%7d %7s %21d %14d %33d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				timing->vui_num_units_in_tick,
				timing->vui_time_scale,
				timing->vui_num_ticks_poc_diff_one_minus1);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265slice_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	mc_video_slice_params_t *slice_params;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H265)) {
			slice_params = &(dev->vpu_ctx[i].slice_params);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265 slice params----\n");
				seq_printf(s, "%7s %7s %27s %26s %25s %24s\n",
					"enc_idx", "enc_id", "h265_independent_slice_mode",
					"h265_independent_slice_arg", "h265_dependent_slice_mode",
					"h265_dependent_slice_arg");
			}
			seq_printf(s, "%7d %7s %27d %26d %25d %24d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				slice_params->h265_slice.h265_independent_slice_mode,
				slice_params->h265_slice.h265_independent_slice_arg,
				slice_params->h265_slice.h265_dependent_slice_mode,
				slice_params->h265_slice.h265_dependent_slice_arg);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265deblfilter_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H265)) {
			mc_video_deblk_filter_params_t *deblk_filter =
					&(dev->vpu_ctx[i].deblk_filter);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265 deblk filter----\n");
				seq_printf(s, "%7s %7s %37s %22s %20s %44s\n", "enc_idx",
					"enc_id", "slice_deblocking_filter_disabled_flag",
					"slice_beta_offset_div2", "slice_tc_offset_div2",
					"slice_loop_filter_across_slices_enabled_flag");
			}
			seq_printf(s, "%7d %7s %37d %22d %20d %44d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				deblk_filter->h265_deblk.slice_deblocking_filter_disabled_flag,
				deblk_filter->h265_deblk.slice_beta_offset_div2,
				deblk_filter->h265_deblk.slice_tc_offset_div2,
		deblk_filter->h265_deblk.slice_loop_filter_across_slices_enabled_flag);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_h265sao_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder) &&
			(dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H265)) {
			mc_h265_sao_params_t *sao_params = &(dev->vpu_ctx[i].sao_params);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode h265 sao param----\n");
				seq_printf(s, "%7s %7s %30s\n", "enc_idx", "enc_id",
					"sample_adaptive_offset_enabled_flag\n");
			}
			seq_printf(s, "%7d %7s %30d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				sao_params->sample_adaptive_offset_enabled_flag);
		}
	}

	return 0;
}

static int32_t vpu_venc_show_status(struct seq_file *s)
{
	uint32_t i;
	int32_t output;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	osal_spin_lock(&dev->vpu_ctx_spinlock);
	output = 0;
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (dev->vpu_ctx[i].context.encoder)) {
			mc_inter_status_t *status = &(dev->vpu_status[i].status);
			int32_t fps = dev->vpu_status[i].fps;
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode status----\n");
				seq_printf(s, "%7s %7s %17s %18s %15s %14s %19s %20s %7s\n",
					"enc_idx", "enc_id", "cur_input_buf_cnt",
					"cur_output_buf_cnt", "left_recv_frame", "left_enc_frame",
					"total_input_buf_cnt", "total_output_buf_cnt", "fps");
			}
			seq_printf(s, "%7d %7s %17d %18d %15d %14d %19d %20d %7d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				status->cur_input_buf_cnt,
				status->cur_output_buf_cnt,
				status->left_recv_frame,
				status->left_enc_frame,
				status->total_input_buf_cnt,
				status->total_output_buf_cnt,
				fps);
		}
	}
	osal_spin_unlock(&dev->vpu_ctx_spinlock);

	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_venc_show(struct seq_file *s, void *unused) /* PRQA S 3206 */
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;
	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	(void)vpu_venc_show_param(s);
	(void)vpu_venc_show_h264cbr_param(s);
	(void)vpu_venc_show_h264vbr_param(s);
	(void)vpu_venc_show_h264avbr_param(s);
	(void)vpu_venc_show_h264fixqp_param(s);
	(void)vpu_venc_show_h264qpmap_param(s);
	(void)vpu_venc_show_h265cbr_param(s);
	(void)vpu_venc_show_h265vbr_param(s);
	(void)vpu_venc_show_h265avbr_param(s);
	(void)vpu_venc_show_h265fixqp_param(s);
	(void)vpu_venc_show_h265qpmap_param(s);
	(void)vpu_venc_show_gop_param(s);
	(void)vpu_venc_show_intrarefresh_param(s);
	(void)vpu_venc_show_longtermref_param(s);
	(void)vpu_venc_show_roi_param(s);
	(void)vpu_venc_show_modedec_param(s);
	(void)vpu_venc_show_modedec2_param(s);
	(void)vpu_venc_show_h264entropy_param(s);
	(void)vpu_venc_show_h264slice_param(s);
	(void)vpu_venc_show_h264deblfilter_param(s);
	(void)vpu_venc_show_h264vuitiming_param(s);
	(void)vpu_venc_show_h264intrapred_param(s);
	(void)vpu_venc_show_h264transform_param(s);
	(void)vpu_venc_show_h265transform_param(s);
	(void)vpu_venc_show_h265predunit_param(s);
	(void)vpu_venc_show_h265vuitiming_param(s);
	(void)vpu_venc_show_h265slice_param(s);
	(void)vpu_venc_show_h265deblfilter_param(s);
	(void)vpu_venc_show_h265sao_param(s);
	(void)vpu_venc_show_h264vui_param(s);
	(void)vpu_venc_show_h265vui_param(s);
	(void)vpu_venc_show_3dnr_param(s);
	(void)vpu_venc_show_smart_bg_param(s);
	(void)vpu_venc_show_status(s);

	return 0;
}

static int32_t vpu_venc_open(struct inode *inodes, struct file *files) /* PRQA S 3673 */
{
	return single_open(files, vpu_venc_show, inodes->i_private);
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
static const struct file_operations vpu_venc_fops = {
	.open = vpu_venc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//////////////// vdec
static int32_t vpu_vdec_show_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;

	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	output = 0;
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (!dev->vpu_ctx[i].context.encoder)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "----decode param----\n");
				seq_printf(s, "%7s %7s %9s %7s %18s %19s %15s\n",
					"dec_idx", "dec_id", "feed_mode", "pix_fmt",
					"bitstream_buf_size", "bitstream_buf_count",
					"frame_buf_count");
			}
			seq_printf(s, "%7d %7s %9d %7d %18d %19d %15d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				dev->vpu_ctx[i].context.video_dec_params.feed_mode,
				dev->vpu_ctx[i].context.video_dec_params.pix_fmt,
				dev->vpu_ctx[i].context.video_dec_params.bitstream_buf_size,
				dev->vpu_ctx[i].context.video_dec_params.bitstream_buf_count,
				dev->vpu_ctx[i].context.video_dec_params.frame_buf_count);
		}
	}

	return 0;
}

static int32_t vpu_vdec_show_h264_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;

	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	output = 0;
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (!dev->vpu_ctx[i].context.encoder)) {
			if (dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H264) {
				mc_h264_dec_config_t *h264_dec_config =
					&dev->vpu_ctx[i].context.video_dec_params.h264_dec_config;
				if (output == 0) {
					output = 1;
					seq_printf(s, "----h264 decode param----\n");
					seq_printf(s, "%7s %7s %14s %9s %13s\n",
						"dec_idx", "dec_id", "reorder_enable",
						"skip_mode", "bandwidth_Opt");
				}
				seq_printf(s, "%7d %7s %14d %9d %13d\n",
					dev->vpu_ctx[i].context.instance_index,
					get_codec(&dev->vpu_ctx[i]),
					h264_dec_config->reorder_enable,
					h264_dec_config->skip_mode,
					h264_dec_config->bandwidth_Opt);
			}
		}
	}

	return 0;
}

static int32_t vpu_vdec_show_h265_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;

	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	output = 0;
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (!dev->vpu_ctx[i].context.encoder)) {
			if (dev->vpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_H265) {
				mc_h265_dec_config_t *h265_dec_config =
					&dev->vpu_ctx[i].context.video_dec_params.h265_dec_config;
				if (output == 0) {
					output = 1;
					seq_printf(s, "----h265 decode param----\n");
					seq_printf(s, "%7s %7s %14s %9s %13s %10s %20s %28s\n",
						"dec_idx", "dec_id", "reorder_enable", "skip_mode",
						"bandwidth_Opt", "cra_as_bla", "dec_temporal_id_mode",
						"target_dec_temporal_id_plus1");
				}
				seq_printf(s, "%7d %7s %14d %9d %13d %10d %20d %28d\n",
					dev->vpu_ctx[i].context.instance_index,
					get_codec(&dev->vpu_ctx[i]),
					h265_dec_config->reorder_enable,
					h265_dec_config->skip_mode,
					h265_dec_config->bandwidth_Opt,
					h265_dec_config->cra_as_bla,
					h265_dec_config->dec_temporal_id_mode,
					h265_dec_config->target_dec_temporal_id_plus1);
			}
		}
	}

	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_vdec_show_frame_info(struct seq_file *s)
{
	uint32_t i;
	int32_t output;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;

	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	osal_spin_lock(&dev->vpu_ctx_spinlock);
	output = 0;
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (!dev->vpu_ctx[i].context.encoder)) {
			mc_h264_h265_output_frame_info_t *frameinfo
								= &(dev->vpu_status[i].frame_info);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n----decode frameinfo----\n");
				seq_printf(s, "%7s %7s %13s %14s\n", "dec_idx", "dec_id",
					"display_width", "display_height");
			}
			seq_printf(s, "%7d %7s %13d %14d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				frameinfo->display_width,
				frameinfo->display_height);
		}
	}

	output = 0;
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		if ((dev->vpu_ctx[i].valid != 0) && (!dev->vpu_ctx[i].context.encoder)) {
			mc_inter_status_t *status =	&(dev->vpu_status[i].status);
			int32_t fps = dev->vpu_status[i].fps;
			if (output == 0) {
				output = 1;
				seq_printf(s, "----decode status----\n");
				seq_printf(s, "%7s %7s %17s %18s %19s %20s %7s\n", "dec_idx",
					"dec_id", "cur_input_buf_cnt", "cur_output_buf_cnt",
					"total_input_buf_cnt", "total_output_buf_cnt", "fps");
			}
			seq_printf(s, "%7d %7s %17d %18d %19d %20d %7d\n",
				dev->vpu_ctx[i].context.instance_index,
				get_codec(&dev->vpu_ctx[i]),
				status->cur_input_buf_cnt,
				status->cur_output_buf_cnt,
				status->total_input_buf_cnt,
				status->total_output_buf_cnt,
				fps);
		}
	}
	osal_spin_unlock(&dev->vpu_ctx_spinlock);

	return 0;
}
// PRQA S 0662 --

static int32_t vpu_vdec_show(struct seq_file *s, void *unused) /* PRQA S 3206 */
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;

	if (dev == NULL) {
		VPU_ERR("Invalid null device\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	(void)vpu_vdec_show_param(s);
	(void)vpu_vdec_show_h264_param(s);
	(void)vpu_vdec_show_h265_param(s);
	(void)vpu_vdec_show_frame_info(s);

	return 0;
}

static int32_t vpu_vdec_open(struct inode *inodes, struct file *files) /* PRQA S 3673 */
{
	return single_open(files, vpu_vdec_show, inodes->i_private);
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
static const struct file_operations vpu_vdec_fops = {
	.open = vpu_vdec_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int32_t vpu_loading_show(struct seq_file *s, void *unused) /* PRQA S 3206 */
{
	osal_time_t ts;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_vpu_dev_t *dev = (hb_vpu_dev_t *)s->private;

	if (dev == NULL) {
		VPU_ERR("Invalid null dev\n");
		return -EINVAL;
	}

	osal_time_get_real(&ts);
	osal_spin_lock(&dev->vpu_ctx_spinlock);
	if ((uint64_t)ts.tv_sec > (uint64_t)dev->vpu_loading_ts.tv_sec + dev->vpu_loading_setting) {
		seq_printf(s, "0\n");
	} else {
		seq_printf(s, "%lld.%01lld\n", dev->vpu_loading/10ULL, dev->vpu_loading%10ULL);
	}
	osal_spin_unlock(&dev->vpu_ctx_spinlock);
	return 0;
}

static int32_t vpu_loading_open(struct inode *inodes, struct file *files) /* PRQA S 3673 */
{
	return single_open(files, vpu_loading_show, inodes->i_private);
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
static const struct file_operations vpu_loading_fops = {
	.open = vpu_loading_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* code review E1: No need to return value */
//coverity[HIS_metric_violation:SUPPRESS]
static void vpu_destroy_resource(hb_vpu_dev_t *dev)
{
	if (dev->common_memory_mapped != 0U) {
		ion_iommu_unmap_ion_phys(dev->device,
			(dma_addr_t)dev->common_memory.iova,
			dev->common_memory.size);
	}
	if ((dev->common_memory.base != 0UL) &&
		(dev->common_memory.phys_addr != 0UL)) {
		(void)vpu_free_dma_buffer(dev, &dev->common_memory);
		(void)memset((void *)&dev->common_memory, 0x00, sizeof(dev->common_memory));
	}
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	ai_mode = hobot_fun_ai_mode_available();
	if (ai_mode) {
		if (dev->resmem.ion_vaddr != 0U) {
			ion_iommu_unmap_ion_phys(dev->device,
				dev->resmem.ion_vaddr, dev->resmem.size);
		}
	}
#endif
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	if (s_video_memory.base != 0UL) {
		osal_iounmap((void *)s_video_memory.base);
		s_video_memory.base = 0;
		vmem_exit(&s_vmem);
	}
#endif
#ifdef RESERVED_WORK_MEMORY
#if defined(CONFIG_HOBOT_FPGA_J5) || defined(CONFIG_HOBOT_J5) || defined(CONFIG_HOBOT_FPGA_HAPS_J5)
#else
	vpu_vunmap_work_buffer_memory(&dev->codec_mem_reserved2);
#endif
#endif

	if (!IS_ERR_OR_NULL((const void *)dev->vpu_ion_client)) {
		ion_client_destroy(dev->vpu_ion_client);
	}

	if (!IS_ERR_OR_NULL((const void *)dev->vpu_dev)) {
		device_destroy(dev->vpu_class, dev->vpu_dev_num);
	}

	if (dev->cdev_added != 0U) {
		cdev_del(&dev->cdev);
	}

	if (dev->cdev_allocted != 0U) {
		unregister_chrdev_region(dev->vpu_dev_num, 1);
	}

	if (!IS_ERR_OR_NULL((const void *)dev->vpu_class)) {
		class_destroy(dev->vpu_class);
	}
}

//#define ARRAY_CNTS	2	/* arrary: 0-start, 1-size */
//coverity[HIS_metric_violation:SUPPRESS]
static int32_t __init vpu_init_system_mem(struct platform_device *pdev,
						hb_vpu_dev_t *dev)
{
	struct resource *res;
#ifdef RESERVED_WORK_MEMORY
	int32_t i;
	//struct device_node *node;
#if defined(CONFIG_HOBOT_FPGA_J5) || defined(CONFIG_HOBOT_J5) || defined(CONFIG_HOBOT_FPGA_HAPS_J5)
	struct device_node *node, *rnode;
#else
	//uint32_t reserved_num = 0;
	uint64_t tmp_offset = 0;
	//uint64_t iova_reserved_reg;
	//uint64_t phys_reserved_reg;
#endif
#endif
	int32_t ret = 0;
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	uint32_t cmd_val = VPU_RES;
	hb_vpu_drv_resmem_t tmp_res;
#endif

	dev->drv_data = vpu_get_drv_data(pdev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		VPU_ERR_DEV(&pdev->dev, "failed to get memory resource\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOENT;
	}
	dev->vpu_mem = devm_request_mem_region(&pdev->dev, res->start, resource_size(res), /* PRQA S 3469 */
					pdev->name);
	if (dev->vpu_mem == NULL) {
		VPU_ERR_DEV(&pdev->dev, "failed to get memory region\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -ENOENT;
	}
	dev->regs_base = (void *)devm_ioremap(&pdev->dev, dev->vpu_mem->start,
					 resource_size(dev->vpu_mem));
	if (dev->regs_base == NULL) {
		VPU_ERR_DEV(&pdev->dev, "failed to ioremap address region\n");
		ret = -ENOENT;
		vpu_destroy_resource(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_2, 0, __LINE__);
		return ret;
	}

#ifdef RESERVED_WORK_MEMORY
#if defined(CONFIG_HOBOT_FPGA_J5) || defined(CONFIG_HOBOT_J5) || defined(CONFIG_HOBOT_FPGA_HAPS_J5)
	rnode = of_find_node_by_path("/reserved-memory");
	if (rnode == NULL) {
		VPU_ERR_DEV(&pdev->dev, "VPU can't find reserved mem\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_3, 0, __LINE__);
		return 0;
	}

	node = of_find_compatible_node(rnode, NULL, "codec-hw-work-buf");
	if (node != NULL) {
		if ((!of_address_to_resource(node, 0, &dev->codec_mem_reserved)) &&
				((dev->codec_mem_reserved.end - dev->codec_mem_reserved.start) >= (SIZE_WORK - 1))) {
			for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
				dev->dec_work_memory[i].base = dev->codec_mem_reserved.start + i * WAVE4DEC_WORKBUF_SIZE;
				dev->dec_work_memory[i].phys_addr = dev->dec_work_memory[i].base;
				dev->dec_work_memory[i].size = WAVE4DEC_WORKBUF_SIZE;
				//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				VPU_DBG_DEV(&pdev->dev, "Reserverd Codec Dec MEM start 0x%llx, size %d\n",
					dev->dec_work_memory[i].phys_addr,
					dev->dec_work_memory[i].size);
			}
			for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
				dev->enc_work_memory[i].base = dev->codec_mem_reserved.start +
						MAX_NUM_VPU_INSTANCE * WAVE4DEC_WORKBUF_SIZE +
						i * WAVE4ENC_WORKBUF_SIZE;
				dev->enc_work_memory[i].phys_addr = dev->enc_work_memory[i].base;
				dev->enc_work_memory[i].size = WAVE4ENC_WORKBUF_SIZE;
				//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				VPU_DBG_DEV(&pdev->dev, "Reserverd Codec Enc MEM start 0x%llx, size %d\n",
					dev->enc_work_memory[i].phys_addr,
					dev->enc_work_memory[i].size);
			}
		}else {
			VPU_ERR_DEV(&pdev->dev, "Invalid codec_hw_reserved memory\n");
			ret = -ENOENT;
			vpu_destroy_resource(dev);
			vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_4, 0, __LINE__);
			return ret;
		}
	} else {
		VPU_ERR_DEV(&pdev->dev, "Failed to get codec_hw_reserved memory\n");
		ret = -ENOENT;
		vpu_destroy_resource(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_5, 0, __LINE__);
		return ret;
	}
#else
#if 0
	node = of_parse_phandle(pdev->dev.of_node, "vpu-ddr", 0);
        ret = of_address_to_resource(node, 0, &source);
        if (ret) {
                VPU_ERR_DEV(&pdev->dev, "Failed to get vpu-ddr");
                return ret;
        }
        phys_reserved_reg = source.start;

        node = of_parse_phandle(pdev->dev.of_node, "vpu-iova", 0);
        ret = of_address_to_resource(node, 0, &source);
        if (ret) {
                VPU_ERR_DEV(&pdev->dev, "Failed to get vpu-ddr");
	}
        iova_reserved_reg = source.start;

        dev->codec_mem_reserved2.base = phys_reserved_reg;
        dev->codec_mem_reserved2.phys_addr = phys_reserved_reg;
        dev->codec_mem_reserved2.iova = iova_reserved_reg;
        dev->codec_mem_reserved2.size = 0x4000000;
	dev->codec_mem_reserved2.vaddr = vpu_vmap_work_buffer_memory(&dev->codec_mem_reserved2);
#endif

	dev->codec_mem_reserved2.size = 0x4000000;
	dma_alloc_coherent(&pdev->dev, dev->codec_mem_reserved2.size, &dev->codec_mem_reserved2.base,
			GFP_KERNEL);
	dev->codec_mem_reserved2.phys_addr = dev->codec_mem_reserved2.base;

	/* re-use ion_iommu_map_on_phys function to do iova & phys_addr mapping */
	ret = ion_iommu_map_ion_phys(&pdev->dev, dev->codec_mem_reserved2.phys_addr, dev->codec_mem_reserved2.size,
			&dev->codec_mem_reserved2.iova, IOMMU_READ | IOMMU_WRITE);
	if (ret < 0) {
			VPU_ERR_DEV(&pdev->dev, "Failed to map ion phys, ret = %d.\n", ret);
			vpu_destroy_resource(dev);
			vpu_send_diag_error_event((u16)EventIdVPUDevIonMapErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return ret;
	}
	dev->codec_mem_reserved2.vaddr = vpu_vmap_work_buffer_memory(&dev->codec_mem_reserved2);


	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		tmp_offset = i * WAVE521DEC_WORKBUF_SIZE;
		dev->dec_work_memory[i].base = dev->codec_mem_reserved2.base + tmp_offset;
		dev->dec_work_memory[i].phys_addr = dev->codec_mem_reserved2.phys_addr + tmp_offset;
		dev->dec_work_memory[i].iova = dev->codec_mem_reserved2.iova + tmp_offset;
		dev->dec_work_memory[i].size = WAVE521DEC_WORKBUF_SIZE;
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_DBG_DEV(&pdev->dev, "Reserverd Codec Dec MEM[%02d] phys 0x%llx, iova 0x%llx, size %d\n",
			i, dev->dec_work_memory[i].phys_addr, dev->dec_work_memory[i].iova, dev->dec_work_memory[i].size);
	}
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		tmp_offset = MAX_NUM_VPU_INSTANCE * WAVE521DEC_WORKBUF_SIZE + i * WAVE521ENC_WORKBUF_SIZE;
		dev->enc_work_memory[i].base = dev->codec_mem_reserved2.base + tmp_offset;
		dev->enc_work_memory[i].phys_addr = dev->codec_mem_reserved2.phys_addr + tmp_offset;
		dev->enc_work_memory[i].iova = dev->codec_mem_reserved2.iova + tmp_offset;
		dev->enc_work_memory[i].size = WAVE521ENC_WORKBUF_SIZE;
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_DBG_DEV(&pdev->dev, "Reserverd Codec Enc MEM[%02d] phys 0x%llx, iova 0x%llx, size %d\n",
			i, dev->enc_work_memory[i].phys_addr, dev->enc_work_memory[i].iova, dev->enc_work_memory[i].size);
	}
#endif
#endif	/* end of RESERVED_WORK_MEMORY */

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	ai_mode = hobot_fun_ai_mode_available();
	if (ai_mode) {
		dev->resmem.work_base = dev->dec_work_memory[0].base;
		//alloc ion mem
		ret = ion_get_heap_range(&tmp_res.ion_phys, &tmp_res.size);
		if (ret < 0) {
			VPU_ERR_DEV(&pdev->dev, "ion_get_heap_range failed, ret=%d\n", ret);
			vpu_destroy_resource(dev);
			vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_4, 0, __LINE__);
			return ret;
		}
		ret = ion_iommu_map_ion_phys(dev->device,
			tmp_res.ion_phys, tmp_res.size, &tmp_res.ion_vaddr, IOMMU_READ | IOMMU_WRITE);
		if (ret < 0) {
			VPU_ERR_DEV(&pdev->dev, "Failed to map ion phys, ret = %d.\n", ret);
			vpu_destroy_resource(dev);
			vpu_send_diag_error_event((u16)EventIdVPUDevIonMapErr, (u8)ERR_SEQ_1, 0, __LINE__);
			return ret;
		}

		dev->resmem.ion_phys = tmp_res.ion_phys;
		dev->resmem.ion_vaddr = tmp_res.ion_vaddr;
		dev->resmem.size = tmp_res.size;

		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		VPU_DBG_DEV(&pdev->dev, "Success to map ion phys, phys:0x%llx, vaddr:0x%llx, size:%ld, cmd_val=0x%x.\n",
			dev->resmem.ion_phys, dev->resmem.ion_vaddr, tmp_res.size, cmd_val);
		vpu_pac_ctrl_raise(cmd_val, (void *)&(dev->resmem));
	}
#endif

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_INFO_DEV(&pdev->dev,
		"vpu IO memory resource: physical base addr = 0x%llx,"
		"virtual base addr = %px\n", dev->vpu_mem->start,
		dev->regs_base);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

static int32_t __init vpu_init_system_irq(struct platform_device *pdev,
						hb_vpu_dev_t *dev)
{
	int32_t ret;
	int32_t irq;
#if 0
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		VPU_ERR_DEV(&pdev->dev, "failed to get irq resource\n");
		ret = -ENOENT;
		vpu_destroy_resource(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return ret;
	}
	dev->irq = (uint32_t)res->start;
#else
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		VPU_ERR_DEV(&pdev->dev, "Failed to get vpu_irq(%d)", irq);
		return -ENOENT;
	}
	dev->irq = (uint32_t)irq;
#endif
	// TODO Add top half irq and bottom half irq?
	ret = devm_request_threaded_irq(&pdev->dev, dev->irq, vpu_irq_handler, NULL,
				IRQF_TRIGGER_HIGH, pdev->name, (void *)dev);
	if (ret != 0) {
		VPU_ERR_DEV(&pdev->dev,
			"failed to install register interrupt handler\n");
		vpu_destroy_resource(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevReqIrqErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return ret;
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(&pdev->dev, "vpu irq number: irq = %d\n", dev->irq);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t __init vpu_init_system_device(const struct platform_device *pdev,
						hb_vpu_dev_t *dev)
{
	int32_t err;

	dev->vpu_class = class_create(THIS_MODULE, VPU_DEV_NAME); /* PRQA S 1021,3469 */
	if (IS_ERR((const void *)dev->vpu_class)) {
		VPU_ERR_DEV(&pdev->dev, "failed to create class\n");
		err = (int32_t)PTR_ERR((const void *)dev->vpu_class);
		vpu_destroy_resource(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevCreateDevErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return err;
	}

	/* get the major number of the character device */
	err = alloc_chrdev_region(&dev->vpu_dev_num, 0, 1, VPU_DEV_NAME);
	if (err < 0) {
		VPU_ERR_DEV(&pdev->dev, "failed to allocate character device\n");
		vpu_destroy_resource(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevCreateDevErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return err;
	} else {
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		dev->major = MAJOR(dev->vpu_dev_num); /* PRQA S 3469,4446 */
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		dev->minor = MINOR(dev->vpu_dev_num); /* PRQA S 3469,1840,4446,0499 */
		dev->cdev_allocted = 1U;
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(&pdev->dev, "vpu device number: major = %d, minor = %d\n",
		dev->major, dev->minor);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	/* initialize the device structure and register the device with the kernel */
	cdev_init(&dev->cdev, &vpu_fops);
	err = cdev_add(&dev->cdev, dev->vpu_dev_num, 1);
	if (err < 0) {
		VPU_ERR_DEV(&pdev->dev, "failed to add character device\n");
		vpu_destroy_resource(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevCreateDevErr, (u8)ERR_SEQ_2, 0, __LINE__);
		return err;
	}
	dev->cdev_added = 1U;

	dev->vpu_dev = device_create(dev->vpu_class, NULL, dev->vpu_dev_num,
					 NULL, VPU_DEV_NAME);
	if (IS_ERR((const void *)dev->vpu_dev)) {
		VPU_ERR_DEV(&pdev->dev, "failed to create device\n");
		err = (int32_t)PTR_ERR((const void *)dev->vpu_dev);
		dev->vpu_dev = NULL;
		vpu_destroy_resource(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevCreateDevErr, (u8)ERR_SEQ_3, 0, __LINE__);
		return err;
	}

	return err;
}

static int32_t __init vpu_init_system_res(struct platform_device *pdev,
					hb_vpu_dev_t *dev)
{
	int32_t err;

	err = vpu_init_system_mem(pdev, dev);
	if (err != 0) {
		return err;
	}

	err = vpu_init_system_irq(pdev, dev);
	if (err != 0) {
		return err;
	}

	err = vpu_init_system_device(pdev, dev);
	if (err != 0) {
		return err;
	}

	return err;
}

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
static int32_t __init vpu_init_reserved_mem(struct platform_device *pdev,
						hb_vpu_dev_t *dev)
{
	int32_t ret = 0;

	if (s_vmem.base_addr == 0) {
		/// *6 For test FHD and UHD stream,
		s_video_memory.size = VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE * 6;
		s_video_memory.phys_addr = VPU_DRAM_PHYSICAL_BASE;
		s_video_memory.base =
			(uint64_t)ioremap_nocache(s_video_memory.phys_addr,
			PAGE_ALIGN(s_video_memory.size)); /* PRQA S 3469,1840,1020,4491 */
		if (!s_video_memory.base) {
			VPU_ERR_DEV(&pdev->dev,
				"fail to remap video memory physical phys_addr=0x%lx,"
				"base==0x%lx, size=%d\n",
				s_video_memory.phys_addr, s_video_memory.base,
				(int32_t)s_video_memory.size);
			ret = -ENOMEM;
			vpu_destroy_resource(dev);
			vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return ret;
		}

		if (vmem_init(&s_vmem, s_video_memory.phys_addr,
			s_video_memory.size) < 0) {
			ret = -ENOMEM;
			VPU_ERR_DEV(&pdev->dev, "fail to init vmem system\n");
			vpu_destroy_resource(dev);
			vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
			return ret;
		}
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(&pdev->dev,
		"success to probe vpu device with reserved video memory"
		"phys_addr==0x%lx, base = =0x%lx\n", s_video_memory.phys_addr,
		s_video_memory.base);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}
#endif

static int32_t vpu_alloc_common_memory(hb_vpu_dev_t *dev) {
	int32_t ret = 0;

	dev->common_memory.size = SIZE_COMMON;
	dev->common_memory.flags = (VPU_ION_TYPE << VPU_ION_TYPE_BIT_OFFSET) | 0U;
	ret = vpu_alloc_dma_buffer(dev, &dev->common_memory);
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to alloc common memory.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return ret;
	}

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = ion_iommu_map_ion_phys(dev->device, dev->common_memory.phys_addr,
			dev->common_memory.size, &dev->common_memory.iova, IOMMU_READ | IOMMU_WRITE);
	if (ret != 0) {
		VPU_ERR_DEV(dev->device, "failed to map common phys.\n");
		(void)vpu_free_dma_buffer(dev, &dev->common_memory);
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return ret;
	}
	dev->common_memory_mapped = 1U;

	return 0;
}

static int32_t __init vpu_setup_module(const struct platform_device *pdev,
						hb_vpu_dev_t *dev)
{
	int32_t ret = 0;

	dev->vpu_freq = MAX_VPU_FREQ;
#ifdef VPU_ENABLE_CLOCK
	ret = hb_vpu_clk_get(dev, dev->vpu_freq);
	if (ret < 0) {
		VPU_ERR_DEV(&pdev->dev, "failed to get clock\n");
		vpu_destroy_resource(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevClkGetErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return ret;
	}
	hb_vpu_clk_put(dev);
#endif

	dev->vpu_ion_client = ion_client_create(hb_ion_dev, "vpu");
	if (IS_ERR((const void *)dev->vpu_ion_client)) {
		VPU_ERR_DEV(&pdev->dev, "failed to ion_client_create\n");
		ret = (int32_t)PTR_ERR((const void *)dev->vpu_ion_client);
		vpu_destroy_resource(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return ret;
	}

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	ai_mode = hobot_fun_ai_mode_available();
	if (!ai_mode) {
#endif
		ret = vpu_alloc_common_memory(dev);
		if (ret != 0) {
			VPU_ERR_DEV(&pdev->dev, "failed to alloc common memory.\n");
			vpu_destroy_resource(dev);
			return ret;
		}
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	}
#endif

	return ret;
}

static int32_t __init vpu_init_list(hb_vpu_dev_t *dev)
{
	uint32_t i;
	int32_t ret = 0;
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		osal_waitqueue_init(&dev->poll_wait_q[i]);
		osal_waitqueue_init(&dev->poll_int_wait_q[i]);
	}

#ifdef SUPPORT_MULTI_INST_INTR
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		osal_waitqueue_init(&dev->interrupt_wait_q[i]);
	}

	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		ret = osal_fifo_alloc(&dev->interrupt_pending_q[i],
				MAX_INTERRUPT_QUEUE * (uint32_t)sizeof(uint32_t), GFP_KERNEL);
		if (ret) {
			VPU_ERR_DEV(dev->device, "failed to do kfifo_alloc failed 0x%x\n", ret);
			vpu_destroy_resource(dev);
			vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return ret;
		}
	}
#else
	osal_waitqueue_init(&dev->interrupt_wait_q);
#endif

	osal_list_head_init(&dev->vbp_head);
	osal_list_head_init(&dev->inst_list_head);
	osal_list_head_init(&dev->dma_data_list_head);
	osal_list_head_init(&dev->phy_data_list_head);

	return ret;
}

/* code review E1: No need to return value */
//coverity[HIS_metric_violation:SUPPRESS]
static void __init vpu_init_lock(hb_vpu_dev_t *dev)
{
#ifdef USE_MUTEX_IN_KERNEL_SPACE
	uint32_t i;
#endif
	osal_spin_init(&dev->irq_spinlock); /* PRQA S 3200,3469,0662 */
	dev->irq_trigger = 0;

	dev->async_queue = NULL;
	dev->open_count = 0;
	osal_sema_init(&dev->vpu_sem, 1);
	osal_sema_init(&dev->vpu_free_inst_sem, MAX_NUM_VPU_INSTANCE);
	osal_spin_init(&dev->vpu_spinlock); /* PRQA S 3200,3469,0662 */
	osal_spin_init(&dev->vpu_ctx_spinlock); /* PRQA S 3200,3469,0662 */
#ifdef SUPPORT_MULTI_INST_INTR
	osal_spin_init(&dev->vpu_kfifo_lock);
#endif

#ifdef USE_MUTEX_IN_KERNEL_SPACE
	for (i=0; i < (uint32_t)VPUDRV_MUTEX_MAX; i++) {
		dev->current_vdi_lock_pid[i] = 0;
	}
#ifndef CONFIG_PREEMPT_RT
	osal_mutex_init(&dev->vpu_vdi_mutex);
	osal_mutex_init(&dev->vpu_vdi_disp_mutex);
	osal_mutex_init(&dev->vpu_vdi_reset_mutex);
	osal_mutex_init(&dev->vpu_vdi_vmem_mutex);
#else
	osal_sema_init(&dev->vpu_vdi_sem, 1);
	osal_sema_init(&dev->vpu_vdi_disp_sem, 1);
	osal_sema_init(&dev->vpu_vdi_reset_sem, 1);
	osal_sema_init(&dev->vpu_vdi_vmem_sem, 1);
#endif
#endif
}

static int32_t vpu_create_debug_file(hb_vpu_dev_t *dev)
{
	int32_t ret = 0;

	dev->debug_root = debugfs_create_dir("vpu", NULL);
	if (dev->debug_root == NULL) {
		VPU_ERR_DEV(dev->device, "%s: failed to create debugfs root directory.\n", __func__);
		vpu_destroy_resource(dev);
		vpu_send_diag_error_event((u16)EventIdVPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	dev->debug_file_venc = debugfs_create_file("venc", 0664, /* PRQA S 0339,3120 */
						dev->debug_root,
						(void *)dev, &vpu_venc_fops);
	if (dev->debug_file_venc == NULL) {
		char buf[MAX_FILE_PATH], *paths;

		paths = dentry_path_raw(dev->debug_root, buf, MAX_FILE_PATH);
		VPU_ERR_DEV(dev->device, "Failed to create client debugfs at %s/%s\n",
			paths, "venc");
	}

	dev->debug_file_vdec = debugfs_create_file("vdec", 0664, /* PRQA S 0339,3120 */
						dev->debug_root,
						(void *)dev, &vpu_vdec_fops);
	if (dev->debug_file_vdec == NULL) {
		char buf[MAX_FILE_PATH], *paths;

		paths = dentry_path_raw(dev->debug_root, buf, MAX_FILE_PATH);
		VPU_ERR_DEV(dev->device, "Failed to create client debugfs at %s/%s\n",
			paths, "vdec");
	}

	dev->vpu_loading_setting = 1;
	dev->vpu_irqenable = 1;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	debugfs_create_u64("irqenable", S_IRWXUGO, dev->debug_root, &dev->vpu_irqenable);
	debugfs_create_u64("loadingsetting", S_IRWXUGO, dev->debug_root, &dev->vpu_loading_setting);
	dev->debug_file_loading = debugfs_create_file("loading", 0664, /* PRQA S 0339,3120 */
						dev->debug_root,
						(void *)dev, &vpu_loading_fops);
	if (dev->debug_file_loading == NULL) {
		char buf[MAX_FILE_PATH], *paths;

		paths = dentry_path_raw(dev->debug_root, buf, MAX_FILE_PATH);
		VPU_ERR_DEV(dev->device, "Failed to create client debugfs at %s/%s\n",
			paths, "loading");
	}

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t __refdata vpu_probe(struct platform_device *pdev)
{
	hb_vpu_dev_t *dev;
	int32_t err = 0;

#if defined(CONFIG_HOBOT_FUSA_DIAG)
	err = diagnose_report_startup_status(ModuleDiag_vpu, MODULE_STARTUP_BEGIN);
	if (err != 0) {
		VPU_ERR_DEV(&pdev->dev, "%s: %d  diagnose_report_startup_status fail! ret=%d\n", __func__, __LINE__, ret);
	}
#endif

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(&pdev->dev, "%s()\n", __func__);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev = (hb_vpu_dev_t *)devm_kzalloc(&pdev->dev, sizeof(hb_vpu_dev_t), GFP_KERNEL);
	if (dev == NULL) {
		VPU_ERR_DEV(&pdev->dev, "Not enough memory for VPU device.\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevProbeErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOMEM;
	}
	dev->device = &pdev->dev;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev->plat_data = (hb_vpu_platform_data_t *)devm_kzalloc(&pdev->dev, sizeof(hb_vpu_platform_data_t),
						GFP_KERNEL);
	if (dev->plat_data == NULL) {
		VPU_ERR_DEV(&pdev->dev, "Not enough memory for VPU platform data\n");
		vpu_send_diag_error_event((u16)EventIdVPUDevProbeErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -ENOMEM;
	}
	pdev->dev.platform_data = (void *)dev->plat_data;

	hb_vpu_pm_ctrl(dev, 1, 1);

	err = vpu_parse_dts(dev->device->of_node, dev);
	if (err != 0) {
		vpu_send_diag_error_event((u16)EventIdVPUDevProbeErr, (u8)ERR_SEQ_2, 0, __LINE__);

		hb_vpu_pm_ctrl(dev, 1, 0);
		return err;
	}

	err = vpu_init_system_res(pdev, dev);
	if (err != 0) {
		vpu_send_diag_error_event((u16)EventIdVPUDevProbeErr, (u8)ERR_SEQ_3, 0, __LINE__);

		hb_vpu_pm_ctrl(dev, 1, 0);
		return err;
	}

	platform_set_drvdata(pdev, (void *)dev);

	err = vpu_setup_module(pdev, dev);
	if (err != 0) {
		vpu_send_diag_error_event((u16)EventIdVPUDevProbeErr, (u8)ERR_SEQ_4, 0, __LINE__);

		hb_vpu_pm_ctrl(dev, 1, 0);
		return err;
	}

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	err = vpu_init_reserved_mem(pdev, dev);
	if (err != 0) {
		vpu_send_diag_error_event((u16)EventIdVPUDevProbeErr, (u8)ERR_SEQ_5, 0, __LINE__);

		hb_vpu_pm_ctrl(dev, 1, 0);
		return err;
	}
#endif

	err = vpu_init_list(dev);
	if (err != 0) {
		vpu_send_diag_error_event((u16)EventIdVPUDevProbeErr, (u8)ERR_SEQ_6, 0, __LINE__);

		hb_vpu_pm_ctrl(dev, 1, 0);
		return err;
	}
	vpu_init_lock(dev);

#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
	err = vpu_prio_init(dev);
	if (err != 0) {
		VPU_ERR_DEV(&pdev->dev, "failed to init prio sched 0x%x\n", err);

		hb_vpu_pm_ctrl(dev, 1, 0);
		return err;
	}
#endif

	err = vpu_create_debug_file(dev);
	if (err != 0) {
		vpu_send_diag_error_event((u16)EventIdVPUDevProbeErr, (u8)ERR_SEQ_7, 0, __LINE__);

		hb_vpu_pm_ctrl(dev, 1, 0);
		return err;
	}

#if defined(CONFIG_HOBOT_FUSA_DIAG)
	ret = diagnose_report_startup_status(ModuleDiag_vpu, MODULE_STARTUP_END);
	if (ret != 0) {
		VPU_ERR_DEV(&pdev->dev, "%s: %d  diagnose_report_startup_status fail! ret=%d\n", __func__, __LINE__, ret);
	}
#endif

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	ai_mode = hobot_fun_ai_mode_available();
	if (ai_mode) {
		vpu_pac = dev;
		err = vpu_pac_notifier_register(&vpu_pac_notifier);
		if (err != 0) {
			VPU_ERR_DEV(&pdev->dev, "%s: %d  Register vpu pac notifier failed, err=%d\n", __func__, __LINE__, err);
		}
	}
#endif

	hb_vpu_pm_ctrl(dev, 0, 0);

	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_remove(struct platform_device *pdev) /* PRQA S 3673 */
{
#ifdef SUPPORT_MULTI_INST_INTR
	uint32_t i;
#endif
	int32_t ret =1;
	hb_vpu_dev_t *dev;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	VPU_DBG_DEV(&pdev->dev, "%s()\n", __func__);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev = (hb_vpu_dev_t *)platform_get_drvdata(pdev);

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	ai_mode = hobot_fun_ai_mode_available();
	if (ai_mode) {
		ret = vpu_pac_notifier_unregister(&vpu_pac_notifier);
		if (ret != 0) {
			VPU_ERR_DEV(&pdev->dev, "Unregister vpu pac notifier failed, ret=%d.\n", ret);
		}
	}
#endif
#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
	vpu_prio_deinit(dev);
#endif

	debugfs_remove_recursive(dev->debug_file_venc);
	debugfs_remove_recursive(dev->debug_file_vdec);
	debugfs_remove_recursive(dev->debug_file_loading);
	debugfs_remove_recursive(dev->debug_root);
	if (dev->instance_pool.base != 0UL) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		osal_free((void *)dev->instance_pool.base);
#else
		(void)vpu_free_dma_buffer(dev, &dev->instance_pool);
#endif
		dev->instance_pool.base = 0;
	}

#ifdef SUPPORT_MULTI_INST_INTR
	for (i = 0; i < MAX_NUM_VPU_INSTANCE; i++) {
		osal_fifo_free(&dev->interrupt_pending_q[i]);
	}
#endif

	vpu_destroy_resource(dev);

	ret = hb_vpu_hw_reset();
	if (ret != 0) {
		VPU_ERR_DEV(&pdev->dev, "Failed to reset vpu.\n");
	}

	hb_vpu_pm_ctrl(dev, 1, 0);

	return ret;
}

#if CONFIG_PM_SLEEP
//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_suspend(struct device *pdev) /* PRQA S 3673 */
{
	uint32_t core, reason;
	int32_t ret, i;
	/* vpu wait timeout to 1sec */
	unsigned long timeout = jiffies + (unsigned long)HZ; /* PRQA S 5209,1840 */
	uint32_t product_code;
	hb_vpu_dev_t *dev;
	int32_t cmd_sleep = W5_CMD_SLEEP_VPU;
	uint64_t suc_reg = W5_RET_SUCCESS;
	uint64_t fail_reg = W5_RET_FAIL_REASON;
	uint64_t reason_reg = W5_VPU_INT_REASON;

	// PRQA S 3469,2810,1021,3430,0306,0497 ++
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev = (hb_vpu_dev_t *) platform_get_drvdata(to_platform_device(pdev));
	// PRQA S 3469,2810,1021,3430,0306,0497 --

	if (dev->vpu_open_ref_count > 0) {
		ret = hb_vpu_clk_enable(dev, dev->vpu_freq);
		VPU_INFO_DEV(dev->device, "[+]vpu_suspend enter\n");
		if (ret != 0) {
			VPU_ERR_DEV(dev->device, "Failed to enable vpu clock.\n");
			vpu_send_diag_error_event((u16)EventIdVPUDevClkEnableErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EINVAL;
		}
		for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
			if (dev->bit_fm_info[core].size == 0U)
				continue;
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER); /* PRQA S 1006,0497,1843,1021 */
			if (product_code == WAVE420_CODE) {
				cmd_sleep = W4_CMD_SLEEP_VPU;
				suc_reg = W4_RET_SUCCESS;
				fail_reg = W4_RET_FAIL_REASON;
				reason_reg = W4_VPU_INT_REASON;
			}
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			reason = VPU_READL(reason_reg); /* PRQA S 1021,1006,0497 */
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_DBG_DEV(dev->device, "[VPUDRV] vpu_suspend core=%d, int_reason=0x%x\n",
				core, reason);
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
			if (reason != 0U) {
				VPU_ERR_DEV(dev->device, "Failed to go into sleep for existing interrupt.\n");
				if (hb_vpu_clk_disable(dev) != 0) {
					VPU_ERR_DEV(dev->device, "failed to disable clock.\n");
				}
				return -EAGAIN;
			}
			if (PRODUCT_CODE_W_SERIES(product_code)) { /* PRQA S 3469 */
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				while (VPU_READL(WAVE_VPU_BUSY_STATUS) != 0U) { /* PRQA S 1006,0497,1843,1021 */
					//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					if (time_after(jiffies, timeout)) { /* PRQA S 3469,3415,4394,4558,4115,1021,1020,2996 */
						VPU_INFO_DEV(dev->device, "SLEEP_VPU BUSY timeout\n");
						if (hb_vpu_clk_disable(dev) != 0) {
							VPU_ERR_DEV(dev->device, "failed to disable clock.\n");
						}
						return -EAGAIN;
					}
				}

				for (i = 0; i < COMMAND_REG_NUM; i++) {
					//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					dev->vpu_reg_store[core][i] = VPU_READL(WAVE_COMMAND + (i * REG_SIZE));
				}

				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				VPU_ISSUE_COMMAND(product_code, core, cmd_sleep); /* PRQA S 0497,1006,1021 */
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				while (VPU_READL(WAVE_VPU_BUSY_STATUS) != 0U) { /* PRQA S 1006,0497,1843,1021 */
					//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					if (time_after(jiffies, timeout)) { /* PRQA S 3469,3415,4394,4558,4115,1021,1020,2996 */
						VPU_INFO_DEV(dev->device, "SLEEP_VPU BUSY timeout\n");
						if (hb_vpu_clk_disable(dev) != 0) {
							VPU_ERR_DEV(dev->device, "failed to disable clock.\n");
						}
						return -EAGAIN;
					}
				}
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				if (VPU_READL(suc_reg) == 0U) { /* PRQA S 1006,0497,1843,1021 */
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					VPU_ERR_DEV(dev->device, "SLEEP_VPU failed [0x%x]", /* PRQA S 1021,1006,0497 */
						VPU_READL(fail_reg));
					if (hb_vpu_clk_disable(dev) != 0) {
						VPU_ERR_DEV(dev->device, "failed to disable clock.\n");
					}
					return -EAGAIN;
				}
			} else {
				VPU_ERR_DEV(dev->device, "[VPUDRV] Unknown product id : %08x\n",
					product_code);
				if (hb_vpu_clk_disable(dev) != 0) {
					VPU_ERR_DEV(dev->device, "failed to disable clock.\n");
				}
				return -EAGAIN;
			}
		}
		if (hb_vpu_clk_disable(dev) != 0) {
			VPU_ERR_DEV(dev->device, "failed to disable clock.\n");
	}
	}


	VPU_INFO_DEV(dev->device, "[-]vpu_suspend leave\n");
	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t vpu_resume(struct device *pdev) /* PRQA S 3673 */
{
	uint32_t core;
	//int32_t ret, i;
	int32_t  i;
	unsigned long timeout = jiffies + (unsigned long)HZ; /* PRQA S 5209,1840 */ /* vpu wait timeout to 1sec */
	uint32_t product_code;

	uint64_t code_base;
	uint32_t code_size;
	uint32_t code_size_mask = DEFAULT_CODE_SIZE_MASK;
	uint32_t remap_size;
	uint32_t remap_size_mask = DEFAULT_REMAP_SIZE_MASK;
	uint32_t regVal;
	uint32_t hwOption = 0;
	hb_vpu_dev_t *dev;
	uint32_t rst_block_all = W5_RST_BLOCK_ALL;
	int32_t cmd_init = W5_CMD_INIT_VPU;
	uint32_t max_code_buf_size_reg = W5_MAX_CODE_BUF_SIZE;
	uint64_t addr_code_base_reg = W5_ADDR_CODE_BASE;
	uint64_t code_size_reg = W5_CODE_SIZE;
	uint64_t code_param_reg = W5_CODE_PARAM;
	uint64_t timeout_cnt_reg = W5_INIT_VPU_TIME_OUT_CNT;
	uint64_t hw_opt_reg = W5_HW_OPTION;
	uint64_t suc_reg = W5_RET_SUCCESS;
	uint64_t fail_reg = W5_RET_FAIL_REASON;

#if defined(CONFIG_HOBOT_FUSA_DIAG)
	ret = diagnose_report_wakeup_status(ModuleDiag_vpu, MODULE_WAKEUP_BEGIN);
	if (ret != 0) {
		VPU_ERR_DEV(pdev, "%s: %d  diagnose_report_startup_status fail! ret=%d\n", __func__, __LINE__, ret);
	}
#endif

	// PRQA S 3469,2810,1021,3430,0306,0497 ++
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev = (hb_vpu_dev_t *) platform_get_drvdata(to_platform_device(pdev));
	// PRQA S 3469,2810,1021,3430,0306,0497 --
	VPU_INFO_DEV(dev->device, "[+]vpu_resume enter\n");
	// ret = hb_vpu_clk_enable(dev, dev->vpu_freq);
	// if (ret != 0) {
	// 	VPU_ERR_DEV(dev->device, "Failed to enable vpu clock.\n");
	// 	vpu_send_diag_error_event((u16)EventIdVPUDevClkEnableErr, (u8)ERR_SEQ_0, 0, __LINE__);
	// 	return -EINVAL;
	// }

	for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
		if (dev->bit_fm_info[core].size == 0U) {
			continue;
		}
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		product_code = VPU_READL(VPU_PRODUCT_CODE_REGISTER); /* PRQA S 1006,0497,1843,1021 */
		if (product_code == WAVE420_CODE) {
			rst_block_all = W4_RST_BLOCK_ALL;
			max_code_buf_size_reg = W4_MAX_CODE_BUF_SIZE;
			addr_code_base_reg = W4_ADDR_CODE_BASE;
			code_size_reg = W4_CODE_SIZE;
			code_param_reg = W4_CODE_PARAM;
			timeout_cnt_reg = W4_INIT_VPU_TIME_OUT_CNT;
			hw_opt_reg = W4_HW_OPTION;
			suc_reg = W4_RET_SUCCESS;
			fail_reg = W4_RET_FAIL_REASON;
			cmd_init = W4_CMD_INIT_VPU;
		}
		if (PRODUCT_CODE_W_SERIES(product_code)) { /* PRQA S 3469 */
			code_base = dev->common_memory.iova;
			/* ALIGN TO 4KB */
			code_size = (max_code_buf_size_reg & ~code_size_mask);
			if (code_size < (dev->bit_fm_info[core].size * CODE_SIZE_TIMES)) {
				VPU_ERR_DEV(dev->device, "Invalid code size %d.(%d)",
					code_size, dev->bit_fm_info[core].size);
				return 0;
			}

			regVal = 0;
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(WAVE_PO_CONF, regVal); /* PRQA S 0497,1021,1006 */

			/* Reset All blocks */
			regVal = rst_block_all;
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(WAVE_VPU_RESET_REQ, regVal); /* PRQA S 0497,1021,1006 */

			/* clear registers */
			for (i = 0; i < COMMAND_REG_NUM; i++) {
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				VPU_WRITEL(WAVE_COMMAND + (i * REG_SIZE), 0x00);
			}

			/* Waiting reset done */
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			while (VPU_READL(WAVE_VPU_RESET_STATUS) != 0U) { /* PRQA S 1006,0497,1843,1021 */
				//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				if (time_after(jiffies, timeout)) { /* PRQA S 3469,3415,4394,4558,4115,1021,1020,2996 */
					VPU_ERR_DEV(dev->device, "Timeout to wait reset done.");
					return 0;
				}
			}
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(WAVE_VPU_RESET_REQ, 0); /* PRQA S 0497,1021,1006 */

			/* remap page size */
			remap_size = (code_size >> CODE_SIZE_OFFSET) & remap_size_mask;
			regVal =
				DEFAULT_REMAP_REG_VAL | ((uint32_t)WAVE_REMAP_CODE_INDEX << (uint32_t)REMAP_REG_OFFSET_1) |
				((uint32_t)0U << (uint32_t)REMAP_REG_OFFSET_2)
				| ((uint32_t)1U << (uint32_t)REMAP_REG_OFFSET_3) | remap_size;
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(WAVE_VPU_REMAP_CTRL, regVal); /* PRQA S 0497,1021,1006 */
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(WAVE_VPU_REMAP_VADDR, 0x00000000); /* PRQA S 0497,1021,1006 */ /* DO NOT CHANGE! */
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(WAVE_VPU_REMAP_PADDR, (uint32_t)code_base); /* PRQA S 0497,1021,1006 */
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(addr_code_base_reg, (uint32_t)code_base); /* PRQA S 0497,1021,1006 */
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(code_size_reg, (uint32_t)code_size); /* PRQA S 0497,1021,1006 */
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(code_param_reg, 0); /* PRQA S 0497,1021,1006 */
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(timeout_cnt_reg, timeout); /* PRQA S 0497,1021,1006 */
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(hw_opt_reg, hwOption); /* PRQA S 0497,1021,1006 */

			/* Interrupt */
			if ((product_code == WAVE521_CODE) ||
				(product_code == WAVE521C_CODE)) {
				regVal  = ((uint32_t)1U << (uint32_t)INT_WAVE5_ENC_SET_PARAM);
				regVal |= ((uint32_t)1U << (uint32_t)INT_WAVE5_ENC_PIC);
				regVal |= ((uint32_t)1U << (uint32_t)INT_WAVE5_INIT_SEQ);
				regVal |= ((uint32_t)1U << (uint32_t)INT_WAVE5_DEC_PIC);
				regVal |= ((uint32_t)1U << (uint32_t)INT_WAVE5_BSBUF_EMPTY);
			} else if (product_code == WAVE420_CODE) {
				regVal  = ((uint32_t)1U << (uint32_t)W4_INT_DEC_PIC_HDR);
				regVal |= ((uint32_t)1U << (uint32_t)W4_INT_DEC_PIC);
				regVal |= ((uint32_t)1U << (uint32_t)W4_INT_QUERY_DEC);
				regVal |= ((uint32_t)1U << (uint32_t)W4_INT_SLEEP_VPU);
				regVal |= ((uint32_t)1U << (uint32_t)W4_INT_BSBUF_EMPTY);
			} else {
				// decoder
				regVal  = ((uint32_t)1U << (uint32_t)INT_WAVE5_INIT_SEQ);
				regVal |= ((uint32_t)1U << (uint32_t)INT_WAVE5_DEC_PIC);
				regVal |= ((uint32_t)1U << (uint32_t)INT_WAVE5_BSBUF_EMPTY);
			}
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(WAVE_VPU_VINT_ENABLE, regVal); /* PRQA S 0497,1021,1006 */
			//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_ISSUE_COMMAND(product_code, core, cmd_init); /* PRQA S 0497,1006,1021 */
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			VPU_WRITEL(WAVE_VPU_REMAP_CORE_START, 1); /* PRQA S 0497,1021,1006 */
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			while (VPU_READL(WAVE_VPU_BUSY_STATUS) != 0U) { /* PRQA S 1006,0497,1843,1021 */
				//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				if (time_after(jiffies, timeout)) { /* PRQA S 3469,3415,4394,4558,4115,1021,1020,2996 */
					VPU_ERR_DEV(dev->device, "Timeout to wait vpu busy status.");
					return 0;
				}
			}
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			if (VPU_READL(suc_reg) == 0U) { /* PRQA S 1006,0497,1843,1021 */
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				VPU_ERR_DEV(dev->device, "WAKEUP_VPU failed [0x%x]\n", /* PRQA S 1006,1021,0497 */
					VPU_READL(fail_reg));
				return 0;
			}
			for (i = 0; i < COMMAND_REG_NUM; i++) {
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				VPU_WRITEL(WAVE_COMMAND + (i * REG_SIZE), dev->vpu_reg_store[core][i]);
			}
		} else {
			VPU_ERR_DEV(dev->device, "[VPUDRV] Unknown product id : %08x\n",
				product_code);
			return 0;
		}
	}

	// if (hb_vpu_clk_disable(dev) != 0) {
	// 	VPU_ERR_DEV(dev->device, "failed to disable clock.\n");
	// }

	VPU_INFO_DEV(dev->device, "[-]vpu_resume leave\n");

#if defined(CONFIG_HOBOT_FUSA_DIAG)
	ret = diagnose_report_wakeup_status(ModuleDiag_vpu, MODULE_WAKEUP_END);
	if (ret != 0) {
		VPU_ERR_DEV(pdev, "%s: %d  diagnose_report_startup_status fail! ret=%d\n", __func__, __LINE__, ret);
	}
#endif

	return 0;
}
#else
#define    vpu_suspend    NULL
#define    vpu_resume    NULL
#endif /* !CONFIG_PM_SLEEP */

/* Power management */
static const struct dev_pm_ops vpu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS((vpu_suspend), (vpu_resume))
};

MODULE_DEVICE_TABLE(of, vpu_of_match);

static struct platform_driver vpu_driver = {
	.probe = vpu_probe,
	.remove = vpu_remove,
	.driver = {
		   .name = VPU_PLATFORM_DEVICE_NAME,
		   .of_match_table = vpu_of_match,
		   .pm = &vpu_pm_ops
		   },
};

module_platform_driver(vpu_driver); /* PRQA S 3219,0605,1036,3449,3219,0605,1035,3451,3432 */

//PRQA S 0633,3213,0286 ++
MODULE_AUTHOR("Hobot");
MODULE_DESCRIPTION("Hobot video processing unit linux driver");
MODULE_LICENSE("GPL v2");
//PRQA S 0633,3213,0286 --
