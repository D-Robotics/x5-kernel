//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <driver/te_drv_trng.h>
#include <hwa/te_hwa_trng.h>
#include <hwa/te_hwa_trngctl.h>
#include <hwa/te_hwa.h>
#include "drv_internal.h"

#define _TRNG_DRV_OUT_           goto __out__

#define __TRNG_DRV_CHECK_CONDITION__(_ret_)                                    \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 _TRNG_DRV_OUT_;                                               \
            }                                                                  \
        } while (0);

#define __TRNG_DRV_ALERT__(_ret_, _msg_)                                       \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X %s\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_),                                       \
                                (_msg_));                                      \
            }                                                                  \
        } while (0);

#define __TRNG_DRV_VERIFY_PARAMS__(_param_)                                    \
        do                                                                     \
        {                                                                      \
            if(NULL == (_param_)){                                             \
                ret = TE_ERROR_BAD_PARAMS;                                     \
                _TRNG_DRV_OUT_;                                                \
            }                                                                  \
        } while (0)


#define ENTROPY_SRC_INTERNAL            (0U)
#define ENTROPY_SRC_EXTERNAL            (1U)

#define TRNG_POOL_SIZE                  (32U)
#define WAIT_POOL_FILLED_US             (3U)
/**
 * Derive the TRNG CTL hwa pointer from the driver context
 */
#define TRNG_CTX_GET_CTL(_ctx) __extension__({            \
    te_hwa_trngctl_t *_ctl_ = NULL;                       \
    _ctl_ = (te_hwa_trngctl_t *)((_ctx)->ctl);            \
    TE_ASSERT(_ctl_ != NULL);                             \
    _ctl_;                                                \
})

/**
 * Derive the TRNG CTX drv pointer from the driver context
 */
#define TRNG_CTX_GET_DRV(_ctx) __extension__({            \
    te_trng_drv_t *_drv = NULL;                           \
    _drv = (te_trng_drv_t *)((_ctx)->base.drv);           \
    TE_ASSERT(_drv != NULL);                              \
    _drv;                                                 \
})
/**
 * Derive the TRNG CTX pointer from the driver context
 */
#define TRNG_GET_CTX(_drv) __extension__({                \
    trng_drv_ctx_t *_ctx = NULL;                          \
    _ctx = (trng_drv_ctx_t *)((_drv)->hctx);              \
    TE_ASSERT(_ctx != NULL);                              \
    _ctx;                                                 \
})

/**
 * Derive the TRNG hwa pointer from the driver context
 */
#define GET_TRNG_HWA(_drv) __extension__({                \
    te_hwa_trng_t *_rnp = NULL;                           \
    _rnp = (te_hwa_trng_t *)((_drv)->base.hwa);           \
    TE_ASSERT(_rnp != NULL);                              \
    _rnp;                                                 \
})

#define DELAY               (10)

/**
 * TRNG context magic number
 */
#define TRNG_CTX_MAGIC  0x43476e72U /**< "rnGC" */

/**
 * Trust engine TRNG pm context structure
 * Single instance
 */
typedef struct trng_pm_ctx {
    te_trng_conf_t conf;           /**< trng configuration */
    te_trng_int_t int_mask;        /**< trng intr mask */
    te_rnp_int_t rnp_int_mask;     /**< rnp intr mask */
} trng_pm_ctx_t;

/**
 * Trust engine TRNG context structure
 * Single instance
 */
typedef struct trng_drv_ctx {
    te_crypt_ctx_t base;            /**< base context */
    uint32_t magic;                 /**< TRNG ctx magic */
    osal_mutex_t mut;               /**< exclusive lock */
    osal_atomic_t refcnt;           /**< reference count */
    te_hwa_crypt_t *ctl;            /**< TRNG CTL HWA */
} trng_drv_ctx_t;

static int _trng_setup_config(te_trng_drv_t *drv,
                               trng_drv_ctx_t *ctx,
                              const te_hwa_trngctl_t *ctl);

static int trng_get_ctx( trng_drv_ctx_t *ctx )
{
    osal_atomic_inc(&ctx->refcnt);
    return TE_SUCCESS;
}

static void _trng_destroy_ctx(trng_drv_ctx_t *ctx)
{
    osal_mutex_destroy(ctx->mut);
    osal_memset(ctx, 0x00, sizeof(trng_drv_ctx_t));
}

static int trng_put_ctx( trng_drv_ctx_t *ctx )
{
    if (0 == osal_atomic_dec(&ctx->refcnt)) {
        _trng_destroy_ctx(ctx);
        osal_free(ctx);
    }

    return TE_SUCCESS;
}

