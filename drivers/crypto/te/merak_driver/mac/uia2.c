//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#if defined(TE_SNOW3G_C)
#include <te_uia2.h>
#include <te_uea2.h>
#include <te_gfmul.h>
#include "../common/te_3gpp_utils.h"

#define SCA_EVAL_BLOCK_SIZE     (8U)

/**
 *  State of UIA2 context
 */
typedef enum {
    UIA2_STATE_RAW,
    UIA2_STATE_INIT,
    UIA2_STATE_READY,
    UIA2_STATE_START,
    UIA2_STATE_UPDATE
} sca_uia2_state_t;

/**
 *  UIA2 private context
 */
typedef struct {
    te_gfmul_ctx_t gctx;                 /**< GFMUL context */
    uint8_t iv[16];                      /**< initial value presents as uint8 */
    uint8_t z[20];                       /**< key stream z1 ~ z5 */
    sca_uia2_state_t state;              /**< state */
    uint8_t npdata[SCA_EVAL_BLOCK_SIZE]; /**< not process data 8 bytes(64bits) */
    uint8_t nplen;                       /**< not process data length */
    uint64_t mblen;                      /**< message length in bit */
} sca_uia2_ctx_t;

int te_uia2_init(te_uia2_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg)
{
    int ret = TE_SUCCESS;
    sca_uia2_ctx_t *prv_ctx = NULL;
    te_sca_drv_t *sdrv = NULL;
    /** sanity check params  */
    if ((NULL == ctx) || (NULL == hdl) || (malg != TE_MAIN_ALGO_SNOW3G)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    sdrv = (te_sca_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_SCA);
    ret = te_sca_alloc_ctx(sdrv, malg, sizeof(sca_uia2_ctx_t), &ctx->crypt);
    if (ret != TE_SUCCESS) {
        goto err_alloc;
    }
    ctx->crypt->alg = TE_ALG_SNOW3G_F8;
    /** private context initialization */
    prv_ctx = (sca_uia2_ctx_t *)uia2_priv_ctx(ctx);
    ret = te_gfmul_init(&prv_ctx->gctx, hdl);
    if (ret != TE_SUCCESS) {
        goto err_gfmul_init;
    }
    prv_ctx->state = UIA2_STATE_INIT;
    goto fin;

err_gfmul_init:
    te_sca_free_ctx(ctx->crypt);
fin:
err_alloc:
    te_drv_put(hdl, TE_DRV_TYPE_SCA);
err_params:
    return ret;
}

int te_uia2_free(te_uia2_ctx_t *ctx)
{
    int ret = TE_SUCCESS;
    sca_uia2_ctx_t *prv_ctx = NULL;
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    prv_ctx = (sca_uia2_ctx_t *)uia2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    /** sanity check state do compensation if need */
    switch (prv_ctx->state) {
    default:
    case UIA2_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        break;
    case UIA2_STATE_INIT:
    case UIA2_STATE_READY:
    case UIA2_STATE_START:
        ret = TE_SUCCESS;
        break;
    case UIA2_STATE_UPDATE:
        ret = te_gfmul_finish(&prv_ctx->gctx, NULL, 0);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_WARN("te_gfmul_finish raises exceptions(%X)\n", ret);
            ret = TE_SUCCESS; /** ignore this error and let it can free resources */
        }
        break;
    }
    if (ret != TE_SUCCESS) {
        goto err_state;
    }
    /** free resources */
    ret = te_gfmul_free(&prv_ctx->gctx);
    /** zeroize private data, anti-leakage */
    osal_memset(prv_ctx, 0x00, sizeof(*prv_ctx));
    if (ret != TE_SUCCESS) {
        te_sca_free_ctx(ctx->crypt);
    } else {
        ret = te_sca_free_ctx(ctx->crypt);
    }

err_state:
err_params:
    return ret;
}

