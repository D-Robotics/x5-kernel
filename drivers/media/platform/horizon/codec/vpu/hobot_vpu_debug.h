/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_VPU_DEBUG_H
#define HOBOT_VPU_DEBUG_H

#include <linux/kernel.h>
#include "osal.h"

// PRQA S 1034,0342 ++
//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define VPU_DEBUG(fmt, args...)		\
		osal_pr_debug("[VPUDRV]%s:%d: " fmt,	\
			__func__, __LINE__, ##args);

//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define VPU_ERR(fmt, args...)				\
		(void)osal_pr_err("[VPUDRV]%s:%d: " fmt,		\
			__func__, __LINE__, ##args);	\

//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define VPU_INFO(fmt, args...)				\
		osal_pr_info("[VPUDRV]%s:%d: " fmt,		\
			__func__, __LINE__, ##args);

//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define VPU_DBG_DEV(dev, fmt, args...)		\
		osal_pr_debug("[VPUDRV] %s:%d: " fmt,	\
			__func__, __LINE__, ##args);

//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define VPU_ERR_DEV(dev, fmt, args...)		\
		(void)osal_pr_err("[VPUDRV] %s:%d: " fmt,	\
			__func__, __LINE__, ##args);

//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define VPU_INFO_DEV(dev, fmt, args...)		\
		osal_pr_info("[VPUDRV]%s:%d: " fmt,	\
			__func__, __LINE__, ##args);
// PRQA S 1034,0342 --
#define VPU_DEBUG_ENTER() VPU_DEBUG("enter\n")
#define VPU_DEBUG_LEAVE() VPU_DEBUG("leave\n")

#endif /* HOBOT_VPU_DEBUG_H */
