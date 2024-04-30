//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <driver/te_drv_sca.h>
#include <hwa/te_hwa_sca.h>
#include <hwa/te_hwa.h>
#include "drv_internal.h"
#include "drv_sess.h"

#define _SCA_DRV_OUT_           goto __out__

#define __SCA_DRV_CHECK_CONDITION__(_ret_)                                     \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X\n",                               \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_));                                      \
                 _SCA_DRV_OUT_;                                                \
            }                                                                  \
        } while (0);

#define __SCA_DRV_ALERT__(_ret_, _msg_)                                        \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X %s\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_),                                       \
                                (_msg_));                                      \
            }                                                                  \
        } while (0);

#define __SCA_DRV_VERIFY_PARAMS__(_param_)                                     \
        do                                                                     \
        {                                                                      \
            if(NULL == (_param_)){                                             \
                ret = TE_ERROR_BAD_PARAMS;                                     \
                _SCA_DRV_OUT_;                                                 \
            }                                                                  \
        } while (0)

/* SCA INIT command shifts */
#define SCA_OP_CODE_INIT                        (0x80U)
#define SCA_INIT_OP_CODE_SHIFT                        (24U)
#define SCA_INIT_SLOT_ID_SHIFT                        (19U)
#define SCA_INIT_KLAD256_SHIFT                        (18U)
#define SCA_INIT_IV_NS_SHIFT                          (17U)
#define SCA_INIT_KEY_NS_SHIFT                         (16U)
#define SCA_INIT_CTR_LEN_SHIFT                        (14U)
#define SCA_INIT_RSVD1_SHIFT                          (13U)
#define SCA_INIT_ALGO_SHIFT                           (10U)
#define SCA_INIT_KEY_SRC_SHIFT                        (8U)
#define SCA_INIT_KEY_LEN_SHIFT                        (6U)
#define SCA_INIT_IV_LOAD_SHIFT                        (5U)
#define SCA_INIT_KEY_MODE_SHIFT                       (1U)
#define SCA_INIT_TRIGGER_INTR_SHIFT                   (0U)
/* SCA INIT command sub fields mask */
#define SCA_INIT_OP_CODE_MASK         (0xFFU)
#define SCA_INIT_SLOT_ID_MASK         (0x0FU)
#define SCA_INIT_CTR_LEN_MASK         (0x03U)
#define SCA_INIT_RSVD1_MASK           (0x01U)
#define SCA_INIT_ALGO_MASK            (0x07U)
#define SCA_INIT_KEY_SRC_MASK         (0x03U)
#define SCA_INIT_KEY_LEN_MASK         (0x03U)
#define SCA_INIT_KEY_IV_LOAD_MASK     (0x01U)
#define SCA_INIT_KEY_MODE_MASK        (0x0FU)
#define SCA_INIT_TRIGGER_INTR_MASK    (0x01U)

/* SCA proc command shifts*/
#define SCA_OP_CODE_PROC                        (0x40U)
#define SCA_PROC_OP_CODE_SHIFT                        (24U)
#define SCA_PROC_SLOT_ID_SHIFT                        (19U)
#define SCA_PROC_RSVD1_SHIFT                          (8U)
#define SCA_PROC_SRC_NS_SHIFT                         (7U)
#define SCA_PROC_DST_NS_SHIFT                         (6U)
#define SCA_PROC_BYPASS_SHIFT                         (5U)
#define SCA_PROC_OP_MODE_SHIFT                        (4U)
#define SCA_PROC_IS_LAST_SHIFT                        (3U)
#define SCA_PROC_SRC_ADDR_TYPE_SHIFT                  (2U)
#define SCA_PROC_DST_ADDR_TYPE_SHIFT                  (1U)
#define SCA_PROC_TRIGGER_INTR_SHIFT                   (0U)
/* SCA proc command sub fields mask*/
#define SCA_PROC_OP_CODE_MASK          (0xFFU)
#define SCA_PROC_SLOT_ID_MASK          (0X0FU)
#define SCA_PROC_RSVD_MASK             (0x1FFFU)
#define SCA_PROC_BYPASS_MASK           (0x01)
#define SCA_PROC_OP_MODE_MASK          (0x01)
#define SCA_PROC_IS_LAST_MASK          (0x01)
#define SCA_PROC_SRC_ADDR_TYPE_MASK    (0x01)
#define SCA_PROC_DST_ADDR_TYPE_MASK    (0x01)
#define SCA_PROC_TRIGGER_INTR_MASK     (0x01)

/* SCA finish command shifts*/
#define SCA_OP_CODE_FINISH                      (0x20U)
#define SCA_FINISH_OP_CODE_SHIFT                        (24U)
#define SCA_FINISH_SLOT_ID_SHIFT                        (19U)
#define SCA_FINISH_MAC_NS_SHIFT                         (5U)
#define SCA_FINISH_MAC_LEN_SHIFT                        (1U)
#define SCA_FINISH_TRIGGER_INTR_SHIFT                   (0U)
/* SCA finish command mask*/
#define SCA_FINISH_OP_CODE_MASK        (0xFFU)
#define SCA_FINISH_SLOT_ID_MASK        (0x0FU)
#define SCA_FINISH_RSVD_MASK           (0x1FFFU)
#define SCA_FINISH_MAC_LEN_MASK        (0x0FU)
#define SCA_FINISH_TRIGGER_INTR_MASK   (0x01U)

/* SCA clear command shifts*/
#define SCA_OP_CODE_CLEAR                       (0xFFU)
#define SCA_CLEAR_OP_CODE_SHIFT                         (24U)
#define SCA_CLEAR_SLOT_ID_SHIFT                         (19U)
#define SCA_CLEAR_RVSD_SHIFT                            (1U)
#define SCA_CLEAR_INTR_SHIFT                            (0U)
/* SCA clear command mask*/
#define SCA_CLEAR_OP_CODE_MASK          (0xFFU)
#define SCA_CLEAR_SLOT_ID_MASK          (0x0FU)
#define SCA_CLEAR_RVSD_MASK             (0x3FFFFU)
#define SCA_CLEAR_INTR_MASK             (0x01U)
#define MERAK_WORD_SIZE         (0x04U)
#define TRIGGER_INT             (0x01U)
#define ADDR_TYPE_NORMAL        (0X00U)
#define ADDR_TYPE_LINK_LIST     (0X01U)

#define BYTE_BITS           (8U)

/*****************************************************************************
 *      Command builder macro defines
 * **************************************************************************/
#define MAKE_CMD(_opcode_, _op_) ((_opcode_) << SCA_##_op_##_OP_CODE_SHIFT)

#define SCA_INIT_CMD_SIZE       (7U)
#define MAKE_INIT_DESC(_alg_, _key_src_, _key_len_,                           \
        _iv_load_, _mode_, _intr_, _ctr_len_)                                 \
    ( MAKE_CMD(SCA_OP_CODE_INIT, INIT)                                        \
    | (((_alg_) & SCA_INIT_ALGO_MASK) << SCA_INIT_ALGO_SHIFT)                 \
    | (((_key_src_) & SCA_INIT_KEY_SRC_MASK) << SCA_INIT_KEY_SRC_SHIFT)       \
    | (((_key_len_) & SCA_INIT_KEY_LEN_MASK) << SCA_INIT_KEY_LEN_SHIFT)       \
    | (((_iv_load_) & SCA_INIT_KEY_IV_LOAD_MASK) << SCA_INIT_IV_LOAD_SHIFT)   \
    | (((_mode_) & SCA_INIT_KEY_MODE_MASK) << SCA_INIT_KEY_MODE_SHIFT)        \
    | (((_intr_) & SCA_INIT_TRIGGER_INTR_MASK) << SCA_INIT_TRIGGER_INTR_SHIFT)\
    | (((_ctr_len_) & SCA_INIT_CTR_LEN_MASK) << SCA_INIT_CTR_LEN_SHIFT))

#define MAKE_CMD_INIT(_cmd_, _size_, _alg_, _mode_, _key_src_, _key_len_,     \
                      _intr_, _iv_, _key_, _key2_, _iv_load_, _ctr_len_)      \
    do{                                                                       \
        (_size_) = 0;                                                         \
        (_cmd_)[(_size_)] = MAKE_INIT_DESC(_alg_, _key_src_, _key_len_,       \
                                    _iv_load_, _mode_, _intr_, _ctr_len_);    \
        (_size_) += 1;                                                        \
        if (0x01 == (_iv_load_)){                                             \
            (_cmd_)[(_size_)] = ((uint64_t)(_iv_)) & 0xFFFFFFFFU;             \
            (_size_) += 1;                                                    \
            (_cmd_)[(_size_)] = (((uint64_t)(_iv_)) >> 32) & 0xFFFFFFFFU;     \
            (_size_) += 1;                                                    \
        }                                                                     \
                                                                              \
        (_cmd_)[(_size_)] = ((uint64_t)_key_) & 0xFFFFFFFFU;                  \
        (_size_) += 1;                                                        \
        (_cmd_)[(_size_)] = (((uint64_t)_key_) >> 32) & 0xFFFFFFFFU;          \
        (_size_) += 1;                                                        \
        if( SCA_DRV_MODE_GCM == (_mode_) ) {                                  \
            (_cmd_)[(_size_)] = ((uint64_t)_key2_) & 0xFFFFFFFFU;             \
            (_size_) += 1;                                                    \
            (_cmd_)[(_size_)] = (((uint64_t)_key2_) >> 32) & 0xFFFFFFFFU;     \
            (_size_) += 1;                                                    \
        }                                                                     \
        (_size_) *= MERAK_WORD_SIZE;                                          \
        OSAL_LOG_DEBUG_DUMP_DATA("INITIAL CMD:", (_cmd_), (_size_));          \
    } while (0);

#define SCA_PROC_CMD_SIZE       (0x06U)
#define MAKE_PROC_DESC(_bypass_, _mode_, _last_,                              \
                      _src_addr_type_, _dst_addr_type_, _intr_)               \
    ( MAKE_CMD(SCA_OP_CODE_PROC, PROC)                                        \
    | (((_bypass_) & SCA_PROC_BYPASS_MASK) << SCA_PROC_BYPASS_SHIFT)          \
    | (((_mode_) & SCA_PROC_OP_CODE_MASK) << SCA_PROC_OP_MODE_SHIFT)          \
    | (((_last_) & SCA_PROC_IS_LAST_MASK) << SCA_PROC_IS_LAST_SHIFT)          \
    | (((_src_addr_type_) & SCA_PROC_SRC_ADDR_TYPE_MASK) \
        << SCA_PROC_SRC_ADDR_TYPE_SHIFT)                                      \
    | (((_dst_addr_type_) & SCA_PROC_DST_ADDR_TYPE_MASK) \
        << SCA_PROC_DST_ADDR_TYPE_SHIFT)                                      \
    | (((_intr_) & SCA_PROC_TRIGGER_INTR_MASK) \
        << SCA_PROC_TRIGGER_INTR_SHIFT) )

#define MAKE_CMD_PROC(_cmd_, _size_, _bypass_, _mode_, _last_,                \
            _src_addr_type_, _dst_addr_type_, _intr_, _in_, _len_, _out_)     \
    do{                                                                       \
        (_size_) = 0;                                                         \
        (_cmd_)[(_size_)] = MAKE_PROC_DESC(_bypass_, _mode_, _last_,          \
                            _src_addr_type_, _dst_addr_type_, _intr_);        \
        (_size_) += 1;                                                        \
        (_cmd_)[(_size_)] = ((uint64_t)(_in_)) & 0xFFFFFFFFU;                 \
        (_size_) += 1;                                                        \
        (_cmd_)[(_size_)] = (((uint64_t)(_in_)) >> 32) & 0xFFFFFFFFU;         \
        (_size_) += 1;                                                        \
        if ( ADDR_TYPE_NORMAL == (_src_addr_type_)) {                         \
            (_cmd_)[(_size_)] = ((_len_) - 1) & 0xFFFFFFFFU;                  \
            (_size_) += 1;                                                    \
        }                                                                     \
        if (0x0U != (_out_)) {                                                \
            (_cmd_)[(_size_)] = ((uint64_t)(_out_)) & 0xFFFFFFFFU;            \
            (_size_) += 1;                                                    \
            (_cmd_)[(_size_)] = ((uint64_t)(_out_) >> 32) & 0xFFFFFFFFU;      \
            (_size_) += 1;                                                    \
        }                                                                     \
        (_size_) *= MERAK_WORD_SIZE;                                          \
        OSAL_LOG_DEBUG_DUMP_DATA("PROC CMD:", (_cmd_), (_size_));             \
    } while (0);

#define SCA_FINISH_CMD_SIZE       (0x03U)
#define MAKE_FINISH_DESC(_mac_len_, _intr_)                                   \
    ( MAKE_CMD(SCA_OP_CODE_FINISH, FINISH)                                    \
    | ((((_mac_len_) - 1) & SCA_FINISH_MAC_LEN_MASK) \
         << SCA_FINISH_MAC_LEN_SHIFT)                                         \
    | (((_intr_) & SCA_FINISH_TRIGGER_INTR_MASK) \
        << SCA_FINISH_TRIGGER_INTR_SHIFT) )

#define MAKE_CMD_FINISH(_cmd_, _size_, _mac_, _mac_len_, _intr_)              \
    do{                                                                       \
        (_size_) = 0;                                                         \
        (_cmd_)[(_size_)] = MAKE_FINISH_DESC(_mac_len_, _intr_);              \
        (_size_) += 1;                                                        \
        if (0x0u != (_mac_)) {                                                \
            (_cmd_)[(_size_)] = ((uint64_t)_mac_) & 0xFFFFFFFFU;              \
            (_size_) += 1;                                                    \
            (_cmd_)[(_size_)] = (((uint64_t)_mac_) >> 32) & 0xFFFFFFFFU;      \
            (_size_) += 1;                                                    \
        }                                                                     \
        (_size_) *= MERAK_WORD_SIZE;                                          \
        OSAL_LOG_DEBUG_DUMP_DATA("FINISH CMD:", (_cmd_), (_size_));           \
    } while (0);

#define SCA_CLEAR_CMD_SIZE       (1U)
#define MAKE_CLEAR_DESC(_intr_)                                               \
    ( MAKE_CMD(SCA_OP_CODE_CLEAR, CLEAR)                                      \
    | (((_intr_) & SCA_CLEAR_INTR_MASK) << SCA_CLEAR_INTR_SHIFT) )

