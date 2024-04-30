//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_ECDSA_H__
#define __TRUSTENGINE_ECDSA_H__

#include "te_ecp.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * The APIs are mainly from mbedtls/ecdsa.h with the following changes:
 *
 * 1. remove write/read signature which do asn.1 encoding/decoding.
 * 2. remove deterministic version of signing. User can supply a deterministic
 * version f_rng/p_rng to use deterministic signing.
 * 3. only supports: sign/verify/generate key/init/free 5 APIs, which satisfy
 * mbedtls porting with ECDSA ALTs:
 *  MBEDTLS_ECDSA_VERIFY_ALT
 *  MBEDTLS_ECDSA_SIGN_ALT
 *  MBEDTLS_ECDSA_GENKEY_ALT
 */

/* ECDSA async request structure */
typedef struct _te_ecdsa_request_t {
    te_async_request_t base; /**< base async request */
    void *internal_data[4];  /**< internal data used by te aca driver */
    union {
        struct _ecdsa_sign_t {
            const te_ecp_group_t *grp;
            const te_bn_t *d;
            const uint8_t *buf;
            size_t size;
            te_bn_t *r;
            te_bn_t *s;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
        } sign_args;
        struct _ecdsa_verify_t {
            const te_ecp_group_t *grp;
            const uint8_t *buf;
            size_t size;
            const te_ecp_point_t *Q;
            const te_bn_t *r;
            const te_bn_t *s;
        } verify_args;
    };
} te_ecdsa_request_t;

/**
 * \brief           This function computes the ECDSA signature of a
 *                  previously-hashed message.
 *
 * \note            If the bitlength of the message hash is larger than the
 *                  bitlength of the group order, then the hash is truncated
 *                  as defined in <em>Standards for Efficient Cryptography Group
 *                  (SECG): SEC1 Elliptic Curve Cryptography</em>, section
 *                  4.1.3, step 5.
 *
 * \see             te_ecp.h
 *
 * \param grp       The context for the elliptic curve to use.
 *                  This must be initialized and have group parameters
 *                  set, for example through te_ecp_group_load().
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
int te_ecdsa_sign(const te_ecp_group_t *grp,
                  const te_bn_t *d,
                  const uint8_t *buf,
                  size_t size,
                  te_bn_t *r,
                  te_bn_t *s,
                  int (*f_rng)(void *, uint8_t *, size_t),
                  void *p_rng);

/**
 * \brief           This function verifies the ECDSA signature of a
 *                  previously-hashed message.
 *
 * \note            If the bitlength of the message hash is larger than the
 *                  bitlength of the group order, then the hash is truncated as
 *                  defined in <em>Standards for Efficient Cryptography Group
 *                  (SECG): SEC1 Elliptic Curve Cryptography</em>, section
 *                  4.1.4, step 3.
 *
 * \see             te_ecp.h
 *
 * \param grp       The ECP group to use.
 *                  This must be initialized and have group parameters
 *                  set, for example through te_ecp_group_load().
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
int te_ecdsa_verify(const te_ecp_group_t *grp,
                    const uint8_t *buf,
                    size_t size,
                    const te_ecp_point_t *Q,
                    const te_bn_t *r,
                    const te_bn_t *s);

/**
 * \brief           This function generates an ECDSA keypair.
 *                  This function equals to te_ecp_gen_keypair
 *
 * \param[in] grp   The ECP group to generate a key pair for.
 *                  This must be initialized and have group parameters
 *                  set, for example through te_ecp_group_load().
 * \param[out] d    The destination MPI (secret part).
 *                  This must be initialized.
 * \param[out] Q    The destination point (public part).
 *                  This must be initialized.
 * \param[in] f_rng The RNG function. This must not be \c NULL.
 * \param[in] p_rng The RNG context to be passed to \p f_rng. This may
 *                  be \c NULL if \p f_rng doesn't need a context argument.
 * \return          See te error code.
 */
int te_ecdsa_gen_keypair(te_ecp_group_t *grp,
                         te_bn_t *d,
                         te_ecp_point_t *Q,
                         int (*f_rng)(void *, uint8_t *, size_t),
                         void *p_rng);
int te_ecdsa_sign_async(te_ecdsa_request_t *req);
int te_ecdsa_verify_async(te_ecdsa_request_t *req);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_ECDSA_H__ */
