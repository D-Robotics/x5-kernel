/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_VPU_USER_H
#define HOBOT_VPU_USER_H

#include <linux/types.h>
#include "inc/hb_media_codec.h"
#include "hobot_vpu_config.h"

#define USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY

#define VPU_DEV_NAME "vpu"

#define MAX_NUM_VPU_INSTANCE            (32U)	// 4 -> 32
#define MAX_VPU_INST_HANDLE_SIZE            (48U)	/* DO NOT CHANGE THIS VALUE */
#define VDI_NUM_LOCK_HANDLES                (5U)

typedef enum hb_vpu_mutex {
	VPUDRV_MUTEX_VPU,
	VPUDRV_MUTEX_DISP_FALG,
	VPUDRV_MUTEX_RESET,
	VPUDRV_MUTEX_VMEM,
	VPUDRV_MUTEX_REV1,
	VPUDRV_MUTEX_MAX
} hb_vpu_mutex_t;

typedef enum {
	DEC_TASK, DEC_WORK, DEC_FBC, DEC_FBCY_TBL, DEC_FBCC_TBL, DEC_BS, DEC_FB_LINEAR, DEC_MV, DEC_ETC,
	ENC_TASK, ENC_WORK, ENC_FBC, ENC_FBCY_TBL, ENC_FBCC_TBL, ENC_BS, ENC_SRC, ENC_MV, ENC_SUBSAMBUF, ENC_ETC
//coverity[misra_c_2012_rule_5_6_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
}MemTypes;

typedef struct hb_vpu_drv_firmware {
	/* size of this structure */
	uint32_t size;
	uint32_t core_idx;
	uint64_t reg_base_offset;
	uint16_t bit_code[512];
} hb_vpu_drv_firmware_t;

typedef struct hb_vpu_drv_buffer {
	uint32_t size;
	uint64_t phys_addr;
	uint64_t phys_addr_cb;
	uint64_t phys_addr_cr;

	/* kernel logical address in use kernel */
	uint64_t base;

	/* virtual user space address */
	uint64_t virt_addr;
	uint64_t virt_addr_cb;
	uint64_t virt_addr_cr;

	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	uint64_t iova;
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	int32_t fd;
	int32_t share_id;
	int64_t handle;
	uint32_t flags;
} hb_vpu_drv_buffer_t;

typedef struct hb_vpu_drv_inst {
	uint32_t core_idx;
	uint32_t inst_idx;

	/* for output only */
	int32_t inst_open_count;
} hb_vpu_drv_inst_t;

typedef struct hb_vpu_ctx_info {
	int32_t valid;
	media_codec_context_t context;
	// encoder
	mc_video_longterm_ref_mode_t ref_mode;
	mc_video_intra_refresh_params_t intra_refr;
	mc_rate_control_params_t rc_params;
	mc_video_deblk_filter_params_t deblk_filter;
	mc_h265_sao_params_t sao_params;
	mc_h264_entropy_params_t entropy_params;
	mc_video_vui_params_t vui_params;
	mc_video_vui_timing_params_t vui_timing;
	mc_video_slice_params_t slice_params;
	hb_u32 force_idr_header;
	hb_s32 enable_idr;
	mc_video_3dnr_enc_params_t noise_rd;
	mc_video_smart_bg_enc_params_t smart_bg;
	mc_video_pred_unit_params_t pred_unit;
	mc_video_transform_params_t transform_params;
	mc_video_roi_params_t roi_params;
	mc_video_mode_decision_params_t mode_decision;
	hb_s32 cam_pipline;
	hb_s32 cam_channel;
} hb_vpu_ctx_info_t;

typedef struct hb_vpu_status_info {
	uint32_t inst_idx;
	mc_inter_status_t status;
	int32_t fps;
	// encoder
	mc_h264_h265_output_stream_info_t stream_info;
	// decoder
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	mc_h264_h265_output_frame_info_t frame_info;
} hb_vpu_status_info_t;

typedef struct hb_vpu_drv_intr {
	uint32_t timeout;
	int32_t intr_reason;

	int32_t intr_inst_index;
} hb_vpu_drv_intr_t;

typedef struct hb_vpu_ion_fd_map {
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	int32_t fd;	/* ion fd */
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	uint64_t iova;	/* IO virtual address */
} hb_vpu_ion_fd_map_t;

typedef struct hb_vpu_ion_phys_map {
	uint64_t phys_addr;	/* physical address */
	uint32_t size;
	//coverity[misra_c_2012_rule_5_7_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
	uint64_t iova;	/* IO virtual address */
} hb_vpu_ion_phys_map_t;

