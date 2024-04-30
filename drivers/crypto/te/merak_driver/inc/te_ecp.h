//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_ECP_H__
#define __TRUSTENGINE_ECP_H__

#include "te_bn.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * The APIs are mainly from mbedtls/ecp.h with the following changes:
 *
 * 1. remove tls_id/tls_id related operation.
 * 2. remove software reduction modulo.
 * 3. remove read/write binary with SEC1 2.3.4 encoding, only support read/write
 * binary with raw data (like bignumber).
 */

/**
 * \brief Domain-parameter identifiers: curve, subgroup, and generator.
 *
 * \note Only curves over prime fields are supported.
 * \note Currently TE_ECP_DP_CURVE25519 and TE_ECP_DP_CURVE448 are not
 * supported.
 *
 * \warning TE driver does not support validation of arbitrary domain
 * parameters. Therefore, only standardized domain parameters from trusted
 * sources should be used. See te_ecp_group_load().
 */
typedef enum te_ecp_group_id {
    TE_ECP_DP_NONE = 0,   /*!< Curve not defined. */
    TE_ECP_DP_SECP192R1,  /*!< Domain parameters for the 192-bit curve defined
                              by FIPS 186-4 and SEC1. */
    TE_ECP_DP_SECP224R1,  /*!< Domain parameters for the 224-bit curve defined
                              by FIPS 186-4 and SEC1. */
    TE_ECP_DP_SECP256R1,  /*!< Domain parameters for the 256-bit curve defined
                              by FIPS 186-4 and SEC1. */
    TE_ECP_DP_SECP384R1,  /*!< Domain parameters for the 384-bit curve defined
                              by FIPS 186-4 and SEC1. */
    TE_ECP_DP_SECP521R1,  /*!< Domain parameters for the 521-bit curve defined
                              by FIPS 186-4 and SEC1. */
    TE_ECP_DP_BP256R1,    /*!< Domain parameters for 256-bit Brainpool curve. */
    TE_ECP_DP_BP384R1,    /*!< Domain parameters for 384-bit Brainpool curve. */
    TE_ECP_DP_BP512R1,    /*!< Domain parameters for 512-bit Brainpool curve. */
    TE_ECP_DP_CURVE25519, /*!< Domain parameters for Curve25519. */
    TE_ECP_DP_SECP192K1,  /*!< Domain parameters for 192-bit "Koblitz" curve. */
    TE_ECP_DP_SECP224K1,  /*!< Domain parameters for 224-bit "Koblitz" curve. */
    TE_ECP_DP_SECP256K1,  /*!< Domain parameters for 256-bit "Koblitz" curve. */
    TE_ECP_DP_CURVE448,   /*!< Domain parameters for Curve448. */
    TE_ECP_DP_SM2P256V1,  /*!< Domain parameters for SM2 curve. */
} te_ecp_group_id_t;

/**
 * \brief           The ECP point structure, in Jacobian coordinates.
 *
 * \note            All functions expect and return points satisfying
 *                  the following condition: <code>Z == 0</code> or
 *                  <code>Z == 1</code>. Other values of \p Z are
 *                  used only by internal functions.
 *                  The point is zero, or "at infinity", if <code>Z == 0</code>.
 *                  Otherwise, \p X and \p Y are its standard (affine)
 *                  coordinates.
 */
typedef struct te_ecp_point {
    te_bn_t *X;
    te_bn_t *Y;
    te_bn_t *Z;
} te_ecp_point_t;

/**
 * \brief           The ECP group structure.
 *
 * We consider two types of curve equations:
 * <ul><li>Short Weierstrass: <code>y^2 = x^3 + A x + B mod P</code>
 * (SEC1 + RFC-4492)</li>
 * <li>Montgomery: <code>y^2 = x^3 + A x^2 + x mod P</code> (Curve25519,
 * Curve448)</li></ul>
 * In both cases, the generator (\p G) for a prime-order subgroup is fixed.
 *
 * For Short Weierstrass, this subgroup is the whole curve, and its
 * cardinality is denoted by \p N. Our code requires that \p N is an
 * odd prime as te_ecp_mul() requires an odd number, and
 * te_ecdsa_sign() requires that it is prime for blinding purposes.
 *
 * For Montgomery curves, we do not store \p A, but <code>(A + 2) / 4</code>,
 * which is the quantity used in the formulas.
 *
 * The reduction modulo \p P is done using a generic algorithm.
 *
 */
