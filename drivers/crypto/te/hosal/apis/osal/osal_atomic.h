//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_ATOMIC_H__
#define __OSAL_ATOMIC_H__

#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *osal_atomic_t;
OSAL_API void osal_atomic_store(osal_atomic_t *atomic, uint32_t data);
OSAL_API uint32_t osal_atomic_load(osal_atomic_t *atomic);

/* return the new value after inc */
OSAL_API uint32_t osal_atomic_inc(osal_atomic_t *atomic);

/* return the new value after dec */
OSAL_API uint32_t osal_atomic_dec(osal_atomic_t *atomic);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_ATOMIC_H__ */
