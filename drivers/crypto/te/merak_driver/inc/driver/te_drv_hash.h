//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DRV_HASH_H__
#define __TRUSTENGINE_DRV_HASH_H__

#include "te_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define te_hwa_hash te_hwa_sca
struct te_hwa_sca;
struct hash_pm_ctx;
struct hash_prvdata;
struct hash_worker;

/**
 * HMAC key structure
 */
typedef struct te_hmac_key {
    te_key_type_t type;                  /**< key type */
    union {
        te_sec_key_t sec;                /**< secure key */
        te_sec_key_v2_t sec_v2;          /**< secure key of key-ladder 256 */
        uint8_t hkey[TE_MAX_HASH_BLOCK]; /**< hashed user key */
    };
} te_hmac_key_t;

/**
 * HASH context state enumeration
 */
typedef enum te_hash_state {
    TE_DRV_HASH_STATE_RAW = 0,
    TE_DRV_HASH_STATE_INIT,
    TE_DRV_HASH_STATE_START,
    TE_DRV_HASH_STATE_UPDATE,
    TE_DRV_HASH_STATE_LAST,
} te_hash_state_t;

/**
 * HASH driver magic number
 */
#define HASH_DRV_MAGIC  0x76724444U /**< "DDrv" */

/**
 * HASH driver structure
 */
typedef struct te_hash_drv {
    te_crypt_drv_t base;            /**< base driver */
    uint32_t magic;                 /**< hash driver magic */
    void *sctx;                     /**< session context */
    struct hash_pm_ctx *pm;         /**< PM context */
    struct hash_prvdata *pd;        /**< HASH private data */
#ifdef CFG_TE_ASYNC_EN
    struct hash_worker *worker;     /**< Worker ptr */
#endif
} te_hash_drv_t;

