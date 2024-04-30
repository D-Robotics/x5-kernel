//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <driver/te_drv_aca.h>
#include <hwa/te_hwa_aca.h>
#include "drv_aca_internal.h"

/**
 * \brief This file contains the SRAM operations in ACA engine.
 *
 * The TE SRAM is contains OP_CTX limbs which are the bignumber data. The TE
 * SRAM is managed by one SRAM pool and aims at:
 * 1. Uses TE SRAM as match as possible.
 * 2. Never lose BN data when doing SRAM operations.
 * 3. Read/Write SRAM without disabling interrupts.
 *
 * Each OP_CTX contains one sram_block to save the SRAM infomation, such as
 * the SRAM address in ACA engine, the swapped area, and some flags. Details
 * about sram_block see structure sram_block_t
 *
 * The SRAM pool contains one linked list ( used_blocks in
 * aca_sram_pool_t ) to link all the allocated sram blocks, and each node
 * contains the structure sram_t. These sram_t nodes in the used_blocks
 * list are sorted by ascending order of SRAM address.
 *
 * When creating one sram_block_t in OP_CTX, the SRAM manager tries to
 * allocate the data area from TE SRAM ( the upper SRAM pool ).If there is
 * enough space in the SRAM pool, then the data area is at TE SRAM, and
 * sram_addr pointer in sram_block_t points to the TE SRAM address (which
 * is also the new created sram_t node's addr ), and the sram_block_t
 * is linked to the preempt_list of aca_sram_pool_t.
 * If there is no enough space in the SRAM pool, then the data area is at
 * swapped area, and swapped_addr pointer points to the system heap address, and
 * the sram_block_t is linked to the swapped_list of aca_sram_pool_t.
 *
 * When one OP_CTX wants to call ACA engine, it should first make sure the BN
 * data is stored in TE SRAM area by calling aca_sram_get. This function will
 * reserve the OP_CTX needed SRAM space from the SRAM pool and swap in the BN
 * data if this OP_CTX's BN data is in swapped area. And then changes the
 * sram_block_t to busy_list.
 * After calling ACA engine, it is required to call aca_sram_put to changes the
 * BN data to preemptable (so that other OP_CTX can have SRAM space to do
 * operation). And the aca_sram_put changes the sram_block_t to preempt_list
 * again.
 *
 * All the operations of the upper three linked-lists MUST be locked with lock
 * in aca_sram_pool_t.
 */

/* The SRAM Pool's magic */
#define ACA_SRAM_POOL_MAGIC (0x4173724DU) /* AsrM */

/* The node structure in used_blocks of SRAM Pool */
typedef struct _sram_t {
    void *addr;
    size_t size;
    sqlist_t node;
} sram_t;

/*
 * Count leading zero bits in a given integer
 */
static size_t _sram_clz(const uint32_t x)
{
    size_t j;
    uint32_t mask = (uint32_t)1 << (32 - 1);

    for (j = 0; j < 32; j++) {
        if (x & mask) {
            break;
        }
        mask >>= 1;
    }

    return j;
}

/**
 * \brief Initialize one SRAM pool
 */
int aca_drv_init_sram_pool(aca_sram_pool_t *sram_pool,
                           const te_hwa_aca_t *aca_hwa)
{
    int ret          = TE_SUCCESS;
    void *sram_base  = NULL;
    size_t sram_size = 0;

    CHECK_PARAM(sram_pool);
    CHECK_PARAM(aca_hwa);

    /* The SRAM pool alignment is TE ACA core's granularity */
    sram_pool->alignment =
        aca_hwa->get_core_granularity((te_hwa_aca_t *)aca_hwa);
    TE_ASSERT(sram_pool->alignment > 0);
    sram_pool->alignment = sram_pool->alignment / 8;

    sram_pool->max_size = aca_hwa->get_core_max_op_len((te_hwa_aca_t *)aca_hwa);
    TE_ASSERT(sram_pool->max_size > 0);
    sram_pool->max_size = sram_pool->max_size / 8;

    ret =
        aca_hwa->get_sram_info((te_hwa_aca_t *)aca_hwa, &sram_base, &sram_size);
    TE_ASSERT(TE_SUCCESS == ret);

    /* Check base and size should be alignment aligned */
    TE_ASSERT((UTILS_IS_ALIGNED(sram_base, sram_pool->alignment)) &&
                (UTILS_IS_ALIGNED(sram_size, sram_pool->alignment)));

    sram_pool->sram_base = (void *)(sram_base);
    sram_pool->sram_size = (size_t)(sram_size);
    OSAL_LOG_INFO("SRAM Pool Base: 0x%08x, size: 0x%x, alignment: 0x%x\n",
                  (uint32_t)(uintptr_t)(sram_pool->sram_base),
                  sram_pool->sram_size, sram_pool->alignment);
    /* init lists */
    sqlist_init(&sram_pool->busy_list);
    sqlist_init(&sram_pool->preempt_list);
    sqlist_init(&sram_pool->swapped_list);

    /* create lock */
    if (osal_mutex_create(&sram_pool->lock) != OSAL_SUCCESS) {
        OSAL_LOG_ERR("Create SRAM lock failed!\n");
        ret = TE_ERROR_OOM;
        goto finish;
    }

    /* init pool */
    sqlist_init(&sram_pool->used_blocks);
    sram_pool->freed_size = sram_pool->sram_size;

    /* init hwa ctx */
    sram_pool->hwa_ctx = (void *)(aca_hwa);

    /* init magic */
    sram_pool->magic = ACA_SRAM_POOL_MAGIC;

    ret = TE_SUCCESS;
finish:
    if (TE_SUCCESS != ret) {
        if (sram_pool->lock) {
            osal_mutex_destroy(sram_pool->lock);
        }
        memset(sram_pool, 0, sizeof(aca_sram_pool_t));
    }
    return ret;
}

/**
 * \brief Cleanup the SRAM pool
 */
void aca_drv_cleanup_sram_pool(aca_sram_pool_t *sram_pool)
{
    if (!sram_pool) {
        return;
    }

    if (ACA_SRAM_POOL_MAGIC != sram_pool->magic) {
        OSAL_LOG_ERR("Invalid SRAM pool!\n");
        return;
    }

    /* lock */
    ACA_POOL_LOCK(sram_pool);

    /* check freed size */
    TE_ASSERT_MSG(sram_pool->freed_size == sram_pool->sram_size,
                  "TE SRAM is in used!\n");

    /* check if swapped list is empty */
    TE_ASSERT_MSG(sqlist_is_empty(&sram_pool->swapped_list),
                  "Swapped SRAM is not cleaned!\n");

    /* all the lists are empty */
    TE_ASSERT(sqlist_is_empty(&sram_pool->busy_list));
    TE_ASSERT(sqlist_is_empty(&sram_pool->preempt_list));

    ACA_POOL_UNLOCK(sram_pool);
    /* destroy mutex */
    osal_mutex_destroy(sram_pool->lock);
    memset(sram_pool, 0, sizeof(aca_sram_pool_t));
    return;
}

/**
 * \brief Iterates the SRAM Pool to find the maximum size of available freed
 * block in TE SRAM.
 */
