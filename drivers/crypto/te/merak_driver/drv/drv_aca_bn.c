
//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <driver/te_drv_aca.h>
#include "drv_aca_internal.h"
#include <hwa/te_hwa_aca.h>

/* helper function to reset one bn to 0, new_byte len can be 0 */
static int _aca_bn_reset(aca_drv_ctx_t *bn_ctx, size_t new_bytelen)
{
    int ret              = TE_SUCCESS;
    size_t old_sram_size = 0;

    if (BN_OP_CTX_IS_VALID(bn_ctx)) {
        ret = aca_sram_get_size(BN_GET_SRAM_BLOCK(bn_ctx), &old_sram_size);
        CHECK_RET_GO;
        /* size match */
        if (new_bytelen == old_sram_size) {
            ret = aca_sram_zeroize(BN_GET_SRAM_BLOCK(bn_ctx));
            CHECK_RET_GO;
            /* set sign to positive */
            bn_ctx->sign = BN_SIGN_POSITIVE;
            return TE_SUCCESS;
        }
    }

    op_ctx_clean(BN_GET_OP_CTX(bn_ctx));

    ret = op_ctx_init(bn_ctx->aca_drv, BN_GET_OP_CTX(bn_ctx), new_bytelen);
    CHECK_RET_GO;

    /* set sign to positive */
    bn_ctx->sign = BN_SIGN_POSITIVE;

    ret = TE_SUCCESS;
finish:
    return ret;
}

static void _aca_bn_occupy_op_ctx(aca_drv_ctx_t *bn_ctx, aca_op_ctx_t *op_ctx)
{
    op_ctx_clean(BN_GET_OP_CTX(bn_ctx));
    memcpy(BN_GET_OP_CTX(bn_ctx), op_ctx, sizeof(aca_op_ctx_t));
    memset(op_ctx, 0, sizeof(aca_op_ctx_t));
    return;
}

static int _aca_bn_cmpare_abs(aca_drv_ctx_t *bna_ctx,
                              aca_drv_ctx_t *bnb_ctx,
                              int *result)
{
    int ret = TE_SUCCESS;

    ret = aca_op_cmp(bna_ctx->aca_drv,
                     BN_GET_OP_CTX(bna_ctx),
                     BN_GET_OP_CTX(bnb_ctx),
                     result);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

int te_aca_bn_set_usr_data(te_aca_bn_t *bn, void *ptr)
{
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;

    BN_CHECK(bn);

    bn_ctx->extra_ctx[1] = ptr;

    return TE_SUCCESS;
}

int te_aca_bn_get_usr_data(const te_aca_bn_t *bn, void **pptr)
{
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;

    BN_CHECK(bn);

    *pptr = bn_ctx->extra_ctx[1];

    return TE_SUCCESS;
}

/**
 * \brief This function allocate one bignumber instance \p bn in te driver \p
 * drv. The new allocated bignumber is binded on te driver \p drv.
 *
 * This makes the bignumber ready to be set or freed, but does not define a
 * value for the bignumber.
 *
 * if \p bytelen_hint is 0, will not allocate sram or GR.
 *
 * \param[in] drv           The te driver instance to bind.
 * \param[in] bytelen_hint  The bignumber byte length hint.
 *                          0 for empty.
 * \param[out] bn           The bignumber context pointer.
 *                          This must not be \c NULL.
 * \return                  See te error code.
 */
int te_aca_bn_alloc(const te_crypt_drv_t *drv,
                    int32_t bytelen_hint,
                    te_aca_bn_t **bn)
{
    int ret               = TE_SUCCESS;
    te_aca_drv_t *aca_drv = (te_aca_drv_t *)drv;
    aca_drv_ctx_t *bn_ctx = NULL;

    CHECK_PARAM(drv && bn && (bytelen_hint >= 0));
    CHECK_PARAM(ACA_DRV_MAGIC == aca_drv->magic);

    bn_ctx = osal_calloc(1, sizeof(aca_drv_ctx_t));
    CHECK_COND_GO(bn_ctx, TE_ERROR_OOM);

    bn_ctx->aca_drv = aca_drv;

    bn_ctx->sign = BN_SIGN_POSITIVE;

    ret = op_ctx_init(aca_drv, BN_GET_OP_CTX(bn_ctx), bytelen_hint);
    CHECK_RET_GO;

    bn_ctx->magic = ACA_CTX_MAGIC;

    *bn = (te_aca_bn_t *)bn_ctx;

    ret = TE_SUCCESS;
finish:
    if (TE_SUCCESS != ret) {
        if (bn_ctx) {
            op_ctx_clean(&(bn_ctx->op_ctx));
            OSAL_SAFE_FREE(bn_ctx);
        }
    }
    return ret;
}

/**
 * \brief This function frees the componets of an bignumber context.
 *
 * \param[in] bn    The bignumber context to be cleared.This may be \c NULL,
 *                  in which case this function is a no-op. If it is
 *                  not \c NULL, it must point to an initialized bignumber.
 * \return          void.
 */
void te_aca_bn_free(te_aca_bn_t *bn)
{
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;

    if (!bn) {
        return;
    }
    if (bn_ctx->magic != ACA_CTX_MAGIC) {
        OSAL_LOG_ERR("BN Free invalid contex!\n");
        return;
    }

    op_ctx_clean(BN_GET_OP_CTX(bn_ctx));

    bn_ctx->magic = 0;
    OSAL_SAFE_FREE(bn_ctx);
    return;
}

/**
 * \brief This function set the sign of one bignumer.
 *
 * \param[in] bn        The bignumber to set sign. It must be initialized.
 * \param[in] sign      The new sign of bignumber.
 * \return              See te error code.
 */
int te_aca_bn_set_sign(te_aca_bn_t *bn, int32_t sign)
{
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;

    BN_CHECK(bn);
    CHECK_PARAM((sign == BN_SIGN_NEGATIVE) || (sign == BN_SIGN_POSITIVE));

    bn_ctx->sign = sign;
    return TE_SUCCESS;
}

/**
 * \brief This function get the sign of one bignumer.
 * \param[in] bn        The bignumber to grow. It must be initialized.
 * \param[out] sign     The pointer to save new sign.
 * \return              See te error code.
 */
int te_aca_bn_get_sign(te_aca_bn_t *bn, int32_t *sign)
{
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;

    BN_CHECK(bn);
    CHECK_PARAM(sign);

    *sign = bn_ctx->sign;
    return TE_SUCCESS;
}

/**
 * \brief Enlarge an bignumber to the specified number of limbs.
 *
 * \note  This function does nothing if the bignumer is already large enough.
 *
 * \param[in] bn        The bignumber to grow. It must be initialized.
 * \param[in] bytelen   The target byte length.
 * \return              See te error code.
 */
int te_aca_bn_grow(te_aca_bn_t *bn, size_t bytelen)
{
    int ret = TE_SUCCESS;

    BN_CHECK(bn);

    if (BN_OP_CTX_IS_VALID(bn)) {
        ret = aca_sram_try_change_size(BN_GET_SRAM_BLOCK(bn), bytelen);
        CHECK_RET_GO;
    } else {
        ret = _aca_bn_reset((aca_drv_ctx_t *)bn, bytelen);
        CHECK_RET_GO;
    }

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief This function resizes an MPI downwards, keeping at least the
 *        specified number of limbs.
 *        Resize down as much as possible, while keeping at least the specified
 * number of limbs
 *
 * \param[in] bn        The MPI to shrink.
 *                      This must point to an initialized MPI.
 * \param[in] bytelen   The target byte length.
 * \return              See te error code.
 */

int te_aca_bn_shrink(te_aca_bn_t *bn, size_t bytelen)
{
    return te_aca_bn_grow(bn, bytelen);
}

/**
 * \brief Make a copy of an MPI.
 *
 * Supported:
 * 1. bna is initialized with 0 length.
 *      Create bna with same size of bnb.
 * 2. bna == bnb:
 *      Do nothing.
 * 3. bna sram space > bnb space:
 *      Copy and clear the high bits of bna.
 * 3. bna sram space < bnb space:
 *      Copy only low bits of bnb.
 * 4. bna sram space == bnb space:
 *      Copy whole bnb.
 *
 * \param[out] bna   The destination MPI.
 *                   This must point to an initialized MPI.
 * \param[in] bnb    The source MPI.
 *                   This must point to an initialized MPI.
 * \return           See te error code.
 */
int te_aca_bn_copy(te_aca_bn_t *bna, const te_aca_bn_t *bnb)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bna_ctx = (aca_drv_ctx_t *)bna;
    aca_drv_ctx_t *bnb_ctx = (aca_drv_ctx_t *)bnb;

    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bna, bnb);

    if (bna == bnb) {
        ret = TE_SUCCESS;
        goto finish;
    }

    if (!BN_OP_CTX_IS_VALID(bnb)) {
        /* reset operation context to 0 */
        ret = _aca_bn_reset(bna_ctx, 0);
        CHECK_RET_GO;
    } else {
        /* copy operation context */
        ret = aca_op_copy(bna_ctx->aca_drv, BN_GET_OP_CTX(bna_ctx),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bnb_ctx));
        CHECK_RET_GO;

        /* set sign */
        bna_ctx->sign = bnb_ctx->sign;
    }

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Swap the contents of two MPIs.
 *
 * Support:
 * 1. bna == bnb:
 *      Do nothing.
 *
 * \param[in] bna   The first MPI. It must be initialized.
 * \param[in] bnb   The second MPI. It must be initialized.
 * \return           See te error code.
 */
int te_aca_bn_swap(te_aca_bn_t *bna, te_aca_bn_t *bnb)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bna_ctx = (aca_drv_ctx_t *)bna;
    aca_drv_ctx_t *bnb_ctx = (aca_drv_ctx_t *)bnb;
    aca_op_ctx_t tmp       = {0};
    int tmp_sign           = 0;

    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bna, bnb);

    if (bna == bnb) {
        ret = TE_SUCCESS;
        goto finish;
    }

    /* Swap the op context, this covers bna or bnb doesn't have data */
    memcpy(&tmp, BN_GET_OP_CTX(bnb_ctx), sizeof(aca_op_ctx_t));
    memcpy(BN_GET_OP_CTX(bnb_ctx), BN_GET_OP_CTX(bna_ctx),
           sizeof(aca_op_ctx_t));
    memcpy(BN_GET_OP_CTX(bna_ctx), &tmp, sizeof(aca_op_ctx_t));

    /* swap sign */
    tmp_sign      = bnb_ctx->sign;
    bnb_ctx->sign = bna_ctx->sign;
    bna_ctx->sign = tmp_sign;

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Import an MPI from unsigned big endian binary data.
 *
 * \param[in] bn    The destination MPI. This must point to an initialized MPI.
 * \param[in] buf   The input buffer. This must be a readable buffer of length
 *                 \p size Bytes.
 * \param[in] size  The length of the input buffer \p buf in Bytes.
 * \param[in] sign  The sign of destination MPI, can be -1 or 1.
 * \return          See te error code.
 */
int te_aca_bn_import(te_aca_bn_t *bn,
                     const uint8_t *buf,
                     size_t size,
                     int32_t sign)
{
    int ret               = TE_SUCCESS;
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;

    BN_CHECK(bn);
    CHECK_PARAM((sign == BN_SIGN_NEGATIVE) || (sign == BN_SIGN_POSITIVE));
    CHECK_PARAM(((buf) && (size)) || (0 == size));

    /* reset bn to target size */
    ret = _aca_bn_reset(bn_ctx, size);
    CHECK_RET_GO;

    if (buf && size) {
        /* sram write, write high bits 0 */
        ret = aca_sram_write(BN_GET_SRAM_BLOCK(bn), buf, size);
        CHECK_RET_GO;
        bn_ctx->sign = sign;
    }

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Export an MPI into unsigned big endian binary data
 *                 of fixed size.
 */
int te_aca_bn_export(te_aca_bn_t *bn, uint8_t *buf, size_t size)
{
    int ret             = TE_SUCCESS;
    size_t stored_bytes = 0;

    BN_CHECK(bn);
    CHECK_PARAM(((buf) && (size)) || (0 == size));

    if (0 == size) {
        return TE_SUCCESS;
    }

    /* BN is 0 */
    if (!BN_OP_CTX_IS_VALID(bn)) {
        memset(buf, 0, size);
        ret = TE_SUCCESS;
        goto finish;
    }

    /* get stored byte length */
    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bn), &stored_bytes);
    CHECK_RET_GO;

    stored_bytes = (stored_bytes + 7) / 8;

    /* check supplied buffer size */
    if (size < stored_bytes) {
        OSAL_LOG_ERR("Size: %d, stored bytes: %d\n", size, stored_bytes);
        ret = TE_ERROR_SHORT_BUFFER;
        goto finish;
    }

    /* sram read stored bytes */
    ret = aca_sram_read(BN_GET_SRAM_BLOCK(bn), buf + (size - stored_bytes),
                        stored_bytes);
    CHECK_RET_GO;

    /* reset others */
    memset(buf, 0, size - stored_bytes);

    ret = TE_SUCCESS;
finish:
    return ret;
}

#define _READ_U32(__value__, __buf__)                                          \
    do {                                                                       \
        __buf__[0] = (((__value__) >> 24) & 0xFF);                             \
        __buf__[1] = (((__value__) >> 16) & 0xFF);                             \
        __buf__[2] = (((__value__) >> 8) & 0xFF);                              \
        __buf__[3] = (((__value__) >> 0) & 0xFF);                              \
    } while (0)

#define _WRITE_U32(__buf__, __value__)                                         \
    do {                                                                       \
        __value__ = __buf__[3];                                                \
        __value__ |= (__buf__[2] << 8);                                        \
        __value__ |= (__buf__[1] << 16);                                       \
        __value__ |= (__buf__[0] << 24);                                       \
    } while (0)

/**
 * \brief Import an MPI from integer value \p z.
 *
 * \note    The sign of value \p z is also saved.
 *
 * \param[in] bn    The destination MPI. This must point to an initialized MPI.
 * \param[in] z     The value to use.
 * \return          See te error code.
 */
