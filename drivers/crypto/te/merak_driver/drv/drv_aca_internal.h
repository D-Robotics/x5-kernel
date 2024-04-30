//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DRV_ACA_INTERNAL_H__
#define __TRUSTENGINE_DRV_ACA_INTERNAL_H__

#include <te_common.h>
#include <hwa/te_hwa.h>
#include <hwa/te_hwa_aca.h>
#include <linux/stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Top control of ACA driver debug. */
#define ACA_DEBUG 1

/**
 * The following macros are sub controls of ACA driver debug. They are efficient
 * only when ACA_DEBUG is 1
 **/

/* Enable ACA SRAM debug, such as alloc/free/swap. */
#define ACA_SRAM_DEBUG 0
/* Enable ACA GR debug, such as alloc/free */
#define ACA_GR_DEBUG 0
/* Enable ACA OP debug. */
#define ACA_OP_DEBUG 0
/**
 * Config the ACA OP debug level.
 * 0: No operation data (big number data) is ouput
 * 1: Will output operation data (big number data).
 **/
#define ACA_OP_DEBUG_LEVEL 0

/* Whether to zeroize SRAM when alloc. Currently MUST be 1 */
#define ACA_SRAM_ALLOC_ZERO 1

#if ACA_DEBUG
#define ACA_DBG_ERR(__fmt__, ...) OSAL_LOG_ERR(__fmt__, ##__VA_ARGS__)
#define ACA_DBG_LOG(__fmt__, ...) OSAL_LOG_DEBUG(__fmt__, ##__VA_ARGS__)
#if ACA_SRAM_DEBUG
#define SRAM_DBG_LOG(__fmt__, ...) OSAL_LOG_DEBUG(__fmt__, ##__VA_ARGS__)
#else
#define SRAM_DBG_LOG(__fmt__, ...)
#endif /* ACA_SRAM_DEBUG */
#if ACA_GR_DEBUG
#define GR_DBG_LOG(__fmt__, ...) OSAL_LOG_DEBUG(__fmt__, ##__VA_ARGS__)
#else
#define GR_DBG_LOG(__fmt__, ...)
#endif /* ACA_GR_DEBUG */
#else
#define ACA_DBG_ERR(__fmt__, ...)
#define ACA_DBG_LOG(__fmt__, ...)
#define SRAM_DBG_LOG(__fmt__, ...)
#define GR_DBG_LOG(__fmt__, ...)
#endif

/* useful macros to check parameters/function return */
#define CHECK_PARAM(__true_condition__)                                        \
    do {                                                                       \
        if (!(__true_condition__)) {                                           \
            ACA_DBG_ERR("Check Parameter " #__true_condition__                 \
                        " failed! func: %s line: %d\n",                        \
                        __func__,                                              \
                        __LINE__);                                             \
            return TE_ERROR_BAD_PARAMS;                                        \
        }                                                                      \
    } while (0)

#define CHECK_FUNC(__function_call__, ...)                                     \
    do {                                                                       \
        ret = __function_call__;                                               \
        if (0 != ret) {                                                        \
            ACA_DBG_ERR("Check Function " #__function_call__                   \
                        " failed! ret: 0x%x func: %s line: %d\n",              \
                        ret,                                                   \
                        __func__,                                              \
                        __LINE__);                                             \
            __VA_ARGS__                                                        \
            return ret;                                                        \
        }                                                                      \
    } while (0)

#define CHECK_COND_RETURN(__true_condition__, __ret_code__, ...)               \
    do {                                                                       \
        if (!(__true_condition__)) {                                           \
            ACA_DBG_ERR("Check Condition " #__true_condition__                 \
                        " failed! return: 0x%x func: %s line: %d\n",           \
                        __ret_code__,                                          \
                        __func__,                                              \
                        __LINE__);                                             \
            __VA_ARGS__                                                        \
            return (__ret_code__);                                             \
        }                                                                      \
    } while (0)

#define CHECK_RET_RETURN                                                       \
    do {                                                                       \
        if ((TE_SUCCESS) != (ret)) {                                           \
            ACA_DBG_ERR("Check Ret failed! ret:0x%x func: %s line: %d\n",      \
                        ret,                                                   \
                        __func__,                                              \
                        __LINE__);                                             \
            return ret;                                                        \
        }                                                                      \
    } while (0)