static size_t _sram_get_max_available_block_size(aca_sram_pool_t *sram_pool)
{
    sram_t *first       = NULL;
    sram_t *tmp         = NULL;
    size_t gap_size     = 0;
    size_t max_gap_size = 0;

    first = SQLIST_PEEK_HEAD_CONTAINER(&sram_pool->used_blocks, first, node);
    if (!first) {
        /* used_blocks is empty, maximum size is total SRAM size */
        return sram_pool->sram_size;
    } else {
        /* try the gap between sram base and first block */
        gap_size = (uintptr_t)(first->addr) - (uintptr_t)(sram_pool->sram_base);
        if (gap_size > max_gap_size) {
            max_gap_size = gap_size;
        }
        /* iterate the list to find max gap */
        SQLIST_FOR_EACH_CONTAINER(&sram_pool->used_blocks, first, node)
        {
            /* next head */
            tmp = SQLIST_PEEK_NEXT_CONTAINER(&sram_pool->used_blocks, first,
                                                node);
            TE_ASSERT(first);
            if (!tmp) {
                /* tmp is the tail of used_block */
                gap_size = (uintptr_t)(sram_pool->sram_base) +
                           sram_pool->sram_size -
                           ((uintptr_t)(first->addr) + first->size);
                SRAM_DBG_LOG("SRAM GAP: [0x%08x - 0x%08x] size: 0x%x(%d)\n",
                             (uint32_t)((uintptr_t)(first->addr) + first->size),
                             (uint32_t)((uintptr_t)(sram_pool->sram_base) +
                                        sram_pool->sram_size),
                             gap_size, gap_size);
            } else {
                /* gap size from last used block */
                gap_size = (uintptr_t)(tmp->addr) -
                           ((uintptr_t)(first->addr) + first->size);
                SRAM_DBG_LOG("SRAM GAP: [0x%08x - 0x%08x] size: 0x%x(%d)\n",
                             (uint32_t)((uintptr_t)(first->addr) + first->size),
                             (uint32_t)((uintptr_t)(tmp->addr)), gap_size,
                             gap_size);
            }
            if (gap_size > max_gap_size) {
                max_gap_size = gap_size;
            }
        }
    }

    SRAM_DBG_LOG("SRAM MAX GAP size: 0x%x(%d)\n", max_gap_size, max_gap_size);

    return max_gap_size;
}

/**
 * \brief Tries to allocate on sram_t from TE SRAM. Returns
 * TE_ERROR_NO_SRAM_SPACE if there is no space in TE SRAM.
 */
static int _sram_try_alloc(aca_sram_pool_t *sram_pool,
                           size_t wanted_size,
                           sram_t **ret_out)
{
    sram_t *first            = NULL;
    sram_t *tmp              = NULL;
    sram_t *new              = NULL;
    size_t gap_size          = 0;
    size_t min_fragment_size = 0;

    TE_ASSERT((wanted_size) &&
                UTILS_IS_ALIGNED(wanted_size, sram_pool->alignment));
    /* wanted size is too large */
    if (wanted_size > sram_pool->freed_size) {
        *ret_out = NULL;
        return TE_ERROR_NO_SRAM_SPACE;
    }

    /* create a new sram_t */
    new = osal_malloc(sizeof(sram_t));
    CHECK_COND_RETURN(new, TE_ERROR_OOM);

    first = SQLIST_PEEK_HEAD_CONTAINER(&sram_pool->used_blocks, first, node);
    if (!first) {
        /* used_blocks is empty, allocate from SRAM base */
        new->addr = sram_pool->sram_base;
        new->size = wanted_size;

        /* add to head */
        sqlist_insert_head(&sram_pool->used_blocks, &new->node);
        sram_pool->freed_size -= wanted_size;
        *ret_out = new;
        return TE_SUCCESS;
    } else {
        /* Here we fist iterates the whole lists to find one appropriate gap
         * which has most nearest size with we wanted size. If find one gap size
         * equals to wanted size, directly use it */
        /* 1. find one appropriate gap */
        min_fragment_size = sram_pool->sram_size;

        /* The gap between base and first block */
        gap_size = (uintptr_t)(first->addr) - (uintptr_t)(sram_pool->sram_base);
        if (gap_size == wanted_size) {
            /* This gap size == we wanted size, direct use it */
            new->addr = sram_pool->sram_base;
            new->size = wanted_size;
            sqlist_insert_before(&sram_pool->used_blocks, &first->node,
                                    &new->node);
            sram_pool->freed_size -= wanted_size;
            *ret_out = new;
            return TE_SUCCESS;
        } else if (gap_size > wanted_size) {
            /* recored the fragment size (gap_size - wanted_size) */
            if ((gap_size - wanted_size) < min_fragment_size) {
                min_fragment_size = gap_size - wanted_size;
            }
        } else {
            /* gap size too small, don't use it */
        }

        /* iterate the list to find minimal fragement sized gap */
        SQLIST_FOR_EACH_CONTAINER(&sram_pool->used_blocks, first, node)
        {
            tmp = SQLIST_PEEK_NEXT_CONTAINER(&sram_pool->used_blocks, first,
                                                node);
            TE_ASSERT(first);
            if (!tmp) {
                /* tmp is tail */
                gap_size = (uintptr_t)(sram_pool->sram_base) +
                           sram_pool->sram_size -
                           ((uintptr_t)(first->addr) + first->size);
            } else {
                gap_size = (uintptr_t)(tmp->addr) -
                           ((uintptr_t)(first->addr) + first->size);
            }
            if (gap_size == wanted_size) {
                /* direct use this gap. */
                new->addr = (void *)((uintptr_t)(first->addr) + first->size);
                new->size = wanted_size;
                /* add after first node, to keep used_blocks sorted by address
                 * ascending order */
                sqlist_insert_after(&sram_pool->used_blocks, &first->node,
                                       &new->node);
                sram_pool->freed_size -= wanted_size;
                *ret_out = new;
                return TE_SUCCESS;
            } else if (gap_size > wanted_size) {
                /* record fragment size */
                if ((gap_size - wanted_size) < min_fragment_size) {
                    min_fragment_size = gap_size - wanted_size;
                }
            } else {
                /* gap size too small. */
            }
        }

        /* 2. find the gap with minimal fragment size and use it */
        first =
            SQLIST_PEEK_HEAD_CONTAINER(&sram_pool->used_blocks, first, node);
        TE_ASSERT(first);

        /* try the gap between sram base and the first block */
        gap_size = (uintptr_t)(first->addr) - (uintptr_t)(sram_pool->sram_base);
        if ((gap_size > wanted_size) &&
            ((gap_size - wanted_size) == min_fragment_size)) {
            /* this gap has minimal fragment size, use it */
            new->addr = sram_pool->sram_base;
            new->size = wanted_size;
            sqlist_insert_before(&sram_pool->used_blocks, &first->node,
                                    &new->node);
            sram_pool->freed_size -= wanted_size;
            *ret_out = new;
            return TE_SUCCESS;
        }

        /* iterate the list to find the gap which has minimal fragment size */
        SQLIST_FOR_EACH_CONTAINER(&sram_pool->used_blocks, first, node)
        {
            tmp = SQLIST_PEEK_NEXT_CONTAINER(&sram_pool->used_blocks, first,
                                                node);
            TE_ASSERT(first);
            if (!tmp) {
                /* tmp is tail */
                gap_size = (uintptr_t)(sram_pool->sram_base) +
                           sram_pool->sram_size -
                           ((uintptr_t)(first->addr) + first->size);
            } else {
                gap_size = (uintptr_t)(tmp->addr) -
                           ((uintptr_t)(first->addr) + first->size);
            }
            if ((gap_size > wanted_size) &&
                ((gap_size - wanted_size) == min_fragment_size)) {
                /* this gap has minimal fragment, use it */
                new->addr = (void *)((uintptr_t)(first->addr) + first->size);
                new->size = wanted_size;
                sqlist_insert_after(&sram_pool->used_blocks, &first->node,
                                       &new->node);
                sram_pool->freed_size -= wanted_size;
                *ret_out = new;
                return TE_SUCCESS;
            }
        }
    }

    /* if we go here, means we don't find one availabe sram block from TE SRAM
     * pool */
    OSAL_SAFE_FREE(new);
    *ret_out = NULL;
    return TE_ERROR_NO_SRAM_SPACE;
}

/**
 * \brief returns one sram block to the TE SRAM pool.
 */
static void _sram_free(aca_sram_pool_t *sram_pool, void *addr)
{
    sram_t *tmp = NULL;
    sram_t *tmp2 = NULL;

    TE_ASSERT(UTILS_IS_ALIGNED(addr, sram_pool->alignment));

    /* find the matched node and delet it */
    SQLIST_FOR_EACH_CONTAINER_SAFE(&sram_pool->used_blocks, tmp, tmp2, node)
    {
        if (tmp->addr == addr) {
            sqlist_remove(&tmp->node);
            sram_pool->freed_size += tmp->size;
            osal_free(tmp);
            tmp = NULL;
            break;
        }
    }
    return;
}

