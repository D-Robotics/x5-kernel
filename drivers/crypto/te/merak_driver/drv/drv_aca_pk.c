//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <driver/te_drv_aca.h>
#include <hwa/te_hwa_aca.h>
#include "drv_aca_internal.h"

/**
 * \brief This function is used for ACA wrapper layer, supplies:
 * 1. ASYNC operation and ASYNC thread enabled.
 * 2. Wrapper layer global mutex lock.
 * The wrapper layer doesn't have any details bout ACA driver and ACA driver
 * context, so support wrapper lock in driver space.
 */

#ifdef CFG_TE_ASYNC_EN

/* async thread flags */
enum {
    ASYNC_THREAD_FLAG_IDLE        = 0U,
    ASYNC_THREAD_FLAG_RUNNING     = 1U,
    ASYNC_THREAD_FLAG_SHOULD_STOP = 2U,
    ASYNC_THREAD_FLAG_STOPPED     = 3U,
};

/* ACA async thread */
static osal_err_t _aca_pk_async_worker(void *arg)
{
    int ret                            = TE_SUCCESS;
    aca_pk_t *pk                       = (aca_pk_t *)(arg);
    sqlist_t *node                     = NULL;
    aca_async_req_header_t *req_header = NULL;

    TE_ASSERT(arg);

    OSAL_LOG_DEBUG("ACA Async Thread Start...\n");

    /* change flag */
    osal_atomic_store(&pk->async_thread_flag, ASYNC_THREAD_FLAG_RUNNING);

    while (true) {
        /* sleep on competion */
        osal_completion_wait(&pk->new_req);

        /* check if we should stop */
        if (osal_atomic_load(&pk->async_thread_flag) ==
            ASYNC_THREAD_FLAG_SHOULD_STOP) {
            break;
        }

    __try_again:
        /* get one node from request list */
        osal_mutex_lock(pk->async_lock);
        node = sqlist_get(&pk->async_list);
        osal_mutex_unlock(pk->async_lock);
        if (!node) {
            continue;
        }
        req_header = SQLIST_CONTAINER(node, req_header, node);
        TE_ASSERT(req_header);

        if ((!req_header->aca_pk_cb) || (!req_header->hdl) ||
            (!req_header->base.completion)) {
            continue;
        }
        /* execute the requet */
        OSAL_LOG_DEBUG("ACA Async execute function: %p\n",
                       req_header->aca_pk_cb);
        ret = req_header->aca_pk_cb((void *)(req_header));
        if (TE_SUCCESS != ret) {
            OSAL_LOG_ERR("ACA Execute async requst failed!\n");
        }

        /* call user callback */
        OSAL_LOG_DEBUG("ACA Async Complete: %p\n", req_header->base.completion);
        req_header->base.completion((struct te_async_request *)(req_header),
                                    ret);
        req_header = NULL;
        node       = NULL;
        goto __try_again;
    }

    /* change flag to stopped */
    osal_atomic_store(&pk->async_thread_flag, ASYNC_THREAD_FLAG_STOPPED);

    OSAL_LOG_DEBUG("ACA Async Thread Stop...\n");

    return TE_SUCCESS;
}
#endif /* CFG_TE_ASYNC_EN */

/**
 * \brief Initialize PK.
 */
int aca_pk_init(aca_pk_t *pk)
{
    CHECK_PARAM(pk);

    /* check size */
    TE_ASSERT(sizeof(aca_async_req_header_t) ==
              sizeof(te_async_request_t) +
              sizeof(pk_request_internal_data_t));

    /* init pk lock */
    if (osal_mutex_create(&pk->lock) != OSAL_SUCCESS) {
        OSAL_LOG_ERR("Create ACA PK lock failed!\n");
        return TE_ERROR_OOM;
    }

#ifdef CFG_TE_ASYNC_EN
    /* init async lock */
    if (osal_mutex_create(&pk->async_lock) != OSAL_SUCCESS) {
        OSAL_LOG_ERR("Create ACA PK Async lock failed!\n");
        osal_mutex_destroy(pk->lock);
        return TE_ERROR_OOM;
    }
    /* init async link list */
    sqlist_init(&pk->async_list);
    /* init async completion */
    if (osal_completion_init(&pk->new_req) != OSAL_SUCCESS) {
        osal_mutex_destroy(pk->async_lock);
        osal_mutex_destroy(pk->lock);
        return TE_ERROR_OOM;
    }

    /* set thread flag to IDLE */
    osal_atomic_store(&pk->async_thread_flag, ASYNC_THREAD_FLAG_IDLE);

    /* create thread */
    if (osal_thread_create(&pk->async_thread, _aca_pk_async_worker,
                           (void *)(pk)) != OSAL_SUCCESS) {
        osal_completion_destroy(&pk->new_req);
        osal_mutex_destroy(pk->async_lock);
        osal_mutex_destroy(pk->lock);
        return TE_ERROR_GENERIC;
    }
#endif /* CFG_TE_ASYNC_EN */

    return TE_SUCCESS;
}

