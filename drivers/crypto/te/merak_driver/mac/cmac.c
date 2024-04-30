//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_cmac.h>
#include <te_cipher.h>
#include "../common/te_worker_pool.h"

#define BYTE_BITS           (8U)

/**
 * cmac user key structure
 */
typedef struct sca_cmac_user_key {
    uint8_t key[TE_MAX_SCA_KEY];    /**< key data */
    uint32_t keybits;               /**< key length in bits */
} sca_cmac_user_key_t;

/**
 * cmac key structure
 */
typedef struct sca_cmac_key {
    te_key_type_t type;             /**< key type */
    /**
     * key descriptor
     */
    union {
        te_sec_key_t sec;           /**< secure key */
        te_sec_key_v2_t sec_v2;     /**< secure key for key-ladder 256 */
        sca_cmac_user_key_t user;   /**< user key */
    };
} sca_cmac_key_t;

/**
 * SCA CMAC private context
 * Note CMAC driver need to buffer at least 1x non-empty blk data,
 * either complete or non-complete, before feeding to the CMAC engine.
 * This is for the CMAC engine always requires for the last process
 * command with effective data before the finish command.
 */
typedef struct sca_cmac_ctx {
    uint8_t iv[SCA_MAX_BLOCK_SIZE];     /**< initial vector */
    uint8_t npdata[SCA_MAX_BLOCK_SIZE]; /**< not processed data */
    uint32_t npdlen;                    /**< data length in byte */
    size_t mlen;                        /**< processed message length in byte */
    sca_cmac_key_t key;                 /**< cipher key */
    te_drv_handle hdl;                  /**< driver handle */
} sca_cmac_ctx_t;

#define CMAC_EHDR_SIZE(x)   (sizeof(cmac_ehdr_t) + (x)->drvctx_sz)
#define CMAC_EHDR_DRVCTX(x) (uint8_t *)(((cmac_ehdr_t *)(x)) + 1)

/**
 * CMAC export state header magic number
 */
#define CMAC_EHDR_MAGIC     0x48456d43U /**< "CmEH" */

/**
 * CMAC export state header structure
 */
typedef struct cmac_export_hdr {
    uint32_t magic;                     /**< magic */
    uint8_t npdata[SCA_MAX_BLOCK_SIZE]; /**< not processed data */
    uint32_t npdlen;
    uint32_t drvctx_sz;                 /**< drvctx size in byte */
    sca_cmac_key_t key;                 /**< cipher key */
    size_t mlen;                        /**< processed message length in byte */
    /*
     * Commented out element used to visualize the layout dynamic part
     * of the struct.
     *
     * uint8_t drvctx[];
     */
} cmac_ehdr_t;

#define __CMAC_OUT__    goto __out__

#define __CMAC_CHECK_CONDITION__(_ret_)                                        \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 __CMAC_OUT__;                                                 \
            }                                                                  \
        } while (0);