/**
 * \brief Tries to change the sram block's size without changing sram address.
 * Considering 3 situations of changing size:
 *
 * 1. if new_size <= current sram block size, shrink the block, which increase
 * the gap between next block.
 * 2. if new size > current sram block size and the gap between next block is
 * large enough to contain new_size, enlarge the block.
 * 3. if new size > current sram block size and the gap between next block is
 * too small to contain new_size, return TE_ERROR_NO_SRAM_SPACE.
 */
static int _sram_try_change_size(aca_sram_pool_t *sram_pool,
                                 size_t new_size,
                                 void *addr)
{
    int ret                     = TE_SUCCESS;
    sram_t *tmp                 = NULL;
    sram_t *sram_ptr            = NULL;
    size_t gap_size             = 0;
    const te_hwa_aca_t *aca_hwa = NULL;

    TE_ASSERT(UTILS_IS_ALIGNED(addr, sram_pool->alignment));
    TE_ASSERT((new_size) && UTILS_IS_ALIGNED(new_size, sram_pool->alignment));

    /* find the machted block */
    SQLIST_FOR_EACH_CONTAINER(&sram_pool->used_blocks, sram_ptr, node)
    {
        if (sram_ptr && (sram_ptr->addr == addr)) {
            break;
        }
    }
    TE_ASSERT(sram_ptr && sram_ptr->addr == addr);

    if (new_size <= sram_ptr->size) {
        /* new size is small, shrink this block */
        sram_pool->freed_size += sram_ptr->size - new_size;
        sram_ptr->size = new_size;
        return TE_SUCCESS;
    } else {
        /* new size is larger, check if we can expand */
        tmp = SQLIST_PEEK_NEXT_CONTAINER(&sram_pool->used_blocks, sram_ptr,
                                            node);
        /* Note: here the gap_size contains current block's size */
        if (!tmp) {
            gap_size = (uintptr_t)(sram_pool->sram_base) +
                       sram_pool->sram_size - (uintptr_t)(sram_ptr->addr);
        } else {
            gap_size = (uintptr_t)(tmp->addr) - (uintptr_t)(sram_ptr->addr);
        }
        if (gap_size >= new_size) {
            /* expand */
#ifdef ACA_SRAM_ALLOC_ZERO
            aca_hwa = (const te_hwa_aca_t *)(sram_pool->hwa_ctx);

#ifdef CFG_TE_DYNCLK_CTL
            ret = aca_hwa->dynamic_clock_ctrl((te_hwa_aca_t *)aca_hwa, true);
            TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

            /* zeroize extended sram */
            ret = aca_hwa->zeroize_sram(
                (te_hwa_aca_t *)aca_hwa,
                (void *)((uintptr_t)(sram_ptr->addr) + sram_ptr->size),
                new_size - sram_ptr->size);
            TE_ASSERT(TE_SUCCESS == (uint32_t)ret);

#ifdef CFG_TE_DYNCLK_CTL
            ret = aca_hwa->dynamic_clock_ctrl((te_hwa_aca_t *)aca_hwa, false);
            TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

#endif
            (void)(ret);
            sram_pool->freed_size -= new_size - sram_ptr->size;
            sram_ptr->size = new_size;
            return TE_SUCCESS;
        }
    }

    /* can't change size */
    return TE_ERROR_NO_SRAM_SPACE;
}

/**
 * \brief changes the sram block's flag, also moves one list to another.
 * with_lock: whether this operation should lock SRAM pool.
 */
static void _sram_block_change_flag(sram_block_t *block,
                                    uint32_t flag,
                                    bool with_lock)
{
    aca_sram_pool_t *sram_pool = ACA_SRAM_GET_POOL(block);

    if (with_lock) {
        ACA_POOL_LOCK(sram_pool);
    }
    block->flags = flag;
    /* also move list */
    sqlist_remove(&block->node);
    if (flag == SRAM_FLAG_BUSY) {
        sqlist_insert_tail(&sram_pool->busy_list, &block->node);
    } else if (flag == SRAM_FLAG_PREEMPT) {
        sqlist_insert_tail(&sram_pool->preempt_list, &block->node);
    } else if (flag == SRAM_FLAG_SWAPPED) {
        sqlist_insert_tail(&sram_pool->swapped_list, &block->node);
    } else {
        TE_ASSERT(0);
    }
    if (with_lock) {
        ACA_POOL_UNLOCK(sram_pool);
    }
}

/**
 * \brief Reserve some space from TE SRAM pool.
 * This may swap out some preempted sram block.
 */
