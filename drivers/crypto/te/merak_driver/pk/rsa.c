//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include "pk_internal.h"
#include "te_rsa.h"

static int _rsa_deduce_crt(const te_drv_handle hdl,
                           const te_bn_t *P,
                           const te_bn_t *Q,
                           const te_bn_t *D,
                           te_bn_t *DP,
                           te_bn_t *DQ,
                           te_bn_t *QP)
{
    int ret    = 0;
    te_bn_t *K = NULL;

    ret = te_bn_alloc(hdl, 0, &K);
    PK_CHECK_RET_GO;

    /* DP = D mod P-1 */
    if (DP != NULL) {
        ret = te_bn_sub_s32(K, P, 1);
        PK_CHECK_RET_GO;
        ret = te_bn_mod_bn(DP, D, K);
        PK_CHECK_RET_GO;
    }

    /* DQ = D mod Q-1 */
    if (DQ != NULL) {
        ret = te_bn_sub_s32(K, Q, 1);
        PK_CHECK_RET_GO;
        ret = te_bn_mod_bn(DQ, D, K);
        PK_CHECK_RET_GO;
    }

    /* QP = Q^{-1} mod P */
    if (QP != NULL) {
        ret = te_bn_inv_mod(QP, Q, P);
        PK_CHECK_RET_GO;
    }

finish:
    te_bn_free(K);
    return ret;
}

/*
 * Given P, Q and the public exponent E, deduce D.
 * This is essentially a modular inversion.
 */
static int _rsa_deduce_private_exponent(const te_drv_handle hdl,
                                        const te_bn_t *P,
                                        const te_bn_t *Q,
                                        const te_bn_t *E,
                                        te_bn_t *D)
{
    int ret    = TE_SUCCESS;
    te_bn_t *K = NULL;
    te_bn_t *L = NULL;
    int result = 0;

    ret = te_bn_cmp_s32((te_bn_t *)P, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result > 0, TE_ERROR_BAD_INPUT_DATA);
    ret = te_bn_cmp_s32((te_bn_t *)Q, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result > 0, TE_ERROR_BAD_INPUT_DATA);
    ret = te_bn_cmp_s32((te_bn_t *)E, 0, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result != 0, TE_ERROR_BAD_INPUT_DATA);

    ret = te_bn_alloc(hdl, 0, &K);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &L);
    PK_CHECK_RET_GO;

    /* Temporarily put K := P-1 and L := Q-1 */
    ret = te_bn_sub_s32(K, P, 1);
    PK_CHECK_RET_GO;
    ret = te_bn_sub_s32(L, Q, 1);
    PK_CHECK_RET_GO;

    /* Temporarily put D := gcd(P-1, Q-1) */
    ret = te_bn_gcd(D, K, L);
    PK_CHECK_RET_GO;

    /* K := LCM(P-1, Q-1) */
    ret = te_bn_mul_bn(K, K, L);
    PK_CHECK_RET_GO;
    ret = te_bn_div_bn(K, NULL, K, D);
    PK_CHECK_RET_GO;

    /* Compute modular inverse of E in LCM(P-1, Q-1) */
    ret = te_bn_inv_mod(D, E, K);
    PK_CHECK_RET_GO;

finish:
    te_bn_free(K);
    te_bn_free(L);
    return ret;
}

/*
 * Compute RSA prime factors from public and private exponents
 *
 * Summary of algorithm:
 * Setting F := lcm(P-1,Q-1), the idea is as follows:
 *
 * (a) For any 1 <= X < N with gcd(X,N)=1, we have X^F = 1 modulo N, so X^(F/2)
 *     is a square root of 1 in Z/NZ. Since Z/NZ ~= Z/PZ x Z/QZ by CRT and the
 *     square roots of 1 in Z/PZ and Z/QZ are +1 and -1, this leaves the four
 *     possibilities X^(F/2) = (+-1, +-1). If it happens that X^(F/2) = (-1,+1)
 *     or (+1,-1), then gcd(X^(F/2) + 1, N) will be equal to one of the prime
 *     factors of N.
 *
 * (b) If we don't know F/2 but (F/2) * K for some odd (!) K, then the same
 *     construction still applies since (-)^K is the identity on the set of
 *     roots of 1 in Z/NZ.
 *
 * The public and private key primitives (-)^E and (-)^D are mutually inverse
 * bijections on Z/NZ if and only if (-)^(DE) is the identity on Z/NZ, i.e.
 * if and only if DE - 1 is a multiple of F, say DE - 1 = F * L.
 * Splitting L = 2^t * K with K odd, we have
 *
 *   DE - 1 = FL = (F/2) * (2^(t+1)) * K,
 *
 * so (F / 2) * K is among the numbers
 *
 *   (DE - 1) >> 1, (DE - 1) >> 2, ..., (DE - 1) >> ord
 *
 * where ord is the order of 2 in (DE - 1).
 * We can therefore iterate through these numbers apply the construction
 * of (a) and (b) above to attempt to factor N.
 *
 */
