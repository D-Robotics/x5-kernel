//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <driver/te_drv_hash.h>
#include <driver/te_drv_sca.h>
#include <driver/te_drv_aca.h>
#include <driver/te_drv_otp.h>
#include <driver/te_drv_trng.h>
#include <driver/te_drv_ctl.h>
#include <hwa/te_hwa.h>
#include "drv_internal.h"
#include "../common/te_worker_pool.h"

/**
 * SCA/HASH slot context number definitions.
 *
 * The total context SRAM size is defined by cq_sram_size.ctx_sram_size.
 * The CTX SRAM is shared by SCA and HASH engines internally.
 * The default setting is given according to:
 *     2x host, and 8x short + 8x long CTX for each engine in each host.
 *     That is: (13 + 21 + 11 + 21) * 4 * 8 * 2 = 4224 bytes.
 *
 * SCA slot contexts are divided into two categories: short(1) and long(2).
 *    cat  |words |bytes |
 *  -------+------+------+
 *   short |  13  |  52  |
 *    long |  21  |  84  |
 *  -------+------+------+
 *
 * HASH slot contexts are divided into two categories: short(1) and long(2).
 *    cat  |words |bytes |
 *  -------+------+------+
 *   short |  11  |  44  |
 *    long |  21  |  84  |
 *  -------+------+------+
 */
#define SCA_CTX_SIZE_S         52
#define SCA_CTX_SIZE_L         84

#define SCA_S_SLOT_CTX1_NUM    8
#define SCA_S_SLOT_CTX2_NUM    8
#define SCA_NS_SLOT_CTX1_NUM   8
#define SCA_NS_SLOT_CTX2_NUM   8

#define HASH_CTX_SIZE_S        44
#define HASH_CTX_SIZE_L        84

#define HASH_S_SLOT_CTX1_NUM   8
#define HASH_S_SLOT_CTX2_NUM   8
#define HASH_NS_SLOT_CTX1_NUM  8
#define HASH_NS_SLOT_CTX2_NUM  8

/**
 * Trust engine driver magic number
 */
#define TE_DRV_MAGIC 0x56724454U /**< "TDrV" */

/**
 * Trust engine driver structure
 */
typedef struct te_drv {
    /* commona parts */
    te_crypt_drv_t base;         /**< base class, must be the first */
    uint32_t magic;
    te_hwa_host_t *host;
    te_suspend_state_t state;

    /* general drivers */
    te_hash_drv_t hash;
    te_sca_drv_t sca;
    te_aca_drv_t aca;
    te_otp_drv_t otp;
    te_trng_drv_t trng;

    /* host0 only drivers */
    te_ctl_drv_t *ctl;
} te_drv_t;

/**
 * Trust engine configuration data
 */
static const struct te_conf_data {
    te_top_conf_t top;
    te_host_conf_t hosts[2];
} g_te_conf_data = {
    /* top configs */
    .top = {
        .hash_arb_gran = TE_ARB_GRAN_CMD,
        .hash_arb_alg  = TE_ARB_ALG_FIXED,
        .sca_arb_gran  = TE_ARB_GRAN_CMD,
        .sca_arb_alg   = TE_ARB_ALG_FIXED,
        .aca_arb_gran  = TE_ARB_GRAN_CMD,
        .aca_arb_alg   = TE_ARB_ALG_FIXED,
        .rnp_arb_gran  = TE_ARB_GRAN_CMD,
        .rnp_arb_alg   = TE_ARB_ALG_FIXED,
    },
    /* host#0 configs (sec) */
    .hosts[0] = {
        .hash_nctx1 = HASH_S_SLOT_CTX1_NUM,
        .hash_nctx2 = HASH_S_SLOT_CTX2_NUM,
        .sca_nctx1  = SCA_S_SLOT_CTX1_NUM,
        .sca_nctx2  = SCA_S_SLOT_CTX2_NUM,
        .hash_sec   = true,
        .sca_sec    = true,
        .aca_sec    = true,
        .rnp_sec    = true,
    },
    /* other-hosts configs (nsec) */
    .hosts[1] = {
        .hash_nctx1 = HASH_NS_SLOT_CTX1_NUM,
        .hash_nctx2 = HASH_NS_SLOT_CTX2_NUM,
        .sca_nctx1  = SCA_NS_SLOT_CTX1_NUM,
        .sca_nctx2  = SCA_NS_SLOT_CTX2_NUM,
        .hash_sec   = false,
        .sca_sec    = false,
        .aca_sec    = false,
        .rnp_sec    = false,
    },
};

