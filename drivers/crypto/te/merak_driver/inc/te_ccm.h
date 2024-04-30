//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_CCM_H__
#define __TRUSTENGINE_CCM_H__

#include "te_cipher.h"
#include "te_cmac.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * CCM state enumeration
 */
typedef enum te_ccm_state {
    TE_CCM_STATE_RAW = 0,
    TE_CCM_STATE_INIT,
    TE_CCM_STATE_READY,
    TE_CCM_STATE_START,
    TE_CCM_STATE_UPDATE,
} te_ccm_state_t;

/**
 * CCM context structure
 */
typedef struct te_ccm_ctx {
    te_crypt_ctx_t *crypt;
} te_ccm_ctx_t;

/**
 * \brief           This function gets the private ctx of a CCM ctx.
 * \param[in] ctx   The CCM context.
 * \return          The private context pointer.
 */
static inline void* ccm_priv_ctx(te_ccm_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the CCM context \p ctx. For main
 *                  algorithm of AES or SM4 only.
 * \param[out] ctx  The CCM context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ccm_init( te_ccm_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg );

/**
 * \brief           This function withdraws the CCM context \p ctx.
 * \param[in] ctx   The CCM context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ccm_free( te_ccm_ctx_t *ctx );

/**
 * \brief           This function sets up the user key for the specified
 *                  CCM context \p ctx.
 * \param[in] ctx     The CCM context.
 * \param[in] key     The buffer holding the user key.
 * \param[in] keybits The CCM key length in bit.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_ccm_setkey( te_ccm_ctx_t *ctx,
                   const uint8_t *key,
                   uint32_t keybits );

/**
 * \brief           This function sets up the secure key for the specified
 *                  CCM context \p ctx.
 * \param[in] ctx   The CCM context.
 * \param[in] key   The secure key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ccm_setseckey( te_ccm_ctx_t *ctx,
                      te_sec_key_t *key );

/**
 * \brief           This function sets up the secure key of key-ladder 256 for
 *                  the specified CCM context \p ctx.
 * \param[in] ctx   The CCM context.
 * \param[in] key   The secure key of key-ladder 256.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ccm_setseckey_v2( te_ccm_ctx_t *ctx,
                         te_sec_key_v2_t *key );
/**
 * \brief           This function performs a CCM authentication encryption or
 *                  decryption operation.
 * \param[in] ctx           The CCM context.
 * \param[in] op            Operation mode.
 * \param[in] nonce         The nonce.
 * \param[in] nlen          The length of the nonce, range from 7 to 13.
 * \param[in] aadlen        The length of the AAD data.
 * \param[in] payload_len   The length of input data.
 * \return           \c TE_SUCCESS on success.
 * \return           \c <0 on failure.
 */
int te_ccm_start( te_ccm_ctx_t *ctx,
                  te_sca_operation_t op,
                  const uint8_t *nonce,
                  uint32_t nonce_len,
                  uint32_t tag_len,
                  uint64_t aad_len,
                  uint64_t payload_len );
/**
 * \brief           This function feeds an input of associated data into an ongoing CCM
 *                  encryption or decryption operation.
 * \param[in] ctx   The CCM context.
 * \param[in] data  The buffer holding the input associated data.
 * \param[in] len   The length of the input associated data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ccm_update_aad( te_ccm_ctx_t *ctx,
                       const uint8_t *data,
                       size_t len );
/**
 * \brief           This function feeds an input of associated list data into an ongoing CCM
 *                  encryption or decryption operation.
 * \param[in] ctx   The CCM context.
 * \param[in] data  The list buffer holding the input associated data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ccm_uplist_aad( te_ccm_ctx_t *ctx,
                       te_memlist_t *data );
/**
 * \brief           This function feeds an input data into an ongoing CCM
 *                  encryption or decryption operation.
 * \param[in] ctx   The CCM context.
 * \param[in] len   The length of the input data.
 * \param[in] in    The buffer holding the input data.
 * \param[out] out  The buffer holding the output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ccm_update( te_ccm_ctx_t *ctx,
                   size_t len,
                   const uint8_t *in,
                   uint8_t *out );
/**
 * \brief           This function feeds an input list data into an ongoing CCM
 *                  encryption or decryption operation.
 * \param[in] ctx   The CCM context.
 * \param[in] len   The length of the input data.
 * \param[in] in    The list holding the input data.
 * \param[out] out  The list holding the output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ccm_uplist( te_ccm_ctx_t *ctx,
                   te_memlist_t *in,
                   te_memlist_t *out );
/**
 * \brief           This function finishes the CCM operation and checks
 *                  (generates) the authentication tag.
 *                  The tag can have a maximum length of 16 bytes.
 * \param[in] ctx     The CCM context.
 * \param[in/out] tag The buffer holding the tag data.
 * \param[in] len     The length of the tag.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_ccm_finish( te_ccm_ctx_t *ctx,
                   uint8_t *tag,
                   uint32_t tag_len );

/**
 * \brief           This function clones the state of a CCM operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid CCM context already.
 *
 * \param[in]  src  The source CCM context.
 * \param[out] dst  The destination CCM context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ccm_clone( const te_ccm_ctx_t *src,
                  te_ccm_ctx_t *dst );

/**
 * \brief           This function performs a CCM authentication encryption or
 *                  decryption operation.
 * \param[in] ctx     The CCM context.
 * \param[in] op      Operation mode.
 * \param[in] len     The length of input data.
 * \param[in] nonce   The nonce.
 * \param[in] nlen    The length of the nonce, range from 7 to 13.
 * \param[in] aad     The buffer holding the AAD data, or NULL.
 * \param[in] aadlen  The length of the AAD data.
 * \param[in] in      The buffer holding the input data.
 * \param[out] out    The buffer holding th output data.
 * \param[in/out] tag The buffer holding the tag data.
 * \param[in] taglen  The length of tag. Should be one of:
 *                    {4, 6, 8, 10, 12, 14, 16}
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_ccm_crypt( te_ccm_ctx_t *ctx,
                  te_sca_operation_t op,
                  size_t len,
                  uint8_t *nonce,
                  uint32_t nlen,
                  const uint8_t *aad,
                  uint64_t aadlen,
                  const uint8_t *in,
                  uint8_t *out,
                  uint8_t *tag,
                  uint32_t taglen);

/**
 * \brief           This function performs a CCM authentication encryption or
 *                  decryption operation.
 * \param[in] ctx     The CCM context.
 * \param[in] op      Operation mode.
 * \param[in] nonce   The nonce.
 * \param[in] nlen    The length of the nonce, range from 7 to 13.
 * \param[in] aad     The buffer holding the AAD data built as memroy link
 *                    list.
 * \param[in] in      The buffer holding the input data built as memory link
 *                    list.
 * \param[out] out    The buffer holding th output data built as memory link
 *                    list.
 * \param[in/out] tag The buffer holding the tag data.
 * \param[in] taglen  The length of tag. Should be one of:
 *                    {4, 6, 8, 10, 12, 14, 16}
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_ccm_crypt_list( te_ccm_ctx_t *ctx,
                       te_sca_operation_t op,
                       uint8_t *nonce,
                       uint32_t nlen,
                       te_memlist_t *aad,
                       te_memlist_t *in,
                       te_memlist_t *out,
                       uint8_t *tag,
                       uint32_t taglen );
/**
 * asynchronous CCM operations
 */
#ifdef CFG_TE_ASYNC_EN
typedef struct te_ccm_request {
    te_async_request_t base;
    int res;                            /**< result */
    /**
     * crypt param
     */
    struct {
        te_sca_operation_t op;  /**< Operation mode */
        uint8_t *nonce;         /**< Nonce */
        uint32_t nlen;          /**< length of nonce, 7-13 */
        te_memlist_t aad;       /**< aad buf list */
        te_memlist_t in;
        te_memlist_t out;
        uint8_t *tag;           /**< Tag data */
        uint32_t taglen;        /**< should {4, 6, 8, 10, 12, 14, 16} */
    } crypt;
} te_ccm_request_t;

/**
 * \brief           This function performs a CCM authentication encryption or
 *                  decryption operation.
 * \param[in] ctx     The CCM context.
 * \param[in] req     The asynchronous requset instance.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_ccm_acrypt( te_ccm_ctx_t *ctx, te_ccm_request_t *req );

#endif /* CFG_TE_ASYNC_EN */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_CCM_H__ */
