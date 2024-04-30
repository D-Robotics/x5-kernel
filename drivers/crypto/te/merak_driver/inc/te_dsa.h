//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DSA_H__
#define __TRUSTENGINE_DSA_H__

#include "te_bn.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * \brief           This function computes the DSA signature of a
 *                  previously-hashed message.
 *
 * \note            If the bitlength of the message hash is larger than the
 *                  bitlength of the Q size, then the hash is truncated
 *                  as defined in FIPS 186-4 4.6.
 *
 * \param[in] P     The prime modulus.
 * \param[in] Q     The sub-prime.
 * \param[in] G     The generater.
 * \param[in] x     The private key.
 * \param[in] buf   The content to be signed. This is usually the hash of
 *                  the original data to be signed. This must be a readable
 *                  buffer of length \p size Bytes. It may be \c NULL if
 *                  \p size is zero.
 * \param[in] size  The length of \p buf in Bytes.
 * \param[out] r    The bignumber context in which to store the first part
 *                  the signature. This must be initialized.
 * \param[out] s    The bignumber context in which to store the second part
 *                  the signature. This must be initialized.
 * \param[in] f_rng     The RNG function. This must not be \c NULL.
 * \param[in] p_rng     The RNG context to be passed to \p f_rng. This may be
 *                  \c NULL if \p f_rng doesn't need a context parameter.
 *
 * \return          See te error code.
 */
int te_dsa_sign(const te_bn_t *P,
                const te_bn_t *Q,
                const te_bn_t *G,
                const te_bn_t *x,
                const uint8_t *buf,
                size_t size,
                te_bn_t *r,
                te_bn_t *s,
                int (*f_rng)(void *, uint8_t *, size_t),
                void *p_rng);

/**
 * \brief           This function verifies the DSA signature of a
 *                  previously-hashed message.
 *
 * \param[in] P     The prime modulus.
 * \param[in] Q     The sub-prime.
 * \param[in] G     The generater.
 * \param[in] y     The public key.
 * \param[in] buf   The content to be verified. This is usually the hash of
 *                  the original data to be signed. This must be a readable
 *                  buffer of length \p size Bytes.
 * \param[in] size  The length of \p buf in Bytes.
 * \param[in] r     The first part the signature. This must be initialized.
 * \param[in] s     The second part the signature. This must be initialized.
 * \param[in] f_rng     The RNG function. This must not be \c NULL.
 * \param[in] p_rng     The RNG context to be passed to \p f_rng. This may be
 *                  \c NULL if \p f_rng doesn't need a context parameter.
 *
 * \return          See te error code.
 */
int te_dsa_verify(const te_bn_t *P,
                  const te_bn_t *Q,
                  const te_bn_t *G,
                  const te_bn_t *y,
                  const uint8_t *buf,
                  size_t size,
                  const te_bn_t *r,
                  const te_bn_t *s);
/**
 * \brief           This function generates an DSA keypair.
 *
 * \param[in] P     The prime modulus.
 * \param[in] Q     The sub-prime.
 * \param[in] G     The generater.
 * \param[out] x    The private key.
 * \param[out] y    The public key.
 * \param[in] f_rng The RNG function. This must not be \c NULL.
 * \param[in] p_rng The RNG context to be passed to \p f_rng. This may
 *                  be \c NULL if \p f_rng doesn't need a context argument.
 * \return          See te error code.
 */
int te_dsa_gen_keypair(const te_bn_t *P,
                       const te_bn_t *Q,
                       const te_bn_t *G,
                       te_bn_t *x,
                       te_bn_t *y,
                       int (*f_rng)(void *, uint8_t *, size_t),
                       void *p_rng);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_ECDSA_H__ */
