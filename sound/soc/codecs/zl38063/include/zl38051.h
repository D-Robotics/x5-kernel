/*
* zl38051.h  --  Header file for defining zl38051 specific register defines
*
* Copyright 2016 Microsemi Inc.
*/

#ifndef __ZL38051_H__
#define __ZL38051_H__


#define ZL380xx_BF_NUM_MICS_REG     0x4C0
#define ZL380xx_BF_DIST_MICS_REG    0x4C2
#define ZL380xx_BF_STEERING_DIR_REG 0x4C4
#define ZL380xx_BF_HALF_BW_REG      0x4C6  /*Half Beam Width*/
#define ZL380xx_BF_OOB_MIN_GAIN_REG 0x4CA
#define ZL380xx_BF_OOB_MAX_GAIN_REG 0x4C8  /*Out of Band Max gain*/

#define ZL380xx_SL_THRESHOLD_REG    0x4D2
#define ZL380xx_SL_CTRL_REG         0x4D0
#define ZL380xx_SL_CTRL_INBEAM_LOC_EN_SHIFT 0
#define ZL380xx_SL_CTRL_INBEAM_LOC_EN      (1<<ZL380xx_SL_CTRL_INBEAM_LOC_EN_SHIFT)
#define ZL380xx_SL_CTRL_REV_ANGLE_SHIFT     4
#define ZL380xx_SL_CTRL_REV_ANGLE           (1<<ZL380xx_SL_CTRL_REV_ANGLE_SHIFT)
#define ZL380xx_SL_CTRL_EN_SHIFT            8
#define ZL380xx_SL_CTRL_EN                  (1<<ZL380xx_SL_CTRL_EN_SHIFT)
#define ZL380xx_SND_LOC_DIR_REG     0x0A0

#endif