typedef struct te_ecp_group {
    te_ecp_group_id_t id; /*!< An internal group identifier. */
    te_bn_t *P;           /*!< The prime modulus of the base field. */
    te_bn_t *A;           /*!< For Short Weierstrass: \p A in the equation. For
                                  Montgomery curves: <code>(A + 2) / 4</code>. */
    te_bn_t *B;           /*!< For Short Weierstrass: \p B in the equation.
                                  For Montgomery curves: unused. */
    te_ecp_point_t G;     /*!< The generator of the subgroup used. */
    te_bn_t *N;           /*!< The order of \p G. */
    size_t pbits;         /*!< The number of bits in \p P.*/
    size_t nbits; /*!< For Short Weierstrass: The number of bits in \p P.
                       For Montgomery curves: the number of bits in the
                       private keys. */
} te_ecp_group_t;

/* ECP async request structure */
typedef struct _te_ecp_request_t {
    te_async_request_t base; /**< base async request */
    void *internal_data[4];  /**< internal data used by te aca driver */
    union {
        struct _ecp_check_pubkey_t {
            const te_ecp_group_t *grp;
            const te_ecp_point_t *pt;
        } check_pubkey_args;
        struct _ecp_check_privkey_t {
            const te_ecp_group_t *grp;
            const te_bn_t *d;
        } check_privkey_args;
        struct _ecp_gen_privkey_t {
            const te_ecp_group_t *grp;
            te_bn_t *d;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
        } gen_privkey_args;
        struct _ecp_gen_keypair_t {
            const te_ecp_group_t *grp;
            te_bn_t *d;
            te_ecp_point_t *Q;
            int (*f_rng)(void *, uint8_t *, size_t);
            void *p_rng;
        } gen_keypair_args;
    };
} te_ecp_request_t;

/**
 * \brief           This function initializes a point as zero.
 *
 * \param[in] hdl   The TE driver handler.
 * \param[in] pt    The point to initialize.
 * \return          See te error code.
 */
int te_ecp_point_init(const te_drv_handle hdl, te_ecp_point_t *pt);

/**
 * \brief           This function initializes an ECP group context
 *                  without loading any domain parameters.
 * \note            After this function is called, domain parameters
 *                  for various ECP groups can be loaded through the
 *                  te_ecp_group_load() function.
 *
 * \param[in] hdl   The TE driver handler.
 * \param[in] grp   The group to initialize.
 * \return          See te error code.
 */
int te_ecp_group_init(const te_drv_handle hdl, te_ecp_group_t *grp);

/**
 * \brief           This function frees the components of a point.
 *
 * \param[in] pt    The point to free. This may be \c NULL, in which
 *                  case this function returns immediately.
 */
void te_ecp_point_free(te_ecp_point_t *pt);

/**
 * \brief           This function frees the components of an ECP group.
 *
 * \param[in] grp   The group to free. This may be \c NULL, in which
 *                  case this function returns immediately. If it is not
 *                  \c NULL, it must point to an initialized ECP group.
 */
void te_ecp_group_free(te_ecp_group_t *grp);

/**
 * \brief           This function copies the contents of point \p Q into
 *                  point \p P.
 *
 * \param[out] P    The destination point. This must be initialized.
 * \param[in] Q     The source point. This must be initialized.
 *
 * \return          See te error code.
 */
int te_ecp_point_copy(te_ecp_point_t *P, const te_ecp_point_t *Q);

/**
 * \brief           This function copies the contents of group \p src into
 *                  group \p dst.
 *
 * \param[out] dst  The destination group. This must be initialized.
 * \param[in] src   The source group. This must be initialized.
 *
 * \return          See te error code.
 */
int te_ecp_group_copy(te_ecp_group_t *dst, const te_ecp_group_t *src);

/**
 * \brief           This function sets a point to the point at infinity.
 *
 * \param[in] pt    The point to set. This must be initialized.
 *
 * \return          See te error code.
 */
int te_ecp_set_zero(te_ecp_point_t *pt);

/**
 * \brief           This function checks if a point is the point at infinity.
 *
 * \param[in] pt    The point to test. This must be initialized.
 *
 * \return          \c 1 if the point is zero.
 * \return          \c 0 if the point is non-zero.
 * \return          A negative error code on failure.
 */
int te_ecp_is_zero(te_ecp_point_t *pt);

/**
 * \brief           This function compares two points.
 *
 * \note            This assumes that the points are normalized. Otherwise,
 *                  they may compare as "not equal" even if they are.
 *
 * \param[in] P     The first point to compare. This must be initialized.
 * \param[in] Q     The second point to compare. This must be initialized.
 *
 * \return          \c 0 if the points are equal.
 * \return          \c 1 if the points are NOT equal.
 * \return          A negative error code on failure.
 */