static int _rsa_deduce_primes(const te_drv_handle hdl,
                              const te_bn_t *N,
                              const te_bn_t *E,
                              const te_bn_t *D,
                              te_bn_t *P,
                              te_bn_t *Q)
{
    int ret          = TE_SUCCESS;
    uint16_t attempt = 0; /* Number of current attempt  */
    uint16_t iter  = 0; /* Number of squares computed in the current attempt */
    uint16_t order = 0; /* Order of 2 in DE - 1 */
    te_bn_t *T     = NULL; /* Holds largest odd divisor of DE - 1     */
    te_bn_t *K     = NULL; /* Temporary holding the current candidate */
    const uint8_t primes[] = {
        2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,
        47,  53,  59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107,
        109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181,
        191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251};
    const size_t num_primes = sizeof(primes) / sizeof(*primes);
    int result = 0, result2 = 0;
    uint32_t tmp = 0;

    ret = te_bn_cmp_s32((te_bn_t *)N, 0, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result > 0, TE_ERROR_BAD_INPUT_DATA);
    ret = te_bn_cmp_s32((te_bn_t *)D, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result > 0, TE_ERROR_BAD_INPUT_DATA);
    ret = te_bn_cmp_bn((te_bn_t *)D, (te_bn_t *)N, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_BAD_INPUT_DATA);
    ret = te_bn_cmp_s32((te_bn_t *)E, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result > 0, TE_ERROR_BAD_INPUT_DATA);
    ret = te_bn_cmp_bn((te_bn_t *)E, (te_bn_t *)N, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_BAD_INPUT_DATA);

    /*
     * Initializations and temporary changes
     */
    ret = te_bn_alloc(hdl, 0, &K);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &T);
    PK_CHECK_RET_GO;

    /* T := D*E - 1 */
    ret = te_bn_mul_bn(T, D, E);
    PK_CHECK_RET_GO;
    ret = te_bn_sub_s32(T, T, 1);
    PK_CHECK_RET_GO;

    ret = te_bn_0bits_before_lsb1(T);
    PK_CHECK_COND_GO(ret >= 0, ret);
    PK_CHECK_COND_GO(ret > 0, TE_ERROR_BAD_INPUT_DATA);

    order = (uint16_t)(ret);

    /* After this operation, T holds the largest odd divisor of DE - 1. */
    ret = te_bn_shift_r(T, T, (int)order);
    PK_CHECK_RET_GO;

    /*
     * Actual work
     */

    attempt = 0;
    /* Skip trying 2 if N == 1 mod 8 */
    ret = te_bn_mod_u32(&tmp, N, 8);
    PK_CHECK_RET_GO;
    if (tmp == 1) {
        attempt = 1;
    }
    for (; attempt < num_primes; ++attempt) {

        ret = te_bn_import_s32(K, primes[attempt]);
        PK_CHECK_RET_GO;

        /* Check if gcd(K,N) = 1 */
        ret = te_bn_gcd(P, K, N);
        PK_CHECK_RET_GO;
        ret = te_bn_cmp_s32(P, 1, &result);
        PK_CHECK_RET_GO;
        if (result != 0) {
            continue;
        }

        /* Go through K^T + 1, K^(2T) + 1, K^(4T) + 1, ...
         * and check whether they have nontrivial GCD with N. */
        ret = te_bn_exp_mod(K, K, T, N);
        PK_CHECK_RET_GO;
        for (iter = 1; iter <= order; ++iter) {
            /* If we reach 1 prematurely, there's no point
             * in continuing to square K */
            ret = te_bn_cmp_s32(K, 1, &result);
            PK_CHECK_RET_GO;
            if (result == 0) {
                break;
            }

            ret = te_bn_add_s32(K, K, 1);
            PK_CHECK_RET_GO;

            ret = te_bn_gcd(P, K, N);
            PK_CHECK_RET_GO;

            ret = te_bn_cmp_s32(P, 1, &result);
            PK_CHECK_RET_GO;
            ret = te_bn_cmp_bn(P, (te_bn_t *)N, &result2);
            PK_CHECK_RET_GO;
            if ((result == 1) && (result2 == -1)) {
                /*
                 * Have found a nontrivial divisor P of N.
                 * Set Q := N / P.
                 */

                ret = te_bn_div_bn(Q, NULL, N, P);
                PK_CHECK_RET_GO;
                ret = TE_SUCCESS;
                goto finish;
            }

            ret = te_bn_sub_s32(K, K, 1);
            PK_CHECK_RET_GO;

            ret = te_bn_square_mod(K, K, N);
            PK_CHECK_RET_GO;
        }

        /*
         * If we get here, then either we prematurely aborted the loop because
         * we reached 1, or K holds primes[attempt]^(DE - 1) mod N, which must
         * be 1 if D,E,N were consistent.
         * Check if that's the case and abort if not, to avoid very long,
         * yet eventually failing, computations if N,D,E were not sane.
         */
        ret = te_bn_cmp_s32(K, 1, &result);
        PK_CHECK_RET_GO;
        if (result != 0) {
            break;
        }
    }

    ret = TE_ERROR_BAD_INPUT_DATA;