static int _sram_block_reserve_space(aca_sram_pool_t *sram_pool,
                                     size_t required_size)
{
    int ret           = TE_SUCCESS;
    sram_block_t *cur = NULL;
    sram_block_t *tmp = NULL;

    /* check maximum available block size meets our requirement */
    if (_sram_get_max_available_block_size(sram_pool) >= required_size) {
        return TE_SUCCESS;
    }

    /* swap out the preempt list from head. The preempt list is added to tail
     * which makes that the near to header is always the older one */
    SQLIST_FOR_EACH_CONTAINER_SAFE(&sram_pool->preempt_list, cur, tmp, node)
    {
        /* prepare to free current block */
        ACA_SRAM_ASSERT_ON_SRAM_BUF(cur);
        /* free old swapped area because current sram block's size may be
         * changed */
        OSAL_SAFE_FREE(cur->swapped_addr);
        cur->swapped_addr = osal_calloc(1, cur->size);
        CHECK_COND_RETURN(cur->swapped_addr, TE_ERROR_OOM);
        SRAM_DBG_LOG("SRAM Allocate SWAPPED (SWAP OUT): %p\n",
                     (void *)(cur->swapped_addr));

        /* swap out current sram block's data */
        SRAM_DBG_LOG("SRAM SWAP OUT (preempted): 0x%08x --> %p\n",
                     (uint32_t)(uintptr_t)(cur->sram_addr),
                     (void *)(cur->swapped_addr));

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(cur)->dynamic_clock_ctrl(ACA_SRAM_GET_HWA(cur),
                                                        true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        ret = ACA_SRAM_GET_HWA(cur)->swap_sram(
            ACA_SRAM_GET_HWA(cur), cur->sram_addr, cur->swapped_addr, cur->size,
            false);
        TE_ASSERT(TE_SUCCESS == ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(cur)->dynamic_clock_ctrl(ACA_SRAM_GET_HWA(cur),
                                                        false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        /* free current sram block */
        SRAM_DBG_LOG("SRAM Free SRAM (SWAP OUT): 0x%x\n",
                     (uint32_t)(uintptr_t)(cur->sram_addr));
        _sram_free(sram_pool, cur->sram_addr);

        /* clear value */
        cur->sram_addr = NULL;

        /* change flag to swapped */
        _sram_block_change_flag(cur, SRAM_FLAG_SWAPPED, false);

#if ACA_DEBUG
        sram_pool->total_swapped_count++;
        sram_pool->total_swapped_size += cur->size;
#endif

        /* check available block size again */
        if (_sram_get_max_available_block_size(sram_pool) >= required_size) {
            return TE_SUCCESS;
        }
    }

    /* all preempt sram blocks have been freed, we still have no space */
    return TE_ERROR_NO_SRAM_SPACE;
}

/**
 * \brief allocate one sram block. The data area may be in TE SRAM or swapped
 * area.
 *
 * \param[in] sram_pool         The sram pool.
 * \param[in] size              The size of sram block. Accept ACA core
 *                              granularity unaligned size.
 * \param[out] ret_sram_block   The pointer to save new created sram block.
 * \return                      TE_SUCCESS: success.
 */
int aca_sram_alloc_block(aca_sram_pool_t *sram_pool,
                         size_t size,
                         sram_block_t **ret_sram_block)
{
    int ret             = TE_SUCCESS;
    sram_block_t *block = NULL;
    sram_t *sram_ptr    = NULL;
    size_t aligned_size = 0;

    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));
    CHECK_PARAM(ret_sram_block);
    CHECK_PARAM(size && size < sram_pool->sram_size &&
                size <= sram_pool->max_size);

    /* get ACA core granularity aligned size */
    aligned_size = UTILS_ROUND_UP(size, sram_pool->alignment);

    /* create one sram block */
    block = osal_calloc(1, sizeof(sram_block_t));
    CHECK_COND_RETURN(block, TE_ERROR_OOM);

    /* init pool pointer */
    block->pool = (void *)(sram_pool);

    /* try to alloc from TE SRAM */
    ACA_POOL_LOCK(sram_pool);
    ret = _sram_try_alloc(sram_pool, aligned_size, &sram_ptr);
    if ((TE_SUCCESS != (uint32_t)ret) &&
        (TE_ERROR_NO_SRAM_SPACE != (uint32_t)ret)) {
        osal_free(block);
        ACA_POOL_UNLOCK(sram_pool);
        return ret;
    }
    if (TE_SUCCESS == (uint32_t)ret) {
        /* return with TE SRAM address */
        TE_ASSERT(sram_ptr);
        TE_ASSERT(sram_ptr->addr);
        TE_ASSERT(sram_ptr->size == aligned_size);

        block->sram_addr = sram_ptr->addr;
        block->size      = sram_ptr->size;
        block->flags     = SRAM_FLAG_PREEMPT;

        SRAM_DBG_LOG("SRAM Allocate SRAM (preempt): 0x%x\n",
                     (uint32_t)(uintptr_t)(block->sram_addr));

#ifdef ACA_SRAM_ALLOC_ZERO
        /* zeroize sram */

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        ret = ACA_SRAM_GET_HWA(block)->zeroize_sram(
            ACA_SRAM_GET_HWA(block), block->sram_addr, block->size);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
#endif

        /* add to tail of preempt list */
        sqlist_insert_tail(&sram_pool->preempt_list, &block->node);
        ACA_POOL_UNLOCK(sram_pool);

        *ret_sram_block = block;
        return TE_SUCCESS;
    }

    /* unlock, because we will allocate from system heap */
    ACA_POOL_UNLOCK(sram_pool);

    /* allocate from the swapped area */
    block->swapped_addr = (uint32_t *)osal_calloc(1, aligned_size);
    CHECK_COND_RETURN(block->swapped_addr, TE_ERROR_OOM, osal_free(block););
    SRAM_DBG_LOG("SRAM Allocate SWAPPED: %p\n", (void *)(block->swapped_addr));

    block->size = aligned_size;

    /* add to the swapped area. make sure all flags' modification are in lock */
    ACA_POOL_LOCK(sram_pool);
    block->flags = SRAM_FLAG_SWAPPED;
    sqlist_insert_tail(&sram_pool->swapped_list, &block->node);
    ACA_POOL_UNLOCK(sram_pool);

    *ret_sram_block = block;
    return TE_SUCCESS;
}

/**
 * \brief allocate one sram block from TE SRAM block and change it to busy.
 */
int aca_sram_alloc_and_get_block(aca_sram_pool_t *sram_pool,
                                 size_t size,
                                 sram_block_t **ret_sram_block,
                                 void **sram_addr,
                                 size_t *sram_size)
{
    int ret             = TE_SUCCESS;
    sram_block_t *block = NULL;
    sram_t *sram_ptr    = NULL;
    size_t aligned_size = 0;

    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));
    CHECK_PARAM(ret_sram_block);
    CHECK_PARAM(size && size < sram_pool->sram_size &&
                size <= sram_pool->max_size);

    aligned_size = UTILS_ROUND_UP(size, sram_pool->alignment);

    /* create one sram block */
    block = osal_calloc(1, sizeof(sram_block_t));
    CHECK_COND_RETURN(block, TE_ERROR_OOM);

    /* init pool pointer */
    block->pool = (void *)(sram_pool);

    ACA_POOL_LOCK(sram_pool);
    /* first try */
    ret = _sram_try_alloc(sram_pool, aligned_size, &sram_ptr);
    if (TE_ERROR_NO_SRAM_SPACE == (uint32_t)ret) {
        /* if no SRAM space, reserve space */
        CHECK_FUNC(_sram_block_reserve_space(sram_pool, aligned_size),
                   osal_free(block);
                   ACA_POOL_UNLOCK(sram_pool););
        /* reserve success, second try */
        ret = _sram_try_alloc(sram_pool, aligned_size, &sram_ptr);
        TE_ASSERT(TE_ERROR_NO_SRAM_SPACE != (uint32_t)ret);
    }
    if (TE_SUCCESS != (uint32_t)ret) {
        osal_free(block);
        ACA_POOL_UNLOCK(sram_pool);
        return ret;
    } else {
        TE_ASSERT(sram_ptr);
        TE_ASSERT(sram_ptr->addr);
        TE_ASSERT(sram_ptr->size == aligned_size);

        block->sram_addr = sram_ptr->addr;
        block->size      = sram_ptr->size;
        block->flags     = SRAM_FLAG_BUSY;

        SRAM_DBG_LOG("SRAM Allocate SRAM (busy): 0x%x\n",
                     (uint32_t)(uintptr_t)(block->sram_addr));

#ifdef ACA_SRAM_ALLOC_ZERO
        /* zeroize sram */
#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
        ret = ACA_SRAM_GET_HWA(block)->zeroize_sram(
            ACA_SRAM_GET_HWA(block), block->sram_addr, block->size);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
#endif

        /* add to busy list */
        sqlist_insert_tail(&sram_pool->busy_list, &block->node);
        ACA_POOL_UNLOCK(sram_pool);

        *ret_sram_block = block;
        /* return address info */
        if (sram_addr) {
            *sram_addr = block->sram_addr;
        }
        if (sram_size) {
            *sram_size = block->size;
        }
        return TE_SUCCESS;
    }
}

/**
 * \brief free one sram block.
 */
void aca_sram_free_block(sram_block_t *block)
{
    aca_sram_pool_t *sram_pool = NULL;

    if (!((block) && (block->pool))) {
        return;
    }
    sram_pool = ACA_SRAM_GET_POOL(block);
    if (!((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic))) {
        OSAL_LOG_ERR("Invalid SRAM block!\n");
        return;
    }

    /* lock */
    ACA_POOL_LOCK(sram_pool);

    if (block->sram_addr) {
        SRAM_DBG_LOG("SRAM Free SRAM: 0x%x\n",
                     (uint32_t)(uintptr_t)(block->sram_addr));
        _sram_free(sram_pool, block->sram_addr);
        block->sram_addr = NULL;
    }

    /* one sram block can have both sram_addr and swapped_addr */
    if (block->swapped_addr) {
        SRAM_DBG_LOG("SRAM Free SWAPPED: %p\n", (void *)(block->swapped_addr));
        osal_free(block->swapped_addr);
        block->swapped_addr = NULL;
    }

    /* remove from list */
    sqlist_remove(&block->node);

    /* change flag to NULL for safe */
    block->flags = 0;
    /* set size to 0 */
    block->size = 0;

    /* unlock */
    ACA_POOL_UNLOCK(sram_pool);

    /* free the sram block */
    osal_free(block);
    return;
}

/**
 * \brief Write some data to sram.
 *        the data size MUST <= sram block size.
 *        The write data order is reverted from BN LSB to MSB.
 *        If data size < sram block size, 0 is filled.
 * \param[in] block The sram block.
 * \param[in] data  The data pointer.
 * \param[in] size  Data size.
 * \return TE_SUCCESS on success.
 */
