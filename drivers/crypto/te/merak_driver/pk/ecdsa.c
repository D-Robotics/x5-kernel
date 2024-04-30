//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include "pk_internal.h"
#include "te_ecdsa.h"
/*
 * Derive a suitable integer for group grp from a buffer of length len
 * SEC1 4.1.3 step 5 aka SEC1 4.1.4 step 3
 */
static int _derive_mpi(const te_ecp_group_t *grp,
                       te_bn_t *x,
                       const uint8_t *buf,
                       size_t blen)
{
    int ret         = TE_SUCCESS;
    size_t n_size   = (grp->nbits + 7) / 8;
    size_t use_size = blen > n_size ? n_size : blen;

    ret = te_bn_import(x, buf, use_size, 1);
    PK_CHECK_RET_GO;
    if (use_size * 8 > grp->nbits) {
        ret = te_bn_shift_r(x, x, use_size * 8 - grp->nbits);
        PK_CHECK_RET_GO;
    }

    /* While at it, reduce modulo N */
    ret = te_bn_mod_bn(x, x, grp->N);
    PK_CHECK_RET_GO;

finish:
    return (ret);
}

/*
 * Compute ECDSA signature of a hashed message (SEC1 4.1.3)
 * Obviously, compared to SEC1 4.1.3, we skip step 4 (hash message)
 */

static int te_ecdsa_sign_core(const te_drv_handle hdl,
                              const te_ecp_group_t *grp,
                              const te_bn_t *d,
                              const uint8_t *buf,
                              size_t size,
                              te_bn_t *r,
                              te_bn_t *s,
                              int (*f_rng)(void *, uint8_t *, size_t),
                              void *p_rng)
{
    int ret = TE_SUCCESS, key_tries = 0, sign_tries = 0;
    te_ecp_point_t R = {0};
    te_bn_t *k = NULL, *e = NULL;
#ifdef CFG_ACA_BLINDING_EN
    te_bn_t *t  = NULL;
#endif
    te_bn_t *pk = NULL, *pr = NULL;
    int result = 0;

    te_pk_lock(hdl);

    ret = te_ecp_point_init(hdl, &R);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &k);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &e);
    PK_CHECK_RET_GO;
#ifdef CFG_ACA_BLINDING_EN
    ret = te_bn_alloc(hdl, 0, &t);
    PK_CHECK_RET_GO;
#endif
    pk = k;
    pr = r;

    /* Fail cleanly on curves such as Curve25519 that can't be used for ECDSA */
    /* TODO: Curve25519 not supported! */

    /* Make sure d is in range 1..n-1 */
    ret = te_bn_cmp_s32((te_bn_t *)d, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result >= 0, TE_ERROR_INVAL_KEY);
    ret = te_bn_cmp_bn((te_bn_t *)d, grp->N, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_INVAL_KEY);

    sign_tries = 0;
    do {
        if (sign_tries++ > 10) {
            ret = TE_ERROR_GEN_RANDOM;
            goto finish;
        }
        /*
         * Steps 1-3: generate a suitable ephemeral keypair
         * and set r = xR mod n
         */
        key_tries = 0;
        do {
            if (key_tries++ > 10) {
                ret = TE_ERROR_GEN_RANDOM;
                goto finish;
            }

            ret = te_ecp_gen_privkey_core(hdl, grp, pk, f_rng, p_rng, false);
            PK_CHECK_RET_GO;
            ret = te_ecp_mul_core(hdl,
                                  grp,
                                  &R,
                                  (const te_bn_t *)(pk),
                                  (const te_ecp_point_t *)(&(grp->G)),
                                  f_rng,
                                  p_rng,
                                  false);
            PK_CHECK_RET_GO;

            ret = te_bn_mod_bn(pr, R.X, grp->N);
            PK_CHECK_RET_GO;

            ret = te_bn_cmp_s32(pr, 0, &result);
            PK_CHECK_RET_GO;
        } while (result == 0);

        /*
         * Accounting for everything up to the end of the loop
         * (step 6, but checking now avoids saving e and t)
         */

        ret = _derive_mpi(grp, e, buf, size);
        PK_CHECK_RET_GO;
#ifdef CFG_ACA_BLINDING_EN
        /*
         * Generate a random value to blind inv_mod in next step,
         * avoiding a potential timing leak.
         */
        ret = te_ecp_gen_privkey_core(hdl, grp, t, f_rng, p_rng, false);
        PK_CHECK_RET_GO;
#endif
        /*
         * Step 6: compute s = (e + r * d) / k = t (e + rd) / (kt) mod n
         * t == 1 if blinding is disabled
         */
        ret = te_bn_mul_bn(s, pr, d);
        PK_CHECK_RET_GO;
        ret = te_bn_add_bn(e, e, s);
        PK_CHECK_RET_GO;
#ifdef CFG_ACA_BLINDING_EN
        ret = te_bn_mul_bn(e, e, t);
        PK_CHECK_RET_GO;
        ret = te_bn_mul_bn(pk, pk, t);
        PK_CHECK_RET_GO;
#endif
        ret = te_bn_inv_mod(s, pk, grp->N);
        PK_CHECK_RET_GO;
        ret = te_bn_mul_mod(s, s, e, grp->N);
        PK_CHECK_RET_GO;

        ret = te_bn_cmp_s32(s, 0, &result);
        PK_CHECK_RET_GO;
    } while (result == 0);

    ret = TE_SUCCESS;
