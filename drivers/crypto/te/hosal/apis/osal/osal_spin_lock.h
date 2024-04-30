//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_SPIN_LOCK_H__
#define __OSAL_SPIN_LOCK_H__

#include "osal_err.h"
#include "osal_common.h"
#include "osal_interrupt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _osal_spin_lock_t {
    void *ctx;
} osal_spin_lock_t;

OSAL_API osal_err_t osal_spin_lock_init(osal_spin_lock_t *lock);
OSAL_API void osal_spin_lock_destroy(osal_spin_lock_t *lock);
OSAL_API void osal_spin_lock(osal_spin_lock_t *lock);
OSAL_API void osal_spin_unlock(osal_spin_lock_t *lock);
OSAL_API void osal_spin_lock_irqsave(osal_spin_lock_t *lock, osal_intr_flag_t *flags);
OSAL_API void osal_spin_unlock_irqrestore(osal_spin_lock_t *lock, osal_intr_flag_t flags);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_SPIN_LOCK_H__ */