/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_JPU_DEBUG_H
#define HOBOT_JPU_DEBUG_H

#include <linux/kernel.h>
#include "osal.h"

// PRQA S 1034,0342 ++
//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define JPU_DEBUG(fmt, args...)				\
		osal_pr_debug("[JPUDRV]%s:%d: " fmt,		\
			__func__, __LINE__, ##args);

//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define JPU_ERR(fmt, args...)				\
		(void)osal_pr_err("[JPUDRV]%s:%d: " fmt,	\
			__func__, __LINE__, ##args);

//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define JPU_INFO(fmt, args...)				\
		osal_pr_info("[JPUDRV]%s:%d: " fmt,		\
			__func__, __LINE__, ##args);

//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define JPU_DBG_DEV(dev, fmt, args...)		\
		osal_pr_debug("[JPUDRV] %s:%d: " fmt,	\
			__func__, __LINE__, ##args);

//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define JPU_ERR_DEV(dev, fmt, args...)		\
		(void)osal_pr_err("[JPUDRV] %s:%d: " fmt,	\
			__func__, __LINE__, ##args);

//coverity[misra_c_2012_rule_20_10_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
#define JPU_INFO_DEV(dev, fmt, args...)		\
		osal_pr_info("[JPUDRV]%s:%d: " fmt,	\
			__func__, __LINE__, ##args);
// PRQA S 1034,0342 --
#define JPU_DEBUG_ENTER() JPU_DEBUG("enter\n")
#define JPU_DEBUG_LEAVE() JPU_DEBUG("leave\n")

#endif /* HOBOT_JPU_DEBUG_H */