#define MAKE_CMD_CLEAR(_cmd_, _size_, _intr_)                                 \
    do{                                                                       \
        (_size_) = 0;                                                         \
        (_cmd_)[(_size_)] = MAKE_CLEAR_DESC(_intr_);                          \
        (_size_) += 1;                                                        \
        (_size_) *= MERAK_WORD_SIZE;                                          \
        OSAL_LOG_DEBUG_DUMP_DATA("CLEAR CMD:", (_cmd_), (_size_));            \
    } while (0);


#define SCA_KEY_BITS_64         (64U)
#define SCA_KEY_BITS_128        (128U)
#define SCA_KEY_BITS_192        (192U)
#define SCA_KEY_BITS_256        (256U)

enum {
    SCA_DRV_KEY_BITS_128 = 0,
    SCA_DRV_KEY_BITS_192 = 1,
    SCA_DRV_KEY_BITS_256 = 2,
    SCA_DRV_KEY_BITS_64 = 3,
};

#define TE_DRV_SCA_SELECT_KEY(keybits)  (                       \
        (keybits) == SCA_KEY_BITS_256 ? SCA_DRV_KEY_BITS_256 :  \
        (keybits) == SCA_KEY_BITS_192 ? SCA_DRV_KEY_BITS_192 :  \
        (keybits) == SCA_KEY_BITS_64 ? SCA_DRV_KEY_BITS_64 :    \
        SCA_DRV_KEY_BITS_128)

#define SCA_DRV_KLAD256         (0x1UL << 31)

#define SCA_DRV_ALG_AES         (0x00U)
#define SCA_DRV_ALG_DES         (0x01U)
#define SCA_DRV_ALG_TDES        (0x02U)
#define SCA_DRV_ALG_GFMUL       (0x03U)
#define SCA_DRV_ALG_SM4         (0x04U)
#define SCA_DRV_ALG_SNOW3G      (0x05U)
#define SCA_DRV_ALG_ZUC         (0x06U)
#define SCA_DRV_ALG_KASUMI      (0x07U)

#define SCA_DRV_MODE_ECB        (0x00U)
#define SCA_DRV_MODE_CTR        (0x01U)
#define SCA_DRV_MODE_CBC        (0x02U)
#define SCA_DRV_MODE_OFB        (0x03U)
#define SCA_DRV_MODE_XTS        (0x04U)
#define SCA_DRV_MODE_CBCMAC     (0x05U)
#define SCA_DRV_MODE_CMAC       (0x06U)
#define SCA_DRV_MODE_GCM        (0x07U)
#define SCA_DRV_MODE_CCM        (0x08U)
#define SCA_DRV_MODE_UEA2       (0x09U)
#define SCA_DRV_MODE_EEA3       (0x0AU)
#define SCA_DRV_MODE_EIA3       (0x0BU)
#define SCA_DRV_MODE_F8         (0x0CU)
#define SCA_DRV_MODE_F9         (0x0DU)

#define SCA_DRV_CTR_LEN_32      (0x00U)
#define SCA_DRV_CTR_LEN_64      (0x01U)
#define SCA_DRV_CTR_LEN_96      (0x02U)
#define SCA_DRV_CTR_LEN_128     (0x03U)

#define SCA_DRV_LINKLIST_NODE_MAX_LEN       (LLST_ENTRY_SZ_MAX)

#define KEY_SRC_MODK            (0x00U)
#define KEY_SRC_ROOTK           (0x01U)
#define KEY_SRC_EXTERNAL        (0x02U)
#define KLAD_256                (0x1U << SCA_INIT_KLAD256_SHIFT)

#define ALIGNED_SIZE            (LINK_LIST_ALIGN)

#ifdef CFG_TE_ASYNC_EN
/* HWORKER COMMAND */
#define HWORKER_CMD_NONE    (0)
#define HWORKER_CMD_QUIT    (1)

/* HWORKER STATE */
#define HWORKER_ST_STOPPED  (0)
#define HWORKER_ST_RUNNING  (1)
#define HWORKER_ST_SLEEPING (2)
#endif

/**
 * sca drv key
 */
typedef struct sca_drv_key {
    te_key_type_t type;             /**< key type */
    /**
     * key descriptor
     */
    te_sec_key_t sec;               /**< secure key */
    te_sec_key_v2_t sec_v2;         /**< secure key for key-ladder-256 */
    te_user_key_t user;             /**< user key */

    /**
     * secondary key (user key only).
     * some algs require for a secondary key, i.e. GCM mode.
     */
    te_user_key_t user2;
} sca_drv_key_t;

/**
 * SCA context magic number
 */
#define SCA_CTX_MAGIC   0x43616373U /**< "scaC" */
/**
 * SCA context structure
 */
typedef struct sca_drv_ctx {
    te_crypt_ctx_t base;            /**< base context */
    uint32_t magic;                 /**< SCA ctx magic */
    te_algo_t malg;                 /**< main algorithm */
    te_sca_operation_t op;          /**< operation mode */
    sca_drv_key_t key;              /**< cipher key */
    te_sca_state_t state;           /**< sca state */
    te_sess_id_t sess;              /**< session handler */
} sca_drv_ctx_t;

/**
 * SCA power management context
 */
typedef struct sca_pm_ctx {
    struct te_sca_ctl ctl;          /**< Saved ctl register in suspend */
    struct te_sca_int msk;          /**< Saved intr mask */
    struct te_sca_suspd_msk suspd;  /**< Saved suspend mask */
} sca_pm_ctx_t ;

typedef struct sca_payload {
    uint32_t *buf;
    size_t size;
} sca_payload_t;

typedef struct sca_prvdata {
    uint32_t caps;            /**< algorithms capacity bitmap */
} sca_prvdata_t;

#define SCA_EHDR_SIZE(x)   (sizeof(sca_ehdr_t) + (x)->hwctx_sz)
#define SCA_EHDR_HWCTX(x)  (uint8_t *)(((sca_ehdr_t *)(x)) + 1)

/**
 * SCA export state header magic number
 */
#define SCA_EHDR_MAGIC     0x72484553U /**< "SEHr" */

/**
 * SCA export state header structure
 */
typedef struct sca_export_hdr {
    uint32_t magic;                 /**< magic */
    te_algo_t alg;                  /**< algorithm identifier */
    te_sca_operation_t op;          /**< operation mode */
    te_sca_state_t state;           /**< sca state */
    uint32_t hwctx_sz;              /**< hwctx size in byte */
    /*
     * Commented out element used to visualize the layout dynamic part
     * of the struct.
     *
     * uint8_t hwctx[];
     */
} sca_ehdr_t;

#ifdef CFG_TE_ASYNC_EN
/**
 *  worker thread
 */
typedef struct sca_worker {
    osal_thread_t wthread;
    osal_spin_lock_t lock;
    osal_completion_t bell;
    volatile uint32_t command;
    volatile uint32_t state;
    sqlist_t tasks;
} sca_worker_t;

typedef struct sca_async_ctx {
    sqlist_t list;
    sca_drv_ctx_t *ctx;
    te_sca_request_t *req;
    te_sess_ar_t *ar;
    union {
        struct {
            link_list_t *in_ll;
            link_list_t *out_ll;
            bool last_update;
        } up;
        struct {
            uint8_t *tag;
        } fin;
    };
    void(*done)(struct sca_async_ctx *ctx);
} sca_async_ctx_t;

static osal_err_t hworker_thread_entry( void *arg );
static sca_worker_t *sca_worker_init(void);
static void sca_worker_destroy( sca_worker_t *worker );
#endif

static inline bool _alg_id_sanity_check_main_algo(te_algo_t malg)
{
    return ( (TE_MAIN_ALGO_AES == malg) ? true :
             (TE_MAIN_ALGO_DES == malg) ? true :
             (TE_MAIN_ALGO_TDES == malg)? true :
             (TE_MAIN_ALGO_SM4 == malg) ? true :
             (TE_MAIN_ALGO_GFMUL == malg) ? true :
             (TE_MAIN_ALGO_SNOW3G == malg) ? true :
             (TE_MAIN_ALGO_ZUC == malg) ? true :
             (TE_MAIN_ALGO_KASUMI == malg) ? true :
                                          false );
}

static int _alg_id_supported_check_main_algo(te_sca_drv_t *drv, te_algo_t malg)
{
    int ret = TE_SUCCESS;

    if ( (NULL == drv) ||
         (NULL == drv->pd) ) {
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }

    switch (malg) {
        case TE_MAIN_ALGO_SNOW3G:
            if ( (drv->pd->caps & (1 << SCA_DRV_ALG_SNOW3G)) == 0 ) {
                ret = TE_ERROR_NOT_SUPPORTED;
            }
            break;
        case TE_MAIN_ALGO_ZUC:
            if ( (drv->pd->caps & (1 << SCA_DRV_ALG_ZUC)) == 0 ) {
                ret = TE_ERROR_NOT_SUPPORTED;
            }
            break;
        case TE_MAIN_ALGO_KASUMI:
            if ( (drv->pd->caps & (1 << SCA_DRV_ALG_KASUMI)) == 0 ) {
                ret = TE_ERROR_NOT_SUPPORTED;
            }
            break;
        default:
            break;
    }

__out__:
    return ret;
}

static int te_sca_drv_suspend( struct te_crypt_drv* drv, te_suspend_state_t state )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *hdrv = (te_sca_drv_t *)drv;
    te_hwa_sca_t *hwa = NULL;
    te_sca_ctl_t ctl = {0};

    TE_ASSERT(NULL != hdrv);
    TE_ASSERT(SCA_DRV_MAGIC == hdrv->magic);
    TE_ASSERT(NULL != hdrv->sctx);

    if (TE_SUSPEND_MEM == state) {
        ret = te_sess_module_suspend( hdrv->sctx );
        if (TE_SUCCESS != ret) {
            _SCA_DRV_OUT_;
        }
    }

    hwa = (te_hwa_sca_t *)hdrv->base.hwa;
#ifdef CFG_TE_IRQ_EN
    ret = hwa->get_int_msk( hwa, &hdrv->pm->msk );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't get sca intr mask!\n" );