#define __CMAC_ALERT__(_ret_, _msg_)                                           \
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
int te_cmac_init( te_cmac_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;
    sca_cmac_ctx_t *prv_ctx = NULL;

    if(NULL == ctx
       || ((TE_MAIN_ALGO_SM4 != malg) && (TE_MAIN_ALGO_AES != malg)
            && (TE_MAIN_ALGO_DES != malg) && (TE_MAIN_ALGO_TDES != malg))){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    ret = te_sca_alloc_ctx(sdrv,
                           malg,
                           sizeof(sca_cmac_ctx_t),
                           &ctx->crypt);
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
    __CMAC_CHECK_CONDITION__(ret);
    prv_ctx = cmac_priv_ctx(ctx);
    TE_ASSERT(prv_ctx != NULL);
    prv_ctx->hdl = hdl;

    switch (malg)
    {
    default:
    case TE_MAIN_ALGO_AES:
        ctx->crypt->alg = TE_ALG_AES_CMAC;
        break;
    case TE_MAIN_ALGO_SM4:
        ctx->crypt->alg = TE_ALG_SM4_CMAC;
        break;
    case TE_MAIN_ALGO_DES:
        ctx->crypt->alg = TE_ALG_DES_CMAC;
        break;
    case TE_MAIN_ALGO_TDES:
        ctx->crypt->alg = TE_ALG_TDES_CMAC;
        break;
    }
__out__:
    return ret;
}

int te_cmac_free( te_cmac_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    sca_cmac_ctx_t *prv_ctx = NULL;

    if(NULL == ctx || !ctx->crypt){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    prv_ctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);
    if (!prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        __CMAC_OUT__;
    }
    ret = te_sca_state(ctx->crypt);

    switch (ret){
    default:
        __CMAC_OUT__;
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        ret = TE_SUCCESS;
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
        /* The last update is always required before finish() for cmac */
        ret = te_sca_update( ctx->crypt, true, ctx->crypt->blk_size,
                             prv_ctx->npdata, NULL );
        __CMAC_ALERT__(ret, "te_sca_update raised exceptions!");
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __CMAC_ALERT__(ret, "te_sca_finish raised exceptions!");
        break;
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        __CMAC_ALERT__(ret, "te_sca_finish raised exceptions!");
        break;
    }

    osal_memset(&prv_ctx->key, 0x00, sizeof(prv_ctx->key));
    ret = te_sca_free_ctx(ctx->crypt);
    __CMAC_ALERT__(ret, "te_sca_free_ctx raised exceptions!");
    ctx->crypt = NULL;
__out__:
    return ret;
}

int te_cmac_setkey( te_cmac_ctx_t *ctx,
                    const uint8_t *key,
                    uint32_t keybits )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};
    sca_cmac_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == key) || !ctx->crypt){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    prv_ctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);
    if (!prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }
    switch (te_sca_state(ctx->crypt)){
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        __CMAC_OUT__;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
        /* The last update is always required before finish() for cmac */
        ret = te_sca_update( ctx->crypt, true, ctx->crypt->blk_size,
                             prv_ctx->npdata, NULL );
    __CMAC_CHECK_CONDITION__(ret);
        ret = te_sca_finish(ctx->crypt, NULL, 0);
    __CMAC_CHECK_CONDITION__(ret);
        break;
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
    __CMAC_CHECK_CONDITION__(ret);
        break;
    }
    key_desc.type = TE_KEY_TYPE_USER;
    key_desc.user.key = (uint8_t *)key;
    key_desc.user.keybits = keybits;
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    __CMAC_CHECK_CONDITION__(ret);
    /**< save cipher key for mlen = 0 scenario*/
    prv_ctx->key.type = TE_KEY_TYPE_USER;
    osal_memcpy(prv_ctx->key.user.key, key, keybits / BYTE_BITS);
    prv_ctx->key.user.keybits = keybits;
__out__:
    return ret;
}

int te_cmac_setseckey( te_cmac_ctx_t *ctx,
                       te_sec_key_t *key )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};
    sca_cmac_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == key) || !ctx->crypt){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    prv_ctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);
    if (!prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    switch (te_sca_state(ctx->crypt)){
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        __CMAC_OUT__;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
        /* The last update is always required before finish() for cmac */
        ret = te_sca_update( ctx->crypt, true, ctx->crypt->blk_size,
                             prv_ctx->npdata, NULL );
    __CMAC_CHECK_CONDITION__(ret);
        ret = te_sca_finish(ctx->crypt, NULL, 0);
    __CMAC_CHECK_CONDITION__(ret);
        break;
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
    __CMAC_CHECK_CONDITION__(ret);
        break;
    }
    key_desc.type = TE_KEY_TYPE_SEC;
    memcpy(&key_desc.sec, key, sizeof(te_sec_key_t));
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    __CMAC_CHECK_CONDITION__(ret);
    /**< save cipher key for mlen = 0 scenario*/
    prv_ctx->key.type = TE_KEY_TYPE_SEC;
    osal_memcpy(&prv_ctx->key.sec, key, sizeof(prv_ctx->key.sec));
__out__:
    return ret;
}

int te_cmac_setseckey_v2( te_cmac_ctx_t *ctx,
                          te_sec_key_v2_t *key )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};
    sca_cmac_ctx_t *prv_ctx = NULL;
    /* AES only */
    if ( (NULL == ctx) || (NULL == key) || !ctx->crypt ||
         (ctx->crypt->alg != TE_ALG_AES_CMAC) ){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    prv_ctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);
    if (!prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    switch (te_sca_state(ctx->crypt)){
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        __CMAC_OUT__;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
        /* The last update is always required before finish() for cmac */
        ret = te_sca_update( ctx->crypt, true, ctx->crypt->blk_size,
                             prv_ctx->npdata, NULL );
    __CMAC_CHECK_CONDITION__(ret);
        ret = te_sca_finish(ctx->crypt, NULL, 0);
    __CMAC_CHECK_CONDITION__(ret);
        break;
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
    __CMAC_CHECK_CONDITION__(ret);
        break;
    }
    key_desc.type = TE_KEY_TYPE_SEC_V2;
    memcpy(&key_desc.sec_v2, key, sizeof(te_sec_key_v2_t));
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    __CMAC_CHECK_CONDITION__(ret);
    /**< save cipher key for mlen = 0 scenario*/
    prv_ctx->key.type = TE_KEY_TYPE_SEC_V2;
    osal_memcpy(&prv_ctx->key.sec_v2, key, sizeof(prv_ctx->key.sec_v2));
__out__:
    return ret;
}

