//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_UIA2_H__
#define __TRUSTENGINE_UIA2_H__

#include "driver/te_drv_sca.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine uia2 context structure
 */
typedef struct te_uia2_ctx {
    te_crypt_ctx_t *crypt;
} te_uia2_ctx_t;

/**
 * \brief           This function gets the private ctx of an uia2 ctx.
 * \param[in] ctx   The uia2 context.
 * \return          The private context pointer.
 */
static inline void* uia2_priv_ctx(const te_uia2_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the uia2 context \p ctx.
 * \param[out] ctx  The uia2 context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm, TE_MAIN_ALGO_SNOW3G only.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_NOT_SUPPORTED if TE doesn't support the \p malg.
 * \return          \c TE_ERROR_BAD_PARAMS if params are invalid.
 */
int te_uia2_init(te_uia2_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg);

/**
 * \brief           This function withdraws the uia2 context \p ctx.
 * \param[in] ctx   The uia2 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL.
 */
int te_uia2_free(te_uia2_ctx_t *ctx);

/**
 * \brief             This function sets up the user key for the specified
 *                    uia2 context \p ctx.
 * \param[in] ctx     The uia2 context.
 * \param[in] key     The buffer holding the user key.
 * \param[in] keybits The uia2 key length in bit.
 * \return            \c TE_SUCCESS on success.
 * \return            \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL.
 * \return            \c TE_ERROR_BAD_KEY_LENGTH if \p keybits isn't \c 128.
 * \return            \c TE_ERROR_OOM if failed to allocate memory.
 */
int te_uia2_setkey(te_uia2_ctx_t *ctx,
                   const uint8_t *key,
                   uint32_t keybits);

/**
 * \brief           This function sets up the secure key for the specified
 *                  uia2 context \p ctx.
 * \param[in] ctx   The uia2 context.
 * \param[in] key   The secure key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL or
 *                  \p key->sel is invalid.
 * \return          \c TE_ERROR_BAD_KEY_LENGTH if \p key->ek3bits isn't \c 128.
 */
int te_uia2_setseckey(te_uia2_ctx_t *ctx,
                      const te_sec_key_t *key);

/**
 * \brief            This function starts an uia2 integrity session
 *                   with specified (count, fresh, dir).
 * \param[in] ctx    The uia2 context.
 * \param[in] count  The count frame dependent input.
 * \param[in] fresh  The fresh random number..
 * \param[in] dir    The direction of transmission, least 1 sinificant
 *                   bit valid.
 * \return           \c TE_SUCCESS on success.
 * \return           \c TE_ERROR_BAD_PARAMS if ctx or iv is \c NULL or \p dir
 *                   is neither \c 1 nor \c 0.
 */
int te_uia2_start(te_uia2_ctx_t *ctx,
                  uint32_t count, uint32_t fresh, uint32_t dir);

/**
 * \brief           This function performs an UIA2 integrity operation.
 * \param[in] ctx   The uia2 context.
 * \param[in] inlen The length of \p in in bytes.
 * \param[in] in    The buffer holding the input data.
 *                  This pointer can be \c NULL if \p inlen == 0.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL, or
 *                  \p inlen > 0 and \p in is \c NULL.
 * \return          \c TE_ERROR_BAD_FORMAT if \p is an invalid context.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 */
int te_uia2_update(te_uia2_ctx_t *ctx, size_t inlen, const uint8_t *in);

/**
 * \brief           This function performs an UIA2 integrity
 *                  operation.
 * \param[in] ctx   The uia2 context.
 * \param[in] in    The memory list holding the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL.
 * \return          \c TE_ERROR_BAD_FORMAT if \p is an invalid context.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_OOM if memory allocate failed.
 */
int te_uia2_uplist(te_uia2_ctx_t *ctx, const te_memlist_t *in);

/**
 * \brief            This function performs an UIA2 integrity
 *                   operation and closes the session.
 * \param[in] ctx    The uia2 context.
 * \param[in] inbits The length of the bit streams \p in.
 * \param[in] in    The buffer holding the input data.
 *                  This pointer can be \c NULL if \p inlen == 0.
 * \param[out] out  The buffer holding the output data MAC-I(32 bits),
 *                  can't be \c NULL.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL, or
 *                     \p inbits > 0 and \p in is \c NULL, or \p mac is \c NULL.
 * \return          \c TE_ERROR_BAD_FORMAT if \p is an invalid context.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_OOM if memory allocate failed.
 */
int te_uia2_finup(te_uia2_ctx_t *ctx,
                  uint64_t inbits, const uint8_t *in, uint8_t mac[4]);

/**
 * \brief           This function finish an uia2 integrity session.
 * \param[in] ctx   The uia2 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL, or
 *                     \p mac is \c NULL.
 * \return          \c TE_ERROR_BAD_FORMAT if \p is an invalid context.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_OOM if memory allocate failed.
 */
int te_uia2_finish(te_uia2_ctx_t *ctx, uint8_t mac[4]);

/**
 * \brief           This function clones the state of a uia2 operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid uia2 context already.
 *
 * \param[in]  src  The source uia2 context.
 * \param[out] dst  The destination uia2 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL, or
 *                     \p dst is \c NULL.
 * \return          \c TE_ERROR_OOM if memory allocate failed.
 */
int te_uia2_clone(const te_uia2_ctx_t *src, te_uia2_ctx_t *dst);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_UIA2_H__ */
