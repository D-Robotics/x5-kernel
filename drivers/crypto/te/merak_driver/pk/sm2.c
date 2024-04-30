//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include "pk_internal.h"
#include "te_sm2.h"
#include "te_ecp.h"
#include "te_ecdsa.h"
#include "te_ecdh.h"

int te_sm2dsa_sign(const te_bn_t *d,
                   const uint8_t *buf,
                   size_t size,
                   te_bn_t *r,
                   te_bn_t *s,
                   int (*f_rng)(void *, uint8_t *, size_t),
                   void *p_rng)
{
    int ret = TE_SUCCESS, key_tries = 0, sign_tries = 0;
    te_drv_handle hdl      = NULL;
    te_ecp_group_t sm2_grp = {0};
    te_ecp_point_t R       = {0};
    te_bn_t *k = NULL, *e = NULL, *t = NULL;
#ifdef CFG_ACA_BLINDING_EN
    te_bn_t *blinding = NULL;
#endif
    int result = 0, result1 = 0;

    PK_CHECK_PARAM(d && buf && size && r && s && f_rng);

    PK_CHECK_FUNC(te_bn_get_drv_handle((te_bn_t *)d, &hdl));

    te_pk_lock(hdl);

    ret = te_ecp_group_init(hdl, &sm2_grp);
    PK_CHECK_RET_GO;

    ret = te_ecp_group_load(&sm2_grp, TE_ECP_DP_SM2P256V1);
    PK_CHECK_RET_GO;

    ret = te_ecp_point_init(hdl, &R);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &k);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &e);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &t);
    PK_CHECK_RET_GO;
#ifdef CFG_ACA_BLINDING_EN
    ret = te_bn_alloc(hdl, 0, &blinding);
    PK_CHECK_RET_GO;
#endif
    /* Make sure d is in range 1..n-1 */
    ret = te_bn_cmp_s32((te_bn_t *)d, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result >= 0, TE_ERROR_INVAL_KEY);
    ret = te_bn_cmp_bn((te_bn_t *)d, sm2_grp.N, &result);
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

            ret =
                te_ecp_gen_privkey_core(hdl, &sm2_grp, k, f_rng, p_rng, false);
            PK_CHECK_RET_GO;
            ret = te_ecp_mul_core(hdl,
                                  &sm2_grp,
                                  &R,
                                  (const te_bn_t *)(k),
                                  (const te_ecp_point_t *)(&(sm2_grp.G)),
                                  NULL,
                                  NULL,
                                  false);
            PK_CHECK_RET_GO;

            /* import digest to e */
            ret = te_bn_import(e, buf, size, 1);
            PK_CHECK_RET_GO;

            /* r = (e + x1) mod N */
            ret = te_bn_add_mod(r, e, R.X, sm2_grp.N);
            PK_CHECK_RET_GO;

            /* t = r + k */
            ret = te_bn_add_bn(t, r, k);
            PK_CHECK_RET_GO;

            ret = te_bn_cmp_s32(r, 0, &result);
            PK_CHECK_RET_GO;
            ret = te_bn_cmp_bn(t, sm2_grp.N, &result1);
            PK_CHECK_RET_GO;
        } while ((result == 0) || (result1 == 0));

#ifdef CFG_ACA_BLINDING_EN
        /*
         * Generate a random value to blind inv_mod in next step,
         * avoiding a potential timing leak.
         */
        ret = te_ecp_gen_privkey_core(
            hdl, &sm2_grp, blinding, f_rng, p_rng, false);
        PK_CHECK_RET_GO;
#endif

        /*
         * Step 6: compute s = (k - r * d)/(1 + d) =
         *        t *(k - r * d)/((1 + d) * t) mod n
         */
        ret = te_bn_mul_mod(e, r, d, sm2_grp.N);
        PK_CHECK_RET_GO;
        ret = te_bn_mod_bn(k, k, sm2_grp.N);
        PK_CHECK_RET_GO;
        ret = te_bn_sub_mod(e, k, e, sm2_grp.N);
        PK_CHECK_RET_GO;

