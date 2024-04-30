//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_F9_H__
#define __TRUSTENGINE_F9_H__

#include "driver/te_drv_sca.h"

#ifdef __cplusplus
extern "C" {
#endif   /* __cplusplus__ */

#ifndef __ASSEMBLY__

/**
 * Trust engine f9 context structure
 */
 typedef struct te_f9_ctx {
     te_crypt_ctx_t *crypt;
 } te_f9_ctx_t;

/**
 * \brief           This function gets the private ctx of a f9 ctx.
 * \param[in] ctx   The f9 context.
 * \return          The private context pointer.
 */
static inline void* f9_priv_ctx(const te_f9_ctx_t* ctx)
{
    return crypt_priv_ctx(ctx->crypt);
}

/**
 * \brief           This function initializes the f9 context \p ctx.
 * \param[out] ctx  The f9 context.
 * \param[in] hdl   The driver handler.
 * \param[in] malg  The main algorithm. Main algorithm of KASUMI only.
 *
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_NOT_SUPPORTED if TE doesn't support the \p malg.
 * \return          \c TE_ERROR_BAD_PARAMS if params are invalid.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_f9_init(te_f9_ctx_t *ctx, te_drv_handle hdl, te_algo_t malg);

/**
 * \brief               This function withdraws the f9 context \p ctx.
 * \param[in] ctx       The f9 context.
 *
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return              \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 */
int te_f9_free(te_f9_ctx_t *ctx);

/**
 * \brief               This function sets up the user key for the specified
 *                      f9 context \p ctx.
 * \param[in] ctx       The f9 context.
 * \param[in] key       The buffer holding the user key.
 * \param[in] keybits   The cipher key length in bit.
 *
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return              \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL.
 * \return              \c TE_ERROR_BAD_KEY_LENGTH if \p keybits isn't \c 128.
 */
int te_f9_setkey(te_f9_ctx_t *ctx,
                 const uint8_t *key,
                 uint32_t keybits);

/**
 * \brief           This function sets up the secure key for the specified
 *                  f9 context \p ctx. Main algorithm of KASUMI only.
 * \param[in] ctx   The f9 context.
 * \param[in] key   The secure key.
 *
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx or \p key is \c NULL or
 *                     \p key->sel is invalid.
 * \return          \c TE_ERROR_BAD_KEY_LENGTH if \p key->ek3bits isn't \c 128.
 */
int te_f9_setseckey(te_f9_ctx_t *ctx,
                    te_sec_key_t *key);


/**
 * \brief               This function performs a f9 start operation.
 * \param[in] ctx       The f9 context.
 * \param[in] count     Frame dependent input (32-bits).
 * \param[in] fresh     Random number FRESH (5-bits).
 * \param[in] dir       Direction of transmission (1-bit).
 *
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return              \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return              \c TE_ERROR_BAD_PARAMS on one of below conditions:
 *                          - \p ctx is invalid pointer.
 *                          - \p ctx is not initialized.
 *                          - \p dir is greater than 0x1 (0001 0001b)
 * \return              \c TE_ERROR_OOM if memory malloc failed.
 */
int te_f9_start(te_f9_ctx_t *ctx,
                uint32_t count,
                uint32_t fresh,
                uint32_t dir);

/**
 * \brief               This function performs a f9 update operation.
 * \param[in] ctx       The f9 context.
 * \param[in] inlen     The length of input data. Must be multiple of block size.
 * \param[in] in        The buffer holding the input data.
 *
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return              \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return              \c TE_ERROR_BAD_PARAMS  on one of below conditions:
 *                          - \p ctx is invalid pointer.
 *                          - \p in is invalid pointer and \p inlen is non-zero.
 */
int te_f9_update(te_f9_ctx_t *ctx,
                 size_t inlen,
                 const uint8_t *in);

/**
 * \brief               This function performs a f9 uplist operation.
 * \param[in] ctx       The f9 context.
 * \param[in] in        The linklist buffer holding the input data.
 *
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return              \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return              \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL .
 * \return              \c TE_ERROR_BAD_INPUT_DATA if \p in->nent > 0
 *                         and \p in->ents is \c NULL .
 */
int te_f9_uplist(te_f9_ctx_t *ctx, const te_memlist_t *in);

/**
 * \brief               This function performs a f9 finup operation.
 * \param[in] ctx       The f9 context.
 * \param[in] inbits    The length of input data.
 * \param[in] in        The buffer holding the input data.
 * \param[out] out      The buffer holding the output data.
 *
 * \return              \c TE_SUCCESS on success.
 * \return              \c TE_ERROR_BAD_STATE if invalid calling sequences.
 * \return              \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return              \c TE_ERROR_BAD_PARAMS if \p ctx is \c NULL or \p in / or \p mac
 *                         is \c NULL and \p inbits > 0 .
 * \return              \c TE_ERROR_OOM if memory malloc failed .
 */
int te_f9_finup(te_f9_ctx_t *ctx,
                uint64_t inbits,
                const uint8_t *in,
                uint8_t mac[4]);

/**
 * \brief           This function performs a f9 finish operation.
 * \param[in] ctx   The f9 context.
 * \param[out] mac  Mac[4]
 *
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p ctx or \p mac is \c NULL.
 * \return          \c TE_ERROR_BAD_FORMAT if \p ctx is invalid.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_f9_finish(te_f9_ctx_t *ctx, uint8_t mac[4]);

/**
 * \brief           This function performs a f9 clone operation.
 * \param[in] src   The f9 source context.
 * \param[out] dst  The f9 destination context.
 *
 * \return          \c TE_SUCCESS on success.
 * \return          \c TE_ERROR_BAD_PARAMS if \p src or \p dst is \c NULL.
 * \return          \c TE_ERROR_BAD_FORMAT if \p src is invalid.
 * \return          \c TE_ERROR_OOM if memory malloc failed.
 */
int te_f9_clone(const te_f9_ctx_t *src, te_f9_ctx_t *dst);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif  /* __cplusplus__ */

#endif /* __TRUSTENGINE_F9_H__ */