finish:
    te_bn_free(K);
    te_bn_free(T);
    return ret;
}

static int _rsa_deduce_N(const te_drv_handle hdl,
                         te_bn_t *N,
                         const te_bn_t *P,
                         const te_bn_t *Q)
{
    int ret = TE_SUCCESS;

    (void)(hdl);
    ret = te_bn_mul_bn(N, P, Q);
    PK_CHECK_RET_GO;

finish:
    return ret;
}

#define _CHECK_BN_EMPTY(__name__)                                              \
    do {                                                                       \
        if (__name__) {                                                        \
            ret = te_bn_bitlen(__name__);                                      \
            PK_CHECK_COND_GO(ret >= 0, ret);                                   \
            if (ret == 0) {                                                    \
                empty_##__name__ = true;                                       \
            } else {                                                           \
                empty_##__name__ = false;                                      \
            }                                                                  \
        } else {                                                               \
            empty_##__name__ = true;                                           \
        }                                                                      \
    } while (0);

static int te_rsa_complete_key_core(const te_drv_handle hdl,
                                    te_bn_t *N,
                                    te_bn_t *E,
                                    te_bn_t *D,
                                    te_bn_t *P,
                                    te_bn_t *Q,
                                    te_bn_t *DP,
                                    te_bn_t *DQ,
                                    te_bn_t *QP)
{
    int ret = TE_SUCCESS;
    bool empty_N, empty_E, empty_D, empty_P, empty_Q, empty_DP, empty_DQ,
        empty_QP;

    te_pk_lock(hdl);

    _CHECK_BN_EMPTY(N);
    _CHECK_BN_EMPTY(E);
    _CHECK_BN_EMPTY(D);
    _CHECK_BN_EMPTY(P);
    _CHECK_BN_EMPTY(Q);
    _CHECK_BN_EMPTY(DP);
    _CHECK_BN_EMPTY(DQ);
    _CHECK_BN_EMPTY(QP);

    if ((P && (!empty_P)) && (Q && (!empty_Q)) && (N && (empty_N))) {
        /*
         * Step 1: Deduce N if P, Q are provided.
         */
        ret = _rsa_deduce_N(hdl, N, P, Q);
        PK_CHECK_RET_GO;
        empty_N = false;
    }

    /*
     * Step 2: Deduce and verify all remaining core parameters.
     */
    if ((N && (!empty_N)) && (E && (!empty_E)) && (D && (!empty_D)) &&
        (P && (empty_P)) && (Q && (empty_Q))) {
        ret = _rsa_deduce_primes(hdl, N, E, D, P, Q);
        PK_CHECK_RET_GO;
        empty_P = false;
        empty_Q = false;
    }

    if ((P && (!empty_P)) && (Q && (!empty_Q)) && (E && (!empty_E)) &&
        (D && (empty_D))) {
        ret = _rsa_deduce_private_exponent(hdl, P, Q, E, D);
        PK_CHECK_RET_GO;
        empty_D = false;
    }
    /*
     * Step 3: Deduce all additional parameters specific
     *         to our current RSA implementation.
     */

    if ((P && (!empty_P)) && (Q && (!empty_Q)) && (D && (!empty_D)) &&
        ((DP) && (empty_DP)) && ((DQ) && (empty_DQ)) && ((QP) && (empty_QP))) {
        ret = _rsa_deduce_crt(hdl, P, Q, D, DP, DQ, QP);
        PK_CHECK_RET_GO;
        empty_DP = false;
        empty_DQ = false;
        empty_QP = false;
    }

finish:
    te_pk_unlock(hdl);
    return ret;
}

