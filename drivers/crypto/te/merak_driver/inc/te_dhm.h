//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DHM_H__
#define __TRUSTENGINE_DHM_H__

#include "te_bn.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Don't use context here, because context will introduce more APIs
 */

/* DHM async request structure */
typedef struct _te_dhm_request_t {
    te_async_request_t base; /**< base async request */
    void *internal_data[4];  /**< internal data used by te aca driver */
    union {
        struct _dhm_make_public_t { /**< args for dhm_make_public */
            const te_bn_t *P;
            const te_bn_t *G;
            size_t x_size;
            te_bn_t *X;
            te_bn_t *GX;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
        } make_public_args;
        struct _dhm_compute_shared_t { /**< args for dhm_compute_shared */
            const te_bn_t *P;
            const te_bn_t *G;
            const te_bn_t *X;
            const te_bn_t *GY;
            te_bn_t *pX;
            te_bn_t *Vi;
            te_bn_t *Vf;
            te_bn_t *K;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
        } compute_shared_args;
    };
} te_dhm_request_t;

/**
 * \brief          This function creates a DHM key pair. If the private key
 *                 \p X is already exists, then this function will derive
 *                 the public key from private key.
 *
 * \param P        The prime modulus
 * \param G        The generator
 * \param x_size   The private key size in Bytes.
 * \param X        Our secret key, if X !=0 will not generate X.
 * \param GX       Our public key = G^X mod P
 *
 * \param f_rng    The RNG function. This must not be \c NULL.
 * \param p_rng    The RNG context to be passed to \p f_rng. This may be \c NULL
 *                 if \p f_rng doesn't need a context argument.
 * \return          See te error code.
 */
int te_dhm_make_public(const te_bn_t *P,
                       const te_bn_t *G,
                       size_t x_size,
                       te_bn_t *X,
                       te_bn_t *GX,
                       int (*f_rng)(void *, uint8_t *, size_t),
                       void *p_rng);

/**
 * \brief          This function derives and exports the shared secret
 *                 \c (G^Y)^X mod \c P.
 *
 * \note           If \p f_rng is not \c NULL, it is used to blind the input as
 *                 a countermeasure against timing attacks. Blinding is used
 *                 only if our private key \c X is re-used, and not used
 *                 otherwise. We recommend always passing a non-NULL
 *                 \p f_rng argument.
 *
 * \param P        The prime modulus
 * \param G        The generator
 * \param X        Our secret key
 * \param GY       Ohters' public key
 * \param pX       The blinding pointer, can be NULL if f_rng is also NULL.
 * \param Vi       The blinding pointer, can be NULL if f_rng is also NULL.
 * \param Vf       The blinding pointer, can be NULL if f_rng is also NULL.

 * \param f_rng     The RNG function, for blinding purposes. This may
 *                  b \c NULL if blinding isn't needed. if not NULL, pX, Vi, Vf
 *                  must also not be NULL.
 * \param p_rng     The RNG context. This may be \c NULL if \p f_rng
 *                  doesn't need a context argument.
 *
 * \return          See te error code.
 */
int te_dhm_compute_shared(const te_bn_t *P,
                          const te_bn_t *G,
                          const te_bn_t *X,
                          const te_bn_t *GY,
                          te_bn_t *pX,
                          te_bn_t *Vi,
                          te_bn_t *Vf,
                          te_bn_t *K,
                          int (*f_rng)(void *, uint8_t *, size_t),
                          void *p_rng);

int te_dhm_make_public_async(te_dhm_request_t *req);
int te_dhm_compute_shared_async(te_dhm_request_t *req);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_DHM_H__ */