/**
 * \brief Cleanup PK.
 */
void aca_pk_cleanup(aca_pk_t *pk)
{
#ifdef CFG_TE_ASYNC_EN
    int i = 0;
#endif
    if (!pk) {
        return;
    }

#ifdef CFG_TE_ASYNC_EN
    /* stop thread */
    do {
        osal_atomic_store(&pk->async_thread_flag,
                          ASYNC_THREAD_FLAG_SHOULD_STOP);
        osal_completion_signal(&pk->new_req);
        osal_sleep_ms(10);
        i++;
        if (i > 100) {
            OSAL_LOG_ERR("TE ACA Async thread not stopped!\n");
            i = -1;
            break;
        }
    } while (osal_atomic_load(&pk->async_thread_flag) !=
             ASYNC_THREAD_FLAG_STOPPED);
    if (i != -1) {
        osal_wait_thread_done(pk->async_thread);
    }
    osal_thread_destroy(pk->async_thread);
    osal_completion_destroy(&pk->new_req);
    osal_mutex_destroy(pk->async_lock);
#endif /* CFG_TE_ASYNC_EN */

    osal_mutex_destroy(pk->lock);
    return;
}

/* ACA lock used in wrapper */
int te_aca_lock(const te_crypt_drv_t *drv)
{
    te_aca_drv_t *aca_drv    = (te_aca_drv_t *)drv;
    aca_priv_drv_t *priv_drv = NULL;

    CHECK_PARAM(aca_drv && (ACA_DRV_MAGIC == aca_drv->magic));
    priv_drv = (aca_priv_drv_t *)(aca_drv->priv_drv);

    osal_mutex_lock(priv_drv->pk.lock);
    return TE_SUCCESS;
}

/* ACA unlock used in wrapper */
int te_aca_unlock(const te_crypt_drv_t *drv)
{
    te_aca_drv_t *aca_drv    = (te_aca_drv_t *)drv;
    aca_priv_drv_t *priv_drv = NULL;

    CHECK_PARAM(aca_drv && (ACA_DRV_MAGIC == aca_drv->magic));
    priv_drv = (aca_priv_drv_t *)(aca_drv->priv_drv);

    osal_mutex_unlock(priv_drv->pk.lock);
    return TE_SUCCESS;
}

/* ACA submit request in wrapper async mode */
int te_aca_submit_req(void *req)
{
#ifdef CFG_TE_ASYNC_EN
    int ret                            = TE_SUCCESS;
    te_aca_drv_t *aca_drv              = NULL;
    aca_priv_drv_t *priv_drv           = NULL;
    aca_async_req_header_t *req_header = (aca_async_req_header_t *)req;

    CHECK_PARAM(req && (req_header->base.completion) &&
                (req_header->aca_pk_cb) && (req_header->hdl));

    aca_drv = (te_aca_drv_t *)te_drv_get(req_header->hdl, TE_DRV_TYPE_ACA);
    CHECK_PARAM(aca_drv && (ACA_DRV_MAGIC == aca_drv->magic));
    CHECK_FUNC(te_drv_put(req_header->hdl, TE_DRV_TYPE_ACA));

    priv_drv = (aca_priv_drv_t *)(aca_drv->priv_drv);

    /* The req is from pk wrapper, use it directly */
    osal_mutex_lock(priv_drv->pk.async_lock);
    sqlist_insert_tail(&priv_drv->pk.async_list, &req_header->node);
    osal_mutex_unlock(priv_drv->pk.async_lock);

    /* signal request */
    osal_completion_signal(&(priv_drv->pk.new_req));

    return TE_SUCCESS;
#else
    (void)(req);
    return TE_ERROR_NOT_SUPPORTED;
#endif /* CFG_TE_ASYNC_EN */
}