static int drv_verify_conf(struct te_hwa_host *host,
                           const struct te_conf_data *cfg)
{
    int rc = TE_SUCCESS, i = 0;
    te_hwa_stat_t *stat = &host->stat;
    te_rtl_conf_t rtl = {0};
    const te_host_conf_t *hconf = NULL;
    uint32_t ntotal = 0;

    TE_ASSERT(stat->conf);
    rc = stat->conf(stat, &rtl);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    for (i = 0; i < rtl.cfg0.sca_nhost; i++) {
        hconf = (0 == i) ? &cfg->hosts[0] : &cfg->hosts[1];
        ntotal += hconf->sca_nctx1 * SCA_CTX_SIZE_S +
                  hconf->sca_nctx2 * SCA_CTX_SIZE_L;
    }

    for (i = 0; i < rtl.cfg0.hash_nhost; i++) {
        hconf = (0 == i) ? &cfg->hosts[0] : &cfg->hosts[1];
        ntotal += hconf->hash_nctx1 * HASH_CTX_SIZE_S +
                  hconf->hash_nctx2 * HASH_CTX_SIZE_L;
    }

    if (ntotal > rtl.sram.ctx_sram_sz) {
        /* bad sram configs */
        return TE_ERROR_EXCESS_DATA;
    }

    return TE_SUCCESS;
}

static int drv_conf_engine(struct te_hwa_host *host,
                           const struct te_conf_data *cfg)
{
    int rc = TE_SUCCESS;
    int i = 0;
    te_hwa_stat_t *stat = &host->stat;
    te_hwa_ctl_t *ctl = host->ctl;
    te_top_conf_t top = {0};
    const te_host_conf_t *hconf = NULL;
    te_top_stat_t tstat = {0};
    te_host_int_t istat = {0};
    int nhost = 0;

    /* check ctx_pool_lock to avoid reconfigure */
    TE_ASSERT(ctl && ctl->top_conf && ctl->conf_top && ctl->conf_host);
    rc = ctl->top_conf(ctl, &top);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    if (top.ctx_pool_lock) {
        return TE_SUCCESS;    /* initialized early */
    }

    /* OTP selection banner */
#ifdef CFG_OTP_WITH_PUF
    OSAL_LOG_INFO("Select PUF OTP\n");
#else
    OSAL_LOG_INFO("Select regular OTP\n");
#endif

    /* sanity check configurations */
    rc = drv_verify_conf(host, cfg);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    /* configure hosts */
    TE_ASSERT(stat->host_num);
    nhost = stat->host_num(stat);
    if (nhost < 0) {
        return nhost;
    }

    for (i = 0; i < nhost; i++) {
        hconf = (0 == i) ? &cfg->hosts[0] : &cfg->hosts[1];
        rc = ctl->conf_host(ctl, i, hconf);
        if (rc != TE_SUCCESS) {
            return rc;
        }
    }

    /* configure top, and lock ctx pool */
    osal_memcpy(&top, &cfg->top, sizeof(top));
    top.ctx_pool_lock = true;
    rc = ctl->conf_top(ctl, &top);

    /*
     * It takes approx ~50 cycles for merak to finish evaluating the host
     * configurations after sw programming the ctx_pool_lock. That equals
     * to ~500ns when merak works @100MHz. Moreover, the number would rise
     * up as host number increases.
     *
     * On success, top_stat.ctx_pool_rdy will assert.
     * On failure, merak_intr_host0.intr_ctx_pool_err will assert.
     *
     * NOTE: It is not allowed to write to SCA/HASH CQ prior to hw done
     * the evaluation. Consequently, explicit sw delay is required here
     * to make sure configuring host goes right.
     */
    OSAL_LOG_DEBUG("Wait hw configuring ctx pool...\n");
    TE_ASSERT(stat->state);
    TE_ASSERT(stat->host_state);
    do {
        memset(&istat, 0, sizeof(istat));
        stat->host_state(stat, 0, &istat);
        TE_ASSERT_MSG(!istat.ctxp_err, "Config ctx pool error!\n");

        memset(&tstat, 0, sizeof(tstat));
        stat->state(stat, &tstat);
    } while (!tstat.ctx_pool_rdy);

    return rc;
}