int te_aca_bn_import_s32(te_aca_bn_t *bn, int32_t z)
{
    int ret               = TE_SUCCESS;
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;
    uint8_t tmp_buf[4]    = {0};

    BN_CHECK(bn);

    /* reset bn to target size, also reset value to 0 */
    ret = _aca_bn_reset(bn_ctx, 4);
    CHECK_RET_GO;

    if (z >= 0) {
        _READ_U32(((uint32_t)z), tmp_buf);
    } else {
        _READ_U32(((uint32_t)(-z)), tmp_buf);
    }

    /* sram write, write high bits 0 */
    ret = aca_sram_write(BN_GET_SRAM_BLOCK(bn), tmp_buf, 4);
    CHECK_RET_GO;

    bn_ctx->sign = (z >= 0) ? (BN_SIGN_POSITIVE) : (BN_SIGN_NEGATIVE);

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Import an MPI from unsigned integer value \p uz.
 *
 * \note    The sign of value \p uz is always positive.
 *
 * \param[in] bn    The destination MPI. This must point to an initialized MPI.
 * \param[in] uz    The value to use.
 * \return          See te error code.
 */
int te_aca_bn_import_u32(te_aca_bn_t *bn, uint32_t uz)
{
    int ret               = TE_SUCCESS;
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;
    uint8_t tmp_buf[4]    = {0};

    BN_CHECK(bn);

    /* reset bn to target size */
    ret = _aca_bn_reset(bn_ctx, 4);
    CHECK_RET_GO;

    _READ_U32(uz, tmp_buf);

    /* sram write, write high bits 0 */
    ret = aca_sram_write(BN_GET_SRAM_BLOCK(bn), tmp_buf, 4);
    CHECK_RET_GO;

    bn_ctx->sign = BN_SIGN_POSITIVE;

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Export an MPI into an integer value \p z.
 *
 * \note  The sign is also set to \p z.
 *        The MPI should can fit to one integer.
 *
 * \param[in] bn    The source MPI. This must point to an initialized MPI.
 * \param[out] z    The pointer to save integer value.
 * \return          See te error code.
 */
int te_aca_bn_export_s32(te_aca_bn_t *bn, int32_t *z)
{
    int ret               = TE_SUCCESS;
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;
    size_t bit_len        = 0;
    uint8_t tmp_buf[4]    = {0};
    uint32_t val          = 0;

    BN_CHECK(bn);
    CHECK_PARAM(z);

    if (!BN_OP_CTX_IS_VALID(bn)) {
        *z  = 0;
        ret = TE_SUCCESS;
        goto finish;
    }

    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bn), &bit_len);
    CHECK_RET_GO;

    if (bit_len > 31) {
        ret = TE_ERROR_SHORT_BUFFER;
        goto finish;
    }

    ret = aca_sram_read(BN_GET_SRAM_BLOCK(bn), tmp_buf, 4);
    CHECK_RET_GO;

    _WRITE_U32(tmp_buf, val);

    if (bn_ctx->sign == BN_SIGN_NEGATIVE) {
        *z = -((int32_t)(val));
    } else {
        *z = ((int32_t)(val));
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Export an MPI into an unsigned integer value \p uz.
 *
 * \note  The sign of bn MUST be positive.
 *        The MPI should can fit to one unsinged integer.
 *
 * \param[in] bn    The source MPI. This must point to an initialized MPI.
 * \param[out] uz   The pointer to save integer value.
 * \return          See te error code.
 */
int te_aca_bn_export_u32(te_aca_bn_t *bn, uint32_t *uz)
{
    int ret               = TE_SUCCESS;
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;
    size_t bit_len        = 0;
    uint8_t tmp_buf[4]    = {0};
    uint32_t val          = 0;

    BN_CHECK(bn);
    CHECK_PARAM(uz);

    if (!BN_OP_CTX_IS_VALID(bn)) {
        *uz = 0;
        ret = TE_SUCCESS;
        goto finish;
    }

    /* bn must be positive */
    if (bn_ctx->sign == BN_SIGN_NEGATIVE) {
        return TE_ERROR_NEGATIVE_VALUE;
    }

    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bn), &bit_len);
    CHECK_RET_GO;

    if (bit_len > 32) {
        ret = TE_ERROR_SHORT_BUFFER;
        goto finish;
    }

    ret = aca_sram_read(BN_GET_SRAM_BLOCK(bn), tmp_buf, 4);
    CHECK_RET_GO;

    _WRITE_U32(tmp_buf, val);
    *uz = val;

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Import an MPI with random data.
 *
 * \param[in] bn    The destination MPI. This must point to an initialized MPI.
 * \param[in] size  The number of random bytes to generate.
 * \param[in] sign  The sign of destination MPI, can be 0 or 1.
 * \param[in] f_rng The RNG function to use. This must not be \c NULL.
 * \param[in] p_rng The RNG parameter to be passed to \p f_rng. This may be
 *                  \c NULL if \p f_rng doesn't need a context argument.
 * \return          See te error code.
 */
int te_aca_bn_import_random(te_aca_bn_t *bn,
                            size_t size,
                            int32_t sign,
                            int (*f_rng)(void *, uint8_t *, size_t),
                            void *p_rng)
{
    int ret               = TE_SUCCESS;
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;
    uint8_t *tmp          = NULL;

    BN_CHECK(bn);
    CHECK_PARAM(f_rng);
    CHECK_PARAM((sign == BN_SIGN_NEGATIVE) || (sign == BN_SIGN_POSITIVE));

    if (size) {
        tmp = osal_malloc(size);
        CHECK_COND_GO(tmp, TE_ERROR_OOM);

        ret = f_rng(p_rng, tmp, size);
        CHECK_COND_GO(0 == ret, TE_ERROR_GEN_RANDOM);

        ret = _aca_bn_reset(bn_ctx, size);
        CHECK_RET_GO;

        ret = aca_sram_write(BN_GET_SRAM_BLOCK(bn), (const uint8_t *)tmp, size);
        CHECK_RET_GO;

        bn_ctx->sign = sign;
    } else {
        ret = _aca_bn_reset(bn_ctx, 0);
        CHECK_RET_GO;
        bn_ctx->sign = sign;
    }
    ret = TE_SUCCESS;
finish:
    OSAL_SAFE_FREE(tmp);
    return ret;
}
/**
 * \brief Compare two MPIs.
 *
 * \param[in] bna       The left-hand MPI. This must point to an initialized
 * MPI. \param[in] bnb       The right-hand MPI. This must point to an
 * initialized MPI. \param[out] result   The pointer to store result. \c 1 if \p
 * bna is greater than \p bnb \c -1 if \p bna is lesser than \p bnb \c 0 if \p
 * bna is equal to \p bnb \return          See te error code.
 */
int te_aca_bn_cmp_bn(te_aca_bn_t *bna, te_aca_bn_t *bnb, int *result)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bna_ctx = (aca_drv_ctx_t *)bna;
    aca_drv_ctx_t *bnb_ctx = (aca_drv_ctx_t *)bnb;
    bool is_equal          = false;
    size_t a_bit_len       = 0;
    size_t b_bit_len       = 0;

    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bna, bnb);
    CHECK_PARAM(result);

    if (bna == bnb) {
        *result = 0;
        return TE_SUCCESS;
    }
    if ((!BN_OP_CTX_IS_VALID(bna)) && (BN_OP_CTX_IS_VALID(bnb))) {
        ret =
            aca_op_cmp_immeb(BN_GET_DRV(bna), BN_GET_OP_CTX(bnb), 0, &is_equal);
        CHECK_RET_GO;
        if (is_equal) {
            *result = 0;
        } else {
            *result = -(bnb_ctx->sign);
        }
    } else if ((BN_OP_CTX_IS_VALID(bna)) && (!BN_OP_CTX_IS_VALID(bnb))) {
        ret =
            aca_op_cmp_immeb(BN_GET_DRV(bna), BN_GET_OP_CTX(bna), 0, &is_equal);
        CHECK_RET_GO;
        if (is_equal) {
            *result = 0;
        } else {
            *result = bna_ctx->sign;
        }
    } else if ((!BN_OP_CTX_IS_VALID(bna)) && (!BN_OP_CTX_IS_VALID(bnb))) {
        *result = 0;
    } else {
        ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bna), &a_bit_len);
        CHECK_RET_GO;
        ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnb), &b_bit_len);
        CHECK_RET_GO;
        if ((!a_bit_len) && (b_bit_len)) {
            ret =
                aca_op_cmp_immeb(BN_GET_DRV(bna), BN_GET_OP_CTX(bnb), 0, &is_equal);
            CHECK_RET_GO;
            if (is_equal) {
                *result = 0;
            } else {
                *result = -(bnb_ctx->sign);
            }
        } else if ((a_bit_len) && (!b_bit_len)) {
            ret =
                aca_op_cmp_immeb(BN_GET_DRV(bna), BN_GET_OP_CTX(bna), 0, &is_equal);
            CHECK_RET_GO;
            if (is_equal) {
                *result = 0;
            } else {
                *result = bna_ctx->sign;
            }
        } else if ((!a_bit_len) && (!b_bit_len)) {
            *result = 0;
        } else {
            /* both a_bit_len and b_bit_len != 0 */
            if (bna_ctx->sign > bnb_ctx->sign) {
                *result = 1;
            } else if (bna_ctx->sign < bnb_ctx->sign) {
                *result = -1;
            } else {
                ret = _aca_bn_cmpare_abs(bna_ctx, bnb_ctx, result);
                CHECK_RET_GO;

                if (bna_ctx->sign == BN_SIGN_NEGATIVE) {
                    *result = -(*result);
                }
            }
        }
    }

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Compare an MPI with an integer.
 *
 * \param[in] bna       The left-hand MPI. This must point to an initialized
 * MPI. \param[in] b         The integer value to compare \p bna to. \param[out]
 * result   The pointer to store result. \c 1 if \p bna is greater than \p bnb
 *                  \c -1 if \p bna is lesser than \p bnb
 *                  \c 0 if \p bna is equal to \p bnb
 * \return          See te error code.
 */
int te_aca_bn_cmp_s32(te_aca_bn_t *bna, int32_t b, int *result)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bna_ctx = (aca_drv_ctx_t *)bna;
    int bna_val            = 0;

    BN_CHECK(bna);
    CHECK_PARAM(result);

    if (!BN_OP_CTX_IS_VALID(bna)) {
        *result = ((0 > b) ? (1) : ((0 < b) ? (-1) : (0)));
        ret     = TE_SUCCESS;
        goto finish;
    }

    if ((bna_ctx->sign == BN_SIGN_NEGATIVE) && (b > 0)) {
        *result = -1;
    } else if ((bna_ctx->sign == BN_SIGN_POSITIVE) && (b < 0)) {
        *result = 1;
    } else {
        ret = te_aca_bn_export_s32(bna, &bna_val);
        if (TE_ERROR_SHORT_BUFFER == (uint32_t)ret) {
            *result = bna_ctx->sign;
        } else {
            CHECK_RET_GO;
            *result = (bna_val > b) ? (1) : ((bna_val < b) ? (-1) : (0));
        }
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Perform a left-shift on an MPI: bna = bnb << count
 *
 * \note MPI \p bna and \p bnb can be same.
 * Support:
 *      1. bna is empty, create bna.
 *      2. bna == bnb.
 *
 * \param[out] bna      The result to save shifted MPI.
 *                      This must point to an initialized MPI.
 * \param[in] bnb       The source MPI to shift.
 *                      This must point to an initialized MPI.
 * \param[in] count     The number of bits to shift by.
 * \return              See te error code.
 */
int te_aca_bn_shift_l(te_aca_bn_t *bna, const te_aca_bn_t *bnb, int count)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bna_ctx = (aca_drv_ctx_t *)bna;

    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bna, bnb);

    if (!BN_OP_CTX_IS_VALID(bnb)) {
        ret = _aca_bn_reset(bna_ctx, 0);
        CHECK_RET_GO;
    } else {
        ret = aca_op_shift(bna_ctx->aca_drv,
                           BN_GET_OP_CTX(bna),
                           BN_GET_OP_CTX(bnb),
                           count,
                           TE_ACA_OP_SHL0);
        CHECK_RET_GO;

        bna_ctx->sign = ((aca_drv_ctx_t *)bnb)->sign;
    }

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Perform a right-shift on an MPI: bna = bnb >> count
 *
 * \note MPI \p bna and \p bnb can be same.
 *
 * \param[out] bna      The result to save shifted MPI.
 *                      This must point to an initialized MPI.
 * \param[in] bnb       The source MPI to shift.
 *                      This must point to an initialized MPI.
 * \param[in] count     The number of bits to shift by.
 * \return              See te error code.
 */
int te_aca_bn_shift_r(te_aca_bn_t *bna, const te_aca_bn_t *bnb, int count)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bna_ctx = (aca_drv_ctx_t *)bna;
    size_t b_bit_len       = 0;

    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bna, bnb);

    if (!BN_OP_CTX_IS_VALID(bnb)) {
        ret = _aca_bn_reset(bna_ctx, 0);
        CHECK_RET_GO;
    } else {
        ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnb), &b_bit_len);
        CHECK_RET_GO;
        if ((size_t)count >= b_bit_len) {
            ret = _aca_bn_reset(bna_ctx, 0);
            CHECK_RET_GO;
        } else {
            ret = aca_op_shift(bna_ctx->aca_drv,
                            BN_GET_OP_CTX(bna),
                            BN_GET_OP_CTX(bnb),
                            count,
                            TE_ACA_OP_SHR0);
            CHECK_RET_GO;

            bna_ctx->sign = ((aca_drv_ctx_t *)bnb)->sign;
        }
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Get a specific bit from an MPI.
 *
 * \param[in] bn    The MPI to query. This must point to an initialized MPI.
 * \param[in] pos   Zero-based index of the bit to query.
 * \return          \c 0 or \c 1 on success, depending on whether bit \c pos
 *                  of \c X is unset or set.
 * \return          A negative error code on failure.
 * \return          See te error code.
 */
int te_aca_bn_get_bit(te_aca_bn_t *bn, int pos)
{
    int ret           = TE_SUCCESS;
    size_t bn_bit_len = 0;
    int val           = 0;

    BN_CHECK(bn);
    CHECK_PARAM(pos >= 0);

    if (!BN_OP_CTX_IS_VALID(bn)) {
        return 0;
    }

    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bn), &bn_bit_len);
    CHECK_RET_GO;

    if ((size_t)pos >= bn_bit_len) {
        return 0;
    }
    ret = aca_sram_get_bit(BN_GET_SRAM_BLOCK(bn), (size_t)(pos), &val);
    CHECK_RET_GO;

    return val;