int te_cmac_start( te_cmac_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    sca_cmac_ctx_t *prv_ctx = NULL;

    if(NULL == ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }
    prv_ctx = cmac_priv_ctx(ctx);
    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }
    osal_memset(prv_ctx->iv, 0x00, ctx->crypt->blk_size);
    prv_ctx->mlen = 0;
    prv_ctx->npdlen = 0;
    ret = te_sca_start(ctx->crypt, TE_DRV_SCA_ENCRYPT, prv_ctx->iv,
                       ctx->crypt->blk_size);
__out__:
    return ret;
}

int te_cmac_update( te_cmac_ctx_t *ctx,
                    size_t len,
                    const uint8_t *in )
{
    int ret = TE_SUCCESS;
    sca_cmac_ctx_t *prv_ctx = NULL;
    size_t remainder = 0;
    size_t num = len;
    const uint8_t *tin = in;

    if(NULL == ctx || NULL == ctx->crypt){
        return TE_ERROR_BAD_PARAMS;
    }

    if(NULL == in && 0 != len){
        return TE_ERROR_BAD_INPUT_DATA;
    }

    if(0 == len){
        return TE_SUCCESS;
    }

    prv_ctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);

    if(NULL == prv_ctx){
        return TE_ERROR_BAD_PARAMS;
    }

    /* still data left? */
    if(0 < prv_ctx->npdlen){
        size_t l = (num > (ctx->crypt->blk_size - prv_ctx->npdlen)) \
                   ? (ctx->crypt->blk_size - prv_ctx->npdlen) : num;
        osal_memcpy(prv_ctx->npdata + prv_ctx->npdlen, tin, l);
        num -= l;
        tin += l;
        prv_ctx->npdlen += l;
    }

    if (0 == num) {
        __CMAC_OUT__;
    }
    /* if fullfill npdata then update npdata */
    if(ctx->crypt->blk_size == prv_ctx->npdlen){
        ret = te_sca_update(ctx->crypt,
                            false,
                            prv_ctx->npdlen,
                            prv_ctx->npdata,
                            NULL);
        __CMAC_CHECK_CONDITION__(ret);
        prv_ctx->npdlen = 0;
    }
    remainder = num % ctx->crypt->blk_size;
    /* because for cmac last update is mandatory so we keep some
     * to feed last block */
    if (0 == remainder) {
        remainder = ctx->crypt->blk_size;
    }
    num -= remainder;

    if(0 < num){
        ret = te_sca_update(ctx->crypt,
                                false,
                                num,
                                tin,
                                NULL);
        __CMAC_CHECK_CONDITION__(ret);
    }

    if(0 < remainder){
        prv_ctx->npdlen = remainder;
        memcpy(prv_ctx->npdata,
              tin + num,
              remainder);
    }

__out__:
    prv_ctx->mlen += len;
    return ret;
}


