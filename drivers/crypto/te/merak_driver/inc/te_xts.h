//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_XTS_H__
#define __TRUSTENGINE_XTS_H__

#include "driver/te_drv_sca.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine XTS context structure
 */
typedef struct te_xts_ctx {
    te_crypt_ctx_t *crypt;
} te_xts_ctx_t;

/**
 * \brief           This function gets the private ctx of a xts ctx.
 * \param[in] ctx   The xts context.
 * \return          The private context pointer.
 */
static inline void* xts_priv_ctx(te_xts_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the xts context \p ctx. For main
 *                  algorithm of AES or SM4 only.
 * \param[out] ctx  The xts context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_xts_init( te_xts_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg );

/**
 * \brief           This function withdraws the xts context \p ctx.
 * \param[in] ctx   The xts context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_xts_free( te_xts_ctx_t *ctx );

/**
 * \brief           This function sets up the user key for the specified
 *                  xts context \p ctx.
 *
 *                  The two sets of user key must be of the equal length and
 *                  concatenated together, key1 || key2.
 *
 *                  For AES, xts supports key length of either 128 or 256
 *                  bits, but not 192 bits.
 *
 * \param[in] ctx     The xts context.
 * \param[in] key     The buffer holding the user key (key1 || key2).
 * \param[in] keybits The xts key length in bit, 2 * 128 or 2 * 256.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_xts_setkey( te_xts_ctx_t *ctx,
                   const uint8_t *key,
                   uint32_t keybits );

/**
 * \brief           This function sets up the secure key for the specified
 *                  xts context \p ctx.
 *
 *                  The ek3bits of the two sets of secure key must be equal,
 *                  either 128 or 256.
 *
 * \param[in] ctx   The xts context.
 * \param[in] key1  The secure key 1.
 * \param[in] key2  The secure key 2.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_xts_setseckey( te_xts_ctx_t *ctx,
                      te_sec_key_t *key1,
                      te_sec_key_t *key2 );

/**
 * \brief           This function sets up the secure key of key-ladder 256 for
 *                  the specified xts context \p ctx. AES only.
 *
 *                  The ek3bits of the two sets of secure key must be equal,
 *                  must be 256.
 *
 * \param[in] ctx   The xts context.
 * \param[in] key   The secure key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_xts_setseckey_v2( te_xts_ctx_t *ctx,
                         te_sec_key_v2_t *key1,
                         te_sec_key_v2_t *key2 );

/**
 * \brief           This function performs an XTS encryption or decryption
 *                  operation for an entire XTS data unit.
 *
 * \param[in] ctx           The xts context.
 * \param[in] op            Operation mode.
 * \param[in] len           The length of input data.
 * \param[inout] data_unit  The address of the data unit encoded as an
 *                          array of 16 bytes in little-endian format.(updated after used)
 * \param[in] in            The buffer holding the input data.
 * \param[out] out          The buffer holding the output data.
 * \return                  \c TE_SUCCESS on success.
 * \return                  \c <0 on failure.
 */
int te_xts_crypt( te_xts_ctx_t *ctx,
                  te_sca_operation_t op,
                  size_t len,
                  uint8_t data_unit[16],
                  const uint8_t *in,
                  uint8_t *out );

/**
 * \brief           This function performs an XTS encryption or decryption
 *                  operation for an entire XTS data unit.
 *
 * \param[in] ctx           The xts context.
 * \param[in] op            Operation mode.
 * \param[inout] data_unit  The address of the data unit encoded as an
 *                          array of 16 bytes in little-endian format.(updated after used)
 * \param[in] in            The memory list holding the input data.
 * \param[out] out          The memory list holding the output data.
 * \return                  \c TE_SUCCESS on success.
 * \return                  \c <0 on failure.
 */
int te_xts_crypt_list( te_xts_ctx_t *ctx,
                       te_sca_operation_t op,
                       uint8_t data_unit[16],
                       te_memlist_t *in,
                       te_memlist_t *out);

/**
 * \brief           This function clones the state of a XTS operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid XTS context already.
 *
 * \param[in]  src  The source XTS context.
 * \param[out] dst  The destination XTS context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_xts_clone( const te_xts_ctx_t *src,
                  te_xts_ctx_t *dst );

#ifdef CFG_TE_ASYNC_EN
/**
 * cipher asynchronous request structure
 */
typedef struct te_xts_request {
    te_async_request_t base; /**< base async request */
    te_sca_operation_t op;   /**< operation mode */
    uint8_t data_unit[16];   /**< data unit array */
    te_memlist_t src;
    te_memlist_t dst;
    int res;
} te_xts_request_t;

/**
 * \brief           This function performs an XTS encryption or decryption
 *                  operation for an entire XTS data unit.
 *
 * \param[in] ctx       The xts context.
 * \param[in] req       The xts async requset instance.
 * \return              \c TE_SUCCESS on success.
 * \return              \c <0 on failure.
 */
int te_xts_acrypt( te_xts_ctx_t *ctx, te_xts_request_t *req );

#endif /* CFG_TE_ASYNC_EN */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_XTS_H__ */
