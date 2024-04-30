//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_xts.h>
#include <te_cipher.h>
#include "../common/te_worker_pool.h"

/**
 * SCA XTS mode context
 * The last process must include >= 1 blk
 */
typedef struct sca_xts_ctx {
    te_cipher_ctx_t ecb;                               /**< ecb sub-context for tweak */
    uint32_t dummy1 __te_dma_aligned;                  /**< placeholder */
    uint8_t etwk[SCA_MAX_BLOCK_SIZE] __te_dma_aligned; /**< encrypted tweak */
    uint32_t dummy2 __te_dma_aligned;                  /**< placeholder */
} sca_xts_ctx_t;

#define __XTS_OUT__     goto __out__

#define __XTS_CHECK_CONDITION__(_ret_)                                         \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 __XTS_OUT__;                                                  \
            }                                                                  \
        } while (0);

#define __XTS_ALERT__(_ret_, _msg_)                                            \
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
 * malg = TE_MAIN_ALGO_AES or TE_MAIN_ALGO_SM4
 */
int te_xts_init( te_xts_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg )
{
    int ret = 0;
    sca_xts_ctx_t *prv_ctx = NULL;
    te_sca_drv_t *sdrv = NULL;

    if((NULL == ctx)
        || ((TE_MAIN_ALGO_SM4 != malg) && (TE_MAIN_ALGO_AES != malg))){
        ret = TE_ERROR_BAD_PARAMS;
        __XTS_OUT__;
    }

    memset(ctx, 0x00, sizeof(te_xts_ctx_t));
    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    ret = te_sca_alloc_ctx(sdrv, malg,
                          sizeof(sca_xts_ctx_t),
                          &ctx->crypt);
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
    __XTS_CHECK_CONDITION__(ret);
    prv_ctx = (sca_xts_ctx_t *)xts_priv_ctx(ctx);
    ret = te_cipher_init(&prv_ctx->ecb, hdl, malg);
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR("%s +%d ret->%X\n", __FILE__, __LINE__, ret);
        goto err_cipher_init;
    }

    switch (malg)
    {
    case TE_MAIN_ALGO_SM4:
        ctx->crypt->alg = TE_ALG_SM4_XTS;
        prv_ctx->ecb.crypt->alg = TE_ALG_SM4_ECB_NOPAD;
        break;
    case TE_MAIN_ALGO_AES:
    default:
        ctx->crypt->alg = TE_ALG_AES_XTS;
        prv_ctx->ecb.crypt->alg = TE_ALG_AES_ECB_NOPAD;
        break;
    }

    return ret;

err_cipher_init:
    te_sca_free_ctx(ctx->crypt);
__out__:
    return ret;
}

int te_xts_free( te_xts_ctx_t *ctx )
{
    int ret = 0;
    sca_xts_ctx_t *prv_ctx = NULL;

    if(NULL == ctx){
            ret = TE_ERROR_BAD_PARAMS;
        __XTS_OUT__;
    }

    ret = te_sca_state(ctx->crypt);
    switch (ret){
    default:
        __XTS_OUT__;
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __XTS_ALERT__(ret, "te_sca_finish raise exceptions!");
        break;
    }

    prv_ctx = (sca_xts_ctx_t *)xts_priv_ctx(ctx);
    if(NULL != prv_ctx){
        ret = te_cipher_free(&prv_ctx->ecb);
        __XTS_ALERT__(ret, "te_cipher_free raise exceptions!");
    }
    ret = te_sca_free_ctx(ctx->crypt);
    __XTS_ALERT__(ret, "te_sca_free_ctx raise exceptions!");

__out__:
    return ret;
}

/**
 * key = key1 || key2, where key1 and key2 are with equal length.
 * keybits = 2 * 128, or 2 * 256
 */