#endif

    ret = hwa->get_suspd_msk( hwa, &hdrv->pm->suspd );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't get sca suspd mask!\n" );

    ret = hwa->get_ctrl( hwa, &hdrv->pm->ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't get sca ctrl!\n" );

    /* Stop the engine */
    ctl.csq_en = 0;
    ctl.cq_wm = 0;
    ctl.clk_en = 0;
    ctl.run = 0;
    ret = hwa->set_ctrl( hwa, &ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set sca ctrl!\n" );
__out__:
    return ret;
}

static int te_sca_drv_resume( struct te_crypt_drv* drv, te_suspend_state_t state )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *hdrv = (te_sca_drv_t *)drv;
    te_hwa_sca_t *hwa = NULL;

    TE_ASSERT(NULL != hdrv);
    TE_ASSERT(SCA_DRV_MAGIC == hdrv->magic);
    TE_ASSERT(NULL != hdrv->sctx);
    hwa = (te_hwa_sca_t *)hdrv->base.hwa;

#ifdef CFG_TE_IRQ_EN
    ret = hwa->set_int_msk( hwa, &hdrv->pm->msk );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set sca intr mask!\n" );
#endif

    ret = hwa->set_suspd_msk( hwa, &hdrv->pm->suspd );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set sca suspd mask!\n" );

    ret = hwa->set_ctrl( hwa, &hdrv->pm->ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set sca ctrl!\n" );

    if (TE_SUSPEND_MEM == state) {
        ret = te_sess_module_resume( hdrv->sctx );
    }

    return ret;
}

static void te_sca_drv_destroy( struct te_crypt_drv* drv )
{
    int ret = TE_SUCCESS;
    te_sca_drv_t *hdrv = (te_sca_drv_t *)drv;
    te_hwa_sca_t *hwa = NULL;
    te_sca_ctl_t ctl = {0};
#ifdef CFG_TE_IRQ_EN
    te_sca_int_t msk __te_unused = { 0 };
#endif

    TE_ASSERT(NULL != hdrv);
    TE_ASSERT(SCA_DRV_MAGIC == hdrv->magic);
    TE_ASSERT(NULL != hdrv->sctx);
    hwa = (te_hwa_sca_t *)hdrv->base.hwa;
    te_sess_module_deinit( hdrv->sctx );
#ifdef CFG_TE_ASYNC_EN
    sca_worker_destroy( hdrv->worker );
#endif
 /* Stop the engine */
    ctl.csq_en = 0;
    ctl.cq_wm = 0;
    ctl.clk_en = 0;
    ctl.run = 0;
    ret = hwa->set_ctrl( hwa, &ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set sca ctrl!\n" );

#ifdef CFG_TE_IRQ_EN
    /* Disable host interrupts */
    msk.stat.cq_wm = 1;
    msk.stat.opcode_err = 1;
    msk.stat.csq_rd_err = 1;
    msk.stat.cq_wr_err = 1;
    msk.stat.axi_to_err = 1;
    /* Disable all slots finish interrupt */
    msk.cmd_fin = 0xffffffffUL;
    /* Disable all slots error interrupt */
    msk.op_err = 0xffffffffUL;
    ret = hwa->set_int_msk( hwa, &msk );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set sca intr mask!\n" );
#endif

    osal_free( hdrv->pm );
    osal_free( hdrv->pd );
    osal_memset( hdrv, 0, sizeof(*hdrv) );
}

static int te_sca_drv_capacity( te_sca_drv_t *drv, uint32_t *capacity)
{
    int ret = TE_SUCCESS;
    te_rtl_conf_t conf = {0};
    te_hwa_host_t *hwa_host = NULL;
    te_hwa_stat_t *hwa_stat = NULL;

    if ( (NULL == drv ) || (NULL == capacity) ||
         (SCA_DRV_MAGIC != drv->magic) ) {
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }

    hwa_host = hwa_crypt_host( (te_hwa_crypt_t *)drv->base.hwa );
    TE_ASSERT( hwa_host != NULL );
    hwa_stat = &hwa_host->stat;
    TE_ASSERT( hwa_stat != NULL );

    /* Set default algorithm bitmap */
    *capacity = 0xFF;

    /* Check the supported algorithms and reset bitmap */
    ret = hwa_stat->conf( hwa_stat, &conf );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                    "Fatal error, can't read host state conf!\n" );
    if ( !conf.cfg0.sca_esnow3g ) {
        *capacity &= ~(1 << SCA_DRV_ALG_SNOW3G);
    }
    if ( !conf.cfg0.sca_ezuc ) {
        *capacity &= ~(1 << SCA_DRV_ALG_ZUC);
    }
    if ( !conf.cfg0.sca_ekasumi ) {
        *capacity &= ~(1 << SCA_DRV_ALG_KASUMI);
    }
   /* Check if support key-ladder 256 and mark the bitmap */
    if ( conf.cfg1.otp_skey_256 ) {
        *capacity |= SCA_DRV_KLAD256;
    }

__out__:
    return ret;
}

int te_sca_drv_init( te_sca_drv_t *drv,
                     const te_hwa_sca_t *hwa,
                     const char* name )
{
    int ret = TE_SUCCESS;
    te_hwa_host_t *hwa_host = NULL;
    te_hwa_stat_t *hwa_stat = NULL;
    te_sca_ctl_t ctl = { 0 };
    te_rtl_conf_t conf = { 0 };
#ifdef CFG_TE_IRQ_EN
    te_sca_int_t msk __te_unused = { 0 };
    te_sca_int_t intr __te_unused = { 0 };
#endif

    if( (NULL == drv ) || (NULL == hwa) ){
        ret = TE_ERROR_BAD_PARAMS;
        goto __out__;
    }

    if (drv->magic == SCA_DRV_MAGIC && osal_atomic_load(&drv->base.refcnt)) {
        /* already initialized */
        return TE_SUCCESS;
    }

    memset(drv, 0x00, sizeof(te_sca_drv_t));
    drv->pm = (sca_pm_ctx_t *)osal_calloc(1, sizeof(sca_pm_ctx_t));
    if (NULL == drv->pm) {
        return TE_ERROR_OOM;
    }
    drv->pd = (sca_prvdata_t *)osal_calloc(1, sizeof(sca_prvdata_t));
    if (NULL == drv->pd) {
        osal_free(drv->pm);
        return TE_ERROR_OOM;
    }
    drv->sctx = te_sess_module_init( (te_hwa_sca_t *)hwa, false );
    if ( drv->sctx == NULL ) {
        osal_free(drv->pm);
        osal_free(drv->pd);
        return TE_ERROR_OOM;
    }
#ifdef CFG_TE_ASYNC_EN
    drv->worker = sca_worker_init();
    if ( drv->worker == NULL ) {
        osal_free( drv->pm );
        osal_free( drv->pd );
        te_sess_module_deinit( drv->sctx );
        return TE_ERROR_OOM;
    }
#endif
    drv->magic = SCA_DRV_MAGIC;
    drv->base.hwa = (te_hwa_crypt_t *)hwa;
    if ( NULL != name ) {
        osal_strncpy(drv->base.name, name, TE_MAX_DRV_NAME - 1);
    }

    /* reset refcnt */
    osal_atomic_store( &drv->base.refcnt, 0U );
    /* install hooks */
    drv->base.suspend = te_sca_drv_suspend;
    drv->base.resume = te_sca_drv_resume;
    drv->base.destroy = te_sca_drv_destroy;

    hwa_host = hwa_crypt_host( (te_hwa_crypt_t *)&hwa->base );
    hwa_stat = &hwa_host->stat;

    ret = hwa_stat->conf( hwa_stat, &conf );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't read host top conf!\n" );

    ret = te_sca_drv_capacity( drv, &drv->pd->caps );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't read sca supported algorithms!\n" );

    ctl.csq_en = 1;
    ctl.cq_wm = 1;
#ifndef CFG_TE_DYNCLK_CTL
    ctl.clk_en = 1;
#endif
    ctl.run = 1;
    ret = hwa->set_ctrl( (te_hwa_sca_t *)hwa, &ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set sca ctrl!\n" );

#ifdef CFG_TE_IRQ_EN
    /* Cleanup intr status */
    ret = hwa->int_state( (te_hwa_sca_t *)hwa, &intr );
    TE_ASSERT_MSG( (ret == TE_SUCCESS), "Fatal error, Can't get interrupt stat\n");

    ctl.clk_en = 1;
    ret = hwa->set_ctrl( (te_hwa_sca_t *)hwa, &ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set sca ctrl!\n" );

    ret = hwa->eoi( (te_hwa_sca_t *)hwa, &intr );
    TE_ASSERT_MSG( (ret == TE_SUCCESS), "Fatal error, Can't ack interrupt\n");

#ifdef CFG_TE_DYNCLK_CTL
    ctl.clk_en = 0;
    ret = hwa->set_ctrl( (te_hwa_sca_t *)hwa, &ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set sca ctrl!\n" );
#endif

    /* Enable host interrupts */
    msk.stat.cq_wm = 1;
    msk.stat.opcode_err = 0;
    msk.stat.csq_rd_err = 0;
    msk.stat.cq_wr_err = 0;
    msk.stat.axi_to_err = 0;
    /* Enable all slots finish interrupt */
    msk.cmd_fin = 0;
    /* Enable all slots error interrupt */
    msk.op_err = 0;
    ret = hwa->set_int_msk( (te_hwa_sca_t *)hwa, &msk );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set sca intr mask!\n" );
#endif

    ret = te_crypt_drv_get(&drv->base);
__out__:
    return ret;
}

int te_sca_drv_exit( te_sca_drv_t *drv )
{
    int ret = TE_SUCCESS;
    if(NULL == drv || SCA_DRV_MAGIC != drv->magic){
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }

    te_crypt_drv_put(&drv->base);
__out__:
    return ret;
}

int te_sca_alloc_ctx( struct te_sca_drv *drv,
                      te_algo_t malg,
                      uint32_t size,
                      te_crypt_ctx_t **ctx )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;

    if ( (NULL == ctx)
        || (NULL == drv)
        || !_alg_id_sanity_check_main_algo(malg)
        || (SCA_DRV_MAGIC != drv->magic) ) {
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }

    /* Check whether engine supports algorithm */
    ret = _alg_id_supported_check_main_algo(drv, malg);
    if ( TE_SUCCESS != ret ) {
        _SCA_DRV_OUT_;
    }

    /* set main algorithm in algorithm identifier */
    drv_ctx = osal_calloc(1, sizeof(sca_drv_ctx_t) + size);

    if ( NULL == drv_ctx ) {
        ret = TE_ERROR_OOM;
        _SCA_DRV_OUT_;
    }

    drv_ctx->base.__ctx = drv_ctx + 1;
    drv_ctx->base.drv = &drv->base;
    drv_ctx->base.alg = malg;  /* Setting alg is to survive te_sca_clone() on
                                * a not started src ctx. This is all we can do
                                * by the moment for the alg is not given yet.
                                * Whatever, this is harmless for the alg is
                                * supposed to be updated later on.
                                */
    drv_ctx->magic = SCA_CTX_MAGIC;
    drv_ctx->malg = malg;
    drv_ctx->sess = INVALID_SESS_ID;
    *ctx = &drv_ctx->base;

    /* initialize metadata related with algoritm */
    switch (malg) {
    default:
    case TE_MAIN_ALGO_AES:
        (*ctx)->blk_size = TE_AES_BLOCK_SIZE;
        break;
    case TE_MAIN_ALGO_DES:
    case TE_MAIN_ALGO_TDES:
        (*ctx)->blk_size = TE_DES_BLOCK_SIZE;
        break;
    case TE_MAIN_ALGO_SM4:
        (*ctx)->blk_size = TE_SM4_BLOCK_SIZE;
        break;
    case TE_MAIN_ALGO_GFMUL:
        (*ctx)->blk_size = TE_MAX_SCA_BLOCK;
        break;
    case TE_MAIN_ALGO_SNOW3G:
        (*ctx)->blk_size = TE_SNOW3G_BLOCK_SIZE;
        break;
    case TE_MAIN_ALGO_ZUC:
        (*ctx)->blk_size = TE_ZUC_BLOCK_SIZE;
        break;
    case TE_MAIN_ALGO_KASUMI:
        (*ctx)->blk_size = TE_KASUMI_BLOCK_SIZE;
        break;
    }
    (*ctx)->ctx_size = size;
    drv_ctx->state = TE_DRV_SCA_STATE_INIT;
    te_crypt_drv_get(&drv->base);
    _SCA_DRV_OUT_;
__out__:
    return ret;
}

int te_sca_free_ctx( te_crypt_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;

    if (NULL == ctx) {
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }

    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );
    /* State machine check */
    switch (drv_ctx->state) {
    default:
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        _SCA_DRV_OUT_;
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    }
    if( NULL != drv_ctx->key.user.key ){
        osal_memset(drv_ctx->key.user.key, 0x00,
                    drv_ctx->key.user.keybits / BYTE_BITS);
        osal_free(drv_ctx->key.user.key);
        drv_ctx->key.user.key = NULL;
    }

    if( NULL != drv_ctx->key.user2.key ){
        osal_memset(drv_ctx->key.user2.key, 0x00,
                    drv_ctx->key.user2.keybits / BYTE_BITS);
        osal_free(drv_ctx->key.user2.key);
        drv_ctx->key.user2.key = NULL;
    }

    te_crypt_drv_put(ctx->drv);
    osal_free(drv_ctx);
__out__:
    return ret;
}

static int _sanity_check_sca_user_key(te_crypt_ctx_t *ctx,
                                          te_user_key_t *key ) {
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = (sca_drv_ctx_t *)ctx;
    /* check key length */
    switch (drv_ctx->malg) {
    case TE_MAIN_ALGO_AES:
        if( ((SCA_KEY_BITS_128 == key->keybits)
            || ((SCA_KEY_BITS_192 == key->keybits)
                 && (TE_CHAIN_MODE_XTS != TE_ALG_GET_CHAIN_MODE(ctx->alg)))
            || (SCA_KEY_BITS_256 == key->keybits)) ){
              ret = TE_SUCCESS;
          } else {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    case TE_MAIN_ALGO_DES:
        if((SCA_KEY_BITS_64 != key->keybits)) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    case TE_MAIN_ALGO_TDES:
        if((SCA_KEY_BITS_192 != key->keybits)) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    case TE_MAIN_ALGO_SM4:
        if((SCA_KEY_BITS_128 != key->keybits)) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    case TE_MAIN_ALGO_GFMUL:
        if((SCA_KEY_BITS_128 != key->keybits) &&
           (SCA_KEY_BITS_64 != key->keybits)) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    case TE_MAIN_ALGO_SNOW3G:
        if((SCA_KEY_BITS_128 != key->keybits)) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    case TE_MAIN_ALGO_ZUC:
        if((SCA_KEY_BITS_128 != key->keybits)) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    case TE_MAIN_ALGO_KASUMI:
        if((SCA_KEY_BITS_128 != key->keybits)) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    default:
        ret = TE_ERROR_BAD_PARAMS;
        break;
    }

    return ret;
}

static int _sanity_check_sca_sec_key(te_crypt_ctx_t *ctx,
                                             te_sec_key_t *key ) {
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = (sca_drv_ctx_t *)ctx;

    /* check key selection */
    if( (TE_KL_KEY_MODEL != key->sel)
        && (TE_KL_KEY_ROOT != key->sel)){
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }

    /* check key length */
    switch ( drv_ctx->malg )
    {
    case TE_MAIN_ALGO_AES:
        if( (SCA_KEY_BITS_128 != key->ek3bits )
            && (SCA_KEY_BITS_256 != key->ek3bits ) ) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    case TE_MAIN_ALGO_DES: /* doesn't support key ladder */
        ret = TE_ERROR_BAD_PARAMS;
        break;
    case TE_MAIN_ALGO_TDES: /* doesn't support key ladder */
        ret = TE_ERROR_BAD_PARAMS;
        break;
    case TE_MAIN_ALGO_SM4:
        if( (SCA_KEY_BITS_128 != key->ek3bits ) ) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    case TE_MAIN_ALGO_SNOW3G:
        if((SCA_KEY_BITS_128 != key->ek3bits)) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    case TE_MAIN_ALGO_ZUC:
        if((SCA_KEY_BITS_128 != key->ek3bits)) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    case TE_MAIN_ALGO_KASUMI:
        if((SCA_KEY_BITS_128 != key->ek3bits)) {
              ret = TE_ERROR_BAD_KEY_LENGTH;
          }
        break;
    default:
        ret = TE_ERROR_BAD_PARAMS;
        break;
    }

__out__:
    return ret;
}

static int _sanity_check_sca_sec_key_v2(te_crypt_ctx_t *ctx,
                                        te_sec_key_v2_t *key ) {
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = (sca_drv_ctx_t *)ctx;

    /* check key selection */
    if( (TE_KL_KEY_MODEL != key->sel) && (TE_KL_KEY_ROOT != key->sel) ){
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }

    /* check key length, key-ladder 256 only supports AES */
    switch ( drv_ctx->malg )
    {
    case TE_MAIN_ALGO_AES:
        if( SCA_KEY_BITS_256 != key->ek3bits ) {
            ret = TE_ERROR_BAD_KEY_LENGTH;
        }
        break;
    default:
        ret = TE_ERROR_BAD_PARAMS;
        break;
    }

__out__:
    return ret;
}

static int _sanity_check_sca_key(te_crypt_ctx_t *ctx,
                                          te_sca_key_t *key ) {
    int ret = TE_SUCCESS;

    /* check key type */
    if( ( TE_KEY_TYPE_SEC != key->type ) &&
        ( TE_KEY_TYPE_USER != key->type ) &&
        ( TE_KEY_TYPE_SEC_V2 != key->type ) ){
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }

    /* check key length */
    switch (key->type)
    {
    case TE_KEY_TYPE_USER:
        if (!key->user.key) {
            ret = TE_ERROR_BAD_PARAMS;
            _SCA_DRV_OUT_;
        }
        ret = _sanity_check_sca_user_key(ctx, &key->user);
        break;
    case TE_KEY_TYPE_SEC:
        ret = _sanity_check_sca_sec_key(ctx, &key->sec);
        break;
    case TE_KEY_TYPE_SEC_V2:
        ret = _sanity_check_sca_sec_key_v2(ctx, &key->sec_v2);
        break;
    default:
        ret = TE_ERROR_BAD_PARAMS;
        break;
    }
    __SCA_DRV_CHECK_CONDITION__(ret);
    /** check ghash key length */
    if (TE_CHAIN_MODE_GCM == TE_ALG_GET_CHAIN_MODE(ctx->alg)) {
        if (key->user2.key == NULL) {
            ret = TE_ERROR_BAD_PARAMS;
        }
        __SCA_DRV_CHECK_CONDITION__(ret);
        if (key->user2.keybits != SCA_KEY_BITS_128) {
            ret = TE_ERROR_BAD_KEY_LENGTH;
        }
    }
    __SCA_DRV_CHECK_CONDITION__(ret);
    /** overwrite gfmul block size */
    if (TE_MAIN_ALGO_GFMUL == TE_ALG_GET_MAIN_ALG(ctx->alg)) {
        if (key->user.keybits == SCA_KEY_BITS_128) {
            ctx->blk_size = TE_GHASH_BLOCK_SIZE;
        } else if (key->user.keybits == SCA_KEY_BITS_64) {
            ctx->blk_size = TE_EVAL_BLOCK_SIZE;
        } else {
            ret = TE_ERROR_BAD_KEY_LENGTH;
        }
    }

__out__:
    return ret;
}

int te_sca_setkey( te_crypt_ctx_t *ctx, te_sca_key_t *key )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;
    te_sca_drv_t *drv = NULL;

    if ( (NULL == ctx) || (NULL == key) ) {
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }
    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );
    drv = (te_sca_drv_t *)drv_ctx->base.drv;
    TE_ASSERT_MSG( drv->magic == SCA_DRV_MAGIC,
                "fatal error: Not valid SCA driver instance\n" );
    switch (drv_ctx->state)
    {
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
        break;
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
    default:
        ret = TE_ERROR_BAD_STATE;
        _SCA_DRV_OUT_;
    }

    ret = _sanity_check_sca_key(ctx, key);
    __SCA_DRV_CHECK_CONDITION__(ret);

    drv_ctx->key.type = key->type;

    if ( TE_KEY_TYPE_USER == key->type ) {
        if ((NULL == drv_ctx->key.user.key)
            || ((NULL != drv_ctx->key.user.key)
                 && drv_ctx->key.user.keybits != key->user.keybits)){
            if (NULL != drv_ctx->key.user.key) {
                osal_memset(drv_ctx->key.user.key, 0x00,
                            drv_ctx->key.user.keybits / BYTE_BITS);
                osal_free(drv_ctx->key.user.key);
                drv_ctx->key.user.key = NULL;
            }
            drv_ctx->key.user.keybits = key->user.keybits;
            drv_ctx->key.user.key = (uint8_t *)osal_calloc(
                            drv_ctx->key.user.keybits / BYTE_BITS ,
                            sizeof(uint8_t));
            if (NULL == drv_ctx->key.user.key) {
                ret = TE_ERROR_OOM;
                _SCA_DRV_OUT_;
            }
        }
        osal_memcpy(drv_ctx->key.user.key,
                key->user.key,
                 key->user.keybits / BYTE_BITS);
    } else {
        if (TE_KEY_TYPE_SEC == key->type) {
            osal_memcpy(&drv_ctx->key.sec, &key->sec, sizeof(te_sec_key_t));
        } else {
            if (!(drv->pd->caps & SCA_DRV_KLAD256)) {
                OSAL_LOG_ERR("key-ladder 256 is not supported!\n");
                return TE_ERROR_NOT_SUPPORTED;
            }
            osal_memcpy(&drv_ctx->key.sec_v2, &key->sec_v2,
                        sizeof(te_sec_key_v2_t));
        }
    }
    /* set usr key 2 for gcm(ghash key) only*/
    if (key->user2.key != NULL) {
        if ((NULL == drv_ctx->key.user2.key)
            || ((NULL != drv_ctx->key.user2.key)
                && drv_ctx->key.user2.keybits != key->user2.keybits)){
            if (NULL != drv_ctx->key.user2.key) {
                osal_memset(drv_ctx->key.user2.key, 0x00,
                            drv_ctx->key.user2.keybits / BYTE_BITS);
                osal_free(drv_ctx->key.user2.key);
                drv_ctx->key.user2.key = NULL;
            }
            drv_ctx->key.user2.keybits = key->user2.keybits;
            drv_ctx->key.user2.key = (uint8_t *)osal_calloc(1,
                            drv_ctx->key.user2.keybits / BYTE_BITS );
            if (NULL == drv_ctx->key.user2.key) {
                ret = TE_ERROR_OOM;
                _SCA_DRV_OUT_;
            }
        }
        osal_memcpy(drv_ctx->key.user2.key,
                key->user2.key,
                key->user2.keybits / BYTE_BITS);
    }

    drv_ctx->state = TE_DRV_SCA_STATE_READY;
__out__:
    return ret;
}

static inline int _te_sca_clear(sca_drv_ctx_t *drv_ctx)
{
    int ret = TE_SUCCESS;
    uint32_t payload[SCA_CLEAR_CMD_SIZE] = {0};
    size_t payload_size = 0;

    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );

    MAKE_CMD_CLEAR(payload, payload_size, TRIGGER_INT);
    ret = te_sess_submit(drv_ctx->sess, payload, payload_size);
    return ret;
}

