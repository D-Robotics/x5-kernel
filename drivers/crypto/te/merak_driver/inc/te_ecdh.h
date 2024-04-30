//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_ECDH_H__
#define __TRUSTENGINE_ECDH_H__

#include "te_ecp.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * The APIs are mainly from mbedtls/ecdh.h with the following changes:
 * 1. only support two APIs: gen pubkey/compute shared, which satisfy mbedtls
 * porting with ALTs:
 *
 *  MBEDTLS_ECDH_GEN_PUBLIC_ALT
 *  MBEDTLS_ECDH_COMPUTE_SHARED_ALT
 */

/* ECDH async request structure */
typedef struct _te_ecdh_request_t {
    te_async_request_t base; /**< base async request */
    void *internal_data[4];  /**< internal data used by te aca driver */
    union {
        struct _ecdh_gen_public_t { /**< args for ecdh_gen_public */
            const te_ecp_group_t *grp;
            te_bn_t *d;
            te_ecp_point_t *Q;
            int (*f_rng)(void *, unsigned char *, size_t);
            void *p_rng;
        } gen_public_args;
        struct _ecdh_compute_shared_t { /**< args for ecdh_compute_shared */
            const te_ecp_group_t *grp;
            const te_bn_t *d;
            const te_ecp_point_t *other_Q;
            te_bn_t *K;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
        } compute_shared_args;
    };
} te_ecdh_request_t;

/**
 * \brief           This function generates an ECDH keypair on an elliptic
 *                  curve. If the private key \p d is already exists, then
 *                  this function will derive the public key from private key.
 *
 *                  This function performs the first of two core computations
 *                  implemented during the ECDH key exchange. The second core
 *                  computation is performed by te_ecdh_compute_shared().
 *
 * \see             te_ecp.h
 *
 * \param grp       The ECP group to use. This must be initialized and have
 *                  domain parameters loaded, for example through
 *                  te_ecp_load().
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
int te_ecdh_gen_public(const te_ecp_group_t *grp,
                       te_bn_t *d,
                       te_ecp_point_t *Q,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng);
/**
 * \brief           This function computes the shared secret.
 *
 *                  This function performs the second of two core computations
 *                  implemented during the ECDH key exchange. The first core
 *                  computation is performed by te_ecdh_gen_public().
 *
 * \see             te_ecp.h
 *
 * \note            If \p f_rng is not NULL, it is used to implement
 *                  countermeasures against side-channel attacks.
 *                  For more information, see te_ecp_mul().
 *
 * \param grp       The ECP group to use. This must be initialized and have
 *                  domain parameters loaded, for example through
 *                  te_ecp_load().
 * \param K         The destination MPI (shared secret).
 *                  This must be initialized.
 * \param other_Q   The public key from another party.
 *                  This must be initialized.
 * \param d         Our secret exponent (private key).
 *                  This must be initialized.
 * \param f_rng     The RNG function. This may be \c NULL if randomization
 *                  of intermediate results during the ECP computations is
 *                  not needed (discouraged). See the documentation of
 *                  te_ecp_mul() for more.
 * \param p_rng     The RNG context to be passed to \p f_rng. This may be
 *                  \c NULL if \p f_rng is \c NULL or doesn't need a
 *                  context argument.
 * \return          See te error code.
 */
int te_ecdh_compute_shared(const te_ecp_group_t *grp,
                           const te_bn_t *d,
                           const te_ecp_point_t *other_Q,
                           te_bn_t *K,
                           int (*f_rng)(void *, uint8_t *, size_t),
                           void *p_rng);

int te_ecdh_gen_public_async(te_ecdh_request_t *req);
int te_ecdh_compute_shared_async(te_ecdh_request_t *req);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_ECDH_H__ */