/**
 * \brief           This function initializes the supplied HASH driver instance
 *                  \p drv by binding it to the given HASH \p hwa.
 * \param[in] drv   The HASH driver instance.
 * \param[in] hwa   The HASH HWA instance.
 * \param[in] name  The HASH driver name. Or NULL to ignore.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_drv_init( te_hash_drv_t *drv,
                      const struct te_hwa_hash *hwa,
                      const char* name );

/**
 * \brief           This function withdraws the supplied HASH driver instance
 *                  \p drv.
 * \param[in] drv   The HASH driver instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_drv_exit( te_hash_drv_t *drv );

/**
 * \brief           This function allocates and initializes a HASH context with
 *                  the supplied main algorithm \p malg. A per algorithm private
 *                  context, with length specified by \p size, is allocated
 *                  (if needed) and linked to the HASH context too (pointed by
 *                  hash->base.__ctx).
 *
 * \param[in] drv   The HASH driver instance.
 * \param[in] malg  The main algorithm.
 * \param[in] size  Length of the private context. Or 0 to ignore.
 * \param[out] ctx  The buffer holding the associated crypto context pointer on
 *                  success.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_alloc_ctx( struct te_hash_drv *drv,
                       te_algo_t alg,
                       uint32_t size,
                       te_crypt_ctx_t **ctx );

/**
 * \brief           This function withdraws the HASH context associated with the
 *                  supplied crypto context \p ctx.
 * \param[in] ctx   The HASH crypto context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_free_ctx( te_crypt_ctx_t *ctx );

/**
 * \brief           This function polls the state of the HASH operation
 *                  associated with the supplied crypto context \p ctx.
 * \param[in] ctx   The HASH crypto context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_state( te_crypt_ctx_t *ctx );

/**
 * \brief           This function starts a HASH operation on the HASH context
 *                  associated with the supplied crypto context \p ctx.
 *
 *                  The key \p key is designed for HMAC operations only. Set it
 *                  to NULL for digest algorithms.
 *
 * \param[in] ctx   The HASH crypto context.
 * \param[in] key   The HMAC key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_start( te_crypt_ctx_t *ctx,
                   te_hmac_key_t *key );

/**
 * \brief           This function feeds an input buffer into an ongoing HASH
 *                  operation. Can be called repeatedly.
 *
 *                  The HASH driver accepts input data of zero-length on the
 *                  te_hash_update() function which is used as indicator for the
 *                  end of the input data stream.
 *
 * \param[in] ctx   The HASH crypto context.
 * \param[in] in    The buffer holding the input data.
 * \param[in] ilen  The length of the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_update( te_crypt_ctx_t *ctx,
                    const uint8_t *in,
                    size_t ilen );

/**
 * \brief           This function feeds a list of input buffers into an ongoing
 *                  HASH operation. Can be called repeatedly.
 *
 *                  The start address of a link list must be aligned to 128-bit
 *                  boundary at least.
 *
 *                  The address and size of each memory block shall be present
 *                  in little endian form.
 *
 *                  The block memory size uses the lower 32-bit only while the
 *                  higher 32-bit shall be set to 0.
 *
 *                  A block address of zero indicates the end of list.
 *
 *                  A te_hash_uplist() with a total length of zero indicates the
 *                  end of the input data stream.
 *
 * \param[in] ctx   The HASH crypto context.
 * \param[in] in    The list of buffers holding the input data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_uplist( te_crypt_ctx_t *ctx,
                    te_memlist_t *in );

/**
 * \brief           This function finishes or terminates the HASH operation
 *                  according to the present state, and writes the calculated
 *                  result to the \p out buffer.
 *
 *                  If none input data is fed before, a last zero PROC command
 *                  will be sent to the HASH engine before the FINISH command.
 *
 *                  If incomplete data block is buffered within the driver, a
 *                  last PROC command will be sent to the HASH engine implicitly
 *                  before the FINISH command.
 *
 * \param[in] ctx   The HASH crypto context.
 * \param[out] out  The buffer holding the output data.
 * \param[in] olen  The length of the output data, up to the block size.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_finish( te_crypt_ctx_t *ctx,
                    uint8_t *out,
                    uint32_t olen );

/**
 * \brief           This function resets the HASH operation associated with the
 *                  supplied crypto context \p ctx and prepares the computation
 *                  of another message, with the same key as the previous
 *                  operation if it is a HMAC operation.
 * \param[in] ctx   The HASH crypto context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_reset( te_crypt_ctx_t *ctx );

/**
 * \brief           This function clones the state of a HASH operation.
 *
 * Here is a schematic of how the .clone() functions is called:
 *
 * KEY--.                  DATA--.
 *      v                        v                 ! .update() may not be called
 *  .setkey() -> .start() -> .update() -> .clone()   at all in this scenario.
 *                            ^     |         |
 *                            '-----'         |
 *                                            |
 * -------- other calculations happen here ---+-------------
 *                                            |
 *                           CLONED_CTX       |
 *                   .------------------------'
 *                   |
 *                   |   DATA1--.
 *                   v          v                  ! .update() may not be called
 *  .alloc_ctx -> .clone -> .update() -> .finish()   at all in this scenario.
 *                           ^    |         |
 *                           '----'         '--> HASH1
 *
 *       CLONED_CTX--.   DATA1--.
 *                   v          v                  ! .update() may not be called
 *      .start -> .clone -> .update() -> .finish()   at all in this scenario.
 *                           ^    |         |
 *                           '----'         '--> HASH1
 *
 *        CLONED_CTX--.
 *  DATA0---.         |   DATA1--.
 *          v         v          v                  ! .update() may not be called
 *      .update -> .clone -> .update() -> .finish()   at all in this scenario.
 *       ^    |               ^    |         |
 *       '----'               '----'         '--> HASH1
 *
 * \param[in] src   The source HASH context.
 * \param[out] dst  The destination HASH context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_clone( const te_crypt_ctx_t *src,
                   te_crypt_ctx_t *dst );

/**
 * \brief           This function exports partial state of the calculation.
 *                  This function dumps the entire state of the specified con-
 *                  text into a provided block of data so it can be @import 'ed
 *                  back later on. This is useful in case you want to save
 *                  partial result of the calculation after processing certain
 *                  amount of data and reload this partial result multiple
 *                  times later on for multiple re-use.
 *
 * Here is a schematic of how the .export()/.import() functions are called:
 *
 * KEY--.                  DATA--.
 *      v                        v                 ! .update() may not be called
 *  .setkey() -> .start() -> .update() -> .export()  at all in this scenario.
 *                            ^     |         |
 *                            '-----'         '--> PARTIAL_HASH
 *
 * ------------ other calculations happen here -----------
 *
 *     PARTIAL_HASH--.   DATA1--.
 *                   v          v                  ! .update() may not be called
 * .alloc_ctx -> .import -> .update() -> .finish()   at all in this scenario.
 *                           ^    |         |
 *                           '----'         '--> HASH1
 *
 *            PARTIAL_HASH--.   DATA1--.
 *                          v          v
 * .setkey -> .start -> .import -> .update() -> .finish()
 *                                  ^    |         |
 *                                  '----'         '--> HASH1
 *
 *                       PARTIAL_HASH--.
 *              DATA0-------.          |   DATA1--.
 *                          v          v          v
 * .setkey -> .start -> .update -> .import -> .update() -> .finish()
 *                       ^     |               ^    |         |
 *                       '-----'               '----'         '--> HASH1
 *
 * \param[in]  ctx  The HASH crypto context.
 * \param[out] out  Buffer filled with the state data on success.
 * \param[inout] olen Size of \p out buffer on input.
 *                    Length of data filled in the \p out buffer on success.
 *                    Required \p out buffer length on TE_ERROR_SHORT_BUFFER.
 * \return          \c TE_SUCCESS on success.
 *                  \c TE_ERROR_SHORT_BUFFER if *olen is less than required.
 * \return          \c <0 on failure.
 */