int te_sca_reset( te_crypt_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;

    __SCA_DRV_VERIFY_PARAMS__(ctx);

    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );
    switch (drv_ctx->state)
    {
    default:
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_READY:
    case TE_DRV_SCA_STATE_RAW:
        ret = TE_ERROR_BAD_STATE;
        _SCA_DRV_OUT_;
    case TE_DRV_SCA_STATE_START:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
        break;
    }

    ret = _te_sca_clear(drv_ctx);
    __SCA_DRV_CHECK_CONDITION__(ret);
    ret = te_sess_close(drv_ctx->sess);
    __SCA_DRV_CHECK_CONDITION__(ret);
    drv_ctx->sess = INVALID_SESS_ID;
    drv_ctx->state = TE_DRV_SCA_STATE_READY;
__out__:
    return ret;
}

static void sca_error_cleanup( te_crypt_ctx_t *ctx )
{
    int ret = TE_ERROR_GENERIC;
    sca_drv_ctx_t *drv_ctx = (sca_drv_ctx_t *)ctx;
    ret = te_sess_cancel( drv_ctx->sess );
    TE_ASSERT( ret == TE_SUCCESS );

    ret = _te_sca_clear(drv_ctx);
    TE_ASSERT( ret == TE_SUCCESS );

    ret = te_sess_close( drv_ctx->sess );
    TE_ASSERT( ret == TE_SUCCESS );
    drv_ctx->sess = INVALID_SESS_ID;
    /* Roll back to ready stat, user can start or free */
    drv_ctx->state = TE_DRV_SCA_STATE_READY;
    return;
}

int te_sca_state( te_crypt_ctx_t *ctx )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;

    __SCA_DRV_VERIFY_PARAMS__(ctx);

    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );
    ret = drv_ctx->state;
__out__:
    return ret;
}

static bool _te_sca_is_iv_mandatory(te_crypt_ctx_t *ctx)
{
    bool mandatory = false;

    switch (TE_ALG_GET_CHAIN_MODE(ctx->alg)) {
        case TE_CHAIN_MODE_ECB_NOPAD:
            mandatory = false;
            break;
        case TE_CHAIN_MODE_F9:
            if (TE_MAIN_ALGO_KASUMI == TE_ALG_GET_MAIN_ALG(ctx->alg)) {
                mandatory = false;
            } else {
                mandatory = true;
            }
            break;
        default:
            mandatory = true;
            break;
    }
    return mandatory;
}

static bool _te_sca_sanity_check_iv(te_crypt_ctx_t *ctx, uint8_t *iv,
                                    size_t ivlen)
{
    if (_te_sca_is_iv_mandatory(ctx)) {
        switch (TE_ALG_GET_CHAIN_MODE(ctx->alg)) {
            case TE_CHAIN_MODE_CCM:
                if (ivlen != ctx->blk_size * 2) {
                    return false;
                }
                break;
            case TE_CHAIN_MODE_F8:
            case TE_CHAIN_MODE_F9:
                switch (TE_ALG_GET_MAIN_ALG(ctx->alg)) {
                    case TE_MAIN_ALGO_SNOW3G:
                        if (ivlen != ctx->blk_size * 4) {
                            return false;
                        }
                        break;
                    case TE_MAIN_ALGO_ZUC:
                        if (ivlen != ctx->blk_size * 4) {
                            return false;
                        }
                        break;
                    case TE_MAIN_ALGO_KASUMI:
                        if (ivlen != ctx->blk_size) {
                            return false;
                        }
                        break;
                    default:
                        return false;
                }
                break;
            default:
                if (ivlen != ctx->blk_size) {
                    return false;
                }
                break;
        }
        if (NULL == iv) {
            return false;
        }
    } else {
        if (iv != NULL || ivlen != 0) {
            return false;
        }
  }
    return true;
}

static int _te_sca_prepare_start( te_crypt_ctx_t *ctx,
                                  te_sca_operation_t op,
                                  uint8_t *iv,
                                  uint32_t ivlen,
                                  sca_payload_t *payload,
                                  te_sess_slot_cat_t *cat )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;
    uint32_t malg = 0;
    uint8_t iv_load = 0x00;
    uint32_t mode = 0;
    uint8_t key_src = 0;
    uint32_t ctr_len = 0;
    uint32_t ns_attr = 0U;

    __SCA_DRV_VERIFY_PARAMS__(ctx);
    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );
    /* sanity check operation && main alg */
    if (((TE_DRV_SCA_ENCRYPT != op) && (TE_DRV_SCA_DECRYPT != op))
         && (drv_ctx->malg != TE_ALG_GET_MAIN_ALG(ctx->alg))){
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }
    /* sanity check iv */
    if (!_te_sca_sanity_check_iv(ctx, iv, ivlen)){
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }

    switch (drv_ctx->state) {
    default:
    case TE_DRV_SCA_STATE_RAW:
    case TE_DRV_SCA_STATE_INIT:
    case TE_DRV_SCA_STATE_UPDATE:
    case TE_DRV_SCA_STATE_LAST:
    case TE_DRV_SCA_STATE_START:
        ret = TE_ERROR_BAD_STATE;
        _SCA_DRV_OUT_;
        break;
    case TE_DRV_SCA_STATE_READY:
        switch (TE_ALG_GET_MAIN_ALG(ctx->alg)) {
        case TE_MAIN_ALGO_DES:
            malg = SCA_DRV_ALG_DES;
            break;
        case TE_MAIN_ALGO_TDES:
            malg = SCA_DRV_ALG_TDES;
            break;
        case TE_MAIN_ALGO_AES:
            malg = SCA_DRV_ALG_AES;
            break;
        case TE_MAIN_ALGO_SM4:
            malg = SCA_DRV_ALG_SM4;
            break;
        case TE_MAIN_ALGO_GFMUL:
            malg = SCA_DRV_ALG_GFMUL;
            break;
        case TE_MAIN_ALGO_SNOW3G:
            malg = SCA_DRV_ALG_SNOW3G;
            break;
        case TE_MAIN_ALGO_ZUC:
            malg = SCA_DRV_ALG_ZUC;
            break;
        case TE_MAIN_ALGO_KASUMI:
            malg = SCA_DRV_ALG_KASUMI;
            break;
        default:
            TE_ASSERT_MSG( false, "fatal error: bad main algorithm\n" );
            break;
        }

        switch (TE_ALG_GET_CHAIN_MODE(ctx->alg)) {
        case TE_CHAIN_MODE_ECB_NOPAD:
            mode = SCA_DRV_MODE_ECB;
            *cat = TE_SLOT_CATEGORY_SHORT;
            break;
        case TE_CHAIN_MODE_CBC_NOPAD:
            if (TE_OPERATION_MAC == TE_ALG_GET_CLASS(ctx->alg)) {
                mode = SCA_DRV_MODE_CBCMAC;
            } else {
                mode = SCA_DRV_MODE_CBC;
            }
            *cat = TE_SLOT_CATEGORY_SHORT;
            break;
        case TE_CHAIN_MODE_CTR:
            mode = SCA_DRV_MODE_CTR;
            ctr_len = SCA_DRV_CTR_LEN_128;
            *cat = TE_SLOT_CATEGORY_SHORT;
            break;
        case TE_CHAIN_MODE_OFB:
            mode = SCA_DRV_MODE_OFB;
            *cat = TE_SLOT_CATEGORY_SHORT;
            break;
        case TE_CHAIN_MODE_XTS:
            mode = SCA_DRV_MODE_XTS;
            *cat = TE_SLOT_CATEGORY_SHORT;
            break;
        case TE_CHAIN_MODE_CBC_MAC_PKCS5:
            mode = SCA_DRV_MODE_CBCMAC;
            *cat = TE_SLOT_CATEGORY_SHORT;
            break;
        case TE_CHAIN_MODE_CMAC:
            mode = SCA_DRV_MODE_CMAC;
            *cat = TE_SLOT_CATEGORY_SHORT;
            break;
        case TE_CHAIN_MODE_CCM:
            mode = SCA_DRV_MODE_CCM;
            ctr_len = SCA_DRV_CTR_LEN_128;
            *cat = TE_SLOT_CATEGORY_LONG;
            break;
        case TE_CHAIN_MODE_GCM:
            mode = SCA_DRV_MODE_GCM;
            ctr_len = SCA_DRV_CTR_LEN_32;
            *cat = TE_SLOT_CATEGORY_LONG;
            break;
        case TE_CHAIN_MODE_F8:
            switch (malg) {
                case SCA_DRV_ALG_SNOW3G:
                    mode = SCA_DRV_MODE_UEA2;
                    *cat = TE_SLOT_CATEGORY_LONG;
                    break;
                case SCA_DRV_ALG_ZUC:
                    mode = SCA_DRV_MODE_EEA3;
                    *cat = TE_SLOT_CATEGORY_LONG;
                    break;
                case SCA_DRV_ALG_KASUMI:
                    mode = SCA_DRV_MODE_F8;
                    *cat = TE_SLOT_CATEGORY_SHORT;
                    break;
                default:
                    TE_ASSERT_MSG( false, "fatal error: bad chain mode\n" );
                break;
            }
            break;
        case TE_CHAIN_MODE_F9:
            switch (malg) {
                case SCA_DRV_ALG_ZUC:
                    mode = SCA_DRV_MODE_EIA3;
                    *cat = TE_SLOT_CATEGORY_LONG;
                    break;
                case SCA_DRV_ALG_KASUMI:
                    mode = SCA_DRV_MODE_F9;
                    *cat = TE_SLOT_CATEGORY_SHORT;
                    break;
                default:
                    TE_ASSERT_MSG( false, "fatal error: bad chain mode\n" );
                break;
            }
            break;
        default:
            TE_ASSERT_MSG( false, "fatal error: bad chain mode\n" );
            break;
        }

        if ( _te_sca_is_iv_mandatory(ctx)){
            iv_load = 0x01;
            osal_cache_clean(iv, ivlen);
        } else {
            iv_load = 0x00;
        }

        if ( (TE_KEY_TYPE_SEC == drv_ctx->key.type) ||
             (TE_KEY_TYPE_SEC_V2 == drv_ctx->key.type) ) {
            if (TE_KL_KEY_MODEL == drv_ctx->key.sec.sel) {
                key_src = KEY_SRC_MODK;
            } else {
                key_src = KEY_SRC_ROOTK;
            }
            MAKE_CMD_INIT(payload->buf,
                            payload->size,
                            malg,
                            mode,
                            key_src,
                            ((TE_KEY_TYPE_SEC == drv_ctx->key.type) ?
                              TE_DRV_SCA_SELECT_KEY(drv_ctx->key.sec.ek3bits) :
                              TE_DRV_SCA_SELECT_KEY(drv_ctx->key.sec_v2.ek3bits)),
                            TRIGGER_INT,
                            osal_virt_to_phys(iv),
                            ((TE_KEY_TYPE_SEC == drv_ctx->key.type) ?
                              osal_virt_to_phys(drv_ctx->key.sec.eks) :
                              osal_virt_to_phys(drv_ctx->key.sec_v2.eks)),
                            osal_virt_to_phys(drv_ctx->key.user2.key),
                            iv_load, ctr_len);
            if ( TE_KEY_TYPE_SEC_V2 == drv_ctx->key.type ) {
                payload->buf[0] |= KLAD_256;
                osal_cache_clean(drv_ctx->key.sec_v2.eks, MAX_EKS_V2_SIZE);
            } else {
                osal_cache_clean(drv_ctx->key.sec.eks, MAX_EKS_SIZE);
            }
        }else{
            key_src = KEY_SRC_EXTERNAL;
            MAKE_CMD_INIT(payload->buf,
                            payload->size,
                            malg,
                            mode,
                            key_src,
                            TE_DRV_SCA_SELECT_KEY(drv_ctx->key.user.keybits),
                            TRIGGER_INT,
                            osal_virt_to_phys(iv),
                            osal_virt_to_phys(drv_ctx->key.user.key),
                            osal_virt_to_phys(drv_ctx->key.user2.key),
                            iv_load, ctr_len);
            osal_cache_clean(drv_ctx->key.user.key,
                             drv_ctx->key.user.keybits / 8);
        }
        if (NULL != drv_ctx->key.user2.key) {
            osal_cache_clean(drv_ctx->key.user2.key,
                                drv_ctx->key.user2.keybits / 8);
        }
        /* set iv & key non-secure attribute */
        ret = te_com_set_nsec_attr(osal_virt_to_phys(iv), ivlen,
                                   &ns_attr, SCA_INIT_IV_NS_SHIFT);
        if (ret != TE_SUCCESS) {
            goto __out__;
        }
        if (TE_KEY_TYPE_SEC == drv_ctx->key.type) {
            ret = te_com_set_nsec_attr(osal_virt_to_phys(drv_ctx->key.sec.eks),
                                       sizeof(drv_ctx->key.sec.eks),
                                       &ns_attr, SCA_INIT_KEY_NS_SHIFT);
        } else if (TE_KEY_TYPE_SEC_V2 == drv_ctx->key.type) {
            ret = te_com_set_nsec_attr(osal_virt_to_phys(drv_ctx->key.sec_v2.eks),
                                       sizeof(drv_ctx->key.sec_v2.eks),
                                       &ns_attr, SCA_INIT_KEY_NS_SHIFT);
        } else {
            ret = te_com_set_nsec_attr(osal_virt_to_phys(drv_ctx->key.user.key),
                                       drv_ctx->key.user.keybits >> 3,
                                       &ns_attr, SCA_INIT_KEY_NS_SHIFT);
        }
        if (ret != TE_SUCCESS) {
            goto __out__;
        }

        payload->buf[0] |= ns_attr;
        break;
    }