static int te_set_sw_init_done( struct te_hwa_host *host )
{
    int rc = TE_SUCCESS;
    te_hwa_ctl_t *ctl = host->ctl;
    te_top_conf_t top;

    TE_ASSERT(ctl && ctl->top_conf && ctl->conf_top);
    rc = ctl->top_conf(ctl, &top);
    if (rc != TE_SUCCESS) {
        goto err;
    }

    if (!top.sw_init_done) {
        top.sw_init_done = true;
        rc = ctl->conf_top(ctl, &top);
        if (rc != TE_SUCCESS) {
            goto err;
        }
    }
err:
    return rc;
}

static int te_drv_init( te_drv_t *drv, struct te_hwa_host *host )
{
#define TE_STUB_DRV_INIT(nM,...) do {                                     \
        rc = te_##nM##_drv_init(&drv->nM, &host->nM, ##__VA_ARGS__, #nM); \
        if (rc < 0) {                                                     \
            goto err_##nM##_init;                                         \
        }                                                                 \
} while(0)

#define TE_HOST_DRV_INIT(nM,...) do {                                     \
    if (conf.cfg0. nM ##_nhost) {                                         \
        TE_STUB_DRV_INIT(nM, ##__VA_ARGS__);                              \
    }                                                                     \
} while(0)

#define TE_STUB_DRV_INIT_ERROR(nM)                                        \
        te_##nM##_drv_exit(&drv->nM);                                     \
err_##nM##_init:

#define TE_HOST_DRV_INIT_ERROR(nM) do {                                   \
    if (conf.cfg0. nM ##_nhost) {                                         \
        TE_STUB_DRV_INIT_ERROR(nM);                                       \
    }                                                                     \
} while(0)

    int rc = TE_SUCCESS, id = 0;
    te_rtl_conf_t conf = { 0 };
    te_hwa_stat_t *stat = &host->stat;

    memset(drv, 0, sizeof(*drv));
    TE_ASSERT(stat->conf);
    rc = stat->conf( stat, &conf );
    TE_ASSERT(TE_SUCCESS == rc);
    /* init ctl drv and configure merak if host0 */
    if (0 == (id = te_hwa_host_id(host))) {
        drv->ctl = osal_calloc(1, sizeof(*drv->ctl));
        if (NULL == drv->ctl) {
            rc = TE_ERROR_OOM;
            goto out;
        }

        rc = te_ctl_drv_init(drv->ctl, host->ctl, host->dma,
                             host->dbgctl, NULL);
        if (rc < 0) {
            goto err1;
        }

        /* configure the trust engine */
        rc = drv_conf_engine(host, &g_te_conf_data);
        if (rc != TE_SUCCESS) {
            goto err2;
        }
    /* wait until sw_init_done is set if not host0 */
    } else {
        #define TE_WAIT_INIT_MS   2
        te_top_stat_t st = {0};

        OSAL_LOG_WARN("host[%d] waits sw_init_done...", id);
        TE_ASSERT(stat->state);
        do {
            rc = stat->state(stat, &st);
            if (rc != TE_SUCCESS) {
                return rc;
            }
            osal_delay_ms(TE_WAIT_INIT_MS);
        } while (!st.sw_init_done);
        OSAL_LOG_WARN("[DONE]\n");
    }

    rc = te_worker_pool_create();
    if (rc != TE_SUCCESS) {
        goto err3;
    }

    /* init otp drv */
    TE_STUB_DRV_INIT(otp, host->otpctl);

    /* init trng drv */
    TE_HOST_DRV_INIT(trng, host->trngctl);

    /* init hash drv */
    TE_HOST_DRV_INIT(hash);

    /* init sca drv */
    TE_HOST_DRV_INIT(sca);

    /* init aca drv */
    TE_HOST_DRV_INIT(aca);

    /* set sw_init_done flag if host0 */
    if (0 == id) {
        rc = te_set_sw_init_done(host);
        if (rc != TE_SUCCESS) {
            goto err4;
        }
        OSAL_LOG_INFO("set sw_init_done!\n");
    }

    drv->host = host;
    drv->magic = TE_DRV_MAGIC;
    drv->state = TE_SUSPEND_ON;
    rc = TE_SUCCESS;
    goto out;

