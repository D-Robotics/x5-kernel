//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include "pk_internal.h"
#include "te_ecp.h"

int te_ecp_point_init(const te_drv_handle hdl, te_ecp_point_t *pt)
{
    int ret = TE_SUCCESS;
    PK_CHECK_PARAM(pt && hdl);

    aca_zeroize(pt, sizeof(te_ecp_point_t));

    ret = te_bn_alloc(hdl, 0, &(pt->X));
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &(pt->Y));
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &(pt->Z));
    PK_CHECK_RET_GO;

    ret = te_bn_import_s32(pt->X, 1);
    PK_CHECK_RET_GO;
    ret = te_bn_import_s32(pt->Y, 1);
    PK_CHECK_RET_GO;
    ret = te_bn_import_s32(pt->Z, 0);
    PK_CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    if (ret != TE_SUCCESS) {
        if (pt->X) {
            te_bn_free(pt->X);
            pt->X = NULL;
        }
        if (pt->Y) {
            te_bn_free(pt->Y);
            pt->Y = NULL;
        }
        if (pt->Z) {
            te_bn_free(pt->Z);
            pt->Z = NULL;
        }
    }
    return ret;
}

void te_ecp_point_free(te_ecp_point_t *pt)
{
    if (!pt) {
        return;
    }
    if (pt->X) {
        te_bn_free(pt->X);
        pt->X = NULL;
    }
    if (pt->Y) {
        te_bn_free(pt->Y);
        pt->Y = NULL;
    }
    if (pt->Z) {
        te_bn_free(pt->Z);
        pt->Z = NULL;
    }
    aca_zeroize(pt, sizeof(te_ecp_point_t));
    return;
}

int te_ecp_point_copy(te_ecp_point_t *P, const te_ecp_point_t *Q)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(P && Q);
    PK_CHECK_FUNC(te_bn_get_drv_handle(Q->X, &hdl));

    ret = te_bn_copy(P->X, Q->X);
    PK_CHECK_RET_GO;
    ret = te_bn_copy(P->Y, Q->Y);
    PK_CHECK_RET_GO;
    ret = te_bn_copy(P->Z, Q->Z);
    PK_CHECK_RET_GO;

finish:
    return ret;
}

int te_ecp_set_zero(te_ecp_point_t *pt)
{
    int ret = TE_SUCCESS;
    PK_CHECK_PARAM(pt);

    ret = te_bn_import_s32(pt->X, 1);
    PK_CHECK_RET_GO;
    ret = te_bn_import_s32(pt->Y, 1);
    PK_CHECK_RET_GO;
    ret = te_bn_import_s32(pt->Z, 0);
    PK_CHECK_RET_GO;

finish:
    return ret;
}

int te_ecp_is_zero(te_ecp_point_t *pt)
{
    int ret    = TE_SUCCESS;
    int result = 0;
    PK_CHECK_PARAM(pt);

    ret = te_bn_cmp_s32(pt->Z, 0, &result);
    PK_CHECK_RET_GO;

    return (result == 0) ? (1) : (0);
finish:
    return ret;
}

/*
 * Compare two points lazily
 */
int te_ecp_point_cmp(const te_ecp_point_t *P, const te_ecp_point_t *Q)
{
    int ret     = TE_SUCCESS;
    int result1 = 0, result2 = 0, result3 = 0;

    PK_CHECK_PARAM(P && Q);

    ret = te_bn_cmp_bn(P->X, Q->X, &result1);
    PK_CHECK_RET_GO;
    ret = te_bn_cmp_bn(P->Y, Q->Y, &result2);
    PK_CHECK_RET_GO;
    ret = te_bn_cmp_bn(P->Z, Q->Z, &result3);
    PK_CHECK_RET_GO;

    return (((result1 == 0) && (result2 == 0) && (result3 == 0)) ? (0) : (1));
finish:
    return ret;
}

/*
 * Export a point into unsigned binary data (SEC1 2.3.3)
 */
