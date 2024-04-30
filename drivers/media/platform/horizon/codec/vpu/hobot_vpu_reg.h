/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_VPU_REG_H
#define HOBOT_VPU_REG_H

#include <linux/io.h>
#include "hobot_vpu_utils.h"
#include "hobot_vpu_reg_wave5.h"
#include "hobot_vpu_reg_wave4.h"

#define VPU_READL(addr)			(readl(dev->regs_base	\
								+ dev->bit_fm_info[core].reg_base_offset	\
								+ (addr)))
#define VPU_WRITEL(addr, val)	(writel((val), dev->regs_base	\
								+ dev->bit_fm_info[core].reg_base_offset	\
								+ (addr)))

/* if the platform driver knows this driver */
/* the definition of VPU_REG_BASE_ADDR and VPU_REG_SIZE are not meaningful */

#define VPU_REG_BASE_ADDR			(0xA8000000)
#define VPU_REG_SIZE				(0x4000*MAX_NUM_VPU_CORE)

#ifdef USE_VPU_CLOSE_INSTANCE_ONCE_ABNORMAL_RELEASE
#define VPU_CREAT_INST_CHECK_TIMEOUT    (10UL*(unsigned long)HZ)
#define VPU_BUSY_CHECK_TIMEOUT          (5UL*(unsigned long)HZ)
#define VPU_DEC_TIMEOUT                 (10UL*(unsigned long)HZ)

// BIT_RUN command
enum {
	DEC_SEQ_INIT = 1,
	ENC_SEQ_INIT = 1,
	DEC_SEQ_END = 2,
	ENC_SEQ_END = 2,
	PIC_RUN = 3,
	SET_FRAME_BUF = 4,
	ENCODE_HEADER = 5,
	ENC_PARA_SET = 6,
	DEC_PARA_SET = 7,
	DEC_BUF_FLUSH = 8,
	RC_CHANGE_PARAMETER    = 9,
	VPU_SLEEP = 10,
	VPU_WAKE = 11,
	ENC_ROI_INIT = 12,
	FIRMWARE_GET = 0xf
};
/**
* @brief This is an enumeration for declaring return codes from API function calls.
The meaning of each return code is the same for all of the API functions, but 
the reasons of non-successful return might be different. Some details of those
reasons are briefly described in the API definition chapter. In this chapter, the basic 
meaning of each return code is presented.
*/
typedef enum {
    RETCODE_SUCCESS,                    /**< This means that operation was done successfully.  */  /* 0  */
    RETCODE_FAILURE,                    /**< This means that operation was not done successfully. When un-recoverable decoder error happens such as header parsing errors, this value is returned from VPU API.  */
    RETCODE_INVALID_HANDLE,             /**< This means that the given handle for the current API function call was invalid (for example, not initialized yet, improper function call for the given handle, etc.).  */
    RETCODE_INVALID_PARAM,              /**< This means that the given argument parameters (for example, input data structure) was invalid (not initialized yet or not valid anymore). */
    RETCODE_INVALID_COMMAND,            /**< This means that the given command was invalid (for example, undefined, or not allowed in the given instances).  */
    RETCODE_ROTATOR_OUTPUT_NOT_SET,     /**< This means that rotator output buffer was not allocated even though postprocessor (rotation, mirroring, or deringing) is enabled. */  /* 5  */
    RETCODE_ROTATOR_STRIDE_NOT_SET,     /**< This means that rotator stride was not provided even though postprocessor (rotation, mirroring, or deringing) is enabled.  */
    RETCODE_FRAME_NOT_COMPLETE,         /**< This means that frame decoding operation was not completed yet, so the given API function call cannot be allowed.  */
    RETCODE_INVALID_FRAME_BUFFER,       /**< This means that the given source frame buffer pointers were invalid in encoder (not initialized yet or not valid anymore).  */
    RETCODE_INSUFFICIENT_FRAME_BUFFERS, /**< This means that the given numbers of frame buffers were not enough for the operations of the given handle. This return code is only received when calling VPU_DecRegisterFrameBuffer() or VPU_EncRegisterFrameBuffer() function. */
    RETCODE_INVALID_STRIDE,             /**< This means that the given stride was invalid (for example, 0, not a multiple of 8 or smaller than picture size). This return code is only allowed in API functions which set stride.  */   /* 10 */
    RETCODE_WRONG_CALL_SEQUENCE,        /**< This means that the current API function call was invalid considering the allowed sequences between API functions (for example, missing one crucial function call before this function call).  */
    RETCODE_CALLED_BEFORE,              /**< This means that multiple calls of the current API function for a given instance are invalid. */
    RETCODE_NOT_INITIALIZED,            /**< This means that VPU was not initialized yet. Before calling any API functions, the initialization API function, VPU_Init(), should be called at the beginning.  */
    RETCODE_USERDATA_BUF_NOT_SET,       /**< This means that there is no memory allocation for reporting userdata. Before setting user data enable, user data buffer address and size should be set with valid value. */
    RETCODE_MEMORY_ACCESS_VIOLATION,    /**< This means that access violation to the protected memory has been occurred. */   /* 15 */
    RETCODE_VPU_RESPONSE_TIMEOUT,       /**< This means that VPU response time is too long, time out. */
    RETCODE_INSUFFICIENT_RESOURCE,      /**< This means that VPU cannot allocate memory due to lack of memory. */
    RETCODE_NOT_FOUND_BITCODE_PATH,     /**< This means that BIT_CODE_FILE_PATH has a wrong firmware path or firmware size is 0 when calling VPU_InitWithBitcode() function.  */
    RETCODE_NOT_SUPPORTED_FEATURE,      /**< This means that HOST application uses an API option that is not supported in current hardware.  */
    RETCODE_NOT_FOUND_VPU_DEVICE,       /**< This means that HOST application uses the undefined product ID. */   /* 20 */
    RETCODE_CP0_EXCEPTION,              /**< This means that coprocessor exception has occurred. (WAVE only) */
    RETCODE_STREAM_BUF_FULL,            /**< This means that stream buffer is full in encoder. */
    RETCODE_ACCESS_VIOLATION_HW,        /**< This means that GDI access error has occurred. It might come from violation of write protection region or spec-out GDI read/write request. (WAVE only) */
    RETCODE_QUERY_FAILURE,              /**< This means that query command was not successful. (WAVE5 only) */
    RETCODE_QUEUEING_FAILURE,           /**< This means that commands cannot be queued. (WAVE5 only) */
    RETCODE_VPU_STILL_RUNNING,          /**< This means that VPU cannot be flushed or closed now, because VPU is running. (WAVE5 only) */
    RETCODE_REPORT_NOT_READY,           /**< This means that report is not ready for Query(GET_RESULT) command. (WAVE5 only) */
    RETCODE_VLC_BUF_FULL,               /**< This means that VLC buffer is full in encoder. (WAVE5 only) */
    RETCODE_INVALID_SFS_INSTANCE,       /**< This means that current instance can't run sub-framesync. (already an instance was running with sub-frame sync (WAVE5 only) */
#ifdef AUTO_FRM_SKIP_DROP
    RETCODE_FRAME_DROP,                 /**< This means that frame is dropped. HOST application don't have to wait INT_BIT_PIC_RUN.  (CODA9 only) */
#endif
} RetCode;

