//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include "pk_internal.h"
#include "te_dhm.h"

/*
 * Verify sanity of parameter with regards to P
 *
 * Parameter should be: 2 <= public_param <= P - 2
 *
 * This means that we need to return an error if
 *              public_param < 2 or public_param > P-2
 *
 * return:
 * 0: success, check pass
 * 1: check failed.
 * others: failed
 *
 * For more information on the attack, see:
 *  http://www.cl.cam.ac.uk/~rja14/Papers/psandqs.pdf
 *  http://web.nvd.nist.gov/view/vuln/detail?vulnId=CVE-2005-2643
 */
static int _dhm_check_range(const te_drv_handle hdl,
                            const te_bn_t *param,
                            const te_bn_t *P)
{
    int ret    = TE_SUCCESS;
    te_bn_t *L = NULL, *U = NULL;
    int result = 0, result1 = 0;

    ret = te_bn_alloc(hdl, 0, &L);
    PK_CHECK_RET_GO;
    ret = te_bn_alloc(hdl, 0, &U);
    PK_CHECK_RET_GO;

    ret = te_bn_import_s32(L, 2);
    PK_CHECK_RET_GO;
    ret = te_bn_sub_s32(U, P, 2);
    PK_CHECK_RET_GO;

    ret = te_bn_cmp_bn((te_bn_t *)param, L, &result);
    PK_CHECK_RET_GO;
    ret = te_bn_cmp_bn((te_bn_t *)param, U, &result1);
    PK_CHECK_RET_GO;
    if ((result < 0) || (result1 > 0)) {
        ret = 1;
        goto finish;
    }
    ret = 0;
finish:
    te_bn_free(L);
    te_bn_free(U);
    return ret;
}

static int te_dhm_make_public_core(const te_drv_handle hdl,
                                   const te_bn_t *P,
                                   const te_bn_t *G,
                                   size_t x_size,
                                   te_bn_t *X,
                                   te_bn_t *GX,
                                   int (*f_rng)(void *, uint8_t *, size_t),
                                   void *p_rng)
{
    int ret    = TE_SUCCESS;
    int result = 0;
    int count  = 0;

    te_pk_lock(hdl);

    ret = te_bn_cmp_s32((te_bn_t *)P, 0, &result);
    PK_CHECK_RET_GO;
    if (result == 0) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        goto finish;
    }
    /* Check if X is 0 */
    ret = te_bn_cmp_s32(X, 0, &result);
    PK_CHECK_RET_GO;
    if (result == 0) {
        /* X is 0, generate */
        goto __gen_priv;
    } else {
        goto __check_priv;
    }

__gen_priv:
    /*
     * generate X and calculate GX = G^X mod P
     */
    do {
        ret = te_bn_import_random(X, x_size, 1, f_rng, p_rng);
        PK_CHECK_RET_GO;

        do {
            ret = te_bn_cmp_bn(X, (te_bn_t *)P, &result);
            PK_CHECK_RET_GO;
            if (result >= 0) {
                ret = te_bn_shift_r(X, X, 1);
                PK_CHECK_RET_GO;
            } else {
                break;
            }
        } while (true);
        if (count++ > 10) {
            ret = TE_ERROR_MAKE_PUBLIC;
            goto finish;
        }
        ret = _dhm_check_range(hdl, X, P);
        PK_CHECK_COND_GO((0 == ret) || (1 == ret), ret);
    } while (ret != 0);

    goto __gen_public;

__check_priv:
    ret = _dhm_check_range(hdl, X, P);
    PK_CHECK_COND_GO((0 == ret) || (1 == ret), ret);
    if (0 != ret) {
        ret = TE_ERROR_NOT_ACCEPTABLE;
        goto finish;
    }

__gen_public:
    ret = te_bn_exp_mod(GX, G, X, P);
    PK_CHECK_RET_GO;

    ret = _dhm_check_range(hdl, GX, P);
    PK_CHECK_COND_GO((0 == ret) || (1 == ret), ret);
    if (0 != ret) {
        ret = TE_ERROR_NOT_ACCEPTABLE;
        goto finish;
    }
    PK_CHECK_RET_GO;

finish:
    te_pk_unlock(hdl);
    return ret;
}

/*
 * Create own private value X and export G^X
 */
