//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DRV_H__
#define __TRUSTENGINE_DRV_H__

#include <te_defines.h>
#include <te_common.h>
#include <te_memlist.h>
#include <sqlist.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_hwa_host;
struct te_hwa_crypt;
struct te_sca_drv;
struct te_hash_drv;
struct te_aca_drv;
struct te_otp_drv;
struct te_trng_drv;
struct te_drv;

/**
 * Algorithem identifier type
 */
typedef uint32_t te_algo_t;

/**
 * Trust engine driver type enumeration
 */
typedef enum te_drv_type {
    TE_DRV_TYPE_NONE = 0,
    TE_DRV_TYPE_HASH,
    TE_DRV_TYPE_SCA,
    TE_DRV_TYPE_ACA,
    TE_DRV_TYPE_OTP,
    TE_DRV_TYPE_TRNG,
    TE_DRV_TYPE_CTL,
} te_drv_type_t;

/**
* Trust engine key type enumeration
*/
typedef enum te_key_type {
    TE_KEY_TYPE_NONE = 0,
    TE_KEY_TYPE_SEC,             /**< secure key (key ladder derived) */
    TE_KEY_TYPE_USER,            /**< user key */
    TE_KEY_TYPE_SEC_V2           /**< secure key (key ladder 256 derived) */
} te_key_type_t;

/**
 * Trust engine suspend state
 */
typedef enum te_suspend_state {
    TE_SUSPEND_ON = 0,          /**< working mode */
    TE_SUSPEND_STANDBY,         /**< clock gating */
    TE_SUSPEND_MEM,             /**< suspend to memory */
} te_suspend_state_t;

/**
* Trust engine key ladder root key selection enumeration
*/
typedef enum te_kl_key_sel {
    TE_KL_KEY_MODEL = 0,         /**< model key */
    TE_KL_KEY_ROOT               /**< device root key */
} te_kl_key_sel_t;

/**
 * secure key structure
 */
#define MAX_EK1_SIZE        (16U)
#define MAX_EK2_SIZE        (16U)
#define MAX_EK3_SIZE        (32U)
#define MAX_EKS_SIZE        (MAX_EK1_SIZE + MAX_EK2_SIZE + MAX_EK3_SIZE)

typedef struct te_sec_key {
    te_kl_key_sel_t sel;                    /**< key ladder root key selection */
    uint32_t ek3bits;                       /**< ek3 length in bits, 128 or 256 */
    union {
        struct {
            uint8_t ek1[MAX_EK1_SIZE];     /**< encrypted key1 (fixed to 128-bit) */
            uint8_t ek2[MAX_EK2_SIZE];     /**< encrypted key2 (fixed to 128-bit) */
            uint8_t ek3[MAX_EK3_SIZE];     /**< encrypted key3 */
        };
        uint8_t eks[MAX_EKS_SIZE];         /**< ek1 || ek2 || ek3 */
    };
} te_sec_key_t;

/**
 * secure key structure for key-kladder 256
 */
#define MAX_EK1_V2_SIZE        (32U)
#define MAX_EK2_V2_SIZE        (32U)
#define MAX_EK3_V2_SIZE        (32U)
#define MAX_EKS_V2_SIZE        (MAX_EK1_V2_SIZE + MAX_EK2_V2_SIZE + MAX_EK3_V2_SIZE)

typedef struct te_sec_key_v2 {
    te_kl_key_sel_t sel;                    /**< key ladder root key selection */
    uint32_t ek3bits;                       /**< must be 256 in bits */
    union {
        struct {
            uint8_t ek1[MAX_EK1_V2_SIZE];   /**< encrypted key1 (fixed to 256-bit) */
            uint8_t ek2[MAX_EK2_V2_SIZE];   /**< encrypted key2 (fixed to 256-bit) */
            uint8_t ek3[MAX_EK3_V2_SIZE];   /**< encrypted key3 (fixed to 256-bit) */
        };
        uint8_t eks[MAX_EKS_V2_SIZE];         /**< ek1 || ek2 || ek3 */
    };
} te_sec_key_v2_t;

/**
 * user key structure
 */
typedef struct te_user_key {
    uint8_t *key;                /**< key data */
    uint32_t keybits;            /**< key length in bits */
} te_user_key_t;

/**
 * key wrapper structure
 */
typedef struct te_key_wrap {
    te_key_type_t type;          /**< key type */
    union {
        te_sec_key_t sec;        /**< secure key */
        te_sec_key_v2_t sec_v2;  /**< secure key for key-ladder 256 */
        te_user_key_t user;      /**< user key */
    };
} te_key_wrap_t;

