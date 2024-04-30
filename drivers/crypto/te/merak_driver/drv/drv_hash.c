//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <driver/te_drv_hash.h>
#include <hwa/te_hwa.h>
#include "drv_internal.h"
#include "drv_sess.h"

/**
 * HASH context magic number
 */
#define HASH_CTX_MAGIC     0x78744344U /**< "DCtx" */

#define TRIGGER_INT             (0x1 << 0)

#define HASH_INIT_CMD_SZ        (5)
#define HASH_INIT_CMD           (0x80 << 24)
#define HASH_SHA1_ALG           (0x00 << 7)
#define HASH_SHA224_ALG         (0x01 << 7)
#define HASH_SHA256_ALG         (0x02 << 7)
#define HASH_SHA384_ALG         (0x03 << 7)
#define HASH_SHA512_ALG         (0x04 << 7)
#define HASH_MD5_ALG            (0x05 << 7)
#define HASH_SM3_ALG            (0x08 << 7)

#define HASH_HMAC_MODE          (0x1 << 4)
#define HASH_DIGEST_MODE        (0x0 << 4)

#define HASH_MODEL_KEY          (0x0 << 2)
#define HASH_ROOT_KEY           (0x1 << 2)
#define HASH_EXT_KEY            (0x2 << 2)
#define HASH_KLAD_256           (0x1 << 13)

#define HASH_128BIT_KEY         (0x0 << 1)
#define HASH_256BIT_KEY         (0x1 << 1)

/* link list PROC frame size */
#define HASH_PROC_LLST_SZ       (3)
/* normal PROC frame size */
#define HASH_PROC_NORMAL_SZ     (4)
#define HASH_PROC_CMD           (0x40 << 24)
#define HASH_PROC_SRC_NS_SHIFT  (6)
#define HASH_PROC_ZERO          (0x01 << 5)
#define HASH_PROC_LAST          (0x01 << 4)
#define HASH_PROC_PADDING       (0x01 << 3)
#define HASH_LINK_LIST_MODE     (0x01 << 1)

#define HASH_SET_CMD_FIELD( _ptr, _v )      ((_ptr)[0] |= (_v))

#define HASH_SET_IV_FIELD( _ptr, _v )       do {                        \
        (_ptr)[1] = (uint32_t)(((uint64_t)(_v)) & 0xffffffff);          \
        (_ptr)[2] = (uint32_t)((((uint64_t)(_v)) >> 32) & 0xffffffff);  \
} while (0)

#define HASH_SET_KEY_FIELD( _ptr, _v )       do {                       \
        (_ptr)[3] = (uint32_t)((_v) & 0xffffffff);                      \
        (_ptr)[4] = (uint32_t)(((_v) >> 32) & 0xffffffff);              \
} while (0)


#define HASH_SET_PROC_SRC_LLST( _ptr, _v )       do {                   \
        (_ptr)[1] = (uint32_t)((_v) & 0xffffffff);                      \
        (_ptr)[2] = (uint32_t)(((_v) >> 32) & 0xffffffff);              \
} while (0)

#define HASH_SET_PROC_SRC_LEN( _ptr, _v )       do {                    \
        (_ptr)[1] = (uint32_t)((_v) & 0xffffffff);                      \
} while (0)

#define HASH_SET_PROC_SRC_ADDR( _ptr, _v )       do {                   \
        (_ptr)[2] = (uint32_t)((_v) & 0xffffffff);                      \
        (_ptr)[3] = (uint32_t)(((_v) >> 32) & 0xffffffff);              \
} while (0)

#define HASH_MAC_LEN(_v)    ({                                          \
        ((((_v) - 1) & 0x7f) << 1);                                     \
})

#define HASH_FINISH_CMD_SZ      (7)
#define HASH_FINISH_CMD         (0x20<<24)
/*
 * Warning!! HASH_SET_FINISH_DEST must be used before
 * HASH_SET_KEY_FIELD and HASH_SET_IV_FIELD
 */
#define HASH_SET_FINISH_DEST( _ptr, _v )       do {                     \
        (_ptr)[1] = (uint32_t)((_v) & 0xffffffff);                      \
        (_ptr)[2] = (uint32_t)(((_v) >> 32) & 0xffffffff);              \
        (_ptr)[3] = (uint32_t)((_v) & 0xffffffff);                      \
        (_ptr)[4] = (uint32_t)(((_v) >> 32) & 0xffffffff);              \
        (_ptr)[5] = (uint32_t)((_v) & 0xffffffff);                      \
        (_ptr)[6] = (uint32_t)(((_v) >> 32) & 0xffffffff);              \
} while (0)

#define HASH_CLEAR_CMD_SZ      (1)
#define HASH_CLEAR_CMD         (0xff<<24)

#ifdef CFG_TE_ASYNC_EN
/* HWORKER COMMAND */
#define HWORKER_CMD_NONE    (0)
#define HWORKER_CMD_QUIT    (1)

/* HWORKER STATE */
#define HWORKER_ST_STOPPED  (0)
#define HWORKER_ST_RUNNING  (1)
#define HWORKER_ST_SLEEPING (2)
#endif

#define HASH_DRV_KLAD256        (0x1UL << 31)

typedef struct hash_prvdata {
    uint32_t caps;            /**< algorithms capacity bitmap, currently
                                   only use bit31 to mark the key-ladder 256 */
} hash_prvdata_t;

/**
 * HASH context structure
 */
typedef struct hash_drv_ctx {
    te_crypt_ctx_t base;               /**< base context */
    uint32_t magic;                    /**< hash ctx magic */
    uint8_t iv[TE_MAX_HASH_SIZE];      /**< hash initial vector */
    uint32_t hlen;                     /**< hash length in byte */
    uint8_t npdata[TE_MAX_HASH_BLOCK]; /**< not processed data */
    uint32_t npdlen;                   /**< data length in byte */
    te_hash_state_t state;             /**< hash state */
    te_sess_id_t sess;                 /**< session handler */
    te_hmac_key_t key;                 /**< HMAC key */
} hash_drv_ctx_t;

/**
 * HASH power management context
 */
typedef struct hash_pm_ctx {
    struct te_sca_ctl ctl;          /**< Saved ctl register in suspend */
    struct te_sca_int msk;          /**< Saved intr mask */
    struct te_sca_suspd_msk suspd;  /**< Saved suspend mask */
} hash_pm_ctx_t ;

#define HASH_EHDR_SIZE(x)   (sizeof(hash_ehdr_t) + (x)->hwctx_sz)
#define HASH_EHDR_HWCTX(x)  (uint8_t *)(((hash_ehdr_t *)(x)) + 1)

/**
 * HASH export state header magic number
 */
#define HASH_EHDR_MAGIC     0x52686548U /**< "HehR" */

/**
 * HASH export state header structure
 */
typedef struct hash_export_hdr {
    uint32_t magic;                    /**< magic */
    te_algo_t alg;                     /**< algorithm identifier */
    uint8_t npdata[TE_MAX_HASH_BLOCK]; /**< not processed data */
    uint32_t npdlen;                   /**< data length in byte */
    te_hash_state_t state;             /**< hash state */
    te_hmac_key_t key;                 /**< HMAC key */
    uint32_t hwctx_sz;                 /**< hwctx size in byte */
    /*
     * Commented out element used to visualize the layout dynamic part
     * of the struct.
     *
     * uint8_t hwctx[];
     */
} hash_ehdr_t;

#ifdef CFG_TE_ASYNC_EN
/**
 * HASH worker thread
 */
typedef struct hash_worker {
    osal_thread_t wthread;
    osal_spin_lock_t lock;
    osal_completion_t bell;
    volatile uint32_t command;
    volatile uint32_t state;
    sqlist_t tasks;
} hash_worker_t;

typedef struct hash_async_ctx {
    sqlist_t list;
    hash_drv_ctx_t *hctx;
    te_hash_request_t *req;
    te_sess_ar_t *ar;
    union {
        struct {
            uint8_t *npdata;
            uint32_t npdlen;
            link_list_t *link_list;
            uint32_t last_update;
        } up;

        struct {
            uint32_t hlen;
            uint32_t dummy1 __te_dma_aligned; /**< placeholder */
            uint8_t mac[TE_MAX_HASH_SIZE] __te_dma_aligned;
            uint32_t dummy2 __te_dma_aligned; /**< placeholder */
        } fin;

        struct {
            te_crypt_ctx_t *src;
            te_crypt_ctx_t *dst;
        } cl;
    };
    void(*done)(struct hash_async_ctx *ctx);
} hash_async_ctx_t;

static osal_err_t hworker_thread_entry( void *arg );
static hash_worker_t *hash_worker_init(void);
static void hash_worker_destroy( hash_worker_t *worker );
#endif

static int te_hash_submit_clear( hash_drv_ctx_t *hctx );

static uint32_t md5_iv[] = {
    HTOLE32(0x67452301U),
    HTOLE32(0xEFCDAB89U),
    HTOLE32(0x98BADCFEU),
    HTOLE32(0x10325476U)
};

static uint32_t sha1_iv[] = {
    HTOBE32(0x67452301U),
    HTOBE32(0xEFCDAB89U),
    HTOBE32(0x98BADCFEU),
    HTOBE32(0x10325476U),
    HTOBE32(0xC3D2E1F0U)
};

static uint32_t sha224_iv[] = {
    HTOBE32(0xC1059ED8U),
    HTOBE32(0x367CD507U),
    HTOBE32(0x3070DD17U),
    HTOBE32(0xF70E5939U),
    HTOBE32(0xFFC00B31U),
    HTOBE32(0x68581511U),
    HTOBE32(0x64F98FA7U),
    HTOBE32(0xBEFA4FA4U)
};

static uint32_t sha256_iv[] = {
    HTOBE32(0x6A09E667U),
    HTOBE32(0xBB67AE85U),
    HTOBE32(0x3C6EF372U),
    HTOBE32(0xA54FF53AU),
    HTOBE32(0x510E527FU),
    HTOBE32(0x9B05688CU),
    HTOBE32(0x1F83D9ABU),
    HTOBE32(0x5BE0CD19U)
};

static uint64_t sha384_iv[] = {
    HTOBE64(0xCBBB9D5DC1059ED8ULL),
    HTOBE64(0x629A292A367CD507ULL),
    HTOBE64(0x9159015A3070DD17ULL),
    HTOBE64(0x152FECD8F70E5939ULL),
    HTOBE64(0x67332667FFC00B31ULL),
    HTOBE64(0x8EB44A8768581511ULL),
    HTOBE64(0xDB0C2E0D64F98FA7ULL),
    HTOBE64(0x47B5481DBEFA4FA4ULL)

};

static uint64_t sha512_iv[] = {
    HTOBE64(0x6A09E667F3BCC908ULL),
    HTOBE64(0xBB67AE8584CAA73BULL),
    HTOBE64(0x3C6EF372FE94F82BULL),
    HTOBE64(0xA54FF53A5F1D36F1ULL),
    HTOBE64(0x510E527FADE682D1ULL),
    HTOBE64(0x9B05688C2B3E6C1FULL),
    HTOBE64(0x1F83D9ABFB41BD6BULL),
    HTOBE64(0x5BE0CD19137E2179ULL)
};

