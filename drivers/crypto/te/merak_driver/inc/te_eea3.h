//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_EEA3_H__
#define __TRUSTENGINE_EEA3_H__

#include "driver/te_drv_sca.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine eea3 context structure
 */
typedef struct te_eea3_ctx {
    te_crypt_ctx_t *crypt;
} te_eea3_ctx_t;

/**
 * \brief           This function gets the private ctx of a EEA3 ctx.
 *
 * \param[in] ctx   The EEA3 context.
 * \return          The private context pointer.
 */
static inline void* eea3_priv_ctx(te_eea3_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the EEA3 context \p ctx.
 *
 * \note            For main algorithm of ZUC only.
 *
 * \param[out] ctx  The EEA3 context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_BAD_PARAMS if params are invalid.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eea3_init(te_eea3_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg);

/**
 * \brief           This function withdraws the EEA3 context \p ctx.
 *
 * \param[in] ctx   The EEA3 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eea3_free(te_eea3_ctx_t *ctx);

/**
 * \brief             This function sets up the user key for the specified
 *                    EEA3 context \p ctx.
 *
 * \note              Only 128-bits \p keybits is valid.
 *
 * \param[in] ctx     The EEA3 context.
 * \param[in] key     The buffer holding the user key.
 * \param[in] keybits The EEA3 key length in bit.
 * \return            \c TE_SUCCESS on success.
 * \return            \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return            \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL.
 * \return            \c TE_ERROR_BAD_KEY_LENGTH if \p keybits isn't \c 128.
 * \return            \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eea3_setkey(te_eea3_ctx_t *ctx, const uint8_t *key, uint32_t keybits);

/**
 * \brief           This function sets up the secure key for the specified
 *                  EEA3 context \p ctx.
 *
 * \param[in] ctx   The EEA3 context.
 * \param[in] key   The secure key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL or
 *                     \p key->sel is invalid.
 * \return          \c TE_ERROR_BAD_KEY_LENGTH if \p key->ek3bits isn't \c 128.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eea3_setseckey(te_eea3_ctx_t *ctx, const te_sec_key_t *key);

/**
 * \brief            This function starts an EEA3 encrypt / decrypt session
 *                   with packaged IV.
 *
 * \param[in] ctx    EEA3 context \p ctx.
 * \param[in] count  The 32-bit counter.
 * \param[in] bearer The 5-bit bearer identity.
 * \param[in] dir    The 1-bit input indicating the direction of transmission.
 * \return           \c TE_SUCCESS on success.
 * \return           \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return           \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return           \c TE_ERROR_BAD_PARAMS if \p iv is \c NULL or \c bearer
 *                      is gt than 0x1F or \p dir neither \c 1 nor \c 0 .
 * \return           \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eea3_start(te_eea3_ctx_t *ctx, uint32_t count, uint32_t bearer, uint32_t dir);

/**
 * \brief           This function performs an EEA3 encryption or decryption
 *                  operation.
 *
 * \note            It is called between te_eea3_start() and te_eea3_finish().
 *                  Could be called repeatedly.
 *
 * \param[in] ctx   The EEA3 context.
 * \param[in] inlen The length of the input data in byte.
 * \param[in] in    The buffer holding the original message.
 * \param[out] out  The buffer holding the processed message.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL, \p in / or \p out
 *                     is \c NULL, and \p inlen > 0 .
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eea3_update(te_eea3_ctx_t *ctx, size_t inlen, const uint8_t *in, uint8_t *out);

/**
 * \brief           This function performs an EEA3 encryption or decryption
 *                  operation.
 *
 * \note            It is called between te_eea3_start() and te_eea3_finish().
 *                  Could be called repeatedly.
 *
 * \param[in] ctx   The EEA3 context.
 * \param[in] in    The list of buffers holding the original message.
 * \param[out] out  The list of buffers holding the processed message.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL or \p in / \p out
 *                     are \c NULL.
 * \return          \c TE_ERROR_BAD_INPUT_DATA if \p in->nent > 0 and \p in->ents is \c NULL .
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eea3_uplist(te_eea3_ctx_t *ctx, const te_memlist_t *in, te_memlist_t *out);

/**
 * \brief           This function performs an EEA3 encryption or decryption
 *                  operation and closes the session.
 *
 * \note            It is called after te_eea3_start(), te_eea3_update() or te_eea3_uplist().
 *                  The current session will be finished after calling this function.
 *
 * \param[in] ctx    The EEA3 context.
 * \param[in] inbits The length of the input data in bit.
 * \param[in] in     The buffer holding the original message.
 * \param[out] out   The buffer holding the processed message.
 * \return           \c TE_SUCCESS on success.
 * \return           \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return           \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return           \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL, \p in / or \p out
 *                      is \c NULL and \p inbits > 0 .
 * \return           \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eea3_finup(te_eea3_ctx_t *ctx, uint64_t inbits, const uint8_t *in, uint8_t *out);

/**
 * \brief          This function finishes the EEA3 operation.
 *
 * \note           It is called after te_eea3_start(), te_eea3_update() or te_eea3_uplist().
 *
 * \param[in] ctx  The EEA3 context.
 * \return         \c TE_SUCCESS on success.
 * \return         \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL.
 * \return         \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return         \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eea3_finish(te_eea3_ctx_t *ctx);

/**
 * \brief           This function clones the state of a EEA3 operation.
 *
 * \note            This function will free the \p dst context before clone if
 *                  it pointed to a valid EEA3 context already.
 *
 * \param[in]  src  The source EEA3 context.
 * \param[out] dst  The destination EEA3 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p src or \p dst is \c NULL.
 * \return          \c TE_ERROR_BAD_FORMAT if \p src is invalid.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eea3_clone(const te_eea3_ctx_t *src, te_eea3_ctx_t *dst);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_EEA3_H__ */