//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_gcm.h>
#include <te_gfmul.h>
#include "../common/te_worker_pool.h"

#define GCM_JIV_LEN     (32U)
#define IV_THRESHOLD    (12U)
#define GCTR_SIZE       (4U) /*gctr size is 32bits, thus 4bytes*/
#define GCM_MAX_COUNTER (0xFFFFFFFFULL)

#define __GCM_OUT__    goto __out__

#define __GCM_CHECK_CONDITION__(_ret_)                                         \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 __GCM_OUT__;                                                  \
            }                                                                  \
        } while (0);

#define __GCM_ALLERT__(_ret_, _msg_)                                           \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X %s\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_),                                       \
                                (_msg_));                                      \
            }                                                                  \
        } while (0);

#define __GCM_VERIFY_PARAMS__(_param_)                                         \
        do                                                                     \
        {                                                                      \
            if(NULL == (_param_)){                                             \
                ret = TE_ERROR_BAD_PARAMS;                                     \
                __GCM_OUT__;                                                   \
            }                                                                  \
        } while (0)

#define COUNTER_INCREASE(iv, bs, w, delta)                                     \
                do {                                                           \
                    size_t _i = 0, _j = 0;                                     \
                    for (_i = 0; _i < (delta); _i++) {                         \
                        for(_j = (bs); _j > ((bs) - (w)); _j--) {              \
                            if (0 != ++((iv)[_j - 1])) {                       \
                                break;                                         \
                            }                                                  \
                        }                                                      \
                    }                                                          \
                }while(0)

#define GCM_READ_COUNTER(iv) __extension__({                                   \
                    uint32_t _counter = 0;                                     \
                    TE_ASSERT(NULL != (iv));                                   \
                    _counter |= (iv)[15];                                      \
                    _counter |= (iv)[14] << 8;                                 \
                    _counter |= (iv)[13] << 16;                                \
                    _counter |= (iv)[12] << 24;                                \
                    _counter;                                                  \
                })

#define GCM_RESET_COUNTER(iv)                                                  \
                do {                                                           \
                    TE_ASSERT(NULL != (iv));                                   \
                    osal_memset((iv) + IV_THRESHOLD, 0x00, GCTR_SIZE);         \
                }while(0)

/**
 * SCA GCM private context
 */
#define DRV_SCA_MAX_PADDING_SIZE        (TE_MAX_SCA_BLOCK + 16U)
typedef struct sca_gcm_ctx {
    te_cipher_ctx_t cctx;                           /**< cipher sub-context */
    te_gfmul_ctx_t  gctx;                           /**< gfmul sub-context */
    te_sca_operation_t op;                          /**< operation mode */
    te_gcm_state_t state;                           /**< gcm state */
    uint64_t mlen;                                  /**< message length in byte */
    uint64_t aadlen;                                /**< aad length in byte */
    uint32_t ivlen;                                 /**< iv length in byte */
    uint8_t iv[16];                                 /**< initial vector */
    uint8_t h[16];                                  /**< hash subkey */
    union {
        struct {
            uint8_t j0[16];                         /**< pre-counter block */
            uint8_t ghash[16];                      /**< intermediate ghash */
        };
        uint8_t jiv[32];                            /**< joint iv: j0 || ghash */
    };
    uint8_t stream[16];                             /**< stream block */
    size_t strpos;                                  /**< stream block pos*/
    uint8_t padding[DRV_SCA_MAX_PADDING_SIZE];      /**< padding should always
                                                         0, don't modify */
    uint32_t dummy1 __te_dma_aligned;               /**< placeholder */
    uint8_t swap[TE_MAX_SCA_BLOCK] __te_dma_aligned;/**< unify use for
                                                         gen H-key and tag */
    uint32_t dummy2 __te_dma_aligned;               /**< placeholder */
} sca_gcm_ctx_t;

/**
 * malg = AES | SM4
 */
int te_gcm_init( te_gcm_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg )
{
    int ret = TE_SUCCESS;
    sca_gcm_ctx_t *prv_ctx = NULL;
    te_sca_drv_t *sdrv = NULL;

    if((NULL == ctx)
        || ((TE_MAIN_ALGO_AES != malg) && (TE_MAIN_ALGO_SM4 != malg))){
        ret = TE_ERROR_BAD_PARAMS;
        __GCM_OUT__;
    }

    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    ret = te_sca_alloc_ctx(sdrv, malg, sizeof(sca_gcm_ctx_t), &ctx->crypt);
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
    __GCM_CHECK_CONDITION__(ret);
    prv_ctx = (sca_gcm_ctx_t *)gcm_priv_ctx(ctx);
    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        goto __err1__;
    }

    ret = te_cipher_init(&prv_ctx->cctx, hdl, malg);
    if(TE_SUCCESS != ret){
        goto __err1__;
    }
    ret = te_gfmul_init(&prv_ctx->gctx, hdl);
    if(TE_SUCCESS == ret){
        prv_ctx->state = TE_GCM_STATE_INIT;
        if( TE_MAIN_ALGO_SM4 == malg){
            ctx->crypt->alg = TE_ALG_SM4_GCM;
        }else{
            ctx->crypt->alg = TE_ALG_AES_GCM;
        }
        __GCM_OUT__;
    }

    te_cipher_free(&prv_ctx->cctx);
