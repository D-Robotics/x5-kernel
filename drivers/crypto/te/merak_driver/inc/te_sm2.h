//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_SM2_H__
#define __TRUSTENGINE_SM2_H__

#include "te_ecp.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * The APIs are mainly from mbedtls/sm2dsa.h/sm2kep.h/sm2pke.h and sm2 in
 * openssl, with the following changes:
 *
 * 1. remove the group info/id, because sm2 uses fixed group.
 * 2. supports: sign/verify/dh, doesn't support encrypt/decrypt.
 * 3. all supported algorithm uses the hashed message as input.
 */

/**
 * The TE SM2 only supports standard P256 bits curve.
 */

/* SM2 async request structure */
typedef struct _te_sm2_request_t {
    te_async_request_t base; /**< base async request */
    void *internal_data[4];  /**< internal data used by te aca driver */
    union {
        struct _sm2dsa_sign_t {
            const te_bn_t *d;
            const uint8_t *buf;
            size_t size;
            te_bn_t *r;
            te_bn_t *s;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
        } sm2dsa_sign_args;
        struct _sm2dsa_verify_t {
            const uint8_t *buf;
            size_t size;
            const te_ecp_point_t *Q;
            const te_bn_t *r;
            const te_bn_t *s;
        } sm2dsa_verify_args;
        struct _sm2dsa_gen_keypair_t {
            const te_drv_handle hdl;
            te_bn_t *d;
            te_ecp_point_t *Q;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
        } sm2dsa_gen_keypair_args;
        struct _sm2dh_gen_public_t {
            const te_drv_handle hdl;
            te_bn_t *d;
            te_ecp_point_t *Q;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
        } sm2dh_gen_public_args;
        struct _sm2dh_compute_shared_t {
            const te_bn_t *d;
            const te_bn_t *tmp;
            const te_ecp_point_t *tmp_Q;
            const te_ecp_point_t *other_tmp_Q;
            const te_ecp_point_t *other_Q;
            te_ecp_point_t *K;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
        } sm2dh_compute_shared_args;
    };
} te_sm2_request_t;

/**
 * \brief           This function computes the SM2 signature of a
 *                  previously-hashed message.
 *
 * \see             te_ecp.h
 *
 * \param r         The bignumber context in which to store the first part
 *                  the signature. This must be initialized.
 * \param s         The bignumber context in which to store the second part
 *                  the signature. This must be initialized.
 * \param d         The private signing key. This must be initialized.
 * \param buf       The content to be signed. This is usually the hash of
 *                  the original data to be signed. This must be a readable
 *                  buffer of length \p size Bytes. It may be \c NULL if
 *                  \p size is zero.
 * \param size      The length of \p buf in Bytes.
 * \param f_rng     The RNG function. This must not be \c NULL.
 * \param p_rng     The RNG context to be passed to \p f_rng. This may be
 *                  \c NULL if \p f_rng doesn't need a context parameter.
 *
 * \return          See te error code.
 */
int te_sm2dsa_sign(const te_bn_t *d,
                   const uint8_t *buf,
                   size_t size,
                   te_bn_t *r,
                   te_bn_t *s,
                   int (*f_rng)(void *, uint8_t *, size_t),
                   void *p_rng);

/**
 * \brief           This function verifies the SM2 signature of a
 *                  previously-hashed message.
 *
 * \see             te_ecp.h
 *
 * \param buf       The hashed content that was signed. This must be a readable
 *                  buffer of length \p size Bytes. It may be \c NULL if
 *                  \p size is zero.
 * \param size      The length of \p buf in Bytes.
 * \param Q         The public key to use for verification. This must be
 *                  initialized and setup.
 * \param r         The first integer of the signature.
 *                  This must be initialized.
 * \param s         The second integer of the signature.
 *                  This must be initialized.
 * \return          See te error code.
 */