struct te_async_request;
/**
 * Completion callback for async requests
 */
typedef void (*te_completion_t)(struct te_async_request *req, int err);

/**
 * Trust engine async request structure
 */
typedef struct te_async_request {
    sqlist_t node;
    te_completion_t completion;
    void *data;                 /**< user data */
    uint32_t flags;             /**< n/a flags */
} te_async_request_t;

/**
 * Trust engine crypto driver structure
 */
typedef struct te_crypt_drv {
    char name[TE_MAX_DRV_NAME];       /**< driver name */
    uint32_t flags;                   /**< flags */
    osal_atomic_t refcnt;             /**< reference count */
    struct te_hwa_crypt *hwa;         /**< hwa crypt */

    int (*suspend)(struct te_crypt_drv* drv, te_suspend_state_t state);
    int (*resume)(struct te_crypt_drv* drv, te_suspend_state_t state);
    void (*destroy)(struct te_crypt_drv* drv);
} te_crypt_drv_t, *te_crypt_drv_ptr;

/**
 * Trust engine crypto context structure
 */
typedef struct te_crypt_ctx {
    te_algo_t alg;               /**< algorithm identifier */
    uint32_t blk_size;           /**< block size */
    uint32_t ctx_size;           /**< private context size */
    te_crypt_drv_t *drv;         /**< driver */
    void *__ctx;                 /**< private context ptr */
} te_crypt_ctx_t, *te_crypt_ctx_ptr;

/**
 * The type of opaque handles
 */
typedef struct __te_drv_handle *te_drv_handle;

typedef struct __te_ctx_handle *te_ctx_handle;

/**
 * te_aca_bn_t is alias of te_crypt_ctx_t
 */
typedef te_crypt_ctx_t te_aca_bn_t;


/**
 * \brief           This function gets the private ctx of a crypto context.
 * \param[in] ctx   The crypto context.
 * \return          The private context pointer.
 */
static inline void* crypt_priv_ctx(te_crypt_ctx_t *ctx)
{
    return ctx->__ctx;
}

/**
 * \brief           This function allocates and initializes a host driver
 *                  instance. The generated driver instance is bound to the
 *                  supplied host instance \p host.
 *
 *                  All underlying drivers will be initialized in this function.
 *
 *                  The driver handle will be set to the handle pointer \p h
 *                  on success.
 *
 * \param[in] host  The host HWA instance.
 * \param[out] h    The buffer holding the driver handle on success.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_drv_alloc( struct te_hwa_host *host, te_drv_handle *h );

/**
 * \brief           This function withdraws and destroies the driver instance
 *                  associated with the supplied handle \p h.
 *
 *                  All underlying drivers will be withdrawed in this function.
 *
 * \param[in] h     The host driver handle.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_drv_free( te_drv_handle h );

/**
 * \brief           This function searches the driver of the supplied \p type in
 *                  the host driver associated with the given handle \p h. And
 *                  obtains a reference to the driver.
 *
 *                  The reference count of the selected driver will be increased
 *                  by one on success.
 *
 * \param[in] h     The host driver handle.
 * \param[in] type  The driver type.
 * \return          The pointer of a crypto driver instance associated with the
 *                  referenced driver.
 * \return          \c NULL on failure.
 */
te_crypt_drv_t* te_drv_get( te_drv_handle h, te_drv_type_t type );

/**
 * \brief           This function drops a reference from the driver of the
 *                  supplied \p type in the host driver associated with the
 *                  given handle \p h.
 *
 *                  The reference count of the selected driver will be decreased
 *                  by one on success.
 *
 * \param[in] h     The host driver handle.
 * \param[in] type  The driver type.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_drv_put( te_drv_handle h, te_drv_type_t type );

/**
 * \brief           This function attempts to suspend all underlying drivers in
 *                  the host driver associated with the supplied handler \p h.
 *
 *                  The suspend process will be abandoned in case of any error
 *                  in suspending an underlying driver.
 *
 * \param[in] h     The host driver handle.
 * \param[in] state The TE engine's target suspend state.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_drv_suspend( te_drv_handle h, te_suspend_state_t state );

/**
 * \brief           This function resumes all underlying drivers that were
 *                  suspended early in the host driver associated with the
 *                  supplied handler \p h. It is the opposite side of the
 *                  suspend call.
 *
 *                  This function will not return until done resuming well.
 *
 * \param[in] h     The host driver handle.
 * \return          \c TE_SUCCESS always.
 */
int te_drv_resume( te_drv_handle h );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_DRV_H__ */
