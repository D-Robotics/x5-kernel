//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_defines.h>
#include <hwa/te_hwa.h>
#include <sqlist.h>
#include "te_regs.h"

/**
 * All known irqs
 */
#define TE_IRQ_TYPE_ALL (TE_IRQ_TYPE_HASH |      \
                         TE_IRQ_TYPE_SCA  |      \
                         TE_IRQ_TYPE_ACA  |      \
                         TE_IRQ_TYPE_RNP  |      \
                         TE_IRQ_TYPE_TRNG |      \
                         TE_IRQ_TYPE_CTX_POOL_ERR)

/**
 * IRQ notifier block structure
 */
typedef struct irq_notifier_block {
    te_irq_notifier_t fn;        /**< hook function */
    void *uparam;                /**< user param pointer */
    uint32_t type;               /**< irq type */
    sqlist_t node;               /**< link node */
} irq_notifier_block_t;

/**
 * HWA host private context structure
 */
typedef struct hwa_host_ctx {
    int id;                      /**< host id, starting from 0 */
    te_common_regs_t *com;       /**< common register file */
    te_host_regs_t *host;        /**< host register file */
#ifdef CFG_TE_IRQ_EN
    int irqno;                   /**< interrupt number */
    osal_spin_lock_t spin;       /**< spin lock */
    sqlist_t nblst;              /**< notifier block list */
    osal_intr_ctx_t irqctx;      /**< irq context */
#endif
} hwa_host_ctx_t;

static int hwa_host_state( te_hwa_host_t *hwa, te_host_int_t *state )
{
    hwa_host_ctx_t *ctx = NULL;

    ctx = hwa_crypt_ctx(&hwa->base);
    TE_ASSERT(ctx != NULL);

    if (NULL == hwa->stat.host_state) {
        return TE_ERROR_NOT_SUPPORTED;
    }

    return hwa->stat.host_state(&hwa->stat, ctx->id, state);
}

#ifdef CFG_TE_IRQ_EN
static osal_err_t hwa_irq_handler(void *param)
{
    int rc = TE_SUCCESS;
    te_hwa_host_t *hwa = (te_hwa_host_t *)param;
    hwa_host_ctx_t *ctx = NULL;
    irq_notifier_block_t *nb = NULL;
    te_host_int_t stat = {0};
    te_irq_type_t itype = 0;

    TE_ASSERT(hwa != NULL);
    ctx = hwa_crypt_ctx(&hwa->base);
    TE_ASSERT(ctx != NULL);

    rc = hwa_host_state(hwa, &stat);
    if (rc != TE_SUCCESS) {
         /* error! */
        goto out;
    }

    itype |= (stat.hash) ? TE_IRQ_TYPE_HASH : 0;
    itype |= (stat.sca) ? TE_IRQ_TYPE_SCA : 0;
    itype |= (stat.aca) ? TE_IRQ_TYPE_ACA : 0;
    itype |= (stat.rnp) ? TE_IRQ_TYPE_RNP : 0;
    /* host#0 specific interrupts */
    if (0 == ctx->id) {
        itype |= (stat.trng) ? TE_IRQ_TYPE_TRNG : 0;
        itype |= (stat.ctxp_err) ? TE_IRQ_TYPE_CTX_POOL_ERR : 0;
    }
    if (0 == itype) {
        return TE_SUCCESS;  /**< fake interrupts!? */
    }

    osal_spin_lock(&ctx->spin);
    SQLIST_FOR_EACH_CONTAINER(&ctx->nblst, nb, node) {
        if (nb->type & itype) {
            nb->fn((nb->type & itype), nb->uparam);
        }
    }
    osal_spin_unlock(&ctx->spin);
    rc = TE_SUCCESS;

out:
    /* clear interrupt sources */
#define HWA_STUB_EOI(_MOD,_hwa,_type) do {              \
    if (itype & TE_IRQ_TYPE_##_MOD) {                   \
        te_##_type##_t _state = {0};                    \
        TE_ASSERT((_hwa)->int_state && (_hwa)->eoi);    \
        (_hwa) ->int_state((_hwa), &_state);            \
        (_hwa) ->eoi((_hwa), &_state);                  \
    }                                                   \
} while(0)

    HWA_STUB_EOI(ACA, &hwa->aca, aca_int);
    HWA_STUB_EOI(RNP, &hwa->trng, rnp_int);

    /* host#0 specific interrupts */
    if (0 == ctx->id) {
        HWA_STUB_EOI(TRNG, hwa->trngctl, trng_int);

        /*
         * TE_IRQ_TYPE_CTX_POOL_ERR can only be cleared by reset
         */
    }

    return rc;
