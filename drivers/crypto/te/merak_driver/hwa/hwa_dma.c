//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <hwa/te_hwa_dma.h>
#include "te_regs.h"

/**
 * AXI DMA HWA private context structure
 */
typedef struct hwa_dma_ctx {
    struct te_dma_regs *regs;   /**< AXI DMA register file */
    osal_spin_lock_t spin;      /**< lock */
    //TODO...
} hwa_dma_ctx_t;

int te_hwa_dma_alloc( struct te_dma_regs *regs,
                      struct te_hwa_host *host,
                      te_hwa_dma_t **hwa )
{
    int rc = TE_SUCCESS;
    te_hwa_dma_t *dma = NULL;

    if (!regs || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc mem */
    if ((dma = osal_calloc(1, sizeof(*dma))) == NULL) {
        return TE_ERROR_OOM;
    }

    /* init hwa */
    rc = te_hwa_dma_init(regs, host, dma);
    if (rc != TE_SUCCESS) {
        osal_free(dma);
        return rc;
    }

    *hwa = dma;
    return TE_SUCCESS;
}

int te_hwa_dma_free( te_hwa_dma_t *hwa )
{
    int rc = TE_SUCCESS;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    rc = te_hwa_dma_exit(hwa);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    osal_memset(hwa, 0, sizeof(*hwa));
    osal_free(hwa);
    return TE_SUCCESS;
}

int te_hwa_dma_init( struct te_dma_regs *regs,
                     struct te_hwa_host *host,
                     te_hwa_dma_t *hwa )
{
    int rc = TE_SUCCESS;
    hwa_dma_ctx_t *ctx = NULL;

    if (!regs || !host || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    if ((ctx = osal_calloc(1, sizeof(*ctx))) == NULL) {
        return TE_ERROR_OOM;
    }

    rc = osal_spin_lock_init(&ctx->spin);
    if (rc != OSAL_SUCCESS) {
        goto err;
    }

    ctx->regs = regs;
    osal_memset(hwa, 0, sizeof(*hwa));
    hwa_crypt_init(&hwa->base, host, (void*)ctx);

    /* set ops */
    //TODO...

    return TE_SUCCESS;

err:
    osal_free(ctx);
    ctx = NULL;
    return rc;
}

int te_hwa_dma_exit( te_hwa_dma_t *hwa )
{
    hwa_dma_ctx_t *ctx = NULL;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = (hwa_dma_ctx_t*)hwa_crypt_ctx(&hwa->base);
    osal_spin_lock_destroy(&ctx->spin);
    osal_memset(ctx, 0, sizeof(*ctx));
    osal_free(ctx);
    hwa_crypt_exit(&hwa->base);
    return TE_SUCCESS;
}

