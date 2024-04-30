//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_hmac.h>
#include <te_hash.h>

/**
 * HASH MAC private context
 */
typedef struct hash_hmac_ctx {
    te_drv_handle hdl;              /**< driver handle */
    te_hmac_key_t key;
} hash_hmac_ctx_t;

#define HMAC_EHDR_SIZE(x)   (sizeof(hmac_ehdr_t) + (x)->drvctx_sz)
#define HMAC_EHDR_DRVCTX(x) (uint8_t *)(((hmac_ehdr_t *)(x)) + 1)

/**
 * HMAC export state header magic number
 */
#define HMAC_EHDR_MAGIC     0x68454d48U /**< "HMEh" */

/**
 * HMAC export state header structure
 */
typedef struct hmac_export_hdr {
    uint32_t magic;                 /**< magic */
    te_drv_handle hdl;              /**< driver handle */
    uint32_t drvctx_sz;             /**< drvctx size in byte */
    /*
     * Commented out element used to visualize the layout dynamic part
     * of the struct.
     *
     * uint8_t drvctx[];
     */
} hmac_ehdr_t;

#ifdef CFG_TE_ASYNC_EN
typedef struct ahmac_ctx {
    te_hmac_ctx_t ctx;
    te_hmac_request_t *req;
    te_hmac_request_t *start;
    te_hmac_request_t *update;
    te_hmac_request_t *finish;
} ahmac_ctx_t;
#endif

#define __HMAC_OUT__    goto __out__

#define __HMAC_CHECK_CONDITION__(_ret_)                                        \
        do {                                                                   \
            if( TE_SUCCESS != _ret_ ){                                         \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 __HMAC_OUT__;                                                 \
            }                                                                  \
        } while (0);

#define __HMAC_ALERT__(_ret_, _msg_)                                           \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X %s\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_),                                       \
                                (_msg_));                                      \
            }                                                                  \
        } while (0);

int te_hmac_init( te_hmac_ctx_t *ctx, te_drv_handle hdl, te_algo_t alg )
{
    int ret = TE_SUCCESS;
    te_hash_drv_t *hdrv = NULL;
    hash_hmac_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx)
        || ((TE_ALG_HMAC_MD5 != alg)
        && (TE_ALG_HMAC_SHA1 != alg)
        && (TE_ALG_HMAC_SHA224 != alg)
        && (TE_ALG_HMAC_SHA256 != alg)
        && (TE_ALG_HMAC_SHA384 != alg)
        && (TE_ALG_HMAC_SHA512 != alg)
        && (TE_ALG_HMAC_SM3 != alg))) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    hdrv = (te_hash_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_HASH);
    ret = te_hash_alloc_ctx(hdrv,
                            alg,
                            sizeof(hash_hmac_ctx_t),
                            &ctx->crypt);
    te_drv_put(hdl, TE_DRV_TYPE_HASH);
    __HMAC_CHECK_CONDITION__(ret);

    /* buffer drv handle */
    prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(ctx);
    TE_ASSERT(prv_ctx != NULL);

    /* Set key type default to user key */
    prv_ctx->key.type = TE_KEY_TYPE_USER;

    prv_ctx->hdl = hdl;

__out__:
    return ret;
}

int te_hmac_free( te_hmac_ctx_t *ctx )
{
    int ret = TE_SUCCESS;

    if (NULL == ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HMAC_OUT__;
    }
    switch (ret) {
    case TE_DRV_HASH_STATE_START:
    case TE_DRV_HASH_STATE_UPDATE:
    case TE_DRV_HASH_STATE_LAST:
        ret = te_hash_finish(ctx->crypt, NULL, 0);
        __HMAC_ALERT__(ret, "te_sca_finish raised exceptions!");
        break;
    default:
        ret = TE_SUCCESS;
        break;
    }

    te_hash_free_ctx(ctx->crypt);
    ctx->crypt = NULL;
__out__:
    return ret;
}