__out__:
    return ret;
}

int te_sca_start( te_crypt_ctx_t *ctx,
                  te_sca_operation_t op,
                  uint8_t *iv,
                  uint32_t ivlen )
{
    int ret = TE_SUCCESS;
    int sub_ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;
    uint32_t buf[SCA_INIT_CMD_SIZE] = {0};
    sca_payload_t payload = {0};
    te_sca_drv_t *drv = NULL;
    te_sess_slot_cat_t cat = {0};

    __SCA_DRV_VERIFY_PARAMS__(ctx);
    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );
    payload.buf = buf;
    ret = _te_sca_prepare_start(ctx, op, iv, ivlen, &payload, &cat);
    __SCA_DRV_CHECK_CONDITION__(ret);
    /* open te session */
    drv = (te_sca_drv_t *)ctx->drv;
    TE_ASSERT(NULL != drv);
    drv_ctx->sess = te_sess_open( drv->sctx, cat );
    if ( drv_ctx->sess < 0 ) {
        ret = drv_ctx->sess;
        drv_ctx->sess = INVALID_SESS_ID;
        _SCA_DRV_OUT_;
    }
    ret = te_sess_submit(drv_ctx->sess, payload.buf, payload.size);
    if (TE_SUCCESS != ret) {
        sub_ret = _te_sca_clear(drv_ctx);
        TE_ASSERT(TE_SUCCESS == sub_ret);
        sub_ret = te_sess_close(drv_ctx->sess);
        TE_ASSERT(TE_SUCCESS == sub_ret);
        drv_ctx->sess = INVALID_SESS_ID;
        _SCA_DRV_OUT_;
    }
    drv_ctx->op = op;
    drv_ctx->state = TE_DRV_SCA_STATE_START;
__out__:
    return ret;
}

static inline size_t _te_sca_get_total_len_phy_node_count(te_memlist_t *list,
                                                          size_t *phy_count)
{
    size_t _i = 0;
    uint64_t total_len = 0;
    uint64_t _len = 0;

    *phy_count = 0;
    if (NULL == list || list->ents == NULL) {
        return 0;
    }
    for (_i = 0; _i < list->nent; _i++){
        total_len += list->ents[_i].len;
        *phy_count += 1;
        _len = list->ents[_i].len;
        while (_len > SCA_DRV_LINKLIST_NODE_MAX_LEN) {
            _len -= SCA_DRV_LINKLIST_NODE_MAX_LEN;
            *phy_count += 1;
        }
    }
    return total_len;
}

static inline int _te_sca_visual_list_to_phy_list(link_list_t *phylist,
                                                    te_memlist_t *vlist)
{
    size_t i = 0;
    size_t j = 0;
    size_t n = 0;
    int ret = TE_SUCCESS;
    uint64_t _len = 0;

    for (i = 0, j = 0; i < vlist->nent; i++) {
        _len = vlist->ents[i].len;
        phylist[j].addr = osal_virt_to_phys(vlist->ents[i].buf);
        n = 0;
        while (_len > SCA_DRV_LINKLIST_NODE_MAX_LEN) {
            n++;
            phylist[j++].sz_attr.sz = SCA_DRV_LINKLIST_NODE_MAX_LEN - 1;
            phylist[j].addr = osal_virt_to_phys((uint8_t*)vlist->ents[i].buf \
                                    + (SCA_DRV_LINKLIST_NODE_MAX_LEN * n));
            _len -= SCA_DRV_LINKLIST_NODE_MAX_LEN;
        }
        phylist[j++].sz_attr.sz = _len - 1;
    }

    phylist[j].addr = 0x0ULL;
    phylist[j].val = 0xffffffffffffffffULL;
    /* set the non-secure bit for each node of the list */
    ret = te_com_set_llst_nsec_attr(phylist);
    return ret;
}

static void _te_sca_clean_memlist(te_memlist_t *list)
{
    size_t i = 0;
    if (NULL == list || NULL == list->ents) {
        return;
    }

    for (i = 0; i < list->nent; i++) {
        if (NULL != list->ents[i].buf) {
            osal_cache_clean(list->ents[i].buf, list->ents[i].len);
        }
    }
}

static void _te_sca_flush_memlist(te_memlist_t *list)
{
    size_t i = 0;
    if (NULL == list || NULL == list->ents) {
        return;
    }

    for (i = 0; i < list->nent; i++) {
        if (NULL != list->ents[i].buf) {
            osal_cache_flush(list->ents[i].buf, list->ents[i].len);
        }
    }
}

static void _te_sca_invalid_memlist(te_memlist_t *list)
{
    size_t i = 0;
    if (NULL == list || NULL == list->ents) {
        return;
    }
    for (i = 0; i < list->nent; i++) {
        if (NULL != list->ents[i].buf) {
            osal_cache_invalidate(list->ents[i].buf, list->ents[i].len);
        }
    }
}

static int _te_sca_prepare_update( te_crypt_ctx_t *ctx,
                                    bool islast,
                                    link_list_t **ll_in,
                                    link_list_t **ll_out,
                                    te_memlist_t *in,
                                    te_memlist_t *out,
                                    sca_payload_t *payload )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;
    uint8_t bypass = 0x00U;
    link_list_t *__in = NULL;
    link_list_t *__out = NULL;
    size_t in_phy_count = 0;
    size_t out_phy_count = 0;
    size_t in_len = 0;
    uint32_t ns_attr = 0U;

    in_len = _te_sca_get_total_len_phy_node_count(in, &in_phy_count);
    if ((!islast && (in_len % ctx->blk_size))) {
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        _SCA_DRV_OUT_;
    }

    /* hw requirement: total len of in === out's*/
    if (TE_OPERATION_MAC != TE_ALG_GET_CLASS(ctx->alg)){
        if (in_len != _te_sca_get_total_len_phy_node_count(out,
                            &out_phy_count)) {
            ret = TE_ERROR_BAD_INPUT_LENGTH;
            _SCA_DRV_OUT_;
        }
    }

    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );
    switch (drv_ctx->state) {
        default:
        case TE_DRV_SCA_STATE_RAW:
        case TE_DRV_SCA_STATE_INIT:
        case TE_DRV_SCA_STATE_READY:
        case TE_DRV_SCA_STATE_LAST:
            ret = TE_ERROR_BAD_STATE;
            _SCA_DRV_OUT_;
            break;
        case TE_DRV_SCA_STATE_START:
        case TE_DRV_SCA_STATE_UPDATE:
            __in = (link_list_t *)osal_malloc_aligned((in_phy_count + 1)
                                                 * sizeof(link_list_t),
                                                 ALIGNED_SIZE);
            if (NULL == __in) {
                ret = TE_ERROR_OOM;
                _SCA_DRV_OUT_;
            }
            /*convert vitual address to phy */
            _te_sca_clean_memlist(in);
            osal_memset(__in, 0x00, (in_phy_count + 1) * sizeof(link_list_t));
            ret = _te_sca_visual_list_to_phy_list(__in, in);
            if (TE_SUCCESS != ret) {
                goto cleanup;
            }
            osal_cache_clean((uint8_t *)__in,
                                 (in_phy_count + 1) * sizeof(link_list_t));

            if (NULL != out && NULL != out->ents) {
                __out = (link_list_t *)osal_malloc_aligned((out_phy_count + 1)\
                                                   *  sizeof(link_list_t),
                                                   ALIGNED_SIZE );
                if (NULL == __out) {
                    ret = TE_ERROR_OOM;
                    goto cleanup;
                }
                osal_memset(__out, 0x00, (out_phy_count + 1) * sizeof(link_list_t));
                /*convert vitual address to phy */
                ret = _te_sca_visual_list_to_phy_list(__out, out);
                if (TE_SUCCESS != ret) {
                    goto cleanup1;
                }
                osal_cache_clean((uint8_t *)__out,
                                    (out_phy_count + 1) * sizeof(link_list_t));
                _te_sca_flush_memlist(out);
            }

            /*make process command*/
            MAKE_CMD_PROC(payload->buf,
                          payload->size,
                          bypass,
                          (TE_DRV_SCA_ENCRYPT == drv_ctx->op) ? 0x00 : 0x01,
                          islast ? 0x01 : 0x00,
                          ADDR_TYPE_LINK_LIST,
                          ADDR_TYPE_LINK_LIST,
                          TRIGGER_INT,
                          osal_virt_to_phys(__in),
                          0,
                          osal_virt_to_phys(__out));
            /* set the non-secure attibute for src & dst */
            ret = te_com_set_nsec_attr(osal_virt_to_phys(__in),
                                       (in_phy_count + 1) * sizeof(link_list_t),
                                       &ns_attr, SCA_PROC_SRC_NS_SHIFT);
            if (ret != TE_SUCCESS) {
                goto cleanup1;
            }
            ret = te_com_set_nsec_attr(osal_virt_to_phys(__out),
                                       (out_phy_count + 1) * sizeof(link_list_t),
                                       &ns_attr, SCA_PROC_DST_NS_SHIFT);
            if (ret != TE_SUCCESS) {
                goto cleanup1;
            }
            payload->buf[0] |= ns_attr;
            break;
    }
    *ll_in = __in;
    *ll_out = __out;
    _SCA_DRV_OUT_;

cleanup1:
    if (NULL != __out){
        osal_free(__out);
        __out = NULL;
    }
cleanup:
    if (NULL != __in){
        osal_free(__in);
        __in = NULL;
    }
__out__:
    return ret;
}

