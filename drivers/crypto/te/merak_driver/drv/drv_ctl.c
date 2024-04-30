//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <driver/te_drv_ctl.h>
#include <hwa/te_hwa_ctl.h>
#include <hwa/te_hwa_dma.h>
#include <hwa/te_hwa_dbgctl.h>
#include <hwa/te_hwa.h>
#include "drv_internal.h"

/**
 * Derive the CTL hwa pointer from the driver context
 */
#define GET_CTL_HWA(_ctx) __extension__({                 \
    te_hwa_ctl_t *_ctl = NULL;                            \
    te_ctl_drv_t *_drv = (te_ctl_drv_t*)(_ctx)->base.drv; \
    TE_ASSERT(_drv != NULL);                              \
    _ctl = (te_hwa_ctl_t*)(_drv->base.hwa);               \
    TE_ASSERT(_ctl != NULL);                              \
    _ctl;                                                 \
})

/**
 * CTL pm context structure
 */
typedef struct ctl_pm_ctx {
    te_clk_stat_t clk_st;           /**< clock state */
    te_host_conf_t hst_conf[TE_MAX_HOST_NUM];     /**< host configs */
    te_top_conf_t top_conf;         /**< top config */
    uint32_t dbgctl;                /**< dbg_ctl */
    uint32_t dbglock;               /**< dbg_lock_en */
} ctl_pm_ctx_t;

/**
 * CTL context magic number
 */
#define CTL_CTX_MAGIC   0x434c5463U /**< "cTLC" */

/**
 * Trust engine CTL context structure
 * Single instance
 */
typedef struct ctl_drv_ctx {
    te_crypt_ctx_t base;            /**< base context */
    uint32_t magic;                 /**< CTL ctx magic */
    osal_mutex_t mut;               /**< exclusive lock */
    osal_atomic_t refcnt;           /**< reference count */
    te_hwa_dma_t *dma;              /**< AXI DMA HWA */
    te_hwa_dbgctl_t *dbg;           /**< DBG HWA */
    ctl_pm_ctx_t pm;                /**< CTL pm context */
} ctl_drv_ctx_t;

static int get_host_num( te_hwa_host_t *host )
{
    TE_ASSERT(host != NULL);
    if (NULL == host->stat.host_num) {
        return TE_ERROR_NOT_SUPPORTED;
    }
    return host->stat.host_num(&host->stat);
}

static int ctl_suspend( te_crypt_drv_t *crypt, te_suspend_state_t state )
{
    int rc = 0, nhst = 0, i = 0;
    te_ctl_drv_t *drv = (te_ctl_drv_t*)crypt;
    te_hwa_ctl_t *ctl = (te_hwa_ctl_t*)drv->base.hwa;
    ctl_pm_ctx_t *pm = &((ctl_drv_ctx_t*)drv->hctx)->pm;
    te_hwa_host_t *host = NULL;

    TE_ASSERT(ctl != NULL);
    TE_ASSERT(drv->hctx != NULL);
    host = ctl->base.host;
    TE_ASSERT(host != NULL);

    if (!host->stat.clock_state || !ctl->top_conf ||
        !host->stat.host_conf || !host->dbgctl ||
        !host->dbgctl->get_dbgctl || !host->dbgctl->get_locken) {
        return TE_ERROR_NOT_SUPPORTED;
    }

    if (TE_SUSPEND_STANDBY == state) {
        return TE_SUCCESS;
    }

    /* save clock state */
    rc = host->stat.clock_state(&host->stat, &pm->clk_st);
    if (rc != TE_SUCCESS) {
        return rc;
    }
    /* save host conf */
    nhst = get_host_num(host);
    if (nhst < 0) {
        return nhst;
    }
    for (i = 0; i < nhst; i++) {
        rc = host->stat.host_conf(&host->stat, i, &pm->hst_conf[i]);
        if (rc != TE_SUCCESS) {
            return rc;
        }
    }
    /* save top conf */
    rc = ctl->top_conf(ctl, &pm->top_conf);
    if (rc != TE_SUCCESS) {
        return rc;
    }
    /* save debug ctrl&lock_en */
    rc = host->dbgctl->get_dbgctl(host->dbgctl, &pm->dbgctl);
    if (rc != TE_SUCCESS) {
        return rc;
    }
    rc = host->dbgctl->get_locken(host->dbgctl, &pm->dbglock);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    return TE_SUCCESS;
}

