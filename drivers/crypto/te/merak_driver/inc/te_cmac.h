//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_CMAC_H__
#define __TRUSTENGINE_CMAC_H__

#include "te_cipher.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine cmac context structure
 */
typedef te_cipher_ctx_t te_cmac_ctx_t;

/**
 * Trust engine cbcmac context structure
 */
typedef te_cipher_ctx_t te_cbcmac_ctx_t;

/**
 * \brief           This function gets the private ctx of a CMAC ctx.
 * \param[in] ctx   The CMAC context.
 * \return          The private context pointer.
 */
static inline void* cmac_priv_ctx(te_cmac_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function gets the private ctx of a CBC-MAC ctx.
 * \param[in] ctx   The CBC-MAC context.
 * \return          The private context pointer.
 */
static inline void* cbcmac_priv_ctx(te_cbcmac_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the CMAC context \p ctx. For main
 *                  algorithm of AES or SM4 only.
 * \param[out] ctx  The CMAC context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_init( te_cmac_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg );

/**
 * \brief           This function withdraws the CMAC context \p ctx.
 * \param[in] ctx   The CMAC context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_free( te_cmac_ctx_t *ctx );

/**
 * \brief           This function sets up the user key for the specified
 *                  CMAC context \p ctx.
 * \param[in] ctx     The CMAC context.
 * \param[in] key     The buffer holding the user key.
 * \param[in] keybits The CMAC key length in bit.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_cmac_setkey( te_cmac_ctx_t *ctx,
                    const uint8_t *key,
                    uint32_t keybits );

/**
 * \brief           This function sets up the secure key for the specified
 *                  CMAC context \p ctx.
 * \param[in] ctx   The CMAC context.
 * \param[in] key   The secure key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_setseckey( te_cmac_ctx_t *ctx,
                       te_sec_key_t *key );

/**
 * \brief           This function sets up the secure key of key-ladder 256 for
 *                  the specified CMAC context \p ctx. AES only.
 * \param[in] ctx   The CMAC context.
 * \param[in] key   The secure key of key ladder 256.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_setseckey_v2( te_cmac_ctx_t *ctx,
                          te_sec_key_v2_t *key );

/**
 * \brief           This function starts a CMAC computation and prepares to
 *                  authenticate the input data.
 * \param[in] ctx   The CMAC context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_start( te_cmac_ctx_t *ctx );

/**
 * \brief           This function feeds an input buffer into an ongoing CMAC
 *                  computation.
 *
 *                  It is called between te_cmac_start() or te_cmac_reset(),
 *                  and te_cmac_finish(). Can be called repeatedly.
 *
 * \param[in] ctx   The CMAC context.
 * \param[in] len   The length of the input data.
 * \param[in] in    The buffer holding the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_update( te_cmac_ctx_t *ctx,
                    size_t len,
                    const uint8_t *in );

/**
 * \brief           This function feeds a list of input buffers into an ongoing
 *                  CMAC computation.
 *
 *                  It is called between te_cmac_start() or te_cmac_reset(),
 *                  and te_cmac_finish(). Can be called repeatedly.
 *
 * \param[in] ctx   The CMAC context.
 * \param[in] in    The list of buffers holding the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_uplist( te_cmac_ctx_t *ctx,
                    te_memlist_t *in );

/**
 * \brief           This function finishes the CMAC computation and writes the
 *                  result to the mac buffer.
 *
 *                  It is called after te_cmac_update() or te_cmac_uplist().
 *                  It can be followed by te_cmac_start() or te_cmac_free().
 *
 * \param[in] ctx    The CMAC context.
 * \param[out] mac   The buffer holding the mac data.
 * \param[in] maclen The length of the mac data.
 * \return           \c TE_SUCCESS on success.
 * \return           \c <0 on failure.
 */
int te_cmac_finish( te_cmac_ctx_t *ctx,
                    uint8_t *mac,
                    uint32_t maclen );

/**
 * \brief           This function resets the CMAC computation and prepares the
 *                  authentication of another message with the same key as the
 *                  previous CMAC operation.
 *
 *                  It is called after te_cmac_update() or te_cmac_uplist().
 *
 * \param[in] ctx   The CMAC context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_reset( te_cmac_ctx_t *ctx );

/**
 * \brief           This function clones the state of a CMAC operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid CMAC context already.
 *
 * \param[in]  src  The source CMAC context.
 * \param[out] dst  The destination CMAC context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_clone( const te_cmac_ctx_t *src,
                   te_cmac_ctx_t *dst );

/**
 * \brief           This function exports partial state of the calculation.
 *                  This function dumps the entire state of the specified con-
 *                  text into a provided block of data so it can be @import 'ed
 *                  back later on. This is useful in case you want to save
 *                  partial result of the calculation after processing certain
 *                  amount of data and reload this partial result multiple
 *                  times later on for multiple re-use.
 *
 * \param[in]  ctx  The CMAC context.
 * \param[out] out  Buffer filled with the state data on success.
 * \param[inout] olen Size of \p out buffer on input.
 *                    Length of data filled in the \p out buffer on success.
 *                    Required \p out buffer length on TE_ERROR_SHORT_BUFFER.
 * \return          \c TE_SUCCESS on success.
 *                  \c TE_ERROR_SHORT_BUFFER if *olen is less than required.
 * \return          \c <0 on failure.
 */
int te_cmac_export( te_cmac_ctx_t *ctx,
                    void *out,
                    uint32_t *olen );

/**
 * \brief           This function imports partial state of the calculation.
 *                  This function loads the entire state of the specified con-
 *                  text from a provided block of data so the calculation can
 *                  continue from this point onward.
 *
 * \param[in] ctx   The CMAC context.
 * \param[in] in    Buffer filled with the state data exported early.
 * \param[in] ilen  Size of the state data in the \p in buffer.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_import( te_cmac_ctx_t *ctx,
                    const void *in,
                    uint32_t ilen );

/**
 * \brief           This function initializes the CBC-MAC context \p ctx. For
 *                  main algorithm of DES, TDES, AES, or SM4.
 * \param[out] ctx  The CBC-MAC context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_init( te_cbcmac_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg );

/**
 * \brief           This function withdraws the CBC-MAC context \p ctx.
 * \param[in] ctx   The CBC-MAC context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_free( te_cbcmac_ctx_t *ctx );

/**
 * \brief           This function sets up the user key for the specified
 *                  CBC-MAC context \p ctx.
 * \param[in] ctx     The CBC-MAC context.
 * \param[in] key     The buffer holding the user key.
 * \param[in] keybits The CBC-MAC key length in bit.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_cbcmac_setkey( te_cbcmac_ctx_t *ctx,
                      const uint8_t *key,
                      uint32_t keybits );

/**
 * \brief           This function sets up the secure key for the specified
 *                  CBC-MAC context \p ctx. Main algorithm of AES or SM4 only.
 * \param[in] ctx   The CBC-MAC context.
 * \param[in] key   The secure key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_setseckey( te_cbcmac_ctx_t *ctx,
                         te_sec_key_t *key );

/**
 * \brief           This function sets up the secure key of key-ladder 256 for
 *                  the specified CBC-MAC context \p ctx. Main algorithm of
 *                  AES only.
 * \param[in] ctx   The CBC-MAC context.
 * \param[in] key   The secure key of key-ladder 256.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_setseckey_v2( te_cbcmac_ctx_t *ctx,
                            te_sec_key_v2_t *key );

/**
 * \brief           This function starts a CBC-MAC encryption or decryption
 *                  operation.
 * \param[in] ctx   The CBC-MAC context.
 * \param[in] iv    The 128-bit initialization vector.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_start( te_cbcmac_ctx_t *ctx,
                     const uint8_t *iv );

/**
 * \brief           This function feeds an input buffer into an ongoing
 *                  CBC-MAC computation.
 *
 *                  It is called between te_cbcmac_start() or te_cbcmac_reset(),
 *                  and te_cbcmac_finish(). Can be called repeatedly.
 *
 * \param[in] ctx   The CBC-MAC context.
 * \param[in] len   The length of the input data.
 * \param[in] in    The buffer holding the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_update( te_cbcmac_ctx_t *ctx,
                      size_t len,
                      const uint8_t *in );

/**
 * \brief           This function feeds a list of input buffers into an ongoing
 *                  CBC-MAC computation.
 *
 *                  It is called between te_cbcmac_start() or te_cbcmac_reset(),
 *                  and te_cbcmac_finish(). Can be called repeatedly.
 *
 * \param[in] ctx   The CBC-MAC context.
 * \param[in] in    The list of buffers holding the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_uplist( te_cbcmac_ctx_t *ctx,
                      te_memlist_t *in );

/**
 * \brief           This function finishes the CBC-MAC computation and writes
 *                  the result to the mac buffer.
 *
 *                  The total length of the input data fed to the CBC-MAC
 *                  operation before must be multiple of 16 bytes.
 *
 *                  It is called after te_cbcmac_update() or te_cbcmac_uplist().
 *                  It can be followed by te_cbcmac_start() or te_cbcmac_free().
 *
 * \param[in] ctx    The CBC-MAC context.
 * \param[out] mac   The buffer holding the mac data.
 * \param[in] maclen The length of the mac data.
 * \return           \c TE_SUCCESS on success.
 * \return           \c <0 on failure.
 */
int te_cbcmac_finish( te_cbcmac_ctx_t *ctx,
                      uint8_t *mac,
                      uint32_t maclen );

/**
 * \brief           This function resets the CBC-MAC computation and prepares
 *                  the authentication of another message with the same key as
 *                  the previous CBC-MAC operation.
 *
 *                  It is called after te_cbcmac_update() or te_cbcmac_uplist().
 *
 * \param[in] ctx   The CBC-MAC context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_reset( te_cbcmac_ctx_t *ctx );

/**
 * \brief           This function clones the state of a CBC-MAC operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid CBC-MAC context already.
 *
 * \param[in]  src  The source CBC-MAC context.
 * \param[out] dst  The destination CBC-CMAC context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_clone( const te_cbcmac_ctx_t *src,
                     te_cbcmac_ctx_t *dst );

/**
 * \brief           This function exports partial state of the calculation.
 *                  This function dumps the entire state of the specified con-
 *                  text into a provided block of data so it can be @import 'ed
 *                  back later on. This is useful in case you want to save
 *                  partial result of the calculation after processing certain
 *                  amount of data and reload this partial result multiple
 *                  times later on for multiple re-use.
 *
 * \param[in]  ctx  The CBC-MAC context.
 * \param[out] out  Buffer filled with the state data on success.
 * \param[inout] olen Size of \p out buffer on input.
 *                    Length of data filled in the \p out buffer on success.
 *                    Required \p out buffer length on TE_ERROR_SHORT_BUFFER.
 * \return          \c TE_SUCCESS on success.
 *                  \c TE_ERROR_SHORT_BUFFER if *olen is less than required.
 * \return          \c <0 on failure.
 */
int te_cbcmac_export( te_cbcmac_ctx_t *ctx,
                      void *out,
                      uint32_t *olen );

/**
 * \brief           This function imports partial state of the calculation.
 *                  This function loads the entire state of the specified con-
 *                  text from a provided block of data so the calculation can
 *                  continue from this point onward.
 *
 * \param[in] ctx   The CBC-MAC context.
 * \param[in] in    Buffer filled with the state data exported early.
 * \param[in] ilen  Size of the state data in the \p in buffer.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_import( te_cbcmac_ctx_t *ctx,
                      const void *in,
                      uint32_t ilen );

/**
 * asynchronous cipher based MAC operations
 */
#ifdef CFG_TE_ASYNC_EN
/**
 * cipher based MAC async request structure
 */
typedef struct te_cmac_request {
    te_async_request_t base;        /**< base request */
    int res;                        /**< result */
    union {
        /**
         * start params
         */
        struct {
            const uint8_t *iv;      /**< initial vector */
        } st;

        /**
         * update params
         */
        struct {
            te_memlist_t in;        /**< input buffer list */
        } up;

        /**
         * finish params
         */
        struct {
            uint8_t *mac;           /**< mac buffer */
            uint32_t maclen;        /**< the length of mac */
        } fin;

        /**
         * async mac params
         */
        struct {
            const uint8_t *iv;      /**< cbcmac iv */
            te_key_wrap_t key;      /**< cipher key */
            te_memlist_t in;        /**< input buffer list */
            uint8_t *mac;           /**< mac buffer */
            uint32_t maclen;        /**< the length of mac */
        } amac;
    };
} te_cmac_request_t;

/**
 * \brief           This function starts asynchronous CMAC computation and
 *                  prepares to authenticate the input data.
 *
 * \param[in] ctx   The CMAC context.
 * \param[in] req   The asynchronous requset instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_astart( te_cmac_ctx_t *ctx,  te_cmac_request_t *req );

/**
 * \brief           This function feeds an input buffer into an ongoing CMAC
 *                  computation, asynchronously.
 *
 * \param[in] ctx   The CMAC context.
 * \param[in] req   The asynchronous requset instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cmac_aupdate( te_cmac_ctx_t *ctx, te_cmac_request_t *req );

/**
 * \brief           This function finishes the CMAC computation and writes the
 *                  result to the requset buffer, asynchronously.
 *
 * \param[in] ctx    The CMAC context.
 * \param[in] req   The asynchronous requset instance.
 * \return           \c TE_SUCCESS on success.
 * \return           \c <0 on failure.
 */
int te_cmac_afinish( te_cmac_ctx_t *ctx, te_cmac_request_t *req );

/**
 * \brief           This function performs the CMAC computation on the input
 *                  buffer specified in \p req, and generates the resulted
 *                  mac data. Supports main algorithm of AES or SM4.
 *
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_acmac( te_drv_handle hdl,
              te_algo_t malg,
              te_cmac_request_t *req );

/**
 * \brief           This function starts an asynchronous CBC-MAC encryption
 *                  or decryption operation.
 *
 * \param[in] ctx   The CBC-MAC context.
 * \param[in] req   The asynchronous requset instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_astart( te_cbcmac_ctx_t *ctx, te_cmac_request_t *req );

/**
 * \brief           This function feeds an ongoing CBC-MAC computation,
 *                  asynchronously.
 *
 * \param[in] ctx   The CBC-MAC context.
 * \param[in] req   The asynchronous requset instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_aupdate( te_cbcmac_ctx_t *ctx, te_cmac_request_t *req );

/**
 * \brief           This function asynchronously finishes the CBC-MAC
 *                  computation and writes the result to the mac buffer
 *                  of asynchronous requset.
 *
 * \param[in] ctx   The CBC-MAC context.
 * \param[in] req   The asynchronous requset instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_cbcmac_afinish( te_cbcmac_ctx_t *ctx, te_cmac_request_t *req );

/**
 * \brief           This function performs the CBC-MAC computation on the input
 *                  buffer specified in \p req, and generates the resulted
 *                  mac data. Supports main algorithm of DES, TDES, AES or SM4.
 *
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_acbcmac( te_drv_handle hdl,
                te_algo_t malg,
                te_cmac_request_t *req );

#endif /* CFG_TE_ASYNC_EN */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_CMAC_H__ */