int te_rsa_complete_key(te_bn_t *N,
                        te_bn_t *E,
                        te_bn_t *D,
                        te_bn_t *P,
                        te_bn_t *Q,
                        te_bn_t *DP,
                        te_bn_t *DQ,
                        te_bn_t *QP)
{
    int ret                 = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    if (N) {
        PK_CHECK_FUNC(te_bn_get_drv_handle(N, &hdl));
    } else if (P) {
        PK_CHECK_FUNC(te_bn_get_drv_handle(P, &hdl));
    } else {
        return TE_ERROR_BAD_PARAMS;
    }

    return te_rsa_complete_key_core(hdl, N, E, D, P, Q, DP, DQ, QP);
}
/*
 * Generate an RSA keypair
 *
 * This generation method follows the RSA key pair generation procedure of
 * FIPS 186-4 if 2^16 < exponent < 2^256 and nbits = 2048 or nbits = 3072.
 */
static int te_rsa_gen_key_core(const te_drv_handle hdl,
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
                               int32_t exponent)
{
    int ret           = TE_SUCCESS;
    te_bn_t *H        = NULL;
    te_bn_t *G        = NULL;
    te_bn_t *L        = NULL;
    int result        = 0;
    bool is_low_error = false;

    te_pk_lock(hdl);

    /*
     * If the modulus is 1024 bit long or shorter, then the security strength of
     * the RSA algorithm is less than or equal to 80 bits and therefore an error
     * rate of 2^-80 is sufficient.
     */
    if (nbits > 1024) {
        is_low_error = true;
    }

    ret = te_bn_alloc(hdl, 0, &H);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &G);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &L);
    PK_CHECK_RET_GO;

    /*
     * find primes P and Q with Q < P so that:
     * 1.  |P-Q| > 2^( nbits / 2 - 100 )
     * 2.  GCD( E, (P-1)*(Q-1) ) == 1
     * 3.  E^-1 mod LCM(P-1, Q-1) > 2^( nbits / 2 )
     */
    ret = te_bn_import_s32(E, exponent);
    PK_CHECK_RET_GO;

    do {
        ret = te_bn_gen_prime(P, is_low_error, false, nbits >> 1, f_rng, p_rng);
        PK_CHECK_RET_GO;

        ret = te_bn_gen_prime(Q, is_low_error, false, nbits >> 1, f_rng, p_rng);
        PK_CHECK_RET_GO;

        /* make sure the difference between p and q is not too small (FIPS 186-4
         * §B.3.3 step 5.4) */
        ret = te_bn_sub_bn(H, P, Q);
        PK_CHECK_RET_GO;
        ret = te_bn_bitlen(H);
        PK_CHECK_COND_GO(ret >= 0, ret);
        if (ret <= ((nbits >= 200) ? ((nbits >> 1) - 99) : 0)) {
            continue;
        }

        /* not required by any standards, but some users rely on the fact that P
         * > Q */
        ret = te_bn_get_sign(H, &result);
        PK_CHECK_RET_GO;
        if (result == -1) {
            ret = te_bn_swap(P, Q);
            PK_CHECK_RET_GO;
        }

        /* Temporarily replace P,Q by P-1, Q-1 */
        ret = te_bn_sub_s32(P, P, 1);
        PK_CHECK_RET_GO;
        ret = te_bn_sub_s32(Q, Q, 1);
        PK_CHECK_RET_GO;
        ret = te_bn_mul_bn(H, P, Q);
        PK_CHECK_RET_GO;

        /* check GCD( E, (P-1)*(Q-1) ) == 1 (FIPS 186-4 §B.3.1 criterion 2(a))
         */
        ret = te_bn_gcd(G, E, H);
        PK_CHECK_RET_GO;
        ret = te_bn_cmp_s32(G, 1, &result);
        PK_CHECK_RET_GO;
        if (result != 0) {
            continue;
        }

        /* compute smallest possible D = E^-1 mod LCM(P-1, Q-1) (FIPS 186-4
         * §B.3.1 criterion 3(b)) */
        ret = te_bn_gcd(G, P, Q);
        PK_CHECK_RET_GO;
        ret = te_bn_div_bn(L, NULL, H, G);
        PK_CHECK_RET_GO;
        ret = te_bn_inv_mod(D, E, L);
        PK_CHECK_RET_GO;

        ret = te_bn_bitlen(D);
        PK_CHECK_COND_GO(ret >= 0, ret);
        if (ret <= ((nbits + 1) / 2)) {
            // (FIPS 186-4 §B.3.1 criterion 3(a))
            continue;
        }

        break;
    } while (1);

    /* Restore P,Q */
    ret = te_bn_add_s32(P, P, 1);
    PK_CHECK_RET_GO;
    ret = te_bn_add_s32(Q, Q, 1);
    PK_CHECK_RET_GO;

    ret = te_bn_mul_bn(N, P, Q);
    PK_CHECK_RET_GO;

    /*
     * DP = D mod (P - 1)
     * DQ = D mod (Q - 1)
     * QP = Q^-1 mod P
     */
    ret = _rsa_deduce_crt(hdl, P, Q, D, DP, DQ, QP);
    PK_CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    te_bn_free(H);
    te_bn_free(G);
    te_bn_free(L);
    te_pk_unlock(hdl);
    return ret;
}

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
                   int32_t exponent)
{
    PK_CHECK_PARAM(hdl && N && E && D && P && Q && DP && DQ && QP && f_rng);
    if (!((nbits >= 128) && (exponent >= 3) && ((nbits % 2) == 0))) {
        return TE_ERROR_BAD_INPUT_DATA;
    }

    return te_rsa_gen_key_core(hdl, N, E, D, P, Q, DP, DQ, QP, f_rng, p_rng,
                               nbits, exponent);
}

