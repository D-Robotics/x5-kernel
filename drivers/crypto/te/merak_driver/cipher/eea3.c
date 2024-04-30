//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#if defined(TE_ZUC_C)
#include <te_eea3.h>
#include "../common/te_3gpp_utils.h"

/**
 * SCA EEA3 private context
 */
typedef struct sca_eea3_ctx {
    uint8_t iv[SCA_MAX_BLOCK_SIZE];     /* initial vector */
    uint8_t npdata[TE_ZUC_BLOCK_SIZE];  /* misaligned block data */
    uint8_t ks[TE_ZUC_BLOCK_SIZE];      /* key stream(32bits) */
    uint8_t kspos;                      /* key stream offset in byte */
} sca_eea3_ctx_t;

int te_eea3_init(te_eea3_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg)
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;

    if ((NULL == ctx) || (TE_MAIN_ALGO_ZUC != malg)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    osal_memset(ctx, 0x00, sizeof(te_eea3_ctx_t));
    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    if (NULL == sdrv) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    ret = te_sca_alloc_ctx(sdrv,
                           malg,
                           sizeof(sca_eea3_ctx_t),
                           &ctx->crypt);
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }

    ctx->crypt->alg = TE_ALG_ZUC_F8;

__out__:
    return ret;
}

int te_eea3_free(te_eea3_ctx_t *ctx)
{
    int ret = TE_SUCCESS;
    sca_eea3_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eea3_ctx_t *)eea3_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    ret = te_sca_state(ctx->crypt);
    switch (ret) {
        case TE_DRV_SCA_STATE_INIT:
        case TE_DRV_SCA_STATE_READY:
            ret = TE_SUCCESS;
            break;
        case TE_DRV_SCA_STATE_START:
        case TE_DRV_SCA_STATE_UPDATE:
        case TE_DRV_SCA_STATE_LAST:
            ret = te_sca_finish(ctx->crypt, NULL, 0);
            if (TE_SUCCESS != ret) {
                OSAL_LOG_ERR("te_sca_finish raised exceptions!\n");
            }
            break;
        case TE_DRV_SCA_STATE_RAW:
            ret = TE_ERROR_BAD_STATE;
            break;
        default:
            if (ret > 0) {
                ret = TE_ERROR_BAD_STATE;
            }
            goto __out__;
    }

    osal_memset(prv_ctx, 0x00, sizeof(sca_eea3_ctx_t));
    ret = te_sca_free_ctx(ctx->crypt);
    ctx->crypt = NULL;
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("te_sca_free_ctx raised exceptions!\n");
    }

__out__:
    return ret;
}

int te_eea3_setkey(te_eea3_ctx_t *ctx, const uint8_t *key, uint32_t keybits)
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};

    if ((NULL == ctx) || (NULL == key)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    ret = te_sca_state(ctx->crypt);
    switch (ret) {
        case TE_DRV_SCA_STATE_INIT:
        case TE_DRV_SCA_STATE_READY:
            ret = TE_SUCCESS;
            break;
        case TE_DRV_SCA_STATE_START:
        case TE_DRV_SCA_STATE_UPDATE:
        case TE_DRV_SCA_STATE_LAST:
            ret = te_sca_finish(ctx->crypt, NULL, 0);
            if (TE_SUCCESS != ret) {
                OSAL_LOG_ERR("te_sca_finish raised exceptions!\n");
                goto __out__;
            }
            break;
        case TE_DRV_SCA_STATE_RAW:
            ret = TE_ERROR_BAD_STATE;
            break;
        default:
            if (ret > 0) {
                ret = TE_ERROR_BAD_STATE;
            }
            goto __out__;
    }

    key_desc.type = TE_KEY_TYPE_USER;
    key_desc.user.key = (uint8_t *)key;
    key_desc.user.keybits = keybits;
    ret = te_sca_setkey(ctx->crypt, &key_desc);

__out__:
    return ret;
}

