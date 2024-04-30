//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>
#include <linux/cache.h>
#include "osal_log.h"
#include "osal_assert.h"
#include "osal_cache.h"

osal_err_t osal_cache_clean(uint8_t *buf, size_t size)
{
    /*NOTE: this is only support AARCH64 */
    //__flush_dcache_area(buf, size);
    unsigned long end = (unsigned long )buf + size;
    dcache_clean_poc((unsigned long )buf, end);
    return OSAL_SUCCESS;
}

osal_err_t osal_cache_flush(uint8_t *buf, size_t size)
{
    /*NOTE: this is only support AARCH64 */
    //__flush_dcache_area(buf, size);
    unsigned long end = (unsigned long )buf + size;
    dcache_clean_inval_poc((unsigned long )buf, end);
    return OSAL_SUCCESS;
}

osal_err_t osal_cache_invalidate(uint8_t *buf, size_t size)
{
    /*NOTE: this is only support AARCH64 */
    //__inval_dcache_area(buf, size);
    unsigned long end = (unsigned long )buf + size;
    dcache_inval_poc((unsigned long )buf, end);
    return OSAL_SUCCESS;
}
