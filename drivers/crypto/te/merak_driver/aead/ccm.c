//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_ccm.h>
#include <te_cipher.h>
#include "../common/te_worker_pool.h"

#define CCM_JIV_LEN     (32U)

#define __CCM_OUT__    goto __out__

#define __CCM_CHECK_CONDITION__(_ret_)                                         \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 __CCM_OUT__;                                                  \
            }                                                                  \
        } while (0);

#define __CCM_CHECK_CONDITION_BREAK__(_ret_, _label_)                          \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 goto _label_;                                                 \
            }                                                                  \
        } while (0);

#define __CCM_ALLERT__(_ret_, _msg_)                                           \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X %s\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_),                                       \
                                (_msg_));                                      \
            }                                                                  \
        } while (0);

#define __CCM_VERIFY_PARAMS__(_param_)                                         \
        do                                                                     \
        {                                                                      \
            if(NULL == (_param_)){                                             \
                ret = TE_ERROR_BAD_PARAMS;                                     \
                __CCM_OUT__;                                                   \
            }                                                                  \
        } while (0)

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
 * SCA CCM private context
 */
typedef struct sca_ccm_ctx {
    te_cbcmac_ctx_t cmctx;              /**< cbc-mac sub-context */
    te_cipher_ctx_t cctx;               /**< ctr sub-context */
    te_sca_operation_t op;              /**< operation mode */
    te_ccm_state_t state;               /**< ccm state */
    uint64_t aadlen;                    /**< updated associated data length in byte */
    uint64_t expected_aadlen;           /**< expected associated data length in byte */
    size_t taglen;                      /**< tag length in byte */
    uint64_t mlen;                      /**< message length in byte */
    uint64_t expected_mlen;             /**< expected message length in byte */
    uint32_t nlen;                      /**< nonce length in byte */
    uint8_t stream[SCA_MAX_BLOCK_SIZE]; /**< stream block */
    size_t strpos;                      /**< stream block offset */
    union {
        struct {
            uint8_t ctr[16];            /**< counter  */
            uint8_t mac[16];            /**< intermediate cbc mac */
        };
        uint8_t jiv[32];                /**< joint iv: ctr || mac */
    };
    uint8_t b0[TE_MAX_SCA_BLOCK];       /**< buffer holding block#0 */
    uint8_t a0[TE_MAX_SCA_BLOCK];       /**< intial counter a0*/
    uint8_t aad0[TE_MAX_SCA_BLOCK];     /**< buffer holding aad block#0 */
    uint8_t padding[TE_MAX_SCA_BLOCK];  /**< buffer holding  padding*/
} sca_ccm_ctx_t;

/**
 * malg = AES | SM4
 */
int te_ccm_init( te_ccm_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg )
{
    int ret = TE_SUCCESS;
    sca_ccm_ctx_t *prv_ctx = NULL;
    te_sca_drv_t *sdrv = NULL;

    if((NULL == ctx)
        || ((TE_MAIN_ALGO_AES != malg) && (TE_MAIN_ALGO_SM4 != malg))){
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }

    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    ret = te_sca_alloc_ctx(sdrv, malg, sizeof(sca_ccm_ctx_t), &ctx->crypt);
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
    __CCM_CHECK_CONDITION__(ret);
    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    if(NULL == prv_ctx){
        ret = TE_ERROR_BAD_PARAMS;
        goto __err1__;
    }

    ret = te_cbcmac_init(&prv_ctx->cmctx, hdl, malg);
    if(TE_SUCCESS != ret){
        goto __err1__;
    }

    ret = te_cipher_init(&prv_ctx->cctx, hdl, malg);
    if(TE_SUCCESS == ret){
        if( TE_MAIN_ALGO_SM4 == malg){
            ctx->crypt->alg = TE_ALG_SM4_CCM;
            prv_ctx->cctx.crypt->alg = TE_ALG_SM4_CTR;
        }else{
            ctx->crypt->alg = TE_ALG_AES_CCM;
            prv_ctx->cctx.crypt->alg = TE_ALG_AES_CTR;
        }
        prv_ctx->state = TE_CCM_STATE_INIT;
        __CCM_OUT__;
    }

    te_cbcmac_free(&prv_ctx->cmctx);
__err1__:
    te_sca_free_ctx(ctx->crypt);
    ctx->crypt = NULL;
__out__:
    return ret;
}