static int te_rsa_public_core(const te_drv_handle hdl,
                              const te_bn_t *N,
                              const te_bn_t *E,
                              const uint8_t *input,
                              uint8_t *output,
                              size_t size)
{
    int ret    = TE_SUCCESS;
    te_bn_t *T = NULL;
    int result = 0;

    te_pk_lock(hdl);

    ret = te_bn_alloc(hdl, 0, &T);
    PK_CHECK_RET_GO;

    ret = te_bn_import(T, input, size, 1);
    PK_CHECK_RET_GO;

    ret = te_bn_cmp_bn(T, (te_bn_t *)N, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_BAD_INPUT_DATA);

    ret = te_bn_exp_mod(T, T, E, N);
    PK_CHECK_RET_GO;

    ret = te_bn_export(T, output, size);
    PK_CHECK_RET_GO;

finish:
    te_bn_free(T);
    te_pk_unlock(hdl);
    return ret;
}

int te_rsa_public(const te_bn_t *N,
                  const te_bn_t *E,
                  const uint8_t *input,
                  uint8_t *output,
                  size_t size)
{
    int ret                 = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(N && E && input && output && size);
    PK_CHECK_FUNC(te_bn_get_drv_handle((te_bn_t *)N, &hdl));

    return te_rsa_public_core(hdl, N, E, input, output, size);
}

#ifdef CFG_ACA_BLINDING_EN
/*
 * Generate or update blinding values, see section 10 of:
 *  KOCHER, Paul C. Timing attacks on implementations of Diffie-Hellman, RSA,
 *  DSS, and other systems. In : Advances in Cryptology-CRYPTO'96. Springer
 *  Berlin Heidelberg, 1996. p. 104-113.
 */
