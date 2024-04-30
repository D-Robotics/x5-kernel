//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#if defined(TE_KASUMI_C)
#include <te_f9.h>
#include "../common/te_3gpp_utils.h"

typedef struct sca_f9_ctx {
    uint32_t dir;                              /* Direction of Transmission */
    uint8_t npdata[2*TE_KASUMI_BLOCK_SIZE];    /* Not processed data */
    uint8_t nplen;                             /* Not processed data length in byte */
} sca_f9_ctx_t;

int te_f9_init(te_f9_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg)
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *sdrv = NULL;

    if ((NULL == ctx) ||
        (NULL == hdl) ||
        (TE_MAIN_ALGO_KASUMI != malg)) {
        ret = TE_ERROR_BAD_PARAMS;
        return ret;
    }

    osal_memset(ctx, 0x00, sizeof(te_f9_ctx_t));
    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    if (NULL == sdrv) {
        ret = TE_ERROR_BAD_FORMAT;
        return ret;
    }

    /* Alloc ctx from te_sca. */
    ret = te_sca_alloc_ctx(sdrv,
                           malg,
                           sizeof(sca_f9_ctx_t),
                           &ctx->crypt);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }

    /* Init ctx->crypto values. */
    ctx->crypt->alg = TE_ALG_KASUMI_F9;

__out__:
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
    return ret;
}

int te_f9_free(te_f9_ctx_t *ctx)
{
    int ret = TE_SUCCESS;
    sca_f9_ctx_t *prv_ctx = NULL;

    if(NULL == ctx || NULL == ctx->crypt){
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
    case TE_DRV_SCA_STATE_LAST:
        ret = TE_ERROR_BAD_STATE;
        goto __out__;
    case TE_DRV_SCA_STATE_READY:
    case TE_DRV_SCA_STATE_INIT:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
        /**
         * State compensation
         * If it fails in te_sca_finish, only log prompt,
         * need to continue free up ctx and reset ret value.
         */
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        if (TE_SUCCESS != 0) {
            OSAL_LOG_ERR("te_sca_finish raised exceptions on te_f9_free!");
        }
        break;
    }

    /* Free te_f9_ctx */
    prv_ctx = (sca_f9_ctx_t *)f9_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    osal_memset(prv_ctx, 0x00, sizeof(sca_f9_ctx_t));
    ret = te_sca_free_ctx(ctx->crypt);
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("te_sca_free_ctx raised exceptions!\n");
    }
    ctx->crypt = NULL;

__out__:
    return ret;
}

int te_f9_setkey(te_f9_ctx_t * ctx, const uint8_t *key, uint32_t keybits)
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
    case TE_DRV_SCA_STATE_LAST:
        ret = TE_ERROR_BAD_STATE;
        goto __out__;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
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
    return ret;
}

int te_f9_setseckey(te_f9_ctx_t *ctx, te_sec_key_t *key)
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
    case TE_DRV_SCA_STATE_LAST:
        ret = TE_ERROR_BAD_STATE;
        goto __out__;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        break;
    }

    /* Set sec key to sca driver */
    key_desc.type = TE_KEY_TYPE_SEC;
    osal_memcpy(&key_desc.sec, key, sizeof(te_sec_key_t));
    ret = te_sca_setkey(ctx->crypt, &key_desc);

__out__:
    return ret;
}