int te_ccm_free( te_ccm_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    sca_ccm_ctx_t *prv_ctx = NULL;
    __te_unused uint8_t tmp[SCA_MAX_BLOCK_SIZE] = {0};

    __CCM_VERIFY_PARAMS__(ctx);
    __CCM_VERIFY_PARAMS__(ctx->crypt);
    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        __CCM_OUT__;
    }

    switch (prv_ctx->state){
    default:
        ret = TE_ERROR_BAD_STATE;
        __CCM_OUT__;
    case TE_CCM_STATE_RAW:
        __CCM_OUT__;
        break;
    case TE_CCM_STATE_READY:
    case TE_CCM_STATE_INIT:
        break;
    case TE_CCM_STATE_START:
    case TE_CCM_STATE_UPDATE:
        ret = te_sca_finish(ctx->crypt, tmp, prv_ctx->taglen);
        __CCM_ALLERT__(ret, "te_sca_finish raises exceptions!");
        break;
    }
    ret = te_cbcmac_free(&prv_ctx->cmctx);
    __CCM_ALLERT__(ret, "te_cbcmac_free raises exceptions!");
    ret = te_cipher_free(&prv_ctx->cctx);
    __CCM_ALLERT__(ret, "te_cipher_free raises exceptions!");

    /* state change must before free, otherwise memory overwritten */
    prv_ctx->state = TE_CCM_STATE_RAW;
    ret = te_sca_free_ctx(ctx->crypt);
    __CCM_CHECK_CONDITION__(ret);
    ctx->crypt = NULL;
__out__:
    return ret;
}

int te_ccm_setkey( te_ccm_ctx_t *ctx,
                   const uint8_t *key,
                   uint32_t keybits )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};
    sca_ccm_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == ctx->crypt) || (NULL == key)){
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }
    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    switch (prv_ctx->state){
    case TE_CCM_STATE_INIT:
    case TE_CCM_STATE_READY:
        break;
    default:
        ret = TE_ERROR_BAD_STATE;
        __CCM_OUT__;
    }
    key_desc.type = TE_KEY_TYPE_USER;
    key_desc.user.key = (uint8_t *)key;
    key_desc.user.keybits = keybits;
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    __CCM_CHECK_CONDITION__(ret);
    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    __CCM_VERIFY_PARAMS__(prv_ctx);
    ret = te_cbcmac_setkey(&prv_ctx->cmctx, key, keybits);
    __CCM_CHECK_CONDITION__(ret);
    ret = te_cipher_setkey(&prv_ctx->cctx, key, keybits);
    __CCM_CHECK_CONDITION__(ret);
    prv_ctx->state = TE_CCM_STATE_READY;
__out__:
    return ret;
}

/**
 * AES and SM4 only
 */
int te_ccm_setseckey( te_ccm_ctx_t *ctx,
                      te_sec_key_t *key )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};
    sca_ccm_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == ctx->crypt) || (NULL == key)){
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }

    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    switch (prv_ctx->state){
    case TE_CCM_STATE_INIT:
    case TE_CCM_STATE_READY:
        break;
    default:
        ret = TE_ERROR_BAD_STATE;
        __CCM_OUT__;
    }
    key_desc.type = TE_KEY_TYPE_SEC;
    osal_memcpy(&key_desc.sec, key, sizeof(te_sec_key_t));
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    __CCM_CHECK_CONDITION__(ret);
    ret = te_cbcmac_setseckey(&prv_ctx->cmctx, key);
    __CCM_CHECK_CONDITION__(ret);
    ret = te_cipher_setseckey(&prv_ctx->cctx, key);
    prv_ctx->state = TE_CCM_STATE_READY;
__out__:
    return ret;
}

/**
 * AES only
 */
int te_ccm_setseckey_v2( te_ccm_ctx_t *ctx,
                         te_sec_key_v2_t *key )
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};
    sca_ccm_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == ctx->crypt) || (NULL == key) ||
       (TE_ALG_AES_CCM != ctx->crypt->alg)){
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }

    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    switch (prv_ctx->state){
    case TE_CCM_STATE_INIT:
    case TE_CCM_STATE_READY:
        break;
    default:
        ret = TE_ERROR_BAD_STATE;
        __CCM_OUT__;
    }
    key_desc.type = TE_KEY_TYPE_SEC_V2;
    osal_memcpy(&key_desc.sec_v2, key, sizeof(te_sec_key_v2_t));
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    __CCM_CHECK_CONDITION__(ret);
    ret = te_cbcmac_setseckey_v2(&prv_ctx->cmctx, key);
    __CCM_CHECK_CONDITION__(ret);
    ret = te_cipher_setseckey_v2(&prv_ctx->cctx, key);
    prv_ctx->state = TE_CCM_STATE_READY;
__out__:
    return ret;
}

int te_ccm_clone( const te_ccm_ctx_t *src,
                  te_ccm_ctx_t *dst )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;
    sca_ccm_ctx_t *spctx = NULL;
    sca_ccm_ctx_t *dpctx = NULL;

    if (NULL == src || NULL == src->crypt || NULL == dst) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * free the dst ctx if it points to a valid ccm ctx already
     */
    if (dst->crypt != NULL) {
        ret = te_ccm_free(dst);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_ccm_free error %x\n", ret);
            return ret;
        }
    }

    spctx = (sca_ccm_ctx_t *)ccm_priv_ctx((te_ccm_ctx_t*)src);
    if(NULL == spctx){
        return TE_ERROR_BAD_PARAMS;
    }

    sdrv = (te_sca_drv_t *)src->crypt->drv;
    ret = te_sca_alloc_ctx(sdrv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_ccm_ctx_t),
                           &dst->crypt);
    if (ret != TE_SUCCESS) {
        return ret;
    }

    dpctx = (sca_ccm_ctx_t *)ccm_priv_ctx(dst);
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
    osal_memset(&dpctx->cmctx, 0, sizeof(dpctx->cmctx));
    osal_memset(&dpctx->cctx, 0, sizeof(dpctx->cctx));

    /*
     * clone sub-contexts
     */
    ret = te_cbcmac_clone(&spctx->cmctx, &dpctx->cmctx);
    if (ret != TE_SUCCESS) {
        te_ccm_free(dst);
        return ret;
    }
    ret = te_cipher_clone(&spctx->cctx, &dpctx->cctx);
    if (ret != TE_SUCCESS) {
        te_ccm_free(dst);
        return ret;
    }

    return TE_SUCCESS;
}

