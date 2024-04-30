//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_hash.h>


#define __HASH_OUT__    goto __out__

#define __HASH_CHECK_CONDITION__(_ret_)                                        \
        do {                                                                   \
            if( TE_SUCCESS != (_ret_) ){                                       \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 __HASH_OUT__;                                                 \
            }                                                                  \
        } while (0);

#define __HASH_ALERT__(_ret_, _msg_)                                           \
        do {                                                                   \
            if( TE_SUCCESS != (_ret_) ){                                       \
                OSAL_LOG_ERR("%s +%d ret->%X %s\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_),                                       \
                                (_msg_));                                      \
            }                                                                  \
        } while (0);

#define __HASH_VERIFY_PARAMS__(_param_)                                        \
        do                                                                     \
        {                                                                      \
            if(NULL == (_param_)){                                             \
                ret = TE_ERROR_BAD_PARAMS;                                     \
                __HASH_OUT__;                                                  \
            }                                                                  \
        } while (0)

#define  _HASH_VERIFY_MAIN_ALGO(_alg_) ( (TE_MAIN_ALGO_MD5) == (_alg_) ? true  \
        : (TE_MAIN_ALGO_SHA1) == (_alg_) ? true                                \
        : (TE_MAIN_ALGO_SHA224) == (_alg_) ? true                              \
        : (TE_MAIN_ALGO_SHA256) == (_alg_) ? true                              \
        : (TE_MAIN_ALGO_SHA384) == (_alg_) ? true                              \
        : (TE_MAIN_ALGO_SHA512) == (_alg_) ? true                              \
        : (TE_MAIN_ALGO_SM3) == (_alg_) ? true                                 \
        : false )

#define _HASH_GET_DIGEST_LEN_(_alg_) (                                         \
          (TE_MAIN_ALGO_MD5) == (_alg_) ? TE_MD5_HASH_SIZE                     \
        : (TE_MAIN_ALGO_SHA1) == (_alg_) ? TE_SHA1_HASH_SIZE                   \
        : (TE_MAIN_ALGO_SHA224) == (_alg_) ? TE_SHA224_HASH_SIZE               \
        : (TE_MAIN_ALGO_SHA256) == (_alg_) ? TE_SHA256_HASH_SIZE               \
        : (TE_MAIN_ALGO_SHA384) == (_alg_) ? TE_SHA384_HASH_SIZE               \
        : (TE_MAIN_ALGO_SHA512) == (_alg_) ? TE_SHA512_HASH_SIZE               \
        : (TE_MAIN_ALGO_SM3) == (_alg_) ? TE_SM3_HASH_SIZE                     \
        : TE_MAX_HASH_SIZE)

#ifdef CFG_TE_ASYNC_EN
typedef struct adgst_ctx {
    te_dgst_ctx_t ctx;
    te_dgst_request_t *req;
    te_hash_request_t *start;
    te_hash_request_t *update;
    te_hash_request_t *finish;
} adgst_ctx_t;
#endif

int te_dgst_init( te_dgst_ctx_t *ctx, te_drv_handle hdl, te_algo_t alg )
{
    int ret = TE_SUCCESS;
    te_hash_drv_t *hdrv = NULL;

    __HASH_VERIFY_PARAMS__(ctx);
    if (!_HASH_VERIFY_MAIN_ALGO(TE_ALG_GET_MAIN_ALG(alg))){
        ret = TE_ERROR_INVAL_ALG;
        __HASH_OUT__;
    }

    hdrv = (te_hash_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_HASH);
    ret = te_hash_alloc_ctx(hdrv, alg, 0, &ctx->crypt);
    te_drv_put(hdl, TE_DRV_TYPE_HASH);
__out__:
    return ret;
}

int te_dgst_free( te_dgst_ctx_t *ctx )
{
    int ret = TE_SUCCESS;

    __HASH_VERIFY_PARAMS__(ctx);
    __HASH_VERIFY_PARAMS__(ctx->crypt);

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HASH_OUT__;
    }
    switch (ret) {
    case TE_DRV_HASH_STATE_START:
    case TE_DRV_HASH_STATE_UPDATE:
    case TE_DRV_HASH_STATE_LAST:
        ret = te_hash_finish(ctx->crypt, NULL, 0);
        __HASH_ALERT__(ret, "te_hash_finish error!");
        break;
    default:
        ret = TE_SUCCESS;
        break;
    }

    ret = te_hash_free_ctx(ctx->crypt);
    __HASH_ALERT__(ret, "te_hash_free_ctx error!");
    ctx->crypt = NULL;

__out__:
    return ret;
}