int te_f9_start(te_f9_ctx_t * ctx, uint32_t count, uint32_t fresh, uint32_t dir)
{
    int ret = TE_SUCCESS;
    uint8_t i = 0;
    sca_f9_ctx_t *prv_ctx = NULL;

    if (NULL == ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* Check significant bit number of dir is 1. */
    if (dir > 0x01U) {
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
    case TE_DRV_SCA_STATE_LAST:
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
        ret = TE_ERROR_BAD_STATE;
        goto __out__;
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
        ret = te_sca_finish(ctx->crypt, NULL, 0);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        break;
    }

    /*
     * There is different from the manual of f9
     * is defined in 3GPP TS 35.201 v16.0.0.
     * The initial value(IV) is not set in the
     * te_sca_start, but is put into npdata and
     * processed by te_sca_update.
     * */
    ret = te_sca_start(ctx->crypt, 0, NULL, 0);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }

    prv_ctx = (sca_f9_ctx_t *)f9_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }
    /* Init with prv_ctx values. */
    osal_memset(prv_ctx, 0, sizeof(*prv_ctx));
    for(i = 0; i < 4; ++i) {
        prv_ctx->npdata[i] = (uint8_t)(count >> (24 - (i*8))) & 0xFF;
        prv_ctx->npdata[i + 4] = (uint8_t)(fresh >> (24 - (i*8))) & 0xFF;
    }
    prv_ctx->nplen = ctx->crypt->blk_size;
    prv_ctx->dir = dir;

    /* Call te_sca_update to update IV. */
    ret = te_sca_update(ctx->crypt,
                        false,
                        prv_ctx->nplen,
                        prv_ctx->npdata,
                        NULL);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    prv_ctx->nplen = 0;
    osal_memset(prv_ctx->npdata, 0x00, sizeof(prv_ctx->npdata));
__out__:
    return ret;
}

int te_f9_update(te_f9_ctx_t *ctx, size_t inlen, const uint8_t *in)
{
    int ret = TE_SUCCESS;
    sca_f9_ctx_t *prv_ctx = NULL;
    size_t left = inlen;
    size_t prolen = 0;
    size_t ofs = 0;

    if((NULL == ctx) || (NULL == ctx->crypt) ||
       (inlen && (NULL == in))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    /* inlen is 0, do nothing */
    if (0 == inlen) {
        goto __out__;
    }

    prv_ctx = (sca_f9_ctx_t *)f9_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    /* Process npdata. */
    if (prv_ctx->nplen > 0) {
        prolen = UTILS_MIN(left, ctx->crypt->blk_size - prv_ctx->nplen);
        osal_memcpy(prv_ctx->npdata + prv_ctx->nplen, in, prolen);
        prv_ctx->nplen += prolen;
        left -= prolen;
        if (ctx->crypt->blk_size == prv_ctx->nplen) {
            ret = te_sca_update(ctx->crypt,
                                false,
                                prv_ctx->nplen,
                                prv_ctx->npdata,
                                NULL);
            if (TE_SUCCESS != ret) {
                goto __out__;
            }
            osal_memset(prv_ctx->npdata, 0x00, sizeof(prv_ctx->npdata));
            prv_ctx->nplen = 0;
        }
    }

    /* Process complete blocks */
    prolen = UTILS_ROUND_DOWN(left, ctx->crypt->blk_size);
    ofs = inlen - left;
    ret = te_sca_update(ctx->crypt,
                        false,
                        prolen,
                        in + ofs,
                        NULL);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    left -= prolen;

    /* Process incomplete block */
    if (left > 0) {
        osal_memcpy(prv_ctx->npdata, in + inlen - left, left);
        prv_ctx->nplen = left;
    }

__out__:
    return ret;
}

int te_f9_finish(te_f9_ctx_t *ctx, uint8_t *mac)
{
    int ret = TE_SUCCESS;
    sca_f9_ctx_t *prv_ctx = NULL;

    if ((NULL == ctx) || (NULL == mac) ||
        (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_f9_ctx_t *)f9_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
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
    case TE_DRV_SCA_STATE_READY:
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_LAST:
        ret = TE_ERROR_BAD_STATE;
        goto __out__;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
        break;
    }

    /* Padding dir | 1 | 0* to 64bits aligned. */
    osal_memset(prv_ctx->npdata + prv_ctx->nplen, 0x00,
                sizeof(prv_ctx->npdata) - prv_ctx->nplen);
    prv_ctx->npdata[prv_ctx->nplen] = ((prv_ctx->dir & 0x01U) << 1 | 1) << 6;
    prv_ctx->nplen = ctx->crypt->blk_size;
    ret = te_sca_update(ctx->crypt,
                        false,
                        prv_ctx->nplen,
                        prv_ctx->npdata,
                        NULL);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
    ret = te_sca_finish(ctx->crypt, mac, 4);
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("te_sca_finish raised exceptions!\n");
    }

__out__:
    return ret;
}

