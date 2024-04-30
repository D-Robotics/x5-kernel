/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_VPU_REG_WAVE5_H
#define HOBOT_VPU_REG_WAVE5_H

#include "hobot_vpu_utils.h"

/* WAVE5 registers */
#define W5_RET_SUCCESS				(WAVE_REG_BASE + 0x0108)

#define W5_REG_BASE				0x0000
#define W5_VPU_BUSY_STATUS			(W5_REG_BASE + 0x0070)
#define W5_VPU_INT_REASON_CLEAR		(W5_REG_BASE + 0x0034)
#define W5_VPU_VINT_CLEAR			(W5_REG_BASE + 0x003C)
#define W5_VPU_VPU_INT_STS			(W5_REG_BASE + 0x0044)
#define W5_VPU_INT_REASON 			(W5_REG_BASE + 0x004c)
#define W5_RET_FAIL_REASON			(W5_REG_BASE + 0x010C)

#ifdef SUPPORT_MULTI_INST_INTR
#define W5_RET_BS_EMPTY_INST			(WAVE_REG_BASE + 0x01E4)
#define W5_RET_QUEUE_CMD_DONE_INST		(WAVE_REG_BASE + 0x01E8)
#define W5_RET_SEQ_DONE_INSTANCE_INFO	(WAVE_REG_BASE + 0x01FC)
#endif

/* WAVE5 INIT, WAKEUP */
#define W5_CMD_INSTANCE_INFO		(WAVE_REG_BASE + 0x0110)
#define W5_RST_BLOCK_ALL			(0x3fffffff)

#define W5_PO_CONF					(W5_REG_BASE + 0x0000)
#define W5_VCPU_CUR_PC              (W5_REG_BASE + 0x0004)
#define W5_VCPU_CUR_LR              (W5_REG_BASE + 0x0008)
#define W5_VPU_PDBG_STEP_MASK_V     (W5_REG_BASE + 0x000C)
#define W5_VPU_PDBG_CTRL            (W5_REG_BASE + 0x0010)         // vCPU debugger ctrl register
#define W5_VPU_PDBG_IDX_REG         (W5_REG_BASE + 0x0014)         // vCPU debugger index register
#define W5_VPU_PDBG_WDATA_REG       (W5_REG_BASE + 0x0018)         // vCPU debugger write data register
#define W5_VPU_PDBG_RDATA_REG       (W5_REG_BASE + 0x001C)         // vCPU debugger read data register
#define W5_VPU_FIO_CTRL_ADDR        (W5_REG_BASE + 0x0020)
#define W5_VPU_FIO_DATA             (W5_REG_BASE + 0x0024)
#define W5_VPU_VINT_REASON_USR      (W5_REG_BASE + 0x0030)
#define W5_VPU_VINT_REASON_CLR      (W5_REG_BASE + 0x0034)
#define W5_VPU_HOST_INT_REQ         (W5_REG_BASE + 0x0038)
#define W5_VPU_VINT_CLEAR           (W5_REG_BASE + 0x003C)
#define W5_VPU_HINT_CLEAR           (W5_REG_BASE + 0x0040)
#define W5_VPU_VPU_INT_STS          (W5_REG_BASE + 0x0044)
#define W5_VPU_VINT_ENABLE			(W5_REG_BASE + 0x0048)
#define W5_VPU_VINT_REASON          (W5_REG_BASE + 0x004C)
#define W5_VPU_RESET_REQ			(W5_REG_BASE + 0x0050)
#define W5_RST_BLOCK_CCLK(_core)    (1<<_core)
#define W5_RST_BLOCK_CCLK_ALL       (0xff)
#define W5_RST_BLOCK_BCLK(_core)    (0x100<<_core)
#define W5_RST_BLOCK_BCLK_ALL       (0xff00)
#define W5_RST_BLOCK_ACLK(_core)    (0x10000<<_core)
#define W5_RST_BLOCK_ACLK_ALL       (0xff0000)
#define W5_RST_BLOCK_VCPU_ALL       (0x3f000000)
#define W5_RST_BLOCK_ALL            (0x3fffffff)
#define W5_VPU_RESET_STATUS         (W5_REG_BASE + 0x0054)

#define W5_VCPU_RESTART             (W5_REG_BASE + 0x0058)
#define W5_VPU_CLK_MASK             (W5_REG_BASE + 0x005C)
#define W5_VPU_REMAP_CTRL			(W5_REG_BASE + 0x0060)
#define W5_VPU_REMAP_VADDR			(W5_REG_BASE + 0x0064)
#define W5_VPU_REMAP_PADDR			(W5_REG_BASE + 0x0068)
#define W5_VPU_REMAP_CORE_START		(W5_REG_BASE + 0x006C)
#define W5_COMMAND					(W5_REG_BASE + 0x0100)

#define W5_REMAP_CODE_INDEX			0U

