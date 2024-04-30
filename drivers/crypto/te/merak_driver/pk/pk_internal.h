//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_PK_INTERNAL_H__
#define __TRUSTENGINE_PK_INTERNAL_H__

#include "driver/te_drv.h"
#include "driver/te_drv_aca.h"
#include "te_bn.h"
#include "te_ecp.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define ACA_PK_DEBUG 1

#if ACA_PK_DEBUG
#define ACA_PK_DBG_ERR(__fmt__, ...) OSAL_LOG_ERR(__fmt__, ##__VA_ARGS__)
#define ACA_PK_DBG_LOG(__fmt__, ...) OSAL_LOG_ERR(__fmt__, ##__VA_ARGS__)
#else
#define ACA_PK_DBG_ERR(__fmt__, ...)
#define ACA_PK_DBG_LOG(__fmt__, ...)
#endif

#define TE_ECP_DP_SECP192R1_ENABLED
#define TE_ECP_DP_SECP224R1_ENABLED
#define TE_ECP_DP_SECP256R1_ENABLED
#define TE_ECP_DP_SECP384R1_ENABLED
#define TE_ECP_DP_SECP521R1_ENABLED
#define TE_ECP_DP_SECP192K1_ENABLED
#define TE_ECP_DP_SECP224K1_ENABLED
#define TE_ECP_DP_SECP256K1_ENABLED
#define TE_ECP_DP_BP256R1_ENABLED
#define TE_ECP_DP_BP384R1_ENABLED
#define TE_ECP_DP_BP512R1_ENABLED
#define TE_ECP_DP_SM2P256V1_ENABLED

/**
 * TODO: How to create a shared header file between TE ACA driver and ACA
 * wrapper?
 */

#define PK_CHECK_FUNC(__function_call__, ...)                                  \
    do {                                                                       \
        ret = __function_call__;                                               \
        if (0 != ret) {                                                        \
            ACA_PK_DBG_ERR("PK Check Function " #__function_call__             \
                           " failed! ret: 0x%x func: %s line: %d\n",           \
                           ret,                                                \
                           __func__,                                           \
                           __LINE__);                                          \
            __VA_ARGS__                                                        \
            return ret;                                                        \
        }                                                                      \
    } while (0)

#define PK_CHECK_PARAM(__true_condition__)                                     \
    do {                                                                       \
        if (!(__true_condition__)) {                                           \
            ACA_PK_DBG_ERR("PK Check Parameter " #__true_condition__           \
                           " failed! func: %s line: %d\n",                     \
                           __func__,                                           \
                           __LINE__);                                          \
            return TE_ERROR_BAD_PARAMS;                                        \
        }                                                                      \
    } while (0)

#define PK_CHECK_COND_RETURN(__true_condition__, __ret_code__, ...)            \
    do {                                                                       \
        if (!(__true_condition__)) {                                           \
            ACA_PK_DBG_ERR("PK Check Condition " #__true_condition__           \
                           " failed! return: 0x%x func: %s line: %d\n",        \
                           __ret_code__,                                       \
                           __func__,                                           \
                           __LINE__);                                          \
            __VA_ARGS__                                                        \
            return (__ret_code__);                                             \
        }                                                                      \
    } while (0)

#define PK_CHECK_RET_GO                                                        \
    do {                                                                       \
        if ((TE_SUCCESS) != (ret)) {                                           \
            ACA_PK_DBG_ERR(                                                    \
                "PK Check Ret failed! ret: 0x%x func: %s line: %d\n",          \
                ret,                                                           \
                __func__,                                                      \
                __LINE__);                                                     \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

#define PK_CHECK_COND_GO(__true_condition__, __ret_code__)                     \
    do {                                                                       \
        if (!(__true_condition__)) {                                           \
            ACA_PK_DBG_ERR("PK Check Condition " #__true_condition__           \
                           " failed! func: %s line: %d\n",                     \
                           __func__,                                           \
                           __LINE__);                                          \
            ret = (__ret_code__);                                              \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

typedef int (*te_pk_async_cb_func_t)(void *args);

#define PK_REQUEST_INIT_DATA(__req__, __func_ptr__, __hdl_ptr__)               \
    do {                                                                       \
        __req__->internal_data[2] = (void *)(__func_ptr__);                    \
        __req__->internal_data[3] = (void *)(__hdl_ptr__);                     \
    } while (0)

#define PK_REQUEST_GET_HDL(__req__)                                            \
    ((const te_drv_handle)(__req__->internal_data[3]))

#if 0
#define PK_DEF_ASYNC_REQ_TYPE(__name__, __args__)                              \
    typedef struct _te_##__name__##_async_req_t {                              \
        sqlist_t node;                                                         \
        te_async_request_t base;                                               \
        te_pk_async_cb_func_t func;                                            \
        int32_t arg_num;                                                       \
        struct _args_t {                                                       \
            __args__                                                           \
        } args;                                                                \
    } te_##__name__##_async_req_t;

PK_DEF_ASYNC_REQ_TYPE(dhm_make_public, const te_drv_handle hdl;
                      const te_bn_t *P;
                      const te_bn_t *G;
                      size_t x_size;
                      te_bn_t * X;
                      te_bn_t * GX;
                      int (*f_rng)(void *, uint8_t *, size_t);
                      void *p_rng;);
#endif

static inline void aca_zeroize(void *buf, size_t size)
{
    memset(buf, 0, size);
}

extern void te_pk_lock(const te_drv_handle hdl);
extern void te_pk_unlock(const te_drv_handle hdl);
extern int te_pk_submit_req(void *req);

extern int te_ecp_mul_core(const te_drv_handle hdl,
                           const te_ecp_group_t *grp,
                           te_ecp_point_t *R,
                           const te_bn_t *m,
                           const te_ecp_point_t *P,
                           int (*f_rng)(void *, uint8_t *, size_t),
                           void *p_rng,
                           bool is_lock);
extern int te_ecp_muladd_core(const te_drv_handle hdl,
                              const te_ecp_group_t *grp,
                              te_ecp_point_t *R,
                              const te_bn_t *m,
                              const te_ecp_point_t *P,
                              const te_bn_t *n,
                              const te_ecp_point_t *Q,
                              bool with_lock);
extern int te_ecp_gen_privkey_core(const te_drv_handle hdl,
                                   const te_ecp_group_t *grp,
                                   te_bn_t *d,
                                   int (*f_rng)(void *, uint8_t *, size_t),
                                   void *p_rng,
                                   bool with_lock);

extern int te_ecp_check_pubkey_core(const te_drv_handle hdl,
                                    const te_ecp_group_t *grp,
                                    const te_ecp_point_t *pt,
                                    bool with_lock);
extern int te_ecp_check_privkey_core(const te_drv_handle hdl,
                                     const te_ecp_group_t *grp,
                                     const te_bn_t *d,
                                     bool with_lock);
#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_PK_INTERNAL_H__ */