#undef HWA_STUB_EOI
}
#endif

int te_hwa_alloc( te_hwa_host_t **hwa, void* base, int irq, int n )
{
    int rc = TE_SUCCESS;
    te_hwa_host_t *nhwa = NULL;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc memory */
    if ((nhwa = osal_calloc(1, sizeof(*nhwa))) == NULL) {
        return TE_ERROR_OOM;
    }

    /* init hwa */
    if ((rc = te_hwa_init(nhwa, base, irq, n)) != TE_SUCCESS) {
        osal_free(nhwa);
        return rc;
    }

    *hwa = nhwa;
    return TE_SUCCESS;
}

int te_hwa_free( te_hwa_host_t *hwa )
{
    int rc = TE_SUCCESS;

    if ((rc = te_hwa_exit(hwa)) == TE_SUCCESS) {
        osal_memset(hwa, 0, sizeof(*hwa));
        osal_free(hwa);
    }
    return rc;
}

int te_hwa_init( te_hwa_host_t *hwa, void* base, int irq, int n )
{
#define HWA_STUB_INIT(regs, nm,...) do {                  \
    rc = te_hwa_##nm##_init(&ctx->regs -> nm, hwa,        \
                            ##__VA_ARGS__, &hwa-> nm);    \
    if (rc != TE_SUCCESS) {                               \
        goto err_##nm##_init;                             \
    }                                                     \
} while(0)

