/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_VPU_UTILS_H
#define HOBOT_VPU_UTILS_H

#include <linux/cdev.h>
#include <linux/ion.h>
#include <linux/kfifo.h>
#include <hobot_ion_iommu.h>
#include "osal.h"
#include "hobot_vpu_user.h"
#include "hobot_vpu_config.h"
#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
#include "hobot_vpu_prio.h"
#endif
// TODO remove this
/* if this driver knows the dedicated video memory address */
/* VPU_SUPPORT_RESERVED_VIDEO_MEMORY should be disabled */

#define VPU_PLATFORM_DEVICE_NAME "hb_vpu"
#define VPU_PCLK_NAME "vpu_pclk"
#define VPU_ACLK_NAME "vpu_aclk"
#define VPU_VCPU_BPU_CLK_NAME "vpu_bclk"
#define VPU_VCE_CLK_NAME "vpu_cclk"

#define VPU_MAX_VENC_CAPACITY (3840UL*2160UL*60UL)	// Fix me, max venc fps test on j6x
#define VPU_MAX_VDEC_CAPACITY (3840UL*2160UL*86UL)	// Fix me, max vdec fps test on j6x
#define VPU_MAX_RADIO (1000UL)

extern struct ion_device *hb_ion_dev;

typedef struct hb_vpu_driver_data {
	const char *fw_name;
} hb_vpu_driver_data_t;

typedef struct hb_vpu_platform_data {
	uint32_t ip_ver;
	uint32_t clock_rate;
	uint32_t min_rate;
} hb_vpu_platform_data_t;

typedef struct hb_vpu_ion_dma_list {
	osal_list_head_t list;
	struct file *filp;
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	int32_t fd;
	struct ion_dma_buf_data ion_dma_data;
} hb_vpu_ion_dma_list_t;

typedef struct hb_vpu_phy_dma_list {
	osal_list_head_t list;
	struct file *filp;
	uint64_t phys_addr;
	uint32_t size;
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	uint64_t iova;
} hb_vpu_phy_dma_list_t;

/* To track the allocated memory buffer */
typedef struct hb_vpu_drv_buffer_pool {
	osal_list_head_t list;
	hb_vpu_drv_buffer_t vb;
	struct file *filp;
} hb_vpu_drv_buffer_pool_t;

/* To track the instance index and buffer in instance pool */
typedef struct hb_vpu_instance_list {
	osal_list_head_t list;
	uint32_t inst_idx;
	uint32_t core_idx;
	struct file *filp;
} hb_vpu_instance_list_t;

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
typedef struct hb_vpu_drv_resmem {
	/* size of this structure */
	uint64_t work_base;
	uint64_t ion_phys;
	uint64_t ion_vaddr;
	size_t size;
} hb_vpu_drv_resmem_t;
#endif