/**
 * NIST Special Publication 800-38C
 * Appendix A: Example of a Formatting and Counter Generation Function
 **/
static int _te_ccm_sanity_check_params( uint64_t len,
                                        uint32_t nlen,
                                        uint64_t aadlen,
                                        size_t tag_len){
    int ret = TE_SUCCESS;
    uint8_t q = 0;
    uint8_t _max_a = 8;

    /*check tag_len*/
    if((4 > tag_len) || (16 < tag_len) || (tag_len & 0x01)){
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }
    /*check length of nonce*/
    if((7 > nlen) || (13 < nlen)){
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }
    /*check len n + q = 15*/
    q = 15 - nlen;
    while (q--){
        len >>= 8;
    }
    if(0 < len){
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        __CCM_OUT__;
    }
    /*check addlen a < 2^64*/
    while (_max_a--){
        aadlen >>= 8;
    }
    if(0 < aadlen){
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }
__out__:
    return ret;
}

static void _te_ccm_read_size(uint8_t *dst, size_t ofs,  uint64_t size)
{
    size_t i = 0;

    for (i = 0; i < sizeof(size); i++, size >>= 8) {
        if (0 == size){
            break;
        }
        dst[ofs- i] = size & 0xFF;
    }
}

static inline void _te_ccm_gen_b0( uint8_t *b0,
                                   uint64_t len,
                                   uint8_t *nonce,
                                   uint32_t nlen,
                                   uint64_t aadlen,
                                   size_t tag_len )
{
    b0[0] = 0;
    if(0 < aadlen){
        b0[0] |= 1 << 6;
    }

    b0[0] |= ((tag_len - 2)/2) << 3;
    b0[0] |= (15 - nlen - 1);

    osal_memcpy(b0 + 1, nonce, nlen);
    _te_ccm_read_size(b0, 15, len);
    return;
}

static inline void _te_ccm_gen_a0( uint8_t *a0,
                                   uint8_t *nonce,
                                   size_t nlen )
{
    a0[0] = 0;
    a0[0] |= (15 - nlen - 1);
    osal_memcpy(a0 + 1, nonce, nlen);
    osal_memset(a0 + 1 + nlen, 0x00, 16 - 1 - nlen);
}

/**
 *  \brief              generate assosiated data block#0 base on specified
 *                      input params \p aad \p aadlen \p blk_size
 *  \param[in] aadlen   length of assosiated data
 *
 *  \return             \c  0 success, others failed.
 */
static inline int _te_ccm_update_aadlen( sca_ccm_ctx_t *ctx, uint64_t aadlen )
{
#define LEN_CASE1       (0x10000U - 0x100U)
#define LEN_CASE2       (0xFFFFFFFFU)

    int ret = TE_SUCCESS;
    size_t _offset = 0;

    if(LEN_CASE1 > aadlen){
    /*0 < aadlen < 2^16 - 2^8*/
        ctx->aad0[_offset++] = (aadlen >> 8) & 0xFF;
        ctx->aad0[_offset++] = aadlen & 0xFF;
    }else if(LEN_CASE2 > aadlen){
    /*2^16 - 2^8 =< aadlen < 2^32*/
        ctx->aad0[_offset++] = 0xFF;
        ctx->aad0[_offset++] = 0xFe;
        ctx->aad0[_offset++] = (aadlen >> 24) & 0xFF;
        ctx->aad0[_offset++] = (aadlen >> 16) & 0xFF;
        ctx->aad0[_offset++] = (aadlen >> 8) & 0xFF;
        ctx->aad0[_offset++] = aadlen & 0xFF;
    }else{
    /*2^32 =< aadlen < 2^64*/
        ctx->aad0[_offset++] = 0xFF;
        ctx->aad0[_offset++] = 0xFF;
        ctx->aad0[_offset++] = (aadlen >> 56) & 0xFF;
        ctx->aad0[_offset++] = (aadlen >> 48) & 0xFF;
        ctx->aad0[_offset++] = (aadlen >> 40) & 0xFF;
        ctx->aad0[_offset++] = (aadlen >> 32) & 0xFF;
        ctx->aad0[_offset++] = (aadlen >> 24) & 0xFF;
        ctx->aad0[_offset++] = (aadlen >> 16) & 0xFF;
        ctx->aad0[_offset++] = (aadlen >> 8) & 0xFF;
        ctx->aad0[_offset++] = aadlen & 0xFF;
    }
    ret = te_cbcmac_update(&ctx->cmctx, _offset, ctx->aad0);
    return ret;
}