finish:
    return ret;
}

/**
 * \brief Set a specific bit in an MPI.
 *
 * \note    This function will grow the target MPI if necessary to set a
 *          bit to \c 1 in a not yet existing limb. It will not grow if
 *          the bit should be set to \c 0.
 *
 * \param[in] bn    The MPI to modify. This must point to an initialized MPI.
 * \param[in] pos   Zero-based index of the bit to modify.
 * \param[in] val   The desired value of bit \c pos: \c 0 or \c 1.
 * \return          See te error code.
 */
int te_aca_bn_set_bit(te_aca_bn_t *bn, int pos, uint8_t val)
{
    int ret           = TE_SUCCESS;
    size_t bn_bit_len = 0;

    BN_CHECK(bn);
    CHECK_PARAM(pos >= 0);
    CHECK_PARAM((val == 0) || (val == 1));

    if (!BN_OP_CTX_IS_VALID(bn)) {
        ret = _aca_bn_reset((aca_drv_ctx_t *)bn, (pos / 8) + 1);
        CHECK_RET_GO;
    }

    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bn), &bn_bit_len);
    CHECK_RET_GO;

    if ((size_t)pos >= bn_bit_len) {
        if (0 == val) {
            ret = TE_SUCCESS;
            goto finish;
        } else {
            ret = aca_sram_try_change_size(BN_GET_SRAM_BLOCK(bn), (pos / 8) + 1);
            CHECK_RET_GO;
        }
    }

    ret = aca_sram_set_bit(BN_GET_SRAM_BLOCK(bn), pos, val);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief   Return the number of bits up to and including the most
 *          significant bit of value \c 1.
 *
 * \note    This is same as the one-based index of the most
 *          significant bit of value \c 1.
 *
 * \param[in] bn    The MPI to query. This must point to an initialized MPI.
 * \return          The number of bits up to and including the most
 *                  significant bit of value \c 1.
 * \return          A negative error code on failure.
 * \return          See te error code.
 */
int te_aca_bn_bitlen(te_aca_bn_t *bn)
{
    int ret        = TE_SUCCESS;
    size_t bit_len = 0;

    BN_CHECK(bn);
    if (!BN_OP_CTX_IS_VALID(bn)) {
        return 0;
    }

    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bn), &bit_len);
    CHECK_RET_GO;

    return bit_len;
finish:
    return ret;
}

/**
 * \brief           Return the number of bits of value \c 0 before the
 *                  least significant bit of value \c 1.
 *
 * \param[in] bn    The MPI to query. This must point to an initialized MPI.
 * \return          The number of bits of value \c 0 before the least
 * significant bit of value \c 1 in \p X.
 */
int te_aca_bn_0bits_before_lsb1(te_aca_bn_t *bn)
{
    int ret          = TE_SUCCESS;
    size_t bit_num   = 0;
    int32_t bit_val  = 0;
    size_t sram_size = 0;

    BN_CHECK(bn);
    if (!BN_OP_CTX_IS_VALID(bn)) {
        return 0;
    }

    ret = aca_sram_get_size(BN_GET_SRAM_BLOCK(bn), &sram_size);
    CHECK_RET_GO;

    while (sram_size) {
        ret = aca_sram_get_bit(BN_GET_SRAM_BLOCK(bn), bit_num, &bit_val);
        CHECK_RET_GO;
        if (bit_val == 1) {
            return bit_num;
        }
        bit_num++;
        sram_size--;
    }
    return 0;
finish:
    return ret;
}

/**
 * \brief   Return the total size of an MPI value in bytes.
 *
 * \note    This is same as (bitlen + 7) / 8.
 *
 * \param[in] bn    The MPI to query. This must point to an initialized MPI.
 * \return          The least number of bytes capable of storing
 *                  the absolute value of \p bnb.
 * \return          A negative error code on failure.
 * \return          See te error code.
 */
int te_aca_bn_bytelen(te_aca_bn_t *bn)
{
    int ret        = TE_SUCCESS;
    size_t bit_len = 0;

    BN_CHECK(bn);
    if (!BN_OP_CTX_IS_VALID(bn)) {
        return 0;
    }

    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bn), &bit_len);
    CHECK_RET_GO;

    return (bit_len + 7) / 8;
finish:
    return ret;
}

/**
 * \brief Assign the absolute values of \p bnb to MPI \p bna : bnb = |bna|
 *
 * \note MPI \p bna and \p bnb can be same.
 *       The sign of \p bna is always positive (0).
 *
 * \param[out] bna  The destination MPI. This must point to an initialized MPI.
 * \param[in] bnb   The original MPI. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_abs(te_aca_bn_t *bna, const te_aca_bn_t *bnb)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bna_ctx = (aca_drv_ctx_t *)bna;
    aca_drv_ctx_t *bnb_ctx = (aca_drv_ctx_t *)bnb;

    BN_CHECK(bna);
    BN_CHECK(bnb);

    if (bna == bnb) {
        bna_ctx->sign = BN_SIGN_POSITIVE;
        return TE_SUCCESS;
    }

    if (!BN_OP_CTX_IS_VALID(bnb)) {
        ret = _aca_bn_reset(bna_ctx, 0);
        CHECK_RET_GO;
    } else {
        /* copy operation context */
        ret = aca_op_copy(bna_ctx->aca_drv, BN_GET_OP_CTX(bna_ctx),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bnb_ctx));
        CHECK_RET_GO;

        /* set sign */
        bna_ctx->sign = BN_SIGN_POSITIVE;
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}

static int _aca_op_sub_carry(const te_aca_drv_t *aca_drv, aca_op_ctx_t *op_ctx)
{
    int ret = TE_SUCCESS;

    /* get the negative data: X^0xFFFF....FFFF + 1 */
    ret = aca_op_run(aca_drv, op_ctx, op_ctx, NULL, 0x1F, NULL, NULL,
                     TE_ACA_OP_XOR, NULL);
    CHECK_RET_GO;

    ret = aca_op_run(aca_drv, op_ctx, op_ctx, NULL, 0x1, NULL, NULL,
                     TE_ACA_OP_ADD, NULL);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

/* there is carry in add, so should extend bn to save the carry bit */
static int _aca_op_add_carry(const te_aca_drv_t *aca_drv, aca_op_ctx_t *op_ctx)
{
    int ret              = TE_SUCCESS;
    size_t old_sram_size = 0;

    (void)(aca_drv);
    ret = aca_sram_get_size(op_ctx->sram_block, &old_sram_size);
    CHECK_RET_GO;

    /* try to change size to old_sram_size + 1 */
    ret = aca_sram_try_change_size(op_ctx->sram_block, old_sram_size + 1);
    CHECK_RET_GO;

    /* set old op bits to 1 */
    ret = aca_sram_set_bit(op_ctx->sram_block, old_sram_size * 8, 1);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

static int _aca_bn_process_sub_carry(aca_drv_ctx_t *bn_ctx)
{
    return _aca_op_sub_carry(bn_ctx->aca_drv, BN_GET_OP_CTX(bn_ctx));
}

static int _aca_bn_process_add_carry(aca_drv_ctx_t *bn_ctx)
{
    return _aca_op_add_carry(bn_ctx->aca_drv, BN_GET_OP_CTX(bn_ctx));
}

/**
 * \brief Perform the AND logic of two MPIs: bnr = bna & bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first MPI. This must point to an initialized MPI.
 * \param[in] bnb   The second MPI. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_and(te_aca_bn_t *bnr,
                  const te_aca_bn_t *bna,
                  const te_aca_bn_t *bnb)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx = (aca_drv_ctx_t *)bnr;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bnr, bna);
    BN_CHECK_CONST_DRV(bnr, bnb);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnb)->sign == BN_SIGN_POSITIVE);

    if ((!BN_OP_CTX_IS_VALID(bna)) || (!BN_OP_CTX_IS_VALID(bnb))) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        CHECK_RET_GO;
    } else {
        ret = aca_op_run(BN_GET_DRV(bnr),
                         BN_GET_OP_CTX(bnr),
                         BN_GET_OP_CTX(bna),
                         BN_GET_OP_CTX(bnb),
                         -1,
                         NULL,
                         NULL,
                         TE_ACA_OP_AND,
                         NULL);
        CHECK_RET_GO;
        bnr_ctx->sign = BN_SIGN_POSITIVE;
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}
/**
 * \brief Perform the OR logic of two MPIs: bnr = bna | bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first MPI. This must point to an initialized MPI.
 * \param[in] bnb   The second MPI. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_or(te_aca_bn_t *bnr,
                 const te_aca_bn_t *bna,
                 const te_aca_bn_t *bnb)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx = (aca_drv_ctx_t *)bnr;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bnr, bna);
    BN_CHECK_CONST_DRV(bnr, bnb);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnb)->sign == BN_SIGN_POSITIVE);

    if ((!BN_OP_CTX_IS_VALID(bna)) && (BN_OP_CTX_IS_VALID(bnb))) {
        /* copy operation context */
        ret = aca_op_copy(((aca_drv_ctx_t *)(bna))->aca_drv, BN_GET_OP_CTX(bnr),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bnb));
        CHECK_RET_GO;
        bnr_ctx->sign = BN_SIGN_POSITIVE;
    } else if ((!BN_OP_CTX_IS_VALID(bnb)) && (BN_OP_CTX_IS_VALID(bna))) {
        /* copy operation context */
        ret = aca_op_copy(((aca_drv_ctx_t *)(bna))->aca_drv, BN_GET_OP_CTX(bnr),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bna));
        CHECK_RET_GO;
        bnr_ctx->sign = BN_SIGN_POSITIVE;
    } else if ((!BN_OP_CTX_IS_VALID(bna)) && (!BN_OP_CTX_IS_VALID(bnb))) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        CHECK_RET_GO;
    } else {
        ret = aca_op_run(BN_GET_DRV(bnr),
                         BN_GET_OP_CTX(bnr),
                         BN_GET_OP_CTX(bna),
                         BN_GET_OP_CTX(bnb),
                         -1,
                         NULL,
                         NULL,
                         TE_ACA_OP_OR,
                         NULL);
        CHECK_RET_GO;
        bnr_ctx->sign = BN_SIGN_POSITIVE;
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}
/**
 * \brief Perform the XOR logic of two MPIs: bnr = bna ^ bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first MPI. This must point to an initialized MPI.
 * \param[in] bnb   The second MPI. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_xor(te_aca_bn_t *bnr,
                  const te_aca_bn_t *bna,
                  const te_aca_bn_t *bnb)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx = (aca_drv_ctx_t *)bnr;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bnr, bna);
    BN_CHECK_CONST_DRV(bnr, bnb);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnb)->sign == BN_SIGN_POSITIVE);

    if ((!BN_OP_CTX_IS_VALID(bna)) && (BN_OP_CTX_IS_VALID(bnb))) {
        /* copy operation context */
        ret = aca_op_copy(((aca_drv_ctx_t *)(bna))->aca_drv, BN_GET_OP_CTX(bnr),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bnb));
        CHECK_RET_GO;
        bnr_ctx->sign = BN_SIGN_POSITIVE;
    } else if ((!BN_OP_CTX_IS_VALID(bnb)) && (BN_OP_CTX_IS_VALID(bna))) {
        /* copy operation context */
        ret = aca_op_copy(((aca_drv_ctx_t *)(bna))->aca_drv, BN_GET_OP_CTX(bnr),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bna));
        CHECK_RET_GO;
        bnr_ctx->sign = BN_SIGN_POSITIVE;
    } else if ((!BN_OP_CTX_IS_VALID(bna)) && (!BN_OP_CTX_IS_VALID(bnb))) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        CHECK_RET_GO;
    } else {
        ret = aca_op_run(BN_GET_DRV(bnr),
                         BN_GET_OP_CTX(bnr),
                         BN_GET_OP_CTX(bna),
                         BN_GET_OP_CTX(bnb),
                         -1,
                         NULL,
                         NULL,
                         TE_ACA_OP_XOR,
                         NULL);
        CHECK_RET_GO;
        bnr_ctx->sign = BN_SIGN_POSITIVE;
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief The following section describes the basic arithmetic operations.
 */

/**
 * \brief Perform a signed addition of MPIs: bnr = bna + bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first summand. This must point to an initialized MPI.
 * \param[in] bnb   The second summand. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_add_bn(te_aca_bn_t *bnr,
                     const te_aca_bn_t *bna,
                     const te_aca_bn_t *bnb)
{
    int ret                       = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx        = (aca_drv_ctx_t *)bnr;
    aca_drv_ctx_t *bna_ctx        = (aca_drv_ctx_t *)bna;
    aca_drv_ctx_t *bnb_ctx        = (aca_drv_ctx_t *)bnb;
    int org_bn_a_sign             = 0;
    aca_op_status_t result_status = {0};

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bnr, bna);
    BN_CHECK_CONST_DRV(bnr, bnb);

    if ((!BN_OP_CTX_IS_VALID(bna)) && (BN_OP_CTX_IS_VALID(bnb))) {
        /* copy operation context */
        ret = aca_op_copy(bna_ctx->aca_drv, BN_GET_OP_CTX(bnr),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bnb));
        CHECK_RET_GO;
        bnr_ctx->sign = bnb_ctx->sign;
    } else if ((!BN_OP_CTX_IS_VALID(bnb)) && (BN_OP_CTX_IS_VALID(bna))) {
        /* copy operation context */
        ret = aca_op_copy(bna_ctx->aca_drv, BN_GET_OP_CTX(bnr),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bna));
        CHECK_RET_GO;
        bnr_ctx->sign = bna_ctx->sign;
    } else if ((!BN_OP_CTX_IS_VALID(bna)) && (!BN_OP_CTX_IS_VALID(bnb))) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        CHECK_RET_GO;
    } else {
        if (bna_ctx->sign == bnb_ctx->sign) {
            ret = aca_op_run(BN_GET_DRV(bnr),
                             BN_GET_OP_CTX(bnr),
                             BN_GET_OP_CTX(bna),
                             BN_GET_OP_CTX(bnb),
                             -1,
                             NULL,
                             NULL,
                             TE_ACA_OP_ADD,
                             &result_status);
            CHECK_RET_GO;
            if (result_status.alu_carry) {
                ret = _aca_bn_process_add_carry(bnr_ctx);
                CHECK_RET_GO;
            }
            bnr_ctx->sign = bna_ctx->sign;
        } else {
            /* a - b */
            org_bn_a_sign = bna_ctx->sign;

            ret = aca_op_run(BN_GET_DRV(bnr),
                             BN_GET_OP_CTX(bnr),
                             BN_GET_OP_CTX(bna),
                             BN_GET_OP_CTX(bnb),
                             -1,
                             NULL,
                             NULL,
                             TE_ACA_OP_SUB,
                             &result_status);
            CHECK_RET_GO;
            if (result_status.alu_carry) {
                ret = _aca_bn_process_sub_carry(bnr_ctx);
                CHECK_RET_GO;
                bnr_ctx->sign = BN_SIGN_NEGATIVE;
            } else {
                bnr_ctx->sign = BN_SIGN_POSITIVE;
            }
            bnr_ctx->sign = (bnr_ctx->sign) * (org_bn_a_sign);
        }
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}