int te_hash_export( te_crypt_ctx_t *ctx,
                    void *out,
                    uint32_t *olen );

/**
 * \brief           This function imports partial state of the calculation.
 *                  This function loads the entire state of the specified con-
 *                  text from a provided block of data so the calculation can
 *                  continue from this point onward.
 *
 * \param[in] ctx   The HASH crypto context.
 * \param[in] in    Buffer filled with the state data exported early.
 * \param[in] ilen  Size of the state data in the \p in buffer.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_import( te_crypt_ctx_t *ctx,
                    const void *in,
                    uint32_t ilen );

#ifdef CFG_TE_ASYNC_EN

/**
 * HASH async request structure
 */
typedef struct te_hash_request {
    te_async_request_t base;
    int res;                        /**< result */
    union {
        /**
         * start param
         */
        struct {
            te_hmac_key_t key;
        } st;
        /**
         * update params
         */
        struct {
#define HASH_FLAGS_LAST  (1 << 0)   /**< last update */
#define HASH_FLAGS_LIST  (1 << 1)   /**< memlist, default to data */
#define HASH_FLAGS_NOPAD (1 << 2)   /**< no pad on last, default to yes */
#define HASH_FLAGS_LE    (1 << 3)   /**< little engine, default to BE */
            uint32_t flags;
            union {
                struct {
                    const uint8_t *in;
                    size_t ilen;
                } data;
                struct {
                    te_memlist_t in;
                } lst;
            };
        } up;
        /**
         * finish params
         */
        struct {
            uint8_t *out;
            uint32_t olen;
        } fin;
        /**
         * clone params
         */
        struct {
            te_crypt_ctx_t *dst;
        } cl;
    };
} te_hash_request_t;

/**
 * \brief           This function starts a HASH operation associated with the
 *                  supplied crypto context \p ctx asynchronously.
 *
 * \param[in] ctx   The HASH crypto context.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_astart( te_crypt_ctx_t *ctx,
                    te_hash_request_t *req );

/**
 * \brief           This function feeds a list of input buffers into an ongoing
 *                  HASH operation asynchronously.
 *
 *                  It can be called repeatedly. However, if HASH_FLAGS_LAST
 *                  flag is set in a request, none of te_hash_aupdate(),
 *                  te_hash_update(), and te_hash_uplist() is callable again on
 *                  to the ongoing HASH operation.
 *
 *                  The total length of the input and output buffers on a
 *                  te_hash_aupdate() shall be equal.
 *
 * \param[in] ctx   The HASH crypto context.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_aupdate( te_crypt_ctx_t *ctx,
                     te_hash_request_t *req );

/**
 * \brief           This function finishes or terminates the HASH operation
 *                  asynchronously according to the present context state,
 *                  and writes the calcuated result to providied
 *                  \p req->fin.out buffer.
 *
 * \param[in] ctx   The HASH crypto context.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_afinish( te_crypt_ctx_t *ctx,
                     te_hash_request_t *req );

/**
 * \brief           This function clones the state of a HASH operation
 *                  asynchronously.
 *
 * \param[in] ctx   The source HASH crypto context.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_hash_aclone( te_crypt_ctx_t *ctx,
                    te_hash_request_t *req );

#endif /* CFG_TE_ASYNC_EN */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_DRV_HASH_H__ */