int te_ccm_start( te_ccm_ctx_t *ctx,
                  te_sca_operation_t op,
                  const uint8_t *nonce,
                  uint32_t nonce_len,
                  uint32_t tag_len,
                  uint64_t aad_len,
                  uint64_t payload_len )
{
    int ret = TE_SUCCESS;
    sca_ccm_ctx_t *prv_ctx = NULL;
    uint8_t _iv[TE_MAX_SCA_BLOCK] = {0};
    __te_unused uint8_t gabage[TE_MAX_SCA_BLOCK] = {0};

    if ((NULL == ctx)
        || (NULL == nonce)
        || (NULL == ctx->crypt)
        || ((TE_DRV_SCA_ENCRYPT != op) && (TE_DRV_SCA_DECRYPT != op))) {
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }
    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    switch (prv_ctx->state) {
        case TE_CCM_STATE_READY:
            break;
        default:
            ret = TE_ERROR_BAD_STATE;
            __CCM_OUT__;
    }
    ret = _te_ccm_sanity_check_params(payload_len, nonce_len, aad_len, tag_len);
    __CCM_CHECK_CONDITION__(ret);
    osal_memset(prv_ctx->b0, 0x00, sizeof(prv_ctx->b0));
    osal_memset(prv_ctx->a0, 0x00, sizeof(prv_ctx->a0));
    osal_memset(prv_ctx->aad0, 0x00, sizeof(prv_ctx->aad0));
    osal_memset(prv_ctx->padding, 0x00, sizeof(prv_ctx->padding));
    /* format first block b0 then set it to the first node of list */
    _te_ccm_gen_b0(prv_ctx->b0, payload_len,
                   (uint8_t *)nonce, nonce_len, aad_len, tag_len);
    /*cbcmac calcualte b0||l(A)||A||O^k*/
    ret = te_cbcmac_start(&prv_ctx->cmctx, _iv);
    __CCM_CHECK_CONDITION__(ret);
    ret = te_cbcmac_update(&prv_ctx->cmctx, prv_ctx->cmctx.crypt->blk_size,
                           prv_ctx->b0);
    if (ret != TE_SUCCESS) {
        te_cbcmac_finish(&prv_ctx->cmctx, gabage,
                               prv_ctx->cmctx.crypt->blk_size);
    }
    if (aad_len > 0) {
        ret = _te_ccm_update_aadlen(prv_ctx, aad_len);
        __CCM_CHECK_CONDITION__(ret);
    }
    _te_ccm_gen_a0(prv_ctx->a0, (uint8_t *)nonce, nonce_len);
    osal_memcpy(prv_ctx->ctr, prv_ctx->a0, prv_ctx->cctx.crypt->blk_size);
    COUNTER_INCREASE(prv_ctx->ctr, prv_ctx->cctx.crypt->blk_size, 1);
    /**< mark claimed aadlen/tag len/message len */
    prv_ctx->expected_aadlen = aad_len;
    prv_ctx->aadlen = 0;
    prv_ctx->taglen = tag_len;
    prv_ctx->expected_mlen = payload_len;
    prv_ctx->mlen = 0;
    prv_ctx->op = op;
    prv_ctx->state = TE_CCM_STATE_START;
__out__:
    return ret;
}

int te_ccm_update_aad(te_ccm_ctx_t *ctx, const uint8_t *data, size_t len)
{
    int ret = TE_SUCCESS;
    sca_ccm_ctx_t *prv_ctx = NULL;
    if ((NULL == ctx)
        || (NULL == data && len)
        || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }

    if (!len) {
        return TE_SUCCESS;
    }

    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    if ((prv_ctx->aadlen + len) > prv_ctx->expected_aadlen
        || prv_ctx->aadlen > len + prv_ctx->aadlen) {
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        __CCM_OUT__;
    }
    switch (prv_ctx->state) {
        case TE_CCM_STATE_START:
            break;
        default:
            ret = TE_ERROR_BAD_STATE;
            __CCM_OUT__;
    }
    ret = te_cbcmac_update(&prv_ctx->cmctx, len, data);
    /**< here use mlen to mark aadlen when updata data don't forget to reset it */
    __CCM_CHECK_CONDITION__(ret);
    prv_ctx->aadlen += len;
__out__:
    return ret;
}

