//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#if defined(TE_KASUMI_C)
#include <te_f8.h>
#include "../common/te_3gpp_utils.h"

typedef struct {
    uint8_t iv[8];                          /* Initial value 64bits */
    uint8_t npdata[TE_KASUMI_BLOCK_SIZE];   /* Not processed data */
    uint8_t ks[TE_KASUMI_BLOCK_SIZE];       /* Key stream */
    uint8_t kspos;                          /* Key stream offset */
} sca_f8_ctx_t;

int te_f8_init(te_f8_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg)
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;

    if ((NULL == ctx) ||
        (NULL == hdl) ||
        (TE_MAIN_ALGO_KASUMI != malg)) {
        ret = TE_ERROR_BAD_PARAMS;
        return ret;
    }

    osal_memset(ctx, 0x00, sizeof(te_f8_ctx_t));
    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    if (NULL == sdrv) {
        ret = TE_ERROR_BAD_FORMAT;
        return ret;
    }

    /* Alloc ctx from te_sca. */
    ret = te_sca_alloc_ctx(sdrv, malg,
                           sizeof(sca_f8_ctx_t),
                           &ctx->crypt);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }

    /* Init ctx->crypto values. */
    ctx->crypt->alg = TE_ALG_KASUMI_F8;

__out__:
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
    return ret;
}

int te_f8_free(te_f8_ctx_t *ctx)
{
    int ret = TE_SUCCESS;
    sca_f8_ctx_t *prv_ctx = NULL;

    if (NULL == ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* Check sca driver state */
    ret = te_sca_state(ctx->crypt);
    if (ret < 0) {
        goto __out__;
    }
    switch (ret) {
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        goto __out__;
    case TE_DRV_SCA_STATE_READY:
    case TE_DRV_SCA_STATE_INIT:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        /**
         * State compensation
         * If it fails in te_sca_finish, only log prompt,
         * need to continue free up ctx and reset ret value.
         */
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        if (TE_SUCCESS != ret) {
            OSAL_LOG_ERR("te_sca_finish raises exceptions on te_f8_free");
        }
        break;
    }

    /* Free te_f8_ctx */
    prv_ctx = (sca_f8_ctx_t *)f8_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    osal_memset(prv_ctx, 0x00, sizeof(sca_f8_ctx_t));
    ret = te_sca_free_ctx(ctx->crypt);
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("te_sca_free_ctx raised exceptions!\n");
    }
    ctx->crypt = NULL;

__out__:
   return ret;
}

int te_f8_setkey(te_f8_ctx_t *ctx, const uint8_t *key, uint32_t keybits)
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};

    if((NULL == ctx) || (NULL == key)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* Check sca driver state */
    ret = te_sca_state(ctx->crypt);
    if (ret < 0) {
        goto __out__;
    }
    switch (ret) {
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        goto __out__;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        break;
    }

    /* Set user key to sca driver */
    key_desc.type = TE_KEY_TYPE_USER;
    key_desc.user.key = (uint8_t *)key;
    key_desc.user.keybits = keybits;
    ret = te_sca_setkey(ctx->crypt, &key_desc);

__out__:
    return ret ;
}

int te_f8_setseckey(te_f8_ctx_t *ctx, te_sec_key_t *key)
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};

    if((NULL == ctx) || (NULL == key)){
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* Check sca driver state */
    ret = te_sca_state(ctx->crypt);
    if (ret < 0) {
        goto __out__;
    }
    switch (ret) {
    default:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        goto __out__;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        break;
    }

    /* set sec key to sca driver */
    key_desc.type = TE_KEY_TYPE_SEC;
    osal_memcpy(&key_desc.sec, key, sizeof(te_sec_key_t));
    ret = te_sca_setkey(ctx->crypt, &key_desc);

__out__:
    return ret;
}

int te_f8_start(te_f8_ctx_t *ctx, uint32_t count, uint32_t bearer, uint32_t dir)
{
    int ret = TE_SUCCESS;
    sca_f8_ctx_t *prv_ctx = NULL;

    if (NULL == ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* Check significant bit number of bearer is 5, dir is 1. */
    if (bearer > 0x1FU || dir > 0x01U) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* Check sca driver state */
    ret = te_sca_state(ctx->crypt);
    if (ret < 0) {
        goto __out__;
    }
    switch (ret) {
    default:
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
        ret = TE_ERROR_BAD_STATE;
        goto __out__;
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        break;
    }

    /* Prepare call start. */
    prv_ctx = (sca_f8_ctx_t *)f8_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }
    osal_memset(prv_ctx->iv, 0x00, sizeof(prv_ctx->iv));
    prv_ctx->iv[0] = (uint8_t)(count >> 24) & 0xFF;
    prv_ctx->iv[1] = (uint8_t)(count >> 16) & 0xFF;
    prv_ctx->iv[2] = (uint8_t)(count >> 8) & 0xFF;
    prv_ctx->iv[3] = (uint8_t)(count) & 0xFF;
    prv_ctx->iv[4] = (uint8_t)((bearer & 0x1FU) << 3) & 0xFF;
    prv_ctx->iv[4] |= (uint8_t)((dir & 0x01U) << 2) & 0xFF;
    ret = te_sca_start(ctx->crypt, 0, prv_ctx->iv, sizeof(prv_ctx->iv));
    if (TE_SUCCESS != ret) {
        return ret;
    }

    /* Init prv_ctx values. */
    osal_memset(prv_ctx, 0x00, sizeof(*prv_ctx));

__out__:
    return ret;
}

