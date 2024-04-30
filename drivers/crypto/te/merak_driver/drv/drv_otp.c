//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <driver/te_drv_otp.h>
#include <hwa/te_hwa_otp.h>
#include <hwa/te_hwa_otpctl.h>
#include <hwa/te_hwa.h>
#include "drv_internal.h"

#define _OTP_DRV_OUT_           goto __out__

#define __OTP_DRV_CHECK_CONDITION__(_ret_)                                     \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 _OTP_DRV_OUT_;                                                \
            }                                                                  \
        } while (0);

#define __OTP_DRV_ALERT__(_ret_, _msg_)                                        \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X %s\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_),                                       \
                                (_msg_));                                      \
            }                                                                  \
        } while (0);

#define __OTP_DRV_VERIFY_PARAMS__(_param_)                                     \
        do                                                                     \
        {                                                                      \
            if(NULL == (_param_)){                                             \
                ret = TE_ERROR_BAD_PARAMS;                                     \
                _OTP_DRV_OUT_;                                                 \
            }                                                                  \
        } while (0)

/**
 * Gerneral control
 */
#define DRV_OTP_GNRCTL_PTA_SHIFT          (0x00U)
#define DRV_OTP_GNRCTL_PTA_SIZE           (0x02U)
#define DRV_OTP_GNRCTL_PTA_MASK           (0x03U)

#define DRV_OTP_GNRCTL_PAPUF_SHIFT        (DRV_OTP_GNRCTL_PTA_SHIFT +       \
                                            DRV_OTP_GNRCTL_PTA_SIZE)
#define DRV_OTP_GNRCTL_PAPUF_SIZE         (0x01U)
#define DRV_OTP_GNRCTL_PAPUF_MASK         (0x01U)

#define DRV_OTP_GNRCTL_PRESETN_SHIFT      (DRV_OTP_GNRCTL_PAPUF_SHIFT +     \
                                            DRV_OTP_GNRCTL_PAPUF_SIZE)
#define DRV_OTP_GNRCTL_PRESETN_SIZE       (0x01U)
#define DRV_OTP_GNRCTL_PDSTB_SHIFT        (DRV_OTP_GNRCTL_PRESETN_SHIFT +   \
                                            DRV_OTP_GNRCTL_PRESETN_SIZE)
#define DRV_OTP_GNRCTL_PDSTB_SIZE         (0x01U)
#define DRV_OTP_GNRCTL_PENVDD2_SHIFT      (DRV_OTP_GNRCTL_PDSTB_SHIFT +     \
                                            DRV_OTP_GNRCTL_PDSTB_SIZE)
#define DRV_OTP_GNRCTL_PENVDD2_SIZE       (0x01U)

#define OTP_WORD_SIZE                   (0x04U)
#define OTP_LCS_MASK                    (0x07U)
#define OTP_INVALID_MASK                (0x08U)

#define OTP_SKEY_MAX_SIZE               (32U)

/**
 * otp layout
 */
#define OTP_LAYOUT_MODLE_ID                         (0x00U)
#define OTP_LAYOUT_MODLE_KEY                        (0x01U)
#define OTP_LAYOUT_DEVICE_ID                        (0x02U)
#define OTP_LAYOUT_DEVICE_RK                        (0x03U)
#define OTP_LAYOUT_SEC_BOOT_HASH                    (0x04U)
#define OTP_LAYOUT_LCS                              (0x05U)
#define OTP_LAYOUT_LOCK_CTRL                        (0x06U)
#define OTP_LAYOUT_USER_NON_SEC_REGION              (0x07U)
#define OTP_LAYOUT_USER_SEC_REGION                  (0x08U)
#define OTP_LAYOUT_TEST_REGION                      (0x09U)

#define LOCATE_LAYOUT(_offset)                                                 \
        ( (_offset) >= TE_OTP_TST_OFFSET ?                                     \
        OTP_LAYOUT_TEST_REGION :                                               \
                (_offset) >= TE_OTP_SEC_OFFSET ?                               \
        OTP_LAYOUT_USER_SEC_REGION :                                           \
                (_offset) >= TE_OTP_NSEC_OFFSET ?                              \
        OTP_LAYOUT_USER_NON_SEC_REGION :                                       \
                (_offset) >= TE_OTP_LOCK_CTRL_OFFSET ?                         \
        OTP_LAYOUT_LOCK_CTRL : (_offset) >= TE_OTP_LCS_OFFSET ?                \
        OTP_LAYOUT_LCS : (_offset) >= TE_OTP_SEC_BOOT_HASH_OFFSET ?            \
        OTP_LAYOUT_SEC_BOOT_HASH : (_offset) >= TE_OTP_DEVICE_RK_OFFSET ?      \
        OTP_LAYOUT_DEVICE_RK : (_offset) >= TE_OTP_DEVICE_ID_OFFSET?           \
        OTP_LAYOUT_DEVICE_ID : (_offset) >= TE_OTP_MODEL_KEY_OFFSET?           \
        OTP_LAYOUT_MODLE_KEY : OTP_LAYOUT_MODLE_ID)

#define __BIT( n ) ( 1UL << ( n ) )
#define __BIT_MASK( V, n ) ( (V) & __BIT( n ) )
#define SUB_REGION_b_SIZE        (128)
#define B_b_SIZE                 (8)
#define SUB_REGION_B_SIZE        (SUB_REGION_b_SIZE / B_b_SIZE)

#define SECTION_MATCH(_locks, _offset, _len)                                    \
        do{                                                                     \
            size_t _i;                                                          \
            size_t __offset = UTILS_ROUND_DOWN( (_offset), SUB_REGION_B_SIZE ); \
            for( _i = (__offset/SUB_REGION_B_SIZE );                            \
                 _i < UTILS_ROUND_UP(( _offset) + (_len), SUB_REGION_B_SIZE ) / \
                                SUB_REGION_B_SIZE;                              \
                 _i++ ){                                                        \
                if( __BIT_MASK(_locks, _i ) ){                                  \
                    b_lock = true;                                              \
                    break;                                                      \
                }                                                               \
            }                                                                   \
        }while (0)
/**
 * LCS define
 */
#define LCS_CM                  (0x00U)
#define LCS_DM                  (0x01U)
#define LCS_DD                  (0x03U)
#define LCS_DR                  (0x07U)
/**
 * OTP READ/WRITE operation define
 */
#define OTP_OP_READ             (0x00U)
#define OTP_OP_WRITE            (0x01U)


#define OTP_READ_MODE_SHADOW        (0x00)
#define OTP_READ_MODE_DIRECT        (0x01)
/**
 * Derive the OTP CTL hwa pointer from the driver context
 */
#define GET_OTP_CTL(_drv) __extension__({                 \
    te_hwa_otpctl_t *_ctl = NULL;                         \
    otp_drv_ctx_t *_ctx = (otp_drv_ctx_t*)(_drv)->hctx;   \
    TE_ASSERT(_ctx != NULL);                              \
    _ctl = (te_hwa_otpctl_t *)(_ctx->ctl);                \
    TE_ASSERT(_ctl != NULL);                              \
    _ctl;                                                 \
})

/**
 * Derive the OTP hwa pointer from the driver context
 */
#define GET_OTP_HWA(_drv) __extension__({                 \
    te_hwa_otp_t *_hwa = NULL;                            \
    _hwa = (te_hwa_otp_t *)(_drv->base.hwa);              \
    TE_ASSERT(_hwa != NULL);                              \
    _hwa;                                                 \
})

/**
 * Get the OTP ctx's drv
 */
#define GET_OTP_CTX_DRV(_ctx) __extension__({             \
    te_otp_drv_t *_drv = NULL;                            \
    _drv = (te_otp_drv_t *)(_ctx->base.drv);              \
    TE_ASSERT(_drv != NULL);                              \
    _drv;                                                 \
})

/**
 * Get the OTP drv's host
 */
#define GET_OTP_DRV_HOST(_drv) __extension__({            \
    te_hwa_host_t *_host = NULL;                          \
    _host = (te_hwa_host_t *)(_drv->base.hwa->host);      \
    TE_ASSERT(_host != NULL);                             \
    _host;                                                \
})

/**
 * Get the OTP drv's otp ctx
 */
#define GET_OTP_DRV_CTX(_drv) __extension__({            \
    otp_drv_ctx_t *_ctx = NULL;                          \
    _ctx = (otp_drv_ctx_t *)(_drv->hctx);                \
    TE_ASSERT(_ctx != NULL);                             \
    _ctx;                                                \
})

#define DELAY_US           (10)

/**
 * OTP context magic number
 */