__err1__:
    te_sca_free_ctx(ctx->crypt);
    ctx->crypt = NULL;
__out__:
    return ret;
}

int te_gcm_free( te_gcm_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    sca_gcm_ctx_t *prv_ctx = NULL;
    __te_unused uint8_t tmp[SCA_MAX_BLOCK_SIZE] = {0};

    __GCM_VERIFY_PARAMS__(ctx);
    __GCM_VERIFY_PARAMS__(ctx->crypt);
    prv_ctx = (sca_gcm_ctx_t *)gcm_priv_ctx(ctx);
    __GCM_VERIFY_PARAMS__(prv_ctx);

    switch (prv_ctx->state){
    default:
        ret = TE_ERROR_BAD_STATE;
        __GCM_OUT__;
    case TE_GCM_STATE_RAW:
        __GCM_OUT__;
        break;
    case TE_GCM_STATE_READY:
    case TE_GCM_STATE_INIT:
        break;
    case TE_GCM_STATE_START:
    case TE_GCM_STATE_UPDATE:
        ret = te_gcm_finish(ctx, tmp, ctx->crypt->blk_size);
        __GCM_ALLERT__(ret, "te_gcm_finish raises exceptions!");
        break;
    }
    ret = te_gfmul_free(&prv_ctx->gctx);
    __GCM_ALLERT__(ret, "te_gfmul_free raises exceptions!");
    ret = te_cipher_free(&prv_ctx->cctx);
    __GCM_ALLERT__(ret, "te_cipher_free raises exceptions!");

    /* state change must before free, otherwise memory overwritten */
    prv_ctx->state = TE_GCM_STATE_RAW;
    ret = te_sca_free_ctx(ctx->crypt);
    __GCM_ALLERT__(ret, "te_sca_free_ctx raises exceptions!");
    ctx->crypt = NULL;

__out__:
    return ret;
}

int te_gcm_setkey( te_gcm_ctx_t *ctx,
                   const uint8_t *key,
                   uint32_t keybits )
{
    int ret = TE_SUCCESS;
    sca_gcm_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == key) || (NULL == ctx->crypt)){
        ret = TE_ERROR_BAD_PARAMS;
        __GCM_OUT__;
    }

    prv_ctx = (sca_gcm_ctx_t *)gcm_priv_ctx(ctx);
    __GCM_VERIFY_PARAMS__(prv_ctx);

    switch (prv_ctx->state){
    case TE_GCM_STATE_INIT:
    case TE_GCM_STATE_READY:
        break;
    default:
        ret = TE_ERROR_BAD_STATE;
        __GCM_OUT__;
    }

    ret = te_cipher_setkey(&prv_ctx->cctx, key, keybits);
    __GCM_CHECK_CONDITION__(ret);
    prv_ctx->state = TE_GCM_STATE_READY;
__out__:
    return ret;
}

/**
 * AES and SM4 only
 */
int te_gcm_setseckey( te_gcm_ctx_t *ctx,
                      te_sec_key_t *key )
{
    int ret = TE_SUCCESS;
    sca_gcm_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == key) || (NULL == ctx->crypt)){
        ret = TE_ERROR_BAD_PARAMS;
        __GCM_OUT__;
    }

    prv_ctx = (sca_gcm_ctx_t *)gcm_priv_ctx(ctx);
    __GCM_VERIFY_PARAMS__(prv_ctx);

    switch (prv_ctx->state){
    case TE_GCM_STATE_INIT:
    case TE_GCM_STATE_READY:
        break;
    default:
        ret = TE_ERROR_BAD_STATE;
        __GCM_OUT__;
    }

    ret = te_cipher_setseckey(&prv_ctx->cctx, key);
    __GCM_CHECK_CONDITION__(ret);
    prv_ctx->state = TE_GCM_STATE_READY;
__out__:
    return ret;
}

/**
 * AES only
 */
int te_gcm_setseckey_v2( te_gcm_ctx_t *ctx,
                         te_sec_key_v2_t *key )
{
    int ret = TE_SUCCESS;
    sca_gcm_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == key) || (NULL == ctx->crypt) ||
       (TE_ALG_AES_GCM != ctx->crypt->alg)){
        ret = TE_ERROR_BAD_PARAMS;
        __GCM_OUT__;
    }

    prv_ctx = (sca_gcm_ctx_t *)gcm_priv_ctx(ctx);
    __GCM_VERIFY_PARAMS__(prv_ctx);

    switch (prv_ctx->state){
    case TE_GCM_STATE_INIT:
    case TE_GCM_STATE_READY:
        break;
    default:
        ret = TE_ERROR_BAD_STATE;
        __GCM_OUT__;
    }

    ret = te_cipher_setseckey_v2(&prv_ctx->cctx, key);
    __GCM_CHECK_CONDITION__(ret);
    prv_ctx->state = TE_GCM_STATE_READY;