int te_dgst_start( te_dgst_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    __HASH_VERIFY_PARAMS__(ctx);
    __HASH_VERIFY_PARAMS__(ctx->crypt);

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HASH_OUT__;
    }
    switch (ret) {
    case TE_DRV_HASH_STATE_START:
        ret = TE_SUCCESS;
        __HASH_OUT__;
    case TE_DRV_HASH_STATE_UPDATE:
    case TE_DRV_HASH_STATE_LAST:
        ret = te_hash_finish(ctx->crypt, NULL, 0);
        __HASH_ALERT__(ret, "te_hash_finish error!");
        __HASH_CHECK_CONDITION__(ret);
        break;
    default:
        break;
    }
    ret = te_hash_start(ctx->crypt, NULL);
__out__:
    return ret;
}

int te_dgst_update( te_dgst_ctx_t *ctx, const uint8_t *in, size_t len )
{
    int ret = TE_SUCCESS;

    __HASH_VERIFY_PARAMS__(ctx);
    __HASH_VERIFY_PARAMS__(ctx->crypt);

    if ((NULL == in) && (len != 0)) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __HASH_OUT__;
    }

    /*
     * te_dgst_update() on a already finished dgst ctx is allowed,
     */
    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HASH_OUT__;
    }

    if ( TE_DRV_HASH_STATE_INIT == ret ) {
        ret = te_hash_start(ctx->crypt, NULL);
        if (ret != TE_SUCCESS) {
            __HASH_OUT__;
        }
    }

    ret = te_hash_update(ctx->crypt, in, len);
__out__:
    return ret;
}

int te_dgst_uplist( te_dgst_ctx_t *ctx, te_memlist_t *in )
{
    int ret = TE_SUCCESS;

    __HASH_VERIFY_PARAMS__(ctx);
    __HASH_VERIFY_PARAMS__(ctx->crypt);

    if ((NULL == in) || ((NULL == in->ents) && (0 != in->nent))) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __HASH_OUT__;
    }

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HASH_OUT__;
    }

    if ( TE_DRV_HASH_STATE_INIT == ret ) {
        ret = te_hash_start(ctx->crypt, NULL);
        if (ret != TE_SUCCESS) {
            __HASH_OUT__;
        }
    }

    ret = te_hash_uplist(ctx->crypt, in);
__out__:
    return ret;
}

int te_dgst_finish( te_dgst_ctx_t *ctx, uint8_t *hash )
{
    int ret = TE_SUCCESS;

    __HASH_VERIFY_PARAMS__(ctx);
    __HASH_VERIFY_PARAMS__(ctx->crypt);

    if (NULL == hash) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __HASH_OUT__;
    }

    /*
     * te_dgst_finish() on a already finished dgst ctx is allowed,
     * where hash("") is expected.
     */
    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HASH_OUT__;
    }

    switch (ret) {
    case TE_DRV_HASH_STATE_INIT:
        ret = te_hash_start(ctx->crypt, NULL);
        __HASH_ALERT__(ret, "te_hash_start error!");
        __HASH_CHECK_CONDITION__(ret);
        /* FALLTHRU */
		fallthrough;
    case TE_DRV_HASH_STATE_START:
        ret = te_hash_update(ctx->crypt, NULL, 0);
        __HASH_ALERT__(ret, "te_hash_update error!");
        __HASH_CHECK_CONDITION__(ret);
        break;
    default:
        break;
    }

    ret = te_hash_finish(ctx->crypt,
                        hash,
                _HASH_GET_DIGEST_LEN_(TE_ALG_GET_MAIN_ALG(ctx->crypt->alg)));
__out__:
    return ret;
}

int te_dgst_reset( te_dgst_ctx_t *ctx )
{
    int ret = TE_SUCCESS;

    __HASH_VERIFY_PARAMS__(ctx);
    __HASH_VERIFY_PARAMS__(ctx->crypt);

    ret = te_hash_reset(ctx->crypt);
__out__:
    return ret;
}

