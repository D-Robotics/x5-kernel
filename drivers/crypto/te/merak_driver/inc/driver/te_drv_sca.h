//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DRV_SCA_H__
#define __TRUSTENGINE_DRV_SCA_H__

#include "te_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCA_MAX_BLOCK_SIZE TE_MAX_SCA_BLOCK

#ifndef __ASSEMBLY__

struct te_hwa_sca;
struct sca_pm_ctx;
struct sca_worker;
struct sca_prvdata;

/**
 * Trust Engine sca key
 */
typedef struct te_sca_key {
    te_key_type_t type;             /**< key type */
    /**
     * key descriptor
     */
    union {
        te_sec_key_t sec;           /**< secure key */
        te_sec_key_v2_t sec_v2;     /**< secure key for key-ladder 256 */
        te_user_key_t user;         /**< user key */
    };

    /**
     * secondary key (user key only).
     * some algs require for a secondary key, i.e. GCM mode.
     */
    te_user_key_t user2;
} te_sca_key_t;

/**
 * SCA operation mode
 */
typedef enum te_sca_operation {
    TE_DRV_SCA_ENCRYPT = 0,         /**< encryption mode */
    TE_DRV_SCA_DECRYPT,             /**< decryption mode */
} te_sca_operation_t;

/**
 * SCA context state enumeration
 */
typedef enum te_sca_state {
    TE_DRV_SCA_STATE_RAW = 0,
    TE_DRV_SCA_STATE_INIT,
    TE_DRV_SCA_STATE_READY,
    TE_DRV_SCA_STATE_START,
    TE_DRV_SCA_STATE_UPDATE,
    TE_DRV_SCA_STATE_LAST,
} te_sca_state_t;

/**
 * SCA driver magic number
 */
#define SCA_DRV_MAGIC   0x64414353U /**< "SCAd" */

/**
 * SCA driver structure
 */
typedef struct te_sca_drv {
    te_crypt_drv_t base;            /**< base driver */
    uint32_t magic;                 /**< SCA driver magic */
    void *sctx;                     /**< session context */
    struct sca_pm_ctx *pm;          /**< SCA PM context */
    struct sca_prvdata *pd;        /**< SCA private data */
#ifdef CFG_TE_ASYNC_EN
    struct sca_worker *worker;     /**< Worker ptr */
#endif
} te_sca_drv_t;

/**
 * \brief           This function initializes the supplied SCA driver instance
 *                  \p drv by binding it to the given SCA \p hwa.
 * \param[in] drv   The SCA driver instance.
 * \param[in] hwa   The SCA HWA instance.
 * \param[in] name  The SCA driver name. Or NULL to ignore.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_drv_init( te_sca_drv_t *drv,
                     const struct te_hwa_sca *hwa,
                     const char* name );

/**
 * \brief           This function withdraws the supplied SCA driver instance
 *                  \p drv.
 * \param[in] drv   The SCA driver instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_drv_exit( te_sca_drv_t *drv );

/**
 * \brief           This function allocates and initializes a SCA context with
 *                  the supplied main algorithm \p malg. A per algorithm private
 *                  context, with length specified by \p size, is allocated
 *                  (if needed) and linked to the SCA context too (pointed by
 *                  sca->base.__ctx).
 *
 * \param[in] drv   The SCA driver instance.
 * \param[in] malg  The main algorithm.
 * \param[in] size  Length of the private context. Or 0 to ignore.
 * \param[out] ctx  The buffer holding the associated crypto context pointer on
 *                  success.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_alloc_ctx( struct te_sca_drv *drv,
                      te_algo_t malg,
                      uint32_t size,
                      te_crypt_ctx_t **ctx );

/**
 * \brief           This function withdraws the SCA context associated with the
 *                  supplied crypto context \p ctx.
 * \param[in] ctx   The SCA crypto context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_free_ctx( te_crypt_ctx_t *ctx );

/**
 * \brief           This function sets key for the SCA context associated with
 *                  the supplied crypto context \p ctx.
 * \param[in] ctx   The SCA crypto context.
 * \param[in] key   The SCA key.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_setkey( te_crypt_ctx_t *ctx, te_sca_key_t *key );

/**
 * \brief           This function resets the SCA operation associated with the
 *                  supplied crypto context \p ctx and prepares the computation
 *                  of another message with the same key as the previous SCA
 *                  operation.
 * \param[in] ctx   The SCA crypto context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_reset( te_crypt_ctx_t *ctx );

/**
 * \brief           This function polls the state of the SCA operation
 *                  associated with the supplied crypto context \p ctx.
 * \param[in] ctx   The SCA crypto context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_state( te_crypt_ctx_t *ctx );

/**
 * \brief           This function starts a SCA encryption or decryption
 *                  operation on the SCA context associated with the supplied
 *                  crypto context \p ctx.
 *
 *                  The initialization vector \p iv might includes one or two
 *                  concatenated IV on start, which could be deduced by the
 *                  specified length \p ivlen.
 *
 * \param[in] ctx   The SCA crypto context.
 * \param[in] op    The operation mode.
 * \param[in] iv    The initialization vector(s).
 * \param[in] ivlen The length of IV.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_start( te_crypt_ctx_t *ctx,
                  te_sca_operation_t op,
                  uint8_t *iv,
                  uint32_t ivlen );

/**
 * \brief           This function feeds an input buffer into an ongoing SCA
 *                  encryption or decryption operation.
 *
 *                  It can be called repeatedly. However, whenever \p islast
 *                  flag is set in a call, neither te_sca_update() nor
 *                  te_sca_uplist() is callable again on to the ongoing
 *                  SCA operation.
 *
 * \param[in] ctx    The SCA crypto context.
 * \param[in] islast True for the last data block. Or false otherwise.
 * \param[in] len    The length of the input data.
 * \param[in] in     The buffer holding the input data.
 * \param[out] out   The buffer holding th output data.
 * \return           \c TE_SUCCESS on success.
 * \return           \c <0 on failure.
 */
