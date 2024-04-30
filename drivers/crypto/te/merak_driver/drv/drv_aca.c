//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <driver/te_drv_aca.h>
#include "drv_aca_internal.h"
#include "drv_internal.h"

#ifndef CFG_TE_DYNCLK_CTL
static int _aca_hwa_clock_enable(const te_hwa_aca_t *aca_hwa)
{
    int ret            = TE_SUCCESS;
    te_aca_ctrl_t ctrl = {0};

    /* init ctrl to reset */
    ctrl.fifo_wm      = 0;
    ctrl.clock_enable = 1;
    ret               = aca_hwa->set_ctrl((te_hwa_aca_t *)aca_hwa,
                            (const te_aca_ctrl_t *)&ctrl);
    TE_ASSERT(TE_SUCCESS == ret);

    return ret;
}

static void _aca_hwa_clock_disable(const te_hwa_aca_t *aca_hwa)
{
    int ret            = TE_SUCCESS;
    te_aca_ctrl_t ctrl = {0};

    /* init aca engine to clock gating */
    ctrl.clock_enable = 0;
    ret               = aca_hwa->set_ctrl((te_hwa_aca_t *)aca_hwa,
                            (const te_aca_ctrl_t *)&ctrl);
    TE_ASSERT(TE_SUCCESS == ret);

    return;
}
#endif

static int _te_aca_drv_suspend(struct te_crypt_drv *drv, te_suspend_state_t state)
{
    int ret               = TE_SUCCESS;
    te_aca_drv_t *aca_drv = (te_aca_drv_t *)drv;
    unsigned long flag    = 0;
    bool is_busy          = false;
#ifdef CFG_TE_DYNCLK_CTL
    bool is_clock_en = false;
#endif

    TE_ASSERT(aca_drv);
    TE_ASSERT(ACA_DRV_MAGIC == aca_drv->magic);

    /* when this function is called, MUST be in locked */
    flag = osal_intr_lock();

    /* swapped all sram */
    if (TE_SUSPEND_MEM == state) {
        ret = aca_sram_swap_all_blocks_nolock(&aca_drv->priv_drv->sram_pool);
        CHECK_RET_GO;
    }

    is_busy = aca_gr_is_busy(&aca_drv->priv_drv->gr_pool);
    CHECK_COND_GO(!is_busy, TE_ERROR_BUSY);

    is_busy = aca_len_type_is_busy(&aca_drv->priv_drv->len_type_pool);
    CHECK_COND_GO(!is_busy, TE_ERROR_BUSY);

#ifdef CFG_TE_DYNCLK_CTL
    /* check dynamic clock status */
    ret = ACA_DRV_GET_HWA(aca_drv)->dynamic_clock_status(
        ACA_DRV_GET_HWA(aca_drv), &is_clock_en);
    OSAL_ASSERT(TE_SUCCESS == ret);
    CHECK_COND_GO(!is_clock_en, TE_ERROR_BUSY);
#else
    _aca_hwa_clock_disable(ACA_DRV_GET_HWA(aca_drv));
#endif

finish:
    osal_intr_unlock(flag);
    return ret;
}

static int _te_aca_drv_resume(struct te_crypt_drv *drv, te_suspend_state_t state)
{
    int ret               = TE_SUCCESS;
    te_aca_drv_t *aca_drv = (te_aca_drv_t *)drv;
    unsigned long flag    = 0;
    bool is_busy          = false;
#ifdef CFG_TE_DYNCLK_CTL
    bool is_clock_en = false;
#endif

    (void)state;
    TE_ASSERT(aca_drv);
    TE_ASSERT(ACA_DRV_MAGIC == aca_drv->magic);

    flag = osal_intr_lock();

    is_busy = aca_gr_is_busy(&aca_drv->priv_drv->gr_pool);
    CHECK_COND_GO(!is_busy, TE_ERROR_BUSY);

    is_busy = aca_len_type_is_busy(&aca_drv->priv_drv->len_type_pool);
    CHECK_COND_GO(!is_busy, TE_ERROR_BUSY);

#ifdef CFG_TE_DYNCLK_CTL
    /* check dynamic clock status. here use assert */
    ret = ACA_DRV_GET_HWA(aca_drv)->dynamic_clock_status(
        ACA_DRV_GET_HWA(aca_drv), &is_clock_en);
    OSAL_ASSERT(TE_SUCCESS == ret);
    OSAL_ASSERT(!is_clock_en);
#else
    ret = _aca_hwa_clock_enable(ACA_DRV_GET_HWA(aca_drv));
    CHECK_RET_GO;
#endif

finish:
    osal_intr_unlock(flag);
    return ret;
}