int te_uia2_setkey(te_uia2_ctx_t *ctx, const uint8_t *key, uint32_t keybits)
{
    int ret = TE_SUCCESS;
    sca_uia2_ctx_t *prv_ctx = NULL;
    te_sca_key_t key_desc = {0};
    uint8_t k[MAX_EK3_SIZE] = {0};
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt) || (NULL == key)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    prv_ctx = (sca_uia2_ctx_t *)uia2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    /** sanity check state do compensation if need */
    switch (prv_ctx->state) {
    default:
    case UIA2_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        break;
    case UIA2_STATE_INIT:
    case UIA2_STATE_READY:
    case UIA2_STATE_START:
        ret = TE_SUCCESS;
        break;
    case UIA2_STATE_UPDATE:
        ret = te_gfmul_finish(&prv_ctx->gctx, NULL, 0);
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
    key_desc.type = TE_KEY_TYPE_USER;
    key_desc.user.key = k;
    key_desc.user.keybits = keybits;
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    osal_memset(k, 0x00, sizeof(k));
    if (ret != TE_SUCCESS) {
        goto err_setkey;
    }
    prv_ctx->state = UIA2_STATE_READY;

err_key:
err_setkey:
err_state:
err_params:
    return ret;
}

int te_uia2_setseckey(te_uia2_ctx_t *ctx, const te_sec_key_t *key)
{
    int ret = TE_SUCCESS;
    sca_uia2_ctx_t *prv_ctx = NULL;
    te_sca_key_t key_desc = {0};
    /** sanity check params */
    if ((NULL == ctx) || (NULL == key) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    prv_ctx = (sca_uia2_ctx_t *)uia2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    /** sanity check state do compensation if need */
    switch (prv_ctx->state) {
    default:
    case UIA2_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        break;
    case UIA2_STATE_INIT:
    case UIA2_STATE_READY:
    case UIA2_STATE_START:
        ret = TE_SUCCESS;
        break;
    case UIA2_STATE_UPDATE:
        ret = te_gfmul_finish(&prv_ctx->gctx, NULL, 0);
        break;
    }
    if (ret != TE_SUCCESS) {
        goto err_state;
    }
    key_desc.type = TE_KEY_TYPE_SEC;
    osal_memcpy(&key_desc.sec, key, sizeof(*key));
    ret = te_sca_setkey(ctx->crypt, &key_desc);
    if (ret != TE_SUCCESS) {
        goto err_setkey;
    }
    prv_ctx->state = UIA2_STATE_READY;

err_setkey:
err_state:
err_params:
    return ret;
}

static int uia2_gen_ks_z(te_uia2_ctx_t *ctx)
{
    int ret = TE_SUCCESS;
    sca_uia2_ctx_t *prv_ctx = NULL;

    TE_ASSERT(ctx != NULL);
    TE_ASSERT(ctx->crypt != NULL);
    prv_ctx = uia2_priv_ctx(ctx);
    TE_ASSERT(prv_ctx != NULL);

    ret = te_sca_start(ctx->crypt, TE_DRV_SCA_ENCRYPT,
                       prv_ctx->iv, sizeof(prv_ctx->iv));
    if (ret != TE_SUCCESS) {
        goto err_start;
    }
    osal_memset(prv_ctx->z, 0x00, sizeof(prv_ctx->z));
    ret = te_sca_update(ctx->crypt, true, sizeof(prv_ctx->z),
                            prv_ctx->z, prv_ctx->z);
    if (ret != TE_SUCCESS) {
        goto err_update;
    }
    ret = te_sca_finish(ctx->crypt, NULL, 0);
    if (ret != TE_SUCCESS) {
        goto err_fin;
    }
    goto fin;
err_update:
    (void)te_sca_finish(ctx->crypt, NULL, 0);
err_fin:
err_start:
fin:
    return ret;
}

int te_uia2_start(te_uia2_ctx_t *ctx, uint32_t count, uint32_t fresh, uint32_t dir)
{
    int ret = TE_SUCCESS;
    sca_uia2_ctx_t *prv_ctx = NULL;
    uint32_t tmp_iv = 0;
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt) || (dir > 1)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    prv_ctx = (sca_uia2_ctx_t *)uia2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    /** sanity check state do compensation if need */
    switch (prv_ctx->state) {
    default:
    case UIA2_STATE_RAW:
    case UIA2_STATE_INIT:
        ret = TE_ERROR_BAD_STATE;
        break;
    case UIA2_STATE_READY:
    case UIA2_STATE_START:
        ret = TE_SUCCESS;
        break;
    case UIA2_STATE_UPDATE:
        /** clean previous GFMUL session */
        ret = te_gfmul_finish(&prv_ctx->gctx, NULL, 0);
        break;
    }
    if (ret != TE_SUCCESS) {
        goto err_state;
    }
    /** build iv */
    tmp_iv = HTOBE32(fresh ^ ((dir && 0x01) << 15));
    osal_memcpy(prv_ctx->iv, &tmp_iv, sizeof(tmp_iv));
    tmp_iv = HTOBE32(count ^ ((dir && 0x01) << 31));
    osal_memcpy(prv_ctx->iv + 4, &tmp_iv, sizeof(tmp_iv));
    tmp_iv = HTOBE32(fresh);
    osal_memcpy(prv_ctx->iv + 8, &tmp_iv, sizeof(tmp_iv));
    tmp_iv = HTOBE32(count);
    osal_memcpy(prv_ctx->iv + 12, &tmp_iv, sizeof(tmp_iv));
    /** generate key stream z1 ~ z5 */
    ret = uia2_gen_ks_z(ctx);
    if (ret != TE_SUCCESS) {
        goto err_genz;
    }
    prv_ctx->mblen = 0;
    prv_ctx->nplen = 0;
    prv_ctx->state = UIA2_STATE_START;

err_genz:
err_state:
err_params:
    return ret;
}

