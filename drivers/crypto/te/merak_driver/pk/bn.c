//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include "pk_internal.h"
#include "driver/te_drv_aca.h"

int te_bn_alloc(const te_drv_handle hdl, int32_t bytelen_hint, te_bn_t **bn)
{
    int ret                   = TE_SUCCESS;
    const te_crypt_drv_t *drv = NULL;

    PK_CHECK_PARAM(hdl);
    PK_CHECK_PARAM(bn);

    drv = (te_crypt_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_ACA);
    PK_CHECK_COND_GO(drv, TE_ERROR_BAD_PARAMS);

    ret = te_aca_bn_alloc(drv, bytelen_hint, (te_aca_bn_t **)(bn));
    PK_CHECK_RET_GO;

    ret = te_aca_bn_set_usr_data((te_aca_bn_t *)(*bn), (void *)hdl);
    PK_CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    if (TE_SUCCESS != ret) {
        if (drv) {
            (void)te_drv_put(hdl, TE_DRV_TYPE_ACA);
        }
    }
    return ret;
}

void te_bn_free(te_bn_t *bn)
{
    int ret   = TE_SUCCESS;
    void *hdl = NULL;

    if (!bn) {
        return;
    }

    ret = te_aca_bn_get_usr_data((const te_aca_bn_t *)bn, (void **)(&(hdl)));
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("Invalid BN! NO user data!\n");
        return;
    }

    ret = te_drv_put((te_drv_handle)hdl, TE_DRV_TYPE_ACA);
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("Put ACA driver failed!\n");
        return;
    }

    te_aca_bn_free((te_aca_bn_t *)bn);

    return;
}

int te_bn_get_drv_handle(te_bn_t *bn, te_drv_handle *hdl)
{
    int ret = TE_SUCCESS;

    PK_CHECK_PARAM(bn);
    PK_CHECK_PARAM(hdl);

    ret = te_aca_bn_get_usr_data((const te_aca_bn_t *)bn, (void **)hdl);
    PK_CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

#define DEFINE_BN_FUNC_ARG1(__func_type__, __func_name__, _arg1, __arg1)       \
    __func_type__ te_bn_##__func_name__(_arg1)                                 \
    {                                                                          \
        return te_aca_bn_##__func_name__(__arg1);                              \
    }
#define DEFINE_BN_FUNC_ARG2(                                                   \
    __func_type__, __func_name__, _arg1, _arg2, __arg1, __arg2)                \
    __func_type__ te_bn_##__func_name__(_arg1, _arg2)                          \
    {                                                                          \
        return te_aca_bn_##__func_name__(__arg1, __arg2);                      \
    }
#define DEFINE_BN_FUNC_ARG3(                                                   \
    __func_type__, __func_name__, _arg1, _arg2, _arg3, __arg1, __arg2, __arg3) \
    __func_type__ te_bn_##__func_name__(_arg1, _arg2, _arg3)                   \
    {                                                                          \
        return te_aca_bn_##__func_name__(__arg1, __arg2, __arg3);              \
    }
#define DEFINE_BN_FUNC_ARG4(__func_type__,                                     \
                            __func_name__,                                     \
                            _arg1,                                             \
                            _arg2,                                             \
                            _arg3,                                             \
                            _arg4,                                             \
                            __arg1,                                            \
                            __arg2,                                            \
                            __arg3,                                            \
                            __arg4)                                            \
    __func_type__ te_bn_##__func_name__(_arg1, _arg2, _arg3, _arg4)            \
    {                                                                          \
        return te_aca_bn_##__func_name__(__arg1, __arg2, __arg3, __arg4);      \
    }
#define DEFINE_BN_FUNC_ARG5(__func_type__,                                     \
                            __func_name__,                                     \
                            _arg1,                                             \
                            _arg2,                                             \
                            _arg3,                                             \
                            _arg4,                                             \
                            _arg5,                                             \
                            __arg1,                                            \
                            __arg2,                                            \
                            __arg3,                                            \
                            __arg4,                                            \
                            __arg5)                                            \
    __func_type__ te_bn_##__func_name__(_arg1, _arg2, _arg3, _arg4, _arg5)     \
    {                                                                          \
        return te_aca_bn_##__func_name__(                                      \
            __arg1, __arg2, __arg3, __arg4, __arg5);                           \
    }

#define _ACA_BN(__bn__) ((te_aca_bn_t *)(__bn__))
#define _CONST_ACA_BN(__bn__) ((const te_aca_bn_t *)(__bn__))

// DEFINE_BN_FUNC_ARG1(void, free, te_bn_t *bn, _ACA_BN(bn))
DEFINE_BN_FUNC_ARG2(
    int, set_sign, te_bn_t *bn, int32_t sign, _ACA_BN(bn), sign);