int te_ccm_uplist_aad(te_ccm_ctx_t *ctx, te_memlist_t *data)
{
    int ret = TE_SUCCESS;
    sca_ccm_ctx_t *prv_ctx = NULL;
    size_t len = 0;
    if ((NULL == ctx)
        || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }
    if (data && data->nent && !data->ents) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __CCM_OUT__;
    }
    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    if( data && data->nent) {
        len = te_memlist_get_total_len(data);
        if (prv_ctx->aadlen + len > prv_ctx->expected_aadlen
            || prv_ctx->aadlen > len + prv_ctx->aadlen) {
            OSAL_LOG_ERR("Aad length overflow!!!!\n");
            ret = TE_ERROR_BAD_INPUT_LENGTH;
            __CCM_OUT__;
        }
    }
    switch (prv_ctx->state) {
        case TE_CCM_STATE_START:
            break;
        default:
            ret = TE_ERROR_BAD_STATE;
            __CCM_OUT__;
    }
    if (!data || (data->nent == 0) || (len == 0)) {
        ret = TE_SUCCESS;
        __CCM_OUT__;
    }
    ret = te_cbcmac_uplist(&prv_ctx->cmctx, data);
    __CCM_CHECK_CONDITION__(ret);
    prv_ctx->aadlen += len;

__out__:
    return ret;
}

static int _te_ccm_finish_aad(sca_ccm_ctx_t *ctx)
{
    int ret = TE_SUCCESS;
    size_t blk_size = 0;
    size_t size_aadlen = 0;
    size_t padding_sz = 0;

    if (ctx->aadlen != ctx->expected_aadlen) {
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        __CCM_OUT__;
    }

    if (ctx->aadlen)
    {
        if(LEN_CASE1 > ctx->aadlen){
        /*0 < aadlen < 2^16 - 2^8*/
            size_aadlen = 2;
        }else if(LEN_CASE2 > ctx->aadlen){
        /*2^16 - 2^8 =< aadlen < 2^32*/
            size_aadlen = 6;
        }else{
        /*2^32 =< aadlen < 2^64*/
            size_aadlen = 10;
        }

        TE_ASSERT(NULL != ctx->cmctx.crypt);
        blk_size = ctx->cmctx.crypt->blk_size;
        padding_sz = blk_size - ((ctx->aadlen + size_aadlen) % blk_size);
        /**< when ctx->aadlen + size_aadlen is complete multiple blocks size
         *   no need to padd */
        if (padding_sz == blk_size) {
            padding_sz = 0;
        }
        if(0 < padding_sz){
            osal_memset(ctx->padding, 0x00, padding_sz);
            ret = te_cbcmac_update(&ctx->cmctx, padding_sz, ctx->padding);
            if (TE_SUCCESS != ret) {
                __CCM_OUT__;
            }
        }
    }
    ctx->state = TE_CCM_STATE_UPDATE;
__out__:
    return ret;
}

int te_ccm_update(te_ccm_ctx_t *ctx, size_t len, const uint8_t *in, uint8_t *out)
{
    int ret = TE_SUCCESS;
    sca_ccm_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx)
        || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }

    if (len && ((NULL == in) || (NULL == out))) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __CCM_OUT__;
    }

    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    switch (prv_ctx->state){
    default:
    case TE_CCM_STATE_RAW:
    case TE_CCM_STATE_INIT:
    case TE_CCM_STATE_READY:
        ret = TE_ERROR_BAD_STATE;
        __CCM_OUT__;
    case TE_CCM_STATE_START:
        ret = _te_ccm_finish_aad(prv_ctx);
        __CCM_CHECK_CONDITION__(ret);
        prv_ctx->strpos = 0;
        break;
    case TE_CCM_STATE_UPDATE:
        break;
    }

    if (len == 0) {
        return TE_SUCCESS;
    }
    if ((prv_ctx->mlen + len) > prv_ctx->expected_mlen
        || prv_ctx->mlen > (len + prv_ctx->mlen)) {
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        __CCM_OUT__;
    }
    if (TE_DRV_SCA_ENCRYPT == prv_ctx->op) {
        ret = te_cbcmac_update(&prv_ctx->cmctx, len, in);
        __CCM_CHECK_CONDITION__(ret);
    }
    ret = te_cipher_ctr(&prv_ctx->cctx, len, &prv_ctx->strpos, prv_ctx->ctr,
                        prv_ctx->stream, in, out);
    __CCM_CHECK_CONDITION__(ret);
    if (TE_DRV_SCA_DECRYPT == prv_ctx->op) {
        ret = te_cbcmac_update(&prv_ctx->cmctx, len, out);
        __CCM_CHECK_CONDITION__(ret);
    }
    prv_ctx->state = TE_CCM_STATE_UPDATE;
    prv_ctx->mlen += len;
__out__:
    return ret;
}