#define OTP_CTX_MAGIC   0x4370544fU /**< "OTpC" */

typedef struct te_otp_pm_ctx {
    te_otp_dummy_t dummy;
    te_otp_ctl_t otp_ctl;
} otp_pm_ctx_t;

/**
 * Trust engine OTP context structure
 * Single instance
 */
typedef struct otp_drv_ctx {
    te_crypt_ctx_t base;            /**< base context */
    uint32_t magic;                 /**< OTP ctx magic */
    osal_mutex_t mut;               /**< exclusive lock */
    osal_atomic_t refcnt;           /**< reference count */
    te_hwa_crypt_t *ctl;            /**< OTP CTL HWA */
    te_otp_conf_t cfg;              /**< OTP configuration */
    void* vops[];                   /**< vendor specific ops ptr */
} otp_drv_ctx_t;

typedef union _te_otp_lock_t {
    uint32_t value;
    struct {
        uint32_t model_id : 1;
        uint32_t model_key : 1;
        uint32_t device_id : 1;
        uint32_t device_root_key : 1;
        uint32_t secure_boot_pk_hash : 1;
        uint32_t reserve : 3;
        uint32_t usr_nonsec_region : 8;
        uint32_t usr_sec_region : 16;
    } locks;
} te_otp_lock_t;

static te_otp_conf_t g_otp_cfg;

static int otp_get_ctx( otp_drv_ctx_t *ctx )
{
    osal_atomic_inc(&ctx->refcnt);
    return TE_SUCCESS;
}

static void _otp_destroy_ctx(otp_drv_ctx_t *ctx)
{
    osal_mutex_destroy(ctx->mut);
    osal_memset(ctx, 0x00, sizeof(otp_drv_ctx_t));
}

static int otp_put_ctx( otp_drv_ctx_t *ctx )
{
    if (0 == osal_atomic_dec(&ctx->refcnt)) {
        _otp_destroy_ctx(ctx);
        osal_free(ctx);
    }

    return TE_SUCCESS;
}

static int _otp_get_lcs(otp_drv_ctx_t *ctx)
{
    int32_t lcs = -1;
    te_hwa_otp_t *otp = GET_OTP_HWA(GET_OTP_CTX_DRV(ctx));
    te_hwa_host_t *host = GET_OTP_DRV_HOST(GET_OTP_CTX_DRV(ctx));
    te_hwa_otpctl_t *ctl = NULL;
    te_otp_dummy_t otp_dummy = {0};
    int ret = TE_SUCCESS;

    if (ctx->cfg.otp_exist) {
        ret = otp->read(otp, TE_OTP_LCS_OFFSET, (uint8_t *)&lcs, sizeof(lcs));
        __OTP_DRV_CHECK_CONDITION__(ret);
        lcs &= OTP_LCS_MASK;
    } else {
        if (0 != te_hwa_host_id(host)) {
            ret = TE_ERROR_ACCESS_DENIED;
            _OTP_DRV_OUT_;
        }
        ctl = GET_OTP_CTL(GET_OTP_CTX_DRV(ctx));
        ret = ctl->get_dummy(ctl, &otp_dummy);
        __OTP_DRV_CHECK_CONDITION__(ret);
        if (otp_dummy.conf.lcs_valid) {
            lcs = otp_dummy.conf.lcs_dr ? LCS_DR :
                  otp_dummy.conf.lcs_dd ? LCS_DD :
                  otp_dummy.conf.lcs_dm ? LCS_DM :
                  otp_dummy.conf.lcs_cm ? LCS_CM :
                  TE_ERROR_NO_DATA;
        } else {
            lcs = TE_ERROR_NO_DATA;
        }
    }
    ret = lcs;
__out__:
    return ret;
}

#ifdef CFG_OTP_WITH_PUF
#define TPENS           (2U)
#define TPENH           (3U)
#define TASH            (11U)
#define TSAS            (3U)

static void _te_otp_puf_power_on(te_hwa_otpctl_t *ctl)
{
    te_otp_ctl_t ctl_action = {0};
    ctl->get_ctl(ctl, &ctl_action);
    ctl_action.general_ctl |= 1 << DRV_OTP_GNRCTL_PENVDD2_SHIFT;
    ctl->set_ctl(ctl, &ctl_action);
    osal_delay_us(TPENS);
    ctl_action.general_ctl |= 1 << DRV_OTP_GNRCTL_PDSTB_SHIFT;
    ctl->set_ctl(ctl, &ctl_action);
    osal_delay_us(TPENH);
    ctl_action.general_ctl |= 1 << DRV_OTP_GNRCTL_PRESETN_SHIFT;
    ctl->set_ctl(ctl, &ctl_action);
}

static int _otp_puf_suspend(te_ctx_handle h, te_suspend_state_t state)
{
    int ret = TE_SUCCESS;
    otp_drv_ctx_t *ctx = (otp_drv_ctx_t *)h;
    te_hwa_otpctl_t *ctl = NULL;
    te_otp_ctl_t ctl_action = {0};

    TE_ASSERT(NULL != ctx);
    if (0 != te_hwa_host_id(GET_OTP_DRV_HOST(GET_OTP_CTX_DRV(ctx)))) {
        ret = TE_ERROR_ACCESS_DENIED;
        _OTP_DRV_OUT_;
    }

#ifndef CFG_OTP_PUF_STB_EN
    /* Do nothing with standby disabled */
    if (TE_SUSPEND_STANDBY == state) {
        _OTP_DRV_OUT_;
    }
#endif
    /* Standby sequence */
    ctl = GET_OTP_CTL(GET_OTP_CTX_DRV(ctx));
    ret = ctl->get_ctl(ctl, &ctl_action);
    __OTP_DRV_CHECK_CONDITION__(ret);
    ctl_action.general_ctl &= ~(1 << DRV_OTP_GNRCTL_PRESETN_SHIFT);
    ret = ctl->set_ctl(ctl, &ctl_action);
    osal_delay_us(TASH);
    ctl_action.general_ctl &= ~(1 << DRV_OTP_GNRCTL_PDSTB_SHIFT);
    ret = ctl->set_ctl(ctl, &ctl_action);

    /* Power down, on demand */
    if (TE_SUSPEND_MEM == state) {
        ctl_action.general_ctl &= ~(1 << DRV_OTP_GNRCTL_PENVDD2_SHIFT);
        ret = ctl->set_ctl(ctl, &ctl_action);
    }
__out__:
    return ret;
}

static int _otp_puf_resume(te_ctx_handle h, te_suspend_state_t state)
{
    int ret = TE_SUCCESS;
    otp_drv_ctx_t *ctx = (otp_drv_ctx_t *)h;
    te_hwa_otpctl_t *ctl = NULL;
    te_otp_ctl_t ctl_action = {0};

    TE_ASSERT(NULL != ctx);
    if (0 != te_hwa_host_id(GET_OTP_DRV_HOST(GET_OTP_CTX_DRV(ctx)))) {
        ret = TE_ERROR_ACCESS_DENIED;
        _OTP_DRV_OUT_;
    }

#ifndef CFG_OTP_PUF_STB_EN
    /* Do nothing with standby disabled */
    if (TE_SUSPEND_STANDBY == state) {
        _OTP_DRV_OUT_;
    }
#endif

    ctl = GET_OTP_CTL(GET_OTP_CTX_DRV(ctx));
    TE_ASSERT(NULL != ctl);
    ret = ctl->get_ctl(ctl, &ctl_action);
    __OTP_DRV_CHECK_CONDITION__(ret);

    if (TE_SUSPEND_MEM == state) {
        ctl_action.general_ctl |= 1 << DRV_OTP_GNRCTL_PENVDD2_SHIFT;
        ctl->set_ctl(ctl, &ctl_action);
        osal_delay_us(TPENS);
    }

    ctl_action.general_ctl |= 1 << DRV_OTP_GNRCTL_PDSTB_SHIFT;
    ret = ctl->set_ctl(ctl, &ctl_action);
    __OTP_DRV_CHECK_CONDITION__(ret);
    osal_delay_us(TSAS);
    ctl_action.general_ctl |= 1 << DRV_OTP_GNRCTL_PRESETN_SHIFT;
    ret = ctl->set_ctl(ctl, &ctl_action);

__out__:
    return ret;
}