int te_eea3_setseckey(te_eea3_ctx_t *ctx, const te_sec_key_t *key)
{
    int ret = TE_SUCCESS;
    te_sca_key_t key_desc = {0};

    if ((NULL == ctx) || (NULL == key)) {
        ret =  TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    ret = te_sca_state(ctx->crypt);
    switch (ret) {
        case TE_DRV_SCA_STATE_INIT:
        case TE_DRV_SCA_STATE_READY:
            ret = TE_SUCCESS;
            break;
        case TE_DRV_SCA_STATE_START:
        case TE_DRV_SCA_STATE_UPDATE:
        case TE_DRV_SCA_STATE_LAST:
            ret = te_sca_finish(ctx->crypt, NULL, 0);
            if (TE_SUCCESS != ret) {
                OSAL_LOG_ERR("te_sca_finish raised exceptions!\n");
                goto __out__;
            }
            break;
        case TE_DRV_SCA_STATE_RAW:
            ret = TE_ERROR_BAD_STATE;
            break;
        default:
            if (ret > 0) {
                ret = TE_ERROR_BAD_STATE;
            }
            goto __out__;
    }

    osal_memset(&key_desc, 0x00, sizeof(key_desc));
    key_desc.type = TE_KEY_TYPE_SEC;
    osal_memcpy(&key_desc.sec, key, sizeof(te_sec_key_t));
    ret = te_sca_setkey(ctx->crypt, &key_desc);

__out__:
    return ret;
}

int te_eea3_start(te_eea3_ctx_t *ctx, uint32_t count, uint32_t bearer, uint32_t dir)
{
    int ret = TE_SUCCESS;
    sca_eea3_ctx_t *prv_ctx = NULL;
    uint8_t *liv = NULL;
    uint32_t lcount = 0;

    if((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    if (bearer > 0x1F || dir > 0x01) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eea3_ctx_t *)eea3_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    ret = te_sca_state(ctx->crypt);
    switch (ret) {
        case TE_DRV_SCA_STATE_READY:
            ret = TE_SUCCESS;
            break;
        case TE_DRV_SCA_STATE_START:
        case TE_DRV_SCA_STATE_UPDATE:
        case TE_DRV_SCA_STATE_LAST:
            ret = te_sca_finish(ctx->crypt, NULL, 0);
            if (TE_SUCCESS != ret) {
                OSAL_LOG_ERR("te_sca_finish raised exceptions!\n");
                goto __out__;
            }
            break;
        case TE_DRV_SCA_STATE_RAW:
        case TE_DRV_SCA_STATE_INIT:
            ret = TE_ERROR_BAD_STATE;
            break;
        default:
            if (ret > 0) {
                ret = TE_ERROR_BAD_STATE;
            }
            goto __out__;
    }

    liv = prv_ctx->iv;
    osal_memset(prv_ctx, 0x00, sizeof(sca_eea3_ctx_t));

    /* IV package */
    lcount = HTOBE32(count);
    osal_memcpy(liv, &lcount, sizeof(lcount));
    liv[8]  = liv[0];
    liv[9]  = liv[1];
    liv[10] = liv[2];
    liv[11] = liv[3];
    liv[4]  = liv[12] = (((bearer & 0x1F) << 1) | (dir & 0x01)) << 2;

    ret = te_sca_start(ctx->crypt, TE_DRV_SCA_ENCRYPT,
                       prv_ctx->iv, sizeof(prv_ctx->iv));

__out__:
    return ret;
}

int te_eea3_update(te_eea3_ctx_t *ctx, size_t inlen, const uint8_t *in, uint8_t *out)
{
    int ret = TE_SUCCESS;
    size_t prolen = 0;
    size_t left = 0;
    const uint8_t *local_in = NULL;
    uint8_t *local_out = NULL;
    sca_eea3_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == ctx->crypt) ||
        ((NULL == out) && (inlen)) || ((NULL == in) && (inlen))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eea3_ctx_t *)eea3_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    if (0 == inlen) {
        ret = TE_SUCCESS;
        goto __out__;
    }

    left = inlen;
    local_in = in;
    local_out = out;

    /* Consume remained keystream */
    if (prv_ctx->kspos) {
        prolen = UTILS_MIN(left, ctx->crypt->blk_size - prv_ctx->kspos);
        BS_XOR(local_out,
               prv_ctx->ks + prv_ctx->kspos,
               local_in,
               prolen);
        prv_ctx->kspos = (prv_ctx->kspos + prolen) % (ctx->crypt->blk_size);
        left -= prolen;
        local_in += prolen;
        local_out += prolen;
    }

    /* Process complete block */
    prolen = UTILS_ROUND_DOWN(left, ctx->crypt->blk_size);
    ret = te_sca_update(ctx->crypt,
                        false,
                        prolen,
                        local_in,
                        local_out);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    left -= prolen;
    local_in += prolen;
    local_out += prolen;

    /* Process incomplete block */
    if (left > 0) {
        osal_memset(prv_ctx->npdata, 0x00, ctx->crypt->blk_size);

        /* Update last block to output */
        osal_memcpy(prv_ctx->npdata, local_in, left);
        ret = te_sca_update(ctx->crypt,
                            false,
                            ctx->crypt->blk_size,
                            prv_ctx->npdata,
                            prv_ctx->ks);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        osal_memcpy(local_out, prv_ctx->ks, left);

        /* Mark kspos in ks */
        prv_ctx->kspos = left;
    }

__out__:
    return ret;
}