int te_ccm_uplist(te_ccm_ctx_t *ctx, te_memlist_t *in, te_memlist_t *out)
{
    int ret = TE_SUCCESS;
    sca_ccm_ctx_t *prv_ctx = NULL;
    size_t len = 0;

    if ((NULL == ctx)
        || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }

    if (in && in->nent && (!in->ents || !out || !out->nent
        || (out->nent && !out->ents))) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __CCM_OUT__;
    }

    if(in) {
        len = te_memlist_get_total_len(in);
    }

    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    switch (prv_ctx->state){
    default:
    case TE_CCM_STATE_RAW:
    case TE_CCM_STATE_INIT:
    case TE_CCM_STATE_READY:
        ret = TE_ERROR_BAD_STATE;
        __CCM_OUT__;
    case TE_CCM_STATE_START:
        ret = _te_ccm_finish_aad(prv_ctx);
        __CCM_CHECK_CONDITION__(ret);
        prv_ctx->strpos = 0;
        break;
    case TE_CCM_STATE_UPDATE:
        break;
    }

    if (!in || !in->nent || (len == 0)) {
        ret = TE_SUCCESS;
        __CCM_OUT__;
    }

    if (((prv_ctx->mlen + len) > prv_ctx->expected_mlen)
        || (prv_ctx->mlen > (len + prv_ctx->mlen))) {
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        __CCM_OUT__;
    }
    if (TE_DRV_SCA_ENCRYPT == prv_ctx->op) {
        ret = te_cbcmac_uplist(&prv_ctx->cmctx, in);
        __CCM_CHECK_CONDITION__(ret);
    }
    ret = te_cipher_ctr_list(&prv_ctx->cctx, &prv_ctx->strpos, prv_ctx->ctr,
                        prv_ctx->stream, in, out);
    __CCM_CHECK_CONDITION__(ret);
    if (TE_DRV_SCA_DECRYPT == prv_ctx->op) {
        ret = te_cbcmac_uplist(&prv_ctx->cmctx, out);
        __CCM_CHECK_CONDITION__(ret);
    }
    prv_ctx->state = TE_CCM_STATE_UPDATE;
    prv_ctx->mlen += len;
__out__:
    return ret;
}

int te_ccm_finish(te_ccm_ctx_t *ctx, uint8_t *tag, uint32_t tag_len)
{
    int ret = TE_SUCCESS;
    sca_ccm_ctx_t *prv_ctx = NULL;
    uint8_t *tmp = NULL;
    size_t padding_sz = 0;

    if ((NULL == ctx) || (NULL == ctx->crypt) || (NULL == tag)) {
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }
    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    /*check tag_len &  message len*/
    if (tag_len != prv_ctx->taglen
        || prv_ctx->mlen != prv_ctx->expected_mlen) {
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        __CCM_OUT__;
    }
    switch (prv_ctx->state){
    default:
    case TE_CCM_STATE_RAW:
    case TE_CCM_STATE_READY:
    case TE_CCM_STATE_START:
        ret = TE_ERROR_BAD_STATE;
        __CCM_OUT__;
    case TE_CCM_STATE_UPDATE:
        break;
    }
    /**< if not alligned pad to complete block size call cbcmac update */
    if (prv_ctx->mlen % prv_ctx->cmctx.crypt->blk_size) {
        padding_sz = prv_ctx->cmctx.crypt->blk_size -
                     (prv_ctx->mlen % prv_ctx->cmctx.crypt->blk_size);
        osal_memset(prv_ctx->padding, 0x00, padding_sz);
        ret = te_cbcmac_update(&prv_ctx->cmctx, padding_sz, prv_ctx->padding);
        __CCM_CHECK_CONDITION__(ret);
    }
    ret = te_cbcmac_finish(&prv_ctx->cmctx, prv_ctx->mac,
                            prv_ctx->cmctx.crypt->blk_size);
    __CCM_CHECK_CONDITION__(ret);
    tmp = osal_malloc_aligned(UTILS_ROUND_UP(prv_ctx->cctx.crypt->blk_size,
                              TE_DMA_ALIGNED), TE_DMA_ALIGNED);
    if (NULL == tmp) {
        ret = TE_ERROR_OOM;
        __CCM_OUT__;
    }
    prv_ctx->strpos = 0;
    osal_memcpy(prv_ctx->ctr, prv_ctx->a0, prv_ctx->cctx.crypt->blk_size);
    ret = te_cipher_ctr(&prv_ctx->cctx,
                        prv_ctx->cctx.crypt->blk_size,
                        &prv_ctx->strpos,
                        prv_ctx->ctr,
                        prv_ctx->stream,
                        prv_ctx->mac,
                        tmp);
    if (TE_SUCCESS != ret) {
        __CCM_OUT__;
    }

    if(TE_DRV_SCA_ENCRYPT == prv_ctx->op){
        osal_memcpy(tag, tmp, tag_len);
    } else {
        if(0 != osal_memcmp(tag, tmp, tag_len)){
            osal_memcpy(tag, tmp, tag_len);
            ret = TE_ERROR_SECURITY;
        }
    }
    prv_ctx->state = TE_CCM_STATE_READY;
__out__:
    if (NULL != tmp) {
        osal_free(tmp);
    }
    return ret;
}