#ifdef CFG_ACA_BLINDING_EN
        ret = te_bn_mul_mod(e, e, blinding, sm2_grp.N);
        PK_CHECK_RET_GO;
#endif
        ret = te_bn_add_s32(k, d, 1);
        PK_CHECK_RET_GO;

#ifdef CFG_ACA_BLINDING_EN
        ret = te_bn_mul_mod(k, k, blinding, sm2_grp.N);
        PK_CHECK_RET_GO;
#endif
        ret = te_bn_inv_mod(s, k, sm2_grp.N);
        PK_CHECK_RET_GO;
        ret = te_bn_mul_mod(s, s, e, sm2_grp.N);
        PK_CHECK_RET_GO;

        ret = te_bn_cmp_s32(s, 0, &result);
        PK_CHECK_RET_GO;
    } while (result == 0);

    ret = TE_SUCCESS;
finish:
    te_ecp_group_free(&sm2_grp);
    te_ecp_point_free(&R);
    te_bn_free(k);
    te_bn_free(e);
    te_bn_free(t);
#ifdef CFG_ACA_BLINDING_EN
    te_bn_free(blinding);
#endif
    te_pk_unlock(hdl);
    return ret;
}

int te_sm2dsa_verify(const uint8_t *buf,
                     size_t size,
                     const te_ecp_point_t *Q,
                     const te_bn_t *r,
                     const te_bn_t *s)
{
    int ret                = TE_SUCCESS;
    te_drv_handle hdl      = NULL;
    te_ecp_group_t sm2_grp = {0};
    te_bn_t *e = NULL, *u1 = NULL, *u2 = NULL;
    te_ecp_point_t R = {0};
    int result       = 0;

    PK_CHECK_PARAM(buf && size && Q && r && s);

    PK_CHECK_FUNC(te_bn_get_drv_handle((te_bn_t *)r, &hdl));

    te_pk_lock(hdl);

    ret = te_ecp_group_init(hdl, &sm2_grp);
    PK_CHECK_RET_GO;
    ret = te_ecp_group_load(&sm2_grp, TE_ECP_DP_SM2P256V1);
    PK_CHECK_RET_GO;

    ret = te_ecp_point_init(hdl, &R);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &e);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &u1);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &u2);
    PK_CHECK_RET_GO;

    /*
     * Step 1: make sure r and s are in range 1..n-1
     */
    ret = te_bn_cmp_s32((te_bn_t *)r, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result >= 0, TE_ERROR_VERIFY_SIG);
    ret = te_bn_cmp_bn((te_bn_t *)r, sm2_grp.N, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_VERIFY_SIG);
    ret = te_bn_cmp_s32((te_bn_t *)s, 1, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result >= 0, TE_ERROR_VERIFY_SIG);
    ret = te_bn_cmp_bn((te_bn_t *)s, sm2_grp.N, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result < 0, TE_ERROR_VERIFY_SIG);

    /*
     * Additional precaution: make sure Q is valid
     */
    ret = te_ecp_check_pubkey_core(hdl, &sm2_grp, Q, false);
    PK_CHECK_RET_GO;

    /* import digest to e */
    ret = te_bn_import(e, buf, size, 1);
    PK_CHECK_RET_GO;

    /* t = (r + s) mod n */
    ret = te_bn_add_mod(u2, r, s, sm2_grp.N);
    PK_CHECK_RET_GO;

    /* compare t with 0 */
    ret = te_bn_cmp_s32(u2, 0, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result != 0, TE_ERROR_VERIFY_SIG);

    /* R = s * G + u2 * Q */
    ret = te_ecp_muladd_core(hdl, &sm2_grp, &R, s, &(sm2_grp.G), u2, Q, false);
    PK_CHECK_RET_GO;

    /* (R.x + e) mod n*/
    ret = te_bn_add_mod(u1, R.X, e, sm2_grp.N);
    PK_CHECK_RET_GO;

    /*
     * check if v (that is, u1) is equal to r
     */
    ret = te_bn_cmp_bn(u1, (te_bn_t *)r, &result);
    PK_CHECK_RET_GO;
    PK_CHECK_COND_GO(result == 0, TE_ERROR_VERIFY_SIG);

    ret = TE_SUCCESS;