__out__:
    return ret;
}

int te_gcm_clone( const te_gcm_ctx_t *src,
                  te_gcm_ctx_t *dst )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;
    sca_gcm_ctx_t *spctx = NULL;
    sca_gcm_ctx_t *dpctx = NULL;

    if (NULL == src || NULL == src->crypt || NULL == dst) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * free the dst ctx if it points to a valid gcm ctx already
     */
    if (dst->crypt != NULL) {
        ret = te_gcm_free(dst);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_gcm_free error %x\n", ret);
            return ret;
        }
    }

    spctx = (sca_gcm_ctx_t *)gcm_priv_ctx((te_gcm_ctx_t*)src);
    if(NULL == spctx){
        return TE_ERROR_BAD_PARAMS;
    }

    sdrv = (te_sca_drv_t *)src->crypt->drv;
    ret = te_sca_alloc_ctx(sdrv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_gcm_ctx_t),
                           &dst->crypt);
    if (ret != TE_SUCCESS) {
        return ret;
    }

    dpctx = (sca_gcm_ctx_t *)gcm_priv_ctx(dst);
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
     * clone private ctx except sub-contexts
     */
    osal_memcpy(dpctx, spctx, sizeof(*dpctx));
    osal_memset(&dpctx->cctx, 0, sizeof(dpctx->cctx));
    osal_memset(&dpctx->gctx, 0, sizeof(dpctx->gctx));

    /*
     * clone sub-contexts
     */
    ret = te_cipher_clone(&spctx->cctx, &dpctx->cctx);
    if (ret != TE_SUCCESS) {
        te_gcm_free(dst);
        return ret;
    }
    ret = te_gfmul_clone(&spctx->gctx, &dpctx->gctx);
    if (ret != TE_SUCCESS) {
        te_gcm_free(dst);
        return ret;
    }

    return TE_SUCCESS;
}

static void _te_gcm_read_size(uint8_t *dst, size_t ofs,  uint64_t size)
{
    size_t i = 0;

    for (i = 0; i < sizeof(size); i++) {
        dst[ofs- i] = (size >> (i * 8)) & 0xFF;
    }
}

static int _te_gcm_format_j0(uint8_t *j0,
                             sca_gcm_ctx_t *ctx,
                             uint8_t *iv,
                             uint64_t ivlen)
{
    int ret = TE_SUCCESS;
    size_t blk_size = 0;
    size_t padding_sz = 0;

    TE_ASSERT(NULL != ctx);
    TE_ASSERT(NULL != ctx->gctx.crypt);
    if( IV_THRESHOLD == ivlen){
        osal_memcpy(j0, iv, ivlen);
        osal_memset(j0 + ivlen, 0x00, 4);
        j0[15] |= 1;
        __GCM_OUT__;
    }else{
        blk_size = ctx->gctx.crypt->blk_size;
        osal_memset(ctx->ghash, 0x00, sizeof(ctx->ghash));
        ret = te_gfmul_start(&ctx->gctx, ctx->ghash);
        __GCM_CHECK_CONDITION__(ret);
        ret = te_gfmul_update(&ctx->gctx, ivlen, iv);
        if (TE_SUCCESS != ret) {
            goto err;
        }
        /** be careful we need second modulus here,
         *  in case of ivlen % blk_size = 0 */
        blk_size = ctx->gctx.crypt->blk_size;
        padding_sz = ((blk_size - (ivlen % blk_size)) % blk_size) + 8;
        osal_memset(ctx->padding, 0x00, padding_sz);
        ret = te_gfmul_update(&ctx->gctx, padding_sz, ctx->padding);
        if (TE_SUCCESS != ret) {
            goto err;
        }
        _te_gcm_read_size(ctx->padding, 7, ivlen * 8);
        ret = te_gfmul_update(&ctx->gctx, 8, ctx->padding);
        if (TE_SUCCESS != ret) {
            goto err;
        }
        ret = te_gfmul_finish(&ctx->gctx, j0, blk_size);
        __GCM_OUT__;
    }
err:
    te_gfmul_finish(&ctx->gctx, NULL, 0);
__out__:
    return ret;
}

