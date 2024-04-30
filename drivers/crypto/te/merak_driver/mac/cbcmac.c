//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_cmac.h>

/**
 * SCA CBCMAC private context
 */
typedef struct sca_cbcmac_ctx {
    uint8_t npdata[SCA_MAX_BLOCK_SIZE]; /**< not processed data */
    uint32_t npdlen;                    /**< data length in byte */
    uint8_t iv[SCA_MAX_BLOCK_SIZE];     /**< initial vector */
} sca_cbcmac_ctx_t;

#define CBCMAC_EHDR_SIZE(x)   (sizeof(cbcmac_ehdr_t) + (x)->drvctx_sz)
#define CBCMAC_EHDR_DRVCTX(x) (uint8_t *)(((cbcmac_ehdr_t *)(x)) + 1)

/**
 * CBCMAC export state header magic number
 */
#define CBCMAC_EHDR_MAGIC     0x48654d63U /**< "cMeH" */

/**
 * CBCMAC export state header structure
 */
typedef struct cbcmac_export_hdr {
    uint32_t magic;                     /**< magic */
    uint8_t npdata[SCA_MAX_BLOCK_SIZE]; /**< not processed data */
    uint32_t npdlen;
    uint32_t drvctx_sz;                 /**< drvctx size in byte */
    /*
     * Commented out element used to visualize the layout dynamic part
     * of the struct.
     *
     * uint8_t drvctx[];
     */
} cbcmac_ehdr_t;

#define __CBCMAC_OUT__    goto __out__

#define __CBCMAC_CHECK_CONDITION__(_ret_)                                      \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 __CBCMAC_OUT__;                                               \
            }                                                                  \
        } while (0);

#define __CBCMAC_ALERT__(_ret_, _msg_)                                         \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X %s\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_),                                       \
                                (_msg_));                                      \
            }                                                                  \
        } while (0);


/**
 * malg = TE_MAIN_ALGO_DES | TDES | AES | SM4
 */
int te_cbcmac_init( te_cbcmac_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;

    if(NULL == ctx ||
       ((TE_MAIN_ALGO_AES != malg)
        &&(TE_MAIN_ALGO_DES != malg)
        &&(TE_MAIN_ALGO_TDES != malg)
        &&(TE_MAIN_ALGO_SM4 != malg))){
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }

    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    ret = te_sca_alloc_ctx(sdrv,
                           malg,
                           sizeof(sca_cbcmac_ctx_t),
                           &ctx->crypt);
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
    __CBCMAC_CHECK_CONDITION__(ret);

    switch (malg)
    {
    case TE_MAIN_ALGO_SM4:
        ctx->crypt->alg = TE_ALG_SM4_CBC_MAC_NOPAD;
        break;
    case TE_MAIN_ALGO_DES:
        ctx->crypt->alg = TE_ALG_DES_CBC_MAC_NOPAD;
        break;
    case TE_MAIN_ALGO_TDES:
        ctx->crypt->alg = TE_ALG_TDES_CBC_MAC_NOPAD;
        break;
    case TE_MAIN_ALGO_AES:
    default:
        ctx->crypt->alg = TE_ALG_AES_CBC_MAC_NOPAD;
        break;
    }
__out__:
    return ret;
}

int te_cbcmac_free( te_cbcmac_ctx_t *ctx )
{
    int ret = TE_SUCCESS;

    if(NULL == ctx || NULL == ctx->crypt){
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }
    ret = te_sca_state(ctx->crypt);
    switch (ret){
    default:
        __CBCMAC_OUT__;
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        ret = TE_SUCCESS;
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __CBCMAC_ALERT__(ret, "te_sca_finish raised exceptions!");
        break;
    }

    te_sca_free_ctx(ctx->crypt);
    ctx->crypt = NULL;
__out__:
    return ret;
}