int te_hmac_start( te_hmac_ctx_t *ctx,
                   const uint8_t *key,
                   uint32_t keybits )
{
#define BYTE_BITS   (8U)
    int ret = TE_SUCCESS;
    hash_hmac_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (key == NULL) || (0 != (keybits % BYTE_BITS))) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HMAC_OUT__;
    }
    switch (ret) {
    case TE_DRV_HASH_STATE_START:
    case TE_DRV_HASH_STATE_UPDATE:
    case TE_DRV_HASH_STATE_LAST:
        ret = te_hash_finish(ctx->crypt, NULL, 0);
        __HMAC_ALERT__(ret, "te_hash_finish error!");
        break;
    default:
        ret = TE_SUCCESS;
        break;
    }

    prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    prv_ctx->key.type = TE_KEY_TYPE_USER;
    memset(prv_ctx->key.hkey, 0, sizeof(prv_ctx->key.hkey));
    if (ctx->crypt->blk_size >= (keybits / BYTE_BITS)) {
        memcpy(prv_ctx->key.hkey, key, keybits / BYTE_BITS);
    } else {
        ret = te_dgst( prv_ctx->hdl,
                       TE_ALG_HASH_ALGO(
                       TE_ALG_GET_MAIN_ALG(ctx->crypt->alg)),
                       key,
                       keybits / BYTE_BITS,
                       prv_ctx->key.hkey );
    }

    __HMAC_CHECK_CONDITION__(ret);
    ret = te_hash_start(ctx->crypt, &prv_ctx->key);
__out__:
    return ret;
}

int te_hmac_start2( te_hmac_ctx_t *ctx,
                    te_sec_key_t *key )
{
    int ret = TE_SUCCESS;
    hash_hmac_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (key == NULL) || (0 != (key->ek3bits % BYTE_BITS)) ||
        (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HMAC_OUT__;
    }

    switch (ret) {
    case TE_DRV_HASH_STATE_START:
    case TE_DRV_HASH_STATE_UPDATE:
    case TE_DRV_HASH_STATE_LAST:
        ret = te_hash_finish(ctx->crypt, NULL, 0);
        __HMAC_ALERT__(ret, "te_hash_finish error!");
        break;
    default:
        ret = TE_SUCCESS;
        break;
    }

    prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(ctx);
    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    prv_ctx->key.type = TE_KEY_TYPE_SEC;
    memcpy(&prv_ctx->key.sec, key, sizeof(te_sec_key_t));
    ret = te_hash_start(ctx->crypt, &prv_ctx->key);
__out__:
    return ret;
}

int te_hmac_start2_v2( te_hmac_ctx_t *ctx,
                       te_sec_key_v2_t *key )
{
    int ret = TE_SUCCESS;
    hash_hmac_ctx_t *prv_ctx = NULL;

    /* Because SM4 doesn't support key-ladder 256, so ban hmac-sm3 */
    if ((NULL == ctx) || (key == NULL) || (0 != (key->ek3bits % BYTE_BITS)) ||
        (NULL == ctx->crypt) || (TE_ALG_HMAC_SM3 == ctx->crypt->alg)) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HMAC_OUT__;
    }

    switch (ret) {
    case TE_DRV_HASH_STATE_START:
    case TE_DRV_HASH_STATE_UPDATE:
    case TE_DRV_HASH_STATE_LAST:
        ret = te_hash_finish(ctx->crypt, NULL, 0);
        __HMAC_ALERT__(ret, "te_hash_finish error!");
        break;
    default:
        ret = TE_SUCCESS;
        break;
    }

    prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(ctx);
    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    prv_ctx->key.type = TE_KEY_TYPE_SEC_V2;
    memcpy(&prv_ctx->key.sec_v2, key, sizeof(te_sec_key_v2_t));
    ret = te_hash_start(ctx->crypt, &prv_ctx->key);
__out__:
    return ret;
}

int te_hmac_update( te_hmac_ctx_t *ctx,
                    const uint8_t *in,
                    size_t len )
{
    int ret = TE_SUCCESS;
    hash_hmac_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == ctx->crypt) || ((NULL == in) && (len != 0))) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HMAC_OUT__;
    }

    switch (ret) {
    case TE_DRV_HASH_STATE_INIT:
        ret = te_hash_start(ctx->crypt, &prv_ctx->key);
        __HMAC_ALERT__(ret, "te_hash_start error!");
        __HMAC_CHECK_CONDITION__(ret);
        break;
    default:
        ret = TE_SUCCESS;
        break;
    }

    ret = te_hash_update(ctx->crypt, in, len);