static inline int uia2_start_gfmul_session(sca_uia2_ctx_t *ctx)
{
    int ret = TE_SUCCESS;

    ret = te_gfmul_setkey(&ctx->gctx, ctx->z, SCA_EVAL_BLOCK_SIZE * 8);
    if (TE_SUCCESS == ret) {
        osal_memset(ctx->iv, 0x00, SCA_EVAL_BLOCK_SIZE);
        ret = te_gfmul_start(&ctx->gctx, ctx->iv);
    }

    return ret;
}

int te_uia2_update(te_uia2_ctx_t *ctx,
                   size_t inlen,
                   const uint8_t *in)
{
    int ret = TE_SUCCESS;
    sca_uia2_ctx_t *prv_ctx = NULL;
    size_t proclen = 0;
    size_t left = inlen;
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt) ||
        ((inlen != 0) && (NULL == in))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    prv_ctx = (sca_uia2_ctx_t *)uia2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    /** sanity check state do compensation if need */
    switch (prv_ctx->state) {
    default:
    case UIA2_STATE_RAW:
    case UIA2_STATE_INIT:
    case UIA2_STATE_READY:
        ret = TE_ERROR_BAD_STATE;
        break;
    case UIA2_STATE_START:
        /** should start a gfmul session */
        ret = uia2_start_gfmul_session(prv_ctx);
        break;
    case UIA2_STATE_UPDATE:
        break;
    }
    if (ret != TE_SUCCESS) {
        goto err_state;
    }
    if (0 == inlen) {
        prv_ctx->state = UIA2_STATE_UPDATE;
        goto fin;
    }
    /** handle not process data if any */
    if (prv_ctx->nplen > 0) {
        proclen = UTILS_MIN(SCA_EVAL_BLOCK_SIZE - prv_ctx->nplen, left);
        osal_memcpy(prv_ctx->npdata + prv_ctx->nplen, in, proclen);
        prv_ctx->nplen += proclen;
        left -= proclen;
        if (SCA_EVAL_BLOCK_SIZE == prv_ctx->nplen) {
            ret = te_gfmul_update(&prv_ctx->gctx,
                                  prv_ctx->nplen, prv_ctx->npdata);
            if (ret != TE_SUCCESS) {
                prv_ctx->nplen -= proclen;
                goto err_update;
            } else {
                prv_ctx->nplen = 0;
            }
        }
    }
    /** handle complete blocks */
    proclen = UTILS_ROUND_DOWN(left, SCA_EVAL_BLOCK_SIZE);
    if (proclen > 0) {
        ret = te_gfmul_update(&prv_ctx->gctx, proclen,
                            in + inlen - left);
        if (ret != TE_SUCCESS) {
            goto err_update;
        }
        left -= proclen;
    }
    /** cache remainder to npdata if any */
    if (left > 0) {
        /** if it goes to here nplen must be \c 0 */
        osal_memcpy(prv_ctx->npdata, in + inlen - left, left);
        prv_ctx->nplen = left;
    }
    prv_ctx->state = UIA2_STATE_UPDATE;
    prv_ctx->mblen += inlen * 8;
    goto fin;

