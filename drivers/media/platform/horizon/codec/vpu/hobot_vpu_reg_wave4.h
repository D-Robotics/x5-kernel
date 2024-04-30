/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_VPU_REG_WAVE4_H
#define HOBOT_VPU_REG_WAVE4_H

#include <linux/io.h>
#include "hobot_vpu_utils.h"

/* WAVE4 registers */
#define W4_RET_SUCCESS				(WAVE_REG_BASE + 0x0110)
#define W4_RET_FAIL_REASON			(WAVE_REG_BASE + 0x0114)

#define W4_RST_BLOCK_ALL			(0x7fffffff)
#define W4_INST_INDEX				(WAVE_REG_BASE + 0x0108)
#define W4_VPU_INT_REASON			(WAVE_REG_BASE + 0x004c)


/* Note: W4_INIT_CODE_BASE_ADDR should be aligned to 4KB */
#define W4_ADDR_CODE_BASE			(WAVE_REG_BASE + 0x0118)
#define W4_CODE_SIZE				(WAVE_REG_BASE + 0x011C)
#define W4_CODE_PARAM				(WAVE_REG_BASE + 0x0120)
#define W4_HW_OPTION				(WAVE_REG_BASE + 0x0124)
#define W4_INIT_VPU_TIME_OUT_CNT	(WAVE_REG_BASE + 0x0134)

/* WAVE4 Wave4BitIssueCommand */
#define W4_CORE_INDEX				(WAVE_REG_BASE + 0x0104)
#define W4_INST_INDEX				(WAVE_REG_BASE + 0x0108)

#define W4_MAX_CODE_BUF_SIZE		(512U*1024U)
#define W4_CMD_INIT_VPU				(0x0001)
#define W4_CMD_SLEEP_VPU			(0x0400)
#define W4_CMD_WAKEUP_VPU			(0x0800)
#define W4_CMD_FINI_SEQ				(0x0004)

typedef enum {
	W4_INT_INIT_VPU			= 0,
	W4_INT_DEC_PIC_HDR		= 1,
	W4_INT_FINI_SEQ			= 2,
	W4_INT_DEC_PIC			= 3,
	W4_INT_SET_FRAMEBUF		= 4,
	W4_INT_FLUSH_DEC		= 5,
	W4_INT_GET_FW_VERSION	= 9,
	W4_INT_QUERY_DEC		= 10,
	W4_INT_SLEEP_VPU		= 11,
	W4_INT_WAKEUP_VPU		= 12,
	W4_INT_CHANGE_INT		= 13,
	W4_INT_CREATE_INSTANCE  = 14,
	W4_INT_BSBUF_EMPTY	    = 15,   /*!<< Bitstream buffer empty */
	W4_INT_ENC_SLICE_INT    = 15,
} Wave4InterruptBit; /* PRQA S 1535 */

#endif /*HOBOT_VPU_REG_WAVE4_H*/