__out__:
    return ret;
}

int te_hmac_uplist( te_hmac_ctx_t *ctx,
                    te_memlist_t *in )
{
    int ret = TE_SUCCESS;
    hash_hmac_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == ctx->crypt) || (NULL == in)) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    if ((0 != in->nent) && (NULL == in->ents)) {
        __HMAC_OUT__;
    }

    prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HMAC_OUT__;
    }

    switch (ret) {
    case TE_DRV_HASH_STATE_INIT:
        ret = te_hash_start(ctx->crypt, &prv_ctx->key);
        __HMAC_ALERT__(ret, "te_hash_start error!");
        __HMAC_CHECK_CONDITION__(ret);
        break;
    default:
        ret = TE_SUCCESS;
        break;
    }

    ret = te_hash_uplist(ctx->crypt, in);
__out__:
    return ret;
}

int te_hmac_finish( te_hmac_ctx_t *ctx,
                    uint8_t *mac,
                    uint32_t maclen )
{
    int ret = TE_SUCCESS;
    hash_hmac_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) ||
        (NULL == ctx->crypt) ||
        ((NULL == mac) && (maclen != 0))) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HMAC_OUT__;
    }

    switch (ret) {
    case TE_DRV_HASH_STATE_INIT:
        /* finish after finish */
        ret = te_hash_start(ctx->crypt, &prv_ctx->key);
        __HMAC_ALERT__(ret, "te_hash_start error!");
        __HMAC_CHECK_CONDITION__(ret);
        /* FALLTHRU */
		fallthrough;
    case TE_DRV_HASH_STATE_START:
        /* finsh after start */
        ret = te_hash_update(ctx->crypt, NULL, 0);
        __HMAC_ALERT__(ret, "te_hash_update error!");
        __HMAC_CHECK_CONDITION__(ret);
        /* FALLTHRU */
		fallthrough;
    case TE_DRV_HASH_STATE_UPDATE:
    case TE_DRV_HASH_STATE_LAST:
        break;
    default:
        ret = TE_SUCCESS;
        break;
    }

    ret = te_hash_finish(ctx->crypt, mac, maclen);
__out__:
    return ret;
}

int te_hmac_reset( te_hmac_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    hash_hmac_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HMAC_OUT__;
    }

    switch (ret) {
    case TE_DRV_HASH_STATE_INIT:
        ret = te_hash_start(ctx->crypt, &prv_ctx->key);
        __HMAC_ALERT__(ret, "te_hash_start error!");
        break;
    case TE_DRV_HASH_STATE_START:
    case TE_DRV_HASH_STATE_UPDATE:
    case TE_DRV_HASH_STATE_LAST:
        ret = te_hash_reset(ctx->crypt);
        __HMAC_ALERT__(ret, "te_hash_reset error!");
        break;
    default:
        ret = TE_SUCCESS;
        break;
    }

__out__:
    return ret;
}

int te_hmac_clone( te_hmac_ctx_t *src, te_hmac_ctx_t *dst)
{
    int ret = TE_SUCCESS;
    hash_hmac_ctx_t *dst_prv_ctx = NULL;
    hash_hmac_ctx_t *src_prv_ctx = NULL;
    te_hash_drv_t *hdrv = NULL;


    if((NULL == src) || (NULL == src->crypt) || (NULL == dst)) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    /* Mbedtls clone before start case */
    ret = te_hash_state(src->crypt);
    if (ret < 0) {
        __HMAC_OUT__;
    }

    switch (ret) {
    case TE_DRV_HASH_STATE_INIT:
        /* If source context not be started, do nothing */
        ret = TE_SUCCESS;
        __HMAC_OUT__;
        break;
    default:
        ret = TE_SUCCESS;
        break;
    }

    /*
     * free the dst ctx if it points to a valid hmac ctx already
     */
    if (NULL != dst->crypt){
        ret = te_hmac_free(dst);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_hmac_free error %x\n", ret);
            return ret;
        }
    }

    hdrv = (te_hash_drv_t *)src->crypt->drv;
    ret = te_hash_alloc_ctx( hdrv,
                             src->crypt->alg,
                             sizeof(hash_hmac_ctx_t),
                             &dst->crypt );
    __HMAC_CHECK_CONDITION__(ret);

    src_prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(src);
    dst_prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(dst);
    memcpy( (void *)dst_prv_ctx,
            (void *)src_prv_ctx,
            sizeof(hash_hmac_ctx_t) );

    ret = te_hash_clone( src->crypt, dst->crypt );
    if ( ret != TE_SUCCESS ) {
        te_hash_free_ctx( dst->crypt );
        __HMAC_OUT__;
    }

