/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_CACHE_H__
#define __OSAL_CACHE_H__

#include <asm/cacheflush.h>

/**
 * @brief Flush DMA memory out to RAM by virtual address
 *
 * @param[in] vaddr virtual address to flush
 * @param[in] size memory size
 */
static inline void osal_dma_cache_clean(void *vaddr, size_t size)
{
	// FIXME: __dma_clean_area 没有export，暂时用__dma_flush_area
	//__dma_flush_area(vaddr, size);
}

/**
 * @brief Invalidate DMA memory out to RAM by virtual address
 *
 * @param[in] vaddr virtual address to flush
 * @param[in] size memory size
 */
static inline void osal_dma_cache_invalidate(void *vaddr, size_t size)
{
	//__inval_dcache_area(vaddr, size);
}

/**
 * @brief Flush DMA memory out to RAM and invalidate the caches by virtual address.
 *
 * @param[in] vaddr virtual address to flush
 * @param[in] size memory size
 */
static inline void osal_osal_dma_cache_flush(void *vaddr, size_t size)
{
	//__flush_dcache_area(vaddr, size);
}

#endif
