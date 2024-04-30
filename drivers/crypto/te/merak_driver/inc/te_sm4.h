//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_SM4_H__
#define __TRUSTENGINE_SM4_H__

#include "te_cipher.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * \brief           This function initializes the SM4 context \p ctx.
 * \param[out] ctx  The SM4 context.
 * \param[in] hdl   The driver handler.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_sm4_init( te_cipher_ctx_t *ctx, te_drv_handle hdl )
{
    return te_cipher_init(ctx, hdl, TE_MAIN_ALGO_SM4);
}

/**
 * \brief           This function withdraws the SM4 context \p ctx.
 * \param[in] ctx   The SM4 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_sm4_free( te_cipher_ctx_t *ctx )
{
    return te_cipher_free(ctx);
}

/**
 * \brief           This function sets up the user key for the specified
 *                  SM4 context \p ctx.
 * \param[in] ctx   The SM4 cipher context.
 * \param[in] key   The buffer holding the 128-bit user key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_sm4_setkey( te_cipher_ctx_t *ctx,
                                 const uint8_t key[16] )
{
    return te_cipher_setkey(ctx, key, 128);
}

/**
 * \brief           This function sets up the secure key for the specified
 *                  SM4 context \p ctx.
 * \param[in] ctx   The SM4 cipher context.
 * \param[in] key   The secure key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_sm4_setseckey( te_cipher_ctx_t *ctx,
                                    te_sec_key_t *key )
{
    return te_cipher_setseckey(ctx, key);
}

/**
 * \brief           This function performs an SM4-ECB encryption or decryption
 *                  operation.
 * \param[in] ctx   The SM4 cipher context.
 * \param[in] op    Operation mode.
 * \param[in] len   The length of input data. Must be multiple of 16.
 * \param[in] in    The buffer holding the input data.
 * \param[out] out  The buffer holding th output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_sm4_ecb( te_cipher_ctx_t *ctx,
                              te_sca_operation_t op,
                              size_t len,
                              const uint8_t *in,
                              uint8_t *out )
{
    ctx->crypt->alg = TE_ALG_SM4_ECB_NOPAD;
    return te_cipher_ecb(ctx, op, len, in, out);
}

/**
 * \brief           This function performs an SM4-ECB encryption or decryption
 *                  operation.
 * \param[in] ctx   The SM4 cipher context.
 * \param[in] op    Operation mode.
 * \param[in] in    The buffer holding the input data built as memory link
                    list and it's total length must be multiple of 8.
 * \param[out] out  The buffer holding th output data built as memory link
                    list.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_sm4_ecb_list( te_cipher_ctx_t *ctx,
                                   te_sca_operation_t op,
                                   te_memlist_t *in,
                                   te_memlist_t *out )
{
    ctx->crypt->alg = TE_ALG_SM4_ECB_NOPAD;
    return te_cipher_ecb_list(ctx, op, in, out);
}

/**
 * \brief           This function performs an SM4-CBC encryption or decryption
 *                  operation.
 * \param[in] ctx   The SM4 cipher context.
 * \param[in] op    Operation mode.
 * \param[in] len   The length of input data. Must be multiple of 16.
 * \param[in] iv    The 128-bit initialization vector (updated after use).
 * \param[in] in    The buffer holding the input data.
 * \param[out] out  The buffer holding the output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_sm4_cbc( te_cipher_ctx_t *ctx,
                              te_sca_operation_t op,
                              size_t len,
                              uint8_t *iv,
                              const uint8_t *in,
                              uint8_t *out )
{
    ctx->crypt->alg = TE_ALG_SM4_CBC_NOPAD;
    return te_cipher_cbc(ctx, op, len, iv, in, out);
}

/**
 * \brief           This function performs an SM4-CBC encryption or decryption
 *                  operation.
 * \param[in] ctx   The SM4 cipher context.
 * \param[in] op    Operation mode.
 * \param[in] iv    The 128-bit initialization vector (updated after use).
 * \param[in] in    The buffer holding the input data built as memory link
                    list and it's total length must be multiple of 8.
 * \param[out] out  The buffer holding the output data built as memory link
                    list.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_sm4_cbc_list( te_cipher_ctx_t *ctx,
                                   te_sca_operation_t op,
                                   uint8_t *iv,
                                   te_memlist_t *in,
                                   te_memlist_t *out )
{
    ctx->crypt->alg = TE_ALG_SM4_CBC_NOPAD;
    return te_cipher_cbc_list(ctx, op, iv, in, out);
}

/**
 * \brief           This function performs an SM4-OFB encryption or decryption
 *                  operation.
 * \param[in] ctx       The SM4 cipher context.
 * \param[in] len       The length of the input data.
 * \param[inout] iv_off The offset in the current \p iv, for resuming within
 *                      the current cipher stream. The offset pointer should
 *                      be 0 at the start of a stream.
 * \param[inout] iv     The 128-bit initialization vector (updated after use).
 * \param[in] in        The buffer holding the input data.
 * \param[out] out      The buffer holding the output data.
 * \return              \c TE_SUCCESS on success.
 * \return              \c <0 on failure.
 */