int te_cbcmac_setkey( te_cbcmac_ctx_t *ctx,
                      const uint8_t *key,
                      uint32_t keybits )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};

    if ((NULL == ctx) || (NULL == ctx->crypt) || (NULL == key)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }

    switch (te_sca_state(ctx->crypt)){
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        __CBCMAC_OUT__;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __CBCMAC_CHECK_CONDITION__(ret);
        break;
    }
    key_desc.type = TE_KEY_TYPE_USER;
    key_desc.user.key = (uint8_t *)key;
    key_desc.user.keybits = keybits;
    ret = te_sca_setkey(ctx->crypt, &key_desc);

__out__:
    return ret;
}

int te_cbcmac_setseckey( te_cbcmac_ctx_t *ctx,
                         te_sec_key_t *key )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};

    if ((NULL == ctx) || (NULL == ctx->crypt) || (NULL == key)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }

    switch (te_sca_state(ctx->crypt)){
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        __CBCMAC_OUT__;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __CBCMAC_CHECK_CONDITION__(ret);
        break;
    }
    key_desc.type = TE_KEY_TYPE_SEC;
    memcpy(&key_desc.sec, key, sizeof(te_sec_key_t));
    ret = te_sca_setkey(ctx->crypt, &key_desc);

__out__:
    return ret;
}

int te_cbcmac_setseckey_v2( te_cbcmac_ctx_t *ctx,
                            te_sec_key_v2_t *key )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};

    if ((NULL == ctx) || (NULL == ctx->crypt) || (NULL == key) ||
        (TE_ALG_AES_CBC_MAC_NOPAD != ctx->crypt->alg) ){
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }

    switch (te_sca_state(ctx->crypt)){
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        __CBCMAC_OUT__;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __CBCMAC_CHECK_CONDITION__(ret);
        break;
    }
    key_desc.type = TE_KEY_TYPE_SEC_V2;
    memcpy(&key_desc.sec_v2, key, sizeof(te_sec_key_v2_t));
    ret = te_sca_setkey(ctx->crypt, &key_desc);

__out__:
    return ret;
}

int te_cbcmac_start( te_cbcmac_ctx_t *ctx,
                     const uint8_t *iv )
{
    int ret = TE_SUCCESS;
    sca_cbcmac_ctx_t *prv_ctx = NULL;
    if((NULL == ctx) || (NULL == iv) || (NULL == ctx->crypt)){
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }
    prv_ctx = (sca_cbcmac_ctx_t*)cbcmac_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    osal_memcpy(prv_ctx->iv, iv, ctx->crypt->blk_size);
    prv_ctx->npdlen = 0;
    ret = te_sca_start(ctx->crypt,
                        TE_DRV_SCA_ENCRYPT,
                        prv_ctx->iv,
                        ctx->crypt->blk_size);
__out__:
    return ret;
}

int te_cbcmac_update( te_cbcmac_ctx_t *ctx,
                      size_t len,
                      const uint8_t *in )
{
    int ret = TE_SUCCESS;
    sca_cbcmac_ctx_t *prv_ctx = NULL;
    size_t remainder = 0;
    size_t _len = 0;

    if(NULL == ctx || NULL == ctx->crypt){
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }

    if(NULL == in && len){
        ret = TE_ERROR_BAD_INPUT_DATA;
        __CBCMAC_OUT__;
    }

    if(0 == len){
        ret = TE_SUCCESS;
        __CBCMAC_OUT__;
    }

    prv_ctx = (sca_cbcmac_ctx_t *)cbcmac_priv_ctx(ctx);

    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }

    /* still data left? */
    if(ctx->crypt->blk_size > prv_ctx->npdlen){
        _len = (len > (ctx->crypt->blk_size - prv_ctx->npdlen)) \
               ? (ctx->crypt->blk_size - prv_ctx->npdlen) : len;
        osal_memcpy(prv_ctx->npdata + prv_ctx->npdlen, in, _len);
        len -= _len;
        in += _len;
        prv_ctx->npdlen += _len;
    }

    if (prv_ctx->npdlen == ctx->crypt->blk_size) {
        ret = te_sca_update(ctx->crypt, false, prv_ctx->npdlen,
                            prv_ctx->npdata, NULL);
        if (TE_SUCCESS != ret) {
            prv_ctx->npdlen -= _len;
            osal_memset(prv_ctx->npdata+prv_ctx->npdlen, 0x00, _len);
            __CBCMAC_OUT__;
        }
        prv_ctx->npdlen = 0;
    }

    if (len == 0) {
        __CBCMAC_OUT__;
    }

    remainder = len % ctx->crypt->blk_size;
    len -= remainder;

    if( 0 < len){
        ret = te_sca_update(ctx->crypt, false, len, in, NULL);
        __CBCMAC_CHECK_CONDITION__(ret);
    }

    if(0 < remainder){
        prv_ctx->npdlen = remainder;
        memcpy(prv_ctx->npdata,
              in + len,
              remainder);
    }