err_update:
    /** clean session */
    if (UIA2_STATE_START == prv_ctx->state) {
        (void)te_gfmul_finish(&prv_ctx->gctx, NULL, 0);
    }
err_state:
err_params:
fin:
    return ret;
}

int te_uia2_uplist(te_uia2_ctx_t *ctx,
                   const te_memlist_t *in)
{
    int ret = TE_SUCCESS;
    sca_uia2_ctx_t *prv_ctx = NULL;
    size_t proclen = 0;
    size_t left = 0;
    size_t inlen = 0;
    te_memlist_t rebuild_in = {0};
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    prv_ctx = (sca_uia2_ctx_t *)uia2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    /** sanity check state do compensation if need */
    switch (prv_ctx->state) {
    default:
    case UIA2_STATE_RAW:
    case UIA2_STATE_INIT:
    case UIA2_STATE_READY:
        ret = TE_ERROR_BAD_STATE;
        break;
    case UIA2_STATE_START:
        /** should start a gfmul session */
        ret = uia2_start_gfmul_session(prv_ctx);
        if (ret != TE_SUCCESS) {
            OSAL_LOG_ERR("uia2_start_gfmul_session raise exceptions(%X)\n",
                            ret);
        }
        break;
    case UIA2_STATE_UPDATE:
        break;
    }
    if (ret != TE_SUCCESS) {
        goto err_state;
    }
    /** if in is NULL do nothing, return */
    if (NULL == in) {
        prv_ctx->state = UIA2_STATE_UPDATE;
        goto fin;
    }
    if ((in->nent > 0) && (NULL == in->ents)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    left = inlen = te_memlist_get_total_len((te_memlist_t *)in);
    /** overflow check */
    if (inlen > prv_ctx->nplen + inlen) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    /** if inlen is \c 0 do nothing, return */
    if (0 == inlen) {
        prv_ctx->state = UIA2_STATE_UPDATE;
        goto fin;
    }
    /** sufficient data to perform an update operation, handle complete blocks */
    if ((prv_ctx->nplen + left) >= SCA_EVAL_BLOCK_SIZE) {
        proclen = UTILS_ROUND_DOWN(left + prv_ctx->nplen,
                                   SCA_EVAL_BLOCK_SIZE);
        /** malloc sufficient room to rebuild in list */
        rebuild_in.nent = in->nent + 1;
        rebuild_in.ents = (te_mement_t *)osal_calloc(sizeof(*rebuild_in.ents),
                                rebuild_in.nent);
        if (NULL == rebuild_in.ents) {
            ret = TE_ERROR_OOM;
            goto err_alloc;
        }
        /** make room for npdata and load in data */
        if (prv_ctx->nplen > 0) {
            rebuild_in.ents++;
            rebuild_in.nent--;
        }
        (void)te_memlist_dup(&rebuild_in, in, 0, proclen - prv_ctx->nplen);
        /** load npdata into first node */
        if (prv_ctx->nplen > 0) {
            rebuild_in.ents--;
            rebuild_in.nent++;
            rebuild_in.ents[0].buf = prv_ctx->npdata;
            rebuild_in.ents[0].len = prv_ctx->nplen;
        }
        ret = te_gfmul_uplist(&prv_ctx->gctx, &rebuild_in);
        if (ret != TE_SUCCESS) {
            goto err_update;
        }
        left -= (proclen - prv_ctx->nplen);
        prv_ctx->nplen = 0;
    }
    /** insufficient data to perform an update, then just cache left-in to npdata */
    if (left > 0) {
        (void)te_memlist_copy(prv_ctx->npdata + prv_ctx->nplen,
                     in, inlen - left, left);
        prv_ctx->nplen += left;
    }
    prv_ctx->state = UIA2_STATE_UPDATE;
    prv_ctx->mblen += inlen * 8;

err_update:
    OSAL_SAFE_FREE(rebuild_in.ents);
err_alloc:
    if (UIA2_STATE_START == prv_ctx->state) {
        (void)te_gfmul_finish(&prv_ctx->gctx, NULL, 0);
    }
err_state:
err_params:
fin:
    return ret;
}

int te_uia2_finup(te_uia2_ctx_t *ctx, uint64_t inbits,
                  const uint8_t *in, uint8_t mac[4])
{
    int ret = TE_SUCCESS;
    sca_uia2_ctx_t *prv_ctx = NULL;
    uint8_t eval[SCA_EVAL_BLOCK_SIZE] = {0};
    uint64_t leftbits = inbits;
    size_t proclen = 0;
    uint32_t tmp = 0;
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt) || (NULL == mac) ||
        ((inbits != 0) && (NULL == in))) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }

    prv_ctx = (sca_uia2_ctx_t *)uia2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    /** sanity check state do compensation if need */
    switch (prv_ctx->state) {
    default:
    case UIA2_STATE_RAW:
    case UIA2_STATE_INIT:
    case UIA2_STATE_READY:
        ret = TE_ERROR_BAD_STATE;
        break;
    case UIA2_STATE_START:
        /** if in exist then start a gfmul session otherwise don't*/
        if (inbits > 0) {
            ret = uia2_start_gfmul_session(prv_ctx);
        }
        break;
    case UIA2_STATE_UPDATE:
        break;
    }
    if (ret != TE_SUCCESS) {
        goto err_state;
    }
    /** full fill npdata first if any*/
    if (prv_ctx->nplen > 0) {
        proclen = UTILS_MIN(SCA_EVAL_BLOCK_SIZE - prv_ctx->nplen,
                            FLOOR_BYTES(leftbits));
        osal_memcpy(prv_ctx->npdata + prv_ctx->nplen, in, proclen);
        prv_ctx->nplen += proclen;
        leftbits -= proclen * BYTE_BITS;
        if (SCA_EVAL_BLOCK_SIZE == prv_ctx->nplen) {
            ret = te_gfmul_update(&prv_ctx->gctx, sizeof(prv_ctx->npdata),
                                    prv_ctx->npdata);
            if (ret != TE_SUCCESS) {
                goto err_upnpd;
            }
            prv_ctx->nplen = 0;
        }
    }
    /** handle complete blocks if any */
    proclen = UTILS_ROUND_DOWN(FLOOR_BYTES(leftbits),
                               SCA_EVAL_BLOCK_SIZE);
    if (proclen > 0) {
        ret = te_gfmul_update(&prv_ctx->gctx,
                              proclen,
                              in + FLOOR_BYTES(inbits - leftbits));
        if (ret != TE_SUCCESS) {
            goto err_upcmpl;
        } else {
            leftbits -= proclen * BYTE_BITS;
        }
    }
    /** handle incomplete blocks D-2 */
    if (leftbits > 0) {
        proclen = CEIL_BYTES(leftbits);
        osal_memcpy(prv_ctx->npdata + prv_ctx->nplen,
                    in + CEIL_BYTES(inbits) - proclen,
                    proclen);
        prv_ctx->nplen += proclen;
    }
    if (prv_ctx->nplen > 0) {
        /** wipe last bits in case of not byte-algined */
        if ((inbits % BYTE_BITS) > 0) {
            prv_ctx->npdata[prv_ctx->nplen - 1] =
                WIPE_TAIL_BITS(prv_ctx->npdata[prv_ctx->nplen - 1],
                           inbits % BYTE_BITS);
        }
        /** pad 0 to block-algined */
        osal_memset(prv_ctx->npdata + prv_ctx->nplen, 0x00,
                    SCA_EVAL_BLOCK_SIZE - prv_ctx->nplen);
        ret = te_gfmul_update(&prv_ctx->gctx, SCA_EVAL_BLOCK_SIZE,
                                prv_ctx->npdata);
        if (ret != TE_SUCCESS) {
            goto err_uprmd;
        }
    }
    prv_ctx->mblen += inbits;
    /** finish previous gfmul session to get eval */
    if (prv_ctx->mblen > 0) {
        ret = te_gfmul_finish(&prv_ctx->gctx, eval, sizeof(eval));
    }
    /** EVAL = EVAL XOR mblen */
    tmp = prv_ctx->mblen >> 32;
    tmp = HTOBE32(tmp);
    osal_memcpy(prv_ctx->npdata, &tmp, sizeof(tmp));
    tmp = prv_ctx->mblen & 0xFFFFFFFFU;
    tmp = HTOBE32(tmp);
    osal_memcpy(prv_ctx->npdata + 4, &tmp, sizeof(tmp));
    BS_XOR(prv_ctx->npdata, eval, prv_ctx->npdata, sizeof(eval));
    /** MUL EVAL by Q to produce MAC-I */
    ret = te_gfmul_setkey(&prv_ctx->gctx, prv_ctx->z + 8,
                          SCA_EVAL_BLOCK_SIZE * BYTE_BITS);
    if (ret != TE_SUCCESS) {
        goto err_mulq;
    }
    osal_memset(prv_ctx->iv, 0x00, sizeof(prv_ctx->iv));
    ret = te_gfmul_start(&prv_ctx->gctx, prv_ctx->iv);
    if (ret != TE_SUCCESS) {
        goto err_mulq;
    }

    ret = te_gfmul_update(&prv_ctx->gctx, sizeof(prv_ctx->npdata),
                          prv_ctx->npdata);
    if (ret != TE_SUCCESS) {
        goto err_mulq;
    }
    ret = te_gfmul_finish(&prv_ctx->gctx, eval, sizeof(eval));
    if (ret != TE_SUCCESS) {
        goto err_gfin;
    }
    BS_XOR(mac, eval, prv_ctx->z + 16, 4);
    prv_ctx->state = UIA2_STATE_READY;

