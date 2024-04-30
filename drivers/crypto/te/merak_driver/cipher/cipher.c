//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_cipher.h>
#include "../common/te_worker_pool.h"

#define __CIPHER_OUT__      goto  __out__

#define __CIPHER_CHECK_CONDITION__(_ret_)                                      \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 __CIPHER_OUT__;                                               \
            }                                                                  \
        } while (0);


#define __CIPHER_ALERT__(_ret_, _msg_)                                         \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X %s\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_),                                       \
                                (_msg_));                                      \
            }                                                                  \
        } while (0);

#define XOR_BLOCK(_a_, _b_, _l_)                                               \
    do{                                                                        \
        uint8_t *_pa = (uint8_t *)(_a_);                                       \
        uint8_t *_pb = (uint8_t *)(_b_);                                       \
        size_t _i_ = 0;                                                        \
        for(; _i_<(_l_); _i_++) {                                              \
            (_pa)[_i_] ^= (_pb)[_i_];                                          \
        }                                                                      \
    }while (0)

#define COUNTER_INCREASE(c, l, delta)                                          \
                do {                                                           \
                    size_t _i = 0, _j = 0;                                     \
                    for (_i = 0; _i < (delta); _i++) {                         \
                        for(_j = (l); _j > 0; _j--) {                          \
                            if (0 != ++((c)[_j - 1])) {                        \
                                break;                                         \
                            }                                                  \
                        }                                                      \
                    }                                                          \
                }while(0)

/**
 * SCA CBC private context
 */
typedef struct sca_cbc_ctx {
    uint8_t iv[SCA_MAX_BLOCK_SIZE];     /**< initial vector */
} sca_cbc_ctx_t;

/**
 * SCA CTR private context
 */
typedef struct sca_ctr_ctx {
    uint8_t iv[SCA_MAX_BLOCK_SIZE];     /**< nonce counter */
    uint8_t stream[SCA_MAX_BLOCK_SIZE]; /**< stream block */
    uint32_t strpos;                    /**< stream block offset */
} sca_ctr_ctx_t;

/**
 * SCA OFB private context
 */
typedef struct sca_ofb_ctx {
    uint8_t stream[SCA_MAX_BLOCK_SIZE]; /**< stream block */
    uint32_t strpos;                    /**< stream block offset */
} sca_ofb_ctx_t;

/**
 * SCA cipher private context structure
 */
typedef struct sca_cipher_ctx {
    sca_cbc_ctx_t cbc;
    sca_ctr_ctx_t ctr;
    sca_ofb_ctx_t ofb;
    te_sca_operation_t op;
    te_algo_t pre_alg;    /**< mark previous algo, once algo changed should close previous session and open a new one */
} sca_cipher_ctx_t;

typedef struct in_out_offs {
    size_t in_ind;
    size_t in_off;
    size_t out_ind;
    size_t out_off;
    size_t out_nent;
    size_t out_len;
} in_out_offs_t;


int te_cipher_init( te_cipher_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;

    if (NULL == ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __CIPHER_OUT__;
    }

    osal_memset(ctx, 0x00, sizeof(te_cipher_ctx_t));
    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    ret = te_sca_alloc_ctx(sdrv, malg,
                          sizeof(sca_cipher_ctx_t),
                          &ctx->crypt);
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
__out__:
    return ret;
}

int te_cipher_free( te_cipher_ctx_t *ctx )
{
    int ret = TE_SUCCESS;

    if ( NULL == ctx ) {
        ret = TE_ERROR_BAD_PARAMS;
        __CIPHER_OUT__;
    }

    ret = te_sca_state(ctx->crypt);
    switch (ret){
    default:
        __CIPHER_OUT__;
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_READY:
    case TE_DRV_SCA_STATE_INIT:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __CIPHER_ALERT__(ret, "te_sca_finish raises exceptions!!!");
        break;
    }

    if (NULL != ctx->crypt) {
        ret = te_sca_free_ctx(ctx->crypt);
        __CIPHER_CHECK_CONDITION__(ret);
        ctx->crypt = NULL;
    }

__out__:
    return ret;
}

int te_cipher_setkey( te_cipher_ctx_t *ctx,
                      const uint8_t *key,
                      uint32_t keybits )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};

    if((NULL == ctx) || (NULL == key) || (NULL == ctx->crypt)) {
        ret =  TE_ERROR_BAD_PARAMS;
        __CIPHER_OUT__;
    }

    ret = te_sca_state(ctx->crypt);
    switch (ret){
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        __CIPHER_OUT__;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __CIPHER_CHECK_CONDITION__(ret);
        break;
    }

    key_desc.type = TE_KEY_TYPE_USER;
    key_desc.user.key = (uint8_t *)key;
    key_desc.user.keybits = keybits;
    ret = te_sca_setkey(ctx->crypt, &key_desc);

__out__:
    return ret ;
}

/**
 * AES and SM4 only
 */
int te_cipher_setseckey( te_cipher_ctx_t *ctx,
                         te_sec_key_t *key )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc;

    if ((NULL == ctx) || (NULL == key) || (NULL == ctx->crypt)) {
        ret =  TE_ERROR_BAD_PARAMS;
        __CIPHER_OUT__;
    }

    ret = te_sca_state(ctx->crypt);
    switch (ret){
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        __CIPHER_OUT__;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __CIPHER_CHECK_CONDITION__(ret);
        break;
    }
    osal_memset(&key_desc, 0x00, sizeof(key_desc));
    key_desc.type = TE_KEY_TYPE_SEC;
    osal_memcpy(&key_desc.sec, key, sizeof(te_sec_key_t));
    ret = te_sca_setkey(ctx->crypt,&key_desc);

__out__:
    return ret ;
}

/**
 * AES only
 */
int te_cipher_setseckey_v2( te_cipher_ctx_t *ctx,
                            te_sec_key_v2_t *key )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc;

    if ((NULL == ctx) || (NULL == key) || (NULL == ctx->crypt)) {
        ret =  TE_ERROR_BAD_PARAMS;
        __CIPHER_OUT__;
    }

    ret = te_sca_state(ctx->crypt);
    switch (ret){
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        __CIPHER_OUT__;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __CIPHER_CHECK_CONDITION__(ret);
        break;
    }
    osal_memset(&key_desc, 0x00, sizeof(key_desc));
    key_desc.type = TE_KEY_TYPE_SEC_V2;
    osal_memcpy(&key_desc.sec_v2, key, sizeof(te_sec_key_v2_t));
    ret = te_sca_setkey(ctx->crypt,&key_desc);

__out__:
    return ret ;
}

/**
 * AES and SM4 only.
 * Dump the key ladder derived key (CM and DM only).
 * The acceptable keybits is either 128 or 256.
 */
int te_cipher_getseckey( te_cipher_ctx_t *ctx,
                         uint8_t *key,
                         uint32_t keybits )
{
    (void)ctx;
    (void)key;
    (void)keybits;
    return TE_ERROR_NOT_IMPLEMENTED;
}