__out__:
    return ret;
}

int te_cbcmac_uplist( te_cbcmac_ctx_t *ctx,
                      te_memlist_t *in )
{
    int ret = TE_SUCCESS;
    sca_cbcmac_ctx_t *prv_ctx = NULL;
    size_t _i = 0;
    size_t remainder = 0;
    size_t _total_size = 0;
    uint8_t _tmp_buf[SCA_MAX_BLOCK_SIZE] = {0};
    te_ml_bp_t in_info = {0};
    te_memlist_t _in = {0};
    size_t org_npdlen = 0;

    if (NULL == ctx || NULL == ctx->crypt
        || (in && in->nent && !in->ents)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }

    if ((NULL == in) || (0 == in->nent)) {
        ret = TE_SUCCESS;
        __CBCMAC_OUT__;
    }

    prv_ctx = (sca_cbcmac_ctx_t *)cbcmac_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }

    _total_size = prv_ctx->npdlen;

    for ( _i = 0; _i < in->nent; _i++) {
        _total_size += in->ents[_i].len;
    }

    if(_total_size <= ctx->crypt->blk_size){
        /**< back up original npdlen, in case of failed then recover */
        org_npdlen = prv_ctx->npdlen;
        /*just copy to npdata */
        for ( _i = 0; _i < in->nent; _i++) {
            memcpy(prv_ctx->npdata + prv_ctx->npdlen,
                   in->ents[_i].buf,
                   in->ents[_i].len);
            prv_ctx->npdlen += in->ents[_i].len;
        }
        /* if full fill complet block then feed into engine */
        if (prv_ctx->npdlen == ctx->crypt->blk_size) {
            ret = te_sca_update(ctx->crypt, false, prv_ctx->npdlen,
                                prv_ctx->npdata, NULL);
            if (TE_SUCCESS != ret) {
                osal_memset(prv_ctx->npdata + org_npdlen, 0x00,
                            prv_ctx->npdlen - org_npdlen);
                prv_ctx->npdlen = org_npdlen;
                __CBCMAC_OUT__;
            }
            prv_ctx->npdlen = 0;
        }
    }else{
        remainder = _total_size % ctx->crypt->blk_size;
        te_memlist_truncate_from_tail(in, _tmp_buf, remainder,
                                            true, &in_info);
        _in.nent = in_info.ind + 1 + ((prv_ctx->npdlen > 0) ? 1 : 0);
        _in.ents = (te_mement_t *)osal_calloc(_in.nent, sizeof(te_mement_t));
        TE_ASSERT(NULL != _in.ents);
        if (prv_ctx->npdlen) {
            _in.ents[0].buf = prv_ctx->npdata;
            _in.ents[0].len = prv_ctx->npdlen;
            _in.nent = 1;
        } else {
            _in.nent = 0;
        }
        osal_memcpy(_in.ents + _in.nent, in->ents,
                    (in_info.ind + 1) * sizeof(te_mement_t));
        _in.nent += (in_info.ind + 1);
        ret = te_sca_uplist(ctx->crypt, false, &_in, NULL);
        in->nent = in_info.nent;
        in->ents[in_info.ind].len = in_info.len;
        __CBCMAC_CHECK_CONDITION__(ret);
        prv_ctx->npdlen = 0;

        /* update npdata */
        if (0 < remainder) {
            memcpy(prv_ctx->npdata, _tmp_buf, remainder);
            prv_ctx->npdlen = remainder;
        }
    }

