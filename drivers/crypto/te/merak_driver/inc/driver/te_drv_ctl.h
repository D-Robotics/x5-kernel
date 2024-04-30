//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DRV_CTL_H__
#define __TRUSTENGINE_DRV_CTL_H__

#include "te_drv.h"
#include "hwa/te_hwa_ctl.h"
#include "hwa/te_hwa_dbgctl.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_hwa_ctl;
struct te_hwa_dma;
struct te_hwa_dbgctl;

/**
 * CTL driver magic number
 */
#define CTL_DRV_MAGIC   0x644c7443U /**< "CtLd" */

/**
 * Trust engine CTL driver structure
 */
typedef struct te_ctl_drv {
    te_crypt_drv_t base;            /**< base driver */
    uint32_t magic;                 /**< CTL driver magic */
    te_ctx_handle hctx;             /**< CTL context handler */
} te_ctl_drv_t;

/**
 * \brief           This function initializes the supplied CTL driver instance
 *                  \p drv by binding it to the given CTL \p ctl, AXI CTL
 *                  \p dma, and debug CTL \p dbg.
 *
 *                  This function is restricted to host0 driver using only.
 *
 *                  A CTL context instance will be created with its crypto
 *                  context linked to \p drv->hctx on success.
 *
 * \param[in] drv   The CTL driver instance.
 * \param[in] ctl   The CTL HWA instance.
 * \param[in] dma   The AXI DMA HWA instance.
 * \param[in] dbg   The Debug CTL HWA instance.
 * \param[in] name  The CTL driver name. Or NULL to ignore.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_drv_init( te_ctl_drv_t *drv,
                     struct te_hwa_ctl *ctl,
                     struct te_hwa_dma *dma,
                     struct te_hwa_dbgctl *dbg,
                     const char* name );

/**
 * \brief           This function withdraws the supplied CTL driver instance
 *                  \p drv. Host0 driver using only.
 * \param[in] drv   The CTL driver instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_drv_exit( te_ctl_drv_t *drv );

/**
 * \brief           This function enables clock for the specified module(s)
 *                  \p mod. One or more modules could be passed to the CTL
 *                  driver via the bitwise \p mod. Host0 driver using only.
 * \param[in] h     The CTL context handler.
 * \param[in] mod   The module(s).
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_clk_enable( te_ctx_handle h, te_module_t mod );

/**
 * \brief           This function disables clock for the specified module(s)
 *                  \p mod. One or more modules could be passed to the CTL
 *                  driver via the bitwise \p mod. Host0 driver using only.
 * \param[in] h     The CTL context handler.
 * \param[in] mod   The module(s).
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_clk_disable( te_ctx_handle h, te_module_t mod );

/**
 * \brief           This function performs software reset for the specified
 *                  module(s) \p mod. One or more modules could be passed to
 *                  the CTL driver via the bitwise \p mod.
 *                  Host0 driver using only.
 * \param[in] h     The CTL context handler.
 * \param[in] mod   The module(s).
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_reset( te_ctx_handle h, te_module_t mod );

/**
 * \brief           This function sets up the host arbitrator for the specified
 *                  module(s) \p mod. One or more modules could be passed to
 *                  the CTL driver via the bitwise \p mod.
 *                  This function supports modules of HASH, SCA, ACA, and TRNG.
 *                  Host0 driver using only.
 * \param[in] h     The CTL context handler.
 * \param[in] mod   The module(s).
 * \param[in] alg   The host arbitration algorithm.
 * \param[in] gran  The host arbitration granularity.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_set_arb( te_ctx_handle h, te_module_t mod,
                    te_arb_algo_t alg, te_arb_gran_t gran );

/**
 * \brief           This function sets up the \p n-th host using the provided
 *                  configuration \p conf.
 *                  Host0 driver using only.
 * \param[in] h     The CTL context handler.
 * \param[in] n     The host id starting from 0.
 * \param[in] conf  The host configuration.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_set_host( te_ctx_handle h, int n, const te_host_conf_t *conf );

/**
 * \brief           This function gets the configuration for the \p n-th host.
 *                  Host0 driver using only.
 * \param[in] h     The CTL context handler.
 * \param[in] n     The host id starting from 0.
 * \param[in] conf  The host configuration on success.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_get_host( te_ctx_handle h, int n, te_host_conf_t *conf );

/**
 * \brief           This function sets the global sw_init_done flag (after the
 *                  host0 driver finishes initializing the trust engine). That
 *                  flag is used as an indicator for driver software of other
 *                  hosts in the system. Host0 driver using only.
 * \param[in] h     The CTL context handler.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_sw_init_done( te_ctx_handle h );

/**
 * \brief           This function locks off the SCA and HASH context pool
 *                  configuration for all hosts in the system.
 *                  Only trust engine reset can clear the lock status.
 *                  Host0 driver using only.
 * \param[in] h     The CTL context handler.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_lock_ctx_pool( te_ctx_handle h );

/**
 * \brief           This function sets up the global debug control for the trust
 *                  engine, and optionally locks off the debug control when \p
 *                  lock flag is set.
 *                  Only trust engine reset can clear the lock status.
 *                  Host0 driver using only.
 * \param[in] h     The CTL context handler.
 * \param[in] ctl   The debug control.
 * \param[in] lock  The lock flag. True to lock.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_set_debug( te_ctx_handle h, const uint32_t ctl, bool lock );

/**
 * \brief           This function gets the global debug control state for the
 *                  trust engine.
 *                  Host0 driver using only.
 * \param[in] h     The CTL context handler.
 * \param[in] ctl   The debug control on success.
 * \param[in] lock  The lock status on success.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_ctl_get_debug( te_ctx_handle h, uint32_t *ctl, uint32_t *lock );

//TODO: declare AXI specific interfaces

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_DRV_CTL_H__ */