int te_ecp_point_export(const te_ecp_group_t *grp,
                        te_ecp_point_t *P,
                        bool is_compressed,
                        uint8_t *buf,
                        size_t *size)
{
    int ret        = TE_SUCCESS;
    size_t plen    = 0;
    size_t out_len = 0;
    int result     = 0;
    int32_t Y_lsb  = 0;

    PK_CHECK_PARAM(grp && P && buf && size);

    /*
     * Common case: P == 0
     */
    ret = te_bn_cmp_s32(P->Z, 0, &result);
    PK_CHECK_RET_GO;
    if (0 == result) {
        if (*size < 1) {
            *size = 1;
            ret   = TE_ERROR_SHORT_BUFFER;
            goto finish;
        }
        buf[0] = 0x00;
        *size  = 1;
        ret    = TE_SUCCESS;
        goto finish;
    }

    plen = (grp->pbits + 7) / 8;

    if (!is_compressed) {
        out_len = 2 * plen + 1;

        if (*size < out_len) {
            *size = out_len;
            ret   = TE_ERROR_SHORT_BUFFER;
            goto finish;
        }

        buf[0] = 0x04;
        ret    = te_bn_export(P->X, buf + 1, plen);
        PK_CHECK_RET_GO;

        ret = te_bn_export(P->Y, buf + 1 + plen, plen);
        PK_CHECK_RET_GO;
        *size = out_len;
    } else {
        out_len = plen + 1;
        if (*size < out_len) {
            *size = out_len;
            ret   = TE_ERROR_SHORT_BUFFER;
            goto finish;
        }

        Y_lsb = te_bn_get_bit(P->Y, 0);
        PK_CHECK_COND_GO(Y_lsb >= 0, Y_lsb);

        buf[0] = 0x02 + Y_lsb;

        ret = te_bn_export(P->X, buf + 1, plen);
        PK_CHECK_RET_GO;
        *size = out_len;
    }
    ret = TE_SUCCESS;
finish:
    return ret;
}

/*
 * Import a point from unsigned binary data (SEC1 2.3.4)
 */
int te_ecp_point_import(const te_ecp_group_t *grp,
                        te_ecp_point_t *P,
                        bool is_compressed,
                        const uint8_t *buf,
                        size_t size)
{
    int ret     = TE_SUCCESS;
    size_t plen = 0;

    (void)(is_compressed);
    PK_CHECK_PARAM(grp && P && buf && (size >= 1));

    if (buf[0] == 0x00) {
        PK_CHECK_PARAM(size == 1);
        return te_ecp_set_zero(P);
    }

    plen = (grp->pbits + 7) / 8;

    if (buf[0] != 0x04) {
        return TE_ERROR_FEATURE_UNAVAIL;
    }
    if (size != 2 * plen + 1) {
        return (TE_ERROR_BAD_INPUT_DATA);
    }

    ret = te_bn_import(P->X, buf + 1, plen, 1);
    PK_CHECK_RET_GO;
    ret = te_bn_import(P->Y, buf + 1 + plen, plen, 1);
    PK_CHECK_RET_GO;
    ret = te_bn_import_s32(P->Z, 1);
    PK_CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

#ifdef CFG_ACA_BLINDING_EN

/*
 * Randomize jacobian coordinates:
 * (X, Y, Z) -> (l^2 X, l^3 Y, l Z) for random l
 * This is sort of the reverse operation of ecp_normalize_jac().
 *
 * This countermeasure was first suggested in [2].
 */
static int _ecp_randomize_jac(const te_drv_handle hdl,
                              const te_ecp_group_t *grp,
                              te_ecp_point_t *pt,
                              int (*f_rng)(void *, unsigned char *, size_t),
                              void *p_rng)
{
    int ret;
    size_t p_size = 0;
    int count     = 0;
    te_bn_t *l = NULL, *ll = NULL;
    int result1 = 0, result2 = 0;

    p_size = (grp->pbits + 7) / 8;

    ret = te_bn_alloc(hdl, 0, &l);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &ll);
    PK_CHECK_RET_GO;

    /* Generate l such that 1 < l < p */
    do {
        ret = te_bn_import_random(l, p_size, 1, f_rng, p_rng);
        PK_CHECK_RET_GO;

        do {
            ret = te_bn_cmp_bn(l, grp->P, &result1);
            PK_CHECK_RET_GO;
            if (result1 >= 0) {
                ret = te_bn_shift_r(l, l, 1);
                PK_CHECK_RET_GO;
            } else {
                break;
            }
        } while (true);

        if (count++ > 10) {
            return TE_ERROR_GEN_RANDOM;
        }

        ret = te_bn_cmp_s32(l, 1, &result2);
        PK_CHECK_RET_GO;
    } while (result2 <= 0);

    /* Z = l * Z */
    ret = te_bn_mul_mod(pt->Z, pt->Z, l, grp->P);
    PK_CHECK_RET_GO;

    /* X = l^2 * X */
    ret = te_bn_mul_mod(ll, l, l, grp->P);
    PK_CHECK_RET_GO;
    ret = te_bn_mul_mod(pt->X, pt->X, ll, grp->P);
    PK_CHECK_RET_GO;

    /* Y = l^3 * Y */
    ret = te_bn_mul_mod(ll, ll, l, grp->P);
    PK_CHECK_RET_GO;
    ret = te_bn_mul_mod(pt->Y, pt->Y, ll, grp->P);
    PK_CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    te_bn_free(l);
    te_bn_free(ll);
    return ret;
}
#endif /* CFG_ACA_BLINDING_EN */