static int otp_puf_enroll(te_ctx_handle h)
{
#define PUF_ENROLL_SEED     (uint8_t *)"\x12\x34\x56\x78"
    int ret = TE_SUCCESS;
    otp_drv_ctx_t *ctx = (otp_drv_ctx_t *)h;
    te_hwa_otpctl_t *ctl = NULL;
    te_otp_ctl_t ctl_action = {0};
    te_otp_ctl_t old_ctl = {0};
    te_hwa_otp_t *otp = NULL;
    te_otp_lock_t lock = {0};
    uint8_t device_rk[OTP_SKEY_MAX_SIZE] = {0};
    int lcs = 0;

    if (NULL == ctx) {
        return TE_ERROR_BAD_PARAMS;
    }
    if (OTP_CTX_MAGIC != ctx->magic) {
        return TE_ERROR_BAD_FORMAT;
    }
    otp_get_ctx(ctx);
    otp = GET_OTP_HWA(GET_OTP_CTX_DRV(ctx));
    osal_mutex_lock(ctx->mut);
    lcs = _otp_get_lcs(ctx);
    if (0 > lcs) {
        ret = lcs;
        goto err_get_lcs;
    }
    switch (lcs) {
        case LCS_DD:
        case LCS_DR:
            ret = TE_ERROR_SECURITY;
            break;
        default:
            break;
    }
    if (TE_SUCCESS != ret) {
        goto err_lcs;
    }
    ret = otp->read(otp, TE_OTP_LOCK_CTRL_OFFSET, (uint8_t *)&lock, sizeof(lock));
    if (TE_SUCCESS != ret) {
        goto err_rd;
    }
    if (lock.locks.device_root_key) {
        ret = TE_ERROR_ACCESS_DENIED;
        goto err_lock;
    }
    ctl = GET_OTP_CTL(GET_OTP_CTX_DRV(ctx));
    ret = ctl->get_ctl(ctl, &old_ctl);
    if (TE_SUCCESS != ret) {
        goto err_get_ctl;
    }
    ctl_action.direct_rd = 1;
    ctl_action.general_ctl = old_ctl.general_ctl;
    ctl_action.general_ctl &= ~(DRV_OTP_GNRCTL_PTA_MASK << DRV_OTP_GNRCTL_PTA_SHIFT);
    ctl_action.general_ctl &= ~( DRV_OTP_GNRCTL_PAPUF_MASK <<
                           DRV_OTP_GNRCTL_PAPUF_SHIFT );
    ret = ctl->set_ctl(ctl, &ctl_action);
    if (TE_SUCCESS != ret) {
        goto err_set_ctl;
    }
    /*write non all zero non all F to last word of
      device root key to trigger enroll*/
    ret = ctl->write(ctl,
                     TE_OTP_DEVICE_RK_OFFSET + 0x0C,
                     PUF_ENROLL_SEED,
                     OTP_WORD_SIZE);
    if (TE_SUCCESS != ret) {
        goto err_write;
    }
    /*hw require direct read back */
    ret = otp->read(otp,
                    TE_OTP_DEVICE_RK_OFFSET,
                    device_rk,
                    TE_OTP_DEVICE_RK_SIZE);
    if (TE_SUCCESS != ret) {
        goto err_read;
    }
    /**< lock device root key */
    lock.locks.device_root_key = 1;
    ret = ctl->write( ctl, TE_OTP_LOCK_CTRL_OFFSET,
                      (uint8_t *)&lock, sizeof(lock) );
err_read:
err_write:
    ctl->set_ctl(ctl, &old_ctl);
err_lock:
err_set_ctl:
err_get_ctl:
err_rd:
err_lcs:
err_get_lcs:
    osal_mutex_unlock(ctx->mut);
    otp_put_ctx(ctx);
    return ret;
}

static int otp_puf_ready( te_ctx_handle h )
{
    int ret = 0U;
    te_hwa_otpctl_t *ctl = NULL;
    otp_drv_ctx_t *ctx = (otp_drv_ctx_t *)h;
    te_otp_stat_t state = {0};

    __OTP_DRV_VERIFY_PARAMS__(ctx);
    if (OTP_CTX_MAGIC != ctx->magic) {
        ret = TE_ERROR_BAD_FORMAT;
        _OTP_DRV_OUT_;
    }

    if (0 != te_hwa_host_id(GET_OTP_DRV_HOST(GET_OTP_CTX_DRV(ctx)))) {
        ret = TE_ERROR_ACCESS_DENIED;
        _OTP_DRV_OUT_;
    }
    otp_get_ctx(ctx);
    ctl = GET_OTP_CTL(GET_OTP_CTX_DRV(ctx));
    ctl->state(ctl, &state);
    otp_put_ctx(ctx);
    ret = state.otp_rdy ? TE_SUCCESS : TE_ERROR_BUSY;
__out__:
    return ret;
}

static int otp_puf_quality_check(te_ctx_handle h)
{
    int ret = TE_SUCCESS;
    otp_drv_ctx_t *ctx = (otp_drv_ctx_t *)h;
    te_hwa_otpctl_t *ctl = NULL;
    te_otp_ctl_t ctl_action = {0};
    te_otp_ctl_t old_ctl = {0};
    te_hwa_otp_t *otp = NULL;
    uint8_t device_rk[OTP_SKEY_MAX_SIZE] = {0};
    int32_t lcs = 0;

    if(NULL == ctx) {
        return TE_ERROR_BAD_PARAMS;
    }
    if (OTP_CTX_MAGIC != ctx->magic) {
        return TE_ERROR_BAD_FORMAT;
    }
    if (0 != te_hwa_host_id(GET_OTP_DRV_HOST(GET_OTP_CTX_DRV(ctx)))) {
        return TE_ERROR_ACCESS_DENIED;
    }
    otp_get_ctx(ctx);
    otp = GET_OTP_HWA(GET_OTP_CTX_DRV(ctx));
    osal_mutex_lock(ctx->mut);
    lcs = _otp_get_lcs(ctx);
    if (0 > lcs) {
        ret = lcs;
        goto err_get_lcs;
    }
    switch (lcs) {
        case LCS_DD:
        case LCS_DR:
            ret = TE_ERROR_SECURITY;
        default:
            break;
    }
    if (TE_SUCCESS != ret) {
        goto err_lcs;
    }
    ctl = GET_OTP_CTL(GET_OTP_CTX_DRV(ctx));
    ret = ctl->get_ctl(ctl, &old_ctl);
    if (TE_SUCCESS != ret) {
        goto err_get_ctl;
    }
    ctl_action.general_ctl = old_ctl.general_ctl;
    ctl_action.direct_rd = 1; /* set direct read to flush device root key */
    ctl_action.general_ctl |= 0x02 << DRV_OTP_GNRCTL_PTA_SHIFT;
    ctl_action.general_ctl |= 1 << DRV_OTP_GNRCTL_PAPUF_SHIFT;
    ret = ctl->set_ctl(ctl, &ctl_action);
    if (TE_SUCCESS != ret) {
        goto err_set_ctl;
    }
    /* read root key to check quality check result */
    ret = otp->read(otp,
                    TE_OTP_DEVICE_RK_OFFSET,
                    device_rk,
                    TE_OTP_DEVICE_RK_SIZE);
    if (TE_SUCCESS != ret) {
        goto err_read;
    }
    if (0x01 != (device_rk[3] & 0x01)) {
        ret = TE_ERROR_BUSY;
        goto err_qc;
    }
    /*hw require direct read back to flush device root key */
    ctl_action.general_ctl = old_ctl.general_ctl;
    ctl_action.direct_rd = 1; /* set direct read to flush device root key */
    ret = ctl->set_ctl(ctl, &ctl_action);
    if (TE_SUCCESS != ret) {
        goto err_set_ctl2;
    }
    ret = otp->read(otp,
                    TE_OTP_DEVICE_RK_OFFSET,
                    device_rk,
                    TE_OTP_DEVICE_RK_SIZE);

err_set_ctl2:
err_qc:
err_read:
    ctl->set_ctl(ctl, &old_ctl);
err_set_ctl:
err_get_ctl:
err_lcs:
err_get_lcs:
    osal_mutex_unlock(ctx->mut);
    otp_put_ctx(ctx);
    return ret;
}

#define PUF_RD_WAIT (4U)
static int _otp_read( te_otp_drv_t *drv,
	                  size_t off,
	                  uint8_t *buf,
	                  size_t len,
	                  bool b_lock );