#define HWA_COM_STUB_INIT(nm,...)          \
    HWA_STUB_INIT(com, nm, ##__VA_ARGS__)
#define HWA_HOST_STUB_INIT(nm,...)         \
    HWA_STUB_INIT(host, nm, ##__VA_ARGS__)

#define HWA_HOST_STUB_ALLOC(nm,...) do {                  \
    rc = te_hwa_##nm##_alloc(&ctx->host-> nm, hwa,        \
                             ##__VA_ARGS__, &hwa-> nm);   \
    if (rc != TE_SUCCESS) {                               \
        goto err_##nm##_alloc;                            \
    }                                                     \
} while(0)

#define HWA_STUB_INIT_ERROR(nm)            \
    te_hwa_##nm##_exit(&hwa-> nm);         \
err_##nm##_init:

#define HWA_STUB_ALLOC_ERROR(nm)           \
    te_hwa_##nm##_free(hwa-> nm);          \
    hwa-> nm = (te_hwa_## nm ##_t *)NULL;  \
err_##nm##_alloc:

    int rc = TE_SUCCESS;
    hwa_host_ctx_t *ctx = NULL;
    union {
        te_merak_ver_t ver;
        uint32_t val;
    } u = {0};

    if (!hwa || !base || irq < 0 || n < 0 || n > (int)TE_MAX_HOST_NUM - 1) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc and init the private ctx */
    if ((ctx = osal_calloc(1, sizeof(*ctx))) == NULL) {
        return TE_ERROR_OOM;
    }

    ctx->id = n;
    ctx->com = (te_common_regs_t*)base;
    ctx->host = (te_host_regs_t*)((uintptr_t)base + COMMON_REGS_SIZE +
                                  HOST_REGS_SIZE * n);
#ifdef CFG_TE_IRQ_EN
    ctx->irqno = irq;
    sqlist_init(&ctx->nblst);
    rc = osal_spin_lock_init(&ctx->spin);
    if (rc != OSAL_SUCCESS) {
        goto err_init_ctx;
    }
#endif

    osal_memset(hwa, 0, sizeof(*hwa));
    /* init hwa submodules */
    HWA_COM_STUB_INIT(stat);
    HWA_COM_STUB_INIT(otp);
    HWA_HOST_STUB_INIT(aca);
    HWA_HOST_STUB_INIT(sca, false);
    HWA_HOST_STUB_INIT(hash);
    HWA_HOST_STUB_INIT(trng);

    /* init host#0 submodules */
    if (0 == n) {
        HWA_HOST_STUB_ALLOC(trngctl);
        HWA_HOST_STUB_ALLOC(dbgctl);
        HWA_HOST_STUB_ALLOC(dma);
        HWA_HOST_STUB_ALLOC(otpctl);
        HWA_HOST_STUB_ALLOC(ctl);
    }

    /**
     * In order to be consistent with the CE Merak,
     * we check the Merak version number for the driver here.
     * It will return error when the Merak's version is less than r3p0.
     */
#define MIN_MERAK_MAJOR_VERSION   (0x03)
    u.val = hwa->stat.version(&hwa->stat);
    if ((u.ver.major < MIN_MERAK_MAJOR_VERSION)) {
        OSAL_LOG_ERR("Current Merak version%x.%x does not match the driver!\n",
                      u.ver.major, u.ver.minor);
        rc = TE_ERROR_NOT_SUPPORTED;
        goto err_ver_check;
    }

#ifdef CFG_TE_IRQ_EN
    /* request irq */
    rc = osal_irq_request(&ctx->irqctx, irq, hwa_irq_handler, (void*)hwa);
    if (rc != OSAL_SUCCESS) {
        goto err_req_irq;
    }
#endif

    /* on success */
    hwa_crypt_init(&hwa->base, hwa, ctx);
    hwa->magic = TE_HOST_MAGIC;
    return TE_SUCCESS;

    /* on error */
#ifdef CFG_TE_IRQ_EN
err_req_irq:
#endif
err_ver_check:
    if (0 == n) {
        HWA_STUB_ALLOC_ERROR(ctl);
        HWA_STUB_ALLOC_ERROR(otpctl);
        HWA_STUB_ALLOC_ERROR(dma);
        HWA_STUB_ALLOC_ERROR(dbgctl);
        HWA_STUB_ALLOC_ERROR(trngctl);
    }

    HWA_STUB_INIT_ERROR(trng);
    HWA_STUB_INIT_ERROR(hash);
    HWA_STUB_INIT_ERROR(sca);
    HWA_STUB_INIT_ERROR(aca);
    HWA_STUB_INIT_ERROR(otp);
    HWA_STUB_INIT_ERROR(stat);

#ifdef CFG_TE_IRQ_EN
    osal_spin_lock_destroy(&ctx->spin);
err_init_ctx:
#endif
    osal_free(ctx);
    ctx = NULL;
    return rc;

#undef HWA_COM_STUB_INIT
#undef HWA_HOST_STUB_INIT
#undef HWA_STUB_INIT
#undef HWA_STUB_INIT_ERROR
#undef HWA_HOST_STUB_ALLOC
#undef HWA_STUB_ALLOC_ERROR
}

int te_hwa_exit( te_hwa_host_t *hwa )
{
    hwa_host_ctx_t *ctx = NULL;
    __attribute__((unused)) unsigned long flags = 0;

    if (!hwa || hwa->magic != TE_HOST_MAGIC)
        return TE_ERROR_BAD_PARAMS;

    ctx = hwa_crypt_ctx(&hwa->base);
    TE_ASSERT(ctx != NULL);

#ifdef CFG_TE_IRQ_EN
    /* free irq */
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    osal_irq_free(&ctx->irqctx);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);
#endif

    if (0 == ctx->id) {
        /* free host#0 only submodules */
        te_hwa_dma_free(hwa->dma);
        te_hwa_dbgctl_free(hwa->dbgctl);
        te_hwa_otpctl_free(hwa->otpctl);
        te_hwa_trngctl_free(hwa->trngctl);
        te_hwa_ctl_free(hwa->ctl);
    }

    te_hwa_stat_exit(&hwa->stat);
    te_hwa_otp_exit(&hwa->otp);
    te_hwa_aca_exit(&hwa->aca);
    te_hwa_sca_exit(&hwa->sca);
    te_hwa_hash_exit(&hwa->hash);
    te_hwa_trng_exit(&hwa->trng);

    osal_memset(ctx, 0, sizeof(*ctx));
    osal_free(ctx);
    hwa_crypt_exit(&hwa->base);

    return TE_SUCCESS;
}

