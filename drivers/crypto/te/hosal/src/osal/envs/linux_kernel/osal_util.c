//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <linux/kernel.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/memory.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/jiffies.h>
#include "osal_utils.h"

uint32_t osal_read_timestamp_ms(void)
{
    return jiffies_to_msecs(jiffies);
}

void osal_delay_us(uint32_t us)
{
    udelay(us);
}

void osal_delay_ms(uint32_t ms)
{
    mdelay(ms);
    return;
}

void osal_sleep_ms(uint32_t ms)
{
    msleep(ms);
    return;
}

/*
 * Only can convert low memory & kmap() memory.
 * This require sg list page need mapped by kmap().
 */
uintptr_t osal_virt_to_phys(void *va)
{
    struct page *page = NULL;
    uintptr_t offs    = (uintptr_t)va & (PAGE_SIZE - 1);
    uintptr_t phys    = 0;

    page = kmap_to_page(va);
    phys = page_to_phys(page);
    phys += offs;

    return phys;
}

void *osal_phys_to_virt(uintptr_t pa)
{
    struct page *page = NULL;
    void *va          = NULL;
    uintptr_t offs    = (uintptr_t)pa & (PAGE_SIZE - 1);
    page              = phys_to_page(pa);

    va = page_address(page);

    return (void *)(((uintptr_t)va) + offs);
}

OSAL_API osal_err_t osal_get_mem_attr(uint64_t paddr,
                                      uint64_t sz,
                                      osal_mem_attr_t *attr)
{
    /* the following implementation is based on the chariot_dev board, the overview
       of the mememory layout is as below:
       0x00000000                   0x70000000               0x100000000
       |----------------|-----------|-------------|----------|-------~~~
       |     nsec       |    sec    |     nsec    | OCM(sec) |   nsec  ~
       |----------------|-----------|-------------|----------|-------~~~
                        0x60000000                0xFFFC0000
       users must make corresponding modifications to fit their customized platform.
    */
#define SECURE_REGION_BASE      (0x60000000ULL)
#define SECURE_REGION_SIZE      (0x10000000ULL)
#define OCM_REGION_BASE         (0xFFFC0000ULL)
#define OCM_REGION_SIZE         (0x40000ULL)
    if ((NULL == attr) || ((paddr + sz) < paddr)) {
        return OSAL_ERROR_BAD_PARAMETERS;
    }
    if (((paddr >= SECURE_REGION_BASE) &&
         ((paddr + sz) <= (SECURE_REGION_BASE + SECURE_REGION_SIZE))) ||
        ((paddr >= OCM_REGION_BASE) &&
         ((paddr + sz) <= (OCM_REGION_BASE + OCM_REGION_SIZE)))) {
        *attr = OSAL_MEM_ATTR_SECURE;
    } else if (((paddr + sz) <= SECURE_REGION_BASE) ||
               ((paddr >= (SECURE_REGION_BASE + SECURE_REGION_SIZE)) &&
                ((paddr + sz) <= OCM_REGION_BASE)) ||
               (paddr >= (OCM_REGION_BASE + OCM_REGION_SIZE))) { /* non-secure region */
        *attr = OSAL_MEM_ATTR_NONSECURE;
    } else {
        *attr = OSAL_MEM_ATTR_UNKNOWN;
    }
    return OSAL_SUCCESS;
#undef SECURE_REGION_BASE
#undef SECURE_REGION_SIZE
#undef OCM_REGION_BASE
#undef OCM_REGION_SIZE
}