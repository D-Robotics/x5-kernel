//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_INTERNAL_H__
#define __OSAL_INTERNAL_H__

#include "utils_ext.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void *osal_env_alloc(size_t size);
void *osal_env_zalloc(size_t size);
void osal_env_free(void *ptr);
unsigned long osal_mem_debug_lock(void);
void osal_mem_debug_unlock(unsigned long flags);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_INTERNAL_H__ */
