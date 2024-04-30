//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DES_H__
#define __TRUSTENGINE_DES_H__

#include "te_cipher.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * \brief           This function initializes the DES context \p ctx.
 * \param[out] ctx  The DES context.
 * \param[in] hdl   The driver handler.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_des_init( te_cipher_ctx_t *ctx, te_drv_handle hdl )
{
    return te_cipher_init(ctx, hdl, TE_MAIN_ALGO_DES);
}

/**
 * \brief           This function withdraws the DES context \p ctx.
 * \param[in] ctx   The DES context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_des_free( te_cipher_ctx_t *ctx )
{
    return te_cipher_free(ctx);
}

/**
 * \brief           This function sets up the user key for the specified
 *                  DES context \p ctx.
 * \param[in] ctx   The DES cipher context.
 * \param[in] key   The buffer holding the 64-bit user key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_des_setkey( te_cipher_ctx_t *ctx,
                                 const uint8_t key[8] )
{
    return te_cipher_setkey(ctx, key, 64);
}

/**
 * \brief           This function performs an DES-ECB encryption or decryption
 *                  operation.
 * \param[in]  ctx  The DES cipher context.
 * \param[in]  op   Operation mode.
 * \param[in]  len  The length of input data. Must be multiple of 8.
 * \param[in]  in   The buffer holding the input data.
 * \param[out] out  The buffer holding th output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_des_ecb( te_cipher_ctx_t *ctx,
                              te_sca_operation_t op,
                              size_t len,
                              const uint8_t *in,
                              uint8_t *out )
{
    ctx->crypt->alg = TE_ALG_DES_ECB_NOPAD;
    return te_cipher_ecb(ctx, op, len, in, out);
}

/**
 * \brief           This function performs an DES-ECB encryption or decryption
 *                  operation.
 * \param[in] ctx   The DES cipher context.
 * \param[in] op    Operation mode.
 * \param[in] in    The buffer holding the input data built as memory link
                    list and it's total length must be multiple of 8.
 * \param[in] out   The buffer holding th output data built as memory link
                    list.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_des_ecb_list( te_cipher_ctx_t *ctx,
                                   te_sca_operation_t op,
                                   te_memlist_t *in,
                                   te_memlist_t *out )
{
    ctx->crypt->alg = TE_ALG_DES_ECB_NOPAD;
    return te_cipher_ecb_list(ctx, op, in, out);
}

/**
 * \brief           This function performs an DES-CBC encryption or decryption
 *                  operation.
 * \param[in]  ctx   The DES cipher context.
 * \param[in]  op    Operation mode.
 * \param[in]  len   The length of input data. Must be multiple of 8.
 * \param[in]  iv    The initialization vector (updated after use).
 * \param[in]  in    The buffer holding the input data.
 * \param[out] out   The buffer holding the output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_des_cbc( te_cipher_ctx_t *ctx,
                              te_sca_operation_t op,
                              size_t len,
                              uint8_t iv[8],
                              const uint8_t *in,
                              uint8_t *out )
{
    ctx->crypt->alg = TE_ALG_DES_CBC_NOPAD;
    return te_cipher_cbc(ctx, op, len, iv, in, out);
}

/**
 * \brief           This function performs an DES-CBC encryption or decryption
 *                  operation.
 * \param[in]  ctx   The DES cipher context.
 * \param[in]  op    Operation mode.
 * \param[in]  iv    The initialization vector (updated after use).
 * \param[in]  in    The buffer holding the input data built as memory link
                     list and it's total length must be multiple of 8.
 * \param[out] out   The buffer holding the output data built as memory link
                     list.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_des_cbc_list( te_cipher_ctx_t *ctx,
                                   te_sca_operation_t op,
                                   uint8_t iv[8],
                                   te_memlist_t *in,
                                   te_memlist_t *out )
{
    ctx->crypt->alg = TE_ALG_DES_CBC_NOPAD;
    return te_cipher_cbc_list(ctx, op, iv, in, out);
}

/**
 * \brief           This function clones the state of a DES operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid DES context already.
 *
 * \param[in]  src  The source DES context.
 * \param[out] dst  The destination DES context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_des_clone( const te_cipher_ctx_t *src,
                                te_cipher_ctx_t *dst )
{
    return te_cipher_clone(src, dst);
}

/**
 * \brief           This function initializes the TDES context \p ctx.
 * \param[out] ctx  The TDES context.
 * \param[in]  hdl  The driver handler.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_tdes_init( te_cipher_ctx_t *ctx, te_drv_handle hdl )
{
    return te_cipher_init(ctx, hdl, TE_MAIN_ALGO_TDES);
}

/**
 * \brief           This function withdraws the TDES context \p ctx.
 * \param[in] ctx   The TDES context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_tdes_free( te_cipher_ctx_t *ctx )
{
    return te_cipher_free(ctx);
}

/**
 * \brief           This function sets up the user key for the specified
 *                  TDES context \p ctx.
 * \param[in] ctx   The TDES cipher context.
 * \param[in] key   The buffer holding the 192-bit user key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_tdes_setkey( te_cipher_ctx_t *ctx,
                                  const uint8_t key[24])
{
    return te_cipher_setkey(ctx, key, 192);
}

/**
 * \brief           This function performs an TDES-ECB encryption or decryption
 *                  operation.
 * \param[in]  ctx  The TDES cipher context.
 * \param[in]  op   Operation mode.
 * \param[in]  len  The length of input data. Must be multiple of 8.
 * \param[in]  in   The buffer holding the input data.
 * \param[out] out  The buffer holding th output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_tdes_ecb( te_cipher_ctx_t *ctx,
                               te_sca_operation_t op,
                               size_t len,
                               const uint8_t *in,
                               uint8_t *out )
{
    ctx->crypt->alg = TE_ALG_TDES_ECB_NOPAD;
    return te_cipher_ecb(ctx, op, len, in, out);
}

/**
 * \brief           This function performs an TDES-ECB encryption or decryption
 *                  operation.
 * \param[in]  ctx  The TDES cipher context.
 * \param[in]  op   Operation mode.
 * \param[in]  in   The buffer holding the input data built as memory link
                    list and it's total length must be multiple of 8.
 * \param[out] out  The buffer holding th output data built as memory link
                    list.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_tdes_ecb_list( te_cipher_ctx_t *ctx,
                                    te_sca_operation_t op,
                                    te_memlist_t *in,
                                    te_memlist_t *out )
{
    ctx->crypt->alg = TE_ALG_TDES_ECB_NOPAD;
    return te_cipher_ecb_list(ctx, op, in, out);
}

/**
 * \brief           This function performs an TDES-CBC encryption or decryption
 *                  operation.
 * \param[in]  ctx  The TDES cipher context.
 * \param[in]  op   Operation mode.
 * \param[in]  len  The length of input data. Must be multiple of 8.
 * \param[in]  iv   The 64-bit initialization vector (updated after use).
 * \param[in]  in   The buffer holding the input data.
 * \param[out] out  The buffer holding the output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_tdes_cbc( te_cipher_ctx_t *ctx,
                               te_sca_operation_t op,
                               size_t len,
                               uint8_t iv[8],
                               const uint8_t *in,
                               uint8_t *out )
{
    ctx->crypt->alg = TE_ALG_TDES_CBC_NOPAD;
    return te_cipher_cbc(ctx, op, len, iv, in, out);
}

/**
 * \brief           This function performs an TDES-CBC encryption or decryption
 *                  operation.
 * \param[in]  ctx  The TDES cipher context.
 * \param[in]  op   Operation mode.
 * \param[in]  iv   The 64-bit initialization vector (updated after use).
 * \param[in]  in   The buffer holding the input data built as memory link
                    list and it's total length must be multiple of 8.
 * \param[out] out  The buffer holding the output data built as memory link
                    list.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_tdes_cbc_list( te_cipher_ctx_t *ctx,
                                    te_sca_operation_t op,
                                    uint8_t iv[8],
                                    te_memlist_t *in,
                                    te_memlist_t *out )
{
    ctx->crypt->alg = TE_ALG_TDES_CBC_NOPAD;
    return te_cipher_cbc_list(ctx, op, iv, in, out);
}

/**
 * \brief           This function clones the state of a TDES operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid TDES context already.
 *
 * \param[in]  src  The source TDES context.
 * \param[out] dst  The destination TDES context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_tdes_clone( const te_cipher_ctx_t *src,
                                 te_cipher_ctx_t *dst )
{
    return te_cipher_clone(src, dst);
}

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_DES_H__ */