/* bnr = bna - bnb */

/**
 * \brief Perform a signed substraction of MPIs: bnr = bna - bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first minuend. This must point to an initialized MPI.
 * \param[in] bnb   The subtrahend. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_sub_bn(te_aca_bn_t *bnr,
                     const te_aca_bn_t *bna,
                     const te_aca_bn_t *bnb)
{
    int ret                       = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx        = (aca_drv_ctx_t *)bnr;
    aca_drv_ctx_t *bna_ctx        = (aca_drv_ctx_t *)bna;
    aca_drv_ctx_t *bnb_ctx        = (aca_drv_ctx_t *)bnb;
    int org_bn_a_sign             = 0;
    aca_op_status_t result_status = {0};

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bnr, bna);
    BN_CHECK_CONST_DRV(bnr, bnb);

    if ((!BN_OP_CTX_IS_VALID(bna)) && (BN_OP_CTX_IS_VALID(bnb))) {
        /* copy operation context */
        ret = aca_op_copy(bna_ctx->aca_drv, BN_GET_OP_CTX(bnr),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bnb));
        CHECK_RET_GO;
        bnr_ctx->sign = -(bnb_ctx->sign);
    } else if ((!BN_OP_CTX_IS_VALID(bnb)) && (BN_OP_CTX_IS_VALID(bna))) {
        /* copy operation context */
        ret = aca_op_copy(bna_ctx->aca_drv, BN_GET_OP_CTX(bnr),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bna));
        CHECK_RET_GO;
        bnr_ctx->sign = bna_ctx->sign;
    } else if ((!BN_OP_CTX_IS_VALID(bna)) && (!BN_OP_CTX_IS_VALID(bnb))) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        CHECK_RET_GO;
    } else {

        if (bna_ctx->sign == bnb_ctx->sign) {
            org_bn_a_sign = bna_ctx->sign;

            ret = aca_op_run(BN_GET_DRV(bnr),
                             BN_GET_OP_CTX(bnr),
                             BN_GET_OP_CTX(bna),
                             BN_GET_OP_CTX(bnb),
                             -1,
                             NULL,
                             NULL,
                             TE_ACA_OP_SUB,
                             &result_status);
            CHECK_RET_GO;
            if (result_status.alu_carry) {
                ret = _aca_bn_process_sub_carry(bnr_ctx);
                CHECK_RET_GO;
                bnr_ctx->sign = BN_SIGN_NEGATIVE;
            } else {
                bnr_ctx->sign = BN_SIGN_POSITIVE;
            }
            bnr_ctx->sign = (bnr_ctx->sign) * (org_bn_a_sign);
        } else {
            /* a + b */
            ret = aca_op_run(BN_GET_DRV(bnr),
                             BN_GET_OP_CTX(bnr),
                             BN_GET_OP_CTX(bna),
                             BN_GET_OP_CTX(bnb),
                             -1,
                             NULL,
                             NULL,
                             TE_ACA_OP_ADD,
                             &result_status);
            CHECK_RET_GO;
            if (result_status.alu_carry) {
                ret = _aca_bn_process_add_carry(bnr_ctx);
                CHECK_RET_GO;
            }
            bnr_ctx->sign = bna_ctx->sign;
        }
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Perform a signed addition of MPI and integer number: bnr = bna + b
 *
 * \note    MPI \p bna and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first summand. This must point to an initialized MPI.
 * \param[in] b     The second integer summand.
 * \return          See te error code.
 */
int te_aca_bn_add_s32(te_aca_bn_t *bnr, const te_aca_bn_t *bna, int32_t b)
{
    int ret            = TE_SUCCESS;
    te_aca_bn_t *tmp_b = NULL;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK_CONST_DRV(bnr, bna);

    /* if bna ctx is invalid, this is handled in te_aca_bn_add_bn */

    ret = te_aca_bn_alloc((const te_crypt_drv_t *)BN_GET_DRV(bnr), 4, &tmp_b);
    CHECK_RET_GO;

    ret = te_aca_bn_import_s32(tmp_b, b);
    CHECK_RET_GO;

    ret = te_aca_bn_add_bn(bnr, bna, (const te_aca_bn_t *)(tmp_b));
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    te_aca_bn_free(tmp_b);
    return ret;
}
/**
 * \brief Perform a signed substraction of MPI and integer number: bnr = bna - b
 *
 * \note    MPI \p bna and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first minuend. This must point to an initialized MPI.
 * \param[in] bnb   The integer subtrahend.
 * \return          See te error code.
 */
int te_aca_bn_sub_s32(te_aca_bn_t *bnr, const te_aca_bn_t *bna, int32_t b)
{
    int ret            = TE_SUCCESS;
    te_aca_bn_t *tmp_b = NULL;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK_CONST_DRV(bnr, bna);

    /* if bna ctx is invalid, this is handled in te_aca_bn_sub_bn */

    ret = te_aca_bn_alloc((const te_crypt_drv_t *)BN_GET_DRV(bnr), 4, &tmp_b);
    CHECK_RET_GO;
    ret = te_aca_bn_import_s32(tmp_b, b);
    CHECK_RET_GO;

    ret = te_aca_bn_sub_bn(bnr, bna, (const te_aca_bn_t *)(tmp_b));
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    te_aca_bn_free(tmp_b);
    return ret;
}
/**
 * \brief Assign the negative values of \p bna to MPI \p bnr : bnr = -bna
 *
 * \note MPI \p bna and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bnb   The original MPI. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_neg_bn(te_aca_bn_t *bnr, const te_aca_bn_t *bna)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx = (aca_drv_ctx_t *)bnr;
    aca_drv_ctx_t *bna_ctx = (aca_drv_ctx_t *)bna;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK_CONST_DRV(bnr, bna);

    if (!BN_OP_CTX_IS_VALID(bna)) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        CHECK_RET_GO;
    } else {
        if (bnr == bna) {
            ret           = TE_SUCCESS;
            bnr_ctx->sign = -(bna_ctx->sign);
            goto finish;
        } else {
            /* copy operation context */
            ret = aca_op_copy(BN_GET_DRV(bnr), BN_GET_OP_CTX(bnr_ctx),
                              (const aca_op_ctx_t *)BN_GET_OP_CTX(bna_ctx));
            CHECK_RET_GO;

            /* set sign */
            bnr_ctx->sign = -(bna_ctx->sign);
        }
    }

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Perform a unsigned addition of MPIs: bnr = |bna| + |bnb|
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first summand. This must point to an initialized MPI.
 * \param[in] bnb   The second summand. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_add_abs(te_aca_bn_t *bnr,
                      const te_aca_bn_t *bna,
                      const te_aca_bn_t *bnb)
{
    int ret                       = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx        = (aca_drv_ctx_t *)bnr;
    aca_drv_ctx_t *bna_ctx        = (aca_drv_ctx_t *)bna;
    aca_op_status_t result_status = {0};

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bnr, bna);
    BN_CHECK_CONST_DRV(bnr, bnb);

    if ((!BN_OP_CTX_IS_VALID(bna)) && (BN_OP_CTX_IS_VALID(bnb))) {
        /* copy operation context */
        ret = aca_op_copy(bna_ctx->aca_drv, BN_GET_OP_CTX(bnr),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bnb));
        CHECK_RET_GO;
        bnr_ctx->sign = BN_SIGN_POSITIVE;
    } else if ((!BN_OP_CTX_IS_VALID(bnb)) && (BN_OP_CTX_IS_VALID(bna))) {
        /* copy operation context */
        ret = aca_op_copy(bna_ctx->aca_drv, BN_GET_OP_CTX(bnr),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bna));
        CHECK_RET_GO;
        bnr_ctx->sign = BN_SIGN_POSITIVE;
    } else if ((!BN_OP_CTX_IS_VALID(bna)) && (!BN_OP_CTX_IS_VALID(bnb))) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        CHECK_RET_GO;
    } else {
        ret = aca_op_run(BN_GET_DRV(bnr),
                         BN_GET_OP_CTX(bnr),
                         BN_GET_OP_CTX(bna),
                         BN_GET_OP_CTX(bnb),
                         -1,
                         NULL,
                         NULL,
                         TE_ACA_OP_ADD,
                         &result_status);
        CHECK_RET_GO;

        if (result_status.alu_carry) {
            ret = _aca_bn_process_add_carry(bnr_ctx);
            CHECK_RET_GO;
        }

        bnr_ctx->sign = BN_SIGN_POSITIVE;
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}
/**
 * \brief Perform a unsigned substraction of MPIs: bnr = |bna| - |bnb|
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first minuend. This must point to an initialized MPI.
 * \param[in] bnb   The subtrahend. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_sub_abs(te_aca_bn_t *bnr,
                      const te_aca_bn_t *bna,
                      const te_aca_bn_t *bnb)
{

    int ret                       = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx        = (aca_drv_ctx_t *)bnr;
    aca_drv_ctx_t *bna_ctx        = (aca_drv_ctx_t *)bna;
    size_t bit_len                = 0;
    aca_op_status_t result_status = {0};
    int cmp_result                = 0;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bnr, bna);
    BN_CHECK_CONST_DRV(bnr, bnb);

    if ((!BN_OP_CTX_IS_VALID(bna)) && (BN_OP_CTX_IS_VALID(bnb))) {
        /* check bnb is 0 */
        ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnb), &bit_len);
        CHECK_RET_GO;
        if (0 == bit_len) {
            ret = _aca_bn_reset(bnr_ctx, 0);
            CHECK_RET_GO;
        } else {
            ret = TE_ERROR_NEGATIVE_VALUE;
            goto finish;
        }
    } else if ((!BN_OP_CTX_IS_VALID(bnb)) && (BN_OP_CTX_IS_VALID(bna))) {
        /* copy operation context */
        ret = aca_op_copy(bna_ctx->aca_drv, BN_GET_OP_CTX(bnr),
                          (const aca_op_ctx_t *)BN_GET_OP_CTX(bna));
        CHECK_RET_GO;
        bnr_ctx->sign = BN_SIGN_POSITIVE;
    } else if ((!BN_OP_CTX_IS_VALID(bna)) && (!BN_OP_CTX_IS_VALID(bnb))) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        CHECK_RET_GO;
    } else {
        ret = aca_op_cmp(BN_GET_DRV(bnr), BN_GET_OP_CTX(bna),
                         BN_GET_OP_CTX(bnb), &cmp_result);
        CHECK_RET_GO;
        if (cmp_result < 0) {
            ret = TE_ERROR_NEGATIVE_VALUE;
            goto finish;
        }

        ret = aca_op_run(BN_GET_DRV(bnr),
                         BN_GET_OP_CTX(bnr),
                         BN_GET_OP_CTX(bna),
                         BN_GET_OP_CTX(bnb),
                         -1,
                         NULL,
                         NULL,
                         TE_ACA_OP_SUB,
                         &result_status);
        CHECK_RET_GO;
        TE_ASSERT(!(result_status.alu_carry));
        bnr_ctx->sign = BN_SIGN_POSITIVE;
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}
/**
 * \brief Perform a multiplication of two MPIs: bnr = bna * bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first factor. This must point to an initialized MPI.
 * \param[in] bnb   The second factor. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_mul_bn(te_aca_bn_t *bnr,
                     const te_aca_bn_t *bna,
                     const te_aca_bn_t *bnb)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx = (aca_drv_ctx_t *)bnr;
    aca_drv_ctx_t *bna_ctx = (aca_drv_ctx_t *)bna;
    aca_drv_ctx_t *bnb_ctx = (aca_drv_ctx_t *)bnb;
    size_t r_sram_size = 0, a_sram_size = 0, b_sram_size = 0, r_h_sram_size = 0,
           r_l_sram_size = 0;
    aca_op_ctx_t r_h     = {0};
    aca_op_ctx_t r_l     = {0};
    uint8_t *tmp_buf     = NULL;
    bool is_rh_zero      = false;
    size_t bit_len       = 0;
    int32_t a_sign       = 0;
    int32_t b_sign       = 0;

    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK(bnr);
    BN_CHECK_CONST_DRV(bnr, bna);
    BN_CHECK_CONST_DRV(bnr, bnb);

    if ((!BN_OP_CTX_IS_VALID(bna)) || (!BN_OP_CTX_IS_VALID(bnb))) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        CHECK_RET_GO;
    } else {
        a_sign = bna_ctx->sign;
        b_sign = bnb_ctx->sign;

        ret = aca_sram_get_size(BN_GET_SRAM_BLOCK(bna), &a_sram_size);
        CHECK_RET_GO;

        ret = aca_sram_get_size(BN_GET_SRAM_BLOCK(bnb), &b_sram_size);
        CHECK_RET_GO;

        /* calculate mul h and mul l */
        ret = aca_op_run(BN_GET_DRV(bnr), &r_l, BN_GET_OP_CTX(bna),
                         BN_GET_OP_CTX(bnb), -1, NULL, NULL, TE_ACA_OP_MUL_LOW,
                         NULL);
        CHECK_RET_GO;

        ret = aca_op_run(BN_GET_DRV(bnr), &r_h, BN_GET_OP_CTX(bna),
                         BN_GET_OP_CTX(bnb), -1, NULL, NULL, TE_ACA_OP_MUL_HIGH,
                         NULL);
        CHECK_RET_GO;

        /* check rh is 0? */
        ret = aca_op_cmp_immeb(BN_GET_DRV(bnr), &r_h, 0, &is_rh_zero);
        CHECK_RET_GO;

        if (is_rh_zero) {
            /* rh is zero, we can shrink rl */
            /* shrink r_l */
            ret = aca_sram_try_change_size(r_l.sram_block, 0);
            CHECK_RET_GO;

            /* get r_l size */
            ret = aca_sram_get_size(r_l.sram_block, &r_l_sram_size);
            CHECK_RET_GO;
            r_h_sram_size = 0;
        } else {
            /* rh is not zero, only shrink rh */

            /* get r_l size */
            ret = aca_sram_get_size(r_l.sram_block, &r_l_sram_size);
            CHECK_RET_GO;

            /* shrink rh as much as possible */
            ret = aca_sram_try_change_size(r_h.sram_block, 0);
            CHECK_RET_GO;
            ret = aca_sram_get_size(r_h.sram_block, &r_h_sram_size);
            CHECK_RET_GO;
        }

        /* create a buffer to save rl and rh */
        tmp_buf = osal_malloc(r_l_sram_size + r_h_sram_size);
        CHECK_COND_GO(tmp_buf, TE_ERROR_OOM);

        ret = aca_sram_read(r_h.sram_block, tmp_buf, r_h_sram_size);
        CHECK_RET_GO;
        ret = aca_sram_read(r_l.sram_block, tmp_buf + r_h_sram_size,
                            r_l_sram_size);
        CHECK_RET_GO;

        if (BN_OP_CTX_IS_VALID(bnr)) {
            ret = aca_sram_get_size(BN_GET_SRAM_BLOCK(bnr), &r_sram_size);
            CHECK_RET_GO;
        } else {
            r_sram_size = 0;
        }

        if (r_sram_size < r_l_sram_size + r_h_sram_size) {
            ret = _aca_bn_reset(bnr_ctx, r_l_sram_size + r_h_sram_size);
            CHECK_RET_GO;
        } else {
            ret = aca_sram_zeroize(BN_GET_SRAM_BLOCK(bnr));
            CHECK_RET_GO;
        }

        ret = aca_sram_write(BN_GET_SRAM_BLOCK(bnr), (const uint8_t *)tmp_buf,
                             r_l_sram_size + r_h_sram_size);
        CHECK_RET_GO;

        ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnr), &bit_len);
        CHECK_RET_GO;
        if (0 != bit_len) {
            bnr_ctx->sign = a_sign * b_sign;
        } else {
            bnr_ctx->sign = BN_SIGN_POSITIVE;
        }
    }
    ret = TE_SUCCESS;