__out__:
    return ret;
}

int te_hmac_export( te_hmac_ctx_t *ctx,
                      void *out,
                      uint32_t *olen )
{
    int ret = TE_ERROR_GENERIC;
    hash_hmac_ctx_t *pctx = NULL;
    hmac_ehdr_t eh = {0};

    if ((NULL == ctx) || (NULL == ctx->crypt) || (NULL == olen)) {
        return TE_ERROR_BAD_PARAMS;
    }

    pctx = (hash_hmac_ctx_t *)hmac_priv_ctx(ctx);
    if(NULL == pctx){
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * poll for drvctx_sz
     */
    eh.drvctx_sz = 0;
    ret = te_hash_export(ctx->crypt, NULL, &eh.drvctx_sz);
    if (ret != (int)TE_ERROR_SHORT_BUFFER) {
        return ret;
    }

    /*
     * be fancy to the caller
     */
    if (*olen < HMAC_EHDR_SIZE(&eh)) {
        *olen = HMAC_EHDR_SIZE(&eh);
        return TE_ERROR_SHORT_BUFFER;
    }

    if (NULL == out) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * export drvctx
     * TODO: lock the hmac driver to stop service of update() or uplist() on
     * the calling context until te_hash_export() ends.
     * Or, it's the caller responsibility to ensure there be no update() or
     * uplist() call on to the same context when an export() is outstanding.
     */
    ret = te_hash_export(ctx->crypt, HMAC_EHDR_DRVCTX(out), &eh.drvctx_sz);
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR( "te_hash_export error %x\n", ret );
        goto err;
    }

    /*
     * make ehdr
     */
    eh.magic = HMAC_EHDR_MAGIC;
    eh.hdl   = pctx->hdl;

    osal_memcpy(out, &eh, sizeof(eh));
    *olen = HMAC_EHDR_SIZE(&eh);
err:
    return ret;
}