int te_f8_update(te_f8_ctx_t *ctx, size_t inlen, const uint8_t *in, uint8_t *out)
{
    int ret = TE_SUCCESS;
    sca_f8_ctx_t *prv_ctx = NULL;
    size_t left = inlen;
    size_t prolen = 0;
    size_t ofs = 0;

    if((NULL == ctx) ||
       (NULL == ctx->crypt) ||
       (inlen && (NULL == in)) ||
       (inlen && (NULL == out))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* inlen is 0, do nothing */
    if (0 == inlen) {
        goto __out__;
    }

    prv_ctx = (sca_f8_ctx_t *)f8_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    /* Consume remained keystream. */
    if (prv_ctx->kspos > 0) {
        prolen = UTILS_MIN(left, ctx->crypt->blk_size - prv_ctx->kspos);
        BS_XOR(out, prv_ctx->ks + prv_ctx->kspos, in, prolen);
        prv_ctx->kspos = (prv_ctx->kspos + prolen) % ctx->crypt->blk_size;
        left -= prolen;
    }

    /* Process complete blocks. */
    prolen = UTILS_ROUND_DOWN(left, ctx->crypt->blk_size);
    if (prolen > 0) {
        ofs = inlen - left;
        ret = te_sca_update(ctx->crypt,
                            false,
                            prolen,
                            in + ofs,
                            out + ofs);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        left -= prolen;
    }

    /* Process incomplete block. */
    if (left > 0) {
        osal_memset(prv_ctx->npdata, 0x00, ctx->crypt->blk_size);
        osal_memcpy(prv_ctx->npdata, in + inlen - left, left);
        ret = te_sca_update(ctx->crypt,
                            false,
                            ctx->crypt->blk_size,
                            prv_ctx->npdata,
                            prv_ctx->ks);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        osal_memcpy(out + inlen - left, prv_ctx->ks, left);
        prv_ctx->kspos = left;
    }

__out__:
    return ret;
}

int te_f8_finish(te_f8_ctx_t *ctx)
{
    int ret = TE_SUCCESS;

    if (NULL == ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }
    ret = te_sca_finish(ctx->crypt, NULL, 0);
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("te_sca_finish raised exceptions!\n");
    }
__out__:
    return ret;
}