static inline int trng_trigger_hwkey_gen(const te_hwa_trngctl_t *ctl)
{
    int ret = TE_SUCCESS;
    te_trng_stat_t state = {0};
    te_trng_ctl_t trng_ctl = {0};

    ret = ctl->get_ctl((te_hwa_trngctl_t *)ctl, &trng_ctl);
    __TRNG_DRV_CHECK_CONDITION__(ret);
    trng_ctl.hw_key_gen = 1;
    ret = ctl->set_ctl((te_hwa_trngctl_t *)ctl, &trng_ctl);
    __TRNG_DRV_CHECK_CONDITION__(ret);

    do {
        ret = ctl->state((te_hwa_trngctl_t *)ctl, &state);
        __TRNG_DRV_CHECK_CONDITION__(ret);
        osal_delay_us(DELAY);
    } while (0 == state.hw_key_valid);
__out__:
    return ret;
}

static int trng_gen_hwkey(const te_hwa_trngctl_t *ctl)
{
    int ret = TE_SUCCESS;
    te_trng_conf_t trng_conf = {0};
    te_trng_stat_t state = {0};

    ret = ctl->state((te_hwa_trngctl_t *)ctl, &state);
    __TRNG_DRV_CHECK_CONDITION__(ret);
    if (state.hw_key_valid != 0) {
        goto __out__;
    }

    ret = ctl->get_conf((te_hwa_trngctl_t *)ctl, &trng_conf);
    __TRNG_DRV_CHECK_CONDITION__(ret);

    trng_conf.src.grp0_en = 1;
    trng_conf.src.grp1_en = 1;
    trng_conf.src.grp2_en = 1;
    trng_conf.src.grp3_en = 1;
    trng_conf.src.src_sel = ENTROPY_SRC_EXTERNAL;
    trng_conf.postproc.prng_bypass = 1;
    trng_conf.postproc.vn_bypass = 1;
    trng_conf.postproc.crng_bypass = 1;
    trng_conf.postproc.rsvd = 0;
    trng_conf.postproc.lfsr_drop_num = 1;
    trng_conf.postproc.lfsr_sel = 0;
    trng_conf.postproc.fast_mode = 1;
    ret = ctl->setup((te_hwa_trngctl_t *)ctl, &trng_conf);
    __TRNG_DRV_CHECK_CONDITION__(ret);

    ret = trng_trigger_hwkey_gen(ctl);
    __TRNG_DRV_CHECK_CONDITION__(ret);

__out__:
    return ret;
}