int te_ecp_mul_core(const te_drv_handle hdl,
                    const te_ecp_group_t *grp,
                    te_ecp_point_t *R,
                    const te_bn_t *m,
                    const te_ecp_point_t *P,
                    int (*f_rng)(void *, uint8_t *, size_t),
                    void *p_rng,
                    bool is_lock)
{
    int ret              = TE_SUCCESS;
    te_ecp_point_t tmp_P = {0};
    int result           = 0;

    if (is_lock) {
        te_pk_lock(hdl);
    }

    ret = te_ecp_point_init(hdl, &tmp_P);
    PK_CHECK_RET_GO;

    ret = te_ecp_point_copy(&tmp_P, P);
    PK_CHECK_RET_GO;

    /* Check P->Z, MUST be 1 */
    ret = te_bn_cmp_s32(tmp_P.Z, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result == 0, TE_ERROR_BAD_INPUT_DATA);

#ifdef CFG_ACA_BLINDING_EN
    /* randomize point P */
    if (f_rng) {
        ret = _ecp_randomize_jac(hdl, grp, &tmp_P, f_rng, p_rng);
        PK_CHECK_RET_GO;
    }
#else
    (void)(f_rng);
    (void)(p_rng);
#endif

    ret = te_bn_ecp_mul((const te_bn_t *)(grp->P),
                        (const te_bn_t *)(grp->A),
                        (const te_bn_t *)(tmp_P.X),
                        (const te_bn_t *)(tmp_P.Y),
                        (const te_bn_t *)(tmp_P.Z),
                        m,
                        tmp_P.X,
                        tmp_P.Y,
                        tmp_P.Z);
    PK_CHECK_RET_GO;

    /* convert jacobian to affine */
    ret = te_bn_ecp_jacobian_to_affine((const te_bn_t *)(grp->P),
                                       (const te_bn_t *)(tmp_P.X),
                                       (const te_bn_t *)(tmp_P.Y),
                                       (const te_bn_t *)(tmp_P.Z),
                                       R->X,
                                       R->Y,
                                       R->Z);
    PK_CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    te_ecp_point_free(&tmp_P);
    if (is_lock) {
        te_pk_unlock(hdl);
    }
    return ret;
}

int te_ecp_mul(const te_ecp_group_t *grp,
               te_ecp_point_t *R,
               const te_bn_t *m,
               const te_ecp_point_t *P,
               int (*f_rng)(void *, uint8_t *, size_t),
               void *p_rng)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(grp && R && m && P);
    PK_CHECK_FUNC(te_bn_get_drv_handle(grp->P, &hdl));

    return te_ecp_mul_core(hdl, grp, R, m, P, f_rng, p_rng, true);
}
/*
 * R = m * P with shortcuts for m == 1 and m == -1
 * NOT constant-time - ONLY for short Weierstrass!
 */
static int _te_ecp_mul_shortcuts(const te_drv_handle hdl,
                                 const te_ecp_group_t *grp,
                                 te_ecp_point_t *R,
                                 const te_bn_t *m,
                                 const te_ecp_point_t *P)
{
    int ret    = TE_SUCCESS;
    int result = 0;

    ret = te_bn_cmp_s32((te_bn_t *)m, 1, &result);
    PK_CHECK_RET_GO;
    if (0 == result) {
        ret = te_ecp_point_copy(R, P);
        PK_CHECK_RET_GO;

        ret = TE_SUCCESS;
        goto finish;
    }

    ret = te_bn_cmp_s32((te_bn_t *)m, -1, &result);
    PK_CHECK_RET_GO;
    if (0 == result) {
        ret = te_ecp_point_copy(R, P);
        PK_CHECK_RET_GO;

        ret = te_bn_cmp_s32(R->Y, 0, &result);
        PK_CHECK_RET_GO;
        if (0 != result) {
            ret = te_bn_sub_bn(R->Y, grp->P, R->Y);
            PK_CHECK_RET_GO;
        }

        ret = TE_SUCCESS;
        goto finish;
    }

    ret = te_ecp_mul_core(hdl, grp, R, m, P, NULL, NULL, false);
    PK_CHECK_RET_GO;

finish:
    return ret;
}