int te_ccm_crypt( te_ccm_ctx_t *ctx,
                  te_sca_operation_t op,
                  size_t len,
                  uint8_t *nonce,
                  uint32_t nlen,
                  const uint8_t *aad,
                  uint64_t aadlen,
                  const uint8_t *in,
                  uint8_t *out,
                  uint8_t *tag,
                  uint32_t taglen)
{
    int ret = TE_SUCCESS;
    uint8_t stream[SCA_MAX_BLOCK_SIZE] = {0};
    uint8_t *tmp = NULL;
    sca_ccm_ctx_t *prv_ctx = NULL;
    size_t nc_off = 0;
    __te_unused uint8_t gabage[SCA_MAX_BLOCK_SIZE] = {0};

    if((NULL == ctx)
        || (NULL == nonce)
        || (NULL == ctx->crypt)
        || (NULL == tag)
        || ((TE_DRV_SCA_ENCRYPT != op) && (TE_DRV_SCA_DECRYPT != op))){
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }
    if (((aadlen > 0) && !aad) || ((len > 0) && (!in || !out))) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __CCM_OUT__;
    }
    prv_ctx = ccm_priv_ctx(ctx);
    ret = te_ccm_start(ctx, op, nonce, nlen, taglen, aadlen, len);
    __CCM_CHECK_CONDITION__(ret);
    ret = te_ccm_update_aad(ctx, aad, aadlen);
    if (TE_SUCCESS != ret) {
        te_cbcmac_finish(&prv_ctx->cmctx, gabage, ctx->crypt->blk_size);
        __CCM_OUT__;
    }
    ret = _te_ccm_finish_aad(prv_ctx);
    if (TE_SUCCESS != ret) {
        te_cbcmac_finish(&prv_ctx->cmctx, gabage, ctx->crypt->blk_size);
        __CCM_OUT__;
    }
    ret = te_cbcmac_finish(&prv_ctx->cmctx,
                            prv_ctx->mac,
                            prv_ctx->cmctx.crypt->blk_size);
    __CCM_CHECK_CONDITION__(ret);
    /*ccm main flow*/
    ret = te_sca_start(ctx->crypt, op, prv_ctx->jiv, CCM_JIV_LEN);
    __CCM_CHECK_CONDITION__(ret);
    ret = te_sca_update(ctx->crypt, true, len, in, out);
    if (TE_SUCCESS != ret) {
        goto cleanup;
    }
    ret = te_sca_finish(ctx->crypt, prv_ctx->mac, ctx->crypt->blk_size);
    __CCM_CHECK_CONDITION__(ret);

    tmp = osal_malloc_aligned(UTILS_ROUND_UP(prv_ctx->cctx.crypt->blk_size,
                              TE_DMA_ALIGNED), TE_DMA_ALIGNED);
    if (NULL == tmp) {
        ret = TE_ERROR_OOM;
        __CCM_OUT__;
    }

    osal_memcpy(prv_ctx->ctr, prv_ctx->a0, prv_ctx->cctx.crypt->blk_size);
    ret = te_cipher_ctr(&prv_ctx->cctx,
                        prv_ctx->cctx.crypt->blk_size,
                        &nc_off,
                        prv_ctx->ctr,
                        stream,
                        prv_ctx->mac,
                        tmp);
    if (TE_SUCCESS != ret) {
        osal_free(tmp);
        tmp = NULL;
        __CCM_OUT__;
    }

    if(TE_DRV_SCA_ENCRYPT == op){
        osal_memcpy(tag, tmp, taglen);
    }else{
        if(0 != osal_memcmp(tag, tmp, taglen)){
            ret = TE_ERROR_SECURITY;
            osal_memset(out, 0x00, len);
        }
    }
    prv_ctx->state = TE_CCM_STATE_READY;
    osal_free(tmp);
    tmp = NULL;
    __CCM_OUT__;
cleanup:
    te_sca_finish(ctx->crypt, NULL, 0);
__out__:
    return ret;
}