#define CHECK_RET_LOG_GO(__fmt__, ...)                                         \
    do {                                                                       \
        if ((TE_SUCCESS) != (ret)) {                                           \
            ACA_DBG_ERR("Check Ret failed! func: %s line: %d, ret=0x%x\n",     \
                        __func__,                                              \
                        __LINE__,                                              \
                        ret);                                                  \
            OSAL_LOG_ERR("[TE ACA Error] %s:%d. Ret is 0x%x \n",               \
                         __func__,                                             \
                         __LINE__,                                             \
                         ret);                                                 \
            OSAL_LOG_ERR(__fmt__, ##__VA_ARGS__);                              \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

#define CHECK_COND_LOG_GO(__true_condition__, __ret_code__, __fmt__, ...)      \
    do {                                                                       \
        if (!(__true_condition__)) {                                           \
            OSAL_LOG_ERR(                                                      \
                "[TE ACA Error] %s:%d. Condition is  " #__true_condition__     \
                "\n",                                                          \
                __func__,                                                      \
                __LINE__);                                                     \
            OSAL_LOG_ERR(__fmt__, ##__VA_ARGS__);                              \
            ret = (__ret_code__);                                              \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

#define CHECK_RET_GO                                                           \
    do {                                                                       \
        if ((TE_SUCCESS) != (ret)) {                                           \
            ACA_DBG_ERR("Check Ret failed! ret: 0x%x func: %s line: %d\n",     \
                        ret,                                                   \
                        __func__,                                              \
                        __LINE__);                                             \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

#define CHECK_COND_GO(__true_condition__, __ret_code__)                        \
    do {                                                                       \
        if (!(__true_condition__)) {                                           \
            ACA_DBG_ERR("Check Condition " #__true_condition__                 \
                        " failed! func: %s line: %d\n",                        \
                        __func__,                                              \
                        __LINE__);                                             \
            ret = (__ret_code__);                                              \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

/**
 * ACA context magic number
 */
#define ACA_CTX_MAGIC 0x43616341U /**< "AcaC" */

/**
 * Sram Block flags
 * BUSY: The BN's data is in TE SRAM and currently is used, can't be swapped
 * out.
 * PREEMPT: The BN's data is in TE SRAM, but currently not used, can be
 * swapped out.
 * SWAPPED: The BN's data is been swapped out in system heap.
 **/
typedef enum sram_flag {
    SRAM_FLAG_BUSY    = 1,
    SRAM_FLAG_PREEMPT = 2,
    SRAM_FLAG_SWAPPED = 3,
} sram_flag_t;

/* The sram block used in OP_CTX */
typedef struct sram_block {
    void *sram_addr;        /* The address in TE SRAM.
                               Can be NULL if data is in swapped area.
                               Read/write with this address MUST makes sure
                               this sram_block is in BUSY state.
                            */
    uint32_t *swapped_addr; /* The swapped area (system heap) address.
                               May be NULL if data is in TE SRAM.
                               Can be not NULL even data is in TE SRAM.
                               Read/write with this address doesn't have any
                               restirction
                            */
    size_t size;            /* The data size in byte. MUST be ACA engine
                               granularity size aligned.
                            */
    int32_t cached_bit_len; /* The cached bit length. Because there are may
                               usage scene of geting BN data's bit length, we
                               cache this size for performance.
                            */
    uint32_t flags;         /* The sram block's flag. */
    sqlist_t node;          /* Linked list node. This node can be linked to
                               SRAM Pool's busy_list, preempt_list or
                               swapped_list according to sram block's flag.
                             */
    void *pool;             /* One reversed pointer points to the SRAM Pool */
} sram_block_t;

/**
 * \brief The GR's usage for one OP_CTX.
 *
 * NULL: Current OP_CTX is not using ACA engine, and there is no GR allocated.
 * IN:   Current OP_CTX is input when using ACA engine.
 *       Such as A, B, C in HW EDS.
 * OUT:  Current OP_CTX is output. Such as R in HW EDS.
 * INOUT:Current OP_CTX is used as both input and output for several ACA's
 *       operations. In most cases, this OP_CTX is one temporary OP_CTX.
 * N:    Current OP_CTX is used as N in HW EDS.
 * P:    Current OP_CTX is used as P in HW EDS.
 * T0:   Current OP_CTX is used as T0 in HW EDS.
 * T1:   Current OP_CTX is used as T1 in HW EDS.
 */
typedef enum gr_usage_hint {
    GR_USAGE_NULL  = 0,
    GR_USAGE_IN    = 1,
    GR_USAGE_OUT   = 2,
    GR_USAGE_INOUT = 3,
    GR_USAGE_N     = 4,
    GR_USAGE_P     = 5,
    GR_USAGE_T0    = 6,
    GR_USAGE_T1    = 7,
} gr_usage_hint_t;

/* The GR info used in OP_CTX */
typedef struct _gr_info_t {
    int32_t gr_id;         /* The GR id.
                              -1:  This OP_CTX doesn't allocate any GR.
                              >=0: A valid GR id.(actually 0 is reserved).
                           */
    gr_usage_hint_t usage; /* The GR's usage */
    void *pool;            /* One reversed pointer points to the GR pool */
} gr_info_t;

/* The bignumber sign, MUST keep align with the defination in mbedtls */
enum {
    BN_SIGN_POSITIVE = 1,
    BN_SIGN_NEGATIVE = -1,
};

/**
 * ACA operation context structure
 */
typedef struct aca_op_ctx {
    sram_block_t *sram_block; /* sram block, allocated from sram_pool */
    gr_info_t *gr_info;       /* gr info, allocated from gr_pool */
    void *extra_np;           /* extra context points to aca_drv_extra_np_t */
} aca_op_ctx_t;

/**
 * ACA operation extra context structure, used to save NP of N which is used
 * in MODE_XXX operations.
 */
typedef struct aca_drv_extra_np {
    aca_op_ctx_t op_P_ctx; /* NP context. Valid only when op_N_ctx == OP_CTX*/
    aca_op_ctx_t op_N_ctx; /* Saved N context, used to check whether the OP_CTX
                              is changed */
} aca_drv_extra_np_t;

/* ACA driver context, inherit from crypto context. */
typedef struct aca_drv_ctx {
    te_crypt_ctx_t base;         /**< Base context */
    uint32_t magic;              /**< ACA ctx magic */
    int sign;                    /**< Integer sign */
    const te_aca_drv_t *aca_drv; /**< ACA driver which this drv ctx is binded */
    aca_op_ctx_t op_ctx;         /**< The OP_CTX */
    void *extra_ctx[2];          /**< Extra contexts array, used by
                                      wrapper layer */
} aca_drv_ctx_t;

/**
 * \brief ACA SRAM Pool
 */
typedef struct aca_sram_pool {
    uint32_t magic;    /* Magic */
    size_t alignment;  /* Alignment, size in bytes of ACA core granularity */
    size_t max_size;   /* Maximum sram block size allowed */
    void *sram_base;   /* SRAM base. */
    size_t sram_size;  /* SRAM size */
    size_t freed_size; /* Recored the totoal freed TE SRAM size. */
    sqlist_t used_blocks; /* List to record allocated SRAM from TE SRAM */

#if ACA_DEBUG
    size_t total_swapped_count; /* Totoal Swapped count */
    size_t total_swapped_size;  /* Total Swapped size */
#endif

    sqlist_t busy_list;    /* List of sram_block who's flag is BUSY */
    sqlist_t preempt_list; /* List of sram_block who's flag is PREEMPT */
    sqlist_t swapped_list; /* List of sram_info who's flag is SWAPPED */
    osal_mutex_t lock;        /* SRAM pool lock */

    void *hwa_ctx; /* A pointer points to the HWA */
} aca_sram_pool_t;

/**
 * \brief GR Pool structure
 */
typedef struct aca_gr_pool {
    uint32_t magic;    /* magic */
    size_t alignment;  /* Alignment, size in bytes of ACA core granularity */
    int32_t gr_number; /* Total GR number */
    uint8_t *gr_array; /* GR array */

    osal_mutex_t lock; /* GR pool lock */

    void *hwa_ctx; /* A pointer points to the HWA */
} aca_gr_pool_t;

/**
 * \brief LengthType Pool structure
 */
typedef struct aca_len_type_pool {
    uint32_t magic;          /* magic */
    int32_t len_type_number; /* Total length type number */
    uint8_t *len_type_array; /* Length type array */

    osal_mutex_t lock; /* LengthType pool lock */

    void *hwa_ctx; /* A pointer points to the HWA */
} aca_len_type_pool_t;

/* operation status */
typedef struct aca_op_status {
    volatile bool is_valid; /* Whether current operation status is valid */
    volatile bool done;     /* internal used */

    /* the following are status from ACA engine */
    volatile bool div_zero;
    volatile bool modinv_zero;
    volatile bool mult_red_err;
    volatile bool red_byd_th_evt;
    volatile bool mod_n_zero_err;
    volatile bool internal_err;

    volatile int32_t reduction_times;
    volatile bool alu_carry;
    volatile bool xor_result_zero;
    volatile bool and_result_zero;
    volatile bool add_result_zero;
} aca_op_status_t;

/**
 * \brief ACA opertaion manager structure
 */
typedef struct aca_operation {
    uint32_t magic;    /* magic */
    osal_mutex_t lock; /* ACA OP lock */
#ifdef CFG_TE_IRQ_EN
    te_irq_nb_handle nb_hanlde; /* NB handle used when interrupt enabled */

    osal_completion_t completion;
#endif

    aca_op_status_t op_status; /* The operation status */

#if ACA_DEBUG
    te_aca_op_entry_t dbg_last_op_entry; /* last OP code */
#endif

    void *hwa_ctx; /* A pointer points to the HWA */
} aca_operation_t;

/**
 * TE ACA async requests header
 * Must keep aligned with te_xxx_request_t.
 * The internal four pointer of te_xxx_request_t are:
 * 1. node.
 * 2. aca_pk_cb.
 * 3. hdl.
 */
typedef struct _aca_async_req_header_t {
    te_async_request_t base;
    sqlist_t node;
    int (*aca_pk_cb)(void *args);
    const te_drv_handle hdl;
} aca_async_req_header_t;

typedef void *pk_request_internal_data_t[4];

/**
 * \brief ACA public key operation structure.
 */
typedef struct aca_pk {
    osal_mutex_t lock; /* ACA PK lock, used by wrapper */
#ifdef CFG_TE_ASYNC_EN
    osal_mutex_t async_lock;         /* Async mode lock of async_list */
    sqlist_t async_list;          /* List to record async requests */
    osal_atomic_t async_thread_flag; /* Async thread's flag */
    osal_thread_t async_thread;      /* Async thread */

    osal_completion_t new_req; /* Signal of new request */
#endif                         /* CFG_TE_ASYNC_EN */
} aca_pk_t;

/**
 * \brief  The ACA private driver context.
 */
typedef struct aca_priv_drv {
    aca_sram_pool_t sram_pool;         /* SRAM pool */
    aca_gr_pool_t gr_pool;             /* GR pool */
    aca_len_type_pool_t len_type_pool; /* LengthType pool */
    aca_operation_t op;                /* ACA operation manager */
    aca_pk_t pk;                       /* ACA public key operation structure */
} aca_priv_drv_t;

/* BN related macros */
#define BN_GET_OP_CTX(__bn_ptr__) (&(((aca_drv_ctx_t *)(__bn_ptr__))->op_ctx))
#define BN_GET_DRV(__bn_ptr__) (((aca_drv_ctx_t *)(__bn_ptr__))->aca_drv)

#define BN_GET_SRAM_BLOCK(__bn_ptr__)                                          \
    ((((aca_drv_ctx_t *)(__bn_ptr__))->op_ctx).sram_block)

#define BN_OP_CTX_IS_VALID(__bn_ptr__) (BN_GET_OP_CTX(__bn_ptr__)->sram_block)

#define BN_CHECK(__bn_ptr__)                                                   \
    do {                                                                       \
        if (!(__bn_ptr__)) {                                                   \
            ACA_DBG_ERR("Check BN failed: NULL ptr! func: %s line: %d\n",      \
                        __func__,                                              \
                        __LINE__);                                             \
            return TE_ERROR_BAD_PARAMS;                                        \
        }                                                                      \
        if ((((aca_drv_ctx_t *)(__bn_ptr__))->magic) != ACA_CTX_MAGIC) {       \
            ACA_DBG_ERR("Check BN failed: Bad magic:0x%x func: %s line: %d\n", \
                        (((aca_drv_ctx_t *)(__bn_ptr__))->magic),              \
                        __func__,                                              \
                        __LINE__);                                             \
            return TE_ERROR_BAD_PARAMS;                                        \
        }                                                                      \
    } while (0)

#define BN_CHECK_CONST_DRV(__bn_ptr1__, __bn_ptr2__)                           \
    do {                                                                       \
        if (((aca_drv_ctx_t *)(__bn_ptr1__))->aca_drv !=                       \
            ((aca_drv_ctx_t *)(__bn_ptr2__))->aca_drv) {                       \
            ACA_DBG_ERR(                                                       \
                "Check BNs failed: Not const driver! func: %s line: %d\n",     \
                __func__,                                                      \
                __LINE__);                                                     \
            return TE_ERROR_INVAL_CTX;                                         \
        }                                                                      \
    } while (0)

#define BN_CHECK_HAVE_DATA(__bn_ptr__)                                         \
    do {                                                                       \
        BN_CHECK(__bn_ptr__);                                                  \
        if (!BN_GET_SRAM_BLOCK((aca_drv_ctx_t *)(__bn_ptr__))) {               \
            ACA_DBG_ERR("Check BN failed: No SRAM Block! func: %s line: %d\n", \
                        __func__,                                              \
                        __LINE__);                                             \
            return TE_ERROR_BAD_PARAMS;                                        \
        }                                                                      \
    } while (0)

#define ACA_POOL_LOCK(__pool_ptr__)                                            \
    do {                                                                       \
        osal_mutex_lock(__pool_ptr__->lock);                                   \
    } while (0)
#define ACA_POOL_UNLOCK(__pool_ptr__)                                          \
    do {                                                                       \
        osal_mutex_unlock(__pool_ptr__->lock);                                 \
    } while (0)

#define ACA_OP_LOCK(__drv_ptr__)                                               \
    do {                                                                       \
        osal_mutex_lock(((aca_priv_drv_t *)(__drv_ptr__->priv_drv))->op.lock); \
    } while (0)

#define ACA_OP_UNLOCK(__drv_ptr__)                                             \
    do {                                                                       \
        osal_mutex_unlock(                                                     \
            ((aca_priv_drv_t *)(__drv_ptr__->priv_drv))->op.lock);             \
    } while (0)

#define ACA_GR_IS_VALID_USAGE(__usage__)                                       \
    (((__usage__) == GR_USAGE_IN) || ((__usage__) == GR_USAGE_OUT) ||          \
     ((__usage__) == GR_USAGE_INOUT) || ((__usage__) == GR_USAGE_N) ||         \
     ((__usage__) == GR_USAGE_P) || ((__usage__) == GR_USAGE_T0) ||            \
     ((__usage__) == GR_USAGE_T1))

#define ACA_SRAM_GET_POOL(__sram_block_ptr__)                                  \
    ((aca_sram_pool_t *)(__sram_block_ptr__->pool))

#define ACA_SRAM_GET_HWA(__sram_block_ptr__)                                   \
    ((te_hwa_aca_t *)(ACA_SRAM_GET_POOL(__sram_block_ptr__)->hwa_ctx))

#define ACA_SRAM_ASSERT_ON_SRAM_BUF(__sram_block_ptr__)                        \
    TE_ASSERT(__sram_block_ptr__->sram_addr && __sram_block_ptr__->size);

#define ACA_SRAM_ASSERT_ON_SWAPPED_BUF(__sram_block_ptr__)                     \
    TE_ASSERT(__sram_block_ptr__->swapped_addr && __sram_block_ptr__->size);

#define ACA_DRV_GET_OP(__aca_drv_ptr__)                                        \
    (&(((aca_priv_drv_t *)(__aca_drv_ptr__->priv_drv))->op))
#define ACA_DRV_GET_OP_STATUS(__aca_drv_ptr__)                                 \
    (&(((aca_priv_drv_t *)(__aca_drv_ptr__->priv_drv))->op.op_status))

#define ACA_DRV_GET_SRAM_POOL(__aca_drv_ptr__)                                 \
    (&(((aca_priv_drv_t *)(__aca_drv_ptr__->priv_drv))->sram_pool))

#define ACA_DRV_GET_GR_POOL(__aca_drv_ptr__)                                   \
    (&(((aca_priv_drv_t *)(__aca_drv_ptr__->priv_drv))->gr_pool))

#define ACA_DRV_GET_LEN_TYPE_POOL(__aca_drv_ptr__)                             \
    (&(((aca_priv_drv_t *)(__aca_drv_ptr__->priv_drv))->len_type_pool))

#define ACA_DRV_GET_HWA(__aca_drv_ptr__) ((te_hwa_aca_t *)(aca_drv->base.hwa))

extern int aca_drv_init_sram_pool(aca_sram_pool_t *sram_pool,
                                  const te_hwa_aca_t *aca_hwa);
extern void aca_drv_cleanup_sram_pool(aca_sram_pool_t *sram_pool);
extern int aca_sram_alloc_block(aca_sram_pool_t *sram_pool,
                                size_t size,
                                sram_block_t **ret_sram_block);
extern int aca_sram_alloc_and_get_block(aca_sram_pool_t *sram_pool,
                                        size_t size,
                                        sram_block_t **ret_sram_block,
                                        void **sram_addr,
                                        size_t *sram_size);
extern void aca_sram_free_block(sram_block_t *block);
extern int aca_sram_write(sram_block_t *block,
                          const uint8_t *data,
                          size_t size);
extern int aca_sram_zeroize(sram_block_t *block);
extern int aca_sram_read(sram_block_t *block, uint8_t *buf, size_t size);
extern int aca_sram_get(sram_block_t *block,
                        void **sram_addr,
                        size_t *sram_size);
extern int aca_sram_put(sram_block_t *block);
extern int aca_sram_swap_out(sram_block_t *block);
extern int aca_sram_swap_all_blocks(aca_sram_pool_t *sram_pool);
extern int aca_sram_swap_all_blocks_nolock(aca_sram_pool_t *sram_pool);
extern int aca_sram_get_bit_len(sram_block_t *block, size_t *bit_len);
/* get the supported max operation len, equals to sram size */
extern int aca_sram_get_size(sram_block_t *block, size_t *size);

extern int aca_sram_set_bit(sram_block_t *block, size_t bit_num, int32_t val);
extern int aca_sram_get_bit(sram_block_t *block, size_t bit_num, int *bit_val);
extern int aca_sram_try_change_size(sram_block_t *block, size_t new_size);
extern int aca_sram_reset(sram_block_t *block, size_t new_size);

extern int aca_drv_init_gr_pool(aca_gr_pool_t *gr_pool,
                                const te_hwa_aca_t *aca_hwa);
extern void aca_drv_cleanup_gr_pool(aca_gr_pool_t *gr_pool);
extern int aca_gr_alloc(aca_gr_pool_t *gr_pool,
                        gr_usage_hint_t usage,
                        void *sram_addr,
                        size_t sram_size,
                        int32_t *gr_id_ret);
extern void aca_gr_free(aca_gr_pool_t *gr_pool, int32_t gr_id);
extern bool aca_gr_is_busy(aca_gr_pool_t *gr_pool);

extern int aca_drv_init_len_type_pool(aca_len_type_pool_t *len_type_pool,
                                      const te_hwa_aca_t *aca_hwa);
extern void aca_drv_cleanup_len_type_pool(aca_len_type_pool_t *len_type_pool);
extern int aca_len_type_alloc(aca_len_type_pool_t *len_type_pool, size_t len);
extern void aca_len_type_free(aca_len_type_pool_t *len_type_pool,
                              int32_t len_type_id);
extern bool aca_len_type_is_busy(aca_len_type_pool_t *len_type_pool);

extern int aca_op_init(aca_operation_t *op, const te_hwa_aca_t *aca_hwa);
extern void aca_op_cleanup(aca_operation_t *op);
extern int op_ctx_init(const te_aca_drv_t *aca_drv,
                       aca_op_ctx_t *op_ctx,
                       int32_t bytelen_hint);
extern void op_ctx_clean(aca_op_ctx_t *op_ctx);
extern int op_ctx_get(aca_op_ctx_t *op_ctx, gr_usage_hint_t bn_usage);
extern int op_ctx_put(aca_op_ctx_t *op_ctx);
extern int op_ctx_get_all(aca_op_ctx_t *op_ctx, int32_t bn_usage, ...);
extern void op_ctx_put_all(aca_op_ctx_t *op_ctx, ...);
extern int op_ctx_update_np(const te_aca_drv_t *aca_drv, aca_op_ctx_t *N);
extern void op_ctx_dump(const char *name, aca_op_ctx_t *op_ctx);

extern int aca_op_run(const te_aca_drv_t *aca_drv,
                      aca_op_ctx_t *R,
                      aca_op_ctx_t *A,
                      aca_op_ctx_t *B,
                      int32_t imme_B,
                      aca_op_ctx_t *C,
                      aca_op_ctx_t *N,
                      te_aca_op_code_t op_code,
                      aca_op_status_t *result_status);
extern int aca_op_shift(const te_aca_drv_t *aca_drv,
                        aca_op_ctx_t *R,
                        aca_op_ctx_t *A,
                        int32_t shift_value,
                        te_aca_op_code_t op_code);
extern int aca_op_copy(const te_aca_drv_t *aca_drv,
                       aca_op_ctx_t *dst,
                       const aca_op_ctx_t *src);
extern int aca_op_cmp(const te_aca_drv_t *aca_drv,
                      aca_op_ctx_t *op_a,
                      aca_op_ctx_t *op_b,
                      int *result);
extern int aca_op_cmp_immeb(const te_aca_drv_t *aca_drv,
                            aca_op_ctx_t *op_a,
                            int32_t imme_b,
                            bool *is_equal);
extern int aca_op_set_u32(const te_aca_drv_t *aca_drv,
                          aca_op_ctx_t *op_ctx,
                          uint32_t value);
extern int aca_op_prepare_t0_t1(const te_aca_drv_t *aca_drv,
                                aca_op_ctx_t *t0,
                                aca_op_ctx_t *t1,
                                size_t full_bit_len);
extern int aca_op_div_bn(const te_aca_drv_t *aca_drv,
                         aca_op_ctx_t *R,
                         aca_op_ctx_t *Q,
                         const aca_op_ctx_t *A,
                         const aca_op_ctx_t *B);
extern int aca_op_mod_bn(const te_aca_drv_t *aca_drv,
                         aca_op_ctx_t *op_r_ctx,
                         aca_op_ctx_t *op_a_ctx,
                         aca_op_ctx_t *op_b_ctx);
extern int aca_op_modinv(const te_aca_drv_t *aca_drv,
                         const aca_op_ctx_t *N,
                         const aca_op_ctx_t *A,
                         aca_op_ctx_t *R);
extern int aca_op_mod_exp(const te_aca_drv_t *aca_drv,
                          aca_op_ctx_t *R,
                          aca_op_ctx_t *A,
                          aca_op_ctx_t *E,
                          aca_op_ctx_t *N);
extern int aca_op_gcd(const te_aca_drv_t *aca_drv,
                      const aca_op_ctx_t *A,
                      const aca_op_ctx_t *B,
                      aca_op_ctx_t *G);
extern int aca_op_check_prime(const te_aca_drv_t *aca_drv,
                              const aca_op_ctx_t *X,
                              int32_t rounds,
                              int (*f_rng)(void *, uint8_t *, size_t),
                              void *p_rng);
extern int aca_ecp_op_convert_affine_to_jacobian(const te_aca_drv_t *aca_drv,
                                                 const aca_op_ctx_t *N,
                                                 const aca_op_ctx_t *X,
                                                 const aca_op_ctx_t *Y,
                                                 aca_op_ctx_t *jx,
                                                 aca_op_ctx_t *jy,
                                                 aca_op_ctx_t *jz);
extern int aca_ecp_op_convert_jacobian_to_affine(const te_aca_drv_t *aca_drv,
                                                 const aca_op_ctx_t *N,
                                                 const aca_op_ctx_t *jx,
                                                 const aca_op_ctx_t *jy,
                                                 const aca_op_ctx_t *jz,
                                                 aca_op_ctx_t *X,
                                                 aca_op_ctx_t *Y,
                                                 aca_op_ctx_t *Z);
extern int aca_op_ecp_mul(const te_aca_drv_t *aca_drv,
                          const aca_op_ctx_t *P,
                          const aca_op_ctx_t *A,
                          const aca_op_ctx_t *G_X,
                          const aca_op_ctx_t *G_Y,
                          const aca_op_ctx_t *G_Z,
                          const aca_op_ctx_t *k,
                          aca_op_ctx_t *R_X,
                          aca_op_ctx_t *R_Y,
                          aca_op_ctx_t *R_Z);
extern int aca_op_ecp_add(const te_aca_drv_t *aca_drv,
                          const aca_op_ctx_t *P,
                          const aca_op_ctx_t *G1_X,
                          const aca_op_ctx_t *G1_Y,
                          const aca_op_ctx_t *G1_Z,
                          const aca_op_ctx_t *G2_X,
                          const aca_op_ctx_t *G2_Y,
                          const aca_op_ctx_t *G2_Z,
                          aca_op_ctx_t *R_X,
                          aca_op_ctx_t *R_Y,
                          aca_op_ctx_t *R_Z);

extern int aca_pk_init(aca_pk_t *pk);
extern void aca_pk_cleanup(aca_pk_t *pk);

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_DRV_ACA_INTERNAL_H__ */
