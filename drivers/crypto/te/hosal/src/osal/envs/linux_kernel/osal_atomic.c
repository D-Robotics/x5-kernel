//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <asm/atomic.h>
#include "osal_atomic.h"
#include "osal_log.h"
#include "osal_assert.h"

void osal_atomic_store(osal_atomic_t *atomic, uint32_t data)
{
    if (atomic == NULL) {
        atomic = (osal_atomic_t *)kmalloc(sizeof(atomic_t), GFP_KERNEL);
        OSAL_ASSERT(atomic != NULL);
    }
    atomic_set((atomic_t *)atomic, data);
}

uint32_t osal_atomic_load(osal_atomic_t *atomic)
{
    OSAL_ASSERT(atomic != NULL);
    return atomic_read((atomic_t *)atomic);
}

uint32_t osal_atomic_inc(osal_atomic_t *atomic)
{
    OSAL_ASSERT(atomic != NULL);
    return atomic_inc_return((atomic_t *)atomic);
}

uint32_t osal_atomic_dec(osal_atomic_t *atomic)
{
    OSAL_ASSERT(atomic != NULL);
    return atomic_dec_return((atomic_t *)atomic);
}
