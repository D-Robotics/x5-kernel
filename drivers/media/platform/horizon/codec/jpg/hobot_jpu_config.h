/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_JPU_CONFIG_H
#define HOBOT_JPU_CONFIG_H

/* definitions to be changed as customer  configuration */
/* if you want to have clock gating scheme frame by frame */
/* you can enable JPU_SUPPORT_CLOCK_CONTROL */

/* if the driver want to use interrupt service from kernel ISR */
#define JPU_SUPPORT_ISR

#define USE_MUTEX_IN_KERNEL_SPACE
// #define USE_SHARE_SEM_BT_KERNEL_AND_USER
#ifdef JPU_SUPPORT_ISR
/* if the driver want to disable and enable IRQ whenever interrupt asserted. */
#define JPU_IRQ_CONTROL
#endif

/* SUPPORT_TIMEOUT_RESOLUTION for high resolution time control */
#define SUPPORT_TIMEOUT_RESOLUTION

#define MAX_HW_NUM_JPU_INSTANCE         (4U)
#define MAX_NUM_JPU_CORE                (1U)
#define JDI_NUM_LOCK_HANDLES            (4U)

#define MAX_JPU_INSTANCE_IDX            (~(uint32_t)0U)

#define MASK_8BIT 0xffu
#define BIT_8 8u
#define FUSA_SW_ERR_CODE 0xffffu
#define FUSA_ENV_LEN 4u
#define MEM_TYPE_OFFSET					(16)
#define MEM_TYPE_MASK					(0x1fU)

enum jpu_event_id {
	EventIdJPUDevAddrErr = 33,
	EventIdJPUDevParamCheckErr,
	EventIdJPUDevAllocDMABufErr,
	EventIdJPUDevClkEnableErr,
	EventIdJPUDevClkDisableErr,
	EventIdJPUDevGetUserErr,
	EventIdJPUDevSetUserErr,
	EventIdJPUDevInstIdxErr,
	EventIdJPUDevInstExistErr,
	EventIdJPUDevDmaMapExistErr,
	EventIdJPUDevDmaMapFindErr,
	EventIdJPUDevIonMapErr,
	EventIdJPUDevIoctlCmdErr,
	EventIdJPUDevMmapErr,
	EventIdJPUDevReqIrqErr,
	EventIdJPUDevCreateDevErr,
	EventIdJPUDevClkGetErr,
	EventIdJPUDevProbeErr,
};

enum jpu_err_seq {
	ERR_SEQ_0,
	ERR_SEQ_1,
	ERR_SEQ_2,
	ERR_SEQ_3,
	ERR_SEQ_4,
	ERR_SEQ_5,
	ERR_SEQ_6,
};

#endif /* HOBOT_JPU_CONFIG_H */