typedef enum hb_vpu_event_e {
	VPU_EVENT_NONE = 0,
	VPU_ENC_PIC_DONE = 1,
	VPU_DEC_PIC_DONE = 2,
	VPU_INST_CLOSED = 3,
	VPU_INST_INTERRUPT = 4,
} hb_vpu_event_t; /* PRQA S 1535 */

typedef struct hb_vpu_instance_pool {
	uint8_t
	codec_inst_pool[MAX_NUM_VPU_INSTANCE][MAX_VPU_INST_HANDLE_SIZE];
	hb_vpu_drv_buffer_t vpu_common_buffer;
	int32_t vpu_instance_num;
	int32_t instance_pool_inited;
	void* pendingInst;
	int32_t pendingInstIdxPlus1;
	int32_t doSwResetInstIdxPlus1;
	uint32_t lastPerformanceCycles;
} hb_vpu_instance_pool_t;

#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
typedef struct hb_vpu_instance_pool_data {
	hb_vpu_drv_buffer_t vdb;
	uint32_t cmd_type;
} hb_vpu_instance_pool_data_t;
#endif

typedef struct hb_vpu_version_info_data {
	uint32_t major;
	uint32_t minor;
} hb_vpu_version_info_data_t;

#define VDI_IOCTL_MAGIC						((uint8_t)'V')
#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY			\
	_IO(VDI_IOCTL_MAGIC, 0U)
#define VDI_IOCTL_FREE_PHYSICALMEMORY				\
	_IO(VDI_IOCTL_MAGIC, 1U)
#define VDI_IOCTL_WAIT_INTERRUPT				\
	_IO(VDI_IOCTL_MAGIC, 2U)
#define VDI_IOCTL_SET_CLOCK_GATE				\
	_IO(VDI_IOCTL_MAGIC, 3U)
#define VDI_IOCTL_RESET						\
	_IO(VDI_IOCTL_MAGIC, 4U)
#define VDI_IOCTL_GET_INSTANCE_POOL				\
	_IO(VDI_IOCTL_MAGIC, 5U)
#define VDI_IOCTL_GET_COMMON_MEMORY				\
	_IO(VDI_IOCTL_MAGIC, 6U)
#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO		\
	_IO(VDI_IOCTL_MAGIC, 8U)
#define VDI_IOCTL_OPEN_INSTANCE					\
	_IO(VDI_IOCTL_MAGIC, 9U)
#define VDI_IOCTL_CLOSE_INSTANCE				\
	_IO(VDI_IOCTL_MAGIC, 10U)
#define VDI_IOCTL_GET_INSTANCE_NUM				\
	_IO(VDI_IOCTL_MAGIC, 11U)
#define VDI_IOCTL_GET_REGISTER_INFO				\
	_IO(VDI_IOCTL_MAGIC, 12U)
#define VDI_IOCTL_GET_FREE_MEM_SIZE				\
	_IO(VDI_IOCTL_MAGIC, 13U)
#define VDI_IOCTL_ALLOCATE_INSTANCE_ID				\
	_IO(VDI_IOCTL_MAGIC, 14U)
#define VDI_IOCTL_FREE_INSTANCE_ID				\
	_IO(VDI_IOCTL_MAGIC, 15U)
#define VDI_IOCTL_POLL_WAIT_INSTANCE				\
	_IO(VDI_IOCTL_MAGIC, 16U)
#define VDI_IOCTL_SET_CTX_INFO				\
	_IO(VDI_IOCTL_MAGIC, 17U)
#define VDI_IOCTL_SET_STATUS_INFO				\
	_IO(VDI_IOCTL_MAGIC, 18U)
#define VDI_IOCTL_MAP_ION_FD				\
	_IO(VDI_IOCTL_MAGIC, 19U)
#define VDI_IOCTL_UNMAP_ION_FD				\
	_IO(VDI_IOCTL_MAGIC, 20U)
#define VDI_IOCTL_MAP_ION_PHYS				\
	_IO(VDI_IOCTL_MAGIC, 21U)
#define VDI_IOCTL_UNMAP_ION_PHYS				\
	_IO(VDI_IOCTL_MAGIC, 22U)
#define VDI_IOCTL_VDI_LOCK	\
	_IO(VDI_IOCTL_MAGIC, 23U)
#define VDI_IOCTL_VDI_UNLOCK	\
	_IO(VDI_IOCTL_MAGIC, 24U)
#define VDI_IOCTL_GET_VERSION_INFO    \
	_IO(VDI_IOCTL_MAGIC, 25U)
#ifdef SUPPORT_SET_PRIORITY_FOR_COMMAND
#define VDI_IOCTL_SET_ENC_DEC_PIC	\
	_IO(VDI_IOCTL_MAGIC, 26U)
#endif
#endif /* HOBOT_VPU_USER_H */