int aca_sram_write(sram_block_t *block, const uint8_t *data, size_t size)
{
    int ret                    = TE_SUCCESS;
    aca_sram_pool_t *sram_pool = NULL;
    uint32_t val               = 0;
    size_t i = 0, j = 0;

    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));
    CHECK_PARAM((data) && (size));

    /* check sram block must have enough space to store this data */
    CHECK_PARAM(block->size >= size);

    if (0 == size) {
        return TE_SUCCESS;
    }

    /* lock */
    ACA_POOL_LOCK(sram_pool);
    if (block->flags == SRAM_FLAG_SWAPPED) {
        ACA_SRAM_ASSERT_ON_SWAPPED_BUF(block);
        /* unlock. Writing swapped area doen't need any constrains */
        ACA_POOL_UNLOCK(sram_pool);

        /* write data to swapped area, fill 0 if user data is finished */
        j = size;
        for (i = 0; i < block->size / sizeof(uint32_t); i++) {
            val = (j > 0) ? (((uint32_t)data[j - 1]) << 0) : (0);
            j   = (j > 0) ? (j - 1) : (0);
            val |= (j > 0) ? (((uint32_t)data[j - 1]) << 8) : (0);
            j = (j > 0) ? (j - 1) : (0);
            val |= (j > 0) ? (((uint32_t)data[j - 1]) << 16) : (0);
            j = (j > 0) ? (j - 1) : (0);
            val |= (j > 0) ? (((uint32_t)data[j - 1]) << 24) : (0);
            j                      = (j > 0) ? (j - 1) : (0);
            block->swapped_addr[i] = val;
        }
    } else if (block->flags == SRAM_FLAG_BUSY) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);
        /* unlock, we are already in BUSY state */
        ACA_POOL_UNLOCK(sram_pool);

        /* call hwa to write */
#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
        ret = ACA_SRAM_GET_HWA(block)->write_sram(
            ACA_SRAM_GET_HWA(block), block->sram_addr, block->size, data, size);
        TE_ASSERT(TE_SUCCESS == ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
    } else if (block->flags == SRAM_FLAG_PREEMPT) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);

        /* change flag to BUSY */
        _sram_block_change_flag(block, SRAM_FLAG_BUSY, false);
        /* unlock, we are in BUSY state */
        ACA_POOL_UNLOCK(sram_pool);

        /* call hwa to write */
#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        ret = ACA_SRAM_GET_HWA(block)->write_sram(
            ACA_SRAM_GET_HWA(block), block->sram_addr, block->size, data, size);
        TE_ASSERT(TE_SUCCESS == ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        /* change flag to preempt again */
        _sram_block_change_flag(block, SRAM_FLAG_PREEMPT, true);
    } else {
        TE_ASSERT(0);
    }

    block->cached_bit_len = -1;
    return TE_SUCCESS;
}

/**
 * \brief Zeroize one sram block
 *
 * \param[in] block The sram block to zeroize.
 * \return          TE_SUCCESS: success.
 */
int aca_sram_zeroize(sram_block_t *block)
{
    int ret                    = TE_SUCCESS;
    aca_sram_pool_t *sram_pool = NULL;
    size_t i                   = 0;

    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));

    /* lock */
    ACA_POOL_LOCK(sram_pool);
    if (block->flags == SRAM_FLAG_SWAPPED) {
        ACA_SRAM_ASSERT_ON_SWAPPED_BUF(block);
        /* unlock */
        ACA_POOL_UNLOCK(sram_pool);

        for (i = 0; i < block->size / sizeof(uint32_t); i++) {
            block->swapped_addr[i] = 0;
        }
    } else if (block->flags == SRAM_FLAG_BUSY) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);
        /* unlock */
        ACA_POOL_UNLOCK(sram_pool);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        ret = ACA_SRAM_GET_HWA(block)->zeroize_sram(
            ACA_SRAM_GET_HWA(block), block->sram_addr, block->size);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

    } else if (block->flags == SRAM_FLAG_PREEMPT) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);

        /* change flag to BUSY */
        _sram_block_change_flag(block, SRAM_FLAG_BUSY, false);
        ACA_POOL_UNLOCK(sram_pool);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        ret = ACA_SRAM_GET_HWA(block)->zeroize_sram(
            ACA_SRAM_GET_HWA(block), block->sram_addr, block->size);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        /* change flag to preempt again */
        _sram_block_change_flag(block, SRAM_FLAG_PREEMPT, true);
    } else {
        TE_ASSERT(0);
    }

    block->cached_bit_len = 0;
    return TE_SUCCESS;
}

/**
 * \brief Read partital data from sram to user buffer.
 * The user buffer size MUST <= block->size
 * The read data order is from BN LSB to MSB, if buffer size < block size, the
 * data is read from BN LSB, and some higher bits are not copied.
 * The data write order is reverted.
 *
 * \param[in] block     The sram block to read.
 * \param[out] buf      The buffer to contain data.
 * \param[in] size      The buffer size.
 */
int aca_sram_read(sram_block_t *block, uint8_t *buf, size_t size)
{

    int ret                    = TE_SUCCESS;
    aca_sram_pool_t *sram_pool = NULL;
    uint32_t val               = 0;
    size_t i = 0, j = 0;

    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));
    CHECK_PARAM(buf);
    /* buffer size MUST <= block size */
    CHECK_PARAM(size <= block->size);

    if (0 == size) {
        return TE_SUCCESS;
    }
    /* lock */
    ACA_POOL_LOCK(sram_pool);
    if (block->flags == SRAM_FLAG_SWAPPED) {
        ACA_SRAM_ASSERT_ON_SWAPPED_BUF(block);
        /* unlock */
        ACA_POOL_UNLOCK(sram_pool);

        /* read data from swapped area */
        j = size;
        for (i = 0; i < block->size / sizeof(uint32_t); i++) {
            val        = block->swapped_addr[i];
            buf[j - 1] = (val >> 0) & 0xFF;
            j--;
            if (j <= 0)
                break;
            buf[j - 1] = (val >> 8) & 0xFF;
            j--;
            if (j <= 0)
                break;
            buf[j - 1] = (val >> 16) & 0xFF;
            j--;
            if (j <= 0)
                break;
            buf[j - 1] = (val >> 24) & 0xFF;
            j--;
            if (j <= 0)
                break;
        }
    } else if (block->flags == SRAM_FLAG_BUSY) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);
        /* unlock */
        ACA_POOL_UNLOCK(sram_pool);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        ret = ACA_SRAM_GET_HWA(block)->read_sram(
            ACA_SRAM_GET_HWA(block), block->sram_addr, block->size, buf, size);
        TE_ASSERT(TE_SUCCESS == ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
    } else if (block->flags == SRAM_FLAG_PREEMPT) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);
        /* change flag to BUSY */
        _sram_block_change_flag(block, SRAM_FLAG_BUSY, false);
        /* unlock */
        ACA_POOL_UNLOCK(sram_pool);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        ret = ACA_SRAM_GET_HWA(block)->read_sram(
            ACA_SRAM_GET_HWA(block), block->sram_addr, block->size, buf, size);
        TE_ASSERT(TE_SUCCESS == ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        _sram_block_change_flag(block, SRAM_FLAG_PREEMPT, true);
    } else {
        TE_ASSERT(0);
    }

    return TE_SUCCESS;
}

/**
 * \brief Lock the sram block to TE SRAM and change the sram block to BUSY
 * state.
 *
 * \param[in] block         The sram block.
 * \param[out] sram_addr    Returns the sram address in TE SRAM.
 * \param[out] sram_size    Returns the sram size.
 */
