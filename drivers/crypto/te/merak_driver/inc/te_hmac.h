//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HMAC_H__
#define __TRUSTENGINE_HMAC_H__

#include "driver/te_drv_hash.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine hmac context structure
 */
typedef struct te_hmac_ctx {
    te_crypt_ctx_t *crypt;
} te_hmac_ctx_t;

/**
 * \brief           This function gets the private ctx of a hmac ctx.
 * \param[in] ctx   The hmac context.
 * \return          The private context pointer.
 */
static inline void* hmac_priv_ctx(te_hmac_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the hmac context \p ctx.
 * \param[out] ctx  The hmac context.
 * \param[in] hdl   The driver handler.
 * \param[in] alg   The algorithm identifier.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hmac_init( te_hmac_ctx_t *ctx, te_drv_handle hdl, te_algo_t alg );

/**
 * \brief           This function withdraws the hmac context \p ctx.
 * \param[in] ctx   The hmac context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hmac_free( te_hmac_ctx_t *ctx );

/**
 * \brief           This function starts a hmac operation with supplied
 *                  user \p key.
 * \param[in] ctx     The hmac context.
 * \param[in] key     The buffer holding the user key.
 * \param[in] keybits The length of key in bit.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_hmac_start( te_hmac_ctx_t *ctx,
                   const uint8_t *key,
                   uint32_t keybits );

/**
 * \brief           This function starts a hmac operation with supplied
 *                  secure \p key.
 * \param[in] ctx     The hmac context.
 * \param[in] key     The secure key.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_hmac_start2( te_hmac_ctx_t *ctx,
                    te_sec_key_t *key );

/**
 * \brief           This function starts a hmac operation with supplied
 *                  secure \p key of key-ladder 256.
 * \param[in] ctx     The hmac context.
 * \param[in] key     The secure key of key-ladder 256.
 * \return            \c TE_SUCCESS on success.
 * \return            \c <0 on failure.
 */
int te_hmac_start2_v2( te_hmac_ctx_t *ctx,
                       te_sec_key_v2_t *key );

/**
 * \brief           This function feeds an input buffer into an ongoing hmac
 *                  operation.
 *
 *                  It is called between te_hmac_start() or te_hmac_reset(),
 *                  and te_hmac_finish(). Can be called repeatedly.
 *
 * \param[in] ctx   The hmac context.
 * \param[in] in    The buffer holding the input data.
 * \param[in] len   The length of the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hmac_update( te_hmac_ctx_t *ctx,
                    const uint8_t *in,
                    size_t len );

/**
 * \brief           This function feeds a list of input buffers into an ongoing
 *                  hmac operation.
 *
 *                  It is called between te_hmac_start() or te_hmac_reset(),
 *                  and te_hmac_finish(). Can be called repeatedly.
 *
 * \param[in] ctx   The hmac context.
 * \param[in] in    The list of buffers holding the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hmac_uplist( te_hmac_ctx_t *ctx,
                    te_memlist_t *in );

/**
 * \brief           This function finishes the hmac operation and writes
 *                  the result to the \p hash buffer.
 *
 *                  The \p hash buffer must be of enough length to load the
 *                  result.
 *
 *                  It is called after te_hmac_update() or te_hmac_uplist().
 *                  It can be followed by te_hmac_start() or te_hmac_free().
 *
 * \param[in] ctx    The hmac context.
 * \param[out] mac   The buffer holding the mac data.
 * \param[in] maclen The length of required mac.
 * \return           \c TE_SUCCESS on success.
 * \return           \c <0 on failure.
 */
int te_hmac_finish( te_hmac_ctx_t *ctx,
                    uint8_t *mac,
                    uint32_t maclen );

/**
 * \brief           This function resets the hmac operation and prepares the
 *                  computation of another message with the same key as the
 *                  previous hmac operation.
 *
 *                  It is called after te_hmac_update() or te_hmac_uplist().
 *
 * \param[in] ctx   The hmac context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hmac_reset( te_hmac_ctx_t *ctx );

/**
 * \brief           This function clones the state of a hmac context.
 *
 * \param[in] src   The source hmac context.
 * \param[out] dst  The destination hmac context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hmac_clone( te_hmac_ctx_t *src, te_hmac_ctx_t *dst);

/**
 * \brief           This function exports partial state of the calculation.
 *                  This function dumps the entire state of the specified con-
 *                  text into a provided block of data so it can be @import 'ed
 *                  back later on. This is useful in case you want to save
 *                  partial result of the calculation after processing certain
 *                  amount of data and reload this partial result multiple
 *                  times later on for multiple re-use.
 *
 * \param[in]  ctx  The HMAC context.
 * \param[out] out  Buffer filled with the state data on success.
 * \param[inout] olen Size of \p out buffer on input.
 *                    Length of data filled in the \p out buffer on success.
 *                    Required \p out buffer length on TE_ERROR_SHORT_BUFFER.
 * \return          \c TE_SUCCESS on success.
 *                  \c TE_ERROR_SHORT_BUFFER if *olen is less than required.
 * \return          \c <0 on failure.
 */
int te_hmac_export( te_hmac_ctx_t *ctx,
                    void *out,
                    uint32_t *olen );

/**
 * \brief           This function imports partial state of the calculation.
 *                  This function loads the entire state of the specified con-
 *                  text from a provided block of data so the calculation can
 *                  continue from this point onward.
 *
 * \param[in] ctx   The HMAC context.
 * \param[in] in    Buffer filled with the state data exported early.
 * \param[in] ilen  Size of the state data in the \p in buffer.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hmac_import( te_hmac_ctx_t *ctx,
                    const void *in,
                    uint32_t ilen );

/**
 * asynchronous hmac operations
 */
#ifdef CFG_TE_ASYNC_EN

/**
 * hmac asynchronous request structure
 */
typedef struct te_hmac_request {
    te_async_request_t base;
    int res;                           /**< result */
    union {
        /**
         * start params
         */
        struct {
            te_key_wrap_t key;
        } st;
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
            uint8_t *mac;
            uint32_t maclen;
        } fin;
        /**
         * hmac clone params
         */
        struct {
            te_hmac_ctx_t *src;        /**< source context */
            te_hmac_ctx_t *dst;        /**< dest context */
        } cl;
        /**
         * hmac params
         */
        struct {
            te_key_wrap_t key;         /**< hmac key */
            te_memlist_t in;
            uint8_t *mac;              /**< mac buffer */
            uint32_t maclen;           /**< mac length in bytes */
        } hmac;

    };
} te_hmac_request_t;