err4:
    TE_HOST_DRV_INIT_ERROR(aca);
    TE_HOST_DRV_INIT_ERROR(sca);
    TE_HOST_DRV_INIT_ERROR(hash);
    TE_HOST_DRV_INIT_ERROR(trng);
    TE_STUB_DRV_INIT_ERROR(otp);

    te_worker_pool_destroy();
err3:
    if (0 == id) {
err2:
        te_ctl_drv_exit(drv->ctl);
err1:
        osal_free(drv->ctl);
    }
out:
    return rc;
#undef TE_STUB_DRV_INIT
#undef TE_STUB_DRV_INIT_ERROR
#undef TE_HOST_DRV_INIT
#undef TE_HOST_DRV_INIT_ERROR
}

static int te_drv_exit( te_drv_t *drv )
{
#define TE_HOST_DRV_EXIT(nM) do {                                         \
    if (conf.cfg0. nM ##_nhost) {                                         \
        te_##nM##_drv_exit(&drv->nM);                                     \
    }                                                                     \
} while(0)

    te_rtl_conf_t conf = { 0 };
    te_hwa_stat_t *stat = NULL;
    int rc = TE_SUCCESS;

    if (NULL == drv->host)
        return TE_ERROR_BAD_STATE;

    stat = &drv->host->stat;
    TE_ASSERT(stat->conf);
    rc = stat->conf( stat, &conf );
    TE_ASSERT(TE_SUCCESS == rc);

    TE_HOST_DRV_EXIT(aca);
    TE_HOST_DRV_EXIT(sca);
    TE_HOST_DRV_EXIT(hash);
    TE_HOST_DRV_EXIT(trng);
    te_worker_pool_destroy();
    te_otp_drv_exit(&drv->otp);
    if (drv->ctl) {
        te_ctl_drv_exit(drv->ctl);
        osal_free(drv->ctl);
    }
    return TE_SUCCESS;
#undef TE_HOST_DRV_EXIT
}

static te_crypt_drv_t* te_find_drv (te_drv_t *drv,
                                    te_drv_type_t type )
{
    te_crypt_drv_t *crypt = NULL;

    switch (type) {
    case TE_DRV_TYPE_HASH:
        crypt = &drv->hash.base;
        break;
    case TE_DRV_TYPE_SCA:
        crypt = &drv->sca.base;
        break;
    case TE_DRV_TYPE_ACA:
        crypt = &drv->aca.base;
        break;
    case TE_DRV_TYPE_OTP:
        crypt = &drv->otp.base;
        break;
    case TE_DRV_TYPE_TRNG:
        crypt = &drv->trng.base;
        break;
    case TE_DRV_TYPE_CTL:
        if (drv->ctl) {
            crypt = &drv->ctl->base;
        }
        break;
    default:
        return NULL;
    }

    return crypt;
}