finish:
    OSAL_SAFE_FREE(tmp_buf);
    op_ctx_clean(&r_l);
    op_ctx_clean(&r_h);
    return ret;
}

/**
 * \brief Perform a multiplication of an MPI with an integer: bnr = bna * b
 *
 * \note    MPI \p bna and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first factor. This must point to an initialized MPI.
 * \param[in] b     The second factor.
 * \return          See te error code.
 */
int te_aca_bn_mul_s32(te_aca_bn_t *bnr, const te_aca_bn_t *bna, const int32_t b)
{
    int ret            = TE_SUCCESS;
    te_aca_bn_t *tmp_b = NULL;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK_CONST_DRV(bnr, bna);

    ret = te_aca_bn_alloc((const te_crypt_drv_t *)BN_GET_DRV(bnr), 4, &tmp_b);
    CHECK_RET_GO;
    ret = te_aca_bn_import_s32(tmp_b, b);
    CHECK_RET_GO;

    ret = te_aca_bn_mul_bn(bnr, bna, (const te_aca_bn_t *)(tmp_b));
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    te_aca_bn_free(tmp_b);
    return ret;
}

/**
 * \brief Perform a square of an MPI: bnr = bna * bna
 *
 * \note    MPI \p bna and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The source MPI to be squared.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_square(te_aca_bn_t *bnr, const te_aca_bn_t *bna)
{
    return te_aca_bn_mul_bn(bnr, bna, bna);
}
/**
 * \brief Perform a division with remainder of two MPIs: bna = bnq * bnb + bnr.
 *        bnr has the same sign as bna, bnq round to zero
 *
 * \note    MPI \p bna and \p bnq can be same.
 *          MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnq can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnq  The destination MPI for the quotient.
 *                  This may be \p NULL if the value of quotient is not needed.
 * \param[out] bnr  The destination MPI the remainder value.
 *                  This may be \c NULL if the value of the remainder is
 *                  not needed.
 * \param[in] bna   The dividend. This must point to an initialized MPi.
 * \param[in] bnb   The divisor. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_div_bn(te_aca_bn_t *bnq,
                     te_aca_bn_t *bnr,
                     const te_aca_bn_t *bna,
                     const te_aca_bn_t *bnb)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx = (aca_drv_ctx_t *)bnr;
    aca_drv_ctx_t *bnq_ctx = (aca_drv_ctx_t *)bnq;
    aca_drv_ctx_t *bna_ctx = (aca_drv_ctx_t *)bna;
    aca_drv_ctx_t *bnb_ctx = (aca_drv_ctx_t *)bnb;
    int bna_sign = 0, bnb_sign = 0;
    bool is_equal = false;

    BN_CHECK(bna);
    BN_CHECK(bnb);

    if ((!bnr) && (!bnq)) {
        ret = TE_SUCCESS;
        goto finish;
    }

    if (!BN_OP_CTX_IS_VALID(bnb)) {
        ret = TE_ERROR_DIV_BY_ZERO;
        goto finish;
    }
    if (!BN_OP_CTX_IS_VALID(bna)) {
        if (bnq) {
            ret = _aca_bn_reset(bnq_ctx, 0);
            CHECK_RET_GO;
        }
        if (bnr) {
            ret = _aca_bn_reset(bnr_ctx, 0);
            CHECK_RET_GO;
        }
    } else {
        bna_sign = bna_ctx->sign;
        bnb_sign = bnb_ctx->sign;

        ret = aca_op_div_bn(BN_GET_DRV(bna),
                            (bnr ? (BN_GET_OP_CTX(bnr)) : (NULL)),
                            (bnq ? (BN_GET_OP_CTX(bnq)) : (NULL)),
                            BN_GET_OP_CTX(bna), BN_GET_OP_CTX(bnb));
        CHECK_RET_GO;

        if (bnq) {
            bnq_ctx->sign = bna_sign * bnb_sign;

            /* if q is 0, set sign to 1 */
            ret = aca_op_cmp_immeb(BN_GET_DRV(bna), BN_GET_OP_CTX(bnq), 0,
                                   &is_equal);
            CHECK_RET_GO;
            if (is_equal) {
                bnq_ctx->sign = 1;
            }
        }
        if (bnr) {
            bnr_ctx->sign = bna_sign;

            /* if r is 0, set sign to 1 */
            ret = aca_op_cmp_immeb(BN_GET_DRV(bna), BN_GET_OP_CTX(bnr), 0,
                                   &is_equal);
            CHECK_RET_GO;
            if (is_equal) {
                bnr_ctx->sign = 1;
            }
        }
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}

/* For all mod operation, we ignore the sign */

/**
 * \brief Perform a modular reduction: bnr = bna mod bnn.
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI for the residue value.
 *                  This must point to an initialized MPI.
 * \param[in] bna   The MPI to compute the residue of.
 *                  This must point to an initialized MPI.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_mod_bn(te_aca_bn_t *bnr,
                     const te_aca_bn_t *bna,
                     const te_aca_bn_t *bnn)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx = (aca_drv_ctx_t *)bnr;
    size_t n_bit_len       = 0;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnn);
    BN_CHECK_CONST_DRV(bnr, bnn);
    BN_CHECK_CONST_DRV(bna, bnn);
    CHECK_PARAM(bnn != bnr);
    CHECK_PARAM(((aca_drv_ctx_t *)bnn)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);

    if (!BN_OP_CTX_IS_VALID(bnn)) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnn), &n_bit_len);
    CHECK_RET_GO;
    if (0 == n_bit_len) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    if (!BN_OP_CTX_IS_VALID(bna)) {
        ret = _aca_bn_reset(((aca_drv_ctx_t *)bnr), 0);
        goto finish;
    }
    /* N is 1, return 0 directly */
    if (1 == n_bit_len) {
        ret = _aca_bn_reset(((aca_drv_ctx_t *)bnr), 0);
        goto finish;
    }
    ret = aca_op_mod_bn(BN_GET_DRV(bna),
                        BN_GET_OP_CTX(bnr),
                        BN_GET_OP_CTX(bna),
                        BN_GET_OP_CTX(bnn));
    CHECK_RET_GO;

    bnr_ctx->sign = BN_SIGN_POSITIVE;

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * \brief Perform a modular reduction on unsigned value: r = bna mod n.
 *
 * \param[out] r    The destination value potiner for the residue value.
 * \param[in] bna   The MPI to compute the residue of.
 *                  This must point to an initialized MPI.
 * \param[in] n     The base of the modular reduction.
 *                  This must NOT be 0.
 * \return          See te error code.
 */
int te_aca_bn_mod_u32(uint32_t *r, const te_aca_bn_t *bna, uint32_t n)
{
    int ret            = TE_SUCCESS;
    aca_op_ctx_t tmp_n = {0};
    aca_op_ctx_t tmp_r = {0};
    uint8_t tmp_buf[4] = {0};

    BN_CHECK(bna);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);

    if (0 == n) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    if (1 == n) {
        *r  = 0;
        ret = TE_SUCCESS;
        goto finish;
    }

    if (!BN_OP_CTX_IS_VALID(bna)) {
        *r  = 0;
        ret = TE_SUCCESS;
        goto finish;
    }
    ret = aca_op_set_u32(BN_GET_DRV(bna), &tmp_n, n);
    CHECK_RET_GO;

    ret = aca_op_mod_bn(BN_GET_DRV(bna), &tmp_r, BN_GET_OP_CTX(bna), &tmp_n);
    CHECK_RET_GO;

    /* convert tmp_r to uint32_t */
    ret = aca_sram_read(tmp_r.sram_block, tmp_buf, 4);
    CHECK_RET_GO;

    _WRITE_U32(tmp_buf, *r);

    ret = TE_SUCCESS;
finish:
    op_ctx_clean(&tmp_n);
    op_ctx_clean(&tmp_r);
    return ret;
}

/* bnr = (bna + bnb) mod bnn. bna and bnb MUST < bnn */

/**
 * \brief   Perform a modular reduction of signed addition of two MPIs:
 *          bnr = (bna + bnb) mod bnn
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI for the residue value.
 *                  This must point to an initialized MPI.
 * \param[in] bna   The first summand. This must point to an initialized MPI.
 * \param[in] bnb   The second summand. This must point to an initialized MPI.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_add_mod(te_aca_bn_t *bnr,
                      const te_aca_bn_t *bna,
                      const te_aca_bn_t *bnb,
                      const te_aca_bn_t *bnn)
{
    int ret                       = TE_SUCCESS;
    aca_op_ctx_t tmp_a            = {0};
    aca_op_ctx_t tmp_b            = {0};
    aca_op_status_t result_status = {0};
    aca_drv_ctx_t *bnr_ctx        = (aca_drv_ctx_t *)bnr;
    size_t n_bit_len              = 0;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK(bnn);
    BN_CHECK_CONST_DRV(bnr, bnn);
    BN_CHECK_CONST_DRV(bna, bnn);
    BN_CHECK_CONST_DRV(bnb, bnn);
    CHECK_PARAM(bnn != bnr);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnb)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnn)->sign == BN_SIGN_POSITIVE);

    if (!BN_OP_CTX_IS_VALID(bnn)) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnn), &n_bit_len);
    CHECK_RET_GO;
    if (0 == n_bit_len) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    /* N is 1, return 0 directly */
    if (1 == n_bit_len) {
        ret = _aca_bn_reset(((aca_drv_ctx_t *)bnr), 0);
        goto finish;
    }

    if (!BN_OP_CTX_IS_VALID(bna)) {
        ret = op_ctx_init(BN_GET_DRV(bnn), &tmp_a, 1);
        CHECK_RET_GO;
    } else {
        /* tmp_a = a mod N */
        ret = aca_op_mod_bn(BN_GET_DRV(bnn), &tmp_a, BN_GET_OP_CTX(bna),
                            BN_GET_OP_CTX(bnn));
        CHECK_RET_GO;
    }

    if (!BN_OP_CTX_IS_VALID(bnb)) {
        ret = op_ctx_init(BN_GET_DRV(bnn), &tmp_b, 1);
        CHECK_RET_GO;
    } else {
        /* tmp_b = b mod N */
        ret = aca_op_mod_bn(BN_GET_DRV(bnn), &tmp_b, BN_GET_OP_CTX(bnb),
                            BN_GET_OP_CTX(bnn));
        CHECK_RET_GO;
    }
    ret = aca_op_run(BN_GET_DRV(bnn),
                     BN_GET_OP_CTX(bnr),
                     &tmp_a,
                     &tmp_b,
                     -1,
                     NULL,
                     BN_GET_OP_CTX(bnn),
                     TE_ACA_OP_MODADD,
                     &result_status);
    CHECK_RET_GO;
    if (result_status.mult_red_err || result_status.mod_n_zero_err) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    bnr_ctx->sign = BN_SIGN_POSITIVE;

    ret = TE_SUCCESS;
