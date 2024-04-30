
//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include "te_dsa.h"
#include "pk_internal.h"

int te_dsa_gen_keypair(const te_bn_t *P,
                       const te_bn_t *Q,
                       const te_bn_t *G,
                       te_bn_t *x,
                       te_bn_t *y,
                       int (*f_rng)(void *, uint8_t *, size_t),
                       void *p_rng)
{
    int ret     = TE_SUCCESS;
    int count   = 0;
    int result1 = 0, result2 = 0;
    size_t q_size = 0;

    ret = te_bn_bytelen((te_bn_t *)Q);
    PK_CHECK_COND_GO(ret >= 0, ret);
    PK_CHECK_COND_GO(ret != 0, TE_ERROR_BAD_INPUT_DATA);
    q_size = ret;

    /* private key x should be from range: 1 <= x <= q-1 (see FIPS 186-4 B.1.2)
     */
    do {
        ret = te_bn_import_random(x, q_size, 1, f_rng, p_rng);
        PK_CHECK_RET_GO;

        do {
            ret = te_bn_cmp_bn(x, (te_bn_t *)Q, &result1);
            PK_CHECK_RET_GO;
            if (result1 >= 0) {
                ret = te_bn_shift_r(x, x, 1);
                PK_CHECK_RET_GO;
            } else {
                break;
            }
        } while (true);

        if (count++ > 10) {
            return TE_ERROR_GEN_RANDOM;
        }

        ret = te_bn_cmp_s32(x, 1, &result2);
        PK_CHECK_RET_GO;
    } while (result2 <= 0);

    /* y = g^x mod p */
    ret = te_bn_exp_mod(y, G, (const te_bn_t *)x, P);
    PK_CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

int te_dsa_sign(const te_bn_t *P,
                const te_bn_t *Q,
                const te_bn_t *G,
                const te_bn_t *x,
                const uint8_t *buf,
                size_t size,
                te_bn_t *r,
                te_bn_t *s,
                int (*f_rng)(void *, uint8_t *, size_t),
                void *p_rng)
{
    int ret           = TE_SUCCESS;
    int count         = 0;
    int result        = 0;
    size_t q_size     = 0;
    te_bn_t *k        = NULL;
    te_bn_t *kinv     = NULL;
    te_bn_t *m        = NULL;
    te_drv_handle hdl = NULL;

    ret = te_bn_get_drv_handle((te_bn_t *)P, &hdl);
    PK_CHECK_RET_GO;

    ret = te_bn_alloc((const te_drv_handle)hdl, 0, &k);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc((const te_drv_handle)hdl, 0, &kinv);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc((const te_drv_handle)hdl, 0, &m);
    PK_CHECK_RET_GO;

    ret = te_bn_bytelen((te_bn_t *)Q);
    PK_CHECK_COND_GO(ret >= 0, ret);
    PK_CHECK_COND_GO(ret != 0, TE_ERROR_BAD_INPUT_DATA);
    q_size = ret;

    do {
        if (count++ > 10) {
            return TE_ERROR_GEN_RANDOM;
        }

        /* gen random k: 1 <= k <= q - 1 */
        ret = te_bn_import_random(k, q_size, 1, f_rng, p_rng);
        PK_CHECK_RET_GO;
        do {
            ret = te_bn_cmp_bn(k, (te_bn_t *)Q, &result);
            PK_CHECK_RET_GO;
            if (result >= 0) {
                ret = te_bn_shift_r(k, k, 1);
                PK_CHECK_RET_GO;
            } else {
                break;
            }
        } while (true);
        ret = te_bn_cmp_s32(k, 1, &result);
        PK_CHECK_RET_GO;
        if (result <= 0) {
            continue;
        }

        /* kinv = 1/k mod q */
        ret = te_bn_inv_mod(kinv, (const te_bn_t *)k, Q);
        PK_CHECK_RET_GO;

        /* r = g^k mod p mod q */
        ret = te_bn_exp_mod(r, G, (const te_bn_t *)k, P);
        PK_CHECK_RET_GO;
        ret = te_bn_mod_bn(r, (const te_bn_t *)r, Q);
        PK_CHECK_RET_GO;

        /* check r is zero */
        ret = te_bn_cmp_s32(r, 0, &result);
        PK_CHECK_RET_GO;
        if (result == 0) {
            continue;
        }

        /* FIPS 186-4 4.6: use leftmost min(bitlen(q), bitlen(hash)) bits of
         * 'hash'*/
        ret = te_bn_import(m, buf, UTILS_MIN(size, q_size), 1);
        PK_CHECK_RET_GO;

        /* s = (m + x * r)/k mod q */
        ret = te_bn_mul_bn(s, x, (const te_bn_t *)r);
        PK_CHECK_RET_GO;
        ret = te_bn_add_bn(s, (const te_bn_t *)m, (const te_bn_t *)s);
        PK_CHECK_RET_GO;
        ret = te_bn_mul_mod(s, (const te_bn_t *)kinv, (const te_bn_t *)s, Q);
        PK_CHECK_RET_GO;

        /* check i is zero */
        ret = te_bn_cmp_s32(r, 0, &result);
        PK_CHECK_RET_GO;
        if (result == 0) {
            continue;
        }
        break;
    } while (true);

finish:
    te_bn_free(k);
    te_bn_free(kinv);
    te_bn_free(m);
    return ret;
}

int te_dsa_verify(const te_bn_t *P,
                  const te_bn_t *Q,
                  const te_bn_t *G,
                  const te_bn_t *y,
                  const uint8_t *buf,
                  size_t size,
                  const te_bn_t *r,
                  const te_bn_t *s)
{
    int ret           = TE_SUCCESS;
    int result        = 0;
    size_t q_size     = 0;
    te_bn_t *m        = NULL;
    te_bn_t *w        = NULL;
    te_bn_t *u1       = NULL;
    te_bn_t *u2       = NULL;
    te_bn_t *v        = NULL;
    te_drv_handle hdl = NULL;

    ret = te_bn_get_drv_handle((te_bn_t *)P, &hdl);
    PK_CHECK_RET_GO;

    ret = te_bn_alloc((const te_drv_handle)hdl, 0, &w);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc((const te_drv_handle)hdl, 0, &m);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc((const te_drv_handle)hdl, 0, &u1);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc((const te_drv_handle)hdl, 0, &u2);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc((const te_drv_handle)hdl, 0, &v);
    PK_CHECK_RET_GO;

    ret = te_bn_bytelen((te_bn_t *)Q);
    PK_CHECK_COND_GO(ret >= 0, ret);
    PK_CHECK_COND_GO(ret != 0, TE_ERROR_BAD_INPUT_DATA);
    q_size = ret;

    /* make sure r and s are in range 1..q-1 */
    ret = te_bn_cmp_s32((te_bn_t *)r, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result >= 0, TE_ERROR_VERIFY_SIG);
    ret = te_bn_cmp_bn((te_bn_t *)r, (te_bn_t *)Q, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_VERIFY_SIG);
    ret = te_bn_cmp_s32((te_bn_t *)s, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result >= 0, TE_ERROR_VERIFY_SIG);
    ret = te_bn_cmp_bn((te_bn_t *)s, (te_bn_t *)Q, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_VERIFY_SIG);

    /* w = 1/s mod q */
    ret = te_bn_inv_mod(w, s, Q);
    PK_CHECK_RET_GO;

    /* FIPS 186-4 4.7: use leftmost min(bitlen(q), bitlen(hash)) bits of 'hash'
     */
    ret = te_bn_import(m, buf, UTILS_MIN(size, q_size), 1);
    PK_CHECK_RET_GO;

    /* u1 = m *w mod q */
    ret = te_bn_mul_mod(u1, (const te_bn_t *)m, (const te_bn_t *)w, Q);
    PK_CHECK_RET_GO;

    /* u2 = r * w mod q */
    ret = te_bn_mul_mod(u2, r, (const te_bn_t *)w, Q);
    PK_CHECK_RET_GO;

    /* v = g^u1 * y ^u2 mod p mod q */
    ret = te_bn_exp_mod(u1, G, (const te_bn_t *)u1, P);
    PK_CHECK_RET_GO;

    ret = te_bn_exp_mod(u2, y, (const te_bn_t *)u2, P);
    PK_CHECK_RET_GO;

    ret = te_bn_mul_mod(v, (const te_bn_t *)u1, (const te_bn_t *)u2, P);
    PK_CHECK_RET_GO;

    ret = te_bn_mod_bn(v, (const te_bn_t *)v, Q);
    PK_CHECK_RET_GO;

    /* compare r with v */
    ret = te_bn_cmp_bn((te_bn_t *)r, v, &result);
    PK_CHECK_RET_GO;
    if (result != 0) {
        ret = TE_ERROR_VERIFY_SIG;
        goto finish;
    }

    ret = TE_SUCCESS;
finish:
    te_bn_free(m);
    te_bn_free(w);
    te_bn_free(u1);
    te_bn_free(u2);
    te_bn_free(v);
    return ret;
}