__out__:
    if (_in.ents) {
        osal_free(_in.ents);
        _in.ents = NULL;
    }
    return ret;
}

int te_cbcmac_finish( te_cbcmac_ctx_t *ctx,
                      uint8_t *mac,
                      uint32_t maclen )
{
    int ret = TE_SUCCESS;
    sca_cbcmac_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == mac)){
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }

    prv_ctx = (sca_cbcmac_ctx_t *)cbcmac_priv_ctx(ctx);

    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }
    /**< non block aligned clear hw engine status and return */
    if (prv_ctx->npdlen > 0) {
        te_sca_finish(ctx->crypt, NULL, 0);
        ret = TE_ERROR_BAD_INPUT_DATA;
        __CBCMAC_OUT__;
    }

    ret = te_sca_finish(ctx->crypt, mac, maclen);
__out__:
    return ret;
}

int te_cbcmac_reset( te_cbcmac_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    sca_cbcmac_ctx_t *prv_ctx = NULL;

    if(NULL == ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __CBCMAC_OUT__;
    }
    /* we can well slip out if the calling ctx is in START state */
    if (TE_DRV_SCA_STATE_START == te_sca_state(ctx->crypt)) {
        ret = TE_SUCCESS;
        __CBCMAC_OUT__;
    }
    ret = te_sca_reset(ctx->crypt);
    __CBCMAC_CHECK_CONDITION__(ret);
    prv_ctx = (sca_cbcmac_ctx_t *)cbcmac_priv_ctx(ctx);

    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_FORMAT;
        __CBCMAC_OUT__;
    }
    osal_memset(prv_ctx, 0x00, sizeof(*prv_ctx));
    ret = te_sca_start( ctx->crypt, TE_DRV_SCA_ENCRYPT, prv_ctx->iv,
                        ctx->crypt->blk_size );
__out__:
    return ret;
}

int te_cbcmac_clone( const te_cbcmac_ctx_t *src,
                     te_cbcmac_ctx_t *dst )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;
    sca_cbcmac_ctx_t *spctx = NULL;
    sca_cbcmac_ctx_t *dpctx = NULL;

    if (NULL == src || NULL == src->crypt || NULL == dst) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * free the dst ctx if it points to a valid cbcmac ctx already
     */
    if (dst->crypt != NULL) {
        ret = te_cbcmac_free(dst);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_cbcmac_free error %x\n", ret);
            return ret;
        }
    }

    spctx = (sca_cbcmac_ctx_t *)cbcmac_priv_ctx((te_cbcmac_ctx_t*)src);
    if(NULL == spctx){
        return TE_ERROR_BAD_PARAMS;
    }

    sdrv = (te_sca_drv_t *)src->crypt->drv;
    ret = te_sca_alloc_ctx(sdrv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_cbcmac_ctx_t),
                           &dst->crypt);
    if (ret != TE_SUCCESS) {
        return ret;
    }

    dpctx = (sca_cbcmac_ctx_t *)cbcmac_priv_ctx(dst);
    TE_ASSERT(dpctx != NULL);

    /*
     * clone driver ctx
     */
    ret = te_sca_clone(src->crypt, dst->crypt);
    if (ret != TE_SUCCESS) {
        te_sca_free_ctx(dst->crypt);
        dst->crypt = NULL;
        return ret;
    }

    /*
     * clone private ctx
     */
    osal_memcpy(dpctx, spctx, sizeof(*dpctx));

    return TE_SUCCESS;
}