int te_gcm_start( te_gcm_ctx_t *ctx,
                  te_sca_operation_t op,
                  uint8_t *iv,
                  uint64_t ivlen )
{
    int ret = TE_SUCCESS;
    sca_gcm_ctx_t *prv_ctx = NULL;

    if((NULL == ctx)
        || (NULL == ctx->crypt)
        || ((TE_DRV_SCA_ENCRYPT != op) && (TE_DRV_SCA_DECRYPT != op))
        || (NULL == iv)){
        ret = TE_ERROR_BAD_PARAMS;
        __GCM_OUT__;
    }

    if (!ivlen || ((uint64_t) ivlen) >> 61 != 0) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __GCM_OUT__;
    }

    prv_ctx = (sca_gcm_ctx_t *)gcm_priv_ctx(ctx);
    __GCM_VERIFY_PARAMS__(prv_ctx);
    switch (prv_ctx->state){
    default:
    case TE_GCM_STATE_RAW:
    case TE_GCM_STATE_INIT:
    case TE_GCM_STATE_START:
    case TE_GCM_STATE_UPDATE:
        ret = TE_ERROR_BAD_STATE;
        __GCM_OUT__;
    case TE_GCM_STATE_READY:
        switch (TE_ALG_GET_MAIN_ALG(ctx->crypt->alg)) {
            case TE_MAIN_ALGO_AES:
                prv_ctx->cctx.crypt->alg = TE_ALG_AES_ECB_NOPAD;
                break;
            case TE_MAIN_ALGO_SM4:
                prv_ctx->cctx.crypt->alg = TE_ALG_SM4_ECB_NOPAD;
                break;
            default:
                ret = TE_ERROR_BAD_PARAMS;
                __GCM_OUT__;
        }
        osal_memset(prv_ctx->padding, 0x00, prv_ctx->cctx.crypt->blk_size);
        ret = te_cipher_ecb(&prv_ctx->cctx,
                            TE_DRV_SCA_ENCRYPT,
                            prv_ctx->cctx.crypt->blk_size,
                            prv_ctx->padding,/**use padding, save malloc op */
                            prv_ctx->swap);
        __GCM_CHECK_CONDITION__(ret);
        osal_memcpy(prv_ctx->h, prv_ctx->swap, prv_ctx->cctx.crypt->blk_size);
        ret = te_gfmul_setkey(&prv_ctx->gctx, (const uint8_t *)prv_ctx->h,
                                sizeof(prv_ctx->h) * 8);
        __GCM_CHECK_CONDITION__(ret);
        ret = _te_gcm_format_j0(prv_ctx->j0,
                                prv_ctx,
                                iv, ivlen);
        __GCM_CHECK_CONDITION__(ret);
        osal_memset(prv_ctx->ghash, 0x00, sizeof(prv_ctx->ghash));
        ret = te_gfmul_start(&prv_ctx->gctx, prv_ctx->ghash);
        __GCM_CHECK_CONDITION__(ret);
        /* update IV to j0 + 1 */
        osal_memcpy(prv_ctx->iv, prv_ctx->j0, ctx->crypt->blk_size);
        COUNTER_INCREASE( prv_ctx->iv, ctx->crypt->blk_size, GCTR_SIZE, 1 );
        prv_ctx->mlen = 0;
        prv_ctx->aadlen = 0;
        prv_ctx->op = op;
        prv_ctx->strpos = 0;
        prv_ctx->state = TE_GCM_STATE_START;
        break;
    }
__out__:
    return ret;
}

int te_gcm_update_aad(te_gcm_ctx_t *ctx, const uint8_t *data, size_t len)
{
    int ret = TE_SUCCESS;
    sca_gcm_ctx_t *prv_ctx = NULL;

    if (NULL == ctx || NULL == ctx->crypt) {
        ret = TE_ERROR_BAD_PARAMS;
        __GCM_OUT__;
    }

    prv_ctx = gcm_priv_ctx(ctx);
    TE_ASSERT(prv_ctx != NULL);
    if ((!data && len) || ((uint64_t)(prv_ctx->aadlen + len) >> 61 != 0)) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __GCM_OUT__;
    }

    if (len == 0) {
        return TE_SUCCESS;
    }
    switch (prv_ctx->state)
    {
    case TE_GCM_STATE_START:
        ret = te_gfmul_update(&prv_ctx->gctx, len, data);
        __GCM_CHECK_CONDITION__(ret);
        prv_ctx->aadlen += len;
        break;
    default:
        ret = TE_ERROR_BAD_STATE;
        __GCM_OUT__;
    }

__out__:
    return ret;
}

int te_gcm_uplist_aad(te_gcm_ctx_t *ctx, te_memlist_t *in)
{
    int ret = TE_SUCCESS;
    sca_gcm_ctx_t *prv_ctx = NULL;
    size_t len = 0;

    if (NULL == ctx || NULL == ctx->crypt) {
        ret = TE_ERROR_BAD_PARAMS;
        __GCM_OUT__;
    }
    if (in && in->nent && !in->ents) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __GCM_OUT__;
    }

    prv_ctx = gcm_priv_ctx(ctx);
    TE_ASSERT(prv_ctx != NULL);

    if (in && in->nent) {
        len = te_memlist_get_total_len(in);
        if (0 != ((uint64_t)(prv_ctx->aadlen + len) >> 61)) {
            ret = TE_ERROR_BAD_INPUT_DATA;
            __GCM_OUT__;
        }
    }

    if ((NULL == in) || (0 == len)) {
        ret = TE_SUCCESS;
        __GCM_OUT__;
    }
    switch (prv_ctx->state)
    {
    case TE_GCM_STATE_START:
        ret = te_gfmul_uplist(&prv_ctx->gctx, in);
        __GCM_CHECK_CONDITION__(ret);
        prv_ctx->aadlen += len;
        break;
    default:
        ret = TE_ERROR_BAD_STATE;
        __GCM_OUT__;
    }