int te_sca_update( te_crypt_ctx_t *ctx,
                   bool islast,
                   size_t len,
                   const uint8_t *in,
                   uint8_t *out )
{
#define MAX_NODE_SIZE       (3U)
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;
    uint32_t buf[SCA_PROC_CMD_SIZE] = {0};
    link_list_t *ll_in = NULL;
    link_list_t *ll_out = NULL;
    te_memlist_t _in = {0};
    te_mement_t in_ent = {0};
    te_memlist_t _out = {0};
    te_mement_t out_ents[MAX_NODE_SIZE] = {0};
    sca_payload_t payload = {0};
    size_t cp_len = 0;
    size_t _len = len;
    uint8_t *out_ptr = out;
    uint8_t *head = NULL;
    uint8_t *tail = NULL;

    __SCA_DRV_VERIFY_PARAMS__(ctx);
    if (len > 0) {
        __SCA_DRV_VERIFY_PARAMS__(in);
    }
    if (TE_OPERATION_MAC != TE_ALG_GET_CLASS(ctx->alg)) {
        if (0 < _len) {
            __SCA_DRV_VERIFY_PARAMS__(out);
        }
    }
    if ( 0 == _len ) {
        _SCA_DRV_OUT_;
    }
    /** Only when last block, should non block aligned be allowed */
    if (!islast && (_len % ctx->blk_size)) {
        ret = TE_ERROR_BAD_INPUT_LENGTH;
        _SCA_DRV_OUT_;
    }

    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );

    _in.nent = 1;
    _in.ents = &in_ent;
    in_ent.buf = (uint8_t *)in;
    in_ent.len = _len;
    if (NULL != out_ptr) {
        _out.nent = 0;
        _out.ents = out_ents;
        /**< settle head alignment */
        if ((uintptr_t)out_ptr & (TE_DMA_ALIGNED - 1)) {
            head = (uint8_t *)osal_malloc_aligned(
                                        TE_DMA_ALIGNED, TE_DMA_ALIGNED);
            if (!head) {
                ret = TE_ERROR_OOM;
                _SCA_DRV_OUT_;
            }
            out_ents[_out.nent].buf = head;
            cp_len = UTILS_ROUND_UP((uintptr_t)out_ptr, TE_DMA_ALIGNED) -
                                                            (uintptr_t)out_ptr;
            if (cp_len > _len) {
                cp_len = _len;
            }
            out_ents[_out.nent].len = cp_len;
            _out.nent++;
            _len -= cp_len;
            out_ptr += cp_len;
        }
        /**< middle complete aligned blocks */
        if (_len && (_len / TE_DMA_ALIGNED)) {
            cp_len = UTILS_ROUND_DOWN(_len, TE_DMA_ALIGNED);
            out_ents[_out.nent].buf = out_ptr;
            out_ents[_out.nent].len = cp_len;
            _out.nent++;
            _len -= cp_len;
        }
        /**< settle tail alignment */
        if (_len) {
            tail = (uint8_t *)osal_malloc_aligned(
                                        TE_DMA_ALIGNED, TE_DMA_ALIGNED);
            if (!tail) {
                ret = TE_ERROR_OOM;
                _SCA_DRV_OUT_;
            }
            out_ents[_out.nent].buf = tail;
            out_ents[_out.nent].len = _len;
            _out.nent++;
        }
    }

    payload.buf = buf;
    ret = _te_sca_prepare_update( ctx,
                                  islast,
                                  &ll_in,
                                  &ll_out,
                                  &_in,
                                  &_out,
                                  &payload);
    __SCA_DRV_CHECK_CONDITION__(ret);
    ret = te_sess_submit( drv_ctx->sess, payload.buf, payload.size );
    if (NULL != ll_in) {
        osal_free(ll_in);
    }
    if (NULL != ll_out) {
        osal_free(ll_out);
    }
    if (TE_SUCCESS != ret) {
        sca_error_cleanup(ctx);
        goto __out__;
    }
    if (islast) {
        drv_ctx->state = TE_DRV_SCA_STATE_LAST;
    }else{
        drv_ctx->state = TE_DRV_SCA_STATE_UPDATE;
    }

    _te_sca_invalid_memlist(&_out);
    /**< copy back to out */
    out_ptr = out;
    _len = len;
    if (out_ptr) {
        /**< head not aligned cases*/
        if ((uintptr_t)out_ptr & (TE_DMA_ALIGNED - 1)) {
            osal_memcpy(out_ptr, (uint8_t *)_out.ents[0].buf, _out.ents[0].len);
            _len -= _out.ents[0].len;
            /**< tail might modified, should copy back too, and 2 scenarios might occur
             * case#1 no complete blocks in the middle, that means out.nent == 2
             * case#2 complete blocks in the middle, that means out.nent == 3
            */
            /**< tail */
            if (_len <= TE_DMA_ALIGNED) {
                osal_memcpy( out_ptr + _out.ents[0].len,
                             (uint8_t *)_out.ents[1].buf, _out.ents[1].len );
            } else {
                osal_memcpy( out_ptr + _out.ents[0].len + _out.ents[1].len,
                             (uint8_t *)_out.ents[2].buf, _out.ents[2].len );
            }
        } else {
            if ( _len > TE_DMA_ALIGNED ) {
                osal_memcpy( out_ptr + _out.ents[0].len,
                             (uint8_t *)_out.ents[1].buf, _out.ents[1].len );
            } else if (_len < TE_DMA_ALIGNED) {
                osal_memcpy( out_ptr,
                             (uint8_t *)_out.ents[0].buf, _out.ents[0].len );
            }
        }
    }
__out__:

    OSAL_SAFE_FREE(head);
    OSAL_SAFE_FREE(tail);
    return ret;
}

typedef struct align_settle_info {
    void *head_addr;
    void *tail_addr;
} align_settle_info_t;

/**
 *  case#1 src's nent = 1, len <= DMA_CACHELINE_SIZE(64)
 *         dst case#1 src.ents[0].buf DAM aligned, let dst.ents[0].buf = src.ents[0].buf
 *                    so does it's length.
 *         dst case#2 src.ents[0].buf DAM not aligned let dst.ents[0].buf = new malloc buf,
 *                    dst.ents[0].len = round_up(src.ents[0].buf) - src.ents[0].buf.
 *                    dst.ents[1].buf = src.ents[0].buf + dst.ents[0].len
 *                    dst.ents[1].len = src.ents[0].len - dst.ents[0].len
 *  case#2 src's nent =1, len > DMA_CACHELINE_SIZE(64)
 *        dst case#1 src.ents[0].buf DAM not aligned let dst.ents[0].buf = new malloc buf,
 *                    dst.ents[0].len = round_up(src.ents[0].buf) - src.ents[0].buf.
 *                    dst.ents[1].buf = src.ents[0].buf + dst.ents[0].len
 *                    dst.ents[1].len = src.ents[0].len - dst.ents[0].len
 *        dst case#2 src.ents[0].buf DAM aligned
 *                    dst.ents[0].buf = src.ents[0].buf.
 *                    dst.ents[0].len = round_down(src.ents[0].len).
 *                    dst.ents[1].buf = src.ents[0].buf + dst.ents[0].len
 *                    dst.ents[1].len = src.ents[0].len - dst.ents[0].len
 *  case#3 src's nent > 1
 *         dst case#1 both src.ents[0].buf and src.ents[last].len DAM aligned,
 *                    let dst.ents = src.ents
 *         dst case#2 src.ents[0].buf aligned src.ents[last].len DAM not aligned,
 *                    dst.nent = src.nent + 1.
 *                    dst.ents[src.nent - 1].buf = src.ents[0].buf
 *                    dst.ents[src.nent - 1].len = src.ents[src.nent - 1].len - src.ents[src.nent - 1].len % DMA_ALIGNED_SIZE
 *                    dst.ents[src.nent].buf = src.ents[src.nent - 1].buf + dst.ents[src.nent - 1].len
 *                    dst.ents[src.nent].len = src.ents[src.nent - 1].len - dst.ents[src.nent - 1].len
 *         dst case#3 src.ents[0].buf not aligned src.ents[last].len DAM aligned,
 *                    dst.nent = src.nent + 1.
 *                    dst.ents[0].buf = new malloc dma aligned buf.
 *                    dst.ents[0].len = round_up(src.ents[0].buf) - src.ents[0].buf
 *                    dst.ents[1].buf = src.ents[0].buf + dst.ents[0].len
 *                    dst.ents[1].len = src.ents[0].len - dst.ents[0].len
 *         dst case#4 src.ents[0].buf not aligned src.ents[last].len DAM not aligned,
 *                    dst.nent = src.nent + 2.
 *                    dst.ents[0].buf = new malloc dma aligned buf.
 *                    dst.ents[0].len = round_up(src.ents[0].buf) - src.ents[0].buf
 *                    dst.ents[1].buf = src.ents[0].buf + dst.ents[0].len
 *                    dst.ents[1].len = src.ents[0].len - dst.ents[0].len
 *                    dst.ents[src.nent - 1].buf = src.ents[0].buf
 *                    dst.ents[src.nent - 1].len = src.ents[src.nent - 1].len - src.ents[src.nent - 1].len % DMA_ALIGNED_SIZE
 *                    dst.ents[src.nent].buf = src.ents[src.nent - 1].buf + dst.ents[src.nent - 1].len
 *                    dst.ents[src.nent].len = src.ents[src.nent - 1].len - dst.ents[src.nent - 1].len
 */
static int te_settle_cacheline_alignment( te_memlist_t *dst,
                                          const te_memlist_t *src,
                                          align_settle_info_t *info )
{
    uintptr_t head = 0;
    size_t cp_len = 0;
    int ret = TE_SUCCESS;

    if (!src || !src->nent ) {
        return TE_SUCCESS;
    }
    if (src && src->nent && !src->ents) {
        return TE_ERROR_BAD_PARAMS;
    }
    dst->ents = (te_mement_t *)osal_calloc(src->nent + 2, sizeof(*dst->ents));
    if (!dst->ents) {
        ret = TE_ERROR_OOM;
        goto out;
    }
    dst->nent = 0;
    /**< settle head */
    head = (uintptr_t)src->ents[0].buf;
    if (head & (TE_DMA_ALIGNED - 1)) {
        /**< get distance to ROUND_UP(cache_line_size) */
        cp_len = UTILS_ROUND_UP(head, TE_DMA_ALIGNED) - head;
        if (cp_len > src->ents[0].len) {
            cp_len = src->ents[0].len;
        }
        info->head_addr = (uint8_t *)osal_malloc_aligned( TE_DMA_ALIGNED,
                                                          TE_DMA_ALIGNED );
        if (!info->head_addr) {
            OSAL_LOG_ERR("%s +%d malloc aligned for head failed\n", __FILE__, __LINE__);
            ret = TE_ERROR_OOM;
            goto err_head;
        }
        dst->ents[0].buf = info->head_addr;
        dst->ents[0].len = cp_len;
        dst->nent++;
        /**< check if should insert new node to head or just replace it */
        if (cp_len == src->ents[0].len) {
            if ( 1U == src->nent ) {
                goto out;
            }
            osal_memcpy( &dst->ents[dst->nent], &src->ents[1],
                        (src->nent - 1) * sizeof(*dst->ents) );
            dst->nent += (src->nent - 1);
        } else {
            /** copy left of the list and update node#1 */
            osal_memcpy( &dst->ents[dst->nent], &src->ents[0],
                        src->nent * sizeof(*dst->ents) );
            dst->ents[dst->nent].buf = (uint8_t *)dst->ents[dst->nent].buf +
                                                                dst->ents[0].len;
            dst->ents[dst->nent].len = src->ents[0].len - dst->ents[0].len;
            dst->nent += src->nent;
        }
    } else {
        osal_memcpy( &dst->ents[dst->nent], src->ents,
                        src->nent * sizeof(*dst->ents) );
        dst->nent += src->nent;
    }
    /**< settle tail, check last node's alignment. */
    if (dst->nent > 0 && (dst->ents[dst->nent - 1].len & (TE_DMA_ALIGNED - 1))) {
        /**< append one node to the tail */
        info->tail_addr = osal_malloc_aligned( TE_DMA_ALIGNED,
                                               TE_DMA_ALIGNED );
        if (!info->tail_addr) {
            ret = TE_ERROR_OOM;
            goto err_tail;
        }
        /** two cases:
         *  case#1:
         *        len < cache line size, in this case just replace last node.
         *  case#2:
         *        len > cache line size, in this case split in to two nodes.
         *        should revise original last node's len, and append one node
         *        to the tail.
         */
        cp_len = dst->ents[dst->nent - 1].len & (TE_DMA_ALIGNED - 1);
        if (cp_len < dst->ents[dst->nent - 1].len) {
            dst->ents[dst->nent].buf = info->tail_addr;
            dst->ents[dst->nent].len = cp_len;
            /**< update original node length */
            dst->ents[dst->nent - 1].len -= cp_len;
            dst->nent++;
        } else {
            dst->ents[dst->nent - 1].buf = info->tail_addr;
        }
    }
    goto out;
err_tail:
    OSAL_SAFE_FREE(info->head_addr);
err_head:
    OSAL_SAFE_FREE(dst->ents);
    osal_memset(dst, 0x00, sizeof(*dst));
out:
    return ret;
}

int te_sca_uplist( te_crypt_ctx_t *ctx,
                   bool islast,
                   te_memlist_t *in,
                   te_memlist_t *out)
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;
    uint32_t buf[SCA_PROC_CMD_SIZE] = {0};
    te_memlist_t _out = {0};    /**< out list after allignment settled */
    link_list_t *ll_in = NULL;
    link_list_t *ll_out = NULL;
    sca_payload_t payload = {0};
    align_settle_info_t info = {0};
    __SCA_DRV_VERIFY_PARAMS__(ctx);
    __SCA_DRV_VERIFY_PARAMS__(in);
    if (TE_OPERATION_MAC != TE_ALG_GET_CLASS(ctx->alg)) {
        __SCA_DRV_VERIFY_PARAMS__(out);
    }
    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );
    /**< settle cache line alignment */
    ret = te_settle_cacheline_alignment(&_out, (const te_memlist_t *)out,
                                        &info);
    __SCA_DRV_CHECK_CONDITION__(ret);
    payload.buf = buf;
    ret = _te_sca_prepare_update( ctx,
                                  islast,
                                  &ll_in,
                                  &ll_out,
                                  in,
                                  &_out,
                                  &payload);
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("%s +%d_te_sca_prepare_update failed:%X\n",
                                        __func__, __LINE__, ret);
        goto err_pepare_update;
    }
    ret = te_sess_submit( drv_ctx->sess, payload.buf, payload.size );
    if( TE_SUCCESS != ret) {
        sca_error_cleanup(ctx);
        goto err_sess_submit;
    }

    _te_sca_invalid_memlist(&_out);
    if (info.head_addr) {
        osal_memcpy(out->ents[0].buf, _out.ents[0].buf, _out.ents[0].len);
    }
    /**< if tail node changed then copy the output to origin's */
    if (info.tail_addr) {
        osal_memcpy( ((uint8_t *)out->ents[out->nent - 1].buf +
                                 out->ents[out->nent - 1].len -
                                 _out.ents[_out.nent - 1].len),
                     _out.ents[_out.nent-1].buf,
                     _out.ents[_out.nent-1].len );
    }
    if (islast) {
        drv_ctx->state = TE_DRV_SCA_STATE_LAST;
    }else{
        drv_ctx->state = TE_DRV_SCA_STATE_UPDATE;
    }

err_sess_submit:
    OSAL_SAFE_FREE(ll_in);
    OSAL_SAFE_FREE(ll_out);
    OSAL_SAFE_FREE(_out.ents);
err_pepare_update:
    OSAL_SAFE_FREE(info.head_addr);
    OSAL_SAFE_FREE(info.tail_addr);
__out__:
    return ret;
}