int te_f9_uplist(te_f9_ctx_t *ctx, const te_memlist_t *in)
{
    int ret = TE_SUCCESS;
    sca_f9_ctx_t *prv_ctx = NULL;
    size_t left = 0;
    size_t prolen = 0;
    size_t inlen = 0;
    te_memlist_t rebuild_in = {0};

    if ((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    if (NULL == in) {
        goto __out__;
    }

    if ((in->nent > 0) && (NULL == in->ents)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_f9_ctx_t *)f9_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    left = inlen = te_memlist_get_total_len((te_memlist_t *)in);
    /* avoid left overflow. */
    if (inlen > prv_ctx->nplen + left) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    } else if (0 == inlen) {
        goto __out__;
    }

    /* Handle complete blocks to update. */
    if (left >= ctx->crypt->blk_size - prv_ctx->nplen) {
        prolen = UTILS_ROUND_DOWN(left + prv_ctx->nplen, ctx->crypt->blk_size);
        rebuild_in.nent = in->nent + 1;
        rebuild_in.ents = (te_mement_t *)osal_calloc(sizeof(*rebuild_in.ents),
                                                     rebuild_in.nent);
        if (NULL == rebuild_in.ents) {
            ret = TE_ERROR_OOM;
            goto __out__;
        }
        if (prv_ctx->nplen > 0) {
            rebuild_in.ents ++;
            rebuild_in.nent --;
        }
        te_memlist_dup(&rebuild_in, in, 0, prolen - prv_ctx->nplen);
        if (prv_ctx->nplen > 0) {
            rebuild_in.ents --;
            rebuild_in.nent ++;
            rebuild_in.ents[0].buf = prv_ctx->npdata;
            rebuild_in.ents[0].len = prv_ctx->nplen;
        }
        ret = te_sca_uplist(ctx->crypt, false, &rebuild_in, NULL);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        left -= (prolen - prv_ctx->nplen);
        prv_ctx->nplen = 0;
    }

    /* insufficient data to update, cached left of in npdata */
    if (left > 0) {
        (void)te_memlist_copy(prv_ctx->npdata + prv_ctx->nplen,
                              in, inlen - left, left);
        prv_ctx->nplen += left;
    }

__out__:
    OSAL_SAFE_FREE(rebuild_in.ents);
    return ret;
}

int te_f9_finup(te_f9_ctx_t *ctx, uint64_t inbits, const uint8_t *in, uint8_t mac[4])
{
    int ret = TE_SUCCESS;
    sca_f9_ctx_t *prv_ctx = NULL;
    uint64_t leftbits = inbits;
    size_t prolen = 0;
    size_t ofs = 0;
    size_t byteofs = 0;
    size_t bitofs = 0;

    if((NULL == ctx) || (NULL == ctx->crypt) ||
       (inbits && (NULL == in)) || (NULL == mac)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    prv_ctx = (sca_f9_ctx_t *)f9_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto __out__;
    }

    /* Consume npdata. */
    if (prv_ctx->nplen > 0) {
        /* process npdata */
        prolen = UTILS_MIN(FLOOR_BYTES(leftbits),
                           ctx->crypt->blk_size - prv_ctx->nplen);
        osal_memcpy(prv_ctx->npdata + prv_ctx->nplen, in, prolen);
        prv_ctx->nplen += prolen;
        leftbits -= prolen * BYTE_BITS;

        if (prv_ctx->nplen == ctx->crypt->blk_size) {
            ret = te_sca_update(ctx->crypt,
                                false,
                                prv_ctx->nplen,
                                prv_ctx->npdata,
                                NULL);
             if (TE_SUCCESS != ret) {
                goto __out__;
            }
            osal_memset(prv_ctx->npdata, 0x00, sizeof(prv_ctx->npdata));
            prv_ctx->nplen = 0;
        }
    }
    /* Process complete blocks */
    prolen = UTILS_ROUND_DOWN(FLOOR_BYTES(leftbits),
                              ctx->crypt->blk_size);
    if (prolen > 0) {
        ofs = FLOOR_BYTES(inbits - leftbits);
        ret = te_sca_update(ctx->crypt,
                            false,
                            prolen,
                            in + ofs,
                            NULL);
        if (TE_SUCCESS != ret) {
            goto __out__;
        }
        leftbits -= prolen * BYTE_BITS;
    }
    /*
     * Padding (dir | 1 | 0*)
     *
     * Two conditions:
     *
     * i .  nplen < 7
     *      update one blk_size data.
     *
     * ii.  leftbis = 7, nplen = 7,
     *      npdata[7] -> 0000 0000B
     *                           ^
     *                          dir
     *      which need to pad 1 on npdata[8]
     *      and update 2*blk_size data.
     * */
    prolen = CEIL_BYTES(leftbits);
    ofs = CEIL_BYTES(inbits) - prolen;
    byteofs = prv_ctx->nplen;
    bitofs = inbits % BYTE_BITS;
    osal_memset(prv_ctx->npdata + byteofs, 0x00,
                sizeof(prv_ctx->npdata) - byteofs);
    osal_memcpy(prv_ctx->npdata + byteofs, in + ofs, prolen);

    /* Padding dir */
    byteofs += FLOOR_BYTES(leftbits);
    prv_ctx->npdata[byteofs] = WIPE_TAIL_BITS(prv_ctx->npdata[byteofs], bitofs);
    prv_ctx->npdata[byteofs] |= (prv_ctx->dir & 0x01U) << (7 - bitofs);

    /* Padding 1 */
    byteofs += ((bitofs + 1) / BYTE_BITS);
    bitofs = (bitofs + 1) % BYTE_BITS;
    prv_ctx->npdata[byteofs] |= 1 << (7 - bitofs);
    if (byteofs >= ctx->crypt->blk_size) {
        prv_ctx->nplen = 2 * ctx->crypt->blk_size;
    } else {
        prv_ctx->nplen = ctx->crypt->blk_size;
    }

    /* Update tail padding data. */
    ret = te_sca_update(ctx->crypt,
                        false,
                        prv_ctx->nplen,
                        prv_ctx->npdata,
                        NULL);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }

    /* Finish and deal with mac array. */
    ret = te_sca_finish(ctx->crypt, mac, 4);
    if (TE_SUCCESS != ret) {
        goto __out__;
    }
__out__:
    return ret;
}

