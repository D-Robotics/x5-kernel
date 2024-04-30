//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include "osal_spin_lock.h"
#include "osal_mem.h"
#include "osal_log.h"
#include "osal_assert.h"

osal_err_t osal_spin_lock_init(osal_spin_lock_t *lock)
{
    spinlock_t *slock = NULL;
    OSAL_ASSERT(lock != NULL);
    OSAL_ASSERT(lock->ctx == NULL);

    slock = (spinlock_t *)kmalloc(sizeof(spinlock_t), GFP_KERNEL);
    OSAL_ASSERT(slock != NULL);
    spin_lock_init(slock);
    lock->ctx = (void *)slock;

    return OSAL_SUCCESS;
}

void osal_spin_lock_destroy(osal_spin_lock_t *lock)
{
    OSAL_ASSERT(lock != NULL);
    OSAL_ASSERT(lock->ctx != NULL);

    kfree(lock->ctx);
    lock->ctx = NULL;
    return;
}

void osal_spin_lock(osal_spin_lock_t *lock)
{
    spinlock_t *slock = NULL;
    OSAL_ASSERT(lock != NULL);
    OSAL_ASSERT(lock->ctx != NULL);

    slock = (spinlock_t *)lock->ctx;
    spin_lock(slock);
    return;
}

void osal_spin_unlock(osal_spin_lock_t *lock)
{
    spinlock_t *slock = NULL;
    OSAL_ASSERT(lock != NULL);
    OSAL_ASSERT(lock->ctx != NULL);

    slock = (spinlock_t *)lock->ctx;
    spin_unlock(slock);
    return;
}

void osal_spin_lock_irqsave(osal_spin_lock_t *lock, unsigned long *flags)
{
    spinlock_t *slock = NULL;
    OSAL_ASSERT(lock != NULL);
    OSAL_ASSERT(flags != NULL);
    OSAL_ASSERT(lock->ctx != NULL);

    slock = (spinlock_t *)lock->ctx;
    spin_lock_irqsave(slock, (*flags));
    return;
}

void osal_spin_unlock_irqrestore(osal_spin_lock_t *lock, unsigned long flags)
{
    spinlock_t *slock = NULL;
    OSAL_ASSERT(lock != NULL);
    OSAL_ASSERT(lock->ctx != NULL);

    slock = (spinlock_t *)lock->ctx;
    spin_unlock_irqrestore(slock, flags);
    return;
}