/* WAVE5 registers */
#define W5_ADDR_CODE_BASE			(WAVE_REG_BASE + 0x0110)
#define W5_CODE_SIZE				(WAVE_REG_BASE + 0x0114)
#define W5_CODE_PARAM				(WAVE_REG_BASE + 0x0118)
#define W5_HW_OPTION				(WAVE_REG_BASE + 0x012C)
#define W5_INIT_VPU_TIME_OUT_CNT	(WAVE_REG_BASE + 0x0130)
#define W5_QUERY_OPTION				(WAVE_REG_BASE + 0x0104)

// #define GET_RESULT					(2)

#define W5_MAX_CODE_BUF_SIZE		(512U*1024U)
#define W5_CMD_INIT_VPU				(0x0001)
#define W5_CMD_SLEEP_VPU			(0x0004)
#define W5_CMD_WAKEUP_VPU			(0x0002)
// #define W5_DESTROY_INSTANCE		(0x0020)
// #define W5_QUERY					(0x4000)

#define W5_VPU_FIO_DATA				(W5_REG_BASE + 0x0024)
#define W5_VPU_FIO_CTRL_ADDR        (W5_REG_BASE + 0x0020)
#define W5_VCPU_CUR_PC              (W5_REG_BASE + 0x0004)
#define W5_CMD_REG_END              0x00000200
#define WAVE5_MAX_CODE_BUF_SIZE     (1024U*1024U)
#define WAVE5_UPPER_PROC_AXI_ID     0x0U
#define W5_VPU_RET_VPU_CONFIG0                  (W5_REG_BASE + 0x0098)
#define W5_VPU_RET_VPU_CONFIG1                  (W5_REG_BASE + 0x009C)
#define WAVE5_PROC_AXI_ID           0x0U
#define WAVE5_PRP_AXI_ID            0x0U
#define WAVE5_FBD_Y_AXI_ID          0x0U
#define WAVE5_FBC_Y_AXI_ID          0x0U
#define WAVE5_FBD_C_AXI_ID          0x0U
#define WAVE5_FBC_C_AXI_ID          0x0U
#define WAVE5_SEC_AXI_ID            0x0U
#define WAVE5_PRI_AXI_ID            0x0U
/************************************************************************/
/* GDI register for Debugging                                           */
/************************************************************************/
#define W5_GDI_BASE                         0x8800
#define W5_GDI_BUS_CTRL                     (W5_GDI_BASE + 0x0F0)
#define W5_GDI_BUS_STATUS                   (W5_GDI_BASE + 0x0F4)

#define W5_BACKBONE_BASE_VCPU               0xFE00
#define W5_BACKBONE_BUS_CTRL_VCPU           (W5_BACKBONE_BASE_VCPU + 0x010)
#define W5_BACKBONE_BUS_STATUS_VCPU         (W5_BACKBONE_BASE_VCPU + 0x014)
#define W5_BACKBONE_PROG_AXI_ID             (W5_BACKBONE_BASE_VCPU + 0x00C)

#define W5_BACKBONE_BASE_VCORE0             0x8E00
#define W5_BACKBONE_BUS_CTRL_VCORE0         (W5_BACKBONE_BASE_VCORE0 + 0x010)
#define W5_BACKBONE_BUS_STATUS_VCORE0       (W5_BACKBONE_BASE_VCORE0 + 0x014)

#define W5_BACKBONE_BASE_VCORE1             0x9E00  // for dual-core product
#define W5_BACKBONE_BUS_CTRL_VCORE1         (W5_BACKBONE_BASE_VCORE1 + 0x010)
#define W5_BACKBONE_BUS_STATUS_VCORE1       (W5_BACKBONE_BASE_VCORE1 + 0x014)

#define W5_COMBINED_BACKBONE_BASE           0xFE00
#define W5_COMBINED_BACKBONE_BUS_CTRL       (W5_COMBINED_BACKBONE_BASE + 0x010)
#define W5_COMBINED_BACKBONE_BUS_STATUS     (W5_COMBINED_BACKBONE_BASE + 0x014)
/************************************************************************/
/* DECODER - QUERY : UPDATE_DISP_FLAG                                   */
/************************************************************************/
#define W5_CMD_DEC_SET_DISP_IDC             (W5_REG_BASE + 0x0118)
#define W5_CMD_DEC_CLR_DISP_IDC             (W5_REG_BASE + 0x011C)
/************************************************************************/
/* DECODER - QUERY : GET_RESULT                                         */
/************************************************************************/
#define W5_CMD_DEC_ADDR_REPORT_BASE         (W5_REG_BASE + 0x0114)
#define W5_CMD_DEC_REPORT_SIZE              (W5_REG_BASE + 0x0118)
#define W5_CMD_DEC_REPORT_PARAM             (W5_REG_BASE + 0x011C)

