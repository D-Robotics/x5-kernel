/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_VPU_CONFIG_H
#define HOBOT_VPU_CONFIG_H

#if defined(CONFIG_HOBOT_FPGA_J5) || defined(CONFIG_HOBOT_J5) || defined(CONFIG_HOBOT_FPGA_HAPS_J5)
#else
#define SUPPORT_MULTI_INST_INTR
#endif

/* definitions to be changed as customer  configuration */
/* if you want to have clock gating scheme frame by frame */
/* you can enable VPU_SUPPORT_CLOCK_CONTROL */

/* if the driver want to use interrupt service from kernel ISR */
#define VPU_SUPPORT_ISR

// #define USE_MUTEX_IN_KERNEL_SPACE
#define USE_SHARE_SEM_BT_KERNEL_AND_USER
#define VPU_LOCK_BUSY_CHECK_TIMEOUT          (120000)
/* USE_MUTEX_IN_KERNEL_SPACE for kernel mutex control */
#define USE_VPU_CLOSE_INSTANCE_ONCE_ABNORMAL_RELEASE

#if defined(PRIORITY)
#define SUPPORT_SET_PRIORITY_FOR_COMMAND
#endif

#ifdef VPU_SUPPORT_ISR
/* if the driver want to disable and enable IRQ whenever interrupt asserted. */
#define VPU_IRQ_CONTROL
#endif

/* SUPPORT_TIMEOUT_RESOLUTION for high resolution time control */
#define SUPPORT_TIMEOUT_RESOLUTION

#define WAVE420_CODE                    (0x4200U)	// Wave420
#define WAVE420L_CODE                   (0x4201U)	// Wave420L
#define WAVE521_CODE                    (0x5210U)
#define WAVE521C_CODE                   (0x521cU)
#define WAVE521C_DUAL_CODE              (0x521dU)	// wave521 dual core

#define MAX_VPU_INSTANCE_IDX            (~(uint32_t)0U)
#define MAX_INTERRUPT_QUEUE             (16U*MAX_NUM_VPU_INSTANCE)
#define MAX_NUM_VPU_CORE                (1U)
// PRQA S 3472 ++
#define PRODUCT_CODE_W_SERIES(x)	(((x) == WAVE521_CODE) || ((x) == WAVE521C_CODE) || \
									((x) == WAVE521C_DUAL_CODE) || \
									((x) == WAVE420_CODE) || ((x) == WAVE420L_CODE))
// PRQA S 3472 --
#define SIZE_COMMON 				(3*1024*1024)

#if defined(CONFIG_HOBOT_FPGA_J5) || defined(CONFIG_HOBOT_J5) || defined(CONFIG_HOBOT_FPGA_HAPS_J5)
#define WAVE4DEC_WORKBUF_SIZE			(3*1024*1024)
#define WAVE4ENC_WORKBUF_SIZE			(128*1024)
#define SIZE_WORK						(MAX_NUM_VPU_INSTANCE*(WAVE4DEC_WORKBUF_SIZE+WAVE4ENC_WORKBUF_SIZE))
#define INST_IDX_OFFSET					(22)
#define INST_IDX_MASK					(0x3f)

#define MASK_8BIT 0xffu
#define BIT_8 8u
#define FUSA_SW_ERR_CODE 0xffffu
#define FUSA_ENV_LEN 4u
#endif

#define MEM_TYPE_OFFSET                 (16)
#define MEM_TYPE_MASK                   (0x1fU)

enum vpu_event_id {
	EventIdVPUDevAddrErr = 33,
	EventIdVPUDevParamCheckErr,
	EventIdVPUDevAllocDMABufErr,
	EventIdVPUDevClkEnableErr,
	EventIdVPUDevClkDisableErr,
	EventIdVPUDevGetUserErr,
	EventIdVPUDevSetUserErr,
	EventIdVPUDevInstIdxErr,
	EventIdVPUDevInstExistErr,
	EventIdVPUDevDmaMapExistErr,
	EventIdVPUDevDmaMapFindErr,
	EventIdVPUDevIonMapErr,
	EventIdVPUDevIoctlCmdErr,
	EventIdVPUDevMmapErr,
	EventIdVPUDevReqIrqErr,
	EventIdVPUDevCreateDevErr,
	EventIdVPUDevClkGetErr,
	EventIdVPUDevProbeErr,
};

enum vpu_err_seq {
	ERR_SEQ_0,
	ERR_SEQ_1,
	ERR_SEQ_2,
	ERR_SEQ_3,
	ERR_SEQ_4,
	ERR_SEQ_5,
	ERR_SEQ_6,
	ERR_SEQ_7,
};

#endif /* HOBOT_VPU_CONFIG_H */