int te_cmac_uplist( te_cmac_ctx_t *ctx,
                    te_memlist_t *in )
{
    int ret = TE_SUCCESS;
    sca_cmac_ctx_t *prv_ctx = NULL;
    size_t _i = 0;
    te_memlist_t _in = {0};
    size_t remainder = 0;
    size_t _total_size = 0;
    te_ml_bp_t in_info = {0};
    uint8_t tmp[TE_MAX_SCA_BLOCK] = {0};

    if(NULL == ctx || NULL == ctx->crypt){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    if (NULL == in) {
        ret = TE_SUCCESS;
        __CMAC_OUT__;
    }

    if(in->nent && !in->ents){
        ret = TE_ERROR_BAD_INPUT_DATA;
        __CMAC_OUT__;
    }

    for ( _i = 0; _i < in->nent; _i++) {
        _total_size += in->ents[_i].len;
    }
    if(_total_size == 0){
        ret = TE_SUCCESS;
        __CMAC_OUT__;
    }

    prv_ctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);
    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    if ((_total_size + prv_ctx->npdlen) <= ctx->crypt->blk_size) {
        /*just copy to npdata and then out*/
        for ( _i = 0; _i < in->nent; _i++) {
            memcpy(prv_ctx->npdata + prv_ctx->npdlen,
                   in->ents[_i].buf,
                   in->ents[_i].len);
            prv_ctx->npdlen += in->ents[_i].len;
        }
    }else{
        remainder = (_total_size + prv_ctx->npdlen) % ctx->crypt->blk_size;
        if (0 == remainder) {
            remainder = ctx->crypt->blk_size;
        }
        te_memlist_truncate_from_tail(in, tmp, remainder,
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
        in->ents[in_info.ind < 0 ? 0 : in_info.ind].len = in_info.len;
        __CMAC_CHECK_CONDITION__(ret);
        osal_memcpy(prv_ctx->npdata, tmp, remainder);
        prv_ctx->npdlen = remainder;
    }

    prv_ctx->mlen += _total_size;
__out__:
    osal_free(_in.ents);
    return ret;
}

static void xor_block( uint8_t *a, uint8_t *b, uint8_t *out, size_t len )
{
    size_t i;
    for (i = 0; i < len; i++) {
        out[i] = a[i] ^ b[i];
    }
}

/*
 * Multiplication by u in the Galois field of GF(2^n)
 *
 * As explained in NIST SP 800-38B, this can be computed:
 *
 *   If MSB(p) = 0, then p = (p << 1)
 *   If MSB(p) = 1, then p = (p << 1) ^ R_n
 *   with R_64 = 0x1B and  R_128 = 0x87
 *
 * Input and output MUST NOT point to the same buffer
 * Block size must be 8 bytes or 16 bytes - the block sizes for DES and AES.
 */
static int cmac_multiply_by_u( uint8_t *output,
                               const uint8_t *input,
                               te_cmac_ctx_t *ctx )
{
    const uint8_t R_128 = 0x87;
    const uint8_t R_64 = 0x1B;
    uint8_t R_n = 0;
    uint8_t mask = 0;
    uint8_t overflow = 0x00;
    int i = 0;

    if (!ctx || !ctx->crypt) {
        return TE_ERROR_BAD_PARAMS;
    }

    switch (TE_ALG_GET_MAIN_ALG(ctx->crypt->alg)) {
        case TE_MAIN_ALGO_AES:
        case TE_MAIN_ALGO_SM4:
            R_n = R_128;
        break;
        case TE_MAIN_ALGO_DES:
        case TE_MAIN_ALGO_TDES:
            R_n = R_64;
            break;
        default:
            return TE_ERROR_UNKNOWN_ALG;
    }

    for( i = (int)ctx->crypt->blk_size - 1; i >= 0; i-- )
    {
        output[i] = input[i] << 1 | overflow;
        overflow = input[i] >> 7;
    }
    mask = - ( input[0] >> 7 );
    output[ ctx->crypt->blk_size - 1 ] ^= R_n & mask;

    return TE_SUCCESS;
}

static int _te_cmac_generate_subkey( te_cipher_ctx_t *ctx,
                                     uint8_t *K1,
                                     uint8_t *K2 )
{
    uint8_t *L = NULL;
    uint8_t *Z = NULL;

    int ret = TE_SUCCESS;
    Z = (uint8_t *)osal_calloc(ctx->crypt->blk_size, 1);
    TE_ASSERT(Z != NULL);
    L = (uint8_t *)osal_malloc_aligned(UTILS_ROUND_UP(ctx->crypt->blk_size,
                                TE_DMA_ALIGNED), TE_DMA_ALIGNED);
    TE_ASSERT(L != NULL);
    osal_memset(L, 0x00, ctx->crypt->blk_size);
    ret = te_cipher_ecb(ctx, TE_DRV_SCA_ENCRYPT, ctx->crypt->blk_size, Z, L);
    __CMAC_CHECK_CONDITION__(ret);
    /*
     * Generate K1 and K2
     */
    if( ( ret = cmac_multiply_by_u( K1, L , ctx ) ) != 0 )
        goto exit;

    if( ( ret = cmac_multiply_by_u( K2, K1 , ctx ) ) != 0 )
        goto exit;

exit:
    osal_memset( L, 0x00, ctx->crypt->blk_size );
__out__:
    if (Z != NULL) {
        osal_free(Z);
    }
    if (L != NULL) {
        osal_free(L);
    }
    return ret;
}

static int _te_cmac ( te_cmac_ctx_t *ctx, uint8_t *mac, size_t maclen )
{
    uint8_t *m_last = NULL;
    int ret = TE_SUCCESS;
    uint8_t K1[TE_MAX_SCA_BLOCK] = {0};
    uint8_t K2[TE_MAX_SCA_BLOCK] = {0};
    te_cipher_ctx_t cctx = {0};
    sca_cmac_ctx_t *prv_ctx = NULL;

    if (maclen > ctx->crypt->blk_size) {
        return TE_ERROR_BAD_INPUT_LENGTH;
    }
    prv_ctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    ret = te_cipher_init(&cctx, prv_ctx->hdl, TE_ALG_GET_MAIN_ALG(ctx->crypt->alg));
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("%s +%d te_cipher_init failed:%X\n", __FILE__, __LINE__, ret);
        return ret;
    }
    switch (TE_ALG_GET_MAIN_ALG(ctx->crypt->alg)) {
    default:
    case TE_MAIN_ALGO_AES:
        cctx.crypt->alg = TE_ALG_AES_ECB_NOPAD;
        break;
    case TE_MAIN_ALGO_SM4:
        cctx.crypt->alg = TE_ALG_SM4_ECB_NOPAD;
        break;
    case TE_MAIN_ALGO_DES:
        cctx.crypt->alg = TE_ALG_DES_ECB_NOPAD;
        break;
    case TE_MAIN_ALGO_TDES:
        cctx.crypt->alg = TE_ALG_TDES_ECB_NOPAD;
        break;
    }
    if (prv_ctx->key.type == TE_KEY_TYPE_SEC) {
        ret = te_cipher_setseckey(&cctx, &prv_ctx->key.sec);
    } else if (prv_ctx->key.type == TE_KEY_TYPE_SEC_V2) {
        ret = te_cipher_setseckey_v2(&cctx, &prv_ctx->key.sec_v2);
    } else {
        ret = te_cipher_setkey(&cctx, prv_ctx->key.user.key, prv_ctx->key.user.keybits);
    }
    if (TE_SUCCESS != ret) {
        goto err_setkey;
    }
    ret = _te_cmac_generate_subkey(&cctx, K1, K2);
    if (TE_SUCCESS != ret) {
        goto err_gen_sub_key;
    }

    m_last = (uint8_t *)osal_malloc_aligned(UTILS_ROUND_UP(cctx.crypt->blk_size,
                                TE_DMA_ALIGNED), TE_DMA_ALIGNED);
    TE_ASSERT(m_last != NULL);
    osal_memset(m_last, 0x00, cctx.crypt->blk_size);
    m_last[0] = 0x80;
    xor_block(m_last, K2, m_last, cctx.crypt->blk_size);
    ret = te_cipher_ecb(&cctx, TE_DRV_SCA_ENCRYPT,
                        cctx.crypt->blk_size, m_last, m_last);
    if (TE_SUCCESS == ret) {
        osal_memcpy(mac, m_last, maclen);
    }
    if (m_last != NULL) {
        osal_free(m_last);
        m_last = NULL;
    }

err_gen_sub_key:
err_setkey:
    te_cipher_free(&cctx);
    return ret;
}