/* tranverse ecb blocks */
static int _te_cipher_ecb_start( te_cipher_ctx_t *ctx,
                                 te_sca_operation_t op )
{
    int ret = TE_SUCCESS;
    sca_cipher_ctx_t *prv_ctx = (sca_cipher_ctx_t *)cipher_priv_ctx(ctx);
    if (!prv_ctx || !ctx->crypt) {
        ret = TE_ERROR_BAD_FORMAT;
        __CIPHER_OUT__;
    }
    ret = te_sca_state(ctx->crypt);
    switch (ret){
    default:
        __CIPHER_OUT__;
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
        ret = TE_ERROR_BAD_STATE;
        break;
    case TE_DRV_SCA_STATE_READY:
        ret = te_sca_start(ctx->crypt, op, NULL, 0);
        __CIPHER_CHECK_CONDITION__(ret);
        prv_ctx->pre_alg = ctx->crypt->alg;
        prv_ctx->op = op;
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        if ((ctx->crypt->alg != prv_ctx->pre_alg)
            || (op != prv_ctx->op)) {
            ret = te_sca_finish(ctx->crypt, NULL, 0);
            __CIPHER_CHECK_CONDITION__(ret);
            ret = te_sca_start(ctx->crypt, op, NULL, 0);
            __CIPHER_CHECK_CONDITION__(ret);
            prv_ctx->pre_alg = ctx->crypt->alg;
            prv_ctx->op = op;
        } else {
            ret = TE_SUCCESS;
        }
        break;
    }

__out__:
    return ret;
}

/* tranverse ecb blocks */
int te_cipher_ecb( te_cipher_ctx_t *ctx,
                   te_sca_operation_t op,
                   size_t len,
                   const uint8_t *in,
                   uint8_t *out )
{
    int ret = TE_SUCCESS;
    /* sanity check parameters */
    if((NULL == ctx)
        || (NULL == ctx->crypt)
        || ((TE_DRV_SCA_ENCRYPT != op) && (TE_DRV_SCA_DECRYPT != op))
        || (len && (NULL == in))
        || (len && (NULL == out))
        || !UTILS_IS_ALIGNED(len, ctx->crypt->blk_size)){
            ret = TE_ERROR_BAD_PARAMS;
            __CIPHER_OUT__;
    }
    if (0 == len) {
        __CIPHER_OUT__;
    }
    ret = _te_cipher_ecb_start(ctx, op);
    __CIPHER_CHECK_CONDITION__(ret);
    ret = te_sca_update(ctx->crypt, false, len, in, out);
__out__:
    return ret;
}

/* tranverse ecb blocks list*/
int te_cipher_ecb_list( te_cipher_ctx_t *ctx,
                   te_sca_operation_t op,
                   te_memlist_t *in,
                   te_memlist_t *out )
{
    int ret = TE_SUCCESS;
    size_t len = 0;
    /* sanity check parameters */
    if ((NULL == ctx)
        || (NULL == ctx->crypt)
        || ((TE_DRV_SCA_ENCRYPT != op) && (TE_DRV_SCA_DECRYPT != op))
        || (NULL == in)
        || (NULL == out)) {
            ret = TE_ERROR_BAD_PARAMS;
            __CIPHER_OUT__;
    }

    len = te_memlist_get_total_len(in);
    if (0 == len) {
        __CIPHER_OUT__;
    }
    if ( len > te_memlist_get_total_len(out)
        || !UTILS_IS_ALIGNED(len, ctx->crypt->blk_size)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CIPHER_OUT__;
    }

    ret = _te_cipher_ecb_start(ctx, op);
    __CIPHER_CHECK_CONDITION__(ret);
    ret = te_sca_uplist(ctx->crypt, false, in, out);
__out__:
    return ret;
}

static inline int _te_get_list_data_backward(te_memlist_t *list,
                                             size_t offset,
                                             uint8_t *dst,
                                             size_t len){
    int ret = TE_SUCCESS;
    int i = 0;
    size_t copy_len = 0;
    size_t _len = 0;

    for (i = list->nent-1; i >= 0; i--)
    {
        if (offset > list->ents[i].len) {
            offset -= list->ents[i].len;
            continue;
        } else {
            _len = list->ents[i].len - offset;
            if(0 < len){
                copy_len = (len > _len) ? _len : len;
                osal_memcpy(dst + len - copy_len,
                            (uint8_t*)list->ents[i].buf + list->ents[i].len -
                            offset - copy_len, copy_len);
                len -= copy_len;
                offset = 0;
            }else{
                break;
            }
        }
    }

    return ret;
}

static int _te_cipher_cbc_start( te_cipher_ctx_t *ctx,
                            te_sca_operation_t op,
                            uint8_t *iv)
{
    int ret = TE_SUCCESS;
    sca_cipher_ctx_t *prv_ctx = NULL;
    if (NULL == iv) {
        ret = TE_ERROR_BAD_PARAMS;
        __CIPHER_OUT__;
    }
    prv_ctx = (sca_cipher_ctx_t *)cipher_priv_ctx(ctx);
    if (!prv_ctx || !ctx->crypt) {
        ret = TE_ERROR_BAD_FORMAT;
        __CIPHER_OUT__;
    }
    ret = te_sca_state(ctx->crypt);
    switch (ret){
    default:
        __CIPHER_OUT__;
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
        ret = TE_ERROR_BAD_STATE;
        break;
    case TE_DRV_SCA_STATE_READY:
        osal_memcpy(prv_ctx->cbc.iv, iv, ctx->crypt->blk_size);
        ret = te_sca_start(ctx->crypt, op, prv_ctx->cbc.iv,
                            ctx->crypt->blk_size);
        __CIPHER_CHECK_CONDITION__(ret);
        prv_ctx->op = op;
        prv_ctx->pre_alg = ctx->crypt->alg;
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        TE_ASSERT(NULL != ctx->crypt);
        if ((ctx->crypt->alg != prv_ctx->pre_alg)
             || (osal_memcmp(iv, prv_ctx->cbc.iv,
                         ctx->crypt->blk_size) != 0)
             || (op != prv_ctx->op)) {
            ret = te_sca_finish(ctx->crypt, NULL, 0);
            __CIPHER_CHECK_CONDITION__(ret);
            osal_memcpy(prv_ctx->cbc.iv, iv, ctx->crypt->blk_size);
            ret = te_sca_start(ctx->crypt, op, prv_ctx->cbc.iv,
                                ctx->crypt->blk_size);
            __CIPHER_CHECK_CONDITION__(ret);
            prv_ctx->op = op;
            prv_ctx->pre_alg = ctx->crypt->alg;
        } else {
            ret = TE_SUCCESS;
        }
        break;
    }
__out__:
    return ret;
}

