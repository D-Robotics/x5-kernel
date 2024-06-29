/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#define pr_fmt(fmt)    "hobot_jpu_dev: " fmt

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/eventpoll.h>
#include <linux/debugfs.h>
#include <linux/sched/signal.h>
#if defined(CONFIG_HOBOT_FUSA_DIAG)
#include <linux/hobot_diag.h>
#endif

#include "osal.h"
#include "hobot_jpu_ctl.h"
#include "hobot_jpu_debug.h"
#include "hobot_jpu_pm.h"
#include "hobot_jpu_reg.h"
#include "hobot_jpu_utils.h"

#define JPU_DRIVER_API_VERSION_MAJOR 1
#define JPU_DRIVER_API_VERSION_MINOR 2

#define PTHREAD_MUTEX_T_HANDLE_SIZE 4
#define MAX_FILE_PATH 256
#define TIMEOUT_MS_TO_NS 1000000UL
#define JPU_GET_SEM_TIMEOUT    (120UL * (uint64_t)HZ)

#define JPU_ENABLE_CLOCK

/**
 * Purpose: JPU log debug information switch
 * Value: [0, ]
 * Range: hobot_jpu_dev.c
 * Attention: NA
 */
static int32_t jpu_debug_info = 0;
/**
 * Purpose: JPU performance report switch
 * Value: [0, 1]
 * Range: hobot_jpu_dev.c
 * Attention: NA
 */
static int32_t jpu_pf_bw_debug = 0;
/**
 * Purpose: JPU frequency
 * Value: (0, MAX_JPU_FREQ]
 * Range: hobot_jpu_dev.c
 * Attention: NA
 */
static unsigned long jpu_clk_freq = MAX_JPU_FREQ; /* PRQA S 5209 */

#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
#define JPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (16*1024*1024)
#define JPU_DRAM_PHYSICAL_BASE (0x63E00000)	//(0x8AA00000)
static jpu_mm_t s_jmem;
static hb_jpu_drv_buffer_t s_video_memory = { 0 };
#endif

/* this definition is only for chipsnmedia FPGA board env */
/* so for SOC env of customers can be ignored */

/*for kernel up to 3.7.0 version*/
#ifndef VM_RESERVED
#define VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#ifdef CONFIG_PM
/* implement to power management functions */
#endif

// PRQA S 0339,4501,4543,3219,4542,4464,0636,0605,0315,4548,0634,3432,1840,5209 ++
module_param(jpu_clk_freq, ulong, 0644);
module_param(jpu_debug_info, int, 0644);
module_param(jpu_pf_bw_debug, int, 0644);
// PRQA S 0339,4501,4543,3219,4542,4464,0636,0605,0315,4548,0634,3432,1840,5209 --

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
static hb_jpu_dev_t *jpu_pac;
static bool ai_mode;

enum jpu_pac_ctrl {
	JPU_NONE = 0,
	JPU_RES,
	JPU_CLKON,
	JPU_CLKOFF,
	JPU_HWRST,
	JPU_IRQ,
	JPU_IRQDONE,
	JPU_NOUSE,
};

int32_t jpu_pac_notifier_register(struct notifier_block *nb);
int32_t jpu_pac_notifier_unregister(struct notifier_block *nb);
void jpu_pac_raise_irq(uint32_t cmd, void *res);
bool hobot_fun_ai_mode_available(void);