static int ctl_resume( te_crypt_drv_t *crypt, te_suspend_state_t state )
{
    int rc = 0, nhst = 0, i = 0;
    te_ctl_drv_t *drv = (te_ctl_drv_t*)crypt;
    te_hwa_ctl_t *ctl = (te_hwa_ctl_t*)drv->base.hwa;
    ctl_pm_ctx_t *pm = &((ctl_drv_ctx_t*)drv->hctx)->pm;
    te_hwa_host_t *host = NULL;
    te_module_t mods = 0;

    TE_ASSERT(ctl != NULL);
    TE_ASSERT(drv->hctx != NULL);
    host = ctl->base.host;
    TE_ASSERT(host != NULL);

    if (TE_SUSPEND_STANDBY == state) {
        return TE_SUCCESS;
    }

    /* restore clock state */
    mods |= pm->clk_st.otp_en ? TE_MOD_OTP : 0;
    if (mods) {
        rc = ctl->clock_ctl(ctl, mods, true);
        if (rc != TE_SUCCESS) {
            return rc;
        }
    }
    /* restore host conf in prior to top conf */
    nhst = get_host_num(host);
    if (nhst < 0) {
        return nhst;
    }
    for (i = 0; i < nhst; i++) {
        rc = ctl->conf_host(ctl, i, &pm->hst_conf[i]);
        if (rc != TE_SUCCESS) {
            return rc;
        }
    }
    /* restore top conf */
    rc = ctl->conf_top(ctl, &pm->top_conf);
    if (rc != TE_SUCCESS) {
        return rc;
    }
    /* restore debug ctrl&lock_en */
    rc = host->dbgctl->set_dbgctl(host->dbgctl, pm->dbgctl);
    if (rc != TE_SUCCESS) {
        return rc;
    }
    rc = host->dbgctl->set_locken(host->dbgctl, pm->dbglock);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    return TE_SUCCESS;
}

static void ctl_destroy_drv( te_crypt_drv_t *crypt )
{
    te_ctl_drv_t *drv = (te_ctl_drv_t*)crypt;
    osal_memset(drv, 0, sizeof(*drv));
}

static void ctl_destroy_ctx( ctl_drv_ctx_t *ctx )
{
    osal_mutex_destroy(ctx->mut);
    osal_memset(ctx, 0, sizeof(*ctx));
}

static int ctl_get_ctx( ctl_drv_ctx_t *ctx )
{
    osal_atomic_inc(&ctx->refcnt);
    return TE_SUCCESS;
}

static int ctl_put_ctx( ctl_drv_ctx_t *ctx )
{
    if (osal_atomic_dec(&ctx->refcnt) == 0) {
        /* purge the ctl context */
        ctl_destroy_ctx(ctx);
        osal_free(ctx);
    }
    return TE_SUCCESS;
}

int te_ctl_drv_init( te_ctl_drv_t *drv,
                     struct te_hwa_ctl *ctl,
                     struct te_hwa_dma *dma,
                     struct te_hwa_dbgctl *dbg,
                     const char* name )
{
    int rc = 0;
    ctl_drv_ctx_t *ctx = NULL;

    if (!drv || !ctl || !dma || !dbg) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* initialize driver */
    if (drv->magic == CTL_DRV_MAGIC && osal_atomic_load(&drv->base.refcnt)) {
        /* already initialized */
        return TE_SUCCESS;
    }

    osal_memset(drv, 0, sizeof(*drv));
    drv->magic        = CTL_DRV_MAGIC;
    drv->base.hwa     = &ctl->base;
    drv->base.suspend = ctl_suspend;
    drv->base.resume  = ctl_resume;
    drv->base.destroy = ctl_destroy_drv;
    if (name) {
        osal_strncpy(drv->base.name, name, TE_MAX_DRV_NAME - 1);
    }

    /* initialize context */
    if ((ctx = osal_calloc(1, sizeof(*ctx))) == NULL) {
        return TE_ERROR_OOM;
    }

    rc = osal_mutex_create(&ctx->mut);
    if (rc != OSAL_SUCCESS) {
        goto err;
    }

    ctx->magic    = CTL_CTX_MAGIC;
    ctx->base.drv = &drv->base;
    ctx->dma      = dma;
    ctx->dbg      = dbg;

    /* complete */
    drv->hctx = (te_ctx_handle)ctx;
    ctl_get_ctx(ctx);
    te_crypt_drv_get(&drv->base);
    return TE_SUCCESS;

err:
    osal_free(ctx);
    return rc;
}