int aca_sram_get(sram_block_t *block, void **sram_addr, size_t *sram_size)
{
    int ret                    = TE_SUCCESS;
    aca_sram_pool_t *sram_pool = NULL;
    sram_t *sram_ptr           = NULL;

    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));

    /* lock */
    ACA_POOL_LOCK(sram_pool);
    if (block->flags == SRAM_FLAG_SWAPPED) {
        /* current sram block is in swapped area, swap in */
        ACA_SRAM_ASSERT_ON_SWAPPED_BUF(block);

        /* frist try alloc space from SRAM */
        ret = _sram_try_alloc(sram_pool, block->size, &sram_ptr);
        if (TE_ERROR_NO_SRAM_SPACE == (uint32_t)ret) {
            /* no space, try reserved space */
            ret = _sram_block_reserve_space(sram_pool, block->size);
            if (TE_SUCCESS != (uint32_t)ret) {
                OSAL_LOG_DEBUG("SRAM reserve size: %d failed, retry!\n", block->size);
                ACA_POOL_UNLOCK(sram_pool);
                return ret;
            }
            /* second try */
            ret = _sram_try_alloc(sram_pool, block->size, &sram_ptr);
            TE_ASSERT(TE_ERROR_NO_SRAM_SPACE != (uint32_t)ret);
        }
        if (TE_SUCCESS != (uint32_t)ret) {
            OSAL_LOG_ERR("No space in TE SRAM for size: %d\n", block->size);
            ACA_POOL_UNLOCK(sram_pool);
            return ret;
        }
        TE_ASSERT(sram_ptr);
        TE_ASSERT(sram_ptr->addr);
        TE_ASSERT(sram_ptr->size == block->size);

        block->sram_addr = sram_ptr->addr;

        SRAM_DBG_LOG("SRAM Allocate SRAM (SWAP IN): 0x%x\n",
                     (uint32_t)(uintptr_t)(block->sram_addr));

        SRAM_DBG_LOG("SRAM SWAP IN: %p --> 0x%08x\n",
                     (void *)(block->swapped_addr),
                     (uint32_t)(uintptr_t)(block->sram_addr));

        /* write data from swapped area to TE sram */

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        ret = ACA_SRAM_GET_HWA(block)->swap_sram(
            ACA_SRAM_GET_HWA(block), block->sram_addr, block->swapped_addr,
            block->size, true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
        /* change flag to BUSY */
        _sram_block_change_flag(block, SRAM_FLAG_BUSY, false);
        if (sram_addr) {
            *sram_addr = block->sram_addr;
        }
        if (sram_size) {
            *sram_size = block->size;
        }
        ACA_POOL_UNLOCK(sram_pool);
    } else if (block->flags == SRAM_FLAG_BUSY) {
        /* current sram block already in BUSY state, do nothing. */
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);
        if (sram_addr) {
            *sram_addr = block->sram_addr;
        }
        if (sram_size) {
            *sram_size = block->size;
        }
        /* unlock */
        ACA_POOL_UNLOCK(sram_pool);
    } else if (block->flags == SRAM_FLAG_PREEMPT) {
        /* current sram block flag is PREEMPT, just change flag */
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);

        /* change flag to BUSY */
        _sram_block_change_flag(block, SRAM_FLAG_BUSY, false);

        if (sram_addr) {
            *sram_addr = block->sram_addr;
        }
        if (sram_size) {
            *sram_size = block->size;
        }
        /* unlock */
        ACA_POOL_UNLOCK(sram_pool);
    } else {
        TE_ASSERT(0);
    }

    return TE_SUCCESS;
}

/**
 * \brief Unlock one sram block and change flag to PREEMPT.
 */
int aca_sram_put(sram_block_t *block)
{
    aca_sram_pool_t *sram_pool = NULL;

    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));

    /* lock */
    ACA_POOL_LOCK(sram_pool);
    if (block->flags == SRAM_FLAG_SWAPPED) {
        ACA_SRAM_ASSERT_ON_SWAPPED_BUF(block);

        ACA_POOL_UNLOCK(sram_pool);
    } else if (block->flags == SRAM_FLAG_BUSY) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);

        /* change flag to preempt */
        _sram_block_change_flag(block, SRAM_FLAG_PREEMPT, false);

        /* unlock */
        ACA_POOL_UNLOCK(sram_pool);
    } else if (block->flags == SRAM_FLAG_PREEMPT) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);

        /* unlock */
        ACA_POOL_UNLOCK(sram_pool);
    } else {
        TE_ASSERT(0);
    }

    return TE_SUCCESS;
}

/**
 * \brief swap out all sram blocks.
 */
static int _sram_swap_all_blocks(aca_sram_pool_t *sram_pool, bool with_lock)
{
    int ret = TE_SUCCESS;

    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));

    if (with_lock) {
        ACA_POOL_LOCK(sram_pool);
    }
    /* reserved maximum sram size from TE SRAM pool */
    ret = _sram_block_reserve_space(sram_pool, sram_pool->sram_size);
    if (TE_SUCCESS != ret) {
        ret = TE_ERROR_BUSY;
        goto finish;
    }

    /* check freed size */
    TE_ASSERT(sram_pool->freed_size == sram_pool->sram_size);

    /* Check busy/preempt list is empty */
    TE_ASSERT(sqlist_is_empty(&sram_pool->busy_list));
    TE_ASSERT(sqlist_is_empty(&sram_pool->preempt_list));

finish:
    if (with_lock) {
        ACA_POOL_UNLOCK(sram_pool);
    }

    return TE_SUCCESS;
}

int aca_sram_swap_all_blocks(aca_sram_pool_t *sram_pool)
{
    return _sram_swap_all_blocks(sram_pool, true);
}

int aca_sram_swap_all_blocks_nolock(aca_sram_pool_t *sram_pool)
{
    return _sram_swap_all_blocks(sram_pool, false);
}

/**
 * \brief return current sram block's size.
 */
int aca_sram_get_size(sram_block_t *block, size_t *size)
{
    aca_sram_pool_t *sram_pool = NULL;

    CHECK_PARAM(size);
    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));

    ACA_POOL_LOCK(sram_pool);
    TE_ASSERT(UTILS_IS_ALIGNED(block->size, sram_pool->alignment));
    *size = block->size;
    ACA_POOL_UNLOCK(sram_pool);

    return TE_SUCCESS;
}

/**
 * \brief return current sram block's bit length
 */
int aca_sram_get_bit_len(sram_block_t *block, size_t *bit_len)
{
    int ret                    = TE_SUCCESS;
    aca_sram_pool_t *sram_pool = NULL;
    size_t i, j = 0;
    uint32_t val                        = 0;
    bool should_restore_flag_to_preempt = false;

    CHECK_PARAM(bit_len);
    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));

#if 0
    /* TODO */
    if (block->cached_bit_len > 0) {
        *bit_len = block->cached_bit_len;
        return TE_SUCCESS;
    }
#endif

    /* lock */
    ACA_POOL_LOCK(sram_pool);

    if (block->flags == SRAM_FLAG_SWAPPED) {
        /* data in swapped area is little endian */
        ACA_SRAM_ASSERT_ON_SWAPPED_BUF(block);
        ACA_POOL_UNLOCK(sram_pool);

        for (i = (block->size / 4); i > 0; i--) {
            val = block->swapped_addr[i - 1];
            if (val != 0) {
                break;
            }
        }
        if (i == 0) {
            block->cached_bit_len = 0;
        } else {
            j                     = 32 - _sram_clz(val);
            block->cached_bit_len = (((i - 1) * 32) + j);
        }
    } else if ((block->flags == SRAM_FLAG_BUSY) ||
               (block->flags == SRAM_FLAG_PREEMPT)) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);

        if (block->flags == SRAM_FLAG_BUSY) {
            /* unlock */
            ACA_POOL_UNLOCK(sram_pool);
        } else {
            /* change flag to BUSY */
            _sram_block_change_flag(block, SRAM_FLAG_BUSY, false);
            /* unlock */
            ACA_POOL_UNLOCK(sram_pool);
            should_restore_flag_to_preempt = true;
        }

        /* data in TE SRAM is little endian */
#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
        for (i = (block->size / 4); i > 0; i--) {
            /* read one word without changing endian */
            ret = ACA_SRAM_GET_HWA(block)->swap_sram(
                ACA_SRAM_GET_HWA(block),
                (void *)((uintptr_t)(block->sram_addr) + (i - 1) * 4), &val,
                sizeof(val), false);
            TE_ASSERT(TE_SUCCESS == ret);
            if (val != 0) {
                break;
            }
        }

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
        if (i == 0) {
            block->cached_bit_len = 0;
        } else {
            j                     = 32 - _sram_clz(val);
            block->cached_bit_len = (((i - 1) * 32) + j);
        }

        if (should_restore_flag_to_preempt) {
            _sram_block_change_flag(block, SRAM_FLAG_PREEMPT, true);
        }
    } else {
        TE_ASSERT(0);
    }

    TE_ASSERT(block->cached_bit_len >= 0);
    *bit_len = block->cached_bit_len;
    return TE_SUCCESS;
}