int te_eea3_uplist(te_eea3_ctx_t *ctx, const te_memlist_t *in, te_memlist_t *out)
{
    int ret = TE_SUCCESS;
    size_t left = 0;
    size_t total_len = 0;
    size_t prolen = 0;
    te_memlist_t local_in = {0};
    te_memlist_t local_out = {0};
    sca_eea3_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == ctx->crypt) ||
        ((in != NULL) && (NULL == out))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    if (NULL == in) {
        ret = TE_SUCCESS;
        goto __out__;
    }

    if (((in->nent) && (NULL == in->ents)) ||
        ((out->nent) && (NULL == out->ents))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    total_len = left = te_memlist_get_total_len((te_memlist_t *)in);
    if (0 == left) {
        ret = TE_SUCCESS;
        goto __out__;
    }
    if (left > te_memlist_get_total_len(out)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eea3_ctx_t *)eea3_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    /* Consume remained keystream */
    if (prv_ctx->kspos > 0) {
        prolen = UTILS_MIN(left, ctx->crypt->blk_size - prv_ctx->kspos);
        ret = te_memlist_xor(out, in, prv_ctx->ks + prv_ctx->kspos, prolen);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        prv_ctx->kspos = (prv_ctx->kspos + prolen) % (ctx->crypt->blk_size);
        left -= prolen;
    }

    /* Processed complet blocks */
    prolen = UTILS_ROUND_DOWN(left, ctx->crypt->blk_size);
    if (prolen > 0) {
        /* Re-construct inlist and outlist */
        local_in.nent = in->nent;
        local_in.ents = (te_mement_t *)osal_calloc(sizeof(*local_in.ents),
                                                   local_in.nent);
        if (NULL == local_in.ents) {
            ret = TE_ERROR_OOM;
            goto __out__;
        }
        local_out.nent = out->nent;
        local_out.ents = (te_mement_t *)osal_calloc(sizeof(*local_out.ents),
                                                    local_out.nent);
        if (NULL == local_out.ents) {
            ret = TE_ERROR_OOM;
            goto __out__;
        }
        /* Duplicate from inlist and outlist */
        ret = te_memlist_dup(&local_in, in, total_len - left, prolen);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        ret = te_memlist_dup(&local_out, out, total_len - left, prolen);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        ret = te_sca_uplist(ctx->crypt, false, &local_in, &local_out);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        left -= prolen;
    }

    /* Processed remained data */
    if (left > 0) {
        OSAL_ASSERT(left < ctx->crypt->blk_size);
        osal_memset(prv_ctx->npdata, 0x00, ctx->crypt->blk_size);
        ret = te_memlist_copy(prv_ctx->npdata, in, total_len - left, left);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        ret = te_sca_update(ctx->crypt, false, ctx->crypt->blk_size,
                            prv_ctx->npdata, prv_ctx->ks);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        /* Update to outlist and save keystream */
        ret = te_memlist_fill(out, total_len - left, prv_ctx->ks, left);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        /* Mark kspos in ks */
        prv_ctx->kspos = left;
    }

__out__:
    OSAL_SAFE_FREE(local_in.ents);
    OSAL_SAFE_FREE(local_out.ents);
    return ret;
}

