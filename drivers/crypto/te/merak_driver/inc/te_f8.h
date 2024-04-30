//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_F8_H__
#define __TRUSTENGINE_F8_H__

#include "driver/te_drv_sca.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine f8 context structure
 */
 typedef struct te_f8_ctx {
     te_crypt_ctx_t *crypt;

 } te_f8_ctx_t;
/**
 * \brief           This function gets the private ctx of a f8 ctx.
 * \param[in] ctx   The f8 context.
 * \return          The private context pointer.
 */
static inline void* f8_priv_ctx(const te_f8_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the f8 context \p ctx.
 * \param[out] ctx  The f8 context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm. Main algorithm of KASUMI only.
 *
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_NOT_SUPPORTED if TE doesn't support the \p malg.
 * \return          \c TE_ERROR_BAD_PARAMS if params are invalid.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_f8_init(te_f8_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg);

/**
 * \brief           This function withdraws the f8 context \p ctx.
 * \param[in] ctx   The f8 context.
 *
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 */
int te_f8_free(te_f8_ctx_t *ctx);

/**
 * \brief               This function sets up the user key for the specified
 *                      f8 context \p ctx.
 * \param[in] ctx       The f8 context.
 * \param[in] key       The buffer holding the user key.
 * \param[in] keybits   The cipher key length in bit.
 *
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return              \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL.
 * \return              \c TE_ERROR_BAD_KEY_LENGTH if \p keybits isn't \c 128.
 */
int te_f8_setkey(te_f8_ctx_t *ctx,
                 const uint8_t *key,
                 uint32_t keybits);

/**
 * \brief               This function sets up the secure key for the specified
 *                      f8 context \p ctx. Main algorithm of KASUMI only.
 * \param[in] ctx       The f8 context.
 * \param[in] key       The secure key.
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return              \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL.
 * \return              \c TE_ERROR_BAD_KEY_LENGTH if \p keybits isn't \c 128.
 */
int te_f8_setseckey(te_f8_ctx_t *ctx,
                    te_sec_key_t *key);


/**
 * \brief               This function performs a f8 start operation.
 * \param[in] ctx       The f8 context.
 * \param[in] count     Frame dependent input(32-bits).
 * \param[in] bearer    Bearer identity(5-bits).
 * \param[in] dir       Direction of transmission(1-bit).
 *
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return              \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return              \c TE_ERROR_BAD_PARAMS on one of below conditions:
 *                          - \p ctx is invalid pointer.
 *                          - \p bearer is greater than 0x1F (0001 1111b)
 *                          - \p dir is greater than 0x1 (0001 0001b)
 * \return              \c TE_ERROR_OOM if memory malloc failed.
 */
int te_f8_start(te_f8_ctx_t *ctx,
                uint32_t count,
                uint32_t bearer,
                uint32_t dir);

/**
 * \brief               This function performs a f8 update operation.
 * \param[in] ctx       The f8 context.
 * \param[in] inlen     The length of input data. Must be multiple of block size.
 * \param[in] in        The buffer holding the input data.
 * \param[out] out      The buffer holding the output data.
 *
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return              \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return              \c TE_ERROR_BAD_PARAMS  on one of below conditions:
 *                          - \p ctx is invalid pointer.
 *                          - \p in is invalid pointer and \p inlen is non-zero.
 *                          - \p out is invalid pointer.
 */
int te_f8_update(te_f8_ctx_t *ctx,
                 size_t inlen,
                 const uint8_t *in,
                 uint8_t *out);

/**
 * \brief               This function performs a f8 uplist operation.
 * \param[in] ctx       The f8 context.
 * \param[in] in        The linklist buffer holding the input data.
 * \param[in] out       The linklist buffer holding the output data.
 *
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return              \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return              \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL or \p in / \p out
 *                      are \c NULL.
 * \return              \c TE_ERROR_BAD_INPUT_DATA if \p in->nent > 0 and \p in->ents is \c NULL .
 */
int te_f8_uplist(te_f8_ctx_t *ctx,
                 const te_memlist_t *in,
                 te_memlist_t *out);

/**
 * \brief               This function performs a f8 finup operation.
 * \param[in] ctx       The f8 context.
 * \param[in] inbits    The length of input data.
 * \param[in] in        The buffer holding the input data.
 * \param[out] out      The buffer holding the output data.
 *
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return              \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return              \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL, \p in / or \p out
 *                      is \c NULL and \p inbits > 0 .
 * \return              \c TE_ERROR_OOM if memory malloc failed.
 */
int te_f8_finup(te_f8_ctx_t *ctx,
                uint64_t inbits,
                const uint8_t *in,
                uint8_t *out);

/**
 * \brief           This function performs a f8 finish operation.
 * \param[in] ctx   The f8 context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_f8_finish(te_f8_ctx_t *ctx);

/**
 * \brief           This function performs a f8 clone operation.
 * \param[in] src   The f8 source context.
 * \param[out] dst  The f8 destination context.
 *
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p src or \p dst is \c NULL.
 * \return          \c TE_ERROR_BAD_FORMAT if \p src is invalid.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_f8_clone(const te_f8_ctx_t *src, te_f8_ctx_t *dst);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif /* __cplusplus__ */

#endif /* __TRUSTENGINE_F8_H__ */