int te_drv_alloc( struct te_hwa_host *host, te_drv_handle *h )
{
    int rc = TE_SUCCESS;
    te_drv_t *drv = NULL;

    if (NULL == host || NULL == h) {
        return TE_ERROR_BAD_PARAMS;
    }

    drv = osal_malloc(sizeof(*drv));
    if (NULL == drv) {
        return TE_ERROR_OOM;
    }

    rc = te_drv_init(drv, host);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    *h = (te_drv_handle)drv;
    return rc;
}

int te_drv_free( te_drv_handle h )
{
    te_drv_t *drv = (te_drv_t*)h;

    if (NULL == drv || drv->magic != TE_DRV_MAGIC) {
        return TE_ERROR_BAD_PARAMS;
    }

    te_drv_exit(drv);
    memset(drv, 0, sizeof(*drv));
    osal_free(drv);
    return TE_SUCCESS;
}

te_crypt_drv_t* te_drv_get( te_drv_handle h, te_drv_type_t type )
{
    te_drv_t *drv = (te_drv_t*)h;
    te_crypt_drv_t *crypt = NULL;

    if (NULL == drv || drv->magic != TE_DRV_MAGIC) {
        return NULL;    /**< TE_ERROR_BAD_PARAMS */
    }

    crypt = te_find_drv(drv, type);
    if (crypt != NULL) {
        te_crypt_drv_get(crypt);
    }
    return crypt;
}

int te_drv_put( te_drv_handle h, te_drv_type_t type )
{
    te_drv_t *drv = (te_drv_t*)h;
    te_crypt_drv_t *crypt = NULL;

    if (NULL == drv || drv->magic != TE_DRV_MAGIC) {
        return TE_ERROR_BAD_PARAMS;
    }

    crypt = te_find_drv(drv, type);
    if (crypt != NULL) {
        te_crypt_drv_put(crypt);
        return TE_SUCCESS;
    } else {
        return TE_ERROR_ITEM_NOT_FOUND;
    }
}

#define TE_STUB_DRV_SUSPEND(dRv, nM, sM) do {                             \
    rc = (dRv)->base.suspend(&(dRv)->base, sM);                           \
    if (rc != TE_SUCCESS) {                                               \
        goto err_suspend_##nM;                                            \
    }                                                                     \
} while(0)

#define TE_STUB_DRV_SUSPEND_ERROR(dRv, nM, sM)                            \
    TE_STUB_DRV_RESUME(dRv, sM);                                          \
err_suspend_##nM:

#define TE_HOST_DRV_SUSPEND(nM, sM) do {                                  \
    if (conf.cfg0. nM ##_nhost) {                                         \
        TE_STUB_DRV_SUSPEND(&drv->nM, nM, sM);                            \
    }                                                                     \
} while(0)

#define TE_HOST_DRV_SUSPEND_ERROR(nM, sM) do {                            \
    if (conf.cfg0. nM ##_nhost) {                                         \
        TE_STUB_DRV_SUSPEND_ERROR(&drv->nM, nM, sM);                      \
    }                                                                     \
} while(0)

#define TE_STUB_DRV_RESUME(dRv, sM) do {                                  \
    int _rc = TE_SUCCESS;                                                 \
    _rc = (dRv)->base.resume(&(dRv)->base, sM);                           \
    TE_ASSERT(TE_SUCCESS == _rc);                                         \
} while(0)

#define TE_HOST_DRV_RESUME(nM, sM) do {                                   \
    if (conf.cfg0. nM ##_nhost) {                                         \
        TE_STUB_DRV_RESUME(&drv->nM, sM);                                 \
    }                                                                     \
} while(0)