int te_sca_update( te_crypt_ctx_t *ctx,
                   bool islast,
                   size_t len,
                   const uint8_t *in,
                   uint8_t *out );

/**
 * \brief           This function feeds a list of input buffers into an ongoing
 *                  SCA encryption or decryption operation.
 *
 *                  It can be called repeatedly. However, whenever \p islast
 *                  flag is set in a call, neither te_sca_update() nor
 *                  te_sca_uplist() is callable again on to the ongoing
 *                  SCA operation.
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
 *                  The total length of the input and output buffers on a
 *                  te_sca_uplist() shall be equal.
 *
 * \param[in] ctx    The SCA crypto context.
 * \param[in] islast True for the last data block. Or false otherwise.
 * \param[in] in     The list of buffers holding the input data.
 * \param[out] out   The list of buffers holding th output data.
 * \return           \c TE_SUCCESS on success.
 * \return           \c <0 on failure.
 */
int te_sca_uplist( te_crypt_ctx_t *ctx,
                   bool islast,
                   te_memlist_t *in,
                   te_memlist_t *out);

/**
 * \brief           This function finishes or terminates the SCA encryption or
 *                  decryption operation according to the present context state,
 *                  and optionally writes the calcuated tag value to providied
 *                  \p tag buffer.
 *
 *                  The \p tag buffer is required by AEAD and MAC operation
 *                  only. Otherwise set it to NULL.
 *
 * \param[in] ctx    The SCA crypto context.
 * \param[out] tag   The buffer holding the tag data.
 * \param[in] taglen The length of tag data, up to the block size.
 * \return           \c TE_SUCCESS on success.
 * \return           \c <0 on failure.
 */
int te_sca_finish( te_crypt_ctx_t *ctx,
                   uint8_t *tag,
                   uint32_t taglen );