int te_cipher_cbc( te_cipher_ctx_t *ctx,
                   te_sca_operation_t op,
                   size_t len,
                   uint8_t *iv,
                   const uint8_t *in,
                   uint8_t *out )
{
    int ret = TE_SUCCESS;
    sca_cipher_ctx_t *prv_ctx = NULL;
    /* sanity check parameters */
    if((NULL == ctx)
        || (NULL == ctx->crypt)
        || ((TE_DRV_SCA_ENCRYPT != op) && (TE_DRV_SCA_DECRYPT != op))
        || (NULL == iv)
        || (NULL == in)
        || (NULL == out)) {
            ret = TE_ERROR_BAD_PARAMS;
            __CIPHER_OUT__;
    }
    if (0 ==  len) {
        __CIPHER_OUT__;
    }
    if (!UTILS_IS_ALIGNED(len, ctx->crypt->blk_size)) {
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        __CIPHER_OUT__;
    }

    prv_ctx = (sca_cipher_ctx_t *)cipher_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        __CIPHER_OUT__;
    }
    ret = _te_cipher_cbc_start(ctx, op, iv);
    __CIPHER_CHECK_CONDITION__(ret);

    /*update IV before decrypting on decryption */
    if (TE_DRV_SCA_DECRYPT == op) {
        osal_memcpy(prv_ctx->cbc.iv,
                    in + len - ctx->crypt->blk_size,
                    ctx->crypt->blk_size);
    }

    ret = te_sca_update(ctx->crypt, false, len, in, out);
    __CIPHER_CHECK_CONDITION__(ret);
    /*update iv*/
    if (TE_DRV_SCA_ENCRYPT == op) {
        osal_memcpy(prv_ctx->cbc.iv,
                    out + len - ctx->crypt->blk_size,
                    ctx->crypt->blk_size);
    }

    osal_memcpy(iv,
                prv_ctx->cbc.iv,
                ctx->crypt->blk_size);
__out__:
    return ret;
}

int te_cipher_cbc_list( te_cipher_ctx_t *ctx,
                   te_sca_operation_t op,
                   uint8_t *iv,
                   te_memlist_t *in,
                   te_memlist_t *out )
{
    int ret = TE_SUCCESS;
    sca_cipher_ctx_t *prv_ctx = NULL;
    size_t len = 0;
    /* sanity check parameters */
    if ((NULL == ctx)
        || (NULL == ctx->crypt)
        || ((TE_DRV_SCA_ENCRYPT != op) && (TE_DRV_SCA_DECRYPT != op))
        || (NULL == iv)
        || (NULL == in)
        || (NULL == out)) {
            ret = TE_ERROR_BAD_PARAMS;
            __CIPHER_OUT__;
    }
    len = te_memlist_get_total_len(in);
    if (0 == len) {
        __CIPHER_OUT__;
    }
    if (len != te_memlist_get_total_len(out)
        ||!UTILS_IS_ALIGNED(len, ctx->crypt->blk_size)) {
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        __CIPHER_OUT__;
    }

    prv_ctx = (sca_cipher_ctx_t *)cipher_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        __CIPHER_OUT__;
    }
    ret = _te_cipher_cbc_start(ctx, op, iv);
    __CIPHER_CHECK_CONDITION__(ret);

    /*update IV before decrypting on decryption */
    if (TE_DRV_SCA_DECRYPT == op) {
        ret = _te_get_list_data_backward(in,
                                        0,
                                        prv_ctx->cbc.iv,
                                        ctx->crypt->blk_size);
    }
    ret = te_sca_uplist(ctx->crypt, false, in, out);
    __CIPHER_CHECK_CONDITION__(ret);
    /*update iv*/
    if (TE_DRV_SCA_ENCRYPT == op) {
        ret = _te_get_list_data_backward(out,
                                         0,
                                         prv_ctx->cbc.iv,
                                         ctx->crypt->blk_size);
    }
    osal_memcpy(iv,
                prv_ctx->cbc.iv,
                ctx->crypt->blk_size);
__out__:
    return ret;
}

static void _te_fill_linklist(te_memlist_t *list,
                             uint8_t *buffer,
                             size_t len,
                             size_t ind_off,
                             size_t len_off){
    size_t i = 0;
    size_t _src_off = 0;
    size_t _copy_len = 0;
    size_t _dst_off = len_off;

    for (i = ind_off; i < list->nent; i++){
        _copy_len = ((list->ents[i].len - _dst_off) > len) ? \
                    len : ((list->ents[i].len - _dst_off));
        osal_memcpy((uint8_t*)list->ents[i].buf + _dst_off,
                    buffer + _src_off,
                    _copy_len);
        _dst_off = 0;
        len -= _copy_len;
        if(0 == len){
            break;
        }

        _src_off += _copy_len;
    }
}

static void _xor_block( uint8_t *in1,
                          uint8_t *in2,
                          uint8_t *out,
                          size_t size )
{
    size_t i = 0;

    for (i = 0; i < size; i++) {
        out[i] = in1[i] ^ in2[i];
    }
}

static size_t _te_memlist_update_stream(te_memlist_t *out,
                                    te_memlist_t *in,
                                    uint8_t *stream,
                                    size_t len,
                                    in_out_offs_t *offs)
{
    size_t i = 0;
    size_t j = 0;
    size_t _len = 0;
    size_t o_off = 0;
    size_t i_off = 0;
    size_t cp_len = 0;
    size_t str_off = 0;

    for (i = 0; i< in->nent; i++) {
        i_off = 0;
        _len = len > in->ents[i].len ? in->ents[i].len : len;
        for (; j < out->nent; ) {
            cp_len = _len > out->ents[j].len - o_off ? out->ents[j].len - o_off : _len;

            _xor_block( stream + str_off,
                        (uint8_t *)in->ents[i].buf + i_off,
                        (uint8_t *)out->ents[j].buf + o_off,
                        cp_len );
            str_off += cp_len;
            o_off += cp_len;
            i_off += cp_len;
            _len -= cp_len;
            len -= cp_len;
            if (0 == _len) {
                break;
            } else {
                /**< goes to new node then reset o_off */
                o_off = 0;
                j++;
            }
        }

        if (0 == len){
            if (i_off != in->ents[i].len) {
                offs->in_off = i_off;
                offs->in_ind = i;
            } else {
                offs->in_off = 0;
                offs->in_ind = i + 1;
            }
            if (o_off != out->ents[j].len) {
                offs->out_off = o_off;
                offs->out_ind = j;
            } else {
                offs->out_off = 0;
                offs->out_ind = j + 1;
            }
            break;
        }
    }

    return str_off;
}