err_gfin:
err_mulq:
err_uprmd:
err_upcmpl:
err_upnpd:
    /** rollback gfmul context */
    if (UIA2_STATE_START == prv_ctx->state) {
        te_gfmul_finish(&prv_ctx->gctx, NULL, 0);
    }
err_state:
err_params:
    return ret;
}

int te_uia2_finish(te_uia2_ctx_t *ctx, uint8_t mac[4])
{
    int ret = TE_SUCCESS;
    sca_uia2_ctx_t *prv_ctx = NULL;
    uint8_t eval[SCA_EVAL_BLOCK_SIZE] = {0};
    uint32_t tmp = 0;
    /** sanity check params */
    if ((NULL == ctx) || (NULL == ctx->crypt) || (NULL == mac)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    prv_ctx = (sca_uia2_ctx_t *)uia2_priv_ctx(ctx);
    if (NULL == prv_ctx) {
        ret = TE_ERROR_BAD_FORMAT;
        goto err_params;
    }
    /** sanity check state do compensation if need */
    switch (prv_ctx->state) {
    default:
    case UIA2_STATE_RAW:
    case UIA2_STATE_INIT:
    case UIA2_STATE_READY:
        ret = TE_ERROR_BAD_STATE;
        break;
    case UIA2_STATE_START:
    case UIA2_STATE_UPDATE:
        break;
    }
    if (ret != TE_SUCCESS) {
        goto err_state;
    }
    /** feed incomplete block to engine if has */
    if (prv_ctx->nplen > 0) {
        osal_memset(prv_ctx->npdata + prv_ctx->nplen, 0x00,
                    SCA_EVAL_BLOCK_SIZE - prv_ctx->nplen);
        ret = te_gfmul_update(&prv_ctx->gctx, sizeof(prv_ctx->npdata),
                                prv_ctx->npdata);
        prv_ctx->nplen = 0;
    }
    /** finish previous gfmul session to get eval */
    osal_memset(eval, 0x00, sizeof(eval));
    if (prv_ctx->mblen > 0) {
        ret = te_gfmul_finish(&prv_ctx->gctx, eval, sizeof(eval));
    }
    /** EVAL = EVAL XOR mblen */
    tmp = prv_ctx->mblen >> 32;
    tmp = HTOBE32(tmp);
    osal_memcpy(prv_ctx->npdata, &tmp, sizeof(tmp));
    tmp = prv_ctx->mblen & 0xFFFFFFFFU;
    tmp = HTOBE32(tmp);
    osal_memcpy(prv_ctx->npdata + 4, &tmp, sizeof(tmp));
    BS_XOR(prv_ctx->npdata, eval, prv_ctx->npdata, sizeof(eval));
    /** MUL EVAL by Q to produce MAC-I */
    ret = te_gfmul_setkey(&prv_ctx->gctx, prv_ctx->z + 8,
                          SCA_EVAL_BLOCK_SIZE * 8);
    if (ret != TE_SUCCESS) {
        goto err_mulq;
    }
    osal_memset(prv_ctx->iv, 0x00, sizeof(prv_ctx->iv));
    ret = te_gfmul_start(&prv_ctx->gctx, prv_ctx->iv);
    if (ret != TE_SUCCESS) {
        goto err_mulq;
    }
    ret = te_gfmul_update(&prv_ctx->gctx, sizeof(prv_ctx->npdata),
                          prv_ctx->npdata);
    if (ret != TE_SUCCESS) {
        goto err_mulq;
    }
    ret = te_gfmul_finish(&prv_ctx->gctx, eval, sizeof(eval));
    if (ret!= TE_SUCCESS) {
        goto err_mulq;
    }
    BS_XOR(mac, eval, prv_ctx->z + 16, 4);
    prv_ctx->state = UIA2_STATE_READY;
err_mulq:
err_state:
err_params:
    return ret;
}

int te_uia2_clone(const te_uia2_ctx_t *src, te_uia2_ctx_t *dst)
{
    int ret = TE_SUCCESS;
    te_uia2_ctx_t swap_ctx = {0};
    sca_uia2_ctx_t *prv_src = NULL;
    sca_uia2_ctx_t *prv_swap = NULL;
    /** sanity check params */
    if ((NULL == src) || (NULL == src->crypt) || (NULL == dst)) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_params;
    }
    osal_memset(&swap_ctx, 0x00, sizeof(swap_ctx));
    ret = te_sca_alloc_ctx((te_sca_drv_t *)src->crypt->drv,
                           TE_ALG_GET_MAIN_ALG(src->crypt->alg),
                           sizeof(sca_uia2_ctx_t),
                           &swap_ctx.crypt);
    if (ret !=  TE_SUCCESS) {
        goto err_alloc;
    }
    /** clone sca driver context */
    ret = te_sca_clone(src->crypt, swap_ctx.crypt);
    if (ret != TE_SUCCESS) {
        goto err_drv_clone;
    }
    /** clone private context*/
    prv_src = (sca_uia2_ctx_t *)uia2_priv_ctx(src);
    prv_swap = (sca_uia2_ctx_t *)uia2_priv_ctx(&swap_ctx);
    osal_memcpy(prv_swap, prv_src, sizeof(*prv_src));
    osal_memset(&prv_swap->gctx, 0x00, sizeof(prv_swap->gctx));
    ret = te_gfmul_clone(&prv_src->gctx, &prv_swap->gctx);
    if (ret != TE_SUCCESS) {
        goto err_gfmul_clone;
    }
    /** replace dst context */
    if (dst->crypt != NULL) {
        /**
         *  Ignore the error return from te_uia2_free, because once caller
         *  provide an uninitialized raw context from stack it may hold a
         *  pointer other than NULL which may treat te_uia2_free to return
         *  error codes such as TE_ERROR_BAD_STATE.
         */
        (void)te_uia2_free(dst);
    }

    osal_memcpy(dst, &swap_ctx, sizeof(*dst));
    goto fin;
err_gfmul_clone:
err_drv_clone:
    te_sca_free_ctx(swap_ctx.crypt);
err_alloc:
err_params:
fin:
    return ret;
}

#endif /* TE_SNOW3G_C */