__out__:
    return ret;
}

static int _te_gcm_finish_aad(sca_gcm_ctx_t *ctx)
{
    int ret = TE_SUCCESS;
    size_t blk_size = 0;
    size_t padding_sz = 0;

    TE_ASSERT(NULL != ctx);
    TE_ASSERT(NULL != ctx->gctx.crypt);
    if (ctx->aadlen) {
        blk_size = ctx->gctx.crypt->blk_size;
        padding_sz = (blk_size - (ctx->aadlen % blk_size)) % blk_size;

        if(0 < padding_sz){
            osal_memset(ctx->padding, 0x00, padding_sz);
            ret = te_gfmul_update(&ctx->gctx, padding_sz, ctx->padding);
            if (TE_SUCCESS != ret) {
                __GCM_OUT__;
            }
        }
    }
    ctx->state = TE_GCM_STATE_UPDATE;
__out__:
    return ret;
}

static inline int gcm_sub_update( te_gcm_ctx_t *ctx,
                                  uint64_t len,
                                  const uint8_t *in,
                                  uint8_t *out )
{
    int ret = TE_SUCCESS;
    uint64_t offset = 0;
    uint64_t proc_len = 0;
    sca_gcm_ctx_t *prv_ctx = NULL;
    uint8_t ctr_iv[SCA_MAX_BLOCK_SIZE] = {0};

    TE_ASSERT(NULL != ctx);
    TE_ASSERT(NULL != in);
    TE_ASSERT(NULL != out);

    prv_ctx = (sca_gcm_ctx_t *)gcm_priv_ctx(ctx);
    __GCM_VERIFY_PARAMS__(prv_ctx);
    __GCM_VERIFY_PARAMS__(ctx->crypt);

    while (offset < len) {
        proc_len = (GCM_MAX_COUNTER - GCM_READ_COUNTER(prv_ctx->iv) + 1)
                    * ctx->crypt->blk_size;
        if ( 0 < prv_ctx->strpos ) {
            proc_len += ctx->crypt->blk_size - prv_ctx->strpos;
        }
        if ((len - offset) < proc_len) {
            proc_len = len - offset;
        }
        osal_memcpy(ctr_iv, prv_ctx->iv, ctx->crypt->blk_size);
        if(TE_DRV_SCA_ENCRYPT == prv_ctx->op){
            ret = te_cipher_ctr(&prv_ctx->cctx,
                                proc_len,
                                &prv_ctx->strpos,
                                ctr_iv,
                                prv_ctx->stream,
                                in + offset,
                                out + offset);
            __GCM_CHECK_CONDITION__(ret);
            ret = te_gfmul_update(&prv_ctx->gctx, proc_len, out + offset);
            __GCM_CHECK_CONDITION__(ret);
        }else{
            ret = te_gfmul_update(&prv_ctx->gctx, proc_len, in + offset);
            __GCM_CHECK_CONDITION__(ret);
            ret = te_cipher_ctr(&prv_ctx->cctx,
                                proc_len,
                                &prv_ctx->strpos,
                                ctr_iv,
                                prv_ctx->stream,
                                in + offset,
                                out + offset);
            __GCM_CHECK_CONDITION__(ret);
        }
        offset += proc_len;
        /*right-most 32 sinificant bits valid for gcm */
        if (offset < len) {
            GCM_RESET_COUNTER(prv_ctx->iv);
        } else {
            osal_memcpy(prv_ctx->iv + IV_THRESHOLD,
                        ctr_iv + IV_THRESHOLD,
                        ctx->crypt->blk_size - IV_THRESHOLD);
        }
    }
__out__:
    return ret;
}