int te_hwa_host_id( te_hwa_host_t *hwa )
{
    hwa_host_ctx_t *ctx = NULL;

    if (!hwa || hwa->magic != TE_HOST_MAGIC)
        return TE_ERROR_BAD_PARAMS;

    ctx = hwa_crypt_ctx(&hwa->base);
    TE_ASSERT(ctx != NULL);

    return ctx->id;
}

int te_hwa_host_conf( te_hwa_host_t *hwa, te_host_conf_t *conf )
{
    hwa_host_ctx_t *ctx = NULL;

    if (!hwa || hwa->magic != TE_HOST_MAGIC)
        return TE_ERROR_BAD_PARAMS;

    ctx = hwa_crypt_ctx(&hwa->base);
    TE_ASSERT(ctx != NULL);

    if (NULL == hwa->stat.host_conf) {
        return TE_ERROR_NOT_SUPPORTED;
    }

    return hwa->stat.host_conf(&hwa->stat, ctx->id, conf);
}

int te_hwa_host_state( te_hwa_host_t *hwa, te_host_int_t *state )
{
    if (!hwa || hwa->magic != TE_HOST_MAGIC)
        return TE_ERROR_BAD_PARAMS;

    return hwa_host_state(hwa, state);
}

int te_hwa_register_notifier( te_hwa_host_t *hwa,
                              const uint32_t irq_type,
                              te_irq_notifier_t fn,
                              void *uparam,
                              te_irq_nb_handle *h )
{
#ifdef CFG_TE_IRQ_EN
    hwa_host_ctx_t *ctx = NULL;
    irq_notifier_block_t *nb = NULL;
    unsigned long flags = 0;

    if (!hwa || hwa->magic != TE_HOST_MAGIC ||
        (irq_type & ~TE_IRQ_TYPE_ALL) || !fn || !h)
        return TE_ERROR_BAD_PARAMS;

    ctx = hwa_crypt_ctx(&hwa->base);
    TE_ASSERT(ctx != NULL);

    if ((nb = osal_calloc(1, sizeof(*nb))) == NULL) {
        return TE_ERROR_OOM;
    }

    nb->fn = fn;
    nb->uparam = uparam;
    nb->type = irq_type;

    osal_spin_lock_irqsave(&ctx->spin, &flags);
    sqlist_insert_tail(&ctx->nblst, &nb->node);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *h = (te_irq_nb_handle)nb;
    return TE_SUCCESS;

#else  /* CFG_TE_IRQ_EN */
    (void)hwa;
    (void)irq_type;
    (void)fn;
    (void)uparam;
    (void)h;
    return TE_ERROR_NOT_IMPLEMENTED;
#endif /* !CFG_TE_IRQ_EN */
}

int te_hwa_unregister_notifier( te_hwa_host_t *hwa,
                                te_irq_nb_handle h )
{
#ifdef CFG_TE_IRQ_EN
    hwa_host_ctx_t *ctx = NULL;
    irq_notifier_block_t *nb = (irq_notifier_block_t*)h;
    unsigned long flags = 0;

    if (!hwa || hwa->magic != TE_HOST_MAGIC || TE_HANDLE_NULL == h)
        return TE_ERROR_BAD_PARAMS;

    ctx = hwa_crypt_ctx(&hwa->base);
    TE_ASSERT(ctx != NULL);

    osal_spin_lock_irqsave(&ctx->spin, &flags);
    sqlist_remove(&nb->node);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);
    osal_free(nb);
    return TE_SUCCESS;

#else  /* CFG_TE_IRQ_EN */
    (void)hwa;
    (void)h;
    return TE_ERROR_NOT_IMPLEMENTED;
#endif /* !CFG_TE_IRQ_EN */
}