int te_dhm_make_public(const te_bn_t *P,
                       const te_bn_t *G,
                       size_t x_size,
                       te_bn_t *X,
                       te_bn_t *GX,
                       int (*f_rng)(void *, uint8_t *, size_t),
                       void *p_rng)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(P && G && (x_size >= 1) && X && GX && f_rng);

    PK_CHECK_FUNC(te_bn_get_drv_handle((te_bn_t *)P, &hdl));
    return te_dhm_make_public_core(hdl, P, G, x_size, X, GX, f_rng, p_rng);
}

#ifdef CFG_ACA_BLINDING_EN
/*
 * Use the blinding method and optimisation suggested in section 10 of:
 *  KOCHER, Paul C. Timing attacks on implementations of Diffie-Hellman, RSA,
 *  DSS, and other systems. In : Advances in Cryptology-CRYPTO'96. Springer
 *  Berlin Heidelberg, 1996. p. 104-113.
 */
static int _dhm_update_blinding(const te_bn_t *P,
                                const te_bn_t *X,
                                te_bn_t *pX,
                                te_bn_t *Vi,
                                te_bn_t *Vf,
                                int (*f_rng)(void *, unsigned char *, size_t),
                                void *p_rng)
{
    int ret    = TE_SUCCESS;
    int result = 0;
    int count  = 0;
    int p_size = 0;

    /*
     * Don't use any blinding the first time a particular X is used,
     * but remember it to use blinding next time.
     */
    ret = te_bn_cmp_bn((te_bn_t *)X, pX, &result);
    PK_CHECK_RET_GO;
    if (result != 0) {
        ret = te_bn_copy(pX, X);
        PK_CHECK_RET_GO;
        ret = te_bn_import_s32(Vi, 1);
        PK_CHECK_RET_GO;
        ret = te_bn_import_s32(Vf, 1);
        PK_CHECK_RET_GO;
        ret = TE_SUCCESS;
        goto finish;
    }

    /*
     * Ok, we need blinding. Can we re-use existing values?
     * If yes, just update them by squaring them.
     */
    ret = te_bn_cmp_s32(Vi, 1, &result);
    PK_CHECK_RET_GO;
    if (result != 0) {
        ret = te_bn_square_mod(Vi, Vi, P);
        PK_CHECK_RET_GO;
        ret = te_bn_square_mod(Vf, Vf, P);
        PK_CHECK_RET_GO;
        ret = TE_SUCCESS;
        goto finish;
    }

    /*
     * We need to generate blinding values from scratch
     */
    p_size = te_bn_bytelen((te_bn_t *)P);
    if (p_size <= 0) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        goto finish;
    }

    /* Vi = random( 2, P-1 ) */
    count = 0;
    do {
        ret = te_bn_import_random(Vi, p_size, 1, f_rng, p_rng);
        PK_CHECK_RET_GO;

        do {
            ret = te_bn_cmp_bn(Vi, (te_bn_t *)P, &result);
            PK_CHECK_RET_GO;
            if (result >= 0) {
                ret = te_bn_shift_r(Vi, Vi, 1);
                PK_CHECK_RET_GO;
            } else {
                break;
            }
        } while (true);

        if (count++ > 10) {
            ret = TE_ERROR_GEN_RANDOM;
            goto finish;
        }
        ret = te_bn_cmp_s32(Vi, 1, &result);
        PK_CHECK_RET_GO;
    } while (result <= 0);

    /* Vf = Vi^-X mod P */
    ret = te_bn_inv_mod(Vf, Vi, P);
    PK_CHECK_RET_GO;
    ret = te_bn_exp_mod(Vf, Vf, X, P);
    PK_CHECK_RET_GO;

finish:
    return ret;
}
#endif

static int te_dhm_compute_shared_core(const te_drv_handle hdl,
                                      const te_bn_t *P,
                                      const te_bn_t *G,
                                      const te_bn_t *X,
                                      const te_bn_t *GY,
                                      te_bn_t *pX,
                                      te_bn_t *Vi,
                                      te_bn_t *Vf,
                                      te_bn_t *K,
                                      int (*f_rng)(void *, uint8_t *, size_t),
                                      void *p_rng)
{
    int ret      = TE_SUCCESS;
    te_bn_t *GYb = NULL;

    (void)(G);
    te_pk_lock(hdl);

    ret = te_bn_alloc(hdl, 0, &GYb);
    PK_CHECK_RET_GO;

    ret = _dhm_check_range(hdl, GY, P);
    if (1 == ret) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        goto finish;
    }
    PK_CHECK_RET_GO;