int te_ecp_point_cmp(const te_ecp_point_t *P, const te_ecp_point_t *Q);

/**
 * \brief           This function imports a point from unsigned binary data.
 *
 * \note            This function does not check that the point actually
 *                  belongs to the given group, see te_ecp_check_pubkey()
 *                  for that.
 *
 * \param[in] grp   The group to which the point should belong.
 *                  This must be initialized and have group parameters
 *                  set, for example through te_ecp_group_load().
 * \param[in] P     The destination context to import the point to.
 *                  This must be initialized.
 * \param[in] buf   The input buffer. This must be a readable buffer
 *                  of length \p size Bytes.
 * \param[in] size  The length of the input buffer \p buf in Bytes.
 *
 * \return          See te error code.
 */
int te_ecp_point_import(const te_ecp_group_t *grp,
                        te_ecp_point_t *P,
                        bool is_compressed,
                        const uint8_t *buf,
                        size_t size);
/**
 * \brief           This function exports a point into unsigned binary data.
 *
 * \param[in] grp   The group to which the point should belong.
 *                  This must be initialized and have group parameters
 *                  set, for example through te_ecp_group_load().
 * \param[in] P     The point to export. This must be initialized.
 * \param[in] is_compressed
 *                  Whether the point is compressed format.
 * \param[out] buf  The output buffer. This must be a writable buffer
 *                  of length \p *size Bytes.
 * \param[inout] size
 *                  The size pointer of output buffer. Also updated to the real
 *                  data size if SUCCESS.
 * \return          See te error code.
 */
int te_ecp_point_export(const te_ecp_group_t *grp,
                        te_ecp_point_t *P,
                        bool is_compressed,
                        uint8_t *buf,
                        size_t *size);
/**
 * \brief           This function sets up an ECP group context
 *                  from a standardized set of domain parameters.
 *
 * \note            The index should be a value of the NamedCurve enum,
 *                  as defined in <em>RFC-4492: Elliptic Curve Cryptography
 *                  (ECC) Cipher Suites for Transport Layer Security (TLS)</em>,
 *                  usually in the form of an \c TE_ECP_DP_XXX macro.
 *
 * \param[in] grp   The group context to setup. This must be initialized.
 * \param[in] id    The identifier of the domain parameter set to load.
 *
 * \return          See te error code.
 */
int te_ecp_group_load(te_ecp_group_t *grp, te_ecp_group_id_t id);

/**
 * \brief           This function performs a scalar multiplication of a point
 *                  by an integer: \p R = \p m * \p P.
 *
 * \note            To prevent timing attacks, this function
 *                  executes the exact same sequence of base-field
 *                  operations for any valid \p m. It avoids any if-branch or
 *                  array index depending on the value of \p m.
 *
 * \note            If \p f_rng is not NULL, it is used to randomize
 *                  intermediate results to prevent potential timing attacks
 *                  targeting these results. We recommend always providing
 *                  a non-NULL \p f_rng. The overhead is negligible.
 *
 * \param[in] grp   The ECP group to use.
 *                  This must be initialized and have group parameters
 *                  set, for example through te_ecp_group_load().
 * \param[out] R    The point in which to store the result of the calculation.
 *                  This must be initialized.
 * \param[in] m     The integer by which to multiply. This must be initialized.
 * \param[in] P     The point to multiply. This must be initialized.
 * \param[in] f_rng The RNG function. This may be \c NULL if randomization
 *                  of intermediate results isn't desired (discouraged).
 * \param[in] p_rng The RNG context to be passed to \p p_rng.
 *
 * \return          See te error code.
 */
int te_ecp_mul(const te_ecp_group_t *grp,
               te_ecp_point_t *R,
               const te_bn_t *m,
               const te_ecp_point_t *P,
               int (*f_rng)(void *, uint8_t *, size_t),
               void *p_rng);

/**
 * \brief           This function performs multiplication and addition of two
 *                  points by integers: \p R = \p m * \p P + \p n * \p Q
 *
 * \note            In contrast to te_ecp_mul(), this function does not
 *                  guarantee a constant execution flow and timing.
 *
 * \param[in] grp   The ECP group to use.
 *                  This must be initialized and have group parameters
 *                  set, for example through te_ecp_group_load().
 * \param[out] R    The point in which to store the result of the calculation.
 *                  This must be initialized.
 * \param[in] m     The integer by which to multiply \p P.
 *                  This must be initialized.
 * \param[in] P     The point to multiply by \p m. This must be initialized.
 * \param[in] n     The integer by which to multiply \p Q.
 *                  This must be initialized.
 * \param[in] Q     The point to be multiplied by \p n.
 *                  This must be initialized.
 * \return          See te error code.
 */
