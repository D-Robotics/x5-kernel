//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_EIA3_H__
#define __TRUSTENGINE_EIA3_H__

#include "driver/te_drv_sca.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine eia3 context structure
 */
typedef struct te_eia3_ctx {
    te_crypt_ctx_t *crypt;
} te_eia3_ctx_t;

/**
 * \brief           This function gets the private ctx of a EIA3 ctx.
 *
 * \param[in] ctx   The EIA3 context.
 * \return          The private context pointer.
 */
static inline void* eia3_priv_ctx(te_eia3_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the EIA3 context \p ctx.
 *
 * \note            For main algorithm of ZUC only.
 *
 * \param[out] ctx  The EIA3 context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_BAD_PARAMS if params are invalid.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eia3_init(te_eia3_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg);

/**
 * \brief           This function withdraws the EIA3 context \p ctx.
 *
 * \param[in] ctx   The EIA3 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eia3_free(te_eia3_ctx_t *ctx);

/**
 * \brief             This function sets up the user key for the specified
 *                    EIA3 context \p ctx.
 *
 * \note              Only 128-bits \p keybits is valid.
 *
 * \param[in] ctx     The EIA3 context.
 * \param[in] key     The buffer holding the user key.
 * \param[in] keybits The EIA3 key length in bit.
 * \return            \c TE_SUCCESS on success.
 * \return            \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return            \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL.
 * \return            \c TE_ERROR_BAD_KEY_LENGTH if \p keybits isn't \c 128.
 * \return            \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eia3_setkey(te_eia3_ctx_t *ctx, const uint8_t *key, uint32_t keybits);

/**
 * \brief           This function sets up the secure key for the specified
 *                  EIA3 context \p ctx.
 *
 * \param[in] ctx   The EIA3 context.
 * \param[in] key   The secure key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL or
 *                     \p key->sel is invalid.
 * \return          \c TE_ERROR_BAD_KEY_LENGTH if \p key->ek3bits isn't \c 128.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eia3_setseckey(te_eia3_ctx_t *ctx, const te_sec_key_t *key);

/**
 * \brief            This function starts an EIA3 integrity session
 *                   with packaged IV.
 *
 * \param[in] ctx    EIA3 context \p ctx.
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
int te_eia3_start(te_eia3_ctx_t *ctx, uint32_t count, uint32_t bearer, uint32_t dir);

/**
 * \brief           This function performs an EIA3 integrity operation.
 *
 * \note            It is called between te_eia3_start() and te_eia3_finish().
 *                  Could be called repeatedly.
 *
 * \param[in] ctx   The EIA3 context.
 * \param[in] inlen The length of the input data in byte.
 * \param[in] in    The buffer holding the original message.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL or \p in /
 *                     is \c NULL and \p inlen > 0 .
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eia3_update(te_eia3_ctx_t *ctx, size_t inlen, const uint8_t *in);

/**
 * \brief           This function performs an EIA3 integrity operation.
 *
 * \note            It is called between te_eia3_start() and te_eia3_finish().
 *                  Could be called repeatedly.
 *
 * \param[in] ctx   The EIA3 context.
 * \param[in] in    The list of buffers holding the original message.
 * \param[out] out  The list of buffers holding the processed message.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL .
 * \return          \c TE_ERROR_BAD_INPUT_DATA if \p in->nent > 0 and \p in->ents is \c NULL .
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eia3_uplist(te_eia3_ctx_t *ctx, const te_memlist_t *in);

/**
 * \brief           This function performs an EIA3 integrity
 *                  operation and closes the session.
 *
 * \note            It is called after te_eia3_start(), te_eia3_update() or te_eia3_uplist().
 *                  The current session will be finished after calling this function.
 *
 * \param[in] ctx    The EIA3 context.
 * \param[in] inbits The length of the input data in bit.
 * \param[in] in     The buffer holding the original message.
 * \param[out] mac   The buffer holding the mac.
 * \return           \c TE_SUCCESS on success.
 * \return           \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return           \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return           \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL or \p in / or \p mac
 *                      is \c NULL and \p inbits > 0 .
 * \return           \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eia3_finup(te_eia3_ctx_t *ctx, uint64_t inbits, const uint8_t *in, uint8_t mac[4]);

/**
 * \brief          This function finish an EIA3 integrity session.
 *
 * \note           It is called after te_eia3_start(), te_eia3_update() or te_eia3_uplist().
 *
 * \param[in] ctx  The EIA3 context.
 * \param[out] mac The buffer holding the mac.
 * \return         \c TE_SUCCESS on success.
 * \return         \c TE_ERROR_BAD_PARAMS if \p ctx or \p mac is \c NULL.
 * \return         \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return         \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eia3_finish(te_eia3_ctx_t *ctx, uint8_t mac[4]);

/**
 * \brief           This function clones the state of a EIA3 operation.
 *
 * \note            This function will free the \p dst context before clone if
 *                  it pointed to a valid EIA3 context already.
 *
 * \param[in]  src  The source EIA3 context.
 * \param[out] dst  The destination EIA3 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p src or \p dst is \c NULL.
 * \return          \c TE_ERROR_BAD_FORMAT if \p src is invalid.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_eia3_clone(const te_eia3_ctx_t *src, te_eia3_ctx_t *dst);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_EIA3_H__ */