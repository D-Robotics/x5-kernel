//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#if defined(TE_ZUC_C)
#include <te_eia3.h>
#include "../common/te_3gpp_utils.h"

#define EIA3_MAX_BLOCK_SIZE   (8U)  /* EIA3 block size(4) + padding size(4) */

/**
 * SCA EIA3 private context
 */
typedef struct sca_eia3_ctx {
    uint8_t iv[SCA_MAX_BLOCK_SIZE];         /* initial vector */
    uint8_t npdata[EIA3_MAX_BLOCK_SIZE];    /* not processed data */
    uint8_t nplen;                          /* not processed data length in byte */
} sca_eia3_ctx_t;

int te_eia3_init(te_eia3_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg)
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;

    if ((NULL == ctx) || (TE_MAIN_ALGO_ZUC != malg)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    osal_memset(ctx, 0x00, sizeof(te_eia3_ctx_t));
    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    if (NULL == sdrv) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    ret = te_sca_alloc_ctx(sdrv,
                           malg,
                           sizeof(sca_eia3_ctx_t),
                           &ctx->crypt);
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }

    ctx->crypt->alg = TE_ALG_ZUC_F9;

__out__:
    return ret;
}

int te_eia3_free(te_eia3_ctx_t *ctx)
{
    int ret = TE_SUCCESS;
    sca_eia3_ctx_t *prv_ctx = NULL;

    if((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eia3_ctx_t *)eia3_priv_ctx(ctx);
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

    osal_memset(prv_ctx, 0x00, sizeof(sca_eia3_ctx_t));
    ret = te_sca_free_ctx(ctx->crypt);
    ctx->crypt = NULL;
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("te_sca_free_ctx raised exceptions!\n");
    }

__out__:
    return ret;
}

int te_eia3_setkey(te_eia3_ctx_t *ctx, const uint8_t *key, uint32_t keybits)
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

int te_eia3_setseckey(te_eia3_ctx_t *ctx, const te_sec_key_t *key)
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
    return ret ;
}

int te_eia3_start(te_eia3_ctx_t *ctx, uint32_t count, uint32_t bearer, uint32_t dir)
{
    int ret = TE_SUCCESS;
    sca_eia3_ctx_t *prv_ctx = NULL;
    uint8_t *liv = NULL;
    uint32_t lcount = 0;

    if ((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    if (bearer > 0x1F || dir > 0x01) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eia3_ctx_t *)eia3_priv_ctx(ctx);
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
    osal_memset(prv_ctx, 0x00, sizeof(sca_eia3_ctx_t));

    /* IV package */
    lcount = HTOBE32(count);
    osal_memcpy(liv, &lcount, sizeof(lcount));
    liv[9]  = liv[1];
    liv[10] = liv[2];
    liv[11] = liv[3];
    liv[4]  = liv[12] = (bearer & 0x1F) << 3;
    liv[8]  = liv[0] ^ ((dir & 0X01) << 7);
    liv[14] = ((dir & 0X01) << 7);

    ret = te_sca_start(ctx->crypt, TE_DRV_SCA_ENCRYPT,
                       prv_ctx->iv, sizeof(prv_ctx->iv));

__out__:
    return ret;
}

int te_eia3_update(te_eia3_ctx_t *ctx, size_t inlen, const uint8_t *in)
{
    int ret = TE_SUCCESS;
    size_t prolen = 0;
    size_t left = 0;
    const uint8_t *local_in = NULL;
    sca_eia3_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == ctx->crypt) ||
        ((NULL == in) && (inlen))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eia3_ctx_t *)eia3_priv_ctx(ctx);
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

    /* Process npdata */
    if (prv_ctx->nplen > 0) {
        prolen = UTILS_MIN(left, ctx->crypt->blk_size - prv_ctx->nplen);
        osal_memcpy(prv_ctx->npdata + prv_ctx->nplen, local_in, prolen);
        prv_ctx->nplen += prolen;
        left -= prolen;
        local_in += prolen;
        /* Flush npdata to engine */
        if (ctx->crypt->blk_size == prv_ctx->nplen) {
            ret = te_sca_update(ctx->crypt,
                                false,
                                prv_ctx->nplen,
                                prv_ctx->npdata,
                                NULL);
            if (TE_SUCCESS != ret) {
                goto __out__;
            }
            osal_memset(prv_ctx->npdata, 0x00, prv_ctx->nplen);
            prv_ctx->nplen = 0;
        }
    }

    /* Process complete block */
    prolen = UTILS_ROUND_DOWN(left, ctx->crypt->blk_size);
    ret = te_sca_update(ctx->crypt,
                        false,
                        prolen,
                        local_in,
                        NULL);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    left -= prolen;
    local_in += prolen;

    /* Process incomplete block */
    if (left > 0) {
        osal_memcpy(prv_ctx->npdata, local_in, left);
        prv_ctx->nplen = left;
    }