int te_drv_suspend( te_drv_handle h, te_suspend_state_t state )
{
    int rc = TE_SUCCESS;
    te_drv_t *drv = (te_drv_t*)h;
    te_rtl_conf_t conf = { 0 };
    te_hwa_stat_t *stat = NULL;

    if (NULL == drv || drv->magic != TE_DRV_MAGIC) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (NULL == drv->host) {
        return TE_ERROR_BAD_STATE;
    }

    if (state > TE_SUSPEND_MEM || state < TE_SUSPEND_STANDBY) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* Device already in low power mode, need to resume to transition to new state */
    if (TE_SUSPEND_ON != drv->state && drv->state != state) {
        return TE_ERROR_BAD_STATE;
    }

    if (TE_SUSPEND_ON != drv->state && drv->state == state) {
        return TE_SUCCESS;
    }

    TE_ASSERT(TE_SUSPEND_ON == drv->state);

    stat = &drv->host->stat;
    TE_ASSERT(stat->conf);
    rc = stat->conf( stat, &conf );
    TE_ASSERT(TE_SUCCESS == rc);

    /*
     * Suspend TE drivers.
     * The suspend order is important. DO NOT change if unsure.
     */
    TE_STUB_DRV_SUSPEND(&drv->otp, otp, state);
    TE_HOST_DRV_SUSPEND(trng, state);
    TE_HOST_DRV_SUSPEND(aca, state);
    TE_HOST_DRV_SUSPEND(hash, state);
    TE_HOST_DRV_SUSPEND(sca, state);

    if (drv->ctl) {
        TE_STUB_DRV_SUSPEND(drv->ctl, ctl, state);
    }

    drv->state = state;

    return TE_SUCCESS;

    /* On errors */
    if (drv->ctl) {
        TE_STUB_DRV_SUSPEND_ERROR(drv->ctl, ctl, state);
    }

    TE_HOST_DRV_SUSPEND_ERROR(sca, state);
    TE_HOST_DRV_SUSPEND_ERROR(hash, state);
    TE_HOST_DRV_SUSPEND_ERROR(aca, state);
    TE_HOST_DRV_SUSPEND_ERROR(trng, state);
    TE_STUB_DRV_SUSPEND_ERROR(&drv->otp, otp, state);
    return rc;
}

int te_drv_resume( te_drv_handle h )
{
    te_drv_t *drv = (te_drv_t*)h;
    te_rtl_conf_t conf = { 0 };
    te_hwa_stat_t *stat = NULL;
    te_hwa_host_t *host = NULL;
    int rc = TE_SUCCESS;

    if (NULL == drv || drv->magic != TE_DRV_MAGIC) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (NULL == drv->host) {
        return TE_ERROR_BAD_STATE;
    }

    /* Already in working state */
    if (TE_SUSPEND_ON == drv->state) {
        return TE_SUCCESS;
    }

    host = drv->host;
    stat = &drv->host->stat;
    TE_ASSERT(stat->conf);
    rc = stat->conf( stat, &conf );
    TE_ASSERT(TE_SUCCESS == rc);

    /*
     * Resume TE drivers in reverse order of suspend.
     * The order is important. DO NOT change if unsure.
     */
    if (drv->ctl) {
        TE_STUB_DRV_RESUME(drv->ctl, drv->state);
    }
    if (0 == (te_hwa_host_id(host))) {
        rc = drv_conf_engine(host, &g_te_conf_data);
        TE_ASSERT(TE_SUCCESS == rc);
        rc = te_set_sw_init_done(host);
        TE_ASSERT(TE_SUCCESS == rc);
    }

    TE_HOST_DRV_RESUME(sca, drv->state);
    TE_HOST_DRV_RESUME(hash, drv->state);
    TE_HOST_DRV_RESUME(aca, drv->state);
    TE_HOST_DRV_RESUME(trng, drv->state);
    TE_STUB_DRV_RESUME(&drv->otp,drv->state);

    drv->state = TE_SUSPEND_ON;

    return TE_SUCCESS;
}
#undef TE_STUB_DRV_SUSPEND
#undef TE_HOST_DRV_SUSPEND
#undef TE_STUB_DRV_SUSPEND_ERROR
#undef TE_HOST_DRV_SUSPEND_ERROR
#undef TE_STUB_DRV_RESUME
#undef TE_HOST_DRV_RESUME