#endif

/* implement to power management functions */
#define BIT_BASE					(0x0000)
#define BIT_CODE_RUN				(BIT_BASE + 0x000)
#define BIT_CODE_DOWN				(BIT_BASE + 0x004)
#define BIT_INT_CLEAR				(BIT_BASE + 0x00C)
#define BIT_INT_STS					(BIT_BASE + 0x010)
#define BIT_CODE_RESET				(BIT_BASE + 0x014)
#define BIT_INT_REASON				(BIT_BASE + 0x174)
#define BIT_BUSY_FLAG				(BIT_BASE + 0x160)
#define BIT_RUN_COMMAND				(BIT_BASE + 0x164)
#define BIT_RUN_INDEX				(BIT_BASE + 0x168)
#define BIT_RUN_COD_STD				(BIT_BASE + 0x16C)

/* Product register */
#define VPU_PRODUCT_CODE_REGISTER		(BIT_BASE + 0x1044)
#if defined(VPU_SUPPORT_PLATFORM_DRIVER_REGISTER) && defined(CONFIG_PM)
static u32 s_vpu_reg_store[MAX_NUM_VPU_CORE][64];
#endif

/* WAVE registers */
#define WAVE_REG_BASE					(0x0000)
#define WAVE_VPU_BUSY_STATUS			(WAVE_REG_BASE + 0x0070)
#define WAVE_VPU_INT_REASON_CLEAR		(WAVE_REG_BASE + 0x0034)
#define WAVE_VPU_VINT_CLEAR				(WAVE_REG_BASE + 0x003C)
#define WAVE_VPU_VPU_INT_STS			(WAVE_REG_BASE + 0x0044)
#define WAVE_VPU_INT_REASON				(WAVE_REG_BASE + 0x004c)

/* WAVE INIT, WAKEUP */
#define WAVE_PO_CONF					(WAVE_REG_BASE + 0x0000)
#define WAVE_VCPU_CUR_PC				(WAVE_REG_BASE + 0x0004)

#define WAVE_VPU_VINT_ENABLE			(WAVE_REG_BASE + 0x0048)

#define WAVE_VPU_RESET_REQ				(WAVE_REG_BASE + 0x0050)
#define WAVE_VPU_RESET_STATUS			(WAVE_REG_BASE + 0x0054)

#define WAVE_VPU_REMAP_CTRL				(WAVE_REG_BASE + 0x0060)
#define WAVE_VPU_REMAP_VADDR			(WAVE_REG_BASE + 0x0064)
#define WAVE_VPU_REMAP_PADDR			(WAVE_REG_BASE + 0x0068)
#define WAVE_VPU_REMAP_CORE_START		(WAVE_REG_BASE + 0x006C)

#define WAVE_REMAP_CODE_INDEX			(0U)

#define WAVE_COMMAND					(WAVE_REG_BASE + 0x0100)
#define WAVE_VPU_HOST_INT_REQ			(WAVE_REG_BASE + 0x0038)

#define VPU_ISSUE_COMMAND(product_code, core, cmd) \
			do {	\
				VPU_WRITEL(WAVE_VPU_BUSY_STATUS, 1);	\
				if ((product_code) == WAVE420_CODE) {	\
					VPU_WRITEL(W4_CORE_INDEX, core);	\
				}	\
				VPU_WRITEL(WAVE_COMMAND, cmd);	\
				VPU_WRITEL(WAVE_VPU_HOST_INT_REQ, 1);	\
			} while (0)

#endif /*HOBOT_VPU_REG_H*/