__out__:
    return ret;
}

int te_eia3_uplist(te_eia3_ctx_t *ctx, const te_memlist_t *in)
{
    int ret = TE_SUCCESS;
    size_t left = 0;
    size_t total_len = 0;
    size_t prolen = 0;
    te_memlist_t local_in = {0};
    sca_eia3_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    if (NULL == in) {
        ret = TE_SUCCESS;
        goto __out__;
    }

    if ((in->nent) && (NULL == in->ents)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eia3_ctx_t *)eia3_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    total_len = left = te_memlist_get_total_len((te_memlist_t *)in);
    if (0 == left) {
        ret = TE_SUCCESS;
        goto __out__;
    }
    /* overflow check */
    if (total_len > prv_ctx->nplen + total_len) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* Processed complete block */
    if (left >= (ctx->crypt->blk_size - prv_ctx->nplen)) {
        prolen = UTILS_ROUND_DOWN(left + prv_ctx->nplen, ctx->crypt->blk_size);
        /* malloc one more node to save npdata */
        local_in.nent = in->nent + 1;
        local_in.ents = (te_mement_t *)osal_calloc(sizeof(*local_in.ents), local_in.nent);
        if (NULL == local_in.ents) {
            ret = TE_ERROR_OOM;
            goto __out__;
        }
        /* Processed inlist */
        if (prv_ctx->nplen > 0) {
            local_in.ents++;
            local_in.nent--;
        }
        ret = te_memlist_dup(&local_in, (te_memlist_t *)in, 0, prolen - prv_ctx->nplen);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        /* Processed npdata */
        if (prv_ctx->nplen > 0) {
            local_in.ents--;
            local_in.nent++;
            local_in.ents[0].buf = prv_ctx->npdata;
            local_in.ents[0].len = prv_ctx->nplen;
        }

        ret = te_sca_uplist(ctx->crypt, false, &local_in, NULL);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        left -= (prolen - prv_ctx->nplen);
        prv_ctx->nplen = 0;
    }

    /* Process remained data */
    if (left > 0) {
        ret = te_memlist_copy(prv_ctx->npdata + prv_ctx->nplen,
                              in, total_len - left, left);
        prv_ctx->nplen += left;
    }

__out__:
    OSAL_SAFE_FREE(local_in.ents);
    return ret;
}

