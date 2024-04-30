//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_gfmul.h>

/**
 * SCA GFmul private context
 * GFmul engine accepts complete blocks only
 */
typedef struct sca_gfmul_ctx {
    uint8_t npdata[SCA_MAX_BLOCK_SIZE]; /**< not processed data */
    uint32_t npdlen;                    /**< data length in byte */
} sca_gfmul_ctx_t;

#define GHASH_EHDR_SIZE(x)   (sizeof(gfmul_ehdr_t) + (x)->drvctx_sz)
#define GHASH_EHDR_DRVCTX(x) (uint8_t *)(((gfmul_ehdr_t *)(x)) + 1)

/**
 * GHASH export state header magic number
 */
#define GHASH_EHDR_MAGIC     0x48456847U /**< "GhEH" */

/**
 * GHASH export state header structure
 */
typedef struct gfmul_export_hdr {
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
} gfmul_ehdr_t;

#define __GHASH_OUT__    goto __out__

#define __GHASH_CHECK_CONDITION__(_ret_)                                       \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 __GHASH_OUT__;                                                \
            }                                                                  \
        } while (0);

#define __GHASH_ALERT__(_ret_, _msg_)                                          \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X %s\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_),                                       \
                                (_msg_));                                      \
            }                                                                  \
        } while (0);


int te_gfmul_init( te_gfmul_ctx_t *ctx, te_drv_handle hdl )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;

    if(NULL == ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __GHASH_OUT__;
    }

    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    ret = te_sca_alloc_ctx(sdrv,
                           TE_MAIN_ALGO_GFMUL,
                           sizeof(sca_gfmul_ctx_t),
                           &ctx->crypt);
    ctx->crypt->alg = TE_ALG_GFMUL_CBC_MAC_NOPAD;
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
    __GHASH_CHECK_CONDITION__(ret);

__out__:
    return ret;
}

int te_gfmul_free( te_gfmul_ctx_t *ctx )
{
    int ret = TE_SUCCESS;

    if(NULL == ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __GHASH_OUT__;
    }
    ret = te_sca_state(ctx->crypt);
    switch (ret){
    default:
        __GHASH_OUT__;
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        ret = TE_SUCCESS;
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __GHASH_ALERT__(ret, "te_sca_finish raised exceptions!");
        break;
    }

    te_sca_free_ctx(ctx->crypt);
    ctx->crypt = NULL;
__out__:
    return ret;
}

int te_gfmul_setkey( te_gfmul_ctx_t *ctx,
                     const uint8_t *key, size_t keybits )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};

    if((NULL == ctx) || (NULL == key)){
        ret = TE_ERROR_BAD_PARAMS;
        __GHASH_OUT__;
    }

    switch (te_sca_state(ctx->crypt)){
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        __GHASH_OUT__;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __GHASH_CHECK_CONDITION__(ret);
        break;
    }
    key_desc.type = TE_KEY_TYPE_USER;
    key_desc.user.key = (uint8_t *)key;
    key_desc.user.keybits = keybits;
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    __GHASH_CHECK_CONDITION__(ret);

__out__:
    return ret;
}

int te_gfmul_start( te_gfmul_ctx_t *ctx, uint8_t *iv )
{
    int ret = TE_SUCCESS;

    if(NULL == ctx || NULL == ctx->crypt){
        ret = TE_ERROR_BAD_PARAMS;
        __GHASH_OUT__;
    }

    ret = te_sca_start( ctx->crypt,
                        TE_DRV_SCA_ENCRYPT,
                        iv,
                        ctx->crypt->blk_size );
__out__:
    return ret;
}

int te_gfmul_update( te_gfmul_ctx_t *ctx,
                     size_t len,
                     const uint8_t *in )
{
    int ret = TE_SUCCESS;
    sca_gfmul_ctx_t *prv_ctx = NULL;
    size_t remainder = 0;
    size_t _len = 0;

    if(NULL == ctx || NULL == ctx->crypt){
        ret = TE_ERROR_BAD_PARAMS;
        __GHASH_OUT__;
    }

    if(NULL == in){
        ret = TE_ERROR_BAD_INPUT_DATA;
        __GHASH_OUT__;
    }

    if(0 == len){
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        __GHASH_OUT__;
    }

    prv_ctx = (sca_gfmul_ctx_t *)gfmul_priv_ctx(ctx);

    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __GHASH_OUT__;
    }

    /* still data left? */
    if(0 < prv_ctx->npdlen){
        _len = (len > (ctx->crypt->blk_size - prv_ctx->npdlen)) \
               ? (ctx->crypt->blk_size - prv_ctx->npdlen) : len;
        osal_memcpy(prv_ctx->npdata + prv_ctx->npdlen, in, _len);
        len -= _len;
        in += _len;
        prv_ctx->npdlen += _len;
        /* if fullfill npdata then update npdata */
        if(ctx->crypt->blk_size == prv_ctx->npdlen){
            ret = te_sca_update(ctx->crypt,
                                false,
                                prv_ctx->npdlen,
                                prv_ctx->npdata,
                                NULL);
            __GHASH_CHECK_CONDITION__(ret);
            prv_ctx->npdlen = 0;
        }
    }

    remainder = len % ctx->crypt->blk_size;
    len -= remainder;

    if( 0 < len){
        ret = te_sca_update(ctx->crypt,
                                false,
                                len,
                                in,
                                NULL);
        __GHASH_CHECK_CONDITION__(ret);
    }

    if(0 < remainder){
        prv_ctx->npdlen = remainder;
        osal_memcpy(prv_ctx->npdata,
              in + len,
              remainder);
    }