static int _te_cipher_ofb_start( te_cipher_ctx_t *ctx,
                   size_t *iv_off,
                   uint8_t *iv)
{
    int ret = TE_SUCCESS;
    sca_cipher_ctx_t *prv_ctx = NULL;

    prv_ctx = (sca_cipher_ctx_t *)cipher_priv_ctx(ctx);
    if (NULL == prv_ctx || NULL == ctx->crypt) {
        ret = TE_ERROR_BAD_FORMAT;
        __CIPHER_OUT__;
    }
    /* check algorithm requirement, only support AES and SM4 */
    if((TE_MAIN_ALGO_AES != TE_ALG_GET_MAIN_ALG(ctx->crypt->alg))
        && (TE_MAIN_ALGO_SM4 != TE_ALG_GET_MAIN_ALG(ctx->crypt->alg))){
            ret = TE_ERROR_BAD_PARAMS;
            __CIPHER_OUT__;
    }
    ret = te_sca_state(ctx->crypt);
    switch (ret){
    default:
        __CIPHER_OUT__;
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
        ret = TE_ERROR_BAD_STATE;
        break;
    case TE_DRV_SCA_STATE_READY:
        osal_memcpy(prv_ctx->ofb.stream, iv, ctx->crypt->blk_size);
        prv_ctx->ofb.strpos = 0;
        ret = te_sca_start(ctx->crypt, TE_DRV_SCA_ENCRYPT,
                           prv_ctx->ofb.stream, ctx->crypt->blk_size);
        __CIPHER_CHECK_CONDITION__(ret);
        prv_ctx->pre_alg = ctx->crypt->alg;
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        if ((ctx->crypt->alg != prv_ctx->pre_alg)
            || (osal_memcmp(iv, prv_ctx->ofb.stream, ctx->crypt->blk_size) != 0)
            || (iv_off && prv_ctx->ofb.strpos != *iv_off)) {
            ret = te_sca_finish(ctx->crypt, NULL, 0);
            __CIPHER_CHECK_CONDITION__(ret);
            osal_memcpy(prv_ctx->ofb.stream, iv, ctx->crypt->blk_size);
            prv_ctx->ofb.strpos = 0;
            ret = te_sca_start(ctx->crypt, TE_DRV_SCA_ENCRYPT,
                               prv_ctx->ofb.stream, ctx->crypt->blk_size);
            __CIPHER_CHECK_CONDITION__(ret);
            prv_ctx->pre_alg = ctx->crypt->alg;
        } else {
            ret = TE_SUCCESS;
        }
        break;
    }

    if (iv_off) {
        prv_ctx->ofb.strpos = *iv_off;
    }
__out__:
    return ret;
}
/**
 * AES/SM4 only
 */
int te_cipher_ofb( te_cipher_ctx_t *ctx,
                   size_t len,
                   size_t *iv_off,
                   uint8_t *iv,
                   const uint8_t *in,
                   uint8_t *out )
{
    int ret = TE_SUCCESS;
    size_t remainder = 0;
    size_t __len = 0;
    uint8_t *_tmp = NULL;
    uint8_t *_tmp_raw = NULL;
    sca_cipher_ctx_t *prv_ctx = NULL;
    uint8_t in_last_block[TE_MAX_SCA_BLOCK] = {0};

    /* sanity check parameters */
    if((NULL == ctx)
        || (NULL == ctx->crypt)
        || (NULL == iv)
        || (NULL == in)
        || (NULL == out)
        || (iv_off && (ctx->crypt->blk_size <= *iv_off))){
            ret = TE_ERROR_BAD_PARAMS;
            __CIPHER_OUT__;
    }
    if (0 == len) {
        __CIPHER_OUT__;
    }
    prv_ctx = (sca_cipher_ctx_t *)cipher_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        __CIPHER_OUT__;
    }
    ret = _te_cipher_ofb_start(ctx, iv_off, iv);
    __CIPHER_CHECK_CONDITION__(ret);

    if(prv_ctx->ofb.strpos != 0){
        remainder = ctx->crypt->blk_size - prv_ctx->ofb.strpos;
        __len = (len >= remainder) ? remainder : len;
        osal_memcpy(out, in, __len);
        XOR_BLOCK(out, prv_ctx->ofb.stream + prv_ctx->ofb.strpos, __len);
        len -= __len;
        in += __len;
        out += __len;
    }

    if(len == 0){
        prv_ctx->ofb.strpos = (prv_ctx->ofb.strpos + __len) %
                                        ctx->crypt->blk_size;
        if (iv_off) {
            *iv_off = prv_ctx->ofb.strpos;
        }
        __CIPHER_OUT__;
    }

    remainder = len % ctx->crypt->blk_size;
    if (0 == remainder) {
        osal_memcpy(in_last_block,
                    in + (len - ctx->crypt->blk_size),
                    ctx->crypt->blk_size);
    }
    len -= remainder;

    if(len >= ctx->crypt->blk_size){
        ret = te_sca_update(ctx->crypt, false, len, in ,out);
        __CIPHER_CHECK_CONDITION__(ret);
    }

    if(0 < remainder){
        _tmp = (uint8_t *)osal_malloc_aligned(
                    UTILS_ROUND_UP(ctx->crypt->blk_size,TE_DMA_ALIGNED),
                    TE_DMA_ALIGNED);
        if( NULL == _tmp) {
            ret = TE_ERROR_OOM;
            __CIPHER_OUT__;
        }
        _tmp_raw = (uint8_t *)osal_calloc(1, ctx->crypt->blk_size);
        if( NULL == _tmp_raw) {
            ret = TE_ERROR_OOM;
            goto __cleanup__;
        }
        osal_memcpy(_tmp_raw, in + len, remainder);
        ret = te_sca_update(ctx->crypt, false, ctx->crypt->blk_size,
                            _tmp_raw ,_tmp);
        if (TE_SUCCESS != ret) {
            goto __cleanup1__;
        }
        osal_memcpy(out + len, _tmp, remainder);
        _xor_block(_tmp_raw, _tmp, prv_ctx->ofb.stream, ctx->crypt->blk_size);
    }else{
        _xor_block( (uint8_t *)in_last_block,
                    out + len - ctx->crypt->blk_size,
                    prv_ctx->ofb.stream,
                    ctx->crypt->blk_size    );
    }

    prv_ctx->ofb.strpos = remainder;
    if (iv_off) {
        *iv_off = prv_ctx->ofb.strpos;
    }
    osal_memcpy(iv, prv_ctx->ofb.stream, ctx->crypt->blk_size);
__cleanup1__:
    if (NULL != _tmp_raw) {
        osal_free(_tmp_raw);
    }
__cleanup__:
    if (NULL != _tmp) {
        osal_free(_tmp);
    }
__out__:
    return ret;
}
/**
 * AES/SM4 only
 */