static int trng_dump( te_ctx_handle h,
                      te_trng_request_t *req )
{
    int ret = TE_SUCCESS;
    trng_drv_ctx_t *ctx = (trng_drv_ctx_t *)h;
    te_hwa_trngctl_t *ctl = NULL;
    te_trng_drv_t *drv = NULL;
    te_hwa_trng_t *rnp = NULL;
    te_rnp_ctl_t _ctl = {0};
    size_t _len = 0;
    size_t i = 0;
    size_t offset = 0;
    te_rnp_int_t int_state = {0};
    te_rnp_stat_t poll_state = {0};
    te_trng_int_t trng_int_state = {0};
    volatile int err = 0;

    if (!ctx || !req) {
        return TE_ERROR_BAD_PARAMS;
    }

    drv = TRNG_CTX_GET_DRV(ctx);
    if (TRNG_DRV_MAGIC != drv->magic) {
        return TE_ERROR_BAD_FORMAT;
    }

    trng_get_ctx(ctx);
    if (0 == te_hwa_host_id(drv->base.hwa->host)) {
        ctl = TRNG_CTX_GET_CTL(ctx);
    }
    if (req->b_conf) {
        if (0 == te_hwa_host_id(drv->base.hwa->host)) {
            ret = ctl->setup(ctl, &req->conf);
            __TRNG_DRV_CHECK_CONDITION__(ret);
        } else {
            ret = TE_ERROR_NOT_SUPPORTED;
            goto __out__;
        }
    }

    osal_mutex_lock(ctx->mut);
    for (i = 0; i < req->nmemb; i++) {
        rnp = (te_hwa_trng_t *)drv->base.hwa;
        TE_ASSERT(NULL != rnp);
        while (offset < req->size) {
            _len = ((req->size - offset) > TRNG_POOL_SIZE) ? TRNG_POOL_SIZE :
                                             req->size - offset;
            _ctl.clk_en = 1;
            _ctl.fill_req = 1;
            ret = rnp->set_ctl(rnp, &_ctl);
            if (TE_SUCCESS != ret) {
                goto __cleanup__;
            }
            do{
                ret = rnp->int_state(rnp, &int_state);
                if (TE_SUCCESS != ret) {
                    goto __cleanup__;
                }
                rnp->eoi(rnp, &int_state);
                if(int_state.fill_done){
                    ret = rnp->read(rnp, (uint8_t *)req->buf + offset, _len);
                    if (TE_SUCCESS != ret) {
                        goto __cleanup__;
                    }
                    offset += _len;
                    if (0 == te_hwa_host_id(rnp->base.host)) {
                        ret = ctl->int_state(ctl, &trng_int_state);
                        ctl->eoi(ctl, &trng_int_state);
                        if (TE_SUCCESS != ret) {
                            goto __cleanup__;
                        }
                        if (trng_int_state.adap_tst_err) {
                            err |= 1 << TE_TRNG_ADAP_TEST_ERR_MASK;
                        }
                        if (trng_int_state.crng_err) {
                            err |= 1 << TE_TRNG_CRNG_ERR_MASK;
                        }
                        if (trng_int_state.rep_tst_err) {
                            err |= 1 << TE_TRNG_REP_TEST_ERR_MASK;
                        }
                        if (trng_int_state.vn_err) {
                            err |= 1 << TE_TRNG_VN_ERR_MASK;
                        }
                    }
                    ret = rnp->state(rnp, &poll_state);
                    if (TE_SUCCESS != ret) {
                        goto __cleanup__;
                    }
                    if (poll_state.ac_tst_err) {
                        err |= 1 << TE_TRNG_AUTOCORR_TEST_ERR_MASK;
                    }
                    if (err) {
                        req->on_error(err);
                        err = 0;
                    }
                    break;
                }
                osal_delay_us(WAIT_POOL_FILLED_US);
            }while (true);
        }
        if (TE_SUCCESS != ret) {
            goto __cleanup__;
        }
        req->on_data(req->buf, req->size);
    }
__cleanup__:
    osal_mutex_unlock(ctx->mut);
__out__:
    trng_put_ctx(ctx);
    return ret;
}

static int te_trng_drv_suspend( struct te_crypt_drv* drv, te_suspend_state_t state )
{
    int ret = TE_SUCCESS;
    te_hwa_trng_t *rnp = NULL;
    te_hwa_trngctl_t *trng_ctrl = NULL;
    te_trng_drv_t *trng_drv = NULL;

    (void) state;
    if (NULL == drv) {
        return TE_ERROR_BAD_PARAMS;
    }
    trng_drv = (te_trng_drv_t *)drv;
    rnp = GET_TRNG_HWA(trng_drv);
    TE_ASSERT(NULL != rnp);
    ret = rnp->get_int_msk(rnp, &trng_drv->pm->rnp_int_mask);
    if (TE_SUCCESS != ret) {
        return ret;
    }
    if (0 == te_hwa_host_id(rnp->base.host)) {
        trng_ctrl = TRNG_CTX_GET_CTL(TRNG_GET_CTX(trng_drv));
        TE_ASSERT(NULL != trng_ctrl);
        ret = trng_ctrl->get_conf(trng_ctrl, &trng_drv->pm->conf);
        if (TE_SUCCESS != ret) {
            return ret;
        }
        ret = trng_ctrl->get_int_msk(trng_ctrl, &trng_drv->pm->int_mask);
    }

    return ret;
}

static int te_trng_drv_resume( struct te_crypt_drv* drv, te_suspend_state_t state )
{
    int ret = TE_SUCCESS;
    te_hwa_trng_t *rnp = NULL;
    te_hwa_trngctl_t *trng_ctrl = NULL;
    te_trng_drv_t *trng_drv = NULL;
    trng_drv_ctx_t *ctx = NULL;

    (void) state;
    if (NULL == drv) {
        return TE_ERROR_BAD_PARAMS;
    }
    trng_drv = (te_trng_drv_t *)drv;
    rnp = GET_TRNG_HWA(trng_drv);
    TE_ASSERT(NULL != rnp);
    ret = rnp->set_int_msk(rnp, &trng_drv->pm->rnp_int_mask);
    if (TE_SUCCESS != ret) {
        return ret;
    }
    ctx = (trng_drv_ctx_t *)trng_drv->hctx;
    if (0 == te_hwa_host_id(rnp->base.host)) {
        trng_ctrl = TRNG_CTX_GET_CTL(TRNG_GET_CTX(trng_drv));
        TE_ASSERT(NULL != trng_ctrl);

        ret = trng_gen_hwkey(trng_ctrl);
        if (TE_SUCCESS != ret) {
            return ret;
        }

        ret = _trng_setup_config(trng_drv, ctx, trng_ctrl);
        if (TE_SUCCESS != ret) {
            return ret;
        }

        ret = trng_ctrl->setup(trng_ctrl, &trng_drv->pm->conf);
        if (TE_SUCCESS != ret) {
            return ret;
        }
        ret = trng_ctrl->set_int_msk(trng_ctrl, &trng_drv->pm->int_mask);
        if (TE_SUCCESS != ret) {
            return ret;
        }
    }
    return TE_SUCCESS;
}

