//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DRV_TRNG_H__
#define __TRUSTENGINE_DRV_TRNG_H__

#include "te_drv.h"
#include "hwa/te_hwa_trng_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define TE_TRNG_ADAP_TEST_ERR_MASK      (0x00U)
#define TE_TRNG_REP_TEST_ERR_MASK       (0x01U)
#define TE_TRNG_CRNG_ERR_MASK           (0x02U)
#define TE_TRNG_VN_ERR_MASK             (0x03U)
#define TE_TRNG_AUTOCORR_TEST_ERR_MASK  (0x04U)

struct te_hwa_trng;
struct te_hwa_trngctl;
struct trng_pm_ctx;

/**
 * TRNG request structure
 */
typedef struct te_trng_request {
    bool b_conf;                    /**< whether need to reconfig trng */
    te_trng_conf_t conf;            /**< configuration */
    void *buf;                      /**< buf for a member */
    size_t size;                    /**< member size in byte */
    size_t nmemb;                   /**< number of members */
    bool bskip;                     /**< skip data on errors */
    void (*on_data)(void *data, size_t len);
    void (*on_error)(int err);      /**< report errors */
} te_trng_request_t;

/**
 * TRNG driver magic number
 */
#define TRNG_DRV_MAGIC  0x64474e52U /**< "RNGd" */

/**
 * Trust engine TRNG driver structure
 */
typedef struct te_trng_drv {
    te_crypt_drv_t base;            /**< base driver */
    uint32_t magic;                 /**< TRNG driver magic */
    te_ctx_handle hctx;             /**< TRNG context handler */
    struct trng_pm_ctx *pm;         /**< TRNG PM context */

    /**
     * CTL specific ops
     */
    int (*dump)(te_ctx_handle h, te_trng_request_t *req);
} te_trng_drv_t;

/**
 * \brief           This function initializes the supplied TRNG driver instance
 *                  \p drv by binding it to the given RNP \p rnp, and TRNG CTL
 *                  \p ctl if exists.
 *
 *                  Note the \p ctl is required by the TRNG driver of the host0
 *                  only. Shall set it to NULL elsewhere. This function could
 *                  have sanity check logic around \p ctl.
 *
 *                  A TRNG context instance will be created with its crypto
 *                  context linked to \p drv->ctx on success.
 *
 * \param[in] drv   The TRNG driver instance.
 * \param[in] rnp   The RNP HWA instance.
 * \param[in] ctl   The TRNG CTL HWA instance.
 * \param[in] name  The TRNG driver name. Or NULL to ignore.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_trng_drv_init( te_trng_drv_t *drv,
                      const struct te_hwa_trng *rnp,
                      const struct te_hwa_trngctl *ctl,
                      const char* name );

/**
 * \brief           This function withdraws the supplied TRNG driver instance
 *                  \p drv.
 * \param[in] drv   The TRNG driver instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_trng_drv_exit( te_trng_drv_t *drv );

/**
 * \brief           This function reads the specified length \p len of random
 *                  data from the random number pool and writes it to \p buf.
 * \param[in] drv   The TRNG driver instance.
 * \param[out] buf  The buffer holding the desired random data.
 * \param[in] len   The length of desired random data.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_trng_read( te_trng_drv_t *drv, uint8_t *buf, size_t len );

/**
 * \brief           This function dumps the specified length of random data
 *                  from the TRNG using the given settings.
 *
 *                  This function is mainly used for TRNG calibration and is
 *                  effective only in the TRNG driver of the host0.
 *
 * \param[in] drv   The TRNG driver instance.
 * \param[in] req   The TRNG request object.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static inline int te_trng_dump( te_trng_drv_t *drv,
                                te_trng_request_t *req )
{
    if (NULL == drv)
        return TE_ERROR_BAD_PARAMS;

    if (drv->dump != NULL) {
        return drv->dump(drv->hctx, req);
    } else {
        (void)req;
        return TE_ERROR_NOT_SUPPORTED;
    }
}

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_DRV_TRNG_H__ */
