//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#if defined(TE_SNOW3G_C)
#include <te_uea2.h>
#include "../common/te_3gpp_utils.h"

/**
 *  UEA2 private context
 */
typedef struct {
    uint8_t iv[16];                       /**< initial value presents as uint8 */
    uint8_t npdata[TE_SNOW3G_BLOCK_SIZE];  /**< remained incomplete data */
    uint8_t ks[TE_SNOW3G_BLOCK_SIZE];      /**< key stream(32bits) */
    uint8_t kspos;                        /**< key stream offset */
} sca_uea2_ctx_t;

int te_uea2_init(te_uea2_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg)
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;
    /** sannity check params  */
    if ((NULL == ctx) || (NULL == hdl) || (malg != TE_MAIN_ALGO_SNOW3G)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    ret = te_sca_alloc_ctx(sdrv, malg, sizeof(sca_uea2_ctx_t), &ctx->crypt);
    if (ret != TE_SUCCESS) {
        goto err_alloc;
    }
    ctx->crypt->alg = TE_ALG_SNOW3G_F8;

err_alloc:
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
err_params:
    return ret;
}

int te_uea2_free(te_uea2_ctx_t *ctx)
{
    int ret = TE_SUCCESS;
    sca_uea2_ctx_t *prv_ctx = NULL;
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    /** sannity check sate do compensation if need */
    ret = te_sca_state(ctx->crypt);
    switch (ret) {
    default:
        /** keep the original error code from lower layer once error occurs */
        if (ret > 0) {
            ret = TE_ERROR_BAD_STATE;
        }
        break;
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        ret = TE_SUCCESS;
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("te_sca_finish raises exceptions(%X)\n", ret);
            ret = TE_SUCCESS; /** ignore this error and let it can free resources */
        }
        break;
    }
    if (ret != TE_SUCCESS) {
        goto err_state;
    }
    /** free resources */
    prv_ctx = uea2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        goto err_params;
    }
    /** zeroize private data, anti-leakage */
    osal_memset(prv_ctx, 0x00, sizeof(*prv_ctx));
    ret = te_sca_free_ctx(ctx->crypt);
    ctx->crypt = NULL;

err_state:
err_params:
    return ret;
}

int te_uea2_setkey(te_uea2_ctx_t *ctx, const uint8_t *key, uint32_t keybits)
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};
    uint8_t k[MAX_EK3_SIZE] = {0};
    /** sanity check params */
    if ((NULL == ctx) || (NULL == key)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    /** sannity check sate do compensation if need */
    ret = te_sca_state(ctx->crypt);
    switch (ret) {
    default:
        if (ret > 0) {
            ret = TE_ERROR_BAD_STATE;
        }
        break;
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        ret = TE_SUCCESS;
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        break;
    }
    if (ret != TE_SUCCESS) {
        goto err_state;
    }
    /** if keybits valid re-build the key
     * K3 = CK[0] || CK[1] || CK[2] || … || CK[31]
     * K2 = CK[32] || CK[33] || CK[34] || … || CK[63]
     * K1 = CK[64] || CK[65] || CK[66] || … || CK[95]
     * K0 = CK[96] || CK[97] || CK[98] || … || CK[127]
    */
    if (128  == keybits) {
        osal_memcpy(k, key + 12, 4);
        osal_memcpy(k + 4, key + 8, 4);
        osal_memcpy(k + 8, key + 4, 4);
        osal_memcpy(k + 12, key, 4);
    } else {
        ret = TE_ERROR_BAD_KEY_LENGTH;
        goto err_key;
    }
    /** set key to sca drv */
    osal_memset(&key_desc, 0x00, sizeof(key_desc));
    key_desc.type = TE_KEY_TYPE_USER;
    key_desc.user.key = k;
    key_desc.user.keybits = keybits;
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    osal_memset(k, 0x00, sizeof(k));

err_key:
err_state:
err_params:
    return ret;
}