int te_ccm_crypt_list( te_ccm_ctx_t *ctx,
                       te_sca_operation_t op,
                       uint8_t *nonce,
                       uint32_t nlen,
                       te_memlist_t *aad,
                       te_memlist_t *in,
                       te_memlist_t *out,
                       uint8_t *tag,
                       uint32_t taglen )
{
    int ret = TE_SUCCESS;
    uint8_t stream[SCA_MAX_BLOCK_SIZE] = {0};
    sca_ccm_ctx_t *prv_ctx = NULL;
    size_t len = 0;
    size_t nc_off = 0;
    uint8_t *tmp = NULL;
    size_t i = 0;
    __te_unused uint8_t gabage[SCA_MAX_BLOCK_SIZE] = {0};

    if((NULL == ctx)
        || (NULL == nonce)
        || (NULL == ctx->crypt)
        || (NULL == tag)
        || ((TE_DRV_SCA_ENCRYPT != op) && (TE_DRV_SCA_DECRYPT != op))){
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }

    if ((aad && aad->nent && !aad->ents)
        || (in && in->nent && (!in->ents || !out
                                || !out->nent || !out->ents))) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __CCM_OUT__;
    }
    len = te_memlist_get_total_len(in);
    if (len > te_memlist_get_total_len(out)) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        __CCM_OUT__;
    }

    prv_ctx = (sca_ccm_ctx_t *)ccm_priv_ctx(ctx);
    TE_ASSERT(NULL != prv_ctx);
    ret = te_ccm_start(ctx, op, nonce, nlen, taglen,
                       te_memlist_get_total_len(aad),
                       te_memlist_get_total_len(in));
    __CCM_CHECK_CONDITION__(ret);
    ret = te_ccm_uplist_aad(ctx, aad);
    if (TE_SUCCESS != ret) {
        te_cbcmac_finish(&prv_ctx->cmctx, gabage, ctx->crypt->blk_size);
        __CCM_OUT__;
    }
    ret = _te_ccm_finish_aad(prv_ctx);
    if (TE_SUCCESS != ret) {
        te_cbcmac_finish(&prv_ctx->cmctx, gabage, ctx->crypt->blk_size);
        __CCM_OUT__;
    }
    ret = te_cbcmac_finish(&prv_ctx->cmctx,
                            prv_ctx->mac,
                            prv_ctx->cmctx.crypt->blk_size);
    __CCM_CHECK_CONDITION__(ret);
    /*if has payload then process ccm main flow*/
    if (0 < len) {
        ret = te_sca_start(ctx->crypt, op, prv_ctx->jiv, CCM_JIV_LEN);
        __CCM_CHECK_CONDITION__(ret);
        ret = te_sca_uplist(ctx->crypt, true, in, out);
        if (TE_SUCCESS != ret) {
            goto cleanup;
        }
        ret = te_sca_finish(ctx->crypt, prv_ctx->mac, ctx->crypt->blk_size);
        __CCM_CHECK_CONDITION__(ret);
    }
    tmp = osal_malloc_aligned(UTILS_ROUND_UP(prv_ctx->cctx.crypt->blk_size,
                              TE_DMA_ALIGNED), TE_DMA_ALIGNED);
    if (NULL == tmp) {
        ret = TE_ERROR_OOM;
        __CCM_OUT__;
    }

    osal_memcpy(prv_ctx->ctr, prv_ctx->a0, prv_ctx->cctx.crypt->blk_size);
    ret = te_cipher_ctr(&prv_ctx->cctx,
                        prv_ctx->cctx.crypt->blk_size,
                        &nc_off,
                        prv_ctx->ctr,
                        stream,
                        prv_ctx->mac,
                        tmp);
    __CCM_CHECK_CONDITION__(ret);
    if(TE_DRV_SCA_ENCRYPT == op){
        osal_memcpy(tag, tmp, taglen);
    }else{
        if(0 != osal_memcmp(tag, tmp, taglen)){
            ret = TE_ERROR_SECURITY;
            for (i = 0; i < out->nent; i++) {
                osal_memset(out->ents[i].buf, 0x00, out->ents[i].len);
            }
        }
    }
    prv_ctx->state = TE_CCM_STATE_READY;
    __CCM_OUT__;
cleanup:
    te_sca_finish(ctx->crypt, NULL, 0);
__out__:
    if (NULL != tmp) {
        osal_free(tmp);
        tmp = NULL;
    }
    return ret;
}

#ifdef CFG_TE_ASYNC_EN
typedef struct accm_ctx {
    te_ccm_ctx_t *ctx;
    te_ccm_request_t *req;
} accm_ctx_t;

static void execute_ccm_crypt(te_worker_task_t *task)
{
    int ret = TE_SUCCESS;
    accm_ctx_t *accm = task->param;
    te_ccm_ctx_t *ctx = accm->ctx;
    te_ccm_request_t *req = accm->req;
    te_sca_operation_t op = req->crypt.op;
    uint8_t *nonce = req->crypt.nonce;
    uint32_t nlen = req->crypt.nlen;
    te_memlist_t *aad = &req->crypt.aad;
    te_memlist_t *in = &req->crypt.in;
    te_memlist_t *out = &req->crypt.out;
    uint8_t *tag = req->crypt.tag;
    uint32_t taglen = req->crypt.taglen;

    ret = te_ccm_crypt_list(ctx, op, nonce,
                            nlen, aad,
                            in, out, tag, taglen);
    osal_free(task);
    osal_free(accm);

    req->res = ret;
    req->base.completion(&req->base, req->res);
    return;
}

int te_ccm_acrypt(te_ccm_ctx_t *ctx, te_ccm_request_t *req)
{
    int ret = TE_SUCCESS;
    accm_ctx_t *accm = NULL;
    te_worker_task_t *task = NULL;
    if((NULL == ctx)
        || (NULL == ctx->crypt)
        || (NULL == req)
        || (NULL == req->crypt.nonce)
        || (NULL  == req->crypt.tag)
        || ((TE_DRV_SCA_ENCRYPT != req->crypt.op) &&
        (TE_DRV_SCA_DECRYPT != req->crypt.op))) {
        ret = TE_ERROR_BAD_PARAMS;
        __CCM_OUT__;
    }

    accm = osal_calloc(1, sizeof(accm_ctx_t));
    if (NULL == accm) {
        ret = TE_ERROR_OOM;
        __CCM_OUT__;
    }

    task = osal_calloc(1, sizeof(te_worker_task_t));
    if (NULL == task) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    task->param = (void *)accm;
    task->execute = execute_ccm_crypt;
    accm->ctx = ctx;
    accm->req = req;
    te_worker_pool_enqueue(task);

    return TE_SUCCESS;

err1:
    osal_free(accm);
__out__:
    return ret;
}
#endif /* CFG_TE_ASYNC_EN */