int te_sm2dsa_verify(const uint8_t *buf,
                     size_t size,
                     const te_ecp_point_t *Q,
                     const te_bn_t *r,
                     const te_bn_t *s);

/**
 *
 * \brief           This function generates an SM2 keypair.
 * \param[in]  hdl  The driver handler
 * \param[out] d    The destination MPI (secret part).
 *                  This must be initialized.
 * \param[out] Q    The destination point (public part).
 *                  This must be initialized.
 * \param[in] f_rng The RNG function. This must not be \c NULL.
 * \param[in] p_rng The RNG context to be passed to \p f_rng. This may
 *                  be \c NULL if \p f_rng doesn't need a context argument.
 * \return          See te error code.
 */
int te_sm2dsa_gen_keypair(const te_drv_handle hdl,
                          te_bn_t *d,
                          te_ecp_point_t *Q,
                          int (*f_rng)(void *, uint8_t *, size_t),
                          void *p_rng);

/**
 * \brief           This function generates an SM2DH keypair on an elliptic
 *                  curve. If the private key \p d is already exists, then
 *                  this function will derive the public key from private key.
 *
 *                  This function performs the first of two core computations
 *                  implemented during the SM2DH key exchange. The second core
 *                  computation is performed by te_sm2dh_compute_shared().
 *
 * \see             te_ecp.h
 * \param[in]  hdl  The driver handler
 * \param d         The destination MPI (private key).
 *                  This must be initialized.
 * \param Q         The destination point (public key).
 *                  This must be initialized.
 * \param f_rng     The RNG function to use. This must not be \c NULL.
 * \param p_rng     The RNG context to be passed to \p f_rng. This may be
 *                  \c NULL in case \p f_rng doesn't need a context argument.
 *
 * \return          See te error code.
 */
int te_sm2dh_gen_public(const te_drv_handle hdl,
                        te_bn_t *d,
                        te_ecp_point_t *Q,
                        int (*f_rng)(void *, uint8_t *, size_t),
                        void *p_rng);

/**
 * \brief           This function computes the shared secret.
 *
 *                  This function performs the second of two core computations
 *                  implemented during the SM2DH key exchange. The first core
 *                  computation is performed by te_sm2dh_gen_public().
 *
 * \see             te_ecp.h
 *
 * \note            If \p f_rng is not NULL, it is used to implement
 *                  countermeasures against side-channel attacks.
 *                  For more information, see te_ecp_mul().
 *
 * \param d             Our secret exponent (private key).
 * \param tmp           Our temporary private key.
 * \param tmp_Q         Our temporary public key.
 * \param other_tmp_Q   The temporary public key from another party.
 * \param other_Q       The public key from another party.
 * \param K             The destination point (used to compute shared secret).
 * \param f_rng     The RNG function. This may be \c NULL if randomization
 *                  of intermediate results during the ECP computations is
 *                  not needed (discouraged). See the documentation of
 *                  te_ecp_mul() for more.
 * \param p_rng     The RNG context to be passed to \p f_rng. This may be
 *                  \c NULL if \p f_rng is \c NULL or doesn't need a
 *                  context argument.
 * \return          See te error code.
 */
int te_sm2dh_compute_shared(const te_bn_t *d,
                            const te_bn_t *tmp,
                            const te_ecp_point_t *tmp_Q,
                            const te_ecp_point_t *other_tmp_Q,
                            const te_ecp_point_t *other_Q,
                            te_ecp_point_t *K,
                            int (*f_rng)(void *, uint8_t *, size_t),
                            void *p_rng);
int te_sm2dsa_sign_async(te_sm2_request_t *req);
int te_sm2dsa_verify_async(te_sm2_request_t *req);
int te_sm2dsa_gen_keypair_async(te_sm2_request_t *req);
int te_sm2dh_gen_public_async(te_sm2_request_t *req);
int te_sm2dh_compute_shared_async(te_sm2_request_t *req);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_SM2_H__ */