finish:
    te_ecp_group_free(&sm2_grp);
    te_ecp_point_free(&R);
    te_bn_free(e);
    te_bn_free(u1);
    te_bn_free(u2);
    te_pk_unlock(hdl);
    return ret;
}

int te_sm2dsa_gen_keypair(const te_drv_handle hdl,
                          te_bn_t *d,
                          te_ecp_point_t *Q,
                          int (*f_rng)(void *, uint8_t *, size_t),
                          void *p_rng)
{
    int ret                = TE_SUCCESS;
    te_ecp_group_t sm2_grp = {0};

    PK_CHECK_PARAM(hdl && d && Q && f_rng);

    ret = te_ecp_group_init(hdl, &sm2_grp);
    PK_CHECK_RET_GO;
    ret = te_ecp_group_load(&sm2_grp, TE_ECP_DP_SM2P256V1);
    PK_CHECK_RET_GO;

    ret = te_ecdsa_gen_keypair(&sm2_grp, d, Q, f_rng, p_rng);
    PK_CHECK_RET_GO;

finish:
    te_ecp_group_free(&sm2_grp);
    return ret;
}

int te_sm2dh_gen_public(const te_drv_handle hdl,
                        te_bn_t *d,
                        te_ecp_point_t *Q,
                        int (*f_rng)(void *, uint8_t *, size_t),
                        void *p_rng)
{
    int ret                = TE_SUCCESS;
    te_ecp_group_t sm2_grp = {0};

    PK_CHECK_PARAM(hdl && d && Q && f_rng);

    ret = te_ecp_group_init(hdl, &sm2_grp);
    PK_CHECK_RET_GO;
    ret = te_ecp_group_load(&sm2_grp, TE_ECP_DP_SM2P256V1);
    PK_CHECK_RET_GO;

    ret = te_ecdh_gen_public(&sm2_grp, d, Q, f_rng, p_rng);
    PK_CHECK_RET_GO;

finish:
    te_ecp_group_free(&sm2_grp);
    return ret;
}