#define W5_RET_DEC_DECODING_SUCCESS         (W5_REG_BASE + 0x01DC)
#ifdef SUPPORT_SW_UART
#define W5_SW_UART_STATUS					(W5_REG_BASE + 0x01D4)
#define W5_SW_UART_TX_DATA					(W5_REG_BASE + 0x01D8)
//#define W5_RET_DEC_WARN_INFO                (W5_REG_BASE + 0x01D4)
//#define W5_RET_DEC_ERR_INFO                 (W5_REG_BASE + 0x01D8)
#else
#define W5_RET_DEC_WARN_INFO                (W5_REG_BASE + 0x01D4)
#define W5_RET_DEC_ERR_INFO                 (W5_REG_BASE + 0x01D8)
#endif
#define W5_BS_OPTION                            (W5_REG_BASE + 0x0120)

#define WAVE5_SYSERR_QUEUEING_FAIL                                     0x00000001U
#define WAVE5_SYSERR_ACCESS_VIOLATION_HW                               0x00000040U
#define WAVE5_SYSERR_RESULT_NOT_READY                                  0x00000800U
#define WAVE5_SYSERR_VPU_STILL_RUNNING                                 0x00001000U
#define WAVE5_SYSERR_UNKNOWN_CMD                                       0x00002000U
#define WAVE5_SYSERR_UNKNOWN_CODEC_STD                                 0x00004000U
#define WAVE5_SYSERR_UNKNOWN_QUERY_OPTION                              0x00008000U
#define WAVE5_SYSERR_VLC_BUF_FULL                                      0x00010000U
#define WAVE5_SYSERR_WATCHDOG_TIMEOUT                                  0x00020000U
#define WAVE5_SYSERR_VCPU_TIMEOUT                                      0x00080000U
#define WAVE5_SYSERR_NEED_MORE_TASK_BUF                                0x00400000U
#define WAVE5_SYSERR_PRESCAN_ERR                                       0x00800000U
#define WAVE5_SYSERR_ENC_GBIN_OVERCONSUME                              0x01000000U
#define WAVE5_SYSERR_ENC_MAX_ZERO_DETECT                               0x02000000U
#define WAVE5_SYSERR_ENC_LVL_FIRST_ERROR                               0x04000000U
#define WAVE5_SYSERR_ENC_EG_RANGE_OVER                                 0x08000000U
#define WAVE5_SYSERR_ENC_IRB_FRAME_DROP                                0x10000000U
#define WAVE5_SYSERR_INPLACE_V                                         0x20000000U
#define WAVE5_SYSERR_FATAL_VPU_HANGUP                                  0xf0000000U

typedef enum {
    GET_VPU_INFO        = 0,
    SET_WRITE_PROT      = 1,
    GET_RESULT          = 2,
    UPDATE_DISP_FLAG    = 3,
    GET_BW_REPORT       = 4,
    GET_BS_RD_PTR       = 5,    // for decoder
    GET_BS_WR_PTR       = 6,    // for encoder
    GET_SRC_BUF_FLAG    = 7,    // for encoder
    SET_BS_RD_PTR       = 8,    // for decoder
    GET_DEBUG_INFO      = 0x61,
} QUERY_OPT;

typedef enum {
	INT_WAVE5_INIT_VPU = 0,
	INT_WAVE5_WAKEUP_VPU = 1,
	INT_WAVE5_SLEEP_VPU = 2,
	INT_WAVE5_CREATE_INSTANCE = 3,
	INT_WAVE5_FLUSH_INSTANCE = 4,
	INT_WAVE5_DESTORY_INSTANCE = 5,
	INT_WAVE5_INIT_SEQ = 6,
	INT_WAVE5_SET_FRAMEBUF = 7,
	INT_WAVE5_DEC_PIC = 8,
	INT_WAVE5_ENC_PIC = 8,
	INT_WAVE5_ENC_SET_PARAM = 9,
#ifdef SUPPORT_SOURCE_RELEASE_INTERRUPT
	INT_WAVE5_ENC_SRC_RELEASE = 10,
#endif
	INT_WAVE5_ENC_LOW_LATENCY = 13,
	INT_WAVE5_DEC_QUERY = 14,
	INT_WAVE5_BSBUF_EMPTY = 15,
	INT_WAVE5_BSBUF_FULL = 15,
} Wave5InterruptBit; /* PRQA S 1535 */

typedef enum {
	W5_INIT_VPU 	   = 0x0001,
	W5_WAKEUP_VPU	   = 0x0002,
	W5_SLEEP_VPU	   = 0x0004,
	W5_CREATE_INSTANCE = 0x0008,			/* queuing command */
	W5_FLUSH_INSTANCE  = 0x0010,
	W5_DESTROY_INSTANCE= 0x0020,			/* queuing command */
	W5_INIT_SEQ 	   = 0x0040,			/* queuing command */
	W5_SET_FB		   = 0x0080,
	W5_DEC_PIC		   = 0x0100,			/* queuing command */
	W5_ENC_PIC		   = 0x0100,			/* queuing command */
	W5_ENC_SET_PARAM   = 0x0200,			/* queuing command */
	W5_QUERY		   = 0x4000,
	W5_UPDATE_BS	   = 0x8000,
	W5_RESET_VPU	   = 0x10000,
	W5_MAX_VPU_COMD = 0x10000,
} W5_VPU_COMMAND;

#endif /*HOBOT_VPU_REG_WAVE5_H*/