/**
 * \brief set one bit in sram block.
 * the bit_num MUST within sram block size.
 *
 * \param[in] block     The sram block to set.
 * \param[in] bit_num   The 0 indexed bit number.
 * \param[in] val       The bit value, 0 or 1.
 */
int aca_sram_set_bit(sram_block_t *block, size_t bit_num, int32_t val)
{
    int ret                    = TE_SUCCESS;
    aca_sram_pool_t *sram_pool = NULL;
    size_t word_offset = 0, bit_offset = 0;
    bool should_restore_flag_to_preempt = false;
    uint32_t tmp_val                    = 0;

    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));
    CHECK_PARAM(bit_num < block->size * 8);
    CHECK_PARAM((val == 0) || (val == 1));

    word_offset = bit_num / 32;
    bit_offset  = bit_num % 32;

    /* lock */
    ACA_POOL_LOCK(sram_pool);
    if (block->flags == SRAM_FLAG_SWAPPED) {
        ACA_SRAM_ASSERT_ON_SWAPPED_BUF(block);
        ACA_POOL_UNLOCK(sram_pool);

        tmp_val = block->swapped_addr[word_offset];
        if (val) {
            tmp_val |= (0x1 << bit_offset);
        } else {
            tmp_val &= (~(0x1 << bit_offset));
        }
        block->swapped_addr[word_offset] = tmp_val;
    } else if ((block->flags == SRAM_FLAG_BUSY) ||
               (block->flags == SRAM_FLAG_PREEMPT)) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);

        if (block->flags == SRAM_FLAG_BUSY) {
            /* unlock */
            ACA_POOL_UNLOCK(sram_pool);
        } else {
            /* change flag to BUSY */
            _sram_block_change_flag(block, SRAM_FLAG_BUSY, false);
            /* unlock */
            ACA_POOL_UNLOCK(sram_pool);
            should_restore_flag_to_preempt = true;
        }

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
        /* read one word without changing endian */
        ret = ACA_SRAM_GET_HWA(block)->swap_sram(
            ACA_SRAM_GET_HWA(block),
            (void *)((uintptr_t)(block->sram_addr) + word_offset * 4), &tmp_val,
            sizeof(tmp_val), false);
        TE_ASSERT(TE_SUCCESS == ret);

        if (val) {
            tmp_val |= (0x1 << bit_offset);
        } else {
            tmp_val &= (~(0x1 << bit_offset));
        }
        /* write new word */
        ret = ACA_SRAM_GET_HWA(block)->swap_sram(
            ACA_SRAM_GET_HWA(block),
            (void *)((uintptr_t)(block->sram_addr) + word_offset * 4), &tmp_val,
            sizeof(tmp_val), true);
        TE_ASSERT(TE_SUCCESS == ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        if (should_restore_flag_to_preempt) {
            _sram_block_change_flag(block, SRAM_FLAG_PREEMPT, true);
        }
    } else {
        TE_ASSERT(0);
    }

    block->cached_bit_len = -1;
    return TE_SUCCESS;
}

/**
 * \brief get one bit in sram block.
 * the bit_num MUST within sram block size.
 *
 * \param[in] block     The sram block to get.
 * \param[in] bit_num   The 0 indexed bit number.
 * \param[in] bit_val   The pointer to save bit value.
 */
int aca_sram_get_bit(sram_block_t *block, size_t bit_num, int *bit_val)
{
    int ret                    = TE_SUCCESS;
    aca_sram_pool_t *sram_pool = NULL;
    size_t word_offset, bit_offset = 0;
    uint32_t val                        = 0;
    bool should_restore_flag_to_preempt = false;

    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));
    CHECK_PARAM(bit_val);
    CHECK_PARAM(bit_num < block->size * 8);

    word_offset = bit_num / 32;
    bit_offset  = bit_num % 32;

    /* lock */
    ACA_POOL_LOCK(sram_pool);
    if (block->flags == SRAM_FLAG_SWAPPED) {
        ACA_SRAM_ASSERT_ON_SWAPPED_BUF(block);
        ACA_POOL_UNLOCK(sram_pool);

        val = block->swapped_addr[word_offset];
        val &= (0x1 << bit_offset);
        *bit_val = ((val) ? (1) : (0));
    } else if ((block->flags == SRAM_FLAG_BUSY) ||
               (block->flags == SRAM_FLAG_PREEMPT)) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);

        if (block->flags == SRAM_FLAG_BUSY) {
            /* unlock */
            ACA_POOL_UNLOCK(sram_pool);
        } else {
            /* change flag to BUSY */
            _sram_block_change_flag(block, SRAM_FLAG_BUSY, false);
            /* unlock */
            ACA_POOL_UNLOCK(sram_pool);
            should_restore_flag_to_preempt = true;
        }

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
        /* read one word */
        ret = ACA_SRAM_GET_HWA(block)->swap_sram(
            ACA_SRAM_GET_HWA(block),
            (void *)((uintptr_t)(block->sram_addr) + word_offset * 4), &val,
            sizeof(val), false);
        TE_ASSERT(TE_SUCCESS == ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
        val &= (0x1 << bit_offset);
        *bit_val = ((val) ? (1) : (0));

        if (should_restore_flag_to_preempt) {
            _sram_block_change_flag(block, SRAM_FLAG_PREEMPT, true);
        }
    } else {
        TE_ASSERT(0);
    }

    return TE_SUCCESS;
}

/**
 * \brief swap out one sram block
 */
int aca_sram_swap_out(sram_block_t *block)
{
    int ret                    = TE_SUCCESS;
    aca_sram_pool_t *sram_pool = NULL;

    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));

    /* lock */
    ACA_POOL_LOCK(sram_pool);

    /* swap preemt and busy */
    if ((block->flags == SRAM_FLAG_PREEMPT) ||
        (block->flags == SRAM_FLAG_BUSY)) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);
        if (block->flags == SRAM_FLAG_PREEMPT) {
            _sram_block_change_flag(block, SRAM_FLAG_BUSY, false);
        }

#if ACA_DEBUG
        sram_pool->total_swapped_count++;
        sram_pool->total_swapped_size += block->size;
#endif
        ACA_POOL_UNLOCK(sram_pool);

        OSAL_SAFE_FREE(block->swapped_addr);
        block->swapped_addr = osal_calloc(1, block->size);
        CHECK_COND_RETURN(block->swapped_addr, TE_ERROR_OOM);
        SRAM_DBG_LOG("SRAM Allocate SWAPPED (SWAP OUT): %p\n",
                     (void *)(block->swapped_addr));

        SRAM_DBG_LOG("SRAM SWAP OUT (voluntary): 0x%08x --> %p\n",
                     (uint32_t)(uintptr_t)(block->sram_addr),
                     (void *)(block->swapped_addr));

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
        /* swap out */
        ret = ACA_SRAM_GET_HWA(block)->swap_sram(
            ACA_SRAM_GET_HWA(block), block->sram_addr, block->swapped_addr,
            block->size, false);
        TE_ASSERT(TE_SUCCESS == ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
        /* free SRAM */
        SRAM_DBG_LOG("SRAM Free SRAM (SWAP OUT): 0x%x\n",
                     (uint32_t)(uintptr_t)(block->sram_addr));
        ACA_POOL_LOCK(sram_pool);
        _sram_free(sram_pool, block->sram_addr);
        ACA_POOL_UNLOCK(sram_pool);
        /* clear value */
        block->sram_addr = NULL;
        /* change flag */
        _sram_block_change_flag(block, SRAM_FLAG_SWAPPED, true);
    } else if (block->flags == SRAM_FLAG_SWAPPED) {
        ACA_SRAM_ASSERT_ON_SWAPPED_BUF(block);
        ACA_POOL_UNLOCK(sram_pool);
    } else {
        TE_ASSERT(0);
    }

    return TE_SUCCESS;
}

/**
 * Note:
 * 1. new_size may NOT be sram aligned
 * 2. shrink for new_size == 0
 **/