int te_ctl_drv_exit( te_ctl_drv_t *drv )
{
    if (!drv)
        return TE_ERROR_BAD_PARAMS;

    if (drv->magic != CTL_DRV_MAGIC)
        return TE_ERROR_BAD_FORMAT;

    ctl_put_ctx((ctl_drv_ctx_t*)drv->hctx);
    te_crypt_drv_put(&drv->base);
    return TE_SUCCESS;
}

int te_ctl_clk_enable( te_ctx_handle h, te_module_t mod )
{
    int rc = 0;
    te_hwa_ctl_t *ctl = NULL;
    ctl_drv_ctx_t *ctx = (ctl_drv_ctx_t*)h;

    if (NULL == h || 0 == mod)
        return TE_ERROR_BAD_PARAMS;

    if (mod & ~(TE_MOD_OTP)) {
        /**
         * modules other than OTP shall be taken care
         * by the relative driver.
         */
        return TE_ERROR_NOT_SUPPORTED;
    }

    if (ctx->magic != CTL_CTX_MAGIC) {
        return TE_ERROR_BAD_FORMAT;
    }

    ctl_get_ctx(ctx);
    ctl = GET_CTL_HWA(ctx);
    if (NULL == ctl->clock_ctl) {
        rc = TE_ERROR_NOT_SUPPORTED;
    } else {
        osal_mutex_lock(ctx->mut);
        rc = ctl->clock_ctl(ctl, mod, true);
        osal_mutex_unlock(ctx->mut);
    }
    ctl_put_ctx(ctx);
    return rc;
}

int te_ctl_clk_disable( te_ctx_handle h, te_module_t mod )
{
    int rc = 0;
    te_hwa_ctl_t *ctl = NULL;
    ctl_drv_ctx_t *ctx = (ctl_drv_ctx_t*)h;

    if (NULL == h || 0 == mod)
        return TE_ERROR_BAD_PARAMS;

    if (mod & ~(TE_MOD_OTP)) {
        /**
         * modules other than OTP shall be taken care
         * by the relative driver.
         */
        return TE_ERROR_NOT_SUPPORTED;
    }

    if (ctx->magic != CTL_CTX_MAGIC) {
        return TE_ERROR_BAD_FORMAT;
    }

    ctl_get_ctx(ctx);
    ctl = GET_CTL_HWA(ctx);
    if (NULL == ctl->clock_ctl) {
        rc = TE_ERROR_NOT_SUPPORTED;
    } else {
        osal_mutex_lock(ctx->mut);
        rc = ctl->clock_ctl(ctl, mod, false);
        osal_mutex_unlock(ctx->mut);
    }
    ctl_put_ctx(ctx);
    return rc;
}

int te_ctl_reset( te_ctx_handle h, te_module_t mod )
{
    int rc = 0;
    te_hwa_ctl_t *ctl = NULL;
    ctl_drv_ctx_t *ctx = (ctl_drv_ctx_t*)h;

    if (NULL == h || 0 == mod)
        return TE_ERROR_BAD_PARAMS;

    if (ctx->magic != CTL_CTX_MAGIC)
        return TE_ERROR_BAD_FORMAT;

    ctl_get_ctx(ctx);
    ctl = GET_CTL_HWA(ctx);
    if (NULL == ctl->sreset) {
        rc = TE_ERROR_NOT_SUPPORTED;
    } else {
        osal_mutex_lock(ctx->mut);
        rc = ctl->sreset(ctl, mod);
        osal_mutex_unlock(ctx->mut);
    }
    ctl_put_ctx(ctx);
    return rc;
}