int te_cmac_finish( te_cmac_ctx_t *ctx,
                    uint8_t *mac,
                    uint32_t maclen )
{
    int ret = TE_SUCCESS;
    sca_cmac_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == mac)){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    prv_ctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);

    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }
    if (prv_ctx->mlen == 0) {
        ret = _te_cmac(ctx, mac, maclen);
        /*
         * This is to support finish() after finish(), that
         * is supposed to output cmac("").
         */
        if (te_sca_state(ctx->crypt) > TE_DRV_SCA_STATE_READY) {
            if (TE_SUCCESS == ret) {
                ret = te_sca_finish(ctx->crypt, NULL, 0);
            } else {
                te_sca_finish(ctx->crypt, NULL, 0);
            }
        }
        __CMAC_OUT__;
    }
    if(0 < prv_ctx->npdlen){
        ret = te_sca_update(ctx->crypt,
                            true,
                            prv_ctx->npdlen,
                            prv_ctx->npdata,
                            NULL);
        __CMAC_CHECK_CONDITION__(ret);
        prv_ctx->npdlen = 0;
    }

    ret = te_sca_finish(ctx->crypt, mac, maclen);
    if (TE_SUCCESS == ret) {
        prv_ctx->mlen = 0;
    }

__out__:
    return ret;
}