int te_ecp_muladd_core(const te_drv_handle hdl,
                       const te_ecp_group_t *grp,
                       te_ecp_point_t *R,
                       const te_bn_t *m,
                       const te_ecp_point_t *P,
                       const te_bn_t *n,
                       const te_ecp_point_t *Q,
                       bool with_lock)
{
    int ret           = TE_SUCCESS;
    te_ecp_point_t mP = {0};

    if (with_lock) {
        te_pk_lock(hdl);
    }

    ret = te_ecp_point_init(hdl, &mP);
    PK_CHECK_RET_GO;

    ret = _te_ecp_mul_shortcuts(hdl, grp, &mP, m, P);
    PK_CHECK_RET_GO;
    ret = _te_ecp_mul_shortcuts(hdl, grp, R, n, Q);
    PK_CHECK_RET_GO;

    ret = te_bn_ecp_add((const te_bn_t *)(grp->P),
                        (const te_bn_t *)(mP.X),
                        (const te_bn_t *)(mP.Y),
                        (const te_bn_t *)(mP.Z),
                        (const te_bn_t *)(R->X),
                        (const te_bn_t *)(R->Y),
                        (const te_bn_t *)(R->Z),
                        R->X,
                        R->Y,
                        R->Z);
    PK_CHECK_RET_GO;

    ret = te_bn_ecp_jacobian_to_affine((const te_bn_t *)(grp->P),
                                       (const te_bn_t *)(R->X),
                                       (const te_bn_t *)(R->Y),
                                       (const te_bn_t *)(R->Z),
                                       R->X,
                                       R->Y,
                                       R->Z);
    PK_CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    te_ecp_point_free(&mP);
    if (with_lock) {
        te_pk_unlock(hdl);
    }
    return ret;
}

int te_ecp_muladd(const te_ecp_group_t *grp,
                  te_ecp_point_t *R,
                  const te_bn_t *m,
                  const te_ecp_point_t *P,
                  const te_bn_t *n,
                  const te_ecp_point_t *Q)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(grp && R && m && P && n && Q);
    PK_CHECK_FUNC(te_bn_get_drv_handle(grp->P, &hdl));

    return te_ecp_muladd_core(hdl, grp, R, m, P, n, Q, true);
}

/*
 * Check that an affine point is valid as a public key,
 * short weierstrass curves (SEC1 3.2.3.1)
 */
int te_ecp_check_pubkey_core(const te_drv_handle hdl,
                             const te_ecp_group_t *grp,
                             const te_ecp_point_t *pt,
                             bool with_lock)
{
    int ret     = TE_SUCCESS;
    te_bn_t *YY = NULL, *RHS = NULL;
    int result = 0;

    if (with_lock) {
        te_pk_lock(hdl);
    }

    /* Must use affine coordinates */
    ret = te_bn_cmp_s32(pt->Z, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(0 == result, TE_ERROR_INVAL_KEY);

    /* pt coordinates must be normalized for our checks */
    ret = te_bn_cmp_s32(pt->X, 0, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result >= 0, TE_ERROR_INVAL_KEY);

    ret = te_bn_cmp_s32(pt->Y, 0, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result >= 0, TE_ERROR_INVAL_KEY);

    ret = te_bn_cmp_bn(pt->X, grp->P, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_INVAL_KEY);

    ret = te_bn_cmp_bn(pt->Y, grp->P, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_INVAL_KEY);

    /*
     * YY = Y^2
     * RHS = X (X^2 + A) + B = X^3 + A X + B
     */

    ret = te_bn_alloc(hdl, 0, &YY);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &RHS);
    PK_CHECK_RET_GO;

    /* YY = Y^2 */
    ret = te_bn_mul_mod(YY, pt->Y, pt->Y, grp->P);
    PK_CHECK_RET_GO;

    ret = te_bn_mul_mod(RHS, pt->X, pt->X, grp->P);
    PK_CHECK_RET_GO;

    ret = te_bn_add_mod(RHS, RHS, grp->A, grp->P);
    PK_CHECK_RET_GO;

    ret = te_bn_mul_mod(RHS, RHS, pt->X, grp->P);
    PK_CHECK_RET_GO;

    ret = te_bn_add_mod(RHS, RHS, grp->B, grp->P);
    PK_CHECK_RET_GO;

    ret = te_bn_cmp_bn(YY, RHS, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(0 == result, TE_ERROR_INVAL_KEY);

    ret = TE_SUCCESS;
finish:
    te_bn_free(YY);
    te_bn_free(RHS);
    if (with_lock) {
        te_pk_unlock(hdl);
    }
    return ret;
}

int te_ecp_check_pubkey(const te_ecp_group_t *grp, const te_ecp_point_t *pt)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(grp && pt);
    PK_CHECK_FUNC(te_bn_get_drv_handle(grp->P, &hdl));
    return te_ecp_check_pubkey_core(hdl, grp, pt, true);
}