int te_ctl_set_arb( te_ctx_handle h, te_module_t mod,
                    te_arb_algo_t alg, te_arb_gran_t gran )
{
    int rc = 0;
    te_hwa_ctl_t *ctl = NULL;
    te_top_conf_t conf = {0};
    ctl_drv_ctx_t *ctx = (ctl_drv_ctx_t*)h;

    if (NULL == h || 0 == mod)
        return TE_ERROR_BAD_PARAMS;

    if (mod & ~(TE_MOD_HASH | TE_MOD_SCA | TE_MOD_ACA | TE_MOD_TRNG)) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (ctx->magic != CTL_CTX_MAGIC) {
        return TE_ERROR_BAD_FORMAT;
    }

    ctl_get_ctx(ctx);
    ctl = GET_CTL_HWA(ctx);
    if (NULL == ctl->top_conf || NULL == ctl->conf_top) {
        rc = TE_ERROR_NOT_SUPPORTED;
        goto out;
    }

    osal_mutex_lock(ctx->mut);
    rc = ctl->top_conf(ctl, &conf);
    if (rc < 0) {
        goto out2;
    }

    if (mod & TE_MOD_HASH) {
        conf.hash_arb_gran = gran;
        conf.hash_arb_alg  = alg;
    }
    if (mod & TE_MOD_SCA) {
        conf.sca_arb_gran = gran;
        conf.sca_arb_alg  = alg;
    }
    if (mod & TE_MOD_ACA) {
        conf.aca_arb_gran = gran;
        conf.aca_arb_alg  = alg;
    }
    if (mod & TE_MOD_TRNG) {
        conf.rnp_arb_gran = gran;
        conf.rnp_arb_alg  = alg;
    }

    rc = ctl->conf_top(ctl, &conf);

out2:
    osal_mutex_unlock(ctx->mut);
out:
    ctl_put_ctx(ctx);
    return rc;
}

int te_ctl_set_host( te_ctx_handle h, int n, const te_host_conf_t *conf )
{
    int rc = 0;
    te_hwa_ctl_t *ctl = NULL;
    ctl_drv_ctx_t *ctx = (ctl_drv_ctx_t*)h;

    if (NULL == h)
        return TE_ERROR_BAD_PARAMS;

    if (ctx->magic != CTL_CTX_MAGIC)
        return TE_ERROR_BAD_FORMAT;

    ctl_get_ctx(ctx);
    ctl = GET_CTL_HWA(ctx);
    if (NULL == ctl->conf_host) {
        rc = TE_ERROR_NOT_SUPPORTED;
        goto out;
    }

    rc = get_host_num(hwa_crypt_host(&ctl->base));
    if (rc < 0) {
        goto out;
    }

    if (n < 0 || n > rc - 1) {
        /* bad host id */
        rc = TE_ERROR_BAD_PARAMS;
        goto out;
    }

    osal_mutex_lock(ctx->mut);
    rc = ctl->conf_host(ctl, n, conf);
    osal_mutex_unlock(ctx->mut);

out:
    ctl_put_ctx(ctx);
    return rc;
}

int te_ctl_get_host( te_ctx_handle h, int n, te_host_conf_t *conf )
{
    int rc = 0;
    te_hwa_ctl_t *ctl = NULL;
    te_hwa_host_t *host = NULL;
    ctl_drv_ctx_t *ctx = (ctl_drv_ctx_t*)h;

    if (NULL == h)
        return TE_ERROR_BAD_PARAMS;

    if (ctx->magic != CTL_CTX_MAGIC)
        return TE_ERROR_BAD_FORMAT;

    ctl_get_ctx(ctx);
    ctl = GET_CTL_HWA(ctx);
    host = hwa_crypt_host(&ctl->base);
    TE_ASSERT(host != NULL);
    if (NULL == host->stat.host_conf) {
        rc = TE_ERROR_NOT_SUPPORTED;
        goto out;
    }

    rc = get_host_num(host);
    if (rc < 0) {
        goto out;
    }

    if (n < 0 || n > rc - 1) {
        /* bad host id */
        rc = TE_ERROR_BAD_PARAMS;
        goto out;
    }

    rc = host->stat.host_conf(&host->stat, n, conf);

out:
    ctl_put_ctx(ctx);
    return rc;
}

int te_ctl_sw_init_done( te_ctx_handle h )
{
    int rc = 0;
    te_hwa_ctl_t *ctl = NULL;
    te_top_conf_t conf = {0};
    ctl_drv_ctx_t *ctx = (ctl_drv_ctx_t*)h;

    if (NULL == h)
        return TE_ERROR_BAD_PARAMS;

    if (ctx->magic != CTL_CTX_MAGIC)
        return TE_ERROR_BAD_FORMAT;

    ctl_get_ctx(ctx);
    ctl = GET_CTL_HWA(ctx);
    if (NULL == ctl->top_conf || NULL == ctl->conf_top) {
        rc = TE_ERROR_NOT_SUPPORTED;
        goto out;
    }

    osal_mutex_lock(ctx->mut);
    rc = ctl->top_conf(ctl, &conf);
    if (rc < 0) {
        goto out2;
    }

    if (!conf.sw_init_done) {
        conf.sw_init_done = 1;
        rc = ctl->conf_top(ctl, &conf);
    } else {
        rc = TE_SUCCESS;
    }

out2:
    osal_mutex_unlock(ctx->mut);
out:
    ctl_put_ctx(ctx);
    return rc;
}