static void te_trng_drv_destroy( struct te_crypt_drv* drv )
{
    te_trng_drv_t *trng_drv = NULL;

    TE_ASSERT(NULL != drv);
    trng_drv = (te_trng_drv_t *)drv;
    osal_free(trng_drv->pm);
    osal_memset(trng_drv, 0x00, sizeof(*trng_drv));
}

static int _trng_setup_config(te_trng_drv_t *drv,
                               trng_drv_ctx_t *ctx,
                              const te_hwa_trngctl_t *ctl)
{
   int ret = TE_SUCCESS;
    te_trng_conf_t trng_conf = {
        .src = {
            .grp0_en = 1,
            .grp1_en = 1,
            .grp2_en = 1,
            .grp3_en = 1,
            .src_sel = ENTROPY_SRC_INTERNAL,
        },
        .sample = {
            .div = 100,
            .dly = 3,
        },
        .ro = {
            .grp0_taps_en = 4,
            .grp1_taps_en = 5,
            .grp2_taps_en = 6,
            .grp3_taps_en = 7,
        },
        .postproc = {
            .prng_bypass = 1,
            .vn_bypass = 0,
            .crng_bypass = 0,
            .rsvd = 0,
            .lfsr_drop_num = 0,
            .lfsr_sel = 0,
            .fast_mode = 1,
        },
        .eval = {
            .adap_tst_th = 589,
            .rep_tst_th = 11,
            .adap_tst_en = 1,
            .rep_tst_en = 1,
            .ac_tst_en = 1,
            .ac_tst_th = 10,
        },
        .thr = {
            .vn_err_th = 1,
            .crng_err_th = 1,
            .rep_err_th = 1,
            .adap_err_th = 1,
        }
    };
    te_trng_int_t int_mask = {
        .vn_err = 1,
        .crng_err = 1,
        .rep_tst_err = 1,
        .adap_tst_err = 1,
    };

    drv->dump = trng_dump;
    ctx->ctl = (te_hwa_crypt_t *)&ctl->base;
    ret = ctl->setup((te_hwa_trngctl_t *)ctl, &trng_conf);
    __TRNG_DRV_CHECK_CONDITION__(ret);
    ret = ctl->set_int_msk((te_hwa_trngctl_t *)ctl, &int_mask);
    __TRNG_DRV_CHECK_CONDITION__(ret);
__out__:
    return ret;
}

int te_trng_drv_init( te_trng_drv_t *drv,
                      const te_hwa_trng_t *rnp,
                      const te_hwa_trngctl_t *ctl,
                      const char* name )
{
    int ret = TE_SUCCESS;
    trng_drv_ctx_t *ctx = NULL;
    te_rnp_int_t rnp_int_mask = {
        .fill_done = 1,
    };

    __TRNG_DRV_VERIFY_PARAMS__(drv);
    __TRNG_DRV_VERIFY_PARAMS__(rnp);

    if ((TRNG_DRV_MAGIC == drv->magic)
        && osal_atomic_load(&drv->base.refcnt)) {
        _TRNG_DRV_OUT_;
    }

    osal_memset(drv, 0, sizeof(te_trng_drv_t));
    drv->magic = TRNG_DRV_MAGIC;
    drv->base.hwa = (te_hwa_crypt_t *)&rnp->base;
    drv->base.resume = te_trng_drv_resume;
    drv->base.suspend = te_trng_drv_suspend;
    drv->base.destroy = te_trng_drv_destroy;

    if ( NULL != name ) {
        osal_strncpy(drv->base.name, name, TE_MAX_DRV_NAME - 1);
    }

    ctx = (trng_drv_ctx_t *)osal_calloc(1, sizeof(*ctx));
    if( NULL == ctx ) {
        ret = TE_ERROR_OOM;
        _TRNG_DRV_OUT_;
    }

    drv->pm = (trng_pm_ctx_t *)osal_calloc(1, sizeof(*drv->pm));
    if( NULL == drv->pm ) {
        ret = TE_ERROR_OOM;
        goto err_calloc_pm;
    }
    ret = osal_mutex_create(&ctx->mut);
    if (OSAL_SUCCESS != ret) {
         goto __cleanup__;
    }
    rnp->set_int_msk((te_hwa_trng_t *)rnp, &rnp_int_mask);
    ctx->magic = TRNG_CTX_MAGIC;
    ctx->base.drv = &drv->base;
    drv->hctx = (te_ctx_handle)ctx;

    if (0 == te_hwa_host_id(rnp->base.host)) {
        if ( NULL == ctl) {
            goto __cleanup1__;
        }

        ret = trng_gen_hwkey(ctl);
        if (TE_SUCCESS != ret) {
            goto __cleanup1__;
        }

        ret = _trng_setup_config(drv, ctx, ctl);
        if (TE_SUCCESS != ret) {
            goto __cleanup1__;
        }
    }

    trng_get_ctx(ctx);
    te_crypt_drv_get(&drv->base);
    _TRNG_DRV_OUT_;
__cleanup1__:
    osal_mutex_destroy(ctx->mut);
__cleanup__:
    osal_free(drv->pm);
err_calloc_pm:
    osal_free(ctx);
__out__:
    return ret;
}