static int otp_puf_init_margin_read(te_ctx_handle h, size_t off,
                            uint8_t *buf, size_t len)
{
    int ret = TE_SUCCESS;
    otp_drv_ctx_t *ctx = (otp_drv_ctx_t *)h;
    te_hwa_otpctl_t *ctl = NULL;
    te_otp_ctl_t new_ctl = {0};
    te_otp_ctl_t old_ctl = {0};

    if (NULL == ctx || NULL == buf) {
        return TE_ERROR_BAD_PARAMS;
    }
    if (OTP_CTX_MAGIC != ctx->magic) {
        return TE_ERROR_BAD_FORMAT;
    }
    if (OTP_LAYOUT_DEVICE_RK == LOCATE_LAYOUT(off)) {
        return TE_ERROR_ACCESS_DENIED;
    }
    otp_get_ctx(ctx);
    osal_mutex_lock(ctx->mut);
    ctl = GET_OTP_CTL(GET_OTP_CTX_DRV(ctx));
    ret = ctl->get_ctl(ctl, &old_ctl);
    if (TE_SUCCESS != ret) {
        goto err_get_ctl;
    }
    new_ctl.general_ctl = old_ctl.general_ctl;
    new_ctl.direct_rd = 1;
    new_ctl.general_ctl &= ~( DRV_OTP_GNRCTL_PTA_MASK << DRV_OTP_GNRCTL_PTA_SHIFT |
                           DRV_OTP_GNRCTL_PTA_MASK << DRV_OTP_GNRCTL_PTA_SHIFT );
    new_ctl.general_ctl |= 0x02 << DRV_OTP_GNRCTL_PTA_SHIFT;
    new_ctl.general_ctl &= ~(1 << DRV_OTP_GNRCTL_PTA_SHIFT);
    ret = ctl->set_ctl(ctl, &new_ctl);
    if (TE_SUCCESS != ret) {
        goto err_set_ctl;
    }
    /**< proper delay is requred, otherwise will read failed*/
    osal_delay_us(PUF_RD_WAIT);
    ret = _otp_read(GET_OTP_CTX_DRV(ctx), off, buf, len, false);
    ctl->set_ctl(ctl, &old_ctl);

err_set_ctl:
err_get_ctl:
    osal_mutex_unlock(ctx->mut);
    otp_put_ctx(ctx);
    return ret;
}

static int otp_puf_pgm_margin_read(te_ctx_handle h, size_t off,
                           uint8_t *buf, size_t len)
{
    int ret = TE_SUCCESS;
    otp_drv_ctx_t *ctx = (otp_drv_ctx_t *)h;
    te_hwa_otpctl_t *ctl = NULL;
    te_otp_ctl_t new_ctl = {0};
    te_otp_ctl_t old_ctl = {0};

    if (NULL == ctx || NULL == buf) {
        return TE_ERROR_BAD_PARAMS;
    }
    if (OTP_CTX_MAGIC != ctx->magic) {
        return TE_ERROR_BAD_FORMAT;
    }
    if (OTP_LAYOUT_DEVICE_RK == LOCATE_LAYOUT(off) ) {
        return TE_ERROR_ACCESS_DENIED;
    }
    otp_get_ctx(ctx);
    osal_mutex_lock(ctx->mut);
    ctl = GET_OTP_CTL(GET_OTP_CTX_DRV(ctx));
    ret = ctl->get_ctl(ctl, &old_ctl);
    if (TE_SUCCESS != ret) {
        goto err_get_ctl;
    }
    new_ctl.general_ctl = old_ctl.general_ctl;
    new_ctl.direct_rd = 1;
    new_ctl.general_ctl &= ~0x7;
    new_ctl.general_ctl |= 0x03 << DRV_OTP_GNRCTL_PTA_SHIFT;
    new_ctl.general_ctl &= ~(1 << DRV_OTP_GNRCTL_PAPUF_SHIFT);
    ret = ctl->set_ctl(ctl, &new_ctl);
    if (TE_SUCCESS != ret) {
        goto err_set_ctl;
    }
    /**< proper delay is requred, otherwise will read failed*/
    osal_delay_us(PUF_RD_WAIT);
    ret = _otp_read(GET_OTP_CTX_DRV(ctx), off, buf, len, false);
    ctl->set_ctl(ctl, &old_ctl);

err_set_ctl:
err_get_ctl:
    osal_mutex_unlock(ctx->mut);
    otp_put_ctx(ctx);
    return ret;
}
#endif

static bool _otp_is_region_lock(te_otp_lock_t *lock,
                            size_t offset,
                            size_t len)
{
    bool b_lock = false;
    switch(LOCATE_LAYOUT(offset)){
        case OTP_LAYOUT_USER_NON_SEC_REGION:
            SECTION_MATCH(lock->locks.usr_nonsec_region,
                          (offset - TE_OTP_NSEC_OFFSET),
                          len);
            break;
        case OTP_LAYOUT_USER_SEC_REGION:
            SECTION_MATCH(lock->locks.usr_sec_region,
                          (offset - TE_OTP_SEC_OFFSET),
                          len);
            break;
        case OTP_LAYOUT_MODLE_ID:
            b_lock = (lock->locks.model_id == 1) ? true : false;
            break;
        case OTP_LAYOUT_MODLE_KEY:
            b_lock = (lock->locks.model_key == 1) ? true : false;
            break;
        case OTP_LAYOUT_DEVICE_RK:
            b_lock = (lock->locks.device_root_key == 1) ? true : false;
            break;
        case OTP_LAYOUT_DEVICE_ID:
            b_lock = (lock->locks.device_id == 1) ? true : false;
            break;
        case OTP_LAYOUT_SEC_BOOT_HASH:
            b_lock = (lock->locks.secure_boot_pk_hash == 1) ? true : false;
            break;
        default:
            break;
    }
    return b_lock;
}

static bool _otp_check_permission(uint32_t lcs, uint32_t host_id,
                                  size_t offset, size_t len,
                                  uint32_t op,
                                  te_otp_lock_t *lock)
{
    if(0 != host_id && OTP_OP_WRITE == op){
        return false;
    }
    if (OTP_OP_READ == op) {
        TE_ASSERT(lock == NULL);
    }
    switch (lcs)
    {
    case LCS_CM:
    case LCS_DM:
        if ( OTP_OP_WRITE == op) {
            /**< when PUF enable, reject write device root key request */
#ifdef CFG_OTP_WITH_PUF
            if (OTP_LAYOUT_DEVICE_RK == LOCATE_LAYOUT(offset)) {
                return false;
            }
#endif
            return (_otp_is_region_lock(lock, offset, len) ? false : true);
        }
        return true;
    case LCS_DD:
        if (0 == host_id) {
            switch (LOCATE_LAYOUT(offset)) {
            case OTP_LAYOUT_MODLE_ID:
            case OTP_LAYOUT_DEVICE_ID:
            case OTP_LAYOUT_SEC_BOOT_HASH:
                if (OTP_OP_READ == op) {
                    return true;
                } else {
                    return false;
                }
            case OTP_LAYOUT_DEVICE_RK:
            case OTP_LAYOUT_MODLE_KEY:
                return false;
            case OTP_LAYOUT_LCS:
            case OTP_LAYOUT_LOCK_CTRL:
            case OTP_LAYOUT_USER_NON_SEC_REGION:
            case OTP_LAYOUT_USER_SEC_REGION:
            case OTP_LAYOUT_TEST_REGION:
                if ( OTP_OP_WRITE == op) {
                    return (_otp_is_region_lock(lock, offset, len) ?
                                                               false : true);
                }
                return true;
            default:
                return false;
            }
        } else {
            switch (LOCATE_LAYOUT(offset)) {
            case OTP_LAYOUT_MODLE_ID:
            case OTP_LAYOUT_DEVICE_ID:
            case OTP_LAYOUT_SEC_BOOT_HASH:
            case OTP_LAYOUT_LCS:
            case OTP_LAYOUT_USER_NON_SEC_REGION:
            case OTP_LAYOUT_TEST_REGION:
                if (OTP_OP_READ == op) {
                    return true;
                } else {
                    return false;
                }
            case OTP_LAYOUT_DEVICE_RK:
            case OTP_LAYOUT_MODLE_KEY:
            case OTP_LAYOUT_LOCK_CTRL:
            case OTP_LAYOUT_USER_SEC_REGION:
                return false;
            default:
                return false;
            }
        }
        break;
    case LCS_DR:
        if (0 == host_id) {
            switch (LOCATE_LAYOUT(offset)) {
            case OTP_LAYOUT_MODLE_ID:
            case OTP_LAYOUT_DEVICE_ID:
            case OTP_LAYOUT_SEC_BOOT_HASH:
            case OTP_LAYOUT_LCS:
                if (OTP_OP_READ == op) {
                    return true;
                } else {
                    return false;
                }
            case OTP_LAYOUT_DEVICE_RK:
            case OTP_LAYOUT_MODLE_KEY:
            case OTP_LAYOUT_USER_SEC_REGION:
                return false;
            case OTP_LAYOUT_LOCK_CTRL:
            case OTP_LAYOUT_USER_NON_SEC_REGION:
            case OTP_LAYOUT_TEST_REGION:
                if ( OTP_OP_WRITE == op) {
                    return (_otp_is_region_lock(lock, offset, len) ?
                                                              false : true);
                }
                return true;
            default:
                return false;
            }
        } else {
            switch (LOCATE_LAYOUT(offset)) {
            case OTP_LAYOUT_MODLE_ID:
            case OTP_LAYOUT_DEVICE_ID:
            case OTP_LAYOUT_SEC_BOOT_HASH:
            case OTP_LAYOUT_LCS:
            case OTP_LAYOUT_USER_NON_SEC_REGION:
            case OTP_LAYOUT_TEST_REGION:
                if (OTP_OP_READ == op) {
                    return true;
                } else {
                    return false;
                }
            case OTP_LAYOUT_DEVICE_RK:
            case OTP_LAYOUT_MODLE_KEY:
            case OTP_LAYOUT_LOCK_CTRL:
            case OTP_LAYOUT_USER_SEC_REGION:
                return false;
            default:
                return false;
            }
        }
        break;
    default:
        return false;
    }
    return false;
}

