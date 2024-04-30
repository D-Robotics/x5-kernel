//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_RSA_H__
#define __TRUSTENGINE_RSA_H__

#include "te_bn.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/* RSA async request structure */
typedef struct _te_rsa_request_t {
    te_async_request_t base; /**< base async request */
    void *internal_data[4];  /**< internal data used by te aca driver */
    union {
        struct _rsa_complete_key_t {
            te_bn_t *N;
            te_bn_t *E;
            te_bn_t *D;
            te_bn_t *P;
            te_bn_t *Q;
            te_bn_t *DP;
            te_bn_t *DQ;
            te_bn_t *QP;
        } complete_key_args;

        struct _rsa_gen_key_t {
            const te_drv_handle hdl;
            te_bn_t *N;
            te_bn_t *E;
            te_bn_t *D;
            te_bn_t *P;
            te_bn_t *Q;
            te_bn_t *DP;
            te_bn_t *DQ;
            te_bn_t *QP;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
            int32_t nbits;
            int32_t exponent;
        } gen_key_args;
        struct _rsa_public_t {
            const te_bn_t *N;
            const te_bn_t *E;
            const uint8_t *input;
            uint8_t *output;
            size_t size;
        } public_args;
        struct _rsa_private_t {
            const te_bn_t *N;
            const te_bn_t *E;
            const te_bn_t *D;
            const te_bn_t *P;
            const te_bn_t *Q;
            const te_bn_t *DP;
            const te_bn_t *DQ;
            const te_bn_t *QP;
            te_bn_t *pN;
            te_bn_t *Vi;
            te_bn_t *Vf;
            const uint8_t *input;
            uint8_t *output;
            size_t size;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
        } private_args;
    };
} te_rsa_request_t;

int te_rsa_complete_key_async(te_rsa_request_t *req);
int te_rsa_gen_key_async(te_rsa_request_t *req);
int te_rsa_public_async(te_rsa_request_t *req);
int te_rsa_private_async(te_rsa_request_t *req);
/**
 * \brief          This function completes all the RSA keys from
 *                 a set of imported core parameters.
 *
 *                 To setup an RSA public key, precisely \p N and \p E
 *                 must have been imported.
 *
 *                 To setup an RSA private key, sufficient information must
 *                 be present for the other parameters to be derivable.
 *
 *                 The default implementation supports the following:
 *                 1. Derive \p P, \p Q from \p N, \p D, \p E.
 *                 2. Derive \p N, \p D from \p P, \p Q, \p E.
 *                 3. Derive \p DP, \p DQ, \p QP from \p P, \p Q, \p D
 *
 *                 If the output parameter is NULL, will ignore the deduce.
 *
 * \warning        This function need not perform consistency checks
 *                 for the imported parameters. In particular, parameters that
 *                 are not needed by the implementation might be silently
 *                 discarded and left unchecked. To check the consistency
 *                 of the key material, see mbedtls_rsa_check_privkey().
 *
 * \param[inout] N      The public modulus
 * \param[inout] E      The public exponent
 * \param[inout] D      The private exponent
 * \param[inout] P      The first prime factor
 * \param[inout] Q      The second prime factor
 * \param[inout] DP     D % (P - 1)
 * \param[inout] DQ     D % (Q - 1)
 * \param[inout] QP     1 / (Q % P)
 *
 * \return          See te error code.
 */
int te_rsa_complete_key(te_bn_t *N,
                        te_bn_t *E,
                        te_bn_t *D,
                        te_bn_t *P,
                        te_bn_t *Q,
                        te_bn_t *DP,
                        te_bn_t *DQ,
                        te_bn_t *QP);