int te_cipher_ofb_list( te_cipher_ctx_t *ctx,
                   size_t *iv_off,
                   uint8_t *iv,
                   te_memlist_t *in,
                   te_memlist_t *out )
{
    int ret = TE_SUCCESS;
    size_t remainder = 0;
    size_t len = 0;
    size_t __len = 0;
    uint8_t *_tmp = NULL;
    uint8_t *_tmp_raw = NULL;
    te_memlist_t _in = {0};
    te_memlist_t _out = {0};
    in_out_offs_t offs = {0};
    te_ml_bp_t in_info = {0};
    te_ml_bp_t out_info = {0};
    sca_cipher_ctx_t *prv_ctx = NULL;
    uint8_t in_last_block[TE_MAX_SCA_BLOCK] = {0};
    /* sanity check parameters */
    if ((NULL == ctx)
        || (NULL == ctx->crypt)
        || (NULL == iv)
        || (NULL == in)
        || (NULL == out)
        || (iv_off && (ctx->crypt->blk_size <= *iv_off))) {
            ret = TE_ERROR_BAD_PARAMS;
            __CIPHER_OUT__;
    }
    len = te_memlist_get_total_len(in);
    if (0 == len) {
        __CIPHER_OUT__;
    }
    if (len > te_memlist_get_total_len(out)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CIPHER_OUT__;
    }

    prv_ctx = (sca_cipher_ctx_t *)cipher_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        __CIPHER_OUT__;
    }
    ret = _te_cipher_ofb_start(ctx, iv_off, iv);
    __CIPHER_CHECK_CONDITION__(ret);
    if (prv_ctx->ofb.strpos) {
        remainder = ctx->crypt->blk_size - prv_ctx->ofb.strpos;
        /*consume unused stream block*/
        __len = _te_memlist_update_stream( out, in,
                                prv_ctx->ofb.stream + prv_ctx->ofb.strpos,
                                remainder, &offs);
        len -= __len;
    }

    if (len != 0) {
        _in.nent = in->nent - offs.in_ind;
        _in.ents = (te_mement_t *)osal_calloc(_in.nent,
                                                sizeof(te_mement_t));
        if (NULL == _in.ents) {
            ret = TE_ERROR_OOM;
            __CIPHER_OUT__;
        }
        osal_memcpy(_in.ents, &in->ents[offs.in_ind],
                    sizeof(te_mement_t) * _in.nent);
        _in.ents[0].buf = (uint8_t*)_in.ents[0].buf + offs.in_off;
        _in.ents[0].len -= offs.in_off;
        _out.nent = out->nent - offs.out_ind;
        _out.ents = (te_mement_t *)osal_calloc(_out.nent,
                                                sizeof(te_mement_t));
        if (NULL == _out.ents) {
            ret = TE_ERROR_OOM;
            goto __cleanup__;
        }
        osal_memcpy(_out.ents, &out->ents[offs.out_ind],
                    sizeof(te_mement_t) * _out.nent);
        _out.ents[0].buf = (uint8_t*)_out.ents[0].buf + offs.out_off;
        _out.ents[0].len -= offs.out_off;
    } else { /** len == 0*/
        prv_ctx->ofb.strpos = (prv_ctx->ofb.strpos + __len) %
                                                ctx->crypt->blk_size;
        if (iv_off) {
            *iv_off = prv_ctx->ofb.strpos;
        }
        __CIPHER_OUT__;
    }

    remainder = len % ctx->crypt->blk_size;
    if (0 == remainder) {
        te_memlist_copy_from_tail(in, in_last_block, ctx->crypt->blk_size);
    }
    len -= remainder;
    _tmp = (uint8_t *)osal_malloc_aligned(UTILS_ROUND_UP(ctx->crypt->blk_size,
                            TE_DMA_ALIGNED), TE_DMA_ALIGNED);
    if( NULL == _tmp) {
        goto __cleanup1__;
    }
    osal_memset(_tmp, 0x00, ctx->crypt->blk_size);
    _tmp_raw = (uint8_t *)osal_calloc(1, ctx->crypt->blk_size);
    if( NULL == _tmp_raw) {
        goto __cleanup2__;
    }
    if (0 < remainder) {
        /**< truncate in list to make it block aligned */
        te_memlist_truncate_from_tail( &_in,
                                        _tmp_raw,
                                        remainder,
                                        true,
                                        &in_info );
        /**< truncate out list to make it block aligned */
        te_memlist_truncate_from_head( &_out,
                                        len,
                                        &out_info );
    }

    if (len > 0) {
        ret = te_sca_uplist(ctx->crypt, false, &_in, &_out);
        if (TE_SUCCESS != ret) {
            /**< if no cut no need to recover */
            if (remainder) {
                /**< there're 2 scenarios. case#1 ind < 0 nothing left after truancated
                 *                         case#2 ind >= 0 some left after truancated
                 */
                if (in_info.ind < 0) {
                    _in.ents[0].len = in_info.len;
                } else {
                    _in.ents[in_info.ind].len = in_info.len;
                }
                _out.ents[out_info.ind].len = out_info.len;
                _out.nent = out_info.nent;
            }
            goto __cleanup3__;
        }
    }

    if(0 < remainder){
        /**< it's important to recovery the memory list to it's original,
         *   otherwise it will failed to fill out the remainder to out.
         *   And there're 2 scenarios.
         *       case#1 ind < 0 nothing left after truancated
         *       case#2 ind >= 0 some left  after truancated
         */
        if (in_info.ind < 0) {
            _in.ents[0].len = in_info.len;
        } else {
            _in.ents[in_info.ind].len = in_info.len;
        }
        _out.ents[out_info.ind].len = out_info.len;
        _out.nent = out_info.nent;
        ret = te_sca_update(ctx->crypt, false, ctx->crypt->blk_size,
                            _tmp_raw, _tmp);
        if (TE_SUCCESS != ret) {
            goto __cleanup3__;
        }
        _te_fill_linklist(&_out,
                            _tmp,
                            remainder,
                            out_info.ind,
                            out_info.offset);
        _xor_block(_tmp_raw, _tmp, prv_ctx->ofb.stream, ctx->crypt->blk_size);
    } else {
        ret = te_memlist_copy_from_tail( out, _tmp, ctx->crypt->blk_size );
        __CIPHER_CHECK_CONDITION__(ret);
        _xor_block(in_last_block, _tmp, prv_ctx->ofb.stream, ctx->crypt->blk_size);
    }

    prv_ctx->ofb.strpos = remainder;
    if (iv_off) {
        *iv_off = prv_ctx->ofb.strpos;
    }
    osal_memcpy(iv, prv_ctx->ofb.stream, ctx->crypt->blk_size);
__cleanup3__:
    if (NULL != _tmp_raw) {
        osal_free(_tmp_raw);
    }
__cleanup2__:
    if (NULL != _tmp) {
        osal_free(_tmp);
    }
__cleanup1__:
    if (NULL != _out.ents) {
        osal_free(_out.ents);
    }
__cleanup__:
    if (NULL != _in.ents) {
        osal_free(_in.ents);
    }
__out__:
    return ret;
}

/**
 * AES/SM4 only
 */
