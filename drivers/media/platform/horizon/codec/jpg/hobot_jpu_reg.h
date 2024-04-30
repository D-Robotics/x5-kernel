/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_JPU_REG_H
#define HOBOT_JPU_REG_H

#include <linux/io.h>

#define JPU_READL(addr)                 (readl(dev->regs_base + (addr)))
#define JPU_WRITEL(addr, val)           (writel((val), dev->regs_base + (addr)))

#define JPU_REG_BASE_ADDR               (0xA8010000)	//0x75300000
#define JPU_REG_SIZE                    (0x300)

#define NPT_BASE                        (0x00U)	/// Reference X2AJ2A address MAP
#define NPT_REG_SIZE                    (0x300U)	/// 64KB
#define MJPEG_PIC_STATUS_REG(_inst_no)  (NPT_BASE + ((_inst_no) * NPT_REG_SIZE) + 0x004U)
#define MJPEG_PIC_START_REG(_inst_no)   (NPT_BASE + ((_inst_no) * NPT_REG_SIZE) + 0x000U)
#define NPT_PROC_BASE                   (NPT_BASE + (4U * NPT_REG_SIZE))
#define MJPEG_INST_CTRL_START_REG       (NPT_PROC_BASE + 0x000U)
#define MJPEG_INST_CTRL_STATUS_REG      (NPT_PROC_BASE + 0x004U)

typedef enum {
	INST_CTRL_IDLE = 0U,
	INST_CTRL_LOAD = 1U,
	INST_CTRL_RUN = 2U,
	INST_CTRL_PAUSE = 3U,
	INST_CTRL_ENC = 4U,
	INST_CTRL_PIC_DONE = 5U,
	INST_CTRL_SLC_DONE = 6U
} InstCtrlStates;

typedef enum {
	INT_JPU_DONE = 0U,
	INT_JPU_ERROR = 1U,
	INT_JPU_BIT_BUF_EMPTY = 2U,
	INT_JPU_BIT_BUF_FULL = 2U,
	INT_JPU_SLICE_DONE = 9U,
} InterruptJpu;

#endif /* HOBOT_JPU_REG_H */