int te_uea2_setseckey(te_uea2_ctx_t *ctx, const te_sec_key_t *key)
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};
    /** sanity check params */
    if ((NULL == ctx) || (NULL == key)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    /** sannity check sate do compensation if need */
    ret = te_sca_state(ctx->crypt);
    switch (ret) {
    default:
        if (ret > 0) {
            ret = TE_ERROR_BAD_STATE;
        }
        break;
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        break;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        ret = TE_SUCCESS;
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        break;
    }
    if (ret != TE_SUCCESS) {
        goto err_state;
    }
    /** set key to sca drv */
    osal_memset(&key_desc, 0x00, sizeof(key_desc));
    key_desc.type = TE_KEY_TYPE_SEC;
    osal_memcpy(&key_desc.sec, key, sizeof(*key));
    ret = te_sca_setkey(ctx->crypt, &key_desc);
err_state:
err_params:
    return ret;
}

int te_uea2_build_iv(uint8_t iv[16], uint32_t count,
                     uint32_t bearer, uint32_t dir)
{
    int ret = TE_SUCCESS;
    uint32_t tmp_iv = 0;
    /** sanity check params */
    if ((NULL == iv) || (bearer > 0x1F) || (dir > 1)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }

    tmp_iv = ((bearer & 0x1f) << 27) | ((dir & 0x1) << 26);
    tmp_iv = HTOBE32(tmp_iv);
    osal_memcpy(iv, &tmp_iv, sizeof(tmp_iv));
    tmp_iv = HTOBE32(count);
    osal_memcpy(iv + 4, &tmp_iv, sizeof(tmp_iv));
    osal_memcpy(iv + 8, iv, 8);
err_params:
    return ret;
}

int te_uea2_start(te_uea2_ctx_t *ctx, const uint8_t iv[16])
{
    int ret = TE_SUCCESS;
    sca_uea2_ctx_t *prv_ctx = NULL;
    /** sanity check params */
    if ((NULL == ctx) || (NULL == iv)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    prv_ctx = (sca_uea2_ctx_t *)uea2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    /** sannity check state do compensation if need */
    ret = te_sca_state(ctx->crypt);
    switch (ret) {
    default:
        if (ret > 0) {
            ret = TE_ERROR_BAD_STATE;
        }
        break;
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
        ret = TE_ERROR_BAD_STATE;
        break;
    case TE_DRV_SCA_STATE_READY:
        ret = TE_SUCCESS;
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        break;
    }
    if (ret != TE_SUCCESS) {
        goto err_state;
    }

    osal_memset(prv_ctx, 0x00, sizeof(*prv_ctx));
    /** private context initialization for new session*/
    osal_memcpy(prv_ctx->iv, iv, sizeof(prv_ctx->iv));
    /** start the new session */
    ret = te_sca_start(ctx->crypt, TE_DRV_SCA_ENCRYPT,
                       prv_ctx->iv, sizeof(prv_ctx->iv));
err_state:
err_params:
    return ret;
}

int te_uea2_update(te_uea2_ctx_t *ctx,
                   size_t inlen,
                   const uint8_t *in,
                   uint8_t *out)
{
    int ret = TE_SUCCESS;
    sca_uea2_ctx_t *prv_ctx = NULL;
    size_t proclen = 0;
    size_t left = inlen;
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt) ||
        ((inlen != 0) && ((NULL == in) || (NULL == out)))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    prv_ctx = (sca_uea2_ctx_t *)uea2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    /** return immediately if 'inlen = 0' */
    if ( 0 == inlen) {
        goto fin;
    }
    /** consume key stream if any */
    if (prv_ctx->kspos > 0) {
        proclen = UTILS_MIN(left, ctx->crypt->blk_size - prv_ctx->kspos);
        BS_XOR(out, prv_ctx->ks + prv_ctx->kspos, in, proclen);
        prv_ctx->kspos = (prv_ctx->kspos + proclen) % ctx->crypt->blk_size;
        left -= proclen;
    }
    /** handle complete blocks */
    proclen = UTILS_ROUND_DOWN(left, ctx->crypt->blk_size);
    if (proclen > 0) {
        ret = te_sca_update(ctx->crypt, false, proclen,
                            in + inlen - left, out + inlen - left);
        if (ret != TE_SUCCESS) {
            goto err_update;
        }
        left -= proclen;
    }
    /** handle remainder if any */
    if (left > 0) {
        osal_memset(prv_ctx->npdata, 0x00, sizeof(prv_ctx->npdata));
        osal_memcpy(prv_ctx->npdata, in + inlen - left, left);
        ret = te_sca_update(ctx->crypt, false, ctx->crypt->blk_size,
                            prv_ctx->npdata, prv_ctx->ks);
        if (ret != TE_SUCCESS) {
            goto err_update;
        }
        osal_memcpy(out + inlen - left, prv_ctx->ks, left);
        prv_ctx->kspos = left;
    }

err_update:
err_params:
fin:
    return ret;
}