static uint32_t sm3_iv[] = {
    HTOBE32(0x7380166fU),
    HTOBE32(0x4914b2b9U),
    HTOBE32(0x172442d7U),
    HTOBE32(0xda8a0600U),
    HTOBE32(0xa96f30bcU),
    HTOBE32(0x163138aaU),
    HTOBE32(0xe38dee4dU),
    HTOBE32(0xb0fb0e4eU)
};

static int te_hash_drv_suspend( struct te_crypt_drv* drv, te_suspend_state_t state )
{
    int ret = TE_ERROR_GENERIC;
    te_hash_drv_t *hdrv = NULL;
    te_hwa_hash_t *hash = NULL;
    te_sca_ctl_t ctl = { 0 };

    TE_ASSERT( drv != NULL );

    hdrv = (te_hash_drv_t *)drv;
    hash = (te_hwa_hash_t *)hdrv->base.hwa;

    if (TE_SUSPEND_MEM == state) {
        ret = te_sess_module_suspend( (te_sess_inst_t *)hdrv->sctx );
        if ( ret != TE_SUCCESS ) {
            return ret;
        }
    }

#ifdef CFG_TE_IRQ_EN
    ret = hash->get_int_msk( hash, &hdrv->pm->msk );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't get hash intr mask!\n" );
#endif

    ret = hash->get_suspd_msk( hash, &hdrv->pm->suspd );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't get hash suspd mask!\n" );

    ret = hash->get_ctrl( hash, &hdrv->pm->ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't get hash ctrl!\n" );

    /* Stop the engine */
    ctl.csq_en = 0;
    ctl.cq_wm = 0;
    ctl.clk_en = 0;
    ctl.run = 0;
    ret = hash->set_ctrl( hash, &ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set hash ctrl!\n" );

    return TE_SUCCESS;
}

static int te_hash_drv_resume( struct te_crypt_drv* drv, te_suspend_state_t state )
{
    int ret = TE_ERROR_GENERIC;
    te_hash_drv_t *hdrv = NULL;
    te_hwa_hash_t *hash = NULL;

    TE_ASSERT( drv != NULL );

    hdrv = (te_hash_drv_t *)drv;
    hash = (te_hwa_hash_t *)hdrv->base.hwa;

#ifdef CFG_TE_IRQ_EN
    ret = hash->set_int_msk( hash, &hdrv->pm->msk );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set hash intr mask!\n" );
#endif

    ret = hash->set_suspd_msk( hash, &hdrv->pm->suspd );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set hash suspd mask!\n" );

    ret = hash->set_ctrl( hash, &hdrv->pm->ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set hash ctrl!\n" );

    if (TE_SUSPEND_MEM == state) {
        ret = te_sess_module_resume( (te_sess_inst_t *)hdrv->sctx );
    }

    return ret;
}

static void te_hash_drv_destroy( struct te_crypt_drv* drv )
{
    int ret = TE_ERROR_GENERIC;
    te_hash_drv_t *hdrv = NULL;
    te_hwa_hash_t *hash = NULL;
    te_sca_ctl_t ctl = { 0 };
    te_sca_int_t msk __te_unused = { 0 };
    TE_ASSERT( drv != NULL );

    hdrv = (te_hash_drv_t *)drv;
    hash = (te_hwa_hash_t *)hdrv->base.hwa;
    te_sess_module_deinit( (te_sess_inst_t *)hdrv->sctx );
#ifdef CFG_TE_ASYNC_EN
    hash_worker_destroy( hdrv->worker );
#endif
    /* Stop the engine */
    ctl.csq_en = 0;
    ctl.cq_wm = 0;
    ctl.clk_en = 0;
    ctl.run = 0;
    ret = hash->set_ctrl( hash, &ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set hash ctrl!\n" );

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
    ret = hash->set_int_msk( hash, &msk );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set hash intr mask!\n" );
#endif

    osal_free( hdrv->pm );
    osal_free( hdrv->pd );
    osal_memset( hdrv, 0, sizeof(*hdrv) );
    return;
}

static int hash_drv_capacity( te_hash_drv_t *drv, uint32_t *capacity )
{
    te_rtl_conf_t conf = {0};
    te_hwa_host_t *hwa_host = NULL;
    te_hwa_stat_t *hwa_stat = NULL;
    int ret = TE_SUCCESS;

    if ( (NULL == drv ) || (NULL == capacity) ||
         (HASH_DRV_MAGIC != drv->magic) ) {
        return TE_ERROR_BAD_PARAMS;
    }

    hwa_host = hwa_crypt_host( (te_hwa_crypt_t *)drv->base.hwa );
    TE_ASSERT( hwa_host != NULL );
    hwa_stat = &hwa_host->stat;
    TE_ASSERT( hwa_stat != NULL );
    ret = hwa_stat->conf( hwa_stat, &conf );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                    "Fatal error, can't read host state conf!\n" );

    /* Set default algorithm bitmap */
    *capacity = 0x0UL;

    /* Check if support key-ladder 256 and mark the bitmap */
    if ( conf.cfg1.otp_skey_256 ) {
        *capacity |= HASH_DRV_KLAD256;
    }

    return TE_SUCCESS;
}

int te_hash_drv_init( te_hash_drv_t *drv,
                      const te_hwa_hash_t *hash,
                      const char* name )
{
    bool ishash = true;
    int ret = TE_ERROR_GENERIC;
    te_hwa_host_t *hwa_host = NULL;
    te_hwa_stat_t *hwa_stat = NULL;
    te_sca_ctl_t ctl = { 0 };
    te_rtl_conf_t conf = { 0 };
    te_sca_int_t msk __te_unused = { 0 };
    te_sca_int_t intr __te_unused = { 0 };

    if ( !drv || !hash ) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* initialize driver */
    if ((drv->magic == HASH_DRV_MAGIC) &&
        (osal_atomic_load(&drv->base.refcnt))) {
        /* already initialized */
        return TE_SUCCESS;
    }

    osal_memset(drv, 0, sizeof(*drv));
    drv->pm = (hash_pm_ctx_t *)osal_calloc( 1, sizeof(hash_pm_ctx_t) );
    if ( drv->pm == NULL ) {
        return TE_ERROR_OOM;
    }
    drv->pd = (hash_prvdata_t *)osal_calloc(1, sizeof(hash_prvdata_t));
    if (NULL == drv->pd) {
        osal_free(drv->pm);
        return TE_ERROR_OOM;
    }

    drv->sctx = te_sess_module_init( (void *)hash, ishash );
    if ( NULL == drv->sctx ) {
        osal_free( drv->pm );
        osal_free( drv->pd );
        return TE_ERROR_OOM;
    }

#ifdef CFG_TE_ASYNC_EN
    drv->worker = hash_worker_init();
    if ( drv->worker == NULL ) {
        osal_free( drv->pm );
        osal_free( drv->pd );
        te_sess_module_deinit( drv->sctx );
        return TE_ERROR_OOM;
    }
#endif

    drv->magic = HASH_DRV_MAGIC;
    drv->base.hwa = (te_hwa_crypt_t *)hash;
    /* reset refcnt */
    osal_atomic_store( &drv->base.refcnt, 0U );
    /* install hooks */
    drv->base.suspend = te_hash_drv_suspend;
    drv->base.resume = te_hash_drv_resume;
    drv->base.destroy = te_hash_drv_destroy;
    if ( name ) {
        osal_strncpy(drv->base.name, name, TE_MAX_DRV_NAME - 1);
    }

    hwa_host = hwa_crypt_host( (te_hwa_crypt_t *)&hash->base );
    hwa_stat = &hwa_host->stat;

    ret = hwa_stat->conf( hwa_stat, &conf );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't read host top conf!\n" );
    ctl.csq_en = 1;
    ctl.cq_wm = 1;
    ret = hash_drv_capacity( drv, &drv->pd->caps );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, failed to read hash driver capacity!\n" );
#ifndef CFG_TE_DYNCLK_CTL
    ctl.clk_en = 1;
#endif
    ctl.run = 1;
    ret = hash->set_ctrl( (te_hwa_hash_t *)hash, &ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set hash ctrl!\n" );

#ifdef CFG_TE_IRQ_EN
    /* Cleanup intr status */
    ret = hash->int_state( (te_hwa_hash_t *)hash, &intr );
    TE_ASSERT_MSG( (ret == TE_SUCCESS), "Fatal error, Can't get interrupt stat\n");

    ctl.clk_en = 1;
    ret = hash->set_ctrl( (te_hwa_hash_t *)hash, &ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set hash ctrl!\n" );

    ret = hash->eoi( (te_hwa_hash_t *)hash, &intr );
    TE_ASSERT_MSG( (ret == TE_SUCCESS), "Fatal error, Can't ack interrupt\n");

#ifdef CFG_TE_DYNCLK_CTL
    ctl.clk_en = 0;
    ret = hash->set_ctrl( (te_hwa_hash_t *)hash, &ctl );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set hash ctrl!\n" );
#endif

    /* Enable host interrupts */
    msk.stat.cq_wm = 1; /* Disable WM interrupt */
    msk.stat.opcode_err = 0;
    msk.stat.csq_rd_err = 0;
    msk.stat.cq_wr_err = 0;
    msk.stat.axi_to_err = 0;
    /* Enable all slots finish interrupt */
    msk.cmd_fin = 0;
    /* Enable all slots error interrupt */
    msk.op_err = 0;
    ret = hash->set_int_msk( (te_hwa_hash_t *)hash, &msk );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't set hash intr mask!\n" );
#endif

    te_crypt_drv_get( &drv->base );

    return TE_SUCCESS;
}

int te_hash_drv_exit( te_hash_drv_t *drv)
{
    if ( !drv )
        return TE_ERROR_BAD_PARAMS;

    if ( drv->magic != HASH_DRV_MAGIC )
        return TE_ERROR_BAD_FORMAT;

    te_crypt_drv_put( &drv->base );
    return TE_SUCCESS;
}

int te_hash_alloc_ctx( struct te_hash_drv *drv,
                       te_algo_t alg,
                       uint32_t size,
                       te_crypt_ctx_t **ctx )
{
    hash_drv_ctx_t *hctx = NULL;
    uint32_t blksz = 0;
    void *iv = NULL;
    uint32_t ivsz = 0;
    uint32_t len = 0;

    if ( !drv || !ctx ) {
        return TE_ERROR_BAD_PARAMS;
    }

    if ( (TE_ALG_GET_CLASS( alg ) != TE_OPERATION_MAC) &&
         (TE_ALG_GET_CLASS( alg ) != TE_OPERATION_DIGEST) ) {
        return TE_ERROR_BAD_PARAMS;
    }

    switch (TE_ALG_GET_MAIN_ALG(alg)){
    case TE_MAIN_ALGO_MD5:
        blksz = TE_MD5_BLK_SIZE;
        len = TE_MD5_HASH_SIZE;
        iv = md5_iv;
        ivsz = sizeof(md5_iv);
        break;
    case TE_MAIN_ALGO_SHA1:
        blksz = TE_SHA1_BLK_SIZE;
        len = TE_SHA1_HASH_SIZE;
        iv = sha1_iv;
        ivsz = sizeof(sha1_iv);
        break;
    case TE_MAIN_ALGO_SHA224:
        blksz = TE_SHA224_BLK_SIZE;
        len = TE_SHA224_HASH_SIZE;
        iv = sha224_iv;
        ivsz = sizeof(sha224_iv);
        break;
    case TE_MAIN_ALGO_SHA256:
        blksz = TE_SHA256_BLK_SIZE;
        len = TE_SHA256_HASH_SIZE;
        iv = sha256_iv;
        ivsz = sizeof(sha256_iv);
        break;
    case TE_MAIN_ALGO_SHA384:
        blksz = TE_SHA384_BLK_SIZE;
        len = TE_SHA384_HASH_SIZE;
        iv = sha384_iv;
        ivsz = sizeof(sha384_iv);
        break;
    case TE_MAIN_ALGO_SHA512:
        blksz = TE_SHA512_BLK_SIZE;
        len = TE_SHA512_HASH_SIZE;
        iv = sha512_iv;
        ivsz = sizeof(sha512_iv);
        break;
    case TE_MAIN_ALGO_SM3:
        blksz = TE_SM3_BLK_SIZE;
        len = TE_SM3_HASH_SIZE;
        iv = sm3_iv;
        ivsz = sizeof(sm3_iv);
        break;
    default:
        return TE_ERROR_BAD_PARAMS;
    }

    hctx = (hash_drv_ctx_t *)
           osal_calloc( 1, (sizeof(hash_drv_ctx_t) + size) );

    if ( hctx == NULL ) {
        return TE_ERROR_OOM;
    }

    hctx->base.alg = alg;
    hctx->base.blk_size = blksz;
    hctx->base.ctx_size = size;
    hctx->base.__ctx = hctx + 1;
    hctx->base.drv = (te_crypt_drv_t *)drv;
    hctx->hlen = len;
    hctx->magic = HASH_CTX_MAGIC;
    hctx->sess = INVALID_SESS_ID;

    /* Fill IV */
    memcpy( (void *)hctx->iv, iv, ivsz );
    osal_cache_flush( hctx->iv, ivsz );

    /* Upate to INIT state */
    hctx->state = TE_DRV_HASH_STATE_INIT;

    *ctx = &hctx->base;

    te_crypt_drv_get( &drv->base );

    return TE_SUCCESS;
}

int te_hash_free_ctx( te_crypt_ctx_t *ctx )
{
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;
    if ( !ctx ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC ),
                     "BUG: Not valid hash driver context\n" );
    /* State machine check */
    if ( hctx->state != TE_DRV_HASH_STATE_INIT ) {
        return TE_ERROR_BAD_STATE;
    }

    te_crypt_drv_put( ctx->drv );
    osal_free( hctx );
    return TE_SUCCESS;
}

int te_hash_state( te_crypt_ctx_t *ctx )
{
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;
    if ( !ctx ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );

    return hctx->state;
}

static int te_hash_submit_clear( hash_drv_ctx_t *hctx )
{
    int ret = TE_ERROR_GENERIC, len = ( HASH_CLEAR_CMD_SZ << 2 );
    uint32_t ptr[HASH_CLEAR_CMD_SZ] = { 0 };

    HASH_SET_CMD_FIELD( ptr, (HASH_CLEAR_CMD |
                             TRIGGER_INT) );
    ret = te_sess_submit( hctx->sess,
                          (const uint32_t *)ptr, len );
    if ( ret != TE_SUCCESS ) {
        OSAL_LOG_ERR( "CLEAR command failed on session(%d)\n", hctx->sess );
    }

    return ret;
}

static void hash_error_cleanup( hash_drv_ctx_t *hctx )
{
    int ret = TE_ERROR_GENERIC;

    ret = te_sess_cancel( hctx->sess );
    TE_ASSERT( ret == TE_SUCCESS );

    ret = te_hash_submit_clear(hctx);
    TE_ASSERT( ret == TE_SUCCESS );

    ret = te_sess_close( hctx->sess );
    TE_ASSERT( ret == TE_SUCCESS );
    hctx->sess = INVALID_SESS_ID;
    /* Roll back to INIT stat, user can start or free */
    hctx->state = TE_DRV_HASH_STATE_INIT;
    return;
}

int te_hash_start( te_crypt_ctx_t *ctx,
                   te_hmac_key_t *key )
{
#define HASH_INIT_IV_NS_SHIFT           (12)
#define HASH_INIT_KEY_NS_SHIFT          (11)
    int ret = TE_ERROR_GENERIC, len = ( HASH_INIT_CMD_SZ<<2 );
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;
    struct te_hash_drv *drv = NULL;
    te_sess_slot_cat_t cat = { 0 };
    te_algo_t alg = { 0 };
    uint32_t ptr[HASH_INIT_CMD_SZ] = { 0 };
    uint64_t keyaddr = 0;
    uint32_t keytype = 0;
    uint32_t ns_attr = 0U;
    uint32_t keylen = 0UL;

    if ( ctx == NULL ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );
    alg = hctx->base.alg;
    drv = (struct te_hash_drv *)hctx->base.drv;

    if ( TE_ALG_GET_CLASS( alg ) == TE_OPERATION_MAC ) {
        if ( key == NULL ) {
            return TE_ERROR_BAD_PARAMS;
        }
        if ( (key->type == TE_KEY_TYPE_SEC_V2) &&
             !(drv->pd->caps & HASH_DRV_KLAD256) ) {
                OSAL_LOG_ERR("key-ladder 256 is not supported!\n");
                return TE_ERROR_NOT_SUPPORTED;
        }

        if ( key && (key->type != TE_KEY_TYPE_SEC) &&
            (key->type != TE_KEY_TYPE_USER) &&
            (key->type != TE_KEY_TYPE_SEC_V2) ) {
            return TE_ERROR_BAD_PARAMS;
        }

        if ( key && (key->type == TE_KEY_TYPE_SEC) &&
             ( ((key->sec.sel != TE_KL_KEY_MODEL) &&
                (key->sec.sel != TE_KL_KEY_ROOT)) ||
               ((key->sec.ek3bits != 128) &&
                (key->sec.ek3bits != 256)) ) ) {
            return TE_ERROR_BAD_PARAMS;
        }

        if ( (key->type == TE_KEY_TYPE_SEC_V2) &&
             (((key->sec.sel != TE_KL_KEY_MODEL) &&
                (key->sec.sel != TE_KL_KEY_ROOT)) ||
             (key->sec.ek3bits != 256)) ) {
            return TE_ERROR_BAD_PARAMS;
        }
    }

    if ( hctx->state != TE_DRV_HASH_STATE_INIT ) {
        return TE_ERROR_BAD_STATE;
    }

    if ( (TE_ALG_GET_MAIN_ALG(alg) == TE_MAIN_ALGO_SHA384) ||
         (TE_ALG_GET_MAIN_ALG(alg) == TE_MAIN_ALGO_SHA512) ) {
        cat = TE_SLOT_CATEGORY_LONG;
    } else {
        cat = TE_SLOT_CATEGORY_SHORT;
    }

    /* open te session */
    hctx->sess = te_sess_open( (te_sess_inst_t *)drv->sctx, cat );
    if ( hctx->sess < 0 ) {
        ret = hctx->sess;
        hctx->sess = INVALID_SESS_ID;
        return ret;
    }

    /* HMAC case */
    if ( TE_ALG_GET_CLASS( alg ) == TE_OPERATION_MAC ) {
        /* Backup the HMAC key, be used by te_hash_reset */
        if ( key && (key != &hctx->key) ) {
            memcpy( (void *)&hctx->key, (void *)key, sizeof(te_hmac_key_t) );
            osal_cache_flush( (uint8_t *)&hctx->key, sizeof(te_hmac_key_t) );
        }

        if ( (key->type == TE_KEY_TYPE_SEC) ||
             (key->type == TE_KEY_TYPE_SEC_V2) ) {
            keytype |= ( (key->sec.sel == TE_KL_KEY_MODEL) ?
                         (HASH_MODEL_KEY) : (HASH_ROOT_KEY) );
            keytype |= ( (key->sec.ek3bits == 128) ?
                         (HASH_128BIT_KEY) : (HASH_256BIT_KEY) );
            keylen = sizeof( hctx->key.sec.ek1 ) + sizeof( hctx->key.sec.ek2 ) +
                     (hctx->key.sec.ek3bits >> 3);
            if ( key->type == TE_KEY_TYPE_SEC ) {
                keyaddr = osal_virt_to_phys( hctx->key.sec.eks );
            } else {
                HASH_SET_CMD_FIELD( ptr, HASH_KLAD_256 );
                keyaddr = osal_virt_to_phys( hctx->key.sec_v2.eks );
            }
        } else {
            keytype |= HASH_EXT_KEY;
            keyaddr = osal_virt_to_phys( hctx->key.hkey );
            keylen = sizeof( hctx->key.hkey );
        }
        /* set non-secure attribute for key */
        ret = te_com_set_nsec_attr(keyaddr, keylen,
                                   &ns_attr, HASH_INIT_KEY_NS_SHIFT);
        if ( ret != TE_SUCCESS ) {
            goto err1;
        }
        HASH_SET_KEY_FIELD( ptr, HTOLE64(keyaddr) );
        HASH_SET_CMD_FIELD( ptr, keytype );
        HASH_SET_CMD_FIELD( ptr, HASH_HMAC_MODE );

    } else {
        /* Digest case */
        HASH_SET_CMD_FIELD( ptr, HASH_DIGEST_MODE );
        /* Digest case don't need key address parameter */
        len -= sizeof(uint64_t);
    }

    switch ( TE_ALG_GET_MAIN_ALG(alg) ){
    case TE_MAIN_ALGO_MD5:
        HASH_SET_CMD_FIELD( ptr, HASH_MD5_ALG );
        break;
    case TE_MAIN_ALGO_SHA1:
        HASH_SET_CMD_FIELD( ptr, HASH_SHA1_ALG );
        break;
    case TE_MAIN_ALGO_SHA224:
        HASH_SET_CMD_FIELD( ptr, HASH_SHA224_ALG );
        break;
    case TE_MAIN_ALGO_SHA256:
        HASH_SET_CMD_FIELD( ptr, HASH_SHA256_ALG );
        break;
    case TE_MAIN_ALGO_SHA384:
        HASH_SET_CMD_FIELD( ptr, HASH_SHA384_ALG );
        break;
    case TE_MAIN_ALGO_SHA512:
        HASH_SET_CMD_FIELD( ptr, HASH_SHA512_ALG );
        break;
    case TE_MAIN_ALGO_SM3:
        HASH_SET_CMD_FIELD( ptr, HASH_SM3_ALG );
        break;
    default:
        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }


    HASH_SET_IV_FIELD( ptr, (osal_virt_to_phys(hctx->iv)) );

    /* set non-secure attribute for key */
    ret = te_com_set_nsec_attr(osal_virt_to_phys(hctx->iv), sizeof( hctx->iv ),
                                &ns_attr, HASH_INIT_IV_NS_SHIFT);
    if ( ret != TE_SUCCESS ) {
        goto err1;
    }

    HASH_SET_CMD_FIELD( ptr, (HASH_INIT_CMD | TRIGGER_INT | ns_attr) );

    ret = te_sess_submit( hctx->sess,
                          (const uint32_t *)ptr, len );
    if ( ret != TE_SUCCESS ) {
        OSAL_LOG_ERR( "INIT command failed on session(%d)\n", hctx->sess );
        goto err2;
    }

    /* update state machine */
    hctx->state = TE_DRV_HASH_STATE_START;

    return TE_SUCCESS;

err2:
    if ( te_hash_submit_clear(hctx) != TE_SUCCESS ) {
        OSAL_LOG_ERR( "Can't CLEAR session(%d)\n", hctx->sess );
    }

err1:
    if ( te_sess_close( hctx->sess ) != TE_SUCCESS ) {
        OSAL_LOG_ERR( "Can't close session(%d)\n", hctx->sess );
    }
    hctx->sess = INVALID_SESS_ID;

    return ret;
#undef HASH_INIT_IV_NS_SHIFT
#undef HASH_INIT_KEY_NS_SHIFT
}

static int te_hash_last_update( hash_drv_ctx_t *hctx )
{
    int ret = TE_ERROR_GENERIC, len = ( HASH_PROC_NORMAL_SZ << 2 );
    uint32_t ptr[HASH_PROC_NORMAL_SZ] = { 0 };
    uint64_t addr = 0;
    uint32_t ns_attr = 0U;

    addr = HTOLE64( osal_virt_to_phys(hctx->npdata) );
    if ( hctx->npdlen ) {
        osal_cache_flush( hctx->npdata, hctx->npdlen );
        HASH_SET_PROC_SRC_LEN( ptr, HTOLE64(hctx->npdlen - 1) );
        HASH_SET_PROC_SRC_ADDR( ptr, HTOLE64(addr) );
        /* set non-secure attribute of src */
        ret = te_com_set_nsec_attr( addr, hctx->npdlen,
                                    &ns_attr, HASH_PROC_SRC_NS_SHIFT );
        if ( ret != TE_SUCCESS ) {
            return ret;
        }
    } else {
        HASH_SET_CMD_FIELD( ptr, HASH_PROC_ZERO );
        /* minus the sizeof(source address + len), if zero process */
        len -= (sizeof(uint32_t) * 3);
    }
    HASH_SET_CMD_FIELD( ptr, HASH_PROC_PADDING );

    HASH_SET_CMD_FIELD( ptr, (HASH_PROC_CMD |
                              HASH_PROC_LAST |
                              TRIGGER_INT | ns_attr) );

    ret = te_sess_submit( hctx->sess,
                          (const uint32_t *)ptr, len );
    if ( ret != TE_SUCCESS ) {
        OSAL_LOG_ERR( "PROC(last) command failed on session(%d)\n", hctx->sess );
        hash_error_cleanup( hctx );
        return ret;
    }

    hctx->npdlen = 0;
    hctx->state = TE_DRV_HASH_STATE_LAST;

    return TE_SUCCESS;
}

int te_hash_update( te_crypt_ctx_t *ctx,
                    const uint8_t *in,
                    size_t ilen )
{
    int ret = TE_ERROR_GENERIC, len = ( HASH_PROC_LLST_SZ << 2 );
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;
    uint64_t total = 0, remain = 0, process = 0;
    uint64_t filled = 0, fillsz = 0;
    int entry = 0, max_entry = 0;
    link_list_t *link_list = NULL;
    uint32_t ptr[HASH_PROC_LLST_SZ] = { 0 };
    uint64_t addr = 0;
    uint32_t ns_attr = 0UL;

    if ( !ctx || ( !in && (ilen != 0)) ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC ),
                     "BUG: Not valid hash driver context\n" );

    if ( (hctx->state != TE_DRV_HASH_STATE_START) &&
         (hctx->state != TE_DRV_HASH_STATE_UPDATE) ) {
        return TE_ERROR_BAD_STATE;
    }

    /*
     * first for npdata, second for 'in'(>4G remainder)
     * and the last entry is the end of list
     */
    max_entry += 3;
    /* ( >4GB ) calc how many additional link_list entry do we need */
    max_entry += ilen / LLST_ENTRY_SZ_MAX;

    link_list = (link_list_t *)osal_malloc_aligned
                    ( (max_entry * sizeof(link_list_t)), LINK_LIST_ALIGN );

    if ( !link_list ) {
        return TE_ERROR_OOM;
    }
    osal_memset( (void *)link_list, 0, (max_entry * sizeof(link_list_t)) );

    addr = osal_virt_to_phys( (void *)link_list );
    total = ilen + hctx->npdlen;
    process = ( total & (~(hctx->base.blk_size - 1ULL)) );
    remain = ( total & (hctx->base.blk_size - 1ULL) );

    if ( process == 0 ) {
        goto npdata;
    }

    if ( hctx->npdlen ) {
        link_list[entry].sz_attr.sz = HTOLE64( hctx->npdlen - 1 );
        link_list[entry].addr = HTOLE64( osal_virt_to_phys( hctx->npdata ) );
        osal_cache_flush( hctx->npdata, hctx->npdlen );
        entry++;
    }

    while ( filled < (ilen - remain) ) {

        fillsz = ( ((ilen - remain - filled) > LLST_ENTRY_SZ_MAX) ?
                                                LLST_ENTRY_SZ_MAX :
                                                (ilen - remain - filled) );
        link_list[entry].sz_attr.sz = HTOLE64( fillsz - 1 );
        link_list[entry].addr = HTOLE64(
                    osal_virt_to_phys( (void *)(uintptr_t)
                                       ((uintptr_t)in + filled)) );
        OSAL_LOG_DEBUG( "Append data(phys:%llx, virt:%llx) to link_list\n",
                        osal_virt_to_phys( (void *)(uintptr_t)
                                           ((uintptr_t)in+filled)),
                                           ((uintptr_t)in+filled) );

        osal_cache_flush( (uint8_t *)(uintptr_t)
                          ((uintptr_t)in + filled), fillsz );

        filled += fillsz;
        entry++;
    }

    /* End of link list */
    link_list[entry].val = 0xffffffffffffffffULL;
    link_list[entry].addr = 0x0ULL;

    TE_ASSERT( entry < max_entry );

    /* set the non-secure bit for each node of the list */
    ret = te_com_set_llst_nsec_attr( link_list );
    if (ret != TE_SUCCESS) {
        osal_free( link_list );
        return ret;
    }

    /* set non-secure attribute of src */
    ret = te_com_set_nsec_attr( addr, max_entry * sizeof(link_list_t),
                                &ns_attr, HASH_PROC_SRC_NS_SHIFT );
    if ( ret != TE_SUCCESS ) {
        osal_free( link_list );
        return ret;
    }
    osal_cache_flush( (uint8_t *)link_list, (max_entry * sizeof(link_list_t)) );

    HASH_SET_PROC_SRC_LLST( ptr, HTOLE64(addr) );
    HASH_SET_CMD_FIELD( ptr, (HASH_PROC_CMD |
                              HASH_LINK_LIST_MODE |
                              TRIGGER_INT | ns_attr) );

    ret = te_sess_submit( hctx->sess,
                          (const uint32_t *)ptr, len );
    if ( ret != TE_SUCCESS ) {
        OSAL_LOG_ERR( "PROC command failed on session(%d)\n", hctx->sess );
        osal_free( link_list );
        hash_error_cleanup( hctx );
        return ret;
    }

npdata:
    /*
     * Here are these cases:
     *  ilen == 0 : process = 0, remain = hctx->npdlen, no data processed.
     *
     *  ilen + npdlen >= block size : npdata will be processed and empty.
     *
     *  ilen + npdlen < block size : which means process = 0,
     *  remain = (npdlen + ilen), need append data to npdata.
     */
    if ( (ilen != 0) && remain && (process != 0) ) {
        /* take care of the input data, it should be processed, partially */
        memcpy( (void *)hctx->npdata,
                (void *)(uintptr_t)((uintptr_t)in + ilen - remain),
                remain );
    } else if ( (ilen != 0) && remain && (process == 0) ) {
        /* No data be processed at all, append new data to npdate */
        memcpy( (void *)&hctx->npdata[hctx->npdlen],
                (void *)in,
                ilen );
    }

    /* Update npdlen */
    hctx->npdlen = remain;

    osal_free( link_list );
    if ( hctx->state == TE_DRV_HASH_STATE_START ) {
        hctx->state = TE_DRV_HASH_STATE_UPDATE;
    }

    return TE_SUCCESS;
}

int te_hash_uplist( te_crypt_ctx_t *ctx,
                    te_memlist_t *in )
{
    int ret = TE_ERROR_GENERIC, len = ( HASH_PROC_LLST_SZ << 2);
    uint32_t i = 0;
    uint64_t total = 0, process = 0, remain = 0, ilen = 0;
    uint64_t linked = 0, offs = 0;
    uint64_t filled = 0, fillsz = 0;
    int entry = 0, max_entry = 0;
    uint64_t addr = 0;
    uint32_t ptr[HASH_PROC_LLST_SZ] = { 0 };
    link_list_t *link_list = NULL;
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;
    uint32_t ns_attr = 0U;

    if ( !ctx || !in || (!in->ents && in->nent) ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );

    if ( (hctx->state != TE_DRV_HASH_STATE_START) &&
         (hctx->state != TE_DRV_HASH_STATE_UPDATE) ) {
        return TE_ERROR_BAD_STATE;
    }

    /* one for npdata, the other for end of list */
    max_entry = 2;
    for ( i = 0; i < in->nent; i++ ) {
        ilen += in->ents[i].len;
        max_entry += in->ents[i].len / LLST_ENTRY_SZ_MAX;
        if ( in->ents[i].len % (LLST_ENTRY_SZ_MAX) ) {
            max_entry++;
        }
    }

    link_list = (link_list_t *)osal_malloc_aligned
                    ( (max_entry * sizeof(link_list_t)), LINK_LIST_ALIGN );

    if ( !link_list ) {
        return TE_ERROR_OOM;
    }
    osal_memset( (void *)link_list, 0, (max_entry * sizeof(link_list_t)) );

    total = ilen + hctx->npdlen;
    process = ( total & (~(hctx->base.blk_size - 1ULL)) );
    remain = ( total & (hctx->base.blk_size - 1ULL) );

    if ( process == 0 ) {
        goto npdata;
    }

    if ( hctx->npdlen ) {
        link_list[entry].sz_attr.sz = HTOLE64( hctx->npdlen - 1 );
        link_list[entry].addr =
            HTOLE64( osal_virt_to_phys( (void *)hctx->npdata ) );
        OSAL_LOG_DEBUG_DUMP_DATA( "npdata: ",
                                  hctx->npdata,
                                  hctx->npdlen );
        OSAL_LOG_DEBUG("\n");
        osal_cache_flush( hctx->npdata, hctx->npdlen );
        entry++;
    }

    linked += hctx->npdlen;
    for ( i = 0; i < in->nent; i++ ) {
        filled = 0;
        OSAL_LOG_DEBUG_DUMP_DATA( "mem list: ",
                                  in->ents[i].buf,
                                  in->ents[i].len );
        OSAL_LOG_DEBUG("\n");
        while ( filled < in->ents[i].len ) {
            fillsz = ( ((in->ents[i].len - filled) > LLST_ENTRY_SZ_MAX) ?
                        LLST_ENTRY_SZ_MAX : in->ents[i].len - filled );

            link_list[entry].sz_attr.sz = HTOLE64( fillsz - 1 );
            link_list[entry].addr = HTOLE64(
                            osal_virt_to_phys( (void *)(uintptr_t)
                                               ((uintptr_t)in->ents[i].buf + filled)) );
            osal_cache_flush( (uint8_t *)(uintptr_t)
                              ((uintptr_t)in->ents[i].buf + filled),
                              fillsz );
            linked += fillsz;
            filled += fillsz;
            if (linked >= process) {
                /* Fixup link list entry size */
                link_list[entry].sz_attr.sz = HTOLE64(
                                      fillsz - (linked - process) - 1 );
                entry++;
                goto done;
            }
            entry++;
        }
    }
done:

    /* End of link list */
    link_list[entry].val = 0xffffffffffffffffULL;
    link_list[entry].addr = 0x0ULL;
    OSAL_LOG_DEBUG_DUMP_DATA("link list:", (uint8_t *)link_list, (max_entry * sizeof(link_list_t)));
    TE_ASSERT( entry < max_entry );

    /* set the non-secure bit for each node of the list */
    ret = te_com_set_llst_nsec_attr(link_list);
    if (ret != TE_SUCCESS) {
        osal_free( link_list );
        return ret;
    }
    /* set non-secure attribute of src */
    ret = te_com_set_nsec_attr( addr, max_entry * sizeof(link_list_t),
                                &ns_attr, HASH_PROC_SRC_NS_SHIFT );
    if ( ret != TE_SUCCESS ) {
        osal_free( link_list );
        return ret;
    }

    osal_cache_flush( (uint8_t *)link_list, (max_entry * sizeof(link_list_t)) );
    addr = osal_virt_to_phys( (void *)link_list );

    HASH_SET_PROC_SRC_LLST( ptr, HTOLE64(addr) );
    HASH_SET_CMD_FIELD( ptr, (HASH_PROC_CMD |
                              HASH_LINK_LIST_MODE |
                              TRIGGER_INT | ns_attr) );
    ret = te_sess_submit( hctx->sess,
                          (const uint32_t *)ptr, len );
    if ( ret != TE_SUCCESS ) {
        OSAL_LOG_ERR( "PROC(link list) command failed on session(%d)\n", hctx->sess );
        osal_free( link_list );
        hash_error_cleanup( hctx );
        return ret;
    }

npdata:
    /*
     * Here are these cases:
     *  ilen == 0 : process = 0, remain = hctx->npdlen, no data processed.
     *
     *  ilen + npdlen >= block size : npdata will be processed and empty.
     *
     *  ilen + npdlen < block size : which means process = 0,
     *  remain = (npdlen + ilen), need append data to npdata.
     */
    if ( (ilen != 0) && remain && (process != 0) ) {
        /* take care of the input data, it has been processed, partially */
        hctx->npdlen = 0;

        /* calculate the internal offset of the ent's buf */
        offs = (filled - (linked - process));

        for (; i< in->nent; i++) {
            memcpy( (void *)&hctx->npdata[hctx->npdlen],
                    (void *)(uintptr_t)((uintptr_t)in->ents[i].buf + offs),
                    (in->ents[i].len - offs) );
            /* update npdlen, accordingly*/
            hctx->npdlen += (in->ents[i].len - offs);
            /* Reset offs to '0', after handle the first ent */
            offs = 0;
        }

    } else if ( (ilen != 0) && remain && (process == 0) ) {
        /* No data be processed at all, append new data to npdate */
        for ( i = 0; i < in->nent; i++ ) {
            memcpy( (void *)&hctx->npdata[hctx->npdlen],
                    in->ents[i].buf,
                    in->ents[i].len );
            hctx->npdlen += in->ents[i].len;
        }
    }

    /* Update npdlen */
    hctx->npdlen = remain;

    osal_free( link_list );

    if ( hctx->state == TE_DRV_HASH_STATE_START ) {
        hctx->state = TE_DRV_HASH_STATE_UPDATE;
    }

    return TE_SUCCESS;
}

static int te_hash_submit_finish( hash_drv_ctx_t *hctx,
                                  uint8_t *out,
                                  uint32_t olen)
{
    int ret = TE_ERROR_GENERIC, len = ( HASH_FINISH_CMD_SZ << 2 );
    uint32_t ptr[HASH_FINISH_CMD_SZ] = { 0 };
    uint8_t *hash = NULL;
    uint64_t addr = 0, keyaddr = 0;
    uint32_t maclen = HASH_MAC_LEN( hctx->hlen );
    uint32_t dma_aligned_sz = UTILS_ROUND_UP( hctx->hlen, TE_DMA_ALIGNED );
    te_algo_t alg = hctx->base.alg;

    hash = (uint8_t *)osal_malloc_aligned( dma_aligned_sz, TE_DMA_ALIGNED );
    if ( hash == NULL ) {
        return TE_ERROR_OOM;
    }

    osal_cache_invalidate( hash, dma_aligned_sz );

    addr = osal_virt_to_phys( (void *) hash );

    HASH_SET_FINISH_DEST( ptr, addr );
    HASH_SET_CMD_FIELD( ptr, (HASH_FINISH_CMD |
                             TRIGGER_INT |
                             maclen) );

    if ( TE_ALG_GET_CLASS( alg ) == TE_OPERATION_MAC ) {

        if ( hctx->key.type == TE_KEY_TYPE_SEC ) {
            keyaddr = osal_virt_to_phys( hctx->key.sec.eks );
        } else  if ( hctx->key.type == TE_KEY_TYPE_SEC_V2 ) {
            keyaddr = osal_virt_to_phys( hctx->key.sec_v2.eks );
        } else{
            keyaddr = osal_virt_to_phys( hctx->key.hkey );
        }

        HASH_SET_IV_FIELD( ptr, (osal_virt_to_phys(hctx->iv)) );
        HASH_SET_KEY_FIELD( ptr, HTOLE64(keyaddr) );
    } else {
        /* Digest case don't need to pass key & iv address */
        len -= sizeof(uint64_t) * 2;
    }

    ret = te_sess_submit( hctx->sess,
                          (const uint32_t *)ptr, len );
    if ( ret != TE_SUCCESS ) {
        OSAL_LOG_ERR( "FINISH command failed on session(%d)\n", hctx->sess );
        hash_error_cleanup( hctx );
        osal_free( hash );
        return ret;
    }

    osal_cache_invalidate( hash, dma_aligned_sz );

    if ( out ) {
        osal_memcpy( out, hash, UTILS_MIN(olen, hctx->hlen) );
    }
    osal_free( hash );
    return ret;
}

int te_hash_finish( te_crypt_ctx_t *ctx,
                    uint8_t *out,
                    uint32_t olen )
{
    int ret = TE_ERROR_GENERIC;
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;

    if ( !ctx || (!out && (olen > 0)) ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );

    if ( (hctx->state != TE_DRV_HASH_STATE_LAST) &&
         (hctx->state != TE_DRV_HASH_STATE_UPDATE) &&
         (hctx->state != TE_DRV_HASH_STATE_START) ) {

        return TE_ERROR_BAD_STATE;
    }

    if ( hctx->state == TE_DRV_HASH_STATE_UPDATE ) {
        ret = te_hash_last_update( hctx);
        if ( ret != TE_SUCCESS ) {
            return ret;
        }
    }

    if ( hctx->state == TE_DRV_HASH_STATE_LAST ) {
        ret = te_hash_submit_finish( hctx, out, olen );
        if ( ret != TE_SUCCESS ) {
            return ret;
        }
    }

    if ( hctx->state == TE_DRV_HASH_STATE_START ) {
        /* Send clear */
        ret = te_hash_submit_clear( hctx );
        if ( ret != TE_SUCCESS ) {
            return ret;
        }
    }

    /* Before transition to STATE_INIT, close session */
    ret = te_sess_close( hctx->sess );
    if ( ret != TE_SUCCESS ) {
        return ret;
    }
    hctx->sess = INVALID_SESS_ID;

    hctx->state = TE_DRV_HASH_STATE_INIT;

    return TE_SUCCESS;
}

int te_hash_reset( te_crypt_ctx_t *ctx )
{
    int ret = TE_ERROR_GENERIC;
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;
    te_hmac_key_t *key = NULL;
    uint8_t hash[TE_MAX_HASH_SIZE] __te_unused  =  { 0 };

    if ( ctx == NULL ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );

    if ( TE_DRV_HASH_STATE_START == hctx->state ) {
        return TE_SUCCESS;
    }

    if ( (hctx->state != TE_DRV_HASH_STATE_UPDATE) &&
         (hctx->state != TE_DRV_HASH_STATE_LAST) ) {
        return TE_ERROR_BAD_STATE;
    }

    if ( (hctx->state == TE_DRV_HASH_STATE_UPDATE) ||
         (hctx->state == TE_DRV_HASH_STATE_LAST) ) {
        ret = te_hash_finish( ctx, hash, hctx->hlen );
        if ( ret != TE_SUCCESS) {
            return ret;
        }
    }

    key = (te_hmac_key_t *)&hctx->key;
    ret = te_hash_start( ctx, key );

    return ret;
}

int te_hash_clone( const te_crypt_ctx_t *src,
                   te_crypt_ctx_t *dst )
{

    hash_drv_ctx_t *sctx = (hash_drv_ctx_t *)src;
    hash_drv_ctx_t *dctx = (hash_drv_ctx_t *)dst;

    if ( (src == NULL) || (dst == NULL) ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (sctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );
    TE_ASSERT_MSG( (dctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );

    if ( dctx->state != TE_DRV_HASH_STATE_INIT ) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * CLONE failed, can not cause our session into error state,
     * Do not need any cleanup operation
     */
    dctx->sess = te_sess_clone( sctx->sess );
    if ( dctx->sess == INVALID_SESS_ID ) {
        return TE_ERROR_GENERIC;
    }

    dctx->npdlen = sctx->npdlen;
    memcpy( (void *)dctx->npdata, (void *)sctx->npdata, sctx->npdlen );
    memcpy( (void *)&dctx->key, (void *)&sctx->key, sizeof(te_hmac_key_t) );
    memcpy( (void *)dctx->iv, (void *)sctx->iv, TE_MAX_HASH_SIZE );
    osal_cache_clean( dctx->iv, TE_MAX_HASH_SIZE );
    osal_cache_clean( (uint8_t *)&dctx->key, sizeof(te_hmac_key_t) );
    dctx->state = sctx->state;

    return TE_SUCCESS;
}

int te_hash_export( te_crypt_ctx_t *ctx,
                    void *out,
                    uint32_t *olen )
{
    int ret = TE_ERROR_GENERIC;
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;
    hash_ehdr_t eh = {
        .magic = HASH_EHDR_MAGIC,
    };

    if ( (NULL == ctx) || (NULL == olen) ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );

    if ( (hctx->state != TE_DRV_HASH_STATE_START) &&
         (hctx->state != TE_DRV_HASH_STATE_UPDATE) &&
         (hctx->state != TE_DRV_HASH_STATE_LAST) ) {
        return TE_ERROR_BAD_STATE;
    }

    /*
     * poll for hwctx_sz
     */
    eh.hwctx_sz = 0;
    ret = te_sess_export( hctx->sess, NULL, &eh.hwctx_sz );
    if (ret != (int)TE_ERROR_SHORT_BUFFER) {
        return ret;
    }

    /*
     * be fancy to the caller
     */
    if (*olen < HASH_EHDR_SIZE(&eh)) {
        *olen = HASH_EHDR_SIZE(&eh);
        return TE_ERROR_SHORT_BUFFER;
    }

    if (NULL == out) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * export hwctx
     * TODO: lock the hash driver to stop service of update() or uplist() on
     * the calling context until te_sess_export() ends.
     * Or, it's the caller responsibility to ensure there be no update() or
     * uplist() call on to the same context when an export() is outstanding.
     */
    ret = te_sess_export( hctx->sess, HASH_EHDR_HWCTX(out), &eh.hwctx_sz );
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR( "te_sess_export error %x\n", ret );
        goto err;
    }

    /*
     * make ehdr
     */
    eh.magic  = HASH_EHDR_MAGIC;
    eh.alg    = hctx->base.alg;
    eh.npdlen = hctx->npdlen;
    eh.state  = hctx->state;
    osal_memcpy( eh.npdata, hctx->npdata, sizeof(eh.npdata) );
    osal_memcpy( &eh.key, &hctx->key, sizeof(eh.key) ); /* FIXME */

    osal_memcpy(out, &eh, sizeof(eh));
    *olen = HASH_EHDR_SIZE(&eh);
err:
    return ret;
}

int te_hash_import( te_crypt_ctx_t *ctx,
                    const void *in,
                    uint32_t ilen )
{
    int ret = TE_ERROR_GENERIC;
    te_sess_id_t sess = INVALID_SESS_ID;
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;
    hash_ehdr_t eh = {0};

    if ( (NULL == ctx) || (NULL == in) ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );

    /*
     * The 'in' might not start at struct ptr safe boundary.
     * Be safe to copy the struct before reading it.
     */
    osal_memcpy(&eh, in, sizeof(eh));

    if ( (eh.magic != HASH_EHDR_MAGIC) ||
         (eh.alg != hctx->base.alg) ||
         (ilen < HASH_EHDR_SIZE(&eh)) ) {
        OSAL_LOG_ERR("Bad or mismatched hash ehdr: %d\n", ilen);
        return TE_ERROR_BAD_PARAMS;
    }

    if ( (hctx->state != TE_DRV_HASH_STATE_INIT) &&
         (hctx->state != TE_DRV_HASH_STATE_START) &&
         (hctx->state != TE_DRV_HASH_STATE_UPDATE) &&
         (hctx->state != TE_DRV_HASH_STATE_LAST) ) {
        return TE_ERROR_BAD_STATE;
    }

    if ( TE_DRV_HASH_STATE_INIT == hctx->state ) {
        /*
         * Open session if ctx is still in INIT state.
         */
        te_sess_slot_cat_t cat = TE_SLOT_CATEGORY_SHORT;
        te_hash_drv_t *drv = (te_hash_drv_t*)ctx->drv;

        if ( (TE_ALG_GET_MAIN_ALG(ctx->alg) == TE_MAIN_ALGO_SHA384) ||
             (TE_ALG_GET_MAIN_ALG(ctx->alg) == TE_MAIN_ALGO_SHA512) ) {
            cat = TE_SLOT_CATEGORY_LONG;
        } else {
            cat = TE_SLOT_CATEGORY_SHORT;
        }

        /* open te session */
        sess = te_sess_open( (te_sess_inst_t *)drv->sctx, cat );
        if (sess < 0) {
            return sess;
        }
        hctx->sess = sess;
    }

    /*
     * import hwctx
     */
    ret = te_sess_import( hctx->sess, HASH_EHDR_HWCTX(in), eh.hwctx_sz );
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR("te_sess_import error %x\n", ret);
        goto err_import;
    }

    /*
     * import hash drv ctx
     */
    osal_memcpy(hctx->npdata, eh.npdata, sizeof(hctx->npdata));
    osal_memcpy(&hctx->key, &eh.key, sizeof(hctx->key));
    osal_cache_clean( (uint8_t *)&hctx->key, sizeof(te_hmac_key_t) );
    hctx->npdlen = eh.npdlen;
    hctx->state  = eh.state;

    return TE_SUCCESS;

err_import:
    if (sess != INVALID_SESS_ID) {
        /*
         * wipe all on errors
         */
        if ( te_hash_submit_clear(hctx) != TE_SUCCESS ) {
            OSAL_LOG_ERR( "Can't CLEAR session(%d)\n", hctx->sess );
        }
        if ( te_sess_close( hctx->sess ) != TE_SUCCESS ) {
            OSAL_LOG_ERR( "Can't close session(%d)\n", hctx->sess );
        }
        hctx->sess = INVALID_SESS_ID;
    }

    return ret;
}

#ifdef CFG_TE_ASYNC_EN
static osal_err_t hworker_thread_entry( void *arg )
{
    unsigned long flags = 0;
    hash_worker_t *worker = (hash_worker_t *)arg;
    hash_async_ctx_t *task = NULL;
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

static hash_worker_t *hash_worker_init(void)
{
    int ret = TE_ERROR_GENERIC;
    hash_worker_t *worker = NULL;

    worker = (hash_worker_t *)osal_calloc( 1, sizeof(hash_worker_t) );
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

static void hash_worker_send_command( hash_worker_t *worker,
                                                uint32_t command )
{
    unsigned long flags = 0;

    osal_spin_lock_irqsave( &worker->lock, &flags );
    worker->command = command;
    osal_spin_unlock_irqrestore( &worker->lock, flags );

    osal_completion_signal( &worker->bell );
    return;
}

static void hash_worker_wait_stop( hash_worker_t *worker )
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

static void hash_worker_enqueue( hash_worker_t *worker, hash_async_ctx_t *task )
{
    unsigned long flags = 0;
    osal_spin_lock_irqsave( &worker->lock, &flags );
    sqlist_enqueue( &worker->tasks, &task->list );
    osal_spin_unlock_irqrestore( &worker->lock, flags );
    osal_completion_signal( &worker->bell );
    return;
}

static void hash_worker_destroy( hash_worker_t *worker )
{

    hash_worker_send_command( worker, HWORKER_CMD_QUIT );
    hash_worker_wait_stop( worker );
    osal_thread_destroy( worker->wthread );
    osal_completion_destroy( &worker->bell );
    osal_spin_lock_destroy( &worker->lock );
    osal_free( worker );
    return;
}

/*
 * This function need be called under thread context
 */
static void hash_astart_done( hash_async_ctx_t *hactx )
{
    hash_drv_ctx_t *hctx = hactx->hctx;
    te_hash_request_t *req = hactx->req;
    te_sess_ar_t *ar = hactx->ar;

    /* Free command frame */
    osal_free( (void *)ar->cmdptr );

    /* Free session async requset */
    osal_free( ar );

    /* update state machine */
    if ( req->res == TE_SUCCESS ) {
        hctx->state = TE_DRV_HASH_STATE_START;
    } else if ( req->res == (int)TE_ERROR_CANCEL ) {
        ;
    } else {
        /* ERROR case, cleanup state machine to TE_DRV_HASH_STATE_INIT */
        hash_error_cleanup( hctx );
    }

    /* Free tempoary hash_async_ctx_t */
    osal_free( hactx );

    /* Notify caller */
    req->base.completion( &req->base, req->res );

    return;
}

/*
 * This function need be called under thread context
 */
static void hash_aupdate_done( hash_async_ctx_t *hactx )
{
    hash_drv_ctx_t *hctx = hactx->hctx;
    te_hash_request_t *req = hactx->req;
    te_sess_ar_t *ar = hactx->ar;

    /* Free command frame */
    osal_free( (void *)ar->cmdptr );

    /* Free session async requset */
    osal_free( ar );

    /* update state machine */
    if ( req->res == TE_SUCCESS ) {
        hctx->state = ( hactx->up.last_update ?
                        TE_DRV_HASH_STATE_LAST :
                        TE_DRV_HASH_STATE_UPDATE );

    } else if ( req->res == (int)TE_ERROR_CANCEL ) {
        /* recovery the hash drv ctx npdata */
        if ( hactx->up.npdata != NULL ) {
            memcpy( (void *)hctx->npdata, (void *)hactx->up.npdata, hactx->up.npdlen );
            hctx->npdlen = hactx->up.npdlen;
        }
    } else {
        /* ERROR case, cleanup state machine to TE_DRV_HASH_STATE_INIT */
        hash_error_cleanup( hctx );
    }

    if ( hactx->up.npdata != NULL ) {
        osal_free( hactx->up.npdata );
        hactx->up.npdlen = 0;
    }

    TE_ASSERT( hactx->up.link_list != NULL );
    osal_free( hactx->up.link_list );
    hactx->up.link_list = NULL;

    /* Free tempoary hash_async_ctx_t */
    osal_free( hactx );

    /* Notify caller */
    req->base.completion( &req->base, req->res );

    return;
}

/*
 * This function need be called under thread context
 */
static void hash_afinish_done( hash_async_ctx_t *hactx )
{
    hash_drv_ctx_t *hctx = hactx->hctx;
    te_hash_request_t *req = hactx->req;
    te_sess_ar_t *ar = hactx->ar;
    uint32_t cpsz = 0;
    int rc = TE_SUCCESS;

    /* Free command frame */
    osal_free( (void *)ar->cmdptr );

    /* Free session async requset */
    osal_free( ar );

    /* update state machine */
    if ( req->res == TE_SUCCESS ) {
        hctx->state = TE_DRV_HASH_STATE_INIT;
        rc = te_sess_close(hctx->sess);
        TE_ASSERT( rc == TE_SUCCESS );
        hctx->sess = INVALID_SESS_ID;
        if ( req->fin.out && req->fin.olen ) {
            cpsz = ( (hactx->fin.hlen > req->fin.olen) ?
                     req->fin.olen : hactx->fin.hlen );

            osal_cache_invalidate( hactx->fin.mac, cpsz);
            memcpy( (void *)req->fin.out, (void *)hactx->fin.mac, cpsz );
        }
    } else if ( req->res == (int)TE_ERROR_CANCEL ) {
        ;
    } else {
        /* ERROR case, cleanup state machine to TE_DRV_HASH_STATE_INIT */
        hash_error_cleanup( hctx );
    }

    /* Free tempoary hash_async_ctx_t */
    osal_free( hactx );

    /* Notify caller */
    req->base.completion( &req->base, req->res );

    return;
}

/*
 * This function need be called under thread context
 */
static void hash_aclone_done( hash_async_ctx_t *hactx )
{
    int ret = TE_ERROR_GENERIC;
    hash_drv_ctx_t *hctx = hactx->hctx;
    te_hash_request_t *req = hactx->req;

    ret = te_hash_clone( hactx->cl.src, hactx->cl.dst );
    req->res = ret;
    if ( ret != TE_SUCCESS ) {
        /* ERROR case, cleanup state machine to TE_DRV_HASH_STATE_INIT */
        hash_error_cleanup( hctx );
    }

    /* Free tempoary hash_async_ctx_t */
    osal_free( hactx );

    /* Notify caller */
    req->base.completion( &req->base, req->res );

    return;
}

static void hash_asubmit_done( te_sess_ar_t *ar )
{
    hash_async_ctx_t *hactx = (hash_async_ctx_t *)ar->para;
    hash_drv_ctx_t *hctx = hactx->hctx;
    te_hash_request_t *req = hactx->req;
    te_hash_drv_t *hdrv = (te_hash_drv_t *)hctx->base.drv;
    hash_worker_t *worker = hdrv->worker;

    /* Update request result */
    req->res = ar->err;

    /* Enqueue task to work thread */
    hash_worker_enqueue( worker, hactx );

    return;
}

int te_hash_astart( te_crypt_ctx_t *ctx,
                    te_hash_request_t *req )
{
    int ret = TE_ERROR_GENERIC, len = ( HASH_INIT_CMD_SZ<<2 );
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;
    struct te_hash_drv *drv = NULL;
    te_sess_slot_cat_t cat = { 0 };
    te_algo_t alg = { 0 };
    uint32_t *ptr = NULL;
    uint64_t keyaddr = 0;
    uint32_t keytype = 0;
    te_sess_ar_t *ar = NULL;
    te_hmac_key_t *key = NULL;
    hash_async_ctx_t *hactx = NULL;

    if ( !ctx || !req ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );
    alg = hctx->base.alg;
    drv = (struct te_hash_drv *)hctx->base.drv;

    if ( TE_ALG_GET_CLASS( alg ) == TE_OPERATION_MAC ) {
        key = &req->st.key;
    }

    if ( key && key->type != TE_KEY_TYPE_SEC && \
         key->type != TE_KEY_TYPE_USER ) {
        return TE_ERROR_BAD_PARAMS;
    }

    if ( key && key->type == TE_KEY_TYPE_SEC &&
         ( (key->sec.sel != TE_KL_KEY_MODEL &&
            key->sec.sel != TE_KL_KEY_ROOT) ||
           (key->sec.ek3bits != 128 &&
            key->sec.ek3bits != 256) ) ) {
        return TE_ERROR_BAD_PARAMS;
    }

    if ( hctx->state != TE_DRV_HASH_STATE_INIT ) {
        return TE_ERROR_BAD_STATE;
    }

    if ( TE_ALG_GET_MAIN_ALG(alg) == TE_MAIN_ALGO_SHA384 ||
         TE_ALG_GET_MAIN_ALG(alg) == TE_MAIN_ALGO_SHA512 ) {
        cat = TE_SLOT_CATEGORY_LONG;
    } else {
        cat = TE_SLOT_CATEGORY_SHORT;
    }

    ptr = (uint32_t *)osal_calloc( 1, len );
    if ( ptr == NULL ) {
        return TE_ERROR_OOM;
    }

    /* open te session */
    hctx->sess = te_sess_open( drv->sctx, cat );
    if ( hctx->sess < 0 ) {
        osal_free( ptr );
        ret = hctx->sess;
        hctx->sess = INVALID_SESS_ID;
        return ret;
    }

    hactx = (hash_async_ctx_t *)osal_calloc( 1, sizeof(hash_async_ctx_t) );
    if ( hactx == NULL ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    /* Backup the HMAC key, be used by te_hash_reset */
    if ( key && key != &hctx->key ) {
        memcpy( (void *)&hctx->key, (void *)key, sizeof(te_hmac_key_t) );
        osal_cache_flush( (void *)&hctx->key, sizeof(te_hmac_key_t) );
    }

    /* HMAC case */
    if ( TE_ALG_GET_CLASS( alg ) == TE_OPERATION_MAC ) {

        if ( (key->type == TE_KEY_TYPE_SEC) ||
             (key->type == TE_KEY_TYPE_SEC_V2) ) {
            keytype |= ( (key->sec.sel == TE_KL_KEY_MODEL) ?
                         (HASH_MODEL_KEY) : (HASH_ROOT_KEY) );
            keytype |= ( (key->sec.ek3bits == 128) ?
                         (HASH_128BIT_KEY) : (HASH_256BIT_KEY) );
            if (key->type == TE_KEY_TYPE_SEC) {
                keyaddr = osal_virt_to_phys( hctx->key.sec.eks );
            } else {
                keyaddr = osal_virt_to_phys( hctx->key.sec_v2.eks );
            }
        } else {
            keytype |= HASH_EXT_KEY;
            keyaddr = osal_virt_to_phys( hctx->key.hkey );
        }

        HASH_SET_KEY_FIELD( ptr, HTOLE64(keyaddr) );
        HASH_SET_CMD_FIELD( ptr, keytype );
        HASH_SET_CMD_FIELD( ptr, HASH_HMAC_MODE );
    } else {
        /* Digest case */
        HASH_SET_CMD_FIELD( ptr, HASH_DIGEST_MODE );
        /* Digest case don't need key address parameter */
        len -= sizeof(uint64_t);
    }

    switch ( TE_ALG_GET_MAIN_ALG(alg) ){
    case TE_MAIN_ALGO_MD5:
        HASH_SET_CMD_FIELD( ptr, HASH_MD5_ALG );
        break;
    case TE_MAIN_ALGO_SHA1:
        HASH_SET_CMD_FIELD( ptr, HASH_SHA1_ALG );
        break;
    case TE_MAIN_ALGO_SHA224:
        HASH_SET_CMD_FIELD( ptr, HASH_SHA224_ALG );
        break;
    case TE_MAIN_ALGO_SHA256:
        HASH_SET_CMD_FIELD( ptr, HASH_SHA256_ALG );
        break;
    case TE_MAIN_ALGO_SHA384:
        HASH_SET_CMD_FIELD( ptr, HASH_SHA384_ALG );
        break;
    case TE_MAIN_ALGO_SHA512:
        HASH_SET_CMD_FIELD( ptr, HASH_SHA512_ALG );
        break;
    case TE_MAIN_ALGO_SM3:
        HASH_SET_CMD_FIELD( ptr, HASH_SM3_ALG );
        break;
    default:
        ret = TE_ERROR_BAD_PARAMS;
        goto err2;
    }


    HASH_SET_IV_FIELD( ptr, (osal_virt_to_phys(hctx->iv)) );
    HASH_SET_CMD_FIELD( ptr, (HASH_INIT_CMD | TRIGGER_INT) );

    ar = osal_calloc( 1, sizeof(te_sess_ar_t) );
    if ( ar == NULL ) {
        goto err2;
    }

    ar->cmdptr = ptr;
    ar->len = len;
    ar->para = (void *)hactx;
    ar->cb = hash_asubmit_done;

    hactx->hctx = hctx;
    hactx->req = req;
    hactx->ar = ar;
    hactx->done = hash_astart_done;

    ret = te_sess_asubmit( hctx->sess, ar );
    if ( ret != TE_SUCCESS ) {
        OSAL_LOG_ERR( "INIT command asubmit failed on session(%d)\n", hctx->sess );
        osal_free( ar );
        goto err2;
    }

    return TE_SUCCESS;

err2:
    osal_free( hactx );
err1:
    if ( te_sess_close( hctx->sess ) != TE_SUCCESS ) {
        OSAL_LOG_ERR( "Can't close session(%d)\n", hctx->sess );
    }
    hctx->sess = INVALID_SESS_ID;
    osal_free( ptr );

    return ret;
}

int te_hash_aupdate( te_crypt_ctx_t *ctx,
                     te_hash_request_t *req )
{
    int ret = TE_ERROR_GENERIC, len = ( HASH_PROC_LLST_SZ << 2);
    uint32_t i = 0;
    uint64_t total = 0, process = 0, remain = 0, ilen = 0;
    uint64_t linked = 0, offs = 0;
    uint64_t filled = 0, fillsz = 0;
    int entry = 0, max_entry = 0;
    uint64_t addr = 0;
    uint32_t *ptr = NULL;
    uint32_t flags = 0, last_update = 0;
    link_list_t *link_list = NULL;
    te_memlist_t *in = NULL;
    te_mement_t fake_mement = { 0 };
    te_memlist_t fake_memlist = { 0 };
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;
    hash_async_ctx_t *hactx = NULL;
    te_sess_ar_t *ar = NULL;
    uint32_t ns_attr = 0U;

    if ( !ctx || !req ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );

    if ( hctx->state != TE_DRV_HASH_STATE_START &&
         hctx->state != TE_DRV_HASH_STATE_UPDATE ) {
        return TE_ERROR_BAD_STATE;
    }

    flags = req->up.flags;

    if ( flags & HASH_FLAGS_LAST ) {
        last_update = 1;
    }

    /*
     * Direct buffer, build a fake memlist, unify
     * the subsequent process flow.
     */
    if ( !(flags & HASH_FLAGS_LIST) ) {
        in = &fake_memlist;
        in->ents = &fake_mement;
        in->nent = 1;
        in->ents[0].buf = (void *)req->up.data.in;
        in->ents[0].len = req->up.data.ilen;
    } else {
        in = &req->up.lst.in;
    }

    /* In memory list is empty, but nent is not '0' */
    if ( !in->ents && in->nent ) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* one for npdata, the other for end of list */
    max_entry = 2;
    for ( i = 0; i < in->nent; i++ ) {
        ilen += in->ents[i].len;
        max_entry += in->ents[i].len / LLST_ENTRY_SZ_MAX;
        if ( in->ents[i].len % (LLST_ENTRY_SZ_MAX) ) {
            max_entry++;
        }
    }

    total = ilen + hctx->npdlen;
    process = ( total & (~(hctx->base.blk_size - 1ULL)) );
    remain = ( total & (hctx->base.blk_size - 1ULL) );

    if ( process == 0 && !last_update ) {
        goto npdata;
    }

    ptr = (uint32_t *)osal_calloc( 1, len );
    if ( ptr == NULL ) {
        return TE_ERROR_OOM;
    }

    hactx = (hash_async_ctx_t *)osal_calloc( 1, sizeof(hash_async_ctx_t) );
    if ( hactx == NULL ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    ar = osal_calloc( 1, sizeof(te_sess_ar_t) );
    if ( ar == NULL ) {
        ret = TE_ERROR_OOM;
        goto err2;
    }

    link_list = (link_list_t *)osal_malloc_aligned
                    ( (max_entry * sizeof(link_list_t)), LINK_LIST_ALIGN );

    if ( !link_list ) {
        ret = TE_ERROR_OOM;
        goto err3;
    }
    osal_memset( (void *)link_list, 0, (max_entry * sizeof(link_list_t)) );

    linked += hctx->npdlen;
    if ( hctx->npdlen ) {
        /*
         * Let hardware consume the temporary buffer.
         * This can help us deal with npdata in a simply way.
         */
        hactx->up.npdata = (uint8_t *)osal_calloc( 1, hctx->npdlen );
        if ( hactx->up.npdata ==  NULL ) {
            ret = TE_ERROR_OOM;
            goto err4;
        }
        hactx->up.npdlen = hctx->npdlen;
        memcpy( (void *)hactx->up.npdata, (void *)hctx->npdata, hctx->npdlen );
        hctx->npdlen = 0;

        link_list[entry].sz_attr.sz = HTOLE64( hactx->up.npdlen - 1 );
        link_list[entry].addr =
            HTOLE64( osal_virt_to_phys( (void *)hactx->up.npdata ) );
        osal_cache_flush( hactx->up.npdata, hactx->up.npdlen );
        entry++;
    }

    for ( i = 0; i < in->nent; i++ ) {
        filled = 0;
        while ( filled < in->ents[i].len ) {
            fillsz = ( ((in->ents[i].len - filled) > LLST_ENTRY_SZ_MAX) ?
                        LLST_ENTRY_SZ_MAX : in->ents[i].len - filled );

            link_list[entry].sz_attr.sz = HTOLE64( fillsz - 1 );
            link_list[entry].addr = HTOLE64(
                            osal_virt_to_phys( (void *)(uintptr_t)
                                               ((uintptr_t)in->ents[i].buf + filled)) );
            osal_cache_flush( (void *)(uintptr_t)
                              ((uintptr_t)in->ents[i].buf + filled),
                              fillsz );
            linked += fillsz;
            filled += fillsz;

            /*
             * process is just equal to the block aligned size.
             * if last_update, just fill all of ents into link list.
             */
            if ( linked >= process && !last_update ) {
                /* Fixup link list entry size */
                link_list[entry].sz_attr.sz = HTOLE64(
                                      fillsz - (linked - process) - 1 );
                entry++;
                goto done;
            }
            entry++;
        }
    }
done:
    /* End of link list */
    link_list[entry].val = 0xffffffffffffffffULL;
    link_list[entry].addr = 0x0ULL;

    TE_ASSERT( entry < max_entry );

    /* set the non-secure bit for each node of the list */
    ret = te_com_set_llst_nsec_attr(link_list);
    if (ret != TE_SUCCESS) {
        goto err5;
    }

    osal_cache_flush( (uint8_t *)link_list, (max_entry * sizeof(link_list_t)) );

npdata:
    /*
     * Here are these cases:
     *  ilen == 0 : process = 0, remain = hctx->npdlen, no data processed.
     *
     *  ilen + npdlen >= block size : npdata will be processed and empty.
     *
     *  ilen + npdlen < block size : which means process = 0,
     *  remain = (npdlen + ilen), need append data to npdata.
     */
    if ( ilen != 0 && remain && process != 0 && !last_update ) {
        /* take care of the input data, it has been processed, partially */
        hctx->npdlen = 0;

        /* calculate the internal offset of the ent's buf */
        offs = (filled - (linked - process));

        for (; i< in->nent; i++) {
            memcpy( (void *)&hctx->npdata[hctx->npdlen],
                    (void *)(uintptr_t)((uintptr_t)in->ents[i].buf + offs),
                    (in->ents[i].len - offs) );
            /* update npdlen, accordingly*/
            hctx->npdlen += (in->ents[i].len - offs);
            /* Reset offs to '0', after handle the first ent */
            offs = 0;
        }

    } else if ( process == 0 && !last_update ) {
        /* No data be processed at all, append new data to npdate */
        for ( i = 0; i < in->nent; i++ ) {
            memcpy( (void *)&hctx->npdata[hctx->npdlen],
                    in->ents[i].buf,
                    in->ents[i].len );
            hctx->npdlen += in->ents[i].len;
        }

        /* Pretend we already do update */
        if ( hctx->state == TE_DRV_HASH_STATE_START ) {
            hctx->state = TE_DRV_HASH_STATE_UPDATE;
        }
        /*
         * if caller does not supply enough(>=block size ) data.
         * no operation will be executed.
         * Just notify the caller that, the requset is done.
         */
        req->res = TE_SUCCESS;
        req->base.completion( &req->base, req->res );
        ret = TE_SUCCESS;
        goto cleanup;
    }

    addr = osal_virt_to_phys( (void *)link_list );
    HASH_SET_PROC_SRC_LLST( ptr, HTOLE64(addr) );

    if ( last_update ) {
        HASH_SET_CMD_FIELD( ptr, HASH_PROC_LAST );

        /* Default to padding */
        if ( !(flags & HASH_FLAGS_NOPAD) ) {
            HASH_SET_CMD_FIELD( ptr, HASH_PROC_PADDING );
        }

        /* Zero proc case */
        if ( total == 0 ) {
            /* Zero proc, command frame no source address field(64bits) */
            len -= sizeof(uint64_t);
            HASH_SET_CMD_FIELD( ptr, HASH_PROC_ZERO );
        }
    }

    /* set non-secure attribute of src */
    ret = te_com_set_nsec_attr( addr, max_entry * sizeof(link_list_t),
                                &ns_attr, HASH_PROC_SRC_NS_SHIFT );
    if ( ret != TE_SUCCESS ) {
        goto err4;
    }
    HASH_SET_CMD_FIELD( ptr, (HASH_PROC_CMD |
                              HASH_LINK_LIST_MODE |
                              TRIGGER_INT | ns_attr) );

    hactx->up.link_list = link_list;
    hactx->up.last_update = last_update;
    hactx->req = req;
    hactx->ar = ar;
    hactx->hctx = hctx;
    hactx->done = hash_aupdate_done;
    ar->cmdptr = ptr;
    ar->len = len;
    ar->para = (void *)hactx;
    ar->cb = hash_asubmit_done;

    ret = te_sess_asubmit( hctx->sess, ar );
    if ( ret != TE_SUCCESS ) {
        hash_error_cleanup( hctx );
        OSAL_LOG_ERR( "Async PROC(link list) command failed on session(%d)\n", hctx->sess );
        goto err5;
    }

    return TE_SUCCESS;

err5:
    if ( hactx->up.npdlen != 0 ) {
        memcpy( (void *)hctx->npdata, (void *)hactx->up.npdata, hactx->up.npdlen );
        hctx->npdlen = hactx->up.npdlen;
        osal_free( hactx->up.npdata );
        hactx->up.npdlen = 0;
    }
cleanup:
err4:
    osal_free( link_list );
err3:
    osal_free ( ar );
err2:
    osal_free( hactx );
err1:
    osal_free( ptr );
    return ret;
}

int te_hash_afinish( te_crypt_ctx_t *ctx,
                     te_hash_request_t *req )
{
    int ret = TE_ERROR_GENERIC, len = ( HASH_FINISH_CMD_SZ << 2 );
    uint32_t *ptr = NULL;
    uint64_t addr = 0, keyaddr = 0;
    uint32_t maclen = 0;
    hash_async_ctx_t *hactx = NULL;
    te_sess_ar_t *ar = NULL;
    hash_drv_ctx_t *hctx = (hash_drv_ctx_t *)ctx;
    te_algo_t alg = hctx->base.alg;

    if ( !ctx || !req ) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT_MSG( (hctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );

    if ( hctx->state != TE_DRV_HASH_STATE_LAST &&
         hctx->state != TE_DRV_HASH_STATE_UPDATE &&
         hctx->state != TE_DRV_HASH_STATE_START ) {
        return TE_ERROR_BAD_STATE;
    }

    if ( hctx->state == TE_DRV_HASH_STATE_START ) {
        /* Send clear, CLEAR must never failed  */
        ret = te_hash_submit_clear( hctx );
        TE_ASSERT( ret == TE_SUCCESS );

        /* Before transition to STATE_INIT, close session */
        ret = te_sess_close( hctx->sess );
        TE_ASSERT( ret == TE_SUCCESS );
        hctx->sess = INVALID_SESS_ID;
        hctx->state = TE_DRV_HASH_STATE_INIT;

        req->res = TE_SUCCESS;
        req->base.completion( &req->base, req->res );
        return TE_SUCCESS;
    }

    if ( hctx->state == TE_DRV_HASH_STATE_UPDATE ) {
        ret = te_hash_last_update( hctx);
        if ( ret != TE_SUCCESS ) {
            /* If failed, notify caller */
            req->res = ret;
            req->base.completion( &req->base, req->res );
            return TE_SUCCESS;
        }
    }


    ptr = (uint32_t *)osal_calloc( 1, len );
    if ( ptr == NULL ) {
        return TE_ERROR_OOM;
    }

    hactx = (hash_async_ctx_t *)osal_calloc( 1, sizeof(hash_async_ctx_t) );
    if ( hactx == NULL ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    ar = osal_calloc( 1, sizeof(te_sess_ar_t) );
    if ( ar == NULL ) {
        ret = TE_ERROR_OOM;
        goto err2;
    }

    hactx->fin.hlen = hctx->hlen;

    addr = osal_virt_to_phys( hactx->fin.mac );
    maclen = HASH_MAC_LEN( hactx->fin.hlen );
    osal_cache_flush( hactx->fin.mac, TE_MAX_HASH_SIZE );

    HASH_SET_FINISH_DEST( ptr, HTOLE64(addr) );
    HASH_SET_CMD_FIELD( ptr, (HASH_FINISH_CMD |
                             TRIGGER_INT |
                             maclen) );

    if ( TE_ALG_GET_CLASS( alg ) == TE_OPERATION_MAC ) {

        if ( hctx->key.type == TE_KEY_TYPE_SEC ) {
            keyaddr = osal_virt_to_phys( hctx->key.sec.eks );
        } else if ( hctx->key.type == TE_KEY_TYPE_SEC_V2 ) {
            keyaddr = osal_virt_to_phys( hctx->key.sec_v2.eks );
        } else {
            keyaddr = osal_virt_to_phys( hctx->key.hkey );
        }

        HASH_SET_IV_FIELD( ptr, (osal_virt_to_phys(hctx->iv)) );
        HASH_SET_KEY_FIELD( ptr, HTOLE64(keyaddr) );
    } else {
        /* Digest case don't need to pass key & iv address */
        len -= sizeof(uint64_t) * 2;
    }

    hactx->hctx = hctx;
    hactx->req = req;
    hactx->ar = ar;
    hactx->done = hash_afinish_done;
    ar->cmdptr = ptr;
    ar->len = len;
    ar->para = (void *)hactx;
    ar->cb = hash_asubmit_done;

    ret = te_sess_asubmit( hctx->sess, ar );
    if ( ret != TE_SUCCESS ) {
        hash_error_cleanup( hctx );
        OSAL_LOG_ERR( "FINISH command failed on session(%d)\n", hctx->sess );
        goto err3;
    }

    return TE_SUCCESS;

err3:
    osal_free( ar );
err2:
    osal_free( hactx );
err1:
    osal_free( ptr );
    return ret;
}

int te_hash_aclone( te_crypt_ctx_t *ctx,
                    te_hash_request_t *req )
{
    hash_drv_ctx_t *sctx = (hash_drv_ctx_t *)ctx;
    te_hash_drv_t *hdrv = NULL;
    hash_worker_t *worker = NULL;
    hash_drv_ctx_t *dctx = NULL;
    hash_async_ctx_t *hactx = NULL;
    if ( !ctx || !req ) {
        return TE_ERROR_BAD_PARAMS;
    }
    dctx = (hash_drv_ctx_t *)req->cl.dst;

    TE_ASSERT_MSG( (sctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );
    TE_ASSERT_MSG( (dctx->magic == HASH_CTX_MAGIC),
                     "BUG: Not valid hash driver context\n" );

    if ( dctx->state != TE_DRV_HASH_STATE_INIT ) {
        return TE_ERROR_BAD_PARAMS;
    }

    /*
     * Since there is no async interface for clone,
     * here let work thread handle this.
     */
    hactx = osal_calloc( 1, sizeof(hash_async_ctx_t) );
    if ( hactx == NULL ) {
        return TE_ERROR_OOM;
    }

    hactx->hctx = sctx;
    hactx->req = req;
    hactx->cl.src = ctx;
    hactx->cl.dst = req->cl.dst;
    hactx->done = hash_aclone_done;

    hdrv = (te_hash_drv_t *)sctx->base.drv;
    worker = hdrv->worker;

    hash_worker_enqueue( worker, hactx );

    return TE_SUCCESS;
}

#endif /* CFG_TE_ASYNC_EN */