int te_dgst_clone( te_dgst_ctx_t *src, te_dgst_ctx_t *dst )
{
    int ret = TE_SUCCESS;
    te_hash_drv_t *hdrv = NULL;

    __HASH_VERIFY_PARAMS__(src);
    __HASH_VERIFY_PARAMS__(dst);

    /*
     * The case of Mbedtls clone before start case, no crypt exist
     */
    if (NULL == src->crypt) {
        ret = TE_SUCCESS;
        __HASH_OUT__;
    }

    /*
     * free the dst ctx if it points to a valid dgst ctx already
     */
    if (NULL != dst->crypt) {
        ret = te_dgst_free(dst);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_dgst_free error %x\n", ret);
            return ret;
        }
    }

    hdrv = (te_hash_drv_t *)src->crypt->drv;
    ret = te_hash_alloc_ctx(hdrv, src->crypt->alg, 0, &dst->crypt);
    __HASH_CHECK_CONDITION__(ret);
    ret = te_hash_clone(src->crypt, dst->crypt);
    if (TE_SUCCESS != ret) {
        te_hash_free_ctx(dst->crypt);
        dst->crypt = NULL;
    }

__out__:
    return ret;
}

int te_dgst_export( te_dgst_ctx_t *ctx,
                    void *out,
                    uint32_t *olen )
{
    int ret = TE_SUCCESS;

    __HASH_VERIFY_PARAMS__(ctx);
    __HASH_VERIFY_PARAMS__(ctx->crypt);

    ret = te_hash_export(ctx->crypt, out, olen);
__out__:
    return ret;
}

int te_dgst_import( te_dgst_ctx_t *ctx,
                    const void *in,
                    uint32_t ilen )
{
    int ret = TE_SUCCESS;

    __HASH_VERIFY_PARAMS__(ctx);
    __HASH_VERIFY_PARAMS__(ctx->crypt);

    ret = te_hash_import(ctx->crypt, in, ilen);
__out__:
    return ret;
}

int te_dgst( te_drv_handle hdl, te_algo_t alg,
             const uint8_t *in, size_t len,
             uint8_t *hash )
{
    int ret = TE_SUCCESS;
    te_dgst_ctx_t dgst_ctx = {0};

    __HASH_VERIFY_PARAMS__(hash);

    if ((NULL == in) && (0 != len)) {
        ret = TE_ERROR_BAD_PARAMS;
        __HASH_OUT__;
    }
    ret = te_dgst_init(&dgst_ctx, hdl, alg);
    __HASH_CHECK_CONDITION__(ret);
    ret = te_dgst_start(&dgst_ctx);
    __HASH_CHECK_CONDITION__(ret);
    ret = te_dgst_update(&dgst_ctx, in, len);
    __HASH_CHECK_CONDITION__(ret);
    ret = te_dgst_finish(&dgst_ctx, hash);
__out__:
    te_dgst_free(&dgst_ctx);
    return ret;
}

#ifdef CFG_TE_ASYNC_EN
static void dgst_async_generic_done( te_async_request_t *r, int err )
{
    te_hash_request_t *hreq = (te_hash_request_t *)r;
    te_dgst_request_t *req = (te_dgst_request_t *)r->data;

    osal_free( hreq );
    req->res = err;
    req->base.completion( &req->base, req->res );
    return;
}

int te_dgst_astart( te_dgst_ctx_t *ctx, te_dgst_request_t *req )
{
    int ret = TE_ERROR_GENERIC;
    te_hash_request_t *hreq = NULL;

    if( !ctx || !req ) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }

    hreq = (te_hash_request_t *)osal_calloc( 1, sizeof(te_hash_request_t) );
    if ( hreq == NULL ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    hreq->base.data = (void *)req;
    hreq->base.completion = dgst_async_generic_done;

    ret = te_hash_astart( ctx->crypt, hreq );
    if ( ret != TE_SUCCESS ) {
        goto err2;
    }

    return ret;

err2:
    osal_free( hreq );
err1:
    return ret;
}

