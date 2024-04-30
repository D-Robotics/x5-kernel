//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_MUTEX_H__
#define __OSAL_MUTEX_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *osal_mutex_t;
OSAL_API osal_err_t osal_mutex_create(osal_mutex_t *mutex);
OSAL_API void osal_mutex_lock(osal_mutex_t mutex);
OSAL_API void osal_mutex_unlock(osal_mutex_t mutex);
OSAL_API void osal_mutex_destroy(osal_mutex_t mutex);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_MUTEX_H__ */
