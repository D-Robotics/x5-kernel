//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_GCM_H__
#define __TRUSTENGINE_GCM_H__

#include "te_cipher.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * GCM state enumeration
 */
typedef enum te_gcm_state {
    TE_GCM_STATE_RAW = 0,
    TE_GCM_STATE_INIT,
    TE_GCM_STATE_READY,
    TE_GCM_STATE_START,
    TE_GCM_STATE_UPDATE,
} te_gcm_state_t;

/**
 * GCM context structure
 */
typedef struct te_gcm_ctx {
    te_crypt_ctx_t *crypt;
} te_gcm_ctx_t;

/**
 * \brief           This function gets the private ctx of a GCM context.
 * \param[in] ctx   The GCM context.
 * \return          The private context pointer.
 */
static inline void* gcm_priv_ctx(te_gcm_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the GCM context \p ctx. For main
 *                  algorithm of AES or SM4 only.
 * \param[out] ctx  The GCM context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gcm_init( te_gcm_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg );

/**
 * \brief           This function withdraws the GCM context \p ctx.
 * \param[in] ctx   The GCM context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gcm_free( te_gcm_ctx_t *ctx );

/**
 * \brief           This function sets up the user key for the specified
 *                  GCM context \p ctx.
 * \param[in] ctx     The GCM context.
 * \param[in] key     The buffer holding the user key.
 * \param[in] keybits The GCM key length in bit.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_gcm_setkey( te_gcm_ctx_t *ctx,
                   const uint8_t *key,
                   uint32_t keybits );

/**
 * \brief           This function sets up the secure key for the specified
 *                  GCM context \p ctx.
 * \param[in] ctx   The GCM context.
 * \param[in] key   The secure key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gcm_setseckey( te_gcm_ctx_t *ctx,
                      te_sec_key_t *key );

/**
 * \brief           This function sets up the secure key of key-ladder 256 for
 *                  the specified GCM context \p ctx.
 * \param[in] ctx   The GCM context.
 * \param[in] key   The secure key of key-ladder 256.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gcm_setseckey_v2( te_gcm_ctx_t *ctx,
                         te_sec_key_v2_t *key );

/**
 * \brief           This function clones the state of a GCM operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid GCM context already.
 *
 * \param[in]  src  The source GCM context.
 * \param[out] dst  The destination GCM context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gcm_clone( const te_gcm_ctx_t *src,
                  te_gcm_ctx_t *dst );

/**
 * \brief           This function starts a GCM encryption or decryption
 *                  operation.
 * \param[in] ctx    The GCM context.
 * \param[in] op     Operation mode.
 * \param[in] iv     The initialization vector.
 * \param[in] ivlen  The length of the IV.
 * \return           \c TE_SUCCESS on success.
 * \return           \c <0 on failure.
 */
int te_gcm_start( te_gcm_ctx_t *ctx,
                  te_sca_operation_t op,
                  uint8_t *iv,
                  uint64_t ivlen );
/**
 * \brief           This function feeds an input of associated data into an ongoing GCM
 *                  encryption or decryption operation.
 * \param[in] ctx   The GCM context.
 * \param[in] data  The buffer holding the input associated data.
 * \param[in] len   The length of the input associated data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gcm_update_aad( te_gcm_ctx_t *ctx,
                       const uint8_t *data,
                       size_t len );

/**
 * \brief           This function feeds an input of associated list data into an ongoing GCM
 *                  encryption or decryption operation.
 * \param[in] ctx   The GCM context.
 * \param[in] data  The list buffer holding the input associated data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gcm_uplist_aad( te_gcm_ctx_t *ctx,
                       te_memlist_t *in) ;

/**
 * \brief           This function feeds an input buffer into an ongoing GCM
 *                  encryption or decryption operation.
 * \param[in] ctx   The GCM context.
 * \param[in] len   The length of the input data.
 * \param[in] in    The buffer holding the input data.
 * \param[out] out  The buffer holding th output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gcm_update( te_gcm_ctx_t *ctx,
                   size_t len,
                   const uint8_t *in,
                   uint8_t *out );

/**
 * \brief           This function feeds a list of input buffers into an ongoing
 *                  GCM encryption or decryption operation.
 *
 *                  The total length of the input and output buffers on a
 *                  te_gcm_uplist() shall be equal.
 *
 * \param[in] ctx   The GCM context.
 * \param[in] in    The list of buffers holding the input data.
 * \param[out] out  The list of buffers holding th output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gcm_uplist( te_gcm_ctx_t *ctx,
                   te_memlist_t *in,
                   te_memlist_t *out );

/**
 * \brief           This function finishes the GCM operation and checks
 *                  (generates) the authentication tag.
 *                  The tag can have a maximum length of 16 bytes.
 * \param[in] ctx     The GCM context.
 * \param[in/out] tag The buffer holding the tag data.
 * \param[in] len     The length of the tag.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_gcm_finish( te_gcm_ctx_t *ctx,
                   uint8_t *tag,
                   uint32_t taglen );

/**
 * asynchronous GCM operations
 */
#ifdef CFG_TE_ASYNC_EN
typedef struct te_gcm_request {
    te_async_request_t base;
    int res;                           /**< result */
	/**
	 * crypt para
	 */
	struct {
            te_sca_operation_t op;
            uint8_t *iv;
            uint64_t ivlen;
            te_memlist_t aad;
            te_memlist_t in;
            te_memlist_t out;
            uint8_t *tag;
            uint32_t taglen;
	} crypt;
} te_gcm_request_t;

/**
 * \brief           This function performs a GCM authentication encryption or
 *                  decryption operation.
 * \param[in] ctx   The GCM context.
 * \param[in] req   The asynchronous requset instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gcm_acrypt( te_gcm_ctx_t *ctx, te_gcm_request_t *req );

#endif /* CFG_TE_ASYNC_EN */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_GCM_H__ */