int te_ecp_check_privkey_core(const te_drv_handle hdl,
                              const te_ecp_group_t *grp,
                              const te_bn_t *d,
                              bool with_lock)
{
    int ret    = TE_SUCCESS;
    int result = 0;

    if (with_lock) {
        te_pk_lock(hdl);
    }

    /* see SEC1 3.2 */
    ret = te_bn_cmp_s32((te_bn_t *)d, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result >= 0, TE_ERROR_INVAL_KEY);

    ret = te_bn_cmp_bn((te_bn_t *)d, grp->N, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_INVAL_KEY);

    ret = TE_SUCCESS;
finish:
    if (with_lock) {
        te_pk_unlock(hdl);
    }
    return ret;
}

int te_ecp_check_privkey(const te_ecp_group_t *grp, const te_bn_t *d)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(grp && d);
    PK_CHECK_FUNC(te_bn_get_drv_handle(grp->P, &hdl));

    return te_ecp_check_privkey_core(hdl, grp, d, true);
}

int te_ecp_gen_privkey_core(const te_drv_handle hdl,
                            const te_ecp_group_t *grp,
                            te_bn_t *d,
                            int (*f_rng)(void *, uint8_t *, size_t),
                            void *p_rng,
                            bool with_lock)
{
    int ret = TE_SUCCESS;
    size_t n_size;
    int result1 = 0, result2 = 0;
    int count = 0;

    if (with_lock) {
        te_pk_lock(hdl);
    }

    n_size = (grp->nbits + 7) / 8;

    /* SEC1 3.2.1: Generate d such that 1 <= n < N */
    /*
     * Match the procedure given in RFC 6979 (deterministic ECDSA):
     * - use the same byte ordering;
     * - keep the leftmost nbits bits of the generated octet string;
     * - try until result is in the desired range.
     * This also avoids any biais, which is especially important for ECDSA.
     */
    do {
        ret = te_bn_import_random(d, n_size, 1, f_rng, p_rng);
        PK_CHECK_RET_GO;
        ret = te_bn_shift_r(d, d, 8 * n_size - grp->nbits);
        PK_CHECK_RET_GO;

        /*
         * Each try has at worst a probability 1/2 of failing (the msb has
         * a probability 1/2 of being 0, and then the result will be < N),
         * so after 30 tries failure probability is a most 2**(-30).
         *
         * For most curves, 1 try is enough with overwhelming probability,
         * since N starts with a lot of 1s in binary, but some curves
         * such as secp224k1 are actually very close to the worst case.
         */
        if (++count > 30) {
            return TE_ERROR_GEN_RANDOM;
        }

        ret = te_bn_cmp_s32(d, 1, &result1);
        PK_CHECK_RET_GO;
        ret = te_bn_cmp_bn(d, grp->N, &result2);
        PK_CHECK_RET_GO;
    } while ((result1 < 0) || (result2 >= 0));

    ret = TE_SUCCESS;
finish:
    if (with_lock) {
        te_pk_unlock(hdl);
    }
    return ret;
}

int te_ecp_gen_privkey(const te_ecp_group_t *grp,
                       te_bn_t *d,
                       int (*f_rng)(void *, uint8_t *, size_t),
                       void *p_rng)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(grp && d && f_rng);
    PK_CHECK_FUNC(te_bn_get_drv_handle(grp->P, &hdl));

    return te_ecp_gen_privkey_core(hdl, grp, d, f_rng, p_rng, true);
}