DEFINE_BN_FUNC_ARG2(
    int, get_sign, te_bn_t *bn, int32_t *sign, _ACA_BN(bn), sign);
DEFINE_BN_FUNC_ARG2(
    int, grow, te_bn_t *bn, size_t bytelen, _ACA_BN(bn), bytelen)
DEFINE_BN_FUNC_ARG2(
    int, shrink, te_bn_t *bn, size_t bytelen, _ACA_BN(bn), bytelen)
DEFINE_BN_FUNC_ARG2(int,
                    copy,
                    te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bna),
                    _CONST_ACA_BN(bnb))
DEFINE_BN_FUNC_ARG2(
    int, swap, te_bn_t *bna, te_bn_t *bnb, _ACA_BN(bna), _ACA_BN(bnb))

DEFINE_BN_FUNC_ARG4(int,
                    import,
                    te_bn_t *bn,
                    const uint8_t *buf,
                    size_t size,
                    int32_t sign,
                    _ACA_BN(bn),
                    buf,
                    size,
                    sign);

DEFINE_BN_FUNC_ARG3(int,
                    export,
                    te_bn_t *bn,
                    uint8_t *buf,
                    size_t size,
                    _ACA_BN(bn),
                    buf,
                    size);

DEFINE_BN_FUNC_ARG2(int, import_s32, te_bn_t *bn, int32_t z, _ACA_BN(bn), z);
DEFINE_BN_FUNC_ARG2(int, import_u32, te_bn_t *bn, uint32_t uz, _ACA_BN(bn), uz);

DEFINE_BN_FUNC_ARG2(int, export_s32, te_bn_t *bn, int32_t *z, _ACA_BN(bn), z);
DEFINE_BN_FUNC_ARG2(
    int, export_u32, te_bn_t *bn, uint32_t *uz, _ACA_BN(bn), uz);

DEFINE_BN_FUNC_ARG5(int,
                    import_random,
                    te_bn_t *bn,
                    size_t size,
                    int32_t sign,
                    int (*f_rng)(void *, uint8_t *, size_t),
                    void *p_rng,
                    _ACA_BN(bn),
                    size,
                    sign,
                    f_rng,
                    p_rng);

DEFINE_BN_FUNC_ARG3(int,
                    cmp_bn,
                    te_bn_t *bna,
                    te_bn_t *bnb,
                    int *result,
                    _ACA_BN(bna),
                    _ACA_BN(bnb),
                    result);

DEFINE_BN_FUNC_ARG3(int,
                    cmp_s32,
                    te_bn_t *bna,
                    int32_t b,
                    int *result,
                    _ACA_BN(bna),
                    b,
                    result);

DEFINE_BN_FUNC_ARG3(int,
                    shift_l,
                    te_bn_t *bna,
                    const te_bn_t *bnb,
                    int count,
                    _ACA_BN(bna),
                    _CONST_ACA_BN(bnb),
                    count);

DEFINE_BN_FUNC_ARG3(int,
                    shift_r,
                    te_bn_t *bna,
                    const te_bn_t *bnb,
                    int count,
                    _ACA_BN(bna),
                    _CONST_ACA_BN(bnb),
                    count);

DEFINE_BN_FUNC_ARG2(int, get_bit, te_bn_t *bn, int pos, _ACA_BN(bn), pos);

DEFINE_BN_FUNC_ARG3(
    int, set_bit, te_bn_t *bn, int pos, uint8_t val, _ACA_BN(bn), pos, val);

DEFINE_BN_FUNC_ARG1(int, bitlen, te_bn_t *bn, _ACA_BN(bn));

DEFINE_BN_FUNC_ARG1(int, 0bits_before_lsb1, te_bn_t *bn, _ACA_BN(bn));

DEFINE_BN_FUNC_ARG1(int, bytelen, te_bn_t *bn, _ACA_BN(bn));

