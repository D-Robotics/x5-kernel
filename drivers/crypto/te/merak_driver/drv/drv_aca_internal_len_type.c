//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <driver/te_drv_aca.h>
#include <hwa/te_hwa_aca.h>
#include "drv_aca_internal.h"

/**
 * \brief This file contains the operations of LengthTypes in ACA engine.
 *
 * All the LengthTypes are managed by one LengthType pool. The LengthType pool
 * is one array which contains each LengthType's usage state. In most cases, the
 * ACA operation only needs one LengthType.
 *
 * When several OP_CTXs want to call ACA engine to do one operation, they MUST
 * allocate one or more len_type_id(s) from the LengthType pool, and the
 * len_type_id(s) is configured to current operation length in bits.
 *
 * When one operation is finished, the len_type_id(s) MUST be returned to the
 * length type pool
 *
 * Variables:
 * aca_len_type_pool_t *len_type_pool: the LengthType pool.
 */

/* The LengthType pool's magic  */
#define ACA_LEN_TYPE_POOL_MAGIC (0x416c744d) /* AltM */

/* The minimal required number of len_type_ids */
#define ACA_LEN_TYPE_MIN_NUM (2)

/* each len_type_id's state in LengthType pool. */
enum {
    LEN_TYPE_IDLE = 0,
    LEN_TYPE_USED = 1,
};

/**
 * \brief Initialize one LengthType pool
 */
int aca_drv_init_len_type_pool(aca_len_type_pool_t *len_type_pool,
                               const te_hwa_aca_t *aca_hwa)
{
    int ret  = TE_SUCCESS;
    size_t i = 0;

    CHECK_PARAM(len_type_pool);
    CHECK_PARAM(aca_hwa);

    len_type_pool->len_type_number =
        aca_hwa->get_len_type_num((te_hwa_aca_t *)aca_hwa);
    TE_ASSERT(len_type_pool->len_type_number >= ACA_LEN_TYPE_MIN_NUM);

    len_type_pool->len_type_array = (uint8_t *)osal_malloc(
        len_type_pool->len_type_number * sizeof(uint8_t));
    CHECK_COND_RETURN(len_type_pool->len_type_array, TE_ERROR_OOM);

    /* init gr array to IDLE */
    for (i = 0; i < (size_t)(len_type_pool->len_type_number); i++) {
        len_type_pool->len_type_array[i] = LEN_TYPE_IDLE;
    }

    /* create lock */
    if (osal_mutex_create(&len_type_pool->lock) != OSAL_SUCCESS) {
        OSAL_LOG_ERR("Create LEN_TYPE lock failed!\n");
        ret = TE_ERROR_OOM;
        goto finish;
    }

    /* init hwa ctx */
    len_type_pool->hwa_ctx = (void *)(aca_hwa);

    /* init magic */
    len_type_pool->magic = ACA_LEN_TYPE_POOL_MAGIC;

    ret = TE_SUCCESS;
finish:
    if (TE_SUCCESS != ret) {
        if (len_type_pool->lock) {
            osal_mutex_destroy(len_type_pool->lock);
        }
        OSAL_SAFE_FREE(len_type_pool->len_type_array);
        memset(len_type_pool, 0, sizeof(aca_len_type_pool_t));
    }
    return ret;
}

/**
 * \brief Cleanup the LengthType pool
 */
void aca_drv_cleanup_len_type_pool(aca_len_type_pool_t *len_type_pool)
{
    size_t i = 0;

    if (!len_type_pool) {
        return;
    }
    if (ACA_LEN_TYPE_POOL_MAGIC != len_type_pool->magic) {
        OSAL_LOG_ERR("Invalid length type pool!\n");
        return;
    }

    /* lock */
    ACA_POOL_LOCK(len_type_pool);

    for (i = 0; i < (size_t)(len_type_pool->len_type_number); i++) {
        TE_ASSERT_MSG(len_type_pool->len_type_array[i] == LEN_TYPE_IDLE,
                      "Length Type %d is busy!\n",
                      i);
    }

    /* free length type array within lock */
    OSAL_SAFE_FREE(len_type_pool->len_type_array);

    ACA_POOL_UNLOCK(len_type_pool);
    /* destroy mutex */
    osal_mutex_destroy(len_type_pool->lock);
    memset(len_type_pool, 0, sizeof(aca_len_type_pool_t));
    return;
}