int te_xts_setkey( te_xts_ctx_t *ctx,
                   const uint8_t *key,
                   uint32_t keybits )
{
#define CIPHER_KEY_BITS_128        (128U)
#define CIPHER_KEY_BITS_256        (256U)
#define KEY_LEN(_bits_)  ((_bits_) / 8)
    int ret = 0;
    te_sca_key_t key_desc = {0};
    sca_xts_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == ctx->crypt) || (NULL==key)){
        ret = TE_ERROR_BAD_PARAMS;
        __XTS_OUT__;
    }
    switch (TE_ALG_GET_MAIN_ALG(ctx->crypt->alg)) {
        case TE_MAIN_ALGO_AES:
            if(((CIPHER_KEY_BITS_128*2) != keybits)
                && ((CIPHER_KEY_BITS_256*2) != keybits)){
                ret = TE_ERROR_BAD_KEY_LENGTH;
                __XTS_OUT__;
            }
            break;
        case TE_MAIN_ALGO_SM4:
            if(((CIPHER_KEY_BITS_128*2) != keybits)){
                ret = TE_ERROR_BAD_KEY_LENGTH;
                __XTS_OUT__;
            }
            break;
        default:
            ret = TE_ERROR_BAD_PARAMS;
            __XTS_OUT__;
    }
    key_desc.type = TE_KEY_TYPE_USER;
    key_desc.user.key = (uint8_t *)key;
    key_desc.user.keybits = keybits / 2;
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    __XTS_CHECK_CONDITION__(ret);
    prv_ctx = (sca_xts_ctx_t *)xts_priv_ctx(ctx);
    if(NULL != prv_ctx){
        ret = te_cipher_setkey(&prv_ctx->ecb,
                               key + KEY_LEN(keybits / 2),
                               (keybits / 2));
    }else{
        ret = TE_ERROR_BAD_PARAMS;
    }
__out__:
    return ret;
}

/**
 * key1.ek3bits and key2.ek3bits must be equal
 */
int te_xts_setseckey( te_xts_ctx_t *ctx,
                      te_sec_key_t *key1,
                      te_sec_key_t *key2 )
{
    int ret = 0;
    te_sca_key_t key_desc = {0};
    sca_xts_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == ctx->crypt) || (NULL==key1) || (NULL==key2)){
        ret = TE_ERROR_BAD_PARAMS;
        __XTS_OUT__;
    }

    if (key1->ek3bits != key2->ek3bits) {
        ret = TE_ERROR_BAD_KEY_LENGTH;
        __XTS_OUT__;
    }

    switch (TE_ALG_GET_MAIN_ALG(ctx->crypt->alg)) {
        case TE_MAIN_ALGO_AES:
            if(((CIPHER_KEY_BITS_128) != key1->ek3bits)
                && ((CIPHER_KEY_BITS_256) != key1->ek3bits)){
                ret = TE_ERROR_BAD_KEY_LENGTH;
                __XTS_OUT__;
            }
            break;
        case TE_MAIN_ALGO_SM4:
            if(((CIPHER_KEY_BITS_128) != key1->ek3bits)){
                ret = TE_ERROR_BAD_KEY_LENGTH;
                __XTS_OUT__;
            }
            break;
        default:
            ret = TE_ERROR_BAD_PARAMS;
            __XTS_OUT__;
    }
    key_desc.type = TE_KEY_TYPE_SEC;
    memcpy(&key_desc.sec, key1, sizeof(te_sec_key_t));
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    __XTS_CHECK_CONDITION__(ret);
    prv_ctx = (sca_xts_ctx_t *)xts_priv_ctx(ctx);
    if(NULL != prv_ctx){
        ret = te_cipher_setseckey(&prv_ctx->ecb, key2);
    }else{
        ret = TE_ERROR_BAD_PARAMS;
    }

__out__:
    return ret;
}

/**
 * key1.ek3bits and key2.ek3bits must be equal, AES only
 */
int te_xts_setseckey_v2( te_xts_ctx_t *ctx,
                         te_sec_key_v2_t *key1,
                         te_sec_key_v2_t *key2 )
{
    int ret = 0;
    te_sca_key_t key_desc = {0};
    sca_xts_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == ctx->crypt) || (NULL==key1) || (NULL==key2) ||
       (TE_ALG_AES_XTS != ctx->crypt->alg)) {
        ret = TE_ERROR_BAD_PARAMS;
        __XTS_OUT__;
    }

    if ((key1->ek3bits != key2->ek3bits) ||
        ((CIPHER_KEY_BITS_256) != key1->ek3bits)) {
        ret = TE_ERROR_BAD_KEY_LENGTH;
        __XTS_OUT__;
    }

    key_desc.type = TE_KEY_TYPE_SEC_V2;
    memcpy(&key_desc.sec_v2, key1, sizeof(te_sec_key_v2_t));
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    __XTS_CHECK_CONDITION__(ret);
    prv_ctx = (sca_xts_ctx_t *)xts_priv_ctx(ctx);
    if (NULL != prv_ctx) {
        ret = te_cipher_setseckey_v2(&prv_ctx->ecb, key2);
    }else{
        ret = TE_ERROR_BAD_PARAMS;
    }

__out__:
    return ret;
}

static void te_xts_mult_x(uint8_t *I)
{
    int x = 0;
    uint8_t t = 0;
    uint8_t tt = 0;

    for (x = t = 0; x < 16; x++) {
       tt = I[x] >> 7;
       I[x] = ((I[x] << 1) | t) & 0xFF;
       t = tt;
    }
    if (tt) {
       I[0] ^= 0x87;
    }
}

