//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <driver/te_drv_aca.h>
#include <hwa/te_hwa_aca.h>
#include "drv_aca_internal.h"

/**
 * \brief This file contains the operations of GR(general registers) in TE.
 *
 * TODO: Implement lazy GR_info free logic.
 *
 * All the GRs are managed by one GR pool. The GR pool is one array which
 * contains each GR's usage state.
 *
 * When one OP_CTX need to call TE, it allocates one gr_id from GR pool by
 * calling aca_gr_alloc function. The allocated gr_id is also configured to
 * some SRAM address and N, P, T0, T1 special usage if have.
 * When the OP_CTX finished one operation, it should free the gr_id to GR
 * pool by calling aca_gr_free function.
 *
 * Variables:
 * aca_gr_pool_t *gr_pool: The GR pool pointer.
 * gr_usage_hint_t usage:  The GR's usage. One GR MUST have one usage when
 * allocating from GR pool. Details about usage see gr_usage_hint_t.
 */

/* GR pool magic */
#define ACA_GR_POOL_MAGIC (0x4167724d) /* AgrM */

/* The minimal required GR numbers of ACA engine */
#define ACA_MIN_GR_NUM (32)

/* each GR's state in GR array. */
enum {
    GR_IDLE = 0, /* GR is idle, can be allocated */
    GR_USED = 1, /* GR is used, can't be allocated */
};

/**
 * Predefined GR ids.
 * These GRs are "exclusive" when using ACA engine, so we reserved them.
 * So, the GR pool's managment is only for general used GRs.
 *
 * 0: reserved. Any OP_CTX should NOT have 0 gr_id when doing operation.
 * 1: N
 * 2: P
 * 3: T0
 * 4: T1
 * 5 - 31: general.
 */
enum {
    GR_ID_RSVD   = 0,
    GR_ID_N      = 1,
    GR_ID_P      = 2,
    GR_ID_T0     = 3,
    GR_ID_T1     = 4,
    GR_ID_COMMON = 5,
};

/**
 * \brief Initialize one GR pool
 */
int aca_drv_init_gr_pool(aca_gr_pool_t *gr_pool, const te_hwa_aca_t *aca_hwa)
{
    int ret   = TE_SUCCESS;
    int32_t i = 0;

    CHECK_PARAM(gr_pool);
    CHECK_PARAM(aca_hwa);

    gr_pool->gr_number = aca_hwa->get_gr_num((te_hwa_aca_t *)aca_hwa);
    TE_ASSERT(gr_pool->gr_number >= ACA_MIN_GR_NUM);

    /* The SRAM pool alignment is TE ACA core's granularity */
    gr_pool->alignment = aca_hwa->get_core_granularity((te_hwa_aca_t *)aca_hwa);
    TE_ASSERT(gr_pool->alignment > 0);
    gr_pool->alignment = gr_pool->alignment / 8;

    gr_pool->gr_array =
        (uint8_t *)osal_malloc(gr_pool->gr_number * sizeof(uint8_t));
    CHECK_COND_GO(gr_pool->gr_array, TE_ERROR_OOM);

    /* init gr array to GR_IDLE */
    for (i = 0; i < gr_pool->gr_number; i++) {
        gr_pool->gr_array[i] = GR_IDLE;
    }

    /* create lock */
    if (osal_mutex_create(&gr_pool->lock) != OSAL_SUCCESS) {
        OSAL_LOG_ERR("Create GR lock failed!\n");
        ret = TE_ERROR_OOM;
        goto finish;
    }

    /* init hwa ctx */
    gr_pool->hwa_ctx = (void *)(aca_hwa);

    /* init magic */
    gr_pool->magic = ACA_GR_POOL_MAGIC;

    ret = TE_SUCCESS;
finish:
    if (TE_SUCCESS != ret) {
        if (gr_pool->lock) {
            osal_mutex_destroy(gr_pool->lock);
        }
        OSAL_SAFE_FREE(gr_pool->gr_array);
        memset(gr_pool, 0, sizeof(aca_gr_pool_t));
    }
    return ret;
}