int te_gcm_update( te_gcm_ctx_t *ctx,
                   size_t len,
                   const uint8_t *in,
                   uint8_t *out )
{
    int ret = TE_SUCCESS;
    sca_gcm_ctx_t *prv_ctx = NULL;

    if(NULL == ctx || NULL == ctx->crypt){
        ret = TE_ERROR_BAD_PARAMS;
        __GCM_OUT__;
    }

    if((!in && len) || (in && !out)){
        ret = TE_ERROR_BAD_INPUT_DATA;
        __GCM_OUT__;
    }
    prv_ctx = (sca_gcm_ctx_t *)gcm_priv_ctx(ctx);
    __GCM_VERIFY_PARAMS__(prv_ctx);
    switch (prv_ctx->state){
    default:
    case TE_GCM_STATE_RAW:
    case TE_GCM_STATE_INIT:
    case TE_GCM_STATE_READY:
        ret = TE_ERROR_BAD_STATE;
        __GCM_OUT__;
    case TE_GCM_STATE_START:
        ret = _te_gcm_finish_aad(prv_ctx);
        __GCM_CHECK_CONDITION__(ret);
        break;
    case TE_GCM_STATE_UPDATE:
        break;
    }
    if(0 == len){
        ret = TE_SUCCESS;
        __GCM_OUT__;
    }
    if (((uint64_t) prv_ctx->mlen + len > 0xFFFFFFFE0ull)
         || (prv_ctx->mlen > (prv_ctx->mlen + len))) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __GCM_OUT__;
    }
    switch (TE_ALG_GET_MAIN_ALG(ctx->crypt->alg)) {
        case TE_MAIN_ALGO_AES:
            prv_ctx->cctx.crypt->alg = TE_ALG_AES_CTR;
            break;
        case TE_MAIN_ALGO_SM4:
            prv_ctx->cctx.crypt->alg = TE_ALG_SM4_CTR;
            break;
        default:
            ret = TE_ERROR_BAD_PARAMS;
            __GCM_OUT__;
    }
    ret = gcm_sub_update(ctx, len, in, out);
    __GCM_CHECK_CONDITION__(ret);
    prv_ctx->mlen += len;
__out__:
    return ret;
}

static inline int gcm_sub_uplist( te_gcm_ctx_t *ctx,
                                  te_memlist_t *in,
                                  te_memlist_t *out )
{
    int ret = TE_SUCCESS;
    uint64_t offset = 0;
    uint64_t proc_len = 0;
    sca_gcm_ctx_t *prv_ctx = NULL;
    te_memlist_t cp_in = {0};
    te_memlist_t cp_out = {0};
    size_t len = 0;
    uint8_t ctr_iv[SCA_MAX_BLOCK_SIZE] = {0};

    TE_ASSERT(NULL != ctx);
    TE_ASSERT(NULL != in);
    TE_ASSERT(NULL != out);

    prv_ctx = (sca_gcm_ctx_t *)gcm_priv_ctx(ctx);
    __GCM_VERIFY_PARAMS__(prv_ctx);
    __GCM_VERIFY_PARAMS__(ctx->crypt);
    len = te_memlist_get_total_len(in);

    while (offset < len) {
        proc_len = ((GCM_MAX_COUNTER - GCM_READ_COUNTER(prv_ctx->iv) + 1)
                    * ctx->crypt->blk_size);
        if ( 0 < prv_ctx->strpos ) {
            proc_len += ctx->crypt->blk_size - prv_ctx->strpos;
        }
        if ((len - offset) < proc_len) {
            proc_len = len - offset;
        }
        ret = te_memlist_clone(&cp_in, in, offset, proc_len);
        if (TE_SUCCESS != ret) {
            __GCM_OUT__;
        }
        ret = te_memlist_clone(&cp_out, out, offset, proc_len);
        if (TE_SUCCESS != ret) {
            goto err_clone_out;
        }
        osal_memcpy(ctr_iv, prv_ctx->iv, ctx->crypt->blk_size);
        if(TE_DRV_SCA_ENCRYPT == prv_ctx->op){
            ret = te_cipher_ctr_list(&prv_ctx->cctx,
                                &prv_ctx->strpos,
                                ctr_iv,
                                prv_ctx->stream,
                                &cp_in,
                                &cp_out);
            if (TE_SUCCESS != ret) {
                goto err_ctr_list;
            }
            ret = te_gfmul_uplist(&prv_ctx->gctx, &cp_out);
            if (TE_SUCCESS != ret) {
                goto err_gfmul_list;
            }
        }else{
            ret = te_gfmul_uplist(&prv_ctx->gctx, &cp_in);
            if (TE_SUCCESS != ret) {
                goto err_gfmul_list;
            }
            ret = te_cipher_ctr_list(&prv_ctx->cctx,
                                &prv_ctx->strpos,
                                ctr_iv,
                                prv_ctx->stream,
                                &cp_in,
                                &cp_out);
            if (TE_SUCCESS != ret) {
                goto err_ctr_list;
            }
        }
        offset += proc_len;
        /*right-most 32 sinificant bits valid for gcm */
        if (offset < len) {
            GCM_RESET_COUNTER(prv_ctx->iv);
            OSAL_SAFE_FREE(cp_in.ents);
            osal_memset(&cp_in, 0x00, sizeof(cp_in));
            OSAL_SAFE_FREE(cp_out.ents);
            osal_memset(&cp_out, 0x00, sizeof(cp_out));
        } else {
            osal_memcpy(prv_ctx->iv + IV_THRESHOLD ,
                        ctr_iv + IV_THRESHOLD,
                        ctx->crypt->blk_size - IV_THRESHOLD);
        }
    }
err_ctr_list:
err_gfmul_list:
    OSAL_SAFE_FREE(cp_out.ents);
err_clone_out:
    OSAL_SAFE_FREE(cp_in.ents);
__out__:
    return ret;
}