int te_uea2_uplist(te_uea2_ctx_t *ctx,
                   const te_memlist_t *in,
                   te_memlist_t *out)
{
    int ret = TE_SUCCESS;
    sca_uea2_ctx_t *prv_ctx = NULL;
    size_t proclen = 0;
    size_t inlen = 0;
    size_t left = 0;
    te_memlist_t left_in = {0};
    te_memlist_t left_out = {0};
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt) ||
        ((in != NULL) && (NULL == out))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    if (NULL == in) {
        goto fin;
    }
    if (((in->nent > 0) && (NULL == in->ents)) ||
        ((out->nent > 0) && (NULL == out->ents))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    inlen = left = te_memlist_get_total_len((te_memlist_t *)in);
    /** return immediately if 'inlen = 0' */
    if (0 == inlen) {
        goto fin;
    }
    if (inlen > te_memlist_get_total_len(out)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    /** consume left key stream first if any */
    prv_ctx = (sca_uea2_ctx_t *)uea2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    if (prv_ctx->kspos > 0) {
        proclen = UTILS_MIN(left, ctx->crypt->blk_size - prv_ctx->kspos);
        (void)te_memlist_xor(out, in, prv_ctx->ks + prv_ctx->kspos, proclen);
        prv_ctx->kspos = (prv_ctx->kspos + proclen) % ctx->crypt->blk_size;
        left -= proclen;
    }
    /** handle complete blocks */
    proclen = UTILS_ROUND_DOWN(left, ctx->crypt->blk_size);
    if (proclen > 0) {
        /** re-build in memory list and out memory list */
        left_in.nent = in->nent;
        left_in.ents = (te_mement_t *)osal_calloc(sizeof(*left_in.ents),
                                                        left_in.nent);
        if (NULL == left_in.ents) {
            ret = TE_ERROR_OOM;
            goto err_alloc_in;
        }
        left_out.nent = out->nent;
        left_out.ents = (te_mement_t *)osal_calloc(sizeof(*left_out.ents),
                                                        left_out.nent);
        if (NULL == left_out.ents) {
            ret = TE_ERROR_OOM;
            goto err_alloc_out;
        }
        (void)te_memlist_dup(&left_in, in, inlen - left, proclen);
        (void)te_memlist_dup(&left_out, out, inlen - left, proclen);
        left -= proclen;
        /** feed data into engine to get the result */
        ret = te_sca_uplist(ctx->crypt, false, &left_in, &left_out);
        if (ret != TE_SUCCESS) {
            goto err_update;
        }
    }
    /**
     * handle remainder if any.
     * [Important Note] Why don't we link the remainder to previous list?
     * Because cacheline issue is there. sca_driver only handle such case
     * that leading address and tail address are not cacheline-aligned,
     * if we link a node to the tail of the list then we can't make sure
     * the previous tail node is cachline-aligned and new cacheline-algined
     * node will cover it and break the rules.
     */
    if (left > 0) {
        /** copy the left-in into npdata and perform an update to get
         *  the out and ks.
         */
        osal_memset(prv_ctx->npdata, 0x00, sizeof(prv_ctx->npdata));
        (void)te_memlist_copy(prv_ctx->npdata, in, inlen - left, left);
        ret = te_sca_update(ctx->crypt, false, sizeof(prv_ctx->npdata),
                            prv_ctx->npdata, prv_ctx->ks);
        if (ret != TE_SUCCESS) {
            goto err_update;
        }
        /** fill out and update key stream */
        (void)te_memlist_fill(out, inlen - left, prv_ctx->ks, left);
        prv_ctx->kspos = left;
    }

err_update:
    OSAL_SAFE_FREE(left_out.ents);
err_alloc_out:
    OSAL_SAFE_FREE(left_in.ents);
err_alloc_in:
err_params:
fin:
    return ret;
}

int te_uea2_finup(te_uea2_ctx_t *ctx, uint64_t inbits,
                  const uint8_t *in, uint8_t *out)
{
    int ret = TE_SUCCESS;
    sca_uea2_ctx_t *prv_ctx = NULL;
    size_t proclen = 0;
    uint64_t leftbits = inbits;
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt) ||
        ((inbits != 0) && ((NULL == in) || (NULL == out)))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    if (0 == inbits) {
        goto fin;
    }
    /** consume left key stream first if any */
    prv_ctx = (sca_uea2_ctx_t *)uea2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    if (prv_ctx->kspos > 0) {
        proclen = UTILS_MIN(CEIL_BYTES(leftbits),
                                ctx->crypt->blk_size - prv_ctx->kspos);
        BS_XOR(out, prv_ctx->ks + prv_ctx->kspos, in, proclen);
        prv_ctx->kspos = (prv_ctx->kspos + proclen) % ctx->crypt->blk_size;
        if (leftbits < (proclen * BYTE_BITS)) {
            leftbits = 0;
        } else {
            leftbits -= (proclen * BYTE_BITS);
        }
    }
    /** handle remained bit streams */
    proclen = CEIL_BYTES(leftbits);
    if (proclen > 0) {
        ret = te_sca_update(ctx->crypt, true, proclen,
                            in + CEIL_BYTES(inbits - leftbits),
                            out + CEIL_BYTES(inbits - leftbits));
        if (ret != TE_SUCCESS) {
            goto err_update;
        }
    }
    /** zero last bits in case of inbits is not byte-aligned */
    if ((inbits % BYTE_BITS) != 0) {
        out[inbits / BYTE_BITS] = WIPE_TAIL_BITS(out[inbits / BYTE_BITS],
                                    inbits % BYTE_BITS);
    }

fin:
    /** finish session */
    ret = te_sca_finish(ctx->crypt, NULL, 0);

err_update:
err_params:
    return ret;
}