/**
 * \brief Allocate one len_type_id from the LengthType pool.
 *
 * \param[in] len_type_pool     The LengthType pool.
 * \param[in] op_bit_len        Current operation length in bits.
 * \return      >= 0: one len_type_id.
 *              TE_ERROR_OP_TOO_LONG: current operation length exceed the
 * maximum of ACA engine's operation.
 *              TE_ERROR_NO_AVAIL_LEN_TYPE: there is no available len_type_id in
 * the pool.
 */
int aca_len_type_alloc(aca_len_type_pool_t *len_type_pool, size_t op_bit_len)
{
    int ret                     = TE_SUCCESS;
    int32_t idx                 = 0;
    const te_hwa_aca_t *aca_hwa = NULL;
    size_t max_op_len           = 0;
    size_t aca_granule_bits     = 0;

    CHECK_PARAM((len_type_pool) &&
                (ACA_LEN_TYPE_POOL_MAGIC == len_type_pool->magic));
    aca_hwa = (const te_hwa_aca_t *)(len_type_pool->hwa_ctx);

    ACA_POOL_LOCK(len_type_pool);

    aca_granule_bits =
        aca_hwa->get_core_granularity((struct te_hwa_aca *)aca_hwa);
    TE_ASSERT(aca_granule_bits > 0);
    max_op_len = aca_hwa->get_core_max_op_len((struct te_hwa_aca *)aca_hwa);
    TE_ASSERT(max_op_len > 0);
    if (op_bit_len > max_op_len) {
        ACA_POOL_UNLOCK(len_type_pool);
        return TE_ERROR_OP_TOO_LONG;
    }

    /* skip check bit length, because bitlen may be real bit length */
    for (idx = 0; idx < len_type_pool->len_type_number; idx++) {
        if (len_type_pool->len_type_array[idx] == LEN_TYPE_IDLE) {
            len_type_pool->len_type_array[idx] = LEN_TYPE_USED;
            break;
        }
    }

    if (idx == len_type_pool->len_type_number) {
        ACA_POOL_UNLOCK(len_type_pool);
        return TE_ERROR_NO_AVAIL_LEN_TYPE;
    } else {
#if 0
        /* config length */
        OSAL_LOG_DEBUG("Config OP Length Type: %d, Size: 0x%x(%d)\n",
                       idx,
                       op_bit_len,
                       op_bit_len);
#endif
        ret = aca_hwa->config_len_type(
            (te_hwa_aca_t *)aca_hwa, (int8_t)(idx), (uint32_t)(op_bit_len));
        TE_ASSERT(TE_SUCCESS == ret);

        ACA_POOL_UNLOCK(len_type_pool);
        return idx;
    }
}

/**
 * \brief Return one len_type_id to the LengthType Pool.
 *
 * \param[in] len_type_pool The LengthType pool.
 * \param[in] len_type_id   The len_typ_id to be returned.
 */
void aca_len_type_free(aca_len_type_pool_t *len_type_pool, int32_t len_type_id)
{
    if ((!len_type_pool) || (ACA_LEN_TYPE_POOL_MAGIC != len_type_pool->magic)) {
        OSAL_LOG_ERR("Invalid Length Type Pool!\n");
        return;
    }

    ACA_POOL_LOCK(len_type_pool);

    if (!((len_type_id >= 0) &&
          (len_type_id < len_type_pool->len_type_number))) {
        OSAL_LOG_ERR("Invalid Length Type ID: %d\n", len_type_id);
        return;
    }
    if (len_type_pool->len_type_array[len_type_id] == LEN_TYPE_USED) {
        len_type_pool->len_type_array[len_type_id] = LEN_TYPE_IDLE;
    }

    ACA_POOL_UNLOCK(len_type_pool);
}

/**
 * \brief Checks whether there is len_type_id used by one operation.
 *        This is used in suspend/resume to check whether ACA driver can be
 *        suspend.
 *
 * \param[in] len_type_pool The LengthType pool.
 * \return true: Some len_type_id(s) are not returned to the LengthType pool.
 * \return false: All len_type_ids are returned to the LengthType pool.
 */
bool aca_len_type_is_busy(aca_len_type_pool_t *len_type_pool)
{
    int32_t i = 0;

    for (i = 0; i < (int32_t)(len_type_pool->len_type_number); i++) {
        if (len_type_pool->len_type_array[i] != LEN_TYPE_IDLE) {
            OSAL_LOG_ERR("Length Type %d is busy!\n", i);
            return true;
        }
    }
    return false;
}