static int _te_sca_prepare_finish( te_crypt_ctx_t *ctx,
                                    uint8_t *tag,
                                    uint32_t taglen,
                                    sca_payload_t *payload )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;
    uint32_t ns_attr = 0U;

    if ( ctx->blk_size < taglen ) {
        ret = TE_ERROR_BAD_PARAMS;
        _SCA_DRV_OUT_;
    }

    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );
    switch (drv_ctx->state) {
        default:
        case TE_DRV_SCA_STATE_RAW:
        case TE_DRV_SCA_STATE_INIT:
        case TE_DRV_SCA_STATE_READY:
            ret = TE_ERROR_BAD_STATE;
            break;
        case TE_DRV_SCA_STATE_START:
            break;
        case TE_DRV_SCA_STATE_UPDATE:
        case TE_DRV_SCA_STATE_LAST:
            if((TE_DRV_SCA_STATE_UPDATE == drv_ctx->state)
                && (TE_CHAIN_MODE_CMAC == TE_ALG_GET_CHAIN_MODE(ctx->alg))){
                ret = TE_ERROR_BAD_STATE;
                _SCA_DRV_OUT_;
            }
            if (NULL != tag) {
                osal_cache_clean(tag, taglen);
            }
            MAKE_CMD_FINISH(payload->buf,
                            payload->size,
                            osal_virt_to_phys(tag),
                            taglen,
                            TRIGGER_INT);
            /* set non-secure attribute for MAC */
            ret = te_com_set_nsec_attr(osal_virt_to_phys(tag), taglen,
                                       &ns_attr, SCA_FINISH_MAC_NS_SHIFT);
            if (ret != TE_SUCCESS) {
                goto __out__;
            }
            payload->buf[0] |= ns_attr;
            break;
    }

__out__:
    return ret;
}

int te_sca_finish( te_crypt_ctx_t *ctx,
                   uint8_t *tag,
                   uint32_t taglen )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;
    uint32_t buf[SCA_FINISH_CMD_SIZE] = {0};
    uint8_t *_tag = NULL;
    size_t _tag_len = 0;
    sca_payload_t payload = {0};

    __SCA_DRV_VERIFY_PARAMS__(ctx);
    _tag_len = UTILS_ROUND_UP(ctx->blk_size, TE_DMA_ALIGNED);
    if (TE_OPERATION_MAC == TE_ALG_GET_CLASS(ctx->alg)
        || TE_OPERATION_AE == TE_ALG_GET_CLASS(ctx->alg)) {
        _tag = (uint8_t *)osal_malloc_aligned(_tag_len, TE_DMA_ALIGNED);
        if (NULL == _tag) {
            ret = TE_ERROR_OOM;
            _SCA_DRV_OUT_;
        }
        osal_memset(_tag, 0x00, _tag_len);
    }
    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );

    payload.buf = buf;
    ret = _te_sca_prepare_finish(ctx, _tag, taglen, &payload);
    if (TE_SUCCESS != ret) {
        goto err;
    }

    if ((TE_DRV_SCA_STATE_START == drv_ctx->state) || (0 == taglen)) {
        ret = _te_sca_clear(drv_ctx);
        TE_ASSERT(TE_SUCCESS == ret);
    } else {
        ret = te_sess_submit( drv_ctx->sess,
                                (const uint32_t *)payload.buf,
                                payload.size );
        if (TE_SUCCESS !=  ret) {
            sca_error_cleanup(ctx);
            goto err;
        }
        if (TE_OPERATION_MAC == TE_ALG_GET_CLASS(ctx->alg)
            || TE_OPERATION_AE == TE_ALG_GET_CLASS(ctx->alg)) {
            osal_cache_invalidate(_tag, _tag_len);
            if(NULL != tag) {
                osal_memcpy(tag, _tag, taglen);
            }
        }
    }

    /* Close session */
    ret = te_sess_close( drv_ctx->sess );
    TE_ASSERT_MSG( TE_SUCCESS == ret,
                     "SCA close session error %x\n", ret);

    drv_ctx->sess = INVALID_SESS_ID;
    drv_ctx->state = TE_DRV_SCA_STATE_READY;

err:
    if(NULL !=  _tag) {
        osal_free(_tag);
        _tag = NULL;
    }
__out__:
    return ret;
}

int te_sca_clone( const te_crypt_ctx_t *src,
                  te_crypt_ctx_t *dst )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *sctx = (sca_drv_ctx_t *)src;
    sca_drv_ctx_t *dctx = (sca_drv_ctx_t *)dst;

    if ( src == NULL || dst == NULL ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (sctx->magic == SCA_CTX_MAGIC),
                     "BUG: Not valid sca driver context\n" );
    TE_ASSERT_MSG( (dctx->magic == SCA_CTX_MAGIC),
                     "BUG: Not valid sca driver context\n" );

    if ( dctx->state != TE_DRV_SCA_STATE_INIT ) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * clone driver context
     */
    dctx->base.alg = sctx->base.alg;
    dctx->base.blk_size = sctx->base.blk_size;
    dctx->op       = sctx->op;
    dctx->state    = sctx->state;
    dctx->key.type = sctx->key.type;
    if ( TE_KEY_TYPE_USER == sctx->key.type ) {
        dctx->key.user.keybits = sctx->key.user.keybits;
        dctx->key.user.key = osal_calloc(1, dctx->key.user.keybits / BYTE_BITS);
        if (NULL == dctx->key.user.key) {
            return TE_ERROR_OOM;
        }
        osal_memcpy(dctx->key.user.key, sctx->key.user.key,
                    dctx->key.user.keybits / BYTE_BITS);
    } else if ( TE_KEY_TYPE_SEC == sctx->key.type ) {
        osal_memcpy(&dctx->key.sec, &sctx->key.sec, sizeof(te_sec_key_t));
    } else {
        osal_memcpy(&dctx->key.sec_v2, &sctx->key.sec_v2, sizeof(te_sec_key_v2_t));
    }
    if (sctx->key.user2.key != NULL) {
        dctx->key.user2.keybits = sctx->key.user2.keybits;
        dctx->key.user2.key = osal_calloc(1, dctx->key.user2.keybits / BYTE_BITS);
        if (NULL == dctx->key.user2.key) {
            ret = TE_ERROR_OOM;
            goto err;
        }
        osal_memcpy(dctx->key.user2.key, sctx->key.user2.key,
                    dctx->key.user2.keybits / BYTE_BITS);

    }

    /*
     * CLONE failed, can not cause our session into error state,
     * Do not need any cleanup operation.
     *
     * In GCM/CCM mainflow case, the sctx->sess might be set to INVALID_SESS_ID.
     * We just skip te_sess_clone() if the sctx->sess is invalid.
     */
    if (sctx->sess != INVALID_SESS_ID) {
        dctx->sess = te_sess_clone( sctx->sess );
        if ( dctx->sess == INVALID_SESS_ID ) {
            ret = TE_ERROR_GENERIC;
            goto err;
        }
    } else {
        OSAL_LOG_TRACE("skip te_sess_clone for invalid src session\n");
    }

    return TE_SUCCESS;

err:
    if ( TE_KEY_TYPE_USER == dctx->key.type ) {
        osal_free(dctx->key.user.key);
        dctx->key.user.key = NULL;
    }
    osal_free(dctx->key.user2.key);
    return ret;
}

int te_sca_export( te_crypt_ctx_t *ctx,
                   void *out,
                   uint32_t *olen )
{
    int ret = TE_ERROR_GENERIC;
    sca_drv_ctx_t *sctx = (sca_drv_ctx_t *)ctx;
    sca_ehdr_t eh = {0};

    if ( NULL == ctx || NULL == olen ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (sctx->magic == SCA_CTX_MAGIC),
                     "BUG: Not valid sca driver context\n" );

    if ( sctx->state != TE_DRV_SCA_STATE_START &&
         sctx->state != TE_DRV_SCA_STATE_UPDATE &&
         sctx->state != TE_DRV_SCA_STATE_LAST ) {
        return TE_ERROR_BAD_STATE;
    }

    /*
     * poll for hwctx_sz
     */
    eh.hwctx_sz = 0;
    ret = te_sess_export( sctx->sess, NULL, &eh.hwctx_sz );
    if (ret != (int)TE_ERROR_SHORT_BUFFER) {
        return ret;
    }

    /*
     * be fancy to the caller
     */
    if (*olen < SCA_EHDR_SIZE(&eh)) {
        *olen = SCA_EHDR_SIZE(&eh);
        return TE_ERROR_SHORT_BUFFER;
    }

    if (NULL == out) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * export hwctx
     * TODO: lock the sca driver to stop service of update() or uplist() on
     * the calling context until te_sess_export() ends.
     * Or, it's the caller responsibility to ensure there be no update() or
     * uplist() call on to the same context when an export() is outstanding.
     */
    ret = te_sess_export( sctx->sess, SCA_EHDR_HWCTX(out), &eh.hwctx_sz );
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR( "te_sess_export error %x\n", ret );
        goto err;
    }

    /*
     * make export hdr
     */
    eh.magic  = SCA_EHDR_MAGIC;
    eh.alg    = sctx->base.alg;
    eh.op     = sctx->op;
    eh.state  = sctx->state;

    osal_memcpy(out, &eh, sizeof(eh));
    *olen = SCA_EHDR_SIZE(&eh);
err:
    return ret;
}

int te_sca_import( te_crypt_ctx_t *ctx,
                   const void *in,
                   uint32_t ilen )
{
    int ret = TE_ERROR_GENERIC;
    te_sess_id_t sess = INVALID_SESS_ID;
    sca_drv_ctx_t *sctx = (sca_drv_ctx_t *)ctx;
    sca_ehdr_t eh = {0};

    if ( NULL == ctx || NULL == in ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (sctx->magic == SCA_CTX_MAGIC),
                     "BUG: Not valid sca driver context\n" );

    /*
     * The 'in' might not start at struct ptr safe boundary.
     * Be safe to copy the struct before reading it.
     */
    osal_memcpy(&eh, in, sizeof(eh));

    if ( eh.magic != SCA_EHDR_MAGIC ||
         eh.alg != sctx->base.alg ||
         ilen < SCA_EHDR_SIZE(&eh) ) {
        OSAL_LOG_ERR("Bad or mismatched sca ehdr: %d\n", ilen);
        return TE_ERROR_BAD_PARAMS;
    }

    if ( sctx->state != TE_DRV_SCA_STATE_INIT &&
         sctx->state != TE_DRV_SCA_STATE_READY &&
         sctx->state != TE_DRV_SCA_STATE_START &&
         sctx->state != TE_DRV_SCA_STATE_UPDATE &&
         sctx->state != TE_DRV_SCA_STATE_LAST ) {
        return TE_ERROR_BAD_STATE;
    }

    if ( TE_DRV_SCA_STATE_INIT == sctx->state ) {
        /*
         * Open session if ctx is still in INIT state.
         */
        te_sess_slot_cat_t cat = TE_SLOT_CATEGORY_SHORT;
        te_sca_drv_t *drv = (te_sca_drv_t*)ctx->drv;

        if (TE_ALG_GET_CHAIN_MODE(eh.alg) == TE_CHAIN_MODE_CCM ||
            TE_ALG_GET_CHAIN_MODE(eh.alg) == TE_CHAIN_MODE_GCM) {
            cat = TE_SLOT_CATEGORY_LONG;
        }

        sess = te_sess_open(drv->sctx, cat);
        if (sess < 0) {
            return sess;
        }
        sctx->sess = sess;
    }

    /*
     * import hwctx
     */
    ret = te_sess_import( sctx->sess, SCA_EHDR_HWCTX(in), eh.hwctx_sz );
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR("te_sess_import error %x\n", ret);
        goto err_import;
    }

    /*
     * import sca drv ctx
     */
    sctx->op    = eh.op;
    sctx->state = eh.state;

    return TE_SUCCESS;

err_import:
    if (sess != INVALID_SESS_ID) {
        /*
         * wipe all on errors
         */
        if ( _te_sca_clear(sctx) != TE_SUCCESS ) {
            OSAL_LOG_ERR( "Can't CLEAR session(%d)\n", sctx->sess );
        }
        if ( te_sess_close( sctx->sess ) != TE_SUCCESS ) {
            OSAL_LOG_ERR( "Can't close session(%d)\n", sctx->sess );
        }
        sctx->sess = INVALID_SESS_ID;
    }
    return ret;
}

#ifdef CFG_TE_ASYNC_EN
static osal_err_t hworker_thread_entry( void *arg )
{
    unsigned long flags = 0;
    sca_worker_t *worker = (sca_worker_t *)arg;
    sca_async_ctx_t *task = NULL;
    sqlist_t *list = NULL;

    while (1) {

        osal_spin_lock_irqsave( &worker->lock, &flags );
        worker->state = HWORKER_ST_RUNNING;

        /* should we quit */
        if (worker->command == HWORKER_CMD_QUIT) {
            worker->state = HWORKER_ST_STOPPED;
            osal_spin_unlock_irqrestore( &worker->lock, flags );
            break;
        }

        /* Do we have task to handle ? */
        list = sqlist_dequeue( &worker->tasks );

        osal_spin_unlock_irqrestore( &worker->lock, flags );

        if ( list == NULL ) {
            osal_spin_lock_irqsave( &worker->lock, &flags );
            worker->state = HWORKER_ST_SLEEPING;
            osal_spin_unlock_irqrestore( &worker->lock, flags );
            OSAL_COMPLETION_COND_WAIT( (!sqlist_is_empty(&worker->tasks) ||
                                       worker->command != HWORKER_CMD_NONE),
                                       &worker->bell );
            continue;
        }

        task = SQLIST_CONTAINER( list, task, list );
        task->done( task );
        task = NULL;
    }

    return OSAL_SUCCESS;
}

static sca_worker_t *sca_worker_init(void)
{
    int ret = TE_ERROR_GENERIC;
    sca_worker_t *worker = NULL;

    worker = (sca_worker_t *)osal_calloc( 1, sizeof(sca_worker_t) );
    if ( worker == NULL ) {
        return NULL;
    }

    sqlist_init( &worker->tasks );
    worker->command = HWORKER_CMD_NONE;
    worker->state = HWORKER_ST_STOPPED;

    ret = osal_spin_lock_init( &worker->lock );
    if ( ret != OSAL_SUCCESS ) {
        goto err1;
    }

    ret = osal_completion_init( &worker->bell );
    if ( ret != OSAL_SUCCESS ) {
        goto err2;
    }

    ret = osal_thread_create( &worker->wthread, hworker_thread_entry, (void *)worker );
    if ( ret != OSAL_SUCCESS ) {
        goto err3;
    }

    return worker;

err3:
    osal_completion_destroy( &worker->bell );
err2:
    osal_spin_lock_destroy( &worker->lock );
err1:
    osal_free( worker );
    return NULL;
}