/**
 * \brief           This function clones the state of a SCA operation.
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
 *                           '----'         '--> CMAC1
 *
 *       CLONED_CTX--.   DATA1--.
 *                   v          v                  ! .update() may not be called
 *      .start -> .clone -> .update() -> .finish()   at all in this scenario.
 *                           ^    |         |
 *                           '----'         '--> CMAC1
 *
 *         CLONED_CTX--.
 *  DATA0---.          |  DATA1--.
 *          v          v         v                  ! .update() may not be called
 *      .update -> .clone -> .update() -> .finish()   at all in this scenario.
 *       ^    |               ^    |         |
 *       '----'               '----'         '--> CMAC1

 * \param[in]  src  The source SCA context.
 * \param[out] dst  The destination SCA context.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_clone( const te_crypt_ctx_t *src,
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
 *  .setkey() -> .start() -> .update() -> .export()   at all in this scenario.
 *                            ^     |         |
 *                            '-----'         '--> PARTIAL_CMAC
 *
 * ------------ other calculations happen here -----------
 *
 *     PARTIAL_CMAC--.   DATA1--.
 *                   v          v                  ! .update() may not be called
 * .alloc_ctx -> .import -> .update() -> .finish()   at all in this scenario.
 *                           ^    |         |
 *                           '----'         '--> CMAC1
 *
 *            PARTIAL_CMAC--.   DATA1--.
 *                          v          v
 * .setkey -> .start -> .import -> .update() -> .finish()
 *                                  ^    |         |
 *                                  '----'         '--> CMAC1
 *
 *                       PARTIAL_CMAC--.
 *              DATA0-------.          |   DATA1--.
 *                          v          v          v
 * .setkey -> .start -> .update -> .import -> .update() -> .finish()
 *                       ^     |               ^    |         |
 *                       '-----'               '----'         '--> CMAC1
 *
 * \param[in]  ctx  The SCA crypto context.
 * \param[out] out  Buffer filled with the state data on success.
 * \param[inout] olen Size of \p out buffer on input.
 *                    Length of data filled in the \p out buffer on success.
 *                    Required \p out buffer length on TE_ERROR_SHORT_BUFFER.
 * \return          \c TE_SUCCESS on success.
 *                  \c TE_ERROR_SHORT_BUFFER if *olen is less than required.
 * \return          \c <0 on failure.
 */
int te_sca_export( te_crypt_ctx_t *ctx,
                   void *out,
                   uint32_t *olen );

/**
 * \brief           This function imports partial state of the calculation.
 *                  This function loads the entire state of the specified con-
 *                  text from a provided block of data so the calculation can
 *                  continue from this point onward.
 *
 * \param[in] ctx   The SCA crypto context.
 * \param[in] in    Buffer filled with the state data exported early.
 * \param[in] ilen  Size of the state data in the \p in buffer.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_import( te_crypt_ctx_t *ctx,
                   const void *in,
                   uint32_t ilen );

#ifdef CFG_TE_ASYNC_EN

/**
 * SCA async request structure
 */
typedef struct te_sca_request {
    te_async_request_t base;
    int res;                        /**< result */
    union {
        /**
         * start params
         */
        struct {
            te_sca_operation_t op;  /**< operation mode */
            uint8_t *iv;            /**< initialization vector */
            uint32_t ivlen;         /**< aead requires 2x iv, ivlen = 2x blk */
        } st;
        /**
         * update params
         */
        struct {
#define SCA_FLAGS_LAST (1 << 0)     /**< last update */
#define SCA_FLAGS_LIST (1 << 1)     /**< memlist, default to data */
            uint32_t flags;
            union {
                struct {
                    size_t len;
                    const uint8_t *src;
                    uint8_t *dst;
                } data;
                struct {
                    te_memlist_t src;
                    te_memlist_t dst;
                } lst;
            };
        } up;
        /**
         * finish params
         */
        struct {
            uint8_t *tag;
            uint32_t taglen;
        } fin;
    };
} te_sca_request_t;

/**
 * \brief           This function starts a SCA encryption or decryption
 *                  operation on the SCA context associated with the supplied
 *                  crypto context \p ctx asynchronously.
 *
 * \param[in] ctx   The SCA crypto context.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_astart( te_crypt_ctx_t *ctx,
                   te_sca_request_t *req );

/**
 * \brief           This function feeds a list of input buffers into an ongoing
 *                  SCA encryption or decryption operation asynchronously.
 *
 *                  It can be called repeatedly. However, if SCA_FLAGS_LAST
 *                  flag is set in a request, none of te_sca_aupdate(),
 *                  te_sca_update(), and te_sca_uplist() is callable again on
 *                  to the ongoing SCA operation.
 *
 *                  The total length of the input and output buffers on a
 *                  te_sca_aupdate() shall be equal.
 *
 * \param[in] ctx   The SCA crypto context.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_aupdate( te_crypt_ctx_t *ctx,
                    te_sca_request_t *req );

/**
 * \brief           This function finishes or terminates the SCA encryption or
 *                  decryption operation asynchronously according to the present
 *                  context state, and optionally writes the calcuated tag value
 *                  to providied \p req->fin.tag buffer.
 *
 *                  The \p tag buffer is required by AEAD and MAC operation
 *                  only. Otherwise set it to NULL.
 *
 * \param[in] ctx   The SCA crypto context.
 * \param[in] req   The request instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sca_afinish( te_crypt_ctx_t *ctx,
                    te_sca_request_t *req );

#endif /* CFG_TE_ASYNC_EN */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_DRV_SCA_H__ */