finish:
    op_ctx_clean(&tmp_a);
    op_ctx_clean(&tmp_b);
    return ret;
}
/**
 * \brief   Perform a modular reduction of signed substraction of two MPIs:
 *          bnr = (bna - bnb) mod bnn
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI for the residue value.
 *                  This must point to an initialized MPI.
 * \param[in] bna   The first minuend. This must point to an initialized MPI.
 * \param[in] bnb   The subtrahend. This must point to an initialized MPI.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_sub_mod(te_aca_bn_t *bnr,
                      const te_aca_bn_t *bna,
                      const te_aca_bn_t *bnb,
                      const te_aca_bn_t *bnn)
{
    int ret                       = TE_SUCCESS;
    aca_op_ctx_t tmp_a            = {0};
    aca_op_ctx_t tmp_b            = {0};
    aca_op_status_t result_status = {0};
    aca_drv_ctx_t *bnr_ctx        = (aca_drv_ctx_t *)bnr;
    size_t n_bit_len              = 0;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK(bnn);
    BN_CHECK_CONST_DRV(bnr, bnn);
    BN_CHECK_CONST_DRV(bna, bnn);
    BN_CHECK_CONST_DRV(bnb, bnn);
    CHECK_PARAM(bnn != bnr);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnb)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnn)->sign == BN_SIGN_POSITIVE);

    if (!BN_OP_CTX_IS_VALID(bnn)) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnn), &n_bit_len);
    CHECK_RET_GO;
    if (0 == n_bit_len) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    /* N is 1, return 0 directly */
    if (1 == n_bit_len) {
        ret = _aca_bn_reset(((aca_drv_ctx_t *)bnr), 0);
        goto finish;
    }

    if (!BN_OP_CTX_IS_VALID(bna)) {
        ret = op_ctx_init(BN_GET_DRV(bnn), &tmp_a, 1);
        CHECK_RET_GO;
    } else {
        /* tmp_a = a mod N */
        ret = aca_op_mod_bn(BN_GET_DRV(bnn), &tmp_a, BN_GET_OP_CTX(bna),
                            BN_GET_OP_CTX(bnn));
        CHECK_RET_GO;
    }

    if (!BN_OP_CTX_IS_VALID(bnb)) {
        ret = op_ctx_init(BN_GET_DRV(bnn), &tmp_b, 1);
        CHECK_RET_GO;
    } else {
        /* tmp_b = b mod N */
        ret = aca_op_mod_bn(BN_GET_DRV(bnn), &tmp_b, BN_GET_OP_CTX(bnb),
                            BN_GET_OP_CTX(bnn));
        CHECK_RET_GO;
    }

    ret = aca_op_run(BN_GET_DRV(bnn),
                     BN_GET_OP_CTX(bnr),
                     &tmp_a,
                     &tmp_b,
                     -1,
                     NULL,
                     BN_GET_OP_CTX(bnn),
                     TE_ACA_OP_MODSUB,
                     &result_status);
    CHECK_RET_GO;
    if (result_status.mult_red_err || result_status.mod_n_zero_err) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    bnr_ctx->sign = BN_SIGN_POSITIVE;

    ret = TE_SUCCESS;
finish:
    op_ctx_clean(&tmp_a);
    op_ctx_clean(&tmp_b);
    return ret;
}

/**
 * \brief Perform a modular reduction of multiplication of two MPIs:
 *          bnr = (bna * bnb) mod bnn
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI for the residue value.
 *                  This must point to an initialized MPI.
 * \param[in] bna   The first factor. This must point to an initialized MPI.
 * \param[in] bnb   The second factor. This must point to an initialized MPI.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_mul_mod(te_aca_bn_t *bnr,
                      const te_aca_bn_t *bna,
                      const te_aca_bn_t *bnb,
                      const te_aca_bn_t *bnn)
{
    int ret                       = TE_SUCCESS;
    aca_op_ctx_t tmp_a            = {0};
    aca_op_ctx_t tmp_b            = {0};
    aca_op_status_t result_status = {0};
    aca_drv_ctx_t *bnr_ctx        = (aca_drv_ctx_t *)bnr;
    size_t n_bit_len              = 0;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK(bnn);
    BN_CHECK_CONST_DRV(bnr, bnn);
    BN_CHECK_CONST_DRV(bna, bnn);
    BN_CHECK_CONST_DRV(bnb, bnn);
    CHECK_PARAM(bnn != bnr);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnb)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnn)->sign == BN_SIGN_POSITIVE);

    if (!BN_OP_CTX_IS_VALID(bnn)) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnn), &n_bit_len);
    CHECK_RET_GO;
    if (0 == n_bit_len) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    /* N is 1, return 0 directly */
    if (1 == n_bit_len) {
        ret = _aca_bn_reset(((aca_drv_ctx_t *)bnr), 0);
        goto finish;
    }

    if (!BN_OP_CTX_IS_VALID(bna)) {
        ret = op_ctx_init(BN_GET_DRV(bnn), &tmp_a, 1);
        CHECK_RET_GO;
    } else {
        ret = aca_op_mod_bn(BN_GET_DRV(bnn), &tmp_a, BN_GET_OP_CTX(bna),
                            BN_GET_OP_CTX(bnn));
        CHECK_RET_GO;
    }

    if (!BN_OP_CTX_IS_VALID(bnb)) {
        ret = op_ctx_init(BN_GET_DRV(bnn), &tmp_b, 1);
        CHECK_RET_GO;
    } else {
        ret = aca_op_mod_bn(BN_GET_DRV(bnn), &tmp_b, BN_GET_OP_CTX(bnb),
                            BN_GET_OP_CTX(bnn));
        CHECK_RET_GO;
    }
    ret =
        aca_op_run(BN_GET_DRV(bnn), BN_GET_OP_CTX(bnr), &tmp_a, &tmp_b, -1,
                   NULL, BN_GET_OP_CTX(bnn), TE_ACA_OP_MODMUL, &result_status);
    CHECK_RET_GO;
    if (result_status.mult_red_err || result_status.mod_n_zero_err) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    bnr_ctx->sign = BN_SIGN_POSITIVE;

    ret = TE_SUCCESS;
finish:
    op_ctx_clean(&tmp_a);
    op_ctx_clean(&tmp_b);
    return ret;
}
/**
 * \brief Perform a modular reduction of square of MPIs:
 *          bnr = (bna * bna) mod bnn
 *
 * \note    The bit length of \p bna and MUST < \p bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI for the residue value.
 *                  This must point to an initialized MPI.
 * \param[in] bna   The source MPI to be squared.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_square_mod(te_aca_bn_t *bnr,
                         const te_aca_bn_t *bna,
                         const te_aca_bn_t *bnn)
{
    int ret                       = TE_SUCCESS;
    aca_op_ctx_t tmp_a            = {0};
    aca_op_status_t result_status = {0};
    aca_drv_ctx_t *bnr_ctx        = (aca_drv_ctx_t *)bnr;
    size_t n_bit_len              = 0;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnn);
    BN_CHECK_CONST_DRV(bnr, bnn);
    BN_CHECK_CONST_DRV(bna, bnn);
    CHECK_PARAM(bnn != bnr);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnn)->sign == BN_SIGN_POSITIVE);

    if (!BN_OP_CTX_IS_VALID(bnn)) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnn), &n_bit_len);
    CHECK_RET_GO;
    if (0 == n_bit_len) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    /* N is 1, return 0 directly */
    if (1 == n_bit_len) {
        ret = _aca_bn_reset(((aca_drv_ctx_t *)bnr), 0);
        goto finish;
    }
    if (!BN_OP_CTX_IS_VALID(bna)) {
        ret = op_ctx_init(BN_GET_DRV(bnn), &tmp_a, 1);
        CHECK_RET_GO;
    } else {
        ret = aca_op_mod_bn(BN_GET_DRV(bnn), &tmp_a, BN_GET_OP_CTX(bna),
                            BN_GET_OP_CTX(bnn));
        CHECK_RET_GO;
    }

    ret =
        aca_op_run(BN_GET_DRV(bnn), BN_GET_OP_CTX(bnr), &tmp_a, &tmp_a, -1,
                   NULL, BN_GET_OP_CTX(bnn), TE_ACA_OP_MODMUL, &result_status);
    CHECK_RET_GO;
    if (result_status.mult_red_err || result_status.mod_n_zero_err) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    bnr_ctx->sign = BN_SIGN_POSITIVE;

    ret = TE_SUCCESS;
finish:
    op_ctx_clean(&tmp_a);
    return ret;
}

/**
 * \brief   Compute the modular inverse: bnr = (a^-1) mod bnn
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The MPI to calculate the modular inverse of.
 *                  This must point to an initialized MPI.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_inv_mod(te_aca_bn_t *bnr,
                      const te_aca_bn_t *bna,
                      const te_aca_bn_t *bnn)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx = (aca_drv_ctx_t *)bnr;
    aca_op_ctx_t tmp_r     = {0};
    size_t n_bit_len       = 0;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnn);
    BN_CHECK_CONST_DRV(bnr, bnn);
    BN_CHECK_CONST_DRV(bna, bnn);
    CHECK_PARAM(bnn != bnr);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnn)->sign == BN_SIGN_POSITIVE);

    if (!BN_OP_CTX_IS_VALID(bnn)) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnn), &n_bit_len);
    CHECK_RET_GO;
    if (0 == n_bit_len) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    if (!BN_OP_CTX_IS_VALID(bna)) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        goto finish;
    }

    /* N is 1, not acceptable */
    /* Max supported N bit_length to 8192 */
    if ((1 == n_bit_len) || (n_bit_len > 8192)) {
        ret = TE_ERROR_NOT_ACCEPTABLE;
        goto finish;
    }

    ret = aca_op_modinv(BN_GET_DRV(bnn), BN_GET_OP_CTX(bnn), BN_GET_OP_CTX(bna),
                        &tmp_r);
    CHECK_RET_GO;

    _aca_bn_occupy_op_ctx(bnr_ctx, &tmp_r);
    bnr_ctx->sign = BN_SIGN_POSITIVE;

    ret = TE_SUCCESS;
finish:
    op_ctx_clean(&tmp_r);
    return ret;
}

/**
 * \brief Perform a exponentiation: bnr = bna^bne mod bnn
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bne and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \note    Both MPIs' bit length of \p bna and \p bne should < bit length of
 *          \p bnn
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The base of the exponentiation.
 *                  This must point to an initialized MPI.
 * \param[in] bne   The exponent MPI. This must point to an initialized MPI.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_exp_mod(te_aca_bn_t *bnr,
                      const te_aca_bn_t *bna,
                      const te_aca_bn_t *bne,
                      const te_aca_bn_t *bnn)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx = (aca_drv_ctx_t *)bnr;
    size_t n_bit_len       = 0;
    size_t bit_len         = 0;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bne);
    BN_CHECK(bnn);
    BN_CHECK_CONST_DRV(bnr, bnn);
    BN_CHECK_CONST_DRV(bna, bnn);
    BN_CHECK_CONST_DRV(bne, bnn);
    CHECK_PARAM(bnn != bnr);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bne)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnn)->sign == BN_SIGN_POSITIVE);

    if (!BN_OP_CTX_IS_VALID(bnn)) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnn), &n_bit_len);
    CHECK_RET_GO;
    if (0 == n_bit_len) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    if (!BN_OP_CTX_IS_VALID(bna)) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        goto finish;
    } else {
        ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bna), &bit_len);
        CHECK_RET_GO;
        if (0 == bit_len) {
            ret = _aca_bn_reset(bnr_ctx, 0);
            goto finish;
        }
    }

    /* N is 1, set resutl to 0 directly */
    if (1 == n_bit_len) {
        ret = te_aca_bn_import_u32(bnr, 0);
        goto finish;
    }

    /* E is 0, set result to 1 */
    if (!BN_OP_CTX_IS_VALID(bne)) {
        ret = te_aca_bn_import_u32(bnr, 1);
        goto finish;
    } else {
        ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bne), &bit_len);
        CHECK_RET_GO;
        if (0 == bit_len) {
            ret = te_aca_bn_import_u32(bnr, 1);
            goto finish;
        }
    }

    ret = aca_op_mod_exp(BN_GET_DRV(bnn),
                         BN_GET_OP_CTX(bnr),
                         BN_GET_OP_CTX(bna),
                         BN_GET_OP_CTX(bne),
                         BN_GET_OP_CTX(bnn));
    CHECK_RET_GO;
    bnr_ctx->sign = BN_SIGN_POSITIVE;
finish:
    return ret;
}

/**
 * \brief Compute the greatest common divisor: bnr = gcd(bna, bnb)
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first operand. This must point to an initialized MPI.
 * \param[in] bnb   The second operand. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_aca_bn_gcd(te_aca_bn_t *bnr,
                  const te_aca_bn_t *bna,
                  const te_aca_bn_t *bnb)
{
    int ret                = TE_SUCCESS;
    aca_drv_ctx_t *bnr_ctx = (aca_drv_ctx_t *)bnr;
    size_t bit_len         = 0;

    BN_CHECK(bnr);
    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bnr, bna);
    BN_CHECK_CONST_DRV(bnr, bnb);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnb)->sign == BN_SIGN_POSITIVE);

    if ((!BN_OP_CTX_IS_VALID(bna)) || (!BN_OP_CTX_IS_VALID(bnb))) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        goto finish;
    }
    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bna), &bit_len);
    CHECK_RET_GO;
    if (0 == bit_len) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        goto finish;
    }
    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnb), &bit_len);
    CHECK_RET_GO;
    if (0 == bit_len) {
        ret = _aca_bn_reset(bnr_ctx, 0);
        goto finish;
    }

    ret = aca_op_gcd(BN_GET_DRV(bnr),
                     BN_GET_OP_CTX(bna),
                     BN_GET_OP_CTX(bnb),
                     BN_GET_OP_CTX(bnr));
    CHECK_RET_GO;
    bnr_ctx->sign = BN_SIGN_POSITIVE;

    ret = TE_SUCCESS;
finish:
    return ret;
}
/**
 * \brief Checks whether two MPIs are relative prime: gcd(bna, bnb) == 1
 *
 * \param[in] bna   The first operand. This must point to an initialized MPI.
 * \param[in] bnb   The second operand. This must point to an initialized MPI.
 * \return          \p TE_ERROR_NOT_ACCEPTABLE : not relative prime
 *                  \p TE_SUCCESS : relative prime
 *                  A negative error code on failure.
 * \return          See te error code.
 */