static int _otp_sanity_check_len( size_t offset,
                                  size_t len )
{
    switch (LOCATE_LAYOUT(offset)) {
    case OTP_LAYOUT_MODLE_ID:
        if ((offset + len) >
            (TE_OTP_MODEL_ID_OFFSET + TE_OTP_MODEL_ID_SIZE)) {
           return TE_ERROR_OVERFLOW;
        }
        break;
    case OTP_LAYOUT_MODLE_KEY:
        if ((offset + len) >
            (TE_OTP_MODEL_KEY_OFFSET + TE_OTP_MODEL_KEY_SIZE)) {
           return TE_ERROR_OVERFLOW;
        }
        /**<  because of scrambling, and unit is word,
         *    so constraint both offset and len to word aligned */
        if ( !UTILS_IS_ALIGNED(offset, OTP_WORD_SIZE)
             || !UTILS_IS_ALIGNED(len, OTP_WORD_SIZE) ) {
            return TE_ERROR_BAD_INPUT_DATA;
        }
        break;
    case OTP_LAYOUT_DEVICE_ID:
        if ((offset + len) >
            (TE_OTP_DEVICE_ID_OFFSET + TE_OTP_DEVICE_ID_SIZE)) {
           return TE_ERROR_OVERFLOW;
        }
        break;
    case OTP_LAYOUT_DEVICE_RK:
        if ((offset + len) >
            (TE_OTP_DEVICE_RK_OFFSET + TE_OTP_DEVICE_RK_SIZE)) {
           return TE_ERROR_OVERFLOW;
        }
        /**<  because of scrambling, and it's unit is word,
         *    so constraint both offset and len to word aligned */
        if ( !UTILS_IS_ALIGNED(offset, OTP_WORD_SIZE)
             || !UTILS_IS_ALIGNED(len, OTP_WORD_SIZE) ) {
            return TE_ERROR_BAD_INPUT_DATA;
        }
        break;
    case OTP_LAYOUT_SEC_BOOT_HASH:
        if ((offset + len) >
            (TE_OTP_SEC_BOOT_HASH_OFFSET + TE_OTP_SEC_BOOT_HASH_SIZE)) {
           return TE_ERROR_OVERFLOW;
        }
        break;
    case OTP_LAYOUT_LCS:
        if ((offset + len) >
            (TE_OTP_LCS_OFFSET + TE_OTP_LCS_SIZE)) {
           return TE_ERROR_OVERFLOW;
        }
        break;
    case OTP_LAYOUT_LOCK_CTRL:
        if ((offset + len) >
            (TE_OTP_LOCK_CTRL_OFFSET + TE_OTP_LOCK_CTRL_SIZE)) {
           return TE_ERROR_OVERFLOW;
        }
        break;
    case OTP_LAYOUT_USER_NON_SEC_REGION:
        if ((offset + len) >
           ((size_t)TE_OTP_NSEC_OFFSET + TE_OTP_NSEC_SIZE)) {
           return TE_ERROR_OVERFLOW;
        }
        break;
    case OTP_LAYOUT_USER_SEC_REGION:
        if ((offset + len) >
           (TE_OTP_SEC_OFFSET + TE_OTP_SEC_SIZE)) {
           return TE_ERROR_OVERFLOW;
        }
        break;
    case OTP_LAYOUT_TEST_REGION:
        if ((offset + len) >
           (TE_OTP_TST_OFFSET + TE_OTP_TST_SIZE)) {
           return TE_ERROR_OVERFLOW;
        }
        break;

    default:
        return TE_ERROR_OVERFLOW;
    }

    return TE_SUCCESS;
}

static int _otp_set_dummy(te_hwa_otpctl_t *ctl, size_t off,
                          size_t len, const uint8_t *buf)
{
    int ret = TE_SUCCESS;
    te_otp_dummy_t dummy = {0};
    int32_t lcs = 0;

    ret = ctl->get_dummy(ctl, &dummy);
    __OTP_DRV_CHECK_CONDITION__(ret);

    switch (LOCATE_LAYOUT(off)) {
    case OTP_LAYOUT_LCS:
        if (TE_OTP_LCS_SIZE != len) {
            ret = TE_ERROR_BAD_INPUT_LENGTH;
            _OTP_DRV_OUT_;
        }
        osal_memcpy(&lcs, buf, len);
        dummy.conf.lcs_valid = 1;
        switch (lcs) {
            case LCS_CM:
                dummy.conf.lcs_cm = 1;
                dummy.conf.lcs_dm = 0;
                dummy.conf.lcs_dd = 0;
                dummy.conf.lcs_dr = 0;
                break;
            case LCS_DM:
                dummy.conf.lcs_cm = 0;
                dummy.conf.lcs_dm = 1;
                dummy.conf.lcs_dd = 0;
                dummy.conf.lcs_dr = 0;
                break;
            case LCS_DD:
                dummy.conf.lcs_cm = 0;
                dummy.conf.lcs_dm = 0;
                dummy.conf.lcs_dd = 1;
                dummy.conf.lcs_dr = 0;
                break;
            case LCS_DR:
                dummy.conf.lcs_cm = 0;
                dummy.conf.lcs_dm = 0;
                dummy.conf.lcs_dd = 0;
                dummy.conf.lcs_dr = 1;
                break;
            default:
                ret = TE_ERROR_NOT_SUPPORTED;
                break;
        }
        break;
    case OTP_LAYOUT_DEVICE_RK:
        dummy.conf.key_valid = 1;
        osal_memcpy(dummy.rootk,
                    buf,
                    (len > TE_OTP_DEVICE_RK_SIZE) ?
                                TE_OTP_DEVICE_RK_SIZE : len);
        break;
    case OTP_LAYOUT_MODLE_KEY:
        dummy.conf.key_valid = 1;
        osal_memcpy(dummy.modk,
                    buf,
                    len > TE_OTP_MODEL_KEY_SIZE ?
                                TE_OTP_MODEL_KEY_SIZE : len);
        break;
    default:
        ret = TE_ERROR_NOT_SUPPORTED;
        break;
    }

    __OTP_DRV_CHECK_CONDITION__(ret);
    ret = ctl->set_dummy(ctl, &dummy);
__out__:
    return  ret;
}

static inline bool _is_region_blank(uint8_t *addr, size_t size)
{
    bool is_blank = true;
    size_t i = 0;

    for (i = 0; i < size; i++) {
        if (0 != addr[i]) {
            is_blank = false;
            break;
        }
    }
    return is_blank;
}

static int otp_alert_revocation_failed( te_hwa_otpctl_t *ctl,
                                        te_hwa_otp_t *otp )
{
    int ret = TE_SUCCESS;
    te_otp_ctl_t old_ctl = {0};
    te_otp_ctl_t tmp = {0};
    uint32_t lcs = 0;

    TE_ASSERT(NULL != ctl);
    TE_ASSERT(NULL != otp);

    ret = ctl->get_ctl(ctl, &old_ctl);
    if (TE_SUCCESS != ret) {
        goto err_get_ctl;
    }
    osal_memcpy(&tmp, &old_ctl, sizeof(tmp));
    tmp.direct_rd = 0x1;
    ret = ctl->set_ctl(ctl, &tmp);
    if (TE_SUCCESS != ret) {
        goto err_set_ctl;
    }
    /** sync otp data */
    ret = otp->read(otp, TE_OTP_LCS_OFFSET, (uint8_t *)&lcs, sizeof(lcs));
    if (TE_SUCCESS !=ret) {
        goto err_read;
    }
    if (LCS_DR != (lcs & OTP_LCS_MASK)) {
        OSAL_LOG_ERR("[Alert] Revoked failed, LCS invalid!!!\n");
    }
    if (!(lcs & OTP_INVALID_MASK)) {
        OSAL_LOG_ERR("[Alert] Revoked failed, Erase failed!!!\n");
    }
err_read:
err_set_ctl:
    ctl->set_ctl(ctl, &old_ctl);
err_get_ctl:
    return ret;
}

