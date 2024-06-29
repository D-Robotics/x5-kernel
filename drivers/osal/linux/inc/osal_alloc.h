/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_ALLOC_H__
#define __OSAL_ALLOC_H__

#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

/**
 * @def OSAL_KMALLOC_KERNEL
 * Mainly used for Linux, tell the allocator to use GFP_KERNEL
 *
 * @def OSAL_KMALLOC_ATOMIC
 * Mainly used for Linux, tell the allocator to use GFP_ATOMIC
 */
#define OSAL_KMALLOC_KERNEL 0x1
#define OSAL_KMALLOC_ATOMIC 0x2

/**
 * @brief allocate physical continuous memory
 *
 * @param[in] size: memory size to allocate
 * @param[in] flags: allocation flags, mainly used to align linux GFP_ATOMIC and GFP_KERNEL
 *                   other OS can ignore this param
 * @return virutal address pointer.
 */
static inline void* osal_kmalloc(size_t size, uint32_t flags)
{
	if (flags & OSAL_KMALLOC_KERNEL)
		return kmalloc(size, GFP_KERNEL);

	if (flags & OSAL_KMALLOC_ATOMIC)
		return kmalloc(size, GFP_ATOMIC);

	return kmalloc(size, GFP_KERNEL);
}

/**
 * @brief allocate physical continuous memory and initialized with all 0
 *
 * @param[in]  size: memory size to allocate
 * @param[in] flags: allocation flags, mainly used to align linux GFP_ATOMIC and GFP_KERNEL
 *
 * @return virutal address pointer.
 */
static inline void* osal_kzalloc(size_t size, uint32_t flags)
{
	if (flags & OSAL_KMALLOC_KERNEL)
		return kzalloc(size, GFP_KERNEL);

	if (flags & OSAL_KMALLOC_ATOMIC)
		return kzalloc(size, GFP_ATOMIC);

	return kzalloc(size, GFP_KERNEL);
}

/**
 * @brief deallocate memory
 *
 * @param[in] ptr: pointer to memory
 */
static inline void osal_kfree(void *ptr)
{
	kfree(ptr);
}

/**
 * @brief allocate physical non-continuous memory
 *
 * @param[in] size: memory size to allocate
 *
 * @return virutal address pointer.
 */
static inline void* osal_malloc(size_t size)
{
	return vmalloc(size);
}

/**
 * @brief deallocate memory
 *
 * @param[in] ptr: pointer to memory
 */
static inline void osal_free(void *ptr)
{
	vfree(ptr);
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
	return copy_from_user(to, from, n);
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
	return copy_to_user(to, from, n);
}

#endif