__out__:
    return ret;
}


int te_gfmul_uplist( te_gfmul_ctx_t *ctx,
                     te_memlist_t *in )
{
    int ret = TE_SUCCESS;
    sca_gfmul_ctx_t *prv_ctx = NULL;
    size_t _i = 0;
    size_t remainder = 0;
    te_memlist_t _in = {0};
    size_t _total_size = 0;
    uint8_t _tmp_buf[SCA_MAX_BLOCK_SIZE] = {0};
    te_ml_bp_t in_info = {0};

    if(NULL == ctx || NULL == ctx->crypt){
        ret = TE_ERROR_BAD_PARAMS;
        __GHASH_OUT__;
    }

    if(in && in->nent && !in->ents){
        ret = TE_ERROR_BAD_INPUT_DATA;
        __GHASH_OUT__;
    }

    if(!in || 0 == in->nent){
        ret = TE_SUCCESS;
        __GHASH_OUT__;
    }

    prv_ctx = (sca_gfmul_ctx_t *)gfmul_priv_ctx(ctx);
    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __GHASH_OUT__;
    }

    _total_size = prv_ctx->npdlen;

    for ( _i = 0; _i < in->nent; _i++) {
        _total_size += in->ents[_i].len;
    }

    if(_total_size <= ctx->crypt->blk_size){
        /*just copy to npdata and then out*/
        for ( _i = 0; _i < in->nent; _i++) {
            osal_memcpy(prv_ctx->npdata + prv_ctx->npdlen,
                   in->ents[_i].buf,
                   in->ents[_i].len);
            prv_ctx->npdlen += in->ents[_i].len;
        }
    }else{
        remainder = _total_size % ctx->crypt->blk_size;
        te_memlist_truncate_from_tail(in, _tmp_buf, remainder,
                                             true, &in_info);
        _in.nent = 1 + in_info.ind + 1;
        _in.ents = (te_mement_t *)osal_calloc(_in.nent , sizeof(te_mement_t));

        if(NULL == _in.ents){
            in->nent = in_info.nent;
            in->ents[in_info.ind].len = in_info.len;
            ret = TE_ERROR_OOM;
            __GHASH_OUT__;
        }

        if (0 < prv_ctx->npdlen) {
            _in.ents[0].buf = prv_ctx->npdata;
            _in.ents[0].len = prv_ctx->npdlen;
            _in.nent = 1;
        } else {
            _in.nent = 0;
        }
        /** when in_info.ind < 0, that means no data left after cut */
        if (in_info.ind >= 0) {
            osal_memcpy( _in.ents + _in.nent,
                        in->ents,
                        (in_info.ind + 1) * sizeof(te_mement_t) );
            _in.nent += in_info.ind + 1;
            _in.ents[_in.nent - 1].len = in_info.offset;
        }

        ret = te_sca_uplist(ctx->crypt, false, &_in, NULL);
        in->nent = in_info.nent;
        if (in_info.offset) {
            in->ents[in_info.ind].len = in_info.len;
        }
        osal_free(_in.ents);
        _in.ents = NULL;
        __GHASH_CHECK_CONDITION__(ret);
        prv_ctx->npdlen = 0;

        /* update npdata */
        if(0 < remainder){
            osal_memcpy(prv_ctx->npdata, _tmp_buf, remainder);
            prv_ctx->npdlen = remainder;
        }
    }

__out__:
    return ret;
}

int te_gfmul_finish( te_gfmul_ctx_t *ctx,
                     uint8_t *mac,
                     uint32_t maclen )
{
    int ret = TE_SUCCESS;
    sca_gfmul_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || ((maclen > 0) && (NULL == mac))){
        ret = TE_ERROR_BAD_PARAMS;
        __GHASH_OUT__;
    }

    prv_ctx = (sca_gfmul_ctx_t *)gfmul_priv_ctx(ctx);

    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __GHASH_OUT__;
    }

    if(0 < prv_ctx->npdlen){
        ret = te_sca_update(ctx->crypt,
                            true,
                            prv_ctx->npdlen,
                            prv_ctx->npdata,
                            NULL);
        __GHASH_CHECK_CONDITION__(ret);
        prv_ctx->npdlen = 0;
    }

    ret = te_sca_finish(ctx->crypt, mac, maclen);
__out__:
    return ret;
}