static void sca_worker_send_command( sca_worker_t *worker,
                                                uint32_t command )
{
    unsigned long flags = 0;

    osal_spin_lock_irqsave( &worker->lock, &flags );
    worker->command = command;
    osal_spin_unlock_irqrestore( &worker->lock, flags );

    osal_completion_signal( &worker->bell );
    return;
}

static void sca_worker_wait_stop( sca_worker_t *worker )
{
#define DURATION_10US       (10U)
    while (1) {
        osal_rmb();
        if ( worker->state == HWORKER_ST_STOPPED ) {
            break;
        }
        osal_delay_us(DURATION_10US);
    }
    return;
}

static void sca_worker_enqueue( sca_worker_t *worker, sca_async_ctx_t *task )
{
    unsigned long flags = 0;
    osal_spin_lock_irqsave( &worker->lock, &flags );
    sqlist_enqueue( &worker->tasks, &task->list );
    osal_spin_unlock_irqrestore( &worker->lock, flags );

    osal_completion_signal( &worker->bell );

}

static void sca_worker_destroy( sca_worker_t *worker )
{

    sca_worker_send_command( worker, HWORKER_CMD_QUIT );
    sca_worker_wait_stop( worker );
    osal_thread_destroy( worker->wthread );
    osal_completion_destroy( &worker->bell );
    osal_spin_lock_destroy( &worker->lock );
    osal_free( worker );
    return;
}

/*
 * This function need be called under thread context
 */
static void sca_astart_done( sca_async_ctx_t *actx )
{
    sca_drv_ctx_t *ctx = actx->ctx;
    te_sca_request_t *req = actx->req;
    te_sess_ar_t *ar = actx->ar;

    /* Free command frame */
    osal_free( (void *)ar->cmdptr );

    /* Free session async requset */
    osal_free( ar );

    /* update state machine */
    if ( req->res == TE_SUCCESS ) {
        ctx->state = TE_DRV_SCA_STATE_START;
    } else if ( req->res == (int)TE_ERROR_CANCEL ) {
        ;
    } else {
        /* ERROR case, cleanup state machine to TE_DRV_SCA_STATE_INIT */
        sca_error_cleanup( (te_crypt_ctx_t *)ctx );
    }

    /* Notify caller */
    req->base.completion( &req->base, req->res );

    /* Free tempoary sca_async_ctx_t */
    osal_free( actx );

    return;
}

/*
 * This function need be called under thread context
 */
static void sca_aupdate_done( sca_async_ctx_t *actx )
{
    sca_drv_ctx_t *ctx = actx->ctx;
    te_sca_request_t *req = actx->req;
    te_sess_ar_t *ar = actx->ar;

    /* Free command frame */
    osal_free( (void *)ar->cmdptr );

    /* Free session async requset */
    osal_free( ar );

    /* update state machine */
    if ( req->res == TE_SUCCESS ) {
        ctx->state = ( actx->up.last_update ?
                        TE_DRV_SCA_STATE_LAST :
                        TE_DRV_SCA_STATE_UPDATE );

    } else if ( req->res == (int)TE_ERROR_CANCEL ) {
        ;
    } else {
        /* ERROR case, cleanup state machine to TE_DRV_SCA_STATE_INIT */
        sca_error_cleanup( (te_crypt_ctx_t *)ctx );
    }

    TE_ASSERT( actx->up.in_ll != NULL );
    osal_free( actx->up.in_ll );
    actx->up.in_ll = NULL;
    if ( actx->up.out_ll != NULL ) {
        osal_free( actx->up.out_ll );
        actx->up.out_ll = NULL;
    }
    /* Free tempoary sca_async_ctx_t */
    osal_free( actx );

    /* Notify caller */
    req->base.completion( &req->base, req->res );

    return;
}

/*
 * This function need be called under thread context
 */
static void sca_afinish_done( sca_async_ctx_t *actx )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *ctx = actx->ctx;
    te_sca_request_t *req = actx->req;
    te_sess_ar_t *ar = actx->ar;

    /* Free command frame */
    osal_free( (void *)ar->cmdptr );

    /* Free session async requset */
    osal_free( ar );

    /* update state machine */
    if ( req->res == TE_SUCCESS ) {
        ret = te_sess_close(ctx->sess);
        TE_ASSERT( ret == TE_SUCCESS );
        ctx->sess = INVALID_SESS_ID;
        if (req->fin.tag && req->fin.taglen) {
            osal_cache_invalidate(actx->fin.tag, req->fin.taglen);
            osal_memcpy(req->fin.tag, actx->fin.tag, req->fin.taglen);
            osal_free(actx->fin.tag);
            actx->fin.tag = NULL;
        }
        ctx->state = TE_DRV_SCA_STATE_READY;
    } else if ( req->res == (int)TE_ERROR_CANCEL ) {
        ;
    } else {
        /* ERROR case, cleanup state machine to TE_DRV_SCA_STATE_READY */
        sca_error_cleanup( (te_crypt_ctx_t *)ctx );
    }

    /* Free tempoary sca_async_ctx_t */
    osal_free( actx );

    /* Notify caller */
    req->base.completion( &req->base, req->res );

    return;
}

static void sca_asubmit_done( te_sess_ar_t *ar )
{
    sca_async_ctx_t *actx = (sca_async_ctx_t *)ar->para;
    sca_drv_ctx_t *ctx = actx->ctx;
    te_sca_request_t *req = actx->req;
    te_sca_drv_t *hdrv = (te_sca_drv_t *)ctx->base.drv;
    sca_worker_t *worker = hdrv->worker;

    /* Update request result */
    req->res = ar->err;

    /* Enqueue task to work thread */
    sca_worker_enqueue( worker, actx );

    return;
}

int te_sca_astart( te_crypt_ctx_t *ctx,
                   te_sca_request_t *req )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;
    te_sca_drv_t *drv = NULL;
    uint32_t *buf = NULL;
    sca_payload_t payload = {0};
    te_sess_ar_t *ar = NULL;
    sca_async_ctx_t *actx = NULL;
    te_sess_slot_cat_t cat = {0};

    __SCA_DRV_VERIFY_PARAMS__(ctx);
    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );
    buf = (uint32_t *)osal_calloc(SCA_INIT_CMD_SIZE, sizeof(uint32_t));
    if (NULL == buf) {
        ret = TE_ERROR_OOM;
        _SCA_DRV_OUT_;
    }
    payload.buf = buf;
    ret = _te_sca_prepare_start(ctx,
                                req->st.op,
                                req->st.iv,
                                req->st.ivlen,
                                &payload,
                                &cat);
    actx = (sca_async_ctx_t *)osal_calloc(1, sizeof(sca_async_ctx_t));
    if ( actx == NULL ) {
        ret = TE_ERROR_OOM;
        goto cleanup1;
    }
    ar = osal_calloc( 1, sizeof(te_sess_ar_t) );
    if ( ar == NULL ) {
        ret = TE_ERROR_OOM;
        goto cleanup2;
    }

    ar->cmdptr = payload.buf;
    ar->len = payload.size;
    ar->para = (void *)actx;
    ar->cb = sca_asubmit_done;

    actx->ctx = (sca_drv_ctx_t *)ctx;
    actx->req = req;
    actx->ar = ar;
    actx->done = sca_astart_done;
    /* open te session */
    drv = (te_sca_drv_t *)ctx->drv;
    TE_ASSERT(NULL != drv);
    drv_ctx->sess = te_sess_open( drv->sctx, cat );
    if ( drv_ctx->sess < 0 ) {
        ret = drv_ctx->sess;
        drv_ctx->sess = INVALID_SESS_ID;
        goto cleanup3;
    }

    ret = te_sess_asubmit(drv_ctx->sess, ar);
    if (TE_SUCCESS != ret) {
        sca_error_cleanup(ctx);
        goto cleanup3;
    }
    _SCA_DRV_OUT_;
 cleanup3:
    osal_free(ar);
 cleanup2:
    osal_free(actx);
 cleanup1:
    osal_free(buf);
__out__:
    return ret;
}

int te_sca_aupdate( te_crypt_ctx_t *ctx,
                    te_sca_request_t *req )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;
    link_list_t *ll_in = NULL;
    link_list_t *ll_out = NULL;
    te_memlist_t *in = NULL;
    te_memlist_t *out = NULL;
    te_sess_ar_t *ar = NULL;
    sca_async_ctx_t *actx = NULL;
    te_mement_t in_ent = {0};
    te_mement_t out_ent = {0};
    te_memlist_t in_ml = {0};
    te_memlist_t out_ml = {0};
    sca_payload_t payload = {0};

    __SCA_DRV_VERIFY_PARAMS__(ctx);
    __SCA_DRV_VERIFY_PARAMS__(req);

    if (req->up.flags & SCA_FLAGS_LIST) {
        in = &req->up.lst.src;
        out = &req->up.lst.dst;
    } else {
        /* for stream buf case , build as a list to unify processing flow */
        in = &in_ml;
        in->nent = 1;
        in->ents = &in_ent;
        in_ent.buf = (void *)req->up.data.src;
        in_ent.len = req->up.data.len;

        out = &out_ml;
        out->nent = 1;
        out->ents = &out_ent;
        out_ent.buf = req->up.data.dst;
        out_ent.len = req->up.data.len;
    }

    payload.buf = (uint32_t *)osal_calloc(SCA_PROC_CMD_SIZE,
                                          sizeof(uint32_t));
    if (NULL == payload.buf) {
        ret = TE_ERROR_OOM;
        _SCA_DRV_OUT_;
    }

    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );
    ret = _te_sca_prepare_update( ctx,
                            ((req->up.flags & SCA_FLAGS_LAST) ? true : false),
                                  &ll_in,
                                  &ll_out,
                                  in,
                                  out,
                                  &payload);
    if (TE_SUCCESS != ret) {
        goto cleanup1;
    }
    actx = osal_calloc(1, sizeof(*actx));
    if (NULL == actx) {
        ret = TE_ERROR_OOM;
        goto cleanup2;
    }

    ar = osal_calloc( 1, sizeof(te_sess_ar_t) );
    TE_ASSERT( ar != NULL );

    ar->cmdptr = payload.buf;
    ar->len = payload.size;
    ar->para = (void *)actx;
    ar->cb = sca_asubmit_done;
    actx->ctx = (sca_drv_ctx_t *)ctx;
    actx->req = req;
    actx->ar = ar;
    actx->done = sca_aupdate_done;
    actx->up.in_ll = ll_in;
    actx->up.out_ll = ll_out;
    actx->up.last_update = (req->up.flags & SCA_FLAGS_LAST) \
                            ? true : false;
    ret = te_sess_asubmit(drv_ctx->sess, ar);
    if(TE_SUCCESS != ret) {
        sca_error_cleanup(ctx);
        goto cleanup3;
    }
    _SCA_DRV_OUT_;
cleanup3:
    osal_free(actx);
    actx = NULL;
cleanup2:
    osal_free(ll_in);
    osal_free(ll_out);
cleanup1:
    osal_free(payload.buf);
__out__:
    return ret;
}

int te_sca_afinish( te_crypt_ctx_t *ctx,
                    te_sca_request_t *req )
{
    int ret = TE_SUCCESS;
    sca_drv_ctx_t *drv_ctx = NULL;
    uint32_t *buf = NULL;
    uint8_t *_tag = NULL;
    size_t _tag_len = UTILS_ROUND_UP(ctx->blk_size, TE_DMA_ALIGNED);
    te_sess_ar_t *ar = NULL;
    sca_async_ctx_t *actx = NULL;
    sca_payload_t payload = {0};

    __SCA_DRV_VERIFY_PARAMS__(ctx);
    __SCA_DRV_VERIFY_PARAMS__(req);
    if (TE_OPERATION_MAC == TE_ALG_GET_CLASS(ctx->alg)
        || TE_OPERATION_AE == TE_ALG_GET_CLASS(ctx->alg)) {
        __SCA_DRV_VERIFY_PARAMS__(req->fin.tag);
        _tag = (uint8_t *)osal_malloc_aligned(_tag_len, TE_DMA_ALIGNED);
        if (NULL == _tag) {
            ret = TE_ERROR_OOM;
            _SCA_DRV_OUT_;
        }
        osal_memset(_tag, 0x00, _tag_len);
    }
    buf = (uint32_t *)osal_calloc(SCA_FINISH_CMD_SIZE,
                                        sizeof(uint32_t));
    TE_ASSERT(buf != NULL);
    payload.buf = buf;
    drv_ctx = (sca_drv_ctx_t *)ctx;
    TE_ASSERT_MSG( drv_ctx->magic == SCA_CTX_MAGIC,
                "fatal error: Not valid SCA driver context\n" );

    ret = _te_sca_prepare_finish(ctx, _tag,
                                 ctx->blk_size, &payload);
    if (TE_SUCCESS != ret) {
        goto cleanup1;
    }
    if (TE_DRV_SCA_STATE_START == drv_ctx->state) {
        ret = _te_sca_clear(drv_ctx);
        TE_ASSERT(TE_SUCCESS == ret);
        drv_ctx->state = TE_DRV_SCA_STATE_READY;
    } else {
        actx = (sca_async_ctx_t *)osal_calloc(1,
                                                sizeof(sca_async_ctx_t));
        if (NULL == actx) {
            ret = TE_ERROR_OOM;
            goto cleanup1;
        }

        ar = osal_calloc( 1, sizeof(te_sess_ar_t) );
        TE_ASSERT( ar != NULL );
        ar->cmdptr = payload.buf;
        ar->len = payload.size;
        ar->para = (void *)actx;
        ar->cb = sca_asubmit_done;
        actx->ctx = (sca_drv_ctx_t *)ctx;
        actx->req = req;
        actx->ar = ar;
        actx->done = sca_afinish_done;
        actx->fin.tag = _tag;

        ret = te_sess_asubmit( drv_ctx->sess, ar );
        TE_ASSERT(TE_SUCCESS == ret);
        if (TE_SUCCESS !=  ret) {
            sca_error_cleanup(ctx);
            goto cleanup2;
        }
    }
    _SCA_DRV_OUT_;
cleanup2:
    osal_free(actx);
cleanup1:
    if (NULL != _tag) {
        osal_free(_tag);
    }
    osal_free(buf);
__out__:
    return ret;
}

#endif /* CFG_TE_ASYNC_EN */