int te_aca_bn_relative_prime(const te_aca_bn_t *bna, const te_aca_bn_t *bnb)
{
    int ret            = TE_SUCCESS;
    aca_op_ctx_t tmp_r = {0};
    bool is_equal      = false;
    size_t bit_len     = 0;

    BN_CHECK(bna);
    BN_CHECK(bnb);
    BN_CHECK_CONST_DRV(bna, bnb);
    CHECK_PARAM(((aca_drv_ctx_t *)bna)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)bnb)->sign == BN_SIGN_POSITIVE);

    if ((!BN_OP_CTX_IS_VALID(bna)) || (!BN_OP_CTX_IS_VALID(bnb))) {
        ret = TE_ERROR_NOT_ACCEPTABLE;
        goto finish;
    }

    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bna), &bit_len);
    CHECK_RET_GO;
    if (0 == bit_len) {
        ret = TE_ERROR_NOT_ACCEPTABLE;
        goto finish;
    }
    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(bnb), &bit_len);
    CHECK_RET_GO;
    if (0 == bit_len) {
        ret = TE_ERROR_NOT_ACCEPTABLE;
        goto finish;
    }

    ret = aca_op_gcd(BN_GET_DRV(bna), BN_GET_OP_CTX(bna), BN_GET_OP_CTX(bnb),
                     &tmp_r);
    CHECK_RET_GO;

    ret = aca_op_cmp_immeb(BN_GET_DRV(bna), &tmp_r, 1, &is_equal);
    CHECK_RET_GO;
    if (is_equal) {
        ret = TE_SUCCESS;
    } else {
        ret = TE_ERROR_NOT_ACCEPTABLE;
    }

finish:
    op_ctx_clean(&tmp_r);
    return ret;
}

static const int32_t small_prime[] = {
    3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,
    53,  59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107, 109,
    113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191,
    193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269,
    271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353,
    359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439,
    443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523,
    541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617,
    619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701, 709,
    719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811,
    821, 823, 827, 829, 839, 853, 857, 859, 863, 877, 881, 883, 887, 907,
    911, 919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997, -103};

/*
 * Small divisors test (X must be positive)
 *
 * Return values:
 * TE_SUCCESS: no small factor (possible prime, more tests needed)
 * 1: certain prime
 * TE_ERROR_NOT_ACCEPTABLE: certain non-prime
 * other negative: error
 */
static int _aca_bn_check_small_factors(const te_aca_bn_t *bn)
{
    int ret                      = TE_SUCCESS;
    size_t i                     = 0;
    int32_t lsb                  = 0;
    aca_op_ctx_t tmp_small_prime = {0};
    aca_op_ctx_t tmp_r           = {0};
    bool is_zero                 = false;
    int32_t val                  = 0;

    ret = aca_sram_get_bit(BN_GET_SRAM_BLOCK(bn), 0, &lsb);
    CHECK_RET_GO;

    if (lsb == 0) {
        ret = TE_ERROR_NOT_ACCEPTABLE;
        goto finish;
    }

    ret = te_aca_bn_export_s32((te_aca_bn_t *)bn, &val);
    if (TE_SUCCESS != ret) {
        val = -1;
    }

    for (i = 0; small_prime[i] > 0; i++) {
        /* bn_int_value means bn can fit in one s32 */
        if ((val != -1) && (val <= small_prime[i])) {
            ret = 1;
            goto finish;
        }
        ret = aca_op_set_u32(BN_GET_DRV(bn), &tmp_small_prime,
                             (uint32_t)small_prime[i]);
        CHECK_RET_GO;

        ret = aca_op_mod_bn(BN_GET_DRV(bn), &tmp_r, BN_GET_OP_CTX(bn),
                            &tmp_small_prime);
        CHECK_RET_GO;

        /* check r is 0 */
        ret = aca_op_cmp_immeb(BN_GET_DRV(bn), &tmp_r, 0, &is_zero);
        CHECK_RET_GO;
        if (is_zero) {
            ret = TE_ERROR_NOT_ACCEPTABLE;
            goto finish;
        }
    }

finish:
    op_ctx_clean(&tmp_small_prime);
    op_ctx_clean(&tmp_r);
    return ret;
}

static int _aca_bn_miller_rabin(te_aca_bn_t *bnX,
                                int32_t rounds,
                                int (*f_rng)(void *, uint8_t *, size_t),
                                void *p_rng)
{
    int ret                        = TE_SUCCESS;
    aca_drv_ctx_t *bnX_ctx         = (aca_drv_ctx_t *)bnX;
    size_t W_zero_bits_before_lsb1 = 0;
    size_t W_bit_len = 0, A_bit_len = 0;
    int32_t result1 = 0, result2 = 0;
    size_t i = 0, j = 0, count = 0;
    te_aca_bn_t *W = NULL;
    te_aca_bn_t *R = NULL;
    te_aca_bn_t *A = NULL;

    /* only check positive */
    TE_ASSERT(bnX_ctx->sign == BN_SIGN_POSITIVE);

    ret = te_aca_bn_alloc((const te_crypt_drv_t *)BN_GET_DRV(bnX), 0, &W);
    CHECK_RET_GO;
    ret = te_aca_bn_alloc((const te_crypt_drv_t *)BN_GET_DRV(bnX), 0, &R);
    CHECK_RET_GO;
    ret = te_aca_bn_alloc((const te_crypt_drv_t *)BN_GET_DRV(bnX), 0, &A);
    CHECK_RET_GO;

    /*
     * W = |X| - 1
     * R = W >> lsb( W )
     */
    ret = te_aca_bn_sub_s32(W, (const te_aca_bn_t *)bnX, 1);
    CHECK_RET_GO;
    TE_ASSERT(((aca_drv_ctx_t *)(W))->sign == BN_SIGN_POSITIVE);
    W_zero_bits_before_lsb1 = te_aca_bn_0bits_before_lsb1(W);
    CHECK_COND_GO(W_zero_bits_before_lsb1 > 0, W_zero_bits_before_lsb1);
    ret = te_aca_bn_shift_r(R, W, W_zero_bits_before_lsb1);
    CHECK_RET_GO;

    ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(W), &W_bit_len);
    CHECK_RET_GO;

    for (i = 0; i < (size_t)rounds; i++) {
        /*
         * pick a random A, 1 < A < |X| - 1
         */
        count = 0;
        do {
            ret = te_aca_bn_import_random(A, (W_bit_len + 7) / 8,
                                          BN_SIGN_POSITIVE, f_rng, p_rng);
            CHECK_RET_GO;

            ret = aca_sram_get_bit_len(BN_GET_SRAM_BLOCK(A), &A_bit_len);
            CHECK_RET_GO;

            if (A_bit_len > W_bit_len) {
                ret = te_aca_bn_shift_r(A, A, A_bit_len - W_bit_len + 1);
                CHECK_RET_GO;
            }

            if (count++ > 30) {
                return TE_ERROR_NOT_ACCEPTABLE;
            }

            ret = te_aca_bn_cmp_bn(A, W, &result1);
            CHECK_RET_GO;
            ret = te_aca_bn_cmp_s32(A, 1, &result2);
            CHECK_RET_GO;

        } while ((result1 >= 0) || (result2 <= 0));

        /*
         * A = A^R mod |X|
         */
        ret = te_aca_bn_exp_mod(A, A, R, bnX);
        CHECK_RET_GO;

        ret = te_aca_bn_cmp_bn(A, W, &result1);
        CHECK_RET_GO;
        ret = te_aca_bn_cmp_s32(A, 1, &result2);
        CHECK_RET_GO;
        if ((result1 == 0) || (result2 == 0)) {
            continue;
        }

        j = 1;
        while (true) {
            if (j >= W_zero_bits_before_lsb1) {
                break;
            }
            ret = te_aca_bn_cmp_bn(A, W, &result1);
            CHECK_RET_GO;
            if (result1 == 0) {
                break;
            }
            /*
             * A = A * A mod |bnX|
             */
            ret = te_aca_bn_mul_mod(A, (const te_aca_bn_t *)A,
                                    (const te_aca_bn_t *)A, bnX);
            CHECK_RET_GO;

            ret = te_aca_bn_cmp_s32(A, 1, &result2);
            CHECK_RET_GO;
            if (result2 == 0) {
                break;
            }
            j++;
        }

        /*
         * not prime if A != |X| - 1 or A == 1
         */
        ret = te_aca_bn_cmp_bn(A, W, &result1);
        CHECK_RET_GO;
        ret = te_aca_bn_cmp_s32(A, 1, &result2);
        CHECK_RET_GO;
        if ((result1 != 0) || (result2 == 0)) {
            ret = TE_ERROR_NOT_ACCEPTABLE;
            goto finish;
        }
    }

    ret = TE_SUCCESS;

finish:
    if (W) {
        te_aca_bn_free(W);
    }
    if (R) {
        te_aca_bn_free(R);
    }
    if (A) {
        te_aca_bn_free(A);
    }
    return ret;
}

/**
 * \brief Perform the Miller-Rabin primality test.
 *
 * \param[in] bn                The MPI to check for primality.
 *                              This must point to an initialized MPI.
 * \param[in] round             The number of bases to perform the Miller-Rabin
 *                              primality test for.
 *                              The probability of returning 0 on a composite is
 *                              at most 2<sup>-2*\p round</sup>.
 * \param[in] f_rng             The RNG function to use.
 *                              This must not be \c NULL.
 * \param[in] p_rng             The RNG parameter to be passed to \p f_rng.
 *                              This may be \c NULL if \p f_rng doesn't use
 *                              a context parameter.
 * \return                      \c 0 if successful, i.e. \p bn is probably
 * prime. \c TE_ERROR_NOT_ACCEPTABLE if \p bn is not prime A negative error code
 * on failure. See te error code.
 */
int te_aca_bn_is_probale_prime(const te_aca_bn_t *bn,
                               uint32_t round,
                               int (*f_rng)(void *, uint8_t *, size_t),
                               void *p_rng)
{
    int ret     = TE_SUCCESS;
    int32_t val = 0;

    BN_CHECK(bn);
    CHECK_PARAM(f_rng);
    CHECK_PARAM(round > 0);
    CHECK_PARAM(((aca_drv_ctx_t *)bn)->sign == BN_SIGN_POSITIVE);

    if (!BN_OP_CTX_IS_VALID(bn)) {
        return TE_ERROR_NOT_ACCEPTABLE;
    }

    /* check 0, 1, or 2 */
    ret = te_aca_bn_export_s32((te_aca_bn_t *)bn, &val);
    if (TE_SUCCESS == ret) {
        if ((0 == val) || (1 == val)) {
            return TE_ERROR_NOT_ACCEPTABLE;
        }
        if (2 == val) {
            return TE_SUCCESS;
        }
    } else {
        val = -1;
    }

    ret = _aca_bn_check_small_factors(bn);
    if (TE_SUCCESS != ret) {
        if (1 == ret) {
            return TE_SUCCESS;
        }
        return ret;
    }

    return _aca_bn_miller_rabin((te_aca_bn_t *)bn, round, f_rng, p_rng);
}

/**
 * \brief Generae a prime number as bignumber.
 * To generate an RSA key in a way recommended by FIPS 186-4, both primes must
 * be either 1024 bits or 1536 bits long, and the is_low_err should be true.
 *
 * \param[out] bn
 *
 * \param[in] bn                The MPI to save the prime number.
 *                              This must point to an initialized MPI.
 * \param[in] is_low_err        Whether uses lower error rate:
 *                              from 2<sup>-80</sup> to 2<sup>-128</sup>
 *                              constraint to generate the prime.
 * \param[in] is_dh_prim        Whether makes (bn - 1)/2 also prime too.
 * \param[in] nbits             The bit number of the prime.
 * \param[in] f_rng             The RNG function to use.
 *                              This must not be \c NULL.
 * \param[in] p_rng             The RNG parameter to be passed to \p f_rng.
 *                              This may be \c NULL if \p f_rng doesn't use
 *                              a context parameter.
 * \return                      See te error code.
 */