int te_cbcmac_export( te_cbcmac_ctx_t *ctx,
                      void *out,
                      uint32_t *olen )
{
    int ret = TE_ERROR_GENERIC;
    sca_cbcmac_ctx_t *pctx = NULL;
    cbcmac_ehdr_t eh = {0};

    if ( NULL == ctx || NULL == ctx->crypt || NULL == olen ) {
        return TE_ERROR_BAD_PARAMS;
    }

    pctx = (sca_cbcmac_ctx_t *)cbcmac_priv_ctx(ctx);
    if(NULL == pctx){
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * poll for drvctx_sz
     */
    eh.drvctx_sz = 0;
    ret = te_sca_export(ctx->crypt, NULL, &eh.drvctx_sz);
    if (ret != (int)TE_ERROR_SHORT_BUFFER) {
        return ret;
    }

    /*
     * be fancy to the caller
     */
    if (*olen < CBCMAC_EHDR_SIZE(&eh)) {
        *olen = CBCMAC_EHDR_SIZE(&eh);
        return TE_ERROR_SHORT_BUFFER;
    }

    if (NULL == out) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * export drvctx
     * TODO: lock the cbcmac driver to stop service of update() or uplist() on
     * the calling context until te_sca_export() ends.
     * Or, it's the caller responsibility to ensure there be no update() or
     * uplist() call on to the same context when an export() is outstanding.
     */
    ret = te_sca_export(ctx->crypt, CBCMAC_EHDR_DRVCTX(out), &eh.drvctx_sz);
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR( "te_sca_export error %x\n", ret );
        goto err;
    }

    /*
     * make ehdr
     */
    eh.magic  = CBCMAC_EHDR_MAGIC;
    eh.npdlen = pctx->npdlen;
    osal_memcpy(eh.npdata, pctx->npdata, sizeof(eh.npdata));

    osal_memcpy(out, &eh, sizeof(eh));
    *olen = CBCMAC_EHDR_SIZE(&eh);
err:
    return ret;
}

