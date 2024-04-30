//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_UEA2_H__
#define __TRUSTENGINE_UEA2_H__

#include "driver/te_drv_sca.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine uea2 context structure
 */
typedef struct te_uea2_ctx {
    te_crypt_ctx_t *crypt;
} te_uea2_ctx_t;

/**
 * \brief           This function gets the private ctx of an uea2 ctx.
 * \param[in] ctx   The uea2 context.
 * \return          The private context pointer.
 */
static inline void* uea2_priv_ctx(const te_uea2_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the uea2 context \p ctx.
 * \param[out] ctx  The uea2 context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm, TE_MAIN_ALGO_SNOW3G only.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_NOT_SUPPORTED if TE doesn't support the \p malg.
 * \return          \c TE_ERROR_BAD_PARAMS if params are invalid.
 */
int te_uea2_init(te_uea2_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg);

/**
 * \brief           This function withdraws the uea2 context \p ctx.
 * \param[in] ctx   The uea2 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL.
 */
int te_uea2_free(te_uea2_ctx_t *ctx);

/**
 * \brief             This function sets up the user key for the specified
 *                    uea2 context \p ctx.
 * \param[in] ctx     The uea2 context.
 * \param[in] key     The buffer holding the user key.
 * \param[in] keybits The uea2 key length in bit.
 * \return            \c TE_SUCCESS on success.
 * \return            \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL.
 * \return            \c TE_ERROR_BAD_KEY_LENGTH if \p keybits isn't \c 128.
 * \return            \c TE_ERROR_OOM if failed to allocate memory.
 */
int te_uea2_setkey(te_uea2_ctx_t *ctx, const uint8_t *key, uint32_t keybits);

/**
 * \brief           This function sets up the secure key for the specified
 *                  uea2 context \p ctx.
 * \param[in] ctx   The uea2 context.
 * \param[in] key   The secure key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL or
 *                  \p key->sel is invalid.
 * \return          \c TE_ERROR_BAD_KEY_LENGTH if \p key->ek3bits isn't \c 128.
 */
int te_uea2_setseckey(te_uea2_ctx_t *ctx, const te_sec_key_t *key);

/**
 * \brief            This function builds iv for uea2 with specified (count,
 *                   bearer, dir).
 * \param[out] iv    The buffer to store iv.
 * \param[in] count  The count frame dependent input.
 * \param[in] bearer The bearer identity, least 5 sinificant bits valid.
 * \param[in] dir    The direction of transmission, least 1 sinificant bit valid.
 * \return           \c TE_SUCCESS on success.
 * \return           \c TE_ERROR_BAD_PARAMS if \p iv is \c NULL, or
 *                   \p bearer > \c 0x1F, or \p dir neither \c 1 nor \c 0 .
 */
int te_uea2_build_iv(uint8_t iv[16], uint32_t count,
                     uint32_t bearer, uint32_t dir);

/**
 * \brief           This function starts an uea2 encrypt / decrypt session
 *                  with specified IV.
 * \param[in] ctx   The uea2 context.
 * \param[in] iv    The initialization vector.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx or \p iv is \c NULL.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_OOM if failed to allocate memory.
 */
int te_uea2_start(te_uea2_ctx_t *ctx, const uint8_t iv[16]);

/**
 * \brief           This function performs an UEA2 encryption or decryption
 *                  operation.
 * \param[in] ctx   The uea2 context.
 * \param[in] inlen The length of \p in in bytes.
 * \param[in] in    The buffer holding the input data.
 *                  This pointer can be \c NULL if \p inlen == 0.
 * \param[out] out  The buffer holding the output data.
 *                  This pointer can be \c NULL if \p inlen == 0.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL, or
 *                     \p inlen > 0 and \p in or \p out is \c NULL.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_OOM if failed to allocate memory.
 */
int te_uea2_update(te_uea2_ctx_t *ctx,
                   size_t inlen, const uint8_t *in, uint8_t *out);

/**
 * \brief           This function performs an UEA2 encryption or decryption
 *                  operation.
 * \param[in] ctx   The uea2 context.
 * \param[in] in    TThe memory list holding the input data.
 * \param[out] out  The memory list holding the output data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL, or
 *                     \p in is not \c NULL and \p out is \c NULL.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_OOM if failed to allocate memory.
 */
int te_uea2_uplist(te_uea2_ctx_t *ctx,
                   const te_memlist_t *in, te_memlist_t *out);

/**
 * \brief            This function performs an UEA2 encryption or decryption
 *                   operation and closes the session.
 * \param[in] ctx    The uea2 context.
 * \param[in] inbits The length of the bit streams \p in.
 * \param[in] in     The buffer holding the input data.
 *                   This pointer can be \c NULL if \p inlen == 0.
 * \param[out] out   The buffer holding the output data.
 *                   This pointer can be \c NULL if \p inlen == 0.
 * \return           \c TE_SUCCESS on success.
 * \return           \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL, or
 *                      \p inlen > 0 and \p in is \c NULL or \p out is \c NULL.
 * \return           \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return           \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return           \c TE_ERROR_OOM if memory allocate failed.
 */
int te_uea2_finup(te_uea2_ctx_t *ctx,
                  uint64_t inbits, const uint8_t *in, uint8_t *out);

/**
 * \brief           This function finishes an uea2 encryption or decryption session.
 * \param[in] ctx   The uea2 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 */
int te_uea2_finish(te_uea2_ctx_t *ctx);

/**
 * \brief           This function clones the state of a uea2 operation.
 *                  This function will free the \p dst context before clone if
 *                  it pointed to a valid uea2 context already.
 *
 * \param[in]  src  The source uea2 context.
 * \param[out] dst  The destination uea2 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p src is \c NULL,
 *                     or \p dst is \c NULL.
 * \return          \c TE_ERROR_OOM if memory allocate failed.
 */
int te_uea2_clone(const te_uea2_ctx_t *src,
                  te_uea2_ctx_t *dst);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_UEA2_H__ */