int te_f8_uplist(te_f8_ctx_t *ctx, const te_memlist_t *in, te_memlist_t *out)
{
    int ret = TE_SUCCESS;
    sca_f8_ctx_t *prv_ctx = NULL;
    size_t left = 0;
    size_t prolen = 0;
    size_t inlen = 0;
    te_memlist_t left_in = {0};
    te_memlist_t left_out = {0};

    if ((NULL == ctx) || (NULL == ctx->crypt) ||
        ((in != NULL) && (NULL == out))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    if (NULL == in) {
        goto __out__;
    }

    if (((in->nent > 0) && (NULL == in->ents)) ||
        ((out->nent > 0) && (NULL == out->ents))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* Check if length of in more than length of out. */
    inlen = left = te_memlist_get_total_len((te_memlist_t *)in);
    if (inlen > te_memlist_get_total_len(out)) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    } else if (0 == inlen) {
        goto __out__;
    }

    prv_ctx = (sca_f8_ctx_t *)f8_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    /* Consume left key stream */
    if (prv_ctx->kspos > 0) {
        prolen = UTILS_MIN(left, ctx->crypt->blk_size - prv_ctx->kspos);
        (void)te_memlist_xor(out, in, prv_ctx->ks + prv_ctx->kspos, prolen);
        prv_ctx->kspos = (prv_ctx->kspos + prolen) % ctx->crypt->blk_size;
        left -= prolen;
    }

    /* Process complete blocks. */
    prolen = UTILS_ROUND_DOWN(left, ctx->crypt->blk_size);
    if (prolen > 0) {
        left_in.nent = in->nent;
        left_in.ents = (te_mement_t *)osal_calloc(sizeof(*left_in.ents),
                                                  left_in.nent);
        if (NULL == left_in.ents) {
            ret = TE_ERROR_OOM;
            goto __out__;
        }
        left_out.nent = out->nent;
        left_out.ents = (te_mement_t *)osal_calloc(sizeof(*left_out.ents),
                                                  left_out.nent);
        if (NULL == left_out.ents) {
            ret = TE_ERROR_OOM;
            goto __out__;
        }
        (void)te_memlist_dup(&left_in, in, inlen - left, prolen);
        (void)te_memlist_dup(&left_out, out, inlen - left, prolen);
        left -= prolen;
        ret = te_sca_uplist(ctx->crypt, false, &left_in, &left_out);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
    }

    /* Process incomplete block. */
    if (left > 0) {
        osal_memset(prv_ctx->npdata, 0, sizeof(prv_ctx->npdata));
        (void)te_memlist_copy(prv_ctx->npdata, in, inlen - left, left);
        ret = te_sca_update(ctx->crypt,
                            false,
                            ctx->crypt->blk_size,
                            prv_ctx->npdata,
                            prv_ctx->ks);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        /* fill out and update out and key stream */
        (void)te_memlist_fill(out, inlen - left, prv_ctx->ks, left);
        prv_ctx->kspos = left;
    }

__out__:
    OSAL_SAFE_FREE(left_in.ents);
    OSAL_SAFE_FREE(left_out.ents);
    return ret;
}

int te_f8_finup(te_f8_ctx_t *ctx, uint64_t inbits, const uint8_t *in, uint8_t *out)
{
    int ret = TE_SUCCESS;
    sca_f8_ctx_t *prv_ctx = NULL;
    size_t leftbits = inbits;
    size_t prolen = 0;
    size_t ofs = 0;

    if((NULL == ctx) ||
       (NULL == ctx->crypt) ||
       (inbits && (NULL == in)) ||
       (inbits && (NULL == out))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* inbits is 0, call finish directly */
    if (0 == inbits) {
        goto __finish__;
    }

    prv_ctx = (sca_f8_ctx_t *)f8_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    /* Consume remained keystream. */
    if (prv_ctx->kspos > 0) {
        prolen = UTILS_MIN(CEIL_BYTES(leftbits),
                           ctx->crypt->blk_size - prv_ctx->kspos);
        BS_XOR(out, prv_ctx->ks + prv_ctx->kspos, in, prolen);
        prv_ctx->kspos = (prv_ctx->kspos + prolen) % ctx->crypt->blk_size;
        if (leftbits < prolen * BYTE_BITS) {
            leftbits = 0;
        } else {
            leftbits -= prolen * BYTE_BITS;
        }
    }

    /* Consume remained bitstream. */
    if (leftbits > 0) {
        prolen = CEIL_BYTES(leftbits);
        ofs = CEIL_BYTES(inbits - leftbits);
        /* This is the last update. */
        ret = te_sca_update(ctx->crypt,
                            true,
                            prolen,
                            in + ofs,
                            out + ofs);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
    }
    if (inbits % BYTE_BITS) {
        out[inbits / BYTE_BITS] =
            WIPE_TAIL_BITS(out[inbits / BYTE_BITS],
                           inbits % BYTE_BITS);
    }

__finish__:
    ret = te_sca_finish(ctx->crypt, NULL, 0);
__out__:
    return ret;
}

int te_f8_clone(const te_f8_ctx_t *src, te_f8_ctx_t *dst)
{
    int ret = TE_SUCCESS;
    te_f8_ctx_t swap_ctx = {0};
    sca_f8_ctx_t *prv_src = NULL;
    sca_f8_ctx_t *prv_swap = NULL;

    /** sanity check params */
    if ((NULL == src) || (NULL == src->crypt) || (NULL == dst)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }
    osal_memset(&swap_ctx, 0x00, sizeof(swap_ctx));
    ret = te_sca_alloc_ctx((te_sca_drv_t *)src->crypt->drv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_f8_ctx_t),
                           &swap_ctx.crypt);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    /** clone sca driver context */
    ret = te_sca_clone(src->crypt, swap_ctx.crypt);
    if (TE_SUCCESS != ret) {
        goto __err__;
    }
    /** clone private context */
    prv_src = (sca_f8_ctx_t *)f8_priv_ctx(src);
    prv_swap = (sca_f8_ctx_t *)f8_priv_ctx(&swap_ctx);
    osal_memcpy(prv_swap, prv_src, sizeof(*prv_src));
    /** replace dst context */
    if (NULL != dst->crypt) {
        /**
         *  Ignore the error return from te_f8_free, because once caller
         *  provide an uninitialized raw context from stack it may hold a
         *  pointer other than NULL which may treat te_f8_free to return
         *  error codes such as TE_ERROR_BAD_STATE.
         */
        (void)te_f8_free(dst);
    }
    osal_memcpy(dst, &swap_ctx, sizeof(*dst));
    goto __out__;
__err__:
    te_sca_free_ctx(swap_ctx.crypt);
__out__:
    return ret;
}
#endif  /* defined(TE_KASUMI_C) */