static void _te_aca_drv_destroy(struct te_crypt_drv *drv)
{
    te_aca_drv_t *aca_drv    = (te_aca_drv_t *)drv;
    aca_priv_drv_t *priv_drv = NULL;
#ifdef CFG_TE_DYNCLK_CTL
    int ret                  = TE_SUCCESS;
    bool is_clock_en = false;
#endif

    TE_ASSERT(aca_drv);
    TE_ASSERT(ACA_DRV_MAGIC == aca_drv->magic);
    TE_ASSERT(0 == osal_atomic_load(&aca_drv->base.refcnt));

    OSAL_LOG_INFO("Destroy ACA driver...\n");

    /* destroy this driver */
    priv_drv = (aca_priv_drv_t *)(aca_drv->priv_drv);

#ifdef CFG_TE_DYNCLK_CTL
    /* check dynamic clock status. here use assert */
    ret = ACA_DRV_GET_HWA(aca_drv)->dynamic_clock_status(
        ACA_DRV_GET_HWA(aca_drv), &is_clock_en);
    OSAL_ASSERT(TE_SUCCESS == ret);
    OSAL_ASSERT(!is_clock_en);
#else
    _aca_hwa_clock_disable((const te_hwa_aca_t *)(ACA_DRV_GET_HWA(aca_drv)));
#endif

    aca_op_cleanup(&priv_drv->op);
    aca_drv_cleanup_len_type_pool(&priv_drv->len_type_pool);
    aca_drv_cleanup_gr_pool(&priv_drv->gr_pool);
    aca_drv_cleanup_sram_pool(&priv_drv->sram_pool);
    aca_pk_cleanup(&priv_drv->pk);
    OSAL_SAFE_FREE(priv_drv);
    memset(aca_drv, 0, sizeof(te_aca_drv_t));

    OSAL_LOG_INFO("ACA driver exits!\n");

    return;
}

int te_aca_drv_init(te_aca_drv_t *aca_drv,
                    const te_hwa_aca_t *aca_hwa,
                    const char *name)
{
    int ret                  = TE_SUCCESS;
    aca_priv_drv_t *priv_drv = NULL;

    CHECK_PARAM(aca_drv && aca_hwa);
    if (aca_drv->magic == ACA_DRV_MAGIC &&
        osal_atomic_load(&aca_drv->base.refcnt)) {
        /* already initialized, add reference count */
        return te_crypt_drv_get(&aca_drv->base);
    }
    memset(aca_drv, 0, sizeof(te_aca_drv_t));
    priv_drv = (aca_priv_drv_t *)osal_calloc(1, sizeof(aca_priv_drv_t));
    if (!priv_drv) {
        ret = TE_ERROR_OOM;
        goto __err0;
    }

    /* init pk */
    ret = aca_pk_init(&priv_drv->pk);
    if (TE_SUCCESS != ret) {
        goto __err1;
    }
    /* init sram pool */
    ret = aca_drv_init_sram_pool(&priv_drv->sram_pool, aca_hwa);
    if (TE_SUCCESS != ret) {
        goto __err2;
    }

    /* init gr pool */
    ret = aca_drv_init_gr_pool(&priv_drv->gr_pool, aca_hwa);
    if (TE_SUCCESS != ret) {
        goto __err3;
    }

    /* init len type pool */
    ret = aca_drv_init_len_type_pool(&priv_drv->len_type_pool, aca_hwa);
    if (TE_SUCCESS != ret) {
        goto __err4;
    }

    /* init operation */
    ret = aca_op_init(&priv_drv->op, aca_hwa);
    if (TE_SUCCESS != ret) {
        goto __err5;
    }

#ifndef CFG_TE_DYNCLK_CTL
    /* init clock */
    ret = _aca_hwa_clock_enable((const te_hwa_aca_t *)(aca_hwa));
    if (TE_SUCCESS != ret) {
        goto __err6;
    }
#endif

    aca_drv->base.hwa = (te_hwa_crypt_t *)aca_hwa;
    if (NULL != name) {
        osal_strncpy(aca_drv->base.name, name, TE_MAX_DRV_NAME - 1);
    }

    aca_drv->priv_drv = (struct aca_priv_drv *)priv_drv;

    /* reset refcnt */
    osal_atomic_store(&aca_drv->base.refcnt, 0U);
    /* install hooks */
    aca_drv->base.suspend = _te_aca_drv_suspend;
    aca_drv->base.resume  = _te_aca_drv_resume;
    aca_drv->base.destroy = _te_aca_drv_destroy;

    aca_drv->magic = ACA_DRV_MAGIC;

    ret = te_crypt_drv_get(&aca_drv->base);
    if (TE_SUCCESS != ret) {
        goto __err7;
    }

    OSAL_LOG_INFO(" ACA driver init success!\n");

    return TE_SUCCESS;
__err7:
#ifndef CFG_TE_DYNCLK_CTL
    _aca_hwa_clock_disable(aca_hwa);
__err6:
#endif
    aca_op_cleanup(&priv_drv->op);
__err5:
    aca_drv_cleanup_len_type_pool(&priv_drv->len_type_pool);
__err4:
    aca_drv_cleanup_gr_pool(&priv_drv->gr_pool);
__err3:
    aca_drv_cleanup_sram_pool(&priv_drv->sram_pool);
__err2:
    aca_pk_cleanup(&priv_drv->pk);
__err1:
    OSAL_SAFE_FREE(priv_drv);
__err0:
    memset(aca_drv, 0, sizeof(te_aca_drv_t));
    return ret;
}

int te_aca_drv_exit(te_aca_drv_t *aca_drv)
{
    CHECK_PARAM(aca_drv && (ACA_DRV_MAGIC == aca_drv->magic));

    TE_ASSERT(1 == osal_atomic_load(&aca_drv->base.refcnt));
    te_crypt_drv_put(&aca_drv->base);

    return TE_SUCCESS;
}

int te_aca_bn_get_drv(const te_crypt_drv_t **drv, const te_aca_bn_t *bn)
{
    aca_drv_ctx_t *bn_ctx = (aca_drv_ctx_t *)bn;

    CHECK_PARAM(drv);
    BN_CHECK(bn);

    *drv = (const te_crypt_drv_t *)(bn_ctx->aca_drv);
    return TE_SUCCESS;
}