static int _te_cipher_ctr_start( te_cipher_ctx_t *ctx,
                                 size_t *nc_off,
                                 uint8_t *nonce_counter,
                                 uint8_t *stream_block )
{
    int ret = TE_SUCCESS;
    sca_cipher_ctx_t *prv_ctx = NULL;

    if ( (NULL == ctx) || (NULL == ctx->crypt) ) {
        return TE_ERROR_BAD_PARAMS;
    }
    prv_ctx = (sca_cipher_ctx_t *)cipher_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    /* check algorithm requirement, only support AES and SM4 */
    if((TE_MAIN_ALGO_AES != TE_ALG_GET_MAIN_ALG(ctx->crypt->alg))
        && (TE_MAIN_ALGO_SM4 != TE_ALG_GET_MAIN_ALG(ctx->crypt->alg))){
            ret = TE_ERROR_BAD_PARAMS;
            __CIPHER_OUT__;
    }
    ret = te_sca_state(ctx->crypt);
    switch (ret){
    default:
        __CIPHER_OUT__;
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
        ret = TE_ERROR_BAD_STATE;
        break;
    case TE_DRV_SCA_STATE_READY:
        osal_memset(&prv_ctx->ctr, 0x00, sizeof(prv_ctx->ctr));
        if (nonce_counter) {
            osal_memcpy(prv_ctx->ctr.iv, nonce_counter, ctx->crypt->blk_size);
        }
        ret = te_sca_start(ctx->crypt, TE_DRV_SCA_ENCRYPT, prv_ctx->ctr.iv,
                            ctx->crypt->blk_size);
        __CIPHER_CHECK_CONDITION__(ret);
        prv_ctx->pre_alg = ctx->crypt->alg;
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        if((ctx->crypt->alg != prv_ctx->pre_alg)
            || (stream_block && (osal_memcmp(stream_block, prv_ctx->ctr.stream,
                        ctx->crypt->blk_size) != 0))
            || (nc_off && (prv_ctx->ctr.strpos != *nc_off))
            || (osal_memcmp(nonce_counter, prv_ctx->ctr.iv,
                        ctx->crypt->blk_size) != 0)){
            ret = te_sca_finish(ctx->crypt, NULL, 0);
            __CIPHER_CHECK_CONDITION__(ret);
            osal_memset(&prv_ctx->ctr, 0x00, sizeof(prv_ctx->ctr));
            if (nonce_counter) {
                osal_memcpy(prv_ctx->ctr.iv, nonce_counter, ctx->crypt->blk_size);
            }
            ret = te_sca_start(ctx->crypt, TE_DRV_SCA_ENCRYPT, prv_ctx->ctr.iv,
                                ctx->crypt->blk_size);
            __CIPHER_CHECK_CONDITION__(ret);
            prv_ctx->pre_alg = ctx->crypt->alg;
        } else {
            ret = TE_SUCCESS;
        }
        break;
    }
    if (nc_off) {
        prv_ctx->ctr.strpos = *nc_off;
    }
    if (stream_block) {
        osal_memcpy( prv_ctx->ctr.stream, stream_block,
                     sizeof(prv_ctx->ctr.stream) );
    }
__out__:
    return ret;
}

/**
 * AES/SM4 only
 */
int te_cipher_ctr( te_cipher_ctx_t *ctx,
                   size_t len,
                   size_t *nc_off,
                   uint8_t *nonce_counter,
                   uint8_t *stream_block,
                   const uint8_t *in,
                   uint8_t *out )
{
    int ret = TE_SUCCESS;
    size_t remainder = 0;
    size_t __len = 0;
    uint8_t *_tmp = NULL;
    uint8_t *_tmp_raw = NULL;
    sca_cipher_ctx_t *prv_ctx = NULL;
    size_t n = 0;
    uint8_t in_last_block[TE_MAX_SCA_BLOCK] = {0};

    /* sanity check parameters */
    if ((NULL == ctx)
         || (NULL == ctx->crypt)
         || (NULL == in)
         || (NULL == out)
         || (NULL == nonce_counter)
         || (nc_off && (*nc_off >= ctx->crypt->blk_size))) {
            ret = TE_ERROR_BAD_PARAMS;
            __CIPHER_OUT__;
    }
    if (0 == len) {
        __CIPHER_OUT__;
    }
    prv_ctx = (sca_cipher_ctx_t *)cipher_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        __CIPHER_OUT__;
    }
    ret = _te_cipher_ctr_start(ctx, nc_off, nonce_counter, stream_block);
    __CIPHER_CHECK_CONDITION__(ret);
    /* consume unused data */
    if(prv_ctx->ctr.strpos){
        remainder = ctx->crypt->blk_size - prv_ctx->ctr.strpos;
        __len = (len >= remainder) ? remainder : len;
        osal_memcpy(out, in, __len);
        XOR_BLOCK(out, prv_ctx->ctr.stream + prv_ctx->ctr.strpos, __len);
        len -= __len;
        in += __len;
        out += __len;
    }
    /* if no data left update nc off and return */
    if(len == 0){
        prv_ctx->ctr.strpos = (prv_ctx->ctr.strpos + __len) %
                                                ctx->crypt->blk_size;
        if (nc_off)  {
            *nc_off = prv_ctx->ctr.strpos;
        }
        __CIPHER_OUT__;
    }

    remainder = len % ctx->crypt->blk_size;
    if (0 == remainder) {
        osal_memcpy(in_last_block,
                    in + len - ctx->crypt->blk_size,
                    ctx->crypt->blk_size);
    }
    len -= remainder;

    if(0 < len){
        ret = te_sca_update(ctx->crypt, false, len, in ,out);
        __CIPHER_CHECK_CONDITION__(ret);
    }

    if( 0 < remainder){
        _tmp = (uint8_t *)osal_calloc(1, TE_MAX_SCA_BLOCK);
        if (NULL == _tmp) {
            __CIPHER_OUT__;
        }
        _tmp_raw = (uint8_t *)osal_calloc(1, TE_MAX_SCA_BLOCK);
        if (NULL == _tmp_raw) {
            goto __cleanup__;
        }
        osal_memcpy(_tmp_raw, in + len, remainder);
        ret = te_sca_update(ctx->crypt, false, ctx->crypt->blk_size,
                            _tmp_raw, _tmp);
        if (TE_SUCCESS != ret) {
            goto __cleanup1__;
        }
        osal_memcpy(out + len, _tmp, remainder);
        _xor_block(_tmp, _tmp_raw, prv_ctx->ctr.stream, ctx->crypt->blk_size);
    }else{
        _xor_block( out + len - ctx->crypt->blk_size,
                    in_last_block,
                    prv_ctx->ctr.stream,
                    ctx->crypt->blk_size );
    }

    /* update nouce counter and stream buffer */
    n = (UTILS_ROUND_UP(len + remainder, ctx->crypt->blk_size)) /
                        ctx->crypt->blk_size;
    COUNTER_INCREASE(nonce_counter, ctx->crypt->blk_size, n);
    osal_memcpy(prv_ctx->ctr.iv, nonce_counter, ctx->crypt->blk_size);
    prv_ctx->ctr.strpos = remainder;
    if (nc_off) {
        *nc_off = prv_ctx->ctr.strpos;
    }
    if (stream_block) {
        osal_memcpy(stream_block, prv_ctx->ctr.stream, ctx->crypt->blk_size);
    }
__cleanup1__:
    if (NULL != _tmp_raw) {
        osal_free(_tmp_raw);
    }
__cleanup__:
    if (NULL != _tmp) {
        osal_free(_tmp);
    }
__out__:
    return ret;
}

/**
 * AES/SM4 only
 */