int te_gcm_uplist( te_gcm_ctx_t *ctx,
                   te_memlist_t *in,
                   te_memlist_t *out )
{
    int ret = TE_SUCCESS;
    sca_gcm_ctx_t *prv_ctx = NULL;
    size_t len = 0;

    if(NULL == ctx || NULL == ctx->crypt){
        ret = TE_ERROR_BAD_PARAMS;
        __GCM_OUT__;
    }

    if(in && in->nent && !in->ents){
        ret = TE_ERROR_BAD_INPUT_DATA;
        __GCM_OUT__;
    }
    prv_ctx = (sca_gcm_ctx_t *)gcm_priv_ctx(ctx);
    __GCM_VERIFY_PARAMS__(prv_ctx);

    switch (prv_ctx->state){
    default:
    case TE_GCM_STATE_RAW:
    case TE_GCM_STATE_INIT:
    case TE_GCM_STATE_READY:
        ret = TE_ERROR_BAD_STATE;
        __GCM_OUT__;
    case TE_GCM_STATE_START:
        ret = _te_gcm_finish_aad(prv_ctx);
        __GCM_CHECK_CONDITION__(ret);
        break;
    case TE_GCM_STATE_UPDATE:
        break;
    }
    if(!in || 0 == in->nent){
        ret = TE_SUCCESS;
        __GCM_OUT__;
    }
    if (in && in->nent) {
        len = te_memlist_get_total_len(in);
        if (((uint64_t) prv_ctx->mlen + len > 0xFFFFFFFE0ull)
            || (prv_ctx->mlen > (prv_ctx->mlen + len))) {
            ret = TE_ERROR_BAD_INPUT_DATA;
            __GCM_OUT__;
        }
    }
    switch (TE_ALG_GET_MAIN_ALG(ctx->crypt->alg)) {
        case TE_MAIN_ALGO_AES:
            prv_ctx->cctx.crypt->alg = TE_ALG_AES_CTR;
            break;
        case TE_MAIN_ALGO_SM4:
            prv_ctx->cctx.crypt->alg = TE_ALG_SM4_CTR;
            break;
        default:
            ret = TE_ERROR_BAD_PARAMS;
            __GCM_OUT__;
    }

    ret = gcm_sub_uplist(ctx, in, out);
    __GCM_CHECK_CONDITION__(ret);

    prv_ctx->mlen += len;
    prv_ctx->state = TE_GCM_STATE_UPDATE;

__out__:
    return ret;
}

int te_gcm_finish( te_gcm_ctx_t *ctx,
                   uint8_t *tag,
                   uint32_t taglen )
{
    int ret = TE_SUCCESS;
    size_t padding_sz = 0;
    sca_gcm_ctx_t *prv_ctx = NULL;
    size_t _len = 0;
    size_t _nc_off = 0;
    uint8_t _stream[TE_MAX_SCA_BLOCK] = {0};

    __GCM_VERIFY_PARAMS__(ctx);
    __GCM_VERIFY_PARAMS__(ctx->crypt);
    __GCM_VERIFY_PARAMS__(tag);
    if ((taglen > ctx->crypt->blk_size) || 4 > taglen) {
        ret = TE_ERROR_BAD_PARAMS;
        __GCM_OUT__;
    }
    prv_ctx = (sca_gcm_ctx_t *)gcm_priv_ctx(ctx);
    __GCM_VERIFY_PARAMS__(prv_ctx);
    switch (prv_ctx->state){
    default:
    case TE_GCM_STATE_RAW:
    case TE_GCM_STATE_INIT:
    case TE_GCM_STATE_READY:
        ret = TE_ERROR_BAD_STATE;
        __GCM_OUT__;
        break;
    case TE_GCM_STATE_START:
        /** Because padding of aad was handled in gcm_update, but thinking of such scenario
         *  1. te_gcm_start() -> 2. te_gcm_finish() -> 3. te_gcm_free(), we should handle
         *  aad's padding here.*/
        ret = _te_gcm_finish_aad(prv_ctx);
        __GCM_CHECK_CONDITION__(ret);
        break;
    case TE_GCM_STATE_UPDATE:
        break;
    }
    /** take care, when prv_ctx->mlen is multiple blocks size, if we miss
     *  last modulus we wil get padding_sz with block size, witch is wrong */
    osal_memset(prv_ctx->padding, 0x00, DRV_SCA_MAX_PADDING_SIZE);
    padding_sz = (ctx->crypt->blk_size -
                   (prv_ctx->mlen % ctx->crypt->blk_size))
                    % ctx->crypt->blk_size;
    _len = 16 + padding_sz;
    _te_gcm_read_size(prv_ctx->padding, padding_sz + 7, prv_ctx->aadlen * 8);
    _te_gcm_read_size(prv_ctx->padding, padding_sz + 15, prv_ctx->mlen * 8);
    ret = te_gfmul_update(&prv_ctx->gctx, _len, prv_ctx->padding);
    __GCM_CHECK_CONDITION__(ret);
    ret = te_gfmul_finish(&prv_ctx->gctx, prv_ctx->ghash,
                            prv_ctx->gctx.crypt->blk_size);
    __GCM_CHECK_CONDITION__(ret);
    switch (TE_ALG_GET_MAIN_ALG(ctx->crypt->alg)) {
        case TE_MAIN_ALGO_AES:
            prv_ctx->cctx.crypt->alg = TE_ALG_AES_CTR;
            break;
        case TE_MAIN_ALGO_SM4:
            prv_ctx->cctx.crypt->alg = TE_ALG_SM4_CTR;
            break;
        default:
            ret = TE_ERROR_BAD_PARAMS;
            __GCM_OUT__;
    }
    ret = te_cipher_ctr(&prv_ctx->cctx,
                        prv_ctx->cctx.crypt->blk_size,
                        &_nc_off,
                        prv_ctx->j0,
                        _stream,
                        prv_ctx->ghash,
                        prv_ctx->swap);
    __GCM_CHECK_CONDITION__(ret);
    if(TE_DRV_SCA_DECRYPT == prv_ctx->op){
        if( 0 != osal_memcmp(tag, prv_ctx->swap, taglen) ){
            osal_memcpy(tag, prv_ctx->swap, taglen);
            ret = TE_ERROR_SECURITY;
        }
    }else{
        osal_memcpy(tag, prv_ctx->swap, taglen);
    }

    prv_ctx->state = TE_GCM_STATE_READY;
__out__:
    return ret;
}