/**
 * \brief Cleanup the GR pool
 */
void aca_drv_cleanup_gr_pool(aca_gr_pool_t *gr_pool)
{
    size_t i = 0;

    if (!gr_pool) {
        return;
    }

    if (ACA_GR_POOL_MAGIC != gr_pool->magic) {
        OSAL_LOG_ERR("Invalid GR pool!\n");
        return;
    }
    /* lock */
    ACA_POOL_LOCK(gr_pool);

    for (i = 0; i < (size_t)(gr_pool->gr_number); i++) {
        TE_ASSERT_MSG(gr_pool->gr_array[i] == GR_IDLE, "GR %d is busy!\n", i);
    }

    /* free gr array within lock */
    OSAL_SAFE_FREE(gr_pool->gr_array);
    ACA_POOL_UNLOCK(gr_pool);
    /* destroy mutex */
    osal_mutex_destroy(gr_pool->lock);
    memset(gr_pool, 0, sizeof(aca_gr_pool_t));
    return;
}

/**
 * \brief Allocate one gr_id from gr_pool
 *
 * \param[in] gr_pool       The GR pool pointer.
 * \param[in] usage         The GR's usage.
 * \param[in] sram_addr     The SRAM address to be binded to this GR.
 * \param[in] sram_size     The SRAM size to be binded to this GR.
 *                          For GRs who's usage is N or IN, the GR block
 *                          number is configured to this size.
 *                          For other GRs, the GR block number is set to 0.
 * \param[out] gr_id_ret    The returned GR id.
 * \return                  TE_ERROR_NO_AVAIL_GR: where there is no avalible GR
 *                          TE_SUCCESS: allocate success.
 */
int aca_gr_alloc(aca_gr_pool_t *gr_pool,
                 gr_usage_hint_t usage,
                 void *sram_addr,
                 size_t sram_size,
                 int32_t *gr_id_ret)
{
    int ret                     = TE_SUCCESS;
    int32_t gr_id               = -1;
    const te_hwa_aca_t *aca_hwa = NULL;
    uint32_t addr               = 0;
    uint32_t abc_blks           = 0;
    int32_t i                   = 0;

    CHECK_PARAM((gr_pool) && (ACA_GR_POOL_MAGIC == gr_pool->magic));
    CHECK_PARAM(ACA_GR_IS_VALID_USAGE(usage));
    CHECK_PARAM((sram_size) &&
                (UTILS_IS_ALIGNED(sram_size, gr_pool->alignment)));
    CHECK_PARAM((sram_addr) &&
                (UTILS_IS_ALIGNED(sram_addr, gr_pool->alignment)));

    ACA_POOL_LOCK(gr_pool);

    if (usage == GR_USAGE_N) {
        gr_id = GR_ID_N;
    } else if (usage == GR_USAGE_P) {
        gr_id = GR_ID_P;
    } else if (usage == GR_USAGE_T0) {
        gr_id = GR_ID_T0;
    } else if (usage == GR_USAGE_T1) {
        gr_id = GR_ID_T1;
    } else {
        gr_id = -1;
    }

    if (gr_id != -1) {
        if (gr_pool->gr_array[gr_id] != GR_IDLE) {
            ret = TE_ERROR_NO_AVAIL_GR;
            goto finish;
        }
    } else {
        for (i = GR_ID_COMMON; i < gr_pool->gr_number; i++) {
            if (gr_pool->gr_array[i] == GR_IDLE) {
                gr_id = i;
                break;
            }
        }
        if (gr_id == -1) {
            ret = TE_ERROR_NO_AVAIL_GR;
            goto finish;
        }
    }

    TE_ASSERT(gr_id != -1);

    /* change gr to used */
    gr_pool->gr_array[gr_id] = GR_USED;

    /* configure GR's address and abn block number */
    aca_hwa = (const te_hwa_aca_t *)(gr_pool->hwa_ctx);

    addr = (uint32_t)(uintptr_t)(sram_addr);
    if ((usage == GR_USAGE_IN) || (usage == GR_USAGE_N)) {
        /* block number is available for N A B C */
        abc_blks = sram_size / gr_pool->alignment;
    } else {
        abc_blks = 0;
    }

    ret = aca_hwa->config_gr_sram_addr((te_hwa_aca_t *)aca_hwa, gr_id, addr,
                                       abc_blks);
    TE_ASSERT(TE_SUCCESS == ret);

    if (usage == GR_USAGE_N) {
        ret = aca_hwa->config_gr_for_n((te_hwa_aca_t *)aca_hwa, gr_id);
        TE_ASSERT(TE_SUCCESS == ret);
    } else if (usage == GR_USAGE_P) {
        ret = aca_hwa->config_gr_for_p((te_hwa_aca_t *)aca_hwa, gr_id);
        TE_ASSERT(TE_SUCCESS == ret);
    } else if (usage == GR_USAGE_T0) {
        ret = aca_hwa->config_gr_for_t0((te_hwa_aca_t *)aca_hwa, gr_id);
        TE_ASSERT(TE_SUCCESS == ret);
    } else if (usage == GR_USAGE_T1) {
        ret = aca_hwa->config_gr_for_t1((te_hwa_aca_t *)aca_hwa, gr_id);
        TE_ASSERT(TE_SUCCESS == ret);
    } else {
        /* do nothing */
    }

    ACA_POOL_UNLOCK(gr_pool);
    GR_DBG_LOG(
        "[GR] Allocate GR: %d, usage: %s\n", gr_id,
        (usage == GR_USAGE_NULL)
            ? ("NULL")
            : ((usage == GR_USAGE_IN)
                   ? ("IN")
                   : ((usage == GR_USAGE_OUT)
                          ? ("OUT")
                          : ((usage == GR_USAGE_INOUT)
                                 ? ("INOUT")
                                 : ((usage == GR_USAGE_N)
                                        ? ("N")
                                        : ((usage == GR_USAGE_P)
                                               ? ("P")
                                               : ((usage == GR_USAGE_T0)
                                                      ? ("T0")
                                                      : ((usage == GR_USAGE_T1)
                                                             ? ("T1")
                                                             : ("Invali"
                                                                "d")))))))));
    *gr_id_ret = gr_id;

finish:
    return ret;
}