#ifdef CFG_ACA_BLINDING_EN
    /* Blind peer's value */
    if (f_rng != NULL) {
        ret = _dhm_update_blinding(P, X, pX, Vi, Vf, f_rng, p_rng);
        PK_CHECK_RET_GO;
        ret = te_bn_mul_mod(GYb, GY, Vi, P);
        PK_CHECK_RET_GO;
    } else
#endif
    {
        (void)(pX);
        (void)(Vi);
        (void)(Vf);
        (void)(f_rng);
        (void)(p_rng);
        ret = te_bn_copy(GYb, GY);
        PK_CHECK_RET_GO;
    }

    /* Do modular exponentiation */
    ret = te_bn_exp_mod(K, GYb, X, P);
    PK_CHECK_RET_GO;

#ifdef CFG_ACA_BLINDING_EN
    /* Unblind secret value */
    if (f_rng != NULL) {
        ret = te_bn_mul_mod(K, K, Vf, P);
        PK_CHECK_RET_GO;
    }
#endif

    ret = TE_SUCCESS;
finish:
    te_bn_free(GYb);
    te_pk_unlock(hdl);
    return ret;
}
/*
 * Derive and export the shared secret (G^Y)^X mod P
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
                          void *p_rng)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(P && G && X && GY && K);
#ifdef CFG_ACA_BLINDING_EN
    if (f_rng) {
        PK_CHECK_PARAM(pX && Vi && Vf);
    }
#endif
    PK_CHECK_FUNC(te_bn_get_drv_handle((te_bn_t *)P, &hdl));

    return te_dhm_compute_shared_core(hdl, P, G, X, GY, pX, Vi, Vf, K, f_rng,
                                      p_rng);
}

static int _te_dhm_make_public_async_cb(te_dhm_request_t *req)
{
    return te_dhm_make_public_core(PK_REQUEST_GET_HDL(req),
                                   req->make_public_args.P,
                                   req->make_public_args.G,
                                   req->make_public_args.x_size,
                                   req->make_public_args.X,
                                   req->make_public_args.GX,
                                   req->make_public_args.f_rng,
                                   req->make_public_args.p_rng);
}

int te_dhm_make_public_async(te_dhm_request_t *req)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->make_public_args.P);
    PK_CHECK_PARAM(req->make_public_args.G);
    PK_CHECK_PARAM(req->make_public_args.x_size >= 1);
    PK_CHECK_PARAM(req->make_public_args.X);
    PK_CHECK_PARAM(req->make_public_args.GX);
    PK_CHECK_PARAM(req->make_public_args.f_rng);

    PK_CHECK_FUNC(
        te_bn_get_drv_handle((te_bn_t *)(req->make_public_args.P), &hdl));

    PK_REQUEST_INIT_DATA(req, _te_dhm_make_public_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_dhm_compute_shared_async_cb(te_dhm_request_t *req)
{
    return te_dhm_compute_shared_core(PK_REQUEST_GET_HDL(req),
                                      req->compute_shared_args.P,
                                      req->compute_shared_args.G,
                                      req->compute_shared_args.X,
                                      req->compute_shared_args.GY,
                                      req->compute_shared_args.pX,
                                      req->compute_shared_args.Vi,
                                      req->compute_shared_args.Vf,
                                      req->compute_shared_args.K,
                                      req->compute_shared_args.f_rng,
                                      req->compute_shared_args.p_rng);
}

int te_dhm_compute_shared_async(te_dhm_request_t *req)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->compute_shared_args.P);
    PK_CHECK_PARAM(req->compute_shared_args.G);
    PK_CHECK_PARAM(req->compute_shared_args.X);
    PK_CHECK_PARAM(req->compute_shared_args.GY);
    PK_CHECK_PARAM(req->compute_shared_args.K);

    if (req->compute_shared_args.f_rng) {
        PK_CHECK_PARAM(req->compute_shared_args.pX);
        PK_CHECK_PARAM(req->compute_shared_args.Vi);
        PK_CHECK_PARAM(req->compute_shared_args.Vf);
    }

    PK_CHECK_FUNC(
        te_bn_get_drv_handle((te_bn_t *)(req->compute_shared_args.P), &hdl));

    PK_REQUEST_INIT_DATA(req, _te_dhm_compute_shared_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}