int te_dgst_aupdate( te_dgst_ctx_t *ctx, te_dgst_request_t *req )
{
    int ret = TE_SUCCESS;
    te_hash_request_t *hreq = NULL;

    if( !ctx || !req ) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }

    if ( !req->up.in.ents && req->up.in.nent != 0 ) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }

    hreq = (te_hash_request_t *)osal_calloc( 1, sizeof(te_hash_request_t) );
    if ( hreq == NULL ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    hreq->base.data = (void *)req;
    hreq->base.completion = dgst_async_generic_done;
    hreq->up.flags = HASH_FLAGS_LIST;
    hreq->up.lst.in.ents = req->up.in.ents;
    hreq->up.lst.in.nent = req->up.in.nent;

    ret = te_hash_aupdate( ctx->crypt, hreq );
    if ( ret != TE_SUCCESS ) {
        goto err2;
    }

    return ret;

err2:
    osal_free( hreq );
err1:
    return ret;
}

int te_dgst_afinish( te_dgst_ctx_t *ctx, te_dgst_request_t *req )
{
    int ret = TE_SUCCESS;
    te_hash_request_t *hreq = NULL;

    __HASH_VERIFY_PARAMS__(ctx);
    __HASH_VERIFY_PARAMS__(req);
    __HASH_VERIFY_PARAMS__(ctx->crypt);
    __HASH_VERIFY_PARAMS__(req->fin.hash);

    /*
     * te_dgst_finish() on a already finished dgst ctx is allowed,
     * where hash("") is expected.
     */
    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HASH_OUT__;
    }

    switch (ret) {
    case TE_DRV_HASH_STATE_INIT:
        ret = te_hash_start(ctx->crypt, NULL);
        __HASH_ALERT__(ret, "te_hash_start error!");
        __HASH_CHECK_CONDITION__(ret);
        /* FALLTHRU */
		fallthrough;
    case TE_DRV_HASH_STATE_START:
        ret = te_hash_update(ctx->crypt, NULL, 0);
        __HASH_ALERT__(ret, "te_hash_update error!");
        __HASH_CHECK_CONDITION__(ret);
        break;
    default:
        break;
    }

    hreq = (te_hash_request_t *)osal_calloc( 1, sizeof(te_hash_request_t) );
    if ( hreq == NULL ) {
        ret = TE_ERROR_OOM;
        __HASH_OUT__;
    }

    hreq->base.data = (void *)req;
    hreq->base.completion = dgst_async_generic_done;
    hreq->fin.out = req->fin.hash;
    hreq->fin.olen = _HASH_GET_DIGEST_LEN_(
                     TE_ALG_GET_MAIN_ALG(ctx->crypt->alg) );

    ret = te_hash_afinish( ctx->crypt, hreq );
    __HASH_CHECK_CONDITION__(ret);

    return ret;

__out__:
    OSAL_SAFE_FREE( hreq );
    return ret;
}

static void dgst_aclone_done( te_async_request_t *r, int err )
{
    te_hash_request_t *hreq = (te_hash_request_t *)r;
    te_dgst_request_t *req = (te_dgst_request_t *)r->data;
    te_dgst_ctx_t *dst = req->cl.dst;

    req->res = err;
    if ( err != TE_SUCCESS ) {
        te_hash_free_ctx( dst->crypt );
        dst->crypt = NULL;
    }

    osal_free( hreq );

    req->base.completion( &req->base, req->res );
    return;
}

int te_dgst_aclone( te_dgst_request_t *req )
{
    int ret = TE_ERROR_GENERIC;
    te_hash_request_t *hreq = NULL;
    te_hash_drv_t *hdrv = NULL;
    te_dgst_ctx_t *src = NULL;
    te_dgst_ctx_t *dst = NULL;

    if ( !req || !req->cl.src || !req->cl.src->crypt || !req->cl.dst ) {
        ret = TE_ERROR_BAD_PARAMS;
        __HASH_OUT__;
    }

    src = req->cl.src;
    dst = req->cl.dst;
    /*
     * free the dst ctx if it points to a valid dgst ctx already
     */
    if (NULL != dst->crypt){
        ret = te_dgst_free(dst);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_dgst_free error %x\n", ret);
            return ret;
        }
    }

    hdrv = (te_hash_drv_t *)src->crypt->drv;
    ret = te_hash_alloc_ctx(hdrv, src->crypt->alg, 0, &dst->crypt);
    __HASH_CHECK_CONDITION__(ret);

    hreq = osal_calloc( 1, sizeof(te_hash_request_t) );
    if ( hreq == NULL ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    hreq->base.data = req;
    hreq->base.completion = dgst_aclone_done;

    ret = te_hash_aclone( src->crypt, hreq );
    if ( ret != TE_SUCCESS ) {
        goto err2;
    }

    return TE_SUCCESS;

err2:
    osal_free( hreq );
err1:
    te_hash_free_ctx( dst->crypt );
    dst->crypt = NULL;
__out__:
    return ret;
}