// ceil(2^31.5)
#define CEIL_MAXUINT_DIV_SQRT2 0xb504f334U
int te_aca_bn_gen_prime(te_aca_bn_t *bn,
                        bool is_low_err,
                        bool is_dh_prim,
                        size_t nbits,
                        int (*f_rng)(void *, uint8_t *, size_t),
                        void *p_rng)
{

    int ret  = TE_SUCCESS;
    size_t k = 0, n = 0;
    int32_t rounds = 0;
    uint32_t r     = 0;
    te_aca_bn_t *Y = NULL;
    uint32_t v     = 0;
    uint8_t *buf   = NULL;
    size_t size    = 0;

    BN_CHECK(bn);
    CHECK_PARAM(f_rng);
    CHECK_PARAM(nbits >= 3);

    n    = (nbits + 31) / 32;
    size = n * 4;

    buf = osal_malloc(size);
    CHECK_COND_GO(buf, TE_ERROR_OOM);

    /* reset bn size */
    ret = _aca_bn_reset((aca_drv_ctx_t *)bn, size);
    CHECK_RET_GO;

    /* set sign to positive */
    ((aca_drv_ctx_t *)bn)->sign = BN_SIGN_POSITIVE;

    if (!is_low_err) {
        /*
         * 2^-80 error probability, number of rounds chosen per HAC, table 4.4
         */
        rounds =
            ((nbits >= 1300)
                 ? 2
                 : (nbits >= 850)
                       ? 3
                       : (nbits >= 650)
                             ? 4
                             : (nbits >= 350)
                                   ? 8
                                   : (nbits >= 250) ? 12
                                                    : (nbits >= 150) ? 18 : 27);
    } else {
        /*
         * 2^-100 error probability, number of rounds computed based on HAC,
         * fact 4.48
         */
        rounds =
            ((nbits >= 1450)
                 ? 4
                 : (nbits >= 1150)
                       ? 5
                       : (nbits >= 1000)
                             ? 6
                             : (nbits >= 850)
                                   ? 7
                                   : (nbits >= 750)
                                         ? 8
                                         : (nbits >= 500)
                                               ? 13
                                               : (nbits >= 250)
                                                     ? 28
                                                     : (nbits >= 150) ? 40
                                                                      : 51);
    }

    while (1) {
        /* generate random data */
        ret = f_rng(p_rng, buf, size);
        CHECK_COND_GO(0 == ret, TE_ERROR_GEN_RANDOM);

        ret = aca_sram_write(BN_GET_SRAM_BLOCK(bn), (const uint8_t *)buf, size);
        CHECK_RET_GO;
        /* make sure generated number is at least (nbits-1)+0.5 bits (FIPS 186-4
         * §B.3.3 steps 4.4, 5.5) */
        v = 0;
        v |= buf[0] << 24;
        v |= buf[1] << 16;
        v |= buf[2] << 8;
        v |= buf[3];
        if (v < CEIL_MAXUINT_DIV_SQRT2) {
            continue;
        }

        k = n * 32;
        if (k > nbits) {
            ret = te_aca_bn_shift_r(bn, bn, k - nbits);
            CHECK_RET_GO;
        }

        ret = aca_sram_set_bit(BN_GET_SRAM_BLOCK(bn), 0, 1);
        CHECK_RET_GO;

        if (is_dh_prim) {
            /* alloc Y */
            ret = te_aca_bn_alloc((const te_crypt_drv_t *)BN_GET_DRV(bn), size,
                                  &Y);
            CHECK_RET_GO;
            /*
             * An necessary condition for Y and X = 2Y + 1 to be prime
             * is X = 2 mod 3 (which is equivalent to Y = 2 mod 3).
             * Make sure it is satisfied, while keeping X = 3 mod 4
             */

            ret = aca_sram_set_bit(BN_GET_SRAM_BLOCK(bn), 1, 1);
            CHECK_RET_GO;

            ret = te_aca_bn_mod_u32(&r, bn, 3);
            CHECK_RET_GO;

            if (0 == r) {
                ret = te_aca_bn_add_s32(bn, bn, 8);
                CHECK_RET_GO;
            } else if (1 == r) {
                ret = te_aca_bn_add_s32(bn, bn, 4);
                CHECK_RET_GO;
            }

            /* Set Y = (X-1) / 2, which is X / 2 because X is odd */
            ret = te_aca_bn_copy(Y, bn);
            CHECK_RET_GO;
            ret = te_aca_bn_shift_r(Y, Y, 1);
            CHECK_RET_GO;

            while (1) {
                /*
                 * First, check small factors for X and Y
                 * before doing Miller-Rabin on any of them
                 */
                if (((ret = _aca_bn_check_small_factors(bn)) == TE_SUCCESS) &&
                    ((ret = _aca_bn_check_small_factors(Y)) == TE_SUCCESS) &&
                    ((ret = _aca_bn_miller_rabin(bn, rounds, f_rng, p_rng)) ==
                     TE_SUCCESS) &&
                    ((ret = _aca_bn_miller_rabin(Y, rounds, f_rng, p_rng)) ==
                     TE_SUCCESS)) {
                    ret = TE_SUCCESS;
                    goto finish;
                }

                if ((uint32_t)ret != TE_ERROR_NOT_ACCEPTABLE) {
                    goto finish;
                }

                /*
                 * Next candidates. We want to preserve Y = (X-1) / 2 and
                 * Y = 1 mod 2 and Y = 2 mod 3 (eq X = 3 mod 4 and X = 2 mod 3)
                 * so up Y by 6 and X by 12.
                 */
                ret = te_aca_bn_add_s32(bn, bn, 12);
                CHECK_RET_GO;
                ret = te_aca_bn_add_s32(Y, Y, 6);
                CHECK_RET_GO;
            }
        } else {
            ret = te_aca_bn_is_probale_prime(bn, rounds, f_rng, p_rng);
            CHECK_COND_GO(((TE_SUCCESS == ret) ||
                           (TE_ERROR_NOT_ACCEPTABLE == (unsigned int)ret)),
                          ret);
            if (TE_SUCCESS == ret) {
                goto finish;
            }
        }
    }

finish:
    te_aca_bn_free(Y);
    OSAL_SAFE_FREE(buf);
    return (ret);
}

/**
 * \brief Perform the ECP Point multiplation:
 *      (Rx, Ry, Rz) = k * (X, Y, Z)
 *
 * \note  Only support Short Weierstrass cureve:
 *      y^2 = x^3 + A x + B mod P
 *
 * \note  The input point (X, Y, Z) fromat is detected by Z == 1:
 * Z == 1: The point is used as Affine format, and mixed_affine is enabled in
 * point add.
 * Z != 1: The point is used as Jacobian format, and mixed_affine is disabled in
 * point add.
 *
 * \param[in] P     The ECP base prime P
 * \param[in] A     The A in the equation
 * \param[in] X     The X value of base point
 * \param[in] Y     The Y value of base point
 * \param[in] Z     The Z value of base point
 * \param[in] k     The multiplation value
 * \param[out] Rx   The X value of output
 * \param[out] Ry   The Y value of output
 * \param[out] Rz   The Z value of output
 * \return          See te error code.
 */
int te_aca_bn_ecp_mul(const te_aca_bn_t *P,
                      const te_aca_bn_t *A,
                      const te_aca_bn_t *X,
                      const te_aca_bn_t *Y,
                      const te_aca_bn_t *Z,
                      const te_aca_bn_t *k,
                      te_aca_bn_t *Rx,
                      te_aca_bn_t *Ry,
                      te_aca_bn_t *Rz)
{
    int ret = TE_SUCCESS;

    BN_CHECK(P);
    BN_CHECK(A);
    BN_CHECK(X);
    BN_CHECK(Y);
    BN_CHECK(Z);
    BN_CHECK(k);
    BN_CHECK_CONST_DRV(P, A);
    BN_CHECK_CONST_DRV(P, X);
    BN_CHECK_CONST_DRV(P, Y);
    BN_CHECK_CONST_DRV(P, Z);
    BN_CHECK_CONST_DRV(P, k);
    BN_CHECK_CONST_DRV(P, Rx);
    BN_CHECK_CONST_DRV(P, Ry);
    BN_CHECK_CONST_DRV(P, Rz);

    CHECK_PARAM(((aca_drv_ctx_t *)P)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)A)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)X)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)Y)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)Z)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)k)->sign == BN_SIGN_POSITIVE);

    if ((!BN_OP_CTX_IS_VALID(P)) || (!BN_OP_CTX_IS_VALID(A)) ||
        (!BN_OP_CTX_IS_VALID(X)) || (!BN_OP_CTX_IS_VALID(Y)) ||
        (!BN_OP_CTX_IS_VALID(Z)) || (!BN_OP_CTX_IS_VALID(k))) {
        return TE_ERROR_BAD_PARAMS;
    }

    ret = aca_op_ecp_mul(BN_GET_DRV(P),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(P),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(A),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(X),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(Y),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(Z),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(k),
                         BN_GET_OP_CTX(Rx),
                         BN_GET_OP_CTX(Ry),
                         BN_GET_OP_CTX(Rz));
    CHECK_RET_GO;

    ((aca_drv_ctx_t *)Rx)->sign = BN_SIGN_POSITIVE;
    ((aca_drv_ctx_t *)Ry)->sign = BN_SIGN_POSITIVE;
    ((aca_drv_ctx_t *)Rz)->sign = BN_SIGN_POSITIVE;

finish:
    return ret;
}

/**
 * \brief Perform the ECP Point add:
 *      (Rx, Ry, Rz) = (X1, Y1, Z1) + (X2, Y2, Z2)
 *
 * \note  Only support Short Weierstrass cureve:
 *      y^2 = x^3 + A x + B mod P
 *
 * \note  The point (X1, Y1, Z1) and (X2, Y2, Z2) can be
 *     jacobian foramt or affine format.
 *        For affine format, Z MUST be 1.
 *
 * \param[in] P     The ECP base prime P
 * \param[in] X1    The X value of first summand
 * \param[in] Y1    The Y value of first summand
 * \param[in] Z1    The Z value of first summand
 * \param[in] X2    The X value of second summand
 * \param[in] Y2    The Y value of second summand
 * \param[in] Z2    The Z value of second summand
 * \param[out] Rx   The X value of output
 * \param[out] Ry   The Y value of output
 * \param[out] Rz   The Z value of output
 * \return          See te error code.
 */
int te_aca_bn_ecp_add(const te_aca_bn_t *P,
                      const te_aca_bn_t *X1,
                      const te_aca_bn_t *Y1,
                      const te_aca_bn_t *Z1,
                      const te_aca_bn_t *X2,
                      const te_aca_bn_t *Y2,
                      const te_aca_bn_t *Z2,
                      te_aca_bn_t *Rx,
                      te_aca_bn_t *Ry,
                      te_aca_bn_t *Rz)
{
    int ret = TE_SUCCESS;

    BN_CHECK(P);
    BN_CHECK(X1);
    BN_CHECK(Y1);
    BN_CHECK(Z1);
    BN_CHECK(X2);
    BN_CHECK(Y2);
    BN_CHECK(Z2);
    BN_CHECK_CONST_DRV(P, X1);
    BN_CHECK_CONST_DRV(P, Y1);
    BN_CHECK_CONST_DRV(P, Z1);
    BN_CHECK_CONST_DRV(P, X2);
    BN_CHECK_CONST_DRV(P, Y2);
    BN_CHECK_CONST_DRV(P, Z2);
    BN_CHECK_CONST_DRV(P, Rx);
    BN_CHECK_CONST_DRV(P, Ry);
    BN_CHECK_CONST_DRV(P, Rz);

    CHECK_PARAM(((aca_drv_ctx_t *)P)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)X1)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)Y1)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)Z1)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)X2)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)Y2)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)Z2)->sign == BN_SIGN_POSITIVE);

    if ((!BN_OP_CTX_IS_VALID(P)) || (!BN_OP_CTX_IS_VALID(X1)) ||
        (!BN_OP_CTX_IS_VALID(Y1)) || (!BN_OP_CTX_IS_VALID(Z1)) ||
        (!BN_OP_CTX_IS_VALID(X2)) || (!BN_OP_CTX_IS_VALID(Y2)) ||
        (!BN_OP_CTX_IS_VALID(Z2))) {
        return TE_ERROR_BAD_PARAMS;
    }

    ret = aca_op_ecp_add(BN_GET_DRV(P),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(P),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(X1),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(Y1),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(Z1),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(X2),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(Y2),
                         (const aca_op_ctx_t *)BN_GET_OP_CTX(Z2),
                         BN_GET_OP_CTX(Rx),
                         BN_GET_OP_CTX(Ry),
                         BN_GET_OP_CTX(Rz));
    CHECK_RET_GO;

    ((aca_drv_ctx_t *)Rx)->sign = BN_SIGN_POSITIVE;
    ((aca_drv_ctx_t *)Ry)->sign = BN_SIGN_POSITIVE;
    ((aca_drv_ctx_t *)Rz)->sign = BN_SIGN_POSITIVE;

finish:
    return ret;
}

/**
 * \brief Conver one ECP Point from jacobian to affine:
 *      Jacobian(x, y, z) = Affine(X, Y)
 *
 * \note  Only support Short Weierstrass cureve:
 *      y^2 = x^3 + A x + B mod P
 *
 * \param[in] P     The ECP base prime P
 * \param[in] X     The X value in jacobian
 * \param[in] Y     The Y value in jacobian
 * \param[in] Z     The Z value in jacobian
 * \param[out] Rx   The X value of output
 * \param[out] Ry   The Y value of output
 * \return          See te error code.
 */

int te_aca_bn_ecp_jacobian_to_affine(const te_aca_bn_t *P,
                                     const te_aca_bn_t *X,
                                     const te_aca_bn_t *Y,
                                     const te_aca_bn_t *Z,
                                     te_aca_bn_t *Rx,
                                     te_aca_bn_t *Ry,
                                     te_aca_bn_t *Rz)
{
    int ret    = TE_SUCCESS;
    int result = 0;

    BN_CHECK(P);
    BN_CHECK(X);
    BN_CHECK(Y);
    BN_CHECK(Z);
    BN_CHECK_CONST_DRV(P, X);
    BN_CHECK_CONST_DRV(P, Y);
    BN_CHECK_CONST_DRV(P, Z);
    BN_CHECK_CONST_DRV(P, Rx);
    BN_CHECK_CONST_DRV(P, Ry);
    BN_CHECK_CONST_DRV(P, Rz);

    CHECK_PARAM(((aca_drv_ctx_t *)P)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)X)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)Y)->sign == BN_SIGN_POSITIVE);
    CHECK_PARAM(((aca_drv_ctx_t *)Z)->sign == BN_SIGN_POSITIVE);

    if ((!BN_OP_CTX_IS_VALID(P)) || (!BN_OP_CTX_IS_VALID(X)) ||
        (!BN_OP_CTX_IS_VALID(Y)) || (!BN_OP_CTX_IS_VALID(Z))) {
        return TE_ERROR_BAD_PARAMS;
    }

    ret = te_aca_bn_cmp_s32((te_aca_bn_t *)Z, 0, &result);
    CHECK_RET_GO;

    if (result == 0) {
        ret = te_aca_bn_import_s32(Rx, 1);
        CHECK_RET_GO;
        ret = te_aca_bn_import_s32(Ry, 1);
        CHECK_RET_GO;
        ret = te_aca_bn_import_s32(Rz, 0);
        CHECK_RET_GO;
    } else {
        ret = aca_ecp_op_convert_jacobian_to_affine(
            BN_GET_DRV(P),
            (const aca_op_ctx_t *)BN_GET_OP_CTX(P),
            (const aca_op_ctx_t *)BN_GET_OP_CTX(X),
            (const aca_op_ctx_t *)BN_GET_OP_CTX(Y),
            (const aca_op_ctx_t *)BN_GET_OP_CTX(Z),
            BN_GET_OP_CTX(Rx),
            BN_GET_OP_CTX(Ry),
            BN_GET_OP_CTX(Rz));
        CHECK_RET_GO;
    }
    ((aca_drv_ctx_t *)Rx)->sign = BN_SIGN_POSITIVE;
    ((aca_drv_ctx_t *)Ry)->sign = BN_SIGN_POSITIVE;
    ((aca_drv_ctx_t *)Rz)->sign = BN_SIGN_POSITIVE;

finish:
    return ret;
}

void te_aca_bn_dump(const char *name, const te_aca_bn_t *bn)
{
    if (!name) {
        name = "NULL";
    }
    OSAL_LOG_INFO("[[[[[[[[[[[ Start Dump BN: %s ]]]]]]]]]]]\n", name);
    if (!bn) {
        OSAL_LOG_INFO("      NULL Pointer!\n");
        goto finish;
    }
    if ((((aca_drv_ctx_t *)(bn))->magic) != ACA_CTX_MAGIC) {
        OSAL_LOG_INFO("      BAD BN! Magic: 0x%x\n",
                       (((aca_drv_ctx_t *)(bn))->magic));
        goto finish;
    }

    op_ctx_dump("BN OP Context", BN_GET_OP_CTX(bn));

finish:
    OSAL_LOG_INFO("[[[[[[[[[[[ End Dump BN: %s ]]]]]]]]]]]\n", name);
    return;
}