int te_eea3_finup(te_eea3_ctx_t *ctx, uint64_t inbits, const uint8_t *in, uint8_t *out)
{
    int ret = TE_SUCCESS;
    size_t prolen = 0;
    size_t leftbits = 0;
    size_t lastbits = 0;
    const uint8_t *local_in = NULL;
    uint8_t *local_out = NULL;
    sca_eea3_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == ctx->crypt) ||
        ((NULL == out) && (inbits)) || ((NULL == in) && (inbits))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eea3_ctx_t *)eea3_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    leftbits = inbits;
    lastbits = inbits % BYTE_BITS;
    local_in = in;
    local_out = out;

    /* Consume remained keystream */
    if (prv_ctx->kspos > 0) {
        prolen = UTILS_MIN(CEIL_BYTES(leftbits),
                           ctx->crypt->blk_size - prv_ctx->kspos);
        BS_XOR(local_out,
               prv_ctx->ks + prv_ctx->kspos,
               local_in,
               prolen);
        if (leftbits < (prolen * BYTE_BITS)) {
            leftbits = 0;
        } else {
            leftbits -= (prolen * BYTE_BITS);
        }
        prv_ctx->kspos = (prv_ctx->kspos + prolen) % (ctx->crypt->blk_size);
        local_in += prolen;
        local_out += prolen;
    }

    /* Process remained bitstream */
    if (leftbits > 0) {
        prolen = CEIL_BYTES(leftbits);
        ret = te_sca_update(ctx->crypt,
                            true,
                            prolen,
                            local_in,
                            local_out);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        local_in += prolen;
        local_out += prolen;
    }

    /* Wipe extra bits and finish session */
    if (lastbits > 0) {
        local_out--;
        local_out[0] = WIPE_TAIL_BITS(local_out[0], lastbits);
    }
    ret = te_sca_finish(ctx->crypt, NULL, 0);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    osal_memset(prv_ctx, 0x00, sizeof(sca_eea3_ctx_t));

__out__:
    return ret;
}

int te_eea3_finish(te_eea3_ctx_t *ctx)
{
    int ret = TE_SUCCESS;
    sca_eea3_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eea3_ctx_t *)eea3_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    ret = te_sca_finish(ctx->crypt, NULL, 0);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    osal_memset(prv_ctx, 0x00, sizeof(sca_eea3_ctx_t));

__out__:
    return ret;
}

int te_eea3_clone(const te_eea3_ctx_t *src, te_eea3_ctx_t *dst)
{
    int ret = TE_SUCCESS;
    te_eea3_ctx_t nctx = {0};
    sca_eea3_ctx_t *spctx = NULL;
    sca_eea3_ctx_t *npctx = NULL;

    if ((NULL == src) || (NULL == src->crypt) ||
        (NULL == dst)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    spctx = (sca_eea3_ctx_t *)eea3_priv_ctx((te_eea3_ctx_t*)src);
    if (NULL == spctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    ret = te_sca_alloc_ctx((te_sca_drv_t *)src->crypt->drv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_eea3_ctx_t),
                           &nctx.crypt);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }

    /* clone driver ctx */
    ret = te_sca_clone(src->crypt, nctx.crypt);
    if (TE_SUCCESS != ret) {
        te_eea3_free(&nctx);
        goto __out__;
    }

    /* clone private ctx */
    npctx = (sca_eea3_ctx_t *)eea3_priv_ctx(&nctx);
    TE_ASSERT(NULL != npctx);
    osal_memcpy(npctx, spctx, sizeof(*npctx));

    /* free dst */
    if (dst->crypt != NULL) {
        (void)te_eea3_free(dst);
    }

    /* copy nctx to dst */
    osal_memcpy(dst, &nctx, sizeof(*dst));

__out__:
    return ret;
}

#endif /* TE_ZUC_C */