/**
 * \brief           This function asynchronously starts a hmac operation.
 *
 * \param[in] ctx   The hmac context.
 * \param[int] req  The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hmac_astart( te_hmac_ctx_t *ctx, te_hmac_request_t *req );

/**
 * \brief           This function asynchronously feeds input buffer to the
 *                  ongoing hmac operation.
 *
 * \param[in] ctx   The hmac context.
 * \param[int] req  The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hmac_aupdate( te_hmac_ctx_t *ctx, te_hmac_request_t *req );

/**
 * \brief           This function asynchronously finishes the hmac operation
 *                  and writes the result into the request buffer.
 *
 * \param[in] ctx   The hmac context.
 * \param[int] req  The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hmac_afinish( te_hmac_ctx_t *ctx, te_hmac_request_t *req );

/**
 * \brief           This function asynchronously clones the state of a hmac
 *                  context.
 *
 * \param[int] req  The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hmac_aclone( te_hmac_request_t *req );

/**
 * \brief           This function asynchronously calculates the hmac of a list
 *                  of buffer.
 *
 *                  The function allocates the context, performs the
 *                  calculation, and frees the context.
 *
 * \param[in] hdl   The driver handler.
 * \param[in] alg   The hmac algorithm identifier.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ahmac( te_drv_handle hdl, te_algo_t alg, te_hmac_request_t *req );

#endif /* CFG_TE_ASYNC_EN */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HMAC_H__ */