static void te_xts_update_tweak(uint8_t *tweek, size_t blocks)
{
    size_t i = 0;
    for (i = 0; i < blocks; i++) {
        te_xts_mult_x(tweek);
    }
}

int te_xts_crypt( te_xts_ctx_t *ctx,
                  te_sca_operation_t op,
                  size_t len,
                  uint8_t data_unit[16],
                  const uint8_t *in,
                  uint8_t *out )
{
    int ret = 0;
    sca_xts_ctx_t *prv_ctx = NULL;

    if((NULL == ctx)
        || (NULL == ctx->crypt)
        || ((TE_DRV_SCA_ENCRYPT != op) && (TE_DRV_SCA_DECRYPT != op))
        || (NULL == data_unit)
        || (NULL == in)
        || (NULL ==out)){
         ret = TE_ERROR_BAD_PARAMS;
         __XTS_OUT__;
    }

    if (ctx->crypt->blk_size > len) {
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        __XTS_OUT__;
    }
    prv_ctx = (sca_xts_ctx_t *)xts_priv_ctx(ctx);
    TE_ASSERT( NULL != prv_ctx );
    ret = te_cipher_ecb(&prv_ctx->ecb,
                        TE_DRV_SCA_ENCRYPT,
                        prv_ctx->ecb.crypt->blk_size,
                        data_unit,
                        prv_ctx->etwk);
    __XTS_CHECK_CONDITION__(ret);
    ret = te_sca_start(ctx->crypt, op,
                       prv_ctx->etwk, ctx->crypt->blk_size);
    __XTS_CHECK_CONDITION__(ret);
    /* [Notes] hw engine require the last proc's len must be greater than
     * one block size  */
    ret = te_sca_update( ctx->crypt,
                         ((len == ctx->crypt->blk_size) ? false: true),
                         len,
                         in,
                         out );
    if (TE_SUCCESS != ret) {
        goto cleanup;
    }
    te_xts_update_tweak( prv_ctx->etwk,
                         UTILS_ROUND_UP(len, ctx->crypt->blk_size) /
                                     ctx->crypt->blk_size);
    ret = te_cipher_ecb( &prv_ctx->ecb,
                         TE_DRV_SCA_DECRYPT,
                         prv_ctx->ecb.crypt->blk_size,
                         prv_ctx->etwk,
                         prv_ctx->etwk );
    if (TE_SUCCESS != ret) {
        goto cleanup;
    }
    osal_memcpy(data_unit, prv_ctx->etwk, prv_ctx->ecb.crypt->blk_size);
    ret = te_sca_finish(ctx->crypt, NULL, 0);
    __XTS_OUT__;
cleanup:
    te_sca_finish(ctx->crypt, NULL, 0);
__out__:
    return ret;
}

int te_xts_crypt_list( te_xts_ctx_t *ctx,
                       te_sca_operation_t op,
                       uint8_t data_unit[16],
                       te_memlist_t *in,
                       te_memlist_t *out )
{
    int ret = TE_SUCCESS;
    sca_xts_ctx_t *prv_ctx = NULL;
    size_t len = 0;
    if((NULL == ctx)
        || (NULL == ctx->crypt)
        || ((TE_DRV_SCA_ENCRYPT != op) && (TE_DRV_SCA_DECRYPT != op))
        || (NULL == data_unit)
        || (NULL == in)
        || (NULL ==out)){
         ret = TE_ERROR_BAD_PARAMS;
         __XTS_OUT__;
    }

    len = te_memlist_get_total_len(in);
    if (len < ctx->crypt->blk_size
        || len > te_memlist_get_total_len(out)) {
            ret = TE_ERROR_BAD_INPUT_LENGTH;
            __XTS_OUT__;
    }
    prv_ctx = (sca_xts_ctx_t *)xts_priv_ctx(ctx);
    TE_ASSERT( NULL != prv_ctx );
    ret = te_cipher_ecb(&prv_ctx->ecb,
                        TE_DRV_SCA_ENCRYPT,
                        prv_ctx->ecb.crypt->blk_size,
                        data_unit,
                        prv_ctx->etwk);
    __XTS_CHECK_CONDITION__(ret);
    ret = te_sca_start(ctx->crypt, op,
                       prv_ctx->etwk, ctx->crypt->blk_size);
    __XTS_CHECK_CONDITION__(ret);
    /* [Notes] hw engine require the last proc's len must be greater than
     * one block size  */
    ret = te_sca_uplist( ctx->crypt,
                         ((len == ctx->crypt->blk_size) ? false : true),
                         in,
                         out );
    if (TE_SUCCESS != ret) {
        goto cleanup;
    }

    ret = te_sca_finish(ctx->crypt, NULL, 0);
    __XTS_CHECK_CONDITION__(ret);
    te_xts_update_tweak( prv_ctx->etwk,
                         UTILS_ROUND_UP(te_memlist_get_total_len(in),
                                        ctx->crypt->blk_size) /
                                            ctx->crypt->blk_size );
    ret = te_cipher_ecb( &prv_ctx->ecb,
                         TE_DRV_SCA_DECRYPT,
                         prv_ctx->ecb.crypt->blk_size,
                         prv_ctx->etwk,
                         prv_ctx->etwk );
    __XTS_CHECK_CONDITION__(ret);
    osal_memcpy(data_unit, prv_ctx->etwk, prv_ctx->ecb.crypt->blk_size);
    __XTS_OUT__;
cleanup:
    te_sca_finish(ctx->crypt, NULL, 0);
__out__:
    return ret;
}