static void adgst_start_done( te_async_request_t *r, int err )
{
    int ret = TE_ERROR_GENERIC;
    adgst_ctx_t *adgst = NULL;
    te_hash_request_t *start = NULL, *update = NULL;
    te_dgst_request_t *req = NULL;

    adgst = (adgst_ctx_t *)r->data;
    start = adgst->start;
    update = adgst->update;
    req = adgst->req;

    if ( err != TE_SUCCESS ) {
        goto error;
    }

    ret = te_hash_aupdate( adgst->ctx.crypt, update );
    if ( ret != TE_SUCCESS ) {
        goto error;
    }

    return;

error:
    te_dgst_free( &adgst->ctx );
    osal_free( start );
    osal_free( adgst );
    req->res = err;
    req->base.completion( &req->base, req->res );
    return;
}

static void adgst_update_done( te_async_request_t *r, int err )
{
    int ret = TE_ERROR_GENERIC;
    adgst_ctx_t *adgst = NULL;
    te_hash_request_t *start = NULL, *finish = NULL;
    te_dgst_request_t *req = NULL;

    adgst = (adgst_ctx_t *)r->data;
    finish = adgst->finish;
    start = adgst->start;
    req = adgst->req;

    if ( err != TE_SUCCESS ) {
        goto error;
    }

    ret = te_hash_afinish( adgst->ctx.crypt, finish );
    if ( ret != TE_SUCCESS ) {
        goto error;
    }

    return;

error:
    te_dgst_free( &adgst->ctx );
    osal_free( start );
    osal_free( adgst );
    req->res = err;
    req->base.completion( &req->base, req->res );
    return;
}

static void adgst_finish_done( te_async_request_t *r, int err )
{
    adgst_ctx_t *adgst = (adgst_ctx_t *)r->data;
    te_hash_request_t *start = adgst->start;
    te_dgst_request_t *req = adgst->req;

    te_dgst_free( &adgst->ctx );
    osal_free( start );
    osal_free( adgst );
    req->res = err;
    req->base.completion( &req->base, req->res );
    return;
}

int te_adgst( te_drv_handle hdl, te_algo_t alg, te_dgst_request_t *req )
{
    int ret = TE_ERROR_GENERIC;
    adgst_ctx_t *adgst = NULL;
    te_hash_request_t *start = NULL, *update = NULL;
    te_hash_request_t *finish = NULL, *hreqs = NULL;

    adgst = osal_calloc( 1, sizeof(adgst_ctx_t) );
    if ( !adgst ) {
        return TE_ERROR_OOM;
    }

    hreqs = osal_calloc( 3, sizeof(te_hash_request_t) );
    if ( !hreqs ) {
        goto err1;
    }

    start = &hreqs[0];
    update = &hreqs[1];
    finish = &hreqs[2];


    start->base.data = (void *)adgst;
    start->base.completion = adgst_start_done;

    update->base.data = (void *)adgst;
    update->base.completion = adgst_update_done;
    update->up.flags = HASH_FLAGS_LAST | HASH_FLAGS_LIST;
    update->up.lst.in.ents =  req->dgst.in.ents;
    update->up.lst.in.nent =  req->dgst.in.nent;

    finish->base.data = (void *)adgst;
    finish->base.completion = adgst_finish_done;
    finish->fin.out = req->dgst.hash;
    finish->fin.olen = _HASH_GET_DIGEST_LEN_(TE_ALG_GET_MAIN_ALG(alg));

    adgst->req = req;
    adgst->start = start;
    adgst->update = update;
    adgst->finish = finish;

    ret = te_dgst_init( &adgst->ctx, hdl, alg );
    if ( ret != TE_SUCCESS ) {
        goto err2;
    }

    ret = te_hash_astart( adgst->ctx.crypt, start );
    if ( ret != TE_SUCCESS ) {
        goto err3;
    }

    return TE_SUCCESS;

err3:
    te_dgst_free( &adgst->ctx );
err2:
    osal_free( hreqs );
err1:
    osal_free( adgst );
    return ret;
}

#endif /* CFG_TE_ASYNC_EN */