/**
 * \brief          This function generates an RSA keypair.
 *
 *                 If the output parameter is NULL, will not save to it.
 * \param[in]  hdl    The driver handler
 * \param[out] N      The public modulus
 * \param[out] E      The public exponent
 * \param[out] D      The private exponent
 * \param[out] P      The first prime factor
 * \param[out] Q      The second prime factor
 * \param[out] DP     D % (P - 1)
 * \param[out] DQ     D % (Q - 1)
 * \param[out] QP     1 / (Q % P)
 * \param[in] f_rng   The RNG function to be used for key generation.
 *                    This must not be \c NULL.
 * \param[in] p_rng   The RNG context to be passed to \p f_rng.
 *                    This may be \c NULL if \p f_rng doesn't need a context.
 * \param nbits  The size of the public key in bits.
 * \param exponent    The public exponent to use. For example, \c 65537.
 *                    This must be odd and greater than \c 1.
 *
 * \return          See te error code.
 */
int te_rsa_gen_key(const te_drv_handle hdl,
                   te_bn_t *N,
                   te_bn_t *E,
                   te_bn_t *D,
                   te_bn_t *P,
                   te_bn_t *Q,
                   te_bn_t *DP,
                   te_bn_t *DQ,
                   te_bn_t *QP,
                   int (*f_rng)(void *, uint8_t *, size_t),
                   void *p_rng,
                   int32_t nbits,
                   int32_t exponent);

/**
 * \brief          This function performs an RSA public key operation.
 *
 * \param[out] N      The public modulus
 * \param[out] E      The public exponent
 *
 * \param input    The input buffer. This must be a readable buffer
 *                 of length key Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 * \param output   The output buffer. This must be a writable buffer
 *                 of length key Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * \note           This function does not handle message padding.
 *
 * \note           Make sure to set \p input[0] = 0 or ensure that
 *                 input is smaller than \p N.
 *
 * \return          See te error code.
 */
int te_rsa_public(const te_bn_t *N,
                  const te_bn_t *E,
                  const uint8_t *input,
                  uint8_t *output,
                  size_t size);

/**
 * \brief          This function performs an RSA private key operation.
 *
 * \note           This function does not handle message padding.
 * \note           Blinding is used if and only if a PRNG is provided.
 *
 * \note           If blinding is used, both the base of exponentation
 *                 and the exponent are blinded, providing protection
 *                 against some side-channel attacks.
 *
 * \warning        It is deprecated and a security risk to not provide
 *                 a PRNG here and thereby prevent the use of blinding.
 *                 Future versions of the library may enforce the presence
 *                 of a PRNG.
 *
 * \param[out] N      The public modulus
 * \param[out] E      The public exponent
 * \param[out] D      The private exponent
 * \param[out] P      The first prime factor
 * \param[out] Q      The second prime factor
 * \param[out] DP     D % (P - 1)
 * \param[out] DQ     D % (Q - 1)
 * \param[out] QP     1 / (Q % P)
 * \param pN          The blinding pointer, can be NULL if f_rng is also NULL.
 * \param Vi          The blinding pointer, can be NULL if f_rng is also NULL.
 * \param Vf          The blinding pointer, can be NULL if f_rng is also NULL.
 *
 * \param input    The input buffer. This must be a readable buffer
 *                 of length key Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 * \param output   The output buffer. This must be a writable buffer
 *                 of length key Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * \param f_rng    The RNG function, used for blinding. It is discouraged
 *                 and deprecated to pass \c NULL here, in which case
 *                 blinding will be omitted.
 * \param p_rng    The RNG context to pass to \p f_rng. This may be \c NULL
 *                 if \p f_rng is \c NULL or if \p f_rng doesn't need a context.
 *
 * \return         See te error code.
 *
 */
int te_rsa_private(const te_bn_t *N,
                   const te_bn_t *E,
                   const te_bn_t *D,
                   const te_bn_t *P,
                   const te_bn_t *Q,
                   const te_bn_t *DP,
                   const te_bn_t *DQ,
                   const te_bn_t *QP,
                   te_bn_t *pN,
                   te_bn_t *Vi,
                   te_bn_t *Vf,
                   const uint8_t *input,
                   uint8_t *output,
                   size_t size,
                   int (*f_rng)(void *, uint8_t *, size_t),
                   void *p_rng);

int te_rsa_complete_key_async(te_rsa_request_t *req);
int te_rsa_gen_key_async(te_rsa_request_t *req);
int te_rsa_public_async(te_rsa_request_t *req);
int te_rsa_private_async(te_rsa_request_t *req);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_RSA_H__ */