static int _rsa_prepare_blinding(const te_bn_t *N,
                                 const te_bn_t *E,
                                 te_bn_t *pN,
                                 te_bn_t *Vi,
                                 te_bn_t *Vf,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng)
{
    int ret        = TE_SUCCESS;
    int count      = 0;
    size_t n_bytes = 0;
    int result     = 0;

    n_bytes = te_bn_bitlen((te_bn_t *)N);
    PK_CHECK_COND_GO(ret >= 0, ret);
    if (n_bytes <= 0) {
        return TE_ERROR_BAD_INPUT_DATA;
    }
    n_bytes = (n_bytes + 7) / 8;

    /* check pN == N */
    ret = te_bn_cmp_bn((te_bn_t *)N, pN, &result);
    PK_CHECK_RET_GO;
    if (result == 0) {
        /* We already have blinding values, just update them by squaring */
        ret = te_bn_square_mod(Vi, Vi, N);
        PK_CHECK_RET_GO;
        ret = te_bn_square_mod(Vf, Vf, N);
        PK_CHECK_RET_GO;
        ret = TE_SUCCESS;
        goto finish;
    }

    /* else, update pN */
    ret = te_bn_copy(pN, N);
    PK_CHECK_RET_GO;

    /* Unblinding value: Vf = random number, invertible mod N */
    do {
        if (count++ > 10) {
            ret = TE_ERROR_GEN_RANDOM;
            goto finish;
        }

        ret = te_bn_import_random(Vf, n_bytes - 1, 1, f_rng, p_rng);
        PK_CHECK_RET_GO;

        ret = te_bn_gcd(Vi, Vf, N);
        PK_CHECK_RET_GO;

        ret = te_bn_cmp_s32(Vi, 1, &result);
        PK_CHECK_RET_GO;

    } while (result != 0);

    /* Blinding value: Vi =  Vf^(-e) mod N */
    ret = te_bn_inv_mod(Vi, Vf, N);
    PK_CHECK_RET_GO;
    ret = te_bn_exp_mod(Vi, Vi, E, N);
    PK_CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

/*
 * Exponent blinding supposed to prevent side-channel attacks using multiple
 * traces of measurements to recover the RSA key. The more collisions are there,
 * the more bits of the key can be recovered. See [3].
 *
 * Collecting n collisions with m bit long blinding value requires 2^(m-m/n)
 * observations on avarage.
 *
 * For example with 28 byte blinding to achieve 2 collisions the adversary has
 * to make 2^112 observations on avarage.
 *
 * (With the currently (as of 2017 April) known best algorithms breaking 2048
 * bit RSA requires approximately as much time as trying out 2^112 random keys.
 * Thus in this sense with 28 byte blinding the security is not reduced by
 * side-channel attacks like the one in [3])
 *
 * This countermeasure does not help if the key recovery is possible with a
 * single trace.
 */
#define RSA_EXPONENT_BLINDING 28

#endif /* CFG_ACA_BLINDING_EN */

/*
 * Do an RSA private key operation
 */
static int te_rsa_private_core(const te_drv_handle hdl,
                               const te_bn_t *N,
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
                               void *p_rng)
{
    int ret    = TE_SUCCESS;
    int result = 0;

    /* Temporary holding the result */
    te_bn_t *T = NULL;
#ifdef CFG_ACA_BLINDING_EN
    /* Temporaries holding P-1, Q-1 and the
     * exponent blinding factor, respectively. */
    te_bn_t *P1 = NULL, *Q1 = NULL, *R = NULL;
    /* Temporary holding the blinded exponent (if used). */
    te_bn_t *D_blind = NULL;
#endif
    /* Pointer to actual exponent to be used - either the unblinded
     * or the blinded one, depending on the presence of a PRNG. */
    te_bn_t *used_D = (te_bn_t *)D;
    /* Temporaries holding the initial input and the double
     * checked result; should be the same in the end. */
    te_bn_t *I = NULL, *C = NULL;

    (void)(DP);
    (void)(DQ);
    (void)(QP);
    te_pk_lock(hdl);

    ret = te_bn_alloc(hdl, 0, &I);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &T);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &C);
    PK_CHECK_RET_GO;