#ifdef CFG_TE_ASYNC_EN
typedef struct agcm_ctx {
    te_gcm_ctx_t *ctx;
    te_gcm_request_t *req;
} agcm_ctx_t;

static void execute_gcm_crypt( te_worker_task_t *task )
{
    int ret = TE_SUCCESS;
    agcm_ctx_t *agcm = task->param;
    te_gcm_ctx_t *ctx = agcm->ctx;
    te_gcm_request_t *req = agcm->req;
    te_sca_operation_t op = req->crypt.op;
    uint8_t *iv = req->crypt.iv;
    uint64_t ivlen = req->crypt.ivlen;
    te_memlist_t *aad = &req->crypt.aad;
    te_memlist_t *in = &req->crypt.in;
    te_memlist_t *out = &req->crypt.out;
    uint8_t *tag = req->crypt.tag;
    uint32_t taglen = req->crypt.taglen;


    ret = te_gcm_start( ctx, op, iv, ivlen);
    if ( ret != TE_SUCCESS ) {
        goto err;
    }

    ret = te_gcm_uplist_aad(ctx, aad);
    if ( ret != TE_SUCCESS ) {
        goto err;
    }

    ret = te_gcm_uplist( ctx, in, out );
    if ( ret != TE_SUCCESS ) {
        goto err;
    }

    ret = te_gcm_finish( ctx, tag, taglen );
    if ( ret != TE_SUCCESS ) {
        goto err;
    }

err:
    osal_free(task);
    osal_free(agcm);

    req->res = ret;
    req->base.completion( &req->base, req->res );
    return;
}

int te_gcm_acrypt(te_gcm_ctx_t *ctx, te_gcm_request_t *req)
{
    int ret = TE_SUCCESS;
    agcm_ctx_t *agcm = NULL;
    te_worker_task_t *task = NULL;
    if((NULL == ctx)
        || (NULL == ctx->crypt)
        || (NULL == req)
        || (NULL == req->crypt.iv)
        || (NULL  == req->crypt.tag)
        || (req->crypt.taglen > ctx->crypt->blk_size)
        || ((TE_DRV_SCA_ENCRYPT != req->crypt.op) &&
        (TE_DRV_SCA_DECRYPT != req->crypt.op))) {
        ret = TE_ERROR_BAD_PARAMS;
        __GCM_OUT__;
    }

    agcm = osal_calloc(1, sizeof(agcm_ctx_t));
    if (NULL == agcm) {
        ret = TE_ERROR_OOM;
        __GCM_OUT__;
    }

    task = osal_calloc(1, sizeof(te_worker_task_t));
    if (NULL == task) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    task->param = (void *)agcm;
    task->execute = execute_gcm_crypt;
    agcm->ctx = ctx;
    agcm->req = req;
    te_worker_pool_enqueue(task);

    return TE_SUCCESS;

err1:
    osal_free(agcm);
__out__:
    return ret;
}
#endif /* CFG_TE_ASYNC_EN */