int te_cipher_ctr_list( te_cipher_ctx_t *ctx,
                   size_t *nc_off,
                   uint8_t *nonce_counter,
                   uint8_t *stream_block,
                   te_memlist_t *in,
                   te_memlist_t *out )
{
    int ret = TE_SUCCESS;
    sca_cipher_ctx_t *prv_ctx = NULL;
    size_t remainder = 0;
    size_t __len = 0;
    uint8_t *_tmp = NULL;
    uint8_t *_tmp_raw = NULL;
    size_t len = 0;
    te_memlist_t _in = {0};
    te_memlist_t _out = {0};
    in_out_offs_t offs = {0};
    te_ml_bp_t in_info = {0};
    te_ml_bp_t out_info = {0};
    size_t n = 0;
    uint8_t in_last_block[TE_MAX_SCA_BLOCK] = {0};
    /* sanity check parameters */
    if ((NULL == ctx)
        || (NULL == ctx->crypt)
        || (NULL == nonce_counter)
        || (NULL == in)
        || (NULL == out)
        || (nc_off && *nc_off >= ctx->crypt->blk_size)) {
            ret = TE_ERROR_BAD_PARAMS;
            __CIPHER_OUT__;
    }
    len = te_memlist_get_total_len(in);
    if (0 == len) {
        __CIPHER_OUT__;
    }
    if(len > te_memlist_get_total_len(out)){
        ret = TE_ERROR_BAD_PARAMS;
        __CIPHER_OUT__;
    }

    prv_ctx = (sca_cipher_ctx_t *)cipher_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        __CIPHER_OUT__;
    }
    ret = _te_cipher_ctr_start(ctx, nc_off, nonce_counter, stream_block);
    __CIPHER_CHECK_CONDITION__(ret);
    if(prv_ctx->ctr.strpos){
        remainder = ctx->crypt->blk_size - prv_ctx->ctr.strpos;
        /*consume unused stream block*/
        __len = _te_memlist_update_stream( out, in,
                                    prv_ctx->ctr.stream + prv_ctx->ctr.strpos,
                                          remainder, &offs );
        len -= __len;
    }

    if (len != 0) {
        _in.nent = in->nent - offs.in_ind;
        _in.ents = (te_mement_t *)osal_calloc( _in.nent,
                                               sizeof(te_mement_t) );
        if (NULL == _in.ents) {
            ret = TE_ERROR_OOM;
            __CIPHER_OUT__;
        }
        osal_memcpy(_in.ents, &in->ents[offs.in_ind],
                    sizeof(te_mement_t) * _in.nent);
        _in.ents[0].buf = (uint8_t*)_in.ents[0].buf + offs.in_off;
        _in.ents[0].len -= offs.in_off;
        _out.nent =out->nent - offs.out_ind;
        _out.ents = (te_mement_t *)osal_calloc(_out.nent,
                                                sizeof(te_mement_t));
        if (NULL == _out.ents) {
            ret = TE_ERROR_OOM;
            goto __cleanup__;
        }
        osal_memcpy(_out.ents, &out->ents[offs.out_ind],
                    sizeof(te_mement_t) * _out.nent);
        _out.ents[0].buf = (uint8_t*)_out.ents[0].buf + offs.out_off;
        _out.ents[0].len -= offs.out_off;
    } else { /** len == 0 */
        prv_ctx->ctr.strpos =  (prv_ctx->ctr.strpos + __len) %
                                                ctx->crypt->blk_size;
        if (nc_off) {
            *nc_off = prv_ctx->ctr.strpos;
        }
        __CIPHER_OUT__;
    }

    remainder = len % ctx->crypt->blk_size;
    if (0 == remainder) {
        te_memlist_copy_from_tail(in, in_last_block, ctx->crypt->blk_size);
    }
    len -= remainder;
    _tmp = (uint8_t *)osal_calloc(1, SCA_MAX_BLOCK_SIZE);
    if( NULL == _tmp) {
        ret = TE_ERROR_OOM;
        goto __cleanup1__;
    }
    _tmp_raw = (uint8_t *)osal_calloc(1, SCA_MAX_BLOCK_SIZE);
    if( NULL == _tmp_raw) {
        ret = TE_ERROR_OOM;
        goto __cleanup2__;
    }

    if(0 < remainder){
        /**< truncate in list to make it block aligned */
        te_memlist_truncate_from_tail( &_in,
                                         _tmp_raw,
                                         remainder,
                                         true,
                                         &in_info);
        /**< truncate out list to make it block aligned */
        te_memlist_truncate_from_head( &_out,
                                         len,
                                         &out_info);

    }

    if (len > 0) {
        ret = te_sca_uplist(ctx->crypt, false, &_in, &_out);
        if (TE_SUCCESS != ret) {
            /**< if no cut no need to recover */
            if (remainder) {
                /**< there're 2 scenarios. case#1 ind < 0 nothing left after truancated
                 *                         case#2 ind >= 0 left some after truancated
                 */
                if (in_info.ind < 0) {
                    _in.ents[0].len = in_info.len;
                } else {
                    _in.ents[in_info.ind].len = in_info.len;
                }
                _out.ents[out_info.ind].len = out_info.len;
                _out.nent = out_info.nent;
            }
            goto __cleanup3__;
        }
    }

    if(0 < remainder){
        /**< it's important to recovery the memory list to it's original,
         *   otherwise it will failed to fill out the remainder to out.
         *   And there're 2 scenarios.
         *       case#1 ind < 0 nothing left after truancated
         *       case#2 ind >= 0 left some after truancated
         */
        if (in_info.ind < 0) {
            _in.ents[0].len = in_info.len;
        } else {
            _in.ents[in_info.ind].len = in_info.len;
        }
        _out.ents[out_info.ind].len = out_info.len;
        _out.nent = out_info.nent;
        ret = te_sca_update(ctx->crypt, false, ctx->crypt->blk_size,
                            _tmp_raw, _tmp);
        if (TE_SUCCESS != ret) {
            goto __cleanup3__;
        }
        _te_fill_linklist( &_out,
                           _tmp,
                           remainder,
                           out_info.ind,
                           out_info.offset );
        _xor_block(_tmp, _tmp_raw, prv_ctx->ctr.stream, ctx->crypt->blk_size);
    } else {
        te_memlist_copy_from_tail(&_out, _tmp, ctx->crypt->blk_size);
        _xor_block(_tmp, in_last_block, prv_ctx->ctr.stream, ctx->crypt->blk_size);
    }
    /* update nouce counter and stream buffer */
    n = UTILS_ROUND_UP(len + remainder, ctx->crypt->blk_size) /
                        ctx->crypt->blk_size;
    COUNTER_INCREASE(nonce_counter, ctx->crypt->blk_size, n);
    osal_memcpy(prv_ctx->ctr.iv, nonce_counter, ctx->crypt->blk_size);
    prv_ctx->ctr.strpos = remainder;
    if (nc_off) {
        *nc_off = prv_ctx->ctr.strpos;
    }
    if (stream_block) {
        osal_memcpy(stream_block, prv_ctx->ctr.stream, ctx->crypt->blk_size);
    }
__cleanup3__:
    if (NULL != _tmp_raw) {
        osal_free(_tmp_raw);
    }
__cleanup2__:
    if (NULL != _tmp) {
        osal_free(_tmp);
    }
__cleanup1__:
    if (NULL != _out.ents) {
        osal_free(_out.ents);
    }
__cleanup__:
    if (NULL != _in.ents) {
        osal_free(_in.ents);
    }
__out__:
    return ret;
}