int te_cbcmac_import( te_cbcmac_ctx_t *ctx,
                      const void *in,
                      uint32_t ilen )
{
    int ret = TE_ERROR_GENERIC;
    sca_cbcmac_ctx_t *pctx = NULL;
    cbcmac_ehdr_t eh = {0};

    if ( NULL == ctx || NULL == ctx->crypt || NULL == in ) {
        return TE_ERROR_BAD_PARAMS;
    }

    pctx = (sca_cbcmac_ctx_t *)cbcmac_priv_ctx(ctx);
    if(NULL == pctx){
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * The 'in' might not start at struct ptr safe boundary.
     * Be safe to copy the struct before reading it.
     */
    osal_memcpy(&eh, in, sizeof(eh));

    if (eh.magic != CBCMAC_EHDR_MAGIC ||
        ilen < CBCMAC_EHDR_SIZE(&eh)) {
        OSAL_LOG_ERR("Bad or mismatched cbcmac ehdr: %d\n", ilen);
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * import drvctx
     */
    ret = te_sca_import(ctx->crypt, CBCMAC_EHDR_DRVCTX(in), eh.drvctx_sz );
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR("te_sca_import error %x\n", ret);
        return ret;
    }

    /*
     * import cbcmac ctx
     */
    osal_memcpy(pctx->npdata, eh.npdata, sizeof(pctx->npdata));
    pctx->npdlen = eh.npdlen;

    return TE_SUCCESS;
}

/**
 * asynchronous cipher based MAC operations
 */
#ifdef CFG_TE_ASYNC_EN
typedef struct acbcmac_ctx {
    te_cmac_request_t *req;
    union {
        struct {
            te_cbcmac_ctx_t *cctx;
            uint8_t npdata[SCA_MAX_BLOCK_SIZE];
            uint32_t npdlen;
            te_memlist_t in;
        } up;

        struct {
            te_cbcmac_ctx_t cctx;
            te_cmac_request_t start;
            te_cmac_request_t update;
            te_cmac_request_t finish;
        } amac;

    };
} acbcmac_ctx_t;

static void cbcmac_aupdate_done( te_async_request_t *r, int err )
{
    te_sca_request_t *sreq = (te_sca_request_t *)r;
    acbcmac_ctx_t *cactx = (acbcmac_ctx_t *)r->data;
    te_cmac_request_t *req = cactx->req;
    sca_cbcmac_ctx_t *prv_ctx = (sca_cbcmac_ctx_t *)
                                cbcmac_priv_ctx( cactx->up.cctx );

    if ( cactx->up.npdlen && err == TE_SUCCESS ) {
        memcpy(prv_ctx->npdata, cactx->up.npdata, cactx->up.npdlen);
        prv_ctx->npdlen = cactx->up.npdlen;
    }

    osal_free( cactx->up.in.ents );
    osal_free( cactx );
    osal_free( sreq );
    req->res = err;
    req->base.completion( &req->base, req->res );
    return;
}

static void cbcmac_async_generic_done( te_async_request_t *r, int err )
{
    te_sca_request_t *sreq = (te_sca_request_t *)r;
    te_cmac_request_t *req = (te_cmac_request_t *)r->data;

    osal_free( sreq );
    req->res = err;
    req->base.completion( &req->base, req->res );
    return;
}

int te_cbcmac_astart( te_cbcmac_ctx_t *ctx, te_cmac_request_t *req )
{
    int ret = TE_ERROR_GENERIC;
    te_sca_request_t *sreq = NULL;
    sca_cbcmac_ctx_t *prv_ctx;

    if ( !ctx || !ctx->crypt || !req || !req->st.iv ) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }

    prv_ctx = cbcmac_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    sreq = (te_sca_request_t *)osal_calloc( 1, sizeof(te_sca_request_t) );
    if ( !sreq ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    osal_memcpy(prv_ctx->iv, req->st.iv, ctx->crypt->blk_size);
    sreq->base.data = (void *)req;
    sreq->base.completion = cbcmac_async_generic_done;
    sreq->st.op = TE_DRV_SCA_ENCRYPT;
    sreq->st.iv = (uint8_t *)prv_ctx->iv;
    sreq->st.ivlen = ctx->crypt->blk_size;

    ret = te_sca_astart(ctx->crypt, sreq );
    if ( ret != TE_SUCCESS ) {
        goto err2;
    }

    return ret;

err2:
    osal_free( sreq );
err1:
    return ret;
}

int te_cbcmac_aupdate( te_cbcmac_ctx_t *ctx, te_cmac_request_t *req )
{
    int ret = TE_ERROR_GENERIC;
    sca_cbcmac_ctx_t *prv_ctx = NULL;
    size_t _i = 0;
    te_memlist_t *_in = NULL;
    te_memlist_t *in = NULL;
    size_t _total_size = 0;
    size_t blk_size = 0;
    size_t remainder = 0;
    te_sca_request_t *sreq = NULL;
    acbcmac_ctx_t *cactx = NULL;
    te_ml_bp_t in_info = {0};

    if ( !ctx || !ctx->crypt || !req ||
         !req->up.in.ents || req->up.in.nent == 0) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }

    in = &req->up.in;
    blk_size = ctx->crypt->blk_size;

    prv_ctx = (sca_cbcmac_ctx_t *)cbcmac_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }

    _total_size = prv_ctx->npdlen;

    for ( _i = 0; _i < in->nent; _i++) {
        _total_size += in->ents[_i].len;
    }

    if(_total_size <= blk_size) {
        /*just copy to npdata and then out*/
        for ( _i = 0; _i < in->nent; _i++) {
            memcpy(prv_ctx->npdata + prv_ctx->npdlen,
                   in->ents[_i].buf,
                   in->ents[_i].len);
            prv_ctx->npdlen += in->ents[_i].len;
        }
        /* No data handle at all */
        req->res = TE_SUCCESS;
        req->base.completion( &req->base, req->res );

        return TE_SUCCESS;
    } else {
        remainder = _total_size % ctx->crypt->blk_size;
        if (0 == remainder) {
            remainder = ctx->crypt->blk_size;
        }

        sreq = (te_sca_request_t *)osal_calloc( 1, sizeof(te_sca_request_t) );
        if ( !sreq ) {
            ret = TE_ERROR_OOM;
            goto err1;
        }

        cactx = (acbcmac_ctx_t *)osal_calloc( 1, sizeof(acbcmac_ctx_t) );
        if ( cactx == NULL ) {
            ret = TE_ERROR_OOM;
            goto err2;
        }
        te_memlist_truncate_from_tail(in, cactx->up.npdata, remainder,
                                             true, &in_info);
        in->nent = in_info.nent;
        in->ents[in_info.ind].len = in_info.len;
        cactx->up.npdlen = remainder;
        cactx->req = req;
        _in = &cactx->up.in;
        _in->nent = 1 + in_info.ind + 1;
        _in->ents = (te_mement_t *)osal_calloc(_in->nent , sizeof(te_mement_t));

        if (NULL == _in->ents) {
            ret = TE_ERROR_OOM;
            goto err3;
        }
        if (0 < prv_ctx->npdlen) {
            _in->ents[0].buf = prv_ctx->npdata;
            _in->ents[0].len = prv_ctx->npdlen;
            _in->nent = 1;
        } else {
            _in->nent = 0;
        }

        memcpy(_in->ents + _in->nent,
                in->ents,
                (in_info.ind + 1)* sizeof(te_mement_t));

        _in->nent += in_info.ind + 1;
        _in->ents[_in->nent - 1].len = in_info.offset;
        sreq->base.completion = cbcmac_aupdate_done;
        sreq->base.data = (void *)cactx;
        cactx->up.cctx = ctx;
        sreq->up.flags = SCA_FLAGS_LIST;
        sreq->up.lst.src.nent = _in->nent;
        sreq->up.lst.src.ents = _in->ents;
    }
    ret = te_sca_aupdate( ctx->crypt, sreq );
    if (ret != TE_SUCCESS) {
        goto err4;
    }

    return TE_SUCCESS;
