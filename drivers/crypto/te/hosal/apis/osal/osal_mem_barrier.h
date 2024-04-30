//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_MEM_BARRIER_H__
#define __OSAL_MEM_BARRIER_H__

#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* write memory barrier */
OSAL_API void osal_wmb(void);

/* read memory barrier */
OSAL_API void osal_rmb(void);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_MEM_BARRIER_H__ */