DEFINE_BN_FUNC_ARG2(int,
                    abs,
                    te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG3(int,
                    and,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG3(int,
                    or
                    ,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG3(int,
                    xor,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG3(int,
                    add_bn,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG3(int,
                    sub_bn,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG3(int,
                    add_s32,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    int32_t b,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    b);
DEFINE_BN_FUNC_ARG3(int,
                    sub_s32,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    int32_t b,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    b);

DEFINE_BN_FUNC_ARG2(int,
                    neg_bn,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna));

DEFINE_BN_FUNC_ARG3(int,
                    add_abs,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG3(int,
                    sub_abs,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG3(int,
                    mul_bn,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG3(int,
                    mul_s32,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const int32_t b,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    b);

DEFINE_BN_FUNC_ARG2(int,
                    square,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna));

DEFINE_BN_FUNC_ARG4(int,
                    div_bn,
                    te_bn_t *bnq,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bnq),
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG3(int,
                    mod_bn,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnn,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnn));

DEFINE_BN_FUNC_ARG3(int,
                    mod_u32,
                    uint32_t *r,
                    const te_bn_t *bna,
                    uint32_t n,
                    r,
                    _CONST_ACA_BN(bna),
                    n);

DEFINE_BN_FUNC_ARG4(int,
                    add_mod,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    const te_bn_t *bnn,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb),
                    _CONST_ACA_BN(bnn));

DEFINE_BN_FUNC_ARG4(int,
                    sub_mod,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    const te_bn_t *bnn,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb),
                    _CONST_ACA_BN(bnn));

DEFINE_BN_FUNC_ARG4(int,
                    mul_mod,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    const te_bn_t *bnn,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb),
                    _CONST_ACA_BN(bnn));

DEFINE_BN_FUNC_ARG3(int,
                    square_mod,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnn,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnn));

DEFINE_BN_FUNC_ARG3(int,
                    inv_mod,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnn,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnn));

DEFINE_BN_FUNC_ARG4(int,
                    exp_mod,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bne,
                    const te_bn_t *bnn,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bne),
                    _CONST_ACA_BN(bnn));

DEFINE_BN_FUNC_ARG3(int,
                    gcd,
                    te_bn_t *bnr,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    _ACA_BN(bnr),
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG2(int,
                    relative_prime,
                    const te_bn_t *bna,
                    const te_bn_t *bnb,
                    _CONST_ACA_BN(bna),
                    _CONST_ACA_BN(bnb));

DEFINE_BN_FUNC_ARG4(int,
                    is_probale_prime,
                    const te_bn_t *bn,
                    uint32_t round,
                    int (*f_rng)(void *, uint8_t *, size_t),
                    void *p_rng,
                    _CONST_ACA_BN(bn),
                    round,
                    f_rng,
                    p_rng);

int te_bn_gen_prime(te_bn_t *bn,
                    bool is_low_err,
                    bool is_dh_prim,
                    size_t nbits,
                    int (*f_rng)(void *, uint8_t *, size_t),
                    void *p_rng)
{
    return te_aca_bn_gen_prime(
        (te_aca_bn_t *)bn, is_low_err, is_dh_prim, nbits, f_rng, p_rng);
}

int te_bn_ecp_mul(const te_bn_t *P,
                  const te_bn_t *A,
                  const te_bn_t *X,
                  const te_bn_t *Y,
                  const te_bn_t *Z,
                  const te_bn_t *k,
                  te_bn_t *Rx,
                  te_bn_t *Ry,
                  te_bn_t *Rz)
{
    return te_aca_bn_ecp_mul((const te_aca_bn_t *)P,
                             (const te_aca_bn_t *)A,
                             (const te_aca_bn_t *)X,
                             (const te_aca_bn_t *)Y,
                             (const te_aca_bn_t *)Z,
                             (const te_aca_bn_t *)k,
                             (te_aca_bn_t *)Rx,
                             (te_aca_bn_t *)Ry,
                             (te_aca_bn_t *)Rz);
}

int te_bn_ecp_add(const te_bn_t *P,
                  const te_bn_t *X1,
                  const te_bn_t *Y1,
                  const te_bn_t *Z1,
                  const te_bn_t *X2,
                  const te_bn_t *Y2,
                  const te_bn_t *Z2,
                  te_bn_t *Rx,
                  te_bn_t *Ry,
                  te_bn_t *Rz)
{
    return te_aca_bn_ecp_add((const te_aca_bn_t *)P,
                             (const te_aca_bn_t *)X1,
                             (const te_aca_bn_t *)Y1,
                             (const te_aca_bn_t *)Z1,
                             (const te_aca_bn_t *)X2,
                             (const te_aca_bn_t *)Y2,
                             (const te_aca_bn_t *)Z2,
                             (te_aca_bn_t *)Rx,
                             (te_aca_bn_t *)Ry,
                             (te_aca_bn_t *)Rz);
}

int te_bn_ecp_jacobian_to_affine(const te_bn_t *P,
                                 const te_bn_t *X,
                                 const te_bn_t *Y,
                                 const te_bn_t *Z,
                                 te_bn_t *Rx,
                                 te_bn_t *Ry,
                                 te_bn_t *Rz)
{
    return te_aca_bn_ecp_jacobian_to_affine((const te_aca_bn_t *)P,
                                            (const te_aca_bn_t *)X,
                                            (const te_aca_bn_t *)Y,
                                            (const te_aca_bn_t *)Z,
                                            (te_aca_bn_t *)Rx,
                                            (te_aca_bn_t *)Ry,
                                            (te_aca_bn_t *)Rz);
}

void te_bn_dump(const char *name, const te_bn_t *bn)
{
    return te_aca_bn_dump(name, (const te_aca_bn_t *)bn);
}