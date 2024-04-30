//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <linux/slab.h>
#include <linux/mm.h>
#include "osal_assert.h"
#include "osal_mem.h"
#include "osal_log.h"
#include "osal_string.h"
#include "../../common/osal_internal.h"

#ifdef CFG_OSAL_MEM_DEBUG
static DEFINE_SPINLOCK(alock);
#endif

void *osal_env_alloc(size_t size)
{
    return kmalloc(size, GFP_KERNEL);
}

void *osal_env_zalloc(size_t size)
{
    return kzalloc(size, GFP_KERNEL);
}

void osal_env_free(void *ptr)
{
    kfree(ptr);
    return;
}

#ifdef CFG_OSAL_MEM_DEBUG
unsigned long osal_mem_debug_lock(void)
{
    unsigned long flags = 0;
    spin_lock_irqsave(&alock, flags);
    return flags;
}

void osal_mem_debug_unlock(unsigned long flags)
{
    spin_unlock_irqrestore(&alock, flags);
    return;
}
#endif