int te_eia3_finup(te_eia3_ctx_t *ctx, uint64_t inbits, const uint8_t *in, uint8_t mac[4])
{
    int ret = TE_SUCCESS;
    size_t prolen = 0;
    size_t leftbits = 0;
    const uint8_t *local_in = NULL;
    sca_eia3_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == ctx->crypt) ||
        ((NULL == in) && (inbits)) || (NULL == mac)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eia3_ctx_t *)eia3_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    leftbits = inbits;
    local_in = in;

    /* Process npdata */
    if (prv_ctx->nplen > 0) {
        prolen = UTILS_MIN(ctx->crypt->blk_size - prv_ctx->nplen,
                           leftbits / BYTE_BITS);
        osal_memcpy(prv_ctx->npdata + prv_ctx->nplen,
                    local_in, prolen);
        prv_ctx->nplen += prolen;
        leftbits -= prolen * BYTE_BITS;
        local_in += prolen;
        /* Flush npdata to engine */
        if (ctx->crypt->blk_size == prv_ctx->nplen) {
            ret = te_sca_update(ctx->crypt,
                                false,
                                prv_ctx->nplen,
                                prv_ctx->npdata,
                                NULL);
            if (TE_SUCCESS != ret) {
                goto __out__;
            }
            osal_memset(prv_ctx->npdata, 0x00, prv_ctx->nplen);
            prv_ctx->nplen = 0;
        }
    }

    /* Process complete block */
    prolen = UTILS_ROUND_DOWN(leftbits / BYTE_BITS, ctx->crypt->blk_size);
    ret = te_sca_update(ctx->crypt,
                        false,
                        prolen,
                        local_in,
                        NULL);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    leftbits -= prolen * BYTE_BITS;
    local_in += prolen;

    /* Copy last incomplete bytes-aligned block to npdata */
    prolen = leftbits / BYTE_BITS;
    osal_memcpy(prv_ctx->npdata + prv_ctx->nplen,
                local_in, prolen);
    prv_ctx->nplen += prolen;
    leftbits -= prolen * BYTE_BITS;
    local_in += prolen;

    /* Padding npdata with last bytes-unaligned data */
    osal_memset(prv_ctx->npdata + prv_ctx->nplen,
                0x00, sizeof(prv_ctx->npdata) - prv_ctx->nplen);
    if (leftbits) {
        osal_memcpy(prv_ctx->npdata + prv_ctx->nplen,
                    local_in, 1);
    }
    prv_ctx->npdata[prv_ctx->nplen] |= 1 << (7 - (leftbits & 0x07));
    prv_ctx->npdata[prv_ctx->nplen] &= 256 - (1 << (7 - (leftbits & 0x07)));

    /* Update last padding data to engine */
    if ((prv_ctx->nplen > 0) || (leftbits)) {
        prolen = sizeof(prv_ctx->npdata);
    } else {
        prolen = ctx->crypt->blk_size;
    }
    ret = te_sca_update(ctx->crypt,
                        false,
                        prolen,
                        prv_ctx->npdata,
                        NULL);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    prv_ctx->nplen = 0;

    ret = te_sca_finish(ctx->crypt, mac, ctx->crypt->blk_size);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    osal_memset(prv_ctx, 0x00, sizeof(sca_eia3_ctx_t));

__out__:
    return ret;
}

int te_eia3_finish(te_eia3_ctx_t *ctx, uint8_t mac[4])
{
    int ret = TE_SUCCESS;
    size_t prolen = 0;
    sca_eia3_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == mac) ||
        (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_eia3_ctx_t *)eia3_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* pad npdata with 10... */
    osal_memset(prv_ctx->npdata + prv_ctx->nplen,
                0x00, sizeof(prv_ctx->npdata) - prv_ctx->nplen);
    prv_ctx->npdata[prv_ctx->nplen] |= 1 << 7;

    if (prv_ctx->nplen > 0) {
        prolen = sizeof(prv_ctx->npdata);
    } else {
        prolen = ctx->crypt->blk_size;
    }

    ret = te_sca_update(ctx->crypt,
                        false,
                        prolen,
                        prv_ctx->npdata,
                        NULL);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    prv_ctx->nplen = 0;

    ret = te_sca_finish(ctx->crypt, mac, ctx->crypt->blk_size);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    osal_memset(prv_ctx, 0x00, sizeof(sca_eia3_ctx_t));

__out__:
    return ret;
}

int te_eia3_clone(const te_eia3_ctx_t *src, te_eia3_ctx_t *dst)
{
    int ret = TE_SUCCESS;
    te_eia3_ctx_t nctx = {0};
    sca_eia3_ctx_t *spctx = NULL;
    sca_eia3_ctx_t *npctx = NULL;

    if ((NULL == src) || (NULL == src->crypt) ||
        (NULL == dst)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    spctx = (sca_eia3_ctx_t *)eia3_priv_ctx((te_eia3_ctx_t*)src);
    if (NULL == spctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    ret = te_sca_alloc_ctx((te_sca_drv_t *)src->crypt->drv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_eia3_ctx_t),
                           &nctx.crypt);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }

    /* clone driver ctx */
    ret = te_sca_clone(src->crypt, nctx.crypt);
    if (TE_SUCCESS != ret) {
        te_eia3_free(&nctx);
        goto __out__;
    }

    /* clone private ctx */
    npctx = (sca_eia3_ctx_t *)eia3_priv_ctx(&nctx);
    TE_ASSERT(NULL != npctx);
    osal_memcpy(npctx, spctx, sizeof(*npctx));

    if (dst->crypt != NULL) {
        (void)te_eia3_free(dst);
    }

    /* copy nctx to dst */
    osal_memcpy(dst, &nctx, sizeof(*dst));

__out__:
    return ret;
}

#endif /* TE_ZUC_C */