int te_cipher_clone( const te_cipher_ctx_t *src,
                     te_cipher_ctx_t *dst )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;
    sca_cipher_ctx_t *spctx = NULL;
    sca_cipher_ctx_t *dpctx = NULL;

    if (NULL == src || NULL == src->crypt || NULL == dst) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * free the dst ctx if it points to a valid cipher ctx already
     */
    if (dst->crypt != NULL) {
        ret = te_cipher_free(dst);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_cipher_free error %x\n", ret);
            return ret;
        }
    }

    spctx = (sca_cipher_ctx_t *)cipher_priv_ctx((te_cipher_ctx_t*)src);
    if(NULL == spctx){
        return TE_ERROR_BAD_PARAMS;
    }

    sdrv = (te_sca_drv_t *)src->crypt->drv;
    ret = te_sca_alloc_ctx(sdrv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_cipher_ctx_t),
                           &dst->crypt);
    if (ret != TE_SUCCESS) {
        return ret;
    }

    dpctx = (sca_cipher_ctx_t *)cipher_priv_ctx(dst);
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

#ifdef CFG_TE_ASYNC_EN

typedef struct acipher_ctx {
    te_cipher_ctx_t *ctx;
    te_cipher_request_t *req;
}te_acipher_ctx_t;

static void execute_cipher_ecb_crypt(te_worker_task_t *task)
{
    int ret = TE_SUCCESS;
    te_acipher_ctx_t *aecb = task->param;
    te_cipher_ctx_t *ctx = aecb->ctx;
    te_cipher_request_t *req = aecb->req;

    ret = te_cipher_ecb_list( ctx, req->op, &req->src, &req->dst);
    osal_free(task);
    osal_free(aecb);

    req->res = ret;
    req->base.completion( &req->base, req->res );
    return;
}

int te_cipher_aecb(te_cipher_ctx_t *ctx, te_cipher_request_t *req)
{
    int ret = TE_SUCCESS;
    te_acipher_ctx_t *aecb = NULL;
    te_worker_task_t *task = NULL;
    if((NULL == ctx)
        || (NULL == ctx->crypt)
        || (NULL == req)) {
        ret = TE_ERROR_OOM;
        __CIPHER_OUT__;
    }

    aecb = osal_calloc(1, sizeof(*aecb));
    if (NULL == aecb) {
        ret = TE_ERROR_OOM;
        __CIPHER_OUT__;
    }

    task = osal_calloc(1, sizeof(*task));
    if (NULL == task) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    task->param = (void *)aecb;
    task->execute = execute_cipher_ecb_crypt;
    aecb->ctx = ctx;
    aecb->req = req;
    te_worker_pool_enqueue(task);

    return TE_SUCCESS;

err1:
    osal_free(aecb);
__out__:
    return ret;
}

static void execute_cipher_cbc_crypt(te_worker_task_t *task)
{
    int ret = TE_SUCCESS;
    te_acipher_ctx_t *acbc = task->param;
    te_cipher_ctx_t *ctx = acbc->ctx;
    te_cipher_request_t *req = acbc->req;

    ret = te_cipher_cbc_list(ctx, req->op, req->iv, &req->src, &req->dst);
    osal_free(task);
    osal_free(acbc);

    req->res = ret;
    req->base.completion(&req->base, req->res);
    return;
}

int te_cipher_acbc( te_cipher_ctx_t *ctx,
                    te_cipher_request_t *req )
{
    int ret = TE_SUCCESS;
    te_acipher_ctx_t *acbc = NULL;
    te_worker_task_t *task = NULL;
    if((NULL == ctx) || (NULL == ctx->crypt) || (NULL == req)) {
        ret = TE_ERROR_OOM;
        __CIPHER_OUT__;
    }

    acbc = osal_calloc(1, sizeof(*acbc));
    if (NULL == acbc) {
        ret = TE_ERROR_OOM;
        __CIPHER_OUT__;
    }

    task = osal_calloc(1, sizeof(*task));
    if (NULL == task) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    task->param = (void *)acbc;
    task->execute = execute_cipher_cbc_crypt;
    acbc->ctx = ctx;
    acbc->req = req;
    te_worker_pool_enqueue(task);

    return TE_SUCCESS;

err1:
    osal_free(acbc);
__out__:
    return ret;
}

static void execute_cipher_ofb_crypt(te_worker_task_t *task)
{
    int ret = TE_SUCCESS;
    te_acipher_ctx_t *aofb = task->param;
    te_cipher_ctx_t *ctx = aofb->ctx;
    te_cipher_request_t *req = aofb->req;

    ret = te_cipher_ofb_list(ctx, req->off, req->iv, &req->src, &req->dst);
    osal_free(task);
    osal_free(aofb);

    req->res = ret;
    req->base.completion(&req->base, req->res);
    return;
}

/**
 * AES/SM4 only
 */
int te_cipher_aofb( te_cipher_ctx_t *ctx,
                    te_cipher_request_t *req )
{
    int ret = TE_SUCCESS;
    te_acipher_ctx_t *aofb = NULL;
    te_worker_task_t *task = NULL;
    if((NULL == ctx)
        || (NULL == ctx->crypt)
        || (NULL == req)) {
        ret = TE_ERROR_OOM;
        __CIPHER_OUT__;
    }

    aofb = osal_calloc(1, sizeof(*aofb));
    if (NULL == aofb) {
        ret = TE_ERROR_OOM;
        __CIPHER_OUT__;
    }

    task = osal_calloc(1, sizeof(*task));
    if (NULL == task) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    task->param = (void *)aofb;
    task->execute = execute_cipher_ofb_crypt;
    aofb->ctx = ctx;
    aofb->req = req;
    te_worker_pool_enqueue(task);

    return TE_SUCCESS;

err1:
    osal_free(aofb);
__out__:
    return ret;
}

static void execute_cipher_ctr_crypt(te_worker_task_t *task)
{
    int ret = TE_SUCCESS;
    te_acipher_ctx_t *actr = task->param;
    te_cipher_ctx_t *ctx = actr->ctx;
    te_cipher_request_t *req = actr->req;

    ret = te_cipher_ctr_list(ctx, req->off, req->iv,
                             req->stream, &req->src, &req->dst);
    osal_free(task);
    osal_free(actr);

    req->res = ret;
    req->base.completion(&req->base, req->res);
    return;
}

/**
 * AES/SM4 only
 */
int te_cipher_actr(te_cipher_ctx_t *ctx, te_cipher_request_t *req)
{
    int ret = TE_SUCCESS;
    te_acipher_ctx_t *actr = NULL;
    te_worker_task_t *task = NULL;
    if((NULL == ctx) || (NULL == ctx->crypt) || (NULL == req)) {
        ret = TE_ERROR_OOM;
        __CIPHER_OUT__;
    }

    actr = osal_calloc(1, sizeof(*actr));
    if (NULL == actr) {
        ret = TE_ERROR_OOM;
        __CIPHER_OUT__;
    }

    task = osal_calloc(1, sizeof(*task));
    if (NULL == task) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    task->param = (void *)actr;
    task->execute = execute_cipher_ctr_crypt;
    actr->ctx = ctx;
    actr->req = req;
    te_worker_pool_enqueue(task);

    return TE_SUCCESS;

err1:
    osal_free(actr);
__out__:
    return ret;
}
#endif /* CFG_TE_ASYNC_EN */