static int te_ecp_gen_keypair_base_core(const te_drv_handle hdl,
                                        const te_ecp_group_t *grp,
                                        const te_ecp_point_t *G,
                                        te_bn_t *d,
                                        te_ecp_point_t *Q,
                                        int (*f_rng)(void *, uint8_t *, size_t),
                                        void *p_rng)
{
    int ret = TE_SUCCESS;

    te_pk_lock(hdl);

    ret = te_ecp_gen_privkey_core(hdl, grp, d, f_rng, p_rng, false);
    PK_CHECK_RET_GO;

    ret = te_ecp_mul_core(hdl, grp, Q, d, G, NULL, NULL, false);
    PK_CHECK_RET_GO;

finish:
    te_pk_unlock(hdl);
    return ret;
}

int te_ecp_gen_keypair_base(const te_ecp_group_t *grp,
                            const te_ecp_point_t *G,
                            te_bn_t *d,
                            te_ecp_point_t *Q,
                            int (*f_rng)(void *, uint8_t *, size_t),
                            void *p_rng)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(grp && G && d && Q && f_rng);
    PK_CHECK_FUNC(te_bn_get_drv_handle(grp->P, &hdl));

    return te_ecp_gen_keypair_base_core(hdl, grp, G, d, Q, f_rng, p_rng);
}

int te_ecp_gen_keypair(const te_ecp_group_t *grp,
                       te_bn_t *d,
                       te_ecp_point_t *Q,
                       int (*f_rng)(void *, uint8_t *, size_t),
                       void *p_rng)
{
    return te_ecp_gen_keypair_base(grp, (const te_ecp_point_t *)(&(grp->G)), d,
                                   Q, f_rng, p_rng);
}

static int _te_ecp_check_pubkey_async_cb(te_ecp_request_t *req)
{
    return te_ecp_check_pubkey_core(PK_REQUEST_GET_HDL(req),
                                    req->check_pubkey_args.grp,
                                    req->check_pubkey_args.pt, true);
}

int te_ecp_check_pubkey_async(te_ecp_request_t *req)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->check_pubkey_args.grp && req->check_pubkey_args.pt);

    PK_CHECK_FUNC(te_bn_get_drv_handle(req->check_pubkey_args.grp->P, &hdl));

    PK_REQUEST_INIT_DATA(req, _te_ecp_check_pubkey_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_ecp_check_privkey_async_cb(te_ecp_request_t *req)
{
    return te_ecp_check_privkey_core(PK_REQUEST_GET_HDL(req),
                                     req->check_privkey_args.grp,
                                     req->check_privkey_args.d, true);
}

int te_ecp_check_privkey_async(te_ecp_request_t *req)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->check_privkey_args.grp && req->check_privkey_args.d);

    PK_CHECK_FUNC(te_bn_get_drv_handle(req->check_privkey_args.grp->P, &hdl));

    PK_REQUEST_INIT_DATA(req, _te_ecp_check_privkey_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_ecp_gen_privkey_async_cb(te_ecp_request_t *req)
{
    return te_ecp_gen_privkey_core(PK_REQUEST_GET_HDL(req),
                                   req->gen_privkey_args.grp,
                                   req->gen_privkey_args.d,
                                   req->gen_privkey_args.f_rng,
                                   req->gen_privkey_args.p_rng,
                                   true);
}

int te_ecp_gen_privkey_async(te_ecp_request_t *req)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->gen_privkey_args.grp && req->gen_privkey_args.d &&
                   req->gen_privkey_args.f_rng);

    PK_CHECK_FUNC(te_bn_get_drv_handle(req->gen_privkey_args.grp->P, &hdl));

    PK_REQUEST_INIT_DATA(req, _te_ecp_gen_privkey_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_ecp_gen_keypair_async_cb(te_ecp_request_t *req)
{
    return te_ecp_gen_keypair(req->gen_keypair_args.grp,
                              req->gen_keypair_args.d,
                              req->gen_keypair_args.Q,
                              req->gen_keypair_args.f_rng,
                              req->gen_keypair_args.p_rng);
}

int te_ecp_gen_keypair_async(te_ecp_request_t *req)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->gen_keypair_args.grp && req->gen_keypair_args.d &&
                   req->gen_keypair_args.Q && req->gen_keypair_args.f_rng);

    PK_CHECK_FUNC(te_bn_get_drv_handle(req->gen_keypair_args.grp->P, &hdl));

    PK_REQUEST_INIT_DATA(req, _te_ecp_gen_keypair_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}
