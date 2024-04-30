//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_GFMUL_H__
#define __TRUSTENGINE_GFMUL_H__

#include "te_cipher.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * GFmul context structure
 */
typedef struct te_gfmul_ctx {
    te_crypt_ctx_t *crypt;
} te_gfmul_ctx_t;

/**
 * \brief           This function gets the private ctx of a GFMUL ctx.
 * \param[in] ctx   The GFMUL context.
 * \return          The private context pointer.
 */
static inline void* gfmul_priv_ctx(te_gfmul_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the GFMUL context \p ctx.
 * \param[out] ctx  The GFMUL context.
 * \param[in] hdl   The driver handler.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gfmul_init( te_gfmul_ctx_t *ctx, te_drv_handle hdl );

/**
 * \brief           This function withdraws the GFMUL context \p ctx.
 * \param[in] ctx   The GFMUL context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gfmul_free( te_gfmul_ctx_t *ctx );

/**
 * \brief               This function sets up the user key for the specified
 *                      GFMUL context \p ctx.
 * \param[in] ctx       The GFMUL context.
 * \param[in] key       The buffer holding the H key.
 * \param[in] keybits   The length of the \p key in bit.
 * \return              \c TE_SUCCESS on success.
 * \return              \c <0 on failure.
 */
int te_gfmul_setkey( te_gfmul_ctx_t *ctx,
                     const uint8_t *key,
                     size_t keybits );

/**
 * \brief           This function starts a GFMUL computation and prepares to
 *                  authenticate the input data.
 * \param[in] ctx   The GFMUL context.
 * \param[in] iv    The 128-bit initialization vector.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gfmul_start( te_gfmul_ctx_t *ctx,
                    uint8_t *iv );

/**
 * \brief           This function feeds an input buffer into an ongoing GFMUL
 *                  computation.
 *
 *                  It is called between te_gfmul_start() or te_gfmul_reset(),
 *                  and te_gfmul_finish(). Can be called repeatedly.
 *
 * \param[in] ctx   The GFMUL context.
 * \param[in] len   The length of the input data.
 * \param[in] in    The buffer holding the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gfmul_update( te_gfmul_ctx_t *ctx,
                     size_t len,
                     const uint8_t *in );

/**
 * \brief           This function feeds a list of input buffers into an ongoing
 *                  GFMUL computation.
 *
 *                  It is called between te_gfmul_start() or te_gfmul_reset(),
 *                  and te_gfmul_finish(). Can be called repeatedly.
 *
 * \param[in] ctx   The GFMUL context.
 * \param[in] in    The list of buffers holding the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gfmul_uplist( te_gfmul_ctx_t *ctx,
                     te_memlist_t *in );

/**
 * \brief           This function finishes the GFMUL computation and writes the
 *                  result to the mac buffer.
 *
 *                  The total length of the input data fed to the GFMUL
 *                  operation before must be multiple of 16 bytes.
 *
 *                  It is called after te_gfmul_update() or te_gfmul_uplist().
 *                  It can be followed by te_gfmul_start() or te_gfmul_free().
 *
 * \param[in] ctx    The GFMUL context.
 * \param[out] mac   The buffer holding the mac data.
 * \param[in] maclen The length of the mac data.
 * \return           \c TE_SUCCESS on success.
 * \return           \c <0 on failure.
 */
int te_gfmul_finish( te_gfmul_ctx_t *ctx,
                     uint8_t *mac,
                     uint32_t maclen );

/**
 * \brief           This function resets the GFMUL computation and prepares the
 *                  computation of another message with the same key as the
 *                  previous GFMUL operation.
 *
 *                  It is called after te_gfmul_update() or te_gfmul_uplist().
 *
 * \param[in] ctx   The GFMUL context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gfmul_reset( te_gfmul_ctx_t *ctx );

/**
 * \brief           This function clones the state of a GFMUL operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid GFMUL context already.
 *
 * \param[in]  src  The source GFMUL context.
 * \param[out] dst  The destination GFMUL context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gfmul_clone( const te_gfmul_ctx_t *src,
                    te_gfmul_ctx_t *dst );

/**
 * \brief           This function exports partial state of the calculation.
 *                  This function dumps the entire state of the specified con-
 *                  text into a provided block of data so it can be @import 'ed
 *                  back later on. This is useful in case you want to save
 *                  partial result of the calculation after processing certain
 *                  amount of data and reload this partial result multiple
 *                  times later on for multiple re-use.
 *
 * \param[in]  ctx  The GFMUL context.
 * \param[out] out  Buffer filled with the state data on success.
 * \param[inout] olen Size of \p out buffer on input.
 *                    Length of data filled in the \p out buffer on success.
 *                    Required \p out buffer length on TE_ERROR_SHORT_BUFFER.
 * \return          \c TE_SUCCESS on success.
 *                  \c TE_ERROR_SHORT_BUFFER if *olen is less than required.
 * \return          \c <0 on failure.
 */
int te_gfmul_export( te_gfmul_ctx_t *ctx,
                     void *out,
                     uint32_t *olen );

/**
 * \brief           This function imports partial state of the calculation.
 *                  This function loads the entire state of the specified con-
 *                  text from a provided block of data so the calculation can
 *                  continue from this point onward.
 *
 * \param[in] ctx   The GFMUL context.
 * \param[in] in    Buffer filled with the state data exported early.
 * \param[in] ilen  Size of the state data in the \p in buffer.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_gfmul_import( te_gfmul_ctx_t *ctx,
                     const void *in,
                     uint32_t ilen );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_GFMUL_H__ */