#ifdef CFG_ACA_BLINDING_EN
    if (f_rng) {
        ret = te_bn_alloc(hdl, 0, &P1);
        PK_CHECK_RET_GO;
        ret = te_bn_alloc(hdl, 0, &Q1);
        PK_CHECK_RET_GO;
        ret = te_bn_alloc(hdl, 0, &R);
        PK_CHECK_RET_GO;
        ret = te_bn_alloc(hdl, 0, &D_blind);
        PK_CHECK_RET_GO;
    }
#else
    (void)(P);
    (void)(Q);
    (void)(pN);
    (void)(Vi);
    (void)(Vf);
    (void)(f_rng);
    (void)(p_rng);
#endif

    ret = te_bn_import(T, input, size, 1);
    PK_CHECK_RET_GO;

    ret = te_bn_cmp_bn(T, (te_bn_t *)N, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_BAD_INPUT_DATA);

    ret = te_bn_copy(I, T);
    PK_CHECK_RET_GO;

#ifdef CFG_ACA_BLINDING_EN
    if (f_rng) {
        /*
         * Blinding
         * T = T * Vi mod N
         */
        ret = _rsa_prepare_blinding(N, E, pN, Vi, Vf, f_rng, p_rng);
        PK_CHECK_RET_GO;
        ret = te_bn_mul_mod(T, T, Vi, N);
        PK_CHECK_RET_GO;

        /*
         * Exponent blinding
         */
        ret = te_bn_sub_s32(P1, P, 1);
        PK_CHECK_RET_GO;
        ret = te_bn_sub_s32(Q1, Q, 1);
        PK_CHECK_RET_GO;

        /*
         * D_blind = ( P - 1 ) * ( Q - 1 ) * R + D
         */
        ret = te_bn_import_random(R, RSA_EXPONENT_BLINDING, 1, f_rng, p_rng);
        PK_CHECK_RET_GO;
        ret = te_bn_mul_bn(D_blind, P1, Q1);
        PK_CHECK_RET_GO;
        ret = te_bn_mul_bn(D_blind, D_blind, R);
        PK_CHECK_RET_GO;
        ret = te_bn_add_bn(D_blind, D_blind, D);
        PK_CHECK_RET_GO;

        used_D = D_blind;
    }
#endif

    ret = te_bn_exp_mod(T, T, used_D, N);
    PK_CHECK_RET_GO;

#ifdef CFG_ACA_BLINDING_EN
    if (f_rng) {
        /*
         * Unblind
         * T = T * Vf mod N
         */
        ret = te_bn_mul_mod(T, T, Vf, N);
        PK_CHECK_RET_GO;
    }
#endif

    /* Verify the result to prevent glitching attacks. */
    ret = te_bn_exp_mod(C, T, E, N);
    PK_CHECK_RET_GO;

    ret = te_bn_cmp_bn(C, I, &result);
    PK_CHECK_RET_GO;
    if (result != 0) {
        ret = TE_ERROR_VERIFY_SIG;
        goto finish;
    }

    ret = te_bn_export(T, output, size);
    PK_CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    te_bn_free(I);
    te_bn_free(T);
    te_bn_free(C);
#ifdef CFG_ACA_BLINDING_EN
    if (f_rng) {
        te_bn_free(P1);
        te_bn_free(Q1);
        te_bn_free(R);
        te_bn_free(D_blind);
    }
#endif
    te_pk_unlock(hdl);
    return ret;
}

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
                   void *p_rng)
{
    int ret                 = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(N && E && D && input && output && size);
#ifdef CFG_ACA_BLINDING_EN
    if (f_rng) {
        PK_CHECK_PARAM(P && Q && Vi && Vf && pN);
    }
#endif
    PK_CHECK_FUNC(te_bn_get_drv_handle((te_bn_t *)N, &hdl));

    return te_rsa_private_core(hdl, N, E, D, P, Q, DP, DQ, QP, pN, Vi, Vf,
                               input, output, size, f_rng, p_rng);
}

static int _te_rsa_complete_key_async_cb(te_rsa_request_t *req)
{
    return te_rsa_complete_key_core(PK_REQUEST_GET_HDL(req),
                                    req->complete_key_args.N,
                                    req->complete_key_args.E,
                                    req->complete_key_args.D,
                                    req->complete_key_args.P,
                                    req->complete_key_args.Q,
                                    req->complete_key_args.DP,
                                    req->complete_key_args.DQ,
                                    req->complete_key_args.QP);
}