int te_cmac_reset( te_cmac_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    sca_cmac_ctx_t *prv_ctx = NULL;

    if(NULL == ctx){
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    prv_ctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);
    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_FORMAT;
        __CMAC_OUT__;
    }
    osal_memset(prv_ctx->iv, 0x00, sizeof(prv_ctx->iv));
    osal_memset(prv_ctx->npdata, 0x00, sizeof(prv_ctx->npdata));
    prv_ctx->npdlen = 0;
    prv_ctx->mlen = 0;

    /* we can well slip out if the calling ctx is in START state */
    switch (te_sca_state(ctx->crypt))
    {
        case TE_DRV_SCA_STATE_START:

            ret = TE_SUCCESS;
            __CMAC_OUT__;
            break;
        case TE_DRV_SCA_STATE_UPDATE:
        case TE_DRV_SCA_STATE_LAST:
            ret = te_sca_reset(ctx->crypt);
            __CMAC_CHECK_CONDITION__(ret);
            break;
        case TE_DRV_SCA_STATE_READY:
            break;
        default:
            ret = TE_ERROR_BAD_STATE;
            break;
    }

    ret = te_sca_start(ctx->crypt, TE_DRV_SCA_ENCRYPT, prv_ctx->iv,
                       ctx->crypt->blk_size);
__out__:
    return ret;
}

int te_cmac_clone( const te_cmac_ctx_t *src,
                   te_cmac_ctx_t *dst )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;
    sca_cmac_ctx_t *spctx = NULL;
    sca_cmac_ctx_t *dpctx = NULL;

    if (NULL == src || NULL == src->crypt || NULL == dst) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * free the dst ctx if it points to a valid cmac ctx already
     */
    if (dst->crypt != NULL) {
        ret = te_cmac_free(dst);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_cmac_free error %x\n", ret);
            return ret;
        }
    }

    spctx = (sca_cmac_ctx_t *)cmac_priv_ctx((te_cmac_ctx_t*)src);
    if(NULL == spctx){
        return TE_ERROR_BAD_PARAMS;
    }

    sdrv = (te_sca_drv_t *)src->crypt->drv;
    ret = te_sca_alloc_ctx(sdrv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_cmac_ctx_t),
                           &dst->crypt);
    if (ret != TE_SUCCESS) {
        return ret;
    }

    dpctx = (sca_cmac_ctx_t *)cmac_priv_ctx(dst);
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

int te_cmac_export( te_cmac_ctx_t *ctx,
                    void *out,
                    uint32_t *olen )
{
    int ret = TE_ERROR_GENERIC;
    sca_cmac_ctx_t *pctx = NULL;
    cmac_ehdr_t eh = {0};

    if ( NULL == ctx || NULL == ctx->crypt || NULL == olen ) {
        return TE_ERROR_BAD_PARAMS;
    }

    pctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);
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
    if (*olen < CMAC_EHDR_SIZE(&eh)) {
        *olen = CMAC_EHDR_SIZE(&eh);
        return TE_ERROR_SHORT_BUFFER;
    }

    if (NULL == out) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * export drvctx
     * TODO: lock the cmac driver to stop service of update() or uplist() on
     * the calling context until te_sca_export() ends.
     * Or, it's the caller responsibility to ensure there be no update() or
     * uplist() call on to the same context when an export() is outstanding.
     */
    ret = te_sca_export(ctx->crypt, CMAC_EHDR_DRVCTX(out), &eh.drvctx_sz);
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR( "te_sca_export error %x\n", ret );
        goto err;
    }

    /*
     * make ehdr
     */
    eh.magic  = CMAC_EHDR_MAGIC;
    eh.npdlen = pctx->npdlen;
    osal_memcpy(eh.npdata, pctx->npdata, sizeof(eh.npdata));
    eh.mlen = pctx->mlen;
    osal_memcpy(&eh.key, &pctx->key, sizeof(eh.key));
    osal_memcpy(out, &eh, sizeof(eh));
    *olen = CMAC_EHDR_SIZE(&eh);
err:
    return ret;
}