/**
 * \brief Try to change the size of sram block without losing data.
 * The new_size may NOT aca core granularity size aligned.
 * if new_size == 0, will shrink the MSB 0 data.
 *
 * \param[in] block     The sram block to change size.
 * \param[in] new_size  New size to change.
 */
int aca_sram_try_change_size(sram_block_t *block, size_t new_size)
{
    int ret                             = TE_SUCCESS;
    aca_sram_pool_t *sram_pool          = NULL;
    uint32_t *new_swapped_area          = NULL;
    size_t new_aligned_size             = 0;
    size_t stored_size                  = 0;
    bool should_restore_flag_to_preempt = false;

    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));
    CHECK_PARAM(new_size < sram_pool->sram_size &&
                new_size <= sram_pool->max_size);

    /* calculate the ACA core aligned new size */
    if (new_size < block->size) {

        /* get bit length to calc stored size */
        ret = aca_sram_get_bit_len(block, &stored_size);
        CHECK_RET_RETURN;
        stored_size =
            (UTILS_ROUND_UP(stored_size, sram_pool->alignment * 8)) / 8;

        /* not lose data */
        if (new_size < stored_size) {
            new_aligned_size = stored_size;
        } else {
            new_aligned_size = UTILS_ROUND_UP(new_size, sram_pool->alignment);
        }
    } else {
        new_aligned_size = UTILS_ROUND_UP(new_size, sram_pool->alignment);
    }

    if (new_aligned_size == block->size) {
        /* do nothing */
        return TE_SUCCESS;
    }

    if (0 == new_aligned_size) {
        /* we need to keep this sram block, keep at least one block */
        new_aligned_size = sram_pool->alignment;
    }

    ACA_POOL_LOCK(sram_pool);

    /* try to change size from sram pool directly */
    if ((block->flags == SRAM_FLAG_BUSY) ||
        (block->flags == SRAM_FLAG_PREEMPT)) {
        ACA_SRAM_ASSERT_ON_SRAM_BUF(block);
        if (block->flags == SRAM_FLAG_PREEMPT) {
            /* change flag to BUSY */
            _sram_block_change_flag(block, SRAM_FLAG_BUSY, false);
            should_restore_flag_to_preempt = true;
        }

        ret = _sram_try_change_size(sram_pool, new_aligned_size,
                                    block->sram_addr);
        if (should_restore_flag_to_preempt) {
            _sram_block_change_flag(block, SRAM_FLAG_PREEMPT, false);
        }
        if (TE_SUCCESS == ret) {
            block->size = new_aligned_size;
            ACA_POOL_UNLOCK(sram_pool);
            return TE_SUCCESS;
        }
    }
    ACA_POOL_UNLOCK(sram_pool);

    /* otherwise, swapped out and change size */
    CHECK_FUNC(aca_sram_swap_out(block));

    new_swapped_area = osal_calloc(1, new_aligned_size);
    CHECK_COND_RETURN(new_swapped_area, TE_ERROR_OOM);

    if (new_aligned_size > block->size) {
        memcpy(new_swapped_area, block->swapped_addr, block->size);
        /* set high bits to 0 */
        memset((uint8_t *)new_swapped_area + block->size, 0,
               new_aligned_size - block->size);
    } else {
        memcpy(new_swapped_area, block->swapped_addr, new_aligned_size);
    }

    OSAL_SAFE_FREE(block->swapped_addr);
    SRAM_DBG_LOG("SRAM Change SWAPPED: %p --> %p\n",
                 (void *)(block->swapped_addr), (void *)(new_swapped_area));
    block->swapped_addr = new_swapped_area;
    block->size         = new_aligned_size;

    return TE_SUCCESS;
}

/**
 * \brief Reset the sram block to new size, and zeroize data.
 * The new_size may NOT be ACA core granularity aligned.
 * The new_size MUST > 0.
 * This equals to try_chnage_size + zeroize.
 *
 * \param[in] block     The sram block to reset.
 * \param[in] new_size  The new size.
 */
int aca_sram_reset(sram_block_t *block, size_t new_size)
{
    int ret                    = TE_SUCCESS;
    aca_sram_pool_t *sram_pool = NULL;
    size_t new_aligned_size    = 0;
    sram_t *sram_ptr           = NULL;

    CHECK_PARAM((block) && (block->pool));
    sram_pool = ACA_SRAM_GET_POOL(block);
    CHECK_PARAM((sram_pool) && (ACA_SRAM_POOL_MAGIC == sram_pool->magic));
    CHECK_PARAM(new_size && (new_size < sram_pool->sram_size) &&
                (new_size <= sram_pool->max_size));

    new_aligned_size = UTILS_ROUND_UP(new_size, sram_pool->alignment);

    if (new_aligned_size == block->size) {
        return aca_sram_zeroize(block);
    }

    /* lock */
    ACA_POOL_LOCK(sram_pool);

    if (block->sram_addr) {
        SRAM_DBG_LOG("SRAM Free SRAM: 0x%x\n",
                     (uint32_t)(uintptr_t)(block->sram_addr));
        _sram_free(sram_pool, block->sram_addr);
        block->sram_addr = NULL;
    }

    /* one sram block can have both sram_addr and swapped_addr */
    if (block->swapped_addr) {
        SRAM_DBG_LOG("SRAM Free SWAPPED: %p\n", (void *)(block->swapped_addr));
        osal_free(block->swapped_addr);
        block->swapped_addr = NULL;
    }

    /* remove from list */
    sqlist_remove(&block->node);

    /* change flag to NULL for safe */
    block->flags = 0;

    /* allocate new size */
    ret = _sram_try_alloc(sram_pool, new_aligned_size, &sram_ptr);
    if ((TE_SUCCESS != (uint32_t)ret) &&
        (TE_ERROR_NO_SRAM_SPACE != (uint32_t)ret)) {
        ACA_POOL_UNLOCK(sram_pool);
        goto finish;
    }
    if (TE_SUCCESS == (uint32_t)ret) {
        TE_ASSERT(sram_ptr);
        TE_ASSERT(sram_ptr->addr);
        TE_ASSERT(sram_ptr->size == new_aligned_size);

        block->sram_addr = sram_ptr->addr;
        block->size      = sram_ptr->size;
        block->flags     = SRAM_FLAG_PREEMPT;

        SRAM_DBG_LOG("SRAM Allocate SRAM (preempt): 0x%x\n",
                     (uint32_t)(uintptr_t)(block->sram_addr));
#ifdef ACA_SRAM_ALLOC_ZERO
#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), true);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

        /* zeroize sram */
        ret = ACA_SRAM_GET_HWA(block)->zeroize_sram(
            ACA_SRAM_GET_HWA(block), block->sram_addr, block->size);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);

#ifdef CFG_TE_DYNCLK_CTL
        ret = ACA_SRAM_GET_HWA(block)->dynamic_clock_ctrl(
            ACA_SRAM_GET_HWA(block), false);
        TE_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
#endif

        /* add to tail of preempt list */
        sqlist_insert_tail(&sram_pool->preempt_list, &block->node);
        ACA_POOL_UNLOCK(sram_pool);

        ret = TE_SUCCESS;
        goto finish;
    }

    /* unlock */
    ACA_POOL_UNLOCK(sram_pool);

    /* For the swapped area, calloc to change size */
    block->swapped_addr = (uint32_t *)osal_calloc(1, new_aligned_size);
    CHECK_COND_RETURN(block->swapped_addr, TE_ERROR_OOM, osal_free(block););
    SRAM_DBG_LOG("SRAM Allocate SWAPPED: %p\n", (void *)(block->swapped_addr));

    block->size = new_aligned_size;

    /* add to the swapped area. make sure all flags' modification are in
     * lock */
    ACA_POOL_LOCK(sram_pool);
    block->flags = SRAM_FLAG_SWAPPED;
    sqlist_insert_tail(&sram_pool->swapped_list, &block->node);
    ACA_POOL_UNLOCK(sram_pool);

finish:
    return TE_SUCCESS;
}