/**
 * \brief Free one gr_id to GR pool
 *
 * \param[in] gr_pool   The gr pool.
 * \param[in] gr_id     The gr_id to be returned.
 */
void aca_gr_free(aca_gr_pool_t *gr_pool, int32_t gr_id)
{
    if (!((gr_pool) && (ACA_GR_POOL_MAGIC == gr_pool->magic))) {
        OSAL_LOG_ERR("Invalid GR Pool!\n");
        return;
    }

    if (gr_id == -1) {
        return;
    }

    if ((gr_id <= GR_ID_RSVD) || (gr_id >= gr_pool->gr_number)) {
        OSAL_LOG_ERR("Invalid GR ID: %d\n", gr_id);
        return;
    }
    ACA_POOL_LOCK(gr_pool);
    /* change gr to idle */
    gr_pool->gr_array[gr_id] = GR_IDLE;
    ACA_POOL_UNLOCK(gr_pool);
    GR_DBG_LOG("[GR] Free GR: %d\n", gr_id);
    return;
}

/**
 * \brief Checks whether there is GR used by one OP_CTX.
 *        This is used in suspend/resume to check whether ACA driver can be
 *        suspend.
 *
 * \param[in] gr_pool The GR pool.
 * \return true: Some GRs are not returned to the GR pool.
 * \return false: All GRs are returned to the GR pool.
 */
bool aca_gr_is_busy(aca_gr_pool_t *gr_pool)
{
    int32_t i = 0;
    for (i = 0; i < (int32_t)(gr_pool->gr_number); i++) {
        if (gr_pool->gr_array[i] != GR_IDLE) {
            OSAL_LOG_ERR("GR %d is busy!\n", i);
            return true;
        }
    }
    return false;
}
