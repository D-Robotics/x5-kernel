//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_SEM_H__
#define __OSAL_SEM_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *osal_sem_t;
OSAL_API osal_err_t osal_sem_create(osal_sem_t *sem, uint32_t init_value);
OSAL_API void osal_sem_post(osal_sem_t sem);
OSAL_API void osal_sem_wait(osal_sem_t sem);
OSAL_API void osal_sem_destroy(osal_sem_t sem);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_SEM_H__ */