int te_xts_clone( const te_xts_ctx_t *src,
                  te_xts_ctx_t *dst )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;
    sca_xts_ctx_t *spctx = NULL;
    sca_xts_ctx_t *dpctx = NULL;

    if (NULL == src || NULL == src->crypt || NULL == dst) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * free the dst ctx if it points to a valid xts ctx already
     */
    if (dst->crypt != NULL) {
        ret = te_xts_free(dst);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_xts_free error %x\n", ret);
            return ret;
        }
    }

    spctx = (sca_xts_ctx_t *)xts_priv_ctx((te_xts_ctx_t*)src);
    if(NULL == spctx){
        return TE_ERROR_BAD_PARAMS;
    }

    sdrv = (te_sca_drv_t *)src->crypt->drv;
    ret = te_sca_alloc_ctx(sdrv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_xts_ctx_t),
                           &dst->crypt);
    if (ret != TE_SUCCESS) {
        return ret;
    }

    dpctx = (sca_xts_ctx_t *)xts_priv_ctx(dst);
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
    osal_memset(&dpctx->ecb, 0, sizeof(dpctx->ecb));

    /*
     * clone sub-contexts
     */
    ret = te_cipher_clone(&spctx->ecb, &dpctx->ecb);
    if (ret != TE_SUCCESS) {
        te_xts_free(dst);
        return ret;
    }

    return TE_SUCCESS;
}

#ifdef CFG_TE_ASYNC_EN
typedef struct axts_ctx {
    te_xts_ctx_t *ctx;
    te_xts_request_t *req;
} axts_ctx_t;

static void execute_xts_crypt(te_worker_task_t *task)
{
    int ret = TE_SUCCESS;
    axts_ctx_t *axts = task->param;
    te_xts_ctx_t *ctx = axts->ctx;
    te_xts_request_t *req = axts->req;
    te_sca_operation_t op = req->op;
    te_memlist_t *in = &req->src;
    te_memlist_t *out = &req->dst;
    uint8_t *data_unit = req->data_unit;

    ret = te_xts_crypt_list(ctx, op, data_unit, in, out);

    osal_free(task);
    osal_free(axts);
    req->res = ret;
    req->base.completion(&req->base, ret);
    return;
}

int te_xts_acrypt(te_xts_ctx_t *ctx, te_xts_request_t *req)
{
    int ret = TE_SUCCESS;
    size_t len = 0;
    axts_ctx_t *axts = NULL;
    te_worker_task_t *task = NULL;

    if((NULL == ctx)
       || (NULL == ctx->crypt) || (NULL == req)
       || (NULL == req->src.ents)
       || (0 == req->src.nent)
       || (NULL == req->dst.ents)
       || (0 == req->dst.nent)
       || ((TE_DRV_SCA_ENCRYPT != req->op)
        && (TE_DRV_SCA_DECRYPT != req->op))){
         ret = TE_ERROR_BAD_PARAMS;
         __XTS_OUT__;
    }

    len = te_memlist_get_total_len(&req->src);
    if (len < ctx->crypt->blk_size
        || len < te_memlist_get_total_len(&req->dst)) {
            ret = TE_ERROR_BAD_PARAMS;
            __XTS_OUT__;
    }

    axts = osal_calloc(1, sizeof(axts_ctx_t));
    if (NULL == axts) {
        ret = TE_ERROR_OOM;
        __XTS_OUT__;
    }

    task = osal_calloc(1, sizeof(te_worker_task_t));
    if (NULL == task) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    task->param = (void *)axts;
    task->execute = execute_xts_crypt;
    axts->ctx = ctx;
    axts->req = req;
    te_worker_pool_enqueue(task);

    return TE_SUCCESS;

err1:
    osal_free(axts);
__out__:
    return ret;
}
#endif

