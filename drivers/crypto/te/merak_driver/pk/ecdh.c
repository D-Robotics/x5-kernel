//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include "pk_internal.h"
#include "te_ecdh.h"

/*
 * Generate public key (restartable version)
 *
 * Note: this internal function relies on its caller preserving the value of
 * the output parameter 'd' across continuation calls. This would not be
 * acceptable for a public function but is OK here as we control call sites.
 */

static int
te_ecdh_gen_public_core(const te_ecp_group_t *grp,
                        te_bn_t *d,
                        te_ecp_point_t *Q,
                        int (*f_rng)(void *, unsigned char *, size_t),
                        void *p_rng)
{
    int ret           = TE_SUCCESS;
    int result        = 0;
    te_drv_handle hdl = NULL;

    PK_CHECK_FUNC(te_bn_get_drv_handle(grp->P, &hdl));

    /* Check if d is 0 */
    ret = te_bn_cmp_s32(d, 0, &result);
    PK_CHECK_RET_GO;
    if (result == 0) {
        /* d is 0, generate */
        goto __gen_priv;
    } else {
        goto __check_priv;
    }

__gen_priv:
    PK_CHECK_PARAM(f_rng);
    ret = te_ecp_gen_privkey_core(hdl, grp, d, f_rng, p_rng, true);
    PK_CHECK_RET_GO;
    goto __gen_public;

__check_priv:
    ret = te_ecp_check_privkey_core(
        (const te_drv_handle)hdl, grp, (const te_bn_t *)d, true);
    PK_CHECK_RET_GO;

__gen_public:
    ret = te_ecp_mul_core(
        hdl, grp, Q, d, (const te_ecp_point_t *)(&(grp->G)), NULL, NULL, true);
    PK_CHECK_RET_GO;

finish:
    return ret;
}

int te_ecdh_gen_public(const te_ecp_group_t *grp,
                       te_bn_t *d,
                       te_ecp_point_t *Q,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng)
{
    PK_CHECK_PARAM(grp && d && Q);
    return te_ecdh_gen_public_core(grp, d, Q, f_rng, p_rng);
}

static int te_ecdh_compute_shared_core(const te_drv_handle hdl,
                                       const te_ecp_group_t *grp,
                                       const te_bn_t *d,
                                       const te_ecp_point_t *other_Q,
                                       te_bn_t *K,
                                       int (*f_rng)(void *, uint8_t *, size_t),
                                       void *p_rng)
{
    int ret          = TE_SUCCESS;
    te_ecp_point_t P = {0};

    te_pk_lock(hdl);

    ret = te_ecp_point_init(hdl, &P);
    PK_CHECK_RET_GO;
    ret = te_ecp_mul_core(hdl, grp, &P, d, other_Q, f_rng, p_rng, false);
    PK_CHECK_RET_GO;

    ret = te_ecp_is_zero(&P);
    PK_CHECK_COND_GO(ret >= 0, ret);
    if (1 == ret) {
        ret = TE_ERROR_BAD_INPUT_DATA;
        goto finish;
    }

    ret = te_bn_copy(K, P.X);
    PK_CHECK_RET_GO;

finish:
    te_ecp_point_free(&P);
    te_pk_unlock(hdl);
    return ret;
}

int te_ecdh_compute_shared(const te_ecp_group_t *grp,
                           const te_bn_t *d,
                           const te_ecp_point_t *other_Q,
                           te_bn_t *K,
                           int (*f_rng)(void *, uint8_t *, size_t),
                           void *p_rng)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(grp && d && other_Q && K);
    PK_CHECK_FUNC(te_bn_get_drv_handle(grp->P, &hdl));
    return te_ecdh_compute_shared_core(hdl, grp, d, other_Q, K, f_rng, p_rng);
}

static int _te_ecdh_gen_public_async_cb(te_ecdh_request_t *req)
{
    PK_CHECK_PARAM((req->gen_public_args.grp) && (req->gen_public_args.d) &&
                   (req->gen_public_args.Q));
    return te_ecdh_gen_public_core(req->gen_public_args.grp,
                                   req->gen_public_args.d,
                                   req->gen_public_args.Q,
                                   req->gen_public_args.f_rng,
                                   req->gen_public_args.p_rng);
}

int te_ecdh_gen_public_async(te_ecdh_request_t *req)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->gen_public_args.grp && req->gen_public_args.d &&
                   req->gen_public_args.Q);

    PK_CHECK_FUNC(te_bn_get_drv_handle(req->gen_public_args.grp->P, &hdl));

    PK_REQUEST_INIT_DATA(req, _te_ecdh_gen_public_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}

static int _te_ecdh_compute_shared_async_cb(te_ecdh_request_t *req)
{
    return te_ecdh_compute_shared_core(PK_REQUEST_GET_HDL(req),
                                       req->compute_shared_args.grp,
                                       req->compute_shared_args.d,
                                       req->compute_shared_args.other_Q,
                                       req->compute_shared_args.K,
                                       req->compute_shared_args.f_rng,
                                       req->compute_shared_args.p_rng);
}

int te_ecdh_compute_shared_async(te_ecdh_request_t *req)
{
    int ret           = TE_SUCCESS;
    te_drv_handle hdl = NULL;

    PK_CHECK_PARAM(req && req->base.completion);
    PK_CHECK_PARAM(req->compute_shared_args.grp);
    PK_CHECK_PARAM(req->compute_shared_args.d);
    PK_CHECK_PARAM(req->compute_shared_args.other_Q);
    PK_CHECK_PARAM(req->compute_shared_args.K);

    PK_CHECK_FUNC(te_bn_get_drv_handle(
        (te_bn_t *)(req->compute_shared_args.grp->P), &hdl));

    PK_REQUEST_INIT_DATA(req, _te_ecdh_compute_shared_async_cb, hdl);
    return te_pk_submit_req((void *)req);
}