int te_ctl_lock_ctx_pool( te_ctx_handle h )
{
    int rc = 0;
    te_hwa_ctl_t *ctl = NULL;
    te_top_conf_t conf = {0};
    ctl_drv_ctx_t *ctx = (ctl_drv_ctx_t*)h;

    if (NULL == h)
        return TE_ERROR_BAD_PARAMS;

    if (ctx->magic != CTL_CTX_MAGIC)
        return TE_ERROR_BAD_FORMAT;

    ctl_get_ctx(ctx);
    ctl = GET_CTL_HWA(ctx);
    if (NULL == ctl->top_conf || NULL == ctl->conf_top) {
        rc = TE_ERROR_NOT_SUPPORTED;
        goto out;
    }

    osal_mutex_lock(ctx->mut);
    rc = ctl->top_conf(ctl, &conf);
    if (rc < 0) {
        goto out2;
    }

    if (!conf.ctx_pool_lock) {
        conf.ctx_pool_lock = 1;
        rc = ctl->conf_top(ctl, &conf);
    } else {
        rc = TE_SUCCESS;
    }

out2:
    osal_mutex_unlock(ctx->mut);
out:
    ctl_put_ctx(ctx);
    return rc;
}

int te_ctl_set_debug( te_ctx_handle h, const uint32_t ctrl, bool lock )
{
    int rc = 0;
    te_hwa_ctl_t *ctl = NULL;
    te_hwa_dbgctl_t *dbg = NULL;
    ctl_drv_ctx_t *ctx = (ctl_drv_ctx_t*)h;

    if (NULL == h)
        return TE_ERROR_BAD_PARAMS;

    if (ctx->magic != CTL_CTX_MAGIC)
        return TE_ERROR_BAD_FORMAT;

    ctl_get_ctx(ctx);
    ctl = GET_CTL_HWA(ctx);
    dbg = hwa_crypt_host(&ctl->base)->dbgctl;
    if (NULL == dbg || NULL == dbg->set_dbgctl ||
        (lock && NULL == dbg->set_locken)) {
        rc = TE_ERROR_NOT_SUPPORTED;
        goto out;
    }

    osal_mutex_lock(ctx->mut);
    rc = dbg->set_dbgctl(dbg, ctrl);
    if (TE_SUCCESS == rc && lock) {
        rc = dbg->set_locken(dbg, ctrl);
    }
    osal_mutex_unlock(ctx->mut);

out:
    ctl_put_ctx(ctx);
    return rc;
}

int te_ctl_get_debug( te_ctx_handle h, uint32_t *ctrl, uint32_t *lock )
{
    int rc = 0;
    te_hwa_ctl_t *ctl = NULL;
    te_hwa_dbgctl_t *dbg = NULL;
    ctl_drv_ctx_t *ctx = (ctl_drv_ctx_t*)h;

    if (NULL == h)
        return TE_ERROR_BAD_PARAMS;

    if (ctx->magic != CTL_CTX_MAGIC)
        return TE_ERROR_BAD_FORMAT;

    ctl_get_ctx(ctx);
    ctl = GET_CTL_HWA(ctx);
    dbg = hwa_crypt_host(&ctl->base)->dbgctl;
    if (NULL == dbg) {
        rc = TE_ERROR_NOT_SUPPORTED;
        goto out;
    }

    if (ctl != NULL) {
        rc = dbg->get_dbgctl ? dbg->get_dbgctl(dbg, ctrl) :
                               (int)TE_ERROR_NOT_SUPPORTED;
        if (rc < 0) {
            goto out;
        }
    }
    if (lock != NULL) {
        rc = dbg->get_locken ? dbg->get_locken(dbg, lock) :
                               (int)TE_ERROR_NOT_SUPPORTED;
    }

out:
    ctl_put_ctx(ctx);
    return rc;
}

//TODO: add other interfaces