int te_cmac_import( te_cmac_ctx_t *ctx,
                    const void *in,
                    uint32_t ilen )
{
    int ret = TE_ERROR_GENERIC;
    sca_cmac_ctx_t *pctx = NULL;
    cmac_ehdr_t eh = {0};

    if ( NULL == ctx || NULL == ctx->crypt || NULL == in ) {
        return TE_ERROR_BAD_PARAMS;
    }

    pctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);
    if(NULL == pctx){
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * The 'in' might not start at struct ptr safe boundary.
     * Be safe to copy the struct before reading it.
     */
    osal_memcpy(&eh, in, sizeof(eh));

    if (eh.magic != CMAC_EHDR_MAGIC ||
        ilen < CMAC_EHDR_SIZE(&eh)) {
        OSAL_LOG_ERR("Bad or mismatched cmac ehdr: %d\n", ilen);
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * import drvctx
     */
    ret = te_sca_import(ctx->crypt, CMAC_EHDR_DRVCTX(in), eh.drvctx_sz );
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR("te_sca_import error %x\n", ret);
        return ret;
    }

    /*
     * import cmac ctx
     */
    osal_memcpy(pctx->npdata, eh.npdata, sizeof(pctx->npdata));
    pctx->npdlen = eh.npdlen;
    pctx->mlen = eh.mlen;
    osal_memcpy(&pctx->key, &eh.key, sizeof(pctx->key));

    return TE_SUCCESS;
}

#ifdef CFG_TE_ASYNC_EN
typedef struct acmac_ctx {
    te_cmac_ctx_t *ctx;
    te_cmac_request_t *req;
} acmac_ctx_t;

static void execute_cmac_start(te_worker_task_t *task)
{
    int ret = TE_SUCCESS;
    acmac_ctx_t *acmac = task->param;
    te_cmac_ctx_t *ctx = acmac->ctx;
    te_cmac_request_t *req = acmac->req;

    ret = te_cmac_start(ctx);

    osal_free(task);
    osal_free(acmac);

    req->res = ret;
    req->base.completion( &req->base, req->res );
    return;
}