int te_trng_drv_exit( te_trng_drv_t *drv )
{
    int ret = TE_SUCCESS;

    __TRNG_DRV_VERIFY_PARAMS__(drv);

    if (TRNG_DRV_MAGIC != drv->magic) {
        ret = TE_ERROR_BAD_FORMAT;
        _TRNG_DRV_OUT_;
    }

    trng_put_ctx((trng_drv_ctx_t *)drv->hctx);
    te_crypt_drv_put(&drv->base);
__out__:
    return ret;
}

int te_trng_read( te_trng_drv_t *drv,
                  uint8_t *buf,
                  size_t len )
{
    int ret = TE_SUCCESS;
    te_hwa_trng_t *rnp = NULL;
    te_rnp_ctl_t _ctl = {0};
    size_t offset = 0;
    size_t _len = 0;
    te_rnp_int_t state = {0};
    te_hwa_trngctl_t *ctl = NULL;
    te_trng_int_t _state = {0};
    trng_drv_ctx_t *ctx = NULL;

    __TRNG_DRV_VERIFY_PARAMS__(drv);
    __TRNG_DRV_VERIFY_PARAMS__(buf);

    if (TRNG_DRV_MAGIC != drv->magic) {
        return TE_ERROR_BAD_FORMAT;
    }

    ctx = TRNG_GET_CTX(drv);
    trng_get_ctx(ctx);
    rnp = (te_hwa_trng_t *)drv->base.hwa;
    TE_ASSERT(NULL != rnp);
    osal_mutex_lock(ctx->mut);
    while (offset < len) {
        _len = (len - offset) > TRNG_POOL_SIZE ? TRNG_POOL_SIZE : len - offset;
        _ctl.clk_en = 1;
        _ctl.fill_req = 1;
        ret = rnp->set_ctl(rnp, &_ctl);
        if (TE_SUCCESS != ret) {
            goto __cleanup__;
        }
        do{
            ret = rnp->int_state(rnp, &state);
            if (TE_SUCCESS != ret) {
                goto __cleanup__;
            }
            rnp->eoi(rnp, &state);
            if(state.fill_done){
                ret = rnp->read(rnp, buf + offset, _len);
                if (TE_SUCCESS != ret) {
                    goto __cleanup__;
                }

                offset += _len;
                if (0 == te_hwa_host_id(rnp->base.host)) {
                    ctl = TRNG_CTX_GET_CTL(TRNG_GET_CTX(drv));
                    ret = ctl->int_state(ctl, &_state);
                    if (TE_SUCCESS != ret) {
                        goto __cleanup__;
                    }
                    ctl->eoi(ctl, &_state);
                    if (_state.adap_tst_err || _state.crng_err
                       || _state.rep_tst_err || _state.vn_err){
                        // TODO will add some logic for trng health check
                        // ret = ctl->get_err_cnt(ctl, );
                    }
                }
                break;
            }
            osal_delay_us(WAIT_POOL_FILLED_US);
        }while (true);
    }
__cleanup__:
    /**< once dynamic clock ctrl enable, gate clock when unused */
#ifdef CFG_TE_DYNCLK_CTL
    _ctl.clk_en = 0;
    _ctl.fill_req = 0;
    rnp->set_ctl(rnp, &_ctl);
#endif
    osal_mutex_unlock(ctx->mut);
    trng_put_ctx(ctx);
__out__:
    return ret;
}