static int otp_write( te_ctx_handle h,
                      size_t off,
                      const uint8_t *buf,
                      size_t len )
{
#define PARTIAL_WRITE(off, len, nm)                                          \
        ((((off) > TE_OTP_##nm##_OFFSET) &&                                  \
          ((off) < TE_OTP_##nm##_OFFSET + TE_OTP_##nm##_SIZE)) ||            \
         (((off) == TE_OTP_##nm##_OFFSET) &&                                 \
          (((off) + (len)) != TE_OTP_##nm##_OFFSET + TE_OTP_##nm##_SIZE)))
    int ret = TE_SUCCESS;
    te_hwa_otpctl_t *ctl = NULL;
    te_otp_drv_t *drv = NULL;
    otp_drv_ctx_t *ctx = (otp_drv_ctx_t *)h;
    int32_t lcs = 0;
    int32_t host_id = 0;
    te_hwa_host_t *host = NULL;
    te_otp_lock_t lock = {0};
    te_hwa_otp_t *otp = NULL;
    size_t idx = 0;
    uint8_t *rd_buf_orig = NULL;
    uint8_t *rd_buf_cfm = NULL;
    size_t ofs_of_word = off & (OTP_WORD_SIZE - 1);
    size_t wt_rd_ofs = ROUND_DN(off, OTP_WORD_SIZE);
    size_t wt_rd_len = 0;

    if (!ctx || !buf) {
        return TE_ERROR_BAD_PARAMS;
    }

    /** reject partial write operation for model key, device root key, and LCS
     *  only accept off eq start address of the region, len eq the region size
     */
    if ( PARTIAL_WRITE(off, len, MODEL_KEY)
            || PARTIAL_WRITE(off, len, DEVICE_RK)
            || PARTIAL_WRITE(off, len, LCS) ) {
            return TE_ERROR_BAD_PARAMS;
    }

    /**< reject blank(all zero) case for model key and device root key */
    if ( ((TE_OTP_MODEL_KEY_OFFSET == off)
            || (TE_OTP_DEVICE_RK_OFFSET == off))
            && (_is_region_blank((uint8_t *)buf, len)) ) {
            return TE_ERROR_BAD_PARAMS;
    }
    otp_get_ctx(ctx);
    if (OTP_CTX_MAGIC != ctx->magic) {
        ret = TE_ERROR_BAD_FORMAT;
        _OTP_DRV_OUT_;
    }
    drv = GET_OTP_CTX_DRV(ctx);
    if (OTP_DRV_MAGIC != drv->magic) {
        ret = TE_ERROR_BAD_FORMAT;
        _OTP_DRV_OUT_;
    }

    host = GET_OTP_DRV_HOST(drv);
    /**< only host#0 secure host can write */
    host_id = te_hwa_host_id(host);
    if (host_id != 0) {
        ret = TE_ERROR_ACCESS_DENIED;
        _OTP_DRV_OUT_;
    }
    osal_mutex_lock(ctx->mut);
    lcs = _otp_get_lcs(ctx);
    if (0 > lcs){
        ret = lcs;
        goto err_get_lcs;
    }
    /**< check lcs contraint */
    if ( (TE_OTP_LCS_OFFSET == off) && (0 < len)
            && ((*buf & OTP_INVALID_MASK)
                ||(((*buf & OTP_LCS_MASK) != LCS_CM)
                    && ((*buf & OTP_LCS_MASK) != LCS_DM)
                    && ((*buf & OTP_LCS_MASK) != LCS_DD)
                    && ((*buf & OTP_LCS_MASK) != LCS_DR))) ) {
        ret = TE_ERROR_BAD_PARAMS;
        goto err_lcs;
    }
    /**< check lcs rollback */
    if ((TE_OTP_LCS_OFFSET == off) && ((*buf & OTP_LCS_MASK) < (uint8_t)lcs)) {
        ret = TE_ERROR_ACCESS_DENIED;
        goto err_lcs;
    }
    otp = GET_OTP_HWA(drv);
    ctl = GET_OTP_CTL(GET_OTP_CTX_DRV(ctx));
    if (ctx->cfg.otp_exist) {
        ret = otp->read(otp, TE_OTP_LOCK_CTRL_OFFSET,
                                (uint8_t *)&lock, sizeof(lock));
        if (ret != TE_SUCCESS) {
            goto err_read_lock;
        }

        if (_otp_check_permission(lcs, host_id, off, len, OTP_OP_WRITE, &lock)) {
            ret = _otp_sanity_check_len(off, len);
            if (TE_SUCCESS == ret){
                /** once len=0, write nothing just cleanup and return*/
                if ( 0 == len ) {
                    goto cleanup;
                }

                /* get otp-word-aligned len for read & write */
                wt_rd_len = ROUND_UP(len + ofs_of_word, OTP_WORD_SIZE);

                /* 1. read original otp data first */
                rd_buf_orig = (uint8_t *)osal_calloc(1, wt_rd_len);
                if(NULL == rd_buf_orig) {
                    ret = TE_ERROR_OOM;
                    goto err_rd;
                }
                ret = otp->read(otp, wt_rd_ofs, rd_buf_orig, wt_rd_len);
                if (ret != TE_SUCCESS) {
                    goto err_rd;
                }

                /* 2. check write buf equal read buf? */
                if (0 == osal_memcmp(buf, &rd_buf_orig[ofs_of_word], len)) {
                    goto cleanup;
                }

                /* 3. calc final bits set */
                for (idx = 0; idx < len; idx++) {
                    rd_buf_orig[idx + ofs_of_word] |= buf[idx];
                }

                /* 4. write otp buf */
                ret = ctl->write(ctl, wt_rd_ofs, rd_buf_orig, wt_rd_len);
                if (ret != TE_SUCCESS) {
                    goto err_after_wr;
                }

                /* 5. confirm otp buf write */
                rd_buf_cfm = (uint8_t *)osal_calloc(1, wt_rd_len);
                if(NULL == rd_buf_cfm) {
                    ret = TE_ERROR_OOM;
                    goto err_after_wr;
                }
                ret = otp->read(otp, wt_rd_ofs, rd_buf_cfm, wt_rd_len);
                if (ret != TE_SUCCESS) {
                    goto err_after_wr;
                }
                if (TE_OTP_LCS_OFFSET == wt_rd_ofs) {
                    *rd_buf_cfm &= ~OTP_INVALID_MASK;
                }
                if (osal_memcmp(rd_buf_cfm, rd_buf_orig, wt_rd_len) != 0) {
                    ret = TE_ERROR_SECURITY;
                    goto err_after_wr;
                }
            } else {
                /** while access overflow jump to err_access, otherwise will
                 *  lead to Line#1210 misjudge */
                goto err_access;
            }
        } else {
            ret = TE_ERROR_ACCESS_DENIED;
            goto err_access;
        }
    } else {
        ret = _otp_set_dummy(ctl, off, len, (uint8_t *)buf);
    }
err_after_wr:
    /** special error handling for change LCS to DR */
    if ( (TE_OTP_LCS_OFFSET == off) &&
         (((TE_OTP_LCS_OFFSET + TE_OTP_LCS_SIZE) == (off + len)) &&
            (LCS_DR == (*buf & OTP_LCS_MASK))) ) {
        (void)otp_alert_revocation_failed(ctl, otp);
    }
err_access:
err_rd:
err_read_lock:
err_lcs:
err_get_lcs:
cleanup:
    osal_mutex_unlock(ctx->mut);
__out__:
    OSAL_SAFE_FREE(rd_buf_orig);
    OSAL_SAFE_FREE(rd_buf_cfm);
    otp_put_ctx(ctx);
    return ret;
}

static int otp_get_vops( te_ctx_handle h,
                         void **pvops )
{
    int ret = TE_SUCCESS;
    otp_drv_ctx_t *ctx = (otp_drv_ctx_t *)h;

    TE_ASSERT(NULL != ctx);
    if (OTP_CTX_MAGIC != ctx->magic) {
        ret = TE_ERROR_BAD_FORMAT;
        _OTP_DRV_OUT_;
    }
    otp_get_ctx(ctx);
    *pvops = ctx->vops;
    otp_put_ctx(ctx);
__out__:
    return ret;
}

static int te_otp_drv_suspend( struct te_crypt_drv* drv, te_suspend_state_t state )
{
    te_otp_drv_t *otp_drv = (te_otp_drv_t *)drv;
    te_hwa_host_t *host = NULL;
    otp_drv_ctx_t *ctx = NULL;
    te_hwa_otpctl_t *otp_ctl = NULL;
    int ret = TE_SUCCESS;

    TE_ASSERT(NULL != otp_drv);
    TE_ASSERT(NULL != otp_drv->hctx);
    host = GET_OTP_DRV_HOST(otp_drv);
    TE_ASSERT(NULL != host);
    if (0 == te_hwa_host_id(host)){
        ctx = GET_OTP_DRV_CTX(otp_drv);
        TE_ASSERT(NULL != ctx);
        otp_ctl = GET_OTP_CTL(otp_drv);
        TE_ASSERT(NULL != otp_ctl);
        if (ctx->cfg.otp_exist) {
            ret = otp_ctl->get_ctl(otp_ctl, &otp_drv->pm->otp_ctl);
            if (TE_SUCCESS != ret) {
                return ret;
            }
#ifdef CFG_OTP_WITH_PUF
            ret = _otp_puf_suspend(otp_drv->hctx, state);
            if (TE_SUCCESS != ret) {
                return ret;
            }
#else
            (void)state;
#endif
        } else { /* dummy otp */
            ret = otp_ctl->get_dummy(otp_ctl, &otp_drv->pm->dummy);
        }
    }

    return ret;
}

static int te_otp_drv_resume( struct te_crypt_drv* drv, te_suspend_state_t state )
{
    te_otp_drv_t *otp_drv = (te_otp_drv_t *)drv;
    te_hwa_host_t *host = NULL;
    otp_drv_ctx_t *ctx = NULL;
    te_hwa_otpctl_t *otp_ctl = NULL;
    te_otp_stat_t hwstate = {0};
#ifdef CFG_OTP_WITH_PUF
    te_otp_ctl_t new_ctl = {0};
#endif
    int ret = TE_SUCCESS;

    TE_ASSERT(NULL != otp_drv);
    TE_ASSERT(NULL != otp_drv->hctx);
    host = GET_OTP_DRV_HOST(otp_drv);
    TE_ASSERT(NULL != host);
    if (0 == te_hwa_host_id(host)){
        ctx = GET_OTP_DRV_CTX(otp_drv);
        TE_ASSERT(NULL != ctx);
        otp_ctl = GET_OTP_CTL(otp_drv);
        TE_ASSERT(NULL != otp_ctl);
        if (ctx->cfg.otp_exist) {
#ifdef CFG_OTP_WITH_PUF
            ret = _otp_puf_resume(otp_drv->hctx, state);
            if (TE_SUCCESS != ret) {
                return ret;
            }
            ret = otp_ctl->get_ctl(otp_ctl, &new_ctl);
            if (TE_SUCCESS !=  ret) {
                return ret;
            }
            new_ctl.direct_rd |= otp_drv->pm->otp_ctl.direct_rd;
            ret = otp_ctl->set_ctl(otp_ctl, &new_ctl);
#else
            (void)state;
            ret = otp_ctl->set_ctl(otp_ctl, &otp_drv->pm->otp_ctl);
#endif
            if (TE_SUCCESS !=  ret) {
                return ret;
            }

            do{
                otp_ctl->state((te_hwa_otpctl_t *)otp_ctl, &hwstate);

                if ((1 == hwstate.init_done) && (0x3F == hwstate.shdw_valid)) {
                    break;
                }
                osal_delay_us(DELAY_US);
            }while(true);

        } else { /* dummy otp */
            ret = otp_ctl->set_dummy(otp_ctl, &otp_drv->pm->dummy);
        }
    }

    return ret;
}

static void te_otp_drv_destroy( struct te_crypt_drv* drv )
{
    te_otp_drv_t *otp_drv = (te_otp_drv_t *)drv;
    TE_ASSERT(NULL != otp_drv);
    osal_free(otp_drv->pm);
    osal_memset(otp_drv, 0x00, sizeof(*otp_drv));
}

static void _otp_setup_host_0( te_otp_drv_t *drv,
                              otp_drv_ctx_t *ctx,
                              const te_hwa_otpctl_t *ctl,
                              emem_puf_ops_t *puf_ops )
{
    drv->get_vops = otp_get_vops;
    drv->write = otp_write;
#ifndef CFG_OTP_WITH_PUF
    (void)puf_ops;
#else
    puf_ops->ready = otp_puf_ready;
    puf_ops->enroll = otp_puf_enroll;
    puf_ops->quality_check = otp_puf_quality_check;
    puf_ops->init_margin_read = otp_puf_init_margin_read;
    puf_ops->pgm_margin_read = otp_puf_pgm_margin_read;
#endif
    ctx->ctl = (te_hwa_crypt_t *)&ctl->base;
}

static int otp_sanity_check( te_hwa_otpctl_t *ctl,
                             te_hwa_otp_t *otp ) {
    int ret = TE_SUCCESS;
    te_otp_ctl_t old_ctl = {0};
    te_otp_ctl_t tmp = {0};
    uint32_t lcs = 0;

    TE_ASSERT(NULL != ctl);
    TE_ASSERT(NULL != otp);
    ret = ctl->get_ctl(ctl, &old_ctl);
    if (TE_SUCCESS != ret) {
        goto err_get_ctl;
    }
    osal_memcpy(&tmp, &old_ctl, sizeof(tmp));
    tmp.direct_rd = 0x1;
    ret = ctl->set_ctl(ctl, &tmp);
    if (TE_SUCCESS != ret) {
        goto err_set_ctl;
    }
    ret = otp->read(otp, TE_OTP_LCS_OFFSET, (uint8_t *)&lcs, sizeof(lcs));
    if (TE_SUCCESS != ret) {
        goto err_read;
    }
    if (LCS_DR == (lcs & OTP_LCS_MASK)) {
        /** check inv done flag */
        if (!(lcs & OTP_INVALID_MASK)) {
            OSAL_LOG_ERR("[Alert] Revoked failed, Erase failed!!!\n");
            ret = TE_ERROR_SECURITY;
        }
    }
err_read:
err_set_ctl:
    ctl->set_ctl(ctl, &old_ctl);
err_get_ctl:
    return ret;
}

int te_otp_drv_init( te_otp_drv_t *drv,
                     const te_hwa_otp_t *otp,
                     const te_hwa_otpctl_t *ctl,
                     const char* name )
{
#define PUF_NAME            "PUF"
    int ret = TE_SUCCESS;
    otp_drv_ctx_t *ctx = NULL;
    te_otp_stat_t state = {0};
    te_hwa_host_t *host = NULL;
    te_rtl_conf_t conf = {0};
#ifdef CFG_OTP_WITH_PUF
    emem_puf_ops_t *puf_ops = NULL;
#endif
    __OTP_DRV_VERIFY_PARAMS__(drv);
    __OTP_DRV_VERIFY_PARAMS__(otp);

    if ((OTP_DRV_MAGIC == drv->magic)
        && osal_atomic_load(&drv->base.refcnt)) {
        _OTP_DRV_OUT_;
    }

    osal_memset(drv, 0, sizeof(te_otp_drv_t));
    drv->magic = OTP_DRV_MAGIC;
    drv->base.hwa = (te_hwa_crypt_t *)&otp->base;
    drv->base.resume = te_otp_drv_resume;
    drv->base.suspend = te_otp_drv_suspend;
    drv->base.destroy = te_otp_drv_destroy;

    if ( NULL != name ) {
        osal_strncpy(drv->base.name, name, TE_MAX_DRV_NAME - 1);
    }

#ifndef CFG_OTP_WITH_PUF
    ctx = (otp_drv_ctx_t *)osal_calloc(1, sizeof(otp_drv_ctx_t));
    if(NULL == ctx) {
        ret = TE_ERROR_OOM;
        _OTP_DRV_OUT_;
    }
#else
    ctx = (otp_drv_ctx_t *)osal_calloc(1, sizeof(otp_drv_ctx_t) +
                    sizeof(emem_puf_ops_t));
    if(NULL == ctx) {
        ret = TE_ERROR_OOM;
        _OTP_DRV_OUT_;
    }

    puf_ops = (emem_puf_ops_t *)(ctx + 1);
    osal_strncpy(puf_ops->name, PUF_NAME, TE_MAX_DRV_NAME - 1);
#endif
    drv->pm = (otp_pm_ctx_t *)osal_calloc(1, sizeof(otp_pm_ctx_t));
    if (NULL == drv->pm) {
        ret = TE_ERROR_OOM;
        goto err_calloc_pm;
    }

    ret = osal_mutex_create(&ctx->mut);
    if (OSAL_SUCCESS != ret) {
         goto __cleanup__;
    }

    ctx->magic = OTP_CTX_MAGIC;
    ctx->base.drv = &drv->base;
    drv->hctx = (te_ctx_handle)ctx;
    host = GET_OTP_DRV_HOST(drv);
    host->stat.conf(&host->stat, &conf);
    ctx->cfg.otp_exist = g_otp_cfg.otp_exist = conf.cfg1.otp_exist;
    if (ctx->cfg.otp_exist) {
        ctx->cfg.otp_ns_sz = g_otp_cfg.otp_ns_sz = conf.cfg1.otp_ns_sz;
        ctx->cfg.otp_s_sz = g_otp_cfg.otp_s_sz = conf.cfg1.otp_s_sz;
        ctx->cfg.otp_tst_sz = g_otp_cfg.otp_tst_sz = conf.cfg1.otp_tst_sz;
    }
    ctx->cfg.otp_skey_sz = g_otp_cfg.otp_skey_sz =
                                            conf.cfg1.otp_skey_256 ? 32 : 16;

    if (0 == te_hwa_host_id(otp->base.host)) {
        __OTP_DRV_VERIFY_PARAMS__(ctl);
#ifdef CFG_OTP_WITH_PUF
        _otp_setup_host_0(drv, ctx, ctl, puf_ops);
        _te_otp_puf_power_on((te_hwa_otpctl_t *)ctl);
#else
        _otp_setup_host_0(drv, ctx, ctl, NULL);
#endif
        do{
            ctl->state((te_hwa_otpctl_t *)ctl, &state);

            if ((1 == state.init_done) && (0x3F == state.shdw_valid)) {
                break;
            }
            osal_delay_us(DELAY_US);
        }while(true);
        /** sanity check for chip revokation */
        otp_sanity_check((te_hwa_otpctl_t*)ctl, (te_hwa_otp_t *)otp);
    }

    otp_get_ctx(ctx);
    te_crypt_drv_get(&drv->base);
    _OTP_DRV_OUT_;
__cleanup__:
    osal_free(drv->pm);
err_calloc_pm:
    osal_free(ctx);
__out__:
    return ret;
}

int te_otp_drv_exit( te_otp_drv_t *drv )
{
    int ret = TE_SUCCESS;

    __OTP_DRV_VERIFY_PARAMS__(drv);
    if (OTP_DRV_MAGIC != drv->magic) {
        ret = TE_ERROR_BAD_FORMAT;
        _OTP_DRV_OUT_;
    }

    otp_put_ctx((otp_drv_ctx_t *)drv->hctx);
    te_crypt_drv_put(&drv->base);
__out__:
    return ret;
}

static int _te_otp_read_dummy(te_otp_drv_t *drv,
                              size_t off,
                              uint8_t *buf,
                              size_t len)
{
    int ret = TE_SUCCESS;
    otp_drv_ctx_t *ctx = NULL;
    te_hwa_otpctl_t *ctl = NULL;
    te_otp_dummy_t dummy = {0};
    int32_t lcs = 0;

    if (NULL == drv) {
        return TE_ERROR_BAD_PARAMS;
    }
    ctx = GET_OTP_DRV_CTX(drv);
    otp_get_ctx(ctx);
    ctl = GET_OTP_CTL(drv);
    ret = ctl->get_dummy(ctl, &dummy);
    __OTP_DRV_CHECK_CONDITION__(ret);

    switch (LOCATE_LAYOUT(off)) {
    case OTP_LAYOUT_LCS:
        if (TE_OTP_LCS_SIZE != len) {
            ret = TE_ERROR_BAD_INPUT_LENGTH;
            _OTP_DRV_OUT_;
        }

        lcs = _otp_get_lcs(ctx);
        if (0 > lcs) {
            ret = lcs;
            _OTP_DRV_OUT_;
        } else {
            osal_memcpy(buf, &lcs, len);
        }
        break;
    case OTP_LAYOUT_DEVICE_RK:
        osal_memcpy(buf,
                    dummy.rootk,
                    len > TE_OTP_DEVICE_RK_SIZE ?
                        TE_OTP_DEVICE_RK_SIZE : len);
        break;
    case OTP_LAYOUT_MODLE_KEY:
        osal_memcpy(buf,
                    dummy.modk,
                    (len > TE_OTP_MODEL_KEY_SIZE) ? TE_OTP_MODEL_KEY_SIZE : len);
        break;
    default:
        ret = TE_ERROR_NOT_SUPPORTED;
        break;
    }
__out__:
    otp_put_ctx(ctx);
    return ret;
}

static bool _otp_is_region_support_shadow(size_t off)
{
    switch (LOCATE_LAYOUT(off)) {
        case OTP_LAYOUT_MODLE_ID:
        case OTP_LAYOUT_MODLE_KEY:
        case OTP_LAYOUT_DEVICE_ID:
        case OTP_LAYOUT_DEVICE_RK:
        case OTP_LAYOUT_LCS:
        case OTP_LAYOUT_LOCK_CTRL:
            return true;
        case OTP_LAYOUT_SEC_BOOT_HASH:
        case OTP_LAYOUT_USER_NON_SEC_REGION:
        case OTP_LAYOUT_USER_SEC_REGION:
        case OTP_LAYOUT_TEST_REGION:
        default:
            return false;
    }

    return false;
}

static int _otp_read( te_otp_drv_t *drv,
	                  size_t off,
	                  uint8_t *buf,
	                  size_t len,
	                  bool b_lock )
{
    int ret = TE_SUCCESS;
    te_hwa_otp_t *otp = NULL;
    int32_t lcs = 0;
    int32_t host_id = 0;
    otp_drv_ctx_t *ctx = NULL;

    __OTP_DRV_VERIFY_PARAMS__(drv);
    __OTP_DRV_VERIFY_PARAMS__(buf);

    if (OTP_DRV_MAGIC != drv->magic) {
        ret = TE_ERROR_BAD_FORMAT;
        _OTP_DRV_OUT_;
    }

    otp = GET_OTP_HWA(drv);
    host_id = te_hwa_host_id(GET_OTP_DRV_HOST(drv));
    ctx = GET_OTP_DRV_CTX(drv);
    otp_get_ctx(ctx);
    /**< make sure lcs is not changed during the whole read process */
    if (b_lock) {
        osal_mutex_lock(ctx->mut);
    }
    lcs = _otp_get_lcs(ctx);
    if (0 > lcs){
        ret = lcs;
        goto err_get_lcs;
    }
    if (ctx->cfg.otp_exist) {
        /**< for lock ctrl only affect write operation so for read just set it to NULL */
        if (_otp_check_permission(lcs, host_id, off, len, OTP_OP_READ, NULL)) {
            ret = _otp_sanity_check_len(off, len);
            if (TE_SUCCESS == ret) {
                if ((0 == host_id)
                    && !_otp_is_region_support_shadow(off)) {
#ifdef CFG_OTP_WITH_PUF
                    if (TE_SUCCESS != otp_puf_ready(drv->hctx)) {
                        ret = TE_ERROR_ACCESS_DENIED;
                        goto err_puf_rdy;
                    }
#endif
                    ret = otp->read(otp, off, buf, len);
                } else {
                    ret = otp->read(otp, off, buf, len);
                }
            }
        } else {
            ret = TE_ERROR_ACCESS_DENIED;
        }
    } else {
        if ( 0 != host_id){
            ret = TE_ERROR_ACCESS_DENIED;
        } else {
            ret = _te_otp_read_dummy(drv, off, buf, len);
        }
    }
#ifdef CFG_OTP_WITH_PUF
err_puf_rdy:
#endif
err_get_lcs:
    if (b_lock) {
        osal_mutex_unlock(ctx->mut);
    }
    otp_put_ctx(ctx);
__out__:
    return ret;
}

int te_otp_read( te_otp_drv_t *drv,
                 size_t off,
                 uint8_t *buf,
                 size_t len )
{
    return _otp_read( drv, off, buf, len, true );
}

int te_otp_get_conf(te_otp_drv_t *drv, te_otp_conf_t *conf)
{
    int ret = TE_SUCCESS;
    (void) drv;
    __OTP_DRV_VERIFY_PARAMS__(conf);
    osal_memcpy(conf, &g_otp_cfg, sizeof(*conf));
__out__:
    return ret;
}
