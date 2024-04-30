//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DIGEST_H__
#define __TRUSTENGINE_DIGEST_H__

#include "driver/te_drv_hash.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine digest context structure
 */
typedef struct te_dgst_ctx {
    te_crypt_ctx_t *crypt;
} te_dgst_ctx_t;

/**
 * \brief           This function initializes the digest context \p ctx.
 * \param[out] ctx  The digest context.
 * \param[in] hdl   The driver handler.
 * \param[in] alg   The digest algorithm identifier.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_init( te_dgst_ctx_t *ctx, te_drv_handle hdl, te_algo_t alg );

/**
 * \brief           This function withdraws the digest context \p ctx.
 * \param[in] ctx   The digest context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_free( te_dgst_ctx_t *ctx );

/**
 * \brief           This function starts a digest operation.
 * \param[in] ctx   The digest context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_start( te_dgst_ctx_t *ctx );

/**
 * \brief           This function feeds an input buffer into an ongoing digest
 *                  operation.
 *
 *                  It is called between te_dgst_start() or te_dgst_reset(),
 *                  and te_dgst_finish(). Can be called repeatedly.
 *
 * \param[in] ctx   The digest context.
 * \param[in] in    The buffer holding the input data.
 * \param[in] len   The length of the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_update( te_dgst_ctx_t *ctx, const uint8_t *in, size_t len );

/**
 * \brief           This function feeds a list of input buffers into an ongoing
 *                  digest operation.
 *
 *                  It is called between te_dgst_start() or te_dgst_reset(),
 *                  and te_dgst_finish(). Can be called repeatedly.
 *
 * \param[in] ctx   The digest context.
 * \param[in] in    The list of buffers holding the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_uplist( te_dgst_ctx_t *ctx, te_memlist_t *in );

/**
 * \brief           This function finishes the digest operation and writes
 *                  the result to the \p hash buffer.
 *
 *                  The \p hash buffer must be of enough length to load the
 *                  result.
 *
 *                  It is called after te_dgst_update() or te_dgst_uplist().
 *                  It can be followed by te_dgst_start() or te_dgst_free().
 *
 * \param[in] ctx   The digest context.
 * \param[out] hash The buffer holding the hash data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_finish( te_dgst_ctx_t *ctx, uint8_t *hash );

/**
 * \brief           This function resets the digest operation and prepares the
 *                  computation of another message.
 *
 *                  It is called after te_dgst_update() or te_dgst_uplist().
 *
 * \param[in] ctx   The digest context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_reset( te_dgst_ctx_t *ctx );

/**
 * \brief           This function clones the state of a digest context.
 *
 * \param[in] src   The source digest context.
 * \param[out] dst  The destination digest context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_clone( te_dgst_ctx_t *src, te_dgst_ctx_t *dst );

/**
 * \brief           This function exports partial state of the calculation.
 *                  This function dumps the entire state of the specified con-
 *                  text into a provided block of data so it can be @import 'ed
 *                  back later on. This is useful in case you want to save
 *                  partial result of the calculation after processing certain
 *                  amount of data and reload this partial result multiple
 *                  times later on for multiple re-use.
 *
 * \param[in]  ctx  The digest crypto context.
 * \param[out] out  Buffer filled with the state data on success.
 * \param[inout] olen Size of \p out buffer on input.
 *                    Length of data filled in the \p out buffer on success.
 *                    Required \p out buffer length on TE_ERROR_SHORT_BUFFER.
 * \return          \c TE_SUCCESS on success.
 *                  \c TE_ERROR_SHORT_BUFFER if *olen is less than required.
 * \return          \c <0 on failure.
 */
int te_dgst_export( te_dgst_ctx_t *ctx,
                    void *out,
                    uint32_t *olen );

/**
 * \brief           This function imports partial state of the calculation.
 *                  This function loads the entire state of the specified con-
 *                  text from a provided block of data so the calculation can
 *                  continue from this point onward.
 *
 * \param[in] ctx   The digest crypto context.
 * \param[in] in    Buffer filled with the state data exported early.
 * \param[in] ilen  Size of the state data in the \p in buffer.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_import( te_dgst_ctx_t *ctx,
                    const void *in,
                    uint32_t ilen );

/**
 * \brief           This function calculates the digest of a buffer.
 *
 *                  The function allocates the context, performs the
 *                  calculation, and frees the context.
 *
 * \param[in] hdl   The driver handler.
 * \param[in] alg   The digest algorithm identifier.
 * \param[in] in    The buffer holding the input data.
 * \param[in] len   The length of the input data.
 * \param[out] hash The buffer holding the resulted digest.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst( te_drv_handle hdl, te_algo_t alg,
             const uint8_t *in, size_t len,
             uint8_t *hash );

/**
 * asynchronous digest operations
 */
#ifdef CFG_TE_ASYNC_EN

/**
 * digest asynchronous request structure
 */
typedef struct te_dgst_request {
    te_async_request_t base;
    int res;                           /**< result */
    union {
        /**
         * update params
         */
        struct {
            te_memlist_t in;
        } up;

        /**
         * finish params
         */
        struct {
            uint8_t *hash;
        } fin;

        /**
         * clone params
         */
        struct {
            te_dgst_ctx_t *src;        /**< source context */
            te_dgst_ctx_t *dst;        /**< dest context */
        } cl;

        /**
         * dgst params
         */
        struct {
            te_memlist_t in;
            uint8_t *hash;
        } dgst;
    };
} te_dgst_request_t;

/**
 * \brief           This function asynchronously starts a digest operation.
 *
 * \param[in] ctx   The digest context.
 * \param[int] req  The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_astart( te_dgst_ctx_t *ctx, te_dgst_request_t *req );

/**
 * \brief           This function feeds an input buffer from the asynchronous
 *                  requset into an ongoing digest operation, asynchronously.
 *
 * \param[in] ctx   The digest context.
 * \param[int] req  The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_aupdate( te_dgst_ctx_t *ctx, te_dgst_request_t *req );

/**
 * \brief           This function asynchronously finishes the digest
 *                  operation and stores the hash value into asynchronous
 *                  request.
 *
 * \param[in] ctx   The digest context.
 * \param[int] req  The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_afinish( te_dgst_ctx_t *ctx, te_dgst_request_t *req );

/**
 * \brief           This function asynchronously clones the state of a digest
 *                  context.
 *
 * \param[int] req  The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_dgst_aclone( te_dgst_request_t *req );

/**
 * \brief           This function asynchronously calculates the digest of a list
 *                  of buffer.
 *
 *                  The function allocates the context, performs the
 *                  calculation, and frees the context.
 *
 * \param[in] hdl   The driver handler.
 * \param[in] alg   The digest algorithm identifier.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_adgst( te_drv_handle hdl, te_algo_t alg, te_dgst_request_t *req );

#endif /* CFG_TE_ASYNC_EN */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_DIGEST_H__ */