err4:
    osal_free( _in->ents );
err3:
    osal_free( cactx );
err2:
    osal_free( sreq );
err1:
    return ret;
}

int te_cbcmac_afinish( te_cbcmac_ctx_t *ctx, te_cmac_request_t *req )
{
    int ret = TE_SUCCESS;
    sca_cbcmac_ctx_t *prv_ctx = NULL;
    te_sca_request_t *sreq = NULL;

    if( !ctx || !ctx->crypt || !req || !req->fin.mac ) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }

    prv_ctx = (sca_cbcmac_ctx_t *)cbcmac_priv_ctx(ctx);

    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }

    if( prv_ctx->npdlen ) {
        ret = te_sca_update(ctx->crypt,
                            true,
                            prv_ctx->npdlen,
                            prv_ctx->npdata,
                            NULL);
        if ( ret != TE_SUCCESS ) {
            goto err1;
        }
        prv_ctx->npdlen = 0;
    }

    sreq = (te_sca_request_t *)osal_calloc( 1, sizeof(te_sca_request_t) );
    if ( !sreq ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }
    sreq->fin.tag = req->fin.mac;
    sreq->fin.taglen = req->fin.maclen;
    sreq->base.completion = cbcmac_async_generic_done;
    sreq->base.data = (void *)req;

    ret = te_sca_afinish(ctx->crypt, sreq);
    if ( ret != TE_SUCCESS ) {
        goto err2;
    }

    return TE_SUCCESS;

err2:
    osal_free( sreq );
err1:
    return ret;
}

static void acbcmac_start_done( te_async_request_t *r, int err )
{
    int ret = err;
    acbcmac_ctx_t *cactx = (acbcmac_ctx_t *)r->data;
    te_cmac_request_t *req = cactx->req;
    te_cmac_request_t *update = &cactx->amac.update;
    te_cbcmac_ctx_t *cctx = &cactx->amac.cctx;

    if ( ret != TE_SUCCESS ) {
        goto err1;
    }

    ret = te_cbcmac_aupdate( cctx, update );
    if ( ret != TE_SUCCESS ) {
        goto err1;
    }

    return;

err1:
    te_cbcmac_free( cctx );
    osal_free( cactx );
    req->res = ret;
    req->base.completion( &req->base, req->res );
    return;
}

