/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_JPU_USER_H
#define HOBOT_JPU_USER_H

#include <linux/types.h>
#include "inc/hb_media_codec.h"

#define JPU_DEV_NAME					"jpu"

#define MAX_NUM_JPU_INSTANCE			(64U)
#define MAX_JPU_INST_HANDLE_SIZE		(48U)
#define JDI_NUM_LOCK_HANDLES			(4U)

typedef enum {
	JPUDRV_MUTEX_JPU,
	JPUDRV_MUTEX_JMEM,
	JPUDRV_MUTEX_MAX
} hb_jpu_mutex_t;

typedef enum {
	DEC_TASK, DEC_WORK, DEC_FBC, DEC_FBCY_TBL, DEC_FBCC_TBL, DEC_BS, DEC_FB_LINEAR, DEC_MV, DEC_ETC,
	ENC_TASK, ENC_WORK, ENC_FBC, ENC_FBCY_TBL, ENC_FBCC_TBL, ENC_BS, ENC_SRC, ENC_MV, ENC_SUBSAMBUF, ENC_ETC
//coverity[misra_c_2012_rule_5_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
} MemTypes;

typedef struct hb_jpu_drv_buffer {
	uint32_t size;
	uint64_t phys_addr;
	/* kernel logical address in use kernel */
	uint64_t base;
	/* virtual user space address */
	uint64_t virt_addr;
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	uint64_t iova;
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	int32_t fd;
	int32_t share_id;
	int64_t handle;
	uint32_t flags;
} hb_jpu_drv_buffer_t;

typedef struct hb_jpu_drv_inst {
	uint32_t inst_idx;
	/* for output only */
	int32_t inst_open_count;
} hb_jpu_drv_inst_t;

typedef struct hb_jpu_drv_intr {
	uint32_t timeout;
	uint32_t intr_reason;
	uint32_t inst_idx;
} hb_jpu_drv_intr_t;

typedef struct hb_jpu_ctx_info {
	int32_t valid;
	media_codec_context_t context;
	// decoder
	mc_jpeg_enc_params_t jpeg_params;
	mc_mjpeg_enc_params_t mjpeg_params;
	mc_rate_control_params_t rc_params;
} hb_jpu_ctx_info_t;

typedef struct hb_jpu_status_info {
	uint32_t inst_idx;
	mc_inter_status_t status;
	int32_t fps;
	// encoder
	mc_mjpeg_jpeg_output_stream_info_t stream_info;
	// decoder
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	mc_mjpeg_jpeg_output_frame_info_t frame_info;
} hb_jpu_status_info_t;

typedef struct hb_jpu_ion_fd_map {
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	int32_t fd;	/* ion fd */
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	uint64_t iova;	/* IO virtual address */
} hb_jpu_ion_fd_map_t;

typedef struct hb_jpu_ion_phys_map {
	uint64_t phys_addr;	/* physical address */
	uint32_t size;
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	uint64_t iova;	/* IO virtual address */
} hb_jpu_ion_phys_map_t;

typedef enum hb_jpu_event_e {
	JPU_EVENT_NONE = 0,
	JPU_PIC_DONE = 1,
	JPU_INST_CLOSED = 2,
	JPU_INST_INTERRUPT = 3,
} hb_jpu_event_t; /* PRQA S 1535 */

typedef struct hb_jpu_drv_instance_pool {
	int8_t jpgInstPool[MAX_NUM_JPU_INSTANCE][MAX_JPU_INST_HANDLE_SIZE];
	int32_t jpu_instance_num;
	int32_t instance_pool_inited;
	void *instPendingInst[MAX_NUM_JPU_INSTANCE];
} hb_jpu_drv_instance_pool_t;

typedef struct hb_jpu_version_info_data {
	uint32_t major;
	uint32_t minor;
} hb_jpu_version_info_data_t;

#define JDI_IOCTL_MAGIC  ((uint8_t)'J')

#define JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY          \
    _IO(JDI_IOCTL_MAGIC, 0U)
#define JDI_IOCTL_FREE_PHYSICAL_MEMORY              \
    _IO(JDI_IOCTL_MAGIC, 1U)
#define JDI_IOCTL_WAIT_INTERRUPT                    \
    _IO(JDI_IOCTL_MAGIC, 2U)
#define JDI_IOCTL_SET_CLOCK_GATE                    \
    _IO(JDI_IOCTL_MAGIC, 3U)
#define JDI_IOCTL_RESET                             \
    _IO(JDI_IOCTL_MAGIC, 4U)
#define JDI_IOCTL_GET_INSTANCE_POOL                 \
    _IO(JDI_IOCTL_MAGIC, 5U)
#define JDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO    \
    _IO(JDI_IOCTL_MAGIC, 6U)
#define JDI_IOCTL_GET_REGISTER_INFO                 \
    _IO(JDI_IOCTL_MAGIC, 7U)
#define JDI_IOCTL_OPEN_INSTANCE                     \
    _IO(JDI_IOCTL_MAGIC, 8U)
#define JDI_IOCTL_CLOSE_INSTANCE                    \
    _IO(JDI_IOCTL_MAGIC, 9U)
#define JDI_IOCTL_GET_INSTANCE_NUM                  \
    _IO(JDI_IOCTL_MAGIC, 10U)
#define JDI_IOCTL_ALLOCATE_INSTANCE_ID              \
    _IO(JDI_IOCTL_MAGIC, 14U)
#define JDI_IOCTL_FREE_INSTANCE_ID                  \
    _IO(JDI_IOCTL_MAGIC, 15U)
#define JDI_IOCTL_POLL_WAIT_INSTANCE                \
    _IO(JDI_IOCTL_MAGIC, 16U)
#define JDI_IOCTL_SET_CTX_INFO				\
    _IO(JDI_IOCTL_MAGIC, 17U)
#define JDI_IOCTL_SET_STATUS_INFO				\
    _IO(JDI_IOCTL_MAGIC, 18U)
#define JDI_IOCTL_MAP_ION_FD                        \
	_IO(JDI_IOCTL_MAGIC, 19U)
#define JDI_IOCTL_UNMAP_ION_FD                      \
	_IO(JDI_IOCTL_MAGIC, 20U)
#define JDI_IOCTL_MAP_ION_PHYS                      \
	_IO(JDI_IOCTL_MAGIC, 21U)
#define JDI_IOCTL_UNMAP_ION_PHYS                    \
	_IO(JDI_IOCTL_MAGIC, 22U)
#define JDI_IOCTL_JDI_LOCK	\
	_IO(JDI_IOCTL_MAGIC, 23U)
#define JDI_IOCTL_JDI_UNLOCK	\
	_IO(JDI_IOCTL_MAGIC, 24U)
#define JDI_IOCTL_GET_VERSION_INFO    \
	_IO(JDI_IOCTL_MAGIC, 25U)
#endif /* HOBOT_JPU_USER_H */