static inline int te_sm4_ofb( te_cipher_ctx_t *ctx,
                              size_t len,
                              size_t *iv_off,
                              uint8_t *iv,
                              const uint8_t *in,
                              uint8_t *out )
{
    ctx->crypt->alg = TE_ALG_SM4_OFB;
    return te_cipher_ofb(ctx, len, iv_off, iv, in, out);
}

/**
 * \brief           This function performs an SM4-OFB encryption or decryption
 *                  operation.
 * \param[in] ctx       The SM4 cipher context.
 * \param[inout] iv_off The offset in the current \p iv, for resuming within
 *                      the current cipher stream. The offset pointer should
 *                      be 0 at the start of a stream.
 * \param[inout] iv     The 128-bit initialization vector (updated after use).
 * \param[in] in        The buffer holding the input data built as memory link
                        list and it's total length must be multiple of 8.
 * \param[out] out      The buffer holding the output data built as memory link
                        list.
 * \return              \c TE_SUCCESS on success.
 * \return              \c <0 on failure.
 */
static inline int te_sm4_ofb_list( te_cipher_ctx_t *ctx,
                                   size_t *iv_off,
                                   uint8_t *iv,
                                   te_memlist_t *in,
                                   te_memlist_t *out )
{
    ctx->crypt->alg = TE_ALG_SM4_OFB;
    return te_cipher_ofb_list(ctx, iv_off, iv, in, out);
}

/**
 * \brief           This function performs an SM4-CTR encryption or decryption
 *                  operation.
 * \param[in] ctx              The SM4 cipher context.
 * \param[in] len              The length of the input data.
 * \param[inout] nc_off        The offset in the current \p stream_block, for
 *                             resuming within the current cipher stream. The
 *                             offset pointer should be 0 at the start of a
 *                             stream.
 * \param[inout] nonce_counter The nonce and counter (updated after use).
 * \param[inout] stream_block  The saved stream block for resuming. This is
 *                             overwritten by the function.
 * \param[in] in               The buffer holding the output data.
 * \param[out] out             The buffer holding the output data.
 * \return                     \c TE_SUCCESS on success.
 * \return                     \c <0 on failure.
 */
static inline int te_sm4_ctr( te_cipher_ctx_t *ctx,
                              size_t len,
                              size_t *nc_off,
                              uint8_t *nonce_counter,
                              uint8_t *stream_block,
                              const uint8_t *in,
                              uint8_t *out )
{
    ctx->crypt->alg = TE_ALG_SM4_CTR;
    return te_cipher_ctr(ctx, len, nc_off, nonce_counter,
                         stream_block, in, out);
}

/**
 * \brief           This function performs an SM4-CTR encryption or decryption
 *                  operation.
 * \param[in] ctx              The SM4 cipher context.
 * \param[inout] nc_off        The offset in the current \p stream_block, for
 *                             resuming within the current cipher stream. The
 *                             offset pointer should be 0 at the start of a
 *                             stream.
 * \param[inout] nonce_counter The nonce and counter (updated after use).
 * \param[inout] stream_block  The saved stream block for resuming. This is
 *                             overwritten by the function.
 * \param[in] in               The buffer holding the of input data built as
                               memory link list and it's total length Must
                               be multiple of 8.
 * \param[out] out             The buffer holding the output data built as
                               memory link list.
 * \return                     \c TE_SUCCESS on success.
 * \return                     \c <0 on failure.
 */
static inline int te_sm4_ctr_list( te_cipher_ctx_t *ctx,
                                   size_t *nc_off,
                                   uint8_t *nonce_counter,
                                   uint8_t *stream_block,
                                   te_memlist_t *in,
                                   te_memlist_t *out )
{
    ctx->crypt->alg = TE_ALG_SM4_CTR;
    return te_cipher_ctr_list( ctx, nc_off, nonce_counter,
                               stream_block, in, out );
}

/**
 * \brief           This function clones the state of a SM4 operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid SM4 context already.
 *
 * \param[in]  src  The source SM4 context.
 * \param[out] dst  The destination SM4 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_sm4_clone( const te_cipher_ctx_t *src,
                                te_cipher_ctx_t *dst )
{
    return te_cipher_clone(src, dst);
}

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_SM4_H__ */