int te_hmac_import( te_hmac_ctx_t *ctx,
                      const void *in,
                      uint32_t ilen )
{
    int ret = TE_ERROR_GENERIC;
    hash_hmac_ctx_t *pctx = NULL;
    hmac_ehdr_t eh = {0};

    if ((NULL == ctx) || (NULL == ctx->crypt) || (NULL == in)) {
        return TE_ERROR_BAD_PARAMS;
    }

    pctx = (hash_hmac_ctx_t *)hmac_priv_ctx(ctx);
    if(NULL == pctx){
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * The 'in' might not start at struct ptr safe boundary.
     * Be safe to copy the struct before reading it.
     */
    osal_memcpy(&eh, in, sizeof(eh));

    if ((eh.magic != HMAC_EHDR_MAGIC) ||
        (ilen < HMAC_EHDR_SIZE(&eh))) {
        OSAL_LOG_ERR("Bad or mismatched hmac ehdr: %d\n", ilen);
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * import drvctx
     */
    ret = te_hash_import(ctx->crypt, HMAC_EHDR_DRVCTX(in), eh.drvctx_sz );
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR("te_hash_import error %x\n", ret);
        return ret;
    }

    /*
     * import hmac ctx
     */
    pctx->hdl = eh.hdl;

    return TE_SUCCESS;
}

#ifdef CFG_TE_ASYNC_EN
static void hmac_async_generic_done( te_async_request_t *r, int err )
{
    te_hash_request_t *hreq = (te_hash_request_t *)r;
    te_hmac_request_t *req = (te_hmac_request_t *)r->data;

    osal_free( hreq );
    req->res = err;
    req->base.completion( &req->base, req->res );
    return;
}

int te_hmac_astart( te_hmac_ctx_t *ctx, te_hmac_request_t *req )
{
    int ret = TE_SUCCESS;
    hash_hmac_ctx_t *prv_ctx = NULL;
    te_hash_request_t *hreq = NULL;

    if( !ctx || !req ) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    if ( req->st.key.type != TE_KEY_TYPE_SEC &&
         req->st.key.type != TE_KEY_TYPE_USER ) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    if ( req->st.key.type == TE_KEY_TYPE_USER &&
         (req->st.key.user.key == NULL ||
          0 != (req->st.key.user.keybits % BYTE_BITS)) ) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    if ( req->st.key.type == TE_KEY_TYPE_SEC &&
         ((req->st.key.sec.sel != TE_KL_KEY_MODEL &&
            req->st.key.sec.sel != TE_KL_KEY_ROOT) ||
           (req->st.key.sec.ek3bits != 128 &&
            req->st.key.sec.ek3bits != 256)) ) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx( ctx );
    if( NULL == prv_ctx ) {
        return TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    hreq = (te_hash_request_t *)osal_calloc( 1, sizeof(te_hash_request_t) );
    if ( hreq == NULL ) {
        ret = TE_ERROR_OOM;
        __HMAC_OUT__;
    }

    if ( req->st.key.type == TE_KEY_TYPE_USER ) {
        hreq->st.key.type = TE_KEY_TYPE_USER;
        if( ctx->crypt->blk_size >=
                (req->st.key.user.keybits / BYTE_BITS) ) {
            memcpy( hreq->st.key.hkey,
                    req->st.key.user.key,
                    req->st.key.user.keybits / BYTE_BITS );
        } else {
            ret = te_dgst( prv_ctx->hdl,
                           TE_ALG_HASH_ALGO(
                           TE_ALG_GET_MAIN_ALG(ctx->crypt->alg) ),
                           req->st.key.user.key,
                           req->st.key.user.keybits / BYTE_BITS,
                           hreq->st.key.hkey );
        }

    } else {
        hreq->st.key.type = TE_KEY_TYPE_SEC;
        memcpy( &hreq->st.key.sec, &req->st.key.sec, sizeof(te_sec_key_t) );
    }

    __HMAC_CHECK_CONDITION__(ret);


    hreq->base.data = (void *)req;
    hreq->base.completion = hmac_async_generic_done;

    ret = te_hash_astart( ctx->crypt, hreq );
    if ( ret != TE_SUCCESS ) {
        osal_free( hreq );
        __HMAC_OUT__;
    }

__out__:
    return ret;
}

int te_hmac_aupdate( te_hmac_ctx_t *ctx, te_hmac_request_t *req )
{
    int ret = TE_SUCCESS;
    te_hash_request_t *hreq = NULL;

    if( !ctx || !req ) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    if ( !req->up.in.ents && req->up.in.nent != 0 ) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    hreq = (te_hash_request_t *)osal_calloc( 1, sizeof(te_hash_request_t) );
    if ( hreq == NULL ) {
        ret = TE_ERROR_OOM;
        __HMAC_OUT__;
    }

    hreq->base.data = (void *)req;
    hreq->base.completion = hmac_async_generic_done;
    hreq->up.flags = HASH_FLAGS_LIST;
    hreq->up.lst.in.ents = req->up.in.ents;
    hreq->up.lst.in.nent = req->up.in.nent;

    ret = te_hash_aupdate( ctx->crypt, hreq );
    if ( ret != TE_SUCCESS ) {
        osal_free( hreq );
        __HMAC_OUT__;
    }

__out__:
    return ret;
}

int te_hmac_afinish( te_hmac_ctx_t *ctx, te_hmac_request_t *req )
{
    int ret = TE_SUCCESS;
    hash_hmac_ctx_t *prv_ctx = NULL;
    te_hash_request_t *hreq = NULL;

    if( !ctx || !req ) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    if ( !req->fin.mac || req->fin.maclen == 0 ) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __HMAC_OUT__;
    }

    ret = te_hash_state(ctx->crypt);
    if (ret < 0) {
        __HMAC_OUT__;
    }

    switch (ret) {
    case TE_DRV_HASH_STATE_INIT:
        /* finish after finish */
        ret = te_hash_start(ctx->crypt, &prv_ctx->key);
        __HMAC_ALERT__(ret, "te_hash_start error!");
        __HMAC_CHECK_CONDITION__(ret);
        /* FALLTHRU */
		fallthrough;
    case TE_DRV_HASH_STATE_START:
        /* finsh after start */
        ret = te_hash_update(ctx->crypt, NULL, 0);
        __HMAC_ALERT__(ret, "te_hash_update error!");
        __HMAC_CHECK_CONDITION__(ret);
		fallthrough;
    case TE_DRV_HASH_STATE_UPDATE:
    case TE_DRV_HASH_STATE_LAST:
        break;
    default:
        ret = TE_SUCCESS;
        break;
    }

    hreq = (te_hash_request_t *)osal_calloc( 1, sizeof(te_hash_request_t) );
    if ( hreq == NULL ) {
        ret = TE_ERROR_OOM;
        __HMAC_OUT__;
    }

    hreq->base.data = (void *)req;
    hreq->base.completion = hmac_async_generic_done;
    hreq->fin.out = req->fin.mac;
    hreq->fin.olen = req->fin.maclen;

    ret = te_hash_afinish( ctx->crypt, hreq );
    if ( ret != TE_SUCCESS ) {
        osal_free( hreq );
        __HMAC_OUT__;
    }

__out__:
    return ret;
}

int te_hmac_aclone( te_hmac_request_t *req)
{
    int ret = TE_ERROR_GENERIC;
    te_hmac_ctx_t *src = NULL;
    te_hmac_ctx_t *dst = NULL;
    hash_hmac_ctx_t *dst_prv_ctx = NULL;
    hash_hmac_ctx_t *src_prv_ctx = NULL;
    te_hash_request_t *hreq = NULL;
    te_hash_drv_t *hdrv = NULL;

    if ( !req || !req->cl.src || !req->cl.src->crypt || !req->cl.dst ) {
        return TE_ERROR_BAD_PARAMS;
    }
    src = req->cl.src;
    dst = req->cl.dst;

    /*
     * free the dst ctx if it points to a valid hmac ctx already
     */
    if (NULL != dst->crypt){
        ret = te_hmac_free(dst);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_hmac_free error %x\n", ret);
            return ret;
        }
    }

    hreq = (te_hash_request_t *)osal_calloc( 1, sizeof(te_hash_request_t) );
    if ( !hreq ) {
        return TE_ERROR_OOM;
    }
    hreq->cl.dst = dst->crypt;
    hreq->base.data = (void *)req;
    hreq->base.completion = hmac_async_generic_done;

    hdrv = (te_hash_drv_t *)src->crypt->drv;
    ret = te_hash_alloc_ctx( hdrv,
                             src->crypt->alg,
                             sizeof(hash_hmac_ctx_t),
                             &dst->crypt );
    if ( ret != TE_SUCCESS ) {
        osal_free( hreq );
        return ret;
    }

    src_prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(src);
    dst_prv_ctx = (hash_hmac_ctx_t *)hmac_priv_ctx(dst);
    memcpy( (void *)dst_prv_ctx,
            (void *)src_prv_ctx,
            sizeof(hash_hmac_ctx_t) );

    ret = te_hash_aclone( src->crypt, hreq );
    if ( ret != TE_SUCCESS ) {
        osal_free( hreq );
        te_hash_free_ctx( dst->crypt );
        return ret;
    }

    return TE_SUCCESS;
}