int te_sm2dh_compute_shared(const te_bn_t *d,
                            const te_bn_t *tmp,
                            const te_ecp_point_t *tmp_Q,
                            const te_ecp_point_t *other_tmp_Q,
                            const te_ecp_point_t *other_Q,
                            te_ecp_point_t *K,
                            int (*f_rng)(void *, uint8_t *, size_t),
                            void *p_rng)
{
    int ret                = TE_SUCCESS;
    te_drv_handle hdl      = NULL;
    te_ecp_group_t sm2_grp = {0};
    size_t w               = 0;
    te_bn_t *t1 = NULL, *t2 = NULL, *u2 = NULL;

    (void)(f_rng);
    (void)(p_rng);
    PK_CHECK_PARAM(d && other_Q && K);

    PK_CHECK_FUNC(te_bn_get_drv_handle((te_bn_t *)d, &hdl));

    ret = te_ecp_group_init(hdl, &sm2_grp);
    PK_CHECK_RET_GO;
    ret = te_ecp_group_load(&sm2_grp, TE_ECP_DP_SM2P256V1);
    PK_CHECK_RET_GO;

    PK_CHECK_COND_GO(sm2_grp.nbits >= 64, TE_ERROR_BAD_INPUT_DATA);

    /* check others' pubkey other_Q and other_tmp_Q */
    ret = te_ecp_check_pubkey_core(hdl, &sm2_grp, other_Q, false);
    PK_CHECK_RET_GO;
    ret = te_ecp_check_pubkey_core(hdl, &sm2_grp, other_tmp_Q, false);
    PK_CHECK_RET_GO;

    ret = te_bn_alloc(hdl, 0, &t1);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &t2);
    PK_CHECK_RET_GO;

    ret = te_bn_alloc(hdl, 0, &u2);
    PK_CHECK_RET_GO;

    /**
     * w = ceil(keybits / 2) - 1
     * x = 2^w + (x and (2^w - 1)) = 2^w + (x mod 2^w), where x = tmp_Q.X
     * t = (d + x * r) mod n, where r = tmp, n = sm2_grp.N, d = d
     * t = (h * t) mod n = t mod n = t (h == 1)
     */

    w = (((sm2_grp.nbits + 1) / 2) - 1);

    /* t1 = 2^w */
    ret = te_bn_grow(t1, (w + 7) / 8);
    PK_CHECK_RET_GO;
    ret = te_bn_set_bit(t1, w, 1);
    PK_CHECK_RET_GO;

    /* t2 = tmp_Q.X mod 2^w */
    ret = te_bn_mod_bn(t2, tmp_Q->X, t1);
    PK_CHECK_RET_GO;

    /* t2 = t2 + 2^w */
    ret = te_bn_add_bn(t2, t2, t1);
    PK_CHECK_RET_GO;

    /* t2 = (t2 * tmp) mod N */
    ret = te_bn_mul_mod(t2, t2, tmp, sm2_grp.N);
    PK_CHECK_RET_GO;

    /* t2 = (t2 + d) mod n */
    ret = te_bn_add_mod(t2, t2, d, sm2_grp.N);
    PK_CHECK_RET_GO;

    /**
     * w = ceil(keybits / 2) - 1
     * x = 2^w + (x and (2^w - 1)) = 2^w + (x mod 2^w), where x = other_tmp_Q.X
     * U = ht * (P + x * R), where t == t2, P = other_Q, R = other_tmp_Q
     * check U != O
     */

    /* u2 = other_tmp_Q.X mod 2^w */
    ret = te_bn_mod_bn(u2, other_tmp_Q->X, t1);
    PK_CHECK_RET_GO;

    /* u2 = u2 + 2^w */
    ret = te_bn_add_bn(u2, u2, t1);
    PK_CHECK_RET_GO;

    /* u2 = (u2 * t2) mod N */
    ret = te_bn_mul_mod(u2, u2, t2, sm2_grp.N);
    PK_CHECK_RET_GO;

    /* K = t2 * other_Q + u2 * other_tmp_Q */
    ret = te_ecp_muladd_core(
        hdl, &sm2_grp, K, t2, other_Q, u2, other_tmp_Q, false);
    PK_CHECK_RET_GO;

    ret = te_ecp_is_zero(K);
    PK_CHECK_COND_GO((ret == 0) || (ret == 1), ret);
    PK_CHECK_COND_GO(ret == 0, TE_ERROR_BAD_INPUT_DATA);

finish:
    te_ecp_group_free(&sm2_grp);
    te_bn_free(t1);
    te_bn_free(t2);
    te_bn_free(u2);
    return ret;
}