static int32_t jpu_pac_dev_event(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	int32_t ret;

	if (event == JPU_CLKON) {
		ret = hb_jpu_clk_enable(jpu_pac, jpu_pac->jpu_freq);
		if (ret != 0) {
			JPU_ERR_DEV(jpu_pac->device, "Failed to enable jpu clock, ret=%d.\n", ret);
			return NOTIFY_BAD;
		}
	} else if (event == JPU_CLKOFF) {
		ret = hb_jpu_clk_disable(jpu_pac);
		if (ret != 0) {
			JPU_ERR_DEV(jpu_pac->device, "Failed to disable jpu clock, ret=%d.\n", ret);
			return NOTIFY_BAD;
		}
	} else if (event == JPU_HWRST) {
		ret = hb_jpu_hw_reset();
		if (ret != 0) {
			JPU_ERR_DEV(jpu_pac->device, "Failed to hw reset jpu, ret=%d.\n", ret);
			return NOTIFY_BAD;
		}
	} else if (event == JPU_IRQDONE) {
		enable_irq(jpu_pac->irq);
	} else {
		JPU_ERR_DEV(jpu_pac->device, "Invalid jpu event, event=%ld.\n", event);
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}

static struct notifier_block jpu_pac_notifier = {
	.notifier_call  = jpu_pac_dev_event,
};
#endif

#if defined(CONFIG_HOBOT_FUSA_DIAG)
static void jpu_send_diag_error_event(u16 id, u8 sub_id, u8 pri_data, u16 line_num)
{
	s32 ret;
	struct diag_event event;

	memset(&event, 0, sizeof(event));
	event.module_id = ModuleDiag_jpu;
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
		JPU_ERR("%s fail %d\n", __func__, ret);
}
#else
static void jpu_send_diag_error_event(u16 id, u8 sub_id, u8 pri_data, u16 line_num)
{
	/* NULL */
}
#endif

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_alloc_dma_buffer(hb_jpu_dev_t *dev,
			hb_jpu_drv_buffer_t * jb)
{
#ifndef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
	struct ion_handle * handle;
	int32_t ret;
	size_t size;
	void * vaddr;
	uint32_t memtypes;
#endif

	if ((jb == NULL) || (dev == NULL)) {
		JPU_ERR("Invalid parameters jb=%p, dev=%p\n", jb, dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevAllocDMABufErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
	jb->phys_addr = (uint64_t)jmem_alloc(&s_jmem, jb->size, 0);
	if (jb->phys_addr == (uint64_t)-1) {
		JPU_ERR_DEV(dev->device, "Physical memory allocation error size=%d\n",
			jb->size);
		jpu_send_diag_error_event((u16)EventIdJPUDevAllocDMABufErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -ENOMEM;
	}

	jb->base = (uint64_t)(s_video_memory.base + (jb->phys_addr -
				s_video_memory.phys_addr));
#else
#if 0
	jb->base = (uint64_t)dma_alloc_coherent(dev->device, PAGE_ALIGN(jb->size), /* PRQA S 3469,1840,1020,4491 */
					(dma_addr_t *) (&jb->phys_addr),
					GFP_DMA | GFP_KERNEL);
	if ((void *)(jb->base) == NULL) {
		JPU_ERR_DEV(dev->device, "Physical memory allocation error size=%d\n", jb->size);
		return -ENOMEM;
	}
#endif
	if (dev->jpu_ion_client == NULL) {
		JPU_ERR_DEV(dev->device, "JPU ion client is NULL.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAllocDMABufErr, (u8)ERR_SEQ_2, 0, __LINE__);
		return -EINVAL;
	}

	memtypes = (jb->flags >> MEM_TYPE_OFFSET) & MEM_TYPE_MASK;
	if ((memtypes == (uint32_t)DEC_BS) || (memtypes == (uint32_t)ENC_BS) ||
		(memtypes == (uint32_t)ENC_SRC) || (memtypes == (uint32_t)DEC_FB_LINEAR)) {
		jb->flags |= ((uint32_t)ION_FLAG_CACHED | (uint32_t)ION_FLAG_CACHED_NEEDS_SYNC);
	}

	handle = ion_alloc(dev->jpu_ion_client, jb->size, 0,
		ION_HEAP_TYPE_CMA_RESERVED_MASK, jb->flags);
	if (IS_ERR_OR_NULL(handle)) {
		JPU_ERR_DEV(dev->device, "failed to allocate ion memory, ret = %p.\n", handle);
		jpu_send_diag_error_event((u16)EventIdJPUDevAllocDMABufErr, (u8)ERR_SEQ_3, 0, __LINE__);
		return (int32_t)PTR_ERR(handle);
	}

	ret = ion_phys(dev->jpu_ion_client, handle->id,
			(phys_addr_t *)&jb->phys_addr, &size);
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "failed to get ion phys address(ret=%d).\n", ret);
		ion_free(dev->jpu_ion_client, handle);
		jpu_send_diag_error_event((u16)EventIdJPUDevAllocDMABufErr, (u8)ERR_SEQ_4, 0, __LINE__);
		return ret;
	}

	vaddr = ion_map_kernel(dev->jpu_ion_client, handle);
	if (IS_ERR_OR_NULL(vaddr)) {
		JPU_ERR_DEV(dev->device, "failed to get ion virt address(ret=%d).\n", (int32_t)PTR_ERR(vaddr));
		ion_free(dev->jpu_ion_client, handle);
		jpu_send_diag_error_event((u16)EventIdJPUDevAllocDMABufErr, (u8)ERR_SEQ_5, 0, __LINE__);
		return -EFAULT;
	}

	/*fd = ion_share_dma_buf_fd(dev->jpu_ion_client, handle);
	if (fd < 0) {
		JPU_ERR_DEV(dev->device, "failed to get ion share fd(ret=%d).\n", fd);
		ion_unmap_kernel(dev->jpu_ion_client, handle);
		ion_free(dev->jpu_ion_client, handle);
		return fd;
	}*/
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	jb->base = (uint64_t)vaddr;
	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	jb->handle = (int64_t)handle;
	jb->fd = 0;
	jb->share_id = handle->share_id;
#endif /* JPU_SUPPORT_RESERVED_VIDEO_MEMORY */
	return 0;
}

static int32_t jpu_free_dma_buffer(hb_jpu_dev_t *dev,
				hb_jpu_drv_buffer_t * jb)
{
#ifndef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
	struct ion_handle *handle;
#endif

	if ((jb == NULL) || (dev == NULL)) {
		JPU_ERR("Invalid parameters jb=%p, dev=%p\n", jb, dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
	if (jb->base != 0UL)
		jmem_free(&s_jmem, jb->phys_addr, 0);
#else
#if 0
	if (jb->base != 0UL)
		dma_free_coherent(dev->device, PAGE_ALIGN(jb->size), (void *)jb->base, /* PRQA S 3469,1840,1020,4491 */
			jb->phys_addr);
#endif
	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	handle = (struct ion_handle *)jb->handle;
	ion_unmap_kernel(dev->jpu_ion_client, handle);
	ion_free(dev->jpu_ion_client, handle);
#endif /* JPUR_SUPPORT_RESERVED_VIDEO_MEMORY */

	return 0;
}

static void jpu_clear_status(hb_jpu_dev_t *dev, uint32_t idx)
{
	uint32_t val;

	val = JPU_READL(MJPEG_PIC_STATUS_REG(idx));
	JPU_WRITEL(MJPEG_PIC_STATUS_REG(idx), val);
	JPU_WRITEL(MJPEG_PIC_START_REG(idx), 0);
	val = JPU_READL(MJPEG_INST_CTRL_STATUS_REG);
	val &= ~(1U << idx);
	JPU_WRITEL(MJPEG_INST_CTRL_STATUS_REG, val);
}

#ifdef USE_SHARE_SEM_BT_KERNEL_AND_USER
static void *get_mutex_base(const hb_jpu_dev_t *dev, u32 core, hb_jpu_mutex_t type)
{
	uint32_t instance_pool_size_per_core;
	void *jip_base;
	void *jdi_mutexes_base;
	void *jdi_mutex;

	if (dev->instance_pool.base == 0U) {
		JPU_ERR_DEV(dev->device, "Invalid parameters.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return NULL;
	}

	/* s_instance_pool.size  assigned to the size of all core once call
	VDI_IOCTL_GET_INSTANCE_POOL by user. */
	instance_pool_size_per_core = (dev->instance_pool.size/MAX_NUM_JPU_CORE);
	jip_base = (void *)(dev->instance_pool.base + (instance_pool_size_per_core /* PRQA S 1891 */
				* core));
	jdi_mutexes_base = (jip_base + (instance_pool_size_per_core - /* PRQA S 0497 */
						(sizeof(void *) * JDI_NUM_LOCK_HANDLES)));
	if (type == JPUDRV_MUTEX_JPU) {
		jdi_mutex = (void *)(jdi_mutexes_base + ((int32_t)JPUDRV_MUTEX_JPU * sizeof(void *))); /* PRQA S 0497,1840 */
	} else if (type == JPUDRV_MUTEX_JMEM) {
		jdi_mutex = (void *)(jdi_mutexes_base + ((int32_t)JPUDRV_MUTEX_JMEM * sizeof(void *))); /* PRQA S 0497,1840 */
	} else {
		JPU_ERR_DEV(dev->device, "unknown MUTEX_TYPE type=%d\n", type);
		jpu_send_diag_error_event((u16)EventIdJPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
		jdi_mutex = NULL;
	}
	return jdi_mutex;
}

static int32_t jdi_lock_release(const struct file *filp, hb_jpu_dev_t *dev, uint32_t core, hb_jpu_mutex_t type)
{
	void *mutex;
	volatile int32_t *sync_lock_ptr;
	int32_t sync_val = current->tgid;
	unsigned long timeout = jiffies + HZ;
	uint64_t flags_mp;
	int32_t i;
	uint32_t val;

	mutex = get_mutex_base(dev, core, type);
	if (mutex == NULL) {
		JPU_ERR("Fail to get mutex base, core=%d, type=%d\n", 0, type);
		return -EINVAL;
	}

	sync_lock_ptr = (volatile int32_t *)mutex;
	if (*sync_lock_ptr == sync_val) {
		JPU_DBG_DEV(dev->device, "Free core=%d, type=%d, sync_lock=%d, current_pid=%d, "
			"tgid=%d, sync_val=%d, \n", core, type,
			(volatile int32_t)*sync_lock_ptr, current->pid, current->tgid, sync_val);
		osal_spin_lock_irqsave(&dev->irq_spinlock, &flags_mp);
		for (i = 0; i < MAX_HW_NUM_JPU_INSTANCE; i++) {
			if (dev->interrupt_flag[i] == 1) {
				if (dev->irq_trigger == 1) {
					jpu_clear_status(dev, i);
					enable_irq(dev->irq);
					dev->irq_trigger = 0;
					dev->poll_int_event--;
				}
				dev->interrupt_flag[i] = 0;
				break;
			} else {
				if (JPU_READL(MJPEG_PIC_STATUS_REG(i)) != 0) {
					dev->ignore_irq = 1;
				}
			}
		}
		if (i == MAX_HW_NUM_JPU_INSTANCE) {
			val = JPU_READL(MJPEG_INST_CTRL_STATUS_REG);
			if ((val >> 4) == INST_CTRL_ENC) {
				dev->ignore_irq = 1;
			}
		}
		osal_spin_unlock_irqrestore(&dev->irq_spinlock, &flags_mp);
		while (JPU_READL(MJPEG_INST_CTRL_STATUS_REG) >> 4 == INST_CTRL_ENC) {
			if (time_after(jiffies, timeout)) {
				break;
			}
		}
		osal_spin_lock_irqsave(&dev->irq_spinlock, &flags_mp);
		for (i = 0; i < MAX_HW_NUM_JPU_INSTANCE; i++) {
			if (JPU_READL(MJPEG_PIC_STATUS_REG(i)) != 0) {
				jpu_clear_status(dev, 0);
				dev->ignore_irq = 0;
				break;
			}
		}
		osal_spin_unlock_irqrestore(&dev->irq_spinlock, &flags_mp);
		(void)__sync_lock_release(sync_lock_ptr); /* PRQA S 3335 */
	}

	return 0;
}
#elif defined(USE_MUTEX_IN_KERNEL_SPACE)
static int32_t jdi_lock_base(struct file *filp, hb_jpu_dev_t *dev, hb_jpu_mutex_t type,
			int32_t fromUserspace)
{
	int32_t ret;
	if (type >= JPUDRV_MUTEX_JPU && type < JPUDRV_MUTEX_MAX) {
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
#ifndef CONFIG_PREEMPT_RT
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		JPU_DBG_DEV(dev->device, "[+] type=%d, LOCK pid %lld, pid=%d, tgid=%d, "
			"fromuser %d.\n",
			type, dev->current_jdi_lock_pid[type], current->pid,
			current->tgid, fromUserspace);
#else
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		JPU_DBG_DEV(dev->device, "[+] type=%d, semcnt %d LOCK pid %lld, pid=%d, tgid=%d, "
			"fromuser %d.\n",
			type, type == JPUDRV_MUTEX_JPU ?
			dev->jpu_jdi_sem.count : dev->jpu_jdi_jmem_sem.count,
			dev->current_jdi_lock_pid[type], current->pid,
			current->tgid, fromUserspace);
#endif
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	}

	if (type == JPUDRV_MUTEX_JPU) {
#ifndef CONFIG_PREEMPT_RT
		ret = mutex_lock_interruptible(&dev->jpu_jdi_mutex);
#else
		ret = osal_sema_down_interruptible(&dev->jpu_jdi_sem);
#endif
		if (fromUserspace == 0) {
			if (ret == -EINTR) {
#ifndef CONFIG_PREEMPT_RT
				ret = mutex_lock_killable(&dev->jpu_jdi_mutex);
#else
				ret = down_killable(&dev->jpu_jdi_sem);
#endif
			}
		}
	} else if (type == JPUDRV_MUTEX_JMEM) {
#ifndef CONFIG_PREEMPT_RT
		ret = mutex_lock_interruptible(&dev->jpu_jdi_jmem_mutex);
#else
		ret = osal_sema_down_interruptible(&dev->jpu_jdi_jmem_sem);
#endif
		if (fromUserspace == 0) {
			if (ret == -EINTR) {
#ifndef CONFIG_PREEMPT_RT
				ret = mutex_lock_killable(&dev->jpu_jdi_jmem_mutex);
#else
				ret = down_killable(&dev->jpu_jdi_jmem_sem);
#endif
			}
		}
	} else {
		JPU_ERR_DEV(dev->device, "unknown MUTEX_TYPE type=%d\n", type);
		jpu_send_diag_error_event((u16)EventIdJPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	if (ret == 0) {
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		dev->current_jdi_lock_pid[type] = (int64_t)filp;
	} else {
		JPU_ERR_DEV(dev->device, "down_interruptible error ret=%d\n", ret);
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
#ifndef CONFIG_PREEMPT_RT
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[-] type=%d, LOCK pid %lld, pid=%d, tgid=%d, "
		"fromuser %d ret = %d.\n",
		type, dev->current_jdi_lock_pid[type], current->pid,
		current->tgid, fromUserspace, ret);
#else
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[-] type=%d, semcnt %d LOCK pid %lld, pid=%d, tgid=%d, "
		"fromuser %d ret = %d.\n",
		type, type == JPUDRV_MUTEX_JPU ?
		dev->jpu_jdi_sem.count : dev->jpu_jdi_jmem_sem.count,
		dev->current_jdi_lock_pid[type], current->pid,
		current->tgid, fromUserspace, ret);
#endif
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

static int32_t jdi_lock_user(struct file *filp, hb_jpu_dev_t *dev, hb_jpu_mutex_t type)
{
	return jdi_lock_base(filp, dev, type, 1);
}

static int32_t jdi_unlock(hb_jpu_dev_t *dev, hb_jpu_mutex_t type)
{
	if (type >= JPUDRV_MUTEX_JPU && type < JPUDRV_MUTEX_MAX) {
		dev->current_jdi_lock_pid[type] = 0;
	} else {
		JPU_ERR_DEV(dev->device, "unknown MUTEX_TYPE type=%d\n", type);
		jpu_send_diag_error_event((u16)EventIdJPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
#ifndef CONFIG_PREEMPT_RT
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[+] type=%d, LOCK pid %lld pid=%d, tgid=%d\n",
		type, dev->current_jdi_lock_pid[type], current->pid, current->tgid);
#else
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[+] type=%d, semcnt %d LOCK pid %lld pid=%d, tgid=%d\n",
		type, type == JPUDRV_MUTEX_JPU ?
		dev->jpu_jdi_sem.count : dev->jpu_jdi_jmem_sem.count,
		dev->current_jdi_lock_pid[type], current->pid, current->tgid);
#endif
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	//coverity[misra_c_2012_rule_15_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (type == JPUDRV_MUTEX_JPU) {
#ifndef CONFIG_PREEMPT_RT
		osal_mutex_unlock(&dev->jpu_jdi_mutex);
#else
		osal_sema_up(&dev->jpu_jdi_sem);
#endif
	} else if (type == JPUDRV_MUTEX_JMEM) {
#ifndef CONFIG_PREEMPT_RT
		osal_mutex_unlock(&dev->jpu_jdi_jmem_mutex);
#else
		osal_sema_up(&dev->jpu_jdi_jmem_sem);
#endif
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
#ifndef CONFIG_PREEMPT_RT
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[-] type=%d, LOCK pid %lld pid=%d, tgid=%d\n",
		type, dev->current_jdi_lock_pid[type], current->pid, current->tgid);
#else
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[-] type=%d, semcnt %d LOCK pid %lld pid=%d, tgid=%d\n",
		type, type == JPUDRV_MUTEX_JPU ?
		dev->jpu_jdi_sem.count : dev->jpu_jdi_jmem_sem.count,
		dev->current_jdi_lock_pid[type], current->pid, current->tgid);
#endif
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return 0;
}

static int32_t jdi_lock_release(const struct file *filp, hb_jpu_dev_t *dev,
				u32 core, hb_jpu_mutex_t type)
{
	uint32_t i, val;
	unsigned long flags_mp; /* PRQA S 5209 */
	/* jpu wait timeout to 50 msec */
	unsigned long timeout = jiffies + (50 * HZ) / 1000; /* PRQA S 5209,1840 */

	if ((type < JPUDRV_MUTEX_JPU) || (type >= JPUDRV_MUTEX_MAX)) {
		JPU_ERR_DEV(dev->device, "unknown MUTEX_TYPE type=%d\n", type);
		return -EINVAL;
	}
	if (dev->current_jdi_lock_pid[type] == (int64_t)filp) {
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
		JPU_INFO_DEV(dev->device, "MUTEX_TYPE: %d, JDI_LOCK_PID: %lld, current->pid: %d, "
			"current->tgid=%d, filp=%px\n", type, dev->current_jdi_lock_pid[type],
			current->pid, current->tgid, filp);

		// PRQA S 1021,3473,1020,3432,2996,3200 ++
		osal_spin_lock_irqsave(&dev->irq_spinlock, (uint64_t *)&flags_mp);
		// PRQA S 1021,3473,1020,3432,2996,3200 --
		for (i = 0; i < MAX_HW_NUM_JPU_INSTANCE; i++) {
			if (dev->interrupt_flag[i] == 1) {
				if (dev->irq_trigger == 1) {
					//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					JPU_DBG_DEV(dev->device, "jpu_lock_release ignore irq.\n");
					jpu_clear_status(dev, i);
					enable_irq(dev->irq);
					dev->irq_trigger = 0;
					dev->poll_int_event--;
				}
				dev->interrupt_flag[i] = 0;
				break;
			} else {
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				if (JPU_READL(MJPEG_PIC_STATUS_REG(i)) != 0U) {
					dev->ignore_irq = 1;
				}
			}
		}
		if (i == MAX_HW_NUM_JPU_INSTANCE) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			val = JPU_READL(MJPEG_INST_CTRL_STATUS_REG);
			if ((val >> 4) == (uint32_t)INST_CTRL_ENC) {
				dev->ignore_irq = 1;
			}
		}
		osal_spin_unlock_irqrestore(&dev->irq_spinlock, (uint64_t *)&flags_mp);
		timeout = jiffies + (50UL * (unsigned long)HZ) / 1000UL;
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		while (JPU_READL(MJPEG_INST_CTRL_STATUS_REG) >> 4 == (uint32_t)INST_CTRL_ENC) {
			if (time_after(jiffies, timeout)) { /* PRQA S 3469,3415,4394,4558,4115,1021,1020,2996 */
				JPU_ERR_DEV(dev->device, "Wait Interrupt BUSY timeout pid %d\n", current->pid);
				break;
			}
		}
		// PRQA S 1021,3473,1020,3432,2996,3200 ++
		osal_spin_lock_irqsave(&dev->irq_spinlock, (uint64_t *)&flags_mp);
		// PRQA S 1021,3473,1020,3432,2996,3200 --
		for (i = 0; i < MAX_HW_NUM_JPU_INSTANCE; i++) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			if (JPU_READL(MJPEG_PIC_STATUS_REG(i)) != 0U) {
				jpu_clear_status(dev, 0);
				dev->ignore_irq = 0;
				break;
			}
		}
		osal_spin_unlock_irqrestore(&dev->irq_spinlock, (uint64_t *)&flags_mp);

		if (i < MAX_HW_NUM_JPU_INSTANCE) {
			JPU_INFO_DEV(dev->device, "Clear status(pid %d).\n", current->pid);
		}

		// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
		jdi_unlock(dev, type);
	}

	return 0;
}
#else
#endif

/* code review E1: No need to return value */
//coverity[HIS_metric_violation:SUPPRESS]
static void jpu_clear_instance_status(hb_jpu_dev_t *dev,
				hb_jpu_drv_instance_list_t *vil)
{
	if (dev->inst_open_flag[vil->inst_idx] != 0) {
		dev->jpu_open_ref_count--;
	}
	dev->inst_open_flag[vil->inst_idx] = 0;
	osal_list_del(&vil->list);
	(void)osal_test_and_clear_bit(vil->inst_idx, (volatile uint64_t *)dev->jpu_inst_bitmap); /* PRQA S 4446 */
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev->poll_event[vil->inst_idx] = LLONG_MIN;
	osal_wake_up(&dev->poll_wait_q[vil->inst_idx]); /*PRQA S 3469*/
	osal_wake_up(&dev->poll_int_wait); /*PRQA S 3469*/
	(void)memset((void *)&dev->jpu_ctx[vil->inst_idx], 0x00,
		sizeof(dev->jpu_ctx[vil->inst_idx]));
	(void)memset((void *)&dev->jpu_status[vil->inst_idx], 0x00,
		sizeof(dev->jpu_status[vil->inst_idx]));
	osal_kfree((void *)vil);
}

static void jpu_force_free_lock(hb_jpu_dev_t *dev)
{
#if defined(USE_MUTEX_IN_KERNEL_SPACE)
	int32_t i;
	for (i = 0; i < (int32_t)JPUDRV_MUTEX_MAX; i++) {
		if (dev->current_jdi_lock_pid[i] != 0) {
			JPU_DBG_DEV(dev->device, "RELEASE MUTEX_TYPE: %d, JDI_LOCK_PID: %lld, current->pid: %d, current->tgid: %d.\n",
			i, dev->current_jdi_lock_pid[i], current->pid, current->tgid);
			jdi_unlock(dev, i);
		}
	}
#elif defined(USE_SHARE_SEM_BT_KERNEL_AND_USER)
#endif
}

static void jpu_free_lock(const struct file *filp, hb_jpu_dev_t *dev, hb_jpu_drv_instance_pool_t *jip)
{
#if defined(USE_MUTEX_IN_KERNEL_SPACE) || defined(USE_SHARE_SEM_BT_KERNEL_AND_USER)
	int32_t ret;
#else
	uint32_t instance_pool_size_per_core;
	void *jdi_mutexes_base;
	uint32_t PTHREAD_MUTEX_T_DESTROY_VALUE = 0xdead10cc;
#endif


#if defined(USE_MUTEX_IN_KERNEL_SPACE) || defined(USE_SHARE_SEM_BT_KERNEL_AND_USER)
	ret = jdi_lock_release(filp, dev, (u32)0, JPUDRV_MUTEX_JPU);
	if (ret != 0) {
		jpu_send_diag_error_event((u16)EventIdJPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}
	ret = jdi_lock_release(filp, dev, (u32)0, JPUDRV_MUTEX_JMEM);
	if (ret != 0) {
		jpu_send_diag_error_event((u16)EventIdJPUDevParamCheckErr, (u8)ERR_SEQ_1, 0, __LINE__);
	}
#else
	instance_pool_size_per_core =
		(uint32_t)(dev->instance_pool.size / MAX_NUM_JPU_CORE);
	jdi_mutexes_base = (jip +
		(instance_pool_size_per_core -
		(uint32_t)PTHREAD_MUTEX_T_HANDLE_SIZE * JDI_NUM_LOCK_HANDLES));
	JPU_INFO_DEV(dev->device, "force to destroy "
		"jdi_mutexes_base=%p in userspace \n",
		jdi_mutexes_base);
	if (jdi_mutexes_base) {
		uint32_t i;
		for (i = 0; i < JDI_NUM_LOCK_HANDLES; i++) {
			memcpy((uint32_t *)jdi_mutexes_base,
				&PTHREAD_MUTEX_T_DESTROY_VALUE,
				PTHREAD_MUTEX_T_HANDLE_SIZE);
			jdi_mutexes_base +=
				PTHREAD_MUTEX_T_HANDLE_SIZE;
		}
	}
#endif
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_free_instances(const struct file *filp)
{
	hb_jpu_drv_instance_list_t *vil, *n;
	hb_jpu_drv_instance_pool_t *vip = NULL;
	void *jip_base;
	uint32_t instance_pool_size_per_core;
	hb_jpu_dev_t *dev;
	hb_jpu_priv_t *priv;
	uint32_t core_idx = 0;

	if (filp == NULL || filp->private_data == NULL) {
		JPU_ERR("failed to free jpu buffers, filp is null.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_jpu_priv_t *)filp->private_data;
	dev = priv->jpu_dev;
	if (dev == NULL) {
		JPU_ERR("failed to free jpu buffers, dev is null.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}
	/* s_instance_pool.size  assigned to the size of all core once call
	   JDI_IOCTL_GET_INSTANCE_POOL by user. */
	instance_pool_size_per_core =
	    (uint32_t)(dev->instance_pool.size / MAX_NUM_JPU_CORE);

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(vil, n, &dev->inst_list_head, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		if (vil->filp == filp) {
			// core_idx need set when multi-core (todo zhaojun.li)
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			jip_base = (void *)(dev->instance_pool.base + /* PRQA S 1891 */
						((uint64_t)instance_pool_size_per_core * (uint64_t)core_idx));
			JPU_INFO_DEV(dev->device, "jpu_free_instances detect instance crash "
				"instIdx=%d, jip_base=%p, instance_pool_size_per_core=%u, current_pid=%d, tgid=%d, filp=%px\n",
				vil->inst_idx, jip_base,
				instance_pool_size_per_core,
				current->pid, current->tgid, filp);
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			vip = (hb_jpu_drv_instance_pool_t *) jip_base;
			if (vip != NULL) {
				/* only first 4 byte is key point(inUse of CodecInst in jpuapi)
				   to free the corresponding instance. */
				(void)memset((void *)&vip->jpgInstPool[vil->inst_idx], 0x00, PTHREAD_MUTEX_T_HANDLE_SIZE);
				jpu_free_lock(filp, dev, vip);
			}
			jpu_clear_instance_status(dev, vil);
		}
	}

	if (vip == NULL) {
		// Note clear the lock for the core index 0 during pre-initialzing.
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		jip_base = (void *)(dev->instance_pool.base + /* PRQA S 1891 */
			(instance_pool_size_per_core * 0UL));
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		vip = (hb_jpu_drv_instance_pool_t *) jip_base;
		(void)jpu_free_lock(filp, dev, vip);
	}

	return 0;
}

/* code review E1: No need to return value */
//coverity[HIS_metric_violation:SUPPRESS]
static void jpu_free_dma_data_list(const struct file *filp,
				const hb_jpu_dev_t *dev)
{
	hb_jpu_ion_dma_list_t *dma_tmp, *dma_n;
	hb_jpu_phy_dma_list_t *phy_tmp, *phy_n;

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
static void jpu_free_drv_buffer(const struct file *filp,
				hb_jpu_dev_t *dev)
{
	hb_jpu_drv_buffer_pool_t *pool, *n;
	hb_jpu_drv_buffer_t jb;
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(pool, n, &dev->jbp_head, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		if (pool->filp == filp) {
			jb = pool->jb;
			if (jb.base != 0UL) {
				(void)jpu_free_dma_buffer(dev, &jb);
				osal_list_del(&pool->list);
				osal_kfree((void *)pool);
			}
		}
	}
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_free_buffers(const struct file *filp)
{
	hb_jpu_dev_t *dev;
	hb_jpu_priv_t *priv;
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	if (filp == NULL) {
		JPU_ERR("failed to free jpu buffers, filp is null.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_jpu_priv_t *)filp->private_data;
	dev = priv->jpu_dev;

	if (dev == NULL) {
		JPU_ERR("failed to free jpu buffers, dev is null.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}

	jpu_free_dma_data_list(filp, dev);

	jpu_free_drv_buffer(filp, dev);
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
static irqreturn_t jpu_irq_handler(int32_t irq, void *dev_id) /* PRQA S 3206 */
{
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	ai_mode = hobot_fun_ai_mode_available();
	if (ai_mode) {
		hb_jpu_dev_t *dev = (hb_jpu_dev_t *) dev_id;
		disable_irq_nosync(dev->irq);
		jpu_pac_raise_irq(JPU_IRQ, NULL);
	} else {
#else
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		hb_jpu_dev_t *dev = (hb_jpu_dev_t *) dev_id;
		uint32_t i, reg_offset;
		u32 flag;

		osal_spin_lock(&dev->irq_spinlock);
#ifdef JPU_IRQ_CONTROL
		disable_irq_nosync(dev->irq);
		dev->irq_trigger = 1;
#endif

		for (i = 0; i < MAX_HW_NUM_JPU_INSTANCE; i++) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			flag = JPU_READL(MJPEG_PIC_STATUS_REG(i)); /* PRQA S 1840,1006,0497,1021,3432 */
			if (flag != 0U) {
				reg_offset = MJPEG_PIC_STATUS_REG(i); /* PRQA S 1840,3432 */
				// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
				//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				JPU_DBG_DEV(dev->device, "[%u] INTERRUPT FLAG: %08x, %08x, %08x\n", i,
					dev->interrupt_reason[i], flag, JPU_READL(MJPEG_INST_CTRL_STATUS_REG));
				// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

				// clear interrupt status register JPU_WRITEL(MJPEG_PIC_STATUS_REG(i), flag);

				// notify the interrupt to userspace
				if (dev->async_queue != NULL)
					kill_fasync(&dev->async_queue, SIGIO, POLL_IN);

				if (dev->ignore_irq != 1) {
					dev->interrupt_reason[i] = flag;
					dev->interrupt_flag[i] = 1;
					osal_wake_up(&dev->interrupt_wait_q[i]); /*PRQA S 3469*/
					osal_spin_lock(&dev->poll_int_wait.lock);
					dev->poll_int_event++;
					// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
					//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					JPU_DBG_DEV(dev->device, "jpu_irq_handler poll_int_event[%d]=%lld.\n",
						i, dev->poll_int_event);
					// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
					dev->total_poll++;
					// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
					//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					JPU_DBG_DEV(dev->device, "jpu_irq_handler total_poll=%lld.\n", dev->total_poll);
					// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
					osal_spin_unlock(&dev->poll_int_wait.lock);
					osal_wake_up(&dev->poll_int_wait); /*PRQA S 3469*/
				} else {
					//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					JPU_DBG_DEV(dev->device, "jpu_irq_handler ignore irq.\n");
					enable_irq(dev->irq);
					dev->irq_trigger = 0;
					dev->interrupt_flag[i] = 0;
					dev->ignore_irq = 0;
				}
				break;
			}
		}

		if (i == MAX_HW_NUM_JPU_INSTANCE) {
			dev->irq_trigger = 0;
			dev->interrupt_flag[0] = 0;
			dev->ignore_irq = 0;
			enable_irq(dev->irq);
		}
		osal_spin_unlock(&dev->irq_spinlock);
#endif
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	} //for else
#endif
	return IRQ_HANDLED;
}

static int32_t jpu_parse_dts(const struct device_node *np, const hb_jpu_dev_t * dev)
{
	hb_jpu_platform_data_t *pdata = dev->plat_data;
	int32_t ret;
	if (np == NULL) {
		dev_err(dev->device, "Invalid device node\n");
		return -EINVAL;
	}

	ret = of_property_read_s32(np, "ip_ver", &pdata->ip_ver);
	if (ret != 0) {
		pdata->ip_ver = 0;
	}
	ret = of_property_read_s32(np, "clock_rate", &pdata->clock_rate);
	if (ret != 0) {
		pdata->clock_rate = 0;
	}
	ret = of_property_read_s32(np, "min_rate", &pdata->min_rate);
	if (ret != 0) {
		pdata->min_rate = 0;
	}

	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_open(struct inode *inodes, struct file *filp) /* PRQA S 3673 */
{
#ifdef JPU_ENABLE_CLOCK
	int32_t ret = 0;
#endif
	hb_jpu_dev_t *dev;
	hb_jpu_priv_t *priv;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev = osal_list_entry(inodes->i_cdev, hb_jpu_dev_t, cdev); /* PRQA S 3432,2810,0306,0662,1021,0497 */
	if (dev == NULL) {
		JPU_ERR("failed to get jpu dev data\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	/* code review D6: Dynamic alloc memory to record different instance index */
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_jpu_priv_t *)osal_kzalloc(sizeof(hb_jpu_priv_t), OSAL_KMALLOC_ATOMIC);
	if (priv == NULL) {
		JPU_ERR_DEV(dev->device, "failed to alloc memory.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -ENOMEM;
	}

	osal_spin_lock(&dev->jpu_spinlock);
	if (dev->open_count == 0U) {
		dev->jpu_freq = jpu_clk_freq;

		/* power-domain might sleep or schedule, needs do spin_unlock */
		osal_spin_unlock(&dev->jpu_spinlock);
		hb_jpu_pm_ctrl(dev, 0, 1);
		osal_spin_lock(&dev->jpu_spinlock);
	}
	osal_spin_unlock(&dev->jpu_spinlock);

#ifdef JPU_ENABLE_CLOCK
	ret = hb_jpu_clk_enable(dev, dev->jpu_freq);
	if (ret != 0) {
		osal_kfree((void *)priv);
		JPU_ERR_DEV(dev->device, "failed to enable clock.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevClkEnableErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
#endif

	osal_spin_lock(&dev->jpu_spinlock);
	dev->open_count++;
	priv->jpu_dev = dev;
	priv->inst_index = MAX_JPU_INSTANCE_IDX;
	priv->is_irq_poll = 0;
	filp->private_data = (void *)priv;
	osal_spin_unlock(&dev->jpu_spinlock);

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_alloc_physical_memory(struct file *filp,
					hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_drv_buffer_pool_t *jbp;
	hb_jpu_drv_buffer_t tmp;
	int32_t ret;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&tmp, (const void *) arg,
			sizeof(hb_jpu_drv_buffer_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	ret = osal_sema_down_interruptible(&dev->jpu_sem);
	if (ret == 0) {
		/* code review D6: Dynamic alloc memory to record allocated physical memory */
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		jbp = (hb_jpu_drv_buffer_pool_t *)osal_kzalloc(sizeof(hb_jpu_drv_buffer_pool_t), OSAL_KMALLOC_ATOMIC);
		if (jbp == NULL) {
			osal_sema_up(&dev->jpu_sem);
			JPU_ERR_DEV(dev->device, "Failed to allocate memory.\n");
			jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -ENOMEM;
		}
		jbp->jb = tmp;

		ret = jpu_alloc_dma_buffer(dev, &(jbp->jb));
		if (ret != 0) {
			osal_kfree((void *)jbp);
			osal_sema_up(&dev->jpu_sem);
			JPU_ERR_DEV(dev->device, "Failed to allocate dma memory.\n");
			return -ENOMEM;
		}
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&(jbp->jb),
				sizeof(hb_jpu_drv_buffer_t));
		if (ret != 0) {
			(void)jpu_free_dma_buffer(dev, &(jbp->jb));
			osal_kfree((void *)jbp);
			osal_sema_up(&dev->jpu_sem);
			JPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
			jpu_send_diag_error_event((u16)EventIdJPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EFAULT;
		}

		jbp->filp = filp;
		osal_spin_lock(&dev->jpu_spinlock);
		osal_list_add(&jbp->list, &dev->jbp_head);
		osal_spin_unlock(&dev->jpu_spinlock);

		osal_sema_up(&dev->jpu_sem);
	}

	return ret;
}

static int32_t jpu_free_physical_memory(hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_drv_buffer_pool_t *jbp, *n;
	hb_jpu_drv_buffer_t jb;
	int32_t ret;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&jb, (const void __user *) arg,
			sizeof(hb_jpu_drv_buffer_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EACCES;
	}

	ret = osal_sema_down_interruptible(&dev->jpu_sem);
	if (ret == 0) {
		if (jb.base != 0UL) {
			(void)jpu_free_dma_buffer(dev, &jb);
		}

		osal_spin_lock(&dev->jpu_spinlock);
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		osal_list_for_each_entry_safe(jbp, n, &dev->jbp_head, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
			if (jbp->jb.base == jb.base) {
				osal_list_del(&jbp->list);
				osal_kfree((void *)jbp);
				break;
			}
		}
		osal_spin_unlock(&dev->jpu_spinlock);

		osal_sema_up(&dev->jpu_sem);
	}

	return ret;
}

#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
static int32_t jpu_get_reserved_memory_info(
					hb_jpu_dev_t *dev, u_long arg)
{
	int32_t ret = 0;
	if (s_video_memory.base != 0) {
		ret = osal_copy_to_app((void __user *)arg, &s_video_memory,
				sizeof(hb_jpu_drv_buffer_t));
		if (ret != 0) {
			jpu_send_diag_error_event((u16)EventIdJPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
			ret = -EFAULT;
		}
	} else {
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		ret = -EFAULT;
	}
	return ret;
}
#endif /* JPU_SUPPORT_RESERVED_VIDEO_MEMORY */

#ifdef USE_MUTEX_IN_KERNEL_SPACE
static int32_t jpu_jdi_lock(struct file *filp, hb_jpu_dev_t *dev, u_long arg)
{
	uint32_t mutex_type;
	int32_t ret = 0;
	uint64_t copy_ret = 0UL;

	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	copy_ret = osal_copy_from_app(&mutex_type, (uint32_t *)arg, sizeof(uint32_t));
	if (copy_ret != 0UL) {
		JPU_ERR_DEV(dev->device, "failed to copy from user.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	ret = jdi_lock_user(filp, dev, mutex_type);

	return ret;
}

static int32_t jpu_jdi_unlock(struct file *filp, hb_jpu_dev_t *dev, u_long arg)
{
	uint32_t mutex_type;
	int32_t ret = 0;
	uint64_t copy_ret = 0UL;

	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	copy_ret = osal_copy_from_app(&mutex_type, (uint32_t *)arg, sizeof(uint32_t));
	if (copy_ret != 0UL) {
		JPU_ERR_DEV(dev->device, "failed to copy from user.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	ret = jdi_unlock(dev, mutex_type);
	if (ret != 0) {
		jpu_send_diag_error_event((u16)EventIdJPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}

	return ret;
}
#endif

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t calc_jpu_loading(hb_jpu_dev_t *dev, u32 intr_inst_index)
{
	osal_time_t ts;
	media_codec_context_t *context = &(dev->jpu_ctx[intr_inst_index].context);
	mc_mjpeg_jpeg_output_frame_info_t *frame_infos =	&(dev->jpu_status[intr_inst_index].frame_info);

	osal_spin_lock(&dev->jpu_ctx_spinlock);
	if (context->encoder) {
		dev->jenc_loading += (uint64_t)context->video_enc_params.width * (uint64_t)context->video_enc_params.height;
	} else {
		dev->jdec_loading += (uint64_t)frame_infos->display_height * (uint64_t)frame_infos->display_width;
	}

	osal_time_get_real(&ts);
	if ((uint64_t)ts.tv_sec >= (uint64_t)dev->jpu_loading_ts.tv_sec + dev->jpu_loading_setting) {
		dev->jpu_loading_ts = ts;
		dev->jpu_loading = dev->jenc_loading * JPU_MAX_RADIO/JPU_MAX_JENC_CAPACITY/dev->jpu_loading_setting
			+ dev->jdec_loading * JPU_MAX_RADIO/JPU_MAX_JDEC_CAPACITY/dev->jpu_loading_setting;
		if (dev->jpu_loading > JPU_MAX_RADIO) {
			dev->jpu_loading = JPU_MAX_RADIO;
		}
		dev->jenc_loading = 0;
		dev->jdec_loading = 0;
	}
	osal_spin_unlock(&dev->jpu_ctx_spinlock);
	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_wait_interrupt(hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_drv_intr_t info;
	u32 instance_no;
#ifdef SUPPORT_TIMEOUT_RESOLUTION
	ktime_t kt;
#endif
	int32_t ret;
	unsigned long flags_mp; /* PRQA S 5209 */

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&info, (const void __user *) arg,
			sizeof(hb_jpu_drv_intr_t));
	if (ret != 0 ||  info.inst_idx >= MAX_HW_NUM_JPU_INSTANCE) {
		JPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	instance_no = info.inst_idx;
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "INSTANCE NO: %d Timeout: %d interrupt flag: %d\n",
		instance_no, info.timeout, dev->interrupt_flag[instance_no]);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
#ifdef SUPPORT_TIMEOUT_RESOLUTION
	kt = ktime_set(0, info.timeout * TIMEOUT_MS_TO_NS);
	//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_9_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = wait_event_interruptible_hrtimeout /* PRQA S ALL */
		(dev->interrupt_wait_q[instance_no],
		dev->interrupt_flag[instance_no] != 0, kt);
#else
	//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_wait_event_interruptible_timeout /*PRQA S ALL*/
		(dev->interrupt_wait_q[instance_no],
		dev->interrupt_flag[instance_no] != 0,
		msecs_to_jiffies(info.timeout)); /* PRQA S 3432 */
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
		JPU_INFO_DEV(dev->device, "INSTANCE NO: %d ERESTARTSYS\n", instance_no);
		return ret;
	}
#endif
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "INST(%d) s_interrupt_flag(%d), reason(0x%08x)\n",
		instance_no, dev->interrupt_flag[instance_no],
		dev->interrupt_reason[instance_no]);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	info.intr_reason = dev->interrupt_reason[instance_no];
	dev->interrupt_flag[instance_no] = 0;
	dev->interrupt_reason[instance_no] = 0;
#ifdef JPU_IRQ_CONTROL
	// PRQA S 1021,3473,1020,3432,2996,3200 ++
	osal_spin_lock_irqsave(&dev->irq_spinlock, (uint64_t *)&flags_mp);
	// PRQA S 1021,3473,1020,3432,2996,3200 --
	if (dev->irq_trigger == 1) {
		enable_irq(dev->irq);
		dev->irq_trigger = 0;
	}
	osal_spin_unlock_irqrestore(&dev->irq_spinlock, (uint64_t *)&flags_mp);
#endif

	// PRQA S 1021,3473,1020,3432,2996,3200 ++
	osal_spin_lock_irqsave(&dev->poll_int_wait.lock, (uint64_t *)&flags_mp);
	// PRQA S 1021,3473,1020,3432,2996,3200 --
	dev->poll_int_event--;
	dev->total_release++;
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "ioctl poll event %lld total_release=%lld.\n",
		dev->poll_int_event, dev->total_release);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	osal_spin_unlock_irqrestore(&dev->poll_int_wait.lock, (uint64_t *)&flags_mp);

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&info,
			sizeof(hb_jpu_drv_intr_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
		ret = -EFAULT;
		jpu_send_diag_error_event((u16)EventIdJPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}

	(void)calc_jpu_loading(dev, instance_no);
	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_set_clock_gate(const hb_jpu_dev_t *dev, u_long arg)
{
	u32 clkgate;
	int32_t ret;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	if (dev == NULL) {
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&clkgate, (const void __user *) arg,
			sizeof(u32));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

#ifdef JPU_SUPPORT_CLOCK_CONTROL
	if (clkgate) {
		ret = hb_jpu_clk_enable(dev, dev->jpu_freq);
		if (ret != 0) {
			JPU_ERR_DEV(dev->device, "Failed to enable jpu clock.\n");
			jpu_send_diag_error_event((u16)EventIdJPUDevClkEnableErr, (u8)ERR_SEQ_0, 0, __LINE__);
		}
	} else {
		ret = hb_jpu_clk_disable(dev);
		if (ret != 0) {
			JPU_ERR_DEV(dev->device, "Failed to disable jpu clock.\n");
			jpu_send_diag_error_event((u16)EventIdJPUDevClkDisableErr, (u8)ERR_SEQ_0, 0, __LINE__);
		}
	}
#endif
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_get_instance_pool(hb_jpu_dev_t *dev, u_long arg)
{
	int32_t ret;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[+]JDI_IOCTL_GET_INSTANCE_POOL\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	ret = osal_sema_down_interruptible(&dev->jpu_sem);
	if (ret == 0) {
		if (dev->instance_pool.base != 0UL) {
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			ret = (int32_t)osal_copy_to_app((void __user *)arg,
					(void *)&dev->instance_pool, sizeof(hb_jpu_drv_buffer_t));
			if (ret != 0) {
				JPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
				jpu_send_diag_error_event((u16)EventIdJPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
				ret = -EFAULT;
			}
		} else {
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			ret = (int32_t)osal_copy_from_app((void *)&dev->instance_pool,
					(const void __user *)arg, sizeof(hb_jpu_drv_buffer_t));
			if (ret == 0) {
				//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				//coverity[misra_c_2012_rule_10_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				dev->instance_pool.base = (uint64_t)osal_malloc(
					PAGE_ALIGN(dev->instance_pool.size));/* PRQA S 3469,1840,1020,4491 */

				dev->instance_pool.phys_addr = dev->instance_pool.base;

				if (dev->instance_pool.base != 0UL) {
					/*clearing memory */
					//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					//coverity[misra_c_2012_directive_4_14_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					(void)memset((void *)dev->instance_pool.base, 0x0,
						dev->instance_pool.size);
					//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
					ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&dev->instance_pool,
							sizeof(hb_jpu_drv_buffer_t));
					if (ret == 0) {
						/* success to get memory for instance pool */
						osal_sema_up(&dev->jpu_sem);
						return ret;
					} else {
						//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
						osal_free((void *)dev->instance_pool.base);
						dev->instance_pool.base = 0UL;
						JPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
						ret = -EFAULT;
					}
				} else {
					ret = -ENOMEM;
					JPU_ERR_DEV(dev->device, "Failed to allocate memory\n");
					jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
				}
			} else {
				JPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
				jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
			}
		}
		osal_sema_up(&dev->jpu_sem);
	}

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[-]JDI_IOCTL_GET_INSTANCE_POOL: %s base: %llx, size: %d\n",
		((ret == 0) ? "OK" : "NG"), dev->instance_pool.base,
		dev->instance_pool.size);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_open_instance(struct file *filp,
				hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_drv_inst_t inst_info;
	hb_jpu_drv_instance_list_t *jil_tmp, *n;
	int32_t ret = 0;
	u32 found = 0U;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	JPU_DBG_DEV(dev->device, "[+]JDI_IOCTL_OPEN_INSTANCE, current_pid=%d, tgid=%d\n", current->pid, current->tgid);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_from_app((void *)&inst_info, (const void __user *) arg,
		sizeof(hb_jpu_drv_inst_t)) != 0UL) {
		JPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	if (inst_info.inst_idx >= MAX_NUM_JPU_INSTANCE) {
		JPU_ERR_DEV(dev->device, "Invalid instance index %d.\n", inst_info.inst_idx);
		jpu_send_diag_error_event((u16)EventIdJPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	osal_spin_lock(&dev->jpu_spinlock);
	// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 ++
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(jil_tmp, n, &dev->inst_list_head, list) {
		if (jil_tmp->inst_idx == inst_info.inst_idx) {
			found = 1U;
			break;
		}
	}
	// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 --

	if (0U == found) {
		osal_spin_unlock(&dev->jpu_spinlock);
		JPU_ERR_DEV(dev->device, "Failed to find instance index %d in list.\n", inst_info.inst_idx);
		jpu_send_diag_error_event((u16)EventIdJPUDevInstIdxErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}

	/* counting the current open instance number */
	inst_info.inst_open_count = 0;
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(jil_tmp, n, &dev->inst_list_head, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		inst_info.inst_open_count++;
	}
	osal_spin_unlock(&dev->jpu_spinlock);

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_to_app((void __user *)arg, (void *)&inst_info,
		sizeof(hb_jpu_drv_inst_t)) != 0UL) {
		JPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	osal_spin_lock(&dev->jpu_spinlock);
	// only clear the last jpu_stats, the jpu_contex has been set.
	(void)memset((void *)&dev->jpu_status[inst_info.inst_idx], 0x00,
		sizeof(dev->jpu_status[inst_info.inst_idx]));

	/* flag just for that jpu is in opened or closed */
	dev->jpu_open_ref_count++;
	dev->inst_open_flag[inst_info.inst_idx] = 1;
	osal_spin_unlock(&dev->jpu_spinlock);

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	JPU_INFO_DEV(dev->device, "[-]JDI_IOCTL_OPEN_INSTANCE inst_idx=%u, "
		"s_jpu_open_ref_count=%d, inst_open_count=%d, current_pid=%d, tgid=%d\n",
		inst_info.inst_idx,
		dev->jpu_open_ref_count,
		inst_info.inst_open_count,
		current->pid, current->tgid);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_close_instance(hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_drv_inst_t inst_info;
	hb_jpu_drv_instance_list_t *jil, *n;
	int32_t found = 0;
	int32_t ret = 0;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	JPU_DBG_DEV(dev->device, "[+]JDI_IOCTL_CLOSE_INSTANCE, current_pid=%d, tgid=%d\n", current->pid, current->tgid);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_from_app((void *)&inst_info, (const void __user *) arg,
		sizeof(hb_jpu_drv_inst_t)) != 0UL) {
		JPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	if (inst_info.inst_idx >= MAX_NUM_JPU_INSTANCE) {
		JPU_ERR_DEV(dev->device, "Invalid instance index %d.\n", inst_info.inst_idx);
		jpu_send_diag_error_event((u16)EventIdJPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	osal_spin_lock(&dev->jpu_spinlock);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(jil, n, &dev->inst_list_head, list) {  /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		if (jil->inst_idx == inst_info.inst_idx) {
			found = 1;
			break;
		}
	}

	if (0 == found) {
		osal_spin_unlock(&dev->jpu_spinlock);
		JPU_ERR_DEV(dev->device, "Failed to find instance index %d.\n", inst_info.inst_idx);
		jpu_send_diag_error_event((u16)EventIdJPUDevInstIdxErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev->poll_event[inst_info.inst_idx] = LLONG_MIN;
	osal_wake_up(&dev->poll_wait_q[inst_info.inst_idx]); /*PRQA S 3469*/
	osal_wake_up(&dev->poll_int_wait); /*PRQA S 3469*/

	/* flag just for that jpu is in opened or closed */
	dev->jpu_open_ref_count--;
	inst_info.inst_open_count = dev->jpu_open_ref_count;
	dev->inst_open_flag[inst_info.inst_idx] = 0;
	osal_spin_unlock(&dev->jpu_spinlock);

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_to_app((void __user *)arg, (void *)&inst_info,
		sizeof(hb_jpu_drv_inst_t)) != 0UL) {
		JPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	JPU_INFO_DEV(dev->device, "[-]JDI_IOCTL_CLOSE_INSTANCE inst_idx=%u, "
		"s_jpu_open_ref_count=%d, inst_open_count=%d, current_pid=%d, tgid=%d\n",
		inst_info.inst_idx, dev->jpu_open_ref_count,
		inst_info.inst_open_count,
		current->pid, current->tgid);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_get_instance_num(hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_drv_inst_t inst_info;
	int32_t ret;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[+]JDI_IOCTL_GET_INSTANCE_NUM\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&inst_info, (const void __user *) arg,
			sizeof(hb_jpu_drv_inst_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	osal_spin_lock(&dev->jpu_spinlock);
	inst_info.inst_open_count = dev->jpu_open_ref_count;
	osal_spin_unlock(&dev->jpu_spinlock);

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&inst_info,
			sizeof(hb_jpu_drv_inst_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[-]JDI_IOCTL_GET_INSTANCE_NUM inst_idx=%u, "
		"open_count=%d\n", inst_info.inst_idx,
		inst_info.inst_open_count);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_get_register_info(const hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_drv_buffer_t reg_buf;
	int32_t ret;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[+]JDI_IOCTL_GET_REGISTER_INFO\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	(void)memset((void *)&reg_buf, 0x00, sizeof(reg_buf));
	reg_buf.phys_addr = dev->jpu_mem->start;
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	reg_buf.virt_addr = (uint64_t)dev->regs_base;
	reg_buf.size = (uint32_t)resource_size(dev->jpu_mem);
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&reg_buf,
			sizeof(hb_jpu_drv_buffer_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[-]JDI_IOCTL_GET_REGISTER_INFO "
		"jpu_register.phys_addr==0x%llx, s_jpu_register.virt_addr=0x%llx,"
		"s_jpu_register.size=%d\n", reg_buf.phys_addr,
		reg_buf.virt_addr, reg_buf.size);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_alloc_instance_id(struct file *filp,
					hb_jpu_dev_t *dev, u_long arg)
{
	uint32_t inst_index;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_jpu_priv_t *priv = (hb_jpu_priv_t *)filp->private_data;
	hb_jpu_drv_inst_t inst_info;
	hb_jpu_drv_instance_list_t *jil, *jil_tmp, *n;
	int32_t ret = 0;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_from_app((void *)&inst_info, (const void __user *) arg,
		sizeof(hb_jpu_drv_inst_t)) != 0UL) {
		JPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	JPU_DBG_DEV(dev->device, "[+]JDI_IOCTL_ALLOCATE_INSTANCE_ID, current_pid=%d, tgid=%d\n", current->pid, current->tgid);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	osal_spin_lock(&dev->jpu_spinlock);
	inst_index = (uint32_t)osal_find_first_zero_bit((const uint64_t *)dev->jpu_inst_bitmap,
				MAX_NUM_JPU_INSTANCE);
	if (inst_index >= MAX_NUM_JPU_INSTANCE) {
		osal_spin_unlock(&dev->jpu_spinlock);
		JPU_ERR_DEV(dev->device, "No available id space.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOMEM;
	}
	osal_set_bit(inst_index, (volatile uint64_t *)dev->jpu_inst_bitmap);
	dev->poll_event[inst_index] = 0;
	priv->inst_index = inst_index; // it's useless
	inst_info.inst_idx = inst_index;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(jil_tmp, n, /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		&dev->inst_list_head, list) {
		if (jil_tmp->inst_idx == inst_index) {
			(void)osal_test_and_clear_bit(inst_index, (volatile uint64_t *)dev->jpu_inst_bitmap); /* PRQA S 4446 */
			osal_spin_unlock(&dev->jpu_spinlock);
			JPU_ERR_DEV(dev->device, "Failed to allocate instance id due to existing id(%d)\n",
				(int32_t)inst_index);
			jpu_send_diag_error_event((u16)EventIdJPUDevInstExistErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EINVAL;
		}
	}

	/* code review D6: Dynamic alloc memory to record instance information */
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	jil = (hb_jpu_drv_instance_list_t *)osal_kzalloc(sizeof(*jil), OSAL_KMALLOC_ATOMIC);
	if (jil == NULL) {
		(void)osal_test_and_clear_bit(inst_index, (volatile uint64_t *)dev->jpu_inst_bitmap); /* PRQA S 4446 */
		osal_spin_unlock(&dev->jpu_spinlock);
		JPU_ERR_DEV(dev->device, "Failed to allocate memory.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOMEM;
	}

	jil->inst_idx = inst_index;
	jil->filp = filp;

	osal_list_add(&jil->list, &dev->inst_list_head);
	osal_spin_unlock(&dev->jpu_spinlock);

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_to_app((void __user *)arg, (void *)&inst_info,
			sizeof(inst_info)) != 0UL) {
		ret = -EFAULT;
		osal_spin_lock(&dev->jpu_spinlock);
		osal_list_del(&jil->list);
		osal_kfree(jil);
		(void)osal_test_and_clear_bit(inst_index, (volatile uint64_t *)dev->jpu_inst_bitmap); /* PRQA S 4446 */
		osal_spin_unlock(&dev->jpu_spinlock);
		JPU_ERR_DEV(dev->device, "failed to copy to user.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return ret;
	}

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	JPU_INFO_DEV(dev->device, "[-]JDI_IOCTL_ALLOCATE_INSTANCE_ID id = %d, current_pid=%d, tgid=%d\n", inst_index, current->pid, current->tgid);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_free_instance_id(const struct file *filp,
					hb_jpu_dev_t *dev, u_long arg)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_jpu_priv_t *priv = (hb_jpu_priv_t *)filp->private_data;
	hb_jpu_drv_inst_t inst_info;
	hb_jpu_drv_instance_list_t *jil, *n;
	u32 found = 0;
	int32_t ret = 0;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[+]JDI_IOCTL_FREE_INSTANCE_ID\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (osal_copy_from_app((void *)&inst_info, (const void __user *)arg,
			sizeof(inst_info)) != 0UL) {
		JPU_ERR_DEV(dev->device, "Failed to copy from users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	if (inst_info.inst_idx >= MAX_NUM_JPU_INSTANCE) {
		JPU_ERR_DEV(dev->device, "Invalid instance id(%d).\n",
			inst_info.inst_idx);
		jpu_send_diag_error_event((u16)EventIdJPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	osal_spin_lock(&dev->jpu_spinlock);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(jil, n, &dev->inst_list_head, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		if (jil->inst_idx == inst_info.inst_idx) {
			osal_list_del(&jil->list);
			osal_kfree((void *)jil);
			found = 1;
			break;
		}
	}

	if (0U == found) {
		osal_spin_unlock(&dev->jpu_spinlock);
		JPU_ERR_DEV(dev->device, "Failed to find instance index %d.\n", inst_info.inst_idx);
		jpu_send_diag_error_event((u16)EventIdJPUDevInstIdxErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}
	osal_clear_bit(inst_info.inst_idx, (volatile uint64_t *)dev->jpu_inst_bitmap);
	priv->inst_index = MAX_JPU_INSTANCE_IDX;
	(void)memset((void *)&dev->jpu_ctx[inst_info.inst_idx], 0x00,
		sizeof(dev->jpu_ctx[inst_info.inst_idx]));
	(void)memset((void *)&dev->jpu_status[inst_info.inst_idx], 0x00,
		sizeof(dev->jpu_status[inst_info.inst_idx]));
	osal_spin_unlock(&dev->jpu_spinlock);

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[-]JDI_IOCTL_FREE_INSTANCE_ID clear id = %d\n",
		inst_info.inst_idx);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_poll_wait_instance(const struct file *filp,
					hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_drv_intr_t info;
	u32 inst_no;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_jpu_priv_t *priv = (hb_jpu_priv_t *)filp->private_data;
	int32_t ret;
	unsigned long flags_mp; /* PRQA S 5209 */

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&info, (const void __user*)arg,
		sizeof(hb_jpu_drv_intr_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "JDI_IOCTL_POLL_WAIT_INSTANCE copy from user fail.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	inst_no = info.inst_idx;
	if (inst_no >= MAX_NUM_JPU_INSTANCE) {
		JPU_ERR_DEV(dev->device, "Invalid instance id(%d).\n",
			inst_no);
		jpu_send_diag_error_event((u16)EventIdJPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	ret = osal_sema_down_interruptible(&dev->jpu_sem);
	if (ret == 0) {
		if (info.intr_reason == 0U) {
			osal_spin_lock(&dev->poll_wait_q[inst_no].lock);
			priv->inst_index = inst_no;
			priv->is_irq_poll = 0;
			osal_spin_unlock(&dev->poll_wait_q[inst_no].lock);
		} else if (info.intr_reason == (uint32_t)JPU_INST_INTERRUPT) {
			// PRQA S 1021,3473,1020,3432,2996,3200 ++
			osal_spin_lock_irqsave(&dev->poll_int_wait.lock, (uint64_t *)&flags_mp);
			// PRQA S 1021,3473,1020,3432,2996,3200 --
			priv->inst_index = inst_no;
			priv->is_irq_poll = 1;
			osal_spin_unlock_irqrestore(&dev->poll_int_wait.lock, (uint64_t *)&flags_mp);
		} else if ((info.intr_reason == (uint32_t)JPU_PIC_DONE) ||
			(info.intr_reason == (uint32_t)JPU_INST_CLOSED)) {
			osal_spin_lock(&dev->poll_wait_q[inst_no].lock);
			dev->poll_event[inst_no]++;
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			JPU_DBG_DEV(dev->device, "ioctl poll wait event dev->poll_event[%u]=%lld.\n",
				inst_no, dev->poll_event[inst_no]);
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
			osal_spin_unlock(&dev->poll_wait_q[inst_no].lock);
			osal_wake_up(&dev->poll_wait_q[inst_no]); /*PRQA S 3469*/
		} else {
			JPU_ERR_DEV(dev->device, "JDI_IOCTL_POLL_WAIT_INSTANCE invalid instance reason"
				"(%d) or index(%d).\n", info.intr_reason, inst_no);
			jpu_send_diag_error_event((u16)EventIdJPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
			ret = -EINVAL;
		}
		osal_sema_up(&dev->jpu_sem);
	}

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_set_ctx_info(hb_jpu_dev_t *dev, u_long arg)
{
	uint32_t inst_index;
	hb_jpu_ctx_info_t *info;
	int32_t ret;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[+]JDI_IOCTL_SET_CTX_INFO\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	/* code review D6: Dynamic alloc memory to copy large context */
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	info = (hb_jpu_ctx_info_t *)osal_kzalloc(sizeof(hb_jpu_ctx_info_t), OSAL_KMALLOC_ATOMIC);
	if (info == NULL) {
		JPU_ERR_DEV(dev->device, "failed to allocate ctx info.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOMEM;
	}

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret =(int32_t)osal_copy_from_app((void *)info, (const void __user *) arg,
				 sizeof(hb_jpu_ctx_info_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "JDI_IOCTL_SET_CTX_INFO copy from user fail.\n");
		osal_kfree((void *)info);
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	inst_index = (uint32_t)info->context.instance_index;
	if (inst_index >= MAX_NUM_JPU_INSTANCE) {
		JPU_ERR_DEV(dev->device, "Invalid instance index %d.\n", inst_index);
		osal_kfree((void *)info);
		jpu_send_diag_error_event((u16)EventIdJPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	osal_spin_lock(&dev->jpu_ctx_spinlock);
	dev->jpu_ctx[inst_index] = *info;
	osal_spin_unlock(&dev->jpu_ctx_spinlock);

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[-]JDI_IOCTL_SET_CTX_INFO\n");
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	osal_kfree((void *)info);

	return ret;
}

static int32_t jpu_set_status_info(hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_status_info_t info;
	uint32_t inst_index;
	int32_t ret;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&info, (const void __user *) arg,
				 sizeof(hb_jpu_status_info_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "JDI_IOCTL_SET_STATUS_INFO copy from user fail.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	inst_index = info.inst_idx;
	if (inst_index >= MAX_NUM_JPU_INSTANCE) {
		JPU_ERR_DEV(dev->device, "Invalid instance index %d.\n", inst_index);
		jpu_send_diag_error_event((u16)EventIdJPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	osal_spin_lock(&dev->jpu_ctx_spinlock);
	dev->jpu_status[inst_index] = info;
	osal_spin_unlock(&dev->jpu_ctx_spinlock);

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_do_map_ion_fd(struct file *filp, hb_jpu_dev_t *dev,
				u_long arg)
{
	int32_t ret = 0;
	hb_jpu_ion_dma_list_t *ion_dma_data, *jil_tmp, *n;
	hb_jpu_ion_fd_map_t dma_map;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&dma_map, (const void __user *) arg,
				sizeof(hb_jpu_ion_fd_map_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "JDI_IOCTL_MAP_ION_FD copy from user fail.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	/* code review D6: Dynamic alloc memory to record mapped ion fd */
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ion_dma_data = (hb_jpu_ion_dma_list_t *)osal_kzalloc(sizeof(*ion_dma_data), OSAL_KMALLOC_ATOMIC);
	if (ion_dma_data == NULL) {
		JPU_ERR_DEV(dev->device, "failed to allocate memory\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOMEM;
	}

	osal_spin_lock(&dev->jpu_spinlock);
	// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 ++
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(jil_tmp, n, &dev->dma_data_list_head, list) {
		if ((jil_tmp->fd == dma_map.fd) &&
			(jil_tmp->filp == filp)) {
			osal_kfree((void *)ion_dma_data);
			JPU_ERR_DEV(dev->device, "Failed to map ion handle due to same fd(%d), "
				"same filp(0x%p) and same tgid(%d).\n",
				dma_map.fd, filp, current->tgid);
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			osal_list_for_each_entry_safe(jil_tmp, n, &dev->dma_data_list_head, list) {
				JPU_ERR_DEV(dev->device, "Existing List items fd(%d), "
					"filp(%p).\n", jil_tmp->fd, jil_tmp->filp);
			}
			osal_spin_unlock(&dev->jpu_spinlock);
			jpu_send_diag_error_event((u16)EventIdJPUDevDmaMapExistErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EINVAL;
		}
	}
	// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 --
	osal_spin_unlock(&dev->jpu_spinlock);

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (ion_iommu_map_ion_fd(dev->device,
		&ion_dma_data->ion_dma_data, dev->jpu_ion_client, dma_map.fd, IOMMU_READ | IOMMU_WRITE) < 0) {
		osal_kfree((void *)ion_dma_data);
		JPU_ERR_DEV(dev->device, "%s: Failed to map ion handle.\n", __func__);
		jpu_send_diag_error_event((u16)EventIdJPUDevDmaMapExistErr, (u8)ERR_SEQ_1, 0, __LINE__);
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
		JPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	osal_spin_lock(&dev->jpu_spinlock);
	osal_list_add(&ion_dma_data->list, &dev->dma_data_list_head);
	osal_spin_unlock(&dev->jpu_spinlock);

	return ret;
}

static int32_t jpu_map_ion_fd(struct file *filp,
					hb_jpu_dev_t *dev, u_long arg)
{
	int32_t ret;

	ret = osal_sema_down_interruptible(&dev->jpu_sem);
	if (ret == 0) {
		ret = jpu_do_map_ion_fd(filp, dev, arg);
		osal_sema_up(&dev->jpu_sem);
	}

	return ret;
}

static int32_t jpu_unmap_ion_fd(const struct file *filp,
				hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_ion_fd_map_t dma_map;
	hb_jpu_ion_dma_list_t *jil_tmp, *n;
	int32_t found = 0;
	int32_t ret;

	ret = osal_sema_down_interruptible(&dev->jpu_sem);
	if (ret == 0) {
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		ret = (int32_t)osal_copy_from_app((void *)&dma_map, (const void __user *) arg,
					sizeof(dma_map));
		if (ret != 0) {
			JPU_ERR_DEV(dev->device, "JDI_IOCTL_UNMAP_ION_FD copy from user fail.\n");
			osal_sema_up(&dev->jpu_sem);
			jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EFAULT;
		}
		osal_spin_lock(&dev->jpu_spinlock);
		// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 ++
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		osal_list_for_each_entry_safe(jil_tmp, n, &dev->dma_data_list_head, list) {
			if ((jil_tmp->fd == dma_map.fd) &&
				(jil_tmp->filp == filp)) {
				osal_list_del(&jil_tmp->list);
				found = 1;
				break;
			}
		}
		// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 --
		osal_spin_unlock(&dev->jpu_spinlock);

		if (0 == found) {
			ret = -EINVAL;
			JPU_ERR_DEV(dev->device, "failed to find ion fd.\n");
			jpu_send_diag_error_event((u16)EventIdJPUDevDmaMapFindErr, (u8)ERR_SEQ_0, 0, __LINE__);
		} else {
			ion_iommu_unmap_ion_fd(dev->device,
				&jil_tmp->ion_dma_data);
			osal_kfree((void *)jil_tmp);
		}
		osal_sema_up(&dev->jpu_sem);
	}

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_do_map_ion_phy(struct file *filp,
				hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_ion_phys_map_t dma_map;
	hb_jpu_phy_dma_list_t *phy_dma_data, *jil_tmp, *n;
	int32_t ret = 0;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_from_app((void *)&dma_map, (const void __user *) arg,
				sizeof(hb_jpu_ion_phys_map_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "JDI_IOCTL_MAP_ION_PHYS copy from user fail.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	osal_spin_lock(&dev->jpu_spinlock);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	osal_list_for_each_entry_safe(jil_tmp, n, /* PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 */
		&dev->phy_data_list_head, list) {
		if ((jil_tmp->phys_addr == dma_map.phys_addr) &&
			(jil_tmp->size == dma_map.size) &&
			(jil_tmp->filp == filp)) {
			osal_spin_unlock(&dev->jpu_spinlock);
			JPU_ERR_DEV(dev->device, "Failed to map ion phy due to same phys(0x%llx) "
				"same size(%d) and same filp(0x%p).\n",
				dma_map.phys_addr, jil_tmp->size, filp);
			jpu_send_diag_error_event((u16)EventIdJPUDevDmaMapExistErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EINVAL;
		}
	}
	osal_spin_unlock(&dev->jpu_spinlock);

	if (ion_check_in_heap_carveout(dma_map.phys_addr, dma_map.size) < 0) {
		JPU_ERR_DEV(dev->device, "Invalid ion physical address(addr=0x%llx, size=%u), not in ion range.\n",
			dma_map.phys_addr, dma_map.size);
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	if (ion_iommu_map_ion_phys(dev->device,
		dma_map.phys_addr, dma_map.size, &dma_map.iova, IOMMU_READ | IOMMU_WRITE) < 0) {
		JPU_ERR_DEV(dev->device, "Failed to map ion phys.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevIonMapErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}

	/* code review D6: Dynamic alloc memory to record mapped physical address */
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	phy_dma_data = (hb_jpu_phy_dma_list_t *)osal_kzalloc(sizeof(*phy_dma_data), OSAL_KMALLOC_ATOMIC);
	if (phy_dma_data == NULL) {
		ion_iommu_unmap_ion_phys(dev->device,
			dma_map.iova, dma_map.size);
		JPU_ERR_DEV(dev->device, "failed to allocate memory\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
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
		JPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevSetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	osal_spin_lock(&dev->jpu_spinlock);
	osal_list_add(&phy_dma_data->list, &dev->phy_data_list_head);
	osal_spin_unlock(&dev->jpu_spinlock);

	return ret;
}

static int32_t jpu_map_ion_phys(struct file *filp, hb_jpu_dev_t *dev, u_long arg)
{
	int32_t ret;

	ret = osal_sema_down_interruptible(&dev->jpu_sem);
	if (ret == 0) {
		ret = jpu_do_map_ion_phy(filp, dev, arg);
		osal_sema_up(&dev->jpu_sem);
	}

	return ret;
}

static int32_t jpu_unmap_ion_phys(const struct file *filp, hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_ion_phys_map_t dma_map;
	hb_jpu_phy_dma_list_t *jil_tmp, *n;
	int32_t found = 0;
	int32_t ret;

	ret = osal_sema_down_interruptible(&dev->jpu_sem);
	if (ret == 0) {
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		ret = (int32_t)osal_copy_from_app((void *)&dma_map, (const void __user *) arg,
				sizeof(hb_jpu_ion_phys_map_t));
		if (ret != 0) {
			JPU_ERR_DEV(dev->device, "JDI_IOCTL_UNMAP_ION_PHYS copy from user fail.\n");
			osal_sema_up(&dev->jpu_sem);
			jpu_send_diag_error_event((u16)EventIdJPUDevGetUserErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return -EFAULT;
		}

		osal_spin_lock(&dev->jpu_spinlock);
		// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 ++
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		osal_list_for_each_entry_safe(jil_tmp, n, &dev->phy_data_list_head, list) {
			if ((jil_tmp->iova == dma_map.iova) &&
				(jil_tmp->size == dma_map.size) &&
				(jil_tmp->filp == filp)) {
				osal_list_del(&jil_tmp->list);
				found = 1;
				break;
			}
		}
		// PRQA S 3673,0497,2810,1020,3432,0306,1021,3418 --
		osal_spin_unlock(&dev->jpu_spinlock);

		if (0 == found) {
			ret = -EINVAL;
			JPU_ERR_DEV(dev->device, "failed to find ion phys.\n");
			jpu_send_diag_error_event((u16)EventIdJPUDevDmaMapFindErr, (u8)ERR_SEQ_0, 0, __LINE__);
		} else {
			ion_iommu_unmap_ion_phys(dev->device, dma_map.iova, dma_map.size);
			osal_kfree((void *)jil_tmp);
		}
		osal_sema_up(&dev->jpu_sem);
	}

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_get_version_info(hb_jpu_dev_t *dev, u_long arg)
{
	hb_jpu_version_info_data_t version_data;
	int32_t ret;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[+]JDI_IOCTL_GET_VERSION_INFO\n");

	(void)memset((void *)&version_data, 0x00, sizeof(version_data));
	version_data.major = JPU_DRIVER_API_VERSION_MAJOR;
	version_data.minor = JPU_DRIVER_API_VERSION_MINOR;
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	ret = (int32_t)osal_copy_to_app((void __user *)arg, (void *)&version_data,
			sizeof(hb_jpu_version_info_data_t));
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "Failed to copy to users.\n");
	}

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "[-]JDI_IOCTL_GET_VERSION_INFO "
		"version_data.major==%d, version_data.minor=%d,",
		version_data.major, version_data.minor);

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static long jpu_ioctl(struct file *filp, u_int cmd, u_long arg) /* PRQA S 5209 */
{
	int32_t ret;
	hb_jpu_dev_t *dev;
	hb_jpu_priv_t *priv;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_jpu_priv_t *)filp->private_data;
	dev = priv->jpu_dev;
	if (dev == NULL) {
		JPU_ERR("failed to get jpu dev data\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevIoctlCmdErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_alloc_physical_memory(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_FREE_PHYSICAL_MEMORY: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_free_physical_memory(dev, arg);
		break;
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_get_reserved_memory_info(dev, arg);
		break;
#endif
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_WAIT_INTERRUPT: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_wait_interrupt(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_SET_CLOCK_GATE: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_set_clock_gate(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_GET_INSTANCE_POOL: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_get_instance_pool(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_OPEN_INSTANCE: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_open_instance(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_CLOSE_INSTANCE: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_close_instance(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_GET_INSTANCE_NUM: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_get_instance_num(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_RESET: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = hb_jpu_hw_reset();
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_GET_REGISTER_INFO: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_get_register_info(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_ALLOCATE_INSTANCE_ID: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_alloc_instance_id(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_FREE_INSTANCE_ID: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_free_instance_id(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_POLL_WAIT_INSTANCE: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_poll_wait_instance(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_SET_CTX_INFO: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_set_ctx_info(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_SET_STATUS_INFO: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_set_status_info(dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_MAP_ION_FD: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_map_ion_fd(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_UNMAP_ION_FD: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_unmap_ion_fd(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_MAP_ION_PHYS: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_map_ion_phys(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_UNMAP_ION_PHYS: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_unmap_ion_phys(filp, dev, arg);
		break;
#ifdef USE_MUTEX_IN_KERNEL_SPACE
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_JDI_LOCK: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_jdi_lock(filp, dev, arg);
		break;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_JDI_UNLOCK: /* PRQA S 4513,4599,4542,4543,1841,0499 */
		ret = jpu_jdi_unlock(filp, dev, arg);
		break;
#endif
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	case JDI_IOCTL_GET_VERSION_INFO:
		ret = jpu_get_version_info(dev, arg);
		break;
	default:
		{
			JPU_ERR_DEV(dev->device, "No such IOCTL, cmd is %d\n", cmd);
			jpu_send_diag_error_event((u16)EventIdJPUDevIoctlCmdErr, (u8)ERR_SEQ_1, 0, __LINE__);
			ret = -EINVAL;
		}
		break;
	}

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static ssize_t jpu_read(struct file *filp, char __user * buf, size_t len, /* PRQA S 3206 */
			loff_t * ppos) /* PRQA S 3206 */
{
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return -EINVAL;
}

//coverity[HIS_metric_violation:SUPPRESS]
static ssize_t jpu_write(struct file *filp, const char __user * buf, /* PRQA S 3673,3206 */
			 size_t len, loff_t * ppos) /* PRQA S 3206 */
{
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	if (buf == NULL) {
		JPU_ERR("jpu_write buf = NULL error \n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EFAULT;
	}
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	return -EINVAL;
}

/* code review E1: No need to return value */
//coverity[HIS_metric_violation:SUPPRESS]
static void jpu_release_internal_res(hb_jpu_dev_t *dev,
				const struct file *filp)
{
	hb_jpu_drv_instance_pool_t *jip;
	uint32_t open_count, i;

	osal_spin_lock(&dev->jpu_spinlock);	//check this place
	/* found and free the not closed instance by user applications */
	(void)jpu_free_instances(filp);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(dev->device, "open_count: %d\n", dev->open_count);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	dev->open_count--;
	open_count = dev->open_count;
	osal_spin_unlock(&dev->jpu_spinlock);

	/* found and free the not handled buffer by user applications */
	(void)jpu_free_buffers(filp);

	if (open_count == 0U) {
		if (dev->instance_pool.base != 0UL) {
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
			JPU_DBG_DEV(dev->device, "free instance pool(address=0x%llx), current_pid=%d, tgid=%d\n",
				dev->instance_pool.base, current->pid, current->tgid);
			// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
			//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			jip = (hb_jpu_drv_instance_pool_t *)dev->instance_pool.base;
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			osal_free((void *)dev->instance_pool.base);
			dev->instance_pool.base = 0;
		}
		jpu_force_free_lock(dev);
		for (i = 0; i < MAX_NUM_JPU_INSTANCE; i++) {
			(void)osal_test_and_clear_bit(i, (volatile uint64_t *)dev->jpu_inst_bitmap);
			dev->inst_open_flag[i] = 0;
		}

		dev->total_poll = 0;
		dev->total_release = 0;
	}
	JPU_INFO_DEV(dev->device, "finish %s, current_pid=%d, tgid=%d\n", __func__, current->pid, current->tgid);
}

static int32_t jpu_release(struct inode *inode, struct file *filp) /* PRQA S 3673 */
{
	int32_t ret;
	uint64_t timeout;
	uint32_t open_count;

	hb_jpu_dev_t *dev;
	hb_jpu_priv_t *priv;
	JPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	dev = osal_list_entry(inode->i_cdev, hb_jpu_dev_t, cdev); /* PRQA S 3432,2810,0306,0662,1021,0497 */
	if (dev == NULL) {
		JPU_ERR("failed to get jpu dev data\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	priv = (hb_jpu_priv_t *)filp->private_data;

	timeout = jiffies + JPU_GET_SEM_TIMEOUT;
	ret = osal_sema_down_timeout(&dev->jpu_sem, (uint32_t)timeout);
	if (ret == 0) {
		jpu_release_internal_res(dev, filp);
		osal_kfree((void *)priv);
		filp->private_data = NULL;
		osal_sema_up(&dev->jpu_sem);
	} else {
		JPU_ERR_DEV(dev->device, "Fail to wait semaphore(ret=%d).\n", ret);
		jpu_send_diag_error_event((u16)EventIdJPUDevParamCheckErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}

#ifdef JPU_ENABLE_CLOCK
	if (hb_jpu_clk_disable(dev) != 0) {
		JPU_ERR_DEV(dev->device, "failed to disable clock.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevClkDisableErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}
#endif

	open_count = dev->open_count;
	if (open_count == 0U)
		hb_jpu_pm_ctrl(dev, 0, 0);

	JPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	return ret;
}

static int32_t jpu_fasync(int32_t fd, struct file *filp, int32_t mode)
{
	int32_t ret;
	hb_jpu_dev_t *dev;
	hb_jpu_priv_t *priv;
	JPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	priv = (hb_jpu_priv_t *)filp->private_data;
	dev = priv->jpu_dev;
	if (dev == NULL) {
		JPU_ERR("failed to get jpu dev data\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	ret = fasync_helper(fd, filp, mode, &dev->async_queue);
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_map_to_register(const struct file *filp, struct vm_area_struct *vm)
{
	uint64_t pfn;
	hb_jpu_dev_t *dev;
	hb_jpu_priv_t *priv;
	int32_t ret;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	if ((filp == NULL) || (vm == NULL)) {
		JPU_ERR("failed to map register, filp or vm is null.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_jpu_priv_t *)filp->private_data;
	dev = priv->jpu_dev;

	if (dev == NULL) {
		JPU_ERR("failed to map register, dev is null.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vm->vm_flags |= VM_IO | VM_RESERVED; /* PRQA S 4542,1841 */
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot); /* PRQA S 3469 */
	pfn = dev->jpu_mem->start >> PAGE_SHIFT;
	ret = (remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end - vm->vm_start,
			      vm->vm_page_prot) != 0)? -EAGAIN : 0;
	if (ret != 0) {
		jpu_send_diag_error_event((u16)EventIdJPUDevMmapErr, (u8)ERR_SEQ_0, 0, __LINE__);
		JPU_ERR_DEV(dev->device, "failed to map physical memory(ret=%d)\n", ret);
	}

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_map_to_physical_memory(const struct file *filp,
				      struct vm_area_struct *vm)
{
	hb_jpu_dev_t *dev;
	hb_jpu_priv_t *priv;
	int32_t ret;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	if ((filp == NULL) || (vm == NULL)) {
		JPU_ERR("failed to map register, filp or vm is null.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_jpu_priv_t *)filp->private_data;
	dev = priv->jpu_dev;

	if (dev == NULL) {
		JPU_ERR("failed to map register, dev is null.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vm->vm_flags |= VM_IO | VM_RESERVED; /* PRQA S 4542,1841 */
	vm->vm_page_prot = pgprot_writecombine(vm->vm_page_prot); /* PRQA S 3469 */
	ret = (remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff,
			vm->vm_end - vm->vm_start,
			vm->vm_page_prot) != 0)? -EAGAIN : 0;
	if (ret != 0) {
		JPU_ERR_DEV(dev->device, "failed to map physical memory(ret=%d)\n", ret);
		jpu_send_diag_error_event((u16)EventIdJPUDevMmapErr, (u8)ERR_SEQ_0, 0, __LINE__);
	}

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */
	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_map_to_instance_pool_memory(const struct file *filp,
					   struct vm_area_struct *vm)
{
	int32_t ret;
	int64_t length;
	uint64_t start;
	char *vmalloc_area_ptr;
	uint64_t pfn;
	uint64_t pagesize = PAGE_SIZE;
	hb_jpu_dev_t *dev;
	hb_jpu_priv_t *priv;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_ENTER(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	if ((filp == NULL) || (vm == NULL)) {
		JPU_ERR("failed to map instances, filp or vm is null.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_jpu_priv_t *)filp->private_data;
	dev = priv->jpu_dev;

	if (dev == NULL) {
		JPU_ERR("failed to map  instances, dev is null.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -EINVAL;
	}

	length = (int64_t)vm->vm_end - (int64_t)vm->vm_start;
	start = vm->vm_start;
	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vmalloc_area_ptr = (char *)dev->instance_pool.base; /* PRQA S 0306 */
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	vm->vm_flags |= VM_RESERVED; /* PRQA S 4542,1841 */

	/* loop over all pages, map it page individually */
	while (length > 0) {
		pfn = vmalloc_to_pfn((void *)vmalloc_area_ptr);
		ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE,
			PAGE_SHARED);
		if (ret < 0) {
			JPU_ERR_DEV(dev->device, "failed to map instance pool(ret=%d)\n", ret);
			jpu_send_diag_error_event((u16)EventIdJPUDevMmapErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return ret;
		}
		start += PAGE_SIZE;
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		vmalloc_area_ptr += PAGE_SIZE; /* PRQA S 0488 */
		length -= (int64_t)pagesize;
	}

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DEBUG_LEAVE(); /* PRQA S 3432,4543,4403,4558,4542,1861,3344 */

	return 0;
}

/*!
* @brief memory map interface for jpu file operation
* @return  0 on success or negative error code on error
*/
static int32_t jpu_mmap(struct file *filp, struct vm_area_struct *vm) /* PRQA S 3673 */
{
	hb_jpu_dev_t *dev;
	hb_jpu_priv_t *priv;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_jpu_priv_t *)filp->private_data;
	dev = priv->jpu_dev;

	if (vm->vm_pgoff == 0UL)
		return jpu_map_to_instance_pool_memory(filp, vm);

	if (vm->vm_pgoff == (dev->jpu_mem->start >> PAGE_SHIFT))
		return jpu_map_to_register(filp, vm);

	return jpu_map_to_physical_memory(filp, vm);
}

static unsigned int jpu_poll(struct file *filp, struct poll_table_struct *wait) /* PRQA S 5209 */
{
	hb_jpu_dev_t *dev;
	hb_jpu_priv_t *priv;
	uint32_t mask = 0;
	int64_t count;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	priv = (hb_jpu_priv_t *)filp->private_data;
	dev = priv->jpu_dev;
	if (priv->inst_index >= MAX_NUM_JPU_INSTANCE) {
		JPU_ERR_DEV(dev->device, "Invalid instance index %d\n", priv->inst_index);
		jpu_send_diag_error_event((u16)EventIdJPUDevInstIdxErr, (u8)ERR_SEQ_0, 0, __LINE__);
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
			mask = 0;
		}
		osal_spin_unlock(&dev->poll_wait_q[priv->inst_index].lock);
	} else {
		poll_wait(filp, &dev->poll_int_wait, wait);
		count = dev->poll_int_event;
		if (count > 0) {
			//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			mask = (uint32_t)EPOLLIN | (uint32_t)EPOLLET; /* PRQA S 4399,0499 */
		}
	}
	return mask;
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
static struct file_operations jpu_fops = {
	.owner = THIS_MODULE,
	.open = jpu_open,
	.read = jpu_read,
	.write = jpu_write,
	.unlocked_ioctl = jpu_ioctl,
	.release = jpu_release,
	.fasync = jpu_fasync,
	.mmap = jpu_mmap,
	.poll = jpu_poll,
};

// PRQA S 0662 ++
//////////////// jenc
static const char *get_codec(const hb_jpu_ctx_info_t *jpu_ctx)
{
	const char * codec_str;
	switch (jpu_ctx->context.codec_id) {
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
			codec_str = "---";
			break;
	}
	return codec_str;
}

static int32_t rcparam_show(struct seq_file *s, const hb_jpu_ctx_info_t *jpu_ctx)
{
	int32_t ret = 0;
	const mc_rate_control_params_t *rc =
		&(jpu_ctx->context.video_enc_params.rc_params);
	if (rc->mode == MC_AV_RC_MODE_MJPEGFIXQP) {
		seq_printf(s, "%7d %9s %10d %14d\n",
			jpu_ctx->context.instance_index,
			"mjpgfixqp",
			rc->mjpeg_fixqp_params.frame_rate,
			rc->mjpeg_fixqp_params.quality_factor);
	} else {
		seq_printf(s, "%7d %9s %10d %14d\n",
			jpu_ctx->context.instance_index,
			"noratecontrol",
			0,
			jpu_ctx->context.video_enc_params.jpeg_enc_config.quality_factor);
	}
	seq_printf(s, "\n");

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_enc_show_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_jpu_dev_t *dev = (hb_jpu_dev_t *)s->private;
	if (dev == NULL) {
		JPU_ERR("Invalid null dev\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_NUM_JPU_INSTANCE; i++) {
		if ((dev->jpu_ctx[i].valid != 0) && (dev->jpu_ctx[i].context.encoder)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "----encode param----\n");
				seq_printf(s, "%7s %7s %5s %6s %7s %10s %15s %11s "
					"%10s %6s %6s\n", "enc_idx", "enc_id", "width", "height",
					"pix_fmt", "fbuf_count", "extern_buf_flag", "bsbuf_count",
					"bsbuf_size", "mirror", "rotate");
			}
			seq_printf(s, "%7d %7s %5d %6d %7d %10d %15d %11d %10d %6d %6d\n",
				dev->jpu_ctx[i].context.instance_index,
				get_codec(&dev->jpu_ctx[i]),
				dev->jpu_ctx[i].context.video_enc_params.width,
				dev->jpu_ctx[i].context.video_enc_params.height,
				dev->jpu_ctx[i].context.video_enc_params.pix_fmt,
				dev->jpu_ctx[i].context.video_enc_params.frame_buf_count,
				dev->jpu_ctx[i].context.video_enc_params.external_frame_buf,
				dev->jpu_ctx[i].context.video_enc_params.bitstream_buf_count,
				dev->jpu_ctx[i].context.video_enc_params.bitstream_buf_size,
				dev->jpu_ctx[i].context.video_enc_params.mir_direction,
				dev->jpu_ctx[i].context.video_enc_params.rot_degree);
		}
	}

	output = 0;
	for (i = 0; i < MAX_NUM_JPU_INSTANCE; i++) {
		if ((dev->jpu_ctx[i].valid != 0) && (dev->jpu_ctx[i].context.encoder)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----encode rc param----\n");
				seq_printf(s, "%7s %9s %10s %14s\n",
					"enc_idx", "rc_mode", "frame_rate", "quality_factor");
			}
			(void)rcparam_show(s, &(dev->jpu_ctx[i]));
		}
	}

	return 0;
}

static int32_t jpu_enc_show_status(struct seq_file *s)
{
	uint32_t i;
	int32_t output;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_jpu_dev_t *dev = (hb_jpu_dev_t *)s->private;
	if (dev == NULL) {
		JPU_ERR("Invalid null dev\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	output = 0;
	for (i = 0; i < MAX_NUM_JPU_INSTANCE; i++) {
		if ((dev->jpu_ctx[i].valid != 0) && (dev->jpu_ctx[i].context.encoder)) {
			mc_inter_status_t *status =	&(dev->jpu_status[i].status);
			int32_t fps = dev->jpu_status[i].fps;
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
				dev->jpu_ctx[i].context.instance_index,
				get_codec(&dev->jpu_ctx[i]),
				status->cur_input_buf_cnt,
				status->cur_output_buf_cnt,
				status->left_recv_frame,
				status->left_enc_frame,
				status->total_input_buf_cnt,
				status->total_output_buf_cnt,
				fps);
		}
	}

	return 0;
}

static int32_t jpu_jenc_show(struct seq_file *s, void *unused) /* PRQA S 3206 */
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_jpu_dev_t *dev = (hb_jpu_dev_t *)s->private;
	if (dev == NULL) {
		JPU_ERR("Invalid null dev\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	(void)jpu_enc_show_param(s);

	osal_spin_lock(&dev->jpu_ctx_spinlock);
	(void)jpu_enc_show_status(s);
	osal_spin_unlock(&dev->jpu_ctx_spinlock);
	return 0;
}

static int32_t jpu_jenc_open(struct inode *inodes, struct file *files) /* PRQA S 3673 */
{
	return single_open(files, jpu_jenc_show, inodes->i_private);
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
static const struct file_operations jpu_jenc_fops = {
	.open = jpu_jenc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//////////////// jdec
//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_dec_show_param(struct seq_file *s)
{
	uint32_t i;
	int32_t output = 0;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_jpu_dev_t *dev = (hb_jpu_dev_t *)s->private;

	if (dev == NULL) {
		JPU_ERR("Invalid null dev\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < MAX_NUM_JPU_INSTANCE; i++) {
		if ((dev->jpu_ctx[i].valid != 0) && (!dev->jpu_ctx[i].context.encoder)) {
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----decode param----\n");
				seq_printf(s, "%7s %7s %9s %7s %18s %19s %15s %6s %6s\n",
					"dec_idx", "dec_id", "feed_mode", "pix_fmt",
					"bitstream_buf_size", "bitstream_buf_count",
					"frame_buf_count", "mirror", "rotate");
			}
			seq_printf(s, "%7d %7s %9d %7d %18d %19d %15d ",
				dev->jpu_ctx[i].context.instance_index,
				get_codec(&dev->jpu_ctx[i]),
				dev->jpu_ctx[i].context.video_dec_params.feed_mode,
				dev->jpu_ctx[i].context.video_dec_params.pix_fmt,
				dev->jpu_ctx[i].context.video_dec_params.bitstream_buf_size,
				dev->jpu_ctx[i].context.video_dec_params.bitstream_buf_count,
				dev->jpu_ctx[i].context.video_dec_params.frame_buf_count);
			if (dev->jpu_ctx[i].context.codec_id == MEDIA_CODEC_ID_MJPEG) {
				mc_mjpeg_dec_config_t *mjpeg =
					&dev->jpu_ctx[i].context.video_dec_params.mjpeg_dec_config;
				seq_printf(s, "%6d %6d\n",
					mjpeg->mir_direction, mjpeg->rot_degree);
			} else if (dev->jpu_ctx[i].context.codec_id ==
												MEDIA_CODEC_ID_JPEG) {
				mc_jpeg_dec_config_t *jpeg =
					&dev->jpu_ctx[i].context.video_dec_params.jpeg_dec_config;
				seq_printf(s, "%6d %6d\n",
					jpeg->mir_direction, jpeg->rot_degree);
			} else {
				seq_printf(s, "\n");
			}
		}
	}

	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_dec_show_status(struct seq_file *s)
{
	uint32_t i;
	int32_t output;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_jpu_dev_t *dev = (hb_jpu_dev_t *)s->private;

	if (dev == NULL) {
		JPU_ERR("Invalid null dev\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	output = 0;
	for (i = 0; i < MAX_NUM_JPU_INSTANCE; i++) {
		if ((dev->jpu_ctx[i].valid != 0) && (!dev->jpu_ctx[i].context.encoder)) {
			mc_mjpeg_jpeg_output_frame_info_t *frameinfo
								= &(dev->jpu_status[i].frame_info);
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----decode frameinfo----\n");
				seq_printf(s, "%7s %7s %13s %14s\n", "dec_idx", "dec_id",
					"display_width", "display_height");
			}
			seq_printf(s, "%7d %7s %13d %14d\n",
				dev->jpu_ctx[i].context.instance_index,
				get_codec(&dev->jpu_ctx[i]),
				frameinfo->display_width,
				frameinfo->display_height);
		}
	}

	output = 0;
	for (i = 0; i < MAX_NUM_JPU_INSTANCE; i++) {
		if ((dev->jpu_ctx[i].valid != 0) && (!dev->jpu_ctx[i].context.encoder)) {
			mc_inter_status_t *status = &(dev->jpu_status[i].status);
			int32_t fps = dev->jpu_status[i].fps;
			if (output == 0) {
				output = 1;
				seq_printf(s, "\n");
				seq_printf(s, "----decode status----\n");
				seq_printf(s, "%7s %7s %17s %18s %19s %20s %7s\n", "dec_idx",
					"dec_id", "cur_input_buf_cnt", "cur_output_buf_cnt",
					"total_input_buf_cnt", "total_output_buf_cnt", "fps");
			}
			seq_printf(s, "%7d %7s %17d %18d %19d %20d %7d\n",
				dev->jpu_ctx[i].context.instance_index,
				get_codec(&dev->jpu_ctx[i]),
				status->cur_input_buf_cnt,
				status->cur_output_buf_cnt,
				status->total_input_buf_cnt,
				status->total_output_buf_cnt,
				fps);
		}
	}

	return 0;
}

static int32_t jpu_jdec_show(struct seq_file *s, void *unused) /* PRQA S 3206 */
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_jpu_dev_t *dev = (hb_jpu_dev_t *)s->private;

	if (dev == NULL) {
		JPU_ERR("Invalid null dev\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	(void)jpu_dec_show_param(s);

	osal_spin_lock(&dev->jpu_ctx_spinlock);
	(void)jpu_dec_show_status(s);
	osal_spin_unlock(&dev->jpu_ctx_spinlock);
	return 0;
}
// PRQA S 0662 --

static int32_t jpu_jdec_open(struct inode *inodes, struct file *files) /* PRQA S 3673 */
{
	return single_open(files, jpu_jdec_show, inodes->i_private);
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
static const struct file_operations jpu_jdec_fops = {
	.open = jpu_jdec_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int32_t jpu_loading_show(struct seq_file *s, void *unused) /* PRQA S 3206 */
{
	osal_time_t ts;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	hb_jpu_dev_t *dev = (hb_jpu_dev_t *)s->private;

	if (dev == NULL) {
		JPU_ERR("Invalid null dev\n");
		return -EINVAL;
	}

	osal_time_get_real(&ts);
	osal_spin_lock(&dev->jpu_ctx_spinlock);
	if ((uint64_t)ts.tv_sec > (uint64_t)dev->jpu_loading_ts.tv_sec + dev->jpu_loading_setting) {
		seq_printf(s, "0\n");
	} else {
		seq_printf(s, "%lld.%01lld\n", dev->jpu_loading/10UL, dev->jpu_loading%10UL);
	}
	osal_spin_unlock(&dev->jpu_ctx_spinlock);
	return 0;
}

static int32_t jpu_loading_open(struct inode *inodes, struct file *files) /* PRQA S 3673 */
{
	return single_open(files, jpu_loading_show, inodes->i_private);
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
static const struct file_operations jpu_loading_fops = {
	.open = jpu_loading_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* code review E1: No need to return value */
static void jpu_destroy_resource(hb_jpu_dev_t *dev)
{
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
	if (s_video_memory.base != 0UL) {
		jmem_exit(&s_jmem);
	}
#endif
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	ai_mode = hobot_fun_ai_mode_available();
	if (ai_mode) {
		if (dev->resmem.ion_vaddr != 0U) {
			ion_iommu_unmap_ion_phys(dev->device,
				dev->resmem.ion_vaddr, dev->resmem.size);
		}
	}
#endif
	if (!IS_ERR_OR_NULL((const void *)dev->jpu_ion_client)) {
		ion_client_destroy(dev->jpu_ion_client);
	}
	if (!IS_ERR_OR_NULL((const void *)dev->jpu_dev)) {
		device_destroy(dev->jpu_class, dev->jpu_dev_num);
	}
	if (dev->cdev_added != 0U) {
		cdev_del(&dev->cdev);
	}
	if (dev->cdev_allocted != 0U) {
		unregister_chrdev_region(dev->jpu_dev_num, 1);
	}
	if (!IS_ERR_OR_NULL((const void *)dev->jpu_class)) {
		class_destroy(dev->jpu_class);
	}
}

#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
static int32_t __init jpu_init_reserved_mem(struct platform_device *pdev,
						hb_jpu_dev_t *dev)
{
	int32_t err = 0;

	if (s_jmem.base_addr == 0) {
		s_video_memory.size = JPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE;
		s_video_memory.phys_addr = JPU_DRAM_PHYSICAL_BASE;
		s_video_memory.base = (uint64_t)__va(s_video_memory.phys_addr);
		if (!s_video_memory.base) {
			JPU_ERR_DEV(&pdev->dev,
				"fail to remap video memory physical phys_addr=0x%lx,"
				"base==0x%lx, size=%u\n",
				s_video_memory.phys_addr, s_video_memory.base,
				s_video_memory.size);
			err = -ENOMEM;
			jpu_destroy_resource(dev);
			jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return err;
		}

		if (jmem_init(&s_jmem, s_video_memory.phys_addr,
			s_video_memory.size) < 0) {
			err = -ENOMEM;
			JPU_ERR_DEV(&pdev->dev, "fail to init jmem system\n");
			jpu_destroy_resource(dev);
			jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
			return err;
		}
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		JPU_DBG_DEV(&pdev->dev,
			"success to probe jpu device with reserved video memory"
			"phys_addr==0x%lx, base = =0x%lx\n",
			s_video_memory.phys_addr, s_video_memory.base);
		// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	}
	return err;
}
#endif

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t __init jpu_init_system_mem(struct platform_device *pdev,
					hb_jpu_dev_t *dev)
{
	struct resource *res;
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	uint32_t cmd_val = JPU_RES;
	hb_jpu_drv_resmem_t tmp_res;
	int32_t ret;
#endif

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		JPU_ERR_DEV(&pdev->dev, "failed to get memory resource\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOENT;
	}
	dev->jpu_mem = devm_request_mem_region(&pdev->dev, res->start, resource_size(res), /* PRQA S 3469 */
					  pdev->name);
	if (dev->jpu_mem == NULL) {
		JPU_ERR_DEV(&pdev->dev, "failed to get memory region\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -ENOENT;
	}
	dev->regs_base = devm_ioremap(&pdev->dev, dev->jpu_mem->start,
					 (size_t)resource_size(dev->jpu_mem));
	if (dev->regs_base == NULL) {
		JPU_ERR_DEV(&pdev->dev, "failed to ioremap address region\n");
		jpu_destroy_resource(dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_2, 0, __LINE__);
		return -ENOENT;
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(&pdev->dev,
		"jpu IO memory resource: physical base addr = 0x%llx,"
		"virtual base addr = %p\n", dev->jpu_mem->start,
		dev->regs_base);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	//alloc ion mem
	ai_mode = hobot_fun_ai_mode_available();
	if (ai_mode) {
		ret = ion_get_heap_range(&tmp_res.ion_phys, &tmp_res.size);
		if (ret < 0) {
			JPU_ERR_DEV(&pdev->dev, "ion_get_heap_range failed, ret=%d\n", ret);
			jpu_destroy_resource(dev);
			jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_4, 0, __LINE__);
			return ret;
		}
		ret = ion_iommu_map_ion_phys(dev->device,
			tmp_res.ion_phys, tmp_res.size, &tmp_res.ion_vaddr, IOMMU_READ | IOMMU_WRITE);
		if (ret < 0) {
			JPU_ERR_DEV(&pdev->dev, "Failed to map ion phys, ret = %d.\n", ret);
			jpu_destroy_resource(dev);
			jpu_send_diag_error_event((u16)EventIdJPUDevIonMapErr, (u8)ERR_SEQ_0, 0, __LINE__);
			return ret;
		}
		dev->resmem.ion_phys = tmp_res.ion_phys;
		dev->resmem.ion_vaddr = tmp_res.ion_vaddr;
		dev->resmem.size = tmp_res.size;

		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		JPU_DBG_DEV(&pdev->dev, "Success to map ion phys, phys:0x%llx, vaddr:0x%llx, size:%ld.\n",
			dev->resmem.ion_phys, dev->resmem.ion_vaddr, tmp_res.size);

		jpu_pac_raise_irq(cmd_val, (void *)&(dev->resmem));
	}
#endif

	return 0;
}

static int32_t __init jpu_init_system_irq(struct platform_device *pdev,
					hb_jpu_dev_t *dev)
{
	int32_t err;
	int32_t irq;
#if 0
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		JPU_ERR_DEV(&pdev->dev, "failed to get irq resource\n");
		jpu_destroy_resource(dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOENT;
	}
	dev->irq = (uint32_t)res->start;
#else
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		JPU_ERR_DEV(&pdev->dev, "Failed to get jpu_irq(%d)", irq);
		return -ENOENT;
	}
	dev->irq = (uint32_t)irq;
#endif
	// TODO Add top half irq and bottom half irq?
	err = devm_request_threaded_irq(&pdev->dev, dev->irq, jpu_irq_handler, NULL,
				   IRQF_TRIGGER_RISING, pdev->name, (void *)dev);
	if (err != 0) {
		JPU_ERR_DEV(&pdev->dev,
			"failed to install register interrupt handler\n");
		jpu_destroy_resource(dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevReqIrqErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return err;
	}
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(&pdev->dev, "jpu irq number: irq = %d\n", dev->irq);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	return err;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t __init jpu_init_system_device(const struct platform_device *pdev,
					hb_jpu_dev_t *dev)
{
	int32_t err;

	dev->jpu_class = class_create(THIS_MODULE, JPU_DEV_NAME); /* PRQA S 1021,3469 */
	if (IS_ERR((const void *)dev->jpu_class)) {
		JPU_ERR_DEV(&pdev->dev, "failed to create class\n");
		err = (int32_t)PTR_ERR((const void *)dev->jpu_class);
		jpu_destroy_resource(dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevCreateDevErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return err;
	}

	/* get the major number of the character device */
	err = alloc_chrdev_region(&dev->jpu_dev_num, 0, 1, JPU_DEV_NAME);
	if (err < 0) {
		JPU_ERR_DEV(&pdev->dev, "failed to allocate character device\n");
		jpu_destroy_resource(dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevCreateDevErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return err;
	} else {
		dev->major = (uint32_t)MAJOR(dev->jpu_dev_num); /* PRQA S 3469,4446 */
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		dev->minor = (uint32_t)MINOR(dev->jpu_dev_num); /* PRQA S 3469,1840,4446,0499 */
	}
	dev->cdev_allocted = 1U;
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(&pdev->dev, "jpu device number: major = %d, minor = %d\n",
		dev->major, dev->minor);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	/* initialize the device structure and register the device with the kernel */
	cdev_init(&dev->cdev, &jpu_fops);
	err = cdev_add(&dev->cdev, dev->jpu_dev_num, 1);
	if (err < 0) {
		JPU_ERR_DEV(&pdev->dev, "failed to add character device\n");
		jpu_destroy_resource(dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevCreateDevErr, (u8)ERR_SEQ_2, 0, __LINE__);
		return err;
	}
	dev->cdev_added = 1U;

	dev->jpu_dev = device_create(dev->jpu_class, NULL, dev->jpu_dev_num,
					 NULL, JPU_DEV_NAME);
	if (IS_ERR((const void *)dev->jpu_dev)) {
		JPU_ERR_DEV(&pdev->dev, "failed to create device\n");
		err = (int32_t)PTR_ERR((const void *)dev->jpu_dev);
		dev->jpu_dev = NULL;
		jpu_destroy_resource(dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevCreateDevErr, (u8)ERR_SEQ_3, 0, __LINE__);
		return err;
	}

	return err;
}

static int32_t __init jpu_init_system_res(struct platform_device *pdev,
					hb_jpu_dev_t *dev)
{
	int32_t err;

	err = jpu_init_system_mem(pdev, dev);
	if (err != 0) {
		return err;
	}

	err = jpu_init_system_irq(pdev, dev);
	if (err != 0) {
		return err;
	}

	err = jpu_init_system_device(pdev, dev);
	if (err != 0) {
		return err;
	}

	return err;
}

static int32_t __init jpu_setup_module(const struct platform_device *pdev,
						hb_jpu_dev_t *dev)
{
	int32_t err = 0;
	dev->jpu_freq = MAX_JPU_FREQ;
#ifdef JPU_ENABLE_CLOCK
	err = hb_jpu_clk_get(dev, dev->jpu_freq);
	if (err < 0) {
		JPU_ERR_DEV(&pdev->dev, "failed to get clock\n");
		jpu_destroy_resource(dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevClkGetErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return err;
	}
	hb_jpu_clk_put(dev);
#endif

	dev->jpu_ion_client = ion_client_create(hb_ion_dev, JPU_DEV_NAME);
	if (IS_ERR((const void *)dev->jpu_ion_client)) {
		JPU_ERR_DEV(&pdev->dev, "failed to ion_client_create\n");
		err = (int32_t)PTR_ERR((const void *)dev->jpu_dev);
		jpu_destroy_resource(dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return err;
	}
	return err;
}

//coverity[HIS_metric_violation:SUPPRESS]
static void __init jpu_init_list(hb_jpu_dev_t *dev) {
	uint32_t i;
	for (i = 0; i < MAX_HW_NUM_JPU_INSTANCE; i++) {
		osal_waitqueue_init(&dev->interrupt_wait_q[i]);
	}

	for (i = 0; i < MAX_NUM_JPU_INSTANCE; i++) {
		osal_waitqueue_init(&dev->poll_wait_q[i]);
	}
	osal_waitqueue_init(&dev->poll_int_wait);

	osal_list_head_init(&dev->jbp_head);
	osal_list_head_init(&dev->inst_list_head);
	osal_list_head_init(&dev->dma_data_list_head);
	osal_list_head_init(&dev->phy_data_list_head);
}

static void __init jpu_init_lock(hb_jpu_dev_t *dev) {
#ifdef USE_MUTEX_IN_KERNEL_SPACE
	uint32_t i;
#endif

	osal_spin_init(&dev->irq_spinlock); /* PRQA S 3200,3469,0662 */
	dev->irq_trigger = 0;
	dev->async_queue = NULL;
	dev->open_count = 0;
	osal_sema_init(&dev->jpu_sem, 1);
	osal_spin_init(&dev->jpu_spinlock); /* PRQA S 3200,3469,0662 */
	osal_spin_init(&dev->jpu_ctx_spinlock); /* PRQA S 3200,3469,0662 */

#ifdef USE_MUTEX_IN_KERNEL_SPACE
	for (i=0; i < (uint32_t)JPUDRV_MUTEX_MAX; i++) {
		dev->current_jdi_lock_pid[i] = 0;
	}
#ifndef CONFIG_PREEMPT_RT
	osal_mutex_init(&dev->jpu_jdi_mutex);
	osal_mutex_init(&dev->jpu_jdi_jmem_mutex);
#else
	osal_sema_init(&dev->jpu_jdi_sem, 1);
	osal_sema_init(&dev->jpu_jdi_jmem_sem, 1);
#endif
#endif
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_create_debug_file(hb_jpu_dev_t *dev)
{
	int32_t ret = 0;
	char buf[MAX_FILE_PATH], *paths;

	dev->debug_root = debugfs_create_dir("jpu", NULL);
	if (dev->debug_root == NULL) {
		JPU_ERR_DEV(dev->device, "%s: failed to create debugfs root directory.\n", __func__);
		jpu_destroy_resource(dev);
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}

	dev->debug_file_jenc = debugfs_create_file("jenc", 0664, /* PRQA S 0339,3120 */
						dev->debug_root,
						(void *)dev, &jpu_jenc_fops);
	if (dev->debug_file_jenc == NULL) {
		paths = dentry_path_raw(dev->debug_root, buf, MAX_FILE_PATH);
		JPU_ERR_DEV(dev->device, "Failed to create client debugfs at %s/%s\n",
			paths, "jenc");
	}

	dev->debug_file_jdec = debugfs_create_file("jdec", 0664, /* PRQA S 0339,3120 */
						dev->debug_root,
						(void *)dev, &jpu_jdec_fops);
	if (dev->debug_file_jdec == NULL) {
		paths = dentry_path_raw(dev->debug_root, buf, MAX_FILE_PATH);
		JPU_ERR_DEV(dev->device, "Failed to create client debugfs at %s/%s\n",
			paths, "jdec");
	}

	dev->jpu_loading_setting = 1;
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	debugfs_create_u64("loadingsetting", S_IRWXUGO, dev->debug_root, &dev->jpu_loading_setting);
	dev->debug_file_loading = debugfs_create_file("loading", 0664, /* PRQA S 0339,3120 */
						dev->debug_root,
						(void *)dev, &jpu_loading_fops);
	if (dev->debug_file_loading == NULL) {
		paths = dentry_path_raw(dev->debug_root, buf, MAX_FILE_PATH);
		JPU_ERR_DEV(dev->device, "Failed to create client debugfs at %s/%s\n",
			paths, "loading");
	}

	return ret;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t __refdata jpu_probe(struct platform_device *pdev)
{
	hb_jpu_dev_t *dev;
	int32_t err;

#if defined(CONFIG_HOBOT_FUSA_DIAG)
	err = diagnose_report_startup_status(ModuleDiag_jpu, MODULE_STARTUP_BEGIN);
	if (err != 0) {
		JPU_ERR_DEV(&pdev->dev, "%s: %d  diagnose_report_startup_status fail! ret=%d\n", __func__, __LINE__, ret);
	}
#endif

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(&pdev->dev, "%s()\n", __func__);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev = (hb_jpu_dev_t *)devm_kzalloc(&pdev->dev, sizeof(hb_jpu_dev_t), OSAL_KMALLOC_KERNEL);
	if (dev == NULL) {
		JPU_ERR_DEV(&pdev->dev, "Not enough memory for JPU device.\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevProbeErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -ENOMEM;
	}
	dev->device = &pdev->dev;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev->plat_data = (hb_jpu_platform_data_t *)
		devm_kzalloc(&pdev->dev, sizeof(hb_jpu_platform_data_t), GFP_KERNEL);
	if (dev->plat_data == NULL) {
		JPU_ERR_DEV(&pdev->dev, "Not enough memory for JPU platform data\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevProbeErr, (u8)ERR_SEQ_1, 0, __LINE__);
		return -ENOMEM;
	}
	pdev->dev.platform_data = (void *)dev->plat_data;

	hb_jpu_pm_ctrl(dev, 1, 1);

	err = jpu_parse_dts(dev->device->of_node, dev);
	if (err != 0) {
		jpu_send_diag_error_event((u16)EventIdJPUDevProbeErr, (u8)ERR_SEQ_2, 0, __LINE__);

		hb_jpu_pm_ctrl(dev, 1, 0);
		return err;
	}

	err = jpu_init_system_res(pdev, dev);
	if (err != 0) {
		jpu_send_diag_error_event((u16)EventIdJPUDevProbeErr, (u8)ERR_SEQ_3, 0, __LINE__);

		hb_jpu_pm_ctrl(dev, 1, 0);
		return err;
	}
	platform_set_drvdata(pdev, (void *)dev);

	err = jpu_setup_module(pdev, dev);
	if (err != 0) {
		jpu_send_diag_error_event((u16)EventIdJPUDevProbeErr, (u8)ERR_SEQ_4, 0, __LINE__);

		hb_jpu_pm_ctrl(dev, 1, 0);
		return err;
	}

#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
	err = jpu_init_reserved_mem(pdev, dev);
	if (err != 0) {
		jpu_send_diag_error_event((u16)EventIdJPUDevProbeErr, (u8)ERR_SEQ_5, 0, __LINE__);

		hb_jpu_pm_ctrl(dev, 1, 0);
		return err;
	}
#endif

	jpu_init_lock(dev);
	jpu_init_list(dev);

	err = jpu_create_debug_file(dev);
	if (err != 0) {
		jpu_send_diag_error_event((u16)EventIdJPUDevProbeErr, (u8)ERR_SEQ_6, 0, __LINE__);

		hb_jpu_pm_ctrl(dev, 1, 0);
		return err;
	}

#if defined(CONFIG_HOBOT_FUSA_DIAG)
	err = diagnose_report_startup_status(ModuleDiag_jpu, MODULE_STARTUP_END);
	if (err != 0) {
		JPU_ERR_DEV(&pdev->dev, "%s: %d  diagnose_report_startup_status fail! ret=%d\n", __func__, __LINE__, ret);
	}
#endif

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	ai_mode = hobot_fun_ai_mode_available();
	if (ai_mode) {
		jpu_pac = dev;
		err = jpu_pac_notifier_register(&jpu_pac_notifier);
		if (err != 0) {
			JPU_ERR_DEV(&pdev->dev, "%s: %d  Register jpu pac notifier failed, err=%d\n", __func__, __LINE__, err);
			return err;
		}
	}
#endif

	hb_jpu_pm_ctrl(dev, 0, 0);

	return 0;
}

//coverity[HIS_metric_violation:SUPPRESS]
static int32_t jpu_remove(struct platform_device *pdev) /* PRQA S 3673 */
{
	hb_jpu_dev_t *dev;
	int32_t ret;

	// PRQA S 3432,4543,4403,4558,4542,1861,3344 ++
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	JPU_DBG_DEV(&pdev->dev, "%s()\n", __func__);
	// PRQA S 3432,4543,4403,4558,4542,1861,3344 --

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev = (hb_jpu_dev_t *)platform_get_drvdata(pdev);

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	ai_mode = hobot_fun_ai_mode_available();
	if (ai_mode) {
		ret = jpu_pac_notifier_unregister(&jpu_pac_notifier);
		if (ret != 0) {
			JPU_ERR_DEV(&pdev->dev, "Unregister jpu pac notifier failed, ret=%d.\n", ret);
		}
	}
#endif

	debugfs_remove_recursive(dev->debug_file_jenc);
	debugfs_remove_recursive(dev->debug_file_jdec);
	debugfs_remove_recursive(dev->debug_file_loading);
	debugfs_remove_recursive(dev->debug_root);

	if (dev->instance_pool.base != 0UL) {
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		osal_free((void *)dev->instance_pool.base);
		dev->instance_pool.base = 0;
	}

	jpu_destroy_resource(dev);

	ret = hb_jpu_hw_reset();
	if (ret != 0) {
		JPU_ERR_DEV(&pdev->dev, "Failed to reset jpu(ret=%d).\n", ret);
	}

	hb_jpu_pm_ctrl(dev, 1, 0);

	return 0;
}

#ifdef CONFIG_PM
static int32_t jpu_suspend(struct device *pdev) /* PRQA S 3673 */
{
	hb_jpu_dev_t *dev;

	// PRQA S 3469,2810,1021,3430,0306,0497 ++
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev = (hb_jpu_dev_t *) platform_get_drvdata(to_platform_device(pdev));
	// PRQA S 3469,2810,1021,3430,0306,0497 --
	if (dev == NULL) {
		JPU_ERR("The jpu dev is NULL!\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	JPU_INFO_DEV(dev->device, "[+]jpu_suspend enter\n");
	JPU_INFO_DEV(dev->device, "[-]jpu_suspend leave\n");
	return 0;

}

static int32_t jpu_resume(struct device *pdev) /* PRQA S 3673 */
{
	hb_jpu_dev_t *dev;
#if defined(CONFIG_HOBOT_FUSA_DIAG)
	int32_t ret = 0;

	ret = diagnose_report_wakeup_status(ModuleDiag_jpu, MODULE_WAKEUP_BEGIN);
	if (ret != 0) {
		JPU_ERR_DEV(pdev, "%s: %d  diagnose_report_startup_status fail! ret=%d\n", __func__, __LINE__, ret);
	}
#endif

	// PRQA S 3469,2810,1021,3430,0306,0497 ++
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	dev = (hb_jpu_dev_t *) platform_get_drvdata(to_platform_device(pdev));
	// PRQA S 3469,2810,1021,3430,0306,0497 --
	if (dev == NULL) {
		JPU_ERR("The jpu dev is NULL!\n");
		jpu_send_diag_error_event((u16)EventIdJPUDevAddrErr, (u8)ERR_SEQ_0, 0, __LINE__);
		return -EINVAL;
	}
	JPU_INFO_DEV(dev->device, "[+]jpu_resume enter\n");
	JPU_INFO_DEV(dev->device, "[-]jpu_resume leave\n");

#if defined(CONFIG_HOBOT_FUSA_DIAG)
	ret = diagnose_report_wakeup_status(ModuleDiag_jpu, MODULE_WAKEUP_END);
	if (ret != 0) {
		JPU_ERR_DEV(pdev, "%s: %d  diagnose_report_startup_status fail! ret=%d\n", __func__, __LINE__, ret);
	}
#endif
	return 0;
}
#else
#define    jpu_suspend    NULL
#define    jpu_resume    NULL
#endif /* !CONFIG_PM */

/* Power management */
static const struct dev_pm_ops jpu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS((jpu_suspend), (jpu_resume))
};

static const struct of_device_id jpu_of_match[] = {
	{
	 .compatible = "hobot,hobot_jpu",
	 .data = NULL,
	 },
	{
	 .compatible = "cm, jpu",
	 .data = NULL,
	 },
	{}, /* PRQA S 1041 */
};

MODULE_DEVICE_TABLE(of, jpu_of_match);

static struct platform_driver jpu_driver = {
	.probe = jpu_probe,
	.remove = jpu_remove,
	.driver = {
		   .name = JPU_PLATFORM_DEVICE_NAME,
		   .of_match_table = jpu_of_match,
		   .pm = &jpu_pm_ops
		   },
};


module_platform_driver(jpu_driver); /* PRQA S 3219,0605,1036,3449,3219,0605,1035,3451,3432 */

//PRQA S 0633,3213,0286 ++
MODULE_AUTHOR("Hobot");
MODULE_DESCRIPTION("Hobot JPEG processing unit linux driver");
MODULE_LICENSE("GPL v2");
//PRQA S 0633,3213,0286 --