int te_uea2_finish(te_uea2_ctx_t *ctx)
{
    int ret = TE_SUCCESS;
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    /** finish session */
    ret = te_sca_finish(ctx->crypt, NULL, 0);

err_params:
    return ret;
}

int te_uea2_clone(const te_uea2_ctx_t *src, te_uea2_ctx_t *dst)
{
    int ret = TE_SUCCESS;
    te_uea2_ctx_t swap_ctx = {0};
    sca_uea2_ctx_t *prv_src = NULL;
    sca_uea2_ctx_t *prv_swap = NULL;
    /** sanity check params */
    if ((NULL == src) || (NULL == src->crypt) || (NULL == dst)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    osal_memset(&swap_ctx, 0x00, sizeof(swap_ctx));
    ret = te_sca_alloc_ctx((te_sca_drv_t *)src->crypt->drv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_uea2_ctx_t),
                           &swap_ctx.crypt);
    if (ret != TE_SUCCESS) {
        goto err_alloc;
    }
    /** clone sca driver context */
    ret = te_sca_clone(src->crypt, swap_ctx.crypt);
    if (ret != TE_SUCCESS) {
        goto err_drv_clone;
    }
    /** clone private context */
    prv_src = (sca_uea2_ctx_t *)uea2_priv_ctx(src);
    prv_swap = (sca_uea2_ctx_t *)uea2_priv_ctx(&swap_ctx);
    osal_memcpy(prv_swap, prv_src, sizeof(*prv_src));
    /** replace dst context */
    if (dst->crypt != NULL) {
        /**
         *  Ignore the error return from te_uea2_free, because once caller
         *  provide an uninitialized raw context from stack it may hold a
         *  pointer other than NULL which may treat te_uea2_free to return
         *  error codes such as TE_ERROR_BAD_STATE.
         */
        (void)te_uea2_free(dst);
    }
    osal_memcpy(dst, &swap_ctx, sizeof(*dst));
    goto fin;
err_drv_clone:
    te_sca_free_ctx(swap_ctx.crypt);
err_alloc:
err_params:
fin:
    return ret;
}

#endif /* TE_SNOW3G_C */