static void ahmac_start_done( te_async_request_t *r, int err )
{
    int ret = TE_ERROR_GENERIC;
    ahmac_ctx_t *ahmac = NULL;
    te_hmac_request_t *start = NULL, *update = NULL;
    te_hmac_request_t *req = NULL;

    ahmac = (ahmac_ctx_t *)r->data;
    start = ahmac->start;
    update = ahmac->update;
    req = ahmac->req;

    if ( err != TE_SUCCESS ) {
        goto error;
    }

    ret = te_hmac_aupdate( &ahmac->ctx, update );
    if ( ret != TE_SUCCESS ) {
        goto error;
    }

    return;

error:
    te_hmac_free( &ahmac->ctx );
    osal_free( start );
    osal_free( ahmac );
    req->res = err;
    osal_wmb();
    req->base.completion( &req->base, req->res );
    return;
}

static void ahmac_update_done( te_async_request_t *r, int err )
{
    int ret = TE_ERROR_GENERIC;
    ahmac_ctx_t *ahmac = NULL;
    te_hmac_request_t *start = NULL, *finish = NULL;
    te_hmac_request_t *req = NULL;

    ahmac = (ahmac_ctx_t *)r->data;
    finish = ahmac->finish;
    start = ahmac->start;
    req = ahmac->req;

    if ( err != TE_SUCCESS ) {
        goto error;
    }

    ret = te_hmac_afinish( &ahmac->ctx, finish );
    if ( ret != TE_SUCCESS ) {
        goto error;
    }

    return;

error:
    te_hmac_free( &ahmac->ctx );
    osal_free( start );
    osal_free( ahmac );
    req->res = err;
    osal_wmb();
    req->base.completion( &req->base, req->res );
    return;
}