typedef struct hb_vpu_dev {
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	struct device *device;
	hb_vpu_platform_data_t *plat_data;
	const hb_vpu_driver_data_t *drv_data;
	struct resource *vpu_mem;
	struct resource codec_mem_reserved;
	void __iomem *regs_base;
	uint32_t irq;
	uint32_t vpu_dev_num;
	int32_t major;
	int32_t minor;
	struct class *vpu_class;
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	struct cdev cdev;
	uint32_t cdev_allocted;
	uint32_t cdev_added;
	struct device *vpu_dev;
	struct kobject *vpu_kobj;
	struct clk *vpu_pclk;
	struct clk *vpu_aclk;
	struct clk *vpu_bclk;
	struct clk *vpu_cclk;
	struct ion_client *vpu_ion_client;
	struct ion_handle *vpu_com_mem_handle;

	DECLARE_BITMAP(vpu_inst_bitmap, MAX_NUM_VPU_INSTANCE); /* PRQA S 1840,3408 */

	osal_waitqueue_t poll_int_wait_q[MAX_NUM_VPU_INSTANCE];
	int64_t poll_int_event[MAX_NUM_VPU_INSTANCE];
	osal_waitqueue_t poll_wait_q[MAX_NUM_VPU_INSTANCE];
	int64_t poll_event[MAX_NUM_VPU_INSTANCE];
	int64_t total_poll[MAX_NUM_VPU_INSTANCE];
	int64_t total_release[MAX_NUM_VPU_INSTANCE];
#ifdef SUPPORT_MULTI_INST_INTR
	osal_waitqueue_t interrupt_wait_q[MAX_NUM_VPU_INSTANCE];
	int32_t interrupt_flag[MAX_NUM_VPU_INSTANCE];
	osal_fifo_t interrupt_pending_q[MAX_NUM_VPU_INSTANCE]; /*PRQA S ALL*/
	osal_spinlock_t vpu_kfifo_lock;
	uint64_t interrupt_reason[MAX_NUM_VPU_INSTANCE];
#else
	osal_waitqueue_t interrupt_wait_q;
	int32_t interrupt_flag;
	uint64_t interrupt_reason;
#endif
	int32_t inst_open_flag[MAX_NUM_VPU_INSTANCE]; // for upper layer have loaded fm
	osal_spinlock_t irq_spinlock;
	int32_t irq_trigger;
	int32_t ignore_irq;

	struct fasync_struct *async_queue;
	u32 open_count;		/*!<< device reference count. Not instance count */
	int32_t vpu_open_ref_count;
	uint64_t vpu_freq;

	hb_vpu_drv_firmware_t bit_fm_info[MAX_NUM_VPU_CORE];

	u32 vpu_reg_store[MAX_NUM_VPU_CORE][64];
	osal_sem_t vpu_sem;
	osal_spinlock_t vpu_spinlock;
	osal_spinlock_t vpu_ctx_spinlock;
	osal_list_head_t vbp_head;
	osal_list_head_t inst_list_head;
	osal_list_head_t dma_data_list_head;
	osal_list_head_t phy_data_list_head;

	hb_vpu_drv_buffer_t instance_pool;
	hb_vpu_drv_buffer_t common_memory;
#if defined(CONFIG_HOBOT_FPGA_J5) || defined(CONFIG_HOBOT_J5) || defined(CONFIG_HOBOT_FPGA_HAPS_J5) || defined(CONFIG_PCIE_HOBOT_EP_FUN_AI)
	hb_vpu_drv_buffer_t dec_work_memory[MAX_NUM_VPU_INSTANCE];
	hb_vpu_drv_buffer_t enc_work_memory[MAX_NUM_VPU_INSTANCE];
#endif
	uint32_t common_memory_mapped;
	hb_vpu_ctx_info_t vpu_ctx[MAX_NUM_VPU_INSTANCE];
	hb_vpu_status_info_t vpu_status[MAX_NUM_VPU_INSTANCE];

	struct dentry *debug_root;
	struct dentry *debug_file_venc;
	struct dentry *debug_file_vdec;
	struct dentry *debug_file_loading;
#ifdef USE_MUTEX_IN_KERNEL_SPACE
	/* PID aquiring the vdi lock */
	int64_t current_vdi_lock_pid[VPUDRV_MUTEX_MAX];
#ifndef CONFIG_PREEMPT_RT
	osal_mutex_t vpu_vdi_mutex;
	osal_mutex_t vpu_vdi_disp_mutex;
	osal_mutex_t vpu_vdi_reset_mutex;
	osal_mutex_t vpu_vdi_vmem_mutex;
#else
	osal_sem_t vpu_vdi_sem;
	osal_sem_t vpu_vdi_disp_sem;
	osal_sem_t vpu_vdi_reset_sem;
	osal_sem_t vpu_vdi_vmem_sem;
#endif
#endif
#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
	hb_vpu_drv_resmem_t resmem;
#endif
#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
	hb_vpu_prio_queue_t *prio_queue;
	hb_vpu_instance_pool_data_t pool_data;
	int32_t cmd_type;
	u32 cmd_type_inited;
	// osal_mutex_t vpu_prio_mutex;
	int32_t inst_prio[MAX_NUM_VPU_INSTANCE];
	int32_t prio_flag;
	int32_t first_flag;
#endif
	uint64_t vpu_loading;
	uint64_t venc_loading;
	uint64_t vdec_loading;
	osal_time_t vpu_loading_ts;
	uint64_t vpu_loading_setting;
} hb_vpu_dev_t;

typedef struct hb_vpu_priv {
	hb_vpu_dev_t *vpu_dev;
	u32 inst_index;
	u32 is_irq_poll;
} hb_vpu_priv_t;

#ifdef USE_VPU_CLOSE_INSTANCE_ONCE_ABNORMAL_RELEASE
#define VPU_WAKE_MODE 0
#define VPU_SLEEP_MODE 1
typedef enum {
	VPUAPI_RET_SUCCESS,
	VPUAPI_RET_FAILURE, // an error reported by FW
	VPUAPI_RET_TIMEOUT,
	VPUAPI_RET_STILL_RUNNING,
	VPUAPI_RET_INVALID_PARAM,
	VPUAPI_RET_MAX
} VpuApiRet;
int vpuapi_close(struct file *filp, hb_vpu_dev_t *dev, u32 core, u32 inst);
int vpuapi_dec_set_stream_end(struct file *filp, hb_vpu_dev_t *dev, u32 core, u32 inst);
int vpuapi_dec_clr_all_disp_flag(struct file *filp, hb_vpu_dev_t *dev, u32 core, u32 inst);
int vpuapi_get_output_info(struct file *filp, hb_vpu_dev_t *dev, u32 core, u32 inst, u32 *error_reason);
#if defined(CONFIG_PM)
int vpu_sleep_wake(struct file *filp, hb_vpu_dev_t *dev, u32 core, int mode);
#endif
int vpu_do_sw_reset(struct file *filp, hb_vpu_dev_t *dev, u32 core, u32 inst, u32 error_reason);
int _vpu_close_instance(struct file *filp, hb_vpu_dev_t *dev, u32 core, u32 inst);
int vpu_check_is_decoder(hb_vpu_dev_t *dev, u32 core, u32 inst);
#endif

#endif /* HOBOT_VPU_UTILS_H */
