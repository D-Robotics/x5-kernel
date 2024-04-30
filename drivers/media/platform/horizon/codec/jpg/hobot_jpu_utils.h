/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_JPU_UTILS_H
#define HOBOT_JPU_UTILS_H

#include <linux/cdev.h>
#include <linux/ion.h>
#include <hobot_ion_iommu.h>

#include "hobot_jpu_config.h"
#include "hobot_jpu_user.h"
#include "osal.h"

//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
//coverity[misra_c_2012_rule_8_5_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
extern struct ion_device *hb_ion_dev;

/* if the platform driver knows the name of this driver */
/* JPU_PLATFORM_DEVICE_NAME */
#define JPU_SUPPORT_PLATFORM_DRIVER_REGISTER

/* if this driver knows the dedicated video memory address */
/* JPU_SUPPORT_RESERVED_VIDEO_MEMORY should be disabled */

#define JPU_PLATFORM_DEVICE_NAME    "hb_jpu"
#define JPU_JPEG_PCLK_NAME           "jpu_pclk"
#define JPU_JPEG_ACLK_NAME           "jpu_aclk"
#define JPU_JPEG_CCLK_NAME           "jpu_cclk"

#define JPU_MAX_JENC_CAPACITY (3840*2160*102)	// Fix me, max jenc fps test on j6x
#define JPU_MAX_JDEC_CAPACITY (3840*2160*130)	// Fix me, max jdec fps test on j6x
#define JPU_MAX_RADIO (10000)

typedef struct hb_jpu_platform_data {
	int32_t ip_ver;
	int32_t clock_rate;
	int32_t min_rate;
} hb_jpu_platform_data_t;

/* To track the allocated memory buffer */
typedef struct hb_jpu_drv_buffer_pool {
	osal_list_head_t list;
	hb_jpu_drv_buffer_t jb;
	struct file *filp;
} hb_jpu_drv_buffer_pool_t;

/* To track the instance index and buffer in instance pool */
typedef struct hb_jpu_drv_instance_list {
	osal_list_head_t list;
	uint32_t inst_idx;
	struct file *filp;
} hb_jpu_drv_instance_list_t;

typedef struct hb_jpu_ion_dma_list {
	osal_list_head_t list;
	struct file *filp;
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	int32_t fd;
	struct ion_dma_buf_data ion_dma_data;
} hb_jpu_ion_dma_list_t;

typedef struct hb_jpu_phy_dma_list {
	osal_list_head_t list;
	struct file *filp;
	uint64_t phys_addr;
	uint32_t size;
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	uint64_t iova;
} hb_jpu_phy_dma_list_t;

typedef struct jpu_drv_context_t {
	struct fasync_struct *async_queue;
	u32 open_count;		/*!<< device reference count. Not instance count */
	u32 interrupt_reason[MAX_HW_NUM_JPU_INSTANCE];
} jpu_drv_context_t;

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
typedef struct hb_jpu_drv_resmem {
	/* size of this structure */
	uint64_t ion_phys;
	uint64_t ion_vaddr;
	size_t size;
} hb_jpu_drv_resmem_t;
#endif

typedef struct hb_jpu_dev {
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	struct device *device;
	hb_jpu_platform_data_t *plat_data;
	struct resource *jpu_mem;
	void __iomem *regs_base;
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	phys_addr_t *phys_base;
#endif
	uint32_t irq;
	uint32_t jpu_dev_num;
	uint32_t major;
	uint32_t minor;
	struct class *jpu_class;
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	struct cdev cdev;
	uint32_t cdev_allocted;
	uint32_t cdev_added;
	struct device *jpu_dev;
	struct kobject *jpu_kobj;
	struct clk *jpu_pclk;
	struct clk *jpu_aclk;
	struct clk *jpu_cclk;
	struct ion_client *jpu_ion_client;

	DECLARE_BITMAP(jpu_inst_bitmap, MAX_NUM_JPU_INSTANCE); /* PRQA S 1840,3408 */

	osal_waitqueue_t interrupt_wait_q[MAX_HW_NUM_JPU_INSTANCE];
	int32_t interrupt_flag[MAX_HW_NUM_JPU_INSTANCE];
	u32 interrupt_reason[MAX_HW_NUM_JPU_INSTANCE];
	int64_t poll_event[MAX_NUM_JPU_INSTANCE];
	osal_waitqueue_t poll_wait_q[MAX_NUM_JPU_INSTANCE];
	int64_t poll_int_event;
	osal_waitqueue_t poll_int_wait;
	int64_t total_poll;
	int64_t total_release;
	int32_t inst_open_flag[MAX_NUM_JPU_INSTANCE];
	osal_spinlock_t irq_spinlock;
	int32_t irq_trigger;
	int32_t ignore_irq;

	struct fasync_struct *async_queue;
	u32 open_count;		/*!<< device reference count. Not instance count */
	int32_t jpu_open_ref_count;
	uint64_t jpu_freq;

	osal_sem_t jpu_sem;
	osal_spinlock_t jpu_spinlock;
	osal_spinlock_t jpu_ctx_spinlock;
	osal_list_head_t jbp_head;
	osal_list_head_t inst_list_head;
	osal_list_head_t dma_data_list_head;
	osal_list_head_t phy_data_list_head;

	hb_jpu_drv_buffer_t instance_pool;
	hb_jpu_drv_buffer_t common_memory;
	u32 inst_index;
	hb_jpu_ctx_info_t jpu_ctx[MAX_NUM_JPU_INSTANCE];
	hb_jpu_status_info_t jpu_status[MAX_NUM_JPU_INSTANCE];

	struct dentry *debug_root;
	struct dentry *debug_file_jenc;
	struct dentry *debug_file_jdec;
	struct dentry *debug_file_loading;
#ifdef USE_MUTEX_IN_KERNEL_SPACE
	/* PID aquiring the jdi lock */
	int64_t current_jdi_lock_pid[JPUDRV_MUTEX_MAX];
#ifndef CONFIG_PREEMPT_RT
	osal_mutex_t jpu_jdi_mutex;
	osal_mutex_t jpu_jdi_jmem_mutex;
#else
	osal_sem_t jpu_jdi_sem;
	osal_sem_t jpu_jdi_jmem_sem;
#endif
#endif
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	hb_jpu_drv_resmem_t resmem;
#endif
	uint64_t jpu_loading;
	uint64_t jenc_loading;
	uint64_t jdec_loading;
	osal_time_t jpu_loading_ts;
	uint64_t jpu_loading_setting;
} hb_jpu_dev_t;

typedef struct hb_jpu_priv {
	hb_jpu_dev_t *jpu_dev;
	u32 inst_index;
	u32 is_irq_poll;
} hb_jpu_priv_t;

#endif /* HOBOT_JPU_UTILS_H */