static void ahmac_finish_done( te_async_request_t *r, int err )
{
    ahmac_ctx_t *ahmac = (ahmac_ctx_t *)r->data;
    te_hmac_request_t *start = ahmac->start;
    te_hmac_request_t *req = ahmac->req;

    te_hmac_free( &ahmac->ctx );
    osal_free( start );
    osal_free( ahmac );
    req->res = err;
    req->base.completion( &req->base, req->res );
    return;
}

int te_ahmac( te_drv_handle hdl, te_algo_t alg, te_hmac_request_t *req)
{
    int ret = TE_ERROR_GENERIC;
    ahmac_ctx_t *ahmac = NULL;
    te_hmac_request_t *start = NULL, *update = NULL;
    te_hmac_request_t *finish = NULL, *mreqs = NULL;

    if ( !req ) {
        return TE_ERROR_BAD_PARAMS;
    }

    ahmac = osal_calloc( 1, sizeof(ahmac_ctx_t) );
    if ( !ahmac ) {
        return TE_ERROR_OOM;
    }

    mreqs = osal_calloc( 3, sizeof(te_hmac_request_t) );
    if ( !mreqs ) {
        goto err1;
    }

    start = &mreqs[0];
    update = &mreqs[1];
    finish = &mreqs[2];


    start->base.data = (void *)ahmac;
    start->base.completion = ahmac_start_done;
    memcpy( (void *)&start->st.key, (void *)&req->hmac.key, sizeof(te_key_wrap_t) );

    update->base.data = (void *)ahmac;
    update->base.completion = ahmac_update_done;
    update->up.in.ents =  req->hmac.in.ents;
    update->up.in.nent =  req->hmac.in.nent;

    finish->base.data = (void *)ahmac;
    finish->base.completion = ahmac_finish_done;
    finish->fin.mac = req->hmac.mac;
    finish->fin.maclen = req->hmac.maclen;

    ahmac->req = req;
    ahmac->start = start;
    ahmac->update = update;
    ahmac->finish = finish;

    ret = te_hmac_init( &ahmac->ctx, hdl, alg );
    if ( ret != TE_SUCCESS ) {
        goto err2;
    }

    ret = te_hmac_astart( &ahmac->ctx, start );
    if ( ret != TE_SUCCESS ) {
        goto err3;
    }

    return TE_SUCCESS;

err3:
    te_hmac_free( &ahmac->ctx );
err2:
    osal_free( mreqs );
err1:
    osal_free( ahmac );
    return ret;
}

#endif /* CFG_TE_ASYNC_EN */