static int _te_sm2dsa_sign_async_cb(te_sm2_request_t *req)
{
    return te_sm2dsa_sign(req->sm2dsa_sign_args.d,
                          req->sm2dsa_sign_args.buf,
                          req->sm2dsa_sign_args.size,
                          req->sm2dsa_sign_args.r,
                          req->sm2dsa_sign_args.s,
                          req->sm2dsa_sign_args.f_rng,
                          req->sm2dsa_sign_args.p_rng);
}
int te_sm2dsa_sign_async(te_sm2_request_t *req)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->sm2dsa_sign_args.d && req->sm2dsa_sign_args.buf &&
                   req->sm2dsa_sign_args.size && req->sm2dsa_sign_args.r &&
                   req->sm2dsa_sign_args.s && req->sm2dsa_sign_args.f_rng);

    PK_CHECK_FUNC(
        te_bn_get_drv_handle((te_bn_t *)(req->sm2dsa_sign_args.d), &hdl));
    PK_REQUEST_INIT_DATA(req, _te_sm2dsa_sign_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_sm2dsa_verify_async_cb(te_sm2_request_t *req)
{
    return te_sm2dsa_verify(req->sm2dsa_verify_args.buf,
                            req->sm2dsa_verify_args.size,
                            req->sm2dsa_verify_args.Q,
                            req->sm2dsa_verify_args.r,
                            req->sm2dsa_verify_args.s);
}
int te_sm2dsa_verify_async(te_sm2_request_t *req)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->sm2dsa_verify_args.buf &&
                   req->sm2dsa_verify_args.size && req->sm2dsa_verify_args.Q &&
                   req->sm2dsa_verify_args.r && req->sm2dsa_verify_args.s);

    PK_CHECK_FUNC(
        te_bn_get_drv_handle((te_bn_t *)(req->sm2dsa_verify_args.r), &hdl));
    PK_REQUEST_INIT_DATA(req, _te_sm2dsa_verify_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_sm2dsa_gen_keypair_async_cb(te_sm2_request_t *req)
{
    return te_sm2dsa_gen_keypair(req->sm2dsa_gen_keypair_args.hdl,
                                 req->sm2dsa_gen_keypair_args.d,
                                 req->sm2dsa_gen_keypair_args.Q,
                                 req->sm2dsa_gen_keypair_args.f_rng,
                                 req->sm2dsa_gen_keypair_args.p_rng);
}

int te_sm2dsa_gen_keypair_async(te_sm2_request_t *req)
{
    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(
        req->sm2dsa_gen_keypair_args.hdl && req->sm2dsa_gen_keypair_args.d &&
        req->sm2dsa_gen_keypair_args.Q && req->sm2dsa_gen_keypair_args.f_rng);

    PK_REQUEST_INIT_DATA(
        req, _te_sm2dsa_gen_keypair_async_cb, req->sm2dsa_gen_keypair_args.hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_sm2dh_gen_public_async_cb(te_sm2_request_t *req)
{
    PK_CHECK_PARAM(PK_REQUEST_GET_HDL(req) == req->sm2dh_gen_public_args.hdl);
    return te_sm2dh_gen_public(req->sm2dh_gen_public_args.hdl,
                               req->sm2dh_gen_public_args.d,
                               req->sm2dh_gen_public_args.Q,
                               req->sm2dh_gen_public_args.f_rng,
                               req->sm2dh_gen_public_args.p_rng);
}
int te_sm2dh_gen_public_async(te_sm2_request_t *req)
{
    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(
        req->sm2dh_gen_public_args.hdl && req->sm2dh_gen_public_args.d &&
        req->sm2dh_gen_public_args.Q && req->sm2dh_gen_public_args.f_rng);

    PK_REQUEST_INIT_DATA(
        req, _te_sm2dh_gen_public_async_cb, req->sm2dh_gen_public_args.hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_sm2dh_compute_shared_async_cb(te_sm2_request_t *req)
{
    return te_sm2dh_compute_shared(req->sm2dh_compute_shared_args.d,
                                   req->sm2dh_compute_shared_args.tmp,
                                   req->sm2dh_compute_shared_args.tmp_Q,
                                   req->sm2dh_compute_shared_args.other_tmp_Q,
                                   req->sm2dh_compute_shared_args.other_Q,
                                   req->sm2dh_compute_shared_args.K,
                                   req->sm2dh_compute_shared_args.f_rng,
                                   req->sm2dh_compute_shared_args.p_rng);
}
int te_sm2dh_compute_shared_async(te_sm2_request_t *req)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->sm2dh_compute_shared_args.d &&
                   req->sm2dh_compute_shared_args.tmp &&
                   req->sm2dh_compute_shared_args.tmp_Q &&
                   req->sm2dh_compute_shared_args.other_tmp_Q &&
                   req->sm2dh_compute_shared_args.other_Q &&
                   req->sm2dh_compute_shared_args.K &&
                   req->sm2dh_compute_shared_args.f_rng);

    PK_CHECK_FUNC(te_bn_get_drv_handle(
        (te_bn_t *)(req->sm2dh_compute_shared_args.d), &hdl));

    PK_REQUEST_INIT_DATA(req, _te_sm2dh_compute_shared_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}