int te_cmac_astart( te_cmac_ctx_t *ctx, te_cmac_request_t *req )
{
    int ret = TE_SUCCESS;
    acmac_ctx_t *acmac = NULL;
    te_worker_task_t *task = NULL;
    if(NULL == ctx
       || NULL == ctx->crypt
       || NULL == req ) {
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    acmac = osal_calloc(1, sizeof(acmac_ctx_t));
    if (NULL == acmac) {
        ret = TE_ERROR_OOM;
        __CMAC_OUT__;
    }

    task = osal_calloc(1, sizeof(te_worker_task_t));
    if (NULL == task) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    task->param = (void *)acmac;
    task->execute = execute_cmac_start;
    acmac->ctx = ctx;
    acmac->req = req;
    te_worker_pool_enqueue(task);

    return TE_SUCCESS;

err1:
    osal_free(acmac);
__out__:
    return ret;
}

static void execute_cmac_update(te_worker_task_t *task)
{
    int ret = TE_SUCCESS;
    acmac_ctx_t *acmac = task->param;
    te_cmac_ctx_t *ctx = acmac->ctx;
    te_cmac_request_t *req = acmac->req;

    ret = te_cmac_uplist(ctx, &req->up.in);

    osal_free(task);
    osal_free(acmac);

    req->res = ret;
    req->base.completion(&req->base, req->res);
    return;
}

int te_cmac_aupdate(te_cmac_ctx_t *ctx, te_cmac_request_t *req)
{
    int ret = TE_SUCCESS;
    acmac_ctx_t *acmac = NULL;
    te_worker_task_t *task = NULL;
    if(NULL == ctx
       || NULL == ctx->crypt
       || NULL == req) {
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    acmac = osal_calloc(1, sizeof(acmac_ctx_t));
    if (NULL == acmac) {
        ret = TE_ERROR_OOM;
        __CMAC_OUT__;
    }

    task = osal_calloc(1, sizeof(te_worker_task_t));
    if (NULL == task) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    task->param = (void *)acmac;
    task->execute = execute_cmac_update;
    acmac->ctx = ctx;
    acmac->req = req;
    te_worker_pool_enqueue(task);

    return TE_SUCCESS;

err1:
    osal_free(acmac);
__out__:
    return ret;
}

static void execute_cmac_finish(te_worker_task_t *task)
{
    int ret = TE_SUCCESS;
    acmac_ctx_t *acmac = task->param;
    te_cmac_ctx_t *ctx = acmac->ctx;
    te_cmac_request_t *req = acmac->req;

    ret = te_cmac_finish(ctx, req->fin.mac, req->fin.maclen);

    osal_free(task);
    osal_free(acmac);

    req->res = ret;
    req->base.completion(&req->base, req->res);
    return;
}

int te_cmac_afinish(te_cmac_ctx_t *ctx, te_cmac_request_t *req)
{
    int ret = TE_SUCCESS;
    acmac_ctx_t *acmac = NULL;
    te_worker_task_t *task = NULL;
    if(NULL == ctx
       || NULL == ctx->crypt
       || NULL == req
       || NULL == req->fin.mac
       || 0 == req->fin.maclen) {
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    acmac = osal_calloc(1, sizeof(acmac_ctx_t));
    if (NULL == acmac) {
        ret = TE_ERROR_OOM;
        __CMAC_OUT__;
    }

    task = osal_calloc(1, sizeof(te_worker_task_t));
    if (NULL == task) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    task->param = (void *)acmac;
    task->execute = execute_cmac_finish;
    acmac->ctx = ctx;
    acmac->req = req;
    te_worker_pool_enqueue(task);

    return TE_SUCCESS;

err1:
    osal_free(acmac);
__out__:
    return ret;
}

static void execute_cmac_mac(te_worker_task_t *task)
{
    int ret = TE_SUCCESS;
    acmac_ctx_t *acmac = task->param;
    te_cmac_ctx_t *ctx = acmac->ctx;
    te_cmac_request_t *req = acmac->req;

    ret = te_cmac_start(ctx);
    if (ret != TE_SUCCESS) {
        goto err;
    }
    ret = te_cmac_uplist(ctx, &req->amac.in);
    if (ret != TE_SUCCESS) {
        goto err;
    }

    ret = te_cmac_finish(ctx, req->amac.mac, req->amac.maclen);
    if (ret != TE_SUCCESS) {
        goto err;
    }

err:
    te_cmac_free(ctx);
    osal_free(ctx);
    osal_free(task);
    osal_free(acmac);

    req->res = ret;
    req->base.completion(&req->base, req->res);
    return;
}

/* malg = DES | TDES | AES | SM4 */
int te_acmac(te_drv_handle hdl,
             te_algo_t malg,
             te_cmac_request_t *req)
{
    int ret = TE_SUCCESS;
    te_cmac_ctx_t *ctx = NULL;
    acmac_ctx_t *acmac = NULL;
    te_worker_task_t *task = NULL;
    te_sca_key_t key_desc = { 0 };
    sca_cmac_ctx_t *prv_ctx = NULL;

    if((TE_MAIN_ALGO_SM4 != malg) && (TE_MAIN_ALGO_AES != malg)
       && (TE_MAIN_ALGO_DES != malg) && (TE_MAIN_ALGO_TDES != malg)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    if(NULL == req
       || NULL == req->amac.mac
       || ( req->amac.key.type != TE_KEY_TYPE_USER
       && req->amac.key.type != TE_KEY_TYPE_SEC)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CMAC_OUT__;
    }

    acmac = osal_calloc(1, sizeof(acmac_ctx_t));
    if (NULL == acmac) {
        ret = TE_ERROR_OOM;
        __CMAC_OUT__;
    }

    task = osal_calloc(1, sizeof(te_worker_task_t));
    if (NULL == task) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    ctx = (te_cmac_ctx_t *)osal_calloc(1, sizeof(te_cmac_ctx_t));
    if (NULL == ctx) {
        ret = TE_ERROR_OOM;
        goto err2;
    }

    ret = te_cmac_init(ctx, hdl, malg);
    if (ret != TE_SUCCESS) {
        goto err3;
    }

    key_desc.type = req->amac.key.type;
    if ( key_desc.type == TE_KEY_TYPE_USER ) {
        key_desc.user.key = req->amac.key.user.key;
        key_desc.user.keybits = req->amac.key.user.keybits;
        prv_ctx = (sca_cmac_ctx_t *)cmac_priv_ctx(ctx);
        prv_ctx->key.type = TE_KEY_TYPE_USER;
        osal_memcpy( prv_ctx->key.user.key,
                     req->amac.key.user.key,
                     key_desc.user.keybits / BYTE_BITS );
        prv_ctx->key.user.keybits = key_desc.user.keybits;
    } else {
        memcpy( &key_desc.sec,
                &req->amac.key.sec,
                sizeof(te_sec_key_t) );
    }

    ret = te_sca_setkey( ctx->crypt, &key_desc );
    if ( ret != TE_SUCCESS ) {
        goto err4;
    }

    task->param = (void *)acmac;
    task->execute = execute_cmac_mac;
    acmac->ctx = ctx;
    acmac->req = req;
    te_worker_pool_enqueue(task);

    return TE_SUCCESS;

err4:
    te_cmac_free(ctx);
err3:
    osal_free(ctx);
err2:
    osal_free(task);
err1:
    osal_free(acmac);
__out__:
    return ret;
}

#endif /* CFG_TE_ASYNC_EN */