finish:
    te_ecp_point_free(&R);
    te_bn_free(k);
    te_bn_free(e);
#ifdef CFG_ACA_BLINDING_EN
    te_bn_free(t);
#endif
    te_pk_unlock(hdl);
    return ret;
}

int te_ecdsa_sign(const te_ecp_group_t *grp,
                  const te_bn_t *d,
                  const uint8_t *buf,
                  size_t size,
                  te_bn_t *r,
                  te_bn_t *s,
                  int (*f_rng)(void *, uint8_t *, size_t),
                  void *p_rng)
{
    int ret                 = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(grp && d && buf && size && r && s && f_rng);
    PK_CHECK_FUNC(te_bn_get_drv_handle(grp->P, &hdl));

    return te_ecdsa_sign_core(hdl, grp, d, buf, size, r, s, f_rng, p_rng);
}

/*
 * Verify ECDSA signature of hashed message (SEC1 4.1.4)
 * Obviously, compared to SEC1 4.1.3, we skip step 2 (hash message)
 */
static int te_ecdsa_verify_core(const te_drv_handle hdl,
                                const te_ecp_group_t *grp,
                                const uint8_t *buf,
                                size_t size,
                                const te_ecp_point_t *Q,
                                const te_bn_t *r,
                                const te_bn_t *s)
{
    int ret    = TE_SUCCESS;
    te_bn_t *e = NULL, *s_inv = NULL, *u1 = NULL, *u2 = NULL;
    te_ecp_point_t R = {0};
    te_bn_t *pu1 = NULL, *pu2 = NULL;
    int result = 0;

    te_pk_lock(hdl);

    ret = te_ecp_point_init(hdl, &R);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &e);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &s_inv);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &u1);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &u2);
    PK_CHECK_RET_GO;
    pu1 = u1;
    pu2 = u2;

    /* Fail cleanly on curves such as Curve25519 that can't be used for ECDSA */
    /* TODO: Curve25519 not supported! */

    /*
     * Step 1: make sure r and s are in range 1..n-1
     */
    ret = te_bn_cmp_s32((te_bn_t *)r, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result >= 0, TE_ERROR_VERIFY_SIG);
    ret = te_bn_cmp_bn((te_bn_t *)r, grp->N, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_VERIFY_SIG);
    ret = te_bn_cmp_s32((te_bn_t *)s, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result >= 0, TE_ERROR_VERIFY_SIG);
    ret = te_bn_cmp_bn((te_bn_t *)s, grp->N, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_VERIFY_SIG);

    /*
     * Step 3: derive MPI from hashed message
     */
    ret = _derive_mpi(grp, e, buf, size);
    PK_CHECK_RET_GO;

    /*
     * Step 4: u1 = e / s mod n, u2 = r / s mod n
     */
    ret = te_bn_inv_mod(s_inv, s, grp->N);
    PK_CHECK_RET_GO;
    ret = te_bn_mul_mod(pu1, e, s_inv, grp->N);
    PK_CHECK_RET_GO;
    ret = te_bn_mul_mod(pu2, r, s_inv, grp->N);
    PK_CHECK_RET_GO;

    /*
     * Step 5: R = u1 G + u2 Q
     */
    ret = te_ecp_muladd_core(hdl, grp, &R, pu1, &(grp->G), pu2, Q, false);
    PK_CHECK_RET_GO;
    ret = te_ecp_is_zero(&R);
    PK_CHECK_COND_GO(ret >= 0, ret);
    if (1 == ret) {
        ret = TE_ERROR_VERIFY_SIG;
        goto finish;
    }
    /*
     * Step 6: convert xR to an integer (no-op)
     * Step 7: reduce xR mod n (gives v)
     */

    ret = te_bn_mod_bn(R.X, R.X, grp->N);
    PK_CHECK_RET_GO;

    /*
     * Step 8: check if v (that is, R.X) is equal to r
     */
    ret = te_bn_cmp_bn(R.X, (te_bn_t *)r, &result);
    PK_CHECK_RET_GO;
    if (result != 0) {
        ret = TE_ERROR_VERIFY_SIG;
        goto finish;
    }

    ret = TE_SUCCESS;
