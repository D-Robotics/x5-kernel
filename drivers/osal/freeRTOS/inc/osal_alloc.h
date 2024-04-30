/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_ALLOC_H__
#define __OSAL_ALLOC_H__

#include <FreeRTOS.h>
#include <string.h>

#ifndef __user
#define __user
#endif

#ifndef GFP_ATOMIC
#define __GFP_HIGH             0x20u
#define __GFP_KSWAPD_RECLAIM   0x800u
#define GFP_ATOMIC    (__GFP_HIGH|__GFP_KSWAPD_RECLAIM)
#endif

static inline void* osal_kmalloc(size_t size, uint32_t flags)
{
    (void)flags;
    return pvPortMalloc(size);
}

static inline void* osal_kzalloc(size_t size, uint32_t flags)
{
    (void)flags;
    void *ret = pvPortMalloc(size);
    if (ret) {
        memset(ret, 0, size);
    }
    return ret;
}

static inline void osal_kfree(void *ptr)
{
    vPortFree(ptr);
}

static inline void* osal_malloc(size_t size)
{
    return pvPortMalloc(size);
}

static inline void osal_free(void *ptr)
{
    vPortFree(ptr);
}

/**
 * @brief copy content form app
 *
 * @param[out]  to: pointer to dest address
 * @param[in] from: pointer to source address
 * @param[in]    n: the length to be copied
 *
 * @return the length already copied
 */
static inline size_t osal_copy_from_app(void *to, const void  __user *from, size_t n)
{
	memcpy(to, from, n);
	return 0;
}

/**
 * @brief copy content to app
 *
 * @param[out]  to: pointer to dest address
 * @param[in] from: pointer to source address
 * @param[in]    n: the length to be copied
 *
 * @return the length already copied
 */
static inline size_t osal_copy_to_app(void __user *to, const void *from, size_t n)
{
	memcpy(to, from, n);
	return 0;
}

#endif