int te_ecp_muladd(const te_ecp_group_t *grp,
                  te_ecp_point_t *R,
                  const te_bn_t *m,
                  const te_ecp_point_t *P,
                  const te_bn_t *n,
                  const te_ecp_point_t *Q);

/**
 * \brief           This function checks that a point is a valid public key
 *                  on this curve.
 *
 *                  It only checks that the point is non-zero, has
 *                  valid coordinates and lies on the curve. It does not verify
 *                  that it is indeed a multiple of \p G. This additional
 *                  check is computationally more expensive, is not required
 *                  by standards, and should not be necessary if the group
 *                  used has a small cofactor. In particular, it is useless for
 *                  the NIST groups which all have a cofactor of 1.
 *
 * \param[in] grp   The ECP group the point should belong to.
 *                  This must be initialized and have group parameters
 *                  set, for example through te_ecp_group_load().
 * \param[in] pt    The point to check. This must be initialized.
 *
 * \return          \c 0 if the point is a valid public key.
 * \return          \c TE_ERROR_INVAL_KEY if the point is a valid public key.
 * \return          Another negative error code on other kinds of failure.
 */
int te_ecp_check_pubkey(const te_ecp_group_t *grp, const te_ecp_point_t *pt);

/**
 * \brief           This function checks that an \p te_bn_t is a
 *                  valid private key for this curve.
 *
 * \param[in] grp   The ECP group the private key should belong to.
 *                  This must be initialized and have group parameters
 *                  set, for example through te_ecp_group_load().
 * \param[in] d     The integer to check. This must be initialized.
 *
 * \return          \c 0 if the point is a valid public key.
 * \return          \c TE_ERROR_INVAL_KEY if the point is a valid public key.
 * \return          Another negative error code on other kinds of failure.
 */
int te_ecp_check_privkey(const te_ecp_group_t *grp, const te_bn_t *d);

/**
 * \brief           This function generates a private key.
 *
 * \param[in] grp   The ECP group to generate a private key for.
 *                  This must be initialized and have group parameters
 *                  set, for example through te_ecp_group_load().
 * \param[out] d    The destination MPI (secret part). This must be initialized.
 * \param[in] f_rng The RNG function. This must not be \c NULL.
 * \param[in] p_rng The RNG parameter to be passed to \p f_rng. This may be
 *                  \c NULL if \p f_rng doesn't need a context argument.
 *
 * \return          See te error code.
 */
int te_ecp_gen_privkey(const te_ecp_group_t *grp,
                       te_bn_t *d,
                       int (*f_rng)(void *, uint8_t *, size_t),
                       void *p_rng);

/**
 * \brief           This function generates a keypair with a configurable base
 *                  point.
 *
 * \param[in] grp   The ECP group to generate a key pair for.
 *                  This must be initialized and have group parameters
 *                  set, for example through te_ecp_group_load().
 * \param[in] G     The base point to use. This must be initialized
 *                  and belong to \p grp. It replaces the default base
 *                  point \c grp->G used by te_ecp_gen_keypair().
 * \param[out] d    The destination MPI (secret part).
 *                  This must be initialized.
 * \param[out] Q    The destination point (public part).
 *                  This must be initialized.
 * \param[in] f_rng The RNG function. This must not be \c NULL.
 * \param[in] p_rng The RNG context to be passed to \p f_rng. This may
 *                  be \c NULL if \p f_rng doesn't need a context argument.
 *
 * \return          See te error code.
 */
int te_ecp_gen_keypair_base(const te_ecp_group_t *grp,
                            const te_ecp_point_t *G,
                            te_bn_t *d,
                            te_ecp_point_t *Q,
                            int (*f_rng)(void *, uint8_t *, size_t),
                            void *p_rng);
/**
 * \brief           This function generates an ECP keypair.
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
int te_ecp_gen_keypair(const te_ecp_group_t *grp,
                       te_bn_t *d,
                       te_ecp_point_t *Q,
                       int (*f_rng)(void *, uint8_t *, size_t),
                       void *p_rng);

int te_ecp_check_pubkey_async(te_ecp_request_t *req);
int te_ecp_check_privkey_async(te_ecp_request_t *req);
int te_ecp_gen_privkey_async(te_ecp_request_t *req);
int te_ecp_gen_keypair_async(te_ecp_request_t *req);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_ECP_H__ */