int te_gfmul_reset( te_gfmul_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    sca_gfmul_ctx_t *prv_ctx = NULL;
    uint8_t *iv = NULL;

    if(NULL == ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __GHASH_OUT__;
    }
    /* we can well slip out if the calling ctx is in START state */
    if (TE_DRV_SCA_STATE_START == te_sca_state(ctx->crypt)) {
        ret = TE_SUCCESS;
        __GHASH_OUT__;
    }
    iv = (uint8_t *)osal_calloc(ctx->crypt->blk_size, 1);
    if (!iv) {
        ret = TE_ERROR_OOM;
        __GHASH_OUT__;
    }
    ret = te_sca_reset(ctx->crypt);
    __GHASH_CHECK_CONDITION__(ret);
    prv_ctx = (sca_gfmul_ctx_t *)gfmul_priv_ctx(ctx);

    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_FORMAT;
        __GHASH_OUT__;
    }
    memset(prv_ctx, 0x00, sizeof(*prv_ctx));
    ret = te_sca_start( ctx->crypt, TE_DRV_SCA_ENCRYPT, iv,
                        ctx->crypt->blk_size );
__out__:
    OSAL_SAFE_FREE(iv);
    return ret;
}

int te_gfmul_clone( const te_gfmul_ctx_t *src,
                    te_gfmul_ctx_t *dst )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;
    sca_gfmul_ctx_t *spctx = NULL;
    sca_gfmul_ctx_t *dpctx = NULL;

    if (NULL == src || NULL == src->crypt || NULL == dst) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * free the dst ctx if it points to a valid gfmul ctx already
     */
    if (dst->crypt != NULL) {
        ret = te_gfmul_free(dst);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_gfmul_free error %x\n", ret);
            return ret;
        }
    }

    spctx = (sca_gfmul_ctx_t *)gfmul_priv_ctx((te_gfmul_ctx_t*)src);
    if(NULL == spctx){
        return TE_ERROR_BAD_PARAMS;
    }

    sdrv = (te_sca_drv_t *)src->crypt->drv;
    ret = te_sca_alloc_ctx(sdrv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_gfmul_ctx_t),
                           &dst->crypt);
    if (ret != TE_SUCCESS) {
        return ret;
    }

    dpctx = (sca_gfmul_ctx_t *)gfmul_priv_ctx(dst);
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

int te_gfmul_export( te_gfmul_ctx_t *ctx,
                     void *out,
                     uint32_t *olen )
{
    int ret = TE_ERROR_GENERIC;
    sca_gfmul_ctx_t *pctx = NULL;
    gfmul_ehdr_t eh = {0};

    if ( NULL == ctx || NULL == ctx->crypt || NULL == olen ) {
        return TE_ERROR_BAD_PARAMS;
    }

    pctx = (sca_gfmul_ctx_t *)gfmul_priv_ctx(ctx);
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
    if (*olen < GHASH_EHDR_SIZE(&eh)) {
        *olen = GHASH_EHDR_SIZE(&eh);
        return TE_ERROR_SHORT_BUFFER;
    }

    if (NULL == out) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * export drvctx
     * TODO: lock the gfmul driver to stop service of update() or uplist() on
     * the calling context until te_sca_export() ends.
     * Or, it's the caller responsibility to ensure there be no update() or
     * uplist() call on to the same context when an export() is outstanding.
     */
    ret = te_sca_export(ctx->crypt, GHASH_EHDR_DRVCTX(out), &eh.drvctx_sz);
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR( "te_sca_export error %x\n", ret );
        goto err;
    }

    /*
     * make ehdr
     */
    eh.magic  = GHASH_EHDR_MAGIC;
    eh.npdlen = pctx->npdlen;
    osal_memcpy(eh.npdata, pctx->npdata, sizeof(eh.npdata));

    osal_memcpy(out, &eh, sizeof(eh));
    *olen = GHASH_EHDR_SIZE(&eh);
err:
    return ret;
}

int te_gfmul_import( te_gfmul_ctx_t *ctx,
                     const void *in,
                     uint32_t ilen )
{
    int ret = TE_ERROR_GENERIC;
    sca_gfmul_ctx_t *pctx = NULL;
    gfmul_ehdr_t eh = {0};

    if ( NULL == ctx || NULL == ctx->crypt || NULL == in ) {
        return TE_ERROR_BAD_PARAMS;
    }

    pctx = (sca_gfmul_ctx_t *)gfmul_priv_ctx(ctx);
    if(NULL == pctx){
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * The 'in' might not start at struct ptr safe boundary.
     * Be safe to copy the struct before reading it.
     */
    osal_memcpy(&eh, in, sizeof(eh));

    if (eh.magic != GHASH_EHDR_MAGIC ||
        ilen < GHASH_EHDR_SIZE(&eh)) {
        OSAL_LOG_ERR("Bad or mismatched gfmul ehdr: %d\n", ilen);
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * import drvctx
     */
    ret = te_sca_import(ctx->crypt, GHASH_EHDR_DRVCTX(in), eh.drvctx_sz );
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR("te_sca_import error %x\n", ret);
        return ret;
    }

    /*
     * import gfmul ctx
     */
    osal_memcpy(pctx->npdata, eh.npdata, sizeof(pctx->npdata));
    pctx->npdlen = eh.npdlen;

    return TE_SUCCESS;
}