int te_f9_clone(const te_f9_ctx_t *src, te_f9_ctx_t *dst)
{
    int ret = TE_SUCCESS;
    te_f9_ctx_t swap_ctx = {0};
    sca_f9_ctx_t *prv_src = NULL;
    sca_f9_ctx_t *prv_swap = NULL;

    /** sanity check params */
    if ((NULL == src) || (NULL == src->crypt) || (NULL == dst)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }
    osal_memset(&swap_ctx, 0x00, sizeof(swap_ctx));
    ret = te_sca_alloc_ctx((te_sca_drv_t *)src->crypt->drv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_f9_ctx_t),
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
    prv_src = (sca_f9_ctx_t *)f9_priv_ctx(src);
    prv_swap = (sca_f9_ctx_t *)f9_priv_ctx(&swap_ctx);
    osal_memcpy(prv_swap, prv_src, sizeof(*prv_src));
    /** replace dst context */
    if (NULL != dst->crypt) {
        /**
         *  Ignore the error return from te_f9_free, because once caller
         *  provide an uninitialized raw context from stack it may hold a
         *  pointer other than NULL which may treat te_f9_free to return
         *  error codes such as TE_ERROR_BAD_STATE.
         */
        (void)te_f9_free(dst);
    }
    osal_memcpy(dst, &swap_ctx, sizeof(*dst));
    goto __out__;
__err__:
    te_sca_free_ctx(swap_ctx.crypt);
__out__:
    return ret;
}
#endif  /* defined(TE_KASUMI_C) */