static void acbcmac_update_done( te_async_request_t *r, int err )
{
    int ret = err;
    acbcmac_ctx_t *cactx = (acbcmac_ctx_t *)r->data;
    te_cmac_request_t *req = cactx->req;
    te_cmac_request_t *finish = &cactx->amac.finish;
    te_cbcmac_ctx_t *cctx = &cactx->amac.cctx;

    if ( ret != TE_SUCCESS ) {
        goto err1;
    }

    ret = te_cbcmac_afinish( cctx, finish );
    if ( ret != TE_SUCCESS ) {
        goto err1;
    }

    return;

err1:
    te_cbcmac_free( cctx );
    osal_free( cactx );
    req->res = ret;
    req->base.completion( &req->base, req->res );
    return;
}

static void acbcmac_finish_done( te_async_request_t *r, int err )
{
    acbcmac_ctx_t *cactx = (acbcmac_ctx_t *)r->data;
    te_cmac_request_t *req = cactx->req;
    te_cbcmac_ctx_t *cctx = &cactx->amac.cctx;

    te_cbcmac_free( cctx );
    osal_free( cactx );

    req->res = err;
    req->base.completion( &req->base, req->res );
    return;
}

/* malg = DES | TDES | AES | SM4 */
int te_acbcmac( te_drv_handle hdl,
                te_algo_t malg,
                te_cmac_request_t *req )
{
    int ret = TE_ERROR_GENERIC;
    acbcmac_ctx_t *cactx = NULL;
    te_cmac_request_t *start = NULL, *update = NULL, *finish = NULL;
    te_sca_key_t key_desc = { 0 };

    if ( !req || !req->amac.mac || !req->amac.in.ents || !req->amac.iv ||
         (req->amac.key.type != TE_KEY_TYPE_USER &&
         req->amac.key.type != TE_KEY_TYPE_SEC)) {

        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }

    cactx = (acbcmac_ctx_t *)osal_calloc( 1, sizeof(acbcmac_ctx_t) );
    if ( !cactx ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    start = &cactx->amac.start;
    start->st.iv = req->amac.iv;
    start->base.completion = acbcmac_start_done;
    start->base.data = (void *)cactx;

    update = &cactx->amac.update;
    update->up.in.nent = req->amac.in.nent;
    update->up.in.ents = req->amac.in.ents;
    update->base.completion = acbcmac_update_done;
    update->base.data = (void *)cactx;

    finish = &cactx->amac.finish;
    finish->fin.mac = req->amac.mac;
    finish->fin.maclen = req->amac.maclen;
    finish->base.completion = acbcmac_finish_done;
    finish->base.data = (void *)cactx;
    cactx->req = req;

    ret = te_cbcmac_init( &cactx->amac.cctx, hdl, malg );
    if ( ret != TE_SUCCESS ) {
        goto err2;
    }

    key_desc.type = req->amac.key.type;
    if ( key_desc.type == TE_KEY_TYPE_USER ) {
        key_desc.user.key = req->amac.key.user.key;
        key_desc.user.keybits = req->amac.key.user.keybits;
    } else {
        memcpy( &key_desc.sec,
                &req->amac.key.sec,
                sizeof(te_sec_key_t) );
    }

    ret = te_sca_setkey( cactx->amac.cctx.crypt, &key_desc );
    if ( ret != TE_SUCCESS ) {
        goto err3;
    }

    ret = te_cbcmac_astart( &cactx->amac.cctx, start );
    if ( ret != TE_SUCCESS ) {
        goto err3;
    }

    return TE_SUCCESS;

err3:
    te_cbcmac_free( &cactx->amac.cctx );
err2:
    osal_free( cactx );
err1:
    return ret;
}
#endif /* CFG_TE_ASYNC_EN */