finish:
    te_ecp_point_free(&R);
    te_bn_free(e);
    te_bn_free(s_inv);
    te_bn_free(u1);
    te_bn_free(u2);
    te_pk_unlock(hdl);
    return ret;
}

int te_ecdsa_verify(const te_ecp_group_t *grp,
                    const uint8_t *buf,
                    size_t size,
                    const te_ecp_point_t *Q,
                    const te_bn_t *r,
                    const te_bn_t *s)
{
    int ret                 = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(grp && buf && size && Q && r && s);
    PK_CHECK_FUNC(te_bn_get_drv_handle(grp->P, &hdl));

    return te_ecdsa_verify_core(hdl, grp, buf, size, Q, r, s);
}

int te_ecdsa_gen_keypair(te_ecp_group_t *grp,
                         te_bn_t *d,
                         te_ecp_point_t *Q,
                         int (*f_rng)(void *, uint8_t *, size_t),
                         void *p_rng)
{
    return te_ecp_gen_keypair(grp, d, Q, f_rng, p_rng);
}

static int _te_ecdsa_sign_async_cb(te_ecdsa_request_t *req)
{
    return te_ecdsa_sign_core(PK_REQUEST_GET_HDL(req),
                              req->sign_args.grp,
                              req->sign_args.d,
                              req->sign_args.buf,
                              req->sign_args.size,
                              req->sign_args.r,
                              req->sign_args.s,
                              req->sign_args.f_rng,
                              req->sign_args.p_rng);
}

int te_ecdsa_sign_async(te_ecdsa_request_t *req)
{
    int ret                 = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->sign_args.grp);
    PK_CHECK_PARAM(req->sign_args.d);
    PK_CHECK_PARAM(req->sign_args.buf);
    PK_CHECK_PARAM(req->sign_args.size);
    PK_CHECK_PARAM(req->sign_args.r);
    PK_CHECK_PARAM(req->sign_args.s);
    PK_CHECK_PARAM(req->sign_args.f_rng);

    PK_CHECK_FUNC(te_bn_get_drv_handle(req->sign_args.grp->P, &hdl));

    PK_REQUEST_INIT_DATA(req, _te_ecdsa_sign_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_ecdsa_verify_async_cb(te_ecdsa_request_t *req)
{
    return te_ecdsa_verify_core(PK_REQUEST_GET_HDL(req),
                                req->verify_args.grp,
                                req->verify_args.buf,
                                req->verify_args.size,
                                req->verify_args.Q,
                                req->verify_args.r,
                                req->verify_args.s);
}

int te_ecdsa_verify_async(te_ecdsa_request_t *req)
{
    int ret                 = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->verify_args.grp);
    PK_CHECK_PARAM(req->verify_args.buf);
    PK_CHECK_PARAM(req->verify_args.size);
    PK_CHECK_PARAM(req->verify_args.Q);
    PK_CHECK_PARAM(req->verify_args.r);
    PK_CHECK_PARAM(req->verify_args.s);

    PK_CHECK_FUNC(te_bn_get_drv_handle(req->verify_args.grp->P, &hdl));

    PK_REQUEST_INIT_DATA(req, _te_ecdsa_verify_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}