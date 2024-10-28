/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright (C) 2020-2023, Horizon Robotics Co., Ltd.
 *                    All rights reserved.
 *************************************************************************/
/**
 * @file hobot_vdsp.h
 *
 * @NO{S05E05C01}
 * @ASIL{B}
 */
#ifndef HOBOT_VDSP_H
#define HOBOT_VDSP_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <ion.h>
#include <hobot_ion_iommu.h>
#include <hobot_remoteproc.h>
#ifdef CONFIG_HOBOT_VDSP_STL
#include <linux/hobot_diag.h>
#include <linux/hobot_diag_id.h>
#endif
#include "hobot_vdsp_ioctl.h"

#define VDSP_DRIVER_API_VERSION_MAJOR			(1)
#define VDSP_DRIVER_API_VERSION_MINOR			(0)
#define VDSP_DRIVER_API_VERSION_PATCH			(0)

#ifndef __STR
//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_20.10_01
#define __STR(x)	#x
#endif
#ifndef STR
#define STR(x)		__STR(x)
#endif
#define GEN_VERSION_STR(x, y, z)			STR(x) "." STR(x) "." STR(z)

#define HB_VDSP_DRIVER_VERSION_STR			GEN_VERSION_STR(VDSP_DRIVER_API_VERSION_MAJOR, \
								VDSP_DRIVER_API_VERSION_MINOR, \
								VDSP_DRIVER_API_VERSION_PATCH)


#define VDSP_DEV_NAME					"vdsp"
#define VDSP0_DEV_NAME					"vdsp0"
#define VDSP1_DEV_NAME					"vdsp1"

#define VDSP0_IDX					(0)
#define VDSP1_IDX					(1)

#define VDSP_BOOT_MODE_ASYNC				(0)

#define RPMSG_SIZE					(32U)

#define DSP_LOGLEVEL					"loglevel"
#define DSP_THREAD_ON					"thread_on"
#define DSP_THREAD_OFF					"thread_off"
#define DSP_WWDT_ON					"wdt_on"
#define DSP_WWDT_OFF					"wdt_off"
#define DSP_UART_SWITCH					"uartsw"
#define DSP_CORRECTECC_INJECT				"correctecc_injection"
#define DSP_ARITHMETIC_INJECT				"arithmetic_injection"

#define UART_COM1_NUM					"1"
#define UART_COM2_NUM					"2"
#define UART_COM3_NUM					"3"

#define ERR_TYPE_LEVEL					(0u)
#define WARN_TYPE_LEVEL					(1u)
#define INFO_TYPE_LEVEL					(2u)
#define DBG_TYPE_LEVEL					(3u)

/**
 * @struct hobot_vdsp_state
 * Define the descriptor of vdsp state.
 * @NO{S05E05C01I}
 */
struct hobot_vdsp_state
{
	uint32_t is_wwdt_enable;			/* wwdt state*/
	uint32_t loglevel;				/* log level*/
	uint32_t is_trace_on;				/* thread trace state*/
	uint32_t uart_switch;				/* switch uart*/
};

/**
 * @struct hobot_vdsp_dev_data
 * Define the descriptor of vdsp device info data.
 * @NO{S05E05C01I}
 */
struct hobot_vdsp_dev_data
{
	int32_t dsp_id;					/**< dsp id */
	char *vpathname;				/* vdsp firmware path & name */
	struct device *dev;				/**< device */
	struct miscdevice miscdev;			/**< miscdevice*/
	wait_queue_head_t poll_wait;			/**< poll wait*/
	struct mutex vdsp_smmu_lock;			/**< smmu mutex lock */
	struct rb_root vdsp_filp_root;			/**< filp rbtree.*/
	struct ion_client *vdsp_ion_client;		/**< ion client*/
	struct work_struct work_poll;			/*poll work*/
	struct workqueue_struct *wq_poll;		/*poll singlethread workqueue*/
	struct hobot_vdsp_state vdsp_state;		/*vdsp state*/
};

/**
 * @struct vdsp_filp_info
 * Define the descriptor of vdsp filp info.
 * @NO{S05E05C01I}
 */
struct vdsp_filp_info {
	struct kref ref;				/**< the reference count.*/
	struct hobot_vdsp_dev_data *vdev;		/**< the device*/
	struct rb_node filpnode;				/**< vdsp filp rbtree node.*/
	struct file *filp;				/**< the filp*/
	struct rb_root vdsp_smmu_root;			/**< smmu rbtree.*/
};

/**
 * @struct vdsp_smmu_info
 * Define the descriptor of vdsp smmu info.
 * @NO{S05E05C01I}
 */
struct vdsp_smmu_info {
	struct kref ref;				/**< the reference count.*/
	struct vdsp_filp_info *filpinfo;		/**< the device*/
	struct rb_node smmunode;				/**< vdsp smmu rbtree node.*/
	int32_t mem_fd;					/**< fd of the mem.*/
	uint64_t mem_va;				/**< virtual address of the mem.*/
	uint64_t mem_size;				/**< map size of the mem.*/
	struct ion_dma_buf_data dmabuf;			/**< Mapped IO address of the mem.*/
};

#endif /*HOBOT_VDSP_H*/
