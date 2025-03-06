//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_CIPHER_H__
#define __TRUSTENGINE_CIPHER_H__

#include "driver/te_drv_sca.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine cipher context structure
 */
typedef struct te_cipher_ctx {
    te_crypt_ctx_t *crypt;
} te_cipher_ctx_t;

/**
 * \brief           This function gets the private ctx of a cipher ctx.
 * \param[in] ctx   The cipher context.
 * \return          The private context pointer.
 */
static inline void* cipher_priv_ctx(te_cipher_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the cipher context \p ctx.
 * \param[out] ctx  The cipher context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_init( te_cipher_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg );

/**
 * \brief           This function withdraws the cipher context \p ctx.
 * \param[in] ctx   The cipher context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_free( te_cipher_ctx_t *ctx );

/**
 * \brief           This function sets up the user key for the specified
 *                  cipher context \p ctx.
 * \param[in] ctx     The cipher context.
 * \param[in] key     The buffer holding the user key.
 * \param[in] keybits The cipher key length in bit.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_cipher_setkey( te_cipher_ctx_t *ctx,
                      const uint8_t *key,
                      uint32_t keybits );

/**
 * \brief           This function sets up the secure key for the specified
 *                  cipher context \p ctx. Main algorithm of AES or SM4 only.
 * \param[in] ctx   The cipher context.
 * \param[in] key   The secure key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_setseckey( te_cipher_ctx_t *ctx,
                         te_sec_key_t *key );

/**
 * \brief           This function sets up the secure key of key-ladder 256 for the
 *                  specified cipher context \p ctx. Main algorithm of AES only.
 * \param[in] ctx   The cipher context.
 * \param[in] key   The secure key of key-ladder 256.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_setseckey_v2( te_cipher_ctx_t *ctx,
                            te_sec_key_v2_t *key );

/**
 * \brief           This function dumps the key ladder derived key (CM and DM
 *                  only). For main algorithm of AES or SM4 only.
 * \param[in] ctx     The cipher context.
 * \param[out] key    The buffer to hold the derived key.
 * \param[in] keybits Key length in bit, either 128 or 256.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_cipher_getseckey( te_cipher_ctx_t *ctx,
                         uint8_t *key,
                         uint32_t keybits );

/**
 * \brief           This function performs an ECB encryption or decryption
 *                  operation.
 * \param[in] ctx   The cipher context.
 * \param[in] op    Operation mode.
 * \param[in] len   The length of input data. Must be multiple of block size.
 * \param[in] in    The buffer holding the input data.
 * \param[out] out  The buffer holding the output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_ecb( te_cipher_ctx_t *ctx,
                   te_sca_operation_t op,
                   size_t len,
                   const uint8_t *in,
                   uint8_t *out );

/**
 * \brief           This function performs an ECB encryption or decryption
 *                  operation.
 * \param[in] ctx   The cipher context.
 * \param[in] op    Operation mode.
 * \param[in] in    The memory list holding the input data.
 * \param[out] out  The memory list holding the output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_ecb_list( te_cipher_ctx_t *ctx,
                   te_sca_operation_t op,
                   te_memlist_t *in,
                   te_memlist_t *out );

/**
 * \brief           This function performs an CBC encryption or decryption
 *                  operation.
 * \param[in] ctx   The cipher context.
 * \param[in] op    Operation mode.
 * \param[in] len   The length of input data. Must be multiple of block size.
 * \param[inout] iv The initialization vector (updated after use).
 * \param[in] in    The buffer holding the input data.
 * \param[out] out  The buffer holding the output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_cbc( te_cipher_ctx_t *ctx,
                   te_sca_operation_t op,
                   size_t len,
                   uint8_t *iv,
                   const uint8_t *in,
                   uint8_t *out );

/**
 * \brief           This function performs an CBC encryption or decryption
 *                  operation.
 * \param[in] ctx   The cipher context.
 * \param[in] op    Operation mode.
 * \param[in] len   The length of input data. Must be multiple of block size.
 * \param[inout] iv The initialization vector (updated after use).
 * \param[in] in    The memory list holding the input data.
 * \param[out] out  The memory list holding the output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_cbc_list( te_cipher_ctx_t *ctx,
                   te_sca_operation_t op,
                   uint8_t *iv,
                   te_memlist_t *in,
                   te_memlist_t *out );

/**
 * \brief           This function performs an OFB encryption or decryption
 *                  operation. For main algorithm of AES or SM4 only.
 * \param[in] ctx       The cipher context.
 * \param[in] len       The length of the input data.
 * \param[inout] iv_off The offset in the current \p iv, for resuming within
 *                      the current cipher stream. The offset pointer should
 *                      be 0 at the start of a stream.
 * \param[inout] iv     The initialization vector (updated after use).
 * \param[in] in        The buffer holding the input data.
 * \param[out] out      The buffer holding the output data.
 * \return              \c TE_SUCCESS on success.
 * \return              \c <0 on failure.
 */
int te_cipher_ofb( te_cipher_ctx_t *ctx,
                   size_t len,
                   size_t *iv_off,
                   uint8_t *iv,
                   const uint8_t *in,
                   uint8_t *out );

/**
 * \brief           This function performs an OFB encryption or decryption
 *                  operation. For main algorithm of AES or SM4 only.
 * \param[in] ctx       The cipher context.
 * \param[inout] iv_off The offset in the current \p iv, for resuming within
 *                      the current cipher stream. The offset pointer should
 *                      be 0 at the start of a stream.
 * \param[inout] iv     The initialization vector (updated after use).
 * \param[in] in        The memory list holding the input data.
 * \param[out] out      The memory list holding the output data.
 * \return              \c TE_SUCCESS on success.
 * \return              \c <0 on failure.
 */
int te_cipher_ofb_list( te_cipher_ctx_t *ctx,
                   size_t *iv_off,
                   uint8_t *iv,
                   te_memlist_t *in,
                   te_memlist_t *out );

/**
 * \brief           This function performs an CTR encryption or decryption
 *                  operation. For main algorithm of AES or SM4 only.
 * \param[in] ctx              The cipher context.
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
int te_cipher_ctr( te_cipher_ctx_t *ctx,
                   size_t len,
                   size_t *nc_off,
                   uint8_t *nonce_counter,
                   uint8_t *stream_block,
                   const uint8_t *in,
                   uint8_t *out );

/**
 * \brief           This function performs an CTR encryption or decryption
 *                  operation. For main algorithm of AES or SM4 only.
 * \param[in] ctx              The cipher context.
 * \param[in] len              The length of the input data.
 * \param[inout] nc_off        The offset in the current \p stream_block, for
 *                             resuming within the current cipher stream. The
 *                             offset pointer should be 0 at the start of a
 *                             stream.
 * \param[inout] nonce_counter The nonce and counter (updated after use).
 * \param[inout] stream_block  The saved stream block for resuming. This is
 *                             overwritten by the function.
 * \param[in] in               The memory list holding the output data.
 * \param[out] out             The memory list holding the output data.
 * \return                     \c TE_SUCCESS on success.
 * \return                     \c <0 on failure.
 */
int te_cipher_ctr_list( te_cipher_ctx_t *ctx,
                   size_t *nc_off,
                   uint8_t *nonce_counter,
                   uint8_t *stream_block,
                   te_memlist_t *in,
                   te_memlist_t *out );

/**
 * \brief           This function clones the state of a cipher operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid cipher context already.
 *
 * \param[in]  src  The source cipher context.
 * \param[out] dst  The destination cipher context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_clone( const te_cipher_ctx_t *src,
                     te_cipher_ctx_t *dst );

/**
 * asynchronous cipher operations
 */
#ifdef CFG_TE_ASYNC_EN

/**
 * cipher asynchronous request structure
 */
typedef struct te_cipher_request {
    te_async_request_t base; /**< base async request */
    te_sca_operation_t op;   /**< operation mode */
    uint8_t *iv;             /**< initial vector or nonce(CTR) */
    uint8_t *stream;         /**< stream block (CTR) */
    size_t *off;             /**< offset of iv (OFB) or stream (CTR) */
    te_memlist_t src;
    te_memlist_t dst;
    int res;
    void *priv[];            /**< private context */
} te_cipher_request_t;

/**
 * \brief           This function obtains the cipher private async context size.
 * \return          \c Private context size in bytes.
 */
int te_cipher_get_async_ctx_size(void);

/**
 * \brief           This function performs an asynchronous ECB encryption or
 *                  decryption operation.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_aecb( te_cipher_ctx_t *ctx,
                    te_cipher_request_t *req );

/**
 * \brief           This function performs an asynchronous CBC encryption or
 *                  decryption operation.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_acbc( te_cipher_ctx_t *ctx,
                    te_cipher_request_t *req );

/**
 * \brief           This function performs an asynchronous OFB encryption or
 *                  decryption operation. AES or SM4 only.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_aofb( te_cipher_ctx_t *ctx,
                    te_cipher_request_t *req );

/**
 * \brief           This function performs an asynchronous CTR encryption or
 *                  decryption operation. AES or SM4 only.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cipher_actr( te_cipher_ctx_t *ctx,
                    te_cipher_request_t *req );

#endif /* CFG_TE_ASYNC_EN */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_CIPHER_H__ */