int te_rsa_complete_key_async(te_rsa_request_t *req)
{
    int ret                 = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    if (req->complete_key_args.N) {
        PK_CHECK_FUNC(te_bn_get_drv_handle(req->complete_key_args.N, &hdl));
    } else if (req->complete_key_args.P) {
        PK_CHECK_FUNC(te_bn_get_drv_handle(req->complete_key_args.P, &hdl));
    } else {
        return TE_ERROR_BAD_PARAMS;
    }

    PK_REQUEST_INIT_DATA(req, _te_rsa_complete_key_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_rsa_gen_key_async_cb(te_rsa_request_t *req)
{
    PK_CHECK_PARAM(PK_REQUEST_GET_HDL(req) == req->gen_key_args.hdl);
    return te_rsa_gen_key_core(req->gen_key_args.hdl,
                               req->gen_key_args.N,
                               req->gen_key_args.E,
                               req->gen_key_args.D,
                               req->gen_key_args.P,
                               req->gen_key_args.Q,
                               req->gen_key_args.DP,
                               req->gen_key_args.DQ,
                               req->gen_key_args.QP,
                               req->gen_key_args.f_rng,
                               req->gen_key_args.p_rng,
                               req->gen_key_args.nbits,
                               req->gen_key_args.exponent);
}
int te_rsa_gen_key_async(te_rsa_request_t *req)
{
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->gen_key_args.hdl && req->gen_key_args.N &&
                   req->gen_key_args.E && req->gen_key_args.D &&
                   req->gen_key_args.P && req->gen_key_args.Q &&
                   req->gen_key_args.DP && req->gen_key_args.DQ &&
                   req->gen_key_args.QP && req->gen_key_args.f_rng);

    if (!((req->gen_key_args.nbits >= 128) &&
          (req->gen_key_args.exponent >= 3) &&
          ((req->gen_key_args.nbits % 2) == 0))) {
        return TE_ERROR_BAD_INPUT_DATA;
    }

    PK_REQUEST_INIT_DATA(req, _te_rsa_gen_key_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_rsa_public_async_cb(te_rsa_request_t *req)
{
    return te_rsa_public_core(PK_REQUEST_GET_HDL(req),
                              req->public_args.N,
                              req->public_args.E,
                              req->public_args.input,
                              req->public_args.output,
                              req->public_args.size);
}

int te_rsa_public_async(te_rsa_request_t *req)
{
    int ret                 = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->public_args.N && req->public_args.E &&
                   req->public_args.input && req->public_args.output &&
                   req->public_args.size);
    PK_CHECK_FUNC(te_bn_get_drv_handle((te_bn_t *)(req->public_args.N), &hdl));

    PK_REQUEST_INIT_DATA(req, _te_rsa_public_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_rsa_private_async_cb(te_rsa_request_t *req)
{
    return te_rsa_private_core(PK_REQUEST_GET_HDL(req),
                               req->private_args.N,
                               req->private_args.E,
                               req->private_args.D,
                               req->private_args.P,
                               req->private_args.Q,
                               req->private_args.DP,
                               req->private_args.DQ,
                               req->private_args.QP,
                               req->private_args.pN,
                               req->private_args.Vi,
                               req->private_args.Vf,
                               req->private_args.input,
                               req->private_args.output,
                               req->private_args.size,
                               req->private_args.f_rng,
                               req->private_args.p_rng);
}

int te_rsa_private_async(te_rsa_request_t *req)
{
    int ret                 = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);

    PK_CHECK_PARAM(req->private_args.N && req->private_args.E &&
                   req->private_args.D && req->private_args.input &&
                   req->private_args.output && req->private_args.size);
#ifdef CFG_ACA_BLINDING_EN
    if (req->private_args.f_rng) {
        PK_CHECK_PARAM(req->private_args.P && req->private_args.Q &&
                       req->private_args.Vi && req->private_args.Vf &&
                       req->private_args.pN);
    }
#endif
    PK_CHECK_FUNC(te_bn_get_drv_handle((te_bn_t *)(req->private_args.N), &hdl));

    PK_REQUEST_INIT_DATA(req, _te_rsa_private_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}
